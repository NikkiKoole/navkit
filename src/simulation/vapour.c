// vapour.c - Fuel vapour and the flame front through it (see vapour.h)
#include "vapour.h"
#include "fire.h"
#include "smoke.h"
#include "temperature.h"
#include "explosion.h"
#include "rooms.h"
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../entities/mover.h"
#include "../core/effects.h"
#include "../core/event_log.h"
#include <string.h>

bool vapourEnabled = true;
float vapourStepInterval = 0.2f;
float vapourThinChance = 0.03f;

#define MAX_VAPOUR_CELLS 65536
#define MAX_FRONT        16384
#define MAX_LEAKS        32
#define IGNITE_CHECK     0.1f    // game seconds between looks for fire next to the vapour
#define FRONT_STEP_MIN   0.04f   // game seconds for the flame to cross into a neighbour
#define FRONT_STEP_MAX   0.10f

typedef struct { int x, y, z; } VCell;
typedef struct { int x, y, z; float t; } FrontCell;
typedef struct { int x, y, z; float rate, t, acc; } Leak;

static VCell cells[MAX_VAPOUR_CELLS];
static int cellCount;
static FrontCell front[MAX_FRONT];
static int frontCount;
static Leak leaks[MAX_LEAKS];
static int leakCount;
typedef struct { int x, y, z; float t; } Spark;
#define MAX_SPARKS 32
static Spark sparks[MAX_SPARKS];
static int sparkCount;
static float stepAcc, igniteAcc;
static int burnedTotal;
static uint32_t fireId;            // which flame (a mover is pushed once per flame)
static float fireX, fireY;         // where it was lit (cells): the push points away from there
static uint32_t moverFire[MAX_MOVERS];
static uint32_t vrng = 0x2545F491u;
static int firedampPockets;

// Pressure: burning vapour in a closed room (not outdoors) pushes on its edge. Per room: the
// vapour burnt per room cell lately (it leaks away), and where it burnt (the edge near the fire
// goes first)
typedef struct { uint16_t room; float p; int x0, y0, x1, y1, z; int vents; } RoomBurn;
#define MAX_ROOM_BURNS 32
static RoomBurn roomBurns[MAX_ROOM_BURNS];
static int roomBurnCount;
static int ventsTotal;
float vapourVentLeak = 1.5f;   // per game second, the share of a room's pressure that leaks away

static uint32_t VRand(void) { vrng ^= vrng << 13; vrng ^= vrng >> 17; vrng ^= vrng << 5; return vrng; }
static float VRand01(void) { return (float)(VRand() & 0xFFFFu) / 65535.0f; }

static inline bool VInBounds(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < gridWidth && y < gridHeight && z < gridDepth;
}
static inline bool CanHold(int x, int y, int z) {
    if (!VInBounds(x, y, z)) return false;
    CellType c = GRID(x, y, z);
    return !CellIsSolid(c) || c == CELL_DOOR_OPEN;   // fumes drift through an open door
}

int GetVapourLevel(int x, int y, int z) {
    if (!VInBounds(x, y, z)) return 0;
    return VAPOUR(x, y, z).level;
}

static void ListCell(int x, int y, int z) {
    VapourCell* c = &VAPOUR_MUT(x, y, z);
    if (c->listed || cellCount >= MAX_VAPOUR_CELLS) return;
    c->listed = 1;
    cells[cellCount++] = (VCell){ x, y, z };
}

static void SetLevel(int x, int y, int z, int level) {
    if (level < 0) level = 0;
    if (level > VAPOUR_MAX_LEVEL) level = VAPOUR_MAX_LEVEL;
    if (VAPOUR(x, y, z).level == level) return;
    VAPOUR_MUT(x, y, z).level = (uint8_t)level;
    if (level > 0) ListCell(x, y, z);
}

// Into a cell: an empty one takes the kind, one with the other kind in it doesn't take any
static bool Takes(int x, int y, int z, int kind) {
    VapourCell c = VAPOUR(x, y, z);
    return c.level == 0 || c.kind == kind;
}
static void SetLevelKind(int x, int y, int z, int level, int kind) {
    if (level > 0 && VAPOUR(x, y, z).kind != kind) VAPOUR_MUT(x, y, z).kind = (uint8_t)kind;
    SetLevel(x, y, z, level);
}

void AddVapourKind(int x, int y, int z, int amount, VapourKind kind) {
    if (!vapourEnabled || !CanHold(x, y, z) || amount <= 0 || !Takes(x, y, z, (int)kind)) return;
    SetLevelKind(x, y, z, GetVapourLevel(x, y, z) + amount, (int)kind);
}
void AddVapour(int x, int y, int z, int amount) { AddVapourKind(x, y, z, amount, VAPOUR_PETROL); }

int GetVapourLevelOf(int x, int y, int z, VapourKind kind) {
    if (!VInBounds(x, y, z)) return 0;
    VapourCell c = VAPOUR(x, y, z);
    return c.kind == (int)kind ? c.level : 0;
}
int GetPetrolLevel(int x, int y, int z) { return GetVapourLevelOf(x, y, z, VAPOUR_PETROL); }
int GetFiredampLevel(int x, int y, int z) { return GetVapourLevelOf(x, y, z, VAPOUR_FIREDAMP); }

int firedampChance = 15;   // setting firedamp: % of natural slate cells dug that let a pocket out
int firedampPocket = 20;   // levels of firedamp in a pocket

void VapourMinedRock(int x, int y, int z, int mat, bool natural) {
    if (!vapourEnabled || !natural || mat != MAT_SLATE || firedampChance <= 0) return;
    unsigned h = (unsigned)(x * 73856093u) ^ (unsigned)(y * 19349663u) ^ (unsigned)(z * 83492791u);
    h ^= h >> 16; h *= 0x45d9f3bu; h ^= h >> 16;
    if ((int)(h % 100u) >= firedampChance) return;
    // The pocket comes out into the cell that was dug (and the open cells beside it)
    int left = firedampPocket;
    static const int d5[5][2] = { {0,0}, {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (int k = 0; k < 5 && left > 0; k++) {
        int nx = x + d5[k][0], ny = y + d5[k][1];
        if (!CanHold(nx, ny, z) || !Takes(nx, ny, z, VAPOUR_FIREDAMP)) continue;
        int room = VAPOUR_MAX_LEVEL - GetVapourLevel(nx, ny, z), m = left < room ? left : room;
        AddVapourKind(nx, ny, z, m, VAPOUR_FIREDAMP);
        left -= m;
    }
    firedampPockets++;
    EventLog("Firedamp: a pocket let out at (%d,%d,z%d)", x, y, z);
}

void ClearVapour(void) {
    SparseLayerClear(&vapourLayer);
    cellCount = frontCount = leakCount = sparkCount = roomBurnCount = 0;
    ventsTotal = 0;
    firedampPockets = 0;
    stepAcc = igniteAcc = 0.0f;
    burnedTotal = 0;
}

bool VapourLeak(int x, int y, int z, float perSecond, float seconds) {
    if (leakCount >= MAX_LEAKS || !VInBounds(x, y, z)) return false;
    leaks[leakCount++] = (Leak){ x, y, z, perSecond, seconds, 0.0f };
    EventLog("Vapour leak at (%d,%d,z%d): %.1f/s for %.0fs", x, y, z, perSecond, seconds);
    return true;
}

bool VapourSpark(int x, int y, int z, float seconds) {
    if (sparkCount >= MAX_SPARKS || !VInBounds(x, y, z)) return false;
    sparks[sparkCount++] = (Spark){ x, y, z, seconds };
    return true;
}

static void StartBurn(int x, int y, int z, float delay) {
    if (frontCount >= MAX_FRONT) return;
    VapourCell* c = &VAPOUR_MUT(x, y, z);
    if (c->burning) return;
    c->burning = 1;
    front[frontCount++] = (FrontCell){ x, y, z, delay };
}

bool VapourIgnite(int x, int y, int z) {
    if (!vapourEnabled || !VInBounds(x, y, z)) return false;
    VapourCell c = VAPOUR(x, y, z);
    if (c.level < VAPOUR_LIGHT_LEVEL || c.burning) return false;
    fireId++;
    fireX = x + 0.5f; fireY = y + 0.5f;
    StartBurn(x, y, z, 0.0f);
    EventLog("Vapour lit at (%d,%d,z%d)", x, y, z);
    return true;
}

int VapourCellCount(void) {
    int n = 0;
    for (int i = 0; i < cellCount; i++) if (VAPOUR(cells[i].x, cells[i].y, cells[i].z).level > 0) n++;
    return n;
}
int VapourTotal(void) {
    int n = 0;
    for (int i = 0; i < cellCount; i++) n += VAPOUR(cells[i].x, cells[i].y, cells[i].z).level;
    return n;
}
int VapourBurningCount(void) { return frontCount; }
int VapourBurnedCells(void) { return burnedTotal; }
int VapourLeakCount(void) { return leakCount; }
bool VapourActive(void) { return cellCount > 0 || frontCount > 0; }
int VapourSparkCount(void) { return sparkCount; }
int VapourVentCount(void) { return ventsTotal; }
int VapourFiredampPockets(void) { return firedampPockets; }

// A cell of vapour burnt at (x, y, z): pressure in its room, if it's a closed one
static void RoomBurnt(int x, int y, int z, int lvl) {
    uint16_t rid = GetRoomAt(x, y, z);
    if (rid == ROOM_NONE) return;
    DetectedRoom* r = GetRoom(rid);
    if (!r || r->outdoors || r->cellCount <= 0) return;
    RoomBurn* b = NULL;
    for (int i = 0; i < roomBurnCount; i++) if (roomBurns[i].room == rid && roomBurns[i].z == z) { b = &roomBurns[i]; break; }
    if (!b) {
        if (roomBurnCount >= MAX_ROOM_BURNS) return;
        b = &roomBurns[roomBurnCount++];
        *b = (RoomBurn){ rid, 0.0f, x, y, x, y, z, 0 };
    }
    b->p += (float)lvl / (float)r->cellCount;
    if (x < b->x0) b->x0 = x; if (x > b->x1) b->x1 = x;
    if (y < b->y0) b->y0 = y; if (y > b->y1) b->y1 = y;
}

// The weakest cell of the room's edge near where it burnt that the pressure can blow out
static bool WeakestEdge(const RoomBurn* b, int* ox, int* oy, float* os, int* odx, int* ody) {
    static const int d4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    const int M = 8;
    float best = 1e9f;
    unsigned bestH = 0;
    bool found = false;
    for (int y = b->y0 - M; y <= b->y1 + M; y++)
        for (int x = b->x0 - M; x <= b->x1 + M; x++) {
            if (!VInBounds(x, y, b->z) || GetRoomAt(x, y, b->z) != b->room) continue;
            for (int k = 0; k < 4; k++) {
                int nx = x + d4[k][0], ny = y + d4[k][1];
                float s = ExplosionVentStrength(nx, ny, b->z);
                if (s <= 0.0f) continue;
                unsigned h = (unsigned)(nx * 73856093u) ^ (unsigned)(ny * 19349663u);
                h ^= h >> 13;
                if (s < best || (s == best && h < bestH)) {
                    best = s; bestH = h; *ox = nx; *oy = ny; *odx = d4[k][0]; *ody = d4[k][1]; found = true;
                }
            }
        }
    *os = best;
    return found;
}

static void PressureTick(float dt) {
    for (int i = 0; i < roomBurnCount; ) {
        RoomBurn* b = &roomBurns[i];
        // Blow out the weakest bit of the edge while the pressure beats it (a few at most)
        int x, y, dx, dy;
        float s;
        while (b->room != ROOM_NONE && b->vents < 4 && b->p > 0.0f && WeakestEdge(b, &x, &y, &s, &dx, &dy) && b->p >= s) {
            ExplosionVent(x, y, b->z, (float)dx, (float)dy, b->p);
            b->vents++;
            ventsTotal++;
            b->p *= 0.6f;   // out through the hole
        }
        // Rooms are found again after an edge broke (ids change): new burning there starts a new
        // record; this one only leaks away
        if (b->vents > 0) b->room = ROOM_NONE;
        b->p -= b->p * vapourVentLeak * dt;
        if (b->p < 0.05f && frontCount == 0) roomBurns[i] = roomBurns[--roomBurnCount];
        else i++;
    }
}

// ---------------------------------------------------------------------------------------------
// Moving: sink, spread level, thin out at the edges

static void Step(void) {
    static const int d4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    int n = cellCount;   // cells listed during the step move on the next one
    for (int i = 0; i < n; i++) {
        int x = cells[i].x, y = cells[i].y, z = cells[i].z;
        VapourCell c = VAPOUR(x, y, z);
        if (c.level == 0 || c.burning) continue;
        int l = c.level, kind = c.kind;
        if (!CanHold(x, y, z)) { SetLevel(x, y, z, 0); continue; }   // a wall was built here
        // Petrol is heavier than air: down while there's room below (a floor holds it). Firedamp
        // is lighter: up (under a floor or a roof of rock it stops; off the top of the map it's gone)
        int vz = kind == VAPOUR_FIREDAMP ? z + 1 : z - 1;
        bool floorBetween = kind == VAPOUR_FIREDAMP ? (vz < gridDepth && HAS_FLOOR(x, y, vz)) : HAS_FLOOR(x, y, z);
        if (kind == VAPOUR_FIREDAMP && vz >= gridDepth) { SetLevel(x, y, z, l - 1); l--; }
        else if (vz >= 0 && !floorBetween && CanHold(x, y, vz) && !VAPOUR(x, y, vz).burning && Takes(x, y, vz, kind)) {
            int b = GetVapourLevel(x, y, vz), m = VAPOUR_MAX_LEVEL - b;
            if (m > l) m = l;
            if (m > 0) { SetLevel(x, y, z, l - m); SetLevelKind(x, y, vz, b + m, kind); l -= m; }
        }
        if (l == 0) continue;
        // Level with its neighbours: one to each that has two less; to one that has one less by a
        // coin toss (so a cloud flattens out thin and wide, like a spill, instead of standing in steps)
        int start = (int)(VRand() & 3u);
        for (int j = 0; j < 4 && l > 0; j++) {
            int k = (start + j) & 3;
            int nx = x + d4[k][0], ny = y + d4[k][1];
            if (!CanHold(nx, ny, z) || VAPOUR(nx, ny, z).burning || !Takes(nx, ny, z, kind)) continue;
            int nb = GetVapourLevel(nx, ny, z);
            // (a lone wisp of 1 doesn't wander off: it stays and thins out)
            if (nb < l - 1 || (nb == l - 1 && l >= 2 && (VRand() & 1u))) { SetLevelKind(nx, ny, z, nb + 1, kind); l--; SetLevel(x, y, z, l); }
        }
        // A thin edge thins out into the air
        if (l == 1 && VRand01() < vapourThinChance) SetLevel(x, y, z, 0);
    }
    // Drop the cells that are empty now
    int w = 0;
    for (int i = 0; i < cellCount; i++) {
        VCell v = cells[i];
        VapourCell c = VAPOUR(v.x, v.y, v.z);
        if (c.level > 0 || c.burning) cells[w++] = v;
        else VAPOUR_MUT(v.x, v.y, v.z).listed = 0;
    }
    cellCount = w;
}

static bool FireNear(int x, int y, int z) {
    if (HasFire(x, y, z)) return true;
    if (z > 0 && HasFire(x, y, z - 1)) return true;   // burning ground (grass burns in the soil cell)
    static const int d4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (int k = 0; k < 4; k++) {
        int nx = x + d4[k][0], ny = y + d4[k][1];
        if (VInBounds(nx, ny, z) && HasFire(nx, ny, z)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// The flame front

#define MAX_BURNED_TICK 512
static void FrontTick(float dt) {
    VCell burned[MAX_BURNED_TICK];
    int burnedLevel[MAX_BURNED_TICK];
    int burnedN = 0;
    for (int i = 0; i < frontCount; ) {
        front[i].t -= dt;
        if (front[i].t > 0.0f) { i++; continue; }
        int x = front[i].x, y = front[i].y, z = front[i].z;
        front[i] = front[--frontCount];
        int lvl = GetVapourLevel(x, y, z);
        VAPOUR_MUT(x, y, z).burning = 0;
        SetLevel(x, y, z, 0);
        burnedTotal++;
        // On into every cell with vapour within 2 (a thin cloud is patchy: the flame jumps the
        // gaps), not through walls: a cell 2 away only past an open cell between
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    if (!dx && !dy && !dz) continue;
                    int d2 = dx * dx + dy * dy;
                    if (d2 > 5) continue;   // round-ish: no far corners
                    int nx = x + dx, ny = y + dy, nz = z + dz;
                    if (!VInBounds(nx, ny, nz)) continue;
                    VapourCell n = VAPOUR(nx, ny, nz);
                    if (n.level == 0 || n.burning) continue;
                    if (d2 > 2 && !CanHold(x + dx / 2, y + dy / 2, z)) continue;
                    float far = d2 > 2 ? 1.6f : 1.0f;
                    StartBurn(nx, ny, nz, far * (FRONT_STEP_MIN + (FRONT_STEP_MAX - FRONT_STEP_MIN) * VRand01()));
                }
        // What it does where it burns
        if (GetFuelAt(x, y, z) > 0 && !HasFire(x, y, z)) IgniteCell(x, y, z);
        if (z > 0 && GetFuelAt(x, y, z - 1) > 0 && !HasFire(x, y, z - 1)) IgniteCell(x, y, z - 1);
        int hot = 150 + lvl * 40;
        if (GetTemperature(x, y, z) < hot) { SetTemperature(x, y, z, hot); DestabilizeTemperature(x, y, z); }
        int smoke = 1 + lvl / 3;
        if (GetSmokeLevel(x, y, z) < smoke) SetSmokeLevel(x, y, z, smoke);
        FxFlame((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, z, (float)lvl);
        RoomBurnt(x, y, z, lvl);
        if (burnedN < MAX_BURNED_TICK) { burned[burnedN] = (VCell){ x, y, z }; burnedLevel[burnedN++] = lvl; }
    }
    if (burnedN == 0) return;
    // Whoever stands in it: pushed away from where it was lit, once per flame
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active || moverFire[i] == fireId) continue;
        int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE), mz = (int)m->z;
        for (int b = 0; b < burnedN; b++) {
            // in it, or under it (a flame racing along a tunnel's roof overhead)
            if (burned[b].x != mx || burned[b].y != my || (burned[b].z != mz && burned[b].z != mz + 1)) continue;
            moverFire[i] = fireId;
            ExplosionKnockMover(i, fireX, fireY, 1.5f + 0.25f * (float)burnedLevel[b]);
            break;
        }
    }
}

void VapourTick(float dt) {
    if (!vapourEnabled) return;
    for (int i = 0; i < leakCount; ) {
        Leak* L = &leaks[i];
        L->acc += L->rate * dt;
        while (L->acc >= 1.0f) { L->acc -= 1.0f; AddVapour(L->x, L->y, L->z, 1); }
        L->t -= dt;
        if (L->t <= 0.0f) leaks[i] = leaks[--leakCount];
        else i++;
    }
    for (int i = 0; i < sparkCount; ) {
        Spark* sp = &sparks[i];
        sp->t -= dt;
        if (sp->t > 0.0f) { i++; continue; }
        FxEmitSparks(sp->x, sp->y, sp->z, 0.3f);
        bool lit = VapourIgnite(sp->x, sp->y, sp->z);
        for (int r = 1; r <= 2 && !lit; r++)
            for (int dy = -r; dy <= r && !lit; dy++)
                for (int dx = -r; dx <= r && !lit; dx++) lit = VapourIgnite(sp->x + dx, sp->y + dy, sp->z);
        sparks[i] = sparks[--sparkCount];
    }
    if (frontCount > 0) FrontTick(dt);
    if (roomBurnCount > 0) PressureTick(dt);
    if (cellCount == 0) return;
    stepAcc += dt;
    if (stepAcc >= vapourStepInterval) { stepAcc -= vapourStepInterval; Step(); }
    igniteAcc += dt;
    if (igniteAcc >= IGNITE_CHECK) {
        igniteAcc -= IGNITE_CHECK;
        for (int i = 0; i < cellCount; i++) {
            int x = cells[i].x, y = cells[i].y, z = cells[i].z;
            VapourCell c = VAPOUR(x, y, z);
            if (c.level >= VAPOUR_LIGHT_LEVEL && !c.burning && FireNear(x, y, z)) VapourIgnite(x, y, z);
        }
    }
}
