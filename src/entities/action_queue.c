// action_queue.c - a mover's queue of small steps (see action_queue.h)
#include "action_queue.h"
#include "items.h"
#include "item_defs.h"
#include "inventory.h"
#include "furniture.h"
#include "stockpiles.h"
#include "stacking.h"
#include "../simulation/needs.h"
#include "../simulation/balance.h"
#include "../core/event_log.h"
#include "../core/time.h"
#include "../world/pathfinding.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

MoverQueue moverQueues[MAX_MOVERS];
bool sitToEat = true;

#define QUEUE_WALK_SLACK      3.0f   // a walk may take 3x the straight line (detours, stairs)
#define QUEUE_WALK_MIN_GH     0.5f   // ... and at least this long before it gives up
#define QUEUE_NO_WAY_REPATHS  3      // repaths that found no path: there is no way there
#define SIT_TO_EAT_RADIUS     10     // cells: a seat further away isn't worth carrying food to

void QueueReset(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    memset(q, 0, sizeof(*q));
    q->heldItem = -1;
    q->seat = -1;
}

void QueueResetAll(void) {
    for (int i = 0; i < MAX_MOVERS; i++) QueueReset(i);
}

bool QueuePush(int moverIdx, MoverAction a) {
    MoverQueue* q = &moverQueues[moverIdx];
    if (q->count >= MAX_MOVER_ACTIONS) return false;
    a.started = 0;
    a.progress = 0;
    q->steps[q->count++] = a;
    return true;
}

bool QueueBusy(int moverIdx) { return movers[moverIdx].freetimeState == FREETIME_QUEUE; }

void QueueStart(int moverIdx, QueueSource src) {
    MoverQueue* q = &moverQueues[moverIdx];
    q->current = 0;
    q->source = (uint8_t)src;
    movers[moverIdx].freetimeState = FREETIME_QUEUE;
    movers[moverIdx].needTarget = -1;
}

// Items this queue reserved but never got into a hand go back (a held one stays held: the
// mover eats it from its hands later, EatFromInventory)
static void QReleaseQueueItems(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    for (int s = 0; s < q->count; s++) {
        const MoverAction* a = &q->steps[s];
        if (a->targetKind != ATGT_ITEM || a->target < 0 || a->target >= MAX_ITEMS) continue;
        Item* it = &items[a->target];
        if (it->active && it->reservedBy == moverIdx && !ItemInMoverHands(moverIdx, a->target) &&
            !ItemIsMoverGear(moverIdx, a->target))
            ReleaseItemReservation(a->target);
    }
}

void QueueEnd(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    if (q->seat >= 0) ReleaseFurniture(q->seat, moverIdx);
    QReleaseQueueItems(moverIdx);
    Mover* m = &movers[moverIdx];
    if (m->freetimeState == FREETIME_QUEUE) m->freetimeState = FREETIME_NONE;
    m->needTarget = -1;
    QueueReset(moverIdx);
}

// ---------------------------------------------------------------------------
// Steps
// ---------------------------------------------------------------------------

typedef enum { STEP_RUNNING, STEP_DONE, STEP_FAILED } StepResult;

static int QMoverCellX(const Mover* m) { return (int)(m->x / CELL_SIZE); }
static int QMoverCellY(const Mover* m) { return (int)(m->y / CELL_SIZE); }

static bool QItemValid(int i) { return i >= 0 && i < MAX_ITEMS && items[i].active; }
static bool QFurnitureValid(int f) { return f >= 0 && f < MAX_FURNITURE && furniture[f].active; }

// Where a step's target is (false: gone)
static bool QTargetCell(const MoverAction* a, int* x, int* y, int* z) {
    switch (a->targetKind) {
        case ATGT_CELL: *x = a->x; *y = a->y; *z = a->z; return true;
        case ATGT_ITEM:
            if (!QItemValid(a->target)) return false;
            *x = (int)(items[a->target].x / CELL_SIZE); *y = (int)(items[a->target].y / CELL_SIZE);
            *z = (int)items[a->target].z;
            return true;
        case ATGT_FURNITURE:
            if (!QFurnitureValid(a->target)) return false;
            *x = furniture[a->target].x; *y = furniture[a->target].y; *z = furniture[a->target].z;
            return true;
        default: return false;
    }
}

static bool QAtCell(const Mover* m, int x, int y, int z) {
    return QMoverCellX(m) == x && QMoverCellY(m) == y && (int)m->z == z;
}

static bool QItemWithinReach(const Mover* m, int i) {
    float dx = m->x - items[i].x, dy = m->y - items[i].y, r = CELL_SIZE * 0.75f;
    return (int)m->z == (int)items[i].z && dx * dx + dy * dy < r * r;
}

static void QStopWalking(Mover* m) {
    m->pathLength = 0;
    m->pathIndex = -1;
    m->needsRepath = false;
}

static StepResult QStepWalkTo(int moverIdx, MoverAction* a) {
    Mover* m = &movers[moverIdx];
    int x, y, z;
    if (!QTargetCell(a, &x, &y, &z)) return STEP_FAILED;
    if (a->targetKind == ATGT_ITEM && ItemInMoverHands(moverIdx, a->target)) return STEP_DONE;
    bool arrived = QAtCell(m, x, y, z) || (a->targetKind == ATGT_ITEM && QItemWithinReach(m, a->target));
    if (arrived) { if (a->started) QStopWalking(m); return STEP_DONE; }
    if (!a->started) {
        a->started = 1;
        m->goal = (Point){ x, y, z };
        m->needsRepath = true;
        moverRepathFails[moverIdx] = 0;
        // The seek clock (as needs.c): slack for the walk itself, so a far target isn't a timeout
        float dx = (x + 0.5f) * CELL_SIZE - m->x, dy = (y + 0.5f) * CELL_SIZE - m->y, dz = (z - m->z) * CELL_SIZE;
        float pxPerGameSecond = m->speed * (60.0f / dayLength);
        a->progress = pxPerGameSecond > 0 ? -QUEUE_WALK_SLACK * sqrtf(dx * dx + dy * dy + dz * dz) / pxPerGameSecond : 0;
        return STEP_RUNNING;
    }
    a->progress += gameDeltaTime;
    if (a->progress > GameHoursToGameSeconds(QUEUE_WALK_MIN_GH) || moverRepathFails[moverIdx] >= QUEUE_NO_WAY_REPATHS) {
        QStopWalking(m);
        return STEP_FAILED;
    }
    return STEP_RUNNING;
}

static StepResult QStepPickUp(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    Mover* m = &movers[moverIdx];
    int it = a->target;
    if (!QItemValid(it)) return STEP_FAILED;
    if (ItemInMoverHands(moverIdx, it)) { q->heldItem = it; return STEP_DONE; }
    if (items[it].reservedBy != -1 && items[it].reservedBy != moverIdx) return STEP_FAILED;
    if (!QItemWithinReach(m, it) || MoverFreeHands(moverIdx) < 1) return STEP_FAILED;
    // One unit: out of a stockpile slot (the rest stays), or off a stack on the ground
    int one = it;
    if (items[it].state == ITEM_IN_STOCKPILE) one = TakeFromStockpileSlot(it, 1);
    else if (items[it].stackCount > 1) {
        one = SplitStack(it, 1);
        if (one >= 0) items[one].state = ITEM_ON_GROUND;
    }
    if (one < 0) return STEP_FAILED;
    if (one != it && items[it].reservedBy == moverIdx) ReleaseItemReservation(it);   // the rest is free again
    if (!MoverHold(moverIdx, one)) return STEP_FAILED;
    a->target = one;   // the unit in hand (QReleaseQueueItems leaves held items alone)
    q->heldItem = one;
    return STEP_DONE;
}

static StepResult QStepSit(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    Mover* m = &movers[moverIdx];
    int f = a->target;
    if (!QFurnitureValid(f) || !QAtCell(m, furniture[f].x, furniture[f].y, furniture[f].z)) return STEP_FAILED;
    q->seat = f;   // held until the queue ends
    QStopWalking(m);
    return STEP_DONE;
}

// Out of the mover's hands (it is about to be eaten)
static void QLetGoOfHeld(int moverIdx, int it) {
    Mover* m = &movers[moverIdx];
    if (m->hands[0] == it) m->hands[0] = -1;
    if (m->hands[1] == it) m->hands[1] = -1;
    MoverRecalcCarried(moverIdx);
}

static StepResult QStepEat(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    Mover* m = &movers[moverIdx];
    int it = a->target >= 0 ? a->target : q->heldItem;
    if (!QItemValid(it)) return STEP_FAILED;
    m->timeWithoutProgress = 0.0f;   // still on purpose
    a->progress += gameDeltaTime;
    if (a->progress < GameHoursToGameSeconds(balance.eatingDurationGH)) return STEP_RUNNING;
    bool held = ItemInMoverHands(moverIdx, it);
    if (held) QLetGoOfHeld(moverIdx, it);
    MoverEatItem(m, moverIdx, it);
    if (q->heldItem == it) q->heldItem = -1;
    return STEP_DONE;
}

static StepResult QStepWait(int moverIdx, MoverAction* a) {
    movers[moverIdx].timeWithoutProgress = 0.0f;
    a->progress += gameDeltaTime;
    return a->progress >= a->duration ? STEP_DONE : STEP_RUNNING;
}

void QueueTick(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    // Several instant steps (pick up, sit) can finish in one tick; a running one ends the tick
    for (int guard = 0; guard < MAX_MOVER_ACTIONS; guard++) {
        if (q->current >= q->count) { QueueEnd(moverIdx); return; }
        MoverAction* a = &q->steps[q->current];
        StepResult r = STEP_FAILED;
        switch (a->type) {
            case ACT_WALK_TO: r = QStepWalkTo(moverIdx, a); break;
            case ACT_PICK_UP: r = QStepPickUp(moverIdx, a); break;
            case ACT_SIT:     r = QStepSit(moverIdx, a); break;
            case ACT_EAT:     r = QStepEat(moverIdx, a); break;
            case ACT_WAIT:    r = QStepWait(moverIdx, a); break;
            default: break;
        }
        if (r == STEP_RUNNING) return;
        if (r == STEP_FAILED && !a->optional) {
            EventLog("Mover %d queue step %d (type %d) failed: queue ends", moverIdx, q->current, a->type);
            QueueEnd(moverIdx);
            return;
        }
        q->current++;
    }
}

const char* QueueStepName(int moverIdx) {
    if (!QueueBusy(moverIdx)) return NULL;
    const MoverQueue* q = &moverQueues[moverIdx];
    if (q->current >= q->count) return NULL;
    const MoverAction* a = &q->steps[q->current];
    const char* what = "";
    if (a->targetKind == ATGT_ITEM && QItemValid(a->target)) what = ItemName(items[a->target].type);
    else if (a->targetKind == ATGT_FURNITURE && QFurnitureValid(a->target)) what = GetFurnitureDef(furniture[a->target].type)->name;
    switch (a->type) {
        case ACT_WALK_TO: return TextFormat("Walking to %s", a->targetKind == ATGT_CELL ? "a spot" : what);
        case ACT_PICK_UP: return TextFormat("Picking up %s", what);
        case ACT_SIT:     return TextFormat("Sitting down on %s", what);
        case ACT_EAT:     return "Eating";
        case ACT_WAIT:    return "Waiting";
        default:          return NULL;
    }
}

// ---------------------------------------------------------------------------
// Sit-then-eat
// ---------------------------------------------------------------------------

// The nearest free seat (chair, sofa, desk: CAP_SEATING) on the mover's level, -1 = none
static int QFindSeatNear(const Mover* m) {
    int best = -1;
    float bestD = (float)(SIT_TO_EAT_RADIUS * SIT_TO_EAT_RADIUS) + 0.5f;
    int mx = QMoverCellX(m), my = QMoverCellY(m), mz = (int)m->z;
    for (int f = 0; f < MAX_FURNITURE; f++) {
        if (!furniture[f].active || furniture[f].z != mz) continue;
        if (!(GetFurnitureDef(furniture[f].type)->capabilities & CAP_SEATING)) continue;
        if (!FurnitureHasRoom(f)) continue;
        float dx = (float)(furniture[f].x - mx), dy = (float)(furniture[f].y - my);
        float d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = f; }
    }
    return best;
}

bool QueueSitThenEat(int moverIdx, int foodItem) {
    if (!sitToEat || !QItemValid(foodItem)) return false;
    Mover* m = &movers[moverIdx];
    if (MoverFreeHands(moverIdx) < 1 || ItemIsTwoHanded(items[foodItem].type)) return false;
    int seat = QFindSeatNear(m);
    if (seat < 0) return false;
    QueueReset(moverIdx);
    QueuePush(moverIdx, ActPickUp(foodItem, false));
    QueuePush(moverIdx, ActWalkToFurniture(seat, true));   // can't get there: eat where it stands
    QueuePush(moverIdx, ActSit(seat, true));
    QueuePush(moverIdx, ActEatHeld());
    ReserveFurniture(seat, moverIdx);
    moverQueues[moverIdx].seat = seat;
    QueueStart(moverIdx, QUEUE_FROM_FREETIME);
    EventLog("Mover %d sit-then-eat: item %d at seat %d (%s)", moverIdx, foodItem, seat, GetFurnitureDef(furniture[seat].type)->name);
    return true;
}
