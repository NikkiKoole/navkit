#ifndef STOCKPILES_H
#define STOCKPILES_H

#include <stdbool.h>
#include "items.h"
#include "../world/material.h"
#include "vendor/raylib.h"

#define MAX_STOCKPILES 64
#define MAX_STOCKPILE_SIZE 32  // max width/height
#define MAX_STACK_SIZE 10      // max items per slot

// Stockpile filter definition (shared between input keybindings and tooltip display)
typedef struct {
    ItemType itemType;
    char key;                // Keyboard key for toggling (e.g., 'r' for Red)
    const char* displayName; // Display name (e.g., "Red")
    const char* shortName;   // Short abbreviation for tooltip (e.g., "R")
    Color color;             // Display color
} StockpileFilterDef;

extern const StockpileFilterDef STOCKPILE_FILTERS[];
extern const int STOCKPILE_FILTER_COUNT;

// Material sub-filter definition (e.g. wood species within ITEM_LOG)
typedef struct {
    MaterialType material;
    ItemType parentItem;     // which item type this is a sub-filter for
    char key;                // Keyboard key ('1', '2', etc.)
    const char* displayName;
    const char* shortName;
    Color color;
} StockpileMaterialFilterDef;

extern const StockpileMaterialFilterDef STOCKPILE_MATERIAL_FILTERS[];
extern const int STOCKPILE_MATERIAL_FILTER_COUNT;

typedef struct {
    int x, y, z;           // top-left corner
    int width, height;
    bool active;
    bool allowedTypes[ITEM_TYPE_COUNT];  // indexed by ItemType
    bool allowedMaterials[MAT_COUNT];    // indexed by MaterialType
    bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];     // which cells are active (for non-rectangular shapes)
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];      // item index per tile, -1 = empty
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // reservation count per tile, 0 = none
    // Stacking support
    int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // number of items in stack
    ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // type of items in slot (-1 = empty)
    uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // material of items in slot (MAT_NONE = empty)
    int maxStackSize;      // per-stockpile stack limit (1-MAX_STACK_SIZE, default MAX_STACK_SIZE)
    // Priority support
    int priority;          // higher = better storage (1-9, default 5)
    // Container support
    int maxContainers;     // max container items allowed as slots (0 = no containers, default 0)
    bool slotIsContainer[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // true if slot is an installed container
    // Ground item cache (avoids expensive FindGroundItemAtTile calls)
    int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // item index on this slot, -1 if none
    // Free slot cache (avoids scanning all tiles when stockpile is full)
    int freeSlotCount;     // number of unreserved slots with room for items (rebuilt each frame)
} Stockpile;

// Gather zones
#define MAX_GATHER_ZONES 32

typedef struct {
    int x, y, z;
    int width, height;
    bool active;
} GatherZone;

extern GatherZone gatherZones[MAX_GATHER_ZONES];
extern int gatherZoneCount;

extern Stockpile stockpiles[MAX_STOCKPILES];
extern int stockpileCount;

// Core functions
void ClearStockpiles(void);
int CreateStockpile(int x, int y, int z, int width, int height);
void DeleteStockpile(int index);
void AddStockpileCells(int stockpileIdx, int x1, int y1, int x2, int y2);
void RemoveStockpileCells(int stockpileIdx, int x1, int y1, int x2, int y2);
bool IsStockpileCellActive(int stockpileIdx, int worldX, int worldY);
int GetStockpileActiveCellCount(int stockpileIdx);

// Filters
void SetStockpileFilter(int stockpileIdx, ItemType type, bool allowed);
void SetStockpileMaterialFilter(int stockpileIdx, MaterialType material, bool allowed);
bool StockpileAcceptsType(int stockpileIdx, ItemType type);
bool StockpileAcceptsItem(int stockpileIdx, ItemType type, uint8_t material);

// Slot management
bool FindFreeStockpileSlot(int stockpileIdx, ItemType type, uint8_t material, int* outX, int* outY);
bool ReserveStockpileSlot(int stockpileIdx, int slotX, int slotY, int moverIdx, ItemType type, uint8_t material);
void ReleaseStockpileSlot(int stockpileIdx, int slotX, int slotY);
void ReleaseAllSlotsForMover(int moverIdx);

// Queries
int FindStockpileForItem(ItemType type, uint8_t material, int* outSlotX, int* outSlotY);  // returns stockpile index or -1
bool IsPositionInStockpile(float x, float y, int z, int* outStockpileIdx);

// Placement
void PlaceItemInStockpile(int stockpileIdx, int slotX, int slotY, int itemIdx);

// Slot state helpers - update slot fields atomically (reduces partial update bugs)
void IncrementStockpileSlot(Stockpile* sp, int slotIdx, int itemIdx, ItemType type, MaterialType mat);
void DecrementStockpileSlot(Stockpile* sp, int slotIdx);
void ClearStockpileSlot(Stockpile* sp, int slotIdx);

// Slot cleanup - clear slot state when item leaves stockpile (delete, push, drop)
void RemoveItemFromStockpileSlot(float x, float y, int z);

// Gather zones
void ClearGatherZones(void);
int CreateGatherZone(int x, int y, int z, int width, int height);
void DeleteGatherZone(int index);
bool IsItemInGatherZone(float x, float y, int z);

// Stacking
void SetStockpileSlotCount(int stockpileIdx, int localX, int localY, ItemType type, int count);
int GetStockpileSlotCount(int stockpileIdx, int slotX, int slotY);
void SetStockpileMaxStackSize(int stockpileIdx, int maxSize);  // allows overfull slots
int GetStockpileMaxStackSize(int stockpileIdx);
bool IsSlotOverfull(int stockpileIdx, int slotX, int slotY);   // slot has more than maxStackSize
int FindStockpileForOverfullItem(int itemIdx, int currentStockpileIdx, int* outSlotX, int* outSlotY);

// Priority
void SetStockpilePriority(int stockpileIdx, int priority);
int GetStockpilePriority(int stockpileIdx);
int FindHigherPriorityStockpile(int itemIdx, int currentStockpileIdx, int* outSlotX, int* outSlotY);

// Stack consolidation
bool FindConsolidationTarget(int stockpileIdx, int srcSlotX, int srcSlotY, int* outDestSlotX, int* outDestSlotY);

// Ground item clearing from stockpile tiles
// Find a ground item on any stockpile tile that needs absorbing (matches filter) or clearing (doesn't match)
// Returns item index, or -1 if none found
// Sets outIsAbsorb to true if item matches stockpile filter (absorb job), false if foreign (clear job)
// Sets outStockpileIdx to the stockpile the item is on
int FindGroundItemOnStockpile(int* outStockpileIdx, bool* outIsAbsorb);

// Ground item cache - call when items land on or leave stockpile tiles
void MarkStockpileGroundItem(float x, float y, int z, int itemIdx);
void RebuildStockpileGroundItemCache(void);  // full rebuild from item positions

// Free slot cache - call once per frame before job assignment
void RebuildStockpileFreeSlotCounts(void);

// =============================================================================
// Stockpile Slot Cache - for O(1) FindStockpileForItem lookups
// =============================================================================
// Call RebuildStockpileSlotCache() once per frame before job assignment.
// Then use FindStockpileForItemCached() instead of FindStockpileForItem().
// The cache stores one available slot per item type + material.

typedef struct {
    int stockpileIdx;   // -1 if no stockpile available for this type
    int slotX, slotY;   // slot coordinates (valid only if stockpileIdx >= 0)
} StockpileSlotCacheEntry;

extern StockpileSlotCacheEntry stockpileSlotCache[ITEM_TYPE_COUNT][MAT_COUNT];

void RebuildStockpileSlotCache(void);  // Build cache - O(types * stockpiles * slots)
int FindStockpileForItemCached(ItemType type, uint8_t material, int* outSlotX, int* outSlotY);  // O(1) lookup
void InvalidateStockpileSlotCache(ItemType type, uint8_t material);  // Call when a slot is reserved
void InvalidateStockpileSlotCacheAll(void);  // Mark entire cache dirty (call when stockpiles added/removed/modified)

// Fill/overfull metrics
float GetStockpileFillRatio(int stockpileIdx);
bool IsStockpileOverfull(int stockpileIdx);

// Container slot support
void SetStockpileMaxContainers(int stockpileIdx, int maxContainers);
int GetStockpileMaxContainers(int stockpileIdx);
int CountInstalledContainers(int stockpileIdx);
bool IsSlotContainer(int stockpileIdx, int slotIdx);

#endif
