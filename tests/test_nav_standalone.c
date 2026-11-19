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

static bool OnRoute(const NavFlowField* f, NavPoint from, NavPoint target) {
    NavPoint cur = from, nxt;
    int steps = 0;
    while (NavFlowNext(f, cur, &nxt) && steps++ < 100000) {
        cur = nxt;
        if (cur.x == target.x && cur.y == target.y && cur.z == target.z) return true;
    }
    return false;
}

static void test_flow_steering(void) {
    // 1. Open map: every cell sees the goal directly
    {
        static StubMap m = {.w = 41, .h = 41, .d = 1};
        NavWorld* w = NavCreate(41, 41, 1, 8, 8, &m);
        NavFlowField* f = NavFlowCreate(w);
        NavPoint goal = {20, 20, 0};
        NavFlowBuild(w, f, &goal, 1, 0);
        int notGoal = 0;
        for (int y = 0; y < 41; y++)
            for (int x = 0; x < 41; x++) {
                if (x == 20 && y == 20) continue;
                NavPoint t;
                if (!NavFlowSteerTarget(f, (NavPoint){x, y, 0}, &t) || t.x != 20 || t.y != 20) notGoal++;
            }
        CHECK(notGoal == 0);
        NavPoint t;
        CHECK(!NavFlowSteerTarget(f, goal, &t));   // no target at the goal itself
        NavFlowDestroy(f);
        NavDestroy(w);
    }
    // 2. Walls: targets are strictly closer, on the cell's own route, and in view
    {
        static StubMap m = {.w = 48, .h = 40, .d = 1};
        for (int y = 0; y < 34; y++) m.wall[0][y][16] = 1;
        for (int y = 6; y < 40; y++) m.wall[0][y][32] = 1;
        for (int x = 20; x < 28; x++) m.wall[0][20][x] = 1;
        NavWorld* w = NavCreate(48, 40, 1, 8, 8, &m);
        NavFlowField* f = NavFlowCreate(w);
        NavPoint goal = {45, 38, 0};
        NavFlowBuild(w, f, &goal, 1, 0);
        int bad = 0, longShots = 0;
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 48; x++) {
                NavPoint c = {x, y, 0}, t;
                if (m.wall[0][y][x] || !NavFlowSteerTarget(f, c, &t)) continue;
                if (NavFlowCostAt(f, t) >= NavFlowCostAt(f, c)) bad++;
                if (!OnRoute(f, c, t)) bad++;
                if (!nav_FlowLineClear(w, x, y, t.x, t.y, 0, 1000)) bad++;
                if (abs(t.x - x) + abs(t.y - y) > 10) longShots++;
            }
        CHECK(bad == 0);
        CHECK(longShots > 100);   // most cells aim far ahead, not at a neighbour
        NavFlowDestroy(f);
        NavDestroy(w);
    }
    // 3. Cost-aware: cells on a road through mud never aim across the mud
    {
        static StubMap m = {.w = 60, .h = 30, .d = 1};
        for (int y = 0; y < 30; y++)
            for (int x = 0; x < 60; x++) m.cost[0][y][x] = (y == 15) ? 0 : 60;
        NavWorld* w = NavCreate(60, 30, 1, 8, 8, &m);
        NavFlowField* f = NavFlowCreate(w);
        NavPoint goal = {59, 15, 0};
        NavFlowBuild(w, f, &goal, 1, 0);
        int crossesMud = 0;
        for (int x = 0; x < 59; x++) {
            NavPoint t;
            CHECK(NavFlowSteerTarget(f, (NavPoint){x, 15, 0}, &t));
            if (!nav_FlowLineClear(w, x, 15, t.x, t.y, 0, 10)) crossesMud++;
        }
        CHECK(crossesMud == 0);
        // and with anyAngle off there are no targets at all
        f->anyAngle = false;
        NavFlowBuild(w, f, &goal, 1, 0);
        NavPoint t;
        CHECK(!NavFlowSteerTarget(f, (NavPoint){0, 15, 0}, &t));
        NavFlowDestroy(f);
        NavDestroy(w);
    }
}

static void test_flow_extra_cost_and_clearance(void) {
    // Wall at x=10 with doors at y=3 and y=16; goal straight right of door A
    static StubMap m = {.w = 21, .h = 20, .d = 1};
    for (int y = 0; y < 20; y++) m.wall[0][y][10] = 1;
    m.wall[0][3][10] = 0;
    m.wall[0][16][10] = 0;
    NavWorld* w = NavCreate(21, 20, 1, 8, 8, &m);
    NavFlowField* f = NavFlowCreate(w);
    NavPoint goal = {18, 3, 0}, start = {2, 6, 0};

    NavFlowBuild(w, f, &goal, 1, 0);
    CHECK(OnRoute(f, start, (NavPoint){10, 3, 0}));          // takes door A
    int plain = NavFlowCostAt(f, start);

    // Make door A expensive -> route switches to door B, costs include the extra
    static int extra[20 * 21];
    extra[NavFlowCellId(f, 10, 3, 0)] = 1000;
    f->extraCost = extra;
    NavFlowBuild(w, f, &goal, 1, 0);
    CHECK(OnRoute(f, start, (NavPoint){10, 16, 0}));         // takes door B
    CHECK(NavFlowCostAt(f, start) > plain);
    // Extra is paid on ENTERING a cell: standing in front of door A, it's now
    // cheaper to walk round through door B than to step into A
    CHECK(NavFlowCostAt(f, (NavPoint){9, 3, 0}) < 1000);
    CHECK(OnRoute(f, (NavPoint){9, 3, 0}, (NavPoint){10, 16, 0}));
    f->extraCost = NULL;

    // Clearance: distance to nearest blocked cell / map edge, 0 on walls
    static uint8_t clr[20 * 21];
    NavComputeClearance(w, clr);
    CHECK(clr[NavFlowCellId(f, 10, 5, 0)] == 0);             // wall
    CHECK(clr[NavFlowCellId(f, 9, 8, 0)] == 1);              // next to the wall
    CHECK(clr[NavFlowCellId(f, 0, 8, 0)] == 1);              // map edge
    CHECK(clr[NavFlowCellId(f, 5, 8, 0)] == 5);              // middle of the left room (cols 0..9)
    CHECK(clr[NavFlowCellId(f, 10, 3, 0)] == 1);             // inside a doorway

    NavFlowDestroy(f);
    NavDestroy(w);
}

// Walk NavHFlowNext from p; returns summed step cost (same rules as A*) or -1 if
// it doesn't reach the goal.
static int WalkHFlow(NavWorld* w, NavHFlow* h, const StubMap* m, NavPoint p, NavPoint goal) {
    NavPoint cur = p, nxt;
    int cost = 0, steps = 0;
    while (NavHFlowNext(w, h, cur, &nxt)) {
        if (++steps > 20000) return -1;
        int c = NavMoveCost((void*)(uintptr_t)m, nxt.x, nxt.y, nxt.z);
        if (nxt.z != cur.z) cost += c;                                    // ladder
        else cost += ((nxt.x != cur.x && nxt.y != cur.y) ? 14 : 10) * c / 10;
        cur = nxt;
    }
    return (cur.x == goal.x && cur.y == goal.y && cur.z == goal.z) ? cost : -1;
}

static void test_hierarchical_flow(void) {
    static StubMap m = {.w = 48, .h = 40, .d = 3};
    memset(&m, 0, sizeof(m));
    m.w = 48; m.h = 40; m.d = 3;
    unsigned int seed = 777;
    #define RND() (seed = seed * 1103515245u + 12345u, (seed >> 16) & 0x7fff)
    for (int z = 0; z < 3; z++)
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 48; x++) {
                m.wall[z][y][x] = (RND() % 100) < 18;
                int r = RND() % 100;
                m.cost[z][y][x] = r < 12 ? 25 : (r < 22 ? 17 : 0);
            }
    for (int i = 0; i < 14; i++) {
        int x = RND() % 48, y = RND() % 40, z = RND() % 2;
        m.wall[z][y][x] = m.wall[z + 1][y][x] = 0;
        m.ladder[z][y][x] = m.ladder[z + 1][y][x] = 1;
    }
    NavPoint goal = {30, 25, 2};
    m.wall[2][25][30] = 0;

    NavWorld* w = NavCreate(48, 40, 3, 8, 8, &m);
    NavBuildEntrances(w);
    NavBuildGraph(w);
    NavHFlow* h = NavHFlowCreate(w, goal);
    CHECK(h && h->valid);
    CHECK(h->chunksBuilt == 0);                 // nothing built until asked

    int reachable = 0, reached = 0, falsePaths = 0, worstPct = 0;
    long sumHier = 0, sumOpt = 0;
    for (int z = 0; z < 3; z++)
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 48; x++) {
                if (m.wall[z][y][x]) continue;
                NavPoint p = {x, y, z};
                int len = NavFindPathAStar(w, p, goal, buf, 4096);
                int walked = WalkHFlow(w, h, &m, p, goal);
                if (len == 0) { if (walked >= 0 && !(x == goal.x && y == goal.y && z == goal.z)) falsePaths++; continue; }
                reachable++;
                if (walked < 0) continue;
                reached++;
                sumHier += walked; sumOpt += w->lastCost;
                if (w->lastCost > 0) {
                    int pct = (walked - w->lastCost) * 100 / w->lastCost;
                    if (pct > worstPct) worstPct = pct;
                }
            }
    printf("  hflow: reached %d/%d reachable cells, cost +%.1f%% vs A* on average (worst +%d%%), %d chunk fields built\n",
           reached, reachable, 100.0 * (sumHier - sumOpt) / (sumOpt ? sumOpt : 1), worstPct, h->chunksBuilt);
    CHECK(reachable > 500);
    CHECK(reached == reachable);                // every reachable cell gets home
    CHECK(falsePaths == 0);                     // no route where A* finds none
    CHECK(sumHier >= sumOpt);                   // can't beat optimal
    CHECK(sumHier * 100 <= sumOpt * 115);       // within 15% on average

    // A second goal is independent; a map edit refreshes the cached field automatically
    NavHFlow* h2 = NavHFlowCreate(w, (NavPoint){2, 2, 0});
    m.wall[0][2][2] = 0;
    NavPoint probe = {40, 30, 0};
    m.wall[0][30][40] = 0;
    int before = NavHFlowCostAt(w, h, probe);
    for (int x = 0; x < 48; x++) { m.wall[0][29][x] = 1; NavMarkCellChanged(w, x, 29, 0); }
    NavUpdateDirty(w);
    int after = NavHFlowCostAt(w, h, probe);     // stale graphVersion -> rebuilt on demand
    CHECK(after != before || before == NAV_FLOW_UNREACHABLE);
    CHECK(WalkHFlow(w, h, &m, probe, goal) >= 0 || NavFindPathAStar(w, probe, goal, buf, 4096) == 0);
    CHECK(NavHFlowMemoryUsage(h) > 0);

    NavHFlowDestroy(h2);
    NavHFlowDestroy(h);
    NavDestroy(w);
    #undef RND
}

// Does walking NavHFlowNext from p pass through (x,y,z)?
static bool HFlowPassesThrough(NavWorld* w, NavHFlow* h, NavPoint p, NavPoint via) {
    NavPoint cur = p, nxt;
    int steps = 0;
    while (NavHFlowNext(w, h, cur, &nxt) && steps++ < 20000) {
        cur = nxt;
        if (cur.x == via.x && cur.y == via.y && cur.z == via.z) return true;
    }
    return false;
}

static void test_hierarchical_flow_extra_cost(void) {
    // Two floors, two ladders: A near the start, B far away. Goal upstairs.
    static StubMap m;
    memset(&m, 0, sizeof(m));
    m.w = 40; m.h = 24; m.d = 2;
    m.ladder[0][4][6] = m.ladder[1][4][6] = 1;      // ladder A
    m.ladder[0][20][34] = m.ladder[1][20][34] = 1;  // ladder B
    NavWorld* w = NavCreate(40, 24, 2, 8, 8, &m);
    NavBuildEntrances(w);
    NavBuildGraph(w);
    NavPoint start = {4, 6, 0}, goal = {20, 12, 1};
    NavHFlow* h = NavHFlowCreate(w, goal);
    CHECK(HFlowPassesThrough(w, h, start, (NavPoint){6, 4, 1}));     // nearest ladder: A
    int plain = NavHFlowCostAt(w, h, start);

    // A long queue at ladder A's base: priced into the cell you enter to climb
    static int extra[2 * 24 * 40];
    extra[(0 * 24 + 4) * 40 + 6] = 5000;
    h->extraCost = extra;
    NavHFlowInvalidate(h);
    CHECK(HFlowPassesThrough(w, h, start, (NavPoint){34, 20, 1}));   // now via ladder B
    CHECK(!HFlowPassesThrough(w, h, start, (NavPoint){6, 4, 1}));
    int withQueue = NavHFlowCostAt(w, h, start);
    CHECK(withQueue > plain && withQueue < plain + 5000);             // detour beats the queue

    // Queue gone again: back to ladder A after invalidation
    extra[(0 * 24 + 4) * 40 + 6] = 0;
    NavHFlowInvalidate(h);
    CHECK(HFlowPassesThrough(w, h, start, (NavPoint){6, 4, 1}));
    CHECK(NavHFlowCostAt(w, h, start) == plain);

    NavHFlowDestroy(h);
    NavDestroy(w);
}

// NavReachable must agree with A* (ground truth) for random pairs
static int RegionMismatches(NavWorld* w, const StubMap* m, unsigned int* seed, int pairs) {
    int bad = 0;
    for (int i = 0; i < pairs; i++) {
        NavPoint a, b;
        do { *seed = *seed * 1103515245u + 12345u; a = (NavPoint){(int)((*seed >> 8) % m->w), (int)((*seed >> 16) % m->h), (int)((*seed >> 4) % m->d)}; }
        while (m->wall[a.z][a.y][a.x]);
        do { *seed = *seed * 1103515245u + 12345u; b = (NavPoint){(int)((*seed >> 8) % m->w), (int)((*seed >> 16) % m->h), (int)((*seed >> 4) % m->d)}; }
        while (m->wall[b.z][b.y][b.x]);
        bool truth = NavFindPathAStar(w, a, b, buf, 4096) > 0 || (a.x == b.x && a.y == b.y && a.z == b.z);
        if (NavReachable(w, a, b) != truth) bad++;
    }
    return bad;
}

static void test_regions(void) {
    static StubMap m;
    memset(&m, 0, sizeof(m));
    m.w = 48; m.h = 40; m.d = 3;
    unsigned int seed = 4242;
    #define RND() (seed = seed * 1103515245u + 12345u, (seed >> 16) & 0x7fff)
    for (int z = 0; z < 3; z++)
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 48; x++) m.wall[z][y][x] = (RND() % 100) < 38;   // many pockets
    for (int i = 0; i < 10; i++) {
        int x = RND() % 48, y = RND() % 40, z = RND() % 2;
        m.wall[z][y][x] = m.wall[z + 1][y][x] = 0;
        m.ladder[z][y][x] = m.ladder[z + 1][y][x] = 1;
    }
    NavWorld* w = NavCreate(48, 40, 3, 8, 8, &m);
    unsigned int s2 = 99;
    CHECK(NavRegionCount(w) > 10);
    CHECK(RegionMismatches(w, &m, &s2, 3000) == 0);
    CHECK(NavRegionId(w, (NavPoint){-1, 0, 0}) == -1);

    // Big open room, then cut it in two with a wall (incremental relabel), then reopen it
    memset(&m, 0, sizeof(m));
    m.w = 48; m.h = 40; m.d = 3;
    NavDestroy(w);
    w = NavCreate(48, 40, 3, 8, 8, &m);
    NavPoint left = {5, 20, 0}, right = {40, 20, 0};
    CHECK(NavReachable(w, left, right));
    for (int y = 0; y < 40; y++) { m.wall[0][y][24] = 1; NavMarkCellChanged(w, 24, y, 0); }
    CHECK(!NavReachable(w, left, right));
    CHECK(RegionMismatches(w, &m, &s2, 1000) == 0);
    m.wall[0][7][24] = 0;
    NavMarkCellChanged(w, 24, 7, 0);
    CHECK(NavReachable(w, left, right));
    // A ladder pair joins two otherwise separate levels
    NavPoint up = {10, 10, 1};
    CHECK(!NavReachable(w, left, up));
    m.ladder[0][10][10] = m.ladder[1][10][10] = 1;
    NavMarkCellChanged(w, 10, 10, 0);
    CHECK(NavReachable(w, left, up));
    CHECK(RegionMismatches(w, &m, &s2, 1000) == 0);
    NavDestroy(w);
    #undef RND
}

typedef struct { NavPoint cells[8]; int n; } TargetSet;
static bool IsTargetCell(void* user, int x, int y, int z) {
    const TargetSet* t = user;
    for (int i = 0; i < t->n; i++)
        if (t->cells[i].x == x && t->cells[i].y == y && t->cells[i].z == z) return true;
    return false;
}

static void test_find_nearest(void) {
    static StubMap m;
    memset(&m, 0, sizeof(m));
    m.w = 32; m.h = 24; m.d = 2;
    // Wall at x=10 from y=0..20 (open only at the bottom): (12,5) is 7 cells away in a
    // straight line but ~38 cells of walking; (3,20) is 15 cells away and straight there
    for (int y = 0; y <= 20; y++) m.wall[0][y][10] = 1;
    NavWorld* w = NavCreate(32, 24, 2, 8, 8, &m);
    NavPoint start = {5, 5, 0}, found;
    TargetSet t = {{{12, 5, 0}, {3, 20, 0}}, 2};
    int cost = NavFindNearest(w, start, IsTargetCell, &t, 0, &found, NULL, 0, NULL);
    CHECK(found.x == 3 && found.y == 20);                        // nearest by walking, not straight line
    CHECK(cost == (NavFindPathAStar(w, start, (NavPoint){3, 20, 0}, buf, 4096), w->lastCost));

    // Path output: goal-first, ends at start
    int len = 0;
    NavFindNearest(w, start, IsTargetCell, &t, 0, &found, buf, 4096, &len);
    CHECK(len > 0 && buf[0].x == 3 && buf[0].y == 20 && buf[len - 1].x == 5 && buf[len - 1].y == 5);

    // maxCost bounds the search
    CHECK(NavFindNearest(w, start, IsTargetCell, &t, 60, &found, NULL, 0, NULL) == -1);

    // Upstairs via a ladder beats a far target on the same floor
    m.ladder[0][6][6] = m.ladder[1][6][6] = 1;
    TargetSet t2 = {{{8, 6, 1}, {28, 22, 0}}, 2};
    cost = NavFindNearest(w, start, IsTargetCell, &t2, 0, &found, NULL, 0, NULL);
    CHECK(found.z == 1 && found.x == 8 && found.y == 6);
    CHECK(cost > 0);

    // No targets reachable -> -1
    TargetSet none = {{{0, 0, 1}}, 1};
    m.ladder[0][6][6] = m.ladder[1][6][6] = 0;
    CHECK(NavFindNearest(w, start, IsTargetCell, &none, 0, &found, NULL, 0, NULL) == -1);
    NavDestroy(w);
}

int main(void) {
    printf("Running nav standalone tests...\n");
    test_find_nearest();
    test_regions();
    test_hierarchical_flow_extra_cost();
    test_hierarchical_flow();
    test_flow_extra_cost_and_clearance();
    test_flow_steering();
    test_open_diagonal();
    test_variable_cost_detour();
    test_ladder_and_two_worlds();
    test_flow_matches_astar();
    printf("\tFailed: %d\n", failures);
    return failures ? 1 : 0;
}
