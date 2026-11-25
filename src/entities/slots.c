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

float carSlotCells = 3.0f;
bool carSlotReal = false;
bool carSlotPhysical = true;
bool carSlotLaneChange = true;
static float SL_REAL_PULL = 2.0f, SL_REAL_ACCEL = 1.5f;   // (tuning: env SL_PULL / SL_ACCEL)

typedef struct { float x, y, h, v, steer; bool on; } SlReal;
static SlReal* slReal = NULL;
static int slRealCap = 0;
static float slVX0 = 0, slVY0 = 0, slVX1 = -1, slVY1 = -1;   // the view (px); empty until set
static long slRealTicks = 0, slRealSum = 0, slRealSnaps = 0, slRealErrN = 0;
static double slRealErrSum = 0;
static float slRealErrMax = 0;
static int slRealTouchMax = 0;

void SlotsSetView(float x0, float y0, float x1, float y1) { slVX0 = x0; slVY0 = y0; slVX1 = x1; slVY1 = y1; }
int SlotsVmax(void) { int v = (int)(9.0f / carSlotCells + 0.5f); return v < 1 ? 1 : v; }

SlotCar* slotCars = NULL;
int slotCarCount = 0;
static int slCarCap = 0;

static int* slSeg0 = NULL;          // first slot of each segment
static unsigned char* slShared = NULL;   // junction curve: its first slots this close to a sibling curve
                                         // (one from the same lane) that two cars there would touch
static unsigned short* slSegN = NULL;   // slots on each segment (0: not used, e.g. bays)
static int* slSlotCar = NULL;           // the car in each slot, -1 empty
static int slTotal = 0, slSegCap = 0, slSlotCap = 0;
static float slAcc = 0;               // seconds into the current step
static int slStep = 0;
static unsigned int slRng = 1;
static long slRoutes = 0, slRouteFails = 0, slReroutes = 0, slOvertakes = 0, slKeepRights = 0, slMerges = 0, slMergeGiveUps = 0;
static int slStillMax = 0;
static long slTouchSamples = 0, slTouchSum = 0;   // SL_TOUCH diagnostic
static int slTouchMax = 0;
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
    slRealTicks = slRealSum = slRealSnaps = slRealErrN = 0;
    slRealErrSum = 0;
    slRealErrMax = 0;
    slRealTouchMax = 0;
    slRealTicks = slRealSum = slRealSnaps = slRealErrN = 0;
    slRealErrSum = 0;
    slRealErrMax = 0;
    slRealTouchMax = 0;
    slTotal = 0;
    slAcc = 0;
    slStep = 0;
    slRoutes = slRouteFails = slReroutes = slOvertakes = slKeepRights = slMerges = slMergeGiveUps = 0;
    slTouchSamples = slTouchSum = 0;
    slTouchMax = 0;
    slStillMax = 0;
    slRouteCursor = 0;
}

static int SlIdx(int seg, int k) { return slSeg0[seg] + k; }
static float slSlotCells(void) { return SL_SLOT_CELLS * CELL_SIZE; }   // px per slot (about: slots stretch per segment)
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
    // Shared starts: curves from one lane start together; until they part by a car width (2.6
    // cells) a car on one and a car on the other would overlap (the user saw a straight-on car
    // drive through one turning right ahead of it)
    slShared = realloc(slShared, (size_t)rdSegCount);
    memset(slShared, 0, (size_t)rdSegCount);
    float lim = 2.6f * CELL_SIZE;
    for (int s = 0; s < rdSegCount; s++) {
        if (rdSegs[s].kind != RD_CONN) continue;
        const RdNode* N = &rdNodes[rdSegs[s].node];
        int shared = 0;
        for (int k = 0; k < slSegN[s]; k++) {
            float x, y, h;
            RoadPointAt(s, SlS(s, k), &x, &y, &h);
            bool near = false;
            for (int q = 0; q < N->nConn && !near; q++) {
                int sb = N->conn[q];
                if (sb == s || rdSegs[sb].from != rdSegs[s].from) continue;
                const RdSeg* B = &rdSegs[sb];
                for (int p = 0; p < B->n && !near; p++) {
                    float dx = rdX[B->p0 + p] - x, dy = rdY[B->p0 + p] - y;
                    if (dx * dx + dy * dy < lim * lim) near = true;
                }
            }
            if (!near) break;
            shared = k + 1;
        }
        slShared[s] = (unsigned char)shared;
    }
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

int SlotsSpawn(int count, unsigned int seed, float cabShare) {
    slRng = seed ? seed : 1;
    if (count > slCarCap) {
        slCarCap = count;
        slotCars = realloc(slotCars, sizeof(SlotCar) * slCarCap);
    }
    if (count > slRealCap) { slRealCap = count; slReal = realloc(slReal, sizeof(SlReal) * slRealCap); }
    memset(slReal, 0, sizeof(SlReal) * count);
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
        c->cab = SlRand() < (unsigned)(cabShare * 32768);
        c->lcStep = -1;
        slSlotCar[idx] = slotCarCount;
        slGrantStep[slotCarCount] = -1;
        SlQueueRoute(slotCarCount);
        slotCarCount++;
    }
    return slotCarCount;
}

// A new trip from the lane the car is on: a random lane and slot
// Route searches run between steps, while the car may still be drawn gliding from its last
// slot to this one: the new route keeps that stretch (old steps prevRi .. ri) in front, so the
// glide carries on (resetting "previous" to "now" made the car snap forward - seen by the user)
static void SlSetRoute(SlotCar* c, const unsigned short* r, int n, int destSlot) {
    unsigned short pre[SL_MAX_ROUTE];
    int keep = c->ri - c->prevRi;
    if (keep < 0 || keep + n > SL_MAX_ROUTE) keep = 0;
    for (int k = 0; k < keep; k++) pre[k] = c->route[c->prevRi + k];
    if (keep == 0) c->prevSlot = c->prevRi == c->ri ? c->prevSlot : c->slot;
    memcpy(c->route, pre, sizeof(unsigned short) * keep);
    memcpy(c->route + keep, r, sizeof(unsigned short) * n);
    c->routeLen = keep + n;
    c->prevRi = 0;
    c->ri = keep;
    c->destSlot = destSlot;
}

static bool SlNewTrip(int i) {
    SlotCar* c = &slotCars[i];
    int cur = c->route[c->ri];
    if (rdSegs[cur].kind != RD_LANE) return false;
    int lanes = RoadLaneCount();
    unsigned short r[SL_MAX_ROUTE];
    for (int tries = 0; tries < 6; tries++) {
        int dl = RoadLaneAt((int)((SlRand() * 32768u + SlRand()) % (unsigned)lanes));
        int ds = (int)(SlRand() % slSegN[dl]);
        int n;
        if (dl == cur && ds > c->slot) { r[0] = (unsigned short)cur; n = 1; }
        else {
            slRoutes++;
            n = RoadRoute(cur, dl, r, SL_MAX_ROUTE - 8);
            if (n <= 0) { slRouteFails++; continue; }
        }
        SlSetRoute(c, r, n, ds);
        c->needRoute = false;
        return true;
    }
    r[0] = (unsigned short)cur;
    SlSetRoute(c, r, 1, slSegN[cur] - 1);
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
            if (rdSegs[seg].parallel == next) break;   // a lane change to come: not onward from the lane's end
            ri++;
            k = 0;
            seg = next;
        }
        if (slSlotCar[SlIdx(seg, k)] >= 0) break;
        n++;
    }
    return n;
}

// --- A more physical slot model (carSlotPhysical): braking takes several steps and curves
// have a speed. Without it a slot car could stop dead from full speed and take a junction
// curve at full speed - invisible as slots, impossible for a real car (carSlotReal snapped).
static int SlBrakeSlots(void) {   // slots per step per step a car may slow by (from carBrake)
    int d = (int)(carBrake * SL_STEP * SL_STEP / SL_SLOT_CELLS + 0.5f);
    return d < 1 ? 1 : d;
}
static int SlBrakeDist(int v, int D) {   // slots needed to stop from v
    int s = 0;
    while (v > 0) { s += v; v -= D; }
    return s;
}
static int SlCurveV(int conn) {           // top speed on a curve, slots per step
    int v = (int)(rdSegs[conn].vmax * SL_STEP / SL_SLOT_CELLS);
    if (v < 1) v = 1;
    return v > SL_VMAX ? SL_VMAX : v;
}
// Will this curve probably be granted when we get there? (green or no light, no crossing curve
// in use, room past it) - then drive at it at its curve speed, else brake for its stop line
// Is a sibling of curve conn (one from the same lane) busy where it starts together with conn?
static bool SlSiblingBusy(int conn) {
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    for (int q = 0; q < N->nConn; q++) {
        int sb = N->conn[q];
        if (sb == conn || rdSegs[sb].from != C->from) continue;
        if (slConnStamp[sb] == slStep) return true;   // someone was let onto it this step
        for (int k = 0; k < slShared[sb]; k++) if (slSlotCar[SlIdx(sb, k)] >= 0) return true;
    }
    return false;
}

static bool SlCurveLooksOpen(int conn) {
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    int light = TrafficLightAt(C->node, C->from);
    if (light == 0 || light == 1) return false;
    if (SlSiblingBusy(conn)) return false;
    int freeRun = 0;
    while (freeRun < slSegN[C->to] && slSlotCar[SlIdx(C->to, freeRun)] < 0) freeRun++;
    if (freeRun <= slSegCount[conn]) return false;
    for (int j = 0; j < N->nConn; j++) {
        int other = N->conn[j];
        if (other != conn && N->conflict[C->ci][j] && slSegCount[other] > 0) return false;
    }
    return true;
}
// The fastest speed from which we can still stop behind whatever is ahead (Gipps: even if a
// car ahead brakes too), and slow to each curve's speed before it. Looks up to 24 slots ahead.
static int SlSafeSpeed(int i) {
    const SlotCar* c = &slotCars[i];
    int D = SlBrakeSlots(), vmax = SL_VMAX, best = vmax;
    int ri = c->ri, k = c->slot, dist = 0;   // free slots so far
    if (rdSegs[c->route[ri]].kind == RD_CONN) { int vc = SlCurveV(c->route[ri]); if (vc < best) best = vc; }
    for (int look = 0; look < 24; look++) {
        int seg = c->route[ri];
        if (ri == c->routeLen - 1 && k >= c->destSlot) break;   // the trip ends: stop here
        k++;
        if (k >= slSegN[seg]) {
            if (ri + 1 >= c->routeLen) break;
            int next = c->route[ri + 1];
            if (rdSegs[seg].kind == RD_LANE && rdSegs[seg].parallel == next) break;   // a lane change to come: the lane's end is a stop line
            if (rdSegs[next].kind == RD_CONN && rdSegs[seg].kind != RD_CONN) {
                bool open = slGrantStep[i] == slStep || SlCurveLooksOpen(next);
                if (!open) break;                                       // brake for the stop line
                int vc = SlCurveV(next), v = vmax;                      // slow to the curve's speed by then
                while (v > vc && SlBrakeDist(v, D) > dist + SlBrakeDist(vc, D)) v--;
                if (v < best) best = v;
            }
            ri++;
            k = 0;
            seg = next;
        }
        int o = slSlotCar[SlIdx(seg, k)];
        if (o >= 0) {                                                   // a car ahead: Gipps
            int vo = slotCars[o].stopSteps > 0 ? 0 : slotCars[o].v;
            int v = vmax;
            while (v > 0 && SlBrakeDist(v, D) > dist + SlBrakeDist(vo, D)) v--;
            return v < best ? v : best;
        }
        dist++;
    }
    if (dist < 24) {                                                    // a stop (line / trip end) dist slots ahead
        int v = vmax;
        while (v > 0 && SlBrakeDist(v, D) > dist) v--;
        if (v < best) best = v;
    }
    return best;
}

// A car changing lanes is drawn half in its old lane during the step: its old slot stays its
// own until the step's moves are done, so the car behind can't drive into it meanwhile (it
// did: the two were drawn overlapping)
static int slGhostIdx[64], slGhostCar[64], slGhostN = 0;
static void SlGhost(int idx, int car) {
    slSlotCar[idx] = car;
    if (slGhostN < 64) { slGhostIdx[slGhostN] = idx; slGhostCar[slGhostN] = car; slGhostN++; }
}

// --- Lane changes (two-lane roads): a diagonal move, one slot forward into the lane beside
static int SlParallelSlot(int from, int k, int to) {   // the slot on lane `to` beside slot k of `from`
    int kk = (int)((k + 0.5f) * slSegN[to] / slSegN[from]);
    return kk >= slSegN[to] ? slSegN[to] - 1 : kk;
}
static int SlFreeRun(int seg, int k, int max) {          // free slots ahead of slot k on one lane
    int n = 0;
    while (n < max && k + 1 + n < slSegN[seg] && slSlotCar[SlIdx(seg, k + 1 + n)] < 0) n++;
    return n;
}
static void SlLaneChanges(void) {
    int D = SlBrakeSlots(), changes = 0;
    for (int i = 0; i < slotCarCount && changes < 40; i++) {   // (each change plans a route: at most 40 a step)
        SlotCar* c = &slotCars[i];
        // A lane change the route needs (into the lane its turn is allowed from): as soon as the
        // slots beside allow it, and carry on with the route from there
        if (c->stopSteps == 0 && c->ri + 1 < c->routeLen && rdSegs[c->route[c->ri]].kind == RD_LANE &&
            rdSegs[c->route[c->ri]].parallel == c->route[c->ri + 1]) {
            int A = c->route[c->ri], B = c->route[c->ri + 1], k = c->slot;
            int kb = SlParallelSlot(A, k, B), kt = kb + 1 < slSegN[B] ? kb + 1 : kb;
            // (and nobody right ahead in our lane: half-way, our front corner would clip it)
            bool gap = slSlotCar[SlIdx(B, kt)] < 0 && slSlotCar[SlIdx(B, kb)] < 0 && (k + 1 >= slSegN[A] || slSlotCar[SlIdx(A, k + 1)] < 0);
            int back = 0, kk = kb - 1;
            while (gap && kk >= 0 && slSlotCar[SlIdx(B, kk)] < 0 && back < 16) { back++; kk--; }
            if (gap && kk >= 0 && back < 16 && SlBrakeDist(slotCars[slSlotCar[SlIdx(B, kk)]].v, D) > back) gap = false;
            if (!gap) {   // no gap for a while (a steady stream beside): go another way from this lane
                if (c->v == 0 && ++c->mergeWait > 8) {
                    c->mergeWait = 0;
                    int dest = c->route[c->routeLen - 1];
                    float saved = rdSegExtra[B];
                    rdSegExtra[B] += 1e7f;
                    unsigned short r[SL_MAX_ROUTE];
                    slRoutes++;
                    int n = RoadRoute(A, dest, r, SL_MAX_ROUTE - 8);
                    rdSegExtra[B] = saved;
                    if (n >= 2 && r[1] != B) { SlSetRoute(c, r, n, c->destSlot); slMergeGiveUps++; }
                }
                continue;
            }
            c->mergeWait = 0;
            SlGhost(SlIdx(A, k), i);   // (the old slot stays ours this step: see SlGhost)
            slSlotCar[SlIdx(B, kt)] = i;
            c->lcFromSeg = A; c->lcFromSlot = k; c->lcStep = slStep;
            c->ri++;
            c->prevRi = c->ri;
            c->slot = c->prevSlot = kt;
            c->lcCool = 3;
            slMerges++;
            changes++;
            continue;
        }
        if (c->lcCool > 0) { c->lcCool--; continue; }
        if (c->stopSteps > 0 || c->needRoute) continue;
        int A = c->route[c->ri];
        if (rdSegs[A].kind != RD_LANE || rdSegs[A].parallel < 0) continue;
        int B = rdSegs[A].parallel, k = c->slot;
        // (safe anywhere: speeds are worked out after this pass, so every car behind - on lane B or
        // coming off a junction curve into it - sees us in our new slot; only not in the last slot)
        if (k > slSegN[A] - 2) continue;
        int dest = c->route[c->routeLen - 1];
        if (rdSegs[dest].edge == rdSegs[A].edge) continue;              // our trip ends on this road
        int kb = SlParallelSlot(A, k, B), kt = kb + 1;
        if (kb < 1 || kt > slSegN[B] - 2) continue;
        if (slSlotCar[SlIdx(B, kb)] >= 0 || slSlotCar[SlIdx(B, kt)] >= 0) continue;
        if (k + 1 < slSegN[A] && slSlotCar[SlIdx(A, k + 1)] >= 0) continue;   // right ahead in our lane: our front corner would clip it
        // why change: overtake a slow / standing car just ahead, or back to the right-hand lane
        int aheadA = SlFreeRun(A, k, 8), aheadB = SlFreeRun(B, kt, 8);
        bool keepRight = rdSegs[A].laneIdx == 1;
        bool overtake = false;
        if (!keepRight && aheadA <= 1 && k + aheadA + 1 < slSegN[A]) {
            int o = slSlotCar[SlIdx(A, k + aheadA + 1)];
            if (o >= 0 && (slotCars[o].v <= 1 || slotCars[o].stopSteps > 0) && aheadB >= aheadA + 2) overtake = true;
        }
        // keep right only onto clearly free road (moving back the moment it had as much room made
        // cars swap lanes all the time and everyone behind brake: 16% fewer trips)
        int clear = slSegN[B] - kt - 1 < 4 ? slSegN[B] - kt - 1 : 4;   // (a short lane: free to its end)
        if (!overtake && !(keepRight && aheadB >= clear && aheadB >= aheadA)) continue;
        // the first car behind in lane B must be able to stop in time
        int back = 0, kk = kb - 1;
        while (kk >= 0 && slSlotCar[SlIdx(B, kk)] < 0 && back < 16) { back++; kk--; }
        if (kk >= 0 && back < 16) {
            const SlotCar* o = &slotCars[slSlotCar[SlIdx(B, kk)]];
            if (SlBrakeDist(o->v, D) > back) continue;
        }
        // plan from lane B; then move
        unsigned short r[SL_MAX_ROUTE];
        slRoutes++;
        int n = RoadRoute(B, dest, r, SL_MAX_ROUTE - 8);
        if (n <= 0) { slRouteFails++; continue; }
        if (n >= 2 && r[1] == A) { c->lcCool = 3; continue; }   // the route would need this lane straight back (for its turn): stay
        SlGhost(SlIdx(A, k), i);   // (the old slot stays ours this step: see SlGhost)
        slSlotCar[SlIdx(B, kt)] = i;
        c->lcFromSeg = A; c->lcFromSlot = k; c->lcStep = slStep;
        memcpy(c->route, r, sizeof(unsigned short) * n);
        c->routeLen = n;
        c->ri = c->prevRi = 0;
        c->slot = c->prevSlot = kt;
        c->lcCool = 5;
        if (overtake) slOvertakes++; else slKeepRights++;
        changes++;
    }
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
        if (SlSiblingBusy(conn)) continue;                                // a car from our lane still where the curves start together
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
    if (carSlotLaneChange) SlLaneChanges();
    // speeds (all from the same occupancy), then moves
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        if (c->stopSteps > 0) { c->stopSteps--; slStepV[i] = 0; if (c->stopSteps == 0) SlQueueRoute(i); continue; }   // a cab at a fare
        if (c->lcStep == slStep) { slStepV[i] = -1; continue; }   // changed lanes this step (it keeps its speed)
        int v = c->v + 1 > SL_VMAX ? SL_VMAX : c->v + 1;
        if (carSlotPhysical) { int vs = SlSafeSpeed(i); if (v > vs) v = vs; }   // brake in time, slow for curves
        int f = SlFreeAhead(i, v);
        if (v > f) v = f;
        if (v > 0 && SlRand() < (unsigned)(SL_DAWDLE * 32768)) v--;
        slStepV[i] = v;
    }
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        if (slStepV[i] < 0) continue;   // changed lanes this step: already moved
        c->prevRi = c->ri;
        c->prevSlot = c->slot;
        int v = slStepV[i];
        c->v = v;
        if (v == 0 && c->stopSteps == 0) { c->waitSteps++; if (c->waitSteps > slStillMax) slStillMax = c->waitSteps; }
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
            if (c->cab) c->stopSteps = 3 + (int)(SlRand() % 6);   // a fare: stand a few steps, then a new trip
            else SlQueueRoute(i);
        }
    }
    for (int g = 0; g < slGhostN; g++) if (slSlotCar[slGhostIdx[g]] == slGhostCar[g]) slSlotCar[slGhostIdx[g]] = -1;
    slGhostN = 0;
}

// Real cars in view: chase the slot pose (speed: the slot's + a pull toward it; steering:
// pure pursuit on a point a little ahead of it), the same bicycle model as the steered cars
static void SlRealTick(float dt) {
    static int tuned = 0;
    if (!tuned) { tuned = 1; if (getenv("SL_PULL")) SL_REAL_PULL = (float)atof(getenv("SL_PULL")); if (getenv("SL_ACCEL")) SL_REAL_ACCEL = (float)atof(getenv("SL_ACCEL")); }
    if (!carSlotReal || slVX1 < slVX0) {
        for (int i = 0; i < slotCarCount; i++) slReal[i].on = false;
        return;
    }
    float CS = CELL_SIZE, m = 3 * CS;
    float wb = carLength * CS * 0.6f, lock = carMaxSteerDeg * 0.0174533f, rate = carSteerRate * 0.0174533f * dt;
    int n = 0;
    for (int i = 0; i < slotCarCount; i++) {
        SlReal* R = &slReal[i];
        if (!SlotCarVisible(i, slVX0 - m, slVY0 - m, slVX1 + m, slVY1 + m)) { R->on = false; continue; }
        float tx, ty, th;
        SlotCarPose(i, &tx, &ty, &th);
        float vT = slotCars[i].v * SL_SLOT_CELLS * CS / SL_STEP;
        if (slotCars[i].stopSteps > 0) vT = 0;
        if (!R->on) { R->x = tx; R->y = ty; R->h = th; R->v = vT; R->steer = 0; R->on = true; n++; continue; }
        n++;
        float dx = tx - R->x, dy = ty - R->y, err = sqrtf(dx * dx + dy * dy);
        if (err > 3 * CS) {
            static long byKind[4], told = 0;
            const SlotCar* sc = &slotCars[i];
            byKind[rdSegs[sc->route[sc->ri]].kind]++;
            if (getenv("SL_SNAPS") && (told++ % 25) == 0)
                printf("snaps so far: on lanes %ld, on junction curves %ld; this one: v %d slot %d/%d err %.1f cells, heading off %.0f deg, real speed %.1f vs slot %.1f cells/s\n",
                       byKind[RD_LANE], byKind[RD_CONN], sc->v, sc->slot, slSegN[sc->route[sc->ri]], err / CS,
                       CarAngleWrap(th - R->h) * 57.3f, R->v / CS, vT / CS);
            R->x = tx; R->y = ty; R->h = th; R->v = vT; R->steer = 0; slRealSnaps++; continue;
        }
        slRealErrSum += err; slRealErrN++;
        if (err > slRealErrMax) slRealErrMax = err;
        // speed: keep up with the slot, closing the gap along our heading
        float along = dx * cosf(R->h) + dy * sinf(R->h);
        float vWant = vT + SL_REAL_PULL * along;
        if (vWant < 0) vWant = 0;
        float a = (vWant - R->v) / dt, aUp = carAccel * SL_REAL_ACCEL * CS, aDown = carBrake * 3 * CS;
        if (a > aUp) a = aUp;
        if (a < -aDown) a = -aDown;
        R->v += a * dt;
        if (R->v < 0) R->v = 0;
        // steering: aim a little ahead of the slot pose
        float Ld = 1.2f * CS + 0.4f * R->v;
        float ax = tx + cosf(th) * Ld, ay = ty + sinf(th) * Ld;
        float rx = R->x - cosf(R->h) * wb * 0.5f, ry = R->y - sinf(R->h) * wb * 0.5f;
        float ddx = ax - rx, ddy = ay - ry, dist = sqrtf(ddx * ddx + ddy * ddy);
        float alpha = CarAngleWrap(atan2f(ddy, ddx) - R->h);
        float want = dist > 1 ? atanf(2 * wb * sinf(alpha) / dist) : 0;
        if (want > lock) want = lock;
        if (want < -lock) want = -lock;
        float d = want - R->steer;
        R->steer += d > rate ? rate : (d < -rate ? -rate : d);
        CarAdvancePose(R->x, R->y, R->h, R->v * dt, R->steer, &R->x, &R->y, &R->h);
    }
    slRealTicks++;
    slRealSum += n;
    // touches between real cars (every half second; few cars in a view: all pairs)
    if ((slRealTicks % 30) == 0) {
        int pairs = 0;
        for (int i = 0; i < slotCarCount; i++) {
            if (!slReal[i].on) continue;
            for (int j = i + 1; j < slotCarCount; j++) {
                if (!slReal[j].on) continue;
                float ex = slReal[i].x - slReal[j].x, ey = slReal[i].y - slReal[j].y;
                if (ex * ex + ey * ey > 9 * CS * CS) continue;
                if (CarBodiesOverlap(slReal[i].x, slReal[i].y, slReal[i].h, slReal[j].x, slReal[j].y, slReal[j].h, -2.0f)) pairs++;
            }
        }
        if (pairs > slRealTouchMax) slRealTouchMax = pairs;
    }
}

bool SlotCarDrawPose(int i, float* x, float* y, float* heading, float* steer) {
    if (carSlotReal && slReal && slReal[i].on) { *x = slReal[i].x; *y = slReal[i].y; *heading = slReal[i].h; *steer = slReal[i].steer; return true; }
    *steer = 0;
    return SlotCarPose(i, x, y, heading);
}

// SL_JUMPS (diagnostic): every tick, every car's drawn pose; count moves bigger than twice
// what the top speed allows in a tick (a visible jump)
static float *jumpX = NULL, *jumpY = NULL;
static long jumpCount = 0;
static float jumpWorst = 0;
static void CheckJumps(float dt) {
    static int on = -1;
    if (on < 0) on = getenv("SL_JUMPS") != NULL;
    if (!on) return;
    static int cap = 0;
    if (slotCarCount > cap) { cap = slotCarCount; jumpX = realloc(jumpX, sizeof(float) * cap); jumpY = realloc(jumpY, sizeof(float) * cap);
                              for (int i = 0; i < cap; i++) jumpX[i] = -1; }
    float limit = 2.0f * SL_VMAX * SL_SLOT_CELLS * CELL_SIZE / SL_STEP * dt + 2.0f;
    for (int i = 0; i < slotCarCount; i++) {
        float x, y, h, st;
        SlotCarDrawPose(i, &x, &y, &h, &st);
        bool inView = slVX1 < slVX0 || (x >= slVX0 && x <= slVX1 && y >= slVY0 && y <= slVY1);   // (with a view: only jumps you could see)
        if (jumpX[i] >= 0 && inView) {
            float d = sqrtf((x - jumpX[i]) * (x - jumpX[i]) + (y - jumpY[i]) * (y - jumpY[i]));
            if (d > limit) {
                jumpCount++;
                if (d > jumpWorst) jumpWorst = d;
                static int told = 0;
                if (told++ < 5) printf("jump: car %d moved %.1f px in a tick (limit %.1f), ri %d/%d slot %d prev %d/%d\n", i, d, limit,
                                       slotCars[i].ri, slotCars[i].routeLen, slotCars[i].slot, slotCars[i].prevRi, slotCars[i].prevSlot);
            }
        }
        jumpX[i] = x; jumpY[i] = y;
    }
}

// SL_TOUCH (diagnostic): every half second, drawn cars that overlap (cars on different curves
// can, slots can't); the first few are printed with what they're on
// (slTouchSamples / slTouchSum / slTouchMax: declared at the top, SlotsClear resets them)

static void CheckTouches(void) {
    static int on = -1, tick = 0;
    if (on < 0) on = getenv("SL_TOUCH") != NULL;
    if (!on || (tick++ % 30) != 0 || slotCarCount > 4000) return;
    static float* px = NULL; static float* py = NULL; static float* ph = NULL;
    static int cap = 0;
    if (slotCarCount > cap) { cap = slotCarCount; px = realloc(px, sizeof(float) * cap); py = realloc(py, sizeof(float) * cap); ph = realloc(ph, sizeof(float) * cap); }
    for (int i = 0; i < slotCarCount; i++) { float st; SlotCarDrawPose(i, &px[i], &py[i], &ph[i], &st); }
    int pairs = 0;
    float near = 3.0f * CELL_SIZE;
    for (int i = 0; i < slotCarCount; i++)
        for (int j = i + 1; j < slotCarCount; j++) {
            float dx = px[i] - px[j], dy = py[i] - py[j];
            if (dx * dx + dy * dy > near * near) continue;
            if (!CarBodiesOverlap(px[i], py[i], ph[i], px[j], py[j], ph[j], -2.0f)) continue;
            pairs++;
            static int told = 0;
            if (told++ < 6) {
                const SlotCar* a = &slotCars[i]; const SlotCar* b = &slotCars[j];
                int sa = a->route[a->ri], sb = b->route[b->ri];
                printf("slot touch: car %d on seg %d (kind %d from %d) slot %d | car %d on seg %d (kind %d from %d) slot %d%s\n", i, sa,
                       rdSegs[sa].kind, rdSegs[sa].from, a->slot, j, sb, rdSegs[sb].kind, rdSegs[sb].from, b->slot,
                       (a->lcStep == slStep || b->lcStep == slStep) ? " (changing lanes)" : "");
            }
        }
    slTouchSamples++;
    slTouchSum += pairs;
    if (pairs > slTouchMax) slTouchMax = pairs;
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
    if (slVX1 < slVX0 && getenv("SL_VIEW")) {   // headless: a view from the environment (cells)
        float a, b, c2, d;
        if (sscanf(getenv("SL_VIEW"), "%f,%f,%f,%f", &a, &b, &c2, &d) == 4) SlotsSetView(a * CELL_SIZE, b * CELL_SIZE, c2 * CELL_SIZE, d * CELL_SIZE);
    }
    PROFILE_BEGIN(SlotsReal);
    SlRealTick(dt);
    PROFILE_END(SlotsReal);
    CheckJumps(dt);
    CheckTouches();
}

bool SlotCarPose(int i, float* x, float* y, float* heading) {
    const SlotCar* c = &slotCars[i];
    int seg = c->route[c->ri];
    float sCur = SlS(seg, c->slot);
    float f = slAcc / SL_STEP;
    if (c->lcStep == slStep) {   // changing lanes this step: from beside the old slot to the new one
        float sA0 = SlS(c->lcFromSeg, c->lcFromSlot), sA1 = sA0 + slSlotCells();
        float sB1 = sCur, sB0 = sB1 - slSlotCells();
        float ax, ay, ah, bx, by, bh;
        RoadPointAt(c->lcFromSeg, sA0 + (sA1 - sA0) * f, &ax, &ay, &ah);
        RoadPointAt(seg, sB0 + (sB1 - sB0) * f, &bx, &by, &bh);
        float w = f * f * (3 - 2 * f);   // smoothstep: an S-curve
        *x = ax + (bx - ax) * w;
        *y = ay + (by - ay) * w;
        float side = 3.0f * CELL_SIZE * 6 * f * (1 - f);   // sideways speed (lanes ~3 cells apart) vs forward
        float lr = (bx - ax) * -sinf(ah) + (by - ay) * cosf(ah) > 0 ? 1.0f : -1.0f;
        *heading = ah + lr * atan2f(side, slSlotCells());
        return true;
    }
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
    if (getenv("SL_TOUCH")) printf("  touches: max %d pairs at once, %ld pair-samples over %ld samples\n", slTouchMax, slTouchSum, slTouchSamples);
    if (getenv("SL_JUMPS")) printf("  jumps: %ld moves bigger than the top speed allows (worst %.0f px)\n", jumpCount, jumpWorst);
    if (carSlotReal)
        printf("  real cars in view: %.1f on average; tracking error mean %.2f cells, max %.2f; %ld snaps; touches max %d pairs\n",
               slRealTicks ? (double)slRealSum / slRealTicks : 0.0, slRealErrN ? slRealErrSum / slRealErrN / CELL_SIZE : 0.0,
               slRealErrMax / CELL_SIZE, slRealSnaps, slRealTouchMax);
    printf("  lane changes: %ld to overtake, %ld back to the right-hand lane, %ld into the lane a turn needs (%ld gave up: another way)\n", slOvertakes, slKeepRights, slMerges, slMergeGiveUps);
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
