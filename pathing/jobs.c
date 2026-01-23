#include "jobs.h"
#include "items.h"
#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include <math.h>

// Distance threshold for "arrived at item"
#define PICKUP_RADIUS 16.0f

void AssignJobs(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if (m->jobState != JOB_IDLE) continue;
        
        // Find nearest unreserved item
        int itemIdx = FindNearestUnreservedItem(m->x, m->y, m->z);
        if (itemIdx < 0) continue;
        
        // Reserve it
        if (!ReserveItem(itemIdx, i)) continue;
        
        // Set mover state
        m->targetItem = itemIdx;
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
                // Item gone - cancel job
                m->jobState = JOB_IDLE;
                m->targetItem = -1;
                continue;
            }
            
            // Check if arrived at item
            Item* item = &items[itemIdx];
            float dx = m->x - item->x;
            float dy = m->y - item->y;
            float distSq = dx*dx + dy*dy;
            
            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                // Arrived - pick up (delete item)
                DeleteItem(itemIdx);
                m->targetItem = -1;
                m->jobState = JOB_IDLE;
            }
        }
    }
}
