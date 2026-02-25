// tests/test_clothing.c - Clothing & textiles system tests
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/designations.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/entities/tool_quality.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/plants.h"
#include "../src/entities/furniture.h"
#include "../src/core/time.h"
#include "../src/game_state.h"
#include "test_helpers.h"
#include <string.h>

// Setup a standard test grid: solid dirt at z=0, walkable air at z=1
static void SetupClothingGrid(void) {
    InitTestGrid(16, 16);
    ClearMovers();
    ClearItems();
    ClearJobs();
    ClearStockpiles();
    ClearWorkshops();
    ClearPlants();
    ClearFurniture();
    InitDesignations();
    InitBalance();
    hungerEnabled = false;
    energyEnabled = false;
    bodyTempEnabled = true;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    daysPerSeason = 7;
    dayNumber = 8;

    // Fill z=0 with solid dirt
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
        }
    }
    // z=1 is walkable air above solid ground
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[1][y][x] = CELL_AIR;
            exploredGrid[1][y][x] = 1;
            exploredGrid[0][y][x] = 1;
        }
    }
}

static int SetupMover(int cx, int cy) {
    int idx = moverCount;
    moverCount = idx + 1;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f,
              cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    movers[idx].capabilities.canHaul = true;
    return idx;
}

// Set ambient temperature for the entire grid at z=1
static void SetAmbientTemp(int tempC) {
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            SetTemperature(x, y, 1, tempC);
}

describe(clothing) {
    // =========================================================================
    // 1. Item flag and cooling reduction values
    // =========================================================================
    it("clothing items have IF_CLOTHING flag") {
        expect(ItemIsClothing(ITEM_GRASS_TUNIC));
        expect(ItemIsClothing(ITEM_FLAX_TUNIC));
        expect(ItemIsClothing(ITEM_LEATHER_VEST));
        expect(ItemIsClothing(ITEM_LEATHER_COAT));
    }

    it("non-clothing items lack IF_CLOTHING flag") {
        expect(!ItemIsClothing(ITEM_CLOTH));
        expect(!ItemIsClothing(ITEM_LINEN));
        expect(!ItemIsClothing(ITEM_LEATHER));
        expect(!ItemIsClothing(ITEM_LOG));
        expect(!ItemIsClothing(ITEM_ROCK));
    }

    it("GetClothingCoolingReduction returns correct values") {
        float eps = 0.01f;
        expect(GetClothingCoolingReduction(ITEM_GRASS_TUNIC) > 0.25f - eps &&
               GetClothingCoolingReduction(ITEM_GRASS_TUNIC) < 0.25f + eps);
        expect(GetClothingCoolingReduction(ITEM_FLAX_TUNIC) > 0.40f - eps &&
               GetClothingCoolingReduction(ITEM_FLAX_TUNIC) < 0.40f + eps);
        expect(GetClothingCoolingReduction(ITEM_LEATHER_VEST) > 0.50f - eps &&
               GetClothingCoolingReduction(ITEM_LEATHER_VEST) < 0.50f + eps);
        expect(GetClothingCoolingReduction(ITEM_LEATHER_COAT) > 0.65f - eps &&
               GetClothingCoolingReduction(ITEM_LEATHER_COAT) < 0.65f + eps);
    }

    it("GetClothingCoolingReduction returns 0 for non-clothing") {
        expect(GetClothingCoolingReduction(ITEM_CLOTH) == 0.0f);
        expect(GetClothingCoolingReduction(ITEM_LOG) == 0.0f);
        expect(GetClothingCoolingReduction(ITEM_ROCK) == 0.0f);
    }

    // =========================================================================
    // 2. Clothing items are non-stackable
    // =========================================================================
    it("clothing items have maxStack 1") {
        expect(ItemMaxStack(ITEM_GRASS_TUNIC) == 1);
        expect(ItemMaxStack(ITEM_FLAX_TUNIC) == 1);
        expect(ItemMaxStack(ITEM_LEATHER_VEST) == 1);
        expect(ItemMaxStack(ITEM_LEATHER_COAT) == 1);
    }

    // =========================================================================
    // 3. Material items are stackable
    // =========================================================================
    it("textile materials are stackable") {
        expect(ItemIsStackable(ITEM_CLOTH));
        expect(ItemIsStackable(ITEM_LINEN));
        expect(ItemIsStackable(ITEM_LEATHER));
    }

    // =========================================================================
    // 4. Mover equippedClothing initializes to -1
    // =========================================================================
    it("mover initializes with equippedClothing = -1") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        expect(movers[mi].equippedClothing == -1);
    }

    // =========================================================================
    // 5. WorkGiver_EquipClothing creates job for unclothed mover
    // =========================================================================
    it("WorkGiver creates equip job when clothing available") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        // Spawn clothing on ground nearby
        int clothIdx = SpawnItem(6 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_GRASS_TUNIC);
        expect(clothIdx >= 0);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId >= 0);

        if (jobId >= 0) {
            Job* job = GetJob(jobId);
            expect(job != NULL);
            expect(job->type == JOBTYPE_EQUIP_CLOTHING);
            expect(job->targetItem == clothIdx);
            expect(job->assignedMover == mi);
            expect(movers[mi].currentJobId == jobId);
            expect(items[clothIdx].reservedBy == mi);
        }

        FreeJobSystem();
    }

    // =========================================================================
    // 6. WorkGiver_EquipClothing picks best clothing (highest reduction)
    // =========================================================================
    it("WorkGiver prefers highest cooling reduction") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        // Spawn weak and strong clothing equidistant
        SpawnItem(6 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_GRASS_TUNIC);
        int coatIdx = SpawnItem(4 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_LEATHER_COAT);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId >= 0);

        if (jobId >= 0) {
            Job* job = GetJob(jobId);
            expect(job->targetItem == coatIdx);  // should pick leather coat
        }

        FreeJobSystem();
    }

    // =========================================================================
    // 7. WorkGiver_EquipClothing returns -1 when no clothing exists
    // =========================================================================
    it("WorkGiver returns -1 when no clothing available") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        // No clothing spawned
        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId == -1);

        FreeJobSystem();
    }

    // =========================================================================
    // 8. WorkGiver skips reserved clothing
    // =========================================================================
    it("WorkGiver skips reserved clothing items") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        int clothIdx = SpawnItem(6 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_GRASS_TUNIC);
        items[clothIdx].reservedBy = 99;  // reserved by someone else

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId == -1);

        FreeJobSystem();
    }

    // =========================================================================
    // 9. RunJob_EquipClothing equips when mover reaches item
    // =========================================================================
    it("RunJob equips clothing when mover is at item location") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        // Spawn clothing at mover's exact position
        float mx = movers[mi].x;
        float my = movers[mi].y;
        int clothIdx = SpawnItem(mx, my, 1.0f, ITEM_LEATHER_VEST);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId >= 0);

        // Run the job - mover is already at pickup location
        Job* job = GetJob(jobId);
        JobRunResult result = RunJob_EquipClothing(job, &movers[mi], TICK_DT);

        expect(result == JOBRUN_DONE);
        expect(movers[mi].equippedClothing == clothIdx);
        expect(items[clothIdx].state == ITEM_CARRIED);
        expect(items[clothIdx].reservedBy == mi);

        FreeJobSystem();
    }

    // =========================================================================
    // 10. Equipping drops old clothing
    // =========================================================================
    it("equipping new clothing drops old clothing") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        float mx = movers[mi].x;
        float my = movers[mi].y;

        // Manually equip a grass tunic
        int oldClothIdx = SpawnItem(mx, my, 1.0f, ITEM_GRASS_TUNIC);
        items[oldClothIdx].state = ITEM_CARRIED;
        items[oldClothIdx].reservedBy = mi;
        movers[mi].equippedClothing = oldClothIdx;

        // Spawn a better item at mover position
        int newClothIdx = SpawnItem(mx, my, 1.0f, ITEM_LEATHER_COAT);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId >= 0);

        Job* job = GetJob(jobId);
        JobRunResult result = RunJob_EquipClothing(job, &movers[mi], TICK_DT);

        expect(result == JOBRUN_DONE);
        expect(movers[mi].equippedClothing == newClothIdx);
        // Old clothing should be dropped (on ground, unreserved)
        expect(items[oldClothIdx].state == ITEM_ON_GROUND);
        expect(items[oldClothIdx].reservedBy == -1);

        FreeJobSystem();
    }

    // =========================================================================
    // 11. Upgrade requires >0.1 improvement
    // =========================================================================
    it("WorkGiver does not upgrade for marginal improvement") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        float mx = movers[mi].x;
        float my = movers[mi].y;

        // Equip flax tunic (0.40)
        int oldIdx = SpawnItem(mx, my, 1.0f, ITEM_FLAX_TUNIC);
        items[oldIdx].state = ITEM_CARRIED;
        items[oldIdx].reservedBy = mi;
        movers[mi].equippedClothing = oldIdx;

        // Spawn leather vest (0.50) — only 0.10 improvement, not > 0.1
        SpawnItem(6 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_LEATHER_VEST);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId == -1);  // not enough improvement

        FreeJobSystem();
    }

    it("WorkGiver upgrades when improvement exceeds 0.1") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        float mx = movers[mi].x;
        float my = movers[mi].y;

        // Equip grass tunic (0.25)
        int oldIdx = SpawnItem(mx, my, 1.0f, ITEM_GRASS_TUNIC);
        items[oldIdx].state = ITEM_CARRIED;
        items[oldIdx].reservedBy = mi;
        movers[mi].equippedClothing = oldIdx;

        // Spawn leather vest (0.50) — 0.25 improvement, should upgrade
        SpawnItem(6 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 1.0f, ITEM_LEATHER_VEST);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId >= 0);  // should create upgrade job

        FreeJobSystem();
    }

    // =========================================================================
    // 12. DropEquippedClothing works correctly
    // =========================================================================
    it("DropEquippedClothing drops clothing on ground") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);

        float mx = movers[mi].x;
        float my = movers[mi].y;
        int clothIdx = SpawnItem(mx, my, 1.0f, ITEM_GRASS_TUNIC);
        items[clothIdx].state = ITEM_CARRIED;
        items[clothIdx].reservedBy = mi;
        movers[mi].equippedClothing = clothIdx;

        DropEquippedClothing(mi);

        expect(movers[mi].equippedClothing == -1);
        expect(items[clothIdx].state == ITEM_ON_GROUND);
        expect(items[clothIdx].reservedBy == -1);
    }

    it("DropEquippedClothing is no-op when no clothing") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        expect(movers[mi].equippedClothing == -1);

        DropEquippedClothing(mi);  // should not crash

        expect(movers[mi].equippedClothing == -1);
    }

    // =========================================================================
    // 13. Clothing reduces body heat cooling rate
    // =========================================================================
    it("clothed mover cools slower than naked mover in cold") {
        SetupClothingGrid();
        hungerEnabled = true;  // must be true so NeedsTick doesn't reset hunger to 1.0
        int nakedIdx = SetupMover(3, 3);
        int clothedIdx = SetupMover(8, 8);

        // Set cold ambient temperature
        SetAmbientTemp(0);  // 0°C — freezing

        // Start both at normal body temp, hunger=0 (no metabolic heat bonus)
        movers[nakedIdx].bodyTemp = balance.bodyTempNormal;   // 37°C
        movers[nakedIdx].hunger = 0.0f;
        movers[clothedIdx].bodyTemp = balance.bodyTempNormal; // 37°C
        movers[clothedIdx].hunger = 0.0f;

        // Equip leather coat on clothed mover
        float cx = movers[clothedIdx].x;
        float cy = movers[clothedIdx].y;
        int clothIdx = SpawnItem(cx, cy, 1.0f, ITEM_LEATHER_COAT);
        items[clothIdx].state = ITEM_CARRIED;
        items[clothIdx].reservedBy = clothedIdx;
        movers[clothedIdx].equippedClothing = clothIdx;

        // Use moderate dt so movers cool but don't both hit the 20°C floor
        float savedDt = gameDeltaTime;
        gameDeltaTime = 1.0f;
        for (int i = 0; i < 30; i++) {
            NeedsTick();
        }
        gameDeltaTime = savedDt;

        // Clothed mover should be warmer than naked mover
        expect(movers[clothedIdx].bodyTemp > movers[nakedIdx].bodyTemp);

        // Both should have cooled below normal
        expect(movers[nakedIdx].bodyTemp < balance.bodyTempNormal);
        expect(movers[clothedIdx].bodyTemp < balance.bodyTempNormal);
    }

    it("clothing has no effect when warming (ambient > body temp)") {
        SetupClothingGrid();
        int nakedIdx = SetupMover(3, 3);
        int clothedIdx = SetupMover(8, 8);

        // Set warm ambient temperature
        SetAmbientTemp(40);  // 40°C — hot

        // Start both cold
        movers[nakedIdx].bodyTemp = 30.0f;
        movers[clothedIdx].bodyTemp = 30.0f;

        // Equip leather coat
        float cx = movers[clothedIdx].x;
        float cy = movers[clothedIdx].y;
        int clothIdx = SpawnItem(cx, cy, 1.0f, ITEM_LEATHER_COAT);
        items[clothIdx].state = ITEM_CARRIED;
        items[clothIdx].reservedBy = clothedIdx;
        movers[clothedIdx].equippedClothing = clothIdx;

        float savedDt = gameDeltaTime;
        gameDeltaTime = 10.0f;
        for (int i = 0; i < 200; i++) {
            NeedsTick();
        }
        gameDeltaTime = savedDt;

        // Both should warm at the same rate (clothing doesn't affect warming)
        float diff = movers[clothedIdx].bodyTemp - movers[nakedIdx].bodyTemp;
        expect(diff > -0.5f && diff < 0.5f);  // roughly equal
    }

    // =========================================================================
    // 14. Better clothing = slower cooling
    // =========================================================================
    it("leather coat insulates better than grass tunic") {
        SetupClothingGrid();
        hungerEnabled = true;  // must be true so NeedsTick doesn't reset hunger to 1.0
        int tunicIdx = SetupMover(3, 3);
        int coatIdx = SetupMover(8, 8);

        SetAmbientTemp(0);

        movers[tunicIdx].bodyTemp = balance.bodyTempNormal;
        movers[tunicIdx].hunger = 0.0f;
        movers[coatIdx].bodyTemp = balance.bodyTempNormal;
        movers[coatIdx].hunger = 0.0f;

        // Equip grass tunic (0.25 reduction)
        float tx = movers[tunicIdx].x;
        float ty = movers[tunicIdx].y;
        int tunicItem = SpawnItem(tx, ty, 1.0f, ITEM_GRASS_TUNIC);
        items[tunicItem].state = ITEM_CARRIED;
        items[tunicItem].reservedBy = tunicIdx;
        movers[tunicIdx].equippedClothing = tunicItem;

        // Equip leather coat (0.65 reduction)
        float cx = movers[coatIdx].x;
        float cy = movers[coatIdx].y;
        int coatItem = SpawnItem(cx, cy, 1.0f, ITEM_LEATHER_COAT);
        items[coatItem].state = ITEM_CARRIED;
        items[coatItem].reservedBy = coatIdx;
        movers[coatIdx].equippedClothing = coatItem;

        // Use moderate dt so movers cool but don't both hit the 20°C floor
        float savedDt = gameDeltaTime;
        gameDeltaTime = 1.0f;
        for (int i = 0; i < 30; i++) {
            NeedsTick();
        }
        gameDeltaTime = savedDt;

        // Leather coat mover should be warmer than grass tunic mover
        expect(movers[coatIdx].bodyTemp > movers[tunicIdx].bodyTemp);
    }

    // =========================================================================
    // 15. Workshop recipes produce correct items
    // =========================================================================
    it("loom recipes exist for cloth and linen") {
        // Verify workshop definitions
        int loomType = WORKSHOP_LOOM;
        expect(workshopDefs[loomType].recipeCount >= 3);

        // Check recipe 0: dried grass -> cloth
        const Recipe* r0 = &workshopDefs[loomType].recipes[0];
        expect(r0->inputType == ITEM_DRIED_GRASS);
        expect(r0->outputType == ITEM_CLOTH);

        // Check recipe 2: flax fiber -> linen
        const Recipe* r2 = &workshopDefs[loomType].recipes[2];
        expect(r2->inputType == ITEM_FLAX_FIBER);
        expect(r2->outputType == ITEM_LINEN);
    }

    it("tanning rack recipe converts hide to leather") {
        int tanType = WORKSHOP_TANNING_RACK;
        expect(workshopDefs[tanType].recipeCount >= 1);

        const Recipe* r = &workshopDefs[tanType].recipes[0];
        expect(r->inputType == ITEM_HIDE);
        expect(r->outputType == ITEM_LEATHER);
        expect(workshopDefs[tanType].passive == true);
    }

    it("tailor recipes produce clothing items") {
        int tailorType = WORKSHOP_TAILOR;
        expect(workshopDefs[tailorType].recipeCount >= 4);

        // Grass tunic: 3 cloth -> 1 grass tunic
        const Recipe* r0 = &workshopDefs[tailorType].recipes[0];
        expect(r0->inputType == ITEM_CLOTH);
        expect(r0->inputCount == 3);
        expect(r0->outputType == ITEM_GRASS_TUNIC);

        // Flax tunic: 2 linen -> 1 flax tunic
        const Recipe* r1 = &workshopDefs[tailorType].recipes[1];
        expect(r1->inputType == ITEM_LINEN);
        expect(r1->inputCount == 2);
        expect(r1->outputType == ITEM_FLAX_TUNIC);

        // Leather vest: 2 leather -> 1 vest
        const Recipe* r2 = &workshopDefs[tailorType].recipes[2];
        expect(r2->inputType == ITEM_LEATHER);
        expect(r2->inputCount == 2);
        expect(r2->outputType == ITEM_LEATHER_VEST);

        // Leather coat: 2 leather + 1 cloth -> 1 coat
        const Recipe* r3 = &workshopDefs[tailorType].recipes[3];
        expect(r3->inputType == ITEM_LEATHER);
        expect(r3->inputCount == 2);
        expect(r3->inputType2 == ITEM_CLOTH);
        expect(r3->inputCount2 == 1);
        expect(r3->outputType == ITEM_LEATHER_COAT);
    }

    // =========================================================================
    // 16. Stockpile filters accept textile items
    // =========================================================================
    it("stockpile filters textile items correctly") {
        SetupClothingGrid();
        int spIdx = CreateStockpile(2, 2, 1, 4, 4);
        expect(spIdx >= 0);

        // New stockpile allows all by default
        expect(StockpileAcceptsType(spIdx, ITEM_CLOTH) == true);
        expect(StockpileAcceptsType(spIdx, ITEM_LEATHER_COAT) == true);

        // Disable all, then selectively enable
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(spIdx, t, false);
        }
        SetStockpileFilter(spIdx, ITEM_CLOTH, true);
        SetStockpileFilter(spIdx, ITEM_LEATHER_COAT, true);

        expect(StockpileAcceptsType(spIdx, ITEM_CLOTH) == true);
        expect(StockpileAcceptsType(spIdx, ITEM_LEATHER_COAT) == true);
        expect(StockpileAcceptsType(spIdx, ITEM_LOG) == false);
        expect(StockpileAcceptsType(spIdx, ITEM_ROCK) == false);
    }

    // =========================================================================
    // 17. CancelJob releases clothing item reservation
    // =========================================================================
    it("CancelJob releases clothing item reservation") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        int clothIdx = SpawnItem(10 * CELL_SIZE + 4, 10 * CELL_SIZE + 4, 1.0f, ITEM_GRASS_TUNIC);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId >= 0);
        expect(items[clothIdx].reservedBy == mi);

        // Cancel the job
        CancelJob(&movers[mi], mi);

        expect(movers[mi].currentJobId == -1);
        expect(items[clothIdx].reservedBy == -1);

        FreeJobSystem();
    }

    // =========================================================================
    // 18. WorkGiver only matches same z-level
    // =========================================================================
    it("WorkGiver ignores clothing on different z-level") {
        SetupClothingGrid();
        int mi = SetupMover(5, 5);  // mover at z=1
        InitJobSystem(moverCount);
        RebuildIdleMoverList();

        // Spawn clothing at z=0 (underground)
        SpawnItem(6 * CELL_SIZE + 4, 5 * CELL_SIZE + 4, 0.0f, ITEM_GRASS_TUNIC);

        int jobId = WorkGiver_EquipClothing(mi);
        expect(jobId == -1);  // different z-level

        FreeJobSystem();
    }

    // =========================================================================
    // 19. Construction recipes exist for new workshops
    // =========================================================================
    it("construction recipes exist for loom, tanning rack, tailor") {
        int loomRecipe = GetConstructionRecipeForWorkshopType(WORKSHOP_LOOM);
        int tanRecipe = GetConstructionRecipeForWorkshopType(WORKSHOP_TANNING_RACK);
        int tailorRecipe = GetConstructionRecipeForWorkshopType(WORKSHOP_TAILOR);

        expect(loomRecipe >= 0);
        expect(tanRecipe >= 0);
        expect(tailorRecipe >= 0);
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(clothing);

    return summary();
}
