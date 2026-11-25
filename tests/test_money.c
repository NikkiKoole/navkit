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

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(purse);
    test(coins_in_the_world);
    test(pick_up_order);
    test(money_save);
    return summary();
}
