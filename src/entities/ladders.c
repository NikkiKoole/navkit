// ladders.c - single-file ladders: one climber per ladder segment, lines at the
// ends, the queue's route cost, climbing speed (see ladders.h)
//
// A ladder segment (one ladder cell, between level zLow and zLow + 1) holds one
// climber at a time. Climbers are found from mover state each tick (on a ladder
// cell, partway between two levels), and lines are service points (service.c),
// so nothing here is saved: lines rebuild themselves after a load.

#include "ladders.h"
#include "mover.h"
#include "service.h"
#include "../core/time.h"
#include "../core/event_log.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/pathfinding.h"
#include "../simulation/balance.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool useSingleFileLadders = true;    // one climber per ladder segment; others wait at the foot/top
bool useLadderQueueCost = true;      // routes see a ladder's queue as extra cost (pathfinding.h hook)
float ladderClimbSpeed = 0.15f;      // climbing speed vs walking (0.15: a level ~7 cells; slower than built stairs, stairsClimbSpeed)

// Seconds each mover has been in a ladder line
static float moverLadderWait[MAX_MOVERS];
// gameTime a climb started (stall detection; -1 = not climbing)
static double moverClimbStart[MAX_MOVERS];
// The front of a ladder line was held back for the previous climber (so its start
// measures the ladder's service interval)
static bool moverWaitedAtFoot[MAX_MOVERS];
#define LADDER_STALL_TIMEOUT 10.0f  // a climb taking longer than this no longer blocks the segment
#define LADDER_REPLAN_AT_1 3.0f     // a waiting mover re-plans at these wait times: with the
#define LADDER_REPLAN_AT_2 7.0f     // queue cost, another ladder or a ramp may now be quicker
#define LADDER_GIVE_UP 20.0f        // an idle (no job) mover leaves the line and does something else
                                    // (all three scaled by MoverPatience)
#define LADDER_JOIN_RADIUS (CELL_SIZE * 3.0f)     // join a ladder's line this close to its foot
#define LADDER_FOLLOW_RANGE (CELL_SIZE * 2.0f)    // line up behind the one ahead within this
#define LADDER_LINE_SPACING (CELL_SIZE * 0.7f)    // gap kept to the one ahead
#define LADDER_LOOKAHEAD 8                        // waypoints scanned for an upcoming climb

typedef struct { int x, y, zLow; int mover; } LadderSegment;
static LadderSegment ladderClimbers[MAX_MOVERS];
static int ladderClimberCount;
static bool MoverClimbingLadder(const Mover* m, LadderSegment* seg) {
    float zf = floorf(m->z);
    if (m->z - zf < 0.0001f) return false;          // standing on a level
    int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)zf;
    if (z + 1 >= gridDepth || !IsLadderCell(GRID(x, y, z)) || !IsLadderCell(GRID(x, y, z + 1))) return false;
    *seg = (LadderSegment){x, y, z, (int)(m - movers)};
    return true;
}

// Next level change on the mover's route, if it is a ladder on the mover's level
// within the next few waypoints: the foot cell (where it waits) and the target level
static bool UpcomingLadderClimb(int i, Point* foot, int* toZ) {
    const Mover* m = &movers[i];
    int mz = (int)m->z;
    if (m->z != (float)mz || m->pathIndex < 0) return false;
    // At the foot already: the current waypoint is the ladder's other end
    Point t = moverPaths[i][m->pathIndex];
    if (t.z != mz) {
        if (abs(t.z - mz) != 1 || t.z < 0 || t.z >= gridDepth || !IsLadderCell(GRID(t.x, t.y, mz)) ||
            !IsLadderCell(GRID(t.x, t.y, t.z))) return false;
        *foot = (Point){t.x, t.y, mz};
        *toZ = t.z;
        return true;
    }
    for (int k = m->pathIndex; k >= 1 && k > m->pathIndex - LADDER_LOOKAHEAD; k--) {
        Point a = moverPaths[i][k], b = moverPaths[i][k - 1];
        if (a.z != mz) return false;
        if (b.z == a.z) continue;
        if (a.x != b.x || a.y != b.y || !IsLadderCell(GRID(a.x, a.y, a.z)) || !IsLadderCell(GRID(b.x, b.y, b.z))) return false;
        *foot = a;
        *toZ = b.z;
        return true;
    }
    return false;
}

// Once per tick: who is on a ladder segment, and who waits in which ladder's line
// (service points, service.h). A mover joins a ladder's line when its route
// climbs that ladder within a few waypoints and it is within LADDER_JOIN_RADIUS
// of the foot; it leaves when it starts climbing, re-plans away or deactivates.
void LaddersUpdate(float dt) {
    ladderClimberCount = 0;
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        LadderSegment seg;
        if (m->active && MoverClimbingLadder(m, &seg)) ladderClimbers[ladderClimberCount++] = seg;
    }
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        int sp = ServiceOf(i);
        Point foot;
        int toZ;
        bool climbing = m->z - floorf(m->z) > 0.0001f;
        bool wants = m->active && !climbing && UpcomingLadderClimb(i, &foot, &toZ);
        if (sp >= 0 && servicePoints[sp].kind == SERVICE_LADDER) {
            const ServicePoint* p = &servicePoints[sp];
            const ServiceEntry* e = ServiceEntryOf(sp, i);
            int segLow = wants ? (toZ < foot.z ? toZ : foot.z) : -1;
            bool same = wants && e && p->x == foot.x && p->y == foot.y && p->z == segLow && e->end == foot.z;
            if (!same) { ServiceLeave(sp, i); moverLadderWait[i] = 0.0f; }
            else {
                float before = moverLadderWait[i];
                moverLadderWait[i] += dt;
                float pat = MoverPatience(m);
                float r1 = LADDER_REPLAN_AT_1 * pat, r2 = LADDER_REPLAN_AT_2 * pat, quit = LADDER_GIVE_UP * pat;
                Mover* mw = &movers[i];
                if (before < quit && moverLadderWait[i] >= quit && endlessMoverMode && mw->currentJobId < 0 &&
                    mw->freetimeState == FREETIME_NONE && ServiceAhead(sp, i) >= 0) {
                    // Fed up (and nothing it has to do): leave the line, go elsewhere
                    EventLog("Mover %d (%s) gave up waiting at the ladder (%d,%d) after %.0fs", i, mw->name,
                             p->x, p->y, moverLadderWait[i]);
                    ServiceLeave(sp, i);
                    moverLadderWait[i] = 0.0f;
                    MoverPickNewGoal(mw);
                    continue;
                }
                if (useLadderQueueCost &&
                    ((before < r1 && moverLadderWait[i] >= r1) || (before < r2 && moverLadderWait[i] >= r2)))
                    mw->needsRepath = true;   // with the queue cost, maybe another way is quicker
                continue;
            }
        }
        if (!wants) continue;
        float fx = foot.x * CELL_SIZE + CELL_SIZE * 0.5f - m->x, fy = foot.y * CELL_SIZE + CELL_SIZE * 0.5f - m->y;
        if (fx * fx + fy * fy > LADDER_JOIN_RADIUS * LADDER_JOIN_RADIUS) continue;
        int segLow = toZ < foot.z ? toZ : foot.z;
        int np = ServiceGet(SERVICE_LADDER, foot.x, foot.y, segLow, 1);
        if (np >= 0 && ServiceJoin(np, i, foot.z, gameTime)) moverLadderWait[i] = 0.0f;
    }
}

// Seconds to walk one cell at the base mover speed; path cost 10 = one cell
static float SecondsPerCell(void) {
    float base = balance.baseMoverSpeed > 0.0f ? balance.baseMoverSpeed : 200.0f;   // 200 = InitBalance default
    float pxPerSec = base * (dayLength > 0.0f ? 60.0f / dayLength : 1.0f);
    return (float)CELL_SIZE / pxPerSec;
}

float LadderClimbSpeedFactor(void) {
    return ladderClimbSpeed > 0.05f ? ladderClimbSpeed : 0.05f;
}

// NAV_CLIMB_EXTRA for the game (see nav_host.h): extra path cost of changing
// level by dz on the ladder at (x,y,z), on top of the pathfinder's own step cost
// (the cell's move cost, i.e. one cell of walking):
//  - slow climbing: a level takes 1/ladderClimbSpeed cells of walking
//  - the queue (useLadderQueueCost): (line at this end + anyone on the segment)
//    x the ladder's measured interval between climbs (until measured: one climb),
//    converted to path cost at the base walking pace
// Uses the state from the latest tick. Ramps: no extra.
int LadderQueueExtraCost(int x, int y, int z, int dz) {
    int zt = z + dz;
    if (zt < 0 || zt >= gridDepth || !IsLadderCell(GRID(x, y, z)) || !IsLadderCell(GRID(x, y, zt))) return 0;
    float slow = 1.0f / LadderClimbSpeedFactor();
    int extra = (int)(GetCellMoveCost(x, y, zt) * (slow - 1.0f) + 0.5f);
    if (extra < 0) extra = 0;
    if (!useLadderQueueCost || !useSingleFileLadders) return extra;
    int segLow = dz > 0 ? z : zt, n = 0;
    for (int k = 0; k < ladderClimberCount; k++)
        if (ladderClimbers[k].x == x && ladderClimbers[k].y == y && ladderClimbers[k].zLow == segLow) n++;
    int sp = ServiceFind(SERVICE_LADDER, x, y, segLow);
    n += ServiceLineLength(sp, z);
    if (n == 0) return extra;
    float perCell = SecondsPerCell();
    float waitSeconds = n * ServiceIntervalOr(sp, perCell * slow);
    return extra + (int)(waitSeconds / perCell * 10.0f + 0.5f);
}

// The front of a ladder line may start climbing when no other end's front that is
// standing ready at its foot arrived earlier (strict arrival order, but nobody
// waits for someone still walking up)
static bool LadderMyTurn(int sp, int i) {
    if (sp < 0) return true;
    if (ServiceAhead(sp, i) >= 0) return false;
    const ServicePoint* p = &servicePoints[sp];
    const ServiceEntry* me = ServiceEntryOf(sp, i);
    if (!me) return true;
    // The line is in arrival order (ties by join order), so only entries before
    // ours can have priority; comparing join times would deadlock on equal ones
    for (int k = 0; k < p->lineCount && p->line[k].mover != i; k++) {
        const ServiceEntry* e = &p->line[k];
        if (e->end == me->end) continue;
        if (ServiceAhead(sp, e->mover) >= 0) continue;              // not a front
        const Mover* o = &movers[e->mover];
        float dx = p->x * CELL_SIZE + CELL_SIZE * 0.5f - o->x, dy = p->y * CELL_SIZE + CELL_SIZE * 0.5f - o->y;
        if ((int)o->z == e->end && dx * dx + dy * dy < (CELL_SIZE * 0.5f) * (CELL_SIZE * 0.5f)) return false;
    }
    return true;
}

// Someone is climbing this segment (a climb stalled past LADDER_STALL_TIMEOUT,
// e.g. a mover stuck mid-ladder, no longer blocks it)
static bool LadderSegmentBusy(int x, int y, int zLow) {
    for (int k = 0; k < ladderClimberCount; k++) {
        const LadderSegment* c = &ladderClimbers[k];
        if (c->x != x || c->y != y || c->zLow != zLow) continue;
        double start = moverClimbStart[c->mover];
        if (start >= 0.0 && gameTime - start > LADDER_STALL_TIMEOUT) continue;
        return true;
    }
    return false;
}

void LaddersClear(void) {
    memset(moverLadderWait, 0, sizeof(moverLadderWait));
    for (int i = 0; i < MAX_MOVERS; i++) moverClimbStart[i] = -1.0;
    memset(moverWaitedAtFoot, 0, sizeof(moverWaitedAtFoot));
    ladderClimberCount = 0;
    ClearServicePoints();
}

// Mover i is at a ladder end (within the arrival radius of `target`, the other
// end) and about to start the segment (x, y, segLow). Returns true if it must
// wait this tick: someone is ahead in its line (stays put), the segment is busy,
// or another end's front arrived earlier. Only the front stands on the foot,
// walking the last bit onto it rather than jumping. Otherwise the climb starts:
// the mover leaves its line and the segment counts as busy from now.
bool LadderHoldAtFoot(int i, Point target, int segLow, float dist, float dxf, float dyf, float effectiveSpeed, float dt) {
    Mover* m = &movers[i];
    int sp = ServiceFind(SERVICE_LADDER, target.x, target.y, segLow);
    if (ServiceOf(i) != sp) sp = -1;          // not in this ladder's line
    bool front = sp < 0 || ServiceAhead(sp, i) < 0;
    bool blocked = !front || LadderSegmentBusy(target.x, target.y, segLow) || !LadderMyTurn(sp, i);
    if (front && dist > 2.0f) {
        float step = fminf(dist, effectiveSpeed * dt);
        m->x += dxf / dist * step;
        m->y += dyf / dist * step;
        blocked = true;
    }
    if (blocked && front && dist <= 2.0f) moverWaitedAtFoot[i] = true;   // held for the previous climber
    if (blocked) {
        m->timeWithoutProgress = 0.0f;  // waiting in line is not being stuck
        m->lastX = m->x;
        m->lastY = m->y;
        return true;
    }
    if (sp >= 0) ServiceLeave(sp, i);
    ServiceRecordStart(ServiceGet(SERVICE_LADDER, target.x, target.y, segLow, 1), gameTime, moverWaitedAtFoot[i]);
    moverWaitedAtFoot[i] = false;
    moverLadderWait[i] = 0.0f;
    moverClimbStart[i] = gameTime;
    if (ladderClimberCount < MAX_MOVERS)
        ladderClimbers[ladderClimberCount++] = (LadderSegment){target.x, target.y, segLow, i};
    return false;
}

void LadderClimbDone(int i) {
    if (i >= 0 && i < MAX_MOVERS) moverClimbStart[i] = -1.0;
}

// In a ladder line behind someone: line up behind the one directly ahead (keep a
// gap; otherwise only avoidance moves us), so the queue takes its own shape
// instead of piling onto the ladder cell. Returns true if the mover is lining up
// (its velocity toward its waypoint is replaced by *vx, *vy).
bool LadderFollowLine(int i, float effectiveSpeed, float* vx, float* vy) {
    Mover* m = &movers[i];
    *vx = *vy = 0.0f;
    int sp = ServiceOf(i);
    int ahead = (sp >= 0 && servicePoints[sp].kind == SERVICE_LADDER) ? ServiceAhead(sp, i) : -1;
    if (ahead < 0 || !movers[ahead].active) return false;
    float ax = movers[ahead].x - m->x, ay = movers[ahead].y - m->y;
    float d = sqrtf(ax * ax + ay * ay);
    const ServicePoint* lp = &servicePoints[sp];
    float fx = lp->x * CELL_SIZE + CELL_SIZE * 0.5f - m->x, fy = lp->y * CELL_SIZE + CELL_SIZE * 0.5f - m->y;
    if (d >= LADDER_FOLLOW_RANGE && fx * fx + fy * fy >= CELL_SIZE * CELL_SIZE) return false;   // walk on
    // Close to the one ahead, or it's still on its way and we're near the foot:
    // don't take the foot, line up
    if (d < LADDER_FOLLOW_RANGE && d > LADDER_LINE_SPACING * 1.1f) {
        float sp2 = effectiveSpeed * fminf(1.0f, (d - LADDER_LINE_SPACING) / (CELL_SIZE * 0.5f));
        *vx = ax / d * sp2;
        *vy = ay / d * sp2;
    }
    m->timeWithoutProgress = 0.0f;   // waiting in line is not being stuck
    m->timeNearWaypoint = 0.0f;
    m->lastX = m->x;
    m->lastY = m->y;
    return true;
}
