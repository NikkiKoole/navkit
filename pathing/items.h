#ifndef ITEMS_H
#define ITEMS_H

#include <stdbool.h>

// Item types (for Phase 0, just colors)
typedef enum {
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE
} ItemType;

// Item state
typedef enum {
    ITEM_ON_GROUND,
    ITEM_CARRIED,
    ITEM_IN_STOCKPILE
} ItemState;

// Item struct
typedef struct {
    float x, y, z;
    ItemType type;
    ItemState state;
    bool active;
    int reservedBy;           // mover index, -1 = none
    float unreachableCooldown; // seconds until retry (0 = can try now)
} Item;

#define MAX_ITEMS 25000

extern Item items[MAX_ITEMS];
extern int itemCount;

// Core functions
void ClearItems(void);
int SpawnItem(float x, float y, float z, ItemType type);
void DeleteItem(int index);

// Reservation
bool ReserveItem(int itemIndex, int moverIndex);
void ReleaseItemReservation(int itemIndex);

// Queries
int FindNearestUnreservedItem(float x, float y, float z);

// Cooldown management
void ItemsTick(float dt);
void SetItemUnreachableCooldown(int itemIndex, float cooldown);

// Ground item queries for stockpile blocking
// Returns the index of a ground item at the given tile, or -1 if none
int FindGroundItemAtTile(int tileX, int tileY, int z);

// Spatial grid for O(1) item lookups (tile-based, includes z-level)
typedef struct {
    int* cellCounts;    // Number of ground items per cell
    int* cellStarts;    // Prefix sum: start index for each cell in itemIndices
    int* itemIndices;   // Item indices sorted by cell (only ON_GROUND items)
    int gridW, gridH;   // Grid dimensions in tiles
    int gridD;          // Grid depth (z-levels)
    int cellCount;      // Total cells (gridW * gridH * gridD)
    int groundItemCount; // Number of ON_GROUND items in the grid
} ItemSpatialGrid;

extern ItemSpatialGrid itemGrid;

// Spatial grid functions
void InitItemSpatialGrid(int tileWidth, int tileHeight, int depth);
void FreeItemSpatialGrid(void);
void BuildItemSpatialGrid(void);

// Query: returns item index at tile, or -1 if none
int QueryItemAtTile(int tileX, int tileY, int z);

// Query: calls callback for each ground item within radius (in tiles) of (tileX, tileY)
// Returns number of items found
typedef void (*ItemNeighborCallback)(int itemIndex, float distSq, void* userData);
int QueryItemsInRadius(int tileX, int tileY, int z, int radiusTiles,
                       ItemNeighborCallback callback, void* userData);

// Query: find first valid item in radius matching filter criteria
// Returns item index or -1 if none found. Much faster than finding "nearest".
// filterFunc returns true if item is valid for this search
typedef bool (*ItemFilterFunc)(int itemIndex, void* userData);
int FindFirstItemInRadius(int tileX, int tileY, int z, int radiusTiles,
                          ItemFilterFunc filter, void* userData);

#endif
