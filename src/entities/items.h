#ifndef ITEMS_H
#define ITEMS_H

#include <stdbool.h>
#include <stdint.h>

// Item types (for Phase 0, just colors)
typedef enum {
    ITEM_NONE = -1,    // No item (sentinel value for optional returns)
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE,
    ITEM_ROCK,         // Raw stone from mining (rock block)
    ITEM_BLOCKS,       // Crafted blocks (material determines wood vs stone)
    ITEM_WOOD,         // Wood logs from chopping trees
    ITEM_SAPLING_OAK,  // Oak sapling
    ITEM_SAPLING_PINE, // Pine sapling
    ITEM_SAPLING_BIRCH,// Birch sapling
    ITEM_SAPLING_WILLOW,// Willow sapling
    ITEM_LEAVES_OAK,   // Oak leaves
    ITEM_LEAVES_PINE,  // Pine leaves
    ITEM_LEAVES_BIRCH, // Birch leaves
    ITEM_LEAVES_WILLOW,// Willow leaves
    ITEM_DIRT,         // Dirt blocks from digging dirt
    ITEM_CLAY,         // Clay blocks from clay soil
    ITEM_GRAVEL,       // Gravel blocks from gravel soil
    ITEM_SAND,         // Sand blocks from sand soil
    ITEM_PEAT,         // Peat blocks from peat soil
    ITEM_TYPE_COUNT    // Must be last - number of item types
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
    uint8_t material;        // MaterialType (stored as u8 to avoid header cycle)
    bool natural;            // True if item is unprocessed/natural
    bool active;
    int reservedBy;           // mover index, -1 = none
    float unreachableCooldown; // seconds until retry (0 = can try now)
} Item;

#define MAX_ITEMS 25000

extern Item items[MAX_ITEMS];
extern int itemCount;
extern int itemHighWaterMark;  // Highest index + 1 that has ever been active (for iteration optimization)

// Core functions
void ClearItems(void);
int SpawnItem(float x, float y, float z, ItemType type);
int SpawnItemWithMaterial(float x, float y, float z, ItemType type, uint8_t material);
uint8_t DefaultMaterialForItemType(ItemType type);
void DeleteItem(int index);

// Reservation
bool ReserveItem(int itemIndex, int moverIndex);
void ReleaseItemReservation(int itemIndex);

// Queries
int FindNearestUnreservedItem(float x, float y, float z);  // Uses spatial grid
int FindNearestUnreservedItemNaive(float x, float y, float z);  // O(MAX_ITEMS) brute force

// Cooldown management
void ItemsTick(float dt);              // Optimized - iterates to itemHighWaterMark
void ItemsTickNaive(float dt);         // O(MAX_ITEMS) brute force
void SetItemUnreachableCooldown(int itemIndex, float cooldown);

// Ground item queries for stockpile blocking
// Returns the index of a ground item at the given tile, or -1 if none
int FindGroundItemAtTile(int tileX, int tileY, int z);

// Push all items out of a cell to nearest walkable neighbor
// Used when building walls on cells that contain items
void PushItemsOutOfCell(int x, int y, int z);

// Drop all items in a cell down one z-level
// Used when floor is removed (channeling)
void DropItemsInCell(int x, int y, int z);

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
void BuildItemSpatialGrid(void);           // Optimized - iterates to itemHighWaterMark
void BuildItemSpatialGridNaive(void);      // O(MAX_ITEMS) brute force

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

// Item getters (inline for zero overhead)
static inline bool IsItemActive(int itemIdx) { return items[itemIdx].active; }
static inline float GetItemX(int itemIdx) { return items[itemIdx].x; }
static inline float GetItemY(int itemIdx) { return items[itemIdx].y; }
static inline int GetItemZ(int itemIdx) { return (int)items[itemIdx].z; }
static inline ItemType GetItemType(int itemIdx) { return items[itemIdx].type; }
static inline int GetItemReservedBy(int itemIdx) { return items[itemIdx].reservedBy; }

static inline bool IsSaplingItem(ItemType type) {
    return type == ITEM_SAPLING_OAK || type == ITEM_SAPLING_PINE ||
           type == ITEM_SAPLING_BIRCH || type == ITEM_SAPLING_WILLOW;
}

static inline bool IsLeafItem(ItemType type) {
    return type == ITEM_LEAVES_OAK || type == ITEM_LEAVES_PINE ||
           type == ITEM_LEAVES_BIRCH || type == ITEM_LEAVES_WILLOW;
}

static inline bool ItemTypeUsesMaterialName(ItemType type) {
    return type == ITEM_WOOD || type == ITEM_BLOCKS || type == ITEM_ROCK ||
           IsSaplingItem(type) || IsLeafItem(type);
}

#endif
