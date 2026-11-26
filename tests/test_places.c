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
#include "../src/entities/visit.h"
#include "../src/entities/chase.h"
#include "../src/entities/activity.h"
#include "../src/entities/relations.h"
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

// A walled home (x 0..4, y 0..4) with its front door at (2,4) opening south; its household has
// one member inside. Returns the home place; *host, *guest are set (the guest stands outside).
static int VisitWorld(int* host, int* guest) {
    Setup();
    hungerEnabled = false; thirstEnabled = false; energyEnabled = false; bladderEnabled = false;
    for (int y = 0; y <= 4; y++)
        for (int x = 0; x <= 4; x++)
            if (x == 0 || x == 4 || y == 0 || y == 4) { GRID(x, y, 1) = CELL_WALL; SetWallMaterial(x, y, 1, MAT_BRICK); }
    GRID(2, 4, 1) = CELL_DOOR;
    BuildEntrances(); BuildGraph();
    int hh = GroupCreate(GROUP_HOUSEHOLD, "Flat B");
    int home = PlaceCreate(hh, 0, PLACE_RULES_HOME, "Flat B");
    PlaceFillRect(home, 0, 0, 4, 4, 1);
    PlaceFillRect(0, 2, 4, 2, 4, 1);          // the threshold is public
    PlaceSetDoor(home, 2, 4, 1);
    *host = AddMover(2, 2);
    *guest = AddMover(7, 8);
    movers[*host].speed = movers[*guest].speed = MOVER_SPEED;
    MoverJoinGroup(*host, hh);
    ClearVisits();
    visitStayMinHours = visitStayMaxHours = 0.2f;
    visitCloseChance = 1.0f;
    return home;
}

static void VisitRun(int maxTicks, long* doneCounter) {
    for (int t = 0; t < maxTicks && *doneCounter == 0; t++) {
        Tick();
        ChaseTick(TICK_DT);
        VisitTick(TICK_DT);
    }
}

describe(places_visit) {
    it("a guest knocks, is let in (a grant), stays, leaves; the door is shut behind it, the grant gone") {
        int host, guest;
        int home = VisitWorld(&host, &guest);
        expect(VisitStart(guest, host) >= 0);
        expect(MoverIsVisiting(guest));
        VisitRun(20000, &visitDone);
        expect(visitAnswered == 1 && visitDone == 1 && visitGivenUp == 0);
        expect(GRID(2, 4, 1) == CELL_DOOR);                 // closed behind it (visitCloseChance 1)
        expect(MoverGrant(guest) == 0);
        int gx = (int)(movers[guest].x / CELL_SIZE), gy = (int)(movers[guest].y / CELL_SIZE);
        expect(PlaceAt(gx, gy, 1) != home);                 // outside
        expect(RelationLike(guest, host) > 0.0f);           // they got on a little
        visitCloseChance = 0.5f; visitStayMinHours = 1.0f; visitStayMaxHours = 2.5f;
    }

    it("nobody home: the guest knocks a while and gives up") {
        int host, guest;
        VisitWorld(&host, &guest);
        movers[host].x = 8.5f * CELL_SIZE; movers[host].y = 1.5f * CELL_SIZE;   // out
        expect(VisitStart(guest, host) >= 0);
        VisitRun(40000, &visitGivenUp);
        expect(visitGivenUp == 1 && visitAnswered == 0);
        expect(MoverGrant(guest) == 0 && !MoverIsVisiting(guest));
        visitCloseChance = 0.5f; visitStayMinHours = 1.0f; visitStayMaxHours = 2.5f;
    }

    it("no visit to one's own household or a household without a home") {
        int host, guest;
        VisitWorld(&host, &guest);
        int other = AddMover(8, 2);
        expect(VisitStart(guest, other) < 0);               // other has no household
        MoverJoinGroup(guest, MoverHousehold(host));
        expect(VisitStart(guest, host) < 0);                // same household
    }
}

// ---------------------------------------------------------------------------
// Stress: full tables, bad input, deep and broken parent chains, a random world checked
// against a plain reference, save/load of a full world
// ---------------------------------------------------------------------------

static unsigned int stressSeed = 12345u;
static int StressRand(int n) {
    stressSeed = stressSeed * 1103515245u + 12345u;
    return (int)((stressSeed >> 8) % (unsigned)n);
}

// The rules written out plainly: walk up from the innermost place, the first that decides the
// rule says yes (open) or members / grant holders only. No depth guard: the test worlds are trees.
static bool RefGrantCovers(int mover, int p) {
    int g = moverGrant[mover];
    for (int q = p; q > 0; q = places[q].parent) if (g > 0 && q == g) return true;
    return false;
}
static bool RefAllows(int mover, int x, int y, int z, uint8_t rule) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return true;
    for (int p = PLACE_AT(x, y, z); p > 0; p = places[p].parent) {
        if (!(places[p].rulesSet & rule)) continue;
        if (!(places[p].rules & rule)) return true;
        bool member = places[p].owner == 0;
        for (int k = 0; k < MAX_MOVER_GROUPS; k++) if (moverGroups[mover][k] == places[p].owner) member = true;
        return member || RefGrantCovers(mover, p);
    }
    return true;
}

describe(places_stress) {
    it("full tables say no (-1) and nothing breaks; bad ids and coordinates are refused") {
        Setup();
        int made = 0;
        while (GroupCreate(GROUP_HOUSEHOLD, "g") > 0) made++;
        expect(made == MAX_GROUPS - 1);
        expect(GroupCreate(GROUP_PERSON, "one more") == -1);
        int placesMade = 0;
        while (PlaceCreate(1, 0, PLACE_RULES_HOME, "p") > 0) placesMade++;
        expect(placesMade == MAX_PLACES - 1);
        expect(PlaceCreate(1, 0, PLACE_RULES_HOME, "one more") == -1);
        expect(PlaceCreate(-1, 0, 0, "bad owner") == -1);
        expect(PlaceCreate(MAX_GROUPS, 0, 0, "bad owner") == -1);
        expect(PlaceCreate(1, MAX_PLACES, 0, "bad parent") == -1);
        // Off the map / out of range: no crash, nothing written, the answer is "anyone may"
        PlaceFillRect(1, -5, -5, 20, 20, 1);          // clipped to the map
        PlaceFillRect(1, 0, 0, 3, 3, -1);
        PlaceFillRect(1, 0, 0, 3, 3, gridDepth);
        PlaceFillRect(MAX_PLACES, 0, 0, 3, 3, 1);
        expect(PlaceAt(0, 0, 1) == 1 && PlaceAt(9, 9, 1) == 1);
        expect(PlaceAt(-1, 0, 1) == 0 && PlaceAt(10, 0, 1) == 0 && PlaceAt(0, 0, gridDepth) == 0);
        expect(MayEnter(0, -1, -1, 1) && MayTake(0, 99, 99, 1));
        // A mover index out of range is nobody: kept out of private places, no crash
        expect(!MayEnter(-1, 2, 2, 1) && !MayEnter(MAX_MOVERS, 2, 2, 1));
        expect(!MoverJoinGroup(-1, 1) && !MoverJoinGroup(0, 0) && !MoverJoinGroup(0, MAX_GROUPS));
        GrantPlace(-1, 1); GrantPlace(MAX_MOVERS, 1); GrantPlace(0, MAX_PLACES);
        expect(MoverHousehold(-1) == 0 && MoverHousehold(MAX_MOVERS) == 0);
        expect(strcmp(PlaceName(MAX_PLACES), "") == 0 && strcmp(GroupName(-3), "") == 0);
    }

    it("a mover is in at most MAX_MOVER_GROUPS groups; leaving frees a slot") {
        Setup();
        int a = AddMover(1, 1);
        int g[MAX_MOVER_GROUPS + 1];
        for (int k = 0; k <= MAX_MOVER_GROUPS; k++) g[k] = GroupCreate(GROUP_BUSINESS, "biz");
        bool joined = true;
        for (int k = 0; k < MAX_MOVER_GROUPS; k++) joined = MoverJoinGroup(a, g[k]) && joined;
        expect(joined);
        expect(MoverJoinGroup(a, g[0]));                      // already in: fine
        expect(!MoverJoinGroup(a, g[MAX_MOVER_GROUPS]));      // full
        MoverLeaveGroup(a, g[1]);
        expect(MoverJoinGroup(a, g[MAX_MOVER_GROUPS]) && MoverInGroup(a, g[MAX_MOVER_GROUPS]));
        expect(!MoverInGroup(a, g[1]));
    }

    it("a place needs a live owner and a live parent (no cycles through a not-yet-made parent)") {
        Setup();
        int hh = GroupCreate(GROUP_HOUSEHOLD, "Flat");
        expect(PlaceCreate(hh + 1, 0, PLACE_RULES_HOME, "nobody's") == -1);   // no such group
        int a = PlaceCreate(hh, 2, PLACE_RULES_HOME, "inside a place that isn't there");
        expect(a == -1);
        int outer = PlaceCreate(hh, 0, PLACE_RULES_HOME, "outer");
        int inner = PlaceCreate(hh, outer, 0, "inner");
        expect(outer > 0 && inner > 0 && places[inner].parent == outer);
    }

    it("deep nesting: a private rule far up the chain still holds") {
        Setup();
        int a = AddMover(1, 1), b = AddMover(2, 1);
        int hh = GroupCreate(GROUP_HOUSEHOLD, "Deep");
        MoverJoinGroup(a, hh);
        int p = PlaceCreate(hh, 0, PLACE_RULES_HOME, "root");
        int depth = 1, made;
        while ((made = PlaceCreate(0, p, 0, "room")) > 0) { p = made; depth++; if (depth >= 200) break; }
        PlaceFillRect(p, 5, 5, 5, 5, 1);
        // Either the chain was cut off at a depth the queries walk in full, or the rule holds
        expect(MayEnter(a, 5, 5, 1));
        expect(!MayEnter(b, 5, 5, 1));
        expect(OwnerAt(5, 5, 1) == hh);
        GrantPlace(b, places[p].parent > 0 ? 1 : p);         // a grant for the root covers the whole chain
        expect(MayEnter(b, 5, 5, 1));
    }

    it("a random world of nested places agrees with the plain rules everywhere") {
        Setup();
        InitTestGrid(64, 64);
        ClearMovers(); ClearPlaces();
        stressSeed = 777u;
        int nGroups = 40;
        for (int g = 0; g < nGroups; g++) GroupCreate((GroupKind)(1 + StressRand(GROUP_KIND_COUNT - 1)), "g");
        int nMovers = 200;
        for (int i = 0; i < nMovers; i++) {
            moverCount = i + 1;
            PlacesResetMover(i);
            int joins = StressRand(MAX_MOVER_GROUPS + 1);
            for (int k = 0; k < joins; k++) MoverJoinGroup(i, 1 + StressRand(nGroups));
        }
        // 600 places: each a rect inside its parent's rect (or anywhere for a root), random rules
        int badIds = 0;
        static int px0[MAX_PLACES], py0[MAX_PLACES], px1[MAX_PLACES], py1[MAX_PLACES];
        for (int n = 0; n < 600; n++) {
            int parent = (n > 0 && StressRand(3) > 0) ? 1 + StressRand(n) : 0;
            int x0, y0, x1, y1;
            if (parent > 0) {
                int w = px1[parent] - px0[parent] + 1, h = py1[parent] - py0[parent] + 1;
                x0 = px0[parent] + StressRand(w); y0 = py0[parent] + StressRand(h);
                x1 = x0 + StressRand(px1[parent] - x0 + 1); y1 = y0 + StressRand(py1[parent] - y0 + 1);
            } else {
                x0 = StressRand(60); y0 = StressRand(60); x1 = x0 + StressRand(20); y1 = y0 + StressRand(20);
                if (x1 > 63) x1 = 63;
                if (y1 > 63) y1 = 63;
            }
            int owner = StressRand(4) == 0 ? 0 : 1 + StressRand(nGroups);
            int p = PlaceCreate(owner, parent, (uint8_t)StressRand(8), "p");
            if (p != n + 1) badIds++;
            places[p].rulesSet = (uint8_t)(places[p].rules | StressRand(8));   // some rules decided open
            px0[p] = x0; py0[p] = y0; px1[p] = x1; py1[p] = y1;
            // Paint only where the parent is (a child never sticks out of its parent) or anywhere for a root
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    int here = PlaceAt(x, y, 1);
                    bool inParent = parent == 0 || here == parent;
                    for (int q = here; !inParent && q > 0; q = places[q].parent) if (q == parent) inParent = true;
                    if (inParent && (parent > 0 || here == 0)) PLACE_AT_MUT(x, y, 1) = (uint16_t)p;
                }
        }
        for (int i = 0; i < nMovers; i++) if (StressRand(4) == 0) GrantPlace(i, 1 + StressRand(600));
        long checked = 0, mismatches = 0;
        static const uint8_t rules[3] = { PLACE_RULE_ENTER, PLACE_RULE_USE, PLACE_RULE_TAKE };
        for (int i = 0; i < nMovers; i += 3)
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++)
                    for (int r = 0; r < 3; r++) {
                        bool want = RefAllows(i, x, y, 1, rules[r]);
                        bool got = r == 0 ? MayEnter(i, x, y, 1) : r == 1 ? MayUse(i, x, y, 1) : MayTake(i, x, y, 1);
                        checked++;
                        if (want != got) mismatches++;
                    }
        if (test_verbose) printf("  random world: %ld answers checked, %ld mismatches\n", checked, mismatches);
        expect(mismatches == 0 && badIds == 0);
    }

    it("a full world (every group and place, 1000 members, a 128x128 layer) survives save/load") {
        Setup();
        InitTestGrid(128, 128);
        ClearMovers(); ClearPlaces();
        int badIds = 0;
        for (int g = 1; g < MAX_GROUPS; g++) GroupCreate(GROUP_HOUSEHOLD, "household with a long name to clip");
        for (int p = 1; p < MAX_PLACES; p++) {
            if (PlaceCreate(1 + (p % (MAX_GROUPS - 1)), p > 1 && p % 4 ? p - 1 : 0, PLACE_RULES_HOME, "p") != p) badIds++;
            PlaceFillRect(p, (p * 7) % 128, (p * 13) % 128, (p * 7) % 128, (p * 13) % 128, 1 + p % 2);
        }
        for (int i = 0; i < 1000; i++) {
            Point goal = {1, 1, 1};
            InitMover(&movers[i], 1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 1.0f, goal, 100.0f);
            moverCount = i + 1;
            MoverJoinGroup(i, 1 + i % (MAX_GROUPS - 1));
            MoverJoinGroup(i, 1 + (i * 31) % (MAX_GROUPS - 1));
        }
        static bool before[1000][3];
        for (int i = 0; i < 1000; i++) {
            int p = 1 + i % (MAX_PLACES - 1), x = (p * 7) % 128, y = (p * 13) % 128, z = 1 + p % 2;
            before[i][0] = MayEnter(i, x, y, z); before[i][1] = MayUse(i, x, y, z); before[i][2] = OwnerAt(x, y, z) > 0;
        }
        expect(SaveWorld("/tmp/test_places_full.bin"));
        ClearPlaces();
        expect(LoadWorld("/tmp/test_places_full.bin"));
        int diffs = 0;
        for (int i = 0; i < 1000; i++) {
            int p = 1 + i % (MAX_PLACES - 1), x = (p * 7) % 128, y = (p * 13) % 128, z = 1 + p % 2;
            if (before[i][0] != MayEnter(i, x, y, z) || before[i][1] != MayUse(i, x, y, z) || before[i][2] != (OwnerAt(x, y, z) > 0)) diffs++;
        }
        expect(diffs == 0 && badIds == 0);
        expect(GroupCreate(GROUP_PERSON, "x") == -1 && PlaceCreate(0, 0, 0, "x") == -1);   // still full
        expect(strlen(GroupName(1)) == PLACE_NAME_LEN - 1);
    }

    it("a new map size clears the layer") {
        Setup();
        int hh = GroupCreate(GROUP_HOUSEHOLD, "Flat");
        int home = PlaceCreate(hh, 0, PLACE_RULES_HOME, "Flat");
        PlaceFillRect(home, 0, 0, 9, 9, 1);
        InitTestGrid(40, 20);
        expect(PlaceAt(5, 5, 1) == 0 && PlaceAt(39, 19, 1) == 0);
    }
}

// ---------------------------------------------------------------------------
// Home first: the furniture in a place, and searches at home looking there first
// ---------------------------------------------------------------------------

describe(places_home_first) {
    it("a place lists the furniture in it and in the places inside it, kept up to date") {
        Setup();
        int hh = GroupCreate(GROUP_HOUSEHOLD, "Flat");
        int home = PlaceCreate(hh, 0, PLACE_RULES_HOME, "Flat");
        int room = PlaceCreate(hh, home, 0, "Bedroom");
        PlaceFillRect(home, 0, 0, 4, 4, 1);
        PlaceFillRect(room, 0, 0, 1, 1, 1);
        int bed = SpawnFurniture(0, 0, 1, FURNITURE_PLANK_BED, MAT_OAK);
        int tap = SpawnFurniture(3, 3, 1, FURNITURE_TAP, MAT_GRANITE);
        int outside = SpawnFurniture(8, 8, 1, FURNITURE_CHAIR, MAT_OAK);
        const int* list;
        int n = PlaceFurniture(home, &list);
        bool inHome = n == 2 && list[0] == bed && list[1] == tap;
        n = PlaceFurniture(room, &list);
        bool inRoom = n == 1 && list[0] == bed;
        RemoveFurniture(bed);
        n = PlaceFurniture(home, &list);
        bool afterRemove = n == 1 && list[0] == tap;
        PlaceFillRect(home, 8, 8, 8, 8, 1);                // the chair's cell joins the home
        n = PlaceFurniture(home, &list);
        bool afterFill = n == 2 && list[0] == tap && list[1] == outside;
        expect(inHome && inRoom && afterRemove && afterFill);
        expect(PlaceContains(home, 1, 1, 1) && PlaceContains(home, 4, 4, 1) && !PlaceContains(room, 4, 4, 1));
    }

    it("at home a tired resident takes its own bed, even with a public bed nearer; away, the nearest") {
        Setup();
        energyEnabled = true;
        int a = AddMover(4, 2);
        int hh = GroupCreate(GROUP_HOUSEHOLD, "Flat");
        MoverJoinGroup(a, hh);
        int home = PlaceCreate(hh, 0, PLACE_RULES_HOME, "Flat");
        PlaceFillRect(home, 0, 0, 4, 9, 1);
        PlaceSetDoor(home, 2, 9, 1);
        int own = SpawnFurniture(0, 9, 1, FURNITURE_PLANK_BED, MAT_OAK);
        int pub = SpawnFurniture(5, 2, 1, FURNITURE_PLANK_BED, MAT_OAK);   // public, one step outside
        movers[a].hunger = 1.0f;
        movers[a].energy = 0.1f;
        movers[a].needSearchCooldown = 0.0f;
        ProcessFreetimeNeeds();
        bool atHome = movers[a].freetimeState == FREETIME_SEEKING_REST && movers[a].needTarget == own;
        // The same mover outside its home: the nearest bed it may use
        ReleaseFurniture(own, a);
        movers[a].freetimeState = FREETIME_NONE;
        movers[a].x = 8.5f * CELL_SIZE; movers[a].y = 2.5f * CELL_SIZE;
        movers[a].needSearchCooldown = 0.0f;
        ProcessFreetimeNeeds();
        bool away = movers[a].freetimeState == FREETIME_SEEKING_REST && movers[a].needTarget == pub;
        expect(atHome && away);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    test(places_rules);
    test(places_needs);
    test(places_save);
    test(places_visit);
    test(places_stress);
    test(places_home_first);
    return summary();
}
