#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdbool.h>
#include "../world/material.h"
#include "../entities/mover.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 62

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

// V48 Item struct (before stackCount field added in v49)
typedef struct {
    float x, y, z;
    ItemType type;
    ItemState state;
    uint8_t material;
    bool natural;
    bool active;
    int reservedBy;
    float unreachableCooldown;
} ItemV48;

// V49 Item struct (before containedIn/contentCount/contentTypeMask fields added in v50)
typedef struct {
    float x, y, z;
    ItemType type;
    ItemState state;
    uint8_t material;
    bool natural;
    bool active;
    int reservedBy;
    float unreachableCooldown;
    int stackCount;
} ItemV49;

// Version 50 constants (before container items: basket, clay pot, chest)
#define V50_ITEM_TYPE_COUNT 28

// V50 Stockpile struct (before container item types added)
// v50 had 28 item types, v51 adds ITEM_BASKET, ITEM_CLAY_POT, ITEM_CHEST at end (indices 28-30)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V50_ITEM_TYPE_COUNT];
    bool allowedMaterials[MAT_COUNT];
    bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int maxStackSize;
    int priority;
    int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int freeSlotCount;
} StockpileV50;

// V51 Stockpile struct (before maxContainers field added in v52)
// Same as current Stockpile minus maxContainers field
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[ITEM_TYPE_COUNT];
    bool allowedMaterials[MAT_COUNT];
    bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int maxStackSize;
    int priority;
    int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int freeSlotCount;
} StockpileV51;

// V52 Mover struct (before energy field added in v53)
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
    float hunger;
    // No energy field in V52
    int freetimeState;
    int needTarget;
    float needProgress;
    float needSearchCooldown;
    float avoidX, avoidY;
    int currentJobId;
    int lastJobType;
    int lastJobResult;
    int lastJobTargetX, lastJobTargetY, lastJobTargetZ;
    unsigned long lastJobEndTick;
    MoverCapabilities capabilities;
} MoverV52;

// Version 54 constants (before plank bed/chair items)
#define V54_ITEM_TYPE_COUNT 31

// V54 Stockpile struct (before plank bed/chair addition)
// v54 had 31 item types, v55 adds ITEM_PLANK_BED and ITEM_CHAIR at end (indices 31-32)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V54_ITEM_TYPE_COUNT];
    bool allowedMaterials[MAT_COUNT];
    bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int maxStackSize;
    int priority;
    int maxContainers;
    bool slotIsContainer[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int freeSlotCount;
} StockpileV54;

// V57 Mover struct (before starvationTimer field added in v58)
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
    float hunger;
    float energy;
    int freetimeState;
    int needTarget;
    float needProgress;
    float needSearchCooldown;
    // No starvationTimer in V57
    float avoidX, avoidY;
    int currentJobId;
    int lastJobType;
    int lastJobResult;
    int lastJobTargetX, lastJobTargetY, lastJobTargetZ;
    unsigned long lastJobEndTick;
    MoverCapabilities capabilities;
} MoverV57;

// V58 Mover struct (before bodyTemp/hypothermiaTimer fields added in v59)
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
    float hunger;
    float energy;
    int freetimeState;
    int needTarget;
    float needProgress;
    float needSearchCooldown;
    float starvationTimer;
    // No bodyTemp/hypothermiaTimer in V58
    float avoidX, avoidY;
    int currentJobId;
    int lastJobType;
    int lastJobResult;
    int lastJobTargetX, lastJobTargetY, lastJobTargetZ;
    unsigned long lastJobEndTick;
    MoverCapabilities capabilities;
} MoverV58;

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

// Version 60 constants (before sharp stone item)
#define V60_ITEM_TYPE_COUNT 33

// V60 Stockpile struct (before ITEM_SHARP_STONE addition)
// v60 had 33 item types, v61 adds ITEM_SHARP_STONE at end (index 33)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V60_ITEM_TYPE_COUNT];
    bool allowedMaterials[MAT_COUNT];
    bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int maxStackSize;
    int priority;
    int maxContainers;
    bool slotIsContainer[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
    int freeSlotCount;
} StockpileV60;

#endif
