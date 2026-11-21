// test_nav_fuzz.c — randomized adversarial checks of the nav library.
//
// Generates many small worlds built to hit the awkward cases (walls and gaps on
// chunk borders, mazes, serpentines, ladders and one-way ramp entries on
// borders and corners, game-style hills with ramps inside chunks, chunk sizes that
// don't divide the map, mud, 4/8-dir) and
// checks every algorithm against ground truth:
//
//   A*        every step legal, cost adds up, equals the full flow field
//   flow      NavFlowField cost == A* on samples (then used as truth for all cells)
//   hflow     from EVERY cell: reaches the goal iff reachable, never loops, every
//             step legal, optimal <= walked cost <= NavHFlowCostAt (the field
//             never promises less than walking delivers: that's what traps agents)
//   hpa       reaches iff A* does, legal steps, cost >= optimal
//   regions   never "unreachable" when A* finds a path
//   nearest   NavFindNearest == cheapest A* among targets
//
// Then edits the world (walls, ladders, ramps, mud; biased to chunk borders),
// updates incrementally, checks the graph equals a fresh full build (same
// entrances, same directed edges and costs), and checks everything again on the
// SAME NavHFlow and regions (stale-state bugs).
//
// Usage: test_nav_fuzz [worlds] [firstSeed] [-v]
//   make test runs 60 worlds (~7 s). Deep sweep: test_nav_fuzz 2000 1000 (~3 min).
//   Every failure prints its seed; rerun one world with: test_nav_fuzz 1 <seed> -v
// Debug switches (environment):
//   FZ_DUMP=1        print the map, goals and the abstract graph's entrance values
//   FZ_DUMPROUND=n   print the map after edit round n
//   FZ_HPA=1         explain HPA* misses (connections, graph reachability, A* path)
//   FZ_ENT=i         with -v, print the edges around entrance i at an hflow failure
//   FZ_EDITS=1       print each edit
//   FZ_FULL=1        full rebuild instead of incremental after edits (bisects
//                    incremental-update bugs from everything else)

#define NAV_HOST_HEADER "../tests/nav_fuzz_host.h"
#include "../nav/nav.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// RNG
static unsigned long long rngState;
static unsigned Rnd(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 7; rngState ^= rngState << 17;
    return (unsigned)(rngState >> 11);
}
static int RndRange(int lo, int hi) { return lo + (int)(Rnd() % (unsigned)(hi - lo + 1)); }  // inclusive
static bool Chance(int pct) { return (int)(Rnd() % 100) < pct; }

// ---------------------------------------------------------------------------
// Failure reporting
static int failures, worldFailures;
static bool verbose;
static char worldDesc[256];
static const char* phase = "";
#define FAIL(...) do { failures++; if (worldFailures++ < 4) { printf("  FAIL [%s] %s: ", worldDesc, phase); printf(__VA_ARGS__); printf("\n"); } } while (0)

static FuzzMap map;
static NavPoint pbuf[FZ_W * FZ_H * FZ_D];

// ---------------------------------------------------------------------------
// Optional extra cost per cell (same layout and meaning as NavFlowField.extraCost:
// added to the cell's move cost, so diagonal and ramp steps scale it by 1.4)
static const int* stepExtra;
static int CellCost(void* host, int x, int y, int z) {
    const FuzzMap* m = host;
    return NavMoveCost(host, x, y, z) + (stepExtra ? stepExtra[(z * m->h + y) * m->w + x] : 0);
}

// Step legality: exactly A*'s move rules. Returns the step cost or -1.
static int StepCost(NavWorld* w, NavPoint a, NavPoint b) {
    void* host = w->host;
    int best = -1;
    int dx = b.x - a.x, dy = b.y - a.y;
    if (a.z == b.z) {
        if (abs(dx) > 1 || abs(dy) > 1 || (dx == 0 && dy == 0)) return -1;
        bool diag = dx != 0 && dy != 0;
        if (diag && !w->eightDir) return -1;
        if (!NavIsWalkable(host, b.x, b.y, b.z)) return -1;
        if (diag && (!NavIsWalkable(host, b.x, a.y, a.z) || !NavIsWalkable(host, a.x, b.y, a.z))) return -1;
        if (!NavCanEnterFrom(host, b.x, b.y, b.z, a.x, a.y)) return -1;
        return ((diag ? 14 : 10) * CellCost(host, b.x, b.y, b.z)) / 10;
    }
    int climbExtra = fz_ClimbExtra(a.x, a.y, a.z, b.z > a.z ? 1 : -1);   // NAV_CLIMB_EXTRA
    if (dx == 0 && dy == 0) {
        if (b.z == a.z + 1 && NavCanClimbUp(host, a.x, a.y, a.z)) best = CellCost(host, b.x, b.y, b.z) + climbExtra;
        if (b.z == a.z - 1 && NavCanClimbDown(host, a.x, a.y, a.z)) best = CellCost(host, b.x, b.y, b.z) + climbExtra;
    }
    int ex, ey, hdx, hdy;
    if (b.z == a.z + 1 && NavRampUp(host, a.x, a.y, a.z, &ex, &ey) && ex == b.x && ey == b.y) {
        int c = (14 * CellCost(host, b.x, b.y, b.z)) / 10 + climbExtra;
        if (best < 0 || c < best) best = c;
    }
    if (b.z == a.z - 1 && abs(dx) + abs(dy) == 1 && NavRampHighSide(host, b.x, b.y, b.z, &hdx, &hdy)
        && b.x + hdx == a.x && b.y + hdy == a.y && NavIsWalkable(host, b.x, b.y, b.z)) {
        int c = (14 * CellCost(host, b.x, b.y, b.z)) / 10 + climbExtra;
        if (best < 0 || c < best) best = c;
    }
    return best;
}

// Cost of a goal-first path, -1 if any step is illegal (reports where)
static int GoalFirstPathCost(NavWorld* w, const NavPoint* p, int len, const char* what) {
    int c = 0;
    for (int k = len - 1; k > 0; k--) {
        int s = StepCost(w, p[k], p[k - 1]);
        if (s < 0) {
            FAIL("%s: illegal step (%d,%d,%d)->(%d,%d,%d)", what, p[k].x, p[k].y, p[k].z, p[k - 1].x, p[k - 1].y, p[k - 1].z);
            return -1;
        }
        c += s;
    }
    return c;
}

// ---------------------------------------------------------------------------
// World generation
static int chunkW, chunkH;

static void GenRandomWalls(int z, int pct) {
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) map.wall[z][y][x] = Chance(pct);
}

// Walls along every chunk border line with a few gaps per segment (gaps often at
// segment ends = chunk corners), plus some noise
static void GenBorderWalls(int z) {
    GenRandomWalls(z, RndRange(0, 15));
    for (int x = 0; x < map.w; x++)
        for (int y = 0; y < map.h; y++)
            if ((x % chunkW == 0 && x > 0) || (y % chunkH == 0 && y > 0)) map.wall[z][y][x] = 1;
    // gaps: vertical lines
    for (int bx = chunkW; bx < map.w; bx += chunkW)
        for (int sy = 0; sy < map.h; sy += chunkH) {
            int n = RndRange(0, 2);
            for (int g = 0; g < n; g++) {
                int y = Chance(40) ? (Chance(50) ? sy : sy + chunkH - 1) : sy + RndRange(0, chunkH - 1);
                if (y < map.h) map.wall[z][y][bx] = 0;
            }
        }
    for (int by = chunkH; by < map.h; by += chunkH)
        for (int sx = 0; sx < map.w; sx += chunkW) {
            int n = RndRange(0, 2);
            for (int g = 0; g < n; g++) {
                int x = Chance(40) ? (Chance(50) ? sx : sx + chunkW - 1) : sx + RndRange(0, chunkW - 1);
                if (x < map.w) map.wall[z][by][x] = 0;
            }
        }
}

// Perfect maze (recursive backtracker) on a 2-cell lattice, optional loops
static void GenMaze(int z) {
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) map.wall[z][y][x] = 1;
    int cw = (map.w + 1) / 2, ch = (map.h + 1) / 2;
    static int stack[FZ_W * FZ_H];
    static unsigned char seen[FZ_H][FZ_W];
    memset(seen, 0, sizeof(seen));
    int sp = 0;
    stack[sp++] = 0;
    seen[0][0] = 1;
    map.wall[z][0][0] = 0;
    while (sp > 0) {
        int c = stack[sp - 1], cx = c % cw, cy = c / cw;
        int opts[4], n = 0;
        static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = cx + ddx[d], ny = cy + ddy[d];
            if (nx >= 0 && ny >= 0 && nx < cw && ny < ch && 2 * nx < map.w && 2 * ny < map.h && !seen[ny][nx]) opts[n++] = d;
        }
        if (n == 0) { sp--; continue; }
        int d = opts[Rnd() % (unsigned)n];
        int nx = cx + ddx[d], ny = cy + ddy[d];
        seen[ny][nx] = 1;
        map.wall[z][2 * ny][2 * nx] = 0;
        map.wall[z][cy * 2 + ddy[d]][cx * 2 + ddx[d]] = 0;
        stack[sp++] = ny * cw + nx;
    }
    int loops = RndRange(0, map.w * map.h / 40);
    for (int i = 0; i < loops; i++) map.wall[z][RndRange(0, map.h - 1)][RndRange(0, map.w - 1)] = 0;
}

// Long horizontal walls with one gap at alternating ends: a single winding route
// that crosses the same chunk border back and forth
static void GenSerpentine(int z) {
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) map.wall[z][y][x] = 0;
    int gap = RndRange(1, 3);
    bool left = Chance(50);
    for (int y = RndRange(1, 2); y < map.h; y += gap + 1) {
        for (int x = 0; x < map.w; x++) map.wall[z][y][x] = 1;
        map.wall[z][y][left ? 0 : map.w - 1] = 0;
        left = !left;
    }
    if (Chance(50)) {   // transpose-ish: also some vertical cuts with gaps
        int x = RndRange(1, map.w - 2);
        for (int y = 0; y < map.h; y++) if (!Chance(15)) map.wall[z][y][x] = 1;
    }
}

static void GenRooms(int z) {
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) map.wall[z][y][x] = 0;
    int rw = RndRange(3, 7), rh = RndRange(3, 7);
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++)
            if (x % rw == 0 || y % rh == 0) map.wall[z][y][x] = 1;
    // doors
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++)
            if (map.wall[z][y][x] && (x % rw != 0 || y % rh != 0) && Chance(18)) map.wall[z][y][x] = 0;
}

static void FzPlaceLadder(int x, int y, int z) {
    if (z + 1 >= map.d) return;
    map.wall[z][y][x] = map.wall[z + 1][y][x] = 0;
    map.ramp[z][y][x] = map.ramp[z + 1][y][x] = 0;
    map.ladder[z][y][x] = map.ladder[z + 1][y][x] = 1;
}

static void FzPlaceRamp(int x, int y, int z, int dir) {
    static const int ox[5] = {0, 0, 1, 0, -1}, oy[5] = {0, -1, 0, 1, 0};
    int ex = x + ox[dir], ey = y + oy[dir];
    if (z + 1 >= map.d || ex < 0 || ey < 0 || ex >= map.w || ey >= map.h) return;
    map.wall[z][y][x] = 0;
    map.ladder[z][y][x] = 0;
    map.ramp[z][y][x] = (unsigned char)dir;
    if (Chance(85)) map.wall[z + 1][ey][ex] = 0;           // usually a usable exit
    if (Chance(70)) {                                      // usually reachable low side
        int lx = x - ox[dir], ly = y - oy[dir];
        if (lx >= 0 && ly >= 0 && lx < map.w && ly < map.h) map.wall[z][ly][lx] = 0;
    }
}

// A random cell, biased to chunk borders and corners
static void BiasedCell(int* x, int* y) {
    int r = RndRange(0, 9);
    if (r < 3) {        // corner region
        int cx = RndRange(0, (map.w - 1) / chunkW), cy = RndRange(0, (map.h - 1) / chunkH);
        *x = cx * chunkW + (Chance(50) ? 0 : chunkW - 1) + RndRange(-1, 1);
        *y = cy * chunkH + (Chance(50) ? 0 : chunkH - 1) + RndRange(-1, 1);
    } else if (r < 6) { // on a border line
        *x = RndRange(0, map.w - 1);
        *y = RndRange(0, map.h - 1);
        if (Chance(50)) *x = (*x / chunkW) * chunkW + (Chance(50) ? 0 : chunkW - 1);
        else *y = (*y / chunkH) * chunkH + (Chance(50) ? 0 : chunkH - 1);
    } else {
        *x = RndRange(0, map.w - 1);
        *y = RndRange(0, map.h - 1);
    }
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x >= map.w) *x = map.w - 1;
    if (*y >= map.h) *y = map.h - 1;
}

// Game-style hills: a heightmap where each column has ONE walkable cell, on top of
// its ground (everything below and above is solid), so the only ways between heights
// are ramps on the slopes and ladders on cliffs. Ramps sit everywhere inside chunks,
// and routes often have to climb and come back down within one chunk (around a pit,
// over a ridge). Some columns get a cave cell under the surface, and some tunnels.
// This is the terrain the 3D chunk search is about: today every one of those ramps
// is a graph node.
static int hillH[FZ_H][FZ_W];
static void GenHills(void) {
    for (int z = 0; z < map.d; z++)
        for (int y = 0; y < map.h; y++)
            for (int x = 0; x < map.w; x++) map.wall[z][y][x] = 1;
    // smooth-ish heightmap: a few random blobs raise the ground, then terraced
    int blobs = RndRange(2, 10);
    static int acc[FZ_H][FZ_W];
    memset(acc, 0, sizeof(acc));
    for (int b = 0; b < blobs; b++) {
        int cx = RndRange(0, map.w - 1), cy = RndRange(0, map.h - 1), r = RndRange(3, 18);
        int up = Chance(75) ? 1 : -1;
        for (int y = 0; y < map.h; y++)
            for (int x = 0; x < map.w; x++) {
                int d = abs(x - cx) + abs(y - cy);
                if (d < r) acc[y][x] += up * (r - d);
            }
    }
    int lo = 1 << 30, hi = -(1 << 30);
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) { if (acc[y][x] < lo) lo = acc[y][x]; if (acc[y][x] > hi) hi = acc[y][x]; }
    int span = hi - lo + 1;
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) {
            int h = (int)((long)(acc[y][x] - lo) * map.d / span);
            if (Chance(4)) h += RndRange(-1, 1);          // noise: lone bumps and pits
            if (h < 0) h = 0;
            if (h >= map.d) h = map.d - 1;
            hillH[y][x] = h;
            map.wall[h][y][x] = 0;
        }
    // ramps on slopes: a column next to one exactly one level higher. Not every
    // slope cell, so some steps are cliffs (some of those get a ladder instead)
    static const int ox[5] = {0, 0, 1, 0, -1}, oy[5] = {0, -1, 0, 1, 0};
    int rampPct = RndRange(30, 90);
    for (int y = 0; y < map.h; y++)
        for (int x = 0; x < map.w; x++) {
            int h = hillH[y][x];
            if (h + 1 >= map.d) continue;
            int start = RndRange(1, 4);
            for (int k = 0; k < 4; k++) {
                int dir = 1 + (start - 1 + k) % 4;
                int ex = x + ox[dir], ey = y + oy[dir];
                if (ex < 0 || ey < 0 || ex >= map.w || ey >= map.h || hillH[ey][ex] != h + 1) continue;
                if (Chance(rampPct)) { map.ramp[h][y][x] = (unsigned char)dir; break; }
                if (Chance(6)) { FzPlaceLadder(x, y, h); break; }   // opens (x,y,h+1): a ledge onto the cliff
            }
        }
    // ladder shafts down a cliff of several levels
    int shafts = RndRange(0, 4);
    for (int i = 0; i < shafts; i++) {
        int x, y;
        BiasedCell(&x, &y);
        int top = hillH[y][x], len = RndRange(1, 3);
        for (int z = top - 1; z >= 0 && z >= top - len; z--) FzPlaceLadder(x, y, z);
    }
    // caves: a walkable cell under the surface, often in runs (tunnels)
    int caves = Chance(50) ? RndRange(1, 6) : 0;
    for (int i = 0; i < caves; i++) {
        int x, y;
        BiasedCell(&x, &y);
        int dx = Chance(50), dy = !dx, len = RndRange(1, 20);
        for (int k = 0; k < len; k++, x += dx, y += dy) {
            if (x >= map.w || y >= map.h) break;
            int z = hillH[y][x] - RndRange(1, 2);
            if (z >= 0) map.wall[z][y][x] = 0;
        }
    }
}

static const char* kGenNames[] = {"random", "borders", "maze", "serpentine", "rooms", "terraces", "hills"};

static void GenerateWorld(int* outGen) {
    memset(&map, 0, sizeof(map));
    static const int sizes[] = {4, 5, 6, 7, 8, 16};
    chunkW = sizes[RndRange(0, 5)];
    chunkH = Chance(70) ? chunkW : sizes[RndRange(0, 5)];
    map.w = RndRange(chunkW + 2, FZ_W);
    map.h = RndRange(chunkH + 2, FZ_H);
    if (Chance(30)) map.w -= map.w % chunkW;          // sometimes exact multiples
    if (map.w < 3) map.w = 3;
    map.d = RndRange(1, 4);
    int gen = RndRange(0, 4);
    // terraces: several open-ish levels joined by many ramps and some ladders, so
    // routes keep choosing between ways up and down (3D heuristics, ramp edges)
    bool terraces = Chance(20);
    if (terraces) { gen = 5; map.d = RndRange(2, 4); }
    *outGen = gen;
    bool hills = Chance(25);
    if (hills) {
        *outGen = 6;
        map.d = RndRange(2, FZ_D);
        if (Chance(60)) { chunkW = chunkH = Chance(50) ? 8 : 16; }   // chunks big enough to hold whole slopes
        map.w = RndRange(chunkW + 2, FZ_W);
        map.h = RndRange(chunkH + 2, FZ_H);
        GenHills();
    }
    for (int z = 0; z < map.d && !hills; z++) {
        int g = Chance(75) ? gen : RndRange(0, 4);
        if (terraces) g = Chance(70) ? 0 : RndRange(0, 4);
        switch (g) {
            case 0: GenRandomWalls(z, terraces ? RndRange(5, 25) : RndRange(5, 42)); break;
            case 1: GenBorderWalls(z); break;
            case 2: GenMaze(z); break;
            case 3: GenSerpentine(z); break;
            default: GenRooms(z); break;
        }
    }
    // mud
    if (Chance(50)) {
        int patches = RndRange(1, 6);
        for (int i = 0; i < patches; i++) {
            int cx = RndRange(0, map.w - 1), cy = RndRange(0, map.h - 1), z = RndRange(0, map.d - 1);
            int r = RndRange(1, 5), c = Chance(30) ? RndRange(60, 250) : RndRange(12, 40);
            for (int y = cy - r; y <= cy + r; y++)
                for (int x = cx - r; x <= cx + r; x++)
                    if (x >= 0 && y >= 0 && x < map.w && y < map.h) map.cost[z][y][x] = (unsigned char)c;
        }
    }
    // vertical connections, often on borders/corners
    if (map.d > 1 && !hills) {
        int ladders = RndRange(0, 10), ramps = RndRange(0, 10);
        if (terraces) { ladders = RndRange(0, 6); ramps = RndRange(15, 60); }
        for (int i = 0; i < ladders; i++) { int x, y; BiasedCell(&x, &y); FzPlaceLadder(x, y, RndRange(0, map.d - 2)); }
        for (int i = 0; i < ramps; i++) { int x, y; BiasedCell(&x, &y); FzPlaceRamp(x, y, RndRange(0, map.d - 2), RndRange(1, 4)); }
    }
}

// ---------------------------------------------------------------------------
// Checks

static int CountWalkable(void) {
    int n = 0;
    for (int z = 0; z < map.d; z++)
        for (int y = 0; y < map.h; y++)
            for (int x = 0; x < map.w; x++) n += !map.wall[z][y][x];
    return n;
}

static NavPoint RandomWalkableCell(bool biased) {
    for (int t = 0; t < 4000; t++) {
        int x, y;
        if (biased) BiasedCell(&x, &y); else { x = RndRange(0, map.w - 1); y = RndRange(0, map.h - 1); }
        int z = RndRange(0, map.d - 1);
        if (!map.wall[z][y][x]) return (NavPoint){x, y, z};
    }
    return (NavPoint){-1, -1, -1};
}

// A goal that is a special cell: ladder or ramp if any, else biased
static NavPoint SpecialGoal(int kind) {
    for (int t = 0; t < 4000; t++) {
        int x = RndRange(0, map.w - 1), y = RndRange(0, map.h - 1), z = RndRange(0, map.d - 1);
        if (map.wall[z][y][x]) continue;
        if (kind == 0 && map.ladder[z][y][x]) return (NavPoint){x, y, z};
        if (kind == 1 && map.ramp[z][y][x]) return (NavPoint){x, y, z};
    }
    return RandomWalkableCell(true);
}

static int Cell(NavPoint p) { return (p.z * map.h + p.y) * map.w + p.x; }

typedef struct { NavPoint t[6]; int n; } Targets;
static bool IsTarget(void* user, int x, int y, int z) {
    const Targets* t = user;
    for (int i = 0; i < t->n; i++) if (t->t[i].x == x && t->t[i].y == y && t->t[i].z == z) return true;
    return false;
}

static int walked[FZ_W * FZ_H * FZ_D];      // -2 unknown, -1 on current walk, >= 0 cost to goal
static NavPoint walk[FZ_W * FZ_H * FZ_D];
static int* extraBuf;

static long statHierCells, statHierExtra, statHierOpt;
static long statInnerRamps, statExcursions, statAStarPaths, statFailShortcuts;

// A path that leaves a level and comes back to it without leaving the chunk: the
// case a chunk-local 3D search must find by itself (today ramp entrances do it)
static bool HasChunkExcursion(const NavPoint* p, int len) {
    int k = 0;
    while (k < len) {
        int cx = p[k].x / chunkW, cy = p[k].y / chunkH, z0 = p[k].z;
        bool left = false;
        int j = k + 1;
        for (; j < len && p[j].x / chunkW == cx && p[j].y / chunkH == cy; j++) {
            if (p[j].z != z0) left = true;
            else if (left) return true;
        }
        k = j;
    }
    return false;
}

static void CountInnerRamps(void) {
    for (int z = 0; z < map.d; z++)
        for (int y = 0; y < map.h; y++)
            for (int x = 0; x < map.w; x++) {
                if (!map.ramp[z][y][x] || map.wall[z][y][x]) continue;
                int lx = x % chunkW, ly = y % chunkH;
                if (lx > 0 && ly > 0 && lx < chunkW - 1 && ly < chunkH - 1) statInnerRamps++;
            }
}

// Debug: print the walk from p with the promise at each cell
static void TraceHFlow(NavWorld* w, NavHFlow* h, const NavFlowField* truth, NavPoint p) {
    NavPoint cur = p;
    for (int k = 0; k < 400; k++) {
        NavPoint nxt, tmp;
        bool field = nav_HFlowFieldStep(w, h, cur, &tmp);
        if (!NavHFlowNext(w, h, cur, &nxt)) break;
        int s = StepCost(w, cur, nxt);
        int a = NavHFlowCostAt(w, h, cur), b = NavHFlowCostAt(w, h, nxt);
        int oc = NavFlowCostAt(truth, cur), on = NavFlowCostAt(truth, nxt);
        if (oc - on > s) printf("    (%d,%d,%d) opt %d -> (%d,%d,%d) opt %d but step only %d <-- truth inconsistent\n", cur.x, cur.y, cur.z, oc, nxt.x, nxt.y, nxt.z, on, s);
        if (b + s > a || !field)
            printf("    (%d,%d,%d) promise %d opt %d -> (%d,%d,%d) promise %d step %d %s%s\n", cur.x, cur.y, cur.z, a,
                   NavFlowCostAt(truth, cur), nxt.x, nxt.y, nxt.z, b, s, field ? "" : "[RESCUE]",
                   b + s > a ? " <-- promise broken" : "");
        cur = nxt;
    }
}

static int ChunkIdOf(NavHFlow* h, NavChunkFlow* f) { for (int c = 0; c < h->chunkCount; c++) if (h->chunks[c] == f) return c; return -1; }
static void DumpHCell(NavWorld* w, NavHFlow* h, NavPoint p) {
    NavChunkFlow* cov[9];
    int n = nav_HFlowCovering(w, h, p, cov);
    printf("    cell (%d,%d,%d) in chunk %d, goal chunk %d\n", p.x, p.y, p.z, nav_GetChunk(w, p.x, p.y, p.z), h->goalChunk);
    for (int k = 0; k < n; k++) {
        NavChunkFlow* cf = cov[k];
        int i = nav_ChunkFlowIndex(cf, p);
        printf("     field %d region [%d,%d)x[%d,%d) cost %d\n", ChunkIdOf(h, cf), cf->minX, cf->minX + cf->w, cf->minY, cf->minY + cf->h, cf->cost[i]);
        for (int e = 0; e < w->entranceNum; e++) {
            NavEntrance* E = &w->entranceList[e];
            if (E->x < cf->minX || E->x >= cf->minX + cf->w || E->y < cf->minY || E->y >= cf->minY + cf->h) continue;
            int j = nav_ChunkFlowIndex(cf, (NavPoint){E->x, E->y, E->z});
            printf("       ent %d (%d,%d) ch %d/%d dist %d next %d via %d | here cost %d seed %d\n", e, E->x, E->y, E->chunk1, E->chunk2,
                   h->dist[e], h->nextEnt[e], h->viaChunk[e], cf->cost[j], cf->seed[j]);
        }
    }
}

// Every cell walks the hierarchical field to the goal
static void CheckHFlow(NavWorld* w, NavHFlow* h, const NavFlowField* truth, NavPoint goal, const int* extra) {
    int cells = map.w * map.h * map.d;
    stepExtra = extra;
    for (int i = 0; i < cells; i++) walked[i] = -2;
    walked[Cell(goal)] = 0;
    int reported = 0;
    for (int z = 0; z < map.d; z++)
        for (int y = 0; y < map.h; y++)
            for (int x = 0; x < map.w; x++) {
                if (map.wall[z][y][x]) continue;
                NavPoint p = {x, y, z};
                int opt = NavFlowCostAt(truth, p);
                int promised = NavHFlowCostAt(w, h, p);
                if (opt == NAV_FLOW_UNREACHABLE) {
                    if (promised != NAV_FLOW_UNREACHABLE && reported++ < 2)
                        FAIL("hflow: unreachable (%d,%d,%d) has cost %d", x, y, z, promised);
                    continue;
                }
                if (promised == NAV_FLOW_UNREACHABLE || promised < opt) {
                    if (reported++ < 2) {
                        FAIL("hflow: cost at (%d,%d,%d) is %d, optimal %d", x, y, z, promised, opt);
                        if (verbose && reported == 1) {
                            DumpHCell(w, h, p);
                            if (getenv("FZ_ENT")) {
                                int e0 = atoi(getenv("FZ_ENT"));
                                for (int e = 0; e < w->entranceNum; e++) {
                                    NavEntrance* E = &w->entranceList[e];
                                    if (E->z != p.z || abs(E->x - w->entranceList[e0].x) > 8 || abs(E->y - w->entranceList[e0].y) > 8) continue;
                                    printf("     ent %d (%d,%d,%d) ch %d/%d out:", e, E->x, E->y, E->z, E->chunk1, E->chunk2);
                                    { int on; const NavGraphEdge* oe = NavOutEdges(w, e, &on); for (int k = 0; k < on; k++) { const NavGraphEdge* g = &oe[k]; printf(" ->%d(c%d,ch%d)", g->to, g->cost, g->chunk); } }
                                    printf("\n");
                                }
                            }
                        }
                    }
                    continue;
                }
                if (walked[Cell(p)] >= 0) continue;
                // walk until the goal or a cell already known to reach it
                int n = 0;
                NavPoint cur = p;
                const char* problem = NULL;
                while (walked[Cell(cur)] < 0) {
                    if (walked[Cell(cur)] == -1) { problem = "loop"; break; }
                    walked[Cell(cur)] = -1;
                    walk[n++] = cur;
                    NavPoint nxt;
                    if (!NavHFlowNext(w, h, cur, &nxt)) { problem = "dead end"; break; }
                    if (StepCost(w, cur, nxt) < 0) { problem = "illegal step"; walk[n++] = nxt; break; }
                    cur = nxt;
                }
                if (problem) {
                    NavPoint at = walk[n - 1];
                    if (reported++ < 2)
                        FAIL("hflow: %s from (%d,%d,%d) at (%d,%d,%d) after %d steps (goal %d,%d,%d)", problem,
                             x, y, z, at.x, at.y, at.z, n, goal.x, goal.y, goal.z);
                    for (int k = 0; k < n; k++) if (walked[Cell(walk[k])] == -1) walked[Cell(walk[k])] = -3;
                    continue;
                }
                // unwind: costs back from the known cell
                int acc = walked[Cell(cur)];
                for (int k = n - 1; k >= 0; k--) {
                    NavPoint nx = (k == n - 1) ? cur : walk[k + 1];
                    acc += StepCost(w, walk[k], nx);
                    walked[Cell(walk[k])] = acc;
                }
                int got = walked[Cell(p)];
                statHierCells++; statHierExtra += got - opt; statHierOpt += opt;
                if (got < opt && reported++ < 2) {
                    FAIL("hflow: walked %d < optimal %d from (%d,%d,%d)", got, opt, x, y, z);
                    if (verbose) TraceHFlow(w, h, truth, p);
                }
                if (got > promised && reported++ < 2) {
                    FAIL("hflow: walked %d > promised %d from (%d,%d,%d) (opt %d)", got, promised, x, y, z, opt);
                    if (verbose) TraceHFlow(w, h, truth, p);
                }
            }
}

static void DumpMap(void) {
    for (int z = 0; z < map.d; z++) {
        printf("  z=%d\n", z);
        for (int y = 0; y < map.h; y++) {
            printf("  %3d ", y);
            for (int x = 0; x < map.w; x++) {
                char c = map.wall[z][y][x] ? '#' : (map.ladder[z][y][x] ? 'H' : (map.ramp[z][y][x] ? "?^>v<"[map.ramp[z][y][x]] : (map.cost[z][y][x] ? '~' : '.')));
                printf("%c", c);
            }
            printf("\n");
        }
    }
}

static void CheckWorld(NavWorld* w, NavHFlow** hs, NavPoint* goals, int goalCount, bool withExtra) {
    // A* sanity + flow agreement + HPA + regions on random pairs
    NavFlowField* f = NavFlowCreate(w);
    f->anyAngle = false;
    for (int gi = 0; gi < goalCount; gi++) {
        NavPoint goal = goals[gi];
        if (goal.x < 0 || map.wall[goal.z][goal.y][goal.x]) continue;
        f->extraCost = NULL;
        NavFlowBuild(w, f, &goal, 1, 0);
        for (int s = 0; s < 12; s++) {
            NavPoint p = RandomWalkableCell(s & 1);
            int la = NavFindPathAStar(w, p, goal, pbuf, (int)(sizeof(pbuf) / sizeof(pbuf[0])));
            int fc = NavFlowCostAt(f, p);
            bool same = p.x == goal.x && p.y == goal.y && p.z == goal.z;
            if (same) continue;
            if ((la > 0) != (fc != NAV_FLOW_UNREACHABLE)) { FAIL("flow/A* disagree on reachability (%d,%d,%d)", p.x, p.y, p.z); continue; }
            if (la > 0) {
                int c = GoalFirstPathCost(w, pbuf, la, "A*");
                if (c >= 0 && c != w->lastCost) FAIL("A*: path cost %d != reported %d", c, w->lastCost);
                if (w->lastCost != fc) FAIL("flow cost %d != A* %d at (%d,%d,%d)", fc, w->lastCost, p.x, p.y, p.z);
            }
        }
        // hierarchical: every cell, with and without extra cost
        if (withExtra) {
            f->extraCost = extraBuf;
            NavFlowBuild(w, f, &goal, 1, 0);
            hs[gi]->extraCost = extraBuf;
            NavHFlowInvalidate(hs[gi]);
        } else if (hs[gi]->extraCost) {
            hs[gi]->extraCost = NULL;
            NavHFlowInvalidate(hs[gi]);
        }
        CheckHFlow(w, hs[gi], f, goal, withExtra ? extraBuf : NULL);
        stepExtra = NULL;
    }
    NavFlowDestroy(f);

    for (int i = 0; i < 30; i++) {
        NavPoint a = RandomWalkableCell(i & 1), b = RandomWalkableCell(!(i & 1));
        if (a.x < 0 || b.x < 0 || (a.x == b.x && a.y == b.y && a.z == b.z)) continue;
        int la = NavFindPathAStar(w, a, b, pbuf, (int)(sizeof(pbuf) / sizeof(pbuf[0])));
        int opt = w->lastCost;
        if (la > 0) { statAStarPaths++; statExcursions += HasChunkExcursion(pbuf, la); }
        // A* must be exactly optimal (an inadmissible heuristic shows up here)
        {
            NavFlowField* ff = NavFlowCreate(w);
            ff->anyAngle = false;
            NavFlowBuild(w, ff, &b, 1, 0);
            int truth = NavFlowCostAt(ff, a);
            if (la > 0 && truth != opt) FAIL("A*: cost %d, optimal %d for (%d,%d,%d) -> (%d,%d,%d)", opt, truth, a.x, a.y, a.z, b.x, b.y, b.z);
            if (la > 0) { int c = GoalFirstPathCost(w, pbuf, la, "A*"); if (c >= 0 && c != opt) FAIL("A*: path cost %d != reported %d", c, opt); }
            NavFlowDestroy(ff);
        }
        if (la > 0 && !NavReachable(w, a, b))
            FAIL("regions: (%d,%d,%d) -> (%d,%d,%d) reachable but regions say no", a.x, a.y, a.z, b.x, b.y, b.z);
        int lh = NavFindPathHPA(w, a, b, pbuf, (int)(sizeof(pbuf) / sizeof(pbuf[0])));
        if ((la > 0) != (lh > 0) && getenv("FZ_HPA")) {
            static int st[NAV_CONNECT_MAX], sc[NAV_CONNECT_MAX], gt[NAV_CONNECT_MAX], gc[NAV_CONNECT_MAX];
            int ns = nav_ConnectToChunk(w, a, nav_GetChunk(w, a.x, a.y, a.z), st, sc, false);
            int ng = nav_ConnectToChunk(w, b, nav_GetChunk(w, b.x, b.y, b.z), gt, gc, true);
            printf("    start conns %d, goal conns %d\n", ns, ng);
            printf("    local path len %d, maxLen %d, tempCap %d\n", nav_LocalPath(w, a.x, a.y, a.z, b.x, b.y, b.z, pbuf, (int)(sizeof(pbuf) / sizeof(pbuf[0]))), (int)(sizeof(pbuf) / sizeof(pbuf[0])), w->tempPathCap);
            for (int i = 0; i < ng; i++) printf("      goal ent %d (%d,%d,%d) cost %d\n", gt[i], w->entranceList[gt[i]].x, w->entranceList[gt[i]].y, w->entranceList[gt[i]].z, gc[i]);
            // BFS over the directed graph from start entrances
            static int seen[NAV_MAX_ENTRANCES]; memset(seen, 0, sizeof(seen));
            static int qq[NAV_MAX_ENTRANCES]; int qh = 0, qt = 0;
            for (int i = 0; i < ns; i++) if (!seen[st[i]]) { seen[st[i]] = 1; qq[qt++] = st[i]; }
            while (qh < qt) { int e = qq[qh++]; int on; const NavGraphEdge* oe = NavOutEdges(w, e, &on); for (int k = 0; k < on; k++) { int t = oe[k].to; if (!seen[t]) { seen[t] = 1; qq[qt++] = t; } } }
            int hit = 0; for (int i = 0; i < ng; i++) hit += seen[gt[i]];
            printf("    graph reaches %d of the goal entrances (%d entrances reached); A* path:\n", hit, qt);
            NavFindPathAStar(w, a, b, pbuf, (int)(sizeof(pbuf) / sizeof(pbuf[0])));
            for (int k = la - 1; k >= 0; k--) printf(" (%d,%d,%d)", pbuf[k].x, pbuf[k].y, pbuf[k].z);
            printf("\n");
        }
        if ((la > 0) != (lh > 0)) {
            FAIL("HPA*: %s (%d,%d,%d) -> (%d,%d,%d), A* %s", lh > 0 ? "found" : "missed", a.x, a.y, a.z, b.x, b.y, b.z,
                 la > 0 ? "found" : "none");
        } else if (lh > 0) {
            if (pbuf[0].x != b.x || pbuf[0].y != b.y || pbuf[0].z != b.z || pbuf[lh - 1].x != a.x || pbuf[lh - 1].y != a.y || pbuf[lh - 1].z != a.z)
                FAIL("HPA*: path endpoints wrong for (%d,%d,%d) -> (%d,%d,%d)", a.x, a.y, a.z, b.x, b.y, b.z);
            int c = GoalFirstPathCost(w, pbuf, lh, "HPA*");
            if (c >= 0 && c < opt) {
                NavFlowField* ff = NavFlowCreate(w);
                ff->anyAngle = false;
                NavFlowBuild(w, ff, &b, 1, 0);
                FAIL("HPA*: cost %d below A* cost %d (true optimum %d) (%d,%d,%d) -> (%d,%d,%d)", c, opt, NavFlowCostAt(ff, a),
                     a.x, a.y, a.z, b.x, b.y, b.z);
                NavFlowDestroy(ff);
            }
        }
        // Coarse route refined leg by leg must give exactly HPA*'s cells
        {
            static NavPoint route[4096], cells[FZ_W * FZ_H * FZ_D];
            int nr = NavFindRouteHPA(w, a, b, route, 4096);
            if ((nr > 0) != (lh > 0)) FAIL("route: %s but HPA* %s (%d,%d,%d) -> (%d,%d,%d)", nr > 0 ? "found" : "none",
                                          lh > 0 ? "found" : "none", a.x, a.y, a.z, b.x, b.y, b.z);
            else if (nr > 0) {
                int nc = 0;
                cells[nc++] = a;
                bool ok = true;
                for (int k = 1; k < nr && ok; k++) {
                    int got = NavRefineLeg(w, route[k - 1], route[k], cells + nc, (int)(sizeof(cells) / sizeof(cells[0])) - nc);
                    if (got < 0) ok = false; else nc += got;
                }
                bool same = ok && nc == lh;
                for (int k = 0; same && k < nc; k++)   // pbuf is goal-first
                    same = cells[k].x == pbuf[lh - 1 - k].x && cells[k].y == pbuf[lh - 1 - k].y && cells[k].z == pbuf[lh - 1 - k].z;
                if (!same) FAIL("route: legs give %d cells (%s), HPA* %d, (%d,%d,%d) -> (%d,%d,%d)", nc, ok ? "ok" : "a leg failed",
                                lh, a.x, a.y, a.z, b.x, b.y, b.z);
            }
        }
    }

    for (int i = 0; i < 8; i++) {
        NavPoint s = RandomWalkableCell(false);
        Targets t = {.n = 0};
        for (int k = 0; k < 6; k++) { NavPoint c = RandomWalkableCell(k & 1); if (c.x >= 0) t.t[t.n++] = c; }
        if (s.x < 0 || t.n == 0 || IsTarget(&t, s.x, s.y, s.z)) continue;
        int best = -1;
        for (int k = 0; k < t.n; k++) {
            int len = NavFindPathAStar(w, s, t.t[k], pbuf, (int)(sizeof(pbuf) / sizeof(pbuf[0])));
            if (len > 0 && (best < 0 || w->lastCost < best)) best = w->lastCost;
        }
        NavPoint found;
        int got = NavFindNearest(w, s, IsTarget, &t, 0, &found, NULL, 0, NULL);
        if (got != best) FAIL("nearest: got %d, best %d from (%d,%d,%d)", got, best, s.x, s.y, s.z);
        else if (got >= 0 && !IsTarget(&t, found.x, found.y, found.z)) FAIL("nearest: found cell is not a target");
    }
}

// The incrementally updated graph must equal a fresh full build: same entrances,
// same directed edges with the same costs. Compared as sorted tuples keyed by
// entrance position + chunks (several entrances can share a cell, e.g. the middle
// of a ladder spanning three levels).
typedef struct { int k[5]; } EntKey;
typedef struct { EntKey a, b; int cost; } EdgeKey;
static EntKey KeyOf(const NavEntrance* e) { return (EntKey){{e->z, e->y, e->x, e->chunk1, e->chunk2}}; }
static int CmpEnt(const void* p, const void* q) { return memcmp(p, q, sizeof(EntKey)); }
static int CmpEdge(const void* p, const void* q) {
    const EdgeKey* a = p; const EdgeKey* b = q;
    int c = memcmp(&a->a, &b->a, sizeof(EntKey));
    if (!c) c = memcmp(&a->b, &b->b, sizeof(EntKey));
    if (!c) c = (a->cost > b->cost) - (a->cost < b->cost);
    return c;
}
static void CheckIncrementalMatchesFull(NavWorld* w) {
    NavWorld* f = NavCreate(map.w, map.h, map.d, chunkW, chunkH, &map);
    f->eightDir = w->eightDir;
    NavBuildEntrances(f);
    NavBuildGraph(f);
    int wEdgeNum, fEdgeNum;
    const NavGraphEdge* wEdges = NavEdgesFlat(w, &wEdgeNum);
    const NavGraphEdge* fEdges = NavEdgesFlat(f, &fEdgeNum);
    (void)wEdgeNum; (void)fEdgeNum;
    if (f->entranceNum != w->entranceNum) FAIL("incremental: %d entrances, full build has %d", w->entranceNum, f->entranceNum);
    if (f->edgeNum != w->edgeNum) FAIL("incremental: %d edges, full build has %d", w->edgeNum, f->edgeNum);
    if (f->entranceNum == w->entranceNum) {
        EntKey* ka = malloc(sizeof(EntKey) * (size_t)(w->entranceNum + 1));
        EntKey* kb = malloc(sizeof(EntKey) * (size_t)(f->entranceNum + 1));
        for (int i = 0; i < w->entranceNum; i++) { ka[i] = KeyOf(&w->entranceList[i]); kb[i] = KeyOf(&f->entranceList[i]); }
        qsort(ka, (size_t)w->entranceNum, sizeof(EntKey), CmpEnt);
        qsort(kb, (size_t)f->entranceNum, sizeof(EntKey), CmpEnt);
        for (int i = 0; i < w->entranceNum; i++)
            if (memcmp(&ka[i], &kb[i], sizeof(EntKey))) {
                FAIL("incremental: entrance sets differ: (%d,%d,%d ch %d/%d) vs full (%d,%d,%d ch %d/%d)", ka[i].k[2], ka[i].k[1], ka[i].k[0],
                     ka[i].k[3], ka[i].k[4], kb[i].k[2], kb[i].k[1], kb[i].k[0], kb[i].k[3], kb[i].k[4]);
                break;
            }
        free(ka); free(kb);
    }
    if (verbose) {
        EdgeKey* ea = malloc(sizeof(EdgeKey) * (size_t)(w->edgeNum + 1));
        EdgeKey* eb = malloc(sizeof(EdgeKey) * (size_t)(f->edgeNum + 1));
        for (int i = 0; i < w->edgeNum; i++) ea[i] = (EdgeKey){KeyOf(&w->entranceList[wEdges[i].from]), KeyOf(&w->entranceList[wEdges[i].to]), wEdges[i].cost};
        for (int i = 0; i < f->edgeNum; i++) eb[i] = (EdgeKey){KeyOf(&f->entranceList[fEdges[i].from]), KeyOf(&f->entranceList[fEdges[i].to]), fEdges[i].cost};
        qsort(ea, (size_t)w->edgeNum, sizeof(EdgeKey), CmpEdge);
        qsort(eb, (size_t)f->edgeNum, sizeof(EdgeKey), CmpEdge);
        int i = 0, j = 0, shown = 0;
        while ((i < w->edgeNum || j < f->edgeNum) && shown < 8) {
            int c = i >= w->edgeNum ? 1 : j >= f->edgeNum ? -1 : CmpEdge(&ea[i], &eb[j]);
            if (c == 0) { i++; j++; continue; }
            EdgeKey* e = c < 0 ? &ea[i++] : &eb[j++];
            printf("      only in %s: (%d,%d,%d ch%d/%d)->(%d,%d,%d ch%d/%d) cost %d\n", c < 0 ? "incremental" : "full",
                   e->a.k[2], e->a.k[1], e->a.k[0], e->a.k[3], e->a.k[4], e->b.k[2], e->b.k[1], e->b.k[0], e->b.k[3], e->b.k[4], e->cost);
            shown++;
        }
        free(ea); free(eb);
    }
    if (f->edgeNum == w->edgeNum) {
        EdgeKey* ea = malloc(sizeof(EdgeKey) * (size_t)(w->edgeNum + 1));
        EdgeKey* eb = malloc(sizeof(EdgeKey) * (size_t)(f->edgeNum + 1));
        for (int i = 0; i < w->edgeNum; i++) {
            ea[i] = (EdgeKey){KeyOf(&w->entranceList[wEdges[i].from]), KeyOf(&w->entranceList[wEdges[i].to]), wEdges[i].cost};
            eb[i] = (EdgeKey){KeyOf(&f->entranceList[fEdges[i].from]), KeyOf(&f->entranceList[fEdges[i].to]), fEdges[i].cost};
        }
        qsort(ea, (size_t)w->edgeNum, sizeof(EdgeKey), CmpEdge);
        qsort(eb, (size_t)f->edgeNum, sizeof(EdgeKey), CmpEdge);
        for (int i = 0; i < w->edgeNum; i++)
            if (CmpEdge(&ea[i], &eb[i])) {
                FAIL("incremental: edges differ: (%d,%d,%d)->(%d,%d,%d) cost %d vs full (%d,%d,%d)->(%d,%d,%d) cost %d",
                     ea[i].a.k[2], ea[i].a.k[1], ea[i].a.k[0], ea[i].b.k[2], ea[i].b.k[1], ea[i].b.k[0], ea[i].cost,
                     eb[i].a.k[2], eb[i].a.k[1], eb[i].a.k[0], eb[i].b.k[2], eb[i].b.k[1], eb[i].b.k[0], eb[i].cost);
                break;
            }
        free(ea); free(eb);
    }
    // Regions after the incremental relabel split the cells exactly like a fresh build
    // (same partition; ids may differ): each incremental id maps to one full id and back
    {
        int nw = NavRegionCount(w), nf = NavRegionCount(f);
        if (nw != nf) FAIL("incremental: %d regions, full build has %d", nw, nf);
        int ids = map.w * map.h * map.d + 1;   // region ids are union-find roots: < cell count
        int* toF = malloc(sizeof(int) * (size_t)ids);
        int* toW = malloc(sizeof(int) * (size_t)ids);
        for (int i = 0; i < ids; i++) toF[i] = toW[i] = -1;
        int bad = 0;
        for (int z = 0; z < map.d && !bad; z++)
            for (int y = 0; y < map.h && !bad; y++)
                for (int x = 0; x < map.w && !bad; x++) {
                    NavPoint p = {x, y, z};
                    int a = NavRegionId(w, p), b = NavRegionId(f, p);
                    if ((a < 0) != (b < 0)) { FAIL("incremental: region of (%d,%d,%d) is %d, full build %d", x, y, z, a, b); bad = 1; break; }
                    if (a < 0) continue;
                    if (toF[a] < 0) toF[a] = b;
                    if (toW[b] < 0) toW[b] = a;
                    if (toF[a] != b || toW[b] != a) { FAIL("incremental: regions at (%d,%d,%d) split differently from a full build", x, y, z); bad = 1; }
                }
        free(toF); free(toW);
    }
    NavDestroy(f);
}

// Random edits, reported cell by cell like a game would
static void EditWorld(NavWorld* w) {
    int edits = RndRange(1, 12);
    for (int i = 0; i < edits; i++) {
        int x, y;
        BiasedCell(&x, &y);
        int z = RndRange(0, map.d - 1);
        int kind = RndRange(0, 9);
        if (getenv("FZ_EDITS")) printf("    edit kind %d at (%d,%d,%d)\n", kind, x, y, z);   // 0-4 wall, 5-6 ladder, 7-8 ramp, 9 cost
        if (kind < 5) {
            map.wall[z][y][x] = !map.wall[z][y][x];
            if (map.wall[z][y][x]) { map.ladder[z][y][x] = 0; map.ramp[z][y][x] = 0; }
            NavMarkCellChanged(w, x, y, z);
        } else if (kind < 7 && map.d > 1) {
            int lz = z < map.d - 1 ? z : z - 1;
            FzPlaceLadder(x, y, lz);
            NavMarkCellChanged(w, x, y, lz);
            NavMarkCellChanged(w, x, y, lz + 1);
        } else if (kind < 9 && map.d > 1) {
            int rz = z < map.d - 1 ? z : z - 1;
            int dir = RndRange(1, 4);
            static const int ox[5] = {0, 0, 1, 0, -1}, oy[5] = {0, -1, 0, 1, 0};
            FzPlaceRamp(x, y, rz, dir);
            NavMarkCellChanged(w, x, y, rz);
            int ex = x + ox[dir], ey = y + oy[dir], lx = x - ox[dir], ly = y - oy[dir];
            if (ex >= 0 && ey >= 0 && ex < map.w && ey < map.h) NavMarkCellChanged(w, ex, ey, rz + 1);
            if (lx >= 0 && ly >= 0 && lx < map.w && ly < map.h) NavMarkCellChanged(w, lx, ly, rz);
        } else {
            map.cost[z][y][x] = Chance(50) ? 0 : (unsigned char)RndRange(12, 200);
            NavMarkCellChanged(w, x, y, z);
        }
    }
    if (getenv("FZ_FULL")) { NavBuildEntrances(w); NavBuildGraph(w); }
    else { NavUpdateDirty(w); CheckIncrementalMatchesFull(w); }
}

// Graph builds hand their per-chunk searches to NavWorld.parallelFor. This one runs
// them backwards (the results must not depend on the order); built with -DFZ_THREADS it
// uses real threads instead (run under -fsanitize=thread to check the searches only read).
#ifdef FZ_THREADS
#include <pthread.h>
#include <stdatomic.h>
typedef struct { void (*fn)(void*, int); void* ctx; int n; atomic_int next; } FzPar;
static void* FzParWorker(void* p) {
    FzPar* j = p;
    for (int i; (i = atomic_fetch_add(&j->next, 1)) < j->n;) j->fn(j->ctx, i);
    return NULL;
}
static void FzParallelFor(void* user, int n, void (*fn)(void* ctx, int i), void* ctx) {
    (void)user;
    FzPar j; j.fn = fn; j.ctx = ctx; j.n = n; atomic_init(&j.next, 0);
    pthread_t th[3];
    for (int k = 0; k < 3; k++) pthread_create(&th[k], NULL, FzParWorker, &j);
    FzParWorker(&j);
    for (int k = 0; k < 3; k++) pthread_join(th[k], NULL);
}
#else
static void FzParallelFor(void* user, int n, void (*fn)(void* ctx, int i), void* ctx) {
    (void)user;
    for (int i = n - 1; i >= 0; i--) fn(ctx, i);
}
#endif

// A graph built through parallelFor is exactly the one built without it
static void CheckParallelBuildSame(NavWorld* w) {
    NavWorld* f = NavCreate(map.w, map.h, map.d, chunkW, chunkH, &map);
    f->eightDir = w->eightDir;
    f->parallelFor = w->parallelFor ? NULL : FzParallelFor;
    NavBuildEntrances(f);
    NavBuildGraph(f);
    if (f->entranceNum != w->entranceNum || f->edgeNum != w->edgeNum)
        FAIL("parallel build: %d entrances %d edges vs %d / %d", w->entranceNum, w->edgeNum, f->entranceNum, f->edgeNum);
    else
        for (int e = 0; e < w->entranceNum; e++)
            if (w->adj[e].num != f->adj[e].num ||
                memcmp(w->adj[e].e, f->adj[e].e, (size_t)w->adj[e].num * sizeof(NavGraphEdge))) {
                FAIL("parallel build: edges of entrance %d differ", e);
                break;
            }
    NavDestroy(f);
}

int main(int argc, char** argv) {
    int worlds = 60;
    unsigned long long firstSeed = 1;
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) verbose = true;
        else if (!strcmp(argv[i], "-q")) {}
        else if (pos == 0) { worlds = atoi(argv[i]); pos++; }
        else firstSeed = strtoull(argv[i], NULL, 10);
    }
    int failedWorlds = 0;
    for (int wi = 0; wi < worlds; wi++) {
        unsigned long long seed = firstSeed + (unsigned long long)wi;
        rngState = seed * 0x9E3779B97F4A7C15ull + 1;
        int gen;
        GenerateWorld(&gen);
        bool eight = Chance(75);
        fz_climbExtraOn = map.d > 1 && Chance(40);   // level changes cost extra (ladder queues)
        snprintf(worldDesc, sizeof(worldDesc), "seed %llu %s %dx%dx%d chunk %dx%d %s%s", seed, kGenNames[gen],
                 map.w, map.h, map.d, chunkW, chunkH, eight ? "8dir" : "4dir", fz_climbExtraOn ? " climb-extra" : "");
        worldFailures = 0;
        if (CountWalkable() < 2) continue;
        CountInnerRamps();

        NavWorld* w = NavCreate(map.w, map.h, map.d, chunkW, chunkH, &map);
        w->eightDir = eight;
        if (seed & 1) w->parallelFor = FzParallelFor;   // (not Chance: keeps every seed's world)
        NavBuildEntrances(w);
        NavBuildGraph(w);
        phase = "parallel";
        CheckParallelBuildSame(w);

        // extra cost layout matches NavFlowField's [z][y][x] over the world size
        extraBuf = NULL;
        static int extraFlat[FZ_D * FZ_H * FZ_W];
        bool useExtra = Chance(30);
        if (useExtra) {
            for (int i = 0; i < map.w * map.h * map.d; i++) extraFlat[i] = Chance(10) ? RndRange(1, 80) : 0;
            extraBuf = extraFlat;
        }

        NavPoint goals[4] = {RandomWalkableCell(false), RandomWalkableCell(true), SpecialGoal(0), SpecialGoal(1)};
        NavHFlow* hs[4];
        for (int g = 0; g < 4; g++) hs[g] = NavHFlowCreate(w, goals[g].x >= 0 ? goals[g] : (NavPoint){0, 0, 0});

        if (getenv("FZ_DUMP")) { DumpMap(); for (int g = 0; g < 4; g++) printf("  goal %d: (%d,%d,%d)\n", g, goals[g].x, goals[g].y, goals[g].z); }
        phase = "initial";
        CheckWorld(w, hs, goals, 4, false);
        if (getenv("FZ_DUMP")) {
            NavHFlow* h = hs[0];
            NavHFlowCostAt(w, h, goals[0]);
            for (int e = 0; e < w->entranceNum; e++) {
                NavEntrance* E = &w->entranceList[e];
                int f = h->nextEnt[e];
                printf("  ent %d (%d,%d,%d) ch %d/%d dist %d next %d via %d", e, E->x, E->y, E->z, E->chunk1, E->chunk2, h->dist[e], f, h->viaChunk[e]);
                if (f >= 0) printf(" -> (%d,%d,%d)", w->entranceList[f].x, w->entranceList[f].y, w->entranceList[f].z);
                printf("\n");
            }
            for (int i = 0; i < w->rampNum; i++) printf("  ramp link %d: ent %d -> %d\n", i, w->rampList[i].entranceRamp, w->rampList[i].entranceExit);
        }
        phase = "extra cost";
        if (useExtra) CheckWorld(w, hs, goals, 4, true);
        for (int round = 0; round < 3; round++) {
            static const char* rounds[] = {"after edits 1", "after edits 2", "after edits 3"};
            phase = rounds[round];
            EditWorld(w);
            if (getenv("FZ_DUMPROUND") && atoi(getenv("FZ_DUMPROUND")) == round + 1) {
                DumpMap();
                for (int g = 0; g < 4; g++) printf("  goal %d: (%d,%d,%d)\n", g, goals[g].x, goals[g].y, goals[g].z);
            }
            // goals that became walls: move them (their NavHFlow must be recreated)
            for (int g = 0; g < 4; g++)
                if (goals[g].x >= 0 && map.wall[goals[g].z][goals[g].y][goals[g].x]) {
                    NavHFlowDestroy(hs[g]);
                    goals[g] = RandomWalkableCell(g & 1);
                    hs[g] = NavHFlowCreate(w, goals[g].x >= 0 ? goals[g] : (NavPoint){0, 0, 0});
                }
            CheckWorld(w, hs, goals, 4, false);
        }
        for (int g = 0; g < 4; g++) NavHFlowDestroy(hs[g]);
        statFailShortcuts += w->absFailShortcuts;
        NavDestroy(w);
        if (worldFailures) failedWorlds++;
        else if (verbose) printf("  ok [%s]\n", worldDesc);
    }
    printf("nav fuzz: %d worlds, %d failed, %d failures; hflow over %ld cells +%.2f%% vs optimal\n", worlds,
           failedWorlds, failures, statHierCells, statHierOpt ? 100.0 * statHierExtra / statHierOpt : 0.0);
    printf("  %ld ramps inside chunks; %ld of %ld A* paths leave a level and come back within one chunk\n",
           statInnerRamps, statExcursions, statAStarPaths);
    printf("  %ld abstract searches answered \"no route\" from the previous failed search\n", statFailShortcuts);
    printf("\tFailed: %d\n", failures ? 1 : 0);
    return failures ? 1 : 0;
}
