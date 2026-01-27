#ifndef FIRE_H
#define FIRE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// Fire level constants (1-7 scale like water)
#define FIRE_MAX_LEVEL 7
#define FIRE_MIN_SPREAD_LEVEL 2  // Minimum level to spread to neighbors

// Fuel constants
#define FUEL_GRASS 3
#define FUEL_DIRT  1
#define FUEL_NONE  0

// Performance tuning
#define FIRE_MAX_UPDATES_PER_TICK 4096

// Fire cell data (parallel to grid)
typedef struct {
    uint16_t level    : 3;   // 0-7 fire intensity (0 = no fire)
    uint16_t stable   : 1;   // true = skip processing
    uint16_t isSource : 1;   // true = permanent fire (torch/lava), never runs out
    uint16_t fuel     : 4;   // 0-15 remaining fuel in this cell
    // 7 bits spare
} FireCell;

// Fire grid (same dimensions as main grid)
extern FireCell fireGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool fireEnabled;            // Master toggle for fire simulation
extern int fireUpdateCount;         // Cells updated last tick (for debug/profiling)

// Tweakable parameters
extern int fireSpreadChance;        // 1 in N chance to spread per tick (lower = faster)
extern int fireFuelConsumption;     // Fuel consumed per N ticks
extern int fireWaterReduction;      // Spread chance multiplier near water (percentage, 0-100)

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

// Mark cell and neighbors as unstable
void DestabilizeFire(int x, int y, int z);

// Get fuel amount for a cell type
int GetBaseFuelForCellType(CellType cell);

#endif // FIRE_H
