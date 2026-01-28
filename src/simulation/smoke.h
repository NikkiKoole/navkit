#ifndef SMOKE_H
#define SMOKE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// Smoke level constants (1-7 scale like water/fire)
#define SMOKE_MAX_LEVEL 7

// Performance tuning
#define SMOKE_MAX_UPDATES_PER_TICK 16384*4
#define SMOKE_PRESSURE_SEARCH_LIMIT 64  // Max cells to search for pressure fill

// Smoke cell data (parallel to grid)
typedef struct {
    uint8_t level         : 3;   // 0-7 smoke density (0 = no smoke)
    uint8_t stable        : 1;   // true = skip processing
    uint8_t hasPressure   : 1;   // true = trapped smoke that wants to escape
    uint8_t pressureSourceZ : 3; // z-level where smoke originated (can fill down to here)
} SmokeCell;

// Smoke grid (same dimensions as main grid)
extern SmokeCell smokeGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool smokeEnabled;           // Master toggle for smoke simulation
extern int smokeUpdateCount;        // Cells updated last tick (for debug/profiling)

// Tweakable parameters (game-time based)
extern float smokeRiseInterval;     // Game-seconds between rise attempts
extern float smokeDissipationTime;  // Game-seconds for smoke to fully dissipate (per level)
extern int smokeGenerationRate;     // Smoke generated per fire level (ratio, not time-based)

// Initialize smoke system
void InitSmoke(void);

// Clear all smoke
void ClearSmoke(void);

// Reset accumulators (call after loading smoke grid from save)
void ResetSmokeAccumulators(void);

// Main simulation tick
void UpdateSmoke(void);

// Place/remove smoke
void SetSmokeLevel(int x, int y, int z, int level);
void AddSmoke(int x, int y, int z, int amount);

// Query
int GetSmokeLevel(int x, int y, int z);
bool HasSmoke(int x, int y, int z);

// Mark cell and neighbors as unstable
void DestabilizeSmoke(int x, int y, int z);

// Generate smoke from fire (called by fire system)
void GenerateSmokeFromFire(int x, int y, int z, int fireLevel);

#endif // SMOKE_H
