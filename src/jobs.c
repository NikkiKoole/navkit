#include "jobs.h"
#include "items.h"
#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include "stockpiles.h"
#include "designations.h"
#include "../shared/profiler.h"
#include "../shared/ui.h"
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
// Job Pool System
// =============================================================================

Job jobs[MAX_JOBS];
int jobHighWaterMark = 0;

int* jobFreeList = NULL;
int jobFreeCount = 0;

int* activeJobList = NULL;
int activeJobCount = 0;
bool* jobIsActive = NULL;

static bool jobPoolInitialized = false;

void InitJobPool(void) {
    if (jobPoolInitialized) return;
    
    jobFreeList = (int*)malloc(MAX_JOBS * sizeof(int));
    activeJobList = (int*)malloc(MAX_JOBS * sizeof(int));
    jobIsActive = (bool*)calloc(MAX_JOBS, sizeof(bool));
    
    jobHighWaterMark = 0;
    jobFreeCount = 0;
    activeJobCount = 0;
    
    // Clear all jobs
    memset(jobs, 0, sizeof(jobs));
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].active = false;
        jobs[i].assignedMover = -1;
        jobs[i].targetItem = -1;
        jobs[i].targetStockpile = -1;
        jobs[i].targetSlotX = -1;
        jobs[i].targetSlotY = -1;
        jobs[i].targetDigX = -1;
        jobs[i].targetDigY = -1;
        jobs[i].targetDigZ = -1;
        jobs[i].targetBlueprint = -1;
        jobs[i].carryingItem = -1;
    }
    
    jobPoolInitialized = true;
}

void FreeJobPool(void) {
    free(jobFreeList);
    free(activeJobList);
    free(jobIsActive);
    jobFreeList = NULL;
    activeJobList = NULL;
    jobIsActive = NULL;
    jobFreeCount = 0;
    activeJobCount = 0;
    jobHighWaterMark = 0;
    jobPoolInitialized = false;
}

void ClearJobs(void) {
    // Initialize pool if needed
    if (!jobPoolInitialized) {
        InitJobPool();
    }
    
    // Reset all jobs
    for (int i = 0; i < jobHighWaterMark; i++) {
        jobs[i].active = false;
        jobs[i].type = JOBTYPE_NONE;
        jobs[i].assignedMover = -1;
        jobs[i].step = 0;
        jobs[i].targetItem = -1;
        jobs[i].targetStockpile = -1;
        jobs[i].targetSlotX = -1;
        jobs[i].targetSlotY = -1;
        jobs[i].targetDigX = -1;
        jobs[i].targetDigY = -1;
        jobs[i].targetDigZ = -1;
        jobs[i].targetBlueprint = -1;
        jobs[i].progress = 0.0f;
        jobs[i].carryingItem = -1;
    }
    
    // Reset tracking
    jobHighWaterMark = 0;
    jobFreeCount = 0;
    activeJobCount = 0;
    memset(jobIsActive, 0, MAX_JOBS * sizeof(bool));
}

int CreateJob(JobType type) {
    // Initialize pool if needed
    if (!jobPoolInitialized) {
        InitJobPool();
    }
    
    int jobId;
    
    // Try to reuse from free list first (O(1))
    if (jobFreeCount > 0) {
        jobId = jobFreeList[--jobFreeCount];
    }
    // Otherwise allocate new slot
    else if (jobHighWaterMark < MAX_JOBS) {
        jobId = jobHighWaterMark++;
    }
    // Pool is full
    else {
        return -1;
    }
    
    // Initialize job
    Job* job = &jobs[jobId];
    job->active = true;
    job->type = type;
    job->assignedMover = -1;
    job->step = 0;
    job->targetItem = -1;
    job->targetStockpile = -1;
    job->targetSlotX = -1;
    job->targetSlotY = -1;
    job->targetDigX = -1;
    job->targetDigY = -1;
    job->targetDigZ = -1;
    job->targetBlueprint = -1;
    job->progress = 0.0f;
    job->carryingItem = -1;
    
    // Add to active list
    activeJobList[activeJobCount++] = jobId;
    jobIsActive[jobId] = true;
    
    return jobId;
}

void ReleaseJob(int jobId) {
    if (jobId < 0 || jobId >= MAX_JOBS) return;
    if (!jobs[jobId].active) return;
    
    // Mark as inactive
    jobs[jobId].active = false;
    jobs[jobId].type = JOBTYPE_NONE;
    
    // Remove from active list (swap with last, O(1))
    if (jobIsActive[jobId]) {
        for (int i = 0; i < activeJobCount; i++) {
            if (activeJobList[i] == jobId) {
                activeJobList[i] = activeJobList[activeJobCount - 1];
                activeJobCount--;
                break;
            }
        }
        jobIsActive[jobId] = false;
    }
    
    // Add to free list for reuse
    jobFreeList[jobFreeCount++] = jobId;
}

Job* GetJob(int jobId) {
    if (jobId < 0 || jobId >= MAX_JOBS) return NULL;
    return &jobs[jobId];
}

// =============================================================================
// Job Drivers (Phase 2)
// =============================================================================

// Forward declaration for helper function used by drivers
static void ClearSourceStockpileSlot(Item* item);

// Haul job driver: pick up item -> carry to stockpile -> drop
JobRunResult RunJob_Haul(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    (void)dt;
    
    if (job->step == STEP_MOVING_TO_PICKUP) {
        int itemIdx = job->targetItem;
        
        // Check if item still exists
        if (itemIdx < 0 || !items[itemIdx].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if stockpile still valid
        if (job->targetStockpile >= 0 && !stockpiles[job->targetStockpile].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if item's cell became a wall
        Item* item = &items[itemIdx];
        int itemCellX = (int)(item->x / CELL_SIZE);
        int itemCellY = (int)(item->y / CELL_SIZE);
        int itemCellZ = (int)item->z;
        if (!IsCellWalkableAt(itemCellZ, itemCellY, itemCellX)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }
        
        // Check if arrived at item
        float dx = mover->x - item->x;
        float dy = mover->y - item->y;
        float distSq = dx*dx + dy*dy;
        
        // Set goal to item if not already moving there
        if (mover->pathLength == 0 && distSq >= PICKUP_RADIUS * PICKUP_RADIUS) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }
        
        // Final approach - when path exhausted but not in pickup range, move directly toward item
        // This handles the case where knot-fix skips to waypoint without snapping position
        int moverCellX = (int)(mover->x / CELL_SIZE);
        int moverCellY = (int)(mover->y / CELL_SIZE);
        bool inSameOrAdjacentCell = (abs(moverCellX - itemCellX) <= 1 && abs(moverCellY - itemCellY) <= 1);
        if (mover->pathLength == 0 && distSq >= PICKUP_RADIUS * PICKUP_RADIUS && inSameOrAdjacentCell) {
            // Move directly toward item (micro-adjustment, not pathfinding)
            float dist = sqrtf(distSq);
            float moveSpeed = mover->speed * TICK_DT;
            if (dist > 0.01f) {
                mover->x -= (dx / dist) * moveSpeed;
                mover->y -= (dy / dist) * moveSpeed;
            }
        }
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }
        
        if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            // Pick up the item
            if (item->state == ITEM_IN_STOCKPILE) {
                // Clear source stockpile slot when re-hauling
                int sourceSp = -1;
                if (IsPositionInStockpile(item->x, item->y, (int)item->z, &sourceSp) && sourceSp >= 0) {
                    int lx = (int)(item->x / CELL_SIZE) - stockpiles[sourceSp].x;
                    int ly = (int)(item->y / CELL_SIZE) - stockpiles[sourceSp].y;
                    if (lx >= 0 && lx < stockpiles[sourceSp].width && ly >= 0 && ly < stockpiles[sourceSp].height) {
                        int idx = ly * stockpiles[sourceSp].width + lx;
                        stockpiles[sourceSp].slotCounts[idx]--;
                        if (stockpiles[sourceSp].slotCounts[idx] <= 0) {
                            stockpiles[sourceSp].slots[idx] = -1;
                            stockpiles[sourceSp].slotTypes[idx] = -1;
                            stockpiles[sourceSp].slotCounts[idx] = 0;
                        }
                    }
                }
            }
            
            item->state = ITEM_CARRIED;
            job->carryingItem = itemIdx;
            job->targetItem = -1;
            job->step = STEP_CARRYING;
            
            // Set goal to stockpile slot
            mover->goal.x = job->targetSlotX;
            mover->goal.y = job->targetSlotY;
            mover->goal.z = stockpiles[job->targetStockpile].z;
            mover->needsRepath = true;
        }
        
        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_CARRYING) {
        int itemIdx = job->carryingItem;
        
        // Check if still carrying
        if (itemIdx < 0 || !items[itemIdx].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if stockpile still valid
        if (!stockpiles[job->targetStockpile].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if stockpile still accepts this item type
        if (!StockpileAcceptsType(job->targetStockpile, items[itemIdx].type)) {
            return JOBRUN_FAIL;
        }
        
        // Check if arrived at target slot
        float targetX = job->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
        float targetY = job->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - targetX;
        float dy = mover->y - targetY;
        float distSq = dx*dx + dy*dy;
        
        // Request repath if no path and not at destination
        if (mover->pathLength == 0 && distSq >= DROP_RADIUS * DROP_RADIUS) {
            mover->goal.x = job->targetSlotX;
            mover->goal.y = job->targetSlotY;
            mover->goal.z = stockpiles[job->targetStockpile].z;
            mover->needsRepath = true;
        }
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }
        
        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;
        
        if (distSq < DROP_RADIUS * DROP_RADIUS) {
            Item* item = &items[itemIdx];
            
            // Place in stockpile
            item->state = ITEM_IN_STOCKPILE;
            item->x = targetX;
            item->y = targetY;
            item->reservedBy = -1;
            
            // Mark slot as occupied
            PlaceItemInStockpile(job->targetStockpile, job->targetSlotX, job->targetSlotY, itemIdx);
            
            job->carryingItem = -1;
            return JOBRUN_DONE;
        }
        
        return JOBRUN_RUNNING;
    }
    
    return JOBRUN_FAIL;
}

// Clear job driver: pick up item -> carry to safe drop location outside stockpile
JobRunResult RunJob_Clear(Job* job, void* moverPtr, float dt) {
    (void)dt;
    Mover* mover = (Mover*)moverPtr;
    
    if (job->step == STEP_MOVING_TO_PICKUP) {
        int itemIdx = job->targetItem;
        
        // Check if item still exists
        if (itemIdx < 0 || !items[itemIdx].active) {
            return JOBRUN_FAIL;
        }
        
        Item* item = &items[itemIdx];
        int itemCellX = (int)(item->x / CELL_SIZE);
        int itemCellY = (int)(item->y / CELL_SIZE);
        int itemCellZ = (int)(item->z);
        
        // Check if item's cell became a wall
        if (!IsCellWalkableAt(itemCellZ, itemCellY, itemCellX)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }
        
        float dx = mover->x - item->x;
        float dy = mover->y - item->y;
        float distSq = dx*dx + dy*dy;
        
        // Final approach
        if (mover->pathLength == 0 && distSq < CELL_SIZE * CELL_SIZE * 4) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }
        
        if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            // Pick up the item
            if (item->state == ITEM_IN_STOCKPILE) {
                ClearSourceStockpileSlot(item);
            }
            item->state = ITEM_CARRIED;
            job->carryingItem = itemIdx;
            job->targetItem = -1;
            job->step = STEP_CARRYING;
            
            // Find drop location outside stockpile
            int moverTileX = (int)(mover->x / CELL_SIZE);
            int moverTileY = (int)(mover->y / CELL_SIZE);
            
            bool foundDrop = false;
            for (int radius = 1; radius <= 5 && !foundDrop; radius++) {
                for (int dy2 = -radius; dy2 <= radius && !foundDrop; dy2++) {
                    for (int dx2 = -radius; dx2 <= radius && !foundDrop; dx2++) {
                        if (abs(dx2) != radius && abs(dy2) != radius) continue;
                        int checkX = moverTileX + dx2;
                        int checkY = moverTileY + dy2;
                        if (checkX < 0 || checkY < 0) continue;
                        if (checkX >= gridWidth || checkY >= gridHeight) continue;
                        if (!IsCellWalkableAt((int)mover->z, checkY, checkX)) continue;
                        
                        int tempSpIdx;
                        if (IsPositionInStockpile(checkX * CELL_SIZE + CELL_SIZE * 0.5f, 
                                                  checkY * CELL_SIZE + CELL_SIZE * 0.5f, 
                                                  (int)mover->z, &tempSpIdx)) continue;
                        job->targetSlotX = checkX;
                        job->targetSlotY = checkY;
                        foundDrop = true;
                    }
                }
            }
            
            if (!foundDrop) {
                job->targetSlotX = moverTileX;
                job->targetSlotY = moverTileY;
            }
            
            mover->goal.x = job->targetSlotX;
            mover->goal.y = job->targetSlotY;
            mover->goal.z = (int)mover->z;
            mover->needsRepath = true;
        }
        
        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_CARRYING) {
        int itemIdx = job->carryingItem;
        
        // Check if still carrying
        if (itemIdx < 0 || !items[itemIdx].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }
        
        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;
        
        // Check if arrived at drop location
        float targetX = job->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
        float targetY = job->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - targetX;
        float dy = mover->y - targetY;
        float distSq = dx*dx + dy*dy;
        
        if (distSq < DROP_RADIUS * DROP_RADIUS) {
            Item* item = &items[itemIdx];
            
            // Drop on ground
            item->state = ITEM_ON_GROUND;
            item->x = targetX;
            item->y = targetY;
            item->reservedBy = -1;
            
            job->carryingItem = -1;
            return JOBRUN_DONE;
        }
        
        return JOBRUN_RUNNING;
    }
    
    return JOBRUN_FAIL;
}

// Dig job driver: move to adjacent tile -> dig wall
JobRunResult RunJob_Dig(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    (void)job->assignedMover;  // Currently unused but available if needed
    
    // Check if designation still exists
    Designation* d = GetDesignation(job->targetDigX, job->targetDigY, job->targetDigZ);
    if (!d || d->type != DESIGNATION_DIG) {
        return JOBRUN_FAIL;
    }
    
    // Check if the wall was already dug
    if (grid[job->targetDigZ][job->targetDigY][job->targetDigX] != CELL_WALL) {
        CancelDesignation(job->targetDigX, job->targetDigY, job->targetDigZ);
        return JOBRUN_FAIL;
    }
    
    if (job->step == STEP_MOVING_TO_WORK) {
        // Find adjacent walkable tile 
        int adjX = -1, adjY = -1;
        int dx4[] = {0, 1, 0, -1};
        int dy4[] = {-1, 0, 1, 0};
        for (int dir = 0; dir < 4; dir++) {
            int ax = job->targetDigX + dx4[dir];
            int ay = job->targetDigY + dy4[dir];
            if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
                if (IsCellWalkableAt(job->targetDigZ, ay, ax)) {
                    adjX = ax;
                    adjY = ay;
                    break;
                }
            }
        }
        
        if (adjX < 0) return JOBRUN_FAIL;  // No adjacent walkable tile
        
        // Set goal if not already moving there
        if (mover->goal.x != adjX || mover->goal.y != adjY || mover->goal.z != job->targetDigZ) {
            mover->goal = (Point){adjX, adjY, job->targetDigZ};
            mover->needsRepath = true;
        }
        
        // Check if arrived at adjacent tile
        float goalX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;
        
        bool correctZ = (int)mover->z == job->targetDigZ;
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            Designation* desig = GetDesignation(job->targetDigX, job->targetDigY, job->targetDigZ);
            if (desig) desig->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }
        
        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }
        
        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress digging
        job->progress += dt / DIG_WORK_TIME;
        d->progress = job->progress;
        
        if (job->progress >= 1.0f) {
            // Dig complete!
            CompleteDigDesignation(job->targetDigX, job->targetDigY, job->targetDigZ);
            return JOBRUN_DONE;
        }
        
        return JOBRUN_RUNNING;
    }
    
    return JOBRUN_FAIL;
}

// Haul to blueprint job driver: pick up item -> carry to blueprint for construction
JobRunResult RunJob_HaulToBlueprint(Job* job, void* moverPtr, float dt) {
    (void)dt;
    Mover* mover = (Mover*)moverPtr;
    
    if (job->step == STEP_MOVING_TO_PICKUP) {
        int itemIdx = job->targetItem;
        
        // Check if item still exists
        if (itemIdx < 0 || !items[itemIdx].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if blueprint still exists
        int bpIdx = job->targetBlueprint;
        if (bpIdx < 0 || !blueprints[bpIdx].active) {
            return JOBRUN_FAIL;
        }
        
        Item* item = &items[itemIdx];
        int itemCellX = (int)(item->x / CELL_SIZE);
        int itemCellY = (int)(item->y / CELL_SIZE);
        int itemCellZ = (int)(item->z);
        
        // Check if item's cell became a wall
        if (!IsCellWalkableAt(itemCellZ, itemCellY, itemCellX)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }
        
        float dx = mover->x - item->x;
        float dy = mover->y - item->y;
        float distSq = dx*dx + dy*dy;
        
        // Final approach
        if (mover->pathLength == 0 && distSq < CELL_SIZE * CELL_SIZE * 4) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }
        
        if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            // Pick up the item
            if (item->state == ITEM_IN_STOCKPILE) {
                ClearSourceStockpileSlot(item);
            }
            item->state = ITEM_CARRIED;
            job->carryingItem = itemIdx;
            job->targetItem = -1;
            job->step = STEP_CARRYING;
            
            // Set goal to blueprint
            Blueprint* bp = &blueprints[bpIdx];
            mover->goal.x = bp->x;
            mover->goal.y = bp->y;
            mover->goal.z = bp->z;
            mover->needsRepath = true;
        }
        
        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_CARRYING) {
        int itemIdx = job->carryingItem;
        int bpIdx = job->targetBlueprint;
        
        // Check if still carrying
        if (itemIdx < 0 || !items[itemIdx].active) {
            return JOBRUN_FAIL;
        }
        
        // Check if blueprint still exists
        if (bpIdx < 0 || !blueprints[bpIdx].active) {
            // Blueprint cancelled - drop item on ground
            items[itemIdx].state = ITEM_ON_GROUND;
            items[itemIdx].x = mover->x;
            items[itemIdx].y = mover->y;
            items[itemIdx].z = mover->z;
            items[itemIdx].reservedBy = -1;
            job->carryingItem = -1;
            return JOBRUN_DONE;  // Job is "done" in the sense that we handled it gracefully
        }
        
        Blueprint* bp = &blueprints[bpIdx];
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }
        
        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;
        
        // Check if arrived at blueprint
        float targetX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float targetY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - targetX;
        float dy = mover->y - targetY;
        float distSq = dx*dx + dy*dy;
        
        if (distSq < DROP_RADIUS * DROP_RADIUS) {
            // Deliver material to blueprint
            DeliverMaterialToBlueprint(bpIdx, itemIdx);
            job->carryingItem = -1;
            return JOBRUN_DONE;
        }
        
        return JOBRUN_RUNNING;
    }
    
    return JOBRUN_FAIL;
}

// Build job driver: move to blueprint -> construct
JobRunResult RunJob_Build(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = job->assignedMover;
    int bpIdx = job->targetBlueprint;
    
    // Check if blueprint still exists
    if (bpIdx < 0 || !blueprints[bpIdx].active) {
        return JOBRUN_FAIL;
    }
    
    Blueprint* bp = &blueprints[bpIdx];
    
    if (job->step == STEP_MOVING_TO_WORK) {
        // Set goal to blueprint if not already moving there
        if (mover->pathLength == 0) {
            mover->goal = (Point){bp->x, bp->y, bp->z};
            mover->needsRepath = true;
        }
        
        // Check if arrived at blueprint
        float targetX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float targetY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - targetX;
        float dy = mover->y - targetY;
        float distSq = dx*dx + dy*dy;
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }
        
        if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
            job->progress = 0.0f;
        }
        
        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Check if blueprint was cancelled
        if (bp->assignedBuilder != moverIdx) {
            return JOBRUN_FAIL;
        }
        
        // Progress building
        job->progress += dt;
        bp->progress = job->progress / BUILD_WORK_TIME;
        
        if (job->progress >= BUILD_WORK_TIME) {
            // Build complete!
            CompleteBlueprint(bpIdx);
            return JOBRUN_DONE;
        }
        
        return JOBRUN_RUNNING;
    }
    
    return JOBRUN_FAIL;
}

// Driver lookup table
static JobDriver jobDrivers[] = {
    [JOBTYPE_NONE] = NULL,
    [JOBTYPE_HAUL] = RunJob_Haul,
    [JOBTYPE_CLEAR] = RunJob_Clear,
    [JOBTYPE_DIG] = RunJob_Dig,
    [JOBTYPE_HAUL_TO_BLUEPRINT] = RunJob_HaulToBlueprint,
    [JOBTYPE_BUILD] = RunJob_Build,
};

// Forward declaration for CancelJob (defined later in file)
static void CancelJob(Mover* m, int moverIdx);

// Tick function - runs job drivers for active jobs
void JobsTick(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if (m->currentJobId < 0) continue;
        
        Job* job = GetJob(m->currentJobId);
        if (!job || !job->active) {
            m->currentJobId = -1;
            AddMoverToIdleList(i);
            continue;
        }
        
        JobDriver driver = jobDrivers[job->type];
        if (!driver) {
            // No driver for this job type - cancel and release
            CancelJob(m, i);
            continue;
        }
        
        JobRunResult result = driver(job, m, TICK_DT);
        
        if (result == JOBRUN_DONE) {
            // Job completed successfully - release job and return mover to idle
            ReleaseJob(m->currentJobId);
            m->currentJobId = -1;
            AddMoverToIdleList(i);
        }
        else if (result == JOBRUN_FAIL) {
            // Job failed - cancel releases all reservations
            CancelJob(m, i);
        }
        // JOBRUN_RUNNING - continue next tick
    }
}

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
        if (movers[i].active && movers[i].currentJobId < 0) {
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
    bool requireCanHaul;
    bool requireCanMine;
    bool requireCanBuild;
} IdleMoverSearchContext;

static void IdleMoverSearchCallback(int moverIdx, float distSq, void* userData) {
    IdleMoverSearchContext* ctx = (IdleMoverSearchContext*)userData;
    
    // Skip if not in idle list (already has a job or not active)
    if (!moverIsInIdleList || !moverIsInIdleList[moverIdx]) return;
    
    // Check capabilities
    Mover* m = &movers[moverIdx];
    if (ctx->requireCanHaul && !m->capabilities.canHaul) return;
    if (ctx->requireCanMine && !m->capabilities.canMine) return;
    if (ctx->requireCanBuild && !m->capabilities.canBuild) return;
    
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
    // Get job data (all job-specific data now comes from Job struct)
    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;
    
    if (job) {
        // Release item reservation
        if (job->targetItem >= 0) {
            ReleaseItemReservation(job->targetItem);
        }
        
        // Release stockpile slot reservation
        if (job->targetStockpile >= 0) {
            ReleaseStockpileSlot(job->targetStockpile, job->targetSlotX, job->targetSlotY);
        }
        
        // If carrying, safe-drop the item
        if (job->carryingItem >= 0 && items[job->carryingItem].active) {
            Item* item = &items[job->carryingItem];
            item->state = ITEM_ON_GROUND;
            item->x = m->x;
            item->y = m->y;
            item->z = m->z;
            item->reservedBy = -1;
        }
        
        // Release dig designation reservation
        if (job->targetDigX >= 0 && job->targetDigY >= 0 && job->targetDigZ >= 0) {
            Designation* d = GetDesignation(job->targetDigX, job->targetDigY, job->targetDigZ);
            if (d && d->assignedMover == moverIdx) {
                d->assignedMover = -1;
                d->progress = 0.0f;  // Reset progress when cancelled
            }
        }
        
        // Release blueprint reservation
        if (job->targetBlueprint >= 0 && job->targetBlueprint < MAX_BLUEPRINTS) {
            Blueprint* bp = &blueprints[job->targetBlueprint];
            if (bp->active) {
                // If we were hauling to this blueprint, release the reserved item
                if (bp->reservedItem >= 0 && items[bp->reservedItem].reservedBy == moverIdx) {
                    bp->reservedItem = -1;
                }
                // If we were building, release the builder assignment
                if (bp->assignedBuilder == moverIdx) {
                    bp->assignedBuilder = -1;
                    bp->state = BLUEPRINT_READY_TO_BUILD;  // Revert to ready state
                    bp->progress = 0.0f;  // Reset progress
                }
            }
        }
        
        // Release Job entry
        ReleaseJob(m->currentJobId);
    }
    
    // Reset mover state (only currentJobId remains - legacy fields removed)
    m->currentJobId = -1;
    
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
            .bestDistSq = 1e30f,
            .requireCanHaul = true,
            .requireCanMine = false,
            .requireCanBuild = false
        };
        
        QueryMoverNeighbors(item->x, item->y, MOVER_SEARCH_RADIUS, -1, IdleMoverSearchCallback, &ctx);
        moverIdx = ctx.bestMoverIdx;
    } else {
        // Fallback: find first idle mover (for tests without spatial grid)
        float bestDistSq = 1e30f;
        for (int i = 0; i < idleMoverCount; i++) {
            int idx = idleMoverList[i];
            // Check capability
            if (!movers[idx].capabilities.canHaul) continue;
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
    
    // Create Job entry
    int jobId = CreateJob(safeDrop ? JOBTYPE_CLEAR : JOBTYPE_HAUL);
    if (jobId >= 0) {
        Job* job = GetJob(jobId);
        job->assignedMover = moverIdx;
        job->targetItem = itemIdx;
        job->targetStockpile = spIdx;
        job->targetSlotX = safeDrop ? -1 : slotX;
        job->targetSlotY = safeDrop ? -1 : slotY;
        job->step = 0;  // STEP_MOVING_TO_PICKUP
        m->currentJobId = jobId;
    }
    
    // Set mover goal
    m->goal = itemCell;
    m->needsRepath = true;
    
    // Remove mover from idle list
    RemoveMoverFromIdleList(moverIdx);
    
    return true;
}

// =============================================================================
// AssignJobsWorkGivers - WorkGiver-based job assignment (slower, for comparison)
// =============================================================================
// 
// This is a mover-centric approach: for each idle mover, try WorkGivers in priority
// order. Currently slower than AssignJobsLegacy due to:
// - Each WorkGiver rebuilds caches (typeHasStockpile)
// - Mover-centric iteration is O(movers × items) vs item-centric O(items)
// - No early exit when all movers assigned
//
// Future optimization: share caches between WorkGivers, use item-centric iteration
// for hauling while keeping mover-centric for sparse jobs (mining, building).

void AssignJobsWorkGivers(void) {
    // Initialize job system if needed
    if (!moverIsInIdleList) {
        InitJobSystem(MAX_MOVERS);
    }
    
    // Rebuild idle list each frame
    RebuildIdleMoverList();
    
    // Early exit: no idle movers means no work to do
    if (idleMoverCount == 0) return;
    
    // Rebuild caches once per frame (same as legacy)
    RebuildStockpileGroundItemCache();
    RebuildStockpileFreeSlotCounts();
    
    // Copy idle mover list since WorkGivers modify it via RemoveMoverFromIdleList
    int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
    int idleCopyCount = idleMoverCount;
    memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));
    
    // Process each idle mover
    for (int i = 0; i < idleCopyCount; i++) {
        int moverIdx = idleCopy[i];
        
        // Skip if mover was already assigned by a previous WorkGiver
        if (!moverIsInIdleList[moverIdx]) continue;
        
        // Try WorkGivers in priority order (matches AssignJobsLegacy priority)
        // Priority 1: Stockpile maintenance (absorb/clear) - highest
        int jobId = WorkGiver_StockpileMaintenance(moverIdx);
        
        // Priority 2: Haul (ground items to stockpiles)
        if (jobId < 0) jobId = WorkGiver_Haul(moverIdx);
        
        // Priority 3: Rehaul (overfull/low-priority transfers)
        if (jobId < 0) jobId = WorkGiver_Rehaul(moverIdx);
        
        // Priority 4: Mining (dig designations)
        if (jobId < 0) jobId = WorkGiver_Mining(moverIdx);
        
        // Priority 5: Blueprint haul (materials to blueprints)
        if (jobId < 0) jobId = WorkGiver_BlueprintHaul(moverIdx);
        
        // Priority 6: Build (construct at blueprints) - lowest
        if (jobId < 0) jobId = WorkGiver_Build(moverIdx);
        
        // jobId >= 0 means job was assigned, mover removed from idle list
        (void)jobId;  // Suppress unused warning
    }
    
    free(idleCopy);
}

// =============================================================================
// AssignJobsHybrid - Best of both: item-centric for hauling, mover-centric for sparse
// =============================================================================
// 
// The key insight: hauling dominates job count (hundreds of items) while
// mining/building targets are sparse (tens of designations/blueprints).
// 
// - Item-centric for hauling: O(items) - iterate items, find nearest mover
// - Mover-centric for sparse: O(movers × targets) - but targets << items
//
// This achieves similar performance to Legacy while using WorkGiver functions.

void AssignJobsHybrid(void) {
    // Initialize job system if needed
    if (!moverIsInIdleList) {
        InitJobSystem(MAX_MOVERS);
    }
    
    // Rebuild idle list each frame
    RebuildIdleMoverList();
    
    // Early exit: no idle movers means no work to do
    if (idleMoverCount == 0) return;
    
    // Rebuild caches once per frame (same as legacy)
    RebuildStockpileGroundItemCache();
    RebuildStockpileFreeSlotCounts();
    
    // Cache which item types have available stockpiles (shared across all hauling)
    bool typeHasStockpile[ITEM_TYPE_COUNT] = {false};
    bool anyTypeHasSlot = false;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        int slotX, slotY;
        if (FindStockpileForItem((ItemType)t, &slotX, &slotY) >= 0) {
            typeHasStockpile[t] = true;
            anyTypeHasSlot = true;
        }
    }
    
    // =========================================================================
    // PRIORITY 1: Stockpile maintenance (absorb/clear) - ITEM-CENTRIC
    // Items on stockpile tiles must be handled first
    // =========================================================================
    while (idleMoverCount > 0) {
        int spOnItem = -1;
        bool absorb = false;
        int itemIdx = FindGroundItemOnStockpile(&spOnItem, &absorb);
        
        if (itemIdx < 0 || items[itemIdx].unreachableCooldown > 0.0f) break;
        
        int slotX, slotY, spIdx;
        bool safeDrop = false;
        
        if (absorb) {
            spIdx = spOnItem;
            slotX = (int)(items[itemIdx].x / CELL_SIZE);
            slotY = (int)(items[itemIdx].y / CELL_SIZE);
        } else {
            spIdx = FindStockpileForItem(items[itemIdx].type, &slotX, &slotY);
            if (spIdx < 0) safeDrop = true;
        }
        
        if (!TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, safeDrop)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        }
    }
    
    // =========================================================================
    // PRIORITY 2a: Stockpile-centric hauling - search items near each stockpile
    // This is more efficient because items are assigned to nearby stockpiles
    // =========================================================================
    if (idleMoverCount > 0 && anyTypeHasSlot && itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        for (int spIdx = 0; spIdx < MAX_STOCKPILES && idleMoverCount > 0; spIdx++) {
            Stockpile* sp = &stockpiles[spIdx];
            if (!sp->active) continue;
            
            // Check each item type this stockpile accepts
            for (int t = 0; t < ITEM_TYPE_COUNT && idleMoverCount > 0; t++) {
                if (!sp->allowedTypes[t]) continue;
                if (!typeHasStockpile[t]) continue;
                
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
                    
                    int minTx = centerTileX - radius;
                    int maxTx = centerTileX + radius;
                    int minTy = centerTileY - radius;
                    int maxTy = centerTileY + radius;
                    
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
                                
                                if (TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, false)) {
                                    foundItem = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (foundItem) break;
                }
            }
        }
    }
    
    // =========================================================================
    // PRIORITY 2b: Item-centric fallback - for items not near any stockpile
    // =========================================================================
    if (idleMoverCount > 0 && anyTypeHasSlot) {
        if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
            int totalIndexed = itemGrid.cellStarts[itemGrid.cellCount];
            
            for (int t = 0; t < totalIndexed && idleMoverCount > 0; t++) {
                int itemIdx = itemGrid.itemIndices[t];
                Item* item = &items[itemIdx];
                
                if (!item->active) continue;
                if (item->reservedBy != -1) continue;
                if (item->state != ITEM_ON_GROUND) continue;
                if (item->unreachableCooldown > 0.0f) continue;
                if (!typeHasStockpile[item->type]) continue;
                if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) continue;
                
                int cellX = (int)(item->x / CELL_SIZE);
                int cellY = (int)(item->y / CELL_SIZE);
                int cellZ = (int)(item->z);
                if (!IsCellWalkableAt(cellZ, cellY, cellX)) continue;
                
                int slotX, slotY;
                int spIdx = FindStockpileForItem(item->type, &slotX, &slotY);
                if (spIdx < 0) continue;
                
                TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, false);
            }
        } else {
            // Fallback: linear scan
            for (int j = 0; j < MAX_ITEMS && idleMoverCount > 0; j++) {
                Item* item = &items[j];
                if (!item->active) continue;
                if (item->reservedBy != -1) continue;
                if (item->state != ITEM_ON_GROUND) continue;
                if (item->unreachableCooldown > 0.0f) continue;
                if (!typeHasStockpile[item->type]) continue;
                if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) continue;
                
                int cellX = (int)(item->x / CELL_SIZE);
                int cellY = (int)(item->y / CELL_SIZE);
                int cellZ = (int)(item->z);
                if (!IsCellWalkableAt(cellZ, cellY, cellX)) continue;
                
                int slotX, slotY;
                int spIdx = FindStockpileForItem(item->type, &slotX, &slotY);
                if (spIdx < 0) continue;
                
                TryAssignItemToMover(j, spIdx, slotX, slotY, false);
            }
        }
    }
    
    // =========================================================================
    // PRIORITY 3: Re-haul from overfull/low-priority stockpiles - ITEM-CENTRIC
    // =========================================================================
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
            
            bool noLongerAllowed = !StockpileAcceptsType(currentSp, items[j].type);
            
            if (noLongerAllowed) {
                destSp = FindStockpileForItem(items[j].type, &destSlotX, &destSlotY);
            } else if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
            } else {
                destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
            }
            
            if (destSp < 0) continue;
            
            TryAssignItemToMover(j, destSp, destSlotX, destSlotY, false);
        }
    }
    
    // =========================================================================
    // PRIORITY 4-6: Mining, BlueprintHaul, Build - MOVER-CENTRIC
    // These have sparse targets so mover-centric is acceptable
    // Skip entirely if no sparse targets exist (performance optimization)
    // =========================================================================
    if (idleMoverCount > 0) {
        // Quick check: any dig designations?
        bool hasDigWork = false;
        for (int z = 0; z < gridDepth && !hasDigWork; z++) {
            for (int y = 0; y < gridHeight && !hasDigWork; y++) {
                for (int x = 0; x < gridWidth && !hasDigWork; x++) {
                    Designation* d = GetDesignation(x, y, z);
                    if (d && d->type == DESIGNATION_DIG && d->assignedMover == -1) {
                        hasDigWork = true;
                    }
                }
            }
        }
        
        // Quick check: any blueprints needing materials or building?
        bool hasBlueprintWork = false;
        for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS && !hasBlueprintWork; bpIdx++) {
            Blueprint* bp = &blueprints[bpIdx];
            if (!bp->active) continue;
            if (bp->state == BLUEPRINT_AWAITING_MATERIALS && bp->reservedItem < 0) {
                hasBlueprintWork = true;
            } else if (bp->state == BLUEPRINT_READY_TO_BUILD && bp->assignedBuilder < 0) {
                hasBlueprintWork = true;
            }
        }
        
        // Only iterate movers if there's sparse work to do
        if (hasDigWork || hasBlueprintWork) {
            // Copy idle list since WorkGivers modify it
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            int idleCopyCount = idleMoverCount;
            memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));
            
            for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                int moverIdx = idleCopy[i];
                
                // Skip if already assigned by hauling above
                if (!moverIsInIdleList[moverIdx]) continue;
                
                // Try sparse-target WorkGivers only (skip hauling WorkGivers)
                int jobId = -1;
                if (hasDigWork) {
                    jobId = WorkGiver_Mining(moverIdx);
                }
                if (jobId < 0 && hasBlueprintWork) {
                    jobId = WorkGiver_BlueprintHaul(moverIdx);
                    if (jobId < 0) jobId = WorkGiver_Build(moverIdx);
                }
                
                (void)jobId;
            }
            
            free(idleCopy);
        }
    }
}

// =============================================================================
// AssignJobs - Main entry point (uses legacy for performance)
// =============================================================================

void AssignJobs(void) {
    AssignJobsLegacy();
}

// =============================================================================
// AssignJobsLegacy - Original optimized implementation (for benchmarking)
// =============================================================================

void AssignJobsLegacy(void) {
    // Rebuild idle mover list if not initialized
    if (!moverIsInIdleList) {
        InitJobSystem(MAX_MOVERS);
    }
    
    // Rebuild idle list each frame (fast O(moverCount) scan)
    // This ensures correctness after movers spawn/despawn
    RebuildIdleMoverList();
    
    // Early exit: no idle movers means no work to do
    if (idleMoverCount == 0) return;
    
    // Rebuild ground item cache for stockpiles (O(stockpiles + items), once per frame)
    // This enables O(1) checks in FindFreeStockpileSlot instead of O(items) per slot
    RebuildStockpileGroundItemCache();
    
    // Rebuild free slot counts (O(stockpiles × slots), once per frame)
    // This enables O(1) "is stockpile full?" check in FindStockpileForItem
    RebuildStockpileFreeSlotCounts();
    
    // Cache which item types have available stockpiles
    bool typeHasStockpile[ITEM_TYPE_COUNT] = {false};
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
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
    // TODO: Performance issue - when many items are clustered together, we still get
    // large spikes (100-300ms) even with early exits. Needs further investigation.
    // The issue may be in FindStockpileForItem, the spatial grid iteration, or TryAssignItemToMover.
    PROFILE_ACCUM_BEGIN(Jobs_FindGroundItem_StockpileCentric);
    // Early check: refresh typeHasStockpile cache and skip if no slots available
    bool anyTypeHasSlotSP = false;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        int slotX, slotY;
        typeHasStockpile[t] = (FindStockpileForItem((ItemType)t, &slotX, &slotY) >= 0);
        if (typeHasStockpile[t]) anyTypeHasSlotSP = true;
    }

    if (idleMoverCount > 0 && anyTypeHasSlotSP && itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        for (int spIdx = 0; spIdx < MAX_STOCKPILES && idleMoverCount > 0; spIdx++) {
            Stockpile* sp = &stockpiles[spIdx];
            if (!sp->active) continue;
            
            // Check each item type this stockpile accepts
            for (int t = 0; t < ITEM_TYPE_COUNT && idleMoverCount > 0; t++) {
                if (!sp->allowedTypes[t]) continue;
                if (!typeHasStockpile[t]) continue;  // Skip if no stockpile has slots for this type
                
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
    // Early exit: reuse the stockpile cache from stockpile-centric (anyTypeHasSlotSP)
    // No need to call FindStockpileForItem again - if stockpile-centric found no slots, neither will we
    if (idleMoverCount > 0 && anyTypeHasSlotSP && itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
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
            
            // Skip items on walls (can't be picked up)
            int cellX = (int)(item->x / CELL_SIZE);
            int cellY = (int)(item->y / CELL_SIZE);
            int cellZ = (int)(item->z);
            if (!IsCellWalkableAt(cellZ, cellY, cellX)) continue;
            
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
            
            // Skip items on walls (can't be picked up)
            int cellX = (int)(item->x / CELL_SIZE);
            int cellY = (int)(item->y / CELL_SIZE);
            int cellZ = (int)(item->z);
            if (!IsCellWalkableAt(cellZ, cellY, cellX)) continue;
            
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
            
            // Check if item is no longer allowed by current stockpile (filter changed)
            bool noLongerAllowed = !StockpileAcceptsType(currentSp, items[j].type);
            
            if (noLongerAllowed) {
                // Find any stockpile that accepts this item type
                destSp = FindStockpileForItem(items[j].type, &destSlotX, &destSlotY);
            } else if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
            } else {
                destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
            }
            
            if (destSp < 0) continue;
            
            TryAssignItemToMover(j, destSp, destSlotX, destSlotY, false);
        }
    }
    PROFILE_ACCUM_END(Jobs_FindRehaulItem);
    
    // PRIORITY 4: Mining designations
    PROFILE_ACCUM_BEGIN(Jobs_FindDigJob);
    if (idleMoverCount > 0) {
        // Find unassigned dig designations
        for (int z = 0; z < gridDepth && idleMoverCount > 0; z++) {
            for (int y = 0; y < gridHeight && idleMoverCount > 0; y++) {
                for (int x = 0; x < gridWidth && idleMoverCount > 0; x++) {
                    Designation* d = GetDesignation(x, y, z);
                    if (!d || d->type != DESIGNATION_DIG || d->assignedMover != -1) continue;
                    if (d->unreachableCooldown > 0.0f) continue;  // Skip if on cooldown
                    
                    // Find an adjacent walkable tile to stand on while digging
                    // Check 4 cardinal directions (miner stands adjacent to wall)
                    int adjX = -1, adjY = -1;
                    int dx4[] = {0, 1, 0, -1};
                    int dy4[] = {-1, 0, 1, 0};
                    for (int dir = 0; dir < 4; dir++) {
                        int ax = x + dx4[dir];
                        int ay = y + dy4[dir];
                        if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
                            if (IsCellWalkableAt(z, ay, ax)) {
                                adjX = ax;
                                adjY = ay;
                                break;
                            }
                        }
                    }
                    
                    if (adjX < 0) continue;  // No adjacent walkable tile
                    
                    // Find nearest idle mover with canMine capability
                    float digPosX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
                    float digPosY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
                    
                    int moverIdx = -1;
                    float bestDistSq = 1e30f;
                    
                    for (int i = 0; i < idleMoverCount; i++) {
                        int idx = idleMoverList[i];
                        if ((int)movers[idx].z != z) continue;  // Same z-level only for now
                        if (!movers[idx].capabilities.canMine) continue;  // Check mining capability
                        float mdx = movers[idx].x - digPosX;
                        float mdy = movers[idx].y - digPosY;
                        float distSq = mdx * mdx + mdy * mdy;
                        if (distSq < bestDistSq) {
                            bestDistSq = distSq;
                            moverIdx = idx;
                        }
                    }
                    
                    if (moverIdx < 0) continue;
                    
                    Mover* m = &movers[moverIdx];
                    
                    // Check reachability
                    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
                    Point adjCell = { adjX, adjY, z };
                    
                    Point tempPath[MAX_PATH];
                    int tempLen = FindPath(moverPathAlgorithm, moverCell, adjCell, tempPath, MAX_PATH);
                    
                    if (tempLen == 0) {
                        // Can't reach - set cooldown to avoid spamming
                        d->unreachableCooldown = UNREACHABLE_COOLDOWN;
                        continue;
                    }
                    
                    // Create Job entry
                    int jobId = CreateJob(JOBTYPE_DIG);
                    if (jobId >= 0) {
                        Job* job = GetJob(jobId);
                        job->assignedMover = moverIdx;
                        job->targetDigX = x;
                        job->targetDigY = y;
                        job->targetDigZ = z;
                        job->step = 0;  // STEP_MOVING_TO_WORK
                        job->progress = 0.0f;
                        m->currentJobId = jobId;
                    }
                    
                    // Assign dig job
                    d->assignedMover = moverIdx;
                    m->goal = adjCell;
                    m->needsRepath = true;
                    
                    RemoveMoverFromIdleList(moverIdx);
                }
            }
        }
    }
    PROFILE_ACCUM_END(Jobs_FindDigJob);
    
    // PRIORITY 5: Haul materials to blueprints
    PROFILE_ACCUM_BEGIN(Jobs_FindBlueprintHaulJob);
    if (idleMoverCount > 0) {
        // Find blueprints needing materials
        for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS && idleMoverCount > 0; bpIdx++) {
            Blueprint* bp = &blueprints[bpIdx];
            if (!bp->active) continue;
            if (bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;
            if (bp->reservedItem >= 0) continue;  // Already has an item reserved
            
            // Find an available orange item (stone blocks for building walls)
            int itemIdx = -1;
            float bestDistSq = 1e30f;
            float bpX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
            float bpY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;
            
            // Linear scan for items (could use spatial grid for optimization)
            for (int j = 0; j < MAX_ITEMS; j++) {
                Item* item = &items[j];
                if (!item->active) continue;
                if (item->type != ITEM_ORANGE) continue;  // Only orange blocks for building
                if (item->reservedBy != -1) continue;
                if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
                if (item->unreachableCooldown > 0.0f) continue;
                
                float dx = item->x - bpX;
                float dy = item->y - bpY;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    itemIdx = j;
                }
            }
            
            if (itemIdx < 0) continue;  // No available items
            
            // Find nearest idle mover with canHaul capability
            Item* item = &items[itemIdx];
            int moverIdx = -1;
            float bestMoverDistSq = 1e30f;
            
            for (int i = 0; i < idleMoverCount; i++) {
                int idx = idleMoverList[i];
                if ((int)movers[idx].z != bp->z) continue;  // Same z-level for now
                if (!movers[idx].capabilities.canHaul) continue;  // Check hauling capability
                float mdx = movers[idx].x - item->x;
                float mdy = movers[idx].y - item->y;
                float distSq = mdx * mdx + mdy * mdy;
                if (distSq < bestMoverDistSq) {
                    bestMoverDistSq = distSq;
                    moverIdx = idx;
                }
            }
            
            if (moverIdx < 0) continue;  // No idle mover on this z-level
            
            Mover* m = &movers[moverIdx];
            
            // Check reachability to item
            Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
            Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
            
            Point tempPath[MAX_PATH];
            int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
            
            if (tempLen == 0) continue;  // Can't reach item
            
            // Reserve item for this blueprint
            if (!ReserveItem(itemIdx, moverIdx)) continue;
            bp->reservedItem = itemIdx;
            
            // NOTE: Don't call ClearSourceStockpileSlot here - RunJob_HaulToBlueprint handles it
            // when the item is actually picked up. This avoids making items invisible in stockpile.
            
            // Create Job entry
            int jobId = CreateJob(JOBTYPE_HAUL_TO_BLUEPRINT);
            if (jobId >= 0) {
                Job* job = GetJob(jobId);
                job->assignedMover = moverIdx;
                job->targetItem = itemIdx;
                job->targetBlueprint = bpIdx;
                job->targetSlotX = bp->x;
                job->targetSlotY = bp->y;
                job->step = 0;  // STEP_MOVING_TO_PICKUP
                m->currentJobId = jobId;
            }
            
            // Set mover goal
            m->goal = itemCell;
            m->needsRepath = true;
            
            RemoveMoverFromIdleList(moverIdx);
        }
    }
    PROFILE_ACCUM_END(Jobs_FindBlueprintHaulJob);
    
    // PRIORITY 6: Build at blueprints with materials delivered
    PROFILE_ACCUM_BEGIN(Jobs_FindBuildJob);
    if (idleMoverCount > 0) {
        for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS && idleMoverCount > 0; bpIdx++) {
            Blueprint* bp = &blueprints[bpIdx];
            if (!bp->active) continue;
            if (bp->state != BLUEPRINT_READY_TO_BUILD) continue;
            if (bp->assignedBuilder >= 0) continue;  // Already has a builder
            
            // Find nearest idle mover with canBuild capability
            float bpX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
            float bpY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;
            
            int moverIdx = -1;
            float bestDistSq = 1e30f;
            
            for (int i = 0; i < idleMoverCount; i++) {
                int idx = idleMoverList[i];
                if ((int)movers[idx].z != bp->z) continue;  // Same z-level
                if (!movers[idx].capabilities.canBuild) continue;  // Check building capability
                float dx = movers[idx].x - bpX;
                float dy = movers[idx].y - bpY;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    moverIdx = idx;
                }
            }
            
            if (moverIdx < 0) continue;
            
            Mover* m = &movers[moverIdx];
            
            // Check reachability to blueprint
            Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
            Point bpCell = { bp->x, bp->y, bp->z };
            
            Point tempPath[MAX_PATH];
            int tempLen = FindPath(moverPathAlgorithm, moverCell, bpCell, tempPath, MAX_PATH);
            
            if (tempLen == 0) continue;  // Can't reach blueprint
            
            // Create Job entry
            int jobId = CreateJob(JOBTYPE_BUILD);
            if (jobId >= 0) {
                Job* job = GetJob(jobId);
                job->assignedMover = moverIdx;
                job->targetBlueprint = bpIdx;
                job->step = 0;  // STEP_MOVING_TO_WORK
                job->progress = 0.0f;
                m->currentJobId = jobId;
            }
            
            // Assign builder to blueprint
            bp->assignedBuilder = moverIdx;
            bp->state = BLUEPRINT_BUILDING;
            
            // Set mover goal
            m->goal = bpCell;
            m->needsRepath = true;
            
            RemoveMoverFromIdleList(moverIdx);
        }
    }
    PROFILE_ACCUM_END(Jobs_FindBuildJob);
}


// =============================================================================
// WorkGivers (Phase 4 of Jobs Refactor)
// =============================================================================

// WorkGiver_Haul: Find a ground item to haul to a stockpile
// Returns job ID if successful, -1 if no job available
// Filter context for WorkGiver_Haul spatial search
typedef struct {
    bool* typeHasStockpile;  // Pointer to array
} HaulFilterContext;

static bool HaulItemFilter(int itemIdx, void* userData) {
    HaulFilterContext* ctx = (HaulFilterContext*)userData;
    Item* item = &items[itemIdx];
    
    if (!item->active) return false;
    if (item->reservedBy != -1) return false;
    if (item->state != ITEM_ON_GROUND) return false;
    if (item->unreachableCooldown > 0.0f) return false;
    if (!ctx->typeHasStockpile[item->type]) return false;
    if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) return false;
    
    // Skip items on walls
    int cellX = (int)(item->x / CELL_SIZE);
    int cellY = (int)(item->y / CELL_SIZE);
    int cellZ = (int)(item->z);
    if (!IsCellWalkableAt(cellZ, cellY, cellX)) return false;
    
    return true;
}

int WorkGiver_Haul(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability
    if (!m->capabilities.canHaul) return -1;
    
    // Cache which item types have available stockpiles
    bool typeHasStockpile[ITEM_TYPE_COUNT] = {false};
    bool anyTypeHasSlot = false;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        int slotX, slotY;
        if (FindStockpileForItem((ItemType)t, &slotX, &slotY) >= 0) {
            typeHasStockpile[t] = true;
            anyTypeHasSlot = true;
        }
    }
    
    if (!anyTypeHasSlot) return -1;
    
    int moverTileX = (int)(m->x / CELL_SIZE);
    int moverTileY = (int)(m->y / CELL_SIZE);
    int moverZ = (int)m->z;
    
    int bestItemIdx = -1;
    
    // Use spatial grid with expanding radius if available
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        HaulFilterContext ctx = { .typeHasStockpile = typeHasStockpile };
        
        // Expanding radius search - find closest items first
        int radii[] = {10, 25, 50, 100};
        int numRadii = sizeof(radii) / sizeof(radii[0]);
        
        for (int r = 0; r < numRadii && bestItemIdx < 0; r++) {
            bestItemIdx = FindFirstItemInRadius(moverTileX, moverTileY, moverZ, radii[r],
                                                HaulItemFilter, &ctx);
        }
    } else {
        // Fallback: linear scan when spatial grid not built (for tests)
        float bestDistSq = 1e30f;
        for (int j = 0; j < MAX_ITEMS; j++) {
            Item* item = &items[j];
            if (!item->active) continue;
            if (item->reservedBy != -1) continue;
            if (item->state != ITEM_ON_GROUND) continue;
            if (item->unreachableCooldown > 0.0f) continue;
            if (!typeHasStockpile[item->type]) continue;
            if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) continue;
            
            // Skip items on walls
            int cellX = (int)(item->x / CELL_SIZE);
            int cellY = (int)(item->y / CELL_SIZE);
            int cellZ = (int)(item->z);
            if (!IsCellWalkableAt(cellZ, cellY, cellX)) continue;
            
            float dx = item->x - m->x;
            float dy = item->y - m->y;
            float distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestItemIdx = j;
            }
        }
    }
    
    if (bestItemIdx < 0) return -1;
    
    Item* item = &items[bestItemIdx];
    
    // Find stockpile slot
    int slotX, slotY;
    int spIdx = FindStockpileForItem(item->type, &slotX, &slotY);
    if (spIdx < 0) return -1;
    
    // Check reachability
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    if (tempLen == 0) {
        SetItemUnreachableCooldown(bestItemIdx, UNREACHABLE_COOLDOWN);
        return -1;
    }
    
    // Reserve item and stockpile slot
    if (!ReserveItem(bestItemIdx, moverIdx)) return -1;
    if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx)) {
        ReleaseItemReservation(bestItemIdx);
        return -1;
    }
    
    // Create job
    int jobId = CreateJob(JOBTYPE_HAUL);
    if (jobId < 0) {
        ReleaseItemReservation(bestItemIdx);
        ReleaseStockpileSlot(spIdx, slotX, slotY);
        return -1;
    }
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = bestItemIdx;
    job->targetStockpile = spIdx;
    job->targetSlotX = slotX;
    job->targetSlotY = slotY;
    job->step = 0;  // STEP_MOVING_TO_PICKUP
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = itemCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}

// WorkGiver_StockpileMaintenance: Handle ground items on stockpile tiles (absorb/clear)
// This is the highest priority - items on stockpile tiles must be absorbed or cleared
// Returns job ID if successful, -1 if no job available
int WorkGiver_StockpileMaintenance(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability
    if (!m->capabilities.canHaul) return -1;
    
    // Find a ground item on a stockpile tile
    int spOnItem = -1;
    bool absorb = false;
    int itemIdx = FindGroundItemOnStockpile(&spOnItem, &absorb);
    
    if (itemIdx < 0) return -1;
    if (items[itemIdx].unreachableCooldown > 0.0f) return -1;
    
    Item* item = &items[itemIdx];
    
    int slotX, slotY, spIdx;
    bool safeDrop = false;
    
    if (absorb) {
        // Absorb: destination is same tile (item matches stockpile filter)
        spIdx = spOnItem;
        slotX = (int)(item->x / CELL_SIZE);
        slotY = (int)(item->y / CELL_SIZE);
    } else {
        // Clear: find destination stockpile or safe-drop location
        spIdx = FindStockpileForItem(item->type, &slotX, &slotY);
        if (spIdx < 0) {
            safeDrop = true;  // No stockpile accepts this type, safe-drop it
        }
    }
    
    // Reserve item
    if (!ReserveItem(itemIdx, moverIdx)) return -1;
    
    // Reserve stockpile slot (unless safeDrop)
    if (!safeDrop) {
        if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx)) {
            ReleaseItemReservation(itemIdx);
            return -1;
        }
    }
    
    // Check reachability
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    
    if (tempLen == 0) {
        // Can't reach - release reservations and set cooldown
        ReleaseItemReservation(itemIdx);
        if (!safeDrop) {
            ReleaseStockpileSlot(spIdx, slotX, slotY);
        }
        SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        return -1;
    }
    
    // Create job
    int jobId = CreateJob(safeDrop ? JOBTYPE_CLEAR : JOBTYPE_HAUL);
    if (jobId < 0) {
        ReleaseItemReservation(itemIdx);
        if (!safeDrop) {
            ReleaseStockpileSlot(spIdx, slotX, slotY);
        }
        return -1;
    }
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = itemIdx;
    job->targetStockpile = spIdx;
    job->targetSlotX = safeDrop ? -1 : slotX;
    job->targetSlotY = safeDrop ? -1 : slotY;
    job->step = 0;  // STEP_MOVING_TO_PICKUP
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = itemCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}

// WorkGiver_Rehaul: Re-haul items from overfull/low-priority stockpiles
// Handles: filter changes (item no longer allowed), overfull slots, higher priority transfers
// Returns job ID if successful, -1 if no job available
int WorkGiver_Rehaul(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability
    if (!m->capabilities.canHaul) return -1;
    
    int moverZ = (int)m->z;
    
    // Find an item in a stockpile that needs re-hauling
    int bestItemIdx = -1;
    int bestDestSp = -1;
    int bestDestSlotX = -1, bestDestSlotY = -1;
    float bestDistSq = 1e30f;
    
    for (int j = 0; j < MAX_ITEMS; j++) {
        if (!items[j].active) continue;
        if (items[j].reservedBy != -1) continue;
        if (items[j].state != ITEM_IN_STOCKPILE) continue;
        if ((int)items[j].z != moverZ) continue;  // Same z-level for now
        
        int currentSp = -1;
        if (!IsPositionInStockpile(items[j].x, items[j].y, (int)items[j].z, &currentSp)) continue;
        if (currentSp < 0) continue;
        
        int itemSlotX = (int)(items[j].x / CELL_SIZE);
        int itemSlotY = (int)(items[j].y / CELL_SIZE);
        
        int destSlotX, destSlotY;
        int destSp = -1;
        
        // Check if item is no longer allowed by current stockpile (filter changed)
        bool noLongerAllowed = !StockpileAcceptsType(currentSp, items[j].type);
        
        if (noLongerAllowed) {
            // Find any stockpile that accepts this item type
            destSp = FindStockpileForItem(items[j].type, &destSlotX, &destSlotY);
        } else if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
            destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
        } else {
            destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
        }
        
        if (destSp < 0) continue;
        
        // Calculate distance to item
        float dx = items[j].x - m->x;
        float dy = items[j].y - m->y;
        float distSq = dx * dx + dy * dy;
        
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestItemIdx = j;
            bestDestSp = destSp;
            bestDestSlotX = destSlotX;
            bestDestSlotY = destSlotY;
        }
    }
    
    if (bestItemIdx < 0) return -1;
    
    Item* item = &items[bestItemIdx];
    
    // Reserve item
    if (!ReserveItem(bestItemIdx, moverIdx)) return -1;
    
    // Reserve destination stockpile slot
    if (!ReserveStockpileSlot(bestDestSp, bestDestSlotX, bestDestSlotY, moverIdx)) {
        ReleaseItemReservation(bestItemIdx);
        return -1;
    }
    
    // Check reachability BEFORE clearing source slot
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    
    if (tempLen == 0) {
        ReleaseItemReservation(bestItemIdx);
        ReleaseStockpileSlot(bestDestSp, bestDestSlotX, bestDestSlotY);
        return -1;
    }
    
    // NOTE: Don't call ClearSourceStockpileSlot here - RunJob_Haul handles it
    // when the item is actually picked up. This avoids double-decrementing.
    
    // Create job
    int jobId = CreateJob(JOBTYPE_HAUL);
    if (jobId < 0) {
        ReleaseItemReservation(bestItemIdx);
        ReleaseStockpileSlot(bestDestSp, bestDestSlotX, bestDestSlotY);
        return -1;
    }
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = bestItemIdx;
    job->targetStockpile = bestDestSp;
    job->targetSlotX = bestDestSlotX;
    job->targetSlotY = bestDestSlotY;
    job->step = 0;  // STEP_MOVING_TO_PICKUP
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = itemCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}

// WorkGiver_Mining: Find a dig designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_Mining(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability
    if (!m->capabilities.canMine) return -1;
    
    int moverZ = (int)m->z;
    
    // Find nearest unassigned dig designation
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = GetDesignation(x, y, z);
                if (!d || d->type != DESIGNATION_DIG || d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;
                
                // Find adjacent walkable tile
                int adjX = -1, adjY = -1;
                int dx4[] = {0, 1, 0, -1};
                int dy4[] = {-1, 0, 1, 0};
                for (int dir = 0; dir < 4; dir++) {
                    int ax = x + dx4[dir];
                    int ay = y + dy4[dir];
                    if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
                        if (IsCellWalkableAt(z, ay, ax)) {
                            adjX = ax;
                            adjY = ay;
                            break;
                        }
                    }
                }
                
                if (adjX < 0) continue;  // No adjacent walkable tile
                
                // Only same z-level for now
                if (z != moverZ) continue;
                
                float digPosX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
                float digPosY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
                float dx = digPosX - m->x;
                float dy = digPosY - m->y;
                float distSq = dx * dx + dy * dy;
                
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestDesigX = x;
                    bestDesigY = y;
                    bestDesigZ = z;
                    bestAdjX = adjX;
                    bestAdjY = adjY;
                }
            }
        }
    }
    
    if (bestDesigX < 0) return -1;
    
    // Check reachability
    Point adjCell = { bestAdjX, bestAdjY, bestDesigZ };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, adjCell, tempPath, MAX_PATH);
    if (tempLen == 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }
    
    // Create job
    int jobId = CreateJob(JOBTYPE_DIG);
    if (jobId < 0) return -1;
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetDigX = bestDesigX;
    job->targetDigY = bestDesigY;
    job->targetDigZ = bestDesigZ;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;
    
    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = adjCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}

// WorkGiver_Build: Find a blueprint ready to build
// Returns job ID if successful, -1 if no job available
int WorkGiver_Build(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability
    if (!m->capabilities.canBuild) return -1;
    
    int moverZ = (int)m->z;
    
    // Find nearest blueprint ready to build
    int bestBpIdx = -1;
    float bestDistSq = 1e30f;
    
    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_READY_TO_BUILD) continue;
        if (bp->assignedBuilder >= 0) continue;  // Already has a builder
        if (bp->z != moverZ) continue;  // Same z-level for now
        
        float bpX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float bpY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = bpX - m->x;
        float dy = bpY - m->y;
        float distSq = dx * dx + dy * dy;
        
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestBpIdx = bpIdx;
        }
    }
    
    if (bestBpIdx < 0) return -1;
    
    Blueprint* bp = &blueprints[bestBpIdx];
    
    // Check reachability
    Point bpCell = { bp->x, bp->y, bp->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, bpCell, tempPath, MAX_PATH);
    if (tempLen == 0) return -1;
    
    // Create job
    int jobId = CreateJob(JOBTYPE_BUILD);
    if (jobId < 0) return -1;
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetBlueprint = bestBpIdx;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;
    
    // Reserve blueprint
    bp->assignedBuilder = moverIdx;
    bp->state = BLUEPRINT_BUILDING;
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = bpCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}

// WorkGiver_BlueprintHaul: Find material to haul to a blueprint
// Returns job ID if successful, -1 if no job available
// Filter for blueprint haul items (ITEM_ORANGE only)
static bool BlueprintHaulItemFilter(int itemIdx, void* userData) {
    (void)userData;
    Item* item = &items[itemIdx];
    
    if (!item->active) return false;
    if (item->type != ITEM_ORANGE) return false;  // Only orange blocks for building
    if (item->reservedBy != -1) return false;
    if (item->state != ITEM_ON_GROUND) return false;  // Spatial grid only has ground items
    if (item->unreachableCooldown > 0.0f) return false;
    
    return true;
}

int WorkGiver_BlueprintHaul(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability
    if (!m->capabilities.canHaul) return -1;
    
    int moverZ = (int)m->z;
    
    // First check if any blueprint needs materials on this z-level
    bool anyBlueprintNeedsMaterials = false;
    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;
        if (bp->reservedItem >= 0) continue;
        if (bp->z != moverZ) continue;
        anyBlueprintNeedsMaterials = true;
        break;
    }
    if (!anyBlueprintNeedsMaterials) return -1;
    
    int moverTileX = (int)(m->x / CELL_SIZE);
    int moverTileY = (int)(m->y / CELL_SIZE);
    
    // Find nearest ITEM_ORANGE
    int bestItemIdx = -1;
    float bestDistSq = 1e30f;
    
    // Try spatial grid first for ground items
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        int radii[] = {10, 25, 50, 100};
        int numRadii = sizeof(radii) / sizeof(radii[0]);
        
        for (int r = 0; r < numRadii && bestItemIdx < 0; r++) {
            bestItemIdx = FindFirstItemInRadius(moverTileX, moverTileY, moverZ, radii[r],
                                                BlueprintHaulItemFilter, NULL);
        }
        
        // Get distance for found item
        if (bestItemIdx >= 0) {
            float dx = items[bestItemIdx].x - m->x;
            float dy = items[bestItemIdx].y - m->y;
            bestDistSq = dx * dx + dy * dy;
        }
    }
    
    // Linear scan for all ITEM_ORANGE (fallback for tests, plus checks stockpile items)
    for (int j = 0; j < MAX_ITEMS; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (item->type != ITEM_ORANGE) continue;
        if (item->reservedBy != -1) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        if ((int)item->z != moverZ) continue;
        
        float dx = item->x - m->x;
        float dy = item->y - m->y;
        float distSq = dx * dx + dy * dy;
        
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestItemIdx = j;
        }
    }
    
    if (bestItemIdx < 0) return -1;
    
    // Now find any blueprint that needs materials (simple linear scan - blueprints are sparse)
    int bestBpIdx = -1;
    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;
        if (bp->reservedItem >= 0) continue;
        if (bp->z != moverZ) continue;
        bestBpIdx = bpIdx;
        break;  // Take first available
    }
    
    if (bestBpIdx < 0) return -1;
    
    Item* item = &items[bestItemIdx];
    Blueprint* bp = &blueprints[bestBpIdx];
    
    // Check reachability to item
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    if (tempLen == 0) return -1;
    
    // Reserve item
    if (!ReserveItem(bestItemIdx, moverIdx)) return -1;
    bp->reservedItem = bestItemIdx;
    
    // NOTE: Don't call ClearSourceStockpileSlot here - RunJob_HaulToBlueprint handles it
    // when the item is actually picked up. This avoids making items invisible in stockpile.
    
    // Create job
    int jobId = CreateJob(JOBTYPE_HAUL_TO_BLUEPRINT);
    if (jobId < 0) {
        ReleaseItemReservation(bestItemIdx);
        bp->reservedItem = -1;
        return -1;
    }
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = bestItemIdx;
    job->targetBlueprint = bestBpIdx;
    job->targetSlotX = bp->x;
    job->targetSlotY = bp->y;
    job->step = 0;  // STEP_MOVING_TO_PICKUP
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = itemCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}
