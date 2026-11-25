// Explosions (src/simulation/explosion.c, docs/todo/world/explosions.md)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/simulation/explosion.h"
#include "../src/core/state_audit.h"
#include "../src/world/designations.h"
#include "../src/core/effects.h"
#include "../src/simulation/water.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/vapour.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/rooms.h"
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
    ClearFire();   // earlier tests leave fires: they'd light the vapour
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

describe(blast_designation) {
    it("marks walls the blast can break, not air, bedrock or trees, and not twice") {
        FlatWorld();
        Wall(5, 5, 1, MAT_GRANITE);
        Wall(6, 5, 1, MAT_BEDROCK);
        GRID(7, 5, 1) = CELL_TREE_TRUNK;
        expect(DesignateBlast(5, 5, 1));
        expect(HasBlastDesignation(5, 5, 1));
        expect(!DesignateBlast(5, 5, 1));       // already marked
        expect(!DesignateBlast(8, 8, 1));       // air
        expect(!DesignateBlast(6, 5, 1));       // bedrock
        expect(!DesignateBlast(7, 5, 1));       // tree
        expect(CountBlastDesignations() == 1);
        CancelDesignation(5, 5, 1);
        expect(CountBlastDesignations() == 0);
    }

    it("doesn't take a wall someone already marked for something else") {
        FlatWorld();
        Wall(5, 5, 1, MAT_GRANITE);
        expect(DesignateMine(5, 5, 1));
        expect(!DesignateBlast(5, 5, 1));
        CancelDesignation(5, 5, 1);
    }
}

static int CountGunpowder(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == ITEM_GUNPOWDER) n++;
    return n;
}

describe(gunpowder_fire) {
    it("goes off a moment after a fire reaches it, and is gone") {
        FlatWorld();
        ClearFire();
        Wall(14, 12, 1, MAT_OAK);
        int gp = Spawn(12, 12, 1, ITEM_GUNPOWDER);
        SetFireLevel(12, 12, 1, FIRE_MAX_LEVEL);
        ExplosionsTick(0.3f);                      // the look: lit, a short fizz
        expect(!items[gp].active);                 // (nothing spawned yet to reuse the slot)
        expect(ExplosionFuseCount() == 1);
        expect(GRID(14, 12, 1) == CELL_WALL);      // not yet
        for (int i = 0; i < 90; i++) ExplosionsTick(1.0f / 60.0f);
        expect(ExplosionFuseCount() == 0);
        expect(GRID(14, 12, 1) == CELL_AIR);       // the bang broke the oak wall 2 cells away
        ClearFire();
    }

    it("makes a bigger bang from a bigger pile") {
        expect(GunpowderPower(1) == blastPower);
        expect(GunpowderPower(8) > 1.99f * blastPower && GunpowderPower(8) < 2.01f * blastPower);
        expect(GunpowderPower(100000) == EXPLOSION_MAX_POWER);
    }

    it("goes off next to a fire only by a spark's chance") {
        FlatWorld();
        ClearFire();
        int gp = Spawn(12, 12, 1, ITEM_GUNPOWDER);
        SetFireLevel(13, 12, 1, FIRE_MAX_LEVEL);
        float saved = gunpowderSparkChance;
        gunpowderSparkChance = 0.0f;
        for (int i = 0; i < 10; i++) ExplosionsTick(0.3f);
        expect(items[gp].active);
        gunpowderSparkChance = 1.0f;
        ExplosionsTick(0.3f);
        expect(!items[gp].active);
        gunpowderSparkChance = saved;
        ClearFire();
    }

    it("is set off by a blast that reaches it, not by one that doesn't") {
        FlatWorld();
        ClearFire();
        int nearGp = Spawn(15, 12, 1, ITEM_GUNPOWDER);   // 3 cells away
        int farGp = Spawn(22, 22, 1, ITEM_GUNPOWDER);    // out of reach
        ExplosionResult r = Explode(12, 12, 1, 5.0f);
        (void)nearGp;
        expect(r.powderSetOff == 1);
        expect(CountGunpowder() == 1);                  // the far one (rubble may reuse the near one's slot)
        expect(items[farGp].active && items[farGp].type == ITEM_GUNPOWDER);
        expect(ExplosionFuseCount() == 1);              // the chained one, a moment later
        for (int i = 0; i < 60; i++) ExplosionsTick(1.0f / 60.0f);
        expect(ExplosionFuseCount() == 0);
        expect(CountGunpowder() == 1);
    }
}

describe(blast_effects) {
    it("leaves a fireball, dust and chunks that age away, none when switched off") {
        FlatWorld();
        Wall(14, 12, 1, MAT_OAK);
        Explode(12, 12, 1, 5.0f);
        int blasts = 0, chunks = 0, dust = 0;
        for (int i = 0; i < fxCount; i++) {
            if (fx[i].kind == FX_BLAST) blasts++;
            if (fx[i].kind == FX_CHUNK) chunks++;
            if (fx[i].kind == FX_DUST) dust++;
        }
        expect(blasts >= 4);                       // a lumpy cluster, not one disc
        expect(chunks > 0);
        expect(dust > 0);
        int smoke = 0;
        for (int i = 0; i < fxCount; i++) if (fx[i].kind == FX_SMOKE) smoke++;
        expect(smoke >= 10);                       // a cloud, not a wisp
        for (int i = 0; i < 6 * 60; i++) FxTick(1.0f / 60.0f);   // the smoke lingers a few seconds
        expect(FxCount() == 0);

        expect(!FxHitStopFrame());                 // power 5: no hit stop
        Explode(12, 12, 1, 9.0f);
        expect(FxHitStopFrame());                  // a big one holds the picture a few frames
        ClearExplosions();
        expect(!FxHitStopFrame());
        effectsEnabled = false;
        Explode(12, 12, 1, 5.0f);
        expect(FxCount() == 0);
        effectsEnabled = true;
    }

    it("sparks for as long as a fuse burns") {
        FlatWorld();
        ExplosionLightFuse(12, 12, 1, 5.0f, 1.0f);
        for (int i = 0; i < 30; i++) { FxTick(1.0f / 60.0f); ExplosionsTick(1.0f / 60.0f); }
        int sparks = 0;
        for (int i = 0; i < fxCount; i++) if (fx[i].kind == FX_SPARK) sparks++;
        expect(sparks > 0);
    }
}

describe(explosion_water) {
    it("throws the water of a pond out over its bank, none made or lost") {
        FlatWorld();
        ClearWater();
        for (int y = 8; y <= 16; y++)
            for (int x = 8; x <= 16; x++) { GRID(x, y, 0) = CELL_AIR; SetWallMaterial(x, y, 0, MAT_NONE); SetWaterLevel(x, y, 0, WATER_MAX_LEVEL); }
        int total = 0, pond = 0, after = 0, out = 0;
        for (int z = 0; z < gridDepth; z++) for (int y = 0; y < gridHeight; y++) for (int x = 0; x < gridWidth; x++) total += GetWaterLevel(x, y, z);
        ExplosionResult r = Explode(12, 12, 1, 5.0f);
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) {
                    int w = GetWaterLevel(x, y, z);
                    after += w;
                    if (z == 0 && x >= 8 && x <= 16 && y >= 8 && y <= 16) pond += w; else out += w;
                }
        expect(r.waterThrown > 0);
        expect(after == total);
        expect(GetWaterLevel(12, 12, 0) == 0);
        expect(out > 0);
        expect(pond < total);
    }

    it("lets a pond into the tunnel when it breaks the wall between them") {
        FlatWorld();
        ClearWater();
        for (int y = 4; y <= 20; y++) for (int x = 4; x <= 20; x++) Wall(x, y, 1, MAT_GRANITE);
        for (int y = 6; y <= 18; y++) for (int x = 6; x <= 10; x++) { GRID(x, y, 1) = CELL_AIR; SetWaterLevel(x, y, 1, WATER_MAX_LEVEL); }
        for (int x = 13; x <= 20; x++) GRID(x, 12, 1) = CELL_AIR;   // a tunnel, 2 cells of granite from the pond
        Explode(12, 12, 1, 5.0f);                                    // drilled into the wall's tunnel face
        expect(GRID(11, 12, 1) == CELL_AIR);
        for (int i = 0; i < 300; i++) UpdateWater();
        int tunnel = 0;
        for (int x = 13; x <= 20; x++) tunnel += GetWaterLevel(x, 12, 1);
        expect(tunnel > 0);
    }
}

describe(explosion_crater) {
    it("leaves a crater with ramps up its sides and a scorched floor") {
        FlatWorld();
        for (int z = 1; z <= 2; z++) for (int y = 0; y < gridHeight; y++) for (int x = 0; x < gridWidth; x++) { Wall(x, y, z, MAT_DIRT); SetWallNatural(x, y, z); }
        ExplosionResult r = Explode(12, 12, 3, 5.0f);
        expect(r.cellsBroken > 0);
        expect(r.rampsMade > 0);
        int ramps = 0, scorched = 0;
        for (int z = 0; z <= 2; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++) {
                    if (CellIsDirectionalRamp(GRID(x, y, z))) { ramps++; expect(CanWalkUpRampAt(x, y, z)); }
                    if (HAS_CELL_FLAG(x, y, z, CELL_FLAG_BURNED)) scorched++;
                }
        expect(ramps == r.rampsMade);
        expect(scorched > 0);
    }

    it("makes no ramp under a roof (in a pocket in rock only the steps inside it)") {
        FlatWorld();
        for (int z = 1; z <= 6; z++) for (int y = 0; y < gridHeight; y++) for (int x = 0; x < gridWidth; x++) { Wall(x, y, z, MAT_GRANITE); SetWallNatural(x, y, z); }
        ExplosionResult r = Explode(12, 12, 3, 5.0f);
        expect(r.cellsBroken > 0);
        for (int z = 0; z <= 6; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    if (CellIsDirectionalRamp(GRID(x, y, z))) { expect(!CellIsSolid(GRID(x, y, z + 1))); expect(CanWalkUpRampAt(x, y, z)); }
    }
}

describe(blast_kinds) {
    it("lets dynamite shatter more rock than gunpowder of the same power, with less smoke") {
        int broken[2], smoke[2];
        for (int k = 0; k < 2; k++) {
            FlatWorld();
            ClearSmoke();
            for (int z = 1; z <= 6; z++) for (int y = 0; y < gridHeight; y++) for (int x = 0; x < gridWidth; x++) { Wall(x, y, z, MAT_GRANITE); SetWallNatural(x, y, z); }
            for (int z = 7; z < gridDepth; z++) for (int y = 0; y < gridHeight; y++) for (int x = 0; x < gridWidth; x++) GRID(x, y, z) = CELL_AIR;
            Wall(12, 12, 6, MAT_NONE); GRID(12, 12, 6) = CELL_AIR;   // a shaft so there's open air to smoke
            ExplosionResult r = ExplodeKind(12, 12, 4, 5.0f, k == 0 ? BLAST_GUNPOWDER : BLAST_DYNAMITE);
            broken[k] = r.cellsBroken;
            smoke[k] = 0;
            for (int z = 0; z < gridDepth; z++) for (int y = 0; y < gridHeight; y++) for (int x = 0; x < gridWidth; x++) smoke[k] += GetSmokeLevel(x, y, z);
        }
        expect(broken[1] > broken[0] * 2);
        expect(smoke[1] < smoke[0] || smoke[0] == 0);
    }

    it("goes off as its own kind from a fuse") {
        FlatWorld();
        for (int z = 1; z <= 4; z++) for (int y = 6; y <= 18; y++) for (int x = 6; x <= 18; x++) { Wall(x, y, z, MAT_GRANITE); SetWallNatural(x, y, z); }
        ExplosionLightFuseKind(12, 12, 2, 5.0f, 0.5f, BLAST_DYNAMITE);
        for (int i = 0; i < 60; i++) ExplosionsTick(1.0f / 60.0f);
        int open = 0;
        for (int z = 1; z <= 4; z++) for (int y = 6; y <= 18; y++) for (int x = 6; x <= 18; x++) if (GRID(x, y, z) == CELL_AIR) open++;
        expect(ExplosionCount() == 1);
        expect(open > 7);   // gunpowder at 5 takes 7 cells of granite
    }
}

static void RunVapour(float seconds) {
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) VapourTick(1.0f / 60.0f);
}

describe(fuel_vapour) {
    it("sinks into a pit and spreads, made from a leak, then thins out") {
        FlatWorld();
        ClearVapour();
        for (int y = 10; y <= 14; y++) for (int x = 14; x <= 18; x++) GRID(x, y, 0) = CELL_AIR;   // a pit
        VapourLeak(11, 12, 1, 20.0f, 2.0f);
        RunVapour(3.5f);
        int pit = 0;
        for (int y = 10; y <= 14; y++) for (int x = 14; x <= 18; x++) pit += GetVapourLevel(x, y, 0);
        expect(VapourTotal() > 0);
        expect(VapourCellCount() > 5);
        expect(pit > 0);
        expect(GetVapourLevel(11, 12, 2) == 0);   // never rises
        RunVapour(120.0f);
        expect(VapourTotal() < 20);
    }

    it("lights from a spark and the flame burns the whole connected cloud") {
        FlatWorld();
        ClearVapour();
        for (int y = 8; y <= 16; y++) for (int x = 4; x <= 20; x++) AddVapour(x, y, 1, 2);
        int before = VapourCellCount();
        expect(VapourIgnite(4, 8, 1));
        RunVapour(3.0f);
        expect(VapourBurnedCells() >= before);   // and what spread meanwhile
        expect(VapourCellCount() == 0);
        expect(VapourBurningCount() == 0);
    }

    it("doesn't burn past a wall the cloud doesn't reach around") {
        FlatWorld();
        ClearVapour();
        vapourThinChance = 0.0f;
        for (int y = 0; y < gridHeight; y++) for (int z = 1; z <= 3; z++) Wall(12, y, z, MAT_GRANITE);
        for (int y = 8; y <= 16; y++) { for (int x = 4; x <= 10; x++) AddVapour(x, y, 1, 2); for (int x = 14; x <= 20; x++) AddVapour(x, y, 1, 2); }
        VapourIgnite(4, 8, 1);
        RunVapour(0.5f);   // burns before the clouds move far
        int east = 0;
        for (int y = 8; y <= 16; y++) for (int x = 14; x <= 20; x++) east += GetVapourLevel(x, y, 1);
        expect(east > 0);
        vapourThinChance = 0.03f;
    }

    it("is lit by a fire beside it, a blast and a fuse's sparks, and pushes whoever stands in it") {
        FlatWorld();
        ClearVapour();
        for (int y = 8; y <= 16; y++) for (int x = 4; x <= 12; x++) AddVapour(x, y, 1, 3);
        int m = AddMover(8, 12, 1);
        Wall(13, 12, 1, MAT_OAK);
        IgniteCell(13, 12, 1);
        RunVapour(1.5f);
        expect(VapourBurnedCells() > 50);
        expect(MoverKnocked(m));
        FlatWorld();
        ClearVapour();
        for (int y = 8; y <= 16; y++) for (int x = 4; x <= 12; x++) AddVapour(x, y, 1, 3);
        Explode(8, 12, 1, 2.0f);
        RunVapour(1.5f);
        expect(VapourCellCount() == 0);
        FlatWorld();
        ClearVapour();
        for (int y = 8; y <= 16; y++) for (int x = 4; x <= 12; x++) AddVapour(x, y, 1, 3);
        ExplosionLightFuse(8, 12, 1, 2.0f, 5.0f);
        ExplosionsTick(1.0f / 60.0f);
        RunVapour(1.5f);
        expect(VapourBurnedCells() > 50);
    }
}

// A closed room x 6..15, y 6..13 on level 1 (walls of m, a roof of planks), a window east if asked
static void GasBox(MaterialType m, bool window) {
    for (int y = 6; y <= 13; y++)
        for (int x = 6; x <= 15; x++) {
            if (x == 6 || x == 15 || y == 6 || y == 13) Wall(x, y, 1, m);
            SET_FLOOR(x, y, 2); SetFloorMaterial(x, y, 2, MAT_OAK);
        }
    if (window) { GRID(15, 9, 1) = CELL_WINDOW; SetWallMaterial(15, 9, 1, MAT_GLASS); }
    InvalidateRooms();
    UpdateRooms();
}

describe(vapour_rooms) {
    it("blows the window out of a closed room full of burning vapour, not the brick") {
        FlatWorld();
        ClearVapour();
        GasBox(MAT_BRICK, true);
        expect(GetRoomAt(10, 10, 1) != ROOM_NONE);
        for (int y = 7; y <= 12; y++) for (int x = 7; x <= 14; x++) AddVapour(x, y, 1, 4);
        int m = AddMover(17, 9, 1);   // outside the window
        VapourIgnite(8, 8, 1);
        RunVapour(2.0f);
        expect(VapourVentCount() >= 1);
        expect(GRID(15, 9, 1) != CELL_WINDOW);
        expect(MoverKnocked(m));
        int brick = 0;
        for (int y = 6; y <= 13; y++) for (int x = 6; x <= 15; x++) if (GRID(x, y, 1) == CELL_WALL && GetWallMaterial(x, y, 1) == MAT_BRICK) brick++;
        expect(brick == 31);   // 32 edge cells less the window
    }

    it("holds when there's little vapour, and outdoors nothing builds up") {
        FlatWorld();
        ClearVapour();
        GasBox(MAT_BRICK, true);
        for (int x = 7; x <= 9; x++) AddVapour(x, 8, 1, 1);
        VapourIgnite(8, 8, 1);
        RunVapour(2.0f);
        expect(VapourVentCount() == 0);
        expect(GRID(15, 9, 1) == CELL_WINDOW);
        FlatWorld();
        ClearVapour();
        for (int y = 7; y <= 12; y++) for (int x = 7; x <= 14; x++) AddVapour(x, y, 1, 4);
        GRID(15, 9, 1) = CELL_WINDOW; SetWallMaterial(15, 9, 1, MAT_GLASS);
        InvalidateRooms();
        UpdateRooms();
        VapourIgnite(8, 8, 1);
        RunVapour(2.0f);
        expect(VapourVentCount() == 0);
    }

    it("tears a weak wall out when the pressure beats its strength") {
        FlatWorld();
        ClearVapour();
        GasBox(MAT_DIRT, false);   // no window, no door: the weakest is the dirt (2)
        for (int y = 7; y <= 12; y++) for (int x = 7; x <= 14; x++) AddVapour(x, y, 1, 7);
        VapourIgnite(8, 8, 1);
        RunVapour(2.0f);
        expect(VapourVentCount() >= 1);
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
    test(blast_designation);
    test(gunpowder_fire);
    test(blast_effects);
    test(explosion_water);
    test(explosion_crater);
    test(blast_kinds);
    test(fuel_vapour);
    test(vapour_rooms);
    return summary();
}
