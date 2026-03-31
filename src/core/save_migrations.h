#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

#include <stdbool.h>
#include "../world/material.h"
#include "../entities/mover.h"

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 92

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
    bool rejectsRotten;
} StockpileV90;

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

#endif
