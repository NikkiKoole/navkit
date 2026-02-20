#include "groundwear.h"
#include "weather.h"
#include "balance.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/material.h"
#include "../core/time.h"
#include "fire.h"
#include "water.h"
#include "trees.h"
#include "../entities/items.h"
#include <string.h>
#include <stdlib.h>

// Wear grid storage (3D)
int wearGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool groundWearEnabled = true;

// Runtime configurable values
int wearTallerToTall = WEAR_TALLER_TO_TALL_DEFAULT;
int wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
int wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
int wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
int wearDirtToGrass = WEAR_DIRT_TO_GRASS_DEFAULT;
int wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
int wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
float wearRecoveryInterval = 2.0f;  // Decay wear every 2.0 game-hours

// Sapling regrowth settings
bool saplingRegrowthEnabled = false;
int saplingRegrowthChance = 5;      // 5 per 10000 = 0.05% per interval per tile
int saplingMinTreeDistance = 4;     // Min 4 tiles from existing trees/saplings
int wearMax = WEAR_MAX_DEFAULT;

// Internal accumulator for game-time
static float wearRecoveryAccum = 0.0f;

// Update vegetation and surface based on current wear value
static void UpdateSurfaceFromWear(int x, int y, int z) {
    int wear = wearGrid[z][y][x];
    if (wear >= wearGrassToDirt) {
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
        SetVegetation(x, y, z, VEG_NONE);
    } else if (wear >= wearNormalToTrampled) {
        SET_CELL_SURFACE(x, y, z, SURFACE_TRAMPLED);
        SetVegetation(x, y, z, VEG_NONE);
    } else if (wear >= wearTallToNormal) {
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
        SetVegetation(x, y, z, VEG_GRASS_SHORT);
    } else if (wear >= wearTallerToTall) {
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
        SetVegetation(x, y, z, VEG_GRASS_TALL);
    } else {
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
        SetVegetation(x, y, z, VEG_GRASS_TALLER);
    }
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

static MaterialType PickTreeTypeForSoil(MaterialType soilMat) {
    switch (soilMat) {
        case MAT_PEAT: return MAT_WILLOW;
        case MAT_SAND: return MAT_BIRCH;
        case MAT_GRAVEL: return MAT_PINE;
        case MAT_CLAY: return MAT_OAK;
        case MAT_DIRT:
        default: return MAT_OAK;
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
            SetWallMaterial(x, y, z, MAT_NONE);
            MarkChunkDirty(x, y, z);
            wearGrid[z][y][x] = 0;  // Reset wear
        }
        return;  // Don't also trample ground below sapling
    }
    
    // Check current cell for dirt terrain
    int targetZ = z;
    
    // In DF mode, movers walk at z=1 on floors above z=0 dirt terrain
    // If current cell isn't dirt terrain, check if we're standing on dirt below
    if (!(CellIsSolid(cell) && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT) && z > 0) {
        CellType cellBelow = grid[z - 1][y][x];
        if (CellIsSolid(cellBelow) && IsWallNatural(x, y, z - 1) && GetWallMaterial(x, y, z - 1) == MAT_DIRT) {
            targetZ = z - 1;
        } else {
            return;  // No dirt to trample
        }
    } else if (!(CellIsSolid(cell) && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT)) {
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
    
    // Early exit: nothing to process
    if (wearActiveCells == 0 && !saplingRegrowthEnabled && waterActiveCells == 0) {
        return;
    }
    
    // Accumulate game time for interval-based decay
    wearRecoveryAccum += gameDeltaTime;
    
    // Only decay wear when interval elapses
    float wearIntervalGS = GameHoursToGameSeconds(wearRecoveryInterval);
    if (wearRecoveryAccum < wearIntervalGS) return;
    wearRecoveryAccum -= wearIntervalGS;
    
    // Cache seasonal growth rate (uses cosf — don't call per-cell)
    float vegRate = GetVegetationGrowthRate();

    // Process all cells at all z-levels
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];
                
                // Only process natural dirt tiles
                if (!CellIsSolid(cell) || !IsWallNatural(x, y, z)) continue;
                bool isDirt = (GetWallMaterial(x, y, z) == MAT_DIRT);
                
                // Skip if on fire - don't regrow grass while burning
                if (HasFire(x, y, z)) continue;
                
                if (isDirt) {
                    // Decay wear (modulated by seasonal vegetation growth)
                    int effectiveDecay = (int)(wearDecayRate * vegRate);
                    int oldWear = wearGrid[z][y][x];
                    if (effectiveDecay > 0 && oldWear > effectiveDecay) {
                        wearGrid[z][y][x] = oldWear - effectiveDecay;
                    } else if (effectiveDecay > 0 && oldWear > 0) {
                        wearGrid[z][y][x] = 0;
                        wearActiveCells--;
                    }

                    // Update surface overlay based on new wear
                    UpdateSurfaceFromWear(x, y, z);
                }
                
                // Sapling regrowth: spawn sapling on tall grass with some chance
                if (saplingRegrowthEnabled && wearGrid[z][y][x] == 0) {
                    // For dirt, require fully recovered grass; for other soils, just require no wear
                    if (!isDirt || GetVegetation(x, y, z) >= VEG_GRASS_TALL) {
                        if (z + 1 < gridDepth && grid[z + 1][y][x] == CELL_AIR) {
                            if (QueryItemAtTile(x, y, z + 1) >= 0) continue;
                            if ((rand() % 10000) < saplingRegrowthChance) {
                                if (!HasNearbyTree(x, y, z, saplingMinTreeDistance)) {
                                    MaterialType soilMat = GetWallMaterial(x, y, z);
                                    MaterialType treeMat = PickTreeTypeForSoil(soilMat);
                                    PlaceSapling(x, y, z + 1, treeMat);
                                }
                            }
                        }
                    }
                }
                
                // Wetness drying: decrease wetness on natural soil cells
                // Only dry if no water is sitting on or above this cell
                // Random chance per cell prevents all mud vanishing in the same tick
                int wetness = GET_CELL_WETNESS(x, y, z);
                if (wetness > 0 && IsSoilMaterial(GetWallMaterial(x, y, z))) {
                    bool waterPresent = (z + 1 < gridDepth && GetWaterLevel(x, y, z + 1) > 0)
                                     || GetWaterLevel(x, y, z) > 0;
                    if (!waterPresent) {
                        // 50% chance to dry — desynchronizes cells so mud fades gradually
                        if (rand() % 100 < 50) {
                            SET_CELL_WETNESS(x, y, z, wetness - 1);
                        }

                        // Wind drying: exposed cells with wind have additional chance to dry
                        if (weatherState.windStrength > 0.5f && IsExposedToSky(x, y, z)) {
                            int currentWetness = GET_CELL_WETNESS(x, y, z);
                            if (currentWetness > 0 && (rand() % 100) < (int)(windDryingMultiplier * 10.0f)) {
                                SET_CELL_WETNESS(x, y, z, currentWetness - 1);
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

float GetWearRecoveryAccum(void) { return wearRecoveryAccum; }
void SetWearRecoveryAccum(float v) { wearRecoveryAccum = v; }
