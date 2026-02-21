#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/tool_quality.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/world/designations.h"
#include "../src/simulation/trees.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/game_state.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// ===========================================================================
// GetItemQualityLevel tests
// ===========================================================================
describe(quality_lookup) {
    it("should return hammer:1 for ITEM_ROCK") {
        expect(GetItemQualityLevel(ITEM_ROCK, QUALITY_HAMMERING) == 1);
    }

    it("should return 0 for ITEM_ROCK cutting quality") {
        expect(GetItemQualityLevel(ITEM_ROCK, QUALITY_CUTTING) == 0);
    }

    it("should return 0 for ITEM_ROCK digging quality") {
        expect(GetItemQualityLevel(ITEM_ROCK, QUALITY_DIGGING) == 0);
    }

    it("should return cutting:1 for ITEM_SHARP_STONE") {
        expect(GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_CUTTING) == 1);
    }

    it("should return fine:1 for ITEM_SHARP_STONE") {
        expect(GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_FINE) == 1);
    }

    it("should return 0 for ITEM_SHARP_STONE hammering quality") {
        expect(GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_HAMMERING) == 0);
    }

    it("should return 0 for non-tool items") {
        expect(GetItemQualityLevel(ITEM_STICKS, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_STICKS, QUALITY_HAMMERING) == 0);
        expect(GetItemQualityLevel(ITEM_STICKS, QUALITY_DIGGING) == 0);
        expect(GetItemQualityLevel(ITEM_LOG, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_PLANKS, QUALITY_SAWING) == 0);
    }

    it("should return 0 for invalid item types") {
        expect(GetItemQualityLevel(-1, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_TYPE_COUNT, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(999, QUALITY_CUTTING) == 0);
    }
}

// ===========================================================================
// ItemHasAnyQuality tests
// ===========================================================================
describe(has_any_quality) {
    it("should return true for ITEM_ROCK") {
        expect(ItemHasAnyQuality(ITEM_ROCK) == true);
    }

    it("should return true for ITEM_SHARP_STONE") {
        expect(ItemHasAnyQuality(ITEM_SHARP_STONE) == true);
    }

    it("should return false for non-tool items") {
        expect(ItemHasAnyQuality(ITEM_STICKS) == false);
        expect(ItemHasAnyQuality(ITEM_LOG) == false);
        expect(ItemHasAnyQuality(ITEM_PLANKS) == false);
        expect(ItemHasAnyQuality(ITEM_CORDAGE) == false);
    }

    it("should return false for invalid types") {
        expect(ItemHasAnyQuality(-1) == false);
        expect(ItemHasAnyQuality(ITEM_TYPE_COUNT) == false);
    }
}

// ===========================================================================
// IF_TOOL flag tests
// ===========================================================================
describe(tool_flag) {
    it("should have IF_TOOL set on ITEM_ROCK") {
        expect(ItemIsTool(ITEM_ROCK) != 0);
    }

    it("should have IF_TOOL set on ITEM_SHARP_STONE") {
        expect(ItemIsTool(ITEM_SHARP_STONE) != 0);
    }

    it("should NOT have IF_TOOL on regular items") {
        expect(ItemIsTool(ITEM_STICKS) == 0);
        expect(ItemIsTool(ITEM_LOG) == 0);
        expect(ItemIsTool(ITEM_PLANKS) == 0);
        expect(ItemIsTool(ITEM_CORDAGE) == 0);
        expect(ItemIsTool(ITEM_DIRT) == 0);
    }
}

// ===========================================================================
// GetToolSpeedMultiplier tests - soft jobs
// ===========================================================================
describe(speed_multiplier_soft) {
    // Soft jobs: bare hands (level 0) = 0.5x, level 1 = 1.0x, level 2 = 1.5x, level 3 = 2.0x

    it("should return 0.5x for bare hands on soft job") {
        float mult = GetToolSpeedMultiplier(0, 0, true);
        expect(mult > 0.49f && mult < 0.51f);
    }

    it("should return 1.0x for level 1 tool on soft job") {
        float mult = GetToolSpeedMultiplier(1, 0, true);
        expect(mult > 0.99f && mult < 1.01f);
    }

    it("should return 1.5x for level 2 tool on soft job") {
        float mult = GetToolSpeedMultiplier(2, 0, true);
        expect(mult > 1.49f && mult < 1.51f);
    }

    it("should return 2.0x for level 3 tool on soft job") {
        float mult = GetToolSpeedMultiplier(3, 0, true);
        expect(mult > 1.99f && mult < 2.01f);
    }
}

// ===========================================================================
// GetToolSpeedMultiplier tests - hard-gated jobs
// ===========================================================================
describe(speed_multiplier_hard) {
    // Hard gate: below min = 0.0 (can't do), at min = 1.0x, above = +0.5x per level

    it("should return 0.0 when tool level below minimum") {
        // Rock mining needs hammer:2, bare hands = 0
        float mult = GetToolSpeedMultiplier(0, 2, false);
        expect(mult < 0.01f);
    }

    it("should return 0.0 when tool level 1 below minimum of 2") {
        // Rock (hammer:1) can't mine rock (needs hammer:2)
        float mult = GetToolSpeedMultiplier(1, 2, false);
        expect(mult < 0.01f);
    }

    it("should return 1.0x when tool meets minimum exactly") {
        // Stone hammer (hammer:2) mines rock at baseline
        float mult = GetToolSpeedMultiplier(2, 2, false);
        expect(mult > 0.99f && mult < 1.01f);
    }

    it("should return 1.5x when tool exceeds minimum by 1") {
        float mult = GetToolSpeedMultiplier(3, 2, false);
        expect(mult > 1.49f && mult < 1.51f);
    }

    it("should return 1.0x for level 1 tool on hard gate min 1") {
        // Sharp stone (cutting:1) can chop trees (needs cutting:1)
        float mult = GetToolSpeedMultiplier(1, 1, false);
        expect(mult > 0.99f && mult < 1.01f);
    }

    it("should return 0.0 for bare hands on hard gate min 1") {
        // Can't chop trees bare-handed
        float mult = GetToolSpeedMultiplier(0, 1, false);
        expect(mult < 0.01f);
    }

    it("should return 1.5x for level 2 tool on hard gate min 1") {
        // Stone axe (cutting:2) chops trees at 1.5x
        float mult = GetToolSpeedMultiplier(2, 1, false);
        expect(mult > 1.49f && mult < 1.51f);
    }
}

// ===========================================================================
// Mover equippedTool initialization tests
// ===========================================================================
describe(mover_equipped_tool) {
    it("should initialize equippedTool to -1") {
        InitTestGrid(8, 8);
        ClearMovers();
        Point goal = {4*CELL_SIZE, 4*CELL_SIZE, 0};
        InitMover(&movers[0], 1*CELL_SIZE, 1*CELL_SIZE, 0, goal, MOVER_SPEED);
        moverCount = 1;

        expect(movers[0].equippedTool == -1);
    }
}

// ===========================================================================
// Scenario tests: tool quality lookups for game scenarios
// ===========================================================================
describe(game_scenarios) {
    it("rock provides hammer:1 — helps building but not rock mining") {
        // Rock has hammering:1
        int rockHammer = GetItemQualityLevel(ITEM_ROCK, QUALITY_HAMMERING);
        expect(rockHammer == 1);

        // Building is soft (minLevel=0): rock at hammer:1 → 1.0x
        float buildSpeed = GetToolSpeedMultiplier(rockHammer, 0, true);
        expect(buildSpeed > 0.99f && buildSpeed < 1.01f);

        // Rock mining is hard (minLevel=2): rock at hammer:1 → 0.0 (can't do)
        float mineSpeed = GetToolSpeedMultiplier(rockHammer, 2, false);
        expect(mineSpeed < 0.01f);
    }

    it("sharp stone provides cutting:1 — can chop trees") {
        int ssCutting = GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_CUTTING);
        expect(ssCutting == 1);

        // Chopping is hard (minLevel=1): sharp stone at cutting:1 → 1.0x
        float chopSpeed = GetToolSpeedMultiplier(ssCutting, 1, false);
        expect(chopSpeed > 0.99f && chopSpeed < 1.01f);
    }

    it("bare hands can dig soil at 0.5x but not mine rock") {
        // Soil digging is soft (minLevel=0)
        float soilSpeed = GetToolSpeedMultiplier(0, 0, true);
        expect(soilSpeed > 0.49f && soilSpeed < 0.51f);

        // Rock mining is hard (minLevel=2)
        float rockSpeed = GetToolSpeedMultiplier(0, 2, false);
        expect(rockSpeed < 0.01f);
    }

    it("effective work time halves when speed doubles") {
        float baseTime = 10.0f;
        // Bare hands on soft job: 0.5x → effective time = 10 / 0.5 = 20
        float bareTime = baseTime / GetToolSpeedMultiplier(0, 0, true);
        expect(bareTime > 19.9f && bareTime < 20.1f);

        // Level 1 tool on soft job: 1.0x → effective time = 10 / 1.0 = 10
        float toolTime = baseTime / GetToolSpeedMultiplier(1, 0, true);
        expect(toolTime > 9.9f && toolTime < 10.1f);

        // Level 3 tool on soft job: 2.0x → effective time = 10 / 2.0 = 5
        float fastTime = baseTime / GetToolSpeedMultiplier(3, 0, true);
        expect(fastTime > 4.9f && fastTime < 5.1f);
    }
}

// ===========================================================================
// Job-to-quality mapping tests
// ===========================================================================
describe(job_tool_requirement) {
    it("should require DIGGING for mining dirt") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_MINE, MAT_DIRT);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_DIGGING);
        expect(req.isSoft == true);
        expect(req.minLevel == 0);
    }

    it("should require HAMMERING:2 for mining stone") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_MINE, MAT_GRANITE);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_HAMMERING);
        expect(req.isSoft == false);
        expect(req.minLevel == 2);
    }

    it("should require CUTTING:1 for chopping") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHOP, MAT_NONE);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_CUTTING);
        expect(req.isSoft == false);
        expect(req.minLevel == 1);
    }

    it("should require CUTTING:1 for chopping felled") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHOP_FELLED, MAT_NONE);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_CUTTING);
        expect(req.isSoft == false);
        expect(req.minLevel == 1);
    }

    it("should require soft HAMMERING for building") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_BUILD, MAT_NONE);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_HAMMERING);
        expect(req.isSoft == true);
        expect(req.minLevel == 0);
    }

    it("should have no requirement for hauling") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_HAUL, MAT_NONE);
        expect(req.hasRequirement == false);
    }

    it("should have no requirement for knapping") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_KNAP, MAT_NONE);
        expect(req.hasRequirement == false);
    }

    it("should require DIGGING for channeling soil") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHANNEL, MAT_CLAY);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_DIGGING);
        expect(req.isSoft == true);
    }

    it("should require HAMMERING:2 for channeling stone") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHANNEL, MAT_GRANITE);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_HAMMERING);
        expect(req.isSoft == false);
        expect(req.minLevel == 2);
    }

    it("should require DIGGING for ramp carving in dirt") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_DIG_RAMP, MAT_DIRT);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_DIGGING);
        expect(req.isSoft == true);
    }

    it("should require HAMMERING:2 for ramp carving in stone") {
        JobToolReq req = GetJobToolRequirement(JOBTYPE_DIG_RAMP, MAT_SANDSTONE);
        expect(req.hasRequirement == true);
        expect(req.qualityType == QUALITY_HAMMERING);
        expect(req.isSoft == false);
        expect(req.minLevel == 2);
    }
}

// ===========================================================================
// CanMoverDoJob tests
// ===========================================================================
describe(can_mover_do_job) {
    it("should allow anyone to do tool-free jobs") {
        toolRequirementsEnabled = true;
        expect(CanMoverDoJob(JOBTYPE_HAUL, MAT_NONE, -1) == true);
        expect(CanMoverDoJob(JOBTYPE_KNAP, MAT_NONE, -1) == true);
        expect(CanMoverDoJob(JOBTYPE_CLEAN, MAT_NONE, -1) == true);
    }

    it("should allow anyone to do soft jobs (even bare-handed)") {
        toolRequirementsEnabled = true;
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_DIRT, -1) == true);
        expect(CanMoverDoJob(JOBTYPE_BUILD, MAT_NONE, -1) == true);
    }

    it("should block hard-gated jobs without the right tool") {
        toolRequirementsEnabled = true;
        // No tool → can't chop
        expect(CanMoverDoJob(JOBTYPE_CHOP, MAT_NONE, -1) == false);
        // No tool → can't mine stone
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, -1) == false);
    }

    it("should allow hard-gated jobs with the right tool") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();

        // Spawn a sharp stone (cutting:1)
        int ssIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        expect(ssIdx >= 0);
        expect(CanMoverDoJob(JOBTYPE_CHOP, MAT_NONE, ssIdx) == true);

        // Sharp stone has no hammering → can't mine stone
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, ssIdx) == false);
    }

    it("should allow rock mining with rock that only has hammer:1 — NOT enough for stone") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();

        int rockIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_ROCK);
        expect(rockIdx >= 0);
        // Rock has hammer:1, stone mining needs hammer:2 → blocked
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, rockIdx) == false);
        // Rock has hammer:1, building is soft → allowed
        expect(CanMoverDoJob(JOBTYPE_BUILD, MAT_NONE, rockIdx) == true);
    }

    it("should bypass all gates when toolRequirementsEnabled is false") {
        toolRequirementsEnabled = false;
        expect(CanMoverDoJob(JOBTYPE_CHOP, MAT_NONE, -1) == true);
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, -1) == true);
        expect(CanMoverDoJob(JOBTYPE_CHOP_FELLED, MAT_NONE, -1) == true);
    }
}

// ===========================================================================
// GetJobToolSpeedMultiplier tests
// ===========================================================================
describe(job_tool_speed) {
    it("should return 0.5x for bare-handed soft dirt mining") {
        toolRequirementsEnabled = true;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_DIRT, -1);
        expect(speed > 0.49f && speed < 0.51f);
    }

    it("should return 1.0x for tool-free jobs regardless of tool") {
        toolRequirementsEnabled = true;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_HAUL, MAT_NONE, -1);
        expect(speed > 0.99f && speed < 1.01f);
    }

    it("should return 0.0 for hard-gated stone mining without tool") {
        toolRequirementsEnabled = true;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_GRANITE, -1);
        expect(speed < 0.01f);
    }

    it("should return 1.0x when toggle is off") {
        toolRequirementsEnabled = false;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_GRANITE, -1);
        expect(speed > 0.99f && speed < 1.01f);
    }

    it("should return 1.0x for sharp stone chopping trees") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int ssIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_CHOP, MAT_NONE, ssIdx);
        expect(speed > 0.99f && speed < 1.01f);
    }

    it("should return 1.0x for rock soft-building") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int rockIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_ROCK);
        // Rock has hammer:1, building is soft (minLevel=0): level 1 → 1.0x
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_BUILD, MAT_NONE, rockIdx);
        expect(speed > 0.99f && speed < 1.01f);
    }
}

// ===========================================================================
// E2E Story tests: tool quality affecting actual job execution
// ===========================================================================

// Helper: set up a flat grid with one wall, a mover, and designations
static void SetupMiningTest(int wallX, int wallY, MaterialType wallMat, int moverX, int moverY) {
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

    // Place a wall at the target position
    grid[0][wallY][wallX] = CELL_WALL;
    SetWallMaterial(wallX, wallY, 0, wallMat);
    SetWallNatural(wallX, wallY, 0);

    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearJobs();
    InitDesignations();

    // Create mover
    Point goal = {moverX, moverY, 0};
    InitMover(&movers[0], moverX * CELL_SIZE + CELL_SIZE * 0.5f,
              moverY * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
    moverCount = 1;
}

// Run simulation ticks until condition is met or max ticks reached.
// Returns the number of ticks run.
static int RunSimUntil(bool (*condition)(void), int maxTicks) {
    for (int i = 0; i < maxTicks; i++) {
        Tick();
        AssignJobs();
        JobsTick();
        if (condition()) return i + 1;
    }
    return maxTicks;
}

static int g_wallCheckX, g_wallCheckY;
static bool WallIsGone(void) {
    return grid[0][g_wallCheckY][g_wallCheckX] != CELL_WALL;
}

describe(story1_bare_hands_soil_mining) {
    it("mover digs dirt bare-handed at 0.5x speed (slower than with tool)") {
        toolRequirementsEnabled = true;
        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);  // dirt wall at (5,3), mover at (4,3) adjacent

        // Confirm wall is there
        expect(grid[0][3][5] == CELL_WALL);
        expect(GetWallMaterial(5, 3, 0) == MAT_DIRT);

        // Mover has no tool
        expect(movers[0].equippedTool == -1);

        // Designate mine
        bool desig = DesignateMine(5, 3, 0);
        expect(desig == true);

        // Run sim until wall is gone
        g_wallCheckX = 5; g_wallCheckY = 3;
        int ticksBareHands = RunSimUntil(WallIsGone, 2000);
        expect(grid[0][3][5] != CELL_WALL);  // Wall should be mined

        // Now do the same test with toolRequirementsEnabled=false (baseline speed)
        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);
        toolRequirementsEnabled = false;
        DesignateMine(5, 3, 0);
        int ticksBaseline = RunSimUntil(WallIsGone, 2000);
        expect(grid[0][3][5] != CELL_WALL);

        // Bare hands should take roughly 2x baseline ticks
        // Allow some tolerance for walking time (mover starts adjacent)
        if (test_verbose) {
            printf("  Bare hands ticks: %d, Baseline ticks: %d, ratio: %.2f\n",
                   ticksBareHands, ticksBaseline, (float)ticksBareHands / ticksBaseline);
        }
        // The work portion should be 2x. Walking is same. So ratio should be > 1.5
        expect(ticksBareHands > ticksBaseline);
        // With mover starting adjacent, walk time is minimal — ratio should be close to 2.0
        float ratio = (float)ticksBareHands / (float)ticksBaseline;
        expect(ratio > 1.6f && ratio < 2.4f);
    }
}

describe(story3_cannot_mine_rock_without_hammer) {
    it("mover cannot mine stone wall without hammer:2 — job stays unassigned") {
        toolRequirementsEnabled = true;
        SetupMiningTest(5, 3, MAT_GRANITE, 4, 3);  // stone wall at (5,3)

        expect(movers[0].equippedTool == -1);

        DesignateMine(5, 3, 0);

        // Run sim for a while — wall should NOT be mined
        g_wallCheckX = 5; g_wallCheckY = 3;
        RunSimUntil(WallIsGone, 500);

        // Wall still there
        expect(grid[0][3][5] == CELL_WALL);

        // Mover should be idle (skipped the job)
        expect(movers[0].currentJobId < 0);

        // Designation still exists, unassigned
        expect(HasMineDesignation(5, 3, 0) == true);
        Designation* d = GetDesignation(5, 3, 0);
        expect(d != NULL);
        expect(d->assignedMover == -1);
    }
}

describe(story5_cannot_chop_without_cutting) {
    it("mover cannot chop tree without cutting:1 — job stays unassigned") {
        toolRequirementsEnabled = true;

        // Set up a grid with a tree
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

        // Place a tree trunk at (5,3)
        grid[0][3][5] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        Point goal = {4, 3, 0};
        InitMover(&movers[0], 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        bool desig = DesignateChop(5, 3, 0);
        expect(desig == true);

        // Run sim — tree should not be chopped
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        expect(grid[0][3][5] == CELL_TREE_TRUNK);
        expect(movers[0].currentJobId < 0);
        expect(HasChopDesignation(5, 3, 0) == true);
    }
}

describe(story9_toggle_disables_everything) {
    it("toolRequirementsEnabled=false lets movers mine stone and chop without tools") {
        toolRequirementsEnabled = false;

        // Test 1: Mine stone without tools
        SetupMiningTest(5, 3, MAT_GRANITE, 4, 3);
        toolRequirementsEnabled = false;  // ensure it's off after SetupMiningTest

        DesignateMine(5, 3, 0);

        g_wallCheckX = 5; g_wallCheckY = 3;
        int ticks = RunSimUntil(WallIsGone, 2000);

        expect(grid[0][3][5] != CELL_WALL);  // Stone wall should be mined
        expect(ticks < 2000);  // Should complete

        // Test 2: Chop tree without tools
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
        grid[0][3][5] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();
        toolRequirementsEnabled = false;

        Point goal2 = {4, 3, 0};
        InitMover(&movers[0], 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal2, MOVER_SPEED);
        moverCount = 1;

        DesignateChop(5, 3, 0);

        bool chopDone = false;
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (grid[0][3][5] != CELL_TREE_TRUNK) {
                chopDone = true;
                break;
            }
        }
        expect(chopDone == true);
    }

    it("speed is 1.0x when toggle off, not 0.5x") {
        // Compare mining speed with toggle on (bare hands → 0.5x) vs off (→ 1.0x)
        toolRequirementsEnabled = true;
        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);
        DesignateMine(5, 3, 0);
        g_wallCheckX = 5; g_wallCheckY = 3;
        int ticksSlow = RunSimUntil(WallIsGone, 2000);

        toolRequirementsEnabled = false;
        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);
        DesignateMine(5, 3, 0);
        int ticksFast = RunSimUntil(WallIsGone, 2000);

        // With toggle off, should be ~2x faster than bare hands
        if (test_verbose) {
            printf("  Toggle on (bare hands): %d ticks, Toggle off: %d ticks\n", ticksSlow, ticksFast);
        }
        expect(ticksSlow > ticksFast);
        float ratio = (float)ticksSlow / (float)ticksFast;
        expect(ratio > 1.6f && ratio < 2.4f);
    }
}

// ===========================================================================
// New tool items: quality lookups (Context 3)
// ===========================================================================
describe(new_tool_quality_lookup) {
    it("digging stick has digging:1") {
        expect(GetItemQualityLevel(ITEM_DIGGING_STICK, QUALITY_DIGGING) == 1);
    }

    it("digging stick has no other qualities") {
        expect(GetItemQualityLevel(ITEM_DIGGING_STICK, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_DIGGING_STICK, QUALITY_HAMMERING) == 0);
        expect(GetItemQualityLevel(ITEM_DIGGING_STICK, QUALITY_SAWING) == 0);
        expect(GetItemQualityLevel(ITEM_DIGGING_STICK, QUALITY_FINE) == 0);
    }

    it("stone axe has cutting:2 and hammering:1") {
        expect(GetItemQualityLevel(ITEM_STONE_AXE, QUALITY_CUTTING) == 2);
        expect(GetItemQualityLevel(ITEM_STONE_AXE, QUALITY_HAMMERING) == 1);
    }

    it("stone axe has no digging or fine") {
        expect(GetItemQualityLevel(ITEM_STONE_AXE, QUALITY_DIGGING) == 0);
        expect(GetItemQualityLevel(ITEM_STONE_AXE, QUALITY_FINE) == 0);
    }

    it("stone pick has digging:2 and hammering:2") {
        expect(GetItemQualityLevel(ITEM_STONE_PICK, QUALITY_DIGGING) == 2);
        expect(GetItemQualityLevel(ITEM_STONE_PICK, QUALITY_HAMMERING) == 2);
    }

    it("stone pick has no cutting or fine") {
        expect(GetItemQualityLevel(ITEM_STONE_PICK, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_STONE_PICK, QUALITY_FINE) == 0);
    }

    it("stone hammer has hammering:2") {
        expect(GetItemQualityLevel(ITEM_STONE_HAMMER, QUALITY_HAMMERING) == 2);
    }

    it("stone hammer has no other qualities") {
        expect(GetItemQualityLevel(ITEM_STONE_HAMMER, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_STONE_HAMMER, QUALITY_DIGGING) == 0);
        expect(GetItemQualityLevel(ITEM_STONE_HAMMER, QUALITY_FINE) == 0);
    }
}

// ===========================================================================
// New tool items: IF_TOOL flag (Context 3)
// ===========================================================================
describe(new_tool_flag) {
    it("all new tool items have IF_TOOL") {
        expect(ItemIsTool(ITEM_DIGGING_STICK) != 0);
        expect(ItemIsTool(ITEM_STONE_AXE) != 0);
        expect(ItemIsTool(ITEM_STONE_PICK) != 0);
        expect(ItemIsTool(ITEM_STONE_HAMMER) != 0);
    }

    it("new tool items have ItemHasAnyQuality") {
        expect(ItemHasAnyQuality(ITEM_DIGGING_STICK) == true);
        expect(ItemHasAnyQuality(ITEM_STONE_AXE) == true);
        expect(ItemHasAnyQuality(ITEM_STONE_PICK) == true);
        expect(ItemHasAnyQuality(ITEM_STONE_HAMMER) == true);
    }

    it("new tools are non-stackable") {
        expect((ItemFlags(ITEM_DIGGING_STICK) & IF_STACKABLE) == 0);
        expect((ItemFlags(ITEM_STONE_AXE) & IF_STACKABLE) == 0);
        expect((ItemFlags(ITEM_STONE_PICK) & IF_STACKABLE) == 0);
        expect((ItemFlags(ITEM_STONE_HAMMER) & IF_STACKABLE) == 0);
    }
}

// ===========================================================================
// New tool items: CanMoverDoJob with new tools (Context 3)
// ===========================================================================
describe(new_tool_can_do_job) {
    it("stone hammer unlocks rock mining") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int hammerIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_HAMMER);
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, hammerIdx) == true);
    }

    it("stone pick unlocks rock mining") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int pickIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_PICK);
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, pickIdx) == true);
    }

    it("digging stick cannot mine rock") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int digIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_DIGGING_STICK);
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_GRANITE, digIdx) == false);
    }

    it("stone axe can chop trees at cutting:2") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int axeIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_AXE);
        expect(CanMoverDoJob(JOBTYPE_CHOP, MAT_NONE, axeIdx) == true);
    }

    it("stone hammer cannot chop trees") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int hammerIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_HAMMER);
        expect(CanMoverDoJob(JOBTYPE_CHOP, MAT_NONE, hammerIdx) == false);
    }

    it("digging stick helps with soft soil mining") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int digIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_DIGGING_STICK);
        // Soft job, always doable
        expect(CanMoverDoJob(JOBTYPE_MINE, MAT_DIRT, digIdx) == true);
    }
}

// ===========================================================================
// New tool items: speed multiplier (Context 3)
// ===========================================================================
describe(new_tool_speed) {
    it("digging stick mines dirt at 1.0x") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int digIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_DIGGING_STICK);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_DIRT, digIdx);
        expect(speed > 0.99f && speed < 1.01f);
    }

    it("stone pick mines dirt at 1.5x") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int pickIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_PICK);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_DIRT, pickIdx);
        expect(speed > 1.49f && speed < 1.51f);
    }

    it("stone hammer mines rock at 1.0x") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int hammerIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_HAMMER);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_GRANITE, hammerIdx);
        expect(speed > 0.99f && speed < 1.01f);
    }

    it("stone pick mines rock at 1.0x") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int pickIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_PICK);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, MAT_GRANITE, pickIdx);
        expect(speed > 0.99f && speed < 1.01f);
    }

    it("stone axe chops trees at 1.5x") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int axeIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_AXE);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_CHOP, MAT_NONE, axeIdx);
        expect(speed > 1.49f && speed < 1.51f);
    }

    it("stone axe builds at 1.0x (hammer:1 on soft job)") {
        toolRequirementsEnabled = true;
        InitTestGrid(8, 8);
        ClearItems();
        int axeIdx = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0, ITEM_STONE_AXE);
        // Stone axe has hammering:1, building is soft (minLevel=0): level 1 → 1.0x
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_BUILD, MAT_NONE, axeIdx);
        expect(speed > 0.99f && speed < 1.01f);
    }
}

// ===========================================================================
// Recipe quality requirements (Context 3)
// ===========================================================================
describe(recipe_quality_requirements) {
    it("carpenter tool recipes require cutting:1") {
        int recipeCount;
        const Recipe* recipes = GetRecipesForWorkshop(WORKSHOP_CARPENTER, &recipeCount);
        expect(recipeCount >= 6);  // 2 existing + 4 tool recipes

        // Find digging stick recipe
        bool foundDigStick = false;
        bool foundHammer = false;
        bool foundAxe = false;
        bool foundPick = false;
        for (int i = 0; i < recipeCount; i++) {
            if (recipes[i].outputType == ITEM_DIGGING_STICK) {
                expect(recipes[i].requiredQuality == QUALITY_CUTTING);
                expect(recipes[i].requiredQualityLevel == 1);
                foundDigStick = true;
            }
            if (recipes[i].outputType == ITEM_STONE_HAMMER) {
                expect(recipes[i].requiredQuality == QUALITY_CUTTING);
                expect(recipes[i].requiredQualityLevel == 1);
                foundHammer = true;
            }
            if (recipes[i].outputType == ITEM_STONE_AXE) {
                expect(recipes[i].requiredQuality == QUALITY_CUTTING);
                expect(recipes[i].requiredQualityLevel == 1);
                foundAxe = true;
            }
            if (recipes[i].outputType == ITEM_STONE_PICK) {
                expect(recipes[i].requiredQuality == QUALITY_CUTTING);
                expect(recipes[i].requiredQualityLevel == 1);
                foundPick = true;
            }
        }
        expect(foundDigStick == true);
        expect(foundHammer == true);
        expect(foundAxe == true);
        expect(foundPick == true);
    }

    it("existing recipes have no quality requirement") {
        int recipeCount;
        const Recipe* recipes = GetRecipesForWorkshop(WORKSHOP_STONECUTTER, &recipeCount);
        for (int i = 0; i < recipeCount; i++) {
            expect(recipes[i].requiredQualityLevel == 0);
        }

        recipes = GetRecipesForWorkshop(WORKSHOP_SAWMILL, &recipeCount);
        for (int i = 0; i < recipeCount; i++) {
            expect(recipes[i].requiredQualityLevel == 0);
        }
    }

    it("carpenter bed/chair recipes have no quality requirement") {
        int recipeCount;
        const Recipe* recipes = GetRecipesForWorkshop(WORKSHOP_CARPENTER, &recipeCount);
        // First two recipes are bed and chair
        expect(recipes[0].requiredQualityLevel == 0);
        expect(recipes[1].requiredQualityLevel == 0);
    }
}

// ===========================================================================
// FindNearestToolForQuality unit tests (Context 4)
// ===========================================================================
describe(find_nearest_tool) {
    it("should find unreserved tool with matching quality on same z-level") {
        InitTestGrid(16, 16);
        ClearItems();

        int axeIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        expect(axeIdx >= 0);
        items[axeIdx].state = ITEM_ON_GROUND;
        items[axeIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == axeIdx);
    }

    it("should not find tool on different z-level") {
        InitTestGrid(16, 16);
        ClearItems();

        int axeIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 1, ITEM_SHARP_STONE);
        items[axeIdx].state = ITEM_ON_GROUND;
        items[axeIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == -1);
    }

    it("should not find reserved tool") {
        InitTestGrid(16, 16);
        ClearItems();

        int axeIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[axeIdx].state = ITEM_ON_GROUND;
        items[axeIdx].reservedBy = 0;  // reserved

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == -1);
    }

    it("should not find tool that lacks required quality") {
        InitTestGrid(16, 16);
        ClearItems();

        // Rock has hammering:1 but no cutting
        int rockIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_ROCK);
        items[rockIdx].state = ITEM_ON_GROUND;
        items[rockIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == -1);
    }

    it("should find closest tool when multiple available") {
        InitTestGrid(16, 16);
        ClearItems();

        int farIdx = SpawnItem(10 * CELL_SIZE, 10 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[farIdx].state = ITEM_ON_GROUND;
        items[farIdx].reservedBy = -1;

        int nearIdx = SpawnItem(4 * CELL_SIZE, 4 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[nearIdx].state = ITEM_ON_GROUND;
        items[nearIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == nearIdx);
    }

    it("should exclude specified item index") {
        InitTestGrid(16, 16);
        ClearItems();

        int onlyIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[onlyIdx].state = ITEM_ON_GROUND;
        items[onlyIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, onlyIdx);
        expect(found == -1);
    }

    it("should find tool in stockpile") {
        InitTestGrid(16, 16);
        ClearItems();

        int toolIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[toolIdx].state = ITEM_IN_STOCKPILE;
        items[toolIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == toolIdx);
    }

    it("should not find carried tool") {
        InitTestGrid(16, 16);
        ClearItems();

        int toolIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[toolIdx].state = ITEM_CARRIED;
        items[toolIdx].reservedBy = 0;

        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 3, 3, 0, 50, -1);
        expect(found == -1);
    }

    it("should respect search radius") {
        InitTestGrid(16, 16);
        ClearItems();

        int toolIdx = SpawnItem(14 * CELL_SIZE, 14 * CELL_SIZE, 0, ITEM_SHARP_STONE);
        items[toolIdx].state = ITEM_ON_GROUND;
        items[toolIdx].reservedBy = -1;

        // Search radius 5 tiles from (1,1) — tool at (14,14) is way out of range
        int found = FindNearestToolForQuality(QUALITY_CUTTING, 1, 1, 1, 0, 5, -1);
        expect(found == -1);
    }

    it("should find stone hammer for hammering:2") {
        InitTestGrid(16, 16);
        ClearItems();

        int hammerIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_STONE_HAMMER);
        items[hammerIdx].state = ITEM_ON_GROUND;
        items[hammerIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_HAMMERING, 2, 3, 3, 0, 50, -1);
        expect(found == hammerIdx);
    }

    it("should not find rock for hammering:2 (only has hammer:1)") {
        InitTestGrid(16, 16);
        ClearItems();

        int rockIdx = SpawnItem(5 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_ROCK);
        items[rockIdx].state = ITEM_ON_GROUND;
        items[rockIdx].reservedBy = -1;

        int found = FindNearestToolForQuality(QUALITY_HAMMERING, 2, 3, 3, 0, 50, -1);
        expect(found == -1);
    }
}

// ===========================================================================
// DropEquippedTool unit tests (Context 4)
// ===========================================================================
describe(drop_equipped_tool) {
    it("should drop tool and clear equippedTool") {
        InitTestGrid(8, 8);
        ClearMovers();
        ClearItems();

        Point goal = {4, 4, 0};
        InitMover(&movers[0], 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int toolIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_SHARP_STONE);
        items[toolIdx].state = ITEM_CARRIED;
        items[toolIdx].reservedBy = 0;
        movers[0].equippedTool = toolIdx;

        DropEquippedTool(0);

        expect(movers[0].equippedTool == -1);
        expect(items[toolIdx].state == ITEM_ON_GROUND);
        expect(items[toolIdx].reservedBy == -1);
    }

    it("should be no-op when equippedTool is -1") {
        InitTestGrid(8, 8);
        ClearMovers();
        ClearItems();

        Point goal = {4, 4, 0};
        InitMover(&movers[0], 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        DropEquippedTool(0);  // should not crash
        expect(movers[0].equippedTool == -1);
    }
}

// ===========================================================================
// Story 2: Mover with digging stick digs soil at 1.0x speed (Context 4 E2E)
// ===========================================================================
describe(story2_digging_stick_soil_mining) {
    it("mover with digging stick digs dirt at 1.0x — twice as fast as bare hands") {
        toolRequirementsEnabled = true;

        // First: measure bare-hands time (0.5x)
        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);
        movers[0].equippedTool = -1;
        DesignateMine(5, 3, 0);
        g_wallCheckX = 5; g_wallCheckY = 3;
        int ticksBareHands = RunSimUntil(WallIsGone, 2000);
        expect(grid[0][3][5] != CELL_WALL);

        // Second: measure with digging stick equipped (dig:1 → 1.0x)
        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);
        int stickIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_DIGGING_STICK);
        items[stickIdx].state = ITEM_CARRIED;
        items[stickIdx].reservedBy = 0;
        movers[0].equippedTool = stickIdx;
        DesignateMine(5, 3, 0);
        int ticksWithStick = RunSimUntil(WallIsGone, 2000);
        expect(grid[0][3][5] != CELL_WALL);

        // Digging stick should be ~2x faster than bare hands
        if (test_verbose) {
            printf("  Bare hands: %d ticks, Digging stick: %d ticks, ratio: %.2f\n",
                   ticksBareHands, ticksWithStick, (float)ticksBareHands / ticksWithStick);
        }
        float ratio = (float)ticksBareHands / (float)ticksWithStick;
        expect(ratio > 1.6f && ratio < 2.4f);

        // Mover still holds the digging stick
        expect(movers[0].equippedTool == stickIdx);
        expect(items[stickIdx].state == ITEM_CARRIED);
    }
}

// ===========================================================================
// Story 4: Mover with stone hammer mines rock at 1.0x (Context 4 E2E)
// ===========================================================================
describe(story4_stone_hammer_mines_rock) {
    it("mover with stone hammer mines stone wall at baseline speed") {
        toolRequirementsEnabled = true;

        // Measure with stone hammer (hammer:2 meets hard gate → 1.0x)
        SetupMiningTest(5, 3, MAT_GRANITE, 4, 3);
        int hammerIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_STONE_HAMMER);
        items[hammerIdx].state = ITEM_CARRIED;
        items[hammerIdx].reservedBy = 0;
        movers[0].equippedTool = hammerIdx;
        DesignateMine(5, 3, 0);
        g_wallCheckX = 5; g_wallCheckY = 3;
        int ticksHammer = RunSimUntil(WallIsGone, 2000);
        expect(grid[0][3][5] != CELL_WALL);

        // Mover still holds the hammer (check before second setup clears items)
        expect(movers[0].equippedTool == hammerIdx);
        expect(items[hammerIdx].state == ITEM_CARRIED);

        // Compare with toggle-off baseline (1.0x, no tools needed)
        SetupMiningTest(5, 3, MAT_GRANITE, 4, 3);
        toolRequirementsEnabled = false;
        DesignateMine(5, 3, 0);
        int ticksBaseline = RunSimUntil(WallIsGone, 2000);
        expect(grid[0][3][5] != CELL_WALL);

        toolRequirementsEnabled = true;

        // Both should be ~1.0x — similar tick counts
        if (test_verbose) {
            printf("  Hammer: %d ticks, Baseline: %d ticks, ratio: %.2f\n",
                   ticksHammer, ticksBaseline, (float)ticksHammer / ticksBaseline);
        }
        float ratio = (float)ticksHammer / (float)ticksBaseline;
        expect(ratio > 0.8f && ratio < 1.2f);
    }
}

// ===========================================================================
// Story 11: Rock (hammer:1) helps building but not rock mining (Context 4 E2E)
// ===========================================================================
describe(story11_rock_hammer1_building_not_mining) {
    it("mover with rock builds at 1.0x but cannot mine stone") {
        toolRequirementsEnabled = true;

        // Part 1: Rock (hammer:1) should allow building at 1.0x (soft, minLevel=0)
        // We test indirectly: mover with rock equipped can be assigned a build job
        // and the speed multiplier is 1.0x (already unit-tested).
        // Here we verify the mining gate: rock hammer:1 < required hammer:2.

        // Set up stone wall for mining
        SetupMiningTest(5, 3, MAT_GRANITE, 4, 3);

        // Give mover a rock as "tool" (hammer:1)
        int rockIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_ROCK);
        items[rockIdx].state = ITEM_CARRIED;
        items[rockIdx].reservedBy = 0;
        movers[0].equippedTool = rockIdx;

        DesignateMine(5, 3, 0);

        // Run sim — rock mining needs hammer:2, rock only has hammer:1
        // Job should stay unassigned
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Wall still there — rock's hammer:1 is not enough for stone mining
        expect(grid[0][3][5] == CELL_WALL);
        expect(HasMineDesignation(5, 3, 0) == true);
    }
}

// ===========================================================================
// Story 6: Mover seeks sharp stone before chopping tree (Context 4)
// ===========================================================================
describe(story6_seek_tool_for_chop) {
    it("mover finds sharp stone, picks it up, then chops tree") {
        toolRequirementsEnabled = true;

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

        // Place a tree trunk at (7,3)
        grid[0][3][7] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Mover at (1,3) with no tool
        Point goal = {1, 3, 0};
        InitMover(&movers[0], 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        // Sharp stone at (3,3)
        int ssIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                              3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_ON_GROUND;
        items[ssIdx].reservedBy = -1;

        // Designate chop
        bool desig = DesignateChop(7, 3, 0);
        expect(desig == true);

        // Run simulation
        bool chopDone = false;
        bool pickedUpTool = false;
        for (int i = 0; i < 3000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            // Track when tool is picked up
            if (movers[0].equippedTool == ssIdx && !pickedUpTool) {
                pickedUpTool = true;
                if (test_verbose) printf("  Tool picked up at tick %d\n", i);
            }

            if (grid[0][3][7] != CELL_TREE_TRUNK) {
                chopDone = true;
                if (test_verbose) printf("  Tree chopped at tick %d\n", i);
                break;
            }
        }

        expect(pickedUpTool == true);
        expect(chopDone == true);
        expect(movers[0].equippedTool == ssIdx);
        expect(items[ssIdx].state == ITEM_CARRIED);
    }
}

// ===========================================================================
// Story 7: Mover keeps tool across consecutive chop jobs (Context 4)
// ===========================================================================
describe(story7_keep_tool_across_jobs) {
    it("mover keeps sharp stone across two chop jobs") {
        toolRequirementsEnabled = true;

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

        // Two trees
        grid[0][3][5] = CELL_TREE_TRUNK;
        grid[0][3][8] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Mover at (3,3) with sharp stone already equipped
        Point goal = {3, 3, 0};
        InitMover(&movers[0], 3 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int ssIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_CARRIED;
        items[ssIdx].reservedBy = 0;
        movers[0].equippedTool = ssIdx;

        // Designate both trees
        DesignateChop(5, 3, 0);
        DesignateChop(8, 3, 0);

        // Run until both trees chopped
        int treesChopped = 0;
        for (int i = 0; i < 5000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            if (grid[0][3][5] != CELL_TREE_TRUNK && grid[0][3][8] != CELL_TREE_TRUNK) {
                treesChopped = 2;
                break;
            }
        }

        expect(treesChopped == 2);
        // Mover still holds the same tool
        expect(movers[0].equippedTool == ssIdx);
        expect(items[ssIdx].state == ITEM_CARRIED);
        expect(items[ssIdx].reservedBy == 0);
    }
}

// ===========================================================================
// Story 8: Mover swaps cutting tool for hammer when mining rock (Context 4)
// ===========================================================================
describe(story8_tool_swap) {
    it("mover drops cutting tool and picks up hammer for rock mining") {
        toolRequirementsEnabled = true;

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

        // Stone wall at (7,3)
        grid[0][3][7] = CELL_WALL;
        SetWallMaterial(7, 3, 0, MAT_GRANITE);
        SetWallNatural(7, 3, 0);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Mover at (1,3) with sharp stone equipped
        Point goal = {1, 3, 0};
        InitMover(&movers[0], 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int ssIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_CARRIED;
        items[ssIdx].reservedBy = 0;
        movers[0].equippedTool = ssIdx;

        // Stone hammer at (3,3)
        int hammerIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_STONE_HAMMER);
        items[hammerIdx].state = ITEM_ON_GROUND;
        items[hammerIdx].reservedBy = -1;

        // Designate mine on stone wall
        DesignateMine(7, 3, 0);

        // Run simulation
        g_wallCheckX = 7; g_wallCheckY = 3;
        RunSimUntil(WallIsGone, 5000);

        expect(grid[0][3][7] != CELL_WALL);  // Wall mined
        expect(movers[0].equippedTool == hammerIdx);  // Now holds hammer
        expect(items[hammerIdx].state == ITEM_CARRIED);
        // Sharp stone was dropped
        expect(items[ssIdx].state == ITEM_ON_GROUND);
        expect(items[ssIdx].reservedBy == -1);
    }
}

// ===========================================================================
// Story 12: Dead mover drops equipped tool (Context 4)
// ===========================================================================
describe(story12_death_drops_tool) {
    it("mover drops tool on starvation death") {
        toolRequirementsEnabled = true;

        InitTestGrid(8, 8);
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        GameMode savedMode = gameMode;
        bool savedHunger = hungerEnabled;
        float savedDt = gameDeltaTime;
        gameMode = GAME_MODE_SURVIVAL;
        hungerEnabled = true;
        gameDeltaTime = 0.016f;

        Point goal = {4, 4, 0};
        InitMover(&movers[0], 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int pickIdx = SpawnItem(movers[0].x, movers[0].y, 0, ITEM_STONE_PICK);
        items[pickIdx].state = ITEM_CARRIED;
        items[pickIdx].reservedBy = 0;
        movers[0].equippedTool = pickIdx;

        // Set mover to starving, push past threshold
        movers[0].hunger = 0.0f;
        movers[0].starvationTimer = GameHoursToGameSeconds(balance.starvationDeathGH) + 1.0f;

        // Tick to trigger death
        NeedsTick();

        expect(movers[0].active == false);
        expect(items[pickIdx].active == true);
        expect(items[pickIdx].state == ITEM_ON_GROUND);
        expect(items[pickIdx].reservedBy == -1);

        gameMode = savedMode;
        hungerEnabled = savedHunger;
        gameDeltaTime = savedDt;
    }
}

// ===========================================================================
// Edge case: cancel job mid-fetch releases tool reservation (Context 4)
// ===========================================================================
describe(cancel_mid_fetch) {
    it("cancelling a job releases toolItem reservation") {
        toolRequirementsEnabled = true;

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

        grid[0][3][7] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Mover at (1,3)
        Point goal = {1, 3, 0};
        InitMover(&movers[0], 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        // Sharp stone at (4,3)
        int ssIdx = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                              3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_ON_GROUND;
        items[ssIdx].reservedBy = -1;

        DesignateChop(7, 3, 0);

        // Run a few ticks to get the job assigned
        for (int i = 0; i < 10; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Mover should have a job and tool should be reserved
        int jobId = movers[0].currentJobId;
        if (jobId >= 0) {
            // Tool should be reserved
            expect(items[ssIdx].reservedBy == 0);

            // Cancel the job
            CancelJob(&movers[0], 0);

            expect(movers[0].currentJobId < 0);
            // If mover hasn't picked up the tool yet, reservation should be released
            if (movers[0].equippedTool != ssIdx) {
                expect(items[ssIdx].reservedBy == -1);
            }
        }
    }
}

// ===========================================================================
// Story 5 regression: no tool available, job stays unassigned (Context 4)
// ===========================================================================
describe(story5_regression_no_tool) {
    it("chop designation stays unassigned when no cutting tool exists") {
        toolRequirementsEnabled = true;

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

        grid[0][3][5] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        Point goal = {4, 3, 0};
        InitMover(&movers[0], 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        // No tools spawned!
        DesignateChop(5, 3, 0);

        for (int i = 0; i < 200; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Tree should still be standing
        expect(grid[0][3][5] == CELL_TREE_TRUNK);
        expect(movers[0].currentJobId < 0);
        expect(HasChopDesignation(5, 3, 0) == true);
    }
}

// ===========================================================================
// Story 10: Full bootstrap — knap, craft digging stick, dig soil (Context 5)
// ===========================================================================

static bool g_sharpStoneFound = false;
static int g_sharpStoneIdx = -1;

static bool SharpStoneExists(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active && items[i].type == ITEM_SHARP_STONE) {
            g_sharpStoneFound = true;
            g_sharpStoneIdx = i;
            return true;
        }
    }
    return false;
}

static bool g_diggingStickFound = false;
static int g_diggingStickIdx = -1;

static bool DiggingStickExists(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active && items[i].type == ITEM_DIGGING_STICK) {
            g_diggingStickFound = true;
            g_diggingStickIdx = i;
            return true;
        }
    }
    return false;
}

describe(story10_full_bootstrap) {
    it("knap sharp stone, craft digging stick, dig soil — full progression chain") {
        toolRequirementsEnabled = true;

        // 16x16 flat grid
        InitTestGridFromAscii(
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Stone wall at (3,5) for knapping
        grid[0][5][3] = CELL_WALL;
        SetWallMaterial(3, 5, 0, MAT_GRANITE);
        SetWallNatural(3, 5, 0);

        // Dirt wall at (12,5) to dig later
        grid[0][5][12] = CELL_WALL;
        SetWallMaterial(12, 5, 0, MAT_DIRT);
        SetWallNatural(12, 5, 0);

        // Carpenter workshop at (7,3) — 3x3 template ".O." / "#X#" / "..."
        // Output tile at (8,3), work tile at (8,4)
        int wsIdx = CreateWorkshop(7, 3, 0, WORKSHOP_CARPENTER);
        expect(wsIdx >= 0);

        // Bill: recipe index 2 = "Craft Digging Stick" (1x sticks, requires cutting:1)
        int billIdx = AddBill(wsIdx, 2, BILL_DO_X_TIMES, 1);
        expect(billIdx >= 0);

        // Stockpile for output — required by WorkGiver_Craft storage check
        int sp = CreateStockpile(13, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_DIGGING_STICK, true);

        // Rock on ground at (2,5) — knapping input
        int rockIdx = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                5 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_ROCK);
        items[rockIdx].state = ITEM_ON_GROUND;
        items[rockIdx].reservedBy = -1;

        // Sticks on ground at (7,2) — crafting input
        int sticksIdx = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                                  2 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_STICKS);
        items[sticksIdx].state = ITEM_ON_GROUND;
        items[sticksIdx].reservedBy = -1;

        // Mover at (1,5), no tool
        Point goal = {1, 5, 0};
        InitMover(&movers[0], 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        // ---- PHASE 1: Knap sharp stone ----
        bool desigOk = DesignateKnap(3, 5, 0);
        expect(desigOk == true);

        g_sharpStoneFound = false;
        g_sharpStoneIdx = -1;
        int knapTicks = RunSimUntil(SharpStoneExists, 5000);
        expect(g_sharpStoneFound == true);
        if (test_verbose) {
            printf("  Phase 1 (knap): sharp stone created at tick %d\n", knapTicks);
        }

        // Stone wall should still exist (knapping doesn't consume it)
        expect(grid[0][5][3] == CELL_WALL);

        // ---- PHASE 2: Craft digging stick ----
        // The carpenter bill is already queued. AssignJobs will pick it up.
        // Mover needs cutting:1 → should seek the sharp stone as a tool.
        g_diggingStickFound = false;
        g_diggingStickIdx = -1;
        int craftTicks = RunSimUntil(DiggingStickExists, 10000);
        expect(g_diggingStickFound == true);
        if (test_verbose) {
            printf("  Phase 2 (craft): digging stick created at tick %d\n", craftTicks);
        }

        // Mover should have the sharp stone equipped (used as tool for crafting)
        expect(movers[0].equippedTool >= 0);
        expect(items[movers[0].equippedTool].type == ITEM_SHARP_STONE);

        // ---- PHASE 3: Dig soil with digging stick ----
        // Designate the dirt wall for mining
        DesignateMine(12, 5, 0);

        // Mover currently holds sharp stone (cutting:1) but needs digging quality.
        // Digging stick (dig:1) is on the ground at the output tile.
        // For soft soil mining, bare hands work at 0.5x.
        // If mover seeks digging stick, it gets 1.0x.
        g_wallCheckX = 12; g_wallCheckY = 5;
        int mineTicks = RunSimUntil(WallIsGone, 8000);
        expect(grid[0][5][12] != CELL_WALL);
        if (test_verbose) {
            printf("  Phase 3 (mine): dirt wall removed at tick %d\n", mineTicks);
        }

        // Verify the full chain completed
        expect(g_sharpStoneFound == true);
        expect(g_diggingStickFound == true);
        expect(grid[0][5][12] != CELL_WALL);
    }
}

// ===========================================================================
// Edge case: Soft job proceeds bare-handed when all tools reserved (Context 5)
// ===========================================================================
describe(edge_soft_job_no_tool_available) {
    it("mover mines dirt bare-handed when only tool is reserved") {
        toolRequirementsEnabled = true;

        SetupMiningTest(5, 3, MAT_DIRT, 4, 3);

        // Spawn a second mover that holds the only digging tool
        Point goal2 = {8, 3, 0};
        InitMover(&movers[1], 8 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal2, MOVER_SPEED);
        moverCount = 2;

        int stickIdx = SpawnItem(movers[1].x, movers[1].y, 0, ITEM_DIGGING_STICK);
        items[stickIdx].state = ITEM_CARRIED;
        items[stickIdx].reservedBy = 1;
        movers[1].equippedTool = stickIdx;

        // First mover has no tool
        movers[0].equippedTool = -1;

        DesignateMine(5, 3, 0);

        // Run sim — mover 0 should still mine dirt bare-handed (soft job, 0.5x)
        g_wallCheckX = 5; g_wallCheckY = 3;
        int ticks = RunSimUntil(WallIsGone, 3000);
        expect(grid[0][3][5] != CELL_WALL);

        // Mover 0 did the work without a tool
        expect(movers[0].equippedTool == -1);

        if (test_verbose) {
            printf("  Bare-handed mining completed at tick %d (soft job, no tool available)\n", ticks);
        }
    }
}

// ===========================================================================
// Edge case: Multiple movers competing for one tool (Context 5)
// ===========================================================================
describe(edge_tool_contention) {
    it("two movers, one tool — only one mover ever holds the tool at a time") {
        toolRequirementsEnabled = true;

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

        // Two trees
        grid[0][3][5] = CELL_TREE_TRUNK;
        grid[0][3][8] = CELL_TREE_TRUNK;

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Mover 0 at (1,3)
        Point goal0 = {1, 3, 0};
        InitMover(&movers[0], 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal0, MOVER_SPEED);
        movers[0].equippedTool = -1;

        // Mover 1 at (1,6)
        Point goal1 = {1, 6, 0};
        InitMover(&movers[1], 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  6 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal1, MOVER_SPEED);
        movers[1].equippedTool = -1;
        moverCount = 2;

        // Only one sharp stone
        int ssIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                              3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_ON_GROUND;
        items[ssIdx].reservedBy = -1;

        // Designate both trees for chopping (hard-gated: needs cutting:1)
        DesignateChop(5, 3, 0);
        DesignateChop(8, 3, 0);

        // Run sim — verify that at no point do both movers hold the tool
        bool bothHeldTool = false;
        for (int i = 0; i < 5000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            // Check invariant: at most one mover has the tool
            if (movers[0].equippedTool == ssIdx && movers[1].equippedTool == ssIdx) {
                bothHeldTool = true;
            }
        }

        // Invariant: tool was never held by both movers simultaneously
        expect(bothHeldTool == false);

        // Both trees should eventually be chopped (one mover does all the work)
        int treesChopped = 0;
        if (grid[0][3][5] != CELL_TREE_TRUNK) treesChopped++;
        if (grid[0][3][8] != CELL_TREE_TRUNK) treesChopped++;
        expect(treesChopped == 2);

        // Exactly one mover should hold the tool
        bool mover0HasTool = (movers[0].equippedTool == ssIdx);
        bool mover1HasTool = (movers[1].equippedTool == ssIdx);
        expect(mover0HasTool || mover1HasTool);

        if (test_verbose) {
            printf("  Trees chopped: %d, Mover 0 tool: %d, Mover 1 tool: %d\n",
                   treesChopped, movers[0].equippedTool, movers[1].equippedTool);
        }
    }
}

// ===========================================================================
// Tri-input crafting: stone hammer requires rock + sticks + cordage
// ===========================================================================
static bool g_stoneHammerFound;
static int g_stoneHammerIdx;
static bool StoneHammerExists(void) {
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == ITEM_STONE_HAMMER) {
            g_stoneHammerFound = true;
            g_stoneHammerIdx = i;
            return true;
        }
    }
    return false;
}

describe(tri_input_craft_stone_hammer) {
    it("crafts stone hammer from rock + sticks + cordage (3 inputs)") {
        toolRequirementsEnabled = true;

        InitTestGridFromAscii(
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Carpenter workshop at (7,3)
        int wsIdx = CreateWorkshop(7, 3, 0, WORKSHOP_CARPENTER);
        expect(wsIdx >= 0);

        // Bill: recipe index 3 = "Craft Stone Hammer" (rock + sticks + cordage, requires cutting:1)
        int billIdx = AddBill(wsIdx, 3, BILL_DO_X_TIMES, 1);
        expect(billIdx >= 0);

        // Output stockpile for stone hammer
        int sp = CreateStockpile(13, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_STONE_HAMMER, true);

        // Input items on ground near workshop
        int rockIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_ROCK);
        items[rockIdx].state = ITEM_ON_GROUND;
        items[rockIdx].reservedBy = -1;

        int sticksIdx = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f,
                                  2 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_STICKS);
        items[sticksIdx].state = ITEM_ON_GROUND;
        items[sticksIdx].reservedBy = -1;

        int cordageIdx = SpawnItem(9 * CELL_SIZE + CELL_SIZE * 0.5f,
                                   3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_CORDAGE);
        items[cordageIdx].state = ITEM_ON_GROUND;
        items[cordageIdx].reservedBy = -1;

        // Sharp stone for the cutting:1 requirement
        int ssIdx = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                              4 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_ON_GROUND;
        items[ssIdx].reservedBy = -1;

        // Mover at (2,3), no tool
        Point goal = {2, 3, 0};
        InitMover(&movers[0], 2 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        // Run sim — hammer should be crafted
        g_stoneHammerFound = false;
        g_stoneHammerIdx = -1;
        int craftTicks = RunSimUntil(StoneHammerExists, 10000);
        expect(g_stoneHammerFound == true);
        if (test_verbose) {
            printf("  Stone hammer crafted at tick %d\n", craftTicks);
        }

        // All 3 inputs should be consumed (slots may be reused by SpawnItem, so
        // check that the original item type is gone or the slot was reused)
        expect(!items[rockIdx].active || items[rockIdx].type != ITEM_ROCK);
        expect(!items[sticksIdx].active || items[sticksIdx].type != ITEM_STICKS);
        expect(!items[cordageIdx].active || items[cordageIdx].type != ITEM_CORDAGE);
    }

    it("cancel craft releases all 3 input reservations") {
        toolRequirementsEnabled = true;

        InitTestGridFromAscii(
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        InitDesignations();

        // Carpenter workshop at (7,3)
        int wsIdx = CreateWorkshop(7, 3, 0, WORKSHOP_CARPENTER);
        expect(wsIdx >= 0);

        // Bill: stone hammer
        int billIdx = AddBill(wsIdx, 3, BILL_DO_X_TIMES, 1);
        expect(billIdx >= 0);

        // Output stockpile
        int sp = CreateStockpile(13, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_STONE_HAMMER, true);

        // Input items
        int rockIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_ROCK);
        items[rockIdx].state = ITEM_ON_GROUND;
        items[rockIdx].reservedBy = -1;

        int sticksIdx = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f,
                                  2 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_STICKS);
        items[sticksIdx].state = ITEM_ON_GROUND;
        items[sticksIdx].reservedBy = -1;

        int cordageIdx = SpawnItem(9 * CELL_SIZE + CELL_SIZE * 0.5f,
                                   3 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_CORDAGE);
        items[cordageIdx].state = ITEM_ON_GROUND;
        items[cordageIdx].reservedBy = -1;

        // Sharp stone tool
        int ssIdx = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                              4 * CELL_SIZE + CELL_SIZE * 0.5f, 0, ITEM_SHARP_STONE);
        items[ssIdx].state = ITEM_ON_GROUND;
        items[ssIdx].reservedBy = -1;

        // Mover
        Point goal = {2, 3, 0};
        InitMover(&movers[0], 2 * CELL_SIZE + CELL_SIZE * 0.5f,
                  3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        movers[0].equippedTool = -1;

        // Run a few ticks so the job gets assigned and items get reserved
        for (int i = 0; i < 50; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Verify mover has a craft job
        expect(movers[0].currentJobId >= 0);
        Job* job = GetJob(movers[0].currentJobId);
        expect(job != NULL);
        expect(job->type == JOBTYPE_CRAFT);

        // Cancel the job
        CancelJob(&movers[0], 0);

        // All items should be unreserved
        expect(items[rockIdx].reservedBy == -1);
        expect(items[sticksIdx].reservedBy == -1);
        expect(items[cordageIdx].reservedBy == -1);
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) test_verbose = true;
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(quality_lookup);
    test(has_any_quality);
    test(tool_flag);
    test(speed_multiplier_soft);
    test(speed_multiplier_hard);
    test(mover_equipped_tool);
    test(game_scenarios);
    test(job_tool_requirement);
    test(can_mover_do_job);
    test(job_tool_speed);
    test(story1_bare_hands_soil_mining);
    test(story3_cannot_mine_rock_without_hammer);
    test(story5_cannot_chop_without_cutting);
    test(story9_toggle_disables_everything);
    test(new_tool_quality_lookup);
    test(new_tool_flag);
    test(new_tool_can_do_job);
    test(new_tool_speed);
    test(recipe_quality_requirements);
    test(find_nearest_tool);
    test(drop_equipped_tool);
    test(story2_digging_stick_soil_mining);
    test(story4_stone_hammer_mines_rock);
    test(story11_rock_hammer1_building_not_mining);
    test(story6_seek_tool_for_chop);
    test(story7_keep_tool_across_jobs);
    test(story8_tool_swap);
    test(story12_death_drops_tool);
    test(cancel_mid_fetch);
    test(story5_regression_no_tool);
    test(story10_full_bootstrap);
    test(edge_soft_job_no_tool_available);
    test(edge_tool_contention);
    test(tri_input_craft_stone_hammer);

    return summary();
}
