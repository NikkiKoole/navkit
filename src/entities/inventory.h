// inventory.h - what a mover has: worn, held, and what is inside those
// (docs/vision/city-1999.md "Inventory"). No inventory list of its own: a mover has what
// is in its hands (two; a big item takes both), on its back (a backpack) and on its body
// (equippedClothing; a pocket jacket has pockets), plus what is inside those containers.
// Worn and held items are ITEM_CARRIED, reservedBy the mover (like the equipped tool);
// what is inside them is ITEM_IN_CONTAINER, and IsItemAccessible keeps other movers out.
#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

typedef enum {
    INV_NONE = 0,     // didn't fit anywhere
    INV_POCKETS,      // in the clothing's pockets
    INV_BACK,         // in the backpack
    INV_HELD_BAG,     // in a container held in a hand
    INV_HAND,         // loose in a free hand
} InvPlace;

int  MoverFreeHands(int moverIdx);                 // 0, 1 or 2 (a job's load in hand counts)
// Jobs and hands: what a job carries sits in the hands (a log in both). Before a job picks
// something up, MoverMakeHandsFor puts down what is in the way (a bag) at the mover's feet,
// reserved for the mover; afterwards the mover goes back for it (needs.c, FREETIME_FETCHING_OWN).
// The set-aside list is saved (Mover.setAside); a load gives the mover its reservations back.
int  MoverJobCarry(int moverIdx);                  // item a job carries now (load or fuel), -1 = none
void MoverMakeHandsFor(int moverIdx, int itemIdx);
int  MoverSetAsideItem(int moverIdx);              // something put down for a job, still there; -1 = none
void MoverForgetSetAside(int moverIdx, int itemIdx);   // gave up on it: free for anyone
bool ItemIsMoverGear(int moverIdx, int itemIdx);   // tool, clothing, back, hands, set aside: its own
bool MoverHold(int moverIdx, int itemIdx);         // into a free hand (both for a two-handed item)
void MoverDropHand(int moverIdx, int hand);        // drop what that hand holds (with its contents)
bool MoverWear(int moverIdx, int itemIdx);         // backpack on the back, clothing on the body (old one dropped)
void MoverDropBack(int moverIdx);
// Put an item away: pockets, then backpack, then a bag in a hand, then a free hand.
// The item may lie anywhere (ground, stockpile, a container): it is taken out first.
InvPlace MoverStow(int moverIdx, int itemIdx);
void MoverDropAll(int moverIdx);                   // hands and back (death); clothing via DropEquippedClothing
void MoverSyncInventory(int moverIdx);             // held/worn items follow the mover (each tick)
// Carried weight (kg), weighted by where it sits: hands and pockets in full, the back at
// INV_BACK_WEIGHT_FACTOR (a backpack carries easier). Kept up to date by the functions
// here (and every second in MoverSyncInventory). Above carryLimitKg nothing more is taken.
#define INV_BACK_WEIGHT_FACTOR 0.5f
extern float carryLimitKg;
float MoverCarriedWeight(int moverIdx);
void  MoverRecalcCarried(int moverIdx);
void  RecalcAllCarried(void);                     // after a load
void  MoverResetInventoryCache(int moverIdx);      // a new mover in this slot
void  ReserveAllMoverGear(void);                   // after a load: reservations were cleared
// Walking speed factor from carried weight (same curve as a job's load: 1 / (1 + 0.02 kg))
static inline float CarriedSpeedFactor(int moverIdx) { return 1.0f / (1.0f + MoverCarriedWeight(moverIdx) * 0.02f); }
bool ItemInMoverHands(int moverIdx, int itemIdx);
int  MoverHandHolding(int moverIdx, int itemType);  // hand (0/1) holding an item of that type, -1 = none
bool MoverPutHeldInto(int moverIdx, int hand, int containerIdx);   // from the hand into a container (if it has room)
// Take an item out of the mover's inventory (a pocket, the backpack, a held bag, a hand) and
// put it down at the mover's feet (to eat it there)
bool MoverTakeOut(int moverIdx, int itemIdx);
bool ItemIsInMoverInventory(int moverIdx, int itemIdx);   // held/worn, or inside one of those

#endif
