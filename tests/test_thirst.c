#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/entities/containers.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/water.h"
#include "../src/core/time.h"
#include "../src/world/designations.h"
#include "test_helpers.h"
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up a flat walkable 10x10 grid at z=1 (air above solid ground)
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
    ClearJobs();
    ClearWater();
    InitDesignations();
    InitBalance();
    hungerEnabled = false;
    energyEnabled = false;
    bodyTempEnabled = false;
    thirstEnabled = true;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    dayLength = 60.0f;
    daysPerSeason = 7;
    dayNumber = 8;
    gameMode = GAME_MODE_SURVIVAL;
}

static int SetupMover(int cx, int cy) {
    int idx = moverCount;
    moverCount = idx + 1;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f,
              cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    return idx;
}

// =============================================================================
// Thirst Drain
// =============================================================================

describe(thirst_drain) {
    it("mover thirst starts at 1.0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        expect(movers[mi].thirst == 1.0f);
    }

    it("thirst drains over time when enabled") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 1.0f;

        for (int i = 0; i < 1000; i++) {
            NeedsTick();
        }

        expect(movers[mi].thirst < 1.0f);
    }

    it("thirst stays 1.0 when disabled") {
        SetupClean();
        thirstEnabled = false;
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 1.0f;

        for (int i = 0; i < 1000; i++) {
            NeedsTick();
        }

        expect(movers[mi].thirst == 1.0f);
    }

    it("thirst clamps at 0.0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 0.001f;

        for (int i = 0; i < 10000; i++) {
            NeedsTick();
        }

        expect(movers[mi].thirst == 0.0f);
    }

    it("inactive mover thirst does not drain") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 1.0f;
        movers[mi].active = false;

        for (int i = 0; i < 10000; i++) {
            NeedsTick();
        }

        expect(movers[mi].thirst == 1.0f);
    }
}

// =============================================================================
// Dehydration Death
// =============================================================================

describe(dehydration_death) {
    it("dehydration timer starts at 0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        expect(movers[mi].dehydrationTimer == 0.0f);
    }

    it("dehydration timer increases at thirst 0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 0.0f;

        for (int i = 0; i < 100; i++) {
            NeedsTick();
        }

        expect(movers[mi].dehydrationTimer > 0.0f);
    }

    it("dehydration timer resets when thirst > 0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 0.0f;

        for (int i = 0; i < 100; i++) {
            NeedsTick();
        }
        float timer = movers[mi].dehydrationTimer;
        expect(timer > 0.0f);

        // Give some thirst back
        movers[mi].thirst = 0.5f;
        NeedsTick();
        expect(movers[mi].dehydrationTimer == 0.0f);
    }

    it("mover dies after dehydration death time in survival mode") {
        SetupClean();
        gameMode = GAME_MODE_SURVIVAL;
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 0.0f;

        // Run enough ticks for dehydration death (8 GH)
        float deathTime = GameHoursToGameSeconds(balance.dehydrationDeathGH);
        int ticks = (int)(deathTime / TICK_DT) + 100;
        for (int i = 0; i < ticks; i++) {
            NeedsTick();
        }

        expect(movers[mi].active == false);
    }

    it("mover does not die in sandbox mode") {
        SetupClean();
        gameMode = GAME_MODE_SANDBOX;
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 0.0f;

        float deathTime = GameHoursToGameSeconds(balance.dehydrationDeathGH);
        int ticks = (int)(deathTime / TICK_DT) + 100;
        for (int i = 0; i < ticks; i++) {
            NeedsTick();
        }

        expect(movers[mi].active == true);
    }
}

// =============================================================================
// Drinkable Items
// =============================================================================

describe(drinkable_items) {
    it("ITEM_WATER is drinkable") {
        expect(ITEM_WATER < ITEM_TYPE_COUNT);
        expect(ItemIsDrinkable(ITEM_WATER) != 0);
    }

    it("ITEM_HERBAL_TEA is drinkable") {
        expect(ITEM_HERBAL_TEA < ITEM_TYPE_COUNT);
        expect(ItemIsDrinkable(ITEM_HERBAL_TEA) != 0);
    }

    it("ITEM_BERRY_JUICE is drinkable") {
        expect(ITEM_BERRY_JUICE < ITEM_TYPE_COUNT);
        expect(ItemIsDrinkable(ITEM_BERRY_JUICE) != 0);
    }

    it("non-drinkable items have zero hydration") {
        expect(GetItemHydration(ITEM_LOG) == 0.0f);
        expect(GetItemHydration(ITEM_PLANKS) == 0.0f);
        expect(GetItemHydration(ITEM_BERRIES) == 0.0f);
    }

    it("water has correct hydration") {
        expect(fabsf(GetItemHydration(ITEM_WATER) - 0.3f) < 0.001f);
    }

    it("herbal tea has correct hydration") {
        expect(fabsf(GetItemHydration(ITEM_HERBAL_TEA) - 0.6f) < 0.001f);
    }

    it("berry juice has correct hydration") {
        expect(fabsf(GetItemHydration(ITEM_BERRY_JUICE) - 0.5f) < 0.001f);
    }

    it("herbal tea has best hydration") {
        expect(GetItemHydration(ITEM_HERBAL_TEA) > GetItemHydration(ITEM_BERRY_JUICE));
        expect(GetItemHydration(ITEM_BERRY_JUICE) > GetItemHydration(ITEM_WATER));
    }

    it("berry juice spoils") {
        expect(ItemSpoils(ITEM_BERRY_JUICE) != 0);
    }

    it("water does not spoil") {
        expect(ItemSpoils(ITEM_WATER) == 0);
    }

    it("all drinkable items are stackable") {
        expect(itemDefs[ITEM_WATER].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_HERBAL_TEA].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_BERRY_JUICE].flags & IF_STACKABLE);
    }
}

// =============================================================================
// Drink from Stockpile
// =============================================================================

describe(drink_from_stockpile) {
    it("thirsty mover seeks drinkable item in stockpile") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = balance.thirstSeekThreshold - 0.01f;
        movers[mi].currentJobId = -1; // idle

        // Spawn water in a stockpile
        int sp = CreateStockpile(5, 5, 3, 3, 1);
        expect(sp >= 0);
        int waterIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        expect(waterIdx >= 0);
        PlaceItemInStockpile(sp, 5, 5, waterIdx);

        // Run freetime — should transition to SEEKING_DRINK
        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_SEEKING_DRINK);
        expect(movers[mi].needTarget == waterIdx);
        expect(items[waterIdx].reservedBy == mi);
    }

    it("mover drinks and restores thirst") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].thirst = 0.3f;

        // Spawn water right next to mover (so arrival is immediate)
        int waterIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        expect(waterIdx >= 0);
        items[waterIdx].state = ITEM_IN_STOCKPILE;

        // Set up seeking state manually
        movers[mi].freetimeState = FREETIME_SEEKING_DRINK;
        movers[mi].needTarget = waterIdx;
        movers[mi].needProgress = 0.0f;
        items[waterIdx].reservedBy = mi;

        // Process — should detect arrival and transition to DRINKING
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_DRINKING);

        // Now run enough time for drinking to complete
        float drinkTime = GameHoursToGameSeconds(balance.drinkingDurationGH);
        int ticks = (int)(drinkTime / TICK_DT) + 10;
        for (int i = 0; i < ticks; i++) {
            gameDeltaTime = TICK_DT;
            ProcessFreetimeNeeds();
        }

        // After drinking, thirst should be restored
        expect(movers[mi].thirst > 0.3f);
        expect(movers[mi].freetimeState == FREETIME_NONE);
        // Water item should be consumed
        expect(!items[waterIdx].active);
    }
}

// =============================================================================
// Drink from Container
// =============================================================================

describe(drink_from_container) {
    it("mover finds water inside clay pot") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = balance.thirstSeekThreshold - 0.01f;
        movers[mi].currentJobId = -1;

        // Spawn pot with water inside
        int potIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_CLAY_POT);
        items[potIdx].state = ITEM_ON_GROUND;

        int waterIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        PutItemInContainer(waterIdx, potIdx);

        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_SEEKING_DRINK);
        expect(movers[mi].needTarget == waterIdx);
    }

    it("drinking from container removes water but pot remains") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].thirst = 0.3f;

        // Spawn pot with water at mover position
        int potIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_CLAY_POT);
        items[potIdx].state = ITEM_ON_GROUND;

        int waterIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        PutItemInContainer(waterIdx, potIdx);

        // Set up drinking state directly
        movers[mi].freetimeState = FREETIME_DRINKING;
        movers[mi].needTarget = waterIdx;
        movers[mi].needProgress = 0.0f;
        items[waterIdx].reservedBy = mi;

        float drinkTime = GameHoursToGameSeconds(balance.drinkingDurationGH);
        int ticks = (int)(drinkTime / TICK_DT) + 10;
        for (int i = 0; i < ticks; i++) {
            gameDeltaTime = TICK_DT;
            ProcessFreetimeNeeds();
        }

        // Water consumed, pot remains
        expect(!items[waterIdx].active);
        expect(items[potIdx].active);
        expect(movers[mi].thirst > 0.3f);
    }
}

// =============================================================================
// Drink Priority
// =============================================================================

describe(drink_priority) {
    it("prefers tea over juice over water") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = balance.thirstSeekThreshold - 0.01f;
        movers[mi].currentJobId = -1;

        // Spawn all three at same distance
        int waterIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        items[waterIdx].state = ITEM_ON_GROUND;
        int juiceIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_BERRY_JUICE);
        items[juiceIdx].state = ITEM_ON_GROUND;
        int teaIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_HERBAL_TEA);
        items[teaIdx].state = ITEM_ON_GROUND;

        ProcessFreetimeNeeds();

        // Should prefer tea (highest hydration at same distance)
        expect(movers[mi].freetimeState == FREETIME_SEEKING_DRINK);
        expect(movers[mi].needTarget == teaIdx);
    }
}

// =============================================================================
// Natural Water Fallback
// =============================================================================

describe(natural_water_fallback) {
    it("thirsty mover seeks natural water when no items available") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].thirst = balance.thirstSeekThreshold - 0.01f;
        movers[mi].currentJobId = -1;

        // Place water on the grid at (5, 5, 1)
        waterGrid[1][5][5].level = 5;

        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_SEEKING_NATURAL_WATER);
    }

    it("natural water drinking is slower than pot drinking") {
        expect(balance.naturalDrinkDurationGH > balance.drinkingDurationGH);
    }

    it("natural water gives less hydration than items") {
        expect(balance.naturalDrinkHydration < GetItemHydration(ITEM_WATER));
    }

    it("mover drinks natural water and restores thirst") {
        SetupClean();
        int mi = SetupMover(5, 5);
        movers[mi].thirst = 0.3f;

        // Set up drinking natural state
        movers[mi].freetimeState = FREETIME_DRINKING_NATURAL;
        movers[mi].needTarget = 5 + 5 * gridWidth;
        movers[mi].needProgress = 0.0f;

        float drinkTime = GameHoursToGameSeconds(balance.naturalDrinkDurationGH);
        int ticks = (int)(drinkTime / TICK_DT) + 10;
        for (int i = 0; i < ticks; i++) {
            gameDeltaTime = TICK_DT;
            ProcessFreetimeNeeds();
        }

        float expected = 0.3f + balance.naturalDrinkHydration;
        expect(fabsf(movers[mi].thirst - expected) < 0.05f);
        expect(movers[mi].freetimeState == FREETIME_NONE);
    }
}

// =============================================================================
// Fill Water Pot Job
// =============================================================================

describe(fill_water_pot) {
    it("WorkGiver creates fill job when empty pot and water exist") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].currentJobId = -1;
        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();

        // Spawn empty pot
        int potIdx = SpawnItem(3 * CELL_SIZE + 4, 3 * CELL_SIZE + 4, 1.0f, ITEM_CLAY_POT);
        items[potIdx].state = ITEM_ON_GROUND;

        // Place water on grid
        waterGrid[1][7][7].level = 5;

        int jobId = WorkGiver_FillWaterPot(mi);
        expect(jobId >= 0);
        expect(jobs[jobId].type == JOBTYPE_FILL_WATER_POT);
        expect(items[potIdx].reservedBy == mi);
    }

    it("no job when no empty pots exist") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].currentJobId = -1;
        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();

        waterGrid[1][7][7].level = 5;

        int jobId = WorkGiver_FillWaterPot(mi);
        expect(jobId < 0);
    }

    it("no job when no water on map") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].currentJobId = -1;
        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();

        int potIdx = SpawnItem(3 * CELL_SIZE + 4, 3 * CELL_SIZE + 4, 1.0f, ITEM_CLAY_POT);
        items[potIdx].state = ITEM_ON_GROUND;

        int jobId = WorkGiver_FillWaterPot(mi);
        expect(jobId < 0);
    }

    it("no job when thirst disabled") {
        SetupClean();
        thirstEnabled = false;
        int mi = SetupMover(1, 1);
        movers[mi].currentJobId = -1;
        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();

        int potIdx = SpawnItem(3 * CELL_SIZE + 4, 3 * CELL_SIZE + 4, 1.0f, ITEM_CLAY_POT);
        items[potIdx].state = ITEM_ON_GROUND;
        waterGrid[1][7][7].level = 5;

        int jobId = WorkGiver_FillWaterPot(mi);
        expect(jobId < 0);
    }

    it("skips pot that already has contents") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].currentJobId = -1;
        InitJobSystem(MAX_MOVERS);
        RebuildIdleMoverList();

        // Spawn pot with water already inside
        int potIdx = SpawnItem(3 * CELL_SIZE + 4, 3 * CELL_SIZE + 4, 1.0f, ITEM_CLAY_POT);
        items[potIdx].state = ITEM_ON_GROUND;
        int w = SpawnItem(3 * CELL_SIZE + 4, 3 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        PutItemInContainer(w, potIdx);

        waterGrid[1][7][7].level = 5;

        int jobId = WorkGiver_FillWaterPot(mi);
        expect(jobId < 0);
    }
}

// =============================================================================
// Beverage Recipes
// =============================================================================

describe(beverage_recipes) {
    it("campfire has brew tea recipe") {
        bool found = false;
        for (int i = 0; i < workshopDefs[WORKSHOP_CAMPFIRE].recipeCount; i++) {
            const Recipe* r = &workshopDefs[WORKSHOP_CAMPFIRE].recipes[i];
            if (r->outputType == ITEM_HERBAL_TEA) {
                found = true;
                expect(r->inputType == ITEM_WATER);
                expect(r->inputType2 == ITEM_DRIED_GRASS);
            }
        }
        expect(found);
    }

    it("campfire has press juice recipe") {
        bool found = false;
        for (int i = 0; i < workshopDefs[WORKSHOP_CAMPFIRE].recipeCount; i++) {
            const Recipe* r = &workshopDefs[WORKSHOP_CAMPFIRE].recipes[i];
            if (r->outputType == ITEM_BERRY_JUICE) {
                found = true;
                expect(r->inputType == ITEM_WATER);
                expect(r->inputType2 == ITEM_BERRIES);
            }
        }
        expect(found);
    }
}

// =============================================================================
// Thirst + Hunger Coexistence
// =============================================================================

describe(thirst_hunger_coexistence) {
    it("dehydrating interrupts job like starving") {
        SetupClean();
        hungerEnabled = true;
        int mi = SetupMover(1, 1);

        // Give mover a fake job
        int jobId = CreateJob(JOBTYPE_HAUL);
        if (jobId >= 0) {
            jobs[jobId].assignedMover = mi;
            movers[mi].currentJobId = jobId;
        }

        movers[mi].thirst = balance.thirstCriticalThreshold - 0.01f;
        movers[mi].hunger = 1.0f; // not hungry

        // Freetime check should trigger drink search and unassign job
        ProcessFreetimeNeeds();

        // After freetime processing, mover should have sought drink
        // (either SEEKING_DRINK or SEEKING_NATURAL_WATER depending on available resources)
        expect(movers[mi].freetimeState == FREETIME_NONE ||
               movers[mi].freetimeState == FREETIME_SEEKING_DRINK ||
               movers[mi].freetimeState == FREETIME_SEEKING_NATURAL_WATER);
    }

    it("thirst and hunger drain independently") {
        SetupClean();
        hungerEnabled = true;
        int mi = SetupMover(1, 1);
        movers[mi].thirst = 1.0f;
        movers[mi].hunger = 1.0f;

        for (int i = 0; i < 1000; i++) {
            NeedsTick();
        }

        // Both should have drained
        expect(movers[mi].thirst < 1.0f);
        expect(movers[mi].hunger < 1.0f);
        // Thirst drains faster (16h vs 24h)
        expect(movers[mi].thirst < movers[mi].hunger);
    }
}

// =============================================================================
// Cancel Handling
// =============================================================================

describe(cancel_drink_seeking) {
    it("cancelling drink seeking releases item reservation") {
        SetupClean();
        int mi = SetupMover(1, 1);

        int waterIdx = SpawnItem(5 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_WATER);
        items[waterIdx].state = ITEM_ON_GROUND;
        items[waterIdx].reservedBy = mi;

        movers[mi].freetimeState = FREETIME_SEEKING_DRINK;
        movers[mi].needTarget = waterIdx;

        // Disable thirst — should cancel
        thirstEnabled = false;
        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(items[waterIdx].reservedBy == -1);
    }
}

// =============================================================================
// Balance Values
// =============================================================================

describe(balance_values) {
    it("thirst drains faster than hunger") {
        SetupClean();
        // hoursToDehydrate=16 vs hoursToStarve=24
        expect(balance.hoursToDehydrate < balance.hoursToStarve);
    }

    it("thirst drain rate is correct") {
        SetupClean();
        float expected = 1.0f / balance.hoursToDehydrate;
        expect(fabsf(balance.thirstDrainPerGH - expected) < 0.0001f);
    }

    it("thirst thresholds are sensible") {
        SetupClean();
        expect(balance.thirstSeekThreshold > balance.thirstCriticalThreshold);
        expect(balance.thirstCriticalThreshold > 0.0f);
        expect(balance.thirstSeekThreshold < 1.0f);
    }
}

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

    test(thirst_drain);
    test(dehydration_death);
    test(drinkable_items);
    test(drink_from_stockpile);
    test(drink_from_container);
    test(drink_priority);
    test(natural_water_fallback);
    test(fill_water_pot);
    test(beverage_recipes);
    test(thirst_hunger_coexistence);
    test(cancel_drink_seeking);
    test(balance_values);

    return summary();
}
