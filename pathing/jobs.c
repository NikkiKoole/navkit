#include "jobs.h"
#include "items.h"
#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include "stockpiles.h"
#include <math.h>

// Distance thresholds
#define PICKUP_RADIUS 16.0f
#define DROP_RADIUS 16.0f
#define JOB_STUCK_TIME 3.0f  // Cancel job if stuck for this long

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
        
        // Find nearest unreserved item that's on the ground (not already stored)
        int itemIdx = -1;
        float nearestDistSq = 1e30f;
        
        for (int j = 0; j < MAX_ITEMS; j++) {
            if (!items[j].active) continue;
            if (items[j].reservedBy != -1) continue;
            if (items[j].state != ITEM_ON_GROUND) continue;  // skip carried/stored items
            
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
        
        if (itemIdx < 0) continue;  // no item to haul
        
        // Find stockpile for this item
        int slotX, slotY;
        int spIdx = FindStockpileForItem(items[itemIdx].type, &slotX, &slotY);
        if (spIdx < 0) continue;  // shouldn't happen, but safety check
        
        // Reserve both item and stockpile slot
        if (!ReserveItem(itemIdx, i)) continue;
        if (!ReserveStockpileSlot(spIdx, slotX, slotY, i)) {
            ReleaseItemReservation(itemIdx);
            continue;
        }
        
        // Set mover state
        m->targetItem = itemIdx;
        m->targetStockpile = spIdx;
        m->targetSlotX = slotX;
        m->targetSlotY = slotY;
        m->jobState = JOB_MOVING_TO_ITEM;
        
        // Set goal to item position and trigger pathfinding
        Item* item = &items[itemIdx];
        m->goal.x = (int)(item->x / CELL_SIZE);
        m->goal.y = (int)(item->y / CELL_SIZE);
        m->goal.z = (int)item->z;
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
            
            // Check if stockpile still valid
            if (m->targetStockpile < 0 || !stockpiles[m->targetStockpile].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stuck (no path and not making progress)
            if (m->pathLength == 0 && m->timeWithoutProgress > JOB_STUCK_TIME) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if arrived at item
            Item* item = &items[itemIdx];
            float dx = m->x - item->x;
            float dy = m->y - item->y;
            float distSq = dx*dx + dy*dy;
            
            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                // Pick up the item
                item->state = ITEM_CARRIED;
                m->carryingItem = itemIdx;
                m->targetItem = -1;
                
                // Now go to stockpile
                m->jobState = JOB_MOVING_TO_STOCKPILE;
                m->goal.x = m->targetSlotX;
                m->goal.y = m->targetSlotY;
                m->goal.z = stockpiles[m->targetStockpile].z;
                m->needsRepath = true;
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
            if (m->targetStockpile < 0 || !stockpiles[m->targetStockpile].active) {
                CancelJob(m, i);
                continue;
            }
            
            // Check if stockpile still accepts this item type (filter may have changed)
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
            
            // Check if arrived at stockpile slot
            float targetX = m->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = m->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx = m->x - targetX;
            float dy = m->y - targetY;
            float distSq = dx*dx + dy*dy;
            
            if (distSq < DROP_RADIUS * DROP_RADIUS) {
                // Drop the item in the stockpile
                Item* item = &items[itemIdx];
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
            }
        }
    }
}
