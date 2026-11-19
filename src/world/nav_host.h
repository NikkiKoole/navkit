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

#include "cell_defs.h"
#include "../../vendor/raylib.h"

#define NAV_MIN_CELL_COST MIN_CELL_COST

#define NAV_LOG_INFO(...) TraceLog(LOG_INFO, __VA_ARGS__)
#define NAV_LOG_WARN(...) TraceLog(LOG_WARNING, __VA_ARGS__)

static inline double NavHostTime(void) { return GetTime(); }

static inline bool NavIsWalkable(void* host, int x, int y, int z) {
    (void)host;
    return IsCellWalkableAt(z, y, x);
}

static inline int NavMoveCost(void* host, int x, int y, int z) {
    (void)host;
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
    CellType cell = grid[z][y][x];
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
    return CanEnterRampFromSide(x, y, z, fromX, fromY);
}

static inline int NavAffectedZ(void* host, int z, int* outZ) {
    (void)host;
    return GetAdditionalAffectedZLevels(z, outZ);
}

// Lighting caches walkability-adjacent data; the pathfinder shouldn't know that.
void InvalidateLighting(void);
static inline void NavOnChunkDirty(void* host, int x, int y, int z) {
    (void)host; (void)x; (void)y; (void)z;
    InvalidateLighting();
}

// Not part of the library contract: used by the game-side random-cell helpers
static inline bool NavIsValidDestination(int x, int y, int z) {
    return IsValidDestination(z, y, x);
}

#endif // NAV_HOST_H
