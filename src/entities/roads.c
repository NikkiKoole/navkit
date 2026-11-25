// roads.c - Road network for the cars experiment (see roads.h)
#include "roads.h"
#include "mover.h"            // CELL_SIZE
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

float rdSetback = 7.0f;
bool rdLaneRules = true;
RdNode rdNodes[RD_MAX_NODES];
RdEdge rdEdges[RD_MAX_EDGES];
RdSeg rdSegs[RD_MAX_SEGS];
int rdNodeCount = 0, rdEdgeCount = 0, rdSegCount = 0;
float *rdX = NULL, *rdY = NULL, *rdS = NULL;
static float* rdH = NULL;   // heading from point k to k+1 (the last point: the one before)
static int rdPtCount = 0, rdPtCap = 0;

static void EnsurePts(int n) {
    if (rdPtCount + n <= rdPtCap) return;
    int cap = rdPtCap ? rdPtCap : 65536;
    while (cap < rdPtCount + n) cap *= 2;
    rdX = realloc(rdX, sizeof(float) * cap);
    rdY = realloc(rdY, sizeof(float) * cap);
    rdS = realloc(rdS, sizeof(float) * cap);
    rdH = realloc(rdH, sizeof(float) * cap);
    rdPtCap = cap;
}
RdBay rdBays[RD_MAX_BAYS];
int rdBayCount = 0;
float rdSegExtra[RD_MAX_SEGS];
static int rdLanes[RD_MAX_SEGS], rdLaneCount = 0;

#define RD_STEP 8.0f            // px between points
#define RD_MAX_TURN 2.1f        // radians (~120 degrees): sharpest turn through a junction
#define RD_LANE_CHANGE_COST 4.0f  // cells a lane change adds to a route
#define RD_MAX_TURN_SHARP 2.6f  // (~150) allowed where nothing else leads in / out of a lane
#define RD_CONFLICT 2.6f        // cells: connectors closer than this conflict
#define RD_ROUTE_TMP 4096

void RoadsClear(void) {
    rdNodeCount = rdEdgeCount = rdSegCount = rdPtCount = rdLaneCount = rdBayCount = 0;
    memset(rdSegExtra, 0, sizeof(rdSegExtra));
}

int RoadAddNode(float cellX, float cellY) {
    if (rdNodeCount >= RD_MAX_NODES) return -1;
    RdNode* n = &rdNodes[rdNodeCount];
    memset(n, 0, sizeof(*n));
    n->x = cellX * CELL_SIZE;
    n->y = cellY * CELL_SIZE;
    n->setback = RD_SETBACK;
    n->ringOf = -1;
    return rdNodeCount++;
}

int RoadAddEdge(int a, int b, float bendCells) {
    if (rdEdgeCount >= RD_MAX_EDGES) return -1;
    if (rdNodes[a].nEdge >= RD_MAX_NODE_EDGES || rdNodes[b].nEdge >= RD_MAX_NODE_EDGES) return -1;
    RdEdge* e = &rdEdges[rdEdgeCount];
    memset(e, 0, sizeof(*e));
    e->a = a; e->b = b; e->bend = bendCells;
    e->lane[0] = e->lane[1] = e->lane2[0] = e->lane2[1] = -1;
    e->lanesPerDir = 1;
    rdNodes[a].edge[rdNodes[a].nEdge++] = rdEdgeCount;
    rdNodes[b].edge[rdNodes[b].nEdge++] = rdEdgeCount;
    return rdEdgeCount++;
}

void RoadSetAvenue(int edge) {   // two lanes each way; a one-way road: two lanes its way
    if (edge >= 0 && edge < rdEdgeCount) rdEdges[edge].lanesPerDir = 2;
}

// ---------------------------------------------------------------------------
// Polylines: a dense curve resampled to RD_STEP px into the point pool
// ---------------------------------------------------------------------------
#define RD_TMP 8192
static float tmpX[RD_TMP], tmpY[RD_TMP];

// Resample tmp[0..n) at RD_STEP into a new segment's points; returns the segment
static int SegFromTmp(int n, RdSegKind kind) {
    if (rdSegCount >= RD_MAX_SEGS || n < 2) return -1;
    RdSeg* sg = &rdSegs[rdSegCount];
    memset(sg, 0, sizeof(*sg));
    sg->kind = kind;
    sg->bay = -1;
    sg->parallel = -1;   // (0 would be lane 0: curves into lane 0 looked like lane changes)
    float total = 0;
    for (int i = 0; i + 1 < n; i++) total += sqrtf((tmpX[i + 1] - tmpX[i]) * (tmpX[i + 1] - tmpX[i]) + (tmpY[i + 1] - tmpY[i]) * (tmpY[i + 1] - tmpY[i]));
    EnsurePts((int)(total / RD_STEP) + 4);
    sg->p0 = rdPtCount;
    float acc = 0;            // arc length at tmp[i]
    float next = 0;           // next sample's arc length
    for (int i = 0; i + 1 < n; i++) {
        float dx = tmpX[i + 1] - tmpX[i], dy = tmpY[i + 1] - tmpY[i];
        float L = sqrtf(dx * dx + dy * dy);
        while (next <= acc + L) {
            float t = L > 1e-6f ? (next - acc) / L : 0;
            rdX[rdPtCount] = tmpX[i] + dx * t;
            rdY[rdPtCount] = tmpY[i] + dy * t;
            rdS[rdPtCount] = next;
            rdPtCount++;
            next += RD_STEP;
        }
        acc += L;
    }
    if (acc - rdS[rdPtCount - 1] > 0.5f) {   // the exact end
        rdX[rdPtCount] = tmpX[n - 1];
        rdY[rdPtCount] = tmpY[n - 1];
        rdS[rdPtCount] = acc;
        rdPtCount++;
    }
    sg->n = rdPtCount - sg->p0;
    sg->len = rdS[rdPtCount - 1];
    for (int i = sg->p0; i < rdPtCount - 1; i++) rdH[i] = atan2f(rdY[i + 1] - rdY[i], rdX[i + 1] - rdX[i]);
    rdH[rdPtCount - 1] = sg->n > 1 ? rdH[rdPtCount - 2] : 0;
    sg->bx0 = sg->by0 = 1e30f; sg->bx1 = sg->by1 = -1e30f;
    for (int i = sg->p0; i < rdPtCount; i++) {
        sg->bx0 = fminf(sg->bx0, rdX[i]); sg->bx1 = fmaxf(sg->bx1, rdX[i]);
        sg->by0 = fminf(sg->by0, rdY[i]); sg->by1 = fmaxf(sg->by1, rdY[i]);
    }
    return rdSegCount++;
}

static float SegHeading(int seg, int i) {   // direction at point i (index within the segment)
    const RdSeg* sg = &rdSegs[seg];
    int a = i > 0 ? i - 1 : 0, b = i + 1 < sg->n ? i + 1 : sg->n - 1;
    return atan2f(rdY[sg->p0 + b] - rdY[sg->p0 + a], rdX[sg->p0 + b] - rdX[sg->p0 + a]);
}

static float AngleDiff(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

// Bend speed limit: the tightest curvature, measured over ~3 points
static float SegVmax(int seg, float latAccel) {
    const RdSeg* sg = &rdSegs[seg];
    float worst = 0;
    for (int i = 0; i + 3 < sg->n; i++) {
        float d = fabsf(AngleDiff(SegHeading(seg, i + 3) - SegHeading(seg, i)));
        float L = rdS[sg->p0 + i + 3] - rdS[sg->p0 + i];
        if (L > 1) worst = fmaxf(worst, d / L);
    }
    if (worst < 1e-5f) return 1e9f;
    return sqrtf(latAccel * CELL_SIZE / worst) / CELL_SIZE;   // cells/s
}

// The centre line of an edge, dense, into tmp; returns the count
static int EdgeCentre(const RdEdge* e) {
    float ax = rdNodes[e->a].x, ay = rdNodes[e->a].y, bx = rdNodes[e->b].x, by = rdNodes[e->b].y;
    if (e->arc) {   // from a to b round the centre, angle decreasing
        float t0 = atan2f(ay - e->arcY, ax - e->arcX), t1 = atan2f(by - e->arcY, bx - e->arcX);
        while (t1 >= t0) t1 -= 6.2831853f;
        while (t1 < t0 - 6.2831853f) t1 += 6.2831853f;
        int n = 200;
        for (int i = 0; i <= n; i++) {
            float t = t0 + (t1 - t0) * i / n;
            tmpX[i] = e->arcX + cosf(t) * e->arcR;
            tmpY[i] = e->arcY + sinf(t) * e->arcR;
        }
        return n + 1;
    }
    float mx = (ax + bx) * 0.5f, my = (ay + by) * 0.5f;
    float dx = bx - ax, dy = by - ay, L = sqrtf(dx * dx + dy * dy);
    float cx = mx + (-dy / L) * 2.0f * e->bend * CELL_SIZE, cy = my + (dx / L) * 2.0f * e->bend * CELL_SIZE;
    int n = 400;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, u = 1 - t;
        tmpX[i] = u * u * ax + 2 * u * t * cx + t * t * bx;
        tmpY[i] = u * u * ay + 2 * u * t * cy + t * t * by;
    }
    return n + 1;
}

// One lane of an edge: the centre line offset to the right of travel, without the
// RD_SETBACK at each end. dir 0: a -> b, 1: b -> a.
static int BuildLane(int ei, int dir, int k) {
    const RdEdge* e = &rdEdges[ei];
    int n = EdgeCentre(e);
    static float cx[RD_TMP], cy[RD_TMP], cs[RD_TMP];
    float acc = 0;
    for (int i = 0; i < n; i++) {
        int k = dir == 0 ? i : n - 1 - i;
        cx[i] = tmpX[k]; cy[i] = tmpY[k];
        if (i > 0) acc += sqrtf((cx[i] - cx[i - 1]) * (cx[i] - cx[i - 1]) + (cy[i] - cy[i - 1]) * (cy[i] - cy[i - 1]));
        cs[i] = acc;
    }
    int startNode = dir == 0 ? e->a : e->b, endNode = dir == 0 ? e->b : e->a;
    float sbA = rdNodes[startNode].setback * CELL_SIZE, sbB = rdNodes[endNode].setback * CELL_SIZE;
    // the right-hand lane (k 0) is the outer one, by the kerb; the overtaking lane (k 1) inner
    // (one-way: one lane on the centre line, or two lanes either side of it: k 0 on the right)
    float off = e->oneWay ? (e->lanesPerDir == 2 ? (k == 0 ? RD_LANE_OFF : -RD_LANE_OFF) * CELL_SIZE : 0)
                          : (RD_LANE_OFF + 3.0f * (e->lanesPerDir - 1 - k)) * CELL_SIZE;
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (cs[i] < sbA || cs[i] > acc - sbB) continue;
        int a = i > 0 ? i - 1 : 0, b = i + 1 < n ? i + 1 : n - 1;
        float tx = cx[b] - cx[a], ty = cy[b] - cy[a], tl = sqrtf(tx * tx + ty * ty);
        tx /= tl; ty /= tl;
        tmpX[m] = cx[i] - ty * off;     // right of travel (y is down)
        tmpY[m] = cy[i] + tx * off;
        m++;
    }
    int seg = SegFromTmp(m, RD_LANE);
    if (seg < 0) return -1;
    RdSeg* sg = &rdSegs[seg];
    sg->from = dir == 0 ? e->a : e->b;
    sg->to = dir == 0 ? e->b : e->a;
    sg->node = sg->to;
    sg->priority = e->arc;
    sg->edge = ei;
    sg->laneIdx = k;
    sg->parallel = -1;
    rdLanes[rdLaneCount++] = seg;
    return seg;
}

// A connector through `node` from the end of lane `in` to the start of lane `out`
static int BuildConnector(int node, int in, int out, float maxTurn) {
    const RdSeg* a = &rdSegs[in];
    const RdSeg* b = &rdSegs[out];
    float x0 = rdX[a->p0 + a->n - 1], y0 = rdY[a->p0 + a->n - 1];
    float x3 = rdX[b->p0], y3 = rdY[b->p0];
    float h0 = SegHeading(in, a->n - 1), h3 = SegHeading(out, 0);
    float turn = fabsf(AngleDiff(h3 - h0));
    if (turn > maxTurn) return -1;   // a U-turn, or (> ~120 degrees) so sharp it sweeps the stop lines
    float d = sqrtf((x3 - x0) * (x3 - x0) + (y3 - y0) * (y3 - y0));
    float k = d * (turn < 0.3f ? 0.33f : 0.42f);
    float x1 = x0 + cosf(h0) * k, y1 = y0 + sinf(h0) * k;
    float x2 = x3 - cosf(h3) * k, y2 = y3 - sinf(h3) * k;
    int n = 128;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, u = 1 - t;
        tmpX[i] = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
        tmpY[i] = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
    }
    int seg = SegFromTmp(n + 1, RD_CONN);
    if (seg < 0) return -1;
    RdSeg* sg = &rdSegs[seg];
    sg->node = node;
    sg->from = in;
    sg->to = out;
    RdNode* nd = &rdNodes[node];
    if (nd->nConn >= RD_MAX_CONN) { rdSegCount--; return -1; }
    sg->ci = nd->nConn;
    nd->conn[nd->nConn++] = seg;
    RdSeg* L = &rdSegs[in];
    if (L->nOut < RD_MAX_OUT) L->out[L->nOut++] = seg;
    return seg;
}

// A cubic from (x0,y0) heading h0 to (x3,y3) heading h3, into tmp; returns the count
static int Cubic(float x0, float y0, float h0, float x3, float y3, float h3, float kf) {
    float d = sqrtf((x3 - x0) * (x3 - x0) + (y3 - y0) * (y3 - y0)), k = d * kf;
    float x1 = x0 + cosf(h0) * k, y1 = y0 + sinf(h0) * k;
    float x2 = x3 - cosf(h3) * k, y2 = y3 - sinf(h3) * k;
    int n = 64;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, u = 1 - t;
        tmpX[i] = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
        tmpY[i] = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
    }
    return n + 1;
}

// A bay on `lane` starting at s0 px: in-curve to the spot, out-curve back to the lane
static void BuildBay(int lane, float s0) {
    if (rdBayCount >= RD_MAX_BAYS || rdSegs[lane].nBays >= 4) return;
    float CS = CELL_SIZE, s1 = s0 + RD_BAY_LEN * CS, sm = (s0 + s1) * 0.5f;
    float ax, ay, ah, mx, my, mh, bx, by, bh;
    RoadPointAt(lane, s0, &ax, &ay, &ah);
    RoadPointAt(lane, sm, &mx, &my, &mh);
    RoadPointAt(lane, s1, &bx, &by, &bh);
    float px = mx - sinf(mh) * RD_BAY_OFF * CS, py = my + cosf(mh) * RD_BAY_OFF * CS;   // right of travel
    int in = SegFromTmp(Cubic(ax, ay, ah, px, py, mh, 0.45f), RD_BAYIN);
    if (in < 0) return;
    int out = SegFromTmp(Cubic(px, py, mh, bx, by, bh, 0.45f), RD_BAYOUT);
    if (out < 0) { rdSegCount--; return; }
    RdBay* b = &rdBays[rdBayCount];
    b->lane = lane; b->s0 = s0; b->s1 = s1; b->in = in; b->out = out;
    rdSegs[in].from = rdSegs[in].to = lane;  rdSegs[in].bay = rdBayCount;  rdSegs[in].bayS = s0;  rdSegs[in].node = -1;
    rdSegs[out].from = rdSegs[out].to = lane; rdSegs[out].bay = rdBayCount; rdSegs[out].bayS = s1; rdSegs[out].node = -1;
    rdSegs[lane].bays[rdSegs[lane].nBays++] = rdBayCount;
    rdBayCount++;
}

// Which way the move from the end of lane `in` to the start of lane `out` turns: -1 left,
// 0 straight on, 1 right (right-hand traffic; y is down, so a right turn is clockwise)
static int TurnOf(int in, int out) {
    float h0 = SegHeading(in, rdSegs[in].n - 1), h1 = SegHeading(out, 0);
    float d = AngleDiff(h1 - h0);
    return d > 0.5f ? 1 : (d < -0.5f ? -1 : 0);
}

int RoadConnTurn(int conn) { return TurnOf(rdSegs[conn].from, rdSegs[conn].to); }

static bool ConnectorsConflict(int s1, int s2) {
    const RdSeg* a = &rdSegs[s1];
    const RdSeg* b = &rdSegs[s2];
    if (a->from == b->from) return false;   // one lane: cars follow each other
    // from two lanes side by side, unless both go straight on: a turning car's rear corner sweeps
    // toward the lane beside (cars start side by side at the stop line, ~1 cell apart)
    if (rdLaneRules && rdSegs[a->from].parallel == b->from && (TurnOf(a->from, a->to) != 0 || TurnOf(b->from, b->to) != 0)) return true;
    if (a->to == b->to) return true;        // merging into one lane
    float lim = RD_CONFLICT * CELL_SIZE;
    for (int i = 0; i < a->n; i++)
        for (int j = 0; j < b->n; j++) {
            float dx = rdX[a->p0 + i] - rdX[b->p0 + j], dy = rdY[a->p0 + i] - rdY[b->p0 + j];
            if (dx * dx + dy * dy < lim * lim) return true;
        }
    return false;
}

bool RoadMakeRoundabout(int node, float rCells) {
    RdNode* N = &rdNodes[node];
    int n = N->nEdge;
    if (n < 3 || rdNodeCount + n > RD_MAX_NODES) return false;
    float r = rCells * CELL_SIZE;
    int ringNode[RD_MAX_NODE_EDGES];
    float ang[RD_MAX_NODE_EDGES];
    // every ring stretch long enough?
    for (int k = 0; k < n; k++) {
        const RdEdge* e = &rdEdges[N->edge[k]];
        const RdNode* o = &rdNodes[e->a == node ? e->b : e->a];
        ang[k] = atan2f(o->y - N->y, o->x - N->x);
    }
    for (int k = 0; k < n; k++) {
        float gap = 1e9f;
        for (int j = 0; j < n; j++) {
            if (j == k) continue;
            float d = ang[k] - ang[j];
            while (d <= 0) d += 6.2831853f;
            if (d < gap) gap = d;
        }
        if (gap * rCells - 2 * RD_RING_SETBACK < RD_RING_MIN_LANE) return false;
    }
    for (int k = 0; k < n; k++) {
        RdEdge* e = &rdEdges[N->edge[k]];
        const RdNode* o = &rdNodes[e->a == node ? e->b : e->a];
        ang[k] = atan2f(o->y - N->y, o->x - N->x);
        int rn = RoadAddNode((N->x + cosf(ang[k]) * r) / CELL_SIZE, (N->y + sinf(ang[k]) * r) / CELL_SIZE);
        N = &rdNodes[node];   // (rdNodes is a fixed array; keep the pointer honest anyway)
        rdNodes[rn].ring = true;
        rdNodes[rn].ringOf = node;
        rdNodes[rn].setback = RD_RING_SETBACK;
        if (e->a == node) e->a = rn; else e->b = rn;
        rdNodes[rn].edge[rdNodes[rn].nEdge++] = N->edge[k];
        ringNode[k] = rn;
    }
    // ring: each ring junction to the next one with a smaller angle (anticlockwise on screen)
    for (int k = 0; k < n; k++) {
        int next = -1;
        float best = -1e9f;
        for (int j = 0; j < n; j++) {
            if (j == k) continue;
            float d = ang[j] - ang[k];
            while (d >= 0) d -= 6.2831853f;   // going down from ang[k]
            if (d > best) { best = d; next = j; }
        }
        int e = RoadAddEdge(ringNode[k], ringNode[next], 0);
        if (e < 0) continue;
        rdEdges[e].oneWay = true;
        rdEdges[e].arc = true;
        rdEdges[e].arcX = N->x; rdEdges[e].arcY = N->y; rdEdges[e].arcR = r;
    }
    N->nEdge = 0;
    return true;
}

static void BuildAll(float latAccel);

// Junctions where a curve passes within RD_CONFLICT of where a car waits at another
// lane's stop line (its centre ~1.4 cells before the lane's end) get one more cell of
// setback; true if any did (then build again)
static bool PushBackCrampedJunctions(void) {
    bool changed = false;
    float lim = RD_CONFLICT * CELL_SIZE, back = 1.4f * CELL_SIZE;
    for (int ni = 0; ni < rdNodeCount; ni++) {
        RdNode* nd = &rdNodes[ni];
        if (nd->ring || nd->nConn == 0 || nd->setback >= 14) continue;
        bool cramped = false;
        for (int q = 0; q < nd->nConn && !cramped; q++) {
            const RdSeg* C = &rdSegs[nd->conn[q]];
            for (int q2 = 0; q2 < nd->nConn && !cramped; q2++) {
                int lane = rdSegs[nd->conn[q2]].from;
                if (lane == C->from) continue;
                const RdSeg* Ln = &rdSegs[lane];
                float wx, wy, wh;
                RoadPointAt(lane, Ln->len - back, &wx, &wy, &wh);
                for (int i = 0; i < C->n; i++) {
                    float dx = rdX[C->p0 + i] - wx, dy = rdY[C->p0 + i] - wy;
                    if (dx * dx + dy * dy < lim * lim) { cramped = true; break; }
                }
            }
        }
        if (cramped) { nd->setback += 1.0f; changed = true; }
    }
    return changed;
}

void RoadsBuild(float latAccel) {
    for (int pass = 0; pass < 6; pass++) {
        BuildAll(latAccel);
        if (!PushBackCrampedJunctions()) break;
    }
}

static void BuildAll(float latAccel) {
    rdSegCount = rdPtCount = rdLaneCount = rdBayCount = 0;
    for (int i = 0; i < rdNodeCount; i++) rdNodes[i].nConn = 0;
    // An avenue's junctions need more room (its lanes reach 6 cells from its centre line)
    for (int e = 0; e < rdEdgeCount; e++)
        if (rdEdges[e].lanesPerDir == 2)
            for (int s = 0; s < 2; s++) {
                RdNode* nd = &rdNodes[s == 0 ? rdEdges[e].a : rdEdges[e].b];
                if (!nd->ring && nd->setback < RD_SETBACK + 2) nd->setback = RD_SETBACK + 2;
            }
    for (int e = 0; e < rdEdgeCount; e++) {
        RdEdge* E = &rdEdges[e];
        E->lane[0] = BuildLane(e, 0, 0);
        E->lane[1] = E->oneWay ? -1 : BuildLane(e, 1, 0);
        E->lane2[0] = E->lane2[1] = -1;
        if (E->lanesPerDir == 2) {
            E->lane2[0] = BuildLane(e, 0, 1);
            E->lane2[1] = E->oneWay ? -1 : BuildLane(e, 1, 1);
            for (int d = 0; d < 2; d++)
                if (E->lane[d] >= 0 && E->lane2[d] >= 0) {
                    rdSegs[E->lane[d]].parallel = E->lane2[d];
                    rdSegs[E->lane2[d]].parallel = E->lane[d];
                }
        }
    }
    // Connectors, by lane (like Trafficity's lane connections): from a road with two lanes
    // this way, right turns only from the right-hand lane, left turns only from the left
    // lane, straight on from both (each into its own lane). From a one-lane road: a right turn
    // into the right-hand lane, a left turn into the left lane of a two-lane road, straight on
    // into both (so every lane has a way in). Cars get into lane in time (routes may change
    // lanes: RoadRoute).
    for (int ni = 0; ni < rdNodeCount; ni++) {
        RdNode* nd = &rdNodes[ni];
        for (int i = 0; i < nd->nEdge; i++) {
            const RdEdge* ei = &rdEdges[nd->edge[i]];
            int d = ei->a == ni ? 1 : 0;                               // the direction arriving here
            int ins[2] = {ei->lane[d], ei->lane2[d]};
            bool twoIn = ins[0] >= 0 && ins[1] >= 0;
            for (int k = 0; k < 2; k++) {
                if (ins[k] < 0) continue;
                for (int j = 0; j < nd->nEdge; j++) {
                    if (j == i) continue;
                    const RdEdge* ej = &rdEdges[nd->edge[j]];
                    int od = ej->a == ni ? 0 : 1;                      // the direction leaving here
                    int o0 = ej->lane[od], o1 = ej->lane2[od];
                    if (o0 < 0) continue;
                    if (!rdLaneRules) {   // the old rule: to the lane with the same index (or the only one)
                        int out = (k == 1 && o1 >= 0) ? o1 : o0;
                        BuildConnector(ni, ins[k], out, RD_MAX_TURN);
                        if (k == 0 && ins[1] < 0 && o1 >= 0) BuildConnector(ni, ins[0], o1, RD_MAX_TURN);
                        continue;
                    }
                    int turn = TurnOf(ins[k], o0);                     // -1 left, 0 straight, 1 right
                    if (twoIn) {
                        if ((turn > 0 && k == 1) || (turn < 0 && k == 0)) continue;
                        int out = turn > 0 ? o0 : (turn < 0 ? (o1 >= 0 ? o1 : o0) : (k == 1 && o1 >= 0 ? o1 : o0));
                        BuildConnector(ni, ins[k], out, RD_MAX_TURN);
                    } else if (turn > 0) BuildConnector(ni, ins[k], o0, RD_MAX_TURN);
                    else if (turn < 0) BuildConnector(ni, ins[k], o1 >= 0 ? o1 : o0, RD_MAX_TURN);
                    else {
                        BuildConnector(ni, ins[k], o0, RD_MAX_TURN);
                        if (o1 >= 0) BuildConnector(ni, ins[k], o1, RD_MAX_TURN);
                    }
                }
            }
        }
        // Sharper turns (up to ~150 degrees) only where a lane would otherwise have no way
        // out or no way in (roads meeting at a sharp angle)
        for (int i = 0; i < nd->nEdge; i++) {
            const RdEdge* ei = &rdEdges[nd->edge[i]];
            int d = ei->a == ni ? 1 : 0, od0 = ei->a == ni ? 0 : 1;
            int lanesIn[2] = {ei->lane[d], ei->lane2[d]}, lanesOut[2] = {ei->lane[od0], ei->lane2[od0]};
            for (int k = 0; k < 2; k++) {
                int in = lanesIn[k];
                if (in >= 0 && rdSegs[in].nOut == 0)
                    for (int j = 0; j < nd->nEdge; j++) {
                        if (j == i) continue;
                        const RdEdge* ej = &rdEdges[nd->edge[j]];
                        int out = ej->a == ni ? ej->lane[0] : ej->lane[1];
                        if (out >= 0) BuildConnector(ni, in, out, RD_MAX_TURN_SHARP);
                    }
                int out = lanesOut[k];
                if (out < 0) continue;
                bool fed = false;
                for (int q = 0; q < nd->nConn && !fed; q++) if (rdSegs[nd->conn[q]].to == out) fed = true;
                if (!fed)
                    for (int j = 0; j < nd->nEdge; j++) {
                        if (j == i) continue;
                        const RdEdge* ej = &rdEdges[nd->edge[j]];
                        int in2 = ej->a == ni ? ej->lane[1] : ej->lane[0];
                        if (in2 >= 0) BuildConnector(ni, in2, out, RD_MAX_TURN_SHARP);
                    }
            }
        }
        for (int i = 0; i < nd->nConn; i++)
            for (int j = 0; j < nd->nConn; j++)
                nd->conflict[i][j] = (i != j) && ConnectorsConflict(nd->conn[i], nd->conn[j]);
    }
    // Parking bays: one per 16 cells of lane (18+ cells), up to 4, spread between its ends
    for (int i = 0; i < rdLaneCount; i++) {
        int l = rdLanes[i];
        if (rdSegs[l].laneIdx != 0) continue;   // only along the right-hand lane
        float len = rdSegs[l].len / CELL_SIZE;
        int nb = len < 18 ? 0 : (int)((len - 2) / 16);   // one per 16 cells of lane (long roads: more parking), up to 4
        if (nb > 4) nb = 4;
        for (int b = 0; b < nb; b++) {
            float mid = len * (b + 1) / (nb + 1);
            BuildBay(l, (mid - RD_BAY_LEN * 0.5f) * CELL_SIZE);
        }
    }
    for (int s = 0; s < rdSegCount; s++) rdSegs[s].vmax = SegVmax(s, latAccel);
}

void RoadPointAt(int seg, float s, float* x, float* y, float* h) {
    const RdSeg* sg = &rdSegs[seg];
    if (s <= 0) { *x = rdX[sg->p0]; *y = rdY[sg->p0]; *h = rdH[sg->p0]; return; }
    if (s >= sg->len) { int k = sg->p0 + sg->n - 1; *x = rdX[k]; *y = rdY[k]; *h = rdH[k]; return; }
    int i = (int)(s / RD_STEP);          // points are RD_STEP apart (the last one closer)
    if (i > sg->n - 2) i = sg->n - 2;
    while (i > 0 && rdS[sg->p0 + i] > s) i--;
    while (i < sg->n - 2 && rdS[sg->p0 + i + 1] < s) i++;
    int k = sg->p0 + i;
    float L = rdS[k + 1] - rdS[k], t = L > 1e-6f ? (s - rdS[k]) / L : 0;
    *x = rdX[k] + (rdX[k + 1] - rdX[k]) * t;
    *y = rdY[k] + (rdY[k + 1] - rdY[k]) * t;
    *h = rdH[k];   // (stored when built: atan2 here was the top cost with 16000 cars)
}

// ---------------------------------------------------------------------------
// Routing: Dijkstra over lanes (a lane's cost = its length + the connector into it)
// ---------------------------------------------------------------------------
// A binary heap of (dist, lane) for RoadRoute; stale entries are skipped on pop
typedef struct { float d; int l; } RdHeapItem;
static RdHeapItem rdHeap[RD_MAX_SEGS * 2];
static int rdHeapN = 0;
static void RdHeapPush(float d, int l) {
    if (rdHeapN >= RD_MAX_SEGS * 2) return;
    int i = rdHeapN++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (rdHeap[p].d <= d) break;
        rdHeap[i] = rdHeap[p];
        i = p;
    }
    rdHeap[i] = (RdHeapItem){d, l};
}
static RdHeapItem RdHeapPop(void) {
    RdHeapItem top = rdHeap[0], last = rdHeap[--rdHeapN];
    int i = 0;
    for (;;) {
        int a = 2 * i + 1, b = a + 1, m = i;
        float md = last.d;
        if (a < rdHeapN && rdHeap[a].d < md) { m = a; md = rdHeap[a].d; }
        if (b < rdHeapN && rdHeap[b].d < md) { m = b; }
        if (m == i) break;
        rdHeap[i] = rdHeap[m];
        i = m;
    }
    rdHeap[i] = last;
    return top;
}

int RoadRoute(int fromLane, int toLane, RdSegId* out, int max) {
    static float dist[RD_MAX_SEGS];
    static int prevLane[RD_MAX_SEGS], prevConn[RD_MAX_SEGS];
    static unsigned char done[RD_MAX_SEGS];
    for (int i = 0; i < rdLaneCount; i++) {
        int l = rdLanes[i];
        dist[l] = 1e30f; prevLane[l] = -1; prevConn[l] = -1; done[l] = 0;
    }
    rdHeapN = 0;
    // A*: a lane's cost counts to its end; the guess is the straight line from there to the
    // goal lane's end (a route is never shorter, so the first time the goal comes off the
    // heap it's the best route)
    const RdSeg* G = &rdSegs[toLane];
    float gx = rdX[G->p0 + G->n - 1], gy = rdY[G->p0 + G->n - 1];
#define RD_GUESS(l) sqrtf((rdX[rdSegs[l].p0 + rdSegs[l].n - 1] - gx) * (rdX[rdSegs[l].p0 + rdSegs[l].n - 1] - gx) + \
                          (rdY[rdSegs[l].p0 + rdSegs[l].n - 1] - gy) * (rdY[rdSegs[l].p0 + rdSegs[l].n - 1] - gy))
    // Start: the lanes reachable from fromLane's end (fromLane itself only by a loop)
    const RdSeg* f = &rdSegs[fromLane];
    for (int k = 0; k < f->nOut; k++) {
        int c = f->out[k], l = rdSegs[c].to;
        float d = rdSegs[c].len + rdSegs[l].len + rdSegExtra[l];
        if (d < dist[l]) { dist[l] = d; prevLane[l] = fromLane; prevConn[l] = c; RdHeapPush(d + RD_GUESS(l), l); }
    }
    if (rdLaneRules && f->parallel >= 0) {   // changing to the lane beside us (prevConn -2: a lane change, no curve)
        int l = f->parallel;
        float d = RD_LANE_CHANGE_COST * CELL_SIZE + rdSegExtra[l];
        if (d < dist[l]) { dist[l] = d; prevLane[l] = fromLane; prevConn[l] = -2; RdHeapPush(d + RD_GUESS(l), l); }
    }
    for (;;) {
        if (rdHeapN == 0) return 0;
        RdHeapItem it = RdHeapPop();
        int best = it.l;
        if (done[best]) continue;
        if (best == toLane) break;
        done[best] = 1;
        const RdSeg* b = &rdSegs[best];
        for (int k = 0; k < b->nOut; k++) {
            int c = b->out[k], l = rdSegs[c].to;
            float d = dist[best] + rdSegs[c].len + rdSegs[l].len + rdSegExtra[l];
            if (!done[l] && d < dist[l]) { dist[l] = d; prevLane[l] = best; prevConn[l] = c; RdHeapPush(d + RD_GUESS(l), l); }
        }
        if (rdLaneRules && b->parallel >= 0) {
            int l = b->parallel;
            float d = dist[best] + RD_LANE_CHANGE_COST * CELL_SIZE + rdSegExtra[l];
            if (!done[l] && d < dist[l]) { dist[l] = d; prevLane[l] = best; prevConn[l] = -2; RdHeapPush(d + RD_GUESS(l), l); }
        }
    }
#undef RD_GUESS
    // Walk back: toLane, conn, lane, ..., fromLane (stop when we reach fromLane the first time
    // we walk off the start's successors; fromLane == toLane loops round once)
    static RdSegId rev[RD_ROUTE_TMP];
    int n = 0, l = toLane;
    rev[n++] = (RdSegId)l;
    for (;;) {
        int c = prevConn[l], p = prevLane[l];
        if (c == -1 || p < 0 || n + 2 >= RD_ROUTE_TMP) return 0;
        if (c >= 0) rev[n++] = (RdSegId)c;   // (-2: a lane change, lane to lane)
        rev[n++] = (RdSegId)p;
        if (p == fromLane) break;
        l = p;
    }
    if (n > max) return 0;
    for (int i = 0; i < n; i++) out[i] = rev[n - 1 - i];
    return n;
}

int RoadLaneCount(void) { return rdLaneCount; }
int RoadLaneAt(int i) { return rdLanes[i]; }

// ---------------------------------------------------------------------------
// Carving: every road a RD_ROAD_WIDTH band along its centre line, junctions a disc
// ---------------------------------------------------------------------------
static void CarveDisc(float cx, float cy, float r, int z) {
    for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
        for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
            if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy > r * r) continue;
            GRID(x, y, z) = CELL_AIR;
            SetWallMaterial(x, y, z, MAT_NONE);
            if (z > 0) SetVegetation(x, y, z - 1, VEG_NONE);
        }
}

void RoadCarveDisc(float cx, float cy, float r, int z) { CarveDisc(cx, cy, r, z); }

// Half the carved width of a road (cells): 3 per lane, + half a cell (slanted roads are
// carved in whole cells); a one-way road is centred on its lanes
static float RoadHalfWidth(const RdEdge* e) {
    if (e->oneWay && !e->arc) return 1.5f * e->lanesPerDir + 1.0f;
    return RD_ROAD_WIDTH * 0.5f * e->lanesPerDir + 0.5f;
}

void RoadsCarve(int z) {
    for (int e = 0; e < rdEdgeCount; e++) {
        int n = EdgeCentre(&rdEdges[e]);
        for (int i = 0; i < n; i += 2)
            CarveDisc(tmpX[i] / CELL_SIZE, tmpY[i] / CELL_SIZE, RoadHalfWidth(&rdEdges[e]), z);
    }
    for (int i = 0; i < rdNodeCount; i++)
        if (rdNodes[i].nEdge > 0 && !rdNodes[i].ring)   // (a roundabout's ring junctions: the island stays)
            CarveDisc(rdNodes[i].x / CELL_SIZE, rdNodes[i].y / CELL_SIZE, rdNodes[i].setback + 1.0f, z);
    // Room round every route a car can drive (lanes and junction curves): the body
    // (half-diagonal ~1.4 cells) always has ~0.5 cell to spare, whatever the angles
    for (int s = 0; s < rdSegCount; s++)
        for (int i = 0; i < rdSegs[s].n; i++)
            CarveDisc(rdX[rdSegs[s].p0 + i] / CELL_SIZE, rdY[rdSegs[s].p0 + i] / CELL_SIZE, 2.5f, z);
}
