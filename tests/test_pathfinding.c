#include "c89spec.h"
#include "raylib.h"
#include "../hpa-star-tests/grid.h"
#include "../hpa-star-tests/terrain.h"
#include "../hpa-star-tests/pathfinding.h"

// Test grid size - 4x4 chunks = 128x128 cells
#define TEST_GRID_SIZE (CHUNK_SIZE * 4)

describe(grid_initialization) {
    it("should initialize grid to all walkable cells") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        int allWalkable = 1;
        for (int y = 0; y < gridHeight && allWalkable; y++)
            for (int x = 0; x < gridWidth && allWalkable; x++)
                if (grid[y][x] != CELL_WALKABLE) allWalkable = 0;
        expect(allWalkable == 1);
    }

    it("should mark chunks as dirty when walls are placed") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        // Clear dirty flags
        for (int cy = 0; cy < chunksY; cy++)
            for (int cx = 0; cx < chunksX; cx++)
                chunkDirty[cy][cx] = false;
        needsRebuild = false;
        
        // Place a wall and mark dirty
        grid[10][10] = CELL_WALL;
        MarkChunkDirty(10, 10);
        
        int cx = 10 / CHUNK_SIZE;
        int cy = 10 / CHUNK_SIZE;
        expect(chunkDirty[cy][cx] == true && needsRebuild == true);
    }
}

describe(entrance_building) {
    it("should create entrances on chunk borders") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();
        expect(entranceCount > 0);
    }

    it("should not create entrances where walls block the border") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        // Block the entire first horizontal border
        int borderY = CHUNK_SIZE;
        for (int x = 0; x < gridWidth; x++) {
            grid[borderY - 1][x] = CELL_WALL;
        }
        BuildEntrances();
        // Check no entrances at y=borderY
        int entrancesAtBorder = 0;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].y == borderY) entrancesAtBorder++;
        }
        expect(entrancesAtBorder == 0);
    }
}

describe(graph_building) {
    it("should create edges between entrances in the same chunk") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();
        BuildGraph();
        expect(graphEdgeCount > 0);
    }
}

describe(astar_pathfinding) {
    it("should find a path on an empty grid") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        startPos = (Point){5, 5};
        goalPos = (Point){50, 50};
        RunAStar();
        expect(pathLength > 0);
    }

    it("should not find a path when goal is walled off") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        // Create a box around the goal
        int gx = 50, gy = 50;
        for (int x = gx - 2; x <= gx + 2; x++) {
            grid[gy - 2][x] = CELL_WALL;
            grid[gy + 2][x] = CELL_WALL;
        }
        for (int y = gy - 2; y <= gy + 2; y++) {
            grid[y][gx - 2] = CELL_WALL;
            grid[y][gx + 2] = CELL_WALL;
        }
        startPos = (Point){5, 5};
        goalPos = (Point){gx, gy};
        RunAStar();
        expect(pathLength == 0);
    }
}

describe(hpa_star_pathfinding) {
    it("should find a path using HPA* on an empty grid") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();
        BuildGraph();
        startPos = (Point){5, 5};
        goalPos = (Point){TEST_GRID_SIZE - 10, TEST_GRID_SIZE - 10};
        RunHPAStar();
        expect(pathLength > 0);
    }

    it("should find same-chunk paths without using the graph") {
        InitGridWithSize(TEST_GRID_SIZE, TEST_GRID_SIZE);
        BuildEntrances();
        BuildGraph();
        // Start and goal in same chunk
        startPos = (Point){5, 5};
        goalPos = (Point){10, 10};
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
        grid[CHUNK_SIZE + 5][CHUNK_SIZE + 5] = CELL_WALL;
        MarkChunkDirty(CHUNK_SIZE + 5, CHUNK_SIZE + 5);
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
            grid[CHUNK_SIZE + 10][CHUNK_SIZE + i] = CELL_WALL;
            MarkChunkDirty(CHUNK_SIZE + i, CHUNK_SIZE + 10);
        }
        UpdateDirtyChunks();
        
        startPos = (Point){5, 5};
        goalPos = (Point){TEST_GRID_SIZE - 10, TEST_GRID_SIZE - 10};
        RunHPAStar();
        expect(pathLength > 0);
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
    test(astar_pathfinding);
    test(hpa_star_pathfinding);
    test(incremental_updates);
    return summary();
}
