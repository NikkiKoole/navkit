// test_nav_standalone.c — the nav library without the game.
//
// Builds nav/nav.c against tests/nav_stub_host.h only (no raylib, no game
// headers, no test_unity.o). If game code ever leaks into nav/, this stops
// compiling. Also checks multiple independent worlds and variable cost.

#define NAV_HOST_HEADER "../tests/nav_stub_host.h"
#include "../nav/nav.c"
#include <stdio.h>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static NavPoint buf[64 * 64 * 4];

static void test_open_diagonal(void) {
    static StubMap m = {.w = 32, .h = 32, .d = 1};
    NavWorld* w = NavCreate(32, 32, 1, 8, 8, &m);
    int len = NavFindPathAStar(w, (NavPoint){0, 0, 0}, (NavPoint){31, 31, 0}, buf, (int)(sizeof(buf) / sizeof(buf[0])));
    CHECK(len == 32);
    CHECK(w->lastCost == 31 * 14);
    CHECK(buf[0].x == 31 && buf[0].y == 31);          // goal-first
    CHECK(buf[len - 1].x == 0 && buf[len - 1].y == 0);

    NavBuildEntrances(w);
    NavBuildGraph(w);
    int hlen = NavFindPathHPA(w, (NavPoint){0, 0, 0}, (NavPoint){31, 31, 0}, buf, 4096);
    CHECK(hlen > 0);
    CHECK(buf[0].x == 31 && buf[0].y == 31 && buf[hlen - 1].x == 0 && buf[hlen - 1].y == 0);
    NavDestroy(w);
}

static void test_variable_cost_detour(void) {
    // Very expensive band (25x) across y=8, except a normal-cost gap at x=6.
    // Straight through costs +245; detouring via the gap costs ~+30.
    static StubMap m = {.w = 32, .h = 16, .d = 1};
    for (int x = 0; x < 32; x++) if (x != 6) m.cost[0][8][x] = 250;
    NavWorld* w = NavCreate(32, 16, 1, 8, 8, &m);
    int len = NavFindPathAStar(w, (NavPoint){2, 2, 0}, (NavPoint){2, 14, 0}, buf, 4096);
    CHECK(len > 0);
    bool crossedCheap = false;
    for (int i = 0; i < len; i++) if (buf[i].y == 8 && buf[i].x == 6) crossedCheap = true;
    CHECK(crossedCheap);
    NavDestroy(w);
}

static void test_ladder_and_two_worlds(void) {
    // World A: two levels joined by one ladder at (5,5). World B: wall splits the map.
    static StubMap a = {.w = 16, .h = 16, .d = 2};
    a.ladder[0][5][5] = a.ladder[1][5][5] = 1;
    static StubMap b = {.w = 16, .h = 16, .d = 1};
    for (int y = 0; y < 16; y++) b.wall[0][y][8] = 1;

    NavWorld* wa = NavCreate(16, 16, 2, 8, 8, &a);
    NavWorld* wb = NavCreate(16, 16, 1, 8, 8, &b);
    NavBuildEntrances(wa); NavBuildGraph(wa);
    NavBuildEntrances(wb); NavBuildGraph(wb);

    CHECK(wa->ladderNum == 1);
    int la = NavFindPathHPA(wa, (NavPoint){0, 0, 0}, (NavPoint){15, 15, 1}, buf, 4096);
    CHECK(la > 0 && buf[0].z == 1 && buf[la - 1].z == 0);
    int la2 = NavFindPathAStar(wa, (NavPoint){0, 0, 0}, (NavPoint){15, 15, 1}, buf, 4096);
    CHECK(la2 > 0);

    CHECK(NavFindPathAStar(wb, (NavPoint){0, 0, 0}, (NavPoint){15, 0, 0}, buf, 4096) == 0);
    CHECK(NavFindPathHPA(wb, (NavPoint){0, 0, 0}, (NavPoint){15, 0, 0}, buf, 4096) == 0);

    // Open the wall in B, mark dirty, incremental update finds the new route
    b.wall[0][3][8] = 0;
    NavMarkCellChanged(wb, 8, 3, 0);
    CHECK(wb->dirty);
    NavUpdateDirty(wb);
    CHECK(!wb->dirty);
    CHECK(NavFindPathHPA(wb, (NavPoint){0, 0, 0}, (NavPoint){15, 0, 0}, buf, 4096) > 0);

    NavDestroy(wa);
    NavDestroy(wb);
}

int main(void) {
    printf("Running nav standalone tests...\n");
    test_open_diagonal();
    test_variable_cost_detour();
    test_ladder_and_two_worlds();
    printf("\tFailed: %d\n", failures);
    return failures ? 1 : 0;
}
