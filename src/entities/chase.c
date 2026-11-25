// chase.c - chasing and fleeing, and a game of tag (see chase.h)
#include "chase.h"
#include "notice.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
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
}

void ChaseSetPlayer(int i, bool on) { if (i >= 0 && i < MAX_MOVERS) { playing[i] = on; stamina[i] = 1.0f; } }
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

void ChaseTick(float dt) {
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
static bool FollowControls(int i);
static bool PackControls(int i);
bool HideControls(int i) { return (hsPhase != HS_NONE && ChaseIsPlaying(i)) || FollowControls(i) || PackControls(i); }
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
            if (CanSeeCell(hsHome.x, hsHome.y, cx, cy, z)) score -= 20.0f;   // seen from home: bad
            int walls = 0;   // tucked in
            for (int oy = -1; oy <= 1; oy++)
                for (int ox = -1; ox <= 1; ox++)
                    if ((ox || oy) && !IsCellWalkableAt(z, cy + oy, cx + ox)) walls++;
            score += walls * 1.5f;
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
            if (CanSeeCell(sx, sy, x, y, z)) hsSeen[y * hsSeenW + x] = 1;
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
static bool FollowControls(int i) { return flCount > 0 && FollowSlot(i) >= 0; }

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
        playing[players[k]] = true;
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
long packCatches = 0;
double packCatchSecondsSum = 0.0;

#define PACK_MAX       8
#define PACK_CLOSE     1.5f    // cells: everyone just goes straight at it
#define PACK_AHEAD     3.0f    // cells: flank points ahead of the runner ...
#define PACK_SIDE      2.5f    // ... and to the side
#define PACK_CUT_AHEAD 5.0f    // cells: where it's going, when it has no goal yet
#define PACK_RESPAWN   10.0f   // cells from every hunter

static int packHunters[PACK_MAX], packN = 0, packTarget = -1;
static int packRole[PACK_MAX];
static float packReplan[PACK_MAX], packTime = 0.0f, packTargetReplan = 0.0f;

static void ClearPack(void) { packN = 0; packTarget = -1; packCatches = 0; packCatchSecondsSum = 0.0; }
int PackTarget(void) { return packTarget >= 0 && packTarget < moverCount && movers[packTarget].active ? packTarget : -1; }

static int PackSlot(int i) {
    for (int k = 0; k < packN; k++) if (packHunters[k] == i) return k;
    return -1;
}
int PackRole(int i) { int k = PackSlot(i); return k >= 0 && PackTarget() >= 0 ? packRole[k] : -1; }
static bool PackControls(int i) { return PackTarget() >= 0 && PackSlot(i) >= 0; }   // (the runner wanders when calm)

static float PackSpeedScale(int i) {
    if (PackTarget() < 0) return 0.0f;
    if (PackSlot(i) >= 0) return packSpeed;
    if (i == packTarget) return fleeing[i] ? (stamina[i] > TIRED ? fleeSpeed : TIRED_SPEED) : 1.0f;
    return 0.0f;
}

void PackHuntStart(const int* hunters, int n, int target) {
    packN = n > PACK_MAX ? PACK_MAX : n;
    for (int k = 0; k < packN; k++) {
        packHunters[k] = hunters[k];
        packRole[k] = 0;
        packReplan[k] = 0.1f * k;   // staggered
        playing[hunters[k]] = true;
    }
    packTarget = target;
    playing[target] = true;
    stamina[target] = 1.0f;
    fleeing[target] = false;
    packTime = 0.0f;
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
        packTime = 0.0f;
        for (int tries = 0; tries < 60; tries++) {   // the runner turns up again far from the pack
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
        return;
    }
    // the runner: flee from the pack when one is close (noticed, or very close), get tired
    bool threat = false;
    for (int k = 0; k < packN && !threat; k++) threat = ThreatClose(tgt, packHunters[k]);
    if (fleeing[tgt]) { stamina[tgt] -= STAMINA_DRAIN * dt; if (stamina[tgt] < 0.0f) stamina[tgt] = 0.0f; }
    else { stamina[tgt] += STAMINA_REST * dt; if (stamina[tgt] > 1.0f) stamina[tgt] = 1.0f; }
    if (threat) {
        packTargetReplan -= dt;
        if (!fleeing[tgt] || T->pathIndex < 0 || packTargetReplan <= 0.0f) {
            FleeRunFrom(tgt, packHunters, packN);
            packTargetReplan = CHASE_REPLAN;
        }
    }
    fleeing[tgt] = threat;
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
