// chase.c - chasing and fleeing, and a game of tag (see chase.h)
#include "chase.h"
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

bool fleeSmart = true;
float chaseSpeed = 1.25f;
float fleeSpeed = 1.2f;
float fleeRadius = 5.0f;
long tagCount = 0;
double tagChaseSecondsSum = 0.0;

static bool playing[MAX_MOVERS];
static float wanderTimer[MAX_MOVERS];   // a calm player's pause between strolls
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

static void ClearHideSeek(void);

static void ClearFollow(void);

static void ClearPack(void);

long playLeftCount = 0;

void ClearChase(void) {
    ClearHideSeek();
    ClearFollow();
    ClearPack();
    memset(playing, 0, sizeof(playing));
    memset(fleeing, 0, sizeof(fleeing));
    memset(replanTimer, 0, sizeof(replanTimer));
    for (int i = 0; i < MAX_MOVERS; i++) stamina[i] = 1.0f;
    tagIt = tagTarget = -1;
    tagTargetHeld = tagFreeze = tagChaseTime = 0.0f;
    tagCount = 0;
    tagChaseSecondsSum = 0.0;
    playLeftCount = 0;
    memset(wanderTimer, 0, sizeof(wanderTimer));
    for (int i = 0; i < moverCount; i++)   // nobody is playing any more
        if (movers[i].freetimeState == FREETIME_PLAY) movers[i].freetimeState = FREETIME_NONE;
}

// --- Playing is a free-time state (FREETIME_PLAY) ---
static void GameDrop(int i);

static bool PlayerFree(int i) {
    const Mover* m = &movers[i];
    return m->active && m->currentJobId < 0 && !m->isDrafted && !QueueSuspended(i) &&
           (m->freetimeState == FREETIME_NONE || m->freetimeState == FREETIME_PLAY);
}

bool ChaseJoin(int i) {
    if (i < 0 || i >= moverCount || !PlayerFree(i)) return false;
    if (!playing[i]) { stamina[i] = 1.0f; fleeing[i] = false; wanderTimer[i] = 0.0f; }
    playing[i] = true;
    movers[i].freetimeState = FREETIME_PLAY;
    return true;
}

void ChaseLeave(int i) {
    if (i < 0 || i >= MAX_MOVERS || !playing[i]) return;
    playing[i] = false;
    fleeing[i] = false;
    GameDrop(i);
    if (i < moverCount && movers[i].freetimeState == FREETIME_PLAY) {   // (a need, job, draft or command took over: theirs now)
        Mover* m = &movers[i];
        m->freetimeState = FREETIME_NONE;
        m->pathLength = 0;
        m->pathIndex = -1;
    }
    playLeftCount++;
}

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
bool ChaseIsPlaying(int i) { return i >= 0 && i < moverCount && playing[i] && movers[i].active; }
bool ChaseIsFleeing(int i) { return ChaseIsPlaying(i) && fleeing[i]; }
int ChaseIt(void) { return (tagIt >= 0 && tagIt < moverCount && movers[tagIt].active) ? tagIt : -1; }
void ChaseSetIt(int i) { tagIt = i; tagTarget = -1; tagFreeze = TAG_FREEZE; tagChaseTime = 0.0f; }

static float FollowSpeedScale(int i);

static float PackSpeedScale(int i);

float ChaseSpeedScale(int i) {
    float fs = FollowSpeedScale(i);
    if (fs > 0.0f) return fs;
    float ps = PackSpeedScale(i);
    if (ps > 0.0f) return ps;
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

static void HideSeekTick(float dt);

static void FollowLeaderTick(float dt);

static void PackTick(float dt);

static void PlaySweep(void);
static void PlayWander(float dt);

void ChaseTick(float dt) {
    PlaySweep();
    PlayWander(dt);
    HideSeekTick(dt);
    FollowLeaderTick(dt);
    PackTick(dt);
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

static bool HideMoverTick(int i);

static bool FollowMoverTick(int i, float dt);

bool ChaseMoverTick(int i, float dt) {
    if (FollowMoverTick(i, dt)) return true;
    if (HideMoverTick(i)) return true;
    if (PackIsDucking(i)) {   // crouched behind cover: still
        Mover* m = &movers[i];
        m->timeWithoutProgress = 0.0f;
        m->lastX = m->x;
        m->lastY = m->y;
        return true;
    }
    if (i != ChaseIt() || tagFreeze <= 0.0f) return false;
    Mover* m = &movers[i];   // counting to three
    m->pathLength = 0;
    m->pathIndex = -1;
    m->timeWithoutProgress = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}

// ============================================================================
// Hide and seek
// ============================================================================
bool hideSmart = true;
long hideFoundCount = 0, hideRounds = 0;
double hideFoundSecondsSum = 0.0;

enum { HS_NONE, HS_COUNT, HS_SEEK };
enum { HIDER_GOING, HIDER_HIDDEN, HIDER_FOUND };
static int hsPhase = HS_NONE, hsSeeker = -1, hsFirstFound = -1;
static float hsTimer = 0.0f, hsLookTimer = 0.0f;
static Point hsHome;
static int hsState[MAX_MOVERS];
static unsigned char* hsSeen = NULL;   // per cell: the seeker has looked here this round
static int hsSeenW = 0, hsSeenH = 0;

#define HS_COUNT_TIME   10.0f   // seconds the seeker counts
#define HS_ROUND_TIME   90.0f   // seconds of seeking before the round ends anyway
#define HS_MIN          4.0f    // cells: hiding spots between these from where the hider stands
#define HS_MAX          14.0f
#define HS_CANDIDATES   40
#define HS_LOOK_RADIUS  6       // cells the seeker marks as looked at (in sight)

static void ClearHideSeek(void) {
    hsPhase = HS_NONE;
    hsSeeker = hsFirstFound = -1;
    hideFoundCount = hideRounds = 0;
    hideFoundSecondsSum = 0.0;
    free(hsSeen);
    hsSeen = NULL;
    hsSeenW = hsSeenH = 0;
}

int HideSeeker(void) { return hsPhase != HS_NONE && hsSeeker >= 0 && hsSeeker < moverCount ? hsSeeker : -1; }
bool HideIsCounting(int i) { return hsPhase == HS_COUNT && i == hsSeeker; }
bool HideIsHidden(int i) { return hsPhase != HS_NONE && ChaseIsPlaying(i) && i != hsSeeker && hsState[i] == HIDER_HIDDEN; }
// Every player's goals come from its game (a calm one wanders round the others: PlayWander)
bool HideControls(int i) { return ChaseIsPlaying(i); }
bool PackIsDucking(int i);
bool MoverCrouching(int i) { return HideIsHidden(i) || PackIsDucking(i); }
bool HideIsFound(int i) { return hsPhase != HS_NONE && ChaseIsPlaying(i) && i != hsSeeker && hsState[i] == HIDER_FOUND; }

static bool SeenCell(int x, int y) { return hsSeen && x >= 0 && y >= 0 && x < hsSeenW && y < hsSeenH && hsSeen[y * hsSeenW + x]; }

// A hiding spot for i: hideSmart = out of sight from home, tucked in, away from other hiders
static bool PickHidingSpot(int i) {
    const Mover* m = &movers[i];
    int z = (int)m->z, bestX = -1, bestY = -1;
    float bestScore = -1e9f;
    for (int c = 0; c < HS_CANDIDATES; c++) {
        float a = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(HS_MIN, HS_MAX);
        int cx = (int)((m->x + cosf(a) * d * CELL_SIZE) / CELL_SIZE), cy = (int)((m->y + sinf(a) * d * CELL_SIZE) / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || !IsCellWalkableAt(z, cy, cx)) continue;
        float score = 0.0f;
        if (hideSmart) {
            bool seen = CanSeeCell(hsHome.x, hsHome.y, cx, cy, z);   // from home (crouched behind cover: not)
            if (seen && coverEnabled && CoverBetween(hsHome.x, hsHome.y, cx, cy, z)) seen = false;
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
                if (j == i || !ChaseIsPlaying(j) || j == hsSeeker) continue;
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

void HideSeekStart(int seeker) {
    hsSeeker = seeker;
    hsPhase = HS_COUNT;
    hsTimer = HS_COUNT_TIME;
    hsFirstFound = -1;
    hsHome = (Point){(int)(movers[seeker].x / CELL_SIZE), (int)(movers[seeker].y / CELL_SIZE), (int)movers[seeker].z};
    if (hsSeenW != gridWidth || hsSeenH != gridHeight) {
        free(hsSeen);
        hsSeenW = gridWidth; hsSeenH = gridHeight;
        hsSeen = calloc((size_t)hsSeenW * hsSeenH, 1);
    } else if (hsSeen) memset(hsSeen, 0, (size_t)hsSeenW * hsSeenH);
    hideRounds++;
    movers[seeker].pathLength = 0;
    movers[seeker].pathIndex = -1;
    for (int i = 0; i < moverCount; i++) {
        if (!ChaseIsPlaying(i) || i == seeker) continue;
        hsState[i] = HIDER_GOING;
        fleeing[i] = true;   // runs to its spot (fleeSpeed, ChaseSpeedScale)
        PickHidingSpot(i);
    }
}

// The seeker marks what it can see as looked at, then goes where it hasn't looked
static void SeekerLook(int s) {
    const Mover* m = &movers[s];
    int sx = (int)(m->x / CELL_SIZE), sy = (int)(m->y / CELL_SIZE), z = (int)m->z;
    for (int dy = -HS_LOOK_RADIUS; dy <= HS_LOOK_RADIUS; dy++)
        for (int dx = -HS_LOOK_RADIUS; dx <= HS_LOOK_RADIUS; dx++) {
            int x = sx + dx, y = sy + dy;
            if (x < 0 || y < 0 || x >= hsSeenW || y >= hsSeenH || dx * dx + dy * dy > HS_LOOK_RADIUS * HS_LOOK_RADIUS) continue;
            if (hsSeen[y * hsSeenW + x] || !IsCellWalkableAt(z, y, x)) continue;
            if (!CanSeeCell(sx, sy, x, y, z)) continue;
            // behind low cover from here: someone could be crouching there, it has to go and look
            if (coverEnabled && dx * dx + dy * dy > 4 && CoverBetween(sx, sy, x, y, z)) continue;
            hsSeen[y * hsSeenW + x] = 1;
        }
}

static void SeekerGo(int s) {
    const Mover* m = &movers[s];
    int z = (int)m->z, bestX = -1, bestY = -1;
    float bestScore = -1e9f;
    for (int c = 0; c < 30; c++) {   // an unlooked-at cell, not too far
        float a = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(3.0f, 12.0f);
        int cx = (int)((m->x + cosf(a) * d * CELL_SIZE) / CELL_SIZE), cy = (int)((m->y + sinf(a) * d * CELL_SIZE) / CELL_SIZE);
        if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || !IsCellWalkableAt(z, cy, cx)) continue;
        int unseen = 0;   // how much unlooked-at ground round it
        for (int oy = -2; oy <= 2; oy++)
            for (int ox = -2; ox <= 2; ox++)
                if (IsCellWalkableAt(z, cy + oy, cx + ox) && !SeenCell(cx + ox, cy + oy)) unseen++;
        float score = unseen - d * 0.3f + ChaseRandRange(0.0f, 1.0f);
        if (score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
    }
    if (bestX >= 0) PathTo(s, bestX, bestY, z);
}

static void HideSeekTick(float dt) {
    if (hsPhase == HS_NONE || hsSeeker < 0 || !movers[hsSeeker].active) return;
    int s = hsSeeker;
    hsTimer -= dt;
    // hiders: arrived at the spot -> hidden; found ones go home
    for (int i = 0; i < moverCount; i++) {
        if (!ChaseIsPlaying(i) || i == s) continue;
        if (hsState[i] == HIDER_GOING && movers[i].pathIndex < 0) {
            hsState[i] = HIDER_HIDDEN;
            fleeing[i] = false;
        }
    }
    if (hsPhase == HS_COUNT) {
        if (hsTimer <= 0.0f) { hsPhase = HS_SEEK; hsTimer = HS_ROUND_TIME; hsLookTimer = 0.0f; }
        return;
    }
    // seeking: look, spot, go on
    if ((hsLookTimer -= dt) <= 0.0f) { SeekerLook(s); hsLookTimer = 0.5f; }
    const MoverNotice* n = &moverNotice[s];
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (!ChaseIsPlaying(j) || j == s || hsState[j] == HIDER_FOUND) continue;
        hsState[j] = HIDER_FOUND;   // spotted!
        fleeing[j] = false;
        hideFoundCount++;
        hideFoundSecondsSum += HS_ROUND_TIME - hsTimer;
        if (hsFirstFound < 0) hsFirstFound = j;
        PathTo(j, hsHome.x, hsHome.y, hsHome.z);   // walk home
    }
    if (movers[s].pathIndex < 0) SeekerGo(s);
    bool anyLeft = false;
    for (int i = 0; i < moverCount; i++)
        if (ChaseIsPlaying(i) && i != s && hsState[i] != HIDER_FOUND) anyLeft = true;
    if (!anyLeft || hsTimer <= 0.0f) {   // next round: the first one found seeks
        int next = hsFirstFound >= 0 ? hsFirstFound : s;
        PathTo(s, hsHome.x, hsHome.y, hsHome.z);
        HideSeekStart(next);
    }
}

// Standing still: the seeker counting, hiders in their spot, found ones waiting at home
static bool HideMoverTick(int i) {
    if (hsPhase == HS_NONE || !ChaseIsPlaying(i)) return false;
    bool stand = (i == hsSeeker && hsPhase == HS_COUNT) || (i != hsSeeker && hsState[i] == HIDER_HIDDEN) ||
                 (i != hsSeeker && hsState[i] == HIDER_FOUND && movers[i].pathIndex < 0);
    if (!stand) return false;
    Mover* m = &movers[i];
    m->pathLength = 0;
    m->pathIndex = -1;
    m->timeWithoutProgress = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}

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

static int flOrder[FOLLOW_MAX], flCount = 0;      // movers, flOrder[0] leads
static float flTrailX[FOLLOW_MAX][FL_TRAIL], flTrailY[FOLLOW_MAX][FL_TRAIL];
static int flTrailHead[FOLLOW_MAX], flTrailLen[FOLLOW_MAX];
static float flTurn = 0.0f;
// leader's route for the stats: a longer trail of the leader only
#define FL_ROUTE 512
static float flRouteX[FL_ROUTE], flRouteY[FL_ROUTE];
static int flRouteHead = 0, flRouteLen = 0;

static void ClearFollow(void) { flCount = 0; flRouteLen = 0; followLeaderChanges = 0; }

static int FollowSlot(int i) {   // index in flOrder, -1
    for (int k = 0; k < flCount; k++) if (flOrder[k] == i) return k;
    return -1;
}
int FollowPlace(int i) { return FollowSlot(i); }
int FollowLeader(void) { return flCount > 0 ? flOrder[0] : -1; }

static float FollowSpeedScale(int i) {
    int k = FollowSlot(i);
    return k == 0 ? FL_LEAD_PACE : 0.0f;   // (followers move themselves: FollowMoverTick)
}

// The trail of slot k: newest first, index n back
static bool TrailPoint(int k, int n, float* x, float* y) {
    if (n >= flTrailLen[k]) return false;
    int idx = (flTrailHead[k] - n + FL_TRAIL) % FL_TRAIL;
    *x = flTrailX[k][idx]; *y = flTrailY[k][idx];
    return true;
}

static void TrailRecord(int k, float x, float y) {
    float lx, ly;
    if (TrailPoint(k, 0, &lx, &ly) && (lx - x) * (lx - x) + (ly - y) * (ly - y) < FL_TRAIL_STEP * FL_TRAIL_STEP) return;
    flTrailHead[k] = (flTrailHead[k] + 1) % FL_TRAIL;
    flTrailX[k][flTrailHead[k]] = x; flTrailY[k][flTrailHead[k]] = y;
    if (flTrailLen[k] < FL_TRAIL) flTrailLen[k]++;
}

void FollowLeaderStart(const int* players, int n) {
    flCount = n > FOLLOW_MAX ? FOLLOW_MAX : n;
    for (int k = 0; k < flCount; k++) {
        flOrder[k] = players[k];
        flTrailLen[k] = 0; flTrailHead[k] = 0;
        ChaseJoin(players[k]);
        TrailRecord(k, movers[players[k]].x, movers[players[k]].y);
        movers[players[k]].pathLength = 0;
        movers[players[k]].pathIndex = -1;
    }
    flTurn = FL_TURN_TIME;
    flRouteLen = 0;
}

static void LeaderNextLeg(int leader) {
    const Mover* m = &movers[leader];
    for (int tries = 0; tries < 12; tries++) {
        float a = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(5.0f, 10.0f);
        int gx = (int)((m->x + cosf(a) * d * CELL_SIZE) / CELL_SIZE), gy = (int)((m->y + sinf(a) * d * CELL_SIZE) / CELL_SIZE);
        if (PathTo(leader, gx, gy, (int)m->z)) return;
    }
}

static void FollowLeaderTick(float dt) {
    if (flCount < 2) return;
    // trails (each slot's trail is about the mover in it) and the leader's longer route
    for (int k = 0; k < flCount; k++) TrailRecord(k, movers[flOrder[k]].x, movers[flOrder[k]].y);
    {
        const Mover* L = &movers[flOrder[0]];
        int last = (flRouteHead + FL_ROUTE - 1) % FL_ROUTE;
        if (flRouteLen == 0 || (flRouteX[last] - L->x) * (flRouteX[last] - L->x) + (flRouteY[last] - L->y) * (flRouteY[last] - L->y) >= FL_TRAIL_STEP * FL_TRAIL_STEP) {
            flRouteX[flRouteHead] = L->x; flRouteY[flRouteHead] = L->y;
            flRouteHead = (flRouteHead + 1) % FL_ROUTE;
            if (flRouteLen < FL_ROUTE) flRouteLen++;
        }
    }
    if (movers[flOrder[0]].pathIndex < 0) LeaderNextLeg(flOrder[0]);
    if ((flTurn -= dt) <= 0.0f) {   // the leader goes to the back; trails move with their movers
        int lead = flOrder[0];
        float tx[FL_TRAIL], ty[FL_TRAIL];
        int th = flTrailHead[0], tl = flTrailLen[0];
        memcpy(tx, flTrailX[0], sizeof(tx)); memcpy(ty, flTrailY[0], sizeof(ty));
        for (int k = 0; k + 1 < flCount; k++) {
            flOrder[k] = flOrder[k + 1];
            memcpy(flTrailX[k], flTrailX[k + 1], sizeof(tx)); memcpy(flTrailY[k], flTrailY[k + 1], sizeof(ty));
            flTrailHead[k] = flTrailHead[k + 1]; flTrailLen[k] = flTrailLen[k + 1];
        }
        flOrder[flCount - 1] = lead;
        memcpy(flTrailX[flCount - 1], tx, sizeof(tx)); memcpy(flTrailY[flCount - 1], ty, sizeof(ty));
        flTrailHead[flCount - 1] = th; flTrailLen[flCount - 1] = tl;
        movers[lead].pathLength = 0; movers[lead].pathIndex = -1;
        flTurn = FL_TURN_TIME;   // (the route goes on: the new leader was walking it)
        followLeaderChanges++;
    }
}

// Where follower slot k should head: a point about FL_GAP back along the trail of the one ahead
// (followTrail), or straight at the one ahead
static bool FollowTarget(int k, float* x, float* y) {
    int ahead = flOrder[k - 1];
    if (!followTrail) {
        float dx = movers[flOrder[k]].x - movers[ahead].x, dy = movers[flOrder[k]].y - movers[ahead].y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < 0.5f) return false;
        *x = movers[ahead].x + dx / d * FL_GAP * CELL_SIZE;
        *y = movers[ahead].y + dy / d * FL_GAP * CELL_SIZE;
        return true;
    }
    float along = 0.0f, px, py;
    if (!TrailPoint(k - 1, 0, &px, &py)) return false;
    for (int n = 1; ; n++) {   // walk back along the trail until FL_GAP cells of it
        float qx, qy;
        if (!TrailPoint(k - 1, n, &qx, &qy)) break;
        along += sqrtf((qx - px) * (qx - px) + (qy - py) * (qy - py));
        px = qx; py = qy;
        if (along >= FL_GAP * CELL_SIZE) break;
    }
    // the follower's own position: if it's already past that point (close to the one ahead), wait
    *x = px; *y = py;
    return true;
}

static bool FollowMoverTick(int i, float dt) {
    int k = FollowSlot(i);
    if (k <= 0) return false;   // the leader walks its path; not playing: nothing here
    Mover* m = &movers[i];
    float tx, ty;
    if (FollowTarget(k, &tx, &ty)) {
        float dx = tx - m->x, dy = ty - m->y, d = sqrtf(dx * dx + dy * dy);
        float ax = movers[flOrder[k - 1]].x - m->x, ay = movers[flOrder[k - 1]].y - m->y;
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
    m->pathLength = 0;
    m->pathIndex = -1;
    m->timeWithoutProgress = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}

bool FollowTrailDistance(int i, float* cells) {
    int k = FollowSlot(i);
    if (k <= 0 || flRouteLen < 8) return false;
    float best = 1e18f;
    for (int n = 0; n < flRouteLen; n++) {
        float dx = flRouteX[n] - movers[i].x, dy = flRouteY[n] - movers[i].y;
        if (dx * dx + dy * dy < best) best = dx * dx + dy * dy;
    }
    *cells = sqrtf(best) / CELL_SIZE;
    return true;
}

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

static int packHunters[PACK_MAX], packN = 0, packTarget = -1;
static int packRole[PACK_MAX];
static float packReplan[PACK_MAX], packTime = 0.0f, packTargetReplan = 0.0f;
static float packLastX, packLastY, packLastHX, packLastHY, packLost = 0.0f;   // where the pack last saw it
static bool packDucked = false;   // the runner is going to / crouching in a spot behind cover
static float packSeenFor = 0.0f;      // seconds the pack has had it in sight without a break

static void ClearPack(void) {
    packN = 0; packTarget = -1; packCatches = 0; packCatchSecondsSum = 0.0;
    packEscapes = packDucks = packFlushed = 0; packDuckSecondsSum = 0.0; packDucked = false; packLost = 0.0f;
}
int PackTarget(void) { return packTarget >= 0 && packTarget < moverCount && movers[packTarget].active ? packTarget : -1; }

static int PackSlot(int i) {
    for (int k = 0; k < packN; k++) if (packHunters[k] == i) return k;
    return -1;
}
int PackRole(int i) { int k = PackSlot(i); return k >= 0 && PackTarget() >= 0 ? packRole[k] : -1; }
bool PackIsDucking(int i) { return PackTarget() >= 0 && i == packTarget && packDucked && movers[i].pathIndex < 0; }
bool PackLost(float* seconds) { if (PackTarget() < 0 || packLost <= 0.0f) return false; if (seconds) *seconds = packLost; return true; }

static float PackSpeedScale(int i) {
    if (PackTarget() < 0) return 0.0f;
    if (PackSlot(i) >= 0) return packSpeed;
    if (i == packTarget) return fleeing[i] ? (stamina[i] > TIRED ? packRunnerSpeed : TIRED_SPEED) : 1.0f;
    return 0.0f;
}

void PackHuntStart(const int* hunters, int n, int target) {
    packN = n > PACK_MAX ? PACK_MAX : n;
    for (int k = 0; k < packN; k++) {
        packHunters[k] = hunters[k];
        packRole[k] = 0;
        packReplan[k] = 0.1f * k;   // staggered
        ChaseJoin(hunters[k]);
    }
    packTarget = target;
    ChaseJoin(target);
    stamina[target] = 1.0f;
    fleeing[target] = false;
    packTime = 0.0f;
    packDucked = false;
    packLost = 0.0f;
    packLastX = movers[target].x; packLastY = movers[target].y;
    packLastHX = packLastHY = 0.0f;
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
static void PackAssignRoles(float ex, float ey, float gx, float gy) {
    const Mover* t = &movers[packTarget];
    int driver = -1, cutter = -1;
    float best = 1e18f;
    for (int k = 0; k < packN; k++) {
        const Mover* h = &movers[packHunters[k]];
        float d = (h->x - t->x) * (h->x - t->x) + (h->y - t->y) * (h->y - t->y);
        if (d < best) { best = d; driver = k; }
    }
    best = 1e18f;
    for (int k = 0; k < packN; k++) {
        if (k == driver) continue;
        const Mover* h = &movers[packHunters[k]];
        float d = (h->x - gx) * (h->x - gx) + (h->y - gy) * (h->y - gy);
        if (d < best) { best = d; cutter = k; }
    }
    for (int k = 0; k < packN; k++) packRole[k] = k == driver ? 0 : (k == cutter ? 1 : 2);
    (void)ex; (void)ey;
}

// Caught or got away: the runner turns up again far from the pack (a new round)
static void PackRespawn(int tgt) {
    Mover* T = &movers[tgt];
    packTime = 0.0f;
    for (int tries = 0; tries < 60; tries++) {
        int x = GetRandomValue(1, gridWidth - 2), y = GetRandomValue(1, gridHeight - 2), z = (int)T->z;
        if (!IsCellWalkableAt(z, y, x)) continue;
        bool far = true;
        for (int q = 0; q < packN; q++) {
            float dx = (x + 0.5f) * CELL_SIZE - movers[packHunters[q]].x, dy = (y + 0.5f) * CELL_SIZE - movers[packHunters[q]].y;
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
    packDucked = false;
    packLost = 0.0f;
    packLastX = T->x; packLastY = T->y; packLastHX = packLastHY = 0.0f;
}

// Can hunter h see the runner (in range, in sight, not crouched behind cover)?
static bool HunterSees(int h, int tgt) {
    return Dist(h, tgt) <= PACK_SIGHT * CELL_SIZE && CanSeeMover(h, tgt);
}

// Would the runner, crouched at (x, y), be out of every hunter's sight?
static bool HiddenFromPack(int x, int y, int z) {
    for (int k = 0; k < packN; k++) {
        const Mover* h = &movers[packHunters[k]];
        int hx = (int)(h->x / CELL_SIZE), hy = (int)(h->y / CELL_SIZE);
        float dx = (float)(x - hx), dy = (float)(y - hy);
        if (dx * dx + dy * dy <= (PACK_FLUSH + 1.0f) * (PACK_FLUSH + 1.0f)) return false;   // too near one
        if (dx * dx + dy * dy > PACK_SIGHT * PACK_SIGHT || !CanSeeCell(hx, hy, x, y, z)) continue;
        if (!CoverBetween(hx, hy, x, y, z)) return false;
    }
    return true;
}

// Out of their sight: a spot near by, next to cover (or in a bush), hidden from all of them
static bool RunnerDuck(int tgt) {
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
            if (!cover || !HiddenFromPack(x, y, z)) continue;
            float nearest = 1e9f;   // far from the nearest hunter, not far to go
            for (int k = 0; k < packN; k++) {
                float hx = movers[packHunters[k]].x / CELL_SIZE - x, hy = movers[packHunters[k]].y / CELL_SIZE - y;
                float d = sqrtf(hx * hx + hy * hy);
                if (d < nearest) nearest = d;
            }
            float score = nearest - sqrtf((float)(dx * dx + dy * dy)) * 1.5f;
            if (GRID(x, y, z) == CELL_BUSH) score += 3.0f;   // hidden from every side
            // they'll come to where it was last seen, then where it was heading: hidden from both,
            // and off that line
            int lx = (int)(packLastX / CELL_SIZE), ly = (int)(packLastY / CELL_SIZE);
            int px = (int)((packLastX + packLastHX * 3.0f * CELL_SIZE) / CELL_SIZE), py = (int)((packLastY + packLastHY * 3.0f * CELL_SIZE) / CELL_SIZE);
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
static void HunterSearch(int k) {
    int h = packHunters[k];
    int z = (int)movers[h].z, lx = (int)(packLastX / CELL_SIZE), ly = (int)(packLastY / CELL_SIZE);
    if (packLost < PACK_HEADED) {
        int ox, oy;
        float px = packLastX + packLastHX * 3.0f * CELL_SIZE, py = packLastY + packLastHY * 3.0f * CELL_SIZE;
        if (NearWalkable(px, py, z, &ox, &oy) && PathTo(h, ox, oy, z)) return;
    }
    float radius = 2.0f + packLost * 0.4f;
    if (radius > 9.0f) radius = 9.0f;
    int bestX = -1, bestY = -1;
    float best = -1e9f;
    for (int c = 0; c < 24; c++) {
        float a = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(0.0f, radius);
        int x = lx + (int)(cosf(a) * d), y = ly + (int)(sinf(a) * d);
        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || !IsCellWalkableAt(z, y, x)) continue;
        float score = ChaseRandRange(0.0f, 1.0f);
        if (GRID(x, y, z) == CELL_BUSH) score += 3.0f;
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                if ((ox || oy) && (IsLowCover(x + ox, y + oy, z) || !IsCellWalkableAt(z, y + oy, x + ox))) score += 1.0f;
        bool inView = false;   // a spot one of them can see right now isn't worth checking
        for (int q = 0; q < packN && !inView; q++) {
            const Mover* o = &movers[packHunters[q]];
            int ox = (int)(o->x / CELL_SIZE), oy = (int)(o->y / CELL_SIZE);
            float ddx = (float)(x - ox), ddy = (float)(y - oy);
            if (ddx * ddx + ddy * ddy < 6.0f * 6.0f && CanSeeCell(ox, oy, x, y, z) && !CoverBetween(ox, oy, x, y, z)) inView = true;
        }
        if (inView) score -= 4.0f;
        for (int q = 0; q < packN; q++) {   // spread out: not where another one is going
            if (q == k) continue;
            const Mover* o = &movers[packHunters[q]];
            float gx = (float)(o->goal.x - x), gy = (float)(o->goal.y - y);
            if (gx * gx + gy * gy < 9.0f) score -= 3.0f;
        }
        score -= sqrtf((movers[h].x / CELL_SIZE - x) * (movers[h].x / CELL_SIZE - x) + (movers[h].y / CELL_SIZE - y) * (movers[h].y / CELL_SIZE - y)) * 0.15f;
        if (score > best) { best = score; bestX = x; bestY = y; }
    }
    if (bestX >= 0) PathTo(h, bestX, bestY, z);
}

static void PackTick(float dt) {
    int tgt = PackTarget();
    if (tgt < 0 || packN == 0) return;
    Mover* T = &movers[tgt];
    packTime += dt;
    // caught?
    for (int k = 0; k < packN; k++) {
        const Mover* h = &movers[packHunters[k]];
        if ((h->x - T->x) * (h->x - T->x) + (h->y - T->y) * (h->y - T->y) > (CATCH_DIST * CELL_SIZE) * (CATCH_DIST * CELL_SIZE)) continue;
        packCatches++;
        packCatchSecondsSum += packTime;
        PackRespawn(tgt);
        return;
    }
    // what the pack knows: where one of them last saw it (packStealth off: always where it is)
    bool seen = !packStealth;
    for (int k = 0; k < packN && !seen; k++) seen = HunterSees(packHunters[k], tgt);
    if (seen) {
        float hx = 0.0f, hy = 0.0f;
        Heading(tgt, &hx, &hy);
        packLastX = T->x; packLastY = T->y; packLastHX = hx; packLastHY = hy;
        packLost = 0.0f;
        packSeenFor += dt;
    } else if (packSeenFor = 0.0f, (packLost += dt) > PACK_GIVE_UP) {   // got away
        packEscapes++;
        PackRespawn(tgt);
        return;
    }
    // the runner: flee from the pack when one is close (noticed, or very close), get tired;
    // out of their sight, duck behind cover and get its breath back
    bool threat = false;
    for (int k = 0; k < packN && !threat; k++) threat = ThreatClose(tgt, packHunters[k]);
    if (fleeing[tgt]) { stamina[tgt] -= STAMINA_DRAIN * dt; if (stamina[tgt] < 0.0f) stamina[tgt] = 0.0f; }
    else { stamina[tgt] += STAMINA_REST * dt; if (stamina[tgt] > 1.0f) stamina[tgt] = 1.0f; }
    if (packDucked) {
        bool near = false;
        for (int k = 0; k < packN && !near; k++) near = Dist(packHunters[k], tgt) < PACK_FLUSH * CELL_SIZE;
        if (packSeenFor >= GLIMPSE || near) {   // spotted (a glimpse isn't enough), or one right there: run again
            packDucked = false;
            packFlushed++;
            packTargetReplan = 0.0f;
        } else {
            fleeing[tgt] = T->pathIndex >= 0;   // running to the spot; there: still, resting
            if (T->pathIndex < 0) packDuckSecondsSum += dt;
        }
    }
    if (!packDucked) {
        static float duckTry = 0.0f;   // (a look round for cover twice a second)
        // out of their sight (they're hunting it): lie low behind cover. (Making for cover while
        // they watch doesn't work - by the time it's there they've moved round and see it, tried:
        // 0.3 s; nor only right after a chase - with four on it, it never breaks sight then.)
        bool chance = packLost >= GLIMPSE;
        if (packStealth && chance && (duckTry -= dt) <= 0.0f && (duckTry = 0.5f, RunnerDuck(tgt))) {
            packDucked = true;
            packDucks++;
        } else {
            if (threat) {
                packTargetReplan -= dt;
                if (!fleeing[tgt] || T->pathIndex < 0 || packTargetReplan <= 0.0f) {
                    FleeRunFrom(tgt, packHunters, packN);
                    packTargetReplan = CHASE_REPLAN;
                }
            }
            fleeing[tgt] = threat;
        }
    }
    if (!seen) {   // the hunters: lost it, search
        for (int k = 0; k < packN; k++) {
            packRole[k] = 3;
            packReplan[k] -= dt;
            if (movers[packHunters[k]].pathIndex >= 0 && packReplan[k] > 0.0f &&
                !(packLost <= dt * 1.5f)) continue;   // (just lost it: re-aim at once)
            packReplan[k] = 3.0f;
            HunterSearch(k);
        }
        return;
    }
    // where it's going: its goal if running, else away from the pack's middle
    float cx = 0.0f, cy = 0.0f;
    for (int k = 0; k < packN; k++) { cx += movers[packHunters[k]].x; cy += movers[packHunters[k]].y; }
    cx /= packN; cy /= packN;
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
    if (packTactics) PackAssignRoles(ex, ey, gx, gy);
    // the hunters
    int z = (int)T->z;
    for (int k = 0; k < packN; k++) {
        int h = packHunters[k];
        packReplan[k] -= dt;
        if (packReplan[k] > 0.0f && movers[h].pathIndex >= 0) continue;
        packReplan[k] = CHASE_REPLAN;
        float dx = T->x - movers[h].x, dy = T->y - movers[h].y;
        if (!packTactics || packRole[k] == 0 || dx * dx + dy * dy < (PACK_CLOSE * CELL_SIZE) * (PACK_CLOSE * CELL_SIZE)) {
            ChaseRun(h, tgt);
            continue;
        }
        float px, py;
        if (packRole[k] == 1) { px = gx; py = gy; }   // cut it off where it's going
        else {   // flank: ahead and to the side it's on
            float side = (movers[h].x - T->x) * (-ey) + (movers[h].y - T->y) * ex >= 0.0f ? 1.0f : -1.0f;
            px = T->x + ex * PACK_AHEAD * CELL_SIZE - ey * side * PACK_SIDE * CELL_SIZE;
            py = T->y + ey * PACK_AHEAD * CELL_SIZE + ex * side * PACK_SIDE * CELL_SIZE;
        }
        int ox, oy;
        if (!NearWalkable(px, py, z, &ox, &oy) || !PathTo(h, ox, oy, z)) ChaseRun(h, tgt);
    }
}

// ============================================================================
// Players come and go (playing is FREETIME_PLAY: no jobs, no talk)
// ============================================================================
static bool InAGame(int i) {
    return ChaseIt() >= 0 || hsPhase != HS_NONE || FollowSlot(i) >= 0 || PackSlot(i) >= 0 || i == PackTarget();
}

static int PlayerCount(void) {
    int n = 0;
    for (int i = 0; i < moverCount; i++) if (ChaseIsPlaying(i)) n++;
    return n;
}

// Someone leaves: the game carries on without it (a new "it", seeker or leader), or ends
static void GameDrop(int i) {
    if (tagTarget == i) tagTarget = -1;
    if (tagIt == i) {   // the nearest one still playing is "it" now
        int next = -1;
        float best = 1e18f;
        for (int j = 0; j < moverCount; j++) {
            if (!ChaseIsPlaying(j)) continue;
            float d = Dist(i, j);
            if (d < best) { best = d; next = j; }
        }
        if (next >= 0) ChaseSetIt(next); else tagIt = -1;
    }
    if (hsPhase != HS_NONE && i == hsSeeker) {   // a new round with someone else seeking
        int next = -1;
        for (int j = 0; j < moverCount && next < 0; j++) if (ChaseIsPlaying(j)) next = j;
        if (next >= 0) HideSeekStart(next); else hsPhase = HS_NONE;
    }
    int k = FollowSlot(i);
    if (k >= 0) {   // out of the line (the next one leads if it was the leader)
        for (int q = k; q < flCount - 1; q++) {
            flOrder[q] = flOrder[q + 1];
            memcpy(flTrailX[q], flTrailX[q + 1], sizeof(flTrailX[q]));
            memcpy(flTrailY[q], flTrailY[q + 1], sizeof(flTrailY[q]));
            flTrailHead[q] = flTrailHead[q + 1];
            flTrailLen[q] = flTrailLen[q + 1];
        }
        flCount--;
        if (k == 0) flTurn = FL_TURN_TIME;
    }
    int p = PackSlot(i);
    if (p >= 0) {
        for (int q = p; q < packN - 1; q++) {
            packHunters[q] = packHunters[q + 1];
            packRole[q] = packRole[q + 1];
            packReplan[q] = packReplan[q + 1];
        }
        packN--;
    }
    if (i == packTarget) { packTarget = -1; packN = 0; }   // the hunt is over (the hunters leave)
}

// Players whose state something else took over (a need, a job, drafting, a command: they are no
// longer FREETIME_PLAY) leave; a game too small to go on ends and its last players leave
static void PlaySweep(void) {
    for (int i = 0; i < moverCount; i++) {
        if (!playing[i]) continue;
        const Mover* m = &movers[i];
        if (!m->active || m->isDrafted || m->currentJobId >= 0 || m->freetimeState != FREETIME_PLAY) ChaseLeave(i);
    }
    int n = PlayerCount();
    if (ChaseIt() >= 0 && n < 2) tagIt = -1;
    if (hsPhase != HS_NONE && n < 2) hsPhase = HS_NONE;
    if (flCount > 0 && flCount < 2) flCount = 0;
    if (packTarget >= 0 && (packN == 0 || !ChaseIsPlaying(packTarget))) { packTarget = -1; packN = 0; }
    for (int i = 0; i < moverCount; i++)
        if (playing[i] && !InAGame(i)) ChaseLeave(i);
}

// A calm player (tag: not running, "it" with nobody in sight; the pack's runner, at ease)
// strolls about near the others, with a pause now and then
static void PlayWander(float dt) {
    bool tag = ChaseIt() >= 0;
    if (!tag && PackTarget() < 0) return;
    float cx = 0.0f, cy = 0.0f;
    int n = 0;
    for (int i = 0; i < moverCount; i++)
        if (ChaseIsPlaying(i)) { cx += movers[i].x; cy += movers[i].y; n++; }
    if (n == 0) return;
    cx /= n; cy /= n;
    for (int i = 0; i < moverCount; i++) {
        if (!ChaseIsPlaying(i) || fleeing[i]) continue;
        bool calm = tag ? (i != tagIt || (tagTarget < 0 && tagFreeze <= 0.0f)) : (i == packTarget && !packDucked);
        if (!calm || movers[i].pathIndex >= 0) continue;
        if ((wanderTimer[i] -= dt) > 0.0f) continue;
        wanderTimer[i] = ChaseRandRange(0.5f, 2.0f);
        Mover* m = &movers[i];
        int z = (int)m->z;
        for (int tries = 0; tries < 6; tries++) {   // somewhere 3-7 cells off, toward the others if far from them
            float a = ChaseRandRange(0.0f, 6.2831853f), d = ChaseRandRange(3.0f, 7.0f);
            float tx = m->x + cosf(a) * d * CELL_SIZE, ty = m->y + sinf(a) * d * CELL_SIZE;
            float fx = tx - cx, fy = ty - cy;
            if (fx * fx + fy * fy > (10.0f * CELL_SIZE) * (10.0f * CELL_SIZE)) { tx = (tx + cx) * 0.5f; ty = (ty + cy) * 0.5f; }
            if (PathTo(i, (int)(tx / CELL_SIZE), (int)(ty / CELL_SIZE), z)) break;
        }
    }
}
