#include "groundwear.h"
#include "../world/grid.h"
#include "../core/time.h"
#include "fire.h"
#include <string.h>

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
    
    // Check current cell for dirt
    CellType cell = grid[z][y][x];
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
    int newWear = wearGrid[targetZ][y][x] + wearTrampleAmount;
    if (newWear > wearMax) newWear = wearMax;
    wearGrid[targetZ][y][x] = newWear;
    
    // Update surface overlay based on new wear
    UpdateSurfaceFromWear(x, y, targetZ);
}

void UpdateGroundWear(void) {
    if (!groundWearEnabled) return;
    
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
                
                // Only process dirt tiles
                if (cell != CELL_DIRT) continue;
                
                // Skip if on fire - don't regrow grass while burning
                if (HasFire(x, y, z)) continue;
                
                // Decay wear
                if (wearGrid[z][y][x] > wearDecayRate) {
                    wearGrid[z][y][x] -= wearDecayRate;
                } else {
                    wearGrid[z][y][x] = 0;
                }
                
                // Update surface overlay based on new wear
                UpdateSurfaceFromWear(x, y, z);
            }
        }
    }
}

int GetGroundWear(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return 0;
    if (z < 0 || z >= gridDepth) return 0;
    return wearGrid[z][y][x];
}
