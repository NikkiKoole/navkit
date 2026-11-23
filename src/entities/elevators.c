// elevators.c - passenger elevators (see elevators.h)

#include "elevators.h"
#include "mover.h"
#include "../core/time.h"
#include "../core/event_log.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <math.h>
#include <string.h>

Elevator elevators[MAX_ELEVATORS];
int elevatorCount = 0;

float elevatorSpeed = 1.5f;      // levels per second at dayLength 60
int elevatorCapacity = 4;
float elevatorDoorTime = 1.5f;
int elevatorRides = 0;

// Per mover: which car it waits for / rides, where it gets on and off
enum { ELEV_NONE, ELEV_WAITING, ELEV_RIDING };
static int moverElevState[MAX_MOVERS];
static int moverElev[MAX_MOVERS];
static int moverElevFrom[MAX_MOVERS];
static int moverElevDest[MAX_MOVERS];
static unsigned long moverElevSeen[MAX_MOVERS];   // tick a waiter last asked (stale = it left)
static unsigned long elevTick = 0;
static unsigned long lastLazyRebuild = 0;

#define ELEV_MAX_LEVELS 256

static void ForgetMover(int i) {
    moverElevState[i] = ELEV_NONE;
    moverElev[i] = -1;
}

void ElevatorForgetMover(int i) {
    if (i < 0 || i >= MAX_MOVERS) return;
    if (moverElevState[i] == ELEV_RIDING) {
        Elevator* e = &elevators[moverElev[i]];
        for (int k = 0; k < e->riderCount; k++)
            if (e->riders[k] == i) { e->riders[k] = e->riders[--e->riderCount]; break; }
    }
    ForgetMover(i);
}

static void ClearMoverState(void) {
    for (int i = 0; i < MAX_MOVERS; i++) ForgetMover(i);
}

void ClearElevators(void) {
    elevatorCount = 0;
    elevatorRides = 0;
    ClearMoverState();
}

void RebuildElevators(void) {
    Elevator old[MAX_ELEVATORS];
    int oldCount = elevatorCount;
    memcpy(old, elevators, sizeof(Elevator) * (size_t)oldCount);
    elevatorCount = 0;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            int z = 0;
            while (z < gridDepth) {
                if (GRID(x, y, z) != CELL_ELEVATOR) { z++; continue; }
                int z0 = z;
                while (z + 1 < gridDepth && GRID(x, y, z + 1) == CELL_ELEVATOR) z++;
                if (z > z0 && elevatorCount < MAX_ELEVATORS) {
                    Elevator* e = &elevators[elevatorCount++];
                    memset(e, 0, sizeof(*e));
                    e->x = x; e->y = y; e->zMin = z0; e->zMax = z;
                    e->z = (float)z0;
                    e->state = CAR_IDLE;
                    for (int k = 0; k < oldCount; k++)   // the car stays where it was
                        if (old[k].x == x && old[k].y == y && old[k].z >= z0 && old[k].z <= z) {
                            e->z = old[k].z; e->dir = old[k].dir; e->state = old[k].state;
                            e->timer = old[k].timer; e->trips = old[k].trips;
                            e->riderCount = old[k].riderCount;
                            memcpy(e->riders, old[k].riders, sizeof(e->riders));
                            break;
                        }
                }
                z++;
            }
        }
    // Movers whose car is gone or renumbered: find it again (riders) or forget them
    for (int i = 0; i < MAX_MOVERS; i++) {
        if (moverElevState[i] == ELEV_NONE) continue;
        int found = -1;
        for (int k = 0; k < elevatorCount && found < 0; k++) {
            if (moverElevState[i] == ELEV_RIDING) {
                for (int r = 0; r < elevators[k].riderCount; r++)
                    if (elevators[k].riders[r] == i) { found = k; break; }
            } else if (moverElev[i] < oldCount && old[moverElev[i]].x == elevators[k].x &&
                       old[moverElev[i]].y == elevators[k].y) {
                found = k;
            }
        }
        if (found < 0) {
            if (moverElevState[i] == ELEV_RIDING && i < moverCount) {   // car gone: stand on a level
                movers[i].z = floorf(movers[i].z + 0.5f);
                movers[i].needsRepath = true;
            }
            ForgetMover(i);
        } else {
            moverElev[i] = found;
        }
    }
}

void ElevatorsAfterLoad(void) {
    elevatorCount = 0;   // cars start at the bottom; nobody aboard
    ClearMoverState();
    RebuildElevators();
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active || m->z == floorf(m->z)) continue;
        int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)m->z;
        if (GRID(x, y, z) != CELL_ELEVATOR) continue;
        m->z = floorf(m->z + 0.5f);   // was riding: put it on the nearest level
        m->needsRepath = true;
    }
}

int ElevatorAt(int x, int y, int z) {
    for (int k = 0; k < elevatorCount; k++) {
        const Elevator* e = &elevators[k];
        if (e->x == x && e->y == y && z >= e->zMin && z <= e->zMax) return k;
    }
    return -1;
}

static int Sign(int v) { return (v > 0) - (v < 0); }

// The car stands at `level`: may the waiter with this trip get on?
static bool CanBoard(const Elevator* e, int level, int dest) {
    if (e->riderCount >= elevatorCapacity || e->riderCount >= ELEVATOR_MAX_RIDERS) return false;
    int want = Sign(dest - level);
    for (int r = 0; r < e->riderCount; r++)
        if (Sign(moverElevDest[e->riders[r]] - level) != want) return false;   // car goes the other way
    return true;
}

// Should the car stop (open its doors) at this level? Someone gets off, or someone
// waiting there can get on.
static bool StopWanted(int k, int level) {
    const Elevator* e = &elevators[k];
    for (int r = 0; r < e->riderCount; r++)
        if (moverElevDest[e->riders[r]] == level) return true;
    for (int i = 0; i < moverCount; i++)
        if (moverElevState[i] == ELEV_WAITING && moverElev[i] == k && moverElevFrom[i] == level &&
            CanBoard(e, level, moverElevDest[i]))
            return true;
    return false;
}

// Any rider destination or waiter beyond `level` in direction dir?
static bool RequestsBeyond(int k, int level, int dir) {
    const Elevator* e = &elevators[k];
    for (int r = 0; r < e->riderCount; r++)
        if (Sign(moverElevDest[e->riders[r]] - level) == dir) return true;
    for (int i = 0; i < moverCount; i++)
        if (moverElevState[i] == ELEV_WAITING && moverElev[i] == k && Sign(moverElevFrom[i] - level) == dir)
            return true;
    return false;
}

static void OpenDoors(Elevator* e, int level) {
    e->z = (float)level;
    e->state = CAR_DOORS;
    e->timer = elevatorDoorTime;
    e->trips++;
}

void ElevatorsUpdate(float dt) {
    elevTick++;
    float scale = 60.0f / dayLength;
    // Waiters that stopped asking (their path changed, job cancelled) leave the line
    for (int i = 0; i < moverCount; i++)
        if (moverElevState[i] == ELEV_WAITING && (elevTick - moverElevSeen[i] > 2 || !movers[i].active))
            ForgetMover(i);
    for (int k = 0; k < elevatorCount; k++) {
        Elevator* e = &elevators[k];
        // Riders who vanished
        for (int r = e->riderCount - 1; r >= 0; r--) {
            int i = e->riders[r];
            if (i >= moverCount || !movers[i].active || moverElevState[i] != ELEV_RIDING) {
                e->riders[r] = e->riders[--e->riderCount];
                if (i < MAX_MOVERS && moverElevState[i] == ELEV_RIDING) ForgetMover(i);
            }
        }
        if (e->state == CAR_DOORS) {
            e->timer -= dt * scale;
            if (e->timer > 0.0f) continue;
            e->state = CAR_IDLE;
        }
        if (e->state == CAR_IDLE) {
            int level = (int)floorf(e->z + 0.5f);
            if (StopWanted(k, level)) { OpenDoors(e, level); continue; }
            if (e->riderCount > 0) e->dir = Sign(moverElevDest[e->riders[0]] - level);
            else if (e->dir == 0 || !RequestsBeyond(k, level, e->dir))
                e->dir = RequestsBeyond(k, level, +1) ? +1 : RequestsBeyond(k, level, -1) ? -1 : 0;
            if (e->dir == 0) continue;
            e->state = CAR_MOVING;
        }
        if (e->state == CAR_MOVING) {
            float from = e->z;
            float to = from + (float)e->dir * elevatorSpeed * scale * dt;
            // Levels passed or reached this tick, in order
            int first = e->dir > 0 ? (int)floorf(from) + 1 : (int)ceilf(from) - 1;
            bool stopped = false;
            for (int level = first; e->dir > 0 ? level <= (int)floorf(to) : level >= (int)ceilf(to); level += e->dir) {
                if (level < e->zMin || level > e->zMax) break;
                bool nothingLeft = !RequestsBeyond(k, level, e->dir);
                if (StopWanted(k, level)) { OpenDoors(e, level); stopped = true; break; }
                if (nothingLeft || level == e->zMin || level == e->zMax) {
                    e->z = (float)level; e->state = CAR_IDLE; stopped = true; break;
                }
            }
            if (!stopped) {
                if (to > (float)e->zMax) to = (float)e->zMax;
                if (to < (float)e->zMin) to = (float)e->zMin;
                e->z = to;
            }
        }
    }
}

// Last level of the shaft run on the mover's path from its next waypoint on (the level
// where it gets off); fromZ if the next waypoint isn't in this shaft.
static int PathShaftDest(int i, const Elevator* e, int fromZ) {
    const Mover* m = &movers[i];
    int dest = fromZ;
    for (int k = m->pathIndex; k >= 0; k--) {
        Point p = moverPaths[i][k];
        if (p.x != e->x || p.y != e->y || p.z < e->zMin || p.z > e->zMax) break;
        dest = p.z;
    }
    return dest;
}

static void HoldStill(Mover* m) {
    m->timeWithoutProgress = 0.0f;   // waiting for / riding the car is not being stuck
    m->lastX = m->x;
    m->lastY = m->y;
}

bool ElevatorWaitAt(int i, Point target, int fromZ) {
    if (i < 0 || i >= MAX_MOVERS) return false;
    Mover* m = &movers[i];
    int k = ElevatorAt(target.x, target.y, fromZ);
    if (k < 0 && elevTick != lastLazyRebuild) {   // shaft built since the last rebuild
        lastLazyRebuild = elevTick;
        RebuildElevators();
        k = ElevatorAt(target.x, target.y, fromZ);
    }
    if (k < 0) return false;
    Elevator* e = &elevators[k];
    int dest = PathShaftDest(i, e, fromZ);
    if (dest == fromZ) return false;
    if (moverElevState[i] != ELEV_WAITING || moverElev[i] != k || moverElevFrom[i] != fromZ) {
        moverElevState[i] = ELEV_WAITING;
        moverElev[i] = k;
        moverElevFrom[i] = fromZ;
    }
    moverElevDest[i] = dest;
    moverElevSeen[i] = elevTick;
    m->x = e->x * CELL_SIZE + CELL_SIZE * 0.5f;
    m->y = e->y * CELL_SIZE + CELL_SIZE * 0.5f;
    m->z = (float)fromZ;
    HoldStill(m);
    if (e->state == CAR_DOORS && (int)floorf(e->z + 0.5f) == fromZ && CanBoard(e, fromZ, dest)) {
        e->riders[e->riderCount++] = i;
        moverElevState[i] = ELEV_RIDING;
        elevatorRides++;
        EventLog("Mover %d boards elevator %d at z%d for z%d", i, k, fromZ, dest);
    }
    return true;
}

bool ElevatorRiderTick(int i) {
    if (i < 0 || i >= MAX_MOVERS || moverElevState[i] != ELEV_RIDING) return false;
    Mover* m = &movers[i];
    Elevator* e = &elevators[moverElev[i]];
    // A new path while aboard: get off where it leaves the shaft (or at the next level)
    Point next = m->pathIndex >= 0 ? moverPaths[i][m->pathIndex] : (Point){-1, -1, -1};
    if (next.x == e->x && next.y == e->y && next.z >= e->zMin && next.z <= e->zMax) {
        int dest = PathShaftDest(i, e, (int)floorf(e->z + 0.5f));
        if (dest != (int)floorf(e->z + 0.5f) || e->state == CAR_DOORS) moverElevDest[i] = dest;
    }
    m->x = e->x * CELL_SIZE + CELL_SIZE * 0.5f;
    m->y = e->y * CELL_SIZE + CELL_SIZE * 0.5f;
    m->z = e->z;
    HoldStill(m);
    int level = (int)floorf(e->z + 0.5f);
    if (e->state != CAR_DOORS || level != moverElevDest[i]) return true;
    // Off: skip the shaft waypoints the ride covered
    m->z = (float)level;
    while (m->pathIndex >= 0 && moverPaths[i][m->pathIndex].x == e->x && moverPaths[i][m->pathIndex].y == e->y)
        m->pathIndex--;
    m->timeNearWaypoint = 0.0f;
    ElevatorForgetMover(i);
    return true;
}

bool ElevatorMoverAboard(int i) {
    return i >= 0 && i < MAX_MOVERS && moverElevState[i] == ELEV_RIDING;
}

bool ElevatorMoverWaiting(int i) {
    return i >= 0 && i < MAX_MOVERS && moverElevState[i] == ELEV_WAITING;
}
