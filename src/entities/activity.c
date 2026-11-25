// activity.c - joint activities: the pool, membership, the sweep (see activity.h)
#include "activity.h"
#include "chase.h"
#include "mover.h"
#include "action_queue.h"
#include <stdlib.h>
#include <string.h>

Activity activities[MAX_ACTIVITIES];
long playLeftCount = 0;
static int moverAct[MAX_MOVERS];       // activity index + 1, 0 = none
static bool moverWaiting[MAX_MOVERS];  // joined, no activity yet

// The kinds (each defined with its logic: the games in chase.c)
const ActivityDef* ActivityDefOf(int kind) {
    switch (kind) {
        case ACT_TAG:           return &tagActivityDef;
        case ACT_HIDE_SEEK:     return &hideSeekActivityDef;
        case ACT_FOLLOW_LEADER: return &followLeaderActivityDef;
        case ACT_PACK_HUNT:     return &packHuntActivityDef;
        default:                return NULL;
    }
}

static void ActFree(int a) {
    Activity* A = &activities[a];
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
    playLeftCount = 0;
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

void ActivityTick(float dt) {
    ActivitySweep();
    for (int a = 0; a < MAX_ACTIVITIES; a++) {
        Activity* A = &activities[a];
        if (!A->active) continue;
        A->age += dt;
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
