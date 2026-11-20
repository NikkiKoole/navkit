// bench_nav_terrains.c — every nav algorithm on every terrain generator.
//
// Run with: make bench_nav_terrains
//
// For each generator (128x128, 16 levels, fixed seed) checks the nav library
// against plain A* (the ground truth) and reports timings:
//   regions   NavReachable vs A*: false "unreachable" must be 0; "reachable" may
//             rarely be a false positive (one-way ramp side exits, deep water)
//   flow      NavFlowField cost == A* cost on sampled cells (exact)
//   hier      NavHFlow reaches the goal wherever A* does; extra cost vs optimal
//   hpa       HPA* paths, same checks, as the baseline the hierarchy builds on
//   nearest   NavFindNearest cost == best A* cost among the targets (exact)
// Exit code is non-zero if a must-hold check fails.

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/world/terrain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double NowMs(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

typedef struct { const char* name; void (*gen)(void); } Gen;
static void GenSparse(void) { GenerateSparse(0.25f); }
static const Gen kGens[] = {
    {"sparse", GenSparse},           {"perlin", GeneratePerlin},         {"city", GenerateCity},
    {"mixed", GenerateMixed},        {"concentric", GenerateConcentricMaze}, {"dungeon", GenerateDungeonRooms},
    {"caves", GenerateCaves},        {"drunkard", GenerateDrunkard},     {"tunneler", GenerateTunneler},
    {"mixmax", GenerateMixMax},      {"towers", GenerateTowers},         {"galleryflat", GenerateGalleryFlat},
    {"castle", GenerateCastle},      {"labyrinth3d", GenerateLabyrinth3D}, {"spiral3d", GenerateSpiral3D},
    {"council", GenerateCouncilEstate}, {"hills", GenerateHills},        {"hillssoils", GenerateHillsSoils},
    {"hillswater", GenerateHillsSoilsWater},
};

// Cost of walking a goal-first path with the same step rules as A*, including
// the game's extra cost for level changes (slow ladders, ladder queues: the
// NAV_CLIMB_EXTRA hook, entities/mover.c)
int LadderQueueExtraCost(int x, int y, int z, int dz);
static int PathCost(const Point* p, int len) {
    int c = 0;
    for (int k = len - 1; k > 0; k--) {
        Point a = p[k], b = p[k - 1];
        int mc = GetCellMoveCost(b.x, b.y, b.z);
        if (a.z != b.z) c += ((a.x == b.x && a.y == b.y) ? mc : (14 * mc) / 10) + LadderQueueExtraCost(a.x, a.y, a.z, b.z - a.z);
        else c += ((a.x != b.x && a.y != b.y) ? 14 : 10) * mc / 10;
    }
    return c;
}

typedef struct { Point t[8]; int n; } Targets;
static bool IsTarget(void* user, int x, int y, int z) {
    const Targets* t = user;
    for (int i = 0; i < t->n; i++) if (t->t[i].x == x && t->t[i].y == y && t->t[i].z == z) return true;
    return false;
}

static Point bufA[MAX_PATH], bufB[MAX_PATH];

// Random walkable cell on any level (no "valid destination" filter, which
// rejects wall tops and would hide most of some maps)
static Point RandomWalkable(void) {
    for (int t = 0; t < 20000; t++) {
        Point p = {GetRandomValue(0, gridWidth - 1), GetRandomValue(0, gridHeight - 1), GetRandomValue(0, gridDepth - 1)};
        if (IsCellWalkableAt(p.z, p.y, p.x)) return p;
    }
    return (Point){-1, -1, 0};
}

int main(void) {
    SetTraceLogLevel(LOG_NONE);
    int hardFailures = 0;
    printf("%-12s %7s | %7s %6s %4s %4s | %5s %6s | %6s %6s | %6s %6s | %4s | %6s %6s\n",
           "terrain", "walk", "regions", "ms", "FN", "FP", "flowX", "ms", "hier%", "+cost", "hpa%", "+cost",
           "nrX", "A*ms", "HPAms");
    for (size_t g = 0; g < sizeof(kGens) / sizeof(kGens[0]); g++) {
        InitGridWithSizeAndChunkSize(128, 128, 16, 16);
        SeedRandom(1234 + (unsigned)g);
        kGens[g].gen();
        BuildEntrances();
        BuildGraph();
        NavWorld* w = NavGame();

        int walk = 0;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) walk += IsCellWalkableAt(z, y, x);
        if (walk < 10) { printf("%-12s  (no walkable cells at this size, skipped)\n", kGens[g].name); continue; }

        double t0 = NowMs();
        int regions = NavRegionCount(w);
        double regMs = NowMs() - t0;

        // Random pairs: regions + A* + HPA*
        SeedRandom(77);
        int fn = 0, fp = 0, pairs = 0, hpaOk = 0, reachable = 0;
        long hpaCost = 0, optCost = 0;
        double astarMs = 0, hpaMs = 0;
        for (int i = 0; i < 120; i++) {
            Point a = RandomWalkable(), b = RandomWalkable();
            if (a.x < 0 || b.x < 0) continue;
            pairs++;
            t0 = NowMs();
            int la = NavFindPathAStar(w, a, b, bufA, MAX_PATH);
            astarMs += NowMs() - t0;
            int opt = w->lastCost;
            bool truth = la > 0;
            bool reg = NavReachable(w, a, b);
            if (truth && !reg) fn++;
            if (!truth && reg) fp++;
            t0 = NowMs();
            int lh = NavFindPathHPA(w, a, b, bufB, MAX_PATH);
            hpaMs += NowMs() - t0;
            if (truth) {
                reachable++;
                if (lh > 0) { hpaOk++; hpaCost += PathCost(bufB, lh); optCost += opt; }
            }
        }

        // Flow field + hierarchical flow toward one goal
        Point goal = RandomWalkable();
        NavFlowField* f = NavFlowCreate(w);
        f->anyAngle = false;
        t0 = NowMs();
        NavFlowBuild(w, f, &goal, 1, 0);
        double flowMs = NowMs() - t0;
        NavHFlow* h = NavHFlowCreate(w, goal);
        int flowMismatch = 0, hierOk = 0, hierReach = 0;
        long hierCost = 0, hierOpt = 0;
        for (int i = 0; i < 40; i++) {
            Point p = RandomWalkable();
            if (p.x < 0) continue;
            int la = NavFindPathAStar(w, p, goal, bufA, MAX_PATH);
            int opt = la > 0 ? w->lastCost : -1;
            int fc = NavFlowCostAt(f, p);
            if ((la > 0) != (fc != NAV_FLOW_UNREACHABLE) || (la > 0 && fc != opt)) flowMismatch++;
            if (la > 0) {
                hierReach++;
                // walk the hierarchical field and sum the real step costs
                int n = 0;
                Point cur = p, nxt;
                bufB[n++] = cur;
                while (n < MAX_PATH && NavHFlowNext(w, h, cur, &nxt)) { cur = nxt; bufB[n++] = cur; }
                if (cur.x == goal.x && cur.y == goal.y && cur.z == goal.z) {
                    hierOk++;
                    // bufB is start-first here: reverse into goal-first for PathCost
                    for (int k = 0; k < n / 2; k++) { Point tmp = bufB[k]; bufB[k] = bufB[n - 1 - k]; bufB[n - 1 - k] = tmp; }
                    hierCost += PathCost(bufB, n);
                    hierOpt += opt;
                }
            }
        }

        // Nearest by walking among 6 random targets == best A* cost
        int nearestMismatch = 0;
        for (int i = 0; i < 15; i++) {
            Point s = RandomWalkable();
            Targets t = {.n = 0};
            for (int k = 0; k < 6; k++) { Point c = RandomWalkable(); if (c.x >= 0) t.t[t.n++] = c; }
            if (s.x < 0 || t.n == 0) continue;
            int best = -1;
            for (int k = 0; k < t.n; k++) {
                int len = NavFindPathAStar(w, s, t.t[k], bufA, MAX_PATH);
                int c = (s.x == t.t[k].x && s.y == t.t[k].y && s.z == t.t[k].z) ? 0 : (len > 0 ? w->lastCost : -1);
                if (c >= 0 && (best < 0 || c < best)) best = c;
            }
            Point found;
            int got = IsTarget(&t, s.x, s.y, s.z) ? 0 : NavFindNearest(w, s, IsTarget, &t, 0, &found, NULL, 0, NULL);
            if (got != best) nearestMismatch++;
        }

        printf("%-11s%c %7d | %7d %6.1f %4d %4d | %5d %6.1f | %5.0f%% %5.1f%% | %5.0f%% %5.1f%% | %4d | %6.2f %6.3f\n",
               kGens[g].name, ' ', walk, regions, regMs, fn, fp, flowMismatch, flowMs,
               hierReach ? 100.0 * hierOk / hierReach : 100.0, hierOpt ? 100.0 * (hierCost - hierOpt) / hierOpt : 0.0,
               reachable ? 100.0 * hpaOk / reachable : 100.0, optCost ? 100.0 * (hpaCost - optCost) / optCost : 0.0,
               nearestMismatch, pairs ? astarMs / pairs : 0.0, pairs ? hpaMs / pairs : 0.0);
        hardFailures += fn + flowMismatch + nearestMismatch + (hierReach - hierOk);
        NavHFlowDestroy(h);
        NavFlowDestroy(f);
        (void)pairs;
    }
    printf("\n");
    printf("must-hold failures (region FN + flow mismatches + hier misses + nearest mismatches): %d\n", hardFailures);
    return hardFailures ? 1 : 0;
}
