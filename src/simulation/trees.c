// trees.c - Tree growth cellular automaton
// Saplings grow into trunks, trunks grow upward, branches/leaves spawn by type

#include "trees.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../entities/items.h"
#include "../world/material.h"
#include <stdlib.h>

// Growth parameters - runtime configurable
int saplingGrowTicks = 100;       // Ticks before sapling becomes trunk
int trunkGrowTicks = 50;          // Ticks between trunk growing upward

// Growth parameters - compile-time constants
#define LEAF_DECAY_TICKS 30       // Ticks before orphan leaf decays
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

static TreeType NormalizeTreeType(TreeType type) {
    if (type <= TREE_TYPE_NONE || type >= TREE_TYPE_COUNT) return TREE_TYPE_OAK;
    return type;
}

const char* TreeTypeName(TreeType type) {
    switch (type) {
        case TREE_TYPE_OAK: return "Oak";
        case TREE_TYPE_PINE: return "Pine";
        case TREE_TYPE_BIRCH: return "Birch";
        case TREE_TYPE_WILLOW: return "Willow";
        default: return "Unknown";
    }
}

ItemType SaplingItemFromTreeType(TreeType type) {
    switch (type) {
        case TREE_TYPE_PINE: return ITEM_SAPLING_PINE;
        case TREE_TYPE_BIRCH: return ITEM_SAPLING_BIRCH;
        case TREE_TYPE_WILLOW: return ITEM_SAPLING_WILLOW;
        case TREE_TYPE_OAK:
        default: return ITEM_SAPLING_OAK;
    }
}

ItemType LeafItemFromTreeType(TreeType type) {
    switch (type) {
        case TREE_TYPE_PINE: return ITEM_LEAVES_PINE;
        case TREE_TYPE_BIRCH: return ITEM_LEAVES_BIRCH;
        case TREE_TYPE_WILLOW: return ITEM_LEAVES_WILLOW;
        case TREE_TYPE_OAK:
        default: return ITEM_LEAVES_OAK;
    }
}

TreeType TreeTypeFromSaplingItem(ItemType type) {
    switch (type) {
        case ITEM_SAPLING_PINE: return TREE_TYPE_PINE;
        case ITEM_SAPLING_BIRCH: return TREE_TYPE_BIRCH;
        case ITEM_SAPLING_WILLOW: return TREE_TYPE_WILLOW;
        case ITEM_SAPLING_OAK:
        default: return TREE_TYPE_OAK;
    }
}

MaterialType MaterialFromTreeType(TreeType type) {
    switch (type) {
        case TREE_TYPE_PINE: return MAT_PINE;
        case TREE_TYPE_BIRCH: return MAT_BIRCH;
        case TREE_TYPE_WILLOW: return MAT_WILLOW;
        case TREE_TYPE_OAK:
        default: return MAT_OAK;
    }
}

TreeType TreeTypeFromMaterial(MaterialType mat) {
    switch (mat) {
        case MAT_PINE: return TREE_TYPE_PINE;
        case MAT_BIRCH: return TREE_TYPE_BIRCH;
        case MAT_WILLOW: return TREE_TYPE_WILLOW;
        case MAT_OAK: return TREE_TYPE_OAK;
        default: return TREE_TYPE_OAK;
    }
}

static void GetTreeHeightRange(TreeType type, int* outMin, int* outMax) {
    switch (type) {
        case TREE_TYPE_PINE:
            *outMin = 5; *outMax = 8; break;
        case TREE_TYPE_BIRCH:
            *outMin = 4; *outMax = 7; break;
        case TREE_TYPE_WILLOW:
            *outMin = 4; *outMax = 6; break;
        case TREE_TYPE_OAK:
        default:
            *outMin = 4; *outMax = 6; break;
    }
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

// Check if a leaf cell is connected to a trunk of the same type within distance
static bool IsConnectedToTrunk(int x, int y, int z, int maxDist, TreeType type) {
    int horizRadius = 3;
    MaterialType mat = MaterialFromTreeType(type);

    for (int checkZ = z; checkZ >= 0 && checkZ >= z - maxDist; checkZ--) {
        for (int dy = -horizRadius; dy <= horizRadius; dy++) {
            for (int dx = -horizRadius; dx <= horizRadius; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                if (grid[checkZ][ny][nx] == CELL_TREE_TRUNK &&
                    GetWallMaterial(nx, ny, checkZ) == mat &&
                    grid[checkZ][ny][nx] != CELL_TREE_FELLED) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Find base of main trunk column (CELL_TREE_TRUNK only)
static int FindTrunkBaseZ(int x, int y, int z) {
    int baseZ = z;
    while (baseZ > 0 &&
           grid[baseZ - 1][y][x] == CELL_TREE_TRUNK) {
        baseZ--;
    }
    return baseZ;
}

// Get the height of the main trunk column from baseZ upward
static int GetTrunkHeightFromBase(int x, int y, int baseZ) {
    int height = 0;
    for (int checkZ = baseZ; checkZ < gridDepth; checkZ++) {
        if (grid[checkZ][y][x] == CELL_TREE_TRUNK) {
            height++;
        } else {
            break;
        }
    }
    return height;
}

static __attribute__((unused)) void PlaceLeafCell(int x, int y, int z, TreeType type) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (grid[z][y][x] != CELL_AIR) return;
    grid[z][y][x] = CELL_TREE_LEAVES;
    SetWallMaterial(x, y, z, MaterialFromTreeType(type));
    MarkChunkDirty(x, y, z);
}

static void PlaceBranchCell(int x, int y, int z, TreeType type) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (grid[z][y][x] != CELL_AIR && grid[z][y][x] != CELL_TREE_LEAVES) return;
    grid[z][y][x] = CELL_TREE_BRANCH;
    SetWallMaterial(x, y, z, MaterialFromTreeType(type));
    MarkChunkDirty(x, y, z);
}

static void PlaceRootCell(int x, int y, int z, TreeType type) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (!IsGroundCell(grid[z][y][x])) return;
    grid[z][y][x] = CELL_TREE_ROOT;
    SetWallMaterial(x, y, z, MaterialFromTreeType(type));
    MarkChunkDirty(x, y, z);
}

static void PlaceLeavesDisk(int cx, int cy, int z, int radius, int skipChance, TreeType type) {
    if (radius <= 0 || z < 0 || z >= gridDepth) return;
    int radiusSq = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            int distSq = dx * dx + dy * dy;
            if (distSq > radiusSq + 1) continue;

            unsigned int h = PositionHash(nx, ny, z);
            if ((int)(h % 100) < skipChance) continue;

            if (grid[z][ny][nx] == CELL_AIR) {
                grid[z][ny][nx] = CELL_TREE_LEAVES;
                SetWallMaterial(nx, ny, z, MaterialFromTreeType(type));
                MarkChunkDirty(nx, ny, z);
            }
        }
    }
}

static void SpawnLeavesForType(TreeType type, int trunkX, int trunkY, int topZ) {
    unsigned int hash = PositionHash(trunkX, trunkY, topZ);

    if (type == TREE_TYPE_OAK) {
        int radius = 2 + (hash % 2);  // 2-3
        int levels = 1 + ((hash >> 4) % 2); // 1-2
        for (int i = 0; i <= levels; i++) {
            int z = topZ + 1 + i;
            int r = radius - (i == levels ? 1 : 0);
            if (r < 1) r = 1;
            PlaceLeavesDisk(trunkX, trunkY, z, r, 20, type);
        }
        // skirt at trunk top
        PlaceLeavesDisk(trunkX, trunkY, topZ, radius, 40, type);
        return;
    }

    if (type == TREE_TYPE_PINE) {
        int levels = 3;
        int radius = 2;
        for (int i = 0; i < levels; i++) {
            int z = topZ + i;
            int r = radius - i;
            if (r < 1) r = 1;
            PlaceLeavesDisk(trunkX, trunkY, z, r, 35, type);
        }
        return;
    }

    if (type == TREE_TYPE_BIRCH) {
        int radius = 1 + (hash % 2);  // 1-2
        PlaceLeavesDisk(trunkX, trunkY, topZ, radius, 50, type);
        PlaceLeavesDisk(trunkX, trunkY, topZ + 1, radius - 1, 55, type);
        return;
    }

    // Willow (drooping)
    int radius = 2 + (hash % 2);  // 2-3
    for (int i = 0; i < 3; i++) {
        int z = topZ - i;
        PlaceLeavesDisk(trunkX, trunkY, z, radius, 50, type);
    }
    // small puff above
    PlaceLeavesDisk(trunkX, trunkY, topZ + 1, radius - 1, 60, type);
}

static void SpawnBranchesForType(TreeType type, int trunkX, int trunkY, int baseZ, int topZ) {
    int height = topZ - baseZ + 1;
    unsigned int hash = PositionHash(trunkX, trunkY, baseZ);

    int dxs[4] = {1, -1, 0, 0};
    int dys[4] = {0, 0, 1, -1};

    if (type == TREE_TYPE_OAK) {
        int levels[3] = { baseZ + 2, baseZ + 3, baseZ + 4 };
        for (int i = 0; i < 3; i++) {
            int z = levels[i];
            if (z >= topZ) continue;
            int branchCount = 2 + ((hash >> (i * 3)) % 2); // 2-3 branches per level
            for (int b = 0; b < branchCount; b++) {
                int dir = (hash >> (b * 5 + i * 2)) % 4;
                int nx = trunkX + dxs[dir];
                int ny = trunkY + dys[dir];
                PlaceBranchCell(nx, ny, z, type);
                if (((hash >> (b * 7 + 1)) % 100) < 60 && z + 1 < gridDepth) {
                    PlaceBranchCell(nx, ny, z + 1, type);
                }
            }
        }
        return;
    }

    if (type == TREE_TYPE_PINE) {
        if ((hash % 100) < 30) {
            int z = baseZ + (height - 2);
            if (z > baseZ && z < topZ) {
                int dir = (hash >> 6) % 4;
                PlaceBranchCell(trunkX + dxs[dir], trunkY + dys[dir], z, type);
            }
        }
        return;
    }

    if (type == TREE_TYPE_BIRCH) {
        if ((hash % 100) < 40) {
            int z = baseZ + 2;
            if (z < topZ) {
                int dir = (hash >> 5) % 4;
                PlaceBranchCell(trunkX + dxs[dir], trunkY + dys[dir], z, type);
            }
        }
        return;
    }

    // Willow
    if ((hash % 100) < 60) {
        int z = baseZ + (height / 2);
        if (z < topZ) {
            int dir = (hash >> 4) % 4;
            PlaceBranchCell(trunkX + dxs[dir], trunkY + dys[dir], z, type);
        }
    }
}

static void PlaceRootsForTree(int baseX, int baseY, int baseZ, TreeType type) {
    if (baseZ <= 0) return;
    unsigned int hash = PositionHash(baseX, baseY, baseZ);

    int rootZ = baseZ - 1;
    int rootCount = 1 + (hash % 3);  // 1-3
    int dxs[4] = {1, -1, 0, 0};
    int dys[4] = {0, 0, 1, -1};

    for (int i = 0; i < rootCount; i++) {
        int dir = (hash >> (i * 3)) % 4;
        int nx = baseX + dxs[dir];
        int ny = baseY + dys[dir];
        PlaceRootCell(nx, ny, rootZ, type);
    }

    // Optional exposed root adjacent to base (only for oak/willow)
    if ((type == TREE_TYPE_OAK || type == TREE_TYPE_WILLOW) && (hash % 100) < 25) {
        int dir = (hash >> 9) % 4;
        int nx = baseX + dxs[dir];
        int ny = baseY + dys[dir];
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
            if (grid[baseZ][ny][nx] == CELL_AIR && baseZ > 0 && CellIsSolid(grid[baseZ - 1][ny][nx])) {
                grid[baseZ][ny][nx] = CELL_TREE_ROOT;
                SetWallMaterial(nx, ny, baseZ, MaterialFromTreeType(type));
                MarkChunkDirty(nx, ny, baseZ);
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

        TreeType type = TreeTypeFromMaterial(GetWallMaterial(x, y, z));
        type = NormalizeTreeType(type);

        // Sapling grows into trunk
        grid[z][y][x] = CELL_TREE_TRUNK;
        SetWallMaterial(x, y, z, MaterialFromTreeType(type));
        MarkChunkDirty(x, y, z);

        // Set target height for this tree based on position (deterministic randomness)
        unsigned int hash = PositionHash(x, y, z);
        int minH, maxH;
        GetTreeHeightRange(type, &minH, &maxH);
        int heightRange = maxH - minH + 1;
        targetHeight[z][y][x] = minH + (hash % heightRange);

        // Stagger trunk growth timer to avoid all trunks growing at once
        growthTimer[z][y][x] = hash % trunkGrowTicks;

        // Roots on conversion
        PlaceRootsForTree(x, y, z, type);
    } else if (cell == CELL_TREE_TRUNK) {
        // Check if we should grow upward
        int baseZ = FindTrunkBaseZ(x, y, z);
        TreeType type = TreeTypeFromMaterial(GetWallMaterial(x, y, baseZ));
        type = NormalizeTreeType(type);
        int maxHeight = targetHeight[baseZ][y][x];
        if (maxHeight == 0) {
            int minH, maxH;
            GetTreeHeightRange(type, &minH, &maxH);
            maxHeight = maxH;
        }

        int height = GetTrunkHeightFromBase(x, y, baseZ);

        if (height < maxHeight && z + 1 < gridDepth) {
            CellType above = grid[z + 1][y][x];
            if (above == CELL_AIR || above == CELL_TREE_LEAVES) {
                grid[z + 1][y][x] = CELL_TREE_TRUNK;
                SetWallMaterial(x, y, z + 1, MaterialFromTreeType(type));
                MarkChunkDirty(x, y, z + 1);
                growthTimer[z + 1][y][x] = 0;
            }
        } else {
            // Reached target height or blocked - spawn branches and leaves
            int topZ = baseZ + height - 1;
            SpawnBranchesForType(type, x, y, baseZ, topZ);
            SpawnLeavesForType(type, x, y, topZ);
            treeActiveCells--;
        }
    } else if (cell == CELL_TREE_LEAVES) {
        TreeType type = TreeTypeFromMaterial(GetWallMaterial(x, y, z));
        type = NormalizeTreeType(type);
        if (!IsConnectedToTrunk(x, y, z, LEAF_TRUNK_CHECK_DIST, type)) {
            grid[z][y][x] = CELL_AIR;
            SetWallMaterial(x, y, z, MAT_NONE);
            MarkChunkDirty(x, y, z);
        }
    }
}

// Run one tick of tree growth simulation
void TreesTick(float dt) {
    (void)dt;

    if (treeActiveCells == 0) {
        return;
    }

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
                    if (z + 1 >= gridDepth ||
                        grid[z + 1][y][x] != CELL_TREE_TRUNK) {
                        growthTimer[z][y][x]++;
                        if (growthTimer[z][y][x] >= trunkGrowTicks) {
                            GrowCell(x, y, z);
                            growthTimer[z][y][x] = 0;
                        }
                    }
                } else if (cell == CELL_TREE_LEAVES) {
                    growthTimer[z][y][x]++;
                    if (growthTimer[z][y][x] >= LEAF_DECAY_TICKS) {
                        GrowCell(x, y, z);
                        growthTimer[z][y][x] = 0;
                    }
                }
            }
        }
    }
}

// Instantly grow a full tree from a sapling position
void TreeGrowFull(int x, int y, int z, TreeType type) {
    type = NormalizeTreeType(type);

    bool addedActive = false;
    if (grid[z][y][x] != CELL_SAPLING && grid[z][y][x] != CELL_TREE_TRUNK) {
        grid[z][y][x] = CELL_SAPLING;
        SetWallMaterial(x, y, z, MaterialFromTreeType(type));
        treeActiveCells++;
        addedActive = true;
        MarkChunkDirty(x, y, z);
    } else if (grid[z][y][x] == CELL_SAPLING) {
        SetWallMaterial(x, y, z, MaterialFromTreeType(type));
        treeActiveCells++;
        addedActive = true;
    }

    GrowCell(x, y, z);

    int baseZ = FindTrunkBaseZ(x, y, z);
    int treeTargetHeight = targetHeight[baseZ][y][x];
    if (treeTargetHeight == 0) {
        unsigned int hash = PositionHash(x, y, z);
        int minH, maxH;
        GetTreeHeightRange(type, &minH, &maxH);
        int heightRange = maxH - minH + 1;
        treeTargetHeight = minH + (hash % heightRange);
    }

    int currentZ = baseZ;
    for (int i = 0; i < treeTargetHeight; i++) {
        if (currentZ + 1 >= gridDepth) break;
        if (grid[currentZ][y][x] != CELL_TREE_TRUNK) break;

        CellType above = grid[currentZ + 1][y][x];
        if (above != CELL_AIR && above != CELL_TREE_LEAVES) break;

        grid[currentZ + 1][y][x] = CELL_TREE_TRUNK;
        SetWallMaterial(x, y, currentZ + 1, MaterialFromTreeType(type));
        MarkChunkDirty(x, y, currentZ + 1);
        currentZ++;
    }

    SpawnBranchesForType(type, x, y, baseZ, currentZ);
    SpawnLeavesForType(type, x, y, currentZ);

    if (addedActive) {
        treeActiveCells--;
    }
}

// Place a sapling that will grow over time
void PlaceSapling(int x, int y, int z, TreeType type) {
    if (grid[z][y][x] != CELL_AIR && !IsGroundCell(grid[z][y][x])) return;

    // Need solid ground below in DF mode
    if (z > 0 && !CellIsSolid(grid[z - 1][y][x])) return;

    type = NormalizeTreeType(type);
    grid[z][y][x] = CELL_SAPLING;
    SetWallMaterial(x, y, z, MaterialFromTreeType(type));

    unsigned int hash = PositionHash(x, y, z);
    growthTimer[z][y][x] = hash % saplingGrowTicks;
    treeActiveCells++;
    MarkChunkDirty(x, y, z);
}
