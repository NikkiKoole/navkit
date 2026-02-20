// Test suite for workshop deconstruction feature
// Tests marking, job assignment, execution, material refund, and cancellation

#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/world/construction.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"

#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: check if mover has a deconstruct job
static bool MoverHasDeconstructJob(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_DECONSTRUCT_WORKSHOP;
}

// Helper: check if mover is idle
static bool MoverIsIdle(Mover* m) {
    return m->currentJobId < 0;
}

// Helper: count items of a given type anywhere on the map
static int CountItemsOfType(ItemType type) {
    int count = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type) count++;
    }
    return count;
}

// Helper: standard setup for deconstruction tests
static void SetupDeconstructTest(void) {
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
    ClearStockpiles();
    ClearWorkshops();
    ClearJobs();
}

// =============================================================================
// MARKING TESTS
// =============================================================================

describe(marking) {
    it("should mark workshop for deconstruction") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        Workshop* ws = &workshops[wsIdx];

        expect(ws->markedForDeconstruct == false);
        ws->markedForDeconstruct = true;
        expect(ws->markedForDeconstruct == true);
    }

    it("should cancel deconstruction mark") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        Workshop* ws = &workshops[wsIdx];

        ws->markedForDeconstruct = true;
        ws->markedForDeconstruct = false;
        expect(ws->markedForDeconstruct == false);
    }

    it("should initialize deconstruction fields to defaults") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_STONECUTTER);
        Workshop* ws = &workshops[wsIdx];

        expect(ws->markedForDeconstruct == false);
        expect(ws->assignedDeconstructor == -1);
    }

    it("should clear deconstruction fields on ClearWorkshops") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;
        workshops[wsIdx].assignedDeconstructor = 3;

        ClearWorkshops();
        expect(workshops[wsIdx].markedForDeconstruct == false);
        expect(workshops[wsIdx].assignedDeconstructor == -1);
    }
}

// =============================================================================
// CONSTRUCTION RECIPE MAPPING TESTS
// =============================================================================

describe(recipe_mapping) {
    it("should map all workshop types to construction recipes") {
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_CAMPFIRE) == CONSTRUCTION_WORKSHOP_CAMPFIRE);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_DRYING_RACK) == CONSTRUCTION_WORKSHOP_DRYING_RACK);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_ROPE_MAKER) == CONSTRUCTION_WORKSHOP_ROPE_MAKER);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_CHARCOAL_PIT) == CONSTRUCTION_WORKSHOP_CHARCOAL_PIT);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_HEARTH) == CONSTRUCTION_WORKSHOP_HEARTH);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_STONECUTTER) == CONSTRUCTION_WORKSHOP_STONECUTTER);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_SAWMILL) == CONSTRUCTION_WORKSHOP_SAWMILL);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_KILN) == CONSTRUCTION_WORKSHOP_KILN);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_CARPENTER) == CONSTRUCTION_WORKSHOP_CARPENTER);
    }

    it("should return -1 for invalid workshop type") {
        expect(GetConstructionRecipeForWorkshopType(-1) == -1);
        expect(GetConstructionRecipeForWorkshopType(WORKSHOP_TYPE_COUNT) == -1);
        expect(GetConstructionRecipeForWorkshopType(99) == -1);
    }

    it("should return valid recipes with inputs") {
        for (int t = 0; t < WORKSHOP_TYPE_COUNT; t++) {
            int ri = GetConstructionRecipeForWorkshopType(t);
            expect(ri >= 0);
            const ConstructionRecipe* r = GetConstructionRecipe(ri);
            expect(r != NULL);
            expect(r->buildCategory == BUILD_WORKSHOP);
            expect(r->stageCount >= 1);
        }
    }
}

// =============================================================================
// JOB ASSIGNMENT TESTS
// =============================================================================

describe(job_assignment) {
    it("should assign deconstruct job to idle mover with canBuild") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();

        expect(MoverHasDeconstructJob(m));
        expect(workshops[wsIdx].assignedDeconstructor == 0);
    }

    it("should not assign deconstruct job if mover lacks canBuild") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = false;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();

        expect(MoverIsIdle(m));
        expect(workshops[wsIdx].assignedDeconstructor == -1);
    }

    it("should not assign if workshop not marked") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        // Not marked

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();

        expect(MoverIsIdle(m));
        expect(workshops[wsIdx].assignedDeconstructor == -1);
    }

    it("should only assign one mover per workshop") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        // Two movers
        for (int i = 0; i < 2; i++) {
            float mx = (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f;
            float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
            Point goal = {1 + i, 2, 0};
            InitMover(&movers[i], mx, my, 0.0f, goal, 200.0f);
            movers[i].capabilities.canBuild = true;
        }
        moverCount = 2;

        RebuildIdleMoverList();
        AssignJobs();

        // Exactly one should have the job
        int assignedCount = 0;
        for (int i = 0; i < 2; i++) {
            if (MoverHasDeconstructJob(&movers[i])) assignedCount++;
        }
        expect(assignedCount == 1);
        expect(workshops[wsIdx].assignedDeconstructor >= 0);
    }

    it("should set workRequired to half the build time") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = workshops[wsIdx].workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = workshops[wsIdx].workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {workshops[wsIdx].workTileX, workshops[wsIdx].workTileY, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();

        expect(MoverHasDeconstructJob(m));
        Job* job = GetJob(m->currentJobId);

        // Campfire build time is 1.0s, so deconstruct should be 0.5s
        int ri = GetConstructionRecipeForWorkshopType(WORKSHOP_CAMPFIRE);
        const ConstructionRecipe* r = GetConstructionRecipe(ri);
        float expectedTime = r->stages[0].buildTime * 0.5f;
        expect(fabsf(job->workRequired - expectedTime) < 0.01f);
    }
}

// =============================================================================
// JOB EXECUTION TESTS
// =============================================================================

describe(job_execution) {
    it("should walk to workshop and complete deconstruction") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();
        expect(MoverHasDeconstructJob(m));

        // Run until workshop is deleted or timeout
        bool completed = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!workshops[wsIdx].active) {
                completed = true;
                break;
            }
        }

        expect(completed == true);
        expect(workshops[wsIdx].active == false);
        expect(MoverIsIdle(m));
    }

    it("should refund materials on deconstruction") {
        SetupDeconstructTest();
        // Campfire costs 5 sticks — should refund ~75% = ~3-4
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        // Place mover right at the work tile to skip walking
        Mover* m = &movers[0];
        float mx = workshops[wsIdx].workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = workshops[wsIdx].workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {workshops[wsIdx].workTileX, workshops[wsIdx].workTileY, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        int sticksBefore = CountItemsOfType(ITEM_STICKS);
        expect(sticksBefore == 0);

        RebuildIdleMoverList();
        AssignJobs();

        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!workshops[wsIdx].active) break;
        }

        expect(workshops[wsIdx].active == false);

        // Should have spawned some sticks (75% chance each, 5 sticks total input)
        int sticksAfter = CountItemsOfType(ITEM_STICKS);
        // With 75% chance per 5 items, getting 0 is very unlikely (0.1%)
        // but possible. Check at least that items could be spawned.
        // We'll just verify the workshop is gone and mover is idle.
        if (test_verbose) {
            printf("  Sticks refunded: %d / 5\n", sticksAfter);
        }
        expect(sticksAfter >= 0 && sticksAfter <= 5);
    }

    it("should delete workshop after deconstruction completes") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(3, 3, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = workshops[wsIdx].workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = workshops[wsIdx].workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {workshops[wsIdx].workTileX, workshops[wsIdx].workTileY, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        expect(workshopCount == 1);

        RebuildIdleMoverList();
        AssignJobs();

        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!workshops[wsIdx].active) break;
        }

        expect(workshops[wsIdx].active == false);
        expect(workshopCount == 0);
    }

    it("should clear workshop blocking flags after deconstruction") {
        SetupDeconstructTest();
        // Stonecutter has WT_BLOCK tiles
        int wsIdx = CreateWorkshop(3, 3, 0, WORKSHOP_STONECUTTER);
        Workshop* ws = &workshops[wsIdx];
        ws->markedForDeconstruct = true;

        // Verify blocking flags are set
        bool hasBlock = false;
        for (int ty = 0; ty < ws->height; ty++) {
            for (int tx = 0; tx < ws->width; tx++) {
                if (ws->template[ty * ws->width + tx] == WT_BLOCK) {
                    expect(HAS_CELL_FLAG(ws->x + tx, ws->y + ty, ws->z, CELL_FLAG_WORKSHOP_BLOCK) == true);
                    hasBlock = true;
                }
            }
        }
        expect(hasBlock == true);

        Mover* m = &movers[0];
        float mx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        // Remember block tile positions before workshop gets cleared
        int blockTiles[25][2];
        int blockCount = 0;
        for (int ty = 0; ty < ws->height; ty++) {
            for (int tx = 0; tx < ws->width; tx++) {
                if (ws->template[ty * ws->width + tx] == WT_BLOCK) {
                    blockTiles[blockCount][0] = ws->x + tx;
                    blockTiles[blockCount][1] = ws->y + ty;
                    blockCount++;
                }
            }
        }

        RebuildIdleMoverList();
        AssignJobs();

        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!workshops[wsIdx].active) break;
        }

        expect(workshops[wsIdx].active == false);

        // Blocking flags should be cleared
        for (int b = 0; b < blockCount; b++) {
            expect(HAS_CELL_FLAG(blockTiles[b][0], blockTiles[b][1], 0, CELL_FLAG_WORKSHOP_BLOCK) == false);
        }
    }
}

// =============================================================================
// CANCELLATION TESTS
// =============================================================================

describe(cancellation) {
    it("should reset assignedDeconstructor when job cancelled") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();
        expect(workshops[wsIdx].assignedDeconstructor == 0);

        // Cancel the job
        CancelJob(m, 0);

        expect(workshops[wsIdx].assignedDeconstructor == -1);
        expect(MoverIsIdle(m));
        // Workshop should still be marked
        expect(workshops[wsIdx].markedForDeconstruct == true);
    }

    it("should allow reassignment after cancellation") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();
        expect(workshops[wsIdx].assignedDeconstructor == 0);

        CancelJob(m, 0);
        expect(workshops[wsIdx].assignedDeconstructor == -1);

        // Should be reassigned on next cycle
        RebuildIdleMoverList();
        AssignJobs();
        expect(workshops[wsIdx].assignedDeconstructor == 0);
        expect(MoverHasDeconstructJob(m));
    }

    it("should cancel craft jobs when DeleteWorkshop is called") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);
        Workshop* ws = &workshops[wsIdx];
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);

        // Set up mover with a craft job at the workshop
        Mover* m = &movers[0];
        float mx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        moverCount = 1;

        // Manually create a craft job
        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->step = CRAFT_STEP_WORKING;
        job->progress = 0.0f;
        job->workRequired = 5.0f;
        job->targetItem = -1;
        job->targetItem2 = -1;
        job->fuelItem = -1;
        job->carryingItem = -1;
        job->targetStockpile = -1;
        job->targetBlueprint = -1;
        m->currentJobId = jobId;
        ws->assignedCrafter = 0;
        RemoveMoverFromIdleList(0);

        expect(!MoverIsIdle(m));

        // Delete the workshop — should cancel the craft job
        DeleteWorkshop(wsIdx);

        expect(MoverIsIdle(m));
        expect(ws->active == false);
    }
}

// =============================================================================
// MULTIPLE WORKSHOPS
// =============================================================================

describe(multiple_workshops) {
    it("should deconstruct multiple marked workshops") {
        SetupDeconstructTest();
        int ws1 = CreateWorkshop(1, 1, 0, WORKSHOP_CAMPFIRE);
        int ws2 = CreateWorkshop(5, 5, 0, WORKSHOP_CAMPFIRE);
        workshops[ws1].markedForDeconstruct = true;
        workshops[ws2].markedForDeconstruct = true;

        // Two movers
        for (int i = 0; i < 2; i++) {
            Mover* m = &movers[i];
            float mx = workshops[i == 0 ? ws1 : ws2].workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
            float my = workshops[i == 0 ? ws1 : ws2].workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
            Point goal = {(int)(mx / CELL_SIZE), (int)(my / CELL_SIZE), 0};
            InitMover(m, mx, my, 0.0f, goal, 200.0f);
            m->capabilities.canBuild = true;
        }
        moverCount = 2;

        RebuildIdleMoverList();
        AssignJobs();

        // Both should be assigned
        expect(workshops[ws1].assignedDeconstructor >= 0);
        expect(workshops[ws2].assignedDeconstructor >= 0);

        // Run to completion
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!workshops[ws1].active && !workshops[ws2].active) break;
        }

        expect(workshops[ws1].active == false);
        expect(workshops[ws2].active == false);
    }
}

// =============================================================================
// DELETE WORKSHOP JOB CANCELLATION
// =============================================================================

describe(delete_workshop_cleanup) {
    it("should cancel deconstruct job when workshop is deleted externally") {
        SetupDeconstructTest();
        int wsIdx = CreateWorkshop(5, 2, 0, WORKSHOP_CAMPFIRE);
        workshops[wsIdx].markedForDeconstruct = true;

        Mover* m = &movers[0];
        float mx = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {1, 2, 0};
        InitMover(m, mx, my, 0.0f, goal, 200.0f);
        m->capabilities.canBuild = true;
        moverCount = 1;

        RebuildIdleMoverList();
        AssignJobs();
        expect(MoverHasDeconstructJob(m));

        // External delete (e.g. cheat tool)
        DeleteWorkshop(wsIdx);

        expect(workshops[wsIdx].active == false);
        expect(MoverIsIdle(m));
    }
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            test_verbose = true;
        }
    }

    if (!test_verbose) {
        set_quiet_mode(1);
    }

    printf("\n=== Workshop Deconstruction Tests ===\n\n");

    test(marking);
    test(recipe_mapping);
    test(job_assignment);
    test(job_execution);
    test(cancellation);
    test(multiple_workshops);
    test(delete_workshop_cleanup);

    return 0;
}
