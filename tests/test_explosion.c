// Explosions (src/simulation/explosion.c, docs/todo/world/explosions.md)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/simulation/explosion.h"
#include "../src/core/state_audit.h"
#include "test_helpers.h"
#include <math.h>

static bool test_verbose = false;

// 24x24: dirt at z0, open air above (people stand at z1)
static void FlatWorld(void) {
    InitTestGrid(24, 24);
    ClearItems();
    ClearMovers();
    ClearStockpiles();
    ClearExplosions();
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, z) = z == 0 ? CELL_WALL : CELL_AIR;
                SetWallMaterial(x, y, z, z == 0 ? MAT_DIRT : MAT_NONE);
                CLEAR_FLOOR(x, y, z);
            }
}

static void Wall(int x, int y, int z, MaterialType m) {
    GRID(x, y, z) = CELL_WALL;
    SetWallMaterial(x, y, z, m);
}

static int Spawn(int x, int y, int z, ItemType t) {
    return SpawnItemWithMaterial((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, (float)z, t, DefaultMaterialForItemType(t));
}

static float Moved(int idx, int x, int y) {
    float dx = items[idx].x - (x + 0.5f) * CELL_SIZE, dy = items[idx].y - (y + 0.5f) * CELL_SIZE;
    return sqrtf(dx * dx + dy * dy) / CELL_SIZE;
}

static void Settle(void) {
    for (int i = 0; i < 600 && ExplosionFlyingCount() > 0; i++) ExplosionsTick(1.0f / 60.0f);
}

static int AddMover(int x, int y, int z) {
    int i = moverCount++;
    Point goal = { x, y, z };
    InitMover(&movers[i], (x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, (float)z, goal, 100.0f);
    return i;
}

describe(explosion_strength) {
    it("rates rock above dirt, bedrock and trees unbreakable, air open") {
        FlatWorld();
        Wall(5, 5, 1, MAT_GRANITE);
        Wall(6, 5, 1, MAT_BEDROCK);
        GRID(7, 5, 1) = CELL_TREE_TRUNK;
        expect(ExplosionCellStrength(5, 5, 1) > ExplosionCellStrength(5, 5, 0));
        expect(ExplosionCellStrength(6, 5, 1) < 0.0f);
        expect(ExplosionCellStrength(7, 5, 1) < 0.0f);
        expect(ExplosionCellStrength(8, 8, 1) == 0.0f);
    }
}

describe(explosion_items) {
    it("throws light things further than heavy ones") {
        FlatWorld();
        int leaves = Spawn(9, 12, 1, ITEM_LEAVES);
        int rock = Spawn(15, 12, 1, ITEM_ROCK);
        ExplosionResult r = Explode(12, 12, 1, 6.0f);
        expect(r.itemsPushed >= 2);
        Settle();
        expect(ExplosionFlyingCount() == 0);
        expect(Moved(leaves, 9, 12) > Moved(rock, 15, 12) + 1.0f);
        expect(items[leaves].x < (9 + 0.5f) * CELL_SIZE);   // away from the charge (west)
        expect(items[rock].x >= (15 + 0.5f) * CELL_SIZE);   // east
    }

    it("leaves things behind a granite wall where they are") {
        FlatWorld();
        for (int x = 4; x <= 20; x++) { Wall(x, 9, 1, MAT_GRANITE); Wall(x, 9, 2, MAT_GRANITE); }
        int leaves = Spawn(12, 7, 1, ITEM_LEAVES);
        Explode(12, 12, 1, 6.0f);   // the wall 3 cells away: 3 power left, granite takes 4
        Settle();
        expect(Moved(leaves, 12, 7) < 0.01f);
        expect(GRID(12, 9, 1) == CELL_WALL);
    }

    it("takes a stored item out of its stockpile cleanly") {
        FlatWorld();
        int sp = CreateStockpile(13, 12, 1, 2, 1);
        expect(sp >= 0);
        int sticks = Spawn(13, 12, 1, ITEM_STICKS);
        PlaceItemInStockpile(sp, 13, 12, sticks);
        expect(items[sticks].state == ITEM_IN_STOCKPILE);
        Explode(12, 12, 1, 6.0f);
        Settle();
        expect(items[sticks].state == ITEM_ON_GROUND);
        expect(Moved(sticks, 13, 12) > 1.0f);
        int v = RunStateAudit(false);
        if (v != 0) RunStateAudit(true);
        expect(v == 0);
    }
}

describe(explosion_walls) {
    it("breaks a wooden wall close by, holds a granite one") {
        FlatWorld();
        Wall(14, 12, 1, MAT_OAK);      // 2 cells east
        Wall(10, 12, 1, MAT_GRANITE);  // 2 cells west
        ExplosionResult r = Explode(12, 12, 1, 5.0f);
        expect(GRID(14, 12, 1) == CELL_AIR);
        expect(GRID(10, 12, 1) == CELL_WALL);
        expect(r.cellsBroken >= 1);
    }

    it("blasts a pocket out of rock with a drilled-in charge, drops included") {
        FlatWorld();
        for (int z = 1; z <= 5; z++)
            for (int y = 6; y <= 18; y++)
                for (int x = 6; x <= 18; x++) Wall(x, y, z, MAT_GRANITE);
        int before = itemCount;
        ExplosionResult r = Explode(12, 12, 3, 8.0f);
        expect(GRID(12, 12, 3) == CELL_AIR);
        expect(GRID(13, 12, 3) == CELL_AIR);
        expect(GRID(12, 12, 4) == CELL_AIR);
        expect(GRID(15, 12, 3) == CELL_WALL);   // not further than its power reaches
        expect(r.cellsBroken >= 15);
        expect(itemCount > before);
    }

    it("never breaks bedrock") {
        FlatWorld();
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) Wall(x, y, 0, MAT_BEDROCK);
        Explode(12, 12, 1, 20.0f);
        expect(GRID(12, 12, 0) == CELL_WALL);
    }

    it("breaks a built floor over air and drops what lay on it") {
        FlatWorld();
        for (int y = 10; y <= 14; y++)
            for (int x = 10; x <= 14; x++) { SET_FLOOR(x, y, 3); SetFloorMaterial(x, y, 3, MAT_OAK); }
        int sticks = Spawn(12, 12, 3, ITEM_STICKS);
        ExplosionResult r = Explode(12, 12, 2, 6.0f);   // under the floor
        expect(r.floorsBroken >= 1);
        expect(!HAS_FLOOR(12, 12, 3));
        Settle();
        expect((int)items[sticks].z < 3);
    }

    it("leaves no floor hanging over the hole it digs") {
        FlatWorld();
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) Wall(x, y, 1, MAT_DIRT);
        Explode(12, 12, 2, 6.0f);   // on dirt two deep
        expect(GRID(12, 12, 1) == CELL_AIR);
        if (GRID(12, 12, 0) != CELL_WALL) expect(!HAS_FLOOR(12, 12, 1));
    }
}

describe(explosion_movers) {
    it("knocks a mover in the open back, not one behind a wall") {
        FlatWorld();
        for (int x = 4; x <= 20; x++) { Wall(x, 9, 1, MAT_GRANITE); Wall(x, 9, 2, MAT_GRANITE); }
        int open = AddMover(14, 13, 1);
        int sheltered = AddMover(12, 7, 1);
        float x0 = movers[open].x;
        ExplosionResult r = Explode(12, 12, 1, 6.0f);
        expect(r.moversKnocked == 1);
        expect(MoverKnocked(open));
        expect(!MoverKnocked(sheltered));
        for (int i = 0; i < 300 && MoverKnocked(open); i++) ExplosionMoverTick(open, 1.0f / 60.0f);
        expect(!MoverKnocked(open));
        expect(movers[open].x > x0 + 0.5f * CELL_SIZE);   // pushed east, away from the charge
        expect(movers[open].needsRepath);
    }
}

describe(explosion_fuse) {
    it("goes off when the fuse has burned down") {
        FlatWorld();
        Wall(13, 12, 1, MAT_OAK);
        expect(ExplosionLightFuse(12, 12, 1, 5.0f, 1.0f));
        for (int i = 0; i < 30; i++) ExplosionsTick(1.0f / 60.0f);
        expect(GRID(13, 12, 1) == CELL_WALL);
        expect(ExplosionFuseCount() == 1);
        for (int i = 0; i < 40; i++) ExplosionsTick(1.0f / 60.0f);
        expect(ExplosionFuseCount() == 0);
        expect(GRID(13, 12, 1) == CELL_AIR);
    }
}

int main(int argc, char** argv) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(explosion_strength);
    test(explosion_items);
    test(explosion_walls);
    test(explosion_movers);
    test(explosion_fuse);
    return summary();
}
