#include "items.h"
#include "item_defs.h"  // for ItemName
#include "mover.h"  // for CELL_SIZE
#include "../world/grid.h"   // for gridWidth, gridHeight, gridDepth
#include "../world/material.h"  // for MaterialType
#include "../core/event_log.h"
#include "../world/cell_defs.h"  // for IsCellWalkableAt
#include "stockpiles.h"  // for MarkStockpileGroundItem
#include <math.h>
#include <stdlib.h>
#include <string.h>

Item items[MAX_ITEMS];
int itemCount = 0;
int itemHighWaterMark = 0;  // Highest index + 1 ever active

void ClearItems(void) {
    for (int i = 0; i < itemHighWaterMark; i++) {  // Only clear up to high water mark
        items[i].active = false;
        items[i].reservedBy = -1;
        items[i].unreachableCooldown = 0.0f;
        items[i].material = MAT_NONE;
        items[i].natural = false;
        items[i].stackCount = 1;
        items[i].containedIn = -1;
        items[i].contentCount = 0;
        items[i].contentTypeMask = 0;
    }
    itemCount = 0;
    itemHighWaterMark = 0;
    
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
            items[i].material = DefaultMaterialForItemType(type);
            items[i].natural = false;
            items[i].active = true;
            items[i].reservedBy = -1;
            items[i].unreachableCooldown = 0.0f;
            items[i].stackCount = 1;
            items[i].containedIn = -1;
            items[i].contentCount = 0;
            items[i].contentTypeMask = 0;
            itemCount++;
            if (i + 1 > itemHighWaterMark) {
                itemHighWaterMark = i + 1;
            }
            // Update stockpile ground item cache
            MarkStockpileGroundItem(x, y, (int)z, i);
            return i;
        }
    }
    return -1;  // no space
}

int SpawnItemWithMaterial(float x, float y, float z, ItemType type, uint8_t material) {
    int idx = SpawnItem(x, y, z, type);
    if (idx >= 0) {
        items[idx].material = material;
    }
    return idx;
}

uint8_t DefaultMaterialForItemType(ItemType type) {
    if (type >= 0 && type < ITEM_TYPE_COUNT) {
        return ItemDefaultMaterial(type);
    }
    return MAT_NONE;
}

// Forward declaration — implemented in containers.c (included after items.c in unity build)
void SpillContainerContents(int containerIdx);

void DeleteItem(int index) {
    if (index >= 0 && index < MAX_ITEMS && items[index].active) {
        EventLog("DeleteItem %d (%s) at (%.0f,%.0f,z%.0f) state=%d",
                 index, ItemName(items[index].type), items[index].x, items[index].y, items[index].z, items[index].state);
        // If this is a container with contents, spill them first
        if (items[index].contentCount > 0) {
            SpillContainerContents(index);
        }
        // If this item is inside a container, update parent's contentCount
        if (items[index].containedIn != -1) {
            int parentIdx = items[index].containedIn;
            if (parentIdx >= 0 && parentIdx < MAX_ITEMS && items[parentIdx].active) {
                items[parentIdx].contentCount--;
            }
            items[index].containedIn = -1;
        }
        if (items[index].state == ITEM_IN_STOCKPILE) {
            RemoveItemFromStockpileSlot(items[index].x, items[index].y, (int)items[index].z);
        }
        items[index].active = false;
        items[index].reservedBy = -1;
        itemCount--;
        // Shrink high water mark if we deleted the last item
        if (index == itemHighWaterMark - 1) {
            while (itemHighWaterMark > 0 && !items[itemHighWaterMark - 1].active) {
                itemHighWaterMark--;
            }
        }
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

// Forward declarations for spatial grid iteration (defined later in file)
typedef bool (*ItemRadiusIterator)(int itemIdx, float distSq, void* userData);
static int IterateItemsInRadius(int tileX, int tileY, int z, int radiusTiles,
                                ItemRadiusIterator iterator, void* userData);

// Naive O(MAX_ITEMS) implementation - scans all items
int FindNearestUnreservedItemNaive(float x, float y, float z) {
    int nearest = -1;
    float nearestDistSq = 1e30f;
    
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].reservedBy != -1) continue;  // skip reserved
        if (items[i].state != ITEM_ON_GROUND) continue;
        
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

// Callback context for spatial nearest-item search
typedef struct {
    float searchX, searchY;
    int nearestIdx;
    float nearestDistSq;
} NearestUnreservedContext;

static bool NearestUnreservedIterator(int itemIdx, float distSq, void* userData) {
    (void)distSq;  // We recalculate with actual position for accuracy
    NearestUnreservedContext* ctx = (NearestUnreservedContext*)userData;
    
    if (items[itemIdx].reservedBy != -1) return true;  // skip reserved, continue
    
    float dx = items[itemIdx].x - ctx->searchX;
    float dy = items[itemIdx].y - ctx->searchY;
    float d = dx * dx + dy * dy;
    
    if (d < ctx->nearestDistSq) {
        ctx->nearestDistSq = d;
        ctx->nearestIdx = itemIdx;
    }
    return true;  // continue searching all in radius
}

// Optimized implementation using spatial grid with expanding radius
int FindNearestUnreservedItem(float x, float y, float z) {
    // Fall back to naive if spatial grid not built
    if (!itemGrid.cellCounts || itemGrid.groundItemCount == 0) {
        return FindNearestUnreservedItemNaive(x, y, z);
    }
    
    int tileX = (int)(x / CELL_SIZE);
    int tileY = (int)(y / CELL_SIZE);
    int tileZ = (int)z;
    
    NearestUnreservedContext ctx = {
        .searchX = x,
        .searchY = y,
        .nearestIdx = -1,
        .nearestDistSq = 1e30f
    };
    
    // Expanding radius search: try small radius first, expand if needed
    int radii[] = {5, 15, 30, 60, 120};
    int numRadii = sizeof(radii) / sizeof(radii[0]);
    
    for (int r = 0; r < numRadii; r++) {
        IterateItemsInRadius(tileX, tileY, tileZ, radii[r], NearestUnreservedIterator, &ctx);
        
        if (ctx.nearestIdx >= 0) {
            // Found something - but need to check if there could be closer items
            // at the edge of our search radius. If nearest is well within radius, we're done.
            float nearestDist = ctx.nearestDistSq;
            float radiusDistSq = (float)(radii[r] * radii[r] * CELL_SIZE * CELL_SIZE);
            
            // If nearest item is closer than 70% of search radius, it's definitely the nearest
            if (nearestDist < radiusDistSq * 0.5f) {
                return ctx.nearestIdx;
            }
            // Otherwise expand to next radius to be sure
        }
    }
    
    // Return whatever we found (or -1 if nothing)
    return ctx.nearestIdx;
}

// Naive O(MAX_ITEMS) implementation - iterates entire array
void ItemsTickNaive(float dt) {
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

// Optimized - only iterates up to high water mark
void ItemsTick(float dt) {
    for (int i = 0; i < itemHighWaterMark; i++) {
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
        EventLog("Item %d (%s) marked unreachable for %.1fs at (%d,%d,z%d)",
                 itemIndex, ItemName(items[itemIndex].type), cooldown,
                 (int)(items[itemIndex].x / CELL_SIZE), (int)(items[itemIndex].y / CELL_SIZE), (int)items[itemIndex].z);
    }
}

void ClearUnreachableCooldownsNearCell(int x, int y, int z, int radius) {
    // Clear unreachable cooldowns for items near a cell where terrain changed
    // This allows immediate re-evaluation of item reachability after mining/building
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].unreachableCooldown <= 0.0f) continue;
        
        int itemX = (int)(items[i].x / CELL_SIZE);
        int itemY = (int)(items[i].y / CELL_SIZE);
        int itemZ = (int)items[i].z;
        
        int dx = itemX - x;
        int dy = itemY - y;
        int dz = itemZ - z;
        
        if (dx*dx + dy*dy + dz*dz <= radius*radius) {
            items[i].unreachableCooldown = 0.0f;
        }
    }
}

static inline int clampi_item(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int FindGroundItemAtTile(int tileX, int tileY, int z) {
    // Use spatial grid for O(1) lookup if grid has been built with items
    // (groundItemCount > 0 means BuildItemSpatialGrid was called after items existed)
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        return QueryItemAtTile(tileX, tileY, z);
    }
    
    // Fallback to O(n) scan if grid not built yet or was built when empty
    for (int i = 0; i < itemHighWaterMark; i++) {
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

void SafeDropItem(int itemIdx, float x, float y, int z) {
    if (itemIdx < 0 || !items[itemIdx].active) return;

    Item* item = &items[itemIdx];
    item->state = ITEM_ON_GROUND;
    item->reservedBy = -1;

    int cellX = (int)(x / CELL_SIZE);
    int cellY = (int)(y / CELL_SIZE);

    if (IsCellWalkableAt(z, cellY, cellX)) {
        item->x = x;
        item->y = y;
        item->z = (float)z;
    } else {
        int dx[] = {0, 0, -1, 1, -1, 1, -1, 1};
        int dy[] = {-1, 1, 0, 0, -1, -1, 1, 1};
        bool found = false;
        for (int d = 0; d < 8; d++) {
            int nx = cellX + dx[d];
            int ny = cellY + dy[d];
            if (IsCellWalkableAt(z, ny, nx)) {
                item->x = nx * CELL_SIZE + CELL_SIZE / 2.0f;
                item->y = ny * CELL_SIZE + CELL_SIZE / 2.0f;
                item->z = (float)z;
                found = true;
                break;
            }
        }
        if (!found) {
            item->x = x;
            item->y = y;
            item->z = (float)z;
        }
    }
}

void PushItemsOutOfCell(int x, int y, int z) {
    // Direction offsets for cardinal neighbors
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};
    
    // Find first walkable neighbor to push items to
    int targetX = -1, targetY = -1;
    for (int d = 0; d < 4; d++) {
        int nx = x + dx[d];
        int ny = y + dy[d];
        if (IsCellWalkableAt(z, ny, nx)) {
            targetX = nx;
            targetY = ny;
            break;
        }
    }
    
    // Move all items at this cell to the target neighbor
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if ((int)items[i].z != z) continue;
        
        int itemTileX = (int)(items[i].x / CELL_SIZE);
        int itemTileY = (int)(items[i].y / CELL_SIZE);
        
        if (itemTileX == x && itemTileY == y) {
            if (items[i].state == ITEM_IN_STOCKPILE) {
                RemoveItemFromStockpileSlot(items[i].x, items[i].y, (int)items[i].z);
                items[i].state = ITEM_ON_GROUND;
            }
            if (targetX >= 0) {
                // Move to walkable neighbor
                items[i].x = targetX * CELL_SIZE + CELL_SIZE * 0.5f;
                items[i].y = targetY * CELL_SIZE + CELL_SIZE * 0.5f;
            }
            // If no walkable neighbor, item stays (will be trapped in wall)
            // This is an edge case - fully surrounded cells shouldn't have blueprints
        }
    }
}

void DropItemsInCell(int x, int y, int z) {
    if (z <= 0) return;  // Can't drop below z=0
    
    // Find the lowest z-level the item can fall to
    int targetZ = z - 1;
    while (targetZ > 0 && !CellIsSolid(grid[targetZ - 1][y][x])) {
        targetZ--;
    }
    
    // Check if there's actually somewhere to fall
    if (CellIsSolid(grid[targetZ][y][x])) return;  // No open space below
    
    // Move all items at this cell down to the target z-level
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if ((int)items[i].z != z) continue;
        
        int itemTileX = (int)(items[i].x / CELL_SIZE);
        int itemTileY = (int)(items[i].y / CELL_SIZE);
        
        if (itemTileX == x && itemTileY == y) {
            // Clear stockpile slot before changing position
            if (items[i].state == ITEM_IN_STOCKPILE) {
                RemoveItemFromStockpileSlot(items[i].x, items[i].y, (int)items[i].z);
                items[i].state = ITEM_ON_GROUND;
            }
            items[i].z = (float)targetZ;
        }
    }
}

// Common spatial grid radius iteration - calls iterator for each valid ground item in radius
// Returns number of items visited (or partial count if stopped early)
// (ItemRadiusIterator typedef is forward-declared above)
static int IterateItemsInRadius(int tileX, int tileY, int z, int radiusTiles,
                                ItemRadiusIterator iterator, void* userData) {
    if (!itemGrid.cellCounts) return 0;
    if (z < 0 || z >= itemGrid.gridD) return 0;
    
    int visited = 0;
    float radiusSq = (float)(radiusTiles * radiusTiles);
    
    int minTx = clampi_item(tileX - radiusTiles, 0, itemGrid.gridW - 1);
    int maxTx = clampi_item(tileX + radiusTiles, 0, itemGrid.gridW - 1);
    int minTy = clampi_item(tileY - radiusTiles, 0, itemGrid.gridH - 1);
    int maxTy = clampi_item(tileY + radiusTiles, 0, itemGrid.gridH - 1);
    
    // Search z-1, z, z+1 to support hilly terrain where items may be one z-level off
    int zMin = (z > 0) ? z - 1 : 0;
    int zMax = (z < itemGrid.gridD - 1) ? z + 1 : z;
    
    for (int sz = zMin; sz <= zMax; sz++) {
        for (int ty = minTy; ty <= maxTy; ty++) {
            for (int tx = minTx; tx <= maxTx; tx++) {
                int cellIdx = sz * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;
                int start = itemGrid.cellStarts[cellIdx];
                int end = itemGrid.cellStarts[cellIdx + 1];
                
                for (int t = start; t < end; t++) {
                    int itemIdx = itemGrid.itemIndices[t];
                    Item* item = &items[itemIdx];
                    
                    if (!item->active || item->state != ITEM_ON_GROUND) continue;
                    
                    int itemTileX = (int)(item->x / CELL_SIZE);
                    int itemTileY = (int)(item->y / CELL_SIZE);
                    float dx = (float)(itemTileX - tileX);
                    float dy = (float)(itemTileY - tileY);
                    float distSq = dx * dx + dy * dy;
                    
                    if (distSq <= radiusSq) {
                        visited++;
                        if (!iterator(itemIdx, distSq, userData)) {
                            return visited;
                        }
                    }
                }
            }
        }
    }
    
    return visited;
}

// Iterator for QueryItemsInRadius - always continues, calls user callback
typedef struct {
    ItemNeighborCallback callback;
    void* userData;
} QueryContext;

static bool QueryIterator(int itemIdx, float distSq, void* userData) {
    QueryContext* ctx = (QueryContext*)userData;
    if (ctx->callback) {
        ctx->callback(itemIdx, distSq, ctx->userData);
    }
    return true;
}

int QueryItemsInRadius(int tileX, int tileY, int z, int radiusTiles,
                       ItemNeighborCallback callback, void* userData) {
    QueryContext ctx = { .callback = callback, .userData = userData };
    return IterateItemsInRadius(tileX, tileY, z, radiusTiles, QueryIterator, &ctx);
}

// Iterator for FindFirstItemInRadius - stops when filter matches
typedef struct {
    ItemFilterFunc filter;
    void* userData;
    int foundIdx;
} FindFirstContext;

static bool FindFirstIterator(int itemIdx, float distSq, void* userData) {
    (void)distSq;
    FindFirstContext* ctx = (FindFirstContext*)userData;
    if (ctx->filter(itemIdx, ctx->userData)) {
        ctx->foundIdx = itemIdx;
        return false;
    }
    return true;
}

int FindFirstItemInRadius(int tileX, int tileY, int z, int radiusTiles,
                          ItemFilterFunc filter, void* userData) {
    FindFirstContext ctx = { .filter = filter, .userData = userData, .foundIdx = -1 };
    IterateItemsInRadius(tileX, tileY, z, radiusTiles, FindFirstIterator, &ctx);
    return ctx.foundIdx;
}

// =============================================================================
// ItemSpatialGrid Implementation
// =============================================================================

ItemSpatialGrid itemGrid = {0};

// Defined earlier, before QueryItemsInRadius

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

// Naive O(MAX_ITEMS) implementation - iterates entire array twice
void BuildItemSpatialGridNaive(void) {
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

// Optimized - only iterates up to itemHighWaterMark
void BuildItemSpatialGrid(void) {
    if (!itemGrid.cellCounts) return;
    
    // Phase 1: Clear counts
    memset(itemGrid.cellCounts, 0, itemGrid.cellCount * sizeof(int));
    itemGrid.groundItemCount = 0;
    
    // Phase 2: Count ground items per cell (optimized range)
    for (int i = 0; i < itemHighWaterMark; i++) {
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
    
    // Phase 5: Scatter item indices into cells (optimized range)
    for (int i = 0; i < itemHighWaterMark; i++) {
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
