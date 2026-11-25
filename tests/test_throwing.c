// Throwing (src/entities/throwing.c, docs/todo/gameplay/throwing.md)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/throwing.h"
#include "../src/entities/animals.h"
#include "../src/simulation/balance.h"
#include "../src/core/state_audit.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <math.h>
#include <string.h>

static bool test_verbose = false;

// 32x32: dirt at z0, open air above (people stand at z1)
static void FlatWorld(void) {
    InitTestGrid(32, 32);
    ClearItems();
    ClearMovers();
    ClearJobs();
    ClearStockpiles();
    ClearThrows();
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, z) = z == 0 ? CELL_WALL : CELL_AIR;
                SetWallMaterial(x, y, z, z == 0 ? MAT_DIRT : MAT_NONE);
                CLEAR_FLOOR(x, y, z);
            }
}

static void Wall(int x, int y, int z) {
    GRID(x, y, z) = CELL_WALL;
    SetWallMaterial(x, y, z, MAT_GRANITE);
}

static int Spawn(int x, int y, int z, ItemType t) {
    return SpawnItemWithMaterial((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, (float)z, t, DefaultMaterialForItemType(t));
}

static void Settle(void) {
    for (int i = 0; i < 1200 && ThrowFlightCount() > 0; i++) ThrowsTick(1.0f / 60.0f);
}

static float DistToCell(int idx, int x, int y) {
    float dx = items[idx].x / CELL_SIZE - (x + 0.5f), dy = items[idx].y / CELL_SIZE - (y + 0.5f);
    return sqrtf(dx * dx + dy * dy);
}

static bool InsideWall(int idx) {
    int x = (int)(items[idx].x / CELL_SIZE), y = (int)(items[idx].y / CELL_SIZE), z = (int)items[idx].z;
    CellType c = GRID(x, y, z);
    return CellIsSolid(c) || CellBlocksMovement(c);
}

describe(throw_what) {
    it("throws small light things, not heavy or two-handed ones") {
        FlatWorld();
        expect(ItemThrowable(Spawn(2, 2, 1, ITEM_SHARP_STONE)));
        expect(ItemThrowable(Spawn(3, 2, 1, ITEM_STICKS)));
        expect(!ItemThrowable(Spawn(4, 2, 1, ITEM_LOG)));    // two-handed
        expect(!ItemThrowable(Spawn(5, 2, 1, ITEM_ROCK)));   // 25 kg, two-handed
        expect(!ItemThrowable(-1));
    }
    it("throws a stone further than a heavier thing") {
        FlatWorld();
        int stone = Spawn(2, 2, 1, ITEM_SHARP_STONE);
        int planks = Spawn(3, 2, 1, ITEM_PLANKS);
        expect(ThrowRange(stone) > ThrowRange(planks) * 2.0f);
    }
}

describe(throw_flight) {
    it("a perfect throw on open ground comes to rest on the goal") {
        FlatWorld();
        int s = Spawn(4, 10, 1, ITEM_SHARP_STONE);
        ThrowAim aim;
        expect(ThrowPlan(s, 4.5f, 10.5f, 1.7f, 12, 10, 1, 1.0f, &aim));
        ThrowLaunch(s, 4.5f, 10.5f, 1.7f, aim, false, -1);
        expect(items[s].state == ITEM_IN_FLIGHT);
        Settle();
        expect(items[s].state == ITEM_ON_GROUND);
        expect((int)items[s].z == 1);
        expect(DistToCell(s, 12, 10) <= 1.0f);
    }
    it("bounces off a wall: ends up on the thrower's side, never inside") {
        FlatWorld();
        for (int y = 0; y < 32; y++) { Wall(10, y, 1); Wall(10, y, 2); Wall(10, y, 3); }
        int s = Spawn(4, 10, 1, ITEM_SHARP_STONE);
        // Flat and hard at the wall (aim at a point behind it)
        ThrowAim aim = { 10.0f, 16.0f, 10.5f, 1.0f };
        ThrowLaunch(s, 4.5f, 10.5f, 1.7f, aim, false, -1);
        Settle();
        expect(throwBounceCount > 0);
        expect(items[s].state == ITEM_ON_GROUND);
        expect(items[s].x / CELL_SIZE < 10.0f);
        expect(!InsideWall(s));
    }
    it("lobs over a wall when a flat throw would hit it") {
        FlatWorld();
        for (int y = 0; y < 32; y++) { Wall(8, y, 1); Wall(8, y, 2); }
        int s = Spawn(4, 10, 1, ITEM_SHARP_STONE);
        ThrowAim aim;
        expect(ThrowPlan(s, 4.5f, 10.5f, 1.7f, 13, 10, 1, 1.0f, &aim));
        ThrowLaunch(s, 4.5f, 10.5f, 1.7f, aim, false, -1);
        Settle();
        expect(items[s].x / CELL_SIZE > 9.0f);
        expect(DistToCell(s, 13, 10) <= 1.0f);
    }
    it("can't plan a throw out of range; thrown anyway it falls short") {
        FlatWorld();
        int s = Spawn(1, 16, 1, ITEM_PLANKS);   // 5 kg: short range
        ThrowAim aim;
        expect(!ThrowPlan(s, 1.5f, 16.5f, 1.7f, 30, 16, 1, 1.0f, &aim));
        ThrowAim tooFar = { 45.0f, 30.5f, 16.5f, 1.0f };
        ThrowLaunch(s, 1.5f, 16.5f, 1.7f, tooFar, false, -1);
        Settle();
        expect(items[s].x / CELL_SIZE < 1.5f + ThrowRange(s) + 2.0f);
        expect(items[s].x / CELL_SIZE < 25.0f);
    }
    it("comes down onto a floor and off an edge to the ground below") {
        FlatWorld();
        // A deck at z3 (floor) over x 10..14: a throw landing past it falls to z1
        for (int y = 0; y < 32; y++) for (int x = 10; x <= 14; x++) SET_FLOOR(x, y, 3);
        int s = Spawn(4, 10, 1, ITEM_SHARP_STONE);
        ThrowAim aim = { 50.0f, 12.5f, 10.5f, 3.0f };
        ThrowLaunch(s, 4.5f, 10.5f, 1.7f, aim, false, -1);
        Settle();
        expect(items[s].state == ITEM_ON_GROUND);
        int x = (int)(items[s].x / CELL_SIZE);
        int z = (int)items[s].z;
        bool onDeck = x >= 10 && x <= 14;
        expect(onDeck ? (z == 3 || z == 1) : z == 1);
        expect(!InsideWall(s));
    }
    it("200 noisy throws in a walled room: all come to rest, none inside a wall") {
        FlatWorld();
        SetRandomSeed(7);
        for (int i = 0; i < 32; i++) { Wall(i, 4, 1); Wall(i, 27, 1); Wall(4, i, 1); Wall(27, i, 1); }
        for (int y = 10; y < 20; y++) { Wall(15, y, 1); Wall(15, y, 2); }   // a pillar row in the middle
        for (int x = 8; x < 12; x++) SET_FLOOR(x, 20, 3);                    // a bit of ceiling
        int s = Spawn(8, 8, 1, ITEM_SHARP_STONE);
        int inWall = 0, stillFlying = 0;
        for (int n = 0; n < 200; n++) {
            int gx = GetRandomValue(5, 26), gy = GetRandomValue(5, 26);
            float a = (float)GetRandomValue(5, 70);
            ThrowAim aim = { a, gx + 0.5f, gy + 0.5f, 1.0f };
            ThrowLaunch(s, 8.5f, 8.5f, 1.7f, aim, true, -1);
            Settle();
            if (items[s].state == ITEM_IN_FLIGHT) stillFlying++;
            if (InsideWall(s)) inWall++;
            items[s].x = 8.5f * CELL_SIZE; items[s].y = 8.5f * CELL_SIZE; items[s].z = 1.0f;
            items[s].state = ITEM_ON_GROUND;
        }
        expect(stillFlying == 0);
        expect(inWall == 0);
    }
    it("a load puts things in the air on the ground") {
        FlatWorld();
        int s = Spawn(4, 10, 1, ITEM_SHARP_STONE);
        items[s].state = ITEM_IN_FLIGHT;   // as saved mid-flight
        items[s].z = 3.0f;
        ThrowLandAll();
        expect(items[s].state == ITEM_ON_GROUND);
        expect((int)items[s].z == 1);
        expect(ThrowFlightCount() == 0);
    }
}

// One animal standing at (x, y) on level z, nothing else around
static int PlaceAnimal(int x, int y, int z, AnimalType type, AnimalBehavior behavior) {
    ClearAnimals();
    animalRespawnEnabled = false;
    Animal* a = &animals[0];
    memset(a, 0, sizeof(*a));
    a->x = (x + 0.5f) * CELL_SIZE; a->y = (y + 0.5f) * CELL_SIZE; a->z = (float)z;
    a->type = type; a->behavior = behavior; a->active = true;
    a->speed = type == ANIMAL_PREDATOR ? 80.0f : ANIMAL_SPEED;
    a->state = ANIMAL_GRAZING;
    a->targetCellX = x; a->targetCellY = y;
    a->targetAnimalIdx = -1; a->reservedByHunter = -1;
    animalCount = 1;
    return 0;
}

// Throw a stone perfectly from (4, 10) at cell (gx, 10) and let it settle
static int ThrowAtCell(int gx) {
    int s = Spawn(4, 10, 1, ITEM_SHARP_STONE);
    ThrowAim aim;
    if (!ThrowPlan(s, 4.5f, 10.5f, 1.7f, gx, 10, 1, 1.0f, &aim)) return -1;
    ThrowLaunch(s, 4.5f, 10.5f, 1.7f, aim, false, -1);
    Settle();
    return s;
}

static float AnimalX(int i) { return animals[i].x / CELL_SIZE; }

static void RunAnimals(float seconds) {
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) AnimalsTick(1.0f / 60.0f);
}

describe(throw_hit) {
    it("a stone hitting a grazer scares it: it runs away from where the stone came from") {
        FlatWorld();
        int a = PlaceAnimal(11, 10, 1, ANIMAL_GRAZER, BEHAVIOR_SIMPLE_GRAZER);
        int s = ThrowAtCell(11);
        expect(s >= 0);
        expect(throwHitCount == 1);
        expect(AnimalScared(a));
        expect(items[s].state == ITEM_ON_GROUND);
        expect(items[s].x / CELL_SIZE < 11.5f);   // bounced off it, didn't fly on through
        float x0 = AnimalX(a);
        RunAnimals(2.0f);
        expect(AnimalX(a) > x0 + 3.0f);           // away from the thrower (at x 4)
        RunAnimals(GameHoursToGameSeconds(animalScareHours));
        expect(!AnimalScared(a));                 // calms down
    }
    it("a stone that lands well short of an animal doesn't scare it") {
        FlatWorld();
        int a = PlaceAnimal(16, 10, 1, ANIMAL_GRAZER, BEHAVIOR_SIMPLE_GRAZER);
        expect(ThrowAtCell(9) >= 0);
        expect(throwHitCount == 0);
        expect(!AnimalScared(a));
    }
    it("a stone passing beside an animal doesn't hit it") {
        FlatWorld();
        int a = PlaceAnimal(11, 12, 1, ANIMAL_GRAZER, BEHAVIOR_SIMPLE_GRAZER);
        expect(ThrowAtCell(11) >= 0);
        expect(throwHitCount == 0);
        expect(!AnimalScared(a));
    }
    it("a steering grazer runs too") {
        FlatWorld();
        int a = PlaceAnimal(11, 10, 1, ANIMAL_GRAZER, BEHAVIOR_STEERING_GRAZER);
        expect(ThrowAtCell(11) >= 0);
        expect(AnimalScared(a));
        float x0 = AnimalX(a);
        RunAnimals(2.0f);
        expect(AnimalX(a) > x0 + 2.0f);
    }
    it("a predator hit stops hunting and runs") {
        FlatWorld();
        int p = PlaceAnimal(11, 10, 1, ANIMAL_PREDATOR, BEHAVIOR_PREDATOR);
        animals[p].state = ANIMAL_HUNTING;
        // prey on the thrower's side: unscared it would come toward it
        Animal* prey = &animals[1];
        *prey = animals[p];
        prey->type = ANIMAL_GRAZER; prey->behavior = BEHAVIOR_SIMPLE_GRAZER;
        prey->x = 7.5f * CELL_SIZE; prey->state = ANIMAL_BEING_HUNTED;   // stands still
        animalCount = 2;
        expect(ThrowAtCell(11) >= 0);
        expect(AnimalScared(p));
        float x0 = AnimalX(p);
        RunAnimals(2.0f);
        expect(AnimalX(p) > x0 + 2.0f);
        expect(animals[1].active);   // not caught
    }
}

describe(throw_job) {
    it("a mover fetches a stone and throws it at the goal") {
        InitTestGridFromAscii(
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n");
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers(); ClearItems(); ClearJobs(); ClearStockpiles(); ClearThrows();
        Point g = { 1, 3, 0 };
        InitMover(&movers[0], 1.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, g, MOVER_SPEED);
        moverCount = 1;
        int stone = Spawn(3, 3, 0, ITEM_SHARP_STONE);
        const char* why = "";
        int job = ThrowOrder(0, -1, 12, 3, 0, &why);
        expect(job >= 0);
        expect(items[stone].reservedBy == 0);
        bool thrown = false;
        for (int t = 0; t < 2000; t++) {
            Tick();
            JobsTick();
            ThrowsTick(gameDeltaTime);
            if (items[stone].state == ITEM_IN_FLIGHT) thrown = true;
            if (thrown && ThrowFlightCount() == 0) break;
        }
        expect(thrown);
        expect(items[stone].state == ITEM_ON_GROUND);
        expect(items[stone].reservedBy == -1);
        expect(movers[0].currentJobId == -1);
        expect(DistToCell(stone, 12, 3) < 4.0f);
        expect(RunStateAudit(false) == 0);
    }
    it("throws one stone of a stack, the rest stays") {
        InitTestGridFromAscii(
            "....................\n"
            "....................\n"
            "....................\n");
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers(); ClearItems(); ClearJobs(); ClearStockpiles(); ClearThrows();
        Point g = { 1, 1, 0 };
        InitMover(&movers[0], 1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, g, MOVER_SPEED);
        moverCount = 1;
        int pile = Spawn(2, 1, 0, ITEM_SHARP_STONE);
        items[pile].stackCount = 5;
        int job = ThrowOrder(0, -1, 10, 1, 0, NULL);
        expect(job >= 0);
        int thrownIdx = jobs[job].targetItem;
        expect(thrownIdx != pile);
        expect(items[pile].stackCount == 4);
        expect(items[thrownIdx].stackCount == 1);
        expect(items[pile].reservedBy == -1);
    }
    it("refuses when there is nothing light enough") {
        InitTestGridFromAscii("..........\n..........\n");
        ClearMovers(); ClearItems(); ClearJobs(); ClearThrows();
        Point g = { 1, 1, 0 };
        InitMover(&movers[0], 1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, g, MOVER_SPEED);
        moverCount = 1;
        Spawn(3, 1, 0, ITEM_LOG);
        const char* why = "";
        expect(ThrowOrder(0, -1, 8, 1, 0, &why) < 0);
        expect(movers[0].currentJobId == -1);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    InitJobPool();
    test(throw_what);
    test(throw_flight);
    test(throw_hit);
    test(throw_job);
    return summary();
}
