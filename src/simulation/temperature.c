#include "temperature.h"
#include <stdlib.h>
#include <string.h>

// Temperature grid
TempCell temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool temperatureEnabled = true;
int tempUpdateCount = 0;

// Tweakable parameters
int ambientSurfaceTemp = TEMP_AMBIENT_DEFAULT;  // 128
int ambientDepthDecay = 0;                       // degrees per z-level underground
int heatTransferSpeed = 50;                      // 1-100 scale
int tempDecayRate = 10;                          // 1-100 scale
int insulationTier1Rate = HEAT_TRANSFER_WOOD;   // 20%
int insulationTier2Rate = HEAT_TRANSFER_STONE;  // 5%

// Direction offsets for orthogonal neighbors
static const int dx[] = {0, 0, -1, 1};
static const int dy[] = {-1, 1, 0, 0};

// ============================================================================
// Initialization
// ============================================================================

void InitTemperature(void) {
    // Initialize all cells to ambient temperature based on depth
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                temperatureGrid[z][y][x].current = (int8_t)ambient;
                temperatureGrid[z][y][x].stable = true;
                temperatureGrid[z][y][x].isHeatSource = false;
                temperatureGrid[z][y][x].isColdSource = false;
                temperatureGrid[z][y][x].sourceTemp = 0;
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
    
    int depth = (gridDepth - 1) - z;
    if (depth < 0) depth = 0;
    
    int ambient = ambientSurfaceTemp - (depth * ambientDepthDecay);
    if (ambient < TEMP_MIN) ambient = TEMP_MIN;
    if (ambient > TEMP_MAX) ambient = TEMP_MAX;
    
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

int GetTemperature(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return GetAmbientTemperature(z);
    }
    return temperatureGrid[z][y][x].current;
}

void SetTemperature(int x, int y, int z, int temp) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    if (temp < TEMP_MIN) temp = TEMP_MIN;
    if (temp > TEMP_MAX) temp = TEMP_MAX;
    
    temperatureGrid[z][y][x].current = (int8_t)temp;
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

bool IsFreezing(int x, int y, int z) {
    return GetTemperature(x, y, z) <= TEMP_WATER_FREEZES;
}

bool IsColdStorage(int x, int y, int z) {
    return GetTemperature(x, y, z) <= TEMP_COLD_STORAGE;
}

bool IsComfortable(int x, int y, int z) {
    int temp = GetTemperature(x, y, z);
    return temp >= TEMP_COMFORTABLE_MIN && temp <= TEMP_COMFORTABLE_MAX;
}

bool IsHot(int x, int y, int z) {
    return GetTemperature(x, y, z) >= TEMP_HOT;
}

// ============================================================================
// Source Management
// ============================================================================

void SetHeatSource(int x, int y, int z, bool isSource, int sourceTemp) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    cell->isHeatSource = isSource;
    cell->isColdSource = false;  // Can't be both
    
    if (isSource) {
        if (sourceTemp < TEMP_MIN) sourceTemp = TEMP_MIN;
        if (sourceTemp > TEMP_MAX) sourceTemp = TEMP_MAX;
        cell->sourceTemp = (int8_t)sourceTemp;
        cell->current = (int8_t)sourceTemp;
    }
    
    DestabilizeTemperature(x, y, z);
}

void SetColdSource(int x, int y, int z, bool isSource, int sourceTemp) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    cell->isColdSource = isSource;
    cell->isHeatSource = false;  // Can't be both
    
    if (isSource) {
        if (sourceTemp < TEMP_MIN) sourceTemp = TEMP_MIN;
        if (sourceTemp > TEMP_MAX) sourceTemp = TEMP_MAX;
        cell->sourceTemp = (int8_t)sourceTemp;
        cell->current = (int8_t)sourceTemp;
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
    cell->sourceTemp = 0;
    
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
    
    // Fire heats cell: temp = max(current, FIRE_MIN + fireLevel * 6)
    // fireLevel 1 -> 86C, fireLevel 7 -> 122C (close to max 127)
    int fireTemp = TEMP_FIRE_MIN + (fireLevel * 6);
    if (fireTemp > TEMP_MAX) fireTemp = TEMP_MAX;
    
    TempCell *cell = &temperatureGrid[z][y][x];
    if (fireTemp > cell->current) {
        cell->current = (int8_t)fireTemp;
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
                    cell->current = cell->sourceTemp;
                    // Keep neighbors destabilized so heat keeps spreading
                    DestabilizeTemperature(x, y, z);
                    continue;
                }
                if (cell->isColdSource) {
                    cell->current = cell->sourceTemp;
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
                
                // Clamp to valid range
                if (currentTemp < TEMP_MIN) currentTemp = TEMP_MIN;
                if (currentTemp > TEMP_MAX) currentTemp = TEMP_MAX;
                
                // Check if temperature changed
                if (currentTemp != cell->current) {
                    cell->current = (int8_t)currentTemp;
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
