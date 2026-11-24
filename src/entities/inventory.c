// inventory.c - hands, back and pockets (see inventory.h)
#include "inventory.h"
#include "mover.h"
#include "items.h"
#include "item_defs.h"
#include "containers.h"
#include "stockpiles.h"
#include "tool_quality.h"
#include "jobs.h"
#include "../core/event_log.h"

static bool ValidItem(int i) { return i >= 0 && i < MAX_ITEMS && items[i].active; }

float carryLimitKg = 30.0f;
static float carried[MAX_MOVERS];   // not saved: RecalcAllCarried after a load
static int setAside[MAX_MOVERS][2]; // put down for a job (not saved); valid while reservedBy the mover
static bool setAsideInit = false;

static void InitSetAside(void) {
    if (setAsideInit) return;
    for (int i = 0; i < MAX_MOVERS; i++) setAside[i][0] = setAside[i][1] = -1;
    setAsideInit = true;
}

int MoverJobCarry(int moverIdx) {
    const Mover* m = &movers[moverIdx];
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job) return -1;
    int it = job->carryingItem;
    if (it < 0 && job->step == CRAFT_STEP_CARRYING_FUEL) it = job->fuelItem;
    if (it < 0 || it >= MAX_ITEMS || !items[it].active || items[it].state != ITEM_CARRIED) return -1;
    return it;
}

// What an item weighs with everything inside it
static float TotalWeight(int it) { return ValidItem(it) ? GetContainerTotalWeight(it) : 0.0f; }

void MoverRecalcCarried(int moverIdx) {
    const Mover* m = &movers[moverIdx];
    float w = TotalWeight(m->hands[0]);
    if (m->hands[1] != m->hands[0]) w += TotalWeight(m->hands[1]);   // a two-handed item once
    w += INV_BACK_WEIGHT_FACTOR * TotalWeight(m->wornBack);
    if (ValidItem(m->equippedClothing))   // the clothing itself is worn, not carried: its pockets are
        w += TotalWeight(m->equippedClothing) - ItemWeight(items[m->equippedClothing].type);
    carried[moverIdx] = w;
}

// The cached inventory weight plus a job's load (looked up live: it changes in jobs.c)
float MoverCarriedWeight(int moverIdx) {
    int job = MoverJobCarry(moverIdx);
    return carried[moverIdx] + (job >= 0 ? TotalWeight(job) : 0.0f);
}

void RecalcAllCarried(void) { for (int i = 0; i < moverCount; i++) MoverRecalcCarried(i); }

// Would `addKg` more (already weighted for where it goes) stay within the limit?
static bool CanCarryMore(int moverIdx, float addKg) {
    return MoverCarriedWeight(moverIdx) + addKg <= carryLimitKg + 0.001f;
}

static int HandsEmpty(int moverIdx) {
    const Mover* m = &movers[moverIdx];
    return (m->hands[0] < 0) + (m->hands[1] < 0);
}

int MoverFreeHands(int moverIdx) {
    int free = HandsEmpty(moverIdx);
    int job = MoverJobCarry(moverIdx);
    if (job >= 0) free -= ItemIsTwoHanded(items[job].type) ? 2 : 1;
    return free < 0 ? 0 : free;
}

void MoverMakeHandsFor(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return;
    InitSetAside();
    Mover* m = &movers[moverIdx];
    int need = ItemIsTwoHanded(items[itemIdx].type) ? 2 : 1;
    for (int h = 1; h >= 0 && HandsEmpty(moverIdx) < need; h--) {
        int it = m->hands[h];
        if (it < 0) continue;
        if (m->hands[0] == it) m->hands[0] = -1;
        if (m->hands[1] == it) m->hands[1] = -1;
        SafeDropItem(it, m->x, m->y, (int)m->z);
        if (items[it].contentCount > 0) MoveContainer(it, items[it].x, items[it].y, items[it].z);
        items[it].reservedBy = moverIdx;   // still its own: nobody hauls it off
        int slot = setAside[moverIdx][0] < 0 ? 0 : 1;
        if (setAside[moverIdx][slot] >= 0) MoverForgetSetAside(moverIdx, setAside[moverIdx][slot]);
        setAside[moverIdx][slot] = it;
        EventLog("Mover %d puts down %s (item %d) to carry %s", moverIdx, ItemName(items[it].type), it, ItemName(items[itemIdx].type));
    }
    MoverRecalcCarried(moverIdx);
}

int MoverSetAsideItem(int moverIdx) {
    InitSetAside();
    for (int k = 0; k < 2; k++) {
        int it = setAside[moverIdx][k];
        if (it < 0) continue;
        if (!ValidItem(it) || items[it].reservedBy != moverIdx || items[it].state != ITEM_ON_GROUND) {
            setAside[moverIdx][k] = -1;   // gone, or someone else's now
            continue;
        }
        return it;
    }
    return -1;
}

void MoverForgetSetAside(int moverIdx, int itemIdx) {
    InitSetAside();
    for (int k = 0; k < 2; k++) {
        if (setAside[moverIdx][k] != itemIdx) continue;
        setAside[moverIdx][k] = -1;
        if (ValidItem(itemIdx) && items[itemIdx].reservedBy == moverIdx) items[itemIdx].reservedBy = -1;
    }
}

bool ItemIsMoverGear(int moverIdx, int itemIdx) {
    const Mover* m = &movers[moverIdx];
    if (itemIdx < 0) return false;
    if (itemIdx == m->equippedTool || itemIdx == m->equippedClothing || itemIdx == m->wornBack ||
        itemIdx == m->hands[0] || itemIdx == m->hands[1]) return true;
    InitSetAside();
    return setAside[moverIdx][0] == itemIdx || setAside[moverIdx][1] == itemIdx;
}

void MoverResetInventoryCache(int moverIdx) {
    InitSetAside();
    setAside[moverIdx][0] = setAside[moverIdx][1] = -1;
    carried[moverIdx] = 0.0f;
}

// Out of wherever it is (stockpile slot, a container) and onto the mover, reserved by it
static void TakeOntoMover(int moverIdx, int itemIdx) {
    Item* it = &items[itemIdx];
    if (it->state == ITEM_IN_STOCKPILE) RemoveItemFromStockpileSlot(it->x, it->y, (int)it->z);
    if (it->containedIn >= 0) {
        int parent = it->containedIn;
        RemoveItemFromContainer(itemIdx);
        SyncStockpileContainerSlotCount(parent);
    }
    const Mover* m = &movers[moverIdx];
    it->state = ITEM_CARRIED;
    it->reservedBy = moverIdx;
    it->x = m->x; it->y = m->y; it->z = m->z;
}

bool MoverHold(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return false;
    Mover* m = &movers[moverIdx];
    bool both = ItemIsTwoHanded(items[itemIdx].type);
    if (MoverFreeHands(moverIdx) < (both ? 2 : 1)) return false;
    if (!CanCarryMore(moverIdx, TotalWeight(itemIdx))) return false;   // too heavy to lift as well
    TakeOntoMover(moverIdx, itemIdx);
    if (both) { m->hands[0] = m->hands[1] = itemIdx; }
    else if (m->hands[0] < 0) m->hands[0] = itemIdx;
    else m->hands[1] = itemIdx;
    EventLog("Mover %d holds %s (item %d)%s", moverIdx, ItemName(items[itemIdx].type), itemIdx, both ? " in both hands" : "");
    MoverRecalcCarried(moverIdx);
    return true;
}

// Let go of an item the mover held or wore: on the ground at its feet, contents along
static void LetGo(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return;
    const Mover* m = &movers[moverIdx];
    items[itemIdx].reservedBy = -1;
    SafeDropItem(itemIdx, m->x, m->y, (int)m->z);
    if (items[itemIdx].contentCount > 0) MoveContainer(itemIdx, items[itemIdx].x, items[itemIdx].y, items[itemIdx].z);
}

void MoverDropHand(int moverIdx, int hand) {
    if (hand < 0 || hand > 1) return;
    Mover* m = &movers[moverIdx];
    int it = m->hands[hand];
    if (it < 0) return;
    if (m->hands[0] == it) m->hands[0] = -1;   // a two-handed item leaves both hands
    if (m->hands[1] == it) m->hands[1] = -1;
    EventLog("Mover %d dropped held item %d", moverIdx, it);
    LetGo(moverIdx, it);
    MoverRecalcCarried(moverIdx);
}

bool MoverWear(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return false;
    Mover* m = &movers[moverIdx];
    ItemType t = items[itemIdx].type;
    if (ItemIsWornOnBack(t)) {
        MoverDropBack(moverIdx);
        TakeOntoMover(moverIdx, itemIdx);
        m->wornBack = itemIdx;
    } else if (ItemIsClothing(t)) {
        DropEquippedClothing(moverIdx);
        TakeOntoMover(moverIdx, itemIdx);
        m->equippedClothing = itemIdx;
    } else {
        return false;
    }
    EventLog("Mover %d wears %s (item %d)", moverIdx, ItemName(t), itemIdx);
    MoverRecalcCarried(moverIdx);
    return true;
}

void MoverDropBack(int moverIdx) {
    Mover* m = &movers[moverIdx];
    int it = m->wornBack;
    if (it < 0) return;
    m->wornBack = -1;
    LetGo(moverIdx, it);
    MoverRecalcCarried(moverIdx);
}

// Into this container if it is one and has room (it is on the mover, so no access check)
static bool TryInto(int moverIdx, int container, int itemIdx, float weightFactor) {
    if (!ValidItem(container) || container == itemIdx) return false;
    if (!GetContainerDef(items[container].type)) return false;
    if (!CanPutItemInContainer(itemIdx, container)) return false;
    if (!CanCarryMore(moverIdx, weightFactor * TotalWeight(itemIdx))) return false;
    // Out of a stockpile slot / other container first; PutItemInContainer sets the state
    Item* it = &items[itemIdx];
    if (it->state == ITEM_IN_STOCKPILE) RemoveItemFromStockpileSlot(it->x, it->y, (int)it->z);
    if (it->containedIn >= 0) {
        int parent = it->containedIn;
        RemoveItemFromContainer(itemIdx);
        SyncStockpileContainerSlotCount(parent);
    }
    it->reservedBy = -1;
    PutItemInContainer(itemIdx, container);
    return true;
}

InvPlace MoverStow(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return INV_NONE;
    Mover* m = &movers[moverIdx];
    if (ItemInMoverHands(moverIdx, itemIdx)) return INV_HAND;
    InvPlace place = INV_NONE;
    if (TryInto(moverIdx, m->equippedClothing, itemIdx, 1.0f)) place = INV_POCKETS;
    else if (TryInto(moverIdx, m->wornBack, itemIdx, INV_BACK_WEIGHT_FACTOR)) place = INV_BACK;
    else if (TryInto(moverIdx, m->hands[0], itemIdx, 1.0f) ||
             (m->hands[1] != m->hands[0] && TryInto(moverIdx, m->hands[1], itemIdx, 1.0f))) place = INV_HELD_BAG;
    else if (MoverHold(moverIdx, itemIdx)) place = INV_HAND;
    if (place != INV_NONE) MoverRecalcCarried(moverIdx);
    if (place != INV_NONE && place != INV_HAND)
        EventLog("Mover %d stowed %s (item %d) in %s", moverIdx, ItemName(items[itemIdx].type), itemIdx,
                 place == INV_POCKETS ? "pockets" : place == INV_BACK ? "backpack" : "held bag");
    return place;
}

void MoverDropAll(int moverIdx) {
    MoverDropHand(moverIdx, 0);
    MoverDropHand(moverIdx, 1);
    MoverDropBack(moverIdx);
    InitSetAside();
    for (int k = 0; k < 2; k++) if (setAside[moverIdx][k] >= 0) MoverForgetSetAside(moverIdx, setAside[moverIdx][k]);
}

void MoverSyncInventory(int moverIdx) {
    const Mover* m = &movers[moverIdx];
    int slots[4] = { m->hands[0], m->hands[1], m->wornBack, m->equippedClothing };
    for (int k = 0; k < 4; k++) {
        if (!ValidItem(slots[k])) continue;
        items[slots[k]].x = m->x; items[slots[k]].y = m->y; items[slots[k]].z = m->z;
    }
    // Contents keep their old position until the container is let go (LetGo moves them):
    // moving them every tick would scan all items per mover
    // Weight: things inside also change without us (eaten, spoiled away): once a second
    if ((currentTick + (unsigned long)moverIdx) % 60 == 0) MoverRecalcCarried(moverIdx);
}

int MoverHandHolding(int moverIdx, int itemType) {
    const Mover* m = &movers[moverIdx];
    for (int h = 0; h < 2; h++)
        if (ValidItem(m->hands[h]) && (int)items[m->hands[h]].type == itemType) return h;
    return -1;
}

bool MoverPutHeldInto(int moverIdx, int hand, int containerIdx) {
    if (hand < 0 || hand > 1) return false;
    Mover* m = &movers[moverIdx];
    int it = m->hands[hand];
    if (!ValidItem(it) || !ValidItem(containerIdx) || !CanPutItemInContainer(it, containerIdx)) return false;
    if (m->hands[0] == it) m->hands[0] = -1;
    if (m->hands[1] == it) m->hands[1] = -1;
    items[it].reservedBy = -1;
    PutItemInContainer(it, containerIdx);
    MoverRecalcCarried(moverIdx);
    return true;
}

bool MoverTakeOut(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return false;
    if (ItemInMoverHands(moverIdx, itemIdx)) {
        Mover* m = &movers[moverIdx];
        int hand = m->hands[0] == itemIdx ? 0 : 1;
        MoverDropHand(moverIdx, hand);
        return true;
    }
    if (!ItemIsInMoverInventory(moverIdx, itemIdx) || items[itemIdx].containedIn < 0) return false;
    MoverSyncInventory(moverIdx);   // containers where the mover is: it lands at its feet
    RemoveItemFromContainer(itemIdx);
    MoverRecalcCarried(moverIdx);
    return true;
}

bool ItemInMoverHands(int moverIdx, int itemIdx) {
    const Mover* m = &movers[moverIdx];
    return itemIdx >= 0 && (m->hands[0] == itemIdx || m->hands[1] == itemIdx);
}

bool ItemIsInMoverInventory(int moverIdx, int itemIdx) {
    if (!ValidItem(itemIdx)) return false;
    const Mover* m = &movers[moverIdx];
    int top = GetOutermostContainer(itemIdx);
    return top == m->hands[0] || top == m->hands[1] || top == m->wornBack || top == m->equippedClothing;
}
