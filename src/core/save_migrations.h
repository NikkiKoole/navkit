#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "../entities/items.h"
#include "../world/material.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 22

// Legacy ITEM_TYPE_COUNT values for each version that changed the item enum
#define V19_ITEM_TYPE_COUNT 23  // Before ITEM_BRICKS/ITEM_CHARCOAL were added
#define V18_ITEM_TYPE_COUNT 21  // Before ITEM_PLANKS/ITEM_STICKS were added
#define V17_ITEM_TYPE_COUNT 21  // Same item count as V18 (no enum changes between V17 and V18)
#define V16_ITEM_TYPE_COUNT 21  // Same item count as V18 (no enum changes between V16 and V18)

// Legacy Stockpile struct for version 19
// (includes allowedMaterials but before item type enum growth to 24)
typedef struct {
    bool allowedTypes[V19_ITEM_TYPE_COUNT];
    bool allowedMaterials[MAT_COUNT];
} Stockpile_V19_AllowedArrays;

// Legacy Stockpile struct for version 18
// (before allowedMaterials was added)
typedef struct {
    bool allowedTypes[V18_ITEM_TYPE_COUNT];
} Stockpile_V18_AllowedArrays;

#endif
