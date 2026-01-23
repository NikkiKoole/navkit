#include "jobs.h"
#include "items.h"
#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include "stockpiles.h"
#include "../shared/profiler.h"
#include "../vendor/raylib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Distance thresholds (relative to CELL_SIZE)
#define PICKUP_RADIUS (CELL_SIZE * 0.75f)  // Large enough to cover same-cell edge cases
#define DROP_RADIUS (CELL_SIZE * 0.75f)    // Same as pickup - covers whole cell reliably
#define JOB_STUCK_TIME 3.0f  // Cancel job if stuck for this long
#define UNREACHABLE_COOLDOWN 5.0f  // Seconds before retrying unreachable item

// Radius search for finding idle movers near items (in pixels)
#define MOVER_SEARCH_RADIUS (CELL_SIZE * 50)  // Search 50 tiles around item for idle mover

// =============================================================================
// Idle Mover Cache
// =============================================================================

int* idleMoverList = NULL;
int idleMoverCount = 0;
bool* moverIsInIdleList = NULL;
static int idleMoverCapacity = 0;

void InitJobSystem(int maxMovers) {
    FreeJobSystem();
    idleMoverCapacity = maxMovers;
    idleMoverList = (int*)malloc(maxMovers * sizeof(int));
    moverIsInIdleList = (bool*)calloc(maxMovers, sizeof(bool));
    idleMoverCount = 0;
}

void FreeJobSystem(void) {
    free(idleMoverList);
    free(moverIsInIdleList);
    idleMoverList = NULL;
    moverIsInIdleList = NULL;
    idleMoverCount = 0;
    idleMoverCapacity = 0;
}

void AddMoverToIdleList(int moverIdx) {
    if (!moverIsInIdleList || moverIdx < 0 || moverIdx >= idleMoverCapacity) return;
    if (moverIsInIdleList[moverIdx]) return;  // Already in list
    
    idleMoverList[idleMoverCount++] = moverIdx;
    moverIsInIdleList[moverIdx] = true;
}

void RemoveMoverFromIdleList(int moverIdx) {
    if (!moverIsInIdleList || moverIdx < 0 || moverIdx >= idleMoverCapacity) return;
    if (!moverIsInIdleList[moverIdx]) return;  // Not in list
    
    // Find and remove (swap with last element for O(1) removal)
    for (int i = 0; i < idleMoverCount; i++) {
        if (idleMoverList[i] == moverIdx) {
            idleMoverList[i] = idleMoverList[idleMoverCount - 1];
            idleMoverCount--;
            break;
        }
    }
    moverIsInIdleList[moverIdx] = false;
}

void RebuildIdleMoverList(void) {
    if (!moverIsInIdleList) return;
    
    idleMoverCount = 0;
    memset(moverIsInIdleList, 0, idleMoverCapacity * sizeof(bool));
    
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active && movers[i].jobState == JOB_IDLE) {
            idleMoverList[idleMoverCount++] = i;
            moverIsInIdleList[i] = true;
        }
    }
}

// =============================================================================
// Mover Search Callback (find nearest idle mover to an item)
// =============================================================================

typedef struct {
    float itemX, itemY;
    int bestMoverIdx;
    float bestDistSq;
} IdleMoverSearchContext;

static void IdleMoverSearchCallback(int moverIdx, float distSq, void* userData) {
    IdleMoverSearchContext* ctx = (IdleMoverSearchContext*)userData;
    
    // Skip if not in idle list (already has a job or not active)
    if (!moverIsInIdleList || !moverIsInIdleList[moverIdx]) return;
    
    if (distSq < ctx->bestDistSq) {
        ctx->bestDistSq = distSq;
        ctx->bestMoverIdx = moverIdx;
    }
}

// Helper: clear item from source stockpile slot when re-hauling
static void ClearSourceStockpileSlot(Item* item) {
    int sourceSp = -1;
    if (!IsPositionInStockpile(item->x, item->y, (int)item->z, &sourceSp) || sourceSp < 0) return;
    
    int lx = (int)(item->x / CELL_SIZE) - stockpiles[sourceSp].x;
    int ly = (int)(item->y / CELL_SIZE) - stockpiles[sourceSp].y;
    if (lx < 0 || lx >= stockpiles[sourceSp].width || ly < 0 || ly >= stockpiles[sourceSp].height) return;
    
    int idx = ly * stockpiles[sourceSp].width + lx;
    stockpiles[sourceSp].slotCounts[idx]--;
    if (stockpiles[sourceSp].slotCounts[idx] <= 0) {
        stockpiles[sourceSp].slots[idx] = -1;
        stockpiles[sourceSp].slotTypes[idx] = -1;
        stockpiles[sourceSp].slotCounts[idx] = 0;
    }
}

// Helper: cancel job and release all reservations
static void CancelJob(Mover* m, int moverIdx) {
    (void)moverIdx;  // reserved for future use
    // Release item reservation
    if (m->targetItem >= 0) {
        ReleaseItemReservation(m->targetItem);
    }
    
    // Release stockpile slot reservation
    if (m->targetStockpile >= 0) {
        ReleaseStockpileSlot(m->targetStockpile, m->targetSlotX, m->targetSlotY);
    }
    
    // If carrying, safe-drop the item
    if (m->carryingItem >= 0 && items[m->carryingItem].active) {
        Item* item = &items[m->carryingItem];
        item->state = ITEM_ON_GROUND;
        item->x = m->x;
        item->y = m->y;
        item->z = m->z;
        item->reservedBy = -1;
    }
    
    // Reset mover state
    m->jobState = JOB_IDLE;
    m->targetItem = -1;
    m->carryingItem = -1;
    m->targetStockpile = -1;
    m->targetSlotX = -1;
    m->targetSlotY = -1;
    
    // Add back to idle list
    AddMoverToIdleList(moverIdx);
}

// Helper: Try to assign a job for a specific item to a nearby idle mover
// Returns true if assignment succeeded, false otherwise
static bool TryAssignItemToMover(int itemIdx, int spIdx, int slotX, int slotY, bool safeDrop) {
    Item* item = &items[itemIdx];
    
    int moverIdx = -1;
    
    // Use MoverSpatialGrid if available and built (has indexed movers)
    if (moverGrid.cellCounts && moverGrid.cellStarts[moverGrid.cellCount] > 0) {
        IdleMoverSearchContext ctx = {
            .itemX = item->x,
            .itemY = item->y,
            .bestMoverIdx = -1,
            .bestDistSq = 1e30f
        };
        
        QueryMoverNeighbors(item->x, item->y, MOVER_SEARCH_RADIUS, -1, IdleMoverSearchCallback, &ctx);
        moverIdx = ctx.bestMoverIdx;
    } else {
        // Fallback: find first idle mover (for tests without spatial grid)
        float bestDistSq = 1e30f;
        for (int i = 0; i < idleMoverCount; i++) {
            int idx = idleMoverList[i];
            float dx = movers[idx].x - item->x;
            float dy = movers[idx].y - item->y;
            float distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                moverIdx = idx;
            }
        }
    }
    
    if (moverIdx < 0) return false;  // No idle mover found
    Mover* m = &movers[moverIdx];
    
    // Reserve item
    if (!ReserveItem(itemIdx, moverIdx)) return false;
    
    // Reserve stockpile slot (unless safeDrop)
    if (!safeDrop) {
        if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx)) {
            ReleaseItemReservation(itemIdx);
            return false;
        }
    }
    
    // Quick reachability check
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    PROFILE_ACCUM_BEGIN(Jobs_ReachabilityCheck);
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    PROFILE_ACCUM_END(Jobs_ReachabilityCheck);
    
    if (tempLen == 0) {
        // Can't reach - release reservations
        ReleaseItemReservation(itemIdx);
        if (!safeDrop) {
            ReleaseStockpileSlot(spIdx, slotX, slotY);
        }
        SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        return false;
    }
    
    // Assign job to mover
    m->targetItem = itemIdx;
    m->targetStockpile = spIdx;
    m->targetSlotX = safeDrop ? -1 : slotX;
    m->targetSlotY = safeDrop ? -1 : slotY;
    m->jobState = JOB_MOVING_TO_ITEM;
    m->goal = itemCell;
    m->needsRepath = true;
    
    // Remove mover from idle list
    RemoveMoverFromIdleList(moverIdx);
    
    return true;
}

void AssignJobs(void) {
    // Rebuild idle mover list if not initialized
    if (!moverIsInIdleList) {
        InitJobSystem(MAX_MOVERS);
    }
    
    // Rebuild idle list each frame (fast O(moverCount) scan)
    // This ensures correctness after movers spawn/despawn
    RebuildIdleMoverList();
    
    // Early exit: no idle movers means no work to do
    if (idleMoverCount == 0) return;
    
    // Cache which item types have available stockpiles
    bool typeHasStockpile[3] = {false, false, false};
    for (int t = 0; t < 3; t++) {
        int slotX, slotY;
        if (FindStockpileForItem((ItemType)t, &slotX, &slotY) >= 0) {
            typeHasStockpile[t] = true;
        }
    }
    
    // PRIORITY 1: Handle ground items on stockpile tiles (absorb/clear jobs)
    PROFILE_ACCUM_BEGIN(Jobs_FindStockpileItem);
    while (idleMoverCount > 0) {
        int spOnItem = -1;
        bool absorb = false;
        int itemIdx = FindGroundItemOnStockpile(&spOnItem, &absorb);
        
        if (itemIdx < 0 || items[itemIdx].unreachableCooldown > 0.0f) break;
        
        int slotX, slotY, spIdx;
        bool safeDrop = false;
        
        if (absorb) {
            // Absorb: destination is same tile
            spIdx = spOnItem;
            slotX = (int)(items[itemIdx].x / CELL_SIZE);
            slotY = (int)(items[itemIdx].y / CELL_SIZE);
        } else {
            // Clear: find destination or safe-drop
            spIdx = FindStockpileForItem(items[itemIdx].type, &slotX, &slotY);
            if (spIdx < 0) {
                safeDrop = true;
            }
        }
        
        if (!TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, safeDrop)) {
            // Couldn't assign - mark item and move on
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        }
    }
    PROFILE_ACCUM_END(Jobs_FindStockpileItem);
    
    // PRIORITY 2a: Stockpile-centric - for each stockpile, find nearby items
    // This is efficient when you have few stockpiles and many items
    PROFILE_ACCUM_BEGIN(Jobs_FindGroundItem_StockpileCentric);
    if (idleMoverCount > 0 && itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        for (int spIdx = 0; spIdx < MAX_STOCKPILES && idleMoverCount > 0; spIdx++) {
            Stockpile* sp = &stockpiles[spIdx];
            if (!sp->active) continue;
            
            // Check each item type this stockpile accepts
            for (int t = 0; t < 3 && idleMoverCount > 0; t++) {
                if (!sp->allowedTypes[t]) continue;
                
                // Find a free slot for this type in this stockpile
                int slotX, slotY;
                if (!FindFreeStockpileSlot(spIdx, (ItemType)t, &slotX, &slotY)) continue;
                
                // Search for items near the stockpile center
                int centerTileX = sp->x + sp->width / 2;
                int centerTileY = sp->y + sp->height / 2;
                
                // Use expanding radius to find closest items first
                int radii[] = {10, 25, 50, 100};
                int numRadii = sizeof(radii) / sizeof(radii[0]);
                
                for (int r = 0; r < numRadii && idleMoverCount > 0; r++) {
                    int radius = radii[r];
                    
                    // Iterate items in this radius
                    int minTx = centerTileX - radius;
                    int maxTx = centerTileX + radius;
                    int minTy = centerTileY - radius;
                    int maxTy = centerTileY + radius;
                    
                    // Clamp to grid bounds
                    if (minTx < 0) minTx = 0;
                    if (minTy < 0) minTy = 0;
                    if (maxTx >= itemGrid.gridW) maxTx = itemGrid.gridW - 1;
                    if (maxTy >= itemGrid.gridH) maxTy = itemGrid.gridH - 1;
                    
                    bool foundItem = false;
                    for (int ty = minTy; ty <= maxTy && idleMoverCount > 0 && !foundItem; ty++) {
                        for (int tx = minTx; tx <= maxTx && idleMoverCount > 0 && !foundItem; tx++) {
                            int cellIdx = sp->z * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;
                            int start = itemGrid.cellStarts[cellIdx];
                            int end = itemGrid.cellStarts[cellIdx + 1];
                            
                            for (int i = start; i < end && idleMoverCount > 0; i++) {
                                int itemIdx = itemGrid.itemIndices[i];
                                Item* item = &items[itemIdx];
                                
                                if (!item->active) continue;
                                if (item->reservedBy != -1) continue;
                                if (item->state != ITEM_ON_GROUND) continue;
                                if (item->type != (ItemType)t) continue;
                                if (item->unreachableCooldown > 0.0f) continue;
                                if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) continue;
                                
                                // Try to assign this item to this stockpile slot
                                if (TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, false)) {
                                    foundItem = true;
                                    break;  // Move to next slot/type
                                }
                            }
                        }
                    }
                    
                    if (foundItem) break;  // Found one, try next slot
                }
            }
        }
    }
    PROFILE_ACCUM_END(Jobs_FindGroundItem_StockpileCentric);
    
    // PRIORITY 2b: Item-centric fallback - for remaining items without nearby stockpiles
    // This catches items that weren't near any stockpile in the search above
    PROFILE_ACCUM_BEGIN(Jobs_FindGroundItem_ItemCentric);
    if (idleMoverCount > 0 && itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        int totalIndexed = itemGrid.cellStarts[itemGrid.cellCount];
        
        for (int t = 0; t < totalIndexed && idleMoverCount > 0; t++) {
            int itemIdx = itemGrid.itemIndices[t];
            Item* item = &items[itemIdx];
            
            // Skip invalid items
            if (!item->active) continue;
            if (item->reservedBy != -1) continue;
            if (item->state != ITEM_ON_GROUND) continue;
            if (item->unreachableCooldown > 0.0f) continue;
            if (!typeHasStockpile[item->type]) continue;
            if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) continue;
            
            // Find stockpile for this item
            int slotX, slotY;
            int spIdx = FindStockpileForItem(item->type, &slotX, &slotY);
            if (spIdx < 0) continue;
            
            TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, false);
        }
    } else if (idleMoverCount > 0) {
        // Fallback: linear scan when spatial grid not built (tests)
        for (int j = 0; j < MAX_ITEMS && idleMoverCount > 0; j++) {
            Item* item = &items[j];
            if (!item->active) continue;
            if (item->reservedBy != -1) continue;
            if (item->state != ITEM_ON_GROUND) continue;
            if (item->unreachableCooldown > 0.0f) continue;
            if (!typeHasStockpile[item->type]) continue;
            if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) continue;
            
            int slotX, slotY;
            int spIdx = FindStockpileForItem(item->type, &slotX, &slotY);
            if (spIdx < 0) continue;
            
            TryAssignItemToMover(j, spIdx, slotX, slotY, false);
        }
    }
    PROFILE_ACCUM_END(Jobs_FindGroundItem_ItemCentric);
    
    // PRIORITY 3: Re-haul items from overfull/low-priority stockpiles
    PROFILE_ACCUM_BEGIN(Jobs_FindRehaulItem);
    if (idleMoverCount > 0) {
        for (int j = 0; j < MAX_ITEMS && idleMoverCount > 0; j++) {
            if (!items[j].active) continue;
            if (items[j].reservedBy != -1) continue;
            if (items[j].state != ITEM_IN_STOCKPILE) continue;
            
            int currentSp = -1;
            if (!IsPositionInStockpile(items[j].x, items[j].y, (int)items[j].z, &currentSp)) continue;
            if (currentSp < 0) continue;
            
            int itemSlotX = (int)(items[j].x / CELL_SIZE);
            int itemSlotY = (int)(items[j].y / CELL_SIZE);
            
            int destSlotX, destSlotY;
            int destSp = -1;
            
            if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
            } else {
                destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
            }
            
            if (destSp < 0) continue;
            
            TryAssignItemToMover(j, destSp, destSlotX, destSlotY, false);
        }
    }
    PROFILE_ACCUM_END(Jobs_FindRehaulItem);
}

void JobsTick(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        
        if (m->jobState == JOB_MOVING_TO_ITEM) {
            int itemIdx = m->targetItem;
            
            // Check if item still exists
            if (itemIdx < 0 || !items[itemIdx].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stockpile still valid (unless it's a safe-drop job with targetStockpile == -1)
            if (m->targetStockpile >= 0 && !stockpiles[m->targetStockpile].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stuck (no path and not making progress) - item is unreachable
            if (m->pathLength == 0 && m->timeWithoutProgress > JOB_STUCK_TIME) {
                // Set cooldown on item so we don't immediately retry
                SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
                CancelJob(m, i);
                continue;
            }
            
            // Check if arrived at item
            Item* item = &items[itemIdx];
            float dx = m->x - item->x;
            float dy = m->y - item->y;
            float distSq = dx*dx + dy*dy;
            
            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                // Pick up the item (if re-hauling, clear the source slot first)
                if (item->state == ITEM_IN_STOCKPILE) {
                    ClearSourceStockpileSlot(item);
                }
                item->state = ITEM_CARRIED;
                m->carryingItem = itemIdx;
                m->targetItem = -1;
                
                if (m->targetStockpile < 0) {
                    // Safe-drop job: find a passable tile outside any stockpile
                    // For now, just drop at current position (will be refined)
                    // The item needs to be dropped outside the stockpile it was on
                    int moverTileX = (int)(m->x / CELL_SIZE);
                    int moverTileY = (int)(m->y / CELL_SIZE);
                    
                    // Search for a nearby passable tile not on any stockpile
                    bool foundDrop = false;
                    for (int radius = 1; radius <= 5 && !foundDrop; radius++) {
                        for (int dy2 = -radius; dy2 <= radius && !foundDrop; dy2++) {
                            for (int dx2 = -radius; dx2 <= radius && !foundDrop; dx2++) {
                                if (abs(dx2) != radius && abs(dy2) != radius) continue; // only check perimeter
                                int checkX = moverTileX + dx2;
                                int checkY = moverTileY + dy2;
                                if (checkX < 0 || checkY < 0) continue;
                                if (!IsCellWalkableAt((int)m->z, checkY, checkX)) continue;
                                int tempSpIdx;
                                if (IsPositionInStockpile(checkX * CELL_SIZE + CELL_SIZE * 0.5f, 
                                                          checkY * CELL_SIZE + CELL_SIZE * 0.5f, 
                                                          (int)m->z, &tempSpIdx)) continue;
                                // Found a valid drop spot
                                m->targetSlotX = checkX;
                                m->targetSlotY = checkY;
                                foundDrop = true;
                            }
                        }
                    }
                    
                    if (!foundDrop) {
                        // Couldn't find a spot, just drop at feet
                        m->targetSlotX = moverTileX;
                        m->targetSlotY = moverTileY;
                    }
                    
                    m->jobState = JOB_MOVING_TO_DROP;
                    m->goal.x = m->targetSlotX;
                    m->goal.y = m->targetSlotY;
                    m->goal.z = (int)m->z;
                    m->needsRepath = true;
                } else {
                    // Normal haul: go to stockpile
                    m->jobState = JOB_MOVING_TO_STOCKPILE;
                    m->goal.x = m->targetSlotX;
                    m->goal.y = m->targetSlotY;
                    m->goal.z = stockpiles[m->targetStockpile].z;
                    m->needsRepath = true;
                }
            }
        }
        else if (m->jobState == JOB_MOVING_TO_STOCKPILE) {
            int itemIdx = m->carryingItem;
            
            // Check if still carrying
            if (itemIdx < 0 || !items[itemIdx].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stockpile still valid
            if (!stockpiles[m->targetStockpile].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stockpile still accepts this item type
            if (!StockpileAcceptsType(m->targetStockpile, items[itemIdx].type)) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stuck (no path and not making progress)
            if (m->pathLength == 0 && m->timeWithoutProgress > JOB_STUCK_TIME) {
                CancelJob(m, i);
                continue;
            }
            
            // Update carried item position to follow mover
            items[itemIdx].x = m->x;
            items[itemIdx].y = m->y;
            items[itemIdx].z = m->z;
            
            // Check if arrived at target slot
            float targetX = m->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = m->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx = m->x - targetX;
            float dy = m->y - targetY;
            float distSq = dx*dx + dy*dy;
            
            if (distSq < DROP_RADIUS * DROP_RADIUS) {
                Item* item = &items[itemIdx];
                
                // Place in stockpile
                item->state = ITEM_IN_STOCKPILE;
                item->x = targetX;
                item->y = targetY;
                item->reservedBy = -1;
                
                // Mark slot as occupied
                PlaceItemInStockpile(m->targetStockpile, m->targetSlotX, m->targetSlotY, itemIdx);
                
                // Job complete
                m->jobState = JOB_IDLE;
                m->carryingItem = -1;
                m->targetStockpile = -1;
                m->targetSlotX = -1;
                m->targetSlotY = -1;
                
                // Add mover back to idle list
                AddMoverToIdleList(i);
            }
        }
        else if (m->jobState == JOB_MOVING_TO_DROP) {
            int itemIdx = m->carryingItem;
            
            // Check if still carrying
            if (itemIdx < 0 || !items[itemIdx].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stuck (no path and not making progress)
            if (m->pathLength == 0 && m->timeWithoutProgress > JOB_STUCK_TIME) {
                CancelJob(m, i);
                continue;
            }
            
            // Update carried item position to follow mover
            items[itemIdx].x = m->x;
            items[itemIdx].y = m->y;
            items[itemIdx].z = m->z;
            
            // Check if arrived at drop location
            float targetX = m->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = m->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx = m->x - targetX;
            float dy = m->y - targetY;
            float distSq = dx*dx + dy*dy;
            
            if (distSq < DROP_RADIUS * DROP_RADIUS) {
                Item* item = &items[itemIdx];
                
                // Drop on ground
                item->state = ITEM_ON_GROUND;
                item->x = targetX;
                item->y = targetY;
                item->reservedBy = -1;
                
                // Job complete
                m->jobState = JOB_IDLE;
                m->carryingItem = -1;
                m->targetStockpile = -1;
                m->targetSlotX = -1;
                m->targetSlotY = -1;
                
                // Add mover back to idle list
                AddMoverToIdleList(i);
            }
        }
    }
}
