// talk.c - idle movers stop to talk (see talk.h)
#include "talk.h"
#include "notice.h"
#include "relations.h"
#include "chase.h"
#include "action_queue.h"
#include "furniture.h"
#include "workshops.h"
#include "jobs.h"
#include "../simulation/fire.h"
#include "../simulation/needs.h"
#include "../simulation/weather.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

Talk talks[MAX_TALKS];
bool talkEnabled = false;   // a per-game rule: games set it (on in survival, island, dollhouse, town)
float talkStartRadius = 3.0f;
float talkChance = 0.05f;
float talkMinSeconds = 6.0f;
float talkMaxSeconds = 15.0f;
float talkCooldown = 30.0f;
long talkStartedCount = 0;
long talkJoinedCount = 0;
bool talkGiveRoom = true;
float watchRadius = 6.0f;
float watchChance = 0.1f;
float watchMinSeconds = 10.0f;
float watchMaxSeconds = 30.0f;
long watchJoinedCount = 0;
float walkChance = 0.05f;
float walkMinLike = 30.0f;
float walkMaxSeconds = 60.0f;
float walkPace = 0.7f;
long walkStartedCount = 0;
long walkJoinedCount = 0;
bool spotByFriends = true;
bool autoGatherings = false;   // a game rule: the games switch it on
long autoGatheringsStarted = 0;
bool mingleEnabled = true;
float mingleChance = 0.02f;   // per check (~4 checks a second) once a better group is there
long mingleCount = 0;
static float groupTime[MAX_MOVERS];     // seconds in its current group (mingling: not before 6 s)
static int groupLast[MAX_MOVERS];       // the talk index + 1 groupTime counts for
static int mingleTarget[MAX_MOVERS];    // talk index + 1 it is walking over to, 0 = none
static float mingleGiveUp[MAX_MOVERS];
static int talkHigh = 0;   // talks[] entries at or above this are all free

static int moverTalkIdx[MAX_MOVERS];        // talk index + 1, 0 = none
static float moverTalkCooldown[MAX_MOVERS]; // sim seconds until it may talk again
static unsigned int talkTickCounter = 0;
static void ClearAutoGatherings(void);
static int dinerTable[MAX_MOVERS];          // table it eats at, -1 (rebuilt every tick)
static int tableSpeaker[MAX_FURNITURE];      // who talks at that table, -1
static float tableSpeakTimer[MAX_FURNITURE];
bool tableTalkEnabled = true;
long tableTalkTicks = 0;


#define TALK_CHECK_EVERY   15       // ticks between start checks per mover (staggered)
#define TALK_SLOT_HALF     0.75f    // cells from the middle to each speaker (pair: 1.5 apart)
#define TALK_TURN_MIN      1.5f     // seconds a speaker keeps the turn
#define TALK_TURN_MAX      4.0f
#define TALK_APPROACH      0.5f     // walking speed fraction while taking its spot
#define WATCH_APPROACH     0.8f     // onlookers walk a bit further to their spot
#define WATCH_RING0        1.5f     // cells from the focus to the first ring
#define WATCH_RING_STEP    1.0f     // cells between rings
#define WATCH_SPACING      1.0f     // cells between neighbours on a ring
#define WATCH_RINGS        3
#define WALK_ASK_RANGE     4.0f     // cells: how close a friend must be to suggest a walk
#define WALK_SIDE          0.8f     // cells: the follower's spot beside the leader
#define WALK_BEHIND        0.9f     // cells: ... or behind it where there's no room beside
#define WALK_APART         4.0f     // cells: further apart than this: the walk is off
#define WALK_CATCH_UP      1.15f    // the follower's speed (x the leader's) when behind its spot
#define WALK_AFTER_TALK    0.5f     // a talk between two who like each other ends: chance they walk on together
#define WALK_NEXT_LEG      8.0f     // seconds left on arrival: the leader picks the next spot
#define WALK_MAX           6        // a group outing: the leader and up to five others
#define WALK_LOOK_PERIOD   2.6f     // seconds: walking and talking, the head goes ahead / to the others
#define WALK_LOOK_SPEAK    0.9f     // of which the speaker looks at someone
#define WALK_LOOK_LISTEN   0.45f    // of which a listener glances at the speaker

static float RandRange(float lo, float hi) {
    return lo + (hi - lo) * (GetRandomValue(0, 10000) / 10000.0f);
}

void ClearTalks(void) {
    memset(talks, 0, sizeof(talks));
    memset(moverTalkIdx, 0, sizeof(moverTalkIdx));
    memset(moverTalkCooldown, 0, sizeof(moverTalkCooldown));
    talkStartedCount = 0;
    talkJoinedCount = 0;
    tableTalkTicks = 0;
    mingleCount = 0;
    autoGatheringsStarted = 0;
    ClearAutoGatherings();
    memset(groupTime, 0, sizeof(groupTime));
    memset(mingleTarget, 0, sizeof(mingleTarget));
    for (int i = 0; i < MAX_MOVERS; i++) dinerTable[i] = -1;
    for (int f = 0; f < MAX_FURNITURE; f++) tableSpeaker[f] = -1;
    watchJoinedCount = 0;
    walkStartedCount = 0;
    talkHigh = 0;
    talkTickCounter = 0;
}

int MoverTalk(int i) {
    if (i < 0 || i >= moverCount) return -1;
    int t = moverTalkIdx[i] - 1;
    return (t >= 0 && talks[t].active) ? t : -1;
}

bool MoverIsWalkingWith(int i) {
    int t = MoverTalk(i);
    return t >= 0 && talks[t].kind == TALK_WALK;
}

static bool WalkSlot(const Talk* tk, int k, float* tx, float* ty);

float TalkWalkSpeedScale(int i) {
    int t = MoverTalk(i);
    if (t < 0 || talks[t].kind != TALK_WALK || talks[t].members[0] != i || talks[t].count < 2) return 1.0f;
    float worst = 0.0f;   // the one furthest from its spot sets the pace
    for (int k = 1; k < talks[t].count; k++) {
        const Mover* f = &movers[talks[t].members[k]];
        float tx, ty;
        if (!WalkSlot(&talks[t], k, &tx, &ty)) { tx = movers[i].x; ty = movers[i].y; }
        float dx = f->x - tx, dy = f->y - ty;
        float d = sqrtf(dx * dx + dy * dy) / CELL_SIZE;
        if (d > worst) worst = d;
    }
    if (worst > 3.0f) return 0.2f;             // all but wait for them (creep: standing still reads as stuck)
    if (worst > 1.5f) return walkPace * 0.6f;  // slow down
    return walkPace;
}

bool MoverIsPerforming(int i) {
    int t = MoverTalk(i);
    return t >= 0 && talks[t].kind == TALK_WATCH && talks[t].focus == i;
}

bool MoverIsWatching(int i) {
    int t = MoverTalk(i);
    return t >= 0 && talks[t].kind == TALK_WATCH && talks[t].focus != i;
}

bool MoverIsSpeaking(int i) {
    int t = MoverTalk(i);
    if (t < 0) {   // eating at a table: its turn to talk?
        int tb = MoverDiningAt(i);
        return tb >= 0 && tableSpeaker[tb] == i;
    }
    if (talks[t].kind == TALK_WATCH && talks[t].focus >= 0) return talks[t].focus == i;   // the performer sings
    if (talks[t].kind == TALK_WATCH && talks[t].count < 2) return false;   // round a place: they chat
    return talks[t].members[talks[t].speaker] == i;
}

// Unit direction mover j walks in (to its next waypoint); false when not walking
// (skips waypoints it is already standing on: a fresh path starts with its own cell)
static bool WalkHeading(int j, float* hx, float* hy) {
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

bool TalkLookAt(int i, float* x, float* y) {
    int tb = MoverDiningAt(i);
    if (tb >= 0 && MoverTalk(i) < 0) {   // at a table: looks at it (and the others round it)
        *x = (furniture[tb].x + 0.5f) * CELL_SIZE;
        *y = (furniture[tb].y + 0.5f) * CELL_SIZE;
        return true;
    }
    int t = MoverTalk(i);
    if (t < 0 || talks[t].focus == i) return false;   // the performer faces its audience (front)
    if (talks[t].kind == TALK_WALK) {   // walking: mostly ahead, now and then a look at the other(s)
        const Talk* tk = &talks[t];
        if (tk->count < 2) return false;
        // a 2.6 s rhythm per mover (offset by index so they don't turn in step): the speaker looks
        // over for 0.9 s of it, a listener glances at the speaker for 0.45 s
        float ph = fmodf(tk->age + (float)i * 0.731f, WALK_LOOK_PERIOD);
        bool speaking = MoverIsSpeaking(i);
        if (!speaking && ph < WALK_LOOK_LISTEN) {
            int sp = tk->members[tk->speaker];
            if (sp != i) { *x = movers[sp].x; *y = movers[sp].y; return true; }
        }
        if (speaking && ph < WALK_LOOK_SPEAK) {   // the speaker looks at the nearest of the others
            int other = -1;
            float best = 1e18f;
            for (int k = 0; k < tk->count; k++) {
                int j = tk->members[k];
                if (j == i) continue;
                float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
                if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; other = j; }
            }
            if (other >= 0) { *x = movers[other].x; *y = movers[other].y; return true; }
        }
        if (tk->members[0] == i) return false;   // the leader: its own path
        float hx, hy;
        if (!WalkHeading(tk->members[0], &hx, &hy)) return false;
        *x = movers[i].x + hx * CELL_SIZE; *y = movers[i].y + hy * CELL_SIZE;
        return true;
    }
    *x = talks[t].centerX;
    *y = talks[t].centerY;
    return true;
}

int TalkPartner(int i) {
    int t = MoverTalk(i);
    if (t < 0) return -1;
    if (talks[t].kind == TALK_WATCH) return talks[t].focus != i ? talks[t].focus : -1;
    for (int k = 0; k < talks[t].count; k++)
        if (talks[t].members[k] != i) return talks[t].members[k];
    return -1;
}

// Free to stop and talk: on its feet, no job, no need being handled, not drafted or travelling
static bool CanTalk(int i) {
    const Mover* m = &movers[i];
    if (!m->active || m->currentJobId >= 0 || m->freetimeState != FREETIME_NONE) return false;
    if (m->isDrafted || m->transportState != TRANSPORT_NONE) return false;
    if (m->z - floorf(m->z) > 0.0001f) return false;   // on a ladder or ramp
    if (moverTalkIdx[i] || moverTalkCooldown[i] > 0.0f) return false;
    if (ChaseIsPlaying(i)) return false;   // playing tag
    if (QueueSuspended(i)) return false;   // a queue on hold for a need (looks idle, isn't)
    return true;
}

// Still free to stay in a talk (a job or a need takes it away)
static bool CanStayInTalk(int i) {
    const Mover* m = &movers[i];
    return m->active && m->currentJobId < 0 && m->freetimeState == FREETIME_NONE && !m->isDrafted &&
           m->transportState == TRANSPORT_NONE && !QueueSuspended(i);
}

static void StopWalking(int i) {
    movers[i].pathLength = 0;
    movers[i].pathIndex = -1;
    movers[i].needsRepath = false;
    moverRouteLen[i] = 0;
}

static void LeaveTalk(int t, int k) {
    Talk* tk = &talks[t];
    int i = tk->members[k];
    moverTalkIdx[i] = 0;
    moverTalkCooldown[i] = tk->kind == TALK_WATCH ? talkCooldown * 0.5f : talkCooldown;
    tk->count--;
    tk->members[k] = tk->members[tk->count];
    tk->slotX[k] = tk->slotX[tk->count];
    tk->slotY[k] = tk->slotY[tk->count];
    tk->spot[k] = tk->spot[tk->count];
    tk->stay[k] = tk->stay[tk->count];
    if (tk->speaker >= tk->count) tk->speaker = 0;
}

static void EndTalk(int t) {
    while (talks[t].count > 0) LeaveTalk(t, talks[t].count - 1);
    int f = talks[t].focus;
    if (f >= 0 && f < moverCount && moverTalkIdx[f] == t + 1) moverTalkIdx[f] = 0;
    talks[t].active = false;
}

// Members stand evenly on a circle round the middle, about 1.5 cells from their neighbours
// (a pair: 1.5 apart). Each keeps the side it was on: sorted by its current angle, spread out
// from the first one's angle. A spot that isn't walkable: it stands where it is.
static void ArrangeSlots(int t) {
    Talk* tk = &talks[t];
    int n = tk->count;
    if (n <= 0) return;
    float space = 0.0f;   // personal space: the members' pairs, averaged
    int pairs = 0;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++) { space += PersonalSpaceScale(tk->members[a], tk->members[b]); pairs++; }
    space = pairs ? space / pairs : 1.0f;
    float r = (n <= 2 ? TALK_SLOT_HALF : TALK_SLOT_HALF / sinf(3.14159265f / (float)n)) * space * CELL_SIZE;
    tk->radius = r;
    float ang[TALK_MAX_MEMBERS];
    int order[TALK_MAX_MEMBERS];
    for (int k = 0; k < n; k++) {
        const Mover* m = &movers[tk->members[k]];
        ang[k] = atan2f(m->y - tk->centerY, m->x - tk->centerX);
        order[k] = k;
    }
    for (int a = 1; a < n; a++)   // sort by angle (n <= 4)
        for (int b = a; b > 0 && ang[order[b]] < ang[order[b - 1]]; b--) {
            int tmp = order[b]; order[b] = order[b - 1]; order[b - 1] = tmp;
        }
    float start = ang[order[0]];
    for (int s = 0; s < n; s++) {
        int k = order[s];
        const Mover* m = &movers[tk->members[k]];
        float a = start + 6.2831853f * (float)s / (float)n;
        float x = tk->centerX + cosf(a) * r, y = tk->centerY + sinf(a) * r;
        int gx = (int)(x / CELL_SIZE), gy = (int)(y / CELL_SIZE);
        if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellStandableAt((int)m->z, gy, gx)) {
            x = m->x; y = m->y;
        }
        tk->slotX[k] = x;
        tk->slotY[k] = y;
    }
}

static bool Roll(float chance);

// --- Gatherings (TALK_WATCH) ---
static int WatchRingCap(int ring) {
    float r = WATCH_RING0 + WATCH_RING_STEP * ring;
    return (int)(6.2831853f * r / WATCH_SPACING);
}

static void WatchSpotPos(const Talk* tk, int spot, float* x, float* y) {
    int ring = spot / 64, k = spot % 64;
    float r = (WATCH_RING0 + WATCH_RING_STEP * ring) * CELL_SIZE;
    float a = 6.2831853f * (float)k / (float)WatchRingCap(ring);
    *x = tk->centerX + cosf(a) * r;
    *y = tk->centerY + sinf(a) * r;
}

static void WatchUpdateRadius(Talk* tk) {
    int outer = 0;
    for (int k = 0; k < tk->count; k++) if (tk->spot[k] / 64 > outer) outer = tk->spot[k] / 64;
    tk->radius = (tk->count ? WATCH_RING0 + WATCH_RING_STEP * outer : WATCH_RING0 * 0.5f) * CELL_SIZE;
}

// The free walkable spot on the innermost ring with room, nearest the angle it comes from; -1 if full
static int WatchPickSpot(const Talk* tk, const Mover* m) {
    float from = atan2f(m->y - tk->centerY, m->x - tk->centerX);
    for (int ring = 0; ring < WATCH_RINGS; ring++) {
        int cap = WatchRingCap(ring), best = -1;
        float bestD = 1e9f;
        for (int k = 0; k < cap; k++) {
            int spot = ring * 64 + k;
            bool taken = false;
            for (int q = 0; q < tk->count; q++) if (tk->spot[q] == spot) { taken = true; break; }
            if (taken) continue;
            float x, y;
            WatchSpotPos(tk, spot, &x, &y);
            int gx = (int)(x / CELL_SIZE), gy = (int)(y / CELL_SIZE);
            if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellStandableAt(tk->z, gy, gx)) continue;
            float ringR = WATCH_RING0 + WATCH_RING_STEP * ring;   // how far round the ring (cells) ...
            float d = fabsf(remainderf(6.2831853f * (float)k / (float)cap - from, 6.2831853f)) * ringR;
            d -= SpotPreference((int)(m - movers), x, y, tk->z);   // ... less: who'd be next to it
            if (d < bestD) { bestD = d; best = spot; }
        }
        if (best >= 0) return best;
    }
    return -1;
}

int GatheringStartAt(float x, float y, int z, float seconds, const char* what, float stayMin, float stayMax) {
    int t = -1;
    for (int k = 0; k < MAX_TALKS; k++) if (!talks[k].active) { t = k; break; }
    if (t < 0) return -1;
    Talk* tk = &talks[t];
    memset(tk, 0, sizeof(*tk));
    tk->active = true;
    tk->kind = TALK_WATCH;
    tk->focus = -1;
    tk->z = z;
    tk->centerX = x;
    tk->centerY = y;
    tk->timeLeft = seconds;
    tk->what = what;
    tk->stayMin = stayMin;
    tk->stayMax = stayMax;
    WatchUpdateRadius(tk);
    if (t + 1 > talkHigh) talkHigh = t + 1;
    return t;
}

int GatheringStart(int focus, float seconds) {
    if (focus < 0 || focus >= moverCount || !movers[focus].active || moverTalkIdx[focus]) return -1;
    int t = GatheringStartAt(movers[focus].x, movers[focus].y, (int)movers[focus].z, seconds, NULL, 0.0f, 0.0f);
    if (t < 0) return -1;
    talks[t].focus = focus;
    moverTalkIdx[focus] = t + 1;
    StopWalking(focus);
    return t;
}

static void WatchJoin(int t, int i) {
    Talk* tk = &talks[t];
    if (tk->count >= TALK_SLOTS || moverTalkIdx[i]) return;
    int spot = WatchPickSpot(tk, &movers[i]);
    if (spot < 0) return;
    int k = tk->count++;
    tk->members[k] = i;
    tk->spot[k] = spot;
    tk->stay[k] = tk->stayMax > 0.0f ? RandRange(tk->stayMin, tk->stayMax) : RandRange(watchMinSeconds, watchMaxSeconds);
    WatchSpotPos(tk, spot, &tk->slotX[k], &tk->slotY[k]);
    WatchUpdateRadius(tk);
    moverTalkIdx[i] = t + 1;
    StopWalking(i);
    watchJoinedCount++;
}

// Can m see the middle of a gathering? (the middle may be a blocked cell, a burning barrel:
// then a free cell next to it in sight will do)
static bool CanSeeGathering(const Mover* m, const Talk* tk) {
    int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE);
    int cx = (int)(tk->centerX / CELL_SIZE), cy = (int)(tk->centerY / CELL_SIZE);
    if (IsCellStandableAt(tk->z, cy, cx)) return CanSeeCell(mx, my, cx, cy, tk->z);
    static const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++)
        if (IsCellStandableAt(tk->z, cy + ny[d], cx + nx[d]) && CanSeeCell(mx, my, cx + nx[d], cy + ny[d], tk->z)) return true;
    return false;
}

// An idle mover in range of a gathering it can see may come to watch
static bool TryWatch(int i) {
    const Mover* m = &movers[i];
    bool dark = IsSkyDark();
    for (int t = 0; t < talkHigh; t++) {
        Talk* tk = &talks[t];
        if (!tk->active || tk->kind != TALK_WATCH || tk->z != (int)m->z || tk->count >= TALK_SLOTS) continue;
        // A place (a campfire) pulls harder after dark: further and more often
        bool nightDraw = dark && tk->focus < 0;
        float reach = watchRadius * CELL_SIZE * (nightDraw ? 1.5f : 1.0f);
        float chance = watchChance * (nightDraw ? 3.0f : 1.0f);
        float dx = tk->centerX - m->x, dy = tk->centerY - m->y;
        if (dx * dx + dy * dy > reach * reach) continue;
        if (!CanSeeGathering(m, tk)) continue;
        bool foe = false;   // someone it can't stand is there (or is the focus): not now
        for (int k = 0; k < tk->count && !foe; k++) foe = RelationLike(i, tk->members[k]) <= -20.0f;
        if (tk->focus >= 0 && RelationLike(i, tk->focus) <= -20.0f) foe = true;
        if (foe) continue;
        if (GetRandomValue(0, 9999) < (int)(chance * 10000.0f)) WatchJoin(t, i);
        return true;   // one roll per check
    }
    return false;
}

// The leader's next spot, 6-14 cells away, with a path to it; false if none found
static bool WalkPickLeg(int leader) {
    Mover* m = &movers[leader];
    int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE), z = (int)m->z;
    for (int tries = 0; tries < 12; tries++) {
        float a = RandRange(0.0f, 6.2831853f), d = RandRange(6.0f, 14.0f);
        int gx = cx + (int)(cosf(a) * d), gy = cy + (int)(sinf(a) * d);
        if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellStandableAt(z, gy, gx)) continue;
        if (!IsExposedToSky(gx, gy, z)) continue;   // a walk goes outdoors
        Point goal = {gx, gy, z};
        m->goal = goal;
        if (MoverPlanPath(leader, (Point){cx, cy, z}, goal) > 0) { m->needsRepath = false; return true; }
    }
    return false;
}

bool TalkWalkStart(int leader, int follower) {
    if (moverTalkIdx[leader] || moverTalkIdx[follower]) return false;
    if (!WalkPickLeg(leader)) return false;
    int z = (int)movers[leader].z;
    int t = -1;
    for (int k = 0; k < MAX_TALKS; k++) if (!talks[k].active) { t = k; break; }
    if (t < 0) return false;
    Talk* tk = &talks[t];
    memset(tk, 0, sizeof(*tk));
    tk->active = true;
    tk->kind = TALK_WALK;
    tk->focus = -1;
    tk->z = z;
    tk->members[0] = leader;
    tk->members[1] = follower;
    tk->count = 2;
    tk->timeLeft = walkMaxSeconds;
    tk->speaker = GetRandomValue(0, 1);
    tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
    moverTalkIdx[leader] = moverTalkIdx[follower] = t + 1;
    StopWalking(follower);
    if (t + 1 > talkHigh) talkHigh = t + 1;
    walkStartedCount++;
    return true;
}

// Join a group walking by (room permitting)
static bool WalkJoin(int t, int i) {
    Talk* tk = &talks[t];
    if (!tk->active || tk->kind != TALK_WALK || tk->count >= WALK_MAX || moverTalkIdx[i]) return false;
    tk->members[tk->count++] = i;
    moverTalkIdx[i] = t + 1;
    StopWalking(i);
    walkJoinedCount++;
    return true;
}

// An idle mover that noticed an idle one it likes close by may suggest a walk
static bool TryWalk(int i) {
    if (!IsExposedToSky((int)(movers[i].x / CELL_SIZE), (int)(movers[i].y / CELL_SIZE), (int)movers[i].z))
        return false;   // you go for a walk outdoors
    const MoverNotice* n = &moverNotice[i];
    float r = WALK_ASK_RANGE * CELL_SIZE;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || (int)movers[j].z != (int)movers[i].z) continue;
        if (RelationLike(i, j) < walkMinLike || RelationLike(j, i) <= -20.0f) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > r * r) continue;
        int t = MoverTalk(j);
        if (t >= 0 && talks[t].kind == TALK_WALK) {   // a friend's group walking by: join it?
            if (talks[t].count >= WALK_MAX) continue;
            bool foe = false;
            for (int q = 0; q < talks[t].count; q++)
                if (RelationLike(i, talks[t].members[q]) <= -20.0f || RelationLike(talks[t].members[q], i) <= -20.0f) foe = true;
            if (foe) continue;
            if (!Roll(walkChance)) return false;
            return WalkJoin(t, i);
        }
        if (!CanTalk(j)) continue;
        if (!Roll(walkChance)) return false;   // one roll per check
        return TalkWalkStart(i, j);
    }
    return false;
}

void TalkStart(int a, int b) {
    int t = -1;
    for (int k = 0; k < MAX_TALKS; k++) if (!talks[k].active) { t = k; break; }
    if (t < 0) return;
    Talk* tk = &talks[t];
    memset(tk, 0, sizeof(*tk));
    tk->active = true;
    tk->focus = -1;
    tk->z = (int)movers[a].z;
    tk->centerX = (movers[a].x + movers[b].x) * 0.5f;
    tk->centerY = (movers[a].y + movers[b].y) * 0.5f;
    tk->members[0] = a;
    tk->members[1] = b;
    tk->count = 2;
    ArrangeSlots(t);
    tk->timeLeft = RandRange(talkMinSeconds, talkMaxSeconds);
    tk->speaker = GetRandomValue(0, 1);
    tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
    moverTalkIdx[a] = moverTalkIdx[b] = t + 1;
    StopWalking(a);
    StopWalking(b);
    if (t + 1 > talkHigh) talkHigh = t + 1;
    talkStartedCount++;
}

void TalkJoin(int t, int i) {
    Talk* tk = &talks[t];
    if (!tk->active || tk->kind != TALK_CHAT || tk->count >= TALK_MAX_MEMBERS || moverTalkIdx[i]) return;
    tk->members[tk->count++] = i;
    moverTalkIdx[i] = t + 1;
    StopWalking(i);
    ArrangeSlots(t);
    float more = RandRange(talkMinSeconds, talkMaxSeconds) * 0.5f;   // a newcomer keeps it going
    if (tk->timeLeft < more) tk->timeLeft = more;
    talkJoinedCount++;
}

// How much more (or less) likely a talk between i and j is: people you know and like, more;
// someone either of them dislikes: never (0)
static float TalkAffinity(int i, int j) {
    float likeIJ = RelationLike(i, j), likeJI = RelationLike(j, i);
    if (likeIJ <= -20.0f || likeJI <= -20.0f) return 0.0f;
    float f = (0.5f + RelationFamiliarity(i, j) / 100.0f) * (1.0f + likeIJ / 50.0f);
    return f < 0.0f ? 0.0f : (f > 3.0f ? 3.0f : f);
}

static bool Roll(float chance) {
    return GetRandomValue(0, 9999) < (int)(chance * 10000.0f);
}

static void TryStartTalk(int i) {
    if (!CanTalk(i)) return;
    const MoverNotice* n = &moverNotice[i];
    float r = talkStartRadius * CELL_SIZE;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || (int)movers[j].z != (int)movers[i].z) continue;
        int t = MoverTalk(j);
        if (t >= 0 && talks[t].kind != TALK_CHAT) continue;
        if (t >= 0) {   // j is in a talk: join it if there's room and its middle is close
            if (talks[t].count >= TALK_MAX_MEMBERS) continue;
            float cx = talks[t].centerX - movers[i].x, cy = talks[t].centerY - movers[i].y;
            float reach = r + talks[t].radius;
            if (cx * cx + cy * cy > reach * reach) continue;
            float aff = 0.0f;   // joining: how it gets on with the ones already there
            bool refuse = false;
            for (int q = 0; q < talks[t].count; q++) {
                float a = TalkAffinity(i, talks[t].members[q]);
                if (a <= 0.0f) refuse = true;
                aff += a;
            }
            if (refuse) continue;
            if (!Roll(talkChance * aff / (float)talks[t].count)) return;
            TalkJoin(t, i);
            return;
        }
        if (!CanTalk(j)) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > r * r) continue;
        float aff = TalkAffinity(i, j);
        if (aff <= 0.0f) continue;
        if (!Roll(talkChance * aff)) return;   // one roll per check
        TalkStart(i, j);
        return;
    }
}

static void MingleTick(float dt);
static bool TryMingle(int i);
static int BestOtherGroup(int i, float* appeal);
static void MingleTo(int i, int t);

void TalkTick(float dt) {
    TableTalkTick(dt);
    AutoGatherTick(dt);
    MingleTick(dt);
    for (int i = 0; i < moverCount; i++)
        if (moverTalkCooldown[i] > 0.0f) moverTalkCooldown[i] -= dt;

    int high = 0;
    for (int t = 0; t < talkHigh; t++) {
        Talk* tk = &talks[t];
        if (!tk->active) continue;
        high = t + 1;
        if (tk->kind == TALK_WATCH) {
            int f = tk->focus;   // a performer (follows it) or -1: a place (stays put)
            bool focusGone = f >= 0 && (f >= moverCount || !CanStayInTalk(f));
            if (focusGone || (tk->timeLeft -= dt) <= 0.0f) { EndTalk(t); continue; }
            if (f >= 0) {
                tk->centerX = movers[f].x;
                tk->centerY = movers[f].y;
            }
            tk->age += dt;
            for (int k = tk->count - 1; k >= 0; k--) {
                tk->stay[k] -= dt;
                if (tk->stay[k] <= 0.0f || !CanStayInTalk(tk->members[k])) LeaveTalk(t, k);
            }
            WatchUpdateRadius(tk);
            if (f < 0 && tk->count >= 2) {   // round a place (a campfire): they chat, turn by turn
                if (tk->speaker >= tk->count) tk->speaker = 0;
                tk->speakTimer -= dt;
                if (tk->speakTimer <= 0.0f) {
                    tk->speaker = (tk->speaker + 1 + GetRandomValue(0, tk->count - 2)) % tk->count;
                    tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
                }
                // ... and get to know each other, at half a conversation's rate (the circle can be big)
                for (int a = 0; a < tk->count && tk->count <= 16; a++)
                    for (int b = 0; b < tk->count; b++) {
                        if (a == b) continue;
                        int ma = tk->members[a];
                        float like = relTalkLikePerSec * (movers[ma].mood + 0.3f) * dt * 0.5f;
                        RelationAdd(ma, tk->members[b], like, relTalkFamiliarityPerSec * dt * 0.5f);
                    }
            }
            continue;
        }
        if (tk->kind == TALK_WALK) {
            int lead = tk->members[0];
            // a follower that has to go, is on another level or got left behind: drops out alone
            for (int k = tk->count - 1; k >= 1; k--) {
                int f = tk->members[k];
                float dx = movers[f].x - movers[lead].x, dy = movers[f].y - movers[lead].y;
                if (!CanStayInTalk(f) || (int)movers[f].z != (int)movers[lead].z ||
                    dx * dx + dy * dy > (WALK_APART * CELL_SIZE) * (WALK_APART * CELL_SIZE))
                    LeaveTalk(t, k);
            }
            bool over = tk->count < 2 || !CanStayInTalk(lead) || (tk->timeLeft -= dt) <= 0.0f || !talkEnabled;
            if (!over && movers[lead].pathIndex < 0)   // arrived: on to the next spot, or home
                over = tk->timeLeft < WALK_NEXT_LEG || !WalkPickLeg(lead);
            if (over) { EndTalk(t); continue; }
            tk->age += dt;
            tk->centerX = tk->centerY = 0.0f;
            for (int k = 0; k < tk->count; k++) { tk->centerX += movers[tk->members[k]].x; tk->centerY += movers[tk->members[k]].y; }
            tk->centerX /= tk->count; tk->centerY /= tk->count;
            tk->radius = 0.0f;   // passers-by don't steer round a walking group (polite passing does)
            if (tk->speaker >= tk->count) tk->speaker = 0;
            tk->speakTimer -= dt;
            if (tk->speakTimer <= 0.0f) {
                tk->speaker = (tk->speaker + 1 + GetRandomValue(0, tk->count - 2)) % tk->count;
                tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
            }
            for (int a = 0; a < tk->count; a++)
                for (int b = 0; b < tk->count; b++) {
                    if (a == b) continue;
                    int ma = tk->members[a];
                    float like = relTalkLikePerSec * (movers[ma].mood + 0.3f) * dt;
                    RelationAdd(ma, tk->members[b], like, relTalkFamiliarityPerSec * dt);
                }
            continue;
        }
        int before = tk->count;
        for (int k = tk->count - 1; k >= 0; k--)
            if (!CanStayInTalk(tk->members[k])) LeaveTalk(t, k);
        if (tk->count >= 2 && tk->count < before) ArrangeSlots(t);   // close the circle
        tk->age += dt;
        tk->timeLeft -= dt;
        if (tk->count < 2 || tk->timeLeft <= 0.0f || !talkEnabled) {
            // A talk among people who all like each other ends: "shall we walk?" (all of them)
            int who[TALK_MAX_MEMBERS], n = tk->count;
            for (int k = 0; k < n; k++) who[k] = tk->members[k];
            bool walkOn = n >= 2 && tk->timeLeft <= 0.0f && talkEnabled &&
                          IsExposedToSky((int)(movers[who[0]].x / CELL_SIZE), (int)(movers[who[0]].y / CELL_SIZE), (int)movers[who[0]].z);
            for (int a = 0; a < n && walkOn; a++)
                for (int b = 0; b < n && walkOn; b++)
                    if (a != b && RelationLike(who[a], who[b]) < walkMinLike) walkOn = false;
            walkOn = walkOn && Roll(WALK_AFTER_TALK);
            EndTalk(t);
            if (walkOn && TalkWalkStart(who[0], who[1]))
                for (int k = 2; k < n; k++) WalkJoin(MoverTalk(who[0]), who[k]);
            else if (mingleEnabled)   // not walking on together: anyone with a friend in another group goes over
                for (int k = 0; k < n; k++) {
                    float a;
                    int g = BestOtherGroup(who[k], &a);
                    if (g >= 0 && a >= 20.0f) MingleTo(who[k], g);
                }
            continue;
        }
        // Talking: they get to know each other; like follows the mood (happy: up, grumpy: down)
        for (int a = 0; a < tk->count; a++)
            for (int b = 0; b < tk->count; b++) {
                if (a == b) continue;
                int ma = tk->members[a];
                float like = relTalkLikePerSec * (movers[ma].mood + 0.3f) * dt;
                RelationAdd(ma, tk->members[b], like, relTalkFamiliarityPerSec * dt);
            }
        tk->speakTimer -= dt;
        if (tk->speakTimer <= 0.0f) {
            tk->speaker = (tk->speaker + 1 + GetRandomValue(0, tk->count - 2)) % tk->count;
            tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
        }
    }

    talkHigh = high;

    if (!noticeEnabled) return;
    unsigned int phase = talkTickCounter++ % TALK_CHECK_EVERY;
    for (int i = (int)phase; i < moverCount; i += TALK_CHECK_EVERY) {
        if (TryMingle(i)) continue;              // in a group: move over to a better one?
        if (mingleTarget[i]) continue;           // on its way to one
        if (!CanTalk(i)) continue;
        if (talkHigh > 0 && TryWatch(i)) continue;   // something worth watching in range comes first
        if (talkEnabled && TryWalk(i)) continue;    // a walk with a friend
        if (talkEnabled) TryStartTalk(i);
    }
}

bool TalkMoverTick(int i, float dt) {
    int t = MoverTalk(i);
    if (t < 0) return false;
    Talk* tk = &talks[t];
    Mover* m = &movers[i];
    if (tk->focus == i) {   // the performer stands where it is
        m->timeWithoutProgress = 0.0f;
        m->lastX = m->x;
        m->lastY = m->y;
        return true;
    }
    if (tk->kind == TALK_WALK) {
        if (tk->members[0] == i) return false;   // the leader walks its own path (at TalkWalkSpeedScale)
        int k = 1;
        while (k < tk->count && tk->members[k] != i) k++;
        float tx, ty;
        if (k >= tk->count || !WalkSlot(tk, k, &tx, &ty)) {   // no direction yet: stay put
            m->timeWithoutProgress = 0.0f;
            m->lastX = m->x;
            m->lastY = m->y;
            return true;
        }
        int z = (int)m->z;
        float dx = tx - m->x, dy = ty - m->y, d = sqrtf(dx * dx + dy * dy);
        if (d > 1.0f) {
            float step = m->speed * (60.0f / dayLength) * walkPace * WALK_CATCH_UP * dt;
            if (step > d) step = d;
            float nx = m->x + dx / d * step, ny = m->y + dy / d * step;
            if (IsCellNudgeableAt(z, (int)(ny / CELL_SIZE), (int)(nx / CELL_SIZE))) { m->x = nx; m->y = ny; }
            else if (IsCellNudgeableAt(z, (int)(m->y / CELL_SIZE), (int)(nx / CELL_SIZE))) m->x = nx;   // slide
            else if (IsCellNudgeableAt(z, (int)(ny / CELL_SIZE), (int)(m->x / CELL_SIZE))) m->y = ny;
        }
        m->timeWithoutProgress = 0.0f;
        m->lastX = m->x;
        m->lastY = m->y;
        return true;
    }
    int k = 0;
    while (k < tk->count && tk->members[k] != i) k++;
    if (k == tk->count) { moverTalkIdx[i] = 0; return false; }
    float dx = tk->slotX[k] - m->x, dy = tk->slotY[k] - m->y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 1.0f) {
        float step = m->speed * (60.0f / dayLength) * (tk->kind == TALK_WATCH ? WATCH_APPROACH : TALK_APPROACH) * dt;
        if (step > d) step = d;
        float nx = m->x + dx / d * step, ny = m->y + dy / d * step;
        if (IsCellNudgeableAt((int)m->z, (int)(ny / CELL_SIZE), (int)(nx / CELL_SIZE))) { m->x = nx; m->y = ny; }
    }
    m->timeWithoutProgress = 0.0f;   // standing in a conversation is not being stuck
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}

// --- Passers-by give a conversation room ---
#define ROOM_LOOK     3.0f    // cells beyond the circle where a walker starts to steer round it
#define ROOM_MARGIN   0.6f    // cells: how wide a berth beyond the members' spots
#define ROOM_CENTRED  0.15f   // cells: closer to my line than this = dead ahead: go right

bool TalkGiveRoomSteer(int i, float hx, float hy, float* sideX, float* sideY) {
    *sideX = *sideY = 0.0f;
    if (!talkGiveRoom || talkHigh == 0 || MoverTalk(i) >= 0) return false;
    const Mover* m = &movers[i];
    float rx = -hy, ry = hx;   // my right
    float sx = 0.0f, sy = 0.0f;
    bool any = false;
    for (int t = 0; t < talkHigh; t++) {
        const Talk* tk = &talks[t];
        if (!tk->active || tk->z != (int)m->z || tk->kind == TALK_WALK) continue;   // a walking pair: passing handles it
        float dx = tk->centerX - m->x, dy = tk->centerY - m->y;
        float berth = tk->radius + ROOM_MARGIN * CELL_SIZE;
        float look = berth + ROOM_LOOK * CELL_SIZE;
        float ahead = dx * hx + dy * hy;
        if (ahead <= 0.0f || ahead > look) continue;
        float lateral = dx * rx + dy * ry;          // > 0: the middle is on my right
        if (fabsf(lateral) > berth) continue;       // my line already clears it
        float w = 1.0f - ahead / look;
        w *= 1.0f - fabsf(lateral) / berth * 0.5f;  // more when heading straight at it
        float side = lateral > ROOM_CENTRED * CELL_SIZE ? -1.0f : 1.0f;
        sx += side * rx * w;
        sy += side * ry * w;
        any = true;
    }
    *sideX = sx;
    *sideY = sy;
    return any;
}

// --- Table talk ---
int MoverDiningAt(int i) {
    return (i >= 0 && i < moverCount) ? dinerTable[i] : -1;
}

// The table next to seat f (a flat surface in the 8 cells round it), -1 if none
static int TableNextTo(int f) {
    for (int k = 0; k < MAX_FURNITURE; k++) {
        if (!furniture[k].active || furniture[k].z != furniture[f].z) continue;
        if (!(GetFurnitureDef(furniture[k].type)->capabilities & CAP_FLAT_SURFACE)) continue;
        if (abs(furniture[k].x - furniture[f].x) <= 1 && abs(furniture[k].y - furniture[f].y) <= 1) return k;
    }
    return -1;
}

void TableTalkTick(float dt) {
    // who eats seated at a table right now
    int tables[64], tableCount = 0;
    for (int i = 0; i < moverCount; i++) {
        dinerTable[i] = -1;
        if (!tableTalkEnabled || !movers[i].active || !QueueBusy(i)) continue;
        const MoverQueue* q = &moverQueues[i];
        if (q->seat < 0 || q->current >= q->count) continue;
        int step = q->steps[q->current].type;
        if (step != ACT_EAT && step != ACT_LINGER) continue;   // eating, or staying on while a tablemate eats
        const Furniture* seat = &furniture[q->seat];
        float sx = (seat->x + 0.5f) * CELL_SIZE - movers[i].x, sy = (seat->y + 0.5f) * CELL_SIZE - movers[i].y;
        if (sx * sx + sy * sy > (CELL_SIZE * 0.6f) * (CELL_SIZE * 0.6f)) continue;   // eating, but not in the seat
        int tb = TableNextTo(q->seat);
        if (tb < 0) continue;
        dinerTable[i] = tb;
        bool seen = false;
        for (int k = 0; k < tableCount; k++) if (tables[k] == tb) seen = true;
        if (!seen && tableCount < 64) tables[tableCount++] = tb;
    }
    // at each table with two or more: turns, and they get to know each other
    for (int k = 0; k < tableCount; k++) {
        int tb = tables[k], diners[16], n = 0;
        for (int i = 0; i < moverCount && n < 16; i++) if (dinerTable[i] == tb) diners[n++] = i;
        if (n < 2) { tableSpeaker[tb] = -1; continue; }
        tableTalkTicks += n;
        bool speakerHere = false;
        for (int d = 0; d < n; d++) if (diners[d] == tableSpeaker[tb]) speakerHere = true;
        tableSpeakTimer[tb] -= dt;
        if (!speakerHere || tableSpeakTimer[tb] <= 0.0f) {
            int pick = diners[GetRandomValue(0, n - 1)];
            if (pick == tableSpeaker[tb]) pick = diners[(GetRandomValue(1, n - 1) + 0) % n];
            tableSpeaker[tb] = pick;
            tableSpeakTimer[tb] = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
        }
        for (int a = 0; a < n; a++)
            for (int b = 0; b < n; b++) {
                if (a == b) continue;
                float like = relTalkLikePerSec * (movers[diners[a]].mood + 0.3f) * dt * 0.5f;
                RelationAdd(diners[a], diners[b], like, relTalkFamiliarityPerSec * dt * 0.5f);
            }
    }
}

bool TalkWalkJoin(int t, int moverIdx) { return WalkJoin(t, moverIdx); }

bool TalkWalkSlot(int t, int k, float* x, float* y) {
    if (t < 0 || t >= MAX_TALKS || !talks[t].active || talks[t].kind != TALK_WALK || k < 1 || k >= talks[t].count) return false;
    return WalkSlot(&talks[t], k, x, y);
}

// Where follower k of a walking group should be: a spot beside / behind the leader (in the
// leader's frame: forward, right), or, where that isn't walkable (a door, a narrow hall), just
// behind the one ahead of it in the group. False while the leader has no direction yet.
static bool WalkSlot(const Talk* tk, int k, float* tx, float* ty) {
    static const float slotFwd[WALK_MAX]   = { 0.0f, 0.0f, -0.9f, -0.9f, -1.8f, -1.8f };
    static const float slotRight[WALK_MAX] = { 0.0f, 0.8f,  0.0f,  0.8f,  0.0f,  0.8f };
    float hx, hy;
    if (!WalkHeading(tk->members[0], &hx, &hy)) return false;
    const Mover* L = &movers[tk->members[0]];
    float rx = -hy, ry = hx;
    float right = slotRight[k];   // beside the one on its left: further if they're not close (never
    if (right > 0.0f) {           // nearer: 0.8 cells is as close as two can walk without overlapping)
        float space = PersonalSpaceScale(tk->members[k], tk->members[k - 1]);
        if (space > 1.0f) right *= space;
    }
    float x = L->x + (hx * slotFwd[k] + rx * right) * CELL_SIZE;
    float y = L->y + (hy * slotFwd[k] + ry * right) * CELL_SIZE;
    int z = (int)L->z;
    if (!IsCellNudgeableAt(z, (int)(y / CELL_SIZE), (int)(x / CELL_SIZE))) {   // single file
        const Mover* ahead = &movers[tk->members[k - 1]];
        x = ahead->x - hx * WALK_BEHIND * CELL_SIZE;
        y = ahead->y - hy * WALK_BEHIND * CELL_SIZE;
    }
    *tx = x;
    *ty = y;
    return true;
}

// --- Choosing a spot by who's next to it ---
#define SPOT_NEAR 1.6f   // cells: who counts as next to a spot

static float NeighbourBonus(int i, int j) {
    float like = RelationLike(i, j);
    if (like <= -20.0f) return like / 10.0f;   // -4 at -40
    if (like >= 20.0f) return like / 25.0f;    // +0.8 .. +4
    return RelationFamiliarity(i, j) < 15.0f ? -0.5f : 0.3f;
}

float SpotPreference(int i, float x, float y, int z) {
    if (!spotByFriends || i < 0 || i >= moverCount) return 0.0f;
    float near = SPOT_NEAR * CELL_SIZE, bonus = 0.0f;
    int counted[32], nc = 0;
    // who will sit next to it: the occupants of seats round the spot
    for (int f = 0; f < MAX_FURNITURE; f++) {
        if (!furniture[f].active || furniture[f].z != z || furniture[f].occupant < 0 || furniture[f].occupant == i) continue;
        float fx = (furniture[f].x + 0.5f) * CELL_SIZE - x, fy = (furniture[f].y + 0.5f) * CELL_SIZE - y;
        if (fx * fx + fy * fy > near * near || fx * fx + fy * fy < 1.0f) continue;   // (not the spot itself)
        int j = furniture[f].occupant;
        if (j >= moverCount || !movers[j].active) continue;
        bonus += NeighbourBonus(i, j);
        if (nc < 32) counted[nc++] = j;
    }
    // ... and who stands there
    for (int j = 0; j < moverCount; j++) {
        if (j == i || !movers[j].active || (int)movers[j].z != z) continue;
        float dx = movers[j].x - x, dy = movers[j].y - y;
        if (dx * dx + dy * dy > near * near) continue;
        bool done = false;
        for (int k = 0; k < nc; k++) if (counted[k] == j) done = true;
        if (!done) bonus += NeighbourBonus(i, j);
    }
    return bonus;
}

float SeatPreference(int i, int f) {
    if (f < 0 || f >= MAX_FURNITURE || !furniture[f].active) return 0.0f;
    return SpotPreference(i, (furniture[f].x + 0.5f) * CELL_SIZE, (furniture[f].y + 0.5f) * CELL_SIZE, furniture[f].z);
}

// --- Mingling ---
#define MINGLE_MIN_TIME   6.0f    // seconds in a group before it may move on
#define MINGLE_RANGE     10.0f    // cells
#define MINGLE_BETTER    15.0f    // how much more it must like the other group
#define MINGLE_PLACE     10.0f    // a place (campfire) is worth this of its own
#define MINGLE_GIVE_UP   15.0f    // seconds to get there

bool MoverIsMingling(int i) { return i >= 0 && i < moverCount && mingleTarget[i] > 0; }

// How much i would like to be in talk t: the one it likes most there (a place: at least
// MINGLE_PLACE); -1000 if someone it dislikes is there or it can't join
static float GroupAppeal(int i, int t) {
    const Talk* tk = &talks[t];
    if (!tk->active || tk->kind == TALK_WALK || tk->z != (int)movers[i].z) return -1000.0f;
    if (tk->kind == TALK_CHAT && tk->count >= TALK_MAX_MEMBERS && MoverTalk(i) != t) return -1000.0f;
    if (tk->kind == TALK_WATCH && tk->focus >= 0) return -1000.0f;   // (a performer: watching, not mingling)
    float best = tk->kind == TALK_WATCH ? MINGLE_PLACE : -100.0f;
    for (int k = 0; k < tk->count; k++) {
        int j = tk->members[k];
        if (j == i) continue;
        float like = RelationLike(i, j);
        if (like <= -20.0f) return -1000.0f;
        if (like > best) best = like;
    }
    return best;
}

// The most appealing other group within range, or -1
static int BestOtherGroup(int i, float* appeal) {
    int best = -1, cur = MoverTalk(i);
    float bestA = -1000.0f, r = MINGLE_RANGE * CELL_SIZE;
    for (int t = 0; t < talkHigh; t++) {
        if (t == cur || !talks[t].active) continue;
        float dx = talks[t].centerX - movers[i].x, dy = talks[t].centerY - movers[i].y;
        if (dx * dx + dy * dy > r * r) continue;
        float a = GroupAppeal(i, t);
        if (a > bestA) { bestA = a; best = t; }
    }
    *appeal = bestA;
    return best;
}

// Leave for group t: walk over (joining happens on arrival, MingleTick)
static void MingleTo(int i, int t) {
    Mover* m = &movers[i];
    Point start = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
    Point goal = {(int)(talks[t].centerX / CELL_SIZE), (int)(talks[t].centerY / CELL_SIZE), talks[t].z};
    if (!IsCellStandableAt(goal.z, goal.y, goal.x)) goal = (Point){(int)(talks[t].slotX[0] / CELL_SIZE), (int)(talks[t].slotY[0] / CELL_SIZE), talks[t].z};
    m->goal = goal;
    if (MoverPlanPath(i, start, goal) <= 0) return;
    m->needsRepath = false;
    mingleTarget[i] = t + 1;
    mingleGiveUp[i] = MINGLE_GIVE_UP;
    moverTalkCooldown[i] = 0.0f;
    mingleCount++;
}

// Every tick: time in groups, minglers arriving (or giving up)
static void MingleTick(float dt) {
    for (int i = 0; i < moverCount; i++) {
        int t = MoverTalk(i);
        if (t + 1 != groupLast[i]) { groupLast[i] = t + 1; groupTime[i] = 0.0f; }
        else groupTime[i] += dt;
        if (!mingleTarget[i]) continue;
        int g = mingleTarget[i] - 1;
        mingleGiveUp[i] -= dt;
        if (t >= 0 || !movers[i].active || !talks[g].active || GroupAppeal(i, g) <= -1000.0f || mingleGiveUp[i] <= 0.0f ||
            movers[i].currentJobId >= 0 || movers[i].freetimeState != FREETIME_NONE) {
            mingleTarget[i] = 0;
            continue;
        }
        float dx = talks[g].centerX - movers[i].x, dy = talks[g].centerY - movers[i].y;
        float reach = talks[g].radius + 1.5f * CELL_SIZE;
        if (dx * dx + dy * dy > reach * reach) continue;
        mingleTarget[i] = 0;
        if (talks[g].kind == TALK_CHAT) TalkJoin(g, i);
        else WatchJoin(g, i);
    }
}

// In a group a while: a clearly better one in range? (one roll per check)
static bool TryMingle(int i) {
    int t = MoverTalk(i);
    if (!mingleEnabled || t < 0 || groupTime[i] < MINGLE_MIN_TIME || mingleTarget[i]) return false;
    if (talks[t].kind == TALK_WALK || talks[t].focus == i) return false;
    if (talks[t].kind == TALK_WATCH && talks[t].focus >= 0) return false;
    float here = GroupAppeal(i, t), there;
    int g = BestOtherGroup(i, &there);
    if (g < 0 || there < here + MINGLE_BETTER) return false;
    if (!Roll(mingleChance)) return false;
    for (int k = 0; k < talks[t].count; k++)
        if (talks[t].members[k] == i) { LeaveTalk(t, k); break; }
    if (talks[t].kind == TALK_CHAT && talks[t].count >= 2) ArrangeSlots(t);
    if (talks[t].kind == TALK_WATCH) WatchUpdateRadius(&talks[t]);
    MingleTo(i, g);
    return true;
}

// --- Gatherings the world starts by itself ---
enum { AG_FIRE, AG_HEAT, AG_COOKING };
typedef struct { int kind, idx, talk; bool seen; } AutoGather;
#define AG_MAX 128
static AutoGather autoGather[AG_MAX];
static int autoGatherCount = 0;
static float autoGatherTimer = 0.0f;

static void ClearAutoGatherings(void) { autoGatherCount = 0; autoGatherTimer = 0.0f; }

static bool IsFireWorkshop(WorkshopType t) {
    return t == WORKSHOP_CAMPFIRE || t == WORKSHOP_GROUND_FIRE || t == WORKSHOP_HEARTH;
}

// Burning: fire on its fuel tile (a lit fire source), or its passive (fuelled) timer running
static bool AgWorkshopBurning(const Workshop* ws) {
    if (ws->fuelTileX >= 0 && (HasFire(ws->fuelTileX, ws->fuelTileY, ws->z) || IsFireSourceAt(ws->fuelTileX, ws->fuelTileY, ws->z)))
        return true;
    return ws->passiveBillIdx >= 0 && ws->passiveProgress > 0.0f;
}

// Someone at work at it (a craft job, at the working step): returns who, or -1
static int WorkshopCook(int w) {
    int c = workshops[w].assignedCrafter;
    if (c < 0 || c >= moverCount || !movers[c].active || movers[c].currentJobId < 0) return -1;
    const Job* job = GetJob(movers[c].currentJobId);
    if (!job || job->type != JOBTYPE_CRAFT || job->targetWorkshop != w || job->step != CRAFT_STEP_WORKING) return -1;
    return c;
}

// A place gathering already within a cell of (x, y)? (a scenario's own, or another source's)
static bool GatheringNear(float x, float y, int z) {
    for (int t = 0; t < talkHigh; t++) {
        const Talk* tk = &talks[t];
        if (!tk->active || tk->kind != TALK_WATCH || tk->focus >= 0 || tk->z != z) continue;
        float dx = tk->centerX - x, dy = tk->centerY - y;
        if (dx * dx + dy * dy < (float)(CELL_SIZE * CELL_SIZE)) return true;
    }
    return false;
}

static void AutoGatherEnsure(int kind, int idx, float x, float y, int z, const char* what, float stayMin, float stayMax) {
    for (int k = 0; k < autoGatherCount; k++) {
        AutoGather* g = &autoGather[k];
        if (g->kind != kind || g->idx != idx) continue;
        if (talks[g->talk].active && talks[g->talk].kind == TALK_WATCH) { g->seen = true; return; }
        *g = autoGather[--autoGatherCount];   // its gathering is gone: start again below
        break;
    }
    if (autoGatherCount >= AG_MAX || GatheringNear(x, y, z)) return;
    int t = GatheringStartAt(x, y, z, 1e9f, what, stayMin, stayMax);
    if (t < 0) return;
    autoGather[autoGatherCount++] = (AutoGather){ kind, idx, t, true };
    autoGatheringsStarted++;
}

void AutoGatherTick(float dt) {
    if ((autoGatherTimer -= dt) > 0.0f) return;
    autoGatherTimer = 1.0f;
    for (int k = 0; k < autoGatherCount; k++) autoGather[k].seen = false;
    if (autoGatherings && noticeEnabled) {
        for (int w = 0; w < MAX_WORKSHOPS; w++) {
            const Workshop* ws = &workshops[w];
            if (!ws->active) continue;
            if (IsFireWorkshop(ws->type) && AgWorkshopBurning(ws)) {
                int fx = ws->fuelTileX >= 0 ? ws->fuelTileX : ws->x, fy = ws->fuelTileX >= 0 ? ws->fuelTileY : ws->y;
                AutoGatherEnsure(AG_FIRE, w, (fx + 0.5f) * CELL_SIZE, (fy + 0.5f) * CELL_SIZE, ws->z, "the fire", 30.0f, 90.0f);
                continue;
            }
            if (IsFireWorkshop(ws->type) || ws->type == WORKSHOP_STOVE) {
                int cook = WorkshopCook(w);
                if (cook >= 0)
                    AutoGatherEnsure(AG_COOKING, w, movers[cook].x, movers[cook].y, (int)movers[cook].z, "the cooking", 10.0f, 30.0f);
            }
        }
        for (int f = 0; f < MAX_FURNITURE; f++) {
            if (!furniture[f].active || !(GetFurnitureDef(furniture[f].type)->capabilities & CAP_HEAT)) continue;
            if (!FurnitureLit(f)) continue;   // a barrel gone out draws nobody
            AutoGatherEnsure(AG_HEAT, f, (furniture[f].x + 0.5f) * CELL_SIZE, (furniture[f].y + 0.5f) * CELL_SIZE,
                             furniture[f].z, "the warmth", 30.0f, 90.0f);
        }
    }
    for (int k = autoGatherCount - 1; k >= 0; k--) {   // sources gone out: their gatherings end
        if (autoGather[k].seen) continue;
        if (talks[autoGather[k].talk].active) EndTalk(autoGather[k].talk);
        autoGather[k] = autoGather[--autoGatherCount];
    }
}
