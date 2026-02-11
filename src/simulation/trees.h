// trees.h - Tree growth system
#ifndef TREES_H
#define TREES_H

#include "../world/grid.h"
#include "../world/material.h"
#include "../entities/items.h"

// Runtime configurable growth parameters
extern int saplingGrowTicks;     // Ticks before sapling becomes trunk (default 100)
extern int trunkGrowTicks;       // Ticks between trunk growth (default 50)

// Tree type helpers (now use MaterialType directly)
const char* TreeTypeName(MaterialType mat);
ItemType SaplingItemFromTreeType(MaterialType mat);
ItemType LeafItemFromTreeType(MaterialType mat);

// Growth grids (exposed for save/load)
extern int growthTimer[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern int targetHeight[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize tree growth system
void InitTrees(void);

// Run one tick of tree growth (call from simulation tick)
void TreesTick(float dt);

// Instantly grow a full tree at position (for placement)
void TreeGrowFull(int x, int y, int z, MaterialType treeMat);

// Place a sapling that will grow over time
void PlaceSapling(int x, int y, int z, MaterialType treeMat);

#endif // TREES_H
