// elevators.c - passenger elevators (see elevators.h)

#include "elevators.h"
#include "mover.h"
#include "../core/time.h"
#include "../core/event_log.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/balance.h"
#include "ladders.h"
#include "items.h"
#include "../world/material.h"
#include "../world/pathfinding.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Elevator elevators[MAX_ELEVATORS];
int elevatorCount = 0;

float elevatorSpeed = 3.0f;      // levels per second at dayLength 60 (1.5 lost every trip to the stairs)
int elevatorCapacity = 4;
float elevatorDoorTime = 0.75f;
int elevatorRides = 0;
bool useElevatorWaitCost = true;
bool elevatorDirectionalPickup = true;
int elevatorPlanningMover = -1;   // set by MoverPlanPath while that mover's route is searched
// Wait and ride samples (seconds), for the distribution
#define ELEV_SAMPLES 8192
static float waitSamples[ELEV_SAMPLES], rideSamples[ELEV_SAMPLES];
static int waitFloor[ELEV_SAMPLES], rideFloors[ELEV_SAMPLES];
static int waitSampleCount = 0, rideSampleCount = 0;
static int lineJoins = 0, lineLeaves = 0;   // joined a line; left one without boarding
static int leaveWhy[4];                     // debug: 0 after my re-plan, 1 other new path, 2 mid-climb/no path, 3 no car
static unsigned long moverReplanTick[MAX_MOVERS];
static int moverLineLeaves[MAX_MOVERS];   // lines this mover left without boarding (stats)
static double moverRideStart[MAX_MOVERS];
static int moverRideFromZ[MAX_MOVERS];
double elevatorWaitSum = 0.0;
float elevatorWaitMax = 0.0f;
int elevatorWaitCount = 0, elevatorDetours = 0;

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

static bool moverSteppingIn[MAX_MOVERS];   // boarded, still walking from the line into the car
static void ResetDoors(void);
static void ForgetMover(int i) {
    moverSteppingIn[i] = false;
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
    elevatorWaitSum = 0.0; elevatorWaitMax = 0.0f; elevatorWaitCount = 0; elevatorDetours = 0;
    waitSampleCount = rideSampleCount = 0;
    lineJoins = lineLeaves = 0;
    ClearMoverState();
}

void RebuildElevators(void) {
    Elevator old[MAX_ELEVATORS];
    int oldCount = elevatorCount;
    memcpy(old, elevators, sizeof(Elevator) * (size_t)oldCount);
    elevatorCount = 0;
    ResetDoors();
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
                            e->rideSecPerLevel = old[k].rideSecPerLevel; e->rideN = old[k].rideN;
                            memcpy(e->levelWait, old[k].levelWait, sizeof(e->levelWait));
                            memcpy(e->levelWaitN, old[k].levelWaitN, sizeof(e->levelWaitN));
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

static bool RequestsBeyond(int k, int level, int dir);

// The car stands at `level`: may the waiter with this trip get on? There must be room,
// and the car must be going the waiter's way: its riders' way, or, empty, the way it is
// sweeping while calls still wait further on (elevatorDirectionalPickup: otherwise an
// empty car turns round for anyone and the calls further on wait).
static bool CanBoard(const Elevator* e, int level, int dest) {
    if (e->riderCount >= elevatorCapacity || e->riderCount >= ELEVATOR_MAX_RIDERS) return false;
    int want = Sign(dest - level);
    for (int r = 0; r < e->riderCount; r++)
        if (Sign(moverElevDest[e->riders[r]] - level) != want) return false;   // car goes the other way
    if (e->riderCount == 0 && elevatorDirectionalPickup && e->dir != 0 && want != e->dir &&
        RequestsBeyond((int)(e - elevators), level, e->dir))
        return false;   // empty, but on its way to calls further on: on the way back
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

bool ElevatorCarBoarding(int k);

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
            if (e->timer > 0.0f || ElevatorCarBoarding(k)) continue;   // doors wait for who steps in
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

// Last level of the shaft run on the mover's path from waypoint `from` on (the level
// where it gets off); fromZ if that waypoint isn't in this shaft.
static int PathShaftDestFrom(int i, const Elevator* e, int from, int fromZ) {
    int dest = fromZ;
    for (int k = from; k >= 0; k--) {
        Point p = moverPaths[i][k];
        if (p.x != e->x || p.y != e->y || p.z < e->zMin || p.z > e->zMax) break;
        dest = p.z;
    }
    return dest;
}

static int PathShaftDest(int i, const Elevator* e, int fromZ) {
    return PathShaftDestFrom(i, e, movers[i].pathIndex, fromZ);
}

// Measured seconds -> seconds at dayLength 60 (what the costs are in)
static float At60(float sec) { return dayLength > 0.0f ? sec * 60.0f / dayLength : sec; }

static void NoteAvg(float* avg, int n, float v) {
    *avg = n == 0 ? v : *avg * 0.8f + v * 0.2f;
}

static void HoldStill(Mover* m) {
    m->timeWithoutProgress = 0.0f;   // waiting for / riding the car is not being stuck
    m->lastX = m->x;
    m->lastY = m->y;
}

static float WalkPxPerSec(const Mover* m) { return m->speed * (60.0f / dayLength); }

// Step toward (tx, ty) at walking speed; true once there
static bool StepToward(Mover* m, float tx, float ty, float dt) {
    float dx = tx - m->x, dy = ty - m->y, d = sqrtf(dx * dx + dy * dy);
    float step = WalkPxPerSec(m) * dt;
    if (d <= step || d < 1.0f) { m->x = tx; m->y = ty; return true; }
    m->x += dx / d * step;
    m->y += dy / d * step;
    return false;
}

static int LazyElevatorAt(int x, int y, int z) {
    int k = ElevatorAt(x, y, z);
    if (k < 0 && elevTick != lastLazyRebuild) {   // shaft built since the last rebuild
        lastLazyRebuild = elevTick;
        RebuildElevators();
        k = ElevatorAt(x, y, z);
    }
    return k;
}

// --- The line at the door -------------------------------------------------------
// On each level the line starts next to the shaft, on the side with the longest
// straight run of walkable cells, and goes straight out from there (a second row
// beside it when it is longer than the run).
#define ELEV_LINE_MAX_RUN 6
static signed char doorDir[MAX_ELEVATORS][ELEV_MAX_LEVELS];   // 0..3, -1 none, -2 not computed yet
static unsigned char doorRun[MAX_ELEVATORS][ELEV_MAX_LEVELS];
static const int DOOR_DX[4] = {0, 1, 0, -1}, DOOR_DY[4] = {1, 0, -1, 0};   // S first: in front on screen

static void ResetDoors(void) { memset(doorDir, -2, sizeof(doorDir)); }

static bool LineCellOk(int x, int y, int z) {
    return x >= 0 && y >= 0 && x < gridWidth && y < gridHeight && IsCellWalkableAt(z, y, x) &&
           !CellIsShaft(GRID(x, y, z));
}

static int DoorDir(int k, int level, int* run) {
    if (level < 0 || level >= ELEV_MAX_LEVELS) { *run = 0; return -1; }
    if (doorDir[k][level] == -2) {
        const Elevator* e = &elevators[k];
        int best = -1, bestRun = 0;
        for (int d = 0; d < 4; d++) {
            int n = 0;
            while (n < ELEV_LINE_MAX_RUN && LineCellOk(e->x + DOOR_DX[d] * (n + 1), e->y + DOOR_DY[d] * (n + 1), level)) n++;
            if (n > bestRun) { bestRun = n; best = d; }
        }
        doorDir[k][level] = (signed char)best;
        doorRun[k][level] = (unsigned char)bestRun;
    }
    *run = doorRun[k][level];
    return doorDir[k][level];
}

// Where the waiter with this place in the line (0 = front) stands, in pixels
static void LineSpot(int k, int level, int place, float* px, float* py) {
    const Elevator* e = &elevators[k];
    int run, d = DoorDir(k, level, &run);
    int cx = e->x, cy = e->y;   // no room: wait in the shaft cell (the old way)
    if (d >= 0 && run > 0) {
        int col = place % run, row = place / run;
        cx = e->x + DOOR_DX[d] * (col + 1);
        cy = e->y + DOOR_DY[d] * (col + 1);
        if (row > 0) {   // rows beside the first, alternating sides
            int side = (row % 2) ? 1 : -1, off = (row + 1) / 2;
            int rx = cx + DOOR_DY[d] * side * off, ry = cy - DOOR_DX[d] * side * off;
            if (LineCellOk(rx, ry, level)) { cx = rx; cy = ry; }
        }
    }
    *px = cx * CELL_SIZE + CELL_SIZE * 0.5f;
    *py = cy * CELL_SIZE + CELL_SIZE * 0.5f;
}

// Walking to its spot, a mover goes around the shaft, not through it: if the straight
// line passes the shaft's middle, head for the free cell beside the shaft nearest to it.
static void LineDetour(int k, int level, const Mover* m, float* tx, float* ty) {
    const Elevator* e = &elevators[k];
    float cx = e->x * CELL_SIZE + CELL_SIZE * 0.5f, cy = e->y * CELL_SIZE + CELL_SIZE * 0.5f;
    float dx = *tx - m->x, dy = *ty - m->y, len2 = dx * dx + dy * dy;
    if (len2 < 1.0f) return;
    float t = ((cx - m->x) * dx + (cy - m->y) * dy) / len2;
    if (t <= 0.0f || t >= 1.0f) return;
    float px = m->x + dx * t - cx, py = m->y + dy * t - cy;
    if (px * px + py * py > (CELL_SIZE * 0.6f) * (CELL_SIZE * 0.6f)) return;
    float best = 1e30f;
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
            if (!ox && !oy) continue;
            if (!LineCellOk(e->x + ox, e->y + oy, level)) continue;
            float wx = cx + ox * CELL_SIZE, wy = cy + oy * CELL_SIZE;
            // a side cell: not across the shaft from the mover, closest to the mover
            if ((wx - cx) * (m->x - cx) + (wy - cy) * (m->y - cy) < 0.0f) continue;
            float d = (wx - m->x) * (wx - m->x) + (wy - m->y) * (wy - m->y);
            if (fabsf((float)(ox * dx + oy * dy)) > 0.9f * sqrtf(len2) * sqrtf((float)(ox * ox + oy * oy))) continue;   // in line with the walk
            if (d < best) { best = d; *tx = wx; *ty = wy; }
        }
}

static unsigned long lineOrderNext = 1;
static unsigned long moverLineOrder[MAX_MOVERS];   // when it joined the line (first come, first served)
static float moverLineWait[MAX_MOVERS];            // seconds in this line
#define ELEV_REPLAN_AT_1 4.0f    // a waiting mover re-plans at these wait times (x patience):
#define ELEV_REPLAN_AT_2 10.0f   // with the wait in the route cost, the stairs may now be quicker
#define ELEV_REPLAN_EVERY 15.0f  // and again every this long after that (x patience)
#define ELEV_REPLAN_MIN 3.0f     // never before this, however impatient
#define ELEV_STAY_FACTOR 0.75f   // a waiter re-planning sees its own line's wait at this much

static void JoinLine(int i, int k, int fromZ) {
    if (moverElevState[i] == ELEV_WAITING && moverElev[i] == k && moverElevFrom[i] == fromZ) return;
    moverElevState[i] = ELEV_WAITING;
    moverElev[i] = k;
    moverElevFrom[i] = fromZ;
    lineJoins++;
    moverLineOrder[i] = lineOrderNext++;
    moverLineWait[i] = 0.0f;
}

static int PlaceInLine(int i) {
    int n = 0;
    for (int j = 0; j < moverCount; j++)
        if (j != i && moverElevState[j] == ELEV_WAITING && moverElev[j] == moverElev[i] &&
            moverElevFrom[j] == moverElevFrom[i] && moverLineOrder[j] < moverLineOrder[i]) n++;
    return n;
}

// No one ahead in the line who could get on this car now
static bool FirstToBoard(int i) {
    const Elevator* e = &elevators[moverElev[i]];
    for (int j = 0; j < moverCount; j++)
        if (j != i && moverElevState[j] == ELEV_WAITING && moverElev[j] == moverElev[i] &&
            moverElevFrom[j] == moverElevFrom[i] && moverLineOrder[j] < moverLineOrder[i] &&
            CanBoard(e, moverElevFrom[i], moverElevDest[j])) return false;
    return true;
}

// The doors are open on the waiter's level, it is first and there is room: get on. Its
// path continues at the shaft waypoint `shaftWp` (the first level change).
static bool TryBoard(int i, int shaftWp) {
    Elevator* e = &elevators[moverElev[i]];
    int fromZ = moverElevFrom[i];
    if (e->state != CAR_DOORS || (int)floorf(e->z + 0.5f) != fromZ) return false;
    if (!CanBoard(e, fromZ, moverElevDest[i]) || !FirstToBoard(i)) return false;
    // Stats: how long it waited; a detour = an empty car taking someone the other way
    // while calls further on in its direction still wait
    if (e->riderCount == 0 && e->dir != 0 && Sign(moverElevDest[i] - fromZ) != e->dir &&
        RequestsBeyond(moverElev[i], fromZ, e->dir)) elevatorDetours++;
    elevatorWaitSum += moverLineWait[i];
    elevatorWaitCount++;
    if (waitSampleCount < ELEV_SAMPLES) {
        waitFloor[waitSampleCount] = fromZ;
        waitSamples[waitSampleCount++] = moverLineWait[i];
    {
        int lv = fromZ < ELEVATOR_STAT_LEVELS ? fromZ : ELEVATOR_STAT_LEVELS - 1;
        NoteAvg(&e->levelWait[lv], e->levelWaitN[lv], At60(moverLineWait[i]));
        if (e->levelWaitN[lv] < 255) e->levelWaitN[lv]++;
    }
    }
    moverRideStart[i] = gameTime;
    moverRideFromZ[i] = fromZ;
    if (moverLineWait[i] > elevatorWaitMax) elevatorWaitMax = moverLineWait[i];
    e->riders[e->riderCount++] = i;
    moverElevState[i] = ELEV_RIDING;
    moverSteppingIn[i] = true;
    movers[i].pathIndex = shaftWp;
    elevatorRides++;
    EventLog("Mover %d boards elevator %d at z%d for z%d", i, moverElev[i], fromZ, moverElevDest[i]);
    return true;
}

#define ELEV_LOOKAHEAD 8                       // waypoints scanned for an upcoming shaft
#define ELEV_JOIN_RADIUS (CELL_SIZE * 4.0f)    // join the line this close to the shaft

bool ElevatorQueueTick(int i, float dt) {
    if (i < 0 || i >= MAX_MOVERS || moverElevState[i] == ELEV_RIDING) return false;
    Mover* m = &movers[i];
    bool waiting = moverElevState[i] == ELEV_WAITING;
    if (m->z != floorf(m->z) || m->pathIndex < 0) { if (waiting) { leaveWhy[2]++; lineLeaves++; ForgetMover(i); } return false; }
    int mz = (int)m->z;
    // The next level change on the path, if it is a shaft on this level
    int wp = -1;
    bool levelChange = false;
    for (int q = m->pathIndex; q >= 0 && q >= m->pathIndex - ELEV_LOOKAHEAD; q--) {
        Point p = moverPaths[i][q];
        if (p.z == mz) continue;
        if (CellIsShaft(GRID(p.x, p.y, p.z)) && CellIsShaft(GRID(p.x, p.y, mz))) wp = q;
        levelChange = true;
        break;
    }
    // A coarse path's window may end at the shaft door, the ride in the next legs
    if (wp < 0 && !levelChange && m->pathIndex <= ELEV_LOOKAHEAD) {
        Point end = moverPaths[i][0];
        float ex = end.x * CELL_SIZE + CELL_SIZE * 0.5f, ey = end.y * CELL_SIZE + CELL_SIZE * 0.5f;
        if (end.z == mz && CellIsShaft(GRID(end.x, end.y, mz)) &&
            fabsf(ex - m->x) <= ELEV_JOIN_RADIUS && fabsf(ey - m->y) <= ELEV_JOIN_RADIUS &&
            MoverRefillRouteNow(i)) {
            for (int q = m->pathIndex; q >= 0 && q >= m->pathIndex - ELEV_LOOKAHEAD; q--) {
                Point p = moverPaths[i][q];
                if (p.z == mz) continue;
                if (CellIsShaft(GRID(p.x, p.y, p.z)) && CellIsShaft(GRID(p.x, p.y, mz))) wp = q;
                break;
            }
        }
    }
    if (wp < 0) { if (waiting) { leaveWhy[elevTick - moverReplanTick[i] < 30 ? 0 : 1]++; lineLeaves++; moverLineLeaves[i]++; ForgetMover(i); } return false; }
    Point sp = moverPaths[i][wp];
    float sx = sp.x * CELL_SIZE + CELL_SIZE * 0.5f, sy = sp.y * CELL_SIZE + CELL_SIZE * 0.5f;
    if (!waiting && (fabsf(sx - m->x) > ELEV_JOIN_RADIUS || fabsf(sy - m->y) > ELEV_JOIN_RADIUS)) return false;
    int k = LazyElevatorAt(sp.x, sp.y, mz);
    if (k < 0) { if (waiting) ForgetMover(i); return false; }
    int dest = PathShaftDestFrom(i, &elevators[k], wp, mz);
    if (dest == mz) { if (waiting) ForgetMover(i); return false; }
    JoinLine(i, k, mz);
    moverElevDest[i] = dest;
    moverElevSeen[i] = elevTick;
    if (!TryBoard(i, wp)) {
        float before = moverLineWait[i], pat = MoverPatience(m);
        moverLineWait[i] += dt;
        float r1 = ELEV_REPLAN_AT_1 * pat, r2 = ELEV_REPLAN_AT_2 * pat;
        if (r1 < ELEV_REPLAN_MIN) r1 = ELEV_REPLAN_MIN;
        if (r2 < r1 + ELEV_REPLAN_MIN) r2 = r1 + ELEV_REPLAN_MIN;
        float re = ELEV_REPLAN_EVERY * pat;
        bool again = moverLineWait[i] > r2 && floorf((moverLineWait[i] - r2) / re) > floorf((before - r2) / re);
        if (useElevatorWaitCost && ((before < r1 && moverLineWait[i] >= r1) || (before < r2 && moverLineWait[i] >= r2) || again))
{ m->needsRepath = true; moverReplanTick[i] = elevTick; }   // maybe the stairs are quicker now
        float tx, ty;
        LineSpot(k, mz, PlaceInLine(i), &tx, &ty);
        LineDetour(k, mz, m, &tx, &ty);
        StepToward(m, tx, ty, dt);
    }
    HoldStill(m);
    return true;
}

bool ElevatorWaitAt(int i, Point target, int fromZ) {
    if (i < 0 || i >= MAX_MOVERS) return false;
    Mover* m = &movers[i];
    int k = LazyElevatorAt(target.x, target.y, fromZ);
    if (k < 0) return false;
    Elevator* e = &elevators[k];
    int dest = PathShaftDest(i, e, fromZ);
    if (dest == fromZ) return false;
    JoinLine(i, k, fromZ);   // reached the shaft without passing the line (spawned there, ...)
    moverElevDest[i] = dest;
    moverElevSeen[i] = elevTick;
    m->x = e->x * CELL_SIZE + CELL_SIZE * 0.5f;
    m->y = e->y * CELL_SIZE + CELL_SIZE * 0.5f;
    m->z = (float)fromZ;
    HoldStill(m);
    TryBoard(i, m->pathIndex);
    moverSteppingIn[i] = false;   // already in the shaft cell
    return true;
}

bool ElevatorRiderTick(int i, float dt) {
    if (i < 0 || i >= MAX_MOVERS || moverElevState[i] != ELEV_RIDING) return false;
    Mover* m = &movers[i];
    Elevator* e = &elevators[moverElev[i]];
    float cx = e->x * CELL_SIZE + CELL_SIZE * 0.5f, cy = e->y * CELL_SIZE + CELL_SIZE * 0.5f;
    HoldStill(m);
    if (moverSteppingIn[i]) {   // walking from the line into the car (the doors wait)
        m->z = (float)moverElevFrom[i];
        if (StepToward(m, cx, cy, dt)) moverSteppingIn[i] = false;
        HoldStill(m);
        return true;
    }
    // A new path while aboard: get off where it leaves the shaft (or at the next level)
    Point next = m->pathIndex >= 0 ? moverPaths[i][m->pathIndex] : (Point){-1, -1, -1};
    if (next.x == e->x && next.y == e->y && next.z >= e->zMin && next.z <= e->zMax) {
        int dest = PathShaftDest(i, e, (int)floorf(e->z + 0.5f));
        if (dest != (int)floorf(e->z + 0.5f) || e->state == CAR_DOORS) moverElevDest[i] = dest;
    }
    m->x = cx;
    m->y = cy;
    m->z = e->z;
    int level = (int)floorf(e->z + 0.5f);
    if (e->state != CAR_DOORS || level != moverElevDest[i]) return true;
    // Off: skip the shaft waypoints the ride covered
    if (rideSampleCount < ELEV_SAMPLES) {
        rideFloors[rideSampleCount] = abs(level - moverRideFromZ[i]);
        rideSamples[rideSampleCount++] = (float)(gameTime - moverRideStart[i]);
    }
    {
        int floors = abs(level - moverRideFromZ[i]);
        if (floors > 0) {
            NoteAvg(&e->rideSecPerLevel, e->rideN, At60((float)(gameTime - moverRideStart[i])) / (float)floors);
            e->rideN++;
        }
    }
    m->z = (float)level;
    while (m->pathIndex >= 0 && moverPaths[i][m->pathIndex].x == e->x && moverPaths[i][m->pathIndex].y == e->y)
        m->pathIndex--;
    m->timeNearWaypoint = 0.0f;
    ElevatorForgetMover(i);
    return true;
}

bool ElevatorCarBoarding(int k) {
    const Elevator* e = &elevators[k];
    for (int r = 0; r < e->riderCount; r++) if (moverSteppingIn[e->riders[r]]) return true;
    return false;
}

bool ElevatorMoverAboard(int i) {
    return i >= 0 && i < MAX_MOVERS && moverElevState[i] == ELEV_RIDING;
}

bool ElevatorMoverWaiting(int i) {
    return i >= 0 && i < MAX_MOVERS && moverElevState[i] == ELEV_WAITING;
}

// --- Route cost -------------------------------------------------------------------
// A ride costs, in path cost at the base walking pace:
//  - boarding, once (ElevatorBoardCost, NAV_ENTER_EXTRA: stepping into the shaft cell on
//    the level where the ride starts): the expected wait there, i.e. the doors, the car
//    coming to this level, and one round trip of the car per full car of people already
//    in this level's line (with useElevatorWaitCost off: the doors only)
//  - each level (ElevatorExtraCost, NAV_CLIMB_EXTRA on top of the one cell a ladder link
//    costs): the car's travel time for the level
// A shaft without a car (not rebuilt yet) costs like a ladder.
static float SecPerCellAt60(void) {
    float base = balance.baseMoverSpeed > 0.0f ? balance.baseMoverSpeed : 200.0f;
    return (float)CELL_SIZE / base;   // seconds per cell of walking, at dayLength 60 like the car
}

static int SecondsToCost(float sec) { return (int)(sec / SecPerCellAt60() * 10.0f + 0.5f); }

int ElevatorBoardCost(int x, int y, int z) {
    int k = ElevatorAt(x, y, z);
    if (k < 0) return 0;
    const Elevator* e = &elevators[k];
    float speed = elevatorSpeed > 0.05f ? elevatorSpeed : 0.05f;
    float wait = 2.0f * elevatorDoorTime;   // the doors where it gets on, and where it gets off
    if (useElevatorWaitCost) {
        int line = 0;
        float longest = 0.0f;   // the longest anyone else in this line has waited so far
        int me = elevatorPlanningMover;
        bool mine = me >= 0 && me < MAX_MOVERS && moverElevState[me] == ELEV_WAITING &&
                    moverElev[me] == k && moverElevFrom[me] == z;
        for (int i = 0; i < moverCount; i++)
            if (moverElevState[i] == ELEV_WAITING && moverElev[i] == k && moverElevFrom[i] == z && i != me) {
                if (!mine || moverLineOrder[i] < moverLineOrder[me]) line++;   // in my own line: who is ahead
                if (moverLineWait[i] > longest) longest = moverLineWait[i];
            }
        int cap = elevatorCapacity < 1 ? 1 : elevatorCapacity;
        float roundTrip = 2.0f * (float)(e->zMax - e->zMin) / speed + 2.0f * elevatorDoorTime;
        wait += fabsf(e->z - (float)z) / speed + (float)(line / cap) * roundTrip;
        // What we can see beats the estimate: a floor the car keeps passing full has
        // people who waited long; then the stairs look better to them and to newcomers
        if (At60(longest) > wait) wait = At60(longest);
        int lv = z < ELEVATOR_STAT_LEVELS ? z : ELEVATOR_STAT_LEVELS - 1;
        if (e->levelWaitN[lv] >= 3 && e->levelWait[lv] + 2.0f * elevatorDoorTime > wait)   // measured here
            wait = e->levelWait[lv] + 2.0f * elevatorDoorTime;
        // Re-planning in its own line: what it already waited is gone (not a reason to
        // leave), and staying gets 25% off, so it leaves only for a clearly better way
        if (mine) wait *= ELEV_STAY_FACTOR;
    }
    return SecondsToCost(wait);
}

// Chance that the car stops at a given level of its ride for someone else: the people
// it carries or will pick up (riders + everyone in its lines), spread over the shaft's
// levels, at most 1. Each such stop costs a door cycle.
static float StopChancePerLevel(int k) {
    const Elevator* e = &elevators[k];
    int people = e->riderCount;
    for (int i = 0; i < moverCount; i++)
        if (moverElevState[i] == ELEV_WAITING && moverElev[i] == k) people++;
    int levels = e->zMax - e->zMin;
    if (levels < 1) levels = 1;
    float p = (float)people / (float)levels;
    return p > 1.0f ? 1.0f : p;
}

int ElevatorExtraCost(int x, int y, int z, int dz) {
    int k = ElevatorAt(x, y, z);
    if (k < 0 || ElevatorAt(x, y, z + dz) != k) return 0;
    float speed = elevatorSpeed > 0.05f ? elevatorSpeed : 0.05f;
    float sec = 1.0f / speed;
    if (useElevatorWaitCost) {
        sec += StopChancePerLevel(k) * elevatorDoorTime;   // stops on the way
        const Elevator* e = &elevators[k];
        if (e->rideN >= 3) sec = e->rideSecPerLevel;      // what rides here really take
    }
    int extra = SecondsToCost(sec) - 10;   // one cell is already charged
    return extra > 0 ? extra : 0;
}

// --- Built stairs ----------------------------------------------------------------
// A built ramp (made from an item: CONSTRUCTION_RAMP records its source item; terrain
// slopes and dug ramps have none) is stairs:
// climbed at stairsClimbSpeed of walking (down a bit quicker), slower than a slope but
// quicker than a ladder (ladderClimbSpeed). Routes see the extra time plus a little
// effort (stairsEffort cells of walking up, half down), so movers take the elevator for
// long trips. Natural slopes stay as they were.
float stairsClimbSpeed = 0.2f;   // a storey in one ramp cell: ~7 cells of walking (0.7 made stairs beat any car)
float stairsEffort = 2.0f;

static bool IsBuiltStairs(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    return CellIsDirectionalRamp(GRID(x, y, z)) && GetWallSourceItem(x, y, z) != ITEM_NONE;
}

// Scenarios: mark a ramp as built (planks), as CONSTRUCTION_RAMP would
void MarkBuiltStairs(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return;
    SetWallSourceItem(x, y, z, ITEM_PLANKS);
    SetWallFinish(x, y, z, FINISH_SMOOTH);
    ClearWallNatural(x, y, z);
}

static float StairsSpeed(int dz) {
    float up = stairsClimbSpeed < 0.1f ? 0.1f : (stairsClimbSpeed > 1.0f ? 1.0f : stairsClimbSpeed);
    if (dz > 0) return up;
    float down = up * 1.4f;   // going down is quicker
    return down > 1.0f ? 1.0f : down;
}

float StairsSpeedFactor(int rampX, int rampY, int rampZ, int dz) {
    return IsBuiltStairs(rampX, rampY, rampZ) ? StairsSpeed(dz) : 1.0f;
}

// NAV_RAMP_EXTRA: the ramp at (x,y,z) taken up (dz +1) or down (-1)
int RampEffortCost(int x, int y, int z, int dz) {
    if (!IsBuiltStairs(x, y, z)) return 0;
    float cells = (1.0f / StairsSpeed(dz) - 1.0f) + (dz > 0 ? stairsEffort : stairsEffort * 0.5f);
    return cells > 0.0f ? (int)(cells * 10.0f + 0.5f) : 0;
}

int ClimbExtraCost(int x, int y, int z, int dz) {
    int zt = z + dz;
    if (zt >= 0 && zt < gridDepth && CellIsShaft(GRID(x, y, z)) && CellIsShaft(GRID(x, y, zt)))
        return ElevatorExtraCost(x, y, z, dz);
    return LadderQueueExtraCost(x, y, z, dz);
}

// --- Building and removing shafts ----------------------------------------------
// Like a ladder: built on a walkable cell, a shaft goes from there up one level (cutting
// the floor above); built on the top of a shaft, it extends it one more level. A car
// runs as soon as the shaft spans two levels (RebuildElevators).

static bool ShaftRoomAbove(int x, int y, int z) {
    return z + 1 < gridDepth && GRID(x, y, z + 1) == CELL_AIR;
}

bool CanPlaceShaft(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    CellType c = GRID(x, y, z);
    if (c == CELL_ELEVATOR) return ShaftRoomAbove(x, y, z);   // the top: extend it
    if (!IsCellWalkableAt(z, y, x) || c != CELL_AIR) return false;   // not on ramps, ladders, doors, track
    return ShaftRoomAbove(x, y, z);
}

static void SetShaftCell(int x, int y, int z, MaterialType mat) {
    PushItemsOutOfCell(x, y, z);
    GRID(x, y, z) = CELL_ELEVATOR;
    CLEAR_FLOOR(x, y, z);   // a hole in the floor up here
    SetWallMaterial(x, y, z, mat);
    MarkChunkDirty(x, y, z);
    InvalidatePathsThroughCell(x, y, z);
}

bool PlaceShaft(int x, int y, int z, MaterialType mat) {
    if (!CanPlaceShaft(x, y, z)) return false;
    if (mat == MAT_NONE) mat = MAT_OAK;
    if (GRID(x, y, z) != CELL_ELEVATOR) SetShaftCell(x, y, z, mat);
    SetShaftCell(x, y, z + 1, mat);
    if (z + 2 < gridDepth) MarkChunkDirty(x, y, z + 2);
    RebuildElevators();
    return true;
}

// Take down the whole shaft through (x, y, z): each cell back to air (a floor again where
// the level around it has one), one plank back per level. Riders get off on the nearest
// level (RebuildElevators). Returns the levels removed.
int RemoveShaftAt(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return 0;
    if (GRID(x, y, z) != CELL_ELEVATOR) return 0;
    int z0 = z, z1 = z;
    while (z0 > 0 && GRID(x, y, z0 - 1) == CELL_ELEVATOR) z0--;
    while (z1 + 1 < gridDepth && GRID(x, y, z1 + 1) == CELL_ELEVATOR) z1++;
    for (int zz = z0; zz <= z1; zz++) {
        GRID(x, y, zz) = CELL_AIR;
        bool floorAround = false;
        static const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (nx >= 0 && ny >= 0 && nx < gridWidth && ny < gridHeight && HAS_FLOOR(nx, ny, zz)) floorAround = true;
        }
        bool solidBelow = zz == 0 || CellIsSolid(GRID(x, y, zz - 1));
        if (floorAround && !solidBelow) SET_FLOOR(x, y, zz);
        MarkChunkDirty(x, y, zz);
        InvalidatePathsThroughCell(x, y, zz);
    }
    if (z1 + 1 < gridDepth) MarkChunkDirty(x, y, z1 + 1);
    RebuildElevators();
    // Riders in that car were let off (z rounded); anyone left mid-air falls as usual
    return z1 - z0 + 1;
}

// --- Measuring ----------------------------------------------------------------------
static int CmpFloat(const void* a, const void* b) {
    float x = *(const float*)a, y = *(const float*)b;
    return (x > y) - (x < y);
}

static float Percentile(const float* sorted, int n, float p) {
    if (n <= 0) return 0.0f;
    int k = (int)(p * (float)(n - 1) + 0.5f);
    return sorted[k];
}

// Waits in line and rides: median, 90th percentile, longest, in seconds, game hours and
// cells of walking (how far a mover walks in that time); longest wait per floor
void ElevatorPrintWaitStats(void) {
    static float tmp[ELEV_SAMPLES];
    float hourSec = dayLength / 24.0f;                       // seconds per game hour
    float walkCellsPerSec = (balance.baseMoverSpeed > 0.0f ? balance.baseMoverSpeed : 200.0f)
                            * (60.0f / dayLength) / (float)CELL_SIZE;
    printf("Time scale: a game hour is %.1f s; a mover walks %.1f cells in it\n", hourSec, walkCellsPerSec * hourSec);
    printf("Lines: %d joined, %d boarded, %d left after re-planning (walked to the shaft for nothing)\n",
           lineJoins, waitSampleCount, lineLeaves);
    printf("  left: %d after a wait re-plan, %d other new path, %d no path/mid-climb, %d no car\n", leaveWhy[0], leaveWhy[1], leaveWhy[2], leaveWhy[3]);
    const char* names[2] = {"wait in line", "ride"};
    for (int which = 0; which < 2; which++) {
        int n = which ? rideSampleCount : waitSampleCount;
        if (n == 0) continue;
        memcpy(tmp, which ? rideSamples : waitSamples, sizeof(float) * (size_t)n);
        qsort(tmp, (size_t)n, sizeof(float), CmpFloat);
        float med = Percentile(tmp, n, 0.5f), p90 = Percentile(tmp, n, 0.9f), mx = tmp[n - 1];
        printf("%-13s n=%4d  median %5.1f s (%4.1f h, %4.0f cells)  p90 %5.1f s (%4.1f h)  longest %5.1f s (%4.1f h)\n",
               names[which], n, med, med / hourSec, med * walkCellsPerSec, p90, p90 / hourSec, mx, mx / hourSec);
    }
    if (rideSampleCount > 0) {
        double perFloor = 0.0; int floors = 0;
        for (int k = 0; k < rideSampleCount; k++) { perFloor += rideSamples[k]; floors += rideFloors[k]; }
        printf("ride time per floor ridden: %.2f s (car speed %.1f floors/s, doors %.1f s at each stop)\n",
               floors > 0 ? perFloor / floors : 0.0, elevatorSpeed * (60.0f / dayLength), elevatorDoorTime * (dayLength / 60.0f));
    }
    if (waitSampleCount > 0) {
        float worst[64] = {0}; int cnt[64] = {0};
        for (int k = 0; k < waitSampleCount; k++) {
            int f = waitFloor[k] < 64 ? waitFloor[k] : 63;
            cnt[f]++;
            if (waitSamples[k] > worst[f]) worst[f] = waitSamples[k];
        }
        printf("longest wait by floor:");
        for (int f = 0; f < 64; f++) if (cnt[f]) printf(" z%d %.0fs(%d)", f, worst[f], cnt[f]);
        printf("\n");
    }
}

int ElevatorMoverLinesLeft(int i) { return (i >= 0 && i < MAX_MOVERS) ? moverLineLeaves[i] : 0; }
