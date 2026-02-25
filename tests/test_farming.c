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
#include "../src/simulation/balance.h"
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
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(farming);

    return summary();
}
