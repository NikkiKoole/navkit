// Test suite for workshop-stockpile linking feature
// Tests core linking logic

#include "../vendor/c89spec.h"
#include "../src/entities/workshops.h"
#include "../src/entities/stockpiles.h"
#include <string.h>

// Global flag for verbose output
static bool test_verbose = false;

// =============================================================================
// TEST MODULES
// =============================================================================

describe(basic_linking) {
    it("should successfully link stockpile to workshop") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].type = WORKSHOP_STONECUTTER;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        
        stockpiles[0].active = true;
        stockpileCount = 1;
        
        bool success = LinkStockpileToWorkshop(0, 0);
        expect(success == true);
        expect(workshops[0].linkedInputCount == 1);
        expect(workshops[0].linkedInputStockpiles[0] == 0);
    }
    
    it("should detect stockpile is linked") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        stockpiles[0].active = true;
        stockpileCount = 1;
        
        LinkStockpileToWorkshop(0, 0);
        bool linked = IsStockpileLinked(0, 0);
        expect(linked == true);
    }
    
    it("should link up to 4 stockpiles") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        
        for (int i = 0; i < 4; i++) {
            stockpiles[i].active = true;
        }
        stockpileCount = 4;
        
        expect(LinkStockpileToWorkshop(0, 0) == true);
        expect(LinkStockpileToWorkshop(0, 1) == true);
        expect(LinkStockpileToWorkshop(0, 2) == true);
        expect(LinkStockpileToWorkshop(0, 3) == true);
        
        expect(workshops[0].linkedInputCount == 4);
        expect(workshops[0].linkedInputStockpiles[0] == 0);
        expect(workshops[0].linkedInputStockpiles[1] == 1);
        expect(workshops[0].linkedInputStockpiles[2] == 2);
        expect(workshops[0].linkedInputStockpiles[3] == 3);
    }
    
    it("should reject 5th link (max 4 slots)") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        
        for (int i = 0; i < 5; i++) {
            stockpiles[i].active = true;
        }
        stockpileCount = 5;
        
        LinkStockpileToWorkshop(0, 0);
        LinkStockpileToWorkshop(0, 1);
        LinkStockpileToWorkshop(0, 2);
        LinkStockpileToWorkshop(0, 3);
        
        bool success = LinkStockpileToWorkshop(0, 4);
        expect(success == false);
        expect(workshops[0].linkedInputCount == 4);
    }
    
    it("should reject duplicate link") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        stockpiles[0].active = true;
        stockpileCount = 1;
        
        LinkStockpileToWorkshop(0, 0);
        
        bool success = LinkStockpileToWorkshop(0, 0);
        expect(success == false);
        expect(workshops[0].linkedInputCount == 1);
    }
}

describe(unlinking) {
    it("should unlink by slot and shift remaining") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        
        for (int i = 0; i < 3; i++) {
            stockpiles[i].active = true;
        }
        stockpileCount = 3;
        
        LinkStockpileToWorkshop(0, 0);
        LinkStockpileToWorkshop(0, 1);
        LinkStockpileToWorkshop(0, 2);
        
        UnlinkStockpileSlot(0, 1);
        expect(workshops[0].linkedInputCount == 2);
        expect(workshops[0].linkedInputStockpiles[0] == 0);
        expect(workshops[0].linkedInputStockpiles[1] == 2);
    }
    
    it("should unlink by stockpile index") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        
        stockpiles[0].active = true;
        stockpiles[1].active = true;
        stockpileCount = 2;
        
        LinkStockpileToWorkshop(0, 0);
        LinkStockpileToWorkshop(0, 1);
        
        bool success = UnlinkStockpile(0, 0);
        expect(success == true);
        expect(workshops[0].linkedInputCount == 1);
        expect(workshops[0].linkedInputStockpiles[0] == 1);
    }
    
    it("should return false if stockpile not linked") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        stockpiles[0].active = true;
        stockpileCount = 1;
        
        bool success = UnlinkStockpile(0, 0);
        expect(success == false);
    }
    
    it("should clear all links") {
        ClearWorkshops();
        ClearStockpiles();
        
        workshops[0].active = true;
        workshops[0].linkedInputCount = 0;
        workshopCount = 1;
        
        stockpiles[0].active = true;
        stockpiles[1].active = true;
        stockpileCount = 2;
        
        LinkStockpileToWorkshop(0, 0);
        LinkStockpileToWorkshop(0, 1);
        
        ClearLinkedStockpiles(0);
        expect(workshops[0].linkedInputCount == 0);
    }
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(int argc, char** argv) {
    // Check for verbose flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            test_verbose = true;
        }
    }
    
    if (!test_verbose) {
        set_quiet_mode(1);
    }
    
    printf("\n=== Workshop-Stockpile Linking Tests ===\n\n");
    
    test(basic_linking);
    test(unlinking);
    
    return 0;
}
