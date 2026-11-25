// town.c - Town: roads, lots and the buildings stamped on them (town.h, docs/vision/town.md)

#include "town.h"
#include "../world/grid.h"
#include "../world/material.h"
#include <stdlib.h>
#include "furniture.h"
#include "mover.h"
#include "items.h"
#include "action_queue.h"
#include "../simulation/balance.h"
#include "../core/time.h"
#include "../simulation/mood.h"
#include <string.h>

TownRoad townRoads[TOWN_MAX_ROADS];
int townRoadCount = 0;
TownLot townLots[TOWN_MAX_LOTS];
int townLotHighWater = 0;
float townArrivalGH = 0.5f;
int townMealsPerResident = 3;
int townStarter = 0;

static short townFurnOwner[MAX_FURNITURE];   // TOWN_OWNER_* or a mover index (beds of homes)
static short townHome[MAX_MOVERS];           // lot a mover lives in, -1 none
static float townHousekeepTimer = 0.0f;
static float townArrivalTimer = 0.0f;

const char* TownLotKindName(LotKind k) {
    switch (k) {
        case LOT_HOME: return "Home";
        case LOT_WORK: return "Work";
        default: return "?";
    }
}

void ClearTown(void) {
    townRoadCount = 0;
    townLotHighWater = 0;
    memset(townLots, 0, sizeof(townLots));
    for (int i = 0; i < MAX_FURNITURE; i++) townFurnOwner[i] = TOWN_OWNER_PUBLIC;
    for (int i = 0; i < MAX_MOVERS; i++) townHome[i] = -1;
    townHousekeepTimer = 0.0f;
    townArrivalTimer = 0.0f;
}

static void TownNormRect(int* x1, int* y1, int* x2, int* y2) {
    if (*x1 > *x2) { int t = *x1; *x1 = *x2; *x2 = t; }
    if (*y1 > *y2) { int t = *y1; *y1 = *y2; *y2 = t; }
}

static bool TownRectsTouch(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2) {
    return ax1 <= bx2 && bx1 <= ax2 && ay1 <= by2 && by1 <= ay2;
}

// Flat ground a road or lot can go on: air at z on a solid block
static bool TownGroundOk(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || z < 1 || z >= gridDepth) return false;
    return GRID(x, y, z) == CELL_AIR && !HAS_FLOOR(x, y, z) && GRID(x, y, z - 1) == CELL_WALL
        && GetFurnitureAt(x, y, z) < 0;
}

static bool TownMoverIn(int x1, int y1, int x2, int y2, int z) {
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active || (int)movers[i].z != z) continue;
        int mx = (int)(movers[i].x / CELL_SIZE), my = (int)(movers[i].y / CELL_SIZE);
        if (mx >= x1 && mx <= x2 && my >= y1 && my <= y2) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Roads
// ---------------------------------------------------------------------------

bool TownIsRoad(int x, int y, int z) {
    for (int r = 0; r < townRoadCount; r++) {
        const TownRoad* rd = &townRoads[r];
        if (rd->z == z && x >= rd->x1 && x <= rd->x2 && y >= rd->y1 && y <= rd->y2) return true;
    }
    return false;
}

static void TownPave(int x, int y, int z) {
    SetWallMaterial(x, y, z - 1, MAT_CONCRETE);
    ClearWallNatural(x, y, z - 1);
    SetWallFinish(x, y, z - 1, FINISH_SMOOTH);
    SetVegetation(x, y, z - 1, VEG_NONE);
}

static void TownUnpave(int x, int y, int z) {
    SetWallMaterial(x, y, z - 1, MAT_DIRT);
    SetWallNatural(x, y, z - 1);
    SetWallFinish(x, y, z - 1, FINISH_ROUGH);
    SetVegetation(x, y, z - 1, VEG_GRASS_SHORT);
}

int TownAddRoad(int x1, int y1, int x2, int y2, int z, const char** why) {
    const char* dummy;
    if (!why) why = &dummy;
    TownNormRect(&x1, &y1, &x2, &y2);
    if (townRoadCount >= TOWN_MAX_ROADS) { *why = "too many roads"; return -1; }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= gridWidth) x2 = gridWidth - 1;
    if (y2 >= gridHeight) y2 = gridHeight - 1;
    if (x1 > x2 || y1 > y2) { *why = "off the map"; return -1; }
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++) {
            if (TownLotAt(x, y, z) >= 0) { *why = "a lot is in the way"; return -1; }
            if (!TownIsRoad(x, y, z) && !TownGroundOk(x, y, z)) { *why = "not flat open ground"; return -1; }
        }
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++) TownPave(x, y, z);
    townRoads[townRoadCount] = (TownRoad){x1, y1, x2, y2, z};
    return townRoadCount++;
}

int TownRemoveRoadsIn(int x1, int y1, int x2, int y2, int z) {
    TownNormRect(&x1, &y1, &x2, &y2);
    int removed = 0;
    for (int r = townRoadCount - 1; r >= 0; r--) {
        TownRoad rd = townRoads[r];
        if (rd.z != z || !TownRectsTouch(x1, y1, x2, y2, rd.x1, rd.y1, rd.x2, rd.y2)) continue;
        townRoads[r] = townRoads[--townRoadCount];
        for (int y = rd.y1; y <= rd.y2; y++)
            for (int x = rd.x1; x <= rd.x2; x++)
                if (!TownIsRoad(x, y, z)) TownUnpave(x, y, z);   // another road may cover it
        removed++;
    }
    return removed;
}

// ---------------------------------------------------------------------------
// Lots: local coordinates u (along the front wall, 0..w-1) and v (depth from the
// front wall's inside, 0..d-1) of the building's inside, turned to face the road.
// ---------------------------------------------------------------------------

// The building: the lot minus the front (yard) row
static void TownBuildingRect(const TownLot* L, int* bx1, int* by1, int* bx2, int* by2) {
    *bx1 = L->x1; *by1 = L->y1; *bx2 = L->x2; *by2 = L->y2;
    switch (L->front) {
        case SIDE_N: (*by1)++; break;
        case SIDE_S: (*by2)--; break;
        case SIDE_W: (*bx1)++; break;
        case SIDE_E: (*bx2)--; break;
    }
}

static void TownInsideSize(const TownLot* L, int* w, int* d) {
    int bx1, by1, bx2, by2;
    TownBuildingRect(L, &bx1, &by1, &bx2, &by2);
    int iw = bx2 - bx1 - 1, ih = by2 - by1 - 1;   // inside the walls
    bool frontNS = L->front == SIDE_N || L->front == SIDE_S;
    *w = frontNS ? iw : ih;
    *d = frontNS ? ih : iw;
}

static void TownLocalToWorld(const TownLot* L, int u, int v, int* x, int* y) {
    int bx1, by1, bx2, by2;
    TownBuildingRect(L, &bx1, &by1, &bx2, &by2);
    int ix1 = bx1 + 1, iy1 = by1 + 1, ix2 = bx2 - 1, iy2 = by2 - 1;
    switch (L->front) {
        case SIDE_N: *x = ix1 + u; *y = iy1 + v; break;
        case SIDE_S: *x = ix2 - u; *y = iy2 - v; break;
        case SIDE_W: *x = ix1 + v; *y = iy2 - u; break;
        case SIDE_E: default: *x = ix2 - v; *y = iy1 + u; break;
    }
}

// Road cells just outside one side of the rect
static int TownRoadAlong(int x1, int y1, int x2, int y2, int z, TownSide s) {
    int n = 0;
    if (s == SIDE_N || s == SIDE_S) {
        int y = s == SIDE_N ? y1 - 1 : y2 + 1;
        for (int x = x1; x <= x2; x++) n += TownIsRoad(x, y, z);
    } else {
        int x = s == SIDE_W ? x1 - 1 : x2 + 1;
        for (int y = y1; y <= y2; y++) n += TownIsRoad(x, y, z);
    }
    return n;
}

int TownLotAt(int x, int y, int z) {
    for (int i = 0; i < townLotHighWater; i++) {
        const TownLot* L = &townLots[i];
        if (L->active && L->z == z && x >= L->x1 && x <= L->x2 && y >= L->y1 && y <= L->y2) return i;
    }
    return -1;
}

int TownLotCount(LotKind kind) {
    int n = 0;
    for (int i = 0; i < townLotHighWater; i++) n += townLots[i].active && townLots[i].kind == kind;
    return n;
}

static void TownLotFurn(TownLot* L, int u, int v, FurnitureType type, uint8_t mat) {
    if (L->furnCount >= TOWN_MAX_LOT_FURN) return;
    int x, y;
    TownLocalToWorld(L, u, v, &x, &y);
    if (GetFurnitureAt(x, y, L->z) >= 0) return;
    int f = SpawnFurniture(x, y, L->z, type, mat);
    if (f < 0) return;
    L->furn[L->furnCount++] = f;
    townFurnOwner[f] = (L->kind == LOT_HOME && GetFurnitureDef(type)->restRate >= 1.0f) ? TOWN_OWNER_NOBODY : TOWN_OWNER_PUBLIC;
    if (type == FURNITURE_FRIDGE) L->fridge = f;
}

// Walls, door, windows, floor, roof
static void TownStampShell(TownLot* L) {
    int bx1, by1, bx2, by2, z = L->z;
    TownBuildingRect(L, &bx1, &by1, &bx2, &by2);
    MaterialType wallMat = L->kind == LOT_HOME ? MAT_BRICK : MAT_CONCRETE;
    for (int y = by1; y <= by2; y++)
        for (int x = bx1; x <= bx2; x++) {
            SET_FLOOR(x, y, z);
            SetFloorMaterial(x, y, z, L->kind == LOT_HOME ? MAT_OAK : MAT_CONCRETE);
            ClearFloorNatural(x, y, z);
            SetVegetation(x, y, z - 1, VEG_NONE);
            if (z + 1 < gridDepth) {
                SET_FLOOR(x, y, z + 1);
                SetFloorMaterial(x, y, z + 1, MAT_SLATE);
                ClearFloorNatural(x, y, z + 1);
                MarkChunkDirty(x, y, z + 1);
            }
            bool edge = x == bx1 || x == bx2 || y == by1 || y == by2;
            if (!edge) { MarkChunkDirty(x, y, z); continue; }
            CellPlacementSpec spec = {
                .cellType = CELL_WALL, .wallMat = wallMat, .wallNatural = false,
                .wallFinish = FINISH_SMOOTH, .clearFloor = false, .clearWater = true,
                .surfaceType = SURFACE_BARE
            };
            PlaceCellFull(x, y, z, spec);
        }
    // Door in the middle of the front wall; windows every third cell of the other walls
    int w, d;
    TownInsideSize(L, &w, &d);
    int doorU = w / 2, dx, dy;
    TownLocalToWorld(L, doorU, 0, &dx, &dy);
    switch (L->front) {   // one step out of the inside = the wall cell
        case SIDE_N: dy--; break;
        case SIDE_S: dy++; break;
        case SIDE_W: dx--; break;
        case SIDE_E: dx++; break;
    }
    GRID(dx, dy, z) = CELL_DOOR;
    L->doorX = dx; L->doorY = dy;
    // Pave the yard cell in front of the door (a path to the road)
    int px = dx, py = dy;
    switch (L->front) {
        case SIDE_N: py--; break;
        case SIDE_S: py++; break;
        case SIDE_W: px--; break;
        case SIDE_E: px++; break;
    }
    TownPave(px, py, z);
    for (int y = by1; y <= by2; y++)
        for (int x = bx1; x <= bx2; x++) {
            bool corner = (x == bx1 || x == bx2) && (y == by1 || y == by2);
            if (corner || GRID(x, y, z) != CELL_WALL) continue;
            if ((x + y) % 3 != 0) continue;
            if (abs(x - dx) + abs(y - dy) <= 1) continue;
            GRID(x, y, z) = CELL_WINDOW;
            SetWallMaterial(x, y, z, MAT_GLASS);
        }
}

// Furniture. The aisle (u = doorU) stays free from the door to the back wall.
static void TownFurnishHome(TownLot* L) {
    int w, d;
    TownInsideSize(L, &w, &d);
    int doorU = w / 2;
    // Beds along the back wall (every other cell), then up the side walls
    int beds = (w * d) / 6;
    if (beds < 1) beds = 1;
    if (beds > 6) beds = 6;
    int placed = 0;
    for (int u = 0; u < w && placed < beds; u += 2) {
        if (u == doorU) { u--; continue; }
        TownLotFurn(L, u, d - 1, FURNITURE_PLANK_BED, MAT_OAK); placed++;
    }
    for (int v = d - 3; v >= 1 && placed < beds; v -= 2) {
        if (doorU != 0) { TownLotFurn(L, 0, v, FURNITURE_PLANK_BED, MAT_OAK); placed++; }
        if (placed < beds && doorU != w - 1) { TownLotFurn(L, w - 1, v, FURNITURE_PLANK_BED, MAT_OAK); placed++; }
    }
    L->beds = placed;
    // Kitchen and toilet along the front wall, either side of the door
    TownLotFurn(L, doorU - 1 >= 0 ? doorU - 1 : w - 1, 0, FURNITURE_TAP, MAT_GRANITE);
    TownLotFurn(L, doorU + 1 < w ? doorU + 1 : 0, 0, FURNITURE_FRIDGE, MAT_IRON);
    if (w >= 5) TownLotFurn(L, w - 1, 0, FURNITURE_TOILET, MAT_GRANITE);
    else if (d >= 3) TownLotFurn(L, w - 1, 1, FURNITURE_TOILET, MAT_GRANITE);
    if (w >= 5 && d >= 3) {
        TownLotFurn(L, 0, 0, FURNITURE_TABLE, MAT_OAK);
        TownLotFurn(L, 0, 1, FURNITURE_CHAIR, MAT_OAK);
    }
}

static void TownFurnishWork(TownLot* L) {
    int w, d;
    TownInsideSize(L, &w, &d);
    int doorU = w / 2, spots = 0;
    // Desks along the back wall, a chair in front of each
    for (int u = 0; u < w; u++) {
        if (u == doorU) continue;
        TownLotFurn(L, u, d - 1, FURNITURE_DESK, MAT_OAK);
        if (d >= 3) TownLotFurn(L, u, d - 2, FURNITURE_CHAIR, MAT_OAK);
        spots++;
    }
    L->beds = spots;
    TownLotFurn(L, doorU - 1 >= 0 ? doorU - 1 : w - 1, 0, FURNITURE_TAP, MAT_GRANITE);
    if (w >= 4) TownLotFurn(L, w - 1, 0, FURNITURE_TOILET, MAT_GRANITE);
}

int TownAddLot(LotKind kind, int x1, int y1, int x2, int y2, int z, const char** why) {
    const char* dummy;
    if (!why) why = &dummy;
    TownNormRect(&x1, &y1, &x2, &y2);
    int w = x2 - x1 + 1, h = y2 - y1 + 1;
    if (w < TOWN_LOT_MIN || h < TOWN_LOT_MIN) { *why = "a lot is at least 5x5"; return -1; }
    if (w > TOWN_LOT_MAX || h > TOWN_LOT_MAX) { *why = "a lot is at most 24x24"; return -1; }
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++) {
            if (TownIsRoad(x, y, z)) { *why = "a road is in the way"; return -1; }
            if (TownLotAt(x, y, z) >= 0) { *why = "another lot is in the way"; return -1; }
            if (!TownGroundOk(x, y, z)) { *why = "not flat open ground"; return -1; }
        }
    if (TownMoverIn(x1, y1, x2, y2, z)) { *why = "someone is standing there"; return -1; }
    // Front: the side with the most road along it
    TownSide best = SIDE_N;
    int bestN = 0;
    for (int s = SIDE_N; s <= SIDE_W; s++) {
        int n = TownRoadAlong(x1, y1, x2, y2, z, (TownSide)s);
        if (n > bestN) { bestN = n; best = (TownSide)s; }
    }
    if (bestN == 0) { *why = "a lot must lie along a road"; return -1; }
    int slot = -1;
    for (int i = 0; i < townLotHighWater; i++) if (!townLots[i].active) { slot = i; break; }
    if (slot < 0) {
        if (townLotHighWater >= TOWN_MAX_LOTS) { *why = "too many lots"; return -1; }
        slot = townLotHighWater++;
    }
    TownLot* L = &townLots[slot];
    memset(L, 0, sizeof(*L));
    *L = (TownLot){ .active = true, .kind = kind, .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2, .z = z, .front = best,
                    .fridge = -1, .foodItem = -1 };
    TownStampShell(L);
    if (kind == LOT_HOME) TownFurnishHome(L);
    else TownFurnishWork(L);
    return slot;
}

void TownRemoveLot(int lot) {
    if (lot < 0 || lot >= townLotHighWater || !townLots[lot].active) return;
    TownLot* L = &townLots[lot];
    for (int i = 0; i < MAX_MOVERS; i++) if (townHome[i] == lot) townHome[i] = -1;   // homeless now
    for (int i = 0; i < L->furnCount; i++) {
        int f = L->furn[i];
        townFurnOwner[f] = TOWN_OWNER_PUBLIC;
        if (!furniture[f].active) continue;
        if (furniture[f].occupant >= 0) ReleaseFurniture(f, furniture[f].occupant);
        RemoveFurniture(f);
    }
    int z = L->z;
    for (int y = L->y1; y <= L->y2; y++)
        for (int x = L->x1; x <= L->x2; x++) {
            if (GRID(x, y, z) != CELL_AIR || HAS_FLOOR(x, y, z)) {
                CellPlacementSpec spec = {
                    .cellType = CELL_AIR, .wallMat = MAT_NONE, .wallNatural = false,
                    .wallFinish = FINISH_ROUGH, .clearFloor = true, .floorMat = MAT_NONE,
                    .clearWater = false, .surfaceType = SURFACE_BARE
                };
                PlaceCellFull(x, y, z, spec);
            }
            if (z + 1 < gridDepth && HAS_FLOOR(x, y, z + 1)) {
                CLEAR_FLOOR(x, y, z + 1);
                SetFloorMaterial(x, y, z + 1, MAT_NONE);
                MarkChunkDirty(x, y, z + 1);
            }
            TownUnpave(x, y, z);
        }
    L->active = false;
    while (townLotHighWater > 0 && !townLots[townLotHighWater - 1].active) townLotHighWater--;
}

int TownRemoveLotsIn(int x1, int y1, int x2, int y2, int z) {
    TownNormRect(&x1, &y1, &x2, &y2);
    int removed = 0;
    for (int i = townLotHighWater - 1; i >= 0; i--) {
        const TownLot* L = &townLots[i];
        if (!L->active || L->z != z || !TownRectsTouch(x1, y1, x2, y2, L->x1, L->y1, L->x2, L->y2)) continue;
        TownRemoveLot(i);
        removed++;
    }
    return removed;
}

// ---------------------------------------------------------------------------
// Residents
// ---------------------------------------------------------------------------

int TownHomeOf(int moverIdx) {
    return (moverIdx >= 0 && moverIdx < MAX_MOVERS) ? townHome[moverIdx] : -1;
}

bool TownMayUseFurniture(int furnIdx, int moverIdx) {
    if (townLotHighWater == 0 || furnIdx < 0 || furnIdx >= MAX_FURNITURE) return true;
    int owner = townFurnOwner[furnIdx];
    return owner == TOWN_OWNER_PUBLIC || owner == moverIdx;
}

int TownResidents(int lot) {
    const TownLot* L = &townLots[lot];
    int n = 0;
    for (int i = 0; i < L->furnCount; i++) n += townFurnOwner[L->furn[i]] >= 0;
    return n;
}

int TownFreeBeds(void) {
    int n = 0;
    for (int l = 0; l < townLotHighWater; l++) {
        const TownLot* L = &townLots[l];
        if (!L->active || L->kind != LOT_HOME) continue;
        for (int i = 0; i < L->furnCount; i++) n += townFurnOwner[L->furn[i]] == TOWN_OWNER_NOBODY;
    }
    return n;
}

int TownHomeless(void) {
    int n = 0;
    for (int i = 0; i < moverCount; i++) n += movers[i].active && townHome[i] < 0;
    return n;
}

// A walkable road cell on the map edge: where newcomers arrive
static bool TownEntryCell(int* ox, int* oy, int* oz) {
    for (int r = 0; r < townRoadCount; r++) {
        const TownRoad* rd = &townRoads[r];
        for (int y = rd->y1; y <= rd->y2; y++)
            for (int x = rd->x1; x <= rd->x2; x++) {
                if (x != 0 && y != 0 && x != gridWidth - 1 && y != gridHeight - 1) continue;
                if (!IsCellWalkableAt(rd->z, y, x)) continue;
                *ox = x; *oy = y; *oz = rd->z;
                return true;
            }
    }
    return false;
}

// Give a mover a home: the first unowned bed. Returns the lot or -1.
static int TownGiveHome(int moverIdx) {
    for (int l = 0; l < townLotHighWater; l++) {
        TownLot* L = &townLots[l];
        if (!L->active || L->kind != LOT_HOME) continue;
        for (int i = 0; i < L->furnCount; i++) {
            if (townFurnOwner[L->furn[i]] != TOWN_OWNER_NOBODY) continue;
            townFurnOwner[L->furn[i]] = (short)moverIdx;
            townHome[moverIdx] = (short)l;
            return l;
        }
    }
    return -1;
}

// Walk home: to the cell just inside the front door
static void TownSendHome(int moverIdx, int lot) {
    const TownLot* L = &townLots[lot];
    int x, y, w, d;
    TownInsideSize(L, &w, &d);
    TownLocalToWorld(L, w / 2, 0, &x, &y);
    char said[96];
    QueueCommandAt(moverIdx, x, y, L->z, false, said, sizeof(said));
}

// Spawning a newcomer: the game's SpawnMoverAt (names, looks: main.c sets the hook); the
// tests, which don't build main.c, get a plain mover
static void TownSpawnFallback(int x, int y, int z) {
    if (moverCount >= MAX_MOVERS || !IsCellWalkableAt(z, y, x)) return;
    Mover* m = &movers[moverCount];
    InitMover(m, x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, (float)z, (Point){x, y, z}, balance.baseMoverSpeed);
    AssignRandomTraits(m);
    moverCount++;
}
void (*townSpawnMover)(int x, int y, int z) = TownSpawnFallback;

static void TownArrive(void) {
    int ex, ey, ez;
    if (TownFreeBeds() == 0 || !TownEntryCell(&ex, &ey, &ez)) return;
    int before = moverCount;
    townSpawnMover(ex, ey, ez);
    if (moverCount == before) return;
    int m = moverCount - 1;
    int lot = TownGiveHome(m);
    if (lot >= 0) TownSendHome(m, lot);
    EventLog("Town: mover %d arrives, home lot %d", m, lot);
}

// Restock a home's fridge (stand-in for shops: docs/vision/town.md)
static void TownRestock(TownLot* L) {
    int residents = TownResidents((int)(L - townLots));
    if (residents == 0 || L->fridge < 0 || !furniture[L->fridge].active) return;
    const Furniture* f = &furniture[L->fridge];
    int want = residents * townMealsPerResident;
    if (want <= 0) return;
    if (L->foodItem >= 0) {
        const Item* it = &items[L->foodItem];
        bool still = it->active && it->type == ITEM_BREAD && (int)(it->x / CELL_SIZE) == f->x
                  && (int)(it->y / CELL_SIZE) == f->y && (int)it->z == f->z && it->state == ITEM_ON_GROUND;
        if (still && it->stackCount * 2 > want) return;   // more than half left
        if (still && it->reservedBy < 0) { items[L->foodItem].stackCount = want; return; }
        if (still) return;   // someone is taking from it: next time
    }
    int idx = SpawnItem(f->x * CELL_SIZE + CELL_SIZE * 0.5f, f->y * CELL_SIZE + CELL_SIZE * 0.5f, (float)f->z, ITEM_BREAD);
    if (idx >= 0) items[idx].stackCount = want;
    L->foodItem = idx;
}

void TownTick(float dt) {
    if (townLotHighWater == 0) return;
    townArrivalTimer += dt;
    townHousekeepTimer += dt;
    float housekeep = GameHoursToGameSeconds(0.1f);
    if (townHousekeepTimer < housekeep) return;
    townHousekeepTimer = 0.0f;
    // Owners who are gone free their bed
    for (int l = 0; l < townLotHighWater; l++) {
        TownLot* L = &townLots[l];
        if (!L->active || L->kind != LOT_HOME) continue;
        for (int i = 0; i < L->furnCount; i++) {
            int o = townFurnOwner[L->furn[i]];
            if (o >= 0 && (o >= moverCount || !movers[o].active)) {
                townFurnOwner[L->furn[i]] = TOWN_OWNER_NOBODY;
                if (o < MAX_MOVERS) townHome[o] = -1;
            }
        }
    }
    // Homeless movers already in town take a free bed first, then newcomers arrive
    for (int i = 0; i < moverCount && TownFreeBeds() > 0; i++)
        if (movers[i].active && townHome[i] < 0) {
            int lot = TownGiveHome(i);
            if (lot >= 0) TownSendHome(i, lot);
        }
    if (townArrivalTimer >= GameHoursToGameSeconds(townArrivalGH)) {
        townArrivalTimer = 0.0f;
        TownArrive();
    }
    for (int l = 0; l < townLotHighWater; l++)
        if (townLots[l].active && townLots[l].kind == LOT_HOME) TownRestock(&townLots[l]);
}
