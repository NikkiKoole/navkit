#include "water.h"
#include "steam.h"
#include "temperature.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include <string.h>
#include <stdlib.h>

// Water grid storage - single buffer (falling sand style)
WaterCell waterGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool waterEnabled = true;
bool waterEvaporationEnabled = true;
float waterEvapInterval = WATER_EVAP_INTERVAL_DEFAULT;
int waterUpdateCount = 0;

// Speed multipliers for movers walking through water
float waterSpeedShallow = 0.85f;    // Level 1-2: slight slowdown (15%)
float waterSpeedMedium = 0.6f;      // Level 3-4: noticeable slowdown (40%)
float waterSpeedDeep = 0.35f;       // Level 5-7: major slowdown (65%)

// Mud speed multiplier
float mudSpeedMultiplier = 0.6f;    // 40% slowdown on muddy terrain

// Wetness sync interval (how often water sets cell wetness on soil)
float wetnessSyncInterval = 2.0f;

// Internal accumulators
static float waterEvapAccum = 0.0f;
static float wetnessSyncAccum = 0.0f;

// BFS queue for pressure propagation
typedef struct {
    int x, y, z;
} WaterPos;

static WaterPos pressureQueue[WATER_PRESSURE_SEARCH_LIMIT];

// Generation-based visited tracking (avoids expensive memset each call)
static uint16_t pressureVisitedGen[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
static uint16_t currentPressureGen = 0;

// Initialize water system
void InitWater(void) {
    ClearWater();
}

// Clear all water
void ClearWater(void) {
    memset(waterGrid, 0, sizeof(waterGrid));
    waterUpdateCount = 0;
    waterEvapAccum = 0.0f;
    wetnessSyncAccum = 0.0f;
    waterActiveCells = 0;
}

// Bounds check helper
static inline bool WaterInBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth && 
           y >= 0 && y < gridHeight && 
           z >= 0 && z < gridDepth;
}

// Check if water can exist in a cell (and flow into it)
static inline bool CanHoldWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    CellType cell = grid[z][y][x];
    if (!CellAllowsFluids(cell)) return false;
    
    // Frozen water acts as a solid block for water flow
    if (waterGrid[z][y][x].isFrozen) return false;
    
    return true;
}

// Mark cell and neighbors as unstable
void DestabilizeWater(int x, int y, int z) {
    if (WaterInBounds(x, y, z)) {
        waterGrid[z][y][x].stable = false;
    }
    
    // 4 horizontal neighbors (orthogonal only, like DF pressure)
    if (WaterInBounds(x-1, y, z)) waterGrid[z][y][x-1].stable = false;
    if (WaterInBounds(x+1, y, z)) waterGrid[z][y][x+1].stable = false;
    if (WaterInBounds(x, y-1, z)) waterGrid[z][y-1][x].stable = false;
    if (WaterInBounds(x, y+1, z)) waterGrid[z][y+1][x].stable = false;
    
    // Above and below
    if (WaterInBounds(x, y, z-1)) waterGrid[z-1][y][x].stable = false;
    if (WaterInBounds(x, y, z+1)) waterGrid[z+1][y][x].stable = false;
}

// Displace water from a cell before placing a wall
void DisplaceWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return;
    
    int level = waterGrid[z][y][x].level;
    if (level == 0) return;
    
    // Push water to horizontal neighbors first, then up
    int dx[] = {-1, 1, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0};
    int dz[] = {0, 0, 0, 0, 1};
    
    for (int i = 0; i < 5 && level > 0; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        
        if (!WaterInBounds(nx, ny, nz)) continue;
        if (CellBlocksFluids(grid[nz][ny][nx])) continue;
        
        int space = WATER_MAX_LEVEL - waterGrid[nz][ny][nx].level;
        if (space > 0) {
            int transfer = (level < space) ? level : space;
            AddWater(nx, ny, nz, transfer);
            level -= transfer;
        }
    }
    
    // Clear remaining water and destabilize neighbors
    SetWaterLevel(x, y, z, 0);
    SetWaterSource(x, y, z, false);
    SetWaterDrain(x, y, z, false);
    DestabilizeWater(x, y, z);
}

// Helper to check if a water cell is "active" (needs processing)
static inline bool WaterCellIsActive(int x, int y, int z) {
    WaterCell *wc = &waterGrid[z][y][x];
    return wc->level > 0 || wc->isSource || wc->isDrain;
}

// Set water level at a cell
void SetWaterLevel(int x, int y, int z, int level) {
    if (!WaterInBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > WATER_MAX_LEVEL) level = WATER_MAX_LEVEL;
    
    int oldLevel = waterGrid[z][y][x].level;
    waterGrid[z][y][x].level = (uint8_t)level;
    
    // Update presence grid: track transition from inactive to active or vice versa
    bool wasActive = oldLevel > 0 || waterGrid[z][y][x].isSource || waterGrid[z][y][x].isDrain;
    bool isActive = level > 0 || waterGrid[z][y][x].isSource || waterGrid[z][y][x].isDrain;
    
    if (!wasActive && isActive) {
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        waterActiveCells--;
    }
    
    if (oldLevel != level) {
        DestabilizeWater(x, y, z);
    }
}

// Add water to a cell
void AddWater(int x, int y, int z, int amount) {
    if (!WaterInBounds(x, y, z)) return;
    int newLevel = waterGrid[z][y][x].level + amount;
    SetWaterLevel(x, y, z, newLevel);
}

// Remove water from a cell
void RemoveWater(int x, int y, int z, int amount) {
    if (!WaterInBounds(x, y, z)) return;
    int newLevel = waterGrid[z][y][x].level - amount;
    SetWaterLevel(x, y, z, newLevel);
}

// Set water source
void SetWaterSource(int x, int y, int z, bool isSource) {
    if (!WaterInBounds(x, y, z)) return;
    
    bool wasActive = WaterCellIsActive(x, y, z);
    waterGrid[z][y][x].isSource = isSource;
    bool isActive = WaterCellIsActive(x, y, z);
    
    if (!wasActive && isActive) {
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        waterActiveCells--;
    }
    
    if (isSource) {
        DestabilizeWater(x, y, z);
    }
}

// Set water drain
void SetWaterDrain(int x, int y, int z, bool isDrain) {
    if (!WaterInBounds(x, y, z)) return;
    
    bool wasActive = WaterCellIsActive(x, y, z);
    waterGrid[z][y][x].isDrain = isDrain;
    bool isActive = WaterCellIsActive(x, y, z);
    
    if (!wasActive && isActive) {
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        waterActiveCells--;
    }
    
    if (isDrain) {
        DestabilizeWater(x, y, z);
    }
}

// Get water level
int GetWaterLevel(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return 0;
    return waterGrid[z][y][x].level;
}

// Check if cell has water
bool HasWater(int x, int y, int z) {
    return GetWaterLevel(x, y, z) > 0;
}

// Check if cell is full (7/7)
bool IsFull(int x, int y, int z) {
    return GetWaterLevel(x, y, z) >= WATER_MAX_LEVEL;
}

// Check if underwater at minimum depth
bool IsUnderwater(int x, int y, int z, int minDepth) {
    return GetWaterLevel(x, y, z) >= minDepth;
}

bool IsWaterSourceAt(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    return waterGrid[z][y][x].isSource;
}

bool IsWaterDrainAt(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    return waterGrid[z][y][x].isDrain;
}

// Get speed multiplier for water (DF-style 1-7 scale)
float GetWaterSpeedMultiplier(int x, int y, int z) {
    int level = GetWaterLevel(x, y, z);
    if (level == 0) return 1.0f;
    
    // 1-2: shallow, slight slowdown
    // 3-4: medium, noticeable slowdown
    // 5-7: deep, major slowdown
    if (level <= 2) return waterSpeedShallow;
    if (level <= 4) return waterSpeedMedium;
    return waterSpeedDeep;
}

// =============================================================================
// FALLING SAND STYLE WATER SIMULATION
// Single buffer, randomized spread direction, bottom-to-top processing
// Priority: 1. Falling  2. Spreading  3. Pressure
// =============================================================================

// Phase 1: FALLING - Water drops down if cell below isn't full
// Returns amount of water that fell
static int TryFall(int x, int y, int z) {
    if (z <= 0) return 0;  // Already at bottom
    if (HAS_FLOOR(x, y, z)) return 0;  // Constructed floor blocks falling
    if (!CanHoldWater(x, y, z - 1)) return 0;  // Wall below
    
    WaterCell* src = &waterGrid[z][y][x];
    WaterCell* dst = &waterGrid[z-1][y][x];
    
    if (src->level == 0) return 0;
    
    int space = WATER_MAX_LEVEL - dst->level;
    if (space <= 0) {
        // Cell below is full - create pressure (water wants to fall but can't)
        dst->hasPressure = true;
        dst->pressureSourceZ = z;
        DestabilizeWater(x, y, z - 1);
        return 0;
    }
    
    int flow = src->level;
    if (flow > space) flow = space;
    
    // Track transitions for counter updates
    bool srcWasActive = src->level > 0;
    bool dstWasActive = dst->level > 0;
    
    src->level -= flow;
    dst->level += flow;
    
    // Update counters for transitions
    if (srcWasActive && src->level == 0) {
        waterActiveCells--;
    }
    if (!dstWasActive && dst->level > 0) {
        waterActiveCells++;
    }
    
    // Falling water onto full water creates pressure
    if (dst->level == WATER_MAX_LEVEL) {
        dst->hasPressure = true;
        dst->pressureSourceZ = z;
    }
    
    DestabilizeWater(x, y, z);
    DestabilizeWater(x, y, z - 1);
    
    return flow;
}

// Phase 2: SPREADING - Equalize water levels with orthogonal neighbors
// Falling sand style: randomize direction order to prevent directional bias
// Returns true if water moved
static bool WaterTrySpread(int x, int y, int z) {
    WaterCell* cell = &waterGrid[z][y][x];
    if (cell->level == 0) return false;
    
    // Orthogonal neighbors - randomize order each call
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    // Fisher-Yates shuffle for direction order
    int order[] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    
    bool moved = false;
    
    for (int i = 0; i < 4; i++) {
        int dir = order[i];
        int nx = x + dx[dir];
        int ny = y + dy[dir];
        
        if (!CanHoldWater(nx, ny, z)) continue;
        
        WaterCell* neighbor = &waterGrid[z][ny][nx];
        
        // Don't spread water into drains
        if (neighbor->isDrain) continue;
        
        int diff = cell->level - neighbor->level;
        
        // Spread to lower neighbors
        if (diff >= 2) {
            // Big difference: give 1 unit, keep going
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;
            
            // Track neighbor becoming active
            if (neighborWasEmpty) {
                waterActiveCells++;
            }
            
            DestabilizeWater(x, y, z);
            DestabilizeWater(nx, ny, z);
            moved = true;
            
            // Stop if we're now at level 1
            if (cell->level <= 1) break;
        } else if (diff == 1 && cell->level > 1) {
            // Small difference: give 1 unit to ONE neighbor only (randomized)
            // This breaks the staircase pattern over time
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;
            
            // Track neighbor becoming active
            if (neighborWasEmpty) {
                waterActiveCells++;
            }
            
            DestabilizeWater(x, y, z);
            DestabilizeWater(nx, ny, z);
            moved = true;
            
            // Only give to one neighbor when diff=1 to prevent oscillation
            break;
        }
    }
    
    return moved;
}

// Phase 3: PRESSURE - When water is full and under pressure, trace through
// full cells to find an outlet (BFS). Water can rise up to pressureSourceZ - 1.
// Returns true if water moved via pressure
static bool TryPressure(int x, int y, int z) {
    WaterCell* cell = &waterGrid[z][y][x];
    
    // Need full water with pressure to propagate
    if (cell->level < WATER_MAX_LEVEL) return false;
    if (!cell->hasPressure) return false;
    
    int maxZ = cell->pressureSourceZ - 1;
    if (maxZ < 0) maxZ = 0;
    
    // BFS to find nearest non-full cell through full cells
    // Use generation counter instead of memset (much faster)
    currentPressureGen++;
    if (currentPressureGen == 0) {
        // Handle wrap-around (rare) - clear the array once
        memset(pressureVisitedGen, 0, sizeof(pressureVisitedGen));
        currentPressureGen = 1;
    }
    
    int queueHead = 0;
    int queueTail = 0;
    
    int dx[] = {-1, 1, 0, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0, 0};
    int dz[] = {0, 0, 0, 0, -1, 1};
    
    pressureVisitedGen[z][y][x] = currentPressureGen;
    
    // Add initial neighbors to queue
    for (int i = 0; i < 6; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        
        if (nz > maxZ) continue;
        if (!CanHoldWater(nx, ny, nz)) continue;
        if (pressureVisitedGen[nz][ny][nx] == currentPressureGen) continue;
        
        pressureVisitedGen[nz][ny][nx] = currentPressureGen;
        pressureQueue[queueTail++] = (WaterPos){nx, ny, nz};
        
        if (queueTail >= WATER_PRESSURE_SEARCH_LIMIT) break;
    }
    
    // BFS through full cells looking for non-full cell
    while (queueHead < queueTail) {
        WaterPos pos = pressureQueue[queueHead++];
        WaterCell* current = &waterGrid[pos.z][pos.y][pos.x];
        
        // Found a non-full cell - push water here!
        if (current->level < WATER_MAX_LEVEL) {
            int space = WATER_MAX_LEVEL - current->level;
            int transfer = 1;
            if (transfer > space) transfer = space;
            if (transfer > cell->level) transfer = cell->level;
            
            if (transfer > 0) {
                bool currentWasEmpty = current->level == 0;
                cell->level -= transfer;
                current->level += transfer;
                
                // Track destination becoming active
                if (currentWasEmpty) {
                    waterActiveCells++;
                }
                
                // Propagate pressure info
                if (current->level == WATER_MAX_LEVEL) {
                    current->hasPressure = true;
                    current->pressureSourceZ = cell->pressureSourceZ;
                }
                
                DestabilizeWater(x, y, z);
                DestabilizeWater(pos.x, pos.y, pos.z);
                
                // Clear pressure if we're no longer full
                if (cell->level < WATER_MAX_LEVEL) {
                    cell->hasPressure = false;
                }
                
                return true;
            }
        }
        
        // Cell is full - continue searching through it
        if (current->level >= WATER_MAX_LEVEL) {
            for (int i = 0; i < 6; i++) {
                int nx = pos.x + dx[i];
                int ny = pos.y + dy[i];
                int nz = pos.z + dz[i];
                
                if (nz > maxZ) continue;
                if (!CanHoldWater(nx, ny, nz)) continue;
                if (pressureVisitedGen[nz][ny][nx] == currentPressureGen) continue;
                
                pressureVisitedGen[nz][ny][nx] = currentPressureGen;
                
                if (queueTail < WATER_PRESSURE_SEARCH_LIMIT) {
                    pressureQueue[queueTail++] = (WaterPos){nx, ny, nz};
                }
            }
        }
    }
    
    return false;
}

// Process a single water cell with DF-style rules
// doEvap: interval has elapsed for evaporation
static bool ProcessWaterCell(int x, int y, int z, bool doEvap) {
    WaterCell* cell = &waterGrid[z][y][x];
    bool moved = false;
    
    // Handle sources - refill to max and add pressure
    if (cell->isSource) {
        if (cell->level < WATER_MAX_LEVEL) {
            cell->level = WATER_MAX_LEVEL;
            cell->hasPressure = true;
            cell->pressureSourceZ = z;
            DestabilizeWater(x, y, z);
            moved = true;
        }
    }
    
    // Handle drains - remove water
    if (cell->isDrain) {
        if (cell->level > 0) {
            cell->level = 0;
            cell->hasPressure = false;
            DestabilizeWater(x, y, z);
            return true;
        }
    }
    
    // No water to process
    if (cell->level == 0) {
        cell->stable = true;
        cell->hasPressure = false;
        return false;
    }
    
    // Phase 1: Try to fall (highest priority)
    int fell = TryFall(x, y, z);
    if (fell > 0) moved = true;
    
    // Phase 2: Try to spread horizontally (if we still have water)
    if (cell->level > 0) {
        if (WaterTrySpread(x, y, z)) moved = true;
    }
    
    // Phase 3: Try pressure propagation (if full and pressurized)
    if (cell->level >= WATER_MAX_LEVEL && cell->hasPressure) {
        if (TryPressure(x, y, z)) moved = true;
    }
    
    // Sources refill AFTER spreading to maintain level 7
    if (cell->isSource) {
        cell->level = WATER_MAX_LEVEL;
        cell->hasPressure = true;
        return true;
    }
    
    // Evaporation: level 1 water has a chance to evaporate each interval
    // Random chance prevents all puddles vanishing simultaneously
    if (doEvap && waterEvaporationEnabled && cell->level == 1 && !cell->isSource) {
        if (rand() % 100 < 50) {
            SetWaterLevel(x, y, z, 0);
            cell->hasPressure = false;
            moved = true;
        }
    }
    
    // Clear pressure if we're no longer full
    if (cell->level < WATER_MAX_LEVEL) {
        cell->hasPressure = false;
    }
    
    // Mark stable if nothing moved AND neighbors are balanced
    if (!moved) {
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        bool balanced = true;
        
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (WaterInBounds(nx, ny, z) && CanHoldWater(nx, ny, z)) {
                int neighborLevel = waterGrid[z][ny][nx].level;
                int diff = neighborLevel - cell->level;
                // Unbalanced if neighbor could give us water (diff >= 1 and they have > 1)
                if (diff >= 1 && neighborLevel > 1) {
                    balanced = false;
                    break;
                }
            }
        }
        
        // Level-1 water can evaporate, so keep it unstable
        if (balanced && cell->level != 1) {
            cell->stable = true;
        }
    }
    
    return moved;
}

// Main water update - process all unstable cells
// Falling sand style: bottom-to-top, single buffer, randomized spread
void UpdateWater(void) {
    if (!waterEnabled) return;
    
    // Early exit: no water activity at all
    if (waterActiveCells == 0) {
        waterUpdateCount = 0;
        return;
    }
    
    waterUpdateCount = 0;
    
    // Accumulate game time for interval-based evaporation
    waterEvapAccum += gameDeltaTime;
    
    // Check if evaporation interval has elapsed
    bool doEvap = waterEvapAccum >= waterEvapInterval;
    if (doEvap) waterEvapAccum -= waterEvapInterval;
    
    // Process from bottom to top (simple iteration, keeps early exit optimization)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                WaterCell* cell = &waterGrid[z][y][x];
                
                // Skip frozen water - it doesn't flow
                if (cell->isFrozen) {
                    continue;
                }
                
                // Skip stable empty cells
                if (cell->stable && cell->level == 0 && !cell->isSource) {
                    continue;
                }
                
                // Skip stable cells (unless source/drain)
                if (cell->stable && !cell->isSource && !cell->isDrain) {
                    continue;
                }
                
                ProcessWaterCell(x, y, z, doEvap);
                waterUpdateCount++;
                
                // Cap updates per tick
                if (waterUpdateCount >= WATER_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
    
    // Sync water presence to cell wetness on soil (interval-based)
    wetnessSyncAccum += gameDeltaTime;
    if (wetnessSyncAccum >= wetnessSyncInterval) {
        wetnessSyncAccum -= wetnessSyncInterval;
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    int level = waterGrid[z][y][x].level;
                    if (level <= 0) continue;
                    
                    // Set wetness on the soil cell below the water
                    // Water sits in the air cell; soil is at z-1
                    int soilZ = z - 1;
                    if (soilZ < 0) continue;
                    if (!CellIsSolid(grid[soilZ][y][x])) continue;
                    if (!IsWallNatural(x, y, soilZ)) continue;
                    if (!IsSoilMaterial(GetWallMaterial(x, y, soilZ))) continue;
                    
                    // Map water level to wetness: 1→damp, 2-3→wet, 4+→soaked
                    int wetness = (level <= 1) ? 1 : (level <= 3) ? 2 : 3;
                    int current = GET_CELL_WETNESS(x, y, soilZ);
                    if (wetness > current) {
                        SET_CELL_WETNESS(x, y, soilZ, wetness);
                    }
                }
            }
        }
    }
}

// =============================================================================
// WATER FREEZING
// =============================================================================

// Check if water is frozen at a cell
bool IsWaterFrozen(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    return waterGrid[z][y][x].isFrozen;
}

// Freeze water at a cell (any water level can freeze)
void FreezeWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return;
    
    WaterCell* cell = &waterGrid[z][y][x];
    
    // No water to freeze
    if (cell->level == 0) return;
    
    // Already frozen
    if (cell->isFrozen) return;
    
    cell->isFrozen = true;
    cell->stable = true;  // Frozen water doesn't need processing
    DestabilizeWater(x, y, z);  // But neighbors might be affected
}

// Thaw frozen water
void ThawWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return;
    
    WaterCell* cell = &waterGrid[z][y][x];
    
    if (!cell->isFrozen) return;
    
    cell->isFrozen = false;
    cell->stable = false;  // Water can flow again
    DestabilizeWater(x, y, z);
}

// Update water freezing and boiling based on temperature
// Call this after UpdateTemperature() in the game loop
void UpdateWaterFreezing(void) {
    if (!waterEnabled) return;
    if (!temperatureEnabled) return;
    
    // Early exit: no water means nothing to freeze/thaw
    if (waterActiveCells == 0) return;
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                WaterCell* cell = &waterGrid[z][y][x];
                
                // Skip empty cells
                if (cell->level == 0) continue;
                
                int temp = GetTemperature(x, y, z);  // Celsius
                
                if (cell->isFrozen) {
                    // Check if should thaw
                    if (temp > TEMP_WATER_FREEZES) {
                        ThawWater(x, y, z);
                    }
                } else {
                    // Check if should freeze (any water level)
                    if (temp <= TEMP_WATER_FREEZES && cell->level > 0) {
                        FreezeWater(x, y, z);
                    }
                    // Check if should boil (convert to steam)
                    else if (temp >= TEMP_BOILING && cell->level > 0) {
                        // Boil faster at higher temperatures
                        // At 100C: 1 level, at 200C: 2 levels, at 300C: 3 levels, etc.
                        int boilRate = 1 + (temp - TEMP_BOILING) / 100;
                        if (boilRate > cell->level) boilRate = cell->level;
                        if (boilRate > 3) boilRate = 3;  // Cap at 3 per tick
                        
                        int newLevel = cell->level - boilRate;
                        SetWaterLevel(x, y, z, newLevel);
                        GenerateSteamFromBoilingWater(x, y, z, boilRate);
                    }
                }
            }
        }
    }
}

// =============================================================================
// SKY WATER (rain simulation)
// =============================================================================

// Ongoing rain state
static float rainTimeRemaining = 0.0f;
static float rainSpawnAccum = 0.0f;
static int rainIntensity = 0;         // cells per spawn wave
static float rainSpawnInterval = 0.3f; // game-seconds between waves

bool IsRaining(void) { return rainTimeRemaining > 0.0f; }

// Spawn a single wave of rain drops at high z
static void SpawnRainWave(int count) {
    int skyZ = gridDepth - 1;
    if (skyZ < 2) return;
    
    for (int i = 0; i < count; i++) {
        int gx = GetRandomValue(0, gridWidth - 1);
        int gy = GetRandomValue(0, gridHeight - 1);
        
        for (int z = skyZ; z >= skyZ - 1 && z >= 1; z--) {
            if (grid[z][gy][gx] == CELL_AIR) {
                int level = GetRandomValue(4, 7);
                AddWater(gx, gy, z, level);
                break;
            }
        }
    }
}

void SpawnSkyWater(int coverage) {
    if (coverage < 1) coverage = 1;
    if (coverage > 100) coverage = 100;
    
    // Set up ongoing rain: spawns waves over 30 game-seconds
    int totalCells = gridWidth * gridHeight;
    // Cells per wave = coverage% of map / number of waves in duration
    float duration = 30.0f;
    int totalWaves = (int)(duration / rainSpawnInterval);
    if (totalWaves < 1) totalWaves = 1;
    int totalDrops = (totalCells * coverage) / 100;
    
    rainIntensity = totalDrops / totalWaves;
    if (rainIntensity < 1) rainIntensity = 1;
    rainTimeRemaining = duration;
    rainSpawnAccum = 0.0f;
}

void UpdateRain(void) {
    if (rainTimeRemaining <= 0.0f) return;
    
    rainTimeRemaining -= gameDeltaTime;
    rainSpawnAccum += gameDeltaTime;
    
    if (rainSpawnAccum >= rainSpawnInterval) {
        rainSpawnAccum -= rainSpawnInterval;
        SpawnRainWave(rainIntensity);
    }
    
    if (rainTimeRemaining <= 0.0f) {
        rainTimeRemaining = 0.0f;
        rainIntensity = 0;
    }
}

void StopRain(void) {
    rainTimeRemaining = 0.0f;
    rainIntensity = 0;
}

float GetWaterEvapAccum(void) { return waterEvapAccum; }
void SetWaterEvapAccum(float v) { waterEvapAccum = v; }
