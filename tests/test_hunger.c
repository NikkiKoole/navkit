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
#include "../src/simulation/plants.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/weather.h"
#include "../src/core/time.h"
#include "../src/world/designations.h"
#include "test_helpers.h"
#include "../src/core/state_audit.h"
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up a flat walkable 10x10 grid at z=1 (air above solid ground)
static void SetupFlatGrid(void) {
    InitTestGrid(10, 10);
    // z=0 is solid ground, z=1 is walkable air
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 10; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
            SET_FLOOR(x, y, 1);
        }
    }
}

// Helper: set up common test state
static void SetupClean(void) {
    SetupFlatGrid();
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearJobs();
    ClearPlants();
    InitDesignations();
    InitBalance();
    hungerEnabled = true;
    energyEnabled = true;
    bodyTempEnabled = true;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    dayLength = 60.0f;
    daysPerSeason = 7;
    dayNumber = 8; // Summer (day 7 in year = summer day 0)
}

// Helper: spawn a mover at cell (cx, cy) on z=1
static int SetupMover(int cx, int cy) {
    int idx = moverCount;
    moverCount = idx + 1;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f,
              cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    return idx;
}

// =============================================================================
// Hunger Drain
// =============================================================================

describe(hunger_drain) {
    it("mover hunger starts at 1.0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        expect(movers[mi].hunger == 1.0f);
    }

    it("hunger drains over time") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 1.0f;

        // Run NeedsTick many times
        for (int i = 0; i < 1000; i++) {
            NeedsTick();
        }

        expect(movers[mi].hunger < 1.0f);
    }

    it("hunger clamps at 0.0") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.001f;

        // Drain a lot
        for (int i = 0; i < 10000; i++) {
            NeedsTick();
        }

        expect(movers[mi].hunger == 0.0f);
    }

    it("inactive mover hunger does not drain") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 1.0f;
        movers[mi].active = false;

        for (int i = 0; i < 10000; i++) {
            NeedsTick();
        }

        expect(movers[mi].hunger == 1.0f);
    }
}

// =============================================================================
// Hunger Speed Penalty
// =============================================================================

describe(hunger_speed_penalty) {
    it("full mover has no speed penalty") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 1.0f;

        // Speed penalty is applied in UpdateMovers, but we can check the math directly
        float hungerMult = 1.0f;
        if (movers[mi].hunger < balance.hungerPenaltyThreshold) {
            float t = movers[mi].hunger / balance.hungerPenaltyThreshold;
            hungerMult = balance.hungerSpeedPenaltyMin + t * (1.0f - balance.hungerSpeedPenaltyMin);
        }
        expect(hungerMult == 1.0f);
    }

    it("mover at threshold has no penalty") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = balance.hungerPenaltyThreshold;

        float hungerMult = 1.0f;
        if (movers[mi].hunger < balance.hungerPenaltyThreshold) {
            float t = movers[mi].hunger / balance.hungerPenaltyThreshold;
            hungerMult = balance.hungerSpeedPenaltyMin + t * (1.0f - balance.hungerSpeedPenaltyMin);
        }
        expect(hungerMult == 1.0f);
    }

    it("starving mover gets 50% speed") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.0f;

        float hungerMult = 1.0f;
        if (movers[mi].hunger < balance.hungerPenaltyThreshold) {
            float t = movers[mi].hunger / balance.hungerPenaltyThreshold;
            hungerMult = balance.hungerSpeedPenaltyMin + t * (1.0f - balance.hungerSpeedPenaltyMin);
        }
        expect(fabsf(hungerMult - balance.hungerSpeedPenaltyMin) < 0.001f);
    }

    it("half-starved mover gets intermediate penalty") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = balance.hungerPenaltyThreshold * 0.5f;

        float hungerMult = 1.0f;
        if (movers[mi].hunger < balance.hungerPenaltyThreshold) {
            float t = movers[mi].hunger / balance.hungerPenaltyThreshold;
            hungerMult = balance.hungerSpeedPenaltyMin + t * (1.0f - balance.hungerSpeedPenaltyMin);
        }
        // Should be 0.75 (halfway between 0.5 and 1.0)
        expect(fabsf(hungerMult - 0.75f) < 0.001f);
    }
}

// =============================================================================
// Food Items
// =============================================================================

describe(food_items) {
    it("ITEM_BERRIES exists and is edible") {
        expect(ITEM_BERRIES < ITEM_TYPE_COUNT);
        expect(ItemIsEdible(ITEM_BERRIES) != 0);
    }

    it("ITEM_DRIED_BERRIES exists and is edible") {
        expect(ITEM_DRIED_BERRIES < ITEM_TYPE_COUNT);
        expect(ItemIsEdible(ITEM_DRIED_BERRIES) != 0);
    }

    it("berries have correct nutrition") {
        expect(fabsf(ItemNutrition(ITEM_BERRIES) - 0.3f) < 0.001f);
    }

    it("dried berries have correct nutrition") {
        expect(fabsf(ItemNutrition(ITEM_DRIED_BERRIES) - 0.25f) < 0.001f);
    }

    it("non-food items have zero nutrition") {
        expect(ItemNutrition(ITEM_LOG) == 0.0f);
        expect(ItemNutrition(ITEM_PLANKS) == 0.0f);
        expect(ItemNutrition(ITEM_GRASS) == 0.0f);
    }

    it("berries are stackable") {
        expect(itemDefs[ITEM_BERRIES].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_DRIED_BERRIES].flags & IF_STACKABLE);
    }
}

// =============================================================================
// Plant Entity System
// =============================================================================

describe(plant_spawn) {
    it("SpawnPlant creates active plant at position") {
        SetupClean();
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        expect(idx >= 0);
        expect(plants[idx].active == true);
        expect(plants[idx].x == 5);
        expect(plants[idx].y == 5);
        expect(plants[idx].z == 1);
        expect(plants[idx].type == PLANT_BERRY_BUSH);
        expect(plants[idx].stage == PLANT_STAGE_BARE);
    }

    it("GetPlantAt finds plant by position") {
        SetupClean();
        SpawnPlant(3, 4, 1, PLANT_BERRY_BUSH);
        Plant* p = GetPlantAt(3, 4, 1);
        expect(p != NULL);
        expect(p->x == 3);
        expect(p->y == 4);
    }

    it("GetPlantAt returns NULL for empty cell") {
        SetupClean();
        Plant* p = GetPlantAt(3, 4, 1);
        expect(p == NULL);
    }

    it("DeletePlant deactivates plant") {
        SetupClean();
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        expect(plants[idx].active == true);
        DeletePlant(idx);
        expect(plants[idx].active == false);
        expect(GetPlantAt(5, 5, 1) == NULL);
    }
}

describe(plant_growth) {
    it("berry bush grows BARE to BUDDING in summer") {
        SetupClean();
        dayNumber = 8; // Summer
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        expect(plants[idx].stage == PLANT_STAGE_BARE);

        // Tick enough to advance one stage: BERRY_BUSH_GROWTH_TIME=120s at rate 1.0
        // Each tick advances by TICK_DT * 1.0 / 120.0
        // Need 120 / TICK_DT = 120 * 60 = 7200 ticks for one stage
        for (int i = 0; i < 8000; i++) {
            PlantsTick(TICK_DT);
        }

        expect(plants[idx].stage == PLANT_STAGE_BUDDING);
    }

    it("berry bush grows BUDDING to RIPE") {
        SetupClean();
        dayNumber = 8; // Summer
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        plants[idx].stage = PLANT_STAGE_BUDDING;
        plants[idx].growthProgress = 0.0f;

        for (int i = 0; i < 8000; i++) {
            PlantsTick(TICK_DT);
        }

        expect(plants[idx].stage == PLANT_STAGE_RIPE);
    }

    it("ripe bush does not advance further") {
        SetupClean();
        dayNumber = 8; // Summer
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        plants[idx].stage = PLANT_STAGE_RIPE;

        for (int i = 0; i < 1000; i++) {
            PlantsTick(TICK_DT);
        }

        expect(plants[idx].stage == PLANT_STAGE_RIPE);
    }

    it("berry bush does NOT grow in winter") {
        SetupClean();
        dayNumber = 22; // Winter (day 21 in year)
        expect(GetCurrentSeason() == SEASON_WINTER);

        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        float startProgress = plants[idx].growthProgress;

        for (int i = 0; i < 10000; i++) {
            PlantsTick(TICK_DT);
        }

        expect(plants[idx].growthProgress == startProgress);
        expect(plants[idx].stage == PLANT_STAGE_BARE);
    }

    it("spring growth is slower than summer") {
        SetupClean();

        // Summer growth
        dayNumber = 8; // Summer
        int s1 = SpawnPlant(1, 1, 1, PLANT_BERRY_BUSH);
        for (int i = 0; i < 1000; i++) PlantsTick(TICK_DT);
        float summerProgress = plants[s1].growthProgress;

        // Reset and test spring
        ClearPlants();
        dayNumber = 1; // Spring
        expect(GetCurrentSeason() == SEASON_SPRING);
        int s2 = SpawnPlant(2, 2, 1, PLANT_BERRY_BUSH);
        for (int i = 0; i < 1000; i++) PlantsTick(TICK_DT);
        float springProgress = plants[s2].growthProgress;

        expect(springProgress < summerProgress);
    }
}

describe(plant_harvest) {
    it("HarvestPlant on ripe bush resets to BARE") {
        SetupClean();
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        plants[idx].stage = PLANT_STAGE_RIPE;

        HarvestPlant(5, 5, 1);

        expect(plants[idx].stage == PLANT_STAGE_BARE);
        expect(plants[idx].growthProgress == 0.0f);
    }

    it("HarvestPlant spawns ITEM_BERRIES on ground") {
        SetupClean();
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        plants[idx].stage = PLANT_STAGE_RIPE;

        HarvestPlant(5, 5, 1);

        // Find the spawned berry item
        bool foundBerries = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_BERRIES) {
                foundBerries = true;
                break;
            }
        }
        expect(foundBerries);
    }

    it("HarvestPlant on non-ripe bush does nothing") {
        SetupClean();
        int idx = SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        plants[idx].stage = PLANT_STAGE_BUDDING;

        HarvestPlant(5, 5, 1);

        expect(plants[idx].stage == PLANT_STAGE_BUDDING);

        // No berries spawned
        bool foundBerries = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_BERRIES) {
                foundBerries = true;
                break;
            }
        }
        expect(!foundBerries);
    }

    it("IsPlantRipe returns true only for ripe plants") {
        SetupClean();
        SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);

        Plant* p = GetPlantAt(5, 5, 1);
        p->stage = PLANT_STAGE_BARE;
        expect(!IsPlantRipe(5, 5, 1));

        p->stage = PLANT_STAGE_BUDDING;
        expect(!IsPlantRipe(5, 5, 1));

        p->stage = PLANT_STAGE_RIPE;
        expect(IsPlantRipe(5, 5, 1));
    }
}

// =============================================================================
// Harvest Designation
// =============================================================================

describe(harvest_designation) {
    it("designating ripe bush succeeds") {
        SetupClean();
        SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        Plant* p = GetPlantAt(5, 5, 1);
        p->stage = PLANT_STAGE_RIPE;

        bool ok = DesignateHarvestBerry(5, 5, 1);
        expect(ok);
        expect(HasHarvestBerryDesignation(5, 5, 1));
    }

    it("designating non-ripe bush fails") {
        SetupClean();
        SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);

        bool ok = DesignateHarvestBerry(5, 5, 1);
        expect(!ok);
        expect(!HasHarvestBerryDesignation(5, 5, 1));
    }

    it("designating same cell twice fails") {
        SetupClean();
        SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        Plant* p = GetPlantAt(5, 5, 1);
        p->stage = PLANT_STAGE_RIPE;

        bool ok1 = DesignateHarvestBerry(5, 5, 1);
        bool ok2 = DesignateHarvestBerry(5, 5, 1);
        expect(ok1);
        expect(!ok2);
    }

    it("completing harvest designation clears it and harvests plant") {
        SetupClean();
        SpawnPlant(5, 5, 1, PLANT_BERRY_BUSH);
        Plant* p = GetPlantAt(5, 5, 1);
        p->stage = PLANT_STAGE_RIPE;

        DesignateHarvestBerry(5, 5, 1);
        expect(HasHarvestBerryDesignation(5, 5, 1));

        CompleteHarvestBerryDesignation(5, 5, 1);

        expect(!HasHarvestBerryDesignation(5, 5, 1));
        expect(p->stage == PLANT_STAGE_BARE);

        // Berries should have spawned
        bool foundBerries = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_BERRIES) {
                foundBerries = true;
                break;
            }
        }
        expect(foundBerries);
    }
}

// =============================================================================
// Freetime / Idle List
// =============================================================================

describe(freetime_idle_list) {
    it("mover with FREETIME_NONE is in idle list") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].freetimeState = FREETIME_NONE;
        movers[mi].currentJobId = -1;

        RebuildIdleMoverList();

        // Check that mover is in idle list
        bool found = false;
        for (int i = 0; i < idleMoverCount; i++) {
            if (idleMoverList[i] == mi) { found = true; break; }
        }
        expect(found);
    }

    it("mover with FREETIME_EATING is NOT in idle list") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].freetimeState = FREETIME_EATING;
        movers[mi].currentJobId = -1;

        RebuildIdleMoverList();

        bool found = false;
        for (int i = 0; i < idleMoverCount; i++) {
            if (idleMoverList[i] == mi) { found = true; break; }
        }
        expect(!found);
    }

    it("mover with FREETIME_SEEKING_FOOD is NOT in idle list") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].freetimeState = FREETIME_SEEKING_FOOD;
        movers[mi].currentJobId = -1;

        RebuildIdleMoverList();

        bool found = false;
        for (int i = 0; i < idleMoverCount; i++) {
            if (idleMoverList[i] == mi) { found = true; break; }
        }
        expect(!found);
    }
}

// =============================================================================
// Eating Behavior
// =============================================================================

describe(eating_food_search) {
    it("hungry mover with no food gets cooldown") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.2f;
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 0.0f;

        ProcessFreetimeNeeds();

        // No food available — should get cooldown, stay in FREETIME_NONE
        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].needSearchCooldown > 0.0f);
    }

    it("mover above hunger threshold does not search") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.5f; // Above 0.3 threshold
        movers[mi].currentJobId = -1;

        // Put berries in stockpile
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].needTarget == -1);
    }

    it("hungry mover finds food in stockpile and enters SEEKING") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.2f;
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 0.0f;

        // Place berries in stockpile on same z-level
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_SEEKING_FOOD);
        expect(movers[mi].needTarget == itemIdx);
        expect(items[itemIdx].reservedBy == mi);
    }

    it("cooldown prevents repeated searches") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.2f;
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 3.0f; // Active cooldown

        ProcessFreetimeNeeds();

        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].needTarget == -1);
    }
}

describe(eating_food_competition) {
    it("two hungry movers one berry - first reserves second gets cooldown") {
        SetupClean();
        int m1 = SetupMover(1, 1);
        int m2 = SetupMover(2, 2);
        movers[m1].hunger = 0.2f;
        movers[m2].hunger = 0.2f;
        movers[m1].currentJobId = -1;
        movers[m2].currentJobId = -1;
        movers[m1].needSearchCooldown = 0.0f;
        movers[m2].needSearchCooldown = 0.0f;

        // Only one berry
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        ProcessFreetimeNeeds();

        // One should have the item, other should have cooldown
        bool m1HasFood = (movers[m1].freetimeState == FREETIME_SEEKING_FOOD);
        bool m2HasFood = (movers[m2].freetimeState == FREETIME_SEEKING_FOOD);

        // Exactly one should have food
        expect(m1HasFood != m2HasFood);

        if (m1HasFood) {
            expect(items[itemIdx].reservedBy == m1);
            expect(movers[m2].needSearchCooldown > 0.0f);
        } else {
            expect(items[itemIdx].reservedBy == m2);
            expect(movers[m1].needSearchCooldown > 0.0f);
        }
    }
}

describe(eating_consumption) {
    it("mover at food location enters EATING and consumes it") {
        SetupClean();
        int mi = SetupMover(5, 5);

        // Place berries right at mover position in stockpile
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        movers[mi].hunger = 0.2f;
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 0.0f;

        // First call: should find food and enter SEEKING
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_SEEKING_FOOD);

        // Mover is already at food location, next call should transition to EATING
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_EATING);

        // Tick through eating duration (2s = 120 ticks at TICK_DT)
        for (int i = 0; i < 130; i++) {
            ProcessFreetimeNeeds();
        }

        // Should have consumed food and returned to FREETIME_NONE
        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(movers[mi].hunger > 0.2f); // Restored by nutrition (0.3)
        expect(movers[mi].needTarget == -1);
    }

    it("a tired mover who finishes eating goes to rest before any job is handed out") {
        SetupClean();
        int mi = SetupMover(5, 5);
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        movers[mi].hunger = 0.2f;
        movers[mi].energy = balance.energyTiredThreshold - 0.05f;   // tired, not exhausted
        ProcessFreetimeNeeds();
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_EATING);
        for (int i = 0; i < 1000 && movers[mi].freetimeState == FREETIME_EATING; i++) ProcessFreetimeNeeds();
        // Same tick as the meal ends, before AssignJobs: already on the way to rest.
        // (Bug: it was FREETIME_NONE here, AssignJobs gave it work and it worked on tired.)
        expect(movers[mi].freetimeState == FREETIME_SEEKING_REST || movers[mi].freetimeState == FREETIME_RESTING);
    }

    it("eating restores correct nutrition amount") {
        SetupClean();
        int mi = SetupMover(5, 5);

        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        movers[mi].hunger = 0.1f;
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 0.0f;

        // Process: NONE → SEEKING → EATING → done
        ProcessFreetimeNeeds(); // → SEEKING
        ProcessFreetimeNeeds(); // → EATING (at food)

        float hungerBefore = movers[mi].hunger;

        // Complete eating
        for (int i = 0; i < 130; i++) {
            ProcessFreetimeNeeds();
        }

        float expected = hungerBefore + ItemNutrition(ITEM_BERRIES);
        expect(fabsf(movers[mi].hunger - expected) < 0.01f);
    }

    it("hunger clamps at 1.0 after eating") {
        SetupClean();
        int mi = SetupMover(5, 5);

        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        movers[mi].hunger = 0.0f; // Force hunger below threshold
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 0.0f;

        // Make mover eat
        ProcessFreetimeNeeds(); // → SEEKING
        ProcessFreetimeNeeds(); // → EATING

        // Set hunger high before eating completes to test clamp
        movers[mi].hunger = 0.9f;

        for (int i = 0; i < 130; i++) {
            ProcessFreetimeNeeds();
        }

        expect(movers[mi].hunger <= 1.0f);
    }

    it("eating one from a stack frees the rest of the stack for others") {
        // Bug: the eater kept its reservation on the remaining stack, so after a few meals
        // every stack was reserved and the colony starved next to a full stockpile.
        SetupClean();
        int mi = SetupMover(5, 5);
        int other = SetupMover(1, 1);

        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BREAD);
        items[itemIdx].stackCount = 3;
        PlaceItemInStockpile(spIdx, 5, 5, itemIdx);
        items[itemIdx].state = ITEM_IN_STOCKPILE;

        movers[mi].hunger = 0.2f;
        movers[mi].currentJobId = -1;
        movers[mi].needSearchCooldown = 0.0f;
        movers[other].hunger = 1.0f;
        currentTick = 1;  // keep the once-a-second stray-reservation scan (mover 0, tick 0) from hiding a leak

        ProcessFreetimeNeeds(); // → SEEKING
        expect(movers[mi].freetimeState == FREETIME_SEEKING_FOOD);
        expect(AuditItemReservations(false) == 0);  // reserved for eating: not a leak
        ProcessFreetimeNeeds(); // → EATING
        expect(movers[mi].freetimeState == FREETIME_EATING);
        for (int i = 0; i < 130; i++) ProcessFreetimeNeeds();
        expect(AuditItemReservations(false) == 0);

        expect(movers[mi].freetimeState == FREETIME_NONE);
        expect(items[itemIdx].active);
        expect(items[itemIdx].stackCount == 2);
        expect(items[itemIdx].reservedBy == -1);

        // The other mover gets hungry and finds the same stack
        movers[other].hunger = 0.2f;
        movers[other].currentJobId = -1;
        movers[other].needSearchCooldown = 0.0f;
        movers[mi].hunger = 1.0f;
        ProcessFreetimeNeeds();
        expect(movers[other].freetimeState == FREETIME_SEEKING_FOOD);
        expect(movers[other].needTarget == itemIdx);
    }
}

// A fake haul job on job slot 0 for mover mi (no targets: unassigning it is harmless)
static void GiveFakeHaulJob(int mi) {
    Job* j = &jobs[0];
    memset(j, 0, sizeof(*j));
    j->active = true;
    j->type = JOBTYPE_HAUL;
    j->assignedMover = mi;
    j->step = STEP_MOVING_TO_WORK;
    j->targetItem = j->targetStockpile = j->targetBlueprint = j->targetWorkshop = -1;
    j->targetMineX = j->targetMineY = j->targetMineZ = -1;
    j->carryingItem = j->targetItem2 = j->targetItem3 = j->fuelItem = j->toolItem = -1;
    j->targetAnimalIdx = -1;
    movers[mi].currentJobId = 0;
    jobHighWaterMark = 1;
    activeJobCount = 1;
}

// Bug: a critical need dropped the job every tick even while its search was on cooldown;
// AssignJobs gave the job back, so the mover stood still taking and losing a job each tick.
describe(critical_need_search_cooldown) {
    it("a starving mover with no food anywhere still goes to drink when dehydrating") {
        SetupClean();
        bool wasThirst = thirstEnabled;
        thirstEnabled = true;
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.0f;
        movers[mi].thirst = balance.thirstCriticalThreshold - 0.01f;
        SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_WATER);
        ProcessFreetimeNeeds();
        thirstEnabled = wasThirst;
        // Bug: the failed food search ended the chain and its cooldown blocked every later
        // drink search, so a colony out of food also died of thirst.
        expect(movers[mi].freetimeState == FREETIME_SEEKING_DRINK);
    }

    it("a starving mover with no food anywhere rests when tired") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.0f;
        movers[mi].energy = balance.energyTiredThreshold - 0.01f;   // tired, not exhausted
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_RESTING || movers[mi].freetimeState == FREETIME_SEEKING_REST);
    }

    it("a starving mover with no food anywhere still sleeps when exhausted") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.0f;
        movers[mi].energy = balance.energyExhaustedThreshold - 0.01f;
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_RESTING || movers[mi].freetimeState == FREETIME_SEEKING_REST);
        // and later, with the food search cooling down, still rests
        movers[mi].freetimeState = FREETIME_NONE;
        movers[mi].needSearchCooldown = 10.0f;
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_RESTING);
    }

    it("starving mover whose food search is on cooldown keeps its job") {
        SetupClean();
        ClearJobs();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.05f;
        movers[mi].needSearchCooldown = 5.0f;   // last search found nothing
        GiveFakeHaulJob(mi);

        ProcessFreetimeNeeds();
        expect(movers[mi].currentJobId == 0);
        expect(movers[mi].freetimeState == FREETIME_NONE);
    }

    it("starving mover with no food drops the job once, then works until the next search") {
        SetupClean();
        ClearJobs();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.05f;
        GiveFakeHaulJob(mi);

        ProcessFreetimeNeeds();                       // searches: drops job, finds nothing
        expect(movers[mi].currentJobId == -1);
        expect(movers[mi].needSearchCooldown > 0.0f);

        GiveFakeHaulJob(mi);                          // AssignJobs hands out work again
        ProcessFreetimeNeeds();
        expect(movers[mi].currentJobId == 0);          // kept: no search this tick
    }

    it("exhausted mover whose rest search is on cooldown lies down where it stands") {
        SetupClean();
        ClearJobs();
        energyEnabled = true;
        int mi = SetupMover(3, 3);
        movers[mi].hunger = 1.0f;
        movers[mi].energy = 0.05f;
        movers[mi].needSearchCooldown = 5.0f;   // e.g. the bed was out of reach
        GiveFakeHaulJob(mi);

        ProcessFreetimeNeeds();
        expect(movers[mi].currentJobId == -1);
        expect(movers[mi].freetimeState == FREETIME_RESTING);
        expect(movers[mi].needTarget == -1);      // ground, no furniture
    }
}

// Food far away: the seek timeout is for food that can't be reached, not for distance.
// Bug: foodSeekTimeoutGH 0.5 is ~8 cells of walking, so movers turned back halfway.
static bool WalkToFarFood(float testDayLength, int* seekStarts) {
    InitTestGridFromAscii(
        "................................\n"
        "................................\n"
        "................................\n");
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers(); ClearItems(); ClearStockpiles(); ClearJobs(); ClearPlants();
    InitBalance();
    hungerEnabled = true; energyEnabled = false; bodyTempEnabled = false;
    thirstEnabled = false; bladderEnabled = false;
    dayLength = testDayLength;
    currentTick = 1;

    int spIdx = CreateStockpile(30, 1, 0, 1, 1);
    int food = SpawnItem(30 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BERRIES);
    PlaceItemInStockpile(spIdx, 30, 1, food);

    Point goal = {1, 1, 0};
    InitMover(&movers[0], 1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
    moverCount = 1;
    movers[0].hunger = 0.25f;  // hungry, not starving (above the hunger speed-penalty threshold)

    bool ate = false;
    int prev = FREETIME_NONE;
    *seekStarts = 0;
    for (int t = 0; t < 20000 && !ate; t++) {
        Tick();
        NeedsTick();
        ProcessFreetimeNeeds();
        if (movers[0].freetimeState == FREETIME_SEEKING_FOOD && prev != FREETIME_SEEKING_FOOD) (*seekStarts)++;
        if (movers[0].freetimeState == FREETIME_EATING) ate = true;
        prev = movers[0].freetimeState;
    }
    dayLength = 60.0f;
    return ate;
}

describe(seek_keeps_its_goal) {
    it("a mover walking to a need keeps its goal when no path is found (not sent wandering)") {
        InitTestGridFromAscii(
            "..........\n"
            "....#.....\n"
            "..........\n");
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers(); ClearItems(); ClearJobs();
        InitBalance();
        bool savedEndless = endlessMoverMode;
        endlessMoverMode = true;
        currentTick = 1;
        int food = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BERRIES);
        InitMover(&movers[0], 1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, (Point){1, 1, 0}, MOVER_SPEED);
        moverCount = 1;
        movers[0].freetimeState = FREETIME_SEEKING_FOOD;
        movers[0].needTarget = food;
        movers[0].goal = (Point){4, 1, 0};   // inside the wall: no path
        movers[0].needsRepath = true;
        for (int t = 0; t < 10; t++) Tick();
        endlessMoverMode = savedEndless;
        // Bug: a mover with no job counted as a wanderer and got a random goal, while still
        // "seeking food"; it walked around all day after food it could never reach.
        expect(movers[0].goal.x == 4 && movers[0].goal.y == 1);
    }
}

describe(food_far_away) {
    it("a hungry mover walks 29 cells to food without giving up on the way") {
        int starts = 0;
        expect(WalkToFarFood(60.0f, &starts));
        expect(starts == 1);
    }

    it("the same walk works at dayLength 120") {
        int starts = 0;
        expect(WalkToFarFood(120.0f, &starts));
        expect(starts == 1);
    }
}

describe(eating_starving_unassigns_job) {
    it("starving mover unassigns current job") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.05f; // Below hungerCriticalThreshold (0.1)

        // Give mover a fake haul job
        ClearJobs();
        Job* j = &jobs[0];
        j->active = true;
        j->type = JOBTYPE_HAUL;
        j->assignedMover = mi;
        j->step = STEP_MOVING_TO_WORK;
        j->targetItem = -1;
        j->targetStockpile = -1;
        j->targetBlueprint = -1;
        j->targetWorkshop = -1;
        j->targetMineX = -1;
        j->targetMineY = -1;
        j->targetMineZ = -1;
        j->carryingItem = -1;
        j->targetItem2 = -1;
        j->fuelItem = -1;
        movers[mi].currentJobId = 0;
        jobHighWaterMark = 1;
        activeJobCount = 1;

        ProcessFreetimeNeeds();

        // Mover should be detached from job
        expect(movers[mi].currentJobId == -1);
    }

    it("starving mover does NOT unassign food-producing job") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.05f;

        // Give mover a harvest berry job
        ClearJobs();
        Job* j = &jobs[0];
        j->active = true;
        j->type = JOBTYPE_HARVEST_BERRY;
        j->assignedMover = mi;
        j->step = STEP_MOVING_TO_WORK;
        j->targetItem = -1;
        j->targetStockpile = -1;
        j->targetBlueprint = -1;
        j->targetWorkshop = -1;
        j->targetMineX = -1;
        j->targetMineY = -1;
        j->targetMineZ = -1;
        j->carryingItem = -1;
        j->targetItem2 = -1;
        j->fuelItem = -1;
        movers[mi].currentJobId = 0;
        jobHighWaterMark = 1;
        activeJobCount = 1;

        ProcessFreetimeNeeds();

        // Job should NOT be unassigned — let mover finish getting food
        expect(movers[mi].currentJobId == 0);
    }

    it("unassign preserves mine designation progress") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.05f;

        // Set up a mine designation at (3,3,1) with 75% progress
        GRID(3, 3, 1) = CELL_WALL;
        SetWallMaterial(3, 3, 1, MAT_GRANITE);
        DesignateMine(3, 3, 1);
        Designation* d = GetDesignation(3, 3, 1);
        d->assignedMover = mi;
        d->progress = 0.75f;

        // Give mover a mining job targeting that designation
        ClearJobs();
        Job* j = &jobs[0];
        j->active = true;
        j->type = JOBTYPE_MINE;
        j->assignedMover = mi;
        j->step = STEP_WORKING;
        j->targetMineX = 3;
        j->targetMineY = 3;
        j->targetMineZ = 1;
        j->targetItem = -1;
        j->targetStockpile = -1;
        j->targetBlueprint = -1;
        j->targetWorkshop = -1;
        j->carryingItem = -1;
        j->targetItem2 = -1;
        j->fuelItem = -1;
        j->progress = 0.75f;
        movers[mi].currentJobId = 0;
        jobHighWaterMark = 1;
        activeJobCount = 1;

        ProcessFreetimeNeeds();

        // Mover should be detached
        expect(movers[mi].currentJobId == -1);

        // Designation should still exist with progress preserved
        Designation* dAfter = GetDesignation(3, 3, 1);
        expect(dAfter != NULL);
        expect(dAfter->assignedMover == -1);  // Unassigned, available for re-pickup
        expect(fabsf(dAfter->progress - 0.75f) < 0.001f);  // Progress preserved!
    }

    it("cancel job resets designation progress to zero") {
        SetupClean();
        int mi = SetupMover(1, 1);

        // Set up a mine designation at (3,3,1) with 75% progress
        GRID(3, 3, 1) = CELL_WALL;
        SetWallMaterial(3, 3, 1, MAT_GRANITE);
        DesignateMine(3, 3, 1);
        Designation* d = GetDesignation(3, 3, 1);
        d->assignedMover = mi;
        d->progress = 0.75f;

        // Give mover a mining job
        ClearJobs();
        Job* j = &jobs[0];
        j->active = true;
        j->type = JOBTYPE_MINE;
        j->assignedMover = mi;
        j->step = STEP_WORKING;
        j->targetMineX = 3;
        j->targetMineY = 3;
        j->targetMineZ = 1;
        j->targetItem = -1;
        j->targetStockpile = -1;
        j->targetBlueprint = -1;
        j->targetWorkshop = -1;
        j->carryingItem = -1;
        j->targetItem2 = -1;
        j->fuelItem = -1;
        j->progress = 0.75f;
        movers[mi].currentJobId = 0;
        jobHighWaterMark = 1;
        activeJobCount = 1;

        // Use CancelJob directly (not via needs system)
        CancelJob(&movers[mi], mi);

        // Designation progress should be RESET (CancelJob destroys progress)
        Designation* dAfter = GetDesignation(3, 3, 1);
        expect(dAfter != NULL);
        expect(dAfter->assignedMover == -1);
        expect(dAfter->progress == 0.0f);  // Progress lost!
    }

    it("exhausted mover unassigns job") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 1.0f;  // Not hungry
        movers[mi].energy = 0.05f; // Below energyExhaustedThreshold

        ClearJobs();
        Job* j = &jobs[0];
        j->active = true;
        j->type = JOBTYPE_HAUL;
        j->assignedMover = mi;
        j->step = STEP_MOVING_TO_WORK;
        j->targetItem = -1;
        j->targetStockpile = -1;
        j->targetBlueprint = -1;
        j->targetWorkshop = -1;
        j->targetMineX = -1;
        j->targetMineY = -1;
        j->targetMineZ = -1;
        j->carryingItem = -1;
        j->targetItem2 = -1;
        j->fuelItem = -1;
        movers[mi].currentJobId = 0;
        jobHighWaterMark = 1;
        activeJobCount = 1;

        ProcessFreetimeNeeds();

        expect(movers[mi].currentJobId == -1);
    }
}

describe(unassign_job_pickup) {
    it("unassigned mine designation is available for another mover") {
        SetupClean();

        int m1 = SetupMover(1, 1);
        int m2 = SetupMover(2, 1);

        // Place a wall to mine at (4,1,1)
        GRID(4, 1, 1) = CELL_WALL;
        SetWallMaterial(4, 1, 1, MAT_GRANITE);

        // Designate it for mining, assign to mover 1 with 75% progress
        DesignateMine(4, 1, 1);
        Designation* d = GetDesignation(4, 1, 1);
        d->assignedMover = m1;
        d->progress = 0.75f;

        // Give mover 1 a mine job manually
        ClearJobs();
        Job* j = &jobs[0];
        j->active = true;
        j->type = JOBTYPE_MINE;
        j->assignedMover = m1;
        j->step = STEP_WORKING;
        j->targetMineX = 4;
        j->targetMineY = 1;
        j->targetMineZ = 1;
        j->targetItem = -1;
        j->targetStockpile = -1;
        j->targetBlueprint = -1;
        j->targetWorkshop = -1;
        j->carryingItem = -1;
        j->targetItem2 = -1;
        j->fuelItem = -1;
        j->progress = 0.75f;
        movers[m1].currentJobId = 0;
        jobHighWaterMark = 1;
        activeJobCount = 1;

        // Unassign the job (mover 1 got hungry)
        UnassignJob(&movers[m1], m1);

        // Mover 1 is now idle
        expect(movers[m1].currentJobId == -1);

        // Designation still exists, progress preserved, available for pickup
        d = GetDesignation(4, 1, 1);
        expect(d != NULL);
        expect(d->assignedMover == -1);
        expect(fabsf(d->progress - 0.75f) < 0.001f);

        // Mover 2 (or anyone) can now claim it since assignedMover == -1
        // Simulate: mover 2 claims the designation
        d->assignedMover = m2;
        expect(d->assignedMover == m2);
        // Progress is still 75% — mover 2 continues where mover 1 left off
        expect(fabsf(d->progress - 0.75f) < 0.001f);
    }
}

// =============================================================================
// Drying Rack Recipe
// =============================================================================

describe(drying_rack_berries) {
    it("drying rack has a berry drying recipe") {
        // Check that drying rack recipes include berries
        bool found = false;
        for (int i = 0; i < dryingRackRecipeCount; i++) {
            if (dryingRackRecipes[i].inputType == ITEM_BERRIES &&
                dryingRackRecipes[i].outputType == ITEM_DRIED_BERRIES) {
                found = true;
                expect(dryingRackRecipes[i].inputCount == 3);
                expect(dryingRackRecipes[i].outputCount == 2);
            }
        }
        expect(found);
    }

    it("drying rack converts berries to dried berries") {
        SetupClean();

        int wsIdx = CreateWorkshop(2, 1, 1, WORKSHOP_DRYING_RACK);
        // Add bill for recipe index 1 (Dry Berries)
        AddBill(wsIdx, 1, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place 3 berries on work tile
        for (int i = 0; i < 3; i++) {
            SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                      ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                      1.0f, ITEM_BERRIES);
        }

        // Run passive workshop ticks (10s recipe = 600 ticks)
        for (int i = 0; i < 700; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        // Check output: should be 2 units of ITEM_DRIED_BERRIES (1 item with stackCount=2)
        int driedCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_DRIED_BERRIES) {
                driedCount += items[i].stackCount;
            }
        }
        expect(driedCount == 2);
    }
}

// =============================================================================
// Starvation (no death)
// =============================================================================

describe(starvation_survival) {
    it("mover at hunger 0 is still active") {
        SetupClean();
        int mi = SetupMover(1, 1);
        movers[mi].hunger = 0.0f;

        // Run many ticks
        for (int i = 0; i < 1000; i++) {
            NeedsTick();
        }

        expect(movers[mi].active == true);
        expect(movers[mi].hunger == 0.0f);
    }
}

// =============================================================================
// dayLength Independence
// =============================================================================

describe(hunger_daylength_independence) {
    it("hunger drain per game-hour is the same regardless of dayLength") {
        // At different dayLengths, draining for 1 game-hour should drain the same amount
        float drainAmounts[3];
        float dayLengths[] = { 24.0f, 60.0f, 720.0f };

        for (int d = 0; d < 3; d++) {
            SetupClean();
            dayLength = dayLengths[d];
            int mi = SetupMover(1, 1);
            movers[mi].hunger = 1.0f;

            // Simulate 1 game-hour worth of game-seconds
            float oneGameHourGS = GameHoursToGameSeconds(1.0f);
            int ticks = (int)(oneGameHourGS / TICK_DT);
            gameDeltaTime = TICK_DT;

            for (int i = 0; i < ticks; i++) {
                NeedsTick();
            }

            drainAmounts[d] = 1.0f - movers[mi].hunger;
            if (test_verbose) {
                printf("  dayLength=%.0f: drain=%.6f over %d ticks (%.2f game-sec)\n",
                       dayLengths[d], drainAmounts[d], ticks, oneGameHourGS);
            }
        }

        // All should be approximately equal (1/8 = 0.125 per game-hour)
        float expected = balance.hungerDrainPerGH;
        for (int d = 0; d < 3; d++) {
            expect(fabsf(drainAmounts[d] - expected) < 0.01f);
        }
    }

    it("full starvation takes hoursToStarve game-hours at any dayLength") {
        float dayLengths[] = { 24.0f, 60.0f, 720.0f };

        for (int d = 0; d < 3; d++) {
            SetupClean();
            dayLength = dayLengths[d];
            int mi = SetupMover(1, 1);
            movers[mi].hunger = 1.0f;

            // Simulate hoursToStarve game-hours
            float starvationGS = GameHoursToGameSeconds(balance.hoursToStarve);
            int ticks = (int)(starvationGS / TICK_DT);
            gameDeltaTime = TICK_DT;

            for (int i = 0; i < ticks; i++) {
                NeedsTick();
            }

            if (test_verbose) {
                printf("  dayLength=%.0f: hunger=%.6f after %d ticks\n",
                       dayLengths[d], movers[mi].hunger, ticks);
            }
            // Should be very close to 0 (within rounding from discrete ticks)
            expect(movers[mi].hunger < 0.02f);
        }
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);

    test(hunger_drain);
    test(hunger_speed_penalty);
    test(food_items);
    test(plant_spawn);
    test(plant_growth);
    test(plant_harvest);
    test(harvest_designation);
    test(freetime_idle_list);
    test(eating_food_search);
    test(eating_food_competition);
    test(eating_consumption);
    test(critical_need_search_cooldown);
    test(food_far_away);
    test(seek_keeps_its_goal);
    test(eating_starving_unassigns_job);
    test(unassign_job_pickup);
    test(drying_rack_berries);
    test(starvation_survival);
    test(hunger_daylength_independence);

    dayLength = 60.0f; // restore
    return summary();
}
