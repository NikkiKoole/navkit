// chase.c - chasing and fleeing, and the group games on top (see chase.h)
//
// The games are joint activities (activity.c): each running game is an activity with its own
// state (TagState, HideState, FollowState, PackState below), so several can run at once. The
// old calls (ChaseSetPlayer, ChaseSetIt, HideSeekStart, ...) still work: "set player" waits to
// play, and a game started next takes the ones waiting.
#include "chase.h"
#include "activity.h"
#include "notice.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "action_queue.h"
#include "../simulation/needs.h"
#include "../simulation/balance.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool fleeSmart = true;
float chaseSpeed = 1.25f;
float fleeSpeed = 1.2f;
float fleeRadius = 5.0f;
long tagCount = 0;
double tagChaseSecondsSum = 0.0;

// Per mover (a mover is in one game at a time)
static float wanderTimer[MAX_MOVERS];   // a calm player's pause between strolls
static float stamina[MAX_MOVERS];   // 0..1: running drains it, resting refills it
static bool fleeing[MAX_MOVERS];
static float replanTimer[MAX_MOVERS];

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
    ClearActivities();   // ends every game (and sets its players free)
    hideFoundCount = hideRounds = 0;
    hideFoundSecondsSum = 0.0;
    followLeaderChanges = 0;
    packCatches = 0; packCatchSecondsSum = 0.0;
    packEscapes = packDucks = packFlushed = 0; packDuckSecondsSum = 0.0;
    memset(fleeing, 0, sizeof(fleeing));
    memset(replanTimer, 0, sizeof(replanTimer));
    for (int i = 0; i < MAX_MOVERS; i++) stamina[i] = 1.0f;
    tagCount = 0;
    tagChaseSecondsSum = 0.0;
    memset(wanderTimer, 0, sizeof(wanderTimer));
}

// --- Playing is a free-time state (FREETIME_PLAY), the membership is activity.c's ---
void ChaseMoverReset(int i) { stamina[i] = 1.0f; fleeing[i] = false; wanderTimer[i] = 0.0f; }
void ChaseMoverLeft(int i) { fleeing[i] = false; }
bool ChaseJoin(int i) { return ActivityWait(i); }
void ChaseLeave(int i) { ActivityLeave(i); }
void ChaseSetPlayer(int i, bool on) { if (on) ChaseJoin(i); else ChaseLeave(i); }

// A need that would send it off (hungry, thirsty, tired, bladder, or anything critical): it
// leaves the game to see to it
bool PlayWantsToStop(int i) {
    const Mover* m = &movers[i];
    return (hungerEnabled && m->hunger < balance.hungerSeekThreshold) ||
           (thirstEnabled && m->thirst < balance.thirstSeekThreshold) ||
           (bladderEnabled && m->bladder < balance.bladderSeekThreshold) ||
           (energyEnabled && m->energy < balance.energyTiredThreshold) ||
           MoverHasCriticalNeed(i, false);
}
float ChaseStamina(int i) { return (i >= 0 && i < MAX_MOVERS) ? stamina[i] : 1.0f; }
bool ChaseIsPlaying(int i) { return MoverInActivity(i); }
bool ChaseIsFleeing(int i) { return ChaseIsPlaying(i) && fleeing[i]; }

// The game a member is in (state of that kind), NULL if it isn't in one of that kind
static void* GameOf(int a, int kind) {
    return (a >= 0 && a < MAX_ACTIVITIES && activities[a].active && activities[a].kind == kind) ? activities[a].state : NULL;
}
static bool ChaseMoverOk(int i) { return i >= 0 && i < moverCount && movers[i].active; }

float ChaseSpeedScale(int i) {
    if (!ChaseIsPlaying(i)) return 1.0f;
    int a = MoverActivity(i);
    if (a >= 0 && activities[a].active) {
        const ActivityDef* d = ActivityDefOf(activities[a].kind);
        float s = d->speedScale ? d->speedScale(a, i) : 0.0f;
        if (s > 0.0f) return s;
    }
    if (!fleeing[i]) return 1.0f;
    return stamina[i] > TIRED ? fleeSpeed : TIRED_SPEED;
}

void ChaseTick(float dt) { ActivityTick(dt); }
bool ChaseMoverTick(int i, float dt) { return ActivityMoverTick(i, dt); }

// Standing still this tick (counting, hidden, crouched): no path, no "stuck" clock
static void ChaseHoldStill(int i) {
    Mover* m = &movers[i];
    m->pathLength = 0;
    m->pathIndex = -1;
    m->timeWithoutProgress = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
}

// A game started with the movers waiting to play (and `with`, if it isn't waiting)
static int StartWithWaiting(int kind, int with) {
    int a = ActivityStart(kind);
    if (a < 0) return -1;
    for (int j = 0; j < moverCount; j++) if (ActivityWaiting(j)) ActivityAdd(a, j);
    if (with >= 0) ActivityAdd(a, with);
    return a;
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

// Distance (pixels) from point p to the segment a-b
static float SegDist(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
    float t = l2 > 0.0f ? ((px - ax) * vx + (py - ay) * vy) / l2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float qx = ax + vx * t - px, qy = ay + vy * t - py;
    return sqrtf(qx * qx + qy * qy);
}

// Flee from one or more threats: candidate spots round the "away" direction (away from all,
// the nearest weighing most), the best = far from the nearest threat, open round it, a straight
// run that doesn't pass close by a threat
static void FleeRunFrom(int i, const int* threats, int n) {
    const Mover* m = &movers[i];
    int z = (int)m->z;
    float ax = 0.0f, ay = 0.0f;
    for (int k = 0; k < n; k++) {
        float dx = m->x - movers[threats[k]].x, dy = m->y - movers[threats[k]].y;
        float d2 = dx * dx + dy * dy + 1.0f;
        ax += dx / d2; ay += dy / d2;
    }
    float al = sqrtf(ax * ax + ay * ay);
    if (al < 1e-6f) { ax = 1.0f; ay = 0.0f; al = 1.0f; }
    ax /= al; ay /= al;
    if (!fleeSmart) {   // straight away, as far as the map allows (how animals flee today)
        for (float d = FLEE_MAX; d >= 1.0f; d -= 1.0f)
            if (PathTo(i, (int)((m->x + ax * d * CELL_SIZE) / CELL_SIZE), (int)((m->y + ay * d * CELL_SIZE) / CELL_SIZE), z)) return;
        return;
    }
    int bestX = -1, bestY = -1;
    float bestScore = -1e9f;
    float base = atan2f(ay, ax);
    for (int c = 0; c < FLEE_CANDIDATES; c++) {
        float a = base + ChaseRandRange(-1.75f, 1.75f), d = ChaseRandRange(FLEE_MIN, FLEE_MAX);
        int cx = (int)((m->x + cosf(a) * d * CELL_SIZE) / CELL_SIZE), cy = (int)((m->y + sinf(a) * d * CELL_SIZE) / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || !IsCellWalkableAt(z, cy, cx)) continue;
        if (!HasLineOfSight((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), cx, cy, z)) continue;   // a straight run
        float px = (cx + 0.5f) * CELL_SIZE, py = (cy + 0.5f) * CELL_SIZE;
        float nearest = 1e9f, pass = 1e9f;
        for (int k = 0; k < n; k++) {
            const Mover* t = &movers[threats[k]];
            float td = sqrtf((px - t->x) * (px - t->x) + (py - t->y) * (py - t->y));
            if (td < nearest) nearest = td;
            float sd = SegDist(t->x, t->y, m->x, m->y, px, py);
            if (sd < pass) pass = sd;
        }
        int open = 0;   // room to run on from there (not a corner)
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                if ((ox || oy) && IsCellWalkableAt(z, cy + oy, cx + ox)) open++;
        float score = nearest / CELL_SIZE + open * 0.5f;
        if (pass < 1.5f * CELL_SIZE) score -= 10.0f;   // runs right past a hunter
        if (score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
    }
    if (bestX < 0 || !PathTo(i, bestX, bestY, z)) {   // nothing in the open: straight away
        for (float d = FLEE_MAX; d >= 1.0f; d -= 1.0f)
            if (PathTo(i, (int)((m->x + ax * d * CELL_SIZE) / CELL_SIZE), (int)((m->y + ay * d * CELL_SIZE) / CELL_SIZE), z)) return;
    }
}

static void FleeRun(int i, int threat) { FleeRunFrom(i, &threat, 1); }

// Is the threat close enough to run from (and has it been seen: noticed, or very close)?
static bool ThreatClose(int i, int threat) {
    if (threat < 0 || (int)movers[threat].z != (int)movers[i].z) return false;
    float d = Dist(i, threat);
    if (d > fleeRadius * CELL_SIZE) return false;
    const MoverNotice* n = &moverNotice[i];
    for (int k = 0; k < n->count; k++) if (n->e[k].mover == threat) return true;
    return d < 2.0f * CELL_SIZE;   // too close not to notice
}

// A calm player (tag: not running, "it" with nobody in sight; the pack's runner, at ease)
// strolls about near the others in its game, with a pause now and then
static bool TagCalm(int a, int i);
static bool PackCalm(int a, int i);
static void PlayWander(int a, float dt) {
    float cx = 0.0f, cy = 0.0f;
    int n = 0;
    for (int i = 0; i < moverCount; i++)
        if (ActivityHas(a, i)) { cx += movers[i].x; cy += movers[i].y; n++; }
    if (n == 0) return;
    cx /= n; cy /= n;
    bool tag = activities[a].kind == ACT_TAG;
    for (int i = 0; i < moverCount; i++) {
        if (!ActivityHas(a, i) || fleeing[i]) continue;
        bool calm = tag ? TagCalm(a, i) : PackCalm(a, i);
        if (!calm || movers[i].pathIndex >= 0) continue;
        if ((wanderTimer[i] -= dt) > 0.0f) continue;
        wanderTimer[i] = ChaseRandRange(0.5f, 2.0f);
        Mover* m = &movers[i];
        int z = (int)m->z;
        for (int tries = 0; tries < 6; tries++) {   // somewhere 3-7 cells off, toward the others if far from them
            float an = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(3.0f, 7.0f);
            float tx = m->x + cosf(an) * d * CELL_SIZE, ty = m->y + sinf(an) * d * CELL_SIZE;
            float fx = tx - cx, fy = ty - cy;
            if (fx * fx + fy * fy > (10.0f * CELL_SIZE) * (10.0f * CELL_SIZE)) { tx = (tx + cx) * 0.5f; ty = (ty + cy) * 0.5f; }
            if (PathTo(i, (int)(tx / CELL_SIZE), (int)(ty / CELL_SIZE), z)) break;
        }
    }
}

// ============================================================================
// Tag
// ============================================================================
typedef struct {
    int it, target;
    float targetHeld;   // seconds on the current target
    float freeze;       // the new "it" counting to three
    float chaseTime;    // since the last tag
} TagState;

static TagState* TagOf(int a) { return GameOf(a, ACT_TAG); }
static int TagItOf(const TagState* t) { return t && ChaseMoverOk(t->it) ? t->it : -1; }
static void TagSetIt(TagState* t, int i) { t->it = i; t->target = -1; t->freeze = TAG_FREEZE; t->chaseTime = 0.0f; }

int ChaseIt(void) {
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        int it = TagItOf(TagOf(a));
        if (it >= 0) return it;
    }
    return -1;
}

// "it" in i's game of tag, or a new game of tag with the ones waiting to play
void ChaseSetIt(int i) {
    int a = MoverActivity(i);
    if (!TagOf(a)) a = StartWithWaiting(ACT_TAG, i);
    TagState* t = TagOf(a);
    if (t) TagSetIt(t, i);
}

static bool TagCalm(int a, int i) {
    const TagState* t = TagOf(a);
    return i != t->it || (t->target < 0 && t->freeze <= 0.0f);
}

static void TagTick(int a, float dt) {
    TagState* t = TagOf(a);
    int it = TagItOf(t);
    if (it < 0) return;
    PlayWander(a, dt);
    t->chaseTime += dt;
    if (t->freeze > 0.0f) t->freeze -= dt;

    // "it": pick (or keep) a target and run at it
    if (t->freeze <= 0.0f) {
        t->targetHeld += dt;
        int best = -1;
        float bestD = 1e9f;
        const MoverNotice* n = &moverNotice[it];
        for (int k = 0; k < n->count; k++) {
            int j = n->e[k].mover;
            if (!ActivityHas(a, j) || j == it || (int)movers[j].z != (int)movers[it].z) continue;
            float d = Dist(it, j);
            if (d < bestD) { bestD = d; best = j; }
        }
        bool keep = t->target >= 0 && ActivityHas(a, t->target) &&
                    (t->targetHeld < TARGET_STICK || best < 0 || Dist(it, t->target) < bestD * 1.5f);
        if (!keep && best >= 0) { t->target = best; t->targetHeld = 0.0f; }
        if (!keep && best < 0) t->target = -1;   // nobody in sight: wander (PlayWander)
        if (t->target >= 0) {
            if (Dist(it, t->target) < CATCH_DIST * CELL_SIZE) {   // tagged!
                tagCount++;
                tagChaseSecondsSum += t->chaseTime;
                int old = it;
                TagSetIt(t, t->target);
                fleeing[t->it] = false;
                movers[t->it].pathLength = 0;
                movers[t->it].pathIndex = -1;
                replanTimer[old] = 0.0f;
                it = t->it;
            } else if ((replanTimer[it] -= dt) <= 0.0f || movers[it].pathIndex < 0) {   // (path ran out: at once)
                ChaseRun(it, t->target);
                replanTimer[it] = CHASE_REPLAN;
            }
        }
    }

    // everyone else: run if "it" is close (and get tired doing it)
    for (int i = 0; i < moverCount; i++) {
        if (!ActivityHas(a, i) || i == it) continue;
        if (fleeing[i]) { stamina[i] -= STAMINA_DRAIN * dt; if (stamina[i] < 0.0f) stamina[i] = 0.0f; }
        else { stamina[i] += STAMINA_REST * dt; if (stamina[i] > 1.0f) stamina[i] = 1.0f; }
        bool threat = ThreatClose(i, it) && t->freeze < TAG_FREEZE - 0.3f;   // (a moment to react)
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

// The new "it" counts to three, standing still
static bool TagMoverTick(int a, int i, float dt) {
    (void)dt;
    const TagState* t = TagOf(a);
    if (i != TagItOf(t) || t->freeze <= 0.0f) return false;
    ChaseHoldStill(i);
    return true;
}

static float TagSpeedScale(int a, int i) { return i == TagOf(a)->it ? chaseSpeed : 0.0f; }

// Someone left: if it was "it", the nearest one still playing is "it" now
static void TagMemberLeft(int a, int i) {
    TagState* t = TagOf(a);
    if (t->target == i) t->target = -1;
    if (t->it != i) return;
    int next = -1;
    float best = 1e18f;
    for (int j = 0; j < moverCount; j++) {
        if (!ActivityHas(a, j)) continue;
        float d = Dist(i, j);
        if (d < best) { best = d; next = j; }
    }
    if (next >= 0) TagSetIt(t, next); else t->it = -1;
}

static bool TagTooSmall(int a) { return activities[a].count < 2 || TagItOf(TagOf(a)) < 0; }

// Proposed: the one who proposed it is "it"
static void TagBegin(int a, const int* players, int n) {
    for (int k = 0; k < n; k++) ActivityAdd(a, players[k]);
    if (ActivityHas(a, players[0])) TagSetIt(TagOf(a), players[0]);
}

const ActivityDef tagActivityDef = {
    "tag", "Playing tag", sizeof(TagState),
    TagTick, TagMoverTick, TagSpeedScale, TagMemberLeft, TagTooSmall, NULL,
    2, 8, TagBegin,
};

// ============================================================================
// Hide and seek
// ============================================================================
bool hideSmart = true;
long hideFoundCount = 0, hideRounds = 0;
double hideFoundSecondsSum = 0.0;

enum { HS_NONE, HS_COUNT, HS_SEEK };
enum { HIDER_GOING, HIDER_HIDDEN, HIDER_FOUND };
typedef struct {
    int phase, seeker, firstFound;
    float timer, lookTimer;
    Point home;
    unsigned char* seen;   // per cell: the seeker has looked here this round
    int seenW, seenH;
} HideState;
static int hiderState[MAX_MOVERS];   // per mover (in its game)

#define HS_COUNT_TIME   10.0f   // seconds the seeker counts
#define HS_ROUND_TIME   90.0f   // seconds of seeking before the round ends anyway
#define HS_MIN          4.0f    // cells: hiding spots between these from where the hider stands
#define HS_MAX          14.0f
#define HS_CANDIDATES   40
#define HS_LOOK_RADIUS  6       // cells the seeker marks as looked at (in sight)

static HideState* HideOf(int a) { return GameOf(a, ACT_HIDE_SEEK); }
static HideState* HideOfMover(int i) { return HideOf(MoverActivity(i)); }

int HideSeeker(void) {
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        const HideState* hs = HideOf(a);
        if (hs && hs->phase != HS_NONE && hs->seeker >= 0 && hs->seeker < moverCount) return hs->seeker;
    }
    return -1;
}
bool HideIsCounting(int i) { const HideState* hs = HideOfMover(i); return hs && hs->phase == HS_COUNT && i == hs->seeker; }
bool HideIsHidden(int i) {
    const HideState* hs = HideOfMover(i);
    return hs && hs->phase != HS_NONE && ChaseIsPlaying(i) && i != hs->seeker && hiderState[i] == HIDER_HIDDEN;
}
bool HideIsFound(int i) {
    const HideState* hs = HideOfMover(i);
    return hs && hs->phase != HS_NONE && ChaseIsPlaying(i) && i != hs->seeker && hiderState[i] == HIDER_FOUND;
}
// Every player's goals come from its game (a calm one wanders round the others: PlayWander)
bool HideControls(int i) { return ChaseIsPlaying(i); }
bool MoverCrouching(int i) { return HideIsHidden(i) || PackIsDucking(i); }

static bool SeenCell(const HideState* hs, int x, int y) {
    return hs->seen && x >= 0 && y >= 0 && x < hs->seenW && y < hs->seenH && hs->seen[y * hs->seenW + x];
}

// A hiding spot for i: hideSmart = out of sight from home, tucked in, away from other hiders
static bool PickHidingSpot(int a, int i) {
    const HideState* hs = HideOf(a);
    const Mover* m = &movers[i];
    int z = (int)m->z, bestX = -1, bestY = -1;
    float bestScore = -1e9f;
    for (int c = 0; c < HS_CANDIDATES; c++) {
        float an = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(HS_MIN, HS_MAX);
        int cx = (int)((m->x + cosf(an) * d * CELL_SIZE) / CELL_SIZE), cy = (int)((m->y + sinf(an) * d * CELL_SIZE) / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || !IsCellWalkableAt(z, cy, cx)) continue;
        float score = 0.0f;
        if (hideSmart) {
            bool seen = CanSeeCell(hs->home.x, hs->home.y, cx, cy, z);   // from home (crouched behind cover: not)
            if (seen && coverEnabled && CoverBetween(hs->home.x, hs->home.y, cx, cy, z)) seen = false;
            if (seen) score -= 20.0f;
            int walls = 0, cover = 0;   // tucked in
            for (int oy = -1; oy <= 1; oy++)
                for (int ox = -1; ox <= 1; ox++) {
                    if (!(ox || oy)) continue;
                    if (!IsCellWalkableAt(z, cy + oy, cx + ox)) walls++;
                    if (coverEnabled && IsLowCover(cx + ox, cy + oy, z)) cover++;
                }
            if (coverEnabled && GRID(cx, cy, z) == CELL_BUSH) cover += 4;   // in a bush
            score += walls * 1.5f + cover * 1.5f;
            for (int j = 0; j < moverCount; j++) {   // away from the other hiders' spots
                if (j == i || !ActivityHas(a, j) || j == hs->seeker) continue;
                float gx = (float)(movers[j].goal.x - cx), gy = (float)(movers[j].goal.y - cy);
                if (gx * gx + gy * gy < 9.0f) score -= 4.0f;
            }
            score += ChaseRandRange(0.0f, 1.0f);
        } else {
            score = ChaseRandRange(0.0f, 1.0f);   // any spot in range
        }
        if (score > bestScore) {
            int ox = (int)(m->x / CELL_SIZE), oy = (int)(m->y / CELL_SIZE);
            Point start = {ox, oy, z}, goal = {cx, cy, z};
            Mover* mm = &movers[i];
            mm->goal = goal;
            if (MoverPlanPath(i, start, goal) <= 0) continue;   // (keeps the best reachable path planned)
            mm->needsRepath = false;
            bestScore = score; bestX = cx; bestY = cy;
        }
    }
    if (bestX < 0) return false;
    return PathTo(i, bestX, bestY, z);   // the best one (the last planned may be a worse one)
}

// A round: the seeker counts at home, the others run off to hide
static void HideRound(int a, int seeker) {
    HideState* hs = HideOf(a);
    hs->seeker = seeker;
    hs->phase = HS_COUNT;
    hs->timer = HS_COUNT_TIME;
    hs->firstFound = -1;
    hs->home = (Point){(int)(movers[seeker].x / CELL_SIZE), (int)(movers[seeker].y / CELL_SIZE), (int)movers[seeker].z};
    if (hs->seenW != gridWidth || hs->seenH != gridHeight) {
        free(hs->seen);
        hs->seenW = gridWidth; hs->seenH = gridHeight;
        hs->seen = calloc((size_t)hs->seenW * hs->seenH, 1);
    } else if (hs->seen) memset(hs->seen, 0, (size_t)hs->seenW * hs->seenH);
    hideRounds++;
    movers[seeker].pathLength = 0;
    movers[seeker].pathIndex = -1;
    for (int i = 0; i < moverCount; i++) {
        if (!ActivityHas(a, i) || i == seeker) continue;
        hiderState[i] = HIDER_GOING;
        fleeing[i] = true;   // runs to its spot (fleeSpeed, ChaseSpeedScale)
        PickHidingSpot(a, i);
    }
}

// A new round in the seeker's game, or a new game with the ones waiting to play
void HideSeekStart(int seeker) {
    int a = MoverActivity(seeker);
    if (!HideOf(a)) a = StartWithWaiting(ACT_HIDE_SEEK, seeker);
    if (HideOf(a)) HideRound(a, seeker);
}

// The seeker marks what it can see as looked at, then goes where it hasn't looked
static void SeekerLook(HideState* hs, int s) {
    const Mover* m = &movers[s];
    int sx = (int)(m->x / CELL_SIZE), sy = (int)(m->y / CELL_SIZE), z = (int)m->z;
    for (int dy = -HS_LOOK_RADIUS; dy <= HS_LOOK_RADIUS; dy++)
        for (int dx = -HS_LOOK_RADIUS; dx <= HS_LOOK_RADIUS; dx++) {
            int x = sx + dx, y = sy + dy;
            if (x < 0 || y < 0 || x >= hs->seenW || y >= hs->seenH || dx * dx + dy * dy > HS_LOOK_RADIUS * HS_LOOK_RADIUS) continue;
            if (hs->seen[y * hs->seenW + x] || !IsCellWalkableAt(z, y, x)) continue;
            if (!CanSeeCell(sx, sy, x, y, z)) continue;
            // behind low cover from here: someone could be crouching there, it has to go and look
            if (coverEnabled && dx * dx + dy * dy > 4 && CoverBetween(sx, sy, x, y, z)) continue;
            hs->seen[y * hs->seenW + x] = 1;
        }
}

static void SeekerGo(const HideState* hs, int s) {
    const Mover* m = &movers[s];
    int z = (int)m->z, bestX = -1, bestY = -1;
    float bestScore = -1e9f;
    for (int c = 0; c < 30; c++) {   // an unlooked-at cell, not too far
        float an = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(3.0f, 12.0f);
        int cx = (int)((m->x + cosf(an) * d * CELL_SIZE) / CELL_SIZE), cy = (int)((m->y + sinf(an) * d * CELL_SIZE) / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || !IsCellWalkableAt(z, cy, cx)) continue;
        int unseen = 0;   // how much unlooked-at ground round it
        for (int oy = -2; oy <= 2; oy++)
            for (int ox = -2; ox <= 2; ox++)
                if (IsCellWalkableAt(z, cy + oy, cx + ox) && !SeenCell(hs, cx + ox, cy + oy)) unseen++;
        float score = unseen - d * 0.3f + ChaseRandRange(0.0f, 1.0f);
        if (score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
    }
    if (bestX >= 0) PathTo(s, bestX, bestY, z);
}

static void HideTick(int a, float dt) {
    HideState* hs = HideOf(a);
    if (hs->phase == HS_NONE || hs->seeker < 0 || !movers[hs->seeker].active) return;
    int s = hs->seeker;
    hs->timer -= dt;
    // hiders: arrived at the spot -> hidden; found ones go home
    for (int i = 0; i < moverCount; i++) {
        if (!ActivityHas(a, i) || i == s) continue;
        if (hiderState[i] == HIDER_GOING && movers[i].pathIndex < 0) {
            hiderState[i] = HIDER_HIDDEN;
            fleeing[i] = false;
        }
    }
    if (hs->phase == HS_COUNT) {
        if (hs->timer <= 0.0f) { hs->phase = HS_SEEK; hs->timer = HS_ROUND_TIME; hs->lookTimer = 0.0f; }
        return;
    }
    // seeking: look, spot, go on
    if ((hs->lookTimer -= dt) <= 0.0f) { SeekerLook(hs, s); hs->lookTimer = 0.5f; }
    const MoverNotice* n = &moverNotice[s];
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (!ActivityHas(a, j) || j == s || hiderState[j] == HIDER_FOUND) continue;
        hiderState[j] = HIDER_FOUND;   // spotted!
        fleeing[j] = false;
        hideFoundCount++;
        hideFoundSecondsSum += HS_ROUND_TIME - hs->timer;
        if (hs->firstFound < 0) hs->firstFound = j;
        PathTo(j, hs->home.x, hs->home.y, hs->home.z);   // walk home
    }
    if (movers[s].pathIndex < 0) SeekerGo(hs, s);
    bool anyLeft = false;
    for (int i = 0; i < moverCount; i++)
        if (ActivityHas(a, i) && i != s && hiderState[i] != HIDER_FOUND) anyLeft = true;
    if (!anyLeft || hs->timer <= 0.0f) {   // next round: the first one found seeks
        int next = hs->firstFound >= 0 ? hs->firstFound : s;
        PathTo(s, hs->home.x, hs->home.y, hs->home.z);
        HideRound(a, next);
    }
}

// Standing still: the seeker counting, hiders in their spot, found ones waiting at home
static bool HideMoverTick(int a, int i, float dt) {
    (void)dt;
    const HideState* hs = HideOf(a);
    if (hs->phase == HS_NONE) return false;
    bool stand = (i == hs->seeker && hs->phase == HS_COUNT) || (i != hs->seeker && hiderState[i] == HIDER_HIDDEN) ||
                 (i != hs->seeker && hiderState[i] == HIDER_FOUND && movers[i].pathIndex < 0);
    if (!stand) return false;
    ChaseHoldStill(i);
    return true;
}

// The seeker left: a new round with someone else seeking
static void HideMemberLeft(int a, int i) {
    HideState* hs = HideOf(a);
    if (hs->phase == HS_NONE || i != hs->seeker) return;
    int next = -1;
    for (int j = 0; j < moverCount && next < 0; j++) if (ActivityHas(a, j)) next = j;
    if (next >= 0) HideRound(a, next); else hs->phase = HS_NONE;
}

static bool HideTooSmall(int a) { return activities[a].count < 2 || HideOf(a)->phase == HS_NONE; }
static void HideEnd(int a) { HideState* hs = HideOf(a); free(hs->seen); hs->seen = NULL; }

// Proposed: the one who proposed it seeks first
static void HideBegin(int a, const int* players, int n) {
    for (int k = 0; k < n; k++) ActivityAdd(a, players[k]);
    if (ActivityHas(a, players[0])) HideRound(a, players[0]);
}

const ActivityDef hideSeekActivityDef = {
    "hide-and-seek", "Playing hide and seek", sizeof(HideState),
    HideTick, HideMoverTick, NULL, HideMemberLeft, HideTooSmall, HideEnd,
    3, 8, HideBegin,
};

// ============================================================================
// Follow the leader
// ============================================================================
bool followTrail = true;
long followLeaderChanges = 0;

#define FL_TRAIL         96      // trail points kept per player
#define FL_TRAIL_STEP    4.0f    // pixels between trail points
#define FL_GAP           1.0f    // cells along the trail between one and the next
#define FL_LEAD_PACE     0.8f
#define FL_FOLLOW_PACE   1.2f    // a follower's top speed (catching up)
#define FL_TURN_TIME     60.0f   // seconds before the leader goes to the back
#define FL_ROUTE         512     // the leader's route for the stats: a longer trail of the leader only

typedef struct {
    int order[FOLLOW_MAX], count;      // movers, order[0] leads
    float trailX[FOLLOW_MAX][FL_TRAIL], trailY[FOLLOW_MAX][FL_TRAIL];
    int trailHead[FOLLOW_MAX], trailLen[FOLLOW_MAX];
    float turn;
    float routeX[FL_ROUTE], routeY[FL_ROUTE];
    int routeHead, routeLen;
} FollowState;

static FollowState* FollowOf(int a) { return GameOf(a, ACT_FOLLOW_LEADER); }

static int FollowSlot(const FollowState* fl, int i) {   // index in order, -1
    if (!fl) return -1;
    for (int k = 0; k < fl->count; k++) if (fl->order[k] == i) return k;
    return -1;
}
int FollowPlace(int i) { return FollowSlot(FollowOf(MoverActivity(i)), i); }
int FollowLeader(void) {
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        const FollowState* fl = FollowOf(a);
        if (fl && fl->count > 0) return fl->order[0];
    }
    return -1;
}

static float FollowSpeedScale(int a, int i) {
    return FollowSlot(FollowOf(a), i) == 0 ? FL_LEAD_PACE : 0.0f;   // (followers move themselves: FollowMoverTick)
}

// The trail of slot k: newest first, index n back
static bool TrailPoint(const FollowState* fl, int k, int n, float* x, float* y) {
    if (n >= fl->trailLen[k]) return false;
    int idx = (fl->trailHead[k] - n + FL_TRAIL) % FL_TRAIL;
    *x = fl->trailX[k][idx]; *y = fl->trailY[k][idx];
    return true;
}

static void TrailRecord(FollowState* fl, int k, float x, float y) {
    float lx, ly;
    if (TrailPoint(fl, k, 0, &lx, &ly) && (lx - x) * (lx - x) + (ly - y) * (ly - y) < FL_TRAIL_STEP * FL_TRAIL_STEP) return;
    fl->trailHead[k] = (fl->trailHead[k] + 1) % FL_TRAIL;
    fl->trailX[k][fl->trailHead[k]] = x; fl->trailY[k][fl->trailHead[k]] = y;
    if (fl->trailLen[k] < FL_TRAIL) fl->trailLen[k]++;
}

static void FollowBegin(int a, const int* players, int n) {
    FollowState* fl = FollowOf(a);
    if (n > FOLLOW_MAX) n = FOLLOW_MAX;
    for (int k = 0; k < n; k++) {
        if (!ActivityAdd(a, players[k])) continue;   // (not free to play)
        int s = fl->count++;
        fl->order[s] = players[k];
        fl->trailLen[s] = 0; fl->trailHead[s] = 0;
        TrailRecord(fl, s, movers[players[k]].x, movers[players[k]].y);
        movers[players[k]].pathLength = 0;
        movers[players[k]].pathIndex = -1;
    }
    fl->turn = FL_TURN_TIME;
    fl->routeLen = 0;
}

void FollowLeaderStart(const int* players, int n) {
    int a = ActivityStart(ACT_FOLLOW_LEADER);
    if (a >= 0) FollowBegin(a, players, n);   // (the first one leads)
}

static void LeaderNextLeg(int leader) {
    const Mover* m = &movers[leader];
    for (int tries = 0; tries < 12; tries++) {
        float an = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(5.0f, 10.0f);
        int gx = (int)((m->x + cosf(an) * d * CELL_SIZE) / CELL_SIZE), gy = (int)((m->y + sinf(an) * d * CELL_SIZE) / CELL_SIZE);
        if (PathTo(leader, gx, gy, (int)m->z)) return;
    }
}

static void FollowTick(int a, float dt) {
    FollowState* fl = FollowOf(a);
    if (fl->count < 2) return;
    // trails (each slot's trail is about the mover in it) and the leader's longer route
    for (int k = 0; k < fl->count; k++) TrailRecord(fl, k, movers[fl->order[k]].x, movers[fl->order[k]].y);
    {
        const Mover* L = &movers[fl->order[0]];
        int last = (fl->routeHead + FL_ROUTE - 1) % FL_ROUTE;
        if (fl->routeLen == 0 || (fl->routeX[last] - L->x) * (fl->routeX[last] - L->x) + (fl->routeY[last] - L->y) * (fl->routeY[last] - L->y) >= FL_TRAIL_STEP * FL_TRAIL_STEP) {
            fl->routeX[fl->routeHead] = L->x; fl->routeY[fl->routeHead] = L->y;
            fl->routeHead = (fl->routeHead + 1) % FL_ROUTE;
            if (fl->routeLen < FL_ROUTE) fl->routeLen++;
        }
    }
    if (movers[fl->order[0]].pathIndex < 0) LeaderNextLeg(fl->order[0]);
    if ((fl->turn -= dt) <= 0.0f) {   // the leader goes to the back; trails move with their movers
        int lead = fl->order[0];
        float tx[FL_TRAIL], ty[FL_TRAIL];
        int th = fl->trailHead[0], tl = fl->trailLen[0];
        memcpy(tx, fl->trailX[0], sizeof(tx)); memcpy(ty, fl->trailY[0], sizeof(ty));
        for (int k = 0; k + 1 < fl->count; k++) {
            fl->order[k] = fl->order[k + 1];
            memcpy(fl->trailX[k], fl->trailX[k + 1], sizeof(tx)); memcpy(fl->trailY[k], fl->trailY[k + 1], sizeof(ty));
            fl->trailHead[k] = fl->trailHead[k + 1]; fl->trailLen[k] = fl->trailLen[k + 1];
        }
        fl->order[fl->count - 1] = lead;
        memcpy(fl->trailX[fl->count - 1], tx, sizeof(tx)); memcpy(fl->trailY[fl->count - 1], ty, sizeof(ty));
        fl->trailHead[fl->count - 1] = th; fl->trailLen[fl->count - 1] = tl;
        movers[lead].pathLength = 0; movers[lead].pathIndex = -1;
        fl->turn = FL_TURN_TIME;   // (the route goes on: the new leader was walking it)
        followLeaderChanges++;
    }
}

// Where follower slot k should head: a point about FL_GAP back along the trail of the one ahead
// (followTrail), or straight at the one ahead
static bool FollowTarget(const FollowState* fl, int k, float* x, float* y) {
    int ahead = fl->order[k - 1];
    if (!followTrail) {
        float dx = movers[fl->order[k]].x - movers[ahead].x, dy = movers[fl->order[k]].y - movers[ahead].y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < 0.5f) return false;
        *x = movers[ahead].x + dx / d * FL_GAP * CELL_SIZE;
        *y = movers[ahead].y + dy / d * FL_GAP * CELL_SIZE;
        return true;
    }
    float along = 0.0f, px, py;
    if (!TrailPoint(fl, k - 1, 0, &px, &py)) return false;
    for (int n = 1; ; n++) {   // walk back along the trail until FL_GAP cells of it
        float qx, qy;
        if (!TrailPoint(fl, k - 1, n, &qx, &qy)) break;
        along += sqrtf((qx - px) * (qx - px) + (qy - py) * (qy - py));
        px = qx; py = qy;
        if (along >= FL_GAP * CELL_SIZE) break;
    }
    // the follower's own position: if it's already past that point (close to the one ahead), wait
    *x = px; *y = py;
    return true;
}

static bool FollowMoverTick(int a, int i, float dt) {
    const FollowState* fl = FollowOf(a);
    int k = FollowSlot(fl, i);
    if (k <= 0) return false;   // the leader walks its path
    Mover* m = &movers[i];
    float tx, ty;
    if (FollowTarget(fl, k, &tx, &ty)) {
        float dx = tx - m->x, dy = ty - m->y, d = sqrtf(dx * dx + dy * dy);
        float ax = movers[fl->order[k - 1]].x - m->x, ay = movers[fl->order[k - 1]].y - m->y;
        bool tooClose = ax * ax + ay * ay < (FL_GAP * 0.8f * CELL_SIZE) * (FL_GAP * 0.8f * CELL_SIZE);
        if (d > 1.0f && !tooClose) {
            float pace = d > CELL_SIZE * 0.5f ? FL_FOLLOW_PACE : FL_LEAD_PACE;
            float step = m->speed * (60.0f / dayLength) * pace * dt;
            if (step > d) step = d;
            float nx = m->x + dx / d * step, ny = m->y + dy / d * step;
            int z = (int)m->z;
            if (IsCellWalkableAt(z, (int)(ny / CELL_SIZE), (int)(nx / CELL_SIZE))) { m->x = nx; m->y = ny; }
            else if (IsCellWalkableAt(z, (int)(m->y / CELL_SIZE), (int)(nx / CELL_SIZE))) m->x = nx;
            else if (IsCellWalkableAt(z, (int)(ny / CELL_SIZE), (int)(m->x / CELL_SIZE))) m->y = ny;
        }
    }
    ChaseHoldStill(i);
    return true;
}

bool FollowTrailDistance(int i, float* cells) {
    const FollowState* fl = FollowOf(MoverActivity(i));
    int k = FollowSlot(fl, i);
    if (k <= 0 || fl->routeLen < 8) return false;
    float best = 1e18f;
    for (int n = 0; n < fl->routeLen; n++) {
        float dx = fl->routeX[n] - movers[i].x, dy = fl->routeY[n] - movers[i].y;
        if (dx * dx + dy * dy < best) best = dx * dx + dy * dy;
    }
    *cells = sqrtf(best) / CELL_SIZE;
    return true;
}

// Out of the line (the next one leads if it was the leader)
static void FollowMemberLeft(int a, int i) {
    FollowState* fl = FollowOf(a);
    int k = FollowSlot(fl, i);
    if (k < 0) return;
    for (int q = k; q < fl->count - 1; q++) {
        fl->order[q] = fl->order[q + 1];
        memcpy(fl->trailX[q], fl->trailX[q + 1], sizeof(fl->trailX[q]));
        memcpy(fl->trailY[q], fl->trailY[q + 1], sizeof(fl->trailY[q]));
        fl->trailHead[q] = fl->trailHead[q + 1];
        fl->trailLen[q] = fl->trailLen[q + 1];
    }
    fl->count--;
    if (k == 0) fl->turn = FL_TURN_TIME;
}

static bool FollowTooSmall(int a) { return FollowOf(a)->count < 2; }

const ActivityDef followLeaderActivityDef = {
    "follow-the-leader", "Playing follow the leader", sizeof(FollowState),
    FollowTick, FollowMoverTick, FollowSpeedScale, FollowMemberLeft, FollowTooSmall, NULL,
    3, 6, FollowBegin,
};

// ============================================================================
// Pack hunt
// ============================================================================
bool packTactics = true;
float packSpeed = 1.1f;
float packRunnerSpeed = 1.4f;   // (faster than the hunters: they need their roles. 1.4: catch 15.5 s vs 19.3 s all-straight)
long packCatches = 0;
double packCatchSecondsSum = 0.0;

#define PACK_MAX       8
#define PACK_CLOSE     1.5f    // cells: everyone just goes straight at it
#define PACK_AHEAD     3.0f    // cells: flank points ahead of the runner ...
#define PACK_SIDE      2.5f    // ... and to the side
#define PACK_CUT_AHEAD 5.0f    // cells: where it's going, when it has no goal yet
#define PACK_RESPAWN   10.0f   // cells from every hunter

#define PACK_SIGHT     12.0f   // cells: how far a hunter sees the runner (in the open)
#define PACK_HEADED    2.0f    // seconds lost: first to where it was heading, then search
#define PACK_GIVE_UP   30.0f   // seconds lost: it got away (a new round)
#define PACK_FLUSH     2.5f    // cells: a hunter this close flushes a ducked runner out
#define DUCK_RANGE     4.0f    // cells: how far the runner looks for cover to duck behind (further: seen getting there)
#define GLIMPSE        0.4f    // seconds: out of sight this long before it ducks; seen this long and it's spotted

bool packStealth = true;
long packEscapes = 0, packDucks = 0, packFlushed = 0;
double packDuckSecondsSum = 0.0;

typedef struct {
    int hunters[PACK_MAX], n, target;
    int role[PACK_MAX];
    float replan[PACK_MAX], time, targetReplan;
    float lastX, lastY, lastHX, lastHY, lost;   // where the pack last saw it
    bool ducked;          // the runner is going to / crouching in a spot behind cover
    float seenFor;        // seconds the pack has had it in sight without a break
    float duckTry;        // a look round for cover twice a second
} PackState;

static PackState* PackOf(int a) { return GameOf(a, ACT_PACK_HUNT); }
static PackState* PackOfMover(int i) { return PackOf(MoverActivity(i)); }
static int PkTarget(const PackState* pk) { return pk && ChaseMoverOk(pk->target) ? pk->target : -1; }

int PackTarget(void) {
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        int t = PkTarget(PackOf(a));
        if (t >= 0) return t;
    }
    return -1;
}

static int PackSlot(const PackState* pk, int i) {
    for (int k = 0; k < pk->n; k++) if (pk->hunters[k] == i) return k;
    return -1;
}
int PackRole(int i) {
    const PackState* pk = PackOfMover(i);
    if (!pk) return -1;
    int k = PackSlot(pk, i);
    return k >= 0 && PkTarget(pk) >= 0 ? pk->role[k] : -1;
}
bool PackIsDucking(int i) {
    const PackState* pk = PackOfMover(i);
    return PkTarget(pk) >= 0 && i == pk->target && pk->ducked && movers[i].pathIndex < 0;
}
bool PackLost(float* seconds) {
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        const PackState* pk = PackOf(a);
        if (PkTarget(pk) < 0) continue;
        if (pk->lost <= 0.0f) return false;
        if (seconds) *seconds = pk->lost;
        return true;
    }
    return false;
}

static float PackSpeedScale(int a, int i) {
    const PackState* pk = PackOf(a);
    if (PkTarget(pk) < 0) return 0.0f;
    if (PackSlot(pk, i) >= 0) return packSpeed;
    if (i == pk->target) return fleeing[i] ? (stamina[i] > TIRED ? packRunnerSpeed : TIRED_SPEED) : 1.0f;
    return 0.0f;
}

void PackHuntStart(const int* hunters, int n, int target) {
    int a = ActivityStart(ACT_PACK_HUNT);
    if (a < 0) return;
    PackState* pk = PackOf(a);
    pk->target = -1;
    pk->n = n > PACK_MAX ? PACK_MAX : n;
    for (int k = 0; k < pk->n; k++) {
        pk->hunters[k] = hunters[k];
        pk->role[k] = 0;
        pk->replan[k] = 0.1f * k;   // staggered
        ActivityAdd(a, hunters[k]);
    }
    pk->target = target;
    ActivityAdd(a, target);
    stamina[target] = 1.0f;
    fleeing[target] = false;
    pk->time = 0.0f;
    pk->ducked = false;
    pk->lost = 0.0f;
    pk->lastX = movers[target].x; pk->lastY = movers[target].y;
    pk->lastHX = pk->lastHY = 0.0f;
}

// A walkable cell near (x, y) (pixels), searching outwards a little; false if none
static bool NearWalkable(float x, float y, int z, int* ox, int* oy) {
    int cx = (int)(x / CELL_SIZE), cy = (int)(y / CELL_SIZE);
    for (int r = 0; r <= 2; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int x2 = cx + dx, y2 = cy + dy;
                if (x2 < 0 || y2 < 0 || x2 >= gridWidth || y2 >= gridHeight || !IsCellWalkableAt(z, y2, x2)) continue;
                *ox = x2; *oy = y2;
                return true;
            }
    return false;
}

// Roles: the nearest drives; the one nearest to where the runner is going cuts it off; the rest
// flank (each on its own side of the runner's escape line)
static void PackAssignRoles(PackState* pk, float gx, float gy) {
    const Mover* t = &movers[pk->target];
    int driver = -1, cutter = -1;
    float best = 1e18f;
    for (int k = 0; k < pk->n; k++) {
        const Mover* h = &movers[pk->hunters[k]];
        float d = (h->x - t->x) * (h->x - t->x) + (h->y - t->y) * (h->y - t->y);
        if (d < best) { best = d; driver = k; }
    }
    best = 1e18f;
    for (int k = 0; k < pk->n; k++) {
        if (k == driver) continue;
        const Mover* h = &movers[pk->hunters[k]];
        float d = (h->x - gx) * (h->x - gx) + (h->y - gy) * (h->y - gy);
        if (d < best) { best = d; cutter = k; }
    }
    for (int k = 0; k < pk->n; k++) pk->role[k] = k == driver ? 0 : (k == cutter ? 1 : 2);
}

// Caught or got away: the runner turns up again far from the pack (a new round)
static void PackRespawn(PackState* pk, int tgt) {
    Mover* T = &movers[tgt];
    pk->time = 0.0f;
    for (int tries = 0; tries < 60; tries++) {
        int x = GetRandomValue(1, gridWidth - 2), y = GetRandomValue(1, gridHeight - 2), z = (int)T->z;
        if (!IsCellWalkableAt(z, y, x)) continue;
        bool far = true;
        for (int q = 0; q < pk->n; q++) {
            float dx = (x + 0.5f) * CELL_SIZE - movers[pk->hunters[q]].x, dy = (y + 0.5f) * CELL_SIZE - movers[pk->hunters[q]].y;
            if (dx * dx + dy * dy < (PACK_RESPAWN * CELL_SIZE) * (PACK_RESPAWN * CELL_SIZE)) far = false;
        }
        if (!far) continue;
        T->x = (x + 0.5f) * CELL_SIZE; T->y = (y + 0.5f) * CELL_SIZE;
        T->pathLength = 0; T->pathIndex = -1;
        moverRouteLen[tgt] = 0;
        break;
    }
    stamina[tgt] = 1.0f;
    fleeing[tgt] = false;
    pk->ducked = false;
    pk->lost = 0.0f;
    pk->lastX = T->x; pk->lastY = T->y; pk->lastHX = pk->lastHY = 0.0f;
}

// Can hunter h see the runner (in range, in sight, not crouched behind cover)?
static bool HunterSees(int h, int tgt) {
    return Dist(h, tgt) <= PACK_SIGHT * CELL_SIZE && CanSeeMover(h, tgt);
}

// Would the runner, crouched at (x, y), be out of every hunter's sight?
static bool HiddenFromPack(const PackState* pk, int x, int y, int z) {
    for (int k = 0; k < pk->n; k++) {
        const Mover* h = &movers[pk->hunters[k]];
        int hx = (int)(h->x / CELL_SIZE), hy = (int)(h->y / CELL_SIZE);
        float dx = (float)(x - hx), dy = (float)(y - hy);
        if (dx * dx + dy * dy <= (PACK_FLUSH + 1.0f) * (PACK_FLUSH + 1.0f)) return false;   // too near one
        if (dx * dx + dy * dy > PACK_SIGHT * PACK_SIGHT || !CanSeeCell(hx, hy, x, y, z)) continue;
        if (!CoverBetween(hx, hy, x, y, z)) return false;
    }
    return true;
}

// Out of their sight: a spot near by, next to cover (or in a bush), hidden from all of them
static bool RunnerDuck(const PackState* pk, int tgt) {
    const Mover* T = &movers[tgt];
    int z = (int)T->z, tx = (int)(T->x / CELL_SIZE), ty = (int)(T->y / CELL_SIZE);
    int r = (int)DUCK_RANGE, bestX = -1, bestY = -1;
    float best = -1e9f;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int x = tx + dx, y = ty + dy;
            if (dx * dx + dy * dy > r * r || x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
            if (!IsCellWalkableAt(z, y, x)) continue;
            bool cover = GRID(x, y, z) == CELL_BUSH;
            for (int oy = -1; oy <= 1 && !cover; oy++)
                for (int ox = -1; ox <= 1 && !cover; ox++) if ((ox || oy) && IsLowCover(x + ox, y + oy, z)) cover = true;
            if (!cover || !HiddenFromPack(pk, x, y, z)) continue;
            float nearest = 1e9f;   // far from the nearest hunter, not far to go
            for (int k = 0; k < pk->n; k++) {
                float hx = movers[pk->hunters[k]].x / CELL_SIZE - x, hy = movers[pk->hunters[k]].y / CELL_SIZE - y;
                float d = sqrtf(hx * hx + hy * hy);
                if (d < nearest) nearest = d;
            }
            float score = nearest - sqrtf((float)(dx * dx + dy * dy)) * 1.5f;
            if (GRID(x, y, z) == CELL_BUSH) score += 3.0f;   // hidden from every side
            // they'll come to where it was last seen, then where it was heading: hidden from both,
            // and off that line
            int lx = (int)(pk->lastX / CELL_SIZE), ly = (int)(pk->lastY / CELL_SIZE);
            int px = (int)((pk->lastX + pk->lastHX * 3.0f * CELL_SIZE) / CELL_SIZE), py = (int)((pk->lastY + pk->lastHY * 3.0f * CELL_SIZE) / CELL_SIZE);
            bool fromLast = CanSeeCell(lx, ly, x, y, z) && !CoverBetween(lx, ly, x, y, z);
            bool fromHead = CanSeeCell(px, py, x, y, z) && !CoverBetween(px, py, x, y, z);
            float hd = sqrtf((float)((x - px) * (x - px) + (y - py) * (y - py)));
            if (fromHead || hd < PACK_FLUSH + 1.0f) continue;
            if (!fromLast) score += 3.0f;
            if (score > best) { best = score; bestX = x; bestY = y; }
        }
    if (bestX < 0) return false;
    if (bestX == tx && bestY == ty) {   // right here: just get down
        Mover* m = &movers[tgt];
        m->pathLength = 0;
        m->pathIndex = -1;
        return true;
    }
    return PathTo(tgt, bestX, bestY, z);
}

// Lost it: go to where it was heading, then search round where it was last seen, at spots
// next to cover none of them can see now (they know the runner hides)
static void HunterSearch(const PackState* pk, int k) {
    int h = pk->hunters[k];
    int z = (int)movers[h].z, lx = (int)(pk->lastX / CELL_SIZE), ly = (int)(pk->lastY / CELL_SIZE);
    if (pk->lost < PACK_HEADED) {
        int ox, oy;
        float px = pk->lastX + pk->lastHX * 3.0f * CELL_SIZE, py = pk->lastY + pk->lastHY * 3.0f * CELL_SIZE;
        if (NearWalkable(px, py, z, &ox, &oy) && PathTo(h, ox, oy, z)) return;
    }
    float radius = 2.0f + pk->lost * 0.4f;
    if (radius > 9.0f) radius = 9.0f;
    int bestX = -1, bestY = -1;
    float best = -1e9f;
    for (int c = 0; c < 24; c++) {
        float an = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(0.0f, radius);
        int x = lx + (int)(cosf(an) * d), y = ly + (int)(sinf(an) * d);
        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || !IsCellWalkableAt(z, y, x)) continue;
        float score = ChaseRandRange(0.0f, 1.0f);
        if (GRID(x, y, z) == CELL_BUSH) score += 3.0f;
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                if ((ox || oy) && (IsLowCover(x + ox, y + oy, z) || !IsCellWalkableAt(z, y + oy, x + ox))) score += 1.0f;
        bool inView = false;   // a spot one of them can see right now isn't worth checking
        for (int q = 0; q < pk->n && !inView; q++) {
            const Mover* o = &movers[pk->hunters[q]];
            int ox = (int)(o->x / CELL_SIZE), oy = (int)(o->y / CELL_SIZE);
            float ddx = (float)(x - ox), ddy = (float)(y - oy);
            if (ddx * ddx + ddy * ddy < 6.0f * 6.0f && CanSeeCell(ox, oy, x, y, z) && !CoverBetween(ox, oy, x, y, z)) inView = true;
        }
        if (inView) score -= 4.0f;
        for (int q = 0; q < pk->n; q++) {   // spread out: not where another one is going
            if (q == k) continue;
            const Mover* o = &movers[pk->hunters[q]];
            float gx = (float)(o->goal.x - x), gy = (float)(o->goal.y - y);
            if (gx * gx + gy * gy < 9.0f) score -= 3.0f;
        }
        score -= sqrtf((movers[h].x / CELL_SIZE - x) * (movers[h].x / CELL_SIZE - x) + (movers[h].y / CELL_SIZE - y) * (movers[h].y / CELL_SIZE - y)) * 0.15f;
        if (score > best) { best = score; bestX = x; bestY = y; }
    }
    if (bestX >= 0) PathTo(h, bestX, bestY, z);
}

static bool PackCalm(int a, int i) {
    const PackState* pk = PackOf(a);
    return i == pk->target && !pk->ducked;
}

static void PackTick(int a, float dt) {
    PackState* pk = PackOf(a);
    int tgt = PkTarget(pk);
    if (tgt < 0) return;
    PlayWander(a, dt);
    if (pk->n == 0) return;
    Mover* T = &movers[tgt];
    pk->time += dt;
    // caught?
    for (int k = 0; k < pk->n; k++) {
        const Mover* h = &movers[pk->hunters[k]];
        if ((h->x - T->x) * (h->x - T->x) + (h->y - T->y) * (h->y - T->y) > (CATCH_DIST * CELL_SIZE) * (CATCH_DIST * CELL_SIZE)) continue;
        packCatches++;
        packCatchSecondsSum += pk->time;
        PackRespawn(pk, tgt);
        return;
    }
    // what the pack knows: where one of them last saw it (packStealth off: always where it is)
    bool seen = !packStealth;
    for (int k = 0; k < pk->n && !seen; k++) seen = HunterSees(pk->hunters[k], tgt);
    if (seen) {
        float hx = 0.0f, hy = 0.0f;
        Heading(tgt, &hx, &hy);
        pk->lastX = T->x; pk->lastY = T->y; pk->lastHX = hx; pk->lastHY = hy;
        pk->lost = 0.0f;
        pk->seenFor += dt;
    } else if (pk->seenFor = 0.0f, (pk->lost += dt) > PACK_GIVE_UP) {   // got away
        packEscapes++;
        PackRespawn(pk, tgt);
        return;
    }
    // the runner: flee from the pack when one is close (noticed, or very close), get tired;
    // out of their sight, duck behind cover and get its breath back
    bool threat = false;
    for (int k = 0; k < pk->n && !threat; k++) threat = ThreatClose(tgt, pk->hunters[k]);
    if (fleeing[tgt]) { stamina[tgt] -= STAMINA_DRAIN * dt; if (stamina[tgt] < 0.0f) stamina[tgt] = 0.0f; }
    else { stamina[tgt] += STAMINA_REST * dt; if (stamina[tgt] > 1.0f) stamina[tgt] = 1.0f; }
    if (pk->ducked) {
        bool near = false;
        for (int k = 0; k < pk->n && !near; k++) near = Dist(pk->hunters[k], tgt) < PACK_FLUSH * CELL_SIZE;
        if (pk->seenFor >= GLIMPSE || near) {   // spotted (a glimpse isn't enough), or one right there: run again
            pk->ducked = false;
            packFlushed++;
            pk->targetReplan = 0.0f;
        } else {
            fleeing[tgt] = T->pathIndex >= 0;   // running to the spot; there: still, resting
            if (T->pathIndex < 0) packDuckSecondsSum += dt;
        }
    }
    if (!pk->ducked) {
        // out of their sight (they're hunting it): lie low behind cover. (Making for cover while
        // they watch doesn't work - by the time it's there they've moved round and see it, tried:
        // 0.3 s; nor only right after a chase - with four on it, it never breaks sight then.)
        bool chance = pk->lost >= GLIMPSE;
        if (packStealth && chance && (pk->duckTry -= dt) <= 0.0f && (pk->duckTry = 0.5f, RunnerDuck(pk, tgt))) {
            pk->ducked = true;
            packDucks++;
        } else {
            if (threat) {
                pk->targetReplan -= dt;
                if (!fleeing[tgt] || T->pathIndex < 0 || pk->targetReplan <= 0.0f) {
                    FleeRunFrom(tgt, pk->hunters, pk->n);
                    pk->targetReplan = CHASE_REPLAN;
                }
            }
            fleeing[tgt] = threat;
        }
    }
    if (!seen) {   // the hunters: lost it, search
        for (int k = 0; k < pk->n; k++) {
            pk->role[k] = 3;
            pk->replan[k] -= dt;
            if (movers[pk->hunters[k]].pathIndex >= 0 && pk->replan[k] > 0.0f &&
                !(pk->lost <= dt * 1.5f)) continue;   // (just lost it: re-aim at once)
            pk->replan[k] = 3.0f;
            HunterSearch(pk, k);
        }
        return;
    }
    // where it's going: its goal if running, else away from the pack's middle
    float cx = 0.0f, cy = 0.0f;
    for (int k = 0; k < pk->n; k++) { cx += movers[pk->hunters[k]].x; cy += movers[pk->hunters[k]].y; }
    cx /= pk->n; cy /= pk->n;
    float gx, gy;
    if (T->pathIndex >= 0) { gx = (T->goal.x + 0.5f) * CELL_SIZE; gy = (T->goal.y + 0.5f) * CELL_SIZE; }
    else {
        float ax = T->x - cx, ay = T->y - cy, al = sqrtf(ax * ax + ay * ay);
        if (al < 1.0f) { ax = 1.0f; ay = 0.0f; al = 1.0f; }
        gx = T->x + ax / al * PACK_CUT_AHEAD * CELL_SIZE; gy = T->y + ay / al * PACK_CUT_AHEAD * CELL_SIZE;
    }
    float ex = gx - T->x, ey = gy - T->y, el = sqrtf(ex * ex + ey * ey);
    if (el < 1.0f) { ex = T->x - cx; ey = T->y - cy; el = sqrtf(ex * ex + ey * ey); if (el < 1.0f) { ex = 1.0f; ey = 0.0f; el = 1.0f; } }
    ex /= el; ey /= el;
    if (packTactics) PackAssignRoles(pk, gx, gy);
    // the hunters
    int z = (int)T->z;
    for (int k = 0; k < pk->n; k++) {
        int h = pk->hunters[k];
        pk->replan[k] -= dt;
        if (pk->replan[k] > 0.0f && movers[h].pathIndex >= 0) continue;
        pk->replan[k] = CHASE_REPLAN;
        float dx = T->x - movers[h].x, dy = T->y - movers[h].y;
        if (!packTactics || pk->role[k] == 0 || dx * dx + dy * dy < (PACK_CLOSE * CELL_SIZE) * (PACK_CLOSE * CELL_SIZE)) {
            ChaseRun(h, tgt);
            continue;
        }
        float px, py;
        if (pk->role[k] == 1) { px = gx; py = gy; }   // cut it off where it's going
        else {   // flank: ahead and to the side it's on
            float side = (movers[h].x - T->x) * (-ey) + (movers[h].y - T->y) * ex >= 0.0f ? 1.0f : -1.0f;
            px = T->x + ex * PACK_AHEAD * CELL_SIZE - ey * side * PACK_SIDE * CELL_SIZE;
            py = T->y + ey * PACK_AHEAD * CELL_SIZE + ex * side * PACK_SIDE * CELL_SIZE;
        }
        int ox, oy;
        if (!NearWalkable(px, py, z, &ox, &oy) || !PathTo(h, ox, oy, z)) ChaseRun(h, tgt);
    }
}

// Crouched behind cover: still
static bool PackMoverTick(int a, int i, float dt) {
    (void)a; (void)dt;
    if (!PackIsDucking(i)) return false;
    Mover* m = &movers[i];
    m->timeWithoutProgress = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}

// A hunter leaves: out of the pack. The runner leaves: the hunt is over (the hunters leave)
static void PackMemberLeft(int a, int i) {
    PackState* pk = PackOf(a);
    int p = PackSlot(pk, i);
    if (p >= 0) {
        for (int q = p; q < pk->n - 1; q++) {
            pk->hunters[q] = pk->hunters[q + 1];
            pk->role[q] = pk->role[q + 1];
            pk->replan[q] = pk->replan[q + 1];
        }
        pk->n--;
    }
    if (i == pk->target) { pk->target = -1; pk->n = 0; }
}

static bool PackTooSmall(int a) {
    const PackState* pk = PackOf(a);
    return pk->target < 0 || pk->n == 0 || !ActivityHas(a, pk->target);
}

const ActivityDef packHuntActivityDef = {
    "pack-hunt", "Hunting as a pack", sizeof(PackState),
    PackTick, PackMoverTick, PackSpeedScale, PackMemberLeft, PackTooSmall, NULL,
    0, 0, NULL,   // (hunting, not play: never proposed)
};

// ============================================================================
// What a player is doing in its game, for the describe lines ("Playing tag: it")
// ============================================================================
static const char* ChaseNameOf(int i) { return (i >= 0 && i < moverCount && movers[i].name[0]) ? movers[i].name : "someone"; }

bool ChaseDescribe(int i, char* buf, int len) {
    if (!ChaseIsPlaying(i)) return false;
    int a = MoverActivity(i);
    if (a < 0 || !activities[a].active) { snprintf(buf, (size_t)len, "Waiting to play"); return true; }
    switch (activities[a].kind) {
        case ACT_TAG: {
            const TagState* t = TagOf(a);
            if (i == t->it && t->freeze > 0.0f) snprintf(buf, (size_t)len, "Playing tag: it, counting to three");
            else if (i == t->it) snprintf(buf, (size_t)len, t->target >= 0 ? "Playing tag: it, after %s" : "Playing tag: it", ChaseNameOf(t->target));
            else if (fleeing[i]) snprintf(buf, (size_t)len, "Playing tag: running from %s", ChaseNameOf(t->it));
            else snprintf(buf, (size_t)len, "Playing tag (%s is it)", ChaseNameOf(t->it));
            return true;
        }
        case ACT_HIDE_SEEK: {
            const HideState* hs = HideOf(a);
            const char* what = "playing";
            if (i == hs->seeker) what = hs->phase == HS_COUNT ? "counting, eyes shut" : "seeking";
            else if (hiderState[i] == HIDER_GOING) what = "running off to hide";
            else if (hiderState[i] == HIDER_HIDDEN) what = "hiding";
            else if (hiderState[i] == HIDER_FOUND) what = "found";
            snprintf(buf, (size_t)len, "Hide and seek: %s", what);
            return true;
        }
        case ACT_FOLLOW_LEADER: {
            const FollowState* fl = FollowOf(a);
            int k = FollowSlot(fl, i);
            if (k == 0) snprintf(buf, (size_t)len, "Follow the leader: leading");
            else if (k > 0) snprintf(buf, (size_t)len, "Follow the leader: behind %s", ChaseNameOf(fl->order[k - 1]));
            else snprintf(buf, (size_t)len, "Follow the leader");
            return true;
        }
        case ACT_PACK_HUNT: {
            static const char* roles[4] = {"driving it", "cutting it off", "flanking", "searching"};
            const PackState* pk = PackOf(a);
            int r = PackRole(i);
            if (i == pk->target) snprintf(buf, (size_t)len, pk->ducked ? "Hunted: lying low" : "Hunted by the pack");
            else if (r >= 0 && r < 4) snprintf(buf, (size_t)len, "Pack hunt: %s", roles[r]);
            else snprintf(buf, (size_t)len, "Pack hunt");
            return true;
        }
    }
    snprintf(buf, (size_t)len, "%s", ActivityDefOf(activities[a].kind)->label);
    return true;
}
