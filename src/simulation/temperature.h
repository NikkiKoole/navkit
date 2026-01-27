#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

// =============================================================================
// TEMPERATURE ENCODING SYSTEM
// =============================================================================
//
// We use a piecewise linear encoding to store temperatures in a single uint8_t
// while covering a wide range (-50°C to ~1500°C) with good precision where it
// matters most (habitable/cooking temperatures).
//
// ENCODING SCHEME:
//
//   Band A (index 0-150): High precision for common temperatures
//     - Range: -50°C to 250°C
//     - Step: 2°C per index
//     - Formula: celsius = -50 + (index * 2)
//     - Covers: freezing, comfort, cooking, early fire
//
//   Band B (index 151-255): Lower precision for extreme heat
//     - Range: 250°C to 1498°C  
//     - Step: 12°C per index
//     - Formula: celsius = 250 + ((index - 151) * 12)
//     - Covers: kilns, forges, magma, metal smelting
//
// KEY INDEX VALUES:
//   Index   0 = -50°C  (deep freeze, cold biome)
//   Index  25 =   0°C  (water freezes)
//   Index  35 =  20°C  (room temperature)
//   Index  75 = 100°C  (water boils)
//   Index 125 = 200°C  (cooking/ovens)
//   Index 150 = 250°C  (band transition)
//   Index 175 = 538°C  (high heat)
//   Index 200 = 838°C  (forge territory)
//   Index 225 = 1138°C (magma)
//   Index 255 = 1498°C (maximum)
//
// MEMORY: TempCell is 2 bytes (uint8_t temp + uint8_t flags)
//
// =============================================================================

// Temperature index bounds (internal storage)
#define TEMP_INDEX_MIN 0
#define TEMP_INDEX_MAX 255
#define TEMP_INDEX_BAND_SPLIT 150   // Index where Band A ends, Band B begins

// Temperature Celsius bounds (actual values)
#define TEMP_CELSIUS_MIN -50        // Coldest representable
#define TEMP_CELSIUS_MAX 1498       // Hottest representable
#define TEMP_CELSIUS_BAND_SPLIT 250 // Celsius where bands meet

// Band A: index 0-150 maps to -50°C to 250°C (2°C steps)
#define TEMP_BAND_A_STEP 2
// Band B: index 151-255 maps to 250°C to 1498°C (12°C steps)  
#define TEMP_BAND_B_STEP 12

// Default ambient (index for 20°C = 35)
#define TEMP_AMBIENT_DEFAULT 35

// Temperature thresholds (as INDEX values for fast comparison)
#define TEMP_DEEP_FREEZE 5          // Index 5 = -40°C
#define TEMP_WATER_FREEZES 25       // Index 25 = 0°C
#define TEMP_COLD_STORAGE 27        // Index 27 = 4°C (rounded from 5°C)
#define TEMP_COMFORTABLE_MIN 32     // Index 32 = 14°C (rounded from 15°C)
#define TEMP_COMFORTABLE_MAX 37     // Index 37 = 24°C (rounded from 25°C)
#define TEMP_HOT 45                 // Index 45 = 40°C
#define TEMP_FIRE_MIN 65            // Index 65 = 80°C
#define TEMP_BOILING 75             // Index 75 = 100°C
#define TEMP_COOKING 125            // Index 125 = 200°C
#define TEMP_IGNITION 175           // Index 175 = 538°C (wood ignites ~300-400)
#define TEMP_FORGE 200              // Index 200 = 838°C
#define TEMP_MAGMA 225              // Index 225 = 1138°C
#define TEMP_EXTREME 255            // Index 255 = 1498°C (maximum)

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
    uint8_t current;        // Temperature index (0-255), use DecodeTemp() for Celsius
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

// =============================================================================
// Temperature Encoding/Decoding
// =============================================================================

// Convert temperature index (0-255) to Celsius (-50 to 1498)
int DecodeTemp(uint8_t index);

// Convert Celsius to temperature index (0-255)
uint8_t EncodeTemp(int celsius);

// Encode common Celsius values (convenience, compile-time friendly)
// Use these for setting temperatures from gameplay code
#define ENCODE_CELSIUS(c) ((c) <= 250 ? (uint8_t)(((c) + 50) / 2) : (uint8_t)(151 + ((c) - 250) / 12))

// =============================================================================
// Initialization
// =============================================================================

// Initialize temperature system (call after grid is initialized)
void InitTemperature(void);

// Clear all temperature data (reset to ambient)
void ClearTemperature(void);

// Main simulation tick (call from Tick())
void UpdateTemperature(void);

// Set/get temperature (Celsius for external use)
void SetTemperature(int x, int y, int z, int celsius);
int GetTemperature(int x, int y, int z);

// Set/get temperature index (0-255, for internal/fast operations)
void SetTemperatureIndex(int x, int y, int z, int index);
int GetTemperatureIndex(int x, int y, int z);

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
