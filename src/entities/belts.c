// belts.c - Conveyor belts (spike, see belts.h and docs/todo/world/conveyors.md)
#include "belts.h"
#include "items.h"
#include "containers.h"
#include "stockpiles.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include "../world/designations.h"
#include <string.h>

Belt belts[MAX_BELTS];
int beltCount = 0;
float beltSpeed = 1.5f;      // cells per second at dayLength 60, like trainSpeed
float beltLiftFactor = 0.4f;
float beltChuteFactor = 3.0f;
int beltItemsIn = 0, beltItemsOut = 0;

// Cell -> belt lookup: open addressing, rebuilt whenever belts are placed or removed
// (a few thousand belts at most, so a rebuild is cheap and there are no tombstones).
#define BELT_HASH_SIZE (MAX_BELTS * 2)
static int beltHash[BELT_HASH_SIZE];
// Processing order: downstream first, so a full line moves as one (the item ahead leaves
// its cell before the one behind wants in).
static int beltOrder[MAX_BELTS];
static int beltOrderCount = 0;

static unsigned BeltKey(int x, int y, int z) {
    return ((unsigned)z << 24) ^ ((unsigned)y << 12) ^ (unsigned)x;
}
static unsigned BeltSlot(unsigned key) { return (key * 2654435761u) & (BELT_HASH_SIZE - 1); }

void BeltDirDelta3(int dir, int* dx, int* dy, int* dz) {
    static const int DX[6] = {0, 1, 0, -1, 0, 0}, DY[6] = {-1, 0, 1, 0, 0, 0}, DZ[6] = {0, 0, 0, 0, 1, -1};
    if (dir < 0 || dir > BELT_DOWN) dir = BELT_N;
    *dx = DX[dir]; *dy = DY[dir]; *dz = DZ[dir];
}

void BeltDirDelta(int dir, int* dx, int* dy) {
    int dz;
    BeltDirDelta3(dir, dx, dy, &dz);
}

int BeltAt(int x, int y, int z) {
    unsigned key = BeltKey(x, y, z);
    for (unsigned s = BeltSlot(key), n = 0; n < BELT_HASH_SIZE; s = (s + 1) & (BELT_HASH_SIZE - 1), n++) {
        int b = beltHash[s];
        if (b < 0) return -1;
        if (belts[b].x == x && belts[b].y == y && belts[b].z == z) return b;
    }
    return -1;
}

static void RebuildBeltCaches(void) {
    for (int s = 0; s < BELT_HASH_SIZE; s++) beltHash[s] = -1;
    for (int b = 0; b < beltCount; b++) {
        if (!belts[b].active) continue;
        unsigned s = BeltSlot(BeltKey(belts[b].x, belts[b].y, belts[b].z));
        while (beltHash[s] >= 0) s = (s + 1) & (BELT_HASH_SIZE - 1);
        beltHash[s] = b;
    }
    for (int b = 0; b < beltCount; b++) belts[b].fed = false;
    for (int b = 0; b < beltCount; b++) {
        Belt* bl = &belts[b];
        bl->next = -1;
        if (!bl->active) continue;
        int dx, dy, dz;
        BeltDirDelta3(bl->dir, &dx, &dy, &dz);
        int n = BeltAt(bl->x + dx, bl->y + dy, bl->z + dz);
        // Two belts pointing at each other would trade items forever: treat as ends
        if (n >= 0) {
            int ndx, ndy, ndz;
            BeltDirDelta3(belts[n].dir, &ndx, &ndy, &ndz);
            if (ndx == -dx && ndy == -dy && ndz == -dz) n = -1;
        }
        bl->next = n;
        if (n >= 0) belts[n].fed = true;
    }
    // Order by distance to the line's end (0 = last belt), ascending. Loops have no end:
    // their distance is capped, order inside them is arbitrary (one gap circulates).
    static int dist[MAX_BELTS];
    int maxDist = 0;
    for (int b = 0; b < beltCount; b++) {
        dist[b] = 0;
        if (!belts[b].active) continue;
        int d = 0;
        for (int c = belts[b].next; c >= 0 && d < MAX_BELTS; c = belts[c].next) d++;
        dist[b] = d;
        if (d > maxDist) maxDist = d;
    }
    beltOrderCount = 0;
    // Counting sort by distance
    static int start[MAX_BELTS + 1];
    memset(start, 0, sizeof(int) * (size_t)(maxDist + 2));
    for (int b = 0; b < beltCount; b++) if (belts[b].active) start[dist[b] + 1]++;
    for (int d = 0; d <= maxDist; d++) start[d + 1] += start[d];
    for (int b = 0; b < beltCount; b++) if (belts[b].active) beltOrder[start[dist[b]]++] = b;
    beltOrderCount = start[maxDist];   // after the fill, start[maxDist] = number of active belts
}

void ClearBelts(void) {
    // Anything still riding goes back on the ground where it is
    for (int b = 0; b < beltCount; b++) {
        int it = belts[b].item;
        if (belts[b].active && it >= 0 && items[it].active && items[it].state == ITEM_ON_BELT)
            items[it].state = ITEM_ON_GROUND;
    }
    memset(belts, 0, sizeof(belts));
    beltCount = 0;
    beltItemsIn = beltItemsOut = 0;
    RebuildBeltCaches();
}

int PlaceBelt(int x, int y, int z, BeltDir dir) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return -1;
    int b = BeltAt(x, y, z);
    if (b < 0) {
        for (int i = 0; i < beltCount; i++) if (!belts[i].active) { b = i; break; }
        if (b < 0) {
            if (beltCount >= MAX_BELTS) return -1;
            b = beltCount++;
        }
        belts[b] = (Belt){ .x = (short)x, .y = (short)y, .z = (signed char)z, .active = true,
                           .item = -1, .progress = 0.0f, .next = -1 };
    }
    belts[b].dir = (unsigned char)dir;
    RebuildBeltCaches();
    return b;
}

void RemoveBelt(int x, int y, int z) {
    int b = BeltAt(x, y, z);
    if (b < 0) return;
    int it = belts[b].item;
    if (it >= 0 && items[it].active && items[it].state == ITEM_ON_BELT) {
        float cx = (x + 0.5f) * CELL_SIZE, cy = (y + 0.5f) * CELL_SIZE;
        SafeDropItem(it, cx, cy, z);
        if (items[it].contentCount > 0) MoveContainer(it, items[it].x, items[it].y, items[it].z);
    }
    belts[b].active = false;
    belts[b].item = -1;
    RebuildBeltCaches();
}

void BeltsAfterLoad(void) {
    if (beltCount < 0 || beltCount > MAX_BELTS) beltCount = 0;
    for (int b = beltCount; b < MAX_BELTS; b++) belts[b] = (Belt){ .item = -1, .next = -1 };
    static bool held[MAX_ITEMS];
    memset(held, 0, sizeof(held));
    for (int b = 0; b < beltCount; b++) {
        int it = belts[b].item;
        if (!belts[b].active || it < 0 || it >= MAX_ITEMS || !items[it].active || items[it].state != ITEM_ON_BELT)
            belts[b].item = -1;
        else held[it] = true;
    }
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].state == ITEM_ON_BELT && !held[i]) items[i].state = ITEM_ON_GROUND;
    beltItemsIn = beltItemsOut = 0;
    RebuildBeltCaches();
}

// Item position from the belt cell and progress across it
static void BeltItemPos(const Belt* bl, float progress, float* px, float* py) {
    int dx, dy;
    BeltDirDelta(bl->dir, &dx, &dy);
    *px = (bl->x + 0.5f + dx * (progress - 0.5f)) * CELL_SIZE;
    *py = (bl->y + 0.5f + dy * (progress - 0.5f)) * CELL_SIZE;
}

static void PutOnBelt(int b, int it, float progress) {
    Belt* bl = &belts[b];
    items[it].state = ITEM_ON_BELT;
    bl->item = it;
    bl->progress = progress;
    float px, py;
    BeltItemPos(bl, progress, &px, &py);
    items[it].z = (float)bl->z;
    if (items[it].contentCount > 0) MoveContainer(it, px, py, (float)bl->z);
    else { items[it].x = px; items[it].y = py; }
}

// An empty belt takes something: a ground item on its own cell, or (line start only) a
// whole stack from the stockpile slot behind it.
static bool BeltTakeInput(int b) {
    Belt* bl = &belts[b];
    int it = FindGroundItemAtTile(bl->x, bl->y, bl->z);
    if (it >= 0 && items[it].state == ITEM_ON_GROUND && items[it].reservedBy == -1
        && items[it].containedIn == -1) {
        PutOnBelt(b, it, 0.5f);
        beltItemsIn++;
        return true;
    }
    if (bl->fed) return false;
    int dx, dy, dz;
    BeltDirDelta3(bl->dir, &dx, &dy, &dz);
    int bx = bl->x - dx, by = bl->y - dy, bz = bl->z - dz;
    if (bz < 0 || bz >= gridDepth) return false;
    int sp;
    if (!IsPositionInStockpile((bx + 0.5f) * CELL_SIZE, (by + 0.5f) * CELL_SIZE, bz, &sp) || sp < 0)
        return false;
    // The stockpile is the pick-up point: any slot of it, the one right behind first
    Stockpile* s = &stockpiles[sp];
    int behind = (by - s->y) * s->width + (bx - s->x);
    int taken = StockpileTakeStack(sp, behind, NULL, NULL);
    if (taken < 0) return false;
    PutOnBelt(b, taken, 0.0f);
    beltItemsIn++;
    return true;
}

// Past the line's last belt (ahead, or above/below for UP/DOWN): into a stockpile there,
// or on the ground if there is room
static bool BeltDropOut(int b) {
    Belt* bl = &belts[b];
    int dx, dy, dz;
    BeltDirDelta3(bl->dir, &dx, &dy, &dz);
    int ox = bl->x + dx, oy = bl->y + dy, z = bl->z + dz;   // UP/DOWN: the cell above/below
    if (ox < 0 || oy < 0 || ox >= gridWidth || oy >= gridHeight || z < 0 || z >= gridDepth) return false;
    if (!IsCellWalkableAt(z, oy, ox)) return false;
    // A blueprint ahead (often the next belt of an unfinished line): wait, don't drop
    // the load on a building site
    if (HasBlueprint(ox, oy, z)) return false;
    int it = bl->item;
    float cx = (ox + 0.5f) * CELL_SIZE, cy = (oy + 0.5f) * CELL_SIZE;

    // A stockpile ahead is the drop-off point: the load goes into any slot of it that takes
    // it (a full or unwilling stockpile blocks the line)
    int sp;
    if (IsPositionInStockpile(cx, cy, z, &sp) && sp >= 0) {
        if (!StockpilePutLoad(sp, it)) return false;
        bl->item = -1;
        beltItemsOut++;
        return true;
    }

    if (FindGroundItemAtTile(ox, oy, z) >= 0) return false;
    items[it].state = ITEM_ON_GROUND;
    items[it].x = cx; items[it].y = cy; items[it].z = (float)z;
    if (items[it].contentCount > 0) MoveContainer(it, cx, cy, (float)z);
    bl->item = -1;
    beltItemsOut++;
    return true;
}

void BeltsTick(float dt) {
    if (beltOrderCount == 0) return;
    float gdt = dt * gameSpeed;
    if (gdt <= 0.0f) return;
    float step = beltSpeed * (60.0f / dayLength) * gdt;

    for (int o = 0; o < beltOrderCount; o++) {
        int b = beltOrder[o];
        Belt* bl = &belts[b];
        if (!bl->active) continue;
        int it = bl->item;
        // Item gone (eaten, deleted, picked off by a load): free the slot
        if (it >= 0 && (!items[it].active || items[it].state != ITEM_ON_BELT)) { bl->item = -1; it = -1; }
        if (it < 0) { BeltTakeInput(b); continue; }

        bl->progress += step * (bl->dir == BELT_UP ? beltLiftFactor : bl->dir == BELT_DOWN ? beltChuteFactor : 1.0f);
        if (bl->progress >= 1.0f) {
            float over = bl->progress - 1.0f;
            bool moved = false;
            if (bl->next >= 0) {
                Belt* nb = &belts[bl->next];
                if (nb->item < 0) {
                    bl->item = -1;
                    // One cell per step at most: progress is capped at the new cell's exit
                    PutOnBelt(bl->next, it, over < 1.0f ? over : 1.0f);
                    moved = true;
                }
            } else {
                moved = BeltDropOut(b);
            }
            if (!moved) bl->progress = 1.0f;   // blocked: wait at the exit edge
            if (moved) continue;
        }
        float px, py;
        BeltItemPos(bl, bl->progress, &px, &py);
        items[it].x = px; items[it].y = py;   // contents catch up on the next cell change
    }
}
