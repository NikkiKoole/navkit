#include "jobs.h"
#include "items.h"
#include "item_defs.h"
#include "mover.h"
#include "workshops.h"
#include "../core/time.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/pathfinding.h"
#include "stockpiles.h"
#include "../world/designations.h"
#include "../../shared/profiler.h"
#include "../../shared/ui.h"
#include "../../vendor/raylib.h"
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

// Direction offsets for cardinal neighbors (N, E, S, W)
static const int DIR_DX[4] = {0, 1, 0, -1};
static const int DIR_DY[4] = {-1, 0, 1, 0};

// Helper: Check if mover's path is exhausted (no path or index exhausted)
static inline bool IsPathExhausted(Mover* mover) {
    return mover->pathLength == 0 || mover->pathIndex < 0;
}

// Helper: Final approach - move mover directly toward target when path exhausted but close
// Returns true if micro-movement was applied
static bool TryFinalApproach(Mover* mover, float targetX, float targetY, int targetCellX, int targetCellY, float radius) {
    if (!IsPathExhausted(mover)) return false;
    
    float dx = mover->x - targetX;
    float dy = mover->y - targetY;
    float distSq = dx * dx + dy * dy;
    
    if (distSq < radius * radius) return false;  // Already in range
    
    int moverCellX = (int)(mover->x / CELL_SIZE);
    int moverCellY = (int)(mover->y / CELL_SIZE);
    bool inSameOrAdjacentCell = (abs(moverCellX - targetCellX) <= 1 && abs(moverCellY - targetCellY) <= 1);
    
    if (!inSameOrAdjacentCell) return false;
    
    // Move directly toward target
    float dist = sqrtf(distSq);
    float moveSpeed = mover->speed * TICK_DT;
    if (dist > 0.01f) {
        mover->x -= (dx / dist) * moveSpeed;
        mover->y -= (dy / dist) * moveSpeed;
    }
    return true;
}

// =============================================================================
// Designation Caches - built once per frame for job assignment performance
// =============================================================================
#define MAX_DESIGNATION_CACHE 4096

// Cache entry for designations where mover stands adjacent (mine, remove ramp)
typedef struct {
    int x, y, z;       // Designation coordinates
    int adjX, adjY;    // Adjacent walkable tile (for mover to stand on)
} AdjacentDesignationEntry;

// Cache entry for designations where mover stands on tile (channel, remove floor)
typedef struct {
    int x, y, z;       // Designation coordinates (mover stands ON this tile)
} OnTileDesignationEntry;

// Mine designation cache
static AdjacentDesignationEntry mineCache[MAX_DESIGNATION_CACHE];
static int mineCacheCount = 0;

// Channel designation cache
static OnTileDesignationEntry channelCache[MAX_DESIGNATION_CACHE];
static int channelCacheCount = 0;

// Remove floor designation cache
static OnTileDesignationEntry removeFloorCache[MAX_DESIGNATION_CACHE];
static int removeFloorCacheCount = 0;

// Remove ramp designation cache
static AdjacentDesignationEntry removeRampCache[MAX_DESIGNATION_CACHE];
static int removeRampCacheCount = 0;

// Helper: Find first adjacent walkable tile. Returns true if found.
static bool FindAdjacentWalkable(int x, int y, int z, int* outAdjX, int* outAdjY) {
    for (int dir = 0; dir < 4; dir++) {
        int ax = x + DIR_DX[dir];
        int ay = y + DIR_DY[dir];
        if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
            if (IsCellWalkableAt(z, ay, ax)) {
                *outAdjX = ax;
                *outAdjY = ay;
                return true;
            }
        }
    }
    return false;
}

// Rebuild cache for designations requiring adjacent standing position
static void RebuildAdjacentDesignationCache(DesignationType type,
                                            AdjacentDesignationEntry* cache,
                                            int* count) {
    *count = 0;
    if (activeDesignationCount == 0) return;

    for (int z = 0; z < gridDepth && *count < MAX_DESIGNATION_CACHE; z++) {
        for (int y = 0; y < gridHeight && *count < MAX_DESIGNATION_CACHE; y++) {
            for (int x = 0; x < gridWidth && *count < MAX_DESIGNATION_CACHE; x++) {
                Designation* d = GetDesignation(x, y, z);
                if (!d || d->type != type) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;

                int adjX, adjY;
                if (!FindAdjacentWalkable(x, y, z, &adjX, &adjY)) continue;

                cache[(*count)++] = (AdjacentDesignationEntry){x, y, z, adjX, adjY};
            }
        }
    }
}

// Rebuild cache for designations where mover stands on the tile
static void RebuildOnTileDesignationCache(DesignationType type,
                                          OnTileDesignationEntry* cache,
                                          int* count) {
    *count = 0;
    if (activeDesignationCount == 0) return;

    for (int z = 0; z < gridDepth && *count < MAX_DESIGNATION_CACHE; z++) {
        for (int y = 0; y < gridHeight && *count < MAX_DESIGNATION_CACHE; y++) {
            for (int x = 0; x < gridWidth && *count < MAX_DESIGNATION_CACHE; x++) {
                Designation* d = GetDesignation(x, y, z);
                if (!d || d->type != type) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;

                cache[(*count)++] = (OnTileDesignationEntry){x, y, z};
            }
        }
    }
}

void RebuildMineDesignationCache(void) {
    RebuildAdjacentDesignationCache(DESIGNATION_MINE, mineCache, &mineCacheCount);
}

void RebuildChannelDesignationCache(void) {
    RebuildOnTileDesignationCache(DESIGNATION_CHANNEL, channelCache, &channelCacheCount);
}

void RebuildRemoveFloorDesignationCache(void) {
    RebuildOnTileDesignationCache(DESIGNATION_REMOVE_FLOOR, removeFloorCache, &removeFloorCacheCount);
}

void RebuildRemoveRampDesignationCache(void) {
    RebuildAdjacentDesignationCache(DESIGNATION_REMOVE_RAMP, removeRampCache, &removeRampCacheCount);
}

// Find first adjacent tile that is both walkable and reachable from moverCell.
// Returns true if found, storing result in outX/outY.
static bool FindReachableAdjacentTile(int targetX, int targetY, int targetZ,
                                       Point moverCell, int* outX, int* outY) {
    Point tempPath[MAX_PATH];
    for (int dir = 0; dir < 4; dir++) {
        int ax = targetX + DIR_DX[dir];
        int ay = targetY + DIR_DY[dir];
        if (ax < 0 || ax >= gridWidth || ay < 0 || ay >= gridHeight) continue;
        if (!IsCellWalkableAt(targetZ, ay, ax)) continue;

        Point adjCell = { ax, ay, targetZ };
        if (FindPath(moverPathAlgorithm, moverCell, adjCell, tempPath, MAX_PATH) > 0) {
            *outX = ax;
            *outY = ay;
            return true;
        }
    }
    return false;
}

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

    if (!jobFreeList || !activeJobList || !jobIsActive) {
        TraceLog(LOG_ERROR, "Failed to allocate job pool memory");
        return;
    }

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
        jobs[i].targetMineX = -1;
        jobs[i].targetMineY = -1;
        jobs[i].targetMineZ = -1;
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
        jobs[i].targetMineX = -1;
        jobs[i].targetMineY = -1;
        jobs[i].targetMineZ = -1;
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
    job->targetMineX = -1;
    job->targetMineY = -1;
    job->targetMineZ = -1;
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
        if (IsPathExhausted(mover) && distSq >= PICKUP_RADIUS * PICKUP_RADIUS) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }

        // Final approach - move directly toward item when close but path exhausted
        TryFinalApproach(mover, item->x, item->y, itemCellX, itemCellY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
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
        if (IsPathExhausted(mover) && distSq >= DROP_RADIUS * DROP_RADIUS) {
            mover->goal.x = job->targetSlotX;
            mover->goal.y = job->targetSlotY;
            mover->goal.z = stockpiles[job->targetStockpile].z;
            mover->needsRepath = true;
        }

        // Final approach - move directly toward drop location when close but path exhausted
        TryFinalApproach(mover, targetX, targetY, job->targetSlotX, job->targetSlotY, DROP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
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

        // Request repath if path exhausted and not at destination
        if (IsPathExhausted(mover) && distSq >= PICKUP_RADIUS * PICKUP_RADIUS) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }

        // Final approach - move directly toward item when close but path exhausted
        TryFinalApproach(mover, item->x, item->y, itemCellX, itemCellY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
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

        // Check if arrived at drop location
        float targetX = job->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
        float targetY = job->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - targetX;
        float dy = mover->y - targetY;
        float distSq = dx*dx + dy*dy;

        // Request repath if path exhausted and not at destination
        if (IsPathExhausted(mover) && distSq >= DROP_RADIUS * DROP_RADIUS) {
            mover->goal.x = job->targetSlotX;
            mover->goal.y = job->targetSlotY;
            mover->goal.z = (int)mover->z;
            mover->needsRepath = true;
        }

        // Final approach - move directly toward drop location when close but path exhausted
        TryFinalApproach(mover, targetX, targetY, job->targetSlotX, job->targetSlotY, DROP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }

        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;

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

// Mine job driver: move to adjacent tile -> mine wall
JobRunResult RunJob_Mine(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    (void)job->assignedMover;  // Currently unused but available if needed

    // Check if designation still exists
    Designation* d = GetDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
    if (!d || d->type != DESIGNATION_MINE) {
        return JOBRUN_FAIL;
    }

    // Check if the wall was already mined
    if (grid[job->targetMineZ][job->targetMineY][job->targetMineX] != CELL_WALL) {
        CancelDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        // Use cached adjacent tile (set when job was created)
        int adjX = job->targetAdjX;
        int adjY = job->targetAdjY;

        // Set goal if not already moving there
        if (mover->goal.x != adjX || mover->goal.y != adjY || mover->goal.z != job->targetMineZ) {
            mover->goal = (Point){adjX, adjY, job->targetMineZ};
            mover->needsRepath = true;
        }

        // Check if arrived at adjacent tile
        float goalX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == job->targetMineZ;

        // Final approach - move directly toward work location when close but path exhausted
        if (correctZ) TryFinalApproach(mover, goalX, goalY, adjX, adjY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            Designation* desig = GetDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
            if (desig) desig->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress mining
        job->progress += dt / MINE_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Mining complete!
            CompleteMineDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Channel job driver: move to tile -> channel (remove floor, mine below, create ramp)
JobRunResult RunJob_Channel(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;  // Reusing mine target fields for channel coordinates
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHANNEL) {
        return JOBRUN_FAIL;
    }

    // Check if floor still exists
    // Either has explicit floor flag or standing on solid below
    bool hasFloor = HAS_FLOOR(tx, ty, tz) || (tz > 0 && CellIsSolid(grid[tz-1][ty][tx]));
    if (!hasFloor) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        // Channel: mover stands ON the tile (not adjacent like mining)
        if (mover->goal.x != tx || mover->goal.y != ty || mover->goal.z != tz) {
            mover->goal = (Point){tx, ty, tz};
            mover->needsRepath = true;
        }

        // Check if arrived at target tile
        float goalX = tx * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = ty * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == tz;

        // Final approach - move directly toward work location when close but path exhausted
        if (correctZ) TryFinalApproach(mover, goalX, goalY, tx, ty, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress channeling
        job->progress += dt / CHANNEL_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Channeling complete!
            // Pass mover index so CompleteChannelDesignation can handle their descent
            CompleteChannelDesignation(tx, ty, tz, job->assignedMover);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Remove floor job driver: move to tile -> remove floor (mover may fall!)
JobRunResult RunJob_RemoveFloor(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;  // Reusing mine target fields
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_REMOVE_FLOOR) {
        return JOBRUN_FAIL;
    }

    // Check if floor still exists
    if (!HAS_FLOOR(tx, ty, tz)) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        // Mover stands ON the tile to remove the floor
        if (mover->goal.x != tx || mover->goal.y != ty || mover->goal.z != tz) {
            mover->goal = (Point){tx, ty, tz};
            mover->needsRepath = true;
        }

        // Check if arrived at target tile
        float goalX = tx * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = ty * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == tz;

        // Final approach - move directly toward work location when close but path exhausted
        if (correctZ) TryFinalApproach(mover, goalX, goalY, tx, ty, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress floor removal
        job->progress += dt / REMOVE_FLOOR_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Floor removal complete!
            CompleteRemoveFloorDesignation(tx, ty, tz, job->assignedMover);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Remove ramp job driver: move to adjacent tile -> remove ramp
JobRunResult RunJob_RemoveRamp(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;  // Reusing mine target fields
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_REMOVE_RAMP) {
        return JOBRUN_FAIL;
    }

    // Check if ramp still exists
    if (!CellIsRamp(grid[tz][ty][tx])) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        // Use cached adjacent tile (set when job was created)
        int adjX = job->targetAdjX;
        int adjY = job->targetAdjY;

        // Set goal if not already moving there
        if (mover->goal.x != adjX || mover->goal.y != adjY || mover->goal.z != tz) {
            mover->goal = (Point){adjX, adjY, tz};
            mover->needsRepath = true;
        }

        // Check if arrived at adjacent tile
        float goalX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == tz;

        // Final approach - move directly toward work location when close but path exhausted
        if (correctZ) TryFinalApproach(mover, goalX, goalY, adjX, adjY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress ramp removal
        job->progress += dt / REMOVE_RAMP_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Ramp removal complete!
            CompleteRemoveRampDesignation(tx, ty, tz, job->assignedMover);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Chop tree job driver: move to adjacent tile -> chop down tree
JobRunResult RunJob_Chop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;  // Reusing mine target fields
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHOP) {
        return JOBRUN_FAIL;
    }

    // Check if trunk still exists
    if (grid[tz][ty][tx] != CELL_TREE_TRUNK) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        // Use cached adjacent tile (set when job was created)
        int adjX = job->targetAdjX;
        int adjY = job->targetAdjY;

        // Set goal if not already moving there
        if (mover->goal.x != adjX || mover->goal.y != adjY || mover->goal.z != tz) {
            mover->goal = (Point){adjX, adjY, tz};
            mover->needsRepath = true;
        }

        // Check if arrived at adjacent tile
        float goalX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == tz;

        // Final approach - move directly toward work location when close but path exhausted
        if (correctZ) TryFinalApproach(mover, goalX, goalY, adjX, adjY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress tree chopping
        job->progress += dt / CHOP_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Chopping complete!
            CompleteChopDesignation(tx, ty, tz, job->assignedMover);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Plant sapling job driver: pick up sapling -> carry to designation -> plant
JobRunResult RunJob_PlantSapling(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;  // Reusing mine target fields for designation location
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_PLANT_SAPLING) {
        return JOBRUN_FAIL;
    }

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

        // Request repath if path exhausted and not at destination
        if (IsPathExhausted(mover) && distSq >= PICKUP_RADIUS * PICKUP_RADIUS) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }

        // Final approach - move directly toward item when close but path exhausted
        TryFinalApproach(mover, item->x, item->y, itemCellX, itemCellY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
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

            // Set goal to designation tile (it should be walkable - it's AIR)
            mover->goal = (Point){ tx, ty, tz };
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

        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;

        // Check if arrived at designation
        int moverCellX = (int)(mover->x / CELL_SIZE);
        int moverCellY = (int)(mover->y / CELL_SIZE);
        int moverCellZ = (int)mover->z;

        bool onTarget = (moverCellX == tx && moverCellY == ty && moverCellZ == tz);

        // If on target, advance to planting (check this BEFORE stuck detection)
        if (onTarget) {
            job->step = STEP_PLANTING;
            job->progress = 0.0f;
            return JOBRUN_RUNNING;
        }

        // Final approach
        if (IsPathExhausted(mover)) {
            float goalX = tx * CELL_SIZE + CELL_SIZE * 0.5f;
            float goalY = ty * CELL_SIZE + CELL_SIZE * 0.5f;
            TryFinalApproach(mover, goalX, goalY, tx, ty, PICKUP_RADIUS);
        }

        // Check if stuck (only if not on target)
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_PLANTING) {
        int itemIdx = job->carryingItem;

        // Progress planting
        job->progress += dt / PLANT_SAPLING_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Planting complete - place sapling cell
            PlaceSapling(tx, ty, tz);

            // Consume the sapling item
            if (itemIdx >= 0 && items[itemIdx].active) {
                items[itemIdx].active = false;
                items[itemIdx].reservedBy = -1;
            }
            job->carryingItem = -1;

            // Clear the designation
            CancelDesignation(tx, ty, tz);

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

        // Request repath if path exhausted and not at destination
        if (IsPathExhausted(mover) && distSq >= PICKUP_RADIUS * PICKUP_RADIUS) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }

        // Final approach - move directly toward item when close but path exhausted
        TryFinalApproach(mover, item->x, item->y, itemCellX, itemCellY, PICKUP_RADIUS);

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
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

            // Set goal to blueprint or adjacent cell if blueprint cell is not walkable
            Blueprint* bp = &blueprints[bpIdx];
            Point goalCell = { bp->x, bp->y, bp->z };

            if (!IsCellWalkableAt(bp->z, bp->y, bp->x)) {
                // Find adjacent walkable cell
                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; i++) {
                    int ax = bp->x + dx[i];
                    int ay = bp->y + dy[i];
                    if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
                        if (IsCellWalkableAt(bp->z, ay, ax)) {
                            goalCell = (Point){ ax, ay, bp->z };
                            break;
                        }
                    }
                }
            }

            mover->goal = goalCell;
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

        // Check if arrived at blueprint (on it or adjacent for building over air)
        int moverCellX = (int)(mover->x / CELL_SIZE);
        int moverCellY = (int)(mover->y / CELL_SIZE);
        int moverCellZ = (int)mover->z;

        bool onBlueprint = (moverCellX == bp->x && moverCellY == bp->y && moverCellZ == bp->z);
        bool adjacentToBlueprint = (moverCellZ == bp->z &&
            ((abs(moverCellX - bp->x) == 1 && moverCellY == bp->y) ||
             (abs(moverCellY - bp->y) == 1 && moverCellX == bp->x)));

        // Final approach - move toward goal when path exhausted but not at blueprint
        if (IsPathExhausted(mover) && !onBlueprint && !adjacentToBlueprint) {
            float goalX = mover->goal.x * CELL_SIZE + CELL_SIZE * 0.5f;
            float goalY = mover->goal.y * CELL_SIZE + CELL_SIZE * 0.5f;
            TryFinalApproach(mover, goalX, goalY, mover->goal.x, mover->goal.y, PICKUP_RADIUS);
        }

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }

        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;

        if (onBlueprint || adjacentToBlueprint) {
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
        // Goal was set by WorkGiver - could be blueprint cell or adjacent cell
        // Don't override it here

        // Check if arrived - either on the blueprint cell OR adjacent to it (for building over air)
        int moverCellX = (int)(mover->x / CELL_SIZE);
        int moverCellY = (int)(mover->y / CELL_SIZE);
        int moverCellZ = (int)mover->z;

        bool onBlueprint = (moverCellX == bp->x && moverCellY == bp->y && moverCellZ == bp->z);
        bool adjacentToBlueprint = (moverCellZ == bp->z &&
            ((abs(moverCellX - bp->x) == 1 && moverCellY == bp->y) ||
             (abs(moverCellY - bp->y) == 1 && moverCellX == bp->x)));

        // Final approach - move toward goal when path exhausted but not at blueprint
        if (IsPathExhausted(mover) && !onBlueprint && !adjacentToBlueprint) {
            float goalX = mover->goal.x * CELL_SIZE + CELL_SIZE * 0.5f;
            float goalY = mover->goal.y * CELL_SIZE + CELL_SIZE * 0.5f;
            TryFinalApproach(mover, goalX, goalY, mover->goal.x, mover->goal.y, PICKUP_RADIUS);
        }

        // Check if stuck
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }

        if (onBlueprint || adjacentToBlueprint) {
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

// Craft job driver: fetch item from stockpile/ground, carry to workshop, craft
JobRunResult RunJob_Craft(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = job->assignedMover;

    // Check if workshop still exists
    if (job->targetWorkshop < 0 || job->targetWorkshop >= MAX_WORKSHOPS) {
        return JOBRUN_FAIL;
    }
    Workshop* ws = &workshops[job->targetWorkshop];
    if (!ws->active) {
        return JOBRUN_FAIL;
    }

    // Check if bill still exists
    if (job->targetBillIdx < 0 || job->targetBillIdx >= ws->billCount) {
        return JOBRUN_FAIL;
    }
    Bill* bill = &ws->bills[job->targetBillIdx];

    // Get recipe
    int recipeCount;
    Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) {
        return JOBRUN_FAIL;
    }
    Recipe* recipe = &recipes[bill->recipeIdx];

    switch (job->step) {
        case CRAFT_STEP_MOVING_TO_INPUT: {
            int itemIdx = job->targetItem;

            // Check if item still exists and is reserved by us
            if (itemIdx < 0 || !items[itemIdx].active) {
                return JOBRUN_FAIL;
            }
            Item* item = &items[itemIdx];
            if (item->reservedBy != moverIdx) {
                return JOBRUN_FAIL;
            }

            int itemCellX = (int)(item->x / CELL_SIZE);
            int itemCellY = (int)(item->y / CELL_SIZE);
            int itemCellZ = (int)(item->z);

            // Set goal to item
            if (mover->goal.x != itemCellX || mover->goal.y != itemCellY || mover->goal.z != itemCellZ) {
                mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
                mover->needsRepath = true;
            }

            float dx = mover->x - item->x;
            float dy = mover->y - item->y;
            float distSq = dx*dx + dy*dy;

            // Final approach - move directly toward item when close but path exhausted
            TryFinalApproach(mover, item->x, item->y, itemCellX, itemCellY, PICKUP_RADIUS);

            // Check if stuck
            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
                return JOBRUN_FAIL;
            }

            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                job->step = CRAFT_STEP_PICKING_UP;
            }
            break;
        }

        case CRAFT_STEP_PICKING_UP: {
            int itemIdx = job->targetItem;
            if (itemIdx < 0 || !items[itemIdx].active) {
                return JOBRUN_FAIL;
            }
            Item* item = &items[itemIdx];

            // Pick up the item
            if (item->state == ITEM_IN_STOCKPILE) {
                // Find which stockpile it's in and clear the slot
                for (int s = 0; s < MAX_STOCKPILES; s++) {
                    Stockpile* sp = &stockpiles[s];
                    if (!sp->active) continue;

                    int localX = (int)(item->x / CELL_SIZE) - sp->x;
                    int localY = (int)(item->y / CELL_SIZE) - sp->y;
                    if (localX >= 0 && localX < sp->width &&
                        localY >= 0 && localY < sp->height) {
                        int slotIdx = localY * sp->width + localX;
                        if (sp->slotCounts[slotIdx] > 0) {
                            sp->slotCounts[slotIdx]--;
                            if (sp->slotCounts[slotIdx] == 0) {
                                sp->slotTypes[slotIdx] = -1;
                                sp->slots[slotIdx] = -1;
                            }
                        }
                        break;
                    }
                }
            }

            item->state = ITEM_CARRIED;
            job->carryingItem = itemIdx;
            job->targetItem = -1;
            job->step = CRAFT_STEP_MOVING_TO_WORKSHOP;
            break;
        }

        case CRAFT_STEP_MOVING_TO_WORKSHOP: {
            // Walk to workshop work tile
            if (mover->goal.x != ws->workTileX || mover->goal.y != ws->workTileY || mover->goal.z != ws->z) {
                mover->goal = (Point){ws->workTileX, ws->workTileY, ws->z};
                mover->needsRepath = true;
            }

            float targetX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx = mover->x - targetX;
            float dy = mover->y - targetY;
            float distSq = dx*dx + dy*dy;

            // Final approach - move directly toward workshop when close but path exhausted
            TryFinalApproach(mover, targetX, targetY, ws->workTileX, ws->workTileY, PICKUP_RADIUS);

            // Check if stuck
            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                return JOBRUN_FAIL;
            }

            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                job->step = CRAFT_STEP_WORKING;
                job->progress = 0.0f;
                job->workRequired = recipe->workRequired;
            }
            break;
        }

        case CRAFT_STEP_WORKING: {
            // Progress crafting
            job->progress += dt / job->workRequired;

            if (job->progress >= 1.0f) {
                // Crafting complete!

                // Consume carried item
                if (job->carryingItem >= 0 && items[job->carryingItem].active) {
                    items[job->carryingItem].active = false;
                    itemCount--;
                }
                job->carryingItem = -1;

                // Spawn output items at workshop output tile
                float outX = ws->outputTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                float outY = ws->outputTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                for (int i = 0; i < recipe->outputCount; i++) {
                    SpawnItem(outX, outY, (float)ws->z, recipe->outputType);
                }

                // Update bill progress
                bill->completedCount++;

                // Release workshop
                ws->assignedCrafter = -1;

                return JOBRUN_DONE;
            }
            break;
        }

        default:
            return JOBRUN_FAIL;
    }

    return JOBRUN_RUNNING;
}

// Driver lookup table
static JobDriver jobDrivers[] = {
    [JOBTYPE_NONE] = NULL,
    [JOBTYPE_HAUL] = RunJob_Haul,
    [JOBTYPE_CLEAR] = RunJob_Clear,
    [JOBTYPE_MINE] = RunJob_Mine,
    [JOBTYPE_CHANNEL] = RunJob_Channel,
    [JOBTYPE_REMOVE_FLOOR] = RunJob_RemoveFloor,
    [JOBTYPE_HAUL_TO_BLUEPRINT] = RunJob_HaulToBlueprint,
    [JOBTYPE_BUILD] = RunJob_Build,
    [JOBTYPE_CRAFT] = RunJob_Craft,
    [JOBTYPE_REMOVE_RAMP] = RunJob_RemoveRamp,
    [JOBTYPE_CHOP] = RunJob_Chop,
    [JOBTYPE_PLANT_SAPLING] = RunJob_PlantSapling,
};

// Forward declaration for CancelJob (defined later in file)
static void CancelJob(Mover* m, int moverIdx);

// Tick function - runs job drivers for active jobs
void JobsTick(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) {
            // Inactive mover - cancel any job they have to release reservations
            if (m->currentJobId >= 0) {
                CancelJob(m, i);
            }
            continue;
        }
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

        JobRunResult result = driver(job, m, gameDeltaTime);

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
    if (!idleMoverList || !moverIsInIdleList) {
        TraceLog(LOG_ERROR, "Failed to allocate job system memory");
        return;
    }
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
            // Skip movers stuck in unwalkable cells (e.g. built themselves into a wall)
            int mx = (int)(movers[i].x / CELL_SIZE);
            int my = (int)(movers[i].y / CELL_SIZE);
            if (!IsCellWalkableAt((int)movers[i].z, my, mx)) continue;

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

        // Release mine designation reservation
        if (job->targetMineX >= 0 && job->targetMineY >= 0 && job->targetMineZ >= 0) {
            Designation* d = GetDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
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

        // Release workshop reservation (for craft jobs)
        if (job->targetWorkshop >= 0 && job->targetWorkshop < MAX_WORKSHOPS) {
            Workshop* ws = &workshops[job->targetWorkshop];
            if (ws->active && ws->assignedCrafter == moverIdx) {
                ws->assignedCrafter = -1;
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
// AssignJobs - Best of both: item-centric for hauling, mover-centric for sparse
// =============================================================================
//
// The key insight: hauling dominates job count (hundreds of items) while
// mining/building targets are sparse (tens of designations/blueprints).
//
// - Item-centric for hauling: O(items) - iterate items, find nearest mover
// - Mover-centric for sparse: O(movers × targets) - but targets << items
//
// This achieves similar performance to Legacy while using WorkGiver functions.

void AssignJobs(void) {
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
    RebuildStockpileSlotCache();  // O(1) FindStockpileForItem lookups

    // Check which item types have available stockpiles (from cache)
    bool typeHasStockpile[ITEM_TYPE_COUNT] = {false};
    bool anyTypeHasSlot = false;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        if (stockpileSlotCache[t].stockpileIdx >= 0) {
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
            spIdx = FindStockpileForItemCached(items[itemIdx].type, &slotX, &slotY);
            if (spIdx < 0) safeDrop = true;
        }

        if (!TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, safeDrop)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        } else if (!safeDrop) {
            // Slot was reserved, invalidate cache for this type
            InvalidateStockpileSlotCache(items[itemIdx].type);
        }
    }

    // =========================================================================
    // PRIORITY 2: Crafting - before hauling so crafters can claim materials
    // =========================================================================
    if (idleMoverCount > 0) {
        // Check for workshops with runnable bills
        bool hasWorkshopWork = false;
        for (int w = 0; w < MAX_WORKSHOPS && !hasWorkshopWork; w++) {
            Workshop* ws = &workshops[w];
            if (!ws->active) continue;
            if (ws->assignedCrafter >= 0) continue;
            if (ws->billCount > 0) hasWorkshopWork = true;
        }

        if (hasWorkshopWork) {
            // Copy idle list since WorkGiver modifies it
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            if (idleCopy) {
                int idleCopyCount = idleMoverCount;
                memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));

                for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                    int moverIdx = idleCopy[i];
                    if (!moverIsInIdleList[moverIdx]) continue;
                    WorkGiver_Craft(moverIdx);
                }
                free(idleCopy);
            }
        }
    }

    // =========================================================================
    // PRIORITY 3a: Stockpile-centric hauling - search items near each stockpile
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
                int spIdx = FindStockpileForItemCached(item->type, &slotX, &slotY);
                if (spIdx < 0) continue;

                if (TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, false)) {
                    InvalidateStockpileSlotCache(item->type);
                }
            }
        } else {
            // Fallback: linear scan
            for (int j = 0; j < itemHighWaterMark && idleMoverCount > 0; j++) {
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
                int spIdx = FindStockpileForItemCached(item->type, &slotX, &slotY);
                if (spIdx < 0) continue;

                if (TryAssignItemToMover(j, spIdx, slotX, slotY, false)) {
                    InvalidateStockpileSlotCache(item->type);
                }
            }
        }
    }

    // =========================================================================
    // PRIORITY 3: Re-haul from overfull/low-priority stockpiles - ITEM-CENTRIC
    // =========================================================================
    if (idleMoverCount > 0) {
        for (int j = 0; j < itemHighWaterMark && idleMoverCount > 0; j++) {
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
                destSp = FindStockpileForItemCached(items[j].type, &destSlotX, &destSlotY);
            } else if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
            } else {
                destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
            }

            if (destSp < 0) continue;

            if (TryAssignItemToMover(j, destSp, destSlotX, destSlotY, false)) {
                if (noLongerAllowed) {
                    InvalidateStockpileSlotCache(items[j].type);
                }
            }
        }
    }

    // =========================================================================
    // PRIORITY 4-6: Mining, BlueprintHaul, Build - MOVER-CENTRIC
    // These have sparse targets so mover-centric is acceptable
    // Skip entirely if no sparse targets exist (performance optimization)
    // =========================================================================
    if (idleMoverCount > 0) {
        // Build designation caches ONCE (replaces grid scan per mover)
        RebuildMineDesignationCache();
        RebuildChannelDesignationCache();
        RebuildRemoveFloorDesignationCache();
        RebuildRemoveRampDesignationCache();

        bool hasMineWork = (mineCacheCount > 0);
        bool hasChannelWork = (channelCacheCount > 0);
        bool hasRemoveFloorWork = (removeFloorCacheCount > 0);
        bool hasRemoveRampWork = (removeRampCacheCount > 0);
        bool hasChopWork = (CountChopDesignations() > 0);
        bool hasPlantSaplingWork = (CountPlantSaplingDesignations() > 0);

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
        if (hasMineWork || hasChannelWork || hasRemoveFloorWork || hasRemoveRampWork || hasChopWork || hasPlantSaplingWork || hasBlueprintWork) {
            // Copy idle list since WorkGivers modify it
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            if (!idleCopy) return;
            int idleCopyCount = idleMoverCount;
            memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));

            for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                int moverIdx = idleCopy[i];

                // Skip if already assigned by hauling above
                if (!moverIsInIdleList[moverIdx]) continue;

                // Try sparse-target WorkGivers only (skip hauling WorkGivers)
                int jobId = -1;
                if (hasMineWork) {
                    jobId = WorkGiver_Mining(moverIdx);
                }

                // Channel (after mining - similar priority)
                if (jobId < 0 && hasChannelWork) {
                    jobId = WorkGiver_Channel(moverIdx);
                }

                // Remove floor (after channel - lower priority)
                if (jobId < 0 && hasRemoveFloorWork) {
                    jobId = WorkGiver_RemoveFloor(moverIdx);
                }

                // Remove ramp (after remove floor - lower priority)
                if (jobId < 0 && hasRemoveRampWork) {
                    jobId = WorkGiver_RemoveRamp(moverIdx);
                }

                // Chop trees (after remove ramp)
                if (jobId < 0 && hasChopWork) {
                    jobId = WorkGiver_Chop(moverIdx);
                }

                // Plant saplings (after chop)
                if (jobId < 0 && hasPlantSaplingWork) {
                    jobId = WorkGiver_PlantSapling(moverIdx);
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
        for (int j = 0; j < itemHighWaterMark; j++) {
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

// WorkGiver_PlantSapling: Find a plant sapling designation and a sapling item to plant
// Returns job ID if successful, -1 if no job available
int WorkGiver_PlantSapling(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability
    if (!m->capabilities.canPlant) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned plant sapling designation
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDesigDistSq = 1e30f;

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = &designations[z][y][x];
                if (d->type != DESIGNATION_PLANT_SAPLING) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;
                if (z != moverZ) continue;  // Same z-level for now

                // Check if designation tile is walkable (should be AIR)
                if (!IsCellWalkableAt(z, y, x)) continue;

                // Distance to designation
                float tileX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float tileY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                float distX = tileX - m->x;
                float distY = tileY - m->y;
                float distSq = distX * distX + distY * distY;

                if (distSq < bestDesigDistSq) {
                    bestDesigDistSq = distSq;
                    bestDesigX = x;
                    bestDesigY = y;
                    bestDesigZ = z;
                }
            }
        }
    }

    if (bestDesigX < 0) return -1;

    // Find nearest available sapling item
    int bestItemIdx = -1;
    float bestItemDistSq = 1e30f;

    for (int j = 0; j < itemHighWaterMark; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (item->type != ITEM_SAPLING) continue;
        if (item->reservedBy != -1) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        if ((int)item->z != moverZ) continue;

        float dx = item->x - m->x;
        float dy = item->y - m->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestItemDistSq) {
            bestItemDistSq = distSq;
            bestItemIdx = j;
        }
    }

    if (bestItemIdx < 0) return -1;

    // Check reachability to item
    Item* item = &items[bestItemIdx];
    int itemCellX = (int)(item->x / CELL_SIZE);
    int itemCellY = (int)(item->y / CELL_SIZE);
    int itemCellZ = (int)item->z;
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point itemCell = { itemCellX, itemCellY, itemCellZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        SetItemUnreachableCooldown(bestItemIdx, UNREACHABLE_COOLDOWN);
        return -1;
    }

    // Check reachability to designation
    Point desigCell = { bestDesigX, bestDesigY, bestDesigZ };
    tempLen = FindPath(moverPathAlgorithm, itemCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Reserve the item
    ReserveItem(bestItemIdx, moverIdx);

    // Reserve the designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Create job
    int jobId = CreateJob(JOBTYPE_PLANT_SAPLING);
    if (jobId < 0) {
        ReleaseItemReservation(bestItemIdx);
        d->assignedMover = -1;
        return -1;
    }

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = bestItemIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = STEP_MOVING_TO_PICKUP;
    job->progress = 0.0f;

    // Update mover
    m->currentJobId = jobId;
    m->goal = (Point){ itemCellX, itemCellY, itemCellZ };
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_Craft: Find workshop with runnable bill and available materials
// Returns job ID if successful, -1 if no job available
int WorkGiver_Craft(int moverIdx) {
    Mover* m = &movers[moverIdx];
    int moverZ = (int)m->z;

    // Note: no canCraft capability check for now - any mover can craft

    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->z != moverZ) continue;  // Same z-level
        if (ws->assignedCrafter >= 0) continue;  // Already has crafter

        // Find first non-suspended bill that can run
        for (int b = 0; b < ws->billCount; b++) {
            Bill* bill = &ws->bills[b];

            // Auto-resume bills that were suspended due to no storage
            if (bill->suspended && bill->suspendedNoStorage) {
                // Check if storage is now available
                int recipeCount;
                Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
                if (bill->recipeIdx >= 0 && bill->recipeIdx < recipeCount) {
                    int outSlotX, outSlotY;
                    if (FindStockpileForItem(recipes[bill->recipeIdx].outputType, &outSlotX, &outSlotY) >= 0) {
                        bill->suspended = false;
                        bill->suspendedNoStorage = false;
                    }
                }
            }

            if (bill->suspended) continue;
            if (!ShouldBillRun(ws, bill)) continue;

            // Get recipe for this bill
            int recipeCount;
            Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
            if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) continue;
            Recipe* recipe = &recipes[bill->recipeIdx];

            // Check if there's stockpile space for the output
            // If not, auto-suspend the bill to prevent items piling up
            int outSlotX, outSlotY;
            if (FindStockpileForItem(recipe->outputType, &outSlotX, &outSlotY) < 0) {
                bill->suspended = true;
                bill->suspendedNoStorage = true;  // Mark why it was suspended
                continue;
            }

            // Find an input item (search nearby or all if radius = 0)
            int searchRadius = bill->ingredientSearchRadius;
            if (searchRadius == 0) searchRadius = 100;  // Large default

            int itemIdx = -1;
            int bestDistSq = searchRadius * searchRadius;

            // Search for closest unreserved item of the required type
            for (int i = 0; i < itemHighWaterMark; i++) {
                Item* item = &items[i];
                if (!item->active) continue;
                if (item->type != recipe->inputType) continue;
                if (item->reservedBy != -1) continue;
                if (item->unreachableCooldown > 0.0f) continue;
                if ((int)item->z != ws->z) continue;

                // Check distance from workshop
                int itemTileX = (int)(item->x / CELL_SIZE);
                int itemTileY = (int)(item->y / CELL_SIZE);
                int dx = itemTileX - ws->x;
                int dy = itemTileY - ws->y;
                int distSq = dx * dx + dy * dy;
                if (distSq > bestDistSq) continue;

                bestDistSq = distSq;
                itemIdx = i;
            }

            if (itemIdx < 0) continue;  // No materials available

            Item* item = &items[itemIdx];

            // Check if mover can reach the item
            Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
            Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };

            Point tempPath[MAX_PATH];
            int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
            if (tempLen == 0) {
                SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
                continue;
            }

            // Check if mover can reach the workshop work tile
            Point workCell = { ws->workTileX, ws->workTileY, ws->z };
            tempLen = FindPath(moverPathAlgorithm, itemCell, workCell, tempPath, MAX_PATH);
            if (tempLen == 0) continue;

            // Reserve item and workshop
            item->reservedBy = moverIdx;
            ws->assignedCrafter = moverIdx;

            // Create job
            int jobId = CreateJob(JOBTYPE_CRAFT);
            if (jobId < 0) {
                item->reservedBy = -1;
                ws->assignedCrafter = -1;
                return -1;
            }

            Job* job = GetJob(jobId);
            job->assignedMover = moverIdx;
            job->targetWorkshop = w;
            job->targetBillIdx = b;
            job->targetItem = itemIdx;
            job->step = CRAFT_STEP_MOVING_TO_INPUT;
            job->progress = 0.0f;
            job->carryingItem = -1;

            // Update mover
            m->currentJobId = jobId;
            m->goal = itemCell;
            m->needsRepath = true;

            RemoveMoverFromIdleList(moverIdx);

            return jobId;
        }
    }

    return -1;
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

    for (int j = 0; j < itemHighWaterMark; j++) {
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

// WorkGiver_Mining: Find a mine designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_Mining(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability
    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned mine designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < mineCacheCount; i++) {
        AdjacentDesignationEntry* entry = &mineCache[i];

        // Only same z-level for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned and not marked unreachable this frame
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        float minePosX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float minePosY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = minePosX - m->x;
        float dy = minePosY - m->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
            bestAdjX = entry->adjX;
            bestAdjY = entry->adjY;
        }
    }

    if (bestDesigX < 0) return -1;

    // Check reachability - try all adjacent walkable tiles until we find one with a valid path
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    if (!FindReachableAdjacentTile(bestDesigX, bestDesigY, bestDesigZ, moverCell, &bestAdjX, &bestAdjY)) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_MINE);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->targetAdjX = bestAdjX;  // Cache adjacent tile
    job->targetAdjY = bestAdjY;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Update mover
    m->currentJobId = jobId;
    m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_Channel: Find a channel designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_Channel(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability - channeling uses the same skill as mining
    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned channel designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < channelCacheCount; i++) {
        OnTileDesignationEntry* entry = &channelCache[i];

        // Same z-level only for now (mover walks to the tile)
        if (entry->z != moverZ) continue;

        // Check if still unassigned and not marked unreachable this frame
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Distance to the tile itself (mover stands on it)
        float tileX = entry->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = tileX - m->x;
        float dy = tileY - m->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
        }
    }

    if (bestDesigX < 0) return -1;

    // Check reachability - mover walks TO the tile (not adjacent)
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point targetCell = { bestDesigX, bestDesigY, bestDesigZ };

    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, targetCell, tempPath, MAX_PATH);

    if (tempLen == 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_CHANNEL);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;  // Reusing mine target fields
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Update mover
    m->currentJobId = jobId;
    m->goal = targetCell;  // Walk to the tile itself
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_RemoveFloor: Find a remove floor designation to work on
// Returns job ID if successful, -1 if no job available
// Priority: below mining/channeling
int WorkGiver_RemoveFloor(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability - uses mining skill
    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned remove floor designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < removeFloorCacheCount; i++) {
        OnTileDesignationEntry* entry = &removeFloorCache[i];

        // Same z-level only for now (mover walks to the tile)
        if (entry->z != moverZ) continue;

        // Check if still unassigned and not marked unreachable this frame
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Distance to the tile itself (mover stands on it)
        float tileX = entry->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = tileX - m->x;
        float dy = tileY - m->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
        }
    }

    if (bestDesigX < 0) return -1;

    // Check reachability - mover walks TO the tile
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point targetCell = { bestDesigX, bestDesigY, bestDesigZ };

    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, targetCell, tempPath, MAX_PATH);

    if (tempLen == 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_REMOVE_FLOOR);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;  // Reusing mine target fields
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Update mover
    m->currentJobId = jobId;
    m->goal = targetCell;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_RemoveRamp: Find a remove ramp designation to work on
// Returns job ID if successful, -1 if no job available
// Priority: below remove floor
int WorkGiver_RemoveRamp(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability - uses mining skill
    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned remove ramp designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < removeRampCacheCount; i++) {
        AdjacentDesignationEntry* entry = &removeRampCache[i];

        // Same z-level only for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned and not marked unreachable this frame
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Distance to adjacent tile (pre-computed in cache)
        float tileX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = tileX - m->x;
        float dy = tileY - m->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
            bestAdjX = entry->adjX;
            bestAdjY = entry->adjY;
        }
    }

    if (bestDesigX < 0) return -1;

    // Check reachability - try all adjacent walkable tiles until we find one with a valid path
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    if (!FindReachableAdjacentTile(bestDesigX, bestDesigY, bestDesigZ, moverCell, &bestAdjX, &bestAdjY)) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_REMOVE_RAMP);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;  // Reusing mine target fields
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->targetAdjX = bestAdjX;  // Cache adjacent tile
    job->targetAdjY = bestAdjY;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Update mover
    m->currentJobId = jobId;
    m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_Chop: Find a tree chop designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_Chop(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability - uses mining skill for chopping
    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned chop designation
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    // Simple search - no cache for chop designations (they're rare)
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = &designations[z][y][x];
                if (d->type != DESIGNATION_CHOP) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;
                if (z != moverZ) continue;  // Same z-level for now

                // Find an adjacent walkable tile
                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; i++) {
                    int adjX = x + dx[i];
                    int adjY = y + dy[i];
                    if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
                    if (!IsCellWalkableAt(z, adjY, adjX)) continue;

                    // Distance to adjacent tile
                    float tileX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
                    float tileY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
                    float distX = tileX - m->x;
                    float distY = tileY - m->y;
                    float distSq = distX * distX + distY * distY;

                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        bestDesigX = x;
                        bestDesigY = y;
                        bestDesigZ = z;
                        bestAdjX = adjX;
                        bestAdjY = adjY;
                    }
                    break;  // Found adjacent tile, no need to check others
                }
            }
        }
    }

    if (bestDesigX < 0) return -1;

    // Check reachability
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    if (!FindReachableAdjacentTile(bestDesigX, bestDesigY, bestDesigZ, moverCell, &bestAdjX, &bestAdjY)) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_CHOP);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->targetAdjX = bestAdjX;
    job->targetAdjY = bestAdjY;
    job->step = 0;  // STEP_MOVING_TO_WORK
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Update mover
    m->currentJobId = jobId;
    m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
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

    // Check reachability - either to the cell itself, or to an adjacent cell if target is not walkable
    Point bpCell = { bp->x, bp->y, bp->z };
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point goalCell = bpCell;

    Point tempPath[MAX_PATH];
    int tempLen = 0;

    if (IsCellWalkableAt(bp->z, bp->y, bp->x)) {
        // Blueprint cell is walkable - path directly to it (e.g. building wall on ground)
        tempLen = FindPath(moverPathAlgorithm, moverCell, bpCell, tempPath, MAX_PATH);
    } else {
        // Blueprint cell is not walkable (e.g. floor over air) - find adjacent walkable cell
        // Check orthogonal neighbors only (no diagonals, like Dwarf Fortress)
        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};
        for (int i = 0; i < 4; i++) {
            int ax = bp->x + dx[i];
            int ay = bp->y + dy[i];
            if (ax < 0 || ax >= gridWidth || ay < 0 || ay >= gridHeight) continue;
            if (!IsCellWalkableAt(bp->z, ay, ax)) continue;

            Point adjCell = { ax, ay, bp->z };
            tempLen = FindPath(moverPathAlgorithm, moverCell, adjCell, tempPath, MAX_PATH);
            if (tempLen > 0) {
                goalCell = adjCell;
                break;
            }
        }
    }

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
    m->goal = goalCell;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_BlueprintHaul: Find material to haul to a blueprint
// Returns job ID if successful, -1 if no job available
// Filter for blueprint haul items (building materials only)
static bool BlueprintHaulItemFilter(int itemIdx, void* userData) {
    (void)userData;
    Item* item = &items[itemIdx];

    if (!item->active) return false;
    if (!ItemIsBuildingMat(item->type)) return false;  // Only building materials
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

    // First check if any blueprint needs materials (check same z OR reachable via adjacent cell)
    bool anyBlueprintNeedsMaterials = false;
    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;
        if (bp->reservedItem >= 0) continue;
        // Check if blueprint cell or adjacent cell might be reachable
        // (detailed path check done later, this is just early bail-out)
        if (bp->z != moverZ) {
            // Different z-level - check if any adjacent cell on bp's z is walkable
            // (could be reachable via ladder)
            bool hasWalkableAdjacent = false;
            int dx[] = {1, -1, 0, 0};
            int dy[] = {0, 0, 1, -1};
            for (int i = 0; i < 4; i++) {
                int ax = bp->x + dx[i];
                int ay = bp->y + dy[i];
                if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
                    if (IsCellWalkableAt(bp->z, ay, ax)) {
                        hasWalkableAdjacent = true;
                        break;
                    }
                }
            }
            if (!hasWalkableAdjacent && !IsCellWalkableAt(bp->z, bp->y, bp->x)) continue;
        }
        anyBlueprintNeedsMaterials = true;
        break;
    }
    if (!anyBlueprintNeedsMaterials) return -1;

    int moverTileX = (int)(m->x / CELL_SIZE);
    int moverTileY = (int)(m->y / CELL_SIZE);

    // Find nearest building material
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

    // Linear scan for all building materials (fallback for tests, plus checks stockpile items)
    for (int j = 0; j < itemHighWaterMark; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (!ItemIsBuildingMat(item->type)) continue;
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

    // Now find any blueprint that needs materials - check reachability via pathfinding
    int bestBpIdx = -1;
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point tempPath[MAX_PATH];

    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;
        if (bp->reservedItem >= 0) continue;

        // Try to find a reachable cell: either the blueprint cell itself or an adjacent one
        int tempLen = 0;

        if (IsCellWalkableAt(bp->z, bp->y, bp->x)) {
            // Blueprint cell is walkable - try pathing to it
            Point bpCell = { bp->x, bp->y, bp->z };
            tempLen = FindPath(moverPathAlgorithm, moverCell, bpCell, tempPath, MAX_PATH);
        }

        if (tempLen == 0) {
            // Blueprint cell not walkable or not reachable - try adjacent cells
            int dx[] = {1, -1, 0, 0};
            int dy[] = {0, 0, 1, -1};
            for (int i = 0; i < 4; i++) {
                int ax = bp->x + dx[i];
                int ay = bp->y + dy[i];
                if (ax < 0 || ax >= gridWidth || ay < 0 || ay >= gridHeight) continue;
                if (!IsCellWalkableAt(bp->z, ay, ax)) continue;

                Point adjCell = { ax, ay, bp->z };
                tempLen = FindPath(moverPathAlgorithm, moverCell, adjCell, tempPath, MAX_PATH);
                if (tempLen > 0) {
                    break;  // Found a reachable adjacent cell
                }
            }
        }

        if (tempLen > 0) {
            bestBpIdx = bpIdx;
            break;  // Take first reachable
        }
    }

    if (bestBpIdx < 0) return -1;

    Item* item = &items[bestItemIdx];
    Blueprint* bp = &blueprints[bestBpIdx];

    // Check reachability to item
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };
    int itemPathLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    if (itemPathLen == 0) return -1;

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
