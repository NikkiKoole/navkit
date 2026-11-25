// slots.c - The abstract ("Game & Watch") traffic for the cars experiment (see slots.h)
#include "slots.h"
#include "roads.h"
#include "traffic.h"
#include "cars.h"
#include "mover.h"            // CELL_SIZE
#include "../../shared/profiler.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SlotCar* slotCars = NULL;
int slotCarCount = 0;
static int slCarCap = 0;

static int* slSeg0 = NULL;          // first slot of each segment
static unsigned short* slSegN = NULL;   // slots on each segment (0: not used, e.g. bays)
static int* slSlotCar = NULL;           // the car in each slot, -1 empty
static int slTotal = 0, slSegCap = 0, slSlotCap = 0;
static float slAcc = 0;               // seconds into the current step
static int slStep = 0;
static unsigned int slRng = 1;
static long slRoutes = 0, slRouteFails = 0, slReroutes = 0;
static int slStillMax = 0;
static int slRouteCursor = 0;

// per step scratch
static int* slStepV = NULL;
static int* slGrantStep = NULL;         // car: the step it was granted its next curve
static int* slConnStamp = NULL;         // segment: granted this step (curves) / claimed this step (exit lanes)
static int* slExitStamp = NULL;
static int* slSegCount = NULL;          // cars on each segment at the step's start
static int* slReqCars = NULL;
static short* slRingLanes = NULL;
static short* slRingCount = NULL;
static int slScratchCap = 0;

#define SL_ROUTES_PER_TICK 12  // route searches per tick (new trips wait at their lane's end): spread
                               // out, not once a step (500 at once made a 140 ms tick every second)
static int* slRouteQ = NULL;   // cars waiting for a route search, in order
static int slQHead = 0, slQLen = 0, slQCap = 0;
static void SlQueueRoute(int i) {
    if (slQLen >= slQCap) return;
    slRouteQ[(slQHead + slQLen) % slQCap] = i;
    slQLen++;
}

static unsigned int SlRand(void) { slRng = slRng * 1103515245u + 12345u; return (slRng >> 16) & 0x7fff; }

void SlotsClear(void) {
    slotCarCount = 0;
    slTotal = 0;
    slAcc = 0;
    slStep = 0;
    slRoutes = slRouteFails = slReroutes = 0;
    slStillMax = 0;
    slRouteCursor = 0;
}

static int SlIdx(int seg, int k) { return slSeg0[seg] + k; }
static float SlS(int seg, int k) { return (k + 0.5f) * rdSegs[seg].len / slSegN[seg]; }

void SlotsBuild(void) {
    if (rdSegCount > slSegCap) {
        slSegCap = rdSegCount;
        slSeg0 = realloc(slSeg0, sizeof(int) * slSegCap);
        slSegN = realloc(slSegN, sizeof(unsigned short) * slSegCap);
        slConnStamp = realloc(slConnStamp, sizeof(int) * slSegCap);
        slExitStamp = realloc(slExitStamp, sizeof(int) * slSegCap);
        slSegCount = realloc(slSegCount, sizeof(int) * slSegCap);
    }
    slTotal = 0;
    float slot = SL_SLOT_CELLS * CELL_SIZE;
    for (int s = 0; s < rdSegCount; s++) {
        int n = 0;
        if (rdSegs[s].kind == RD_LANE || rdSegs[s].kind == RD_CONN) {
            n = (int)(rdSegs[s].len / slot);
            if (n < 1) n = 1;
        }
        slSeg0[s] = slTotal;
        slSegN[s] = (unsigned short)n;
        slTotal += n;
        slConnStamp[s] = slExitStamp[s] = -1;
    }
    if (slTotal > slSlotCap) { slSlotCap = slTotal; slSlotCar = realloc(slSlotCar, sizeof(int) * slSlotCap); }
    for (int i = 0; i < slTotal; i++) slSlotCar[i] = -1;
    if (rdNodeCount > 0) {
        slRingLanes = realloc(slRingLanes, sizeof(short) * rdNodeCount);
        slRingCount = realloc(slRingCount, sizeof(short) * rdNodeCount);
        memset(slRingLanes, 0, sizeof(short) * rdNodeCount);
        for (int i = 0; i < RoadLaneCount(); i++) {
            const RdSeg* S = &rdSegs[RoadLaneAt(i)];
            if (S->priority && rdNodes[S->to].ringOf >= 0) slRingLanes[rdNodes[S->to].ringOf]++;
        }
    }
}

int SlotsSpawn(int count, unsigned int seed) {
    slRng = seed ? seed : 1;
    if (count > slCarCap) {
        slCarCap = count;
        slotCars = realloc(slotCars, sizeof(SlotCar) * slCarCap);
    }
    if (count > slScratchCap) {
        slScratchCap = count;
        slStepV = realloc(slStepV, sizeof(int) * slScratchCap);
        slGrantStep = realloc(slGrantStep, sizeof(int) * slScratchCap);
        slReqCars = realloc(slReqCars, sizeof(int) * slScratchCap);
        slRouteQ = realloc(slRouteQ, sizeof(int) * slScratchCap);
    }
    slQCap = slScratchCap;
    slQHead = slQLen = 0;
    slotCarCount = 0;
    int lanes = RoadLaneCount();
    for (int tries = 0; tries < count * 20 && slotCarCount < count && lanes > 0; tries++) {
        int lane = RoadLaneAt((int)((SlRand() * 32768u + SlRand()) % (unsigned)lanes));
        int k = (int)(SlRand() % slSegN[lane]);
        int idx = SlIdx(lane, k);
        if (slSlotCar[idx] >= 0) continue;
        SlotCar* c = &slotCars[slotCarCount];
        memset(c, 0, offsetof(SlotCar, route));
        c->route[0] = (unsigned short)lane;
        c->routeLen = 1;
        c->ri = c->prevRi = 0;
        c->slot = c->prevSlot = k;
        c->destSlot = slSegN[lane] - 1;
        c->needRoute = true;   // plans when it gets to the lane's end (searches spread over steps)
        slSlotCar[idx] = slotCarCount;
        slGrantStep[slotCarCount] = -1;
        SlQueueRoute(slotCarCount);
        slotCarCount++;
    }
    return slotCarCount;
}

// A new trip from the lane the car is on: a random lane and slot
static bool SlNewTrip(int i) {
    SlotCar* c = &slotCars[i];
    int cur = c->route[c->ri];
    if (rdSegs[cur].kind != RD_LANE) return false;
    int lanes = RoadLaneCount();
    for (int tries = 0; tries < 6; tries++) {
        int dl = RoadLaneAt((int)((SlRand() * 32768u + SlRand()) % (unsigned)lanes));
        int ds = (int)(SlRand() % slSegN[dl]);
        int n;
        if (dl == cur && ds > c->slot) { c->route[0] = (unsigned short)cur; n = 1; }
        else {
            slRoutes++;
            n = RoadRoute(cur, dl, c->route, SL_MAX_ROUTE);
            if (n <= 0) { slRouteFails++; c->route[0] = (unsigned short)cur; continue; }
        }
        c->routeLen = n;
        c->ri = c->prevRi = 0;
        c->prevSlot = c->slot;
        c->destSlot = ds;
        c->needRoute = false;
        return true;
    }
    c->routeLen = 1; c->ri = c->prevRi = 0; c->prevSlot = c->slot; c->destSlot = slSegN[cur] - 1;
    return false;
}

// Free slots ahead (up to want), passing onto a curve only if granted this step
static int SlFreeAhead(int i, int want) {
    const SlotCar* c = &slotCars[i];
    int ri = c->ri, k = c->slot, n = 0;
    while (n < want) {
        int seg = c->route[ri];
        if (ri == c->routeLen - 1 && k >= c->destSlot) break;   // the trip ends here
        k++;
        if (k >= slSegN[seg]) {
            if (ri + 1 >= c->routeLen) break;
            int next = c->route[ri + 1];
            if (rdSegs[next].kind == RD_CONN && rdSegs[seg].kind != RD_CONN && slGrantStep[i] != slStep) break;
            ri++;
            k = 0;
            seg = next;
        }
        if (slSlotCar[SlIdx(seg, k)] >= 0) break;
        n++;
    }
    return n;
}

// Junction requests: cars at the end of a lane whose next step is a curve
static int SlWantsCurve(int i) {
    const SlotCar* c = &slotCars[i];
    int seg = c->route[c->ri];
    if (rdSegs[seg].kind != RD_LANE || c->ri + 1 >= c->routeLen) return -1;
    // only when we can get there this step (a grant we can't use blocks crossing traffic)
    int vNext = c->v + 1 > SL_VMAX ? SL_VMAX : c->v + 1;
    if (c->slot + vNext < slSegN[seg]) return -1;
    int next = c->route[c->ri + 1];
    return rdSegs[next].kind == RD_CONN ? next : -1;
}

// Our exit has been full for a while: plan again with it counted as very long (as in the
// full sim: this is what breaks gridlock loops of full lanes waiting for each other)
static void SlRerouteAvoiding(int i, int avoidLane) {
    SlotCar* c = &slotCars[i];
    int cur = c->route[c->ri], dest = c->route[c->routeLen - 1];
    if (rdSegs[cur].kind != RD_LANE || dest == avoidLane) return;
    float saved = rdSegExtra[avoidLane];
    rdSegExtra[avoidLane] += 1e7f;
    unsigned short r[SL_MAX_ROUTE];
    slRoutes++;
    int n = RoadRoute(cur, dest, r, SL_MAX_ROUTE);
    rdSegExtra[avoidLane] = saved;
    if (n < 2 || rdSegs[r[1]].to == avoidLane) return;
    memcpy(c->route, r, sizeof(unsigned short) * n);
    c->routeLen = n;
    c->ri = c->prevRi = 0;
    c->prevSlot = c->slot;
    c->noRoomSteps = 0;
    slReroutes++;
}

static int SlReqCompare(const void* a, const void* b) {
    const SlotCar* ca = &slotCars[*(const int*)a];
    const SlotCar* cb = &slotCars[*(const int*)b];
    bool pa = rdSegs[ca->route[ca->ri]].priority, pb = rdSegs[cb->route[cb->ri]].priority;
    if (pa != pb) return pa ? -1 : 1;                    // roundabout traffic first
    return cb->waitSteps - ca->waitSteps;                // then the longest waiting
}

static void SlGrantCurves(void) {
    int nreq = 0;
    for (int i = 0; i < slotCarCount; i++) if (!slotCars[i].needRoute || slotCars[i].ri + 1 < slotCars[i].routeLen)
        if (SlWantsCurve(i) >= 0) slReqCars[nreq++] = i;
    qsort(slReqCars, (size_t)nreq, sizeof(int), SlReqCompare);
    for (int q = 0; q < nreq; q++) {
        int i = slReqCars[q];
        const SlotCar* c = &slotCars[i];
        int conn = c->route[c->ri + 1];
        const RdSeg* C = &rdSegs[conn];
        const RdNode* N = &rdNodes[C->node];
        int light = TrafficLightAt(C->node, C->from);
        if (light == 0 || light == 1) continue;                         // red / amber
        if (slExitStamp[C->to] == slStep) continue;                       // someone else enters that lane now
        // room past the junction for us and every car already on this curve: else a car can
        // end up waiting inside the junction, blocking the crossing paths (that gridlocked)
        int freeRun = 0;
        while (freeRun < slSegN[C->to] && slSlotCar[SlIdx(C->to, freeRun)] < 0) freeRun++;
        if (freeRun <= slSegCount[conn]) { slotCars[i].noRoomSteps++; continue; }
        slotCars[i].noRoomSteps = 0;
        bool clash = false;
        for (int j = 0; j < N->nConn && !clash; j++) {
            int other = N->conn[j];
            if (other == conn || !N->conflict[C->ci][j]) continue;
            if (slSegCount[other] > 0 || slConnStamp[other] == slStep) clash = true;
        }
        if (clash) continue;
        // entering a roundabout: room on the ring, and a gap in the ring traffic arriving here
        if (!rdSegs[C->from].priority && rdSegs[C->to].priority && N->ringOf >= 0) {
            int r = N->ringOf;
            if (slRingCount[r] >= (slRingLanes[r] > 1 ? slRingLanes[r] : 1)) continue;
            bool gap = true;
            for (int e = 0; e < N->nEdge && gap; e++) {
                const RdEdge* E = &rdEdges[N->edge[e]];
                int in = E->b == C->node ? E->lane[0] : E->lane[1];
                if (in < 0 || !rdSegs[in].priority) continue;
                for (int k = slSegN[in] - 2; k < slSegN[in]; k++) if (k >= 0 && slSlotCar[SlIdx(in, k)] >= 0) gap = false;
            }
            if (!gap) continue;
            slRingCount[r]++;
        }
        slGrantStep[i] = slStep;
        slConnStamp[conn] = slStep;
        slExitStamp[C->to] = slStep;
    }
}

static void SlRunStep(void) {
    slStep++;
    // occupancy per segment, roundabout counts
    for (int s = 0; s < rdSegCount; s++) slSegCount[s] = 0;
    if (slRingCount) memset(slRingCount, 0, sizeof(short) * rdNodeCount);
    for (int i = 0; i < slotCarCount; i++) {
        const SlotCar* c = &slotCars[i];
        int seg = c->route[c->ri];
        slSegCount[seg]++;
        const RdSeg* S = &rdSegs[seg];
        if (S->kind == RD_LANE && S->priority) slRingCount[rdNodes[S->to].ringOf]++;
        else if (S->kind == RD_CONN && rdSegs[S->to].priority && rdNodes[S->node].ringOf >= 0) slRingCount[rdNodes[S->node].ringOf]++;
    }
    // Queues make a lane look longer to new routes (drivers avoid jams, as in the full sim):
    // every 5 steps, 8 cells per stopped car
    if (slStep % 5 == 0) {
        for (int s = 0; s < rdSegCount; s++) rdSegExtra[s] = 0;
        for (int i = 0; i < slotCarCount; i++)
            if (slotCars[i].v == 0) rdSegExtra[slotCars[i].route[slotCars[i].ri]] += 8.0f * CELL_SIZE;
    }
    SlGrantCurves();
    // a car refused for want of room for 15 steps looks for another way (a few per step)
    int rer = 0;
    for (int i = 0; i < slotCarCount && rer < 20; i++)
        if (slotCars[i].noRoomSteps >= 15) {
            const SlotCar* c = &slotCars[i];
            int conn = c->ri + 1 < c->routeLen ? c->route[c->ri + 1] : -1;
            if (conn >= 0 && rdSegs[conn].kind == RD_CONN) SlRerouteAvoiding(i, rdSegs[conn].to);
            slotCars[i].noRoomSteps = 0;
            rer++;
        }
    // speeds (all from the same occupancy), then moves
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        int v = c->v + 1 > SL_VMAX ? SL_VMAX : c->v + 1;
        int f = SlFreeAhead(i, v);
        if (v > f) v = f;
        if (v > 0 && SlRand() < (unsigned)(SL_DAWDLE * 32768)) v--;
        slStepV[i] = v;
    }
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        c->prevRi = c->ri;
        c->prevSlot = c->slot;
        int v = slStepV[i];
        c->v = v;
        if (v == 0) { c->waitSteps++; if (c->waitSteps > slStillMax) slStillMax = c->waitSteps; }
        else c->waitSteps = 0;
        if (v > 0) {
            slSlotCar[SlIdx(c->route[c->ri], c->slot)] = -1;
            for (int m = 0; m < v; m++) {
                c->slot++;
                if (c->slot >= slSegN[c->route[c->ri]]) { c->ri++; c->slot = 0; }
            }
            slSlotCar[SlIdx(c->route[c->ri], c->slot)] = i;
        }
        // arrived: the next trip is planned within the budget (it waits at its lane's end)
        if (!c->needRoute && c->ri == c->routeLen - 1 && c->slot >= c->destSlot) {
            c->trips++;
            c->needRoute = true;
            c->destSlot = slSegN[c->route[c->ri]] - 1;
            SlQueueRoute(i);
        }
    }
}

void SlotsTick(float dt) {
    if (slotCarCount == 0 || slTotal == 0) return;
    TrafficClockTick(dt);   // the traffic lights' clock
    PROFILE_BEGIN(SlotsRoutes);
    for (int n = 0; n < SL_ROUTES_PER_TICK && slQLen > 0; n++) {
        int i = slRouteQ[slQHead];
        slQHead = (slQHead + 1) % slQCap;
        slQLen--;
        if (slotCars[i].needRoute && !SlNewTrip(i)) SlQueueRoute(i);   // none found: try again later
    }
    PROFILE_END(SlotsRoutes);
    slAcc += dt;
    while (slAcc >= SL_STEP) {
        slAcc -= SL_STEP;
        PROFILE_BEGIN(SlotsStep);
        SlRunStep();
        PROFILE_END(SlotsStep);
    }
}

bool SlotCarPose(int i, float* x, float* y, float* heading) {
    const SlotCar* c = &slotCars[i];
    int seg = c->route[c->ri];
    float sCur = SlS(seg, c->slot);
    float f = slAcc / SL_STEP;
    if (c->prevRi == c->ri && c->prevSlot == c->slot) { RoadPointAt(seg, sCur, x, y, heading); return true; }
    // from the old slot to the new one along the route
    int pr = c->prevRi;
    float sPrev = SlS(c->route[pr], c->prevSlot);
    float total;
    if (pr == c->ri) total = sCur - sPrev;
    else {
        total = rdSegs[c->route[pr]].len - sPrev;
        for (int k = pr + 1; k < c->ri; k++) total += rdSegs[c->route[k]].len;
        total += sCur;
    }
    float d = f * total, s = sPrev + d;
    int k = pr;
    while (k < c->ri && s > rdSegs[c->route[k]].len) { s -= rdSegs[c->route[k]].len; k++; }
    RoadPointAt(c->route[k], s, x, y, heading);
    return true;
}

bool SlotCarVisible(int i, float x0, float y0, float x1, float y1) {
    const RdSeg* S = &rdSegs[slotCars[i].route[slotCars[i].ri]];
    return !(S->bx1 < x0 || S->by1 < y0 || S->bx0 > x1 || S->by0 > y1);
}

// Why is car i refused its curve? (diagnostic, SL_WHY env)
static void SlExplain(int i) {
    const SlotCar* c = &slotCars[i];
    int seg = c->route[c->ri];
    printf("  car %d waited %d s: on seg %d kind %d slot %d/%d, ri %d/%d, needRoute %d", i, c->waitSteps, seg, rdSegs[seg].kind, c->slot,
           slSegN[seg], c->ri, c->routeLen, c->needRoute);
    int conn = SlWantsCurve(i);
    if (conn < 0) {
        int ahead = -1, ri = c->ri, k = c->slot + 1;
        if (k >= slSegN[seg] && ri + 1 < c->routeLen) { ri++; k = 0; }
        if (ri < c->routeLen && k < slSegN[c->route[ri]]) ahead = slSlotCar[SlIdx(c->route[ri], k)];
        printf(" - no curve wanted; slot ahead holds car %d (seg %d kind %d)\n", ahead, c->route[ri], rdSegs[c->route[ri]].kind);
        return;
    }
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    int freeRun = 0;
    while (freeRun < slSegN[C->to] && slSlotCar[SlIdx(C->to, freeRun)] < 0) freeRun++;
    printf(" - wants curve %d (node %d): light %d, exit lane %d (%d slots) free run %d (on curve %d), in it:", conn, C->node, TrafficLightAt(C->node, C->from),
           C->to, slSegN[C->to], freeRun, slSegCount[conn]);
    for (int k = 0; k < slSegN[C->to]; k++) {
        int o = slSlotCar[SlIdx(C->to, k)];
        if (o >= 0) printf(" [slot %d car %d v %d wait %d ri %d/%d dest %d needRoute %d]", k, o, slotCars[o].v, slotCars[o].waitSteps,
                           slotCars[o].ri, slotCars[o].routeLen, slotCars[o].destSlot, slotCars[o].needRoute);
    }
    for (int j = 0; j < N->nConn; j++) {
        int other = N->conn[j];
        if (other == conn || !N->conflict[C->ci][j] || slSegCount[other] == 0) continue;
        printf(", crossing curve %d occupied", other);
        for (int k = 0; k < slSegN[other]; k++) {
            int o = slSlotCar[SlIdx(other, k)];
            if (o >= 0) printf(" by car %d (slot %d, waited %d)", o, k, slotCars[o].waitSteps);
        }
    }
    printf("\n");
}

void SlotsPrintStats(void) {
    if (getenv("SL_WHY")) {
        int shown = 0;
        int only = atoi(getenv("SL_WHY"));
        for (int i = 0; i < slotCarCount && shown < 12; i++) if ((only > 0 && i == only) || (only <= 0 && slotCars[i].waitSteps > 60)) { SlExplain(i); shown++; }
    }
    int trips = 0, moving = 0, waitingJunction = 0, needRoute = 0;
    long vsum = 0;
    for (int i = 0; i < slotCarCount; i++) {
        const SlotCar* c = &slotCars[i];
        trips += c->trips;
        vsum += c->v;
        if (c->v > 0) moving++;
        if (c->needRoute) needRoute++;
        if (c->v == 0 && SlWantsCurve(i) >= 0) waitingJunction++;
    }
    printf("Slots: %d slots (%.1f cells each) on %d segments; %d cars (%.0f%% of the slots)\n", slTotal, SL_SLOT_CELLS,
           rdSegCount, slotCarCount, slTotal ? 100.0 * slotCarCount / slTotal : 0.0);
    printf("Slot traffic: %d trips, mean speed now %.1f cells/s, %d moving now, %d waiting at junctions, %d waiting for a route\n",
           trips, slotCarCount ? (double)vsum / slotCarCount * SL_SLOT_CELLS / SL_STEP : 0.0, moving, waitingJunction, needRoute);
    printf("  longest standstill %.0f s; route searches %ld (%ld found nothing), %ld re-routes round full exits\n", slStillMax * SL_STEP, slRoutes, slRouteFails, slReroutes);
}

bool SlotsCheck(char* why, int whyLen) {
    int occupied = 0;
    for (int s = 0; s < slTotal; s++) if (slSlotCar[s] >= 0) occupied++;
    for (int i = 0; i < slotCarCount; i++) {
        const SlotCar* c = &slotCars[i];
        if (slSlotCar[SlIdx(c->route[c->ri], c->slot)] != i) {
            snprintf(why, (size_t)whyLen, "slots: car %d is not in its slot", i);
            return false;
        }
    }
    if (occupied != slotCarCount) {
        snprintf(why, (size_t)whyLen, "slots: %d slots occupied for %d cars", occupied, slotCarCount);
        return false;
    }
    int trips = 0;
    for (int i = 0; i < slotCarCount; i++) trips += slotCars[i].trips;
    if (slotCarCount > 0 && trips < 1) { snprintf(why, (size_t)whyLen, "slots: no trips done"); return false; }
    return true;
}
