#include "sim_manager.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/steam.h"
#include "../simulation/smoke.h"
#include "../simulation/temperature.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/floordirt.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "vendor/raylib.h"

// Active cell counts for early exit
int waterActiveCells = 0;
int steamActiveCells = 0;
int fireActiveCells = 0;
int smokeActiveCells = 0;
int tempSourceCount = 0;
int tempUnstableCells = 0;
int treeActiveCells = 0;
int wearActiveCells = 0;
int dirtActiveCells = 0;

void InitSimActivity(void) {
    waterActiveCells = 0;
    steamActiveCells = 0;
    fireActiveCells = 0;
    smokeActiveCells = 0;
    tempSourceCount = 0;
    tempUnstableCells = 0;
    treeActiveCells = 0;
    wearActiveCells = 0;
    dirtActiveCells = 0;
}

// Rebuild counters from simulation grids (call after loading a save)
void RebuildSimActivityCounts(void) {
    waterActiveCells = 0;
    steamActiveCells = 0;
    fireActiveCells = 0;
    smokeActiveCells = 0;
    tempSourceCount = 0;
    tempUnstableCells = 0;
    treeActiveCells = 0;
    wearActiveCells = 0;
    dirtActiveCells = 0;
    
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Water: level > 0 or has source/drain
                WaterCell *wc = &waterGrid[z][y][x];
                if (wc->level > 0 || wc->isSource || wc->isDrain) {
                    waterActiveCells++;
                }
                // Steam
                if (GetSteamLevel(x, y, z) > 0) {
                    steamActiveCells++;
                }
                // Fire: level > 0 or is source
                FireCell *fc = &fireGrid[z][y][x];
                if (fc->level > 0 || fc->isSource) {
                    fireActiveCells++;
                }
                // Smoke
                if (GetSmokeLevel(x, y, z) > 0) {
                    smokeActiveCells++;
                }
                // Temperature sources
                if (IsHeatSource(x, y, z) || IsColdSource(x, y, z)) {
                    tempSourceCount++;
                }
                // Temperature: unstable or differs from ambient
                TempCell *tc = &temperatureGrid[z][y][x];
                if (!tc->stable || tc->current != ambient) {
                    tempUnstableCells++;
                }
                // Trees: saplings are always active
                CellType cell = grid[z][y][x];
                if (cell == CELL_SAPLING) {
                    treeActiveCells++;
                }
                // Groundwear: worn dirt terrain tiles
                if (cell == CELL_WALL && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT && GetGroundWear(x, y, z) > 0) {
                    wearActiveCells++;
                }
                // Floor dirt: constructed floor tiles with tracked-in dirt
                if (floorDirtGrid[z][y][x] > 0) {
                    dirtActiveCells++;
                }
            }
        }
    }
}

// Validate activity counters against actual grid state, auto-correct if drift detected
// Returns true if counters are valid, false if drift was detected and corrected
bool ValidateSimActivityCounts(void) {
    int actualWater = 0, actualSteam = 0, actualFire = 0, actualSmoke = 0;
    int actualTempSource = 0, actualTempUnstable = 0, actualTree = 0, actualWear = 0, actualDirt = 0;
    
    // Count actual active cells from grids (same logic as RebuildSimActivityCounts)
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                WaterCell *wc = &waterGrid[z][y][x];
                if (wc->level > 0 || wc->isSource || wc->isDrain) actualWater++;
                if (GetSteamLevel(x, y, z) > 0) actualSteam++;
                FireCell *fc = &fireGrid[z][y][x];
                if (fc->level > 0 || fc->isSource) actualFire++;
                if (GetSmokeLevel(x, y, z) > 0) actualSmoke++;
                if (IsHeatSource(x, y, z) || IsColdSource(x, y, z)) actualTempSource++;
                TempCell *tc = &temperatureGrid[z][y][x];
                if (!tc->stable || tc->current != ambient) actualTempUnstable++;
                CellType cell = grid[z][y][x];
                if (cell == CELL_SAPLING) actualTree++;
                if (cell == CELL_WALL && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT && GetGroundWear(x, y, z) > 0) actualWear++;
                if (floorDirtGrid[z][y][x] > 0) actualDirt++;
            }
        }
    }
    
    bool valid = true;
    
    #define CHECK_COUNTER(name, actual) \
        if (name != actual) { \
            TraceLog(LOG_WARNING, "Activity counter drift: " #name " = %d, actual = %d (correcting)", name, actual); \
            name = actual; \
            valid = false; \
        }
    
    CHECK_COUNTER(waterActiveCells, actualWater);
    CHECK_COUNTER(steamActiveCells, actualSteam);
    CHECK_COUNTER(fireActiveCells, actualFire);
    CHECK_COUNTER(smokeActiveCells, actualSmoke);
    CHECK_COUNTER(tempSourceCount, actualTempSource);
    CHECK_COUNTER(tempUnstableCells, actualTempUnstable);
    CHECK_COUNTER(treeActiveCells, actualTree);
    CHECK_COUNTER(wearActiveCells, actualWear);
    CHECK_COUNTER(dirtActiveCells, actualDirt);
    
    #undef CHECK_COUNTER
    
    return valid;
}
