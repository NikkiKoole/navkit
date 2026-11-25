// money.c - purses and coins on the ground (money.h)
#include "money.h"
#include "items.h"
#include "../core/event_log.h"
#include <stdio.h>
#include <string.h>

int moverCents[MAX_MOVERS];

void MoneyResetAll(void) {
    memset(moverCents, 0, sizeof(moverCents));
}

void MoverAddCents(int moverIdx, int cents) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return;
    long v = (long)moverCents[moverIdx] + cents;
    if (v < 0) v = 0;
    if (v > 100000000L) v = 100000000L;   // a million guilders is plenty
    moverCents[moverIdx] = (int)v;
}

bool MoverPay(int moverIdx, int cents) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS || cents < 0) return false;
    if (moverCents[moverIdx] < cents) return false;
    moverCents[moverIdx] -= cents;
    return true;
}

int SpawnCoins(float x, float y, float z, int cents) {
    if (cents <= 0) return -1;
    int it = SpawnItem(x, y, z, ITEM_COIN);
    if (it < 0) return -1;
    items[it].stackCount = cents;
    return it;
}

int MoverDropCents(int moverIdx, int cents) {
    if (moverIdx < 0 || moverIdx >= moverCount || !movers[moverIdx].active) return -1;
    if (cents > moverCents[moverIdx]) cents = moverCents[moverIdx];
    if (cents <= 0) return -1;
    Mover* m = &movers[moverIdx];
    int it = SpawnCoins(m->x, m->y, m->z, cents);
    if (it < 0) return -1;
    moverCents[moverIdx] -= cents;
    EventLog("Mover %d dropped %d cents (item %d)", moverIdx, cents, it);
    return it;
}

bool MoverPocketCoins(int moverIdx, int coinItem) {
    if (coinItem < 0 || coinItem >= MAX_ITEMS || !items[coinItem].active || items[coinItem].type != ITEM_COIN) return false;
    MoverAddCents(moverIdx, items[coinItem].stackCount);
    EventLog("Mover %d picked up %d cents (item %d)", moverIdx, items[coinItem].stackCount, coinItem);
    DeleteItem(coinItem);
    return true;
}

const char* MoneyFormat(int cents, char* buf, int len) {
    if (cents < 0) cents = 0;
    snprintf(buf, (size_t)len, "fl %d,%02d", cents / 100, cents % 100);   // guilders (florin), as written in 1999
    return buf;
}
