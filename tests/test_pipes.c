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
#include "../src/world/construction.h"
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
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);
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

    it("pipes survive a save and load (v126): bits, network, counts") {
        SetupGround();
        LayFloorDrain(2, 2, 8, 4, 1);
        SetPipe(8, 4, 1, GetPipe(8, 4, 1) | PIPE_END);
        SetPipe(5, 2, 1, GetPipe(5, 2, 1) | PIPE_VALVE | PIPE_VALVE_CLOSED);
        SetPipe(12, 12, 1, PIPE_FLOOR_DRAIN);                 // a lone, unconnected drain
        int cells = PipeDrainCellCount();
        expect(SaveWorld("/tmp/test_pipes_save.bin"));
        ClearPipes();
        expect(PipeDrainCellCount() == 0);
        expect(LoadWorld("/tmp/test_pipes_save.bin"));
        expect(PipeDrainCellCount() == cells);
        expect(GetPipe(8, 4, 1) == (PIPE_FLOOR_DRAIN | PIPE_END));
        expect(GetPipe(5, 2, 1) & PIPE_VALVE_CLOSED);
        expect(PipeDrainConnected(6, 2, 1));                  // outlet side of the shut valve
        expect(!PipeDrainConnected(2, 2, 1));                 // beyond it
        expect(!PipeDrainConnected(12, 12, 1));
        SetPipe(5, 2, 1, GetPipe(5, 2, 1) & (uint8_t)~PIPE_VALVE_CLOSED);   // the list works after a load
        expect(PipeDrainConnected(2, 2, 1));
    }

    it("a new world has no pipes") {
        SetupGround();
        LayFloorDrain(2, 2, 8, 2, 1);
        InitTestGrid(16, 16);
        expect(PipeDrainCellCount() == 0 && GetPipe(2, 2, 1) == 0 && !PipeDrainConnected(2, 2, 1));
    }
}

describe(water_pipes) {
    it("water pipe openings: a network's other ends; a shut valve splits it; drains are no openings") {
        SetupGround();
        expect(PipeOpeningCount() == 0);
        LayFloorSupply(2, 2, 10, 2, 1);
        SetPipe(2, 2, 1, GetPipe(2, 2, 1) | PIPE_END);
        SetPipe(10, 2, 1, GetPipe(10, 2, 1) | PIPE_END);
        LayFloorDrain(2, 6, 6, 6, 1);
        SetPipe(6, 6, 1, GetPipe(6, 6, 1) | PIPE_END);        // a drain outlet: not a water opening
        expect(PipeOpeningCount() == 2);
        expect(PipeIsOpening(2, 2, 1) && PipeIsOpening(10, 2, 1) && !PipeIsOpening(6, 6, 1) && !PipeIsOpening(5, 2, 1));
        int ox[4], oy[4], oz[4];
        expect(PipeOtherOpenings(2, 2, 1, ox, oy, oz, 4) == 1);
        expect(ox[0] == 10 && oy[0] == 2);
        expect(PIPE_CONN(5, 2, 1) & PIPE_CONN_SUPPLY);
        SetPipe(6, 2, 1, GetPipe(6, 2, 1) | PIPE_VALVE | PIPE_VALVE_CLOSED);
        expect(PipeOtherOpenings(2, 2, 1, ox, oy, oz, 4) == 0);
        expect(!(PIPE_CONN(5, 2, 1) & PIPE_CONN_SUPPLY));
    }

    it("a stack joins water pipes across levels") {
        SetupGround();
        SetPipe(3, 3, 1, PIPE_FLOOR_SUPPLY | PIPE_END);
        SetPipe(4, 3, 1, PIPE_WALL_SUPPLY | PIPE_STACK);
        SetPipe(4, 3, 0, PIPE_WALL_SUPPLY | PIPE_STACK);
        SetPipe(5, 3, 0, PIPE_FLOOR_SUPPLY | PIPE_END);
        int ox[4], oy[4], oz[4];
        expect(PipeOtherOpenings(3, 3, 1, ox, oy, oz, 4) == 1);
        expect(ox[0] == 5 && oz[0] == 0);
    }
}

describe(drain_blueprints) {
    it("a drain blueprint goes on walkable ground without a drain; built, it is a floor drain") {
        SetupGround();
        int bp = CreateRecipeBlueprint(4, 4, 1, CONSTRUCTION_DRAIN);
        expect(bp >= 0);
        expect(GetPipe(4, 4, 1) == 0);                        // a plan, not a pipe yet
        CompleteBlueprint(bp);
        expect(!blueprints[bp].active);
        expect(GetPipe(4, 4, 1) == PIPE_FLOOR_DRAIN);
        expect(GRID(4, 4, 1) == CELL_AIR);                    // the cell itself is unchanged
        expect(CreateRecipeBlueprint(4, 4, 1, CONSTRUCTION_DRAIN) < 0);   // already a drain
        expect(CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRAIN) < 0);   // inside the ground
    }

    it("an outlet goes on a drain (or bare ground) and makes it the drain's end") {
        SetupGround();
        LayFloorDrain(2, 2, 6, 2, 1);
        int bp = CreateRecipeBlueprint(6, 2, 1, CONSTRUCTION_DRAIN_OUTLET);
        expect(bp >= 0);
        expect(!PipeDrainConnected(2, 2, 1));
        CompleteBlueprint(bp);
        expect(GetPipe(6, 2, 1) == (PIPE_FLOOR_DRAIN | PIPE_END));
        expect(PipeDrainConnected(2, 2, 1));
        expect(CreateRecipeBlueprint(6, 2, 1, CONSTRUCTION_DRAIN_OUTLET) < 0);   // already an outlet
        int lone = CreateRecipeBlueprint(9, 9, 1, CONSTRUCTION_DRAIN_OUTLET);
        expect(lone >= 0);
        CompleteBlueprint(lone);
        expect(GetPipe(9, 9, 1) == (PIPE_FLOOR_DRAIN | PIPE_END));
    }

    it("drains cost a brick, an outlet two") {
        expect(GetConstructionRecipe(CONSTRUCTION_DRAIN)->stages[0].inputs[0].alternatives[0].itemType == ITEM_BRICKS);
        expect(GetConstructionRecipe(CONSTRUCTION_DRAIN)->stages[0].inputs[0].count == 1);
        expect(GetConstructionRecipe(CONSTRUCTION_DRAIN_OUTLET)->stages[0].inputs[0].count == 2);
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

    it("a flush on a connected drain sends a pulse from the toilet along the drain to the outlet") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        LayFloorDrain(6, 5, 12, 8, 1);                        // east to 12, then south to 8
        SetPipe(12, 8, 1, GetPipe(12, 8, 1) | PIPE_END);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        int k = -1;
        for (int i = 0; i < PIPE_PULSE_MAX; i++) if (pipePulses[i].active) k = i;
        expect(k >= 0);
        if (k >= 0) {
            const PipePulse* p = &pipePulses[k];
            expect(p->len == 7 + 3);                          // (6..12, 5) then (12, 6..8)
            expect(p->x[0] == 6 && p->y[0] == 5);
            expect(p->x[p->len - 1] == 12 && p->y[p->len - 1] == 8);
        }
    }

    it("no drains anywhere: the old rules (mains water flushes)") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == 0);
        bool anyPulse = false;   // nothing to run down
        for (int i = 0; i < PIPE_PULSE_MAX; i++) if (pipePulses[i].active) anyPulse = true;
        expect(!anyPulse);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(drain_network);
    test(water_pipes);
    test(drain_blueprints);
    test(drain_toilet);
    return summary();
}
