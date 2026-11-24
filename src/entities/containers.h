#ifndef CONTAINERS_H
#define CONTAINERS_H

#include "items.h"
#include <stdbool.h>

// Container properties (indexed by ItemType for container items)
typedef struct {
    int maxContents;        // max distinct stacks inside
    float spoilageModifier; // spoilage rate multiplier (1.0 = normal)
    bool weatherProtection; // blocks rain spoilage
    bool acceptsLiquids;    // can hold liquid items
    float capacityL;        // litres it holds (0 = no volume limit: only maxContents stacks)
} ContainerDef;

// Container definitions table (indexed by ItemType, NULL entries for non-containers)
extern ContainerDef containerDefs[ITEM_TYPE_COUNT];

// Returns container properties, or NULL if itemType is not a container
const ContainerDef* GetContainerDef(ItemType type);

// Core operations
// Contents lists (Item.firstChild / nextSibling). Every change of containedIn goes through
// these two: they keep containedIn, contentCount, contentTypeMask and the list in step.
void ContainerLink(int itemIdx, int containerIdx);
void ContainerUnlink(int itemIdx);
void RebuildContainerLists(void);     // from containedIn (after a load)
// Direct contents: for (int c = FirstInContainer(k); c >= 0; c = NextInContainer(c))
static inline int FirstInContainer(int containerIdx) { return items[containerIdx].firstChild; }
static inline int NextInContainer(int itemIdx) { return items[itemIdx].nextSibling; }

bool CanPutItemInContainer(int itemIdx, int containerIdx);
// Volume: one item with what is inside it, and what a container holds now (litres)
float ItemTotalVolume(int itemIdx);
float ContainerContentsVolume(int containerIdx);
bool ContainerHasRoomFor(int containerIdx, ItemType type, uint8_t material);  // a free slot or a stack to merge into
void PutItemInContainer(int itemIdx, int containerIdx);
void RemoveItemFromContainer(int itemIdx);

// Queries (O(1))
bool IsContainerFull(int containerIdx);
int GetContainerContentCount(int containerIdx);
bool ContainerMightHaveType(int containerIdx, ItemType type);

// Accessibility — walks containedIn chain, false if any ancestor reserved or carried
bool IsItemAccessible(int itemIdx);

// Recursive operations
void MoveContainer(int containerIdx, float x, float y, float z);
void SpillContainerContents(int containerIdx);

// Iteration
typedef void (*ContainerContentCallback)(int itemIdx, void* data);
void ForEachContainedItem(int containerIdx, ContainerContentCallback cb, void* data);
void ForEachContainedItemRecursive(int containerIdx, ContainerContentCallback cb, void* data);

// Weight (recursive — container + all contents)
float GetContainerTotalWeight(int containerIdx);

// Walk up containedIn chain, return topmost container (or itemIdx if not contained)
int GetOutermostContainer(int itemIdx);

// Search containers in range for an item matching type.
// Uses bloom filter for fast reject, IsItemAccessible to skip locked containers.
// extraFilter is optional (NULL = no extra filtering).
// Returns contained item index, sets *outContainerIdx to outermost container (-1 if not found).
typedef bool (*ContainerItemFilter)(int itemIdx, void* data);
int FindItemInContainers(ItemType type, int z, int searchCenterX, int searchCenterY,
                         int searchRadius, int excludeItemIdx,
                         ContainerItemFilter extraFilter, void* filterData,
                         int* outContainerIdx);

#endif
