#include "steam.h"
#include "water.h"
#include "temperature.h"
#include "sim_presence.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include <string.h>
#include <stdlib.h>

// Steam grid storage
SteamCell steamGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool steamEnabled = true;
int steamUpdateCount = 0;

// Tweakable parameters (game-time based, temps in Celsius)
float steamRiseInterval = 0.5f;     // Rise attempt every 0.5 game-seconds
int steamCondensationTemp = 60;     // 60C - steam lingers longer before condensing
int steamGenerationTemp = 100;      // 100C (boiling point)
int steamCondensationChance = 3;    // 1 in N ticks attempts condensation
int steamRiseFlow = 1;              // Units of steam that rise per attempt

// Internal accumulators for game-time
static float steamRiseAccum = 0.0f;

// Track which cells have received risen steam this tick (prevents cascading through z-levels)
static uint16_t steamRiseGeneration = 0;
static uint16_t steamHasRisen[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize steam system
void InitSteam(void) {
    ClearSteam();
}

// Clear all steam
void ClearSteam(void) {
    memset(steamGrid, 0, sizeof(steamGrid));
    steamUpdateCount = 0;
    steamRiseAccum = 0.0f;
    steamActiveCells = 0;
}

// Reset accumulators (call after loading steam grid from save)
void ResetSteamAccumulators(void) {
    steamRiseAccum = 0.0f;
    
    // Also destabilize all cells so they get processed after load
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                steamGrid[z][y][x].stable = false;
            }
        }
    }
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
    
    // Update presence tracking
    if (oldLevel == 0 && level > 0) {
        steamActiveCells++;
    } else if (oldLevel > 0 && level == 0) {
        steamActiveCells--;
    }
    
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
        if (src->level > 0) {
            int escaped = 1;
            if (escaped > src->level) escaped = src->level;
            int newLevel = src->level - escaped;
            // Track counter transition when steam escapes
            if (src->level > 0 && newLevel == 0) {
                steamActiveCells--;
            }
            src->level = newLevel;
            return escaped;
        }
        return 0;
    }
    
    if (!CanHoldSteam(x, y, z + 1)) return 0;  // Blocked above
    
    SteamCell* dst = &steamGrid[z+1][y][x];
    
    if (src->level == 0) return 0;
    
    // Don't rise if this cell's steam already rose into it this tick
    // This prevents steam from cascading through multiple z-levels in one tick
    if (steamHasRisen[z][y][x] == steamRiseGeneration) {
        return 0;
    }
    
    int space = STEAM_MAX_LEVEL - dst->level;
    if (space <= 0) {
        // Cell above is full - pressure builds (Phase 2)
        return 0;
    }
    
    int flow = steamRiseFlow;
    if (flow > src->level) flow = src->level;
    if (flow > space) flow = space;
    
    // Track counter transitions
    bool srcWasActive = src->level > 0;
    bool dstWasActive = dst->level > 0;
    
    src->level -= flow;
    dst->level += flow;
    
    // Update counters for transitions
    if (srcWasActive && src->level == 0) {
        steamActiveCells--;
    }
    if (!dstWasActive && dst->level > 0) {
        steamActiveCells++;
    }
    
    // Mark destination as having received risen steam this tick
    // This prevents the steam from immediately rising again when z+1 is processed
    steamHasRisen[z+1][y][x] = steamRiseGeneration;
    
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
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;
            
            // Track neighbor becoming active
            if (neighborWasEmpty) {
                steamActiveCells++;
            }
            
            DestabilizeSteam(x, y, z);
            DestabilizeSteam(nx, ny, z);
            moved = true;
            
            if (cell->level <= 1) break;
        } else if (diff == 1 && cell->level > 1) {
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;
            
            // Track neighbor becoming active
            if (neighborWasEmpty) {
                steamActiveCells++;
            }
            
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
    if (steamCondensationChance > 1 && (rand() % steamCondensationChance) != 0) return false;
    
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
        
        // Remove steam - track counter transition
        int newLevel = cell->level - condenseAmount;
        if (cell->level > 0 && newLevel == 0) {
            steamActiveCells--;
        }
        cell->level = newLevel;
        
        DestabilizeSteam(x, y, z);
        
        // Condensation releases heat (warms the cell slightly)
        // This is latent heat release - could implement later
        
        return true;
    }
    
    return false;
}

// Process a single steam cell
// doRise: interval has elapsed for rising
static bool ProcessSteamCell(int x, int y, int z, bool doRise) {
    SteamCell* cell = &steamGrid[z][y][x];
    bool moved = false;
    
    // No steam to process
    if (cell->level == 0) {
        cell->stable = true;
        return false;
    }
    
    // Phase 1: Try to rise (highest priority for steam)
    if (doRise) {
        int rose = SteamTryRise(x, y, z);
        if (rose > 0) moved = true;
    }
    
    // Phase 2: Try to spread horizontally (if we still have steam)
    // Spreading happens every tick for smooth equalization
    if (cell->level > 0) {
        if (SteamTrySpread(x, y, z)) moved = true;
    }
    
    // Phase 3: Try to condense (if temperature is low enough)
    // Condensation happens every tick, controlled by temperature
    if (cell->level > 0) {
        if (SteamTryCondense(x, y, z)) moved = true;
    }
    
    // Mark stable if nothing is happening
    if (!moved && cell->level == 0) {
        cell->stable = true;
    }
    
    return moved;
}

// Internal tick counter for scan direction alternation
static int steamTick = 0;

// Main steam update - process from BOTTOM to TOP (steam rises)
void UpdateSteam(void) {
    if (!steamEnabled) return;
    
    // Early exit: no steam activity at all
    if (steamActiveCells == 0) {
        steamUpdateCount = 0;
        return;
    }
    
    steamUpdateCount = 0;
    steamTick++;
    
    // Accumulate game time for interval-based actions
    steamRiseAccum += gameDeltaTime;
    
    // Check if rise interval has elapsed
    bool doRise = steamRiseAccum >= steamRiseInterval;
    if (doRise) {
        steamRiseAccum -= steamRiseInterval;
        
        // Increment generation for rise tracking (prevents cascading through z-levels)
        steamRiseGeneration++;
        if (steamRiseGeneration == 0) {
            // Handle overflow by clearing the tracking array
            memset(steamHasRisen, 0, sizeof(steamHasRisen));
            steamRiseGeneration = 1;
        }
    }
    
    // Alternate scan direction each tick to avoid directional bias
    bool reverseX = (steamTick & 1);
    bool reverseY = (steamTick & 2);
    
    // Process from bottom to top (simple iteration)
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
                
                ProcessSteamCell(x, y, z, doRise);
                steamUpdateCount++;
                
                // Cap updates per tick
                if (steamUpdateCount >= STEAM_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
}
