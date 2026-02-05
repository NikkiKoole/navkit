#ifndef WATER_H
#define WATER_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// Water level constants (DF-style: 1-7 scale)
#define WATER_MAX_LEVEL 7           // Maximum water depth per cell (7/7 = full)
#define WATER_MIN_FLOW 1            // Minimum level difference to trigger spread
#define WATER_BLOCKS_MOVEMENT 4     // Water level that blocks walking (movers can wade 1-3)

// Pressure settings
#define WATER_PRESSURE_SEARCH_LIMIT 64  // Max cells to search when tracing pressure

// Evaporation (default: shallow water evaporates every 10 game-seconds)
#define WATER_EVAP_INTERVAL_DEFAULT 10.0f

// Performance tuning
#define WATER_MAX_UPDATES_PER_TICK 4096  // Cap cells processed per tick

// Water cell data (parallel to grid)
// Packed into 2 bytes for memory efficiency:
//   level:          3 bits (0-7)
//   stable:         1 bit
//   isSource:       1 bit
//   isDrain:        1 bit
//   hasPressure:    1 bit
//   pressureSourceZ: 4 bits (0-15)
//   isFrozen:       1 bit
//   (2 bits spare)
typedef struct {
    uint16_t level          : 3;   // 0-7 water depth (0 = dry, 7 = full)
    uint16_t stable         : 1;   // true = skip processing (no recent changes)
    uint16_t isSource       : 1;   // true = refills to max each tick (also generates pressure)
    uint16_t isDrain        : 1;   // true = removes water each tick
    uint16_t hasPressure    : 1;   // true = this water was placed under pressure (can push up)
    uint16_t pressureSourceZ: 4;   // z-level of the pressure source (water can rise to sourceZ - 1)
    uint16_t isFrozen       : 1;   // true = water is frozen (blocks flow, can be harvested as ice)
} WaterCell;

// Water grid (same dimensions as main grid)
extern WaterCell waterGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool waterEnabled;           // Master toggle for water simulation
extern bool waterEvaporationEnabled; // Toggle evaporation (disable for testing)
extern float waterEvapInterval;     // Game-seconds between evaporation attempts for level-1 water
extern int waterUpdateCount;        // Cells updated last tick (for debug/profiling)

// Speed multipliers for movers walking through water (lower = slower)
extern float waterSpeedShallow;     // Speed in level 1-2 water (default: 0.85 = 15% slowdown)
extern float waterSpeedMedium;      // Speed in level 3-4 water (default: 0.6 = 40% slowdown)
extern float waterSpeedDeep;        // Speed in level 5-7 water (default: 0.35 = 65% slowdown)

// Initialize water system (call after grid is initialized)
void InitWater(void);

// Clear all water
void ClearWater(void);

// Main simulation tick (call from Tick())
void UpdateWater(void);

// Place/remove water
void SetWaterLevel(int x, int y, int z, int level);
void AddWater(int x, int y, int z, int amount);
void RemoveWater(int x, int y, int z, int amount);

// Source/drain management
void SetWaterSource(int x, int y, int z, bool isSource);
void SetWaterDrain(int x, int y, int z, bool isDrain);

// Query
int GetWaterLevel(int x, int y, int z);
bool HasWater(int x, int y, int z);
bool IsUnderwater(int x, int y, int z, int minDepth);  // level >= minDepth
bool IsFull(int x, int y, int z);                      // level == 7
bool IsWaterSourceAt(int x, int y, int z);
bool IsWaterDrainAt(int x, int y, int z);

// Freezing
bool IsWaterFrozen(int x, int y, int z);               // Check if water is frozen
void FreezeWater(int x, int y, int z);                 // Freeze water at cell
void ThawWater(int x, int y, int z);                   // Thaw frozen water
void UpdateWaterFreezing(void);                        // Check temps and freeze/thaw (call after UpdateTemperature)

// Mark cell and neighbors as unstable (needs processing)
void DestabilizeWater(int x, int y, int z);

// Displace water from a cell (push to neighbors/up, then clear)
// Call before placing a wall or other fluid-blocking structure
void DisplaceWater(int x, int y, int z);

// Speed multiplier for movers walking through water
// Returns 1.0 for no water, lower values for deeper water
float GetWaterSpeedMultiplier(int x, int y, int z);

// Water cell state getters (inline for zero overhead)
static inline bool IsWaterStable(int x, int y, int z) { return waterGrid[z][y][x].stable; }
static inline bool HasWaterPressure(int x, int y, int z) { return waterGrid[z][y][x].hasPressure; }

#endif // WATER_H
