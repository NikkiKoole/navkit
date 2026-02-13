#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdbool.h>
#include "../world/material.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 45

// ============================================================================
// SAVE MIGRATION PATTERN (for future use when backward compatibility needed)
// ============================================================================
//
// When the save format changes (add/remove fields, change enum sizes, etc.):
// 1. Bump CURRENT_SAVE_VERSION
// 2. Add version constants here (e.g., #define V22_ITEM_TYPE_COUNT 24)
// 3. Define legacy struct typedefs here (e.g., typedef struct { ... } StockpileV22;)
// 4. In LoadWorld(), add migration path: if (version >= 23) { /* new format */ } else { /* V22 migration */ }
// 5. Update inspect.c with same migration logic using same structs from this header
//
// This ensures saveload.c and inspect.c can't drift - they share the same legacy definitions.
//
// Currently in development: only V22 is supported, old saves will error out cleanly.
// ============================================================================

// Version 31 constants (before sapling/leaf consolidation)
#define V31_ITEM_TYPE_COUNT 28

// V31 Stockpile struct (before sapling/leaf consolidation)
// v31 had 8 separate types: ITEM_SAPLING_OAK/PINE/BIRCH/WILLOW at indices 16-19,
// ITEM_LEAVES_OAK/PINE/BIRCH/WILLOW at indices 20-23
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V31_ITEM_TYPE_COUNT];  // OLD: 28 bools
    bool allowedMaterials[MAT_COUNT];
    int maxStackSize;
} StockpileV31;

// Version 32 constants (before bark/stripped log items)
#define V32_ITEM_TYPE_COUNT 22

// V32 Stockpile struct (before bark/stripped log addition)
// v32 had 22 item types, v33 adds ITEM_BARK and ITEM_STRIPPED_LOG at end (indices 22-23)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V32_ITEM_TYPE_COUNT];  // OLD: 22 bools
    bool allowedMaterials[MAT_COUNT];
    int maxStackSize;
} StockpileV32;

// Version 34 constants (before short string/cordage items)
#define V34_ITEM_TYPE_COUNT 24

// V34 Stockpile struct (before short string/cordage addition)
// v33/v34 had 24 item types, v35 adds ITEM_SHORT_STRING and ITEM_CORDAGE at end (indices 24-25)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V34_ITEM_TYPE_COUNT];  // OLD: 24 bools
    bool allowedMaterials[MAT_COUNT];
    int maxStackSize;
} StockpileV34;

#endif
