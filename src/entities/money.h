// money.h - money (City 1999, docs/vision/city-1999.md "Money and exchange")
//
// Money counts in whole coins. It is a number on whoever holds it: a mover's purse
// (moverCoins[i]). It only becomes an item (ITEM_COIN, stackCount = coins) when it lies in the
// world: dropped, lost, thrown in a hat. Picking it up makes it a number again. That keeps money
// physical where it matters without one item per purse (the item cap). Saved after the action
// queues (v119).
#ifndef MONEY_H
#define MONEY_H

#include <stdbool.h>
#include "mover.h"

extern int moverCoins[MAX_MOVERS];

void MoneyResetAll(void);                        // new world / old save: every purse empty
static inline int MoverCoins(int moverIdx) { return moverCoins[moverIdx]; }
void MoverAddCoins(int moverIdx, int coins);     // negative takes away (never below 0)
bool MoverPay(int moverIdx, int coins);          // false (nothing paid): not enough
// Drop coins from the purse as a coin item at the mover's feet; returns the item, -1 = none
// (nothing in the purse). Drops at most what it has.
int MoverDropCoins(int moverIdx, int coins);
// Coins on the ground at x,y,z (pixels) as one item; -1 = coins <= 0 or no item slot
int SpawnCoins(float x, float y, float z, int coins);
// The coin item into the mover's purse (the item is gone afterwards); false: not a coin
bool MoverPocketCoins(int moverIdx, int coinItem);
// "3 coins", "1 coin"; buf at least 24
const char* MoneyFormat(int coins, char* buf, int len);

#endif
