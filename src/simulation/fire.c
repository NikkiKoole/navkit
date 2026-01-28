#include "fire.h"
#include "water.h"
#include "smoke.h"
#include "temperature.h"
#include "groundwear.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include <string.h>
#include <stdlib.h>

// Fire grid storage
FireCell fireGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool fireEnabled = true;
int fireUpdateCount = 0;

// Tweakable parameters (game-time based)
float fireSpreadInterval = 0.2f;   // Spread attempt every 0.2 game-seconds
float fireFuelInterval = 0.1f;     // Consume fuel every 0.1 game-seconds
int fireWaterReduction = 25;       // 25% spread chance near water

// Internal accumulators for game-time based updates
static float fireSpreadAccum = 0.0f;
static float fireFuelAccum = 0.0f;

// Initialize fire system
void InitFire(void) {
    ClearFire();
}

// Clear all fire
void ClearFire(void) {
    memset(fireGrid, 0, sizeof(fireGrid));
    fireUpdateCount = 0;
    fireSpreadAccum = 0.0f;
    fireFuelAccum = 0.0f;
}

// Bounds check helper
static inline bool FireInBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth && 
           y >= 0 && y < gridHeight && 
           z >= 0 && z < gridDepth;
}

// Get base fuel for a cell type
int GetBaseFuelForCellType(CellType cell) {
    return CellFuel(cell);
}

// Check if cell can burn (has fuel and not already burned)
static inline bool CanBurn(int x, int y, int z) {
    if (!FireInBounds(x, y, z)) return false;
    
    // Already burned cells can't burn again
    if (HAS_CELL_FLAG(x, y, z, CELL_FLAG_BURNED)) return false;
    
    // Check if cell type has fuel
    CellType cell = grid[z][y][x];
    return GetBaseFuelForCellType(cell) > 0;
}

// Mark cell and neighbors as unstable
void DestabilizeFire(int x, int y, int z) {
    if (FireInBounds(x, y, z)) {
        fireGrid[z][y][x].stable = false;
    }
    
    // 4 horizontal neighbors (orthogonal only)
    if (FireInBounds(x-1, y, z)) fireGrid[z][y][x-1].stable = false;
    if (FireInBounds(x+1, y, z)) fireGrid[z][y][x+1].stable = false;
    if (FireInBounds(x, y-1, z)) fireGrid[z][y-1][x].stable = false;
    if (FireInBounds(x, y+1, z)) fireGrid[z][y+1][x].stable = false;
    
    // Above (for smoke generation later)
    if (FireInBounds(x, y, z+1)) fireGrid[z+1][y][x].stable = false;
}

// Set fire level at a cell
void SetFireLevel(int x, int y, int z, int level) {
    if (!FireInBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > FIRE_MAX_LEVEL) level = FIRE_MAX_LEVEL;
    
    FireCell* cell = &fireGrid[z][y][x];
    int oldLevel = cell->level;
    cell->level = (uint16_t)level;
    
    // Initialize fuel if igniting for the first time
    if (oldLevel == 0 && level > 0 && cell->fuel == 0) {
        CellType cellType = grid[z][y][x];
        cell->fuel = GetBaseFuelForCellType(cellType);
    }
    
    if (oldLevel != level) {
        DestabilizeFire(x, y, z);
    }
}

// Ignite cell at max level if it can burn
void IgniteCell(int x, int y, int z) {
    if (!FireInBounds(x, y, z)) return;
    if (!CanBurn(x, y, z)) return;
    
    FireCell* cell = &fireGrid[z][y][x];
    
    // Initialize fuel from cell type
    CellType cellType = grid[z][y][x];
    cell->fuel = GetBaseFuelForCellType(cellType);
    cell->level = FIRE_MAX_LEVEL;
    
    DestabilizeFire(x, y, z);
}

// Extinguish fire at a cell
void ExtinguishCell(int x, int y, int z) {
    if (!FireInBounds(x, y, z)) return;
    
    FireCell* cell = &fireGrid[z][y][x];
    if (cell->level > 0) {
        cell->level = 0;
        DestabilizeFire(x, y, z);
    }
}

// Set fire source
void SetFireSource(int x, int y, int z, bool isSource) {
    if (!FireInBounds(x, y, z)) return;
    fireGrid[z][y][x].isSource = isSource;
    if (isSource) {
        fireGrid[z][y][x].level = FIRE_MAX_LEVEL;
        fireGrid[z][y][x].fuel = 15;  // Max fuel for sources
        DestabilizeFire(x, y, z);
        // Fire sources also act as heat sources
        SetHeatSource(x, y, z, true);
    } else {
        // Removing fire source also removes heat source
        SetHeatSource(x, y, z, false);
    }
}

// Query functions
int GetFireLevel(int x, int y, int z) {
    if (!FireInBounds(x, y, z)) return 0;
    return fireGrid[z][y][x].level;
}

bool HasFire(int x, int y, int z) {
    return GetFireLevel(x, y, z) > 0;
}

int GetCellFuel(int x, int y, int z) {
    if (!FireInBounds(x, y, z)) return 0;
    return fireGrid[z][y][x].fuel;
}

// Check if any adjacent cell has water
static bool HasAdjacentWater(int x, int y, int z) {
    if (HasWater(x-1, y, z)) return true;
    if (HasWater(x+1, y, z)) return true;
    if (HasWater(x, y-1, z)) return true;
    if (HasWater(x, y+1, z)) return true;
    return false;
}

// Try to spread fire to neighbors
// Called when spread interval is reached - fire level and water affect probability
static bool FireTrySpread(int x, int y, int z) {
    FireCell* cell = &fireGrid[z][y][x];
    if (cell->level < FIRE_MIN_SPREAD_LEVEL) return false;
    
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
    
    bool spread = false;
    
    for (int i = 0; i < 4; i++) {
        int dir = order[i];
        int nx = x + dx[dir];
        int ny = y + dy[dir];
        
        if (!CanBurn(nx, ny, z)) continue;
        
        FireCell* neighbor = &fireGrid[z][ny][nx];
        if (neighbor->level > 0) continue;  // Already burning
        
        // Base spread chance: higher fire level = more likely to spread
        // At level 2 (min): 20% chance, at level 7 (max): 70% chance
        int spreadPercent = 10 + (cell->level * 10);
        
        // Reduce chance if neighbor is near water
        if (HasAdjacentWater(nx, ny, z)) {
            spreadPercent = spreadPercent * fireWaterReduction / 100;
            if (spreadPercent < 5) spreadPercent = 5;
        }
        
        if ((rand() % 100) < spreadPercent) {
            // Ignite neighbor
            CellType cellType = grid[z][ny][nx];
            neighbor->fuel = GetBaseFuelForCellType(cellType);
            neighbor->level = FIRE_MIN_SPREAD_LEVEL;  // Start at low intensity
            
            DestabilizeFire(nx, ny, z);
            spread = true;
        }
    }
    
    return spread;
}

// Process a single fire cell
static bool ProcessFireCell(int x, int y, int z, bool doSpread, bool doFuel) {
    FireCell* cell = &fireGrid[z][y][x];
    bool changed = false;
    
    // Handle sources - always burn at max, never consume fuel
    if (cell->isSource) {
        if (cell->level < FIRE_MAX_LEVEL) {
            cell->level = FIRE_MAX_LEVEL;
            DestabilizeFire(x, y, z);
            changed = true;
        }
        // Sources still spread, generate smoke, and heat
        if (doSpread) FireTrySpread(x, y, z);
        GenerateSmokeFromFire(x, y, z, cell->level);
        ApplyFireHeat(x, y, z, cell->level);
        return changed;
    }
    
    // No fire to process
    if (cell->level == 0) {
        cell->stable = true;
        return false;
    }
    
    // Water extinguishes fire immediately
    if (HasWater(x, y, z)) {
        cell->level = 0;
        cell->fuel = 0;
        DestabilizeFire(x, y, z);
        return true;
    }
    
    // Fuel consumption (on fuel interval)
    if (doFuel) {
        if (cell->fuel > 0) {
            cell->fuel--;
            
            // Reduce fire level as fuel depletes
            if (cell->fuel == 0) {
                // No fuel left - fire dies, mark cell as burned
                cell->level = 0;
                
                // Transform cell based on burnsInto property
                CellType currentCell = grid[z][y][x];
                CellType burnResult = CellBurnsInto(currentCell);
                if (burnResult != currentCell) {
                    grid[z][y][x] = burnResult;
                    // Set high wear on burned ground so it takes time to regrow
                    if (burnResult == CELL_DIRT && z == 0) {
                        wearGrid[y][x] = wearMax;
                    }
                }
                
                SET_CELL_FLAG(x, y, z, CELL_FLAG_BURNED);
                DestabilizeFire(x, y, z);
                return true;
            } else if (cell->fuel <= 2 && cell->level > 3) {
                // Low fuel - reduce intensity
                cell->level = 3;
                changed = true;
            }
        }
    }
    
    // Fire intensity grows if there's fuel
    if (cell->fuel > 2 && cell->level < FIRE_MAX_LEVEL) {
        if ((rand() % 3) == 0) {
            cell->level++;
            changed = true;
        }
    }
    
    // Try to spread (on spread interval)
    if (doSpread && FireTrySpread(x, y, z)) {
        changed = true;
    }
    
    // Generate smoke from active fire and apply heat
    if (cell->level > 0) {
        GenerateSmokeFromFire(x, y, z, cell->level);
        ApplyFireHeat(x, y, z, cell->level);
    }
    
    // Mark stable if nothing is happening
    if (!changed) {
        // Check if neighbors might spread to us or we might spread to them
        bool hasActiveNeighbor = false;
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (FireInBounds(nx, ny, z)) {
                FireCell* neighbor = &fireGrid[z][ny][nx];
                if (neighbor->level > 0 || CanBurn(nx, ny, z)) {
                    hasActiveNeighbor = true;
                    break;
                }
            }
        }
        
        if (!hasActiveNeighbor && cell->level > 0) {
            // Burning alone with no spread potential - still process for fuel consumption
            cell->stable = false;
        } else if (cell->level == 0) {
            cell->stable = true;
        }
    }
    
    return changed;
}

// Main fire update
void UpdateFire(void) {
    if (!fireEnabled) return;
    
    fireUpdateCount = 0;
    
    // Accumulate game time for interval-based updates
    fireSpreadAccum += gameDeltaTime;
    fireFuelAccum += gameDeltaTime;
    
    // Check if we should do spread/fuel this tick
    bool doSpread = fireSpreadAccum >= fireSpreadInterval;
    bool doFuel = fireFuelAccum >= fireFuelInterval;
    
    if (doSpread) fireSpreadAccum -= fireSpreadInterval;
    if (doFuel) fireFuelAccum -= fireFuelInterval;
    
    // Process from bottom to top (like water)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                FireCell* cell = &fireGrid[z][y][x];
                
                // Skip stable empty non-source cells
                if (cell->stable && cell->level == 0 && !cell->isSource) {
                    continue;
                }
                
                // Skip stable cells (unless source)
                if (cell->stable && !cell->isSource) {
                    continue;
                }
                
                ProcessFireCell(x, y, z, doSpread, doFuel);
                fireUpdateCount++;
                
                // Cap updates per tick
                if (fireUpdateCount >= FIRE_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
}
