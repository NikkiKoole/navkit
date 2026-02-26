#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/construction.h"
#include "../src/world/cell_defs.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/workshops.h"
#include "../src/entities/stockpiles.h"
#include "../src/simulation/lighting.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// =============================================================================
// Item definitions
// =============================================================================

describe(glass_items) {
    it("ITEM_GLASS should have correct definition") {
        expect(ITEM_GLASS >= 0);
        expect(ITEM_GLASS < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_GLASS].name, "Glass") == 0);
        expect(itemDefs[ITEM_GLASS].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_GLASS].flags & IF_BUILDING_MAT);
        expect(itemDefs[ITEM_GLASS].maxStack == 10);
    }
}

describe(lye_mortar_items) {
    it("ITEM_LYE should have correct definition") {
        expect(ITEM_LYE >= 0);
        expect(ITEM_LYE < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_LYE].name, "Lye") == 0);
        expect(itemDefs[ITEM_LYE].flags & IF_STACKABLE);
        expect(!(itemDefs[ITEM_LYE].flags & IF_BUILDING_MAT));
        expect(itemDefs[ITEM_LYE].maxStack == 20);
    }

    it("ITEM_MORTAR should have correct definition") {
        expect(ITEM_MORTAR >= 0);
        expect(ITEM_MORTAR < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_MORTAR].name, "Mortar") == 0);
        expect(itemDefs[ITEM_MORTAR].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_MORTAR].flags & IF_BUILDING_MAT);
        expect(itemDefs[ITEM_MORTAR].maxStack == 20);
    }
}

// =============================================================================
// Workshop recipes
// =============================================================================

describe(kiln_glass_recipe) {
    it("kiln should have Make Glass recipe") {
        bool found = false;
        for (int i = 0; i < kilnRecipeCount; i++) {
            if (strcmp(kilnRecipes[i].name, "Make Glass") == 0) {
                found = true;
                expect(kilnRecipes[i].inputType == ITEM_SAND);
                expect(kilnRecipes[i].inputCount == 3);
                expect(kilnRecipes[i].outputType == ITEM_GLASS);
                expect(kilnRecipes[i].outputCount == 1);
                expect(kilnRecipes[i].fuelRequired == 1);
                expect(kilnRecipes[i].workRequired == 8.0f);
                break;
            }
        }
        expect(found);
    }
}

describe(hearth_lye_recipe) {
    it("hearth should have Make Lye recipe") {
        bool found = false;
        for (int i = 0; i < hearthRecipeCount; i++) {
            if (strcmp(hearthRecipes[i].name, "Make Lye") == 0) {
                found = true;
                expect(hearthRecipes[i].inputType == ITEM_ASH);
                expect(hearthRecipes[i].inputCount == 2);
                expect(hearthRecipes[i].inputType2 == ITEM_WATER);
                expect(hearthRecipes[i].inputCount2 == 1);
                expect(hearthRecipes[i].outputType == ITEM_LYE);
                expect(hearthRecipes[i].outputCount == 1);
                expect(hearthRecipes[i].workRequired == 4.0f);
                break;
            }
        }
        expect(found);
    }
}

describe(stonecutter_mortar_recipe) {
    it("stonecutter should have Mix Mortar recipe") {
        bool found = false;
        for (int i = 0; i < stonecutterRecipeCount; i++) {
            if (strcmp(stonecutterRecipes[i].name, "Mix Mortar") == 0) {
                found = true;
                expect(stonecutterRecipes[i].inputType == ITEM_LYE);
                expect(stonecutterRecipes[i].inputCount == 1);
                expect(stonecutterRecipes[i].inputType2 == ITEM_SAND);
                expect(stonecutterRecipes[i].inputCount2 == 1);
                expect(stonecutterRecipes[i].outputType == ITEM_MORTAR);
                expect(stonecutterRecipes[i].outputCount == 2);
                expect(stonecutterRecipes[i].workRequired == 2.0f);
                break;
            }
        }
        expect(found);
    }
}

// =============================================================================
// CELL_WINDOW
// =============================================================================

describe(cell_window) {
    it("CELL_WINDOW should exist and have CF_WALL flags") {
        expect(CELL_WINDOW >= 0);
        expect(CELL_WINDOW < CELL_TYPE_COUNT);
        expect(CellBlocksMovement(CELL_WINDOW));
        expect(CellBlocksFluids(CELL_WINDOW));
        expect(CellIsSolid(CELL_WINDOW));
    }

    it("CELL_WINDOW should drop ITEM_GLASS") {
        expect(cellDefs[CELL_WINDOW].dropsItem == ITEM_GLASS);
        expect(cellDefs[CELL_WINDOW].dropCount == 1);
    }
}

// =============================================================================
// Construction recipes
// =============================================================================

describe(window_construction) {
    it("glass window construction recipe should exist") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_GLASS_WINDOW);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WALL);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputCount == 2);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_GLASS);
        expect(r->stages[0].inputs[0].count == 2);
        expect(r->stages[0].inputs[1].alternatives[0].itemType == ITEM_STICKS);
        expect(r->stages[0].inputs[1].count == 2);
    }
}

describe(mortar_wall_construction) {
    it("mortar wall construction recipe should exist") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_MORTAR_WALL);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WALL);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputCount == 2);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_MORTAR);
        expect(r->stages[0].inputs[0].count == 3);
        expect(r->stages[0].inputs[1].alternatives[0].itemType == ITEM_ROCK);
        expect(r->stages[0].inputs[1].count == 2);
        expect(r->materialFromSlot == 1);
    }
}

// =============================================================================
// Window light transmission
// =============================================================================

describe(window_light) {
    it("sky light should pass through CELL_WINDOW but not CELL_WALL") {
        InitTestGrid(8, 8);

        // Set up: solid ground at z=0, air at z=1..3
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                grid[2][y][x] = CELL_AIR;
                grid[3][y][x] = CELL_AIR;
            }
        }

        // Place a solid roof at z=3 across the whole area
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                grid[3][y][x] = CELL_WALL;
            }
        }

        // Put a window at (4,4,3) and keep a wall at (2,2,3)
        grid[3][4][4] = CELL_WINDOW;

        // Recompute lighting
        lightingEnabled = true;
        skyLightEnabled = true;
        lightingDirty = true;
        RecomputeLighting();

        // Cell below window should have some sky light
        int lightBelowWindow = GetSkyLight(4, 4, 2);
        // Cell below solid wall should have no direct sky light
        int lightBelowWall = GetSkyLight(2, 2, 2);

        expect(lightBelowWindow > lightBelowWall);
    }
}

// =============================================================================
// Stockpile filters
// =============================================================================

describe(loop_closer_stockpile_filters) {
    it("ITEM_GLASS should be in FILTER_CAT_STONE") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_GLASS) {
                expect(STOCKPILE_FILTERS[i].category == FILTER_CAT_STONE);
                found = true;
                break;
            }
        }
        expect(found);
    }

    it("ITEM_LYE should be in FILTER_CAT_CRAFT") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_LYE) {
                expect(STOCKPILE_FILTERS[i].category == FILTER_CAT_CRAFT);
                found = true;
                break;
            }
        }
        expect(found);
    }

    it("ITEM_MORTAR should be in FILTER_CAT_EARTH") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_MORTAR) {
                expect(STOCKPILE_FILTERS[i].category == FILTER_CAT_EARTH);
                found = true;
                break;
            }
        }
        expect(found);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    if (quiet) set_quiet_mode(1);

    test(glass_items);
    test(lye_mortar_items);
    test(kiln_glass_recipe);
    test(hearth_lye_recipe);
    test(stonecutter_mortar_recipe);
    test(cell_window);
    test(window_construction);
    test(mortar_wall_construction);
    test(window_light);
    test(loop_closer_stockpile_filters);

    return summary();
}
