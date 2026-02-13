#include "floordirt.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/material.h"
#include "../world/cell_defs.h"
#include "../entities/mover.h"
#include <string.h>

// Grid storage
uint8_t floorDirtGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool floorDirtEnabled = true;

// Per-mover previous cell tracking (avoids Mover struct changes)
static struct { int x, y, z; } prevMoverCell[MAX_MOVERS];

void InitFloorDirt(void) {
    ClearFloorDirt();
}

void ClearFloorDirt(void) {
    memset(floorDirtGrid, 0, sizeof(floorDirtGrid));
    ResetMoverDirtTracking();
}

void ResetMoverDirtTracking(void) {
    for (int i = 0; i < MAX_MOVERS; i++) {
        prevMoverCell[i].x = -1;
        prevMoverCell[i].y = -1;
        prevMoverCell[i].z = -1;
    }
}

// Check if position is natural soil terrain (dirt source for tracking)
// Uses same z/z-1 pattern as TrampleGround() in groundwear.c
bool IsDirtSource(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return false;

    for (int cz = z; cz >= z - 1 && cz >= 0; cz--) {
        CellType cell = grid[cz][y][x];
        if (!CellIsSolid(cell) || !IsWallNatural(x, y, cz)) continue;
        MaterialType mat = GetWallMaterial(x, y, cz);
        if (mat == MAT_DIRT || mat == MAT_CLAY || mat == MAT_SAND
            || mat == MAT_GRAVEL || mat == MAT_PEAT)
            return true;
    }
    return false;
}

// Check if position is a constructed floor (dirt target)
bool IsDirtTarget(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return false;

    // Explicit constructed floor (balconies, bridges)
    if (HAS_FLOOR(x, y, z)) return true;

    // Walking on top of constructed (non-natural) solid block
    if (z > 0) {
        CellType below = grid[z - 1][y][x];
        if (CellIsSolid(below) && !IsWallNatural(x, y, z - 1)) return true;
    }
    return false;
}

// Internal: add dirt from source to target floor
static void TrackFloorDirt(int srcX, int srcY, int srcZ,
                           int dstX, int dstY, int dstZ) {
    if (!IsDirtSource(srcX, srcY, srcZ)) return;
    if (!IsDirtTarget(dstX, dstY, dstZ)) return;

    int amount = DIRT_TRACK_AMOUNT;

    // Muddy source tracks 3x more dirt
    {
        int groundZ = srcZ;
        if (groundZ > 0 && !CellIsSolid(grid[groundZ][srcY][srcX]))
            groundZ--;
        if (IsMuddy(srcX, srcY, groundZ))
            amount *= 3;
    }

    // Stone floors accumulate dirt slower
    MaterialType floorMat = MAT_NONE;
    if (HAS_FLOOR(dstX, dstY, dstZ)) {
        floorMat = GetFloorMaterial(dstX, dstY, dstZ);
    } else if (dstZ > 0) {
        floorMat = GetWallMaterial(dstX, dstY, dstZ - 1);
    }
    if (IsStoneMaterial(floorMat)) {
        amount = (amount * DIRT_STONE_MULTIPLIER) / 100;
        if (amount < 1) amount = 1;
    }

    uint8_t old = floorDirtGrid[dstZ][dstY][dstX];
    int newVal = old + amount;
    if (newVal > DIRT_MAX) newVal = DIRT_MAX;
    floorDirtGrid[dstZ][dstY][dstX] = (uint8_t)newVal;

    if (old == 0 && newVal > 0) dirtActiveCells++;
}

void MoverTrackDirt(int moverIdx, int cellX, int cellY, int cellZ) {
    if (!floorDirtEnabled) return;
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return;

    int px = prevMoverCell[moverIdx].x;
    int py = prevMoverCell[moverIdx].y;
    int pz = prevMoverCell[moverIdx].z;

    // Only track when cell actually changed
    if (px >= 0 && (px != cellX || py != cellY || pz != cellZ)) {
        TrackFloorDirt(px, py, pz, cellX, cellY, cellZ);
    }

    prevMoverCell[moverIdx].x = cellX;
    prevMoverCell[moverIdx].y = cellY;
    prevMoverCell[moverIdx].z = cellZ;
}

int GetFloorDirt(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return 0;
    return floorDirtGrid[z][y][x];
}

void SetFloorDirt(int x, int y, int z, int value) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return;
    uint8_t old = floorDirtGrid[z][y][x];
    uint8_t nv = (uint8_t)(value > DIRT_MAX ? DIRT_MAX : (value < 0 ? 0 : value));
    floorDirtGrid[z][y][x] = nv;
    if (old == 0 && nv > 0) dirtActiveCells++;
    if (old > 0 && nv == 0) dirtActiveCells--;
}

int CleanFloorDirt(int x, int y, int z, int amount) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return 0;
    uint8_t old = floorDirtGrid[z][y][x];
    int nv = (int)old - amount;
    if (nv < 0) nv = 0;
    floorDirtGrid[z][y][x] = (uint8_t)nv;
    if (old > 0 && nv == 0) dirtActiveCells--;
    return nv;
}
