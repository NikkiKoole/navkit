// action_queue.h - a mover's queue of small steps (docs/todo/architecture/action-queue-and-need-index.md)
//
// A queue is a short list of steps a mover works through in order: walk to a thing, pick it up,
// sit, eat, wait. Optional steps are skipped when they can't happen (no chair: eat standing),
// a required step that fails ends the queue. While a queue runs the mover's freetime state is
// FREETIME_QUEUE (not idle: no jobs, no talk). Freetime runs it (needs.c); the first user is
// sit-then-eat, later the player's commands go into the same queue.
//
// Per mover, kept next to the movers like the paths (moverQueues[i]) and saved (v112).
#ifndef ACTION_QUEUE_H
#define ACTION_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include "mover.h"

typedef enum {            // append only: saves store the number
    ACT_NONE,
    ACT_WALK_TO,          // to the target's cell (an item: within reach)
    ACT_PICK_UP,          // the target item (one unit of a stack) into a free hand
    ACT_SIT,              // on the target furniture (reserved when the queue was made; held to the end)
    ACT_EAT,              // the food picked up by this queue (or the target item), eatingDurationGH
    ACT_WAIT,             // duration game seconds
    ACT_LINGER,           // stay seated after a meal while someone at the same table still eats
                          // (at most duration game seconds; ends at once with nobody else eating)
    ACT_PUT_TRASH_IN,     // the wrapper / can / leftovers in hand into the target container (a bin);
                          // not within reach: dropped where it stands
    ACT_DROP_TRASH,       // the wrapper / can / leftovers in hand onto the ground here
    ACT_SEARCH_BIN,       // rummage in the target bin a while, then stow what's worth taking
                          // (empty cans: the deposit, City 1999) until nothing more fits
    ACT_STOW,             // the target item (or what this queue picked up) into pockets / backpack /
                          // a held bag (MoverStow); stays in the hand when nothing has room
    ACT_CASH_IN,          // at the target bottle machine: hand in the empty cans carried, one at a
                          // time, for their deposit (money.h); ends when none are left or the till is short
    ACT_TYPE_COUNT
} ActionType;

typedef enum {            // append only
    ATGT_NONE,
    ATGT_CELL,            // x, y, z
    ATGT_ITEM,            // target = item index
    ATGT_FURNITURE,       // target = furniture index
} ActionTargetKind;

typedef struct {
    uint8_t type;         // ActionType
    uint8_t targetKind;   // ActionTargetKind
    uint8_t optional;     // skip instead of ending the queue when it can't happen
    uint8_t started;      // set up (goal set, clock started)
    int target;           // item / furniture index
    int16_t x, y, z;      // ATGT_CELL
    int16_t pad;
    float progress;       // game seconds done (walks: the seek clock)
    float duration;       // ACT_WAIT
} MoverAction;

#define MAX_MOVER_ACTIONS 8

typedef enum { QUEUE_FROM_FREETIME, QUEUE_FROM_PLAYER } QueueSource;

typedef struct {
    MoverAction steps[MAX_MOVER_ACTIONS];
    uint8_t count;        // steps in the queue
    uint8_t current;      // the step being done
    uint8_t source;       // QueueSource
    uint8_t suspended;    // put aside for a critical need: freetime handles the need, then it resumes
    int heldItem;         // what ACT_PICK_UP took (ACT_EAT eats it), -1 = none
    int seat;             // furniture this queue reserved (released at the end), -1 = none
} MoverQueue;

extern MoverQueue moverQueues[MAX_MOVERS];

void QueueResetAll(void);                         // new world / old save: every queue empty
void QueueReset(int moverIdx);                    // empty, nothing held or reserved (no releases)
bool QueuePush(int moverIdx, MoverAction a);      // false: full
// Insert n steps at position pos (current + 1 = right after the step being done: a follow-up,
// like throwing the wrapper away after eating). False: they don't all fit (nothing inserted).
bool QueueInsert(int moverIdx, int pos, const MoverAction* steps, int n);
bool QueueBusy(int moverIdx);                     // a queue is running (freetimeState == FREETIME_QUEUE)
void QueueStart(int moverIdx, QueueSource src);   // run it: the mover's freetime state becomes FREETIME_QUEUE
void QueueEnd(int moverIdx);                      // stop: release the seat and reserved items, freetime NONE
void QueueTick(int moverIdx);                     // do the current step (needs.c, FREETIME_QUEUE)
// Interrupt and resume (needs.c): a critical need puts a running queue aside (QueueSuspend:
// freetime NONE, the need is handled as usual); once the mover is free again it carries on
// with the step it was on (QueueResume). setting queueYieldsToNeeds (default on).
bool QueueSuspended(int moverIdx);
void QueueSuspend(int moverIdx);
void QueueResume(int moverIdx);
bool QueueWantsInterrupt(int moverIdx);          // running, and a critical need may interrupt it
bool QueueReadyToResume(int moverIdx);           // put aside, and the need is handled (or can't be now)
extern bool queueYieldsToNeeds;
const char* QueueStepName(int moverIdx);          // "Walking to Chair", NULL when idle (describe)

// Step builders
static inline MoverAction ActWalkToItem(int item, bool optional) {
    return (MoverAction){ .type = ACT_WALK_TO, .targetKind = ATGT_ITEM, .target = item, .optional = optional };
}
static inline MoverAction ActWalkToFurniture(int f, bool optional) {
    return (MoverAction){ .type = ACT_WALK_TO, .targetKind = ATGT_FURNITURE, .target = f, .optional = optional };
}
static inline MoverAction ActWalkToCell(int x, int y, int z, bool optional) {
    return (MoverAction){ .type = ACT_WALK_TO, .targetKind = ATGT_CELL, .x = (int16_t)x, .y = (int16_t)y,
                          .z = (int16_t)z, .target = -1, .optional = optional };
}
static inline MoverAction ActPickUp(int item, bool optional) {
    return (MoverAction){ .type = ACT_PICK_UP, .targetKind = ATGT_ITEM, .target = item, .optional = optional };
}
static inline MoverAction ActSit(int f, bool optional) {
    return (MoverAction){ .type = ACT_SIT, .targetKind = ATGT_FURNITURE, .target = f, .optional = optional };
}
static inline MoverAction ActEatHeld(void) {
    return (MoverAction){ .type = ACT_EAT, .targetKind = ATGT_NONE, .target = -1 };
}
static inline MoverAction ActLinger(float maxGameSeconds) {
    return (MoverAction){ .type = ACT_LINGER, .duration = maxGameSeconds, .target = -1, .optional = 1 };
}
#define LINGER_MAX_GH 0.5f   // longest a diner lingers for a slow eater (game hours)
static inline MoverAction ActPutTrashIn(int container) {
    return (MoverAction){ .type = ACT_PUT_TRASH_IN, .targetKind = ATGT_ITEM, .target = container };
}
static inline MoverAction ActDropTrash(void) {
    return (MoverAction){ .type = ACT_DROP_TRASH, .target = -1 };
}
static inline MoverAction ActSearchBin(int bin) {
    return (MoverAction){ .type = ACT_SEARCH_BIN, .targetKind = ATGT_ITEM, .target = bin };
}
static inline MoverAction ActStowHeld(void) {
    return (MoverAction){ .type = ACT_STOW, .target = -1, .optional = 1 };
}
static inline MoverAction ActCashIn(int machine) {
    return (MoverAction){ .type = ACT_CASH_IN, .targetKind = ATGT_FURNITURE, .target = machine };
}
#define CASH_IN_PER_UNIT_GH 0.01f   // feeding one can into the machine (game hours)
#define SEARCH_BIN_GH 0.2f   // rummaging in a bin (game hours), as scrounging
static inline MoverAction ActWait(float gameSeconds) {
    return (MoverAction){ .type = ACT_WAIT, .duration = gameSeconds, .target = -1 };
}

// Player commands (step 3): the selected mover does what fits the cell the player points at:
// food -> walk there, pick it up, eat; another item -> walk there, pick it up; a seat (chair,
// sofa, desk) -> walk there, sit a while; a bottle machine -> walk there, cash in the cans;
// walkable ground -> walk there. append (Shift) adds it
// to a running player queue instead of replacing it. A job or a need it was busy with is
// dropped. Fills *said with what it will do ("Kira: eat Berries"). False: nothing to do there.
bool QueueCommandAt(int moverIdx, int x, int y, int z, bool append, char* said, int saidLen);
// "Walk to Chair > [Sit] > Wait" (the current step in brackets), "" when no queue runs
void QueueDescribe(int moverIdx, char* buf, int len);
#define QUEUE_SIT_WHILE_GH 0.5f   // a commanded sit lasts this long (game hours)

// Sit-then-eat (needs.c): at its food, a mover with a free hand and a free seat nearby takes
// one unit to the seat and eats there. True = the queue was started.
bool QueueSitThenEat(int moverIdx, int foodItem);
extern bool sitToEat;   // setting (default on)

#endif
