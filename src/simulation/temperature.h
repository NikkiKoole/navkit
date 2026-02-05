#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// =============================================================================
// TEMPERATURE SYSTEM
// =============================================================================
//
// Temperature is stored as int16_t in Celsius. No encoding needed.
// Range: -32768 to 32767 (way more than we'll ever need)
//
// Memory: TempCell is 4 bytes (int16_t temp + flags)
//
// =============================================================================

// Temperature bounds (Celsius)
#define TEMP_MIN -100
#define TEMP_MAX 2000

// Default ambient temperature (Celsius)
#define TEMP_AMBIENT_DEFAULT 20

// Temperature thresholds (Celsius) - these are now human-readable!
#define TEMP_DEEP_FREEZE    -40     // Extreme cold
#define TEMP_WATER_FREEZES    0     // Water freezes
#define TEMP_COLD_STORAGE     5     // Refrigerator temperature
#define TEMP_COMFORTABLE_MIN 15     // Minimum comfortable
#define TEMP_COMFORTABLE_MAX 25     // Maximum comfortable
#define TEMP_HOT             40     // Uncomfortably hot
#define TEMP_FIRE_MIN        80     // Minimum fire temperature
#define TEMP_BOILING        100     // Water boils
#define TEMP_COOKING        200     // Oven temperature
#define TEMP_IGNITION       300     // Wood ignites
#define TEMP_FORGE          800     // Forge temperature
#define TEMP_MAGMA         1200     // Lava temperature

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
    int16_t current;            // Temperature in Celsius
    uint8_t stable : 1;         // True = skip processing (no recent changes)
    uint8_t isHeatSource : 1;   // True = permanent heat source
    uint8_t isColdSource : 1;   // True = permanent cold source
    uint8_t reserved : 5;       // Padding for alignment
} TempCell;

// Temperature grid (same dimensions as main grid)
extern TempCell temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool temperatureEnabled;     // Master toggle for temperature simulation
extern int tempUpdateCount;         // Cells updated last tick (for debug/profiling)

// Tweakable parameters (temps in Celsius, time in game-seconds)
extern int ambientSurfaceTemp;      // Default surface temperature (default: 20) - use SetAmbientSurfaceTemp() to change
extern int ambientDepthDecay;       // Temperature decrease per Z-level down (default: 0)
extern float heatTransferInterval;  // Game-seconds between heat transfer steps
extern float tempDecayInterval;     // Game-seconds between decay toward ambient steps
extern int insulationTier1Rate;     // Wood transfer rate percentage (default: 20)
extern int insulationTier2Rate;     // Stone transfer rate percentage (default: 5)
extern int heatSourceTemp;          // Temperature of heat sources (default: 200)
extern int coldSourceTemp;          // Temperature of cold sources (default: -20)

// Heat physics parameters
extern int heatRiseBoost;           // Upward heat transfer multiplier % (default: 150 = 50% boost)
extern int heatSinkReduction;       // Downward heat transfer multiplier % (default: 50 = 50% reduction)
extern int heatDecayPercent;        // Decay toward ambient per interval (default: 10 = 10%)
extern int diagonalTransferPercent; // Diagonal transfer vs orthogonal % (default: 70)

// =============================================================================
// Initialization
// =============================================================================

// Initialize temperature system (call after grid is initialized)
void InitTemperature(void);

// Clear all temperature data (reset to ambient)
void ClearTemperature(void);

// Main simulation tick (call from Tick())
void UpdateTemperature(void);

// Set/get temperature (Celsius)
void SetTemperature(int x, int y, int z, int celsius);
int GetTemperature(int x, int y, int z);

// Heat/cold source management
void SetHeatSource(int x, int y, int z, bool isSource);
void SetColdSource(int x, int y, int z, bool isSource);
void RemoveTemperatureSource(int x, int y, int z);

// Query
bool IsHeatSource(int x, int y, int z);
bool IsColdSource(int x, int y, int z);
int GetAmbientTemperature(int z);   // Get ambient temp for a z-level (Celsius)
int GetInsulationTier(int x, int y, int z);  // Get insulation tier for cell

// Temperature checks (all use Celsius thresholds)
bool IsFreezing(int x, int y, int z);       // temp <= TEMP_WATER_FREEZES
bool IsColdStorage(int x, int y, int z);    // temp <= TEMP_COLD_STORAGE
bool IsComfortable(int x, int y, int z);    // temp in comfortable range
bool IsHot(int x, int y, int z);            // temp >= TEMP_HOT

// Mark cell and neighbors as unstable (needs processing)
void DestabilizeTemperature(int x, int y, int z);

// Change ambient surface temperature (destabilizes all cells that now differ from ambient)
void SetAmbientSurfaceTemp(int temp);

// Utility: apply heat from fire (called by fire system)
void ApplyFireHeat(int x, int y, int z, int fireLevel);

// Temperature cell state getters (inline for zero overhead)
static inline bool IsTemperatureStable(int x, int y, int z) { return temperatureGrid[z][y][x].stable; }

#endif // TEMPERATURE_H
