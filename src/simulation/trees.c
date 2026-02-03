// trees.c - Tree growth cellular automaton
// Saplings grow into trunks, trunks grow upward, top spawns leaves

#include "trees.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <stdlib.h>

// Growth parameters
#define SAPLING_GROW_TICKS 100    // Ticks before sapling becomes trunk
#define TRUNK_GROW_TICKS 50       // Ticks between trunk growing upward
#define LEAF_SPREAD_TICKS 20      // Ticks between leaf spread iterations
#define LEAF_DECAY_TICKS 30       // Ticks before orphan leaf decays
#define MAX_TREE_HEIGHT 6         // Maximum trunk height
#define LEAF_SPREAD_RADIUS 2      // How far leaves spread from trunk top
#define LEAF_TRUNK_CHECK_DIST 4   // Max distance to check for trunk connection

// Simple timer grid for growth (could be optimized with a list later)
static int growthTimer[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

void InitTrees(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                growthTimer[z][y][x] = 0;
            }
        }
    }
}

// Check if a leaf cell is connected to a trunk within distance
static bool IsConnectedToTrunk(int x, int y, int z, int maxDist) {
    // Simple BFS to find trunk
    // For now, just check directly below and adjacent
    for (int checkZ = z; checkZ >= 0 && checkZ >= z - maxDist; checkZ--) {
        // Check this column
        if (grid[checkZ][y][x] == CELL_TREE_TRUNK) return true;
        
        // Check adjacent at same z
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                if (grid[checkZ][ny][nx] == CELL_TREE_TRUNK) return true;
            }
        }
    }
    return false;
}

// Spawn leaves around top of trunk
static void SpawnLeavesAroundTrunk(int x, int y, int z) {
    // Spawn leaves in a cross/plus pattern around and above the trunk top
    int leafZ = z + 1;
    if (leafZ >= gridDepth) leafZ = z;
    
    // Above trunk
    if (leafZ < gridDepth && grid[leafZ][y][x] == CELL_AIR) {
        grid[leafZ][y][x] = CELL_TREE_LEAVES;
        MarkChunkDirty(x, y, leafZ);
    }
    
    // Adjacent at same level as top trunk
    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
        if (grid[z][ny][nx] == CELL_AIR) {
            grid[z][ny][nx] = CELL_TREE_LEAVES;
            MarkChunkDirty(nx, ny, z);
        }
        // Also one level up
        if (leafZ < gridDepth && grid[leafZ][ny][nx] == CELL_AIR) {
            grid[leafZ][ny][nx] = CELL_TREE_LEAVES;
            MarkChunkDirty(nx, ny, leafZ);
        }
    }
    
    // Diagonals at leaf level only
    int ddx[] = {1, 1, -1, -1};
    int ddy[] = {1, -1, 1, -1};
    for (int i = 0; i < 4; i++) {
        int nx = x + ddx[i];
        int ny = y + ddy[i];
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
        if (leafZ < gridDepth && grid[leafZ][ny][nx] == CELL_AIR) {
            grid[leafZ][ny][nx] = CELL_TREE_LEAVES;
            MarkChunkDirty(nx, ny, leafZ);
        }
    }
}

// Get the height of trunk at a position (counting downward from z)
static int GetTrunkHeight(int x, int y, int z) {
    int height = 0;
    for (int checkZ = z; checkZ >= 0; checkZ--) {
        if (grid[checkZ][y][x] == CELL_TREE_TRUNK) {
            height++;
        } else {
            break;
        }
    }
    return height;
}

// Single growth tick for one cell
static void GrowCell(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    
    if (cell == CELL_SAPLING) {
        // Sapling grows into trunk
        grid[z][y][x] = CELL_TREE_TRUNK;
        MarkChunkDirty(x, y, z);
        growthTimer[z][y][x] = 0;
        
    } else if (cell == CELL_TREE_TRUNK) {
        // Check if we should grow upward
        int height = GetTrunkHeight(x, y, z);
        
        if (height < MAX_TREE_HEIGHT && z + 1 < gridDepth) {
            CellType above = grid[z + 1][y][x];
            if (above == CELL_AIR || above == CELL_TREE_LEAVES) {
                // Grow trunk upward
                grid[z + 1][y][x] = CELL_TREE_TRUNK;
                MarkChunkDirty(x, y, z + 1);
                growthTimer[z + 1][y][x] = 0;
            }
        } else {
            // Reached max height or blocked - spawn leaves
            SpawnLeavesAroundTrunk(x, y, z);
        }
        
    } else if (cell == CELL_TREE_LEAVES) {
        // Check if still connected to trunk
        if (!IsConnectedToTrunk(x, y, z, LEAF_TRUNK_CHECK_DIST)) {
            // Decay - turn to air
            grid[z][y][x] = CELL_AIR;
            MarkChunkDirty(x, y, z);
        }
    }
}

// Run one tick of tree growth simulation
void TreesTick(float dt) {
    (void)dt;  // Currently tick-based, not time-based
    
    // Update timers and trigger growth
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];
                
                if (cell == CELL_SAPLING) {
                    growthTimer[z][y][x]++;
                    if (growthTimer[z][y][x] >= SAPLING_GROW_TICKS) {
                        GrowCell(x, y, z);
                    }
                } else if (cell == CELL_TREE_TRUNK) {
                    // Only grow from topmost trunk in a column
                    if (z + 1 >= gridDepth || grid[z + 1][y][x] != CELL_TREE_TRUNK) {
                        growthTimer[z][y][x]++;
                        if (growthTimer[z][y][x] >= TRUNK_GROW_TICKS) {
                            GrowCell(x, y, z);
                            growthTimer[z][y][x] = 0;
                        }
                    }
                } else if (cell == CELL_TREE_LEAVES) {
                    growthTimer[z][y][x]++;
                    if (growthTimer[z][y][x] >= LEAF_DECAY_TICKS) {
                        GrowCell(x, y, z);  // Will check connection and decay if orphan
                        growthTimer[z][y][x] = 0;
                    }
                }
            }
        }
    }
}

// Instantly grow a full tree from a sapling position
// Runs growth ticks until tree is mature
void TreeGrowFull(int x, int y, int z) {
    // Place sapling if not already there
    if (grid[z][y][x] != CELL_SAPLING && grid[z][y][x] != CELL_TREE_TRUNK) {
        grid[z][y][x] = CELL_SAPLING;
        MarkChunkDirty(x, y, z);
    }
    
    // Grow sapling to trunk
    GrowCell(x, y, z);
    
    // Grow trunk upward until max height or blocked
    int currentZ = z;
    for (int i = 0; i < MAX_TREE_HEIGHT; i++) {
        if (currentZ + 1 >= gridDepth) break;
        if (grid[currentZ][y][x] != CELL_TREE_TRUNK) break;
        
        CellType above = grid[currentZ + 1][y][x];
        if (above != CELL_AIR && above != CELL_TREE_LEAVES) break;
        
        // Grow upward
        grid[currentZ + 1][y][x] = CELL_TREE_TRUNK;
        MarkChunkDirty(x, y, currentZ + 1);
        currentZ++;
    }
    
    // Spawn leaves at top
    SpawnLeavesAroundTrunk(x, y, currentZ);
}

// Place a sapling that will grow over time
void PlaceSapling(int x, int y, int z) {
    if (grid[z][y][x] != CELL_AIR && grid[z][y][x] != CELL_DIRT) return;
    
    // Need solid ground below in DF mode
    if (z > 0 && !CellIsSolid(grid[z - 1][y][x])) return;
    
    grid[z][y][x] = CELL_SAPLING;
    growthTimer[z][y][x] = 0;
    MarkChunkDirty(x, y, z);
}
