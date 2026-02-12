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
#include "../src/simulation/temperature.h"
#include "../src/simulation/trees.h"
#include "../assets/atlas.h"
#include <stdlib.h>
#include <string.h>


// Global flag for verbose output in tests
static bool test_verbose = false;

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

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
static UNUSED void RunFireTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateFire();
    }
}

// =============================================================================
// Stage A: Material Grid Initialization
// =============================================================================

describe(material_grid_initialization) {
    it("should initialize air cells with MAT_NONE and wall cells with MAT_GRANITE") {
        InitGridFromAsciiWithChunkSize(
            ".#..\n"
            "....\n", 4, 2);
        
        // Air cells should have MAT_NONE (no wall material for empty space)
        expect(GetWallMaterial(0, 0, 0) == MAT_NONE);
        expect(!IsWallNatural(0, 0, 0));
        expect(GetFloorMaterial(0, 0, 0) == MAT_NONE);
        expect(!IsFloorNatural(0, 0, 0));
        
        // Wall cells should have MAT_GRANITE (natural) from SyncMaterialsToTerrain
        expect(GetWallMaterial(1, 0, 0) == MAT_GRANITE);
        expect(IsWallNatural(1, 0, 0));
    }
    
    it("should allow setting and getting wall material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        SetWallMaterial(1, 0, 0, MAT_OAK);
        expect(GetWallMaterial(1, 0, 0) == MAT_OAK);
        
        SetWallMaterial(2, 0, 0, MAT_GRANITE);
        expect(GetWallMaterial(2, 0, 0) == MAT_GRANITE);
        
        // Other air cells still MAT_NONE
        expect(GetWallMaterial(0, 0, 0) == MAT_NONE);
        expect(GetWallMaterial(3, 0, 0) == MAT_NONE);
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
        expect(CellDropsItem(CELL_TREE_TRUNK) == ITEM_LOG);
    }
    
    it("should have dropsItem for sapling") {
        expect(CellDropsItem(CELL_SAPLING) == ITEM_NONE);
    }
    
    it("should have ITEM_NONE for air") {
        expect(CellDropsItem(CELL_AIR) == ITEM_NONE);
    }
    
    it("should have ITEM_ROCK for wall") {
        expect(CellDropsItem(CELL_WALL) == ITEM_ROCK);
    }
    
    it("should have ITEM_NONE for air") {
        expect(CellDropsItem(CELL_AIR) == ITEM_NONE);
    }
    
    it("should have dropCount of 1 for wall") {
        expect(CellDropCount(CELL_WALL) == 1);
    }
    
    it("should have dropCount of 0 for non-mineable cells") {
        expect(CellDropCount(CELL_AIR) == 0);
    }

    it("should have dropCount of 1 for wall (any solid)") {
        expect(CellDropCount(CELL_WALL) == 1);
    }
}

describe(item_def_materials) {
    it("should provide sensible default materials for items") {
        expect(DefaultMaterialForItemType(ITEM_LOG) == MAT_OAK);
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
        expect(MaterialDropsItem(MAT_OAK) == ITEM_LOG);
        expect(MaterialDropsItem(MAT_GRANITE) == ITEM_ROCK);
        expect(MaterialDropsItem(MAT_NONE) == ITEM_NONE);
    }
}

// =============================================================================
// Stage C: Blueprint Material Tracking
// =============================================================================

describe(blueprint_material_tracking) {
    it("should initialize blueprint with MAT_NONE deliveries") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_LOG_WALL);
        expect(bpIdx >= 0);
        expect(blueprints[bpIdx].stageDeliveries[0].deliveredMaterial == MAT_NONE);
    }
    
    it("should set wood material when delivering wood item") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_LOG_WALL);
        expect(bpIdx >= 0);
        
        // Spawn and deliver wood (oak)
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        expect(blueprints[bpIdx].stageDeliveries[0].deliveredMaterial == MAT_OAK);
    }
    
    it("should set stone material when delivering stone blocks") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        
        // Spawn and deliver granite blocks
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_BLOCKS, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);
        
        expect(blueprints[bpIdx].stageDeliveries[0].deliveredMaterial == MAT_GRANITE);
    }
    
    it("should set wall material when completing log wall blueprint with pine") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_LOG_WALL);
        
        // Deliver 2 pine logs and complete
        for (int i = 0; i < 2; i++) {
            int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_PINE);
            DeliverMaterialToBlueprint(bpIdx, itemIdx);
        }
        
        CompleteBlueprint(bpIdx);
        
        // Cell should be CELL_WALL with MAT_PINE (constructed)
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetWallMaterial(1, 0, 0) == MAT_PINE);
        expect(!IsWallNatural(1, 0, 0));
    }
    
    it("should set wall material when completing dry stone wall with granite") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        SET_FLOOR(1, 0, 0);
        
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_DRY_STONE_WALL);
        
        // Deliver 3 granite rocks and complete
        for (int i = 0; i < 3; i++) {
            int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_ROCK, MAT_GRANITE);
            DeliverMaterialToBlueprint(bpIdx, itemIdx);
        }
        
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
        
        // Should have spawned ITEM_ROCK with granite material (via MaterialDropsItem)
        expect(CountItemsOfTypeWithMaterial(ITEM_ROCK, MAT_GRANITE) == 1);
    }
    
    it("should drop ITEM_LOG when mining constructed wood wall") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Set wall to oak material (constructed)
        SetWallMaterial(0, 0, 0, MAT_OAK);
        ClearWallNatural(0, 0, 0);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_LOG with oak material
        expect(CountItemsOfTypeWithMaterial(ITEM_LOG, MAT_OAK) == 1);
        
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
        
        // Should have spawned ITEM_ROCK with granite material (no source item set)
        expect(CountItemsOfTypeWithMaterial(ITEM_ROCK, MAT_GRANITE) == 1);
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
        
        // Natural granite wall - drops based on material (no source item)
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        SetWallNatural(0, 0, 0);
        expect(GetWallDropItem(0, 0, 0) == ITEM_ROCK);
        
        // Wood wall (no source item set)
        SetWallMaterial(0, 0, 0, MAT_OAK);
        ClearWallNatural(0, 0, 0);
        expect(GetWallDropItem(0, 0, 0) == ITEM_LOG);
        
        // Granite wall (no source item set)
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        ClearWallNatural(0, 0, 0);
        expect(GetWallDropItem(0, 0, 0) == ITEM_ROCK);
    }

    it("should use GetWallDropItem for CELL_WALL material-based drops") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_CLAY);
        expect(GetWallDropItem(0, 0, 0) == ITEM_CLAY);
        
        grid[0][0][1] = CELL_WALL;
        SetWallMaterial(1, 0, 0, MAT_PEAT);
        expect(GetWallDropItem(1, 0, 0) == ITEM_PEAT);
        
        grid[0][0][2] = CELL_WALL;
        SetWallMaterial(2, 0, 0, MAT_SAND);
        expect(GetWallDropItem(2, 0, 0) == ITEM_SAND);
        
        grid[0][0][3] = CELL_WALL;
        SetWallMaterial(3, 0, 0, MAT_DIRT);
        expect(GetWallDropItem(3, 0, 0) == ITEM_DIRT);
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
        
        // Should have spawned ITEM_LOG with oak material
        expect(CountItemsOfTypeWithMaterial(ITEM_LOG, MAT_OAK) == 1);
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
        
        // Step 1: Create floor and blueprint (log wall = 2 logs)
        SET_FLOOR(1, 0, 0);
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_LOG_WALL);
        
        // Step 2: Deliver 2 oak logs and complete build
        for (int i = 0; i < 2; i++) {
            int woodItem = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
            DeliverMaterialToBlueprint(bpIdx, woodItem);
        }
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
        expect(CountItemsOfTypeWithMaterial(ITEM_LOG, MAT_OAK) == 1);
        expect(CountItemsOfType(ITEM_ROCK) == 0);
    }
    
    it("should complete full cycle: build stone wall then mine it") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitDesignations();
        ClearItems();
        
        // Build with stone (dry stone wall = 3 rocks/blocks)
        SET_FLOOR(1, 0, 0);
        int bpIdx = CreateRecipeBlueprint(1, 0, 0, CONSTRUCTION_DRY_STONE_WALL);
        for (int i = 0; i < 3; i++) {
            int stoneItem = SpawnItemWithMaterial(0, 0, 0, ITEM_BLOCKS, MAT_GRANITE);
            DeliverMaterialToBlueprint(bpIdx, stoneItem);
        }
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
// Phase 0: Material Property Helpers
// =============================================================================

describe(material_sprites) {
    it("should return correct sprites for soil/stone materials") {
        expect(MaterialSprite(MAT_DIRT) == SPRITE_dirt);
        expect(MaterialSprite(MAT_CLAY) == SPRITE_clay);
        expect(MaterialSprite(MAT_GRAVEL) == SPRITE_gravel);
        expect(MaterialSprite(MAT_SAND) == SPRITE_sand);
        expect(MaterialSprite(MAT_PEAT) == SPRITE_peat);
        expect(MaterialSprite(MAT_GRANITE) == SPRITE_granite);
        expect(MaterialSprite(MAT_BEDROCK) == SPRITE_bedrock);
    }

    it("should return correct sprites for wood materials") {
        expect(MaterialSprite(MAT_OAK) == SPRITE_tree_trunk_oak);
        expect(MaterialSprite(MAT_PINE) == SPRITE_tree_trunk_pine);
        expect(MaterialSprite(MAT_BIRCH) == SPRITE_tree_trunk_birch);
        expect(MaterialSprite(MAT_WILLOW) == SPRITE_tree_trunk_willow);
    }

    it("should return 0 for materials without natural form") {
        expect(MaterialSprite(MAT_NONE) == 0);
        expect(MaterialSprite(MAT_BRICK) == 0);
        expect(MaterialSprite(MAT_IRON) == 0);
        expect(MaterialSprite(MAT_GLASS) == 0);
    }

    it("should resolve leaves sprites via sprite overrides") {
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_OAK) == SPRITE_tree_leaves_oak);
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_PINE) == SPRITE_tree_leaves_pine);
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_BIRCH) == SPRITE_tree_leaves_birch);
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_WILLOW) == SPRITE_tree_leaves_willow);
    }

    it("should resolve sapling sprites via sprite overrides") {
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_OAK) == SPRITE_tree_sapling_oak);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_PINE) == SPRITE_tree_sapling_pine);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_BIRCH) == SPRITE_tree_sapling_birch);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_WILLOW) == SPRITE_tree_sapling_willow);
    }

    it("should use material canonical for non-wood leaves, cell default for MAT_NONE") {
        // Granite has a canonical sprite, so leaves+granite = SPRITE_granite (step 2)
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_GRANITE) == SPRITE_granite);
        // MAT_NONE has no sprite, falls to cell default (step 3)
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_NONE) == SPRITE_tree_leaves_oak);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_NONE) == SPRITE_tree_sapling_oak);
    }
}

describe(material_insulation_tiers) {
    it("should return AIR tier for soil materials") {
        expect(MaterialInsulationTier(MAT_DIRT) == INSULATION_TIER_AIR);
        expect(MaterialInsulationTier(MAT_CLAY) == INSULATION_TIER_AIR);
        expect(MaterialInsulationTier(MAT_GRAVEL) == INSULATION_TIER_AIR);
        expect(MaterialInsulationTier(MAT_SAND) == INSULATION_TIER_AIR);
        expect(MaterialInsulationTier(MAT_PEAT) == INSULATION_TIER_AIR);
    }

    it("should return STONE tier for stone/metal/bedrock") {
        expect(MaterialInsulationTier(MAT_GRANITE) == INSULATION_TIER_STONE);
        expect(MaterialInsulationTier(MAT_BEDROCK) == INSULATION_TIER_STONE);
        expect(MaterialInsulationTier(MAT_IRON) == INSULATION_TIER_STONE);
        expect(MaterialInsulationTier(MAT_BRICK) == INSULATION_TIER_STONE);
    }

    it("should return WOOD tier for wood materials") {
        expect(MaterialInsulationTier(MAT_OAK) == INSULATION_TIER_WOOD);
        expect(MaterialInsulationTier(MAT_PINE) == INSULATION_TIER_WOOD);
        expect(MaterialInsulationTier(MAT_BIRCH) == INSULATION_TIER_WOOD);
        expect(MaterialInsulationTier(MAT_WILLOW) == INSULATION_TIER_WOOD);
    }
}

describe(material_burns_into) {
    it("should return MAT_DIRT for peat") {
        expect(MaterialBurnsIntoMat(MAT_PEAT) == MAT_DIRT);
    }

    it("should return self for non-combustible terrain materials") {
        expect(MaterialBurnsIntoMat(MAT_GRANITE) == MAT_GRANITE);
        expect(MaterialBurnsIntoMat(MAT_DIRT) == MAT_DIRT);
        expect(MaterialBurnsIntoMat(MAT_CLAY) == MAT_CLAY);
        expect(MaterialBurnsIntoMat(MAT_SAND) == MAT_SAND);
    }

    it("should return MAT_NONE for wood (burns to nothing)") {
        expect(MaterialBurnsIntoMat(MAT_OAK) == MAT_NONE);
        expect(MaterialBurnsIntoMat(MAT_PINE) == MAT_NONE);
    }
}

describe(material_bedrock) {
    it("should have MF_UNMINEABLE flag") {
        expect(MaterialIsUnmineable(MAT_BEDROCK));
    }

    it("should not have MF_UNMINEABLE on regular materials") {
        expect(!MaterialIsUnmineable(MAT_GRANITE));
        expect(!MaterialIsUnmineable(MAT_OAK));
        expect(!MaterialIsUnmineable(MAT_NONE));
    }

    it("should have correct terrain sprite") {
        expect(MaterialSprite(MAT_BEDROCK) == SPRITE_bedrock);
    }

    it("should have STONE insulation tier") {
        expect(MaterialInsulationTier(MAT_BEDROCK) == INSULATION_TIER_STONE);
    }

    it("should drop nothing") {
        expect(MaterialDropsItem(MAT_BEDROCK) == ITEM_NONE);
    }
}

describe(material_dirt_fuel_fix) {
    it("should have fuel=1 for dirt") {
        expect(MaterialFuel(MAT_DIRT) == 1);
    }

    it("should have fuel=6 for peat") {
        expect(MaterialFuel(MAT_PEAT) == 6);
    }
}

describe(get_cell_sprite_at) {
    it("should return material sprite for terrain cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_DIRT);
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_dirt);

        grid[0][0][1] = CELL_WALL;
        SetWallMaterial(1, 0, 0, MAT_GRANITE);
        expect(GetCellSpriteAt(1, 0, 0) == SPRITE_granite);

        grid[0][0][2] = CELL_WALL;
        SetWallMaterial(2, 0, 0, MAT_PEAT);
        expect(GetCellSpriteAt(2, 0, 0) == SPRITE_peat);
    }

    it("should return rock sprite for natural walls") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);

        // SyncMaterialsToTerrain sets MAT_GRANITE + natural
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_granite);
    }

    it("should return tree sprites via wallMaterial") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_TREE_TRUNK;
        SetWallMaterial(0, 0, 0, MAT_PINE);
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_tree_trunk_pine);

        grid[0][0][1] = CELL_TREE_LEAVES;
        SetWallMaterial(1, 0, 0, MAT_BIRCH);
        expect(GetCellSpriteAt(1, 0, 0) == SPRITE_tree_leaves_birch);

        grid[0][0][2] = CELL_SAPLING;
        SetWallMaterial(2, 0, 0, MAT_WILLOW);
        expect(GetCellSpriteAt(2, 0, 0) == SPRITE_tree_sapling_willow);
    }

    it("should use wallMaterial for tree cell sprites") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_TREE_TRUNK;
        SetWallMaterial(0, 0, 0, MAT_OAK);
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_tree_trunk_oak);
    }
}

describe(get_insulation_at) {
    it("should return material insulation when material is set") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_OAK);
        expect(GetInsulationAt(0, 0, 0) == INSULATION_TIER_WOOD);

        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        expect(GetInsulationAt(0, 0, 0) == INSULATION_TIER_STONE);
    }

    it("should fall back to cell insulation when no material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        // Air cell with no material
        expect(GetInsulationAt(0, 0, 0) == INSULATION_TIER_AIR);
    }

    it("should return STONE for out-of-bounds") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        expect(GetInsulationAt(-1, 0, 0) == INSULATION_TIER_STONE);
        expect(GetInsulationAt(100, 0, 0) == INSULATION_TIER_STONE);
    }
}

describe(get_cell_name_at) {
    it("should return material name for walls") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);

        SetWallMaterial(0, 0, 0, MAT_OAK);
        expect(strcmp(GetCellNameAt(0, 0, 0), "Oak") == 0);

        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        expect(strcmp(GetCellNameAt(0, 0, 0), "Granite") == 0);
    }

    it("should return combined name for tree cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_TREE_TRUNK;
        SetWallMaterial(0, 0, 0, MAT_OAK);
        expect(strcmp(GetCellNameAt(0, 0, 0), "Oak tree trunk") == 0);

        grid[0][0][1] = CELL_TREE_LEAVES;
        SetWallMaterial(1, 0, 0, MAT_PINE);
        expect(strcmp(GetCellNameAt(1, 0, 0), "Pine tree leaves") == 0);
    }

    it("should return material name for terrain cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_DIRT);
        expect(strcmp(GetCellNameAt(0, 0, 0), "Dirt") == 0);

        expect(strcmp(GetCellNameAt(1, 0, 0), "air") == 0);
    }
}

// =============================================================================
// Phase 1: CELL_WALL coexists with old types
// =============================================================================

describe(sprite_overrides) {
    it("should return override for wood walls") {
        expect(GetSpriteForCellMat(CELL_WALL, MAT_OAK) == SPRITE_wall_wood);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_PINE) == SPRITE_wall_wood);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_BIRCH) == SPRITE_wall_wood);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_WILLOW) == SPRITE_wall_wood);
    }

    it("should return override for leaves") {
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_OAK) == SPRITE_tree_leaves_oak);
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_PINE) == SPRITE_tree_leaves_pine);
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_BIRCH) == SPRITE_tree_leaves_birch);
        expect(GetSpriteForCellMat(CELL_TREE_LEAVES, MAT_WILLOW) == SPRITE_tree_leaves_willow);
    }

    it("should return override for saplings") {
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_OAK) == SPRITE_tree_sapling_oak);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_PINE) == SPRITE_tree_sapling_pine);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_BIRCH) == SPRITE_tree_sapling_birch);
        expect(GetSpriteForCellMat(CELL_SAPLING, MAT_WILLOW) == SPRITE_tree_sapling_willow);
    }

    it("should use material canonical sprite when no override") {
        expect(GetSpriteForCellMat(CELL_WALL, MAT_DIRT) == SPRITE_dirt);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_GRANITE) == SPRITE_granite);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_CLAY) == SPRITE_clay);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_GRANITE) == SPRITE_granite);
        expect(GetSpriteForCellMat(CELL_TREE_TRUNK, MAT_OAK) == SPRITE_tree_trunk_oak);
        expect(GetSpriteForCellMat(CELL_TREE_TRUNK, MAT_PINE) == SPRITE_tree_trunk_pine);
    }

    it("should fall back to cell default when no material") {
        expect(GetSpriteForCellMat(CELL_AIR, MAT_NONE) == SPRITE_air);
        expect(GetSpriteForCellMat(CELL_LADDER_UP, MAT_NONE) == SPRITE_ladder_up);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_NONE) == SPRITE_wall);
    }

    it("should fall back to cell default when material has no sprite") {
        expect(GetSpriteForCellMat(CELL_WALL, MAT_BRICK) == SPRITE_wall);
        expect(GetSpriteForCellMat(CELL_WALL, MAT_IRON) == SPRITE_wall);
    }
}

describe(wall_flags) {
    it("should be solid") {
        expect(CellIsSolid(CELL_WALL));
    }

    it("should block fluids") {
        expect(CellBlocksFluids(CELL_WALL));
    }

    it("should block movement") {
        expect(CellBlocksMovement(CELL_WALL));
    }
}

describe(cell_terrain_with_materials) {
    it("should return material sprite from GetCellSpriteAt") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_DIRT);
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_dirt);

        grid[0][0][1] = CELL_WALL;
        SetWallMaterial(1, 0, 0, MAT_GRANITE);
        expect(GetCellSpriteAt(1, 0, 0) == SPRITE_granite);

        grid[0][0][2] = CELL_WALL;
        SetWallMaterial(2, 0, 0, MAT_PEAT);
        expect(GetCellSpriteAt(2, 0, 0) == SPRITE_peat);
    }

    it("should fall back to cellDef sprite when no material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        // No SetWallMaterial — MAT_NONE
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_wall);  // cellDef default
    }

    it("should return material name from GetCellNameAt") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_DIRT);
        expect(strcmp(GetCellNameAt(0, 0, 0), "Dirt") == 0);

        grid[0][0][1] = CELL_WALL;
        SetWallMaterial(1, 0, 0, MAT_GRANITE);
        expect(strcmp(GetCellNameAt(1, 0, 0), "Granite") == 0);
    }

    it("should fall back to cellDef name when no material") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        expect(strcmp(GetCellNameAt(0, 0, 0), "wall") == 0);
    }

    it("should return material insulation from GetInsulationAt") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_GRANITE);
        expect(GetInsulationAt(0, 0, 0) == INSULATION_TIER_STONE);

        grid[0][0][1] = CELL_WALL;
        SetWallMaterial(1, 0, 0, MAT_DIRT);
        expect(GetInsulationAt(1, 0, 0) == INSULATION_TIER_AIR);
    }

    it("should work with MAT_BEDROCK for unmineable terrain") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_WALL;
        SetWallMaterial(0, 0, 0, MAT_BEDROCK);
        expect(MaterialIsUnmineable(GetWallMaterial(0, 0, 0)));
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_bedrock);
        expect(GetInsulationAt(0, 0, 0) == INSULATION_TIER_STONE);
        expect(strcmp(GetCellNameAt(0, 0, 0), "Bedrock") == 0);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
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
    
    // Phase 0: Material property helpers
    test(material_sprites);
    test(material_insulation_tiers);
    test(material_burns_into);
    test(material_bedrock);
    test(material_dirt_fuel_fix);
    test(get_cell_sprite_at);
    test(get_insulation_at);
    test(get_cell_name_at);
    
    // Sprite overrides table
    test(sprite_overrides);
    
    // Phase 1: CELL_WALL coexists with old types
    test(wall_flags);
    test(cell_terrain_with_materials);
    
    return summary();
}
