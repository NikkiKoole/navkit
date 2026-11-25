#include "items.h"
#include "item_defs.h"  // for ItemName
#include "mover.h"  // for CELL_SIZE
#include "../world/grid.h"   // for gridWidth, gridHeight, gridDepth
#include "../world/material.h"  // for MaterialType
#include "../core/event_log.h"
#include "../world/cell_defs.h"  // for IsCellWalkableAt
#include "stockpiles.h"  // for MarkStockpileGroundItem
#include "containers.h"  // for ContainerDef, GetContainerDef, GetOutermostContainer
#include "furniture.h"   // CAPABILITY, CAP_COLD_STORAGE, fridgeSpoilageFactor
#include "../simulation/temperature.h"  // for temperatureGrid
#include "../simulation/balance.h"  // GameHoursToGameSeconds
#include "../simulation/water.h"    // GetWaterFlow (drifting)
#include "../simulation/explosion.h"  // ClearExplosions
#include <math.h>
#include <stdlib.h>
#include <string.h>

Item items[MAX_ITEMS];
int itemCount = 0;
int itemHighWaterMark = 0;  // Highest index + 1 ever active

void ClearItems(void) {
    ClearExplosions();   // flying items hold indices
    for (int i = 0; i < itemHighWaterMark; i++) {  // Only clear up to high water mark
        items[i].active = false;
        items[i].reservedBy = -1;
        items[i].unreachableCooldown = 0.0f;
        items[i].material = MAT_NONE;
        items[i].natural = false;
        items[i].stackCount = 1;
        items[i].containedIn = -1;
        items[i].firstChild = items[i].nextSibling = -1;
        items[i].contentCount = 0;
        items[i].contentTypeMask = 0;
        items[i].spoilageTimer = 0.0f;
        items[i].condition = CONDITION_FRESH;
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
            items[i].firstChild = items[i].nextSibling = -1;
            items[i].contentCount = 0;
            items[i].contentTypeMask = 0;
            items[i].spoilageTimer = 0.0f;
            items[i].condition = CONDITION_FRESH;
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

ItemType ItemLeavesBehind(ItemType type) {
    switch (type) {
        case ITEM_SODA:  return ITEM_EMPTY_CAN;
        case ITEM_SNACK: return ITEM_WRAPPER;
        case ITEM_BURGER: return ITEM_WRAPPER;
        default:         return ITEM_NONE;
    }
}

int ItemLeftoverUnits(ItemType type, float need) {
    if (type != ITEM_BURGER) return 0;
    if (need < 0.0f) need = 0.0f;
    float rest = ItemNutrition(type) - need;
    if (rest < 1.5f * LEFTOVER_UNIT_NUTRITION) return 0;   // a bite or two: eaten anyway
    return (int)(rest / LEFTOVER_UNIT_NUTRITION + 0.5f);
}

void DeleteItem(int index) {
    if (index >= 0 && index < MAX_ITEMS && items[index].active) {
        EventLog("DeleteItem %d (%s) at (%.0f,%.0f,z%.0f) state=%d",
                 index, ItemName(items[index].type), items[index].x, items[index].y, items[index].z, items[index].state);
        // If this is a container with contents, spill them first
        if (items[index].contentCount > 0) {
            SpillContainerContents(index);
        }
        // If this item is inside a container, take it out of the container's contents
        if (items[index].containedIn != -1) ContainerUnlink(index);
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
    if (!itemGrid.cellFirst || itemGrid.groundItemCount == 0) {
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

// Advance spoilage timers and update item condition
static void SpoilageTick(float dt) {
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (!ItemSpoils(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;  // Already rotten, stop
        float limit = ItemSpoilageLimit(items[i].type);
        if (limit <= 0.0f) continue;
        // Don't spoil carried items (prevents mid-job deletion)
        if (items[i].state == ITEM_CARRIED) continue;

        // Container modifier: walk to outermost container
        float rate = dt;
        if (items[i].containedIn != -1) {
            int outermost = GetOutermostContainer(i);
            // Skip items in a carried container
            if (items[outermost].state == ITEM_CARRIED) continue;
            const ContainerDef* cdef = GetContainerDef(items[outermost].type);
            if (cdef) rate = dt * cdef->spoilageModifier;
        }

        // Kept on a fridge (cold storage on its cell): spoils slower
        int sx = (int)(items[i].x / CELL_SIZE), sy = (int)(items[i].y / CELL_SIZE), sz = (int)items[i].z;
        if (sx >= 0 && sx < gridWidth && sy >= 0 && sy < gridHeight && sz >= 0 && sz < gridDepth &&
            (CAPABILITY(sx, sy, sz) & CAP_COLD_STORAGE)) rate *= fridgeSpoilageFactor;

        items[i].spoilageTimer += rate;

        // Update condition based on timer ratio
        float ratio = items[i].spoilageTimer / limit;
        if (ratio >= 1.0f) {
            items[i].condition = CONDITION_ROTTEN;
            EventLog("SPOILED item %d (%s x%d) at (%.0f,%.0f,z%.0f) after %.0fs → rotten",
                     i, ItemName(items[i].type), items[i].stackCount,
                     items[i].x, items[i].y, items[i].z, items[i].spoilageTimer);
        } else if (ratio >= 0.5f) {
            items[i].condition = CONDITION_STALE;
        }
    }
}

// Optimized - only iterates up to high water mark
// Cool hot items toward ambient temperature (Newton's cooling). The gap to ambient halves
// every foodCoolingHalfLifeGH game hours: 0.5 = a meal from the stove (80C) is below
// "warm" (40C) after ~45 game minutes, at any day length.
float foodCoolingHalfLifeGH = 0.5f;
static void TemperatureTick(float dt) {
    float halfLife = GameHoursToGameSeconds(foodCoolingHalfLifeGH);
    float keep = halfLife > 0.0f ? expf(-0.69314718f * dt / halfLife) : 0.0f;   // share of the gap left
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].temperature <= 0.0f) continue;
        int tx = (int)(items[i].x / CELL_SIZE);
        int ty = (int)(items[i].y / CELL_SIZE);
        int tz = (int)items[i].z;
        float ambient = 20.0f;  // default room temp
        if (tx >= 0 && tx < gridWidth && ty >= 0 && ty < gridHeight && tz >= 0 && tz < gridDepth) {
            ambient = (float)GetTemperature(tx, ty, tz);
        }
        items[i].temperature = ambient + (items[i].temperature - ambient) * keep;
        if (fabsf(items[i].temperature - ambient) < 0.5f) {
            items[i].temperature = 0.0f;  // close enough — stop tracking
        }
    }
}

// ---- Drifting: loose things that float go with the current ----
bool itemsDrift = true;   // setting itemsDrift
#define DRIFT_INTERVAL 0.1f      // game-seconds between drift steps
static float driftAccum = 0.0f;

// How deep the water must be for an item to float off (0 = it sinks or stays): light plant
// matter goes in any water (1), logs, planks and other wood need 2
static int ItemFloatDepth(const Item* it) {
    switch (it->type) {
        case ITEM_STICKS: case ITEM_LEAVES: case ITEM_GRASS: case ITEM_DRIED_GRASS: case ITEM_BARK:
        case ITEM_REEDS: case ITEM_SAPLING:
            return 1;
        case ITEM_LOG: case ITEM_PLANKS: case ITEM_POLES: case ITEM_STRIPPED_LOG: case ITEM_BASKET:
        case ITEM_REED_MAT:
            return 2;
        default:
            return IsWoodMaterial((MaterialType)it->material) ? 2 : 0;
    }
}

// Can a drifting item be in this cell (inside the map, not inside a wall)?
static bool DriftCellOpen(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    CellType c = GRID(x, y, z);
    return !CellIsSolid(c) && !CellBlocksFluids(c);
}

static bool DriftEdgeCell(int x, int y) {
    return x == 0 || y == 0 || x == gridWidth - 1 || y == gridHeight - 1;
}

// The water a floating item at (x, y, z) rests on: this cell's water, or (in an empty cell with
// no floor) the water right below it
static bool DriftOverWater(int x, int y, int z) {
    if (WATER(x, y, z).level > 0) return true;
    return z > 0 && !HAS_FLOOR(x, y, z) && DriftCellOpen(x, y, z - 1) && WATER(x, y, z - 1).level > 0;
}

// Float to the surface: up while the cell above holds water, down while this one is empty air
// over water. Returns the z the item floats at.
static int DriftSurfaceZ(int x, int y, int z) {
    while (z + 1 < gridDepth && WATER(x, y, z).level > 0 && !HAS_FLOOR(x, y, z + 1) &&
           DriftCellOpen(x, y, z + 1) && WATER(x, y, z + 1).level > 0) z++;
    while (z > 0 && WATER(x, y, z).level == 0 && !HAS_FLOOR(x, y, z) &&
           DriftCellOpen(x, y, z - 1) && WATER(x, y, z - 1).level > 0) z--;
    return z;
}

// Loose floating items go with the current (GetWaterFlow), 0.15-1 cell per game-second by its
// strength. They float at the surface (a log on a full river bed comes up to the moving layer)
// and need water ItemFloatDepth deep (light matter 1, wood 2), counting a full cell below.
// Carried off the map edge (where water flows off, waterEdgeOutflow) they are lost. Not carried, stored,
// contained or reserved: a job never walks to a spot its item has left. Blocked diagonally they
// slide along the bank; with no water ahead at all (a bank, a beach) they stay where they are.
static void ItemsDriftTick(float dt) {
    if (!itemsDrift || waterActiveCells == 0) return;
    driftAccum += dt;
    if (driftAccum < DRIFT_INTERVAL) return;
    float step = driftAccum;
    driftAccum = 0.0f;
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* it = &items[i];
        if (!it->active || it->state != ITEM_ON_GROUND || it->containedIn >= 0 || it->reservedBy >= 0) continue;
        int cx = (int)(it->x / CELL_SIZE), cy = (int)(it->y / CELL_SIZE), cz = (int)it->z;
        if (cx < 0 || cy < 0 || cz < 0 || cx >= gridWidth || cy >= gridHeight || cz >= gridDepth) continue;
        // An open edge cell where water flows off the map: empty as fast as it fills, but moving
        bool edgeHere = waterEdgeOutflow && cz > waterEdgeSeaZ && DriftEdgeCell(cx, cy);
        if (!edgeHere && !DriftOverWater(cx, cy, cz)) continue;   // dry: the common case, first
        int needDepth = ItemFloatDepth(it);
        if (needDepth == 0) continue;
        if (!edgeHere) {
            cz = DriftSurfaceZ(cx, cy, cz);
            it->z = (float)cz;
            const WaterCell* w = &WATER(cx, cy, cz);
            if (w->isFrozen) continue;
            int depth = w->level + (cz > 0 && WATER(cx, cy, cz - 1).level == WATER_MAX_LEVEL ? WATER_MAX_LEVEL : 0);
            if (depth < needDepth) continue;
        }
        // The current of the surface layer and the one under it (a full river moves on top)
        float fx = 0.0f, fy = 0.0f, gx, gy;
        if (GetWaterFlow(cx, cy, cz, &gx, &gy)) { fx += gx; fy += gy; }
        if (cz > 0 && GetWaterFlow(cx, cy, cz - 1, &gx, &gy)) { fx += gx; fy += gy; }
        float m = sqrtf(fx * fx + fy * fy);
        if (m < 0.5f) continue;
        float strength = log2f(1.0f + m) / 5.0f;
        if (strength > 1.0f) strength = 1.0f;
        float dist = (0.15f + 0.85f * strength) * step * CELL_SIZE;
        float mx = fx / m * dist, my = fy / m * dist;
        // Try the step, then (blocked) its x part, then its y part: slide along a bank
        float tryX[3] = {mx, mx, 0.0f}, tryY[3] = {my, 0.0f, my};
        for (int t = 0; t < 3; t++) {
            if (tryX[t] == 0.0f && tryY[t] == 0.0f) continue;
            float nx = it->x + tryX[t], ny = it->y + tryY[t];
            int ncx = (int)(nx / CELL_SIZE), ncy = (int)(ny / CELL_SIZE);
            int nz = cz;
            if (ncx != cx || ncy != cy) {
                bool offMap = ncx < 0 || ncy < 0 || ncx >= gridWidth || ncy >= gridHeight;
                if (offMap && waterEdgeOutflow) {   // carried off the map: lost to the sea beyond
                    EventLog("Item %d (%s) drifted off the map at (%d,%d,z%d)", i, ItemName(it->type), cx, cy, cz);
                    DeleteItem(i);
                    break;
                }
                if (!DriftCellOpen(ncx, ncy, cz)) continue;
                // An open edge cell where water flows off is empty as fast as it fills: the item
                // goes in, and on, off the map
                bool edgeOut = waterEdgeOutflow && cz > waterEdgeSeaZ && DriftEdgeCell(ncx, ncy);
                if (!edgeOut && !DriftOverWater(ncx, ncy, cz)) continue;
                nz = DriftSurfaceZ(ncx, ncy, cz);   // over a drop it goes down with the water
            }
            it->x = nx;
            it->y = ny;
            it->z = (float)nz;
            break;
        }
    }
}

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
    SpoilageTick(dt);
    TemperatureTick(dt);
    ItemsDriftTick(dt);
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
    if (itemGrid.cellFirst && itemGrid.groundItemCount > 0) {
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
    while (targetZ > 0 && !CellIsSolid(GRID(x, y, targetZ - 1))) {
        targetZ--;
    }
    
    // Check if there's actually somewhere to fall
    if (CellIsSolid(GRID(x, y, targetZ))) return;  // No open space below
    
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
    if (!itemGrid.cellFirst) return 0;
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
                int start, end;
                ItemGridCellRange(cellIdx, &start, &end);
                
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

void InitItemSpatialGrid(int tileWidth, int tileHeight, int depth) {
    FreeItemSpatialGrid();
    
    itemGrid.gridW = tileWidth;
    itemGrid.gridH = tileHeight;
    itemGrid.gridD = depth;
    itemGrid.cellCount = tileWidth * tileHeight * depth;
    itemGrid.groundItemCount = 0;
    
    if (itemGrid.cellCount <= 0) return;
    
    itemGrid.cellFirst = (int*)malloc(itemGrid.cellCount * sizeof(int));
    itemGrid.itemIndices = (int*)malloc(MAX_ITEMS * sizeof(int));
    itemGrid.itemCells = (int*)malloc(MAX_ITEMS * sizeof(int));
    if (!itemGrid.cellFirst || !itemGrid.itemIndices || !itemGrid.itemCells) { FreeItemSpatialGrid(); return; }
    memset(itemGrid.cellFirst, 0xff, itemGrid.cellCount * sizeof(int));   // all -1
}

void FreeItemSpatialGrid(void) {
    free(itemGrid.cellFirst);
    free(itemGrid.itemIndices);
    free(itemGrid.itemCells);
    itemGrid.cellFirst = NULL;
    itemGrid.itemIndices = NULL;
    itemGrid.itemCells = NULL;
    itemGrid.cellCount = 0;
    itemGrid.groundItemCount = 0;
}

void BuildItemSpatialGrid(void) {
    if (!itemGrid.cellFirst) return;
    static int tmpIdx[MAX_ITEMS], tmpCell[MAX_ITEMS];
    
    // Phase 1: clear only the cells the last build filled
    for (int t = 0; t < itemGrid.groundItemCount; t++) itemGrid.cellFirst[itemGrid.itemCells[t]] = -1;
    
    // Phase 2: collect ground items (in index order)
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        
        int tx = (int)(items[i].x / CELL_SIZE);
        int ty = (int)(items[i].y / CELL_SIZE);
        int tz = (int)items[i].z;
        
        tx = clampi_item(tx, 0, itemGrid.gridW - 1);
        ty = clampi_item(ty, 0, itemGrid.gridH - 1);
        tz = clampi_item(tz, 0, itemGrid.gridD - 1);
        
        itemGrid.itemIndices[n] = i;
        itemGrid.itemCells[n] = tz * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;
        n++;
    }
    itemGrid.groundItemCount = n;
    
    // Phase 3: stable LSD radix sort by cell, 11 bits per pass (index order kept
    // within a cell: same order as a full-map counting sort)
    int* srcI = itemGrid.itemIndices; int* srcC = itemGrid.itemCells;
    int* dstI = tmpIdx; int* dstC = tmpCell;
    for (int shift = 0; (itemGrid.cellCount - 1) >> shift; shift += 11) {
        int count[2048 + 1] = {0};
        for (int t = 0; t < n; t++) count[((srcC[t] >> shift) & 2047) + 1]++;
        for (int b = 0; b < 2048; b++) count[b + 1] += count[b];
        for (int t = 0; t < n; t++) {
            int d = count[(srcC[t] >> shift) & 2047]++;
            dstI[d] = srcI[t]; dstC[d] = srcC[t];
        }
        int* ti = srcI; srcI = dstI; dstI = ti;
        int* tc = srcC; srcC = dstC; dstC = tc;
    }
    if (srcI != itemGrid.itemIndices) {
        memcpy(itemGrid.itemIndices, srcI, n * sizeof(int));
        memcpy(itemGrid.itemCells, srcC, n * sizeof(int));
    }
    
    // Phase 4: first entry of each cell
    for (int t = n - 1; t >= 0; t--) itemGrid.cellFirst[itemGrid.itemCells[t]] = t;
}

int QueryItemAtTile(int tileX, int tileY, int z) {
    if (!itemGrid.cellFirst) return -1;
    if (tileX < 0 || tileX >= itemGrid.gridW) return -1;
    if (tileY < 0 || tileY >= itemGrid.gridH) return -1;
    if (z < 0 || z >= itemGrid.gridD) return -1;
    
    int cellIdx = z * (itemGrid.gridW * itemGrid.gridH) + tileY * itemGrid.gridW + tileX;
    int start, end;
    ItemGridCellRange(cellIdx, &start, &end);
    
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
