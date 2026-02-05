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
    it("should initialize wall materials to MAT_RAW and floor materials to MAT_NONE") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        // InitMaterials is called by InitGridFromAsciiWithChunkSize via InitGridWithSizeAndChunkSize
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(GetWallMaterial(x, y, z) == MAT_RAW);
                    expect(GetFloorMaterial(x, y, z) == MAT_NONE);
                }
            }
        }
    }
    
    it("should allow setting and getting wall material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        SetWallMaterial(1, 0, 0, MAT_WOOD);
        expect(GetWallMaterial(1, 0, 0) == MAT_WOOD);
        
        SetWallMaterial(2, 0, 0, MAT_STONE);
        expect(GetWallMaterial(2, 0, 0) == MAT_STONE);
        
        // Other cells still raw
        expect(GetWallMaterial(0, 0, 0) == MAT_RAW);
        expect(GetWallMaterial(3, 0, 0) == MAT_RAW);
    }
    
    it("should allow setting and getting floor material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        SetFloorMaterial(1, 0, 0, MAT_WOOD);
        expect(GetFloorMaterial(1, 0, 0) == MAT_WOOD);
        
        SetFloorMaterial(2, 0, 0, MAT_STONE);
        expect(GetFloorMaterial(2, 0, 0) == MAT_STONE);
        
        // Other cells still none
        expect(GetFloorMaterial(0, 0, 0) == MAT_NONE);
        expect(GetFloorMaterial(3, 0, 0) == MAT_NONE);
    }
    
    it("should identify constructed walls correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        expect(!IsConstructedWall(0, 0, 0));  // MAT_RAW
        
        SetWallMaterial(1, 0, 0, MAT_WOOD);
        expect(IsConstructedWall(1, 0, 0));   // MAT_WOOD
        
        SetWallMaterial(2, 0, 0, MAT_STONE);
        expect(IsConstructedWall(2, 0, 0));   // MAT_STONE
    }
}

// =============================================================================
// Stage B: CellDef and ItemDef Extensions
// =============================================================================

describe(cell_def_drops) {
    it("should have dropsItem for wall") {
        expect(CellDropsItem(CELL_WALL) == ITEM_ROCK);
    }
    
    // Note: CELL_WOOD_WALL removed - use CELL_WALL + MAT_WOOD instead
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
    
    it("should have ITEM_NONE for dirt") {
        expect(CellDropsItem(CELL_DIRT) == ITEM_NONE);
    }
    
    it("should have ITEM_NONE for bedrock") {
        expect(CellDropsItem(CELL_BEDROCK) == ITEM_NONE);
    }
    
    it("should have dropCount of 1 for wall") {
        expect(CellDropCount(CELL_WALL) == 1);
    }
    
    it("should have dropCount of 0 for non-mineable cells") {
        expect(CellDropCount(CELL_AIR) == 0);
        expect(CellDropCount(CELL_DIRT) == 0);
        expect(CellDropCount(CELL_BEDROCK) == 0);
    }
}

describe(item_def_materials) {
    it("should produce MAT_WOOD from wood item") {
        expect(ItemProducesMaterial(ITEM_WOOD) == MAT_WOOD);
    }
    
    it("should produce MAT_STONE from stone blocks") {
        expect(ItemProducesMaterial(ITEM_STONE_BLOCKS) == MAT_STONE);
    }
    
    it("should produce MAT_STONE from raw stone (orange)") {
        expect(ItemProducesMaterial(ITEM_ROCK) == MAT_STONE);
    }
    
    it("should produce MAT_NONE from non-building items") {
        expect(ItemProducesMaterial(ITEM_RED) == MAT_NONE);
        expect(ItemProducesMaterial(ITEM_GREEN) == MAT_NONE);
        expect(ItemProducesMaterial(ITEM_BLUE) == MAT_NONE);
        expect(ItemProducesMaterial(ITEM_SAPLING_OAK) == MAT_NONE);
        expect(ItemProducesMaterial(ITEM_SAPLING_PINE) == MAT_NONE);
        expect(ItemProducesMaterial(ITEM_SAPLING_BIRCH) == MAT_NONE);
        expect(ItemProducesMaterial(ITEM_SAPLING_WILLOW) == MAT_NONE);
    }
}

describe(material_def_properties) {
    it("should have correct fuel values") {
        expect(MaterialFuel(MAT_WOOD) == 128);  // Wood burns
        expect(MaterialFuel(MAT_STONE) == 0);   // Stone doesn't burn
        expect(MaterialFuel(MAT_IRON) == 0);    // Iron doesn't burn
        expect(MaterialFuel(MAT_RAW) == 0);     // Raw uses cell default
        expect(MaterialFuel(MAT_NONE) == 0);    // None has no fuel
    }
    
    it("should have correct flammability flags") {
        expect(MaterialIsFlammable(MAT_WOOD));
        expect(!MaterialIsFlammable(MAT_STONE));
        expect(!MaterialIsFlammable(MAT_IRON));
        expect(!MaterialIsFlammable(MAT_RAW));
        expect(!MaterialIsFlammable(MAT_NONE));
    }
    
    it("should have correct drop items") {
        expect(MaterialDropsItem(MAT_WOOD) == ITEM_WOOD);
        expect(MaterialDropsItem(MAT_STONE) == ITEM_STONE_BLOCKS);
        expect(MaterialDropsItem(MAT_RAW) == ITEM_NONE);
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
        
        // Spawn and deliver wood
        int itemIdx = SpawnItem(0, 0, 0, ITEM_WOOD);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        expect(blueprints[bpIdx].deliveredMaterial == MAT_WOOD);
    }
    
    it("should set stone material when delivering stone blocks") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        expect(bpIdx >= 0);
        
        // Spawn and deliver stone blocks
        int itemIdx = SpawnItem(0, 0, 0, ITEM_STONE_BLOCKS);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        expect(blueprints[bpIdx].deliveredMaterial == MAT_STONE);
    }
    
    it("should set wall material when completing wall blueprint with wood") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        
        // Deliver wood and complete
        int itemIdx = SpawnItem(0, 0, 0, ITEM_WOOD);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        // Simulate builder completing
        blueprints[bpIdx].progress = BUILD_WORK_TIME;
        CompleteBlueprint(bpIdx);
        
        // Cell should be CELL_WALL with MAT_WOOD
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_WOOD);
    }
    
    it("should set wall material when completing wall blueprint with stone") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        
        // Deliver stone and complete
        int itemIdx = SpawnItem(0, 0, 0, ITEM_STONE_BLOCKS);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        blueprints[bpIdx].progress = BUILD_WORK_TIME;
        CompleteBlueprint(bpIdx);
        
        // Cell should be CELL_WALL with MAT_STONE
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_STONE);
    }
}

// =============================================================================
// Stage D: Mining Drops Based on Material
// =============================================================================

describe(mining_material_drops) {
    it("should drop based on natural cell type for MAT_RAW wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);  // # = wall
        InitDesignations();
        ClearItems();
        
        // Wall is raw/natural (MAT_RAW)
        expect(GetWallMaterial(0, 0, 0) == MAT_RAW);
        expect(grid[0][0][0] == CELL_WALL);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_ROCK (default wall drop)
        int orangeCount = CountItemsOfType(ITEM_ROCK);
        expect(orangeCount == 1);
    }
    
    it("should drop ITEM_WOOD when mining MAT_WOOD wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Set wall to wood material
        SetWallMaterial(0, 0, 0, MAT_WOOD);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_WOOD
        int woodCount = CountItemsOfType(ITEM_WOOD);
        expect(woodCount == 1);
        
        // Should NOT have spawned ITEM_ROCK
        int orangeCount = CountItemsOfType(ITEM_ROCK);
        expect(orangeCount == 0);
    }
    
    it("should drop ITEM_STONE_BLOCKS when mining MAT_STONE wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Set wall to stone material
        SetWallMaterial(0, 0, 0, MAT_STONE);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_STONE_BLOCKS
        int stoneCount = CountItemsOfType(ITEM_STONE_BLOCKS);
        expect(stoneCount == 1);
    }
    
    it("should reset wall material to MAT_NONE after mining") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SetWallMaterial(0, 0, 0, MAT_WOOD);
        expect(GetWallMaterial(0, 0, 0) == MAT_WOOD);
        
        CompleteMineDesignation(0, 0, 0);
        
        expect(GetWallMaterial(0, 0, 0) == MAT_NONE);
    }
    
    it("should create rough floor when mining natural rock") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Wall starts as MAT_RAW (natural rock)
        expect(GetWallMaterial(0, 0, 0) == MAT_RAW);
        expect(GetFloorMaterial(0, 0, 0) == MAT_NONE);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Wall removed, floor created with rough stone (MAT_RAW)
        expect(GetWallMaterial(0, 0, 0) == MAT_NONE);
        expect(GetFloorMaterial(0, 0, 0) == MAT_RAW);
    }
    
    it("should use GetWallDropItem for correct material-based drops") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        
        // Natural wall
        expect(GetWallDropItem(0, 0, 0) == ITEM_ROCK);
        
        // Wood wall
        SetWallMaterial(0, 0, 0, MAT_WOOD);
        expect(GetWallDropItem(0, 0, 0) == ITEM_WOOD);
        
        // Stone wall
        SetWallMaterial(0, 0, 0, MAT_STONE);
        expect(GetWallDropItem(0, 0, 0) == ITEM_STONE_BLOCKS);
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
        SetFloorMaterial(1, 0, 0, MAT_WOOD);
        
        CompleteRemoveFloorDesignation(1, 0, 0, -1);
        
        // Should have spawned ITEM_WOOD
        int woodCount = CountItemsOfType(ITEM_WOOD);
        expect(woodCount == 1);
    }
    
    it("should reset floor material after floor removal") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        SetFloorMaterial(1, 0, 0, MAT_STONE);
        
        CompleteRemoveFloorDesignation(1, 0, 0, -1);
        
        expect(GetFloorMaterial(1, 0, 0) == MAT_NONE);
    }
}

// =============================================================================
// Stage E: Material Flammability
// =============================================================================

describe(material_flammability) {
    it("should allow CELL_WALL with MAT_WOOD to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            ".#..\n"
            "....\n", 4, 3);
        InitFire();
        
        // Set the wall to wood material
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_WOOD);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should be burning (wood has fuel)
        expect(HasFire(1, 1, 0));
    }
    
    it("should NOT allow CELL_WALL with MAT_STONE to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            ".#..\n"
            "....\n", 4, 3);
        InitFire();
        
        // Set the wall to stone material
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_STONE);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should NOT be burning (stone has no fuel)
        expect(!HasFire(1, 1, 0));
    }
    
    it("should NOT allow raw natural CELL_WALL to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            ".#..\n"
            "....\n", 4, 3);
        InitFire();
        
        // Natural stone wall (MAT_RAW)
        grid[0][1][1] = CELL_WALL;
        expect(GetWallMaterial(1, 1, 0) == MAT_RAW);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should NOT be burning (raw wall has fuel=0)
        expect(!HasFire(1, 1, 0));
    }
    
    it("should allow constructed wood wall (CELL_WALL + MAT_WOOD) to burn") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n", 4, 3);
        InitFire();
        
        // Place wall with wood material
        grid[0][1][1] = CELL_WALL;
        SetWallMaterial(1, 1, 0, MAT_WOOD);
        
        // Try to ignite
        IgniteCell(1, 1, 0);
        
        // Should be burning (MAT_WOOD has fuel=128)
        expect(HasFire(1, 1, 0));
    }
    
    it("should verify material fuel values directly") {
        expect(MaterialFuel(MAT_WOOD) > 0);     // Wood has fuel
        expect(MaterialFuel(MAT_STONE) == 0);   // Stone has no fuel
        expect(MaterialFuel(MAT_IRON) == 0);    // Iron has no fuel
        expect(MaterialFuel(MAT_RAW) == 0);     // Raw defers to cell
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
        int woodItem = SpawnItem(0, 0, 0, ITEM_WOOD);
        DeliverMaterialToBlueprint(bpIdx, woodItem);
        CompleteBlueprint(bpIdx);
        
        // Verify: wall exists with wood material
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_WOOD);
        
        // Step 3: Mine the wall
        ClearItems();  // Clear any leftover items
        CompleteMineDesignation(1, 0, 0);
        
        // Verify: wall gone, material reset, wood dropped
        expect(grid[0][0][1] == CELL_AIR);
        expect(GetWallMaterial(1, 0, 0) == MAT_NONE);
        expect(CountItemsOfType(ITEM_WOOD) == 1);
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
        int stoneItem = SpawnItem(0, 0, 0, ITEM_STONE_BLOCKS);
        DeliverMaterialToBlueprint(bpIdx, stoneItem);
        CompleteBlueprint(bpIdx);
        
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_STONE);
        
        // Mine it
        ClearItems();
        CompleteMineDesignation(1, 0, 0);
        
        // Should drop stone blocks, not raw stone
        expect(CountItemsOfType(ITEM_STONE_BLOCKS) == 1);
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
