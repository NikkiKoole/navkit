// traffic.c - Cars driving on the road network (see traffic.h, roads.h)
//
// Headless diagnostics (env): TR_TRACE=<car> prints that car twice a second; TR_WALLS /
// TR_TOUCH print the first wall / car-car touches; TR_LONG prints why a junction request
// waits 45 s; TR_JDUMP dumps junctions with old requests at the end of the run; TR_PULL
// prints what blocks a car waiting 45 s to pull out of a bay; TR_HOLDS lists junction
// holders at the end; TR_BAY=<bay segment> prints that bay's geometry; TR_STILL lists cars
// standing 60+ s at the end; TR_RING prints roundabout occupancy at the end; TR_NET checks
// every lane is reachable; TR_FAILS prints the first failing route searches.
#include "traffic.h"
#include "roads.h"
#include "cars.h"
#include "mover.h"            // CELL_SIZE
#include "../../shared/profiler.h"
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
float trafficPullOutWaitMax = 0;
int trafficLightCount = 0;
int trafficLightEntries = 0, trafficRedRuns = 0;

// Traffic lights: per junction an offset into the cycle; per arriving lane its group
// (which pair of opposite approaches it belongs to)
#define TL_GREEN 12.0f
#define TL_AMBER 3.0f
#define TL_ALLRED 1.0f
#define TL_HALF (TL_GREEN + TL_AMBER + TL_ALLRED)
static bool tlOn[RD_MAX_NODES];
static float tlOffset[RD_MAX_NODES];
static signed char tlGroup[RD_MAX_SEGS];   // lane arriving at a lit junction: 0 / 1

#define TR_MAX_HOLD 24
#define TR_MAX_REQ 48
#define TR_TIME_GAP 1.0f       // s: IDM time gap
#define TR_MIN_GAP 1.0f        // cells: IDM standstill gap (bumper to bumper)
#define TR_STOP_BACK 0.4f      // cells: stop this far before a junction's start
#define TR_ASK_AHEAD 6.0f      // cells beyond braking distance: ask the junction
#define TR_STANLEY_K 2.5f      // 1/s: sideways error correction
#define TR_GIVE_WAY_S 3.0f     // s: give way to a car with right of way this close (in time)
#define TR_GIVE_WAY_CELLS 3.0f // cells: ... or this close
#define TR_PULL_OUT_ROOM 1.5f      // cells (+ 0.5 s at our speed) to the car ahead before changing lanes
#define TR_LANE_CHANGE_CELLS 6.0f   // cells of road a lane change takes
#define TR_LET_IN 3.0f         // s: a car waiting this long to pull out gets let in
#define TR_PULL_BEHIND 8.0f    // cells: the lane must be clear this far behind a bay's exit (+ 2 s at the car's speed)
#define TR_QUEUE_COST 8.0f     // cells of route cost per slow car on a lane
#define RD_CONFLICT_CELLS 2.6f  // cells: a car on a sibling curve this close to ours is in our way

typedef struct { short car; int seg; } TrHold;
typedef struct { short car; int seg; float t, waited, noRoom; } TrReq;
static TrHold trHold[RD_MAX_NODES][TR_MAX_HOLD];
static int trHoldN[RD_MAX_NODES];
static TrReq trReq[RD_MAX_NODES][TR_MAX_REQ];
static int trReqN[RD_MAX_NODES];
static int segHead[RD_MAX_SEGS], carNext[MAX_CARS];
static int ghostHead[RD_MAX_SEGS], ghostNext[MAX_CARS];   // cars changing out of a lane, still partly in it
// The cars on each segment sorted by s (built each tick): the car ahead is the next one,
// not a scan of the whole segment (that was k^2 per lane in dense traffic)
static int segFirst[RD_MAX_SEGS], segCnt[RD_MAX_SEGS], sortedCars[MAX_CARS], carSlot[MAX_CARS];
// The cars on each segment sorted by s (built each tick): the car ahead is the next one,
// not a scan of the whole segment (that was k^2 per lane in dense traffic)
static int segFirst[RD_MAX_SEGS], segCnt[RD_MAX_SEGS], sortedCars[MAX_CARS], carSlot[MAX_CARS];
// The cars on each segment sorted by s (built each tick): the car ahead is the next one,
// not a scan of the whole segment (that was k^2 per lane in dense traffic)
static int segFirst[RD_MAX_SEGS], segCnt[RD_MAX_SEGS], sortedCars[MAX_CARS], carSlot[MAX_CARS];
static float trTime = 0;
// Route searches (the expensive call): per caller, total and the most in one tick
static long trRoutes[4];
static int trRoutesTick, trRoutesTickMax;
static long trRouteFails[4];
static int TrRoute(int caller, int from, int to, RdSegId* out, int max) {
    trRoutes[caller]++;
    trRoutesTick++;
    int n = RoadRoute(from, to, out, max);
    if (n <= 0) {
        trRouteFails[caller]++;
        static int told = 0;
        if (getenv("TR_FAILS") && told++ < 8)
            printf("route fail (caller %d): from seg %d kind %d nOut %d to lane %d kind %d\n", caller, from, rdSegs[from].kind, rdSegs[from].nOut, to, rdSegs[to].kind);
    }
    return n;
}
static short bayCar[RD_MAX_BAYS];   // the car parked in / heading for a bay, -1 free
static unsigned int trRng = 1;

static unsigned int TrRand(void) { trRng = trRng * 1103515245u + 12345u; return (trRng >> 16) & 0x7fff; }
static float TrRandF(void) { return TrRand() / 32767.0f; }

void TrafficReset(void) {
    memset(trHoldN, 0, sizeof(trHoldN));
    memset(trReqN, 0, sizeof(trReqN));
    trTime = 0;
    trRng = 1;
    memset(trRoutes, 0, sizeof(trRoutes));
    trRoutesTick = trRoutesTickMax = 0;
    trafficOverlapMax = trafficOffRoadMax = trafficSamples = 0;
    trafficOverlapSamples = 0;
    trafficWaitMax = trafficJunctionWaitMax = trafficPullOutWaitMax = 0;
    for (int b = 0; b < RD_MAX_BAYS; b++) bayCar[b] = -1;
    memset(tlOn, 0, sizeof(tlOn));
    trafficLightCount = 0;
    trafficLightEntries = trafficRedRuns = 0;
}

static float TrRandF(void);

void TrafficSetupLights(bool on) {
    memset(tlOn, 0, sizeof(tlOn));
    trafficLightCount = 0;
    if (!on) return;
    for (int nd = 0; nd < rdNodeCount; nd++) {
        const RdNode* N = &rdNodes[nd];
        if (N->nEdge != 4) continue;
        // roads by their direction from here; opposite ones (0,2) and (1,3) after sorting
        int idx[4]; float ang[4];
        for (int k = 0; k < 4; k++) {
            const RdEdge* e = &rdEdges[N->edge[k]];
            const RdNode* o = &rdNodes[e->a == nd ? e->b : e->a];
            idx[k] = k; ang[k] = atan2f(o->y - N->y, o->x - N->x);
        }
        for (int a = 0; a < 4; a++)
            for (int b = a + 1; b < 4; b++)
                if (ang[idx[b]] < ang[idx[a]]) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
        for (int k = 0; k < 4; k++) {
            const RdEdge* e = &rdEdges[N->edge[idx[k]]];
            int d = e->a == nd ? 1 : 0;
            if (e->lane[d] >= 0) tlGroup[e->lane[d]] = (signed char)(k % 2);
            if (e->lane2[d] >= 0) tlGroup[e->lane2[d]] = (signed char)(k % 2);
        }
        tlOn[nd] = true;
        tlOffset[nd] = TrRandF() * 2 * TL_HALF;
        trafficLightCount++;
    }
}

// 2 green, 1 amber, 0 red; *left = seconds of green + amber left (green only)
static int LightState(int nd, int group, float* left) {
    float t = fmodf(trTime + tlOffset[nd], 2 * TL_HALF);
    int phase = t < TL_HALF ? 0 : 1;
    float u = t - phase * TL_HALF;
    if (left) *left = 0;
    if (phase != group) return 0;
    if (u < TL_GREEN) { if (left) *left = TL_GREEN + TL_AMBER - u; return 2; }
    if (u < TL_GREEN + TL_AMBER) return 1;
    return 0;
}

int TrafficLightAt(int node, int lane) {
    if (node < 0 || node >= rdNodeCount || !tlOn[node]) return -1;
    return LightState(node, tlGroup[lane], NULL);
}

// ---------------------------------------------------------------------------
// Route steps. Bays join a lane part-way: the step before a bay's IN segment ends at
// its s0 on the lane (not the lane's end); the step after a bay's OUT segment starts
// at its s1. A car's s is the arc length on its current segment.
// ---------------------------------------------------------------------------
static float StepEnd(const Car* c, int k) {
    if (k + 1 < c->routeLen && rdSegs[c->route[k + 1]].kind == RD_BAYIN) return rdSegs[c->route[k + 1]].bayS;
    return rdSegs[c->route[k]].len;
}
static float StepStart(const Car* c, int k) {
    if (k > 0 && rdSegs[c->route[k - 1]].kind == RD_BAYOUT) return rdSegs[c->route[k - 1]].bayS;
    return 0;
}

// ---------------------------------------------------------------------------
// Trips: to a free parking bay (or, with none free, a random spot on a lane), from
// the bay we're parked in or the lane we're on
// ---------------------------------------------------------------------------
static bool TripToBay(int i) {
    Car* c = &cars[i];
    if (rdBayCount < 2) return false;
    if (!carCityParking && rdSegs[c->route[c->routeIdx]].kind == RD_LANE) return false;   // just driving
    bool parked = c->routeLen > 0 && rdSegs[c->route[c->routeIdx]].kind == RD_BAYIN;
    int from = parked ? c->bay : -1;
    int lane = parked ? rdBays[from].lane : c->route[c->routeIdx];
    float s = parked ? rdBays[from].s1 : c->s;
    for (int tries = 0; tries < 300; tries++) {   // (a taken bay costs nothing to skip; searching is the cost)
        int d = (int)(TrRand() % (unsigned)rdBayCount);
        if (bayCar[d] >= 0 || d == from) continue;
        RdSegId r[CAR_MAX_ROUTE];
        int n = 0;
        if (parked) r[n++] = (RdSegId)rdBays[from].out;
        if (rdBays[d].lane == lane && rdBays[d].s0 > s + 4 * CELL_SIZE) r[n++] = (RdSegId)lane;
        else {
            int m = TrRoute(0, lane, rdBays[d].lane, r + n, CAR_MAX_ROUTE - n - 1);
            if (m <= 0) continue;
            n += m;
        }
        r[n++] = (RdSegId)rdBays[d].in;
        memcpy(c->route, r, sizeof(RdSegId) * n);
        c->routeLen = n;
        c->routeIdx = 0;
        if (parked) { c->s = 0; c->bayFrom = from; }
        c->bay = d;
        bayCar[d] = (short)i;
        c->destS = rdSegs[rdBays[d].in].len;
        c->legTime = 0;
        return true;
    }
    return false;
}

static bool NewTrip(int i) {
    if (TripToBay(i)) return true;
    Car* c = &cars[i];
    int cur = c->route[c->routeIdx];
    if (rdSegs[cur].kind != RD_LANE) return false;   // parked, every bay taken: stay a while longer
    for (int tries = 0; tries < 20; tries++) {
        int dl = RoadLaneAt((int)(TrRand() % (unsigned)RoadLaneCount()));
        float len = rdSegs[dl].len;
        if (len < 8 * CELL_SIZE) continue;
        float ds = 3 * CELL_SIZE + (len - 6 * CELL_SIZE) * (TrRand() / 32767.0f);
        int n;
        if (dl == cur && ds > c->s + 4 * CELL_SIZE) { c->route[0] = (RdSegId)cur; n = 1; }
        else n = TrRoute(0, cur, dl, c->route, CAR_MAX_ROUTE);
        if (n <= 0) continue;
        c->routeLen = n;
        c->routeIdx = 0;
        c->destS = ds;
        c->legTime = 0;
        return true;
    }
    c->routeLen = 1; c->route[0] = (RdSegId)cur; c->routeIdx = 0; c->destS = rdSegs[cur].len;
    return false;
}

int TrafficSpawn(int lane, float s, int z) {
    int i = SpawnCar(0, 0, z, 0);
    if (i < 0) return -1;
    Car* c = &cars[i];
    RoadPointAt(lane, s, &c->x, &c->y, &c->heading);
    c->onRoad = true;
    c->state = DRIVE_ROAD;
    c->route[0] = (RdSegId)lane;
    c->routeLen = 1;
    c->routeIdx = 0;
    c->s = s;
    c->holdNode = -1;
    c->bay = c->bayFrom = -1;
    c->ghostLane = -1;
    NewTrip(i);
    return i;
}

int TrafficSpawnParked(int bay, float parkFor, int z) {
    if (bay < 0 || bay >= rdBayCount || bayCar[bay] >= 0) return -1;
    int i = SpawnCar(0, 0, z, 0);
    if (i < 0) return -1;
    Car* c = &cars[i];
    int in = rdBays[bay].in;
    RoadPointAt(in, rdSegs[in].len, &c->x, &c->y, &c->heading);
    c->onRoad = true;
    c->state = DRIVE_ROAD;
    c->route[0] = (RdSegId)in;
    c->routeLen = 1;
    c->routeIdx = 0;
    c->s = rdSegs[in].len;
    c->holdNode = -1;
    c->bay = bay;
    c->bayFrom = -1;
    c->ghostLane = -1;
    c->parkTime = parkFor > 0 ? parkFor : 0.01f;
    bayCar[bay] = (short)i;
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
    trReq[nd][trReqN[nd]++] = (TrReq){(short)i, connSeg, trTime, 0, 0};
    c->requested = true;
}

static void CancelAsk(int i) {
    Car* c = &cars[i];
    for (int nd = 0; nd < rdNodeCount && c->requested; nd++)
        for (int k = 0; k < trReqN[nd]; k++)
            if (trReq[nd][k].car == i) {
                for (int m = k; m + 1 < trReqN[nd]; m++) trReq[nd][m] = trReq[nd][m + 1];   // keep the order
                trReqN[nd]--;
                c->requested = false;
                break;
            }
    c->requested = false;
    c->noRoom = 0;
}

// Our exit is full: plan again with it counted as very long (on a roundabout: go round
// once more). Keeps the route if there's no other way.
static void RerouteAvoiding(int i, int avoidLane) {
    Car* c = &cars[i];
    int cur = c->route[c->routeIdx];
    if (rdSegs[cur].kind != RD_LANE) return;
    int destLane = c->bay >= 0 ? rdBays[c->bay].lane : c->route[c->routeLen - 1];
    RdSegId r[CAR_MAX_ROUTE];
    int n = 0;
    if (rdSegs[cur].priority) {
        // On a roundabout: one more lap - on to the next ring stretch, then plan from there
        int lap = -1;
        for (int k = 0; k < rdSegs[cur].nOut; k++)
            if (rdSegs[rdSegs[rdSegs[cur].out[k]].to].priority) lap = rdSegs[cur].out[k];
        if (lap < 0) return;
        int next = rdSegs[lap].to;
        r[0] = (RdSegId)cur; r[1] = (RdSegId)lap;
        int m = next == destLane ? 1 : TrRoute(1, next, destLane, r + 2, CAR_MAX_ROUTE - 3);
        if (m <= 0) return;
        if (next == destLane) r[2] = (RdSegId)next;
        n = 2 + m;
    } else {
        if (destLane == avoidLane) return;
        float saved = rdSegExtra[avoidLane];
        rdSegExtra[avoidLane] += 1e7f;
        n = TrRoute(1, cur, destLane, r, CAR_MAX_ROUTE - 1);
        rdSegExtra[avoidLane] = saved;
        if (n < 2 || rdSegs[r[1]].to == avoidLane) return;
    }
    if (c->bay >= 0) r[n++] = (RdSegId)rdBays[c->bay].in;
    CancelAsk(i);
    memcpy(c->route, r, sizeof(RdSegId) * n);
    c->routeLen = n;
    c->routeIdx = 0;
    c->reroutes++;
}

static void Release(int i) {
    Car* c = &cars[i];
    if (c->holdNode < 0) return;
    int nd = rdSegs[c->holdNode].node;
    for (int k = 0; k < trHoldN[nd]; k++)
        if (trHold[nd][k].car == i) { trHold[nd][k] = trHold[nd][--trHoldN[nd]]; break; }
    c->holdNode = -1;
}

// Roundabout r (its centre node): cars on it (ring lanes, connectors at its junctions,
// cars granted in), and how many it may hold: one car per ring stretch. Then every car
// rolls up to its stop line and leaves room behind it, so the ring can't lock up
// (with "capacity minus one per stretch" two cars per stretch still could, and did)
static bool OnRing(const Car* c, int r) {
    int sg = c->route[c->routeIdx];
    const RdSeg* S = &rdSegs[sg];
    if (S->kind == RD_LANE && S->priority && rdNodes[S->to].ringOf == r) return true;
    if (S->kind == RD_CONN && rdNodes[S->node].ringOf == r && rdSegs[S->to].priority) return true;
    if (c->holdNode >= 0) {
        const RdSeg* H = &rdSegs[c->holdNode];
        if (rdNodes[H->node].ringOf == r && rdSegs[H->to].priority) return true;
    }
    return false;
}

static long dbgEnterAsk = 0, dbgRingFull = 0;
static short ringLanes[RD_MAX_NODES], ringCars[RD_MAX_NODES];   // per roundabout centre, counted each tick

static void CountRings(void) {
    static int countedFor = -1;   // the ring lanes don't change: count them once per network
    if (countedFor != rdSegCount) {
        memset(ringLanes, 0, sizeof(RdSegId) * (size_t)rdNodeCount);
        for (int i = 0; i < RoadLaneCount(); i++) {
            const RdSeg* S = &rdSegs[RoadLaneAt(i)];
            if (S->priority && rdNodes[S->to].ringOf >= 0) ringLanes[rdNodes[S->to].ringOf]++;
        }
        countedFor = rdSegCount;
    }
    memset(ringCars, 0, sizeof(RdSegId) * (size_t)rdNodeCount);
    for (int i = 0; i < carCount; i++) {
        const Car* c = &cars[i];
        if (!c->active || !c->onRoad) continue;
        int sg = c->route[c->routeIdx], r = -1;
        const RdSeg* S = &rdSegs[sg];
        if (S->kind == RD_LANE && S->priority) r = rdNodes[S->to].ringOf;
        else if (S->kind == RD_CONN && rdSegs[S->to].priority) r = rdNodes[S->node].ringOf;
        else if (c->holdNode >= 0 && rdSegs[rdSegs[c->holdNode].to].priority) r = rdNodes[rdSegs[c->holdNode].node].ringOf;
        if (r >= 0) ringCars[r]++;
    }
}

static bool RingFull(int r) {
    int cap = ringLanes[r] > 1 ? ringLanes[r] : 1;
    return ringCars[r] >= cap;
}

// A car with right of way arriving at junction nd within TR_GIVE_WAY_S whose next
// connector conflicts with connector index ci?
static bool PriorityComing(int nd, int ci) {
    const RdNode* N = &rdNodes[nd];
    for (int k = 0; k < N->nEdge; k++) {
        const RdEdge* e = &rdEdges[N->edge[k]];
        int in = e->b == nd ? e->lane[0] : e->lane[1];
        if (in < 0 || !rdSegs[in].priority) continue;
        for (int o = segHead[in]; o >= 0; o = carNext[o]) {
            const Car* pc = &cars[o];
            if (pc->routeIdx + 1 >= pc->routeLen) continue;
            int next = pc->route[pc->routeIdx + 1];
            if (rdSegs[next].kind != RD_CONN || rdSegs[next].node != nd) continue;
            if (!N->conflict[ci][rdSegs[next].ci]) continue;
            float dist = rdSegs[in].len - pc->s;
            if (dist < TR_GIVE_WAY_CELLS * CELL_SIZE + pc->speed * TR_GIVE_WAY_S) return true;
        }
    }
    // also ring cars already on a connector into this junction's ring lane? (they hold it: claimed)
    return false;
}

// Grant in arrival order: a request is granted if it conflicts with no connector held
// or asked for earlier, and its exit lane has room for the car (plus the cars already
// granted into that lane)
static void ServeJunctions(float dt) {
    // room for the car, its standstill gap, and enough to get its rear out of the junction
    // (it releases at L/2 + 0.3 cells into the lane): else it can stop half in and hold it
    float need = (carLength + TR_MIN_GAP + 0.6f) * CELL_SIZE;
    for (int nd = 0; nd < rdNodeCount; nd++) {
        if (trReqN[nd] == 0) continue;
        const RdNode* N = &rdNodes[nd];
        unsigned char claimed[RD_MAX_CONN] = {0};
        for (int k = 0; k < trHoldN[nd]; k++) claimed[rdSegs[trHold[nd][k].seg].ci] = 1;
        // Pass 0: requests from lanes with right of way (a roundabout's ring); pass 1: the rest
        for (int pass = 0; pass < 2; pass++) {
        int k = 0;
        while (k < trReqN[nd]) {
            TrReq* r = &trReq[nd][k];
            int ci = rdSegs[r->seg].ci;
            bool prio = rdSegs[rdSegs[r->seg].from].priority;
            if (prio != (pass == 0)) { k++; continue; }
            // Giving way: a car with right of way that will be here within ~3 s and whose
            // path crosses ours goes first (it may not have asked yet)
            if (!prio && PriorityComing(nd, ci)) { r->waited += dt; k++; continue; }
            // Entering a roundabout: only while it has room to keep circulating
            if (!prio && rdSegs[rdSegs[r->seg].to].priority) dbgEnterAsk++;
            if (!prio && rdSegs[rdSegs[r->seg].to].priority && N->ringOf >= 0 && RingFull(N->ringOf)) { dbgRingFull++; r->waited += dt; k++; continue; }
            if (tlOn[nd]) {
                float left;
                int inLane = rdSegs[r->seg].from;
                bool go = LightState(nd, tlGroup[inLane], &left) == 2;
                if (go) {   // only if it reaches the line before the green + amber run out
                    const Car* rc = &cars[r->car];
                    float dist = rc->route[rc->routeIdx] == inLane ? StepEnd(rc, rc->routeIdx) - rc->s : 1e9f;
                    float vv = fmaxf(rc->speed, 2.0f * CELL_SIZE);
                    if (dist / vv > left) go = false;
                }
                if (!go) { r->waited += dt; k++; continue; }   // red: waits, claims nothing
            }
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
                    if (N->ringOf >= 0 && !prio && rdSegs[rdSegs[r->seg].to].priority) ringCars[N->ringOf]++;
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
            else { r->noRoom += dt; cars[r->car].noRoom = r->noRoom; }
            r->waited += dt;
            if (r->waited >= 45 && r->waited - dt < 45 && getenv("TR_LONG")) {
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
}

// ---------------------------------------------------------------------------
// One car
// ---------------------------------------------------------------------------
// The point and heading `ahead` px further along the route (clamped at its ends)
static void RouteAheadH(const Car* c, float ahead, float* x, float* y, float* h) {
    int k = c->routeIdx;
    float s = c->s + ahead;
    while (k < c->routeLen - 1 && s > StepEnd(c, k)) { s = s - StepEnd(c, k) + StepStart(c, k + 1); k++; }
    while (k > 0 && s < StepStart(c, k)) { s = s - StepStart(c, k) + StepEnd(c, k - 1); k--; }
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

// Nearest point on a segment, searching the whole segment
static float ProjectOnWide(int seg, float s0, float x, float y) {
    const RdSeg* sg = &rdSegs[seg];
    float best = 1e30f, bs = s0;
    for (int i = 0; i + 1 < sg->n; i++) {
        int k = sg->p0 + i;
        float ex = rdX[k + 1] - rdX[k], ey = rdY[k + 1] - rdY[k], L2 = ex * ex + ey * ey;
        float t = L2 > 0 ? ((x - rdX[k]) * ex + (y - rdY[k]) * ey) / L2 : 0;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        float px = rdX[k] + ex * t - x, py = rdY[k] + ey * t - y, d = px * px + py * py;
        if (d < best) { best = d; bs = rdS[k] + (rdS[k + 1] - rdS[k]) * t; }
    }
    return bs;
}

// Nearest point to (x, y) on a segment, searching around arc length s0
static float ProjectOn(int seg, float s0, float x, float y) {
    const RdSeg* sg = &rdSegs[seg];
    int c0 = (int)(s0 / 8.0f);
    int a = c0 - 2 < 0 ? 0 : c0 - 2, b = c0 + 3 > sg->n - 2 ? sg->n - 2 : c0 + 3;
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

// Can a car pull out of bay b: the lane clear from TR_PULL_BEHIND cells (more for fast
// cars) behind the join point to 3 cells past it?
static bool PullOutClear(int b) {
    const RdBay* B = &rdBays[b];
    float CS = CELL_SIZE;
    float spot = B->s1 - rdSegs[B->out].len;   // the spot as a lane position (about)
    for (int o = segHead[B->lane]; o >= 0; o = carNext[o]) {
        float so = cars[o].s, back = (TR_PULL_BEHIND + carLength) * CS + cars[o].speed * 2.0f;
        if (so < B->s1 - back || so > B->s1 + 3 * CS) continue;
        if (so > spot - carLength * CS) return false;         // beside us or ahead
        if (cars[o].speed > 0.3f * CS) return false;          // coming: wait
        // behind and stopped (a queue): it will let us in (it sees us as soon as we move)
    }
    // cars still on the bay's own in-curve start or the lane's other bays near the join
    for (int q = 0; q < rdSegs[B->lane].nBays; q++) {
        const RdBay* O = &rdBays[rdSegs[B->lane].bays[q]];
        if (O == B) continue;
        for (int o = segHead[O->in]; o >= 0; o = carNext[o]) if (fabsf(O->s0 + cars[o].s - B->s1) < 4 * CS) return false;
        for (int o = segHead[O->out]; o >= 0; o = carNext[o]) if (cars[o].s > 0.5f * CS && fabsf(O->s1 - B->s1) < 8 * CS) return false;
    }
    return true;
}

// Is lane P clear for us at s (cars and cars changing into / out of it, cars at its
// bays), with room behind for its cars at their speed?
static bool LaneClearAt(int i, int P, float s) {
    float CS = CELL_SIZE, L = carLength * CS, v = cars[i].speed;
    for (int pass = 0; pass < 2; pass++)
        for (int o = pass == 0 ? segHead[P] : ghostHead[P]; o >= 0; o = pass == 0 ? carNext[o] : ghostNext[o]) {
            if (o == i) continue;
            float so = cars[o].s;
            if (so > s - (L + 2 * CS + cars[o].speed * 1.5f) && so < s + L + 3 * CS + v * 1.0f) return false;
        }
    for (int q = 0; q < rdSegs[P].nBays; q++) {
        const RdBay* B = &rdBays[rdSegs[P].bays[q]];
        if (B->s0 > s + 10 * CS || B->s1 < s - 6 * CS) continue;
        if (segHead[B->in] >= 0 || segHead[B->out] >= 0) return false;
    }
    return true;
}

// Overtake a slow car on a two-lane road, or move back to the right-hand lane once it's
// clear. Not near the lane's end, not on the road we park on, not once we've asked
// or hold the next junction.
static void MaybeChangeLane(int i, int lead, float gap, float vWant) {
    Car* c = &cars[i];
    float CS = CELL_SIZE, L = carLength * CS;
    int cur = c->route[c->routeIdx];
    const RdSeg* S = &rdSegs[cur];
    if (S->kind != RD_LANE || S->parallel < 0 || c->latOff != 0 || c->parkTime > 0) return;
    if (c->requested || c->holdNode >= 0) return;
    if (c->s < 2 * CS || StepEnd(c, c->routeIdx) - c->s < TR_LANE_CHANGE_CELLS * CS + 3 * CS) return;
    int destLane = c->bay >= 0 ? rdBays[c->bay].lane : c->route[c->routeLen - 1];
    if (rdSegs[destLane].edge == S->edge) return;
    int P = S->parallel;
    bool out = false;
    if (S->laneIdx == 0) {
        // a slow car ahead (not just queuing: it has road in front of it, or it's pulling in)
        if (lead < 0 || cars[lead].route[cars[lead].routeIdx] != cur) return;
        const Car* l = &cars[lead];
        bool parking = l->routeIdx + 1 < l->routeLen && rdSegs[l->route[l->routeIdx + 1]].kind == RD_BAYIN;
        if (gap > 6 * CS + c->speed * 2.0f) return;
        if (!parking && (l->speed > vWant - 1.5f * CS || l->gap < 8 * CS)) return;
        out = true;
    } else {
        // back to the right-hand lane when there's no slow car there just ahead
        for (int o = segHead[P]; o >= 0; o = carNext[o])
            if (cars[o].s > c->s && cars[o].s < c->s + 10 * CS && cars[o].speed < c->speed - 1.0f * CS) return;
    }
    // room to pull out: the car ahead in our lane far enough that our nose clears it
    if (gap < TR_PULL_OUT_ROOM * CS + c->speed * 0.5f) return;
    float sP = ProjectOnWide(P, c->s, c->x, c->y);
    if (!LaneClearAt(i, P, sP)) return;
    RdSegId r[CAR_MAX_ROUTE];
    int n = TrRoute(2, P, destLane, r, CAR_MAX_ROUTE - 1);
    if (n <= 0) return;
    if (c->bay >= 0) r[n++] = (RdSegId)rdBays[c->bay].in;
    float px, py, ph;
    RoadPointAt(P, sP, &px, &py, &ph);
    c->latOff = (c->x - px) * -sinf(ph) + (c->y - py) * cosf(ph);
    c->ghostLane = cur;
    memcpy(c->route, r, sizeof(RdSegId) * n);
    c->routeLen = n;
    c->routeIdx = 0;
    c->s = sP;
    c->lcCool = 4.0f;
    if (out) c->overtakes++; else c->keepRights++;
    (void)L;
}

static void DriveCar(int i, float dt) {
    Car* c = &cars[i];
    float CS = CELL_SIZE, L = carLength * CS;
    // Parked: wait, then plan the next trip
    if (c->parkTime > 0) {
        c->speed = 0;
        c->why = WHY_FREE;
        c->parkTime -= dt;
        if (c->parkTime > 0) return;
        c->parkTime = 0;
        if (!NewTrip(i)) { c->parkTime = 5; return; }
        c->pullWait = 0;
    }
    // Waiting at a junction for room past it: on a roundabout's ring go round again after
    // 1 s (a car stopped on the ring blocks it), elsewhere look for another way after 15 s
    // (an attempt that finds no other way waits 5 s before the next: route searches are the
    // expensive part, and a stuck car used to search every tick)
    if (c->rerouteCool > 0) c->rerouteCool -= dt;
    else if (c->requested && c->noRoom > (rdSegs[c->route[c->routeIdx]].priority ? 1.0f : 15.0f) &&
             c->routeIdx + 1 < c->routeLen && rdSegs[c->route[c->routeIdx + 1]].kind == RD_CONN) {
        RerouteAvoiding(i, rdSegs[c->route[c->routeIdx + 1]].to);
        c->rerouteCool = 5.0f;
    }
    // About to pull out: wait for a gap in the lane
    if (c->routeIdx == 0 && rdSegs[c->route[0]].kind == RD_BAYOUT && c->s < 0.5f * CS && c->speed < 0.1f * CS) {
        int b = rdSegs[c->route[0]].bay;
        if (!PullOutClear(b)) {
            c->speed = 0;
            c->why = WHY_JUNCTION;
            c->pullWait += dt;
            if (c->pullWait >= 45 && c->pullWait - dt < 45 && getenv("TR_PULL")) {
                const RdBay* B = &rdBays[b];
                printf("pull-out wait 45 s: car %d, bay %d on lane %d (len %.1f) s0 %.1f s1 %.1f spot %.1f\n", i, b, B->lane, rdSegs[B->lane].len / CS,
                       B->s0 / CS, B->s1 / CS, (B->s1 - rdSegs[B->out].len) / CS);
                for (int o = segHead[B->lane]; o >= 0; o = carNext[o])
                    printf("    lane car %d s %.1f v %.1f why %d still %.0f\n", o, cars[o].s / CS, cars[o].speed / CS, cars[o].why, cars[o].stillTime);
                for (int q = 0; q < rdSegs[B->lane].nBays; q++) {
                    const RdBay* O = &rdBays[rdSegs[B->lane].bays[q]];
                    for (int o = segHead[O->in]; o >= 0; o = carNext[o]) printf("    bay-in car %d (bay %d) s %.1f v %.1f\n", o, rdSegs[O->in].bay, cars[o].s / CS, cars[o].speed / CS);
                    for (int o = segHead[O->out]; o >= 0; o = carNext[o]) printf("    bay-out car %d (bay %d) s %.1f v %.1f\n", o, rdSegs[O->out].bay, cars[o].s / CS, cars[o].speed / CS);
                }
            }
            if (c->pullWait > trafficPullOutWaitMax) trafficPullOutWaitMax = c->pullWait;
            return;
        }
    }
    float v = c->speed > 0 ? c->speed : 0;
    float bc = carBrake * CS;
    float brakeD = v * v / (2 * bc);

    // Look along the route: the car ahead, a junction not granted yet, bends
    float gap = 1e9f, vLead = 0;
    int why = WHY_FREE, lead = -1;
    float vAllow = fminf(carMaxSpeed, rdSegs[c->route[c->routeIdx]].vmax) * CS;
    float lookD = brakeD + 3 * L + TR_ASK_AHEAD * CS;
    float acc = StepStart(c, c->routeIdx) - c->s;   // from the car centre to step k's start point
    for (int k = c->routeIdx; k < c->routeLen && acc < lookD; k++) {
        int sg = c->route[k];
        float st = StepStart(c, k), en = StepEnd(c, k);
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
        {   // cars on the segment: the first one ahead of us (sorted by s)
            int a0 = segFirst[sg], n0 = segCnt[sg];
            int j = k == c->routeIdx ? carSlot[i] - a0 + 1 : 0;
            for (; j < n0; j++) {
                int o = sortedCars[a0 + j];
                float so = cars[o].s;
                if (o == i || so < st || (k == c->routeIdx && so <= c->s)) continue;
                if (so <= en + L) {
                    float d = acc + (so - st) - L;   // bumper to bumper
                    if (d < gap) { gap = d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; lead = o; }
                }
                break;
            }
        }
        for (int o = ghostHead[sg]; o >= 0; o = ghostNext[o]) {   // cars changing out of it
            if (o == i) continue;
            float so = cars[o].s;
            if (so < st || so > en + L) continue;
            if (k == c->routeIdx && so <= c->s) continue;
            float d = acc + (so - st) - L;
            if (d < gap) { gap = d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; lead = o; }
        }
        // Changing lanes: until we're out of the old lane, cars ahead in it are in our way too
        if (k == c->routeIdx && c->ghostLane >= 0)
            for (int o = segHead[c->ghostLane]; o >= 0; o = carNext[o]) {
                if (o == i || cars[o].s <= c->s || cars[o].s > c->s + 4 * L) continue;
                float d = cars[o].s - c->s - L;
                if (d < gap) { gap = d < 0 ? 0 : d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; lead = o; }
            }
        // A lane: cars pulling into / out of its bays are still in it for a few cells; a
        // car that has waited TR_LET_IN s to pull out gets let in (we stop short of its exit)
        if (rdSegs[sg].kind == RD_LANE)
            for (int q = 0; q < rdSegs[sg].nBays; q++) {
                const RdBay* B = &rdBays[rdSegs[sg].bays[q]];
                for (int o = segHead[B->out]; o >= 0; o = carNext[o]) {
                    if (o == i || cars[o].pullWait < TR_LET_IN || cars[o].s > 0.3f * CS) continue;
                    float stopAt = B->s1 - rdSegs[B->out].len - L;   // a car length short of its spot
                    float me = k == c->routeIdx ? c->s : st;
                    if (stopAt < st || stopAt > en || me > stopAt - 0.5f * L) continue;   // already past: go on
                    float d = acc + (stopAt - st) - L * 0.5f;
                    if (d < gap) { gap = d; vLead = 0; why = WHY_JUNCTION; found = true; }
                }
                for (int side = 0; side < 2; side++) {
                    int bs = side == 0 ? B->in : B->out;
                    for (int o = segHead[bs]; o >= 0; o = carNext[o]) {
                        if (o == i) continue;
                        float so = cars[o].s, lp;
                        // pulling in: in the lane for most of the curve; pulling out: from the first move
                        if (side == 0) { if (so > 0.8f * rdSegs[bs].len) continue; lp = B->s0 + so; }
                        else { if (so < 0.3f * CS) continue; lp = B->s1 - (rdSegs[bs].len - so); }
                        if (lp < st || lp > en + L) continue;
                        if (k == c->routeIdx && lp <= c->s) continue;
                        float d = acc + (lp - st) - L;
                        if (d < gap) { gap = d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; }
                    }
                }
            }
        // Pulling out: lane cars ahead of us (as a lane position) are in our way; cars
        // behind give way to us (they see us from our first move)
        if (rdSegs[sg].kind == RD_BAYOUT) {
            const RdBay* B = &rdBays[rdSegs[sg].bay];
            float startLp = B->s1 - rdSegs[sg].len;                        // this step's start as a lane position
            float meLp = k == c->routeIdx ? B->s1 - (rdSegs[sg].len - c->s) : startLp;
            for (int o = segHead[B->lane]; o >= 0; o = carNext[o]) {
                float so = cars[o].s;
                if (so <= meLp || so > B->s1 + L) continue;
                float d = acc + (so - startLp) - L;
                if (d < gap) { gap = d < 0 ? 0 : d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; }
            }
        }
        // Pulling in: until we're mostly in the bay, lane cars ahead of us are in our way
        if (rdSegs[sg].kind == RD_BAYIN) {
            const RdBay* B = &rdBays[rdSegs[sg].bay];
            float meLp = k == c->routeIdx ? B->s0 + c->s : B->s0;
            if (k != c->routeIdx || c->s < 0.8f * rdSegs[sg].len)
                for (int o = segHead[B->lane]; o >= 0; o = carNext[o]) {
                    float so = cars[o].s;
                    if (so <= meLp || so > B->s0 + 0.8f * rdSegs[sg].len + L) continue;
                    float d = acc + (so - B->s0) - L;
                    if (d < gap) { gap = d < 0 ? 0 : d; vLead = cars[o].speed > 0 ? cars[o].speed : 0; why = WHY_FOLLOW; found = true; }
                }
        }
        // The end of the route is a parking spot: stop there (car centre on its end)
        if (k == c->routeLen - 1 && rdSegs[sg].kind == RD_BAYIN) {
            float d = acc + (rdSegs[sg].len - st) + TR_MIN_GAP * CS - 0.15f * CS;
            if (d < gap) { gap = d; vLead = 0; if (!found) why = WHY_BEND; }
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
        acc += en - st;
    }
    // The trip's end: don't overshoot it (a new trip starts there)
    c->gap = gap;
    c->why = why;
    static int traceCar = -2;
    if (traceCar == -2) traceCar = getenv("TR_TRACE") ? atoi(getenv("TR_TRACE")) : -1;
    if (i == traceCar && ((int)(trTime * 60) % 30) == 0)
        printf("t %.1f car %d seg %d (%s) s %.1f/%.1f v %.2f gap %.1f why %d vAllow %.1f hold %d req %d routeIdx %d/%d\n", trTime, i, c->route[c->routeIdx], rdSegs[c->route[c->routeIdx]].kind == RD_CONN ? "conn" : "lane", c->s / CS, rdSegs[c->route[c->routeIdx]].len / CS, v / CS, gap / CS, why, vAllow / CS, c->holdNode, c->requested, c->routeIdx, c->routeLen);

    if (c->lcCool > 0) c->lcCool -= dt;
    else MaybeChangeLane(i, lead, gap, fmaxf(vAllow, 0.5f * CS));

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
    if (c->ghostLane >= 0) {   // still partly in the old lane until our centre is within a cell of the new one
        float cx = c->x - cosf(c->heading) * wb * 0.5f + cosf(hp) * wb * 0.5f, cy = c->y - sinf(c->heading) * wb * 0.5f + sinf(hp) * wb * 0.5f;
        if (fabsf((cx - px) * -sinf(hp) + (cy - py) * cosf(hp)) < 1.0f * CS && c->latOff == 0) c->ghostLane = -1;
    }
    if (c->latOff != 0) {   // changing lanes: aim at a line drifting from the old lane to the new one
        px += -sinf(hp) * c->latOff; py += cosf(hp) * c->latOff;
        // by distance driven, not time: a car changing lanes in a queue mustn't be told to be
        // in the new lane while it stands still (it ended up across both, and was hit)
        float shrink = v * dt * (3.0f / TR_LANE_CHANGE_CELLS);
        if (fabsf(c->latOff) <= shrink) c->latOff = 0; else c->latOff -= c->latOff > 0 ? shrink : -shrink;
    }
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
    while (sGuess > StepEnd(c, c->routeIdx) && c->routeIdx < c->routeLen - 1) {
        sGuess = sGuess - StepEnd(c, c->routeIdx) + StepStart(c, c->routeIdx + 1);
        c->routeIdx++;
        seg = c->route[c->routeIdx];
        if (rdSegs[seg].kind == RD_CONN && tlOn[rdSegs[seg].node]) {   // entering a lit junction
            trafficLightEntries++;
            if (LightState(rdSegs[seg].node, tlGroup[rdSegs[seg].from], NULL) == 0) trafficRedRuns++;
        }
    }
    c->s = ProjectOn(seg, sGuess, c->x, c->y);
    // Out of the bay we left and on the lane: it's free
    if (c->bayFrom >= 0 && rdSegs[seg].kind != RD_BAYOUT) {
        if (bayCar[c->bayFrom] == i) bayCar[c->bayFrom] = -1;
        c->bayFrom = -1;
    }

    // Release the junction once we're through it: the connector is the segment just
    // behind us and our rear is out of it
    if (c->holdNode >= 0 && c->routeIdx > 0 && c->route[c->routeIdx - 1] == c->holdNode &&
        c->s > L * 0.5f + 0.3f * CS)
        Release(i);

    // Stats
    c->legTime += dt;
    c->driveTime += dt;
    if (v < 0.1f * CS && c->parkTime <= 0) { c->stillTime += dt; if (c->stillTime > trafficWaitMax) trafficWaitMax = c->stillTime; }
    else c->stillTime = 0;

    // Trip done: parked in the bay (10-40 s), or (no bay) the next trip starts from here
    if (c->routeIdx == c->routeLen - 1 && rdSegs[seg].kind == RD_BAYIN) {
        if (c->s >= rdSegs[seg].len - 0.3f * CS || (v < 0.05f * CS && c->s >= rdSegs[seg].len - 1.0f * CS)) {
            c->legsDone++;
            c->speed = 0;
            c->parkTime = 10 + 30 * (TrRand() / 32767.0f);
            c->route[0] = (RdSegId)seg; c->routeLen = 1; c->routeIdx = 0;
        }
    } else if (c->routeIdx == c->routeLen - 1 && c->s >= c->destS) {
        c->legsDone++;
        NewTrip(i);
    }
}

// ---------------------------------------------------------------------------
// "Not silly" samples: cars touching each other, cars touching walls
// ---------------------------------------------------------------------------
// Cars bucketed in a coarse grid (8 cells) so the touch test only compares neighbours
#define TR_BUCKET 8
static int bucketHead[(2048 / TR_BUCKET + 1) * (2048 / TR_BUCKET + 1)], bucketNext[MAX_CARS];

static void Sample(void) {
    int pairs = 0, off = 0;
    int bw = gridWidth / TR_BUCKET + 1, bh = gridHeight / TR_BUCKET + 1;
    for (int b = 0; b < bw * bh; b++) bucketHead[b] = -1;
    for (int i = 0; i < carCount; i++) {
        const Car* a = &cars[i];
        if (!a->active || !a->onRoad) continue;
        int bx = (int)(a->x / CELL_SIZE) / TR_BUCKET, by = (int)(a->y / CELL_SIZE) / TR_BUCKET;
        if (bx < 0 || by < 0 || bx >= bw || by >= bh) continue;
        bucketNext[i] = bucketHead[by * bw + bx];
        bucketHead[by * bw + bx] = i;
    }
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
        int bx = (int)(a->x / CELL_SIZE) / TR_BUCKET, by = (int)(a->y / CELL_SIZE) / TR_BUCKET;
        for (int ny = by - 1; ny <= by + 1; ny++)
        for (int nx = bx - 1; nx <= bx + 1; nx++) {
        if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
        for (int j = bucketHead[ny * bw + nx]; j >= 0; j = bucketNext[j]) {
            if (j <= i) continue;
            const Car* b = &cars[j];
            float dx = a->x - b->x, dy = a->y - b->y;
            if (dx * dx + dy * dy > 9.0f * CELL_SIZE * CELL_SIZE) continue;
            if (CarBodiesOverlap(a->x, a->y, a->heading, b->x, b->y, b->heading, -2.0f)) {
                pairs++;
                static int told = 0;
                if (getenv("TR_TOUCH") && told++ < 10) {
                    int sa = a->route[a->routeIdx], sb = b->route[b->routeIdx];
                    printf("touch t %.1f: car %d on %s %d (node %d, from %d to %d) s %.1f v %.1f hold %d | car %d on %s %d (node %d, from %d to %d) s %.1f v %.1f hold %d [%.1f,%.1f h%.0f / %.1f,%.1f h%.0f]\n", trTime,
                           i, rdSegs[sa].kind == RD_CONN ? "conn" : "lane", sa, rdSegs[sa].node, rdSegs[sa].from, rdSegs[sa].to, a->s / CELL_SIZE, a->speed / CELL_SIZE, a->holdNode,
                           j, rdSegs[sb].kind == RD_CONN ? "conn" : "lane", sb, rdSegs[sb].node, rdSegs[sb].from, rdSegs[sb].to, b->s / CELL_SIZE, b->speed / CELL_SIZE, b->holdNode,
                           a->x / CELL_SIZE, a->y / CELL_SIZE, a->heading * 57.3f, b->x / CELL_SIZE, b->y / CELL_SIZE, b->heading * 57.3f);
                }
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
    PROFILE_BEGIN(TrafficLists);
    if (trRoutesTick > trRoutesTickMax) trRoutesTickMax = trRoutesTick;
    trRoutesTick = 0;
    for (int s = 0; s < rdSegCount; s++) segHead[s] = ghostHead[s] = -1;
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active || !c->onRoad) continue;
        int sg = c->route[c->routeIdx];
        carNext[i] = segHead[sg];
        segHead[sg] = i;
        if (c->ghostLane >= 0) { ghostNext[i] = ghostHead[c->ghostLane]; ghostHead[c->ghostLane] = i; }
    }
    PROFILE_END(TrafficLists);
    // Sorted per segment (counting sort into slices, then insertion sort by s: few each)
    {
        int at = 0;
        for (int s = 0; s < rdSegCount; s++) {
            segFirst[s] = at;
            int n = 0;
            for (int o = segHead[s]; o >= 0; o = carNext[o]) {
                int j = at + n++;
                while (j > at && cars[sortedCars[j - 1]].s > cars[o].s) { sortedCars[j] = sortedCars[j - 1]; j--; }
                sortedCars[j] = o;
            }
            segCnt[s] = n;
            at += n;
        }
        for (int j = 0; j < at; j++) carSlot[sortedCars[j]] = j;
    }
    // Queues make a lane look longer to new trips (drivers avoid jams): every half second
    static int queueTick = 0;
    if ((queueTick++ % 30) == 0)
        for (int s = 0; s < rdSegCount; s++) {
            float slow = 0;
            for (int o = segHead[s]; o >= 0; o = carNext[o]) if (cars[o].speed < 1.0f * CELL_SIZE) slow += 1;
            rdSegExtra[s] = slow * TR_QUEUE_COST * CELL_SIZE;
        }
    CountRings();
    PROFILE_BEGIN(TrafficJunctions);
    ServeJunctions(dt);
    PROFILE_END(TrafficJunctions);
    PROFILE_BEGIN(TrafficDrive);
    for (int i = 0; i < carCount; i++)
        if (cars[i].active && cars[i].onRoad) DriveCar(i, dt);
    PROFILE_END(TrafficDrive);
    static int tick = 0;
    PROFILE_BEGIN(TrafficSample);
    if ((tick++ % 30) == 0) Sample();
    PROFILE_END(TrafficSample);
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
    if (getenv("TR_NET")) {
        static int in[RD_MAX_SEGS];
        memset(in, 0, sizeof(in));
        int noOut = 0, noIn = 0, unreach = 0;
        for (int s = 0; s < rdSegCount; s++) if (rdSegs[s].kind == RD_CONN) in[rdSegs[s].to]++;
        for (int i = 0; i < RoadLaneCount(); i++) {
            int l = RoadLaneAt(i);
            if (rdSegs[l].nOut == 0) { if (noOut++ < 5) printf("  lane %d: no way out (node %d, %d roads, ring %d)\n", l, rdSegs[l].to, rdNodes[rdSegs[l].to].nEdge, rdNodes[rdSegs[l].to].ring); }
            if (in[l] == 0) { if (noIn++ < 5) printf("  lane %d: no way in (from node %d, %d roads, ring %d)\n", l, rdSegs[l].from, rdNodes[rdSegs[l].from].nEdge, rdNodes[rdSegs[l].from].ring); }
        }
        RdSegId tmp[CAR_MAX_ROUTE];
        int l0 = RoadLaneAt(0);
        for (int i = 0; i < RoadLaneCount(); i++) if (RoadRoute(l0, RoadLaneAt(i), tmp, CAR_MAX_ROUTE) <= 0) unreach++;
        printf("network: %d lanes, %d with no way out, %d with no way in, %d unreachable from lane %d\n", RoadLaneCount(), noOut, noIn, unreach, l0);
    }
    if (getenv("TR_RING")) {
        printf("ring: entering asks %ld, ring full %ld\n", dbgEnterAsk, dbgRingFull);
        for (int r = 0; r < rdNodeCount; r++) {
            int n = 0;
            for (int i = 0; i < carCount; i++) if (cars[i].onRoad && OnRing(&cars[i], r)) n++;
            bool any = false;
            for (int i = 0; i < RoadLaneCount(); i++) { const RdSeg* S = &rdSegs[RoadLaneAt(i)]; if (S->priority && rdNodes[S->to].ringOf == r) any = true; }
            if (!any) continue;
            printf("roundabout %d: OnRing %d, full %d\n", r, n, RingFull(r));
            for (int i = 0; i < RoadLaneCount(); i++) {
                int l = RoadLaneAt(i);
                const RdSeg* S = &rdSegs[l];
                if (!S->priority || rdNodes[S->to].ringOf != r) continue;
                printf("  ring lane %d (len %.1f) cars:", l, S->len / CELL_SIZE);
                for (int o = segHead[l]; o >= 0; o = carNext[o]) printf(" %d@%.1f", o, cars[o].s / CELL_SIZE);
                printf("\n");
            }
        }
    }
    if (getenv("TR_STILL"))
        for (int i = 0; i < carCount; i++) {
            const Car* c = &cars[i];
            if (!c->onRoad || c->stillTime < 60) continue;
            int sg = c->route[c->routeIdx], nx = c->routeIdx + 1 < c->routeLen ? c->route[c->routeIdx + 1] : -1;
            printf("still %.0f s: car %d at (%.1f,%.1f) on seg %d kind %d prio %d s %.1f/%.1f why %d gap %.1f req %d noRoom %.1f hold %d next %d (kind %d node %d) parked %.0f pullWait %.0f\n",
                   c->stillTime, i, c->x / CELL_SIZE, c->y / CELL_SIZE, sg, rdSegs[sg].kind, rdSegs[sg].priority, c->s / CELL_SIZE, rdSegs[sg].len / CELL_SIZE,
                   c->why, c->gap / CELL_SIZE, c->requested, c->noRoom, c->holdNode, nx, nx >= 0 ? (int)rdSegs[nx].kind : -1, nx >= 0 ? rdSegs[nx].node : -1,
                   c->parkTime, c->pullWait);
        }
    if (getenv("TR_BAY")) {
        int sg = atoi(getenv("TR_BAY"));
        const RdBay* B = &rdBays[rdSegs[sg].bay];
        float x, y, h;
        printf("bay %d: lane %d len %.1f, s0 %.1f s1 %.1f, in %d (len %.1f) out %d (len %.1f)\n", rdSegs[sg].bay, B->lane, rdSegs[B->lane].len / CELL_SIZE,
               B->s0 / CELL_SIZE, B->s1 / CELL_SIZE, B->in, rdSegs[B->in].len / CELL_SIZE, B->out, rdSegs[B->out].len / CELL_SIZE);
        for (float t = 0; t <= rdSegs[B->lane].len; t += 2 * CELL_SIZE) { RoadPointAt(B->lane, t, &x, &y, &h); printf("  lane s %.0f: %.1f,%.1f h%.0f\n", t / CELL_SIZE, x / CELL_SIZE, y / CELL_SIZE, h * 57.3f); }
        for (float t = 0; t <= rdSegs[B->out].len; t += CELL_SIZE) { RoadPointAt(B->out, t, &x, &y, &h); printf("  out s %.0f: %.1f,%.1f h%.0f\n", t / CELL_SIZE, x / CELL_SIZE, y / CELL_SIZE, h * 57.3f); }
    }
    if (getenv("TR_JDUMP")) DumpJunctions();
    int n = 0, trips = 0, moving = 0, still20 = 0, held = 0, asked = 0;
    float drive = 0, speedSum = 0;
    for (int i = 0; i < carCount; i++) {
        const Car* c = &cars[i];
        if (!c->active || !c->onRoad) continue;
        n++; trips += c->legsDone; drive += c->driveTime; speedSum += c->speed;
        if (c->speed > 0.5f * CELL_SIZE) moving++;
        if (c->stillTime > 20 && c->parkTime <= 0) still20++;
        if (c->holdNode >= 0) {
            held++;
            if (getenv("TR_HOLDS")) printf("  hold: car %d holds %d, on seg %d (kind %d) step %d/%d, parked %.0f, prev step seg %d\n", i, c->holdNode,
                                          c->route[c->routeIdx], rdSegs[c->route[c->routeIdx]].kind, c->routeIdx, c->routeLen, c->parkTime,
                                          c->routeIdx > 0 ? c->route[c->routeIdx - 1] : -1);
        }
        if (c->requested) asked++;
    }
    if (n == 0) return;
    printf("Roads: %d junctions (%d with lights), %d roads, %d lanes, %d segments\n", rdNodeCount, trafficLightCount, rdEdgeCount, RoadLaneCount(), rdSegCount);
    printf("Traffic: %d cars, %d trips (%.1f per car per driving minute), mean speed now %.1f cells/s, %d moving now\n",
           n, trips, drive > 0 ? trips / (drive / 60.0f) * 1.0f : 0, speedSum / n / CELL_SIZE, moving);
    int rer = 0, ot = 0, kr = 0;
    for (int i = 0; i < carCount; i++) { rer += cars[i].reroutes; ot += cars[i].overtakes; kr += cars[i].keepRights; }
    printf("  lane changes: %d out to overtake, %d back to the right-hand lane\n", ot, kr);
    printf("  junctions: %d cars hold a connector, %d waiting for one; longest wait for a grant %.1f s; %d re-routes round full exits\n",
           held, asked, trafficJunctionWaitMax, rer);
    int parked = 0;
    for (int i = 0; i < carCount; i++) if (cars[i].active && cars[i].onRoad && cars[i].parkTime > 0) parked++;
    printf("  route searches: %ld for trips (%ld found nothing), %ld re-routes (%ld), %ld lane changes (%ld); most in one tick %d\n",
           trRoutes[0], trRouteFails[0], trRoutes[1], trRouteFails[1], trRoutes[2], trRouteFails[2], trRoutesTickMax);
    if (trafficLightCount > 0)
        printf("  lights: %d cars entered lit junctions, %d of them on red\n", trafficLightEntries, trafficRedRuns);
    printf("  parking: %d bays, %d cars parked now; longest wait to pull out %.1f s\n", rdBayCount, parked, trafficPullOutWaitMax);
    printf("  still > 20 s now (not parked): %d; longest standstill %.1f s\n", still20, trafficWaitMax);
    printf("  silly: car pairs touching max %d per sample (avg %.3f over %d samples), cars touching walls max %d\n",
           trafficOverlapMax, trafficSamples ? (double)trafficOverlapSamples / trafficSamples : 0.0, trafficSamples, trafficOffRoadMax);
}
