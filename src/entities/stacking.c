#include "stacking.h"
#include "item_defs.h"
#include "../core/event_log.h"

int MergeItemIntoStack(int existingIdx, int incomingIdx) {
    if (existingIdx < 0 || existingIdx >= MAX_ITEMS) return 0;
    if (incomingIdx < 0 || incomingIdx >= MAX_ITEMS) return 0;
    if (!items[existingIdx].active || !items[incomingIdx].active) return 0;
    if (existingIdx == incomingIdx) return 0;

    int maxStack = ItemMaxStack(items[existingIdx].type);
    int room = maxStack - items[existingIdx].stackCount;
    if (room <= 0) return 0;

    int toMerge = items[incomingIdx].stackCount;
    // Spoilage: take the worse (higher) timer on merge
    if (items[incomingIdx].spoilageTimer > items[existingIdx].spoilageTimer) {
        items[existingIdx].spoilageTimer = items[incomingIdx].spoilageTimer;
    }
    // Condition: take the worse (higher) condition on merge
    if (items[incomingIdx].condition > items[existingIdx].condition) {
        items[existingIdx].condition = items[incomingIdx].condition;
    }
    if (toMerge <= room) {
        // Full merge — incoming item is consumed
        items[existingIdx].stackCount += toMerge;
        EventLog("Stack merge: item %d (%s) absorbed item %d (x%d), now x%d",
                 existingIdx, ItemName(items[existingIdx].type), incomingIdx, toMerge, items[existingIdx].stackCount);
        DeleteItem(incomingIdx);
        return toMerge;
    } else {
        // Partial merge — incoming item keeps the remainder
        items[existingIdx].stackCount = maxStack;
        items[incomingIdx].stackCount -= room;
        EventLog("Stack partial merge: item %d (%s) took %d from item %d, now x%d / x%d",
                 existingIdx, ItemName(items[existingIdx].type), room, incomingIdx, items[existingIdx].stackCount, items[incomingIdx].stackCount);
        return room;
    }
}

int SplitStack(int itemIdx, int count) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS) return -1;
    if (!items[itemIdx].active) return -1;
    if (count <= 0 || count >= items[itemIdx].stackCount) return -1;

    items[itemIdx].stackCount -= count;

    int newIdx = SpawnItemWithMaterial(
        items[itemIdx].x, items[itemIdx].y, items[itemIdx].z,
        items[itemIdx].type, items[itemIdx].material
    );
    if (newIdx < 0) return -1;

    items[newIdx].stackCount = count;
    items[newIdx].spoilageTimer = items[itemIdx].spoilageTimer;
    items[newIdx].condition = items[itemIdx].condition;
    items[newIdx].natural = items[itemIdx].natural;
    // New item inherits state from original (ground, stockpile, etc.)
    items[newIdx].state = items[itemIdx].state;

    // If inside a container, new item is also inside the same container
    if (items[itemIdx].containedIn != -1) {
        items[newIdx].containedIn = items[itemIdx].containedIn;
        items[newIdx].state = ITEM_IN_CONTAINER;
        // Increment parent's contentCount (new item entry in container)
        int parentIdx = items[itemIdx].containedIn;
        if (parentIdx >= 0 && parentIdx < MAX_ITEMS && items[parentIdx].active) {
            items[parentIdx].contentCount++;
            // contentTypeMask already has this type's bit set
        }
    }

    EventLog("Stack split: item %d (%s) split off x%d as item %d, remainder x%d",
             itemIdx, ItemName(items[itemIdx].type), count, newIdx, items[itemIdx].stackCount);
    return newIdx;
}
