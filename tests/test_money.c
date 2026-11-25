// Money (src/entities/money.c, City 1999): a number in a mover's purse; coins (ITEM_COIN,
// stackCount = coins) only when it lies in the world. docs/vision/city-1999.md "Money and exchange"
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/furniture.h"
#include "../src/entities/action_queue.h"
#include "../src/entities/money.h"
#include "../src/entities/inventory.h"
#include "../src/entities/containers.h"
#include "../src/simulation/plants.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/core/state_audit.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

static const char* openFloor =
    "................\n"
    "................\n"
    "................\n"
    "................\n"
    "................\n";

// A 16x5 floor, two movers at (1,2) and (1,4), needs off
static void Setup(void) {
    InitTestGridFromAscii(openFloor);
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    ClearMovers(); ClearItems(); ClearStockpiles(); ClearJobs(); ClearPlants(); ClearFurniture();
    InitBalance();
    hungerEnabled = false; energyEnabled = false; bodyTempEnabled = false;
    thirstEnabled = false; bladderEnabled = false;
    gameDeltaTime = TICK_DT;
    currentTick = 1;
    Point g0 = {1, 2, 0}, g1 = {1, 4, 0};
    InitMover(&movers[0], 1.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, g0, MOVER_SPEED);
    InitMover(&movers[1], 1.5f * CELL_SIZE, 4.5f * CELL_SIZE, 0.0f, g1, MOVER_SPEED);
    moverCount = 2;
}

static int CoinsOnGround(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == ITEM_COIN) n++;
    return n;
}

describe(purse) {
    it("starts empty, takes money in, pays only what it has") {
        Setup();
        expect(MoverCoins(0) == 0);
        MoverAddCoins(0, 325);
        expect(MoverCoins(0) == 325);
        expect(MoverPay(0, 150));
        expect(MoverCoins(0) == 175);
        expect(!MoverPay(0, 200));       // not enough: nothing paid
        expect(MoverCoins(0) == 175);
        MoverAddCoins(0, -500);          // taking more than there is leaves 0
        expect(MoverCoins(0) == 0);
    }

    it("formats as coins") {
        char b[24];
        expect(strcmp(MoneyFormat(3, b, sizeof b), "3 coins") == 0);
        expect(strcmp(MoneyFormat(1, b, sizeof b), "1 coin") == 0);
        expect(strcmp(MoneyFormat(0, b, sizeof b), "0 coins") == 0);
    }

    it("a new mover in a reused slot has an empty purse; clearing the movers empties all") {
        Setup();
        MoverAddCoins(0, 100); MoverAddCoins(1, 200);
        Point g = {1, 2, 0};
        InitMover(&movers[0], 1.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, g, MOVER_SPEED);
        expect(MoverCoins(0) == 0 && MoverCoins(1) == 200);
        ClearMovers();
        expect(MoverCoins(1) == 0);
    }
}

describe(coins_in_the_world) {
    it("dropping makes one coin item at the feet, picking it up makes it a number again") {
        Setup();
        MoverAddCoins(0, 300);
        int it = MoverDropCoins(0, 120);
        expect(it >= 0 && items[it].type == ITEM_COIN && items[it].stackCount == 120);
        expect(items[it].state == ITEM_ON_GROUND);
        expect((int)(items[it].x / CELL_SIZE) == 1 && (int)(items[it].y / CELL_SIZE) == 2);
        expect(MoverCoins(0) == 180);
        expect(MoverPocketCoins(1, it));      // someone else finds it
        expect(!items[it].active && CoinsOnGround() == 0);
        expect(MoverCoins(1) == 120);
    }

    it("drops at most what is in the purse; an empty purse drops nothing") {
        Setup();
        MoverAddCoins(0, 50);
        int it = MoverDropCoins(0, 500);
        expect(it >= 0 && items[it].stackCount == 50 && MoverCoins(0) == 0);
        expect(MoverDropCoins(0, 10) == -1);
        expect(CoinsOnGround() == 1);
    }

    it("no job hauls coins to a stockpile") {
        Setup();
        CreateStockpile(10, 0, 0, 3, 3);
        SpawnCoins(5.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, 75);
        for (int t = 0; t < 600; t++) { Tick(); AssignJobs(); JobsTick(); }
        int coin = -1;
        for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == ITEM_COIN) coin = i;
        expect(coin >= 0 && items[coin].state == ITEM_ON_GROUND && items[coin].reservedBy == -1);
        expect((int)(items[coin].x / CELL_SIZE) == 5);
    }
}

describe(pick_up_order) {
    it("E on coins: walks there and puts all of it in the purse, hands stay free") {
        Setup();
        int coin = SpawnCoins(8.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, 240);
        char said[96];
        expect(QueueCommandAt(0, 8, 2, 0, false, said, sizeof(said)));
        expect(strstr(said, "240 coins") != NULL);
        for (int t = 0; t < 3000 && QueueBusy(0); t++) { Tick(); ProcessFreetimeNeeds(); }
        expect(!QueueBusy(0));
        expect(!items[coin].active);
        expect(MoverCoins(0) == 240);
        expect(movers[0].hands[0] == -1 && movers[0].hands[1] == -1);
        expect((int)(movers[0].x / CELL_SIZE) >= 7);
        expect(RunStateAudit(test_verbose) == 0);
    }
}

describe(money_save) {
    it("purses and coins on the ground survive a save and load (v119)") {
        Setup();
        MoverAddCoins(0, 175); MoverAddCoins(1, 2);
        SpawnCoins(6.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, 90);
        expect(SaveWorld("/tmp/test_money_save.bin"));
        ClearItems(); ClearMovers();
        expect(MoverCoins(0) == 0);
        expect(LoadWorld("/tmp/test_money_save.bin"));
        expect(MoverCoins(0) == 175 && MoverCoins(1) == 2);
        int coin = -1;
        for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == ITEM_COIN) coin = i;
        expect(coin >= 0 && items[coin].stackCount == 90);
    }
}

// A shop: a 2x1 stockpile at (10,2) for sale, `n` units of `type` in its first slot
static int shop;
static int StockShop(ItemType type, int n) {
    shop = CreateStockpile(10, 2, 0, 2, 1);
    ShopSetForSale(shop, true);
    int it = SpawnItem(10.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, type);
    items[it].stackCount = n;
    PlaceItemInStockpile(shop, 10, 2, it);
    return it;
}

// Needs run until the mover's hunger went up (ate) or the time is up
static bool RunUntilAte(int mi, int maxTicks) {
    float before = movers[mi].hunger;
    for (int t = 0; t < maxTicks; t++) {
        Tick(); NeedsTick(); ProcessFreetimeNeeds();
        if (movers[mi].hunger > before + 0.05f) return true;
        before = movers[mi].hunger;
    }
    return false;
}

describe(shops) {
    it("a stockpile for sale prices what lies in it; not for sale or no price is free") {
        Setup();
        int snack = StockShop(ITEM_SNACK, 5);
        expect(ShopCount() == 1);
        expect(ItemShop(snack) == shop && ItemShopPrice(snack) == 3);
        expect(!MoverCanAfford(0, snack));
        MoverAddCoins(0, 3);
        expect(MoverCanAfford(0, snack));
        ShopSetForSale(shop, false);
        expect(ShopCount() == 0 && ItemShopPrice(snack) == 0 && MoverCanAfford(1, snack));
        ShopSetForSale(shop, true);
        int stone = SpawnItem(11.5f * CELL_SIZE, 2.5f * CELL_SIZE, 0.0f, ITEM_ROCK);
        PlaceItemInStockpile(shop, 11, 2, stone);
        expect(ItemShop(stone) == shop && ItemShopPrice(stone) == 0 && MoverCanAfford(1, stone));
        int loose = SpawnItem(5.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, ITEM_SNACK);
        expect(ItemShop(loose) == -1 && ItemShopPrice(loose) == 0);
    }

    it("buying pays into the till; a short purse pays nothing") {
        Setup();
        int snack = StockShop(ITEM_SNACK, 5);
        MoverAddCoins(0, 4);
        expect(MoverBuy(0, snack));
        expect(MoverCoins(0) == 1 && stockpileTill[shop] == 3);
        expect(!MoverBuy(0, snack));
        expect(MoverCoins(0) == 1 && stockpileTill[shop] == 3);
    }

    it("a hungry mover with money buys at the shop and eats; the till gets the price") {
        Setup();
        hungerEnabled = true;
        StockShop(ITEM_SNACK, 5);
        MoverAddCoins(0, 10);
        movers[0].hunger = 0.2f;
        expect(RunUntilAte(0, 20000));
        expect(MoverCoins(0) == 7 && stockpileTill[shop] == 3);
        expect(RunStateAudit(test_verbose) == 0);
    }

    it("without money it leaves the shop alone and eats free food on the ground instead") {
        Setup();
        hungerEnabled = true;
        int snack = StockShop(ITEM_SNACK, 5);
        int free = SpawnItem(4.5f * CELL_SIZE, 0.5f * CELL_SIZE, 0.0f, ITEM_BERRIES);
        movers[0].hunger = 0.2f;
        expect(RunUntilAte(0, 20000));
        expect(!items[free].active);
        expect(items[snack].active && items[snack].stackCount == 5 && stockpileTill[shop] == 0);
    }

    it("without money and no free food it doesn't even walk to the shop") {
        Setup();
        hungerEnabled = true;
        int snack = StockShop(ITEM_SNACK, 5);
        movers[0].hunger = 0.2f;
        bool wentForIt = false;
        for (int t = 0; t < 6000; t++) {
            Tick(); NeedsTick(); ProcessFreetimeNeeds();
            if (movers[0].freetimeState == FREETIME_SEEKING_FOOD && movers[0].needTarget == snack) wentForIt = true;
        }
        expect(!wentForIt);
        expect(items[snack].stackCount == 5 && items[snack].reservedBy == -1);
    }

    it("a thirsty mover pays for a soda") {
        Setup();
        thirstEnabled = true;
        StockShop(ITEM_SODA, 5);
        MoverAddCoins(0, 5);
        movers[0].thirst = 0.2f;
        float before = movers[0].thirst;
        for (int t = 0; t < 20000 && movers[0].thirst <= before + 0.05f; t++) { Tick(); NeedsTick(); ProcessFreetimeNeeds(); }
        expect(movers[0].thirst > before + 0.05f);
        expect(MoverCoins(0) == 3 && stockpileTill[shop] == 2);
    }

    it("E on shop food: can't pay says so; with money it buys, then eats") {
        Setup();
        hungerEnabled = true;
        movers[0].hunger = 0.5f;
        int snack = StockShop(ITEM_SNACK, 5);
        char said[128];
        expect(!QueueCommandAt(0, 10, 2, 0, false, said, sizeof(said)));
        expect(strstr(said, "can't pay") != NULL);
        MoverAddCoins(0, 3);
        expect(QueueCommandAt(0, 10, 2, 0, false, said, sizeof(said)));
        for (int t = 0; t < 6000 && QueueBusy(0); t++) { Tick(); NeedsTick(); ProcessFreetimeNeeds(); }
        expect(MoverCoins(0) == 0 && stockpileTill[shop] == 3);
        expect(items[snack].stackCount == 4);
    }

    it("a new or deleted stockpile sells nothing; clearing resets every shop") {
        Setup();
        StockShop(ITEM_SNACK, 5);
        stockpileTill[shop] = 9;
        DeleteStockpile(shop);
        expect(!stockpileForSale[shop] && stockpileTill[shop] == 0 && ShopCount() == 0);
        int again = CreateStockpile(10, 2, 0, 2, 1);
        expect(!stockpileForSale[again]);
        ShopSetForSale(again, true);
        ClearStockpiles();
        expect(ShopCount() == 0 && !stockpileForSale[again]);
    }

    it("shops and tills survive a save and load (v121)") {
        Setup();
        StockShop(ITEM_SNACK, 5);
        stockpileTill[shop] = 42;
        expect(SaveWorld("/tmp/test_money_shop.bin"));
        ClearStockpiles(); ClearItems(); ClearMovers();
        expect(LoadWorld("/tmp/test_money_shop.bin"));
        expect(stockpileForSale[shop] && stockpileTill[shop] == 42 && ShopCount() == 1);
    }
}

// The bottle machine at (6,2) next to the shop at (10,2); mover 0 carries `cans` in a backpack
static int machine;
static void SetupMachine(int cans, int till) {
    Setup();
    shop = CreateStockpile(10, 2, 0, 2, 1);
    ShopSetForSale(shop, true);
    stockpileTill[shop] = till;
    machine = SpawnFurniture(6, 2, 0, FURNITURE_BOTTLE_MACHINE, MAT_NONE);
    MoverWear(0, SpawnItem(movers[0].x, movers[0].y, 0.0f, ITEM_BACKPACK));
    for (int i = 0; i < cans; i++) MoverStow(0, SpawnItem(movers[0].x, movers[0].y, 0.0f, ITEM_EMPTY_CAN));
}

static void RunQueueOut(int maxTicks) {
    for (int t = 0; t < maxTicks && QueueBusy(0); t++) { Tick(); ProcessFreetimeNeeds(); }
}

describe(bottle_machine) {
    it("pays the deposit for every can from the nearest shop's till; the cans are gone") {
        SetupMachine(4, 20);
        expect(BottleMachineShop(machine) == shop);
        expect(MoverDepositUnits(0) == 4);
        char said[128];
        expect(QueueCommandAt(0, 6, 2, 0, false, said, sizeof(said)));
        expect(strstr(said, "cash in 4 cans") != NULL);
        RunQueueOut(6000);
        expect(!QueueBusy(0));
        expect(MoverDepositUnits(0) == 0);
        expect(MoverCoins(0) == 4 && stockpileTill[shop] == 16);
        int cansLeft = 0;
        for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == ITEM_EMPTY_CAN) cansLeft++;
        expect(cansLeft == 0);
        expect(RunStateAudit(test_verbose) == 0);
    }

    it("a short till pays what it has; the rest of the cans stay in the bag") {
        SetupMachine(4, 2);
        char said[128];
        expect(QueueCommandAt(0, 6, 2, 0, false, said, sizeof(said)));
        RunQueueOut(6000);
        expect(MoverCoins(0) == 2 && stockpileTill[shop] == 0);
        expect(MoverDepositUnits(0) == 2);
    }

    it("no shop in range: out of order, nothing paid") {
        SetupMachine(3, 20);
        ShopSetForSale(shop, false);
        expect(BottleMachineShop(machine) == -1);
        char said[128];
        expect(QueueCommandAt(0, 6, 2, 0, false, said, sizeof(said)));
        RunQueueOut(6000);
        expect(MoverCoins(0) == 0 && MoverDepositUnits(0) == 3);
    }

    it("E on the machine with no cans says so; Shift+E after a bin search is allowed") {
        SetupMachine(0, 20);
        char said[128];
        expect(!QueueCommandAt(0, 6, 2, 0, false, said, sizeof(said)));
        expect(strstr(said, "no cans") != NULL);
        int bin = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, ITEM_TRASH_BIN);
        for (int i = 0; i < 2; i++) PutItemInContainer(SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, ITEM_EMPTY_CAN), bin);
        expect(QueueCommandAt(0, 3, 3, 0, false, said, sizeof(said)));
        expect(QueueCommandAt(0, 6, 2, 0, true, said, sizeof(said)));
        RunQueueOut(20000);
        expect(MoverCoins(0) == 2 && stockpileTill[shop] == 18 && MoverDepositUnits(0) == 0);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(purse);
    test(coins_in_the_world);
    test(pick_up_order);
    test(money_save);
    test(shops);
    test(bottle_machine);
    return summary();
}
