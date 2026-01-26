#include "water.h"
#include "grid.h"
#include <string.h>
#include <stdlib.h>

// Water grid storage
WaterCell waterGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool waterEnabled = true;
int waterUpdateCount = 0;

// BFS queue for pressure propagation
typedef struct {
    int x, y, z;
} WaterPos;

static WaterPos pressureQueue[WATER_PRESSURE_SEARCH_LIMIT];
static bool pressureVisited[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize water system
void InitWater(void) {
    ClearWater();
}

// Clear all water
void ClearWater(void) {
    memset(waterGrid, 0, sizeof(waterGrid));
    waterUpdateCount = 0;
}

// Bounds check helper
static inline bool InBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth && 
           y >= 0 && y < gridHeight && 
           z >= 0 && z < gridDepth;
}

// Check if water can exist in a cell
static inline bool CanHoldWater(int x, int y, int z) {
    if (!InBounds(x, y, z)) return false;
    CellType cell = grid[z][y][x];
    return cell != CELL_WALL;
}

// Mark cell and neighbors as unstable
void DestabilizeWater(int x, int y, int z) {
    if (InBounds(x, y, z)) {
        waterGrid[z][y][x].stable = false;
    }
    
    // 4 horizontal neighbors (orthogonal only, like DF pressure)
    if (InBounds(x-1, y, z)) waterGrid[z][y][x-1].stable = false;
    if (InBounds(x+1, y, z)) waterGrid[z][y][x+1].stable = false;
    if (InBounds(x, y-1, z)) waterGrid[z][y-1][x].stable = false;
    if (InBounds(x, y+1, z)) waterGrid[z][y+1][x].stable = false;
    
    // Above and below
    if (InBounds(x, y, z-1)) waterGrid[z-1][y][x].stable = false;
    if (InBounds(x, y, z+1)) waterGrid[z+1][y][x].stable = false;
}

// Set water level at a cell
void SetWaterLevel(int x, int y, int z, int level) {
    if (!InBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > WATER_MAX_LEVEL) level = WATER_MAX_LEVEL;
    
    int oldLevel = waterGrid[z][y][x].level;
    waterGrid[z][y][x].level = (uint8_t)level;
    
    if (oldLevel != level) {
        DestabilizeWater(x, y, z);
    }
}

// Add water to a cell
void AddWater(int x, int y, int z, int amount) {
    if (!InBounds(x, y, z)) return;
    int newLevel = waterGrid[z][y][x].level + amount;
    SetWaterLevel(x, y, z, newLevel);
}

// Remove water from a cell
void RemoveWater(int x, int y, int z, int amount) {
    if (!InBounds(x, y, z)) return;
    int newLevel = waterGrid[z][y][x].level - amount;
    SetWaterLevel(x, y, z, newLevel);
}

// Set water source
void SetWaterSource(int x, int y, int z, bool isSource) {
    if (!InBounds(x, y, z)) return;
    waterGrid[z][y][x].isSource = isSource;
    if (isSource) {
        DestabilizeWater(x, y, z);
    }
}

// Set water drain
void SetWaterDrain(int x, int y, int z, bool isDrain) {
    if (!InBounds(x, y, z)) return;
    waterGrid[z][y][x].isDrain = isDrain;
    if (isDrain) {
        DestabilizeWater(x, y, z);
    }
}

// Get water level
int GetWaterLevel(int x, int y, int z) {
    if (!InBounds(x, y, z)) return 0;
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

// Get speed multiplier for water (DF-style 1-7 scale)
float GetWaterSpeedMultiplier(int x, int y, int z) {
    int level = GetWaterLevel(x, y, z);
    if (level == 0) return 1.0f;
    
    // 1-2: shallow, slight slowdown
    // 3-4: medium, noticeable slowdown
    // 5-7: deep, major slowdown
    if (level <= 2) return 0.85f;
    if (level <= 4) return 0.6f;
    return 0.35f;
}

// =============================================================================
// DF-STYLE WATER SIMULATION
// Priority: 1. Falling  2. Spreading  3. Pressure
// =============================================================================

// Phase 1: FALLING - Water drops down if cell below isn't full
// Returns amount of water that fell
static int TryFall(int x, int y, int z) {
    if (z <= 0) return 0;  // Already at bottom
    if (!CanHoldWater(x, y, z - 1)) return 0;  // Wall below
    
    WaterCell* src = &waterGrid[z][y][x];
    WaterCell* dst = &waterGrid[z-1][y][x];
    
    if (src->level == 0) return 0;
    
    int space = WATER_MAX_LEVEL - dst->level;
    if (space <= 0) return 0;  // Cell below is full
    
    int flow = src->level;
    if (flow > space) flow = space;
    
    src->level -= flow;
    dst->level += flow;
    
    // Falling water onto full water creates pressure
    // The pressure source z is where the water fell FROM
    if (dst->level == WATER_MAX_LEVEL) {
        dst->hasPressure = true;
        dst->pressureSourceZ = z;  // Water came from z, can rise to z-1
    }
    
    DestabilizeWater(x, y, z);
    DestabilizeWater(x, y, z - 1);
    
    return flow;
}

// Phase 2: SPREADING - Equalize water levels with orthogonal neighbors
// Returns true if water moved
static bool TrySpread(int x, int y, int z) {
    WaterCell* cell = &waterGrid[z][y][x];
    if (cell->level == 0) return false;
    
    // Orthogonal neighbors only (DF-style)
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    bool moved = false;
    
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        
        if (!CanHoldWater(nx, ny, z)) continue;
        
        WaterCell* neighbor = &waterGrid[z][ny][nx];
        int diff = cell->level - neighbor->level;
        
        // Only flow to lower neighbors
        if (diff >= 2) {  // Need at least 2 difference to transfer 1
            // Transfer half the difference (rounds down)
            int transfer = diff / 2;
            if (transfer < 1) transfer = 1;
            
            cell->level -= transfer;
            neighbor->level += transfer;
            
            DestabilizeWater(x, y, z);
            DestabilizeWater(nx, ny, z);
            moved = true;
            
            // Stop if we're empty
            if (cell->level == 0) break;
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
    
    int maxZ = cell->pressureSourceZ - 1;  // DF rule: can rise to source - 1
    if (maxZ < 0) maxZ = 0;
    
    // BFS to find nearest non-full cell through full cells
    // Only orthogonal movement (no diagonals for pressure)
    memset(pressureVisited, 0, sizeof(pressureVisited));
    
    int queueHead = 0;
    int queueTail = 0;
    
    // Start from neighbors of this cell
    int dx[] = {-1, 1, 0, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0, 0};
    int dz[] = {0, 0, 0, 0, -1, 1};  // Can also go up/down
    
    pressureVisited[z][y][x] = true;
    
    // Add initial neighbors to queue
    for (int i = 0; i < 6; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        
        // Can't go above max pressure height
        if (nz > maxZ) continue;
        if (!CanHoldWater(nx, ny, nz)) continue;
        if (pressureVisited[nz][ny][nx]) continue;
        
        pressureVisited[nz][ny][nx] = true;
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
            int transfer = 1;  // Pressure moves 1 unit at a time
            if (transfer > space) transfer = space;
            if (transfer > cell->level) transfer = cell->level;
            
            if (transfer > 0) {
                cell->level -= transfer;
                current->level += transfer;
                
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
                if (pressureVisited[nz][ny][nx]) continue;
                
                pressureVisited[nz][ny][nx] = true;
                
                if (queueTail < WATER_PRESSURE_SEARCH_LIMIT) {
                    pressureQueue[queueTail++] = (WaterPos){nx, ny, nz};
                }
            }
        }
    }
    
    return false;
}

// Process a single water cell with DF-style rules
static bool ProcessWaterCell(int x, int y, int z) {
    WaterCell* cell = &waterGrid[z][y][x];
    bool moved = false;
    
    // Handle sources - refill to max and add pressure
    if (cell->isSource) {
        if (cell->level < WATER_MAX_LEVEL) {
            cell->level = WATER_MAX_LEVEL;
            cell->hasPressure = true;
            cell->pressureSourceZ = z;  // Source creates pressure at its own level
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
        if (TrySpread(x, y, z)) moved = true;
    }
    
    // Phase 3: Try pressure propagation (if full and pressurized)
    if (cell->level >= WATER_MAX_LEVEL && cell->hasPressure) {
        if (TryPressure(x, y, z)) moved = true;
    }
    
    // Evaporation: level 1 water has a chance to disappear
    if (cell->level == 1 && !cell->isSource) {
        if ((rand() % WATER_EVAP_CHANCE) == 0) {
            cell->level = 0;
            cell->hasPressure = false;
            DestabilizeWater(x, y, z);
            moved = true;
        }
    }
    
    // Clear pressure if we're no longer full
    if (cell->level < WATER_MAX_LEVEL) {
        cell->hasPressure = false;
    }
    
    // Mark stable if nothing moved
    if (!moved) {
        cell->stable = true;
    }
    
    return moved;
}

// Main water update - process all unstable cells
void UpdateWater(void) {
    if (!waterEnabled) return;
    
    waterUpdateCount = 0;
    
    // Process from bottom to top (so falling water settles properly)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                WaterCell* cell = &waterGrid[z][y][x];
                
                // Skip stable empty cells
                if (cell->stable && cell->level == 0 && !cell->isSource) {
                    continue;
                }
                
                // Skip stable cells (unless source/drain)
                if (cell->stable && !cell->isSource && !cell->isDrain) {
                    continue;
                }
                
                ProcessWaterCell(x, y, z);
                waterUpdateCount++;
                
                // Cap updates per tick
                if (waterUpdateCount >= WATER_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
}
