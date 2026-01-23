#include "items.h"
#include "mover.h"  // for CELL_SIZE
#include "grid.h"   // for gridWidth, gridHeight, gridDepth
#include <math.h>
#include <stdlib.h>
#include <string.h>

Item items[MAX_ITEMS];
int itemCount = 0;

void ClearItems(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].active = false;
        items[i].reservedBy = -1;
        items[i].unreachableCooldown = 0.0f;
    }
    itemCount = 0;
    
    // Initialize spatial grid if grid dimensions are set
    if (gridWidth > 0 && gridHeight > 0 && gridDepth > 0) {
        InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
    }
}

int SpawnItem(float x, float y, float z, ItemType type) {
    // Find first inactive slot
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) {
            items[i].x = x;
            items[i].y = y;
            items[i].z = z;
            items[i].type = type;
            items[i].state = ITEM_ON_GROUND;
            items[i].active = true;
            items[i].reservedBy = -1;
            items[i].unreachableCooldown = 0.0f;
            itemCount++;
            return i;
        }
    }
    return -1;  // no space
}

void DeleteItem(int index) {
    if (index >= 0 && index < MAX_ITEMS && items[index].active) {
        items[index].active = false;
        items[index].reservedBy = -1;
        itemCount--;
    }
}

bool ReserveItem(int itemIndex, int moverIndex) {
    if (itemIndex < 0 || itemIndex >= MAX_ITEMS) return false;
    if (!items[itemIndex].active) return false;
    if (items[itemIndex].reservedBy != -1) return false;  // already reserved
    
    items[itemIndex].reservedBy = moverIndex;
    return true;
}

void ReleaseItemReservation(int itemIndex) {
    if (itemIndex >= 0 && itemIndex < MAX_ITEMS) {
        items[itemIndex].reservedBy = -1;
    }
}

int FindNearestUnreservedItem(float x, float y, float z) {
    int nearest = -1;
    float nearestDistSq = 1e30f;
    
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].reservedBy != -1) continue;  // skip reserved
        
        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float dz = items[i].z - z;
        float distSq = dx*dx + dy*dy + dz*dz;
        
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = i;
        }
    }
    
    return nearest;
}

void ItemsTick(float dt) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].unreachableCooldown > 0.0f) {
            items[i].unreachableCooldown -= dt;
            if (items[i].unreachableCooldown < 0.0f) {
                items[i].unreachableCooldown = 0.0f;
            }
        }
    }
}

void SetItemUnreachableCooldown(int itemIndex, float cooldown) {
    if (itemIndex >= 0 && itemIndex < MAX_ITEMS && items[itemIndex].active) {
        items[itemIndex].unreachableCooldown = cooldown;
    }
}

int FindGroundItemAtTile(int tileX, int tileY, int z) {
    // Use spatial grid for O(1) lookup if grid has been built with items
    // (groundItemCount > 0 means BuildItemSpatialGrid was called after items existed)
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        return QueryItemAtTile(tileX, tileY, z);
    }
    
    // Fallback to O(n) scan if grid not built yet or was built when empty
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        if ((int)items[i].z != z) continue;
        
        int itemTileX = (int)(items[i].x / CELL_SIZE);
        int itemTileY = (int)(items[i].y / CELL_SIZE);
        
        if (itemTileX == tileX && itemTileY == tileY) {
            return i;
        }
    }
    return -1;
}

// =============================================================================
// ItemSpatialGrid Implementation
// =============================================================================

ItemSpatialGrid itemGrid = {0};

static inline int clampi_item(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

void InitItemSpatialGrid(int tileWidth, int tileHeight, int depth) {
    FreeItemSpatialGrid();
    
    itemGrid.gridW = tileWidth;
    itemGrid.gridH = tileHeight;
    itemGrid.gridD = depth;
    itemGrid.cellCount = tileWidth * tileHeight * depth;
    itemGrid.groundItemCount = 0;
    
    if (itemGrid.cellCount <= 0) return;
    
    itemGrid.cellCounts = (int*)calloc(itemGrid.cellCount, sizeof(int));
    itemGrid.cellStarts = (int*)calloc(itemGrid.cellCount + 1, sizeof(int));
    itemGrid.itemIndices = (int*)malloc(MAX_ITEMS * sizeof(int));
}

void FreeItemSpatialGrid(void) {
    free(itemGrid.cellCounts);
    free(itemGrid.cellStarts);
    free(itemGrid.itemIndices);
    itemGrid.cellCounts = NULL;
    itemGrid.cellStarts = NULL;
    itemGrid.itemIndices = NULL;
    itemGrid.cellCount = 0;
    itemGrid.groundItemCount = 0;
}

void BuildItemSpatialGrid(void) {
    if (!itemGrid.cellCounts) return;
    
    // Phase 1: Clear counts
    memset(itemGrid.cellCounts, 0, itemGrid.cellCount * sizeof(int));
    itemGrid.groundItemCount = 0;
    
    // Phase 2: Count ground items per cell
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        
        int tx = (int)(items[i].x / CELL_SIZE);
        int ty = (int)(items[i].y / CELL_SIZE);
        int tz = (int)items[i].z;
        
        tx = clampi_item(tx, 0, itemGrid.gridW - 1);
        ty = clampi_item(ty, 0, itemGrid.gridH - 1);
        tz = clampi_item(tz, 0, itemGrid.gridD - 1);
        
        int cellIdx = tz * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;
        itemGrid.cellCounts[cellIdx]++;
        itemGrid.groundItemCount++;
    }
    
    // Phase 3: Build prefix sum
    itemGrid.cellStarts[0] = 0;
    for (int c = 0; c < itemGrid.cellCount; c++) {
        itemGrid.cellStarts[c + 1] = itemGrid.cellStarts[c] + itemGrid.cellCounts[c];
    }
    
    // Phase 4: Reset counts to use as write cursors
    for (int c = 0; c < itemGrid.cellCount; c++) {
        itemGrid.cellCounts[c] = itemGrid.cellStarts[c];
    }
    
    // Phase 5: Scatter item indices into cells
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        
        int tx = (int)(items[i].x / CELL_SIZE);
        int ty = (int)(items[i].y / CELL_SIZE);
        int tz = (int)items[i].z;
        
        tx = clampi_item(tx, 0, itemGrid.gridW - 1);
        ty = clampi_item(ty, 0, itemGrid.gridH - 1);
        tz = clampi_item(tz, 0, itemGrid.gridD - 1);
        
        int cellIdx = tz * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;
        itemGrid.itemIndices[itemGrid.cellCounts[cellIdx]++] = i;
    }
}

int QueryItemAtTile(int tileX, int tileY, int z) {
    if (!itemGrid.cellCounts) return -1;
    if (tileX < 0 || tileX >= itemGrid.gridW) return -1;
    if (tileY < 0 || tileY >= itemGrid.gridH) return -1;
    if (z < 0 || z >= itemGrid.gridD) return -1;
    
    int cellIdx = z * (itemGrid.gridW * itemGrid.gridH) + tileY * itemGrid.gridW + tileX;
    int start = itemGrid.cellStarts[cellIdx];
    int end = itemGrid.cellStarts[cellIdx + 1];
    
    // Return first valid item found at this tile
    for (int t = start; t < end; t++) {
        int itemIdx = itemGrid.itemIndices[t];
        // Double-check the item is still valid (handles edge cases during same frame)
        if (items[itemIdx].active && items[itemIdx].state == ITEM_ON_GROUND) {
            return itemIdx;
        }
    }
    return -1;
}
