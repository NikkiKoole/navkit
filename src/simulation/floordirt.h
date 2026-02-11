#ifndef FLOORDIRT_H
#define FLOORDIRT_H

#include <stdbool.h>
#include "../world/grid.h"

// Floor dirt thresholds (uint8_t range: 0-255)
#define DIRT_VISIBLE_THRESHOLD  10   // Overlay starts rendering above this
#define DIRT_CLEAN_THRESHOLD    30   // Cleaners target tiles above this
#define DIRT_MAX               255   // uint8_t max

// Dirt tracking amounts
#define DIRT_TRACK_AMOUNT        2   // Base dirt per step from soil to floor
#define DIRT_STONE_MULTIPLIER   50   // Stone floors: 50% rate (value / 100)
#define DIRT_CLEAN_AMOUNT       50   // Dirt removed per completed clean job

// Grid storage (parallel to main grid)
extern uint8_t floorDirtGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool floorDirtEnabled;

// Initialize floor dirt system
void InitFloorDirt(void);

// Clear all dirt values
void ClearFloorDirt(void);

// Called from mover movement hook with mover index and current cell
// Tracks previous cell internally; adds dirt when stepping from soil to floor
void MoverTrackDirt(int moverIdx, int cellX, int cellY, int cellZ);

// Reset per-mover tracking (call from ClearMovers)
void ResetMoverDirtTracking(void);

// Accessors
int  GetFloorDirt(int x, int y, int z);
void SetFloorDirt(int x, int y, int z, int value);

// Cleaning: reduces dirt by amount, returns new value
int  CleanFloorDirt(int x, int y, int z, int amount);

// Query helpers (exposed for tests)
bool IsDirtSource(int x, int y, int z);
bool IsDirtTarget(int x, int y, int z);

#endif // FLOORDIRT_H
