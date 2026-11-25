// support.c - Structural support (see support.h)
#include "support.h"
#include "grid.h"
#include "cell_defs.h"
#include "material.h"
#include "designations.h"
#include "pathfinding.h"
#include "../game_state.h"
#include "../core/event_log.h"
#include "../entities/items.h"
#include "../entities/mover.h"
#include "../entities/stockpiles.h"
#include "../simulation/water.h"
#include "../simulation/smoke.h"
#include "../simulation/rooms.h"
#include "../simulation/explosion.h"
#include <stdlib.h>
#include <string.h>

bool supportEnabled = true;      // setting structuralSupport
float collapseWarning = 2.0f;    // setting collapseWarning

#define SUPPORT_BUDGET     20000  // cells one search may visit; bigger pieces count as anchored
#define QUEUE_CAP          8192   // noted changes waiting (more in one tick are dropped)
#define CHECKS_PER_TICK    256    // noted changes checked per tick
#define MAX_PENDING        64
#define COLLAPSE_KNOCK     4.0f   // knockback power for someone a wall lands on

// ---------------------------------------------------------------------------------------------
// Cells of the structure

static bool IsTreeCell(CellType c) {
    return c == CELL_TREE_TRUNK || c == CELL_TREE_BRANCH || c == CELL_TREE_ROOT ||
           c == CELL_TREE_LEAVES || c == CELL_TREE_FELLED || c == CELL_SAPLING;
}

static inline bool SupInBounds(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < gridWidth && y < gridHeight && z < gridDepth;
}

static bool IsSolidNode(int x, int y, int z) {
    CellType c = GRID(x, y, z);
    return CellIsSolid(c) && !IsTreeCell(c);
}

static bool IsFloorNode(int x, int y, int z) {
    return HAS_FLOOR(x, y, z) && !CellIsSolid(GRID(x, y, z));
}

static bool IsNode(int x, int y, int z) {
    return SupInBounds(x, y, z) && (IsSolidNode(x, y, z) || IsFloorNode(x, y, z));
}

// Trees hold what rests on them (soil on roots, a wall against a trunk) but are no part of the
// structure: they never fall here (trees.c owns them)
static bool TouchesTree(int x, int y, int z) {
    static const int f[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    for (int k = 0; k < 6; k++) {
        int nx = x + f[k][0], ny = y + f[k][1], nz = z + f[k][2];
        if (SupInBounds(nx, ny, nz) && IsTreeCell(GRID(nx, ny, nz))) return true;
    }
    return false;
}

static bool IsAnchor(int x, int y, int z) {
    if (z == 0) return true;
    if (!IsSolidNode(x, y, z)) return false;
    if (TouchesTree(x, y, z)) return true;
    MaterialType m = GetWallMaterial(x, y, z);
    return m < MAT_COUNT && (materialDefs[m].flags & MF_UNMINEABLE);
}

static inline int Key(int x, int y, int z) { return (z * gridHeight + y) * gridWidth + x; }
static inline void Unkey(int k, int* x, int* y, int* z) {
    *x = k % gridWidth; k /= gridWidth;
    *y = k % gridHeight; *z = k / gridHeight;
}

// Structure cells holding (x, y, z): faces of a solid cell; a floor hangs off solid cells in
// the 3x3 on its level and the level below, and off floors beside it (and the other way round)
static int Neighbours(int x, int y, int z, int* out) {
    int n = 0;
    bool floor = IsFloorNode(x, y, z);
    static const int f[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    for (int k = 0; k < 6; k++) {
        int nx = x + f[k][0], ny = y + f[k][1], nz = z + f[k][2];
        if (!SupInBounds(nx, ny, nz)) continue;
        bool solidN = IsSolidNode(nx, ny, nz), floorN = IsFloorNode(nx, ny, nz);
        if (!floor && solidN) out[n++] = Key(nx, ny, nz);          // solid - solid, faces
        else if (floor && floorN && f[k][2] == 0) out[n++] = Key(nx, ny, nz);   // floor - floor, beside
    }
    // floor - solid: solid in the 3x3 at the floor's level or below (seen from either side)
    int dz0 = floor ? -1 : 0, dz1 = floor ? 0 : 1;
    for (int dz = dz0; dz <= dz1; dz++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy && !dz) continue;
                int nx = x + dx, ny = y + dy, nz = z + dz;
                if (!SupInBounds(nx, ny, nz)) continue;
                if (floor ? IsSolidNode(nx, ny, nz) : IsFloorNode(nx, ny, nz)) out[n++] = Key(nx, ny, nz);
            }
    return n;
}

// ---------------------------------------------------------------------------------------------
// Cell sets (open addressing, cleared by generation)

typedef struct { int* keys; uint32_t* gen; uint32_t cur; int mask, count; } CellSet;

static void SetEnsure(CellSet* s, int bits) {
    if (s->keys) return;
    s->mask = (1 << bits) - 1;
    s->keys = malloc(sizeof(int) * (size_t)(s->mask + 1));
    s->gen = calloc((size_t)(s->mask + 1), sizeof(uint32_t));
    s->cur = 1;
}
static void SetClear(CellSet* s) {
    s->count = 0;
    if (++s->cur == 0) { memset(s->gen, 0, sizeof(uint32_t) * (size_t)(s->mask + 1)); s->cur = 1; }
}
static bool SetHas(const CellSet* s, int k) {
    for (int h = ((unsigned)k * 2654435761u) & s->mask; ; h = (h + 1) & s->mask) {
        if (s->gen[h] != s->cur) return false;
        if (s->keys[h] == k) return true;
    }
}
// false if already in (or the set is full)
static bool SetAdd(CellSet* s, int k) {
    if (s->count * 10 > (s->mask + 1) * 7) return false;
    for (int h = ((unsigned)k * 2654435761u) & s->mask; ; h = (h + 1) & s->mask) {
        if (s->gen[h] != s->cur) { s->gen[h] = s->cur; s->keys[h] = k; s->count++; return true; }
        if (s->keys[h] == k) return false;
    }
}

// ---------------------------------------------------------------------------------------------
// State

typedef struct { int n; int* cells; float t; } Pending;
static Pending pending[MAX_PENDING];
static int pendingCount;
static int queue[QUEUE_CAP][3];
static int queueCount;
static CellSet visitedSet, groundedSet, pendingSet;
static int visitList[SUPPORT_BUDGET];
static int visitCount;

static void RebuildPendingSet(void) {
    SetEnsure(&pendingSet, 17);
    SetClear(&pendingSet);
    for (int p = 0; p < pendingCount; p++)
        for (int i = 0; i < pending[p].n; i++) SetAdd(&pendingSet, pending[p].cells[i]);
}

void ClearSupport(void) {
    for (int p = 0; p < pendingCount; p++) free(pending[p].cells);
    pendingCount = 0;
    queueCount = 0;
    if (pendingSet.keys) SetClear(&pendingSet);
}

int SupportPendingCount(void) { return pendingCount; }

void SupportNoteChange(int x, int y, int z) {
    if (!supportEnabled || queueCount >= QUEUE_CAP || !SupInBounds(x, y, z)) return;
    // Only a cell that is open now can have taken support away. Cells with water in them are
    // the water reporting its depth to the pathfinder (water.c, tide.c): nothing was removed.
    if (IsSolidNode(x, y, z) || WATER(x, y, z).level > 0) return;
    queue[queueCount][0] = x; queue[queueCount][1] = y; queue[queueCount][2] = z;
    queueCount++;
}

// ---------------------------------------------------------------------------------------------
// The search: lowest levels first (in terrain the ground is a few steps straight down)

// Bucket queue by z
#define MAX_LEVELS 256
static int bucketHead[MAX_LEVELS];
static int poolKey[SUPPORT_BUDGET + 64], poolNext[SUPPORT_BUDGET + 64];

// 1 = reaches the ground, 0 = doesn't (visitList holds the piece), -1 = too big to tell (anchored)
static int Search(int x, int y, int z) {
    SetEnsure(&visitedSet, 16);
    SetEnsure(&groundedSet, 18);
    SetClear(&visitedSet);
    visitCount = 0;
    int depth = gridDepth < MAX_LEVELS ? gridDepth : MAX_LEVELS;
    for (int i = 0; i < depth; i++) bucketHead[i] = -1;
    int pool = 0, minZ = z;
    int k0 = Key(x, y, z);
    SetAdd(&visitedSet, k0);
    visitList[visitCount++] = k0;
    poolKey[pool] = k0; poolNext[pool] = -1; bucketHead[z] = pool++;
    int nb[32];
    while (1) {
        while (minZ < depth && bucketHead[minZ] < 0) minZ++;
        if (minZ >= depth) return 0;
        int e = bucketHead[minZ];
        bucketHead[minZ] = poolNext[e];
        int cx, cy, cz;
        Unkey(poolKey[e], &cx, &cy, &cz);
        if (IsAnchor(cx, cy, cz) || SetHas(&groundedSet, poolKey[e])) return 1;
        int n = Neighbours(cx, cy, cz, nb);
        for (int i = 0; i < n; i++) {
            if (!SetAdd(&visitedSet, nb[i])) continue;
            if (visitCount >= SUPPORT_BUDGET) return -1;
            visitList[visitCount++] = nb[i];
            int nx, ny, nz;
            Unkey(nb[i], &nx, &ny, &nz);
            if (nz >= depth) continue;
            poolKey[pool] = nb[i]; poolNext[pool] = bucketHead[nz]; bucketHead[nz] = pool++;
            if (nz < minZ) minZ = nz;
        }
    }
}

bool SupportIsGrounded(int x, int y, int z) {
    if (!IsNode(x, y, z)) return true;
    if (groundedSet.keys) SetClear(&groundedSet);
    return Search(x, y, z) != 0;
}

// Whole world: the structure cells not connected to the ground, into *out (malloc'd); returns
// how many. One sweep marks every cell standing on a grounded column (lowest level up) and lists
// the structure cells left over (overhangs, floors, cave roofs: few); a flood from the grounded
// ones into that list does the rest. Rows go through FOR_X_IN_ROW: whole 2048 maps are 67M cells.
static int CollectUnsupported(int** out) {
    long n = (long)gridWidth * gridHeight * gridDepth;
    uint8_t* bits = calloc((size_t)(n / 8 + 1), 1);
    #define GB_GET(k) (bits[(k) >> 3] & (1u << ((k) & 7)))
    #define GB_SET(k) (bits[(k) >> 3] |= (uint8_t)(1u << ((k) & 7)))
    int restCap = 1024, restN = 0;
    int* rest = malloc(sizeof(int) * (size_t)restCap);
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++) {
            int rowKey = Key(0, y, z), belowKey = z > 0 ? Key(0, y, z - 1) : -1;
            FOR_X_IN_ROW(x, i, y, z) {
                CellType c = (CellType)LAYER_AT(grid, i);
                bool solid = CellIsSolid(c);
                if (solid ? IsTreeCell(c) : !(LAYER_AT(cellFlags, i) & CELL_FLAG_HAS_FLOOR)) continue;
                if (z == 0 || (GB_GET(belowKey + x) && IsSolidNode(x, y, z - 1)) || IsAnchor(x, y, z)) {
                    GB_SET(rowKey + x);
                    continue;
                }
                if (restN == restCap) { restCap *= 2; rest = realloc(rest, sizeof(int) * (size_t)restCap); }
                rest[restN++] = rowKey + x;
            }
        }
    // Flood from grounded cells into the rest
    int cap = 1024, count = 0;
    int* stack = malloc(sizeof(int) * (size_t)cap);
    int nb[32];
    for (int r = 0; r < restN; r++) {
        int k = rest[r], x, y, z;
        if (GB_GET(k)) continue;
        Unkey(k, &x, &y, &z);
        int m = Neighbours(x, y, z, nb);
        for (int j = 0; j < m; j++) {
            if (!GB_GET(nb[j])) continue;
            GB_SET(k);
            if (count == cap) { cap *= 2; stack = realloc(stack, sizeof(int) * (size_t)cap); }
            stack[count++] = k;
            break;
        }
        while (count > 0) {
            int q = stack[--count], qx, qy, qz;
            Unkey(q, &qx, &qy, &qz);
            int mm = Neighbours(qx, qy, qz, nb);
            for (int j = 0; j < mm; j++) {
                if (GB_GET(nb[j])) continue;
                GB_SET(nb[j]);
                if (count == cap) { cap *= 2; stack = realloc(stack, sizeof(int) * (size_t)cap); }
                stack[count++] = nb[j];
            }
        }
    }
    free(stack);
    int un = 0;
    for (int r = 0; r < restN; r++) if (!GB_GET(rest[r])) rest[un++] = rest[r];
    #undef GB_SET
    #undef GB_GET
    free(bits);
    *out = rest;
    return un;
}

int SupportCountUnsupported(void) {
    int* list;
    int n = CollectUnsupported(&list);
    free(list);
    return n;
}

// ---------------------------------------------------------------------------------------------
// The fall

static int CompareKeys(const void* a, const void* b) {
    int ka = *(const int*)a, kb = *(const int*)b;
    return (ka > kb) - (ka < kb);
}

// Someone a wall came down on: out to the side, or up on top of the pile, then knocked + dazed
static void Unbury(int i) {
    Mover* m = &movers[i];
    int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)m->z;
    static const int d8[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1} };
    for (int r = 1; r <= 3; r++)
        for (int k = 0; k < 8; k++) {
            int nx = x + d8[k][0] * r, ny = y + d8[k][1] * r;
            if (!SupInBounds(nx, ny, z) || !IsCellWalkableAt(z, ny, nx)) continue;
            m->x = (nx + 0.5f) * CELL_SIZE; m->y = (ny + 0.5f) * CELL_SIZE;
            m->needsRepath = true;
            ExplosionKnockMover(i, x + 0.5f, y + 0.5f, COLLAPSE_KNOCK);
            return;
        }
    for (int nz = z + 1; nz < gridDepth; nz++)
        if (IsCellWalkableAt(nz, y, x)) {
            m->z = (float)nz;
            m->needsRepath = true;
            ExplosionKnockMover(i, x + 0.5f, y + 0.5f, COLLAPSE_KNOCK);
            return;
        }
}

static void Fall(int* cells, int n, bool quiet) {
    qsort(cells, (size_t)n, sizeof(int), CompareKeys);   // lowest level first
    int bx0 = gridWidth, by0 = gridHeight, bz0 = gridDepth, bx1 = -1, by1 = -1, bz1 = -1;
    int walls = 0, floors = 0, rubble = 0;
    // Floors first: they break into rubble (walls above then fall through where they were)
    for (int i = 0; i < n; i++) {
        int x, y, z;
        Unkey(cells[i], &x, &y, &z);
        if (!IsFloorNode(x, y, z)) continue;
        CompleteRemoveFloorDesignation(x, y, z, -1);
        floors++;
    }
    int* landed = malloc(sizeof(int) * (size_t)(n + 1));
    int landedN = 0;
    for (int i = 0; i < n; i++) {
        int x, y, z;
        Unkey(cells[i], &x, &y, &z);
        if (x < bx0) bx0 = x; if (y < by0) by0 = y; if (z < bz0) bz0 = z;
        if (x > bx1) bx1 = x; if (y > by1) by1 = y; if (z > bz1) bz1 = z;
        if (!IsSolidNode(x, y, z)) continue;
        CellType ct = GRID(x, y, z);
        MaterialType mat = GetWallMaterial(x, y, z);
        bool natural = IsWallNatural(x, y, z);
        ItemType source = GetWallSourceItem(x, y, z);
        ItemType drop = GetWallDropItem(x, y, z);
        // Out of its place
        GRID(x, y, z) = CELL_AIR;
        SetWallMaterial(x, y, z, MAT_NONE);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_ROUGH);
        SetWallSourceItem(x, y, z, ITEM_NONE);
        if (DESIGNATION(x, y, z).type != DESIGNATION_NONE) {
            activeDesignationCount--;
            DESIGNATION_MUT(x, y, z).type = DESIGNATION_NONE;
            DESIGNATION_MUT(x, y, z).assignedMover = -1;
            DESIGNATION_MUT(x, y, z).progress = 0.0f;
        }
        MarkChunkDirty(x, y, z);
        DestabilizeWater(x, y, z);
        // Straight down, through any floor on the way
        int lz = z;
        while (lz > 0 && !CellIsSolid(GRID(x, y, lz - 1))) {
            if (HAS_FLOOR(x, y, lz)) CompleteRemoveFloorDesignation(x, y, lz, -1);
            lz--;
        }
        if (lz < bz0) bz0 = lz;
        if (GRID(x, y, lz) == CELL_AIR && ct == CELL_WALL) {
            GRID(x, y, lz) = CELL_WALL;
            SetWallMaterial(x, y, lz, mat);
            if (natural) SetWallNatural(x, y, lz); else ClearWallNatural(x, y, lz);
            SetWallFinish(x, y, lz, FINISH_ROUGH);
            SetWallSourceItem(x, y, lz, source);
            CLEAR_FLOOR(x, y, lz);
            SetWaterLevel(x, y, lz, 0);
            MarkChunkDirty(x, y, lz);
            DestabilizeWater(x, y, lz);
            PushItemsOutOfCell(x, y, lz);
            landed[landedN++] = Key(x, y, lz);
            walls++;
        } else if (drop != ITEM_NONE) {
            // Lands on a ramp, ladder, tree ...: breaks up (a door or window always does)
            uint8_t dm = mat != MAT_NONE ? (uint8_t)mat : DefaultMaterialForItemType(drop);
            SpawnItemWithMaterial((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, (float)lz, drop, dm);
            rubble++;
        }
    }
    // Whoever a wall landed on
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE), mz = (int)m->z;
        if (!SupInBounds(mx, my, mz) || mx < bx0 || mx > bx1 || my < by0 || my > by1) continue;
        if (CellIsSolid(GRID(mx, my, mz))) Unbury(i);
    }
    // What lay on top (and rubble from floors) comes down too; dust where it landed
    for (int i = 0; i < n; i++) {
        int x, y, z;
        Unkey(cells[i], &x, &y, &z);
        if (z + 1 < gridDepth) DropItemsInCell(x, y, z + 1);
        DropItemsInCell(x, y, z);
    }
    for (int i = 0; i < landedN && !quiet; i += 3) {
        int x, y, z;
        Unkey(landed[i], &x, &y, &z);
        if (z + 1 < gridDepth && !CellIsSolid(GRID(x, y, z + 1))) AddSmoke(x, y, z + 1, 2);
    }
    free(landed);
    ValidateAndCleanupRamps(bx0 - 1, by0 - 1, bz0 - 1, bx1 + 1, by1 + 1, bz1 + 1);
    InvalidateRooms();
    RebuildStockpileFreeSlotCounts();
    InvalidateDesignationCache(DESIGNATION_MINE);
    if (!quiet) {
        float shake = 1.0f + n * 0.05f;
        TriggerScreenShake(shake > 6.0f ? 6.0f : shake, 0.4f);
    }
    EventLog("Collapse at (%d..%d,%d..%d,z%d..%d): %d walls fell, %d floors broke, %d rubble",
             bx0, bx1, by0, by1, bz0, bz1, walls, floors, rubble);
}

// ---------------------------------------------------------------------------------------------
// Tick

static void Schedule(void) {
    if (pendingCount >= MAX_PENDING) return;
    Pending* p = &pending[pendingCount++];
    p->n = visitCount;
    p->cells = malloc(sizeof(int) * (size_t)visitCount);
    memcpy(p->cells, visitList, sizeof(int) * (size_t)visitCount);
    p->t = collapseWarning;
    RebuildPendingSet();
    // The warning: dust trickles from its underside
    int puffs = 0;
    for (int i = 0; i < p->n && puffs < 32; i++) {
        int x, y, z;
        Unkey(p->cells[i], &x, &y, &z);
        if (z > 0 && !CellIsSolid(GRID(x, y, z - 1)) && !HAS_FLOOR(x, y, z)) { AddSmoke(x, y, z - 1, 1); puffs++; }
    }
    int x, y, z;
    Unkey(p->cells[0], &x, &y, &z);
    EventLog("Unsupported: %d cells near (%d,%d,z%d) creak, falling in %.1fs", p->n, x, y, z, collapseWarning);
    TraceLog(LOG_INFO, "Unsupported: %d cells near (%d,%d,z%d) (%s, %s) creak, falling in %.1fs", p->n, x, y, z,
             CellTypeName(GRID(x, y, z)), IsFloorNode(x, y, z) ? "floor" : materialDefs[GetWallMaterial(x, y, z)].name, collapseWarning);
}

static void Resolve(int pi) {
    Pending p = pending[pi];
    pending[pi] = pending[--pendingCount];
    RebuildPendingSet();
    int start = -1;
    for (int i = 0; i < p.n && start < 0; i++) {
        int x, y, z;
        Unkey(p.cells[i], &x, &y, &z);
        if (IsNode(x, y, z)) start = p.cells[i];
    }
    free(p.cells);
    if (start < 0) return;   // mined away meanwhile
    int x, y, z;
    Unkey(start, &x, &y, &z);
    SetEnsure(&groundedSet, 18);
    SetClear(&groundedSet);
    if (Search(x, y, z) != 0) {   // propped up in time
        EventLog("Held: the piece near (%d,%d,z%d) is supported again", x, y, z);
        return;
    }
    int* cells = malloc(sizeof(int) * (size_t)visitCount);
    memcpy(cells, visitList, sizeof(int) * (size_t)visitCount);
    int n = visitCount;
    Fall(cells, n, false);
    free(cells);
}

void SupportTick(float dt) {
    if (!supportEnabled) { queueCount = 0; return; }
    for (int p = 0; p < pendingCount; ) {
        pending[p].t -= dt;
        if (pending[p].t <= 0.0f) Resolve(p);   // swaps the last one in: look at p again
        else p++;
    }
    if (queueCount == 0) return;
    SetEnsure(&groundedSet, 18);
    SetEnsure(&pendingSet, 17);
    SetClear(&groundedSet);
    int done = queueCount < CHECKS_PER_TICK ? queueCount : CHECKS_PER_TICK;
    for (int q = 0; q < done; q++) {
        int x = queue[q][0], y = queue[q][1], z = queue[q][2];
        if (!SupInBounds(x, y, z)) continue;
        // Every structure cell around the change could have hung from it
        for (int dz = -1; dz <= 1; dz++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy, nz = z + dz;
                    if (!IsNode(nx, ny, nz)) continue;
                    int k = Key(nx, ny, nz);
                    if (SetHas(&groundedSet, k) || SetHas(&pendingSet, k)) continue;
                    int r = Search(nx, ny, nz);
                    if (r == 0) Schedule();
                    else {
                        // All of it hangs on the ground (or is too big to fall): skip it this tick.
                        // Full: start over (only a cache)
                        for (int i = 0; i < visitCount; i++)
                            if (!SetAdd(&groundedSet, visitList[i]) && groundedSet.count * 10 > (groundedSet.mask + 1) * 7) {
                                SetClear(&groundedSet);
                                break;
                            }
                    }
                }
    }
    memmove(queue, queue + done, sizeof(queue[0]) * (size_t)(queueCount - done));
    queueCount -= done;
}

int SupportSettleWorld(void) {
    int* list;
    int n = CollectUnsupported(&list);   // 2048 world: ~0.2 s (2026-11-25)
    if (n > 0) {
        Fall(list, n, true);
        TraceLog(LOG_INFO, "Support: %d structure cells were hanging in the new world: settled", n);
    }
    free(list);
    queueCount = 0;   // what the settling noted is done
    return n;
}
