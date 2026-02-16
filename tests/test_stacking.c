#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/stacking.h"
#include "../src/entities/stockpiles.h"
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

    return summary();
}
