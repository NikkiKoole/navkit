// traffic.c - Cars driving on the road network (see traffic.h, roads.h)
//
// Headless diagnostics (env): TR_TRACE=<car> prints that car twice a second; TR_WALLS /
// TR_TOUCH print the first wall / car-car touches; TR_LONG prints why a junction request
// waits 45 s; TR_JDUMP dumps junctions with old requests at the end of the run.
#include "traffic.h"
#include "roads.h"
#include "cars.h"
#include "mover.h"            // CELL_SIZE
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int trafficOverlapMax = 0;
long trafficOverlapSamples = 0;
int trafficOffRoadMax = 0;
int trafficSamples = 0;
float trafficWaitMax = 0;
float trafficJunctionWaitMax = 0;

#define TR_MAX_HOLD 24
#define TR_MAX_REQ 48
#define TR_TIME_GAP 1.0f       // s: IDM time gap
#define TR_MIN_GAP 1.0f        // cells: IDM standstill gap (bumper to bumper)
#define TR_STOP_BACK 0.4f      // cells: stop this far before a junction's start
#define TR_ASK_AHEAD 6.0f      // cells beyond braking distance: ask the junction
#define TR_STANLEY_K 2.5f      // 1/s: sideways error correction
#define TR_QUEUE_COST 8.0f     // cells of route cost per slow car on a lane
#define RD_CONFLICT_CELLS 2.6f  // cells: a car on a sibling curve this close to ours is in our way

typedef struct { short car, seg; } TrHold;
typedef struct { short car, seg; float t, waited; } TrReq;
static TrHold trHold[RD_MAX_NODES][TR_MAX_HOLD];
static int trHoldN[RD_MAX_NODES];
static TrReq trReq[RD_MAX_NODES][TR_MAX_REQ];
static int trReqN[RD_MAX_NODES];
static int segHead[RD_MAX_SEGS], carNext[MAX_CARS];
static float trTime = 0;
static unsigned int trRng = 1;

static unsigned int TrRand(void) { trRng = trRng * 1103515245u + 12345u; return (trRng >> 16) & 0x7fff; }

void TrafficReset(void) {
    memset(trHoldN, 0, sizeof(trHoldN));
    memset(trReqN, 0, sizeof(trReqN));
    trTime = 0;
    trRng = 1;
    trafficOverlapMax = trafficOffRoadMax = trafficSamples = 0;
    trafficOverlapSamples = 0;
    trafficWaitMax = trafficJunctionWaitMax = 0;
}

// ---------------------------------------------------------------------------
// Trips: a random spot on a random lane, routed from the lane we're on
// ---------------------------------------------------------------------------
static bool NewTrip(int i) {
    Car* c = &cars[i];
    int cur = c->route[c->routeIdx];
    for (int tries = 0; tries < 20; tries++) {
        int dl = RoadLaneAt((int)(TrRand() % (unsigned)RoadLaneCount()));
        float len = rdSegs[dl].len;
        if (len < 8 * CELL_SIZE) continue;
        float ds = 3 * CELL_SIZE + (len - 6 * CELL_SIZE) * (TrRand() / 32767.0f);
        int n;
        if (dl == cur && ds > c->s + 4 * CELL_SIZE) { c->route[0] = (short)cur; n = 1; }
        else n = RoadRoute(cur, dl, c->route, CAR_MAX_ROUTE);
        if (n <= 0) continue;
        c->routeLen = n;
        c->routeIdx = 0;
        c->destS = ds;
        c->legTime = 0;
        return true;
    }
    c->routeLen = 1; c->route[0] = (short)cur; c->routeIdx = 0; c->destS = rdSegs[cur].len;
    return false;
}

int TrafficSpawn(int lane, float s, int z) {
    int i = SpawnCar(0, 0, z, 0);
    if (i < 0) return -1;
    Car* c = &cars[i];
    RoadPointAt(lane, s, &c->x, &c->y, &c->heading);
    c->onRoad = true;
    c->state = DRIVE_ROAD;
    c->route[0] = (short)lane;
    c->routeLen = 1;
    c->routeIdx = 0;
    c->s = s;
    c->holdNode = -1;
    NewTrip(i);
    return i;
}

// ---------------------------------------------------------------------------
// Junctions
// ---------------------------------------------------------------------------
static float ExitRoom(int lane) {   // px free at the start of a lane (to the nearest car's rear)
    float room = rdSegs[lane].len;
    for (int o = segHead[lane]; o >= 0; o = carNext[o]) {
        float r = cars[o].s - carLength * CELL_SIZE * 0.5f;
        if (r < room) room = r;
    }
    return room;
}

static void Ask(int i, int connSeg) {
    Car* c = &cars[i];
    if (c->requested || c->holdNode >= 0) return;   // one junction at a time
    int nd = rdSegs[connSeg].node;
    if (trReqN[nd] >= TR_MAX_REQ) return;
    trReq[nd][trReqN[nd]++] = (TrReq){(short)i, (short)connSeg, trTime, 0};
    c->requested = true;
}

static void Release(int i) {
    Car* c = &cars[i];
    if (c->holdNode < 0) return;
    int nd = rdSegs[c->holdNode].node;
    for (int k = 0; k < trHoldN[nd]; k++)
        if (trHold[nd][k].car == i) { trHold[nd][k] = trHold[nd][--trHoldN[nd]]; break; }
    c->holdNode = -1;
}

// Grant in arrival order: a request is granted if it conflicts with no connector held
// or asked for earlier, and its exit lane has room for the car (plus the cars already
// granted into that lane)
static void ServeJunctions(float dt) {
    float need = (carLength + TR_MIN_GAP) * CELL_SIZE;
    for (int nd = 0; nd < rdNodeCount; nd++) {
        if (trReqN[nd] == 0) continue;
        const RdNode* N = &rdNodes[nd];
        unsigned char claimed[RD_MAX_CONN] = {0};
        for (int k = 0; k < trHoldN[nd]; k++) claimed[rdSegs[trHold[nd][k].seg].ci] = 1;
        int k = 0;
        while (k < trReqN[nd]) {
            TrReq* r = &trReq[nd][k];
            int ci = rdSegs[r->seg].ci;
            bool clash = false;
            for (int j = 0; j < N->nConn && !clash; j++) if (claimed[j] && N->conflict[ci][j]) clash = true;
            if (!clash) {
                int out = rdSegs[r->seg].to;
                float room = ExitRoom(out);
                for (int h = 0; h < trHoldN[nd]; h++) {
                    const Car* hc = &cars[trHold[nd][h].car];
                    if (rdSegs[trHold[nd][h].seg].to == out && hc->route[hc->routeIdx] != out) room -= need;
                }
                if (room >= need && trHoldN[nd] < TR_MAX_HOLD) {
                    trHold[nd][trHoldN[nd]++] = (TrHold){r->car, r->seg};
                    cars[r->car].holdNode = r->seg;
                    cars[r->car].requested = false;
                    claimed[ci] = 1;
                    trReq[nd][k] = trReq[nd][--trReqN[nd]];
                    // keep arrival order for the rest: re-sort the tail (small)
                    for (int a = k; a < trReqN[nd]; a++)
                        for (int b = a + 1; b < trReqN[nd]; b++)
                            if (trReq[nd][b].t < trReq[nd][a].t) { TrReq t = trReq[nd][a]; trReq[nd][a] = trReq[nd][b]; trReq[nd][b] = t; }
                    continue;
                }
            }
            // Waiting for a crossing path: later conflicting requests wait behind it (so it
            // can't starve). Waiting only for room past the junction: it isn't in anyone's way.
            if (clash) claimed[ci] = 1;
            r->waited += dt;
            if (getenv("TR_LONG") && r->waited >= 45 && r->waited - dt < 45) {
                printf("long wait t %.0f: car %d at junction %d (%d roads) wants conn %d (ci %d -> lane %d): %s, room %.1f\n", trTime, r->car, nd,
                       N->nEdge, r->seg, ci, rdSegs[r->seg].to, clash ? "crossing path claimed" : "no room past the junction", ExitRoom(rdSegs[r->seg].to) / CELL_SIZE);
                for (int h = 0; h < trHoldN[nd]; h++) {
                    const Car* hc = &cars[trHold[nd][h].car];
                    printf("    held: car %d ci %d conflict %d, car on seg %d s %.1f v %.1f why %d\n", trHold[nd][h].car, rdSegs[trHold[nd][h].seg].ci,
                           N->conflict[ci][rdSegs[trHold[nd][h].seg].ci], hc->route[hc->routeIdx], hc->s / CELL_SIZE, hc->speed / CELL_SIZE, hc->why);
                }
                for (int q = 0; q < k; q++)
                    printf("    asked earlier: car %d ci %d conflict %d waited %.0f\n", trReq[nd][q].car, rdSegs[trReq[nd][q].seg].ci,
                           N->conflict[ci][rdSegs[trReq[nd][q].seg].ci], trReq[nd][q].waited);
            }
            if (r->waited > trafficJunctionWaitMax) trafficJunctionWaitMax = r->waited;
            k++;
        }
    }
}

// ---------------------------------------------------------------------------
// One car
// ---------------------------------------------------------------------------
// The point and heading `ahead` px further along the route (clamped at its ends)
static void RouteAheadH(const Car* c, float ahead, float* x, float* y, float* h) {
    int k = c->routeIdx;
    float s = c->s + ahead;
    while (k < c->routeLen - 1 && s > rdSegs[c->route[k]].len) { s -= rdSegs[c->route[k]].len; k++; }
    while (k > 0 && s < 0) { k--; s += rdSegs[c->route[k]].len; }
    RoadPointAt(c->route[k], s, x, y, h);
}

// Point, heading and curvature (1/px, signed like the heading) `ahead` px along the route
static void RouteFrame(const Car* c, float ahead, float* x, float* y, float* h, float* kappa) {
    float ax, ay, bx, by, ha, hb;
    RouteAheadH(c, ahead, x, y, h);
    RouteAheadH(c, ahead - 12.0f, &ax, &ay, &ha);
    RouteAheadH(c, ahead + 12.0f, &bx, &by, &hb);
    float d = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    *kappa = d > 1 ? CarAngleWrap(hb - ha) / d : 0;
}

// Nearest point to (x, y) on a segment, searching around arc length s0
static float ProjectOn(int seg, float s0, float x, float y) {
    const RdSeg* sg = &rdSegs[seg];
    int c0 = (int)(s0 / 8.0f);
    int a = c0 - 3 < 0 ? 0 : c0 - 3, b = c0 + 6 > sg->n - 2 ? sg->n - 2 : c0 + 6;
    float best = 1e30f, bs = s0;
    for (int i = a; i <= b; i++) {
        int k = sg->p0 + i;
        float ex = rdX[k + 1] - rdX[k], ey = rdY[k + 1] - rdY[k], L2 = ex * ex + ey * ey;
        float t = L2 > 0 ? ((x - rdX[k]) * ex + (y - rdY[k]) * ey) / L2 : 0;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        float px = rdX[k] + ex * t - x, py = rdY[k] + ey * t - y, d = px * px + py * py;
        if (d < best) { best = d; bs = rdS[k] + (rdS[k + 1] - rdS[k]) * t; }
    }
    return bs;
}

static void DriveCar(int i, float dt) {
    Car* c = &cars[i];
    float CS = CELL_SIZE, L = carLength * CS;
    float v = c->speed > 0 ? c->speed : 0;
    float bc = carBrake * CS;
    float brakeD = v * v / (2 * bc);

    // Look along the route: the car ahead, a junction not granted yet, bends
    float gap = 1e9f, vLead = 0;
    int why = WHY_FREE;
    float vAllow = fminf(carMaxSpeed, rdSegs[c->route[c->routeIdx]].vmax) * CS;
    float lookD = brakeD + 3 * L + TR_ASK_AHEAD * CS;
    float acc = -c->s;   // distance from the car centre to the start of segment k
    for (int k = c->routeIdx; k < c->routeLen && acc < lookD; k++) {
        int sg = c->route[k];
        if (k > c->routeIdx) {
            if (rdSegs[sg].kind == RD_CONN && c->holdNode != sg) {
                if (acc - L * 0.5f < brakeD + TR_ASK_AHEAD * CS) Ask(i, sg);
                float d = acc - L * 0.5f - TR_STOP_BACK * CS;
                if (d < gap) { gap = d; vLead = 0; why = WHY_JUNCTION; }
                break;
            }
            float vb = rdSegs[sg].vmax * CS, room = acc - L * 0.5f;
            float va = sqrtf(vb * vb + 2 * bc * (room > 0 ? room : 0));
            if (va < vAllow) { vAllow = va; if (why == WHY_FREE) why = WHY_BEND; }
        }
        bool found = false;
        for (int o = segHead[sg]; o >= 0; o = carNext[o]) {
            if (o == i) continue;
            if (k == c->routeIdx && cars[o].s <= c->s) continue;
            float d = acc + cars[o].s - L;   // bumper to bumper
            if (d < gap) { gap = d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; }
        }
        // A junction curve: cars that came from our lane onto a sibling curve are ahead of
        // us too, until the curves part by a car width
        if (rdSegs[sg].kind == RD_CONN) {
            const RdNode* N = &rdNodes[rdSegs[sg].node];
            for (int q = 0; q < N->nConn; q++) {
                int sib = N->conn[q];
                if (sib == sg || rdSegs[sib].from != rdSegs[sg].from) continue;
                for (int o = segHead[sib]; o >= 0; o = carNext[o]) {
                    if (o == i) continue;
                    float so = cars[o].s;
                    if (k == c->routeIdx && so <= c->s) continue;
                    float mx, my, mh;
                    RoadPointAt(sg, so, &mx, &my, &mh);
                    float dx = cars[o].x - mx, dy = cars[o].y - my;
                    if (dx * dx + dy * dy > (RD_CONFLICT_CELLS * CS) * (RD_CONFLICT_CELLS * CS)) continue;
                    float d = acc + so - L;
                    if (d < gap) { gap = d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; }
                }
            }
        }
        if (found) break;
        acc += rdSegs[sg].len;
    }
    // The trip's end: don't overshoot it (a new trip starts there)
    c->gap = gap;
    c->why = why;
    static int traceCar = -2;
    if (traceCar == -2) traceCar = getenv("TR_TRACE") ? atoi(getenv("TR_TRACE")) : -1;
    if (i == traceCar && ((int)(trTime * 60) % 30) == 0)
        printf("t %.1f car %d seg %d (%s) s %.1f/%.1f v %.2f gap %.1f why %d vAllow %.1f hold %d req %d routeIdx %d/%d\n", trTime, i, c->route[c->routeIdx], rdSegs[c->route[c->routeIdx]].kind == RD_CONN ? "conn" : "lane", c->s / CS, rdSegs[c->route[c->routeIdx]].len / CS, v / CS, gap / CS, why, vAllow / CS, c->holdNode, c->requested, c->routeIdx, c->routeLen);

    // IDM
    float a0 = carAccel * CS, v0 = fmaxf(vAllow, 0.5f * CS), s0 = TR_MIN_GAP * CS;
    float sStar = s0 + v * TR_TIME_GAP + v * (v - vLead) / (2 * sqrtf(a0 * bc));
    float freeTerm = 1 - powf(v / v0, 4);
    float inter = gap < 1e8f ? (sStar / fmaxf(gap, 1.0f)) * (sStar / fmaxf(gap, 1.0f)) : 0;
    float a = a0 * (freeTerm - inter);
    if (a < -3 * bc) a = -3 * bc;
    v += a * dt;
    if (v < 0) v = 0;
    if (gap < 0.05f * CS && v > 0) v = 0;
    c->speed = v;

    // Steering: Stanley at the front axle (pure pursuit cut the inside of junction
    // curves by more than the lane's slack): the road's curvature there (feedforward),
    // plus the heading error, plus a correction for the sideways error
    float wb = carLength * CS * 0.6f;
    float px, py, hp, kappa;
    RouteFrame(c, wb * 0.5f, &px, &py, &hp, &kappa);
    float fx = c->x + cosf(c->heading) * wb * 0.5f, fy = c->y + sinf(c->heading) * wb * 0.5f;
    float e = (fx - px) * -sinf(hp) + (fy - py) * cosf(hp);   // + = front is on the +90 degree side
    float want = CarAngleWrap(hp - c->heading) - atan2f(TR_STANLEY_K * e, v + 0.5f * CS) + atanf(wb * kappa);
    CarSteerToward(c, want, dt);
    c->lookX = px; c->lookY = py;

    // Move
    float nx, ny, nh;
    CarAdvancePose(c->x, c->y, c->heading, v * dt, c->steer, &nx, &ny, &nh);
    c->x = nx; c->y = ny; c->heading = nh;

    // Where are we along the route now
    float sGuess = c->s + v * dt;
    int seg = c->route[c->routeIdx];
    while (sGuess > rdSegs[seg].len && c->routeIdx < c->routeLen - 1) {
        sGuess -= rdSegs[seg].len;
        c->routeIdx++;
        seg = c->route[c->routeIdx];
    }
    c->s = ProjectOn(seg, sGuess, c->x, c->y);

    // Release the junction once we're through it: the connector is the segment just
    // behind us and our rear is out of it
    if (c->holdNode >= 0 && c->routeIdx > 0 && c->route[c->routeIdx - 1] == c->holdNode &&
        c->s > L * 0.5f + 0.3f * CS)
        Release(i);

    // Stats
    c->legTime += dt;
    c->driveTime += dt;
    if (v < 0.1f * CS) { c->stillTime += dt; if (c->stillTime > trafficWaitMax) trafficWaitMax = c->stillTime; }
    else c->stillTime = 0;

    // Trip done: the next one starts from here
    if (c->routeIdx == c->routeLen - 1 && c->s >= c->destS) {
        c->legsDone++;
        NewTrip(i);
    }
}

// ---------------------------------------------------------------------------
// "Not silly" samples: cars touching each other, cars touching walls
// ---------------------------------------------------------------------------
static void Sample(void) {
    int pairs = 0, off = 0;
    for (int i = 0; i < carCount; i++) {
        const Car* a = &cars[i];
        if (!a->active || !a->onRoad) continue;
        if (CarPoseBlockedMargin(a->x, a->y, a->z, a->heading, -2.0f)) {
            off++;
            static int told = 0;
            if (getenv("TR_WALLS") && told++ < 15) {
                int sg = a->route[a->routeIdx];
                printf("wall: car %d at (%.1f,%.1f) on %s %d (node %d) s %.1f/%.1f v %.1f steer %.0f\n", i, a->x / CELL_SIZE, a->y / CELL_SIZE,
                       rdSegs[sg].kind == RD_CONN ? "conn" : "lane", sg, rdSegs[sg].node, a->s / CELL_SIZE, rdSegs[sg].len / CELL_SIZE,
                       a->speed / CELL_SIZE, a->steer * 57.3f);
            }
        }
        for (int j = i + 1; j < carCount; j++) {
            const Car* b = &cars[j];
            if (!b->active || !b->onRoad) continue;
            float dx = a->x - b->x, dy = a->y - b->y;
            if (dx * dx + dy * dy > 9.0f * CELL_SIZE * CELL_SIZE) continue;
            if (CarBodiesOverlap(a->x, a->y, a->heading, b->x, b->y, b->heading, -2.0f)) {
                pairs++;
                static int told = 0;
                if (getenv("TR_TOUCH") && told++ < 10) {
                    int sa = a->route[a->routeIdx], sb = b->route[b->routeIdx];
                    printf("touch t %.1f: car %d on %s %d (node %d, from %d to %d) s %.1f v %.1f hold %d | car %d on %s %d (node %d, from %d to %d) s %.1f v %.1f hold %d\n", trTime,
                           i, rdSegs[sa].kind == RD_CONN ? "conn" : "lane", sa, rdSegs[sa].node, rdSegs[sa].from, rdSegs[sa].to, a->s / CELL_SIZE, a->speed / CELL_SIZE, a->holdNode,
                           j, rdSegs[sb].kind == RD_CONN ? "conn" : "lane", sb, rdSegs[sb].node, rdSegs[sb].from, rdSegs[sb].to, b->s / CELL_SIZE, b->speed / CELL_SIZE, b->holdNode);
                }
            }
        }
    }
    trafficSamples++;
    trafficOverlapSamples += pairs;
    if (pairs > trafficOverlapMax) trafficOverlapMax = pairs;
    if (off > trafficOffRoadMax) trafficOffRoadMax = off;
}

void TrafficTick(float dt) {
    if (rdSegCount == 0) return;
    trTime += dt;
    for (int s = 0; s < rdSegCount; s++) segHead[s] = -1;
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active || !c->onRoad) continue;
        int sg = c->route[c->routeIdx];
        carNext[i] = segHead[sg];
        segHead[sg] = i;
    }
    // Queues make a lane look longer to new trips (drivers avoid jams)
    for (int s = 0; s < rdSegCount; s++) {
        float slow = 0;
        for (int o = segHead[s]; o >= 0; o = carNext[o]) if (cars[o].speed < 1.0f * CELL_SIZE) slow += 1;
        rdSegExtra[s] = slow * TR_QUEUE_COST * CELL_SIZE;
    }
    ServeJunctions(dt);
    for (int i = 0; i < carCount; i++)
        if (cars[i].active && cars[i].onRoad) DriveCar(i, dt);
    static int tick = 0;
    if ((tick++ % 30) == 0) Sample();
}

static void DumpJunctions(void) {
    for (int nd = 0; nd < rdNodeCount; nd++) {
        if (trReqN[nd] == 0) continue;
        bool old = false;
        for (int k = 0; k < trReqN[nd]; k++) if (trReq[nd][k].waited > 15) old = true;
        if (!old) continue;
        printf("junction %d at (%.0f,%.0f): %d edges, %d connectors\n", nd, rdNodes[nd].x / CELL_SIZE, rdNodes[nd].y / CELL_SIZE, rdNodes[nd].nEdge, rdNodes[nd].nConn);
        for (int h = 0; h < trHoldN[nd]; h++) {
            const Car* c = &cars[trHold[nd][h].car];
            int sg = c->route[c->routeIdx];
            printf("  holds: car %d conn %d (ci %d -> lane %d), car on %s %d s %.1f v %.1f why %d gap %.1f\n", trHold[nd][h].car, trHold[nd][h].seg,
                   rdSegs[trHold[nd][h].seg].ci, rdSegs[trHold[nd][h].seg].to, rdSegs[sg].kind == RD_CONN ? "conn" : "lane", sg, c->s / CELL_SIZE,
                   c->speed / CELL_SIZE, c->why, c->gap / CELL_SIZE);
        }
        for (int k = 0; k < trReqN[nd]; k++) {
            const TrReq* r = &trReq[nd][k];
            const Car* c = &cars[r->car];
            printf("  asks: car %d conn %d (ci %d -> lane %d, room %.1f) waited %.1f, car s %.1f/%.1f v %.1f\n", r->car, r->seg, rdSegs[r->seg].ci,
                   rdSegs[r->seg].to, ExitRoom(rdSegs[r->seg].to) / CELL_SIZE, r->waited, c->s / CELL_SIZE,
                   rdSegs[c->route[c->routeIdx]].len / CELL_SIZE, c->speed / CELL_SIZE);
        }
    }
}

void TrafficPrintStats(void) {
    if (getenv("TR_JDUMP")) DumpJunctions();
    int n = 0, trips = 0, moving = 0, still20 = 0, held = 0, asked = 0;
    float drive = 0, speedSum = 0;
    for (int i = 0; i < carCount; i++) {
        const Car* c = &cars[i];
        if (!c->active || !c->onRoad) continue;
        n++; trips += c->legsDone; drive += c->driveTime; speedSum += c->speed;
        if (c->speed > 0.5f * CELL_SIZE) moving++;
        if (c->stillTime > 20) still20++;
        if (c->holdNode >= 0) held++;
        if (c->requested) asked++;
    }
    if (n == 0) return;
    printf("Roads: %d junctions, %d roads, %d lanes, %d segments\n", rdNodeCount, rdEdgeCount, RoadLaneCount(), rdSegCount);
    printf("Traffic: %d cars, %d trips (%.1f per car per game-minute), mean speed now %.1f cells/s, %d moving now\n",
           n, trips, drive > 0 ? trips / (drive / 60.0f) * 1.0f : 0, speedSum / n / CELL_SIZE, moving);
    printf("  junctions: %d cars hold a connector, %d waiting for one; longest wait for a grant %.1f s\n",
           held, asked, trafficJunctionWaitMax);
    printf("  still > 20 s now: %d; longest standstill %.1f s\n", still20, trafficWaitMax);
    printf("  silly: car pairs touching max %d per sample (avg %.3f over %d samples), cars touching walls max %d\n",
           trafficOverlapMax, trafficSamples ? (double)trafficOverlapSamples / trafficSamples : 0.0, trafficSamples, trafficOffRoadMax);
}
