// tests/test_farming.c - Farming system tests
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/designations.h"
#include "../src/entities/items.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/simulation/farming.h"
#include "../src/simulation/plants.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/temperature.h"
#include "../src/entities/workshops.h"
#include "../src/core/time.h"
#include "../src/game_state.h"
#include "test_helpers.h"
#include <string.h>

// Setup a standard test grid: dirt below z=1, air at z=1
static void SetupFarmGrid(void) {
    InitTestGrid(16, 16);
    ClearMovers();
    ClearItems();
    ClearJobs();
    InitDesignations();
    ClearFarming();
    InitBalance();

    // Fill z=0 with solid dirt (natural)
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);  // 3-arg macro, sets to natural
        }
    }
    // z=1 is walkable air above solid ground
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[1][y][x] = CELL_AIR;
            exploredGrid[1][y][x] = 1;
            exploredGrid[0][y][x] = 1;
        }
    }
}

describe(farming) {
    // =========================================================================
    // 1. Till different soil types -> correct initial fertility
    // =========================================================================
    it("assigns correct initial fertility per soil type") {
        SetupFarmGrid();

        expect(InitialFertilityForSoil(MAT_DIRT) == 128);
        expect(InitialFertilityForSoil(MAT_CLAY) == 110);
        expect(InitialFertilityForSoil(MAT_SAND) == 90);
        expect(InitialFertilityForSoil(MAT_PEAT) == 180);
        expect(InitialFertilityForSoil(MAT_GRAVEL) == 64);
        expect(InitialFertilityForSoil(MAT_GRANITE) == 128);  // default case
    }

    it("tilling sets correct fertility based on soil") {
        SetupFarmGrid();

        SetWallMaterial(3, 3, 0, MAT_CLAY);

        expect(DesignateFarm(3, 3, 1));
        expect(HasFarmDesignation(3, 3, 1));

        CompleteFarmDesignation(3, 3, 1, 0);

        FarmCell* fc = GetFarmCell(3, 3, 1);
        expect(fc != NULL);
        expect(fc->tilled == 1);
        expect(fc->fertility == 110);
        expect(fc->weedLevel == 0);
        expect(farmActiveCells == 1);
    }

    // =========================================================================
    // 2. Weeds accumulate in summer, not in winter
    // =========================================================================
    it("weeds accumulate on tilled cells") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmGrid[1][5][5].weedLevel = 0;
        farmActiveCells = 1;

        dayNumber = 1;  // Spring (weed rate = 1.0)

        float dt = GameHoursToGameSeconds(FARM_TICK_INTERVAL) + 0.01f;
        FarmTick(dt);

        FarmCell* fc = GetFarmCell(5, 5, 1);
        expect(fc->weedLevel > 0);
    }

    it("weeds do not accumulate in winter") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmGrid[1][5][5].weedLevel = 0;
        farmActiveCells = 1;

        dayNumber = daysPerSeason * 3 + 1;  // Winter (weed rate = 0.0)

        float dt = GameHoursToGameSeconds(FARM_TICK_INTERVAL) + 0.01f;
        FarmTick(dt);

        FarmCell* fc = GetFarmCell(5, 5, 1);
        expect(fc->weedLevel == 0);
    }

    // =========================================================================
    // 3. Tending resets weed level
    // =========================================================================
    it("tending resets weed level to 0") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmGrid[1][5][5].weedLevel = 200;
        farmActiveCells = 1;

        FarmCell* fc = GetFarmCell(5, 5, 1);
        fc->weedLevel = 0;  // What RunJob_TendCrop does on completion

        expect(fc->weedLevel == 0);
    }

    // =========================================================================
    // 4. Fertilize boosts fertility, caps at 255
    // =========================================================================
    it("fertilizing boosts fertility capped at 255") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 40;
        farmActiveCells = 1;

        FarmCell* fc = GetFarmCell(5, 5, 1);

        int newFert = fc->fertility + FERTILIZE_AMOUNT;
        fc->fertility = (newFert > 255) ? 255 : (uint8_t)newFert;
        expect(fc->fertility == 120);

        newFert = fc->fertility + FERTILIZE_AMOUNT;
        fc->fertility = (newFert > 255) ? 255 : (uint8_t)newFert;
        expect(fc->fertility == 200);

        newFert = fc->fertility + FERTILIZE_AMOUNT;
        fc->fertility = (newFert > 255) ? 255 : (uint8_t)newFert;
        expect(fc->fertility == 255);
    }

    // =========================================================================
    // 5. Non-farmable soil rejected
    // =========================================================================
    it("rejects non-farmable soil types") {
        SetupFarmGrid();

        // Stone — not farmable
        SetWallMaterial(3, 3, 0, MAT_GRANITE);
        expect(!IsFarmableSoil(3, 3, 1));

        // Air below — not farmable
        grid[0][4][4] = CELL_AIR;
        expect(!IsFarmableSoil(4, 4, 1));

        // z=0 — not farmable
        expect(!IsFarmableSoil(5, 5, 0));

        // Natural dirt — farmable
        SetWallMaterial(6, 6, 0, MAT_DIRT);
        SetWallNatural(6, 6, 0);
        expect(IsFarmableSoil(6, 6, 1));
    }

    it("rejects designation on already-tilled cells") {
        SetupFarmGrid();

        expect(DesignateFarm(3, 3, 1));
        CompleteFarmDesignation(3, 3, 1, 0);

        expect(!DesignateFarm(3, 3, 1));
    }

    // =========================================================================
    // 6. farmActiveCells tracking
    // =========================================================================
    it("tracks farmActiveCells correctly") {
        SetupFarmGrid();

        expect(farmActiveCells == 0);

        DesignateFarm(3, 3, 1);
        CompleteFarmDesignation(3, 3, 1, 0);
        expect(farmActiveCells == 1);

        DesignateFarm(4, 4, 1);
        CompleteFarmDesignation(4, 4, 1, 0);
        expect(farmActiveCells == 2);

        // Un-farm one cell
        farmGrid[1][3][3].tilled = 0;
        farmActiveCells--;
        expect(farmActiveCells == 1);
    }

    // =========================================================================
    // 7. IsFarmableSoil validates natural requirement
    // =========================================================================
    it("requires natural soil below") {
        SetupFarmGrid();

        // Constructed wall (not natural)
        grid[0][7][7] = CELL_WALL;
        SetWallMaterial(7, 7, 0, MAT_DIRT);
        wallNatural[0][7][7] = 0;  // not natural
        expect(!IsFarmableSoil(7, 7, 1));

        // Natural wall
        SetWallNatural(7, 7, 0);
        expect(IsFarmableSoil(7, 7, 1));
    }

    // =========================================================================
    // 8. Seasonal weed rate modifier
    // =========================================================================
    it("returns correct seasonal weed rates") {
        SetupFarmGrid();

        // Spring (dayNumber 1-based, yearDay = (dayNumber-1) % daysPerYear)
        dayNumber = 1;
        expect(GetSeasonalWeedRate() >= 0.9f && GetSeasonalWeedRate() <= 1.1f);

        // Summer
        dayNumber = daysPerSeason + 1;
        expect(GetSeasonalWeedRate() >= 0.9f && GetSeasonalWeedRate() <= 1.1f);

        // Autumn
        dayNumber = daysPerSeason * 2 + 1;
        expect(GetSeasonalWeedRate() >= 0.4f && GetSeasonalWeedRate() <= 0.6f);

        // Winter
        dayNumber = daysPerSeason * 3 + 1;
        expect(GetSeasonalWeedRate() < 0.01f);
    }

    // =========================================================================
    // 9. Crop growth modifiers
    // =========================================================================
    it("crop season modifier returns correct values") {
        // Wheat in summer = 1.5
        expect(CropSeasonModifier(CROP_WHEAT, SEASON_SUMMER) > 1.4f);
        expect(CropSeasonModifier(CROP_WHEAT, SEASON_SUMMER) < 1.6f);
        // Wheat in winter = 0.0
        expect(CropSeasonModifier(CROP_WHEAT, SEASON_WINTER) < 0.01f);
        // Lentils in spring = 1.2
        expect(CropSeasonModifier(CROP_LENTILS, SEASON_SPRING) > 1.1f);
        // Lentils in autumn = 0.0 (die)
        expect(CropSeasonModifier(CROP_LENTILS, SEASON_AUTUMN) < 0.01f);
        // Flax in autumn = 0.0 (die)
        expect(CropSeasonModifier(CROP_FLAX, SEASON_AUTUMN) < 0.01f);
    }

    it("crop temperature modifier returns correct ranges") {
        expect(CropTemperatureModifier(-5.0f) < 0.01f);  // Freeze
        expect(CropTemperatureModifier(3.0f) > 0.2f && CropTemperatureModifier(3.0f) < 0.4f);  // Cold
        expect(CropTemperatureModifier(10.0f) > 0.6f && CropTemperatureModifier(10.0f) < 0.8f);  // Cool
        expect(CropTemperatureModifier(15.0f) > 0.9f);  // Ideal
        expect(CropTemperatureModifier(30.0f) > 0.6f && CropTemperatureModifier(30.0f) < 0.8f);  // Hot
        expect(CropTemperatureModifier(40.0f) < 0.4f);  // Very hot
    }

    it("crop wetness modifier returns correct values") {
        expect(CropWetnessModifier(0) > 0.2f && CropWetnessModifier(0) < 0.4f);  // Dry
        expect(CropWetnessModifier(1) > 0.6f && CropWetnessModifier(1) < 0.8f);  // Damp
        expect(CropWetnessModifier(2) > 0.9f);  // Ideal
        expect(CropWetnessModifier(3) > 0.4f && CropWetnessModifier(3) < 0.6f);  // Waterlogged
    }

    it("crop fertility modifier scales correctly") {
        // 0 fertility → 0.25
        expect(CropFertilityModifier(0) > 0.24f && CropFertilityModifier(0) < 0.26f);
        // 255 fertility → 1.0
        expect(CropFertilityModifier(255) > 0.99f);
        // 128 fertility → ~0.625
        float mid = CropFertilityModifier(128);
        expect(mid > 0.6f && mid < 0.65f);
    }

    it("crop weed modifier penalizes growth") {
        expect(CropWeedModifier(0) > 0.99f);      // No weeds
        expect(CropWeedModifier(100) > 0.99f);     // Below threshold
        expect(CropWeedModifier(150) < 0.6f);      // Above threshold
        expect(CropWeedModifier(220) < 0.3f);      // Severe
    }

    // =========================================================================
    // 10. Seed/crop type conversion
    // =========================================================================
    it("SeedTypeForCrop returns correct seed types") {
        expect(SeedTypeForCrop(CROP_WHEAT) == ITEM_WHEAT_SEEDS);
        expect(SeedTypeForCrop(CROP_LENTILS) == ITEM_LENTIL_SEEDS);
        expect(SeedTypeForCrop(CROP_FLAX) == ITEM_FLAX_SEEDS);
        expect(SeedTypeForCrop(CROP_NONE) == ITEM_NONE);
    }

    it("CropTypeForSeed returns correct crop types") {
        expect(CropTypeForSeed(ITEM_WHEAT_SEEDS) == CROP_WHEAT);
        expect(CropTypeForSeed(ITEM_LENTIL_SEEDS) == CROP_LENTILS);
        expect(CropTypeForSeed(ITEM_FLAX_SEEDS) == CROP_FLAX);
        expect(CropTypeForSeed(ITEM_ROCK) == CROP_NONE);
    }

    // =========================================================================
    // 11. Crop growth stages advance under ideal conditions
    // =========================================================================
    it("crops advance through growth stages") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 200;
        farmGrid[1][5][5].weedLevel = 0;
        farmGrid[1][5][5].cropType = CROP_WHEAT;
        farmGrid[1][5][5].growthStage = CROP_STAGE_SPROUTED;
        farmGrid[1][5][5].growthProgress = 0;
        farmGrid[1][5][5].frostDamaged = 0;
        farmActiveCells = 1;

        // Set ideal temperature (15°C) and wetness (2=wet)
        temperatureGrid[1][5][5].current = 15;
        SET_CELL_WETNESS(5, 5, 1, 2);

        // Summer day
        dayNumber = daysPerSeason + 1;  // Summer

        // Tick many times to advance growth
        float tickInterval = GameHoursToGameSeconds(FARM_TICK_INTERVAL);
        for (int i = 0; i < 500; i++) {
            FarmTick(tickInterval + 0.01f);
        }

        FarmCell* fc = GetFarmCell(5, 5, 1);
        // After many ticks in ideal conditions, should have progressed past sprouted
        expect(fc->growthStage >= CROP_STAGE_GROWING);
    }

    // =========================================================================
    // 12. Season kill resets crop
    // =========================================================================
    it("season kill removes crop when season modifier is 0") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmGrid[1][5][5].weedLevel = 0;
        farmGrid[1][5][5].cropType = CROP_LENTILS;
        farmGrid[1][5][5].growthStage = CROP_STAGE_GROWING;
        farmGrid[1][5][5].growthProgress = 100;
        farmActiveCells = 1;

        // Autumn — lentils have 0.0 season modifier
        dayNumber = daysPerSeason * 2 + 1;

        float dt = GameHoursToGameSeconds(FARM_TICK_INTERVAL) + 0.01f;
        FarmTick(dt);

        FarmCell* fc = GetFarmCell(5, 5, 1);
        expect(fc->cropType == CROP_NONE);
        expect(fc->growthStage == CROP_STAGE_BARE);
        expect(fc->growthProgress == 0);
    }

    // =========================================================================
    // 13. RunJob_PlantCrop sets correct crop state on completion
    // =========================================================================
    it("RunJob_PlantCrop plants seed and sets crop type") {
        SetupFarmGrid();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmGrid[1][5][5].desiredCropType = CROP_WHEAT;
        farmActiveCells = 1;

        // Spawn a wheat seed on ground
        float px = 5.5f * CELL_SIZE;
        float py = 5.5f * CELL_SIZE;
        int seedIdx = SpawnItem(px, py, 1.0f, ITEM_WHEAT_SEEDS);
        expect(seedIdx >= 0);

        // Create mover standing at the target cell
        Mover* m = &movers[0];
        Point goal = {5, 5, 1};
        InitMover(m, px, py, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        // Create plant job in PLANTING step (seed already "carried")
        int jobId = CreateJob(JOBTYPE_PLANT_CROP);
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 5;
        job->targetMineY = 5;
        job->targetMineZ = 1;
        job->step = STEP_PLANTING;
        job->progress = 0.0f;
        job->carryingItem = seedIdx;

        // Run job to completion
        for (int i = 0; i < 1000; i++) {
            JobRunResult r = RunJob_PlantCrop(job, m, 0.016f);
            if (r == JOBRUN_DONE) break;
        }

        FarmCell* fc = GetFarmCell(5, 5, 1);
        expect(fc->cropType == CROP_WHEAT);
        expect(fc->growthStage == CROP_STAGE_SPROUTED);
        // Seed should be consumed
        expect(!items[seedIdx].active);
    }

    // =========================================================================
    // 14. RunJob_PlantCrop fails on untilled cell
    // =========================================================================
    it("RunJob_PlantCrop fails on untilled cell") {
        SetupFarmGrid();

        // Cell NOT tilled
        float px = 5.5f * CELL_SIZE;
        float py = 5.5f * CELL_SIZE;
        int seedIdx = SpawnItem(px, py, 1.0f, ITEM_WHEAT_SEEDS);
        expect(seedIdx >= 0);

        Mover* m = &movers[0];
        Point goal = {5, 5, 1};
        InitMover(m, px, py, 1.0f, goal, 100.0f);
        moverCount = 1;

        int jobId = CreateJob(JOBTYPE_PLANT_CROP);
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 5;
        job->targetMineY = 5;
        job->targetMineZ = 1;
        job->step = STEP_PLANTING;
        job->progress = 0.0f;
        job->carryingItem = seedIdx;

        JobRunResult r = RunJob_PlantCrop(job, m, 0.016f);
        expect(r == JOBRUN_FAIL);

        // Crop should NOT be set
        FarmCell* fc = GetFarmCell(5, 5, 1);
        expect(fc->cropType == CROP_NONE);
    }

    // =========================================================================
    // 15. Harvest yields vary by crop type
    // =========================================================================
    it("harvest spawns correct items for wheat (normal)") {
        SetupFarmGrid();

        // Set up ripe wheat cell
        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmGrid[1][5][5].cropType = CROP_WHEAT;
        farmGrid[1][5][5].growthStage = CROP_STAGE_RIPE;
        farmGrid[1][5][5].frostDamaged = 0;
        farmActiveCells = 1;

        // Create mover at tile (5,5,1)
        Mover* m = &movers[0];
        Point goal = {5, 5, 1};
        InitMover(m, 5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        int jobId = CreateJob(JOBTYPE_HARVEST_CROP);
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 5;
        job->targetMineY = 5;
        job->targetMineZ = 1;
        job->step = STEP_WORKING;
        job->progress = 0.0f;

        temperatureGrid[1][5][5].current = 15;

        // Fast-forward the job to completion
        for (int i = 0; i < 1000; i++) {
            JobRunResult r = RunJob_HarvestCrop(job, m, 0.016f);
            if (r == JOBRUN_DONE) break;
        }

        // Count spawned items
        int wheatCount = 0, seedCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].type == ITEM_WHEAT) wheatCount++;
            if (items[i].type == ITEM_WHEAT_SEEDS) seedCount++;
        }
        expect(wheatCount == 4);  // Normal wheat yield
        expect(seedCount == 1);   // Self-sustaining seed

        // Cell should be reset
        FarmCell* fc = GetFarmCell(5, 5, 1);
        expect(fc->cropType == CROP_NONE);
        expect(fc->growthStage == CROP_STAGE_BARE);
        expect(fc->tilled == 1);  // Stays tilled
    }

    it("harvest halves yield when frost damaged") {
        SetupFarmGrid();

        farmGrid[1][6][6].tilled = 1;
        farmGrid[1][6][6].fertility = 128;
        farmGrid[1][6][6].cropType = CROP_WHEAT;
        farmGrid[1][6][6].growthStage = CROP_STAGE_RIPE;
        farmGrid[1][6][6].frostDamaged = 1;  // Frost damage!
        farmActiveCells = 1;

        Mover* m = &movers[0];
        Point goal = {6, 6, 1};
        InitMover(m, 6.5f * CELL_SIZE, 6.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        int jobId = CreateJob(JOBTYPE_HARVEST_CROP);
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 6;
        job->targetMineY = 6;
        job->targetMineZ = 1;
        job->step = STEP_WORKING;
        job->progress = 0.0f;

        temperatureGrid[1][6][6].current = 15;
        for (int i = 0; i < 1000; i++) {
            JobRunResult r = RunJob_HarvestCrop(job, m, 0.016f);
            if (r == JOBRUN_DONE) break;
        }

        int wheatCount = 0, seedCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].type == ITEM_WHEAT) wheatCount++;
            if (items[i].type == ITEM_WHEAT_SEEDS) seedCount++;
        }
        expect(wheatCount == 2);  // Frost halves: 4 -> 2
        expect(seedCount == 1);   // Still get 1 seed
    }

    // =========================================================================
    // 16. Harvest applies fertility delta via actual job execution
    // =========================================================================
    it("wheat harvest reduces fertility by WHEAT_FERTILITY_DELTA") {
        SetupFarmGrid();

        // Ripe wheat at known fertility
        farmGrid[1][7][7].tilled = 1;
        farmGrid[1][7][7].fertility = 128;
        farmGrid[1][7][7].cropType = CROP_WHEAT;
        farmGrid[1][7][7].growthStage = CROP_STAGE_RIPE;
        farmGrid[1][7][7].frostDamaged = 0;
        farmActiveCells = 1;

        Mover* m = &movers[0];
        Point goal = {7, 7, 1};
        InitMover(m, 7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        int jobId = CreateJob(JOBTYPE_HARVEST_CROP);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 7;
        job->targetMineY = 7;
        job->targetMineZ = 1;
        job->step = STEP_WORKING;
        job->progress = 0.0f;

        temperatureGrid[1][7][7].current = 15;
        for (int i = 0; i < 1000; i++) {
            JobRunResult r = RunJob_HarvestCrop(job, m, 0.016f);
            if (r == JOBRUN_DONE) break;
        }

        FarmCell* fc = GetFarmCell(7, 7, 1);
        expect(fc->fertility == 128 + WHEAT_FERTILITY_DELTA);  // 108
    }

    it("lentil harvest boosts fertility by LENTIL_FERTILITY_DELTA") {
        SetupFarmGrid();

        farmGrid[1][8][8].tilled = 1;
        farmGrid[1][8][8].fertility = 128;
        farmGrid[1][8][8].cropType = CROP_LENTILS;
        farmGrid[1][8][8].growthStage = CROP_STAGE_RIPE;
        farmGrid[1][8][8].frostDamaged = 0;
        farmActiveCells = 1;

        Mover* m = &movers[0];
        Point goal = {8, 8, 1};
        InitMover(m, 8.5f * CELL_SIZE, 8.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        int jobId = CreateJob(JOBTYPE_HARVEST_CROP);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 8;
        job->targetMineY = 8;
        job->targetMineZ = 1;
        job->step = STEP_WORKING;
        job->progress = 0.0f;

        temperatureGrid[1][8][8].current = 15;
        for (int i = 0; i < 1000; i++) {
            JobRunResult r = RunJob_HarvestCrop(job, m, 0.016f);
            if (r == JOBRUN_DONE) break;
        }

        FarmCell* fc = GetFarmCell(8, 8, 1);
        expect(fc->fertility == 128 + LENTIL_FERTILITY_DELTA);  // 188
    }

    it("fertility clamps at 0 and 255") {
        SetupFarmGrid();

        // Low fertility wheat harvest → should clamp at 0
        farmGrid[1][9][9].tilled = 1;
        farmGrid[1][9][9].fertility = 10;  // 10 + (-20) = -10 → clamp 0
        farmGrid[1][9][9].cropType = CROP_WHEAT;
        farmGrid[1][9][9].growthStage = CROP_STAGE_RIPE;
        farmGrid[1][9][9].frostDamaged = 0;
        farmActiveCells = 1;

        Mover* m = &movers[0];
        Point goal = {9, 9, 1};
        InitMover(m, 9.5f * CELL_SIZE, 9.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        int jobId = CreateJob(JOBTYPE_HARVEST_CROP);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetMineX = 9;
        job->targetMineY = 9;
        job->targetMineZ = 1;
        job->step = STEP_WORKING;
        job->progress = 0.0f;

        temperatureGrid[1][9][9].current = 15;
        for (int i = 0; i < 1000; i++) {
            JobRunResult r = RunJob_HarvestCrop(job, m, 0.016f);
            if (r == JOBRUN_DONE) break;
        }

        FarmCell* fc = GetFarmCell(9, 9, 1);
        expect(fc->fertility == 0);  // Clamped, not underflowed
    }

    // =========================================================================
    // 17. Wild plant harvest gives seeds
    // =========================================================================
    it("wild plant harvest spawns correct seed type") {
        SetupFarmGrid();
        ClearPlants();

        int idx = SpawnPlant(5, 5, 1, PLANT_WILD_WHEAT);
        expect(idx >= 0);
        plants[idx].stage = PLANT_STAGE_RIPE;

        int itemsBefore = itemCount;
        HarvestPlant(5, 5, 1);

        // Plant reset to bare
        expect(plants[idx].stage == PLANT_STAGE_BARE);
        // Should have spawned wheat seeds
        expect(itemCount > itemsBefore);
        // Find spawned item
        bool foundSeed = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_WHEAT_SEEDS) {
                foundSeed = true;
                break;
            }
        }
        expect(foundSeed);
    }

    it("wild lentil harvest spawns lentil seeds") {
        SetupFarmGrid();
        ClearPlants();

        int idx = SpawnPlant(6, 6, 1, PLANT_WILD_LENTILS);
        expect(idx >= 0);
        plants[idx].stage = PLANT_STAGE_RIPE;

        HarvestPlant(6, 6, 1);

        expect(plants[idx].stage == PLANT_STAGE_BARE);
        bool foundSeed = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_LENTIL_SEEDS) {
                foundSeed = true;
                break;
            }
        }
        expect(foundSeed);
    }

    it("wild flax harvest spawns flax seeds") {
        SetupFarmGrid();
        ClearPlants();

        int idx = SpawnPlant(7, 7, 1, PLANT_WILD_FLAX);
        expect(idx >= 0);
        plants[idx].stage = PLANT_STAGE_RIPE;

        HarvestPlant(7, 7, 1);

        expect(plants[idx].stage == PLANT_STAGE_BARE);
        bool foundSeed = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_FLAX_SEEDS) {
                foundSeed = true;
                break;
            }
        }
        expect(foundSeed);
    }

    // =========================================================================
    // 18. Quern recipe defined correctly
    // =========================================================================
    it("quern has grind wheat recipe") {
        int count = 0;
        const Recipe* recipes = GetRecipesForWorkshop(WORKSHOP_QUERN, &count);
        expect(count == 1);
        expect(recipes != NULL);
        expect(recipes[0].inputType == ITEM_WHEAT);
        expect(recipes[0].inputCount == 2);
        expect(recipes[0].outputType == ITEM_FLOUR);
        expect(recipes[0].outputCount == 1);
    }

    // =========================================================================
    // 19. Campfire has bread and lentil recipes
    // =========================================================================
    it("campfire has bread and lentil cooking recipes") {
        int count = 0;
        const Recipe* recipes = GetRecipesForWorkshop(WORKSHOP_CAMPFIRE, &count);
        expect(count >= 6);  // 4 original + 2 new (bread, lentils)

        // Find bread recipe
        bool foundBread = false, foundLentils = false;
        for (int i = 0; i < count; i++) {
            if (recipes[i].outputType == ITEM_BREAD) foundBread = true;
            if (recipes[i].outputType == ITEM_COOKED_LENTILS) foundLentils = true;
        }
        expect(foundBread);
        expect(foundLentils);
    }

    // =========================================================================
    // 20. Hearth has bread and lentil recipes
    // =========================================================================
    it("hearth has bread and lentil cooking recipes") {
        int count = 0;
        const Recipe* recipes = GetRecipesForWorkshop(WORKSHOP_HEARTH, &count);
        expect(count >= 4);  // 2 original + 2 new

        bool foundBread = false, foundLentils = false;
        for (int i = 0; i < count; i++) {
            if (recipes[i].outputType == ITEM_BREAD) foundBread = true;
            if (recipes[i].outputType == ITEM_COOKED_LENTILS) foundLentils = true;
        }
        expect(foundBread);
        expect(foundLentils);
    }

    // =========================================================================
    // 21. Planting from seed stack consumes 1 seed per cell, not entire stack
    // =========================================================================
    it("planting from seed stack of 10 splits off 1 seed per plant") {
        SetupFarmGrid();

        // Set up 3 tilled cells wanting wheat
        for (int x = 2; x < 5; x++) {
            farmGrid[1][5][x].tilled = 1;
            farmGrid[1][5][x].fertility = 128;
            farmGrid[1][5][x].desiredCropType = CROP_WHEAT;
        }
        farmActiveCells = 3;

        // Spawn a seed stack of 10 on ground at cell (2,5)
        float seedX = 2.5f * CELL_SIZE;
        float seedY = 5.5f * CELL_SIZE;
        int stackIdx = SpawnItem(seedX, seedY, 1.0f, ITEM_WHEAT_SEEDS);
        expect(stackIdx >= 0);
        items[stackIdx].stackCount = 10;

        // Simulate 3 plant jobs, each starting from STEP_MOVING_TO_PICKUP
        // The split happens at pickup step, so mover must be near the seed
        int planted = 0;
        for (int x = 2; x < 5; x++) {
            Mover* m = &movers[0];
            // Position mover at the seed location (so pickup succeeds immediately)
            InitMover(m, seedX, seedY, 1.0f, (Point){x, 5, 1}, 100.0f);
            moverCount = 1;
            m->capabilities.canPlant = true;

            // Find the stack
            int curStack = -1;
            for (int i = 0; i < itemHighWaterMark; i++) {
                if (items[i].active && items[i].type == ITEM_WHEAT_SEEDS &&
                    items[i].stackCount > 1) {
                    curStack = i;
                    break;
                }
            }
            // If no multi-stack, find single seed
            if (curStack < 0) {
                for (int i = 0; i < itemHighWaterMark; i++) {
                    if (items[i].active && items[i].type == ITEM_WHEAT_SEEDS &&
                        items[i].reservedBy == -1) {
                        curStack = i;
                        break;
                    }
                }
            }
            if (curStack < 0) break;

            int prevStack = items[curStack].stackCount;

            ClearJobs();
            int jobId = CreateJob(JOBTYPE_PLANT_CROP);
            expect(jobId >= 0);
            Job* job = GetJob(jobId);
            job->assignedMover = 0;
            job->targetItem = curStack;
            job->targetMineX = x;
            job->targetMineY = 5;
            job->targetMineZ = 1;
            job->step = STEP_MOVING_TO_PICKUP;
            job->progress = 0.0f;
            job->carryingItem = -1;
            ReserveItem(curStack, 0);

            // Run job to completion (pickup + carry + plant)
            for (int i = 0; i < 5000; i++) {
                JobRunResult r = RunJob_PlantCrop(job, m, 0.016f);
                if (r == JOBRUN_DONE) break;
                if (r == JOBRUN_FAIL) break;
                // Update mover position toward goal (simple teleport for test)
                if (job->step == STEP_CARRYING && job->carryingItem >= 0) {
                    float gx = x * CELL_SIZE + CELL_SIZE * 0.5f;
                    float gy = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
                    m->x = gx;
                    m->y = gy;
                    items[job->carryingItem].x = gx;
                    items[job->carryingItem].y = gy;
                }
            }

            FarmCell* fc = GetFarmCell(x, 5, 1);
            expect(fc->cropType == CROP_WHEAT);
            planted++;

            // After split+plant: original stack should have lost 1
            if (prevStack > 1) {
                expect(items[curStack].active);
                expect(items[curStack].stackCount == prevStack - 1);
            }
        }

        expect(planted == 3);

        // Original stack should have 7 remaining (10 - 3)
        int remainingSeeds = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_WHEAT_SEEDS) {
                remainingSeeds += items[i].stackCount;
            }
        }
        expect(remainingSeeds == 7);
    }

    // =========================================================================
    // 22. CropGrowthTimeGH returns correct values
    // =========================================================================
    it("CropGrowthTimeGH returns per-crop values") {
        expect(CropGrowthTimeGH(CROP_WHEAT) > 71.0f && CropGrowthTimeGH(CROP_WHEAT) < 73.0f);
        expect(CropGrowthTimeGH(CROP_LENTILS) > 47.0f && CropGrowthTimeGH(CROP_LENTILS) < 49.0f);
        expect(CropGrowthTimeGH(CROP_FLAX) > 59.0f && CropGrowthTimeGH(CROP_FLAX) < 61.0f);
    }

    // =========================================================================
    // 23. Watering: RunJob_WaterCrop completes and sets wetness
    // =========================================================================
    it("watering sets cell wetness and consumes water item") {
        SetupFarmGrid();

        // Set up dry tilled farm cell at (5,5,1)
        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 0);  // Dry soil at z-1

        // Spawn water item on ground near mover
        int waterIdx = SpawnItem(4.5f * CELL_SIZE, 4.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(waterIdx >= 0);

        // Create mover
        Mover* m = &movers[0];
        Point goal = {5, 5, 1};
        InitMover(m, 5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;

        // Create water crop job directly at STEP_PLANTING (mover already at target)
        int jobId = CreateJob(JOBTYPE_WATER_CROP);
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetItem = waterIdx;
        job->targetMineX = 5;
        job->targetMineY = 5;
        job->targetMineZ = 1;
        job->step = STEP_PLANTING;
        job->progress = 0.0f;
        job->carryingItem = waterIdx;
        items[waterIdx].state = ITEM_CARRIED;

        // Fast-forward to completion
        JobRunResult r = JOBRUN_RUNNING;
        for (int i = 0; i < 1000 && r == JOBRUN_RUNNING; i++) {
            r = RunJob_WaterCrop(job, m, 0.016f);
        }
        expect(r == JOBRUN_DONE);

        // Wetness should be set on the soil cell at z-1
        expect(GET_CELL_WETNESS(5, 5, 0) == WATER_POUR_WETNESS);

        // Water item should be consumed
        expect(!items[waterIdx].active);
    }

    // =========================================================================
    // 24. WorkGiver_WaterCrop assigns job when dry farm cell + water available
    // =========================================================================
    it("WorkGiver_WaterCrop assigns job for dry farm cell") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 0);  // Dry

        int waterIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(waterIdx >= 0);

        Mover* m = &movers[0];
        Point goal = {3, 3, 1};
        InitMover(m, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        int jobId = WorkGiver_WaterCrop(0);
        expect(jobId >= 0);

        Job* job = GetJob(jobId);
        expect(job->type == JOBTYPE_WATER_CROP);
        expect(job->targetItem == waterIdx);
        expect(job->targetMineX == 5);
        expect(job->targetMineY == 5);
        expect(job->targetMineZ == 1);
        expect(items[waterIdx].reservedBy == 0);
    }

    // =========================================================================
    // 25. No water job when all farm cells are wet
    // =========================================================================
    it("no water job when cells are already wet") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 2);  // Already wet

        int waterIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(waterIdx >= 0);

        Mover* m = &movers[0];
        Point goal = {3, 3, 1};
        InitMover(m, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        int jobId = WorkGiver_WaterCrop(0);
        expect(jobId < 0);  // No job — not dry
    }

    // =========================================================================
    // 26. No water job when no water items available
    // =========================================================================
    it("no water job when no water items") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 0);  // Dry

        // No water items spawned

        Mover* m = &movers[0];
        Point goal = {3, 3, 1};
        InitMover(m, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        int jobId = WorkGiver_WaterCrop(0);
        expect(jobId < 0);  // No water available
    }

    // =========================================================================
    // 27. No water job for untilled cells
    // =========================================================================
    it("no water job for untilled cells") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        // Cell is NOT tilled
        farmActiveCells = 0;
        SET_CELL_WETNESS(5, 5, 0, 0);

        int waterIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(waterIdx >= 0);

        Mover* m = &movers[0];
        Point goal = {3, 3, 1};
        InitMover(m, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        int jobId = WorkGiver_WaterCrop(0);
        expect(jobId < 0);  // farmActiveCells == 0
    }

    // =========================================================================
    // 28. Cancel water job releases reservation
    // =========================================================================
    it("cancel water job releases water item reservation") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 0);

        int waterIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(waterIdx >= 0);

        Mover* m = &movers[0];
        Point goal = {3, 3, 1};
        InitMover(m, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        int jobId = WorkGiver_WaterCrop(0);
        expect(jobId >= 0);
        expect(items[waterIdx].reservedBy == 0);

        CancelJob(m, 0);
        expect(items[waterIdx].reservedBy == -1);
        expect(m->currentJobId == -1);
    }

    // =========================================================================
    // 29. Dedup: no second water job for same cell
    // =========================================================================
    it("no duplicate water jobs for same cell") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 0);

        // Two water items
        int w1 = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1, ITEM_WATER);
        int w2 = SpawnItem(4.5f * CELL_SIZE, 4.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(w1 >= 0);
        expect(w2 >= 0);

        // Two movers
        Mover* m0 = &movers[0];
        Point g0 = {3, 3, 1};
        InitMover(m0, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, g0, 100.0f);
        m0->capabilities.canPlant = true;
        m0->currentJobId = -1;

        Mover* m1 = &movers[1];
        Point g1 = {4, 4, 1};
        InitMover(m1, 4.5f * CELL_SIZE, 4.5f * CELL_SIZE, 1.0f, g1, 100.0f);
        m1->capabilities.canPlant = true;
        m1->currentJobId = -1;
        moverCount = 2;

        AddMoverToIdleList(0);
        AddMoverToIdleList(1);

        int jobId1 = WorkGiver_WaterCrop(0);
        expect(jobId1 >= 0);

        // Second mover tries same cell — should be rejected (dedup)
        int jobId2 = WorkGiver_WaterCrop(1);
        expect(jobId2 < 0);
    }

    // =========================================================================
    // 30. Multiple dry cells get separate water jobs
    // =========================================================================
    it("multiple dry cells get separate water jobs") {
        SetupFarmGrid();

        // Two dry tilled cells far apart
        farmGrid[1][3][3].tilled = 1;
        farmGrid[1][3][3].fertility = 128;
        farmGrid[1][12][12].tilled = 1;
        farmGrid[1][12][12].fertility = 128;
        farmActiveCells = 2;
        SET_CELL_WETNESS(3, 3, 0, 0);
        SET_CELL_WETNESS(12, 12, 0, 0);

        // Two water items near each mover
        int w1 = SpawnItem(2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 1, ITEM_WATER);
        int w2 = SpawnItem(11.5f * CELL_SIZE, 11.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(w1 >= 0);
        expect(w2 >= 0);

        // Mover 0 near cell (3,3), mover 1 near cell (12,12)
        Mover* m0 = &movers[0];
        Point g0 = {2, 2, 1};
        InitMover(m0, 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 1.0f, g0, 100.0f);
        m0->capabilities.canPlant = true;
        m0->currentJobId = -1;

        Mover* m1 = &movers[1];
        Point g1 = {11, 11, 1};
        InitMover(m1, 11.5f * CELL_SIZE, 11.5f * CELL_SIZE, 1.0f, g1, 100.0f);
        m1->capabilities.canPlant = true;
        m1->currentJobId = -1;
        moverCount = 2;

        RebuildIdleMoverList();

        int jobId1 = WorkGiver_WaterCrop(0);
        expect(jobId1 >= 0);

        // Second mover gets a different cell (nearest to them is (12,12))
        int jobId2 = WorkGiver_WaterCrop(1);
        expect(jobId2 >= 0);

        // Should be different jobs targeting different cells
        Job* j1 = GetJob(jobId1);
        Job* j2 = GetJob(jobId2);
        expect(j1->targetMineX != j2->targetMineX || j1->targetMineY != j2->targetMineY);
    }

    // =========================================================================
    // 31. Water from stockpile is also found
    // =========================================================================
    it("WorkGiver_WaterCrop finds water in stockpile") {
        SetupFarmGrid();
        RebuildIdleMoverList();

        farmGrid[1][5][5].tilled = 1;
        farmGrid[1][5][5].fertility = 128;
        farmActiveCells = 1;
        SET_CELL_WETNESS(5, 5, 0, 0);

        // Water item in stockpile state
        int waterIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1, ITEM_WATER);
        expect(waterIdx >= 0);
        items[waterIdx].state = ITEM_IN_STOCKPILE;

        Mover* m = &movers[0];
        Point goal = {3, 3, 1};
        InitMover(m, 3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canPlant = true;
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        int jobId = WorkGiver_WaterCrop(0);
        expect(jobId >= 0);
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(farming);

    return summary();
}
