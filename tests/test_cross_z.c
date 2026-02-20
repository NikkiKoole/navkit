#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/designations.h"
#include "../src/world/construction.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/stacking.h"
#include "../src/entities/containers.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/jobs.h"
#include "../src/entities/workshops.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

static bool AcceptAll(int itemIdx, void* userData) {
    (void)userData;
    (void)itemIdx;
    return true;
}

// Build a hillside grid:
//   z=0: all CELL_WALL (bedrock)
//   z=1: x=0..4 CELL_WALL, x=5 ramp, x=6..9 CELL_WALL
//   z=2: x=0..4 CELL_AIR (walkable), x=5 CELL_RAMP_E, x=6..9 CELL_WALL
//   z=3: x=0..5 CELL_AIR, x=6..9 CELL_AIR (walkable on z=2 walls)
//
// Walkable: z=2 left (x=0..4), z=3 right (x=6..9), ramp at x=5 connects.
static void SetupHillsideGrid(void) {
    InitGridWithSizeAndChunkSize(10, 5, 10, 5);
    InitDesignations();

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 10; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_GRANITE);

            if (x < 5) {
                grid[1][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 1, MAT_DIRT);
                grid[2][y][x] = CELL_AIR;
                grid[3][y][x] = CELL_AIR;
            } else if (x == 5) {
                grid[1][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 1, MAT_DIRT);
                grid[2][y][x] = CELL_RAMP_E;
                grid[3][y][x] = CELL_AIR;
            } else {
                grid[1][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 1, MAT_DIRT);
                grid[2][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 2, MAT_DIRT);
                grid[3][y][x] = CELL_AIR;
            }
        }
    }
}

static void SetupSystems(void) {
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearJobs();
    ClearWorkshops();
    InitJobSystem(MAX_MOVERS);
}

// Create idle mover on left side at z=2
static int SetupMoverLeft(int tileX, int tileY) {
    Mover* m = &movers[0];
    float px = tileX * CELL_SIZE + CELL_SIZE * 0.5f;
    float py = tileY * CELL_SIZE + CELL_SIZE * 0.5f;
    Point goal = { tileX, tileY, 2 };
    InitMover(m, px, py, 2.0f, goal, 100.0f);
    m->capabilities.canMine = true;
    m->capabilities.canHaul = true;
    m->capabilities.canBuild = true;
    m->capabilities.canPlant = true;
    moverCount = 1;
    AddMoverToIdleList(0);
    return 0;
}

// ===========================================================================
// FindFirstItemInRadius across z-levels
// ===========================================================================
describe(cross_z_find_item_in_radius) {
    it("finds item at z+1 via spatial grid search") {
        SetupHillsideGrid();
        SetupSystems();

        InitItemSpatialGrid(10, 5, gridDepth);

        int itemIdx = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                3.0f, ITEM_RED);
        BuildItemSpatialGrid();

        int found = FindFirstItemInRadius(2, 2, 2, 50, AcceptAll, NULL);
        expect(found == itemIdx);
    }

    it("finds item at z-1 via spatial grid search") {
        SetupHillsideGrid();
        SetupSystems();

        InitItemSpatialGrid(10, 5, gridDepth);

        int itemIdx = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2.0f, ITEM_RED);
        BuildItemSpatialGrid();

        int found = FindFirstItemInRadius(7, 2, 3, 50, AcceptAll, NULL);
        expect(found == itemIdx);
    }
}

// ===========================================================================
// FindItemInContainers across z-levels
// ===========================================================================
describe(cross_z_find_item_in_container) {
    it("finds item in container at z+1") {
        SetupHillsideGrid();
        SetupSystems();

        // Temporarily make ITEM_RED a container
        containerDefs[ITEM_RED].maxContents = 15;
        containerDefs[ITEM_RED].spoilageModifier = 1.0f;

        int containerIdx = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                                     2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                     3.0f, ITEM_RED);
        int contentIdx = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                                   2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                   3.0f, ITEM_BLUE);

        PutItemInContainer(contentIdx, containerIdx);

        int outContainer = -1;
        int found = FindItemInContainers(ITEM_BLUE, 2, 2, 2, 50, -1, NULL, NULL, &outContainer);
        expect(found == contentIdx);
        expect(outContainer == containerIdx);

        containerDefs[ITEM_RED].maxContents = 0;
    }
}

// ===========================================================================
// Haul across z-levels
// ===========================================================================
describe(cross_z_haul_item) {
    it("hauls item from z=2 to stockpile at z=3") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                  2 * CELL_SIZE + CELL_SIZE * 0.5f,
                  2.0f, ITEM_RED);

        int spIdx = CreateStockpile(7, 2, 3, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);

        int jobId = WorkGiver_Haul(0);
        expect(jobId >= 0);
    }

    it("hauls item from z=3 to stockpile at z=2") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                  2 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3.0f, ITEM_RED);

        int spIdx = CreateStockpile(2, 3, 2, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);

        int jobId = WorkGiver_Haul(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
// Mining across z-levels
// ===========================================================================
describe(cross_z_mine_designation) {
    it("assigns mine job across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        // Mine the wall on the right side at z=2
        DesignateMine(6, 2, 2);
        RebuildMineDesignationCache();

        int jobId = WorkGiver_Mining(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
// Gather Grass across z-levels (uses AssignJobs to rebuild cache)
// ===========================================================================
describe(cross_z_gather_grass) {
    it("assigns gather grass job across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        SetVegetation(7, 2, 3, VEG_GRASS_TALLER);
        DesignateGatherGrass(7, 2, 3);

        // AssignJobs rebuilds all designation caches internally
        AssignJobs();

        expect(movers[0].currentJobId >= 0);
    }
}

// ===========================================================================
// Plant Sapling across z-levels
// ===========================================================================
describe(cross_z_plant_sapling) {
    it("assigns plant sapling job across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        int sapIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                               2 * CELL_SIZE + CELL_SIZE * 0.5f,
                               2.0f, ITEM_SAPLING);
        items[sapIdx].material = MAT_OAK;

        DesignatePlantSapling(7, 2, 3);

        AssignJobs();

        expect(movers[0].currentJobId >= 0);
    }
}

// ===========================================================================
// Craft across z-levels
// ===========================================================================
describe(cross_z_workshop_craft) {
    it("assigns craft job across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        // Sawmill on right side at z=3
        int wsIdx = CreateWorkshop(7, 1, 3, WORKSHOP_SAWMILL);
        expect(wsIdx >= 0);
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);

        // Log near the workshop
        int logIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f,
                               2 * CELL_SIZE + CELL_SIZE * 0.5f,
                               3.0f, ITEM_LOG);
        items[logIdx].material = MAT_OAK;

        // Stockpile for output (planks)
        int spIdx = CreateStockpile(1, 1, 2, 2, 2);
        SetStockpileFilter(spIdx, ITEM_PLANKS, true);

        int jobId = WorkGiver_Craft(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
// Deliver to Passive Workshop across z-levels
// ===========================================================================
describe(cross_z_deliver_passive_workshop) {
    it("assigns delivery across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        int wsIdx = CreateWorkshop(7, 1, 3, WORKSHOP_CHARCOAL_PIT);
        expect(wsIdx >= 0);
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);

        Workshop* ws = &workshops[wsIdx];
        const Recipe* recipe = &workshopDefs[ws->type].recipes[0];

        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2.0f, recipe->inputType);
        if (recipe->inputType == ITEM_LOG) {
            items[itemIdx].material = MAT_OAK;
        }

        int jobId = WorkGiver_DeliverToPassiveWorkshop(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
// Build Blueprint across z-levels
// ===========================================================================
describe(cross_z_build_blueprint) {
    it("assigns build job across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        int bpIdx = CreateRecipeBlueprint(7, 2, 3, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);

        // Force to ready-to-build state
        blueprints[bpIdx].state = BLUEPRINT_READY_TO_BUILD;

        int jobId = WorkGiver_Build(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
// Blueprint Haul across z-levels
// ===========================================================================
describe(cross_z_blueprint_haul) {
    it("assigns blueprint haul across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        int bpIdx = CreateRecipeBlueprint(7, 2, 3, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);

        // Spawn rocks on left side at z=2
        for (int i = 0; i < 3; i++) {
            int idx = SpawnItem((2 + i) * CELL_SIZE + CELL_SIZE * 0.5f,
                                3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2.0f, ITEM_ROCK);
            items[idx].material = MAT_GRANITE;
        }

        int jobId = WorkGiver_BlueprintHaul(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
// Ignite Workshop across z-levels
// ===========================================================================
describe(cross_z_ignite_workshop) {
    it("assigns ignite job across z-levels") {
        SetupHillsideGrid();
        SetupSystems();
        SetupMoverLeft(2, 2);

        int wsIdx = CreateWorkshop(7, 1, 3, WORKSHOP_CHARCOAL_PIT);
        expect(wsIdx >= 0);
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);

        Workshop* ws = &workshops[wsIdx];
        const Recipe* recipe = &workshopDefs[ws->type].recipes[0];

        // Place input on work tile (already delivered)
        int inputIdx = SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                                 ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                                 (float)ws->z, recipe->inputType);
        if (recipe->inputType == ITEM_LOG) {
            items[inputIdx].material = MAT_OAK;
        }

        ws->passiveReady = false;

        int jobId = WorkGiver_IgniteWorkshop(0);
        expect(jobId >= 0);
    }
}

// ===========================================================================
int main(int argc, char* argv[]) {
    bool verbose = false;
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    test_verbose = verbose;
    if (!verbose) {
        if (quiet) {
            set_quiet_mode(1);
        }
        SetTraceLogLevel(LOG_NONE);
    }

    test(cross_z_find_item_in_radius);
    test(cross_z_find_item_in_container);
    test(cross_z_haul_item);
    test(cross_z_mine_designation);
    test(cross_z_gather_grass);
    test(cross_z_plant_sapling);
    test(cross_z_workshop_craft);
    test(cross_z_deliver_passive_workshop);
    test(cross_z_build_blueprint);
    test(cross_z_blueprint_haul);
    test(cross_z_ignite_workshop);

    return summary();
}
