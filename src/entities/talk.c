// talk.c - idle movers stop to talk (see talk.h)
#include "talk.h"
#include "notice.h"
#include "relations.h"
#include "../simulation/needs.h"
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
    talkHigh = 0;
    talkTickCounter = 0;
}

int MoverTalk(int i) {
    if (i < 0 || i >= moverCount) return -1;
    int t = moverTalkIdx[i] - 1;
    return (t >= 0 && talks[t].active) ? t : -1;
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

bool TalkLookAt(int i, float* x, float* y) {
    int t = MoverTalk(i);
    if (t < 0 || talks[t].focus == i) return false;   // the performer faces its audience (front)
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
        int before = tk->count;
        for (int k = tk->count - 1; k >= 0; k--)
            if (!CanStayInTalk(tk->members[k])) LeaveTalk(t, k);
        if (tk->count >= 2 && tk->count < before) ArrangeSlots(t);   // close the circle
        tk->age += dt;
        tk->timeLeft -= dt;
        if (tk->count < 2 || tk->timeLeft <= 0.0f || !talkEnabled) { EndTalk(t); continue; }
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
        if (!tk->active || tk->z != (int)m->z) continue;
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
