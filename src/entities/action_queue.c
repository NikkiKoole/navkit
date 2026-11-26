// action_queue.c - a mover's queue of small steps (see action_queue.h)
#include "action_queue.h"
#include "money.h"
#include "talk.h"
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
#include "../world/cell_defs.h"
#include "jobs.h"
#include "namegen.h"
#include "containers.h"
#include "places.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "../simulation/floordirt.h"

MoverQueue moverQueues[MAX_MOVERS];
bool sitToEat = true;
bool queueYieldsToNeeds = true;

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

bool QueueInsert(int moverIdx, int pos, const MoverAction* steps, int n) {
    MoverQueue* q = &moverQueues[moverIdx];
    if (n <= 0) return true;
    if (q->count + n > MAX_MOVER_ACTIONS) return false;
    if (pos < q->current) pos = q->current;
    if (pos > q->count) pos = q->count;
    memmove(&q->steps[pos + n], &q->steps[pos], sizeof(MoverAction) * (size_t)(q->count - pos));
    for (int i = 0; i < n; i++) {
        q->steps[pos + i] = steps[i];
        q->steps[pos + i].started = 0;
        q->steps[pos + i].progress = 0;
    }
    q->count = (uint8_t)(q->count + n);
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
    return MoverCanReach(m, items[i].x, items[i].y, (int)items[i].z);
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
    if (!QItemWithinReach(m, it)) return STEP_FAILED;
    if (ItemIsMoney(items[it].type)) {   // coins go in the purse, all of them, no hand needed (money.h)
        MoverPocketCoins(moverIdx, it);
        a->target = -1;
        return STEP_DONE;
    }
    if (MoverFreeHands(moverIdx) < 1) return STEP_FAILED;
    if (!MoverBuy(moverIdx, it)) return STEP_FAILED;   // a shop: pay first (money.h)
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
    // Eating ends with "get rid of the trash", which sets the freetime state (and may send the
    // mover to a bin): the queue keeps going; the trash waits until the queue is done
    m->freetimeState = FREETIME_QUEUE;
    m->needTarget = -1;
    QStopWalking(m);
    if (q->heldItem == it) q->heldItem = -1;
    // What it left (a wrapper, a can) goes away next, as steps of this queue (a follow-up, as a
    // sim puts its plate away): after lingering at the table, before what was queued after
    if (MoverHasTrash(moverIdx)) {
        int pos = q->current + 1;
        while (pos < q->count && q->steps[pos].type == ACT_LINGER) pos++;
        int bin = FindTrashBinFor(moverIdx, MoverTrashType(moverIdx));
        MoverAction follow[2];
        int n = 0;
        if (bin >= 0) { follow[n++] = ActWalkToItem(bin, true); follow[n++] = ActPutTrashIn(bin); }
        else follow[n++] = ActDropTrash();
        if (!QueueInsert(moverIdx, pos, follow, n)) MoverThrowTrash(moverIdx, -1);   // queue full: drop it
    }
    return STEP_DONE;
}

static StepResult QStepPutTrashIn(int moverIdx, MoverAction* a) {
    Mover* m = &movers[moverIdx];
    if (!MoverHasTrash(moverIdx)) return STEP_DONE;
    bool reach = QItemValid(a->target) && QItemWithinReach(m, a->target);
    MoverThrowTrash(moverIdx, reach ? a->target : -1);   // out of reach (bin gone, no way): dropped here
    return STEP_DONE;
}

static StepResult QStepDropTrash(int moverIdx, MoverAction* a) {
    (void)a;
    if (MoverHasTrash(moverIdx)) MoverThrowTrash(moverIdx, -1);
    return STEP_DONE;
}

// What a search takes from a bin: empty cans (worth a deposit, City 1999). Food in bins is
// scrounging (needs.c), for the hungry.
static bool WorthCollecting(int it) { return items[it].type == ITEM_EMPTY_CAN; }

static StepResult QStepSearchBin(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    Mover* m = &movers[moverIdx];
    int bin = a->target;
    if (!QItemValid(bin) || !QItemWithinReach(m, bin)) return STEP_FAILED;
    if (!a->started) {
        if (items[bin].reservedBy != -1 && items[bin].reservedBy != moverIdx) {   // someone rummages there
            if (q->source == QUEUE_FROM_PLAYER) AddMessage(TextFormat("%s: someone is searching that bin", MoverDisplayName(moverIdx)), GRAY);
            return STEP_FAILED;
        }
        a->started = 1;
        if (items[bin].reservedBy == -1) ReserveItem(bin, moverIdx);
        QStopWalking(m);
    }
    m->timeWithoutProgress = 0.0f;
    a->progress += gameDeltaTime;
    if (a->progress < GameHoursToGameSeconds(SEARCH_BIN_GH)) return STEP_RUNNING;
    // Take what's worth it until nothing more fits
    int want[64], n = 0;
    for (int c = FirstInContainer(bin); c >= 0 && n < 64; c = NextInContainer(c))
        if (items[c].active && items[c].reservedBy == -1 && WorthCollecting(c)) want[n++] = c;
    int took = 0;
    for (int i = 0; i < n; i++) {
        if (MoverStow(moverIdx, want[i]) == INV_NONE) break;
        took++;
    }
    if (items[bin].reservedBy == moverIdx) ReleaseItemReservation(bin);
    EventLog("Mover %d searched bin %d: took %d of %d cans", moverIdx, bin, took, n);
    if (q->source == QUEUE_FROM_PLAYER) {
        const char* who = MoverDisplayName(moverIdx);
        if (took > 0) AddMessage(TextFormat("%s found %d can%s%s", who, took, took == 1 ? "" : "s", took < n ? " (no room for more)" : ""), SKYBLUE);
        else AddMessage(TextFormat(n > 0 ? "%s: no room to take anything" : "%s: nothing worth taking in that bin", who), GRAY);
    }
    return STEP_DONE;
}

static StepResult QStepCashIn(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    Mover* m = &movers[moverIdx];
    int f = a->target;
    if (!QFurnitureValid(f) || !QAtCell(m, furniture[f].x, furniture[f].y, furniture[f].z)) return STEP_FAILED;
    if (!a->started) {
        a->started = 1;
        a->duration = 0.0f;   // coins earned so far (for the message)
        QStopWalking(m);
        if (BottleMachineShop(f) < 0) {
            if (q->source == QUEUE_FROM_PLAYER) AddMessage(TextFormat("%s: the machine is out of order (no shop nearby to pay)", MoverDisplayName(moverIdx)), GRAY);
            return STEP_FAILED;
        }
    }
    m->timeWithoutProgress = 0.0f;
    a->progress += gameDeltaTime;
    if (a->progress < GameHoursToGameSeconds(CASH_IN_PER_UNIT_GH)) return STEP_RUNNING;
    a->progress = 0.0f;
    int before = MoverCoins(moverIdx);
    if (MoverCashInOne(moverIdx, f)) { a->duration += (float)(MoverCoins(moverIdx) - before); return STEP_RUNNING; }
    // Nothing more to hand in, or the till is short
    int got = (int)a->duration, left = MoverDepositUnits(moverIdx);
    EventLog("Mover %d cashed in at machine %d: %d coins, %d cans left", moverIdx, f, got, left);
    if (q->source == QUEUE_FROM_PLAYER) {
        char mb[24];
        if (got > 0) AddMessage(TextFormat("%s got %s for the cans%s", MoverDisplayName(moverIdx), MoneyFormat(got, mb, sizeof mb),
                                           left > 0 ? " (the till ran out)" : ""), SKYBLUE);
        else AddMessage(TextFormat(left > 0 ? "%s: the till is empty" : "%s: no cans to hand in", MoverDisplayName(moverIdx)), GRAY);
    }
    return STEP_DONE;
}

static StepResult QStepStow(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    int it = a->target >= 0 ? a->target : q->heldItem;
    if (!QItemValid(it) || !ItemInMoverHands(moverIdx, it)) return STEP_DONE;
    // MoverStow leaves a held item where it is: out of the hand first, back in if nothing has room
    QLetGoOfHeld(moverIdx, it);
    InvPlace where = MoverStow(moverIdx, it);
    if (where == INV_NONE) { MoverHold(moverIdx, it); where = INV_HAND; }
    if (where != INV_HAND && q->heldItem == it) q->heldItem = -1;
    return STEP_DONE;
}

static StepResult QStepWait(int moverIdx, MoverAction* a) {
    movers[moverIdx].timeWithoutProgress = 0.0f;
    a->progress += gameDeltaTime;
    return a->progress >= a->duration ? STEP_DONE : STEP_RUNNING;
}

// Cleaning the floor where it stands (the walk before it brought it here)
static StepResult QStepTidy(int moverIdx, MoverAction* a) {
    Mover* m = &movers[moverIdx];
    if (!QAtCell(m, a->x, a->y, a->z)) return STEP_FAILED;
    m->timeWithoutProgress = 0.0f;
    a->progress += gameDeltaTime;
    if (a->progress < a->duration) return STEP_RUNNING;
    CleanFloorDirt(a->x, a->y, a->z, DIRT_CLEAN_AMOUNT);
    return STEP_DONE;
}

// The table (CAP_FLAT_SURFACE: table, desk) next to a seat, -1 = none
static int QTableOfSeat(int seat) {
    if (!QFurnitureValid(seat)) return -1;
    const Furniture* s = &furniture[seat];
    for (int f = 0; f < furnitureHighWater; f++) {
        if (!furniture[f].active || f == seat || furniture[f].z != s->z) continue;
        if (!(GetFurnitureDef(furniture[f].type)->capabilities & CAP_FLAT_SURFACE)) continue;
        if (abs(furniture[f].x - s->x) <= 1 && abs(furniture[f].y - s->y) <= 1) return f;
    }
    return -1;
}

// Someone else eating in a seat at this table right now
static bool QOtherDinerEating(int moverIdx, int table) {
    for (int j = 0; j < moverCount; j++) {
        if (j == moverIdx || !movers[j].active || !QueueBusy(j)) continue;
        const MoverQueue* q = &moverQueues[j];
        if (q->current >= q->count || q->steps[q->current].type != ACT_EAT || q->seat < 0) continue;
        if (QTableOfSeat(q->seat) == table) return true;
    }
    return false;
}

// Linger after the meal while a tablemate still eats (company: table talk, talk.c)
static StepResult QStepLinger(int moverIdx, MoverAction* a) {
    MoverQueue* q = &moverQueues[moverIdx];
    if (!a->started) { a->started = 1; a->target = QTableOfSeat(q->seat); }
    if (a->target < 0 || !QOtherDinerEating(moverIdx, a->target)) return STEP_DONE;
    movers[moverIdx].timeWithoutProgress = 0.0f;
    a->progress += gameDeltaTime;
    return a->progress >= a->duration ? STEP_DONE : STEP_RUNNING;
}

// ---------------------------------------------------------------------------
// Interrupt and resume
// ---------------------------------------------------------------------------

bool QueueSuspended(int moverIdx) { return moverQueues[moverIdx].suspended != 0; }

// A step still to come eats: hunger is what the queue is about, it doesn't interrupt for it
static bool QueueWillEat(int moverIdx) {
    const MoverQueue* q = &moverQueues[moverIdx];
    for (int s = q->current; s < q->count; s++) if (q->steps[s].type == ACT_EAT) return true;
    return false;
}

bool QueueWantsInterrupt(int moverIdx) {
    if (!queueYieldsToNeeds || !QueueBusy(moverIdx)) return false;
    if (movers[moverIdx].needSearchCooldown > 0.0f) return false;   // looked in vain just now: carry on
    return MoverHasCriticalNeed(moverIdx, QueueWillEat(moverIdx));
}

bool QueueReadyToResume(int moverIdx) {
    if (!moverQueues[moverIdx].suspended) return false;
    return movers[moverIdx].needSearchCooldown > 0.0f || !MoverHasCriticalNeed(moverIdx, QueueWillEat(moverIdx));
}

void QueueSuspend(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    Mover* m = &movers[moverIdx];
    q->suspended = 1;
    if (q->current < q->count) q->steps[q->current].started = 0;   // a walk plans again on resume
    QStopWalking(m);
    m->freetimeState = FREETIME_NONE;
    m->needTarget = -1;
    EventLog("Mover %d queue put aside at step %d for a critical need", moverIdx, q->current);
}

void QueueResume(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    if (!q->suspended) return;
    q->suspended = 0;
    if (q->current < q->count) q->steps[q->current].started = 0;
    // A meal put aside (toilet, ...): back to the seat first, then eat / linger on
    if (q->seat >= 0 && q->current < q->count && QFurnitureValid(q->seat) &&
        (q->steps[q->current].type == ACT_EAT || q->steps[q->current].type == ACT_LINGER) &&
        !QAtCell(&movers[moverIdx], furniture[q->seat].x, furniture[q->seat].y, furniture[q->seat].z)) {
        MoverAction back[2] = { ActWalkToFurniture(q->seat, true), ActSit(q->seat, true) };
        QueueInsert(moverIdx, q->current, back, 2);
    }
    movers[moverIdx].freetimeState = FREETIME_QUEUE;
    movers[moverIdx].needTarget = -1;
    EventLog("Mover %d queue resumed at step %d", moverIdx, q->current);
}

// The queue ran out or a required step failed: end it, then what eating left goes to a bin
static void QueueFinish(int moverIdx) {
    QueueEnd(moverIdx);
    MoverGetRidOfTrash(&movers[moverIdx], moverIdx);
}

void QueueTick(int moverIdx) {
    MoverQueue* q = &moverQueues[moverIdx];
    // Several instant steps (pick up, sit) can finish in one tick; a running one ends the tick
    for (int guard = 0; guard < MAX_MOVER_ACTIONS; guard++) {
        if (q->current >= q->count) { QueueFinish(moverIdx); return; }
        MoverAction* a = &q->steps[q->current];
        StepResult r = STEP_FAILED;
        switch (a->type) {
            case ACT_WALK_TO: r = QStepWalkTo(moverIdx, a); break;
            case ACT_PICK_UP: r = QStepPickUp(moverIdx, a); break;
            case ACT_SIT:     r = QStepSit(moverIdx, a); break;
            case ACT_EAT:     r = QStepEat(moverIdx, a); break;
            case ACT_WAIT:    r = QStepWait(moverIdx, a); break;
            case ACT_LINGER:  r = QStepLinger(moverIdx, a); break;
            case ACT_PUT_TRASH_IN: r = QStepPutTrashIn(moverIdx, a); break;
            case ACT_DROP_TRASH:   r = QStepDropTrash(moverIdx, a); break;
            case ACT_SEARCH_BIN:   r = QStepSearchBin(moverIdx, a); break;
            case ACT_STOW:         r = QStepStow(moverIdx, a); break;
            case ACT_CASH_IN:      r = QStepCashIn(moverIdx, a); break;
            case ACT_TIDY:         r = QStepTidy(moverIdx, a); break;
            default: break;
        }
        if (r == STEP_RUNNING) return;
        if (r == STEP_FAILED && !a->optional) {
            EventLog("Mover %d queue step %d (type %d) failed: queue ends", moverIdx, q->current, a->type);
            QueueFinish(moverIdx);
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
        case ACT_LINGER:  return "Lingering at the table";
        case ACT_PUT_TRASH_IN: return TextFormat("Throwing trash in the %s", what);
        case ACT_DROP_TRASH:   return "Dropping trash";
        case ACT_SEARCH_BIN:   return "Searching the bin";
        case ACT_STOW:         return "Putting it away";
        case ACT_CASH_IN:      return "Cashing in cans";
        case ACT_TIDY:         return "Tidying up";
        default:          return NULL;
    }
}

// ---------------------------------------------------------------------------
// Player commands
// ---------------------------------------------------------------------------

static const char* StepLabel(const MoverAction* a) {
    const char* what = "";
    if (a->targetKind == ATGT_ITEM && QItemValid(a->target)) what = ItemName(items[a->target].type);
    else if (a->targetKind == ATGT_FURNITURE && QFurnitureValid(a->target)) what = GetFurnitureDef(furniture[a->target].type)->name;
    switch (a->type) {
        case ACT_WALK_TO: return a->targetKind == ATGT_CELL ? TextFormat("Walk to %d,%d", a->x, a->y) : TextFormat("Walk to %s", what);
        case ACT_PICK_UP: return TextFormat("Pick up %s", what);
        case ACT_SIT:     return "Sit";
        case ACT_EAT:     return "Eat";
        case ACT_WAIT:    return "Wait";
        case ACT_LINGER:  return "Linger";
        case ACT_PUT_TRASH_IN: return "Throw away";
        case ACT_DROP_TRASH:   return "Drop trash";
        case ACT_SEARCH_BIN:   return "Search bin";
        case ACT_STOW:         return "Put away";
        case ACT_CASH_IN:      return "Cash in";
        case ACT_TIDY:         return "Tidy";
        default:          return "?";
    }
}

void QueueDescribe(int moverIdx, char* buf, int len) {
    buf[0] = '\0';
    if (!QueueBusy(moverIdx)) return;
    const MoverQueue* q = &moverQueues[moverIdx];
    int n = 0;
    for (int s = q->current; s < q->count && n < len - 1; s++) {
        const char* l = StepLabel(&q->steps[s]);
        n += snprintf(buf + n, (size_t)(len - n), s == q->current ? "%s[%s]" : "%s%s", s > q->current ? " > " : "", l);
    }
}

// Drop what the mover was busy with: a job, or a need's target (an item it reserved, the
// furniture it sat or lay on). Its own gear stays reserved.
static void DropCurrentBusiness(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (m->currentJobId >= 0) CancelJob(m, moverIdx);
    if (QueueBusy(moverIdx) || QueueSuspended(moverIdx)) QueueEnd(moverIdx);
    if (m->freetimeState != FREETIME_NONE) {
        ReleaseFurnitureForMover(moverIdx);
        for (int i = 0; i < itemHighWaterMark; i++)
            if (items[i].active && items[i].reservedBy == moverIdx && !ItemIsMoverGear(moverIdx, i))
                ReleaseItemReservation(i);
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
}

// The item on cell (x,y,z) a command means: food first, then anything not fixed in place
static int ItemForCommand(int moverIdx, int x, int y, int z) {
    int best = -1;
    for (int i = 0; i < itemHighWaterMark; i++) {
        const Item* it = &items[i];
        if (!it->active || (it->state != ITEM_ON_GROUND && it->state != ITEM_IN_STOCKPILE)) continue;
        if ((int)(it->x / CELL_SIZE) != x || (int)(it->y / CELL_SIZE) != y || (int)it->z != z) continue;
        if (it->reservedBy != -1 && it->reservedBy != moverIdx) continue;
        if (ItemIsEdible(it->type) && it->condition != CONDITION_ROTTEN) return i;
        if (best < 0 && !(itemDefs[it->type].flags & IF_FIXED)) best = i;
    }
    return best;
}

bool QueueCommandAt(int moverIdx, int x, int y, int z, bool append, char* said, int saidLen) {
    said[0] = '\0';
    if (moverIdx < 0 || moverIdx >= moverCount || !movers[moverIdx].active) return false;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    MoverQueue* q = &moverQueues[moverIdx];
    bool adding = append && QueueBusy(moverIdx) && q->source == QUEUE_FROM_PLAYER;
    const char* who = MoverDisplayName(moverIdx);

    // What is there: a bin to search, an item, a seat, ground
    int bin = -1;
    for (int i = 0; i < itemHighWaterMark && bin < 0; i++)
        if (items[i].active && items[i].type == ITEM_TRASH_BIN && items[i].state == ITEM_ON_GROUND &&
            (int)(items[i].x / CELL_SIZE) == x && (int)(items[i].y / CELL_SIZE) == y && (int)items[i].z == z) bin = i;
    int item = ItemForCommand(moverIdx, x, y, z);
    int f = GetFurnitureAt(x, y, z);
    bool machine = f >= 0 && furniture[f].type == FURNITURE_BOTTLE_MACHINE;
    bool seat = f >= 0 && (GetFurnitureDef(furniture[f].type)->capabilities & CAP_SEATING) &&
                (FurnitureHasRoom(f) || q->seat == f);
    bool ground = IsCellWalkableAt(z, y, x);
    if (bin < 0 && item < 0 && !seat && !machine && !ground) return false;
    if (seat && adding && q->seat >= 0 && q->seat != f) { snprintf(said, (size_t)saidLen, "%s: one seat per queue", who); return false; }

    MoverAction steps[4];
    int n = 0;
    const char* doing;
    if (bin >= 0) {
        steps[n++] = ActWalkToItem(bin, false);
        steps[n++] = ActSearchBin(bin);
        doing = "search the bin";
        item = -1;   // nothing to reserve: the search takes what it finds
    } else if (item >= 0 && ItemIsEdible(items[item].type)) {
        steps[n++] = ActWalkToItem(item, false);
        steps[n++] = ActPickUp(item, false);
        steps[n++] = ActEatHeld();
        doing = TextFormat("eat %s", ItemName(items[item].type));
    } else if (machine) {
        int cans = MoverDepositUnits(moverIdx);
        if (cans == 0 && !adding) { snprintf(said, (size_t)saidLen, "%s: no cans to hand in", who); return false; }
        steps[n++] = ActWalkToFurniture(f, false);
        steps[n++] = ActCashIn(f);
        doing = cans > 0 ? TextFormat("cash in %d can%s", cans, cans == 1 ? "" : "s") : "cash in the cans";
        item = -1;
    } else if (item >= 0 && ItemIsMoney(items[item].type)) {
        char mb[24];
        steps[n++] = ActWalkToItem(item, false);
        steps[n++] = ActPickUp(item, false);   // straight into the purse
        doing = TextFormat("pick up %s", MoneyFormat(items[item].stackCount, mb, sizeof mb));
    } else if (item >= 0) {
        steps[n++] = ActWalkToItem(item, false);
        steps[n++] = ActPickUp(item, false);
        steps[n++] = ActStowHeld();   // into the bag if there's room, else it stays in the hand
        doing = TextFormat("pick up %s", ItemName(items[item].type));
    } else if (seat) {
        steps[n++] = ActWalkToFurniture(f, false);
        steps[n++] = ActSit(f, false);
        steps[n++] = ActWait(GameHoursToGameSeconds(QUEUE_SIT_WHILE_GH));
        doing = TextFormat("sit on the %s", GetFurnitureDef(furniture[f].type)->name);
    } else {
        steps[n++] = ActWalkToCell(x, y, z, false);
        doing = TextFormat("go to %d,%d", x, y);
    }
    if (item >= 0 && !MoverCanAfford(moverIdx, item)) {
        char mb[24];
        snprintf(said, (size_t)saidLen, "%s: can't pay for the %s (%s)", who, ItemName(items[item].type),
                 MoneyFormat(ItemShopPrice(item), mb, sizeof mb));
        return false;
    }
    if (adding && q->count + n > MAX_MOVER_ACTIONS) { snprintf(said, (size_t)saidLen, "%s: queue is full", who); return false; }

    if (!adding) {
        DropCurrentBusiness(moverIdx);
        QueueReset(moverIdx);
    }
    for (int i = 0; i < n; i++) QueuePush(moverIdx, steps[i]);
    if (item >= 0 && items[item].reservedBy == -1) ReserveItem(item, moverIdx);
    if (seat && q->seat != f) { ReserveFurniture(f, moverIdx); q->seat = f; }
    if (!adding) QueueStart(moverIdx, QUEUE_FROM_PLAYER);
    snprintf(said, (size_t)saidLen, "%s: %s%s", who, adding ? "then " : "", doing);
    EventLog("Mover %d player command: %s%s", moverIdx, adding ? "then " : "", doing);
    return true;
}

// ---------------------------------------------------------------------------
// Sit-then-eat
// ---------------------------------------------------------------------------

// The nearest free seat (chair, sofa, desk: CAP_SEATING) on the mover's level, -1 = none
static int QFindSeatNear(const Mover* m) {
    int best = -1;
    float bestScore = 1e9f;   // distance less how nice the neighbours are (talk.c SeatPreference)
    int mx = QMoverCellX(m), my = QMoverCellY(m), mz = (int)m->z;
    for (int f = 0; f < furnitureHighWater; f++) {
        if (!furniture[f].active || furniture[f].z != mz) continue;
        if (!(GetFurnitureDef(furniture[f].type)->capabilities & CAP_SEATING)) continue;
        if (!FurnitureHasRoom(f)) continue;
        if (!MayUse((int)(m - movers), furniture[f].x, furniture[f].y, furniture[f].z)) continue;   // someone else's (places.h)
        float dx = (float)(furniture[f].x - mx), dy = (float)(furniture[f].y - my);
        float d = dx * dx + dy * dy;
        if (d > (float)(SIT_TO_EAT_RADIUS * SIT_TO_EAT_RADIUS) + 0.5f) continue;
        float score = sqrtf(d) - SeatPreference((int)(m - movers), f);
        if (score < bestScore) { bestScore = score; best = f; }
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
    QueuePush(moverIdx, ActLinger(GameHoursToGameSeconds(LINGER_MAX_GH)));   // company at the table
    ReserveFurniture(seat, moverIdx);
    moverQueues[moverIdx].seat = seat;
    QueueStart(moverIdx, QUEUE_FROM_FREETIME);
    EventLog("Mover %d sit-then-eat: item %d at seat %d (%s)", moverIdx, foodItem, seat, GetFurnitureDef(furniture[seat].type)->name);
    return true;
}
