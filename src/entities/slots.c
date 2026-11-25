// slots.c - The abstract ("Game & Watch") traffic for the cars experiment (see slots.h)
#include "slots.h"
#include "roads.h"
#include "traffic.h"
#include "cars.h"
#include "mover.h"            // CELL_SIZE
#include "trains.h"           // TRANSPORT_RIDING (a driving mover rides its car)
#include "../../shared/profiler.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float carSlotCells = 3.0f;
bool carSlotReal = false;
bool carSlotPhysical = true;
bool carSlotLaneChange = true;
bool carSlotKerbs = true;
bool carSlotTempers = true;
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
static int* slKerbCar = NULL;           // the car in the kerb spot beside each slot, -1 empty
static unsigned char* slKerbOk = NULL;  // per slot: 1 = a kerb spot beside it, 2 = kerb strip (carved), no spot
static long slParks = 0, slFareStops = 0, slKerbFull = 0, slPullOutWaits = 0;
// Pedestrians (movers on the road): slot centres in buckets of SL_BUCKET cells; each step the
// slots a mover is in or walks into within ~a second are stamped and count as taken
#define SL_BUCKET 4
static float *slCX = NULL, *slCY = NULL, *slCH = NULL, *slHalf = NULL, *slCZ = NULL;   // (+ height, levels)   // slot centre, heading, half length (px)
static int* slSlotSeg = NULL;
static int* slPedStamp = NULL;
static int *slYieldUntil = NULL, *slYieldCar = NULL;   // a slot held open for a merging car (until that step)
static long slYields = 0, slYieldMerges = 0, slClassYields = 0;
static int *slBHead = NULL, *slBList = NULL, slBW = 0, slBH = 0;
static bool SlInCar(int car, float x, float y, float grow);
static long slDrives = 0, slDrivesDone = 0, slDriveFails = 0;
static float slDriveSpeed = 2.0f;   // cells/s straight line, running average of finished drives
static long slChoseDrive = 0, slChoseWalk = 0;
static double slSumWalkT = 0, slSumDriveT = 0;
static bool SlDriveTo(int car);
static void SlDriverOut(int car);
static double* slPedHeldAt = NULL;   // gameTime a mover was last held (waiting: no walking ahead)
static int slPedHeldCap = 0;
// Stepping off the kerb is announced: the pedestrian waits for the next step, which then counts
// its path (going out in the middle of a step, cars already moving there drove into it)
static int* slPedGoStep = NULL;
static float *slPedDX = NULL, *slPedDY = NULL;
static long slPedBlocks = 0, slHardStops = 0, slPedHolds = 0, slPedHitSamples = 0, slPedHits = 0;
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
    slParks = slFareStops = slKerbFull = slPullOutWaits = 0;
    slPedBlocks = slHardStops = slPedHolds = slPedHitSamples = slPedHits = 0;
    slYields = slYieldMerges = slClassYields = 0;
    slDrives = slDrivesDone = slDriveFails = 0;
    slDriveSpeed = 2.0f;
    slChoseDrive = slChoseWalk = 0;
    slSumWalkT = slSumDriveT = 0;
    slStillMax = 0;
    slRouteCursor = 0;
}

static int SlIdx(int seg, int k) { return slSeg0[seg] + k; }
// a segment's top speed, slots per step (its class's limit; a street: SL_VMAX)
static int SlSegV(int seg) {
    if (!rdClasses) return SL_VMAX;
    int v = (int)(RoadSegSpeed(seg) * SL_STEP / SL_SLOT_CELLS + 0.5f);
    return v < 1 ? 1 : v;
}
static int SlVmaxAny(void) {   // the fastest class's
    int v = SL_VMAX;
    if (rdClasses) for (int k = 0; k < RD_CLS_COUNT; k++) { int w = (int)(rdClassSpeed[k] * SL_STEP / SL_SLOT_CELLS + 0.5f); if (w > v) v = w; }
    return v;
}
static int SlVmax(const SlotCar* c) {    // top speed, slots per step
    int v = SlSegV(c->route[c->ri]);
    if (c->temper == SL_SPEEDER) v++;
    else if (c->temper == SL_SLOW && v > 1) v--;
    return v;
}
static int SlBrakeSlots(void);
static int SlBrakeDist(int v, int D);
static float SlDawdle(const SlotCar* c) { return c->temper == SL_SPEEDER ? 0.05f : (c->temper == SL_SLOW ? 0.35f : SL_DAWDLE); }
static int SlGapMargin(const SlotCar* c) { return c->temper == SL_SPEEDER ? 1 : (c->temper == SL_SLOW ? -2 : 0); }   // slots of a gap it can do without
// Is a car `back` free slots behind, at speed vBehind, too close for c to pull in front of it?
// (standing: never; moving: it must be able to stop, give or take c's character)
static bool SlTooClose(int vBehind, int back, const SlotCar* c) {
    return vBehind > 0 && SlBrakeDist(vBehind, SlBrakeSlots()) > back + SlGapMargin(c);
}
static float slSlotCells(void) { return SL_SLOT_CELLS * CELL_SIZE; }   // px per slot (about: slots stretch per segment)
static float SlS(int seg, int k) { return (k + 0.5f) * rdSegs[seg].len / slSegN[seg]; }

static void SlBuildBuckets(void) {
    float B = SL_BUCKET * CELL_SIZE;
    slBW = gridWidth / SL_BUCKET + 2;
    slBH = gridHeight / SL_BUCKET + 2;
    slBHead = realloc(slBHead, sizeof(int) * (size_t)(slBW * slBH + 1));
    memset(slBHead, 0, sizeof(int) * (size_t)(slBW * slBH + 1));
    for (int s = 0; s < rdSegCount; s++)
        for (int k = 0; k < slSegN[s]; k++) {
            int idx = slSeg0[s] + k;
            RoadPointAt(s, (k + 0.5f) * rdSegs[s].len / slSegN[s], &slCX[idx], &slCY[idx], &slCH[idx]);
            slHalf[idx] = 0.5f * rdSegs[s].len / slSegN[s];
            slCZ[idx] = RoadZAt(s, (k + 0.5f) * rdSegs[s].len / slSegN[s]);
            slSlotSeg[idx] = s;
            slPedStamp[idx] = -1;
            slYieldUntil[idx] = -1;
            int bx = (int)(slCX[idx] / B), by = (int)(slCY[idx] / B);
            if (bx < 0) bx = 0;
            if (by < 0) by = 0;
            if (bx >= slBW) bx = slBW - 1;
            if (by >= slBH) by = slBH - 1;
            slBHead[by * slBW + bx + 1]++;
        }
    for (int b = 1; b <= slBW * slBH; b++) slBHead[b] += slBHead[b - 1];
    int* fill = calloc((size_t)(slBW * slBH), sizeof(int));
    for (int idx = 0; idx < slTotal; idx++) {
        int bx = (int)(slCX[idx] / B), by = (int)(slCY[idx] / B);
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;
        if (bx >= slBW) bx = slBW - 1;
        if (by >= slBH) by = slBH - 1;
        slBList[slBHead[by * slBW + bx] + fill[by * slBW + bx]++] = idx;
    }
    free(fill);
}
// Squared distance from (x, y) to the stretch slot idx covers (a car glides along all of it)
static float SlSlotDist2(int idx, float x, float y) {
    float dx = x - slCX[idx], dy = y - slCY[idx];
    float c = cosf(slCH[idx]), sn = sinf(slCH[idx]);
    float u = dx * c + dy * sn, v = -dx * sn + dy * c;
    if (u > slHalf[idx]) u -= slHalf[idx]; else if (u < -slHalf[idx]) u += slHalf[idx]; else u = 0;
    return u * u + v * v;
}
#define SL_SPAN_REACH (3.0f * CELL_SIZE)
// In the road (not on the pavement: the pavement has a floor) at (x, y) px on level z?
static bool SlRoadAt(float x, float y, int z) {
    int cx = (int)(x / CELL_SIZE), cy = (int)(y / CELL_SIZE);
    if (cx < 0 || cy < 0 || cx >= gridWidth || cy >= gridHeight || z < 0 || z >= gridDepth) return false;
    return !HAS_FLOOR(cx, cy, z);
}   // longest half slot: bucket searches reach this much further

// Is slot idx taken: a car in it, or a pedestrian (this step)?
static bool SlFree(int idx) { return slSlotCar[idx] < 0 && slPedStamp[idx] != slStep && slYieldUntil[idx] < slStep; }
static bool SlFreeFor(int idx, int car) {   // free, or held open for this car
    return slSlotCar[idx] < 0 && slPedStamp[idx] != slStep && (slYieldUntil[idx] < slStep || slYieldCar[idx] == car);
}
static int SlCarV(int idx) { int o = slSlotCar[idx]; return o >= 0 ? slotCars[o].v : 0; }   // (a pedestrian / held slot: standing)

// --- Kerb spots (carSlotKerbs): beside every second slot of the right-hand lane
static void SlKerbPoint(int lane, int k, float* x, float* y, float* h) {
    RoadPointAt(lane, SlS(lane, k), x, y, h);
    *x -= sinf(*h) * RD_BAY_OFF * CELL_SIZE;   // right of travel (as the bays)
    *y += cosf(*h) * RD_BAY_OFF * CELL_SIZE;
}
static void SlBuildKerbs(void) {
    memset(slKerbOk, 0, (size_t)slTotal);
    if (!carSlotKerbs) return;
    // every lane / curve point in buckets of 4 cells: a spot must be 3.2 cells from all but its own lane
    float B = 4.0f * CELL_SIZE, lim = 3.2f * CELL_SIZE;
    int bw = 1, bh = 1;
    for (int s = 0; s < rdSegCount; s++) {
        if (rdSegs[s].kind != RD_LANE && rdSegs[s].kind != RD_CONN) continue;
        int x = (int)(rdSegs[s].bx1 / B) + 2, y = (int)(rdSegs[s].by1 / B) + 2;
        if (x > bw) bw = x;
        if (y > bh) bh = y;
    }
    int* head = calloc((size_t)(bw * bh + 1), sizeof(int));
    int np = 0;
    for (int pass = 0; pass < 2; pass++) {
        static int* pts = NULL; static int ptsCap = 0;
        if (pass == 1) {
            for (int b = 1; b <= bw * bh; b++) head[b] += head[b - 1];
            if (np > ptsCap) { ptsCap = np; pts = realloc(pts, sizeof(int) * ptsCap); }
        }
        int* fill = pass == 1 ? calloc((size_t)(bw * bh), sizeof(int)) : NULL;
        for (int s = 0; s < rdSegCount; s++) {
            if (rdSegs[s].kind != RD_LANE && rdSegs[s].kind != RD_CONN) continue;
            for (int p = 0; p < rdSegs[s].n; p++) {
                int q = rdSegs[s].p0 + p;
                int bx = (int)(rdX[q] / B), by = (int)(rdY[q] / B);
                if (bx < 0 || by < 0 || bx >= bw || by >= bh) continue;
                if (pass == 0) { head[by * bw + bx + 1]++; np++; }
                else pts[head[by * bw + bx] + fill[by * bw + bx]++] = q;
            }
        }
        if (pass == 1) {
            free(fill);
            // the segment of each point: points are stored per segment in order
            static int* ptSeg = NULL; static int ptSegCap = 0;
            int total = 0;
            for (int s = 0; s < rdSegCount; s++) if (rdSegs[s].p0 + rdSegs[s].n > total) total = rdSegs[s].p0 + rdSegs[s].n;
            if (total > ptSegCap) { ptSegCap = total; ptSeg = realloc(ptSeg, sizeof(int) * ptSegCap); }
            for (int s = 0; s < rdSegCount; s++) for (int p = 0; p < rdSegs[s].n; p++) ptSeg[rdSegs[s].p0 + p] = s;
            for (int L = 0; L < rdSegCount; L++) {
                const RdSeg* S = &rdSegs[L];
                if (S->kind != RD_LANE || S->laneIdx != 0 || S->priority || rdEdges[S->edge].arc || rdEdges[S->edge].lifted) continue;
                for (int k = 2; k <= slSegN[L] - 3; k++) {
                    float x, y, h;
                    SlKerbPoint(L, k, &x, &y, &h);
                    int bx = (int)(x / B), by = (int)(y / B);
                    bool clear = true;
                    for (int yy = by - 1; yy <= by + 1 && clear; yy++)
                        for (int xx = bx - 1; xx <= bx + 1 && clear; xx++) {
                            if (xx < 0 || yy < 0 || xx >= bw || yy >= bh) continue;
                            for (int e = head[yy * bw + xx]; e < head[yy * bw + xx + 1] && clear; e++) {
                                int q = pts[e];
                                if (ptSeg[q] == L) continue;
                                float dx = rdX[q] - x, dy = rdY[q] - y;
                                if (dx * dx + dy * dy < lim * lim) clear = false;
                            }
                        }
                    if (clear) slKerbOk[SlIdx(L, k)] = (k % 2 == 0) ? 1 : 2;
                }
            }
        }
    }
    free(head);
}

void SlotsCarveKerbs(int z) {
    for (int L = 0; L < rdSegCount; L++) {
        if (rdSegs[L].kind != RD_LANE) continue;
        for (int k = 0; k < slSegN[L]; k++) {
            if (!slKerbOk[SlIdx(L, k)]) continue;
            float x, y, h;
            SlKerbPoint(L, k, &x, &y, &h);
            RoadCarveDisc(x / CELL_SIZE, y / CELL_SIZE, 2.0f, z);
        }
    }
}

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
    if (slTotal > slSlotCap) {
        slSlotCap = slTotal;
        slSlotCar = realloc(slSlotCar, sizeof(int) * slSlotCap);
        slKerbCar = realloc(slKerbCar, sizeof(int) * slSlotCap);
        slKerbOk = realloc(slKerbOk, (size_t)slSlotCap);
        slCX = realloc(slCX, sizeof(float) * slSlotCap);
        slCY = realloc(slCY, sizeof(float) * slSlotCap);
        slCH = realloc(slCH, sizeof(float) * slSlotCap);
        slCZ = realloc(slCZ, sizeof(float) * slSlotCap);
        slHalf = realloc(slHalf, sizeof(float) * slSlotCap);
        slSlotSeg = realloc(slSlotSeg, sizeof(int) * slSlotCap);
        slPedStamp = realloc(slPedStamp, sizeof(int) * slSlotCap);
        slYieldUntil = realloc(slYieldUntil, sizeof(int) * slSlotCap);
        slYieldCar = realloc(slYieldCar, sizeof(int) * slSlotCap);
        slBList = realloc(slBList, sizeof(int) * slSlotCap);
    }
    SlBuildBuckets();
    SlBuildKerbs();
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
    for (int i = 0; i < slTotal; i++) slSlotCar[i] = slKerbCar[i] = -1;
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

int SlotsSpawn(int count, unsigned int seed, float cabShare, float parkShare) {
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
    // some parked at the kerb first (they leave over the first minute)
    int parked = (int)(count * (1.0f - cabShare) * parkShare);
    for (int tries = 0; tries < parked * 20 && slotCarCount < parked && lanes > 0; tries++) {
        int lane = RoadLaneAt((int)((SlRand() * 32768u + SlRand()) % (unsigned)lanes));
        int k = (int)(SlRand() % slSegN[lane]);
        int idx = SlIdx(lane, k);
        if (slKerbOk[idx] != 1 || slKerbCar[idx] >= 0) continue;
        SlotCar* c = &slotCars[slotCarCount];
        memset(c, 0, offsetof(SlotCar, route));
        c->route[0] = (unsigned short)lane;
        c->routeLen = 1;
        c->slot = c->prevSlot = k;
        c->destSlot = slSegN[lane] - 1;
        c->needRoute = true;
        c->kerb = true;
        c->stopSteps = 1 + (int)(SlRand() % 60);
        c->lcStep = c->kerbStep = -1;
        c->owner = -1;
        if (carSlotTempers) { unsigned r = SlRand() % 100; c->temper = r < 20 ? SL_SPEEDER : (r < 40 ? SL_SLOW : SL_NORMAL); }
        slKerbCar[idx] = slotCarCount;
        slGrantStep[slotCarCount] = -1;
        slotCarCount++;
    }
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
        c->lcStep = c->kerbStep = -1;
        if (carSlotTempers) {   // 20% speeders, 20% slow; cabs mostly speeders (it's New York)
            unsigned r = SlRand() % 100;
            c->temper = c->cab ? (r < 60 ? SL_SPEEDER : SL_NORMAL) : (r < 20 ? SL_SPEEDER : (r < 40 ? SL_SLOW : SL_NORMAL));
        }
        c->owner = -1;
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
        // a cab's fare / a car that will park: stop beside a kerb spot (just before it)
        c->parkNext = false;
        if (carSlotKerbs && (c->cab || (carCityParking && SlRand() < (unsigned)(0.4f * 32768)))) {
            for (int t = 0; t < 4; t++) {
                int k = 2 + 2 * (int)(SlRand() % (slSegN[dl] / 2 + 1));
                if (k < slSegN[dl] && slKerbOk[SlIdx(dl, k)] == 1) { ds = k - 1; c->parkNext = true; break; }
            }
        }
        int n;
        if (dl == cur && ds > c->slot + 1) { r[0] = (unsigned short)cur; n = 1; }
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
        if (!SlFree(SlIdx(seg, k))) break;
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
    int top = SlSegV(conn);   // (the smaller road's limit)
    return v > top ? top : v;
}
// Will this curve probably be granted when we get there? (green or no light, no crossing curve
// in use, room past it) - then drive at it at its curve speed, else brake for its stop line
static bool SlPedOn(int seg) {   // a pedestrian on (or about to be on) segment seg
    for (int k = 0; k < slSegN[seg]; k++) if (slPedStamp[SlIdx(seg, k)] == slStep) return true;
    return false;
}

// Is a sibling of curve conn (one from the same lane) busy where it starts together with conn?
static bool SlSiblingBusy(int conn) {
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    for (int q = 0; q < N->nConn; q++) {
        int sb = N->conn[q];
        if (sb == conn || rdSegs[sb].from != C->from) continue;
        if (slConnStamp[sb] == slStep) return true;   // someone was let onto it this step
        for (int k = 0; k < slShared[sb]; k++) if (!SlFree(SlIdx(sb, k))) return true;
    }
    return false;
}

static bool SlCurveLooksOpen(int conn, bool amberOk) {
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    int light = TrafficLightAt(C->node, C->from);
    if (light == 0 || (light == 1 && !amberOk)) return false;
    if (SlSiblingBusy(conn)) return false;
    if (SlPedOn(conn)) return false;
    int freeRun = 0;
    while (freeRun < slSegN[C->to] && SlFree(SlIdx(C->to, freeRun))) freeRun++;
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
    int D = SlBrakeSlots(), vmax = SlVmax(c), best = vmax;
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
                bool open = slGrantStep[i] == slStep || SlCurveLooksOpen(next, c->temper == SL_SPEEDER);
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
        if (o >= 0 || slPedStamp[SlIdx(seg, k)] == slStep ||
            (slYieldUntil[SlIdx(seg, k)] >= slStep && slYieldCar[SlIdx(seg, k)] != i)) {   // a car (a pedestrian, a gap held for a merger) ahead: Gipps
            int vo = o < 0 || slotCars[o].stopSteps > 0 ? 0 : slotCars[o].v;
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

static int slKGhostIdx[64], slKGhostCar[64], slKGhostN = 0;   // kerb spots held during a pull-out

// --- Kerb moves: pull in at the end of a trip, pull out when the next trip is planned
static void SlKerbMoves(void) {
    int D = SlBrakeSlots();
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        int L = c->route[c->ri], k = c->slot, N = slSegN[L];
        if (c->wantKerb) {
            c->wantKerb = false;
            int kk = -1;
            if (rdSegs[L].kind == RD_LANE) {
                if (k + 1 < N && slKerbOk[SlIdx(L, k + 1)] == 1 && slKerbCar[SlIdx(L, k + 1)] < 0 && SlFree(SlIdx(L, k + 1))) kk = k + 1;
                else if (slKerbOk[SlIdx(L, k)] == 1 && slKerbCar[SlIdx(L, k)] < 0) kk = k;
            }
            if (kk < 0) {   // taken: a cab stands in the lane (the old way), a parker drives on
                slKerbFull++;
                if (c->owner >= 0) { if (!SlDriveTo(i)) SlQueueRoute(i); continue; }   // a driver: the next free spot near the goal
                if (c->cab) c->stopSteps = 3 + (int)(SlRand() % 6);
                else SlQueueRoute(i);
                continue;
            }
            SlGhost(SlIdx(L, k), i);                        // half in the lane during the step
            if (kk == k + 1) SlGhost(SlIdx(L, k + 1), i);   // (our front passes there)
            slKerbCar[SlIdx(L, kk)] = i;
            c->kerb = true; c->kerbIn = true; c->kerbStep = slStep; c->kerbFrom = k;
            c->prevRi = c->ri;
            c->slot = c->prevSlot = kk;
            c->v = 0;
            c->stopSteps = c->cab ? 3 + (int)(SlRand() % 6) : 20 + (int)(SlRand() % 70);
            if (c->owner >= 0) { c->stopSteps = 1 << 30; c->needRoute = false; }   // parked until its owner comes back
            if (c->cab) slFareStops++; else slParks++;
            continue;
        }
        if (c->kerb && c->ownerIn && c->kerbIn && c->kerbStep > c->boardStep && c->kerbStep != slStep) { SlDriverOut(i); continue; }   // parked again: the driver gets out
        if (!c->kerb || c->kerbStep == slStep || c->stopSteps > 0 || c->needRoute) continue;
        // pull out: diagonally into the lane, one slot on; passing beside lane slot k
        if (k + 1 >= N || !SlFree(SlIdx(L, k)) || !SlFree(SlIdx(L, k + 1))) { slPullOutWaits++; continue; }
        int back = 0, kk = k - 1;
        while (kk >= 0 && SlFree(SlIdx(L, kk)) && back < 16) { back++; kk--; }
        if (kk >= 0 && back < 16 && SlTooClose(SlCarV(SlIdx(L, kk)), back, c)) { slPullOutWaits++; continue; }
        if (kk < 0 && back < SlBrakeDist(SlVmaxAny() + 1, D)) {   // near the lane's start: nobody coming off the junction
            const RdNode* Nd = &rdNodes[rdSegs[L].from];
            bool coming = false;
            for (int q = 0; q < Nd->nConn && !coming; q++)
                if (rdSegs[Nd->conn[q]].to == L && (slSegCount[Nd->conn[q]] > 0 || slConnStamp[Nd->conn[q]] == slStep)) coming = true;
            if (coming) { slPullOutWaits++; continue; }
        }
        SlGhost(SlIdx(L, k), i);
        slSlotCar[SlIdx(L, k + 1)] = i;
        if (slKGhostN < 64) { slKGhostIdx[slKGhostN] = SlIdx(L, k); slKGhostCar[slKGhostN] = i; slKGhostN++; }   // (the spot stays ours this step)
        else slKerbCar[SlIdx(L, k)] = -1;
        c->kerb = false; c->kerbIn = false; c->kerbStep = slStep; c->kerbFrom = k;
        c->prevRi = c->ri;
        c->slot = c->prevSlot = k + 1;
        c->v = 1;
        c->lcCool = 3;
    }
}

// --- Lane changes (two-lane roads): a diagonal move, one slot forward into the lane beside
static int SlParallelSlot(int from, int k, int to) {   // the slot on lane `to` beside slot k of `from`
    int kk = (int)((k + 0.5f) * slSegN[to] / slSegN[from]);
    return kk >= slSegN[to] ? slSegN[to] - 1 : kk;
}
static int SlFreeRun(int seg, int k, int max) {          // free slots ahead of slot k on one lane
    int n = 0;
    while (n < max && k + 1 + n < slSegN[seg] && SlFree(SlIdx(seg, k + 1 + n))) n++;
    return n;
}
static void SlLaneChanges(void) {
    int D = SlBrakeSlots(), changes = 0;
    for (int i = 0; i < slotCarCount && changes < 40; i++) {   // (each change plans a route: at most 40 a step)
        SlotCar* c = &slotCars[i];
        if (c->kerb || c->kerbStep == slStep) continue;
        // A lane change the route needs (into the lane its turn is allowed from): as soon as the
        // slots beside allow it, and carry on with the route from there
        if (c->stopSteps == 0 && c->ri + 1 < c->routeLen && rdSegs[c->route[c->ri]].kind == RD_LANE &&
            rdSegs[c->route[c->ri]].parallel == c->route[c->ri + 1]) {
            int A = c->route[c->ri], B = c->route[c->ri + 1], k = c->slot;
            int kb = SlParallelSlot(A, k, B), kt = kb + 1 < slSegN[B] ? kb + 1 : kb;
            // standing or crawling: straight sideways into the slot beside (the zipper, in a queue);
            // else diagonally one slot on (and nobody right ahead in our lane: half-way, our front
            // corner would clip it)
            bool side = c->v <= 1 && SlFreeFor(SlIdx(B, kb), i);
            if (side) kt = kb;
            bool gap = SlFreeFor(SlIdx(B, kt), i) && SlFreeFor(SlIdx(B, kb), i) && (side || k + 1 >= slSegN[A] || SlFree(SlIdx(A, k + 1)));
            int back = 0, kk = kb - 1;
            while (gap && kk >= 0 && SlFree(SlIdx(B, kk)) && back < 16) { back++; kk--; }
            if (gap && kk >= 0 && back < 16 && SlTooClose(SlCarV(SlIdx(B, kk)), back, c)) gap = false;
            if (getenv("SL_YIELDLOG") && slYieldUntil[SlIdx(B, kb)] >= slStep && slYieldCar[SlIdx(B, kb)] == i)
                printf("step %d: car %d merge check with held gap: gap %d side %d kb %d kt %d free %d %d, A ahead free %d, back %d\n", slStep, i, gap, side, kb, kt,
                       SlFreeFor(SlIdx(B, kt), i), SlFreeFor(SlIdx(B, kb), i), k + 1 >= slSegN[A] || SlFree(SlIdx(A, k + 1)), back);
            if (!gap && carSlotTempers) {
                // Courtesy (the zipper): the first car behind the gap, if not a speeder and able to
                // stop short of it, holds it open for us (this step and the next)
                // (standing / crawling we only need the slot beside, kb)
                int top = c->v <= 1 ? kb : kt;
                int y = -1, dist = 0;
                for (int q = kb - 1; q >= 0 && q >= kb - 6; q--, dist++) { int o = slSlotCar[SlIdx(B, q)]; if (o >= 0 && o != i) { y = o; break; } }
                if (y >= 0 && slotCars[y].temper != SL_SPEEDER && slotCars[y].lcStep != slStep && SlBrakeDist(slotCars[y].v, D) <= dist &&
                    slSlotCar[SlIdx(B, top)] < 0 && slSlotCar[SlIdx(B, kb)] < 0) {
                    if (slYieldUntil[SlIdx(B, kb)] < slStep) slYields++;
                    if (getenv("SL_YIELDLOG")) printf("step %d: car %d (v %d, at %d/%d of %d) indicating; car %d (v %d, %d slots back) holds slot %d/%d of %d open\n", slStep, i, c->v, k, slSegN[A], A, y, slotCars[y].v, dist, kb, slSegN[B], B);
                    slYieldUntil[SlIdx(B, kb)] = slYieldUntil[SlIdx(B, top)] = slStep + 1;
                    slYieldCar[SlIdx(B, kb)] = slYieldCar[SlIdx(B, top)] = i;
                }
            }
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
            if (slYieldUntil[SlIdx(B, kt)] >= slStep && slYieldCar[SlIdx(B, kt)] == i) { slYieldMerges++; slYieldUntil[SlIdx(B, kt)] = slYieldUntil[SlIdx(B, kb)] = -1; }
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
        if (!SlFree(SlIdx(B, kb)) || !SlFree(SlIdx(B, kt))) continue;
        if (k + 1 < slSegN[A] && !SlFree(SlIdx(A, k + 1))) continue;   // right ahead in our lane: our front corner would clip it
        // why change: overtake a slow / standing car just ahead, or back to the right-hand lane
        int aheadA = SlFreeRun(A, k, 8), aheadB = SlFreeRun(B, kt, 8);
        bool keepRight = rdSegs[A].laneIdx == 1;
        bool overtake = false;
        int near = c->temper == SL_SPEEDER ? 3 : 1;   // speeders overtake anything slower a few slots ahead; slow drivers never
        if (!keepRight && c->temper != SL_SLOW && aheadA <= near && k + aheadA + 1 < slSegN[A]) {
            int o = slSlotCar[SlIdx(A, k + aheadA + 1)];
            int slow = c->temper == SL_SPEEDER ? SlVmax(c) - 1 : 1;
            if (o >= 0 && (slotCars[o].v <= slow || slotCars[o].stopSteps > 0) && aheadB >= aheadA + 2) overtake = true;
        }
        // keep right only onto clearly free road (moving back the moment it had as much room made
        // cars swap lanes all the time and everyone behind brake: 16% fewer trips)
        int clear = slSegN[B] - kt - 1 < 4 ? slSegN[B] - kt - 1 : 4;   // (a short lane: free to its end)
        if (!overtake && !(keepRight && aheadB >= clear && aheadB >= aheadA)) continue;
        // the first car behind in lane B must be able to stop in time
        int back = 0, kk = kb - 1;
        while (kk >= 0 && SlFree(SlIdx(B, kk)) && back < 16) { back++; kk--; }
        if (kk >= 0 && back < 16) {
            if (SlTooClose(SlCarV(SlIdx(B, kk)), back, c)) continue;
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
    if (c->kerb || rdSegs[seg].kind != RD_LANE || c->ri + 1 >= c->routeLen) return -1;
    // only when we can get there this step (a grant we can't use blocks crossing traffic)
    int vNext = c->v + 1 > SlVmax(c) ? SlVmax(c) : c->v + 1;
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
    if (rdClasses) {                                     // then the bigger road's
        int ka = RoadSegClass(ca->route[ca->ri]), kb = RoadSegClass(cb->route[cb->ri]);
        if (ka != kb) return kb - ka;
    }
    return cb->waitSteps - ca->waitSteps;                // then the longest waiting
}

// No lights, classes: someone on a bigger road coming up to this junction (within its stopping
// distance + 2 slots of the line) for a curve that crosses ours goes first
static bool SlYieldToBigger(int i, int conn) {
    if (!rdClasses) return false;
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    int mine = RoadSegClass(C->from), D = SlBrakeSlots();
    for (int e = 0; e < N->nEdge; e++) {
        const RdEdge* E = &rdEdges[N->edge[e]];
        if (E->cls <= mine) continue;
        int d = E->b == C->node ? 0 : 1;
        int ins[2] = {E->lane[d], E->lane2[d]};
        for (int q = 0; q < 2; q++) {
            int in = ins[q];
            if (in < 0 || rdSegs[in].to != C->node) continue;
            int reach = SlBrakeDist(SlSegV(in), D) + 2;
            for (int k = slSegN[in] - 1, back = 0; k >= 0 && back < reach; k--, back++) {
                int o = slSlotCar[SlIdx(in, k)];
                if (o < 0 || o == i) continue;
                const SlotCar* oc = &slotCars[o];
                if (oc->kerb || oc->route[oc->ri] != in || oc->ri + 1 >= oc->routeLen) continue;
                int oconn = oc->route[oc->ri + 1];
                if (rdSegs[oconn].kind != RD_CONN || rdSegs[oconn].node != C->node) continue;
                if (oconn == conn || N->conflict[C->ci][rdSegs[oconn].ci]) return true;
            }
        }
    }
    return false;
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
        if (light == 0 || (light == 1 && c->temper != SL_SPEEDER)) continue;   // red / amber (speeders go on amber)
        if (slExitStamp[C->to] == slStep) continue;                       // someone else enters that lane now
        if (SlSiblingBusy(conn)) continue;                                // a car from our lane still where the curves start together
        if (SlPedOn(conn)) continue;                                      // someone walking across the curve
        // room past the junction for us and every car already on this curve: else a car can
        // end up waiting inside the junction, blocking the crossing paths (that gridlocked)
        int freeRun = 0;
        while (freeRun < slSegN[C->to] && SlFree(SlIdx(C->to, freeRun))) freeRun++;
        if (freeRun <= slSegCount[conn]) { slotCars[i].noRoomSteps++; continue; }
        slotCars[i].noRoomSteps = 0;
        bool clash = false;
        for (int j = 0; j < N->nConn && !clash; j++) {
            int other = N->conn[j];
            if (other == conn || !N->conflict[C->ci][j]) continue;
            if (slSegCount[other] > 0 || slConnStamp[other] == slStep) clash = true;
        }
        if (clash) continue;
        if (light < 0 && SlYieldToBigger(i, conn)) { slClassYields++; continue; }
        // entering a roundabout: room on the ring, and a gap in the ring traffic arriving here
        if (!rdSegs[C->from].priority && rdSegs[C->to].priority && N->ringOf >= 0) {
            int r = N->ringOf;
            if (slRingCount[r] >= (slRingLanes[r] > 1 ? slRingLanes[r] : 1)) continue;
            bool gap = true;
            for (int e = 0; e < N->nEdge && gap; e++) {
                const RdEdge* E = &rdEdges[N->edge[e]];
                int in = E->b == C->node ? E->lane[0] : E->lane[1];
                if (in < 0 || !rdSegs[in].priority) continue;
                for (int k = slSegN[in] - 2; k < slSegN[in]; k++) if (k >= 0 && !SlFree(SlIdx(in, k))) gap = false;
            }
            if (!gap) continue;
            slRingCount[r]++;
        }
        slGrantStep[i] = slStep;
        slConnStamp[conn] = slStep;
        slExitStamp[C->to] = slStep;
    }
}

// Slots within 1.8 cells of the stretch a pedestrian walks this step (from where it is, along
// its last step's velocity, 1.2 s on): a car may not be there
static float* slPedLX = NULL;

static float* slPedLY = NULL;
static void SlStampPedestrians(void) {
    if (moverCount == 0) return;
    static int cap = 0;
    if (moverCount > cap) {
        slPedLX = realloc(slPedLX, sizeof(float) * moverCount);
        slPedLY = realloc(slPedLY, sizeof(float) * moverCount);
        for (int i = cap; i < moverCount; i++) slPedLX[i] = -1;
        cap = moverCount;
    }
    float CS = CELL_SIZE, B = SL_BUCKET * CS, R = 1.5f * CS;   // (car half width + a pedestrian + a margin, from the slot's stretch)
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        if (!m->active || m->transportState == TRANSPORT_RIDING) { slPedLX[i] = -1; continue; }
        float px = m->x, py = m->y, qx = px, qy = py;
        bool held = i < slPedHeldCap && slPedHeldAt[i] > gameTime - 0.5;
        bool going = i < slPedHeldCap && slPedGoStep[i] == slStep;
        if (!going && !SlRoadAt(px, py, (int)m->z)) { slPedLX[i] = px; slPedLY[i] = py; continue; }   // on the pavement: no car comes there
        if (i < slPedHeldCap && slPedGoStep[i] == slStep) {   // steps off the kerb now: its way across
            qx = px + slPedDX[i] * 3.0f * CS; qy = py + slPedDY[i] * 3.0f * CS;
        } else if (slPedLX[i] >= 0 && !held) {
            float vx = (px - slPedLX[i]) / SL_STEP, vy = (py - slPedLY[i]) / SL_STEP;
            if (vx * vx + vy * vy < 25 * CS * CS) { qx = px + vx * 1.2f; qy = py + vy * 1.2f; }
        }
        slPedLX[i] = px; slPedLY[i] = py;
        float ex = qx - px, ey = qy - py, el = ex * ex + ey * ey;
        int bx0 = (int)((fminf(px, qx) - R - SL_SPAN_REACH) / B), bx1 = (int)((fmaxf(px, qx) + R + SL_SPAN_REACH) / B);
        int by0 = (int)((fminf(py, qy) - R - SL_SPAN_REACH) / B), by1 = (int)((fmaxf(py, qy) + R + SL_SPAN_REACH) / B);
        for (int by = by0 < 0 ? 0 : by0; by <= by1 && by < slBH; by++)
            for (int bx = bx0 < 0 ? 0 : bx0; bx <= bx1 && bx < slBW; bx++)
                for (int e = slBHead[by * slBW + bx]; e < slBHead[by * slBW + bx + 1]; e++) {
                    int idx = slBList[e];
                    if (fabsf(slCZ[idx] - m->z) > 0.6f) continue;   // (a road above or below)
                    int ns = el > 0 ? 1 + (int)(sqrtf(el) / (0.5f * CS)) : 0;   // points every half cell along the walk
                    for (int t = 0; t <= ns; t++) {
                        float f = ns ? (float)t / ns : 0;
                        if (SlSlotDist2(idx, px + ex * f, py + ey * f) < R * R) { if (slPedStamp[idx] != slStep) slPedBlocks++; slPedStamp[idx] = slStep; break; }
                    }
                }
    }
}

static void SlRunStep(void) {
    slStep++;
    // occupancy per segment, roundabout counts
    for (int s = 0; s < rdSegCount; s++) slSegCount[s] = 0;
    if (slRingCount) memset(slRingCount, 0, sizeof(short) * rdNodeCount);
    for (int i = 0; i < slotCarCount; i++) {
        const SlotCar* c = &slotCars[i];
        if (c->kerb) continue;
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
            if (slotCars[i].v == 0 && !slotCars[i].kerb) rdSegExtra[slotCars[i].route[slotCars[i].ri]] += 8.0f * CELL_SIZE;
    }
    SlStampPedestrians();
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
    SlKerbMoves();
    if (carSlotLaneChange) SlLaneChanges();
    // speeds (all from the same occupancy), then moves
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        if (c->kerb) { if (c->stopSteps > 0 && --c->stopSteps == 0) SlQueueRoute(i); slStepV[i] = -1; continue; }   // at the kerb
        if (c->kerbStep == slStep) { slStepV[i] = -1; continue; }   // pulled out this step (moved already)
        if (c->stopSteps > 0) { c->stopSteps--; slStepV[i] = 0; if (c->stopSteps == 0) SlQueueRoute(i); continue; }   // a cab at a fare
        if (c->lcStep == slStep) { slStepV[i] = -1; continue; }   // changed lanes this step (it keeps its speed)
        int v = c->v + 1 > SlVmax(c) ? SlVmax(c) : c->v + 1;
        if (carSlotPhysical) { int vs = SlSafeSpeed(i); if (v > vs) v = vs; }   // brake in time, slow for curves
        int f = SlFreeAhead(i, v);
        if (v > f) v = f;
        if (v > 0 && SlRand() < (unsigned)(SlDawdle(c) * 32768)) v--;
        if (v < c->v - SlBrakeSlots()) slHardStops++;
        slStepV[i] = v;
    }
    for (int i = 0; i < slotCarCount; i++) {
        SlotCar* c = &slotCars[i];
        if (slStepV[i] < 0) continue;   // changed lanes this step: already moved
        c->prevRi = c->ri;
        c->prevSlot = c->slot;
        int v = slStepV[i];
        c->vBefore = c->v;
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
            if (c->parkNext) { c->parkNext = false; c->wantKerb = true; }   // pulls in next step
            else if (c->cab) c->stopSteps = 3 + (int)(SlRand() % 6);   // a fare: stand a few steps, then a new trip
            else SlQueueRoute(i);
        }
    }
    for (int g = 0; g < slGhostN; g++) if (slSlotCar[slGhostIdx[g]] == slGhostCar[g]) slSlotCar[slGhostIdx[g]] = -1;
    slGhostN = 0;
    for (int g = 0; g < slKGhostN; g++) if (slKerbCar[slKGhostIdx[g]] == slKGhostCar[g]) slKerbCar[slKGhostIdx[g]] = -1;
    slKGhostN = 0;
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
        if (slotCars[i].stopSteps > 0 || slotCars[i].kerb) vT = 0;
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
    float limit = 2.0f * (SlVmaxAny() + 1) * SL_SLOT_CELLS * CELL_SIZE / SL_STEP * dt + 2.0f;
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
    static float* pz = NULL; static int zcap = 0;
    if (slotCarCount > zcap) { zcap = slotCarCount; pz = realloc(pz, sizeof(float) * zcap); }
    for (int i = 0; i < slotCarCount; i++) { float st; SlotCarDrawPose(i, &px[i], &py[i], &ph[i], &st); pz[i] = SlotCarZ(i); }
    int pairs = 0;
    float near = 3.0f * CELL_SIZE;
    for (int i = 0; i < slotCarCount; i++)
        for (int j = i + 1; j < slotCarCount; j++) {
            float dx = px[i] - px[j], dy = py[i] - py[j];
            if (dx * dx + dy * dy > near * near) continue;
            if (fabsf(pz[i] - pz[j]) > 0.6f) continue;   // one over the other
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
    for (int mi = 0; mi < moverCount; mi++) {
        if (!movers[mi].active || movers[mi].transportState == TRANSPORT_RIDING) continue;
        for (int i = 0; i < slotCarCount; i++) {
            float dx = px[i] - movers[mi].x, dy = py[i] - movers[mi].y;
            if (dx * dx + dy * dy > near * near) continue;
            if (fabsf(pz[i] - movers[mi].z) > 0.6f) continue;
            if (SlInCar(i, movers[mi].x, movers[mi].y, -2.0f)) {
                slPedHits++;
                static int toldP = 0;
                if (toldP++ < 14) printf("pedestrian %d at (%.1f,%.1f) inside car %d at (%.1f,%.1f) (v %d, seg kind %d slot %d/%d, kerb %d, lc %d, ped held %.1f s ago, path %d/%d)\n", mi,
                                        movers[mi].x / CELL_SIZE, movers[mi].y / CELL_SIZE, i, px[i] / CELL_SIZE, py[i] / CELL_SIZE, slotCars[i].v,
                                        rdSegs[slotCars[i].route[slotCars[i].ri]].kind, slotCars[i].slot, slSegN[slotCars[i].route[slotCars[i].ri]], slotCars[i].kerb,
                                        slotCars[i].lcStep == slStep, mi < slPedHeldCap ? gameTime - slPedHeldAt[mi] : -1.0, movers[mi].pathIndex, movers[mi].pathLength);
            }
        }
    }
    slPedHitSamples++;
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
    for (int i = 0; i < slotCarCount; i++)
        if (slotCars[i].ownerIn && slotCars[i].owner >= 0 && slotCars[i].owner < moverCount) {
            float x, y, h, st;
            SlotCarDrawPose(i, &x, &y, &h, &st);
            movers[slotCars[i].owner].x = x;
            movers[slotCars[i].owner].y = y;
            movers[slotCars[i].owner].transportState = TRANSPORT_RIDING;   // (the goal picker's path planning resets it: MaybeTakeTrain)
            movers[slotCars[i].owner].needsRepath = false;                 // (the repath pass doesn't skip riders)
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
    if (c->kerbStep == slStep || c->kerb) {   // at the kerb, or pulling in / out this step
        float OFF = RD_BAY_OFF * CELL_SIZE;
        float s0 = c->kerbStep == slStep ? SlS(seg, c->kerbFrom) : sCur;
        float o0 = c->kerb ? OFF : 0, o1 = c->kerb ? OFF : 0;
        if (c->kerbStep == slStep) { o0 = c->kerbIn ? 0 : OFF; o1 = c->kerbIn ? OFF : 0; }
        float w = f * f * (3 - 2 * f);
        float s = s0 + (sCur - s0) * f, off = o0 + (o1 - o0) * w, h;
        RoadPointAt(seg, s, x, y, &h);
        *x -= sinf(h) * off;
        *y += cosf(h) * off;
        float ds = sCur - s0 > 1 ? sCur - s0 : slSlotCells();
        *heading = h + atan2f((o1 - o0) * 6 * f * (1 - f), ds);
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

// Indicators and brake lights, from what the car is about to do (its route, the kerb)
#define SL_SIGNAL_SLOTS 4   // indicate this many slots before a turn / the kerb
int SlotCarSignals(int i) {
    const SlotCar* c = &slotCars[i];
    int seg = c->route[c->ri], sig = 0;
    // braking: slower than last step, or standing in traffic (not parked)
    if (!c->kerb && (c->v < c->vBefore || c->v == 0)) sig |= SL_SIG_BRAKE;
    if (c->kerb) {
        if (c->cab && c->stopSteps > 0) return SL_SIG_LEFT | SL_SIG_RIGHT;          // a cab at a fare: hazards
        if (c->stopSteps <= 2 && c->kerbStep != slStep) return SL_SIG_LEFT;       // about to pull out
        if (c->kerbStep == slStep && c->kerbIn) return SL_SIG_RIGHT;              // just pulled in
        return 0;
    }
    if (c->kerbStep == slStep && !c->kerbIn) return sig | SL_SIG_LEFT;            // pulling out
    if (c->stopSteps > 0 && c->cab) return sig | SL_SIG_LEFT | SL_SIG_RIGHT;      // a cab at a fare in the lane
    // a lane change: under way, or one the route needs next
    if (c->lcStep == slStep) return sig | (rdSegs[seg].laneIdx == 1 ? SL_SIG_LEFT : SL_SIG_RIGHT);
    if (rdSegs[seg].kind == RD_CONN) {                                             // on a junction curve
        int t = RoadConnTurn(seg);
        return sig | (t < 0 ? SL_SIG_LEFT : t > 0 ? SL_SIG_RIGHT : 0);
    }
    if (c->ri + 1 < c->routeLen) {
        int next = c->route[c->ri + 1];
        if (rdSegs[seg].parallel == next) return sig | (rdSegs[next].laneIdx == 1 ? SL_SIG_LEFT : SL_SIG_RIGHT);
        if (rdSegs[next].kind == RD_CONN && slSegN[seg] - c->slot <= SL_SIGNAL_SLOTS) {
            int t = RoadConnTurn(next);
            return sig | (t < 0 ? SL_SIG_LEFT : t > 0 ? SL_SIG_RIGHT : 0);
        }
    }
    if (c->parkNext && c->ri == c->routeLen - 1 && c->destSlot - c->slot <= SL_SIGNAL_SLOTS) sig |= SL_SIG_RIGHT;   // pulling in soon
    return sig;
}

// --- Movers driving
float SlotsDriveSpeed(void) { return slDriveSpeed; }
void SlotsNoteChoice(bool drove, float tWalk, float tDrive) {
    if (drove) slChoseDrive++; else slChoseWalk++;
    slSumWalkT += tWalk; slSumDriveT += tDrive;
}
void SlotsSetOwner(int car, int mover) {
    SlotCar* c = &slotCars[car];
    c->owner = mover;
    c->ownerIn = false;
    if (c->kerb) { c->stopSteps = 1 << 30; c->needRoute = false; }
}
int SlotsCarOf(int mover) {
    for (int i = 0; i < slotCarCount; i++) if (slotCars[i].owner == mover) return i;
    return -1;
}
bool SlotsCarParked(int car) { return car >= 0 && slotCars[car].kerb && !slotCars[car].ownerIn && slotCars[car].kerbStep != slStep; }
bool SlotCarDriven(int car) { return slotCars[car].ownerIn; }

// Plan car's drive to the free kerb spot nearest its goal (one it can route to); false: none
static bool SlDriveTo(int car) {
    SlotCar* c = &slotCars[car];
    int cur = c->route[c->ri];
    if (rdSegs[cur].kind != RD_LANE) return false;
    float B = SL_BUCKET * CELL_SIZE;
    int gbx = (int)(c->goalX / B), gby = (int)(c->goalY / B);
    for (int ring = 0; ring <= 10; ring++) {   // buckets in growing squares round the goal
        int best = -1;
        float bestD = 1e30f;
        for (int by = gby - ring; by <= gby + ring; by++)
            for (int bx = gbx - ring; bx <= gbx + ring; bx++) {
                if (bx < 0 || by < 0 || bx >= slBW || by >= slBH) continue;
                if (ring > 0 && by != gby - ring && by != gby + ring && bx != gbx - ring && bx != gbx + ring) continue;
                for (int e = slBHead[by * slBW + bx]; e < slBHead[by * slBW + bx + 1]; e++) {
                    int idx = slBList[e];
                    if (slKerbOk[idx] != 1 || slKerbCar[idx] >= 0) continue;
                    if (idx == SlIdx(cur, c->slot) && c->kerb) continue;   // (not the spot we're leaving)
                    float dx = slCX[idx] - c->goalX, dy = slCY[idx] - c->goalY;
                    if (dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; best = idx; }
                }
            }
        if (best < 0) continue;
        int dl = slSlotSeg[best], k = best - slSeg0[dl];
        unsigned short r[SL_MAX_ROUTE];
        int n;
        if (dl == cur && k - 1 > c->slot + 1) { r[0] = (unsigned short)cur; n = 1; }
        else {
            slRoutes++;
            n = RoadRoute(cur, dl, r, SL_MAX_ROUTE - 8);
            if (n <= 0) { slRouteFails++; continue; }
        }
        SlSetRoute(c, r, n, k - 1);
        c->needRoute = false;
        c->parkNext = true;
        if (c->kerb) c->stopSteps = 0;   // pulls out as soon as there's a gap
        return true;
    }
    return false;
}

bool SlotsMoverBoard(int mi, int car) {
    if (!SlotsCarParked(car) || slotCars[car].owner != mi) return false;
    Mover* m = &movers[mi];
    SlotCar* c = &slotCars[car];
    c->goalX = (m->transportFinalGoal.x + 0.5f) * CELL_SIZE;
    c->goalY = (m->transportFinalGoal.y + 0.5f) * CELL_SIZE;
    if (!SlDriveTo(car)) { slDriveFails++; c->stopSteps = 1 << 30; return false; }
    c->ownerIn = true;
    c->boardStep = slStep;
    c->boardTime = gameTime;
    c->boardX = m->x; c->boardY = m->y;
    m->transportState = TRANSPORT_RIDING;
    TrainsMarkPersonalTrip(mi, true);   // its own trip: the jobs' idle pass leaves the ride alone
    m->transportTrainIdx = -1;
    ClearMoverPath(mi);
    slDrives++;
    if (getenv("SL_DRIVELOG")) printf("t %.1f: mover %d gets in car %d at (%.0f,%.0f), to (%d,%d)\n", gameTime, mi, car, m->x / CELL_SIZE, m->y / CELL_SIZE, m->transportFinalGoal.x, m->transportFinalGoal.y);
    return true;
}

// Parked with its owner in it: the owner gets out on the pavement beside it, walks on
static void SlDriverOut(int car) {
    SlotCar* c = &slotCars[car];
    Mover* m = &movers[c->owner];
    float x, y, h, st;
    SlotCarDrawPose(car, &x, &y, &h, &st);
    int cx = (int)(x / CELL_SIZE), cy = (int)(y / CELL_SIZE), z = (int)m->z;
    int bx = cx, by = cy;
    float bestD = 1e30f;
    for (int dy = -4; dy <= 4; dy++)
        for (int dx = -4; dx <= 4; dx++) {
            int xx = cx + dx, yy = cy + dy;
            if (xx < 0 || yy < 0 || xx >= gridWidth || yy >= gridHeight || !HAS_FLOOR(xx, yy, z)) continue;
            if (dx * dx + dy * dy < bestD) { bestD = (float)(dx * dx + dy * dy); bx = xx; by = yy; }
        }
    m->x = (bx + 0.5f) * CELL_SIZE;
    m->y = (by + 0.5f) * CELL_SIZE;
    m->lastX = m->x; m->lastY = m->y;
    m->transportState = TRANSPORT_NONE;
    TrainsMarkPersonalTrip(c->owner, false);
    m->goal = m->transportFinalGoal;
    ClearMoverPath(c->owner);
    m->needsRepath = true;
    c->ownerIn = false;
    c->stopSteps = 1 << 30;
    slDrivesDone++;
    {   // the drive's speed into the running average
        double t = gameTime - c->boardTime;
        float d = hypotf(m->x - c->boardX, m->y - c->boardY) / CELL_SIZE;
        if (t > 5 && d > 10) slDriveSpeed = 0.8f * slDriveSpeed + 0.2f * (float)(d / t);
    }
    if (getenv("SL_DRIVELOG")) printf("t %.1f: mover %d gets out of car %d at (%.0f,%.0f)\n", gameTime, c->owner, car, m->x / CELL_SIZE, m->y / CELL_SIZE);
}

// Is (x, y) inside car `car`'s body (grown by `grow` px)?
static bool SlInCar(int car, float x, float y, float grow) {
    float cx, cy, ch, st;
    SlotCarDrawPose(car, &cx, &cy, &ch, &st);
    float dx = x - cx, dy = y - cy;
    float u = dx * cosf(ch) + dy * sinf(ch), v = -dx * sinf(ch) + dy * cosf(ch);
    return fabsf(u) < carLength * CELL_SIZE * 0.5f + grow && fabsf(v) < carWidth * CELL_SIZE * 0.5f + grow;
}

static double* slPedWaitStart = NULL;
static int slPedWaitCap = 0;
#define SL_PED_PATIENCE 6.0   // seconds at the kerb before a pedestrian just goes (the cars stop)
bool SlotsPedHold(int mi, float dirX, float dirY) {
    if (slotCarCount == 0 || slTotal == 0 || !slBHead) return false;
    if (mi >= slPedWaitCap) {
        int n = mi + 64;
        slPedWaitStart = realloc(slPedWaitStart, sizeof(double) * n);
        for (int i = slPedWaitCap; i < n; i++) slPedWaitStart[i] = -1;
        slPedWaitCap = n;
    }
    if (mi >= slPedHeldCap) {
        int n = mi + 64;
        slPedHeldAt = realloc(slPedHeldAt, sizeof(double) * n);
        slPedGoStep = realloc(slPedGoStep, sizeof(int) * n);
        slPedDX = realloc(slPedDX, sizeof(float) * n);
        slPedDY = realloc(slPedDY, sizeof(float) * n);
        for (int i = slPedHeldCap; i < n; i++) { slPedHeldAt[i] = -1e9; slPedGoStep[i] = -1; }
        slPedHeldCap = n;
    }
    const Mover* m = &movers[mi];
    float CS = CELL_SIZE, B = SL_BUCKET * CS;
    float qx = m->x + dirX * 0.8f * CS, qy = m->y + dirY * 0.8f * CS;
    bool onRoad = SlRoadAt(m->x, m->y, (int)m->z), qOnRoad = SlRoadAt(qx, qy, (int)m->z), hold = false, inWay = false;
    int wayCar = -1;
    int bx0 = (int)((m->x - 3 * CS - SL_SPAN_REACH) / B), bx1 = (int)((m->x + 3 * CS + SL_SPAN_REACH) / B);
    int by0 = (int)((m->y - 3 * CS - SL_SPAN_REACH) / B), by1 = (int)((m->y + 3 * CS + SL_SPAN_REACH) / B);
    for (int by = by0 < 0 ? 0 : by0; by <= by1 && by < slBH; by++)
        for (int bx = bx0 < 0 ? 0 : bx0; bx <= bx1 && bx < slBW; bx++)
            for (int e = slBHead[by * slBW + bx]; e < slBHead[by * slBW + bx + 1]; e++) {
                int idx = slBList[e];
                (void)idx;
            }
    for (int by = by0 < 0 ? 0 : by0; by <= by1 && by < slBH && !inWay; by++)
        for (int bx = bx0 < 0 ? 0 : bx0; bx <= bx1 && bx < slBW && !inWay; bx++)
            for (int e = slBHead[by * slBW + bx]; e < slBHead[by * slBW + bx + 1] && !inWay; e++) {
                int idx = slBList[e];
                if (SlSlotDist2(idx, qx, qy) > 2.6f * 2.6f * CS * CS) continue;
                if (fabsf(slCZ[idx] - m->z) > 0.6f) continue;   // (a road above or below)
                int car = slSlotCar[idx];
                // a car right where we step (unless we're touching it already: then walk on, out of it)
                // (in the road: only a moving car - waiting for a standing one locked whole districts:
                // it stood for the traffic the pedestrians held up). A car is where it is
                // drawn and where its slot ends this step (it may be gliding in)
                bool patient = !onRoad || slotCars[car < 0 ? 0 : car].v > 0;
                if (car >= 0 && patient && !SlInCar(car, m->x, m->y, 0.4f * CS)) {
                    bool atSlot = slotCars[car].v > 0 && SlSlotDist2(idx, qx, qy) < 1.5f * 1.5f * CS * CS;
                    if (atSlot || SlInCar(car, qx, qy, 0.4f * CS)) { inWay = true; wayCar = car; }
                }
                if (onRoad || !qOnRoad) continue;   // the kerb wait: only when stepping off the pavement
                // stepping off the kerb: a moving car there or coming (4 slots back) - wait
                if (car >= 0 && slotCars[car].v > 0) hold = true;
                int seg = slSlotSeg[idx], k = idx - slSeg0[seg];
                for (int b = 1; b <= 4 && k - b >= 0; b++) {
                    int o = slSlotCar[SlIdx(seg, k - b)];
                    if (o >= 0 && slotCars[o].v > 0) hold = true;
                }
            }
    if (getenv("SL_PEDDBG")) {
        static int told = 0;
        static unsigned char seen[4096];
        if ((inWay || hold) && mi < 4096 && !seen[mi] && told++ < 30 && (seen[mi] = 1))
            printf("t %.1f ped %d at (%.1f,%.1f) dir (%.2f,%.2f) onRoad %d qOnRoad %d inWay %d hold %d waited %.1f car %d\n", gameTime, mi, m->x / CS, m->y / CS, dirX, dirY,
                   onRoad, qOnRoad, inWay, hold, slPedWaitStart[mi] < 0 ? 0.0 : gameTime - slPedWaitStart[mi], wayCar);
        if (wayCar >= 0 && told < 30) { const SlotCar* c = &slotCars[wayCar]; printf("   car %d v %d wait %d seg %d kind %d slot %d/%d ri %d/%d needRoute %d stop %d kerb %d\n", wayCar, c->v, c->waitSteps, c->route[c->ri], rdSegs[c->route[c->ri]].kind, c->slot, slSegN[c->route[c->ri]], c->ri, c->routeLen, c->needRoute, c->stopSteps, c->kerb); }
    }
    if (onRoad) {
        if (inWay) {   // a car where we'd step: wait; go on at the next step's start (announced, as below)
            if (slPedWaitStart[mi] < 0) slPedWaitStart[mi] = gameTime;
            slPedGoStep[mi] = -1; slPedHolds++; slPedHeldAt[mi] = gameTime;
            return true;
        }
        if (slPedHeldAt[mi] > gameTime - 0.1 && slPedGoStep[mi] < 0) { slPedGoStep[mi] = slStep + 1; slPedDX[mi] = dirX; slPedDY[mi] = dirY; }
        if (slPedGoStep[mi] >= 0 && slStep < slPedGoStep[mi]) { slPedHolds++; slPedHeldAt[mi] = gameTime; return true; }
        slPedWaitStart[mi] = -1;
        return false;
    }
    if (!qOnRoad) { slPedGoStep[mi] = -1; slPedWaitStart[mi] = -1; return false; }   // walking the pavement
    // at the kerb, about to step off
    // At a junction with lights: cross the road only while its traffic has red (the walk sign);
    // the nearest lane says which road we cross
    {
        int best = -1;
        float bestD = 8.0f * 8.0f * CS * CS;
        for (int by = by0 - 1 < 0 ? 0 : by0 - 1; by <= by1 + 1 && by < slBH; by++)
            for (int bx = bx0 - 1 < 0 ? 0 : bx0 - 1; bx <= bx1 + 1 && bx < slBW; bx++)
                for (int e = slBHead[by * slBW + bx]; e < slBHead[by * slBW + bx + 1]; e++) {
                    int idx = slBList[e];
                    if (rdSegs[slSlotSeg[idx]].kind != RD_LANE) continue;
                    float dx = slCX[idx] - qx, dy = slCY[idx] - qy;
                    if (dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; best = slSlotSeg[idx]; }
                }
        if (best >= 0) {
            const RdEdge* E = &rdEdges[rdSegs[best].edge];
            int nodes[2] = {E->a, E->b};
            for (int t = 0; t < 2; t++) {
                const RdNode* N = &rdNodes[nodes[t]];
                float dx = N->x - qx, dy = N->y - qy;
                if (dx * dx + dy * dy > 10.0f * 10.0f * CS * CS) continue;
                int in = E->b == nodes[t] ? E->lane[0] : E->lane[1];
                if (in < 0) in = E->b == nodes[t] ? E->lane[1] : E->lane[0];   // (one-way: the lights of its other end... none: -1 below)
                int light = in >= 0 && rdSegs[in].to == nodes[t] ? TrafficLightAt(nodes[t], in) : -1;
                if (light == 1 || light == 2) { slPedWaitStart[mi] = -1; slPedGoStep[mi] = -1; slPedHolds++; slPedHeldAt[mi] = gameTime; return true; }   // don't walk
            }
        }
    }
    if (inWay) hold = true;   // a car where we'd step: wait (with patience) like for a gap
    if (hold) {
        if (slPedWaitStart[mi] < 0) slPedWaitStart[mi] = gameTime;
        if (gameTime - slPedWaitStart[mi] < SL_PED_PATIENCE) { slPedGoStep[mi] = -1; slPedHolds++; slPedHeldAt[mi] = gameTime; return true; }
        // waited long enough: steps out anyway (announced: the cars stop)
    }
    if (slPedGoStep[mi] < 0) { slPedGoStep[mi] = slStep + 1; slPedDX[mi] = dirX; slPedDY[mi] = dirY; }
    if (slStep < slPedGoStep[mi]) { slPedHolds++; slPedHeldAt[mi] = gameTime; return true; }   // until the next step starts
    return false;
}

float SlotCarZ(int i) {
    const SlotCar* c = &slotCars[i];
    int seg = c->route[c->ri];
    float z1 = RoadZAt(seg, SlS(seg, c->slot));
    if (c->prevRi != c->ri) return z1;
    float z0 = RoadZAt(seg, SlS(seg, c->prevSlot)), f = slAcc / SL_STEP;
    return z0 + (z1 - z0) * f;
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
        printf(" - no curve wanted; slot ahead holds car %d (seg %d kind %d)%s\n", ahead, c->route[ri], rdSegs[c->route[ri]].kind,
               ri < c->routeLen && k < slSegN[c->route[ri]] && slPedStamp[SlIdx(c->route[ri], k)] == slStep ? " - a pedestrian" : "");
        static int depth = 0;
        if (ahead >= 0 && ahead != i && depth < 40) { depth++; SlExplain(ahead); depth--; }   // the queue's front
        return;
    }
    const RdSeg* C = &rdSegs[conn];
    const RdNode* N = &rdNodes[C->node];
    int freeRun = 0;
    while (freeRun < slSegN[C->to] && SlFree(SlIdx(C->to, freeRun))) freeRun++;
    printf(" - wants curve %d (node %d)%s: light %d, exit lane %d (%d slots) free run %d (on curve %d), in it:", conn, C->node, SlPedOn(conn) ? " [pedestrian on it]" : "", TrafficLightAt(C->node, C->from),
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
    if (getenv("SL_TOUCH")) printf("  touches: max %d pairs at once, %ld pair-samples over %ld samples; pedestrians inside a car: %ld\n", slTouchMax, slTouchSum, slTouchSamples, slPedHits);
    if (moverCount > 0) {
        int onRoadNow = 0;
        for (int mi = 0; mi < moverCount; mi++) {
            if (!movers[mi].active) continue;
            float B = SL_BUCKET * CELL_SIZE;
            int bx = (int)(movers[mi].x / B), by = (int)(movers[mi].y / B);
            bool on = false;
            for (int yy = by - 1; yy <= by + 1 && !on; yy++) for (int xx = bx - 1; xx <= bx + 1 && !on; xx++) {
                if (xx < 0 || yy < 0 || xx >= slBW || yy >= slBH) continue;
                for (int e = slBHead[yy * slBW + xx]; e < slBHead[yy * slBW + xx + 1]; e++) {
                    float dx = slCX[slBList[e]] - movers[mi].x, dy = slCY[slBList[e]] - movers[mi].y;
                    if (dx * dx + dy * dy < 1.8f * 1.8f * CELL_SIZE * CELL_SIZE) { on = true; break; }
                }
            }
            if (on) onRoadNow++;
            if (on && getenv("SL_PEDLIST")) {
                const Mover* m = &movers[mi];
                printf("   in road: ped %d at (%.1f,%.1f) state %d owns car %d (in %d) held %.1f s ago path %d/%d goal (%d,%d) road %d noProgress %.1f\n", mi, m->x / CELL_SIZE, m->y / CELL_SIZE,
                       m->transportState, SlotsCarOf(mi), SlotsCarOf(mi) >= 0 ? slotCars[SlotsCarOf(mi)].ownerIn : -1, mi < slPedHeldCap ? gameTime - slPedHeldAt[mi] : -1.0,
                       m->pathIndex, m->pathLength, m->goal.x, m->goal.y, SlRoadAt(m->x, m->y, (int)m->z), m->timeWithoutProgress);
            }
        }
        printf("  pedestrians in the road now: %d\n", onRoadNow);
        if (getenv("SL_PEDNODE")) {
            const RdNode* N = &rdNodes[atoi(getenv("SL_PEDNODE"))];
            printf("  node at (%.1f,%.1f)\n", N->x / CELL_SIZE, N->y / CELL_SIZE);
            for (int mi = 0; mi < moverCount; mi++) {
                float dx = movers[mi].x - N->x, dy = movers[mi].y - N->y;
                if (dx * dx + dy * dy > 12.0f * 12.0f * CELL_SIZE * CELL_SIZE) continue;
                const Mover* m = &movers[mi];
                printf("   ped %d at (%.1f,%.1f) road %d held %.1f s ago, goStep %d (step %d), waitStart %.1f ago, path %d/%d goal (%d,%d) next (%d,%d) noProgress %.1f\n", mi, m->x / CELL_SIZE, m->y / CELL_SIZE,
                       SlRoadAt(m->x, m->y, (int)m->z), mi < slPedHeldCap ? gameTime - slPedHeldAt[mi] : -1.0, mi < slPedHeldCap ? slPedGoStep[mi] : -9, slStep,
                       mi < slPedWaitCap && slPedWaitStart[mi] >= 0 ? gameTime - slPedWaitStart[mi] : -1.0, m->pathIndex, m->pathLength, m->goal.x, m->goal.y,
                       m->pathIndex >= 0 ? moverPaths[mi][m->pathIndex].x : -1, m->pathIndex >= 0 ? moverPaths[mi][m->pathIndex].y : -1, m->timeWithoutProgress);
            }
        }
    }
    if (slDrives || slDriveFails) printf("  movers driving: %ld drives started, %ld parked and got out, %ld found no spot to drive to\n", slDrives, slDrivesDone, slDriveFails);
    if (slChoseDrive + slChoseWalk > 0)
        printf("  drive or walk (owners, car parked): %ld drove, %ld walked; estimates on average: walk %.0f s, drive %.0f s; drives' speed %.2f cells/s (straight line)\n",
               slChoseDrive, slChoseWalk, slSumWalkT / (slChoseDrive + slChoseWalk), slSumDriveT / (slChoseDrive + slChoseWalk), slDriveSpeed);
    if (moverCount > 0) printf("  pedestrians: %d; %ld slots blocked for them, %ld ticks waiting at the kerb or for a car; hard stops (any cause) %ld\n",
                               moverCount, slPedBlocks, slPedHolds, slHardStops);
    if (getenv("SL_JUMPS")) printf("  jumps: %ld moves bigger than the top speed allows (worst %.0f px)\n", jumpCount, jumpWorst);
    if (carSlotReal)
        printf("  real cars in view: %.1f on average; tracking error mean %.2f cells, max %.2f; %ld snaps; touches max %d pairs\n",
               slRealTicks ? (double)slRealSum / slRealTicks : 0.0, slRealErrN ? slRealErrSum / slRealErrN / CELL_SIZE : 0.0,
               slRealErrMax / CELL_SIZE, slRealSnaps, slRealTouchMax);
    { int atKerb = 0; for (int i = 0; i < slotCarCount; i++) if (slotCars[i].kerb) atKerb++;
      printf("  kerb: %d cars there now; %ld parked, %ld cab fares at the kerb, %ld arrivals found the spot taken, %ld pull-out waits\n",
             atKerb, slParks, slFareStops, slKerbFull, slPullOutWaits); }
    { int sl = 0, sr = 0, haz = 0, br = 0;
      for (int i = 0; i < slotCarCount; i++) { int g = SlotCarSignals(i);
          if ((g & 3) == 3) haz++; else if (g & SL_SIG_LEFT) sl++; else if (g & SL_SIG_RIGHT) sr++;
          if (g & SL_SIG_BRAKE) br++; }
      printf("  signals now: %d left, %d right, %d hazards, %d brake lights\n", sl, sr, haz, br); }
    if (carSlotTempers) {
        int n[3] = {0}, tr[3] = {0};
        long vs[3] = {0};
        for (int i = 0; i < slotCarCount; i++) { int t = slotCars[i].temper; n[t]++; tr[t] += slotCars[i].trips; vs[t] += slotCars[i].v; }
        printf("  drivers: normal %d (%.2f trips each, speed now %.1f), speeders %d (%.2f, %.1f), slow %d (%.2f, %.1f) [cells/s]; %ld gaps held for merging cars, %ld used\n",
               n[0], n[0] ? (double)tr[0] / n[0] : 0.0, n[0] ? (double)vs[0] / n[0] * SL_SLOT_CELLS : 0.0,
               n[1], n[1] ? (double)tr[1] / n[1] : 0.0, n[1] ? (double)vs[1] / n[1] * SL_SLOT_CELLS : 0.0,
               n[2], n[2] ? (double)tr[2] / n[2] : 0.0, n[2] ? (double)vs[2] / n[2] * SL_SLOT_CELLS : 0.0, slYields, slYieldMerges);
    }
    if (rdClasses) {   // road classes: lanes, their cars' speed now, yields to the bigger road
        static const char* nm[RD_CLS_COUNT] = {"lane", "street", "arterial", "highway"};
        int lanes[RD_CLS_COUNT] = {0}, cars[RD_CLS_COUNT] = {0}; long vs[RD_CLS_COUNT] = {0};
        for (int sg = 0; sg < rdSegCount; sg++) if (rdSegs[sg].kind == RD_LANE) lanes[RoadSegClass(sg)]++;
        for (int i = 0; i < slotCarCount; i++) { const SlotCar* c = &slotCars[i]; if (c->kerb || rdSegs[c->route[c->ri]].kind != RD_LANE) continue;
            int k = RoadSegClass(c->route[c->ri]); cars[k]++; vs[k] += c->v; }
        printf("  classes:");
        for (int k = RD_CLS_COUNT - 1; k >= 0; k--) if (lanes[k])
            printf(" %s %d lanes (limit %.0f, %d cars at %.1f)", nm[k], lanes[k], rdClassSpeed[k], cars[k], cars[k] ? (double)vs[k] / cars[k] * SL_SLOT_CELLS : 0.0);
        printf(" [cells/s]; %ld curves held for the bigger road's traffic\n", slClassYields);
    }
    {   // heights: lifted roads, how much of them is up a level
        int lifted = 0, up = 0, pts = 0;
        for (int e = 0; e < rdEdgeCount; e++) if (rdEdges[e].lifted) lifted++;
        for (int sg = 0; sg < rdSegCount; sg++) if (rdSegs[sg].kind == RD_LANE && rdEdges[rdSegs[sg].edge].lifted)
            for (int q = 0; q < rdSegs[sg].n; q++) { pts++; if (rdZ[rdSegs[sg].p0 + q] > rdBaseZ + 0.5f) up++; }
        if (lifted) printf("  heights: %d lifted roads, %.0f%% of their lanes a level up; %d couldn't clear a crossing\n", lifted, pts ? 100.0 * up / pts : 0.0, rdLiftShort);
    }
    printf("  lane changes: %ld to overtake, %ld back to the right-hand lane, %ld into the lane a turn needs (%ld gave up: another way)\n", slOvertakes, slKeepRights, slMerges, slMergeGiveUps);
    printf("  longest standstill %.0f s; route searches %ld (%ld found nothing), %ld re-routes round full exits\n", slStillMax * SL_STEP, slRoutes, slRouteFails, slReroutes);
}

bool SlotsCheck(char* why, int whyLen) {
    int occupied = 0;
    for (int s = 0; s < slTotal; s++) { if (slSlotCar[s] >= 0) occupied++; if (slKerbCar[s] >= 0) occupied++; }
    for (int i = 0; i < slotCarCount; i++) {
        const SlotCar* c = &slotCars[i];
        if ((c->kerb ? slKerbCar : slSlotCar)[SlIdx(c->route[c->ri], c->slot)] != i) {
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
