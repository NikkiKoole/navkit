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
uint16_t* wearGrid = NULL;   // allocated in grid_layers.c

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
// Rows (z * height + y) of the current interval's sweep already processed. Always
// WearRowsDue(wearRecoveryAccum), so it isn't saved: the load recomputes it.
static int wearSweepRow = 0;

// Map wear value to the vegetation level it implies
static VegetationType VegLevelForWear(int wear) {
    if (wear >= wearGrassToDirt)      return VEG_NONE;
    if (wear >= wearNormalToTrampled) return VEG_NONE;
    if (wear >= wearTallToNormal)     return VEG_GRASS_SHORT;
    if (wear >= wearTallerToTall)     return VEG_GRASS_TALL;
    return VEG_GRASS_TALLER;
}

// Next vegetation level in the wear recovery sequence (skips VEG_GRASS)
static VegetationType NextWearVegLevel(VegetationType current) {
    switch (current) {
        case VEG_NONE:        return VEG_GRASS_SHORT;
        case VEG_GRASS_SHORT: return VEG_GRASS_TALL;
        case VEG_GRASS_TALL:  return VEG_GRASS_TALLER;
        default:              return current;
    }
}

// Update vegetation and surface based on current wear value.
// recovering=true: wear is decaying, vegetation can increase back.
// recovering=false: trampling, vegetation can only decrease (never upgrade bare dirt to grass).
static void UpdateSurfaceFromWear(int x, int y, int z, bool recovering) {
    int wear = WEAR(x, y, z);
    VegetationType target = VegLevelForWear(wear);
    VegetationType current = GetVegetation(x, y, z);

    // Surface overlay
    if (wear >= wearGrassToDirt) {
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
    } else if (wear >= wearNormalToTrampled) {
        SET_CELL_SURFACE(x, y, z, SURFACE_TRAMPLED);
    } else {
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
    }

    // Vegetation: when trampling, only decrease; when recovering, grow one step at a time.
    // HAD_VEGETATION flag tracks cells that once had grass — allows recovery from VEG_NONE.
    // Intentionally bare dirt (flag not set) stays bare forever.
    if (recovering) {
        if (target > current) {
            if (current > VEG_NONE) {
                // Gradual regrowth: SHORT → TALL → TALLER, one step per tick
                SetVegetation(x, y, z, NextWearVegLevel(current));
            } else if (HAS_CELL_FLAG(x, y, z, CELL_FLAG_HAD_VEGETATION)) {
                // Was grassy before trampling destroyed it — start regrowing
                SetVegetation(x, y, z, VEG_GRASS_SHORT);
                CLEAR_CELL_FLAG(x, y, z, CELL_FLAG_HAD_VEGETATION);
            }
        } else if (target < current) {
            SetVegetation(x, y, z, target);
        }
    } else {
        // Trampling: can only decrease vegetation
        if (target < current) {
            // Remember that this cell had vegetation before trampling killed it
            if (current > VEG_NONE && target == VEG_NONE) {
                SET_CELL_FLAG(x, y, z, CELL_FLAG_HAD_VEGETATION);
            }
            SetVegetation(x, y, z, target);
        }
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
                
                CellType c = GRID(nx, ny, checkZ);
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
    CLEAR_GRID_LAYER(wearGrid, 0);
    wearRecoveryAccum = 0.0f;
    wearSweepRow = 0;
}

void TrampleGround(int x, int y, int z) {
    if (!groundWearEnabled) return;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return;
    if (z < 0 || z >= gridDepth) return;
    
    // Trample saplings - only destroy after significant wear accumulates
    CellType cell = GRID(x, y, z);
    if (cell == CELL_SAPLING) {
        // Saplings need heavy traffic to be trampled, not just one pass
        // Use the wear grid to track trampling damage to the sapling
        int wear = WEAR(x, y, z);
        WEAR(x, y, z) = (wear < WearCap()) ? wear + 1 : WearCap();
        
        // Only destroy sapling after significant wear (half of max)
        if (WEAR(x, y, z) >= wearMax / 2) {
            GRID(x, y, z) = CELL_AIR;
            SetWallMaterial(x, y, z, MAT_NONE);
            MarkChunkDirty(x, y, z);
            WEAR(x, y, z) = 0;  // Reset wear
        }
        return;  // Don't also trample ground below sapling
    }
    
    // Check current cell for dirt terrain
    int targetZ = z;
    
    // In DF mode, movers walk at z=1 on floors above z=0 dirt terrain
    // If current cell isn't dirt terrain, check if we're standing on dirt below
    if (!(CellIsSolid(cell) && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT) && z > 0) {
        CellType cellBelow = GRID(x, y, z - 1);
        if (CellIsSolid(cellBelow) && IsWallNatural(x, y, z - 1) && GetWallMaterial(x, y, z - 1) == MAT_DIRT) {
            targetZ = z - 1;
        } else {
            return;  // No dirt to trample
        }
    } else if (!(CellIsSolid(cell) && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT)) {
        return;  // No dirt to trample
    }
    
    // Increase wear (cap at wearMax)
    int oldWear = WEAR(x, y, targetZ);
    int newWear = oldWear + wearTrampleAmount;
    if (newWear > WearCap()) newWear = WearCap();
    WEAR(x, y, targetZ) = newWear;
    
    // Track active worn cells
    if (oldWear == 0 && newWear > 0) {
        wearActiveCells++;
    }
    
    // Update surface overlay — trampling can only decrease vegetation
    UpdateSurfaceFromWear(x, y, targetZ, false);
}

// One row of the wear sweep: decay, reed and sapling regrowth, drying
static void UpdateGroundWearRow(int y, int z, float vegRate) {
    // Per 16-cell run: fire and water are sparse (a block per 16x16 column), so a run
    // whose column has no fire block can't burn, and one with no water block in its
    // column or the 4 next to it can't have water beside a cell (most of a big map)
    for (int x0 = 0; x0 < gridWidth; x0 += STORE_CHUNK) {
        int x1 = x0 + STORE_CHUNK < gridWidth ? x0 + STORE_CHUNK : gridWidth;
        bool fireRun = SparseLayerHasBlock(&fireLayer, x0, y);
        bool waterRun = z + 1 < gridDepth && (SparseLayerHasBlock(&waterLayer, x0, y)
            || (x0 > 0 && SparseLayerHasBlock(&waterLayer, x0 - 1, y))
            || (x1 < gridWidth && SparseLayerHasBlock(&waterLayer, x1, y))
            || (y > 0 && SparseLayerHasBlock(&waterLayer, x0, y - 1))
            || (y < gridHeight - 1 && SparseLayerHasBlock(&waterLayer, x0, y + 1)));
        for (int x = x0, ci = CellIndex(x0, y, z); x < x1; x++, ci++) {
            CellType cell = LAYER_AT(grid, ci);
        
            // Only process natural dirt tiles
            if (!CellIsSolid(cell) || !LAYER_AT(wallNatural, ci)) continue;
            bool isDirt = (GetWallMaterial(x, y, z) == MAT_DIRT);
        
            // Skip if on fire - don't regrow grass while burning
            if (fireRun && HasFire(x, y, z)) continue;
        
            if (isDirt) {
                // Decay wear (modulated by seasonal vegetation growth)
                int effectiveDecay = (int)(wearDecayRate * vegRate);
                int oldWear = WEAR(x, y, z);
                if (effectiveDecay > 0 && oldWear > effectiveDecay) {
                    WEAR(x, y, z) = oldWear - effectiveDecay;
                    // Recovery — vegetation grows one step per tick
                    UpdateSurfaceFromWear(x, y, z, true);
                } else if (effectiveDecay > 0 && oldWear > 0) {
                    // Wear would decay to 0. If vegetation hasn't fully
                    // regrown, hold wear at 1 to keep recovery ticking.
                    VegetationType veg = GetVegetation(x, y, z);
                    WEAR(x, y, z) = 0;
                    UpdateSurfaceFromWear(x, y, z, true);
                    veg = GetVegetation(x, y, z);
                    if (veg > VEG_NONE && veg < VEG_GRASS_TALLER) {
                        WEAR(x, y, z) = 1;  // keep ticking
                    } else {
                        wearActiveCells--;
                    }
                }
            }

            // Reed regrowth: bare soil adjacent to water regrows reeds
            if (isDirt && GetVegetation(x, y, z) == VEG_NONE && WEAR(x, y, z) == 0) {
                bool hasAdjacentWater = false;
                if (waterRun) {
                    if (x > 0 && GetWaterLevel(x - 1, y, z + 1) > 0) hasAdjacentWater = true;
                    if (x < gridWidth - 1 && GetWaterLevel(x + 1, y, z + 1) > 0) hasAdjacentWater = true;
                    if (y > 0 && GetWaterLevel(x, y - 1, z + 1) > 0) hasAdjacentWater = true;
                    if (y < gridHeight - 1 && GetWaterLevel(x, y + 1, z + 1) > 0) hasAdjacentWater = true;
                }
                if (hasAdjacentWater) {
                    SetVegetation(x, y, z, VEG_REEDS);
                }
            }

            // Sapling regrowth: spawn sapling on tall grass with some chance
            if (saplingRegrowthEnabled && WEAR(x, y, z) == 0) {
                // For dirt, require fully recovered grass; for other soils, just require no wear
                if (!isDirt || GetVegetation(x, y, z) >= VEG_GRASS_TALL) {
                    if (z + 1 < gridDepth && GRID(x, y, z + 1) == CELL_AIR) {
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
        
            // (soil drying: moisture.c)
        }
    }
}

// Rows of the sweep due after `accum` game-seconds of the interval. Maps bigger than
// wearSpreadMinCells spread the sweep evenly over the interval instead of doing it in
// one tick (a whole 2048 map took ~360 ms); smaller maps sweep at the interval's end.
// Every cell is processed once per interval either way.
int wearSpreadMinCells = 1 << 20;   // ~5 ms of sweep
static int WearRowsDue(float accum, float interval) {
    int total = gridDepth * gridHeight;
    if (accum >= interval) return 0;   // (an interval shorter than a tick: whole sweeps)
    if ((long long)total * gridWidth <= wearSpreadMinCells) return 0;
    int due = (int)((double)total * accum / interval);
    return due < 0 ? 0 : due > total ? total : due;
}

void UpdateGroundWear(void) {
    if (!groundWearEnabled) return;
    
    // Early exit: nothing to process
    if (wearActiveCells == 0 && !saplingRegrowthEnabled && waterActiveCells == 0) {
        return;
    }
    
    // Accumulate game time for interval-based decay
    wearRecoveryAccum += gameDeltaTime;
    float wearIntervalGS = GameHoursToGameSeconds(wearRecoveryInterval);
    
    // Cache seasonal growth rate (uses cosf — don't call per-cell)
    float vegRate = GetVegetationGrowthRate();
    int total = gridDepth * gridHeight;
    if (wearSweepRow > total) wearSweepRow = total;

    // The rows due so far; when the interval ends, finish the sweep and start the next
    // (at most one full sweep per call, like the old all-at-once update)
    for (;;) {
        int due = wearRecoveryAccum >= wearIntervalGS ? total : WearRowsDue(wearRecoveryAccum, wearIntervalGS);
        for (int r = wearSweepRow; r < due; r++) UpdateGroundWearRow(r % gridHeight, r / gridHeight, vegRate);
        if (due > wearSweepRow) wearSweepRow = due;
        if (wearRecoveryAccum < wearIntervalGS) break;
        wearRecoveryAccum -= wearIntervalGS;
        wearSweepRow = 0;
        if (wearRecoveryAccum >= wearIntervalGS) break;
    }
}

int GetGroundWear(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return 0;
    if (z < 0 || z >= gridDepth) return 0;
    return WEAR(x, y, z);
}

float GetWearRecoveryAccum(void) { return wearRecoveryAccum; }
void SetWearRecoveryAccum(float v) {
    wearRecoveryAccum = v;
    wearSweepRow = WearRowsDue(v, GameHoursToGameSeconds(wearRecoveryInterval));
}
