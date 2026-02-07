#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/terrain.h"
#include "../src/game_state.h"
#include "../src/simulation/water.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/mover.h"
#include "../src/entities/workshops.h"
#include "../src/entities/jobs.h"
#include <string.h>
#include <stdlib.h>

static bool test_verbose = false;

// =============================================================================
// Finding 2: GenerateCaves border should be on same z-level as cave interior
// =============================================================================

describe(caves_border_zlevel) {
    it("should place border walls at z=0, same level as cave interior") {
        GenerateCaves();

        // Border cells should be rock terrain (CELL_TERRAIN with MAT_GRANITE) at z=0
        bool allBorderRock = true;
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][0][x] != CELL_TERRAIN || GetWallMaterial(x, 0, 0) != MAT_GRANITE) allBorderRock = false;
            if (grid[0][gridHeight - 1][x] != CELL_TERRAIN || GetWallMaterial(x, gridHeight - 1, 0) != MAT_GRANITE) allBorderRock = false;
        }
        for (int y = 1; y < gridHeight - 1; y++) {
            if (grid[0][y][0] != CELL_TERRAIN || GetWallMaterial(0, y, 0) != MAT_GRANITE) allBorderRock = false;
            if (grid[0][y][gridWidth - 1] != CELL_TERRAIN || GetWallMaterial(gridWidth - 1, y, 0) != MAT_GRANITE) allBorderRock = false;
        }

        expect(allBorderRock);
    }

    it("should not have a complete border ring floating at z=1") {
        GenerateCaves();

        // Count how many border cells at z=1 are rock terrain
        int z1BorderRock = 0;
        for (int x = 0; x < gridWidth; x++) {
            if (grid[1][0][x] == CELL_TERRAIN && GetWallMaterial(x, 0, 1) == MAT_GRANITE) z1BorderRock++;
            if (grid[1][gridHeight - 1][x] == CELL_TERRAIN && GetWallMaterial(x, gridHeight - 1, 1) == MAT_GRANITE) z1BorderRock++;
        }
        for (int y = 1; y < gridHeight - 1; y++) {
            if (grid[1][y][0] == CELL_TERRAIN && GetWallMaterial(0, y, 1) == MAT_GRANITE) z1BorderRock++;
            if (grid[1][y][gridWidth - 1] == CELL_TERRAIN && GetWallMaterial(gridWidth - 1, y, 1) == MAT_GRANITE) z1BorderRock++;
        }

        // The full border perimeter should NOT be rock at z=1
        int borderPerimeter = 2 * gridWidth + 2 * (gridHeight - 2);
        expect(z1BorderRock < borderPerimeter);
    }

    it("should have cave interior walkable cells at z=0") {
        GenerateCaves();

        int dirtCount = 0;
        for (int y = 1; y < gridHeight - 1; y++)
            for (int x = 1; x < gridWidth - 1; x++)
                if (grid[0][y][x] == CELL_TERRAIN && GetWallMaterial(x, y, 0) == MAT_DIRT) dirtCount++;

        // Cellular automata creates ~50% open, expect at least 25%
        int interiorCells = (gridWidth - 2) * (gridHeight - 2);
        expect(dirtCount > interiorCells / 4);
    }
}

// =============================================================================
// Finding 4: Terrain generators should clear entity state on regeneration
// =============================================================================

describe(terrain_regen_clears_entities) {
    it("should clear movers when terrain is regenerated") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_DIRT;

        ClearMovers();
        InitJobPool();
        InitJobSystem(MAX_MOVERS);

        Mover* m = &movers[0];
        Point goal = {1, 1, 1};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 1.0f, goal, 100.0f);
        moverCount = 1;

        expect(moverCount == 1);

        GenerateCaves();

        expect(moverCount == 0);
    }

    it("should clear items when terrain is regenerated") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_DIRT;

        ClearItems();
        SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 3.5f, 1.0f, ITEM_LOG);
        SpawnItem(CELL_SIZE * 5.5f, CELL_SIZE * 5.5f, 1.0f, ITEM_ROCK);

        expect(itemCount == 2);

        GenerateCaves();

        expect(itemCount == 0);
    }

    it("should clear workshops when terrain is regenerated") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_DIRT;

        ClearWorkshops();
        CreateWorkshop(3, 3, 1, WORKSHOP_STONECUTTER);

        expect(workshopCount == 1);

        GenerateCaves();

        expect(workshopCount == 0);
    }

    it("should clear stockpiles when terrain is regenerated") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_DIRT;

        ClearStockpiles();
        CreateStockpile(2, 2, 1, 3, 3);

        expect(stockpileCount == 1);

        GenerateCaves();

        expect(stockpileCount == 0);
    }
}

// =============================================================================
// Finding 5: Water state should be cleared when switching terrain types
// =============================================================================

describe(terrain_regen_clears_water) {
    it("should clear water when switching from water terrain to non-water terrain") {
        // First generate a water terrain
        GenerateHillsSoilsWater();

        // Place some extra water to guarantee it exists
        SetWaterLevel(5, 5, 0, 7);
        SetWaterLevel(6, 5, 0, 7);
        SetWaterSource(5, 5, 0, true);

        // Verify water exists
        int waterBefore = 0;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    waterBefore += GetWaterLevel(x, y, z);
        expect(waterBefore > 0);

        // Now generate a non-water terrain
        GenerateHills();

        // Water should be cleared
        int waterAfter = 0;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    waterAfter += GetWaterLevel(x, y, z);

        expect(waterAfter == 0);
    }

    it("should clear water sources when switching terrain types") {
        // Use a simple flat grid so we can control water source placement
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        InitWater();
        SetWaterSource(5, 2, 0, true);
        SetWaterLevel(5, 2, 0, 7);
        expect(IsWaterSourceAt(5, 2, 0) == true);

        // Switch to a different terrain
        GenerateCaves();

        // Water sources should be cleared
        int sourceCount = 0;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    if (IsWaterSourceAt(x, y, z)) sourceCount++;

        expect(sourceCount == 0);
    }
}

// =============================================================================
// Finding 3: North ramp should not overwrite existing ramps
//
// The ramp placement scan goes y=0..H, x=0..W. When processing cell (x,y),
// it checks each neighbor. East/South/West check !CellIsRamp() before placing,
// but North does not. This means a North ramp can overwrite a previously placed
// East/South/West ramp.
//
// We test by running GenerateHills with rampDensity=1.0 across multiple seeds
// and counting CELL_RAMP_N cells where the west neighbor was also 1 level lower
// (meaning East would have placed a ramp first, but North overwrote it).
// =============================================================================

describe(ramp_north_duplicate_prevention) {
    it("should not have north ramps at cells where east also qualified") {
        float origDensity = rampDensity;
        rampDensity = 1.0f;
        uint64_t origSeed = worldSeed;

        int totalOverwrites = 0;

        for (int seed = 0; seed < 5; seed++) {
            worldSeed = (uint64_t)(seed * 1000 + 42);
            GenerateHills();

            // For each CELL_RAMP_N at (x,y,z): check if the cell to the west
            // (x-1,y) has a solid top at z-1 and air at z. That means when
            // processing (x-1,y), the East check saw (x,y) as 1 higher and
            // would have placed CELL_RAMP_E here first. If we see CELL_RAMP_N
            // instead, North overwrote East.
            for (int z = 1; z < gridDepth; z++) {
                for (int y = 1; y < gridHeight; y++) {
                    for (int x = 1; x < gridWidth; x++) {
                        if (grid[z][y][x] != CELL_RAMP_N) continue;

                        // Check if west neighbor (x-1,y) is 1 level lower
                        // (solid at z-1, not solid at z)
                        if (CellIsSolid(grid[z-1][y][x-1]) &&
                            !CellIsSolid(grid[z][y][x-1]) &&
                            !CellIsRamp(grid[z][y][x-1])) {
                            // East direction from (x-1,y) would have qualified
                            // for a ramp at (x,y,z) — processed before North
                            totalOverwrites++;
                        }
                    }
                }
            }
        }

        if (test_verbose) {
            printf("North ramp overwrites detected: %d\n", totalOverwrites);
        }

        worldSeed = origSeed;
        rampDensity = origDensity;

        // If the bug is fixed, no North ramps should exist where East also qualified
        expect(totalOverwrites == 0);
    }
}

// =============================================================================
// Finding 1: Connectivity fix should not destroy the largest component
// =============================================================================

describe(connectivity_fix_preserves_largest) {
    it("should preserve the largest walkable component after connectivity fix") {
        bool origReport = hillsWaterConnectivityReport;
        bool origFix = hillsWaterConnectivityFixSmall;
        int origThreshold = hillsWaterConnectivitySmallThreshold;

        hillsWaterConnectivityReport = false;
        hillsWaterConnectivityFixSmall = true;
        hillsWaterConnectivitySmallThreshold = 50;

        GenerateHillsSoilsWater();

        int walkableAfter = 0;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    if (IsCellWalkableAt(z, y, x)) walkableAfter++;

        // Largest component should survive with substantial walkable area
        int totalCells = gridWidth * gridHeight;
        expect(walkableAfter > totalCells / 4);

        if (test_verbose) {
            printf("Walkable cells after connectivity fix: %d / %d\n",
                   walkableAfter, totalCells * gridDepth);
        }

        hillsWaterConnectivityReport = origReport;
        hillsWaterConnectivityFixSmall = origFix;
        hillsWaterConnectivitySmallThreshold = origThreshold;
    }

    it("should still have walkable terrain across multiple seeds") {
        bool origReport = hillsWaterConnectivityReport;
        bool origFix = hillsWaterConnectivityFixSmall;
        int origThreshold = hillsWaterConnectivitySmallThreshold;
        uint64_t origSeed = worldSeed;

        hillsWaterConnectivityReport = false;
        hillsWaterConnectivityFixSmall = true;
        hillsWaterConnectivitySmallThreshold = 50;

        bool allHaveWalkable = true;
        for (int seed = 0; seed < 5; seed++) {
            worldSeed = (uint64_t)(seed * 7777 + 1);
            GenerateHillsSoilsWater();

            int walkable = 0;
            for (int z = 0; z < gridDepth; z++)
                for (int y = 0; y < gridHeight; y++)
                    for (int x = 0; x < gridWidth; x++)
                        if (IsCellWalkableAt(z, y, x)) walkable++;

            if (walkable == 0) allHaveWalkable = false;
        }

        expect(allHaveWalkable);

        worldSeed = origSeed;
        hillsWaterConnectivityReport = origReport;
        hillsWaterConnectivityFixSmall = origFix;
        hillsWaterConnectivitySmallThreshold = origThreshold;
    }
}

// =============================================================================
// Main
// =============================================================================

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

    test(caves_border_zlevel);
    test(terrain_regen_clears_entities);
    test(terrain_regen_clears_water);
    test(ramp_north_duplicate_prevention);
    test(connectivity_fix_preserves_largest);

    return summary();
}
