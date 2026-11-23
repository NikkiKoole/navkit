// tide.h - Tide (experiment, docs/todo/world/tide.md)
//
// The sea is one body: the water cells on its top level (tideSeaZ) connected to the map edge
// carry WaterCell.isSea. The tide sets their level directly (a cosine over tidePeriodHours,
// between tideLowLevel and full); the water sim puts a sea cell it visits back to that level,
// so rivers flow into it and it floods what it touches. At low tide shallow floor falls dry.
#ifndef TIDE_H
#define TIDE_H

#include <stdbool.h>

extern bool tideEnabled;          // off: sea cells keep whatever level they have
extern float tidePeriodHours;     // game hours from high tide to high tide (North Sea: 12.42)
extern int tideLowLevel;          // water level of the top sea level at low tide (0 = dry)
extern int tideLevel;             // current level of the top sea level (0..7)
extern int tideSeaZ;              // the sea's top level (-1 = no sea marked)
extern int tideSeaCells;          // cells marked isSea
extern int tideSteps;             // level changes since TideMarkSea (stats)
extern double tideLastStepMs, tideMaxStepMs;

// Mark the sea: water cells at level z, connected to the map edge. Returns the cell count.
int TideMarkSea(int z);
void ClearTide(void);             // forget the sea (bits stay in the water layer until it is cleared)
void TideAfterLoad(void);         // rebuild the sea list from the loaded isSea bits (keeps the settings)
int TideLevelAt(float gameHours); // the level the tide wants at that time
void TideTick(void);              // called from UpdateWater

#endif
