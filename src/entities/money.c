// money.c - purses and coins on the ground (money.h)
#include "money.h"
#include "items.h"
#include "item_defs.h"
#include "stockpiles.h"
#include "furniture.h"
#include "inventory.h"
#include "../core/event_log.h"
#include <stdio.h>
#include <string.h>

int moverCoins[MAX_MOVERS];

void MoneyResetAll(void) {
    memset(moverCoins, 0, sizeof(moverCoins));
}

void MoverAddCoins(int moverIdx, int coins) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return;
    long v = (long)moverCoins[moverIdx] + coins;
    if (v < 0) v = 0;
    if (v > 1000000L) v = 1000000L;   // a million coins is plenty
    moverCoins[moverIdx] = (int)v;
}

bool MoverPay(int moverIdx, int coins) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS || coins < 0) return false;
    if (moverCoins[moverIdx] < coins) return false;
    moverCoins[moverIdx] -= coins;
    return true;
}

int SpawnCoins(float x, float y, float z, int coins) {
    if (coins <= 0) return -1;
    int it = SpawnItem(x, y, z, ITEM_COIN);
    if (it < 0) return -1;
    items[it].stackCount = coins;
    return it;
}

int MoverDropCoins(int moverIdx, int coins) {
    if (moverIdx < 0 || moverIdx >= moverCount || !movers[moverIdx].active) return -1;
    if (coins > moverCoins[moverIdx]) coins = moverCoins[moverIdx];
    if (coins <= 0) return -1;
    Mover* m = &movers[moverIdx];
    int it = SpawnCoins(m->x, m->y, m->z, coins);
    if (it < 0) return -1;
    moverCoins[moverIdx] -= coins;
    EventLog("Mover %d dropped %d coins (item %d)", moverIdx, coins, it);
    return it;
}

bool MoverPocketCoins(int moverIdx, int coinItem) {
    if (coinItem < 0 || coinItem >= MAX_ITEMS || !items[coinItem].active || items[coinItem].type != ITEM_COIN) return false;
    MoverAddCoins(moverIdx, items[coinItem].stackCount);
    EventLog("Mover %d picked up %d coins (item %d)", moverIdx, items[coinItem].stackCount, coinItem);
    DeleteItem(coinItem);
    return true;
}

const char* MoneyFormat(int coins, char* buf, int len) {
    if (coins < 0) coins = 0;
    snprintf(buf, (size_t)len, "%d coin%s", coins, coins == 1 ? "" : "s");
    return buf;
}

// ---------------------------------------------------------------------------
// Shops
// ---------------------------------------------------------------------------

bool stockpileForSale[MAX_STOCKPILES];
int stockpileTill[MAX_STOCKPILES];
static int shopsForSale;   // how many stockpiles are for sale: 0 skips every lookup (no game pays)

void ShopRecount(void) {
    shopsForSale = 0;
    for (int i = 0; i < MAX_STOCKPILES; i++) if (stockpileForSale[i]) shopsForSale++;
}

void ShopResetAll(void) {
    memset(stockpileForSale, 0, sizeof(stockpileForSale));
    memset(stockpileTill, 0, sizeof(stockpileTill));
    shopsForSale = 0;
}

void ShopReset(int sp) {
    if (sp < 0 || sp >= MAX_STOCKPILES) return;
    stockpileForSale[sp] = false;
    stockpileTill[sp] = 0;
    ShopRecount();
}

void ShopSetForSale(int sp, bool forSale) {
    if (sp < 0 || sp >= MAX_STOCKPILES) return;
    stockpileForSale[sp] = forSale;
    ShopRecount();
}

int ShopCount(void) { return shopsForSale; }

int ItemShop(int item) {
    if (shopsForSale == 0) return -1;
    if (item < 0 || item >= MAX_ITEMS || !items[item].active || items[item].state != ITEM_IN_STOCKPILE) return -1;
    int sp = -1;
    if (!IsPositionInStockpile(items[item].x, items[item].y, (int)items[item].z, &sp)) return -1;
    return (sp >= 0 && stockpileForSale[sp]) ? sp : -1;
}

int ItemShopPrice(int item) {
    if (ItemShop(item) < 0) return 0;
    return ItemPrice(items[item].type);
}

bool MoverCanAfford(int moverIdx, int item) {
    int price = ItemShopPrice(item);
    return price <= 0 || MoverCoins(moverIdx) >= price;
}

bool MoverBuy(int moverIdx, int item) {
    int sp = ItemShop(item);
    if (sp < 0) return true;
    int price = ItemPrice(items[item].type);
    if (price <= 0) return true;
    if (!MoverPay(moverIdx, price)) return false;
    stockpileTill[sp] += price;
    EventLog("Mover %d bought %s for %d coins at stockpile %d (till %d)", moverIdx, ItemName(items[item].type), price, sp, stockpileTill[sp]);
    return true;
}

// ---------------------------------------------------------------------------
// Bottle machine
// ---------------------------------------------------------------------------

int ItemDeposit(int type) {
    return type == ITEM_EMPTY_CAN ? ItemPrice(type) : 0;
}

int BottleMachineShop(int f) {
    if (f < 0 || f >= MAX_FURNITURE || !furniture[f].active || furniture[f].type != FURNITURE_BOTTLE_MACHINE) return -1;
    int best = -1, bestD = BOTTLE_MACHINE_SHOP_RANGE + 1;
    for (int i = 0; i < stockpileHighWater; i++) {
        if (!stockpiles[i].active || !stockpileForSale[i] || stockpiles[i].z != furniture[f].z) continue;
        const Stockpile* sp = &stockpiles[i];
        int dx = furniture[f].x < sp->x ? sp->x - furniture[f].x : (furniture[f].x >= sp->x + sp->width ? furniture[f].x - (sp->x + sp->width - 1) : 0);
        int dy = furniture[f].y < sp->y ? sp->y - furniture[f].y : (furniture[f].y >= sp->y + sp->height ? furniture[f].y - (sp->y + sp->height - 1) : 0);
        int d = dx > dy ? dx : dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// A deposit unit the mover carries, -1 = none
static int MoneyCarriedDepositItem(int moverIdx) {
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && ItemDeposit(items[i].type) > 0 && ItemIsInMoverInventory(moverIdx, i)) return i;
    return -1;
}

int MoverDepositUnits(int moverIdx) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && ItemDeposit(items[i].type) > 0 && ItemIsInMoverInventory(moverIdx, i)) n += items[i].stackCount;
    return n;
}

bool MoverCashInOne(int moverIdx, int f) {
    int sp = BottleMachineShop(f);
    if (sp < 0) return false;
    int it = MoneyCarriedDepositItem(moverIdx);
    if (it < 0) return false;
    int pay = ItemDeposit(items[it].type);
    if (stockpileTill[sp] < pay) return false;
    stockpileTill[sp] -= pay;
    MoverAddCoins(moverIdx, pay);
    Mover* m = &movers[moverIdx];
    if (items[it].stackCount > 1) items[it].stackCount--;
    else {
        if (m->hands[0] == it) m->hands[0] = -1;   // a can held in the hand
        if (m->hands[1] == it) m->hands[1] = -1;
        DeleteItem(it);
    }
    MoverRecalcCarried(moverIdx);
    return true;
}
