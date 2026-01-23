#ifndef STOCKPILES_H
#define STOCKPILES_H

#include <stdbool.h>
#include "items.h"

#define MAX_STOCKPILES 64
#define MAX_STOCKPILE_SIZE 16  // max width/height

typedef struct {
    int x, y, z;           // top-left corner
    int width, height;
    bool active;
    bool allowedTypes[3];  // indexed by ItemType (RED, GREEN, BLUE)
    int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];      // item index per tile, -1 = empty
    int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE]; // mover index per tile, -1 = none
} Stockpile;

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

#endif
