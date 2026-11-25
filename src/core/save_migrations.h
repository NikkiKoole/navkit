#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "../world/material.h"
#include "../entities/mover.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 111

// Minimum supported save version (older saves are rejected)
#define MIN_SAVE_VERSION 82

// ============================================================================
// SAVE MIGRATION PATTERN
// ============================================================================
//
// When the save format changes (add/remove fields, change enum sizes, etc.):
// 1. Bump CURRENT_SAVE_VERSION
// 2. Add version constants here (e.g., #define V90_ITEM_TYPE_COUNT 72)
// 3. Define legacy struct typedefs here (e.g., typedef struct { ... } StockpileV90;)
// 4. In LoadWorld(), add migration path: if (version >= 91) { /* new format */ } else { /* V90 migration */ }
// 5. Update inspect.c with same migration logic using same structs from this header
//
// This ensures saveload.c and inspect.c can't drift - they share the same legacy definitions.
// ============================================================================

// v94: grid (CellType) stored as uint8_t, targetHeight as uint8_t, wearGrid as uint16_t.
// Before v94 each was saved as 4-byte int per cell. These read n legacy ints into the
// narrow array (values are in range: CellType < 256, tree heights < depth, wear <= wearMax).
#define V94_NARROW_GRID_LAYERS 94
// v95: each per-cell layer in the grid section is run-length coded (save_rle.h).
#define V95_RLE_GRID_LAYERS 95
// v96: TempCell.offset (was .current) is the temperature minus the level's ambient, not the
// absolute temperature: cells at ambient follow the day/season cycle without per-cell work.
// Older saves: stable non-source cells were at ambient (offset 0); others keep their
// temperature at the ambient of the moment they load.
#define V96_TEMP_OFFSETS 96
// v97: Train gets freight (cargo item indices, their source priority, cargoCount) at
// the end of the struct; older saves read TrainV96 (no freight)
#define V97_TRAIN_CARGO 97
// v98: conveyor belts (belts.c) after furniture at the end of the entities section:
// beltCount + Belt[beltCount] (raw structs; next/fed are rebuilt by BeltsAfterLoad).
// Older saves: no belts.
#define V98_BELTS 98
// v99: train settings + endlessMoverMode in the settings section, after diurnalAmplitude
// (trainSpeed float; useTrainTimeChoice, useTrainTransfers, trainFreight, endlessMoverMode
// bool). Older saves: the defaults.
#define V99_TRAIN_SETTINGS 99
// v100: elevator settings after the v99 train settings (elevatorSpeed, elevatorDoorTime
// float; elevatorCapacity int; useElevatorWaitCost bool). Older saves: the defaults.
#define V100_ELEVATOR_SETTINGS 100
// v101: map-edge water after the v100 elevator settings (waterEdgeOutflow bool,
// waterEdgeSeaZ int). Older saves: outflow on, sea height from the edge sources the island
// used to place (none: -1).
#define V101_WATER_EDGE 101
// v102: tide settings after the v101 water-edge fields (tideEnabled bool, tidePeriodHours
// float, tideLowLevel int). The sea itself rides in the water cells (WaterCell.isSea, a bit
// that was spare: 0 in older saves). Older saves: no tide.
#define V102_TIDE 102
// v103: springs (springTrickle bool, springRate float, after the tide settings). Older saves:
// sources are taps (as they were saved).
#define V103_SPRINGS 103
// v104: trash. Five item types (soda, snack, empty can, wrapper, trash bin: stockpile
// allowedTypes grows from V103_ITEM_TYPE_COUNT) and Mover.heldTrash at the end of the Mover.
// Older saves: new types not allowed in old stockpiles, hands free.
#define V104_TRASH 104
#define V103_ITEM_TYPE_COUNT 73
#define MOVER_V103_SIZE 344   // sizeof(Mover) in v93-v103 (ends with bladder)
_Static_assert(offsetof(Mover, heldTrash) == MOVER_V103_SIZE, "Mover v103 layout must be a prefix of Mover");
// v105: MAT_CONCRETE (stockpile allowedMaterials grows from V104_MAT_COUNT). CELL_RAILING is
// appended to CellType too, but nothing saved is sized by the cell count. Older saves: old
// stockpiles allow the new material.
#define V105_CONCRETE 105
#define V104_MAT_COUNT 19
#define V104_ITEM_TYPE_COUNT 78
_Static_assert(MAT_CONCRETE == V104_MAT_COUNT, "v104 materials end where MAT_CONCRETE starts");
// v106: inventory. Three item types (pocket jacket, backpack, plastic bag: stockpile
// allowedTypes grows from V105_ITEM_TYPE_COUNT) and Mover.hands[2] + Mover.wornBack at the end
// of the Mover. Older saves: hands free, nothing on the back.
#define V106_INVENTORY 106
#define V105_ITEM_TYPE_COUNT 78
#define V105_MAT_COUNT 20
#define MOVER_V105_RECORD 352   // sizeof(Mover) in v104-v105 (heldTrash + 4 bytes padding)
#define MOVER_V105_DATA   348   // the part of it that holds fields (up to and with heldTrash)
_Static_assert(offsetof(Mover, hands) == MOVER_V105_DATA, "Mover v105 layout must be a prefix of Mover");
_Static_assert(MAT_COUNT == V105_MAT_COUNT, "v106 changes no materials: add a migration if MAT_COUNT grew");
// v107: scrounging. Two item types (burger, leftovers: stockpile allowedTypes grows from
// V106_ITEM_TYPE_COUNT). New FreetimeState values and a trait are appended (no size change).
#define V107_SCROUNGE 107
#define V106_ITEM_TYPE_COUNT 81
// v108: container contents lists (Item.firstChild / nextSibling at the end of the Item).
// Older saves: items read as ITEM_V107_SIZE records; the lists are rebuilt after every load.
#define V108_CONTENTS_LISTS 108
#define ITEM_V107_SIZE 60
_Static_assert(offsetof(Item, firstChild) == ITEM_V107_SIZE, "Item v107 layout must be a prefix of Item");
// v109: Mover.setAside[2] at the end of the Mover (things put down for a job, see inventory.c).
// Older saves: none set aside.
#define V109_SET_ASIDE 109
// v110: one-way layer (grid.h oneWayLayer, one byte per cell: one-way ladders) after the
// track connections grid. Older saves: every cell both ways.
#define V110_ONE_WAY 110
// v111: relations (relations.h MoverRelations, one per saved mover) after the conveyor belts.
// Older saves: nobody knows anybody.
#define V111_RELATIONS 111
#define MOVER_V108_SIZE 360   // sizeof(Mover) in v106-v108 (ends with wornBack)
_Static_assert(offsetof(Mover, setAside) == MOVER_V108_SIZE, "Mover v108 layout must be a prefix of Mover");
static inline void FreadLegacyIntsAsU8(uint8_t* dst, size_t n, FILE* f) {
    int32_t buf[256];
    while (n > 0) {
        size_t k = n < 256 ? n : 256;
        size_t got = fread(buf, sizeof(int32_t), k, f);
        for (size_t i = 0; i < got; i++) dst[i] = (uint8_t)buf[i];
        if (got < k) return;
        dst += k; n -= k;
    }
}
static inline void FreadLegacyIntsAsU16(uint16_t* dst, size_t n, FILE* f) {
    int32_t buf[256];
    while (n > 0) {
        size_t k = n < 256 ? n : 256;
        size_t got = fread(buf, sizeof(int32_t), k, f);
        for (size_t i = 0; i < got; i++) {
            int32_t v = buf[i];
            dst[i] = (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
        }
        if (got < k) return;
        dst += k; n -= k;
    }
}

// V82 Mover struct (before name/gender/age/appearanceSeed/isDrafted fields added in v83)
typedef struct {
    float x, y, z;
    Point goal;
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
    float thirst;
    float dehydrationTimer;
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
    int equippedClothing;
    // No name/gender/age/appearanceSeed/isDrafted in V82
} MoverV82;

// V85 Mover struct (before transport fields added in v86)
typedef struct {
    float x, y, z;
    Point goal;
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
    float thirst;
    float dehydrationTimer;
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
    int equippedClothing;
    char name[16];
    uint8_t gender;
    uint8_t age;
    uint32_t appearanceSeed;
    bool isDrafted;
    // No transport fields in V85
} MoverV85;

// V89 Mover struct (before mood fields added in v90)
typedef struct {
    float x, y, z;
    Point goal;
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
    float thirst;
    float dehydrationTimer;
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
    int equippedClothing;
    char name[16];
    uint8_t gender;
    uint8_t age;
    uint32_t appearanceSeed;
    bool isDrafted;
    // No mood fields in V89
    int transportState;
    int transportStation;
    int transportExitStation;
    int transportTrainIdx;
    Point transportFinalGoal;
} MoverV89;

// V85 Train struct (before station/transport fields added in v86)
typedef struct {
    float x, y;
    int z;
    int cellX, cellY;
    int prevCellX, prevCellY;
    float speed;
    float progress;
    int lightCellX, lightCellY;
    bool active;
    // No cartState, stateTimer, atStation, ridingMovers, ridingCount in V85
} TrainV85;

// V86 TrainStation struct (before multi-cell platform fields added in v87)
typedef struct {
    int trackX, trackY, z;
    int platX, platY;
    bool active;
    // No platformCells, platformCellCount, queueDirX/Y in V86
    int waitingMovers[MAX_STATION_WAITING];
    float waitingSince[MAX_STATION_WAITING];
    int waitingCount;
} TrainStationV86;

// V91 Item struct (before temperature field added in v92)
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
    uint8_t condition;
    // No temperature field in V91
} ItemV91;

// V90 item type count (before ITEM_BOILED_WATER, ITEM_SANDWICH added in v91)
#define V90_ITEM_TYPE_COUNT 68

// V90 Stockpile struct (allowedTypes was V90_ITEM_TYPE_COUNT)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V90_ITEM_TYPE_COUNT];
    bool allowedMaterials[V104_MAT_COUNT];
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
    bool rejectsRotten;
} StockpileV90;

// V103 Stockpile struct (v91-v103: allowedTypes was V103_ITEM_TYPE_COUNT). Same fields as
// StockpileV90, only the type count differs: one migration macro serves both.
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V103_ITEM_TYPE_COUNT];
    bool allowedMaterials[V104_MAT_COUNT];
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
    bool rejectsRotten;
} StockpileV103;

// V104 Stockpile struct (v104 only: allowedTypes already V104_ITEM_TYPE_COUNT, allowedMaterials
// still V104_MAT_COUNT, before MAT_CONCRETE)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V104_ITEM_TYPE_COUNT];
    bool allowedMaterials[V104_MAT_COUNT];
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
    bool rejectsRotten;
} StockpileV104;

// V105 Stockpile struct (v105 only: allowedTypes V105_ITEM_TYPE_COUNT, materials with concrete)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V105_ITEM_TYPE_COUNT];
    bool allowedMaterials[V105_MAT_COUNT];
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
    bool rejectsRotten;
} StockpileV105;

// V106 Stockpile struct (v106 only: allowedTypes V106_ITEM_TYPE_COUNT)
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    bool allowedTypes[V106_ITEM_TYPE_COUNT];
    bool allowedMaterials[V105_MAT_COUNT];
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
    bool rejectsRotten;
} StockpileV106;

// Copy an old stockpile (StockpileV90 / V103 / V104) into a Stockpile; item types the old
// save didn't know are not allowed, materials it didn't know are
#define MIGRATE_OLD_STOCKPILE(sp, old, oldTypeCount) MIGRATE_OLD_STOCKPILE_M(sp, old, oldTypeCount, V104_MAT_COUNT)
#define MIGRATE_OLD_STOCKPILE_M(sp, old, oldTypeCount, oldMatCount) do { \
    (sp)->x = (old)->x; (sp)->y = (old)->y; (sp)->z = (old)->z; \
    (sp)->width = (old)->width; (sp)->height = (old)->height; \
    (sp)->active = (old)->active; \
    memset((sp)->allowedTypes, 0, sizeof((sp)->allowedTypes)); \
    memcpy((sp)->allowedTypes, (old)->allowedTypes, (oldTypeCount) * sizeof(bool)); \
    for (int m_ = 0; m_ < MAT_COUNT; m_++) (sp)->allowedMaterials[m_] = true; \
    memcpy((sp)->allowedMaterials, (old)->allowedMaterials, (oldMatCount) * sizeof(bool)); \
    memcpy((sp)->cells, (old)->cells, sizeof((sp)->cells)); \
    memcpy((sp)->slots, (old)->slots, sizeof((sp)->slots)); \
    memcpy((sp)->reservedBy, (old)->reservedBy, sizeof((sp)->reservedBy)); \
    memcpy((sp)->slotCounts, (old)->slotCounts, sizeof((sp)->slotCounts)); \
    memcpy((sp)->slotTypes, (old)->slotTypes, sizeof((sp)->slotTypes)); \
    memcpy((sp)->slotMaterials, (old)->slotMaterials, sizeof((sp)->slotMaterials)); \
    (sp)->maxStackSize = (old)->maxStackSize; \
    (sp)->priority = (old)->priority; \
    (sp)->maxContainers = (old)->maxContainers; \
    memcpy((sp)->slotIsContainer, (old)->slotIsContainer, sizeof((sp)->slotIsContainer)); \
    memcpy((sp)->groundItemIdx, (old)->groundItemIdx, sizeof((sp)->groundItemIdx)); \
    (sp)->freeSlotCount = (old)->freeSlotCount; \
    (sp)->rejectsRotten = (old)->rejectsRotten; \
} while (0)

// V87 Train struct (before multi-car trail fields added in v88)
typedef struct {
    float x, y;
    int z;
    int cellX, cellY;
    int prevCellX, prevCellY;
    float speed;
    float progress;
    int lightCellX, lightCellY;
    bool active;
    int cartState;
    float stateTimer;
    int atStation;
    int ridingMovers[MAX_CART_CAPACITY];
    int ridingCount;
    // No carCount, trail fields in V87
} TrainV87;

// V96 Train struct (v88-v96: multi-car, before freight added in v97). Same layout as the
// start of Train.
typedef struct {
    float x, y;
    int z;
    int cellX, cellY;
    int prevCellX, prevCellY;
    float speed;
    float progress;
    int lightCellX, lightCellY;
    bool active;
    int cartState;
    float stateTimer;
    int atStation;
    int ridingMovers[MAX_CART_CAPACITY];
    int ridingCount;
    int carCount;
    int trailCellX[MAX_TRAIL_LENGTH];
    int trailCellY[MAX_TRAIL_LENGTH];
    int trailCount;
} TrainV96;

_Static_assert(offsetof(Train, cargo) == sizeof(TrainV96), "TrainV96 must be the start of Train");

static inline void MigrateTrainV96(Train* t, const TrainV96* old) {
    memset(t, 0, sizeof(*t));
    memcpy(t, old, sizeof(*old));   // TrainV96 is a prefix of Train
    t->cargoCount = 0;
}

#endif
