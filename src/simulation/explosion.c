// explosion.c - Explosions (see explosion.h, docs/todo/world/explosions.md)
//
// Explode() floods the blast outward from the charge over the 26 neighbours of each cell,
// highest power first (a priority queue), losing one power per cell of distance. A wall, window
// or floor in the way costs its strength; one that the blast gets past breaks. Walls shelter what
// is behind them because the blast has to go round (diagonal steps can't squeeze between two
// walls). Then, in this order: walls and floors break, loose items get a push of power / weight
// (light things fly, logs and stones shift), movers are knocked back and dazed, cells near the
// charge catch fire and smoke.
#include "explosion.h"
#include "fire.h"
#include "smoke.h"
#include "water.h"
#include "../game_state.h"
#include "../core/event_log.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../world/designations.h"
#include "../world/pathfinding.h"
#include "../world/support.h"
#include "../core/effects.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/mover.h"
#include "../entities/jobs.h"
#include "../entities/stockpiles.h"
#include "../entities/containers.h"
#include "../entities/inventory.h"
#include "../core/sim_manager.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Tunables (kept here until they need a settings row)
#define ITEM_PUSH        2.4f    // launch speed (cells/s) = ITEM_PUSH * power / (kg + 0.5)^0.25
#define ITEM_MAX_SPEED   14.0f
#define ITEM_FRICTION    8.0f    // cells/s lost per second sliding
#define MOVER_PUSH       1.2f    // launch speed = MOVER_PUSH * power / sqrt(kg / 70)
#define MOVER_MAX_SPEED  10.0f
#define MOVER_FRICTION   8.0f
#define MOVER_BODY_KG    70.0f
#define MIN_SPEED        0.5f    // weaker pushes do nothing
#define ITEM_LAND_SPEED  2.0f    // a thrown item keeps its height until it slows to this

float gunpowderFireCheck = 0.25f;   // game seconds between looks for gunpowder in or by a fire
float gunpowderSparkChance = 0.25f; // per look: gunpowder next to a fire (not in it) goes off
float blastPower = 5.0f;   // granite (4): the drilled cell + the 6 around it; ~20 cells of soil
float blastFuse = 5.0f;

// ---------------------------------------------------------------------------------------------
// Strength

static float MaterialStrength(MaterialType m) {
    if (m >= MAT_COUNT) return 3.0f;
    if (materialDefs[m].flags & MF_UNMINEABLE) return -1.0f;
    switch (m) {
        case MAT_GRANITE:   return 4.0f;
        case MAT_SLATE:     return 3.5f;
        case MAT_SANDSTONE: return 3.0f;
        case MAT_BRICK:     return 3.0f;
        case MAT_CONCRETE:  return 4.5f;
        case MAT_IRON:      return 8.0f;
        case MAT_GLASS:     return 0.5f;
        case MAT_NONE:      return 3.0f;
        default: break;
    }
    if (IsWoodMaterial(m)) return 2.5f;
    return 2.0f;   // dirt, clay, gravel, sand, peat, mud, cob
}

float ExplosionCellStrength(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return -1.0f;
    CellType c = GRID(x, y, z);
    if (c == CELL_WALL) return MaterialStrength(GetWallMaterial(x, y, z));
    if (c == CELL_WINDOW) return 0.5f;
    if (CellIsSolid(c) || CellBlocksMovement(c) || CellBlocksFluids(c)) {
        // Trees, doors, workshop-ish blockers: stop the blast, don't break (yet)
        return (c == CELL_RAILING) ? 0.0f : -1.0f;
    }
    return 0.0f;
}

// A floor that separates (x, y, z) from the open cell below it (a built floor over air)
static float FloorStrength(int x, int y, int z) {
    if (z <= 0 || !HAS_FLOOR(x, y, z)) return 0.0f;
    if (CellIsSolid(GRID(x, y, z - 1))) return 0.0f;   // the wall below is the barrier
    float s = MaterialStrength(GetFloorMaterial(x, y, z));
    return s < 0.0f ? -1.0f : s * 0.5f;
}

// ---------------------------------------------------------------------------------------------
// Transient state

#define MAX_FUSES 64
typedef struct { int x, y, z; float power, t; int kind; } Fuse;
static Fuse fuses[MAX_FUSES];
static int fuseCount;

#define MAX_FLYING 4096
typedef struct { int idx; float vx, vy; } Flying;   // cells per second; airborne at its z until it lands
static Flying flying[MAX_FLYING];
static int flyingCount;

static int explosionsTotal;   // since the last ClearExplosions

static float knockVX[MAX_MOVERS], knockVY[MAX_MOVERS], knockDaze[MAX_MOVERS];
static int knockedCount;   // movers with knockDaze > 0 (fast exit when none)

void ClearExplosions(void) {
    ClearSupport();   // pending collapses: transient like fuses
    ClearFx();
    fuseCount = 0;
    flyingCount = 0;
    explosionsTotal = 0;
    if (knockedCount > 0) {
        memset(knockVX, 0, sizeof(knockVX));
        memset(knockVY, 0, sizeof(knockVY));
        memset(knockDaze, 0, sizeof(knockDaze));
    }
    knockedCount = 0;
}

int ExplosionFuseCount(void) { return fuseCount; }
int ExplosionCount(void) { return explosionsTotal; }
int ExplosionFlyingCount(void) { return flyingCount; }
bool MoverKnocked(int i) { return i >= 0 && i < MAX_MOVERS && knockDaze[i] > 0.0f; }

// What kinds of explosive do differently (power is the reach either way)
const BlastKindDef blastKinds[BLAST_KIND_COUNT] = {
    //               name         brisance fire  smoke push  fireball
    [BLAST_GUNPOWDER] = { "gunpowder", 1.0f, 2.5f, 1.0f, 1.0f, 1.0f },
    [BLAST_DYNAMITE]  = { "dynamite",  2.0f, 1.0f, 0.5f, 0.8f, 0.7f },
};

static float curBrisance = 1.0f;   // the kind going off now (StepBlocked reads it)

bool ExplosionLightFuse(int x, int y, int z, float power, float seconds) {
    return ExplosionLightFuseKind(x, y, z, power, seconds, BLAST_GUNPOWDER);
}

bool ExplosionLightFuseKind(int x, int y, int z, float power, float seconds, BlastKind kind) {
    if (fuseCount >= MAX_FUSES) return false;
    fuses[fuseCount++] = (Fuse){ x, y, z, power, seconds, (int)kind };
    FxEmitSparks(x, y, z, seconds);
    EventLog("Fuse lit at (%d,%d,z%d) power %.1f, %.1fs", x, y, z, power, seconds);
    return true;
}

// ---------------------------------------------------------------------------------------------
// The blast flood

typedef struct { float v; int i; } HeapNode;
typedef struct { HeapNode* a; int n, cap; } Heap;

static void HeapPush(Heap* h, float v, int i) {
    if (h->n == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 1024;
        h->a = realloc(h->a, (size_t)h->cap * sizeof(HeapNode));
    }
    int k = h->n++;
    while (k > 0) {
        int p = (k - 1) / 2;
        if (h->a[p].v >= v) break;
        h->a[k] = h->a[p];
        k = p;
    }
    h->a[k] = (HeapNode){ v, i };
}

static HeapNode HeapPop(Heap* h) {
    HeapNode top = h->a[0], last = h->a[--h->n];
    int k = 0;
    for (;;) {
        int c = 2 * k + 1;
        if (c >= h->n) break;
        if (c + 1 < h->n && h->a[c + 1].v > h->a[c].v) c++;
        if (h->a[c].v <= last.v) break;
        h->a[k] = h->a[c];
        k = c;
    }
    if (h->n > 0) h->a[k] = last;
    return top;
}

// Blocks a diagonal squeeze: a wall on one of the straight steps that the blast couldn't break
// going straight (power v left here). Inside rock it can break, diagonals are fine.
static bool StepBlocked(int x, int y, int z, float v) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return true;
    float s = ExplosionCellStrength(x, y, z);
    return s < 0.0f || s / curBrisance > v - 1.0f;
}

// Hash for a direction when something sits right on the charge
static float HashAngle(int i) {
    unsigned h = (unsigned)i * 2654435761u;
    h ^= h >> 15;
    return (float)(h % 6283u) / 1000.0f;
}

// Fall to the lowest open level (like DropItemsInCell, for one item)
static int LandingZ(int x, int y, int z) {
    while (z > 0 && !HAS_FLOOR(x, y, z) && !CellIsSolid(GRID(x, y, z - 1))) z--;
    return z;
}

static void SetItemPos(int i, float px, float py, float pz) {
    if (items[i].contentCount > 0) MoveContainer(i, px, py, pz);
    else { items[i].x = px; items[i].y = py; items[i].z = pz; }
}

static void PushItem(int i, float power, float launchZ, float ox, float oy, ExplosionResult* res) {
    Item* it = &items[i];
    if (it->reservedBy >= 0) {
        // Held for a job: the job lets go (it would walk to where the item was). A need's
        // reservation (someone on the way to eat it) keeps the item where it is.
        int r = it->reservedBy;
        if (r < moverCount && movers[r].active && movers[r].currentJobId >= 0) CancelJob(&movers[r], r);
        if (it->reservedBy >= 0) return;
    }
    float kg = ItemWeight(it->type) * (float)(it->stackCount > 1 ? it->stackCount : 1);
    if (it->contentCount > 0) kg = GetContainerTotalWeight(i);
    float speed = ITEM_PUSH * power / powf(kg + 0.5f, 0.25f);
    if (speed < MIN_SPEED) return;
    if (speed > ITEM_MAX_SPEED) speed = ITEM_MAX_SPEED;
    if (flyingCount >= MAX_FLYING) return;
    float dx = it->x / CELL_SIZE - ox, dy = it->y / CELL_SIZE - oy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.05f) { float a = HashAngle(i); dx = cosf(a); dy = sinf(a); d = 1.0f; }
    if (it->state == ITEM_IN_STOCKPILE) {
        RemoveItemFromStockpileSlot(it->x, it->y, (int)it->z);
        it->state = ITEM_ON_GROUND;
    }
    // Thrown from where it lay (the ground under it may have gone; it lands where it comes down)
    if (it->z != launchZ) SetItemPos(i, it->x, it->y, launchZ);
    flying[flyingCount++] = (Flying){ i, dx / d * speed, dy / d * speed };
    res->itemsPushed++;
}

static void KnockMover(int i, float power, float ox, float oy, ExplosionResult* res) {
    Mover* m = &movers[i];
    float kg = MOVER_BODY_KG + MoverCarriedWeight(i);
    float speed = MOVER_PUSH * power / sqrtf(kg / MOVER_BODY_KG);
    if (speed < MIN_SPEED) return;
    if (speed > MOVER_MAX_SPEED) speed = MOVER_MAX_SPEED;
    float dx = m->x / CELL_SIZE - ox, dy = m->y / CELL_SIZE - oy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.05f) { float a = HashAngle(i + 7919); dx = cosf(a); dy = sinf(a); d = 1.0f; }
    if (knockDaze[i] <= 0.0f) knockedCount++;
    knockVX[i] = dx / d * speed;
    knockVY[i] = dy / d * speed;
    float daze = 0.4f + 0.15f * speed;
    knockDaze[i] = daze > 2.5f ? 2.5f : daze;
    m->fallTimer = knockDaze[i];   // drawn like a fall (blue) while dazed
    res->moversKnocked++;
}

// ---------------------------------------------------------------------------------------------
// Gunpowder going off: in a fire, or reached by a blast

float GunpowderPower(int stackCount) {
    float p = blastPower * cbrtf((float)(stackCount > 1 ? stackCount : 1));
    return p > EXPLOSION_MAX_POWER ? EXPLOSION_MAX_POWER : p;
}

// The item becomes a lit charge where it is (a carried one: its carrier drops it first)
static bool SetOffGunpowder(int i, float fuse, const char* why) {
    Item* it = &items[i];
    if (!it->active || it->type != ITEM_GUNPOWDER) return false;
    if (it->state == ITEM_CARRIED) {
        for (int m = 0; m < moverCount; m++) {
            if (!movers[m].active || movers[m].currentJobId < 0) continue;
            Job* job = GetJob(movers[m].currentJobId);
            if (job && job->carryingItem == i) { CancelJob(&movers[m], m); break; }
        }
        if (it->state == ITEM_CARRIED) return false;   // held some other way (pockets): leave it
    }
    if (fuseCount >= MAX_FUSES) return false;
    int x = (int)(it->x / CELL_SIZE), y = (int)(it->y / CELL_SIZE), z = (int)it->z;
    float power = GunpowderPower(it->stackCount);
    EventLog("Gunpowder %d (x%d) at (%d,%d,z%d) goes off: %s", i, it->stackCount, x, y, z, why);
    DeleteItem(i);
    return ExplosionLightFuse(x, y, z, power, fuse);
}

static bool FireAt(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < gridWidth && y < gridHeight && z < gridDepth && HasFire(x, y, z);
}

// Every gunpowderFireCheck seconds while anything burns: gunpowder in a burning cell (or on
// burning ground) goes off after a short fizz; next to one (the 8 around, on its level or the
// level below) a spark sets it off by chance
static void GunpowderFireTick(float dt) {
    static float accum = 0.0f;
    static unsigned rng = 12345u;
    if (fireActiveCells <= 0) { accum = 0.0f; return; }
    accum += dt;
    if (accum < gunpowderFireCheck) return;
    accum = 0.0f;
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (!it->active || it->type != ITEM_GUNPOWDER) continue;
        int x = (int)(it->x / CELL_SIZE), y = (int)(it->y / CELL_SIZE), z = (int)it->z;
        // In: its own cell burns, or the ground it lies on (grass burns in the soil cell below)
        bool in = FireAt(x, y, z) || (z > 0 && FireAt(x, y, z - 1) && !HAS_FLOOR(x, y, z));
        bool near = false;
        for (int dz = 0; dz >= -1 && !in && !near; dz--)
            for (int dy = -1; dy <= 1 && !near; dy++)
                for (int dx = -1; dx <= 1 && !near; dx++)
                    if ((dx || dy || dz) && FireAt(x + dx, y + dy, z + dz)) near = true;
        if (!in && !near) continue;
        if (!in) {
            rng = rng * 1664525u + 1013904223u;
            if ((float)(rng >> 8) / 16777216.0f >= gunpowderSparkChance) continue;
        }
        SetOffGunpowder(i, 0.5f + (float)(i % 5) * 0.15f, in ? "in a fire" : "a spark from a fire");
    }
}

void ExplosionKnockMover(int moverIdx, float fromX, float fromY, float power) {
    if (moverIdx < 0 || moverIdx >= moverCount || !movers[moverIdx].active) return;
    ExplosionResult unused = {0};
    KnockMover(moverIdx, power, fromX, fromY, &unused);
}

ExplosionResult Explode(int x, int y, int z, float power) {
    return ExplodeKind(x, y, z, power, BLAST_GUNPOWDER);
}

ExplosionResult ExplodeKind(int x, int y, int z, float power, BlastKind kind) {
    ExplosionResult res = {0};
    if ((int)kind < 0 || kind >= BLAST_KIND_COUNT) kind = BLAST_GUNPOWDER;
    const BlastKindDef* K = &blastKinds[kind];
    curBrisance = K->brisance;
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return res;
    if (power <= 0.0f) return res;
    if (power > EXPLOSION_MAX_POWER) power = EXPLOSION_MAX_POWER;
    explosionsTotal++;

    int R = (int)ceilf(power);
    int x0 = x - R < 0 ? 0 : x - R, x1 = x + R >= gridWidth ? gridWidth - 1 : x + R;
    int y0 = y - R < 0 ? 0 : y - R, y1 = y + R >= gridHeight ? gridHeight - 1 : y + R;
    int z0 = z - R < 0 ? 0 : z - R, z1 = z + R >= gridDepth ? gridDepth - 1 : z + R;
    int W = x1 - x0 + 1, H = y1 - y0 + 1, D = z1 - z0 + 1;
    int n = W * H * D;
    float* best = malloc((size_t)n * sizeof(float));
    uint8_t* breakCell = calloc((size_t)n, 1);    // 1 = wall breaks
    uint8_t* breakFloor = calloc((size_t)n, 1);   // 1 = this cell's floor breaks
    for (int i = 0; i < n; i++) best[i] = -1.0f;
    #define BIDX(cx, cy, cz) ((((cz) - z0) * H + ((cy) - y0)) * W + ((cx) - x0))

    // The charge: drilled into a wall it sits in a hole, so that wall goes for free
    float start = power;
    float s0 = ExplosionCellStrength(x, y, z);
    if (s0 < 0.0f) start = 0.0f;
    else if (s0 > 0.0f) breakCell[BIDX(x, y, z)] = 1;
    Heap h = {0};
    if (start > 0.0f) { best[BIDX(x, y, z)] = start; HeapPush(&h, start, BIDX(x, y, z)); }

    while (h.n > 0) {
        HeapNode top = HeapPop(&h);
        if (top.v < best[top.i]) continue;   // stale entry
        int cz = top.i / (W * H) + z0, rem = top.i % (W * H);
        int cy = rem / W + y0, cx = rem % W + x0;
        for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy && !dz) continue;
            int nx = cx + dx, ny = cy + dy, nz = cz + dz;
            if (nx < x0 || nx > x1 || ny < y0 || ny > y1 || nz < z0 || nz > z1) continue;
            int axes = (dx != 0) + (dy != 0) + (dz != 0);
            float v = top.v - (axes == 1 ? 1.0f : axes == 2 ? 1.41421f : 1.73205f);
            if (v <= 0.0f) continue;
            if (axes > 1) {   // no squeezing diagonally between walls
                if (dx && StepBlocked(cx + dx, cy, cz, top.v)) continue;
                if (dy && StepBlocked(cx, cy + dy, cz, top.v)) continue;
                if (dz && StepBlocked(cx, cy, cz + dz, top.v)) continue;
            }
            // Crossing a level: the floor of the upper of the two cells
            if (dz != 0) {
                int fx = dz > 0 ? nx : cx, fy = dz > 0 ? ny : cy, fz = dz > 0 ? nz : cz;
                float fs = FloorStrength(fx, fy, fz);
                if (fs > 0.0f) fs /= K->brisance;
                if (fs < 0.0f || v < fs) continue;
                if (fs > 0.0f) { v -= fs; breakFloor[BIDX(fx, fy, fz)] = 1; if (v <= 0.0f) continue; }
            }
            int ni = BIDX(nx, ny, nz);
            float s = ExplosionCellStrength(nx, ny, nz);
            if (s > 0.0f) s /= K->brisance;   // a sharper explosive shatters walls for less
            if (s < 0.0f || v < s) continue;
            if (s > 0.0f) { breakCell[ni] = 1; v -= s; if (v <= 0.0f) continue; }
            if (v > best[ni]) { best[ni] = v; HeapPush(&h, v, ni); }
        }
    }
    free(h.a);

    // Gunpowder the blast reaches goes off too (a moment later): stockpiles chain
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (!it->active || it->type != ITEM_GUNPOWDER) continue;
        int ix = (int)(it->x / CELL_SIZE), iy = (int)(it->y / CELL_SIZE), iz = (int)it->z;
        if (ix < x0 || ix > x1 || iy < y0 || iy > y1 || iz < z0 || iz > z1) continue;
        if (best[BIDX(ix, iy, iz)] >= 1.0f && SetOffGunpowder(i, 0.15f + (float)(i % 4) * 0.05f, "reached by a blast"))
            res.powderSetOff++;
    }

    // Who gets pushed, by the power where they are now (before the ground under them goes)
    int pushN = 0, knockN = 0;
    int* pushIdx = malloc(sizeof(int) * (size_t)(itemHighWaterMark + 1));
    float* pushPow = malloc(sizeof(float) * (size_t)(itemHighWaterMark + 1));
    float* pushZ = malloc(sizeof(float) * (size_t)(itemHighWaterMark + 1));
    uint8_t* listed = calloc(MAX_ITEMS, 1);
    int* knockIdx = malloc(sizeof(int) * (size_t)(moverCount + 1));
    float* knockPow = malloc(sizeof(float) * (size_t)(moverCount + 1));
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (!it->active || it->containedIn >= 0) continue;
        if (it->state != ITEM_ON_GROUND && it->state != ITEM_IN_STOCKPILE) continue;
        int ix = (int)(it->x / CELL_SIZE), iy = (int)(it->y / CELL_SIZE), iz = (int)it->z;
        if (ix < x0 || ix > x1 || iy < y0 || iy > y1 || iz < z0 || iz > z1) continue;
        float v = best[BIDX(ix, iy, iz)];
        if (v > 0.0f) { pushIdx[pushN] = i; pushZ[pushN] = it->z; pushPow[pushN++] = v; listed[i] = 1; }
    }
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE), mz = (int)m->z;
        if (mx < x0 || mx > x1 || my < y0 || my > y1 || mz < z0 || mz > z1) continue;
        float v = best[BIDX(mx, my, mz)];
        if (v > 0.0f) { knockIdx[knockN] = i; knockPow[knockN++] = v; }
    }

    // 1. Walls break (mined: drops, ramps checked, rooms, water)
    for (int cz = z0; cz <= z1; cz++)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                if (!breakCell[BIDX(cx, cy, cz)]) continue;
                if (res.cellsBroken < 48) {   // what flies off it (looks only: the drops are real)
                    int mat = GetWallMaterial(cx, cy, cz);
                    float v = best[BIDX(cx, cy, cz)];
                    FxChunks(cx, cy, cz, mat, (x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, 3, 110.0f + 25.0f * (v > 0.0f ? v : 0.0f));
                    if (res.cellsBroken % 2 == 0) FxDust((cx + 0.5f) * CELL_SIZE, (cy + 0.5f) * CELL_SIZE, cz, mat, 1, CELL_SIZE * 0.3f);
                }
                CompleteMineDesignation(cx, cy, cz);
                res.cellsBroken++;
            }
    // Mining leaves a floor in the cell; over a hole the blast made below it there is none
    for (int cz = z0; cz <= z1; cz++)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                if (!breakCell[BIDX(cx, cy, cz)] || cz == 0) continue;
                if (CellIsSolid(GRID(cx, cy, cz - 1)) || !HAS_FLOOR(cx, cy, cz)) continue;
                CLEAR_FLOOR(cx, cy, cz);
                SetFloorMaterial(cx, cy, cz, MAT_NONE);
                ClearFloorNatural(cx, cy, cz);
                MarkChunkDirty(cx, cy, cz);
            }
    // 2. Floors break: what lay on them drops (and the floor itself, as rubble)
    for (int cz = z1; cz >= z0; cz--)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                if (!breakFloor[BIDX(cx, cy, cz)] || !HAS_FLOOR(cx, cy, cz)) continue;
                CompleteRemoveFloorDesignation(cx, cy, cz, -1);
                res.floorsBroken++;
            }
    // Items resting on a wall that broke under them fall
    for (int cz = z0; cz <= z1; cz++)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                if (!breakCell[BIDX(cx, cy, cz)] || cz + 1 >= gridDepth) continue;
                if (!HAS_FLOOR(cx, cy, cz + 1)) DropItemsInCell(cx, cy, cz + 1);
                if (!HAS_FLOOR(cx, cy, cz)) DropItemsInCell(cx, cy, cz);
            }

    // A crater, not a pit: where the blast left a step of one level up to standing ground, the
    // floor of the hole slopes up to it (a ramp), so you can walk in and out. Scorched in the middle
    for (int cz = z0; cz <= z1 && cz + 1 < gridDepth; cz++)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                if (!breakCell[BIDX(cx, cy, cz)] || GRID(cx, cy, cz) != CELL_AIR) continue;
                if (cz > 0 && !CellIsSolid(GRID(cx, cy, cz - 1))) continue;   // not standing ground
                float sx = (float)(cx - x), sy = (float)(cy - y), sz = (float)(cz - z);
                if (cz > 0 && sx * sx + sy * sy + sz * sz <= power * power * 0.36f) SET_CELL_FLAG(cx, cy, cz - 1, CELL_FLAG_BURNED);
                static const int d4[4][2] = { {0,-1}, {1,0}, {0,1}, {-1,0} };
                static const CellType ramps[4] = { CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W };
                for (int k = 0; k < 4; k++) {
                    int nx = cx + d4[k][0], ny = cy + d4[k][1];
                    if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                    if (!CellIsSolid(GRID(nx, ny, cz)) || !IsCellWalkableAt(cz + 1, ny, nx)) continue;
                    if (CellIsSolid(GRID(cx, cy, cz + 1))) break;   // a roof over it: no way up
                    GRID(cx, cy, cz) = ramps[k];
                    SET_FLOOR(cx, cy, cz);
                    rampCount++;
                    MarkChunkDirty(cx, cy, cz);
                    res.rampsMade++;
                    break;
                }
            }

    // Stockpile cells whose ground went are no longer free slots
    if (res.cellsBroken > 0 || res.floorsBroken > 0) RebuildStockpileFreeSlotCounts();

    // 3. Loose items: a push away from the charge, by power and weight. Rubble the blast made
    // (drops of broken walls) flies too, by the power in the cell it came from
    float ox = x + 0.5f, oy = y + 0.5f;
    for (int k = 0; k < pushN; k++) PushItem(pushIdx[k], pushPow[k] * K->push, pushZ[k], ox, oy, &res);
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (listed[i] || !it->active || it->containedIn >= 0 || it->state != ITEM_ON_GROUND) continue;
        int ix = (int)(it->x / CELL_SIZE), iy = (int)(it->y / CELL_SIZE), iz = (int)it->z;
        if (ix < x0 || ix > x1 || iy < y0 || iy > y1 || iz < z0 || iz > z1) continue;
        if (!breakCell[BIDX(ix, iy, iz)]) continue;
        float v = best[BIDX(ix, iy, iz)];
        PushItem(i, (v > 0.0f ? v : 0.0f) + 1.0f, it->z, ox, oy, &res);
    }
    // 4. Movers: knocked back and dazed
    for (int k = 0; k < knockN; k++) KnockMover(knockIdx[k], knockPow[k] * K->push, ox, oy, &res);
    free(pushIdx); free(pushPow); free(pushZ); free(listed); free(knockIdx); free(knockPow);

    // 5. Fire near the charge, smoke over the inner half
    for (int cz = z0; cz <= z1; cz++)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                float v = best[BIDX(cx, cy, cz)];
                if (v <= 0.0f) continue;
                if (v >= power - K->fire && !HasFire(cx, cy, cz) && GetFuelAt(cx, cy, cz) > 0) {
                    IgniteCell(cx, cy, cz);
                    if (HasFire(cx, cy, cz)) res.cellsIgnited++;
                }
                // Real smoke (the smoke simulation: it rises, spreads, fills rooms, drifts off),
                // thickest at the centre, in the open cells of the inner half of the blast
                if (v >= power * 0.5f && !CellIsSolid(GRID(cx, cy, cz))) {
                    int level = (int)((2.0f + 5.0f * v / power) * K->smoke);
                    if (level > SMOKE_MAX_LEVEL) level = SMOKE_MAX_LEVEL;
                    if (GetSmokeLevel(cx, cy, cz) < level) SetSmokeLevel(cx, cy, cz, level);
                }
            }

    // 6. Water: the surface in the inner half of the blast is thrown outward, along the
    // direction from the charge, as far as the blast reaches, and lands there. Nothing is made
    // or lost (what finds no room falls back where it was); the water simulation runs it back
    for (int cz = z0; cz <= z1; cz++)
        for (int cy = y0; cy <= y1; cy++)
            for (int cx = x0; cx <= x1; cx++) {
                float v = best[BIDX(cx, cy, cz)];
                if (v < power * 0.5f) continue;
                int lvl = GetWaterLevel(cx, cy, cz);
                if (lvl <= 0 || IsWaterFrozen(cx, cy, cz)) continue;
                if (cz + 1 < gridDepth && GetWaterLevel(cx, cy, cz + 1) > 0) continue;   // under the surface
                float dx = (float)(cx - x), dy = (float)(cy - y), d = sqrtf(dx * dx + dy * dy);
                if (d < 0.5f) { float a = HashAngle(cx * 31 + cy * 17 + cz); dx = cosf(a); dy = sinf(a); d = 1.0f; }
                dx /= d; dy /= d;
                // Thrown up and out: it flies through the air over the surface (the level above,
                // when that's open) while the blast still reaches and the way is open, then falls
                int az = (cz + 1 < gridDepth && !CellIsSolid(GRID(cx, cy, cz + 1))) ? cz + 1 : cz;
                int lx = cx, ly = cy;
                for (float t = 1.0f; t <= power * 1.5f; t += 0.5f) {
                    int tx = (int)floorf(cx + 0.5f + dx * t), ty = (int)floorf(cy + 0.5f + dy * t);
                    if (tx == lx && ty == ly) continue;
                    if (tx < 0 || ty < 0 || tx >= gridWidth || ty >= gridHeight) break;
                    if (CellIsSolid(GRID(tx, ty, az))) break;
                    lx = tx; ly = ty;
                    if (az > z1 || tx < x0 || tx > x1 || ty < y0 || ty > y1 || best[BIDX(tx, ty, az)] <= 0.0f) break;
                }
                if (lx == cx && ly == cy) continue;
                int room = WATER_MAX_LEVEL - GetWaterLevel(lx, ly, az);
                int moved = lvl < room ? lvl : room;
                if (moved <= 0) continue;
                RemoveWater(cx, cy, cz, moved);
                AddWater(lx, ly, az, moved);
                res.waterThrown += moved;
                if (res.waterThrown < 200) {   // looks only: droplets and spray
                    FxSplash((cx + 0.5f) * CELL_SIZE, (cy + 0.5f) * CELL_SIZE, cz, dx, dy, 3 + moved, 90.0f + 20.0f * v);
                }
            }

    // 7. The shake: loose ground within reach slides where it has a drop
    if (landslidesEnabled)
        for (int cz = z0; cz <= z1; cz++)
            for (int cy = y0; cy <= y1; cy++)
                for (int cx = x0; cx <= x1; cx++) {
                    float ddx = (float)(cx - x), ddy = (float)(cy - y), ddz = (float)(cz - z);
                    if (ddx * ddx + ddy * ddy + ddz * ddz <= (power + 1.0f) * (power + 1.0f)) LandslideDisturb(cx, cy, cz);
                }

    #undef BIDX
    free(best);
    free(breakCell);
    free(breakFloor);

    FxBlast((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, z, power * K->fireball);
    float shake = power * 0.8f;
    TriggerScreenShake(shake > 8.0f ? 8.0f : shake, 0.3f + power * 0.03f);
    TraceLog(LOG_INFO, "Explosion (%s) at (%d,%d,z%d) power %.1f: %d walls, %d floors, %d items, %d movers, %d fires, %d ramps, %d water",
             K->name, x, y, z, power, res.cellsBroken, res.floorsBroken, res.itemsPushed, res.moversKnocked, res.cellsIgnited, res.rampsMade, res.waterThrown);
    EventLog("Explosion (%s) at (%d,%d,z%d) power %.1f: %d walls, %d floors, %d items, %d movers, %d fires",
             K->name, x, y, z, power, res.cellsBroken, res.floorsBroken, res.itemsPushed, res.moversKnocked, res.cellsIgnited);
    return res;
}

// ---------------------------------------------------------------------------------------------
// Flying items, fuses

static bool ItemCellOpen(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    CellType c = GRID(x, y, z);
    return !CellIsSolid(c) && !CellBlocksMovement(c);
}

static void Land(int i) {
    Item* it = &items[i];
    int cx = (int)(it->x / CELL_SIZE), cy = (int)(it->y / CELL_SIZE), cz = (int)it->z;
    if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight) return;
    int lz = LandingZ(cx, cy, cz);
    if (lz != cz) SetItemPos(i, it->x, it->y, (float)lz);
}

static void FlyingTick(float dt) {
    int w = 0;
    for (int f = 0; f < flyingCount; f++) {
        Flying fl = flying[f];
        Item* it = &items[fl.idx];
        if (!it->active || it->state != ITEM_ON_GROUND || it->containedIn >= 0) continue;
        if (it->reservedBy >= 0) { Land(fl.idx); continue; }
        int cx = (int)(it->x / CELL_SIZE), cy = (int)(it->y / CELL_SIZE), cz = (int)it->z;
        float speed = sqrtf(fl.vx * fl.vx + fl.vy * fl.vy);
        float mx = fl.vx * dt * CELL_SIZE, my = fl.vy * dt * CELL_SIZE;
        // Full step, else slide along the wall (the blocked part of the speed is lost)
        float tryX[3] = { mx, mx, 0.0f }, tryY[3] = { my, 0.0f, my };
        bool moved = false;
        for (int t = 0; t < 3 && !moved; t++) {
            if (tryX[t] == 0.0f && tryY[t] == 0.0f) continue;
            float nx = it->x + tryX[t], ny = it->y + tryY[t];
            int ncx = (int)floorf(nx / CELL_SIZE), ncy = (int)floorf(ny / CELL_SIZE);
            if ((ncx != cx || ncy != cy) && !ItemCellOpen(ncx, ncy, cz)) continue;
            SetItemPos(fl.idx, nx, ny, (float)cz);
            if (t == 1) fl.vy = 0.0f;
            if (t == 2) fl.vx = 0.0f;
            moved = true;
        }
        float ns = speed - ITEM_FRICTION * dt;
        if (!moved || ns < ITEM_LAND_SPEED) { Land(fl.idx); continue; }   // hit a wall, or came down
        fl.vx *= ns / speed;
        fl.vy *= ns / speed;
        flying[w++] = fl;
    }
    flyingCount = w;
}

void ExplosionsTick(float dt) {
    GunpowderFireTick(dt);
    if (fuseCount > 0) {
        for (int f = 0; f < fuseCount; ) {
            fuses[f].t -= dt;
            if (fuses[f].t > 0.0f) { f++; continue; }
            Fuse fu = fuses[f];
            fuses[f] = fuses[--fuseCount];
            ExplodeKind(fu.x, fu.y, fu.z, fu.power, (BlastKind)fu.kind);
        }
    }
    if (flyingCount > 0) FlyingTick(dt);
}

// ---------------------------------------------------------------------------------------------
// Knocked movers

static bool MoverCellOpen(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    CellType c = GRID(x, y, z);
    if (CellIsSolid(c) || CellBlocksMovement(c)) return false;
    return !(CELL_FLAGS(x, y, z) & CELL_FLAG_WORKSHOP_BLOCK);
}

static void EndKnock(int i) {
    knockDaze[i] = 0.0f;
    knockVX[i] = knockVY[i] = 0.0f;
    knockedCount--;
    movers[i].needsRepath = true;
}

bool ExplosionMoverTick(int i, float dt) {
    if (knockedCount <= 0 || knockDaze[i] <= 0.0f) return false;
    Mover* m = &movers[i];
    if (!m->active) { EndKnock(i); return false; }
    float vx = knockVX[i], vy = knockVY[i];
    float speed = sqrtf(vx * vx + vy * vy);
    if (speed > 0.0f) {
        int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE), cz = (int)m->z;
        float mx = vx * dt * CELL_SIZE, my = vy * dt * CELL_SIZE;
        float tryX[3] = { mx, mx, 0.0f }, tryY[3] = { my, 0.0f, my };
        bool moved = false;
        for (int t = 0; t < 3 && !moved; t++) {
            if (tryX[t] == 0.0f && tryY[t] == 0.0f) continue;
            float nx = m->x + tryX[t], ny = m->y + tryY[t];
            int ncx = (int)floorf(nx / CELL_SIZE), ncy = (int)floorf(ny / CELL_SIZE);
            if ((ncx != cx || ncy != cy) && !MoverCellOpen(ncx, ncy, cz)) continue;
            m->x = nx; m->y = ny;
            if (t == 1) vy = 0.0f;
            if (t == 2) vx = 0.0f;
            moved = true;
        }
        float ns = moved ? speed - MOVER_FRICTION * dt : 0.0f;
        if (ns < 0.1f) { vx = vy = 0.0f; }
        else { vx *= ns / speed; vy *= ns / speed; }
        knockVX[i] = vx; knockVY[i] = vy;
    }
    // Flying it keeps its height; come down over a hole or an edge, the normal update lets it fall
    if (knockVX[i] == 0.0f && knockVY[i] == 0.0f &&
        !IsCellWalkableAt((int)m->z, (int)(m->y / CELL_SIZE), (int)(m->x / CELL_SIZE))) { EndKnock(i); return false; }
    knockDaze[i] -= dt;
    if (knockDaze[i] <= 0.0f) { knockDaze[i] = 0.01f; EndKnock(i); return false; }  // EndKnock zeroes it
    return true;
}
