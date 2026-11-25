// Structural support (src/world/support.c, docs/todo/world/explosions.md)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/designations.h"
#include "../src/world/support.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/simulation/explosion.h"
#include "test_helpers.h"

static bool test_verbose = false;

// 24x24: dirt at z0, open air above
static void FlatWorld(void) {
    InitTestGrid(24, 24);
    ClearItems();
    ClearMovers();
    ClearStockpiles();
    ClearExplosions();
    supportEnabled = true;
    collapseWarning = 2.0f;
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

// Run the support tick for some game seconds
static void Run(float seconds) {
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) SupportTick(1.0f / 60.0f);
}

describe(support_grounded) {
    it("finds nothing unsupported in a world standing on the ground") {
        FlatWorld();
        for (int z = 1; z <= 5; z++) Wall(12, 12, z, MAT_GRANITE);
        for (int x = 13; x <= 16; x++) Wall(x, 12, 5, MAT_GRANITE);   // an arm off the top
        expect(SupportCountUnsupported() == 0);
        expect(SupportIsGrounded(16, 12, 5));
    }

    it("sees a floor hanging off a wall as held, a floor on its own as not") {
        FlatWorld();
        for (int z = 1; z <= 3; z++) Wall(5, 5, z, MAT_OAK);
        for (int x = 6; x <= 9; x++) SET_FLOOR(x, 5, 4);   // deck off the post's top
        SET_FLOOR(15, 15, 3);                               // a floor in the air
        expect(SupportIsGrounded(9, 5, 4));
        expect(!SupportIsGrounded(15, 15, 3));
        expect(SupportCountUnsupported() == 1);
    }
}

describe(support_collapse) {
    it("drops a pillar that lost its foot onto the ground, as walls, after the warning") {
        FlatWorld();
        for (int z = 1; z <= 5; z++) Wall(12, 12, z, MAT_GRANITE);
        CompleteMineDesignation(12, 12, 1);   // notes the change (MarkChunkDirty)
        Run(0.1f);
        expect(SupportPendingCount() == 1);
        expect(GRID(12, 12, 5) == CELL_WALL);   // still up: the warning
        Run(2.5f);
        expect(SupportPendingCount() == 0);
        for (int z = 1; z <= 4; z++) { expect(GRID(12, 12, z) == CELL_WALL); }
        expect(GRID(12, 12, 5) == CELL_AIR);
        expect(GetWallMaterial(12, 12, 1) == MAT_GRANITE);
        expect(SupportCountUnsupported() == 0);
    }

    it("holds if propped up before the warning runs out") {
        FlatWorld();
        for (int z = 1; z <= 5; z++) Wall(12, 12, z, MAT_GRANITE);
        CompleteMineDesignation(12, 12, 1);
        Run(0.5f);
        expect(SupportPendingCount() == 1);
        Wall(12, 12, 1, MAT_GRANITE);
        CLEAR_FLOOR(12, 12, 1);
        Run(2.5f);
        expect(GRID(12, 12, 5) == CELL_WALL);
    }

    it("keeps a bridge standing while one pillar holds it") {
        FlatWorld();
        for (int z = 1; z <= 3; z++) { Wall(6, 12, z, MAT_GRANITE); Wall(14, 12, z, MAT_GRANITE); }
        for (int x = 7; x <= 13; x++) Wall(x, 12, 3, MAT_GRANITE);
        CompleteMineDesignation(6, 12, 1);
        Run(3.0f);
        expect(GRID(6, 12, 3) == CELL_WALL);
        expect(GRID(10, 12, 3) == CELL_WALL);
        expect(SupportPendingCount() == 0);
    }

    it("breaks a deck that lost its post into rubble, and what lay on it comes down") {
        FlatWorld();
        for (int z = 1; z <= 3; z++) Wall(5, 5, z, MAT_OAK);
        for (int x = 6; x <= 9; x++) { SET_FLOOR(x, 5, 4); SetFloorMaterial(x, 5, 4, MAT_OAK); }
        int log = SpawnItemWithMaterial(8.5f * CELL_SIZE, 5.5f * CELL_SIZE, 4.0f, ITEM_LOG, MAT_OAK);
        int before = itemCount;
        CompleteMineDesignation(5, 5, 1);
        CompleteMineDesignation(5, 5, 2);
        Run(3.0f);
        for (int x = 6; x <= 9; x++) { expect(!HAS_FLOOR(x, 5, 4)); }
        expect(GRID(5, 5, 3) == CELL_AIR);       // the post's top fell ...
        expect(GRID(5, 5, 1) == CELL_WALL);      // ... to the ground
        expect((int)items[log].z == 1);
        expect(itemCount > before);              // planks from the deck
    }

    it("pushes someone a falling slab lands on aside, and dazes them") {
        FlatWorld();
        Wall(16, 12, 1, MAT_GRANITE); Wall(16, 12, 2, MAT_GRANITE);       // the only pillar
        for (int x = 8; x <= 16; x++) for (int y = 11; y <= 13; y++) Wall(x, y, 3, MAT_GRANITE);
        int m = moverCount++;
        Point goal = { 10, 12, 1 };
        InitMover(&movers[m], 10.5f * CELL_SIZE, 12.5f * CELL_SIZE, 1.0f, goal, 100.0f);
        CompleteMineDesignation(16, 12, 1);
        Run(2.5f);
        expect(GRID(10, 12, 1) == CELL_WALL);    // the slab came down
        int mx = (int)(movers[m].x / CELL_SIZE), my = (int)(movers[m].y / CELL_SIZE);
        expect(!CellIsSolid(GRID(mx, my, (int)movers[m].z)));
        expect(MoverKnocked(m));
    }

    it("settles a new world: a floating lump drops into place at once, without a warning") {
        FlatWorld();
        Wall(5, 5, 4, MAT_DIRT); Wall(6, 5, 4, MAT_DIRT); Wall(6, 5, 5, MAT_DIRT);   // hanging in the air
        for (int z = 1; z <= 3; z++) Wall(12, 12, z, MAT_GRANITE);                    // standing: stays
        expect(SupportCountUnsupported() == 3);
        expect(SupportSettleWorld() == 3);
        expect(SupportCountUnsupported() == 0);
        expect(SupportPendingCount() == 0);
        expect(GRID(5, 5, 1) == CELL_WALL);
        expect(GRID(6, 5, 1) == CELL_WALL);
        expect(GRID(6, 5, 2) == CELL_WALL);
        expect(GRID(6, 5, 5) == CELL_AIR);
        expect(GRID(12, 12, 3) == CELL_WALL);
    }

    it("does nothing when switched off") {
        FlatWorld();
        supportEnabled = false;
        for (int z = 1; z <= 5; z++) Wall(12, 12, z, MAT_GRANITE);
        CompleteMineDesignation(12, 12, 1);
        Run(3.0f);
        expect(GRID(12, 12, 5) == CELL_WALL);
        supportEnabled = true;
    }
}

int main(int argc, char** argv) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(support_grounded);
    test(support_collapse);
    return summary();
}
