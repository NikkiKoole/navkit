// Test suite for workshop-stockpile linking feature
// Tests core linking logic, ingredient search, and edge cases

#include "../src/test_framework.h"
#include "../src/entities/workshops.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/items.h"
#include "../src/entities/movers.h"
#include "../src/entities/jobs.h"
#include "../src/grid/grid.h"

// Test verbose flag
extern int test_verbose;

// Helper function declarations
static void SetupBasicScene(void);
static int CreateTestWorkshop(int x, int y, int z, WorkshopType type);
static int CreateTestStockpile(int x, int y, int z, int w, int h);
static int SpawnTestItem(int x, int y, int z, ItemType type, MaterialType material);
static void ClearTestState(void);

// =============================================================================
// SETUP & TEARDOWN HELPERS
// =============================================================================

static void SetupBasicScene(void) {
    InitGridWithSizeAndChunkSize(32, 32, 16, 16);
    
    // Create a flat floor for testing
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            SetCellType(x, y, 0, CELL_AIR);
            SetFloor(x, y, 0, true);
            SetWalkable(x, y, 0, true);
        }
    }
}

static void ClearTestState(void) {
    ClearWorkshops();
    ClearStockpiles();
    ClearItems();
    ClearMovers();
    ClearJobs();
}

static int CreateTestWorkshop(int x, int y, int z, WorkshopType type) {
    int idx = AddWorkshop(x, y, z, type);
    if (idx >= 0) {
        workshops[idx].active = true;
        workshops[idx].width = 3;
        workshops[idx].height = 3;
    }
    return idx;
}

static int CreateTestStockpile(int x, int y, int z, int w, int h) {
    int idx = AddStockpile(x, y, z, w, h);
    if (idx >= 0) {
        stockpiles[idx].active = true;
        stockpiles[idx].priority = 5;
        stockpiles[idx].maxStackSize = 10;
        
        // Accept all items by default
        for (int i = 0; i < ITEM_TYPE_COUNT; i++) {
            stockpiles[idx].allowedTypes[i] = true;
        }
        for (int i = 0; i < MAT_COUNT; i++) {
            stockpiles[idx].allowedMaterials[i] = true;
        }
    }
    return idx;
}

static int SpawnTestItem(int x, int y, int z, ItemType type, MaterialType material) {
    int idx = SpawnItem(type, x, y, z, 1);
    if (idx >= 0) {
        items[idx].material = material;
        items[idx].state = ITEM_ON_GROUND;
    }
    return idx;
}

// =============================================================================
// TEST: Basic Linking Operations
// =============================================================================

static void test_link_stockpile_to_workshop(void) {
    describe("Link stockpile to workshop");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int spIdx = CreateTestStockpile(5, 5, 0, 4, 4);
    
    it("should successfully link stockpile to workshop") {
        bool success = LinkStockpileToWorkshop(wsIdx, spIdx);
        expect(success == true);
        expect(workshops[wsIdx].linkedInputCount == 1);
        expect(workshops[wsIdx].linkedInputStockpiles[0] == spIdx);
    }
    
    it("should detect stockpile is linked") {
        bool linked = IsStockpileLinked(wsIdx, spIdx);
        expect(linked == true);
    }
    
    ClearTestState();
}

static void test_link_multiple_stockpiles(void) {
    describe("Link multiple stockpiles to workshop");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 5, 0, 4, 4);
    int sp3 = CreateTestStockpile(5, 15, 0, 4, 4);
    int sp4 = CreateTestStockpile(15, 15, 0, 4, 4);
    
    it("should link up to 4 stockpiles") {
        expect(LinkStockpileToWorkshop(wsIdx, sp1) == true);
        expect(LinkStockpileToWorkshop(wsIdx, sp2) == true);
        expect(LinkStockpileToWorkshop(wsIdx, sp3) == true);
        expect(LinkStockpileToWorkshop(wsIdx, sp4) == true);
        
        expect(workshops[wsIdx].linkedInputCount == 4);
        expect(workshops[wsIdx].linkedInputStockpiles[0] == sp1);
        expect(workshops[wsIdx].linkedInputStockpiles[1] == sp2);
        expect(workshops[wsIdx].linkedInputStockpiles[2] == sp3);
        expect(workshops[wsIdx].linkedInputStockpiles[3] == sp4);
    }
    
    ClearTestState();
}

static void test_reject_fifth_link(void) {
    describe("Reject linking 5th stockpile");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 5, 0, 4, 4);
    int sp3 = CreateTestStockpile(5, 15, 0, 4, 4);
    int sp4 = CreateTestStockpile(15, 15, 0, 4, 4);
    int sp5 = CreateTestStockpile(20, 20, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, sp1);
    LinkStockpileToWorkshop(wsIdx, sp2);
    LinkStockpileToWorkshop(wsIdx, sp3);
    LinkStockpileToWorkshop(wsIdx, sp4);
    
    it("should reject 5th link (max 4 slots)") {
        bool success = LinkStockpileToWorkshop(wsIdx, sp5);
        expect(success == false);
        expect(workshops[wsIdx].linkedInputCount == 4);
    }
    
    ClearTestState();
}

static void test_reject_duplicate_link(void) {
    describe("Reject linking same stockpile twice");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int spIdx = CreateTestStockpile(5, 5, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, spIdx);
    
    it("should reject duplicate link") {
        bool success = LinkStockpileToWorkshop(wsIdx, spIdx);
        expect(success == false);
        expect(workshops[wsIdx].linkedInputCount == 1);
    }
    
    ClearTestState();
}

// =============================================================================
// TEST: Unlinking Operations
// =============================================================================

static void test_unlink_stockpile_by_slot(void) {
    describe("Unlink stockpile by slot index");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 5, 0, 4, 4);
    int sp3 = CreateTestStockpile(5, 15, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, sp1);
    LinkStockpileToWorkshop(wsIdx, sp2);
    LinkStockpileToWorkshop(wsIdx, sp3);
    
    it("should unlink slot 1 and shift remaining") {
        UnlinkStockpileSlot(wsIdx, 1);  // Remove sp2
        expect(workshops[wsIdx].linkedInputCount == 2);
        expect(workshops[wsIdx].linkedInputStockpiles[0] == sp1);
        expect(workshops[wsIdx].linkedInputStockpiles[1] == sp3);  // Shifted down
    }
    
    ClearTestState();
}

static void test_unlink_stockpile_by_index(void) {
    describe("Unlink stockpile by stockpile index");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 5, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, sp1);
    LinkStockpileToWorkshop(wsIdx, sp2);
    
    it("should find and unlink specific stockpile") {
        bool success = UnlinkStockpile(wsIdx, sp1);
        expect(success == true);
        expect(workshops[wsIdx].linkedInputCount == 1);
        expect(workshops[wsIdx].linkedInputStockpiles[0] == sp2);
    }
    
    it("should return false if stockpile not linked") {
        bool success = UnlinkStockpile(wsIdx, sp1);  // Already unlinked
        expect(success == false);
    }
    
    ClearTestState();
}

static void test_clear_all_links(void) {
    describe("Clear all stockpile links");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 5, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, sp1);
    LinkStockpileToWorkshop(wsIdx, sp2);
    
    it("should clear all links at once") {
        ClearLinkedStockpiles(wsIdx);
        expect(workshops[wsIdx].linkedInputCount == 0);
    }
    
    ClearTestState();
}

// =============================================================================
// TEST: Ingredient Search with Linked Stockpiles
// =============================================================================

static void test_search_only_linked_stockpiles(void) {
    describe("Ingredient search restricted to linked stockpiles");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);   // Linked
    int sp2 = CreateTestStockpile(15, 15, 0, 4, 4); // Not linked
    
    // Put items in both stockpiles
    int item1 = SpawnTestItem(6, 6, 0, ITEM_ROCK, MAT_GRANITE);
    int item2 = SpawnTestItem(16, 16, 0, ITEM_ROCK, MAT_GRANITE);
    
    // Add items to stockpiles
    AddItemToStockpile(sp1, item1);
    AddItemToStockpile(sp2, item2);
    
    // Link only sp1
    LinkStockpileToWorkshop(wsIdx, sp1);
    
    it("should find item only in linked stockpile") {
        Recipe recipe = { .inputType = ITEM_ROCK, .inputCount = 1 };
        int foundItem = FindInputItem(&workshops[wsIdx], &recipe, 0);
        
        expect(foundItem == item1);  // Should find item1 from sp1, not item2 from sp2
    }
    
    ClearTestState();
}

static void test_search_all_stockpiles_when_no_links(void) {
    describe("Ingredient search falls back to radius when no links");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 15, 0, 4, 4);
    
    int item1 = SpawnTestItem(6, 6, 0, ITEM_ROCK, MAT_GRANITE);
    int item2 = SpawnTestItem(16, 16, 0, ITEM_ROCK, MAT_GRANITE);
    
    AddItemToStockpile(sp1, item1);
    AddItemToStockpile(sp2, item2);
    
    // No links
    expect(workshops[wsIdx].linkedInputCount == 0);
    
    it("should find nearest item from any stockpile") {
        Recipe recipe = { .inputType = ITEM_ROCK, .inputCount = 1 };
        int foundItem = FindInputItem(&workshops[wsIdx], &recipe, 0);
        
        // Should find item1 (closer to workshop at 10,10)
        expect(foundItem == item1 || foundItem == item2);  // Either is valid
    }
    
    ClearTestState();
}

static void test_no_items_in_linked_stockpiles(void) {
    describe("Return -1 when linked stockpiles have no matching items");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);
    int sp2 = CreateTestStockpile(15, 15, 0, 4, 4);
    
    // Put ROCK in sp2 (not linked)
    int item = SpawnTestItem(16, 16, 0, ITEM_ROCK, MAT_GRANITE);
    AddItemToStockpile(sp2, item);
    
    // Link only sp1 (which is empty)
    LinkStockpileToWorkshop(wsIdx, sp1);
    
    it("should return -1 (no materials found)") {
        Recipe recipe = { .inputType = ITEM_ROCK, .inputCount = 1 };
        int foundItem = FindInputItem(&workshops[wsIdx], &recipe, 0);
        
        expect(foundItem == -1);  // Should NOT find item in sp2 since only sp1 is linked
    }
    
    ClearTestState();
}

// =============================================================================
// TEST: Bill Execution with Linked Stockpiles
// =============================================================================

static void test_bill_uses_linked_stockpile_only(void) {
    describe("Bill execution respects linked stockpiles");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);   // Oak logs
    int sp2 = CreateTestStockpile(15, 15, 0, 4, 4); // Pine logs
    
    // Filter stockpiles
    for (int i = 0; i < ITEM_TYPE_COUNT; i++) {
        stockpiles[sp1].allowedTypes[i] = false;
        stockpiles[sp2].allowedTypes[i] = false;
    }
    stockpiles[sp1].allowedTypes[ITEM_LOG] = true;
    stockpiles[sp2].allowedTypes[ITEM_LOG] = true;
    
    for (int i = 0; i < MAT_COUNT; i++) {
        stockpiles[sp1].allowedMaterials[i] = false;
        stockpiles[sp2].allowedMaterials[i] = false;
    }
    stockpiles[sp1].allowedMaterials[MAT_OAK] = true;
    stockpiles[sp2].allowedMaterials[MAT_PINE] = true;
    
    // Put oak log in sp1, pine log in sp2
    int oakLog = SpawnTestItem(6, 6, 0, ITEM_LOG, MAT_OAK);
    int pineLog = SpawnTestItem(16, 16, 0, ITEM_LOG, MAT_PINE);
    AddItemToStockpile(sp1, oakLog);
    AddItemToStockpile(sp2, pineLog);
    
    // Link only oak stockpile
    LinkStockpileToWorkshop(wsIdx, sp1);
    
    // Add bill for sawmill recipe (LOG -> PLANKS)
    int billIdx = AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);  // Recipe 0 = saw planks
    
    it("should find oak log from linked sp1, not pine log from sp2") {
        Recipe* recipe = GetRecipeForBill(&workshops[wsIdx], &workshops[wsIdx].bills[billIdx]);
        int foundItem = FindInputItem(&workshops[wsIdx], recipe, 0);
        
        expect(foundItem == oakLog);  // Should use oak, not pine
        expect(items[foundItem].material == MAT_OAK);
    }
    
    ClearTestState();
}

static void test_bill_cannot_run_without_linked_items(void) {
    describe("Bill cannot run when linked stockpiles are empty");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int sp1 = CreateTestStockpile(5, 5, 0, 4, 4);   // Empty, linked
    int sp2 = CreateTestStockpile(15, 15, 0, 4, 4); // Has items, not linked
    
    int item = SpawnTestItem(16, 16, 0, ITEM_ROCK, MAT_GRANITE);
    AddItemToStockpile(sp2, item);
    
    LinkStockpileToWorkshop(wsIdx, sp1);
    
    int billIdx = AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
    
    it("should not assign craft job (no materials in linked stockpile)") {
        // Create a mover to attempt job
        int moverIdx = AddMover(12, 12, 0);
        movers[moverIdx].active = true;
        
        Job* job = WorkGiver_Craft(&movers[moverIdx]);
        
        expect(job == NULL);  // Should not create job
    }
    
    ClearTestState();
}

// =============================================================================
// TEST: Edge Cases
// =============================================================================

static void test_delete_linked_stockpile(void) {
    describe("Delete stockpile that is linked to workshop");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int spIdx = CreateTestStockpile(5, 5, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, spIdx);
    
    it("should remove link when stockpile deleted") {
        RemoveStockpile(spIdx);
        
        // Workshop should detect stockpile is gone and clean up
        // (This requires implementation in RemoveStockpile)
        bool linked = IsStockpileLinked(wsIdx, spIdx);
        expect(linked == false);
    }
    
    ClearTestState();
}

static void test_delete_workshop_with_links(void) {
    describe("Delete workshop that has linked stockpiles");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int spIdx = CreateTestStockpile(5, 5, 0, 4, 4);
    
    LinkStockpileToWorkshop(wsIdx, spIdx);
    
    it("should cleanly delete workshop (no dangling references)") {
        RemoveWorkshop(wsIdx);
        
        expect(workshops[wsIdx].active == false);
        // No crash or assertion failures
    }
    
    ClearTestState();
}

static void test_link_same_stockpile_to_multiple_workshops(void) {
    describe("Link same stockpile to multiple workshops");
    
    SetupBasicScene();
    ClearTestState();
    
    int ws1 = CreateTestWorkshop(5, 5, 0, WORKSHOP_STONECUTTER);
    int ws2 = CreateTestWorkshop(15, 15, 0, WORKSHOP_SAWMILL);
    int spIdx = CreateTestStockpile(10, 10, 0, 4, 4);
    
    it("should allow linking same stockpile to multiple workshops") {
        bool success1 = LinkStockpileToWorkshop(ws1, spIdx);
        bool success2 = LinkStockpileToWorkshop(ws2, spIdx);
        
        expect(success1 == true);
        expect(success2 == true);
        expect(IsStockpileLinked(ws1, spIdx) == true);
        expect(IsStockpileLinked(ws2, spIdx) == true);
    }
    
    ClearTestState();
}

static void test_link_unreachable_stockpile(void) {
    describe("Link stockpile on different z-level");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_STONECUTTER);
    int spIdx = CreateTestStockpile(10, 10, 1, 4, 4);  // Different z-level
    
    it("should allow linking (reachability checked at job assignment)") {
        bool success = LinkStockpileToWorkshop(wsIdx, spIdx);
        expect(success == true);
    }
    
    it("should not find items if unreachable") {
        // Put item in unreachable stockpile
        int item = SpawnTestItem(11, 11, 1, ITEM_ROCK, MAT_GRANITE);
        AddItemToStockpile(spIdx, item);
        
        // Try to find it for bill
        Recipe recipe = { .inputType = ITEM_ROCK, .inputCount = 1 };
        int foundItem = FindInputItem(&workshops[wsIdx], &recipe, 0);
        
        // Should fail reachability check during search
        expect(foundItem == -1);
    }
    
    ClearTestState();
}

// =============================================================================
// TEST: Material Matching with Linked Stockpiles
// =============================================================================

static void test_material_specific_linking(void) {
    describe("Material-specific stockpile linking");
    
    SetupBasicScene();
    ClearTestState();
    
    int wsIdx = CreateTestWorkshop(10, 10, 0, WORKSHOP_SAWMILL);
    int oakStockpile = CreateTestStockpile(5, 5, 0, 4, 4);
    int pineStockpile = CreateTestStockpile(15, 5, 0, 4, 4);
    
    // Filter by material
    for (int i = 0; i < MAT_COUNT; i++) {
        stockpiles[oakStockpile].allowedMaterials[i] = false;
        stockpiles[pineStockpile].allowedMaterials[i] = false;
    }
    stockpiles[oakStockpile].allowedMaterials[MAT_OAK] = true;
    stockpiles[pineStockpile].allowedMaterials[MAT_PINE] = true;
    
    // Put logs in each
    int oakLog1 = SpawnTestItem(6, 6, 0, ITEM_LOG, MAT_OAK);
    int oakLog2 = SpawnTestItem(7, 7, 0, ITEM_LOG, MAT_OAK);
    int pineLog = SpawnTestItem(16, 16, 0, ITEM_LOG, MAT_PINE);
    
    AddItemToStockpile(oakStockpile, oakLog1);
    AddItemToStockpile(oakStockpile, oakLog2);
    AddItemToStockpile(pineStockpile, pineLog);
    
    // Link only oak stockpile
    LinkStockpileToWorkshop(wsIdx, oakStockpile);
    
    it("should only find oak logs from linked stockpile") {
        Recipe recipe = {
            .inputType = ITEM_LOG,
            .inputCount = 1,
            .inputMaterialMatch = MATCH_WOOD  // Any wood
        };
        
        int foundItem = FindInputItem(&workshops[wsIdx], &recipe, 0);
        
        expect(foundItem == oakLog1 || foundItem == oakLog2);
        expect(items[foundItem].material == MAT_OAK);
    }
    
    ClearTestState();
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(int argc, char** argv) {
    // Check for verbose flag
    test_verbose = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            test_verbose = 1;
        }
    }
    
    printf("\n=== Workshop-Stockpile Linking Tests ===\n\n");
    
    // Basic linking operations
    test("Link stockpile to workshop", test_link_stockpile_to_workshop);
    test("Link multiple stockpiles", test_link_multiple_stockpiles);
    test("Reject 5th link", test_reject_fifth_link);
    test("Reject duplicate link", test_reject_duplicate_link);
    
    // Unlinking operations
    test("Unlink by slot", test_unlink_stockpile_by_slot);
    test("Unlink by index", test_unlink_stockpile_by_index);
    test("Clear all links", test_clear_all_links);
    
    // Ingredient search
    test("Search only linked stockpiles", test_search_only_linked_stockpiles);
    test("Search all when no links", test_search_all_stockpiles_when_no_links);
    test("No items in linked stockpiles", test_no_items_in_linked_stockpiles);
    
    // Bill execution
    test("Bill uses linked stockpile", test_bill_uses_linked_stockpile_only);
    test("Bill cannot run without items", test_bill_cannot_run_without_linked_items);
    
    // Edge cases
    test("Delete linked stockpile", test_delete_linked_stockpile);
    test("Delete workshop with links", test_delete_workshop_with_links);
    test("Same stockpile to multiple workshops", test_link_same_stockpile_to_multiple_workshops);
    test("Link unreachable stockpile", test_link_unreachable_stockpile);
    
    // Material matching
    test("Material-specific linking", test_material_specific_linking);
    
    return show_test_summary();
}
