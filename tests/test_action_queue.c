// The action queue (src/entities/action_queue.c): a mover's list of small steps, run by freetime.
// First user: sit-then-eat (at its food, a hungry mover with a free hand and a seat nearby takes
// one unit to the seat and eats there). docs/todo/architecture/action-queue-and-need-index.md
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/furniture.h"
#include "../src/entities/inventory.h"
#include "../src/entities/containers.h"
#include "../src/entities/action_queue.h"
#include "../src/simulation/plants.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/core/state_audit.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// A 16x5 floor at z=0, one hungry mover at (1,2), a stack of 3 berries at (3,2)
static int food;
static int Setup(const char* ascii) {
    InitTestGridFromAscii(ascii);
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers(); ClearItems(); ClearStockpiles(); ClearJobs(); ClearPlants(); ClearFurniture();
    InitBalance();
    hungerEnabled = true; energyEnabled = false; bodyTempEnabled = false;
    thirstEnabled = false; bladderEnabled = false;
    dayLength = 60.0f;
    gameDeltaTime = TICK_DT;
    currentTick = 1;
    sitToEat = true;
    food = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BERRIES);
    items[food].stackCount = 3;
    Point goal = {1, 2, 0};
    InitMover(&movers[0], 1.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
    moverCount = 1;
    movers[0].hunger = 0.25f;
    return 0;
}

static const char* openFloor =
    "................\n"
    "................\n"
    "................\n"
    "................\n"
    "................\n";

typedef struct { bool queued, ateStanding, ate; int ateX, ateY; } Run;

// Run until the mover ate (hunger went up) or the time is up
static Run RunUntilFed(int maxTicks) {
    Run r = {0};
    float before = movers[0].hunger;
    for (int t = 0; t < maxTicks; t++) {
        Tick();
        NeedsTick();
        ProcessFreetimeNeeds();
        if (movers[0].freetimeState == FREETIME_QUEUE) r.queued = true;
        if (movers[0].freetimeState == FREETIME_EATING) r.ateStanding = true;
        if (movers[0].hunger > before + 0.05f) {
            r.ate = true;
            r.ateX = (int)(movers[0].x / CELL_SIZE);
            r.ateY = (int)(movers[0].y / CELL_SIZE);
            break;
        }
        before = movers[0].hunger;
    }
    return r;
}

describe(sit_then_eat) {
    it("takes one berry to the chair, sits and eats there; the seat and the rest are free after") {
        Setup(openFloor);
        int chair = SpawnFurniture(9, 2, 0, FURNITURE_CHAIR, MAT_OAK);
        Run r = RunUntilFed(20000);
        expect(r.ate && r.queued && !r.ateStanding);
        expect(r.ateX == 9 && r.ateY == 2);   // at the chair
        // A tick later the queue is over: idle again, seat free, hands empty, two berries left
        Tick(); NeedsTick(); ProcessFreetimeNeeds();
        expect(movers[0].freetimeState != FREETIME_QUEUE);
        expect(FurnitureHasRoom(chair));
        expect(movers[0].hands[0] == -1 && movers[0].hands[1] == -1);
        expect(items[food].active && items[food].stackCount == 2 && items[food].reservedBy == -1);
        expect(RunStateAudit(test_verbose) == 0);
    }

    it("no seat nearby: eats standing where the food lies, as before") {
        Setup(openFloor);
        Run r = RunUntilFed(20000);
        expect(r.ate && !r.queued && r.ateStanding);
        expect(r.ateX >= 2 && r.ateX <= 3 && r.ateY == 2);   // within reach of the berries
    }

    it("a seat it can't reach: the walk and the sitting are skipped, it still eats") {
        Setup("................\n"
              "........###.....\n"
              "........#.#.....\n"
              "........###.....\n"
              "................\n");
        SpawnFurniture(9, 2, 0, FURNITURE_CHAIR, MAT_OAK);   // walled in
        Run r = RunUntilFed(60000);
        expect(r.ate && r.queued);
        expect(!(r.ateX == 9 && r.ateY == 2));
        expect(movers[0].hands[0] == -1 && movers[0].hands[1] == -1);
    }

    it("setting off (sitToEat 0): eats standing") {
        Setup(openFloor);
        SpawnFurniture(9, 2, 0, FURNITURE_CHAIR, MAT_OAK);
        sitToEat = false;
        Run r = RunUntilFed(20000);
        expect(r.ate && !r.queued && r.ateStanding);
        sitToEat = true;
    }
}

describe(action_queue_basics) {
    it("optional steps that can't happen are skipped, a required one ends the queue") {
        Setup(openFloor);
        QueueReset(0);
        QueuePush(0, ActSit(5, true));          // no such furniture: skipped
        QueuePush(0, ActWait(0.5f));
        QueueStart(0, QUEUE_FROM_PLAYER);
        for (int t = 0; t < 120 && QueueBusy(0); t++) { Tick(); ProcessFreetimeNeeds(); }
        expect(!QueueBusy(0));                  // waited and finished
        QueueReset(0);
        QueuePush(0, ActPickUp(12345, false));  // required, impossible
        QueuePush(0, ActWait(100.0f));
        QueueStart(0, QUEUE_FROM_PLAYER);
        ProcessFreetimeNeeds();
        expect(!QueueBusy(0) && movers[0].freetimeState == FREETIME_NONE);
    }

    it("a queue is saved and loaded (v112)") {
        Setup(openFloor);
        int chair = SpawnFurniture(9, 2, 0, FURNITURE_CHAIR, MAT_OAK);
        for (int t = 0; t < 20000 && movers[0].freetimeState != FREETIME_QUEUE; t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
        }
        expect(movers[0].freetimeState == FREETIME_QUEUE);
        int count = moverQueues[0].count, current = moverQueues[0].current;
        expect(SaveWorld("/tmp/test_action_queue_save.bin"));
        ClearItems(); ClearMovers();
        expect(LoadWorld("/tmp/test_action_queue_save.bin"));
        expect(movers[0].freetimeState == FREETIME_QUEUE);
        expect(moverQueues[0].count == count && moverQueues[0].current == current);
        expect(moverQueues[0].seat == chair);
        // and it carries on: eats at the chair
        Run r = RunUntilFed(20000);
        expect(r.ate && r.ateX == 9 && r.ateY == 2);
    }
}

// Run until the queue is done (or the time is up); true = it finished
static bool RunQueue(int maxTicks) {
    for (int t = 0; t < maxTicks; t++) {
        Tick(); NeedsTick(); ProcessFreetimeNeeds();
        if (!QueueBusy(0)) return true;
    }
    return false;
}
static int CellX(void) { return (int)(movers[0].x / CELL_SIZE); }
static int CellY(void) { return (int)(movers[0].y / CELL_SIZE); }

describe(player_commands) {
    it("ground: walks there") {
        Setup(openFloor);
        movers[0].hunger = 1.0f;
        char said[128];
        expect(QueueCommandAt(0, 12, 4, 0, false, said, sizeof(said)));
        expect(strstr(said, "go to 12,4") != NULL);
        expect(movers[0].freetimeState == FREETIME_QUEUE);
        expect(RunQueue(20000));
        expect(CellX() == 12 && CellY() == 4);
    }
    it("food: walks there, picks one up and eats it (even when not hungry)") {
        Setup(openFloor);
        movers[0].hunger = 0.6f;
        char said[128];
        expect(QueueCommandAt(0, 3, 2, 0, false, said, sizeof(said)));
        expect(strstr(said, "eat") != NULL);
        expect(RunQueue(20000));
        expect(movers[0].hunger > 0.6f);
        expect(items[food].active && items[food].stackCount == 2 && items[food].reservedBy == -1);
    }
    it("a seat: walks there, sits a while, then the seat is free again") {
        Setup(openFloor);
        movers[0].hunger = 1.0f;
        int chair = SpawnFurniture(9, 2, 0, FURNITURE_CHAIR, MAT_OAK);
        char said[128];
        expect(QueueCommandAt(0, 9, 2, 0, false, said, sizeof(said)));
        expect(!FurnitureHasRoom(chair));   // reserved for it
        bool satThere = false;
        for (int t = 0; t < 40000 && QueueBusy(0); t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            if (moverQueues[0].current == 2 && CellX() == 9 && CellY() == 2) satThere = true;
        }
        expect(satThere && !QueueBusy(0));
        expect(FurnitureHasRoom(chair));
    }
    it("Shift appends: go there, then eat") {
        Setup(openFloor);
        movers[0].hunger = 0.6f;
        char said[128];
        expect(QueueCommandAt(0, 12, 4, 0, false, said, sizeof(said)));
        expect(QueueCommandAt(0, 3, 2, 0, true, said, sizeof(said)));
        expect(strstr(said, "then eat") != NULL);
        expect(moverQueues[0].count == 4);
        bool wasThere = false;
        for (int t = 0; t < 40000 && QueueBusy(0); t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            if (CellX() == 12 && CellY() == 4) wasThere = true;
        }
        expect(wasThere && movers[0].hunger > 0.6f);
    }
    it("eat, then walk on: the queue carries on after the meal; the wrapper is dealt with after") {
        Setup(openFloor);
        movers[0].hunger = 0.6f;
        SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_SNACK);
        char said[128];
        expect(QueueCommandAt(0, 5, 2, 0, false, said, sizeof(said)));
        expect(strstr(said, "Snack") != NULL || strstr(said, "snack") != NULL);
        expect(QueueCommandAt(0, 12, 4, 0, true, said, sizeof(said)));
        expect(RunQueue(40000));
        expect(CellX() == 12 && CellY() == 4);   // walked on after eating
        expect(movers[0].hunger > 0.6f);
        // no bin here: the wrapper is dropped once the queue is done
        for (int t = 0; t < 600 && movers[0].heldTrash != ITEM_NONE; t++) { Tick(); NeedsTick(); ProcessFreetimeNeeds(); }
        expect(movers[0].heldTrash == ITEM_NONE);
    }
    it("a command replaces what it was doing (here: on its way to food)") {
        Setup(openFloor);
        for (int t = 0; t < 200 && movers[0].freetimeState != FREETIME_SEEKING_FOOD; t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
        }
        expect(movers[0].freetimeState == FREETIME_SEEKING_FOOD);
        expect(items[food].reservedBy == 0);
        char said[128];
        expect(QueueCommandAt(0, 12, 4, 0, false, said, sizeof(said)));
        expect(movers[0].freetimeState == FREETIME_QUEUE);
        expect(items[food].reservedBy == -1);   // the food it was going for is free again
        expect(RunQueue(20000));
        expect(CellX() == 12 && CellY() == 4);
    }
    it("a wall: nothing to do there") {
        Setup("................\n"
              "........#.......\n"
              "................\n");
        char said[128];
        expect(!QueueCommandAt(0, 8, 1, 0, false, said, sizeof(said)));
        expect(!QueueBusy(0));
    }
}

describe(linger) {
    it("lingers at the table while a tablemate still eats, then gets up") {
        Setup(openFloor);
        SpawnFurniture(9, 2, 0, FURNITURE_TABLE, MAT_OAK);
        SpawnFurniture(9, 1, 0, FURNITURE_CHAIR, MAT_OAK);
        int chair1 = SpawnFurniture(9, 3, 0, FURNITURE_CHAIR, MAT_OAK);
        // A second diner with food in hand sits at the other chair and waits (until the first one
        // is eating, then it starts eating too: it finishes a moment later)
        Point g = {9, 3, 0};
        InitMover(&movers[1], 9.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, g, MOVER_SPEED);
        moverCount = 2;
        movers[1].hunger = 1.0f;   // not hungry: it doesn't eat the snack on its own
        int snack = SpawnItem(9.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, ITEM_BERRIES);
        expect(MoverHold(1, snack));
        QueueReset(1);
        QueuePush(1, ActWalkToFurniture(chair1, false));
        QueuePush(1, ActSit(chair1, false));
        QueuePush(1, ActWait(1e9f));
        QueuePush(1, ActEatHeld());
        ReserveFurniture(chair1, 1);
        moverQueues[1].seat = chair1;
        moverQueues[1].heldItem = snack;
        QueueStart(1, QUEUE_FROM_FREETIME);
        bool secondStarted = false, lingered = false;
        int lingeredAlone = 0;   // ticks lingering with nobody else eating (1 allowed: the order within a tick)
        for (int t = 0; t < 40000; t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            const MoverQueue* q0 = &moverQueues[0];
            MoverQueue* q1 = &moverQueues[1];
            bool zeroEating = QueueBusy(0) && q0->current < q0->count && q0->steps[q0->current].type == ACT_EAT;
            if (zeroEating && !secondStarted && QueueBusy(1) && q1->steps[q1->current].type == ACT_WAIT) {
                q1->steps[q1->current].duration = 0;   // done waiting: eat
                secondStarted = true;
            }
            bool zeroLingering = QueueBusy(0) && q0->current < q0->count && q0->steps[q0->current].type == ACT_LINGER;
            bool oneEating = QueueBusy(1) && q1->steps[q1->current].type == ACT_EAT;
            if (zeroLingering) { lingered = true; if (!oneEating) lingeredAlone++; }
            if (secondStarted && !QueueBusy(0) && !QueueBusy(1)) break;
        }
        expect(secondStarted && lingered && lingeredAlone <= 1);
        expect(!QueueBusy(0) && !QueueBusy(1));
    }
    it("alone at the table: no lingering") {
        Setup(openFloor);
        SpawnFurniture(9, 2, 0, FURNITURE_TABLE, MAT_OAK);
        SpawnFurniture(9, 1, 0, FURNITURE_CHAIR, MAT_OAK);
        bool lingered = false;
        for (int t = 0; t < 40000; t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            const MoverQueue* q0 = &moverQueues[0];
            if (QueueBusy(0) && q0->current < q0->count && q0->steps[q0->current].type == ACT_LINGER) lingered = true;
            if (movers[0].hunger > 0.5f && !QueueBusy(0)) break;
        }
        expect(!lingered && movers[0].hunger > 0.5f);
    }
}

describe(follow_ups_and_interrupts) {
    it("after eating, the wrapper goes in the bin next (inserted), before walking on") {
        Setup(openFloor);
        movers[0].hunger = 0.6f;
        SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_SNACK);
        int bin = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_TRASH_BIN);
        char said[128];
        expect(QueueCommandAt(0, 5, 2, 0, false, said, sizeof(said)));
        expect(QueueCommandAt(0, 14, 4, 0, true, said, sizeof(said)));
        bool sawThrow = false, binBeforeArrival = false;
        for (int t = 0; t < 40000 && QueueBusy(0); t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            const MoverQueue* q = &moverQueues[0];
            // walking to the bin (the throw itself is instant on arrival)
            if (QueueBusy(0) && q->steps[q->current].type == ACT_WALK_TO && q->steps[q->current].target == bin) sawThrow = true;
            if (CellX() == 14 && CellY() == 4 && FirstInContainer(bin) >= 0) binBeforeArrival = true;
        }
        expect(sawThrow);
        expect(binBeforeArrival);
        expect(movers[0].heldTrash == ITEM_NONE);
        expect(CellX() == 14 && CellY() == 4);
    }

    it("a desperate bladder puts a long walk aside: toilet first, then it walks on") {
        Setup(openFloor);
        movers[0].hunger = 1.0f;
        bladderEnabled = true;
        SpawnFurniture(6, 4, 0, FURNITURE_TOILET, MAT_OAK);
        char said[128];
        expect(QueueCommandAt(0, 15, 0, 0, false, said, sizeof(said)));
        bool suspended = false, usedToilet = false;
        for (int t = 0; t < 60000; t++) {
            if (t == 30) movers[0].bladder = 0.01f;   // desperate, on the way
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            if (QueueSuspended(0)) suspended = true;
            if (movers[0].freetimeState == FREETIME_USING_TOILET) usedToilet = true;
            if (!QueueBusy(0) && !QueueSuspended(0) && t > 30) break;
        }
        expect(suspended && usedToilet);
        expect(CellX() == 15 && CellY() == 0);   // and then it finished the walk
        bladderEnabled = false;
    }

    it("queueYieldsToNeeds off: the order comes first") {
        Setup(openFloor);
        movers[0].hunger = 1.0f;
        bladderEnabled = true;
        queueYieldsToNeeds = false;
        SpawnFurniture(6, 4, 0, FURNITURE_TOILET, MAT_OAK);
        char said[128];
        expect(QueueCommandAt(0, 15, 0, 0, false, said, sizeof(said)));
        bool suspended = false;
        for (int t = 0; t < 60000 && (QueueBusy(0) || t < 31); t++) {
            if (t == 30) movers[0].bladder = 0.01f;
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            if (QueueSuspended(0)) suspended = true;
        }
        expect(!suspended && CellX() == 15 && CellY() == 0);
        queueYieldsToNeeds = true;
        bladderEnabled = false;
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(sit_then_eat);
    test(action_queue_basics);
    test(player_commands);
    test(linger);
    test(follow_ups_and_interrupts);
    return summary();
}
