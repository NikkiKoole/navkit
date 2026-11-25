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
#include "stockpiles.h"

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

// ---------------------------------------------------------------------------
// Shops (step 2): a stockpile can sell what lies in it. Taking a unit for yourself (eating,
// drinking, picking it up) pays its price (ItemPrice, item_defs.c) into the stockpile's till.
// Price 0 = free, and a stockpile that isn't for sale is free as before. Saved right after the
// stockpiles (v121). Haulers and crafters still take from a shop for free (later).
extern bool stockpileForSale[MAX_STOCKPILES];
extern int stockpileTill[MAX_STOCKPILES];
void ShopResetAll(void);                         // no shops, empty tills
void ShopReset(int sp);                          // a new / deleted stockpile: not for sale, till empty
void ShopSetForSale(int sp, bool forSale);
int ShopCount(void);                             // stockpiles for sale (0: every lookup is skipped)
void ShopRecount(void);                          // after writing stockpileForSale directly (a load)
int ItemShop(int item);                          // the stockpile selling this item, -1 = not for sale
int ItemShopPrice(int item);                     // what taking one unit costs (0 = free)
bool MoverCanAfford(int moverIdx, int item);     // free, or the purse holds the price
// Take one unit for yourself: free -> true; for sale -> pays into the till (true), or false
// (nothing paid) when the purse is short
bool MoverBuy(int moverIdx, int item);

// ---------------------------------------------------------------------------
// Bottle machine (step 3, FURNITURE_BOTTLE_MACHINE): hand in empty cans for their deposit
// (ItemDeposit), paid from the till of the nearest shop on its level within
// BOTTLE_MACHINE_SHOP_RANGE cells (a shop that sells nothing can't pay deposits). The cans
// go into the machine and are gone (later: it fills up and a truck empties it).
#define BOTTLE_MACHINE_SHOP_RANGE 12
int ItemDeposit(int type);                       // coins a machine pays for one unit (0 = takes none)
int BottleMachineShop(int furnitureIdx);         // the stockpile whose till pays, -1 = none
int MoverDepositUnits(int moverIdx);             // cans it carries (hands, pockets, bags)
// Hand in one unit: true = paid (the unit is gone, the coins in the purse); false: nothing to
// hand in or the till is short
bool MoverCashInOne(int moverIdx, int furnitureIdx);

#endif
