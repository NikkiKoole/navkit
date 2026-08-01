// bench_astar_heap.c - A/B: current RunAStar (linear scan) vs heap-based A*
//
// Run with: make bench_astar_heap
//
// Backs the findings in docs/todo/pathfinding/astar-priority-queue-and-extraction.md.
// RunAStar() picks the next node by linear-scanning the whole nodeData array on every
// pop (pathfinding.c:1648) and clears the full array on entry (:1623), even though a
// working binary heap already sits unused in the same file (:227).
//
// V0 = current RunAStar()            : O(W*H*D) scan per pop + full nodeData clear
// V1 = heap + full clear             : isolates the priority-queue win
// V2 = heap + generation stamps      : also removes the O(W*H*D) init clear
//
// V1/V2 are throwaway reference implementations living only in this file — they exist
// to size the win, not to be linked by the game. All three MUST report identical goal
// g-costs; a "*** MISMATCH ***" line means a variant diverged and the timings are void.
//
// Note: this benchmark is deliberately NOT part of `make bench`. V0 is slow *because*
// it is the thing being measured (~4s for a single 128x128 path), which would dominate
// the aggregate bench run. Once RunAStar is fixed, fold it in.

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/simulation/water.h"
#include "../src/simulation/weather.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COST_INF 999999

// See use in Scenario2() — calibration for extrapolating V0 at sizes too slow to run.
#define NS_PER_SCANNED_CELL 1.09e-6

static double Now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ---------------------------------------------------------------- heap A*
static int   *hG, *hF, *hParent;
static unsigned char *hState;      // 0=unseen 1=open 2=closed
static unsigned int  *hGen;
static unsigned int   curGen = 0;
static int   *heap, *heapIdx, heapCount;
static int    capNodes = 0;

static int benchNodesExplored = 0;

static inline int NodeId(int x, int y, int z) { return (z * gridHeight + y) * gridWidth + x; }

static void AllocNodes(void) {
    int n = gridWidth * gridHeight * gridDepth;
    if (n <= capNodes) return;
    free(hG); free(hF); free(hParent); free(hState); free(hGen); free(heap); free(heapIdx);
    hG      = malloc(n * sizeof(int));
    hF      = malloc(n * sizeof(int));
    hParent = malloc(n * sizeof(int));
    hState  = malloc(n);
    hGen    = malloc(n * sizeof(unsigned int));
    heap    = malloc(n * sizeof(int));
    heapIdx = malloc(n * sizeof(int));
    memset(hGen, 0, n * sizeof(unsigned int));
    capNodes = n;
}

static inline void Touch(int id, bool useGen) {
    if (!useGen) return;
    if (hGen[id] != curGen) {
        hGen[id] = curGen;
        hG[id] = COST_INF; hF[id] = COST_INF; hParent[id] = -1; hState[id] = 0;
        heapIdx[id] = -1;
    }
}

static void HBubbleUp(int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (hF[heap[p]] <= hF[heap[i]]) break;
        int t = heap[p]; heap[p] = heap[i]; heap[i] = t;
        heapIdx[heap[p]] = p; heapIdx[heap[i]] = i;
        i = p;
    }
}
static void HBubbleDown(int i) {
    for (;;) {
        int l = 2*i + 1, r = l + 1, s = i;
        if (l < heapCount && hF[heap[l]] < hF[heap[s]]) s = l;
        if (r < heapCount && hF[heap[r]] < hF[heap[s]]) s = r;
        if (s == i) break;
        int t = heap[s]; heap[s] = heap[i]; heap[i] = t;
        heapIdx[heap[s]] = s; heapIdx[heap[i]] = i;
        i = s;
    }
}
static void HPush(int id) { heap[heapCount] = id; heapIdx[id] = heapCount; heapCount++; HBubbleUp(heapCount - 1); }
static int  HPop(void) {
    int top = heap[0];
    heapCount--;
    if (heapCount > 0) { heap[0] = heap[heapCount]; heapIdx[heap[0]] = 0; HBubbleDown(0); }
    heapIdx[top] = -1;
    return top;
}

static int Heur3D(int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = abs(x1-x0), dy = abs(y1-y0), dz = abs(z1-z0);
    if (use8Dir) {
        int mx = dx > dy ? dx : dy, mn = dx < dy ? dx : dy;
        return (mx - mn) * MIN_CELL_COST + mn * 11 + dz * MIN_CELL_COST;
    }
    return (dx + dy + dz) * MIN_CELL_COST;
}

// Returns g-cost at goal, or -1 if unreachable.
static int HeapAStar(Point s, Point g, bool useGen) {
    AllocNodes();
    benchNodesExplored = 0;

    if (useGen) {
        curGen++;
    } else {
        int n = gridWidth * gridHeight * gridDepth;
        for (int i = 0; i < n; i++) { hG[i] = COST_INF; hF[i] = COST_INF; hParent[i] = -1; hState[i] = 0; heapIdx[i] = -1; }
    }
    heapCount = 0;

    int dx4[] = {0,1,0,-1}, dy4[] = {-1,0,1,0};
    int dx8[] = {0,1,1,1,0,-1,-1,-1}, dy8[] = {-1,-1,0,1,1,1,0,-1};
    int *dx = use8Dir ? dx8 : dx4, *dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    int sid = NodeId(s.x, s.y, s.z);
    Touch(sid, useGen);
    hG[sid] = 0;
    hF[sid] = Heur3D(s.x, s.y, s.z, g.x, g.y, g.z);
    hState[sid] = 1;
    HPush(sid);

    while (heapCount > 0) {
        int cur = HPop();
        int cz = cur / (gridHeight * gridWidth);
        int rem = cur % (gridHeight * gridWidth);
        int cy = rem / gridWidth, cx = rem % gridWidth;

        if (cx == g.x && cy == g.y && cz == g.z) return hG[cur];
        if (hState[cur] == 2) continue;
        hState[cur] = 2;
        benchNodesExplored++;

        for (int i = 0; i < numDirs; i++) {
            int nx = cx + dx[i], ny = cy + dy[i], nz = cz;
            if (!IsCellWalkableAt(nz, ny, nx)) continue;
            int nid = NodeId(nx, ny, nz);
            Touch(nid, useGen);
            if (hState[nid] == 2) continue;
            if (use8Dir && dx[i] && dy[i]) {
                if (!IsCellWalkableAt(cz, cy, cx + dx[i]) || !IsCellWalkableAt(cz, cy + dy[i], cx)) continue;
            }
            if (!CanEnterRampFromSide(nx, ny, nz, cx, cy)) continue;
            int baseCost = (dx[i] && dy[i]) ? 14 : 10;
            int ng = hG[cur] + (baseCost * GetCellMoveCost(nx, ny, nz)) / 10;
            if (ng < hG[nid]) {
                hG[nid] = ng;
                hF[nid] = ng + Heur3D(nx, ny, nz, g.x, g.y, g.z);
                hParent[nid] = cur;
                if (heapIdx[nid] >= 0) HBubbleUp(heapIdx[nid]);
                else { hState[nid] = 1; HPush(nid); }
            }
        }

        // ladders
        if (CanClimbUpAt(cx, cy, cz)) {
            int nid = NodeId(cx, cy, cz + 1);
            Touch(nid, useGen);
            if (hState[nid] != 2) {
                int ng = hG[cur] + GetCellMoveCost(cx, cy, cz + 1);
                if (ng < hG[nid]) {
                    hG[nid] = ng; hF[nid] = ng + Heur3D(cx, cy, cz+1, g.x, g.y, g.z); hParent[nid] = cur;
                    if (heapIdx[nid] >= 0) HBubbleUp(heapIdx[nid]); else { hState[nid] = 1; HPush(nid); }
                }
            }
        }
        if (CanClimbDownAt(cx, cy, cz)) {
            int nid = NodeId(cx, cy, cz - 1);
            Touch(nid, useGen);
            if (hState[nid] != 2) {
                int ng = hG[cur] + GetCellMoveCost(cx, cy, cz - 1);
                if (ng < hG[nid]) {
                    hG[nid] = ng; hF[nid] = ng + Heur3D(cx, cy, cz-1, g.x, g.y, g.z); hParent[nid] = cur;
                    if (heapIdx[nid] >= 0) HBubbleUp(heapIdx[nid]); else { hState[nid] = 1; HPush(nid); }
                }
            }
        }
        // ramp up
        if (CanWalkUpRampAt(cx, cy, cz)) {
            int hdx, hdy;
            GetRampHighSideOffset(grid[cz][cy][cx], &hdx, &hdy);
            int ex = cx + hdx, ey = cy + hdy, ez = cz + 1;
            if (ex >= 0 && ex < gridWidth && ey >= 0 && ey < gridHeight && ez < gridDepth) {
                int nid = NodeId(ex, ey, ez);
                Touch(nid, useGen);
                if (hState[nid] != 2) {
                    int ng = hG[cur] + (14 * GetCellMoveCost(ex, ey, ez)) / 10;
                    if (ng < hG[nid]) {
                        hG[nid] = ng; hF[nid] = ng + Heur3D(ex, ey, ez, g.x, g.y, g.z); hParent[nid] = cur;
                        if (heapIdx[nid] >= 0) HBubbleUp(heapIdx[nid]); else { hState[nid] = 1; HPush(nid); }
                    }
                }
            }
        }
        // ramp down
        if (cz > 0) {
            int off[4][2] = {{0,-1},{1,0},{0,1},{-1,0}};
            CellType match[4] = {CELL_RAMP_S, CELL_RAMP_W, CELL_RAMP_N, CELL_RAMP_E};
            for (int i = 0; i < 4; i++) {
                int rx = cx + off[i][0], ry = cy + off[i][1], rz = cz - 1;
                if (rx < 0 || rx >= gridWidth || ry < 0 || ry >= gridHeight) continue;
                if (grid[rz][ry][rx] != match[i]) continue;
                if (!IsCellWalkableAt(rz, ry, rx)) continue;
                int nid = NodeId(rx, ry, rz);
                Touch(nid, useGen);
                if (hState[nid] == 2) continue;
                int ng = hG[cur] + (14 * GetCellMoveCost(rx, ry, rz)) / 10;
                if (ng < hG[nid]) {
                    hG[nid] = ng; hF[nid] = ng + Heur3D(rx, ry, rz, g.x, g.y, g.z); hParent[nid] = cur;
                    if (heapIdx[nid] >= 0) HBubbleUp(heapIdx[nid]); else { hState[nid] = 1; HPush(nid); }
                }
            }
        }
    }
    return -1;
}

// ---------------------------------------------------------------- scenarios
static void MakeOpen(int w, int h) {
    InitGridWithSizeAndChunkSize(w, h, 16, 16);
    InitWater();
    InitSnow();
    // z=0 is walkable by the implicit-bedrock rule
}

// Serpentine walls force heavy exploration (realistic colony interior)
static void MakeSerpentine(int w, int h) {
    MakeOpen(w, h);
    for (int y = 4; y < h - 4; y += 8) {
        int gapX = ((y / 8) % 2 == 0) ? w - 2 : 1;
        for (int x = 0; x < w; x++) {
            if (x == gapX) continue;
            grid[0][y][x] = CELL_WALL;
        }
    }
}

static int RunAStarGoalCost(void) {
    // RunAStar leaves g at goal in nodeData
    return nodeData[goalPos.z][goalPos.y][goalPos.x].g;
}

static void Scenario2(const char *name, int w, int h, int iters, bool runV0) {
    Point s = {1, 1, 0};
    Point g = {w - 2, h - 2, 0};

    startPos = s; goalPos = g;

    // --- V0: current RunAStar
    double e0 = 0; int cost0 = 0, explored0 = 0;
    if (runV0) {
        double t0 = Now();
        for (int i = 0; i < iters; i++) RunAStar();
        e0 = (Now() - t0) / iters * 1000.0;
        cost0 = RunAStarGoalCost();
        explored0 = nodesExplored;
    }

    // --- V1: heap, full clear
    double t1 = Now();
    int cost1 = 0;
    for (int i = 0; i < iters; i++) cost1 = HeapAStar(s, g, false);
    double e1 = (Now() - t1) / iters * 1000.0;

    // --- V2: heap + generation stamps
    double t2 = Now();
    int cost2 = 0;
    for (int i = 0; i < iters; i++) cost2 = HeapAStar(s, g, true);
    double e2 = (Now() - t2) / iters * 1000.0;

    if (runV0) {
        printf("%-22s %4dx%-4d  pops=%-7d  cost=%d/%d/%d %s\n",
               name, w, h, explored0, cost0, cost1, cost2,
               (cost0 == cost1 && cost1 == cost2) ? "OK" : "*** MISMATCH ***");
        printf("    V0 scan+clear : %9.3f ms\n", e0);
        printf("    V1 heap+clear : %9.3f ms   (%.0fx faster)\n", e1, e0 / e1);
        printf("    V2 heap+gen   : %9.3f ms   (%.0fx faster)\n\n", e2, e0 / e2);
    } else {
        // Extrapolate V0 rather than run it: at these sizes a single path takes
        // minutes to hours. V0's work is pops * (W*H*D) cells scanned; the constant
        // is measured from the 128x128 open case (2017 ms / 6872 pops / 128*128*16
        // cells = ~1.09 ns per cell scanned) on the machine that produced the doc's
        // table. Re-derive it if the numbers here look off on other hardware.
        double predicted = (double)benchNodesExplored * gridWidth * gridHeight * gridDepth * NS_PER_SCANNED_CELL;
        printf("%-22s %4dx%-4d  pops=%-7d  cost=%d/%d %s\n",
               name, w, h, benchNodesExplored, cost1, cost2,
               (cost1 == cost2) ? "OK" : "*** MISMATCH ***");
        printf("    V0 scan+clear : %9.0f ms  (extrapolated)\n", predicted);
        printf("    V1 heap+clear : %9.3f ms   (~%.0fx faster)\n", e1, predicted / e1);
        printf("    V2 heap+gen   : %9.3f ms   (~%.0fx faster)\n\n", e2, predicted / e2);
    }
}

static void Scenario(const char *name, int w, int h, int iters) { Scenario2(name, w, h, iters, true); }

int main(void) {
    SetTraceLogLevel(LOG_NONE);
    use8Dir = true;

    printf("=== A*: linear scan vs binary heap ===\n");
    printf("gridDepth is forced to %d for every grid (grid.c:47)\n\n", MAX_GRID_DEPTH);

    MakeOpen(32, 32);      Scenario("open", 32, 32, 20);
    MakeOpen(64, 64);      Scenario("open", 64, 64, 5);
    MakeOpen(128, 128);    Scenario("open", 128, 128, 2);
    MakeSerpentine(64, 64);   Scenario("serpentine", 64, 64, 3);
    MakeSerpentine(128, 128); Scenario("serpentine", 128, 128, 1);

    printf("--- realistic game grid sizes (V0 extrapolated, too slow to run) ---\n\n");
    MakeOpen(256, 256);       Scenario2("open", 256, 256, 20, false);
    MakeOpen(512, 512);       Scenario2("open", 512, 512, 10, false);
    MakeSerpentine(512, 512); Scenario2("serpentine", 512, 512, 5, false);

    return 0;
}
