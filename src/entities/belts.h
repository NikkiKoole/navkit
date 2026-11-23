// belts.h - Conveyor belts (spike, see docs/todo/world/conveyors.md)
//
// A belt is a cell with a direction holding at most one item index: a loose stack or a
// container (whatever is inside a crate rides along). Items move one cell per 1/beltSpeed
// game seconds and only into a free cell, so a blocked end backs the line up.
//
// In: an empty belt takes an unreserved ground item lying on its cell; the first belt of a
// line (nothing feeds it) also takes a whole stack from the stockpile behind it (any slot).
// Out: past the last belt, a stockpile ahead takes the load into any slot that accepts it;
// no stockpile: it is dropped on the ground ahead if no ground item lies there. Else it waits.
// Items on a belt are ITEM_ON_BELT: not in the item spatial grid, so no job sees them.
//
// Saved since v98: saveload.c writes beltCount + belts[] (raw structs: changing Belt needs a
// save version and a migration); BeltsAfterLoad rebuilds the lookup and line order.
#ifndef BELTS_H
#define BELTS_H

#include <stdbool.h>

#define MAX_BELTS 4096

// UP/DOWN move the load to the cell above/below (a lift, or a chute when going down);
// the line goes on from whatever belt is there.
typedef enum { BELT_N, BELT_E, BELT_S, BELT_W, BELT_UP, BELT_DOWN } BeltDir;

typedef struct {
    short x, y;
    signed char z;
    unsigned char dir;      // BeltDir
    bool active;
    int item;               // item index riding this cell, -1 = empty
    float progress;         // 0 = entry edge, 0.5 = centre, 1 = exit edge
    int next;               // belt index this one feeds, -1 = line end (cached)
    bool fed;               // another belt feeds this one (cached): not a line start
} Belt;

extern Belt belts[MAX_BELTS];
extern int beltCount;          // high-water mark
extern float beltSpeed;        // cells per game second
extern float beltLiftFactor;   // speed of a lift (BELT_UP) as a fraction of beltSpeed: climbing is slow
extern float beltChuteFactor;  // speed of a chute (BELT_DOWN) as a multiple of beltSpeed: falling is quick
extern int beltItemsIn, beltItemsOut;   // counters since ClearBelts (scenario stats)

void ClearBelts(void);
int  PlaceBelt(int x, int y, int z, BeltDir dir);   // replaces the direction of an existing belt
void RemoveBelt(int x, int y, int z);               // drops what it carries on the ground
int  BeltAt(int x, int y, int z);                   // belt index or -1
void BeltsTick(float dt);
// After belts[] / beltCount were read from a save (or reset for an older save): rebuild the
// lookup and line order, drop belt slots whose item is gone, and put any ITEM_ON_BELT item no
// belt holds back on the ground (saves from before v98 could hold those).
void BeltsAfterLoad(void);
void BeltDirDelta(int dir, int* dx, int* dy);               // horizontal part (0, 0 for UP/DOWN)
void BeltDirDelta3(int dir, int* dx, int* dy, int* dz);

#endif
