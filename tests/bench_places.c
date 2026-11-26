// bench_places.c - ownership and access (src/entities/places.c) under load
//
// Run with: make bench_places
//
// What it times (docs/todo/architecture/ownership-and-access.md):
//   1. MayEnter on every cell of a 512x512 level: no places, 1000 flats, flats with rooms
//      nested 8 and 32 deep (a stranger: the worst case, the whole chain plus the group and
//      grant checks)
//   2. HomeOfGroup (a scan of the place table) with the table full
//   3. The food search's extra cost: a scan of 10000 items with and without MayTake
//   4. VisitTick: movers at home checking for a friend to visit (VisitPickFriend asks
//      HomeOfGroup for every other mover)

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/places.h"
#include "../src/entities/visit.h"
#include "../src/entities/activity.h"
#include "../src/entities/talk.h"
#include "../src/entities/relations.h"
#include "../src/core/time.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static double Now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}

#define BW 512
#define BH 512
static volatile long sink;

static void Fresh(void) {
    InitGridWithSizeAndChunkSize(BW, BH, 16, 16);
    ClearMovers();
    ClearPlaces();
}

// A stranger: in 4 groups, none of which owns anything, and a grant somewhere else
static int Stranger(int otherGroups) {
    int i = 0;
    moverCount = 1;
    PlacesResetMover(i);
    for (int k = 0; k < MAX_MOVER_GROUPS; k++) MoverJoinGroup(i, otherGroups + k);
    return i;
}

// Flats of 8x6 cells in a grid over the level, `depth` places nested in each (the innermost
// covers the middle 2x2), only the outermost sets rules
static int BuildFlats(int count, int depth) {
    int made = 0;
    for (int f = 0; f < count; f++) {
        int hh = GroupCreate(GROUP_HOUSEHOLD, "flat");
        if (hh < 0) break;
        int ox = (f % 60) * 8, oy = (f / 60) * 6;
        if (oy + 6 > BH) break;
        int p = PlaceCreate(hh, 0, PLACE_RULES_HOME, "flat");
        if (p < 0) break;
        PlaceFillRect(p, ox, oy, ox + 7, oy + 5, 1);
        for (int d = 1; d < depth; d++) {
            int q = PlaceCreate(0, p, 0, "room");
            if (q < 0) break;
            p = q;
        }
        PlaceFillRect(p, ox + 3, oy + 2, ox + 4, oy + 3, 1);
        made++;
    }
    return made;
}

static double TimeAllCells(int mover, int reps) {
    double t0 = Now();
    long yes = 0;
    for (int r = 0; r < reps; r++)
        for (int y = 0; y < BH; y++)
            for (int x = 0; x < BW; x++) yes += MayEnter(mover, x, y, 1);
    double dt = Now() - t0;
    sink += yes;
    return dt * 1e9 / ((double)reps * BW * BH);
}

// The innermost cells only (where the whole chain is walked)
static double TimeInnermost(int mover, int flats, int reps) {
    double t0 = Now();
    long yes = 0;
    for (int r = 0; r < reps * 64; r++)
        for (int f = 0; f < flats; f++) {
            int ox = (f % 60) * 8, oy = (f / 60) * 6;
            yes += MayEnter(mover, ox + 3, oy + 2, 1);
        }
    double dt = Now() - t0;
    sink += yes;
    return dt * 1e9 / ((double)reps * 64 * flats);
}

static void BenchQueries(void) {
    printf("--- MayEnter, a stranger (ns per query) ---\n");
    printf("  %-34s %10s %12s\n", "world", "all cells", "innermost");
    Fresh();
    int m = Stranger(1);
    for (int g = 0; g < MAX_MOVER_GROUPS + 1; g++) GroupCreate(GROUP_BUSINESS, "elsewhere");
    printf("  %-34s %10.2f %12s\n", "no places (empty layer)", TimeAllCells(m, 20), "-");
    int depths[3] = { 1, 8, PLACE_MAX_DEPTH };
    for (int d = 0; d < 3; d++) {
        Fresh();
        for (int g = 0; g < MAX_MOVER_GROUPS + 1; g++) GroupCreate(GROUP_BUSINESS, "elsewhere");
        m = Stranger(1);
        int flats = BuildFlats(MAX_PLACES / depths[d], depths[d]);
        GrantPlace(m, 1);                      // a guest in the first flat: the grant walk runs too
        char label[64];
        snprintf(label, sizeof label, "%d flats, nested %d deep", flats, depths[d]);
        printf("  %-34s %10.2f %12.2f\n", label, TimeAllCells(m, 20), TimeInnermost(m, flats, 20));
    }
    printf("\n");
}

static void BenchHomeOfGroup(void) {
    printf("--- HomeOfGroup, place table full (%d places) ---\n", MAX_PLACES - 1);
    Fresh();
    int first = GroupCreate(GROUP_HOUSEHOLD, "first");
    for (int p = 1; p < MAX_PLACES; p++) {
        int g = p == MAX_PLACES - 1 ? GroupCreate(GROUP_HOUSEHOLD, "last") : first;
        int q = PlaceCreate(g, 0, PLACE_RULES_HOME, "p");
        if (p == 1 || p == MAX_PLACES - 1) PlaceSetDoor(q, 1, 1, 1);
    }
    int last = first + 1;
    int reps = 200000;
    double t0 = Now();
    for (int r = 0; r < reps; r++) sink += HomeOfGroup(first);
    double best = (Now() - t0) * 1e9 / reps;
    t0 = Now();
    for (int r = 0; r < reps; r++) sink += HomeOfGroup(last);
    double worst = (Now() - t0) * 1e9 / reps;
    t0 = Now();
    for (int r = 0; r < reps; r++) sink += HomeOfGroup(last + 1);   // no home at all
    double none = (Now() - t0) * 1e9 / reps;
    printf("  first place: %.1f ns, last place: %.1f ns, no home: %.1f ns\n\n", best, worst, none);
}

static void BenchFoodScan(void) {
    printf("--- Food search scan, 10000 items (us per search) ---\n");
    Fresh();
    ClearItems();
    for (int g = 0; g < MAX_MOVER_GROUPS + 1; g++) GroupCreate(GROUP_BUSINESS, "elsewhere");
    int m = Stranger(1);
    BuildFlats(MAX_PLACES - 1, 1);
    SetRandomSeed(4242);
    for (int i = 0; i < 10000; i++) {
        float x = GetRandomValue(0, BW - 1) * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = GetRandomValue(0, BH - 1) * CELL_SIZE + CELL_SIZE * 0.5f;
        SpawnItem(x, y, 1.0f, ITEM_BERRIES);
    }
    int reps = 2000;
    for (int withPlaces = 0; withPlaces < 2; withPlaces++) {
        double t0 = Now();
        for (int r = 0; r < reps; r++) {
            int best = -1;
            float bestD = 1e30f;
            for (int i = 0; i < itemHighWaterMark; i++) {
                if (!items[i].active || items[i].reservedBy != -1) continue;
                if (withPlaces && !MayTake(m, (int)(items[i].x / CELL_SIZE), (int)(items[i].y / CELL_SIZE), (int)items[i].z)) continue;
                float dx = items[i].x - 100.0f, dy = items[i].y - 100.0f, d = dx * dx + dy * dy;
                if (d < bestD) { bestD = d; best = i; }
            }
            sink += best;
        }
        printf("  %-20s %8.2f\n", withPlaces ? "with MayTake" : "without", (Now() - t0) * 1e6 / reps);
    }
    printf("\n");
}

static void BenchVisitTick(void) {
    printf("--- VisitTick: every mover at home, in a household (ms per tick, 60 ticks) ---\n");
    int counts[3] = { 200, 1000, 2000 };
    for (int c = 0; c < 3; c++) {
        Fresh();
        ClearActivities(); ClearTalks(); ClearRelations(); ClearVisits();
        int n = counts[c];
        int flats = BuildFlats(n / 2 < MAX_PLACES - 1 ? n / 2 : MAX_PLACES - 1, 1);
        for (int p = 1; p <= flats; p++) {
            int ox = ((p - 1) % 60) * 8, oy = ((p - 1) / 60) * 6;
            PlaceSetDoor(p, ox + 3, oy + 5, 1);
        }
        for (int i = 0; i < n; i++) {
            int p = 1 + i % flats;
            int ox = ((p - 1) % 60) * 8, oy = ((p - 1) / 60) * 6;
            Point goal = { ox + 2, oy + 2, 1 };
            InitMover(&movers[i], (ox + 2.5f) * CELL_SIZE, (oy + 2.5f) * CELL_SIZE, 1.0f, goal, 100.0f);
            moverCount = i + 1;
            PlacesResetMover(i);
            MoverJoinGroup(i, places[p].owner);
        }
        // Everyone checks (a visit chance of 1 per check) but nobody likes anyone enough to go:
        // the whole friend search runs and nothing changes
        autoVisits = true;
        dayLength = 60.0f;
        visitsPerDay = 1000.0f;
        visitMinLike = 1000.0f;
        double t0 = Now();
        for (int t = 0; t < 60; t++) { currentTick = t; VisitTick(TICK_DT); }
        double ms = (Now() - t0) * 1e3 / 60;
        printf("  %5d movers in %4d flats: %8.3f ms per tick (each mover checks once a second)\n", n, flats, ms);
    }
    printf("\n");
}

int main(void) {
    SetTraceLogLevel(LOG_NONE);
    BenchQueries();
    BenchHomeOfGroup();
    BenchFoodScan();
    BenchVisitTick();
    return 0;
}
