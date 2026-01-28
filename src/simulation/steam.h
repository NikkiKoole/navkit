#ifndef STEAM_H
#define STEAM_H

#include <stdint.h>
#include <stdbool.h>
#include "../world/grid.h"

// Steam constants
#define STEAM_MAX_LEVEL 7
#define STEAM_MAX_UPDATES_PER_TICK 8192

// Steam cell data
// Note: Steam temperature uses the temperature grid, not stored here
typedef struct {
    uint8_t level : 3;        // 0-7 steam density
    uint8_t pressure : 3;     // 0-7 pressure level (Phase 2)
    uint8_t stable : 1;       // Optimization flag
    uint8_t reserved : 1;     // Future use
} SteamCell;

// Steam grid (same dimensions as main grid)
extern SteamCell steamGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool steamEnabled;
extern int steamUpdateCount;        // Cells updated last tick (for debug/profiling)

// Tweakable parameters (game-time based)
extern float steamRiseInterval;     // Game-seconds between rise attempts
extern int steamCondensationTemp;   // Celsius below which steam condenses (default: 60C)
extern int steamGenerationTemp;     // Celsius above which water boils (default: 100C)
extern int steamCondensationChance; // 1 in N ticks attempts condensation (default: 3)
extern int steamRiseFlow;           // Units of steam that rise per attempt (default: 1)

// Initialize/clear
void InitSteam(void);
void ClearSteam(void);
void ResetSteamAccumulators(void);

// Level operations
void SetSteamLevel(int x, int y, int z, int level);
void AddSteam(int x, int y, int z, int amount);
int GetSteamLevel(int x, int y, int z);
bool HasSteam(int x, int y, int z);

// Main update
void UpdateSteam(void);

// Called by water system when water boils
void GenerateSteamFromBoilingWater(int x, int y, int z, int amount);

// Stability
void DestabilizeSteam(int x, int y, int z);

#endif // STEAM_H
