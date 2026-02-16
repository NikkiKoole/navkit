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
} ContainerDef;

// Container definitions table (indexed by ItemType, NULL entries for non-containers)
extern ContainerDef containerDefs[ITEM_TYPE_COUNT];

// Returns container properties, or NULL if itemType is not a container
const ContainerDef* GetContainerDef(ItemType type);

// Core operations
bool CanPutItemInContainer(int itemIdx, int containerIdx);
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

#endif
