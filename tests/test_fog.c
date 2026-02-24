#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/animals.h"
#include "../src/entities/tool_quality.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/world/designations.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/game_state.h"
#include "test_helpers.h"
#include <string.h>

// Helper: set up a basic flat walkable grid and clear all state
static void SetupFogTestGrid(void) {
    InitTestGridFromAscii(
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n");
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers();
    ClearItems();
    ClearJobs();
    ClearAnimals();
    ClearStockpiles();
    InitDesignations();
    InitBalance();
    toolRequirementsEnabled = false;
}

// Helper: set up an animal manually at a specific position
static int SetupAnimalForFog(float x, float y, int z, AnimalType type) {
    if (animalCount >= MAX_ANIMALS) return -1;
    int idx = animalCount++;
    Animal* a = &animals[idx];
    memset(a, 0, sizeof(Animal));
    a->x = x;
    a->y = y;
    a->z = (float)z;
    a->type = type;
    a->state = ANIMAL_IDLE;
    a->behavior = BEHAVIOR_SIMPLE_GRAZER;
    a->active = true;
    a->speed = ANIMAL_SPEED;
    a->targetAnimalIdx = -1;
    a->markedForHunt = false;
    a->reservedByHunter = -1;
    return idx;
}

// Helper: create a mover with standard capabilities at given cell
static void SetupMoverAt(int cellX, int cellY, int z) {
    Point goal = {cellX, cellY, z};
    InitMover(&movers[0], (cellX + 0.5f) * CELL_SIZE, (cellY + 0.5f) * CELL_SIZE, (float)z, goal, MOVER_SPEED);
    movers[0].capabilities.canHaul = true;
    movers[0].capabilities.canHunt = true;
    movers[0].capabilities.canMine = true;
    movers[0].capabilities.canPlant = true;
    moverCount = 1;
}

// ===========================================================================
// IsExplored basic behavior tests
// ===========================================================================
describe(fog_explored) {
    it("sandbox mode always returns explored") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SANDBOX;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        expect(IsExplored(5, 5, 0));

        gameMode = GAME_MODE_SANDBOX;
    }

    it("survival mode respects exploredGrid") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        expect(!IsExplored(5, 5, 0));

        SetExplored(5, 5, 0);
        expect(IsExplored(5, 5, 0));

        gameMode = GAME_MODE_SANDBOX;
    }

    it("out of bounds returns explored for safety") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;

        expect(IsExplored(-1, 0, 0));
        expect(IsExplored(0, -1, 0));
        expect(IsExplored(gridWidth, 0, 0));

        gameMode = GAME_MODE_SANDBOX;
    }
}

// ===========================================================================
// WorkGiver_Hunt fog of war tests
// ===========================================================================
describe(fog_hunt) {
    it("hunt skips animals in unexplored cells") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        // Explore mover area but NOT animal area
        SetExplored(2, 2, 0);

        SetupMoverAt(2, 2, 0);

        int animalIdx = SetupAnimalForFog(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId == -1);

        gameMode = GAME_MODE_SANDBOX;
    }

    it("hunt finds animals in explored cells") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        // Explore everything
        for (int x = 0; x < 10; x++)
            for (int y = 0; y < 10; y++)
                SetExplored(x, y, 0);

        SetupMoverAt(2, 2, 0);

        int animalIdx = SetupAnimalForFog(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId >= 0);

        gameMode = GAME_MODE_SANDBOX;
    }
}

// ===========================================================================
// Mine designation cache fog of war tests
// ===========================================================================
describe(fog_mine) {
    it("mine designation in unexplored area not in cache") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        // Place a wall at (7,5,0)
        grid[0][5][7] = CELL_WALL;
        cellFlags[0][5][7] |= (CF_WALL | CF_SOLID);
        SetWallMaterial(7, 5, 0, MAT_GRANITE);

        // Create mine designation
        designations[0][5][7].type = DESIGNATION_MINE;
        designations[0][5][7].assignedMover = -1;
        designations[0][5][7].unreachableCooldown = 0.0f;
        activeDesignationCount = 1;

        SetupMoverAt(2, 2, 0);

        InitJobSystem(MAX_MOVERS);
        InvalidateDesignationCache(DESIGNATION_MINE);
        RebuildMineDesignationCache();
        RebuildIdleMoverList();
        AssignJobs();

        // Mover should NOT get a mining job — designation is in unexplored area
        expect(movers[0].currentJobId == -1);

        gameMode = GAME_MODE_SANDBOX;
    }

    it("mine designation in explored area gets assigned") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        // Explore relevant areas
        for (int x = 0; x < 10; x++)
            for (int y = 0; y < 10; y++)
                SetExplored(x, y, 0);

        // Place a wall at (7,5,0)
        grid[0][5][7] = CELL_WALL;
        cellFlags[0][5][7] |= (CF_WALL | CF_SOLID);
        SetWallMaterial(7, 5, 0, MAT_GRANITE);

        // Create mine designation
        designations[0][5][7].type = DESIGNATION_MINE;
        designations[0][5][7].assignedMover = -1;
        designations[0][5][7].unreachableCooldown = 0.0f;
        activeDesignationCount = 1;

        SetupMoverAt(2, 2, 0);

        InitJobSystem(MAX_MOVERS);
        InvalidateDesignationCache(DESIGNATION_MINE);
        RebuildMineDesignationCache();
        RebuildIdleMoverList();
        AssignJobs();

        // Mover SHOULD get a mining job
        expect(movers[0].currentJobId >= 0);

        gameMode = GAME_MODE_SANDBOX;
    }
}

// ===========================================================================
// Explore designation exception tests
// ===========================================================================
describe(fog_explore_exception) {
    it("explore designation works in survival mode on unexplored cells") {
        SetupFogTestGrid();
        gameMode = GAME_MODE_SURVIVAL;
        memset(exploredGrid, 0, sizeof(exploredGrid));

        // Explore mover area
        SetExplored(2, 2, 0);

        // Create explore designation at unexplored (5,5,0)
        designations[0][5][5].type = DESIGNATION_EXPLORE;
        designations[0][5][5].assignedMover = -1;
        designations[0][5][5].unreachableCooldown = 0.0f;
        activeDesignationCount = 1;

        SetupMoverAt(2, 2, 0);

        InitJobSystem(MAX_MOVERS);
        InvalidateDesignationCache(DESIGNATION_EXPLORE);
        RebuildIdleMoverList();
        AssignJobs();

        // Mover SHOULD get an explore job — explore intentionally targets unexplored
        expect(movers[0].currentJobId >= 0);

        gameMode = GAME_MODE_SANDBOX;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(fog_explored);
    test(fog_hunt);
    test(fog_mine);
    test(fog_explore_exception);

    return summary();
}
