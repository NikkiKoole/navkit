#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/butchering.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/entities/animals.h"
#include "../src/entities/tool_quality.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/world/designations.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/game_state.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// Helper: count items of a specific type on the ground
static int CountItemsOfType(ItemType type) {
    int count = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type) count++;
    }
    return count;
}

// Helper: count total stack quantity of an item type
static int CountItemStacksOfType(ItemType type) {
    int count = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type) count += items[i].stackCount;
    }
    return count;
}

// Helper: find first item of type
static int FindFirstItemOfType(ItemType type) {
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].type == type) return i;
    }
    return -1;
}

// ===========================================================================
// Item definition tests
// ===========================================================================
describe(item_definitions) {
    it("ITEM_CARCASS should not be stackable") {
        expect(!(itemDefs[ITEM_CARCASS].flags & IF_STACKABLE));
        expect(itemDefs[ITEM_CARCASS].maxStack == 1);
    }

    it("ITEM_RAW_MEAT should be stackable and edible") {
        expect(itemDefs[ITEM_RAW_MEAT].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_RAW_MEAT].flags & IF_EDIBLE);
        expect(itemDefs[ITEM_RAW_MEAT].maxStack == 5);
        expect(itemDefs[ITEM_RAW_MEAT].nutrition > 0.0f);
    }

    it("ITEM_COOKED_MEAT should be stackable and edible") {
        expect(itemDefs[ITEM_COOKED_MEAT].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_COOKED_MEAT].flags & IF_EDIBLE);
        expect(itemDefs[ITEM_COOKED_MEAT].maxStack == 5);
        expect(itemDefs[ITEM_COOKED_MEAT].nutrition > 0.0f);
    }

    it("cooked meat should have higher nutrition than raw") {
        expect(itemDefs[ITEM_COOKED_MEAT].nutrition > itemDefs[ITEM_RAW_MEAT].nutrition);
    }

    it("ITEM_HIDE should be stackable but not edible") {
        expect(itemDefs[ITEM_HIDE].flags & IF_STACKABLE);
        expect(!(itemDefs[ITEM_HIDE].flags & IF_EDIBLE));
        expect(itemDefs[ITEM_HIDE].maxStack == 5);
    }
}

// ===========================================================================
// Yield table tests
// ===========================================================================
describe(yield_table) {
    it("default yield returns 2 products (meat + hide)") {
        const ButcherYieldDef* yield = GetButcherYield(MAT_NONE);
        expect(yield != NULL);
        expect(yield->productCount == 2);
    }

    it("default yield produces 3 raw meat") {
        const ButcherYieldDef* yield = GetButcherYield(MAT_NONE);
        expect(yield->products[0].type == ITEM_RAW_MEAT);
        expect(yield->products[0].count == 3);
    }

    it("default yield produces 1 hide") {
        const ButcherYieldDef* yield = GetButcherYield(MAT_NONE);
        expect(yield->products[1].type == ITEM_HIDE);
        expect(yield->products[1].count == 1);
    }

    it("unknown material falls back to default yield") {
        const ButcherYieldDef* yield = GetButcherYield(MAT_OAK);
        expect(yield->productCount == 2);
        expect(yield->products[0].type == ITEM_RAW_MEAT);
    }
}

// ===========================================================================
// Workshop definition tests
// ===========================================================================
describe(workshop_def) {
    it("butcher workshop should be defined") {
        expect(workshopDefs[WORKSHOP_BUTCHER].type == WORKSHOP_BUTCHER);
        expect(strcmp(workshopDefs[WORKSHOP_BUTCHER].displayName, "Butcher") == 0);
    }

    it("butcher workshop should be 1x1") {
        expect(workshopDefs[WORKSHOP_BUTCHER].width == 1);
        expect(workshopDefs[WORKSHOP_BUTCHER].height == 1);
    }

    it("butcher workshop should not be passive") {
        expect(!workshopDefs[WORKSHOP_BUTCHER].passive);
    }

    it("butcher recipe input should be ITEM_CARCASS") {
        expect(butcherRecipes[0].inputType == ITEM_CARCASS);
        expect(butcherRecipes[0].inputCount == 1);
    }

    it("butcher recipe output should be ITEM_NONE (yield table handles output)") {
        expect(butcherRecipes[0].outputType == ITEM_NONE);
    }
}

// ===========================================================================
// Cooking recipe tests
// ===========================================================================
describe(cooking_recipes) {
    it("campfire should have Cook Meat recipe") {
        bool found = false;
        for (int i = 0; i < campfireRecipeCount; i++) {
            if (campfireRecipes[i].inputType == ITEM_RAW_MEAT) {
                found = true;
                expect(campfireRecipes[i].outputType == ITEM_COOKED_MEAT);
                expect(campfireRecipes[i].outputCount == 1);
                expect(campfireRecipes[i].passiveWorkRequired > 0.0f);
            }
        }
        expect(found);
    }

    it("hearth should have Cook Meat recipe") {
        bool found = false;
        for (int i = 0; i < hearthRecipeCount; i++) {
            if (hearthRecipes[i].inputType == ITEM_RAW_MEAT) {
                found = true;
                expect(hearthRecipes[i].outputType == ITEM_COOKED_MEAT);
                expect(hearthRecipes[i].outputCount == 2);
                expect(hearthRecipes[i].inputCount == 2);
            }
        }
        expect(found);
    }
}

// ===========================================================================
// KillAnimal tests
// ===========================================================================
describe(kill_animal) {
    it("KillAnimal deactivates animal and spawns carcass") {
        InitTestGrid(10, 10);
        ClearAnimals();
        ClearItems();

        // Manually set up an animal (avoid SpawnAnimal's random walkable cell search)
        animals[0].x = 5.5f * CELL_SIZE;
        animals[0].y = 5.5f * CELL_SIZE;
        animals[0].z = 0.0f;
        animals[0].type = ANIMAL_GRAZER;
        animals[0].active = true;
        animals[0].behavior = BEHAVIOR_SIMPLE_GRAZER;
        animals[0].state = ANIMAL_IDLE;
        animalCount = 1;

        float ax = animals[0].x;
        float ay = animals[0].y;

        KillAnimal(0);

        // Animal should be deactivated
        expect(!animals[0].active);
        // animalCount is a high-water mark, should NOT decrement
        expect(animalCount == 1);

        // Carcass should be spawned
        int carcassIdx = FindFirstItemOfType(ITEM_CARCASS);
        expect(carcassIdx >= 0);
        expect(items[carcassIdx].active);
        // Position should match animal death position
        expect(items[carcassIdx].x == ax);
        expect(items[carcassIdx].y == ay);
    }

    it("KillAnimal on invalid index does nothing") {
        InitTestGrid(10, 10);
        ClearAnimals();
        ClearItems();

        KillAnimal(-1);
        KillAnimal(999);
        expect(CountItemsOfType(ITEM_CARCASS) == 0);
    }

    it("KillAnimal on already-dead animal does nothing") {
        InitTestGrid(10, 10);
        ClearAnimals();
        ClearItems();

        // Manually set up an animal
        animals[0].x = 5.5f * CELL_SIZE;
        animals[0].y = 5.5f * CELL_SIZE;
        animals[0].z = 0.0f;
        animals[0].type = ANIMAL_GRAZER;
        animals[0].active = true;
        animals[0].behavior = BEHAVIOR_SIMPLE_GRAZER;
        animals[0].state = ANIMAL_IDLE;
        animalCount = 1;

        KillAnimal(0);
        int carcassCount1 = CountItemsOfType(ITEM_CARCASS);

        KillAnimal(0);  // already dead
        int carcassCount2 = CountItemsOfType(ITEM_CARCASS);
        expect(carcassCount2 == carcassCount1);
    }
}

// ===========================================================================
// Butcher craft job E2E test
// ===========================================================================
describe(butcher_craft_job) {
    it("butchering a carcass produces meat and hide") {
        // 10x10 grid with solid floor at z=0, air at z=1
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
        ClearWorkshops();
        ClearStockpiles();
        InitBalance();
        toolRequirementsEnabled = false;

        // Create butcher workshop at (5,5,0)
        int wsIdx = CreateWorkshop(5, 5, 0, WORKSHOP_BUTCHER);
        expect(wsIdx >= 0);
        Workshop* ws = &workshops[wsIdx];

        // Add a bill: butcher 1 carcass
        int billIdx = AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        expect(billIdx >= 0);

        // Spawn a carcass on the workshop work tile
        int carcassIdx = SpawnItem(5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0.0f, ITEM_CARCASS);
        expect(carcassIdx >= 0);

        // Create mover adjacent to workshop
        Point goal = {4, 5, 0};
        InitMover(&movers[0], 4.5f * CELL_SIZE, 5.5f * CELL_SIZE, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;

        // Tick simulation until job completes or timeout
        bool done = false;
        for (int tick = 0; tick < 2000 && !done; tick++) {
            Tick();
            AssignJobs();
            JobsTick();

            // Check if butchering completed
            if (ws->bills[0].completedCount >= 1) {
                done = true;
            }
        }

        expect(done);

        // Verify outputs: 3 raw meat (as stacks) + 1 hide
        int meatCount = CountItemStacksOfType(ITEM_RAW_MEAT);
        int hideCount = CountItemStacksOfType(ITEM_HIDE);
        expect(meatCount == 3);
        expect(hideCount == 1);

        // Verify carcass was consumed
        expect(CountItemsOfType(ITEM_CARCASS) == 0);

        if (test_verbose) {
            printf("  Butcher E2E: meat=%d, hide=%d, done=%d\n", meatCount, hideCount, done);
        }
    }
}

// ===========================================================================
// Edibility tests
// ===========================================================================
describe(edibility) {
    it("raw meat is edible") {
        expect(ItemIsEdible(ITEM_RAW_MEAT));
    }

    it("cooked meat is edible") {
        expect(ItemIsEdible(ITEM_COOKED_MEAT));
    }

    it("carcass is not edible") {
        expect(!ItemIsEdible(ITEM_CARCASS));
    }

    it("hide is not edible") {
        expect(!ItemIsEdible(ITEM_HIDE));
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) test_verbose = true;
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(item_definitions);
    test(yield_table);
    test(workshop_def);
    test(cooking_recipes);
    test(kill_animal);
    test(butcher_craft_job);
    test(edibility);

    return summary();
}
