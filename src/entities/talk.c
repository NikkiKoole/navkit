// talk.c - idle movers stop to talk (see talk.h)
#include "talk.h"
#include "notice.h"
#include "relations.h"
#include "../simulation/needs.h"
#include "../simulation/weather.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <math.h>
#include <string.h>

Talk talks[MAX_TALKS];
bool talkEnabled = false;   // a per-game rule: games set it (dollhouse on)
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
static int talkHigh = 0;   // talks[] entries at or above this are all free

static int moverTalkIdx[MAX_MOVERS];        // talk index + 1, 0 = none
static float moverTalkCooldown[MAX_MOVERS]; // sim seconds until it may talk again
static unsigned int talkTickCounter = 0;

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

static float RandRange(float lo, float hi) {
    return lo + (hi - lo) * (GetRandomValue(0, 10000) / 10000.0f);
}

void ClearTalks(void) {
    memset(talks, 0, sizeof(talks));
    memset(moverTalkIdx, 0, sizeof(moverTalkIdx));
    memset(moverTalkCooldown, 0, sizeof(moverTalkCooldown));
    talkStartedCount = 0;
    talkJoinedCount = 0;
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

float TalkWalkSpeedScale(int i) {
    int t = MoverTalk(i);
    if (t < 0 || talks[t].kind != TALK_WALK || talks[t].members[0] != i || talks[t].count < 2) return 1.0f;
    const Mover* f = &movers[talks[t].members[1]];
    float dx = f->x - movers[i].x, dy = f->y - movers[i].y;
    float d = sqrtf(dx * dx + dy * dy) / CELL_SIZE;
    if (d > 3.0f) return 0.2f;                 // all but wait for them (creep: standing still reads as stuck)
    if (d > 1.5f) return walkPace * 0.6f;      // slow down
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
    if (t < 0) return false;
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
    int t = MoverTalk(i);
    if (t < 0 || talks[t].focus == i) return false;   // the performer faces its audience (front)
    if (talks[t].kind == TALK_WALK) {   // walking: the speaker looks at the other, else ahead
        const Talk* tk = &talks[t];
        if (tk->count < 2) return false;
        int other = tk->members[0] == i ? tk->members[1] : tk->members[0];
        if (MoverIsSpeaking(i)) { *x = movers[other].x; *y = movers[other].y; return true; }
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
    return true;
}

// Still free to stay in a talk (a job or a need takes it away)
static bool CanStayInTalk(int i) {
    const Mover* m = &movers[i];
    return m->active && m->currentJobId < 0 && m->freetimeState == FREETIME_NONE && !m->isDrafted &&
           m->transportState == TRANSPORT_NONE;
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
    float r = (n <= 2 ? TALK_SLOT_HALF : TALK_SLOT_HALF / sinf(3.14159265f / (float)n)) * CELL_SIZE;
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
        if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellWalkableAt((int)m->z, gy, gx)) {
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
            if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellWalkableAt(tk->z, gy, gx)) continue;
            float d = fabsf(remainderf(6.2831853f * (float)k / (float)cap - from, 6.2831853f));
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
        if (!HasLineOfSight((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE),
                            (int)(tk->centerX / CELL_SIZE), (int)(tk->centerY / CELL_SIZE), tk->z)) continue;
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
        if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellWalkableAt(z, gy, gx)) continue;
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

// An idle mover that noticed an idle one it likes close by may suggest a walk
static bool TryWalk(int i) {
    if (!IsExposedToSky((int)(movers[i].x / CELL_SIZE), (int)(movers[i].y / CELL_SIZE), (int)movers[i].z))
        return false;   // you go for a walk outdoors
    const MoverNotice* n = &moverNotice[i];
    float r = WALK_ASK_RANGE * CELL_SIZE;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !CanTalk(j) || (int)movers[j].z != (int)movers[i].z) continue;
        if (RelationLike(i, j) < walkMinLike || RelationLike(j, i) <= -20.0f) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > r * r) continue;
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

void TalkTick(float dt) {
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
            int lead = tk->members[0], fol = tk->count > 1 ? tk->members[1] : -1;
            bool over = fol < 0 || !CanStayInTalk(lead) || !CanStayInTalk(fol) || (tk->timeLeft -= dt) <= 0.0f ||
                        !talkEnabled || (int)movers[lead].z != (int)movers[fol].z;
            if (!over && movers[lead].pathIndex < 0)   // arrived: on to the next spot, or home
                over = tk->timeLeft < WALK_NEXT_LEG || !WalkPickLeg(lead);
            if (!over) {
                float dx = movers[fol].x - movers[lead].x, dy = movers[fol].y - movers[lead].y;
                over = dx * dx + dy * dy > (WALK_APART * CELL_SIZE) * (WALK_APART * CELL_SIZE);
            }
            if (over) { EndTalk(t); continue; }
            tk->age += dt;
            tk->centerX = (movers[lead].x + movers[fol].x) * 0.5f;
            tk->centerY = (movers[lead].y + movers[fol].y) * 0.5f;
            tk->radius = 0.0f;   // passers-by don't steer round a walking pair (polite passing does)
            tk->speakTimer -= dt;
            if (tk->speakTimer <= 0.0f) {
                tk->speaker = 1 - tk->speaker;
                tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
            }
            for (int a = 0; a < 2; a++) {
                int ma = tk->members[a], mb = tk->members[1 - a];
                float like = relTalkLikePerSec * (movers[ma].mood + 0.3f) * dt;
                RelationAdd(ma, mb, like, relTalkFamiliarityPerSec * dt);
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
            int a = tk->members[0], b = tk->count > 1 ? tk->members[1] : -1;
            bool walkOn = tk->count == 2 && tk->timeLeft <= 0.0f && talkEnabled &&
                          IsExposedToSky((int)(movers[a].x / CELL_SIZE), (int)(movers[a].y / CELL_SIZE), (int)movers[a].z) &&
                          RelationLike(a, b) >= walkMinLike && RelationLike(b, a) >= walkMinLike && Roll(WALK_AFTER_TALK);
            EndTalk(t);
            if (walkOn) TalkWalkStart(a, b);   // "shall we walk?"
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
        int lead = tk->members[0];
        float hx, hy;
        const Mover* L = &movers[lead];
        if (!WalkHeading(lead, &hx, &hy)) {   // no direction yet: stay put rather than walk into it
            m->timeWithoutProgress = 0.0f;
            m->lastX = m->x;
            m->lastY = m->y;
            return true;
        }
        // its spot: at the leader's right, else behind it (a door, a narrow hall)
        float tx = L->x - hy * WALK_SIDE * CELL_SIZE, ty = L->y + hx * WALK_SIDE * CELL_SIZE;
        int z = (int)m->z;
        if (!IsCellWalkableAt(z, (int)(ty / CELL_SIZE), (int)(tx / CELL_SIZE))) {
            tx = L->x - hx * WALK_BEHIND * CELL_SIZE;
            ty = L->y - hy * WALK_BEHIND * CELL_SIZE;
        }
        float dx = tx - m->x, dy = ty - m->y, d = sqrtf(dx * dx + dy * dy);
        if (d > 1.0f) {
            float step = m->speed * (60.0f / dayLength) * walkPace * WALK_CATCH_UP * dt;
            if (step > d) step = d;
            float nx = m->x + dx / d * step, ny = m->y + dy / d * step;
            if (IsCellWalkableAt(z, (int)(ny / CELL_SIZE), (int)(nx / CELL_SIZE))) { m->x = nx; m->y = ny; }
            else if (IsCellWalkableAt(z, (int)(m->y / CELL_SIZE), (int)(nx / CELL_SIZE))) m->x = nx;   // slide
            else if (IsCellWalkableAt(z, (int)(ny / CELL_SIZE), (int)(m->x / CELL_SIZE))) m->y = ny;
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
        if (IsCellWalkableAt((int)m->z, (int)(ny / CELL_SIZE), (int)(nx / CELL_SIZE))) { m->x = nx; m->y = ny; }
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
