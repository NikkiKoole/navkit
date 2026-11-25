// cars.c - Cars experiment (see cars.h, docs/research/cars-experiment.md)
#include "cars.h"
#include "roads.h"
#include "traffic.h"
#include "slots.h"
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
bool carFollow = false;
int carTrafficCount = 30;
int carCityCount = 100;
int carCitySeed = 7;
bool carCityLights = true;
int carCityRoundabouts = 3;
bool carCityAvenues = true;
bool carCityParking = true;
bool carCitySlots = false;
float carGapCells = 1.0f;
float carCitySpacingX = 72.0f, carCitySpacingY = 40.0f;   // long blocks east-west: long straights
int carCityJitter = 2;
float carCityBends = 0.1f;
int carCityLayout = 0;   // 0 grid, 1 Manhattan
float carCityCabs = 0.3f;
bool carCityGreenWave = true;
float carCitySetback = 4.0f;   // compact junctions (with near-square angles even steered 2x2 cars fit; 7 for jittery cities)

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

float CarAngleWrap(float a) {
    while (a > CAR_PI) a -= 2 * CAR_PI;
    while (a < -CAR_PI) a += 2 * CAR_PI;
    return a;
}

const char* DriveStateName(int s) {
    switch (s) {
        case DRIVE_PARKED: return "parked";
        case DRIVE_FOLLOW: return "driving";
        case DRIVE_STUCK: return "stuck";
        case DRIVE_ROAD: return "road";
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
bool CarPoseBlockedMargin(float x, float y, int z, float heading, float margin) {
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

// ---------------------------------------------------------------------------
// Other cars: two bodies (rotated rectangles) overlap? Separating axes: each
// body's length and width directions. `margin` px grows both bodies.
// ---------------------------------------------------------------------------
bool CarBodiesOverlap(float x1, float y1, float h1, float x2, float y2, float h2, float margin) {
    float hl = carLength * CELL_SIZE * 0.5f - CAR_BODY_INSET + margin * 0.5f;
    float hw = carWidth * CELL_SIZE * 0.5f - CAR_BODY_INSET + margin * 0.5f;
    float ax[4][2] = {{cosf(h1), sinf(h1)}, {-sinf(h1), cosf(h1)}, {cosf(h2), sinf(h2)}, {-sinf(h2), cosf(h2)}};
    float dx = x2 - x1, dy = y2 - y1;
    for (int k = 0; k < 4; k++) {
        float a0 = ax[k][0], a1 = ax[k][1];
        float r1 = hl * fabsf(ax[0][0] * a0 + ax[0][1] * a1) + hw * fabsf(ax[1][0] * a0 + ax[1][1] * a1);
        float r2 = hl * fabsf(ax[2][0] * a0 + ax[2][1] * a1) + hw * fabsf(ax[3][0] * a0 + ax[3][1] * a1);
        if (fabsf(dx * a0 + dy * a1) > r1 + r2) return false;   // separated on this axis
    }
    return true;
}

// The first other car a body at (x, y, h) would touch (2 px margin), or -1
static int CarInWay(const Car* self, float x, float y, float h) {
    float reach = (carLength + carWidth) * CELL_SIZE;   // > the two bodies' diagonals
    for (int i = 0; i < carCount; i++) {
        const Car* o = &cars[i];
        if (o == self || !o->active || o->z != self->z) continue;
        float dx = o->x - x, dy = o->y - y;
        if (dx * dx + dy * dy > reach * reach) continue;
        if (CarBodiesOverlap(x, y, h, o->x, o->y, o->heading, 2.0f)) return i;
    }
    return -1;
}

// Distance along the current arc (sign = direction) to the first car in the way, or -1
static float ArcCarGap(const Car* c, float steer, float maxDist, int sign, int* who) {
    for (float d = 4.0f; d <= maxDist + 0.01f; d += 6.0f) {
        float x, y, h;
        CarAdvancePose(c->x, c->y, c->heading, d * sign, steer, &x, &y, &h);
        int k = CarInWay(c, x, y, h);
        if (k >= 0) { *who = k; return d; }
    }
    *who = -1;
    return -1;
}

bool CarPoseBlocked(float x, float y, int z, float heading) {
    return CarPoseBlockedMargin(x, y, z, heading, 0.0f);
}

// Kinematic bicycle about the rear axle: the centre pose after driving `dist`
// pixels (signed: negative = reverse) with the wheels at `steer`
void CarAdvancePose(float x, float y, float h, float dist, float steer,
                        float* ox, float* oy, float* oh) {
    float wb = WheelBase();
    float rx = x - cosf(h) * wb * 0.5f, ry = y - sinf(h) * wb * 0.5f;
    float nh = h + dist / wb * tanf(steer);
    float mh = (h + nh) * 0.5f;
    rx += cosf(mh) * dist;
    ry += sinf(mh) * dist;
    *oh = CarAngleWrap(nh);
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
#define HA_WALL_DIST 24.0f   // px: routes keep this far off walls where there is room
#define HA_WALL_COST 1.0f    // extra cost (x arc length) for an arc closer than that

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
        CarAdvancePose(x, y, h, sub * dir, steer, &x, &y, &h);
        float sx = x - haStartX, sy = y - haStartY;
        float m = (sx * sx + sy * sy < haLooseR * haLooseR) ? -CAR_GRAZE : carPlanMargin;   // a car that scraped may start grazing
        if (CarPoseBlockedMargin(x, y, z, h, m)) return false;
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
    haLooseR = CarPoseBlockedMargin(car->x, car->y, car->z, car->heading, carPlanMargin) ? 1.5f * CELL_SIZE : 0.0f;
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
                // keep off the walls where there is room (drivers keep to the middle of the road)
                if (CarPoseBlockedMargin(nx, ny, car->z, nh, HA_WALL_DIST)) cost += HA_STEP * HA_WALL_COST;
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
            CarAdvancePose(x, y, h, sub * n->dir, n->steer, &x, &y, &h);
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
// Smoothing (open road only: tight places keep the planned route). Hybrid A* routes
// zigzag on straight roads: every pose is binned, and
// when the straight-ahead bin is taken the search has to steer a little. Per stretch
// of one gear: take every HA_SUBSTEPS-th point (one per arc) and pull each towards
// the middle of its neighbours while a weaker pull holds it near where it was
// planned: zigzags flatten, real bends keep their shape. The stretch's first two and
// last two points stay put (start pose, end, gear changes keep their headings). A
// Catmull-Rom curve through the points gives the route back at every index, with
// heading and wheel angle from the curve. Where the result comes closer to a wall
// than the plan margin, or bends tighter than the wheel can, the points around it
// are pinned to the planned route and the stretch is smoothed again.
// ---------------------------------------------------------------------------
#define SMOOTH_ITER 150
#define SMOOTH_PULL 0.5f     // towards the neighbours
#define SMOOTH_HOLD 0.005f   // back towards the planned point (weak: only real bends hold)
static float smX[CAR_MAX_PATH], smY[CAR_MAX_PATH], smOX[CAR_MAX_PATH], smOY[CAR_MAX_PATH];
static int smIdx[CAR_MAX_PATH];
static bool smPin[CAR_MAX_PATH];
static float outX[CAR_MAX_PATH], outY[CAR_MAX_PATH], outH[CAR_MAX_PATH], outS[CAR_MAX_PATH];

static float StretchHeading(float dx, float dy, int dir) {
    float h = atan2f(dy, dx);
    return dir < 0 ? CarAngleWrap(h + CAR_PI) : h;
}

// Curve through smX/smY into out* for path indices a..b (b = last)
static void SmoothRebuild(const Car* c, int a, int b, int n, int dir) {
    for (int k = 0; k + 1 < n; k++) {
        int i0 = smIdx[k], i1 = smIdx[k + 1];
        int km = k > 0 ? k - 1 : k, kp = k + 2 < n ? k + 2 : k + 1;
        for (int i = i0; i < i1; i++) {
            float t = (float)(i - i0) / (float)(i1 - i0), t2 = t * t, t3 = t2 * t;
            float w0 = -0.5f * t3 + t2 - 0.5f * t, w1 = 1.5f * t3 - 2.5f * t2 + 1;
            float w2 = -1.5f * t3 + 2 * t2 + 0.5f * t, w3 = 0.5f * t3 - 0.5f * t2;
            outX[i] = w0 * smX[km] + w1 * smX[k] + w2 * smX[k + 1] + w3 * smX[kp];
            outY[i] = w0 * smY[km] + w1 * smY[k] + w2 * smY[k + 1] + w3 * smY[kp];
        }
    }
    outX[b] = smX[n - 1]; outY[b] = smY[n - 1];
    outH[a] = c->pathH[a]; outH[b] = c->pathH[b];
    for (int i = a + 1; i < b; i++) outH[i] = StretchHeading(outX[i + 1] - outX[i - 1], outY[i + 1] - outY[i - 1], dir);
    float wb = WheelBase();
    for (int i = a; i < b; i++) {
        int j = i + 2 <= b ? i + 2 : b, i0 = j - 2 >= a ? j - 2 : a;   // two steps: less noise
        float dx = outX[j] - outX[i0], dy = outY[j] - outY[i0];
        float ds = sqrtf(dx * dx + dy * dy) * dir;
        outS[i] = fabsf(ds) > 1e-3f ? atanf(wb * CarAngleWrap(outH[j] - outH[i0]) / ds) : 0;
    }
    outS[b] = outS[b > a ? b - 1 : a];
}

static bool SmoothStretch(Car* c, int a, int b) {
    int dir = c->pathDir[a];
    int n = 0;
    for (int i = a; i < b; i += HA_SUBSTEPS) smIdx[n++] = i;
    smIdx[n++] = b;
    if (n < 5) return false;
    for (int k = 0; k < n; k++) {
        smOX[k] = c->pathX[smIdx[k]]; smOY[k] = c->pathY[smIdx[k]];
        // tight places (less than 0.4 cell of room round the body) stay exactly as planned
        smPin[k] = k < 2 || k >= n - 2 || CarPoseBlockedMargin(smOX[k], smOY[k], c->z, c->pathH[smIdx[k]], 0.4f * CELL_SIZE);
    }
    float lock = MaxSteer();
    for (int round = 0; round < 6; round++) {
        for (int k = 0; k < n; k++) { smX[k] = smOX[k]; smY[k] = smOY[k]; }
        for (int it = 0; it < SMOOTH_ITER; it++)
            for (int k = 1; k < n - 1; k++) {
                if (smPin[k]) continue;
                smX[k] += SMOOTH_PULL * ((smX[k - 1] + smX[k + 1]) * 0.5f - smX[k]) + SMOOTH_HOLD * (smOX[k] - smX[k]);
                smY[k] += SMOOTH_PULL * ((smY[k - 1] + smY[k + 1]) * 0.5f - smY[k]) + SMOOTH_HOLD * (smOY[k] - smY[k]);
            }
        SmoothRebuild(c, a, b, n, dir);
        // Bad points: closer to a wall than the margin (unless the plan was too, near
        // the start), or a bend the wheel can't make. Pin the arcs around them.
        bool bad = false;
        for (int i = a + 1; i < b; i++) {
            bool hit = CarPoseBlockedMargin(outX[i], outY[i], c->z, outH[i], carPlanMargin)
                       && !CarPoseBlockedMargin(c->pathX[i], c->pathY[i], c->z, c->pathH[i], carPlanMargin);
            if (!hit && fabsf(outS[i]) <= lock * 0.9f) continue;
            int k = 0;
            while (k + 1 < n && smIdx[k + 1] <= i) k++;
            for (int q = k - 2; q <= k + 3; q++)
                if (q >= 0 && q < n && !smPin[q]) { smPin[q] = true; bad = true; }
        }
        if (!bad) {
            for (int i = a + 1; i < b; i++) { c->pathX[i] = outX[i]; c->pathY[i] = outY[i]; c->pathH[i] = outH[i]; }
            for (int i = a; i < b; i++) c->pathSteer[i] = outS[i];
            return true;
        }
    }
    return false;   // keep the planned stretch
}

static void SmoothPath(Car* c) {
    int a = 0;
    while (a < c->pathLen - 1) {
        int b = a;
        while (b < c->pathLen - 1 && c->pathDir[b] == c->pathDir[a]) b++;
        SmoothStretch(c, a, b);
        a = b;
    }
}

// ---------------------------------------------------------------------------
// Spawning, stops, planning
// ---------------------------------------------------------------------------
static void CarFreePath(Car* c) {
    free(c->pathX); free(c->pathY); free(c->pathDir); free(c->pathH); free(c->pathSteer);
    c->pathX = c->pathY = c->pathH = c->pathSteer = NULL;
    c->pathDir = NULL;
}

static void CarEnsurePath(Car* c) {
    if (c->pathX) return;
    c->pathX = malloc(sizeof(float) * CAR_MAX_PATH);
    c->pathY = malloc(sizeof(float) * CAR_MAX_PATH);
    c->pathH = malloc(sizeof(float) * CAR_MAX_PATH);
    c->pathSteer = malloc(sizeof(float) * CAR_MAX_PATH);
    c->pathDir = malloc(CAR_MAX_PATH);
}

void ClearCars(void) {
    for (int i = 0; i < MAX_CARS; i++) CarFreePath(&cars[i]);
    RoadsClear();
    TrafficReset();
    SlotsClear();
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
    CarFreePath(c);
    memset(c, 0, sizeof(*c));
    c->holdNode = -1;
    c->active = true;
    c->x = (cellX + 0.5f) * CELL_SIZE;
    c->y = (cellY + 0.5f) * CELL_SIZE;
    c->z = z;
    c->heading = headingDeg * CAR_DEG;
    c->state = DRIVE_PARKED;
    c->stopIdx = -1;
    c->whyCar = -1;
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
    CarEnsurePath(c);
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
        int sx, sy;
        if (NearestNode(c->x, c->y, 2, &sx, &sy) && holo[sy * clrW + sx] < 0) ok = false;   // cut off: don't search
        if (ok) {
            float gpx, gpy;
            NodeCentre(gx, gy, &gpx, &gpy);
            ok = HybridAStar(c, gpx, gpy, &exp);
            if (ok) SmoothPath(c);
        }
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

void CarSteerToward(Car* c, float want, float dt) {
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
        CarAdvancePose(c->x, c->y, c->heading, d * sign, steer, &x, &y, &h);
        if (CarPoseBlockedMargin(x, y, c->z, h, -CAR_GRAZE)) return true;
    }
    return false;
}

// Move by speed*dt. 0 = moved; 1 = a wall in the way (a bump); 2 = another car
// in the way (stopped, no bump: it will move on). Stopped either way.
static int Move(Car* c, float dt) {
    float dist = c->speed * dt;
    if (fabsf(dist) < 1e-4f) return 0;
    float x, y, h;
    CarAdvancePose(c->x, c->y, c->heading, dist, c->steer, &x, &y, &h);
    if (CarPoseBlockedMargin(x, y, c->z, h, -CAR_GRAZE)) {
        c->speed = 0;
        c->bumps++;
        return 1;
    }
    if (CarInWay(c, x, y, h) >= 0) { c->speed = 0; return 2; }
    c->x = x; c->y = y; c->heading = h;
    return 0;
}

static void GiveUp(Car* c) {
    c->stuck++;
    c->speed = 0;
    SetState(c, DRIVE_STUCK);
}

static void Arrive(Car* c) {
    c->speed = 0;
    c->replanStreak = 0;
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
    float mx = c->x - c->replanX, my = c->y - c->replanY;
    if (mx * mx + my * my > (carLength * CELL_SIZE) * (carLength * CELL_SIZE)) c->replanStreak = 0;
    c->replanX = c->x; c->replanY = c->y;
    // The same spot over and over (a route the planner thinks is clear but the car
    // can't drive): give up this stop rather than loop
    if (++c->replanStreak > 6) { c->replanStreak = 0; GiveUp(c); return; }
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
    float refH = c->pathH[i0] + CarAngleWrap(c->pathH[i0 + 1] - c->pathH[i0]) * t;
    float headErr = CarAngleWrap(refH - c->heading);
    float nX = -sinf(c->heading), nY = cosf(c->heading);       // the car's left (+heading side)
    float lat = ((c->x - px) * nX + (c->y - py) * nY) / CELL_SIZE;   // cells, + = left of route
    float want = c->pathSteer[i0] + gear * 1.0f * headErr
               - gear * atanf(2.0f * lat / (v / CELL_SIZE + 1.0f));
    CarSteerToward(c, want, dt);
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
            int j = i + 4 <= cusp ? i + 4 : cusp;   // ~half a cell ahead
            float ex = c->pathX[j] - c->pathX[i], ey = c->pathY[j] - c->pathY[i];
            float L2 = sqrtf(ex * ex + ey * ey);
            if (L > 0.5f && L2 > 0.5f) {
                float curv = fabsf(CarAngleWrap(c->pathH[j] - c->pathH[i])) / L2;   // 1/px, over ~half a cell
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

    // Another car on the arc ahead: brake to stop half a cell behind it
    // (car-following like trackgen's traffic mode: the leader moving on frees the road)
    int who;
    float gapd = ArcCarGap(c, c->steer, v * v / (2 * brake) + 1.5f * CELL_SIZE, gear, &who);
    if (gapd >= 0) {
        float room = gapd - 0.5f * CELL_SIZE;
        vmax = fminf(vmax, room > 0 ? sqrtf(2 * brake * room) : 0);
    }
    c->whyCar = who;

    bool blocked = false;
    float stopDist = v * v / (2 * brake);
    if (ArcBlocked(c, c->steer, stopDist + 2.0f, gear)) {
        vmax = 0;
        // Stopped: really blocked only if the wheel angle we're turning to is too
        // (otherwise wait for the wheel)
        if (v < 0.05f * CELL_SIZE && ArcBlocked(c, wantC, 4.0f, gear)) blocked = true;
    }
    SpeedToward(c, vmax * gear, dt);
    int moved = Move(c, dt);
    if (moved == 1) blocked = true;
    // Waiting behind a car: not "blocked" (no replan: the planner can't see cars)
    if ((who >= 0 || moved == 2) && fabsf(c->speed) < 0.05f * CELL_SIZE) {
        float before = c->waitNow;
        c->waitNow += dt;
        c->waitTime += dt;
        if (before < CAR_DEADLOCK_S && c->waitNow >= CAR_DEADLOCK_S) c->deadlocks++;
        // Our stop is taken / occupied: park where we are if close
        float gx = (c->goalCellX + 0.5f) * CELL_SIZE - c->x, gy = (c->goalCellY + 0.5f) * CELL_SIZE - c->y;
        if (gx * gx + gy * gy < (2.0f * carLength * CELL_SIZE) * (2.0f * carLength * CELL_SIZE)) { c->waitNow = 0; Arrive(c); return; }
        c->blockedTime = 0;
        return;
    }
    c->waitNow = 0;
    if (blocked) c->blockedTime += dt;
    else c->blockedTime = 0;
    if (c->blockedTime > 0.5f) {
        if (CarTrace())
            printf("  blocked: pos (%.2f,%.2f) hdg %.1f steer %.1f want %.1f gear %d idx %d/%d cusp %d toCusp %.1f arcCur %d arcWant %d\n",
                   c->x / CELL_SIZE, c->y / CELL_SIZE, c->heading / CAR_DEG, c->steer / CAR_DEG, wantC / CAR_DEG, gear,
                   c->pathIdx, c->pathLen, cusp, c->toCusp, ArcBlocked(c, c->steer, 4.0f, gear), ArcBlocked(c, wantC, 4.0f, gear));
        // Within a car length of the goal: close enough, park here
        float gx = (c->goalCellX + 0.5f) * CELL_SIZE - c->x, gy = (c->goalCellY + 0.5f) * CELL_SIZE - c->y;
        if (gx * gx + gy * gy < (carLength * CELL_SIZE) * (carLength * CELL_SIZE)) { Arrive(c); return; }
        Replan(idx, "blocked");
    }
}

void CarsTick(float dt) {
    TrafficTick(dt);
    static int dumpTick = 0;
    FILE* df = CarDump();
    if (df && (dumpTick++ % 3) == 0)
        for (int i = 0; i < carCount; i++)
            if (cars[i].active)
                fprintf(df, "{\"c\":%d,\"t\":%.2f,\"p\":[%.1f,%.1f,%.3f,%.3f,%.2f,%d]}\n", i, (double)gameTime,
                        cars[i].x, cars[i].y, cars[i].heading, cars[i].steer, cars[i].speed / CELL_SIZE, cars[i].state);
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active || c->onRoad) continue;
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
    if (carCount > 0 && cars[0].onRoad) { TrafficPrintStats(); return; }
    printf("Cars: %d (length %.1f, width %.1f cells, lock %.0f deg, top %.1f cells/s, wheelbase %.2f cells, turn radius %.2f cells, plan margin %.1f px)\n",
           carCount, carLength, carWidth, carMaxSteerDeg, carMaxSpeed,
           WheelBase() / CELL_SIZE, WheelBase() / tanf(MaxSteer()) / CELL_SIZE, carPlanMargin);
    printf("Car plans: %d, avg %.1f ms, max %.1f ms, max %d expansions\n", carPlanCount,
           carPlanCount ? carPlanMsTotal / carPlanCount : 0.0, carPlanMsMax, carPlanExpansionsMax);
    if (carCount > 4) {
        int legs = 0, bumps = 0, stuck = 0, dl = 0, replans = 0, fails = 0, now = 0, moving = 0;
        float drive = 0, wait = 0;
        for (int i = 0; i < carCount; i++) {
            Car* c = &cars[i];
            legs += c->legsDone; drive += c->driveTime; wait += c->waitTime; bumps += c->bumps;
            stuck += c->stuck; dl += c->deadlocks; replans += c->replans; fails += c->planFails;
            if (c->waitNow >= CAR_DEADLOCK_S) now++;
            if (fabsf(c->speed) > 0.5f * CELL_SIZE) moving++;
        }
        printf("Traffic: %d cars, %d trips done (avg %.1f s driving), %.0f s waiting behind cars in all (%.1f per car),\n"
               "  %d deadlocks (stopped behind a car > %.0f s), %d deadlocked right now, %d moving right now,\n"
               "  %d replans, %d bumps, %d stuck, %d plan fails\n",
               carCount, legs, legs ? drive / legs : 0.0f, wait, wait / carCount, dl, CAR_DEADLOCK_S, now, moving,
               replans, bumps, stuck, fails);
        for (int i = 0; i < carCount; i++) {
            Car* c = &cars[i];
            if (c->waitNow < CAR_DEADLOCK_S) continue;
            printf("  deadlocked: car %d at (%.1f,%.1f) heading %.0f, waiting %.0f s behind car %d at (%.1f,%.1f)\n",
                   i, c->x / CELL_SIZE, c->y / CELL_SIZE, c->heading / CAR_DEG, c->waitNow, c->whyCar,
                   c->whyCar >= 0 ? cars[c->whyCar].x / CELL_SIZE : 0, c->whyCar >= 0 ? cars[c->whyCar].y / CELL_SIZE : 0);
        }
        return;
    }
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active) continue;
        printf("  car %d: %d legs done, %.1f s driving (avg %.1f s/leg), %d gear changes, %d replans, %d bumps, %d stuck, %d plan fails, now %s at (%.1f,%.1f)\n",
               i, c->legsDone, c->driveTime, c->legsDone ? c->driveTime / c->legsDone : 0.0f,
               c->gearChanges, c->replans, c->bumps, c->stuck, c->planFails, DriveStateName(c->state),
               c->x / CELL_SIZE, c->y / CELL_SIZE);
    }
}
