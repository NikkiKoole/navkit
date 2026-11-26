// Levers, pressure plates and links (src/world/mechanisms.c, docs/todo/world/infrastructure.md
// step 3): a linked door switches between open and locked, waits for its doorway to clear before
// it locks, plates weigh movers, animals and items, a worker pulls a lever by order.
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"

static bool test_verbose = false;

#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pipes.h"
#include "../src/world/mechanisms.h"
#include "../src/world/designations.h"
#include "../src/entities/mover.h"
#include "../src/entities/animals.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>

// 12x8 on z=0, a wall across x=6 with a door at (6,4)
static void SetupWorld(void) {
    InitTestGridFromAscii(
        "......#.....\n"
        "......#.....\n"
        "......#.....\n"
        "......#.....\n"
        "......D.....\n"
        "......#.....\n"
        "......#.....\n"
        "......#.....\n");
    GRID(6, 4, 0) = CELL_DOOR;
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers();
    ClearAnimals();
    ClearItems();
    ClearJobs();
    ClearWorkshops();
    ClearStockpiles();
    InitDesignations();
    InitBalance();
    ClearMechanisms();
    plateWeight = 1;
}

static int PutMover(int x, int y) {
    int idx = moverCount++;
    Point goal = { x, y, 0 };
    InitMover(&movers[idx], x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, MOVER_SPEED);
    movers[idx].currentJobId = -1;
    return idx;
}

describe(levers_and_doors) {
    it("a linked lever opens the door when on and locks it when off") {
        SetupWorld();
        int lever = PlaceMechanism(2, 4, 0, MECH_LEVER);
        expect(lever >= 0);
        expect(PlaceMechanism(2, 4, 0, MECH_PLATE) < 0);            // the cell is taken
        expect(LinkMechanism(lever, 6, 4, 0, false) >= 0);
        expect(LinkMechanism(lever, 5, 4, 0, false) < 0);           // not a door or valve
        expect(MechRead(6, 4, 0) == 1);                             // closed
        PullLever(lever);
        expect(mechanisms[lever].on == 1);
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
        expect(MechRead(6, 4, 0) == 2);
        PullLever(lever);
        expect(GRID(6, 4, 0) == CELL_DOOR_LOCKED);
        expect(MechRead(6, 4, 0) == 0);
        expect(!IsCellWalkableAt(0, 4, 6));
        PullLever(lever);
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);                    // unlocked and opened in one go
    }

    it("an inverted link does the opposite, and one lever works several doors") {
        SetupWorld();
        GRID(6, 1, 0) = CELL_DOOR;
        int lever = PlaceMechanism(2, 4, 0, MECH_LEVER);
        LinkMechanism(lever, 6, 4, 0, false);
        LinkMechanism(lever, 6, 1, 0, true);
        expect(MechLinkCount(lever) == 2);
        expect(LinkMechanism(lever, 6, 1, 0, true) >= 0 && MechLinkCount(lever) == 2);   // relinking: no duplicate
        PullLever(lever);
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
        expect(GRID(6, 1, 0) == CELL_DOOR_LOCKED);
        PullLever(lever);
        expect(GRID(6, 4, 0) == CELL_DOOR_LOCKED);
        expect(GRID(6, 1, 0) == CELL_DOOR_OPEN);
    }

    it("a door told to lock waits while someone stands in the doorway, then locks") {
        SetupWorld();
        int lever = PlaceMechanism(2, 4, 0, MECH_LEVER);
        LinkMechanism(lever, 6, 4, 0, false);
        PullLever(lever);                                           // open
        int m = PutMover(6, 4);
        PullLever(lever);                                           // lock, but someone is in it
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);                    // what it is (read) ...
        expect(mechanisms[lever].on == 0);                          // ... differs from what it was told
        expect(MechLockWaiting(6, 4, 0));
        MechanismsTick();
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
        movers[m].x = 7.5f * CELL_SIZE;                             // out of the doorway
        MechanismsTick();
        expect(GRID(6, 4, 0) == CELL_DOOR_LOCKED);
        expect(!MechLockWaiting(6, 4, 0));
    }

    it("pulling it on again while waiting cancels the lock") {
        SetupWorld();
        int lever = PlaceMechanism(2, 4, 0, MECH_LEVER);
        LinkMechanism(lever, 6, 4, 0, false);
        PullLever(lever);
        int m = PutMover(6, 4);
        PullLever(lever);
        PullLever(lever);
        expect(!MechLockWaiting(6, 4, 0));
        movers[m].x = 7.5f * CELL_SIZE;
        MechanismsTick();
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
    }

    it("removing a lever removes its links") {
        SetupWorld();
        int lever = PlaceMechanism(2, 4, 0, MECH_LEVER);
        LinkMechanism(lever, 6, 4, 0, false);
        expect(RemoveMechanism(2, 4, 0));
        expect(MechanismAt(2, 4, 0) < 0);
        expect(MechLinkCount(-1) == 0);
    }
}

describe(pressure_plates) {
    it("a plate is on while a mover stands on it and opens its door") {
        SetupWorld();
        int plate = PlaceMechanism(3, 4, 0, MECH_PLATE);
        LinkMechanism(plate, 6, 4, 0, false);
        MechanismsTick();
        expect(mechanisms[plate].on == 0);
        int m = PutMover(3, 4);
        MechanismsTick();
        expect(mechanisms[plate].on == 1);
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
        movers[m].x = 1.5f * CELL_SIZE;
        MechanismsTick();
        expect(mechanisms[plate].on == 0);
        expect(GRID(6, 4, 0) == CELL_DOOR_LOCKED);
    }

    it("items count by their stack against plateWeight") {
        SetupWorld();
        plateWeight = 3;
        int plate = PlaceMechanism(3, 4, 0, MECH_PLATE);
        int it = SpawnItem(3.5f * CELL_SIZE, 4.5f * CELL_SIZE, 0.0f, ITEM_ROCK);
        BuildItemSpatialGrid();
        MechanismsTick();
        expect(mechanisms[plate].on == 0);                          // 1 < 3
        items[it].stackCount = 3;
        MechanismsTick();
        expect(mechanisms[plate].on == 1);
        plateWeight = 1;
    }
}

describe(valves_and_numbers) {
    it("a lever opens and shuts a pipe valve") {
        SetupWorld();
        SetPipe(9, 2, 0, PIPE_DRAIN | PIPE_VALVE | PIPE_VALVE_CLOSED);
        int lever = PlaceMechanism(8, 2, 0, MECH_LEVER);
        expect(LinkMechanism(lever, 9, 2, 0, false) >= 0);
        expect(MechRead(9, 2, 0) == 0);
        PullLever(lever);
        expect(MechRead(9, 2, 0) == 1);
        expect(!(GetPipe(9, 2, 0) & PIPE_VALVE_CLOSED));
        PullLever(lever);
        expect(MechRead(9, 2, 0) == 0);
        expect(MechRead(1, 1, 0) == -1);                            // nothing there
    }
}

describe(mechanism_save) {
    it("levers, plates, links and a waiting lock survive a save and load") {
        SetupWorld();
        int lever = PlaceMechanism(2, 4, 0, MECH_LEVER);
        PlaceMechanism(3, 4, 0, MECH_PLATE);
        LinkMechanism(lever, 6, 4, 0, true);
        PullLever(lever);                                           // inverted: on locks; nobody in it
        expect(GRID(6, 4, 0) == CELL_DOOR_LOCKED);
        FILE* f = tmpfile();
        MechanismsSave(f);
        ClearMechanisms();
        expect(MechanismCount() == 0);
        rewind(f);
        MechanismsLoad(f);
        fclose(f);
        expect(MechanismCount() == 2);
        expect(mechanisms[lever].on == 1);
        expect(MechLinkCount(lever) == 1);
        PullLever(lever);
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
    }
}

describe(pulling_by_order) {
    it("a worker walks to a lever marked to pull and pulls it") {
        SetupWorld();
        int lever = PlaceMechanism(2, 2, 0, MECH_LEVER);
        LinkMechanism(lever, 6, 4, 0, false);
        expect(DesignatePullLever(2, 2, 0));
        expect(!DesignatePullLever(2, 2, 0));                       // already marked
        expect(!DesignatePullLever(3, 3, 0));                       // no lever
        PutMover(9, 6);                                             // on the far side of the door
        bool pulled = false;
        for (int t = 0; t < 3000 && !pulled; t++) {
            Tick();
            AssignJobs();
            JobsTick();
            pulled = mechanisms[lever].on == 1;
        }
        expect(pulled);
        expect(GRID(6, 4, 0) == CELL_DOOR_OPEN);
        expect(DESIGNATION(2, 2, 0).type == DESIGNATION_NONE);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(levers_and_doors);
    test(pressure_plates);
    test(valves_and_numbers);
    test(mechanism_save);
    test(pulling_by_order);
    return summary();
}
