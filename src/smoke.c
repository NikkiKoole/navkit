#include "smoke.h"
#include "grid.h"
#include <string.h>
#include <stdlib.h>

// Smoke grid storage
SmokeCell smokeGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool smokeEnabled = true;
int smokeUpdateCount = 0;

// Tweakable parameters (defaults)
int smokeRiseChance = 2;         // 1 in 2 chance to rise per tick (fast)
int smokeDissipationRate = 10;   // Dissipate every 10 ticks
int smokeGenerationRate = 3;     // Fire level / 3 = smoke generated

// Internal tick counter
static int smokeTick = 0;

// BFS queue for pressure propagation (fill down)
typedef struct {
    int x, y, z;
} SmokePos;

static SmokePos pressureQueue[SMOKE_PRESSURE_SEARCH_LIMIT];
static bool pressureVisited[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize smoke system
void InitSmoke(void) {
    ClearSmoke();
}

// Clear all smoke
void ClearSmoke(void) {
    memset(smokeGrid, 0, sizeof(smokeGrid));
    smokeUpdateCount = 0;
    smokeTick = 0;
}

// Bounds check helper
static inline bool InBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth && 
           y >= 0 && y < gridHeight && 
           z >= 0 && z < gridDepth;
}

// Check if smoke can exist in a cell (not a wall)
static inline bool CanHoldSmoke(int x, int y, int z) {
    if (!InBounds(x, y, z)) return false;
    CellType cell = grid[z][y][x];
    return cell != CELL_WALL;
}

// Mark cell and neighbors as unstable
void DestabilizeSmoke(int x, int y, int z) {
    if (InBounds(x, y, z)) {
        smokeGrid[z][y][x].stable = false;
    }
    
    // 4 horizontal neighbors
    if (InBounds(x-1, y, z)) smokeGrid[z][y][x-1].stable = false;
    if (InBounds(x+1, y, z)) smokeGrid[z][y][x+1].stable = false;
    if (InBounds(x, y-1, z)) smokeGrid[z][y-1][x].stable = false;
    if (InBounds(x, y+1, z)) smokeGrid[z][y+1][x].stable = false;
    
    // Above and below
    if (InBounds(x, y, z-1)) smokeGrid[z-1][y][x].stable = false;
    if (InBounds(x, y, z+1)) smokeGrid[z+1][y][x].stable = false;
}

// Set smoke level at a cell
void SetSmokeLevel(int x, int y, int z, int level) {
    if (!InBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > SMOKE_MAX_LEVEL) level = SMOKE_MAX_LEVEL;
    
    int oldLevel = smokeGrid[z][y][x].level;
    smokeGrid[z][y][x].level = (uint8_t)level;
    
    if (oldLevel != level) {
        DestabilizeSmoke(x, y, z);
    }
}

// Add smoke to a cell
void AddSmoke(int x, int y, int z, int amount) {
    if (!InBounds(x, y, z)) return;
    int newLevel = smokeGrid[z][y][x].level + amount;
    SetSmokeLevel(x, y, z, newLevel);
}

// Query functions
int GetSmokeLevel(int x, int y, int z) {
    if (!InBounds(x, y, z)) return 0;
    return smokeGrid[z][y][x].level;
}

bool HasSmoke(int x, int y, int z) {
    return GetSmokeLevel(x, y, z) > 0;
}

// Generate smoke from fire
void GenerateSmokeFromFire(int x, int y, int z, int fireLevel) {
    if (!InBounds(x, y, z)) return;
    if (fireLevel <= 0) return;
    
    // Generate smoke based on fire level
    int smokeAmount = fireLevel / smokeGenerationRate;
    if (smokeAmount < 1 && fireLevel > 0) smokeAmount = 1;
    
    // Add smoke to current cell and cell above
    AddSmoke(x, y, z, smokeAmount);
    
    // Smoke rises - add more to cell above if possible
    if (CanHoldSmoke(x, y, z + 1)) {
        AddSmoke(x, y, z + 1, smokeAmount);
        smokeGrid[z + 1][y][x].pressureSourceZ = z;  // Track origin
    }
}

// Phase 1: RISING - Smoke moves up if there's space above
static int TryRise(int x, int y, int z) {
    if (z >= gridDepth - 1) return 0;  // At top
    if (!CanHoldSmoke(x, y, z + 1)) return 0;  // Blocked above
    
    SmokeCell* src = &smokeGrid[z][y][x];
    SmokeCell* dst = &smokeGrid[z+1][y][x];
    
    if (src->level == 0) return 0;
    
    // Roll for rise chance
    if ((rand() % smokeRiseChance) != 0) return 0;
    
    int space = SMOKE_MAX_LEVEL - dst->level;
    if (space <= 0) {
        // Cell above is full - create pressure (smoke wants to rise but can't)
        src->hasPressure = true;
        if (src->pressureSourceZ == 0) src->pressureSourceZ = z;
        return 0;
    }
    
    int flow = 1;  // Move 1 unit at a time for gradual rising
    if (flow > src->level) flow = src->level;
    if (flow > space) flow = space;
    
    src->level -= flow;
    dst->level += flow;
    
    // Track pressure source
    if (dst->pressureSourceZ == 0 || dst->pressureSourceZ > z) {
        dst->pressureSourceZ = z;
    }
    
    // If destination is now full, mark for pressure
    if (dst->level == SMOKE_MAX_LEVEL) {
        dst->hasPressure = true;
    }
    
    DestabilizeSmoke(x, y, z);
    DestabilizeSmoke(x, y, z + 1);
    
    return flow;
}

// Phase 2: SPREADING - Equalize smoke levels with horizontal neighbors
static bool TrySpread(int x, int y, int z) {
    SmokeCell* cell = &smokeGrid[z][y][x];
    if (cell->level == 0) return false;
    
    // Orthogonal neighbors - randomize order
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    // Fisher-Yates shuffle
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
        
        if (!CanHoldSmoke(nx, ny, z)) continue;
        
        SmokeCell* neighbor = &smokeGrid[z][ny][nx];
        int diff = cell->level - neighbor->level;
        
        // Spread to lower neighbors
        if (diff >= 2) {
            cell->level -= 1;
            neighbor->level += 1;
            
            DestabilizeSmoke(x, y, z);
            DestabilizeSmoke(nx, ny, z);
            moved = true;
            
            if (cell->level <= 1) break;
        } else if (diff == 1 && cell->level > 1) {
            cell->level -= 1;
            neighbor->level += 1;
            
            DestabilizeSmoke(x, y, z);
            DestabilizeSmoke(nx, ny, z);
            moved = true;
            break;  // Only give to one neighbor when diff=1
        }
    }
    
    return moved;
}

// Phase 3: FILL DOWN - When trapped, smoke fills downward (inverse pressure)
static bool TryFillDown(int x, int y, int z) {
    SmokeCell* cell = &smokeGrid[z][y][x];
    
    // Need full smoke with pressure to fill down
    if (cell->level < SMOKE_MAX_LEVEL) return false;
    if (!cell->hasPressure) return false;
    
    int minZ = cell->pressureSourceZ;
    if (minZ >= z) minZ = 0;  // Can fill all the way down if no source tracked
    
    // BFS to find nearest non-full cell through full cells (going down and horizontal)
    memset(pressureVisited, 0, sizeof(pressureVisited));
    
    int queueHead = 0;
    int queueTail = 0;
    
    int dx[] = {-1, 1, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0};
    int dz[] = {0, 0, 0, 0, -1};  // Include down direction
    
    pressureVisited[z][y][x] = true;
    
    // Add initial neighbors to queue (prioritize going down)
    for (int i = 4; i >= 0; i--) {  // Start with down
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        
        if (nz < minZ) continue;
        if (!CanHoldSmoke(nx, ny, nz)) continue;
        if (pressureVisited[nz][ny][nx]) continue;
        
        pressureVisited[nz][ny][nx] = true;
        pressureQueue[queueTail++] = (SmokePos){nx, ny, nz};
        
        if (queueTail >= SMOKE_PRESSURE_SEARCH_LIMIT) break;
    }
    
    // BFS through full cells looking for non-full cell
    while (queueHead < queueTail) {
        SmokePos pos = pressureQueue[queueHead++];
        SmokeCell* current = &smokeGrid[pos.z][pos.y][pos.x];
        
        // Found a non-full cell - push smoke here
        if (current->level < SMOKE_MAX_LEVEL) {
            int space = SMOKE_MAX_LEVEL - current->level;
            int transfer = 1;
            if (transfer > space) transfer = space;
            if (transfer > cell->level) transfer = cell->level;
            
            if (transfer > 0) {
                cell->level -= transfer;
                current->level += transfer;
                
                DestabilizeSmoke(x, y, z);
                DestabilizeSmoke(pos.x, pos.y, pos.z);
                
                // Clear pressure if we're no longer full
                if (cell->level < SMOKE_MAX_LEVEL) {
                    cell->hasPressure = false;
                }
                
                return true;
            }
        }
        
        // Cell is full - continue searching through it
        if (current->level >= SMOKE_MAX_LEVEL) {
            for (int i = 4; i >= 0; i--) {
                int nx = pos.x + dx[i];
                int ny = pos.y + dy[i];
                int nz = pos.z + dz[i];
                
                if (nz < minZ) continue;
                if (!CanHoldSmoke(nx, ny, nz)) continue;
                if (pressureVisited[nz][ny][nx]) continue;
                
                pressureVisited[nz][ny][nx] = true;
                
                if (queueTail < SMOKE_PRESSURE_SEARCH_LIMIT) {
                    pressureQueue[queueTail++] = (SmokePos){nx, ny, nz};
                }
            }
        }
    }
    
    return false;
}

// Process a single smoke cell
static bool ProcessSmokeCell(int x, int y, int z) {
    SmokeCell* cell = &smokeGrid[z][y][x];
    bool moved = false;
    
    // No smoke to process
    if (cell->level == 0) {
        cell->stable = true;
        cell->hasPressure = false;
        return false;
    }
    
    // Phase 1: Try to rise (highest priority for smoke)
    int rose = TryRise(x, y, z);
    if (rose > 0) moved = true;
    
    // Phase 2: Try to spread horizontally (if we still have smoke)
    if (cell->level > 0) {
        if (TrySpread(x, y, z)) moved = true;
    }
    
    // Phase 3: Try fill down (if full and pressurized)
    if (cell->level >= SMOKE_MAX_LEVEL && cell->hasPressure) {
        if (TryFillDown(x, y, z)) moved = true;
    }
    
    // Dissipation: smoke gradually fades
    if (smokeTick % smokeDissipationRate == 0 && cell->level > 0) {
        // Smoke at lower z-levels (in open air) dissipates faster
        // Smoke at higher z-levels or trapped dissipates slower
        bool isTrapped = cell->hasPressure || (z > 0 && !CanHoldSmoke(x, y, z + 1));
        
        if (!isTrapped || (rand() % 3) == 0) {
            cell->level--;
            if (cell->level == 0) {
                cell->hasPressure = false;
                cell->pressureSourceZ = 0;
            }
            DestabilizeSmoke(x, y, z);
            moved = true;
        }
    }
    
    // Clear pressure if we're no longer full
    if (cell->level < SMOKE_MAX_LEVEL) {
        cell->hasPressure = false;
    }
    
    // Mark stable if nothing is happening
    if (!moved && cell->level == 0) {
        cell->stable = true;
    }
    
    return moved;
}

// Main smoke update - process from TOP to BOTTOM (opposite of water)
void UpdateSmoke(void) {
    if (!smokeEnabled) return;
    
    smokeUpdateCount = 0;
    smokeTick++;
    
    // Process from top to bottom (smoke rises, so process high cells first)
    for (int z = gridDepth - 1; z >= 0; z--) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                SmokeCell* cell = &smokeGrid[z][y][x];
                
                // Skip stable empty cells
                if (cell->stable && cell->level == 0) {
                    continue;
                }
                
                // Skip stable cells
                if (cell->stable) {
                    continue;
                }
                
                ProcessSmokeCell(x, y, z);
                smokeUpdateCount++;
                
                // Cap updates per tick
                if (smokeUpdateCount >= SMOKE_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
}
