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
    uint8_t pad;
    int heldItem;         // what ACT_PICK_UP took (ACT_EAT eats it), -1 = none
    int seat;             // furniture this queue reserved (released at the end), -1 = none
} MoverQueue;

extern MoverQueue moverQueues[MAX_MOVERS];

void QueueResetAll(void);                         // new world / old save: every queue empty
void QueueReset(int moverIdx);                    // empty, nothing held or reserved (no releases)
bool QueuePush(int moverIdx, MoverAction a);      // false: full
bool QueueBusy(int moverIdx);                     // a queue is running (freetimeState == FREETIME_QUEUE)
void QueueStart(int moverIdx, QueueSource src);   // run it: the mover's freetime state becomes FREETIME_QUEUE
void QueueEnd(int moverIdx);                      // stop: release the seat and reserved items, freetime NONE
void QueueTick(int moverIdx);                     // do the current step (needs.c, FREETIME_QUEUE)
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
static inline MoverAction ActWait(float gameSeconds) {
    return (MoverAction){ .type = ACT_WAIT, .duration = gameSeconds, .target = -1 };
}

// Sit-then-eat (needs.c): at its food, a mover with a free hand and a free seat nearby takes
// one unit to the seat and eats there. True = the queue was started.
bool QueueSitThenEat(int moverIdx, int foodItem);
extern bool sitToEat;   // setting (default on)

#endif
