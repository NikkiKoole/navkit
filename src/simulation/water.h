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

// Evaporation (default: shallow water evaporates every 4.0 game-hours)
#define WATER_EVAP_INTERVAL_DEFAULT 4.0f

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
//   pressureDirty:  1 bit
//   (1 bit spare)
typedef struct {
    uint16_t level          : 3;   // 0-7 water depth (0 = dry, 7 = full)
    uint16_t stable         : 1;   // true = skip processing (no recent changes)
    uint16_t isSource       : 1;   // true = refills to max each tick (also generates pressure)
    uint16_t isDrain        : 1;   // true = removes water each tick
    uint16_t hasPressure    : 1;   // true = this water was placed under pressure (can push up)
    uint16_t pressureSourceZ: 4;   // z-level of the pressure source (water can rise to sourceZ - 1)
    uint16_t isFrozen       : 1;   // true = water is frozen (blocks flow, can be harvested as ice)
    uint16_t pressureDirty  : 1;   // true = pressure BFS needs to run (water moved nearby)
    uint16_t isSea          : 1;   // part of the sea body: its level follows the tide (tide.c)
    uint16_t isSurf         : 1;   // beach one level above the sea: high-tide swash set by tide.c, doesn't flow
} WaterCell;

// Water grid (same dimensions as main grid)
// Sparse (grid.h SparseLayer): memory only in chunks that hold water or a non-empty water state.
// WATER(x, y, z) is read-only (an empty cell reads as level 0, stable); write with WATER_MUT.
extern SparseLayer waterLayer;
#define WATER(x, y, z) (*(const WaterCell*)SparseLayerRead(&waterLayer, (x), (y), (z)))
#define WATER_MUT(x, y, z) (*(WaterCell*)SparseLayerWrite(&waterLayer, (x), (y), (z)))

// Global state
extern bool waterEnabled;           // Master toggle for water simulation
extern bool waterEvaporationEnabled; // Toggle evaporation (disable for testing)
// The map edge: beyond it lies water up to waterEdgeSeaZ (a sea, -1 = nothing). Edge cells
// trade with it: water above that height flows off the map, below it the edge fills up.
// Generated terrains turn it on (GenerateCurrentTerrain); off = the edge is a wall (tests)
extern bool waterEdgeOutflow;
extern int waterEdgeSeaZ;
extern float waterEvapInterval;     // Game-seconds between evaporation attempts for level-1 water
extern int waterUpdateCount;        // Cells updated last tick (for debug/profiling)

// Speed multipliers for movers walking through water (lower = slower)
extern float waterSpeedShallow;     // Speed in level 1-2 water (default: 0.85 = 15% slowdown)
extern float waterSpeedMedium;      // Speed in level 3-4 water (default: 0.6 = 40% slowdown)
extern float waterSpeedDeep;        // Speed in level 5-7 water (default: 0.35 = 65% slowdown)

// Mud speed multiplier (soil cells with wetness >= 2)
extern float mudSpeedMultiplier;    // Speed on muddy terrain (default: 0.6 = 40% slowdown)

// Wetness sync interval (game-seconds between water→wetness updates)
extern float wetnessSyncInterval;   // Default: 2.0s

// Initialize water system (call after grid is initialized)
void InitWater(void);

// Clear all water
void ClearWater(void);

// Main simulation tick (call from Tick())
void UpdateWater(void);

// Place/remove water
void SetWaterLevel(int x, int y, int z, int level);
// A sea cell's level, set by the tide (tide.c): like SetWaterLevel, but the sea cell stays asleep
// and only neighbours that aren't sea are woken (the whole sea waking every tide step ate the
// update budget rivers need). Stays awake itself only if it can spill into a non-sea neighbour.
void SetSeaLevel(int x, int y, int z, int level);
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
void WaterWakeAll(void);   // every cell gets looked at on the next tick (after a load)

// Springs (2026-11-24): with springTrickle on, a source cell inside the map is a spring that adds
// springRate units of water per game-second (a full cell is 7) instead of refilling to full on every
// visit (an endless tap: every tap with no way out floods). Sources on the map edge stay taps:
// they stand for a river coming in from beyond (hills-water). Off for test maps, sandbox and
// survival; the island generator turns it on. Saved (v103).
extern bool springTrickle;
extern float springRate;

// Water flow (current): which way water has been moving through a cell lately. Bookkeeping
// only, the sim doesn't read it. Every unit that moves sideways (spreading, a pressure push
// along its whole path, the map edge) adds its direction to the cells it leaves and enters;
// the sum fades with a half-life of waterFlowHalfLife water ticks (lazily, on read and write).
// Sparse: only chunks where water has moved get a block. Not saved (rebuilds in seconds).
typedef struct {
    int16_t fx, fy;      // summed direction, WATER_FLOW_SCALE per unit moved
    uint32_t tick;       // waterFlowTick when fx, fy were last decayed
} WaterFlowCell;
#define WATER_FLOW_SCALE 64
extern SparseLayer waterFlowLayer;
extern float waterFlowHalfLife;   // water ticks for the current to fade to half
extern uint32_t waterFlowTick;    // counts UpdateWater calls
// Current at a cell in units moved per half-life (decayed to now). false = no current there.
bool GetWaterFlow(int x, int y, int z, float* fx, float* fy);
void ClearWaterFlow(void);

// Displace water from a cell (push to neighbors/up, then clear)
// Call before placing a wall or other fluid-blocking structure
void DisplaceWater(int x, int y, int z);

// Speed multiplier for movers walking through water
// Returns 1.0 for no water, lower values for deeper water
float GetWaterSpeedMultiplier(int x, int y, int z);

// Water cell state getters (inline for zero overhead)
static inline bool IsWaterStable(int x, int y, int z) { return WATER(x, y, z).stable; }
static inline bool HasWaterPressure(int x, int y, int z) { return WATER(x, y, z).hasPressure; }

// Sky water (rain simulation)
// Starts continuous rain for ~30 game-seconds, spawning water in waves
// coverage = 1-100 (percentage of map area, spread across the duration)
void SpawnSkyWater(int coverage);
void UpdateRain(void);   // Call from game tick
void StopRain(void);
bool IsRaining(void);

// Accumulator getters/setters (for save/load)
float GetWaterEvapAccum(void);
void SetWaterEvapAccum(float v);

#endif // WATER_H
