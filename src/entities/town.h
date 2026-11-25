// town.h - Town: roads and lots the player draws; buildings appear on the lots at once
// (docs/vision/town.md). Every citizen is a mover.
//
// Roads are axis-aligned rectangles of paved ground (the ground block below the walking
// level becomes concrete). Kept as a list, not a per-cell layer: a road query is a scan
// over the rectangles, and straight strips convert easily to the cars branch's road network.
//
// A lot is a rectangle next to a road, kind Home or Work (a bakery, for now: step 4). Placing one stamps a building
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
#define TOWN_MAX_LOT_WS 24
#define TOWN_MAX_LOT_STORES 4

typedef struct { int x1, y1, x2, y2, z; } TownRoad;   // z = the walking level on it

// Saved as a number: append only. LOT_WORK is the bakery.
typedef enum { LOT_HOME, LOT_WORK, LOT_MILL, LOT_FARM, LOT_GRANARY, LOT_KIND_COUNT } LotKind;
#define TownIsWorkplace(k) ((k) == LOT_WORK || (k) == LOT_MILL)   // has jobs, opening hours
typedef enum { SIDE_N, SIDE_E, SIDE_S, SIDE_W } TownSide;

typedef struct {
    bool active;
    LotKind kind;
    int x1, y1, x2, y2, z;   // the lot (inclusive), z = walking level
    TownSide front;          // the side facing the road
    int doorX, doorY;        // the front door cell
    int beds;                // homes: bed count; work: work spots (desks)
    int furn[TOWN_MAX_LOT_FURN], furnCount;   // furniture this lot's building placed
    int fridge;              // homes: the fridge's furniture index (-1 none)
    int stockpile;           // homes: the bread stockpile on the fridge (-1 none)
    int ws[TOWN_MAX_LOT_WS], wsCount;   // work: the bakery's stoves and counters (workshops)
    int stores[TOWN_MAX_LOT_STORES], storeCount;   // bakery: flour store, mill: granary, farm: seed store
    int supplyX, supplyY;    // bakery / mill: where flour / wheat from outside town is delivered
    int supplyItem;          // the delivered stack (-1 none; checked before use)
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
// A Granary lot is storage for the winter: its whole floor stores wheat and flour (they keep;
// bread spoils in ~5 days), ranked between the farm's barn and the mill's granary, so a surplus
// harvest moves in and flows on to the mill as it frees space.
//
// The grain chain (step 6): farm -> mill -> bakery -> pantries, all carried by the haul jobs.
// A Farm lot is a wheat field (the farming designations) with a seed store; a Mill has querns
// (Grind Wheat) and a granary. A bakery or mill that has run out imports a batch from outside
// (counted in townImported[]): the town never locks up, and a weak chain shows in the count.
//
// Food (step 4): a Work lot is a bakery: stoves + counters bake bread ("Bake Bread", a bill
// that runs until there's no room), flour arrives from outside town at its delivery spot.
// Each home's fridge is a one-cell bread stockpile: the haul jobs carry bread home.
//
// Residents (step 3): a home with a free bed draws a newcomer, who arrives on a road at the
// map edge and walks home. Each bed in a home is private: its owner's, or (unassigned) nobody's.
// ---------------------------------------------------------------------------

#define TOWN_OWNER_PUBLIC   -1   // anyone may use it (every furniture outside town homes)
#define TOWN_OWNER_NOBODY   -2   // a home's bed with no owner yet: kept for the next newcomer

extern float townArrivalGH;      // game hours between newcomers (setting)
extern int townBreadPerHome;     // bakeries bake until the pantries hold this many loaves per home (setting)
extern int townFlourTarget;      // mills grind until the bakeries' stores hold this much flour (setting)
extern bool townIrrigation;      // town farms are kept wet (stand-in for a well + watering; setting)
extern int townGroceries;        // loaves a newcomer brings for the pantry (setting)
extern int townOpenHour;         // bakeries open at this hour (setting) ...
extern int townCloseHour;        // ... and close at this one
extern int townFlourDelivery;    // flour a bakery imports when it has run out (setting; 0 = never)
extern int townWheatDelivery;    // wheat a mill imports when it has run out (setting; 0 = never)
extern int townStarter;          // homes already standing when the town game starts (setting)

extern void (*townSpawnMover)(int x, int y, int z);   // main.c: SpawnMoverAt

void TownTick(float dt);
void TownNoteHarvest(int x, int y, int z);        // jobs.c: a crop was harvested here
bool TownMakesFlour(void);                        // a town mill has ground flour
bool TownMakesWheat(void);                        // a town farm has harvested
extern int townImported[];                        // emergency imports per item type (not saved)
extern int townWasted[];                          // rotten food thrown out per item type (not saved)
// One line for headless runs and the console: people, homes, jobs, stocks, imports
void TownSummary(char* buf, int len);
int  TownStoreCapacity(void);                     // wheat + flour the town's stores can hold
// For the lot inspector (describe.c DescribeLot)
int  TownLotStock(int lot, int itemType);         // units of a type in the lot's stores (and pantry)
int  TownLotStoreRoom(int lot);                   // units the lot's stores hold when full
typedef struct { int fields, tilled, waiting, growing, ripe, fertilitySum, heavyWeeds; } TownFarmStats;
TownFarmStats TownFarmStatsOf(int lot);
int  TownTotalStock(int itemType);                // units of a type anywhere in town                          // arrivals, leaving owners, flour deliveries
int  TownHomeOf(int moverIdx);                    // lot index, -1 = no home
int  TownResidents(int lot);                      // movers who live there
int  TownFreeBeds(void);                          // unowned beds in all homes
int  TownHomeless(void);                          // active movers without a home (town only)
// Save/load (v113, after relations). Lots are saved as geometry + the bed owners in cell order;
// their furniture, workshops, pantry and flour stack are found again from the cells after a load.
#include <stdio.h>
void TownSave(FILE* f);
bool TownLoad(FILE* f);                // false: corrupt counts
bool TownSkipSave(FILE* f);            // inspect.c: read past the town section

// Jobs (step 4b): each bakery has a job per stove; a resident without a job takes a free one.
// A bakery's stoves are worked only by its own workers, and only while it is open.
int  TownJobOf(int moverIdx);                     // lot index, -1 = no job
int  TownWorkers(int lot);                        // movers with a job there
int  TownFreeJobs(void);
int  TownUnemployed(void);                        // residents (with a home) without a job
bool TownLotOpen(int lot);                        // a workplace's opening hours, now
// May this mover take a craft job at this workshop? (jobs.c WorkGiver_Craft). Cheap when no town.
bool TownMayCraftAt(int workshopIdx, int moverIdx);
// May this mover take a job whose target is this cell? (jobs.c JobInRange, which every WorkGiver
// calls). A worker on shift (their workplace open) only works inside their workplace; off shift,
// and anyone without a job, anywhere. Cheap when no town.
bool TownJobAllowed(int moverIdx, int x, int y, int z);

// May this mover rest on / use this furniture? (needs.c bed search). Cheap when no town.
bool TownMayUseFurniture(int furnIdx, int moverIdx);

#endif
