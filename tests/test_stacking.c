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
        // Find free slot first, then spawn item at that slot's position
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_PLANKS, MAT_OAK, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int plankIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 10;
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

        // Add "Build Chest" bill (recipe index 4 at sawmill)
        AddBill(wsIdx, 4, BILL_DO_X_TIMES, 0);
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
        // Find free slot first, then spawn item at that slot's position
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_PLANKS, MAT_OAK, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int plankIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 4;
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

        AddBill(wsIdx, 4, BILL_DO_X_TIMES, 0);
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

// ===========================================================================
// TakeFromStockpileSlot
// ===========================================================================
describe(take_from_stockpile_slot) {
    it("should split stack and return split-off when taking partial") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n", 10, 10);
        ClearMovers();
        ClearItems();
        ClearStockpiles();

        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_BERRIES, true);
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_BERRIES, MAT_NONE, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_BERRIES, MAT_NONE);
        items[berryIdx].stackCount = 10;
        PlaceItemInStockpile(sp, freeX, freeY, berryIdx);

        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;

        int taken = TakeFromStockpileSlot(berryIdx, 3);

        // Should return a different item (the split-off)
        expect(taken >= 0);
        expect(taken != berryIdx);
        expect(items[taken].type == ITEM_BERRIES);
        expect(items[taken].stackCount == 3);
        expect(items[taken].state == ITEM_ON_GROUND);

        // Original stays in stockpile with reduced count
        expect(items[berryIdx].active == true);
        expect(items[berryIdx].stackCount == 7);
        expect(items[berryIdx].state == ITEM_IN_STOCKPILE);
        expect(spPtr->slots[slotIdx] == berryIdx);
        expect(spPtr->slotCounts[slotIdx] == 7);
    }

    it("should take whole item and clear slot when taking all") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n", 10, 10);
        ClearMovers();
        ClearItems();
        ClearStockpiles();

        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_BERRIES, true);
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_BERRIES, MAT_NONE, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_BERRIES, MAT_NONE);
        items[berryIdx].stackCount = 5;
        PlaceItemInStockpile(sp, freeX, freeY, berryIdx);

        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;

        int taken = TakeFromStockpileSlot(berryIdx, 5);

        // Returns same item
        expect(taken == berryIdx);
        expect(items[taken].stackCount == 5);
        // Slot should be cleared
        expect(spPtr->slots[slotIdx] == -1);
        expect(spPtr->slotCounts[slotIdx] == 0);
    }

    it("should transfer reservation from original to split-off") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n", 10, 10);
        ClearMovers();
        ClearItems();
        ClearStockpiles();

        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_BERRIES, true);
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_BERRIES, MAT_NONE, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_BERRIES, MAT_NONE);
        items[berryIdx].stackCount = 10;
        items[berryIdx].reservedBy = 3; // reserved by mover 3
        PlaceItemInStockpile(sp, freeX, freeY, berryIdx);

        int taken = TakeFromStockpileSlot(berryIdx, 2);

        // Split-off gets the reservation
        expect(taken >= 0);
        expect(items[taken].reservedBy == 3);
        // Original is now unreserved (available for other jobs)
        expect(items[berryIdx].reservedBy == -1);
    }

    it("should pass through non-stockpile items unchanged") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n", 10, 10);
        ClearItems();

        int idx = SpawnItem(16.0f, 16.0f, 0.0f, ITEM_BERRIES);
        items[idx].stackCount = 5;
        // Item is ON_GROUND, not in stockpile

        int taken = TakeFromStockpileSlot(idx, 3);

        // Should return same item, no split (not in stockpile)
        expect(taken == idx);
        expect(items[idx].stackCount == 5);
    }
}

// ===========================================================================
// Deliver-to-workshop pickup should split stacks (takes 1 per trip)
// ===========================================================================
describe(deliver_to_workshop_split) {
    it("should take only 1 from stockpile stack for passive workshop delivery") {
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

        // Create drying rack at (5,1)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_DRYING_RACK);

        // Create stockpile with 10 berries
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_BERRIES, true);
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_BERRIES, MAT_NONE, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_BERRIES, MAT_NONE);
        items[berryIdx].stackCount = 10;
        PlaceItemInStockpile(sp, freeX, freeY, berryIdx);

        // Set up mover at the berry location
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, slotX, slotY, 0.0f, goal, 200.0f);
        moverCount = 1;

        // Create deliver job manually at pickup step
        AddBill(wsIdx, 1, BILL_DO_FOREVER, 0); // recipe 1 = "Dry Berries" (needs 3)
        int jobId = CreateJob(JOBTYPE_DELIVER_TO_WORKSHOP);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->targetItem = berryIdx;
        job->carryingItem = -1;
        job->step = STEP_MOVING_TO_PICKUP;
        m->currentJobId = jobId;

        // Run until pickup happens
        JobRunResult result = RunJob_DeliverToWorkshop(job, m, TICK_DT);

        // Mover should be carrying 1 berry
        expect(result == JOBRUN_RUNNING);
        int carriedIdx = job->carryingItem;
        expect(carriedIdx >= 0);
        expect(items[carriedIdx].stackCount == 1);

        // Stockpile should keep 9
        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;
        expect(spPtr->slots[slotIdx] == berryIdx);
        expect(items[berryIdx].stackCount == 9);
        expect(spPtr->slotCounts[slotIdx] == 9);
        // Original should be unreserved (available for more deliveries)
        expect(items[berryIdx].reservedBy == -1);
    }
}

// ===========================================================================
// Passive workshop consumption with stacked items
// ===========================================================================
describe(passive_consumption_stacks) {
    it("should consume correct units from a single stacked item on work tile") {
        // Drying rack "Dry Berries": needs 3 berries, outputs 2 dried berries
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();

        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_DRYING_RACK);
        Workshop* ws = &workshops[wsIdx];
        AddBill(wsIdx, 1, BILL_DO_FOREVER, 0); // "Dry Berries": 3 berries -> 2 dried berries

        // Place 5 berries on work tile (recipe needs 3)
        float wx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float wy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItem(wx, wy, 0.0f, ITEM_BERRIES);
        items[berryIdx].stackCount = 5;

        // Force workshop to be ready and almost done
        ws->passiveBillIdx = 0;
        ws->passiveProgress = 0.99f;

        // Tick to complete
        PassiveWorkshopsTick(0.2f);

        // Should have consumed 3, left 2
        // The original item was deleted; a split-off with 2 should remain
        int berriesRemaining = 0;
        int driedBerriesCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].type == ITEM_BERRIES) berriesRemaining += items[i].stackCount;
            if (items[i].type == ITEM_DRIED_BERRIES) driedBerriesCount += items[i].stackCount;
        }
        expect(berriesRemaining == 2);
        expect(driedBerriesCount == 2);
    }

    it("should consume exact stack without leftovers") {
        // 3 berries on tile, recipe needs 3 — all consumed, nothing left
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();

        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_DRYING_RACK);
        Workshop* ws = &workshops[wsIdx];
        AddBill(wsIdx, 1, BILL_DO_FOREVER, 0);

        float wx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float wy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItem(wx, wy, 0.0f, ITEM_BERRIES);
        items[berryIdx].stackCount = 3;

        ws->passiveBillIdx = 0;
        ws->passiveProgress = 0.99f;

        PassiveWorkshopsTick(0.2f);

        int berriesRemaining = 0;
        int driedBerriesCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].type == ITEM_BERRIES) berriesRemaining += items[i].stackCount;
            if (items[i].type == ITEM_DRIED_BERRIES) driedBerriesCount += items[i].stackCount;
        }
        expect(berriesRemaining == 0);
        expect(driedBerriesCount == 2);
    }

    it("should consume across multiple stacked items on work tile") {
        // Two berry items (stackCount=1 each + stackCount=2), recipe needs 3
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();

        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_DRYING_RACK);
        Workshop* ws = &workshops[wsIdx];
        AddBill(wsIdx, 1, BILL_DO_FOREVER, 0);

        float wx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float wy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        int b1 = SpawnItem(wx, wy, 0.0f, ITEM_BERRIES);
        items[b1].stackCount = 1;
        int b2 = SpawnItem(wx, wy, 0.0f, ITEM_BERRIES);
        items[b2].stackCount = 2;

        ws->passiveBillIdx = 0;
        ws->passiveProgress = 0.99f;

        PassiveWorkshopsTick(0.2f);

        int berriesRemaining = 0;
        int driedBerriesCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].type == ITEM_BERRIES) berriesRemaining += items[i].stackCount;
            if (items[i].type == ITEM_DRIED_BERRIES) driedBerriesCount += items[i].stackCount;
        }
        expect(berriesRemaining == 0);
        expect(driedBerriesCount == 2);
    }

    it("should not start passive workshop with insufficient stacked units") {
        // 2 berries on tile (stackCount=2), recipe needs 3 — should stall
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();

        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_DRYING_RACK);
        Workshop* ws = &workshops[wsIdx];
        AddBill(wsIdx, 1, BILL_DO_FOREVER, 0);

        float wx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float wy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        int berryIdx = SpawnItem(wx, wy, 0.0f, ITEM_BERRIES);
        items[berryIdx].stackCount = 2;

        ws->passiveBillIdx = 0;
        ws->passiveProgress = 0.0f;

        // Tick — should not advance (only 2 units, need 3)
        PassiveWorkshopsTick(1.0f);
        expect(ws->passiveProgress == 0.0f);
    }
}

// ===========================================================================
// Active craft (sawmill) pickup splits with inputCount > 1
// ===========================================================================
describe(active_craft_split) {
    it("should split stack for active craft with multi-input recipe") {
        // Sawmill "Build Chest": needs PLANKS x4
        // Stockpile has 10 planks, mover should take 4, leave 6
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
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_PLANKS, MAT_OAK, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int plankIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 10;
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

        // recipe index 4 = "Build Chest" (PLANKS x4 -> CHEST x1)
        AddBill(wsIdx, 4, BILL_DO_X_TIMES, 0);
        workshops[wsIdx].assignedCrafter = 0;

        JobRunResult result = RunJob_Craft(job, m, TICK_DT);

        expect(result == JOBRUN_RUNNING);

        int carriedIdx = job->carryingItem;
        expect(carriedIdx >= 0);
        expect(items[carriedIdx].stackCount == 4);
        expect(items[carriedIdx].state == ITEM_CARRIED);

        // Stockpile keeps 6
        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;
        expect(spPtr->slots[slotIdx] == plankIdx);
        expect(items[plankIdx].stackCount == 6);
        expect(spPtr->slotCounts[slotIdx] == 6);
    }

    it("should split stack for active craft with single input recipe") {
        // Sawmill "Saw Planks": needs LOG x1
        // Stockpile has 5 logs, mover should take 1, leave 4
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
        SetStockpileFilter(sp, ITEM_LOG, true);
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_LOG, MAT_OAK, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int logIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_LOG, MAT_OAK);
        items[logIdx].stackCount = 5;
        PlaceItemInStockpile(sp, freeX, freeY, logIdx);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, slotX, slotY, 0.0f, goal, 200.0f);
        moverCount = 1;

        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->targetItem = logIdx;
        job->carryingItem = -1;
        job->fuelItem = -1;
        job->step = CRAFT_STEP_PICKING_UP;
        m->currentJobId = jobId;

        // recipe index 0 = "Saw Planks" (LOG x1 -> PLANKS x4)
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 0);
        workshops[wsIdx].assignedCrafter = 0;

        JobRunResult result = RunJob_Craft(job, m, TICK_DT);

        expect(result == JOBRUN_RUNNING);

        int carriedIdx = job->carryingItem;
        expect(carriedIdx >= 0);
        expect(items[carriedIdx].stackCount == 1);

        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;
        expect(items[logIdx].stackCount == 4);
        expect(spPtr->slotCounts[slotIdx] == 4);
    }
}

// ===========================================================================
// Semi-passive workshop (charcoal pit) consumption with stacks
// ===========================================================================
describe(semi_passive_consumption_stacks) {
    it("should consume correct units from stacked item for charcoal pit") {
        // Charcoal pit "Char Sticks": needs STICKS x4, outputs CHARCOAL x2
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();

        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_CHARCOAL_PIT);
        Workshop* ws = &workshops[wsIdx];
        // recipe index 2 = "Char Sticks" (STICKS x4 -> CHARCOAL x1)
        AddBill(wsIdx, 2, BILL_DO_FOREVER, 0);

        float wx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float wy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        int sticksIdx = SpawnItem(wx, wy, 0.0f, ITEM_STICKS);
        items[sticksIdx].stackCount = 7;

        // Semi-passive: set passiveReady=true (crafter already ignited)
        ws->passiveBillIdx = 0;
        ws->passiveProgress = 0.99f;
        ws->passiveReady = true;

        PassiveWorkshopsTick(1.0f);

        // Should consume 4 sticks, leave 3
        int sticksRemaining = 0;
        int charcoalCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].type == ITEM_STICKS) sticksRemaining += items[i].stackCount;
            if (items[i].type == ITEM_CHARCOAL) charcoalCount += items[i].stackCount;
        }
        expect(sticksRemaining == 3);
        expect(charcoalCount == 1);
    }
}

// ===========================================================================
// Hearth (active, ITEM_MATCH_ANY_FUEL) craft pickup splits fuel stack
// ===========================================================================
describe(hearth_fuel_split) {
    it("should take only 1 fuel from stack for hearth craft") {
        // Hearth "Burn Fuel": needs 1 fuel item
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

        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_HEARTH);

        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_CHARCOAL, true);
        int freeX, freeY;
        FindFreeStockpileSlot(sp, ITEM_CHARCOAL, MAT_NONE, &freeX, &freeY);
        float slotX = freeX * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = freeY * CELL_SIZE + CELL_SIZE * 0.5f;
        int charIdx = SpawnItemWithMaterial(slotX, slotY, 0.0f, ITEM_CHARCOAL, MAT_NONE);
        items[charIdx].stackCount = 8;
        PlaceItemInStockpile(sp, freeX, freeY, charIdx);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, slotX, slotY, 0.0f, goal, 200.0f);
        moverCount = 1;

        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->targetItem = charIdx;
        job->carryingItem = -1;
        job->fuelItem = -1;
        job->step = CRAFT_STEP_PICKING_UP;
        m->currentJobId = jobId;

        // recipe index 0 = "Burn Fuel" (ANY_FUEL x1 -> ASH x1)
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 0);
        workshops[wsIdx].assignedCrafter = 0;

        JobRunResult result = RunJob_Craft(job, m, TICK_DT);

        expect(result == JOBRUN_RUNNING);

        int carriedIdx = job->carryingItem;
        expect(carriedIdx >= 0);
        expect(items[carriedIdx].stackCount == 1);

        // Stockpile keeps 7
        Stockpile* spPtr = &stockpiles[sp];
        int lx = freeX - spPtr->x;
        int ly = freeY - spPtr->y;
        int slotIdx = ly * spPtr->width + lx;
        expect(items[charIdx].stackCount == 7);
        expect(spPtr->slotCounts[slotIdx] == 7);
    }
}

// ===========================================================================
// End-to-end: craft chest from stacked planks
// ===========================================================================
describe(e2e_craft_chest_from_stack) {
    it("should consume exactly 4 planks from stack of 8 and produce 1 chest") {
        // Full setup: grid, movers, stockpiles, workshop, items
        InitTestGrid(12, 12);
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearMovers();
        ClearJobs();
        InitJobSystem(MAX_MOVERS);
        InitDesignations();
        moverPathAlgorithm = PATH_ALGO_ASTAR;

        int z = 0;

        // Solid ground everywhere at z=0 (walkable surface)
        // Items/movers/workshops operate at z=0 with floor support from below
        // Actually we need z=1 walkable with z=0 solid
        for (int y = 0; y < 12; y++)
            for (int x = 0; x < 12; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;
            }
        z = 1;

        // Create sawmill at (2,2) — work tile at (3,3), output at (3,2)
        int wsIdx = CreateWorkshop(2, 2, z, WORKSHOP_SAWMILL);
        // Build Chest = recipe index 4, needs 4 planks
        AddBill(wsIdx, 4, BILL_DO_FOREVER, 0);

        // Stockpile for planks at (6,2) — 1x1
        int spPlanks = CreateStockpile(6, 2, z, 1, 1);
        SetStockpileFilter(spPlanks, ITEM_PLANKS, true);

        // Place 8 planks as a single stack in the stockpile
        int plankIdx = SpawnItemWithMaterial(
            6 * CELL_SIZE + 16, 2 * CELL_SIZE + 16, z, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 8;
        PlaceItemInStockpile(spPlanks, 6, 2, plankIdx);
        expect(GetStockpileSlotCount(spPlanks, 6, 2) == 8);

        // Stockpile for chests at (8,2) — 1x1
        int spChest = CreateStockpile(8, 2, z, 1, 1);
        SetStockpileFilter(spChest, ITEM_CHEST, true);

        // Create a mover at (1,1) that can haul
        Mover* m = &movers[0];
        Point goal = {1, 1, z};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, (float)z, goal, 100.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;
        AddMoverToIdleList(0);

        // Run simulation until a chest appears or timeout
        bool chestFound = false;
        for (int tick = 0; tick < 10000; tick++) {
            Tick();
            AssignJobs();
            JobsTick();

            // Check if a chest exists
            for (int i = 0; i < itemHighWaterMark; i++) {
                if (items[i].active && items[i].type == ITEM_CHEST) {
                    chestFound = true;
                    break;
                }
            }
            if (chestFound) break;
        }

        expect(chestFound);

        // Count remaining plank units
        int plankUnits = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_PLANKS) {
                plankUnits += items[i].stackCount;
            }
        }
        // Started with 8, used 4 for 1 chest = 4 remaining
        expect(plankUnits == 4);

        // Count chests
        int chestCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_CHEST) {
                chestCount++;
            }
        }
        expect(chestCount >= 1);

        if (test_verbose) {
            printf("  Planks remaining: %d, Chests: %d\n", plankUnits, chestCount);
        }
    }
}

// ===========================================================================
// Helper: set up a 12x12 grid with z=0 solid, z=1 walkable, 1 mover
// ===========================================================================
static void SetupE2E(void) {
    InitTestGrid(12, 12);
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearMovers();
    ClearJobs();
    InitJobSystem(MAX_MOVERS);
    InitDesignations();
    moverPathAlgorithm = PATH_ALGO_ASTAR;

    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
        }
}

static void SpawnE2EMover(int tileX, int tileY, int z) {
    Mover* m = &movers[0];
    Point goal = {tileX, tileY, z};
    InitMover(m, CELL_SIZE * (tileX + 0.5f), CELL_SIZE * (tileY + 0.5f), (float)z, goal, 100.0f);
    m->capabilities.canHaul = true;
    moverCount = 1;
    AddMoverToIdleList(0);
}

static int CountItemUnits(ItemType type) {
    int total = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type)
            total += items[i].stackCount;
    }
    return total;
}

// ===========================================================================
// E2E: passive delivery split (drying rack + berries)
// ===========================================================================
describe(e2e_passive_delivery_split) {
    it("should deliver 3 berries from stack of 10 and dry them, leaving 7") {
        SetupE2E();
        int z = 1;

        // Drying rack at (4,4) — work tile (5,4), output tile (4,5)
        int wsIdx = CreateWorkshop(4, 4, z, WORKSHOP_DRYING_RACK);
        // Recipe 1 = "Dry Berries": 3 berries -> 2 dried berries, passiveTime=10
        AddBill(wsIdx, 1, BILL_DO_X_TIMES, 1);

        // Stockpile with 10 berries at (8,4)
        int spIn = CreateStockpile(8, 4, z, 1, 1);
        SetStockpileFilter(spIn, ITEM_BERRIES, true);
        int berryIdx = SpawnItem(8 * CELL_SIZE + 16, 4 * CELL_SIZE + 16, z, ITEM_BERRIES);
        items[berryIdx].stackCount = 10;
        PlaceItemInStockpile(spIn, 8, 4, berryIdx);

        // Output stockpile at (8,6)
        int spOut = CreateStockpile(8, 6, z, 1, 1);
        SetStockpileFilter(spOut, ITEM_DRIED_BERRIES, true);

        SpawnE2EMover(1, 4, z);

        // Run until dried berries appear or timeout
        bool found = false;
        for (int t = 0; t < 20000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (CountItemUnits(ITEM_DRIED_BERRIES) > 0) { found = true; break; }
        }

        expect(found);
        expect(CountItemUnits(ITEM_BERRIES) == 7);
        expect(CountItemUnits(ITEM_DRIED_BERRIES) == 2);

        if (test_verbose) {
            printf("  Berries remaining: %d, Dried: %d\n",
                   CountItemUnits(ITEM_BERRIES), CountItemUnits(ITEM_DRIED_BERRIES));
        }
    }
}

// ===========================================================================
// E2E: semi-passive delivery (charcoal pit + sticks)
// ===========================================================================
describe(e2e_semi_passive_sticks) {
    it("should deliver 4 sticks from stack of 10, char them, leaving 6") {
        SetupE2E();
        int z = 1;

        // Charcoal pit at (4,4) — fuel(4,4), work(5,4), output(4,5)
        int wsIdx = CreateWorkshop(4, 4, z, WORKSHOP_CHARCOAL_PIT);
        // Recipe 2 = "Char Sticks": 4 sticks -> 1 charcoal, work=2, passive=40
        AddBill(wsIdx, 2, BILL_DO_X_TIMES, 1);

        // Stockpile with 10 sticks at (8,4)
        int spIn = CreateStockpile(8, 4, z, 1, 1);
        SetStockpileFilter(spIn, ITEM_STICKS, true);
        int stickIdx = SpawnItem(8 * CELL_SIZE + 16, 4 * CELL_SIZE + 16, z, ITEM_STICKS);
        items[stickIdx].stackCount = 10;
        PlaceItemInStockpile(spIn, 8, 4, stickIdx);

        // Output stockpile at (8,6)
        int spOut = CreateStockpile(8, 6, z, 1, 1);
        SetStockpileFilter(spOut, ITEM_CHARCOAL, true);

        SpawnE2EMover(1, 4, z);

        // Run until charcoal appears or timeout
        bool found = false;
        for (int t = 0; t < 50000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (CountItemUnits(ITEM_CHARCOAL) > 0) { found = true; break; }
        }

        expect(found);
        expect(CountItemUnits(ITEM_STICKS) == 6);
        expect(CountItemUnits(ITEM_CHARCOAL) >= 1);

        if (test_verbose) {
            printf("  Sticks remaining: %d, Charcoal: %d\n",
                   CountItemUnits(ITEM_STICKS), CountItemUnits(ITEM_CHARCOAL));
        }
    }
}

// ===========================================================================
// E2E: hearth fuel from stack
// ===========================================================================
describe(e2e_hearth_fuel) {
    it("should burn 1 charcoal from stack of 8 and produce ash") {
        SetupE2E();
        int z = 1;

        // Hearth at (4,4) — fuel(4,4), work(5,4), output(4,5)
        int wsIdx = CreateWorkshop(4, 4, z, WORKSHOP_HEARTH);
        // Recipe 0 = "Burn Fuel": 1 ANY_FUEL -> 1 ash
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);

        // Stockpile with 8 charcoal at (8,4)
        int spIn = CreateStockpile(8, 4, z, 1, 1);
        SetStockpileFilter(spIn, ITEM_CHARCOAL, true);
        int charIdx = SpawnItem(8 * CELL_SIZE + 16, 4 * CELL_SIZE + 16, z, ITEM_CHARCOAL);
        items[charIdx].stackCount = 8;
        PlaceItemInStockpile(spIn, 8, 4, charIdx);

        // Output stockpile at (8,6)
        int spOut = CreateStockpile(8, 6, z, 1, 1);
        SetStockpileFilter(spOut, ITEM_ASH, true);

        SpawnE2EMover(1, 4, z);

        // Run until ash appears
        bool found = false;
        for (int t = 0; t < 20000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (CountItemUnits(ITEM_ASH) > 0) { found = true; break; }
        }

        expect(found);
        expect(CountItemUnits(ITEM_CHARCOAL) == 7);
        expect(CountItemUnits(ITEM_ASH) == 1);

        if (test_verbose) {
            printf("  Charcoal remaining: %d, Ash: %d\n",
                   CountItemUnits(ITEM_CHARCOAL), CountItemUnits(ITEM_ASH));
        }
    }
}

// ===========================================================================
// E2E: auto-resume passive bill after output stockpile created
// ===========================================================================
describe(e2e_auto_resume_passive) {
    it("should suspend bill when no output stockpile, resume when one is created") {
        SetupE2E();
        int z = 1;

        // Drying rack at (4,4)
        int wsIdx = CreateWorkshop(4, 4, z, WORKSHOP_DRYING_RACK);
        // Recipe 0 = "Dry Grass": 1 grass -> 1 dried grass
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);

        // Input stockpile with grass
        int spIn = CreateStockpile(8, 4, z, 1, 1);
        SetStockpileFilter(spIn, ITEM_GRASS, true);
        int grassIdx = SpawnItem(8 * CELL_SIZE + 16, 4 * CELL_SIZE + 16, z, ITEM_GRASS);
        items[grassIdx].stackCount = 5;
        PlaceItemInStockpile(spIn, 8, 4, grassIdx);

        SpawnE2EMover(1, 4, z);

        // Run enough for delivery + drying to complete (no output stockpile!)
        // The grass should get delivered to work tile and dried,
        // but then the bill should auto-suspend because there's no output stockpile
        bool suspended = false;
        for (int t = 0; t < 20000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (workshops[wsIdx].bills[0].suspended && workshops[wsIdx].bills[0].suspendedNoStorage) {
                suspended = true;
                break;
            }
        }
        expect(suspended);

        // Now create an output stockpile
        int spOut = CreateStockpile(8, 6, z, 1, 1);
        SetStockpileFilter(spOut, ITEM_DRIED_GRASS, true);

        // Run more ticks — bill should auto-resume via PassiveWorkshopsTick
        bool resumed = false;
        for (int t = 0; t < 5000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!workshops[wsIdx].bills[0].suspended) {
                resumed = true;
                break;
            }
        }
        expect(resumed);

        if (test_verbose) {
            printf("  Suspended then resumed: %s\n", resumed ? "yes" : "no");
        }
    }
}

// ===========================================================================
// E2E: re-haul after stockpile cell deleted drops stack to ground
// ===========================================================================
describe(e2e_rehaul_after_drop) {
    it("should re-haul items to another stockpile after source stockpile deleted") {
        SetupE2E();
        int z = 1;

        // Two stockpiles: one with items, one empty
        int sp1 = CreateStockpile(4, 4, z, 1, 1);
        SetStockpileFilter(sp1, ITEM_PLANKS, true);
        int plankIdx = SpawnItemWithMaterial(
            4 * CELL_SIZE + 16, 4 * CELL_SIZE + 16, z, ITEM_PLANKS, MAT_OAK);
        items[plankIdx].stackCount = 5;
        PlaceItemInStockpile(sp1, 4, 4, plankIdx);
        expect(GetStockpileSlotCount(sp1, 4, 4) == 5);

        int sp2 = CreateStockpile(8, 4, z, 1, 1);
        SetStockpileFilter(sp2, ITEM_PLANKS, true);

        SpawnE2EMover(1, 4, z);

        // Delete stockpile 1 — items should drop to ground
        RemoveStockpileCells(sp1, 4, 4, 4, 4);

        expect(items[plankIdx].active);
        expect(items[plankIdx].state == ITEM_ON_GROUND);
        expect(items[plankIdx].stackCount == 5);

        // Run simulation — mover should haul the dropped stack to sp2
        bool hauled = false;
        for (int t = 0; t < 10000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[plankIdx].state == ITEM_IN_STOCKPILE) {
                hauled = true;
                break;
            }
        }

        expect(hauled);
        // Verify planks are in stockpile 2 with full stack
        expect(GetStockpileSlotCount(sp2, 8, 4) == 5);

        if (test_verbose) {
            printf("  Re-hauled: %s, slot count: %d\n",
                   hauled ? "yes" : "no", GetStockpileSlotCount(sp2, 8, 4));
        }
    }
}

// ===========================================================================
// E2E: individual planks hauled + crafted (reproduces headless plank bug)
// ===========================================================================
describe(e2e_individual_planks_craft) {
    it("should haul 8 individual planks, craft 1 chest, leave exactly 4 planks") {
        SetupE2E();
        int z = 1;

        // Sawmill at (2,2)
        int wsIdx = CreateWorkshop(2, 2, z, WORKSHOP_SAWMILL);
        // Build Chest = recipe 4, needs 4 planks
        AddBill(wsIdx, 4, BILL_DO_X_TIMES, 1);

        // Stockpile for planks at (6,2)
        int spPlanks = CreateStockpile(6, 2, z, 1, 1);
        SetStockpileFilter(spPlanks, ITEM_PLANKS, true);

        // Stockpile for chests at (8,2)
        int spChest = CreateStockpile(8, 2, z, 1, 1);
        SetStockpileFilter(spChest, ITEM_CHEST, true);

        // Spawn 8 INDIVIDUAL planks on the ground (stackCount=1 each)
        for (int i = 0; i < 8; i++) {
            SpawnItemWithMaterial(
                10 * CELL_SIZE + 16, 2 * CELL_SIZE + 16, z, ITEM_PLANKS, MAT_OAK);
        }
        expect(CountItemUnits(ITEM_PLANKS) == 8);

        SpawnE2EMover(1, 2, z);

        // Run until a chest appears
        bool chestFound = false;
        for (int t = 0; t < 30000; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (CountItemUnits(ITEM_CHEST) > 0) { chestFound = true; break; }
        }

        int planksLeft = CountItemUnits(ITEM_PLANKS);
        int chests = CountItemUnits(ITEM_CHEST);

        expect(chestFound);
        // 8 planks - 4 for chest = 4 remaining
        expect(planksLeft == 4);
        expect(chests >= 1);

        if (test_verbose) {
            printf("  Planks remaining: %d, Chests: %d\n", planksLeft, chests);
        }
    }
}

// ===========================================================================
// freeSlotCount after RemoveStockpileCells
// ===========================================================================
describe(free_slot_count_after_remove) {
    it("should update freeSlotCount when cells are removed") {
        Setup();
        // Create 2x1 stockpile
        int sp = CreateStockpile(1, 1, 0, 2, 1);
        SetStockpileFilter(sp, ITEM_RED, true);

        // Place item in slot (1,1)
        int a = SpawnItem(1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, ITEM_RED);
        items[a].stackCount = 3;
        PlaceItemInStockpile(sp, 1, 1, a);

        // Before removal: 1 slot occupied (not full), 1 empty = 2 with room, but...
        RebuildStockpileFreeSlotCounts();
        // Both slots have room (one has 3/10, one has 0/10)
        expect(stockpiles[sp].freeSlotCount == 2);

        // Remove the empty cell (2,1)
        RemoveStockpileCells(sp, 2, 1, 2, 1);
        RebuildStockpileFreeSlotCounts();
        // Only 1 cell remains, it has 3/10 room
        expect(stockpiles[sp].freeSlotCount == 1);

        // Remove the occupied cell (1,1)
        RemoveStockpileCells(sp, 1, 1, 1, 1);
        // Stockpile should be deleted (0 active cells)
        expect(!stockpiles[sp].active);

        // Item should be on ground
        expect(items[a].active);
        expect(items[a].state == ITEM_ON_GROUND);
        expect(items[a].stackCount == 3);
    }
}

// ===========================================================================
// Removing stockpile cells drops full stacks
// ===========================================================================
describe(remove_stockpile_cell_drops_stack) {
    it("should drop all 3 items when removing cell with stack of 3") {
        Setup();
        int sp = CreateStockpile(1, 1, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_RED, true);

        int a = SpawnItem(1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, ITEM_RED);
        items[a].stackCount = 3;
        PlaceItemInStockpile(sp, 1, 1, a);

        expect(items[a].state == ITEM_IN_STOCKPILE);
        expect(items[a].stackCount == 3);

        // Remove the cell
        RemoveStockpileCells(sp, 1, 1, 1, 1);

        // Item should be dropped to ground with full stack intact
        expect(items[a].active);
        expect(items[a].state == ITEM_ON_GROUND);
        expect(items[a].stackCount == 3);
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
    test(take_from_stockpile_slot);
    test(deliver_to_workshop_split);
    test(passive_consumption_stacks);
    test(active_craft_split);
    test(semi_passive_consumption_stacks);
    test(hearth_fuel_split);
    test(e2e_craft_chest_from_stack);
    test(e2e_passive_delivery_split);
    test(e2e_semi_passive_sticks);
    test(e2e_hearth_fuel);
    test(e2e_auto_resume_passive);
    test(e2e_rehaul_after_drop);
    test(e2e_individual_planks_craft);
    test(free_slot_count_after_remove);
    test(remove_stockpile_cell_drops_stack);

    return summary();
}
