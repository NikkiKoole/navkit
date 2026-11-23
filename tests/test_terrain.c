#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/terrain.h"
#include "../src/world/pathfinding.h"
#include "../src/game_state.h"
#include "../src/simulation/water.h"
#include "../src/simulation/plants.h"
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

        // Border cells should be rock terrain (CELL_WALL with MAT_GRANITE) at z=0
        bool allBorderRock = true;
        for (int x = 0; x < gridWidth; x++) {
            if (GRID(x, 0, 0) != CELL_WALL || GetWallMaterial(x, 0, 0) != MAT_GRANITE) allBorderRock = false;
            if (GRID(x, gridHeight - 1, 0) != CELL_WALL || GetWallMaterial(x, gridHeight - 1, 0) != MAT_GRANITE) allBorderRock = false;
        }
        for (int y = 1; y < gridHeight - 1; y++) {
            if (GRID(0, y, 0) != CELL_WALL || GetWallMaterial(0, y, 0) != MAT_GRANITE) allBorderRock = false;
            if (GRID(gridWidth - 1, y, 0) != CELL_WALL || GetWallMaterial(gridWidth - 1, y, 0) != MAT_GRANITE) allBorderRock = false;
        }

        expect(allBorderRock);
    }

    it("should not have a complete border ring floating at z=1") {
        GenerateCaves();

        // Count how many border cells at z=1 are rock terrain
        int z1BorderRock = 0;
        for (int x = 0; x < gridWidth; x++) {
            if (GRID(x, 0, 1) == CELL_WALL && GetWallMaterial(x, 0, 1) == MAT_GRANITE) z1BorderRock++;
            if (GRID(x, gridHeight - 1, 1) == CELL_WALL && GetWallMaterial(x, gridHeight - 1, 1) == MAT_GRANITE) z1BorderRock++;
        }
        for (int y = 1; y < gridHeight - 1; y++) {
            if (GRID(0, y, 1) == CELL_WALL && GetWallMaterial(0, y, 1) == MAT_GRANITE) z1BorderRock++;
            if (GRID(gridWidth - 1, y, 1) == CELL_WALL && GetWallMaterial(gridWidth - 1, y, 1) == MAT_GRANITE) z1BorderRock++;
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
                if (GRID(x, y, 0) == CELL_WALL && GetWallMaterial(x, y, 0) == MAT_DIRT) dirtCount++;

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
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, 0) = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }

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
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, 0) = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }

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
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, 0) = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }

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
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, 0) = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }

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
                        if (GRID(x, y, z) != CELL_RAMP_N) continue;

                        // Check if west neighbor (x-1,y) is 1 level lower
                        // (solid at z-1, not solid at z)
                        if (CellIsSolid(GRID(x-1, y, z-1)) &&
                            !CellIsSolid(GRID(x-1, y, z)) &&
                            !CellIsRamp(GRID(x-1, y, z))) {
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

        uint64_t origSeed = worldSeed;

        hillsWaterConnectivityReport = false;
        hillsWaterConnectivityFixSmall = true;
        hillsWaterConnectivitySmallThreshold = 50;

        // its own map: it used to inherit an 8x8 grid from an earlier test, where rivers
        // to the map edge left 20 of 640 cells walkable in some runs
        gridDepth = 16;                              // before init: it sizes the levels
        InitGridWithSizeAndChunkSize(64, 64, 16, 16);
        worldSeed = 1;
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

        worldSeed = origSeed;
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

        gridDepth = 16;
        InitGridWithSizeAndChunkSize(64, 64, 16, 16);
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
// Same seed, same map: generation must not depend on leftover memory
// =============================================================================

static unsigned HillsChecksum(void) {
    unsigned h = 2166136261u;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                h = (h ^ (unsigned)GRID(x, y, z)) * 16777619u;
                h = (h ^ (unsigned)GetWaterLevel(x, y, z)) * 16777619u;
                h = (h ^ (unsigned)GetWallMaterial(x, y, z)) * 16777619u;
            }
    return h;
}

// Fill freed heap memory with a pattern, so a buffer the generator reads before writing
// comes back different from the first run
static void ScribbleHeap(unsigned char pattern) {
    for (int i = 0; i < 48; i++) {
        unsigned char* p = malloc((size_t)1 << 20);
        if (!p) break;
        memset(p, pattern ^ i, (size_t)1 << 20);
        free(p);
    }
}

describe(hills_water_deterministic) {
    // The same seed must give the same map. 035ff9f8 (rivers to the map edge) made one binary
    // give different maps run to run on a tiny grid. Generated twice with the heap filled with
    // a pattern in between, so a buffer read before it is written shows up as a difference.
    // Plants, items and water are cleared first, as the game's generate paths do: generators
    // place trees and items, and leftovers from the previous map change how many random
    // numbers get drawn (seed 4 after seed 3 came out different from seed 4 after seed 4).
    it("should give the same map for the same seed, whatever was in memory before") {
        uint64_t origSeed = worldSeed;
        bool origReport = hillsWaterConnectivityReport;
        hillsWaterConnectivityReport = false;
        int sizes[2] = {32, 128}, diffs = 0;
        for (int k = 0; k < 2; k++) {
            gridDepth = 16;
            InitGridWithSizeAndChunkSize(sizes[k], sizes[k], 16, 16);
            for (uint64_t seed = 1; seed <= 6; seed++) {
                unsigned sum[2];
                for (int r = 0; r < 2; r++) {
                    ScribbleHeap(r ? 0xA5 : 0x00);
                    ClearItems();
                    InitPlants();
                    ClearWater();
                    worldSeed = seed;
                    GenerateHillsSoilsWater();
                    sum[r] = HillsChecksum();
                }
                if (sum[0] != sum[1]) {
                    diffs++;
                    if (test_verbose) printf("hills-water %d seed %d: maps differ\n", sizes[k], (int)seed);
                }
            }
        }
        expect(diffs == 0);
        worldSeed = origSeed;
        hillsWaterConnectivityReport = origReport;
    }
}

// =============================================================================
// Caves are carved after trees/bushes: they must not remove the ground under them
// =============================================================================

describe(caves_keep_surface_support) {
    it("should leave no tree or bush floating over a cave") {
        uint64_t origSeed = worldSeed;
        bool origReport = hillsWaterConnectivityReport;
        hillsWaterConnectivityReport = false;
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        gridDepth = 16;
        int floating = 0, trees = 0;
        // seeds 2 and 6 had floating trees and bushes before caves kept a roof
        uint64_t seeds[] = {2, 6, 7, 8};
        for (int k = 0; k < 4; k++) {
            worldSeed = seeds[k];
            GenerateHillsSoilsWater();
            for (int z = 1; z < gridDepth; z++)
                for (int y = 0; y < gridHeight; y++)
                    for (int x = 0; x < gridWidth; x++) {
                        CellType c = GRID(x, y, z), below = GRID(x, y, z - 1);
                        bool base = (c == CELL_TREE_TRUNK && below != CELL_TREE_TRUNK) ||
                                    c == CELL_BUSH || c == CELL_SAPLING;
                        if (!base) continue;
                        if (c == CELL_TREE_TRUNK) trees++;
                        if (!CellIsSolid(below)) floating++;
                    }
        }
        expect(trees > 100);                         // the check saw real forests
        expect(floating == 0);
        worldSeed = origSeed;
        hillsWaterConnectivityReport = origReport;
    }
}

// =============================================================================
// Island generator (src/world/island_gen.c)
// =============================================================================

// z of the ground in a column: the highest wall or ramp (not a tree on it)
static int IslandGroundTop(int x, int y) {
    for (int z = gridDepth - 1; z >= 0; z--) {
        CellType c = GRID(x, y, z);
        if (c == CELL_WALL || CellIsRamp(c)) return z;
    }
    return -1;
}

// Tree roots replace the ground cell or lie on top of it, so a column with a tree in it has
// no reliable ground height
static bool IslandColumnHasTree(int x, int y) {
    for (int z = 0; z < gridDepth; z++) {
        CellType c = GRID(x, y, z);
        if (c >= CELL_SAPLING && c <= CELL_TREE_LEAVES) return true;
    }
    return false;
}

static unsigned IslandChecksum(void) {
    unsigned h = 2166136261u;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                h = (h ^ (unsigned)GRID(x, y, z)) * 16777619u;
                h = (h ^ (unsigned)GetWaterLevel(x, y, z)) * 16777619u;
            }
    return h;
}

// Every cell reachable from a (reverse: every cell that can reach a), by the pathfinder's own
// single steps; mark[] gets 1 for those. q: scratch, one int per cell.
static void IslandSearch(NavPoint a, bool reverse, unsigned char* mark, int* q) {
    int n = gridWidth * gridHeight * gridDepth, head = 0, tail = 0;
    memset(mark, 0, (size_t)n);
    int start = (a.z * gridHeight + a.y) * gridWidth + a.x;
    mark[start] = 1;
    q[tail++] = start;
    NavPoint st[NAV_MAX_STEPS];
    while (head < tail) {
        int id = q[head++];
        NavPoint p = {id % gridWidth, (id / gridWidth) % gridHeight, id / (gridWidth * gridHeight)};
        int k = NavSteps(NavGame(), p, reverse, st, NAV_MAX_STEPS);
        for (int i = 0; i < k; i++) {
            int t = (st[i].z * gridHeight + st[i].y) * gridWidth + st[i].x;
            if (!mark[t]) { mark[t] = 1; q[tail++] = t; }
        }
    }
}

describe(island_generator) {
    it("should make an island: sea at the frame, land in the middle, rivers") {
        uint64_t origSeed = worldSeed;
        gridDepth = 16;                              // before init: it sizes the levels
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        for (uint64_t seed = 1; seed <= 4; seed++) {
            worldSeed = seed;
            GenerateIsland();
            int land = 0, river = 0, frameDry = 0;
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) {
                    int t = IslandGroundTop(x, y);
                    bool wet = t + 1 < gridDepth && GetWaterLevel(x, y, t + 1) > 0;
                    bool frame = x == 0 || y == 0 || x == gridWidth - 1 || y == gridHeight - 1;
                    if (frame && !wet) frameDry++;
                    if (!wet) land++;
                    else if (t + 1 > 3) river++;            // water above the sea surface
                }
            int pct = 100 * land / (gridWidth * gridHeight);
            if (test_verbose) printf("seed %d: land %d%% river/lake cells %d\n", (int)seed, pct, river);
            expect(frameDry == 0);
            expect(pct >= 15 && pct <= 65);
            expect(river > 20);
        }
        worldSeed = origSeed;
    }

    it("should never step dry ground more than one level between neighbours (ramps can climb it)") {
        uint64_t origSeed = worldSeed;
        islandSkipBoulders = true;   // a boulder is a one-cell wall on the ground, not a step
        gridDepth = 16;                              // before init: it sizes the levels
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        int cliffs = 0;
        for (uint64_t seed = 1; seed <= 4; seed++) {
            worldSeed = seed;
            GenerateIsland();
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x + 1 < gridWidth; x++) {
                    if (IslandColumnHasTree(x, y) || IslandColumnHasTree(x + 1, y)) continue;
                    int a = IslandGroundTop(x, y), b = IslandGroundTop(x + 1, y);
                    if (GetWaterLevel(x, y, a + 1) > 0 || GetWaterLevel(x + 1, y, b + 1) > 0) continue;
                    if (abs(a - b) > 1) {
                        if (test_verbose && cliffs < 12)
                            printf("cliff seed %d at %d,%d: z%d vs z%d\n", (int)seed, x, y, a, b);
                        cliffs++;
                    }
                }
        }
        expect(cliffs == 0);
        islandSkipBoulders = false;
        worldSeed = origSeed;
    }

    it("should give the same island for the same seed") {
        uint64_t origSeed = worldSeed;
        gridDepth = 16;                              // before init: it sizes the levels
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        worldSeed = 7;
        GenerateIsland();
        unsigned a = IslandChecksum();
        worldSeed = 8;
        GenerateIsland();
        unsigned b = IslandChecksum();
        worldSeed = 7;
        GenerateIsland();
        expect(IslandChecksum() == a);
        expect(a != b);
        worldSeed = origSeed;
    }

    it("should give every ramp two clear ends: ground at its level past the top, no tree on either end") {
        uint64_t origSeed = worldSeed;
        gridDepth = 16;
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        int ramps = 0, nowhere = 0, treeBlocked = 0;
        for (uint64_t seed = 1; seed <= 4; seed++) {
            worldSeed = seed;
            GenerateIsland();
            for (int z = 1; z + 1 < gridDepth; z++)
                for (int y = 1; y + 1 < gridHeight; y++)
                    for (int x = 1; x + 1 < gridWidth; x++) {
                        int d = (int)GRID(x, y, z) - CELL_RAMP_N;
                        if (d < 0 || d > 3) continue;
                        int dx = (d == 1) - (d == 3), dy = (d == 2) - (d == 0);   // toward the high side
                        ramps++;
                        if (IslandGroundTop(x + dx, y + dy) != z) nowhere++;    // the top end steps onto ground at z
                        CellType lo = GRID(x - dx, y - dy, z), hi = GRID(x + dx, y + dy, z + 1);
                        if ((lo >= CELL_SAPLING && lo <= CELL_TREE_LEAVES) || (hi >= CELL_SAPLING && hi <= CELL_TREE_LEAVES))
                            treeBlocked++;
                    }
        }
        if (test_verbose) printf("ramps %d, leading nowhere %d, tree on an end %d\n", ramps, nowhere, treeBlocked);
        expect(ramps > 500);
        expect(nowhere == 0);
        expect(treeBlocked == 0);
        worldSeed = origSeed;
    }

    it("should leave no tree or bush floating") {
        uint64_t origSeed = worldSeed;
        gridDepth = 16;                              // before init: it sizes the levels
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        worldSeed = 3;
        GenerateIsland();
        int floating = 0, trees = 0;
        for (int z = 1; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) {
                    CellType c = GRID(x, y, z), below = GRID(x, y, z - 1);
                    bool base = (c == CELL_TREE_TRUNK && below != CELL_TREE_TRUNK) || c == CELL_BUSH;
                    if (!base) continue;
                    if (c == CELL_TREE_TRUNK) trees++;
                    if (!CellIsSolid(below)) floating++;
                }
        expect(trees > 50);
        expect(floating == 0);
        worldSeed = origSeed;
    }

    // Leaving a ramp sideways is a one-way step, so ground reached only that way traps whoever
    // walks in (a settler stranded so re-ran hundreds of failing path searches). Pockets:
    // reachable from the main land (forward search) but no way back (backward search).
    it("should leave no one-way pockets: everything reachable can get back") {
        uint64_t origSeed = worldSeed;
        gridDepth = 16;
        InitGridWithSizeAndChunkSize(256, 256, 16, 16);   // the island game's size (128 islands are mostly cut off: docs/todo/world/island.md)
        int n = gridWidth * gridHeight * gridDepth;
        unsigned char* F = malloc((size_t)n);
        unsigned char* B = malloc((size_t)n);
        int* q = malloc(sizeof(int) * (size_t)n);
        int pockets = 0, mainLand = 0;
        for (uint64_t seed = 1; seed <= 3; seed++) {
            worldSeed = seed;
            GenerateIsland();
            // main land: of a few sampled cells, the one with the biggest there-and-back area
            NavPoint anchor = {-1, -1, -1};
            int best = 0;
            for (int k = 0; k < 40; k++) {
                Point c = GetRandomWalkableCell();       // dry land only: the shallow sea bed is walkable too
                if (c.x < 0 || GetWaterLevel(c.x, c.y, c.z) > 0) continue;
                IslandSearch((NavPoint){c.x, c.y, c.z}, false, F, q);
                IslandSearch((NavPoint){c.x, c.y, c.z}, true, B, q);
                int two = 0;
                for (int i = 0; i < n; i++) two += F[i] && B[i];
                if (two > best) { best = two; anchor = (NavPoint){c.x, c.y, c.z}; }
            }
            expect(anchor.x >= 0);
            if (anchor.x < 0) continue;
            IslandSearch(anchor, false, F, q);
            IslandSearch(anchor, true, B, q);
            for (int i = 0; i < n; i++) if (F[i] && !B[i]) pockets++;
            mainLand += best;
        }
        if (test_verbose) printf("main land %d cells, pocket cells %d (seeds 1-3)\n", mainLand, pockets);
        expect(mainLand > 3 * 15000);                // the search saw the island, not a corner of it
        expect(pockets == 0);
        free(F); free(B); free(q);
        worldSeed = origSeed;
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);

    test(caves_border_zlevel);
    test(terrain_regen_clears_entities);
    test(terrain_regen_clears_water);
    test(ramp_north_duplicate_prevention);
    test(connectivity_fix_preserves_largest);
    test(hills_water_deterministic);
    test(caves_keep_surface_support);
    test(island_generator);

    return summary();
}
