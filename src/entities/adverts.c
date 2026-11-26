// adverts.c - free time at home: things advertise what they offer (adverts.h)
#include "adverts.h"
#include "action_queue.h"
#include "activity.h"
#include "chase.h"
#include "furniture.h"
#include "mover.h"
#include "places.h"
#include "talk.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/event_log.h"
#include "../core/time.h"
#include "../simulation/balance.h"
#include "../simulation/floordirt.h"
#include "../simulation/needs.h"
#include "../simulation/mood.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

bool homeLife = false;
long advertStarted[ADV_KIND_COUNT];

#define ADV_RANGE       10      // cells round the mover it looks at (and the distance scale)
#define ADV_MAX         48      // adverts weighed per choice
#define ADV_GAP_MIN_GH  0.1f    // free time between activities (game hours): talks, visits
#define ADV_GAP_MAX_GH  0.6f

typedef struct {
    const char* name;           // "relax"
    const char* doing;          // describe (relax adds the furniture: "Relaxing on the Sofa")
    float minGH, maxGH;         // how long it lasts (game hours)
} AdvertKindDef;

static const AdvertKindDef advertKinds[ADV_KIND_COUNT] = {
    [ADV_RELAX]    = { "relax",    "Relaxing",          0.5f,  1.5f  },
    [ADV_LOOK_OUT] = { "look out", "Looking out of the window",   0.15f, 0.4f  },
    [ADV_TIDY]     = { "tidy",     "Tidying up",                  0.15f, 0.15f },
    [ADV_POTTER]   = { "potter",   "Pottering about",             0.05f, 0.2f  },
};

typedef struct {
    int kind;
    int target;                 // furniture (relax), -1
    int x, y, z;                // where it stands (look out, tidy, potter)
    float score;
} Advert;

static int advertKind[MAX_MOVERS];      // the activity it is doing + 1, 0 = none
static int advertTarget[MAX_MOVERS];    // its furniture (relax), for describe
static int advertCellX[MAX_MOVERS], advertCellY[MAX_MOVERS];   // its spot (no two at one)
static float advertGap[MAX_MOVERS];     // game seconds of free time before the next

static float AdvRand(float lo, float hi) {
    return lo + (hi - lo) * (float)GetRandomValue(0, 10000) / 10000.0f;
}

static bool AdvHasTrait(int i, TraitType t) {
    for (int k = 0; k < MAX_TRAITS; k++) if (movers[i].traits[k] == t) return true;
    return false;
}

void ClearAdverts(void) {
    memset(advertKind, 0, sizeof(advertKind));
    memset(advertGap, 0, sizeof(advertGap));
    memset(advertStarted, 0, sizeof(advertStarted));
    for (int i = 0; i < MAX_MOVERS; i++) advertCellX[i] = advertCellY[i] = advertTarget[i] = -1;
}

const char* AdvertKindName(int kind) {
    return (kind >= 0 && kind < ADV_KIND_COUNT) ? advertKinds[kind].name : "";
}

bool AdvertBusy(int i) {
    return i >= 0 && i < moverCount && advertKind[i] > 0 && QueueBusy(i);
}

static void AdvertDone(int i, float gapGH) {
    advertKind[i] = 0;
    advertTarget[i] = advertCellX[i] = advertCellY[i] = -1;
    advertGap[i] = GameHoursToGameSeconds(gapGH);
}

void AdvertStop(int i) {
    if (!AdvertBusy(i)) return;
    QueueEnd(i);   // releases the seat
    AdvertDone(i, AdvRand(ADV_GAP_MIN_GH, ADV_GAP_MAX_GH));
}

bool AdvertDescribe(int i, char* buf, int len) {
    if (!AdvertBusy(i)) return false;
    int k = advertKind[i] - 1;
    const char* what = (advertTarget[i] >= 0 && advertTarget[i] < MAX_FURNITURE && furniture[advertTarget[i]].active)
                       ? GetFurnitureDef(furniture[advertTarget[i]].type)->name : "";
    if (k == ADV_RELAX && what[0]) snprintf(buf, (size_t)len, "Relaxing on the %s", what);
    else snprintf(buf, (size_t)len, "%s", advertKinds[k].doing);
    return true;
}

// Free, still and at home: may start something
static bool AdvCanStart(int i, int home) {
    const Mover* m = &movers[i];
    if (!m->active || m->currentJobId >= 0 || m->freetimeState != FREETIME_NONE || m->isDrafted) return false;
    if (m->transportState != TRANSPORT_NONE || m->pathIndex >= 0 || m->needsRepath) return false;
    if (MoverTalk(i) >= 0 || MoverInActivity(i) || QueueBusy(i) || QueueSuspended(i)) return false;
    if (PlayWantsToStop(i)) return false;   // a need first
    return PlaceContains(home, (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z);
}

static bool AdvSpotTaken(int i, int x, int y) {
    for (int j = 0; j < moverCount; j++)
        if (j != i && advertKind[j] > 0 && advertCellX[j] == x && advertCellY[j] == y) return true;
    return false;
}

static void AdvAdd(Advert* ads, int* n, int kind, int target, int x, int y, int z, float rate, const Mover* m) {
    if (*n >= ADV_MAX || rate <= 0.0f) return;
    float dx = (x + 0.5f) - m->x / CELL_SIZE, dy = (y + 0.5f) - m->y / CELL_SIZE;
    ads[(*n)++] = (Advert){ kind, target, x, y, z, rate / (1.0f + sqrtf(dx * dx + dy * dy) / ADV_RANGE) };
}

// What the home offers this mover now
static int AdvCollect(int i, int home, Advert* ads) {
    const Mover* m = &movers[i];
    int n = 0, mz = (int)m->z, mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE);
    // Seats: a sofa more than a chair, more so when tired
    const int* list;
    int nf = PlaceFurniture(home, &list);
    for (int k = 0; k < nf; k++) {
        int f = list[k];
        const FurnitureDef* def = GetFurnitureDef(furniture[f].type);
        if (furniture[f].z != mz || !(def->capabilities & CAP_SEATING) || !FurnitureHasRoom(f)) continue;
        if (!MayUse(i, furniture[f].x, furniture[f].y, furniture[f].z)) continue;
        float rate = (def->restRate > 0.0f ? 1.2f : 0.8f) * (0.4f + 1.2f * (1.0f - m->energy));   // tired: sit
        AdvAdd(ads, &n, ADV_RELAX, f, furniture[f].x, furniture[f].y, mz, rate, m);
    }
    // Windows (a spot inside next to one), dirty floor, and a few spots to potter to
    bool dark = IsSkyDark();
    bool tidy = AdvHasTrait(i, TRAIT_TIDY);
    static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
    for (int y = my - ADV_RANGE; y <= my + ADV_RANGE; y++)
        for (int x = mx - ADV_RANGE; x <= mx + ADV_RANGE; x++) {
            if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
            if (GRID(x, y, mz) == CELL_WINDOW) {
                for (int d = 0; d < 4; d++) {
                    int sx = x + ddx[d], sy = y + ddy[d];
                    if (sx < 0 || sy < 0 || sx >= gridWidth || sy >= gridHeight) continue;
                    if (!PlaceContains(home, sx, sy, mz) || !IsCellStandableAt(mz, sy, sx) || AdvSpotTaken(i, sx, sy)) continue;
                    AdvAdd(ads, &n, ADV_LOOK_OUT, -1, sx, sy, mz, dark ? 0.3f : 1.0f, m);
                    break;
                }
                continue;
            }
            int dirt = GetFloorDirt(x, y, mz);
            if (dirt >= DIRT_CLEAN_THRESHOLD && PlaceContains(home, x, y, mz) && IsCellStandableAt(mz, y, x) && !AdvSpotTaken(i, x, y))
                AdvAdd(ads, &n, ADV_TIDY, -1, x, y, mz, (0.3f + dirt / 120.0f) * (tidy ? 2.0f : 1.0f), m);
        }
    for (int tries = 0, found = 0; tries < 12 && found < 3; tries++) {
        int x = mx + GetRandomValue(-ADV_RANGE / 2, ADV_RANGE / 2), y = my + GetRandomValue(-ADV_RANGE / 2, ADV_RANGE / 2);
        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || (x == mx && y == my)) continue;
        if (!PlaceContains(home, x, y, mz) || !IsCellStandableAt(mz, y, x) || AdvSpotTaken(i, x, y)) continue;
        AdvAdd(ads, &n, ADV_POTTER, -1, x, y, mz, 0.5f, m);
        found++;
    }
    return n;
}

// The best offer of each kind, then one of those at random, weighted by score (variety: four
// seats in a flat must not crowd out the window)
static int AdvPick(Advert* ads, int n) {
    int best[ADV_KIND_COUNT];
    for (int k = 0; k < ADV_KIND_COUNT; k++) best[k] = -1;
    for (int a = 0; a < n; a++)
        if (best[ads[a].kind] < 0 || ads[a].score > ads[best[ads[a].kind]].score) best[ads[a].kind] = a;
    float total = 0.0f;
    for (int k = 0; k < ADV_KIND_COUNT; k++) if (best[k] >= 0) total += ads[best[k]].score;
    if (total <= 0.0f) return -1;
    float r = AdvRand(0.0f, total);
    for (int k = 0; k < ADV_KIND_COUNT; k++) {
        if (best[k] < 0) continue;
        r -= ads[best[k]].score;
        if (r <= 0.0f) return best[k];
    }
    for (int k = ADV_KIND_COUNT - 1; k >= 0; k--) if (best[k] >= 0) return best[k];
    return -1;
}

static bool AdvStart(int i, const Advert* ad) {
    const AdvertKindDef* def = &advertKinds[ad->kind];
    float secs = GameHoursToGameSeconds(AdvRand(def->minGH, def->maxGH));
    QueueReset(i);
    switch (ad->kind) {
    case ADV_RELAX:
        QueuePush(i, ActWalkToFurniture(ad->target, false));
        QueuePush(i, ActSit(ad->target, false));
        QueuePush(i, ActWait(secs));
        ReserveFurniture(ad->target, i);
        moverQueues[i].seat = ad->target;
        break;
    case ADV_TIDY:
        QueuePush(i, ActWalkToCell(ad->x, ad->y, ad->z, false));
        QueuePush(i, ActTidy(ad->x, ad->y, ad->z, secs));
        break;
    default:   // look out, potter: go and stand there
        QueuePush(i, ActWalkToCell(ad->x, ad->y, ad->z, false));
        QueuePush(i, ActWait(secs));
        break;
    }
    QueueStart(i, QUEUE_FROM_FREETIME);
    advertKind[i] = ad->kind + 1;
    advertTarget[i] = ad->target;
    advertCellX[i] = ad->kind == ADV_RELAX ? -1 : ad->x;
    advertCellY[i] = ad->kind == ADV_RELAX ? -1 : ad->y;
    advertStarted[ad->kind]++;
    EventLog("Mover %d at home: %s (score %.2f)", i, def->name, ad->score);
    return true;
}

void AdvertTick(float dt) {
    if (!homeLife) return;
    for (int i = 0; i < moverCount; i++) {
        if (advertKind[i] > 0) {
            if (!QueueBusy(i) && !QueueSuspended(i)) AdvertDone(i, AdvRand(ADV_GAP_MIN_GH, ADV_GAP_MAX_GH));   // done
            else if (QueueBusy(i) && PlayWantsToStop(i)) { QueueEnd(i); AdvertDone(i, 0.05f); }        // a need
            continue;
        }
        if (advertGap[i] > 0.0f) advertGap[i] -= dt;
    }
    // Starting: each free resident looks round once a second
    for (int i = (int)(currentTick % TICK_RATE); i < moverCount; i += TICK_RATE) {
        if (advertKind[i] > 0 || advertGap[i] > 0.0f) continue;
        int home = HomeOfGroup(MoverHousehold(i));
        if (home <= 0 || !AdvCanStart(i, home)) continue;
        Advert ads[ADV_MAX];
        int n = AdvCollect(i, home, ads);
        int a = AdvPick(ads, n);
        if (a >= 0) AdvStart(i, &ads[a]);
        else advertGap[i] = GameHoursToGameSeconds(ADV_GAP_MIN_GH);   // nothing here: look again later
    }
}
