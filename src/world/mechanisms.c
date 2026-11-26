// mechanisms.c - levers, pressure plates and links (see mechanisms.h)
#include "mechanisms.h"
#include "grid.h"
#include "cell_defs.h"
#include "pipes.h"
#include "designations.h"
#include "../entities/mover.h"
#include "../entities/animals.h"
#include "../entities/items.h"
#include "../core/event_log.h"
#include <string.h>

Mechanism mechanisms[MAX_MECHANISMS];
MechLink mechLinks[MAX_MECH_LINKS];
int plateWeight = 1;
bool linksOverlay = false;

static int mechHigh = 0;        // one past the highest used mechanism slot
static int linkHigh = 0;        // one past the highest used link slot
static int plateCount = 0;      // plates placed: the tick does nothing without them

// Doors told to lock while someone stood in them; retried each tick until the doorway is clear
typedef struct { int16_t x, y, z; } MechCell;
#define MAX_WAITING_LOCKS 128
static MechCell waitingLocks[MAX_WAITING_LOCKS];
static int waitingLockCount = 0;

static bool InMap(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < gridWidth && y < gridHeight && z < gridDepth;
}

const char* MechKindName(int kind) {
    switch (kind) {
        case MECH_LEVER: return "lever";
        case MECH_PLATE: return "pressure plate";
        default: return "?";
    }
}

static void RecountMechanisms(void) {
    mechHigh = 0; plateCount = 0;
    for (int i = 0; i < MAX_MECHANISMS; i++) {
        if (mechanisms[i].kind == MECH_NONE) continue;
        mechHigh = i + 1;
        if (mechanisms[i].kind == MECH_PLATE) plateCount++;
    }
    linkHigh = 0;
    for (int i = 0; i < MAX_MECH_LINKS; i++) if (mechLinks[i].src >= 0) linkHigh = i + 1;
}

void ClearMechanisms(void) {
    memset(mechanisms, 0, sizeof(mechanisms));
    for (int i = 0; i < MAX_MECH_LINKS; i++) { memset(&mechLinks[i], 0, sizeof(MechLink)); mechLinks[i].src = -1; }
    mechHigh = linkHigh = plateCount = 0;
    waitingLockCount = 0;
}

int MechanismAt(int x, int y, int z) {
    for (int i = 0; i < mechHigh; i++) {
        Mechanism* m = &mechanisms[i];
        if (m->kind != MECH_NONE && m->x == x && m->y == y && m->z == z) return i;
    }
    return -1;
}

int MechanismCount(void) {
    int n = 0;
    for (int i = 0; i < mechHigh; i++) if (mechanisms[i].kind != MECH_NONE) n++;
    return n;
}

int PlaceMechanism(int x, int y, int z, MechKind kind) {
    if (!InMap(x, y, z) || kind == MECH_NONE || MechanismAt(x, y, z) >= 0) return -1;
    for (int i = 0; i < MAX_MECHANISMS; i++) {
        if (mechanisms[i].kind != MECH_NONE) continue;
        mechanisms[i] = (Mechanism){ (int16_t)x, (int16_t)y, (int16_t)z, (uint8_t)kind, 0 };
        RecountMechanisms();
        EventLog("Mechanism: %s placed at %d,%d,%d", MechKindName(kind), x, y, z);
        return i;
    }
    return -1;
}

bool RemoveMechanism(int x, int y, int z) {
    int idx = MechanismAt(x, y, z);
    if (idx < 0) return false;
    for (int i = 0; i < linkHigh; i++) if (mechLinks[i].src == idx) mechLinks[i].src = -1;
    if (InMap(x, y, z) && DESIGNATION(x, y, z).type == DESIGNATION_PULL_LEVER) CancelDesignation(x, y, z);
    memset(&mechanisms[idx], 0, sizeof(Mechanism));
    RecountMechanisms();
    return true;
}

// ---------------------------------------------------------------------------
// Targets
// ---------------------------------------------------------------------------

bool MechIsTarget(int x, int y, int z) {
    if (!InMap(x, y, z)) return false;
    if (CellIsDoor(GRID(x, y, z))) return true;
    return (GetPipe(x, y, z) & PIPE_VALVE) != 0;
}

static bool CellHasCreature(int x, int y, int z) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (m->active && (int)(m->x / CELL_SIZE) == x && (int)(m->y / CELL_SIZE) == y && (int)m->z == z) return true;
    }
    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (a->active && (int)(a->x / CELL_SIZE) == x && (int)(a->y / CELL_SIZE) == y && (int)a->z == z) return true;
    }
    return false;
}

static int WaitingLockIndex(int x, int y, int z) {
    for (int i = 0; i < waitingLockCount; i++)
        if (waitingLocks[i].x == x && waitingLocks[i].y == y && waitingLocks[i].z == z) return i;
    return -1;
}

static void DropWaitingLock(int x, int y, int z) {
    int w = WaitingLockIndex(x, y, z);
    if (w >= 0) waitingLocks[w] = waitingLocks[--waitingLockCount];
}

bool MechLockWaiting(int x, int y, int z) { return WaitingLockIndex(x, y, z) >= 0; }

void MechWrite(int x, int y, int z, int value) {
    if (!InMap(x, y, z)) return;
    CellType c = GRID(x, y, z);
    if (CellIsDoor(c)) {
        if (value != 0) {
            DropWaitingLock(x, y, z);
            SetDoorLocked(x, y, z, false);
            SetDoorOpen(x, y, z, true);
        } else if (c != CELL_DOOR_LOCKED) {
            if (CellHasCreature(x, y, z)) {   // someone in the doorway: lock once they are out
                if (WaitingLockIndex(x, y, z) < 0 && waitingLockCount < MAX_WAITING_LOCKS)
                    waitingLocks[waitingLockCount++] = (MechCell){ (int16_t)x, (int16_t)y, (int16_t)z };
            } else {
                SetDoorLocked(x, y, z, true);
            }
        }
        return;
    }
    uint8_t b = GetPipe(x, y, z);
    if (b & PIPE_VALVE) {
        uint8_t nb = value != 0 ? (uint8_t)(b & ~PIPE_VALVE_CLOSED) : (uint8_t)(b | PIPE_VALVE_CLOSED);
        if (nb != b) SetPipe(x, y, z, nb);
    }
}

int MechRead(int x, int y, int z) {
    if (!InMap(x, y, z)) return -1;
    CellType c = GRID(x, y, z);
    if (CellIsDoor(c)) return c == CELL_DOOR_LOCKED ? 0 : c == CELL_DOOR ? 1 : 2;
    uint8_t b = GetPipe(x, y, z);
    if (b & PIPE_VALVE) return (b & PIPE_VALVE_CLOSED) ? 0 : 1;
    return -1;
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------

int LinkMechanism(int src, int tx, int ty, int tz, bool invert) {
    if (src < 0 || src >= MAX_MECHANISMS || mechanisms[src].kind == MECH_NONE) return -1;
    if (!MechIsTarget(tx, ty, tz)) return -1;
    int freeSlot = -1;
    for (int i = 0; i < MAX_MECH_LINKS; i++) {
        MechLink* l = &mechLinks[i];
        if (l->src == src && l->x == tx && l->y == ty && l->z == tz) { l->invert = invert; return i; }
        if (l->src < 0 && freeSlot < 0) freeSlot = i;
        if (i >= linkHigh && freeSlot >= 0) break;
    }
    if (freeSlot < 0) return -1;
    mechLinks[freeSlot] = (MechLink){ (int16_t)src, (int16_t)tx, (int16_t)ty, (int16_t)tz, (uint8_t)invert, 0 };
    if (freeSlot >= linkHigh) linkHigh = freeSlot + 1;
    EventLog("Mechanism: %s %d,%d,%d linked to %d,%d,%d%s", MechKindName(mechanisms[src].kind),
             mechanisms[src].x, mechanisms[src].y, mechanisms[src].z, tx, ty, tz, invert ? " (inverted)" : "");
    return freeSlot;
}

bool UnlinkMechanism(int src, int tx, int ty, int tz) {
    for (int i = 0; i < linkHigh; i++) {
        MechLink* l = &mechLinks[i];
        if (l->src == src && l->x == tx && l->y == ty && l->z == tz) { l->src = -1; return true; }
    }
    return false;
}

int MechLinkCount(int src) {
    int n = 0;
    for (int i = 0; i < linkHigh; i++) if (mechLinks[i].src >= 0 && (src < 0 || mechLinks[i].src == src)) n++;
    return n;
}

void SetMechanismOn(int idx, bool on) {
    if (idx < 0 || idx >= MAX_MECHANISMS || mechanisms[idx].kind == MECH_NONE) return;
    Mechanism* m = &mechanisms[idx];
    if ((m->on != 0) == on) return;
    m->on = on;
    EventLog("Mechanism: %s %d,%d,%d %s", MechKindName(m->kind), m->x, m->y, m->z, on ? "on" : "off");
    for (int i = 0; i < linkHigh; i++) {
        MechLink* l = &mechLinks[i];
        if (l->src != idx) continue;
        MechWrite(l->x, l->y, l->z, (on != (l->invert != 0)) ? 1 : 0);
    }
}

void PullLever(int idx) {
    if (idx < 0 || idx >= MAX_MECHANISMS || mechanisms[idx].kind != MECH_LEVER) return;
    SetMechanismOn(idx, !mechanisms[idx].on);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

typedef struct { int weight; } PlateWeigh;
static void Mech_AddItemWeight(int itemIndex, float distSq, void* userData) {
    if (distSq > 0.01f) return;   // radius 0 can still hand out neighbours on some grids
    PlateWeigh* w = (PlateWeigh*)userData;
    int s = items[itemIndex].stackCount;
    w->weight += s > 0 ? s : 1;
}

static int PlateWeightAt(int x, int y, int z) {
    PlateWeigh w = { 0 };
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (m->active && (int)(m->x / CELL_SIZE) == x && (int)(m->y / CELL_SIZE) == y && (int)m->z == z) w.weight++;
    }
    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (a->active && (int)(a->x / CELL_SIZE) == x && (int)(a->y / CELL_SIZE) == y && (int)a->z == z) w.weight++;
    }
    if (itemGrid.cellFirst) QueryItemsInRadius(x, y, z, 0, Mech_AddItemWeight, &w);
    return w.weight;
}

void MechanismsTick(void) {
    if (plateCount > 0) {
        int need = plateWeight > 0 ? plateWeight : 1;
        for (int i = 0; i < mechHigh; i++) {
            Mechanism* m = &mechanisms[i];
            if (m->kind != MECH_PLATE) continue;
            SetMechanismOn(i, PlateWeightAt(m->x, m->y, m->z) >= need);
        }
    }
    for (int i = waitingLockCount - 1; i >= 0; i--) {
        MechCell c = waitingLocks[i];
        if (!InMap(c.x, c.y, c.z) || !CellIsDoor(GRID(c.x, c.y, c.z))) { waitingLocks[i] = waitingLocks[--waitingLockCount]; continue; }
        if (CellHasCreature(c.x, c.y, c.z)) continue;
        waitingLocks[i] = waitingLocks[--waitingLockCount];
        SetDoorLocked(c.x, c.y, c.z, true);
    }
}

// ---------------------------------------------------------------------------
// Pulling a lever by order
// ---------------------------------------------------------------------------

bool DesignatePullLever(int x, int y, int z) {
    int idx = MechanismAt(x, y, z);
    if (idx < 0 || mechanisms[idx].kind != MECH_LEVER) return false;
    if (DESIGNATION(x, y, z).type != DESIGNATION_NONE) return false;
    DESIGNATION_MUT(x, y, z).type = DESIGNATION_PULL_LEVER;
    DESIGNATION_MUT(x, y, z).assignedMover = -1;
    DESIGNATION_MUT(x, y, z).progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_PULL_LEVER);
    return true;
}

void CompletePullLeverDesignation(int x, int y, int z) {
    if (!InMap(x, y, z) || DESIGNATION(x, y, z).type != DESIGNATION_PULL_LEVER) return;
    activeDesignationCount--;
    DESIGNATION_MUT(x, y, z).type = DESIGNATION_NONE;
    DESIGNATION_MUT(x, y, z).assignedMover = -1;
    DESIGNATION_MUT(x, y, z).progress = 0.0f;
    InvalidateDesignationCache(DESIGNATION_PULL_LEVER);
    PullLever(MechanismAt(x, y, z));
}

// ---------------------------------------------------------------------------
// Save / load (v124, at the end of the save before the end marker)
// ---------------------------------------------------------------------------

void MechanismsSave(FILE* f) {
    fwrite(mechanisms, sizeof(Mechanism), MAX_MECHANISMS, f);
    fwrite(mechLinks, sizeof(MechLink), MAX_MECH_LINKS, f);
    fwrite(&waitingLockCount, sizeof(int), 1, f);
    fwrite(waitingLocks, sizeof(MechCell), MAX_WAITING_LOCKS, f);
}

void MechanismsLoad(FILE* f) {
    fread(mechanisms, sizeof(Mechanism), MAX_MECHANISMS, f);
    fread(mechLinks, sizeof(MechLink), MAX_MECH_LINKS, f);
    fread(&waitingLockCount, sizeof(int), 1, f);
    fread(waitingLocks, sizeof(MechCell), MAX_WAITING_LOCKS, f);
    if (waitingLockCount < 0 || waitingLockCount > MAX_WAITING_LOCKS) waitingLockCount = 0;
    RecountMechanisms();
}
