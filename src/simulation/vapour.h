// vapour.h - Fuel vapour (petrol fumes): an invisible-ish gas heavier than air, and the flame
// that runs through it when it's lit (docs/todo/world/explosions.md "kinds of explosive, gas
// and flash fires").
//
// Two kinds that don't mix: petrol vapour (leaks, sinks) and firedamp (methane from the coal
// measures: let out by digging slate, it rises and gathers under the roof of a mine).
// Vapour leaks from a source, sinks (pools in pits, cellars, low ground), spreads level with
// its neighbours and thins out slowly at its edges. A fire or spark in a cell with enough of it
// (level >= VAPOUR_LIGHT_LEVEL) lights the cloud: the flame front runs cell to cell through
// every connected cell with vapour in it, a few cells per tenth of a second (not one instant
// disc). Each cell that burns: its vapour is gone, burnable things in and under it catch fire,
// it heats up, leaves a little smoke and pushes whoever is in it away from where the fire
// started. In the open that's a flash fire rolling over the ground. In a closed room (rooms.c,
// not outdoors) the burnt vapour per room cell is a pressure: while it beats the weakest bit of
// the room's edge near the fire (a window 1, a door 1.5, a wall its blast strength: wood 2.5,
// brick 3, stone 4) that bit blows out with a jet of flame (explosion.c ExplosionVent) and the
// pressure drops; it also leaks away by vapourVentLeak per second.
//
// Sparse layer like steam (memory only where vapour has been); the cells with vapour are also
// kept in a list, so a tick costs the cloud, not the map. Not saved yet: after a load there is
// no vapour.
#ifndef VAPOUR_H
#define VAPOUR_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

#define VAPOUR_MAX_LEVEL 7
#define VAPOUR_LIGHT_LEVEL 1   // a spark lights a cell with this much or more (the thin edge too)

typedef struct {
    uint8_t level : 3;    // 0-7
    uint8_t listed : 1;   // in the active list
    uint8_t burning : 1;  // the flame front is in it
    uint8_t kind : 1;     // VapourKind: petrol vapour (sinks) or firedamp (rises)
    uint8_t pad : 2;
} VapourCell;

typedef enum { VAPOUR_PETROL = 0, VAPOUR_FIREDAMP = 1 } VapourKind;

extern SparseLayer vapourLayer;
#define VAPOUR(x, y, z) (*(const VapourCell*)SparseLayerRead(&vapourLayer, (x), (y), (z)))
#define VAPOUR_MUT(x, y, z) (*(VapourCell*)SparseLayerWrite(&vapourLayer, (x), (y), (z)))

extern bool vapourEnabled;        // setting vapour
extern float vapourStepInterval;  // setting vapourStep: game seconds between moves (sink, spread)
extern float vapourThinChance;    // setting vapourThin: per move, chance a thin edge cell (level 1) clears

void ClearVapour(void);
int GetVapourLevel(int x, int y, int z);            // either kind
int GetVapourLevelOf(int x, int y, int z, VapourKind kind);
int GetPetrolLevel(int x, int y, int z);
int GetFiredampLevel(int x, int y, int z);
void AddVapour(int x, int y, int z, int amount);   // petrol; capped at VAPOUR_MAX_LEVEL
// Kinds don't mix: a cell holding the other kind takes none
void AddVapourKind(int x, int y, int z, int amount, VapourKind kind);

// Firedamp: digging natural slate (the coal measures) lets a pocket out now and then. Call with
// the cell's material before it's dug out (mining jobs, blasts)
extern int firedampChance;   // setting firedamp: % of natural slate cells dug that let a pocket out
extern int firedampPocket;   // levels in a pocket
void VapourMinedRock(int x, int y, int z, int mat, bool natural);
int VapourFiredampPockets(void);
// A leak: `perSecond` levels of vapour come out at (x, y, z) for `seconds`
bool VapourLeak(int x, int y, int z, float perSecond, float seconds);
// Light the cloud at (x, y, z) if there's enough vapour there; true if it caught
bool VapourIgnite(int x, int y, int z);
// A spark at (x, y, z) after `seconds` (a match, a pipe being lit): lights vapour in that cell or
// within 2 of it on its level. False if all spark slots are taken
bool VapourSpark(int x, int y, int z, float seconds);

void VapourTick(float dt);   // SimStep

int VapourCellCount(void);      // cells with vapour now
int VapourTotal(void);          // sum of levels
int VapourBurningCount(void);   // cells the flame front is in now
int VapourBurnedCells(void);    // cells burned since the last ClearVapour
int VapourLeakCount(void);
bool VapourActive(void);        // any vapour or flame at all (cheap)
int VapourSparkCount(void);     // sparks waiting
int VapourVentCount(void);      // room edges blown out since the last ClearVapour
extern float vapourVentLeak;    // setting vapourVentLeak: per second, the share of a room's pressure that leaks away

#endif
