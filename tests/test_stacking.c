#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/stacking.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/jobs.h"
#include "../src/entities/workshops.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

static void Setup(void) {
    InitTestGrid(8, 8);
    ClearItems();
    ClearStockpiles();
}

// ===========================================================================
// MergeItemIntoStack tests
// ===========================================================================
describe(merge_into_stack) {
    it("should fully merge when room available") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        int b = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;
        items[b].stackCount = 2;

        int merged = MergeItemIntoStack(a, b);

        expect(merged == 2);
        expect(items[a].stackCount == 5);
        expect(!items[b].active);  // incoming item deleted
    }

    it("should partially merge when exceeding max stack") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        int b = SpawnItem(16, 16, 0, ITEM_RED);
        int maxStack = ItemMaxStack(ITEM_RED);
        items[a].stackCount = maxStack - 2;
        items[b].stackCount = 5;

        int merged = MergeItemIntoStack(a, b);

        expect(merged == 2);
        expect(items[a].stackCount == maxStack);
        expect(items[b].active);  // incoming kept with remainder
        expect(items[b].stackCount == 3);
    }

    it("should return 0 when existing stack is full") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        int b = SpawnItem(16, 16, 0, ITEM_RED);
        int maxStack = ItemMaxStack(ITEM_RED);
        items[a].stackCount = maxStack;
        items[b].stackCount = 1;

        int merged = MergeItemIntoStack(a, b);

        expect(merged == 0);
        expect(items[a].stackCount == maxStack);
        expect(items[b].active);
        expect(items[b].stackCount == 1);
    }

    it("should return 0 for invalid indices") {
        Setup();
        expect(MergeItemIntoStack(-1, 0) == 0);
        expect(MergeItemIntoStack(0, -1) == 0);
        expect(MergeItemIntoStack(MAX_ITEMS, 0) == 0);
    }

    it("should return 0 when merging item with itself") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;

        int merged = MergeItemIntoStack(a, a);

        expect(merged == 0);
        expect(items[a].stackCount == 3);
    }

    it("should return 0 for inactive items") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        int b = SpawnItem(16, 16, 0, ITEM_RED);
        items[b].active = false;

        expect(MergeItemIntoStack(a, b) == 0);
    }
}

// ===========================================================================
// SplitStack tests
// ===========================================================================
describe(split_stack) {
    it("should split off requested count") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 5;

        int b = SplitStack(a, 2);

        expect(b >= 0);
        expect(items[a].stackCount == 3);
        expect(items[b].stackCount == 2);
        expect(items[b].active);
        expect(items[b].type == ITEM_RED);
    }

    it("should copy position from original") {
        Setup();
        int a = SpawnItem(48, 80, 0, ITEM_LOG);
        items[a].stackCount = 4;
        items[a].material = MAT_OAK;

        int b = SplitStack(a, 1);

        expect(b >= 0);
        expect(items[b].x == items[a].x);
        expect(items[b].y == items[a].y);
        expect(items[b].z == items[a].z);
        expect(items[b].material == MAT_OAK);
    }

    it("should inherit state from original") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;
        items[a].state = ITEM_IN_STOCKPILE;

        int b = SplitStack(a, 1);

        expect(b >= 0);
        expect(items[b].state == ITEM_IN_STOCKPILE);
    }

    it("should fail when count is 0") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;

        expect(SplitStack(a, 0) == -1);
        expect(items[a].stackCount == 3);
    }

    it("should fail when count equals stack") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;

        expect(SplitStack(a, 3) == -1);
        expect(items[a].stackCount == 3);
    }

    it("should fail when count exceeds stack") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;

        expect(SplitStack(a, 4) == -1);
        expect(items[a].stackCount == 3);
    }

    it("should fail for invalid index") {
        Setup();
        expect(SplitStack(-1, 1) == -1);
        expect(SplitStack(MAX_ITEMS, 1) == -1);
    }

    it("should fail for inactive item") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 3;
        items[a].active = false;

        expect(SplitStack(a, 1) == -1);
    }
}

// ===========================================================================
// GetItemStackCount / default stackCount tests
// ===========================================================================
describe(stack_count_basics) {
    it("should default to 1 for newly spawned items") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);

        expect(items[a].stackCount == 1);
        expect(GetItemStackCount(a) == 1);
    }

    it("should return correct value after manual set") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 7;

        expect(GetItemStackCount(a) == 7);
    }
}

// ===========================================================================
// Stockpile stacking integration tests
// ===========================================================================
describe(stockpile_stacking) {
    it("should merge items when placed in occupied slot") {
        Setup();
        // Create stockpile and place an item
        int sp = CreateStockpile(1, 1, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_RED, true);

        int a = SpawnItem(1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, ITEM_RED);
        items[a].stackCount = 3;
        PlaceItemInStockpile(sp, 1, 1, a);

        expect(GetStockpileSlotCount(sp, 1, 1) == 3);

        // Place second item — should merge
        int b = SpawnItem(1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, ITEM_RED);
        items[b].stackCount = 2;
        PlaceItemInStockpile(sp, 1, 1, b);

        expect(GetStockpileSlotCount(sp, 1, 1) == 5);
        expect(!items[b].active);  // merged into a
        expect(items[a].stackCount == 5);
    }

    it("should not merge different materials") {
        Setup();
        int sp = CreateStockpile(1, 1, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_LOG, true);

        int a = SpawnItemWithMaterial(1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, ITEM_LOG, MAT_OAK);
        items[a].stackCount = 2;
        PlaceItemInStockpile(sp, 1, 1, a);

        int b = SpawnItemWithMaterial(1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, ITEM_LOG, MAT_PINE);
        items[b].stackCount = 1;
        PlaceItemInStockpile(sp, 1, 1, b);

        // Should not merge — material mismatch
        expect(GetStockpileSlotCount(sp, 1, 1) == 2);  // still just a's count
        expect(items[b].active);  // b not deleted
    }

    it("should report overfull when slot exceeds max") {
        Setup();
        int sp = CreateStockpile(1, 1, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_RED, true);
        SetStockpileMaxStackSize(sp, 3);
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 5);

        expect(IsSlotOverfull(sp, 1, 1));
        expect(GetStockpileSlotCount(sp, 1, 1) == 5);
    }

    it("should not report overfull when at or below max") {
        Setup();
        int sp = CreateStockpile(1, 1, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_RED, true);
        SetStockpileMaxStackSize(sp, 5);
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 5);

        expect(!IsSlotOverfull(sp, 1, 1));
    }

    it("SetStockpileSlotCount should create representative item") {
        Setup();
        int sp = CreateStockpile(1, 1, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_RED, true);
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 4);

        Stockpile* s = &stockpiles[sp];
        int repIdx = s->slots[0];
        expect(repIdx >= 0);
        expect(items[repIdx].active);
        expect(items[repIdx].stackCount == 4);
        expect(items[repIdx].type == ITEM_RED);
        expect(items[repIdx].state == ITEM_IN_STOCKPILE);
    }
}

// ===========================================================================
// Merge + Split roundtrip
// ===========================================================================
describe(roundtrip) {
    it("should preserve total count through split then merge") {
        Setup();
        int a = SpawnItem(16, 16, 0, ITEM_RED);
        items[a].stackCount = 8;

        int b = SplitStack(a, 3);
        expect(items[a].stackCount == 5);
        expect(items[b].stackCount == 3);

        int merged = MergeItemIntoStack(a, b);
        expect(merged == 3);
        expect(items[a].stackCount == 8);
        expect(!items[b].active);
    }
}

// ===========================================================================
// Craft pickup should split stacks
// ===========================================================================
describe(craft_pickup_split) {
    it("should split stack when recipe needs fewer than stackCount") {
        // Story: Sawmill has "Build Chest" bill (needs 4 planks).
        // Stockpile has a stack of 10 planks. Mover picks up for crafting.
        // Expected: mover carries 4 planks, stockpile keeps 6.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create sawmill at (5,1) — "Build Chest" is recipe index 2
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);

        // Create a stockpile with 10 planks in one slot
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_PLANKS, true);
        float slotX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        int plankIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 10;
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_PLANKS, MAT_OAK, &freeX, &freeY);
        PlaceItemInStockpile(sp, freeX, freeY, plankIdx);

        // Verify stockpile has the stack
        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;
        expect(spPtr->slots[slotIdx] == plankIdx);
        expect(spPtr->slotCounts[slotIdx] == 10);

        // Set up mover right next to the planks (so pickup is immediate)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, slotX, slotY, 0.0f, goal, 200.0f);
        moverCount = 1;

        // Create craft job manually at PICKING_UP step
        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;  // Will look up recipe from bill
        job->targetItem = plankIdx;
        job->carryingItem = -1;
        job->fuelItem = -1;
        job->step = CRAFT_STEP_PICKING_UP;
        m->currentJobId = jobId;

        // Add "Build Chest" bill (recipe index 2 at sawmill)
        AddBill(wsIdx, 2, BILL_DO_X_TIMES, 0);
        workshops[wsIdx].assignedCrafter = 0;

        // Run one step of the craft job
        JobRunResult result = RunJob_Craft(job, m, TICK_DT);

        // Job should be running (moved to next step)
        expect(result == JOBRUN_RUNNING);
        expect(job->step == CRAFT_STEP_MOVING_TO_WORKSHOP);

        // Mover should be carrying 4 planks (recipe inputCount)
        int carriedIdx = job->carryingItem;
        expect(carriedIdx >= 0);
        expect(items[carriedIdx].active == true);
        expect(items[carriedIdx].type == ITEM_PLANKS);
        expect(items[carriedIdx].stackCount == 4);
        expect(items[carriedIdx].state == ITEM_CARRIED);

        // Stockpile should still have 6 planks remaining
        expect(spPtr->slots[slotIdx] >= 0);
        int remainIdx = spPtr->slots[slotIdx];
        expect(items[remainIdx].active == true);
        expect(items[remainIdx].stackCount == 6);
        expect(spPtr->slotCounts[slotIdx] == 6);
    }

    it("should take whole stack when recipe needs exactly stackCount") {
        // Stockpile has exactly 4 planks, recipe needs 4. No split needed.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);

        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_PLANKS, true);
        float slotX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        int plankIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 4;
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_PLANKS, MAT_OAK, &freeX, &freeY);
        PlaceItemInStockpile(sp, freeX, freeY, plankIdx);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, slotX, slotY, 0.0f, goal, 200.0f);
        moverCount = 1;

        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->targetItem = plankIdx;
        job->carryingItem = -1;
        job->fuelItem = -1;
        job->step = CRAFT_STEP_PICKING_UP;
        m->currentJobId = jobId;

        AddBill(wsIdx, 2, BILL_DO_X_TIMES, 0);
        workshops[wsIdx].assignedCrafter = 0;

        JobRunResult result = RunJob_Craft(job, m, TICK_DT);

        expect(result == JOBRUN_RUNNING);
        // Mover carries whole stack (4 planks)
        int carriedIdx = job->carryingItem;
        expect(carriedIdx >= 0);
        expect(items[carriedIdx].stackCount == 4);

        // Stockpile slot should be cleared
        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;
        expect(spPtr->slots[slotIdx] == -1);
        expect(spPtr->slotCounts[slotIdx] == 0);
    }
}

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

    test(merge_into_stack);
    test(split_stack);
    test(stack_count_basics);
    test(stockpile_stacking);
    test(roundtrip);
    test(craft_pickup_split);

    return summary();
}
