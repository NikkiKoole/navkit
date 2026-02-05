# Simulation Presence Grid Optimization

## Problem
Water, fire, steam, smoke, and temperature simulations iterate the full 256×256×16 grid (~1M cells) every tick, even when empty. Profiler shows ~21% CPU combined (water 7.1%, steam 7.5%, fire 6.6%) with no active simulations.

Current code checks `cell->stable && cell->level == 0` for each cell - still costs ~1M comparisons per simulation per tick.

### All simulations to consider
| Simulation | File | Update function | Called from |
|------------|------|-----------------|-------------|
| Water | water.c | UpdateWater() | mover.c:1330 |
| Fire | fire.c | UpdateFire() | mover.c:1335 |
| Smoke | smoke.c | UpdateSmoke() | mover.c:1340 |
| Steam | steam.c | UpdateSteam() | mover.c:1345 |
| Temperature | temperature.c | UpdateTemperature() | mover.c:1350 |
| Water freezing | water.c | UpdateWaterFreezing() | mover.c:1351 |

**Note:** Smoke currently has a comment saying stable optimization is disabled due to z-level skipping bugs:
```c
// Don't skip any cells - the stable optimization causes z-level skipping bugs
// TODO: fix stable optimization to account for smoke rising from below
```

**Note:** There is a separate smoke z-level skipping bug being investigated in `docs/doing/smoke-bug-investigation.md`. 
The presence grid will help with the stable optimization issue, but the other bug may have a different root cause.

**Temperature fix applied:** Moved `tempUpdateCount = 0` reset to after the early-exit check in `UpdateTemperature()` 
so the count is preserved when the interval doesn't elapse (temperature runs every 0.1s, not every tick).

## Solution
Add a compact presence grid that tracks which cells have active simulation elements:

```c
// Two bytes per cell, each bit = one simulation type (16 types max)
uint16_t simPresenceGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

#define SIM_HAS_WATER       (1 << 0)
#define SIM_HAS_STEAM       (1 << 1)
#define SIM_HAS_FIRE        (1 << 2)
#define SIM_HAS_SMOKE       (1 << 3)
#define SIM_HAS_TEMP_SOURCE (1 << 4)   // heat or cold source
// Future:
// #define SIM_HAS_LAVA      (1 << 5)
// #define SIM_HAS_LIGHT     (1 << 6)
// #define SIM_HAS_SOUND     (1 << 7)
// #define SIM_HAS_DIRT      (1 << 8)   // falling dirt/sand
// #define SIM_HAS_GAS       (1 << 9)   // poison, miasma
// #define SIM_HAS_ELECTRIC  (1 << 10)
// bits 11-15 spare
```

Memory: 2MB for 256×256×16 grid (still fits in L3 cache on most CPUs).

### Why this approach
- 2 bytes per cell vs checking multiple 2-byte structs per simulation
- Better cache locality - presence grid is compact (2MB for 256×256×16)
- Simple bookkeeping - just set/clear bits on level changes
- Can scan presence grid to build work list, then process only active cells
- Room for 16 simulation types (5 used, 11 spare)

### Alternative approaches considered
1. **Active cell list** - Track list of cells with water/fire/steam
   - Pro: Perfect efficiency, only iterate active cells
   - Con: Removal is O(n), complex bookkeeping, needs index stored in cell struct
   
2. **Dirty chunk tracking** - Track which chunks have activity
   - Pro: Low memory, simple
   - Con: Still iterates full chunk even if only 1 cell active

Presence grid is a good middle ground - simple implementation, good cache behavior, minimal overhead.

## Implementation

### Files to modify

**src/simulation/sim_presence.h** (new file)
```c
#ifndef SIM_PRESENCE_H
#define SIM_PRESENCE_H

#include "../world/grid.h"
#include <stdint.h>
#include <stdbool.h>

// Presence flags - 16 bits available
#define SIM_HAS_WATER       (1 << 0)
#define SIM_HAS_STEAM       (1 << 1)
#define SIM_HAS_FIRE        (1 << 2)
#define SIM_HAS_SMOKE       (1 << 3)
#define SIM_HAS_TEMP_SOURCE (1 << 4)
// Future: LAVA (5), LIGHT (6), SOUND (7), DIRT (8), GAS (9), ELECTRIC (10)
// bits 11-15 spare

extern uint16_t simPresenceGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Active cell counts for early exit optimization
extern int waterActiveCells;
extern int steamActiveCells;
extern int fireActiveCells;
extern int smokeActiveCells;
extern int tempSourceCount;

void InitSimPresence(void);
void ClearSimPresenceGrid(void);
void RebuildSimPresence(void);  // Rebuild from grids (call after load)

static inline void SetSimPresenceFlag(int x, int y, int z, uint16_t flag) {
    simPresenceGrid[z][y][x] |= flag;
}

static inline void ClearSimPresenceFlag(int x, int y, int z, uint16_t flag) {
    simPresenceGrid[z][y][x] &= ~flag;
}

static inline bool HasSimPresenceFlag(int x, int y, int z, uint16_t flag) {
    return simPresenceGrid[z][y][x] & flag;
}

#endif
```

**src/simulation/sim_presence.c** (new file)
```c
#include "sim_presence.h"
#include "water.h"
#include "fire.h"
#include "steam.h"
#include "smoke.h"
#include "temperature.h"
#include <string.h>

uint16_t simPresenceGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

int waterActiveCells = 0;
int steamActiveCells = 0;
int fireActiveCells = 0;
int smokeActiveCells = 0;
int tempSourceCount = 0;

void InitSimPresence(void) {
    ClearSimPresenceGrid();
}

void ClearSimPresenceGrid(void) {
    memset(simPresenceGrid, 0, sizeof(simPresenceGrid));
    waterActiveCells = 0;
    steamActiveCells = 0;
    fireActiveCells = 0;
    smokeActiveCells = 0;
    tempSourceCount = 0;
}

// Rebuild presence grid from simulation grids (call after loading a save)
void RebuildSimPresence(void) {
    ClearSimPresenceGrid();
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Water: level > 0 or has source/drain
                WaterCell *wc = &waterGrid[z][y][x];
                if (wc->level > 0 || wc->isSource || wc->isDrain) {
                    SetSimPresenceFlag(x, y, z, SIM_HAS_WATER);
                    waterActiveCells++;
                }
                // Steam
                if (GetSteamLevel(x, y, z) > 0) {
                    SetSimPresenceFlag(x, y, z, SIM_HAS_STEAM);
                    steamActiveCells++;
                }
                // Fire: level > 0 or is source
                FireCell *fc = &fireGrid[z][y][x];
                if (fc->level > 0 || fc->isSource) {
                    SetSimPresenceFlag(x, y, z, SIM_HAS_FIRE);
                    fireActiveCells++;
                }
                // Smoke
                if (GetSmokeLevel(x, y, z) > 0) {
                    SetSimPresenceFlag(x, y, z, SIM_HAS_SMOKE);
                    smokeActiveCells++;
                }
                // Temperature sources
                if (IsHeatSource(x, y, z) || IsColdSource(x, y, z)) {
                    SetSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
                    tempSourceCount++;
                }
            }
        }
    }
}
```

**src/simulation/water.c** - Modify SetWaterLevel:
```c
void SetWaterLevel(int x, int y, int z, int level) {
    if (!WaterInBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > WATER_MAX_LEVEL) level = WATER_MAX_LEVEL;
    
    int oldLevel = waterGrid[z][y][x].level;
    waterGrid[z][y][x].level = (uint8_t)level;
    
    // Update presence grid and count
    if (oldLevel == 0 && level > 0) {
        SetSimPresenceFlag(x, y, z, SIM_HAS_WATER);
        waterActiveCells++;
    } else if (oldLevel > 0 && level == 0) {
        ClearSimPresenceFlag(x, y, z, SIM_HAS_WATER);
        waterActiveCells--;
    }
    
    if (oldLevel != level) {
        DestabilizeWater(x, y, z);
    }
}
```

**src/simulation/water.c** - Modify UpdateWater:
```c
void UpdateWater(void) {
    if (!waterEnabled) return;
    
    // Early exit: no water activity at all
    // Note: still need to check sources/drains even if waterActiveCells == 0
    // Consider tracking sourceCount/drainCount separately for true early exit
    
    waterUpdateCount = 0;
    
    // ... evaporation accumulator logic ...
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Fast skip via presence grid
                if (!(simPresenceGrid[z][y][x] & SIM_HAS_WATER)) continue;
                
                WaterCell* cell = &waterGrid[z][y][x];
                
                // Skip frozen water
                if (cell->isFrozen) continue;
                
                // Skip stable cells (unless source/drain)
                if (cell->stable && !cell->isSource && !cell->isDrain) continue;
                
                ProcessWaterCell(x, y, z, doEvap);
                waterUpdateCount++;
                
                if (waterUpdateCount >= WATER_MAX_UPDATES_PER_TICK) return;
            }
        }
    }
}
```

**Note:** Sources/drains need to be marked in presence grid too via SetWaterSource/SetWaterDrain:
```c
void SetWaterSource(int x, int y, int z, bool isSource) {
    if (!WaterInBounds(x, y, z)) return;
    bool wasActive = waterGrid[z][y][x].level > 0 || waterGrid[z][y][x].isSource || waterGrid[z][y][x].isDrain;
    waterGrid[z][y][x].isSource = isSource;
    bool isActive = waterGrid[z][y][x].level > 0 || waterGrid[z][y][x].isSource || waterGrid[z][y][x].isDrain;
    
    // Update presence
    if (!wasActive && isActive) {
        SetSimPresenceFlag(x, y, z, SIM_HAS_WATER);
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        ClearSimPresenceFlag(x, y, z, SIM_HAS_WATER);
        waterActiveCells--;
    }
    
    if (isSource) DestabilizeWater(x, y, z);
}
```

**src/simulation/fire.c** - Modify SetFireLevel (similar pattern):
```c
void SetFireLevel(int x, int y, int z, int level) {
    // ... existing bounds/clamp logic ...
    
    int oldLevel = cell->level;
    cell->level = (uint16_t)level;
    
    // Update presence grid and count
    if (oldLevel == 0 && level > 0) {
        SetSimPresenceFlag(x, y, z, SIM_HAS_FIRE);
        fireActiveCells++;
    } else if (oldLevel > 0 && level == 0) {
        ClearSimPresenceFlag(x, y, z, SIM_HAS_FIRE);
        fireActiveCells--;
    }
    
    // ... rest of function ...
}
```

**src/simulation/steam.c** - Modify SetSteamLevel (similar pattern)

**src/simulation/smoke.c** - Modify SetSmokeLevel (similar pattern)

**src/simulation/temperature.c** - Modify SetHeatSource/SetColdSource:
```c
void SetHeatSource(int x, int y, int z, bool isSource) {
    // ... existing logic ...
    
    bool wasSource = cell->isHeatSource || cell->isColdSource;
    cell->isHeatSource = isSource;
    cell->isColdSource = false;
    bool isNowSource = cell->isHeatSource || cell->isColdSource;
    
    // Update presence grid
    if (!wasSource && isNowSource) {
        SetSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
        tempSourceCount++;
    } else if (wasSource && !isNowSource) {
        ClearSimPresenceFlag(x, y, z, SIM_HAS_TEMP_SOURCE);
        tempSourceCount--;
    }
    
    // ... rest of function ...
}
```

**src/core/saveload.c** - Call RebuildSimPresence after loading:
```c
// At the end of LoadGame(), after all grids are loaded:
RebuildSimPresence();
```

**Clear functions** - Each ClearX() must also clear presence:
```c
void ClearWater(void) {
    memset(waterGrid, 0, sizeof(waterGrid));
    waterUpdateCount = 0;
    waterEvapAccum = 0.0f;
    
    // Clear presence bits for water
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                ClearSimPresenceFlag(x, y, z, SIM_HAS_WATER);
            }
        }
    }
    waterActiveCells = 0;
}
// Similar for ClearFire(), ClearSteam(), ClearSmoke()
```

**src/main.c** or init code:
```c
#include "simulation/sim_presence.h"

// In initialization:
InitSimPresence();
```

### Edge cases to handle
- Sources/drains with level 0 still need processing
- ClearWater/ClearFire/ClearSteam/ClearSmoke must also clear presence bits
- Loading saves must rebuild presence grid (or save/load it)
  - Save/load is in `src/core/saveload.c` - saves waterGrid, fireGrid, steamGrid, smokeGrid, temperatureGrid
  - Option A: Save simPresenceGrid too
  - Option B: Rebuild from grids after load (scan once, set bits)
- Smoke has stable optimization disabled - presence grid may help fix that
- Temperature is different: it doesn't have "level", it has sources that spread heat
  - Only track `SIM_HAS_TEMP_SOURCE` for heat/cold sources
  - Temperature itself spreads to all cells, so presence grid doesn't help much there
  - Could still early-exit if no sources exist

## Test Plan

### 0. New tests for presence grid itself

**tests/test_sim_presence.c** (new file)
```c
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/simulation/sim_presence.h"
#include "../src/simulation/water.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/steam.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/temperature.h"

// =============================================================================
// Basic Presence Grid Operations
// =============================================================================

describe(presence_grid_init) {
    it("should initialize with all zeros") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                expect(simPresenceGrid[0][y][x] == 0);
            }
        }
        expect(waterActiveCells == 0);
        expect(fireActiveCells == 0);
        expect(steamActiveCells == 0);
        expect(smokeActiveCells == 0);
        expect(tempSourceCount == 0);
    }
}

// =============================================================================
// Water Presence Tracking
// =============================================================================

describe(water_presence_tracking) {
    it("should set presence when water added") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_WATER) == false);
        expect(waterActiveCells == 0);
        
        SetWaterLevel(2, 1, 0, 5);
        
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_WATER) == true);
        expect(waterActiveCells == 1);
    }
    
    it("should clear presence when water removed") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        SetWaterLevel(2, 1, 0, 5);
        expect(waterActiveCells == 1);
        
        SetWaterLevel(2, 1, 0, 0);
        
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_WATER) == false);
        expect(waterActiveCells == 0);
    }
    
    it("should not double-count when changing non-zero levels") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        SetWaterLevel(2, 1, 0, 3);
        expect(waterActiveCells == 1);
        
        SetWaterLevel(2, 1, 0, 7);  // Change level but still non-zero
        expect(waterActiveCells == 1);  // Should still be 1, not 2
        
        SetWaterLevel(2, 1, 0, 1);
        expect(waterActiveCells == 1);
    }
    
    it("should track multiple water cells independently") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        SetWaterLevel(0, 0, 0, 5);
        SetWaterLevel(1, 0, 0, 5);
        SetWaterLevel(2, 0, 0, 5);
        expect(waterActiveCells == 3);
        
        SetWaterLevel(1, 0, 0, 0);  // Remove middle one
        expect(waterActiveCells == 2);
        expect(HasSimPresenceFlag(0, 0, 0, SIM_HAS_WATER) == true);
        expect(HasSimPresenceFlag(1, 0, 0, SIM_HAS_WATER) == false);
        expect(HasSimPresenceFlag(2, 0, 0, SIM_HAS_WATER) == true);
    }
    
    it("should track water sources in presence") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        SetWaterSource(2, 1, 0, true);
        
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_WATER) == true);
        expect(waterActiveCells == 1);
        
        SetWaterSource(2, 1, 0, false);
        
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_WATER) == false);
        expect(waterActiveCells == 0);
    }
    
    it("should track water drains in presence") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        SetWaterDrain(2, 1, 0, true);
        
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_WATER) == true);
        expect(waterActiveCells == 1);
    }
}

// =============================================================================
// Fire Presence Tracking
// =============================================================================

describe(fire_presence_tracking) {
    it("should set presence when fire added") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitFire();
        
        SetFireLevel(1, 1, 0, 5);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_FIRE) == true);
        expect(fireActiveCells == 1);
    }
    
    it("should clear presence when fire extinguished") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitFire();
        
        SetFireLevel(1, 1, 0, 5);
        SetFireLevel(1, 1, 0, 0);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_FIRE) == false);
        expect(fireActiveCells == 0);
    }
    
    it("should track fire sources in presence") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitFire();
        
        SetFireSource(1, 1, 0, true);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_FIRE) == true);
        expect(fireActiveCells == 1);
    }
}

// =============================================================================
// Steam Presence Tracking
// =============================================================================

describe(steam_presence_tracking) {
    it("should set presence when steam added") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitSteam();
        
        SetSteamLevel(1, 1, 0, 5);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_STEAM) == true);
        expect(steamActiveCells == 1);
    }
    
    it("should clear presence when steam dissipates") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitSteam();
        
        SetSteamLevel(1, 1, 0, 5);
        SetSteamLevel(1, 1, 0, 0);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_STEAM) == false);
        expect(steamActiveCells == 0);
    }
}

// =============================================================================
// Smoke Presence Tracking
// =============================================================================

describe(smoke_presence_tracking) {
    it("should set presence when smoke added") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitSmoke();
        
        SetSmokeLevel(1, 1, 0, 5);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_SMOKE) == true);
        expect(smokeActiveCells == 1);
    }
    
    it("should clear presence when smoke clears") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitSmoke();
        
        SetSmokeLevel(1, 1, 0, 5);
        SetSmokeLevel(1, 1, 0, 0);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_SMOKE) == false);
        expect(smokeActiveCells == 0);
    }
}

// =============================================================================
// Temperature Source Tracking
// =============================================================================

describe(temp_source_tracking) {
    it("should set presence for heat source") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitTemperature();
        
        SetHeatSource(1, 1, 0, true);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_TEMP_SOURCE) == true);
        expect(tempSourceCount == 1);
    }
    
    it("should set presence for cold source") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitTemperature();
        
        SetColdSource(1, 1, 0, true);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_TEMP_SOURCE) == true);
        expect(tempSourceCount == 1);
    }
    
    it("should clear presence when source removed") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitTemperature();
        
        SetHeatSource(1, 1, 0, true);
        SetHeatSource(1, 1, 0, false);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_TEMP_SOURCE) == false);
        expect(tempSourceCount == 0);
    }
    
    it("should not double-count when switching heat to cold") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitTemperature();
        
        SetHeatSource(1, 1, 0, true);
        expect(tempSourceCount == 1);
        
        SetColdSource(1, 1, 0, true);  // Switches from heat to cold
        expect(tempSourceCount == 1);  // Still just 1
    }
}

// =============================================================================
// Multiple Simulation Types Per Cell
// =============================================================================

describe(multiple_sim_types) {
    it("should track multiple simulation types in same cell") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        InitSteam();
        InitFire();
        
        SetWaterLevel(1, 1, 0, 3);
        SetSteamLevel(1, 1, 0, 2);
        SetFireLevel(1, 1, 0, 1);
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_WATER) == true);
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_STEAM) == true);
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_FIRE) == true);
        expect(waterActiveCells == 1);
        expect(steamActiveCells == 1);
        expect(fireActiveCells == 1);
    }
    
    it("should clear only the relevant flag") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        InitSteam();
        
        SetWaterLevel(1, 1, 0, 3);
        SetSteamLevel(1, 1, 0, 2);
        
        SetWaterLevel(1, 1, 0, 0);  // Remove water only
        
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_WATER) == false);
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_STEAM) == true);
        expect(waterActiveCells == 0);
        expect(steamActiveCells == 1);
    }
}

// =============================================================================
// Rebuild From Grids (Save/Load)
// =============================================================================

describe(rebuild_presence) {
    it("should rebuild presence from existing grids") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitWater();
        InitFire();
        InitSteam();
        InitSmoke();
        InitTemperature();
        
        // Set up grids directly (simulating a load)
        waterGrid[0][0][0].level = 5;
        waterGrid[0][1][1].level = 3;
        fireGrid[0][0][1].level = 2;
        steamGrid[0][1][0].level = 4;
        smokeGrid[0][0][2].level = 1;
        temperatureGrid[0][1][2].isHeatSource = true;
        
        // Clear and rebuild
        InitSimPresence();
        RebuildSimPresence();
        
        expect(waterActiveCells == 2);
        expect(fireActiveCells == 1);
        expect(steamActiveCells == 1);
        expect(smokeActiveCells == 1);
        expect(tempSourceCount == 1);
        
        expect(HasSimPresenceFlag(0, 0, 0, SIM_HAS_WATER) == true);
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_WATER) == true);
        expect(HasSimPresenceFlag(1, 0, 0, SIM_HAS_FIRE) == true);
        expect(HasSimPresenceFlag(0, 1, 0, SIM_HAS_STEAM) == true);
        expect(HasSimPresenceFlag(2, 0, 0, SIM_HAS_SMOKE) == true);
        expect(HasSimPresenceFlag(2, 1, 0, SIM_HAS_TEMP_SOURCE) == true);
    }
    
    it("should include water sources and drains in rebuild") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitWater();
        
        // Source with no water level
        waterGrid[0][0][0].isSource = true;
        waterGrid[0][0][0].level = 0;
        
        // Drain with no water level
        waterGrid[0][1][1].isDrain = true;
        waterGrid[0][1][1].level = 0;
        
        InitSimPresence();
        RebuildSimPresence();
        
        expect(waterActiveCells == 2);
        expect(HasSimPresenceFlag(0, 0, 0, SIM_HAS_WATER) == true);
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_WATER) == true);
    }
    
    it("should include fire sources in rebuild") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitFire();
        
        fireGrid[0][0][0].isSource = true;
        fireGrid[0][0][0].level = 0;
        
        InitSimPresence();
        RebuildSimPresence();
        
        expect(fireActiveCells == 1);
        expect(HasSimPresenceFlag(0, 0, 0, SIM_HAS_FIRE) == true);
    }
}

// =============================================================================
// Clear Functions
// =============================================================================

describe(clear_functions) {
    it("should clear water presence when ClearWater called") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        SetWaterLevel(0, 0, 0, 5);
        SetWaterLevel(1, 1, 0, 5);
        expect(waterActiveCells == 2);
        
        ClearWater();
        
        expect(waterActiveCells == 0);
        expect(HasSimPresenceFlag(0, 0, 0, SIM_HAS_WATER) == false);
        expect(HasSimPresenceFlag(1, 1, 0, SIM_HAS_WATER) == false);
    }
    
    // Similar tests for ClearFire, ClearSteam, ClearSmoke...
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(presence_edge_cases) {
    it("should handle out of bounds gracefully") {
        InitGridFromAsciiWithChunkSize("....\n....\n", 4, 2);
        InitSimPresence();
        InitWater();
        
        // These should not crash
        SetWaterLevel(-1, 0, 0, 5);
        SetWaterLevel(100, 0, 0, 5);
        SetWaterLevel(0, -1, 0, 5);
        SetWaterLevel(0, 100, 0, 5);
        
        // Count should still be 0
        expect(waterActiveCells == 0);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }
    
    test(presence_grid_init);
    test(water_presence_tracking);
    test(fire_presence_tracking);
    test(steam_presence_tracking);
    test(smoke_presence_tracking);
    test(temp_source_tracking);
    test(multiple_sim_types);
    test(rebuild_presence);
    test(clear_functions);
    test(presence_edge_cases);
    
    return summary();
}
```

Add to test_unity.c:
```c
#include "test_sim_presence.c"
```

### 1. All existing tests must pass
Run `make test` after implementation. Existing tests cover:
- Basic operations (init, clear, set/get levels)
- Flow, falling, pressure, spreading  
- Source/drain behavior
- Freezing, evaporation, condensation
- Wall blocking
- Multi-z behavior
- Edge cases

Key test files:
- `tests/test_water.c` - 20+ test scenarios
- `tests/test_fire.c` - 14+ test scenarios  
- `tests/test_steam.c` - 8+ test scenarios
- `tests/test_temperature.c` - temperature tests

Run with: `make test`

### 2. Performance verification

#### Headless benchmark test

Use the existing headless mode with a save file to measure before/after:

```bash
# Before implementation - baseline
bin/path --headless --load saves/2026-02-04_16-51-54.bin.gz --ticks 1000

# After implementation - compare
bin/path --headless --load saves/2026-02-04_16-51-54.bin.gz --ticks 1000
```

**Add simulation stats to headless output** (`src/main.c` RunHeadless function):
```c
// After the tick loop, add:
printf("\n=== SIMULATION STATS ===\n");
printf("Water: updateCount=%d, activeCells=%d\n", waterUpdateCount, waterActiveCells);
printf("Fire:  updateCount=%d, activeCells=%d\n", fireUpdateCount, fireActiveCells);
printf("Steam: updateCount=%d, activeCells=%d\n", steamUpdateCount, steamActiveCells);
printf("Smoke: updateCount=%d, activeCells=%d\n", smokeUpdateCount, smokeActiveCells);
printf("Temp:  updateCount=%d, sourceCount=%d\n", tempUpdateCount, tempSourceCount);
```

Also add timing per-simulation (optional, more detailed):
```c
// In the tick loop or after:
extern double waterUpdateTime, fireUpdateTime, steamUpdateTime, smokeUpdateTime, tempUpdateTime;
printf("Water: %.2fms, Fire: %.2fms, Steam: %.2fms, Smoke: %.2fms, Temp: %.2fms\n",
       waterUpdateTime, fireUpdateTime, steamUpdateTime, smokeUpdateTime, tempUpdateTime);
```

#### Headless instrumentation available

Run: `bin/path --headless --load <save> --ticks N`

Output includes:
- **Update counts**: How many cells were processed (may hit caps)
- **Timing (avg ms)**: Per-simulation timing from profiler (Water, Fire, Smoke, Steam, Temperature)
- **Active cells**: Actual count of cells with level > 0
- **Smoke per z-level**: Distribution of smoke through z-levels

Example:
```
=== SIMULATION STATS (last tick) ===
Update counts: Water=0, Fire=383, Steam=8192, Smoke=65536, Temp=4096
Timing (avg ms): Water=1.04 Fire=1.01 Smoke=0.07 Steam=0.41 Temperature=0.42
Active cells: Water=16, Fire=383, Steam=1, Smoke=769
Smoke per z-level: z7=2465(384c) z8=2472(385c) (total=4937, maxZ=8 at 108,135)
```

#### Expected results
Compare before/after:
- **Empty grid**: Simulation timing should drop from ~3ms to ~0ms
- **Active simulation**: Timing proportional to active cells, not grid size
- **Sparse activity** (small water pool): should only process those cells

#### Test saves needed

**1. Empty simulations save** (current: `saves/2026-02-04_16-51-54.bin.gz`)
- Tests early-exit optimization
- Currently shows smoke iterating full grid due to disabled stable optimization

**2. Mixed simulations save** (`saves/2026-02-05_08-39-58.bin.gz`)
- Water source, fire source, heat/cold sources
- All simulations have activity (some stabilize over time)

#### Baseline (before implementation)

**Empty save** - `saves/2026-02-04_16-51-54.bin.gz` (100 ticks)

```
=== SIMULATION STATS (last tick) ===
Update counts: Water=0, Fire=0, Steam=8192, Smoke=65536, Temp=4096
Timing (avg ms): Water=1.04 Fire=1.01 Smoke=0.07 Steam=0.41 Temperature=0.42
Active cells: Water=0, Fire=0, Steam=0, Smoke=0
```

**Key insight**: With ZERO active cells, we still spend ~3ms total on simulations:
- Water: 1.04ms (full grid iteration)
- Fire: 1.01ms (full grid iteration)
- Steam: 0.41ms (full grid iteration)
- Temperature: 0.42ms (full grid iteration)
- Smoke: 0.07ms (already fast despite disabled stable opt)

This is pure overhead that presence grid should eliminate.

**Mixed save** - `saves/2026-02-05_08-39-58.bin.gz` (100 ticks)

```
=== SIMULATION STATS (last tick) ===
Update counts: Water=0, Fire=383, Steam=8192, Smoke=65536, Temp=4096
Timing (avg ms): Water=1.04 Fire=1.01 Smoke=0.07 Steam=0.41 Temperature=0.42
Active cells: Water=16, Fire=383, Steam=1, Smoke=769
```

**Key insights**:
- Steam takes 0.41ms for 1 active cell (should be ~0ms)
- Water takes 1.04ms for 16 active cells (should be ~0ms)
- Fire timing stays same regardless of activity (full iteration)

**Smoke bug fixed**: The smoke z-level skipping bug was caused by dissipation removing 
smoke that had just risen. Fixed by checking `smokeHasRisen` before dissipation.
See `docs/done/smoke-bug-investigation.md`.

#### Target metrics after implementation

| Metric | Empty Save Before | Empty Save After | Notes |
|--------|------------------|------------------|-------|
| Water timing | 1.04ms | ~0ms | No active cells = no work |
| Fire timing | 1.01ms | ~0ms | No active cells = no work |
| Steam timing | 0.41ms | ~0ms | No active cells = no work |
| Smoke timing | 0.07ms | ~0ms | No active cells = no work |
| Temp timing | 0.42ms | ~0ms | No sources = no work |
| **Total sim** | **~3ms** | **~0ms** | Major win for empty worlds |

| Metric | Mixed Save Before | Mixed Save After | Notes |
|--------|------------------|------------------|-------|
| Water timing | 1.04ms | <0.1ms | 16 cells, not 1M |
| Fire timing | 1.01ms | <0.5ms | 383 cells, not 1M |
| Steam timing | 0.41ms | ~0ms | 1 cell, not 1M |
| Smoke timing | 0.07ms | <0.1ms | 769 cells |

### 3. Correctness spot checks
After tests pass, manually verify in game:
- Place water source → water spreads normally
- Ignite grass → fire spreads, smoke rises
- Boil water → steam rises, condenses

### 4. Save/load verification
- Save game with active water/fire/steam
- Load game
- Verify simulations continue correctly

## Rollback plan
If issues arise, the change is isolated to:
- New files: sim_presence.h, sim_presence.c
- Modified: water.c, fire.c, steam.c (SetLevel and Update functions)
- Modified: main.c or init (InitSimPresence call)

Can revert by removing presence grid checks and keeping old iteration logic.

## Future improvements
- Could track source/drain presence separately for even faster empty-grid skip
- Water sources/drains could have their own list for O(1) iteration

## Open questions to address

### Neighbor iteration
When processing a cell with presence, simulations often need to check/modify neighbors (water flows to adjacent cells, heat transfers to neighbors, etc.). 

Questions:
- Should we iterate only cells with presence bits, then check their neighbors on-demand?
- Or should we also set presence bits on neighbors that *might* receive flow?
- What about "frontier" cells - empty cells adjacent to active cells that could receive content?

Options:
1. **Presence = has content**: Only cells with water/smoke/etc have the bit set. When processing, check neighbors directly. Simple, but neighbors may not be in cache.
2. **Presence = has content OR adjacent to content**: More bits set, but guarantees we process cells that could receive flow. More iterations but better correctness.
3. **Two-pass**: First pass builds neighbor list, second pass processes. More complex.

Recommendation: Start with option 1 (simplest). If we see issues with content not flowing properly, expand to option 2.

### Chunk-level early exit
Could add a coarse-grained check before iterating individual cells:

```c
// One bit per chunk (e.g., 16x16x1 = 256 cells per chunk)
// 256/16 = 16 chunks in x, 16 in y, 16 in z = 4096 chunks total
uint64_t chunkHasPresence[64];  // 4096 bits = 64 uint64_t

// Before iterating a chunk:
if (!(chunkHasPresence[chunkIndex / 64] & (1ULL << (chunkIndex % 64)))) {
    continue;  // Skip entire chunk
}
```

This adds:
- Very fast skip for large empty regions
- Extra bookkeeping when presence changes (update chunk bit too)
- ~512 bytes memory

Could be worth it for mostly-empty worlds, but adds complexity. Consider as Phase 2 optimization after basic presence grid is working.

## Other optimization strategies (can combine)

These approaches can be layered on top of each other for maximum performance:

### 1. Active cell list
Track an explicit list of cells that have water/fire/steam, iterate only those.

```c
typedef struct { int16_t x, y, z; } SimPos;
SimPos activeWaterCells[MAX_ACTIVE_WATER];
int activeWaterCount = 0;

void UpdateWater(void) {
    for (int i = 0; i < activeWaterCount; i++) {
        ProcessWaterCell(activeWaterCells[i].x, ...);
    }
}
```

**Pros:**
- Perfect O(n) where n = active cells
- No grid iteration at all

**Cons:**
- Removal is O(n) unless you store index in cell struct (+2 bytes per cell × 1M = 2MB)
- Complex bookkeeping for add/remove
- Need to handle neighbors being added during processing

**Best for:** Very sparse simulations where active cells << grid size

### 2. Dirty region/chunk tracking
Track which chunks have active elements, only scan those chunks.

```c
// 16×16 chunks on 256×256 = 16×16 = 256 chunks per z-level
bool waterDirtyChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X];

void UpdateWater(void) {
    for each chunk where waterDirtyChunks[z][cy][cx] {
        for cells in chunk {
            // process
        }
    }
}
```

**Pros:**
- Low memory overhead (~16KB for 512×512×16)
- Simple - just mark chunk dirty when any cell changes
- Works well with spatial locality (water pools together)

**Cons:**
- Still iterates full chunk even if only 1 cell active
- Worst case (1 water cell per chunk) not much better than full scan

**Best for:** Spatially coherent simulations (pools, fires) on large grids

### 3. Throttling
Run simulations every N ticks instead of every tick.

```c
void UpdateWater(void) {
    static int tickCounter = 0;
    if (++tickCounter < WATER_THROTTLE_TICKS) return;
    tickCounter = 0;
    
    // ... actual update logic
}
```

**Pros:**
- Trivial to implement
- Directly reduces CPU by 1/N
- Can be adaptive based on load

**Cons:**
- Changes simulation behavior/speed
- Waterfalls look choppy, fire feels unresponsive
- Need to compensate with more movement per tick

**Variants:**
```c
// Adaptive throttling - slow down when busy
int throttle = (waterActiveCells > 1000) ? 2 : 1;

// Quality setting
int throttle = (graphicsQuality == LOW) ? 3 : 1;

// Per-simulation tuning (steam can be slower than water)
#define WATER_THROTTLE 1
#define STEAM_THROTTLE 2  // Steam rising doesn't need 60Hz
#define FIRE_THROTTLE  1
```

**Best for:** Background simulations, low-end hardware, simulations player doesn't watch closely

### 4. Early exit with cell counts
Track total active cells, skip entire Update function when count is 0.

```c
int waterActiveCells = 0;  // increment/decrement on level changes

void UpdateWater(void) {
    if (!waterEnabled) return;
    if (waterActiveCells == 0 && !hasWaterSources && !hasWaterDrains) return;  // early exit
    
    // ... rest of update
}
```

**Pros:**
- Simplest optimization
- Zero cost when simulation is empty
- No iteration at all in empty case

**Cons:**
- Only helps when completely empty
- Doesn't help with sparse activity

**Best for:** First optimization to implement, catches "no water in world" case

### Combining strategies

These can stack:

```c
void UpdateWater(void) {
    // Layer 1: Early exit (free when empty)
    if (waterActiveCells == 0 && !hasWaterSources) return;
    
    // Layer 2: Throttling (reduce tick frequency)
    static int tick = 0;
    if (++tick < WATER_THROTTLE) return;
    tick = 0;
    
    // Layer 3: Presence grid or dirty chunks (reduce cells checked)
    for (...) {
        if (!(simPresenceGrid[z][y][x] & SIM_HAS_WATER)) continue;
        // process
    }
}
```

**Recommended implementation order:**
1. Early exit (trivial, big win for empty case)
2. Presence grid (moderate effort, helps sparse case)
3. Throttling (trivial, but changes behavior - make it configurable)
4. Active cell list (complex, only if presence grid isn't enough)
