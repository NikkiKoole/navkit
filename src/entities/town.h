// town.h - Town: roads and lots the player draws; buildings appear on the lots at once
// (docs/vision/town.md). Every citizen is a mover.
//
// Roads are axis-aligned rectangles of paved ground (the ground block below the walking
// level becomes concrete). Kept as a list, not a per-cell layer: a road query is a scan
// over the rectangles, and straight strips convert easily to the cars branch's road network.
//
// A lot is a rectangle next to a road, kind Home or Work. Placing one stamps a building
// from a template that fits: a front yard row on the road side, walls round the rest, the
// door facing the road, furniture inside, a roof. Removing it clears the cells again.
#ifndef TOWN_H
#define TOWN_H

#include <stdbool.h>

#define TOWN_MAX_ROADS 1024
#define TOWN_MAX_LOTS 1024
#define TOWN_LOT_MIN 5          // a lot is at least 5x5 cells (yard row + walls + a 3x2 inside)
#define TOWN_LOT_MAX 24
#define TOWN_MAX_LOT_FURN 32

typedef struct { int x1, y1, x2, y2, z; } TownRoad;   // z = the walking level on it

typedef enum { LOT_HOME, LOT_WORK, LOT_KIND_COUNT } LotKind;
typedef enum { SIDE_N, SIDE_E, SIDE_S, SIDE_W } TownSide;

typedef struct {
    bool active;
    LotKind kind;
    int x1, y1, x2, y2, z;   // the lot (inclusive), z = walking level
    TownSide front;          // the side facing the road
    int doorX, doorY;        // the front door cell
    int beds;                // homes: bed count; work: work spots (desks)
    int furn[TOWN_MAX_LOT_FURN], furnCount;   // furniture this lot's building placed
    int fridge;              // homes: the fridge's furniture index (-1 none); food is restocked on it
    int foodItem;            // homes: the restocked food stack (-1 none; checked before use)
} TownLot;

extern TownRoad townRoads[TOWN_MAX_ROADS];
extern int townRoadCount;
extern TownLot townLots[TOWN_MAX_LOTS];
extern int townLotHighWater;   // lots [0, townLotHighWater) may be active

const char* TownLotKindName(LotKind k);

void ClearTown(void);   // forgets roads and lots (the world itself is reset separately)

// Roads. Add paves the ground; returns the road index or -1 (*why says why).
int  TownAddRoad(int x1, int y1, int x2, int y2, int z, const char** why);
int  TownRemoveRoadsIn(int x1, int y1, int x2, int y2, int z);   // whole roads touching the rect; count
bool TownIsRoad(int x, int y, int z);

// Lots. Add builds the building at once; returns the lot index or -1 (*why says why).
int  TownAddLot(LotKind kind, int x1, int y1, int x2, int y2, int z, const char** why);
void TownRemoveLot(int lot);
int  TownRemoveLotsIn(int x1, int y1, int x2, int y2, int z);   // count removed
int  TownLotAt(int x, int y, int z);                             // -1 = none
int  TownLotCount(LotKind kind);                                  // active lots of a kind

// ---------------------------------------------------------------------------
// Residents (step 3): a home with a free bed draws a newcomer, who arrives on a road at the
// map edge and walks home. Each bed in a home is private: its owner's, or (unassigned) nobody's.
// ---------------------------------------------------------------------------

#define TOWN_OWNER_PUBLIC   -1   // anyone may use it (every furniture outside town homes)
#define TOWN_OWNER_NOBODY   -2   // a home's bed with no owner yet: kept for the next newcomer

extern float townArrivalGH;      // game hours between newcomers (setting)
extern int townMealsPerResident; // restocked food on a home's fridge per resident (setting)
extern int townStarter;          // homes already standing when the town game starts (setting)

extern void (*townSpawnMover)(int x, int y, int z);   // main.c: SpawnMoverAt

void TownTick(float dt);                          // arrivals, leaving owners, restocking
int  TownHomeOf(int moverIdx);                    // lot index, -1 = no home
int  TownResidents(int lot);                      // movers who live there
int  TownFreeBeds(void);                          // unowned beds in all homes
int  TownHomeless(void);                          // active movers without a home (town only)
// May this mover rest on / use this furniture? (needs.c bed search). Cheap when no town.
bool TownMayUseFurniture(int furnIdx, int moverIdx);

#endif
