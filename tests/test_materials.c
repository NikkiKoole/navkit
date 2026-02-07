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
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
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
        int itemIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_PINE);
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
        expect(GetWallDropItem(0, 0, 0) == ITEM_LOG);
        
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
        
        // Step 1: Create floor and blueprint
        SET_FLOOR(1, 0, 0);
        int bpIdx = CreateBuildBlueprint(1, 0, 0);
        
        // Step 2: Deliver wood and complete build
        int woodItem = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
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
        expect(CountItemsOfTypeWithMaterial(ITEM_LOG, MAT_OAK) == 1);
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
// Phase 0: Material Property Helpers
// =============================================================================

describe(material_terrain_sprites) {
    it("should return correct terrain sprites for soil materials") {
        expect(MaterialTerrainSprite(MAT_DIRT) == SPRITE_dirt);
        expect(MaterialTerrainSprite(MAT_CLAY) == SPRITE_clay);
        expect(MaterialTerrainSprite(MAT_GRAVEL) == SPRITE_gravel);
        expect(MaterialTerrainSprite(MAT_SAND) == SPRITE_sand);
        expect(MaterialTerrainSprite(MAT_PEAT) == SPRITE_peat);
        expect(MaterialTerrainSprite(MAT_GRANITE) == SPRITE_rock);
        expect(MaterialTerrainSprite(MAT_BEDROCK) == SPRITE_bedrock);
    }

    it("should return 0 for materials without terrain form") {
        expect(MaterialTerrainSprite(MAT_NONE) == 0);
        expect(MaterialTerrainSprite(MAT_OAK) == 0);
        expect(MaterialTerrainSprite(MAT_BRICK) == 0);
        expect(MaterialTerrainSprite(MAT_IRON) == 0);
    }

    it("should return correct tree sprites for wood materials") {
        expect(MaterialTreeTrunkSprite(MAT_OAK) == SPRITE_tree_trunk_oak);
        expect(MaterialTreeTrunkSprite(MAT_PINE) == SPRITE_tree_trunk_pine);
        expect(MaterialTreeTrunkSprite(MAT_BIRCH) == SPRITE_tree_trunk_birch);
        expect(MaterialTreeTrunkSprite(MAT_WILLOW) == SPRITE_tree_trunk_willow);

        expect(MaterialTreeLeavesSprite(MAT_OAK) == SPRITE_tree_leaves_oak);
        expect(MaterialTreeLeavesSprite(MAT_PINE) == SPRITE_tree_leaves_pine);
        expect(MaterialTreeLeavesSprite(MAT_BIRCH) == SPRITE_tree_leaves_birch);
        expect(MaterialTreeLeavesSprite(MAT_WILLOW) == SPRITE_tree_leaves_willow);

        expect(MaterialTreeSaplingSprite(MAT_OAK) == SPRITE_tree_sapling_oak);
        expect(MaterialTreeSaplingSprite(MAT_PINE) == SPRITE_tree_sapling_pine);
        expect(MaterialTreeSaplingSprite(MAT_BIRCH) == SPRITE_tree_sapling_birch);
        expect(MaterialTreeSaplingSprite(MAT_WILLOW) == SPRITE_tree_sapling_willow);
    }

    it("should return 0 tree sprites for non-wood materials") {
        expect(MaterialTreeTrunkSprite(MAT_GRANITE) == 0);
        expect(MaterialTreeLeavesSprite(MAT_DIRT) == 0);
        expect(MaterialTreeSaplingSprite(MAT_NONE) == 0);
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
        expect(MaterialTerrainSprite(MAT_BEDROCK) == SPRITE_bedrock);
    }

    it("should have STONE insulation tier") {
        expect(MaterialInsulationTier(MAT_BEDROCK) == INSULATION_TIER_STONE);
    }

    it("should drop nothing") {
        expect(MaterialDropsItem(MAT_BEDROCK) == ITEM_NONE);
    }
}

describe(material_dirt_fuel_fix) {
    it("should have fuel=1 matching CELL_DIRT cellDef") {
        expect(MaterialFuel(MAT_DIRT) == 1);
        expect(MaterialFuel(MAT_DIRT) == CellFuel(CELL_DIRT));
    }

    it("should have fuel=6 for peat matching CELL_PEAT cellDef") {
        expect(MaterialFuel(MAT_PEAT) == 6);
        expect(MaterialFuel(MAT_PEAT) == CellFuel(CELL_PEAT));
    }
}

describe(get_cell_sprite_at) {
    it("should return cellDefs sprite for ground cells (backward compat)") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_DIRT;
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_dirt);

        grid[0][0][1] = CELL_ROCK;
        expect(GetCellSpriteAt(1, 0, 0) == SPRITE_rock);

        grid[0][0][2] = CELL_PEAT;
        expect(GetCellSpriteAt(2, 0, 0) == SPRITE_peat);
    }

    it("should return rock sprite for natural walls") {
        InitGridFromAsciiWithChunkSize(
            "#...\n", 4, 1);

        // SyncMaterialsToTerrain sets MAT_GRANITE + natural
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_rock);
    }

    it("should return tree sprites via treeTypeGrid (backward compat)") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_TREE_TRUNK;
        treeTypeGrid[0][0][0] = TREE_TYPE_PINE;
        expect(GetCellSpriteAt(0, 0, 0) == SPRITE_tree_trunk_pine);

        grid[0][0][1] = CELL_TREE_LEAVES;
        treeTypeGrid[0][0][1] = TREE_TYPE_BIRCH;
        expect(GetCellSpriteAt(1, 0, 0) == SPRITE_tree_leaves_birch);

        grid[0][0][2] = CELL_SAPLING;
        treeTypeGrid[0][0][2] = TREE_TYPE_WILLOW;
        expect(GetCellSpriteAt(2, 0, 0) == SPRITE_tree_sapling_willow);
    }

    it("should prefer wallMaterial over treeTypeGrid for tree cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_TREE_TRUNK;
        treeTypeGrid[0][0][0] = TREE_TYPE_PINE;
        SetWallMaterial(0, 0, 0, MAT_OAK);
        // wallMaterial (oak) should override treeTypeGrid (pine)
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
        treeTypeGrid[0][0][0] = TREE_TYPE_OAK;
        expect(strcmp(GetCellNameAt(0, 0, 0), "Oak tree trunk") == 0);

        grid[0][0][1] = CELL_TREE_LEAVES;
        treeTypeGrid[0][0][1] = TREE_TYPE_PINE;
        expect(strcmp(GetCellNameAt(1, 0, 0), "Pine tree leaves") == 0);
    }

    it("should return cellDef name for other cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);

        grid[0][0][0] = CELL_DIRT;
        expect(strcmp(GetCellNameAt(0, 0, 0), "dirt") == 0);

        expect(strcmp(GetCellNameAt(1, 0, 0), "air") == 0);
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
    test(material_terrain_sprites);
    test(material_insulation_tiers);
    test(material_burns_into);
    test(material_bedrock);
    test(material_dirt_fuel_fix);
    test(get_cell_sprite_at);
    test(get_insulation_at);
    test(get_cell_name_at);
    
    return summary();
}
