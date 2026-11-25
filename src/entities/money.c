// money.c - purses and coins on the ground (money.h)
#include "money.h"
#include "items.h"
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
