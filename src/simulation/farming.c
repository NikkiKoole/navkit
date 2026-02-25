#include "farming.h"
#include "../core/sim_manager.h"
#include "../core/event_log.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../simulation/balance.h"
#include "../simulation/weather.h"
#include "../entities/items.h"
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

FarmCell* GetFarmCell(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return NULL;
    return &farmGrid[z][y][x];
}

// =============================================================================
// Crop/Seed type conversion
// =============================================================================

int SeedTypeForCrop(CropType crop) {
    switch (crop) {
        case CROP_WHEAT:   return ITEM_WHEAT_SEEDS;
        case CROP_LENTILS: return ITEM_LENTIL_SEEDS;
        case CROP_FLAX:    return ITEM_FLAX_SEEDS;
        default: return ITEM_NONE;
    }
}

CropType CropTypeForSeed(int seedType) {
    switch (seedType) {
        case ITEM_WHEAT_SEEDS:  return CROP_WHEAT;
        case ITEM_LENTIL_SEEDS: return CROP_LENTILS;
        case ITEM_FLAX_SEEDS:   return CROP_FLAX;
        default: return CROP_NONE;
    }
}

// =============================================================================
// Growth modifiers (pure functions, no side effects)
// =============================================================================

float CropGrowthTimeGH(CropType crop) {
    switch (crop) {
        case CROP_WHEAT:   return WHEAT_GROWTH_GH;
        case CROP_LENTILS: return LENTIL_GROWTH_GH;
        case CROP_FLAX:    return FLAX_GROWTH_GH;
        default: return 72.0f;
    }
}

float CropSeasonModifier(CropType crop, int season) {
    // Per-crop seasonal rates:
    //              Spring  Summer  Autumn  Winter
    // Wheat:       1.0     1.5     0.8     0.0
    // Lentils:     1.2     1.0     0.0     0.0
    // Flax:        1.0     1.2     0.0     0.0
    static const float table[CROP_TYPE_COUNT][4] = {
        [CROP_NONE]    = {0.0f, 0.0f, 0.0f, 0.0f},
        [CROP_WHEAT]   = {1.0f, 1.5f, 0.8f, 0.0f},
        [CROP_LENTILS] = {1.2f, 1.0f, 0.0f, 0.0f},
        [CROP_FLAX]    = {1.0f, 1.2f, 0.0f, 0.0f},
    };
    if (crop <= CROP_NONE || crop >= CROP_TYPE_COUNT) return 0.0f;
    if (season < 0 || season > 3) return 0.0f;
    return table[crop][season];
}

float CropTemperatureModifier(float tempC) {
    if (tempC <= CROP_FREEZE_TEMP) return 0.0f;  // Frost kills
    if (tempC <= CROP_COLD_TEMP) return 0.3f;     // Cold
    if (tempC <= CROP_IDEAL_LOW) return 0.7f;     // Cool
    if (tempC <= CROP_IDEAL_HIGH) return 1.0f;    // Ideal
    if (tempC <= CROP_HOT_TEMP) return 0.7f;      // Hot
    return 0.3f;                                    // Very hot
}

float CropWetnessModifier(int wetness) {
    // Discrete 0-3 mapping (from cell wetness system)
    switch (wetness) {
        case 0: return 0.3f;  // Dry
        case 1: return 0.7f;  // Damp
        case 2: return 1.0f;  // Wet (ideal)
        case 3: return 0.5f;  // Waterlogged
        default: return 0.3f;
    }
}

float CropFertilityModifier(uint8_t fertility) {
    return 0.25f + 0.75f * ((float)fertility / 255.0f);
}

float CropWeedModifier(uint8_t weedLevel) {
    if (weedLevel < WEED_THRESHOLD) return 1.0f;
    if (weedLevel < WEED_SEVERE) return 0.5f;
    return 0.25f;
}

// =============================================================================
// Farm Tick
// =============================================================================

void FarmTick(float dt) {
    if (farmActiveCells == 0) return;

    farmTickAccumulator += dt;
    float interval = GameHoursToGameSeconds(FARM_TICK_INTERVAL);
    if (farmTickAccumulator < interval) return;
    farmTickAccumulator = 0;

    float seasonWeedRate = GetSeasonalWeedRate();
    Season season = GetCurrentSeason();

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                FarmCell* fc = &farmGrid[z][y][x];
                if (!fc->tilled) continue;

                // === Weed growth ===
                if (seasonWeedRate > 0.0f) {
                    int weedGrowth = (int)(WEED_GROWTH_PER_TICK * seasonWeedRate);
                    if (weedGrowth < 1) weedGrowth = 1;
                    int newWeed = fc->weedLevel + weedGrowth;
                    if (newWeed > 255) newWeed = 255;
                    fc->weedLevel = (uint8_t)newWeed;
                }

                // === Crop growth ===
                if (fc->cropType == CROP_NONE) continue;
                if (fc->growthStage == CROP_STAGE_RIPE) continue;  // Already ripe
                if (fc->growthStage == CROP_STAGE_BARE) continue;  // Just planted, needs sprouting

                // Compute composite growth rate
                float seasonMod = CropSeasonModifier(fc->cropType, season);

                // Season kill: if season modifier is 0.0 and crop is growing, kill it
                if (seasonMod <= 0.0f) {
                    EventLog("Crop %d at (%d,%d,z%d) killed by season", fc->cropType, x, y, z);
                    fc->cropType = CROP_NONE;
                    fc->growthStage = CROP_STAGE_BARE;
                    fc->growthProgress = 0;
                    fc->frostDamaged = 0;
                    continue;
                }

                float tempMod = CropTemperatureModifier(GetTemperature(x, y, z));

                // Frost damage check
                if (GetTemperature(x, y, z) <= CROP_FREEZE_TEMP && fc->growthStage > CROP_STAGE_BARE) {
                    fc->frostDamaged = 1;
                }

                int wetness = GET_CELL_WETNESS(x, y, z);
                float wetMod = CropWetnessModifier(wetness);
                float fertMod = CropFertilityModifier(fc->fertility);
                float weedMod = CropWeedModifier(fc->weedLevel);

                float rate = seasonMod * tempMod * wetMod * fertMod * weedMod;
                if (rate <= 0.0f) continue;

                // Advance growth progress
                // Each stage takes growthTimeGH / 4 game-hours (4 growth stages: sprouted→growing→mature→ripe)
                float growthTimeGH = CropGrowthTimeGH(fc->cropType);
                float stageTimeGH = growthTimeGH / 4.0f;
                float stageTimeSec = GameHoursToGameSeconds(stageTimeGH);

                // progress increment per tick = (tickInterval / stageTime) * rate * 255
                float tickTimeSec = GameHoursToGameSeconds(FARM_TICK_INTERVAL);
                float increment = (tickTimeSec / stageTimeSec) * rate * 255.0f;

                int newProgress = fc->growthProgress + (int)(increment + 0.5f);
                if (newProgress >= 255) {
                    fc->growthProgress = 0;
                    fc->growthStage++;
                    if (fc->growthStage > CROP_STAGE_RIPE) {
                        fc->growthStage = CROP_STAGE_RIPE;
                    }
                } else {
                    fc->growthProgress = (uint8_t)newProgress;
                }
            }
        }
    }
}
