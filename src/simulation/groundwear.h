#ifndef GROUNDWEAR_H
#define GROUNDWEAR_H

#include <stdbool.h>
#include "../world/grid.h"

// Ground wear system - creates emergent paths
// Grass becomes dirt when trampled, dirt becomes grass when left alone

// Wear thresholds - scaled 10x to allow fractional trample amounts
#define WEAR_TALL_TO_NORMAL_DEFAULT 200  // Above this, tall grass becomes normal grass
#define WEAR_NORMAL_TO_TRAMPLED_DEFAULT 600  // Above this, normal grass becomes trampled
#define WEAR_GRASS_TO_DIRT_DEFAULT 1000  // Above this, grass becomes bare dirt
#define WEAR_DIRT_TO_GRASS_DEFAULT 0     // Below this, dirt becomes grass
#define WEAR_MAX_DEFAULT 3000            // Maximum wear value (set when burned)

// Wear change rates - now variables for UI tweaking
#define WEAR_TRAMPLE_AMOUNT_DEFAULT 1    // Added when a mover walks on tile (1 = 0.1 effective)
#define WEAR_DECAY_RATE_DEFAULT 10       // Subtracted per decay interval (natural recovery)

// Runtime configurable values
extern int wearTallToNormal;     // Threshold for tall grass -> normal grass
extern int wearNormalToTrampled; // Threshold for normal grass -> trampled
extern int wearGrassToDirt;      // Threshold to turn grass into bare dirt
extern int wearDirtToGrass;      // Threshold to turn dirt back to grass  
extern int wearTrampleAmount;    // Wear added per mover step
extern int wearDecayRate;        // Wear removed per decay interval
extern float wearRecoveryInterval; // Game-seconds between decay updates
extern int wearMax;              // Maximum wear value

// Wear grid (parallel to main grid, all z-levels)
extern int wearGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool groundWearEnabled;

// Initialize ground wear system
void InitGroundWear(void);

// Clear all wear values
void ClearGroundWear(void);

// Called when a mover steps on a tile - increases wear
void TrampleGround(int x, int y, int z);

// Update wear decay and grass/dirt conversion (call from main tick)
void UpdateGroundWear(void);

// Get current wear value at position
int GetGroundWear(int x, int y, int z);

#endif // GROUNDWEAR_H
