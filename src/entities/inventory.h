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

int  MoverFreeHands(int moverIdx);                 // 0, 1 or 2
bool MoverHold(int moverIdx, int itemIdx);         // into a free hand (both for a two-handed item)
void MoverDropHand(int moverIdx, int hand);        // drop what that hand holds (with its contents)
bool MoverWear(int moverIdx, int itemIdx);         // backpack on the back, clothing on the body (old one dropped)
void MoverDropBack(int moverIdx);
// Put an item away: pockets, then backpack, then a bag in a hand, then a free hand.
// The item may lie anywhere (ground, stockpile, a container): it is taken out first.
InvPlace MoverStow(int moverIdx, int itemIdx);
void MoverDropAll(int moverIdx);                   // hands and back (death); clothing via DropEquippedClothing
void MoverSyncInventory(int moverIdx);             // held/worn items follow the mover (each tick)
bool ItemInMoverHands(int moverIdx, int itemIdx);
int  MoverHandHolding(int moverIdx, int itemType);  // hand (0/1) holding an item of that type, -1 = none
bool MoverPutHeldInto(int moverIdx, int hand, int containerIdx);   // from the hand into a container (if it has room)
// Take an item out of the mover's inventory (a pocket, the backpack, a held bag, a hand) and
// put it down at the mover's feet (to eat it there)
bool MoverTakeOut(int moverIdx, int itemIdx);
bool ItemIsInMoverInventory(int moverIdx, int itemIdx);   // held/worn, or inside one of those

#endif
