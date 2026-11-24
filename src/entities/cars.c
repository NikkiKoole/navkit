// cars.c - Cars experiment (see cars.h, docs/research/cars-experiment.md)
#include "cars.h"
#include "mover.h"            // CELL_SIZE
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Car cars[MAX_CARS];
int carCount = 0;

float carLength = 2.0f;
float carWidth = 2.0f;
float carMaxSpeed = 6.0f;
float carAccel = 3.0f;
float carBrake = 8.0f;
float carLatAccel = 5.0f;
float carMaxSteerDeg = 35.0f;
float carSteerRate = 150.0f;
float carLookahead = 1.0f;
float carStopWait = 2.0f;
float carPlanMargin = 3.0f;
bool carDebug = true;

int carPlanCount = 0, carPlanExpansionsMax = 0;
double carPlanMsTotal = 0, carPlanMsMax = 0;

#define CAR_PI 3.14159265f
#define CAR_DEG (CAR_PI / 180.0f)
#define CAR_REVERSE_SPEED 1.5f     // cells/s: top speed in reverse
#define CAR_STUCK_RETRY 3.0f       // seconds before a stuck car plans again
#define CAR_BODY_INSET 1.0f        // px: the body is this much smaller than carLength x carWidth
#define CAR_GRAZE 1.0f             // px: driving tolerates touching a wall this much (planning: none)

static float WheelBase(void) { return carLength * CELL_SIZE * 0.6f; }
static float MaxSteer(void) { return carMaxSteerDeg * CAR_DEG; }

static float AngleWrap(float a) {
    while (a > CAR_PI) a -= 2 * CAR_PI;
    while (a < -CAR_PI) a += 2 * CAR_PI;
    return a;
}

const char* DriveStateName(int s) {
    switch (s) {
        case DRIVE_PARKED: return "parked";
        case DRIVE_FOLLOW: return "driving";
        case DRIVE_STUCK: return "stuck";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Where a car may be: walkable, but no ladders, ramps (stairs), elevator shafts
// ---------------------------------------------------------------------------
bool CarCellDrivable(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    if (!IsCellWalkableAt(z, y, x)) return false;
    CellType c = GRID(x, y, z);
    if (CellIsLadder(c) || CellIsRamp(c) || CellIsShaft(c)) return false;
    return true;
}

// The car body (a rotated rectangle, grown by `margin` px on every side) against
// every non-drivable cell it might touch: exact separating-axis test (cell axes
// x/y, car axes u/v).
static bool PoseBlockedMargin(float x, float y, int z, float heading, float margin) {
    float hl = carLength * CELL_SIZE * 0.5f - CAR_BODY_INSET + margin;
    float hw = carWidth * CELL_SIZE * 0.5f - CAR_BODY_INSET + margin;
    float c = cosf(heading), s = sinf(heading);
    float ex = fabsf(c) * hl + fabsf(s) * hw;   // half extents of the bounding box
    float ey = fabsf(s) * hl + fabsf(c) * hw;
    int x0 = (int)floorf((x - ex) / CELL_SIZE), x1 = (int)floorf((x + ex) / CELL_SIZE);
    int y0 = (int)floorf((y - ey) / CELL_SIZE), y1 = (int)floorf((y + ey) / CELL_SIZE);
    float h = CELL_SIZE * 0.5f;
    for (int cy = y0; cy <= y1; cy++)
        for (int cx = x0; cx <= x1; cx++) {
            if (CarCellDrivable(cx, cy, z)) continue;
            float dx = (cx + 0.5f) * CELL_SIZE - x, dy = (cy + 0.5f) * CELL_SIZE - y;
            if (fabsf(dx) >= ex + h || fabsf(dy) >= ey + h) continue;
            float du = dx * c + dy * s, dv = -dx * s + dy * c;
            float r = h * (fabsf(c) + fabsf(s));
            if (fabsf(du) >= hl + r || fabsf(dv) >= hw + r) continue;
            return true;
        }
    return false;
}

bool CarPoseBlocked(float x, float y, int z, float heading) {
    return PoseBlockedMargin(x, y, z, heading, 0.0f);
}

// Kinematic bicycle about the rear axle: the centre pose after driving `dist`
// pixels (signed: negative = reverse) with the wheels at `steer`
static void AdvancePose(float x, float y, float h, float dist, float steer,
                        float* ox, float* oy, float* oh) {
    float wb = WheelBase();
    float rx = x - cosf(h) * wb * 0.5f, ry = y - sinf(h) * wb * 0.5f;
    float nh = h + dist / wb * tanf(steer);
    float mh = (h + nh) * 0.5f;
    rx += cosf(mh) * dist;
    ry += sinf(mh) * dist;
    *oh = AngleWrap(nh);
    *ox = rx + cosf(*oh) * wb * 0.5f;
    *oy = ry + sinf(*oh) * wb * 0.5f;
}

// ---------------------------------------------------------------------------
// Clearance map (one z level): clr[x,y] = side of the largest drivable square
// whose top-left cell is (x,y). A grid node (x,y) is valid if clr >= S
// (S = ceil(carWidth)); its car centre is the middle of that square.
// ---------------------------------------------------------------------------
static unsigned char* clr = NULL;
static int clrW = 0, clrH = 0, clrZ = -1;

static void BuildClearance(int z) {
    if (clrW != gridWidth || clrH != gridHeight || !clr) {
        free(clr);
        clrW = gridWidth; clrH = gridHeight;
        clr = malloc((size_t)clrW * clrH);
    }
    clrZ = z;
    for (int y = clrH - 1; y >= 0; y--)
        for (int x = clrW - 1; x >= 0; x--) {
            int v = 0;
            if (CarCellDrivable(x, y, z)) {
                int r = x + 1 < clrW ? clr[y * clrW + x + 1] : 0;
                int d = y + 1 < clrH ? clr[(y + 1) * clrW + x] : 0;
                int rd = (x + 1 < clrW && y + 1 < clrH) ? clr[(y + 1) * clrW + x + 1] : 0;
                int m = r < d ? r : d;
                if (rd < m) m = rd;
                v = m + 1;
                if (v > 255) v = 255;
            }
            clr[y * clrW + x] = (unsigned char)v;
        }
}

int CarNodeSize(void) {
    int s = (int)ceilf(carWidth - 0.01f);
    return s < 1 ? 1 : s;
}

int CarClearance(int x, int y, int z) {
    if (!clr || clrZ != z || clrW != gridWidth || clrH != gridHeight) BuildClearance(z);
    if (x < 0 || y < 0 || x >= clrW || y >= clrH) return 0;
    return clr[y * clrW + x];
}

static bool NodeOk(int x, int y) {
    return x >= 0 && y >= 0 && x < clrW && y < clrH && clr[y * clrW + x] >= CarNodeSize();
}

// ---------------------------------------------------------------------------
// Heuristic: grid distance (tenths of a cell) from every node to the goal node,
// Dijkstra over valid nodes, 8 directions (no corner cutting). "How far is the
// goal going round the buildings": Hybrid A* would wander into dead ends without it.
// ---------------------------------------------------------------------------
static const int dirX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int dirY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

typedef struct { float f; int idx; } CarHeapNode;
static CarHeapNode* heap = NULL;
static int heapCap = 0, heapLen = 0;

static void HeapPush(float f, int idx) {
    if (heapLen >= heapCap) {
        heapCap = heapCap ? heapCap * 2 : 4096;
        heap = realloc(heap, sizeof(CarHeapNode) * heapCap);
    }
    int i = heapLen++;
    heap[i] = (CarHeapNode){f, idx};
    while (i > 0) {
        int p = (i - 1) / 2;
        if (heap[p].f <= heap[i].f) break;
        CarHeapNode t = heap[p]; heap[p] = heap[i]; heap[i] = t;
        i = p;
    }
}

static CarHeapNode HeapPop(void) {
    CarHeapNode top = heap[0];
    heap[0] = heap[--heapLen];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < heapLen && heap[l].f < heap[m].f) m = l;
        if (r < heapLen && heap[r].f < heap[m].f) m = r;
        if (m == i) break;
        CarHeapNode t = heap[m]; heap[m] = heap[i]; heap[i] = t;
        i = m;
    }
    return top;
}

static int* holo = NULL;   // per node: distance to goal (tenths of a cell), -1 = unreachable
static int holoCap = 0;

static void BuildHolonomic(int gx, int gy) {
    int n = clrW * clrH;
    if (n > holoCap) { holoCap = n; holo = realloc(holo, sizeof(int) * n); }
    for (int i = 0; i < n; i++) holo[i] = -1;
    heapLen = 0;
    holo[gy * clrW + gx] = 0;
    HeapPush(0, gy * clrW + gx);
    while (heapLen > 0) {
        CarHeapNode cur = HeapPop();
        int ci = cur.idx;
        if ((int)cur.f != holo[ci]) continue;
        int cx = ci % clrW, cy = ci / clrW;
        for (int d = 0; d < 8; d++) {
            int nx = cx + dirX[d], ny = cy + dirY[d];
            if (!NodeOk(nx, ny)) continue;
            bool diag = dirX[d] && dirY[d];
            if (diag && (!NodeOk(cx + dirX[d], cy) || !NodeOk(cx, cy + dirY[d]))) continue;
            int ni = ny * clrW + nx;
            int g = holo[ci] + (diag ? 14 : 10);
            if (holo[ni] < 0 || g < holo[ni]) { holo[ni] = g; HeapPush((float)g, ni); }
        }
    }
}

// Pixel position of a node's car centre
static void NodeCentre(int nx, int ny, float* px, float* py) {
    float h = CarNodeSize() * 0.5f;
    *px = (nx + h) * CELL_SIZE;
    *py = (ny + h) * CELL_SIZE;
}

// Valid node whose centre is nearest to (px,py), within `radius` nodes
static bool NearestNode(float px, float py, int radius, int* outX, int* outY) {
    float h = CarNodeSize() * 0.5f;
    int cx = (int)floorf(px / CELL_SIZE - h + 0.5f), cy = (int)floorf(py / CELL_SIZE - h + 0.5f);
    float best = 1e30f;
    bool found = false;
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = cx + dx, ny = cy + dy;
            if (!NodeOk(nx, ny)) continue;
            float qx, qy;
            NodeCentre(nx, ny, &qx, &qy);
            float d = (qx - px) * (qx - px) + (qy - py) * (qy - py);
            if (d < best) { best = d; *outX = nx; *outY = ny; found = true; }
        }
    return found;
}

// Heuristic in pixels: grid distance from the nearest node, never less than straight
static float Heuristic(float x, float y, float gx, float gy) {
    float e = sqrtf((x - gx) * (x - gx) + (y - gy) * (y - gy));
    int nx, ny;
    if (!NearestNode(x, y, 1, &nx, &ny)) return e * 1.5f;   // squeezed between nodes
    int d = holo[ny * clrW + nx];
    if (d < 0) return 1e7f;                                  // cut off from the goal
    float qx, qy;
    NodeCentre(nx, ny, &qx, &qy);
    float h = d * CELL_SIZE / 10.0f + sqrtf((qx - x) * (qx - x) + (qy - y) * (qy - y)) * 0.5f;
    return h > e ? h : e;
}

// ---------------------------------------------------------------------------
// Hybrid A*: states (x, y, heading) binned at half a cell x 32 headings; each
// expansion drives 14 arcs of HA_STEP px (wheel at 0, +-lock/8, +-lock/2, +-lock; forward and
// reverse), checking the body (grown by carPlanMargin) every 4 px. Costs: distance, reverse
// x2, a gear change 1.5 cells, a little for steering. Goal: centre within 0.35
// cells of the goal node's centre, any heading.
// ---------------------------------------------------------------------------
#define HA_XY_RES 16.0f
#define HA_HEADINGS 32
#define HA_STEP 20.0f
#define HA_SUBSTEPS 5
#define HA_MAX_EXPAND 150000

typedef struct { float x, y, h, g; int parent; signed char dir; float steer; } HaNode;
static HaNode* haPool = NULL;
static int haPoolCap = 0, haPoolLen = 0;
static unsigned char* haClosed = NULL;
static int haClosedCap = 0;

static int HaPoolAdd(HaNode n) {
    if (haPoolLen >= haPoolCap) {
        haPoolCap = haPoolCap ? haPoolCap * 2 : 65536;
        haPool = realloc(haPool, sizeof(HaNode) * haPoolCap);
    }
    haPool[haPoolLen] = n;
    return haPoolLen++;
}

static int HaBin(float x, float y, float h, int bw, int bh) {
    int bx = (int)(x / HA_XY_RES), by = (int)(y / HA_XY_RES);
    if (x < 0 || y < 0 || bx >= bw || by >= bh) return -1;
    int hb = (int)lroundf(h / (2 * CAR_PI / HA_HEADINGS));
    hb = ((hb % HA_HEADINGS) + HA_HEADINGS) % HA_HEADINGS;
    return (by * bw + bx) * HA_HEADINGS + hb;
}

// A car that stopped closer to a wall than the margin must still be able to plan
// its way out: near its start (haLooseR px) only the body itself has to be free.
static float haStartX, haStartY, haLooseR;

// Drive one arc from node `from`; false if the body hits something on the way
static bool HaArc(const HaNode* from, int dir, float steer, int z, float* ox, float* oy, float* oh) {
    float x = from->x, y = from->y, h = from->h;
    float sub = HA_STEP / HA_SUBSTEPS;
    for (int k = 0; k < HA_SUBSTEPS; k++) {
        AdvancePose(x, y, h, sub * dir, steer, &x, &y, &h);
        float sx = x - haStartX, sy = y - haStartY;
        float m = (sx * sx + sy * sy < haLooseR * haLooseR) ? 0.0f : carPlanMargin;
        if (PoseBlockedMargin(x, y, z, h, m)) return false;
    }
    *ox = x; *oy = y; *oh = h;
    return true;
}

// Plan from the car's pose to (goalX, goalY) px into its route; false if none
static bool HybridAStar(Car* car, float goalX, float goalY, int* outExpansions) {
    int bw = (int)ceilf(gridWidth * CELL_SIZE / HA_XY_RES), bh = (int)ceilf(gridHeight * CELL_SIZE / HA_XY_RES);
    int nb = bw * bh * HA_HEADINGS;
    if (nb > haClosedCap) { haClosedCap = nb; haClosed = realloc(haClosed, (size_t)nb); }
    memset(haClosed, 0, (size_t)nb);
    haPoolLen = 0;
    heapLen = 0;
    float lock = MaxSteer();
    const float steers[7] = {-lock, -lock * 0.5f, -lock * 0.125f, 0, lock * 0.125f, lock * 0.5f, lock};
    haStartX = car->x; haStartY = car->y;
    haLooseR = PoseBlockedMargin(car->x, car->y, car->z, car->heading, carPlanMargin) ? 1.5f * CELL_SIZE : 0.0f;
    int start = HaPoolAdd((HaNode){car->x, car->y, car->heading, 0, -1, 0, 0});
    HeapPush(Heuristic(car->x, car->y, goalX, goalY), start);
    int goal = -1, expansions = 0;
    float tol2 = (0.35f * CELL_SIZE) * (0.35f * CELL_SIZE);
    while (heapLen > 0 && expansions < HA_MAX_EXPAND) {
        int ci = HeapPop().idx;
        HaNode cur = haPool[ci];
        int bin = HaBin(cur.x, cur.y, cur.h, bw, bh);
        if (bin < 0 || haClosed[bin]) continue;
        haClosed[bin] = 1;
        expansions++;
        float gdx = cur.x - goalX, gdy = cur.y - goalY;
        if (gdx * gdx + gdy * gdy < tol2) { goal = ci; break; }
        for (int dir = 1; dir >= -1; dir -= 2)
            for (int s = 0; s < 7; s++) {
                float nx, ny, nh;
                if (!HaArc(&cur, dir, steers[s], car->z, &nx, &ny, &nh)) continue;
                int nbin = HaBin(nx, ny, nh, bw, bh);
                if (nbin < 0 || haClosed[nbin]) continue;
                float cost = HA_STEP * (dir < 0 ? 2.0f : 1.0f);
                if (cur.dir != 0 && cur.dir != dir) cost += 1.5f * CELL_SIZE;   // gear change
                cost += fabsf(steers[s]) / lock * 1.0f;
                if (steers[s] != cur.steer) cost += 1.0f;
                float g = cur.g + cost;
                int ni = HaPoolAdd((HaNode){nx, ny, nh, g, ci, (signed char)dir, steers[s]});
                HeapPush(g + 1.1f * Heuristic(nx, ny, goalX, goalY), ni);
            }
    }
    *outExpansions = expansions;
    if (goal < 0) return false;

    // Rebuild: replay each node's arc from its parent in HA_SUBSTEPS points
    int nodes = 0;
    for (int i = goal; i >= 0; i = haPool[i].parent) nodes++;
    int len = 1 + (nodes - 1) * HA_SUBSTEPS;
    if (len > CAR_MAX_PATH) return false;
    int k = len - 1;
    for (int i = goal; haPool[i].parent >= 0; i = haPool[i].parent) {
        const HaNode* n = &haPool[i];
        const HaNode* p = &haPool[n->parent];
        float x = p->x, y = p->y, h = p->h;
        float px[HA_SUBSTEPS], py[HA_SUBSTEPS], ph[HA_SUBSTEPS];
        float sub = HA_STEP / HA_SUBSTEPS;
        for (int j = 0; j < HA_SUBSTEPS; j++) {
            AdvancePose(x, y, h, sub * n->dir, n->steer, &x, &y, &h);
            px[j] = x; py[j] = y; ph[j] = h;
        }
        for (int j = HA_SUBSTEPS - 1; j >= 0; j--, k--) {
            car->pathX[k] = px[j];
            car->pathY[k] = py[j];
            car->pathH[k] = ph[j];
            car->pathDir[k - 1] = n->dir;       // the step into point k
            car->pathSteer[k - 1] = n->steer;
        }
    }
    car->pathX[0] = car->x;
    car->pathY[0] = car->y;
    car->pathH[0] = car->heading;
    car->pathDir[len - 1] = len > 1 ? car->pathDir[len - 2] : 1;
    car->pathSteer[len - 1] = len > 1 ? car->pathSteer[len - 2] : 0;
    car->pathLen = len;
    car->pathIdx = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Spawning, stops, planning
// ---------------------------------------------------------------------------
void ClearCars(void) {
    memset(cars, 0, sizeof(cars));
    carCount = 0;
    clrZ = -1;   // the world may have changed
    carPlanCount = carPlanExpansionsMax = 0;
    carPlanMsTotal = carPlanMsMax = 0;
}

int SpawnCar(int cellX, int cellY, int z, float headingDeg) {
    if (carCount >= MAX_CARS) return -1;
    int i = carCount++;
    Car* c = &cars[i];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->x = (cellX + 0.5f) * CELL_SIZE;
    c->y = (cellY + 0.5f) * CELL_SIZE;
    c->z = z;
    c->heading = headingDeg * CAR_DEG;
    c->state = DRIVE_PARKED;
    c->stopIdx = -1;
    return i;
}

void CarAddStop(int idx, int cellX, int cellY, char name) {
    Car* c = &cars[idx];
    if (c->stopCount >= CAR_MAX_STOPS) return;
    c->stopX[c->stopCount] = cellX;
    c->stopY[c->stopCount] = cellY;
    c->stopName[c->stopCount] = name;
    c->stopCount++;
}

static bool CarTrace(void) {
    static int trace = -1;
    if (trace < 0) trace = getenv("CAR_TRACE") != NULL;
    return trace;
}

// CAR_DUMP=file: record the run (map, every plan, poses at 20 Hz) as JSON lines,
// for an offline replay. Experiment-only; off unless the env var is set.
static FILE* CarDump(void) {
    static int init = 0;
    static FILE* f = NULL;
    if (!init) {
        init = 1;
        const char* path = getenv("CAR_DUMP");
        if (path) f = fopen(path, "w");
        if (f) {
            fprintf(f, "{\"map\":{\"w\":%d,\"h\":%d,\"cell\":%d,\"rows\":[", gridWidth, gridHeight, (int)CELL_SIZE);
            for (int y = 0; y < gridHeight; y++) {
                fputc('"', f);
                for (int x = 0; x < gridWidth; x++) fputc(CarCellDrivable(x, y, 1) ? '.' : '#', f);
                fprintf(f, "\"%s", y < gridHeight - 1 ? "," : "");
            }
            fprintf(f, "]}}\n");
        }
    }
    return f;
}

static void SetState(Car* c, int s) {
    if (CarTrace())
        printf("[car %d] %s -> %s at (%.2f,%.2f) hdg %.0f spd %.2f path %d/%d\n",
               (int)(c - cars), DriveStateName(c->state), DriveStateName(s), c->x / CELL_SIZE, c->y / CELL_SIZE,
               c->heading / CAR_DEG, c->speed / CELL_SIZE, c->pathIdx, c->pathLen);
    c->state = s;
    c->stateTime = 0;
}

static double NowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

bool CarDriveTo(int idx, int cellX, int cellY) {
    Car* c = &cars[idx];
    double t0 = NowMs();
    BuildClearance(c->z);   // cheap (one level); keeps up with edits
    int gx, gy;
    float tx = (cellX + 0.5f) * CELL_SIZE, ty = (cellY + 0.5f) * CELL_SIZE;
    c->goalCellX = cellX;
    c->goalCellY = cellY;
    bool ok = NearestNode(tx, ty, 4, &gx, &gy);
    int exp = 0;
    if (ok) {
        BuildHolonomic(gx, gy);
        float gpx, gpy;
        NodeCentre(gx, gy, &gpx, &gpy);
        ok = HybridAStar(c, gpx, gpy, &exp);
    }
    double ms = NowMs() - t0;
    carPlanCount++;
    carPlanMsTotal += ms;
    if (ms > carPlanMsMax) carPlanMsMax = ms;
    if (exp > carPlanExpansionsMax) carPlanExpansionsMax = exp;
    if (CarTrace())
        printf("[car %d] plan to (%d,%d): %s, %d expansions, %d points, %.1f ms\n",
               idx, cellX, cellY, ok ? "ok" : "FAILED", exp, ok ? c->pathLen : 0, ms);
    if (!ok) { c->planFails++; return false; }
    c->legTime = 0;
    c->blockedTime = 0;
    FILE* df = CarDump();
    if (df) {
        fprintf(df, "{\"plan\":%d,\"t\":%.2f,\"goal\":[%d,%d],\"ms\":%.1f,\"pts\":[", idx, (double)gameTime, cellX, cellY, ms);
        for (int i = 0; i < c->pathLen; i++)
            fprintf(df, "[%.1f,%.1f,%d]%s", c->pathX[i], c->pathY[i], c->pathDir[i], i < c->pathLen - 1 ? "," : "");
        fprintf(df, "]}\n");
    }
    SetState(c, DRIVE_FOLLOW);
    return true;
}

// ---------------------------------------------------------------------------
// Following the route: pure pursuit on the current stretch (same gear), stop at
// the gear change, go on in the other gear
// ---------------------------------------------------------------------------

// Last point of the stretch the car is on (where the gear changes, or the end)
static int CuspIndex(const Car* c) {
    int d = c->pathDir[c->pathIdx], i = c->pathIdx;
    while (i + 1 < c->pathLen - 1 && c->pathDir[i + 1] == d) i++;
    return i + 1;
}

// Move pathIdx along the stretch (never past the cusp); the projection point, and
// whether the car is past the cusp
static bool ProjectOnStretch(Car* c, int cusp, float* outX, float* outY) {
    for (;;) {
        int i = c->pathIdx;
        float ax = c->pathX[i], ay = c->pathY[i], bx = c->pathX[i + 1], by = c->pathY[i + 1];
        float dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
        float t = L2 > 0 ? ((c->x - ax) * dx + (c->y - ay) * dy) / L2 : 1.0f;
        if (t >= 1.0f && i + 1 < cusp) { c->pathIdx++; continue; }
        if (t < 0) t = 0;
        bool past = t >= 1.0f;
        if (t > 1) t = 1;
        *outX = ax + t * dx; *outY = ay + t * dy;
        return past;
    }
}

// Point `dist` px on from the projection, along the stretch; beyond the cusp it
// carries on straight along the last step (so pursuit aims through it). Also the
// length left to the cusp.
static void AlongStretch(const Car* c, int cusp, float px, float py, float dist,
                         float* outX, float* outY, float* toCusp) {
    float left = dist, rem = 0, x = px, y = py;
    bool placed = false;
    for (int i = c->pathIdx + 1; i <= cusp; i++) {
        float dx = c->pathX[i] - x, dy = c->pathY[i] - y;
        float L = sqrtf(dx * dx + dy * dy);
        if (!placed && L >= left && L > 0) {
            *outX = x + dx * left / L; *outY = y + dy * left / L;
            placed = true;
        }
        if (!placed) left -= L;
        rem += L;
        x = c->pathX[i]; y = c->pathY[i];
    }
    if (!placed) {
        int a = cusp > 0 ? cusp - 1 : 0;
        float dx = c->pathX[cusp] - c->pathX[a], dy = c->pathY[cusp] - c->pathY[a];
        float L = sqrtf(dx * dx + dy * dy);
        if (L < 1e-3f) { dx = cosf(c->heading) * c->pathDir[a]; dy = sinf(c->heading) * c->pathDir[a]; L = 1; }
        *outX = x + dx / L * left; *outY = y + dy / L * left;
    }
    *toCusp = rem;
}

static void SteerToward(Car* c, float want, float dt) {
    float maxS = MaxSteer();
    if (want > maxS) want = maxS;
    if (want < -maxS) want = -maxS;
    float rate = carSteerRate * CAR_DEG * dt;
    float d = want - c->steer;
    if (d > rate) d = rate;
    if (d < -rate) d = -rate;
    c->steer += d;
}

static void SpeedToward(Car* c, float want, float dt) {
    float accel = carAccel * CELL_SIZE * dt, brake = carBrake * CELL_SIZE * dt;
    float d = want - c->speed;
    bool slowing = (c->speed > 0 && d < 0) || (c->speed < 0 && d > 0);
    float lim = slowing ? brake : accel;
    if (d > lim) d = lim;
    if (d < -lim) d = -lim;
    c->speed += d;
}

// Is the arc ahead (sign = direction of travel) blocked within `dist` px?
static bool ArcBlocked(const Car* c, float steer, float dist, int sign) {
    for (float d = 2.0f; d <= dist + 0.01f; d += 4.0f) {
        float x, y, h;
        AdvancePose(c->x, c->y, c->heading, d * sign, steer, &x, &y, &h);
        if (PoseBlockedMargin(x, y, c->z, h, -CAR_GRAZE)) return true;
    }
    return false;
}

// Move by speed*dt; false (and stopped) if the new pose is blocked
static bool Move(Car* c, float dt) {
    float dist = c->speed * dt;
    if (fabsf(dist) < 1e-4f) return true;
    float x, y, h;
    AdvancePose(c->x, c->y, c->heading, dist, c->steer, &x, &y, &h);
    if (PoseBlockedMargin(x, y, c->z, h, -CAR_GRAZE)) {
        c->speed = 0;
        c->bumps++;
        return false;
    }
    c->x = x; c->y = y; c->heading = h;
    return true;
}

static void GiveUp(Car* c) {
    c->stuck++;
    c->speed = 0;
    SetState(c, DRIVE_STUCK);
}

static void Arrive(Car* c) {
    c->speed = 0;
    c->legsDone++;
    c->driveTime += c->legTime;
    SetState(c, DRIVE_PARKED);
}

static void NextStop(int idx) {
    Car* c = &cars[idx];
    if (c->stopCount == 0) return;
    for (int tries = 0; tries < c->stopCount; tries++) {
        c->stopIdx = (c->stopIdx + 1) % c->stopCount;
        if (CarDriveTo(idx, c->stopX[c->stopIdx], c->stopY[c->stopIdx])) return;
    }
    GiveUp(c);
}

static void Replan(int idx, const char* why) {
    Car* c = &cars[idx];
    if (CarTrace()) printf("[car %d] replan: %s\n", idx, why);
    c->replans++;
    c->speed = 0;
    if (!CarDriveTo(idx, c->goalCellX, c->goalCellY)) GiveUp(c);
}

static void TickFollow(int idx, float dt) {
    Car* c = &cars[idx];
    if (c->pathLen < 2) { Arrive(c); return; }
    int cusp = CuspIndex(c);
    int gear = c->pathDir[c->pathIdx];
    float px, py;
    bool past = ProjectOnStretch(c, cusp, &px, &py);
    float v = fabsf(c->speed);
    float Ld = carLookahead * CELL_SIZE;
    if (gear > 0 && v * 0.5f > Ld) Ld = v * 0.5f;
    AlongStretch(c, cusp, px, py, Ld, &c->lookX, &c->lookY, &c->toCusp);

    // At the cusp: the end, or change gear
    if (past || c->toCusp < 3.0f) {
        c->speed = 0;
        if (cusp >= c->pathLen - 1) { Arrive(c); return; }
        c->pathIdx = cusp;
        c->gearChanges++;
        return;
    }
    // Drifted off the route (pushed by a corner, say): plan again from here
    float ox = px - c->x, oy = py - c->y;
    if (ox * ox + oy * oy > (0.5f * CELL_SIZE) * (0.5f * CELL_SIZE)) { Replan(idx, "off route"); return; }

    // Steering: the planned wheel angle (feedforward) plus corrections for heading
    // error and sideways error (Stanley-style). Heading rate is speed * tan(steer),
    // so in reverse both corrections flip sign.
    float wb = WheelBase();
    int i0 = c->pathIdx;
    float sx = c->pathX[i0 + 1] - c->pathX[i0], sy = c->pathY[i0 + 1] - c->pathY[i0];
    float sL2 = sx * sx + sy * sy;
    float t = sL2 > 0 ? ((c->x - c->pathX[i0]) * sx + (c->y - c->pathY[i0]) * sy) / sL2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    float refH = c->pathH[i0] + AngleWrap(c->pathH[i0 + 1] - c->pathH[i0]) * t;
    float headErr = AngleWrap(refH - c->heading);
    float nX = -sinf(c->heading), nY = cosf(c->heading);       // the car's left (+heading side)
    float lat = ((c->x - px) * nX + (c->y - py) * nY) / CELL_SIZE;   // cells, + = left of route
    float want = c->pathSteer[i0] + gear * 1.0f * headErr
               - gear * atanf(2.0f * lat / (v / CELL_SIZE + 1.0f));
    SteerToward(c, want, dt);
    // Standing (start, gear change): turn the wheel first, then pull away
    float wantC = fmaxf(-MaxSteer(), fminf(MaxSteer(), want));
    if (v < 0.2f * CELL_SIZE && fabsf(c->steer - wantC) > 5.0f * CAR_DEG) { c->speed = 0; return; }

    // Speed: top speed (reverse: slow), cornering grip, stopping at the cusp
    float brake = carBrake * CELL_SIZE;
    float vmax = (gear > 0 ? carMaxSpeed : fminf(carMaxSpeed, CAR_REVERSE_SPEED)) * CELL_SIZE;
    float tanS = fabsf(tanf(c->steer));
    if (tanS > 1e-3f) vmax = fminf(vmax, sqrtf(carLatAccel * CELL_SIZE * wb / tanS));
    // the sharpest bend within braking distance + 2 cells: slow down before it
    {
        float reach = v * v / (2 * brake) + 2.0f * CELL_SIZE, d = 0;
        float x = px, y = py;
        for (int i = c->pathIdx + 1; i + 1 <= cusp && d < reach; i++) {
            float dx = c->pathX[i] - x, dy = c->pathY[i] - y;
            float L = sqrtf(dx * dx + dy * dy);
            d += L;
            float ex = c->pathX[i + 1] - c->pathX[i], ey = c->pathY[i + 1] - c->pathY[i];
            float L2 = sqrtf(ex * ex + ey * ey);
            if (L > 0.5f && L2 > 0.5f) {
                float curv = fabsf(AngleWrap(atan2f(ey, ex) - atan2f(dy, dx))) / L2;   // 1/px
                if (curv > 1e-4f) {
                    float vc = sqrtf(carLatAccel * CELL_SIZE / curv);
                    vmax = fminf(vmax, sqrtf(vc * vc + 2 * brake * d));
                }
            }
            x = c->pathX[i]; y = c->pathY[i];
        }
    }
    vmax = fminf(vmax, sqrtf(2 * brake * 0.8f * c->toCusp));
    if (vmax < 0.3f * CELL_SIZE) vmax = 0.3f * CELL_SIZE;   // creep, don't stall

    bool blocked = false;
    float stopDist = v * v / (2 * brake);
    if (ArcBlocked(c, c->steer, stopDist + 2.0f, gear)) {
        vmax = 0;
        // Stopped: really blocked only if the wheel angle we're turning to is too
        // (otherwise wait for the wheel)
        if (v < 0.05f * CELL_SIZE && ArcBlocked(c, wantC, 4.0f, gear)) blocked = true;
    }
    SpeedToward(c, vmax * gear, dt);
    if (!Move(c, dt)) blocked = true;
    if (blocked) c->blockedTime += dt;
    else c->blockedTime = 0;
    if (c->blockedTime > 0.5f) {
        if (CarTrace())
            printf("  blocked: pos (%.2f,%.2f) hdg %.1f steer %.1f want %.1f gear %d idx %d/%d cusp %d toCusp %.1f arcCur %d arcWant %d\n",
                   c->x / CELL_SIZE, c->y / CELL_SIZE, c->heading / CAR_DEG, c->steer / CAR_DEG, wantC / CAR_DEG, gear,
                   c->pathIdx, c->pathLen, cusp, c->toCusp, ArcBlocked(c, c->steer, 4.0f, gear), ArcBlocked(c, wantC, 4.0f, gear));
        Replan(idx, "blocked");
    }
}

void CarsTick(float dt) {
    static int dumpTick = 0;
    FILE* df = CarDump();
    if (df && (dumpTick++ % 3) == 0)
        for (int i = 0; i < carCount; i++)
            if (cars[i].active)
                fprintf(df, "{\"c\":%d,\"t\":%.2f,\"p\":[%.1f,%.1f,%.3f,%.3f,%.2f,%d]}\n", i, (double)gameTime,
                        cars[i].x, cars[i].y, cars[i].heading, cars[i].steer, cars[i].speed / CELL_SIZE, cars[i].state);
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active) continue;
        c->stateTime += dt;
        if (c->state == DRIVE_FOLLOW) c->legTime += dt;
        switch (c->state) {
            case DRIVE_PARKED:
                if (c->stopCount > 0 && (c->stopIdx < 0 || c->stateTime >= carStopWait)) NextStop(i);
                break;
            case DRIVE_FOLLOW: TickFollow(i, dt); break;
            case DRIVE_STUCK:
                if (c->stateTime >= CAR_STUCK_RETRY && !CarDriveTo(i, c->goalCellX, c->goalCellY)) NextStop(i);
                break;
        }
    }
}

void CarsPrintStats(void) {
    printf("Cars: %d (length %.1f, width %.1f cells, lock %.0f deg, top %.1f cells/s, wheelbase %.2f cells, turn radius %.2f cells, plan margin %.1f px)\n",
           carCount, carLength, carWidth, carMaxSteerDeg, carMaxSpeed,
           WheelBase() / CELL_SIZE, WheelBase() / tanf(MaxSteer()) / CELL_SIZE, carPlanMargin);
    printf("Car plans: %d, avg %.1f ms, max %.1f ms, max %d expansions\n", carPlanCount,
           carPlanCount ? carPlanMsTotal / carPlanCount : 0.0, carPlanMsMax, carPlanExpansionsMax);
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active) continue;
        printf("  car %d: %d legs done, %.1f s driving (avg %.1f s/leg), %d gear changes, %d replans, %d bumps, %d stuck, %d plan fails, now %s at (%.1f,%.1f)\n",
               i, c->legsDone, c->driveTime, c->legsDone ? c->driveTime / c->legsDone : 0.0f,
               c->gearChanges, c->replans, c->bumps, c->stuck, c->planFails, DriveStateName(c->state),
               c->x / CELL_SIZE, c->y / CELL_SIZE);
    }
}
