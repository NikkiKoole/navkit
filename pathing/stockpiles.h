#ifndef STOCKPILES_H
#define STOCKPILES_H

#include <stdbool.h>
#include "items.h"

#define MAX_STOCKPILES 64
#define MAX_STOCKPILE_SIZE 16  // max width/height
#define MAX_STACK_SIZE 10      // max items per slot

typedef struct {
    int x, y, z;           // top-left corner
    int width, height;
    bool active;
    bool allowedTypes[3];  // indexed by ItemType (RED, GREEN, BLUE)
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];      // item index per tile, -1 = empty
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // mover index per tile, -1 = none
    // Stacking support
    int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // number of items in stack
    ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // type of items in slot (-1 = empty)
    int maxStackSize;      // per-stockpile stack limit (1-MAX_STACK_SIZE, default MAX_STACK_SIZE)
    // Priority support
    int priority;          // higher = better storage (1-9, default 5)
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

// Filters
void SetStockpileFilter(int stockpileIdx, ItemType type, bool allowed);
bool StockpileAcceptsType(int stockpileIdx, ItemType type);

// Slot management
bool FindFreeStockpileSlot(int stockpileIdx, ItemType type, int* outX, int* outY);
bool ReserveStockpileSlot(int stockpileIdx, int slotX, int slotY, int moverIdx);
void ReleaseStockpileSlot(int stockpileIdx, int slotX, int slotY);
void ReleaseAllSlotsForMover(int moverIdx);

// Queries
int FindStockpileForItem(ItemType type, int* outSlotX, int* outSlotY);  // returns stockpile index or -1
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

#endif
