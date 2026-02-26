#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/construction.h"
#include "../src/world/designations.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/workshops.h"
#include "../src/entities/stockpiles.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// =============================================================================
// Item definitions
// =============================================================================

describe(reeds_items) {
    it("ITEM_REEDS should have correct definition") {
        expect(ITEM_REEDS >= 0);
        expect(ITEM_REEDS < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_REEDS].name, "Reeds") == 0);
        expect(itemDefs[ITEM_REEDS].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_REEDS].maxStack == 20);
        expect(itemDefs[ITEM_REEDS].defaultMaterial == MAT_NONE);
    }

    it("ITEM_REED_MAT should have correct definition") {
        expect(ITEM_REED_MAT >= 0);
        expect(ITEM_REED_MAT < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_REED_MAT].name, "Reed Mat") == 0);
        expect(itemDefs[ITEM_REED_MAT].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_REED_MAT].flags & IF_BUILDING_MAT);
        expect(itemDefs[ITEM_REED_MAT].maxStack == 10);
    }
}

// =============================================================================
// Vegetation type
// =============================================================================

describe(reeds_vegetation) {
    it("VEG_REEDS should exist after VEG_GRASS_TALLER") {
        expect(VEG_REEDS > VEG_GRASS_TALLER);
    }

    it("should be able to set and get VEG_REEDS on a cell") {
        InitTestGrid(32, 32);
        SetVegetation(5, 5, 0, VEG_REEDS);
        expect(GetVegetation(5, 5, 0) == VEG_REEDS);
    }
}

// =============================================================================
// Stockpile filters
// =============================================================================

describe(reeds_stockpile) {
    it("ITEM_REEDS should be in stockpile filter list") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_REEDS) {
                found = true;
                break;
            }
        }
        expect(found);
    }

    it("ITEM_REED_MAT should be in stockpile filter list") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_REED_MAT) {
                found = true;
                break;
            }
        }
        expect(found);
    }
}

// =============================================================================
// Rope maker recipes
// =============================================================================

describe(reeds_recipes) {
    it("rope maker should have Weave Reed Mat recipe") {
        extern Recipe ropeMakerRecipes[];
        extern int ropeMakerRecipeCount;
        bool found = false;
        for (int i = 0; i < ropeMakerRecipeCount; i++) {
            if (ropeMakerRecipes[i].inputType == ITEM_REEDS &&
                ropeMakerRecipes[i].outputType == ITEM_REED_MAT) {
                found = true;
                expect(ropeMakerRecipes[i].inputCount == 4);
                expect(ropeMakerRecipes[i].outputCount == 1);
                break;
            }
        }
        expect(found);
    }

    it("rope maker should have Weave Reed Basket recipe") {
        extern Recipe ropeMakerRecipes[];
        extern int ropeMakerRecipeCount;
        bool found = false;
        for (int i = 0; i < ropeMakerRecipeCount; i++) {
            if (ropeMakerRecipes[i].inputType == ITEM_REEDS &&
                ropeMakerRecipes[i].outputType == ITEM_BASKET) {
                found = true;
                expect(ropeMakerRecipes[i].inputCount == 6);
                expect(ropeMakerRecipes[i].inputType2 == ITEM_CORDAGE);
                expect(ropeMakerRecipes[i].inputCount2 == 1);
                break;
            }
        }
        expect(found);
    }
}

// =============================================================================
// Construction recipe
// =============================================================================

describe(reeds_construction) {
    it("CONSTRUCTION_REED_ROOF should exist") {
        expect(CONSTRUCTION_REED_ROOF >= 0);
        expect(CONSTRUCTION_REED_ROOF < CONSTRUCTION_RECIPE_COUNT);
    }

    it("reed roof should be a floor recipe with 2 stages") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_REED_ROOF);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_FLOOR);
        expect(recipe->stageCount == 2);
    }

    it("reed roof stage 0 should require 2 poles") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_REED_ROOF);
        expect(recipe->stages[0].inputCount == 1);
        expect(recipe->stages[0].inputs[0].alternatives[0].itemType == ITEM_POLES);
        expect(recipe->stages[0].inputs[0].count == 2);
    }

    it("reed roof stage 1 should require 4 reeds") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_REED_ROOF);
        expect(recipe->stages[1].inputCount == 1);
        expect(recipe->stages[1].inputs[0].alternatives[0].itemType == ITEM_REEDS);
        expect(recipe->stages[1].inputs[0].count == 4);
    }
}

// =============================================================================
// Gather reeds designation
// =============================================================================

describe(reeds_gather) {
    it("should designate gather reeds on a cell with VEG_REEDS") {
        InitTestGrid(32, 32);
        InitDesignations();
        // Set up walkable cell: solid dirt at z=0, air at z=1
        grid[0][5][5] = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        grid[1][5][5] = CELL_AIR;
        SetVegetation(5, 5, 0, VEG_REEDS);
        // Mark as explored
        exploredGrid[1][5][5] = 1;

        bool result = DesignateGatherReeds(5, 5, 1);
        expect(result);
        expect(HasGatherReedsDesignation(5, 5, 1));
    }

    it("should not designate gather reeds without VEG_REEDS") {
        InitTestGrid(32, 32);
        InitDesignations();
        grid[0][5][5] = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        grid[1][5][5] = CELL_AIR;
        SetVegetation(5, 5, 0, VEG_GRASS_TALLER);
        exploredGrid[1][5][5] = 1;

        bool result = DesignateGatherReeds(5, 5, 1);
        expect(!result);
    }

    it("completing gather reeds should spawn ITEM_REEDS and clear vegetation") {
        InitTestGrid(32, 32);
        InitDesignations();
        ClearItems();
        grid[0][5][5] = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        grid[1][5][5] = CELL_AIR;
        SetVegetation(5, 5, 0, VEG_REEDS);
        exploredGrid[1][5][5] = 1;

        DesignateGatherReeds(5, 5, 1);
        CompleteGatherReedsDesignation(5, 5, 1, 0);

        expect(GetVegetation(5, 5, 0) == VEG_NONE);
        expect(!HasGatherReedsDesignation(5, 5, 1));

        // Find the spawned item
        bool foundReeds = false;
        for (int i = 0; i < itemCount; i++) {
            if (items[i].active && items[i].type == ITEM_REEDS) {
                foundReeds = true;
                break;
            }
        }
        expect(foundReeds);
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

    test(reeds_items);
    test(reeds_vegetation);
    test(reeds_stockpile);
    test(reeds_recipes);
    test(reeds_construction);
    test(reeds_gather);

    return summary();
}
