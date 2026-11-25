// notice.c - movers notice each other (see notice.h)
#include "notice.h"
#include "mover.h"
#include "../core/time.h"
#include "relations.h"
#include "chase.h"
#include "furniture.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

MoverNotice moverNotice[MAX_MOVERS];
bool noticeEnabled = true;
float noticeRadius = 6.0f;
int noticeEveryTicks = 15;
bool politePassing = true;
bool socialSeek = true;
bool socialAvoid = true;
float passRadius = 4.0f;
float passStrength = 1.0f;
float passSlow = 0.0f;

static unsigned int noticeTickCounter = 0;
static unsigned int noticeClock = 0;   // ticks, for glance/greet timers (no per-tick loop over movers)

bool glancesEnabled = true, greetingsEnabled = true;
long glanceCount = 0, greetCount = 0;
#define GLANCE_TICKS     48     // 0.8 s
#define GREET_TICKS      60     // 1 s
#define GREET_RANGE      4.0f   // cells
#define GREET_APART      (180 * 60)  // ticks out of sight before the same two greet again (3 minutes)
#define GLANCE_STRANGER  30     // % chance to glance at a stranger
#define GREET_MEMORY     16
typedef struct { int who; unsigned int until; } GreetMemo;
static int glanceAt[MAX_MOVERS];
static unsigned int glanceUntil[MAX_MOVERS];
static bool glanceAway[MAX_MOVERS];
static unsigned char greetKind[MAX_MOVERS];
static unsigned int greetFrom[MAX_MOVERS];
static GreetMemo greeted[MAX_MOVERS][GREET_MEMORY];

// Salience bonuses on top of closeness (1 at the same spot, 0 at the radius edge)
#define NOTICE_STICKY_BONUS    0.35f   // already noticed: attention doesn't flick between equals
#define NOTICE_APPROACH_BONUS  0.25f   // walking toward me

static bool BlocksSight(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) return true;
    CellType c = GRID(x, y, z);
    if (c == CELL_RAILING || c == CELL_WINDOW) return false;
    return CellBlocksMovement(c);
}

bool CanSeeCell(int x0, int y0, int x1, int y1, int z) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err, nx = x, ny = y;
        if (e2 >= dy) { err += dy; nx += sx; }
        if (e2 <= dx) { err += dx; ny += sy; }
        if (nx != x && ny != y && BlocksSight(nx, y, z) && BlocksSight(x, ny, z)) return false;   // a closed corner
        x = nx; y = ny;
        if ((x != x1 || y != y1) && BlocksSight(x, y, z)) return false;
    }
    return true;
}

// --- Cover: crouching behind something low ---
bool coverEnabled = true;
#define COVER_OVERLOOK 2.0f   // cells: this close, you look over the cover

bool IsLowCover(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) return false;
    CellType c = GRID(x, y, z);
    if (c == CELL_RAILING || c == CELL_BUSH) return true;
    if (!FURNITURE_MOVE_COST(x, y, z)) return false;   // no furniture here (cheap test first)
    int f = GetFurnitureAt(x, y, z);
    if (f < 0) return false;
    switch (furniture[f].type) {
        case FURNITURE_TABLE: case FURNITURE_DESK: case FURNITURE_SOFA: case FURNITURE_TILL:
        case FURNITURE_BATH: case FURNITURE_FRIDGE: return true;
        default: return false;
    }
}

// Low cover right at the target, on the side the viewer looks from: the target's own cell is
// a bush, or the last cell(s) the line passes before reaching it are cover
bool CoverBetween(int x0, int y0, int x1, int y1, int z) {
    if (GRID(x1, y1, z) == CELL_BUSH) return true;   // in a bush: hidden from every side
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    int x = x0, y = y0, px = x0, py = y0;
    while (x != x1 || y != y1) {   // walk to the target, remembering the cell before it
        int e2 = 2 * err;
        px = x; py = y;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
    if (px == x1 && py == y1) return false;
    if (IsLowCover(px, py, z)) return true;
    if (px != x1 && py != y1)   // came in diagonally: either corner covers
        return IsLowCover(px, y1, z) || IsLowCover(x1, py, z);
    return false;
}

bool CanSeeMover(int viewer, int target) {
    const Mover* a = &movers[viewer];
    const Mover* b = &movers[target];
    int z = (int)a->z;
    if ((int)b->z != z) return false;
    int ax = (int)(a->x / CELL_SIZE), ay = (int)(a->y / CELL_SIZE);
    int bx = (int)(b->x / CELL_SIZE), by = (int)(b->y / CELL_SIZE);
    if (!CanSeeCell(ax, ay, bx, by, z)) return false;
    if (!coverEnabled || !MoverCrouching(target)) return true;
    float dx = (b->x - a->x) / CELL_SIZE, dy = (b->y - a->y) / CELL_SIZE;
    if (dx * dx + dy * dy <= COVER_OVERLOOK * COVER_OVERLOOK) return true;
    return !CoverBetween(ax, ay, bx, by, z);
}

void ClearNotices(void) {
    memset(moverNotice, 0, sizeof(moverNotice));
    noticeTickCounter = 0;
    noticeClock = 0;
    memset(glanceUntil, 0, sizeof(glanceUntil));
    memset(greetKind, 0, sizeof(greetKind));
    memset(greeted, 0, sizeof(greeted));
    glanceCount = greetCount = 0;
}

int NoticeTop(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return -1;
    const MoverNotice* n = &moverNotice[moverIdx];
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j >= 0 && j < moverCount && movers[j].active) return j;
    }
    return -1;
}

// Unit direction mover j walks in (toward its next waypoint); false when it isn't walking.
static bool MoverHeading(int j, float* hx, float* hy) {
    const Mover* o = &movers[j];
    if (o->pathLength <= 0 || o->pathIndex < 0) return false;
    Point wp = moverPaths[j][o->pathIndex];
    if (wp.z != (int)o->z) return false;   // climbing: not walking on the level
    float dx = wp.x * CELL_SIZE + CELL_SIZE * 0.5f - o->x;
    float dy = wp.y * CELL_SIZE + CELL_SIZE * 0.5f - o->y;
    float l = sqrtf(dx * dx + dy * dy);
    if (l < 0.5f) return false;
    *hx = dx / l; *hy = dy / l;
    return true;
}

// Is mover j walking toward point (x, y)?
static bool HeadingToward(int j, float x, float y) {
    float hx, hy;
    if (!MoverHeading(j, &hx, &hy)) return false;
    float tx = x - movers[j].x, ty = y - movers[j].y;
    float tl = sqrtf(tx * tx + ty * ty);
    if (tl < 0.5f) return false;
    return (hx * tx + hy * ty) / tl > 0.7f;
}

static const NoticeEntry* FindEntry(const MoverNotice* n, int j) {
    for (int k = 0; k < n->count; k++) if (n->e[k].mover == j) return &n->e[k];
    return NULL;
}

int NoticeGlanceAt(int i, bool* away) {
    if (i < 0 || i >= moverCount || noticeClock >= glanceUntil[i]) return -1;
    int j = glanceAt[i];
    if (j < 0 || j >= moverCount || !movers[j].active) return -1;
    if (away) *away = glanceAway[i];
    return j;
}

int NoticeGreeting(int i, float* phase) {
    if (i < 0 || i >= moverCount || greetKind[i] == GREET_NONE) return GREET_NONE;
    unsigned int age = noticeClock - greetFrom[i];
    if (age >= GREET_TICKS) { greetKind[i] = GREET_NONE; return GREET_NONE; }
    if (phase) *phase = age / (float)GREET_TICKS;
    return greetKind[i];
}

static bool GreetedRecently(int i, int j) {
    for (int k = 0; k < GREET_MEMORY; k++)
        if (greeted[i][k].who == j + 1 && noticeClock < greeted[i][k].until) return true;
    return false;
}

static void RememberGreet(int i, int j) {
    int slot = 0;
    for (int k = 0; k < GREET_MEMORY; k++)   // a free or the oldest slot
        if (noticeClock >= greeted[i][k].until) { slot = k; break; }
        else if (greeted[i][k].until < greeted[i][slot].until) slot = k;
    greeted[i][slot] = (GreetMemo){ j + 1, noticeClock + GREET_APART };
}

// Still in sight of someone it greeted: the greeting holds (no new one until they've been apart)
static void KeepGreeted(int i, int j) {
    for (int k = 0; k < GREET_MEMORY; k++)
        if (greeted[i][k].who == j + 1 && noticeClock < greeted[i][k].until) {
            greeted[i][k].until = noticeClock + GREET_APART;
            return;
        }
}

static void StartGreet(int i, int j, int kind) {
    greetKind[i] = (unsigned char)kind;
    greetFrom[i] = noticeClock;
    glanceAt[i] = j;
    glanceAway[i] = false;
    glanceUntil[i] = noticeClock + GREET_TICKS;
    RememberGreet(i, j);
}

// Someone it has just noticed: glance, greet, or look away
static void ReactToNewlyNoticed(int i, int j) {
    float like = RelationLike(i, j), fam = RelationFamiliarity(i, j);
    float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
    bool near = dx * dx + dy * dy <= (GREET_RANGE * CELL_SIZE) * (GREET_RANGE * CELL_SIZE);
    if (like <= -20.0f) {   // someone it dislikes: look away
        if (!glancesEnabled) return;
        glanceAt[i] = j; glanceAway[i] = true; glanceUntil[i] = noticeClock + GLANCE_TICKS;
        greetKind[i] = GREET_AWAY; greetFrom[i] = noticeClock;
        glanceCount++;
        return;
    }
    if (greetingsEnabled && fam >= 15.0f && near && !GreetedRecently(i, j)) {
        int kind = like >= 50.0f ? GREET_HI : GREET_NOD;
        StartGreet(i, j, kind);
        // they greet back if they've seen it too and know it (one greeting each)
        if (!GreetedRecently(j, i) && RelationLike(j, i) > -20.0f && RelationFamiliarity(j, i) >= 15.0f)
            StartGreet(j, i, RelationLike(j, i) >= 50.0f ? GREET_HI : GREET_NOD);
        RelationAdd(i, j, 0.5f, 0.5f);
        RelationAdd(j, i, 0.5f, 0.5f);
        greetCount++;
        return;
    }
    if (!glancesEnabled || movers[i].pathIndex < 0) return;   // glances: while walking past
    if (fam < 15.0f && GetRandomValue(0, 99) >= GLANCE_STRANGER) return;
    glanceAt[i] = j; glanceAway[i] = false; glanceUntil[i] = noticeClock + GLANCE_TICKS;
    glanceCount++;
}

void NoticeMover(int i) {
    MoverNotice* n = &moverNotice[i];
    const Mover* m = &movers[i];
    if (!noticeEnabled || !m->active || !moverGrid.cellCounts) { n->count = 0; return; }

    MoverNotice fresh = {0};
    float radius = noticeRadius * CELL_SIZE;
    float radiusSq = radius * radius;
    int z = (int)m->z;

    int radCells = (int)ceilf(radius * moverGrid.invCellSize);
    int gx0 = (int)(m->x * moverGrid.invCellSize), gy0 = (int)(m->y * moverGrid.invCellSize);
    int minGx = gx0 - radCells < 0 ? 0 : gx0 - radCells;
    int minGy = gy0 - radCells < 0 ? 0 : gy0 - radCells;
    int maxGx = gx0 + radCells > moverGrid.gridW - 1 ? moverGrid.gridW - 1 : gx0 + radCells;
    int maxGy = gy0 + radCells > moverGrid.gridH - 1 ? moverGrid.gridH - 1 : gy0 + radCells;

    for (int gy = minGy; gy <= maxGy; gy++) {
        for (int gx = minGx; gx <= maxGx; gx++) {
            int cell = gy * moverGrid.gridW + gx;
            for (int t = moverGrid.cellStarts[cell]; t < moverGrid.cellStarts[cell + 1]; t++) {
                int j = moverGrid.moverIndices[t];
                if (j == i || !movers[j].active || (int)movers[j].z != z) continue;
                float dx = movers[j].x - m->x, dy = movers[j].y - m->y;
                float dSq = dx * dx + dy * dy;
                if (dSq > radiusSq) continue;
                if (!CanSeeMover(i, j)) continue;
                KeepGreeted(i, j);   // in sight (noticed or not): a greeting still holds

                const NoticeEntry* old = FindEntry(n, j);
                float s = 1.0f - sqrtf(dSq) / radius;
                if (old) s += NOTICE_STICKY_BONUS;
                const Relation* rel = RelationGet(i, j);   // people you know (or feel strongly about) catch your eye
                if (rel) s += rel->familiarity * 0.003f + fabsf(rel->like) * 0.003f;
                if (HeadingToward(j, m->x, m->y)) s += NOTICE_APPROACH_BONUS;

                // Insert into the top-N, most salient first
                int pos = fresh.count;
                while (pos > 0 && fresh.e[pos - 1].salience < s) pos--;
                if (pos >= NOTICE_MAX) continue;
                int last = fresh.count < NOTICE_MAX ? fresh.count : NOTICE_MAX - 1;
                for (int k = last; k > pos; k--) fresh.e[k] = fresh.e[k - 1];
                fresh.e[pos] = (NoticeEntry){ j, old ? old->since : gameTime, s };
                if (fresh.count < NOTICE_MAX) fresh.count++;
            }
        }
    }
    // The most salient one it hadn't noticed before: glance / greet / look away (unless busy with one)
    if ((glancesEnabled || greetingsEnabled) && noticeClock >= glanceUntil[i])
        for (int k = 0; k < fresh.count; k++)
            if (!FindEntry(n, fresh.e[k].mover)) { ReactToNewlyNoticed(i, fresh.e[k].mover); break; }
    *n = fresh;
    // Seeing someone makes them familiar, slowly
    float seenSec = (noticeEveryTicks < 1 ? 1 : noticeEveryTicks) / (float)TICK_RATE;
    for (int k = 0; k < n->count; k++) RelationAdd(i, n->e[k].mover, 0.0f, relSeenFamiliarityPerSec * seenSec);
}

void NoticeTick(void) {
    if (!noticeEnabled) return;
    int every = noticeEveryTicks < 1 ? 1 : noticeEveryTicks;
    noticeClock++;
    unsigned int phase = noticeTickCounter++ % (unsigned int)every;
    for (int i = (int)phase; i < moverCount; i += every) NoticeMover(i);
}

// --- Passing politely ---
#define PASS_LANE_HALF   1.2f    // cells: the other counts as "in my way" within this of my line
#define PASS_CENTRED     0.15f   // cells: closer to my line than this = dead ahead: keep right
#define PASS_ONCOMING   -0.5f    // heading dot product below this = coming at me

bool NoticePassSteer(int i, float hx, float hy, float* sideX, float* sideY, float* speedScale) {
    *sideX = *sideY = 0.0f;
    *speedScale = 1.0f;
    if (!politePassing || !noticeEnabled) return false;
    const Mover* m = &movers[i];
    const MoverNotice* n = &moverNotice[i];
    float radius = passRadius * CELL_SIZE;
    float rx = -hy, ry = hx;   // my right (screen y points down)
    float sx = 0.0f, sy = 0.0f, maxW = 0.0f;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !movers[j].active || (int)movers[j].z != (int)m->z) continue;
        float jhx, jhy;
        if (!MoverHeading(j, &jhx, &jhy)) continue;              // standing still: separation handles it
        if (hx * jhx + hy * jhy > PASS_ONCOMING) continue;       // not coming at me
        float dx = movers[j].x - m->x, dy = movers[j].y - m->y;
        float ahead = dx * hx + dy * hy;
        if (ahead <= 0.0f || ahead > radius) continue;
        float lateral = dx * rx + dy * ry;                       // > 0: on my right
        float space = PersonalSpaceScale(i, j);                  // friends pass close
        if (space > 1.0f) space = 1.0f;   // (not wider for strangers: a stronger drift pushes through one-way doors)
        if (fabsf(lateral) > PASS_LANE_HALF * space * CELL_SIZE) continue;
        float w = (1.0f - ahead / radius) * space;
        float side = lateral > PASS_CENTRED * CELL_SIZE ? -1.0f : 1.0f;   // away from them, else right
        sx += side * rx * w;
        sy += side * ry * w;
        if (w > maxW) maxW = w;
    }
    if (maxW <= 0.0f) return false;
    *sideX = sx;
    *sideY = sy;
    *speedScale = 1.0f - passSlow * maxW;
    return true;
}

// --- Friends and foes ---
#define LIKE_SEEK     20.0f   // like at or above: worth walking over to
#define LIKE_AVOID   -20.0f   // like at or below: keep away
#define SEEK_CHANCE   0.6f    // a wander goal near a liked one instead of a random one
#define SEEK_RANGE   12.0f    // cells: friends it knows this close count (not only ones in view)
#define AVOID_LOOK    3.0f    // cells ahead where a walker starts to steer round a disliked one
#define AVOID_LANE    1.5f    // cells: ... if they're this close to its line
#define AVOID_GOAL    6.0f    // cells: a wander goal this close to a disliked one is refused
#define AVOID_LEAVE   2.5f    // cells: standing about and a disliked one comes this close: leave

bool NoticeSocialWanderGoal(int i, int* gx, int* gy, int* gz) {
    if (!socialSeek || !noticeEnabled) return false;
    // The one it likes most among those it knows within SEEK_RANGE (you know roughly where your
    // friends are; not only the ones in view)
    const MoverRelations* mr = &moverRelations[i];
    int best = -1;
    float bestLike = LIKE_SEEK, range = SEEK_RANGE * CELL_SIZE;
    for (int k = 0; k < mr->count; k++) {
        int j = mr->r[k].other;
        if (mr->r[k].like < bestLike || !RelationGet(i, j)) continue;
        if ((int)movers[j].z != (int)movers[i].z) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > range * range) continue;
        bestLike = mr->r[k].like;
        best = j;
    }
    if (best < 0 || GetRandomValue(0, 99) >= (int)(SEEK_CHANCE * 100.0f)) return false;
    int bx = (int)(movers[best].x / CELL_SIZE), by = (int)(movers[best].y / CELL_SIZE), z = (int)movers[best].z;
    for (int tries = 0; tries < 8; tries++) {   // a free walkable cell 1-2 cells from them
        int x = bx + GetRandomValue(-2, 2), y = by + GetRandomValue(-2, 2);
        if ((x == bx && y == by) || x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
        if (!IsCellWalkableAt(z, y, x)) continue;
        *gx = x; *gy = y; *gz = z;
        return true;
    }
    return false;
}

bool NoticeGoalNearDisliked(int i, int gx, int gy, int gz) {
    if (!socialAvoid || !noticeEnabled) return false;
    const MoverNotice* n = &moverNotice[i];
    float r = AVOID_GOAL * CELL_SIZE;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !movers[j].active || (int)movers[j].z != gz) continue;
        if (RelationLike(i, j) > LIKE_AVOID) continue;
        float dx = (gx + 0.5f) * CELL_SIZE - movers[j].x, dy = (gy + 0.5f) * CELL_SIZE - movers[j].y;
        if (dx * dx + dy * dy < r * r) return true;
    }
    return false;
}

bool NoticeDislikedClose(int i) {
    if (!socialAvoid || !noticeEnabled) return false;
    const MoverNotice* n = &moverNotice[i];
    float r = AVOID_LEAVE * CELL_SIZE;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !movers[j].active || (int)movers[j].z != (int)movers[i].z) continue;
        if (RelationLike(i, j) > LIKE_AVOID) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy < r * r) return true;
    }
    return false;
}

bool NoticeAvoidSteer(int i, float hx, float hy, float* sideX, float* sideY) {
    *sideX = *sideY = 0.0f;
    if (!socialAvoid || !noticeEnabled) return false;
    const Mover* m = &movers[i];
    const MoverNotice* n = &moverNotice[i];
    float look = AVOID_LOOK * CELL_SIZE, lane = AVOID_LANE * CELL_SIZE;
    float rx = -hy, ry = hx;
    bool any = false;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !movers[j].active || (int)movers[j].z != (int)m->z) continue;
        float like = RelationLike(i, j);
        if (like > LIKE_AVOID) continue;
        float dx = movers[j].x - m->x, dy = movers[j].y - m->y;
        float ahead = dx * hx + dy * hy;
        if (ahead <= 0.0f || ahead > look) continue;
        float lateral = dx * rx + dy * ry;
        if (fabsf(lateral) > lane) continue;
        float w = (1.0f - ahead / look) * (1.0f - fabsf(lateral) / lane * 0.5f);
        w *= fminf(1.0f, -like / 50.0f);           // the more disliked, the wider the berth
        float side = lateral > PASS_CENTRED * CELL_SIZE ? -1.0f : 1.0f;
        *sideX += side * rx * w;
        *sideY += side * ry * w;
        any = true;
    }
    return any;
}
