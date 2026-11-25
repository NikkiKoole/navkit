#ifndef NAV_HOST_H
#define NAV_HOST_H

// =============================================================================
// NavKit colony sim — host implementation of the nav library contract
// =============================================================================
//
// nav/nav.c asks the world questions exclusively through these functions
// (contract documented in nav/nav.h). This header answers them from the
// game's grid via cell_defs.h. The game has a single world, so `host` is
// unused here; another host (e.g. a crowd demo with several maps) would pass
// its own map pointer to NavCreate and read it back from `host`.
//
// All static inline so the seam costs nothing in the hot search loops.
// =============================================================================

#include <time.h>
#include "cell_defs.h"
#include "../../vendor/raylib.h"

#define NAV_MIN_CELL_COST MIN_CELL_COST

#define NAV_LOG_INFO(...) TraceLog(LOG_INFO, __VA_ARGS__)
#define NAV_LOG_WARN(...) TraceLog(LOG_WARNING, __VA_ARGS__)

// Monotonic clock: raylib's GetTime() is 0 without a window (headless runs, benchmarks)
static inline double NavHostTime(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static inline bool NavIsWalkable(void* host, int x, int y, int z) {
    (void)host;
    return IsCellWalkableAt(z, y, x);
}

// Reachability regions ignore deep water: water is never notified to the
// pathfinder, so regions must stay a superset of what's walkable right now.
static inline bool NavRegionWalkableGame(void* host, int x, int y, int z) {
    (void)host;
    return IsCellWalkableAtEx(z, y, x, true);
}
#define NAV_REGION_WALKABLE NavRegionWalkableGame

// A scenario's extra route cost per cell on one level (cars experiment: pedestrians keep to the
// pavements, crossing roads the short way); walking speed is not affected. NULL: none.
extern unsigned char* navPathExtra;   // gridWidth x gridHeight, for level navPathExtraZ
extern int navPathExtraZ;
static inline int NavMoveCost(void* host, int x, int y, int z) {
    (void)host;
    if (navPathExtra && z == navPathExtraZ) return GetCellMoveCost(x, y, z) + navPathExtra[y * gridWidth + x];
    return GetCellMoveCost(x, y, z);
}

static inline bool NavCanClimbUp(void* host, int x, int y, int z) {
    (void)host;
    return CanClimbUpAt(x, y, z);
}

static inline bool NavCanClimbDown(void* host, int x, int y, int z) {
    (void)host;
    return CanClimbDownAt(x, y, z);
}

static inline bool NavRampHighSide(void* host, int x, int y, int z, int* dx, int* dy) {
    (void)host;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    CellType cell = GRID(x, y, z);
    if (!CellIsDirectionalRamp(cell)) return false;
    GetRampHighSideOffset(cell, dx, dy);
    return true;
}

static inline bool NavRampUp(void* host, int x, int y, int z, int* exitX, int* exitY) {
    int dx, dy;
    if (!NavRampHighSide(host, x, y, z, &dx, &dy)) return false;
    if (!CanWalkUpRampAt(x, y, z)) return false;
    *exitX = x + dx;
    *exitY = y + dy;
    return true;
}

static inline bool NavCanEnterFrom(void* host, int x, int y, int z, int fromX, int fromY) {
    (void)host;
    return CanStepAcrossRamps(fromX, fromY, x, y, z);   // ramp sides, one-way doors
}

static inline int NavAffectedZ(void* host, int z, int* outZ) {
    (void)host;
    return GetAdditionalAffectedZLevels(z, outZ);
}

// Ladder queues make climbing cost more (entities/ladders.c keeps the queues); an
// elevator shaft costs the car's travel and the expected wait (entities/elevators.c)
int ClimbExtraCost(int x, int y, int z, int dz);
#define NAV_CLIMB_EXTRA(host, x, y, z, dz) ClimbExtraCost((x), (y), (z), (dz))
// Stairs effort: going up or down a ramp costs more than the walk (setting stairsEffort)
int RampEffortCost(int x, int y, int z, int dz);
#define NAV_RAMP_EXTRA(host, x, y, z, dz) RampEffortCost((x), (y), (z), (dz))
// Boarding an elevator: stepping into a shaft cell costs the expected wait, once per ride
int ElevatorBoardCost(int x, int y, int z);
static inline int NavEnterExtraHost(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return 0;
    return CellIsShaft(GRID(x, y, z)) ? ElevatorBoardCost(x, y, z) : 0;
}
#define NAV_ENTER_EXTRA(host, x, y, z) NavEnterExtraHost((x), (y), (z))

// Lighting caches walkability-adjacent data; the pathfinder shouldn't know that.
void InvalidateLightingAt(int x, int y, int z);
void TreesNoteCellChanged(int x, int y, int z);
static inline void NavOnChunkDirty(void* host, int x, int y, int z) {
    (void)host;
    InvalidateLightingAt(x, y, z);
    TreesNoteCellChanged(x, y, z);   // leaf decay checks near trunk/branch changes
}

// Not part of the library contract: used by the game-side random-cell helpers
static inline bool NavIsValidDestination(int x, int y, int z) {
    return IsValidDestination(z, y, x);
}

#endif // NAV_HOST_H
