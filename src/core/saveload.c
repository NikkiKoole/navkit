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
#include "../entities/tool_quality.h"
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
    X(float, animalSpawnInterval)

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

    // Trains (v46+)
    fwrite(&trainCount, sizeof(trainCount), 1, f);
    fwrite(trains, sizeof(Train), trainCount, f);

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
    
    // Support v48+ (with migration to current)
    if (version < 48 || version > CURRENT_SAVE_VERSION) {
        printf("ERROR: Save version mismatch (file: v%d, supported: v48-v%d)\n", version, CURRENT_SAVE_VERSION);
        AddMessage(TextFormat("Save version mismatch: v%d (expected v48-v%d).", version, CURRENT_SAVE_VERSION), RED);
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
    
    // Game mode and needs toggles (v62+)
    if (version >= 62) {
        uint8_t gm;
        fread(&gm, sizeof(gm), 1, f);
        gameMode = (GameMode)gm;
        fread(&hungerEnabled, sizeof(bool), 1, f);
        fread(&energyEnabled, sizeof(bool), 1, f);
        fread(&bodyTempEnabled, sizeof(bool), 1, f);
        if (version >= 65) {
            fread(&toolRequirementsEnabled, sizeof(bool), 1, f);
        } else {
            toolRequirementsEnabled = false;
        }
        if (version >= 79) {
            fread(&thirstEnabled, sizeof(bool), 1, f);
        } else {
            thirstEnabled = false;
        }
    } else {
        gameMode = GAME_MODE_SANDBOX;
        hungerEnabled = false;
        energyEnabled = false;
        bodyTempEnabled = false;
        toolRequirementsEnabled = false;
        thirstEnabled = false;
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
    
    // Snow grid (V45)
    if (version >= 45) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(snowGrid[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }
    } else {
        // Initialize to zero for old saves
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                memset(snowGrid[z][y], 0, gridWidth);
            }
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
    if (version >= 56) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(growthTimer[z][y], sizeof(float), gridWidth, f);
            }
        }
    } else {
        // v55 and earlier: int grid, discard and zero out
        int oldRow[MAX_GRID_WIDTH];
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(oldRow, sizeof(int), gridWidth, f);
                memset(growthTimer[z][y], 0, sizeof(float) * gridWidth);
            }
        }
    }

    // Tree target height grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(targetHeight[z][y], sizeof(int), gridWidth, f);
        }
    }

    // Tree harvest state grid (v34+)
    if (version >= 34) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(treeHarvestState[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }
    } else {
        // Old save: init all mature trees to full harvest
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    treeHarvestState[z][y][x] = 0;
                    if (grid[z][y][x] == CELL_TREE_TRUNK) {
                        // Only set on trunk base cells
                        if (z == 0 || grid[z - 1][y][x] != CELL_TREE_TRUNK) {
                            treeHarvestState[z][y][x] = TREE_HARVEST_MAX;
                        }
                    }
                }
            }
        }
    }

    // Floor dirt grid (v36+)
    if (version >= 36) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(floorDirtGrid[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }
    } else {
        memset(floorDirtGrid, 0, sizeof(floorDirtGrid));
    }

    // Explored grid (fog of war, v75+)
    if (version >= 75) {
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(exploredGrid[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }
    } else {
        memset(exploredGrid, 1, sizeof(exploredGrid));  // Old saves: all explored
    }

    // Farm grid (v76+)
    if (version >= 77) {
        // Current format (8-byte FarmCell with crop fields)
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(farmGrid[z][y], sizeof(FarmCell), gridWidth, f);
            }
        }
        fread(&farmActiveCells, sizeof(farmActiveCells), 1, f);
    } else if (version >= 76) {
        // V76 format (4-byte FarmCell without crop fields)
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    FarmCellV76 old;
                    fread(&old, sizeof(FarmCellV76), 1, f);
                    farmGrid[z][y][x].fertility = old.fertility;
                    farmGrid[z][y][x].weedLevel = old.weedLevel;
                    farmGrid[z][y][x].tilled = old.tilled;
                    farmGrid[z][y][x].desiredCropType = old.desiredCropType;
                    farmGrid[z][y][x].cropType = 0;
                    farmGrid[z][y][x].growthStage = 0;
                    farmGrid[z][y][x].growthProgress = 0;
                    farmGrid[z][y][x].frostDamaged = 0;
                }
            }
        }
        fread(&farmActiveCells, sizeof(farmActiveCells), 1, f);
    } else {
        memset(farmGrid, 0, sizeof(farmGrid));
        farmActiveCells = 0;
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
    if (version >= 73) {
        fread(items, sizeof(Item), itemHighWaterMark, f);
    } else if (version >= 71) {
        // V71-V72 items: have spoilageTimer but no condition field
        for (int i = 0; i < itemHighWaterMark; i++) {
            ItemV72 old;
            fread(&old, sizeof(ItemV72), 1, f);
            items[i].x = old.x; items[i].y = old.y; items[i].z = old.z;
            items[i].type = old.type;
            items[i].state = old.state;
            items[i].material = old.material;
            items[i].natural = old.natural;
            items[i].active = old.active;
            items[i].reservedBy = old.reservedBy;
            items[i].unreachableCooldown = old.unreachableCooldown;
            items[i].stackCount = old.stackCount;
            items[i].containedIn = old.containedIn;
            items[i].contentCount = old.contentCount;
            items[i].contentTypeMask = old.contentTypeMask;
            items[i].spoilageTimer = old.spoilageTimer;
            // Derive condition from timer
            if (old.type >= 0 && old.type < V72_ITEM_TYPE_COUNT && old.active) {
                // Delete ITEM_ROT items (old enum index 45)
                if (old.type == V72_ITEM_TYPE_COUNT - 1) {
                    items[i].active = false;
                    items[i].condition = CONDITION_FRESH;
                    continue;
                }
                float limit = ItemSpoilageLimit(old.type);
                if (limit > 0.0f && old.spoilageTimer > 0.0f) {
                    float ratio = old.spoilageTimer / limit;
                    if (ratio >= 1.0f) items[i].condition = CONDITION_ROTTEN;
                    else if (ratio >= 0.5f) items[i].condition = CONDITION_STALE;
                    else items[i].condition = CONDITION_FRESH;
                } else {
                    items[i].condition = CONDITION_FRESH;
                }
            } else {
                items[i].condition = CONDITION_FRESH;
            }
        }
    } else if (version >= 50) {
        // V70 items don't have spoilageTimer
        for (int i = 0; i < itemHighWaterMark; i++) {
            ItemV70 old;
            fread(&old, sizeof(ItemV70), 1, f);
            items[i].x = old.x; items[i].y = old.y; items[i].z = old.z;
            items[i].type = old.type;
            items[i].state = old.state;
            items[i].material = old.material;
            items[i].natural = old.natural;
            items[i].active = old.active;
            items[i].reservedBy = old.reservedBy;
            items[i].unreachableCooldown = old.unreachableCooldown;
            items[i].stackCount = old.stackCount;
            items[i].containedIn = old.containedIn;
            items[i].contentCount = old.contentCount;
            items[i].contentTypeMask = old.contentTypeMask;
            items[i].spoilageTimer = 0.0f;
            items[i].condition = CONDITION_FRESH;
        }
    } else if (version == 49) {
        // V49 items don't have containedIn/contentCount/contentTypeMask
        for (int i = 0; i < itemHighWaterMark; i++) {
            ItemV49 old;
            fread(&old, sizeof(ItemV49), 1, f);
            items[i].x = old.x; items[i].y = old.y; items[i].z = old.z;
            items[i].type = old.type;
            items[i].state = old.state;
            items[i].material = old.material;
            items[i].natural = old.natural;
            items[i].active = old.active;
            items[i].reservedBy = old.reservedBy;
            items[i].unreachableCooldown = old.unreachableCooldown;
            items[i].stackCount = old.stackCount;
            items[i].containedIn = -1;
            items[i].contentCount = 0;
            items[i].contentTypeMask = 0;
        }
    } else {
        // V48 items don't have stackCount — read with old struct, copy, set stackCount=1
        for (int i = 0; i < itemHighWaterMark; i++) {
            ItemV48 old;
            fread(&old, sizeof(ItemV48), 1, f);
            items[i].x = old.x; items[i].y = old.y; items[i].z = old.z;
            items[i].type = old.type;
            items[i].state = old.state;
            items[i].material = old.material;
            items[i].natural = old.natural;
            items[i].active = old.active;
            items[i].reservedBy = old.reservedBy;
            items[i].unreachableCooldown = old.unreachableCooldown;
            items[i].stackCount = old.active ? 1 : 0;
            items[i].containedIn = -1;
            items[i].contentCount = 0;
            items[i].contentTypeMask = 0;
        }
    }
    // Ensure default materials for any missing entries
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].material == MAT_NONE) {
            items[i].material = DefaultMaterialForItemType(items[i].type);
        }
    }
    
    // Stockpiles - migrate v31 → v32 if needed
    if (version == 31) {
        // V31 had 8 separate sapling/leaf types, v32 consolidates to 2
        StockpileV31 v31_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v31_sp, sizeof(StockpileV31), 1, f);

            // Copy unchanged fields
            stockpiles[i].x = v31_sp.x;
            stockpiles[i].y = v31_sp.y;
            stockpiles[i].z = v31_sp.z;
            stockpiles[i].width = v31_sp.width;
            stockpiles[i].height = v31_sp.height;
            stockpiles[i].active = v31_sp.active;
            stockpiles[i].maxStackSize = v31_sp.maxStackSize;

            // Migrate allowedTypes: if ANY old sapling/leaf type enabled, enable unified type
            // v31 indices 16-19: oak/pine/birch/willow saplings
            // v31 indices 20-23: oak/pine/birch/willow leaves
            bool anySapling = v31_sp.allowedTypes[16] || v31_sp.allowedTypes[17] ||
                              v31_sp.allowedTypes[18] || v31_sp.allowedTypes[19];
            bool anyLeaves = v31_sp.allowedTypes[20] || v31_sp.allowedTypes[21] ||
                             v31_sp.allowedTypes[22] || v31_sp.allowedTypes[23];

            // Copy items 0-15 directly (ITEM_RED through ITEM_LOG)
            for (int j = 0; j < 16; j++) {
                stockpiles[i].allowedTypes[j] = v31_sp.allowedTypes[j];
            }
            // Set unified types at indices 16-17 (ITEM_SAPLING, ITEM_LEAVES)
            stockpiles[i].allowedTypes[16] = anySapling;  // ITEM_SAPLING
            stockpiles[i].allowedTypes[17] = anyLeaves;   // ITEM_LEAVES
            // Copy items 24-27 to positions 18-21 (shift up 6)
            for (int j = 24; j < V31_ITEM_TYPE_COUNT; j++) {
                stockpiles[i].allowedTypes[j - 6] = v31_sp.allowedTypes[j];
            }

            // Copy materials array unchanged
            memcpy(stockpiles[i].allowedMaterials, v31_sp.allowedMaterials,
                   sizeof(v31_sp.allowedMaterials));
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version == 32) {
        // V32 had 22 item types, v33 adds ITEM_BARK and ITEM_STRIPPED_LOG at end
        StockpileV32 v32_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v32_sp, sizeof(StockpileV32), 1, f);
            stockpiles[i].x = v32_sp.x;
            stockpiles[i].y = v32_sp.y;
            stockpiles[i].z = v32_sp.z;
            stockpiles[i].width = v32_sp.width;
            stockpiles[i].height = v32_sp.height;
            stockpiles[i].active = v32_sp.active;
            stockpiles[i].maxStackSize = v32_sp.maxStackSize;
            for (int j = 0; j < V32_ITEM_TYPE_COUNT; j++) {
                stockpiles[i].allowedTypes[j] = v32_sp.allowedTypes[j];
            }
            stockpiles[i].allowedTypes[ITEM_BARK] = false;
            stockpiles[i].allowedTypes[ITEM_STRIPPED_LOG] = false;
            memcpy(stockpiles[i].allowedMaterials, v32_sp.allowedMaterials,
                   sizeof(v32_sp.allowedMaterials));
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version == 33 || version == 34) {
        // V33/V34 had 24 item types, v35 adds SHORT_STRING and CORDAGE
        StockpileV34 v34_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v34_sp, sizeof(StockpileV34), 1, f);
            stockpiles[i].x = v34_sp.x;
            stockpiles[i].y = v34_sp.y;
            stockpiles[i].z = v34_sp.z;
            stockpiles[i].width = v34_sp.width;
            stockpiles[i].height = v34_sp.height;
            stockpiles[i].active = v34_sp.active;
            stockpiles[i].maxStackSize = v34_sp.maxStackSize;
            for (int j = 0; j < V34_ITEM_TYPE_COUNT; j++) {
                stockpiles[i].allowedTypes[j] = v34_sp.allowedTypes[j];
            }
            stockpiles[i].allowedTypes[ITEM_SHORT_STRING] = false;
            stockpiles[i].allowedTypes[ITEM_CORDAGE] = false;
            memcpy(stockpiles[i].allowedMaterials, v34_sp.allowedMaterials,
                   sizeof(v34_sp.allowedMaterials));
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 48) {
        // v35-v47: 26 item types, migrate to 28
        StockpileV47 v47_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v47_sp, sizeof(StockpileV47), 1, f);
            stockpiles[i].x = v47_sp.x;
            stockpiles[i].y = v47_sp.y;
            stockpiles[i].z = v47_sp.z;
            stockpiles[i].width = v47_sp.width;
            stockpiles[i].height = v47_sp.height;
            stockpiles[i].active = v47_sp.active;
            memcpy(stockpiles[i].allowedTypes, v47_sp.allowedTypes,
                   sizeof(v47_sp.allowedTypes));
            stockpiles[i].allowedTypes[ITEM_BERRIES] = false;
            stockpiles[i].allowedTypes[ITEM_DRIED_BERRIES] = false;
            memcpy(stockpiles[i].allowedMaterials, v47_sp.allowedMaterials,
                   sizeof(v47_sp.allowedMaterials));
            stockpiles[i].maxStackSize = v47_sp.maxStackSize;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 51) {
        // v48-v50: 28 item types, migrate to 31
        StockpileV50 v50_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v50_sp, sizeof(StockpileV50), 1, f);
            stockpiles[i].x = v50_sp.x;
            stockpiles[i].y = v50_sp.y;
            stockpiles[i].z = v50_sp.z;
            stockpiles[i].width = v50_sp.width;
            stockpiles[i].height = v50_sp.height;
            stockpiles[i].active = v50_sp.active;
            memcpy(stockpiles[i].allowedTypes, v50_sp.allowedTypes,
                   sizeof(v50_sp.allowedTypes));
            stockpiles[i].allowedTypes[ITEM_BASKET] = false;
            stockpiles[i].allowedTypes[ITEM_CLAY_POT] = false;
            stockpiles[i].allowedTypes[ITEM_CHEST] = false;
            memcpy(stockpiles[i].allowedMaterials, v50_sp.allowedMaterials,
                   sizeof(v50_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v50_sp.cells, sizeof(v50_sp.cells));
            memcpy(stockpiles[i].slots, v50_sp.slots, sizeof(v50_sp.slots));
            memcpy(stockpiles[i].reservedBy, v50_sp.reservedBy, sizeof(v50_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v50_sp.slotCounts, sizeof(v50_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v50_sp.slotTypes, sizeof(v50_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v50_sp.slotMaterials, sizeof(v50_sp.slotMaterials));
            stockpiles[i].maxStackSize = v50_sp.maxStackSize;
            stockpiles[i].priority = v50_sp.priority;
            stockpiles[i].maxContainers = 0;
            memset(stockpiles[i].slotIsContainer, 0, sizeof(stockpiles[i].slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v50_sp.groundItemIdx, sizeof(v50_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v50_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 52) {
        // v51: no maxContainers field
        StockpileV51 v51_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v51_sp, sizeof(StockpileV51), 1, f);
            stockpiles[i].x = v51_sp.x;
            stockpiles[i].y = v51_sp.y;
            stockpiles[i].z = v51_sp.z;
            stockpiles[i].width = v51_sp.width;
            stockpiles[i].height = v51_sp.height;
            stockpiles[i].active = v51_sp.active;
            memcpy(stockpiles[i].allowedTypes, v51_sp.allowedTypes,
                   sizeof(v51_sp.allowedTypes));
            memcpy(stockpiles[i].allowedMaterials, v51_sp.allowedMaterials,
                   sizeof(v51_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v51_sp.cells, sizeof(v51_sp.cells));
            memcpy(stockpiles[i].slots, v51_sp.slots, sizeof(v51_sp.slots));
            memcpy(stockpiles[i].reservedBy, v51_sp.reservedBy, sizeof(v51_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v51_sp.slotCounts, sizeof(v51_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v51_sp.slotTypes, sizeof(v51_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v51_sp.slotMaterials, sizeof(v51_sp.slotMaterials));
            stockpiles[i].maxStackSize = v51_sp.maxStackSize;
            stockpiles[i].priority = v51_sp.priority;
            stockpiles[i].maxContainers = 0;  // NEW: default to no containers
            memset(stockpiles[i].slotIsContainer, 0, sizeof(stockpiles[i].slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v51_sp.groundItemIdx, sizeof(v51_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v51_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 55) {
        // v52-v54: 31 item types, migrate to 33
        StockpileV54 v54_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v54_sp, sizeof(StockpileV54), 1, f);
            stockpiles[i].x = v54_sp.x;
            stockpiles[i].y = v54_sp.y;
            stockpiles[i].z = v54_sp.z;
            stockpiles[i].width = v54_sp.width;
            stockpiles[i].height = v54_sp.height;
            stockpiles[i].active = v54_sp.active;
            memcpy(stockpiles[i].allowedTypes, v54_sp.allowedTypes,
                   sizeof(v54_sp.allowedTypes));
            // Default new item types to true
            for (int t = V54_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v54_sp.allowedMaterials,
                   sizeof(v54_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v54_sp.cells, sizeof(v54_sp.cells));
            memcpy(stockpiles[i].slots, v54_sp.slots, sizeof(v54_sp.slots));
            memcpy(stockpiles[i].reservedBy, v54_sp.reservedBy, sizeof(v54_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v54_sp.slotCounts, sizeof(v54_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v54_sp.slotTypes, sizeof(v54_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v54_sp.slotMaterials, sizeof(v54_sp.slotMaterials));
            stockpiles[i].maxStackSize = v54_sp.maxStackSize;
            stockpiles[i].priority = v54_sp.priority;
            stockpiles[i].maxContainers = v54_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v54_sp.slotIsContainer, sizeof(v54_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v54_sp.groundItemIdx, sizeof(v54_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v54_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 61) {
        // v55-v60: 33 item types, migrate to 34
        StockpileV60 v60_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v60_sp, sizeof(StockpileV60), 1, f);
            stockpiles[i].x = v60_sp.x;
            stockpiles[i].y = v60_sp.y;
            stockpiles[i].z = v60_sp.z;
            stockpiles[i].width = v60_sp.width;
            stockpiles[i].height = v60_sp.height;
            stockpiles[i].active = v60_sp.active;
            memcpy(stockpiles[i].allowedTypes, v60_sp.allowedTypes,
                   sizeof(v60_sp.allowedTypes));
            for (int t = V60_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v60_sp.allowedMaterials,
                   sizeof(v60_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v60_sp.cells, sizeof(v60_sp.cells));
            memcpy(stockpiles[i].slots, v60_sp.slots, sizeof(v60_sp.slots));
            memcpy(stockpiles[i].reservedBy, v60_sp.reservedBy, sizeof(v60_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v60_sp.slotCounts, sizeof(v60_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v60_sp.slotTypes, sizeof(v60_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v60_sp.slotMaterials, sizeof(v60_sp.slotMaterials));
            stockpiles[i].maxStackSize = v60_sp.maxStackSize;
            stockpiles[i].priority = v60_sp.priority;
            stockpiles[i].maxContainers = v60_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v60_sp.slotIsContainer, sizeof(v60_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v60_sp.groundItemIdx, sizeof(v60_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v60_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 66) {
        // v61-v65: 34 item types, migrate to 38
        StockpileV65 v65_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v65_sp, sizeof(StockpileV65), 1, f);
            stockpiles[i].x = v65_sp.x;
            stockpiles[i].y = v65_sp.y;
            stockpiles[i].z = v65_sp.z;
            stockpiles[i].width = v65_sp.width;
            stockpiles[i].height = v65_sp.height;
            stockpiles[i].active = v65_sp.active;
            memcpy(stockpiles[i].allowedTypes, v65_sp.allowedTypes,
                   sizeof(v65_sp.allowedTypes));
            for (int t = V65_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v65_sp.allowedMaterials,
                   sizeof(v65_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v65_sp.cells, sizeof(v65_sp.cells));
            memcpy(stockpiles[i].slots, v65_sp.slots, sizeof(v65_sp.slots));
            memcpy(stockpiles[i].reservedBy, v65_sp.reservedBy, sizeof(v65_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v65_sp.slotCounts, sizeof(v65_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v65_sp.slotTypes, sizeof(v65_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v65_sp.slotMaterials, sizeof(v65_sp.slotMaterials));
            stockpiles[i].maxStackSize = v65_sp.maxStackSize;
            stockpiles[i].priority = v65_sp.priority;
            stockpiles[i].maxContainers = v65_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v65_sp.slotIsContainer, sizeof(v65_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v65_sp.groundItemIdx, sizeof(v65_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v65_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 68) {
        // v66-v67: 38 item types, migrate to 42
        StockpileV67 v67_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v67_sp, sizeof(StockpileV67), 1, f);
            stockpiles[i].x = v67_sp.x;
            stockpiles[i].y = v67_sp.y;
            stockpiles[i].z = v67_sp.z;
            stockpiles[i].width = v67_sp.width;
            stockpiles[i].height = v67_sp.height;
            stockpiles[i].active = v67_sp.active;
            memcpy(stockpiles[i].allowedTypes, v67_sp.allowedTypes,
                   sizeof(v67_sp.allowedTypes));
            for (int t = V67_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v67_sp.allowedMaterials,
                   sizeof(v67_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v67_sp.cells, sizeof(v67_sp.cells));
            memcpy(stockpiles[i].slots, v67_sp.slots, sizeof(v67_sp.slots));
            memcpy(stockpiles[i].reservedBy, v67_sp.reservedBy, sizeof(v67_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v67_sp.slotCounts, sizeof(v67_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v67_sp.slotTypes, sizeof(v67_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v67_sp.slotMaterials, sizeof(v67_sp.slotMaterials));
            stockpiles[i].maxStackSize = v67_sp.maxStackSize;
            stockpiles[i].priority = v67_sp.priority;
            stockpiles[i].maxContainers = v67_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v67_sp.slotIsContainer, sizeof(v67_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v67_sp.groundItemIdx, sizeof(v67_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v67_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 70) {
        // v68-v69: 42 item types, migrate to 45
        StockpileV69 v69_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v69_sp, sizeof(StockpileV69), 1, f);
            stockpiles[i].x = v69_sp.x;
            stockpiles[i].y = v69_sp.y;
            stockpiles[i].z = v69_sp.z;
            stockpiles[i].width = v69_sp.width;
            stockpiles[i].height = v69_sp.height;
            stockpiles[i].active = v69_sp.active;
            memcpy(stockpiles[i].allowedTypes, v69_sp.allowedTypes,
                   sizeof(v69_sp.allowedTypes));
            for (int t = V69_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v69_sp.allowedMaterials,
                   sizeof(v69_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v69_sp.cells, sizeof(v69_sp.cells));
            memcpy(stockpiles[i].slots, v69_sp.slots, sizeof(v69_sp.slots));
            memcpy(stockpiles[i].reservedBy, v69_sp.reservedBy, sizeof(v69_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v69_sp.slotCounts, sizeof(v69_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v69_sp.slotTypes, sizeof(v69_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v69_sp.slotMaterials, sizeof(v69_sp.slotMaterials));
            stockpiles[i].maxStackSize = v69_sp.maxStackSize;
            stockpiles[i].priority = v69_sp.priority;
            stockpiles[i].maxContainers = v69_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v69_sp.slotIsContainer, sizeof(v69_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v69_sp.groundItemIdx, sizeof(v69_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v69_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 72) {
        // v70-v71: 45 item types, 17 materials — same as v73 current (ITEM_ROT/rotten mats removed)
        StockpileV71 v71_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v71_sp, sizeof(StockpileV71), 1, f);
            stockpiles[i].x = v71_sp.x;
            stockpiles[i].y = v71_sp.y;
            stockpiles[i].z = v71_sp.z;
            stockpiles[i].width = v71_sp.width;
            stockpiles[i].height = v71_sp.height;
            stockpiles[i].active = v71_sp.active;
            // v71 had 45 types, 17 mats — same counts as v73 current
            memcpy(stockpiles[i].allowedTypes, v71_sp.allowedTypes, sizeof(v71_sp.allowedTypes));
            memcpy(stockpiles[i].allowedMaterials, v71_sp.allowedMaterials, sizeof(v71_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v71_sp.cells, sizeof(v71_sp.cells));
            memcpy(stockpiles[i].slots, v71_sp.slots, sizeof(v71_sp.slots));
            memcpy(stockpiles[i].reservedBy, v71_sp.reservedBy, sizeof(v71_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v71_sp.slotCounts, sizeof(v71_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v71_sp.slotTypes, sizeof(v71_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v71_sp.slotMaterials, sizeof(v71_sp.slotMaterials));
            stockpiles[i].maxStackSize = v71_sp.maxStackSize;
            stockpiles[i].priority = v71_sp.priority;
            stockpiles[i].maxContainers = v71_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v71_sp.slotIsContainer, sizeof(v71_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v71_sp.groundItemIdx, sizeof(v71_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v71_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version == 72) {
        // v72: 46 item types (includes ITEM_ROT at idx 45), 19 materials (includes rotten mats)
        // Migrate to v73: drop ITEM_ROT type, remap materials, add rejectsRotten
        StockpileV72 v72_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v72_sp, sizeof(StockpileV72), 1, f);
            stockpiles[i].x = v72_sp.x;
            stockpiles[i].y = v72_sp.y;
            stockpiles[i].z = v72_sp.z;
            stockpiles[i].width = v72_sp.width;
            stockpiles[i].height = v72_sp.height;
            stockpiles[i].active = v72_sp.active;
            // Migrate allowedTypes (46 → 45): drop ITEM_ROT (old index 45)
            memcpy(stockpiles[i].allowedTypes, v72_sp.allowedTypes, ITEM_TYPE_COUNT * sizeof(bool));
            // Migrate allowedMaterials (19 → 17): remove MAT_ROTTEN_MEAT(16), MAT_ROTTEN_PLANT(17)
            // Old: [0..15] same, [16]=rotten_meat, [17]=rotten_plant, [18]=bedrock
            // New: [0..15] same, [16]=bedrock
            memset(stockpiles[i].allowedMaterials, 0, sizeof(stockpiles[i].allowedMaterials));
            for (int m = 0; m < 16; m++) {
                stockpiles[i].allowedMaterials[m] = v72_sp.allowedMaterials[m];
            }
            // Old MAT_BEDROCK was at index 18, new MAT_BEDROCK is at index 16
            stockpiles[i].allowedMaterials[MAT_BEDROCK] = v72_sp.allowedMaterials[18];
            memcpy(stockpiles[i].cells, v72_sp.cells, sizeof(v72_sp.cells));
            memcpy(stockpiles[i].slots, v72_sp.slots, sizeof(v72_sp.slots));
            memcpy(stockpiles[i].reservedBy, v72_sp.reservedBy, sizeof(v72_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v72_sp.slotCounts, sizeof(v72_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v72_sp.slotTypes, sizeof(v72_sp.slotTypes));
            // Remap slot materials: shift indices above old rotten range
            for (int s = 0; s < MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE; s++) {
                uint8_t m = v72_sp.slotMaterials[s];
                if (m >= 18) stockpiles[i].slotMaterials[s] = m - 2;      // bedrock+ shift down
                else if (m >= 16) stockpiles[i].slotMaterials[s] = 0;     // rotten mats → MAT_NONE
                else stockpiles[i].slotMaterials[s] = m;
            }
            stockpiles[i].maxStackSize = v72_sp.maxStackSize;
            stockpiles[i].priority = v72_sp.priority;
            stockpiles[i].maxContainers = v72_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v72_sp.slotIsContainer, sizeof(v72_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v72_sp.groundItemIdx, sizeof(v72_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v72_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = true;
        }
    } else if (version < 76) {
        // v73-v75: 45 item types, migrate to 46 (adding ITEM_COMPOST)
        StockpileV75 v75_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v75_sp, sizeof(StockpileV75), 1, f);
            stockpiles[i].x = v75_sp.x;
            stockpiles[i].y = v75_sp.y;
            stockpiles[i].z = v75_sp.z;
            stockpiles[i].width = v75_sp.width;
            stockpiles[i].height = v75_sp.height;
            stockpiles[i].active = v75_sp.active;
            memcpy(stockpiles[i].allowedTypes, v75_sp.allowedTypes,
                   sizeof(v75_sp.allowedTypes));
            for (int t = V75_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v75_sp.allowedMaterials,
                   sizeof(v75_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v75_sp.cells, sizeof(v75_sp.cells));
            memcpy(stockpiles[i].slots, v75_sp.slots, sizeof(v75_sp.slots));
            memcpy(stockpiles[i].reservedBy, v75_sp.reservedBy, sizeof(v75_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v75_sp.slotCounts, sizeof(v75_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v75_sp.slotTypes, sizeof(v75_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v75_sp.slotMaterials, sizeof(v75_sp.slotMaterials));
            stockpiles[i].maxStackSize = v75_sp.maxStackSize;
            stockpiles[i].priority = v75_sp.priority;
            stockpiles[i].maxContainers = v75_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v75_sp.slotIsContainer, sizeof(v75_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v75_sp.groundItemIdx, sizeof(v75_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v75_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = v75_sp.rejectsRotten;
        }
    } else if (version < 77) {
        // v76: 46 item types, migrate to 55 (adding 9 farming items)
        StockpileV76 v76_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v76_sp, sizeof(StockpileV76), 1, f);
            stockpiles[i].x = v76_sp.x;
            stockpiles[i].y = v76_sp.y;
            stockpiles[i].z = v76_sp.z;
            stockpiles[i].width = v76_sp.width;
            stockpiles[i].height = v76_sp.height;
            stockpiles[i].active = v76_sp.active;
            memcpy(stockpiles[i].allowedTypes, v76_sp.allowedTypes,
                   sizeof(v76_sp.allowedTypes));
            for (int t = V76_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v76_sp.allowedMaterials,
                   sizeof(v76_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v76_sp.cells, sizeof(v76_sp.cells));
            memcpy(stockpiles[i].slots, v76_sp.slots, sizeof(v76_sp.slots));
            memcpy(stockpiles[i].reservedBy, v76_sp.reservedBy, sizeof(v76_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v76_sp.slotCounts, sizeof(v76_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v76_sp.slotTypes, sizeof(v76_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v76_sp.slotMaterials, sizeof(v76_sp.slotMaterials));
            stockpiles[i].maxStackSize = v76_sp.maxStackSize;
            stockpiles[i].priority = v76_sp.priority;
            stockpiles[i].maxContainers = v76_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v76_sp.slotIsContainer, sizeof(v76_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v76_sp.groundItemIdx, sizeof(v76_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v76_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = v76_sp.rejectsRotten;
        }
    } else if (version < 78) {
        // v77: 55 item types, migrate to 62 (adding 7 clothing/textile items)
        StockpileV77 v77_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v77_sp, sizeof(StockpileV77), 1, f);
            stockpiles[i].x = v77_sp.x;
            stockpiles[i].y = v77_sp.y;
            stockpiles[i].z = v77_sp.z;
            stockpiles[i].width = v77_sp.width;
            stockpiles[i].height = v77_sp.height;
            stockpiles[i].active = v77_sp.active;
            memcpy(stockpiles[i].allowedTypes, v77_sp.allowedTypes,
                   sizeof(v77_sp.allowedTypes));
            for (int t = V77_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v77_sp.allowedMaterials,
                   sizeof(v77_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v77_sp.cells, sizeof(v77_sp.cells));
            memcpy(stockpiles[i].slots, v77_sp.slots, sizeof(v77_sp.slots));
            memcpy(stockpiles[i].reservedBy, v77_sp.reservedBy, sizeof(v77_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v77_sp.slotCounts, sizeof(v77_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v77_sp.slotTypes, sizeof(v77_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v77_sp.slotMaterials, sizeof(v77_sp.slotMaterials));
            stockpiles[i].maxStackSize = v77_sp.maxStackSize;
            stockpiles[i].priority = v77_sp.priority;
            stockpiles[i].maxContainers = v77_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v77_sp.slotIsContainer, sizeof(v77_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v77_sp.groundItemIdx, sizeof(v77_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v77_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = v77_sp.rejectsRotten;
        }
    } else if (version < 79) {
        // v78: 62 item types, migrate to 65 (adding 3 liquid items)
        StockpileV78 v78_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v78_sp, sizeof(StockpileV78), 1, f);
            stockpiles[i].x = v78_sp.x;
            stockpiles[i].y = v78_sp.y;
            stockpiles[i].z = v78_sp.z;
            stockpiles[i].width = v78_sp.width;
            stockpiles[i].height = v78_sp.height;
            stockpiles[i].active = v78_sp.active;
            memcpy(stockpiles[i].allowedTypes, v78_sp.allowedTypes,
                   sizeof(v78_sp.allowedTypes));
            for (int t = V78_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v78_sp.allowedMaterials,
                   sizeof(v78_sp.allowedMaterials));
            memcpy(stockpiles[i].cells, v78_sp.cells, sizeof(v78_sp.cells));
            memcpy(stockpiles[i].slots, v78_sp.slots, sizeof(v78_sp.slots));
            memcpy(stockpiles[i].reservedBy, v78_sp.reservedBy, sizeof(v78_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v78_sp.slotCounts, sizeof(v78_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v78_sp.slotTypes, sizeof(v78_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v78_sp.slotMaterials, sizeof(v78_sp.slotMaterials));
            stockpiles[i].maxStackSize = v78_sp.maxStackSize;
            stockpiles[i].priority = v78_sp.priority;
            stockpiles[i].maxContainers = v78_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v78_sp.slotIsContainer, sizeof(v78_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v78_sp.groundItemIdx, sizeof(v78_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v78_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = v78_sp.rejectsRotten;
        }
    } else if (version < 80) {
        // v79: 65 item types and 17 materials, migrate to current (adding 2 items + 2 materials)
        StockpileV79 v79_sp;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            fread(&v79_sp, sizeof(StockpileV79), 1, f);
            stockpiles[i].x = v79_sp.x;
            stockpiles[i].y = v79_sp.y;
            stockpiles[i].z = v79_sp.z;
            stockpiles[i].width = v79_sp.width;
            stockpiles[i].height = v79_sp.height;
            stockpiles[i].active = v79_sp.active;
            memcpy(stockpiles[i].allowedTypes, v79_sp.allowedTypes,
                   sizeof(v79_sp.allowedTypes));
            for (int t = V79_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, v79_sp.allowedMaterials,
                   sizeof(v79_sp.allowedMaterials));
            for (int m = V79_MAT_COUNT; m < MAT_COUNT; m++) {
                stockpiles[i].allowedMaterials[m] = true;
            }
            memcpy(stockpiles[i].cells, v79_sp.cells, sizeof(v79_sp.cells));
            memcpy(stockpiles[i].slots, v79_sp.slots, sizeof(v79_sp.slots));
            memcpy(stockpiles[i].reservedBy, v79_sp.reservedBy, sizeof(v79_sp.reservedBy));
            memcpy(stockpiles[i].slotCounts, v79_sp.slotCounts, sizeof(v79_sp.slotCounts));
            memcpy(stockpiles[i].slotTypes, v79_sp.slotTypes, sizeof(v79_sp.slotTypes));
            memcpy(stockpiles[i].slotMaterials, v79_sp.slotMaterials, sizeof(v79_sp.slotMaterials));
            stockpiles[i].maxStackSize = v79_sp.maxStackSize;
            stockpiles[i].priority = v79_sp.priority;
            stockpiles[i].maxContainers = v79_sp.maxContainers;
            memcpy(stockpiles[i].slotIsContainer, v79_sp.slotIsContainer, sizeof(v79_sp.slotIsContainer));
            memcpy(stockpiles[i].groundItemIdx, v79_sp.groundItemIdx, sizeof(v79_sp.groundItemIdx));
            stockpiles[i].freeSlotCount = v79_sp.freeSlotCount;
            stockpiles[i].rejectsRotten = v79_sp.rejectsRotten;
        }
    } else {
        // v80+ format - direct read
        fread(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    }

    // Clear transient reservation counts (not meaningful across save/load)
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        memset(stockpiles[i].reservedBy, 0, sizeof(stockpiles[i].reservedBy));
    }

    // v48→v49 migration: consolidate stockpile stacks
    // Old model: slotCounts[s] items each with stackCount=1 at the same position
    // New model: 1 representative item with stackCount = old slotCounts[s]
    if (version == 48) {
        for (int sp = 0; sp < MAX_STOCKPILES; sp++) {
            if (!stockpiles[sp].active) continue;
            Stockpile* s = &stockpiles[sp];
            int totalSlots = s->width * s->height;
            for (int slot = 0; slot < totalSlots; slot++) {
                if (!s->cells[slot]) continue;
                if (s->slotCounts[slot] <= 1) continue;  // 0 or 1 items, nothing to consolidate

                int worldX = s->x + (slot % s->width);
                int worldY = s->y + (slot / s->width);
                int count = s->slotCounts[slot];

                // Find representative item (first active item at this position)
                int repIdx = -1;
                for (int j = 0; j < itemHighWaterMark; j++) {
                    if (!items[j].active) continue;
                    if (items[j].state != ITEM_IN_STOCKPILE) continue;
                    if ((int)(items[j].x / CELL_SIZE) != worldX) continue;
                    if ((int)(items[j].y / CELL_SIZE) != worldY) continue;
                    if ((int)items[j].z != s->z) continue;
                    if (repIdx < 0) {
                        repIdx = j;
                    } else {
                        // Delete duplicate — representative absorbs it
                        items[j].active = false;
                        itemCount--;
                    }
                }

                if (repIdx >= 0) {
                    items[repIdx].stackCount = count;
                    s->slots[slot] = repIdx;
                }
            }
        }
        // Rebuild itemHighWaterMark after deletions
        while (itemHighWaterMark > 0 && !items[itemHighWaterMark - 1].active) {
            itemHighWaterMark--;
        }
    }
    
    // Gather zones
    fread(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fread(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    if (version >= 63) {
        fread(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    } else {
        // V62 and earlier: Blueprint without workshop fields
        for (int i = 0; i < MAX_BLUEPRINTS; i++) {
            BlueprintV62 old;
            fread(&old, sizeof(BlueprintV62), 1, f);
            Blueprint* bp = &blueprints[i];
            bp->x = old.x; bp->y = old.y; bp->z = old.z;
            bp->active = old.active;
            bp->state = old.state;
            bp->recipeIndex = old.recipeIndex;
            bp->stage = old.stage;
            memcpy(bp->stageDeliveries, old.stageDeliveries, sizeof(old.stageDeliveries));
            memcpy(bp->consumedItems, old.consumedItems, sizeof(old.consumedItems));
            bp->assignedBuilder = old.assignedBuilder;
            bp->progress = old.progress;
            bp->workshopOriginX = 0;
            bp->workshopOriginY = 0;
            bp->workshopType = 0;
        }
    }
    
    // Workshops
    if (version >= 64) {
        fread(workshops, sizeof(Workshop), MAX_WORKSHOPS, f);
    } else {
        for (int i = 0; i < MAX_WORKSHOPS; i++) {
            WorkshopV63 old;
            fread(&old, sizeof(WorkshopV63), 1, f);
            Workshop* ws = &workshops[i];
            ws->x = old.x; ws->y = old.y; ws->z = old.z;
            ws->width = old.width; ws->height = old.height;
            ws->active = old.active;
            ws->type = old.type;
            memcpy(ws->template, old.template, sizeof(old.template));
            memcpy(ws->bills, old.bills, sizeof(old.bills));
            ws->billCount = old.billCount;
            ws->assignedCrafter = old.assignedCrafter;
            ws->passiveProgress = old.passiveProgress;
            ws->passiveBillIdx = old.passiveBillIdx;
            ws->passiveReady = old.passiveReady;
            ws->visualState = old.visualState;
            ws->inputStarvationTime = old.inputStarvationTime;
            ws->outputBlockedTime = old.outputBlockedTime;
            ws->lastWorkTime = old.lastWorkTime;
            ws->workTileX = old.workTileX; ws->workTileY = old.workTileY;
            ws->outputTileX = old.outputTileX; ws->outputTileY = old.outputTileY;
            ws->fuelTileX = old.fuelTileX; ws->fuelTileY = old.fuelTileY;
            memcpy(ws->linkedInputStockpiles, old.linkedInputStockpiles, sizeof(old.linkedInputStockpiles));
            ws->linkedInputCount = old.linkedInputCount;
            ws->markedForDeconstruct = false;
            ws->assignedDeconstructor = -1;
        }
    }
    
    // Movers
    fread(&moverCount, sizeof(moverCount), 1, f);
    if (version >= 79) {
        // v79+: Mover struct with thirst/dehydrationTimer, paths separate
        fread(movers, sizeof(Mover), moverCount, f);
        for (int i = 0; i < moverCount; i++) {
            fread(moverPaths[i], sizeof(Point), MAX_MOVER_PATH, f);
        }
    } else if (version >= 78) {
        // v78: Mover without thirst/dehydrationTimer, paths separate
        for (int i = 0; i < moverCount; i++) {
            MoverV78 old;
            fread(&old, sizeof(MoverV78), 1, f);
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
            m->thirst = 1.0f;
            m->dehydrationTimer = 0.0f;
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
        }
    } else if (version >= 69) {
        // v69-v77: Mover without equippedClothing, paths separate
        for (int i = 0; i < moverCount; i++) {
            MoverV77 old;
            fread(&old, sizeof(MoverV77), 1, f);
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
            m->thirst = 1.0f;
            m->dehydrationTimer = 0.0f;
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
            m->equippedClothing = -1;
        }
    } else if (version >= 65) {
        // V65-V68: old Mover with path[] inline
        for (int i = 0; i < moverCount; i++) {
            MoverV68 old;
            fread(&old, sizeof(MoverV68), 1, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            memcpy(moverPaths[i], old.path, sizeof(old.path));
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
            m->equippedClothing = -1;
        }
    } else if (version >= 59) {
        // V59-V64 movers don't have equippedTool field
        for (int i = 0; i < moverCount; i++) {
            MoverV64 old;
            fread(&old, sizeof(MoverV64), 1, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            memcpy(moverPaths[i], old.path, sizeof(old.path));
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
        }
    } else if (version >= 58) {
        // V58 movers don't have bodyTemp/hypothermiaTimer fields
        for (int i = 0; i < moverCount; i++) {
            MoverV58 old;
            fread(&old, sizeof(MoverV58), 1, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            memcpy(moverPaths[i], old.path, sizeof(old.path));
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
            m->bodyTemp = 37.0f;
            m->hypothermiaTimer = 0.0f;
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
        }
    } else if (version >= 53) {
        // V53-V57 movers don't have starvationTimer field
        for (int i = 0; i < moverCount; i++) {
            MoverV57 old;
            fread(&old, sizeof(MoverV57), 1, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            memcpy(moverPaths[i], old.path, sizeof(old.path));
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
            m->starvationTimer = 0.0f;  // Init new field
            m->bodyTemp = 37.0f;
            m->hypothermiaTimer = 0.0f;
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
        }
    } else if (version >= 48) {
        // V48-V52 movers don't have energy field — read with old struct, then copy
        for (int i = 0; i < moverCount; i++) {
            MoverV52 old;
            fread(&old, sizeof(MoverV52), 1, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            memcpy(moverPaths[i], old.path, sizeof(old.path));
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
            m->energy = 1.0f;  // Init new field
            m->freetimeState = old.freetimeState;
            m->needTarget = old.needTarget;
            m->needProgress = old.needProgress;
            m->needSearchCooldown = old.needSearchCooldown;
            m->starvationTimer = 0.0f;
            m->bodyTemp = 37.0f;
            m->hypothermiaTimer = 0.0f;
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
        }
    } else {
        // V47 movers don't have hunger/needs fields — read with old struct, then copy
        for (int i = 0; i < moverCount; i++) {
            MoverV47 old;
            fread(&old, sizeof(MoverV47), 1, f);
            Mover* m = &movers[i];
            m->x = old.x; m->y = old.y; m->z = old.z;
            m->goal = old.goal;
            memcpy(moverPaths[i], old.path, sizeof(old.path));
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
            m->avoidX = old.avoidX; m->avoidY = old.avoidY;
            m->currentJobId = old.currentJobId;
            m->lastJobType = old.lastJobType;
            m->lastJobResult = old.lastJobResult;
            m->lastJobTargetX = old.lastJobTargetX;
            m->lastJobTargetY = old.lastJobTargetY;
            m->lastJobTargetZ = old.lastJobTargetZ;
            m->lastJobEndTick = old.lastJobEndTick;
            m->capabilities = old.capabilities;
            // Init new hunger/needs/energy fields
            m->hunger = 1.0f;
            m->energy = 1.0f;
            m->freetimeState = FREETIME_NONE;
            m->needTarget = -1;
            m->needProgress = 0.0f;
            m->needSearchCooldown = 0.0f;
            m->starvationTimer = 0.0f;
            m->bodyTemp = 37.0f;
            m->hypothermiaTimer = 0.0f;
        }
    }

    // Initialize canPlant for old saves (field added later)
    for (int i = 0; i < moverCount; i++) {
        movers[i].capabilities.canPlant = true;
    }

    // Initialize equippedTool for old saves (v65+)
    if (version < 65) {
        for (int i = 0; i < moverCount; i++) {
            movers[i].equippedTool = -1;
        }
    }

    // Initialize equippedClothing for old saves (v78+)
    if (version < 78) {
        for (int i = 0; i < moverCount; i++) {
            movers[i].equippedClothing = -1;
        }
    }

    // Initialize thirst for old saves (v79+)
    if (version < 79) {
        for (int i = 0; i < moverCount; i++) {
            movers[i].thirst = 1.0f;
            movers[i].dehydrationTimer = 0.0f;
        }
    }

    // Animals (v42+)
    if (version >= 42) {
        fread(&animalCount, sizeof(animalCount), 1, f);
        if (animalCount > 0) {
            fread(animals, sizeof(Animal), animalCount, f);
        }
    } else {
        animalCount = 0;
    }

    // Trains (v47+, struct changed from v46)
    if (version >= 47) {
        fread(&trainCount, sizeof(trainCount), 1, f);
        if (trainCount > 0) {
            fread(trains, sizeof(Train), trainCount, f);
        }
    } else if (version == 46) {
        // v46 had trains with smaller struct (no lightCellX/Y) — skip the data
        int oldCount;
        fread(&oldCount, sizeof(oldCount), 1, f);
        if (oldCount > 0) {
            // Old struct was 40 bytes (Train without lightCellX/lightCellY)
            fseek(f, oldCount * (sizeof(Train) - 2 * sizeof(int)), SEEK_CUR);
        }
        trainCount = 0;
    } else {
        trainCount = 0;
    }

    // Jobs
    fread(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fread(&activeJobCount, sizeof(activeJobCount), 1, f);
    fread(jobs, sizeof(Job), jobHighWaterMark, f);
    fread(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fread(activeJobList, sizeof(int), activeJobCount, f);
    
    // Light sources (v37+)
    if (version >= 37) {
        fread(&lightSourceCount, sizeof(lightSourceCount), 1, f);
        if (lightSourceCount > 0) {
            fread(lightSources, sizeof(LightSource), lightSourceCount, f);
        }
    } else {
        lightSourceCount = 0;
        memset(lightSources, 0, sizeof(lightSources));
    }
    InvalidateLighting();
    
    // Plants (v48+)
    if (version >= 48) {
        fread(&plantCount, sizeof(plantCount), 1, f);
        if (plantCount > 0) {
            fread(plants, sizeof(Plant), plantCount, f);
        }
    } else {
        InitPlants();
    }
    
    // Furniture (v54+)
    if (version >= 54) {
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
    } else {
        ClearFurniture();
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
    if (version < 56) {
        // v55 and earlier: saplingGrowTicks/trunkGrowTicks were int fields.
        // The X-macro will read 4 bytes of int data into the float variable.
        // We use a separate V53 settings table to read old format, then convert.
        #define READ_SETTING_V53(type, name) fread(&name, sizeof(type), 1, f);
        // Read all fields before Trees section using their current types
        // (they haven't changed), then read the two tree ints, then continue.
        // Actually simpler: since int and float are both 4 bytes, just read
        // the raw bytes, then reinterpret the tree fields.
        SETTINGS_TABLE(READ_SETTING_V53)
        #undef READ_SETTING_V53

        // The two tree fields were read as float but contain int bit patterns.
        // Reinterpret and convert: oldTicks / 60.0 * 0.4
        int oldSaplingTicks, oldTrunkTicks;
        memcpy(&oldSaplingTicks, &saplingGrowGH, sizeof(int));
        memcpy(&oldTrunkTicks, &trunkGrowGH, sizeof(int));
        saplingGrowGH = (float)oldSaplingTicks / 60.0f * 0.4f;
        trunkGrowGH = (float)oldTrunkTicks / 60.0f * 0.4f;
        // Clamp to sane defaults if conversion produces garbage
        if (saplingGrowGH <= 0.0f || saplingGrowGH > 1000.0f) saplingGrowGH = 0.667f;
        if (trunkGrowGH <= 0.0f || trunkGrowGH > 1000.0f) trunkGrowGH = 0.333f;
    } else {
        #define READ_SETTING(type, name) fread(&name, sizeof(type), 1, f);
        SETTINGS_TABLE(READ_SETTING)
        if (version >= 57) {
            BALANCE_SETTINGS_TABLE(READ_SETTING)
        }
        #undef READ_SETTING
    }

    // v73 and earlier: animal respawn settings not saved, use defaults
    if (version < 74) {
        animalRespawnEnabled = true;
        animalTargetPopulation = 8;
        animalSpawnInterval = 180.0f;
    }

    // v60+: diurnal amplitude
    if (version >= 60) {
        fread(&diurnalAmplitude, sizeof(int), 1, f);
    } else {
        diurnalAmplitude = 5;
    }

    // v56 and earlier: balance table not saved, use defaults
    if (version < 57) {
        InitBalance();
    } else {
        RecalcBalanceTable();
    }

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
    if (version >= 44) {
        fread(&accum, sizeof(float), 1, f); SetRainWetnessAccum(accum);
        fread(&accum, sizeof(float), 1, f); SetWeatherWindAccum(accum);

        // Weather state
        fread(&weatherState, sizeof(WeatherState), 1, f);
    }
    
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
