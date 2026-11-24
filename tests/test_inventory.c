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
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// From core/saveload.c
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

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
        MoverWear(mi, Spawn(5, 5, ITEM_POCKET_JACKET));   // 2 L
        MoverWear(mi, Spawn(5, 5, ITEM_BACKPACK));        // 25 L, at most 20 stacks
        MoverHold(mi, Spawn(5, 5, ITEM_PLASTIC_BAG));     // 10 L, at most 10 stacks
        int placed[INV_HAND + 1] = {0};
        InvPlace last = INV_NONE;
        for (int k = 0; k < 5 + 20 + 10 + 1 + 1; k++) {
            int it = Spawn(5, 5, ITEM_SODA);   // 0.4 L
            items[it].material = (uint8_t)(k % 250 + 1);   // different materials: no merging into a stack
            last = MoverStow(mi, it);
            placed[last]++;
        }
        expect(placed[INV_POCKETS] == 5);    // 5 x 0.4 = 2 L
        expect(placed[INV_BACK] == 20);      // stack limit before the litres run out
        expect(placed[INV_HELD_BAG] == 10);
        expect(placed[INV_HAND] == 1);
        expect(last == INV_NONE);
        expect(RunStateAudit(test_verbose) == 0);
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

describe(volume_weight) {
    it("a pocket takes five cans, not a sixth; a log doesn't fit in a backpack") {
        int mi = Setup();
        int jacket = Spawn(5, 5, ITEM_POCKET_JACKET);
        MoverWear(mi, jacket);
        for (int k = 0; k < 5; k++) {
            int c = Spawn(5, 5, ITEM_EMPTY_CAN);
            items[c].material = (uint8_t)(k + 1);
            expect(CanPutItemInContainer(c, jacket));
            PutItemInContainer(c, jacket);
        }
        int sixth = Spawn(5, 5, ITEM_EMPTY_CAN);
        items[sixth].material = 9;
        expect(!CanPutItemInContainer(sixth, jacket));
        expect(ContainerContentsVolume(jacket) > 1.99f);
        int pack = Spawn(5, 5, ITEM_BACKPACK);
        expect(!CanPutItemInContainer(Spawn(5, 5, ITEM_LOG), pack));
        expect(ItemVolume(ITEM_LOG) > 25.0f);
    }

    it("a full bag takes up more room than an empty one") {
        Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        float empty = ItemTotalVolume(bag);
        PutItemInContainer(Spawn(5, 5, ITEM_BURGER), bag);
        expect(ItemTotalVolume(bag) > empty + 0.4f);
    }

    it("weight on the back counts half; in the hands in full") {
        int mi = Setup();
        int pack = Spawn(5, 5, ITEM_BACKPACK);
        MoverWear(mi, pack);
        int soda = Spawn(5, 5, ITEM_SODA);
        items[soda].stackCount = 10;   // 3.5 kg
        expect(MoverStow(mi, soda) == INV_BACK);
        float back = MoverCarriedWeight(mi);
        expect(back > 0.5f * (1.0f + 3.5f) - 0.01f && back < 0.5f * (1.0f + 3.5f) + 0.01f);
        MoverDropBack(mi);
        expect(MoverCarriedWeight(mi) < 0.01f);
        int soda2 = Spawn(5, 5, ITEM_SODA);
        items[soda2].stackCount = 10;
        MoverHold(mi, soda2);
        expect(MoverCarriedWeight(mi) > 3.49f);
    }

    it("the lifting limit: a log too heavy on top of what it carries is not picked up") {
        int mi = Setup();
        float saved = carryLimitKg;
        carryLimitKg = 10.0f;
        int log = Spawn(5, 5, ITEM_LOG);   // 20 kg
        expect(!MoverHold(mi, log));
        expect(items[log].state == ITEM_ON_GROUND);
        carryLimitKg = 30.0f;
        expect(MoverHold(mi, log));
        carryLimitKg = saved;
    }

    it("a real load tires: more energy drain while carrying a log") {
        int mi = Setup();
        InitBalance();
        movers[mi].bodyTemp = balance.bodyTempNormal;
        float idle = MoverEnergyDrainPerGH(mi);
        expect(idle > 0.0f);
        MoverHold(mi, Spawn(5, 5, ITEM_LOG));
        expect(MoverEnergyDrainPerGH(mi) > idle * 1.05f);
    }

    it("carrying slows walking") {
        int mi = Setup();
        float before = MoverWalkSecondsPerCell(mi);
        MoverHold(mi, Spawn(5, 5, ITEM_LOG));
        expect(MoverWalkSecondsPerCell(mi) > before * 1.3f);
    }

    it("contents lists follow puts, takes and deletes; a rebuild gives the same") {
        Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        int a = Spawn(5, 5, ITEM_SODA), b = Spawn(5, 5, ITEM_SNACK), c = Spawn(5, 5, ITEM_BURGER);
        PutItemInContainer(a, bag); PutItemInContainer(b, bag); PutItemInContainer(c, bag);
        int n = 0;
        for (int i = FirstInContainer(bag); i >= 0; i = NextInContainer(i)) n++;
        expect(n == 3 && items[bag].contentCount == 3);
        RemoveItemFromContainer(b);
        DeleteItem(c);
        n = 0;
        for (int i = FirstInContainer(bag); i >= 0; i = NextInContainer(i)) { n++; expect(i == a); }
        expect(n == 1 && items[bag].contentCount == 1);
        RebuildContainerLists();
        expect(FirstInContainer(bag) == a && NextInContainer(a) == -1);
        expect(RunStateAudit(test_verbose) == 0);
    }
}

describe(jobs_and_hands) {
    it("a log to haul: the bag in hand goes down at the mover's feet, still its own") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        int can = Spawn(5, 5, ITEM_EMPTY_CAN);
        MoverHold(mi, bag);
        MoverStow(mi, can);
        int log = Spawn(5, 5, ITEM_LOG);
        MoverMakeHandsFor(mi, log);
        expect(!ItemInMoverHands(mi, bag));
        expect(items[bag].state == ITEM_ON_GROUND && items[bag].reservedBy == mi);
        expect(items[can].containedIn == bag);
        expect(MoverSetAsideItem(mi) == bag);
        expect(ItemIsMoverGear(mi, bag));
        expect(RunStateAudit(test_verbose) == 0);
    }

    it("berries fit beside the bag: nothing goes down") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        MoverHold(mi, bag);
        MoverMakeHandsFor(mi, Spawn(5, 5, ITEM_BERRIES));
        expect(ItemInMoverHands(mi, bag));
        expect(MoverSetAsideItem(mi) == -1);
    }

    it("a job's load counts in the hands and in the weight") {
        int mi = Setup();
        InitJobPool();
        int log = Spawn(5, 5, ITEM_LOG);
        int j = CreateJob(JOBTYPE_HAUL);
        expect(j >= 0);
        jobs[j].assignedMover = mi;
        jobs[j].carryingItem = log;
        items[log].state = ITEM_CARRIED;
        movers[mi].currentJobId = j;
        expect(MoverJobCarry(mi) == log);
        expect(MoverFreeHands(mi) == 0);
        expect(MoverCarriedWeight(mi) > 19.9f);
        expect(!MoverHold(mi, Spawn(5, 5, ITEM_SODA)));   // no hand free while hauling a log
    }

    it("idle a while with nothing to do: it goes back for its bag and holds it again") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        MoverHold(mi, bag);
        MoverMakeHandsFor(mi, Spawn(5, 5, ITEM_LOG));
        movers[mi].x = 2.5f * CELL_SIZE;   // walked off with the log
        hungerEnabled = false; thirstEnabled = false; energyEnabled = false;
        bodyTempEnabled = false; bladderEnabled = false;
        InitBalance();
        bool fetching = false;
        for (int t = 0; t < 4000 && !fetching; t++) {
            ProcessFreetimeNeeds();
            fetching = movers[mi].freetimeState == FREETIME_FETCHING_OWN;
        }
        expect(fetching);
        expect(movers[mi].needTarget == bag);
        movers[mi].x = items[bag].x; movers[mi].y = items[bag].y;   // arrive
        ProcessFreetimeNeeds();
        expect(ItemInMoverHands(mi, bag));
        expect(MoverSetAsideItem(mi) == -1);
        expect(RunStateAudit(test_verbose) == 0);
    }

    it("the stale-reservation sweep leaves a set-aside bag reserved") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        MoverHold(mi, bag);
        MoverMakeHandsFor(mi, Spawn(5, 5, ITEM_LOG));
        hungerEnabled = false; thirstEnabled = false; energyEnabled = false;
        bodyTempEnabled = false; bladderEnabled = false;
        currentTick = TICK_RATE;
        ProcessFreetimeNeeds();
        expect(items[bag].reservedBy == mi);
    }

    it("save and load: worn, held and set-aside things stay the mover's own (reserved)") {
        int mi = Setup();
        int jacket = Spawn(5, 5, ITEM_POCKET_JACKET), pack = Spawn(5, 5, ITEM_BACKPACK);
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG), can = Spawn(5, 5, ITEM_EMPTY_CAN);
        int soda = Spawn(5, 5, ITEM_SODA);
        MoverWear(mi, jacket); MoverWear(mi, pack);
        MoverHold(mi, bag); MoverStow(mi, can);   // the can goes in the pockets
        MoverHold(mi, soda);
        MoverMakeHandsFor(mi, Spawn(5, 5, ITEM_LOG));   // both hands: the bag and the soda go down
        expect(MoverSetAsideItem(mi) >= 0);
        expect(SaveWorld("/tmp/test_inventory_save.bin"));
        ClearItems(); ClearMovers();
        expect(LoadWorld("/tmp/test_inventory_save.bin"));
        expect(moverCount == 1);
        expect(movers[mi].equippedClothing == jacket && movers[mi].wornBack == pack);
        expect(items[jacket].reservedBy == mi && items[pack].reservedBy == mi);
        expect(movers[mi].setAside[0] >= 0 || movers[mi].setAside[1] >= 0);
        expect(items[bag].reservedBy == mi && items[soda].reservedBy == mi);
        expect(items[can].containedIn == jacket && FirstInContainer(jacket) == can);
        expect(RunStateAudit(test_verbose) == 0);
        // and it still goes back for them
        int first = MoverSetAsideItem(mi);
        expect(first == bag || first == soda);
    }

    it("death: set-aside things become free for anyone") {
        int mi = Setup();
        int bag = Spawn(5, 5, ITEM_PLASTIC_BAG);
        MoverHold(mi, bag);
        MoverMakeHandsFor(mi, Spawn(5, 5, ITEM_LOG));
        MoverDropAll(mi);
        expect(items[bag].reservedBy == -1);
        expect(MoverSetAsideItem(mi) == -1);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(hands);
    test(worn);
    test(stow);
    test(volume_weight);
    test(jobs_and_hands);
    return summary();
}
