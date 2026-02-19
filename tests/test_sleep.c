#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/simulation/plants.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/entities/furniture.h"
#include "../src/simulation/weather.h"
#include "../src/core/time.h"
#include "../src/world/designations.h"
#include "test_helpers.h"
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up a flat walkable 10x10 grid at z=1
static void SetupFlatGrid(void) {
    InitTestGrid(10, 10);
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 10; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
            SET_FLOOR(x, y, 1);
        }
    }
}

static void SetupClean(void) {
    SetupFlatGrid();
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearFurniture();
    ClearJobs();
    ClearPlants();
    InitDesignations();
    InitBalance();
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    daysPerSeason = 7;
    dayNumber = 8;
}

static int SetupMover(int cx, int cy) {
    int idx = moverCount;
    moverCount = idx + 1;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f,
              cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    return idx;
}

// Simulate one tick of needs: drain + freetime processing (what the game loop does)
static void SimNeedsTick(void) {
    NeedsTick();
    ProcessFreetimeNeeds();
}

// =============================================================================
// Story: A mover who has been working all day should eventually get tired
//        and lie down to sleep on the ground
// =============================================================================

describe(mover_gets_tired_and_sleeps) {
    it("an idle mover gradually loses energy over time") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f; // keep fed so hunger doesn't interfere

        float startEnergy = movers[mi].energy;
        expect(startEnergy == 1.0f);

        // Simulate 10 seconds of idle time (600 ticks)
        for (int i = 0; i < 600; i++) {
            movers[mi].hunger = 1.0f; // pin hunger high
            SimNeedsTick();
        }

        // Player expects: mover should have less energy than when they started
        expect(movers[mi].energy < startEnergy);
        // After 10s idle at 0.01/s, should be ~0.9
        expect(movers[mi].energy > 0.85f);
        expect(movers[mi].energy < 0.95f);
    }

    it("a working mover drains energy faster than an idle one") {
        SetupClean();
        int mi_idle = SetupMover(3, 3);
        int mi_work = SetupMover(7, 7);
        movers[mi_idle].hunger = 1.0f;
        movers[mi_work].hunger = 1.0f;
        movers[mi_work].currentJobId = 0; // fake job

        for (int i = 0; i < 600; i++) {
            movers[mi_idle].hunger = 1.0f;
            movers[mi_work].hunger = 1.0f;
            SimNeedsTick();
        }

        // Player expects: the working mover should be more tired
        expect(movers[mi_work].energy < movers[mi_idle].energy);
    }

    it("when energy drops below tired threshold, an idle mover lies down to rest") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.19f; // just below tired threshold (0.2)

        SimNeedsTick();

        // Player expects: mover should be resting on the ground
        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }

    it("a resting mover recovers energy and eventually wakes up") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.1f; // very tired

        // First tick: should start resting
        SimNeedsTick();
        expect(movers[mi].freetimeState == FREETIME_RESTING);

        // Ground rest is slow: net rate is (0.012 recovery - 0.010 drain) = 0.002/s
        // Need 0.7 energy to reach 0.8 from 0.1: 0.7 / 0.002 = 350s = 21000 ticks
        // But NeedsTick skips drain during FREETIME_RESTING, so actual rate is 0.012/s
        // 0.7 / 0.012 = ~58s = ~3500 ticks. BUT energy also drains between the
        // moment the mover stops resting. Let's just run until they wake.
        int tickCount = 0;
        while (movers[mi].freetimeState == FREETIME_RESTING && tickCount < 30000) {
            movers[mi].hunger = 1.0f; // keep fed
            SimNeedsTick();
            tickCount++;
        }

        // Player expects: mover should have woken up and be available again
        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].energy >= balance.energyWakeThreshold);

        if (test_verbose) {
            printf("  Mover recovered from 0.1 in %d ticks (%.1f seconds)\n",
                   tickCount, tickCount * TICK_DT);
        }
    }
}

// =============================================================================
// Story: An exhausted mover should drop what they're doing to sleep,
//        but a merely tired mover should finish their job first
// =============================================================================

describe(exhaustion_interrupts_work) {
    it("an exhausted mover abandons their job to sleep") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.05f; // exhausted (<0.1)

        // Give mover a real job
        int ji = CreateJob(JOBTYPE_HAUL);
        Job* j = GetJob(ji);
        j->targetItem = -1;
        j->carryingItem = -1;
        j->targetStockpile = -1;
        j->targetWorkshop = -1;
        j->fuelItem = -1;
        movers[mi].currentJobId = ji;
        j->assignedMover = mi;

        SimNeedsTick();

        // Player expects: mover stops working and lies down
        expect(movers[mi].currentJobId == -1);
        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }

    it("a tired mover with a job keeps working until the job is done") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.25f; // tired but not exhausted

        int ji = CreateJob(JOBTYPE_HAUL);
        Job* j = GetJob(ji);
        j->targetItem = -1;
        j->carryingItem = -1;
        j->targetStockpile = -1;
        j->targetWorkshop = -1;
        j->fuelItem = -1;
        movers[mi].currentJobId = ji;
        j->assignedMover = mi;

        SimNeedsTick();

        // Player expects: mover continues their job — they're tired but not collapsing
        expect(movers[mi].currentJobId == ji);
        expect(movers[mi].freetimeState == FREETIME_NONE);
    }
}

// =============================================================================
// Story: A starving mover who is also exhausted should eat first — you
//        can't sleep on an empty stomach when you're about to die
// =============================================================================

describe(hunger_trumps_sleep) {
    it("a starving AND exhausted mover seeks food, not sleep") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.05f;  // exhausted
        movers[mi].hunger = 0.05f;  // starving

        // Place food nearby so the search succeeds
        int spIdx = CreateStockpile(6, 5, 1, 2, 2);
        int itemIdx = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f,
                                5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 6, 5, itemIdx);

        SimNeedsTick();

        // Player expects: mover goes to eat, not sleep
        expect(movers[mi].freetimeState == FREETIME_SEEKING_FOOD);
    }

    it("an exhausted mover who is not starving sleeps instead of eating") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.05f;  // exhausted
        movers[mi].hunger = 0.5f;   // perfectly fine

        SimNeedsTick();

        // Player expects: mover lies down — no need to eat
        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }

    it("a hungry mover who is not tired eats, doesn't sleep") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.5f;   // fine
        movers[mi].hunger = 0.2f;   // hungry

        int spIdx = CreateStockpile(6, 5, 1, 2, 2);
        int itemIdx = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f,
                                5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 6, 5, itemIdx);

        SimNeedsTick();

        expect(movers[mi].freetimeState == FREETIME_SEEKING_FOOD);
    }

    it("a tired mover who is not hungry sleeps, doesn't eat") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.2f;   // tired
        movers[mi].hunger = 0.5f;   // fine

        SimNeedsTick();

        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }
}

// =============================================================================
// Story: A sleeping mover who starts starving should wake up to eat,
//        then go back to sleep if still tired
// =============================================================================

describe(starvation_wakes_sleeper) {
    it("a sleeping mover wakes up when they start starving") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.3f;
        movers[mi].hunger = 0.05f;  // starving threshold
        movers[mi].freetimeState = FREETIME_RESTING;
        movers[mi].needTarget = -1;

        SimNeedsTick();

        // Player expects: mover wakes up — starvation overrides sleep
        expect(movers[mi].freetimeState != FREETIME_RESTING);
    }

    it("a sleeping mover who is merely hungry stays asleep") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.3f;
        movers[mi].hunger = 0.25f;  // hungry but not starving
        movers[mi].freetimeState = FREETIME_RESTING;
        movers[mi].needTarget = -1;

        SimNeedsTick();

        // Player expects: mover keeps sleeping — hunger isn't dire enough
        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }

    it("a mover who wakes from starvation eats then goes back to sleep") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.2f;   // still tired after waking
        movers[mi].hunger = 0.05f;  // starving
        movers[mi].freetimeState = FREETIME_RESTING;
        movers[mi].needTarget = -1;

        // Place food right at mover's position for immediate pickup
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);

        // Wake up from starvation
        SimNeedsTick();
        expect(movers[mi].freetimeState != FREETIME_RESTING);

        // Next tick: should start seeking food (starvation priority)
        SimNeedsTick();

        // The mover is at the food, so should quickly transition to eating
        // Run through seeking → eating → done cycle
        for (int i = 0; i < 200; i++) {
            movers[mi].energy = 0.2f; // keep tired (pin energy so it doesn't change from drain)
            SimNeedsTick();
        }

        // After eating, mover should be FREETIME_NONE momentarily
        // then on next tick should go back to rest since still tired
        // Give it a couple ticks to settle
        movers[mi].energy = 0.2f; // still tired
        movers[mi].hunger = 0.5f; // fed now
        movers[mi].freetimeState = FREETIME_NONE;
        movers[mi].needSearchCooldown = 0.0f;

        SimNeedsTick();

        // Player expects: mover goes back to sleep since they're still tired
        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }
}

// =============================================================================
// Story: A mover's full day cycle — work until tired, sleep, wake refreshed
// =============================================================================

describe(full_day_cycle) {
    it("a mover works until tired, sleeps on the ground, and wakes up refreshed") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 1.0f;

        // Phase 1: Simulate idle drain until mover becomes tired
        int ticksUntilTired = 0;
        while (movers[mi].energy >= balance.energyTiredThreshold && ticksUntilTired < 10000) {
            movers[mi].hunger = 1.0f; // keep fed
            SimNeedsTick();
            ticksUntilTired++;
        }

        // Mover should have become tired and started resting
        // (energy dropped below 0.3, freetime triggered rest)
        expect(movers[mi].energy < balance.energyTiredThreshold);

        // Give it one more tick to enter resting if not already
        movers[mi].hunger = 1.0f;
        SimNeedsTick();
        expect(movers[mi].freetimeState == FREETIME_RESTING);

        if (test_verbose) {
            printf("  Mover became tired after %d ticks (%.1f seconds)\n",
                   ticksUntilTired, ticksUntilTired * TICK_DT);
        }

        // Phase 2: Sleep until recovered
        int ticksUntilWake = 0;
        while (movers[mi].freetimeState == FREETIME_RESTING && ticksUntilWake < 10000) {
            movers[mi].hunger = 1.0f;
            SimNeedsTick();
            ticksUntilWake++;
        }

        // Player expects: mover eventually wakes up refreshed and ready to work
        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].energy >= balance.energyWakeThreshold);

        if (test_verbose) {
            printf("  Mover slept for %d ticks (%.1f seconds), energy now %.2f\n",
                   ticksUntilWake, ticksUntilWake * TICK_DT, movers[mi].energy);
        }

        // Sanity: the total cycle should be reasonable (not instant, not forever)
        // Idle drain 0.01/s: 0.7 energy / 0.01 = 70s = 4200 ticks
        // Ground rest 0.012/s: 0.5 energy / 0.012 = ~42s = 2500 ticks
        expect(ticksUntilTired > 2000);
        expect(ticksUntilTired < 8000);
        expect(ticksUntilWake > 1000);
        expect(ticksUntilWake < 6000);
    }
}

// =============================================================================
// Story: A tired mover with a bed nearby should prefer sleeping in the bed
//        over sleeping on the ground — and recover faster
// =============================================================================

describe(furniture_rest_seeking) {
    it("a tired mover chooses a nearby bed over ground rest") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.19f; // just below tired threshold

        // Place a plank bed nearby
        int fi = SpawnFurniture(7, 5, 1, FURNITURE_PLANK_BED, 0);
        expect(fi >= 0);

        SimNeedsTick();

        // Player expects: mover seeks the bed, not ground rest
        expect(movers[mi].freetimeState == FREETIME_SEEKING_REST);
        expect(movers[mi].needTarget == fi);
        expect(furniture[fi].occupant == mi);
    }

    it("a tired mover picks the best furniture when multiple are available") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.19f;

        // Place a chair and a bed at similar distances
        int chair = SpawnFurniture(7, 5, 1, FURNITURE_CHAIR, 0);
        int bed = SpawnFurniture(7, 6, 1, FURNITURE_PLANK_BED, 0);
        expect(chair >= 0);
        expect(bed >= 0);

        SimNeedsTick();

        // Player expects: mover picks the bed (higher rest rate)
        expect(movers[mi].freetimeState == FREETIME_SEEKING_REST);
        expect(movers[mi].needTarget == bed);
        expect(furniture[bed].occupant == mi);
        expect(furniture[chair].occupant == -1); // chair not reserved
    }

    it("two movers competing for one bed — second falls back to ground") {
        SetupClean();
        int mi1 = SetupMover(5, 5);
        int mi2 = SetupMover(6, 5);
        movers[mi1].hunger = 1.0f;
        movers[mi2].hunger = 1.0f;
        movers[mi1].energy = 0.19f;
        movers[mi2].energy = 0.19f;

        // Only one bed
        int bed = SpawnFurniture(8, 5, 1, FURNITURE_PLANK_BED, 0);
        expect(bed >= 0);

        SimNeedsTick();

        // One mover gets the bed, the other gets ground rest
        bool mi1_got_bed = (movers[mi1].needTarget == bed);
        bool mi2_got_bed = (movers[mi2].needTarget == bed);

        // Exactly one should have the bed
        expect(mi1_got_bed != mi2_got_bed);

        // The other should be on ground rest
        if (mi1_got_bed) {
            expect(movers[mi1].freetimeState == FREETIME_SEEKING_REST);
            expect(movers[mi2].freetimeState == FREETIME_RESTING);
            expect(movers[mi2].needTarget == -1);
        } else {
            expect(movers[mi2].freetimeState == FREETIME_SEEKING_REST);
            expect(movers[mi1].freetimeState == FREETIME_RESTING);
            expect(movers[mi1].needTarget == -1);
        }
    }

    it("a mover resting in a bed recovers faster than on the ground") {
        SetupClean();
        int mi_bed = SetupMover(5, 5);
        int mi_ground = SetupMover(8, 8);
        movers[mi_bed].hunger = 1.0f;
        movers[mi_ground].hunger = 1.0f;
        movers[mi_bed].energy = 0.5f;
        movers[mi_ground].energy = 0.5f;

        // Put bed mover directly into resting-in-bed state
        int fi = SpawnFurniture(5, 5, 1, FURNITURE_PLANK_BED, 0);
        furniture[fi].occupant = mi_bed;
        movers[mi_bed].freetimeState = FREETIME_RESTING;
        movers[mi_bed].needTarget = fi;

        // Put ground mover into ground rest
        movers[mi_ground].freetimeState = FREETIME_RESTING;
        movers[mi_ground].needTarget = -1;

        // Simulate 5 seconds (300 ticks)
        for (int i = 0; i < 300; i++) {
            movers[mi_bed].hunger = 1.0f;
            movers[mi_ground].hunger = 1.0f;
            SimNeedsTick();
        }

        // Bed rate is 0.040/s, ground is 0.012/s — bed should be WAY ahead
        // After 5s: bed = 0.5 + 0.2 = 0.7, ground = 0.5 + 0.06 = 0.56
        expect(movers[mi_bed].energy > movers[mi_ground].energy);
        expect(movers[mi_bed].energy > 0.65f);
        expect(movers[mi_ground].energy < 0.6f);
    }

    it("a mover waking from a bed releases the furniture reservation") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.5f;

        int fi = SpawnFurniture(5, 5, 1, FURNITURE_PLANK_BED, 0);
        furniture[fi].occupant = mi;
        movers[mi].freetimeState = FREETIME_RESTING;
        movers[mi].needTarget = fi;

        // Run until mover wakes (bed rate 0.040/s, need 0.3 more = ~7.5s)
        int ticks = 0;
        while (movers[mi].freetimeState == FREETIME_RESTING && ticks < 10000) {
            movers[mi].hunger = 1.0f;
            SimNeedsTick();
            ticks++;
        }

        // Player expects: mover woke up and bed is free
        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].energy >= balance.energyWakeThreshold);
        expect(furniture[fi].occupant == -1);
    }

    it("starvation wakes a mover from a bed and releases the reservation") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].energy = 0.3f;
        movers[mi].hunger = 0.05f;  // starving

        int fi = SpawnFurniture(5, 5, 1, FURNITURE_PLANK_BED, 0);
        furniture[fi].occupant = mi;
        movers[mi].freetimeState = FREETIME_RESTING;
        movers[mi].needTarget = fi;

        SimNeedsTick();

        // Player expects: mover wakes up to eat, bed is released
        expect(movers[mi].freetimeState != FREETIME_RESTING);
        expect(furniture[fi].occupant == -1);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    test_verbose = verbose;
    if (!verbose) {
        if (quiet) {
            set_quiet_mode(1);
        }
        SetTraceLogLevel(LOG_NONE);
    }

    test(mover_gets_tired_and_sleeps);
    test(exhaustion_interrupts_work);
    test(hunger_trumps_sleep);
    test(starvation_wakes_sleeper);
    test(full_day_cycle);
    test(furniture_rest_seeking);

    return 0;
}
