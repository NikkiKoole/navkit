#include "steam.h"
#include "water.h"
#include "temperature.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <string.h>
#include <stdlib.h>

// Steam grid storage
SteamCell steamGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool steamEnabled = true;
int steamUpdateCount = 0;

// Tweakable parameters (all in Celsius now!)
int steamRiseChance = 1;            // 1 in 1 = always rise (steam is energetic)
int steamSpreadChance = 2;          // 1 in 2 chance to spread horizontally
int steamCondensationTemp = 60;     // 60C - steam lingers longer before condensing
int steamGenerationTemp = 100;      // 100C (boiling point)

// Internal tick counter
static int steamTick = 0;

// Initialize steam system
void InitSteam(void) {
    ClearSteam();
}

// Clear all steam
void ClearSteam(void) {
    memset(steamGrid, 0, sizeof(steamGrid));
    steamUpdateCount = 0;
    steamTick = 0;
}

// Bounds check helper
static inline bool SteamInBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth && 
           y >= 0 && y < gridHeight && 
           z >= 0 && z < gridDepth;
}

// Check if steam can exist in a cell (not blocked by walls)
static inline bool CanHoldSteam(int x, int y, int z) {
    if (!SteamInBounds(x, y, z)) return false;
    CellType cell = grid[z][y][x];
    return CellAllowsFluids(cell);
}

// Mark cell and neighbors as unstable
void DestabilizeSteam(int x, int y, int z) {
    if (SteamInBounds(x, y, z)) {
        steamGrid[z][y][x].stable = false;
    }
    
    // 4 horizontal neighbors
    if (SteamInBounds(x-1, y, z)) steamGrid[z][y][x-1].stable = false;
    if (SteamInBounds(x+1, y, z)) steamGrid[z][y][x+1].stable = false;
    if (SteamInBounds(x, y-1, z)) steamGrid[z][y-1][x].stable = false;
    if (SteamInBounds(x, y+1, z)) steamGrid[z][y+1][x].stable = false;
    
    // Above and below
    if (SteamInBounds(x, y, z-1)) steamGrid[z-1][y][x].stable = false;
    if (SteamInBounds(x, y, z+1)) steamGrid[z+1][y][x].stable = false;
}

// Set steam level at a cell
void SetSteamLevel(int x, int y, int z, int level) {
    if (!SteamInBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > STEAM_MAX_LEVEL) level = STEAM_MAX_LEVEL;
    
    int oldLevel = steamGrid[z][y][x].level;
    steamGrid[z][y][x].level = (uint8_t)level;
    
    if (oldLevel != level) {
        DestabilizeSteam(x, y, z);
    }
}

// Add steam to a cell
void AddSteam(int x, int y, int z, int amount) {
    if (!SteamInBounds(x, y, z)) return;
    int newLevel = steamGrid[z][y][x].level + amount;
    SetSteamLevel(x, y, z, newLevel);
}

// Query functions
int GetSteamLevel(int x, int y, int z) {
    if (!SteamInBounds(x, y, z)) return 0;
    return steamGrid[z][y][x].level;
}

bool HasSteam(int x, int y, int z) {
    return GetSteamLevel(x, y, z) > 0;
}

// Generate steam from boiling water (called by water system or externally)
void GenerateSteamFromBoilingWater(int x, int y, int z, int amount) {
    if (!SteamInBounds(x, y, z)) return;
    if (amount <= 0) return;
    
    // Add steam to current cell
    AddSteam(x, y, z, amount);
    
    // Also add to cell above if possible (steam rises immediately)
    if (CanHoldSteam(x, y, z + 1)) {
        AddSteam(x, y, z + 1, amount);
    }
}

// Phase 1: RISING - Steam moves up if there's space above
static int SteamTryRise(int x, int y, int z) {
    SteamCell* src = &steamGrid[z][y][x];
    
    // At top of world - steam escapes into the sky
    if (z >= gridDepth - 1) {
        if (src->level > 0 && (rand() % steamRiseChance) == 0) {
            int escaped = 1;
            if (escaped > src->level) escaped = src->level;
            src->level -= escaped;
            return escaped;
        }
        return 0;
    }
    
    if (!CanHoldSteam(x, y, z + 1)) return 0;  // Blocked above
    
    SteamCell* dst = &steamGrid[z+1][y][x];
    
    if (src->level == 0) return 0;
    
    // Roll for rise chance
    if ((rand() % steamRiseChance) != 0) return 0;
    
    int space = STEAM_MAX_LEVEL - dst->level;
    if (space <= 0) {
        // Cell above is full - pressure builds (Phase 2)
        return 0;
    }
    
    int flow = 1;  // Move 1 unit at a time
    if (flow > src->level) flow = src->level;
    if (flow > space) flow = space;
    
    src->level -= flow;
    dst->level += flow;
    
    // Steam carries heat - warm up destination cell
    int srcTemp = GetTemperature(x, y, z);
    int dstTemp = GetTemperature(x, y, z + 1);
    if (srcTemp > dstTemp) {
        // Transfer 75% of heat difference with the steam
        int heatTransfer = (srcTemp - dstTemp) * 3 / 4;
        if (heatTransfer > 0) {
            SetTemperature(x, y, z + 1, dstTemp + heatTransfer);
            DestabilizeTemperature(x, y, z + 1);
        }
    }
    
    DestabilizeSteam(x, y, z);
    DestabilizeSteam(x, y, z + 1);
    
    return flow;
}

// Phase 2: SPREADING - Equalize steam levels with horizontal neighbors
// (same logic as smoke - stays cohesive)
static bool SteamTrySpread(int x, int y, int z) {
    SteamCell* cell = &steamGrid[z][y][x];
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
        
        if (!CanHoldSteam(nx, ny, z)) continue;
        
        SteamCell* neighbor = &steamGrid[z][ny][nx];
        int diff = cell->level - neighbor->level;
        
        // Spread to lower neighbors
        if (diff >= 2) {
            cell->level -= 1;
            neighbor->level += 1;
            
            DestabilizeSteam(x, y, z);
            DestabilizeSteam(nx, ny, z);
            moved = true;
            
            if (cell->level <= 1) break;
        } else if (diff == 1 && cell->level > 1) {
            cell->level -= 1;
            neighbor->level += 1;
            
            DestabilizeSteam(x, y, z);
            DestabilizeSteam(nx, ny, z);
            moved = true;
            break;  // Only give to one neighbor when diff=1
        }
    }
    
    return moved;
}

// Phase 3: CONDENSATION - Steam below threshold turns back to water
static bool SteamTryCondense(int x, int y, int z) {
    SteamCell* cell = &steamGrid[z][y][x];
    if (cell->level == 0) return false;
    
    // Only condense sometimes (steam lingers)
    if ((rand() % 3) != 0) return false;
    
    // Check temperature at this cell (Celsius)
    int temp = GetTemperature(x, y, z);
    
    // If temperature is below condensation point, steam condenses
    if (temp < steamCondensationTemp) {
        // Convert some steam to water
        int condenseAmount = 1;
        
        // Find where water should go - falls down until it hits something
        int waterZ = z;
        while (waterZ > 0 && CanHoldSteam(x, y, waterZ - 1) && !HasWater(x, y, waterZ - 1)) {
            waterZ--;
        }
        
        // Add water at the destination (or current cell if can't fall)
        // Water level is proportional to steam condensed
        AddWater(x, y, waterZ, condenseAmount);
        
        // Remove steam
        cell->level -= condenseAmount;
        
        DestabilizeSteam(x, y, z);
        
        // Condensation releases heat (warms the cell slightly)
        // This is latent heat release - could implement later
        
        return true;
    }
    
    return false;
}

// Process a single steam cell
static bool ProcessSteamCell(int x, int y, int z) {
    SteamCell* cell = &steamGrid[z][y][x];
    bool moved = false;
    
    // No steam to process
    if (cell->level == 0) {
        cell->stable = true;
        return false;
    }
    
    // Phase 1: Try to rise (highest priority for steam)
    int rose = SteamTryRise(x, y, z);
    if (rose > 0) moved = true;
    
    // Phase 2: Try to spread horizontally (if we still have steam)
    if (cell->level > 0) {
        if (SteamTrySpread(x, y, z)) moved = true;
    }
    
    // Phase 3: Try to condense (if temperature is low enough)
    if (cell->level > 0) {
        if (SteamTryCondense(x, y, z)) moved = true;
    }
    
    // Mark stable if nothing is happening
    if (!moved && cell->level == 0) {
        cell->stable = true;
    }
    
    return moved;
}

// Main steam update - process from BOTTOM to TOP (steam rises)
void UpdateSteam(void) {
    if (!steamEnabled) return;
    
    steamUpdateCount = 0;
    steamTick++;
    
    // Alternate scan direction each tick to avoid directional bias
    bool reverseX = (steamTick & 1);
    bool reverseY = (steamTick & 2);
    
    // Process from bottom to top (steam rises, so process low cells first)
    for (int z = 0; z < gridDepth; z++) {
        for (int yi = 0; yi < gridHeight; yi++) {
            int y = reverseY ? (gridHeight - 1 - yi) : yi;
            for (int xi = 0; xi < gridWidth; xi++) {
                int x = reverseX ? (gridWidth - 1 - xi) : xi;
                SteamCell* cell = &steamGrid[z][y][x];
                
                // Skip stable empty cells
                if (cell->stable && cell->level == 0) {
                    continue;
                }
                
                // Skip stable cells
                if (cell->stable) {
                    continue;
                }
                
                ProcessSteamCell(x, y, z);
                steamUpdateCount++;
                
                // Cap updates per tick
                if (steamUpdateCount >= STEAM_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
}
