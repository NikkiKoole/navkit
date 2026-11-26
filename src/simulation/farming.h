#ifndef FARMING_H
#define FARMING_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"
#include "../world/material.h"

// Crop types
typedef enum {
    CROP_NONE = 0,
    CROP_WHEAT,
    CROP_LENTILS,
    CROP_FLAX,
    CROP_TYPE_COUNT
} CropType;

// Crop growth stages
typedef enum {
    CROP_STAGE_BARE = 0,      // Just planted, seed in ground
    CROP_STAGE_SPROUTED,      // Small sprout visible
    CROP_STAGE_GROWING,       // Leafy growth
    CROP_STAGE_MATURE,        // Full size, developing fruit/grain
    CROP_STAGE_RIPE,          // Ready for harvest
} CropStage;

// Farm cell state (8 bytes, all uint8_t for packing)
typedef struct {
    uint8_t fertility;       // 0-255, depletes on crop harvest, restored by compost
    uint8_t weedLevel;       // 0-255, accumulates over time on tilled cells
    uint8_t tilled;          // 1 = has been tilled
    uint8_t desiredCropType; // what player wants planted here (CropType)
    uint8_t cropType;        // what's currently growing (CropType, CROP_NONE = nothing)
    uint8_t growthStage;     // CropStage (0-4)
    uint8_t growthProgress;  // 0-255, stage-up at 255
    uint8_t frostDamaged : 1; // 1 = frost reduced yield by 50%
    uint8_t stress : 7;       // 0-127: too dry or too wet for a while (CROP_STRESS_DEATH: the crop dies)
} FarmCell;
_Static_assert(sizeof(FarmCell) == 8, "FarmCell is saved as 8 raw bytes (stress lives in frostDamaged's spare bits: old saves read 0)");

// Farm grid (parallel to main grid, zero-initialized = no farm data)
// Sparse (grid.h SparseLayer): memory only in chunks that hold farm state. FARM(x, y, z) is
// read-only (a cell in a chunk without a block reads as all zero); write with FARM_MUT.
extern SparseLayer farmLayer;
#define FARM(x, y, z) (*(const FarmCell*)SparseLayerRead(&farmLayer, (x), (y), (z)))
#define FARM_MUT(x, y, z) (*(FarmCell*)SparseLayerWrite(&farmLayer, (x), (y), (z)))
extern int farmActiveCells;  // count of tilled cells (for sim tick skipping)

// Fertility constants
#define FERTILITY_DEFAULT   128
#define FERTILITY_LOW       64   // Below this, auto-fertilize triggers

// Weed constants
#define WEED_THRESHOLD      128  // Above this, crop penalty 0.5x
#define WEED_SEVERE         200  // Above this, crop penalty 0.25x
#define WEED_GROWTH_PER_TICK  3  // Base weed growth per farm tick

// Farm tick interval (game-hours)
#define FARM_TICK_INTERVAL  0.5f

// Work times (game-hours)
#define TILL_WORK_TIME      1.0f
#define TEND_WORK_TIME      0.4f
#define FERTILIZE_WORK_TIME 0.3f
#define PLANT_CROP_WORK_TIME 0.3f
#define HARVEST_CROP_WORK_TIME 0.4f

// Fertilize amount
#define FERTILIZE_AMOUNT    80

// Watering
#define WATER_CROP_WORK_TIME 0.3f   // Game-hours to pour water
#define WATER_POUR_MOISTURE  160    // Watering wets the soil to at least this: moist, inside every crop's range, below mud (180)

// Growth times (game-hours per full growth cycle at 1.0x rate)
#define WHEAT_GROWTH_GH     72.0f
#define LENTIL_GROWTH_GH    48.0f
#define FLAX_GROWTH_GH      60.0f

// Temperature thresholds (Celsius)
#define CROP_FREEZE_TEMP    0.0f
#define CROP_COLD_TEMP      5.0f
#define CROP_IDEAL_LOW      10.0f
#define CROP_IDEAL_HIGH     25.0f
#define CROP_HOT_TEMP       35.0f

// Fertility deltas on harvest
#define WHEAT_FERTILITY_DELTA   -20
#define LENTIL_FERTILITY_DELTA   60
#define FLAX_FERTILITY_DELTA    -15

// Initialize / clear
void InitFarming(void);
void ClearFarming(void);

// Tick (weed accumulation + crop growth, called from main loop)
void FarmTick(float dt);

// Soil check: can this cell be designated as farm?
bool IsFarmableSoil(int x, int y, int z);

// Initial fertility based on soil material
uint8_t InitialFertilityForSoil(MaterialType mat);

// Seasonal weed rate modifier
float GetSeasonalWeedRate(void);

// Accessors
FarmCell* GetFarmCell(int x, int y, int z);

// Crop/seed type conversion
int SeedTypeForCrop(CropType crop);   // Returns ItemType
CropType CropTypeForSeed(int seedType); // Takes ItemType

// Growth modifier functions (pure, no side effects)
float CropSeasonModifier(CropType crop, int season);
float CropTemperatureModifier(float tempC);
float CropWetnessModifier(int wetness);   // (the old 0-3 buckets; growth uses CropMoistureModifier)

// Soil moisture per crop (moisture.h values 0-255): full growth inside its ideal range, slower
// outside it; below `drought` or at/above `flood` the crop builds up stress (+2 a farm tick,
// -1 when fine) and dies at CROP_STRESS_DEATH: ~a day of drought (wilted) or waterlogging (rotted)
typedef struct { int idealLow, idealHigh, drought, flood; } CropWater;
#define CROP_STRESS_DEATH 96
#define CROP_STRESS_SHOWS 16     // from here the crop looks and reads stressed
const CropWater* CropWaterFor(CropType crop);
float CropMoistureModifier(CropType crop, int moisture);
// What the soil is like for this crop: "too dry" / "good" / "too wet" (NULL for no crop)
const char* CropMoistureVerdict(CropType crop, int moisture);
float CropFertilityModifier(uint8_t fertility);
float CropWeedModifier(uint8_t weedLevel);

// Growth time for a crop type (game-hours)
float CropGrowthTimeGH(CropType crop);
// Worth planting now: the crop grows this season, and if the next season kills it, there is
// time left to ripen first (at this season's pace). A crop the season kills takes its seed
// with it: a town that replanted every harvest lost all its seed at the start of winter.
bool CropWorthPlantingNow(CropType crop);

#endif // FARMING_H
