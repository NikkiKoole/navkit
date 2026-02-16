#include "stacking.h"
#include "item_defs.h"

int MergeItemIntoStack(int existingIdx, int incomingIdx) {
    if (existingIdx < 0 || existingIdx >= MAX_ITEMS) return 0;
    if (incomingIdx < 0 || incomingIdx >= MAX_ITEMS) return 0;
    if (!items[existingIdx].active || !items[incomingIdx].active) return 0;
    if (existingIdx == incomingIdx) return 0;

    int maxStack = ItemMaxStack(items[existingIdx].type);
    int room = maxStack - items[existingIdx].stackCount;
    if (room <= 0) return 0;

    int toMerge = items[incomingIdx].stackCount;
    if (toMerge <= room) {
        // Full merge — incoming item is consumed
        items[existingIdx].stackCount += toMerge;
        DeleteItem(incomingIdx);
        return toMerge;
    } else {
        // Partial merge — incoming item keeps the remainder
        items[existingIdx].stackCount = maxStack;
        items[incomingIdx].stackCount -= room;
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

    return newIdx;
}
