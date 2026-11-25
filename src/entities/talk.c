// talk.c - idle movers stop to talk (see talk.h)
#include "talk.h"
#include "notice.h"
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

static int moverTalkIdx[MAX_MOVERS];        // talk index + 1, 0 = none
static float moverTalkCooldown[MAX_MOVERS]; // sim seconds until it may talk again
static unsigned int talkTickCounter = 0;

#define TALK_CHECK_EVERY   15       // ticks between start checks per mover (staggered)
#define TALK_SLOT_HALF     0.75f    // cells from the middle to each speaker (pair: 1.5 apart)
#define TALK_TURN_MIN      1.5f     // seconds a speaker keeps the turn
#define TALK_TURN_MAX      4.0f
#define TALK_APPROACH      0.5f     // walking speed fraction while taking its spot

static float RandRange(float lo, float hi) {
    return lo + (hi - lo) * (GetRandomValue(0, 10000) / 10000.0f);
}

void ClearTalks(void) {
    memset(talks, 0, sizeof(talks));
    memset(moverTalkIdx, 0, sizeof(moverTalkIdx));
    memset(moverTalkCooldown, 0, sizeof(moverTalkCooldown));
    talkStartedCount = 0;
    talkTickCounter = 0;
}

int MoverTalk(int i) {
    if (i < 0 || i >= moverCount) return -1;
    int t = moverTalkIdx[i] - 1;
    return (t >= 0 && talks[t].active) ? t : -1;
}

bool MoverIsSpeaking(int i) {
    int t = MoverTalk(i);
    return t >= 0 && talks[t].members[talks[t].speaker] == i;
}

bool TalkLookAt(int i, float* x, float* y) {
    int t = MoverTalk(i);
    if (t < 0) return false;
    *x = talks[t].centerX;
    *y = talks[t].centerY;
    return true;
}

int TalkPartner(int i) {
    int t = MoverTalk(i);
    if (t < 0) return -1;
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
    moverTalkCooldown[i] = talkCooldown;
    tk->count--;
    tk->members[k] = tk->members[tk->count];
    tk->slotX[k] = tk->slotX[tk->count];
    tk->slotY[k] = tk->slotY[tk->count];
    if (tk->speaker >= tk->count) tk->speaker = 0;
}

static void EndTalk(int t) {
    while (talks[t].count > 0) LeaveTalk(t, talks[t].count - 1);
    talks[t].active = false;
}

// Spot for a member: dist cells from the middle toward (ux, uy); where it stands if not walkable
static void PickSlot(const Mover* m, float cx, float cy, float ux, float uy, float* sx, float* sy) {
    float x = cx + ux * TALK_SLOT_HALF * CELL_SIZE, y = cy + uy * TALK_SLOT_HALF * CELL_SIZE;
    int gx = (int)(x / CELL_SIZE), gy = (int)(y / CELL_SIZE);
    if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight || !IsCellWalkableAt((int)m->z, gy, gx)) {
        x = m->x; y = m->y;
    }
    *sx = x; *sy = y;
}

void TalkStart(int a, int b) {
    int t = -1;
    for (int k = 0; k < MAX_TALKS; k++) if (!talks[k].active) { t = k; break; }
    if (t < 0) return;
    Talk* tk = &talks[t];
    memset(tk, 0, sizeof(*tk));
    tk->active = true;
    tk->z = (int)movers[a].z;
    tk->centerX = (movers[a].x + movers[b].x) * 0.5f;
    tk->centerY = (movers[a].y + movers[b].y) * 0.5f;
    float dx = movers[b].x - movers[a].x, dy = movers[b].y - movers[a].y;
    float len = sqrtf(dx * dx + dy * dy);
    float ux = len > 0.5f ? dx / len : 1.0f, uy = len > 0.5f ? dy / len : 0.0f;
    tk->members[0] = a;
    tk->members[1] = b;
    tk->count = 2;
    PickSlot(&movers[a], tk->centerX, tk->centerY, -ux, -uy, &tk->slotX[0], &tk->slotY[0]);
    PickSlot(&movers[b], tk->centerX, tk->centerY, ux, uy, &tk->slotX[1], &tk->slotY[1]);
    tk->timeLeft = RandRange(talkMinSeconds, talkMaxSeconds);
    tk->speaker = GetRandomValue(0, 1);
    tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
    moverTalkIdx[a] = moverTalkIdx[b] = t + 1;
    StopWalking(a);
    StopWalking(b);
    talkStartedCount++;
}

static void TryStartTalk(int i) {
    if (!CanTalk(i)) return;
    const MoverNotice* n = &moverNotice[i];
    float r = talkStartRadius * CELL_SIZE;
    for (int k = 0; k < n->count; k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !CanTalk(j) || (int)movers[j].z != (int)movers[i].z) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > r * r) continue;
        if (GetRandomValue(0, 9999) >= (int)(talkChance * 10000.0f)) return;   // one roll per check
        TalkStart(i, j);
        return;
    }
}

void TalkTick(float dt) {
    for (int i = 0; i < moverCount; i++)
        if (moverTalkCooldown[i] > 0.0f) moverTalkCooldown[i] -= dt;

    for (int t = 0; t < MAX_TALKS; t++) {
        Talk* tk = &talks[t];
        if (!tk->active) continue;
        for (int k = tk->count - 1; k >= 0; k--)
            if (!CanStayInTalk(tk->members[k])) LeaveTalk(t, k);
        tk->age += dt;
        tk->timeLeft -= dt;
        if (tk->count < 2 || tk->timeLeft <= 0.0f || !talkEnabled) { EndTalk(t); continue; }
        tk->speakTimer -= dt;
        if (tk->speakTimer <= 0.0f) {
            tk->speaker = (tk->speaker + 1 + GetRandomValue(0, tk->count - 2)) % tk->count;
            tk->speakTimer = RandRange(TALK_TURN_MIN, TALK_TURN_MAX);
        }
    }

    if (!talkEnabled || !noticeEnabled) return;
    unsigned int phase = talkTickCounter++ % TALK_CHECK_EVERY;
    for (int i = (int)phase; i < moverCount; i += TALK_CHECK_EVERY) TryStartTalk(i);
}

bool TalkMoverTick(int i, float dt) {
    int t = MoverTalk(i);
    if (t < 0) return false;
    Talk* tk = &talks[t];
    int k = 0;
    while (k < tk->count && tk->members[k] != i) k++;
    if (k == tk->count) { moverTalkIdx[i] = 0; return false; }
    Mover* m = &movers[i];
    float dx = tk->slotX[k] - m->x, dy = tk->slotY[k] - m->y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 1.0f) {
        float step = m->speed * (60.0f / dayLength) * TALK_APPROACH * dt;
        if (step > d) step = d;
        float nx = m->x + dx / d * step, ny = m->y + dy / d * step;
        if (IsCellWalkableAt((int)m->z, (int)(ny / CELL_SIZE), (int)(nx / CELL_SIZE))) { m->x = nx; m->y = ny; }
    }
    m->timeWithoutProgress = 0.0f;   // standing in a conversation is not being stuck
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}
