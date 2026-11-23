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
extern bool islandTide;           // the island game starts with the tide on (games.c StartIsland)
extern float tidePeriodHours;     // game hours from high tide to high tide (North Sea: 12.42)
extern int tideLowLevel;          // water level of the top sea level at low tide (0 = dry)
extern int tideLevel;             // current level of the top sea level (0..7)
extern int tideSeaZ;              // the sea's top level (-1 = no sea marked)
extern int tideSeaCells;          // cells marked isSea
extern int tideSteps;             // level changes since TideMarkSea (stats)
extern double tideLastStepMs, tideMaxStepMs;
extern float tideWaveHours;       // the tide wave: game hours from the first strip of the sea to the last
extern float tideWaveAngle;       // direction the wave travels (degrees, 0 = from the west going east)
int TideCellLevel(int x, int y);  // the level the tide gives this sea cell now (its strip's)
void TideCellRange(int x, int y, int* lo, int* hi);   // levels a sea cell may have now (tide .. tide + surf)
// Surf: real swash on the flats near deep water, a wave every tideSurfPeriod game hours running up
// to tideSurfReach cells, tideSurfHeight deep near the sea; never past wading depth (nav unaffected)
extern bool tideSurf;
extern int tideSurfReach, tideSurfHeight;
extern float tideSurfPeriod;
extern int tideSurfCells, tideSurfChanges;   // cells in the surf zone; cells changed last tick
void TideSettle(void);            // every sea cell to the level the tide wants now (setup, before the nav graph)

// Mark the sea: water cells at level z, connected to the map edge. Returns the cell count.
int TideMarkSea(int z);
void ClearTide(void);             // forget the sea (bits stay in the water layer until it is cleared)
void TideAfterLoad(void);         // rebuild the sea list from the loaded isSea bits (keeps the settings)
int TideLevelAt(float gameHours); // the level the tide wants at that time
// Ramps from the flats up to the land: a land cell whose solid top is the sea's level, with a
// flat (sea cell over solid floor) on one side and land on the other, becomes a ramp rising
// from the flat (the beach as a slope). Call after TideMarkSea, then rebuild the nav graph.
int TidePrepareShore(void);
void TideTick(void);              // called from UpdateWater

#endif
