#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdbool.h>
#include "../world/material.h"
#include "../entities/mover.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 48

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

// V47 Mover struct (before hunger/needs fields added in v48)
// Must match the Mover struct layout from save version 47 exactly.
typedef struct {
    float x, y, z;
    Point goal;
    Point path[MAX_MOVER_PATH];
    int pathLength;
    int pathIndex;
    bool active;
    bool needsRepath;
    int repathCooldown;
    float speed;
    float timeNearWaypoint;
    float lastX, lastY, lastZ;
    float timeWithoutProgress;
    float fallTimer;
    float workAnimPhase;
    // No hunger/needs fields in V47
    float avoidX, avoidY;
    int currentJobId;
    int lastJobType;
    int lastJobResult;
    int lastJobTargetX, lastJobTargetY, lastJobTargetZ;
    unsigned long lastJobEndTick;
    MoverCapabilities capabilities;
} MoverV47;

// Version 47 constants (before berries items)
#define V47_ITEM_TYPE_COUNT 26

// V47 Stockpile struct (before berries addition)
// v47 had 26 item types, v48 adds ITEM_BERRIES and ITEM_DRIED_BERRIES at end (indices 26-27)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V47_ITEM_TYPE_COUNT];
    bool allowedMaterials[MAT_COUNT];
    int maxStackSize;
} StockpileV47;

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
