// chase.c - chasing and fleeing, and a game of tag (see chase.h)
#include "chase.h"
#include "notice.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <math.h>
#include <string.h>

bool fleeSmart = true;
float chaseSpeed = 1.25f;
float fleeSpeed = 1.2f;
float fleeRadius = 5.0f;
long tagCount = 0;
double tagChaseSecondsSum = 0.0;

static bool playing[MAX_MOVERS];
static float stamina[MAX_MOVERS];   // 0..1: running drains it, resting refills it
static bool fleeing[MAX_MOVERS];
static float replanTimer[MAX_MOVERS];
static int tagIt = -1;
static int tagTarget = -1;
static float tagTargetHeld = 0.0f;   // seconds on the current target
static float tagFreeze = 0.0f;       // the new "it" counting to three
static float tagChaseTime = 0.0f;    // since the last tag

#define STAMINA_DRAIN    0.125f  // per second fleeing: 8 s from fresh to spent
#define STAMINA_REST     0.1f    // per second not fleeing: 10 s to recover
#define TIRED            0.3f    // stamina below this: worn out
#define TIRED_SPEED      0.85f   // a worn-out runner's pace, x walking speed
#define CHASE_REPLAN     0.5f    // seconds between path re-plans while chasing / fleeing
#define CHASE_LEAD_MAX   3.0f    // cells: how far ahead of the target it aims at most
#define CATCH_DIST       0.6f    // cells: tagged
#define TAG_FREEZE       1.5f    // seconds the new "it" stands (counts to three)
#define TARGET_STICK     2.0f    // seconds before "it" may switch to a closer player
#define FLEE_MIN         5.0f    // cells: candidate flee spots between these
#define FLEE_MAX         9.0f
#define FLEE_CANDIDATES  12
#define FLEE_REPICK      2.5f    // cells: the threat this close -> pick a new spot (if the old one is no good)

static float ChaseRandRange(float lo, float hi) {
    return lo + (hi - lo) * (GetRandomValue(0, 10000) / 10000.0f);
}

void ClearChase(void) {
    memset(playing, 0, sizeof(playing));
    memset(fleeing, 0, sizeof(fleeing));
    memset(replanTimer, 0, sizeof(replanTimer));
    for (int i = 0; i < MAX_MOVERS; i++) stamina[i] = 1.0f;
    tagIt = tagTarget = -1;
    tagTargetHeld = tagFreeze = tagChaseTime = 0.0f;
    tagCount = 0;
    tagChaseSecondsSum = 0.0;
}

void ChaseSetPlayer(int i, bool on) { if (i >= 0 && i < MAX_MOVERS) { playing[i] = on; stamina[i] = 1.0f; } }
float ChaseStamina(int i) { return (i >= 0 && i < MAX_MOVERS) ? stamina[i] : 1.0f; }
bool ChaseIsPlaying(int i) { return i >= 0 && i < moverCount && playing[i] && movers[i].active; }
bool ChaseIsFleeing(int i) { return ChaseIsPlaying(i) && fleeing[i]; }
int ChaseIt(void) { return (tagIt >= 0 && tagIt < moverCount && movers[tagIt].active) ? tagIt : -1; }
void ChaseSetIt(int i) { tagIt = i; tagTarget = -1; tagFreeze = TAG_FREEZE; tagChaseTime = 0.0f; }

float ChaseSpeedScale(int i) {
    if (!ChaseIsPlaying(i)) return 1.0f;
    if (i == tagIt) return chaseSpeed;
    if (!fleeing[i]) return 1.0f;
    return stamina[i] > TIRED ? fleeSpeed : TIRED_SPEED;
}

static float Dist(int a, int b) {
    float dx = movers[a].x - movers[b].x, dy = movers[a].y - movers[b].y;
    return sqrtf(dx * dx + dy * dy);
}

// Unit direction mover j walks in (next waypoint it isn't standing on); false if not walking
static bool Heading(int j, float* hx, float* hy) {
    const Mover* o = &movers[j];
    if (o->pathLength <= 0 || o->pathIndex < 0) return false;
    for (int k = o->pathIndex; k >= 0; k--) {
        Point wp = moverPaths[j][k];
        float dx = wp.x * CELL_SIZE + CELL_SIZE * 0.5f - o->x, dy = wp.y * CELL_SIZE + CELL_SIZE * 0.5f - o->y;
        float l = sqrtf(dx * dx + dy * dy);
        if (l < CELL_SIZE * 0.5f) continue;
        *hx = dx / l; *hy = dy / l;
        return true;
    }
    return false;
}

static bool PathTo(int i, int gx, int gy, int gz) {
    Mover* m = &movers[i];
    if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellWalkableAt(gz, gy, gx)) return false;
    Point start = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
    Point goal = {gx, gy, gz};
    m->goal = goal;
    if (MoverPlanPath(i, start, goal) <= 0) return false;
    m->needsRepath = false;
    return true;
}

// --- Chase: run at where the target is going ---
static void ChaseRun(int i, int target) {
    const Mover* t = &movers[target];
    float d = Dist(i, target) / CELL_SIZE;
    float lead = d < 1.5f ? 0.0f : d * 0.5f;   // close: straight at it
    if (lead > CHASE_LEAD_MAX) lead = CHASE_LEAD_MAX;
    float hx = 0.0f, hy = 0.0f;
    Heading(target, &hx, &hy);
    int z = (int)t->z;
    // aim ahead of it; if that cell is no good, at it
    int ax = (int)((t->x + hx * lead * CELL_SIZE) / CELL_SIZE), ay = (int)((t->y + hy * lead * CELL_SIZE) / CELL_SIZE);
    if (!PathTo(i, ax, ay, z)) PathTo(i, (int)(t->x / CELL_SIZE), (int)(t->y / CELL_SIZE), z);
}

// --- Flee: run to a good spot away from the threat ---
static bool FleeSpotStillGood(int i, int threat) {
    const Mover* m = &movers[i];
    if (m->pathIndex < 0 || m->pathLength <= 0) return false;
    // still heading away from the threat?
    float gx = (m->goal.x + 0.5f) * CELL_SIZE - m->x, gy = (m->goal.y + 0.5f) * CELL_SIZE - m->y;
    float tx = movers[threat].x - m->x, ty = movers[threat].y - m->y;
    return gx * tx + gy * ty < 0.0f;
}

static void FleeRun(int i, int threat) {
    const Mover* m = &movers[i];
    int z = (int)m->z;
    float ax = m->x - movers[threat].x, ay = m->y - movers[threat].y;   // away from the threat
    float al = sqrtf(ax * ax + ay * ay);
    if (al < 0.5f) { ax = 1.0f; ay = 0.0f; al = 1.0f; }
    ax /= al; ay /= al;
    if (!fleeSmart) {   // straight away, as far as the map allows (how animals flee today)
        for (float d = FLEE_MAX; d >= 1.0f; d -= 1.0f)
            if (PathTo(i, (int)((m->x + ax * d * CELL_SIZE) / CELL_SIZE), (int)((m->y + ay * d * CELL_SIZE) / CELL_SIZE), z)) return;
        return;
    }
    // Candidates spread round the "away" direction (up to +-100 degrees), 5-9 cells out
    int bestX = -1, bestY = -1;
    float bestScore = -1e9f;
    float base = atan2f(ay, ax);
    for (int c = 0; c < FLEE_CANDIDATES; c++) {
        float a = base + ChaseRandRange(-1.75f, 1.75f), d = ChaseRandRange(FLEE_MIN, FLEE_MAX);
        int cx = (int)((m->x + cosf(a) * d * CELL_SIZE) / CELL_SIZE), cy = (int)((m->y + sinf(a) * d * CELL_SIZE) / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || !IsCellWalkableAt(z, cy, cx)) continue;
        if (!HasLineOfSight((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), cx, cy, z)) continue;   // a straight run
        float px = (cx + 0.5f) * CELL_SIZE, py = (cy + 0.5f) * CELL_SIZE;
        float fromThreat = sqrtf((px - movers[threat].x) * (px - movers[threat].x) + (py - movers[threat].y) * (py - movers[threat].y));
        // room to run on from there (not a corner): walkable neighbours
        int open = 0;
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                if ((ox || oy) && IsCellWalkableAt(z, cy + oy, cx + ox)) open++;
        float score = fromThreat / CELL_SIZE + open * 0.5f;
        if (score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
    }
    if (bestX < 0 || !PathTo(i, bestX, bestY, z)) {   // nothing in the open: straight away
        for (float d = FLEE_MAX; d >= 1.0f; d -= 1.0f)
            if (PathTo(i, (int)((m->x + ax * d * CELL_SIZE) / CELL_SIZE), (int)((m->y + ay * d * CELL_SIZE) / CELL_SIZE), z)) return;
    }
}

// Is the threat close enough to run from (and has it been seen: noticed, or very close)?
static bool ThreatClose(int i, int threat) {
    if (threat < 0 || (int)movers[threat].z != (int)movers[i].z) return false;
    float d = Dist(i, threat);
    if (d > fleeRadius * CELL_SIZE) return false;
    const MoverNotice* n = &moverNotice[i];
    for (int k = 0; k < n->count; k++) if (n->e[k].mover == threat) return true;
    return d < 2.0f * CELL_SIZE;   // too close not to notice
}

void ChaseTick(float dt) {
    int it = ChaseIt();
    if (it < 0) return;
    tagChaseTime += dt;
    if (tagFreeze > 0.0f) tagFreeze -= dt;

    // "it": pick (or keep) a target and run at it
    if (tagFreeze <= 0.0f) {
        tagTargetHeld += dt;
        int best = -1;
        float bestD = 1e9f;
        const MoverNotice* n = &moverNotice[it];
        for (int k = 0; k < n->count; k++) {
            int j = n->e[k].mover;
            if (!ChaseIsPlaying(j) || j == it || (int)movers[j].z != (int)movers[it].z) continue;
            float d = Dist(it, j);
            if (d < bestD) { bestD = d; best = j; }
        }
        bool keep = tagTarget >= 0 && ChaseIsPlaying(tagTarget) &&
                    (tagTargetHeld < TARGET_STICK || best < 0 || Dist(it, tagTarget) < bestD * 1.5f);
        if (!keep && best >= 0) { tagTarget = best; tagTargetHeld = 0.0f; }
        if (!keep && best < 0) tagTarget = -1;   // nobody in sight: wander (the mover loop picks goals)
        if (tagTarget >= 0) {
            if (Dist(it, tagTarget) < CATCH_DIST * CELL_SIZE) {   // tagged!
                tagCount++;
                tagChaseSecondsSum += tagChaseTime;
                int old = it;
                ChaseSetIt(tagTarget);
                fleeing[tagIt] = false;
                movers[tagIt].pathLength = 0;
                movers[tagIt].pathIndex = -1;
                replanTimer[old] = 0.0f;
                it = tagIt;
            } else if ((replanTimer[it] -= dt) <= 0.0f || movers[it].pathIndex < 0) {   // (path ran out: at once)
                ChaseRun(it, tagTarget);
                replanTimer[it] = CHASE_REPLAN;
            }
        }
    }

    // everyone else: run if "it" is close (and get tired doing it)
    for (int i = 0; i < moverCount; i++) {
        if (!ChaseIsPlaying(i) || i == it) continue;
        if (fleeing[i]) { stamina[i] -= STAMINA_DRAIN * dt; if (stamina[i] < 0.0f) stamina[i] = 0.0f; }
        else { stamina[i] += STAMINA_REST * dt; if (stamina[i] > 1.0f) stamina[i] = 1.0f; }
        bool threat = ThreatClose(i, it) && tagFreeze < TAG_FREEZE - 0.3f;   // (a moment to react)
        if (!threat) { fleeing[i] = false; continue; }
        replanTimer[i] -= dt;
        bool closeIn = Dist(i, it) < FLEE_REPICK * CELL_SIZE;
        bool needSpot = !fleeing[i] || movers[i].pathIndex < 0 ||   // started running, or got there
                        (replanTimer[i] <= 0.0f && (!FleeSpotStillGood(i, it) || (closeIn && movers[i].pathIndex < 1)));
        if (needSpot) {
            FleeRun(i, it);
            replanTimer[i] = CHASE_REPLAN;
        }
        fleeing[i] = true;
    }
}

bool ChaseMoverTick(int i, float dt) {
    (void)dt;
    if (i != ChaseIt() || tagFreeze <= 0.0f) return false;
    Mover* m = &movers[i];   // counting to three
    m->pathLength = 0;
    m->pathIndex = -1;
    m->timeWithoutProgress = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}
