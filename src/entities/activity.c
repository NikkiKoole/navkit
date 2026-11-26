// activity.c - joint activities: the pool, membership, the sweep (see activity.h)
#include "activity.h"
#include "visit.h"
#include "chase.h"
#include "mover.h"
#include "action_queue.h"
#include "notice.h"
#include "talk.h"
#include "relations.h"
#include "social.h"
#include "../core/event_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Activity activities[MAX_ACTIVITIES];
long playLeftCount = 0;
static int moverAct[MAX_MOVERS];       // activity index + 1, 0 = none
static bool moverWaiting[MAX_MOVERS];  // joined, no activity yet
static float playCooldown[MAX_MOVERS]; // seconds before it may play again (after leaving a game)

bool autoGames = true;          // a game rule: the games set it (scenarios: off unless they ask)
float gameChance = 0.005f;      // per check with a friend (like 50) near by: ~ one game a minute of idling together
float gameMinSeconds = 40.0f, gameMaxSeconds = 90.0f;
float gameCooldown = 120.0f;
float gameJoinChance = 0.02f;   // per check, seeing friends play near by: joins within a few seconds
long autoGameStarts[ACT_KIND_COUNT], autoGameDeclined = 0, autoGameEnded = 0, autoGameJoins = 0;
long autoGamePlayersSum = 0;
double autoGameSecondsSum = 0.0;

#define PROPOSE_EVERY  15       // ticks between checks per mover (staggered by index)
#define PROPOSE_RANGE  6.0f     // cells: who it may ask
#define PROPOSE_MAX    6        // players in a proposed game, the one who proposed it included
#define JOIN_RANGE     8.0f     // cells: a game this close (one of its players) can be joined
#define JOIN_LAST      15.0f    // seconds: a game with less left isn't worth joining

// The kinds (each defined with its logic: the games in chase.c)
const ActivityDef* ActivityDefOf(int kind) {
    switch (kind) {
        case ACT_TAG:           return &tagActivityDef;
        case ACT_HIDE_SEEK:     return &hideSeekActivityDef;
        case ACT_FOLLOW_LEADER: return &followLeaderActivityDef;
        case ACT_PACK_HUNT:     return &packHuntActivityDef;
        case ACT_VISIT:         return &visitActivityDef;
        default:                return NULL;
    }
}

static void ActFree(int a) {
    Activity* A = &activities[a];
    if (A->timeLeft > 0.0f) {
        autoGameEnded++; autoGameSecondsSum += A->age;
        EventLog("Game over: %s after %.0f s (%d players left)", ActivityDefOf(A->kind)->name, A->age, A->count);
    }
    const ActivityDef* d = ActivityDefOf(A->kind);
    if (d && d->end) d->end(a);
    free(A->state);
    A->state = NULL;
    A->active = false;
}

void ClearActivities(void) {
    for (int a = 0; a < MAX_ACTIVITIES; a++) if (activities[a].active) ActFree(a);
    memset(activities, 0, sizeof(activities));
    memset(moverAct, 0, sizeof(moverAct));
    memset(moverWaiting, 0, sizeof(moverWaiting));
    memset(playCooldown, 0, sizeof(playCooldown));
    playLeftCount = 0;
    memset(autoGameStarts, 0, sizeof(autoGameStarts));
    autoGameDeclined = autoGameEnded = autoGameJoins = autoGamePlayersSum = 0;
    autoGameSecondsSum = 0.0;
    for (int i = 0; i < moverCount; i++)   // nobody is playing any more
        if (movers[i].freetimeState == FREETIME_PLAY) movers[i].freetimeState = FREETIME_NONE;
}

bool ActivityMoverFree(int i) {
    if (i < 0 || i >= moverCount) return false;
    const Mover* m = &movers[i];
    return m->active && m->currentJobId < 0 && !m->isDrafted && !QueueSuspended(i) &&
           (m->freetimeState == FREETIME_NONE || m->freetimeState == FREETIME_PLAY);
}

int MoverActivity(int i) {
    if (i < 0 || i >= MAX_MOVERS || moverAct[i] == 0) return -1;
    return moverAct[i] - 1;
}
bool ActivityWaiting(int i) { return i >= 0 && i < MAX_MOVERS && moverWaiting[i]; }
bool MoverInActivity(int i) {
    return i >= 0 && i < moverCount && (moverAct[i] || moverWaiting[i]) && movers[i].active;
}
bool ActivityHas(int a, int i) { return a >= 0 && MoverInActivity(i) && moverAct[i] == a + 1; }
void* ActivityState(int a) { return (a >= 0 && a < MAX_ACTIVITIES) ? activities[a].state : NULL; }

int ActivityFirst(int kind) {
    for (int a = 0; a < MAX_ACTIVITIES; a++)
        if (activities[a].active && activities[a].kind == kind) return a;
    return -1;
}

int ActivityStart(int kind) {
    const ActivityDef* d = ActivityDefOf(kind);
    if (!d) return -1;
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        Activity* A = &activities[a];
        if (A->active) continue;
        // a slot whose activity ended may still have members the sweep hasn't freed yet
        bool stale = false;
        for (int i = 0; i < moverCount && !stale; i++) stale = moverAct[i] == a + 1;
        if (stale) continue;
        memset(A, 0, sizeof(*A));
        A->active = true;
        A->kind = kind;
        A->state = calloc(1, d->stateSize ? d->stateSize : 1);
        return a;
    }
    return -1;
}

// Out of its activity (or the wait) without counting as leaving or giving up its state
static void ActDetach(int i) {
    int a = moverAct[i] - 1;
    moverAct[i] = 0;
    moverWaiting[i] = false;
    if (a < 0 || !activities[a].active) return;
    activities[a].count--;
    const ActivityDef* d = ActivityDefOf(activities[a].kind);
    if (d && d->memberLeft) d->memberLeft(a, i);
}

static void ActJoined(int i) {
    movers[i].freetimeState = FREETIME_PLAY;
}

bool ActivityWait(int i) {
    if (!ActivityMoverFree(i)) return false;
    if (MoverInActivity(i)) return true;   // already playing (or waiting): stays where it is
    ChaseMoverReset(i);
    moverWaiting[i] = true;
    ActJoined(i);
    return true;
}

bool ActivityAdd(int a, int i) {
    if (a < 0 || a >= MAX_ACTIVITIES || !activities[a].active || !ActivityMoverFree(i)) return false;
    if (moverAct[i] == a + 1) return true;
    if (!MoverInActivity(i)) ChaseMoverReset(i);
    if (moverAct[i] || moverWaiting[i]) ActDetach(i);
    moverAct[i] = a + 1;
    activities[a].count++;
    ActJoined(i);
    return true;
}

void ActivityLeave(int i) {
    if (i < 0 || i >= MAX_MOVERS || !(moverAct[i] || moverWaiting[i])) return;
    ChaseMoverLeft(i);
    ActDetach(i);
    playCooldown[i] = gameCooldown;
    if (i < moverCount && movers[i].freetimeState == FREETIME_PLAY) {   // (a need, job, draft or command took over: theirs now)
        Mover* m = &movers[i];
        m->freetimeState = FREETIME_NONE;
        m->pathLength = 0;
        m->pathIndex = -1;
    }
    playLeftCount++;
}

void ActivityEnd(int a) {
    if (a < 0 || a >= MAX_ACTIVITIES || !activities[a].active) return;
    ActFree(a);
}

// Members whose state something else took over (a need, a job, drafting, a command: no longer
// FREETIME_PLAY) leave; an activity too small to go on ends and its last members leave
static void ActivitySweep(void) {
    for (int i = 0; i < moverCount; i++) {
        if (!(moverAct[i] || moverWaiting[i])) continue;
        const Mover* m = &movers[i];
        if (!m->active || m->isDrafted || m->currentJobId >= 0 || m->freetimeState != FREETIME_PLAY) ActivityLeave(i);
    }
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        Activity* A = &activities[a];
        if (!A->active) continue;
        const ActivityDef* d = ActivityDefOf(A->kind);
        if (d->tooSmall ? d->tooSmall(a) : A->count < 2) ActFree(a);
    }
    for (int i = 0; i < moverCount; i++) {
        if (moverWaiting[i]) { ActivityLeave(i); continue; }   // nothing started: free again
        int a = moverAct[i] - 1;
        if (a >= 0 && !activities[a].active) ActivityLeave(i);
    }
}

float ActivityCooldown(int i) { return (i >= 0 && i < MAX_MOVERS && playCooldown[i] > 0.0f) ? playCooldown[i] : 0.0f; }

int ActivityStartWith(int kind, const int* players, int n, float seconds) {
    const ActivityDef* d = ActivityDefOf(kind);
    if (!d || !d->begin || n < 1) return -1;
    int a = ActivityStart(kind);
    if (a < 0) return -1;
    activities[a].timeLeft = seconds;
    d->begin(a, players, n);
    if (activities[a].count < (d->minPlayers > 0 ? d->minPlayers : 2)) { ActivityEnd(a); return -1; }
    return a;
}

// Free and idle, not just out of a game, nothing pressing: may propose or be asked
static bool ActCanPlay(int i) {
    if (!ActivityMoverFree(i) || MoverInActivity(i) || playCooldown[i] > 0.0f) return false;
    const Mover* m = &movers[i];
    if (m->freetimeState != FREETIME_NONE || MoverTalk(i) >= 0) return false;
    if (m->z - floorf(m->z) > 0.0001f || m->transportState != TRANSPORT_NONE) return false;   // on a ladder, travelling
    return !PlayWantsToStop(i);
}

static float ActRoll(void) { return GetRandomValue(0, 10000) / 10000.0f; }

// How much i likes the players of a (average), and how much they like i (lowest)
static void ActLikes(int a, int i, float* iLikes, float* theyLike) {
    float sum = 0.0f, low = 1e9f;
    int n = 0;
    for (int j = 0; j < moverCount; j++) {
        if (!ActivityHas(a, j)) continue;
        sum += RelationLike(i, j);
        float back = RelationLike(j, i);
        if (back < low) low = back;
        n++;
    }
    *iLikes = n ? sum / n : 0.0f;
    *theyLike = n ? low : 0.0f;
}

// i sees friends playing near by: it may join (a game they started themselves, room left, time left)
static bool ActTryJoin(int i) {
    const MoverNotice* nt = &moverNotice[i];
    for (int k = 0; k < nt->count; k++) {
        int j = nt->e[k].mover;
        if (j < 0 || j >= moverCount || (int)movers[j].z != (int)movers[i].z) continue;
        int a = MoverActivity(j);
        if (a < 0 || !activities[a].active || activities[a].timeLeft <= 0.0f) continue;   // (scenario games: no)
        const Activity* A = &activities[a];
        const ActivityDef* d = ActivityDefOf(A->kind);
        if (!d->join || (d->maxPlayers > 0 && A->count >= d->maxPlayers) || A->timeLeft - A->age < JOIN_LAST) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > (JOIN_RANGE * CELL_SIZE) * (JOIN_RANGE * CELL_SIZE)) continue;
        float iLikes, theyLike;
        ActLikes(a, i, &iLikes, &theyLike);
        if (iLikes <= 0.0f || theyLike < -10.0f) continue;   // with ones it likes, and nobody minds it
        float p = gameJoinChance * fminf(iLikes / 50.0f, 1.5f);
        if (SocialActive()) p *= 1.5f - moverSocial[i];
        if (ActRoll() >= p) return false;   // one roll per check
        if (!d->join(a, i)) return false;
        autoGameJoins++;
        autoGamePlayersSum++;
        EventLog("Mover %d joins %s (%d players)", i, d->name, activities[a].count);
        return true;
    }
    return false;
}

// i sometimes proposes a game to the ones it likes near by; they each say yes or no
static void ActPropose(int i) {
    if (!ActCanPlay(i)) return;
    if (ActTryJoin(i)) return;
    int cand[16], nc = 0;
    float likeSum = 0.0f;
    const MoverNotice* nt = &moverNotice[i];
    for (int k = 0; k < nt->count && nc < 16; k++) {
        int j = nt->e[k].mover;
        if (j < 0 || j >= moverCount || (int)movers[j].z != (int)movers[i].z || !ActCanPlay(j)) continue;
        float dx = movers[j].x - movers[i].x, dy = movers[j].y - movers[i].y;
        if (dx * dx + dy * dy > (PROPOSE_RANGE * CELL_SIZE) * (PROPOSE_RANGE * CELL_SIZE)) continue;
        float like = RelationLike(i, j);
        if (like <= 0.0f) continue;   // only with ones it likes
        cand[nc++] = j;
        likeSum += like;
    }
    if (nc == 0) return;
    float p = gameChance * fminf(likeSum / nc / 50.0f, 1.5f);
    if (SocialActive()) p *= 1.5f - moverSocial[i];   // lonely: keener
    if (ActRoll() >= p) return;
    int players[PROPOSE_MAX], n = 0;
    players[n++] = i;
    for (int k = 0; k < nc && n < PROPOSE_MAX; k++) {
        int j = cand[k];
        float yes = fminf(RelationLike(j, i) / 30.0f, 1.0f);
        if (SocialActive()) yes *= 1.5f - moverSocial[j];
        if (ActRoll() < yes) players[n++] = j; else autoGameDeclined++;
    }
    if (n < 2) return;
    int kinds[ACT_KIND_COUNT], nk = 0;   // the games these many can play
    for (int kd = 0; kd < ACT_KIND_COUNT; kd++) {
        const ActivityDef* d = ActivityDefOf(kd);
        if (d->minPlayers > 0 && d->begin && n >= d->minPlayers) kinds[nk++] = kd;
    }
    if (nk == 0) return;
    int kind = kinds[GetRandomValue(0, nk - 1)];
    const ActivityDef* d = ActivityDefOf(kind);
    if (d->maxPlayers > 0 && n > d->maxPlayers) n = d->maxPlayers;
    float secs = gameMinSeconds + (gameMaxSeconds - gameMinSeconds) * ActRoll();
    int g = ActivityStartWith(kind, players, n, secs);
    if (g >= 0) {
        autoGameStarts[kind]++;
        autoGamePlayersSum += activities[g].count;
        EventLog("Mover %d proposes %s: %d players, %.0f s", i, d->name, n, secs);
    }
}

void ActivityTick(float dt) {
    ActivitySweep();
    for (int i = 0; i < moverCount; i++) if (playCooldown[i] > 0.0f) playCooldown[i] -= dt;
    if (autoGames && talkEnabled)
        for (int i = (int)(currentTick % PROPOSE_EVERY); i < moverCount; i += PROPOSE_EVERY) ActPropose(i);
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        Activity* A = &activities[a];
        if (!A->active) continue;
        A->age += dt;
        if (A->timeLeft > 0.0f && A->age >= A->timeLeft) { ActFree(a); continue; }   // game over (members freed next sweep)
        const ActivityDef* d = ActivityDefOf(A->kind);
        if (d->tick) d->tick(a, dt);
    }
}

bool ActivityMoverTick(int i, float dt) {
    int a = MoverActivity(i);
    if (a < 0 || !activities[a].active) return false;
    const ActivityDef* d = ActivityDefOf(activities[a].kind);
    return d->moverTick && d->moverTick(a, i, dt);
}
