// trees.c - Tree growth cellular automaton
// Saplings grow into trunks, trunks grow upward, branches/leaves spawn by type

#include "trees.h"
#include "balance.h"
#include "weather.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../entities/items.h"
#include "../world/material.h"
#include <stdlib.h>
#include <math.h>

// Growth parameters - runtime configurable (game-hours)
// 2 game-years ≈ 2688 GH. ~12-15 growth events → ~200 GH per stage.
// Seasonal modulation (dormant in winter) stretches effective time.
float saplingGrowGH = 200.0f;    // Game-hours before sapling becomes young tree
float trunkGrowGH = 200.0f;      // Game-hours between growth stages (young + mature)
float youngToMatureGH = 400.0f;  // Game-hours young tree waits at full height before maturing

// Growth parameters - compile-time constants (game-hours)
#define LEAF_DECAY_GH 0.2f       // Game-hours before orphan leaf decays
#define LEAF_TRUNK_CHECK_DIST 4   // Max distance to check for trunk connection

// Simple timer grid for growth (could be optimized with a list later)
float growthTimer[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Target height per tree (set when sapling becomes trunk, based on position)
int targetHeight[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Harvest state per cell (only meaningful on trunk base cells)
uint8_t treeHarvestState[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Simple hash for position-based randomness (deterministic)
static unsigned int PositionHash(int x, int y, int z) {
    unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + z * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

static MaterialType NormalizeTreeType(MaterialType mat) {
    if (!IsWoodMaterial(mat)) return MAT_OAK;
    return mat;
}

const char* TreeTypeName(MaterialType mat) {
    return MaterialName(mat);
}

ItemType SaplingItemFromTreeType(MaterialType mat) {
    (void)mat;  // Material stored on item, not in type
    return ITEM_SAPLING;
}

ItemType LeafItemFromTreeType(MaterialType mat) {
    (void)mat;  // Material stored on item, not in type
    return ITEM_LEAVES;
}

// Forward declaration (defined later, needed by young tree helpers)
static void PlaceLeavesDisk(int cx, int cy, int z, int radius, int skipChance, MaterialType treeMat);

// Young tree max height per species (before maturation into CELL_TREE_TRUNK)
int GetYoungTreeHeight(MaterialType treeMat) {
    switch (treeMat) {
        case MAT_PINE:   return 3;
        case MAT_BIRCH:  return 2;
        case MAT_WILLOW: return 2;
        case MAT_OAK:
        default:         return 3;
    }
}

// Check if cell is the base of a young tree (CELL_TREE_BRANCH column on ground)
bool IsYoungTreeBase(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    if (grid[z][y][x] != CELL_TREE_BRANCH) return false;
    // Young tree base: no trunk/branch below (sitting on ground)
    if (z > 0 && (grid[z - 1][y][x] == CELL_TREE_TRUNK || grid[z - 1][y][x] == CELL_TREE_BRANCH)) return false;
    return true;
}

// Find base of a young tree branch column
static int FindYoungTreeBaseZ(int x, int y, int z) {
    int baseZ = z;
    while (baseZ > 0 && grid[baseZ - 1][y][x] == CELL_TREE_BRANCH) {
        baseZ--;
    }
    // Must not have trunk below to be a young tree
    if (baseZ > 0 && grid[baseZ - 1][y][x] == CELL_TREE_TRUNK) return -1;
    return baseZ;
}

// Get height of young tree branch column from base
static int GetYoungTreeHeightFromBase(int x, int y, int baseZ) {
    int height = 0;
    for (int checkZ = baseZ; checkZ < gridDepth; checkZ++) {
        if (grid[checkZ][y][x] == CELL_TREE_BRANCH) {
            height++;
        } else {
            break;
        }
    }
    return height;
}

// Place sparse young tree leaves at top
static void PlaceYoungTreeLeaves(int x, int y, int topZ, MaterialType treeMat) {
    int leafZ = topZ + 1;
    if (leafZ >= gridDepth) return;
    PlaceLeavesDisk(x, y, leafZ, 1, 60, treeMat);
}

// Remove leaves around a young tree top (before moving them up)
static void ClearYoungTreeLeaves(int x, int y, int topZ, MaterialType treeMat) {
    int leafZ = topZ + 1;
    if (leafZ < 0 || leafZ >= gridDepth) return;
    // Clear radius-1 disk
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (grid[leafZ][ny][nx] == CELL_TREE_LEAVES && GetWallMaterial(nx, ny, leafZ) == treeMat) {
                grid[leafZ][ny][nx] = CELL_AIR;
                SetWallMaterial(nx, ny, leafZ, MAT_NONE);
                MarkChunkDirty(nx, ny, leafZ);
            }
        }
    }
}

static void GetTreeHeightRange(MaterialType treeMat, int* outMin, int* outMax) {
    switch (treeMat) {
        case MAT_PINE:
            *outMin = 5; *outMax = 8; break;
        case MAT_BIRCH:
            *outMin = 4; *outMax = 7; break;
        case MAT_WILLOW:
            *outMin = 4; *outMax = 6; break;
        case MAT_OAK:
        default:
            *outMin = 4; *outMax = 6; break;
    }
}

void InitTrees(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                growthTimer[z][y][x] = 0.0f;
                targetHeight[z][y][x] = 0;
                treeHarvestState[z][y][x] = 0;
            }
        }
    }
}

// Check if a leaf cell is connected to a trunk of the same type within distance
static bool IsConnectedToTrunk(int x, int y, int z, int maxDist, MaterialType treeMat) {
    int horizRadius = 3;
    MaterialType mat = treeMat;

    for (int checkZ = z; checkZ >= 0 && checkZ >= z - maxDist; checkZ--) {
        for (int dy = -horizRadius; dy <= horizRadius; dy++) {
            for (int dx = -horizRadius; dx <= horizRadius; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                CellType c = grid[checkZ][ny][nx];
                if ((c == CELL_TREE_TRUNK || c == CELL_TREE_BRANCH) &&
                    GetWallMaterial(nx, ny, checkZ) == mat) {
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

// Convert topmost trunk cells to branches for visual taper
// height >= 4: top 2 become branches; height >= 2: top 1; height < 2: none
static void TaperTrunkTop(int x, int y, int baseZ, int height, MaterialType treeMat) {
    (void)treeMat;  // Material already set on trunk cells
    int taperCount = (height >= 4) ? 2 : (height >= 2) ? 1 : 0;
    int topZ = baseZ + height - 1;
    for (int i = 0; i < taperCount; i++) {
        int z = topZ - i;
        if (grid[z][y][x] == CELL_TREE_TRUNK) {
            grid[z][y][x] = CELL_TREE_BRANCH;
            MarkChunkDirty(x, y, z);
        }
    }
}

static __attribute__((unused)) void PlaceLeafCell(int x, int y, int z, MaterialType treeMat) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (grid[z][y][x] != CELL_AIR) return;
    grid[z][y][x] = CELL_TREE_LEAVES;
    SetWallMaterial(x, y, z, treeMat);
    MarkChunkDirty(x, y, z);
}

static void PlaceBranchCell(int x, int y, int z, MaterialType treeMat) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (grid[z][y][x] != CELL_AIR && grid[z][y][x] != CELL_TREE_LEAVES) return;
    grid[z][y][x] = CELL_TREE_BRANCH;
    SetWallMaterial(x, y, z, treeMat);
    MarkChunkDirty(x, y, z);
}

static void PlaceRootCell(int x, int y, int z, MaterialType treeMat) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (!CellIsSolid(grid[z][y][x])) return;
    grid[z][y][x] = CELL_TREE_ROOT;
    SetWallMaterial(x, y, z, treeMat);
    MarkChunkDirty(x, y, z);
}

static void PlaceLeavesDisk(int cx, int cy, int z, int radius, int skipChance, MaterialType treeMat) {
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
                SetWallMaterial(nx, ny, z, treeMat);
                MarkChunkDirty(nx, ny, z);
            }
        }
    }
}

static void SpawnLeavesForType(MaterialType treeMat, int trunkX, int trunkY, int topZ) {
    unsigned int hash = PositionHash(trunkX, trunkY, topZ);

    if (treeMat == MAT_OAK) {
        int radius = 2 + (hash % 2);  // 2-3
        int levels = 1 + ((hash >> 4) % 2); // 1-2
        for (int i = 0; i <= levels; i++) {
            int z = topZ + 1 + i;
            int r = radius - (i == levels ? 1 : 0);
            if (r < 1) r = 1;
            PlaceLeavesDisk(trunkX, trunkY, z, r, 20, treeMat);
        }
        // skirt at trunk top
        PlaceLeavesDisk(trunkX, trunkY, topZ, radius, 40, treeMat);
        return;
    }

    if (treeMat == MAT_PINE) {
        int levels = 3;
        int radius = 2;
        for (int i = 0; i < levels; i++) {
            int z = topZ + i;
            int r = radius - i;
            if (r < 1) r = 1;
            PlaceLeavesDisk(trunkX, trunkY, z, r, 35, treeMat);
        }
        return;
    }

    if (treeMat == MAT_BIRCH) {
        int radius = 1 + (hash % 2);  // 1-2
        PlaceLeavesDisk(trunkX, trunkY, topZ, radius, 50, treeMat);
        PlaceLeavesDisk(trunkX, trunkY, topZ + 1, radius - 1, 55, treeMat);
        return;
    }

    // Willow (drooping)
    int radius = 2 + (hash % 2);  // 2-3
    for (int i = 0; i < 3; i++) {
        int z = topZ - i;
        PlaceLeavesDisk(trunkX, trunkY, z, radius, 50, treeMat);
    }
    // small puff above
    PlaceLeavesDisk(trunkX, trunkY, topZ + 1, radius - 1, 60, treeMat);
}

static void SpawnBranchesForType(MaterialType treeMat, int trunkX, int trunkY, int baseZ, int topZ) {
    int height = topZ - baseZ + 1;
    unsigned int hash = PositionHash(trunkX, trunkY, baseZ);

    int dxs[4] = {1, -1, 0, 0};
    int dys[4] = {0, 0, 1, -1};

    if (treeMat == MAT_OAK) {
        int levels[3] = { baseZ + 2, baseZ + 3, baseZ + 4 };
        for (int i = 0; i < 3; i++) {
            int z = levels[i];
            if (z >= topZ) continue;
            int branchCount = 2 + ((hash >> (i * 3)) % 2); // 2-3 branches per level
            for (int b = 0; b < branchCount; b++) {
                int dir = (hash >> (b * 5 + i * 2)) % 4;
                int nx = trunkX + dxs[dir];
                int ny = trunkY + dys[dir];
                PlaceBranchCell(nx, ny, z, treeMat);
                if (((hash >> (b * 7 + 1)) % 100) < 60 && z + 1 < gridDepth) {
                    PlaceBranchCell(nx, ny, z + 1, treeMat);
                }
            }
        }
        return;
    }

    if (treeMat == MAT_PINE) {
        if ((hash % 100) < 30) {
            int z = baseZ + (height - 2);
            if (z > baseZ && z < topZ) {
                int dir = (hash >> 6) % 4;
                PlaceBranchCell(trunkX + dxs[dir], trunkY + dys[dir], z, treeMat);
            }
        }
        return;
    }

    if (treeMat == MAT_BIRCH) {
        if ((hash % 100) < 40) {
            int z = baseZ + 2;
            if (z < topZ) {
                int dir = (hash >> 5) % 4;
                PlaceBranchCell(trunkX + dxs[dir], trunkY + dys[dir], z, treeMat);
            }
        }
        return;
    }

    // Willow
    if ((hash % 100) < 60) {
        int z = baseZ + (height / 2);
        if (z < topZ) {
            int dir = (hash >> 4) % 4;
            PlaceBranchCell(trunkX + dxs[dir], trunkY + dys[dir], z, treeMat);
        }
    }
}

static void PlaceRootsForTree(int baseX, int baseY, int baseZ, MaterialType treeMat) {
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
        PlaceRootCell(nx, ny, rootZ, treeMat);
    }

    // Optional exposed root adjacent to base (only for oak/willow)
    if ((treeMat == MAT_OAK || treeMat == MAT_WILLOW) && (hash % 100) < 25) {
        int dir = (hash >> 9) % 4;
        int nx = baseX + dxs[dir];
        int ny = baseY + dys[dir];
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
            if (grid[baseZ][ny][nx] == CELL_AIR && baseZ > 0 && CellIsSolid(grid[baseZ - 1][ny][nx])) {
                grid[baseZ][ny][nx] = CELL_TREE_ROOT;
                SetWallMaterial(nx, ny, baseZ, treeMat);
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

        MaterialType treeMat = (GetWallMaterial(x, y, z));
        treeMat = NormalizeTreeType(treeMat);

        // Sapling grows into young tree (CELL_TREE_BRANCH, thin sprite)
        grid[z][y][x] = CELL_TREE_BRANCH;
        SetWallMaterial(x, y, z, treeMat);
        MarkChunkDirty(x, y, z);

        // Set target height to young tree height (1-3 per species)
        targetHeight[z][y][x] = GetYoungTreeHeight(treeMat);

        // Stagger growth timer
        unsigned int hash = PositionHash(x, y, z);
        growthTimer[z][y][x] = fmodf((float)(hash % 10000) / 10000.0f * GameHoursToGameSeconds(trunkGrowGH), GameHoursToGameSeconds(trunkGrowGH));

        // Place small leaf cluster on top
        PlaceYoungTreeLeaves(x, y, z, treeMat);
    } else if (cell == CELL_TREE_BRANCH && IsYoungTreeBase(x, y, z)) {
        // Young tree growth: grow upward or mature into trunk
        int baseZ = FindYoungTreeBaseZ(x, y, z);
        if (baseZ < 0) return;
        MaterialType treeMat = GetWallMaterial(x, y, baseZ);
        treeMat = NormalizeTreeType(treeMat);
        int youngMaxH = targetHeight[baseZ][y][x];
        if (youngMaxH <= 0) youngMaxH = GetYoungTreeHeight(treeMat);
        int height = GetYoungTreeHeightFromBase(x, y, baseZ);
        int topZ = baseZ + height - 1;

        if (height < youngMaxH && topZ + 1 < gridDepth) {
            // Grow upward: add another branch cell, move leaves
            CellType above = grid[topZ + 1][y][x];
            if (above == CELL_AIR || above == CELL_TREE_LEAVES) {
                ClearYoungTreeLeaves(x, y, topZ, treeMat);
                grid[topZ + 1][y][x] = CELL_TREE_BRANCH;
                SetWallMaterial(x, y, topZ + 1, treeMat);
                MarkChunkDirty(x, y, topZ + 1);
                PlaceYoungTreeLeaves(x, y, topZ + 1, treeMat);
                growthTimer[z][y][x] = 0.0f;
            }
        } else {
            // Young tree at full height — mature into trunk tree
            // Clear young leaves first
            ClearYoungTreeLeaves(x, y, topZ, treeMat);

            // Convert all CELL_TREE_BRANCH in column to CELL_TREE_TRUNK
            for (int cz = baseZ; cz <= topZ; cz++) {
                if (grid[cz][y][x] == CELL_TREE_BRANCH) {
                    grid[cz][y][x] = CELL_TREE_TRUNK;
                    MarkChunkDirty(x, y, cz);
                }
            }

            // Set mature target height and reset timer for continued growth
            unsigned int hash = PositionHash(x, y, baseZ);
            int minH, maxH;
            GetTreeHeightRange(treeMat, &minH, &maxH);
            int heightRange = maxH - minH + 1;
            targetHeight[baseZ][y][x] = minH + (hash % heightRange);
            growthTimer[topZ][y][x] = 0.0f;

            // Tree starts fully harvestable
            treeHarvestState[baseZ][y][x] = TREE_HARVEST_MAX;

            // Roots on maturation
            PlaceRootsForTree(x, y, baseZ, treeMat);

            // treeActiveCells stays the same — still growing as trunk now
        }
    } else if (cell == CELL_TREE_TRUNK) {
        // Check if we should grow upward
        int baseZ = FindTrunkBaseZ(x, y, z);
        MaterialType treeMat = (GetWallMaterial(x, y, baseZ));
        treeMat = NormalizeTreeType(treeMat);
        int maxHeight = targetHeight[baseZ][y][x];
        if (maxHeight == 0) {
            int minH, maxH;
            GetTreeHeightRange(treeMat, &minH, &maxH);
            maxHeight = maxH;
        }

        int height = GetTrunkHeightFromBase(x, y, baseZ);

        if (height < maxHeight && z + 1 < gridDepth) {
            CellType above = grid[z + 1][y][x];
            if (above == CELL_AIR || above == CELL_TREE_LEAVES) {
                grid[z + 1][y][x] = CELL_TREE_TRUNK;
                SetWallMaterial(x, y, z + 1, treeMat);
                MarkChunkDirty(x, y, z + 1);
                growthTimer[z + 1][y][x] = 0.0f;
            }
        } else {
            // Reached target height or blocked - taper top, spawn branches and leaves
            TaperTrunkTop(x, y, baseZ, height, treeMat);
            int topZ = baseZ + height - 1;
            SpawnBranchesForType(treeMat, x, y, baseZ, topZ);
            SpawnLeavesForType(treeMat, x, y, topZ);
            treeActiveCells--;
        }
    } else if (cell == CELL_TREE_LEAVES) {
        MaterialType treeMat = (GetWallMaterial(x, y, z));
        treeMat = NormalizeTreeType(treeMat);
        if (!IsConnectedToTrunk(x, y, z, LEAF_TRUNK_CHECK_DIST, treeMat)) {
            grid[z][y][x] = CELL_AIR;
            SetWallMaterial(x, y, z, MAT_NONE);
            MarkChunkDirty(x, y, z);
        }
    }
}

// Run one tick of tree growth simulation
void TreesTick(float dt) {
    bool hasGrowing = (treeActiveCells > 0);
    bool hasRegen = (treeRegenCells > 0);

    // Early exit: nothing to grow and nothing to regenerate
    if (!hasGrowing && !hasRegen) return;

    // Seasonal modulation: trees grow in spurts (fast spring, dormant winter)
    float seasonMod = GetVegetationGrowthRate();
    float seasonalDt = dt * seasonMod;

    float saplingThreshold = GameHoursToGameSeconds(saplingGrowGH);
    float trunkThreshold = GameHoursToGameSeconds(trunkGrowGH);
    float youngMatureThreshold = GameHoursToGameSeconds(youngToMatureGH);
    float regenThreshold = GameHoursToGameSeconds(TREE_HARVEST_REGEN_GH);
    float leafThreshold = GameHoursToGameSeconds(LEAF_DECAY_GH);

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];

                if (cell == CELL_SAPLING) {
                    if (!hasGrowing) continue;
                    growthTimer[z][y][x] += seasonalDt;
                    if (growthTimer[z][y][x] >= saplingThreshold) {
                        GrowCell(x, y, z);
                    }
                } else if (cell == CELL_TREE_BRANCH && IsYoungTreeBase(x, y, z)) {
                    // Young tree: grow upward or mature
                    if (!hasGrowing) continue;
                    int baseZ = FindYoungTreeBaseZ(x, y, z);
                    if (baseZ < 0) continue;
                    int height = GetYoungTreeHeightFromBase(x, y, baseZ);
                    int youngMaxH = targetHeight[baseZ][y][x];
                    if (youngMaxH <= 0) {
                        MaterialType treeMat = NormalizeTreeType(GetWallMaterial(x, y, baseZ));
                        youngMaxH = GetYoungTreeHeight(treeMat);
                    }

                    if (height < youngMaxH) {
                        // Still growing upward — use trunkGrowGH timer on base
                        growthTimer[z][y][x] += seasonalDt;
                        if (growthTimer[z][y][x] >= trunkThreshold) {
                            GrowCell(x, y, z);
                        }
                    } else {
                        // At full young height — wait youngToMatureGH then mature
                        growthTimer[z][y][x] += seasonalDt;
                        if (growthTimer[z][y][x] >= youngMatureThreshold) {
                            GrowCell(x, y, z);
                        }
                    }
                } else if (cell == CELL_TREE_TRUNK) {
                    // Growth: only topmost trunk, only when trees are growing
                    if (hasGrowing &&
                        (z + 1 >= gridDepth || grid[z + 1][y][x] != CELL_TREE_TRUNK)) {
                        growthTimer[z][y][x] += seasonalDt;
                        if (growthTimer[z][y][x] >= trunkThreshold) {
                            GrowCell(x, y, z);
                            growthTimer[z][y][x] = 0.0f;
                        }
                    }

                    // Harvest regen on trunk base cells only (not seasonal)
                    if (hasRegen && (z == 0 || grid[z - 1][y][x] != CELL_TREE_TRUNK)) {
                        if (treeHarvestState[z][y][x] < TREE_HARVEST_MAX) {
                            growthTimer[z][y][x] += dt;
                            if (growthTimer[z][y][x] >= regenThreshold) {
                                treeHarvestState[z][y][x]++;
                                growthTimer[z][y][x] = 0.0f;
                                if (treeHarvestState[z][y][x] >= TREE_HARVEST_MAX) {
                                    treeRegenCells--;
                                }
                            }
                        }
                    }
                } else if (cell == CELL_TREE_LEAVES) {
                    if (!hasGrowing) continue;
                    growthTimer[z][y][x] += dt;  // Leaf decay not seasonal
                    if (growthTimer[z][y][x] >= leafThreshold) {
                        GrowCell(x, y, z);
                        growthTimer[z][y][x] = 0.0f;
                    }
                }
            }
        }
    }
}

// Instantly grow a full mature tree (skips young tree stage)
void TreeGrowFull(int x, int y, int z, MaterialType treeMat) {
    treeMat = NormalizeTreeType(treeMat);

    // Don't grow on cells that already have a tree (re-growing corrupts taper/leaves)
    CellType existing = grid[z][y][x];
    if (existing == CELL_TREE_TRUNK || existing == CELL_TREE_BRANCH ||
        existing == CELL_TREE_LEAVES || existing == CELL_TREE_ROOT) {
        return;
    }

    // Place trunk directly at base (skip sapling/young tree stages)
    grid[z][y][x] = CELL_TREE_TRUNK;
    SetWallMaterial(x, y, z, treeMat);
    MarkChunkDirty(x, y, z);

    // Set target height
    unsigned int hash = PositionHash(x, y, z);
    int minH, maxH;
    GetTreeHeightRange(treeMat, &minH, &maxH);
    int heightRange = maxH - minH + 1;
    int treeTargetHeight = minH + (hash % heightRange);
    targetHeight[z][y][x] = treeTargetHeight;

    // Tree starts fully harvestable
    treeHarvestState[z][y][x] = TREE_HARVEST_MAX;

    // Place roots
    PlaceRootsForTree(x, y, z, treeMat);

    // Grow trunk upward
    int currentZ = z;
    for (int i = 0; i < treeTargetHeight; i++) {
        if (currentZ + 1 >= gridDepth) break;

        CellType above = grid[currentZ + 1][y][x];
        if (above != CELL_AIR && above != CELL_TREE_LEAVES) break;

        grid[currentZ + 1][y][x] = CELL_TREE_TRUNK;
        SetWallMaterial(x, y, currentZ + 1, treeMat);
        MarkChunkDirty(x, y, currentZ + 1);
        currentZ++;
    }

    int fullHeight = currentZ - z + 1;
    TaperTrunkTop(x, y, z, fullHeight, treeMat);
    SpawnBranchesForType(treeMat, x, y, z, currentZ);
    SpawnLeavesForType(treeMat, x, y, currentZ);
}

// Instantly grow a young tree (1-3 CELL_TREE_BRANCH cells + sparse leaves) for worldgen
void TreeGrowYoung(int x, int y, int z, MaterialType treeMat) {
    treeMat = NormalizeTreeType(treeMat);

    CellType existing = grid[z][y][x];
    if (existing == CELL_TREE_TRUNK || existing == CELL_TREE_BRANCH ||
        existing == CELL_TREE_LEAVES || existing == CELL_TREE_ROOT) {
        return;
    }

    int youngMaxH = GetYoungTreeHeight(treeMat);
    // Use hash to pick a height 1..youngMaxH
    unsigned int hash = PositionHash(x, y, z);
    int height = 1 + (int)(hash % (unsigned int)youngMaxH);

    // Place branch column
    for (int i = 0; i < height; i++) {
        int cz = z + i;
        if (cz >= gridDepth) break;
        if (i == 0 && grid[cz][y][x] != CELL_AIR && grid[cz][y][x] != CELL_SAPLING) break;
        if (i > 0 && grid[cz][y][x] != CELL_AIR) break;
        grid[cz][y][x] = CELL_TREE_BRANCH;
        SetWallMaterial(x, y, cz, treeMat);
        MarkChunkDirty(x, y, cz);
    }

    // Store young target height for future growth
    targetHeight[z][y][x] = youngMaxH;

    // Place leaves on top
    int topZ = z + height - 1;
    PlaceYoungTreeLeaves(x, y, topZ, treeMat);

    // Mark as active so TreesTick processes it (will continue growing/maturing)
    treeActiveCells++;
}

// Place a sapling that will grow over time
void PlaceSapling(int x, int y, int z, MaterialType treeMat) {
    if (grid[z][y][x] != CELL_AIR && !CellIsSolid(grid[z][y][x])) return;

    // Need solid ground below in DF mode
    if (z > 0 && !CellIsSolid(grid[z - 1][y][x])) return;

    treeMat = NormalizeTreeType(treeMat);
    grid[z][y][x] = CELL_SAPLING;
    SetWallMaterial(x, y, z, treeMat);

    unsigned int hash = PositionHash(x, y, z);
    growthTimer[z][y][x] = fmodf((float)(hash % 10000) / 10000.0f * GameHoursToGameSeconds(saplingGrowGH), GameHoursToGameSeconds(saplingGrowGH));
    treeActiveCells++;
    MarkChunkDirty(x, y, z);
}
