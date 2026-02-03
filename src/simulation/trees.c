// trees.c - Tree growth cellular automaton
// Saplings grow into trunks, trunks grow upward, top spawns leaves

#include "trees.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../entities/items.h"
#include <stdlib.h>

// Growth parameters - runtime configurable
int saplingGrowTicks = 100;       // Ticks before sapling becomes trunk
int trunkGrowTicks = 50;          // Ticks between trunk growing upward

// Growth parameters - compile-time constants
#define LEAF_SPREAD_TICKS 20      // Ticks between leaf spread iterations
#define LEAF_DECAY_TICKS 30       // Ticks before orphan leaf decays
#define MIN_TREE_HEIGHT 3         // Minimum trunk height
#define MAX_TREE_HEIGHT 6         // Maximum trunk height
#define LEAF_SPREAD_RADIUS 2      // How far leaves spread from trunk top
#define LEAF_TRUNK_CHECK_DIST 4   // Max distance to check for trunk connection

// Simple timer grid for growth (could be optimized with a list later)
static int growthTimer[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Target height per tree (set when sapling becomes trunk, based on position)
static int targetHeight[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Simple hash for position-based randomness (deterministic)
static unsigned int PositionHash(int x, int y, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + z * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

void InitTrees(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                growthTimer[z][y][x] = 0;
                targetHeight[z][y][x] = 0;
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

// Spawn oak-style canopy around top of trunk
// Uses position-based randomness for organic variation
static void SpawnLeavesAroundTrunk(int x, int y, int z) {
    unsigned int hash = PositionHash(x, y, z);
    
    // Determine canopy size based on tree height and randomness
    // Taller trees get bigger canopies
    int trunkHeight = GetTrunkHeight(x, y, z);
    int baseRadius = 1 + (trunkHeight / 2);  // 1-3 based on height
    if (baseRadius > 3) baseRadius = 3;
    
    // Add some random variation to radius
    int radiusVariation = (hash % 2);  // 0 or 1
    int canopyRadius = baseRadius + radiusVariation;
    
    // Canopy extends 1-2 levels above trunk top
    int canopyLevels = 1 + (hash >> 4) % 2;  // 1 or 2 levels
    
    // Build canopy from top down for oak shape (wider at top)
    for (int level = 0; level <= canopyLevels; level++) {
        int leafZ = z + 1 + level;
        if (leafZ >= gridDepth) continue;
        
        // Radius shrinks slightly at very top for rounded look
        int levelRadius = canopyRadius;
        if (level == canopyLevels) levelRadius = canopyRadius - 1;
        if (levelRadius < 1) levelRadius = 1;
        
        // Fill leaves in a rough circle
        for (int dy = -levelRadius; dy <= levelRadius; dy++) {
            for (int dx = -levelRadius; dx <= levelRadius; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                
                // Use circular distance check with some randomness for organic edge
                int distSq = dx * dx + dy * dy;
                int maxDistSq = levelRadius * levelRadius;
                
                // Allow some cells just outside radius based on position hash
                unsigned int cellHash = PositionHash(nx, ny, leafZ);
                bool allowExtra = (cellHash % 4) == 0;  // 25% chance for edge cells
                
                if (distSq <= maxDistSq || (distSq <= maxDistSq + 2 && allowExtra)) {
                    if (grid[leafZ][ny][nx] == CELL_AIR) {
                        grid[leafZ][ny][nx] = CELL_TREE_LEAVES;
                        MarkChunkDirty(nx, ny, leafZ);
                    }
                }
            }
        }
    }
    
    // Also add leaves at trunk top level (z) around the trunk
    // This creates the "skirt" of the canopy
    int skirtRadius = canopyRadius;
    for (int dy = -skirtRadius; dy <= skirtRadius; dy++) {
        for (int dx = -skirtRadius; dx <= skirtRadius; dx++) {
            if (dx == 0 && dy == 0) continue;  // Skip trunk position
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            
            int distSq = dx * dx + dy * dy;
            int maxDistSq = skirtRadius * skirtRadius;
            
            // Skirt is sparser than top - only ~60% fill
            unsigned int cellHash = PositionHash(nx, ny, z);
            bool skip = (cellHash % 5) < 2;  // 40% chance to skip
            
            if (distSq <= maxDistSq && !skip) {
                if (grid[z][ny][nx] == CELL_AIR) {
                    grid[z][ny][nx] = CELL_TREE_LEAVES;
                    MarkChunkDirty(nx, ny, z);
                }
            }
        }
    }
}

// Single growth tick for one cell
static void GrowCell(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    
    if (cell == CELL_SAPLING) {
        // Block growth if items are on this tile
        if (QueryItemAtTile(x, y, z) >= 0) {
            return;  // Item present, don't grow
        }
        
        // Sapling grows into trunk
        grid[z][y][x] = CELL_TREE_TRUNK;
        MarkChunkDirty(x, y, z);
        growthTimer[z][y][x] = 0;
        
        // Set target height for this tree based on position (deterministic randomness)
        unsigned int hash = PositionHash(x, y, z);
        int heightRange = MAX_TREE_HEIGHT - MIN_TREE_HEIGHT + 1;
        targetHeight[z][y][x] = MIN_TREE_HEIGHT + (hash % heightRange);
        
    } else if (cell == CELL_TREE_TRUNK) {
        // Check if we should grow upward
        int height = GetTrunkHeight(x, y, z);
        
        // Find base of trunk to get target height
        int baseZ = z;
        while (baseZ > 0 && grid[baseZ - 1][y][x] == CELL_TREE_TRUNK) {
            baseZ--;
        }
        int maxHeight = targetHeight[baseZ][y][x];
        if (maxHeight == 0) maxHeight = MAX_TREE_HEIGHT;  // Fallback for old trees
        
        if (height < maxHeight && z + 1 < gridDepth) {
            CellType above = grid[z + 1][y][x];
            if (above == CELL_AIR || above == CELL_TREE_LEAVES) {
                // Grow trunk upward
                grid[z + 1][y][x] = CELL_TREE_TRUNK;
                MarkChunkDirty(x, y, z + 1);
                growthTimer[z + 1][y][x] = 0;
            }
        } else {
            // Reached target height or blocked - spawn leaves
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
                    if (growthTimer[z][y][x] >= saplingGrowTicks) {
                        GrowCell(x, y, z);
                    }
                } else if (cell == CELL_TREE_TRUNK) {
                    // Only grow from topmost trunk in a column
                    if (z + 1 >= gridDepth || grid[z + 1][y][x] != CELL_TREE_TRUNK) {
                        growthTimer[z][y][x]++;
                        if (growthTimer[z][y][x] >= trunkGrowTicks) {
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
    
    // Grow sapling to trunk (this sets the target height)
    GrowCell(x, y, z);
    
    // Get target height for this tree
    int treeTargetHeight = targetHeight[z][y][x];
    if (treeTargetHeight == 0) {
        // Fallback: calculate from position hash
        unsigned int hash = PositionHash(x, y, z);
        int heightRange = MAX_TREE_HEIGHT - MIN_TREE_HEIGHT + 1;
        treeTargetHeight = MIN_TREE_HEIGHT + (hash % heightRange);
    }
    
    // Grow trunk upward until target height or blocked
    int currentZ = z;
    for (int i = 0; i < treeTargetHeight; i++) {
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
