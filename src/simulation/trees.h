// trees.h - Tree growth system
#ifndef TREES_H
#define TREES_H

// Runtime configurable growth parameters
extern int saplingGrowTicks;     // Ticks before sapling becomes trunk (default 100)
extern int trunkGrowTicks;       // Ticks between trunk growth (default 50)

// Initialize tree growth system
void InitTrees(void);

// Run one tick of tree growth (call from simulation tick)
void TreesTick(float dt);

// Instantly grow a full tree at position (for placement)
void TreeGrowFull(int x, int y, int z);

// Place a sapling that will grow over time
void PlaceSapling(int x, int y, int z);

#endif // TREES_H
