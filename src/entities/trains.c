#include "trains.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/lighting.h"
#include "../core/time.h"
#include "../core/event_log.h"
#include "../world/pathfinding.h"
#include "stockpiles.h"
#include "items.h"
#include "jobs.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

Train trains[MAX_TRAINS];
static int transitGen = 0;   // bumped when stations/lines change: the transit graph rebuilds on next use
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

// ============================================================================
// Lines: each connected piece of track is one line with its own colour.
// Derived from the grid (never saved), rebuilt with the stations. Trains
// follow track, so a train only ever serves the stations of its own line.
// ============================================================================

typedef struct { int key; int line; } TrackLineCell;   // key = (z*H + y)*W + x
static TrackLineCell* trackLineCells = NULL;
static int trackLineCellCount = 0, trackLineCellCap = 0;
static int trackLineCount = 0;

static const char* trackLineNames[TRACK_LINE_COLORS] = {
    "Red", "Blue", "Green", "Yellow", "Purple", "Orange", "Cyan", "Pink",
};

static int TrackLineIndex(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return -1;
    int key = (z * gridHeight + y) * gridWidth + x;
    int lo = 0, hi = trackLineCellCount - 1;   // cells are added in key order
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (trackLineCells[mid].key == key) return mid;
        if (trackLineCells[mid].key < key) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

void RebuildTrackLines(void) {
    trackLineCellCount = 0;
    trackLineCount = 0;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                if (GRID(x, y, z) != CELL_TRACK) continue;
                if (trackLineCellCount == trackLineCellCap) {
                    int cap = trackLineCellCap ? trackLineCellCap * 2 : 1024;
                    TrackLineCell* grown = realloc(trackLineCells, cap * sizeof(TrackLineCell));
                    if (!grown) return;
                    trackLineCells = grown;
                    trackLineCellCap = cap;
                }
                trackLineCells[trackLineCellCount++] = (TrackLineCell){ (z * gridHeight + y) * gridWidth + x, -1 };
            }
    if (trackLineCellCount == 0) return;
    // Flood fill along the connections each cell has (the ones trains follow)
    static const int dx[] = { 0, 1, 0, -1 }, dy[] = { -1, 0, 1, 0 };
    static const uint8_t dirBit[] = { TRACK_N, TRACK_E, TRACK_S, TRACK_W };
    int* stack = malloc(trackLineCellCount * sizeof(int));
    if (!stack) return;
    for (int i = 0; i < trackLineCellCount; i++) {
        if (trackLineCells[i].line >= 0) continue;
        int line = trackLineCount++;
        int top = 0;
        stack[top++] = i;
        trackLineCells[i].line = line;
        while (top > 0) {
            int c = stack[--top];
            int key = trackLineCells[c].key;
            int x = key % gridWidth, y = (key / gridWidth) % gridHeight, z = key / (gridWidth * gridHeight);
            uint8_t conn = TRACK_CONN(x, y, z);
            for (int d = 0; d < 4; d++) {
                if (!(conn & dirBit[d])) continue;
                int n = TrackLineIndex(x + dx[d], y + dy[d], z);
                if (n < 0 || trackLineCells[n].line >= 0) continue;
                trackLineCells[n].line = line;
                stack[top++] = n;
            }
        }
    }
    free(stack);
}

int TrackLineCount(void) { return trackLineCount; }

int TrackLineAt(int x, int y, int z) {
    int i = TrackLineIndex(x, y, z);
    return i >= 0 ? trackLineCells[i].line : -1;
}

int StationLine(int stationIdx) {
    if (stationIdx < 0 || stationIdx >= stationCount || !stations[stationIdx].active) return -1;
    const TrainStation* s = &stations[stationIdx];
    return TrackLineAt(s->trackX, s->trackY, s->z);
}

int TrainLine(int trainIdx) {
    if (trainIdx < 0 || trainIdx >= MAX_TRAINS || !trains[trainIdx].active) return -1;
    const Train* t = &trains[trainIdx];
    return TrackLineAt(t->cellX, t->cellY, t->z);
}

const char* TrackLineName(int line) {
    return line < 0 ? "No line" : trackLineNames[line % TRACK_LINE_COLORS];
}

void TrackLineRGB(int line, unsigned char* r, unsigned char* g, unsigned char* b) {
    static const unsigned char rgb[TRACK_LINE_COLORS][3] = {
        {230, 41, 55}, {40, 110, 230}, {30, 180, 70}, {245, 205, 30},
        {150, 70, 200}, {245, 130, 20}, {30, 200, 210}, {240, 110, 180},
    };
    int i = line < 0 ? 0 : line % TRACK_LINE_COLORS;
    *r = rgb[i][0]; *g = rgb[i][1]; *b = rgb[i][2];
}

void ClearTrains(void) {
    trackDistReady = false;   // runtime trip estimates start over
    trainDecisions = trainDecisionsFaster = 0;
    trainChoicesConsidered = 0;
    trainTransferPlans = 0;
    memset(trainPlanChanges, 0, sizeof(trainPlanChanges));
    transitGen++;
    trainDecisionTrainSec = trainDecisionWalkSec = 0.0;
    moverGaveUpInit = false;   // forget who gave up (cooldowns)
    trainCargoLoaded = trainCargoUnloaded = trainCargoHaulDrops = 0;
    for (int i = 0; i < MAX_TRAINS; i++) {
        if (trains[i].active) DropTrainCargo(i);
        trains[i].cargoCount = 0;
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
    trackLineCellCount = 0;   // lines come back with RebuildStations
    trackLineCount = 0;
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
    t->cargoCount = 0;
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

// Is platform cell (x,y,z) already part of one of the first `count` stations?
static bool PlatformCellClaimed(int x, int y, int z, int count) {
    for (int si = 0; si < count; si++) {
        if (stations[si].z != z) continue;
        for (int pc = 0; pc < stations[si].platformCellCount; pc++)
            if (stations[si].platformCells[pc][0] == x && stations[si].platformCells[pc][1] == y) return true;
    }
    return false;
}

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
                    if (PlatformCellClaimed(px, py, z, stationCount)) continue;
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

                    // Walk in +trackDir, then -trackDir, each up to the first cell another stop
                    // already has: which way +trackDir points depends on the platform's side, and
                    // walking back over claimed cells made a stop per cell for a platform below
                    // (or left of) its track
                    int cx = px + trackDirX, cy = py + trackDirY;
                    while (s->platformCellCount < MAX_PLATFORM_CELLS) {
                        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) break;
                        if (GRID(cx, cy, z) != CELL_PLATFORM) break;
                        if (PlatformCellClaimed(cx, cy, z, stationCount)) break;
                        s->platformCells[s->platformCellCount][0] = cx;
                        s->platformCells[s->platformCellCount][1] = cy;
                        s->platformCellCount++;
                        cx += trackDirX;
                        cy += trackDirY;
                    }
                    int plusCells = s->platformCellCount - 1;
                    // Walk in -trackDir
                    cx = px - trackDirX;
                    cy = py - trackDirY;
                    while (s->platformCellCount < MAX_PLATFORM_CELLS) {
                        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) break;
                        if (GRID(cx, cy, z) != CELL_PLATFORM) break;
                        if (PlatformCellClaimed(cx, cy, z, stationCount)) break;
                        s->platformCells[s->platformCellCount][0] = cx;
                        s->platformCells[s->platformCellCount][1] = cy;
                        s->platformCellCount++;
                        cx -= trackDirX;
                        cy -= trackDirY;
                    }

                    // The line forms along the stop's own cells
                    if (plusCells == 0 && s->platformCellCount > 1) {
                        s->queueDirX = -trackDirX;
                        s->queueDirY = -trackDirY;
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

    RebuildTrackLines();
    transitGen++;   // the transit graph rebuilds on its next use

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
    uint8_t conn = TRACK_CONN(x, y, z);
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

// ============================================================================
// Transit graph: trip choice over any number of changes of line
// ============================================================================
//
// Built from the track, the stations and the trains (lazily, when one of them
// changed). Per line, one train's route is followed until it repeats: that cycle
// (a loop once round, a shuttle out and back) gives the line's stops in the order
// trains reach them. A node is "a train of line L at stop k of its cycle" (a
// shuttle's middle stop is two nodes, one per direction). Edges: ride on to the
// next stop of the cycle, or get off and walk at most TRAIN_TRANSFER_WALK cells (any level) to
// a node of another line. A trip is a Dijkstra search from the stops near the mover
// (walk + the real wait for the next train there) to the stops near its goal (walk
// out). Later waits are estimates: the station's measured wait, else half the gap
// between the line's trains. Trains follow track deterministically except at
// junctions (10% random turns), so a line with junctions is only roughly right.

#define TRANSIT_MAX_NODES 256
#define TRANSIT_ACCESS_DZ 12   // stations this many levels above/below count (walks by the pathfinder)
#define TRANSIT_MAX_LINES 32
#define TRANSIT_MAX_CHANGES 4096

typedef struct { short x, y, px, py; } TransitCell;
typedef struct {
    int line;           // index into transitLines
    int station;
    int cycleIdx;       // where in the line's cycle the train is at this stop
    int cellsToNext;    // to the next stop of the cycle
    int next;           // that stop's node
    int changeStart, changeCount;   // into transitChanges
} TransitNode;
typedef struct { int to; float walkCells; } TransitChange;
typedef struct {
    int trackLine, z;
    TransitCell* cycle;
    int cycleLen;
    int trainCount;
} TransitLine;

static TransitNode transitNodes[TRANSIT_MAX_NODES];
static int transitNodeCount = 0;
static TransitLine transitLines[TRANSIT_MAX_LINES];
static int transitLineCount = 0;
static TransitChange transitChanges[TRANSIT_MAX_CHANGES];
static int transitChangeCount = 0;
static int transitBuiltGen = -1;
static unsigned transitBuiltTrains = 0;

bool useTrainTransfers = true;
int trainTransferPlans = 0;
int trainPlanChanges[TRAIN_PLAN_CHANGES_MAX + 1];

static unsigned TransitTrainSignature(void) {
    unsigned sig = 2166136261u;
    for (int i = 0; i < MAX_TRAINS; i++) sig = (sig ^ (trains[i].active ? (unsigned)(i + 1) : 0u)) * 16777619u;
    return sig;
}

static void TransitFree(void) {
    for (int l = 0; l < transitLineCount; l++) free(transitLines[l].cycle);
    transitLineCount = transitNodeCount = transitChangeCount = 0;
}

static void TransitBuild(void) {
    TransitFree();
    int limit = 4 * (gridWidth + gridHeight) * 8;
    for (int L = 0; L < TrackLineCount() && transitLineCount < TRANSIT_MAX_LINES; L++) {
        int t0 = -1, count = 0;
        for (int t = 0; t < MAX_TRAINS; t++)
            if (trains[t].active && TrainLine(t) == L) { if (t0 < 0) t0 = t; count++; }
        if (t0 < 0) continue;   // no train: nobody can ride this line
        const Train* t = &trains[t0];
        int x = t->cellX, y = t->cellY, px = t->prevCellX, py = t->prevCellY, z = t->z;
        int nx, ny;
        if (px == x && py == y) {   // just spawned: take one step to get a direction
            if (!FollowTrack(z, x, y, px, py, &nx, &ny)) continue;
            px = x; py = y; x = nx; y = ny;
        }
        TransitCell* cyc = malloc(sizeof(TransitCell) * (size_t)limit);
        if (!cyc) continue;
        int len = 0, sx = x, sy = y, spx = px, spy = py;
        bool closed = false;
        while (len < limit) {
            cyc[len++] = (TransitCell){ (short)x, (short)y, (short)px, (short)py };
            if (!FollowTrack(z, x, y, px, py, &nx, &ny)) break;
            px = x; py = y; x = nx; y = ny;
            if (x == sx && y == sy && px == spx && py == spy) { closed = true; break; }
        }
        if (!closed) { free(cyc); continue; }
        TransitLine* tl = &transitLines[transitLineCount];
        *tl = (TransitLine){ L, z, cyc, len, count };
        int first = transitNodeCount;
        for (int i = 0; i < len && transitNodeCount < TRANSIT_MAX_NODES; i++) {
            int st = GetStationAt(cyc[i].x, cyc[i].y, z);
            int prevSt = GetStationAt(cyc[(i + len - 1) % len].x, cyc[(i + len - 1) % len].y, z);
            if (st < 0 || st == prevSt) continue;
            transitNodes[transitNodeCount++] = (TransitNode){ transitLineCount, st, i, 0, -1, 0, 0 };
        }
        int n = transitNodeCount - first;
        if (n == 0) { free(cyc); transitNodeCount = first; continue; }
        for (int k = 0; k < n; k++) {
            TransitNode* nd = &transitNodes[first + k];
            const TransitNode* nx2 = &transitNodes[first + (k + 1) % n];
            nd->next = first + (k + 1) % n;
            nd->cellsToNext = (nx2->cycleIdx - nd->cycleIdx + len) % len;
            if (nd->cellsToNext == 0) nd->cellsToNext = len;
        }
        transitLineCount++;
    }
    // Changes: to another line's stop within TRAIN_TRANSFER_WALK cells of walking, by
    // the pathfinder (so a change can go up or down ramps to a line on another level).
    // One bounded search per station, from its boarding cell.
    static float changeWalk[MAX_STATIONS][MAX_STATIONS];   // cells, -1 = too far / no way
    for (int a = 0; a < stationCount; a++) {
        NavPoint targets[MAX_STATIONS];
        int stationOf[MAX_STATIONS], n = 0;
        for (int b = 0; b < stationCount; b++) {
            changeWalk[a][b] = -1.0f;
            if (b == a || !stations[a].active || !stations[b].active) continue;
            if (abs(stations[b].z - stations[a].z) > TRANSIT_ACCESS_DZ) continue;
            int bx, by;
            GetNearestPlatformCell(b, stations[a].platX, stations[a].platY, &bx, &by);
            if (abs(bx - stations[a].platX) + abs(by - stations[a].platY) > TRAIN_TRANSFER_WALK) continue;
            targets[n] = (NavPoint){bx, by, stations[b].z};
            stationOf[n++] = b;
        }
        if (n == 0) continue;
        int costs[MAX_STATIONS];
        NavCostsToTargets(NavGame(), (NavPoint){stations[a].platX, stations[a].platY, stations[a].z},
                          targets, n, 10 * TRAIN_TRANSFER_WALK, costs);
        for (int k = 0; k < n; k++)
            if (costs[k] >= 0) changeWalk[a][stationOf[k]] = costs[k] / 10.0f;
    }
    for (int a = 0; a < transitNodeCount; a++) {
        TransitNode* na = &transitNodes[a];
        na->changeStart = transitChangeCount;
        for (int b = 0; b < transitNodeCount && transitChangeCount < TRANSIT_MAX_CHANGES; b++) {
            const TransitNode* nb = &transitNodes[b];
            if (nb->line == na->line) continue;
            float walk = changeWalk[na->station][nb->station];
            if (walk < 0.0f) continue;
            transitChanges[transitChangeCount++] = (TransitChange){ b, walk };
        }
        na->changeCount = transitChangeCount - na->changeStart;
    }
    transitBuiltGen = transitGen;
    transitBuiltTrains = TransitTrainSignature();
}

static void TransitEnsure(void) {
    if (transitBuiltGen != transitGen || transitBuiltTrains != TransitTrainSignature()) TransitBuild();
}

// Where in its line's cycle a train is (-1: not on the cycle)
static int TransitTrainCycleIdx(const TransitLine* tl, const Train* t) {
    for (int i = 0; i < tl->cycleLen; i++) {
        const TransitCell* c = &tl->cycle[i];
        if (c->x == t->cellX && c->y == t->cellY && c->px == t->prevCellX && c->py == t->prevCellY) return i;
    }
    return -1;
}

// Expected wait for a train at a stop further on in the trip
static float TransitLaterWait(const TransitNode* n, float cellsPerSec) {
    if (stationWaitSamples[n->station] >= 3) return stationAvgWait[n->station];
    const TransitLine* tl = &transitLines[n->line];
    return 0.5f * (tl->cycleLen / cellsPerSec) / (float)(tl->trainCount > 0 ? tl->trainCount : 1);
}

// Seconds from now until a train of the node's line reaches its stop, at or after
// `notBefore` seconds, skipping `skip` trains (a long line at the platform fills them)
static float TransitFirstArrival(const TransitNode* n, float notBefore, int skip, float cellsPerSec,
                                 const int* trainIdx) {
    const TransitLine* tl = &transitLines[n->line];
    float lap = tl->cycleLen / cellsPerSec;
    float arrivals[MAX_TRAINS * 4];
    int count = 0;
    for (int t = 0; t < MAX_TRAINS; t++) {
        if (trainIdx[t] < 0 || TrainLine(t) != tl->trackLine) continue;
        float delay = (trains[t].cartState == CART_DOORS_OPEN) ? trains[t].stateTimer : 0.0f;
        float at = delay + ((n->cycleIdx - trainIdx[t] + tl->cycleLen) % tl->cycleLen) / cellsPerSec;
        if (at < notBefore) at += ceilf((notBefore - at) / lap) * lap;
        for (int k = 0; k < 4 && count < MAX_TRAINS * 4; k++) arrivals[count++] = at + k * lap;
    }
    if (count == 0) return 1e9f;
    if (skip > count - 1) skip = count - 1;
    for (int i = 0; i <= skip; i++)   // partial selection sort: the (skip+1)-th earliest
        for (int j = i + 1; j < count; j++)
            if (arrivals[j] < arrivals[i]) { float tmp = arrivals[i]; arrivals[i] = arrivals[j]; arrivals[j] = tmp; }
    return arrivals[skip];
}

// Runtime only: the lines of each mover's planned trip (for the tooltip)
typedef struct { signed char lines[TRAIN_PLAN_CHANGES_MAX + 1]; unsigned char count; } PlannedRoute;
static PlannedRoute plannedRoute[MAX_MOVERS];

int TrainPlannedRoute(int moverIdx, int* lines, int maxLines) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS || movers[moverIdx].transportState == TRANSPORT_NONE) return 0;
    int n = plannedRoute[moverIdx].count < maxLines ? plannedRoute[moverIdx].count : maxLines;
    for (int i = 0; i < n; i++) lines[i] = plannedRoute[moverIdx].lines[i];
    return n;
}

// Walking seconds between (x,y,z) and each station's nearest platform cell, by the
// pathfinder's own costs (ramps, ladders, mud: any level), for stations within
// TRANSPORT_STATION_RADIUS across and TRANSIT_ACCESS_DZ levels; -1 otherwise.
// toStation false: from the platform to the point (searched from the point: walking
// is near enough symmetric).
static void TransitAccessSeconds(Point p, float pace, float* outSec) {
    NavPoint targets[MAX_STATIONS];
    int stationOf[MAX_STATIONS], n = 0;
    for (int i = 0; i < stationCount; i++) {
        outSec[i] = -1.0f;
        if (!stations[i].active || abs(stations[i].z - p.z) > TRANSIT_ACCESS_DZ) continue;
        int px, py;
        GetNearestPlatformCell(i, p.x, p.y, &px, &py);
        if (abs(px - p.x) + abs(py - p.y) > TRANSPORT_STATION_RADIUS) continue;
        targets[n] = (NavPoint){px, py, stations[i].z};
        stationOf[n++] = i;
    }
    if (n == 0) return;
    int costs[MAX_STATIONS];
    NavCostsToTargets(NavGame(), (NavPoint){p.x, p.y, p.z}, targets, n,
                      10 * 3 * TRANSPORT_STATION_RADIUS, costs);
    for (int k = 0; k < n; k++)
        if (costs[k] >= 0) outSec[stationOf[k]] = (costs[k] / 10.0f) * pace;
}

// Best trip by train for the mover to its goal. Returns seconds (1e9 = none) and
// the stop to board at and the stop to get off at for the first ride (the mover
// plans again there), the number of changes and the lines in order.
static float TransitPlan(int moverIdx, int* outEntry, int* outExit, int* outChanges,
                         signed char* outLines, int* outLineCount) {
    *outEntry = *outExit = -1;
    *outChanges = 0;
    *outLineCount = 0;
    TransitEnsure();
    if (transitNodeCount == 0) return 1e9f;
    const Mover* m = &movers[moverIdx];
    int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE), mz = (int)m->z;
    float pace = MoverWalkSecondsPerCell(moverIdx);
    float cellsPerSec = trainSpeed * (60.0f / dayLength);
    if (cellsPerSec <= 0.0f) return 1e9f;

    int trainIdx[MAX_TRAINS];
    for (int t = 0; t < MAX_TRAINS; t++) {
        trainIdx[t] = -1;
        if (!trains[t].active) continue;
        int L = TrainLine(t);
        for (int l = 0; l < transitLineCount; l++)
            if (transitLines[l].trackLine == L) { trainIdx[t] = TransitTrainCycleIdx(&transitLines[l], &trains[t]); break; }
    }

    static float dist[TRANSIT_MAX_NODES];
    static int pred[TRANSIT_MAX_NODES];        // previous node, -1 = boarded here first
    static bool predChange[TRANSIT_MAX_NODES]; // reached by changing line
    static bool done[TRANSIT_MAX_NODES];
    for (int i = 0; i < transitNodeCount; i++) { dist[i] = 1e9f; pred[i] = -1; predChange[i] = false; done[i] = false; }

    static float accessIn[MAX_STATIONS], accessOut[MAX_STATIONS];
    TransitAccessSeconds((Point){mx, my, mz}, pace, accessIn);
    TransitAccessSeconds(m->goal, pace, accessOut);
    for (int i = 0; i < transitNodeCount; i++) {   // boarding near the mover
        const TransitNode* n = &transitNodes[i];
        const TrainStation* st = &stations[n->station];
        float walkIn = accessIn[n->station];
        if (walkIn < 0.0f) continue;
        float arrive = TransitFirstArrival(n, walkIn, st->waitingCount / MAX_CART_CAPACITY, cellsPerSec, trainIdx);
        if (arrive >= 1e8f) continue;
        dist[i] = arrive + TRAIN_DOOR_TIME;
    }

    float best = 1e9f;
    int bestEnd = -1;
    for (;;) {
        int u = -1;
        for (int i = 0; i < transitNodeCount; i++)
            if (!done[i] && dist[i] < 1e8f && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0 || dist[u] >= best) break;
        done[u] = true;
        const TransitNode* n = &transitNodes[u];
        // get off here for the goal (after riding at least one stop)
        if (pred[u] >= 0 && !predChange[u] && accessOut[n->station] >= 0.0f) {
            float total = dist[u] + 0.5f * TRAIN_DOOR_TIME + accessOut[n->station];
            if (total < best) { best = total; bestEnd = u; }
        }
        // ride on (a stop on the way costs half a door stop: it may or may not stop)
        int v = n->next;
        float ride = dist[u] + n->cellsToNext / cellsPerSec + 0.5f * TRAIN_DOOR_TIME;
        if (!done[v] && ride < dist[v]) { dist[v] = ride; pred[v] = u; predChange[v] = false; }
        // change line (only where the mover got here by train)
        if (!useTrainTransfers || pred[u] < 0 || predChange[u]) continue;
        for (int c = 0; c < n->changeCount; c++) {
            const TransitChange* ch = &transitChanges[n->changeStart + c];
            int w = ch->to;
            if (done[w]) continue;
            float cost = dist[u] + 0.5f * TRAIN_DOOR_TIME + ch->walkCells * pace
                       + TransitLaterWait(&transitNodes[w], cellsPerSec) + TRAIN_DOOR_TIME;
            if (cost < dist[w]) { dist[w] = cost; pred[w] = u; predChange[w] = true; }
        }
    }
    if (bestEnd < 0) return 1e9f;

    // Walk back: the boarding node, the first change, the lines in order
    int path[TRANSIT_MAX_NODES], len = 0;
    for (int u = bestEnd; u >= 0 && len < TRANSIT_MAX_NODES; u = pred[u]) path[len++] = u;
    int board = path[len - 1];
    int firstOff = bestEnd, changes = 0, lineCount = 0;
    outLines[lineCount++] = (signed char)transitLines[transitNodes[board].line].trackLine;
    for (int k = len - 2; k >= 0; k--) {
        int u = path[k];
        if (!predChange[u]) continue;
        if (changes == 0) firstOff = path[k + 1];
        changes++;
        if (lineCount <= TRAIN_PLAN_CHANGES_MAX)
            outLines[lineCount++] = (signed char)transitLines[transitNodes[u].line].trackLine;
    }
    *outEntry = transitNodes[board].station;
    *outExit = transitNodes[firstOff].station;
    *outChanges = changes;
    *outLineCount = lineCount;
    if (*outEntry == *outExit) return 1e9f;
    return best;
}

void TrainsMarkPersonalTrip(int moverIdx, bool personal) {
    if (moverIdx >= 0 && moverIdx < MAX_MOVERS) personalTrip[moverIdx] = personal;
}

bool TrainsIsPersonalTrip(int moverIdx) {
    return moverIdx >= 0 && moverIdx < MAX_MOVERS && personalTrip[moverIdx] &&
           movers[moverIdx].transportState != TRANSPORT_NONE;
}

// After a load: stations and trains come from the save, but lines, the transit graph and
// the runtime estimates are derived and were never saved (RebuildStations is skipped for
// v86+ saves). Without this every station is on no line: no freight, no transfers.
void TrainsAfterLoad(void) {
    RebuildTrackLines();
    transitGen++;
    trackDistReady = false;
    moverGaveUpInit = false;
    memset(personalTrip, 0, sizeof(personalTrip));
    memset(plannedRoute, 0, sizeof(plannedRoute));
}

int StationBoardings(int stationIdx) {
    return (trackDistReady && stationIdx >= 0 && stationIdx < MAX_STATIONS) ? stationBoardings[stationIdx] : 0;
}

// Stats: a trip the decision sent by train (headless output)
static float chosenTripSec = -1.0f;   // set by ShouldUseTrain (time-based choice)

void TrainNoteDecision(int moverIdx, int entry, int exit) {
    float train = chosenTripSec >= 0.0f ? chosenTripSec
                                        : TrainTripSeconds(moverIdx, entry, exit, movers[moverIdx].goal);
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
    chosenTripSec = -1.0f;
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

    // Time-based: take the train when the best trip (transit graph, any number of
    // changes) beats walking the current path by 10%. Stations on other levels count:
    // walking to and from them is costed by the pathfinder.
    if (useTrainTimeChoice) {
        int a, b, changes, lineCount;
        signed char lines[TRAIN_PLAN_CHANGES_MAX + 1];
        float train = TransitPlan(moverIdx, &a, &b, &changes, lines, &lineCount);
        if (a < 0 || train >= 1e8f) return false;
        trainChoicesConsidered++;
        if (train >= 0.9f * WalkTripSeconds(moverIdx)) return false;
        chosenEntry = a;
        chosenExit = b;
        chosenTripSec = train;
        if (changes > 0) trainTransferPlans++;
        trainPlanChanges[changes < TRAIN_PLAN_CHANGES_MAX ? changes : TRAIN_PLAN_CHANGES_MAX]++;
        PlannedRoute* r = &plannedRoute[moverIdx];
        r->count = (unsigned char)lineCount;
        for (int i = 0; i < lineCount; i++) r->lines[i] = lines[i];
        return true;
    }

    // Old rule: same level, goal far enough, a station near both ends
    if (m->goal.z != mz) return false;
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

    uint8_t conn = TRACK_CONN(t->cellX, t->cellY, t->z);
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

// ============================================================================
// Freight
// ============================================================================

int trainCargoLoaded = 0, trainCargoUnloaded = 0, trainCargoHaulDrops = 0;
bool trainFreight = true;

int StationCargoStockpiles(int stationIdx, int* out, int maxOut) {
    if (stationIdx < 0 || stationIdx >= stationCount || !stations[stationIdx].active) return 0;
    const TrainStation* st = &stations[stationIdx];
    int n = 0;
    for (int sp = 0; sp < stockpileHighWater && n < maxOut; sp++) {
        const Stockpile* s = &stockpiles[sp];
        if (!s->active || s->z != st->z) continue;
        bool near = false;
        for (int p = 0; p < st->platformCellCount && !near; p++) {
            int px = st->platformCells[p][0], py = st->platformCells[p][1];
            int dx = px < s->x ? s->x - px : (px >= s->x + s->width ? px - (s->x + s->width - 1) : 0);
            int dy = py < s->y ? s->y - py : (py >= s->y + s->height ? py - (s->y + s->height - 1) : 0);
            near = dx <= TRAIN_CARGO_REACH && dy <= TRAIN_CARGO_REACH;
        }
        if (near) out[n++] = sp;
    }
    return n;
}

static bool LineHasTrain(int line) {
    for (int t = 0; t < MAX_TRAINS; t++) if (trains[t].active && TrainLine(t) == line) return true;
    return false;
}

// Another stop of `line` (not `fromStation`) has a cargo stockpile with priority above
// `fromPriority` that accepts the item and has room
static bool CargoBetterStopOnLine(int line, int fromStation, int itemIdx, int fromPriority) {
    const Item* it = &items[itemIdx];
    for (int st = 0; st < stationCount; st++) {
        if (st == fromStation || !stations[st].active || StationLine(st) != line) continue;
        int sps[16];
        int n = StationCargoStockpiles(st, sps, 16);
        for (int k = 0; k < n; k++) {
            if (stockpiles[sps[k]].priority <= fromPriority) continue;
            int sx, sy;
            if (FindFreeStockpileSlot(sps[k], it->type, it->material, &sx, &sy)) return true;
        }
    }
    return false;
}

bool TrainCargoWillForward(int stockpileIdx, int itemIdx) {
    if (!trainFreight || stockpileIdx < 0 || !stockpiles[stockpileIdx].active) return false;
    for (int st = 0; st < stationCount; st++) {
        int sps[16];
        int n = StationCargoStockpiles(st, sps, 16);
        for (int k = 0; k < n; k++) {
            if (sps[k] != stockpileIdx) continue;
            int line = StationLine(st);
            if (line >= 0 && LineHasTrain(line) &&
                CargoBetterStopOnLine(line, st, itemIdx, stockpiles[stockpileIdx].priority)) return true;
        }
    }
    return false;
}

typedef struct { int line, station, priority; } CargoWant;
static bool CargoWantItem(int itemIdx, void* user) {
    const CargoWant* w = user;
    return CargoBetterStopOnLine(w->line, w->station, itemIdx, w->priority);
}

static int TrainCargoCapacity(const Train* t) {
    int cap = (t->carCount > 0 ? t->carCount : 1) * TRAIN_CARGO_PER_CAR;
    return cap < MAX_TRAIN_CARGO ? cap : MAX_TRAIN_CARGO;
}

static void CargoRemoveAt(Train* t, int k) {
    t->cargo[k] = t->cargo[t->cargoCount - 1];
    t->cargoFromPriority[k] = t->cargoFromPriority[t->cargoCount - 1];
    t->cargoCount--;
}

// The cargo stockpile here that would take this load (priority above where it came from)
static int CargoUnloadTarget(int stationIdx, const Train* t, int k) {
    const Item* it = &items[t->cargo[k]];
    int sps[16], best = -1;
    int n = StationCargoStockpiles(stationIdx, sps, 16);
    for (int j = 0; j < n; j++) {
        int sp = sps[j];
        if (stockpiles[sp].priority <= t->cargoFromPriority[k]) continue;
        int sx, sy;
        if (!FindFreeStockpileSlot(sp, it->type, it->material, &sx, &sy)) continue;
        if (best < 0 || stockpiles[sp].priority > stockpiles[best].priority) best = sp;
    }
    return best;
}

// Something to unload or load at this stop (decides whether the train stops)
static bool TrainCargoWorkAt(int trainIdx, int stationIdx) {
    Train* t = &trains[trainIdx];
    if (!trainFreight && t->cargoCount == 0) return false;   // off: only unload what is aboard
    for (int k = 0; k < t->cargoCount; k++)
        if (CargoUnloadTarget(stationIdx, t, k) >= 0) return true;
    if (t->cargoCount >= TrainCargoCapacity(t)) return false;
    int line = TrainLine(trainIdx), sps[16];
    int n = StationCargoStockpiles(stationIdx, sps, 16);
    for (int j = 0; j < n; j++) {
        const Stockpile* s = &stockpiles[sps[j]];
        int slots = s->width * s->height;
        for (int i = 0; i < slots; i++) {
            int si = s->slots[i];
            if (!s->cells[i] || s->slotIsContainer[i] || si < 0 || !items[si].active) continue;
            if (items[si].state != ITEM_IN_STOCKPILE || items[si].reservedBy != -1) continue;
            if (CargoBetterStopOnLine(line, stationIdx, si, s->priority)) return true;
        }
    }
    return false;
}

static void TrainCargoAtStop(int trainIdx, int stationIdx) {
    Train* t = &trains[trainIdx];
    for (int k = t->cargoCount - 1; k >= 0; k--) {   // unload first
        int sp = CargoUnloadTarget(stationIdx, t, k);
        if (sp < 0) continue;
        int it = t->cargo[k];
        if (StockpilePutLoad(sp, it)) {
            CargoRemoveAt(t, k);
            trainCargoUnloaded++;
        }
    }
    if (!trainFreight) return;
    int line = TrainLine(trainIdx), sps[16];
    int n = StationCargoStockpiles(stationIdx, sps, 16);
    for (int j = 0; j < n && t->cargoCount < TrainCargoCapacity(t); j++) {
        CargoWant w = { line, stationIdx, stockpiles[sps[j]].priority };
        while (t->cargoCount < TrainCargoCapacity(t)) {
            int it = StockpileTakeStack(sps[j], -1, CargoWantItem, &w);
            if (it < 0) break;
            items[it].state = ITEM_ON_TRAIN;
            items[it].reservedBy = -1;
            items[it].x = t->x; items[it].y = t->y; items[it].z = (float)t->z;
            t->cargo[t->cargoCount] = it;
            t->cargoFromPriority[t->cargoCount] = (unsigned char)stockpiles[sps[j]].priority;
            t->cargoCount++;
            trainCargoLoaded++;
        }
    }
}

void DropTrainCargo(int trainIdx) {
    Train* t = &trains[trainIdx];
    for (int k = 0; k < t->cargoCount; k++) {
        int it = t->cargo[k];
        if (it < 0 || it >= MAX_ITEMS || !items[it].active || items[it].state != ITEM_ON_TRAIN) continue;
        items[it].state = ITEM_ON_GROUND;
        items[it].x = t->cellX * CELL_SIZE + CELL_SIZE * 0.5f;
        items[it].y = t->cellY * CELL_SIZE + CELL_SIZE * 0.5f;
        items[it].z = (float)t->z;
    }
    t->cargoCount = 0;
}

bool TrainCargoRedirectHaul(int moverIdx, int entryStation) {
    Mover* m = &movers[moverIdx];
    if (!trainFreight || m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    if (!job || job->type != JOBTYPE_HAUL || job->step != STEP_CARRYING) return false;
    int it = job->carryingItem, target = job->targetStockpile;
    if (it < 0 || !items[it].active || target < 0 || !stockpiles[target].active) return false;
    if (items[it].contentCount > 0) return false;   // containers ride with their hauler
    int line = StationLine(entryStation);
    if (line < 0 || !LineHasTrain(line)) return false;
    int sps[16];
    int n = StationCargoStockpiles(entryStation, sps, 16);
    for (int j = 0; j < n; j++) {
        int sp = sps[j];
        if (sp == target || stockpiles[sp].priority >= stockpiles[target].priority) continue;
        if (!StockpileAcceptsItem(sp, items[it].type, items[it].material)) continue;
        if (!CargoBetterStopOnLine(line, entryStation, it, stockpiles[sp].priority)) continue;
        int sx, sy;
        if (!FindFreeStockpileSlot(sp, items[it].type, items[it].material, &sx, &sy)) continue;
        if (!ReserveStockpileSlot(sp, sx, sy, moverIdx, items[it].type, items[it].material)) continue;
        ReleaseStockpileSlot(target, job->targetSlotX, job->targetSlotY);
        InvalidateStockpileSlotCache(items[it].type, items[it].material);
        job->targetStockpile = sp;
        job->targetSlotX = sx;
        job->targetSlotY = sy;
        m->goal = (Point){sx, sy, stockpiles[sp].z};
        m->needsRepath = true;
        trainCargoHaulDrops++;
        EventLog("Mover %d drops item %d at cargo stop (stockpile %d, station %d) instead of riding",
                 moverIdx, it, sp, entryStation);
        return true;
    }
    return false;
}

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
            DropTrainCargo(i);
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
                bool hasCargo = TrainCargoWorkAt(i, stIdx);
                if (hasWaiters || hasExiters || hasCargo) {
                    // Pull forward: only stop if the next track cell is NOT part of this station
                    int peekX, peekY;
                    bool hasNext = FindNextTrackCell(t, &peekX, &peekY);
                    bool nextIsStation = hasNext && GetStationAt(peekX, peekY, t->z) == stIdx;
                    if (!nextIsStation) {
                        t->cartState = CART_DOORS_OPEN;
                        t->stateTimer = TRAIN_DOOR_TIME;
                        t->atStation = stIdx;
                        t->progress = 0.0f;
                        if (hasCargo) TrainCargoAtStop(i, stIdx);
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

        // Freight rides along
        for (int k = 0; k < t->cargoCount; k++) {
            int it = t->cargo[k];
            if (it >= 0 && it < MAX_ITEMS && items[it].active) { items[it].x = t->x; items[it].y = t->y; }
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
