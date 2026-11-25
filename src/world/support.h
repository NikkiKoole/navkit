// support.h - Structural support: what loses its connection to the ground falls
// (docs/todo/world/explosions.md "Structural support").
//
// The structure is every solid cell except trees (walls, rock, soil, windows, doors) plus every
// floor. Solid cells hold each other face to face; a floor hangs off a solid cell in the 3x3
// around it on its own level or the level below, or off a neighbouring floor. A piece of
// structure that can't reach z = 0 or bedrock through that is unsupported: after a warning
// (dust, `collapseWarning` game seconds) it falls. Each wall drops straight down and lands as a
// wall again (same material) on what's below; floors become rubble items; whoever stands where
// a wall lands is pushed aside and dazed (explosion.c knockback).
//
// Every cell change that reaches the pathfinder (MarkChunkDirty) is noted here; a change that
// left a cell open is checked next tick. Fresh generated worlds are fully connected (measured
// 2026-11-25: one Towers3D seed has 10 floating cells), so nothing falls by itself.
#ifndef SUPPORT_H
#define SUPPORT_H

#include <stdbool.h>

extern bool supportEnabled;      // setting structuralSupport
extern float collapseWarning;    // setting collapseWarning: game seconds between creak and fall

void SupportNoteChange(int x, int y, int z);   // MarkChunkDirty calls this
void SupportTick(float dt);                    // SimStep
void ClearSupport(void);

// Is the structure cell at (x, y, z) connected to the ground? (true for non-structure cells)
bool SupportIsGrounded(int x, int y, int z);
int SupportPendingCount(void);                 // collapses waiting out their warning
int SupportCountUnsupported(void);             // whole-world scan (tests, scenario checks, `support`)
// A new world: everything hanging drops into place now, without warning or effects (terrain
// generators leave the odd floating lump). Returns the cells that were hanging.
int SupportSettleWorld(void);

// Landslides: loose natural soil (dirt, sand, gravel, peat, mud) with its top open and a drop of
// two levels or more beside it slides down that side a moment after something shakes it (a
// blast, a collapse or another slide landing near it). It lands as the same wall one level or
// more down; whoever is there is pushed aside and dazed. Only shaken cells are looked at.
extern bool landslidesEnabled;                  // setting landslides
void LandslideDisturb(int x, int y, int z);     // shaken: slides in a moment if it can
void LandslideDisturbAround(int x, int y, int z, int r);   // every cell within r (a cube)
int LandslideCount(void);                       // cells slid since the last ClearSupport
int LandslidePendingCount(void);

#endif
