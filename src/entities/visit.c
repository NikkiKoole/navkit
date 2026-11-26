// visit.c - visiting a friend at home (see visit.h)
#include "visit.h"
#include "activity.h"
#include "places.h"
#include "relations.h"
#include "mover.h"
#include "chase.h"
#include "talk.h"
#include "namegen.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/event_log.h"
#include "../simulation/needs.h"
#include "../simulation/balance.h"
#include "../simulation/mood.h"
#include <string.h>
#include <stdio.h>

bool autoVisits = false;
float visitsPerDay = 1.0f;
float visitMinLike = 30.0f;
float visitWaitHours = 1.0f;
float visitStayMinHours = 1.0f, visitStayMaxHours = 2.5f;
float visitCooldownHours = 8.0f;
float visitCloseChance = 0.5f;
long visitStarted, visitAnswered, visitGivenUp, visitDone, visitCutShort;
long visitTalks;   // visits where guest and host got talking

static float visitCooldown[MAX_MOVERS];   // sim seconds before it may visit again
static bool visitGoHome[MAX_MOVERS];      // a visit ended: walk home (and lose the grant outside)

typedef enum { VS_WALK, VS_KNOCK, VS_ANSWER, VS_ENTER, VS_STAY, VS_LEAVE } VisitPhase;
static const char* const visitPhaseNames[] = { "walking over", "knocking", "the door is answered",
                                               "coming in", "staying", "leaving" };

typedef struct {
    int guest, host;           // movers (host -1 until someone answers)
    int friendMover;           // whom it came for
    int household, home;       // the host household and its home place
    int phase;
    float timer;               // seconds in this phase
    float stay;                // seconds the stay lasts
    bool talked;               // the stay's conversation has started (once)
    int outX, outY, inX, inY, z;   // the doorstep outside and the cell inside the front door
    int spotX, spotY;          // where the guest goes inside
} VisitState;

static VisitState* VisitOf(int a) {
    return (a >= 0 && a < MAX_ACTIVITIES && activities[a].active && activities[a].kind == ACT_VISIT)
        ? (VisitState*)activities[a].state : NULL;
}

static float VisitRand(float lo, float hi) { return lo + (hi - lo) * (GetRandomValue(0, 10000) / 10000.0f); }

static bool VisitHasTrait(int i, TraitType t) {
    for (int k = 0; k < MAX_TRAITS; k++) if (movers[i].traits[k] == t) return true;
    return false;
}

static bool VisitAtCell(int i, int x, int y, int z) {
    const Mover* m = &movers[i];
    return (int)(m->x / CELL_SIZE) == x && (int)(m->y / CELL_SIZE) == y && (int)m->z == z;
}

static void VisitGoTo(int i, int x, int y, int z) {
    Mover* m = &movers[i];
    if (VisitAtCell(i, x, y, z)) return;
    if (m->goal.x == x && m->goal.y == y && m->goal.z == z && (m->pathIndex >= 0 || m->needsRepath)) return;
    m->goal = (Point){x, y, z};
    m->needsRepath = true;
}

// The front door's two sides: the walkable cell outside the home and the one inside
static bool VisitDoorSides(int home, int* outX, int* outY, int* inX, int* inY, int* z) {
    const PlaceInfo* p = &places[home];
    if (!p->active || p->doorX < 0) return false;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {1, 0, -1, 0};
    bool haveOut = false, haveIn = false;
    for (int d = 0; d < 4; d++) {
        int x = p->doorX + dx[d], y = p->doorY + dy[d];
        if (!IsCellWalkableAt(p->doorZ, y, x)) continue;
        if (PlaceAt(x, y, p->doorZ) == home) { if (!haveIn) { *inX = x; *inY = y; haveIn = true; } }
        else if (!haveOut) { *outX = x; *outY = y; haveOut = true; }
    }
    *z = p->doorZ;
    return haveOut && haveIn;
}

// A member of the household at home (inside its home place) and free to come to the door
static int VisitFindHost(const VisitState* st) {
    for (int j = 0; j < moverCount; j++) {
        if (j == st->guest || !movers[j].active || !MoverInGroup(j, st->household)) continue;
        int x = (int)(movers[j].x / CELL_SIZE), y = (int)(movers[j].y / CELL_SIZE);
        if (PlaceAt(x, y, (int)movers[j].z) != st->home) continue;
        if (movers[j].freetimeState != FREETIME_NONE || !ActivityMoverFree(j) || MoverInActivity(j)) continue;
        if (PlayWantsToStop(j)) continue;
        return j;
    }
    return -1;
}

// The guest's spot inside: two cells in from the door if it can stand there, else just inside
static void VisitPickSpot(VisitState* st) {
    int sx = st->inX + (st->inX - st->outX) / 2, sy = st->inY + (st->inY - st->outY) / 2;
    if (IsCellStandableAt(st->z, sy, sx) && PlaceAt(sx, sy, st->z) == st->home) { st->spotX = sx; st->spotY = sy; }
    else { st->spotX = st->inX; st->spotY = st->inY; }
}

static void VisitPhaseTo(VisitState* st, int phase) {
    st->phase = phase;
    st->timer = 0.0f;
}

// Inside the home (or a place within it)?
static bool VisitInside(int i, int home) {
    const Mover* m = &movers[i];
    for (int p = PlaceAt((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z), guard = 0; p > 0 && guard < PLACE_MAX_DEPTH;
         p = places[p].parent, guard++)
        if (p == home) return true;
    return false;
}

// A guest outside the home it was let into loses the grant at once (so a need that comes up on
// the doormat doesn't send it back in to the host's toilet).
// A walk still headed inside (the guest's spot: a visit cut short in the doorway) stops there.
static void VisitDropGrantOutside(int i) {
    if (i < 0 || MoverGrant(i) <= 0 || VisitInside(i, MoverGrant(i))) return;
    GrantPlace(i, 0);
    Mover* m = &movers[i];
    if (!MayEnter(i, m->goal.x, m->goal.y, m->goal.z)) {
        m->goal = (Point){(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
        m->pathLength = 0;
        m->pathIndex = -1;
        m->needsRepath = false;
        moverRouteLen[i] = 0;
    }
}

static void VisitFinish(int a, VisitState* st, bool gaveUp) {
    if (gaveUp) visitGivenUp++; else visitDone++;
    EventLog("Visit: %s %s %s", MoverDisplayName(st->guest), gaveUp ? "gave up at" : "left", PlaceName(st->home));
    if (st->guest >= 0) { visitGoHome[st->guest] = true; VisitDropGrantOutside(st->guest); }
    ActivityEnd(a);
}

static void VisitTickOne(int a, float dt) {
    VisitState* st = VisitOf(a);
    if (!st) return;
    st->timer += dt;
    int g = st->guest;
    switch (st->phase) {
    case VS_WALK:
        if (VisitAtCell(g, st->outX, st->outY, st->z)) {
            VisitPhaseTo(st, VS_KNOCK);
            EventLog("Visit: %s knocks at %s", MoverDisplayName(g), PlaceName(st->home));
        } else if (st->timer > GameHoursToGameSeconds(3.0f)) {
            VisitFinish(a, st, true);   // never got there
        } else {
            VisitGoTo(g, st->outX, st->outY, st->z);
        }
        break;
    case VS_KNOCK: {
        int h = VisitFindHost(st);
        if (h >= 0 && ActivityAdd(a, h)) {
            st->host = h;
            VisitPhaseTo(st, VS_ANSWER);
            EventLog("Visit: %s comes to the door for %s", MoverDisplayName(h), MoverDisplayName(g));
        } else if (st->timer > GameHoursToGameSeconds(visitWaitHours)) {
            VisitFinish(a, st, true);
        }
        break;
    }
    case VS_ANSWER:
        if (st->host < 0) { VisitPhaseTo(st, VS_KNOCK); break; }   // called away: knock again
        if (!VisitAtCell(st->host, st->inX, st->inY, st->z)) { VisitGoTo(st->host, st->inX, st->inY, st->z); break; }
        SetDoorOpen(st->outX + (st->inX - st->outX) / 2, st->outY + (st->inY - st->outY) / 2, st->z, true);
        GrantPlace(g, st->home);
        visitAnswered++;
        VisitPickSpot(st);
        VisitPhaseTo(st, VS_ENTER);
        EventLog("Visit: %s lets %s in", MoverDisplayName(st->host), MoverDisplayName(g));
        break;
    case VS_ENTER:
        VisitGoTo(g, st->spotX, st->spotY, st->z);
        if (st->host >= 0) {   // the host steps back from the door, beside the guest's spot
            int hx = st->spotX + (st->spotY - st->inY), hy = st->spotY + (st->spotX - st->inX);
            if (IsCellStandableAt(st->z, hy, hx) && PlaceAt(hx, hy, st->z) == st->home) VisitGoTo(st->host, hx, hy, st->z);
        }
        if (VisitAtCell(g, st->spotX, st->spotY, st->z)) {
            st->stay = GameHoursToGameSeconds(VisitRand(visitStayMinHours, visitStayMaxHours));
            VisitPhaseTo(st, VS_STAY);
        } else if (st->timer > GameHoursToGameSeconds(1.0f)) {
            VisitPhaseTo(st, VS_LEAVE);   // couldn't get in
        }
        break;
    case VS_STAY:
        // They talk from the moment the guest is in, for the whole stay (talk.c: turns, faces)
        if (st->host >= 0 && talkEnabled && MoverTalk(g) < 0 && MoverTalk(st->host) < 0 && !st->talked) {
            if (TalkStart(g, st->host) >= 0) {
                TalkSetTimeLeft(g, st->stay - st->timer);
                st->talked = true;
                visitTalks++;
            }
        }
        if (st->host >= 0) {   // company: they like each other a little more for it
            RelationAdd(g, st->host, 0.5f * dt / GameHoursToGameSeconds(1.0f), 2.0f * dt / GameHoursToGameSeconds(1.0f));
            RelationAdd(st->host, g, 0.5f * dt / GameHoursToGameSeconds(1.0f), 2.0f * dt / GameHoursToGameSeconds(1.0f));
        }
        if (st->timer >= st->stay || st->host < 0) VisitPhaseTo(st, VS_LEAVE);
        break;
    case VS_LEAVE:
        if (MoverTalk(g) >= 0) TalkLeave(g);   // goodbye (the host, left on its own, stops too)
        VisitGoTo(g, st->outX, st->outY, st->z);
        if (st->host >= 0) VisitGoTo(st->host, st->inX, st->inY, st->z);
        if (VisitAtCell(g, st->outX, st->outY, st->z)) {
            int dx = st->outX + (st->inX - st->outX) / 2, dy = st->outY + (st->inY - st->outY) / 2;
            if (st->host >= 0 && (VisitHasTrait(st->host, TRAIT_TIDY) || VisitRand(0.0f, 1.0f) < visitCloseChance)) {
                SetDoorOpen(dx, dy, st->z, false);
                EventLog("Visit: %s closes the door behind %s", MoverDisplayName(st->host), MoverDisplayName(g));
            }
            VisitFinish(a, st, false);
        } else if (st->timer > GameHoursToGameSeconds(2.0f)) {
            VisitFinish(a, st, false);
        }
        break;
    }
}

static void VisitMemberLeft(int a, int i) {
    VisitState* st = VisitOf(a);
    if (!st) return;
    if (i == st->host) st->host = -1;
    if (i == st->guest) {   // a need took it away: home afterwards
        visitCutShort++;
        EventLog("Visit: %s's visit to %s cut short", MoverDisplayName(i), PlaceName(st->home));
        visitGoHome[i] = true;
        VisitDropGrantOutside(i);
    }
}

static bool VisitTooSmall(int a) {
    const VisitState* st = VisitOf(a);
    return !st || st->guest < 0 || !ActivityHas(a, st->guest);
}

const ActivityDef visitActivityDef = {
    "visit", "Visiting", sizeof(VisitState),
    VisitTickOne, NULL, NULL, VisitMemberLeft, VisitTooSmall, NULL,
    0, 0, NULL, NULL,   // (not a game: never proposed by the game rule, nobody joins)
};

int VisitStart(int guest, int friendMover) {
    if (guest < 0 || guest >= moverCount || friendMover < 0 || friendMover >= moverCount || guest == friendMover) return -1;
    int hh = MoverHousehold(friendMover);
    int home = HomeOfGroup(hh);
    if (hh <= 0 || home <= 0 || MoverInGroup(guest, hh)) return -1;
    VisitState tmp = {0};
    if (!VisitDoorSides(home, &tmp.outX, &tmp.outY, &tmp.inX, &tmp.inY, &tmp.z)) return -1;
    if (!ActivityMoverFree(guest) || MoverInActivity(guest)) return -1;
    int a = ActivityStart(ACT_VISIT);
    if (a < 0) return -1;
    VisitState* st = VisitOf(a);
    *st = tmp;
    st->guest = guest;
    st->host = -1;
    st->friendMover = friendMover;
    st->household = hh;
    st->home = home;
    st->phase = VS_WALK;
    if (!ActivityAdd(a, guest)) { ActivityEnd(a); return -1; }
    visitStarted++;
    visitGoHome[guest] = false;
    visitCooldown[guest] = GameHoursToGameSeconds(visitCooldownHours);
    EventLog("Visit: %s goes to see %s at %s", MoverDisplayName(guest), MoverDisplayName(friendMover), PlaceName(home));
    VisitGoTo(guest, st->outX, st->outY, st->z);
    return a;
}

void ClearVisits(void) {
    memset(visitCooldown, 0, sizeof(visitCooldown));
    memset(visitGoHome, 0, sizeof(visitGoHome));
    visitStarted = visitAnswered = visitGivenUp = visitDone = visitCutShort = 0;
    visitTalks = 0;
}

// Free, idle, nothing pressing: may set off on a visit
static bool VisitCanGo(int i) {
    const Mover* m = &movers[i];
    if (!m->active || !ActivityMoverFree(i) || MoverInActivity(i) || m->freetimeState != FREETIME_NONE) return false;
    if (MoverTalk(i) >= 0 || visitCooldown[i] > 0.0f || PlayWantsToStop(i)) return false;
    int hh = MoverHousehold(i);
    if (hh <= 0) return false;
    return PlaceAt((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z) == HomeOfGroup(hh);   // from home
}

static int VisitPickFriend(int i) {
    int best = -1;
    float bestLike = visitMinLike;
    int hh = MoverHousehold(i);
    for (int j = 0; j < moverCount; j++) {
        if (j == i || !movers[j].active) continue;
        int hj = MoverHousehold(j);
        if (hj <= 0 || hj == hh || HomeOfGroup(hj) <= 0) continue;
        float like = RelationLike(i, j);
        if (like >= bestLike) { bestLike = like; best = j; }
    }
    return best;
}

void VisitTick(float dt) {
    for (int i = 0; i < moverCount; i++) if (visitCooldown[i] > 0.0f) visitCooldown[i] -= dt;
    // Former guests: walk home; the grant goes once they are out of the home it was for
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)m->z;
        if (!MoverIsVisiting(i)) VisitDropGrantOutside(i);
        if (!visitGoHome[i]) continue;
        int home = HomeOfGroup(MoverHousehold(i));
        if (home <= 0 || PlaceAt(x, y, z) == home) { visitGoHome[i] = false; continue; }
        if (m->freetimeState != FREETIME_NONE || m->currentJobId >= 0 || MoverInActivity(i) || m->pathIndex >= 0 || m->needsRepath) continue;
        int ox, oy, ix, iy, hz;
        if (VisitDoorSides(home, &ox, &oy, &ix, &iy, &hz)) {
            m->goal = (Point){ix, iy, hz};
            m->needsRepath = true;
        } else visitGoHome[i] = false;
    }
    // Setting off: each free resident checks once a second
    if (!autoVisits || dayLength <= 0.0f) return;
    float p = visitsPerDay / dayLength;   // per check (once a sim second)
    for (int i = (int)(currentTick % TICK_RATE); i < moverCount; i += TICK_RATE) {
        if (!VisitCanGo(i) || VisitRand(0.0f, 1.0f) >= p) continue;
        int f = VisitPickFriend(i);
        if (f >= 0) VisitStart(i, f);
    }
}

bool MoverIsVisiting(int i) {
    const VisitState* st = VisitOf(MoverActivity(i));
    return st && st->guest == i;
}

bool MoverIsHosting(int i) {
    const VisitState* st = VisitOf(MoverActivity(i));
    return st && st->host == i;
}

bool VisitDescribe(int i, char* buf, int len) {
    const VisitState* st = VisitOf(MoverActivity(i));
    if (!st) return false;
    if (st->guest == i) snprintf(buf, (size_t)len, "Visiting %s at %s: %s", MoverDisplayName(st->friendMover),
                                 PlaceName(st->home), visitPhaseNames[st->phase]);
    else snprintf(buf, (size_t)len, "Has %s over: %s", MoverDisplayName(st->guest), visitPhaseNames[st->phase]);
    return true;
}
