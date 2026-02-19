#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/entities/furniture.h"
#include "../src/entities/jobs.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

static void Setup(void) {
    InitTestGrid(16, 16);
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearFurniture();
}

describe(furniture_blocking) {
    it("a placed bed slows movement but does not block") {
        Setup();
        // Cell (5,5,0) should be walkable before placing furniture
        expect(IsCellWalkableAt(0, 5, 5) == true);
        int baseCost = GetCellMoveCost(5, 5, 0);

        int fi = SpawnFurniture(5, 5, 0, FURNITURE_PLANK_BED, 0);
        expect(fi >= 0);
        expect(furniture[fi].active == true);
        expect(furniture[fi].type == FURNITURE_PLANK_BED);

        // Bed is non-blocking — still walkable but more expensive
        expect(IsCellWalkableAt(0, 5, 5) == true);
        expect(GetCellMoveCost(5, 5, 0) == 12);
        expect(GetCellMoveCost(5, 5, 0) > baseCost);
    }

    it("a placed leaf pile slows movement but does not block") {
        Setup();
        expect(IsCellWalkableAt(0, 5, 5) == true);
        int baseCost = GetCellMoveCost(5, 5, 0);

        int fi = SpawnFurniture(5, 5, 0, FURNITURE_LEAF_PILE, 0);
        expect(fi >= 0);

        // Leaf pile is non-blocking — still walkable but more expensive
        expect(IsCellWalkableAt(0, 5, 5) == true);
        expect(GetCellMoveCost(5, 5, 0) == 12);
        expect(GetCellMoveCost(5, 5, 0) > baseCost);
    }

    it("removing furniture restores the cell to normal") {
        Setup();

        // Test bed furniture (non-blocking with move cost)
        int bed = SpawnFurniture(5, 5, 0, FURNITURE_PLANK_BED, 0);
        expect(IsCellWalkableAt(0, 5, 5) == true);
        expect(GetCellMoveCost(5, 5, 0) == 12);
        RemoveFurniture(bed);
        expect(GetCellMoveCost(5, 5, 0) < 12);

        // Test non-blocking furniture
        int pile = SpawnFurniture(6, 6, 0, FURNITURE_LEAF_PILE, 0);
        expect(GetCellMoveCost(6, 6, 0) == 12);
        RemoveFurniture(pile);
        expect(GetCellMoveCost(6, 6, 0) < 12);
    }
}

describe(furniture_placement) {
    it("only one piece of furniture can occupy a cell") {
        Setup();
        int fi1 = SpawnFurniture(5, 5, 0, FURNITURE_LEAF_PILE, 0);
        expect(fi1 >= 0);

        int fi2 = SpawnFurniture(5, 5, 0, FURNITURE_CHAIR, 0);
        expect(fi2 == -1);  // rejected — cell occupied

        expect(furnitureCount == 1);
    }

    it("GetFurnitureAt finds the right furniture") {
        Setup();
        int a = SpawnFurniture(3, 3, 0, FURNITURE_LEAF_PILE, 0);
        int b = SpawnFurniture(7, 7, 0, FURNITURE_PLANK_BED, 0);
        int c = SpawnFurniture(10, 10, 0, FURNITURE_CHAIR, 0);

        expect(GetFurnitureAt(3, 3, 0) == a);
        expect(GetFurnitureAt(7, 7, 0) == b);
        expect(GetFurnitureAt(10, 10, 0) == c);
        expect(GetFurnitureAt(0, 0, 0) == -1);  // empty cell
    }

    it("removing furniture decrements the count") {
        Setup();
        SpawnFurniture(3, 3, 0, FURNITURE_LEAF_PILE, 0);
        SpawnFurniture(7, 7, 0, FURNITURE_PLANK_BED, 0);
        expect(furnitureCount == 2);

        RemoveFurniture(GetFurnitureAt(3, 3, 0));
        expect(furnitureCount == 1);
        expect(GetFurnitureAt(3, 3, 0) == -1);
        expect(GetFurnitureAt(7, 7, 0) >= 0);
    }
}

describe(furniture_occupant) {
    it("a mover reserves furniture and releases it") {
        Setup();
        int fi = SpawnFurniture(5, 5, 0, FURNITURE_PLANK_BED, 0);
        expect(furniture[fi].occupant == -1);

        // Reserve
        furniture[fi].occupant = 0;
        expect(furniture[fi].occupant == 0);

        // Release
        ReleaseFurniture(fi, 0);
        expect(furniture[fi].occupant == -1);
    }

    it("ReleaseFurniture only releases if mover matches") {
        Setup();
        int fi = SpawnFurniture(5, 5, 0, FURNITURE_PLANK_BED, 0);
        furniture[fi].occupant = 3;

        // Wrong mover — should not release
        ReleaseFurniture(fi, 7);
        expect(furniture[fi].occupant == 3);

        // Correct mover — releases
        ReleaseFurniture(fi, 3);
        expect(furniture[fi].occupant == -1);
    }

    it("ReleaseFurnitureForMover clears all reservations for a deleted mover") {
        Setup();
        int a = SpawnFurniture(3, 3, 0, FURNITURE_LEAF_PILE, 0);
        int b = SpawnFurniture(7, 7, 0, FURNITURE_CHAIR, 0);
        int c = SpawnFurniture(10, 10, 0, FURNITURE_PLANK_BED, 0);

        furniture[a].occupant = 2;
        furniture[b].occupant = 2;
        furniture[c].occupant = 5;  // different mover

        ReleaseFurnitureForMover(2);

        expect(furniture[a].occupant == -1);
        expect(furniture[b].occupant == -1);
        expect(furniture[c].occupant == 5);  // untouched
    }
}

describe(furniture_defs) {
    it("furniture defs have correct properties") {
        Setup();
        const FurnitureDef* bed = GetFurnitureDef(FURNITURE_PLANK_BED);
        expect(bed->blocking == false);
        expect(bed->moveCost == 12);
        expect(bed->restRate > 0.0f);

        const FurnitureDef* pile = GetFurnitureDef(FURNITURE_LEAF_PILE);
        expect(pile->blocking == false);
        expect(pile->moveCost == 12);
        expect(pile->restRate > 0.0f);

        const FurnitureDef* chair = GetFurnitureDef(FURNITURE_CHAIR);
        expect(chair->blocking == false);
        expect(chair->moveCost == 11);
        expect(chair->restRate > 0.0f);

        // Bed has highest rest rate
        expect(bed->restRate > pile->restRate);
        expect(pile->restRate > chair->restRate);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) test_verbose = true;
        if (strcmp(argv[i], "-q") == 0) set_quiet_mode(1);
    }

    test(furniture_blocking);
    test(furniture_placement);
    test(furniture_occupant);
    test(furniture_defs);

    return summary();
}
