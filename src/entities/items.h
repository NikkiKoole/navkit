#ifndef ITEMS_H
#define ITEMS_H

#include <stdbool.h>
#include <stdint.h>
#include "item_defs.h"

// Item types (for Phase 0, just colors)
typedef enum {
    ITEM_NONE = -1,    // No item (sentinel value for optional returns)
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE,
    ITEM_ROCK,         // Raw stone from mining (rock block)
    ITEM_BLOCKS,       // Crafted blocks (material determines wood vs stone)
    ITEM_LOG,          // Raw logs from chopping felled trees
    ITEM_SAPLING,      // Tree saplings (material determines species)
    ITEM_LEAVES,       // Tree leaves (material determines species)
    ITEM_DIRT,         // Dirt blocks from digging dirt
    ITEM_CLAY,         // Clay blocks from clay soil
    ITEM_GRAVEL,       // Gravel blocks from gravel soil
    ITEM_SAND,         // Sand blocks from sand soil
    ITEM_PEAT,         // Peat blocks from peat soil
    ITEM_PLANKS,       // Sawn lumber from sawmill
    ITEM_STICKS,       // Small pieces from sawmill
    ITEM_POLES,        // Thin trunks from tree branches
    ITEM_GRASS,        // Harvested grass (can be dried)
    ITEM_DRIED_GRASS,  // Dried grass for thatch/bedding
    ITEM_BRICKS,       // Fired clay bricks from kiln
    ITEM_CHARCOAL,     // Charcoal from kiln (efficient fuel)
    ITEM_ASH,          // Ash from burning fuel (hearth byproduct)
    ITEM_BARK,         // Bark stripped from logs at sawmill
    ITEM_STRIPPED_LOG,  // Log after bark removal (bonus planks)
    ITEM_SHORT_STRING,  // Twisted plant fiber string
    ITEM_CORDAGE,       // Braided rope from string
    ITEM_BERRIES,       // Fresh berries (edible)
    ITEM_DRIED_BERRIES, // Dried berries (edible, longer lasting)
    ITEM_BASKET,        // Woven basket (cordage container)
    ITEM_CLAY_POT,      // Fired clay pot (kiln container)
    ITEM_CHEST,         // Wooden chest (planks container)
    ITEM_PLANK_BED,     // Crafted bed (furniture, placed via construction)
    ITEM_CHAIR,         // Crafted chair (furniture, placed via construction)
    ITEM_SHARP_STONE,   // Knapped stone tool/sharp edge
    ITEM_DIGGING_STICK, // Crude digging tool (digging:1)
    ITEM_STONE_AXE,     // Stone axe (cutting:2, hammering:1)
    ITEM_STONE_PICK,    // Stone pickaxe (digging:2, hammering:2)
    ITEM_STONE_HAMMER,  // Stone hammer (hammering:2)
    ITEM_CARCASS,       // Animal carcass (must be butchered)
    ITEM_RAW_MEAT,      // Raw meat (edible, poor nutrition)
    ITEM_COOKED_MEAT,   // Cooked meat (edible, good nutrition)
    ITEM_HIDE,          // Animal hide (for clothing later)
    ITEM_ROOT,          // Raw root (barely edible)
    ITEM_ROASTED_ROOT,  // Roasted root (decent food)
    ITEM_DRIED_ROOT,    // Dried root (preserved winter food)
    ITEM_COMPOST,       // Compost (fertilizer from organic waste)
    ITEM_WHEAT_SEEDS,   // Wheat seeds (plantable)
    ITEM_LENTIL_SEEDS,  // Lentil seeds (plantable)
    ITEM_FLAX_SEEDS,    // Flax seeds (plantable)
    ITEM_WHEAT,         // Harvested wheat grain
    ITEM_LENTILS,       // Harvested lentils
    ITEM_FLAX_FIBER,    // Harvested flax fiber
    ITEM_FLOUR,         // Ground wheat flour
    ITEM_BREAD,         // Baked bread (edible)
    ITEM_COOKED_LENTILS, // Cooked lentils (edible)
    ITEM_CLOTH,          // Woven cloth (from dried grass or cordage)
    ITEM_LINEN,          // Linen fabric (from flax fiber)
    ITEM_LEATHER,        // Tanned leather (from hide)
    ITEM_GRASS_TUNIC,    // Grass cloth tunic (basic clothing)
    ITEM_FLAX_TUNIC,     // Linen tunic (medium clothing)
    ITEM_LEATHER_VEST,   // Leather vest (good clothing)
    ITEM_LEATHER_COAT,   // Leather coat (best clothing)
    ITEM_WATER,          // Water (drinkable liquid, stored in containers)
    ITEM_HERBAL_TEA,     // Herbal tea (best hydration, brewed at campfire)
    ITEM_BERRY_JUICE,    // Berry juice (good hydration, spoils)
    ITEM_MUD,            // Mud (dirt + clay, wet building material)
    ITEM_COB,            // Cob (mud + dried grass, strong building material)
    ITEM_REEDS,          // Harvested reeds (waterside plant)
    ITEM_REED_MAT,       // Woven reed mat (building material)
    ITEM_GLASS,          // Glass pane (from sand at kiln)
    ITEM_LYE,            // Lye (from ash at hearth)
    ITEM_MORTAR,         // Mortar (from lye + sand at stonecutter)
    ITEM_TYPE_COUNT    // Must be last - number of item types
} ItemType;

// Item state
typedef enum {
    ITEM_ON_GROUND,
    ITEM_CARRIED,
    ITEM_IN_STOCKPILE,
    ITEM_IN_CONTAINER     // inside another item
} ItemState;

// Item condition (spoilage progression)
typedef enum {
    CONDITION_FRESH = 0,   // Timer < 50% of limit
    CONDITION_STALE,       // Timer 50-80% of limit
    CONDITION_ROTTEN       // Timer >= 100% of limit
} ItemCondition;

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
    int stackCount;           // how many units this item represents (default 1)
    int containedIn;          // item index of container (-1 = not contained)
    int contentCount;         // items directly inside this container (0 if not container)
    uint32_t contentTypeMask; // bitmask of ItemTypes inside (bloom filter, never cleared on remove)
    float spoilageTimer;      // game-seconds elapsed since spawn (0 = fresh, only used if IF_SPOILS)
    uint8_t condition;        // ItemCondition (CONDITION_FRESH/STALE/ROTTEN)
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
void ClearUnreachableCooldownsNearCell(int x, int y, int z, int radius); // Clear cooldowns when terrain changes

// Ground item queries for stockpile blocking
// Returns the index of a ground item at the given tile, or -1 if none
int FindGroundItemAtTile(int tileX, int tileY, int z);

// Drop an item at the given position, searching 8 neighbors for walkable cell if needed
void SafeDropItem(int itemIdx, float x, float y, int z);

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
static inline int GetItemStackCount(int itemIdx) { return items[itemIdx].stackCount; }

static inline bool IsItemRotten(int itemIdx) {
    return items[itemIdx].condition == CONDITION_ROTTEN;
}

static inline bool IsSaplingItem(ItemType type) {
    return type == ITEM_SAPLING;
}

static inline bool IsLeafItem(ItemType type) {
    return type == ITEM_LEAVES;
}

static inline bool ItemTypeUsesMaterialName(ItemType type) {
    return type >= 0 && type < ITEM_TYPE_COUNT && ItemUsesMaterialName(type);
}

#endif
