#ifndef NAV_HOST_H
#define NAV_HOST_H

// =============================================================================
// NAV HOST INTERFACE — the only world knowledge the pathfinder is allowed to use
// =============================================================================
//
// pathfinding.c asks the world questions exclusively through the Nav* functions
// below. It does not read grid[], cell types, water, snow, fog, furniture, etc.
//
// This file is the *host* side of the seam: NavKit's colony sim implements the
// contract on top of cell_defs.h. A different host (a crowd demo, another game)
// supplies its own nav_host.h with the same function names and semantics.
//
// Everything is static inline so the seam costs nothing in the hot search loops.
// All coordinates are (x, y, z). Callers may pass out-of-bounds coordinates;
// implementations must return false / a safe default for those.
//
// Contract:
//   NavIsWalkable(x,y,z)          can an agent stand in this cell?
//   NavMoveCost(x,y,z)            cost to enter the cell, fixed-point (10 = 1.0x).
//                                 Must be >= NAV_MIN_CELL_COST (heuristic admissibility).
//   NavCanClimbUp(x,y,z)          vertical link (x,y,z) -> (x,y,z+1)  (ladder-like)
//   NavCanClimbDown(x,y,z)        vertical link (x,y,z) -> (x,y,z-1)
//   NavRampHighSide(x,y,z,&dx,&dy) is (x,y,z) a ramp? if so, its high side direction
//   NavRampUp(x,y,z,&ex,&ey)      ramp at (x,y,z) leads up to (ex,ey,z+1), exit walkable
//   NavCanEnterFrom(x,y,z,fx,fy)  same-z step (fx,fy)->(x,y) allowed? (ramp side-entry)
//   NavAffectedZ(z,outZ)          a change at z also changes walkability at these z's
//   NavIsValidDestination(x,y,z)  walkable AND sensible as a random goal (demo/test utils)
//   NavOnChunkDirty(x,y,z)        notification: pathfinder saw a cell change
// =============================================================================

#include "cell_defs.h"

#define NAV_MIN_CELL_COST MIN_CELL_COST

static inline bool NavIsWalkable(int x, int y, int z) {
    return IsCellWalkableAt(z, y, x);
}

static inline int NavMoveCost(int x, int y, int z) {
    return GetCellMoveCost(x, y, z);
}

static inline bool NavCanClimbUp(int x, int y, int z) {
    return CanClimbUpAt(x, y, z);
}

static inline bool NavCanClimbDown(int x, int y, int z) {
    return CanClimbDownAt(x, y, z);
}

static inline bool NavRampHighSide(int x, int y, int z, int* dx, int* dy) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return false;
    GetRampHighSideOffset(cell, dx, dy);
    return true;
}

static inline bool NavRampUp(int x, int y, int z, int* exitX, int* exitY) {
    int dx, dy;
    if (!NavRampHighSide(x, y, z, &dx, &dy)) return false;
    if (!CanWalkUpRampAt(x, y, z)) return false;
    *exitX = x + dx;
    *exitY = y + dy;
    return true;
}

static inline bool NavCanEnterFrom(int x, int y, int z, int fromX, int fromY) {
    return CanEnterRampFromSide(x, y, z, fromX, fromY);
}

static inline int NavAffectedZ(int z, int* outZ) {
    return GetAdditionalAffectedZLevels(z, outZ);
}

static inline bool NavIsValidDestination(int x, int y, int z) {
    return IsValidDestination(z, y, x);
}

// Lighting caches walkability-adjacent data; the pathfinder shouldn't know that.
void InvalidateLighting(void);
static inline void NavOnChunkDirty(int x, int y, int z) {
    (void)x; (void)y; (void)z;
    InvalidateLighting();
}

#endif // NAV_HOST_H
