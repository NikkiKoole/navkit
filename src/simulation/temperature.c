#include "temperature.h"
#include <stdlib.h>
#include <string.h>

// Temperature grid
TempCell temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool temperatureEnabled = true;
int tempUpdateCount = 0;

// Tweakable parameters (stored as INDEX values, not Celsius)
// Quick reference: index = (celsius + 50) / 2  for Band A (-50C to 250C)
// Examples: 0C=25, 20C=35, 100C=75, 200C=125, 250C=150
int ambientSurfaceTemp = TEMP_AMBIENT_DEFAULT;  // Index 35 = 20C
int ambientDepthDecay = 0;                       // Index units per z-level underground
int heatTransferSpeed = 50;                      // 1-100 scale
int tempDecayRate = 10;                          // 1-100 scale
int insulationTier1Rate = HEAT_TRANSFER_WOOD;   // 20%
int insulationTier2Rate = HEAT_TRANSFER_STONE;  // 5%
int heatSourceTemp = 125;                        // Index 125 = 200C
int coldSourceTemp = 10;                         // Index 10 = -30C

// Direction offsets for orthogonal neighbors
static const int dx[] = {0, 0, -1, 1};
static const int dy[] = {-1, 1, 0, 0};

// Direction offsets for diagonal neighbors
static const int diag_dx[] = {-1, -1, 1, 1};
static const int diag_dy[] = {-1, 1, -1, 1};

// ============================================================================
// Temperature Encoding/Decoding
// ============================================================================
//
// Piecewise linear encoding:
//   Band A (index 0-150): -50°C to 250°C, 2°C steps
//   Band B (index 151-255): 250°C to 1498°C, 12°C steps
//

int DecodeTemp(uint8_t index) {
    if (index <= TEMP_INDEX_BAND_SPLIT) {
        // Band A: celsius = -50 + (index * 2)
        return TEMP_CELSIUS_MIN + (index * TEMP_BAND_A_STEP);
    } else {
        // Band B: celsius = 250 + ((index - 151) * 12)
        return TEMP_CELSIUS_BAND_SPLIT + ((index - TEMP_INDEX_BAND_SPLIT - 1) * TEMP_BAND_B_STEP);
    }
}

uint8_t EncodeTemp(int celsius) {
    // Clamp to valid range
    if (celsius < TEMP_CELSIUS_MIN) celsius = TEMP_CELSIUS_MIN;
    if (celsius > TEMP_CELSIUS_MAX) celsius = TEMP_CELSIUS_MAX;
    
    if (celsius <= TEMP_CELSIUS_BAND_SPLIT) {
        // Band A: index = (celsius + 50) / 2
        int index = (celsius - TEMP_CELSIUS_MIN) / TEMP_BAND_A_STEP;
        if (index < 0) index = 0;
        if (index > TEMP_INDEX_BAND_SPLIT) index = TEMP_INDEX_BAND_SPLIT;
        return (uint8_t)index;
    } else {
        // Band B: index = 151 + (celsius - 250) / 12
        int index = TEMP_INDEX_BAND_SPLIT + 1 + (celsius - TEMP_CELSIUS_BAND_SPLIT) / TEMP_BAND_B_STEP;
        if (index > TEMP_INDEX_MAX) index = TEMP_INDEX_MAX;
        return (uint8_t)index;
    }
}

// ============================================================================
// Initialization
// ============================================================================

void InitTemperature(void) {
    // Initialize all cells to ambient temperature based on depth
    for (int z = 0; z < gridDepth; z++) {
        uint8_t ambient = (uint8_t)GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                temperatureGrid[z][y][x].current = ambient;
                temperatureGrid[z][y][x].stable = true;
                temperatureGrid[z][y][x].isHeatSource = false;
                temperatureGrid[z][y][x].isColdSource = false;
            }
        }
    }
    tempUpdateCount = 0;
}

void ClearTemperature(void) {
    InitTemperature();
}

// ============================================================================
// Ambient Temperature
// ============================================================================

int GetAmbientTemperature(int z) {
    // Surface is at z = gridDepth - 1, underground is lower z values
    // For simplicity, treat z=0 as deepest, higher z = closer to surface
    // Ambient = surfaceTemp - (depth * decay)
    // where depth = (gridDepth - 1) - z
    // Returns INDEX value, not Celsius
    
    int depth = (gridDepth - 1) - z;
    if (depth < 0) depth = 0;
    
    int ambient = ambientSurfaceTemp - (depth * ambientDepthDecay);
    if (ambient < TEMP_INDEX_MIN) ambient = TEMP_INDEX_MIN;
    if (ambient > TEMP_INDEX_MAX) ambient = TEMP_INDEX_MAX;
    
    return ambient;
}

// ============================================================================
// Insulation
// ============================================================================

int GetInsulationTier(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return INSULATION_TIER_STONE;  // Out of bounds = solid stone
    }
    
    CellType cell = grid[z][y][x];
    
    switch (cell) {
        case CELL_WALL:
            // Default wall is stone tier
            return INSULATION_TIER_STONE;
        
        // TODO: Add CELL_WOOD_WALL, CELL_STONE_WALL, etc. when those CellTypes are added
        // case CELL_WOOD_WALL:
        //     return INSULATION_TIER_WOOD;
        // case CELL_STONE_WALL:
        //     return INSULATION_TIER_STONE;
        
        default:
            // Air, floor, grass, etc. = no insulation
            return INSULATION_TIER_AIR;
    }
}

// Get heat transfer rate based on insulation tier
static int GetHeatTransferRate(int tier) {
    switch (tier) {
        case INSULATION_TIER_WOOD:
            return insulationTier1Rate;
        case INSULATION_TIER_STONE:
            return insulationTier2Rate;
        default:
            return HEAT_TRANSFER_AIR;
    }
}

// ============================================================================
// Temperature Queries
// ============================================================================

// Get temperature as Celsius (for display, external use)
int GetTemperature(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return DecodeTemp((uint8_t)GetAmbientTemperature(z));
    }
    return DecodeTemp(temperatureGrid[z][y][x].current);
}

// Get temperature as index (for fast internal comparisons)
int GetTemperatureIndex(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return GetAmbientTemperature(z);
    }
    return temperatureGrid[z][y][x].current;
}

// Set temperature from Celsius value
void SetTemperature(int x, int y, int z, int celsius) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    temperatureGrid[z][y][x].current = EncodeTemp(celsius);
    DestabilizeTemperature(x, y, z);
}

// Set temperature from index value (for internal use)
void SetTemperatureIndex(int x, int y, int z, int index) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    if (index < TEMP_INDEX_MIN) index = TEMP_INDEX_MIN;
    if (index > TEMP_INDEX_MAX) index = TEMP_INDEX_MAX;
    
    temperatureGrid[z][y][x].current = (uint8_t)index;
    DestabilizeTemperature(x, y, z);
}

bool IsHeatSource(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return temperatureGrid[z][y][x].isHeatSource;
}

bool IsColdSource(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return temperatureGrid[z][y][x].isColdSource;
}

// Threshold checks use INDEX values for speed (no decode needed)
bool IsFreezing(int x, int y, int z) {
    return GetTemperatureIndex(x, y, z) <= TEMP_WATER_FREEZES;
}

bool IsColdStorage(int x, int y, int z) {
    return GetTemperatureIndex(x, y, z) <= TEMP_COLD_STORAGE;
}

bool IsComfortable(int x, int y, int z) {
    int index = GetTemperatureIndex(x, y, z);
    return index >= TEMP_COMFORTABLE_MIN && index <= TEMP_COMFORTABLE_MAX;
}

bool IsHot(int x, int y, int z) {
    return GetTemperatureIndex(x, y, z) >= TEMP_HOT;
}

// ============================================================================
// Source Management
// ============================================================================

void SetHeatSource(int x, int y, int z, bool isSource) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    cell->isHeatSource = isSource;
    cell->isColdSource = false;  // Can't be both
    
    if (isSource) {
        cell->current = (uint8_t)heatSourceTemp;  // heatSourceTemp is already an index
    }
    
    DestabilizeTemperature(x, y, z);
}

void SetColdSource(int x, int y, int z, bool isSource) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    cell->isColdSource = isSource;
    cell->isHeatSource = false;  // Can't be both
    
    if (isSource) {
        cell->current = (uint8_t)coldSourceTemp;  // coldSourceTemp is already an index
    }
    
    DestabilizeTemperature(x, y, z);
}

void RemoveTemperatureSource(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    cell->isHeatSource = false;
    cell->isColdSource = false;
    
    DestabilizeTemperature(x, y, z);
}

// ============================================================================
// Stability
// ============================================================================

void DestabilizeTemperature(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    // Mark this cell unstable
    temperatureGrid[z][y][x].stable = false;
    
    // Mark orthogonal neighbors unstable
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
            temperatureGrid[z][ny][nx].stable = false;
        }
    }
    
    // Mark diagonal neighbors unstable
    for (int i = 0; i < 4; i++) {
        int nx = x + diag_dx[i];
        int ny = y + diag_dy[i];
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
            temperatureGrid[z][ny][nx].stable = false;
        }
    }
    
    // Also destabilize vertical neighbors
    if (z > 0) {
        temperatureGrid[z-1][y][x].stable = false;
    }
    if (z < gridDepth - 1) {
        temperatureGrid[z+1][y][x].stable = false;
    }
}

// ============================================================================
// Fire Integration
// ============================================================================

void ApplyFireHeat(int x, int y, int z, int fireLevel) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    if (fireLevel <= 0) return;
    
    // Fire heats cell based on fire level (INDEX values)
    // fireLevel 1 -> index 68 (~86C), fireLevel 7 -> index 86 (~122C)
    // Using TEMP_FIRE_MIN (index 65 = 80C) as base
    int fireIndex = TEMP_FIRE_MIN + (fireLevel * 3);
    if (fireIndex > TEMP_INDEX_MAX) fireIndex = TEMP_INDEX_MAX;
    
    TempCell *cell = &temperatureGrid[z][y][x];
    if (fireIndex > cell->current) {
        cell->current = (uint8_t)fireIndex;
        DestabilizeTemperature(x, y, z);
    }
}

// ============================================================================
// Main Update Loop
// ============================================================================

void UpdateTemperature(void) {
    if (!temperatureEnabled) return;
    
    tempUpdateCount = 0;
    
    // Process all cells (could optimize with stability, but start simple)
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                TempCell *cell = &temperatureGrid[z][y][x];
                
                // Skip stable cells (optimization)
                if (cell->stable) continue;
                
                // Cap updates per tick
                if (tempUpdateCount >= TEMP_MAX_UPDATES_PER_TICK) {
                    return;
                }
                tempUpdateCount++;
                
                // Sources maintain their temperature and keep spreading
                if (cell->isHeatSource) {
                    cell->current = (uint8_t)heatSourceTemp;
                    // Keep neighbors destabilized so heat keeps spreading
                    DestabilizeTemperature(x, y, z);
                    continue;
                }
                if (cell->isColdSource) {
                    cell->current = (uint8_t)coldSourceTemp;
                    // Keep neighbors destabilized so cold keeps spreading
                    DestabilizeTemperature(x, y, z);
                    continue;
                }
                
                int currentTemp = cell->current;
                int myInsulation = GetInsulationTier(x, y, z);
                
                // Phase 1: Heat transfer with neighbors
                // Average temperature with neighbors, weighted by insulation
                int totalTransfer = 0;
                int neighborCount = 0;
                
                // Orthogonal neighbors (same z-level)
                for (int i = 0; i < 4; i++) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) {
                        continue;
                    }
                    
                    int neighborTemp = temperatureGrid[z][ny][nx].current;
                    int neighborInsulation = GetInsulationTier(nx, ny, z);
                    
                    // Use the higher insulation of the two cells (barrier effect)
                    int effectiveInsulation = (myInsulation > neighborInsulation) ? myInsulation : neighborInsulation;
                    int transferRate = GetHeatTransferRate(effectiveInsulation);
                    
                    // Calculate transfer amount
                    int tempDiff = neighborTemp - currentTemp;
                    int transfer = (tempDiff * transferRate * heatTransferSpeed) / 4000;
                    
                    totalTransfer += transfer;
                    neighborCount++;
                }
                
                // Diagonal neighbors (same z-level, reduced transfer due to distance)
                for (int i = 0; i < 4; i++) {
                    int nx = x + diag_dx[i];
                    int ny = y + diag_dy[i];
                    
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) {
                        continue;
                    }
                    
                    int neighborTemp = temperatureGrid[z][ny][nx].current;
                    int neighborInsulation = GetInsulationTier(nx, ny, z);
                    
                    int effectiveInsulation = (myInsulation > neighborInsulation) ? myInsulation : neighborInsulation;
                    int transferRate = GetHeatTransferRate(effectiveInsulation);
                    
                    // Calculate transfer amount (70% of orthogonal due to ~1.4x distance)
                    int tempDiff = neighborTemp - currentTemp;
                    int transfer = (tempDiff * transferRate * heatTransferSpeed * 70) / (4000 * 100);
                    
                    totalTransfer += transfer;
                    neighborCount++;
                }
                
                // Vertical neighbors (z-1 and z+1)
                // Heat rises: transfer up is boosted, transfer down is reduced
                for (int dz = -1; dz <= 1; dz += 2) {
                    int nz = z + dz;
                    if (nz < 0 || nz >= gridDepth) continue;
                    
                    int neighborTemp = temperatureGrid[nz][y][x].current;
                    int neighborInsulation = GetInsulationTier(x, y, nz);
                    
                    int effectiveInsulation = (myInsulation > neighborInsulation) ? myInsulation : neighborInsulation;
                    int transferRate = GetHeatTransferRate(effectiveInsulation);
                    
                    int tempDiff = neighborTemp - currentTemp;
                    int transfer = (tempDiff * transferRate * heatTransferSpeed) / 4000;
                    
                    // Heat rises: boost upward transfer of heat, reduce downward
                    if (dz > 0) {
                        // Neighbor is above us - if we're hotter, heat rises faster
                        if (currentTemp > neighborTemp) {
                            transfer = transfer * 150 / 100;  // 50% boost for rising heat
                        }
                    } else {
                        // Neighbor is below us - heat doesn't sink as easily
                        if (currentTemp > neighborTemp) {
                            transfer = transfer * 50 / 100;  // 50% reduction for sinking heat
                        }
                    }
                    
                    totalTransfer += transfer;
                    neighborCount++;
                }
                
                // Apply transfer
                if (neighborCount > 0) {
                    currentTemp += totalTransfer / neighborCount;
                }
                
                // Phase 2: Decay toward ambient
                if (currentTemp != ambient) {
                    int diff = ambient - currentTemp;
                    int decay = (diff * tempDecayRate) / 1000;
                    
                    // Ensure at least some decay happens if there's a difference
                    if (decay == 0 && diff != 0) {
                        decay = (diff > 0) ? 1 : -1;
                    }
                    
                    currentTemp += decay;
                }
                
                // Clamp to valid index range
                if (currentTemp < TEMP_INDEX_MIN) currentTemp = TEMP_INDEX_MIN;
                if (currentTemp > TEMP_INDEX_MAX) currentTemp = TEMP_INDEX_MAX;
                
                // Check if temperature changed
                if (currentTemp != cell->current) {
                    cell->current = (uint8_t)currentTemp;
                    // Keep neighbors unstable for continued spreading
                    DestabilizeTemperature(x, y, z);
                } else {
                    // No change, mark stable
                    cell->stable = true;
                }
            }
        }
    }
}
