// Inventory: two hands, a back slot, pockets on a pocket jacket (src/entities/inventory.c)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/containers.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/jobs.h"
#include "../src/entities/inventory.h"
#include "../src/entities/tool_quality.h"
#include "../src/core/state_audit.h"
#include "../src/simulation/needs.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

static int Spawn(int cx, int cy, ItemType t) {
    return SpawnItem(cx * CELL_SIZE + CELL_SIZE * 0.5f, cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, t);
}

// A flat 10x10 world at z=1 with one mover at (5,5)
static int Setup(void) {
    InitTestGrid(10, 10);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
        }
    ClearMovers(); ClearItems(); ClearStockpiles(); ClearJobs();
    InitMover(&movers[0], 5.5f * CELL_SIZE, 5.5f * CELL_SIZE, 1.0f, (Point){5, 5, 1}, 100.0f);
    moverCount = 1;
    return 0;
}

describe(hands) {
    it("a one-handed item takes one hand, carried and reserved by the mover") {
        int mi = Setup();
        int can = Spawn(5, 5, ITEM_SODA);
        expect(MoverFreeHands(mi) == 2);
        expect(MoverHold(mi, can));
        expect(MoverFreeHands(mi) == 1);
        expect(items[can].state == ITEM_CARRIED && items[can].reservedBy == mi);
        expect(ItemInMoverHands(mi, can));
    }

    it("a log takes both hands; then nothing else fits in a hand") {
        int mi = Setup();
        int log = Spawn(5, 5, ITEM_LOG);
        int can = Spawn(5, 5, ITEM_SODA);
        expect(MoverHold(mi, log));
        expect(movers[mi].hands[0] == log && movers[mi].hands[1] == log);
        expect(MoverFreeHands(mi) == 0);
        expect(!MoverHold(mi, can));
        expect(items[can].state == ITEM_ON_GROUND);
    }

    it("one hand full: a log doesn't fit") {
        int mi = Setup();
        expect(MoverHold(mi, Spawn(5, 5, ITEM_PLASTIC_BAG)));
        int log = Spawn(5, 5, ITEM_LOG);
        expect(!MoverHold(mi, log));
        expect(items[log].state == ITEM_ON_GROUND);
    }

    it("a bag in one hand, a can in the other") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        int can = Spawn(5, 5, ITEM_SODA);
        expect(MoverHold(mi, bag));
        expect(MoverHold(mi, can));
        expect(MoverFreeHands(mi) == 0);
    }

    it("dropping a two-handed item frees both hands; it lies at the mover's feet") {
        int mi = Setup();
        int log = Spawn(2, 2, ITEM_LOG);
        expect(MoverHold(mi, log));
        MoverDropHand(mi, 1);
        expect(MoverFreeHands(mi) == 2);
        expect(items[log].state == ITEM_ON_GROUND && items[log].reservedBy == -1);
        expect((int)(items[log].x / CELL_SIZE) == 5 && (int)(items[log].y / CELL_SIZE) == 5);
    }

    it("held items follow the mover; a dropped bag keeps its contents") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        int can = Spawn(5, 5, ITEM_SODA);
        MoverHold(mi, bag);
        expect(MoverStow(mi, can) == INV_HELD_BAG);
        movers[mi].x = 8.5f * CELL_SIZE;
        MoverSyncInventory(mi);
        expect((int)(items[bag].x / CELL_SIZE) == 8);
        MoverDropHand(mi, 0);
        expect(items[can].containedIn == bag);
        expect((int)(items[can].x / CELL_SIZE) == 8);
    }
}

describe(worn) {
    it("a backpack goes on the back, a pocket jacket on the body (old clothing dropped)") {
        int mi = Setup();
        int pack = Spawn(5, 5, ITEM_BACKPACK);
        int tunic = Spawn(5, 5, ITEM_GRASS_TUNIC);
        int jacket = Spawn(5, 5, ITEM_POCKET_JACKET);
        expect(MoverWear(mi, pack));
        expect(movers[mi].wornBack == pack && MoverFreeHands(mi) == 2);
        expect(MoverWear(mi, tunic));
        expect(MoverWear(mi, jacket));
        expect(movers[mi].equippedClothing == jacket);
        expect(items[tunic].state == ITEM_ON_GROUND && items[tunic].reservedBy == -1);
        expect(!MoverWear(mi, Spawn(5, 5, ITEM_SODA)));   // a can is not worn
    }

    it("a dropped pocket jacket keeps what is in its pockets, where it lands") {
        int mi = Setup();
        int jacket = Spawn(5, 5, ITEM_POCKET_JACKET);
        int can = Spawn(5, 5, ITEM_SODA);
        MoverWear(mi, jacket);
        expect(MoverStow(mi, can) == INV_POCKETS);
        movers[mi].x = 2.5f * CELL_SIZE;
        MoverSyncInventory(mi);
        DropEquippedClothing(mi);
        expect(items[can].containedIn == jacket);
        expect((int)(items[can].x / CELL_SIZE) == 2 && (int)(items[jacket].x / CELL_SIZE) == 2);
    }

    it("only the pocket jacket has pockets") {
        expect(GetContainerDef(ITEM_POCKET_JACKET) != NULL);
        expect(GetContainerDef(ITEM_GRASS_TUNIC) == NULL);
        expect(GetContainerDef(ITEM_LEATHER_COAT) == NULL);
        expect(GetClothingCoolingReduction(ITEM_POCKET_JACKET) > 0.0f);
    }
}

describe(stow) {
    it("pockets first, then the backpack, then a bag in hand, then a free hand, then nothing") {
        int mi = Setup();
        MoverWear(mi, Spawn(5, 5, ITEM_POCKET_JACKET));   // 3 stacks
        MoverWear(mi, Spawn(5, 5, ITEM_BACKPACK));        // 10 stacks
        MoverHold(mi, Spawn(5, 5, ITEM_PLASTIC_BAG));     // 4 stacks
        // Distinct non-stacking things so each takes a slot
        ItemType kinds[] = { ITEM_SHARP_STONE, ITEM_STICKS, ITEM_CORDAGE };
        int placed[INV_HAND + 1] = {0};
        InvPlace last = INV_NONE;
        for (int k = 0; k < 3 + 10 + 4 + 1 + 1; k++) {
            int it = Spawn(5, 5, kinds[k % 3]);
            items[it].material = (uint8_t)(k + 1);   // different materials: no merging into a stack
            last = MoverStow(mi, it);
            placed[last]++;
        }
        expect(placed[INV_POCKETS] == 3);
        expect(placed[INV_BACK] == 10);
        expect(placed[INV_HELD_BAG] == 4);
        expect(placed[INV_HAND] == 1);
        expect(last == INV_NONE);
    }

    it("stowing from a stockpile frees the slot") {
        int mi = Setup();
        MoverWear(mi, Spawn(5, 5, ITEM_BACKPACK));
        int sp = CreateStockpile(2, 2, 1, 1, 1);
        int can = Spawn(2, 2, ITEM_SODA);
        PlaceItemInStockpile(sp, 2, 2, can);
        expect(items[can].state == ITEM_IN_STOCKPILE);
        expect(MoverStow(mi, can) == INV_BACK);
        expect(items[can].containedIn == movers[mi].wornBack);
        expect(stockpiles[sp].slots[0] == -1);
    }

    it("what is in a worn or held container is out of reach for others") {
        int mi = Setup();
        MoverWear(mi, Spawn(5, 5, ITEM_BACKPACK));
        int can = Spawn(5, 5, ITEM_SODA);
        MoverStow(mi, can);
        expect(!IsItemAccessible(can));
        expect(ItemIsInMoverInventory(mi, can));
        MoverDropBack(mi);
        expect(IsItemAccessible(can));
        expect(!ItemIsInMoverInventory(mi, can));
    }

    it("an idle mover keeps its gear reserved through the stale-reservation sweep (needs.c)") {
        int mi = Setup();
        int jacket = Spawn(5, 5, ITEM_POCKET_JACKET), pack = Spawn(5, 5, ITEM_BACKPACK);
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG), can = Spawn(5, 5, ITEM_SODA);
        int stray = Spawn(2, 2, ITEM_STICKS);
        MoverWear(mi, jacket); MoverWear(mi, pack); MoverHold(mi, bag); MoverHold(mi, can);
        items[stray].reservedBy = mi;   // a leftover reservation the sweep should clear
        hungerEnabled = false; thirstEnabled = false; energyEnabled = false;
        bodyTempEnabled = false; bladderEnabled = false;
        currentTick = TICK_RATE;        // the sweep runs for mover 0 on this tick
        ProcessFreetimeNeeds();
        expect(items[stray].reservedBy == -1);
        expect(items[jacket].reservedBy == mi && items[pack].reservedBy == mi);
        expect(items[bag].reservedBy == mi && items[can].reservedBy == mi);
        expect(RunStateAudit(test_verbose) == 0);
    }

    it("the state audit is clean with things held and worn, and after dropping all") {
        int mi = Setup();
        MoverWear(mi, Spawn(5, 5, ITEM_POCKET_JACKET));
        MoverWear(mi, Spawn(5, 5, ITEM_BACKPACK));
        MoverHold(mi, Spawn(5, 5, ITEM_LOG));
        MoverStow(mi, Spawn(5, 5, ITEM_SODA));
        expect(RunStateAudit(test_verbose) == 0);
        MoverDropAll(mi);
        DropEquippedClothing(mi);
        expect(RunStateAudit(test_verbose) == 0);
        expect(MoverFreeHands(mi) == 2 && movers[mi].wornBack == -1);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(hands);
    test(worn);
    test(stow);
    return summary();
}
