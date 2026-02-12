#ifndef FIRE_H
#define FIRE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// Fire level constants (1-7 scale like water)
#define FIRE_MAX_LEVEL 7
#define FIRE_MIN_SPREAD_LEVEL 2  // Minimum level to spread to neighbors

// Performance tuning
#define FIRE_MAX_UPDATES_PER_TICK 4096*4

// Fire cell data (parallel to grid)
typedef struct {
    uint16_t level    : 3;   // 0-7 fire intensity (0 = no fire)
    uint16_t stable   : 1;   // true = skip processing
    uint16_t isSource : 1;   // true = permanent fire (torch/lava), never runs out
    uint16_t fuel     : 8;   // 0-255 remaining fuel in this cell
} FireCell;

// Fire grid (same dimensions as main grid)
extern FireCell fireGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool fireEnabled;            // Master toggle for fire simulation
extern int fireUpdateCount;         // Cells updated last tick (for debug/profiling)

// Tweakable parameters (game-time based)
extern float fireSpreadInterval;    // Seconds between spread attempts
extern float fireFuelInterval;      // Seconds between fuel consumption
extern int fireWaterReduction;      // Spread chance multiplier near water (percentage, 0-100)

// Fire spread chance formula: spreadPercent = fireSpreadBase + (level * fireSpreadPerLevel)
// At level 2 (min): spreadBase + 2*perLevel, at level 7 (max): spreadBase + 7*perLevel
extern int fireSpreadBase;          // Base spread chance percentage (default: 10)
extern int fireSpreadPerLevel;      // Additional spread chance per fire level (default: 10)

// Initialize fire system
void InitFire(void);

// Clear all fire
void ClearFire(void);

// Main simulation tick
void UpdateFire(void);

// Place/remove fire
void SetFireLevel(int x, int y, int z, int level);
void IgniteCell(int x, int y, int z);  // Ignite at max level if cell has fuel
void ExtinguishCell(int x, int y, int z);

// Source management
void SetFireSource(int x, int y, int z, bool isSource);

// Query
int GetFireLevel(int x, int y, int z);
bool HasFire(int x, int y, int z);
int GetCellFuel(int x, int y, int z);  // Get remaining fuel
bool IsFireSourceAt(int x, int y, int z);

// Rebuild fire-based light sources from current fire grid (call after loading save)
void SyncFireLighting(void);

// Mark cell and neighbors as unstable
void DestabilizeFire(int x, int y, int z);

// Get fuel amount for a cell type
int GetBaseFuelForCellType(CellType cell);

// Get fuel at position considering materials, floors, and surfaces
int GetFuelAt(int x, int y, int z);

// Accumulator getters/setters (for save/load)
float GetFireSpreadAccum(void);
float GetFireFuelAccum(void);
void SetFireSpreadAccum(float v);
void SetFireFuelAccum(float v);

#endif // FIRE_H
