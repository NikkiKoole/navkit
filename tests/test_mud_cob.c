#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/construction.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/workshops.h"
#include "../src/entities/stockpiles.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// =============================================================================
// Material definitions
// =============================================================================

describe(mud_cob_materials) {
    it("MAT_MUD should exist and have correct properties") {
        expect(MAT_MUD > 0);
        expect(MAT_MUD < MAT_COUNT);
        expect(strcmp(materialDefs[MAT_MUD].name, "Mud") == 0);
        expect(materialDefs[MAT_MUD].dropsItem == ITEM_MUD);
        expect(materialDefs[MAT_MUD].insulationTier == INSULATION_TIER_AIR);
        expect(!(materialDefs[MAT_MUD].flags & MF_FLAMMABLE));
    }

    it("MAT_COB should exist and have correct properties") {
        expect(MAT_COB > 0);
        expect(MAT_COB < MAT_COUNT);
        expect(strcmp(materialDefs[MAT_COB].name, "Cob") == 0);
        expect(materialDefs[MAT_COB].dropsItem == ITEM_COB);
        expect(materialDefs[MAT_COB].insulationTier == INSULATION_TIER_STONE);
        expect(!(materialDefs[MAT_COB].flags & MF_FLAMMABLE));
    }
}

// =============================================================================
// Item definitions
// =============================================================================

describe(mud_cob_items) {
    it("ITEM_MUD should have correct definition") {
        expect(ITEM_MUD >= 0);
        expect(ITEM_MUD < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_MUD].name, "Mud") == 0);
        expect(itemDefs[ITEM_MUD].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_MUD].flags & IF_BUILDING_MAT);
        expect(itemDefs[ITEM_MUD].maxStack == 20);
        expect(itemDefs[ITEM_MUD].defaultMaterial == MAT_MUD);
    }

    it("ITEM_COB should have correct definition") {
        expect(ITEM_COB >= 0);
        expect(ITEM_COB < ITEM_TYPE_COUNT);
        expect(strcmp(itemDefs[ITEM_COB].name, "Cob") == 0);
        expect(itemDefs[ITEM_COB].flags & IF_STACKABLE);
        expect(itemDefs[ITEM_COB].flags & IF_BUILDING_MAT);
        expect(itemDefs[ITEM_COB].maxStack == 20);
        expect(itemDefs[ITEM_COB].defaultMaterial == MAT_COB);
    }
}

// =============================================================================
// Workshop definition
// =============================================================================

describe(mud_mixer_workshop) {
    it("WORKSHOP_MUD_MIXER should exist in enum") {
        expect(WORKSHOP_MUD_MIXER >= 0);
        expect(WORKSHOP_MUD_MIXER < WORKSHOP_TYPE_COUNT);
    }

    it("should have correct workshop definition") {
        const WorkshopDef* def = &workshopDefs[WORKSHOP_MUD_MIXER];
        expect(def->type == WORKSHOP_MUD_MIXER);
        expect(strcmp(def->name, "MUD_MIXER") == 0);
        expect(strcmp(def->displayName, "Mud Mixer") == 0);
        expect(def->width == 2);
        expect(def->height == 1);
        expect(def->passive == false);
    }

    it("should have 2 recipes") {
        const WorkshopDef* def = &workshopDefs[WORKSHOP_MUD_MIXER];
        expect(def->recipeCount == 2);
    }

    it("Mix Mud recipe should use dirt+clay and produce mud") {
        const Recipe* r = &mudMixerRecipes[0];
        expect(strcmp(r->name, "Mix Mud") == 0);
        expect(r->inputType == ITEM_DIRT);
        expect(r->inputCount == 2);
        expect(r->inputType2 == ITEM_CLAY);
        expect(r->inputCount2 == 1);
        expect(r->outputType == ITEM_MUD);
        expect(r->outputCount == 3);
        expect(r->workRequired == 3.0f);
        expect(r->passiveWorkRequired == 0);
    }

    it("Make Cob recipe should use mud+dried grass and produce cob") {
        const Recipe* r = &mudMixerRecipes[1];
        expect(strcmp(r->name, "Make Cob") == 0);
        expect(r->inputType == ITEM_MUD);
        expect(r->inputCount == 2);
        expect(r->inputType2 == ITEM_DRIED_GRASS);
        expect(r->inputCount2 == 1);
        expect(r->outputType == ITEM_COB);
        expect(r->outputCount == 2);
        expect(r->workRequired == 4.0f);
        expect(r->passiveWorkRequired == 0);
    }
}

// =============================================================================
// Construction recipes
// =============================================================================

describe(mud_cob_construction) {
    it("mud wall recipe should exist with correct properties") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_MUD_WALL);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WALL);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputCount == 1);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_MUD);
        expect(r->stages[0].inputs[0].count == 4);
        expect(r->resultMaterial == MAT_MUD);
    }

    it("cob wall recipe should exist with correct properties") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_COB_WALL);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WALL);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputCount == 1);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_COB);
        expect(r->stages[0].inputs[0].count == 3);
        expect(r->resultMaterial == MAT_COB);
    }

    it("mud floor recipe should exist with correct properties") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_MUD_FLOOR);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_FLOOR);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_MUD);
        expect(r->stages[0].inputs[0].count == 2);
        expect(r->resultMaterial == MAT_MUD);
    }

    it("mud mixer workshop construction recipe should exist") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_WORKSHOP_MUD_MIXER);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WORKSHOP);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_STICKS);
        expect(r->stages[0].inputs[0].count == 4);
    }

    it("workshop type should map to construction recipe") {
        int idx = GetConstructionRecipeForWorkshopType(WORKSHOP_MUD_MIXER);
        expect(idx == CONSTRUCTION_WORKSHOP_MUD_MIXER);
    }
}

// =============================================================================
// Stockpile filters
// =============================================================================

describe(mud_cob_stockpile_filters) {
    it("ITEM_MUD should be in stockpile filter list") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_MUD) {
                found = true;
                expect(STOCKPILE_FILTERS[i].category == FILTER_CAT_EARTH);
                break;
            }
        }
        expect(found);
    }

    it("ITEM_COB should be in stockpile filter list") {
        bool found = false;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if (STOCKPILE_FILTERS[i].itemType == ITEM_COB) {
                found = true;
                expect(STOCKPILE_FILTERS[i].category == FILTER_CAT_EARTH);
                break;
            }
        }
        expect(found);
    }
}

// =============================================================================
// Water proximity helper
// =============================================================================

describe(water_proximity) {
    it("should detect water adjacent to workshop footprint") {
        InitTestGrid(10, 10);
        FillGroundLevel();
        InitWater();
        // Place water at (3, 2, 1)
        SetWaterLevel(3, 2, 1, 5);
        // Workshop at (4, 2) — water is to the west
        expect(HasWater(3, 2, 1));
    }

    it("should not detect water far from footprint") {
        InitTestGrid(10, 10);
        FillGroundLevel();
        InitWater();
        // No water near (5, 5)
        expect(!HasWater(5, 5, 1));
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

    test(mud_cob_materials);
    test(mud_cob_items);
    test(mud_mixer_workshop);
    test(mud_cob_construction);
    test(mud_cob_stockpile_filters);
    test(water_proximity);

    return summary();
}
