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

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(sit_then_eat);
    test(action_queue_basics);
    return summary();
}
