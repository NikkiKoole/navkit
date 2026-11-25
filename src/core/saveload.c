// core/saveload.c - Save and Load functions
#include "../game_state.h"

// Forward declarations
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);
void RebuildPostLoadState(void);
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../entities/inventory.h"   // RecalcAllCarried
#include "../entities/belts.h"
#include "../entities/relations.h"
#include "../entities/elevators.h"
#include "../simulation/fire.h"
#include "../simulation/tide.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/floordirt.h"
#include "../simulation/lighting.h"
#include "../simulation/weather.h"
#include "../simulation/plants.h"
#include "../simulation/farming.h"
#include "../simulation/balance.h"
#include "../core/sim_manager.h"
#include "../world/material.h"
#include "../world/biome.h"
#include "../entities/tool_quality.h"
#include "../entities/namegen.h"
#include "save_migrations.h"
#include "save_rle.h"

#define V21_MAT_COUNT 10  // MAT_COUNT before clay/gravel/sand/peat materials
#define SAVE_MAGIC 0x4E41564B  // "NAVK"

// Section markers (readable in hex dump)
#define MARKER_GRIDS    0x47524944  // "GRID"

// Row I/O for per-cell layers. Each layer is one stream of rows in [z][y][x] order (the save
// format), read and written through the layer macro element by element, so the in-memory
// layout (CELL_AT) can differ from the file layout. MACRO is a layer accessor like WATER or GRID.
// Since v95 each layer's stream is run-length coded (save_rle.h): the stream starts at the
// layer's first row (y == 0, z == 0) and ends after its last row. Older saves are plain rows.
static _Alignas(16) unsigned char saveRowBuf[MAX_GRID_WIDTH * 32];
static RleWriter saveRle;
static RleReader loadRle;
static bool loadRleMode;           // set by LoadWorld from the file's version
static bool loadRleOk = true;      // false if any layer stream was malformed

static void SaveRowOut(FILE* f, size_t es, int y, int z) {
    if (y == 0 && z == 0) RleWriterBegin(&saveRle, f, es);
    RleWriterPush(&saveRle, saveRowBuf, gridWidth);
    if (y == gridHeight - 1 && z == gridDepth - 1) RleWriterEnd(&saveRle);
}

static void LoadRowIn(FILE* f, size_t es, int y, int z) {
    if (!loadRleMode) { fread(saveRowBuf, es, gridWidth, f); return; }
    if (y == 0 && z == 0) RleReaderBegin(&loadRle, f, es);
    RleReaderPull(&loadRle, saveRowBuf, (uint64_t)gridWidth);
    if (y == gridHeight - 1 && z == gridDepth - 1 && !RleReaderEnd(&loadRle)) loadRleOk = false;
}

// n bytes of zeros? (8 at a time; n is at most 16 elements of up to 32 bytes)
static inline bool RowRunIsZero(const unsigned char* p, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) { uint64_t v; memcpy(&v, p + i, 8); if (v) return false; }
    for (; i < n; i++) if (p[i]) return false;
    return true;
}

static inline bool RowElemIsZero(const unsigned char* p, size_t es) {
    for (size_t i = 0; i < es; i++) if (p[i]) return false;
    return true;
}

#define ROW_ELEM_SIZE(MACRO) sizeof(MACRO(0, 0, 0))
#define ROW_TO_BUF(MACRO, y, z) do { \
        _Static_assert(sizeof(MACRO(0, 0, 0)) <= 32, "saveRowBuf element too small"); \
        for (int x_ = 0; x_ < gridWidth; x_++) \
            memcpy(saveRowBuf + (size_t)x_ * ROW_ELEM_SIZE(MACRO), &MACRO(x_, (y), (z)), ROW_ELEM_SIZE(MACRO)); \
    } while (0)
#define ROW_FROM_BUF(MACRO, y, z) do { \
        for (int x_ = 0; x_ < gridWidth; x_++) \
            memcpy(&MACRO(x_, (y), (z)), saveRowBuf + (size_t)x_ * ROW_ELEM_SIZE(MACRO), ROW_ELEM_SIZE(MACRO)); \
    } while (0)
// Into a layer cleared at its first row: write only non-zero elements, so loading leaves empty
// memory untouched (a written zero would still cost a page).
// Cells of a 16-cell storage run are contiguous: address computed once per run.
#define ROW_FROM_BUF_NZ(MACRO, y, z) do { \
        for (int xs_ = 0; xs_ < gridWidth; xs_ += STORE_CHUNK) { \
            int xe_ = xs_ + STORE_CHUNK < gridWidth ? xs_ + STORE_CHUNK : gridWidth; \
            const unsigned char* e_ = saveRowBuf + (size_t)xs_ * ROW_ELEM_SIZE(MACRO); \
            if (RowRunIsZero(e_, (size_t)(xe_ - xs_) * ROW_ELEM_SIZE(MACRO))) continue; \
            unsigned char* d_ = (unsigned char*)&MACRO(xs_, (y), (z)); \
            for (int k_ = 0; k_ < xe_ - xs_; k_++, d_ += ROW_ELEM_SIZE(MACRO), e_ += ROW_ELEM_SIZE(MACRO)) \
                if (!RowElemIsZero(e_, ROW_ELEM_SIZE(MACRO))) memcpy(d_, e_, ROW_ELEM_SIZE(MACRO)); \
        } \
    } while (0)
// &MACRO(0, 0, 0) is the layer's first element (CellIndex(0, 0, 0) == 0): clear the whole layer.
#define LOAD_CLEAR_AT_FIRST_ROW(MACRO, y, z) do { \
        if ((y) == 0 && (z) == 0) ClearGridLayer(&MACRO(0, 0, 0), ROW_ELEM_SIZE(MACRO), 0); \
    } while (0)
#define SAVE_ROW(f, MACRO, y, z) do { ROW_TO_BUF(MACRO, y, z); SaveRowOut((f), ROW_ELEM_SIZE(MACRO), (y), (z)); } while (0)
#define LOAD_ROW(f, MACRO, y, z) do { LOAD_CLEAR_AT_FIRST_ROW(MACRO, y, z); \
        LoadRowIn((f), ROW_ELEM_SIZE(MACRO), (y), (z)); ROW_FROM_BUF_NZ(MACRO, y, z); } while (0)
// Sparse layer (grid.h SparseLayer): write only cells that differ from the empty value, so
// loading a mostly-empty layer doesn't allocate a block for every chunk.
#define LOAD_ROW_SPARSE(f, LAYER, MUTMACRO, y, z) do { \
        size_t es_ = (LAYER).elemSize; \
        LoadRowIn((f), es_, (y), (z)); \
        for (int x_ = 0; x_ < gridWidth; x_++) { \
            if (memcmp(saveRowBuf + (size_t)x_ * es_, (LAYER).empty, es_) != 0) \
                memcpy(&MUTMACRO(x_, (y), (z)), saveRowBuf + (size_t)x_ * es_, es_); \
        } \
    } while (0)
// Byte layers stored with an XOR in memory (source items, explored): the file keeps the plain value.
#define SAVE_ROW_XOR(f, MACRO, y, z, xorv) do { ROW_TO_BUF(MACRO, y, z); \
        for (int x_ = 0; x_ < gridWidth; x_++) saveRowBuf[x_] ^= (unsigned char)(xorv); \
        SaveRowOut((f), 1, (y), (z)); } while (0)
#define LOAD_ROW_XOR(f, MACRO, y, z, xorv) do { LOAD_CLEAR_AT_FIRST_ROW(MACRO, y, z); \
        LoadRowIn((f), 1, (y), (z)); \
        for (int x_ = 0; x_ < gridWidth; x_++) saveRowBuf[x_] ^= (unsigned char)(xorv); \
        ROW_FROM_BUF_NZ(MACRO, y, z); } while (0)
#define MARKER_ENTITIES 0x454E5449  // "ENTI"
#define MARKER_VIEW     0x56494557  // "VIEW"
#define MARKER_SETTINGS 0x53455454  // "SETT"
#define MARKER_END      0x454E4421  // "END!"

// Settings save/load macro table
// X-macro for all tweakable simulation settings
// Adding a new setting: add one line here, it will be saved/loaded automatically
// Animal respawn used to be saved in seconds; the slot stays so the save format is unchanged.
static float savedUnusedAnimalSpawnSeconds = 180.0f;

#define SETTINGS_TABLE(X) \
    /* Game time */ \
    X(double, gameTime) \
    X(float, timeOfDay) \
    X(int, dayNumber) \
    X(float, gameSpeed) \
    X(unsigned long, currentTick) \
    /* Water */ \
    X(bool, waterEnabled) \
    X(bool, waterEvaporationEnabled) \
    X(float, waterEvapInterval) \
    X(float, waterSpeedShallow) \
    X(float, waterSpeedMedium) \
    X(float, waterSpeedDeep) \
    X(float, mudSpeedMultiplier) \
    X(float, wetnessSyncInterval) \
    /* Fire */ \
    X(bool, fireEnabled) \
    X(float, fireSpreadInterval) \
    X(float, fireFuelInterval) \
    X(int, fireWaterReduction) \
    X(float, fireSpreadBase) \
    X(float, fireSpreadPerLevel) \
    /* Smoke */ \
    X(bool, smokeEnabled) \
    X(float, smokeRiseInterval) \
    X(float, smokeDissipationTime) \
    X(float, smokeGenerationRate) \
    /* Steam */ \
    X(bool, steamEnabled) \
    X(float, steamRiseInterval) \
    X(int, steamCondensationTemp) \
    X(int, steamGenerationTemp) \
    /* Temperature */ \
    X(bool, temperatureEnabled) \
    X(int, ambientSurfaceTemp) \
    X(int, ambientDepthDecay) \
    X(float, heatTransferInterval) \
    X(float, tempDecayInterval) \
    X(int, heatSourceTemp) \
    X(int, coldSourceTemp) \
    X(float, heatRiseBoost) \
    X(float, heatSinkReduction) \
    X(float, heatDecayPercent) \
    X(float, diagonalTransferPercent) \
    /* Ground wear */ \
    X(bool, groundWearEnabled) \
    X(int, wearGrassToDirt) \
    X(int, wearDirtToGrass) \
    X(int, wearTrampleAmount) \
    X(float, wearDecayRate) \
    X(float, wearRecoveryInterval) \
    X(int, wearMax) \
    /* Trees */ \
    X(float, saplingGrowGH) \
    X(float, trunkGrowGH) \
    X(bool, saplingRegrowthEnabled) \
    X(float, saplingRegrowthChance) \
    X(int, saplingMinTreeDistance) \
    /* Seasons */ \
    X(int, daysPerSeason) \
    X(int, baseSurfaceTemp) \
    X(int, seasonalAmplitude) \
    /* Weather */ \
    X(bool, weatherEnabled) \
    X(float, weatherMinDuration) \
    X(float, weatherMaxDuration) \
    X(float, rainWetnessInterval) \
    X(float, heavyRainWetnessInterval) \
    X(float, intensityRampSpeed) \
    X(float, windDryingMultiplier) \
    /* Snow */ \
    X(float, snowAccumulationRate) \
    X(float, snowMeltingRate) \
    /* Lightning */ \
    X(float, lightningInterval) \
    /* Animals */ \
    X(bool, animalRespawnEnabled) \
    X(int, animalTargetPopulation) \
    X(float, savedUnusedAnimalSpawnSeconds) /* was seconds; now animalSpawnIntervalGH, not saved */ \
    /* Trains */ \
    X(bool, trainQueueEnabled)

// Night energy drain was planned but never used, and is not wanted (energy must not depend on the clock).
static float savedUnusedNightEnergyMult = 1.2f;

#define BALANCE_SETTINGS_TABLE(X) \
    X(float, balance.baseMoverSpeed) \
    X(float, balance.moverSpeedVariance) \
    X(float, balance.workHoursPerDay) \
    X(float, balance.sleepHoursInBed) \
    X(float, balance.sleepOnGround) \
    X(float, balance.hoursToStarve) \
    X(float, balance.hoursToExhaustWorking) \
    X(float, balance.hoursToExhaustIdle) \
    X(float, balance.eatingDurationGH) \
    X(float, balance.hungerSeekThreshold) \
    X(float, balance.hungerCriticalThreshold) \
    X(float, balance.energyTiredThreshold) \
    X(float, balance.energyExhaustedThreshold) \
    X(float, balance.energyWakeThreshold) \
    X(float, savedUnusedNightEnergyMult) /* removed setting; slot kept so the save format is unchanged */ \
    X(float, balance.carryingEnergyMult) \
    X(float, balance.hungerSpeedPenaltyMin) \
    X(float, balance.hungerPenaltyThreshold) \
    X(float, balance.hoursToDehydrate) \
    X(float, balance.thirstSeekThreshold) \
    X(float, balance.thirstCriticalThreshold) \
    X(float, balance.drinkingDurationGH) \
    X(float, balance.dehydrationDeathGH) \
    X(float, balance.naturalDrinkDurationGH) \
    X(float, balance.naturalDrinkHydration)

bool SaveWorld(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        AddMessage(TextFormat("Failed to open %s for writing", filename), RED);
        return false;
    }
    
    // Header
    uint32_t magic = SAVE_MAGIC;
    uint32_t version = CURRENT_SAVE_VERSION;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    
    // World seed (for reproducible terrain regeneration)
    fwrite(&worldSeed, sizeof(worldSeed), 1, f);
    
    // Grid dimensions
    fwrite(&gridWidth, sizeof(gridWidth), 1, f);
    fwrite(&gridHeight, sizeof(gridHeight), 1, f);
    fwrite(&gridDepth, sizeof(gridDepth), 1, f);
    fwrite(&chunkWidth, sizeof(chunkWidth), 1, f);
    fwrite(&chunkHeight, sizeof(chunkHeight), 1, f);
    
    // Game mode and needs toggles (v62+)
    {
        uint8_t gm = (uint8_t)gameMode;
        fwrite(&gm, sizeof(gm), 1, f);
        fwrite(&hungerEnabled, sizeof(bool), 1, f);
        fwrite(&energyEnabled, sizeof(bool), 1, f);
        fwrite(&bodyTempEnabled, sizeof(bool), 1, f);
        fwrite(&toolRequirementsEnabled, sizeof(bool), 1, f);
        fwrite(&thirstEnabled, sizeof(bool), 1, f);
        fwrite(&bladderEnabled, sizeof(bool), 1, f);
        fwrite(&selectedBiome, sizeof(int), 1, f);
    }

    // === GRIDS SECTION ===
    uint32_t marker = MARKER_GRIDS;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, GRID, y, z);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, WATER, y, z);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, FIRE, y, z);
        }
    }
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, SMOKE, y, z);
        }
    }
    
    // Steam grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, STEAM, y, z);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, CELL_FLAGS, y, z);
        }
    }
    
    // Wall material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, WALL_MAT, y, z);
        }
    }
    
    // Floor material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, FLOOR_MAT, y, z);
        }
    }

    // Wall natural grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, WALL_NATURAL, y, z);
        }
    }

    // Floor natural grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, FLOOR_NATURAL, y, z);
        }
    }

    // Wall finish grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, WALL_FINISH, y, z);
        }
    }

    // Floor finish grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, FLOOR_FINISH, y, z);
        }
    }

    // Wall source item grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW_XOR(f, WALL_SOURCE_ITEM, y, z, SOURCE_ITEM_XOR);
        }
    }

    // Floor source item grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW_XOR(f, FLOOR_SOURCE_ITEM, y, z, SOURCE_ITEM_XOR);
        }
    }

    // Vegetation grid (V29)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, VEGETATION, y, z);
        }
    }
    
    // Snow grid (V45)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, SNOW, y, z);
        }
    }
    
    // Temperature grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, TEMPERATURE, y, z);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, DESIGNATION, y, z);
        }
    }
    
    // Wear grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, WEAR, y, z);
        }
    }

    // Tree growth timer grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, GROWTH_TIMER, y, z);
        }
    }

    // Tree target height grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, TARGET_HEIGHT, y, z);
        }
    }

    // Tree harvest state grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, TREE_HARVEST, y, z);
        }
    }

    // Floor dirt grid (v36+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, FLOOR_DIRT, y, z);
        }
    }

    // Explored grid (fog of war, v75+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW_XOR(f, UNEXPLORED, y, z, 1);   // file: 1 = explored
        }
    }

    // Farm grid (v76+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, FARM, y, z);
        }
    }
    fwrite(&farmActiveCells, sizeof(farmActiveCells), 1, f);

    // Track connections grid (v89+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, TRACK_CONN, y, z);
        }
    }

    // One-way grid (v110+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            SAVE_ROW(f, ONE_WAY, y, z);
        }
    }

    // === ENTITIES SECTION ===
    marker = MARKER_ENTITIES;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Items
    fwrite(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    fwrite(items, sizeof(Item), itemHighWaterMark, f);
    
    // Stockpiles
    fwrite(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    
    // Gather zones
    fwrite(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fwrite(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fwrite(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Workshops
    fwrite(workshops, sizeof(Workshop), MAX_WORKSHOPS, f);
    
    // Movers (v69+: struct without path, then paths separately)
    fwrite(&moverCount, sizeof(moverCount), 1, f);
    fwrite(movers, sizeof(Mover), moverCount, f);
    for (int i = 0; i < moverCount; i++) {
        fwrite(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
    }

    // Animals (v42+)
    fwrite(&animalCount, sizeof(animalCount), 1, f);
    fwrite(animals, sizeof(Animal), animalCount, f);

    // Trains (v88+: Train has multi-car trail fields)
    fwrite(&trainCount, sizeof(trainCount), 1, f);
    fwrite(trains, sizeof(Train), trainCount, f);

    // Stations (v88+)
    fwrite(&stationCount, sizeof(stationCount), 1, f);
    fwrite(stations, sizeof(TrainStation), stationCount, f);

    // Jobs
    fwrite(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fwrite(&activeJobCount, sizeof(activeJobCount), 1, f);
    fwrite(jobs, sizeof(Job), jobHighWaterMark, f);
    fwrite(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fwrite(activeJobList, sizeof(int), activeJobCount, f);
    
    // Light sources (v37+)
    fwrite(&lightSourceCount, sizeof(lightSourceCount), 1, f);
    fwrite(lightSources, sizeof(LightSource), lightSourceCount, f);
    
    // Plants (v48+)
    fwrite(&plantCount, sizeof(plantCount), 1, f);
    fwrite(plants, sizeof(Plant), plantCount, f);
    
    // Furniture (v54+)
    fwrite(&furnitureCount, sizeof(furnitureCount), 1, f);
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (furniture[i].active) {
            fwrite(&furniture[i], sizeof(Furniture), 1, f);
        }
    }

    // Conveyor belts (v98+); items riding them are saved as ITEM_ON_BELT with the items
    fwrite(&beltCount, sizeof(beltCount), 1, f);
    fwrite(belts, sizeof(Belt), beltCount, f);

    // Relations (v111+): who knows whom, one MoverRelations per saved mover
    fwrite(moverRelations, sizeof(MoverRelations), moverCount, f);
    
    // === VIEW SECTION ===
    marker = MARKER_VIEW;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // View state
    fwrite(&currentViewZ, sizeof(currentViewZ), 1, f);
    fwrite(&zoom, sizeof(zoom), 1, f);
    fwrite(&offset, sizeof(offset), 1, f);
    
    // === SETTINGS SECTION ===
    marker = MARKER_SETTINGS;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Simulation settings (generated from SETTINGS_TABLE macro)
    #define WRITE_SETTING(type, name) fwrite(&name, sizeof(type), 1, f);
    SETTINGS_TABLE(WRITE_SETTING)
    BALANCE_SETTINGS_TABLE(WRITE_SETTING)
    #undef WRITE_SETTING
    // v60+: diurnal amplitude
    fwrite(&diurnalAmplitude, sizeof(int), 1, f);
    // v99+: train settings and endless mode (scenarios change them; a load kept the old values)
    fwrite(&trainSpeed, sizeof(float), 1, f);
    fwrite(&useTrainTimeChoice, sizeof(bool), 1, f);
    fwrite(&useTrainTransfers, sizeof(bool), 1, f);
    fwrite(&trainFreight, sizeof(bool), 1, f);
    fwrite(&endlessMoverMode, sizeof(bool), 1, f);
    // v100+: elevator settings
    fwrite(&elevatorSpeed, sizeof(float), 1, f);
    fwrite(&elevatorDoorTime, sizeof(float), 1, f);
    fwrite(&elevatorCapacity, sizeof(int), 1, f);
    fwrite(&useElevatorWaitCost, sizeof(bool), 1, f);
    // v101+: map-edge water
    fwrite(&waterEdgeOutflow, sizeof(bool), 1, f);
    fwrite(&waterEdgeSeaZ, sizeof(int), 1, f);
    // v102+: tide
    fwrite(&tideEnabled, sizeof(bool), 1, f);
    fwrite(&tidePeriodHours, sizeof(float), 1, f);
    fwrite(&tideLowLevel, sizeof(int), 1, f);
    // v103+: springs
    fwrite(&springTrickle, sizeof(bool), 1, f);
    fwrite(&springRate, sizeof(float), 1, f);

    // Simulation accumulators (static locals, saved via getters)
    float accum;
    accum = GetFireSpreadAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetFireFuelAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetWaterEvapAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetSmokeRiseAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetSmokeDissipationAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetSteamRiseAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetHeatTransferAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetTempDecayAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetWearRecoveryAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetRainWetnessAccum(); fwrite(&accum, sizeof(float), 1, f);
    accum = GetWeatherWindAccum(); fwrite(&accum, sizeof(float), 1, f);

    // Weather state
    fwrite(&weatherState, sizeof(WeatherState), 1, f);
    
    // === END MARKER ===
    marker = MARKER_END;
    fwrite(&marker, sizeof(marker), 1, f);
    
    fclose(f);
    return true;
}

// Rebuild transient state that isn't saved: entity counts, job free list,
// and clear stale item reservations. Called after LoadWorld and usable from tests.
void RebuildPostLoadState(void) {
    // Rebuild entity count globals
    itemCount = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active) itemCount++;
    }
    stockpileCount = 0;
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (stockpiles[i].active) stockpileCount++;
    }
    workshopCount = 0;
    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        if (workshops[i].active) workshopCount++;
    }
    blueprintCount = 0;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (blueprints[i].active) blueprintCount++;
    }
    
    // Rebuild furniture move cost grid and clear stale occupants
    RebuildFurnitureMoveCostGrid();
    RebuildCapabilityGrid();   // not saved: prep surfaces of tables, counters, ...
    RebuildContainerLists();   // contents lists follow containedIn (old saves have none)
    RecalcAllCarried();        // carried weight per mover (inventory.c, not saved)
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (furniture[i].active && furniture[i].occupant >= 0) {
            if (furniture[i].occupant >= moverCount || !movers[furniture[i].occupant].active) {
                furniture[i].occupant = -1;
            }
        }
    }
    
    // Rebuild job free list (not saved, must be reconstructed from gaps)
    jobFreeCount = 0;
    for (int i = 0; i < jobHighWaterMark; i++) {
        if (!jobs[i].active && !jobIsActive[i]) {
            jobFreeList[jobFreeCount++] = i;
        }
    }
    
    // Clear transient item reservations (not meaningful across save/load)
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active) {
            items[i].reservedBy = -1;
        }
    }

    // ...but the active jobs were loaded and carry on: give them back the items they hold
    // and the stockpile slots haul jobs are heading for (the counts were cleared on load).
    // Without this an idle mover picks up a load another job is about to fetch, and a
    // slot can overfill. Same rules as the state audit (item <-> job, slot <-> haul job).
    for (int j = 0; j < jobHighWaterMark; j++) {
        const Job* job = &jobs[j];
        if (!job->active || job->assignedMover < 0) continue;
        int held[5] = { job->targetItem, job->carryingItem, job->targetItem2, job->targetItem3, job->toolItem };
        for (int k = 0; k < 5; k++)
            if (held[k] >= 0 && held[k] < MAX_ITEMS && items[held[k]].active)
                items[held[k]].reservedBy = job->assignedMover;
        if (job->type == JOBTYPE_HAUL && job->targetStockpile >= 0 && job->targetStockpile < MAX_STOCKPILES) {
            Stockpile* sp = &stockpiles[job->targetStockpile];
            int lx = job->targetSlotX - sp->x, ly = job->targetSlotY - sp->y;
            if (sp->active && lx >= 0 && ly >= 0 && lx < sp->width && ly < sp->height)
                sp->reservedBy[ly * sp->width + lx]++;
        }
    }
    // ...and the movers their own gear: worn, held, put down for a job (inventory.c)
    ReserveAllMoverGear();

    // Rebuild waterActiveCells from loaded water grid
    waterActiveCells = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                const WaterCell* c = &WATER(x, y, z);
                if (c->level > 0 || c->isSource || c->isDrain) {
                    waterActiveCells++;
                }
            }
        }
    }
}

bool LoadWorld(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        AddMessage(TextFormat("Failed to open %s", filename), RED);
        return false;
    }
    
    // Check header
    uint32_t magic, version;
    fread(&magic, sizeof(magic), 1, f);
    fread(&version, sizeof(version), 1, f);
    
    if (magic != SAVE_MAGIC) {
        printf("ERROR: Invalid save file (bad magic: 0x%08X, expected 0x%08X)\n", magic, SAVE_MAGIC);
        AddMessage("Invalid save file (bad magic)", RED);
        fclose(f);
        return false;
    }
    
    // Support v82+ (with migration to current)
    if (version < MIN_SAVE_VERSION || version > CURRENT_SAVE_VERSION) {
        printf("ERROR: Save version mismatch (file: v%d, supported: v%d-v%d)\n", version, MIN_SAVE_VERSION, CURRENT_SAVE_VERSION);
        AddMessage(TextFormat("Save version mismatch: v%d (expected v%d-v%d).", version, MIN_SAVE_VERSION, CURRENT_SAVE_VERSION), RED);
        fclose(f);
        return false;
    }
    
    // World seed
    fread(&worldSeed, sizeof(worldSeed), 1, f);
    
    // Grid dimensions
    int newWidth, newHeight, newDepth, newChunkW, newChunkH;
    fread(&newWidth, sizeof(newWidth), 1, f);
    fread(&newHeight, sizeof(newHeight), 1, f);
    fread(&newDepth, sizeof(newDepth), 1, f);
    fread(&newChunkW, sizeof(newChunkW), 1, f);
    fread(&newChunkH, sizeof(newChunkH), 1, f);
    
    // Game mode and needs toggles
    {
        uint8_t gm;
        fread(&gm, sizeof(gm), 1, f);
        gameMode = (GameMode)gm;
        fread(&hungerEnabled, sizeof(bool), 1, f);
        fread(&energyEnabled, sizeof(bool), 1, f);
        fread(&bodyTempEnabled, sizeof(bool), 1, f);
        fread(&toolRequirementsEnabled, sizeof(bool), 1, f);
        fread(&thirstEnabled, sizeof(bool), 1, f);
        if (version >= 93) {
            fread(&bladderEnabled, sizeof(bool), 1, f);
        }
        if (version >= 84) {
            fread(&selectedBiome, sizeof(int), 1, f);
            if (selectedBiome < 0 || selectedBiome >= BIOME_COUNT) selectedBiome = 0;
        } else {
            selectedBiome = 0;
        }
    }
    
    // Reinitialize grid if dimensions don't match
    if (newWidth != gridWidth || newHeight != gridHeight || 
        newChunkW != chunkWidth || newChunkH != chunkHeight) {
        InitGridWithSizeAndChunkSize(newWidth, newHeight, newChunkW, newChunkH);
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    }
    gridDepth = newDepth;
    
    // Clear current state
    ClearMovers();
    ClearJobs();
    ClearGatherZones();
    
    // === GRIDS SECTION ===
    uint32_t marker;
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_GRIDS) {
        printf("ERROR: Bad GRID marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_GRIDS);
        AddMessage(TextFormat("Bad GRID marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    loadRleMode = (version >= V95_RLE_GRID_LAYERS);   // v95+: layers are run-length coded
    loadRleOk = true;
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            if (version >= V94_NARROW_GRID_LAYERS) LOAD_ROW(f, GRID, y, z);
            else { FreadLegacyIntsAsU8((uint8_t*)saveRowBuf, gridWidth, f); ROW_FROM_BUF(GRID, y, z); }
        }
    }
    
    // Water grid
    SparseLayerClear(&waterLayer);   // sparse: load writes only non-empty cells
    ClearWaterFlow();                 // the current is not saved: it rebuilds as water moves
    WaterWakeAll();                   // the scan's run bits know nothing of the loaded cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_SPARSE(f, waterLayer, WATER_MUT, y, z);
        }
    }
    
    // Fire grid
    SparseLayerClear(&fireLayer);   // sparse: load writes only non-empty cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_SPARSE(f, fireLayer, FIRE_MUT, y, z);
        }
    }
    SyncFireLighting();
    
    // Smoke grid
    SparseLayerClear(&smokeLayer);   // sparse: load writes only non-empty cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_SPARSE(f, smokeLayer, SMOKE_MUT, y, z);
        }
    }
    
    // Steam grid
    SparseLayerClear(&steamLayer);   // sparse: load writes only non-empty cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_SPARSE(f, steamLayer, STEAM_MUT, y, z);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, CELL_FLAGS, y, z);
        }
    }
    
    // Wall material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, WALL_MAT, y, z);
        }
    }
    
    // Floor material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, FLOOR_MAT, y, z);
        }
    }

    // Wall natural grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, WALL_NATURAL, y, z);
        }
    }

    // Floor natural grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, FLOOR_NATURAL, y, z);
        }
    }

    // Wall finish grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, WALL_FINISH, y, z);
        }
    }

    // Floor finish grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, FLOOR_FINISH, y, z);
        }
    }

    // Wall source item grid (V28)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_XOR(f, WALL_SOURCE_ITEM, y, z, SOURCE_ITEM_XOR);
        }
    }

    // Floor source item grid (V28)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_XOR(f, FLOOR_SOURCE_ITEM, y, z, SOURCE_ITEM_XOR);
        }
    }

    // Vegetation grid (V29)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, VEGETATION, y, z);
        }
    }
    
    // Snow grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, SNOW, y, z);
        }
    }
    SnowGridChanged();
    
    // Temperature grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, TEMPERATURE, y, z);
        }
    }
    
    // Designations (sparse: start empty, then write the saved non-empty cells)
    activeDesignationCount = 0;
    SparseLayerClear(&designationLayer);
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_SPARSE(f, designationLayer, DESIGNATION_MUT, y, z);
            // Count active designations for early-exit optimizations
            for (int x = 0; x < gridWidth; x++) {
                if (DESIGNATION(x, y, z).type != DESIGNATION_NONE) {
                    activeDesignationCount++;
                }
            }
        }
    }
    
    // Wear grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            if (version >= V94_NARROW_GRID_LAYERS) LOAD_ROW(f, WEAR, y, z);
            else { FreadLegacyIntsAsU16((uint16_t*)saveRowBuf, gridWidth, f); ROW_FROM_BUF(WEAR, y, z); }
        }
    }

    // Tree growth timer grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, GROWTH_TIMER, y, z);
        }
    }

    // Tree target height grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            if (version >= V94_NARROW_GRID_LAYERS) LOAD_ROW(f, TARGET_HEIGHT, y, z);
            else { FreadLegacyIntsAsU8((uint8_t*)saveRowBuf, gridWidth, f); ROW_FROM_BUF(TARGET_HEIGHT, y, z); }
        }
    }

    // Tree harvest state grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, TREE_HARVEST, y, z);
        }
    }

    // Floor dirt grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW(f, FLOOR_DIRT, y, z);
        }
    }

    // Explored grid (fog of war)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_XOR(f, UNEXPLORED, y, z, 1);
        }
    }

    // Farm grid
    SparseLayerClear(&farmLayer);   // sparse: load writes only non-empty cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            LOAD_ROW_SPARSE(f, farmLayer, FARM_MUT, y, z);
        }
    }
    fread(&farmActiveCells, sizeof(farmActiveCells), 1, f);

    // Track connections grid (v89+)
    if (version >= 89) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                LOAD_ROW(f, TRACK_CONN, y, z);
            }
        }
    } else {
        CLEAR_GRID_LAYER(trackConnections, 0);
    }

    // One-way grid (v110+)
    SparseLayerClear(&oneWayLayer);   // sparse: load writes only non-empty cells
    if (version >= V110_ONE_WAY) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                LOAD_ROW_SPARSE(f, oneWayLayer, ONE_WAY_MUT, y, z);
            }
        }
    }

    if (!loadRleOk) {
        printf("ERROR: corrupt grid data (a layer's run-length stream ended early or late)\n");
        AddMessage("Corrupt grid data in save", RED);
        fclose(f);
        return false;
    }

    // === ENTITIES SECTION ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_ENTITIES) {
        printf("ERROR: Bad ENTI marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_ENTITIES);
        AddMessage(TextFormat("Bad ENTI marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // Items
    fread(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    if (version >= 92) {
        if (version >= V108_CONTENTS_LISTS) fread(items, sizeof(Item), itemHighWaterMark, f);
        else for (int i = 0; i < itemHighWaterMark; i++) fread(&items[i], ITEM_V107_SIZE, 1, f);   // v92-v107: no contents lists
    } else {
        // v91 and below: Item struct without temperature field
        for (int i = 0; i < itemHighWaterMark; i++) {
            ItemV91 old;
            fread(&old, sizeof(ItemV91), 1, f);
            items[i].x = old.x; items[i].y = old.y; items[i].z = old.z;
            items[i].type = old.type; items[i].state = old.state;
            items[i].material = old.material; items[i].natural = old.natural;
            items[i].active = old.active; items[i].reservedBy = old.reservedBy;
            items[i].unreachableCooldown = old.unreachableCooldown;
            items[i].stackCount = old.stackCount; items[i].containedIn = old.containedIn;
            items[i].contentCount = old.contentCount; items[i].contentTypeMask = old.contentTypeMask;
            items[i].spoilageTimer = old.spoilageTimer; items[i].condition = old.condition;
            items[i].temperature = 0.0f;
        }
    }
    // Ensure default materials for any missing entries
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].material == MAT_NONE) {
            items[i].material = DefaultMaterialForItemType(items[i].type);
        }
    }
    
    // Stockpiles
    if (version >= V107_SCROUNGE) {
        fread(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    } else if (version >= V106_INVENTORY) {
        // v106: allowedTypes was V106_ITEM_TYPE_COUNT (81) bools
        static StockpileV106 oldSp[MAX_STOCKPILES];
        fread(oldSp, sizeof(StockpileV106), MAX_STOCKPILES, f);
        for (int i = 0; i < MAX_STOCKPILES; i++) MIGRATE_OLD_STOCKPILE_M(&stockpiles[i], &oldSp[i], V106_ITEM_TYPE_COUNT, V105_MAT_COUNT);
    } else if (version >= V105_CONCRETE) {
        // v105: allowedTypes was V105_ITEM_TYPE_COUNT (78) bools
        static StockpileV105 oldSp[MAX_STOCKPILES];
        fread(oldSp, sizeof(StockpileV105), MAX_STOCKPILES, f);
        for (int i = 0; i < MAX_STOCKPILES; i++) MIGRATE_OLD_STOCKPILE_M(&stockpiles[i], &oldSp[i], V105_ITEM_TYPE_COUNT, V105_MAT_COUNT);
    } else if (version >= V104_TRASH) {
        // v104: allowedMaterials was V104_MAT_COUNT (19) bools
        static StockpileV104 oldSp[MAX_STOCKPILES];
        fread(oldSp, sizeof(StockpileV104), MAX_STOCKPILES, f);
        for (int i = 0; i < MAX_STOCKPILES; i++) MIGRATE_OLD_STOCKPILE(&stockpiles[i], &oldSp[i], V104_ITEM_TYPE_COUNT);
    } else if (version >= 91) {
        // v91-v103: allowedTypes was V103_ITEM_TYPE_COUNT (73) bools
        static StockpileV103 oldSp[MAX_STOCKPILES];
        fread(oldSp, sizeof(StockpileV103), MAX_STOCKPILES, f);
        for (int i = 0; i < MAX_STOCKPILES; i++) MIGRATE_OLD_STOCKPILE(&stockpiles[i], &oldSp[i], V103_ITEM_TYPE_COUNT);
    } else {
        // v90 and below: allowedTypes was V90_ITEM_TYPE_COUNT (68) bools
        static StockpileV90 oldSp[MAX_STOCKPILES];
        fread(oldSp, sizeof(StockpileV90), MAX_STOCKPILES, f);
        for (int i = 0; i < MAX_STOCKPILES; i++) MIGRATE_OLD_STOCKPILE(&stockpiles[i], &oldSp[i], V90_ITEM_TYPE_COUNT);
    }

    // Clear transient reservation counts (not meaningful across save/load)
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        memset(stockpiles[i].reservedBy, 0, sizeof(stockpiles[i].reservedBy));
    }

    // Gather zones
    fread(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fread(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fread(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Workshops
    fread(workshops, sizeof(Workshop), MAX_WORKSHOPS, f);
    
    // Movers
    fread(&moverCount, sizeof(moverCount), 1, f);
    if (version >= V109_SET_ASIDE) {
        // v109+: Mover struct with setAside
        fread(movers, sizeof(Mover), moverCount, f);
        for (int i = 0; i < moverCount; i++) {
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
        }
    } else if (version >= V106_INVENTORY) {
        // v106-v108: Mover records without setAside (all movers, then all paths)
        for (int i = 0; i < moverCount; i++) fread(&movers[i], MOVER_V108_SIZE, 1, f);
        for (int i = 0; i < moverCount; i++) {
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
        }
    } else if (version >= V104_TRASH) {
        // v104-v105: Mover records without hands/wornBack (all movers, then all paths)
        for (int i = 0; i < moverCount; i++) {
            unsigned char rec[MOVER_V105_RECORD];
            fread(rec, MOVER_V105_RECORD, 1, f);
            memcpy(&movers[i], rec, MOVER_V105_DATA);
        }
        for (int i = 0; i < moverCount; i++) {
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
        }
    } else if (version >= 93) {
        // v93-v103: Mover with bladder, without heldTrash (all movers, then all paths)
        for (int i = 0; i < moverCount; i++) fread(&movers[i], MOVER_V103_SIZE, 1, f);
        for (int i = 0; i < moverCount; i++) {
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
        }
    } else if (version >= 90) {
        // v90-v92: Mover without bladder (4 bytes smaller at end)
        for (int i = 0; i < moverCount; i++) {
            fread(&movers[i], MOVER_V103_SIZE - sizeof(float), 1, f);
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
            movers[i].bladder = 1.0f;
        }
    } else if (version >= 86) {
        // v86-v89: Mover without mood fields
        for (int i = 0; i < moverCount; i++) {
            MoverV89 old;
            fread(&old, sizeof(MoverV89), 1, f);
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            m->pathLength = old.pathLength;
            m->pathIndex = old.pathIndex;
            m->active = old.active;
            m->needsRepath = old.needsRepath;
            m->repathCooldown = old.repathCooldown;
            m->speed = old.speed;
            m->timeNearWaypoint = old.timeNearWaypoint;
            m->lastX = old.lastX; m->lastY = old.lastY; m->lastZ = old.lastZ;
            m->timeWithoutProgress = old.timeWithoutProgress;
            m->fallTimer = old.fallTimer;
            m->workAnimPhase = old.workAnimPhase;
            m->hunger = old.hunger;
            m->energy = old.energy;
            m->freetimeState = old.freetimeState;
            m->needTarget = old.needTarget;
            m->needProgress = old.needProgress;
            m->needSearchCooldown = old.needSearchCooldown;
            m->starvationTimer = old.starvationTimer;
            m->thirst = old.thirst;
            m->dehydrationTimer = old.dehydrationTimer;
            m->bodyTemp = old.bodyTemp;
            m->hypothermiaTimer = old.hypothermiaTimer;
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
            m->equippedTool = old.equippedTool;
            m->equippedClothing = old.equippedClothing;
            memcpy(m->name, old.name, sizeof(old.name));
            m->gender = old.gender;
            m->age = old.age;
            m->appearanceSeed = old.appearanceSeed;
            m->isDrafted = old.isDrafted;
            // Copy transport fields
            m->transportState = old.transportState;
            m->transportStation = old.transportStation;
            m->transportExitStation = old.transportExitStation;
            m->transportTrainIdx = old.transportTrainIdx;
            m->transportFinalGoal = old.transportFinalGoal;
        }
    } else if (version >= 83) {
        // v83-v85: Mover without transport fields
        for (int i = 0; i < moverCount; i++) {
            MoverV85 old;
            fread(&old, sizeof(MoverV85), 1, f);
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            m->pathLength = old.pathLength;
            m->pathIndex = old.pathIndex;
            m->active = old.active;
            m->needsRepath = old.needsRepath;
            m->repathCooldown = old.repathCooldown;
            m->speed = old.speed;
            m->timeNearWaypoint = old.timeNearWaypoint;
            m->lastX = old.lastX; m->lastY = old.lastY; m->lastZ = old.lastZ;
            m->timeWithoutProgress = old.timeWithoutProgress;
            m->fallTimer = old.fallTimer;
            m->workAnimPhase = old.workAnimPhase;
            m->hunger = old.hunger;
            m->energy = old.energy;
            m->freetimeState = old.freetimeState;
            m->needTarget = old.needTarget;
            m->needProgress = old.needProgress;
            m->needSearchCooldown = old.needSearchCooldown;
            m->starvationTimer = old.starvationTimer;
            m->thirst = old.thirst;
            m->dehydrationTimer = old.dehydrationTimer;
            m->bodyTemp = old.bodyTemp;
            m->hypothermiaTimer = old.hypothermiaTimer;
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
            m->equippedTool = old.equippedTool;
            m->equippedClothing = old.equippedClothing;
            memcpy(m->name, old.name, sizeof(old.name));
            m->gender = old.gender;
            m->age = old.age;
            m->appearanceSeed = old.appearanceSeed;
            m->isDrafted = old.isDrafted;
            // Default transport fields
            m->transportState = TRANSPORT_NONE;
            m->transportStation = -1;
            m->transportExitStation = -1;
            m->transportTrainIdx = -1;
            m->transportFinalGoal = (Point){0, 0, 0};
        }
    } else if (version >= 79) {
        // v79-v82: Mover without identity fields, paths separate
        for (int i = 0; i < moverCount; i++) {
            MoverV82 old;
            fread(&old, sizeof(MoverV82), 1, f);
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            m->pathLength = old.pathLength;
            m->pathIndex = old.pathIndex;
            m->active = old.active;
            m->needsRepath = old.needsRepath;
            m->repathCooldown = old.repathCooldown;
            m->speed = old.speed;
            m->timeNearWaypoint = old.timeNearWaypoint;
            m->lastX = old.lastX; m->lastY = old.lastY; m->lastZ = old.lastZ;
            m->timeWithoutProgress = old.timeWithoutProgress;
            m->fallTimer = old.fallTimer;
            m->workAnimPhase = old.workAnimPhase;
            m->hunger = old.hunger;
            m->energy = old.energy;
            m->freetimeState = old.freetimeState;
            m->needTarget = old.needTarget;
            m->needProgress = old.needProgress;
            m->needSearchCooldown = old.needSearchCooldown;
            m->starvationTimer = old.starvationTimer;
            m->thirst = old.thirst;
            m->dehydrationTimer = old.dehydrationTimer;
            m->bodyTemp = old.bodyTemp;
            m->hypothermiaTimer = old.hypothermiaTimer;
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
            m->equippedTool = old.equippedTool;
            m->equippedClothing = old.equippedClothing;
            // Generate identity for migrated movers
            uint32_t seed = (uint32_t)(worldSeed ^ (uint64_t)i ^ 0xDEADBEEF);
            m->appearanceSeed = seed;
            m->gender = (seed & 1) ? GENDER_FEMALE : GENDER_MALE;
            m->age = 50 + (seed >> 1) % 21;
            m->isDrafted = false;
            GenerateMoverName(m->name, m->gender, seed);
        }
    }

    // Initialize canPlant for old saves (field added later)
    for (int i = 0; i < moverCount; i++) {
        movers[i].capabilities.canPlant = true;
    }


    // Initialize transport fields for old saves (v86+)
    if (version < 86) {
        for (int i = 0; i < moverCount; i++) {
            movers[i].transportState = TRANSPORT_NONE;
            movers[i].transportStation = -1;
            movers[i].transportExitStation = -1;
            movers[i].transportTrainIdx = -1;
            movers[i].transportFinalGoal = (Point){0, 0, 0};
        }
    }

    // Hands free in saves from before trash (v104)
    if (version < V104_TRASH) {
        for (int i = 0; i < moverCount; i++) movers[i].heldTrash = ITEM_NONE;
    }
    if (version < V106_INVENTORY) {
        for (int i = 0; i < moverCount; i++) { movers[i].hands[0] = movers[i].hands[1] = -1; movers[i].wornBack = -1; }
    }
    if (version < V109_SET_ASIDE) {
        for (int i = 0; i < moverCount; i++) movers[i].setAside[0] = movers[i].setAside[1] = -1;
    }

    // Initialize mood fields for old saves (v90+)
    if (version < 90) {
        for (int i = 0; i < moverCount; i++) {
            movers[i].mood = 0.0f;
            movers[i].moodletCount = 0;
            memset(movers[i].moodlets, 0, sizeof(movers[i].moodlets));
            movers[i].traits[0] = TRAIT_NONE;
            movers[i].traits[1] = TRAIT_NONE;
            movers[i].bladder = 1.0f;
        }
    }

    // Animals
    fread(&animalCount, sizeof(animalCount), 1, f);
    if (animalCount > 0) {
        fread(animals, sizeof(Animal), animalCount, f);
    }

    // Trains (v88+: Train has multi-car trail fields; v97+: freight)
    if (version >= 88) {
        fread(&trainCount, sizeof(trainCount), 1, f);
        if (trainCount > 0) {
            if (version >= V97_TRAIN_CARGO) {
                fread(trains, sizeof(Train), trainCount, f);
            } else {
                for (int ti = 0; ti < trainCount; ti++) {
                    TrainV96 old;
                    fread(&old, sizeof(TrainV96), 1, f);
                    MigrateTrainV96(&trains[ti], &old);
                }
            }
        }
        fread(&stationCount, sizeof(stationCount), 1, f);
        if (stationCount > 0) {
            fread(stations, sizeof(TrainStation), stationCount, f);
        }
    } else if (version >= 87) {
        fread(&trainCount, sizeof(trainCount), 1, f);
        if (trainCount > 0) {
            for (int ti = 0; ti < trainCount; ti++) {
                TrainV87 old;
                fread(&old, sizeof(TrainV87), 1, f);
                Train* t = &trains[ti];
                t->x = old.x; t->y = old.y;
                t->z = old.z;
                t->cellX = old.cellX; t->cellY = old.cellY;
                t->prevCellX = old.prevCellX; t->prevCellY = old.prevCellY;
                t->speed = old.speed;
                t->progress = old.progress;
                t->lightCellX = old.lightCellX; t->lightCellY = old.lightCellY;
                t->active = old.active;
                t->cartState = old.cartState;
                t->stateTimer = old.stateTimer;
                t->atStation = old.atStation;
                t->ridingCount = old.ridingCount;
                for (int r = 0; r < old.ridingCount; r++) {
                    t->ridingMovers[r] = old.ridingMovers[r];
                }
                t->carCount = 1;
                t->trailCount = 0;
            }
        }
        fread(&stationCount, sizeof(stationCount), 1, f);
        if (stationCount > 0) {
            fread(stations, sizeof(TrainStation), stationCount, f);
        }
    } else if (version >= 86) {
        fread(&trainCount, sizeof(trainCount), 1, f);
        if (trainCount > 0) {
            for (int ti = 0; ti < trainCount; ti++) {
                TrainV87 old;
                fread(&old, sizeof(TrainV87), 1, f);
                Train* t = &trains[ti];
                t->x = old.x; t->y = old.y;
                t->z = old.z;
                t->cellX = old.cellX; t->cellY = old.cellY;
                t->prevCellX = old.prevCellX; t->prevCellY = old.prevCellY;
                t->speed = old.speed;
                t->progress = old.progress;
                t->lightCellX = old.lightCellX; t->lightCellY = old.lightCellY;
                t->active = old.active;
                t->cartState = old.cartState;
                t->stateTimer = old.stateTimer;
                t->atStation = old.atStation;
                t->ridingCount = old.ridingCount;
                for (int r = 0; r < old.ridingCount; r++) {
                    t->ridingMovers[r] = old.ridingMovers[r];
                }
                t->carCount = 1;
                t->trailCount = 0;
            }
        }
        // v86 stations: old struct without multi-cell platform fields
        fread(&stationCount, sizeof(stationCount), 1, f);
        if (stationCount > 0) {
            for (int si = 0; si < stationCount; si++) {
                TrainStationV86 old;
                fread(&old, sizeof(TrainStationV86), 1, f);
                TrainStation* s = &stations[si];
                s->trackX = old.trackX; s->trackY = old.trackY; s->z = old.z;
                s->platX = old.platX; s->platY = old.platY;
                s->active = old.active;
                s->waitingCount = old.waitingCount;
                for (int j = 0; j < old.waitingCount; j++) {
                    s->waitingMovers[j] = old.waitingMovers[j];
                    s->waitingSince[j] = old.waitingSince[j];
                }
                // Zero new fields — RebuildStations will fill them
                s->platformCellCount = 0;
                s->queueDirX = 0; s->queueDirY = 0;
                memset(s->platformCells, 0, sizeof(s->platformCells));
            }
        }
        // Rebuild to fill multi-cell platform data
        RebuildStations();
    }

    // Jobs
    fread(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fread(&activeJobCount, sizeof(activeJobCount), 1, f);
    fread(jobs, sizeof(Job), jobHighWaterMark, f);
    fread(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fread(activeJobList, sizeof(int), activeJobCount, f);
    
    // Light sources
    fread(&lightSourceCount, sizeof(lightSourceCount), 1, f);
    if (lightSourceCount > 0) {
        fread(lightSources, sizeof(LightSource), lightSourceCount, f);
    }
    InvalidateLighting();
    
    // Plants
    fread(&plantCount, sizeof(plantCount), 1, f);
    if (plantCount > 0) {
        fread(plants, sizeof(Plant), plantCount, f);
    }
    
    // Furniture
    {
        int savedCount;
        fread(&savedCount, sizeof(savedCount), 1, f);
        ClearFurniture();
        for (int i = 0; i < savedCount; i++) {
            Furniture f_tmp;
            fread(&f_tmp, sizeof(Furniture), 1, f);
            if (f_tmp.active) {
                furniture[i] = f_tmp;
            }
        }
        furnitureCount = savedCount;
    }

    // Conveyor belts (v98+). Older saves have none; BeltsAfterLoad also puts any item left
    // ITEM_ON_BELT by a pre-v98 save back on the ground.
    memset(belts, 0, sizeof(belts));
    beltCount = 0;
    if (version >= V98_BELTS) {
        fread(&beltCount, sizeof(beltCount), 1, f);
        if (beltCount < 0 || beltCount > MAX_BELTS) {
            printf("ERROR: bad belt count %d\n", beltCount);
            fclose(f);
            return false;
        }
        if (beltCount > 0) fread(belts, sizeof(Belt), beltCount, f);
    }
    BeltsAfterLoad();

    // Relations (v111+). Older saves: nobody knows anybody.
    ClearRelations();
    if (version >= V111_RELATIONS && moverCount > 0) fread(moverRelations, sizeof(MoverRelations), moverCount, f);
    
    // === VIEW SECTION ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_VIEW) {
        printf("ERROR: Bad VIEW marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_VIEW);
        AddMessage(TextFormat("Bad VIEW marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // View state
    fread(&currentViewZ, sizeof(currentViewZ), 1, f);
    fread(&zoom, sizeof(zoom), 1, f);
    fread(&offset, sizeof(offset), 1, f);
    
    // === SETTINGS SECTION ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_SETTINGS) {
        printf("ERROR: Bad SETT marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_SETTINGS);
        AddMessage(TextFormat("Bad SETT marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // Simulation settings (generated from SETTINGS_TABLE macro)
    {
        #define READ_SETTING(type, name) fread(&name, sizeof(type), 1, f);
        SETTINGS_TABLE(READ_SETTING)
        BALANCE_SETTINGS_TABLE(READ_SETTING)
        #undef READ_SETTING
    }


    // v87 and earlier: train queue toggle not saved, default to enabled
    if (version < 88) {
        trainQueueEnabled = true;
    }

    // Diurnal amplitude
    fread(&diurnalAmplitude, sizeof(int), 1, f);
    if (version >= V99_TRAIN_SETTINGS) {
        fread(&trainSpeed, sizeof(float), 1, f);
        fread(&useTrainTimeChoice, sizeof(bool), 1, f);
        fread(&useTrainTransfers, sizeof(bool), 1, f);
        fread(&trainFreight, sizeof(bool), 1, f);
        fread(&endlessMoverMode, sizeof(bool), 1, f);
    } else {
        trainSpeed = TRAIN_DEFAULT_SPEED;
        useTrainTimeChoice = useTrainTransfers = trainFreight = true;
        endlessMoverMode = true;
    }
    if (version >= V100_ELEVATOR_SETTINGS) {
        fread(&elevatorSpeed, sizeof(float), 1, f);
        fread(&elevatorDoorTime, sizeof(float), 1, f);
        fread(&elevatorCapacity, sizeof(int), 1, f);
        fread(&useElevatorWaitCost, sizeof(bool), 1, f);
    } else {
        elevatorSpeed = 3.0f;
        elevatorDoorTime = 0.75f;
        elevatorCapacity = 4;
        useElevatorWaitCost = true;
    }
    if (version >= V101_WATER_EDGE) {
        fread(&waterEdgeOutflow, sizeof(bool), 1, f);
        fread(&waterEdgeSeaZ, sizeof(int), 1, f);
    } else {
        // Older islands kept their sea up with sources along the frame: that height is the
        // sea beyond the edge now (and those sources go: the edge refills itself)
        waterEdgeOutflow = true;
        waterEdgeSeaZ = -1;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) {
                    if (x != 0 && y != 0 && x != gridWidth - 1 && y != gridHeight - 1) continue;
                    if (!WATER(x, y, z).isSource) continue;
                    if (z > waterEdgeSeaZ) waterEdgeSeaZ = z;
                    SetWaterSource(x, y, z, false);
                }
    }
    if (version >= V102_TIDE) {
        fread(&tideEnabled, sizeof(bool), 1, f);
        fread(&tidePeriodHours, sizeof(float), 1, f);
        fread(&tideLowLevel, sizeof(int), 1, f);
    } else {
        tideEnabled = false;
        tidePeriodHours = 12.42f;
        tideLowLevel = 0;
    }
    if (version >= V103_SPRINGS) {
        fread(&springTrickle, sizeof(bool), 1, f);
        fread(&springRate, sizeof(float), 1, f);
    } else {
        springTrickle = false;   // saved with taps
    }
    TideAfterLoad();   // the sea list from the loaded water cells' isSea bits

    RecalcBalanceTable();

    // Simulation accumulators (static locals, loaded via setters)
    float accum;
    fread(&accum, sizeof(float), 1, f); SetFireSpreadAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetFireFuelAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetWaterEvapAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetSmokeRiseAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetSmokeDissipationAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetSteamRiseAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetHeatTransferAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetTempDecayAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetWearRecoveryAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetRainWetnessAccum(accum);
    fread(&accum, sizeof(float), 1, f); SetWeatherWindAccum(accum);

    // Weather state
    fread(&weatherState, sizeof(WeatherState), 1, f);
    
    // === END MARKER ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_END) {
        printf("ERROR: Bad END marker: 0x%08X (expected 0x%08X) - file may be truncated or corrupted\n", marker, MARKER_END);
        AddMessage(TextFormat("Bad END marker: 0x%08X (file may be truncated or corrupted)", marker), RED);
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    RebuildPostLoadState();

    // Rebuild stations from grid (for old saves and to verify loaded stations)
    if (version < 86) {
        RebuildStations();
    }
    TrainsAfterLoad();   // lines + transit graph are derived, not saved
    ElevatorsAfterLoad();   // cars come from the shaft cells; nobody aboard

    // v96: temperatures became offsets from the level's ambient (save_migrations.h). Weather
    // and time are loaded by now, so GetAmbientTemperature is the loaded moment's.
    if (version < V96_TEMP_OFFSETS) {
        for (int z = 0; z < gridDepth; z++) {
            int ambient = GetAmbientTemperature(z);
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) {
                    TempCell* tc = &TEMPERATURE(x, y, z);
                    bool source = tc->isHeatSource || tc->isColdSource;
                    tc->offset = (tc->stable && !source) ? 0 : (int16_t)(tc->offset - ambient);
                }
        }
    }

    // Rebuild active cell counters from loaded simulation grids
    RebuildSimActivityCounts();
    
    // Rebuild HPA* graph after loading - mark all chunks dirty
    hpaNeedsRebuild = true;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y += chunkHeight) {
            for (int x = 0; x < gridWidth; x += chunkWidth) {
                MarkChunkDirty(x, y, z);
            }
        }
    }
    
    // Rebuild spatial grids
    BuildMoverSpatialGrid();
    BuildItemSpatialGrid();
    
    // Validate and cleanup any invalid ramps (e.g., from older saves)
    int removedRamps = ValidateAllRamps();
    if (removedRamps > 0) {
        AddMessage(TextFormat("Cleaned up %d invalid ramps", removedRamps), YELLOW);
    }
    
    // Rebuild pathfinding graph (entrances + edges)
    BuildEntrances();
    BuildGraph();
    
    return true;
}
