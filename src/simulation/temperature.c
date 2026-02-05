#include "temperature.h"
#include "../core/sim_manager.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include <stdlib.h>
#include <string.h>

// Temperature grid
TempCell temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool temperatureEnabled = true;
int tempUpdateCount = 0;

// Tweakable parameters (temps in Celsius, time in game-seconds)
int ambientSurfaceTemp = TEMP_AMBIENT_DEFAULT;  // 20C
int ambientDepthDecay = 0;                       // Celsius per z-level underground
float heatTransferInterval = 0.1f;               // Transfer heat every 0.1 game-seconds
float tempDecayInterval = 0.1f;                  // Decay toward ambient every 0.1 game-seconds
int insulationTier1Rate = HEAT_TRANSFER_WOOD;   // 20%
int insulationTier2Rate = HEAT_TRANSFER_STONE;  // 5%
int heatSourceTemp = 200;                        // 200C (fire/furnace)
int coldSourceTemp = -20;                        // -20C (ice/cold source)

// Heat physics parameters
int heatRiseBoost = 150;            // 150% = heat rises 50% faster
int heatSinkReduction = 50;         // 50% = heat sinks 50% slower
int heatDecayPercent = 10;          // Decay 10% of difference per interval
int diagonalTransferPercent = 70;   // Diagonal is 70% of orthogonal (due to ~1.4x distance)

// Internal accumulators for game-time
static float heatTransferAccum = 0.0f;
static float tempDecayAccum = 0.0f;

// Direction offsets for orthogonal neighbors
static const int dx[] = {0, 0, -1, 1};
static const int dy[] = {-1, 1, 0, 0};

// Direction offsets for diagonal neighbors
static const int diag_dx[] = {-1, -1, 1, 1};
static const int diag_dy[] = {-1, 1, -1, 1};

// ============================================================================
// Initialization
// ============================================================================

void InitTemperature(void) {
    // Initialize all cells to ambient temperature based on depth
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                temperatureGrid[z][y][x].current = (int16_t)ambient;
                temperatureGrid[z][y][x].stable = true;
                temperatureGrid[z][y][x].isHeatSource = false;
                temperatureGrid[z][y][x].isColdSource = false;
            }
        }
    }
    tempUpdateCount = 0;
    tempSourceCount = 0;
    tempUnstableCells = 0;
    heatTransferAccum = 0.0f;
    tempDecayAccum = 0.0f;
}

void ClearTemperature(void) {
    InitTemperature();
}

// ============================================================================
// Ambient Temperature
// ============================================================================

int GetAmbientTemperature(int z) {
    // Surface is at z = gridDepth - 1, underground is lower z values
    // Ambient = surfaceTemp - (depth * decay)
    // where depth = (gridDepth - 1) - z
    // Returns Celsius
    
    int depth = (gridDepth - 1) - z;
    if (depth < 0) depth = 0;
    
    int ambient = ambientSurfaceTemp - (depth * ambientDepthDecay);
    if (ambient < TEMP_MIN) ambient = TEMP_MIN;
    if (ambient > TEMP_MAX) ambient = TEMP_MAX;
    
    return ambient;
}

void SetAmbientSurfaceTemp(int temp) {
    if (temp == ambientSurfaceTemp) return;
    
    ambientSurfaceTemp = temp;
    
    // Recalculate tempUnstableCells since ambient changed
    tempUnstableCells = 0;
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                TempCell *cell = &temperatureGrid[z][y][x];
                // A cell needs processing if unstable OR differs from ambient
                if (!cell->stable || cell->current != ambient) {
                    tempUnstableCells++;
                }
            }
        }
    }
}

// ============================================================================
// Insulation
// ============================================================================

int GetInsulationTier(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return INSULATION_TIER_STONE;  // Out of bounds = solid stone
    }
    
    CellType cell = grid[z][y][x];
    return CellInsulationTier(cell);
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

void SetTemperature(int x, int y, int z, int celsius) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    if (celsius < TEMP_MIN) celsius = TEMP_MIN;
    if (celsius > TEMP_MAX) celsius = TEMP_MAX;
    
    temperatureGrid[z][y][x].current = (int16_t)celsius;
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

// Threshold checks - now using Celsius directly!
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

void SetHeatSource(int x, int y, int z, bool isSource) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    bool wasSource = cell->isHeatSource || cell->isColdSource;
    
    cell->isHeatSource = isSource;
    cell->isColdSource = false;  // Can't be both
    
    bool isNowSource = cell->isHeatSource || cell->isColdSource;
    
    if (!wasSource && isNowSource) {
#if USE_PRESENCE_GRID
        SetSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
#endif
        tempSourceCount++;
    } else if (wasSource && !isNowSource) {
#if USE_PRESENCE_GRID
        ClearSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
#endif
        tempSourceCount--;
    }
    
    if (isSource) {
        cell->current = (int16_t)heatSourceTemp;
    }
    
    DestabilizeTemperature(x, y, z);
}

void SetColdSource(int x, int y, int z, bool isSource) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    TempCell *cell = &temperatureGrid[z][y][x];
    bool wasSource = cell->isHeatSource || cell->isColdSource;
    
    cell->isColdSource = isSource;
    cell->isHeatSource = false;  // Can't be both
    
    bool isNowSource = cell->isHeatSource || cell->isColdSource;
    
    if (!wasSource && isNowSource) {
#if USE_PRESENCE_GRID
        SetSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
#endif
        tempSourceCount++;
    } else if (wasSource && !isNowSource) {
#if USE_PRESENCE_GRID
        ClearSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
#endif
        tempSourceCount--;
    }
    
    if (isSource) {
        cell->current = (int16_t)coldSourceTemp;
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

// Helper to mark a single cell unstable and update counter
static void MarkCellUnstable(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    TempCell *cell = &temperatureGrid[z][y][x];
    if (cell->stable) {
        cell->stable = false;
        int ambient = GetAmbientTemperature(z);
        // Only increment if it was stable AND at ambient (otherwise already counted)
        if (cell->current == ambient) {
            tempUnstableCells++;
        }
    }
}

void DestabilizeTemperature(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    // Mark this cell unstable
    MarkCellUnstable(x, y, z);
    
    // Mark orthogonal neighbors unstable
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        MarkCellUnstable(nx, ny, z);
    }
    
    // Mark diagonal neighbors unstable
    for (int i = 0; i < 4; i++) {
        int nx = x + diag_dx[i];
        int ny = y + diag_dy[i];
        MarkCellUnstable(nx, ny, z);
    }
    
    // Also destabilize vertical neighbors
    MarkCellUnstable(x, y, z - 1);
    MarkCellUnstable(x, y, z + 1);
}

// ============================================================================
// Fire Integration
// ============================================================================

void ApplyFireHeat(int x, int y, int z, int fireLevel) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    if (fireLevel <= 0) return;
    
    // Fire heats cell based on fire level (Celsius)
    // fireLevel 1 -> 100C, fireLevel 7 -> 250C
    int fireTemp = TEMP_BOILING + (fireLevel * 20);
    if (fireTemp > TEMP_MAX) fireTemp = TEMP_MAX;
    
    TempCell *cell = &temperatureGrid[z][y][x];
    if (fireTemp > cell->current) {
        cell->current = (int16_t)fireTemp;
        DestabilizeTemperature(x, y, z);
    }
}

// ============================================================================
// Main Update Loop
// ============================================================================

void UpdateTemperature(void) {
    if (!temperatureEnabled) return;
    
    // Accumulate game time for interval-based actions
    heatTransferAccum += gameDeltaTime;
    tempDecayAccum += gameDeltaTime;
    
    // Check if intervals have elapsed
    bool doTransfer = heatTransferAccum >= heatTransferInterval;
    bool doDecay = tempDecayAccum >= tempDecayInterval;
    
    // Reset accumulators when intervals elapse
    if (doTransfer) heatTransferAccum -= heatTransferInterval;
    if (doDecay) tempDecayAccum -= tempDecayInterval;
    
    // Early exit if nothing to do this tick (keep previous count for reporting)
    if (!doTransfer && !doDecay) {
        return;
    }
    
    // Early exit if no cells need processing (all stable at ambient, no sources)
    if (tempUnstableCells == 0 && tempSourceCount == 0) {
        tempUpdateCount = 0;
        return;
    }
    
    // Reset count only when we're actually going to process
    tempUpdateCount = 0;
    
    // Process all cells
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                TempCell *cell = &temperatureGrid[z][y][x];
                
                // Skip stable cells (but not if temperature differs from ambient)
                if (cell->stable && cell->current == ambient) {
                    continue;
                }
                
                // Cap updates per tick
                if (tempUpdateCount >= TEMP_MAX_UPDATES_PER_TICK) {
                    return;
                }
                tempUpdateCount++;
                
                // Sources maintain their temperature and keep spreading
                if (cell->isHeatSource) {
                    cell->current = (int16_t)heatSourceTemp;
                    DestabilizeTemperature(x, y, z);
                    continue;
                }
                if (cell->isColdSource) {
                    cell->current = (int16_t)coldSourceTemp;
                    DestabilizeTemperature(x, y, z);
                    continue;
                }
                
                int currentTemp = cell->current;
                int myInsulation = GetInsulationTier(x, y, z);
                
                // Phase 1: Heat transfer with neighbors (only when interval elapses)
                if (doTransfer) {
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
                        
                        // Use the higher insulation of the two cells
                        int effectiveInsulation = (myInsulation > neighborInsulation) ? myInsulation : neighborInsulation;
                        int transferRate = GetHeatTransferRate(effectiveInsulation);
                        
                        // Calculate transfer amount (transferRate is percentage 0-100)
                        int tempDiff = neighborTemp - currentTemp;
                        int transfer = (tempDiff * transferRate) / 100;
                        
                        totalTransfer += transfer;
                        neighborCount++;
                    }
                    
                    // Diagonal neighbors (same z-level, reduced transfer)
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
                        
                        // Diagonal transfer is reduced due to ~1.4x distance
                        int tempDiff = neighborTemp - currentTemp;
                        int transfer = (tempDiff * transferRate * diagonalTransferPercent) / (100 * 100);
                        
                        totalTransfer += transfer;
                        neighborCount++;
                    }
                    
                    // Vertical neighbors (z-1 and z+1)
                    for (int dz = -1; dz <= 1; dz += 2) {
                        int nz = z + dz;
                        if (nz < 0 || nz >= gridDepth) continue;
                        
                        int neighborTemp = temperatureGrid[nz][y][x].current;
                        int neighborInsulation = GetInsulationTier(x, y, nz);
                        
                        int effectiveInsulation = (myInsulation > neighborInsulation) ? myInsulation : neighborInsulation;
                        int transferRate = GetHeatTransferRate(effectiveInsulation);
                        
                        int tempDiff = neighborTemp - currentTemp;
                        int transfer = (tempDiff * transferRate) / 100;
                        
                        // Heat rises: boost upward, reduce downward
                        if (dz > 0 && currentTemp > neighborTemp) {
                            transfer = transfer * heatRiseBoost / 100;
                        } else if (dz < 0 && currentTemp > neighborTemp) {
                            transfer = transfer * heatSinkReduction / 100;
                        }
                        
                        totalTransfer += transfer;
                        neighborCount++;
                    }
                    
                    // Apply transfer
                    if (neighborCount > 0) {
                        currentTemp += totalTransfer / neighborCount;
                    }
                }
                
                // Phase 2: Decay toward ambient (only when interval elapses)
                if (doDecay && currentTemp != ambient) {
                    int diff = ambient - currentTemp;
                    // Decay by heatDecayPercent% of the difference each interval
                    int decay = (diff * heatDecayPercent) / 100;
                    
                    // Ensure at least some decay happens
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
                    cell->current = (int16_t)currentTemp;
                    DestabilizeTemperature(x, y, z);
                } else {
                    // Cell didn't change - mark stable
                    if (!cell->stable) {
                        cell->stable = true;
                        // If now stable AND at ambient, decrement counter
                        if (currentTemp == ambient) {
                            tempUnstableCells--;
                        }
                    }
                }
            }
        }
    }
}
