// trees.h - Tree growth system
#ifndef TREES_H
#define TREES_H

#include "../world/grid.h"
#include "../world/material.h"
#include "../entities/items.h"

// Runtime configurable growth parameters (game-hours)
extern float saplingGrowGH;     // Game-hours before sapling becomes trunk
extern float trunkGrowGH;       // Game-hours between trunk growth stages

// Tree type helpers (now use MaterialType directly)
const char* TreeTypeName(MaterialType mat);
ItemType SaplingItemFromTreeType(MaterialType mat);
ItemType LeafItemFromTreeType(MaterialType mat);

// Growth grids (exposed for save/load)
extern float growthTimer[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern int targetHeight[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Harvest state (stored on trunk base cell only)
// 0 = depleted, TREE_HARVEST_MAX = fully harvestable
// Regen uses growthTimer on the base cell (idle on mature trees)
#define TREE_HARVEST_MAX 2
#define TREE_HARVEST_REGEN_GH 24.0f  // Game-hours to regen one harvest level
extern uint8_t treeHarvestState[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize tree growth system
void InitTrees(void);

// Run one tick of tree growth (call from simulation tick)
void TreesTick(float dt);

// Instantly grow a full tree at position (for placement)
void TreeGrowFull(int x, int y, int z, MaterialType treeMat);

// Place a sapling that will grow over time
void PlaceSapling(int x, int y, int z, MaterialType treeMat);

#endif // TREES_H
