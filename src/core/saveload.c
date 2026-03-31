// core/saveload.c - Save and Load functions
#include "../game_state.h"

// Forward declarations
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);
void RebuildPostLoadState(void);
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../simulation/fire.h"
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

#define V21_MAT_COUNT 10  // MAT_COUNT before clay/gravel/sand/peat materials
#define SAVE_MAGIC 0x4E41564B  // "NAVK"

// Section markers (readable in hex dump)
#define MARKER_GRIDS    0x47524944  // "GRID"
#define MARKER_ENTITIES 0x454E5449  // "ENTI"
#define MARKER_VIEW     0x56494557  // "VIEW"
#define MARKER_SETTINGS 0x53455454  // "SETT"
#define MARKER_END      0x454E4421  // "END!"

// Settings save/load macro table
// X-macro for all tweakable simulation settings
// Adding a new setting: add one line here, it will be saved/loaded automatically
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
    X(float, animalSpawnInterval) \
    /* Trains */ \
    X(bool, trainQueueEnabled)

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
    X(float, balance.nightEnergyMult) \
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
        fwrite(&selectedBiome, sizeof(int), 1, f);
    }

    // === GRIDS SECTION ===
    uint32_t marker = MARKER_GRIDS;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(grid[z][y], sizeof(CellType), gridWidth, f);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(waterGrid[z][y], sizeof(WaterCell), gridWidth, f);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(fireGrid[z][y], sizeof(FireCell), gridWidth, f);
        }
    }
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(smokeGrid[z][y], sizeof(SmokeCell), gridWidth, f);
        }
    }
    
    // Steam grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(steamGrid[z][y], sizeof(SteamCell), gridWidth, f);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Wall material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wallMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Floor material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall natural grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wallNatural[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor natural grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorNatural[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall finish grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wallFinish[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor finish grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorFinish[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall source item grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wallSourceItem[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor source item grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorSourceItem[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Vegetation grid (V29)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(vegetationGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Snow grid (V45)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(snowGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Temperature grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(temperatureGrid[z][y], sizeof(TempCell), gridWidth, f);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(designations[z][y], sizeof(Designation), gridWidth, f);
        }
    }
    
    // Wear grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wearGrid[z][y], sizeof(int), gridWidth, f);
        }
    }

    // Tree growth timer grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(growthTimer[z][y], sizeof(float), gridWidth, f);
        }
    }

    // Tree target height grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(targetHeight[z][y], sizeof(int), gridWidth, f);
        }
    }

    // Tree harvest state grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(treeHarvestState[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor dirt grid (v36+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorDirtGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Explored grid (fog of war, v75+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(exploredGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Farm grid (v76+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(farmGrid[z][y], sizeof(FarmCell), gridWidth, f);
        }
    }
    fwrite(&farmActiveCells, sizeof(farmActiveCells), 1, f);

    // Track connections grid (v89+)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(trackConnections[z][y], sizeof(uint8_t), gridWidth, f);
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

    // Rebuild waterActiveCells from loaded water grid
    waterActiveCells = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                WaterCell* c = &waterGrid[z][y][x];
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
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(grid[z][y], sizeof(CellType), gridWidth, f);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(waterGrid[z][y], sizeof(WaterCell), gridWidth, f);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(fireGrid[z][y], sizeof(FireCell), gridWidth, f);
        }
    }
    SyncFireLighting();
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(smokeGrid[z][y], sizeof(SmokeCell), gridWidth, f);
        }
    }
    
    // Steam grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(steamGrid[z][y], sizeof(SteamCell), gridWidth, f);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Wall material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(wallMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Floor material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(floorMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall natural grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(wallNatural[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor natural grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(floorNatural[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall finish grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(wallFinish[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor finish grid (V22 only)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(floorFinish[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall source item grid (V28)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(wallSourceItem[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor source item grid (V28)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(floorSourceItem[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Vegetation grid (V29)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(vegetationGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Snow grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(snowGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Temperature grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(temperatureGrid[z][y], sizeof(TempCell), gridWidth, f);
        }
    }
    
    // Designations
    activeDesignationCount = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(designations[z][y], sizeof(Designation), gridWidth, f);
            // Count active designations for early-exit optimizations
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type != DESIGNATION_NONE) {
                    activeDesignationCount++;
                }
            }
        }
    }
    
    // Wear grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(wearGrid[z][y], sizeof(int), gridWidth, f);
        }
    }

    // Tree growth timer grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(growthTimer[z][y], sizeof(float), gridWidth, f);
        }
    }

    // Tree target height grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(targetHeight[z][y], sizeof(int), gridWidth, f);
        }
    }

    // Tree harvest state grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(treeHarvestState[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor dirt grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(floorDirtGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Explored grid (fog of war)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(exploredGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Farm grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(farmGrid[z][y], sizeof(FarmCell), gridWidth, f);
        }
    }
    fread(&farmActiveCells, sizeof(farmActiveCells), 1, f);

    // Track connections grid (v89+)
    if (version >= 89) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(trackConnections[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }
    } else {
        memset(trackConnections, 0, sizeof(trackConnections));
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
        fread(items, sizeof(Item), itemHighWaterMark, f);
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
    if (version >= 91) {
        fread(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    } else {
        // v90 and below: allowedTypes was V90_ITEM_TYPE_COUNT (68) bools
        StockpileV90 oldSp[MAX_STOCKPILES];
        fread(oldSp, sizeof(StockpileV90), MAX_STOCKPILES, f);
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            Stockpile* sp = &stockpiles[i];
            sp->x = oldSp[i].x; sp->y = oldSp[i].y; sp->z = oldSp[i].z;
            sp->width = oldSp[i].width; sp->height = oldSp[i].height;
            sp->active = oldSp[i].active;
            memset(sp->allowedTypes, 0, sizeof(sp->allowedTypes));
            memcpy(sp->allowedTypes, oldSp[i].allowedTypes, V90_ITEM_TYPE_COUNT * sizeof(bool));
            memcpy(sp->allowedMaterials, oldSp[i].allowedMaterials, sizeof(sp->allowedMaterials));
            memcpy(sp->cells, oldSp[i].cells, sizeof(sp->cells));
            memcpy(sp->slots, oldSp[i].slots, sizeof(sp->slots));
            memcpy(sp->reservedBy, oldSp[i].reservedBy, sizeof(sp->reservedBy));
            memcpy(sp->slotCounts, oldSp[i].slotCounts, sizeof(sp->slotCounts));
            memcpy(sp->slotTypes, oldSp[i].slotTypes, sizeof(sp->slotTypes));
            memcpy(sp->slotMaterials, oldSp[i].slotMaterials, sizeof(sp->slotMaterials));
            sp->maxStackSize = oldSp[i].maxStackSize;
            sp->priority = oldSp[i].priority;
            sp->maxContainers = oldSp[i].maxContainers;
            memcpy(sp->slotIsContainer, oldSp[i].slotIsContainer, sizeof(sp->slotIsContainer));
            memcpy(sp->groundItemIdx, oldSp[i].groundItemIdx, sizeof(sp->groundItemIdx));
            sp->freeSlotCount = oldSp[i].freeSlotCount;
            sp->rejectsRotten = oldSp[i].rejectsRotten;
        }
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
    if (version >= 93) {
        // v93+: Mover struct with bladder field
        fread(movers, sizeof(Mover), moverCount, f);
        for (int i = 0; i < moverCount; i++) {
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
        }
    } else if (version >= 90) {
        // v90-v92: Mover without bladder (4 bytes smaller at end)
        for (int i = 0; i < moverCount; i++) {
            fread(&movers[i], sizeof(Mover) - sizeof(float), 1, f);
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

    // Trains (v88+: Train has multi-car trail fields)
    if (version >= 88) {
        fread(&trainCount, sizeof(trainCount), 1, f);
        if (trainCount > 0) {
            fread(trains, sizeof(Train), trainCount, f);
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
