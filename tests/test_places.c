// Ownership and access (src/entities/places.c): groups, places with parents and rules, grants,
// and the soft rule in the need searches. docs/todo/architecture/ownership-and-access.md
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
#include "../src/entities/furniture.h"
#include "../src/entities/places.h"
#include "../src/simulation/plants.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/world/designations.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// A 10x10 floor at z=1 on solid ground, needs on, no movers
static void Setup(void) {
    InitTestGrid(10, 10);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
            SET_FLOOR(x, y, 1);
        }
    ClearMovers(); ClearItems(); ClearStockpiles(); ClearWorkshops(); ClearJobs(); ClearPlants();
    ClearFurniture();
    InitDesignations();
    InitBalance();
    ClearPlaces();
    hungerEnabled = true;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
}

static int AddMover(int cx, int cy) {
    int idx = moverCount++;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f, cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    return idx;
}

static int StoredFood(int sp, int x, int y) {
    int it = SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_BERRIES);
    PlaceItemInStockpile(sp, x, y, it);
    items[it].state = ITEM_IN_STOCKPILE;
    return it;
}

describe(places_rules) {
    it("no places: anyone may do anything anywhere") {
        Setup();
        int m = AddMover(1, 1);
        expect(PlaceAt(3, 3, 1) == 0);
        expect(MayEnter(m, 3, 3, 1) && MayUse(m, 3, 3, 1) && MayTake(m, 3, 3, 1));
        expect(OwnerAt(3, 3, 1) == 0);
    }

    it("a home is for its household; others may not enter, use or take") {
        Setup();
        int a = AddMover(1, 1), b = AddMover(8, 8);
        int hA = GroupCreate(GROUP_HOUSEHOLD, "Flat A");
        int home = PlaceCreate(hA, 0, PLACE_RULES_HOME, "Flat A");
        PlaceFillRect(home, 0, 0, 4, 4, 1);
        expect(MoverJoinGroup(a, hA));
        expect(MoverHousehold(a) == hA && MoverHousehold(b) == 0);
        expect(PlaceAt(2, 2, 1) == home && PlaceAt(5, 5, 1) == 0);
        expect(MayEnter(a, 2, 2, 1) && MayUse(a, 2, 2, 1) && MayTake(a, 2, 2, 1));
        expect(!MayEnter(b, 2, 2, 1) && !MayUse(b, 2, 2, 1) && !MayTake(b, 2, 2, 1));
        expect(MayUse(b, 5, 5, 1));                       // outside: public
        expect(OwnerAt(2, 2, 1) == hA);
        expect(strcmp(PlaceName(home), "Flat A") == 0 && strcmp(GroupName(hA), "Flat A") == 0);
    }

    it("a rule a place doesn't set is asked of its parent") {
        Setup();
        int a = AddMover(1, 1), b = AddMover(8, 8);
        int hA = GroupCreate(GROUP_HOUSEHOLD, "Flat A");
        MoverJoinGroup(a, hA);
        int home = PlaceCreate(hA, 0, PLACE_RULES_HOME, "Flat A");
        int hall = PlaceCreate(0, home, PLACE_RULE_ENTER, "Hall");   // the hall: decides only entering...
        places[hall].rules = 0;                                      // ... and anyone may
        PlaceFillRect(home, 0, 0, 4, 4, 1);
        PlaceFillRect(hall, 0, 0, 1, 4, 1);
        expect(MayEnter(b, 0, 2, 1));                     // hall: open
        expect(!MayUse(b, 0, 2, 1));                      // using: the home decides
        expect(!MayEnter(b, 3, 2, 1));                    // the rest of the home
        expect(OwnerAt(0, 2, 1) == hA);                   // the hall has no owner: the home's
    }

    it("a grant lets a guest into the place and the places inside it, until taken back") {
        Setup();
        int a = AddMover(1, 1), g = AddMover(8, 8);
        int hA = GroupCreate(GROUP_HOUSEHOLD, "Flat A");
        MoverJoinGroup(a, hA);
        int home = PlaceCreate(hA, 0, PLACE_RULES_HOME, "Flat A");
        int bedroom = PlaceCreate(hA, home, PLACE_RULES_HOME, "Bedroom");
        PlaceFillRect(home, 0, 0, 4, 4, 1);
        PlaceFillRect(bedroom, 3, 3, 4, 4, 1);
        GrantPlace(g, home);
        expect(MayUse(g, 1, 1, 1) && MayUse(g, 4, 4, 1));
        GrantPlace(g, 0);
        expect(!MayUse(g, 1, 1, 1));
    }

    it("a new mover in a reused slot belongs nowhere; a new world has no places") {
        Setup();
        int a = AddMover(1, 1);
        int hA = GroupCreate(GROUP_HOUSEHOLD, "Flat A");
        MoverJoinGroup(a, hA);
        GrantPlace(a, 1);
        Point goal = {1, 1, 1};
        InitMover(&movers[a], 1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        expect(MoverHousehold(a) == 0 && MoverGrant(a) == 0);
        int home = PlaceCreate(hA, 0, PLACE_RULES_HOME, "Flat A");
        PlaceFillRect(home, 0, 0, 4, 4, 1);
        InitTestGrid(10, 10);
        expect(PlaceAt(2, 2, 1) == 0 && !places[home].active);
    }
}

describe(places_needs) {
    it("a hungry mover leaves the neighbours' food alone and walks to its own") {
        Setup();
        int a = AddMover(4, 1);
        movers[a].hunger = 0.2f;
        movers[a].needSearchCooldown = 0.0f;
        int hA = GroupCreate(GROUP_HOUSEHOLD, "Flat A"), hB = GroupCreate(GROUP_HOUSEHOLD, "Flat B");
        MoverJoinGroup(a, hA);
        int homeA = PlaceCreate(hA, 0, PLACE_RULES_HOME, "Flat A");
        int homeB = PlaceCreate(hB, 0, PLACE_RULES_HOME, "Flat B");
        PlaceFillRect(homeA, 0, 6, 9, 9, 1);     // its own home is the far one
        PlaceFillRect(homeB, 0, 0, 9, 3, 1);     // the neighbours' is right here
        int spB = CreateStockpile(5, 1, 1, 1, 1), spA = CreateStockpile(5, 8, 1, 1, 1);
        int theirs = StoredFood(spB, 5, 1);
        int mine = StoredFood(spA, 5, 8);
        ProcessFreetimeNeeds();
        expect(movers[a].freetimeState == FREETIME_SEEKING_FOOD);
        expect(movers[a].needTarget == mine);
        expect(items[theirs].reservedBy == -1);
    }

    it("a guest may eat at the host's") {
        Setup();
        int g = AddMover(4, 1);
        movers[g].hunger = 0.2f;
        movers[g].needSearchCooldown = 0.0f;
        int hB = GroupCreate(GROUP_HOUSEHOLD, "Flat B");
        int homeB = PlaceCreate(hB, 0, PLACE_RULES_HOME, "Flat B");
        PlaceFillRect(homeB, 0, 0, 9, 3, 1);
        int sp = CreateStockpile(5, 1, 1, 1, 1);
        int food = StoredFood(sp, 5, 1);
        ProcessFreetimeNeeds();
        expect(movers[g].freetimeState != FREETIME_SEEKING_FOOD);   // not a guest: leaves it
        GrantPlace(g, homeB);
        movers[g].needSearchCooldown = 0.0f;
        ProcessFreetimeNeeds();
        expect(movers[g].needTarget == food);
    }
}

describe(places_save) {
    it("groups, places, memberships and the layer survive a save; grants don't") {
        Setup();
        int a = AddMover(1, 1);
        int hA = GroupCreate(GROUP_HOUSEHOLD, "Flat A");
        MoverJoinGroup(a, hA);
        int home = PlaceCreate(hA, 0, PLACE_RULES_HOME, "Flat A");
        PlaceFillRect(home, 2, 2, 4, 4, 1);
        PlaceSetDoor(home, 3, 5, 1);
        GrantPlace(a, home);
        expect(SaveWorld("/tmp/test_places_save.bin"));
        ClearPlaces();
        expect(LoadWorld("/tmp/test_places_save.bin"));
        expect(PlaceAt(3, 3, 1) == home && PlaceAt(1, 1, 1) == 0);
        expect(places[home].owner == hA && places[home].doorY == 5);
        expect(strcmp(GroupName(hA), "Flat A") == 0);
        expect(MoverHousehold(a) == hA);
        expect(HomeOfGroup(hA) == home);
        expect(MoverGrant(a) == 0);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    test(places_rules);
    test(places_needs);
    test(places_save);
    return summary();
}
