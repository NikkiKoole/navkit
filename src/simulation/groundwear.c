#include "groundwear.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../core/time.h"
#include "fire.h"
#include "trees.h"
#include "../entities/items.h"
#include <string.h>
#include <stdlib.h>

// Wear grid storage (3D)
int wearGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool groundWearEnabled = true;

// Runtime configurable values
int wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
int wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
int wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
int wearDirtToGrass = WEAR_DIRT_TO_GRASS_DEFAULT;
int wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
int wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
float wearRecoveryInterval = 5.0f;  // Decay wear every 5 game-seconds

// Sapling regrowth settings
bool saplingRegrowthEnabled = false;
int saplingRegrowthChance = 5;      // 5 per 10000 = 0.05% per interval per tile
int saplingMinTreeDistance = 4;     // Min 4 tiles from existing trees/saplings
int wearMax = WEAR_MAX_DEFAULT;

// Internal accumulator for game-time
static float wearRecoveryAccum = 0.0f;

// Update surface overlay based on current wear value
static void UpdateSurfaceFromWear(int x, int y, int z) {
    int wear = wearGrid[z][y][x];
    int surface;
    if (wear >= wearGrassToDirt) {
        surface = SURFACE_BARE;
    } else if (wear >= wearNormalToTrampled) {
        surface = SURFACE_TRAMPLED;
    } else if (wear >= wearTallToNormal) {
        surface = SURFACE_GRASS;
    } else {
        surface = SURFACE_TALL_GRASS;
    }
    SET_CELL_SURFACE(x, y, z, surface);
}

// Check if there's a tree or sapling within distance
static bool HasNearbyTree(int x, int y, int z, int dist) {
    for (int dz = -1; dz <= dist; dz++) {  // Check up to dist levels above
        int checkZ = z + dz;
        if (checkZ < 0 || checkZ >= gridDepth) continue;
        
        for (int dy = -dist; dy <= dist; dy++) {
            for (int dx = -dist; dx <= dist; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                
                CellType c = grid[checkZ][ny][nx];
                if (c == CELL_SAPLING || c == CELL_TREE_TRUNK || c == CELL_TREE_LEAVES) {
                    return true;
                }
            }
        }
    }
    return false;
}

static TreeType PickTreeTypeForSoil(CellType soil) {
    switch (soil) {
        case CELL_PEAT: return TREE_TYPE_WILLOW;
        case CELL_SAND: return TREE_TYPE_BIRCH;
        case CELL_GRAVEL: return TREE_TYPE_PINE;
        case CELL_CLAY: return TREE_TYPE_OAK;
        case CELL_DIRT:
        default: return TREE_TYPE_OAK;
    }
}

void InitGroundWear(void) {
    ClearGroundWear();
}

void ClearGroundWear(void) {
    memset(wearGrid, 0, sizeof(wearGrid));
    wearRecoveryAccum = 0.0f;
}

void TrampleGround(int x, int y, int z) {
    if (!groundWearEnabled) return;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return;
    if (z < 0 || z >= gridDepth) return;
    
    // Trample saplings - only destroy after significant wear accumulates
    CellType cell = grid[z][y][x];
    if (cell == CELL_SAPLING) {
        // Saplings need heavy traffic to be trampled, not just one pass
        // Use the wear grid to track trampling damage to the sapling
        int wear = wearGrid[z][y][x];
        wearGrid[z][y][x] = (wear < wearMax) ? wear + 1 : wearMax;
        
        // Only destroy sapling after significant wear (half of max)
        if (wearGrid[z][y][x] >= wearMax / 2) {
            grid[z][y][x] = CELL_AIR;
            treeTypeGrid[z][y][x] = TREE_TYPE_NONE;
            treePartGrid[z][y][x] = TREE_PART_NONE;
            MarkChunkDirty(x, y, z);
            wearGrid[z][y][x] = 0;  // Reset wear
        }
        return;  // Don't also trample ground below sapling
    }
    
    // Check current cell for dirt
    int targetZ = z;
    
    // In DF mode, movers walk at z=1 on floors above z=0 dirt
    // If current cell isn't dirt, check if we're standing on dirt below
    if (cell != CELL_DIRT && z > 0) {
        CellType cellBelow = grid[z - 1][y][x];
        if (cellBelow == CELL_DIRT) {
            targetZ = z - 1;
        } else {
            return;  // No dirt to trample
        }
    } else if (cell != CELL_DIRT) {
        return;  // No dirt to trample
    }
    
    // Increase wear (cap at wearMax)
    int oldWear = wearGrid[targetZ][y][x];
    int newWear = oldWear + wearTrampleAmount;
    if (newWear > wearMax) newWear = wearMax;
    wearGrid[targetZ][y][x] = newWear;
    
    // Track active worn cells
    if (oldWear == 0 && newWear > 0) {
        wearActiveCells++;
    }
    
    // Update surface overlay based on new wear
    UpdateSurfaceFromWear(x, y, targetZ);
}

void UpdateGroundWear(void) {
    if (!groundWearEnabled) return;
    
    // Early exit: no worn cells and sapling regrowth disabled
    if (wearActiveCells == 0 && !saplingRegrowthEnabled) {
        return;
    }
    
    // Accumulate game time for interval-based decay
    wearRecoveryAccum += gameDeltaTime;
    
    // Only decay wear when interval elapses
    if (wearRecoveryAccum < wearRecoveryInterval) return;
    wearRecoveryAccum -= wearRecoveryInterval;
    
    // Process all cells at all z-levels
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];
                
                // Only process soil tiles
                if (!IsGroundCell(cell)) continue;
                bool isDirt = (cell == CELL_DIRT);
                
                // Skip if on fire - don't regrow grass while burning
                if (HasFire(x, y, z)) continue;
                
                if (isDirt) {
                    // Decay wear
                    int oldWear = wearGrid[z][y][x];
                    if (oldWear > wearDecayRate) {
                        wearGrid[z][y][x] = oldWear - wearDecayRate;
                    } else if (oldWear > 0) {
                        wearGrid[z][y][x] = 0;
                        wearActiveCells--;
                    }

                    // Update surface overlay based on new wear
                    UpdateSurfaceFromWear(x, y, z);
                }
                
                // Sapling regrowth: spawn sapling on tall grass with some chance
                if (saplingRegrowthEnabled && wearGrid[z][y][x] == 0) {
                    // For dirt, require fully recovered grass; for other soils, just require no wear
                    if (!isDirt || GET_CELL_SURFACE(x, y, z) == SURFACE_TALL_GRASS) {
                        if (z + 1 < gridDepth && grid[z + 1][y][x] == CELL_AIR) {
                            if (QueryItemAtTile(x, y, z + 1) >= 0) continue;
                            if ((rand() % 10000) < saplingRegrowthChance) {
                                if (!HasNearbyTree(x, y, z, saplingMinTreeDistance)) {
                                    TreeType type = PickTreeTypeForSoil(grid[z][y][x]);
                                    PlaceSapling(x, y, z + 1, type);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

int GetGroundWear(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return 0;
    if (z < 0 || z >= gridDepth) return 0;
    return wearGrid[z][y][x];
}
