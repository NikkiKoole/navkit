#include "farming.h"
#include "moisture.h"
#include "../core/sim_manager.h"
#include "../core/event_log.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../simulation/balance.h"
#include "../simulation/weather.h"
#include "../entities/items.h"
#include "../entities/town.h"
#include <string.h>

// Grid storage
int farmActiveCells = 0;

// Tick accumulator
static float farmTickAccumulator = 0;

void InitFarming(void) {
    ClearFarming();
}

void ClearFarming(void) {
    SparseLayerClear(&farmLayer);
    farmActiveCells = 0;
    farmTickAccumulator = 0;
}

bool IsFarmableSoil(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight
        || z < 0 || z >= gridDepth) return false;
    if (z == 0) return false;  // can't farm on bedrock layer

    // Must be walkable surface (air cell with solid below)
    if (CellIsSolid(GRID(x, y, z))) return false;
    if (!CellIsSolid(GRID(x, y, z - 1))) return false;

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
    return &FARM_MUT(x, y, z);
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

bool CropWorthPlantingNow(CropType crop) {
    int season = (int)GetCurrentSeason();
    float mod = CropSeasonModifier(crop, season);
    if (mod <= 0.0f) return false;
    if (CropSeasonModifier(crop, (season + 1) % 4) > 0.0f) return true;   // it can carry on next season
    int dayInSeason = daysPerSeason > 0 ? GetYearDay() % daysPerSeason : 0;
    float hoursLeft = (float)(daysPerSeason - dayInSeason) * 24.0f - timeOfDay;
    return hoursLeft >= CropGrowthTimeGH(crop) / mod;
}

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

// Wheat: normal to moist ground. Lentils: cope with dry ground, hate wet feet. Flax: likes it
// moist, takes soaked ground. (Normal ground is MOISTURE_BASELINE 100, wet 180, soaked 230.)
static const CropWater cropWater[CROP_TYPE_COUNT] = {
    [CROP_NONE]    = {  0, 255,  0, 256 },
    [CROP_WHEAT]   = { 90, 200, 30, 240 },
    [CROP_LENTILS] = { 60, 170, 15, 230 },
    [CROP_FLAX]    = {120, 235, 45, 256 },
};

const CropWater* CropWaterFor(CropType crop) {
    return (crop > CROP_NONE && crop < CROP_TYPE_COUNT) ? &cropWater[crop] : &cropWater[CROP_NONE];
}

float CropMoistureModifier(CropType crop, int moisture) {
    const CropWater* w = CropWaterFor(crop);
    int off = moisture < w->idealLow ? w->idealLow - moisture : moisture > w->idealHigh ? moisture - w->idealHigh : 0;
    float f = 1.0f - 0.7f * (float)off / 60.0f;   // 60 points outside the range: 0.3x
    return f < 0.1f ? 0.1f : f;
}

const char* CropMoistureVerdict(CropType crop, int moisture) {
    if (crop <= CROP_NONE || crop >= CROP_TYPE_COUNT) return NULL;
    const CropWater* w = CropWaterFor(crop);
    if (moisture < w->idealLow) return moisture < w->drought ? "far too dry" : "too dry";
    if (moisture > w->idealHigh) return moisture >= w->flood ? "waterlogged" : "too wet";
    return "good";
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
            if ((y & STORE_MASK) == 0 && !SparseLayerBandHasBlock(&farmLayer, y)) { y |= STORE_MASK; continue; }
            for (int x = 0; x < gridWidth; x++) {
                // A chunk without a farm block has no tilled cells
                if (!SparseLayerHasBlock(&farmLayer, x, y)) { x |= STORE_MASK; continue; }
                if (!FARM(x, y, z).tilled) continue;
                FarmCell* fc = &FARM_MUT(x, y, z);

                // === Weed growth ===
                if (seasonWeedRate > 0.0f) {
                    int weedGrowth = (int)(WEED_GROWTH_PER_TICK * seasonWeedRate);
                    if (weedGrowth < 1) weedGrowth = 1;
                    float slower = TownWeedFactor(x, y, z);   // town farms: weeded about once a crop
                    if (slower < 1.0f) {
                        float g = WEED_GROWTH_PER_TICK * seasonWeedRate * slower;
                        weedGrowth = (int)g;
                        if (GetRandomValue(0, 999) < (int)((g - weedGrowth) * 1000.0f)) weedGrowth++;
                    }
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
                    fc->stress = 0;
                    continue;
                }

                // Soil moisture (the soil is the cell below the farm cell): too dry or too wet
                // for long kills the crop, otherwise it slows growth outside the crop's range
                int moisture = z > 0 ? SoilMoistureAt(x, y, z - 1) : MOISTURE_BASELINE;
                const CropWater* cw = CropWaterFor((CropType)fc->cropType);
                if (moisture < cw->drought || moisture >= cw->flood) {
                    int st = fc->stress + 2;
                    if (st >= CROP_STRESS_DEATH) {
                        EventLog("Crop %d at (%d,%d,z%d) %s", fc->cropType, x, y, z,
                                 moisture < cw->drought ? "wilted (drought)" : "rotted (waterlogged)");
                        fc->cropType = CROP_NONE;
                        fc->growthStage = CROP_STAGE_BARE;
                        fc->growthProgress = 0;
                        fc->frostDamaged = 0;
                        fc->stress = 0;
                        continue;
                    }
                    fc->stress = (uint8_t)st;
                } else if (fc->stress > 0) {
                    fc->stress--;
                }

                float tempMod = CropTemperatureModifier(GetTemperature(x, y, z));

                // Frost damage check
                if (GetTemperature(x, y, z) <= CROP_FREEZE_TEMP && fc->growthStage > CROP_STAGE_BARE) {
                    fc->frostDamaged = 1;
                }

                float wetMod = CropMoistureModifier((CropType)fc->cropType, moisture);
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
