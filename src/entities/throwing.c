// throwing.c - throwing things, and things flying (see throwing.h)
#include "throwing.h"
#include "items.h"
#include "item_defs.h"
#include "mover.h"
#include "jobs.h"
#include "stacking.h"
#include "inventory.h"
#include "stockpiles.h"
#include "animals.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/water.h"
#include "../core/event_log.h"
#include <math.h>
#include <string.h>

float throwSpeed = 20.0f;
float throwGravity = 30.0f;
float throwBounce = 0.35f;
float throwSpread = 1.0f;
float throwMaxWeight = 5.0f;
long throwCount = 0, throwBounceCount = 0, throwHitCount = 0, throwStartleCount = 0;

#define THROW_RELEASE_HEIGHT 0.7f   // levels above the thrower's feet
#define THROW_PI             3.14159265f
#define THROW_STEP_LEN       0.2f   // cells: longest move per sub-step (never skips a cell)
#define THROW_ROLL_VZ        1.5f   // cells/s: a ground bounce slower than this starts rolling
#define THROW_ROLL_FRICTION  30.0f  // cells/s^2 lost rolling (grass, dirt: it soon stops)
#define THROW_REST_SPEED     0.3f   // cells/s: rolling slower than this, it lies still
#define THROW_GROUND_KEEP    0.5f   // horizontal speed kept on a ground bounce
#define THROW_WALL_KEEP      0.85f  // speed along the wall kept on a wall bounce
#define THROW_MAX_TIME       12.0f  // seconds: a flight longer than this just lands
#define THROW_WINDUP         0.35f  // seconds the thrower winds up
#define THROW_REPLAN         0.25f  // seconds between "can I throw from here?" checks
#define THROW_AIM_TOLERANCE  1.0f   // cells: a planned throw comes down this close to the goal
#define THROW_HIT_RADIUS     0.45f  // cells: an animal's body, seen from above
#define THROW_HIT_HEIGHT     0.8f   // levels: how tall an animal is
#define THROW_HIT_SPEED      3.0f   // cells/s: slower than this it just bumps into it (no scare)
#define THROW_HIT_KEEP       0.25f  // speed kept bouncing off an animal
#define THROW_STARTLE_RADIUS 1.5f   // cells: a stone coming down this close scares an animal
#define THROW_HIT_AIM_H      0.4f   // levels: aim at the middle of an animal's body
#define THROW_HIT_AIM_TOL    0.3f   // cells: a perfect throw passes this close to that point

// Job steps
#define THROW_STEP_FETCH    0   // walking to the item
#define THROW_STEP_POSITION 1   // carrying it, walking until a throw from here would get there
#define THROW_STEP_WINDUP   2   // standing, about to let go

typedef struct {
    int item;
    float x, y, h;        // cells; h in levels (the floor of level z is h = z)
    float vx, vy, vz;     // cells/s
    float age;
    bool rolling;
    int thrower;
    int lastX, lastY, lastZ;   // last open cell it was in (landing falls back on it)
    float fromX, fromY;   // cells: where it was let go (an animal hit runs from there)
    int hitAnimal;        // the animal it hit (-1: none yet); one hit per throw
    bool landed;          // touched the ground once (the moment animals nearby get startled)
} Flight;

static Flight flights[THROW_MAX_FLIGHTS];
static int flightCount = 0;

void ClearThrows(void) {
    flightCount = 0;
    throwCount = throwBounceCount = throwHitCount = throwStartleCount = 0;
}

int ThrowFlightCount(void) { return flightCount; }

// ---------------------------------------------------------------------------------------------
// What is light and small enough

static float ThrowUnitWeight(int itemIdx) {
    return ItemWeight(items[itemIdx].type);
}

bool ItemThrowable(int itemIdx) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS || !items[itemIdx].active) return false;
    const Item* it = &items[itemIdx];
    if (itemDefs[it->type].flags & IF_TWO_HANDED) return false;
    if (it->contentCount > 0) return false;   // a full basket stays a basket
    return ThrowUnitWeight(itemIdx) <= throwMaxWeight;
}

// Top launch speed: a stone-sized thing (about 1 kg) goes furthest; heavier ones slower
// (1/sqrt kg), very light ones lose to the air.
static float ThrowMaxSpeed(int itemIdx) {
    float kg = ThrowUnitWeight(itemIdx);
    float f;
    if (kg > 1.0f) f = 1.0f / sqrtf(kg);
    else if (kg < 0.2f) f = 0.55f + 0.45f * (kg / 0.2f);
    else f = 1.0f;
    return throwSpeed * f;
}

float ThrowRange(int itemIdx) {
    float v = ThrowMaxSpeed(itemIdx);
    return throwGravity > 0.0f ? v * v / throwGravity : 0.0f;   // 45 degrees, flat ground
}

// ---------------------------------------------------------------------------------------------
// The world as a flying thing sees it

static bool ThrowCellBlocked(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || z < 0) return true;
    if (z >= gridDepth) return false;   // open sky above the map
    CellType c = GRID(x, y, z);
    return CellIsSolid(c) || CellBlocksMovement(c);
}

// A floor (or the map bottom) between level z-1 and level z at (x, y)
static bool ThrowFloorAt(int x, int y, int z) {
    if (z <= 0) return true;
    if (z >= gridDepth) return false;
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) return true;
    return HAS_FLOOR(x, y, z);
}

// Something to lie on at level z: a floor, or solid ground below
static bool ThrowGroundAt(int x, int y, int z) {
    return ThrowFloorAt(x, y, z) || ThrowCellBlocked(x, y, z - 1);
}

static int ThrowShadowZ(int x, int y, int z) {
    if (z >= gridDepth) z = gridDepth - 1;
    while (z > 0 && !ThrowGroundAt(x, y, z)) z--;
    return z;
}

// Can it go from cell a to cell b (one step: each axis changes by at most one)?
static bool ThrowPassable(int ax, int ay, int az, int bx, int by, int bz) {
    if (ThrowCellBlocked(bx, by, bz)) return false;
    if (bz != az) {
        int upper = bz > az ? bz : az;
        if (ThrowFloorAt(bx, by, upper) || ThrowFloorAt(ax, ay, upper)) return false;
    }
    return true;
}

static bool ThrowInWater(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    return GetWaterLevel(x, y, z) >= 2;
}

// ---------------------------------------------------------------------------------------------
// Flight physics (shared by real flights and the thrower's plan)

typedef enum { FLY_ON, FLY_REST, FLY_WATER } FlyState;

// One sub-step. Returns FLY_REST when it came to lie still, FLY_WATER when it fell into water.
// *firstDown: set true the first time it hits the ground (for planning).
static FlyState ThrowSubStep(Flight* f, float dt, bool* hitGround, int* bounces) {
    int ax = (int)floorf(f->x), ay = (int)floorf(f->y), az = (int)floorf(f->h);

    if (f->rolling) {
        float sp = sqrtf(f->vx * f->vx + f->vy * f->vy);
        float ns = sp - THROW_ROLL_FRICTION * dt;
        if (ns < THROW_REST_SPEED) return FLY_REST;
        f->vx *= ns / sp; f->vy *= ns / sp;
        float nx = f->x + f->vx * dt, ny = f->y + f->vy * dt;
        int bx = (int)floorf(nx), by = (int)floorf(ny);
        if (bx != ax || by != ay) {
            bool blockX = bx != ax && !ThrowPassable(ax, ay, az, bx, ay, az);
            bool blockY = by != ay && !ThrowPassable(ax, ay, az, ax, by, az);
            bool blockXY = !ThrowPassable(ax, ay, az, bx, by, az);
            if (blockX || blockY || blockXY) {
                if (blockX || (!blockY && blockXY)) f->vx = -f->vx * throwBounce;
                if (blockY || (!blockX && blockXY)) f->vy = -f->vy * throwBounce;
                (*bounces)++;
                return FLY_ON;
            }
            if (!ThrowGroundAt(bx, by, az)) f->rolling = false;   // rolled off an edge: falls
        }
        f->x = nx; f->y = ny;
        return FLY_ON;
    }

    f->vz -= throwGravity * dt;
    float nx = f->x + f->vx * dt, ny = f->y + f->vy * dt, nh = f->h + f->vz * dt;
    int bx = (int)floorf(nx), by = (int)floorf(ny), bz = (int)floorf(nh);
    if (bx == ax && by == ay && bz == az) { f->x = nx; f->y = ny; f->h = nh; return FLY_ON; }

    if (ThrowPassable(ax, ay, az, bx, by, bz)) {
        f->x = nx; f->y = ny; f->h = nh;
        if (bz < az && ThrowInWater(bx, by, bz)) return FLY_WATER;
        return FLY_ON;
    }
    // Something in the way: which side did it hit? Each axis on its own.
    bool hitX = bx != ax && !ThrowPassable(ax, ay, az, bx, ay, az);
    bool hitY = by != ay && !ThrowPassable(ax, ay, az, ax, by, az);
    bool hitZ = bz != az && !ThrowPassable(ax, ay, az, ax, ay, bz);
    if (!hitX && !hitY && !hitZ) {   // only the corner: it hits the edge, every axis that changed
        hitX = bx != ax; hitY = by != ay; hitZ = bz != az;
    }
    (*bounces)++;
    if (hitX) { f->vx = -f->vx * throwBounce; f->vy *= THROW_WALL_KEEP; f->vz *= THROW_WALL_KEEP; }
    if (hitY) { f->vy = -f->vy * throwBounce; f->vx *= THROW_WALL_KEEP; f->vz *= THROW_WALL_KEEP; }
    if (hitZ) {
        bool down = bz < az;
        f->vz = -f->vz * throwBounce;
        if (down) {
            *hitGround = true;
            f->vx *= THROW_GROUND_KEEP; f->vy *= THROW_GROUND_KEEP;
            if (fabsf(f->vz) < THROW_ROLL_VZ) {
                f->vz = 0.0f;
                f->h = (float)az;
                f->rolling = true;
            }
        }
    }
    // It stays where it was this sub-step (they are short)
    return FLY_ON;
}

static void ThrowStepFlight(Flight* f, float dt, bool* hitGround, int* bounces, FlyState* st) {
    float sp = fabsf(f->vx) + fabsf(f->vy) + fabsf(f->vz);
    int n = (int)ceilf(sp * dt / THROW_STEP_LEN);
    if (n < 1) n = 1;
    if (n > 64) n = 64;
    float sdt = dt / (float)n;
    for (int s = 0; s < n; s++) {
        *st = ThrowSubStep(f, sdt, hitGround, bounces);
        int cx = (int)floorf(f->x), cy = (int)floorf(f->y), cz = (int)floorf(f->h);
        if (!ThrowCellBlocked(cx, cy, cz) && cz < gridDepth) { f->lastX = cx; f->lastY = cy; f->lastZ = cz; }
        if (*st != FLY_ON) return;
    }
}

// Launch velocity from (x, y, h) to goal (tx, ty, th) at `angle` degrees: the speed that gets
// there in a vacuum. False if no speed does (goal too high for this angle).
static bool ThrowSolve(float x, float y, float h, float tx, float ty, float th, float angleDeg,
                       float* vx, float* vy, float* vz) {
    float dx = tx - x, dy = ty - y;
    float D = sqrtf(dx * dx + dy * dy);
    float dh = th - h;
    float a = angleDeg * (THROW_PI / 180.0f);
    if (D < 0.01f) { *vx = *vy = 0.0f; *vz = 2.0f; return true; }
    float denom = 2.0f * cosf(a) * cosf(a) * (D * tanf(a) - dh);
    if (denom <= 0.0f) return false;
    float v = sqrtf(throwGravity * D * D / denom);
    *vx = dx / D * v * cosf(a);
    *vy = dy / D * v * cosf(a);
    *vz = v * sinf(a);
    return true;
}

static void ThrowClampSpeed(int itemIdx, float* vx, float* vy, float* vz) {
    float v = sqrtf(*vx * *vx + *vy * *vy + *vz * *vz);
    float vmax = ThrowMaxSpeed(itemIdx);
    if (v > vmax) { float k = vmax / v; *vx *= k; *vy *= k; *vz *= k; }
}

// Simulate a perfect throw (no noise) until it lies still; where? (cells, level)
static bool ThrowSimRest(float x, float y, float h, float vx, float vy, float vz,
                         float* outX, float* outY, int* outZ) {
    Flight f = { .x = x, .y = y, .h = h, .vx = vx, .vy = vy, .vz = vz, .thrower = -1,
                 .lastX = (int)x, .lastY = (int)y, .lastZ = (int)h };
    bool hitGround = false;
    int bounces = 0;
    FlyState st = FLY_ON;
    for (float t = 0.0f; t < THROW_MAX_TIME; t += 1.0f / 30.0f) {
        ThrowStepFlight(&f, 1.0f / 30.0f, &hitGround, &bounces, &st);
        if (st != FLY_ON) {
            int cx = (int)floorf(f.x), cy = (int)floorf(f.y), cz = (int)floorf(f.h);
            if (cz >= gridDepth || ThrowCellBlocked(cx, cy, cz)) { cx = f.lastX; cy = f.lastY; cz = f.lastZ; }
            *outX = f.x; *outY = f.y; *outZ = ThrowShadowZ(cx, cy, cz);
            return true;
        }
    }
    return false;
}

bool ThrowPlan(int itemIdx, float x, float y, float h, int gx, int gy, int gz, float tolerance, ThrowAim* aim) {
    float tx = gx + 0.5f, ty = gy + 0.5f, th = (float)gz;
    float dx = tx - x, dy = ty - y, D = sqrtf(dx * dx + dy * dy);
    if (D < 0.01f) { dx = 1.0f; dy = 0.0f; } else { dx /= D; dy /= D; }
    float vmax = ThrowMaxSpeed(itemIdx);
    // A comfortable arc first, then flatter or steeper ones (under a ceiling, over a wall).
    // For each: aim short by how far it hops and rolls on after coming down, until a perfect
    // throw would come to rest on the goal.
    static const float tries[] = { 35, 30, 40, 25, 45, 50, 20, 55, 60, 15, 65, 70 };
    for (int t = 0; t < (int)(sizeof(tries) / sizeof(tries[0])); t++) {
        float a = tries[t];
        float aimD = D;
        for (int iter = 0; iter < 4 && aimD > 0.05f; iter++) {
            float ax = x + dx * aimD, ay = y + dy * aimD;
            float vx, vy, vz;
            if (!ThrowSolve(x, y, h, ax, ay, th, a, &vx, &vy, &vz)) break;
            if (vx * vx + vy * vy + vz * vz > vmax * vmax) break;
            float rx, ry; int rz;
            if (!ThrowSimRest(x, y, h, vx, vy, vz, &rx, &ry, &rz)) break;
            float ex = rx - tx, ey = ry - ty;
            if (rz == gz && ex * ex + ey * ey <= tolerance * tolerance) {
                aim->angle = a; aim->x = ax; aim->y = ay; aim->h = th;
                return true;
            }
            if (rz != gz) break;              // came to rest on another level: not this arc
            aimD -= ex * dx + ey * dy;        // overshoot along the line: aim that much shorter
        }
    }
    return false;
}

// A perfect throw: how close does it pass to (tx, ty, th) before it first touches anything?
static float ThrowSimClosest(float x, float y, float h, float vx, float vy, float vz, float tx, float ty, float th) {
    Flight f = { .x = x, .y = y, .h = h, .vx = vx, .vy = vy, .vz = vz, .thrower = -1,
                 .lastX = (int)x, .lastY = (int)y, .lastZ = (int)h };
    float best = 1e9f;
    for (float t = 0.0f; t < 4.0f; t += 1.0f / 60.0f) {
        float x0 = f.x, y0 = f.y, h0 = f.h;
        bool hitGround = false;
        int bounces = 0;
        FlyState st = FLY_ON;
        ThrowStepFlight(&f, 1.0f / 60.0f, &hitGround, &bounces, &st);
        float sx = f.x - x0, sy = f.y - y0, sh = f.h - h0;
        float len2 = sx * sx + sy * sy + sh * sh;
        float u = len2 > 1e-8f ? ((tx - x0) * sx + (ty - y0) * sy + (th - h0) * sh) / len2 : 0.0f;
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        float dx = x0 + sx * u - tx, dy = y0 + sy * u - ty, dh = h0 + sh * u - th;
        float d = sqrtf(dx * dx + dy * dy + dh * dh);
        if (d < best) best = d;
        if (st != FLY_ON || hitGround || bounces > 0) break;
    }
    return best;
}

bool ThrowPlanHit(int itemIdx, float x, float y, float h, float tx, float ty, int tz, float tvx, float tvy, ThrowAim* aim) {
    float th = (float)tz + THROW_HIT_AIM_H;
    float vmax = ThrowMaxSpeed(itemIdx);
    // Flat arcs first: they get there sooner (a running animal has less time to move on)
    static const float tries[] = { 15, 20, 25, 30, 10, 35, 40, 45, 50 };
    for (int t = 0; t < (int)(sizeof(tries) / sizeof(tries[0])); t++) {
        float a = tries[t];
        float px = tx, py = ty, vx = 0.0f, vy = 0.0f, vz = 0.0f;
        bool ok = true;
        for (int iter = 0; iter < 3 && ok; iter++) {   // where will it be when the stone gets there?
            if (!ThrowSolve(x, y, h, px, py, th, a, &vx, &vy, &vz)) { ok = false; break; }
            float hs = sqrtf(vx * vx + vy * vy);
            float dx = px - x, dy = py - y;
            float time = hs > 0.01f ? sqrtf(dx * dx + dy * dy) / hs : 0.0f;
            px = tx + tvx * time; py = ty + tvy * time;
        }
        if (!ok || !ThrowSolve(x, y, h, px, py, th, a, &vx, &vy, &vz)) continue;
        if (vx * vx + vy * vy + vz * vz > vmax * vmax) continue;
        if (ThrowSimClosest(x, y, h, vx, vy, vz, px, py, th) > THROW_HIT_AIM_TOL) continue;   // in the way
        aim->angle = a; aim->x = px; aim->y = py; aim->h = th;
        return true;
    }
    return false;
}

// ~N(0, 1) from three uniforms (deterministic through raylib's seed)
static float ThrowGauss(void) {
    float s = 0.0f;
    for (int i = 0; i < 3; i++) s += GetRandomValue(0, 10000) / 10000.0f;
    return (s - 1.5f) * 2.0f;
}

void ThrowLaunch(int itemIdx, float x, float y, float h, ThrowAim aim, bool noisy, int thrower) {
    Item* it = &items[itemIdx];
    float tx = aim.x, ty = aim.y, angle = aim.angle;
    float vx, vy, vz;
    if (!ThrowSolve(x, y, h, tx, ty, aim.h, angle, &vx, &vy, &vz)) {
        // Too high for this arc: as steep as it gets, full strength
        float dx = tx - x, dy = ty - y, D = sqrtf(dx * dx + dy * dy);
        if (D < 0.01f) { dx = 1.0f; D = 1.0f; }
        float a = 60.0f * (THROW_PI / 180.0f), v = ThrowMaxSpeed(itemIdx);
        vx = dx / D * v * cosf(a); vy = dy / D * v * cosf(a); vz = v * sinf(a);
    }
    if (noisy && throwSpread > 0.0f) {
        float dx = tx - x, dy = ty - y, D = sqrtf(dx * dx + dy * dy);
        float yaw = throwSpread * (1.5f + 0.15f * D) * (THROW_PI / 180.0f) * ThrowGauss();
        float strength = 1.0f + throwSpread * (0.03f + 0.004f * D) * ThrowGauss();
        float c = cosf(yaw), s = sinf(yaw);
        float rx = vx * c - vy * s, ry = vx * s + vy * c;
        vx = rx * strength; vy = ry * strength; vz *= strength;
    }
    ThrowClampSpeed(itemIdx, &vx, &vy, &vz);

    if (flightCount >= THROW_MAX_FLIGHTS) {   // no room: it just drops
        it->state = ITEM_ON_GROUND;
        it->reservedBy = -1;
        it->x = x * CELL_SIZE; it->y = y * CELL_SIZE; it->z = (float)ThrowShadowZ((int)x, (int)y, (int)h);
        return;
    }
    if (it->state == ITEM_IN_STOCKPILE) RemoveItemFromStockpileSlot(it->x, it->y, (int)it->z);
    it->state = ITEM_IN_FLIGHT;
    it->reservedBy = -1;
    it->x = x * CELL_SIZE; it->y = y * CELL_SIZE; it->z = floorf(h);
    flights[flightCount++] = (Flight){ .item = itemIdx, .x = x, .y = y, .h = h, .vx = vx, .vy = vy, .vz = vz,
                                       .thrower = thrower, .lastX = (int)x, .lastY = (int)y, .lastZ = (int)h,
                                       .fromX = x, .fromY = y, .hitAnimal = -1 };
    throwCount++;
    EventLog("Throw: item %d (%s) by mover %d aimed at (%.1f,%.1f,%.0f), %.0f deg, %.1f cells/s",
             itemIdx, ItemName(it->type), thrower, tx, ty, aim.h, angle, sqrtf(vx * vx + vy * vy + vz * vz));
}

// It lies still (or splashed): an ordinary item on the ground again
static void ThrowLand(Flight* f) {
    Item* it = &items[f->item];
    int cx = (int)floorf(f->x), cy = (int)floorf(f->y), cz = (int)floorf(f->h);
    float px = f->x, py = f->y;
    if (cz >= gridDepth || ThrowCellBlocked(cx, cy, cz)) {   // rounding put it in a wall: last open cell
        cx = f->lastX; cy = f->lastY; cz = f->lastZ;
        px = cx + 0.5f; py = cy + 0.5f;
    }
    if (cz >= gridDepth) cz = gridDepth - 1;
    if (cz < 0) cz = 0;
    it->state = ITEM_ON_GROUND;
    it->x = px * CELL_SIZE; it->y = py * CELL_SIZE;
    it->z = (float)ThrowShadowZ(cx, cy, cz);
    EventLog("Throw: item %d came to rest at (%d,%d,%d)", f->item, cx, cy, (int)it->z);
}

// Where an animal runs from (pixels): the thrower, else where the stone was let go
static void ThrowSource(const Flight* f, float* x, float* y) {
    *x = f->fromX * CELL_SIZE; *y = f->fromY * CELL_SIZE;
    if (f->thrower >= 0 && f->thrower < moverCount && movers[f->thrower].active) {
        *x = movers[f->thrower].x; *y = movers[f->thrower].y;
    }
}

// The stone first came down: animals close by (not the one it hit) get a fright and run
static void ThrowStartle(Flight* f) {
    int sz = ThrowShadowZ((int)floorf(f->x), (int)floorf(f->y), (int)floorf(f->h));
    float fromX, fromY;
    ThrowSource(f, &fromX, &fromY);
    for (int i = 0; i < animalCount; i++) {
        const Animal* a = &animals[i];
        if (!a->active || i == f->hitAnimal || AnimalDazed(i)) continue;
        if ((int)a->z != sz) continue;
        float dx = a->x / CELL_SIZE - f->x, dy = a->y / CELL_SIZE - f->y;
        if (dx * dx + dy * dy > THROW_STARTLE_RADIUS * THROW_STARTLE_RADIUS) continue;
        ScareAnimal(i, fromX, fromY);
        throwStartleCount++;
        EventLog("Throw: item %d came down by animal %d: it runs", f->item, i);
    }
}

// Did the stone, moving from (x0,y0,h0) to where f is now, go through an animal? A hit scares
// the animal (it runs from the thrower, or from where the stone was let go) and the stone
// bounces off it, losing most of its speed.
static void ThrowHitAnimals(Flight* f, float x0, float y0, float h0) {
    if (f->hitAnimal >= 0) return;
    float speed = sqrtf(f->vx * f->vx + f->vy * f->vy + f->vz * f->vz);
    if (speed < THROW_HIT_SPEED) return;
    float sx = f->x - x0, sy = f->y - y0, sh = f->h - h0;
    float len2 = sx * sx + sy * sy;
    for (int i = 0; i < animalCount; i++) {
        const Animal* a = &animals[i];
        if (!a->active) continue;
        float ax = a->x / CELL_SIZE, ay = a->y / CELL_SIZE;
        float t = len2 > 1e-6f ? ((ax - x0) * sx + (ay - y0) * sy) / len2 : 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float px = x0 + sx * t - ax, py = y0 + sy * t - ay, ph = h0 + sh * t - a->z;
        if (px * px + py * py > THROW_HIT_RADIUS * THROW_HIT_RADIUS) continue;
        if (ph < 0.0f || ph > THROW_HIT_HEIGHT) continue;
        // Hit: dazed
        float fromX, fromY;
        ThrowSource(f, &fromX, &fromY);
        DazeAnimal(i, fromX, fromY);
        f->hitAnimal = i;
        float hx = x0 + sx * t, hy = y0 + sy * t, hh = h0 + sh * t;   // where it struck
        if (!ThrowCellBlocked((int)floorf(hx), (int)floorf(hy), (int)floorf(hh))) {
            f->x = hx; f->y = hy; f->h = hh;
        }
        f->vx *= -THROW_HIT_KEEP; f->vy *= -THROW_HIT_KEEP;
        if (f->vz > 0.0f) f->vz *= THROW_HIT_KEEP;
        f->rolling = false;
        throwHitCount++;
        EventLog("Throw: item %d hit animal %d at (%.1f,%.1f,%.1f), %.1f cells/s: dazed",
                 f->item, i, f->x, f->y, f->h, speed);
        return;
    }
}

void ThrowsTick(float dt) {
    if (flightCount == 0 || dt <= 0.0f) return;
    int w = 0;
    for (int i = 0; i < flightCount; i++) {
        Flight f = flights[i];
        Item* it = &items[f.item];
        if (!it->active || it->state != ITEM_IN_FLIGHT) continue;   // cleared or taken: forget it
        bool hitGround = false;
        int bounces = 0;
        FlyState st = FLY_ON;
        float x0 = f.x, y0 = f.y, h0 = f.h;
        ThrowStepFlight(&f, dt, &hitGround, &bounces, &st);
        throwBounceCount += bounces;
        if (st == FLY_ON) ThrowHitAnimals(&f, x0, y0, h0);
        if ((hitGround || st != FLY_ON) && !f.landed) { f.landed = true; ThrowStartle(&f); }
        f.age += dt;
        if (st != FLY_ON || f.age > THROW_MAX_TIME) { ThrowLand(&f); continue; }
        int cz = (int)floorf(f.h);
        if (cz >= gridDepth) cz = gridDepth - 1;
        if (cz < 0) cz = 0;
        it->x = f.x * CELL_SIZE; it->y = f.y * CELL_SIZE; it->z = (float)cz;
        flights[w++] = f;
    }
    flightCount = w;
}

void ThrowLandAll(void) {
    // After a load the flight list belongs to the old world: forget it. A save made mid-flight
    // has items in flight with no flight: they come down where they are.
    flightCount = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (!it->active || it->state != ITEM_IN_FLIGHT) continue;
        int cx = (int)(it->x / CELL_SIZE), cy = (int)(it->y / CELL_SIZE);
        Flight f = { .item = i, .x = it->x / CELL_SIZE, .y = it->y / CELL_SIZE, .h = it->z,
                     .lastX = cx, .lastY = cy, .lastZ = (int)it->z };
        ThrowLand(&f);
    }
}

bool ThrowFlightInfo(int i, int* itemIdx, float* x, float* y, float* h, int* shadowZ) {
    if (i < 0 || i >= flightCount) return false;
    const Flight* f = &flights[i];
    if (!items[f->item].active || items[f->item].state != ITEM_IN_FLIGHT) return false;
    int cz = (int)floorf(f->h);
    int sz = ThrowShadowZ((int)floorf(f->x), (int)floorf(f->y), cz);
    *itemIdx = f->item; *x = f->x; *y = f->y;
    *h = f->h - (float)sz;
    *shadowZ = sz;
    return true;
}

// ---------------------------------------------------------------------------------------------
// The throw job

// Where the mover lets go from: its feet plus a bit, a step toward the goal
static void ThrowReleasePoint(const Mover* m, int gx, int gy, float* x, float* y, float* h) {
    *x = m->x / CELL_SIZE;
    *y = m->y / CELL_SIZE;
    *h = floorf(m->z) + THROW_RELEASE_HEIGHT;
    (void)gx; (void)gy;
}

// A walkable cell near the goal to walk toward (the goal itself if it is one)
static bool ThrowApproachCell(int gx, int gy, int gz, int* ax, int* ay, int* az) {
    for (int r = 0; r <= 4; r++) {
        for (int dz = 0; dz <= 2; dz++) {
            int zs[2] = { gz - dz, gz + dz };
            for (int k = 0; k < (dz == 0 ? 1 : 2); k++) {
                int z = zs[k];
                if (z < 0 || z >= gridDepth) continue;
                for (int dy = -r; dy <= r; dy++)
                    for (int dx = -r; dx <= r; dx++) {
                        if (abs(dx) != r && abs(dy) != r) continue;
                        int x = gx + dx, y = gy + dy;
                        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
                        if (!IsCellWalkableAt(z, y, x)) continue;
                        *ax = x; *ay = y; *az = z;
                        return true;
                    }
            }
        }
    }
    return false;
}

static void ThrowStopWalking(Mover* m) {
    m->goal = (Point){ (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    m->needsRepath = true;
}

int ThrowFindItem(int moverIdx, float maxDist) {
    Mover* m = &movers[moverIdx];
    for (int h = 0; h < 2; h++)   // not its tool (a knife, an axe)
        if (m->hands[h] >= 0 && m->hands[h] != m->equippedTool && ItemThrowable(m->hands[h])) return m->hands[h];
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].state == ITEM_IN_CONTAINER && i != m->equippedTool && ItemThrowable(i) &&
            ItemIsInMoverInventory(moverIdx, i)) return i;
    int found = -1;
    float best = maxDist * CELL_SIZE * maxDist * CELL_SIZE;
    int mz = (int)m->z;
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (!it->active || it->reservedBy >= 0) continue;
        if (it->state != ITEM_ON_GROUND && it->state != ITEM_IN_STOCKPILE) continue;
        if ((int)it->z != mz || !ItemThrowable(i)) continue;
        float dx = it->x - m->x, dy = it->y - m->y, d = dx * dx + dy * dy;
        if (d <= best) { best = d; found = i; }
    }
    return found;
}

int ThrowReadyItem(int moverIdx, int itemIdx, const char** why) {
    // Out of its hands / pockets: at its feet, then picked up right away
    if (ItemIsInMoverInventory(moverIdx, itemIdx)) {
        if (items[itemIdx].reservedBy == moverIdx) ReleaseItemReservation(itemIdx);
        MoverTakeOut(moverIdx, itemIdx);
        if (items[itemIdx].reservedBy >= 0) ReleaseItemReservation(itemIdx);
    }
    if (items[itemIdx].reservedBy >= 0) { *why = "that item is taken"; return -1; }
    // One unit of a stack
    if (items[itemIdx].stackCount > 1) {
        // The unit keeps the stack's state (a stockpile unit leaves its slot when picked up,
        // like a seed for planting)
        int one = SplitStack(itemIdx, 1);
        if (one < 0) { *why = "couldn't split the stack"; return -1; }
        itemIdx = one;
    }
    return itemIdx;
}

bool ThrowPickUp(int moverIdx, int itemIdx) {
    Mover* m = &movers[moverIdx];
    Item* it = &items[itemIdx];
    if (!it->active || (it->state != ITEM_ON_GROUND && it->state != ITEM_IN_STOCKPILE)) return false;
    // Close, or in its cell (a mover arriving stops anywhere in the goal cell, the item may lie in a corner)
    const float pickR = CELL_SIZE * 0.75f;
    float dx = m->x - it->x, dy = m->y - it->y;
    bool sameCell = (int)(m->x / CELL_SIZE) == (int)(it->x / CELL_SIZE) && (int)(m->y / CELL_SIZE) == (int)(it->y / CELL_SIZE);
    if ((int)m->z != (int)it->z || (!sameCell && dx * dx + dy * dy >= pickR * pickR)) return false;
    if (it->state == ITEM_IN_STOCKPILE) RemoveItemFromStockpileSlot(it->x, it->y, (int)it->z);
    it->state = ITEM_CARRIED;
    MoverMakeHandsFor(moverIdx, itemIdx);
    return true;
}

bool ThrowAtAnimal(int moverIdx, int itemIdx, int animalIdx, bool launch) {
    const Mover* m = &movers[moverIdx];
    const Animal* a = &animals[animalIdx];
    float x, y, h;
    ThrowReleasePoint(m, 0, 0, &x, &y, &h);
    float avx, avy;
    AnimalVelocity(animalIdx, &avx, &avy);
    ThrowAim aim;
    if (!ThrowPlanHit(itemIdx, x, y, h, a->x / CELL_SIZE, a->y / CELL_SIZE, (int)a->z,
                      avx / CELL_SIZE, avy / CELL_SIZE, &aim)) return false;
    if (launch) ThrowLaunch(itemIdx, x, y, h, aim, true, moverIdx);
    return true;
}

void ThrowDropHeld(int moverIdx, int itemIdx) {
    if (itemIdx < 0 || !items[itemIdx].active || items[itemIdx].state != ITEM_CARRIED) return;
    const Mover* m = &movers[moverIdx];
    SafeDropItem(itemIdx, m->x, m->y, (int)m->z);
    if (items[itemIdx].reservedBy >= 0) ReleaseItemReservation(itemIdx);
}

int ThrowOrder(int moverIdx, int itemIdx, int gx, int gy, int gz, const char** why) {
    static const char* none = "";
    if (!why) why = &none;
    if (moverIdx < 0 || moverIdx >= moverCount || !movers[moverIdx].active) { *why = "no such mover"; return -1; }
    if (gx < 0 || gy < 0 || gz < 0 || gx >= gridWidth || gy >= gridHeight || gz >= gridDepth) { *why = "goal outside the map"; return -1; }
    Mover* m = &movers[moverIdx];

    // Something to throw: given, else in its hands, else in its pockets, else the nearest on the ground
    if (itemIdx < 0) itemIdx = ThrowFindItem(moverIdx, 1e6f);
    if (!ItemThrowable(itemIdx)) { *why = "nothing light enough to throw"; return -1; }

    if (m->currentJobId >= 0) CancelJob(m, moverIdx);
    itemIdx = ThrowReadyItem(moverIdx, itemIdx, why);
    if (itemIdx < 0) return -1;

    int jobId = CreateJob(JOBTYPE_THROW);
    if (jobId < 0) { *why = "job pool full"; return -1; }
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = itemIdx;
    job->targetMineX = gx; job->targetMineY = gy; job->targetMineZ = gz;
    job->step = THROW_STEP_FETCH;
    job->progress = 0.0f;
    ReserveItem(itemIdx, moverIdx);

    m->currentJobId = jobId;
    m->goal = (Point){ (int)(items[itemIdx].x / CELL_SIZE), (int)(items[itemIdx].y / CELL_SIZE), (int)items[itemIdx].z };
    m->needsRepath = true;
    RemoveMoverFromIdleList(moverIdx);
    EventLog("ThrowOrder: mover %d throws item %d (%s) at (%d,%d,%d)", moverIdx, itemIdx, ItemName(items[itemIdx].type), gx, gy, gz);
    return jobId;
}

JobRunResult RunJob_Throw(Job* job, void* moverPtr, float dt) {
    Mover* m = (Mover*)moverPtr;
    int moverIdx = (int)(m - movers);
    int gx = job->targetMineX, gy = job->targetMineY, gz = job->targetMineZ;

    if (job->step == THROW_STEP_FETCH) {
        int itemIdx = job->targetItem;
        if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;
        Item* it = &items[itemIdx];
        if (it->state != ITEM_ON_GROUND && it->state != ITEM_IN_STOCKPILE) return JOBRUN_FAIL;
        if (ThrowPickUp(moverIdx, itemIdx)) {
            job->carryingItem = itemIdx;
            job->targetItem = -1;
            job->step = THROW_STEP_POSITION;
            job->progress = THROW_REPLAN;   // check right away
            int ax, ay, az;
            if (ThrowApproachCell(gx, gy, gz, &ax, &ay, &az)) {
                job->targetAdjX = ax; job->targetAdjY = ay; job->workRequired = (float)az;
            } else {
                job->targetAdjX = (int)(m->x / CELL_SIZE); job->targetAdjY = (int)(m->y / CELL_SIZE);
                job->workRequired = m->z;
            }
            return JOBRUN_RUNNING;
        }
        if (m->pathLength == 0 && m->timeWithoutProgress > 3.0f) return JOBRUN_FAIL;
        return JOBRUN_RUNNING;
    }

    int itemIdx = job->carryingItem;
    if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;
    items[itemIdx].x = m->x; items[itemIdx].y = m->y; items[itemIdx].z = m->z;

    if (job->step == THROW_STEP_POSITION) {
        job->progress += dt;
        float x, y, h;
        ThrowReleasePoint(m, gx, gy, &x, &y, &h);
        if (job->progress >= THROW_REPLAN) {
            job->progress = 0.0f;
            ThrowAim aim;
            if (ThrowPlan(itemIdx, x, y, h, gx, gy, gz, THROW_AIM_TOLERANCE, &aim)) {
                job->workRequired = 0.0f;   // planned: plan again at the release (same spot, same answer)
                job->step = THROW_STEP_WINDUP;
                ThrowStopWalking(m);
                return JOBRUN_RUNNING;
            }
        }
        int ax = job->targetAdjX, ay = job->targetAdjY, az = (int)job->workRequired;
        if (m->goal.x != ax || m->goal.y != ay || m->goal.z != az) {
            m->goal = (Point){ ax, ay, az };
            m->needsRepath = true;
        }
        bool arrived = (int)(m->x / CELL_SIZE) == ax && (int)(m->y / CELL_SIZE) == ay && (int)m->z == az;
        if (arrived || (m->pathLength == 0 && m->timeWithoutProgress > 3.0f)) {
            // As close as it gets and still no clean throw: throw anyway (it may fall short)
            job->workRequired = 1.0f;   // no clean throw: full strength at 45 degrees
            job->step = THROW_STEP_WINDUP;
            ThrowStopWalking(m);
        }
        return JOBRUN_RUNNING;
    }

    if (job->step == THROW_STEP_WINDUP) {
        job->progress += dt;
        m->timeWithoutProgress = 0.0f;
        if (job->progress < THROW_WINDUP) return JOBRUN_RUNNING;
        float x, y, h;
        ThrowReleasePoint(m, gx, gy, &x, &y, &h);
        ThrowAim aim = { 45.0f, gx + 0.5f, gy + 0.5f, (float)gz };
        if (job->workRequired == 0.0f) ThrowPlan(itemIdx, x, y, h, gx, gy, gz, THROW_AIM_TOLERANCE, &aim);
        job->carryingItem = -1;
        ThrowLaunch(itemIdx, x, y, h, aim, true, moverIdx);
        return JOBRUN_DONE;
    }
    return JOBRUN_FAIL;
}
