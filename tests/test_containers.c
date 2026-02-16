#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/stacking.h"
#include "../src/entities/containers.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/jobs.h"
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

// ===========================================================================
// Phase 2: Container Item Types (basket, chest, clay pot)
// ===========================================================================

// Restore the real container definitions (previous tests clear them)
static void RestoreRealContainerDefs(void) {
    memset(containerDefs, 0, sizeof(containerDefs));
    containerDefs[ITEM_BASKET]   = (ContainerDef){ .maxContents = 15, .spoilageModifier = 1.0f, .weatherProtection = false, .acceptsLiquids = false };
    containerDefs[ITEM_CHEST]    = (ContainerDef){ .maxContents = 20, .spoilageModifier = 0.7f, .weatherProtection = true,  .acceptsLiquids = false };
    containerDefs[ITEM_CLAY_POT] = (ContainerDef){ .maxContents = 5,  .spoilageModifier = 0.5f, .weatherProtection = true,  .acceptsLiquids = true  };
}

static void SetupWithRealDefs(void) {
    InitTestGrid(8, 8);
    ClearItems();
    RestoreRealContainerDefs();
}

describe(container_item_types) {
    it("should have IF_CONTAINER flag on container items") {
        expect(ItemIsContainer(ITEM_BASKET));
        expect(ItemIsContainer(ITEM_CLAY_POT));
        expect(ItemIsContainer(ITEM_CHEST));
    }

    it("should NOT have IF_CONTAINER on non-container items") {
        expect(!ItemIsContainer(ITEM_RED));
        expect(!ItemIsContainer(ITEM_LOG));
        expect(!ItemIsContainer(ITEM_ROCK));
        expect(!ItemIsContainer(ITEM_PLANKS));
        expect(!ItemIsContainer(ITEM_CORDAGE));
    }

    it("should have correct basket definition") {
        RestoreRealContainerDefs();
        const ContainerDef* def = GetContainerDef(ITEM_BASKET);
        expect(def != NULL);
        expect(def->maxContents == 15);
        expect(def->spoilageModifier == 1.0f);
        expect(def->weatherProtection == false);
        expect(def->acceptsLiquids == false);
    }

    it("should have correct chest definition") {
        RestoreRealContainerDefs();
        const ContainerDef* def = GetContainerDef(ITEM_CHEST);
        expect(def != NULL);
        expect(def->maxContents == 20);
        expect(def->spoilageModifier == 0.7f);
        expect(def->weatherProtection == true);
        expect(def->acceptsLiquids == false);
    }

    it("should have correct clay pot definition") {
        RestoreRealContainerDefs();
        const ContainerDef* def = GetContainerDef(ITEM_CLAY_POT);
        expect(def != NULL);
        expect(def->maxContents == 5);
        expect(def->spoilageModifier == 0.5f);
        expect(def->weatherProtection == true);
        expect(def->acceptsLiquids == true);
    }

    it("should return NULL for non-container item types") {
        RestoreRealContainerDefs();
        // Non-containers have maxContents=0 in the defs table
        expect(GetContainerDef(ITEM_LOG) == NULL);
        expect(GetContainerDef(ITEM_ROCK) == NULL);
        expect(GetContainerDef(ITEM_PLANKS) == NULL);
    }

    it("should allow baskets to be stackable") {
        expect(ItemIsStackable(ITEM_BASKET));
        expect(ItemMaxStack(ITEM_BASKET) == 10);
    }

    it("should allow clay pots to be stackable") {
        expect(ItemIsStackable(ITEM_CLAY_POT));
        expect(ItemMaxStack(ITEM_CLAY_POT) == 10);
    }

    it("should NOT allow chests to be stackable") {
        expect(!ItemIsStackable(ITEM_CHEST));
        expect(ItemMaxStack(ITEM_CHEST) == 1);
    }

    it("should put items into a real basket container") {
        SetupWithRealDefs();
        int basket = SpawnItem(4, 4, 0, ITEM_BASKET);
        int rock = SpawnItem(4, 4, 0, ITEM_ROCK);

        expect(CanPutItemInContainer(rock, basket));
        PutItemInContainer(rock, basket);
        expect(items[rock].containedIn == basket);
        expect(items[rock].state == ITEM_IN_CONTAINER);
        expect(items[basket].contentCount == 1);
    }

    it("should respect basket capacity of 15 stacks") {
        SetupWithRealDefs();
        int basket = SpawnItem(4, 4, 0, ITEM_BASKET);

        // Fill with 15 different item types (using ITEM_RED through various types)
        for (int i = 0; i < 15; i++) {
            int item = SpawnItemWithMaterial(4, 4, 0, ITEM_RED + (i % 3), i % 3);
            // Make each unique by using different material so they don't merge
            items[item].material = (uint8_t)i;  // unique material per item
            PutItemInContainer(item, basket);
        }
        expect(items[basket].contentCount == 15);
        expect(IsContainerFull(basket));

        // 16th should fail
        int extra = SpawnItem(4, 4, 0, ITEM_DIRT);
        expect(!CanPutItemInContainer(extra, basket));
    }

    it("should respect clay pot capacity of 5 stacks") {
        SetupWithRealDefs();
        int pot = SpawnItem(4, 4, 0, ITEM_CLAY_POT);

        for (int i = 0; i < 5; i++) {
            int item = SpawnItem(4, 4, 0, ITEM_RED);
            items[item].material = (uint8_t)i;
            PutItemInContainer(item, pot);
        }
        expect(items[pot].contentCount == 5);
        expect(IsContainerFull(pot));

        int extra = SpawnItem(4, 4, 0, ITEM_DIRT);
        expect(!CanPutItemInContainer(extra, pot));
    }

    it("should have correct weights") {
        expect(ItemWeight(ITEM_BASKET) == 1.0f);
        expect(ItemWeight(ITEM_CLAY_POT) == 3.0f);
        expect(ItemWeight(ITEM_CHEST) == 8.0f);
    }
}

// ===========================================================================
// Phase 3: Stockpile Container Integration
// ===========================================================================

static void StockpileSetup(void) {
    InitTestGrid(8, 8);
    ClearItems();
    ClearStockpiles();
    RestoreRealContainerDefs();
}

describe(stockpile_containers) {
    it("should install container in stockpile slot when maxContainers > 0") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        expect(items[basket].state == ITEM_IN_STOCKPILE);
        expect(IsSlotContainer(sp, 0));
        expect(CountInstalledContainers(sp) == 1);
        // slotCounts should be 0 (container is empty)
        expect(stockpiles[sp].slotCounts[0] == 0);
    }

    it("should store container as regular item when maxContainers == 0") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        // maxContainers defaults to 0

        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        expect(items[basket].state == ITEM_IN_STOCKPILE);
        // Should be a normal slot, not a container slot
        expect(!IsSlotContainer(sp, 0));
        expect(CountInstalledContainers(sp) == 0);
        expect(stockpiles[sp].slotCounts[0] == 1);
    }

    it("should route items into container slot") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        // Install a basket
        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);
        expect(IsSlotContainer(sp, 0));

        // Place a rock into the same slot — should go inside the basket
        int rock = SpawnItem(0, 0, 0, ITEM_ROCK);
        PlaceItemInStockpile(sp, 0, 0, rock);

        expect(items[rock].containedIn == basket);
        expect(items[rock].state == ITEM_IN_CONTAINER);
        expect(items[basket].contentCount == 1);
        expect(stockpiles[sp].slotCounts[0] == 1);
    }

    it("should merge same-type items in container slot") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        int rock1 = SpawnItemWithMaterial(0, 0, 0, ITEM_ROCK, MAT_GRANITE);
        items[rock1].stackCount = 3;
        PlaceItemInStockpile(sp, 0, 0, rock1);
        expect(items[basket].contentCount == 1);

        int rock2 = SpawnItemWithMaterial(0, 0, 0, ITEM_ROCK, MAT_GRANITE);
        items[rock2].stackCount = 2;
        PlaceItemInStockpile(sp, 0, 0, rock2);

        // rock2 should have been merged into rock1
        expect(items[rock1].stackCount == 5);
        expect(items[basket].contentCount == 1);
        expect(stockpiles[sp].slotCounts[0] == 1);
    }

    it("should support mixed item types in single container") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        int rock = SpawnItem(0, 0, 0, ITEM_ROCK);
        PlaceItemInStockpile(sp, 0, 0, rock);
        int log = SpawnItem(0, 0, 0, ITEM_LOG);
        PlaceItemInStockpile(sp, 0, 0, log);
        int dirt = SpawnItem(0, 0, 0, ITEM_DIRT);
        PlaceItemInStockpile(sp, 0, 0, dirt);

        expect(items[basket].contentCount == 3);
        expect(stockpiles[sp].slotCounts[0] == 3);
        expect(items[rock].containedIn == basket);
        expect(items[log].containedIn == basket);
        expect(items[dirt].containedIn == basket);
    }

    it("should respect container capacity") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        // Clay pot has capacity 5
        int pot = SpawnItem(0, 0, 0, ITEM_CLAY_POT);
        PlaceItemInStockpile(sp, 0, 0, pot);
        expect(IsSlotContainer(sp, 0));

        // Fill all 5 slots
        for (int i = 0; i < 5; i++) {
            int item = SpawnItem(0, 0, 0, ITEM_RED);
            items[item].material = (uint8_t)i;  // unique so they don't merge
            PlaceItemInStockpile(sp, 0, 0, item);
        }
        expect(items[pot].contentCount == 5);
        expect(stockpiles[sp].slotCounts[0] == 5);

        // 6th item should NOT go into the full container via FindFreeStockpileSlot
        int outX, outY;
        // The container slot should be full, so FindFreeStockpileSlot should skip it
        // and find an empty bare slot instead
        bool found = FindFreeStockpileSlot(sp, ITEM_RED, MAT_NONE, &outX, &outY);
        expect(found);
        // Should be a different slot, not (0,0)
        expect(outX != 0 || outY != 0);
    }

    it("should enforce maxContainers limit") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 3, 1);
        SetStockpileMaxContainers(sp, 2);

        // Install first two containers
        int b1 = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, b1);
        int b2 = SpawnItem(CELL_SIZE, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 1, 0, b2);

        expect(CountInstalledContainers(sp) == 2);
        expect(IsSlotContainer(sp, 0));
        expect(IsSlotContainer(sp, 1));

        // Third container should be stored as regular item (limit reached)
        int b3 = SpawnItem(2 * CELL_SIZE, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 2, 0, b3);

        expect(!IsSlotContainer(sp, 2));
        expect(CountInstalledContainers(sp) == 2);
        expect(stockpiles[sp].slotCounts[2] == 1);  // stored as regular stack
    }

    it("should prefer container slots in FindFreeStockpileSlot") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 1);
        SetStockpileMaxContainers(sp, 2);

        // Install basket in slot 0
        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);
        expect(IsSlotContainer(sp, 0));
        // FindFreeStockpileSlot should prefer the container slot
        int outX, outY;
        bool found = FindFreeStockpileSlot(sp, ITEM_ROCK, MAT_GRANITE, &outX, &outY);
        expect(found);
        expect(outX == 0);
        expect(outY == 0);
    }

    it("should not put containers inside containers via FindFreeStockpileSlot") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 1);
        SetStockpileMaxContainers(sp, 2);

        // Install basket in slot 0
        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        // Finding slot for another basket should NOT return the container slot
        int outX, outY;
        bool found = FindFreeStockpileSlot(sp, ITEM_BASKET, MAT_NONE, &outX, &outY);
        expect(found);
        // Should be slot 1 (empty), not slot 0 (container)
        expect(outX == 1);
    }

    it("should spill container contents when stockpile deleted") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        int rock = SpawnItem(0, 0, 0, ITEM_ROCK);
        PlaceItemInStockpile(sp, 0, 0, rock);
        int log = SpawnItem(0, 0, 0, ITEM_LOG);
        PlaceItemInStockpile(sp, 0, 0, log);

        expect(items[basket].contentCount == 2);

        DeleteStockpile(sp);

        // Container contents should be spilled
        expect(items[rock].containedIn == -1);
        expect(items[rock].state == ITEM_ON_GROUND);
        expect(items[log].containedIn == -1);
        expect(items[log].state == ITEM_ON_GROUND);
        // Basket itself should be on ground too
        expect(items[basket].state == ITEM_ON_GROUND);
        expect(items[basket].contentCount == 0);
    }

    it("should account for container capacity in fill ratio") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 1, 1);  // 1 cell stockpile
        SetStockpileMaxContainers(sp, 1);

        // Install a clay pot (capacity 5) in the single slot
        int pot = SpawnItem(0, 0, 0, ITEM_CLAY_POT);
        PlaceItemInStockpile(sp, 0, 0, pot);
        expect(IsSlotContainer(sp, 0));

        // Empty container: fill ratio should be 0
        float ratio = GetStockpileFillRatio(sp);
        expect(ratio < 0.01f);

        // Add 1 item: fill ratio should be 1/5 = 0.2
        int rock = SpawnItem(0, 0, 0, ITEM_ROCK);
        PlaceItemInStockpile(sp, 0, 0, rock);
        ratio = GetStockpileFillRatio(sp);
        expect(ratio > 0.19f && ratio < 0.21f);

        // Add 4 more items: fill ratio should be 5/5 = 1.0
        for (int i = 0; i < 4; i++) {
            int item = SpawnItem(0, 0, 0, ITEM_RED);
            items[item].material = (uint8_t)(i + 1);
            PlaceItemInStockpile(sp, 0, 0, item);
        }
        ratio = GetStockpileFillRatio(sp);
        expect(ratio > 0.99f && ratio < 1.01f);
    }

    it("should detect container slots with IsSlotContainer") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 1);
        SetStockpileMaxContainers(sp, 1);

        // Slot 0: empty
        expect(!IsSlotContainer(sp, 0));
        expect(!IsSlotContainer(sp, 1));

        // Install basket in slot 0
        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);
        expect(IsSlotContainer(sp, 0));
        expect(!IsSlotContainer(sp, 1));

        // Place a non-container in slot 1
        int rock = SpawnItem(CELL_SIZE, 0, 0, ITEM_ROCK);
        PlaceItemInStockpile(sp, 1, 0, rock);
        expect(IsSlotContainer(sp, 0));
        expect(!IsSlotContainer(sp, 1));
    }

    it("should get and set maxContainers") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);

        expect(GetStockpileMaxContainers(sp) == 0);
        SetStockpileMaxContainers(sp, 5);
        expect(GetStockpileMaxContainers(sp) == 5);
        SetStockpileMaxContainers(sp, 0);
        expect(GetStockpileMaxContainers(sp) == 0);
        // Negative clamps to 0
        SetStockpileMaxContainers(sp, -1);
        expect(GetStockpileMaxContainers(sp) == 0);
    }

    it("should handle free slot count with containers") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 1);
        SetStockpileMaxContainers(sp, 2);

        // Install basket (capacity 15) in slot 0
        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        PlaceItemInStockpile(sp, 0, 0, basket);

        RebuildStockpileFreeSlotCounts();
        // Slot 0 = container with 15 capacity, slot 1 = bare with maxStackSize capacity
        // Both should be "free"
        expect(stockpiles[sp].freeSlotCount == 2);

        // Fill the container
        for (int i = 0; i < 15; i++) {
            int item = SpawnItem(0, 0, 0, ITEM_RED);
            items[item].material = (uint8_t)i;
            PlaceItemInStockpile(sp, 0, 0, item);
        }
        RebuildStockpileFreeSlotCounts();
        // Container slot is full, bare slot still free
        expect(stockpiles[sp].freeSlotCount == 1);
    }
}

// ===========================================================================
// Phase 4: Container-Aware Search + Extraction
// ===========================================================================

static void SearchSetup(void) {
    InitTestGrid(8, 8);
    ClearItems();
    ClearStockpiles();
    ClearContainerDefs();
    SetupContainerType(ITEM_RED, 10);  // ITEM_RED as container for testing
}

describe(container_search) {
    it("should find item inside container") {
        SearchSetup();
        // Create container at tile (3,3)
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        // Put an ITEM_BLUE inside
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);

        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 3, 3, 50, -1, NULL, NULL, &containerIdx);
        expect(found == blue);
        expect(containerIdx == basket);
    }

    it("should find item in nested container") {
        SearchSetup();
        SetupContainerType(ITEM_GREEN, 10);  // ITEM_GREEN as inner container
        int outer = SpawnItemWithMaterial(4 * CELL_SIZE + 16, 4 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int inner = SpawnItemWithMaterial(0, 0, 0, ITEM_GREEN, MAT_NONE);
        PutItemInContainer(inner, outer);
        int target = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(target, inner);

        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 4, 4, 50, -1, NULL, NULL, &containerIdx);
        expect(found == target);
        expect(containerIdx == outer);  // outermost container
    }

    it("should return -1 when bloom filter rejects") {
        SearchSetup();
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);

        // Search for ITEM_LOG which is not in the container
        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_LOG, 0, 3, 3, 50, -1, NULL, NULL, &containerIdx);
        expect(found == -1);
        expect(containerIdx == -1);
    }

    it("should skip reserved containers") {
        SearchSetup();
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);
        items[basket].reservedBy = 0;  // Container reserved by mover 0

        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 3, 3, 50, -1, NULL, NULL, &containerIdx);
        expect(found == -1);  // Skipped because container is reserved
    }

    it("should skip carried containers") {
        SearchSetup();
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);
        items[basket].state = ITEM_CARRIED;

        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 3, 3, 50, -1, NULL, NULL, &containerIdx);
        expect(found == -1);
    }

    it("should skip reserved items inside container") {
        SearchSetup();
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);
        items[blue].reservedBy = 0;  // Item itself is reserved

        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 3, 3, 50, -1, NULL, NULL, &containerIdx);
        expect(found == -1);
    }

    it("should respect z-level") {
        SearchSetup();
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 1, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);

        // Search z=0, container is at z=1
        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 3, 3, 50, -1, NULL, NULL, &containerIdx);
        expect(found == -1);
    }

    it("should respect search radius") {
        SearchSetup();
        // Container at tile (7,7), searching from (0,0) with radius 5
        int basket = SpawnItemWithMaterial(7 * CELL_SIZE + 16, 7 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);

        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 0, 0, 5, -1, NULL, NULL, &containerIdx);
        expect(found == -1);  // Too far

        // Same search with larger radius
        found = FindItemInContainers(ITEM_BLUE, 0, 0, 0, 50, -1, NULL, NULL, &containerIdx);
        expect(found == blue);
    }

    it("should exclude specific item index") {
        SearchSetup();
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);

        // Exclude the item we'd find
        int containerIdx = -1;
        int found = FindItemInContainers(ITEM_BLUE, 0, 3, 3, 50, blue, NULL, NULL, &containerIdx);
        expect(found == -1);
    }
}

describe(outermost_container) {
    it("should return self for non-contained item") {
        Setup();
        int item = SpawnItemWithMaterial(100, 100, 0, ITEM_BLUE, MAT_NONE);
        expect(GetOutermostContainer(item) == item);
    }

    it("should walk chain to outermost") {
        Setup();
        SetupContainerType(ITEM_RED, 10);
        SetupContainerType(ITEM_GREEN, 10);
        int chest = SpawnItemWithMaterial(100, 100, 0, ITEM_RED, MAT_NONE);
        int bag = SpawnItemWithMaterial(0, 0, 0, ITEM_GREEN, MAT_NONE);
        PutItemInContainer(bag, chest);
        int seed = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(seed, bag);

        expect(GetOutermostContainer(seed) == chest);
        expect(GetOutermostContainer(bag) == chest);
        expect(GetOutermostContainer(chest) == chest);
    }
}

describe(container_extraction) {
    it("should extract item from container") {
        Setup();
        SetupContainerType(ITEM_RED, 10);
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int blue = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        PutItemInContainer(blue, basket);
        expect(items[blue].state == ITEM_IN_CONTAINER);
        expect(items[blue].containedIn == basket);

        RemoveItemFromContainer(blue);
        expect(items[blue].state == ITEM_ON_GROUND);
        expect(items[blue].containedIn == -1);
        expect(items[basket].contentCount == 0);
    }

    it("should sync stockpile slotCounts on extraction") {
        InitTestGrid(8, 8);
        ClearItems();
        ClearStockpiles();
        RestoreRealContainerDefs();

        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 2);
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) SetStockpileFilter(sp, t, true);

        // Install a basket in the stockpile
        int basket = SpawnItemWithMaterial(0 * CELL_SIZE + 16, 0 * CELL_SIZE + 16, 0, ITEM_BASKET, MAT_NONE);
        PlaceItemInStockpile(sp, 0, 0, basket);
        expect(stockpiles[sp].slotIsContainer[0] == true);

        // Put items in the container
        int rock1 = SpawnItemWithMaterial(100, 100, 0, ITEM_ROCK, MAT_GRANITE);
        PlaceItemInStockpile(sp, 0, 0, rock1);
        expect(items[rock1].containedIn == basket);
        expect(stockpiles[sp].slotCounts[0] == 1);

        int rock2 = SpawnItemWithMaterial(100, 100, 0, ITEM_ROCK, MAT_SANDSTONE);
        PlaceItemInStockpile(sp, 0, 0, rock2);
        expect(stockpiles[sp].slotCounts[0] == 2);

        // Extract one item
        int parent = items[rock1].containedIn;
        RemoveItemFromContainer(rock1);
        SyncStockpileContainerSlotCount(parent);
        expect(stockpiles[sp].slotCounts[0] == 1);
        expect(items[basket].contentCount == 1);
    }

    it("should handle multiple extractions from same container") {
        Setup();
        SetupContainerType(ITEM_RED, 10);
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);

        int a = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        int b = SpawnItemWithMaterial(0, 0, 0, ITEM_GREEN, MAT_NONE);
        int c = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
        PutItemInContainer(a, basket);
        PutItemInContainer(b, basket);
        PutItemInContainer(c, basket);
        expect(items[basket].contentCount == 3);

        RemoveItemFromContainer(a);
        expect(items[basket].contentCount == 2);
        RemoveItemFromContainer(b);
        expect(items[basket].contentCount == 1);
        RemoveItemFromContainer(c);
        expect(items[basket].contentCount == 0);
    }

    it("should split stack and extract partial amount") {
        Setup();
        SetupContainerType(ITEM_RED, 10);
        int basket = SpawnItemWithMaterial(3 * CELL_SIZE + 16, 3 * CELL_SIZE + 16, 0, ITEM_RED, MAT_NONE);
        int stack = SpawnItemWithMaterial(0, 0, 0, ITEM_BLUE, MAT_NONE);
        items[stack].stackCount = 10;
        PutItemInContainer(stack, basket);

        // Split 3 from the stack inside the container
        int split = SplitStack(stack, 3);
        expect(split >= 0);
        expect(items[split].containedIn == basket);
        expect(items[split].stackCount == 3);
        expect(items[stack].stackCount == 7);
        expect(items[basket].contentCount == 2);  // original + split

        // Extract the split portion
        RemoveItemFromContainer(split);
        expect(items[split].state == ITEM_ON_GROUND);
        expect(items[split].containedIn == -1);
        expect(items[basket].contentCount == 1);  // original remains
    }
}

// ===========================================================================
// Phase 5: Container Hauling (carried containers)
// ===========================================================================
describe(container_hauling) {
    it("should move contents when container position changes") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        PutItemInContainer(b, container);

        // Simulate mover carrying: update container position, then MoveContainer
        items[container].state = ITEM_CARRIED;
        items[container].x = 48.0f;
        items[container].y = 48.0f;
        items[container].z = 0.0f;
        MoveContainer(container, 48.0f, 48.0f, 0.0f);

        expect(items[a].x > 47.9f && items[a].x < 48.1f);
        expect(items[a].y > 47.9f && items[a].y < 48.1f);
        expect(items[b].x > 47.9f && items[b].x < 48.1f);
        expect(items[b].y > 47.9f && items[b].y < 48.1f);
    }

    it("should move nested container contents recursively") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        SetupContainerType(ITEM_GREEN, 15);
        int outer = SpawnItem(16, 16, 0, ITEM_RED);
        int inner = SpawnItem(16, 16, 0, ITEM_GREEN);
        int seed = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(inner, outer);
        PutItemInContainer(seed, inner);

        items[outer].state = ITEM_CARRIED;
        items[outer].x = 48.0f;
        items[outer].y = 48.0f;
        MoveContainer(outer, 48.0f, 48.0f, 0.0f);

        expect(items[inner].x > 47.9f && items[inner].x < 48.1f);
        expect(items[seed].x > 47.9f && items[seed].x < 48.1f);
    }

    it("should use total weight for carried container") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        items[a].stackCount = 5;
        PutItemInContainer(a, container);

        float totalW = GetContainerTotalWeight(container);
        float expected = ItemWeight(ITEM_RED) + ItemWeight(ITEM_LOG) * 5;
        expect(totalW > expected - 0.01f && totalW < expected + 0.01f);

        // Non-container single item weight (for comparison)
        float singleW = ItemWeight(ITEM_LOG);
        expect(totalW > singleW);
    }

    it("should make contents inaccessible when container is carried") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        PutItemInContainer(a, container);

        // Before carrying — accessible
        expect(IsItemAccessible(a));

        // Mark as carried
        items[container].state = ITEM_CARRIED;
        expect(!IsItemAccessible(a));

        // Drop — accessible again
        items[container].state = ITEM_ON_GROUND;
        expect(IsItemAccessible(a));
    }

    it("should make contents inaccessible when container is reserved") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        PutItemInContainer(a, container);

        items[container].reservedBy = 0;
        expect(!IsItemAccessible(a));

        items[container].reservedBy = -1;
        expect(IsItemAccessible(a));
    }

    it("should keep contents intact when container is safe-dropped") {
        Setup();
        SetupContainerType(ITEM_RED, 15);
        int container = SpawnItem(16, 16, 0, ITEM_RED);
        int a = SpawnItem(16, 16, 0, ITEM_LOG);
        int b = SpawnItem(16, 16, 0, ITEM_ROCK);
        PutItemInContainer(a, container);
        PutItemInContainer(b, container);

        // Simulate carry
        items[container].state = ITEM_CARRIED;
        items[container].x = 48.0f;
        items[container].y = 48.0f;
        MoveContainer(container, 48.0f, 48.0f, 0.0f);

        // Safe drop the container
        SafeDropItem(container, 16.0f, 16.0f, 0);
        MoveContainer(container, items[container].x, items[container].y, items[container].z);
        items[container].state = ITEM_ON_GROUND;

        // Contents still inside
        expect(items[a].containedIn == container);
        expect(items[b].containedIn == container);
        expect(items[container].contentCount == 2);

        // Contents moved to container's new position
        expect(items[a].x == items[container].x);
        expect(items[a].y == items[container].y);
        expect(items[b].x == items[container].x);
        expect(items[b].y == items[container].y);
    }

    it("should install full container in stockpile with contents intact") {
        StockpileSetup();
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        // Create basket with items inside
        int basket = SpawnItem(0, 0, 0, ITEM_BASKET);
        int log = SpawnItem(0, 0, 0, ITEM_LOG);
        int rock = SpawnItem(0, 0, 0, ITEM_ROCK);
        PutItemInContainer(log, basket);
        PutItemInContainer(rock, basket);

        // Place basket in stockpile — should install as container slot
        PlaceItemInStockpile(sp, 0, 0, basket);

        // Basket installed as container slot
        expect(items[basket].state == ITEM_IN_STOCKPILE);
        expect(IsSlotContainer(sp, 0));

        // Contents still inside
        expect(items[log].containedIn == basket);
        expect(items[rock].containedIn == basket);
        expect(items[basket].contentCount == 2);
    }
}

// ===========================================================================
// Filter Change Cleanup (Phase 6)
// ===========================================================================

static void FilterCleanupSetup(void) {
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
    RestoreRealContainerDefs();
    // Create one idle mover
    Mover* m = &movers[0];
    Point goal = {1, 1, 0};
    InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
    moverCount = 1;
}

describe(container_filter_cleanup) {
    it("should extract illegal items from container after filter change") {
        FilterCleanupSetup();

        // Stockpile at (5,5) accepts baskets + logs + rocks
        int sp = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        // Place basket in stockpile with a log inside
        float sx = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float sy = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        int basket = SpawnItem(sx, sy, 0, ITEM_BASKET);
        int log = SpawnItem(sx, sy, 0, ITEM_LOG);
        PutItemInContainer(log, basket);
        PlaceItemInStockpile(sp, 5, 5, basket);

        // Verify initial state
        expect(items[log].containedIn == basket);
        expect(items[log].state == ITEM_IN_CONTAINER);

        // Disallow logs from this stockpile
        SetStockpileFilter(sp, ITEM_LOG, false);

        // Run AssignJobs — should extract log from basket
        AssignJobs();

        // Log should be extracted from container
        expect(items[log].containedIn == -1);
        expect(items[log].state != ITEM_IN_CONTAINER);

        // Basket should still be installed as container slot
        expect(IsSlotContainer(sp, 0));
        expect(items[basket].state == ITEM_IN_STOCKPILE);
    }

    it("should leave legal items in container untouched") {
        FilterCleanupSetup();

        int sp = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        float sx = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float sy = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        int basket = SpawnItem(sx, sy, 0, ITEM_BASKET);
        int log = SpawnItem(sx, sy, 0, ITEM_LOG);
        int rock = SpawnItem(sx, sy, 0, ITEM_ROCK);
        PutItemInContainer(log, basket);
        PutItemInContainer(rock, basket);
        PlaceItemInStockpile(sp, 5, 5, basket);

        // Disallow rocks only
        SetStockpileFilter(sp, ITEM_ROCK, false);

        AssignJobs();

        // Rock extracted, log stays
        expect(items[rock].containedIn == -1);
        expect(items[log].containedIn == basket);
        expect(items[log].state == ITEM_IN_CONTAINER);
        expect(items[basket].contentCount == 1);
    }

    it("should not extract items when all types still allowed") {
        FilterCleanupSetup();

        int sp = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        float sx = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float sy = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        int basket = SpawnItem(sx, sy, 0, ITEM_BASKET);
        int log = SpawnItem(sx, sy, 0, ITEM_LOG);
        PutItemInContainer(log, basket);
        PlaceItemInStockpile(sp, 5, 5, basket);

        // Don't change any filters — everything still allowed
        AssignJobs();

        // Log should remain in container
        expect(items[log].containedIn == basket);
        expect(items[log].state == ITEM_IN_CONTAINER);
        expect(items[basket].contentCount == 1);
    }

    it("should keep empty container as slot after all contents cleared") {
        FilterCleanupSetup();

        int sp = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileMaxContainers(sp, 4);

        float sx = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float sy = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        int basket = SpawnItem(sx, sy, 0, ITEM_BASKET);
        int log = SpawnItem(sx, sy, 0, ITEM_LOG);
        PutItemInContainer(log, basket);
        PlaceItemInStockpile(sp, 5, 5, basket);

        // Disallow logs
        SetStockpileFilter(sp, ITEM_LOG, false);

        AssignJobs();

        // Container should still be installed even though empty now
        expect(IsSlotContainer(sp, 0));
        expect(items[basket].state == ITEM_IN_STOCKPILE);
        expect(items[basket].contentCount == 0);
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
    test(container_item_types);
    test(stockpile_containers);
    test(container_search);
    test(outermost_container);
    test(container_extraction);
    test(container_hauling);
    test(container_filter_cleanup);

    return summary();
}
