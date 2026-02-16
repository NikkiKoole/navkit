#include "stockpiles.h"
#include "mover.h"
#include "items.h"
#include "stacking.h"
#include "containers.h"
#include "jobs.h"
#include "../world/cell_defs.h"
#include <string.h>

Stockpile stockpiles[MAX_STOCKPILES];
int stockpileCount = 0;

GatherZone gatherZones[MAX_GATHER_ZONES];
int gatherZoneCount = 0;

// Stockpile filter definitions (shared between input.c keybindings and tooltips.c display)
// This table is the single source of truth for all item type filters.
// To add a new filterable item: just add a row here with a unique key.
const StockpileFilterDef STOCKPILE_FILTERS[] = {
    {ITEM_RED,          'r', "Red",          "R", RED},
    {ITEM_GREEN,        'g', "Green",        "G", GREEN},
    {ITEM_BLUE,         'b', "Blue",         "B", BLUE},
    {ITEM_ROCK,         'o', "Rock",         "O", ORANGE},
    {ITEM_BLOCKS,       's', "Blocks",       "S", GRAY},
    {ITEM_LOG,          'w', "Wood",         "W", BROWN},
    {ITEM_DIRT,         'd', "Dirt",         "D", BROWN},
    {ITEM_PLANKS,       'p', "Planks",       "P", BROWN},
    {ITEM_STICKS,       'k', "Sticks",       "K", BROWN},
    {ITEM_POLES,        'l', "Poles",         "L", BROWN},
    {ITEM_GRASS,        'm', "Grass",        "M", GREEN},
    {ITEM_DRIED_GRASS,  'h', "Dried Grass",  "H", YELLOW},
    {ITEM_BRICKS,       'i', "Bricks",       "I", ORANGE},
    {ITEM_CHARCOAL,     'c', "Charcoal",     "C", GRAY},
    {ITEM_BARK,         'a', "Bark",         "A", BROWN},
    {ITEM_STRIPPED_LOG,  'e', "Stripped Log",  "E", BROWN},
    {ITEM_SHORT_STRING, 'n', "String",        "N", BEIGE},
    {ITEM_CORDAGE,      'j', "Cordage",       "J", BEIGE},
    {ITEM_SAPLING,      't', "Saplings",      "T", GREEN},
    {ITEM_LEAVES,       'v', "Leaves",        "V", GREEN},
    {ITEM_CLAY,         'y', "Clay",          "Y", BROWN},
    {ITEM_GRAVEL,       'q', "Gravel",        "Q", GRAY},
    {ITEM_SAND,         'z', "Sand",          "Z", YELLOW},
    {ITEM_PEAT,         'u', "Peat",          "U", BROWN},
    {ITEM_ASH,          'f', "Ash",           "F", GRAY},
    {ITEM_BERRIES,      'x', "Berries",       "X", PURPLE},
    {ITEM_DRIED_BERRIES,'1', "Dried Berries", "1", PURPLE},
    {ITEM_BASKET,       '2', "Baskets",       "2", BEIGE},
    {ITEM_CLAY_POT,     '3', "Clay Pots",     "3", ORANGE},
    {ITEM_CHEST,        '4', "Chests",        "4", BROWN},
};
const int STOCKPILE_FILTER_COUNT = sizeof(STOCKPILE_FILTERS) / sizeof(STOCKPILE_FILTERS[0]);

// Material sub-filter definitions (e.g. wood species within ITEM_LOG)
// To add a new material sub-filter: just add a row here.
const StockpileMaterialFilterDef STOCKPILE_MATERIAL_FILTERS[] = {
    {MAT_OAK,    ITEM_LOG, '1', "Oak",    "1", BROWN},
    {MAT_PINE,   ITEM_LOG, '2', "Pine",   "2", BROWN},
    {MAT_BIRCH,  ITEM_LOG, '3', "Birch",  "3", BROWN},
    {MAT_WILLOW, ITEM_LOG, '4', "Willow", "4", BROWN},
};
const int STOCKPILE_MATERIAL_FILTER_COUNT = sizeof(STOCKPILE_MATERIAL_FILTERS) / sizeof(STOCKPILE_MATERIAL_FILTERS[0]);

static inline uint8_t ResolveItemMaterial(ItemType type, uint8_t material) {
    uint8_t mat = (material == MAT_NONE) ? DefaultMaterialForItemType(type) : material;
    return (mat < MAT_COUNT) ? mat : MAT_NONE;
}

void ClearStockpiles(void) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        stockpiles[i].active = false;
    }
    stockpileCount = 0;
}

// =============================================================================
// Ground Item Cache
// =============================================================================
// 
// Problem: FindFreeStockpileSlot() was calling FindGroundItemAtTile() for every
// slot it checked. With many stockpile tiles and items, this became O(tiles × items)
// per assignment attempt - causing 79% of frame time in pathological cases.
//
// Solution: Cache which stockpile slots have ground items blocking them.
// - groundItemIdx[slot] stores the item index (-1 if none) per stockpile
// - RebuildStockpileGroundItemCache() does a full rebuild, called once per frame
//   at the start of AssignJobs()
// - SpawnItem() also marks the cache incrementally for immediate correctness
// - FindFreeStockpileSlot() now does O(1) bool checks instead of O(items) lookups
//
// The cache may be briefly stale between item state changes and the next
// AssignJobs() call, but this is harmless - the full rebuild ensures correctness
// before any job assignment decisions are made.
// =============================================================================

// Mark/unmark a tile as having a ground item (for the groundItemIdx cache)
void MarkStockpileGroundItem(float x, float y, int z, int itemIdx) {
    int tileX = (int)(x / CELL_SIZE);
    int tileY = (int)(y / CELL_SIZE);
    
    // Find which stockpile this tile belongs to
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        if (sp->z != z) continue;
        
        // Check if tile is within stockpile bounds
        int lx = tileX - sp->x;
        int ly = tileY - sp->y;
        if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) continue;
        
        int idx = ly * sp->width + lx;
        if (!sp->cells[idx]) continue;  // not an active cell
        
        sp->groundItemIdx[idx] = itemIdx;
        return;  // tile can only belong to one stockpile
    }
}

// Rebuild the entire groundItemIdx cache from current item positions
void RebuildStockpileGroundItemCache(void) {
    // Clear all to -1 (no item)
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        int totalSlots = stockpiles[i].width * stockpiles[i].height;
        memset(stockpiles[i].groundItemIdx, -1, totalSlots * sizeof(int));
    }
    
    // Mark slots with their ground item indices
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        MarkStockpileGroundItem(items[i].x, items[i].y, (int)items[i].z, i);
    }
}

// Rebuild free slot counts for all stockpiles
// A slot is "free" if: active cell, not reserved, not full, no ground item blocking, walkable
void RebuildStockpileFreeSlotCounts(void) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        
        int freeCount = 0;
        int totalSlots = sp->width * sp->height;
        for (int s = 0; s < totalSlots; s++) {
            if (!sp->cells[s]) continue;            // inactive cell
            if (sp->groundItemIdx[s] >= 0) continue; // ground item blocking
            
            // Check walkability
            int lx = s % sp->width;
            int ly = s / sp->width;
            int worldX = sp->x + lx;
            int worldY = sp->y + ly;
            if (!IsCellWalkableAt(sp->z, worldY, worldX)) continue;
            
            // Container slots use container capacity, bare slots use maxStackSize
            if (IsSlotContainer(i, s)) {
                const ContainerDef* def = GetContainerDef(items[sp->slots[s]].type);
                if (def && items[sp->slots[s]].contentCount + sp->reservedBy[s] < def->maxContents) {
                    freeCount++;
                }
            } else if (sp->slotCounts[s] + sp->reservedBy[s] < sp->maxStackSize) {
                freeCount++;
            }
        }
        sp->freeSlotCount = freeCount;
    }
}

// =============================================================================
// Stockpile Slot Cache Implementation
// =============================================================================

StockpileSlotCacheEntry stockpileSlotCache[ITEM_TYPE_COUNT][MAT_COUNT];
static bool stockpileSlotCacheDirty = true;  // Rebuild on first use and when stockpiles change

void RebuildStockpileSlotCache(void) {
    if (!stockpileSlotCacheDirty) return;  // Already up-to-date
    
    // Initialize all entries to "no stockpile available"
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        for (int m = 0; m < MAT_COUNT; m++) {
            stockpileSlotCache[t][m].stockpileIdx = -1;
            stockpileSlotCache[t][m].slotX = -1;
            stockpileSlotCache[t][m].slotY = -1;
        }
    }
    
    // For each item type + material, find the first available stockpile slot
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        for (int m = 0; m < MAT_COUNT; m++) {
            int slotX, slotY;
            int spIdx = FindStockpileForItem((ItemType)t, (uint8_t)m, &slotX, &slotY);
            if (spIdx >= 0) {
                stockpileSlotCache[t][m].stockpileIdx = spIdx;
                stockpileSlotCache[t][m].slotX = slotX;
                stockpileSlotCache[t][m].slotY = slotY;
            }
        }
    }
    stockpileSlotCacheDirty = false;  // Cache is now up-to-date
}

int FindStockpileForItemCached(ItemType type, uint8_t material, int* outSlotX, int* outSlotY) {
    if (type < 0 || type >= ITEM_TYPE_COUNT) return -1;

    uint8_t mat = ResolveItemMaterial(type, material);
    if (mat >= MAT_COUNT) mat = MAT_NONE;

    StockpileSlotCacheEntry* entry = &stockpileSlotCache[type][mat];
    if (entry->stockpileIdx < 0) return -1;
    
    *outSlotX = entry->slotX;
    *outSlotY = entry->slotY;
    return entry->stockpileIdx;
}

void InvalidateStockpileSlotCache(ItemType type, uint8_t material) {
    if (type < 0 || type >= ITEM_TYPE_COUNT) return;

    uint8_t mat = ResolveItemMaterial(type, material);
    if (mat >= MAT_COUNT) mat = MAT_NONE;

    // Re-find a slot for this type + material
    int slotX, slotY;
    int spIdx = FindStockpileForItem(type, mat, &slotX, &slotY);
    if (spIdx >= 0) {
        stockpileSlotCache[type][mat].stockpileIdx = spIdx;
        stockpileSlotCache[type][mat].slotX = slotX;
        stockpileSlotCache[type][mat].slotY = slotY;
    } else {
        stockpileSlotCache[type][mat].stockpileIdx = -1;
        stockpileSlotCache[type][mat].slotX = -1;
        stockpileSlotCache[type][mat].slotY = -1;
    }
}

void InvalidateStockpileSlotCacheAll(void) {
    stockpileSlotCacheDirty = true;
}

int CreateStockpile(int x, int y, int z, int width, int height) {
    // Find first inactive slot
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) {
            Stockpile* sp = &stockpiles[i];
            sp->x = x;
            sp->y = y;
            sp->z = z;
            sp->width = (width > MAX_STOCKPILE_SIZE) ? MAX_STOCKPILE_SIZE : width;
            sp->height = (height > MAX_STOCKPILE_SIZE) ? MAX_STOCKPILE_SIZE : height;
            sp->active = true;
            
            // Default: allow all types
            for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
                sp->allowedTypes[t] = true;
            }
            // Default: allow all materials
            for (int m = 0; m < MAT_COUNT; m++) {
                sp->allowedMaterials[m] = true;
            }
            
            // Initialize slots as empty
            int totalSlots = sp->width * sp->height;
            for (int s = 0; s < totalSlots; s++) {
                sp->cells[s] = true;    // all cells active initially
                sp->slots[s] = -1;      // no item
                sp->reservedBy[s] = 0; // not reserved
                sp->slotCounts[s] = 0;  // no items stacked
                sp->slotTypes[s] = -1;  // no type
                sp->slotMaterials[s] = MAT_NONE; // no material
                sp->groundItemIdx[s] = -1; // no ground item blocking
                sp->slotIsContainer[s] = false;
            }
            
            // Initialize free slot count (so WorkGiver_Haul works before a rebuild)
            int freeCount = 0;
            for (int s = 0; s < totalSlots; s++) {
                int lx = s % sp->width;
                int ly = s / sp->width;
                int worldX = sp->x + lx;
                int worldY = sp->y + ly;
                if (IsCellWalkableAt(sp->z, worldY, worldX)) {
                    freeCount++;
                }
            }
            sp->freeSlotCount = freeCount;
            
            // Default priority, stack size, and container limit
            sp->priority = 5;
            sp->maxStackSize = MAX_STACK_SIZE;
            sp->maxContainers = 0;
            
            stockpileCount++;
            InvalidateStockpileSlotCacheAll();  // New stockpile added
            return i;
        }
    }
    return -1;  // no space
}

void DeleteStockpile(int index) {
    if (index >= 0 && index < MAX_STOCKPILES && stockpiles[index].active) {
        Stockpile* sp = &stockpiles[index];
        
        // Spill container contents and drop all ITEM_IN_STOCKPILE items to ground
        int totalSlots = sp->width * sp->height;
        for (int s = 0; s < totalSlots; s++) {
            if (!sp->cells[s]) continue;
            if (sp->slots[s] >= 0 && sp->slots[s] < MAX_ITEMS
                && items[sp->slots[s]].active && items[sp->slots[s]].contentCount > 0) {
                SpillContainerContents(sp->slots[s]);
            }
        }
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if (items[i].state != ITEM_IN_STOCKPILE) continue;
            
            // Check if item is within this stockpile's bounds
            int itemTileX = (int)(items[i].x / CELL_SIZE);
            int itemTileY = (int)(items[i].y / CELL_SIZE);
            int itemZ = (int)items[i].z;
            
            if (itemZ == sp->z &&
                itemTileX >= sp->x && itemTileX < sp->x + sp->width &&
                itemTileY >= sp->y && itemTileY < sp->y + sp->height) {
                items[i].state = ITEM_ON_GROUND;
            }
        }
        
        sp->active = false;
        stockpileCount--;
        InvalidateStockpileSlotCacheAll();  // Stockpile deleted
    }
}

void AddStockpileCells(int stockpileIdx, int x1, int y1, int x2, int y2) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    for (int wy = y1; wy <= y2; wy++) {
        for (int wx = x1; wx <= x2; wx++) {
            int lx = wx - sp->x;
            int ly = wy - sp->y;
            if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) continue;
            int idx = ly * sp->width + lx;
            sp->cells[idx] = true;
        }
    }
    InvalidateStockpileSlotCacheAll();  // Stockpile geometry changed
}

void RemoveStockpileCells(int stockpileIdx, int x1, int y1, int x2, int y2) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    for (int wy = y1; wy <= y2; wy++) {
        for (int wx = x1; wx <= x2; wx++) {
            int lx = wx - sp->x;
            int ly = wy - sp->y;
            if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) continue;
            int idx = ly * sp->width + lx;
            if (!sp->cells[idx]) continue;  // already inactive
            
            // Drop any items in this slot to the ground and clear reservations
            if (sp->slotCounts[idx] > 0) {
                // Find items at this tile and set them to ground state
                for (int i = 0; i < itemHighWaterMark; i++) {
                    if (!items[i].active) continue;
                    if (items[i].state != ITEM_IN_STOCKPILE) continue;
                    int itemTileX = (int)(items[i].x / CELL_SIZE);
                    int itemTileY = (int)(items[i].y / CELL_SIZE);
                    int itemZ = (int)items[i].z;
                    if (itemTileX == wx && itemTileY == wy && itemZ == sp->z) {
                        items[i].state = ITEM_ON_GROUND;
                        // Clear item reservations when dropping to ground
                        items[i].reservedBy = -1;
                    }
                }
            }
            
            sp->cells[idx] = false;
            // Clear slot data including reservations
            sp->slots[idx] = -1;
            sp->reservedBy[idx] = 0;
            sp->slotCounts[idx] = 0;
            sp->slotTypes[idx] = -1;
            sp->slotMaterials[idx] = MAT_NONE;
        }
    }
    
    // Cancel any haul/clear jobs targeting the removed cells
    for (int i = 0; i < activeJobCount; i++) {
        int jobId = activeJobList[i];
        Job* job = &jobs[jobId];
        if (!job->active) continue;
        if (job->type != JOBTYPE_HAUL && job->type != JOBTYPE_CLEAR) continue;
        if (job->targetStockpile != stockpileIdx) continue;
        
        // Check if job targets a removed cell
        if (job->targetSlotX >= x1 && job->targetSlotX <= x2 &&
            job->targetSlotY >= y1 && job->targetSlotY <= y2) {
            // Release item reservation
            if (job->targetItem >= 0 && items[job->targetItem].active) {
                items[job->targetItem].reservedBy = -1;
            }
            // Release job
            ReleaseJob(jobId);
            // Reset mover
            int moverIdx = job->assignedMover;
            if (moverIdx >= 0 && moverIdx < moverCount && movers[moverIdx].active) {
                Mover* m = &movers[moverIdx];
                m->currentJobId = -1;
                ClearMoverPath(moverIdx);
                m->needsRepath = false;
                m->timeWithoutProgress = 0.0f;
                AddMoverToIdleList(moverIdx);
            }
        }
    }
    
    InvalidateStockpileSlotCacheAll();  // Stockpile geometry changed
    
    // Check if stockpile is now empty - if so, delete it
    if (GetStockpileActiveCellCount(stockpileIdx) == 0) {
        DeleteStockpile(stockpileIdx);
    }
}

bool IsStockpileCellActive(int stockpileIdx, int worldX, int worldY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    
    int lx = worldX - sp->x;
    int ly = worldY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return false;
    
    int idx = ly * sp->width + lx;
    return sp->cells[idx];
}

int GetStockpileActiveCellCount(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return 0;
    
    int count = 0;
    int totalSlots = sp->width * sp->height;
    for (int s = 0; s < totalSlots; s++) {
        if (sp->cells[s]) count++;
    }
    return count;
}

void SetStockpileFilter(int stockpileIdx, ItemType type, bool allowed) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    if (!stockpiles[stockpileIdx].active) return;
    if (type < 0 || type >= ITEM_TYPE_COUNT) return;
    
    stockpiles[stockpileIdx].allowedTypes[type] = allowed;
}

void SetStockpileMaterialFilter(int stockpileIdx, MaterialType material, bool allowed) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    if (!stockpiles[stockpileIdx].active) return;
    if (material < 0 || material >= MAT_COUNT) return;

    stockpiles[stockpileIdx].allowedMaterials[material] = allowed;
}

bool StockpileAcceptsType(int stockpileIdx, ItemType type) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    if (!stockpiles[stockpileIdx].active) return false;
    if (type < 0 || type >= ITEM_TYPE_COUNT) return false;
    return StockpileAcceptsItem(stockpileIdx, type, DefaultMaterialForItemType(type));
}

bool StockpileAcceptsItem(int stockpileIdx, ItemType type, uint8_t material) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    if (!stockpiles[stockpileIdx].active) return false;
    if (type < 0 || type >= ITEM_TYPE_COUNT) return false;

    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->allowedTypes[type]) return false;

    MaterialType mat = (MaterialType)ResolveItemMaterial(type, material);
    if (type == ITEM_LOG && IsWoodMaterial(mat) && !sp->allowedTypes[ITEM_LOG]) return false;
    if (mat != MAT_NONE && !sp->allowedMaterials[mat]) return false;

    return true;
}

// Helper: get slot index from local coordinates
static int SlotIndex(Stockpile* sp, int localX, int localY) {
    return localY * sp->width + localX;
}

bool FindFreeStockpileSlot(int stockpileIdx, ItemType type, uint8_t material, int* outX, int* outY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    if (!StockpileAcceptsItem(stockpileIdx, type, material)) return false;

    MaterialType mat = (MaterialType)ResolveItemMaterial(type, material);
    
    // Pass 0: find a container slot with room (containers are preferred storage)
    // Don't put containers inside containers via this path — container installation
    // is handled separately in PlaceItemInStockpile
    if (!ItemIsContainer(type)) {
        for (int ly = 0; ly < sp->height; ly++) {
            for (int lx = 0; lx < sp->width; lx++) {
                int idx = SlotIndex(sp, lx, ly);
                if (!sp->cells[idx]) continue;
                if (sp->groundItemIdx[idx] >= 0) continue;
                if (!IsSlotContainer(stockpileIdx, idx)) continue;

                int worldX = sp->x + lx;
                int worldY = sp->y + ly;
                if (!IsCellWalkableAt(sp->z, worldY, worldX)) continue;

                int containerIdx = sp->slots[idx];
                const ContainerDef* def = GetContainerDef(items[containerIdx].type);
                if (!def || items[containerIdx].contentCount + sp->reservedBy[idx] >= def->maxContents) continue;
                *outX = worldX;
                *outY = worldY;
                return true;
            }
        }
    }
    
    // First pass: find a partial stack of same type that still has room
    // (accounting for both existing items and pending reservations)
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            int idx = SlotIndex(sp, lx, ly);
            if (!sp->cells[idx]) continue;            // skip inactive cells
            if (sp->groundItemIdx[idx] >= 0) continue; // skip if ground item blocking (O(1) check)
            
            // Skip cells that aren't walkable (e.g., above ramps)
            int worldX = sp->x + lx;
            int worldY = sp->y + ly;
            if (!IsCellWalkableAt(sp->z, worldY, worldX)) continue;
            
            if (sp->slotTypes[idx] == type &&
                sp->slotMaterials[idx] == mat &&
                sp->slotCounts[idx] > 0 &&
                sp->slotCounts[idx] + sp->reservedBy[idx] < sp->maxStackSize) {
                // Found partial stack of same type with room
                *outX = worldX;
                *outY = worldY;
                return true;
            }
        }
    }
    
    // Second pass: find a reserved-but-empty slot of matching type with room
    // (ReserveStockpileSlot stamps type on first reservation, so we can safely stack)
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            int idx = SlotIndex(sp, lx, ly);
            if (!sp->cells[idx]) continue;
            if (sp->groundItemIdx[idx] >= 0) continue;
            if (sp->reservedBy[idx] <= 0) continue;   // only reserved slots
            
            int worldX = sp->x + lx;
            int worldY = sp->y + ly;
            if (!IsCellWalkableAt(sp->z, worldY, worldX)) continue;
            
            if (sp->slotTypes[idx] == type &&
                sp->slotMaterials[idx] == mat &&
                sp->slotCounts[idx] + sp->reservedBy[idx] < sp->maxStackSize) {
                *outX = worldX;
                *outY = worldY;
                return true;
            }
        }
    }
    
    // Third pass: find an empty unreserved slot
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            int idx = SlotIndex(sp, lx, ly);
            if (!sp->cells[idx]) continue;
            if (sp->reservedBy[idx] > 0) continue;
            if (sp->groundItemIdx[idx] >= 0) continue;
            
            int worldX = sp->x + lx;
            int worldY = sp->y + ly;
            if (!IsCellWalkableAt(sp->z, worldY, worldX)) continue;
            
            if (sp->slotCounts[idx] == 0 && sp->slots[idx] == -1) {
                *outX = worldX;
                *outY = worldY;
                return true;
            }
        }
    }
    
    return false;  // stockpile full or all reserved
}

bool ReserveStockpileSlot(int stockpileIdx, int slotX, int slotY, int moverIdx, ItemType type, uint8_t material) {
    (void)moverIdx;  // Reservations are tracked by count, not mover ID
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return false;
    
    int idx = SlotIndex(sp, lx, ly);
    
    // Can reserve if slot has room for another item (counting existing + pending)
    if (sp->slotCounts[idx] + sp->reservedBy[idx] >= sp->maxStackSize) return false;
    
    MaterialType mat = (MaterialType)ResolveItemMaterial(type, material);
    
    // If slot already has a type (from items or prior reservations), reject mismatches
    if (sp->slotTypes[idx] >= 0 && (sp->slotTypes[idx] != type || sp->slotMaterials[idx] != mat)) {
        return false;
    }
    
    // Stamp type on first reservation of an empty slot
    if (sp->slotCounts[idx] == 0 && sp->reservedBy[idx] == 0) {
        sp->slotTypes[idx] = type;
        sp->slotMaterials[idx] = mat;
    }
    
    sp->reservedBy[idx]++;
    return true;
}

void ReleaseStockpileSlot(int stockpileIdx, int slotX, int slotY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return;
    
    int idx = SlotIndex(sp, lx, ly);
    if (sp->reservedBy[idx] > 0) sp->reservedBy[idx]--;
    
    // Clear type stamp if slot is now empty and unreserved
    if (sp->reservedBy[idx] == 0 && sp->slotCounts[idx] == 0) {
        sp->slotTypes[idx] = -1;
        sp->slotMaterials[idx] = MAT_NONE;
    }
}

void ReleaseAllSlotsForMover(int moverIdx) {
    (void)moverIdx;  // No longer tracking individual movers
    // This is only called from ClearMovers() which clears all movers,
    // so we just zero all reservation counts
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        int totalSlots = sp->width * sp->height;
        for (int s = 0; s < totalSlots; s++) {
            sp->reservedBy[s] = 0;
        }
    }
}

// TODO: Optimization opportunities for FindStockpileForItem (14.7% of frame time)
// This function is called many times per frame for the same item types.
// 
// Option 1: Cache results per item type per frame
//   - Build lookup table at frame start: bestStockpileForType[ITEM_TYPE_COUNT]
//   - Pre-compute slot coordinates for each type
//   - Call FindStockpileForItem once per type instead of once per item
//   - Biggest impact, requires cache invalidation when slots fill up
//
// Option 2: Track first free slot index per stockpile  
//   - Avoid scanning from index 0 each time
//   - Maintain a "firstFreeSlot" hint that gets updated on reserve/release
//
// Option 3: Sort stockpiles by priority once per frame
//   - Currently iterating in index order, not priority order
//   - Pre-sort active stockpiles by priority for better slot selection

int FindStockpileForItem(ItemType type, uint8_t material, int* outSlotX, int* outSlotY) {
    // Find stockpile that accepts this type and has a free slot, respecting priority
    // Iterate from highest priority (9) to lowest (1)
    int bestIdx = -1;
    int bestSlotX = 0, bestSlotY = 0;
    int bestPriority = 0;
    
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (!StockpileAcceptsItem(i, type, material)) continue;
        if (stockpiles[i].freeSlotCount <= 0) continue;  // O(1) early exit if full
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, material, &slotX, &slotY)) {
            // Take this stockpile if it's higher priority than what we've found so far
            if (stockpiles[i].priority > bestPriority) {
                bestPriority = stockpiles[i].priority;
                bestIdx = i;
                bestSlotX = slotX;
                bestSlotY = slotY;
            }
        }
    }
    
    if (bestIdx >= 0) {
        *outSlotX = bestSlotX;
        *outSlotY = bestSlotY;
        return bestIdx;
    }
    return -1;  // no stockpile available
}

bool IsPositionInStockpile(float x, float y, int z, int* outStockpileIdx) {
    int gx = (int)(x / CELL_SIZE);
    int gy = (int)(y / CELL_SIZE);
    
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        if (sp->z != z) continue;
        
        if (gx >= sp->x && gx < sp->x + sp->width &&
            gy >= sp->y && gy < sp->y + sp->height) {
            // Check if this specific cell is active
            int lx = gx - sp->x;
            int ly = gy - sp->y;
            int idx = ly * sp->width + lx;
            if (!sp->cells[idx]) continue;
            
            if (outStockpileIdx) *outStockpileIdx = i;
            return true;
        }
    }
    return false;
}

// Place an item in a stockpile slot (called when mover drops item)
void PlaceItemInStockpile(int stockpileIdx, int slotX, int slotY, int itemIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return;
    
    int idx = SlotIndex(sp, lx, ly);
    
    // Validate cell is active before placing item
    if (!sp->cells[idx]) return;
    
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS || !items[itemIdx].active) return;
    
    if (sp->reservedBy[idx] > 0) sp->reservedBy[idx]--;  // one reservation fulfilled
    sp->groundItemIdx[idx] = -1;  // item is being placed in slot, not on ground
    
    // If slot has a container, route item into it
    if (IsSlotContainer(stockpileIdx, idx)) {
        int containerIdx = sp->slots[idx];
        PutItemInContainer(itemIdx, containerIdx);
        sp->slotCounts[idx] = items[containerIdx].contentCount;
        return;
    }
    
    // Check if this is a container being installed as a slot
    if (ItemIsContainer(items[itemIdx].type) && sp->maxContainers > 0
        && sp->slotCounts[idx] == 0 && sp->slots[idx] == -1) {
        int installed = CountInstalledContainers(stockpileIdx);
        if (installed < sp->maxContainers) {
            // Install container as the slot's storage unit
            items[itemIdx].state = ITEM_IN_STOCKPILE;
            sp->slots[idx] = itemIdx;
            sp->slotTypes[idx] = items[itemIdx].type;
            sp->slotMaterials[idx] = ResolveItemMaterial(items[itemIdx].type, items[itemIdx].material);
            sp->slotCounts[idx] = 0;  // container is empty
            sp->slotIsContainer[idx] = true;
            return;
        }
    }
    
    // Validate slot types and materials match if slot already has items
    if (sp->slotTypes[idx] >= 0 && sp->slotTypes[idx] != items[itemIdx].type) {
        return;
    }
    MaterialType incomingMat = ResolveItemMaterial(items[itemIdx].type, items[itemIdx].material);
    if (sp->slotCounts[idx] > 0 && sp->slotMaterials[idx] != incomingMat) {
        return;
    }
    
    // If slot already has an item, merge stacks (incoming item may be deleted)
    if (sp->slotCounts[idx] > 0 && sp->slots[idx] >= 0 && sp->slots[idx] < MAX_ITEMS
        && items[sp->slots[idx]].active) {
        MergeItemIntoStack(sp->slots[idx], itemIdx);
        sp->slotCounts[idx] = items[sp->slots[idx]].stackCount;
    } else {
        // First item in slot
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        sp->slots[idx] = itemIdx;
        sp->slotTypes[idx] = items[itemIdx].type;
        sp->slotMaterials[idx] = incomingMat;
        sp->slotCounts[idx] = items[itemIdx].stackCount;
    }
}

// Slot state helpers - update slot fields atomically
void IncrementStockpileSlot(Stockpile* sp, int slotIdx, int itemIdx, ItemType type, MaterialType mat) {
    if (sp->slotCounts[slotIdx] == 0) {
        // First item in slot - initialize type and material
        sp->slots[slotIdx] = itemIdx;
        sp->slotTypes[slotIdx] = type;
        sp->slotMaterials[slotIdx] = mat;
    }
    sp->slotCounts[slotIdx]++;
}

void DecrementStockpileSlot(Stockpile* sp, int slotIdx) {
    if (sp->slotCounts[slotIdx] > 0) {
        sp->slotCounts[slotIdx]--;
        if (sp->slotCounts[slotIdx] == 0) {
            ClearStockpileSlot(sp, slotIdx);
        }
    }
}

__attribute__((noinline))
void ClearStockpileSlot(Stockpile* sp, int slotIdx) {
    sp->slots[slotIdx] = -1;
    sp->slotTypes[slotIdx] = -1;
    sp->slotMaterials[slotIdx] = MAT_NONE;
    sp->slotCounts[slotIdx] = 0;
    sp->slotIsContainer[slotIdx] = false;
}

// Remove an item from a stockpile slot at world position (x, y, z)
// Called when an item leaves a stockpile (DeleteItem, PushItemsOutOfCell, DropItemsInCell)
// With stackCount model: one Item per slot, so removing it clears the slot.
void RemoveItemFromStockpileSlot(float x, float y, int z) {
    int sourceSp = -1;
    if (!IsPositionInStockpile(x, y, z, &sourceSp) || sourceSp < 0) return;

    int lx = (int)(x / CELL_SIZE) - stockpiles[sourceSp].x;
    int ly = (int)(y / CELL_SIZE) - stockpiles[sourceSp].y;
    if (lx < 0 || lx >= stockpiles[sourceSp].width || ly < 0 || ly >= stockpiles[sourceSp].height) return;

    int idx = ly * stockpiles[sourceSp].width + lx;
    ClearStockpileSlot(&stockpiles[sourceSp], idx);
}

void SyncStockpileSlotCount(float x, float y, int z) {
    int sourceSp = -1;
    if (!IsPositionInStockpile(x, y, z, &sourceSp) || sourceSp < 0) return;

    int lx = (int)(x / CELL_SIZE) - stockpiles[sourceSp].x;
    int ly = (int)(y / CELL_SIZE) - stockpiles[sourceSp].y;
    if (lx < 0 || lx >= stockpiles[sourceSp].width || ly < 0 || ly >= stockpiles[sourceSp].height) return;

    int idx = ly * stockpiles[sourceSp].width + lx;
    int slotItem = stockpiles[sourceSp].slots[idx];
    if (slotItem >= 0 && items[slotItem].active) {
        stockpiles[sourceSp].slotCounts[idx] = items[slotItem].stackCount;
    }
}

// =============================================================================
// Gather Zones (stub implementations)
// =============================================================================

void ClearGatherZones(void) {
    for (int i = 0; i < MAX_GATHER_ZONES; i++) {
        gatherZones[i].active = false;
    }
    gatherZoneCount = 0;
}

int CreateGatherZone(int x, int y, int z, int width, int height) {
    for (int i = 0; i < MAX_GATHER_ZONES; i++) {
        if (!gatherZones[i].active) {
            GatherZone* gz = &gatherZones[i];
            gz->x = x;
            gz->y = y;
            gz->z = z;
            gz->width = width;
            gz->height = height;
            gz->active = true;
            gatherZoneCount++;
            return i;
        }
    }
    return -1;
}

void DeleteGatherZone(int index) {
    if (index >= 0 && index < MAX_GATHER_ZONES && gatherZones[index].active) {
        gatherZones[index].active = false;
        gatherZoneCount--;
    }
}

bool IsItemInGatherZone(float x, float y, int z) {
    // If no gather zones exist, all items are eligible
    if (gatherZoneCount == 0) return true;
    
    int gx = (int)(x / CELL_SIZE);
    int gy = (int)(y / CELL_SIZE);
    
    for (int i = 0; i < MAX_GATHER_ZONES; i++) {
        if (!gatherZones[i].active) continue;
        GatherZone* gz = &gatherZones[i];
        if (gz->z != z) continue;
        
        if (gx >= gz->x && gx < gz->x + gz->width &&
            gy >= gz->y && gy < gz->y + gz->height) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Stacking (stub implementations)
// =============================================================================

void SetStockpileSlotCount(int stockpileIdx, int localX, int localY, ItemType type, int count) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    if (localX < 0 || localX >= sp->width || localY < 0 || localY >= sp->height) return;
    
    int idx = SlotIndex(sp, localX, localY);
    sp->slotTypes[idx] = type;
    sp->slotCounts[idx] = count;
    sp->slotMaterials[idx] = (count > 0) ? DefaultMaterialForItemType(type) : MAT_NONE;

    // Create/update representative item to keep stackCount in sync
    if (count > 0) {
        int worldX = sp->x + localX;
        int worldY = sp->y + localY;
        float cx = worldX * CELL_SIZE + CELL_SIZE * 0.5f;
        float cy = worldY * CELL_SIZE + CELL_SIZE * 0.5f;
        if (sp->slots[idx] >= 0 && sp->slots[idx] < MAX_ITEMS && items[sp->slots[idx]].active) {
            items[sp->slots[idx]].stackCount = count;
        } else {
            int itemIdx = SpawnItemWithMaterial(cx, cy, (float)sp->z, type,
                                               DefaultMaterialForItemType(type));
            if (itemIdx >= 0) {
                items[itemIdx].state = ITEM_IN_STOCKPILE;
                items[itemIdx].stackCount = count;
                sp->slots[idx] = itemIdx;
            }
        }
    } else {
        ClearStockpileSlot(sp, idx);
    }
}

int GetStockpileSlotCount(int stockpileIdx, int slotX, int slotY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return 0;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return 0;
    
    int idx = SlotIndex(sp, lx, ly);
    return sp->slotCounts[idx];
}

void SetStockpileMaxStackSize(int stockpileIdx, int maxSize) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    // Clamp to valid range
    if (maxSize < 1) maxSize = 1;
    if (maxSize > MAX_STACK_SIZE) maxSize = MAX_STACK_SIZE;
    
    sp->maxStackSize = maxSize;
    
    // Note: We don't eject items when reducing max stack size.
    // Overfull slots are allowed to exist (shown visually as over-capacity).
    // New items won't be added to overfull slots, so they'll naturally drain
    // as items are used or re-hauled to other stockpiles.
}

int GetStockpileMaxStackSize(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    if (!stockpiles[stockpileIdx].active) return 0;
    return stockpiles[stockpileIdx].maxStackSize;
}

bool IsSlotOverfull(int stockpileIdx, int slotX, int slotY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return false;
    
    int idx = SlotIndex(sp, lx, ly);
    return sp->slotCounts[idx] > sp->maxStackSize;
}

int FindStockpileForOverfullItem(int itemIdx, int currentStockpileIdx, int* outSlotX, int* outSlotY) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS || !items[itemIdx].active) return -1;
    if (currentStockpileIdx < 0 || currentStockpileIdx >= MAX_STOCKPILES) return -1;
    
    ItemType type = items[itemIdx].type;
    uint8_t material = items[itemIdx].material;
    
    // First: try to spread within the same stockpile (overfull slot → empty/partial slot)
    {
        int slotX, slotY;
        if (FindFreeStockpileSlot(currentStockpileIdx, type, material, &slotX, &slotY)) {
            // Make sure we're not targeting the same slot the item is already on
            int itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
            int itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
            if (slotX != itemTileX || slotY != itemTileY) {
                *outSlotX = slotX;
                *outSlotY = slotY;
                return currentStockpileIdx;
            }
        }
    }
    
    // Then: find any other stockpile that accepts this type and has room
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (i == currentStockpileIdx) continue;
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, material, &slotX, &slotY)) {
            *outSlotX = slotX;
            *outSlotY = slotY;
            return i;
        }
    }
    return -1;
}

// =============================================================================
// Priority (stub implementations)
// =============================================================================

void SetStockpilePriority(int stockpileIdx, int priority) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    if (!stockpiles[stockpileIdx].active) return;
    stockpiles[stockpileIdx].priority = priority;
    InvalidateStockpileSlotCacheAll();  // Priority change affects which stockpile is chosen
}

int GetStockpilePriority(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    if (!stockpiles[stockpileIdx].active) return 0;
    return stockpiles[stockpileIdx].priority;
}

float GetStockpileFillRatio(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0.0f;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return 0.0f;

    int totalItems = 0;
    int maxCapacity = 0;
    int totalSlots = sp->width * sp->height;

    for (int i = 0; i < totalSlots; i++) {
        if (!sp->cells[i]) continue;
        totalItems += sp->slotCounts[i];
        if (IsSlotContainer(stockpileIdx, i)) {
            const ContainerDef* def = GetContainerDef(items[sp->slots[i]].type);
            maxCapacity += def ? def->maxContents : sp->maxStackSize;
        } else {
            maxCapacity += sp->maxStackSize;
        }
    }

    if (maxCapacity <= 0) return 0.0f;
    return (float)totalItems / (float)maxCapacity;
}

bool IsStockpileOverfull(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;

    int totalItems = 0;
    int activeCells = 0;
    int totalSlots = sp->width * sp->height;

    for (int i = 0; i < totalSlots; i++) {
        if (!sp->cells[i]) continue;
        activeCells++;
        totalItems += sp->slotCounts[i];
        if (sp->slotCounts[i] > sp->maxStackSize) return true;
    }

    int maxCapacity = activeCells * sp->maxStackSize;
    return totalItems > maxCapacity;
}

int FindHigherPriorityStockpile(int itemIdx, int currentStockpileIdx, int* outSlotX, int* outSlotY) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS || !items[itemIdx].active) return -1;
    if (currentStockpileIdx < 0 || currentStockpileIdx >= MAX_STOCKPILES) return -1;
    
    ItemType type = items[itemIdx].type;
    uint8_t material = items[itemIdx].material;
    int currentPriority = stockpiles[currentStockpileIdx].priority;
    
    // Find stockpile with higher priority that accepts this type and has room
    int bestIdx = -1;
    int bestPriority = currentPriority;
    int bestSlotX = 0, bestSlotY = 0;
    
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (i == currentStockpileIdx) continue;  // skip current stockpile
        if (stockpiles[i].priority <= currentPriority) continue;  // must be higher priority
        if (stockpiles[i].freeSlotCount <= 0) continue;  // O(1) early exit if full
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, material, &slotX, &slotY)) {
            if (stockpiles[i].priority > bestPriority) {
                bestPriority = stockpiles[i].priority;
                bestIdx = i;
                bestSlotX = slotX;
                bestSlotY = slotY;
            }
        }
    }
    
    if (bestIdx >= 0) {
        *outSlotX = bestSlotX;
        *outSlotY = bestSlotY;
    }
    return bestIdx;
}

bool FindConsolidationTarget(int stockpileIdx, int srcSlotX, int srcSlotY, int* outDestSlotX, int* outDestSlotY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    
    int srcLx = srcSlotX - sp->x;
    int srcLy = srcSlotY - sp->y;
    if (srcLx < 0 || srcLx >= sp->width || srcLy < 0 || srcLy >= sp->height) return false;
    
    int srcIdx = SlotIndex(sp, srcLx, srcLy);
    if (sp->slotCounts[srcIdx] <= 0) return false;
    if (sp->slotCounts[srcIdx] >= sp->maxStackSize) return false;  // source is full, nothing to consolidate
    
    ItemType type = sp->slotTypes[srcIdx];
    uint8_t mat = sp->slotMaterials[srcIdx];
    
    // Find another partial stack of same type/material with room
    // ONLY consolidate from smaller stacks onto LARGER ones to avoid ping-pong
    int srcCount = sp->slotCounts[srcIdx];
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            if (lx == srcLx && ly == srcLy) continue;  // skip source slot
            int idx = SlotIndex(sp, lx, ly);
            if (!sp->cells[idx]) continue;
            
            if (sp->slotTypes[idx] == type &&
                sp->slotMaterials[idx] == mat &&
                sp->slotCounts[idx] > srcCount &&  // dest must be LARGER than source
                sp->slotCounts[idx] + sp->reservedBy[idx] < sp->maxStackSize) {
                *outDestSlotX = sp->x + lx;
                *outDestSlotY = sp->y + ly;
                return true;
            }
        }
    }
    return false;
}

// =============================================================================
// Ground item clearing from stockpile tiles
// =============================================================================

int FindGroundItemOnStockpile(int* outStockpileIdx, bool* outIsAbsorb) {
    // Scan all stockpiles for ground items on their tiles
    for (int spIdx = 0; spIdx < MAX_STOCKPILES; spIdx++) {
        if (!stockpiles[spIdx].active) continue;
        Stockpile* sp = &stockpiles[spIdx];
        
        // Check each tile in this stockpile
        for (int ly = 0; ly < sp->height; ly++) {
            for (int lx = 0; lx < sp->width; lx++) {
                int idx = ly * sp->width + lx;
                if (!sp->cells[idx]) continue;  // skip inactive cells
                
                int itemIdx = sp->groundItemIdx[idx];  // O(1) cache lookup
                if (itemIdx < 0) continue;  // no ground item on this slot
                
                // Validate item is still active and on ground (cache may be slightly stale)
                if (!items[itemIdx].active || items[itemIdx].state != ITEM_ON_GROUND) continue;
                
                // Check if it's already reserved
                if (items[itemIdx].reservedBy != -1) continue;
                
                // Check if item matches stockpile filter
                bool matches = StockpileAcceptsItem(spIdx, items[itemIdx].type, items[itemIdx].material);
                
                *outStockpileIdx = spIdx;
                *outIsAbsorb = matches;
                return itemIdx;
            }
        }
    }
    
    return -1;  // no ground items on stockpile tiles
}

// =============================================================================
// Container Slot Support
// =============================================================================

void SetStockpileMaxContainers(int stockpileIdx, int maxContainers) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    if (maxContainers < 0) maxContainers = 0;
    sp->maxContainers = maxContainers;
    InvalidateStockpileSlotCacheAll();
}

int GetStockpileMaxContainers(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    if (!stockpiles[stockpileIdx].active) return 0;
    return stockpiles[stockpileIdx].maxContainers;
}

int CountInstalledContainers(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return 0;

    int count = 0;
    int totalSlots = sp->width * sp->height;
    for (int s = 0; s < totalSlots; s++) {
        if (sp->slotIsContainer[s]) count++;
    }
    return count;
}

bool IsSlotContainer(int stockpileIdx, int slotIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    if (slotIdx < 0 || slotIdx >= sp->width * sp->height) return false;
    return sp->slotIsContainer[slotIdx];
}

void SyncStockpileContainerSlotCount(int containerIdx) {
    if (containerIdx < 0 || containerIdx >= MAX_ITEMS) return;
    if (!items[containerIdx].active) return;
    if (items[containerIdx].state != ITEM_IN_STOCKPILE) return;

    // Find which stockpile slot holds this container
    for (int s = 0; s < stockpileCount; s++) {
        Stockpile* sp = &stockpiles[s];
        if (!sp->active) continue;
        for (int idx = 0; idx < sp->width * sp->height; idx++) {
            if (!sp->slotIsContainer[idx]) continue;
            if (sp->slots[idx] != containerIdx) continue;
            sp->slotCounts[idx] = items[containerIdx].contentCount;
            return;
        }
    }
}
