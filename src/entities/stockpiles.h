#ifndef STOCKPILES_H
#define STOCKPILES_H

#include <stdbool.h>
#include "items.h"
#include "../world/material.h"

#define MAX_STOCKPILES 64
#define MAX_STOCKPILE_SIZE 32  // max width/height
#define MAX_STACK_SIZE 10      // max items per slot

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
bool ReserveStockpileSlot(int stockpileIdx, int slotX, int slotY, int moverIdx);
void ReleaseStockpileSlot(int stockpileIdx, int slotX, int slotY);
void ReleaseAllSlotsForMover(int moverIdx);

// Queries
int FindStockpileForItem(ItemType type, uint8_t material, int* outSlotX, int* outSlotY);  // returns stockpile index or -1
bool IsPositionInStockpile(float x, float y, int z, int* outStockpileIdx);

// Placement
void PlaceItemInStockpile(int stockpileIdx, int slotX, int slotY, int itemIdx);

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

#endif
