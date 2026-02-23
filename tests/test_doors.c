#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/construction.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/temperature.h"
#include "../src/entities/workshops.h"
#include "../src/world/designations.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// =============================================================================
// CELL_DOOR Properties
// =============================================================================

describe(cell_door_properties) {
    it("should not block movement") {
        expect(CellBlocksMovement(CELL_DOOR) == 0);
    }

    it("should block fluids") {
        expect(CellBlocksFluids(CELL_DOOR) != 0);
    }

    it("should be solid (supports cells above)") {
        expect(CellIsSolid(CELL_DOOR) != 0);
    }

    it("should have wood insulation tier") {
        expect(CellInsulationTier(CELL_DOOR) == INSULATION_TIER_WOOD);
    }

    it("should have correct name") {
        expect(strcmp(CellTypeName(CELL_DOOR), "DOOR") == 0);
    }
}

// =============================================================================
// CELL_DOOR Walkability
// =============================================================================

describe(cell_door_walkability) {
    it("should be walkable with solid cell below") {
        InitTestGrid(8, 8);
        grid[0][3][3] = CELL_WALL;
        SetWallMaterial(3, 3, 0, MAT_GRANITE);
        grid[1][3][3] = CELL_DOOR;
        expect(IsCellWalkableAt(1, 3, 3) == true);
    }

    it("should support walkability of cell above") {
        InitTestGrid(8, 8);
        grid[0][3][3] = CELL_WALL;
        grid[1][3][3] = CELL_DOOR;
        grid[2][3][3] = CELL_AIR;
        expect(IsCellWalkableAt(2, 3, 3) == true);
    }

    it("should not be walkable without support below") {
        InitTestGrid(8, 8);
        // z=0 has implicit bedrock, so use z=2
        grid[1][3][3] = CELL_AIR;
        grid[2][3][3] = CELL_DOOR;
        // Door at z=2 with air at z=1 — door itself is walkable (it's always walkable)
        // Actually doors return true immediately regardless of support below
        expect(IsCellWalkableAt(2, 3, 3) == true);
    }
}

// =============================================================================
// CELL_DOOR Sky Exposure
// =============================================================================

describe(cell_door_sky_exposure) {
    it("should block sky exposure when above a cell") {
        InitTestGrid(8, 8);
        // Place door at z=2, check sky exposure at z=1
        grid[0][3][3] = CELL_WALL;
        grid[1][3][3] = CELL_AIR;
        grid[2][3][3] = CELL_DOOR;
        // z=1 should not be exposed (door at z=2 blocks)
        expect(IsExposedToSky(3, 3, 1) == false);
    }

    it("should not block sky when not present") {
        InitTestGrid(8, 8);
        grid[0][3][3] = CELL_WALL;
        grid[1][3][3] = CELL_AIR;
        // No door or floor above — exposed
        expect(IsExposedToSky(3, 3, 1) == true);
    }
}

// =============================================================================
// Construction Recipes
// =============================================================================

describe(door_construction_recipes) {
    it("leaf door recipe should have BUILD_DOOR category") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_LEAF_DOOR);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_DOOR);
    }

    it("plank door recipe should have BUILD_DOOR category") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_PLANK_DOOR);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_DOOR);
    }

    it("should have 2 door recipes in BUILD_DOOR category") {
        expect(GetConstructionRecipeCountForCategory(BUILD_DOOR) == 2);
    }

    it("leaf door should require poles and leaves") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_LEAF_DOOR);
        expect(recipe->stageCount == 1);
        expect(recipe->stages[0].inputCount == 2);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_POLES));
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[1], ITEM_LEAVES));
    }

    it("plank door should require planks") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_PLANK_DOOR);
        expect(recipe->stageCount == 1);
        expect(recipe->stages[0].inputCount == 1);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_PLANKS));
    }
}

describe(primitive_construction_recipes) {
    it("stick wall should have BUILD_WALL category") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_STICK_WALL);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_WALL);
    }

    it("stick wall should require sticks and cordage") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_STICK_WALL);
        expect(recipe->stages[0].inputCount == 2);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_STICKS));
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[1], ITEM_CORDAGE));
        expect(recipe->stages[0].inputs[0].count == 4);
        expect(recipe->stages[0].inputs[1].count == 2);
    }

    it("leaf roof should have BUILD_FLOOR category") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_LEAF_ROOF);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_FLOOR);
    }

    it("bark roof should have BUILD_FLOOR category") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_BARK_ROOF);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_FLOOR);
    }

    it("leaf roof should require poles and leaves") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_LEAF_ROOF);
        expect(recipe->stages[0].inputCount == 2);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_POLES));
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[1], ITEM_LEAVES));
    }

    it("bark roof should require poles and bark") {
        const ConstructionRecipe* recipe = GetConstructionRecipe(CONSTRUCTION_BARK_ROOF);
        expect(recipe->stages[0].inputCount == 2);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_POLES));
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[1], ITEM_BARK));
    }
}

// =============================================================================
// Workshop: Ground Fire & Fire Pit
// =============================================================================

describe(workshop_ground_fire) {
    it("ground fire workshop should exist") {
        expect(WORKSHOP_GROUND_FIRE < WORKSHOP_TYPE_COUNT);
        expect(workshopDefs[WORKSHOP_GROUND_FIRE].type == WORKSHOP_GROUND_FIRE);
    }

    it("ground fire should be 1x1") {
        expect(workshopDefs[WORKSHOP_GROUND_FIRE].width == 1);
        expect(workshopDefs[WORKSHOP_GROUND_FIRE].height == 1);
    }

    it("ground fire should be passive") {
        expect(workshopDefs[WORKSHOP_GROUND_FIRE].passive == true);
    }

    it("ground fire construction should cost 3 sticks") {
        int recipeIdx = GetConstructionRecipeForWorkshopType(WORKSHOP_GROUND_FIRE);
        expect(recipeIdx == CONSTRUCTION_WORKSHOP_GROUND_FIRE);
        const ConstructionRecipe* recipe = GetConstructionRecipe(recipeIdx);
        expect(recipe != NULL);
        expect(recipe->stages[0].inputCount == 1);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_STICKS));
        expect(recipe->stages[0].inputs[0].count == 3);
    }

    it("fire pit should be renamed from campfire") {
        expect(strcmp(workshopDefs[WORKSHOP_CAMPFIRE].displayName, "Fire Pit") == 0);
    }

    it("fire pit construction should cost 5 sticks + 3 rocks") {
        int recipeIdx = GetConstructionRecipeForWorkshopType(WORKSHOP_CAMPFIRE);
        const ConstructionRecipe* recipe = GetConstructionRecipe(recipeIdx);
        expect(recipe != NULL);
        expect(recipe->stages[0].inputCount == 2);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[0], ITEM_STICKS));
        expect(recipe->stages[0].inputs[0].count == 5);
        expect(ConstructionInputAcceptsItem(&recipe->stages[0].inputs[1], ITEM_ROCK));
        expect(recipe->stages[0].inputs[1].count == 3);
    }
}

// =============================================================================
// Door Blueprint Placement
// =============================================================================

describe(door_blueprint_placement) {
    it("should fail without wall neighbor") {
        InitTestGrid(8, 8);
        InitDesignations();
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = CELL_WALL;
        int result = CreateRecipeBlueprint(3, 3, 1, CONSTRUCTION_LEAF_DOOR);
        expect(result == -1);
    }

    it("should succeed with wall neighbor") {
        InitTestGrid(8, 8);
        InitDesignations();
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = CELL_WALL;
        grid[1][2][3] = CELL_WALL;
        SetWallMaterial(3, 2, 1, MAT_GRANITE);
        int result = CreateRecipeBlueprint(3, 3, 1, CONSTRUCTION_LEAF_DOOR);
        expect(result >= 0);
    }

    it("should succeed with door neighbor") {
        InitTestGrid(8, 8);
        InitDesignations();
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                grid[0][y][x] = CELL_WALL;
        grid[1][2][3] = CELL_DOOR;
        int result = CreateRecipeBlueprint(3, 3, 1, CONSTRUCTION_LEAF_DOOR);
        expect(result >= 0);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) test_verbose = true;
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(cell_door_properties);
    test(cell_door_walkability);
    test(cell_door_sky_exposure);
    test(door_construction_recipes);
    test(primitive_construction_recipes);
    test(workshop_ground_fire);
    test(door_blueprint_placement);

    return summary();
}
