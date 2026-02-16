#include "containers.h"
#include "item_defs.h"
#include "stacking.h"
#include "mover.h"  // CELL_SIZE

// Container definitions table — entries with maxContents=0 mean "not a container".
ContainerDef containerDefs[ITEM_TYPE_COUNT] = {
    [ITEM_BASKET]   = { .maxContents = 15, .spoilageModifier = 1.0f, .weatherProtection = false, .acceptsLiquids = false },
    [ITEM_CHEST]    = { .maxContents = 20, .spoilageModifier = 0.7f, .weatherProtection = true,  .acceptsLiquids = false },
    [ITEM_CLAY_POT] = { .maxContents = 5,  .spoilageModifier = 0.5f, .weatherProtection = true,  .acceptsLiquids = true  },
};

const ContainerDef* GetContainerDef(ItemType type) {
    if (type < 0 || type >= ITEM_TYPE_COUNT) return NULL;
    if (containerDefs[type].maxContents <= 0) return NULL;
    return &containerDefs[type];
}

bool CanPutItemInContainer(int itemIdx, int containerIdx) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS) return false;
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return false;
    if (!items[itemIdx].active || !items[containerIdx].active) return false;
    if (itemIdx == containerIdx) return false;

    const ContainerDef* def = GetContainerDef(items[containerIdx].type);
    if (!def) return false;

    // Don't allow putting a container inside itself (cycle check: walk up chain)
    int ancestor = items[containerIdx].containedIn;
    while (ancestor != -1) {
        if (ancestor == itemIdx) return false;  // would create cycle
        ancestor = items[ancestor].containedIn;
    }

    // Check if there's a matching stack to merge into
    ItemType inType = items[itemIdx].type;
    uint8_t inMat = items[itemIdx].material;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;
        if (items[i].type == inType && items[i].material == inMat) {
            // Can merge if stack has room
            int maxStack = ItemMaxStack(inType);
            if (items[i].stackCount < maxStack) return true;
        }
    }

    // No merge target — need a new slot
    return items[containerIdx].contentCount < def->maxContents;
}

void PutItemInContainer(int itemIdx, int containerIdx) {
    if (!CanPutItemInContainer(itemIdx, containerIdx)) return;

    // Look for existing stack to merge into
    ItemType inType = items[itemIdx].type;
    uint8_t inMat = items[itemIdx].material;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;
        if (items[i].type == inType && items[i].material == inMat) {
            int maxStack = ItemMaxStack(inType);
            if (items[i].stackCount < maxStack) {
                // Merge — incoming item is consumed (fully or partially)
                MergeItemIntoStack(i, itemIdx);
                // MergeItemIntoStack handles DeleteItem on full merge
                // which decrements contentCount via DeleteItem's containedIn check.
                // If partial merge, incoming item still exists — put remainder in container.
                if (items[itemIdx].active) {
                    // Partial merge: remainder becomes a new entry in container
                    items[itemIdx].containedIn = containerIdx;
                    items[itemIdx].state = ITEM_IN_CONTAINER;
                    items[itemIdx].x = items[containerIdx].x;
                    items[itemIdx].y = items[containerIdx].y;
                    items[itemIdx].z = items[containerIdx].z;
                    items[containerIdx].contentCount++;
                    items[containerIdx].contentTypeMask |= (1u << (inType % 32));
                }
                return;
            }
        }
    }

    // No merge target — add as new entry
    items[itemIdx].containedIn = containerIdx;
    items[itemIdx].state = ITEM_IN_CONTAINER;
    items[itemIdx].x = items[containerIdx].x;
    items[itemIdx].y = items[containerIdx].y;
    items[itemIdx].z = items[containerIdx].z;
    items[containerIdx].contentCount++;
    items[containerIdx].contentTypeMask |= (1u << (inType % 32));
}

void RemoveItemFromContainer(int itemIdx) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS) return;
    if (!items[itemIdx].active) return;
    if (items[itemIdx].containedIn == -1) return;

    int parentIdx = items[itemIdx].containedIn;

    // Find outermost container for position
    int outerIdx = parentIdx;
    while (items[outerIdx].containedIn != -1) {
        outerIdx = items[outerIdx].containedIn;
    }

    // Decrement direct parent's contentCount
    if (parentIdx >= 0 && parentIdx < MAX_ITEMS && items[parentIdx].active) {
        items[parentIdx].contentCount--;
    }

    // Remove from container
    items[itemIdx].containedIn = -1;
    items[itemIdx].state = ITEM_ON_GROUND;

    // Position at outermost container's location via safe drop
    SafeDropItem(itemIdx, items[outerIdx].x, items[outerIdx].y, (int)items[outerIdx].z);
}

bool IsContainerFull(int containerIdx) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return true;
    if (!items[containerIdx].active) return true;
    const ContainerDef* def = GetContainerDef(items[containerIdx].type);
    if (!def) return true;
    return items[containerIdx].contentCount >= def->maxContents;
}

int GetContainerContentCount(int containerIdx) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return 0;
    return items[containerIdx].contentCount;
}

bool ContainerMightHaveType(int containerIdx, ItemType type) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return false;
    if (type < 0 || type >= ITEM_TYPE_COUNT) return false;
    return (items[containerIdx].contentTypeMask & (1u << (type % 32))) != 0;
}

bool IsItemAccessible(int itemIdx) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS) return false;
    if (!items[itemIdx].active) return false;

    // Walk up containedIn chain — if any ancestor is reserved or carried, not accessible
    int current = items[itemIdx].containedIn;
    while (current != -1) {
        if (!items[current].active) return false;
        if (items[current].reservedBy != -1) return false;
        if (items[current].state == ITEM_CARRIED) return false;
        current = items[current].containedIn;
    }
    return true;
}

void MoveContainer(int containerIdx, float x, float y, float z) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return;
    if (!items[containerIdx].active) return;

    items[containerIdx].x = x;
    items[containerIdx].y = y;
    items[containerIdx].z = z;

    // Recursively update all direct children
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;
        // If this child is also a container, recurse
        if (items[i].contentCount > 0) {
            MoveContainer(i, x, y, z);
        } else {
            items[i].x = x;
            items[i].y = y;
            items[i].z = z;
        }
    }
}

void SpillContainerContents(int containerIdx) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return;
    if (!items[containerIdx].active) return;

    // Spill direct children only — sub-containers keep their contents
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;

        items[i].containedIn = -1;
        items[i].state = ITEM_ON_GROUND;
        SafeDropItem(i, items[containerIdx].x, items[containerIdx].y, (int)items[containerIdx].z);
    }

    items[containerIdx].contentCount = 0;
    items[containerIdx].contentTypeMask = 0;
}

void ForEachContainedItem(int containerIdx, ContainerContentCallback cb, void* data) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;
        cb(i, data);
    }
}

static void ForEachRecursiveHelper(int containerIdx, ContainerContentCallback cb, void* data) {
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;
        cb(i, data);
        // Recurse into sub-containers
        if (items[i].contentCount > 0) {
            ForEachRecursiveHelper(i, cb, data);
        }
    }
}

void ForEachContainedItemRecursive(int containerIdx, ContainerContentCallback cb, void* data) {
    ForEachRecursiveHelper(containerIdx, cb, data);
}

static void WeightSumCallback(int itemIdx, void* data) {
    float* total = (float*)data;
    *total += ItemWeight(items[itemIdx].type) * items[itemIdx].stackCount;
    // Recurse into sub-containers (their own weight is already counted by the callback)
}

float GetContainerTotalWeight(int containerIdx) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return 0.0f;
    if (!items[containerIdx].active) return 0.0f;

    float total = ItemWeight(items[containerIdx].type) * items[containerIdx].stackCount;
    ForEachContainedItemRecursive(containerIdx, WeightSumCallback, &total);
    return total;
}

int GetOutermostContainer(int itemIdx) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS) return itemIdx;
    int current = itemIdx;
    while (items[current].containedIn != -1) {
        current = items[current].containedIn;
    }
    return current;
}

// Search inside a single container (and nested sub-containers) for matching item
static int SearchContainerForItem(int containerIdx, ItemType type, int excludeItemIdx,
                                  ContainerItemFilter extraFilter, void* filterData,
                                  int* outContainerIdx) {
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].containedIn != containerIdx) continue;

        // Check sub-containers recursively
        if (items[i].contentCount > 0 && ContainerMightHaveType(i, type)) {
            int found = SearchContainerForItem(i, type, excludeItemIdx,
                                               extraFilter, filterData, outContainerIdx);
            if (found >= 0) return found;
        }

        // Check this item
        if (items[i].type != type) continue;
        if (i == excludeItemIdx) continue;
        if (items[i].reservedBy != -1) continue;
        if (items[i].unreachableCooldown > 0.0f) continue;
        if (extraFilter && !extraFilter(i, filterData)) continue;

        if (outContainerIdx) *outContainerIdx = GetOutermostContainer(containerIdx);
        return i;
    }
    return -1;
}

int FindItemInContainers(ItemType type, int z, int searchCenterX, int searchCenterY,
                         int searchRadius, int excludeItemIdx,
                         ContainerItemFilter extraFilter, void* filterData,
                         int* outContainerIdx) {
    if (outContainerIdx) *outContainerIdx = -1;

    int bestItemIdx = -1;
    int bestContainerIdx = -1;
    int bestDistSq = searchRadius * searchRadius;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (!GetContainerDef(items[i].type)) continue;
        if (items[i].contentCount <= 0) continue;
        if ((int)items[i].z != z) continue;

        // Only search top-level containers (not nested ones — those are reached via recursion)
        if (items[i].containedIn != -1) continue;

        // Distance check (tile coords)
        int tileX = (int)(items[i].x / CELL_SIZE);
        int tileY = (int)(items[i].y / CELL_SIZE);
        int dx = tileX - searchCenterX;
        int dy = tileY - searchCenterY;
        int distSq = dx * dx + dy * dy;
        if (distSq > bestDistSq) continue;

        // Bloom filter fast reject — only for direct children.
        // Skip this check entirely since nested containers mean the outer's
        // bloom filter may not include grandchild types. The per-level bloom
        // filter check in SearchContainerForItem handles sub-containers.

        // Skip if container is reserved or carried
        if (items[i].reservedBy != -1) continue;
        if (items[i].state == ITEM_CARRIED) continue;

        // Search inside this container
        int foundContainer = -1;
        int found = SearchContainerForItem(i, type, excludeItemIdx,
                                           extraFilter, filterData, &foundContainer);
        if (found >= 0 && distSq < bestDistSq) {
            bestDistSq = distSq;
            bestItemIdx = found;
            bestContainerIdx = foundContainer;
        }
    }

    if (outContainerIdx) *outContainerIdx = bestContainerIdx;
    return bestItemIdx;
}
