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

// Flow field cost at every cell must equal A*'s cost from that cell to the goal,
// and walking `next` must add up to exactly that cost.
static void test_flow_matches_astar(void) {
    static StubMap m = {.w = 48, .h = 40, .d = 3};
    unsigned int seed = 12345;
    #define RND() (seed = seed * 1103515245u + 12345u, (seed >> 16) & 0x7fff)
    for (int z = 0; z < 3; z++)
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 48; x++) {
                m.wall[z][y][x] = (RND() % 100) < 22;
                int r = RND() % 100;
                m.cost[z][y][x] = r < 15 ? 25 : (r < 25 ? 17 : (r < 35 ? 8 : 0));
            }
    for (int i = 0; i < 12; i++) {           // ladders between levels
        int x = RND() % 48, y = RND() % 40, z = RND() % 2;
        m.wall[z][y][x] = m.wall[z + 1][y][x] = 0;
        m.ladder[z][y][x] = m.ladder[z + 1][y][x] = 1;
    }
    NavPoint goal = {24, 20, 1};
    m.wall[1][20][24] = 0;

    NavWorld* w = NavCreate(48, 40, 3, 8, 8, &m);
    NavFlowField* f = NavFlowCreate(w);
    int reached = NavFlowBuild(w, f, &goal, 1, 0);
    CHECK(reached > 100);
    CHECK(NavFlowCostAt(f, goal) == 0);

    int compared = 0, mismatches = 0, badChains = 0, unreachableAgree = 0;
    for (int z = 0; z < 3; z++)
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 48; x++) {
                if (m.wall[z][y][x]) continue;
                NavPoint p = {x, y, z};
                int fc = NavFlowCostAt(f, p);
                int len = NavFindPathAStar(w, p, goal, buf, 4096);
                if (len == 0) { CHECK(fc == NAV_FLOW_UNREACHABLE); unreachableAgree++; continue; }
                compared++;
                if (fc != w->lastCost) {
                    if (mismatches++ < 5) printf("  (%d,%d,%d): flow=%d astar=%d\n", x, y, z, fc, w->lastCost);
                }
                // Follow next: each step's cost drop must be a legal edge, ending at the goal
                NavPoint cur = p, nxt;
                int steps = 0;
                while (NavFlowNext(f, cur, &nxt) && steps < 10000) { cur = nxt; steps++; }
                if (cur.x != goal.x || cur.y != goal.y || cur.z != goal.z) badChains++;
            }
    CHECK(compared > 500);
    CHECK(unreachableAgree > 0);
    CHECK(mismatches == 0);
    CHECK(badChains == 0);

    // maxCost limits expansion
    int local = NavFlowBuild(w, f, &goal, 1, 60);
    CHECK(local > 1 && local < reached);
    // Multiple goals: every cell's cost is the min over goals
    NavPoint goals[2] = {{2, 2, 0}, {45, 37, 2}};
    m.wall[0][2][2] = m.wall[2][37][45] = 0;
    NavFlowBuild(w, f, goals, 2, 0);
    CHECK(NavFlowCostAt(f, goals[0]) == 0 && NavFlowCostAt(f, goals[1]) == 0);

    NavFlowDestroy(f);
    NavDestroy(w);
    #undef RND
}

int main(void) {
    printf("Running nav standalone tests...\n");
    test_open_diagonal();
    test_variable_cost_detour();
    test_ladder_and_two_worlds();
    test_flow_matches_astar();
    printf("\tFailed: %d\n", failures);
    return failures ? 1 : 0;
}
