#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// Temperature scale constants (Celsius, -128 to 127)
#define TEMP_MIN -128
#define TEMP_MAX 127
#define TEMP_AMBIENT_DEFAULT 20     // Default surface temperature (20C, room temp)

// Temperature thresholds (Celsius)
#define TEMP_DEEP_FREEZE -40        // Below this: deep freeze
#define TEMP_WATER_FREEZES 0        // Water freezes at or below this
#define TEMP_COLD_STORAGE 5         // Upper bound of cold storage range
#define TEMP_COMFORTABLE_MIN 15     // Comfortable range start
#define TEMP_COMFORTABLE_MAX 25     // Comfortable range end
#define TEMP_HOT 40                 // Hot territory
#define TEMP_FIRE_MIN 80            // Minimum temperature fire produces
#define TEMP_EXTREME 127            // Extreme heat (kiln, lava)

// Insulation tiers (affects heat transfer rate)
#define INSULATION_TIER_AIR 0       // Air/empty: heat flows freely
#define INSULATION_TIER_WOOD 1      // Wood: some insulation
#define INSULATION_TIER_STONE 2     // Stone: strong insulation

// Heat transfer rates (percentage, 0-100)
#define HEAT_TRANSFER_AIR 100       // 100% transfer through air
#define HEAT_TRANSFER_WOOD 20       // 20% transfer through wood
#define HEAT_TRANSFER_STONE 5       // 5% transfer through stone

// Performance tuning
#define TEMP_MAX_UPDATES_PER_TICK 4096

// Temperature cell data (parallel to grid)
typedef struct {
    int8_t current;         // Current temperature in Celsius (-128 to 127)
    uint8_t stable : 1;     // True = skip processing (no recent changes)
    uint8_t isHeatSource : 1;   // True = permanent heat source
    uint8_t isColdSource : 1;   // True = permanent cold source
} TempCell;

// Temperature grid (same dimensions as main grid)
extern TempCell temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool temperatureEnabled;     // Master toggle for temperature simulation
extern int tempUpdateCount;         // Cells updated last tick (for debug/profiling)

// Tweakable parameters
extern int ambientSurfaceTemp;      // Default surface temperature (default: 20)
extern int ambientDepthDecay;       // Temperature decrease per Z-level down (default: 0)
extern int heatTransferSpeed;       // How fast heat moves (1-100, default: 50)
extern int tempDecayRate;           // How fast temps return to ambient (1-100, default: 10)
extern int insulationTier1Rate;     // Wood transfer rate percentage (default: 20)
extern int insulationTier2Rate;     // Stone transfer rate percentage (default: 5)
extern int heatSourceTemp;          // Temperature of heat sources (default: 100)
extern int coldSourceTemp;          // Temperature of cold sources (default: -20)

// Initialize temperature system (call after grid is initialized)
void InitTemperature(void);

// Clear all temperature data (reset to ambient)
void ClearTemperature(void);

// Main simulation tick (call from Tick())
void UpdateTemperature(void);

// Set/get temperature
void SetTemperature(int x, int y, int z, int temp);
int GetTemperature(int x, int y, int z);

// Heat/cold source management (uses global heatSourceTemp/coldSourceTemp)
void SetHeatSource(int x, int y, int z, bool isSource);
void SetColdSource(int x, int y, int z, bool isSource);
void RemoveTemperatureSource(int x, int y, int z);

// Query
bool IsHeatSource(int x, int y, int z);
bool IsColdSource(int x, int y, int z);
int GetAmbientTemperature(int z);   // Get ambient temp for a z-level
int GetInsulationTier(int x, int y, int z);  // Get insulation tier for cell

// Temperature checks
bool IsFreezing(int x, int y, int z);       // temp <= TEMP_WATER_FREEZES
bool IsColdStorage(int x, int y, int z);    // temp <= TEMP_COLD_STORAGE
bool IsComfortable(int x, int y, int z);    // temp in comfortable range
bool IsHot(int x, int y, int z);            // temp >= TEMP_HOT

// Mark cell and neighbors as unstable (needs processing)
void DestabilizeTemperature(int x, int y, int z);

// Utility: apply heat from fire (called by fire system)
void ApplyFireHeat(int x, int y, int z, int fireLevel);

#endif // TEMPERATURE_H
