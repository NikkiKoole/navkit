#include "farming.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../simulation/balance.h"
#include "../simulation/weather.h"
#include <string.h>

// Grid storage
FarmCell farmGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
int farmActiveCells = 0;

// Tick accumulator
static float farmTickAccumulator = 0;

void InitFarming(void) {
    ClearFarming();
}

void ClearFarming(void) {
    memset(farmGrid, 0, sizeof(farmGrid));
    farmActiveCells = 0;
    farmTickAccumulator = 0;
}

bool IsFarmableSoil(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return false;
    if (z == 0) return false;  // can't farm on bedrock layer

    // Must be walkable surface (air cell with solid below)
    if (CellIsSolid(grid[z][y][x])) return false;
    if (!CellIsSolid(grid[z - 1][y][x])) return false;

    // Solid cell below must be natural soil material
    if (!IsWallNatural(x, y, z - 1)) return false;
    MaterialType mat = GetWallMaterial(x, y, z - 1);
    return (mat == MAT_DIRT || mat == MAT_CLAY || mat == MAT_SAND ||
            mat == MAT_PEAT || mat == MAT_GRAVEL);
}

uint8_t InitialFertilityForSoil(MaterialType mat) {
    switch (mat) {
        case MAT_DIRT:   return 128;
        case MAT_CLAY:   return 110;
        case MAT_SAND:   return 90;
        case MAT_PEAT:   return 180;
        case MAT_GRAVEL: return 64;
        default:         return 128;
    }
}

float GetSeasonalWeedRate(void) {
    Season s = GetCurrentSeason();
    switch (s) {
        case SEASON_SPRING: return 1.0f;
        case SEASON_SUMMER: return 1.0f;
        case SEASON_AUTUMN: return 0.5f;
        case SEASON_WINTER: return 0.0f;
        default: return 1.0f;
    }
}

void FarmTick(float dt) {
    if (farmActiveCells == 0) return;

    farmTickAccumulator += dt;
    float interval = GameHoursToGameSeconds(FARM_TICK_INTERVAL);
    if (farmTickAccumulator < interval) return;
    farmTickAccumulator = 0;

    float seasonWeedRate = GetSeasonalWeedRate();
    if (seasonWeedRate <= 0.0f) return;  // No weed growth in winter

    int weedGrowth = (int)(WEED_GROWTH_PER_TICK * seasonWeedRate);
    if (weedGrowth < 1) weedGrowth = 1;

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (!farmGrid[z][y][x].tilled) continue;
                int newWeed = farmGrid[z][y][x].weedLevel + weedGrowth;
                if (newWeed > 255) newWeed = 255;
                farmGrid[z][y][x].weedLevel = (uint8_t)newWeed;
            }
        }
    }
}

FarmCell* GetFarmCell(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return NULL;
    return &farmGrid[z][y][x];
}
