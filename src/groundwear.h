#ifndef GROUNDWEAR_H
#define GROUNDWEAR_H

#include <stdbool.h>
#include "grid.h"

// Ground wear system - creates emergent paths
// Grass becomes dirt when trampled, dirt becomes grass when left alone

// Wear thresholds - now variables for UI tweaking
#define WEAR_GRASS_TO_DIRT_DEFAULT 1000  // Above this, grass becomes dirt
#define WEAR_DIRT_TO_GRASS_DEFAULT 500   // Below this, dirt becomes grass
#define WEAR_MAX_DEFAULT 10000           // Maximum wear value

// Wear change rates - now variables for UI tweaking
#define WEAR_TRAMPLE_AMOUNT_DEFAULT 1    // Added when a mover walks on tile (low = needs many passes)
#define WEAR_DECAY_RATE_DEFAULT 1        // Subtracted per decay tick (natural recovery)
#define WEAR_DECAY_INTERVAL_DEFAULT 50   // Only decay every N ticks (higher = slower recovery)

// Runtime configurable values
extern int wearGrassToDirt;      // Threshold to turn grass into dirt
extern int wearDirtToGrass;      // Threshold to turn dirt back to grass  
extern int wearTrampleAmount;    // Wear added per mover step
extern int wearDecayRate;        // Wear removed per decay tick
extern int wearDecayInterval;    // Ticks between decay updates
extern int wearMax;              // Maximum wear value

// Wear grid (parallel to main grid, only z=0 for now)
extern int wearGrid[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool groundWearEnabled;
extern int groundWearTickCounter;

// Initialize ground wear system
void InitGroundWear(void);

// Clear all wear values
void ClearGroundWear(void);

// Called when a mover steps on a tile - increases wear
void TrampleGround(int x, int y);

// Update wear decay and grass/dirt conversion (call from main tick)
void UpdateGroundWear(void);

// Get current wear value at position
int GetGroundWear(int x, int y);

#endif // GROUNDWEAR_H
