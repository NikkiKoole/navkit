// The drain spike (src/world/pipes.c, docs/todo/world/infrastructure.md step 4): the pipe byte,
// the drain network pass (which pipes reach an outlet) and the toilet that only flushes when its
// drain does.
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"

static bool test_verbose = false;

#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pipes.h"
#include "../src/world/designations.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/entities/furniture.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/floordirt.h"
#include "../src/simulation/rooms.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>

static void SetupGround(void) {
    InitTestGrid(16, 16);   // a new world: no pipes
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
            GRID(x, y, 1) = CELL_AIR;
        }
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearFurniture();
    ClearJobs();
    InitDesignations();
    InitRooms();
    InitFloorDirt();
    InitBalance();
    bladderEnabled = true;
    toiletsPlumbed = true;   // mains water: the drain decides
    hungerEnabled = thirstEnabled = energyEnabled = false;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    dayLength = 60.0f;
}

static int SetupNeedyMover(int cx, int cy) {
    int idx = moverCount++;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f, cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    movers[idx].bladder = balance.bladderSeekThreshold * 0.5f;
    movers[idx].currentJobId = -1;
    movers[idx].needSearchCooldown = 0.0f;
    return idx;
}

static void UseToiletUntilDone(int mi) {
    for (int i = 0; i < 2000 && movers[mi].bladder < 1.0f; i++) ProcessFreetimeNeeds();
}

describe(drain_network) {
    it("a drain reaches an outlet through its pipes; a gap or no outlet cuts it off") {
        SetupGround();
        expect(PipeDrainCellCount() == 0);
        expect(LayFloorDrain(2, 2, 8, 2, 1) == 7);
        expect(PipeDrainCellCount() == 7);
        expect(!PipeDrainConnected(2, 2, 1));                 // no outlet yet
        SetPipe(8, 2, 1, GetPipe(8, 2, 1) | PIPE_END);
        expect(PipeDrainConnected(2, 2, 1) && PipeDrainConnected(8, 2, 1));
        SetPipe(5, 2, 1, 0);                                  // a gap
        expect(PipeDrainCellCount() == 6);
        expect(!PipeDrainConnected(2, 2, 1) && PipeDrainConnected(6, 2, 1));
    }

    it("an L from one corner to the other turns once") {
        SetupGround();
        expect(LayFloorDrain(2, 2, 5, 6, 1) == 4 + 4);        // (2..5, 2) then (5, 3..6)
        expect(GetPipe(5, 2, 1) & PIPE_FLOOR_DRAIN);
        expect(GetPipe(5, 6, 1) & PIPE_FLOOR_DRAIN);
        expect(!(GetPipe(2, 6, 1) & PIPE_FLOOR_DRAIN));
    }

    it("a shut valve cuts off what lies beyond it, opening it joins it again") {
        SetupGround();
        LayFloorDrain(2, 2, 8, 2, 1);
        SetPipe(8, 2, 1, GetPipe(8, 2, 1) | PIPE_END);
        SetPipe(5, 2, 1, GetPipe(5, 2, 1) | PIPE_VALVE | PIPE_VALVE_CLOSED);
        expect(!PipeDrainConnected(2, 2, 1));
        expect(PipeDrainConnected(5, 2, 1));                  // the valve itself is reached
        SetPipe(5, 2, 1, GetPipe(5, 2, 1) & (uint8_t)~PIPE_VALVE_CLOSED);
        expect(PipeDrainConnected(2, 2, 1));
    }

    it("a stack carries a drain down to an outlet below, never up to one above") {
        SetupGround();
        uint8_t stack = PIPE_WALL_DRAIN | PIPE_STACK;
        SetPipe(4, 4, 0, stack | PIPE_END);                   // the soil stack's foot: an outlet
        SetPipe(4, 4, 1, stack);
        SetPipe(4, 4, 2, stack);
        expect(PipeDrainConnected(4, 4, 2));                  // runs down to the outlet
        SetupGround();
        SetPipe(4, 4, 2, stack | PIPE_END);                   // the outlet on top
        SetPipe(4, 4, 1, stack);
        expect(!PipeDrainConnected(4, 4, 1));                 // would have to run uphill
    }

    it("a fixture is on the drain by its own floor drain or a wall drain next to it") {
        SetupGround();
        SetPipe(6, 5, 1, PIPE_FLOOR_DRAIN | PIPE_END);
        expect(PipeFixtureOnDrain(6, 5, 1));
        expect(!PipeFixtureOnDrain(8, 5, 1));
        SetPipe(9, 5, 1, PIPE_WALL_DRAIN | PIPE_END);
        expect(PipeFixtureOnDrain(8, 5, 1));                  // the wall cell east of it
        expect(!PipeFixtureOnDrain(8, 7, 1));                 // diagonal doesn't count
    }

    it("a new world has no pipes") {
        SetupGround();
        LayFloorDrain(2, 2, 8, 2, 1);
        InitTestGrid(16, 16);
        expect(PipeDrainCellCount() == 0 && GetPipe(2, 2, 1) == 0 && !PipeDrainConnected(2, 2, 1));
    }
}

describe(drain_toilet) {
    it("in a world with drains a toilet off the drain is a dry toilet, on a connected drain it flushes") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        LayFloorDrain(6, 5, 12, 5, 1);                        // ends nowhere
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == toiletDryDirtPerUse);

        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        LayFloorDrain(6, 5, 12, 5, 1);
        SetPipe(12, 5, 1, GetPipe(12, 5, 1) | PIPE_END);
        mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == 0);
    }

    it("no drains anywhere: the old rules (mains water flushes)") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == 0);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(drain_network);
    test(drain_toilet);
    return summary();
}
