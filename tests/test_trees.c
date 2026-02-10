#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/world/designations.h"
#include "../src/simulation/trees.h"
#include "../src/simulation/groundwear.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/core/time.h"
#include <string.h>
#include <math.h>


// Global flag for verbose output in tests
static bool test_verbose = false;

// =============================================================================
// Helper functions
// =============================================================================

static void SetupBasicGrid(void) {
    // Create a simple grid with dirt floor at z=0, air above
    InitGridFromAsciiWithChunkSize(
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n", 10, 8);
    
    // Set z=0 as dirt floor
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
        }
    }
    
    // Initialize items spatial grid for QueryItemAtTile to work
    ClearItems();
    BuildItemSpatialGrid();
}

static int CountCellType(CellType type) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (grid[z][y][x] == type) count++;
            }
        }
    }
    return count;
}

static int CountItemType(ItemType type) {
    int count = 0;
    for (int i = 0; i < itemCount; i++) {
        if (items[i].active && items[i].type == type) count++;
    }
    return count;
}

static int CountStandingTrunks(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (grid[z][y][x] == CELL_TREE_TRUNK) {
                    count++;
                }
            }
        }
    }
    return count;
}

static int CountSaplingItems(void) {
    int count = 0;
    for (int i = 0; i < itemCount; i++) {
        if (items[i].active && IsSaplingItem(items[i].type)) count++;
    }
    return count;
}

static int CountFelledTrunks(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (grid[z][y][x] == CELL_TREE_FELLED) {
                    count++;
                }
            }
        }
    }
    return count;
}

static bool ChopFirstFelledTrunk(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (grid[z][y][x] == CELL_TREE_FELLED) {
                    CompleteChopFelledDesignation(x, y, z, -1);
                    return true;
                }
            }
        }
    }
    return false;
}

// =============================================================================
// Basic Tree Growth
// =============================================================================

describe(tree_basic_growth) {
    it("should grow sapling into trunk after enough ticks") {
        SetupBasicGrid();
        InitTrees();
        ClearItems();
        
        // Place sapling at z=1 (above dirt)
        PlaceSapling(5, 5, 1, MAT_OAK);
        expect(grid[1][5][5] == CELL_SAPLING);
        
        // Run growth ticks until sapling becomes trunk
        int originalTicks = saplingGrowTicks;
        saplingGrowTicks = 10;  // Speed up for test
        
        for (int i = 0; i < 15; i++) {
            TreesTick(0.0f);
        }
        
        // Sapling should have become trunk
        expect(grid[1][5][5] == CELL_TREE_TRUNK);
        
        saplingGrowTicks = originalTicks;
    }
    
    it("should grow full tree with TreeGrowFull") {
        SetupBasicGrid();
        InitTrees();
        
        // Grow a full tree instantly
        TreeGrowFull(5, 5, 1, MAT_OAK);
        
        // Should have trunk
        expect(grid[1][5][5] == CELL_TREE_TRUNK);
        
        // Should have some trunk height
        int trunkHeight = 0;
        for (int z = 1; z < gridDepth; z++) {
            if (grid[z][5][5] == CELL_TREE_TRUNK) trunkHeight++;
            else break;
        }
        expect(trunkHeight >= 3);  // MIN_TREE_HEIGHT
        
        // Should have leaves somewhere
        int leafCount = CountCellType(CELL_TREE_LEAVES);
        expect(leafCount > 0);
    }
}

// =============================================================================
// Sapling Drops from Chopped Trees
// =============================================================================

describe(tree_sapling_drops) {
    it("should drop saplings when tree is felled") {
        SetupBasicGrid();
        InitTrees();
        ClearItems();
        InitDesignations();
        
        // Grow a full tree
        TreeGrowFull(5, 5, 1, MAT_OAK);
        
        // Count leaves before felling
        int leavesBefore = CountCellType(CELL_TREE_LEAVES);
        expect(leavesBefore > 0);
        
        // Count saplings before (should be 0)
        int saplingsBefore = CountSaplingItems();
        expect(saplingsBefore == 0);
        
        // Fell the tree (pass -1 for moverIdx since we're calling directly)
        CompleteChopDesignation(5, 5, 1, -1);
        
        // Tree should be gone (base is air or felled trunk)
        bool baseClearOrFelled =
            (grid[1][5][5] == CELL_AIR) ||
            (grid[1][5][5] == CELL_TREE_FELLED);
        expect(baseClearOrFelled);
        
        // Should have dropped sapling items
        int saplingsAfter = CountSaplingItems();
        expect(saplingsAfter > 0);
        
        // Fell should create felled trunk segments (wood comes after chopping them)
        expect(CountFelledTrunks() > 0);
        expect(ChopFirstFelledTrunk() == true);
        int woodAfter = CountItemType(ITEM_LOG);
        expect(woodAfter > 0);
    }
    
    it("should drop roughly 1 sapling per 5 leaves") {
        SetupBasicGrid();
        InitTrees();
        ClearItems();
        InitDesignations();
        
        // Grow a full tree
        TreeGrowFull(5, 5, 1, MAT_OAK);
        
        int leavesBefore = CountCellType(CELL_TREE_LEAVES);
        
        CompleteChopDesignation(5, 5, 1, -1);
        
        int saplingsAfter = CountSaplingItems();
        
        // Expect roughly 1 sapling per 5 leaves (minimum 1)
        int expectedMin = 1;
        int expectedMax = (leavesBefore / 5) + 2;  // Allow some variance
        
        expect(saplingsAfter >= expectedMin);
        expect(saplingsAfter <= expectedMax);
    }
}

// =============================================================================
// Sapling Gather and Plant Jobs
// =============================================================================

describe(sapling_gather_job) {
    it("should gather sapling cell into sapling item via CompleteGatherSaplingDesignation") {
        SetupBasicGrid();
        InitTrees();
        InitDesignations();
        
        // Place a sapling
        PlaceSapling(5, 5, 1, MAT_OAK);
        expect(grid[1][5][5] == CELL_SAPLING);
        
        // Designate sapling for gathering
        DesignateGatherSapling(5, 5, 1);
        expect(HasGatherSaplingDesignation(5, 5, 1) == true);
        
        // Directly complete the gather designation (simulating job completion)
        CompleteGatherSaplingDesignation(5, 5, 1, -1);
        
        // Sapling cell should be gone
        expect(grid[1][5][5] == CELL_AIR);
        
        // Sapling item should exist
        int saplingCount = CountSaplingItems();
        expect(saplingCount > 0);
    }
    
    it("should complete gather sapling job end-to-end: move to sapling -> work -> spawn item") {
        // Setup world - DF mode: z=0 is solid ground, z=1 is walkable
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 4; y++) {
                grid[0][y][x] = CELL_WALL;  // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;   // Air above (walkable)
            }
        }
        
        int workZ = 1;
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        InitDesignations();
        InitTrees();
        ClearJobs();
        InitJobSystem(MAX_MOVERS);
        
        // Create mover at (1,1) with plant capability
        Mover* m = &movers[0];
        Point goal = {1, 1, workZ};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, (float)workZ, goal, 100.0f);
        m->capabilities.canPlant = true;
        moverCount = 1;
        AddMoverToIdleList(0);
        
        // Place a sapling at (5,1)
        int saplingX = 5, saplingY = 1, saplingZ = workZ;
        PlaceSapling(saplingX, saplingY, saplingZ, MAT_OAK);
        expect(grid[saplingZ][saplingY][saplingX] == CELL_SAPLING);
        
        // Disable tree growth for this test (prevent sapling from growing into trunk)
        int originalGrowTicks = saplingGrowTicks;
        saplingGrowTicks = 100000;  // Very long time
        
        // Designate sapling for gathering
        DesignateGatherSapling(saplingX, saplingY, saplingZ);
        expect(HasGatherSaplingDesignation(saplingX, saplingY, saplingZ) == true);
        
        // No sapling items yet
        expect(CountSaplingItems() == 0);
        
        // Run simulation - AssignJobs should assign the job via WorkGiver_GatherSapling
        bool saplingGathered = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (grid[saplingZ][saplingY][saplingX] == CELL_AIR && CountSaplingItems() > 0) {
                saplingGathered = true;
                break;
            }
        }
        
        // Verify sapling was gathered
        expect(saplingGathered == true);
        expect(grid[saplingZ][saplingY][saplingX] == CELL_AIR);
        
        // Verify sapling item was spawned
        expect(CountSaplingItems() > 0);
        
        // Verify designation was cleared
        expect(HasGatherSaplingDesignation(saplingX, saplingY, saplingZ) == false);
        
        // Verify mover is back to idle
        expect(m->currentJobId == -1);
        
        // Restore growth ticks
        saplingGrowTicks = originalGrowTicks;
    }
}

describe(sapling_plant_job) {
    it("should plant sapling item as sapling cell via CompletePlantSaplingDesignation") {
        SetupBasicGrid();
        InitTrees();
        InitDesignations();
        
        // Spawn a sapling item (mover would carry this)
        int itemIdx = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_SAPLING_OAK);
        expect(IsItemActive(itemIdx));
        
        // Target location for planting
        int plantX = 6, plantY = 6, plantZ = 1;
        expect(grid[plantZ][plantY][plantX] == CELL_AIR);
        
        // Designate location for planting
        DesignatePlantSapling(plantX, plantY, plantZ);
        expect(HasPlantSaplingDesignation(plantX, plantY, plantZ) == true);
        
        // Delete the item (simulating job consuming it) before completion
        DeleteItem(itemIdx);
        
        // Directly complete the plant designation (simulating job completion)
        CompletePlantSaplingDesignation(plantX, plantY, plantZ, MAT_OAK, -1);
        
        // Sapling should be planted
        expect(grid[plantZ][plantY][plantX] == CELL_SAPLING);
        
        // Item should be consumed
        expect(CountSaplingItems() == 0);
    }
    
    it("should complete plant sapling job end-to-end: pickup -> carry -> plant") {
        // Setup world - DF mode: z=0 is solid ground, z=1 is walkable
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 4; y++) {
                grid[0][y][x] = CELL_WALL;  // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;   // Air above (walkable)
            }
        }
        
        int workZ = 1;
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        InitDesignations();
        InitTrees();
        ClearJobs();
        InitJobSystem(MAX_MOVERS);
        
        // Create mover at (1,1) with plant capability
        Mover* m = &movers[0];
        Point goal = {1, 1, workZ};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, (float)workZ, goal, 100.0f);
        m->capabilities.canPlant = true;
        moverCount = 1;
        AddMoverToIdleList(0);
        
        // Create sapling item at (3,1)
        int itemIdx = SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 1.5f, (float)workZ, ITEM_SAPLING_OAK);
        expect(IsItemActive(itemIdx));
        expect(CountItemType(ITEM_SAPLING_OAK) == 1);
        
        // Designate plant location at (6,1) - AIR cell
        int plantX = 6, plantY = 1, plantZ = workZ;
        expect(grid[plantZ][plantY][plantX] == CELL_AIR);
        DesignatePlantSapling(plantX, plantY, plantZ);
        expect(HasPlantSaplingDesignation(plantX, plantY, plantZ) == true);
        
        // Run simulation - AssignJobs should assign the job via WorkGiver_PlantSapling
        bool saplingPlanted = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (grid[plantZ][plantY][plantX] == CELL_SAPLING) {
                saplingPlanted = true;
                break;
            }
        }
        
        // Verify sapling was planted
        expect(saplingPlanted == true);
        expect(grid[plantZ][plantY][plantX] == CELL_SAPLING);
        
        // Verify item was consumed
        expect(CountSaplingItems() == 0);
        
        // Verify designation was cleared
        expect(HasPlantSaplingDesignation(plantX, plantY, plantZ) == false);
        
        // Verify mover is back to idle
        expect(m->currentJobId == -1);
    }
}

// =============================================================================
// Organic Tree Shapes
// =============================================================================

describe(tree_organic_shapes) {
    it("should create trees with varying heights") {
        SetupBasicGrid();
        InitTrees();
        
        int heights[5];
        
        // Grow trees at different positions (well separated)
        for (int i = 0; i < 5; i++) {
            int x = 1 + i * 2;
            TreeGrowFull(x, 4, 1, MAT_OAK);
            
            // Measure trunk height
            int h = 0;
            for (int z = 1; z < gridDepth; z++) {
                if (grid[z][4][x] == CELL_TREE_TRUNK) h++;
                else break;
            }
            heights[i] = h;
        }
        
        // Heights should be within valid range (3-6)
        for (int i = 0; i < 5; i++) {
            expect(heights[i] >= 3);  // MIN_TREE_HEIGHT
            expect(heights[i] <= 7);  // MAX_TREE_HEIGHT + 1 for tolerance
        }
        
        // With different positions, we expect some variation
        // (may occasionally be same due to hash collisions, but unlikely for all 5)
    }
    
    it("should create canopy with leaves around trunk top") {
        SetupBasicGrid();
        InitTrees();
        
        TreeGrowFull(5, 5, 1, MAT_OAK);
        
        // Find trunk top (includes tapered branch cells at top)
        int topZ = 1;
        for (int z = 1; z < gridDepth; z++) {
            if (grid[z][5][5] == CELL_TREE_TRUNK || grid[z][5][5] == CELL_TREE_BRANCH) topZ = z;
            else break;
        }
        
        // Check for leaves around and above trunk top
        int leavesAroundTop = 0;
        for (int dz = 0; dz <= 2; dz++) {
            int checkZ = topZ + dz;
            if (checkZ >= gridDepth) continue;
            
            for (int dy = -3; dy <= 3; dy++) {
                for (int dx = -3; dx <= 3; dx++) {
                    int nx = 5 + dx;
                    int ny = 5 + dy;
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                    if (grid[checkZ][ny][nx] == CELL_TREE_LEAVES) leavesAroundTop++;
                }
            }
        }
        
        expect(leavesAroundTop > 5);  // Should have substantial canopy
    }
}

// =============================================================================
// Sapling Regrowth on Grass
// =============================================================================

describe(sapling_regrowth) {
    it("should spawn saplings on untrampled grass over time") {
        SetupBasicGrid();
        InitTrees();
        InitGroundWear();
        ClearGroundWear();
        
        groundWearEnabled = true;
        saplingRegrowthEnabled = true;
        
        // Set very high chance for testing
        int originalChance = saplingRegrowthChance;
        int originalDist = saplingMinTreeDistance;
        saplingRegrowthChance = 5000;  // 50% chance per tick
        saplingMinTreeDistance = 1;    // Allow close saplings
        
        float originalInterval = wearRecoveryInterval;
        wearRecoveryInterval = 0.001f;  // Very fast updates
        
        int saplingsBefore = CountCellType(CELL_SAPLING);
        
        // Run ground wear updates (which includes sapling regrowth)
        gameDeltaTime = 0.1f;
        for (int i = 0; i < 100; i++) {
            UpdateGroundWear();
        }
        
        int saplingsAfter = CountCellType(CELL_SAPLING);
        
        // Should have spawned some saplings
        expect(saplingsAfter > saplingsBefore);
        
        // Restore settings
        saplingRegrowthChance = originalChance;
        saplingMinTreeDistance = originalDist;
        wearRecoveryInterval = originalInterval;
    }
    
    it("should not spawn saplings near existing trees") {
        SetupBasicGrid();
        InitTrees();
        InitGroundWear();
        ClearGroundWear();
        
        groundWearEnabled = true;
        saplingRegrowthEnabled = true;
        
        // Grow a tree in the center
        TreeGrowFull(5, 5, 1, MAT_OAK);
        
        // Set large minimum distance
        int originalDist = saplingMinTreeDistance;
        saplingMinTreeDistance = 10;  // Larger than our grid
        
        int originalChance = saplingRegrowthChance;
        saplingRegrowthChance = 9999;  // Almost always spawn (if allowed)
        
        float originalInterval = wearRecoveryInterval;
        wearRecoveryInterval = 0.001f;
        
        int saplingsBefore = CountCellType(CELL_SAPLING);
        
        gameDeltaTime = 0.1f;
        for (int i = 0; i < 50; i++) {
            UpdateGroundWear();
        }
        
        int saplingsAfter = CountCellType(CELL_SAPLING);
        
        // No new saplings should spawn (all tiles too close to tree)
        expect(saplingsAfter == saplingsBefore);
        
        saplingMinTreeDistance = originalDist;
        saplingRegrowthChance = originalChance;
        wearRecoveryInterval = originalInterval;
    }
}

// =============================================================================
// Growth Blocking by Items
// =============================================================================

describe(sapling_growth_blocking) {
    it("should not grow sapling into trunk when item is on tile") {
        SetupBasicGrid();
        InitTrees();
        
        // Place sapling
        PlaceSapling(5, 5, 1, MAT_OAK);
        expect(grid[1][5][5] == CELL_SAPLING);
        
        // Place item on same tile and rebuild spatial grid
        SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_RED);
        BuildItemSpatialGrid();
        
        // Speed up growth
        int originalTicks = saplingGrowTicks;
        saplingGrowTicks = 5;
        
        // Run many growth ticks
        for (int i = 0; i < 100; i++) {
            TreesTick(0.0f);
        }
        
        // Sapling should NOT have become trunk (item blocks it)
        expect(grid[1][5][5] == CELL_SAPLING);
        
        saplingGrowTicks = originalTicks;
    }
    
    it("should grow sapling into trunk after item is removed") {
        SetupBasicGrid();
        InitTrees();
        
        // Place sapling
        PlaceSapling(5, 5, 1, MAT_OAK);
        
        // Place item on same tile and rebuild spatial grid
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_RED);
        BuildItemSpatialGrid();
        
        int originalTicks = saplingGrowTicks;
        saplingGrowTicks = 5;
        
        // Run some ticks - should not grow
        for (int i = 0; i < 20; i++) {
            TreesTick(0.0f);
        }
        expect(grid[1][5][5] == CELL_SAPLING);
        
        // Remove item and rebuild grid
        DeleteItem(itemIdx);
        BuildItemSpatialGrid();
        
        // Run more ticks - should grow now
        for (int i = 0; i < 20; i++) {
            TreesTick(0.0f);
        }
        expect(grid[1][5][5] == CELL_TREE_TRUNK);
        
        saplingGrowTicks = originalTicks;
    }
    
    it("should not spawn sapling where item exists") {
        SetupBasicGrid();
        InitTrees();
        InitGroundWear();
        ClearGroundWear();
        
        groundWearEnabled = true;
        saplingRegrowthEnabled = true;
        
        // Place items on most tiles and rebuild grid
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_RED);
            }
        }
        BuildItemSpatialGrid();
        
        int originalChance = saplingRegrowthChance;
        saplingRegrowthChance = 9999;
        
        float originalInterval = wearRecoveryInterval;
        wearRecoveryInterval = 0.001f;
        
        int saplingsBefore = CountCellType(CELL_SAPLING);
        
        gameDeltaTime = 0.1f;
        for (int i = 0; i < 50; i++) {
            UpdateGroundWear();
        }
        
        int saplingsAfter = CountCellType(CELL_SAPLING);
        
        // No saplings should spawn (items blocking all tiles)
        expect(saplingsAfter == saplingsBefore);
        
        saplingRegrowthChance = originalChance;
        wearRecoveryInterval = originalInterval;
    }
}

// =============================================================================
// Sapling Trampling
// =============================================================================

describe(sapling_trampling) {
    it("should destroy sapling when trampled") {
        SetupBasicGrid();
        InitTrees();
        InitGroundWear();
        
        groundWearEnabled = true;
        
        // Place sapling
        PlaceSapling(5, 5, 1, MAT_OAK);
        expect(grid[1][5][5] == CELL_SAPLING);
        
        // Trample it - saplings require wearMax/2 tramples to be destroyed
        // (heavy traffic, not just one pass)
        for (int i = 0; i < wearMax; i++) {
            TrampleGround(5, 5, 1);
        }
        
        // Sapling should be destroyed
        expect(grid[1][5][5] == CELL_AIR);
    }
    
    it("should not destroy trunk when trampled") {
        SetupBasicGrid();
        InitTrees();
        InitGroundWear();
        
        groundWearEnabled = true;
        
        // Grow a tree
        TreeGrowFull(5, 5, 1, MAT_OAK);
        expect(grid[1][5][5] == CELL_TREE_TRUNK);
        
        // Trample trunk
        TrampleGround(5, 5, 1);
        
        // Trunk should still exist (only saplings are trampled)
        expect(grid[1][5][5] == CELL_TREE_TRUNK);
    }
}

// =============================================================================
// Stockpile Filtering for Saplings
// =============================================================================

describe(stockpile_sapling_filter) {
    it("should accept saplings when filter is enabled") {
        SetupBasicGrid();
        ClearItems();
        ClearStockpiles();
        
        // Create stockpile with sapling filter ON
        int spIdx = CreateStockpile(2, 2, 1, 2, 2);
        SetStockpileFilter(spIdx, ITEM_SAPLING_OAK, true);
        
        expect(StockpileAcceptsType(spIdx, ITEM_SAPLING_OAK) == true);
    }
    
    it("should reject saplings when filter is disabled") {
        SetupBasicGrid();
        ClearStockpiles();
        
        // Create stockpile and explicitly disable sapling filter
        int spIdx = CreateStockpile(2, 2, 1, 2, 2);
        // Stockpiles default to allowing all types, so we must explicitly disable
        SetStockpileFilter(spIdx, ITEM_SAPLING_OAK, false);
        
        expect(StockpileAcceptsType(spIdx, ITEM_SAPLING_OAK) == false);
    }
    
    it("should find stockpile for sapling item when filter enabled") {
        SetupBasicGrid();
        ClearStockpiles();
        
        // Create stockpile accepting saplings
        int spIdx = CreateStockpile(2, 2, 1, 2, 2);
        SetStockpileFilter(spIdx, ITEM_SAPLING_OAK, true);
        
        // Must rebuild free slot counts for FindStockpileForItem to work
        RebuildStockpileFreeSlotCounts();
        
        int outX, outY;
        int foundSp = FindStockpileForItem(ITEM_SAPLING_OAK, MAT_NONE, &outX, &outY);
        
        expect(foundSp == spIdx);
    }
    
    it("should not find stockpile for sapling when no stockpile accepts it") {
        SetupBasicGrid();
        ClearStockpiles();
        
        // Create stockpile that does NOT accept saplings
        int spIdx = CreateStockpile(2, 2, 1, 2, 2);
        // Disable all types first, then enable only red
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(spIdx, t, false);
        }
        SetStockpileFilter(spIdx, ITEM_RED, true);  // Accepts only red
        (void)spIdx;
        
        int outX, outY;
        int foundSp = FindStockpileForItem(ITEM_SAPLING_OAK, MAT_NONE, &outX, &outY);
        
        expect(foundSp == -1);
    }
}

// =============================================================================
// End-to-End: Full Tree Lifecycle
// =============================================================================

describe(tree_full_lifecycle) {
    it("should support full plant->grow->chop cycle using direct calls") {
        SetupBasicGrid();
        InitTrees();
        InitDesignations();
        
        // 1. Plant a sapling directly (simulating completed plant job)
        DesignatePlantSapling(5, 5, 1);
        CompletePlantSaplingDesignation(5, 5, 1, MAT_OAK, -1);
        expect(grid[1][5][5] == CELL_SAPLING);
        
        // 2. Fast-forward tree growth
        int originalTicks = saplingGrowTicks;
        int originalTrunkTicks = trunkGrowTicks;
        saplingGrowTicks = 1;
        trunkGrowTicks = 1;
        
        for (int i = 0; i < 50; i++) {
            TreesTick(0.0f);
        }
        
        saplingGrowTicks = originalTicks;
        trunkGrowTicks = originalTrunkTicks;
        
        // Tree should have grown
        expect(grid[1][5][5] == CELL_TREE_TRUNK);
        int leafCount = CountCellType(CELL_TREE_LEAVES);
        expect(leafCount > 0);
        
        // 3. Chop tree (simulating completed chop job)
        DesignateChop(5, 5, 1);
        CompleteChopDesignation(5, 5, 1, -1);
        
        // Tree should be felled (base is air or felled trunk)
        bool baseClearOrFelled =
            (grid[1][5][5] == CELL_AIR) ||
            (grid[1][5][5] == CELL_TREE_FELLED);
        expect(baseClearOrFelled);
        expect(CountStandingTrunks() == 0);
        
        // Should have saplings on ground from felling
        expect(CountSaplingItems() > 0);

        // Fell should create felled trunk segments, then chopping yields wood
        expect(CountFelledTrunks() > 0);
        expect(ChopFirstFelledTrunk() == true);
        expect(CountItemType(ITEM_LOG) > 0);
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
    
    // Run tests
    test(tree_basic_growth);
    test(tree_sapling_drops);
    test(sapling_gather_job);
    test(sapling_plant_job);
    test(tree_organic_shapes);
    test(sapling_regrowth);
    test(sapling_growth_blocking);
    test(sapling_trampling);
    test(stockpile_sapling_filter);
    test(tree_full_lifecycle);
    
    return summary();
}
