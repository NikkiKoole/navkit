// test_materials.c - Tests for the data-driven materials system
// Tests for Stages A-E of the materials implementation

#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/designations.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/stockpiles.h"
#include "../src/simulation/fire.h"
#include <stdlib.h>
#include <string.h>

// Helper to count items of a specific type
static int CountItemsOfType(ItemType type) {
    int count = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type) {
            count++;
        }
    }
    return count;
}

static int CountItemsOfTypeWithMaterial(ItemType type, MaterialType mat) {
    int count = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type && items[i].material == (uint8_t)mat) {
            count++;
        }
    }
    return count;
}

// Helper to run fire ticks
static void RunFireTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateFire();
    }
}

// =============================================================================
// Stage A: Material Grid Initialization
// =============================================================================

describe(material_grid_initialization) {
    it("should initialize wall materials to MAT_GRANITE (natural) and floor materials to MAT_NONE") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        // InitMaterials is called by InitGridFromAsciiWithChunkSize via InitGridWithSizeAndChunkSize
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(GetWallMaterial(x, y, z) == MAT_GRANITE);
                    expect(IsWallNatural(x, y, z));
                    expect(GetFloorMaterial(x, y, z) == MAT_NONE);
                    expect(!IsFloorNatural(x, y, z));
                }
            }
        }
    }
    
    it("should allow setting and getting wall material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        SetWallMaterial(1, 0, 0, MAT_OAK);
        expect(GetWallMaterial(1, 0, 0) == MAT_OAK);
        
        SetWallMaterial(2, 0, 0, MAT_GRANITE);
        expect(GetWallMaterial(2, 0, 0) == MAT_GRANITE);
        
        // Other cells still natural granite
        expect(GetWallMaterial(0, 0, 0) == MAT_GRANITE);
        expect(IsWallNatural(0, 0, 0));
        expect(GetWallMaterial(3, 0, 0) == MAT_GRANITE);
        expect(IsWallNatural(3, 0, 0));
    }
    
    it("should allow setting and getting floor material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        SetFloorMaterial(1, 0, 0, MAT_OAK);
        expect(GetFloorMaterial(1, 0, 0) == MAT_OAK);
        
        SetFloorMaterial(2, 0, 0, MAT_GRANITE);
        expect(GetFloorMaterial(2, 0, 0) == MAT_GRANITE);
        
        // Other cells still none
        expect(GetFloorMaterial(0, 0, 0) == MAT_NONE);
        expect(GetFloorMaterial(3, 0, 0) == MAT_NONE);
    }
    
    it("should identify constructed walls correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        SetWallNatural(0, 0, 0);
        expect(!IsConstructedWall(0, 0, 0));  // natural wall

        grid[0][0][1] = CELL_WALL;
        SetWallMaterial(1, 0, 0, MAT_OAK);
        ClearWallNatural(1, 0, 0);
        expect(IsConstructedWall(1, 0, 0));   // constructed wood

        grid[0][0][2] = CELL_WALL;
        SetWallMaterial(2, 0, 0, MAT_GRANITE);
        ClearWallNatural(2, 0, 0);
        expect(IsConstructedWall(2, 0, 0));   // constructed stone
    }
}

// =============================================================================
// Stage B: CellDef and ItemDef Extensions
// =============================================================================

describe(cell_def_drops) {
    it("should have dropsItem for wall") {
        expect(CellDropsItem(CELL_WALL) == ITEM_ROCK);
    }
    
    // Note: CELL_WOOD_WALL removed - use CELL_WALL + wood material instead
    // Wood material drops are tested in mining_material_drops
    
    it("should have dropsItem for tree trunk") {
        expect(CellDropsItem(CELL_TREE_TRUNK) == ITEM_WOOD);
    }
    
    it("should have dropsItem for sapling") {
        expect(CellDropsItem(CELL_SAPLING) == ITEM_NONE);
    }
    
    it("should have ITEM_NONE for air") {
        expect(CellDropsItem(CELL_AIR) == ITEM_NONE);
    }
    
    it("should have ITEM_DIRT for dirt") {
        expect(CellDropsItem(CELL_DIRT) == ITEM_DIRT);
    }
    
    it("should have ITEM_NONE for bedrock") {
        expect(CellDropsItem(CELL_BEDROCK) == ITEM_NONE);
    }
    
    it("should have dropCount of 1 for wall") {
        expect(CellDropCount(CELL_WALL) == 1);
    }
    
    it("should have dropCount of 0 for non-mineable cells") {
        expect(CellDropCount(CELL_AIR) == 0);
        expect(CellDropCount(CELL_BEDROCK) == 0);
    }

    it("should have dropCount of 1 for dirt") {
        expect(CellDropCount(CELL_DIRT) == 1);
    }
}

describe(item_def_materials) {
    it("should provide sensible default materials for items") {
        expect(DefaultMaterialForItemType(ITEM_WOOD) == MAT_OAK);
        expect(DefaultMaterialForItemType(ITEM_BLOCKS) == MAT_GRANITE);
        expect(DefaultMaterialForItemType(ITEM_ROCK) == MAT_GRANITE);
        expect(DefaultMaterialForItemType(ITEM_DIRT) == MAT_DIRT);
        expect(DefaultMaterialForItemType(ITEM_RED) == MAT_NONE);
    }
}

describe(material_def_properties) {
    it("should have correct fuel values") {
        expect(MaterialFuel(MAT_OAK) == 128);   // Wood burns
        expect(MaterialFuel(MAT_GRANITE) == 0); // Stone doesn't burn
        expect(MaterialFuel(MAT_IRON) == 0);    // Iron doesn't burn
        expect(MaterialFuel(MAT_NONE) == 0);    // None has no fuel
    }
    
    it("should have correct flammability flags") {
        expect(MaterialIsFlammable(MAT_OAK));
        expect(!MaterialIsFlammable(MAT_GRANITE));
        expect(!MaterialIsFlammable(MAT_IRON));
        expect(!MaterialIsFlammable(MAT_NONE));
    }
    
    it("should have correct drop items") {
        expect(MaterialDropsItem(MAT_OAK) == ITEM_WOOD);
        expect(MaterialDropsItem(MAT_GRANITE) == ITEM_BLOCKS);
        expect(MaterialDropsItem(MAT_NONE) == ITEM_NONE);
    }
}

// =============================================================================
// Stage C: Blueprint Material Tracking
// =============================================================================

describe(blueprint_material_tracking) {
    it("should initialize blueprint with MAT_NONE") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Create floor so we can place blueprint
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        expect(bpIdx >= 0);
        expect(blueprints[bpIdx].deliveredMaterial == MAT_NONE);
    }
    
    it("should set wood material when delivering wood item") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        expect(bpIdx >= 0);
        
        // Spawn and deliver wood (oak)
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_WOOD, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        expect(blueprints[bpIdx].deliveredMaterial == MAT_OAK);
    }
    
    it("should set stone material when delivering stone blocks") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        expect(bpIdx >= 0);
        
        // Spawn and deliver granite blocks
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_BLOCKS, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        expect(blueprints[bpIdx].deliveredMaterial == MAT_GRANITE);
    }
    
    it("should set wall material when completing wall blueprint with wood") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        
        // Deliver wood (pine) and complete
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_WOOD, MAT_PINE);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        // Simulate builder completing
        blueprints[bpIdx].progress = BUILD_WORK_TIME;
        CompleteBlueprint(bpIdx);
        
        // Cell should be CELL_WALL with MAT_PINE (constructed)
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_PINE);
        expect(!IsWallNatural(1, 0, 0));
    }
    
    it("should set wall material when completing wall blueprint with stone") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        
        // Deliver granite blocks and complete
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_BLOCKS, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        blueprints[bpIdx].progress = BUILD_WORK_TIME;
        CompleteBlueprint(bpIdx);
        
        // Cell should be CELL_WALL with MAT_GRANITE (constructed)
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_GRANITE);
        expect(!IsWallNatural(1, 0, 0));
    }
}

// =============================================================================
// Stage D: Mining Drops Based on Material
// =============================================================================

describe(mining_material_drops) {
    it("should drop based on natural cell type for natural granite wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);  // # = wall
        InitDesignations();
        ClearItems();
        
        // Wall is natural granite
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        SetWallNatural(0, 0, 0);
        expect(GetWallMaterial(0, 0, 0) == MAT_GRANITE);
        expect(IsWallNatural(0, 0, 0));
        expect(grid[0][0][0] == CELL_WALL);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_ROCK with granite material
        expect(CountItemsOfTypeWithMaterial(ITEM_ROCK, MAT_GRANITE) == 1);
    }
    
    it("should drop ITEM_WOOD when mining constructed wood wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Set wall to oak material (constructed)
        SetWallMaterial(0, 0, 0, MAT_OAK);
        ClearWallNatural(0, 0, 0);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_WOOD with oak material
        expect(CountItemsOfTypeWithMaterial(ITEM_WOOD, MAT_OAK) == 1);
        
        // Should NOT have spawned ITEM_ROCK
        int orangeCount = CountItemsOfType(ITEM_ROCK);
        expect(orangeCount == 0);
    }
    
    it("should drop ITEM_BLOCKS when mining constructed granite wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Set wall to granite material (constructed)
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        ClearWallNatural(0, 0, 0);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_BLOCKS with granite material
        expect(CountItemsOfTypeWithMaterial(ITEM_BLOCKS, MAT_GRANITE) == 1);
    }
    
    it("should reset wall material to MAT_NONE after mining") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SetWallMaterial(0, 0, 0, MAT_OAK);
        ClearWallNatural(0, 0, 0);
        expect(GetWallMaterial(0, 0, 0) == MAT_OAK);
        
        CompleteMineDesignation(0, 0, 0);
        
        expect(GetWallMaterial(0, 0, 0) == MAT_NONE);
    }
    
    it("should create natural granite floor when mining natural rock") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Wall starts as natural granite
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        SetWallNatural(0, 0, 0);
        expect(GetWallMaterial(0, 0, 0) == MAT_GRANITE);
        expect(GetFloorMaterial(0, 0, 0) == MAT_NONE);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Wall removed, floor created with natural granite
        expect(GetWallMaterial(0, 0, 0) == MAT_NONE);
        expect(GetFloorMaterial(0, 0, 0) == MAT_GRANITE);
        expect(IsFloorNatural(0, 0, 0));
    }
    
    it("should use GetWallDropItem for correct material-based drops") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        
        // Natural wall
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        SetWallNatural(0, 0, 0);
        expect(GetWallDropItem(0, 0, 0) == ITEM_ROCK);
        
        // Wood wall
        SetWallMaterial(0, 0, 0, MAT_OAK);
        ClearWallNatural(0, 0, 0);
        expect(GetWallDropItem(0, 0, 0) == ITEM_WOOD);
        
        // Granite wall
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        ClearWallNatural(0, 0, 0);
        expect(GetWallDropItem(0, 0, 0) == ITEM_BLOCKS);
    }
}

describe(floor_removal_material_drops) {
    it("should drop based on floor material when removing") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Create a constructed floor with wood material
        SET_FLOOR(1, 0, 0);
        SetFloorMaterial(1, 0, 0, MAT_OAK);
        ClearFloorNatural(1, 0, 0);
        
        CompleteRemoveFloorDesignation(1, 0, 0, -1);
        
        // Should have spawned ITEM_WOOD with oak material
        expect(CountItemsOfTypeWithMaterial(ITEM_WOOD, MAT_OAK) == 1);
    }
    
    it("should reset floor material after floor removal") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        SetFloorMaterial(1, 0, 0, MAT_GRANITE);
        ClearFloorNatural(1, 0, 0);
        
        CompleteRemoveFloorDesignation(1, 0, 0, -1);
        
        expect(GetFloorMaterial(1, 0, 0) == MAT_NONE);
    }
}

// =============================================================================
// Stage E: Material Flammability
// =============================================================================

describe(material_flammability) {
    it("should allow CELL_WALL with MAT_OAK to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            ".#..\n"
            "....\n", 4, 3);
        InitFire();
        
        // Set the wall to wood material
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_OAK);
        ClearWallNatural(1, 1, 0);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should be burning (wood has fuel)
        expect(HasFire(1, 1, 0));
    }
    
    it("should NOT allow CELL_WALL with MAT_GRANITE to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            ".#..\n"
            "....\n", 4, 3);
        InitFire();
        
        // Set the wall to stone material
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_GRANITE);
        ClearWallNatural(1, 1, 0);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should NOT be burning (stone has no fuel)
        expect(!HasFire(1, 1, 0));
    }
    
    it("should NOT allow natural granite wall to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            ".#..\n"
            "....\n", 4, 3);
        InitFire();
        
        // Natural granite wall
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_GRANITE);
        SetWallNatural(1, 1, 0);
        expect(GetWallMaterial(1, 1, 0) == MAT_GRANITE);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should NOT be burning (raw wall has fuel=0)
        expect(!HasFire(1, 1, 0));
    }
    
    it("should allow constructed wood wall (CELL_WALL + MAT_OAK) to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n", 4, 3);
        InitFire();
        
        // Place wall with wood material
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_OAK);
        ClearWallNatural(1, 1, 0);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should be burning (MAT_OAK has fuel=128)
        expect(HasFire(1, 1, 0));
    }
    
    it("should verify material fuel values directly") {
        expect(MaterialFuel(MAT_OAK) > 0);      // Wood has fuel
        expect(MaterialFuel(MAT_GRANITE) == 0); // Stone has no fuel
        expect(MaterialFuel(MAT_IRON) == 0);    // Iron has no fuel
        expect(MaterialFuel(MAT_NONE) == 0);    // None has no fuel
    }
}

// =============================================================================
// Integration: Full Build-Mine Cycle
// =============================================================================

describe(build_mine_cycle) {
    it("should complete full cycle: build wood wall then mine it") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Step 1: Create floor and blueprint
        SET_FLOOR(1, 0, 0);
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        
        // Step 2: Deliver wood and complete build
        int woodItem = SpawnItemWithMaterial(0, 0, 0, ITEM_WOOD, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, woodItem);
        CompleteBlueprint(bpIdx);
        
        // Verify: wall exists with wood material
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_OAK);
        
        // Step 3: Mine the wall
        ClearItems();  // Clear any leftover items
        CompleteMineDesignation(1, 0, 0);
        
        // Verify: wall gone, material reset, wood dropped
        expect(grid[0][0][1] == CELL_AIR);
        expect(GetWallMaterial(1, 0, 0) == MAT_NONE);
        expect(CountItemsOfTypeWithMaterial(ITEM_WOOD, MAT_OAK) == 1);
        expect(CountItemsOfType(ITEM_ROCK) == 0);
    }
    
    it("should complete full cycle: build stone wall then mine it") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Build with stone
        SET_FLOOR(1, 0, 0);
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        int stoneItem = SpawnItemWithMaterial(0, 0, 0, ITEM_BLOCKS, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, stoneItem);
        CompleteBlueprint(bpIdx);
        
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_GRANITE);
        
        // Mine it
        ClearItems();
        CompleteMineDesignation(1, 0, 0);
        
        // Should drop stone blocks, not raw stone
        expect(CountItemsOfTypeWithMaterial(ITEM_BLOCKS, MAT_GRANITE) == 1);
        expect(CountItemsOfType(ITEM_ROCK) == 0);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }
    
    // Stage A: Material grid initialization
    test(material_grid_initialization);
    
    // Stage B: CellDef and ItemDef extensions
    test(cell_def_drops);
    test(item_def_materials);
    test(material_def_properties);
    
    // Stage C: Blueprint material tracking
    test(blueprint_material_tracking);
    
    // Stage D: Mining drops based on material
    test(mining_material_drops);
    test(floor_removal_material_drops);
    
    // Stage E: Material flammability
    test(material_flammability);
    
    // Integration tests
    test(build_mine_cycle);
    
    return 0;
}
