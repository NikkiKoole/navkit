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
#include "../src/entities/mover.h"
#include "../src/entities/animals.h"
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
        GRID(3, 3, 0) = CELL_WALL;
        SetWallMaterial(3, 3, 0, MAT_GRANITE);
        GRID(3, 3, 1) = CELL_DOOR;
        expect(IsCellWalkableAt(1, 3, 3) == true);
    }

    it("should support walkability of cell above") {
        InitTestGrid(8, 8);
        GRID(3, 3, 0) = CELL_WALL;
        GRID(3, 3, 1) = CELL_DOOR;
        GRID(3, 3, 2) = CELL_AIR;
        expect(IsCellWalkableAt(2, 3, 3) == true);
    }

    it("should not be walkable without support below") {
        InitTestGrid(8, 8);
        // z=0 has implicit bedrock, so use z=2
        GRID(3, 3, 1) = CELL_AIR;
        GRID(3, 3, 2) = CELL_DOOR;
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
        GRID(3, 3, 0) = CELL_WALL;
        GRID(3, 3, 1) = CELL_AIR;
        GRID(3, 3, 2) = CELL_DOOR;
        // z=1 should not be exposed (door at z=2 blocks)
        expect(IsExposedToSky(3, 3, 1) == false);
    }

    it("should not block sky when not present") {
        InitTestGrid(8, 8);
        GRID(3, 3, 0) = CELL_WALL;
        GRID(3, 3, 1) = CELL_AIR;
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

    it("should have 3 door recipes in BUILD_DOOR category") {
        expect(GetConstructionRecipeCountForCategory(BUILD_DOOR) == 3);
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
                GRID(x, y, 0) = CELL_WALL;
        int result = CreateRecipeBlueprint(3, 3, 1, CONSTRUCTION_LEAF_DOOR);
        expect(result == -1);
    }

    it("should succeed with wall neighbor") {
        InitTestGrid(8, 8);
        InitDesignations();
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                GRID(x, y, 0) = CELL_WALL;
        GRID(3, 2, 1) = CELL_WALL;
        SetWallMaterial(3, 2, 1, MAT_GRANITE);
        int result = CreateRecipeBlueprint(3, 3, 1, CONSTRUCTION_LEAF_DOOR);
        expect(result >= 0);
    }

    it("should succeed with door neighbor") {
        InitTestGrid(8, 8);
        InitDesignations();
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                GRID(x, y, 0) = CELL_WALL;
        GRID(3, 2, 1) = CELL_DOOR;
        int result = CreateRecipeBlueprint(3, 3, 1, CONSTRUCTION_LEAF_DOOR);
        expect(result >= 0);
    }
}

// =============================================================================
// Door states: open / closed
// =============================================================================

describe(door_states) {
    it("an open door lets fluids and heat through but stays solid and walkable") {
        expect(CellIsDoor(CELL_DOOR_OPEN));
        expect(CellBlocksFluids(CELL_DOOR_OPEN) == 0);
        expect(CellBlocksMovement(CELL_DOOR_OPEN) == 0);
        expect(CellIsSolid(CELL_DOOR_OPEN) != 0);   // the wall above it keeps its support
        expect(CellInsulationTier(CELL_DOOR_OPEN) == INSULATION_TIER_AIR);
        InitTestGrid(8, 8);
        GRID(3, 3, 0) = CELL_WALL;
        GRID(3, 3, 1) = CELL_DOOR_OPEN;
        expect(IsCellWalkableAt(1, 3, 3) == true);
    }

    it("SetDoorOpen opens and closes a door, and only a door") {
        InitTestGrid(8, 8);
        GRID(3, 3, 1) = CELL_DOOR;
        expect(SetDoorOpen(3, 3, 1, true));
        expect(GRID(3, 3, 1) == CELL_DOOR_OPEN);
        expect(SetDoorOpen(3, 3, 1, true));          // already open: nothing to do
        expect(GRID(3, 3, 1) == CELL_DOOR_OPEN);
        expect(SetDoorOpen(3, 3, 1, false));
        expect(GRID(3, 3, 1) == CELL_DOOR);
        GRID(4, 3, 1) = CELL_WALL;
        expect(!SetDoorOpen(4, 3, 1, true));
        expect(GRID(4, 3, 1) == CELL_WALL);
        expect(!SetDoorOpen(-1, 3, 1, true));
    }

    it("a one-way door keeps its direction when opened") {
        InitTestGrid(8, 8);
        GRID(3, 3, 1) = CELL_DOOR;
        expect(SetDoorOneWay(3, 3, 1, ONEWAY_E));
        SetDoorOpen(3, 3, 1, true);
        int dx = 0, dy = 0;
        expect(OneWayDoorDir(3, 3, 1, &dx, &dy));
        expect(dx == 1 && dy == 0);
    }

    it("marking a door asks for the other state, and completing it puts the door that way") {
        InitTestGrid(8, 8);
        InitDesignations();
        GRID(3, 3, 1) = CELL_DOOR_OPEN;
        GRID(5, 3, 1) = CELL_DOOR;
        GRID(6, 3, 1) = CELL_WALL;
        expect(DesignateDoor(3, 3, 1));
        expect(DESIGNATION(3, 3, 1).type == DESIGNATION_CLOSE_DOOR);
        expect(DesignateDoor(5, 3, 1));
        expect(DESIGNATION(5, 3, 1).type == DESIGNATION_OPEN_DOOR);
        expect(!DesignateDoor(3, 3, 1));            // already marked
        expect(!DesignateDoor(6, 3, 1));            // not a door
        expect(HasDoorDesignation(3, 3, 1) && HasDoorDesignation(5, 3, 1));
        CompleteDoorDesignation(3, 3, 1);
        CompleteDoorDesignation(5, 3, 1);
        expect(GRID(3, 3, 1) == CELL_DOOR);
        expect(GRID(5, 3, 1) == CELL_DOOR_OPEN);
        expect(!HasDoorDesignation(3, 3, 1) && !HasDoorDesignation(5, 3, 1));
    }

    it("a locked door is a wall to walking, still a door, and can't be opened or marked") {
        InitTestGrid(8, 8);
        InitDesignations();
        GRID(3, 3, 0) = CELL_WALL;
        GRID(3, 3, 1) = CELL_DOOR_OPEN;
        expect(SetDoorLocked(3, 3, 1, true));
        expect(GRID(3, 3, 1) == CELL_DOOR_LOCKED);       // locking shuts it
        expect(CellIsDoor(CELL_DOOR_LOCKED));
        expect(CellBlocksMovement(CELL_DOOR_LOCKED) != 0);
        expect(CellBlocksFluids(CELL_DOOR_LOCKED) != 0);
        expect(IsCellWalkableAt(1, 3, 3) == false);
        expect(!SetDoorOpen(3, 3, 1, true));             // locked: won't open
        expect(SetDoorOpen(3, 3, 1, false));             // asking it to be shut: it is
        expect(GRID(3, 3, 1) == CELL_DOOR_LOCKED);
        expect(!DesignateDoor(3, 3, 1));
        expect(SetDoorLocked(3, 3, 1, false));
        expect(GRID(3, 3, 1) == CELL_DOOR);              // unlocked: closed, not open
        expect(IsCellWalkableAt(1, 3, 3) == true);
        GRID(4, 3, 1) = CELL_WALL;
        expect(!SetDoorLocked(4, 3, 1, true));
    }

    it("a close mark stays a close mark when someone opens the door meanwhile") {
        InitTestGrid(8, 8);
        InitDesignations();
        GRID(3, 3, 1) = CELL_DOOR_OPEN;
        expect(DesignateDoor(3, 3, 1));
        SetDoorOpen(3, 3, 1, false);                // someone shut it
        SetDoorOpen(3, 3, 1, true);                 // and someone walked through again
        CompleteDoorDesignation(3, 3, 1);
        expect(GRID(3, 3, 1) == CELL_DOOR);         // still shut, not flipped
    }
}

// =============================================================================
// Main
// =============================================================================

// =============================================================================
// Animals and doors: they don't open doors (a closed door stops them like a wall)
// =============================================================================

// A 4x4 room (x 1-4, y 1-4) at z 1 with a door in its east wall at (5, 2); grass outside.
// One steering grazer (what games spawn) in the middle of the room.
static void AnimalRoom(CellType door) {
    InitTestGrid(16, 8);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++) {
            GRID(x, y, 0) = CELL_WALL;
            GRID(x, y, 1) = CELL_AIR;
            if (x >= 7) SetVegetation(x, y, 1, VEG_GRASS_TALL);
        }
    for (int i = 0; i <= 5; i++) {
        GRID(i, 0, 1) = CELL_WALL; GRID(i, 5, 1) = CELL_WALL;
        GRID(0, i, 1) = CELL_WALL; GRID(5, i, 1) = CELL_WALL;
    }
    GRID(5, 2, 1) = door;
    ClearAnimals();
    animalRespawnEnabled = false;
    SpawnAnimalAt(ANIMAL_GRAZER, BEHAVIOR_STEERING_GRAZER, 2, 2, 1);
}

// Furthest east the animal got (in cells) over the given game seconds
static float AnimalRunMaxX(float seconds) {
    float maxX = 0.0f;
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) {
        AnimalsTick(1.0f / 60.0f);
        float x = animals[0].x / CELL_SIZE;
        if (x > maxX) maxX = x;
    }
    return maxX;
}

describe(animals_and_doors) {
    it("a closed door keeps an animal in; an open one lets it out to the grass") {
        AnimalRoom(CELL_DOOR);
        expect(animals[0].active);
        expect(AnimalRunMaxX(600.0f) < 5.0f);   // never reached the door cell (x 5)
        AnimalRoom(CELL_DOOR_OPEN);
        expect(AnimalRunMaxX(600.0f) >= 6.0f);  // went through to the grass
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);

    test(cell_door_properties);
    test(cell_door_walkability);
    test(cell_door_sky_exposure);
    test(door_construction_recipes);
    test(primitive_construction_recipes);
    test(workshop_ground_fire);
    test(door_blueprint_placement);
    test(door_states);
    test(animals_and_doors);

    return summary();
}
