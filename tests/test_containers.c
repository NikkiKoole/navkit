#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/stacking.h"
#include "../src/entities/containers.h"
#include "../src/world/material.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// Test helper: set up ITEM_RED as a container with given maxContents
static void SetupContainerType(ItemType type, int maxContents) {
    containerDefs[type].maxContents = maxContents;
    containerDefs[type].spoilageModifier = 1.0f;
    containerDefs[type].weatherProtection = false;
    containerDefs[type].acceptsLiquids = false;
}

static void ClearContainerDefs(void) {
    memset(containerDefs, 0, sizeof(containerDefs));
}

static void Setup(void) {
    InitTestGrid(8, 8);
    ClearItems();
    ClearContainerDefs();
}

// ===========================================================================
// GetContainerDef
// ===========================================================================
describe(container_def) {
    it("should return NULL for non-container types") {
        Setup();
        expect(GetContainerDef(ITEM_RED) == NULL);
        expect(GetContainerDef(ITEM_LOG) == NULL);
    }

    it("should return def for configured container types") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        const ContainerDef* def = GetContainerDef(ITEM_RED);
        expect(def != NULL);
        expect(def->maxContents == 15);
    }

    it("should return NULL for invalid types") {
        Setup();
        expect(GetContainerDef(ITEM_NONE) == NULL);
        expect(GetContainerDef(ITEM_TYPE_COUNT) == NULL);
    }
}

// ===========================================================================
// PutItemInContainer / CanPutItemInContainer
// ===========================================================================
describe(put_item_in_container) {
    it("should put item in container and set fields correctly") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);
        int item = SpawnItem(0, 0, 0, ITEM_BLUE);

        expect(CanPutItemInContainer(item, container));
        PutItemInContainer(item, container);

        expect(items[item].containedIn == container);
        expect(items[item].state == ITEM_IN_CONTAINER);
        expect(items[container].contentCount == 1);
        expect(ContainerMightHaveType(container, ITEM_BLUE));
        // Item position mirrors container
        expect(items[item].x == items[container].x);
        expect(items[item].y == items[container].y);
    }

    it("should merge same type+material into existing stack") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);

        int a = SpawnItemWithMaterial(16, 16, 0, ITEM_LOG, MAT_OAK);
        items[a].stackCount = 3;
        PutItemInContainer(a, container);
        expect(items[container].contentCount == 1);

        int b = SpawnItemWithMaterial(16, 16, 0, ITEM_LOG, MAT_OAK);
        items[b].stackCount = 2;
        PutItemInContainer(b, container);

        // b should have been merged into a, contentCount unchanged
        expect(!items[b].active);  // consumed by merge
        expect(items[a].stackCount == 5);
        expect(items[container].contentCount == 1);
    }

    it("should add different type as new entry") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);

        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        PutItemInContainer(a, container);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(b, container);

        expect(items[container].contentCount == 2);
        expect(ContainerMightHaveType(container, ITEM_LOG));
        expect(ContainerMightHaveType(container, ITEM_ROCK));
    }

    it("should reject when container is full") {
        Setup();
        SetupContainerType(ITEM_RED, 2);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);

        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        PutItemInContainer(b, container);
        expect(items[container].contentCount == 2);

        int c = SpawnItem(16, 16, 0, ITEM_BLUE);
        expect(!CanPutItemInContainer(c, container));
        PutItemInContainer(c, container);  // should be no-op
        expect(items[c].containedIn == -1);
        expect(items[container].contentCount == 2);
    }

    it("should reject putting item in non-container") {
        Setup();
        int notContainer = SpawnItem(16, 16, 0, ITEM_RED);
        int item = SpawnItem(16, 16, 0, ITEM_BLUE);
        expect(!CanPutItemInContainer(item, notContainer));
    }

    it("should reject putting item into itself") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        expect(!CanPutItemInContainer(container, container));
    }

    it("should reject creating a cycle") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int outer = SpawnItem(16, 16, 0, ITEM_RED);
        int inner = SpawnItem(16, 16, 0, ITEM_GREEN);

        PutItemInContainer(inner, outer);
        expect(items[inner].containedIn == outer);

        // Trying to put outer inside inner would create a cycle
        expect(!CanPutItemInContainer(outer, inner));
    }
}

// ===========================================================================
// RemoveItemFromContainer
// ===========================================================================
describe(remove_from_container) {
    it("should remove item and update fields") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);
        int item = SpawnItem(0, 0, 0, ITEM_BLUE);
        PutItemInContainer(item, container);
        expect(items[container].contentCount == 1);

        RemoveItemFromContainer(item);

        expect(items[item].containedIn == -1);
        expect(items[item].state == ITEM_ON_GROUND);
        expect(items[container].contentCount == 0);
        expect(items[item].active);
    }

    it("should remove from nested container at outermost position") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int chest = SpawnItem(4 * CELL_SIZE, 5 * CELL_SIZE, 0, ITEM_RED);
        int bag = SpawnItem(16, 16, 0, ITEM_GREEN);
        int seed = SpawnItem(16, 16, 0, ITEM_BLUE);

        PutItemInContainer(bag, chest);
        PutItemInContainer(seed, bag);

        // Seed is in bag, bag is in chest
        expect(items[seed].containedIn == bag);
        expect(items[bag].containedIn == chest);

        RemoveItemFromContainer(seed);

        // Seed should be at chest's position (outermost), not bag's
        expect(items[seed].containedIn == -1);
        expect(items[seed].state == ITEM_ON_GROUND);
        expect(items[bag].contentCount == 0);
        // Bag stays in chest
        expect(items[bag].containedIn == chest);
        expect(items[chest].contentCount == 1);
    }

    it("should be no-op for item not in container") {
        Setup();
        int item = SpawnItem(16, 16, 0, ITEM_RED);
        RemoveItemFromContainer(item);
        expect(items[item].state == ITEM_ON_GROUND);
        expect(items[item].containedIn == -1);
    }
}

// ===========================================================================
// Container queries
// ===========================================================================
describe(container_queries) {
    it("should report full correctly") {
        Setup();
        SetupContainerType(ITEM_RED, 2);
        int container = SpawnItem(16, 16, 0, ITEM_RED);

        expect(!IsContainerFull(container));

        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        expect(!IsContainerFull(container));
        PutItemInContainer(b, container);
        expect(IsContainerFull(container));
    }

    it("should return correct content count") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);

        expect(GetContainerContentCount(container) == 0);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        PutItemInContainer(a, container);
        expect(GetContainerContentCount(container) == 1);
    }

    it("should detect type via bitmask (bloom filter)") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);

        expect(!ContainerMightHaveType(container, ITEM_LOG));

        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        PutItemInContainer(a, container);
        expect(ContainerMightHaveType(container, ITEM_LOG));

        // After removal, bitmask is stale (bloom filter — never cleared)
        RemoveItemFromContainer(a);
        expect(ContainerMightHaveType(container, ITEM_LOG));  // stale true is OK
    }
}

// ===========================================================================
// IsItemAccessible
// ===========================================================================
describe(accessibility) {
    it("should be accessible when container is free") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int item = SpawnItem(16, 16, 0, ITEM_BLUE);
        PutItemInContainer(item, container);

        expect(IsItemAccessible(item));
    }

    it("should not be accessible when container is reserved") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int item = SpawnItem(16, 16, 0, ITEM_BLUE);
        PutItemInContainer(item, container);

        items[container].reservedBy = 0;  // reserved by mover 0
        expect(!IsItemAccessible(item));
    }

    it("should not be accessible when container is carried") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int item = SpawnItem(16, 16, 0, ITEM_BLUE);
        PutItemInContainer(item, container);

        items[container].state = ITEM_CARRIED;
        expect(!IsItemAccessible(item));
    }

    it("should check entire ancestor chain") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int chest = SpawnItem(16, 16, 0, ITEM_RED);
        int bag = SpawnItem(16, 16, 0, ITEM_GREEN);
        int seed = SpawnItem(16, 16, 0, ITEM_BLUE);

        PutItemInContainer(bag, chest);
        PutItemInContainer(seed, bag);

        expect(IsItemAccessible(seed));

        // Reserve the chest — seed deep inside should be inaccessible
        items[chest].reservedBy = 0;
        expect(!IsItemAccessible(seed));
    }

    it("should be accessible for loose items") {
        Setup();
        int item = SpawnItem(16, 16, 0, ITEM_RED);
        expect(IsItemAccessible(item));
    }
}

// ===========================================================================
// MoveContainer
// ===========================================================================
describe(move_container) {
    it("should recursively move all contents") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        PutItemInContainer(b, container);

        float newX = 5 * CELL_SIZE;
        float newY = 6 * CELL_SIZE;
        float newZ = 1;
        MoveContainer(container, newX, newY, newZ);

        expect(items[container].x == newX);
        expect(items[container].y == newY);
        expect(items[a].x == newX);
        expect(items[a].y == newY);
        expect(items[b].x == newX);
        expect(items[b].y == newY);
    }

    it("should handle nested containers") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int chest = SpawnItem(16, 16, 0, ITEM_RED);
        int bag = SpawnItem(16, 16, 0, ITEM_GREEN);
        int seed = SpawnItem(16, 16, 0, ITEM_BLUE);

        PutItemInContainer(bag, chest);
        PutItemInContainer(seed, bag);

        float newX = 4 * CELL_SIZE;
        float newY = 4 * CELL_SIZE;
        MoveContainer(chest, newX, newY, 0);

        expect(items[chest].x == newX);
        expect(items[bag].x == newX);
        expect(items[seed].x == newX);
    }
}

// ===========================================================================
// SpillContainerContents
// ===========================================================================
describe(spill_contents) {
    it("should spill direct children to ground") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        PutItemInContainer(b, container);

        SpillContainerContents(container);

        expect(items[a].containedIn == -1);
        expect(items[a].state == ITEM_ON_GROUND);
        expect(items[b].containedIn == -1);
        expect(items[b].state == ITEM_ON_GROUND);
        expect(items[container].contentCount == 0);
        expect(items[container].contentTypeMask == 0);
    }

    it("should preserve sub-container contents") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int chest = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);
        int bag = SpawnItem(16, 16, 0, ITEM_GREEN);
        int seed = SpawnItem(16, 16, 0, ITEM_BLUE);

        PutItemInContainer(bag, chest);
        PutItemInContainer(seed, bag);

        SpillContainerContents(chest);

        // Bag is spilled out of chest
        expect(items[bag].containedIn == -1);
        expect(items[bag].state == ITEM_ON_GROUND);
        // But seed stays inside bag
        expect(items[seed].containedIn == bag);
        expect(items[seed].state == ITEM_IN_CONTAINER);
        expect(items[bag].contentCount == 1);
    }
}

// ===========================================================================
// DeleteItem container interaction
// ===========================================================================
describe(delete_container) {
    it("should spill contents when container deleted") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        PutItemInContainer(a, container);

        DeleteItem(container);

        expect(!items[container].active);
        expect(items[a].active);
        expect(items[a].containedIn == -1);
        expect(items[a].state == ITEM_ON_GROUND);
    }

    it("should decrement parent contentCount when contained item deleted") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        PutItemInContainer(b, container);
        expect(items[container].contentCount == 2);

        DeleteItem(a);

        expect(!items[a].active);
        expect(items[container].contentCount == 1);
    }
}

// ===========================================================================
// SplitStack inside container
// ===========================================================================
describe(split_in_container) {
    it("should keep split item inside same container") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int item = SpawnItem(16, 16, 0, ITEM_LOG);
        items[item].stackCount = 10;
        PutItemInContainer(item, container);
        expect(items[container].contentCount == 1);

        int split = SplitStack(item, 3);
        expect(split >= 0);
        expect(items[split].containedIn == container);
        expect(items[split].state == ITEM_IN_CONTAINER);
        expect(items[split].stackCount == 3);
        expect(items[item].stackCount == 7);
        // contentCount increases — now 2 items inside
        expect(items[container].contentCount == 2);
    }
}

// ===========================================================================
// ForEachContainedItem / ForEachContainedItemRecursive
// ===========================================================================
static void CountCallback(int itemIdx, void* data) {
    (void)itemIdx;
    int* count = (int*)data;
    (*count)++;
}

describe(iteration) {
    it("should iterate direct children only") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int chest = SpawnItem(16, 16, 0, ITEM_RED);
        int bag = SpawnItem(16, 16, 0, ITEM_GREEN);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);

        PutItemInContainer(bag, chest);
        PutItemInContainer(a, bag);
        PutItemInContainer(b, chest);

        int count = 0;
        ForEachContainedItem(chest, CountCallback, &count);
        expect(count == 2);  // bag + b (not a, which is in bag)
    }

    it("should iterate all descendants recursively") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int chest = SpawnItem(16, 16, 0, ITEM_RED);
        int bag = SpawnItem(16, 16, 0, ITEM_GREEN);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);

        PutItemInContainer(bag, chest);
        PutItemInContainer(a, bag);
        PutItemInContainer(b, chest);

        int count = 0;
        ForEachContainedItemRecursive(chest, CountCallback, &count);
        expect(count == 3);  // bag + a + b
    }
}

// ===========================================================================
// GetContainerTotalWeight
// ===========================================================================
describe(weight) {
    it("should sum container and content weights") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        items[a].stackCount = 3;
        PutItemInContainer(a, container);

        float w = GetContainerTotalWeight(container);
        float expected = ItemWeight(ITEM_RED) * 1 + ItemWeight(ITEM_LOG) * 3;
        expect(w > expected - 0.01f && w < expected + 0.01f);
    }
}

// ===========================================================================
// Spatial grid exclusion
// ===========================================================================
describe(spatial_grid) {
    it("should not include contained items in spatial grid") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_RED);
        int item = SpawnItem(2 * CELL_SIZE, 3 * CELL_SIZE, 0, ITEM_BLUE);
        PutItemInContainer(item, container);

        BuildItemSpatialGrid();

        // Container should be in grid (it's ON_GROUND)
        int found = QueryItemAtTile(2, 3, 0);
        expect(found == container);
        // The contained item should NOT be found at that tile as a separate entry
        // (QueryItemAtTile returns one item, and it should be the container)
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

    test(container_def);
    test(put_item_in_container);
    test(remove_from_container);
    test(container_queries);
    test(accessibility);
    test(move_container);
    test(spill_contents);
    test(delete_container);
    test(split_in_container);
    test(iteration);
    test(weight);
    test(spatial_grid);

    return summary();
}
