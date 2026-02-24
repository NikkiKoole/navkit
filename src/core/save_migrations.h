#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdbool.h>
#include "../world/material.h"
#include "../entities/mover.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 75

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

// Version 65 constants (before tool item types: digging stick, stone axe/pick/hammer)
#define V65_ITEM_TYPE_COUNT 34

// V65 Stockpile struct (before tool item types added)
// v65 had 34 item types, v66 adds ITEM_DIGGING_STICK, ITEM_STONE_AXE, ITEM_STONE_PICK, ITEM_STONE_HAMMER (indices 34-37)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V65_ITEM_TYPE_COUNT];
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
} StockpileV65;

// Version 66 constants (before CELL_DOOR and WORKSHOP_GROUND_FIRE)
#define V66_CELL_TYPE_COUNT 17

// V62 Blueprint struct (before workshop construction fields added in v63)
// Must match exact layout: no workshopOriginX/Y, workshopType fields
#include "../world/construction.h"
typedef struct {
    int x, y, z;
    bool active;
    int state;  // BlueprintState enum

    int recipeIndex;
    int stage;
    StageDelivery stageDeliveries[MAX_INPUTS_PER_STAGE];
    ConsumedRecord consumedItems[MAX_CONSTRUCTION_STAGES][MAX_INPUTS_PER_STAGE];

    int assignedBuilder;
    float progress;
    // No workshopOriginX/Y, workshopType in V62
} BlueprintV62;

// V64 Mover struct (before equippedTool field added in v65)
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
    float bodyTemp;
    float hypothermiaTimer;
    float avoidX, avoidY;
    int currentJobId;
    int lastJobType;
    int lastJobResult;
    int lastJobTargetX, lastJobTargetY, lastJobTargetZ;
    unsigned long lastJobEndTick;
    MoverCapabilities capabilities;
    // No equippedTool in V64
} MoverV64;

// V68 Mover struct (before path extraction in v69)
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
    float bodyTemp;
    float hypothermiaTimer;
    float avoidX, avoidY;
    int currentJobId;
    int lastJobType;
    int lastJobResult;
    int lastJobTargetX, lastJobTargetY, lastJobTargetZ;
    unsigned long lastJobEndTick;
    MoverCapabilities capabilities;
    int equippedTool;
} MoverV68;

// Version 69 constants (before root items: root, roasted root, dried root)
#define V69_ITEM_TYPE_COUNT 42

// V69 Stockpile struct (before root item types added)
// v69 had 42 item types, v70 adds ITEM_ROOT, ITEM_ROASTED_ROOT, ITEM_DRIED_ROOT (indices 42-44)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V69_ITEM_TYPE_COUNT];
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
} StockpileV69;

// Version 67 constants (before butchering items: carcass, raw meat, cooked meat, hide)
#define V67_ITEM_TYPE_COUNT 38

// V67 Stockpile struct (before butchering item types added)
// v67 had 38 item types, v68 adds ITEM_CARCASS, ITEM_RAW_MEAT, ITEM_COOKED_MEAT, ITEM_HIDE (indices 38-41)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V67_ITEM_TYPE_COUNT];
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
} StockpileV67;

// V70 Item struct (before spoilageTimer field added in v71)
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
    int containedIn;
    int contentCount;
    uint32_t contentTypeMask;
} ItemV70;

// Version 71 constants (before ITEM_ROT + MAT_ROTTEN_MEAT/PLANT in v72)
#define V71_ITEM_TYPE_COUNT 45
#define V71_MAT_COUNT 17

// V71 Stockpile struct (before ITEM_ROT and rot materials added in v72)
// v71 had 45 item types and 17 materials
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V71_ITEM_TYPE_COUNT];
    bool allowedMaterials[V71_MAT_COUNT];
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
} StockpileV71;

// Version 72 constants (before condition field on Item, before ITEM_ROT/MAT_ROTTEN removal in v73)
#define V72_ITEM_TYPE_COUNT 46
#define V72_MAT_COUNT 19

// V72 Item struct (before condition field added in v73)
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
    int containedIn;
    int contentCount;
    uint32_t contentTypeMask;
    float spoilageTimer;
    // No condition field in V72
} ItemV72;

// V72 Stockpile struct (before ITEM_ROT removal + rejectsRotten addition in v73)
// v72 had 46 item types (including ITEM_ROT at index 45) and 19 materials
// (including MAT_ROTTEN_MEAT at 16 and MAT_ROTTEN_PLANT at 17)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V72_ITEM_TYPE_COUNT];
    bool allowedMaterials[V72_MAT_COUNT];
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
    // No rejectsRotten in V72
} StockpileV72;

// V63 Workshop struct (before markedForDeconstruct/assignedDeconstructor fields added in v64)
#include "../entities/workshops.h"
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    WorkshopType type;
    char template[MAX_WORKSHOP_SIZE * MAX_WORKSHOP_SIZE];
    Bill bills[MAX_BILLS_PER_WORKSHOP];
    int billCount;
    int assignedCrafter;
    float passiveProgress;
    int passiveBillIdx;
    bool passiveReady;
    WorkshopVisualState visualState;
    float inputStarvationTime;
    float outputBlockedTime;
    float lastWorkTime;
    int workTileX, workTileY;
    int outputTileX, outputTileY;
    int fuelTileX, fuelTileY;
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedInputCount;
    // No markedForDeconstruct, assignedDeconstructor in V63
} WorkshopV63;

#endif
