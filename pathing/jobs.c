#include "jobs.h"
#include "items.h"
#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include "stockpiles.h"
#include "../shared/profiler.h"
#include <math.h>
#include <stdlib.h>

// Distance thresholds (relative to CELL_SIZE)
#define PICKUP_RADIUS (CELL_SIZE * 0.75f)  // Large enough to cover same-cell edge cases
#define DROP_RADIUS (CELL_SIZE * 0.75f)    // Same as pickup - covers whole cell reliably
#define JOB_STUCK_TIME 3.0f  // Cancel job if stuck for this long
#define UNREACHABLE_COOLDOWN 5.0f  // Seconds before retrying unreachable item

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
}

void AssignJobs(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if (m->jobState != JOB_IDLE) continue;
        
        int itemIdx = -1;
        float nearestDistSq = 1e30f;
        int stockpileForItem = -1;
        bool isAbsorbJob = false;
        bool isClearJob = false;
        
        // PRIORITY 1: Find ground items on stockpile tiles that need absorbing or clearing
        PROFILE_ACCUM_BEGIN(Jobs_FindStockpileItem);
        int spOnItem = -1;
        bool absorb = false;
        int stockpileItemIdx = FindGroundItemOnStockpile(&spOnItem, &absorb);
        PROFILE_ACCUM_END(Jobs_FindStockpileItem);
        
        if (stockpileItemIdx >= 0 && items[stockpileItemIdx].unreachableCooldown <= 0.0f) {
            if (absorb) {
                // Absorb job: item matches stockpile filter, just pick up and place in same tile
                isAbsorbJob = true;
                itemIdx = stockpileItemIdx;
                stockpileForItem = spOnItem;
            } else {
                // Clear job: item doesn't match, need to find another destination or safe-drop
                isClearJob = true;
                itemIdx = stockpileItemIdx;
                // stockpileForItem will be determined below (find destination for this item type)
            }
        }
        
        // PRIORITY 2: Find normal ground items to haul (only if no absorb/clear job)
        PROFILE_ACCUM_BEGIN(Jobs_FindGroundItem);
        if (itemIdx < 0) {
            for (int j = 0; j < MAX_ITEMS; j++) {
                if (!items[j].active) continue;
                if (items[j].reservedBy != -1) continue;
                if (items[j].state != ITEM_ON_GROUND) continue;  // skip carried/stored items
                if (items[j].unreachableCooldown > 0.0f) continue;  // skip items on cooldown
                if (!IsItemInGatherZone(items[j].x, items[j].y, (int)items[j].z)) continue;  // skip items outside gather zones
                
                // Check if there's a stockpile that accepts this item type
                int slotX, slotY;
                int spIdx = FindStockpileForItem(items[j].type, &slotX, &slotY);
                if (spIdx < 0) continue;  // no valid destination
                
                float dx = items[j].x - m->x;
                float dy = items[j].y - m->y;
                float distSq = dx*dx + dy*dy;
                
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    itemIdx = j;
                }
            }
        }
        PROFILE_ACCUM_END(Jobs_FindGroundItem);
        
        // If no ground item found, try to find an item to re-haul:
        // 1. Items in overfull slots (priority - must move these)
        // 2. Items in low-priority stockpiles (optional optimization)
        PROFILE_ACCUM_BEGIN(Jobs_FindRehaulItem);
        if (itemIdx < 0) {
            for (int j = 0; j < MAX_ITEMS; j++) {
                if (!items[j].active) continue;
                if (items[j].reservedBy != -1) continue;
                if (items[j].state != ITEM_IN_STOCKPILE) continue;
                
                // Find which stockpile this item is in
                int currentSp = -1;
                if (!IsPositionInStockpile(items[j].x, items[j].y, (int)items[j].z, &currentSp)) continue;
                if (currentSp < 0) continue;
                
                int itemSlotX = (int)(items[j].x / CELL_SIZE);
                int itemSlotY = (int)(items[j].y / CELL_SIZE);
                
                // Check if slot is overfull OR if there's a higher-priority stockpile
                int destSlotX, destSlotY;
                int destSp = -1;
                
                if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                    // Overfull: find any stockpile with room
                    destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
                } else {
                    // Not overfull: only move to higher priority
                    destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
                }
                
                if (destSp < 0) continue;  // no destination available
                
                float dx = items[j].x - m->x;
                float dy = items[j].y - m->y;
                float distSq = dx*dx + dy*dy;
                
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    itemIdx = j;
                }
            }
        }
        PROFILE_ACCUM_END(Jobs_FindRehaulItem);
        
        if (itemIdx < 0) continue;  // no item to haul or re-haul
        
        // Find stockpile for this item
        PROFILE_ACCUM_BEGIN(Jobs_FindDestination);
        int slotX, slotY;
        int spIdx;
        bool safeDrop = false;  // true if we should safe-drop (clear job with no destination)
        
        if (isAbsorbJob) {
            // Absorb job: destination is the same tile the item is already on
            spIdx = stockpileForItem;
            slotX = (int)(items[itemIdx].x / CELL_SIZE);
            slotY = (int)(items[itemIdx].y / CELL_SIZE);
        } else if (isClearJob) {
            // Clear job: find a stockpile that accepts this item type
            spIdx = FindStockpileForItem(items[itemIdx].type, &slotX, &slotY);
            if (spIdx < 0) {
                // No valid destination - we'll safe-drop outside the stockpile
                safeDrop = true;
                spIdx = -1;
            }
        } else if (items[itemIdx].state == ITEM_IN_STOCKPILE) {
            // Re-haul: check if overfull or moving to higher priority
            int currentSp = -1;
            IsPositionInStockpile(items[itemIdx].x, items[itemIdx].y, (int)items[itemIdx].z, &currentSp);
            int itemSlotX = (int)(items[itemIdx].x / CELL_SIZE);
            int itemSlotY = (int)(items[itemIdx].y / CELL_SIZE);
            
            if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                spIdx = FindStockpileForOverfullItem(itemIdx, currentSp, &slotX, &slotY);
            } else {
                spIdx = FindHigherPriorityStockpile(itemIdx, currentSp, &slotX, &slotY);
            }
        } else {
            // Normal haul: find any accepting stockpile
            spIdx = FindStockpileForItem(items[itemIdx].type, &slotX, &slotY);
        }
        PROFILE_ACCUM_END(Jobs_FindDestination);
        
        if (spIdx < 0 && !safeDrop) continue;  // no valid destination
        
        // Reserve item
        if (!ReserveItem(itemIdx, i)) continue;
        
        // Reserve stockpile slot (unless safeDrop - no destination)
        if (!safeDrop) {
            if (!ReserveStockpileSlot(spIdx, slotX, slotY, i)) {
                ReleaseItemReservation(itemIdx);
                continue;
            }
        }
        
        // Set goal to item position and check if reachable before assigning
        Item* item = &items[itemIdx];
        Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
        Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
        
        // Quick reachability check - try to find a path using current algorithm
        PROFILE_ACCUM_BEGIN(Jobs_ReachabilityCheck);
        Point tempPath[MAX_PATH];
        int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
        PROFILE_ACCUM_END(Jobs_ReachabilityCheck);
        
        if (tempLen == 0) {
            // Can't reach item - release reservations and set cooldown
            ReleaseItemReservation(itemIdx);
            if (!safeDrop) {
                ReleaseStockpileSlot(spIdx, slotX, slotY);
            }
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            continue;
        }
        
        // Set mover state
        m->targetItem = itemIdx;
        m->targetStockpile = spIdx;  // -1 for safeDrop
        m->targetSlotX = safeDrop ? -1 : slotX;
        m->targetSlotY = safeDrop ? -1 : slotY;
        m->jobState = JOB_MOVING_TO_ITEM;
        
        // Set goal to item position and trigger pathfinding
        m->goal = itemCell;
        m->needsRepath = true;
    }
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
                    
                    m->jobState = JOB_MOVING_TO_STOCKPILE;  // Reuse this state for safe-drop
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
            bool isSafeDrop = (m->targetStockpile < 0);
            
            // Check if still carrying
            if (itemIdx < 0 || !items[itemIdx].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stockpile still valid (skip for safe-drop jobs)
            if (!isSafeDrop && !stockpiles[m->targetStockpile].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stockpile still accepts this item type (skip for safe-drop jobs)
            if (!isSafeDrop && !StockpileAcceptsType(m->targetStockpile, items[itemIdx].type)) {
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
                
                if (isSafeDrop) {
                    // Safe-drop: just drop on ground
                    item->state = ITEM_ON_GROUND;
                    item->x = targetX;
                    item->y = targetY;
                    item->reservedBy = -1;
                } else {
                    // Normal drop: place in stockpile
                    item->state = ITEM_IN_STOCKPILE;
                    item->x = targetX;
                    item->y = targetY;
                    item->reservedBy = -1;
                    
                    // Mark slot as occupied
                    PlaceItemInStockpile(m->targetStockpile, m->targetSlotX, m->targetSlotY, itemIdx);
                }
                
                // Job complete
                m->jobState = JOB_IDLE;
                m->carryingItem = -1;
                m->targetStockpile = -1;
                m->targetSlotX = -1;
                m->targetSlotY = -1;
            }
        }
    }
}
