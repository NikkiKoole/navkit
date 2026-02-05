#include "sim_presence.h"
#include "water.h"
#include "fire.h"
#include "steam.h"
#include "smoke.h"
#include "temperature.h"
#include "../world/grid.h"

// Active cell counts for early exit
int waterActiveCells = 0;
int steamActiveCells = 0;
int fireActiveCells = 0;
int smokeActiveCells = 0;
int tempSourceCount = 0;
int tempUnstableCells = 0;

void InitSimPresence(void) {
    waterActiveCells = 0;
    steamActiveCells = 0;
    fireActiveCells = 0;
    smokeActiveCells = 0;
    tempSourceCount = 0;
    tempUnstableCells = 0;
}

// Rebuild counters from simulation grids (call after loading a save)
void RebuildSimPresenceCounts(void) {
    waterActiveCells = 0;
    steamActiveCells = 0;
    fireActiveCells = 0;
    smokeActiveCells = 0;
    tempSourceCount = 0;
    tempUnstableCells = 0;
    
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
            }
        }
    }
}
