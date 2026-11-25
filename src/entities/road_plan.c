// Road plan: an editable graph of roads. See road_plan.h.
#include "road_plan.h"
#include "roads.h"
#include "road_tiles.h"   // RT_RING_MIN
#include "mover.h"        // CELL_SIZE
#include <math.h>
#include <stdio.h>
#include <string.h>

RpNode rpNodes[RP_MAX_NODES];
RpEdge rpEdges[RP_MAX_EDGES];
int rpNodeCount = 0, rpEdgeCount = 0;

static const char* const rpClassName[RD_CLS_COUNT] = {"lane", "street", "arterial", "highway"};

void RoadPlanClear(void) { rpNodeCount = rpEdgeCount = 0; }

int RoadPlanRoads(void) { int n = 0; for (int e = 0; e < rpEdgeCount; e++) if (rpEdges[e].alive) n++; return n; }

static int RpDegree(int n) {
    int d = 0;
    for (int e = 0; e < rpEdgeCount; e++) if (rpEdges[e].alive && (rpEdges[e].a == n || rpEdges[e].b == n)) d++;
    return d;
}

int RoadPlanJunctions(void) { int n = 0; for (int i = 0; i < rpNodeCount; i++) if (rpNodes[i].alive && RpDegree(i) > 0) n++; return n; }

void RoadPlanPoint(int e, float t, float* x, float* y) {
    const RpEdge* E = &rpEdges[e];
    const RpNode *A = &rpNodes[E->a], *B = &rpNodes[E->b];
    float u = 1 - t;
    *x = u * u * A->x + 2 * u * t * E->cx + t * t * B->x;
    *y = u * u * A->y + 2 * u * t * E->cy + t * t * B->y;
}

static int RpNewNode(float x, float y) {
    if (rpNodeCount >= RP_MAX_NODES) return -1;
    rpNodes[rpNodeCount] = (RpNode){x, y, false, true};
    return rpNodeCount++;
}

static int RpNewEdge(int a, int b, float cx, float cy, int cls, bool avenue, bool oneWay) {
    if (a < 0 || b < 0 || a == b || rpEdgeCount >= RP_MAX_EDGES) return -1;
    for (int e = 0; e < rpEdgeCount; e++)   // (already there: one road between two junctions)
        if (rpEdges[e].alive && ((rpEdges[e].a == a && rpEdges[e].b == b) || (rpEdges[e].a == b && rpEdges[e].b == a))) return -1;
    rpEdges[rpEdgeCount] = (RpEdge){a, b, cx, cy, cls, avenue, oneWay, true};
    return rpEdgeCount++;
}

int RoadPlanNearestNode(float x, float y, float within) {
    int best = -1;
    float bd = within * within;
    for (int i = 0; i < rpNodeCount; i++) {
        if (!rpNodes[i].alive) continue;
        float dx = rpNodes[i].x - x, dy = rpNodes[i].y - y;
        if (dx * dx + dy * dy <= bd) { bd = dx * dx + dy * dy; best = i; }
    }
    return best;
}

#define RP_SAMPLES 48
int RoadPlanNearestEdge(float x, float y, float within, float* tOut) {
    int best = -1;
    float bd = within * within, bt = 0;
    for (int e = 0; e < rpEdgeCount; e++) {
        if (!rpEdges[e].alive) continue;
        for (int i = 0; i <= RP_SAMPLES; i++) {
            float t = (float)i / RP_SAMPLES, px, py;
            RoadPlanPoint(e, t, &px, &py);
            float d = (px - x) * (px - x) + (py - y) * (py - y);
            if (d < bd) { bd = d; best = e; bt = t; }
        }
    }
    if (tOut) *tOut = bt;
    return best;
}

// Split road e at t: a junction there, the two halves exact sub-curves (de Casteljau)
static int RpSplit(int e, float t) {
    RpEdge E = rpEdges[e];
    const RpNode *A = &rpNodes[E.a], *B = &rpNodes[E.b];
    float q0x = A->x + (E.cx - A->x) * t, q0y = A->y + (E.cy - A->y) * t;
    float q1x = E.cx + (B->x - E.cx) * t, q1y = E.cy + (B->y - E.cy) * t;
    int m = RpNewNode(q0x + (q1x - q0x) * t, q0y + (q1y - q0y) * t);
    if (m < 0) return -1;
    rpEdges[e].b = m; rpEdges[e].cx = q0x; rpEdges[e].cy = q0y;
    RpNewEdge(m, E.b, q1x, q1y, E.cls, E.avenue, E.oneWay);
    return m;
}

// The junction for a road's end at (x, y): one near it, or a road near it split there, or new
static int RpJunctionAt(float x, float y) {
    int n = RoadPlanNearestNode(x, y, RP_MERGE);
    if (n >= 0) return n;
    float t;
    int e = RoadPlanNearestEdge(x, y, RP_MERGE, &t);
    if (e >= 0 && t > 0.02f && t < 0.98f) return RpSplit(e, t);
    return RpNewNode(x, y);
}

// Where segments p1-p2 and p3-p4 cross: s along the first, u along the second (0..1)
static bool RpSegCross(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float* s, float* u) {
    float d = (x2 - x1) * (y4 - y3) - (y2 - y1) * (x4 - x3);
    float l1 = hypotf(x2 - x1, y2 - y1), l2 = hypotf(x4 - x3, y4 - y3);
    if (fabsf(d) <= 1e-3f * l1 * l2) return false;   // (parallel, or near enough: one road along another doesn't cross it)
    *s = ((x3 - x1) * (y4 - y3) - (y3 - y1) * (x4 - x3)) / d;
    *u = ((x3 - x1) * (y2 - y1) - (y3 - y1) * (x2 - x1)) / d;
    return *s >= 0 && *s <= 1 && *u >= 0 && *u <= 1;
}

int RoadPlanAdd(float x1, float y1, float x2, float y2, bool curved, float cx, float cy, int cls) {
    if (cls < 0 || cls >= RD_CLS_COUNT) cls = RD_CLS_STREET;
    if (!curved) { cx = (x1 + x2) * 0.5f; cy = (y1 + y2) * 0.5f; }
    // where it crosses the roads there now (before its own ends split any of them)
    float P0x = x1, P0y = y1, P1x = x2, P1y = y2;
    #define RP_NEW_N 64
    float nx[RP_NEW_N + 1], ny[RP_NEW_N + 1];
    for (int i = 0; i <= RP_NEW_N; i++) {
        float t = (float)i / RP_NEW_N, u = 1 - t;
        nx[i] = u * u * P0x + 2 * u * t * cx + t * t * P1x;
        ny[i] = u * u * P0y + 2 * u * t * cy + t * t * P1y;
    }
    float crossT[64], crossX[64], crossY[64];
    int nCross = 0;
    for (int e = 0; e < rpEdgeCount && nCross < 64; e++) {
        if (!rpEdges[e].alive) continue;
        float ox[RP_SAMPLES + 1], oy[RP_SAMPLES + 1];
        for (int j = 0; j <= RP_SAMPLES; j++) RoadPlanPoint(e, (float)j / RP_SAMPLES, &ox[j], &oy[j]);
        for (int i = 0; i < RP_NEW_N && nCross < 64; i++)
            for (int j = 0; j < RP_SAMPLES && nCross < 64; j++) {
                float s, u;
                if (!RpSegCross(nx[i], ny[i], nx[i + 1], ny[i + 1], ox[j], oy[j], ox[j + 1], oy[j + 1], &s, &u)) continue;
                float px = nx[i] + (nx[i + 1] - nx[i]) * s, py = ny[i] + (ny[i + 1] - ny[i]) * s;
                if (hypotf(px - P0x, py - P0y) < RP_MERGE || hypotf(px - P1x, py - P1y) < RP_MERGE) continue;   // (at an end: the end joins it)
                crossT[nCross] = (i + s) / RP_NEW_N; crossX[nCross] = px; crossY[nCross] = py; nCross++;
            }
    }
    // the junctions along it, in order: its start, every crossing, its end
    for (int i = 1; i < nCross; i++)
        for (int j = i; j > 0 && crossT[j] < crossT[j - 1]; j--) {
            float t;
            t = crossT[j]; crossT[j] = crossT[j - 1]; crossT[j - 1] = t;
            t = crossX[j]; crossX[j] = crossX[j - 1]; crossX[j - 1] = t;
            t = crossY[j]; crossY[j] = crossY[j - 1]; crossY[j - 1] = t;
        }
    int ids[66]; float ts[66]; int n = 0;
    ids[n] = RpJunctionAt(x1, y1); ts[n++] = 0;
    for (int k = 0; k < nCross; k++) {
        int id = RpJunctionAt(crossX[k], crossY[k]);
        if (id < 0 || id == ids[n - 1]) continue;
        ids[n] = id; ts[n++] = crossT[k];
    }
    int last = RpJunctionAt(x2, y2);
    if (last >= 0 && last != ids[n - 1]) { ids[n] = last; ts[n++] = 1; }
    // the pieces between them: each the exact sub-curve (control: start + its span x the half tangent)
    int added = 0;
    for (int k = 0; k + 1 < n; k++) {
        float t0 = ts[k], t1 = ts[k + 1], u0 = 1 - t0;
        float bx = u0 * u0 * P0x + 2 * u0 * t0 * cx + t0 * t0 * P1x, by = u0 * u0 * P0y + 2 * u0 * t0 * cy + t0 * t0 * P1y;
        float dx = u0 * (cx - P0x) + t0 * (P1x - cx), dy = u0 * (cy - P0y) + t0 * (P1y - cy);
        float pcx = bx + (t1 - t0) * dx, pcy = by + (t1 - t0) * dy;
        if (!curved) { pcx = (rpNodes[ids[k]].x + rpNodes[ids[k + 1]].x) * 0.5f; pcy = (rpNodes[ids[k]].y + rpNodes[ids[k + 1]].y) * 0.5f; }
        if (RpNewEdge(ids[k], ids[k + 1], pcx, pcy, cls, cls == RD_CLS_HIGHWAY, false) >= 0) added++;
    }
    return added;
}

// Tangent of road e leaving node n (unit)
static void RpTangent(int e, int n, float* tx, float* ty) {
    const RpEdge* E = &rpEdges[e];
    const RpNode* N = &rpNodes[n];
    const RpNode* O = &rpNodes[E->a == n ? E->b : E->a];
    float dx = E->cx - N->x, dy = E->cy - N->y;
    if (dx * dx + dy * dy < 0.01f) { dx = O->x - N->x; dy = O->y - N->y; }
    float L = sqrtf(dx * dx + dy * dy);
    *tx = L > 0 ? dx / L : 1; *ty = L > 0 ? dy / L : 0;
}

static bool RpStraight(int e) {   // the control point on the line between its ends
    const RpEdge* E = &rpEdges[e];
    const RpNode *A = &rpNodes[E->a], *B = &rpNodes[E->b];
    float dx = B->x - A->x, dy = B->y - A->y, L = sqrtf(dx * dx + dy * dy);
    if (L < 1e-3f) return true;
    return fabsf((E->cx - A->x) * dy - (E->cy - A->y) * dx) / L < 0.5f;
}

bool RoadPlanRemoveAt(float x, float y, float within) {
    int e = RoadPlanNearestEdge(x, y, within, NULL);
    if (e < 0) return false;
    rpEdges[e].alive = false;
    int ends[2] = {rpEdges[e].a, rpEdges[e].b};
    for (int k = 0; k < 2; k++) {
        int n = ends[k];
        int d = RpDegree(n);
        if (d == 0) { rpNodes[n].alive = false; continue; }
        if (d != 2 || rpNodes[n].roundabout) continue;
        // two straight pieces of one road straight on: one road again
        int e1 = -1, e2 = -1;
        for (int q = 0; q < rpEdgeCount; q++) if (rpEdges[q].alive && (rpEdges[q].a == n || rpEdges[q].b == n)) { if (e1 < 0) e1 = q; else e2 = q; }
        if (e1 < 0 || e2 < 0 || !RpStraight(e1) || !RpStraight(e2)) continue;
        const RpEdge *E1 = &rpEdges[e1], *E2 = &rpEdges[e2];
        if (E1->cls != E2->cls || E1->avenue != E2->avenue || E1->oneWay || E2->oneWay) continue;
        float ax, ay, bx, by;
        RpTangent(e1, n, &ax, &ay); RpTangent(e2, n, &bx, &by);
        if (ax * bx + ay * by > -0.98f) continue;   // (a bend: keep the junction)
        int o1 = E1->a == n ? E1->b : E1->a, o2 = E2->a == n ? E2->b : E2->a;
        int cls = E1->cls; bool av = E1->avenue;
        rpEdges[e1].alive = rpEdges[e2].alive = false;
        rpNodes[n].alive = false;
        RpNewEdge(o1, o2, (rpNodes[o1].x + rpNodes[o2].x) * 0.5f, (rpNodes[o1].y + rpNodes[o2].y) * 0.5f, cls, av, false);
    }
    return true;
}

bool RoadPlanSetRoundabout(float x, float y, bool on) {
    int n = RoadPlanNearestNode(x, y, RP_MERGE);
    if (n < 0) return false;
    rpNodes[n].roundabout = on;
    return true;
}

float RoadPlanMinRadius(int cls) {
    switch (cls) {
        case RD_CLS_HIGHWAY: return 24.0f;
        case RD_CLS_ARTERIAL: return 10.0f;
        default: return 6.0f;
    }
}

static float RpLength(int e) {
    float L = 0, px, py, qx, qy;
    RoadPlanPoint(e, 0, &px, &py);
    for (int i = 1; i <= RP_SAMPLES; i++) { RoadPlanPoint(e, (float)i / RP_SAMPLES, &qx, &qy); L += hypotf(qx - px, qy - py); px = qx; py = qy; }
    return L;
}

static float RpMinRadius(int e) {   // the tightest radius along it (curvature of the quadratic)
    const RpEdge* E = &rpEdges[e];
    const RpNode *A = &rpNodes[E->a], *B = &rpNodes[E->b];
    float ddx = 2 * (A->x - 2 * E->cx + B->x), ddy = 2 * (A->y - 2 * E->cy + B->y), best = 1e9f;
    for (int i = 0; i <= RP_SAMPLES; i++) {
        float t = (float)i / RP_SAMPLES;
        float dx = 2 * ((1 - t) * (E->cx - A->x) + t * (B->x - E->cx)), dy = 2 * ((1 - t) * (E->cy - A->y) + t * (B->y - E->cy));
        float cr = fabsf(dx * ddy - dy * ddx), v = sqrtf(dx * dx + dy * dy);
        if (cr > 1e-6f) { float r = v * v * v / cr; if (r < best) best = r; }
    }
    return best;
}

// A junction a road just passes through straight on (two pieces of one road): not a real one
static bool RpPassThrough(int n) {
    if (RpDegree(n) != 2 || rpNodes[n].roundabout) return false;
    int e1 = -1, e2 = -1;
    for (int q = 0; q < rpEdgeCount; q++) if (rpEdges[q].alive && (rpEdges[q].a == n || rpEdges[q].b == n)) { if (e1 < 0) e1 = q; else e2 = q; }
    float ax, ay, bx, by;
    RpTangent(e1, n, &ax, &ay); RpTangent(e2, n, &bx, &by);
    return ax * bx + ay * by < -0.94f;   // (within ~20 degrees of straight on)
}

int RoadPlanCheck(char lines[][128], int max) {
    int k = 0;
    for (int e = 0; e < rpEdgeCount && k < max; e++) {
        if (!rpEdges[e].alive) continue;
        const RpNode *A = &rpNodes[rpEdges[e].a], *B = &rpNodes[rpEdges[e].b];
        float L = RpLength(e);
        if (L < RP_MIN_LEN && !RpPassThrough(rpEdges[e].a) && !RpPassThrough(rpEdges[e].b))
            snprintf(lines[k++], 128, "road (%.0f,%.0f)-(%.0f,%.0f): %.0f cells between junctions (want %.0f)", A->x, A->y, B->x, B->y, L, RP_MIN_LEN);
        float r = RpMinRadius(e), rmin = RoadPlanMinRadius(rpEdges[e].cls);
        if (k < max && r < rmin)
            snprintf(lines[k++], 128, "road (%.0f,%.0f)-(%.0f,%.0f): a curve of radius %.0f cells (a %s wants %.0f)", A->x, A->y, B->x, B->y, r, rpClassName[rpEdges[e].cls], rmin);
    }
    for (int n = 0; n < rpNodeCount && k < max; n++) {
        if (!rpNodes[n].alive) continue;
        int es[16], d = 0;
        for (int q = 0; q < rpEdgeCount && d < 16; q++) if (rpEdges[q].alive && (rpEdges[q].a == n || rpEdges[q].b == n)) es[d++] = q;
        if (d > RD_MAX_NODE_EDGES) snprintf(lines[k++], 128, "junction (%.0f,%.0f): %d roads (at most %d)", rpNodes[n].x, rpNodes[n].y, d, RD_MAX_NODE_EDGES);
        if (k < max && rpNodes[n].roundabout && d < 3) snprintf(lines[k++], 128, "roundabout (%.0f,%.0f): %d roads (wants 3 or more)", rpNodes[n].x, rpNodes[n].y, d);
        if (k < max && d == 1) snprintf(lines[k++], 128, "dead end at (%.0f,%.0f)", rpNodes[n].x, rpNodes[n].y);
        float worst = 180;
        for (int i = 0; i < d; i++)
            for (int j = i + 1; j < d; j++) {
                float ax, ay, bx, by;
                RpTangent(es[i], n, &ax, &ay); RpTangent(es[j], n, &bx, &by);
                float c = ax * bx + ay * by, a = acosf(c < -1 ? -1 : c > 1 ? 1 : c) * 57.29578f;
                if (a < worst) worst = a;
            }
        if (k < max && d >= 2 && worst < RP_MIN_ANGLE)
            snprintf(lines[k++], 128, "junction (%.0f,%.0f): roads %.0f degrees apart (want %.0f)", rpNodes[n].x, rpNodes[n].y, worst, RP_MIN_ANGLE);
    }
    return k;
}

// One saved copy of the plan (a drag's start: right-click puts it back)
static RpNode rpSaveN[RP_MAX_NODES];
static RpEdge rpSaveE[RP_MAX_EDGES];
static int rpSaveNN = -1, rpSaveNE = 0;
void RoadPlanSave(void) {
    memcpy(rpSaveN, rpNodes, sizeof(RpNode) * (size_t)rpNodeCount);
    memcpy(rpSaveE, rpEdges, sizeof(RpEdge) * (size_t)rpEdgeCount);
    rpSaveNN = rpNodeCount; rpSaveNE = rpEdgeCount;
}
void RoadPlanRestore(void) {
    if (rpSaveNN < 0) return;
    memcpy(rpNodes, rpSaveN, sizeof(RpNode) * (size_t)rpSaveNN);
    memcpy(rpEdges, rpSaveE, sizeof(RpEdge) * (size_t)rpSaveNE);
    rpNodeCount = rpSaveNN; rpEdgeCount = rpSaveNE;
}

bool RoadPlanIsStraight(int e) { return e >= 0 && e < rpEdgeCount && RpStraight(e); }

void RoadPlanEdgeMid(int e, float* x, float* y) { RoadPlanPoint(e, 0.5f, x, y); }

// Bend road e so its middle passes through (x, y) (B(0.5) = P0/4 + C/2 + P1/4); within a cell
// of the straight line's middle: straight again
void RoadPlanBendThrough(int e, float x, float y, float straightWithin) {
    if (e < 0 || e >= rpEdgeCount || !rpEdges[e].alive) return;
    const RpNode *A = &rpNodes[rpEdges[e].a], *B = &rpNodes[rpEdges[e].b];
    float mx = (A->x + B->x) * 0.5f, my = (A->y + B->y) * 0.5f;
    if (hypotf(x - mx, y - my) < straightWithin) { rpEdges[e].cx = mx; rpEdges[e].cy = my; return; }
    rpEdges[e].cx = 2.0f * x - mx;
    rpEdges[e].cy = 2.0f * y - my;
}

// Move junction n to (x, y): its straight roads stay straight, a curved one's control point moves
// half as far (it keeps roughly its shape)
void RoadPlanMoveNode(int n, float x, float y) {
    if (n < 0 || n >= rpNodeCount || !rpNodes[n].alive) return;
    float dx = x - rpNodes[n].x, dy = y - rpNodes[n].y;
    bool straight[64];
    int es[64], k = 0;
    for (int e = 0; e < rpEdgeCount && k < 64; e++)
        if (rpEdges[e].alive && (rpEdges[e].a == n || rpEdges[e].b == n)) { es[k] = e; straight[k] = RpStraight(e); k++; }
    rpNodes[n].x = x; rpNodes[n].y = y;
    for (int i = 0; i < k; i++) {
        RpEdge* E = &rpEdges[es[i]];
        if (straight[i]) { E->cx = (rpNodes[E->a].x + rpNodes[E->b].x) * 0.5f; E->cy = (rpNodes[E->a].y + rpNodes[E->b].y) * 0.5f; }
        else { E->cx += dx * 0.5f; E->cy += dy * 0.5f; }
    }
}

// The road whose middle handle is within `within` of (x, y), -1: none
int RoadPlanHandleAt(float x, float y, float within) {
    int best = -1;
    float bd = within * within;
    for (int e = 0; e < rpEdgeCount; e++) {
        if (!rpEdges[e].alive) continue;
        float hx, hy;
        RoadPlanEdgeMid(e, &hx, &hy);
        float d = (hx - x) * (hx - x) + (hy - y) * (hy - y);
        if (d <= bd) { bd = d; best = e; }
    }
    return best;
}

// Snap a point being drawn: to a junction near it, else onto a road near it, else (unless free)
// to the 8-cell grid's tile centres (where the tile town's roads run)
void RoadPlanSnap(float* x, float* y, bool free) {
    int n = RoadPlanNearestNode(*x, *y, 4.0f);
    if (n >= 0) { *x = rpNodes[n].x; *y = rpNodes[n].y; return; }
    float t;
    int e = RoadPlanNearestEdge(*x, *y, 3.0f, &t);
    if (e >= 0) { RoadPlanPoint(e, t, x, y); return; }
    if (free) return;
    *x = floorf(*x / 8.0f) * 8.0f + 4.0f;
    *y = floorf(*y / 8.0f) * 8.0f + 4.0f;
}

// What adding this road would break: the check's new problems (after minus before), the first
// one into why; the plan is left as it was
int RoadPlanTry(float x1, float y1, float x2, float y2, bool curved, float cx, float cy, int cls, char* why, int whyLen) {
    static RpNode saveN[RP_MAX_NODES];
    static RpEdge saveE[RP_MAX_EDGES];
    static char before[32][128], after[32][128];
    int nn = rpNodeCount, ne = rpEdgeCount;
    memcpy(saveN, rpNodes, sizeof(RpNode) * (size_t)nn);
    memcpy(saveE, rpEdges, sizeof(RpEdge) * (size_t)ne);
    int b = RoadPlanCheck(before, 32);
    RoadPlanAdd(x1, y1, x2, y2, curved, cx, cy, cls);
    int a = RoadPlanCheck(after, 32);
    int fresh = 0;
    if (why && whyLen > 0) why[0] = 0;
    for (int i = 0; i < a; i++) {   // (problems not there before)
        bool old = false;
        for (int j = 0; j < b && !old; j++) if (strcmp(after[i], before[j]) == 0) old = true;
        if (old) continue;
        if (fresh == 0 && why) snprintf(why, (size_t)whyLen, "%s", after[i]);
        fresh++;
    }
    memcpy(rpNodes, saveN, sizeof(RpNode) * (size_t)nn);
    memcpy(rpEdges, saveE, sizeof(RpEdge) * (size_t)ne);
    rpNodeCount = nn; rpEdgeCount = ne;
    return fresh;
}

int RoadPlanToNetwork(void) {
    RoadsClear();
    static int map[RP_MAX_NODES];
    for (int n = 0; n < rpNodeCount; n++) {
        map[n] = -1;
        if (rpNodes[n].alive && RpDegree(n) > 0) map[n] = RoadAddNode(rpNodes[n].x, rpNodes[n].y);
    }
    int made = 0;
    for (int e = 0; e < rpEdgeCount; e++) {
        const RpEdge* E = &rpEdges[e];
        if (!E->alive || map[E->a] < 0 || map[E->b] < 0) continue;
        int re = RoadAddEdge(map[E->a], map[E->b], 0);
        if (re < 0) continue;
        if (!RpStraight(e)) RoadSetControl(re, E->cx, E->cy);
        if (E->avenue) RoadSetAvenue(re);
        RoadSetClass(re, E->cls);
        if (E->oneWay) RoadSetOneWay(re);
        made++;
    }
    for (int n = 0; n < rpNodeCount; n++) {
        if (!rpNodes[n].roundabout || map[n] < 0) continue;
        bool ok = false;
        for (float r = RT_RING_MIN; r <= 12.0f && !ok; r += 1.0f) ok = RoadMakeRoundabout(map[n], r);
        if (!ok) printf("road plan: no roundabout at (%.0f,%.0f) - fewer than 3 roads, or too close in angle\n", rpNodes[n].x, rpNodes[n].y);
    }
    return made;
}

void RoadPlanFromNetwork(const bool* roundaboutNode) {
    RoadPlanClear();
    for (int n = 0; n < rdNodeCount && n < RP_MAX_NODES; n++) {
        rpNodes[n] = (RpNode){rdNodes[n].x / CELL_SIZE, rdNodes[n].y / CELL_SIZE, roundaboutNode && roundaboutNode[n], true};
        rpNodeCount = n + 1;
    }
    for (int e = 0; e < rdEdgeCount && e < RP_MAX_EDGES; e++) {
        const RdEdge* E = &rdEdges[e];
        float ax = rdNodes[E->a].x, ay = rdNodes[E->a].y, bx = rdNodes[E->b].x, by = rdNodes[E->b].y;
        float mx = (ax + bx) * 0.5f, my = (ay + by) * 0.5f, dx = bx - ax, dy = by - ay, L = sqrtf(dx * dx + dy * dy);
        float cx = E->hasCtrl ? E->ctrlX : mx + (L > 0 ? -dy / L : 0) * 2.0f * E->bend * CELL_SIZE;
        float cy = E->hasCtrl ? E->ctrlY : my + (L > 0 ? dx / L : 0) * 2.0f * E->bend * CELL_SIZE;
        rpEdges[e] = (RpEdge){E->a, E->b, cx / CELL_SIZE, cy / CELL_SIZE, E->cls, E->lanesPerDir == 2, E->oneWay, true};
        rpEdgeCount = e + 1;
    }
}

void RoadPlanPrint(void) {
    for (int e = 0; e < rpEdgeCount; e++) {
        const RpEdge* E = &rpEdges[e];
        if (!E->alive) continue;
        const RpNode *A = &rpNodes[E->a], *B = &rpNodes[E->b];
        if (RpStraight(e)) printf("road add %s %.0f %.0f %.0f %.0f\n", rpClassName[E->cls], A->x, A->y, B->x, B->y);
        else printf("road add %s %.0f %.0f %.0f %.0f %.1f %.1f\n", rpClassName[E->cls], A->x, A->y, B->x, B->y, E->cx, E->cy);
    }
    for (int n = 0; n < rpNodeCount; n++)
        if (rpNodes[n].alive && rpNodes[n].roundabout) printf("road roundabout %.0f %.0f\n", rpNodes[n].x, rpNodes[n].y);
}
