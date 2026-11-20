#include "trains.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/lighting.h"
#include "../core/time.h"
#include "../core/event_log.h"
#include <stdlib.h>
#include <math.h>

Train trains[MAX_TRAINS];
int trainCount = 0;

TrainStation stations[MAX_STATIONS];
int stationCount = 0;

bool trainQueueEnabled = true;
float trainSpeed = TRAIN_DEFAULT_SPEED;   // cells per second at dayLength 60 (console var trainSpeed)

// gameTime a mover last gave up waiting for a train (not saved: a fresh load just
// lets it try again); for TRAIN_GIVE_UP_COOLDOWN it walks instead
#define TRAIN_GIVE_UP_COOLDOWN 120.0

// Trip time estimates (runtime only, see "Trip time estimates" below)
#define TRAIN_WAIT_FALLBACK 10.0f   // seconds, until a station has measured waits
static float stationAvgWait[MAX_STATIONS];
static int stationWaitSamples[MAX_STATIONS];
static int stationBoardings[MAX_STATIONS];   // stats
// Train trips a mover started for its own goal (no job). The job system clears
// transport state of jobless movers as stale; these are not stale. Runtime only:
// after a load such a trip is dropped and the mover re-plans.
static bool personalTrip[MAX_MOVERS];
static bool trackDistReady = false;
int trainDecisions = 0, trainDecisionsFaster = 0;
double trainDecisionTrainSec = 0.0, trainDecisionWalkSec = 0.0;
static double moverGaveUpOnTrain[MAX_MOVERS];
static bool moverGaveUpInit = false;


void InitTrains(void) {
    ClearTrains();
}

void ClearTrains(void) {
    trackDistReady = false;   // runtime trip estimates start over
    trainDecisions = trainDecisionsFaster = 0;
    trainChoicesConsidered = 0;
    trainDecisionTrainSec = trainDecisionWalkSec = 0.0;
    moverGaveUpInit = false;   // forget who gave up (cooldowns)
    for (int i = 0; i < MAX_TRAINS; i++) {
        trains[i].active = false;
        trains[i].cartState = CART_MOVING;
        trains[i].stateTimer = 0.0f;
        trains[i].atStation = -1;
        trains[i].ridingCount = 0;
    }
    trainCount = 0;
    for (int i = 0; i < MAX_STATIONS; i++) {
        stations[i].active = false;
        stations[i].waitingCount = 0;
    }
    stationCount = 0;
}

int SpawnTrain(int x, int y, int z) {
    if (trainCount >= MAX_TRAINS) return -1;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return -1;
    if (GRID(x, y, z) != CELL_TRACK) return -1;

    int idx = -1;
    for (int i = 0; i < MAX_TRAINS; i++) {
        if (!trains[i].active) { idx = i; break; }
    }
    if (idx < 0) return -1;

    Train* t = &trains[idx];
    t->x = x * CELL_SIZE + CELL_SIZE * 0.5f;
    t->y = y * CELL_SIZE + CELL_SIZE * 0.5f;
    t->z = z;
    t->cellX = x;
    t->cellY = y;
    t->prevCellX = x;
    t->prevCellY = y;
    t->speed = TRAIN_DEFAULT_SPEED;
    t->progress = 1.0f;  // Start "arrived" so first tick picks a direction
    t->lightCellX = -1;
    t->lightCellY = -1;
    t->active = true;
    t->cartState = CART_MOVING;
    t->stateTimer = 0.0f;
    t->atStation = -1;
    t->ridingCount = 0;
    t->carCount = 1;
    t->trailCount = 0;
    trainCount++;
    return idx;
}

// Spawn heading (dx, dy): as if it had just come from (x - dx, y - dy). Several
// trains on one loop must run the same way round (they don't avoid each other).
int SpawnTrainHeading(int x, int y, int z, int cars, int dx, int dy) {
    int idx = SpawnTrainWithCars(x, y, z, cars);
    if (idx < 0) return -1;
    trains[idx].prevCellX = x - dx;
    trains[idx].prevCellY = y - dy;
    return idx;
}

int SpawnTrainWithCars(int x, int y, int z, int cars) {
    int idx = SpawnTrain(x, y, z);
    if (idx >= 0) {
        if (cars < 1) cars = 1;
        if (cars > MAX_TRAIL_LENGTH + 1) cars = MAX_TRAIL_LENGTH + 1;
        trains[idx].carCount = cars;
    }
    return idx;
}

// ============================================================================
// Station Detection
// ============================================================================

void RebuildStations(void) {
    trackDistReady = false;   // stations changed: distances and waits start over
    // Save old waiters before rebuilding
    // We'll try to preserve waiting movers at stations that still exist
    typedef struct { int trackX, trackY, z; int waitingMovers[MAX_STATION_WAITING]; float waitingSince[MAX_STATION_WAITING]; int waitingCount; } OldStation;
    OldStation oldStations[MAX_STATIONS];
    int oldCount = stationCount;
    for (int i = 0; i < oldCount; i++) {
        oldStations[i].trackX = stations[i].trackX;
        oldStations[i].trackY = stations[i].trackY;
        oldStations[i].z = stations[i].z;
        oldStations[i].waitingCount = stations[i].waitingCount;
        for (int j = 0; j < stations[i].waitingCount; j++) {
            oldStations[i].waitingMovers[j] = stations[i].waitingMovers[j];
            oldStations[i].waitingSince[j] = stations[i].waitingSince[j];
        }
    }

    // Clear and rebuild
    stationCount = 0;
    for (int i = 0; i < MAX_STATIONS; i++) {
        stations[i].active = false;
        stations[i].waitingCount = 0;
    }

    static const int dx[] = { 0, 1, 0, -1 };
    static const int dy[] = { -1, 0, 1, 0 };

    for (int z = 0; z < gridDepth && stationCount < MAX_STATIONS; z++) {
        for (int y = 0; y < gridHeight && stationCount < MAX_STATIONS; y++) {
            for (int x = 0; x < gridWidth && stationCount < MAX_STATIONS; x++) {
                if (GRID(x, y, z) != CELL_TRACK) continue;
                // Check 4 cardinal neighbors for platform
                for (int d = 0; d < 4; d++) {
                    int px = x + dx[d];
                    int py = y + dy[d];
                    if (px < 0 || px >= gridWidth || py < 0 || py >= gridHeight) continue;
                    if (GRID(px, py, z) != CELL_PLATFORM) continue;
                    // Check if this platform cell is already claimed by another station
                    bool alreadyClaimed = false;
                    for (int si = 0; si < stationCount && !alreadyClaimed; si++) {
                        if (stations[si].z != z) continue;
                        for (int pc = 0; pc < stations[si].platformCellCount; pc++) {
                            if (stations[si].platformCells[pc][0] == px && stations[si].platformCells[pc][1] == py) {
                                alreadyClaimed = true;
                                break;
                            }
                        }
                    }
                    if (alreadyClaimed) continue;
                    // Found a station: track at (x,y), platform at (px,py)
                    TrainStation* s = &stations[stationCount];
                    s->trackX = x;
                    s->trackY = y;
                    s->z = z;
                    s->platX = px;
                    s->platY = py;
                    s->active = true;
                    s->waitingCount = 0;

                    // Multi-cell platform: walk parallel to track collecting contiguous platforms
                    // Away direction (track → platform, perpendicular to track)
                    int awayX = px - x;
                    int awayY = py - y;
                    // Track runs perpendicular to away direction
                    int trackDirX = -awayY;
                    int trackDirY = awayX;
                    // Queue extends along the platform (parallel to track)
                    s->queueDirX = trackDirX;
                    s->queueDirY = trackDirY;

                    // Start with the boarding cell (adjacent to this track cell)
                    s->platformCells[0][0] = px;
                    s->platformCells[0][1] = py;
                    s->platformCellCount = 1;

                    // Walk in +trackDir
                    int cx = px + trackDirX, cy = py + trackDirY;
                    while (s->platformCellCount < MAX_PLATFORM_CELLS) {
                        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) break;
                        if (GRID(cx, cy, z) != CELL_PLATFORM) break;
                        s->platformCells[s->platformCellCount][0] = cx;
                        s->platformCells[s->platformCellCount][1] = cy;
                        s->platformCellCount++;
                        cx += trackDirX;
                        cy += trackDirY;
                    }
                    // Walk in -trackDir
                    cx = px - trackDirX;
                    cy = py - trackDirY;
                    while (s->platformCellCount < MAX_PLATFORM_CELLS) {
                        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) break;
                        if (GRID(cx, cy, z) != CELL_PLATFORM) break;
                        s->platformCells[s->platformCellCount][0] = cx;
                        s->platformCells[s->platformCellCount][1] = cy;
                        s->platformCellCount++;
                        cx -= trackDirX;
                        cy -= trackDirY;
                    }

                    // Restore waiters from old station at same location
                    for (int oi = 0; oi < oldCount; oi++) {
                        if (oldStations[oi].trackX == x && oldStations[oi].trackY == y && oldStations[oi].z == z) {
                            s->waitingCount = oldStations[oi].waitingCount;
                            for (int j = 0; j < s->waitingCount; j++) {
                                s->waitingMovers[j] = oldStations[oi].waitingMovers[j];
                                s->waitingSince[j] = oldStations[oi].waitingSince[j];
                            }
                            break;
                        }
                    }

                    stationCount++;
                    break;  // One station per track cell (use first platform found)
                }
            }
        }
    }

    // Eject waiters from stations that no longer exist
    for (int oi = 0; oi < oldCount; oi++) {
        bool stillExists = false;
        for (int ni = 0; ni < stationCount; ni++) {
            if (stations[ni].trackX == oldStations[oi].trackX &&
                stations[ni].trackY == oldStations[oi].trackY &&
                stations[ni].z == oldStations[oi].z) {
                stillExists = true;
                break;
            }
        }
        if (!stillExists) {
            // Reset transport state for any movers that were waiting here
            for (int j = 0; j < oldStations[oi].waitingCount; j++) {
                int mi = oldStations[oi].waitingMovers[j];
                if (mi >= 0 && mi < moverCount && movers[mi].active) {
                    movers[mi].transportState = TRANSPORT_NONE;
                    movers[mi].transportStation = -1;
                    movers[mi].transportExitStation = -1;
                    movers[mi].transportTrainIdx = -1;
                    // Restore original goal
                    movers[mi].goal = movers[mi].transportFinalGoal;
                    movers[mi].needsRepath = true;
                }
            }
        }
    }
}

int GetStationAt(int x, int y, int z) {
    for (int i = 0; i < stationCount; i++) {
        TrainStation* s = &stations[i];
        if (!s->active || s->z != z) continue;
        // Check if (x,y) is the track cell adjacent to any platform cell
        // awayDir: from track to platform, so track = platform - awayDir
        int awayX = s->platX - s->trackX;
        int awayY = s->platY - s->trackY;
        for (int p = 0; p < s->platformCellCount; p++) {
            int tx = s->platformCells[p][0] - awayX;
            int ty = s->platformCells[p][1] - awayY;
            if (tx == x && ty == y) return i;
        }
    }
    return -1;
}

int FindNearestStation(int x, int y, int z, int maxRadius) {
    int best = -1;
    int bestDist = maxRadius + 1;
    for (int i = 0; i < stationCount; i++) {
        TrainStation* s = &stations[i];
        if (!s->active || s->z != z) continue;
        // Check distance to nearest platform cell
        for (int p = 0; p < s->platformCellCount; p++) {
            int dist = abs(s->platformCells[p][0] - x) + abs(s->platformCells[p][1] - y);
            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }
    }
    return best;
}

// Find the nearest platform cell of a station to a given position
void GetNearestPlatformCell(int stationIdx, int x, int y, int* outX, int* outY) {
    TrainStation* s = &stations[stationIdx];
    int bestDist = 999999;
    *outX = s->platX;
    *outY = s->platY;
    for (int p = 0; p < s->platformCellCount; p++) {
        int dist = abs(s->platformCells[p][0] - x) + abs(s->platformCells[p][1] - y);
        if (dist < bestDist) {
            bestDist = dist;
            *outX = s->platformCells[p][0];
            *outY = s->platformCells[p][1];
        }
    }
}

// ============================================================================
// WaitingSet Operations
// ============================================================================

void StationAddWaiter(int stationIdx, int moverIdx) {
    if (stationIdx < 0 || stationIdx >= stationCount) return;
    TrainStation* s = &stations[stationIdx];
    if (!s->active || s->waitingCount >= MAX_STATION_WAITING) return;
    // Check not already waiting
    for (int i = 0; i < s->waitingCount; i++) {
        if (s->waitingMovers[i] == moverIdx) return;
    }
    s->waitingMovers[s->waitingCount] = moverIdx;
    s->waitingSince[s->waitingCount] = (float)gameTime;
    s->waitingCount++;
}

void StationRemoveWaiter(int stationIdx, int moverIdx) {
    if (stationIdx < 0 || stationIdx >= stationCount) return;
    TrainStation* s = &stations[stationIdx];
    for (int i = 0; i < s->waitingCount; i++) {
        if (s->waitingMovers[i] == moverIdx) {
            // Shift-down to preserve FIFO order
            for (int j = i; j < s->waitingCount - 1; j++) {
                s->waitingMovers[j] = s->waitingMovers[j + 1];
                s->waitingSince[j] = s->waitingSince[j + 1];
            }
            s->waitingCount--;
            return;
        }
    }
}

int StationGetNextBoarder(int stationIdx) {
    if (stationIdx < 0 || stationIdx >= stationCount) return -1;
    TrainStation* s = &stations[stationIdx];
    if (s->waitingCount == 0) return -1;
    // FIFO: array[0] is always the front of the queue
    return s->waitingMovers[0];
}

void StationGetQueuePosition(int stationIdx, int slotIndex, float* outX, float* outY) {
    TrainStation* s = &stations[stationIdx];
    float spacing = CELL_SIZE * 0.6f;
    float startX = s->platformCells[0][0] * CELL_SIZE + CELL_SIZE * 0.5f;
    float startY = s->platformCells[0][1] * CELL_SIZE + CELL_SIZE * 0.5f;
    float offset = slotIndex * spacing;
    *outX = startX + s->queueDirX * offset;
    *outY = startY + s->queueDirY * offset;
}

// ============================================================================
// Trip time estimates (train vs walking) and their measurement
// ============================================================================
//
// Runtime only (not saved): per-station waits are measured at boarding; track
// distances between stations are cached until stations are rebuilt.


static void ResetTripEstimates(void) {
    memset(stationWaitSamples, 0, sizeof(stationWaitSamples));
    memset(stationBoardings, 0, sizeof(stationBoardings));
    trackDistReady = true;
}

static float CellsBetween(int x0, int y0, int x1, int y1) {   // octile distance
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int mn = dx < dy ? dx : dy, mx = dx > dy ? dx : dy;
    return (float)(mx - mn) + 1.414f * (float)mn;
}

// One step along the track the way a train goes (FindNextTrackCell's rules,
// deterministic: straight ahead where possible, else the first open way; reverse
// only at a dead end). false = isolated cell.
static bool FollowTrack(int z, int x, int y, int px, int py, int* nx, int* ny) {
    static const int dx[] = { 0, 1, 0, -1 }, dy[] = { -1, 0, 1, 0 };
    static const uint8_t dirBit[] = { TRACK_N, TRACK_E, TRACK_S, TRACK_W };
    uint8_t conn = trackConnections[z][y][x];
    int sx = x + (x - px), sy = y + (y - py);
    int firstX = -1, firstY = -1;
    for (int d = 0; d < 4; d++) {
        if (!(conn & dirBit[d])) continue;
        int cx = x + dx[d], cy = y + dy[d];
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || GRID(cx, cy, z) != CELL_TRACK) continue;
        if (cx == px && cy == py) continue;
        if (cx == sx && cy == sy) { *nx = cx; *ny = cy; return true; }
        if (firstX < 0) { firstX = cx; firstY = cy; }
    }
    if (firstX >= 0) { *nx = firstX; *ny = firstY; return true; }
    if (px != x || py != y) { *nx = px; *ny = py; return true; }   // dead end
    return false;
}

// Follow a train's route from its current state and note, in cells travelled,
// when it next reaches station `entry` (after `notBefore` cells) and then station
// `exit`. Also counts stations passed between them. false if it doesn't (within a
// bound of twice the track length).
static bool TrainRouteCells(const Train* t, int entry, int exit, int notBefore,
                            int* toEntry, int* entryToExit, int* stopsBetween) {
    int x = t->cellX, y = t->cellY, px = t->prevCellX, py = t->prevCellY;
    int limit = 4 * (gridWidth + gridHeight) * 8;   // generous: loops are bounded by the map
    int cells = 0, phase = 0, entryAt = -1, stops = 0, lastStation = -1;
    for (int step = 0; step < limit; step++) {
        int st = GetStationAt(x, y, t->z);
        if (st != lastStation) {
            if (phase == 0 && st == entry && cells >= notBefore) { phase = 1; entryAt = cells; stops = 0; }
            else if (phase == 1 && st == exit) {
                *toEntry = entryAt;
                *entryToExit = cells - entryAt;
                *stopsBetween = stops;
                return true;
            } else if (phase == 1 && st >= 0) stops++;
            lastStation = st;
        }
        int nx, ny;
        if (!FollowTrack(t->z, x, y, px, py, &nx, &ny)) return false;
        px = x; py = y; x = nx; y = ny;
        cells++;
    }
    return false;
}

// Seconds to get to `goal` by train via `entry` and `exit`: walk to the entry
// platform; wait for the first train (in the direction it really runs) that gets
// there after the mover does; doors; ride in that direction (stations on the way
// cost half a door stop each, they may or may not stop); doors; walk on.
// A long line at the platform skips trains that will arrive full. 1e9 = no train.
float TrainTripSeconds(int moverIdx, int entry, int exit, Point goal) {
    const Mover* m = &movers[moverIdx];
    float pace = MoverWalkSecondsPerCell(moverIdx);
    float cellsPerSec = trainSpeed * (60.0f / dayLength);
    if (cellsPerSec <= 0.0f) return 1e9f;
    int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE);
    int ex, ey, ox, oy;
    GetNearestPlatformCell(entry, mx, my, &ex, &ey);
    GetNearestPlatformCell(exit, goal.x, goal.y, &ox, &oy);
    float walkIn = CellsBetween(mx, my, ex, ey) * pace;
    float walkOut = CellsBetween(ox, oy, goal.x, goal.y) * pace;
    int skipFull = stations[entry].waitingCount / MAX_CART_CAPACITY;   // trains that fill up before us
    float best = 1e9f;
    for (int ti = 0; ti < MAX_TRAINS; ti++) {
        const Train* t = &trains[ti];
        if (!t->active || t->z != stations[entry].z) continue;
        float delay = (t->cartState == CART_DOORS_OPEN) ? t->stateTimer : 0.0f;
        int notBefore = 0, toEntry, ride, stops;
        // the first arrival after the mover reaches the platform (and after full trains)
        for (int tries = 0; tries < 4; tries++) {
            if (!TrainRouteCells(t, entry, exit, notBefore, &toEntry, &ride, &stops)) break;
            float arrive = delay + toEntry / cellsPerSec;
            if (arrive < walkIn || skipFull > 0) {
                if (arrive >= walkIn) skipFull--;
                notBefore = toEntry + 1;
                continue;
            }
            float total = arrive + TRAIN_DOOR_TIME + ride / cellsPerSec + stops * 0.5f * TRAIN_DOOR_TIME
                        + TRAIN_DOOR_TIME + walkOut;
            if (total < best) best = total;
            break;
        }
    }
    return best;
}

// Seconds to walk the mover's current path (cells along its waypoints)
float WalkTripSeconds(int moverIdx) {
    const Mover* m = &movers[moverIdx];
    float cells = 0.0f;
    int px = (int)(m->x / CELL_SIZE), py = (int)(m->y / CELL_SIZE);
    for (int k = m->pathIndex; k >= 0; k--) {
        Point w = moverPaths[moverIdx][k];
        cells += CellsBetween(px, py, w.x, w.y);
        px = w.x; py = w.y;
    }
    if (m->pathIndex < 0) cells = CellsBetween(px, py, m->goal.x, m->goal.y);
    else cells += MoverRouteCellsLeft(moverIdx);   // coarse path: the route beyond the window
    return cells * MoverWalkSecondsPerCell(moverIdx);
}

// Best (entry, exit) pair among the stations within TRANSPORT_STATION_RADIUS of the
// mover and of its goal, by trip time. Returns its seconds (1e9 = none).
static float BestTrainTrip(int moverIdx, int* outEntry, int* outExit) {
    const Mover* m = &movers[moverIdx];
    int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE), mz = (int)m->z;
    float best = 1e9f;
    *outEntry = *outExit = -1;
    for (int a = 0; a < stationCount; a++) {
        if (!stations[a].active || stations[a].z != mz) continue;
        int ax, ay;
        GetNearestPlatformCell(a, mx, my, &ax, &ay);
        if (abs(ax - mx) + abs(ay - my) > TRANSPORT_STATION_RADIUS) continue;
        for (int b = 0; b < stationCount; b++) {
            if (b == a || !stations[b].active || stations[b].z != mz) continue;
            int bx, by;
            GetNearestPlatformCell(b, m->goal.x, m->goal.y, &bx, &by);
            if (abs(bx - m->goal.x) + abs(by - m->goal.y) > TRANSPORT_STATION_RADIUS) continue;
            float sec = TrainTripSeconds(moverIdx, a, b, m->goal);
            if (sec < best) { best = sec; *outEntry = a; *outExit = b; }
        }
    }
    return best;
}

void TrainsMarkPersonalTrip(int moverIdx, bool personal) {
    if (moverIdx >= 0 && moverIdx < MAX_MOVERS) personalTrip[moverIdx] = personal;
}

bool TrainsIsPersonalTrip(int moverIdx) {
    return moverIdx >= 0 && moverIdx < MAX_MOVERS && personalTrip[moverIdx] &&
           movers[moverIdx].transportState != TRANSPORT_NONE;
}

int StationBoardings(int stationIdx) {
    return (trackDistReady && stationIdx >= 0 && stationIdx < MAX_STATIONS) ? stationBoardings[stationIdx] : 0;
}

// Stats: a trip the decision sent by train (headless output)
void TrainNoteDecision(int moverIdx, int entry, int exit) {
    float train = TrainTripSeconds(moverIdx, entry, exit, movers[moverIdx].goal);
    float walk = WalkTripSeconds(moverIdx);
    trainDecisions++;
    if (train < walk) trainDecisionsFaster++;
    trainDecisionTrainSec += train;
    trainDecisionWalkSec += walk;
}

// ============================================================================
// Transport Heuristic
// ============================================================================

void TrainsNoteGaveUp(int moverIdx) {
    if (!moverGaveUpInit) { for (int i = 0; i < MAX_MOVERS; i++) moverGaveUpOnTrain[i] = -1e9; moverGaveUpInit = true; }
    if (moverIdx >= 0 && moverIdx < MAX_MOVERS) moverGaveUpOnTrain[moverIdx] = gameTime;
}

// Trip chosen by the last ShouldUseTrain that returned true (time-based choice)
static int chosenEntry = -1, chosenExit = -1;
bool useTrainTimeChoice = true;
int trainChoicesConsidered = 0;

void TrainChosenStations(int* entry, int* exit) { *entry = chosenEntry; *exit = chosenExit; }

bool ShouldUseTrain(int moverIdx) {
    chosenEntry = chosenExit = -1;
    if (stationCount < 2) return false;
    Mover* m = &movers[moverIdx];
    if (m->transportState != TRANSPORT_NONE) return false;
    if (moverGaveUpInit && gameTime - moverGaveUpOnTrain[moverIdx] < TRAIN_GIVE_UP_COOLDOWN) return false;

    // Don't use train for pickup walks — only for carrying/delivery legs.
    // During pickup (step 0), the goal is the item location. After picking up,
    // the mover will repath to the delivery destination, and THAT is when
    // transport should be considered.
    if (m->currentJobId >= 0) {
        Job* j = GetJob(m->currentJobId);
        if (j && j->step == STEP_MOVING_TO_PICKUP) return false;
    }

    int mx = (int)(m->x / CELL_SIZE);
    int my = (int)(m->y / CELL_SIZE);
    int mz = (int)m->z;

    // Must be same z-level
    if (m->goal.z != mz) return false;

    // Time-based: take the train when the best (entry, exit) pair, in the
    // direction the trains really run, beats walking the current path by 10%
    if (useTrainTimeChoice) {
        int a, b;
        float train = BestTrainTrip(moverIdx, &a, &b);
        if (a < 0) return false;
        trainChoicesConsidered++;
        if (train >= 0.9f * WalkTripSeconds(moverIdx)) return false;
        chosenEntry = a;
        chosenExit = b;
        return true;
    }

    // Old rule: goal far enough, a station near both ends
    int dist = abs(m->goal.x - mx) + abs(m->goal.y - my);
    if (dist < TRANSPORT_FAR_THRESHOLD) return false;

    // Find station near mover
    int entryStation = FindNearestStation(mx, my, mz, TRANSPORT_STATION_RADIUS);
    if (entryStation < 0) return false;

    // Find station near goal
    int exitStation = FindNearestStation(m->goal.x, m->goal.y, mz, TRANSPORT_STATION_RADIUS);
    if (exitStation < 0 || exitStation == entryStation) return false;

    chosenEntry = entryStation;
    chosenExit = exitStation;
    return true;
}

// ============================================================================
// Board / Exit
// ============================================================================

void BoardMoverOnTrain(int trainIdx, int moverIdx, int stationIdx) {
    Train* t = &trains[trainIdx];
    Mover* m = &movers[moverIdx];

    // Measure how long boarders waited here (moving average)
    if (stationIdx >= 0 && stationIdx < stationCount) {
        const TrainStation* st = &stations[stationIdx];
        for (int w = 0; w < st->waitingCount; w++) {
            if (st->waitingMovers[w] != moverIdx) continue;
            float waited = (float)gameTime - st->waitingSince[w];
            if (!trackDistReady) ResetTripEstimates();
            stationAvgWait[stationIdx] = stationWaitSamples[stationIdx] == 0 ? waited
                                       : stationAvgWait[stationIdx] * 0.8f + waited * 0.2f;
            stationWaitSamples[stationIdx]++;
            stationBoardings[stationIdx]++;
            break;
        }
    }

    StationRemoveWaiter(stationIdx, moverIdx);

    t->ridingMovers[t->ridingCount++] = moverIdx;
    m->transportState = TRANSPORT_RIDING;
    m->transportTrainIdx = trainIdx;
    // Clear path — mover is now riding
    m->pathLength = 0;
    m->pathIndex = -1;
    m->needsRepath = false;

    EventLog("Mover %d boarded train %d at station %d", moverIdx, trainIdx, stationIdx);
}

void ExitMoverFromTrain(int trainIdx, int riderSlot, int stationIdx) {
    Train* t = &trains[trainIdx];
    int moverIdx = t->ridingMovers[riderSlot];
    Mover* m = &movers[moverIdx];

    // Swap-last remove from riding list
    t->ridingMovers[riderSlot] = t->ridingMovers[t->ridingCount - 1];
    t->ridingCount--;

    // Place mover on a spread-out platform cell (cycle through available cells)
    TrainStation* s = &stations[stationIdx];
    static int exitSpreadIdx = 0;
    int cellSlot = exitSpreadIdx % (s->platformCellCount > 0 ? s->platformCellCount : 1);
    int exitX = s->platX, exitY = s->platY;
    if (s->platformCellCount > 0) {
        exitX = s->platformCells[cellSlot][0];
        exitY = s->platformCells[cellSlot][1];
    }
    exitSpreadIdx++;
    m->x = exitX * CELL_SIZE + CELL_SIZE * 0.5f;
    m->y = exitY * CELL_SIZE + CELL_SIZE * 0.5f;
    m->z = (float)s->z;
    m->lastX = m->x;
    m->lastY = m->y;
    m->lastZ = m->z;

    // Restore original goal
    m->goal = m->transportFinalGoal;
    m->transportState = TRANSPORT_NONE;
    m->transportStation = -1;
    m->transportExitStation = -1;
    m->transportTrainIdx = -1;
    m->needsRepath = true;
    m->timeWithoutProgress = 0.0f;

    EventLog("Mover %d exited train %d at station %d", moverIdx, trainIdx, stationIdx);
}

void DismountAllRiders(int trainIdx) {
    Train* t = &trains[trainIdx];
    while (t->ridingCount > 0) {
        int moverIdx = t->ridingMovers[0];
        Mover* m = &movers[moverIdx];

        // Place at train's current cell (best effort)
        m->x = t->x;
        m->y = t->y;
        m->z = (float)t->z;
        m->lastX = m->x;
        m->lastY = m->y;
        m->lastZ = m->z;

        EventLog("Mover %d dismounted from train %d (DismountAll)", moverIdx, trainIdx);
        m->goal = m->transportFinalGoal;
        m->transportState = TRANSPORT_NONE;
        m->transportStation = -1;
        m->transportExitStation = -1;
        m->transportTrainIdx = -1;
        m->needsRepath = true;
        m->timeWithoutProgress = 0.0f;

        // Swap-last remove
        t->ridingMovers[0] = t->ridingMovers[t->ridingCount - 1];
        t->ridingCount--;
    }
}

// ============================================================================
// Track Cell Navigation
// ============================================================================

// Find the next track cell to move to (uses trackConnections grid)
// Returns true if a next cell was found, writes to outX/outY
static bool FindNextTrackCell(Train* t, int* outX, int* outY) {
    static const int dx[] = { 0, 1, 0, -1 };  // N, E, S, W
    static const int dy[] = { -1, 0, 1, 0 };
    static const uint8_t dirBit[] = { TRACK_N, TRACK_E, TRACK_S, TRACK_W };

    uint8_t conn = trackConnections[t->z][t->cellY][t->cellX];
    int options[4][2];
    int optionCount = 0;

    for (int d = 0; d < 4; d++) {
        if (!(conn & dirBit[d])) continue;  // Only follow connected directions
        int nx = t->cellX + dx[d];
        int ny = t->cellY + dy[d];
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
        if (GRID(nx, ny, t->z) != CELL_TRACK) continue;
        // Don't reverse into previous cell (unless it's the only option)
        if (nx == t->prevCellX && ny == t->prevCellY) continue;
        options[optionCount][0] = nx;
        options[optionCount][1] = ny;
        optionCount++;
    }

    if (optionCount > 0) {
        if (optionCount == 1) {
            *outX = options[0][0];
            *outY = options[0][1];
        } else {
            // Prefer going straight (same direction as current heading)
            int headingX = t->cellX - t->prevCellX;
            int headingY = t->cellY - t->prevCellY;
            int straightX = t->cellX + headingX;
            int straightY = t->cellY + headingY;

            // Check if straight-ahead is one of the options
            int straightIdx = -1;
            for (int i = 0; i < optionCount; i++) {
                if (options[i][0] == straightX && options[i][1] == straightY) {
                    straightIdx = i;
                    break;
                }
            }

            if (straightIdx >= 0 && (rand() % 100) >= 10) {
                // Go straight (90% chance; 10% chance to pick randomly instead)
                *outX = options[straightIdx][0];
                *outY = options[straightIdx][1];
            } else {
                // No straight option (T-junction) — pick random among remaining
                int pick = rand() % optionCount;
                *outX = options[pick][0];
                *outY = options[pick][1];
            }
        }
        return true;
    }

    // Dead end: reverse
    if (t->prevCellX != t->cellX || t->prevCellY != t->cellY) {
        *outX = t->prevCellX;
        *outY = t->prevCellY;
        return true;
    }

    return false;  // Isolated single track cell
}

// ============================================================================
// Train Tick
// ============================================================================

void TrainsTick(float dt) {
    float gdt = dt * gameSpeed;
    if (gdt <= 0.0f) return;

    for (int i = 0; i < MAX_TRAINS; i++) {
        Train* t = &trains[i];
        if (!t->active) continue;

        // Check track still exists under us
        if (GRID(t->cellX, t->cellY, t->z) != CELL_TRACK) {
            if (t->lightCellX >= 0) RemoveLightSource(t->lightCellX, t->lightCellY, t->z);
            DismountAllRiders(i);
            t->active = false;
            trainCount--;
            continue;
        }

        // Handle doors-open state (stopped at station)
        if (t->cartState == CART_DOORS_OPEN) {
            // 1. Exit riders whose exit station is this station
            for (int r = t->ridingCount - 1; r >= 0; r--) {
                int mi = t->ridingMovers[r];
                if (mi >= 0 && mi < moverCount && movers[mi].active) {
                    if (movers[mi].transportExitStation == t->atStation) {
                        ExitMoverFromTrain(i, r, t->atStation);
                    }
                }
            }

            // 2. Board waiters (FIFO) while capacity available
            while (t->ridingCount < MAX_CART_CAPACITY) {
                int boarder = StationGetNextBoarder(t->atStation);
                if (boarder < 0) break;
                // Only board if this mover's entry station matches
                if (movers[boarder].transportStation == t->atStation) {
                    BoardMoverOnTrain(i, boarder, t->atStation);
                } else {
                    break;  // Don't board movers waiting for other stations
                }
            }

            t->stateTimer -= gdt;
            if (t->stateTimer <= 0.0f) {
                t->cartState = CART_MOVING;
                t->atStation = -1;
            }
            // Update riding mover positions even while stopped
            for (int r = 0; r < t->ridingCount; r++) {
                int mi = t->ridingMovers[r];
                if (mi >= 0 && mi < moverCount && movers[mi].active) {
                    movers[mi].x = t->x;
                    movers[mi].y = t->y;
                    movers[mi].timeWithoutProgress = 0.0f;
                }
            }
            continue;
        }

        // CART_MOVING state
        t->progress += trainSpeed * (60.0f / dayLength) * gdt;

        while (t->progress >= 1.0f) {
            t->progress -= 1.0f;

            // Arrived at cellX/cellY — check for station
            int stIdx = GetStationAt(t->cellX, t->cellY, t->z);
            if (stIdx >= 0) {
                // Check if anyone wants to board or exit here
                bool hasWaiters = stations[stIdx].waitingCount > 0 && t->ridingCount < MAX_CART_CAPACITY;
                bool hasExiters = false;
                for (int r = 0; r < t->ridingCount && !hasExiters; r++) {
                    int mi = t->ridingMovers[r];
                    if (mi >= 0 && mi < moverCount && movers[mi].active) {
                        if (movers[mi].transportExitStation == stIdx) hasExiters = true;
                    }
                }
                if (hasWaiters || hasExiters) {
                    // Pull forward: only stop if the next track cell is NOT part of this station
                    int peekX, peekY;
                    bool hasNext = FindNextTrackCell(t, &peekX, &peekY);
                    bool nextIsStation = hasNext && GetStationAt(peekX, peekY, t->z) == stIdx;
                    if (!nextIsStation) {
                        t->cartState = CART_DOORS_OPEN;
                        t->stateTimer = TRAIN_DOOR_TIME;
                        t->atStation = stIdx;
                        t->progress = 0.0f;
                        break;
                    }
                }
            }

            // Pick next cell
            int nextX, nextY;
            if (FindNextTrackCell(t, &nextX, &nextY)) {
                // Push current prevCell onto trail for trailing cars
                int maxTrail = t->carCount - 1;
                if (maxTrail > MAX_TRAIL_LENGTH) maxTrail = MAX_TRAIL_LENGTH;
                if (maxTrail > 0) {
                    // Shift trail down
                    for (int k = MAX_TRAIL_LENGTH - 1; k > 0; k--) {
                        t->trailCellX[k] = t->trailCellX[k - 1];
                        t->trailCellY[k] = t->trailCellY[k - 1];
                    }
                    t->trailCellX[0] = t->prevCellX;
                    t->trailCellY[0] = t->prevCellY;
                    if (t->trailCount < maxTrail) t->trailCount++;
                }
                t->prevCellX = t->cellX;
                t->prevCellY = t->cellY;
                t->cellX = nextX;
                t->cellY = nextY;
            } else {
                // Stuck on isolated cell
                t->progress = 0.0f;
                break;
            }
        }

        // Interpolate pixel position between prev cell and current cell
        float fromX = t->prevCellX * CELL_SIZE + CELL_SIZE * 0.5f;
        float fromY = t->prevCellY * CELL_SIZE + CELL_SIZE * 0.5f;
        float toX = t->cellX * CELL_SIZE + CELL_SIZE * 0.5f;
        float toY = t->cellY * CELL_SIZE + CELL_SIZE * 0.5f;
        t->x = fromX + (toX - fromX) * t->progress;
        t->y = fromY + (toY - fromY) * t->progress;

        // Push movers out of the way if train is moving
        if (t->cartState == CART_MOVING) {
            float pushRadius = CELL_SIZE * 0.8f;
            float pushRadiusSq = pushRadius * pushRadius;
            float pushStrength = CELL_SIZE * 4.0f;  // Strong shove

            for (int mi = 0; mi < moverCount; mi++) {
                Mover* m = &movers[mi];
                if (!m->active || (int)m->z != t->z) continue;
                // Don't push riders or movers waiting to board
                if (m->transportState == TRANSPORT_RIDING || m->transportState == TRANSPORT_WAITING) continue;

                float dx = m->x - t->x;
                float dy = m->y - t->y;
                float distSq = dx * dx + dy * dy;

                if (distSq < 1e-10f || distSq >= pushRadiusSq) continue;

                float dist = sqrtf(distSq);
                float push = pushStrength * (1.0f - dist / pushRadius) * gdt;
                float nx = dx / dist;
                float ny = dy / dist;
                m->x += nx * push;
                m->y += ny * push;
            }
        }

        // Update riding mover positions
        for (int r = 0; r < t->ridingCount; r++) {
            int mi = t->ridingMovers[r];
            if (mi >= 0 && mi < moverCount && movers[mi].active) {
                movers[mi].x = t->x;
                movers[mi].y = t->y;
                movers[mi].timeWithoutProgress = 0.0f;
            }
        }

        // Update locomotive light at current cell
        int curCellX = (int)(t->x / CELL_SIZE);
        int curCellY = (int)(t->y / CELL_SIZE);
        if (curCellX != t->lightCellX || curCellY != t->lightCellY) {
            if (t->lightCellX >= 0) RemoveLightSource(t->lightCellX, t->lightCellY, t->z);
            AddLightSource(curCellX, curCellY, t->z, 200, 180, 120, 8);
            t->lightCellX = curCellX;
            t->lightCellY = curCellY;
        }
    }
}
