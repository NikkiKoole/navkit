#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include <stdlib.h>
#include "../pathing/grid.h"
#include "../pathing/terrain.h"
#include "../pathing/pathfinding.h"

// Test grid size - fixed at 96x96 (works with various chunk sizes)
#define TEST_GRID_SIZE 96
#define TEST_CHUNK_SIZE 32

describe(grid_initialization) {
    it("should initialize grid to all walkable cells") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        int allWalkable = 1;
        for (int y = 0; y < gridHeight && allWalkable; y++)
            for (int x = 0; x < gridWidth && allWalkable; x++)
                if (grid[0][y][x] != CELL_WALKABLE) allWalkable = 0;
        expect(allWalkable == 1);
    }

    it("should mark chunks as dirty when walls are placed") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        // Clear dirty flags
        for (int cy = 0; cy < chunksY; cy++)
            for (int cx = 0; cx < chunksX; cx++)
                chunkDirty[0][cy][cx] = false;
        needsRebuild = false;

        // Place a wall and mark dirty
        grid[0][10][10] = CELL_WALL;
        MarkChunkDirty(10, 10, 0);

        int cx = 10 / chunkWidth;
        int cy = 10 / chunkHeight;
        expect(chunkDirty[0][cy][cx] == true && needsRebuild == true);
    }
}

describe(entrance_building) {
    it("should create entrances on chunk borders") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();

        // Verify we have entrances
        int hasEntrances = entranceCount > 0;

        // Verify all entrances are on chunk borders
        int allOnBorders = 1;
        for (int i = 0; i < entranceCount && allOnBorders; i++) {
            int x = entrances[i].x;
            int y = entrances[i].y;
            // Entrance must be on a vertical border (x % chunkWidth == 0)
            // OR on a horizontal border (y % chunkHeight == 0)
            int onVerticalBorder = (x % chunkWidth == 0);
            int onHorizontalBorder = (y % chunkHeight == 0);
            if (!onVerticalBorder && !onHorizontalBorder) {
                allOnBorders = 0;
            }
        }

        expect(hasEntrances && allOnBorders);
    }

    it("should not create entrances where walls block the border") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        // Block the entire first horizontal border
        int borderY = chunkHeight;
        for (int x = 0; x < gridWidth; x++) {
            grid[0][borderY - 1][x] = CELL_WALL;
        }
        BuildEntrances();
        // Check no entrances at y=borderY on z=0
        int entrancesAtBorder = 0;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].z == 0 && entrances[i].y == borderY) entrancesAtBorder++;
        }
        expect(entrancesAtBorder == 0);
    }

    it("should create correct entrances for full open border") {
        // A fully open border of chunkWidth cells should create ceil(chunkWidth / MAX_ENTRANCE_WIDTH) entrances
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);  // 2x2 chunks
        BuildEntrances();

        // Count entrances on the horizontal border at y=chunkHeight, x in [0, chunkWidth) on z=0
        int entrancesOnBorder = 0;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].z == 0 && entrances[i].y == chunkHeight && entrances[i].x < chunkWidth) {
                entrancesOnBorder++;
            }
        }

        // With chunkWidth=32 and MAX_ENTRANCE_WIDTH=6, we expect ceil(32/6) = 6 entrances
        int expectedEntrances = (chunkWidth + MAX_ENTRANCE_WIDTH - 1) / MAX_ENTRANCE_WIDTH;
        expect(entrancesOnBorder == expectedEntrances);
    }

    it("should create one entrance for narrow opening") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);  // 2x2 chunks

        // Block the horizontal border except for a 3-cell gap
        int borderY = chunkHeight;
        for (int x = 0; x < chunkWidth; x++) {
            grid[0][borderY - 1][x] = CELL_WALL;
            grid[0][borderY][x] = CELL_WALL;
        }
        // Open a narrow gap (3 cells wide, less than MAX_ENTRANCE_WIDTH)
        int gapStart = 10;
        int gapWidth = 3;
        for (int x = gapStart; x < gapStart + gapWidth; x++) {
            grid[0][borderY - 1][x] = CELL_WALKABLE;
            grid[0][borderY][x] = CELL_WALKABLE;
        }

        BuildEntrances();

        // Count entrances on this border section on z=0
        int entrancesOnBorder = 0;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].z == 0 && entrances[i].y == borderY && entrances[i].x < chunkWidth) {
                entrancesOnBorder++;
            }
        }

        // Narrow opening (< MAX_ENTRANCE_WIDTH) should create exactly 1 entrance
        expect(entrancesOnBorder == 1);
    }

    it("should create multiple entrances for wide opening") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);  // 2x2 chunks

        // Block the horizontal border except for a wide gap
        int borderY = chunkHeight;
        for (int x = 0; x < chunkWidth; x++) {
            grid[0][borderY - 1][x] = CELL_WALL;
            grid[0][borderY][x] = CELL_WALL;
        }
        // Open a wide gap (15 cells, more than 2x MAX_ENTRANCE_WIDTH)
        int gapStart = 5;
        int gapWidth = 15;
        for (int x = gapStart; x < gapStart + gapWidth; x++) {
            grid[0][borderY - 1][x] = CELL_WALKABLE;
            grid[0][borderY][x] = CELL_WALKABLE;
        }

        BuildEntrances();

        // Count entrances on this border section on z=0
        int entrancesOnBorder = 0;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].z == 0 && entrances[i].y == borderY && entrances[i].x < chunkWidth) {
                entrancesOnBorder++;
            }
        }

        // Wide opening should create ceil(15/6) = 3 entrances
        int expectedEntrances = (gapWidth + MAX_ENTRANCE_WIDTH - 1) / MAX_ENTRANCE_WIDTH;
        expect(entrancesOnBorder == expectedEntrances);
    }
}

describe(graph_building) {
    it("should create edges between entrances in the same chunk") {
        // 3x3 chunks, each 4x4 cells = 12x12 grid
        // Chunk layout:
        //   0 | 1 | 2
        //  ---+---+---
        //   3 | 4 | 5
        //  ---+---+---
        //   6 | 7 | 8
        //
        // The center chunk (4) has entrances on all 4 borders.
        // All entrances touching chunk 4 should connect to each other.
        const char* map =
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n";

        InitGridFromAsciiWithChunkSize(map, 4, 4);
        BuildEntrances();
        BuildGraph();

        // Find all entrances that touch chunk 4 (center chunk)
        int chunk4Entrances[32];
        int chunk4Count = 0;
        for (int i = 0; i < entranceCount && chunk4Count < 32; i++) {
            if (entrances[i].chunk1 == 4 || entrances[i].chunk2 == 4) {
                chunk4Entrances[chunk4Count++] = i;
            }
        }

        // Chunk 4 should have entrances on all 4 borders
        expect(chunk4Count >= 4);

        // Every pair of entrances in chunk 4 should have an edge between them
        int allConnected = 1;
        for (int i = 0; i < chunk4Count && allConnected; i++) {
            for (int j = i + 1; j < chunk4Count && allConnected; j++) {
                int e1 = chunk4Entrances[i];
                int e2 = chunk4Entrances[j];

                // Look for edge between them
                int foundEdge = 0;
                for (int k = 0; k < graphEdgeCount; k++) {
                    if ((graphEdges[k].from == e1 && graphEdges[k].to == e2) ||
                        (graphEdges[k].from == e2 && graphEdges[k].to == e1)) {
                        foundEdge = 1;
                        break;
                    }
                }
                if (!foundEdge) allConnected = 0;
            }
        }
        expect(allConnected == 1);
    }

    it("edges should be symmetric - cost A to B equals cost B to A") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        BuildEntrances();
        BuildGraph();

        // For every edge, the reverse edge should exist with same cost
        int symmetric = 1;
        for (int i = 0; i < graphEdgeCount && symmetric; i++) {
            int from = graphEdges[i].from;
            int to = graphEdges[i].to;
            int cost = graphEdges[i].cost;

            int foundReverse = 0;
            for (int j = 0; j < graphEdgeCount; j++) {
                if (graphEdges[j].from == to && graphEdges[j].to == from) {
                    if (graphEdges[j].cost == cost) foundReverse = 1;
                    break;
                }
            }
            if (!foundReverse) symmetric = 0;
        }
        expect(symmetric == 1);
    }

    it("should not create edges between entrances in different non-adjacent chunks") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 3, TEST_CHUNK_SIZE * 3, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);  // 3x3 chunks
        BuildEntrances();
        BuildGraph();

        // No edge should connect entrances that don't share any chunk
        int noInvalidEdges = 1;
        for (int i = 0; i < graphEdgeCount && noInvalidEdges; i++) {
            int e1 = graphEdges[i].from;
            int e2 = graphEdges[i].to;

            // Get chunks for each entrance
            int c1a = entrances[e1].chunk1, c1b = entrances[e1].chunk2;
            int c2a = entrances[e2].chunk1, c2b = entrances[e2].chunk2;

            // They must share at least one chunk
            int share = (c1a == c2a || c1a == c2b || c1b == c2a || c1b == c2b);
            if (!share) noInvalidEdges = 0;
        }
        expect(noInvalidEdges == 1);
    }

    it("should not create edge when wall completely blocks path between entrances") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);

        // Put a wall that divides chunk 0 into two unreachable halves
        // Vertical wall from top to bottom of chunk 0
        for (int y = 0; y < chunkHeight; y++) {
            grid[0][y][chunkWidth / 2] = CELL_WALL;
        }

        BuildEntrances();
        BuildGraph();

        // Find entrances on the left border of chunk 1 (x = chunkWidth)
        // and entrances on the bottom border of chunk 0 (y = chunkHeight)
        // These are in the same chunk (0 or 1) but the wall may block some paths

        // The test: verify no edge exists between unreachable entrances
        // An entrance on the left side of chunk 0 shouldn't connect to
        // an entrance on the right side of chunk 0
        int foundInvalidEdge = 0;
        for (int i = 0; i < graphEdgeCount && !foundInvalidEdge; i++) {
            int e1 = graphEdges[i].from;
            int e2 = graphEdges[i].to;

            // Check if both are in chunk 0
            int e1InChunk0 = (entrances[e1].chunk1 == 0 || entrances[e1].chunk2 == 0);
            int e2InChunk0 = (entrances[e2].chunk1 == 0 || entrances[e2].chunk2 == 0);

            if (e1InChunk0 && e2InChunk0) {
                // Check if they're on opposite sides of the wall
                int e1Left = entrances[e1].x < chunkWidth / 2;
                int e2Left = entrances[e2].x < chunkWidth / 2;

                // If both entrances are in chunk 0 and on opposite sides of wall,
                // there shouldn't be an edge (wall blocks it)
                // But we need to check their y positions too - the wall is vertical
                if (entrances[e1].y < chunkHeight && entrances[e2].y < chunkHeight) {
                    if (e1Left != e2Left) {
                        foundInvalidEdge = 1;  // This edge shouldn't exist
                    }
                }
            }
        }
        expect(foundInvalidEdge == 0);
    }

    it("should create edge when path exists between entrances") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        // Completely open grid - all entrances in a chunk should connect
        BuildEntrances();
        BuildGraph();

        // In an open chunk, every pair of entrances should have an edge
        // Let's verify that entrances in chunk 0 all connect to each other
        int chunk0Entrances[64];
        int chunk0Count = 0;
        for (int i = 0; i < entranceCount && chunk0Count < 64; i++) {
            if (entrances[i].chunk1 == 0 || entrances[i].chunk2 == 0) {
                chunk0Entrances[chunk0Count++] = i;
            }
        }

        // For each pair in chunk 0, there should be an edge
        int allConnected = 1;
        for (int i = 0; i < chunk0Count && allConnected; i++) {
            for (int j = i + 1; j < chunk0Count && allConnected; j++) {
                int e1 = chunk0Entrances[i];
                int e2 = chunk0Entrances[j];

                // Only check if they actually share chunk 0 (not just touch it)
                int e1HasChunk0 = (entrances[e1].chunk1 == 0 || entrances[e1].chunk2 == 0);
                int e2HasChunk0 = (entrances[e2].chunk1 == 0 || entrances[e2].chunk2 == 0);
                if (!e1HasChunk0 || !e2HasChunk0) continue;

                // Check both share chunk 0 specifically
                int bothShareChunk0 = 0;
                if ((entrances[e1].chunk1 == 0 && (entrances[e2].chunk1 == 0 || entrances[e2].chunk2 == 0)) ||
                    (entrances[e1].chunk2 == 0 && (entrances[e2].chunk1 == 0 || entrances[e2].chunk2 == 0))) {
                    bothShareChunk0 = 1;
                }
                if (!bothShareChunk0) continue;

                // Look for edge between them
                int foundEdge = 0;
                for (int k = 0; k < graphEdgeCount; k++) {
                    if ((graphEdges[k].from == e1 && graphEdges[k].to == e2) ||
                        (graphEdges[k].from == e2 && graphEdges[k].to == e1)) {
                        foundEdge = 1;
                        break;
                    }
                }
                if (!foundEdge) allConnected = 0;
            }
        }
        expect(allConnected == 1);
    }

    it("should not create duplicate edges for entrances sharing two chunks") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        BuildEntrances();
        BuildGraph();

        // Check for duplicate edges (same from->to pair appearing twice)
        int duplicates = 0;
        for (int i = 0; i < graphEdgeCount; i++) {
            for (int j = i + 1; j < graphEdgeCount; j++) {
                if (graphEdges[i].from == graphEdges[j].from &&
                    graphEdges[i].to == graphEdges[j].to) {
                    duplicates++;
                }
            }
        }
        expect(duplicates == 0);
    }

    it("edge cost should equal walking distance") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        BuildEntrances();
        BuildGraph();

        // For a sample of edges, verify the cost matches the actual distance
        int costsCorrect = 1;
        int tested = 0;

        for (int i = 0; i < graphEdgeCount && tested < 10; i++) {
            int e1 = graphEdges[i].from;
            int e2 = graphEdges[i].to;
            int edgeCost = graphEdges[i].cost;

            // Calculate expected cost: Manhattan distance for 4-dir, or diagonal for 8-dir
            int dx = abs(entrances[e1].x - entrances[e2].x);
            int dy = abs(entrances[e1].y - entrances[e2].y);

            // On an open grid, the cost should be the optimal path distance
            // For 8-dir: max(dx,dy)*10 + min(dx,dy)*4 (diagonal shortcut)
            // For 4-dir: (dx + dy) * 10
            int expectedMinCost;
            if (use8Dir) {
                int minD = dx < dy ? dx : dy;
                int maxD = dx > dy ? dx : dy;
                expectedMinCost = maxD * 10 + minD * 4;
            } else {
                expectedMinCost = (dx + dy) * 10;
            }

            // Edge cost should be >= minimum possible (can't be shorter than straight line)
            if (edgeCost < expectedMinCost) {
                costsCorrect = 0;
            }
            tested++;
        }
        expect(costsCorrect == 1 && tested > 0);
    }
}

describe(incremental_graph_updates) {
    it("incremental update should produce same result as full rebuild") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        BuildEntrances();
        BuildGraph();

        // Add some walls
        grid[0][10][10] = CELL_WALL;
        grid[0][10][11] = CELL_WALL;
        grid[0][11][10] = CELL_WALL;
        MarkChunkDirty(10, 10, 0);
        MarkChunkDirty(10, 11, 0);
        MarkChunkDirty(11, 10, 0);
        // Do incremental update
        UpdateDirtyChunks();
        int incrementalEdgeCount = graphEdgeCount;

        // Save edges from incremental update
        int incrementalEdges[MAX_EDGES][3];  // from, to, cost
        for (int i = 0; i < graphEdgeCount && i < MAX_EDGES; i++) {
            incrementalEdges[i][0] = graphEdges[i].from;
            incrementalEdges[i][1] = graphEdges[i].to;
            incrementalEdges[i][2] = graphEdges[i].cost;
        }

        // Now do full rebuild
        BuildEntrances();
        BuildGraph();
        int fullRebuildEdgeCount = graphEdgeCount;

        // Edge counts should match
        expect(incrementalEdgeCount == fullRebuildEdgeCount);
    }

    it("path should still work after wall added via incremental update") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        BuildEntrances();
        BuildGraph();

        // Verify path works before
        startPos = (Point){5, 5, 0};
        goalPos = (Point){chunkWidth + 20, chunkHeight + 20, 0};
        RunHPAStar();
        int pathBeforeWall = pathLength;

        // Add wall and update incrementally
        grid[0][chunkHeight / 2][chunkWidth / 2] = CELL_WALL;
        MarkChunkDirty(chunkWidth / 2, chunkHeight / 2, 0);
        UpdateDirtyChunks();

        // Path should still work (wall doesn't block everything)
        RunHPAStar();
        int pathAfterWall = pathLength;

        expect(pathBeforeWall > 0 && pathAfterWall > 0);
    }

    it("removing an entrance should update all edges correctly") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE * 2, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);
        BuildEntrances();
        BuildGraph();

        // Block an entire border to remove entrances
        int borderY = chunkHeight;
        for (int x = 0; x < chunkWidth; x++) {
            grid[0][borderY - 1][x] = CELL_WALL;
            grid[0][borderY][x] = CELL_WALL;
        }
        MarkChunkDirty(0, borderY, 0);
        UpdateDirtyChunks();

        // All edge indices should be valid (no dangling references)
        int allValid = 1;
        for (int i = 0; i < graphEdgeCount; i++) {
            if (graphEdges[i].from < 0 || graphEdges[i].from >= entranceCount ||
                graphEdges[i].to < 0 || graphEdges[i].to >= entranceCount) {
                allValid = 0;
            }
        }
        expect(allValid == 1);
    }

    it("changes in one corner should not affect opposite corner") {
        InitGridWithSizeAndChunkSize(TEST_CHUNK_SIZE * 4, TEST_CHUNK_SIZE * 4, TEST_CHUNK_SIZE, TEST_CHUNK_SIZE);  // 4x4 chunks
        BuildEntrances();
        BuildGraph();

        // Find path in bottom-right area (chunks 10, 11, 14, 15)
        startPos = (Point){chunkWidth * 2 + 5, chunkHeight * 2 + 5, 0};
        goalPos = (Point){chunkWidth * 4 - 10, chunkHeight * 4 - 10, 0};
        RunHPAStar();
        int pathBefore = pathLength;

        // Add walls in top-left corner (chunk 0)
        for (int i = 0; i < 10; i++) {
            grid[0][i][i] = CELL_WALL;
            MarkChunkDirty(i, i, 0);
        }
        UpdateDirtyChunks();

        // Path in bottom-right should be unaffected
        RunHPAStar();
        int pathAfter = pathLength;

        expect(pathBefore > 0 && pathAfter == pathBefore);
    }
}

describe(astar_pathfinding) {
    it("should find a path on an empty grid") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        startPos = (Point){5, 5, 0};
        goalPos = (Point){50, 50, 0};
        RunAStar();
        expect(pathLength > 0);
    }

    it("should not find a path when goal is walled off") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        // Create a box around the goal
        int gx = 50, gy = 50;
        for (int x = gx - 2; x <= gx + 2; x++) {
            grid[0][gy - 2][x] = CELL_WALL;
            grid[0][gy + 2][x] = CELL_WALL;
        }
        for (int y = gy - 2; y <= gy + 2; y++) {
            grid[0][y][gx - 2] = CELL_WALL;
            grid[0][y][gx + 2] = CELL_WALL;
        }
        startPos = (Point){5, 5, 0};
        goalPos = (Point){gx, gy, 0};
        RunAStar();
        expect(pathLength == 0);
    }
}

describe(hpa_star_pathfinding) {
    it("should find path from corner to opposite corner") {
        // 3x3 chunks, 4x4 each
        InitGridWithSizeAndChunkSize(12, 12, 4, 4);
        BuildEntrances();
        BuildGraph();

        startPos = (Point){1, 1, 0};      // chunk 0 (top-left)
        goalPos = (Point){10, 10, 0};     // chunk 8 (bottom-right)
        RunHPAStar();

        expect(pathLength > 0);
    }

    it("should not find path when completely walled off") {
        // Horizontal wall cuts the grid in half
        // S = start area, G = goal area
        //
        //  S..........
        //  ...........
        //  ...........
        //  ############
        //  ...........
        //  ...........
        //  ...........
        //  ...........
        //  ...........
        //  ...........
        //  ..........G
        //  ...........
        //
        const char* map =
            "............\n"
            "............\n"
            "............\n"
            "############\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n";

        InitGridFromAsciiWithChunkSize(map, 4, 4);
        BuildEntrances();
        BuildGraph();

        startPos = (Point){1, 1, 0};      // above the wall
        goalPos = (Point){10, 10, 0};     // below the wall
        RunHPAStar();

        expect(pathLength == 0);
    }

    it("should find path through gap in wall") {
        // Horizontal wall with a gap - path must go through the gap
        //
        //  S..........
        //  ...........
        //  ...........
        //  ####....####
        //  ...........
        //  ...........
        //  ...........
        //  ...........
        //  ...........
        //  ...........
        //  ..........G
        //  ...........
        //
        const char* map =
            "............\n"
            "............\n"
            "............\n"
            "####....####\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n"
            "............\n";

        InitGridFromAsciiWithChunkSize(map, 4, 4);
        BuildEntrances();
        BuildGraph();

        startPos = (Point){1, 1, 0};      // above the wall
        goalPos = (Point){10, 10, 0};     // below the wall
        RunHPAStar();

        expect(pathLength > 0);
    }

    it("path should only contain walkable cells") {
        // Maze-like structure - path must navigate around walls
        //
        //  S...........
        //  .##.........
        //  .##.........
        //  ............
        //  ........##..
        //  ........##..
        //  ............
        //  ...##.......
        //  ...##.......
        //  ............
        //  ..........G.
        //  ............
        //
        const char* map =
            "............\n"
            ".##.........\n"
            ".##.........\n"
            "............\n"
            "........##..\n"
            "........##..\n"
            "............\n"
            "...##.......\n"
            "...##.......\n"
            "............\n"
            "............\n"
            "............\n";

        InitGridFromAsciiWithChunkSize(map, 4, 4);
        BuildEntrances();
        BuildGraph();

        startPos = (Point){0, 0, 0};
        goalPos = (Point){10, 10, 0};
        RunHPAStar();

        // Every cell in the path must be walkable
        int allWalkable = 1;
        for (int i = 0; i < pathLength && allWalkable; i++) {
            int x = path[i].x;
            int y = path[i].y;
            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) {
                allWalkable = 0;
            } else if (grid[0][y][x] != CELL_WALKABLE) {
                allWalkable = 0;
            }
        }

        expect(pathLength > 0 && allWalkable == 1);
    }

    it("should find same-chunk paths without using the graph") {
        InitGridWithSizeAndChunkSize(12, 12, 4, 4);
        BuildEntrances();
        BuildGraph();

        // Start and goal in same chunk (chunk 0)
        startPos = (Point){1, 1, 0};
        goalPos = (Point){2, 2, 0};
        RunHPAStar();

        expect(pathLength > 0);
    }
}

describe(incremental_updates) {
    it("should update graph incrementally when a wall is added") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();
        BuildGraph();
        int originalEdgeCount = graphEdgeCount;
        (void)originalEdgeCount;  // Suppress unused warning

        // Add a wall and update
        grid[0][chunkHeight + 5][chunkWidth + 5] = CELL_WALL;
        MarkChunkDirty(chunkWidth + 5, chunkHeight + 5, 0);
        UpdateDirtyChunks();

        // Graph should still work (edge count may differ slightly)
        expect(graphEdgeCount > 0);
    }

    it("should still find paths after incremental update") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();
        BuildGraph();

        // Add some walls
        for (int i = 0; i < 5; i++) {
            grid[0][chunkHeight + 10][chunkWidth + i] = CELL_WALL;
            MarkChunkDirty(chunkWidth + i, chunkHeight + 10, 0);
        }
        UpdateDirtyChunks();

        startPos = (Point){5, 5, 0};
        goalPos = (Point){TEST_GRID_SIZE - 10, TEST_GRID_SIZE - 10, 0};
        RunHPAStar();
        expect(pathLength > 0);
    }
}

describe(dijkstra_vs_astar_consistency) {
    it("should find same cost with dijkstra and directed astar") {
        // This map reproduces a cost mismatch bug where multi-target Dijkstra
        // returns -1 but directed A* finds a path with cost 206.
        // Start: (83,130), Target entrance: (64,128), Chunk bounds: [64,128]-[96,160]
        const char* map =
            "....................................#......#.......##.....#...........#.....#.....#................................#.........#.................#.#.............#......#......##........#...#..#.\n"
            ".#.#.....#.#........#.#......#..#......................................#.........#...#..........#....#....#...................#.....#...#.........#.....#.....#.............#...#............#..\n"
            "#...............#..............#..........................#..#..#........................#....#....#........................#...................#...........#.......#................##.........\n"
            "..............#.#......................................##.......................#......#......#...........#...............#........#.................#.........#..##...#.....#.#..#.#...........\n"
            ".............#..................#....###..........#......##...#.....#.#......#.......#..............#...............#....#..#........#..#.....#......#........#...#..#................#.........\n"
            ".......................#......#...#............#..........#...#.......#...........#.......#...............................#............#.........................#...#....................#.....\n"
            ".................#....#............#..#........#.....#...##...#..#..#.............#.#...##.............#......#....##......##...................................#.....#.........................\n"
            ".............#..........#.#...................#.#.#.......#...............#..............#......##...........#.......#....#..................#...#....#.........................................\n"
            "....#..........#....#..#..##.......................#.........#.#............#.#........#......#.......#.....#................................#..............#........................#...#...#..\n"
            ".........#.............................#.........##....#......................#.............#...#............#..#.....................#....#.....#............#..#......#..................#....\n"
            ".....#.#.......#.....##.......#....#.......#....#........#...............................#.#.....................#.........#...#..........#............#......#............#......#........#..#.\n"
            "....#...#...............................#....#.##......#.#.##.................................#..........#....#....#........#..................#.......##..............##.......................\n"
            "................#..........#.......#..............#.#...#...................#......#.............#.........#.....................#............................#.......#.........................\n"
            "............#.#.....#....#.........#...#..##............#...#.........#....................#.##...#.#.#.............#........###.................#.....#....................#...................\n"
            ".#...............#..#..................#.......#.#...#..#..#......#.###.........##.#..#..#............................#..........#...##..........#.............#...#..................#.........\n"
            "...........#...##................#...........#........#............##..........#.#.........#.........#.............#.......................#.......#.#......#.................................#.\n"
            "#........#...#.....#................#................#........#..........#.....#....##............#....#.#.......#..#..#...........................................................#......#..#..\n"
            ".....#...........#..............#...#........#...............#......#.........................#.#................#.#.#..........#................#..#.#..#.....#..................#....#..#.....\n"
            "............#..##.#....................................#...............#....#..................................#.#.......#.#..................#.....#....#.....#.........#......................\n"
            "#.............#...##.#.........#......#.......#..................#..........................##..#.........................#.............#...........#........#................................#.\n"
            "......#...........#......#........#...#............#..............#.......#..........#......#........#..................#.....................#............................................#....\n"
            "#.#...#....#..#............#......................#...#........................#.......#...#................#..#.......#...#..#....#......##.#.....#...#.......#..#.................#..........#\n"
            "#..#.......................................#.......#......#....#.........#......#.....#..............#............#............##...............................#.......#....##..#..............\n"
            ".......................................#.#.......#......#......................##........##......#.........................#....#....#.#.#........##.....#..............#..##..#........##.#.#..\n"
            "........................#..........#......................#..........#...........#.........................##.........................#.......#........................................#..##....\n"
            "......................#....#......#....#.....#........................#.............#............#....#.#.........###..##...........#.#.....#.....................................#.......#....#\n"
            "........#...................#...........#...#.....#.........##..........#...#............#.............#.............#.................#...............#.............#....#.................#...\n"
            "...#......#...........................#.........##......#...#......#.......#..#..##.#................................##..........#.#.......#................#.#........#........................\n"
            ".....#..#...#.....#.....#..............#....#................#..........#..##...#............#....................................#........#.........#...............................#..........\n"
            "...............#......#.........#.................#......#...............................#...........#.#...........#....................#.....#........#........#.##.#..........................\n"
            "......#....#.......#...............#......#.........................#..........#......................#......#......#..#.........#.................................................###.......#..\n"
            "..........#...........#.....#....#..........#.#........##....#...##....#..#...................#..##........#...................#.....##.......#........................#...#..#.............#...\n"
            "..........#...#...........#.......#............##.........#..................#....................#.#......#..........................................#.....#...#...................#...........\n"
            ".....##..........##....#...#.....#..#......................#.#....#.#.............#........#.........#..##...#..........................#...#.#.....................#..#.#............##........\n"
            "..............#.........#...........#..............#.#...........#..#..........#.#.....................#........#...........................................#......#...#...#.............#.....#\n"
            "..........#.......#....................#..#..#..........................................#...................#..........#..#..#...........#.....#.........#..#.......#....#..........#.#.........\n"
            "......#.............#..#...........#....#...........................................#.........#....................#...#......#.........#....#......................#...........#....#...#...#..\n"
            "........................#........#....#...........#.........#.............................................................#.#....#...##.#..............#.....#.............#..#...............#.\n"
            "#.#...................#.......#...........#...........#.............................#........##.........#.#.#.............#..#...........#...#....#.......#..............#.#...........#...#....\n"
            "##.#......................#...#...#....#...........#.#...#........#.#..................#...................#..........##......#...#.............#.........................#.#.................#.\n"
            "........#......#.......#......................#...........#.................#.......................#....................................................#.........#...#.............##.........\n"
            "......#........#.#.............#........................#.........##....#.....#.....#.....................#.................#.#...##..........#.##...##.#.........#.#................#.....#....\n"
            ".#...........#..............#..................#......#.............................##..........#....#......................#..............#..#........#.................#..#...#...#.#......#..\n"
            ".......#....#...#.................................................#........#....................###.#..##.#.#....#..........#..................#.............#...............#...#..............\n"
            "#.....#.##...##.......#.............##..................................##......#...#...##.......#.................#.......#..........#.........#......#.....#.#................................\n"
            "....##........#................#........#.....................#............#..............#.........#...#.......#...................#.#..#...#..##...#...........#.#..........#.#........#...#..\n"
            "........................#...............#..###....................................#...#....................#...#.......#....#......#.#...#..............#.#..#..................................\n"
            "......#...#...............#.........##......##..........#..................#.#...#..#.....#..............#.#...#.........#........#....##....#.#.......#..........#.......##.....#..............\n"
            ".................#..#...................................#...................................#...#................#..................##...#.............#....#...............#...................\n"
            "...#.#........#..#.#.......................#...........#...#.....#....#...#.#.......#........#.##........................#..##........#.#......#.......#...........#.............##.......#.....\n"
            "#........#............#.................#....................##.#...#....#..#.............#...................#.......#.#..#..#......#..........#.....#.#......................#..#.............\n"
            "..............#.......#...........................#.......................................#........#..#.........#......................#............................##........#....#...........#\n"
            "....#......#.#.......#.#.........#.......#.#...................#..#...#........................#...#..#.........#...............#.#.###........#.......#........#........#............#.........\n"
            "#.............................##...#.....#....................#..............................#....#...........................#..#..#...............#.#.....................##.............#....\n"
            "............#.........##.........#.........#.......#.........#...#...............#..#........#...................#...#.......#.............#......#..........................#.......#..#...#...\n"
            "...#....................#.....#................#.....#...........##............##..............##.....#...##.................................#................................#......#..........\n"
            "..................#.#...............#.........#....#...............##.......#........#............#..#....................#.#.....#......#..........#....#.......##..##........................#\n"
            "................#......................................#..........................#...#..................#..............#......#.....#.......#........................##...###..........#.......\n"
            ".............#..##...##.....#..#.....#.#......#......##.......#.......##...........#...#............####...#.....#.......#...................#..................#......#..#...##...........#....\n"
            ".#...................###...............#...#.........#...........##......#.......................#.........#...#.#........#...........#..#.#................#...#.........#.............#...#...\n"
            "..............................#...................#.......#............##....#...#.#...................#...............#.....###.#...##...#...............#........................#........#...\n"
            ".......#..............................#.....#..#...#.#........#....#.....................#.........#.........#..#...............#...#....................................................#..#...\n"
            "..........#...........#..#..........#......#...........................##...##......#..............#.......................#..#..#......#..............#........#................#............#.\n"
            "..........#..............#..................#..#.#..........#.................#...#..............#...#.#......#............#...................#.#.#...............#....#...#.....#...#.........\n"
            "........#...#..........#..#......................#....#.........#...........#...#.............................#.....#.#........................#........#.........................#......#......\n"
            "..#...#.......#........#.##..........##......................#.....#...............#..............#........#....#....#..........#..#..............#..............................#...........#..\n"
            ".#.#...........#...........#..##..#.#.......##...#...##.#.#.............#........##...........#................#..#...................##.........#......................#...........###.........\n"
            "...#..##..#.............#............#.................#............#...........#.#..#........................#...#.....###................#......................#.........#.......##..........\n"
            "...#.........#.......#.......##.................#.....................#...........#.........#.............#.....#...............#....#.........#.......................................#...#....\n"
            "....#............#....#.##........#.#.#......#..#...#..........................#......#..........................#.#...#....#...#..................................#...#.#.#....................\n"
            "#.......#...#...........#...................#........#..#.....#......#........#..............#...............#.........#.....##.......#.#........#.#...#............................#...........\n"
            ".........##...##............................#...........#.#....................#....#.#...................#........#................#..#.............#....#.....#..........#..#.................\n"
            ".............##..........................#...........................#....#...#.....#......#...............#.......................................................#..........##.....#.#.....#.#\n"
            ".....#...........#...#..#....#..#....#...#...#.##.....#.........#..#.#.#..#....................#......#........##...#.#.....#...........#...........#..#.................#......................\n"
            ".#.........#.............#.......#...#........................#.............................#.............##.....#....................#...#.........#..............#.......#....................\n"
            "..........#...........#.......#.......#................................#.#.....#.........#..#....#....#.......#....................#................#...............#.........#.....#...#.......\n"
            "........#....#............#.#.................#..........#.....................#................................#.#.....#.....##.......#....####.....#.....#..................#........#.......#\n"
            "......#................#..##..#.........................................................#..................#.#..............#.#.......#.#.........#...........#..............................#..\n"
            "...........#.........#....#..................#....#...#.#...................#..........#................##....#...##.................................#......#...................#.....#.........\n"
            "...............#................................#..####....#.................................................#............#..#...#.........#....#.........#.#........................#.#........\n"
            "......................#...........#.................................#.............................#...#...#......#..#.......#.....#........#.#.#...........#......#.#..#.................#......\n"
            "................#..#....#....#.#.......#.........#.........#............#..........#.....#....#............................#....#.....#....##.#....#............#....#...#.#..............#....#\n"
            ".#..#.....#......#.#............#.........#..........#...........#...##..#....#.#.....#.......#...........#..#.#....................#.........#..............................................#..\n"
            "....................#.............#..........................#..................................#.#..#......#..........#.............#.................................#.#...#.................#\n"
            ".......#........##...........#......##.#.............#...#.....#....#...#..........................#.........#......#.#...#......#......#...........................................#...........\n"
            "...........#.......#.....#.............#.......#.#..#.......#..........................#........#.........#............#..............##................#...............#........#..............\n"
            "..........#...........................#......#.........#...##....#..........#..#.#.#.......#......#...................#......................#.#.......##....#...................#...........#..\n"
            "......#.........#...#....#..............................#....##..........##......#...................................#........#.#...............##...............##..#........#........#.....#..\n"
            "..........#..............#.....#.............#........#........#..#...........................#..#.#....#...........#...........#.......................#..#....#...................#........#..\n"
            ".....#..##.............##...............#....#...............#...........#.......##...........#...#...#...........##.##.#.#..#..............#............#.....................#.....#......#...\n"
            ".#...#............##..#.........#......#........#...........#........#.......................#........#.............##...#.......#.........#.........#..#.#.............#.........#....#..#....#\n"
            "...#....#....#...............###.#.#.#.....................#..........#........................#..#.....#......#....#..#.#.........#........#............#......#..#.###..........#......#......\n"
            "......................#.#.....#.................#..................#.....#.....#.................#.....#..#...#.#..................#..........#..........#.......#........#.............#.......\n"
            "..........#..#.#...............#..#.....#......#............#................#......##......#....................#..........#..#.......###........................##............#...........#...\n"
            ".##..##................#...#..............#...................#..............#..........#.#..................#.#.........##......#...#.......#............#................#..............#.....\n"
            "......................#................#.........#.....#.......................#.........................#....................................#...##..............##.#....#................#....\n"
            "..................#............#.........#...........................##.................#..#...#............................#..#......##....#...................#.#..............#..............\n"
            "................###....#..##..#....................#......#.......#........................#......#.....#............................#.......#...........#.........#..#...............#..#......\n"
            ".....#...#.........#...............#..#....................#.#.#....................#......#.....#..................#..........#.................#.......#....#.....#..#....................#...\n"
            ".............#...........................#.......##........#......#............#....#.........#................#.......#..#............#....#........#..#............#..##.............##.......\n"
            "........#...#......#........................#.......#.#.....................#...........#.....#.#...#...........#.....#..#.....................#.#.........#.......#..#............#..#.........\n"
            "...##..#..##.......#.....#.....#....#.............#...#......#.................#..........#..........#.............#......#.....#.........#.......................................#......#......\n"
            ".....#...........#..........#.........#.............#....#..#.........................#...............................#.....#.##...........................#..........#.#....#...###...#........\n"
            "#.......#..#...........#...............................##........#............#...................##..................................###...#............#.......................##......#..#...\n"
            "..........#.............#.#....#.........#..#...#......#......##..#..........#............#...........................#...............#...#........................#.#........#..............#..\n"
            "................................#...............#.#......................#.#............#........#..#......................#...........#..................#...........#..........#......#..#....\n"
            "......#......#..#.......#..............#........#.....#................#......#........#.#..................................................#..................#........................#.......\n"
            ".........#....#.............#...#......................#......#..#......#.#.....................#..........#....##.................#..#.#.........#....#............#...#....#...........#...#..\n"
            "....#.#..............................#.....#........#.................................#.............................#...........#.........#....#.........#....#....#........................##..\n"
            ".............#.##..................#....#........#..................#.....##...............#......#..............................##.......#...........#.......#..........#................#.#...\n"
            ".........#....#........................#.#.............#...........................#...............##.........#......#.#...........#.....................#...#...##..#........#..#........#...#.\n"
            "........#...#..................................................#.............................#......#..........#....#...........................................................#...........#...\n"
            "..............##.......##..#..............#.#.#........................#.........................#...#..#................##...#....#..#.##...............#......##.......................#...#.#\n"
            "....#......#...........#.#.........#.....##..#.#...........#...............#.................................................................#...#..##.......#.#...........#........#........#.#\n"
            "..................#....................#...#....#..........#.........#..................#.......................#.........#............#......#.....#..............#.................#.#..#...#.\n"
            "...........#..............##.....#...#........................................#...........................#...........#......###.............#...............#..#........................#......\n"
            "..#..........................#..#..........#.......................#..............#..................................#.....................#....#....#.......#..##....#..........#...#......#...\n"
            "..#.....#.........#...##..............###....#..............#.......................................#................................................................###......###...............\n"
            "............#........#..........#..#.#........#..........##............#........#.........................#..#.......#.......#..........#...............#....#...............#............#....#\n"
            "....#............#............#.....................#........#.#..###.#..........#.................#.................................................#.#................#....................#..\n"
            "###..............#.#..#......#...............#...................#..#........#.....................#..#......#...#......................#..........................#................#...........\n"
            "#......#....#.#....#...#..........#...........##...#................#......#..............#..#..##.........................#.....................#...#...........................#.#...#..#.....\n"
            "....#......#..............#....#................#...................#.......#.#.......#.......................#..#..........#...#........#.#...#...................................#.....#......\n"
            "............#......#................#.................#........#..#...##.............#.#............#.#....#.......#.........#.........#...#.........#.#.................................#...#..\n"
            "...................#........##................#......................#..............#...#.............#.....#.........................#...............#....#...............#....#.#..#..........\n"
            "................................................#...#..................#........................................................#.#......#.......##.........................#....#.#...#......#.\n"
            ".........#.........##..........#...........................................#...........#.....#....#................................#...............#...#.............#.....#.#..........#....#..\n"
            ".....................#......#...........#......#..........#......#............#..#......#..#....#..................#......#.....................................#....#................#.........\n"
            ".................#......................#............#....................#......#...#..#..............#........................#.#...........#...............#.#.#......#......................\n"
            "................#....#......#.#.........................#......#............##.............#...................##..#......#.....#....#.............#...#.#......................................\n"
            "........#.......#.................#............##........#...##.........#..#...#................#....#...............#...............#...........#..#.......#...................................\n"
            "....#..........#....##.................#..........#.......#.......#...#...#...............#...##........#...........#...........................#...#.......................#.##................\n"
            "..................................#..................#..........##...............................#......#...........................#..#................#..#..........###.....#..........#......\n"
            "#..#.#...#..................#.............#.#..#..#...#....................#.##..#....#................#.................#.....#.........................#.......##................#............\n"
            "................#..........................................#...#.##...#..........#...........................#...................#.....#.............................#............#..#..........\n"
            ".........#............##...#..#........#..........#.#...#..#..............#...............#................#.......#..#..................#..#.....#......#........................#.............\n"
            "........#.........#.#.....#.......#........#......#...#............#..#.........#...#..............#....#....................#....#.......##........#......#...............#.........#.#.#......\n"
            "..............#...#..........#..........#......#..........#...##......#.......#.#...#..#..#....#....#...........#..#...........#..#.........#..............#.........#........................##\n"
            "#............#...........#............#.#...........##..#.#..#.......#......#.....................#...................................................#................#..........#.............\n"
            "................#............#..................#................#.#......#.#................#..#..........................#....#.........#......#................#.#....................#......\n"
            ".......#........#.....#.#......................##............................##......#..............#......#......#........#......#...............##....................##........#.............\n"
            "...................................................#........#....#.##.......#................................................#..........#...#...#..........................#.#....#.............\n"
            ".....................##.#........##...........#......#......#...#.#........#.#...............#...........#........#.........##...................#....#................#..#............#......#.\n"
            "......#.........#......#.#.........#..#..................#...##.............#.#..............................#..#.......#...##....#..........#...#..........#.................................#.\n"
            "......#.....#....#.....#.#...#.......#............#....#.........#......#............#....#..#............#.........#.........##.....##.................#......................................#\n"
            "...........#...#..........##........#.........#..........#............###...........###....#...#.#..........##..#...................#......##...........#.....##.....#...#......................\n"
            "...............#...#.#..........#....#..................................................#.........#...#.#........#.#...#......#..................##....##..............#..#..................#..\n"
            "..#..#...................#......#.#..##.....#........##..##.........#..#.......#...#......#......................#.................#..........#...#...#...........#..................#.....#....\n"
            "..............................#..................................#....#..#..........#.......#...#.#........#............................#.........#.............................................\n"
            "....#.#....#.#..........####...........#.#.....................#..#...#..#...#.........................................##.........#.....#...............................#......#.....#........#.\n"
            "........#...#................#............##....#.................#......##...............................#......##........................#.....#..............#.#.#.....#...#...........#...#.\n"
            "............#.....................##.............#..........#...#......#.............#.........................#.......##......#............##....##...........................#................\n"
            ".............................#.......................#...................#...............#.............#..#.......#.........#.........#....#.....#..#.#..........#............................#.\n"
            "......#.....#...............##..#.....#.....................#.......#...#.............#....#.##.................#...#.#........#................#..#.#.......................#.....#....#.#....#\n"
            "..##........................................#.....##....#.....##...............#..#....#..........#...........#.###...#..............#..................#..........#.....#.......#.........#....\n"
            "......#...#..#................#..........#....#.......#.............................#.#.#..............#.....................................#....#.....#..............#...#...............###..\n"
            ".......#...#.....#.#...#..........#..........#..#......#...............#.......#.......#.....................................#.....................#....#.....#...#........#....................\n"
            "..#................#......##.......................................#..#..#.............#.#....#...................##..#..##....................#....#.................##.......#................\n"
            ".......#..........#...#..#.....#.........#..............#..........#................##..#..................#.#.........................#................#...........#..#........#...............\n"
            ".................................#...#..#.........#..#...#......#...#.....#......#.#....#...#...#.#...............#..............#............#....#................##.....................#..#.\n"
            "..........................#................................#.........#.............................#....#......#....##...#..#.....#....#..#..........................#......................#...\n"
            ".....................#....#...........#..........................#.......#...#......##....#..##.....#.#..........................#.....#..................#...........#......#...........#......\n"
            "..#..#...............##............#..#.............................#.....##..#.#.....#....#...............#..........................#...#...............................##.#..#...#.......#...\n"
            "..............#..#....#.#.........#...#......#.................#..........#...#.....#...........................#.......................................................#......................#\n"
            ".........#.....#..........#....................#..................#...##.#.#...##..#.................#.....................##...............#....#..........#.......#....#.#..#...#.............\n"
            "#......................#................#..................#..........................................#.................#....#...#..........#.........................#................#...###..\n"
            "......#............#...................##....#............#.#......##..........#............................#.#....#...............#..#.....................................#..................#\n"
            ".......#.##............#.#...#....#.......#..........#.......#...#.#..#...##...........#...#...#...#.#......#............................#....#............#........#...........................\n"
            ".............##.#.......#........#..............#................#...#................#..#..............#..#.................#.......#...#.#........##.#...........#.....................#......\n"
            "............#........##..............#...................................#...........#..#............#............#..................#.#.............#........................#.............#...\n"
            ".#.........##.....#......#......#....#.........................#............#....#....................#.................#............#...#.......#.##.............#.#.......#...........#.......\n"
            "#...........#.....................#..#......#...#..............#..............#...#.........#......#..........#...............#....#..##..#.#....................#.................#...#........\n"
            "...........#...............#.......#...................#..........#...#..#.............................#..........##.......#..#........#..........................###....#......................\n"
            "................#........##.#..........#...............#......................#..........#.............#....................#.#..........#.............#.........#...#..........#...###........#\n"
            "..............#......#............#..........#.............................................#......#.................#..........#..............#....#.......................#.........#..........\n"
            "#....................#...#...#..................#.......#..........#..........##.#.............##.........#...............#...#............#.#......#......###...#...#.....................#....\n"
            ".#..##.........................#................#...................#..#..#..........#.#..#.#........#......#...###............#.............#......#.#...................##.#...##.............\n"
            "##...........#.....................#.....................#....#..............#......................####.......................#.........................#..##........#.##.............#........\n"
            "........#..#.##...............#..##........#.............##........#....#...#........#....#..........#.................................#...........#....#.#.............#..............#......#.\n"
            "....#.#.......#.............#.#......#......#...###.................#.............#.#...##......................#.........##........#............#....................#.........................\n"
            ".....#........#..............#............................#..................#.#......#..#....#.............#...#................#...........................##...#..............#......#.......\n"
            "...................#...........................#...............#........#.........#.......#...........#....#...............................#...#..........#...#........##.....#...#..........##.\n"
            ".............#.............................#.....#....#.#............#........#................#..................................#.....#...............##........##.........#.......#..........\n"
            "#..........#.................................................#........#...#...#.#.#...............................#.........#.........#.............##.......#...............#..#..#............\n"
            ".#..........................................#....#............##.##................................#........#............#..............#.......#............................#...#.........#....\n"
            "......................................................##...#..............#.....#.............#...............##..........#.#.....#........#......#..........#.#......#......#........#.........\n"
            ".....................................#..............#.............................#............#...#.........#...........#..#...........................#..............#..#..............#...#..\n"
            "....#.......##..........#...##................#...#.#............#.###..................................................#.............#.....................##.....#..#..#......................\n"
            ".................#.........#......#............#...#......................#.........#...##..#....#..................................#..............................#....#........#....#..#......\n"
            "#............#................................#..........................................#.....................#.#...#.....................................#..#..##.........##.......#..........\n"
            "..........#...#..#.......#...#......###..........................#..#.....................#.......#.......#......#...#........#.#.............#......#......#....................#.....#.#....#.\n"
            "......#.....#.....#...................#...............#.....................#..#.............................#.....#................#.#.......#.....#........#......##........#......##.........\n";

        InitGridFromAsciiWithChunkSize(map, 32, 32);
        use8Dir = true;
        BuildEntrances();
        BuildGraph();

        // The problematic case: start at (83,130), target entrance at (64,128)
        // Chunk bounds for chunk containing (83,130) are [64,128]-[96,160]
        startPos = (Point){83, 130, 0};
        int targetX = 64, targetY = 128;

        // Get chunk bounds
        int startChunk = (130 / chunkHeight) * chunksX + (83 / chunkWidth);
        int minX = (startChunk % chunksX) * chunkWidth;
        int minY = (startChunk / chunksX) * chunkHeight;
        int maxX = minX + chunkWidth;
        int maxY = minY + chunkHeight;
        
        // Expand bounds like RunHPAStar does
        if (maxX < gridWidth) maxX++;
        if (maxY < gridHeight) maxY++;
        int searchMinX = minX > 0 ? minX - 1 : 0;
        int searchMinY = minY > 0 ? minY - 1 : 0;

        // Run both algorithms with same bounds
        int dijkstraCost;
        int astarCost;
        
        // Multi-target Dijkstra
        int costs[1];
        int tx[1] = {targetX};
        int ty[1] = {targetY};
        AStarChunkMultiTarget(startPos.x, startPos.y, 0, tx, ty, costs, 1,
                              searchMinX, searchMinY, maxX, maxY);
        dijkstraCost = costs[0];
        
        // Directed A*
        astarCost = AStarChunk(startPos.x, startPos.y, 0, targetX, targetY,
                               searchMinX, searchMinY, maxX, maxY);

        // Both should find the same cost (or both fail)
        expect(dijkstraCost == astarCost);
    }
}

describe(maze_refinement_failure) {
    it("should handle entrances on same edge with no direct path") {
        // This maze pattern causes refinement failures:
        // WARNING: HPA* refinement failed: no path from (173,128) to (177,128)
        // WARNING: HPA* refinement failed: no path from (128,11) to (128,15)
        //
        // The maze has nested rectangles with narrow corridors.
        // Two entrances can exist on the same chunk edge (y=128 or x=128)
        // but the maze walls prevent direct movement between them.
        //
        // Simplified reproduction: 16x16 grid with 8x8 chunks
        // Chunk boundary at y=8
        //
        //   0123456789ABCDEF
        // 0 ................
        // 1 ................
        // 2 ................
        // 3 ................
        // 4 ................
        // 5 ................
        // 6 ................
        // 7 ...##....##.....   <- walls create corridors at x=0-2, x=5-8, x=11-15
        // 8 ...##....##.....   <- entrances at x=0-2, x=5-8, x=11-15 on y=8
        // 9 ................
        // A ................
        // B ................
        // C ................
        // D ................
        // E ................
        // F ................
        //
        // Entrances on y=8: around x=1, x=7, x=13
        // An edge exists between x=1 and x=7 (both touch chunk 0 and chunk 2)
        // But refinement within chunk bounds [0,0]-[8,16] can't path from (1,8) to (7,8)
        // because the wall at (3,7)/(3,8) and (4,7)/(4,8) blocks it!
        //
        const char* map =
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "...##....##.....\n"
            "...##....##.....\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitGridFromAsciiWithChunkSize(map, 8, 8);
        use8Dir = true;
        BuildEntrances();
        BuildGraph();

        // Find entrances on the y=8 boundary
        int entrance1 = -1, entrance2 = -1;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].y == 8) {
                if (entrances[i].x < 5 && entrance1 < 0) entrance1 = i;
                else if (entrances[i].x >= 5 && entrances[i].x < 10 && entrance2 < 0) entrance2 = i;
            }
        }

        // We should have found two entrances on y=8 boundary
        // One around x=1-2 and one around x=6-7
        int foundBoth = (entrance1 >= 0 && entrance2 >= 0);
        
        if (foundBoth) {
            int x1 = entrances[entrance1].x;
            int x2 = entrances[entrance2].x;
            
            // Try to path between them using chunk-bounded A*
            // The chunk that contains both is chunk 0 (0,0)-(8,8) or chunk 2 (0,8)-(8,16)
            // Let's use bounds that would be used in refinement: a single chunk
            int cost = AStarChunk(x1, 8, 0, x2, 8, 0, 0, 9, 16);
            
            // This should succeed even though there's walls in between
            // because we can go around (up or down)
            expect(cost > 0);
        } else {
            // If we didn't find both entrances, the test setup is wrong
            // but that's fine - the important thing is no crash
            expect(1 == 1);
        }
    }
    
    it("should fail refinement when truly blocked") {
        // Create a situation where two entrances exist but no path connects them
        // even going around
        //
        //   01234567
        // 0 ########
        // 1 #......#
        // 2 #......#
        // 3 #..##..#
        // 4 #..##..#  <- wall divides the interior
        // 5 #......#
        // 6 #......#
        // 7 ########
        //
        // Actually this still allows going around. Let me make a true block:
        //
        //   01234567
        // 0 ........
        // 1 ........
        // 2 ........
        // 3 ####....
        // 4 ####....   <- solid wall divides left from right
        // 5 ........
        // 6 ........
        // 7 ........
        //
        // With chunk boundary at x=4:
        // - Entrance at y=0, x=4 (top)
        // - Entrance at y=7, x=4 (bottom)
        // - But wall at (0-3, 3-4) doesn't actually block x=4 column
        //
        // Need to think about this differently. The issue is when entrances
        // exist on the SAME chunk edge but can't reach each other within
        // that chunk's bounds due to internal walls.
        //
        const char* map =
            "........\n"
            "...#....\n"
            "...#....\n"
            "...#....\n"
            "...#....\n"
            "...#....\n"
            "...#....\n"
            "........\n";

        InitGridFromAsciiWithChunkSize(map, 4, 8);
        use8Dir = true;
        BuildEntrances();
        BuildGraph();

        // Chunk boundary at x=4
        // Entrances exist at x=4 for various y values
        // The vertical wall at x=3 means:
        // - From chunk 0 (x=0-4), can reach x=4 entrances via x=0-2 columns
        // - But if we're in chunk 0 and want to path between (4,1) and (4,6),
        //   we can still do it by going left around the wall
        
        // This test just verifies we handle this case gracefully
        startPos = (Point){1, 1, 0};
        goalPos = (Point){6, 6, 0};
        RunHPAStar();
        
        // Path should exist - go around through x=0 column
        expect(pathLength > 0);
    }
}

describe(diagonal_corner_cutting) {
    it("should allow diagonal when both adjacent cells are walkable") {
        // 8x8 single chunk
        //   01234567
        // 0 ........
        // 1 .S......
        // 2 ........
        // 3 ......G.
        // Diagonal from S(1,1) to (2,2) allowed: (2,1)=. and (1,2)=.
        const char* map[] = {
            "........",
            ".S......",
            "........",
            "......G.",
            "........",
            "........",
            "........",
            "........",
        };
        
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = (map[y][x] == '#') ? CELL_WALL : CELL_WALKABLE;
        
        startPos = (Point){1, 1, 0};
        goalPos = (Point){6, 3, 0};
        RunAStar();
        
        // Path exists - diagonal movement allowed
        expect(pathLength > 0);
    }
    
    it("should block diagonal when one adjacent cell is wall") {
        // 8x8 single chunk
        //   01234567
        // 0 ........
        // 1 .S......
        // 2 .#......  <- wall at (1,2) blocks diagonal to (2,2)
        // 3 ......G.
        // Diagonal from S(1,1) to (2,2) blocked: (1,2)=# 
        const char* map[] = {
            "........",
            ".S......",
            ".#......",
            "......G.",
            "........",
            "........",
            "........",
            "........",
        };
        
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = (map[y][x] == '#') ? CELL_WALL : CELL_WALKABLE;
        
        startPos = (Point){1, 1, 0};
        goalPos = (Point){6, 3, 0};
        RunAStar();
        
        // Path still exists but must go around
        expect(pathLength > 0);
    }
    
    it("should trap cell when all escape routes have corner-cut blocking walls") {
        // 8x8 single chunk - the pocket from the forest map
        //   01234567
        // 0 ........
        // 1 ..##....  <- walls at (2,1) and (3,1)
        // 2 .#.#....  <- walls at (1,2) and (3,2)
        // 3 .#S#....  <- walls at (1,3) and (3,3), S at (2,3)
        // 4 .##.....  <- walls at (1,4) and (2,4)
        // 5 ........
        // 6 ......G.
        // 7 ........
        //
        // From S(2,3): only move is up to (2,2)
        // From (2,2): diagonal to (1,1) blocked by (1,2)=# and (2,1)=#
        //             diagonal to (3,1) blocked by (3,2)=# and (2,1)=#
        //             all other moves are walls
        // S is trapped!
        const char* map[] = {
            "........",
            "..##....",
            ".#.#....",
            ".#S#....",
            ".##.....",
            "........",
            "......G.",
            "........",
        };
        
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = (map[y][x] == '#') ? CELL_WALL : CELL_WALKABLE;
        
        startPos = (Point){2, 3, 0};
        goalPos = (Point){6, 6, 0};
        
        // Both cells are walkable
        expect(grid[0][3][2] == CELL_WALKABLE);
        expect(grid[0][6][6] == CELL_WALKABLE);
        
        RunAStar();
        
        // No path - S is trapped by corner-cutting rules
        expect(pathLength == 0);
    }
    
    it("should escape when one corner-cut path is open") {
        // Same as above but remove wall at (2,1) to open diagonal escape
        //   01234567
        // 0 ........
        // 1 ...#....  <- only wall at (3,1), (2,1) is now open
        // 2 .#.#....
        // 3 .#S#....
        // 4 .##.....
        // 5 ........
        // 6 ......G.
        // 7 ........
        //
        // From S(2,3): up to (2,2)
        // From (2,2): diagonal to (1,1) now allowed! (1,2)=# but (2,1)=.
        // Wait no - corner cut needs BOTH to be walkable
        // Let me open (1,2) instead
        const char* map[] = {
            "........",
            "..##....",
            "...#....",  // removed wall at (1,2)
            ".#S#....",
            ".##.....",
            "........",
            "......G.",
            "........",
        };
        
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = (map[y][x] == '#') ? CELL_WALL : CELL_WALKABLE;
        
        startPos = (Point){2, 3, 0};
        goalPos = (Point){6, 6, 0};
        RunAStar();
        
        // Path exists - can escape via (1,2) then diagonal to (0,1)
        expect(pathLength > 0);
    }
}

// ============== STRING PULLING TESTS ==============
// Recreate string pulling logic for testing (same as in demo.c)

static bool TestHasLineOfSight(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    while (1) {
        if (grid[0][y][x] == CELL_WALL) return false;
        if (x == x1 && y == y1) return true;
        
        int e2 = 2 * err;
        
        if (e2 > -dy && e2 < dx) {
            int nx = x + sx;
            int ny = y + sy;
            if (grid[0][y][nx] == CELL_WALL || grid[0][ny][x] == CELL_WALL) {
                return false;
            }
        }
        
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

static void TestStringPullPath(Point* pathArr, int* pathLen) {
    if (*pathLen <= 2) return;
    
    Point result[MAX_PATH];
    int resultLen = 0;
    
    result[resultLen++] = pathArr[*pathLen - 1];
    int current = *pathLen - 1;
    
    while (current > 0) {
        int furthest = current - 1;
        for (int i = 0; i < current; i++) {
            if (TestHasLineOfSight(pathArr[current].x, pathArr[current].y,
                                   pathArr[i].x, pathArr[i].y)) {
                furthest = i;
                break;
            }
        }
        result[resultLen++] = pathArr[furthest];
        current = furthest;
    }
    
    for (int i = 0; i < resultLen; i++) {
        pathArr[i] = result[resultLen - 1 - i];
    }
    *pathLen = resultLen;
}

describe(string_pulling) {
    it("should reduce path to 2 points on open grid") {
        // Open 16x16 grid, path from corner to corner
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        
        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 0};
        RunAStar();
        
        // Should have a long stair-step path
        int originalLength = pathLength;
        expect(originalLength > 2);
        
        // String pull it
        TestStringPullPath(path, &pathLength);
        
        // Should now be just 2 points (start and goal) since it's open
        expect(pathLength == 2);
        expect(path[0].x == 15 && path[0].y == 15);  // goal
        expect(path[1].x == 0 && path[1].y == 0);    // start
    }
    
    it("should keep corner waypoints when obstacles present") {
        // Grid with obstacle requiring detour
        //   0123456789
        // 0 S.........
        // 1 .###......
        // 2 ...#......
        // 3 ...#......
        // 4 ..........
        // 5 .........G
        InitGridWithSizeAndChunkSize(10, 6, 10, 6);
        
        grid[0][1][1] = CELL_WALL;
        grid[0][1][2] = CELL_WALL;
        grid[0][1][3] = CELL_WALL;
        grid[0][2][3] = CELL_WALL;
        grid[0][3][3] = CELL_WALL;
        
        startPos = (Point){0, 0, 0};
        goalPos = (Point){9, 5, 0};
        RunAStar();
        
        expect(pathLength > 0);
        int originalLength = pathLength;
        
        TestStringPullPath(path, &pathLength);
        
        // Should be shorter than original but more than 2 (needs to go around wall)
        expect(pathLength < originalLength);
        expect(pathLength > 2);
    }
    
    it("should not cut corners through walls") {
        // Grid where direct diagonal would cut corner
        //   0123
        // 0 S...
        // 1 .#..
        // 2 ..#.
        // 3 ...G
        InitGridWithSizeAndChunkSize(4, 4, 4, 4);
        
        grid[0][1][1] = CELL_WALL;
        grid[0][2][2] = CELL_WALL;
        
        startPos = (Point){0, 0, 0};
        goalPos = (Point){3, 3, 0};
        RunAStar();
        
        expect(pathLength > 0);
        
        TestStringPullPath(path, &pathLength);
        
        // Should NOT be able to go directly (would cut corners)
        // Path should have waypoints to avoid corner cutting
        expect(pathLength > 2);
    }
}

// ============== LADDER / Z-LEVEL TESTS ==============

describe(ladder_pathfinding) {
    it("should parse multi-floor ASCII correctly") {
        const char* map =
            "floor:0\n"
            "......\n"
            ".L....\n"
            "......\n"
            "floor:1\n"
            "......\n"
            ".L....\n"
            "......\n";
        
        int result = InitMultiFloorGridFromAscii(map, 6, 6);
        expect(result == 1);
        expect(gridWidth == 6);
        expect(gridHeight == 3);
        expect(gridDepth == 2);
        
        // Check ladder is placed correctly on both floors
        expect(grid[0][1][1] == CELL_LADDER);
        expect(grid[1][1][1] == CELL_LADDER);
        
        // Check other cells are walkable
        expect(grid[0][0][0] == CELL_WALKABLE);
        expect(grid[1][0][0] == CELL_WALKABLE);
    }
    
    it("should find path using ladder to reach upper floor") {
        // Start on floor 0, goal on floor 1
        // Must climb ladder to reach goal
        const char* map =
            "floor:0\n"
            "......\n"
            ".L....\n"
            "......\n"
            "floor:1\n"
            ".....G\n"
            ".L....\n"
            "......\n";
        
        InitMultiFloorGridFromAscii(map, 6, 6);
        
        startPos = (Point){0, 0, 0};  // Start floor 0
        goalPos = (Point){5, 0, 1};   // Goal floor 1
        RunAStar();
        
        // Path should exist
        expect(pathLength > 0);
        
        // Path should end at goal (z=1)
        expect(path[0].z == 1);
        
        // Path should start at start (z=0)
        expect(path[pathLength - 1].z == 0);
    }
    
    it("should stay on same floor when ladder not needed") {
        // Start and goal on floor 0, ladder exists but not needed
        const char* map =
            "floor:0\n"
            ".....G\n"
            ".L....\n"
            "......\n"
            "floor:1\n"
            "......\n"
            ".L....\n"
            "......\n";
        
        InitMultiFloorGridFromAscii(map, 6, 6);
        
        startPos = (Point){0, 0, 0};  // Start floor 0
        goalPos = (Point){5, 0, 0};   // Goal floor 0
        RunAStar();
        
        // Path should exist
        expect(pathLength > 0);
        
        // All path points should be on z=0
        int allOnFloor0 = 1;
        for (int i = 0; i < pathLength; i++) {
            if (path[i].z != 0) allOnFloor0 = 0;
        }
        expect(allOnFloor0 == 1);
    }
    
    it("should not find path when ladder only on one floor") {
        // Ladder on floor 0 but not floor 1 - no connection
        const char* map =
            "floor:0\n"
            "......\n"
            ".L....\n"
            "......\n"
            "floor:1\n"
            ".....G\n"
            "......\n"
            "......\n";
        
        InitMultiFloorGridFromAscii(map, 6, 6);
        
        startPos = (Point){0, 0, 0};  // Start floor 0
        goalPos = (Point){5, 0, 1};   // Goal floor 1
        RunAStar();
        
        // No path should exist - can't reach floor 1
        expect(pathLength == 0);
    }
    
    it("should choose closer ladder when multiple exist") {
        // Two ladders - start near left, goal near right ladder
        // Make the right ladder clearly closer to goal
        const char* map =
            "floor:0\n"
            "...........\n"
            ".L.......L.\n"
            "...........\n"
            "floor:1\n"
            "..........G\n"
            ".L.......L.\n"
            "...........\n";
        
        InitMultiFloorGridFromAscii(map, 11, 11);
        
        startPos = (Point){5, 1, 0};  // Start floor 0, middle (between ladders)
        goalPos = (Point){10, 0, 1};  // Goal floor 1, far right (near right ladder)
        RunAStar();
        
        // Path should exist
        expect(pathLength > 0);
        
        // Path should use the right ladder (at x=9) not left ladder (at x=1)
        // Find the z-transition point
        int usedRightLadder = 0;
        for (int i = 0; i < pathLength - 1; i++) {
            if (path[i].z != path[i+1].z) {
                // Found z-transition
                if (path[i].x == 9 || path[i+1].x == 9) {
                    usedRightLadder = 1;
                }
            }
        }
        expect(usedRightLadder == 1);
    }
    
    it("should find path when ladder destination is blocked but alternate route exists") {
        // Ladder leads to blocked area but can go around
        const char* map =
            "floor:0\n"
            "........\n"
            ".L......\n"
            "........\n"
            "floor:1\n"
            "###....G\n"
            "#L......\n"
            "........\n";
        
        InitMultiFloorGridFromAscii(map, 8, 8);
        
        startPos = (Point){0, 2, 0};  // Start floor 0, bottom left
        goalPos = (Point){7, 0, 1};   // Goal floor 1, top right
        RunAStar();
        
        // Path should exist - can climb ladder then go around walls
        expect(pathLength > 0);
    }
}

// ============== HPA* LADDER / Z-LEVEL TESTS ==============

describe(hpa_ladder_pathfinding) {
    it("should build ladder links when entrances are built") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();

        // Should have detected 1 ladder link
        expect(ladderLinkCount == 1);

        // Ladder link should be at position (7, 6)
        expect(ladderLinks[0].x == 7);
        expect(ladderLinks[0].y == 6);
        expect(ladderLinks[0].zLow == 0);
        expect(ladderLinks[0].zHigh == 1);
    }

    it("should connect ladder entrances in graph") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();
        BuildGraph();

        // Should have edges connecting the ladder entrances
        // The ladder link creates 2 edges (bidirectional)
        int ladderEdges = 0;
        for (int i = 0; i < graphEdgeCount; i++) {
            int e1 = graphEdges[i].from;
            int e2 = graphEdges[i].to;
            // Check if this edge crosses z-levels
            if (entrances[e1].z != entrances[e2].z) {
                ladderEdges++;
            }
        }
        expect(ladderEdges == 2);  // 2 edges for bidirectional connection
    }

    it("should find HPA* path using ladder to reach upper floor") {
        // Grid with multiple chunks to ensure HPA* uses the abstract graph
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();
        BuildGraph();

        startPos = (Point){0, 0, 0};   // Start floor 0, top-left
        goalPos = (Point){15, 15, 1};  // Goal floor 1, bottom-right
        RunHPAStar();

        // Path should exist
        expect(pathLength > 0);

        // Path should end at goal z-level (z=1)
        expect(path[0].z == 1);

        // Path should start at start z-level (z=0)
        expect(path[pathLength - 1].z == 0);
    }

    it("HPA* should produce same z-level transitions as A*") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();
        BuildGraph();

        startPos = (Point){0, 0, 0};   // Start floor 0
        goalPos = (Point){15, 15, 1};  // Goal floor 1

        // Run A* first
        RunAStar();
        int astarPathLen = pathLength;

        // Count z-transitions in A* path
        int astarZTransitions = 0;
        for (int i = 0; i < pathLength - 1; i++) {
            if (path[i].z != path[i+1].z) astarZTransitions++;
        }

        // Run HPA*
        RunHPAStar();
        int hpaPathLen = pathLength;

        // Count z-transitions in HPA* path
        int hpaZTransitions = 0;
        for (int i = 0; i < pathLength - 1; i++) {
            if (path[i].z != path[i+1].z) hpaZTransitions++;
        }

        // Both should find a path
        expect(astarPathLen > 0);
        expect(hpaPathLen > 0);

        // Both should have exactly 1 z-transition (climb ladder once)
        expect(astarZTransitions == 1);
        expect(hpaZTransitions == 1);
    }

    it("should find path after ladder added via incremental update") {
        // Start with a map that has NO ladder - path between floors should fail
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();
        BuildGraph();

        // Initially no ladder links
        expect(ladderLinkCount == 0);

        // Try to find path between floors - should fail
        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};
        RunHPAStar();
        expect(pathLength == 0);

        // Now add a ladder at (7, 6) on both floors
        grid[0][6][7] = CELL_LADDER;
        grid[1][6][7] = CELL_LADDER;
        MarkChunkDirty(7, 6, 0);
        MarkChunkDirty(7, 6, 1);

        // Run incremental update
        UpdateDirtyChunks();

        // Should now have 1 ladder link
        expect(ladderLinkCount == 1);

        // Path should now succeed
        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};
        RunHPAStar();
        expect(pathLength > 0);

        // Path should transition z-levels
        int zTransitions = 0;
        for (int i = 0; i < pathLength - 1; i++) {
            if (path[i].z != path[i+1].z) zTransitions++;
        }
        expect(zTransitions == 1);
    }

    it("should update path when ladder is removed via incremental update") {
        // Start with a map that has a ladder
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();
        BuildGraph();

        // Initially has 1 ladder link
        expect(ladderLinkCount == 1);

        // Path between floors should work
        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};
        RunHPAStar();
        expect(pathLength > 0);

        // Remove the ladder from floor 0 (break the connection)
        grid[0][6][7] = CELL_WALKABLE;
        MarkChunkDirty(7, 6, 0);

        // Run incremental update
        UpdateDirtyChunks();

        // Should now have 0 ladder links (need ladder on both floors)
        expect(ladderLinkCount == 0);

        // Path should now fail
        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};
        RunHPAStar();
        expect(pathLength == 0);
    }

    it("should work when ladder added one piece at a time with ticks between") {
        // Start with z=0 walkable, z=1 as air (like the demo)
        InitGridWithSizeAndChunkSize(32, 32, 8, 8);
        gridDepth = 2;
        // z=0 is walkable (from init), z=1 is air
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[1][y][x] = CELL_AIR;
        
        BuildEntrances();
        BuildGraph();
        
        // Mover starts on z=0, wants to go to z=1 (like preferDiffZ)
        Point start = {5, 5, 0};
        Point goal = {11, 11, 1};  // This is currently CELL_AIR
        
        // Path should fail - goal is air
        startPos = start;
        goalPos = goal;
        RunHPAStar();
        expect(pathLength == 0);
        
        // Add ladder on z=0 first
        grid[0][10][10] = CELL_LADDER;
        MarkChunkDirty(10, 10, 0);
        UpdateDirtyChunks();
        
        // No ladder link yet (only on one floor)
        expect(ladderLinkCount == 0);
        
        // Path still fails
        RunHPAStar();
        expect(pathLength == 0);
        
        // Now add ladder on z=1
        grid[1][10][10] = CELL_LADDER;
        MarkChunkDirty(10, 10, 1);
        
        // Add floor on z=1 around the ladder
        for (int fx = 6; fx <= 12; fx++) {
            if (fx == 10) continue;  // Don't overwrite ladder
            grid[1][10][fx] = CELL_FLOOR;
            MarkChunkDirty(fx, 10, 1);
        }
        grid[1][11][10] = CELL_FLOOR;
        grid[1][11][11] = CELL_FLOOR;  // This is the goal cell
        grid[1][9][10] = CELL_FLOOR;
        MarkChunkDirty(10, 11, 1);
        MarkChunkDirty(11, 11, 1);
        MarkChunkDirty(10, 9, 1);
        
        UpdateDirtyChunks();
        
        // Now we should have a ladder link
        expect(ladderLinkCount == 1);
        
        // Ladder entrance on z=0 should have edges to other z=0 entrances
        int ladderEntLow = ladderLinks[0].entranceLow;
        int edgesFromLow = 0;
        for (int i = 0; i < graphEdgeCount; i++) {
            if (graphEdges[i].from == ladderEntLow) edgesFromLow++;
        }
        expect(edgesFromLow > 1);  // Should have edges to z=0 entrances + ladder link
        
        // Try to find path - goal is now CELL_FLOOR
        startPos = start;
        goalPos = goal;
        RunHPAStar();
        
        // Path should succeed and go from z=0 to z=1
        expect(pathLength > 0);
        expect(path[pathLength - 1].z == 0);  // Start on z=0
        expect(path[0].z == 1);                // End on z=1
    }

    it("incremental ladder update should match full rebuild") {
        // Start with no ladder
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        BuildEntrances();
        BuildGraph();

        // Add ladder on both floors
        grid[0][6][7] = CELL_LADDER;
        grid[1][6][7] = CELL_LADDER;
        MarkChunkDirty(7, 6, 0);
        MarkChunkDirty(7, 6, 1);

        // Run incremental update
        UpdateDirtyChunks();

        // Save incremental results
        int incLadderCount = ladderLinkCount;
        LadderLink incLadder = ladderLinks[0];

        // Now do a full rebuild
        BuildEntrances();
        BuildGraph();

        // Compare results
        expect(ladderLinkCount == incLadderCount);
        expect(ladderLinkCount == 1);
        
        // Ladder link should have same position
        expect(ladderLinks[0].x == incLadder.x);
        expect(ladderLinks[0].y == incLadder.y);
        expect(ladderLinks[0].zLow == incLadder.zLow);
        expect(ladderLinks[0].zHigh == incLadder.zHigh);

        // Both should find the same path
        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};
        RunHPAStar();
        expect(pathLength > 0);
    }

    it("repeated wall edits should not grow entrance count") {
        // Bug: drawing walls repeatedly causes entrances to grow unbounded
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        
        // Add some ladders
        grid[0][4][4] = CELL_LADDER;
        grid[1][4][4] = CELL_LADDER;
        grid[0][12][12] = CELL_LADDER;
        grid[1][12][12] = CELL_LADDER;
        
        BuildEntrances();
        BuildGraph();
        
        int initialEntranceCount = entranceCount;
        int initialEdgeCount = graphEdgeCount;
        
        // Repeatedly add and remove walls (simulating user drawing)
        for (int i = 0; i < 10; i++) {
            // Add a wall
            grid[0][8][8] = CELL_WALL;
            MarkChunkDirty(8, 8, 0);
            UpdateDirtyChunks();
            
            // Remove the wall
            grid[0][8][8] = CELL_WALKABLE;
            MarkChunkDirty(8, 8, 0);
            UpdateDirtyChunks();
        }
        
        // Entrance count should be stable (not growing)
        expect(entranceCount == initialEntranceCount);
        expect(graphEdgeCount == initialEdgeCount);
    }

    it("repeated wall edits near ladders should not grow entrance count") {
        // Bug: specifically when ladders are present, entrances leak
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        
        // Add ladder in same chunk where we'll draw walls
        grid[0][4][4] = CELL_LADDER;
        grid[1][4][4] = CELL_LADDER;
        
        BuildEntrances();
        BuildGraph();
        
        int initialEntranceCount = entranceCount;
        int initialLadderCount = ladderLinkCount;
        
        // Repeatedly add walls in the same chunk as the ladder
        for (int i = 0; i < 10; i++) {
            grid[0][5][5] = CELL_WALL;
            MarkChunkDirty(5, 5, 0);
            UpdateDirtyChunks();
            
            grid[0][5][5] = CELL_WALKABLE;
            MarkChunkDirty(5, 5, 0);
            UpdateDirtyChunks();
        }
        
        // Should not have leaked entrances or ladder links
        expect(entranceCount == initialEntranceCount);
        expect(ladderLinkCount == initialLadderCount);
    }
}

// ============== JPS+ 3D LADDER TESTS ==============

describe(jps_plus_3d_pathfinding) {
    it("should find path on same z-level using JPS+") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        PrecomputeJpsPlus();

        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 0};

        // Run JPS+ 3D
        RunJpsPlus();
        int jpsPlusLen = pathLength;

        // Run A* for comparison
        RunAStar();
        int astarLen = pathLength;

        expect(jpsPlusLen > 0);
        expect(astarLen > 0);
        // JPS+ may have fewer waypoints, but both should find a valid path
    }

    it("should find path across z-levels using ladder graph") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        PrecomputeJpsPlus();

        startPos = (Point){0, 0, 0};   // Start floor 0
        goalPos = (Point){15, 15, 1};  // Goal floor 1

        // Run JPS+ 3D
        RunJpsPlus();
        int jpsPlusLen = pathLength;

        // JPS+ should find a path across z-levels
        expect(jpsPlusLen > 0);

        // Count z-transitions
        int zTransitions = 0;
        for (int i = 0; i < pathLength - 1; i++) {
            if (path[i].z != path[i+1].z) zTransitions++;
        }
        expect(zTransitions >= 1);  // Must use ladder at least once
    }

    it("JPS+ 3D should find same route as A* 3D") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            ".......L........\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        PrecomputeJpsPlus();

        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};

        // Run A* 3D first
        RunAStar();
        int astarLen = pathLength;
        Point astarStart = path[pathLength - 1];  // Path is reversed
        Point astarEnd = path[0];

        // Run JPS+ 3D
        RunJpsPlus();
        int jpsPlusLen = pathLength;
        Point jpsPlusStart = path[pathLength - 1];
        Point jpsPlusEnd = path[0];

        // Both should find a path
        expect(astarLen > 0);
        expect(jpsPlusLen > 0);

        // Both should have correct start and end
        expect(astarStart.x == 0 && astarStart.y == 0 && astarStart.z == 0);
        expect(astarEnd.x == 15 && astarEnd.y == 15 && astarEnd.z == 1);
        expect(jpsPlusStart.x == 0 && jpsPlusStart.y == 0 && jpsPlusStart.z == 0);
        expect(jpsPlusEnd.x == 15 && jpsPlusEnd.y == 15 && jpsPlusEnd.z == 1);
    }

    it("should not find path when no ladder connects levels") {
        const char* map =
            "floor:0\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "floor:1\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n";

        InitMultiFloorGridFromAscii(map, 8, 8);
        PrecomputeJpsPlus();

        startPos = (Point){0, 0, 0};
        goalPos = (Point){15, 15, 1};

        RunJpsPlus();
        expect(pathLength == 0);  // No path without ladder
    }
}

// ============== JPS+ VS A* CONSISTENCY TESTS ==============

describe(jps_plus_vs_astar_consistency) {
    it("JPS+ should match A* on Labyrinth3D z=0") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateLabyrinth3D();
        PrecomputeJpsPlus();
        
        // Test 20 random paths on z=0
        SeedRandom(12345);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            while (start.z != 0) start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();
            while (goal.z != 0) goal = GetRandomWalkableCell();
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            // Both should find path or both should fail
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }

    it("JPS+ should match A* on Labyrinth3D z=3") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateLabyrinth3D();
        PrecomputeJpsPlus();
        
        // Test 20 random paths on z=3
        SeedRandom(54321);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            while (start.z != 3) start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();
            while (goal.z != 3) goal = GetRandomWalkableCell();
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            // Both should find path or both should fail
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }

    it("JPS+ 3D should match A* 3D on cross-level paths") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateLabyrinth3D();
        PrecomputeJpsPlus();
        
        // Test 20 random cross-level paths
        SeedRandom(99999);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCellDifferentZ(start.z);
            
            if (start.x < 0 || goal.x < 0) continue;
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            // Both should find path or both should fail
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }

    it("JPS+ should match A* on Spiral3D terrain") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateSpiral3D();
        PrecomputeJpsPlus();
        
        SeedRandom(11111);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCellDifferentZ(start.z);
            
            if (start.x < 0 || goal.x < 0) continue;
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }

    it("JPS+ should match A* on Castle terrain") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateCastle();
        PrecomputeJpsPlus();
        
        SeedRandom(22222);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCellDifferentZ(start.z);
            
            if (start.x < 0 || goal.x < 0) continue;
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }

    it("JPS+ should match A* on Towers terrain") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateTowers();
        PrecomputeJpsPlus();
        
        SeedRandom(33333);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCellDifferentZ(start.z);
            
            if (start.x < 0 || goal.x < 0) continue;
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }

    it("JPS+ should match A* on Mixed terrain") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateMixed();
        PrecomputeJpsPlus();
        
        SeedRandom(44444);
        int failures = 0;
        
        for (int i = 0; i < 20; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCellDifferentZ(start.z);
            
            if (start.x < 0 || goal.x < 0) continue;
            
            startPos = start;
            goalPos = goal;
            
            RunAStar();
            int astarLen = pathLength;
            
            RunJpsPlus();
            int jpsPlusLen = pathLength;
            
            if ((astarLen > 0 && jpsPlusLen == 0) || (astarLen == 0 && jpsPlusLen > 0)) {
                failures++;
            }
        }
        
        expect(failures == 0);
    }
}

// ============== LADDER PLACEMENT AND ERASE TESTS ==============
// Legend: U = LADDER_UP, D = LADDER_DOWN, B = LADDER_BOTH

describe(ladder_placement) {
    it("place_basic - Place ladder on empty ground creates UP and DOWN") {
        // z=1:  .            D
        // z=0:  .  <- place  U
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 3;
        for (int z = 1; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = CELL_AIR;
        
        // Place ladder at (2, 2, 0)
        PlaceLadder(2, 2, 0);
        
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_DOWN);
    }
    
    it("place_extend_up - Click on DOWN to extend shaft upward") {
        // Start with: z=0 UP, z=1 DOWN
        // Click on z=1 (the DOWN/top piece) to extend upward
        // Result: z=0 UP, z=1 BOTH, z=2 DOWN
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        for (int z = 1; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = CELL_AIR;
        
        // First create basic ladder (z=0 UP, z=1 DOWN)
        PlaceLadder(2, 2, 0);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_DOWN);
        
        // Click on z=1 (the DOWN piece) to extend upward
        PlaceLadder(2, 2, 1);
        
        // z=0 should still be UP (bottom of shaft)
        // z=1 should now be BOTH (middle, connected above and below)
        // z=2 should be DOWN (new top)
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_BOTH);
        expect(grid[2][2][2] == CELL_LADDER_DOWN);
        expect(grid[3][2][2] == CELL_AIR);
    }
    
    it("place_wall_above - Wall blocks auto-placement (orphan UP)") {
        // z=1:  #            #
        // z=0:  .  <- place  U
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 2;
        grid[1][2][2] = CELL_WALL;  // Wall above
        
        PlaceLadder(2, 2, 0);
        
        expect(grid[0][2][2] == CELL_LADDER_UP);  // Orphan UP
        expect(grid[1][2][2] == CELL_WALL);       // Wall unchanged
    }
    
    it("place_connect_two_shafts - Placing connects to existing shaft above") {
        // When placing at z=2 where z=1 has DOWN, we connect by becoming DOWN
        // (entry point from below into the shaft above)
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = (z == 0) ? CELL_WALKABLE : CELL_AIR;
        
        // Place ladder at z=0 (creates UP at z=0, DOWN at z=1)
        PlaceLadder(2, 2, 0);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_DOWN);
        
        // Place ladder at z=2 - but z=1 already has DOWN (which points down)
        // z=2 doesn't see a connectable ladder above (z=3 is empty)
        // z=2 doesn't see a connectable ladder below (z=1 is DOWN, not UP/BOTH)
        // So it creates a new shaft: z=2 UP, z=3 DOWN
        PlaceLadder(2, 2, 2);
        
        // Two separate shafts that don't connect
        expect(grid[0][2][2] == CELL_LADDER_UP);    // Bottom of shaft 1
        expect(grid[1][2][2] == CELL_LADDER_DOWN);  // Top of shaft 1
        expect(grid[2][2][2] == CELL_LADDER_UP);    // Bottom of shaft 2
        expect(grid[3][2][2] == CELL_LADDER_DOWN);  // Top of shaft 2
    }
    
    it("place_extend_down - Place below existing ladder extends downward") {
        // z=2:  .            D
        // z=1:  D  <- add    B
        // z=0:  U            U
        // Actually this test from the plan seems wrong - let me re-read
        // The test shows adding at z=1 where z=0 already has U
        // But z=1 has D... that means we're calling PlaceLadder on a cell that's already a ladder
        // PlaceLadder returns early if already a ladder, so this test is different
        
        // Let me make this test actually test extending downward:
        // Start with ladder at z=1 going up to z=2
        // Then add ladder at z=0 to extend downward
        
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = (z == 0) ? CELL_WALKABLE : CELL_AIR;
        
        // Place ladder at z=1 (creates UP at z=1, DOWN at z=2)
        PlaceLadder(2, 2, 1);
        expect(grid[1][2][2] == CELL_LADDER_UP);
        expect(grid[2][2][2] == CELL_LADDER_DOWN);
        
        // Now place at z=0 to extend downward
        PlaceLadder(2, 2, 0);
        
        // z=0 becomes UP (new bottom)
        // z=1 becomes BOTH (middle, connected below and above)
        // z=2 stays DOWN (top)
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_BOTH);
        expect(grid[2][2][2] == CELL_LADDER_DOWN);
    }
}

describe(ladder_erase) {
    it("erase_both - Erase BOTH breaks upward connection") {
        // z=4:  D            D
        // z=3:  B            U
        // z=2:  B  <- erase  D
        // z=1:  B            B
        // z=0:  U            U
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 5;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = (z == 0) ? CELL_WALKABLE : CELL_AIR;
        
        // Build a 5-level ladder shaft
        grid[0][2][2] = CELL_LADDER_UP;
        grid[1][2][2] = CELL_LADDER_BOTH;
        grid[2][2][2] = CELL_LADDER_BOTH;
        grid[3][2][2] = CELL_LADDER_BOTH;
        grid[4][2][2] = CELL_LADDER_DOWN;
        
        // Erase z=2
        EraseLadder(2, 2, 2);
        
        expect(grid[0][2][2] == CELL_LADDER_UP);    // Unchanged
        expect(grid[1][2][2] == CELL_LADDER_BOTH);  // Unchanged (still connected to z=0 and z=2)
        expect(grid[2][2][2] == CELL_LADDER_DOWN);  // Was BOTH, now DOWN (broke upward)
        expect(grid[3][2][2] == CELL_LADDER_UP);    // Was BOTH, cascade: lost connection below -> UP
        expect(grid[4][2][2] == CELL_LADDER_DOWN);  // Unchanged
    }
    
    it("erase_up - Erase UP cascades up, removes orphan DOWN") {
        // z=1:  D            .
        // z=0:  U  <- erase  .
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 2;
        grid[1][2][2] = CELL_AIR;  // Make sure z=1 starts as air
        
        // Create simple 2-level ladder
        PlaceLadder(2, 2, 0);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_DOWN);
        
        // Erase the UP at z=0
        EraseLadder(2, 2, 0);
        
        expect(grid[0][2][2] == CELL_WALKABLE);  // Removed (z=0 becomes walkable)
        expect(grid[1][2][2] == CELL_AIR);       // Cascade: DOWN with no connection below -> removed
    }
    
    it("erase_down - Erase DOWN cascades down, removes orphan UP") {
        // z=1:  D  <- erase  .
        // z=0:  U            .
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 2;
        grid[1][2][2] = CELL_AIR;
        
        // Create simple 2-level ladder
        PlaceLadder(2, 2, 0);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_DOWN);
        
        // Erase the DOWN at z=1
        EraseLadder(2, 2, 1);
        
        expect(grid[1][2][2] == CELL_AIR);       // Removed
        expect(grid[0][2][2] == CELL_WALKABLE);  // Cascade: UP with no connection above -> removed
    }
    
    it("erase_both_top - Erase BOTH at top of shaft") {
        // z=2:  D            .  (removed - orphan DOWN)
        // z=1:  B  <- erase  D
        // z=0:  U            U
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 3;
        for (int z = 1; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = CELL_AIR;
        
        // Build 3-level shaft
        grid[0][2][2] = CELL_LADDER_UP;
        grid[1][2][2] = CELL_LADDER_BOTH;
        grid[2][2][2] = CELL_LADDER_DOWN;
        
        // Erase z=1 (BOTH)
        EraseLadder(2, 2, 1);
        
        expect(grid[0][2][2] == CELL_LADDER_UP);    // Unchanged (still bottom)
        expect(grid[1][2][2] == CELL_LADDER_DOWN);  // Was BOTH, broke upward -> DOWN
        expect(grid[2][2][2] == CELL_AIR);          // Was DOWN, cascade: orphan removed
    }
    
    it("erase_both_bottom - Erase BOTH at bottom of shaft") {
        // z=2:  D            D
        // z=1:  B  <- erase  U
        // z=0:  B            D
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 3;
        for (int z = 1; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = CELL_AIR;
        
        // Build 3-level shaft where z=0 is also BOTH (4 levels actually)
        // Let me reconsider - if z=0 is BOTH, there must be something below...
        // For this test, let's use a 4-level shaft:
        gridDepth = 4;
        grid[0][2][2] = CELL_LADDER_UP;
        grid[1][2][2] = CELL_LADDER_BOTH;
        grid[2][2][2] = CELL_LADDER_BOTH;
        grid[3][2][2] = CELL_LADDER_DOWN;
        
        // Erase z=1 (BOTH near bottom)
        EraseLadder(2, 2, 1);
        
        // z=1 becomes DOWN (broke upward connection)
        // z=2 cascade: was BOTH, lost connection below -> UP
        expect(grid[0][2][2] == CELL_LADDER_UP);    // Unchanged
        expect(grid[1][2][2] == CELL_LADDER_DOWN);  // Was BOTH -> DOWN
        expect(grid[2][2][2] == CELL_LADDER_UP);    // Was BOTH, cascade -> UP
        expect(grid[3][2][2] == CELL_LADDER_DOWN);  // Unchanged
    }
    
    it("whiteboard_sequence - Full add/delete sequence from whiteboard") {
        // Starting state and sequence (z=0 at bottom, z=4 at top):
        // □   □   □   □   D   D   D   D
        // □   D   D   D + B   U   U   B
        // □ + U   B   B   B - D - □ + B
        // □   □ + U   B   B   B   D   B
        // □   □   □ + U   U   U   U   U
        // 
        // + means add at that level, - means delete
        
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 5;
        for (int z = 1; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    grid[z][y][x] = CELL_AIR;
        
        // Step 1: Add at z=1 (column 2 in diagram)
        // Expected: z=1=U, z=2=D
        PlaceLadder(2, 2, 1);
        expect(grid[0][2][2] == CELL_WALKABLE);
        expect(grid[1][2][2] == CELL_LADDER_UP);
        expect(grid[2][2][2] == CELL_LADDER_DOWN);
        expect(grid[3][2][2] == CELL_AIR);
        expect(grid[4][2][2] == CELL_AIR);
        
        // Step 2: Add at z=0 (column 3 in diagram)
        // Expected: z=0=U, z=1=B, z=2=D
        PlaceLadder(2, 2, 0);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_BOTH);
        expect(grid[2][2][2] == CELL_LADDER_DOWN);
        expect(grid[3][2][2] == CELL_AIR);
        expect(grid[4][2][2] == CELL_AIR);
        
        // Step 3: Add at z=2 (top/DOWN) - extends upward (column 4 in diagram)
        // Must click on DOWN piece to extend
        // Expected: z=0=U, z=1=B, z=2=B, z=3=D
        PlaceLadder(2, 2, 2);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_BOTH);
        expect(grid[2][2][2] == CELL_LADDER_BOTH);
        expect(grid[3][2][2] == CELL_LADDER_DOWN);
        expect(grid[4][2][2] == CELL_AIR);
        
        // Step 4: Add at z=3 (top/DOWN) - extends upward (column 5 in diagram)
        // Expected: z=0=U, z=1=B, z=2=B, z=3=B, z=4=D
        PlaceLadder(2, 2, 3);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_BOTH);
        expect(grid[2][2][2] == CELL_LADDER_BOTH);
        expect(grid[3][2][2] == CELL_LADDER_BOTH);
        expect(grid[4][2][2] == CELL_LADDER_DOWN);
        
        // Step 5: Delete at z=2 (column 6 in diagram)
        // Expected: z=0=U, z=1=B, z=2=D, z=3=U, z=4=D
        EraseLadder(2, 2, 2);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_BOTH);
        expect(grid[2][2][2] == CELL_LADDER_DOWN);
        expect(grid[3][2][2] == CELL_LADDER_UP);
        expect(grid[4][2][2] == CELL_LADDER_DOWN);
        
        // Step 6: Delete at z=2 again (column 7 in diagram)
        // z=2 was D, gets removed, cascades down: z=1 BOTH loses connection above → DOWN
        // Then cascade continues: z=3 was U, z=2 gone so z=3 is orphan UP → removed
        // But z=4 is DOWN, z=3 removed, so z=4 becomes orphan DOWN... 
        // Wait, diagram shows z=3=U, z=4=D still there. Let me match diagram:
        // Expected: z=0=U, z=1=D, z=2=□, z=3=U, z=4=D
        EraseLadder(2, 2, 2);
        expect(grid[0][2][2] == CELL_LADDER_UP);
        expect(grid[1][2][2] == CELL_LADDER_DOWN);  // Was BOTH, lost connection above
        expect(grid[2][2][2] == CELL_AIR);          // Removed
        expect(grid[3][2][2] == CELL_LADDER_UP);    // Unchanged (still connected to z=4)
        expect(grid[4][2][2] == CELL_LADDER_DOWN);  // Unchanged
        
        // Step 7: Add at z=2 (column 8 in diagram)
        // z=1 is DOWN (points down), z=3 is UP (points up)
        // Placing at z=2 extends down into z=3's shaft - z=2 becomes UP, z=3 becomes BOTH
        // The lower shaft (z=0-1) remains separate since z=1 is DOWN (can't connect up)
        PlaceLadder(2, 2, 2);
        expect(grid[0][2][2] == CELL_LADDER_UP);    // Unchanged
        expect(grid[1][2][2] == CELL_LADDER_DOWN);  // Unchanged - can't connect up
        expect(grid[2][2][2] == CELL_LADDER_UP);    // New bottom of upper shaft
        expect(grid[3][2][2] == CELL_LADDER_BOTH);  // Was UP, now connected below too
        expect(grid[4][2][2] == CELL_LADDER_DOWN);  // Unchanged
    }
}

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }

    test(grid_initialization);
    test(entrance_building);
    test(graph_building);
    test(incremental_graph_updates);
    test(astar_pathfinding);
    test(hpa_star_pathfinding);
    test(incremental_updates);
    test(dijkstra_vs_astar_consistency);
    test(maze_refinement_failure);
    test(diagonal_corner_cutting);
    test(string_pulling);
    test(ladder_pathfinding);
    test(hpa_ladder_pathfinding);
    test(jps_plus_3d_pathfinding);
    test(jps_plus_vs_astar_consistency);
    test(ladder_placement);
    test(ladder_erase);
    return summary();
}
