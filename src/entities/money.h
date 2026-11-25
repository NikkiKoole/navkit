// money.h - money (City 1999, docs/vision/city-1999.md "Money and exchange")
//
// Money is a number on whoever holds it: a mover's purse (moverCents[i], whole cents). It only
// becomes an item (ITEM_COIN, stackCount = cents) when it lies in the world: dropped, lost,
// thrown in a hat. Picking it up makes it a number again. That keeps money physical where it
// matters without one item per purse (the item cap). Saved after the action queues (v119).
#ifndef MONEY_H
#define MONEY_H

#include <stdbool.h>
#include "mover.h"

extern int moverCents[MAX_MOVERS];

void MoneyResetAll(void);                        // new world / old save: every purse empty
static inline int MoverCents(int moverIdx) { return moverCents[moverIdx]; }
void MoverAddCents(int moverIdx, int cents);     // negative takes away (never below 0)
bool MoverPay(int moverIdx, int cents);          // false (nothing paid): not enough
// Drop cents from the purse as a coin item at the mover's feet; returns the item, -1 = none
// (nothing in the purse). Drops at most what it has.
int MoverDropCents(int moverIdx, int cents);
// Coins on the ground at x,y,z (pixels) as one item; -1 = cents <= 0 or no item slot
int SpawnCoins(float x, float y, float z, int cents);
// The coin item into the mover's purse (the item is gone afterwards); false: not a coin
bool MoverPocketCoins(int moverIdx, int coinItem);
// "fl 3,25" (the 1999 guilder: fl = florin; the font has no ƒ); buf at least 16
const char* MoneyFormat(int cents, char* buf, int len);

#endif
