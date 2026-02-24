#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/animals.h"
#include "../src/entities/tool_quality.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/world/designations.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/game_state.h"
#include "test_helpers.h"
#include <string.h>

// Helper: set up an animal manually at a specific position (avoids random spawn)
static int SetupAnimalAt(float x, float y, int z, AnimalType type) {
    if (animalCount >= MAX_ANIMALS) return -1;
    int idx = animalCount++;
    Animal* a = &animals[idx];
    memset(a, 0, sizeof(Animal));
    a->x = x;
    a->y = y;
    a->z = (float)z;
    a->type = type;
    a->state = ANIMAL_IDLE;
    a->behavior = BEHAVIOR_SIMPLE_GRAZER;
    a->active = true;
    a->speed = ANIMAL_SPEED;
    a->targetAnimalIdx = -1;
    a->markedForHunt = false;
    a->reservedByHunter = -1;
    return idx;
}

// Helper: count items of a specific type
static int CountItemsOfType(ItemType type) {
    int count = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type) count++;
    }
    return count;
}

// ===========================================================================
// Hunt designation tests
// ===========================================================================
describe(hunt_designation) {
    it("marking animal sets markedForHunt") {
        ClearAnimals();
        int idx = SetupAnimalAt(5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        expect(idx >= 0);
        expect(!animals[idx].markedForHunt);
        animals[idx].markedForHunt = true;
        expect(animals[idx].markedForHunt);
    }

    it("reservedByHunter defaults to -1") {
        ClearAnimals();
        int idx = SetupAnimalAt(5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        expect(animals[idx].reservedByHunter == -1);
    }

    it("GetAnimalAtGrid finds animal at cell") {
        ClearAnimals();
        int idx = SetupAnimalAt(5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        int found = GetAnimalAtGrid(5, 5, 0);
        expect(found == idx);
    }

    it("GetAnimalAtGrid returns -1 for empty cell") {
        ClearAnimals();
        SetupAnimalAt(5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        int found = GetAnimalAtGrid(9, 9, 0);
        expect(found == -1);
    }

    it("GetAnimalAtGrid has 1-cell tolerance") {
        ClearAnimals();
        // Place animal at cell boundary (between cell 4 and 5)
        int idx = SetupAnimalAt(5.0f * CELL_SIZE, 5.0f * CELL_SIZE, 0, ANIMAL_GRAZER);
        int found = GetAnimalAtGrid(4, 4, 0);
        expect(found == idx);
    }
}

// ===========================================================================
// WorkGiver_Hunt tests
// ===========================================================================
describe(workgiver_hunt) {
    it("assigns hunt job to idle mover for marked animal") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        // Create mover at (2,2)
        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        // Create marked animal at (7,7)
        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        // WorkGiver should assign
        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId >= 0);

        Job* job = GetJob(jobId);
        expect(job->type == JOBTYPE_HUNT);
        expect(job->targetAnimalIdx == animalIdx);
        expect(job->assignedMover == 0);
        expect(animals[animalIdx].reservedByHunter == 0);
    }

    it("does not assign if animal is already reserved") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;
        animals[animalIdx].reservedByHunter = 5;  // Already reserved

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId == -1);
    }

    it("does not assign if mover cannot hunt") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        movers[0].capabilities.canHunt = false;
        moverCount = 1;

        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId == -1);
    }

    it("does not assign for unmarked animal") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        // Not marked

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId == -1);
    }
}

// ===========================================================================
// Hunt job cancellation tests
// ===========================================================================
describe(hunt_cancel) {
    it("CancelJob releases animal reservation") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId >= 0);
        expect(animals[animalIdx].reservedByHunter == 0);

        // Cancel
        CancelJob(&movers[0], 0);
        expect(animals[animalIdx].reservedByHunter == -1);
        expect(movers[0].currentJobId == -1);
    }

    it("CancelJob unfreezes animal in BEING_HUNTED state") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        WorkGiver_Hunt(0);

        // Simulate animal being frozen during attack
        animals[animalIdx].state = ANIMAL_BEING_HUNTED;

        CancelJob(&movers[0], 0);
        expect(animals[animalIdx].state != ANIMAL_BEING_HUNTED);
        expect(animals[animalIdx].reservedByHunter == -1);
    }

    it("animal dies mid-hunt cancels job cleanly") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 7.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();
        int jobId = WorkGiver_Hunt(0);
        expect(jobId >= 0);

        // Kill animal externally (e.g., predator)
        KillAnimal(animalIdx);
        expect(!animals[animalIdx].active);

        // Job driver should fail on next tick
        Job* job = GetJob(jobId);
        JobRunResult result = RunJob_Hunt(job, &movers[0], 0.016f);
        expect(result == JOBRUN_FAIL);
    }
}

// ===========================================================================
// Full hunt E2E test
// ===========================================================================
describe(hunt_e2e) {
    it("mover chases animal and produces carcass") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearJobs();
        ClearAnimals();
        InitBalance();
        toolRequirementsEnabled = false;

        // Mover at (2,2), animal at (7,5) — close enough to catch quickly
        Point goal = {2, 2, 0};
        InitMover(&movers[0], 2.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        int animalIdx = SetupAnimalAt(7.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        animals[animalIdx].markedForHunt = true;

        // Tick until carcass appears or timeout
        bool done = false;
        for (int tick = 0; tick < 3000 && !done; tick++) {
            Tick();
            AssignJobs();
            JobsTick();

            if (CountItemsOfType(ITEM_CARCASS) > 0) {
                done = true;
            }
        }

        expect(done);
        expect(CountItemsOfType(ITEM_CARCASS) == 1);
        // Animal should be dead
        expect(!animals[animalIdx].active);
        // Mover should be idle again
        expect(movers[0].currentJobId == -1);
    }

    it("drag-select marks multiple animals") {
        ClearAnimals();
        int a1 = SetupAnimalAt(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        int a2 = SetupAnimalAt(5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0, ANIMAL_GRAZER);
        int a3 = SetupAnimalAt(9.5f * CELL_SIZE, 9.5f * CELL_SIZE, 0, ANIMAL_GRAZER);  // outside range

        // Simulate marking animals in rectangle (2,2) to (6,6)
        for (int i = 0; i < animalCount; i++) {
            Animal* a = &animals[i];
            if (!a->active) continue;
            int ax = (int)(a->x / CELL_SIZE);
            int ay = (int)(a->y / CELL_SIZE);
            if (ax >= 2 && ax <= 6 && ay >= 2 && ay <= 6) {
                a->markedForHunt = true;
            }
        }

        expect(animals[a1].markedForHunt);
        expect(animals[a2].markedForHunt);
        expect(!animals[a3].markedForHunt);
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(hunt_designation);
    test(workgiver_hunt);
    test(hunt_cancel);
    test(hunt_e2e);

    return summary();
}
