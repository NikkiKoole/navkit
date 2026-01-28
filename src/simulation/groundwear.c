#include "groundwear.h"
#include "../world/grid.h"
#include "fire.h"
#include <string.h>

// Wear grid storage
int wearGrid[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool groundWearEnabled = true;
int groundWearTickCounter = 0;

// Runtime configurable values
int wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
int wearDirtToGrass = WEAR_DIRT_TO_GRASS_DEFAULT;
int wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
int wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
int wearDecayInterval = WEAR_DECAY_INTERVAL_DEFAULT;
int wearMax = WEAR_MAX_DEFAULT;

void InitGroundWear(void) {
    ClearGroundWear();
}

void ClearGroundWear(void) {
    memset(wearGrid, 0, sizeof(wearGrid));
    groundWearTickCounter = 0;
}

void TrampleGround(int x, int y) {
    if (!groundWearEnabled) return;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return;
    
    // Only affect grass and dirt tiles at z=0
    CellType cell = grid[0][y][x];
    if (cell != CELL_WALKABLE && cell != CELL_GRASS && cell != CELL_DIRT) return;
    
    // Increase wear (cap at wearMax)
    int newWear = wearGrid[y][x] + wearTrampleAmount;
    if (newWear > wearMax) newWear = wearMax;
    wearGrid[y][x] = newWear;
    
    // Check if grass should become dirt
    if ((cell == CELL_WALKABLE || cell == CELL_GRASS) && wearGrid[y][x] >= wearGrassToDirt) {
        grid[0][y][x] = CELL_DIRT;
    }
}

void UpdateGroundWear(void) {
    if (!groundWearEnabled) return;
    
    groundWearTickCounter++;
    
    // Only decay wear every N ticks (makes recovery slower than trampling)
    if (groundWearTickCounter < wearDecayInterval) return;
    groundWearTickCounter = 0;
    
    // Process all cells
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            CellType cell = grid[0][y][x];
            
            // Only process grass/dirt tiles
            if (cell != CELL_WALKABLE && cell != CELL_GRASS && cell != CELL_DIRT) continue;
            
            // Decay wear
            if (wearGrid[y][x] > wearDecayRate) {
                wearGrid[y][x] -= wearDecayRate;
            } else {
                wearGrid[y][x] = 0;
            }
            
            // Check if dirt should become grass (but not while on fire)
            if (cell == CELL_DIRT && wearGrid[y][x] <= wearDirtToGrass && !HasFire(x, y, 0)) {
                grid[0][y][x] = CELL_GRASS;
                CLEAR_CELL_FLAG(x, y, 0, CELL_FLAG_BURNED);
            }
        }
    }
}

int GetGroundWear(int x, int y) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return 0;
    return wearGrid[y][x];
}
