#include "stockpiles.h"
#include "mover.h"
#include "items.h"

Stockpile stockpiles[MAX_STOCKPILES];
int stockpileCount = 0;

GatherZone gatherZones[MAX_GATHER_ZONES];
int gatherZoneCount = 0;

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
// - hasGroundItem[slot] is a simple bool array per stockpile
// - RebuildStockpileGroundItemCache() does a full rebuild, called once per frame
//   at the start of AssignJobs()
// - SpawnItem() also marks the cache incrementally for immediate correctness
// - FindFreeStockpileSlot() now does O(1) bool checks instead of O(items) lookups
//
// The cache may be briefly stale between item state changes and the next
// AssignJobs() call, but this is harmless - the full rebuild ensures correctness
// before any job assignment decisions are made.
// =============================================================================

// Mark/unmark a tile as having a ground item (for the hasGroundItem cache)
void MarkStockpileGroundItem(float x, float y, int z, bool hasItem) {
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
        
        sp->hasGroundItem[idx] = hasItem;
        return;  // tile can only belong to one stockpile
    }
}

// Rebuild the entire hasGroundItem cache from current item positions
void RebuildStockpileGroundItemCache(void) {
    // Clear all flags
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        int totalSlots = sp->width * sp->height;
        for (int s = 0; s < totalSlots; s++) {
            sp->hasGroundItem[s] = false;
        }
    }
    
    // Mark slots that have ground items
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        
        MarkStockpileGroundItem(items[i].x, items[i].y, (int)items[i].z, true);
    }
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
            for (int t = 0; t < 3; t++) {
                sp->allowedTypes[t] = true;
            }
            
            // Initialize slots as empty
            int totalSlots = sp->width * sp->height;
            for (int s = 0; s < totalSlots; s++) {
                sp->cells[s] = true;    // all cells active initially
                sp->slots[s] = -1;      // no item
                sp->reservedBy[s] = -1; // not reserved
                sp->slotCounts[s] = 0;  // no items stacked
                sp->slotTypes[s] = -1;  // no type
                sp->hasGroundItem[s] = false; // no ground item blocking
            }
            
            // Default priority and stack size
            sp->priority = 5;
            sp->maxStackSize = MAX_STACK_SIZE;
            
            stockpileCount++;
            return i;
        }
    }
    return -1;  // no space
}

void DeleteStockpile(int index) {
    if (index >= 0 && index < MAX_STOCKPILES && stockpiles[index].active) {
        stockpiles[index].active = false;
        stockpileCount--;
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
            
            // Drop any items in this slot to the ground
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
                    }
                }
            }
            
            sp->cells[idx] = false;
            // Clear slot data
            sp->slots[idx] = -1;
            sp->reservedBy[idx] = -1;
            sp->slotCounts[idx] = 0;
            sp->slotTypes[idx] = -1;
        }
    }
    
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
    if (type < 0 || type > 2) return;
    
    stockpiles[stockpileIdx].allowedTypes[type] = allowed;
}

bool StockpileAcceptsType(int stockpileIdx, ItemType type) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    if (!stockpiles[stockpileIdx].active) return false;
    if (type < 0 || type > 2) return false;
    
    return stockpiles[stockpileIdx].allowedTypes[type];
}

// Helper: get slot index from local coordinates
static int SlotIndex(Stockpile* sp, int localX, int localY) {
    return localY * sp->width + localX;
}

bool FindFreeStockpileSlot(int stockpileIdx, ItemType type, int* outX, int* outY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    if (!sp->allowedTypes[type]) return false;
    
    // First pass: find a partial stack of same type that isn't full or reserved
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            int idx = SlotIndex(sp, lx, ly);
            if (!sp->cells[idx]) continue;            // skip inactive cells
            if (sp->reservedBy[idx] != -1) continue;  // skip reserved
            if (sp->hasGroundItem[idx]) continue;     // skip if ground item blocking (O(1) check)
            
            if (sp->slotTypes[idx] == type && sp->slotCounts[idx] > 0 && sp->slotCounts[idx] < sp->maxStackSize) {
                // Found partial stack of same type
                *outX = sp->x + lx;
                *outY = sp->y + ly;
                return true;
            }
        }
    }
    
    // Second pass: find an empty slot
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            int idx = SlotIndex(sp, lx, ly);
            if (!sp->cells[idx]) continue;            // skip inactive cells
            if (sp->reservedBy[idx] != -1) continue;  // skip reserved
            if (sp->hasGroundItem[idx]) continue;     // skip if ground item blocking (O(1) check)
            
            if (sp->slotCounts[idx] == 0 && sp->slots[idx] == -1) {
                // Found empty slot
                *outX = sp->x + lx;
                *outY = sp->y + ly;
                return true;
            }
        }
    }
    
    return false;  // stockpile full or all reserved
}

bool ReserveStockpileSlot(int stockpileIdx, int slotX, int slotY, int moverIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return false;
    
    int idx = SlotIndex(sp, lx, ly);
    
    // Can reserve if: (empty OR partial stack not full) AND not already reserved
    bool hasRoom = (sp->slotCounts[idx] == 0) || (sp->slotCounts[idx] < sp->maxStackSize);
    if (!hasRoom) return false;                   // slot is full
    if (sp->reservedBy[idx] != -1) return false;  // already reserved
    
    sp->reservedBy[idx] = moverIdx;
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
    sp->reservedBy[idx] = -1;
}

void ReleaseAllSlotsForMover(int moverIdx) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        int totalSlots = sp->width * sp->height;
        for (int s = 0; s < totalSlots; s++) {
            if (sp->reservedBy[s] == moverIdx) {
                sp->reservedBy[s] = -1;
            }
        }
    }
}

int FindStockpileForItem(ItemType type, int* outSlotX, int* outSlotY) {
    // Find any stockpile that accepts this type and has a free slot
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (!stockpiles[i].allowedTypes[type]) continue;
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, &slotX, &slotY)) {
            *outSlotX = slotX;
            *outSlotY = slotY;
            return i;
        }
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
    sp->slots[idx] = itemIdx;
    sp->reservedBy[idx] = -1;  // no longer reserved, now occupied
    
    // Update stacking info
    if (itemIdx >= 0 && itemIdx < MAX_ITEMS && items[itemIdx].active) {
        sp->slotTypes[idx] = items[itemIdx].type;
        sp->slotCounts[idx]++;
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
    
    // Find any stockpile (other than current) that accepts this type and has room
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (i == currentStockpileIdx) continue;
        if (!stockpiles[i].allowedTypes[type]) continue;
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, &slotX, &slotY)) {
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
}

int GetStockpilePriority(int stockpileIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return 0;
    if (!stockpiles[stockpileIdx].active) return 0;
    return stockpiles[stockpileIdx].priority;
}

int FindHigherPriorityStockpile(int itemIdx, int currentStockpileIdx, int* outSlotX, int* outSlotY) {
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS || !items[itemIdx].active) return -1;
    if (currentStockpileIdx < 0 || currentStockpileIdx >= MAX_STOCKPILES) return -1;
    
    ItemType type = items[itemIdx].type;
    int currentPriority = stockpiles[currentStockpileIdx].priority;
    
    // Find stockpile with higher priority that accepts this type and has room
    int bestIdx = -1;
    int bestPriority = currentPriority;
    int bestSlotX = 0, bestSlotY = 0;
    
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (i == currentStockpileIdx) continue;  // skip current stockpile
        if (stockpiles[i].priority <= currentPriority) continue;  // must be higher priority
        if (!stockpiles[i].allowedTypes[type]) continue;  // must accept this type
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, &slotX, &slotY)) {
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
                if (!sp->hasGroundItem[idx]) continue;  // O(1) cache check - skip if no ground item
                
                int tileX = sp->x + lx;
                int tileY = sp->y + ly;
                
                int itemIdx = FindGroundItemAtTile(tileX, tileY, sp->z);
                if (itemIdx < 0) continue;
                
                // Found a ground item on this stockpile tile
                // Check if it's already reserved
                if (items[itemIdx].reservedBy != -1) continue;
                
                // Check if item matches stockpile filter
                bool matches = sp->allowedTypes[items[itemIdx].type];
                
                *outStockpileIdx = spIdx;
                *outIsAbsorb = matches;
                return itemIdx;
            }
        }
    }
    
    return -1;  // no ground items on stockpile tiles
}
