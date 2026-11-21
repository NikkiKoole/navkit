#include "sim_manager.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/steam.h"
#include "../simulation/smoke.h"
#include "../simulation/temperature.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/floordirt.h"
#include "../simulation/farming.h"
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
int treeRegenCells = 0;
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
    treeRegenCells = 0;
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
    treeRegenCells = 0;
    wearActiveCells = 0;
    dirtActiveCells = 0;
    farmActiveCells = 0;

    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Water: level > 0 or has source/drain
                const WaterCell *wc = &WATER(x, y, z);
                if (wc->level > 0 || wc->isSource || wc->isDrain) {
                    waterActiveCells++;
                }
                // Steam
                if (GetSteamLevel(x, y, z) > 0) {
                    steamActiveCells++;
                }
                // Fire: level > 0 or is source
                const FireCell *fc = &FIRE(x, y, z);
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
                TempCell *tc = &TEMPERATURE(x, y, z);
                if (!tc->stable || tc->current != ambient) {
                    tempUnstableCells++;
                }
                // Trees: saplings, young trees and still-growing trunks
                CellType cell = GRID(x, y, z);
                if (IsGrowingTreeCell(x, y, z)) {
                    treeActiveCells++;
                }
                // Tree regen: trunk bases with depleted harvest
                if (IsRegrowingTreeCell(x, y, z)) {
                    treeRegenCells++;
                }
                // Groundwear: worn dirt terrain tiles
                if (cell == CELL_WALL && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT && GetGroundWear(x, y, z) > 0) {
                    wearActiveCells++;
                }
                // Floor dirt: constructed floor tiles with tracked-in dirt
                if (FLOOR_DIRT(x, y, z) > 0) {
                    dirtActiveCells++;
                }
                // Farm: tilled cells
                if (FARM(x, y, z).tilled) {
                    farmActiveCells++;
                }
            }
        }
    }
}

// Validate activity counters against actual grid state, auto-correct if drift detected
// Returns true if counters are valid, false if drift was detected and corrected
bool ValidateSimActivityCounts(void) {
    int actualWater = 0, actualSteam = 0, actualFire = 0, actualSmoke = 0;
    int actualTempSource = 0, actualTempUnstable = 0, actualTree = 0, actualTreeRegen = 0, actualWear = 0, actualDirt = 0, actualFarm = 0;
    
    // Count actual active cells from grids (same logic as RebuildSimActivityCounts)
    for (int z = 0; z < gridDepth; z++) {
        int ambient = GetAmbientTemperature(z);
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                const WaterCell *wc = &WATER(x, y, z);
                if (wc->level > 0 || wc->isSource || wc->isDrain) actualWater++;
                if (GetSteamLevel(x, y, z) > 0) actualSteam++;
                const FireCell *fc = &FIRE(x, y, z);
                if (fc->level > 0 || fc->isSource) actualFire++;
                if (GetSmokeLevel(x, y, z) > 0) actualSmoke++;
                if (IsHeatSource(x, y, z) || IsColdSource(x, y, z)) actualTempSource++;
                TempCell *tc = &TEMPERATURE(x, y, z);
                if (!tc->stable || tc->current != ambient) actualTempUnstable++;
                CellType cell = GRID(x, y, z);
                if (IsGrowingTreeCell(x, y, z)) actualTree++;
                if (IsRegrowingTreeCell(x, y, z)) actualTreeRegen++;
                if (cell == CELL_WALL && IsWallNatural(x, y, z) && GetWallMaterial(x, y, z) == MAT_DIRT && GetGroundWear(x, y, z) > 0) actualWear++;
                if (FLOOR_DIRT(x, y, z) > 0) actualDirt++;
                if (FARM(x, y, z).tilled) actualFarm++;
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
    CHECK_COUNTER(treeRegenCells, actualTreeRegen);
    CHECK_COUNTER(wearActiveCells, actualWear);
    CHECK_COUNTER(dirtActiveCells, actualDirt);
    CHECK_COUNTER(farmActiveCells, actualFarm);

    #undef CHECK_COUNTER
    
    return valid;
}
