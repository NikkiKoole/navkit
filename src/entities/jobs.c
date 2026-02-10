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
#include "../simulation/trees.h"
#include "../../shared/profiler.h"
#include "../../shared/ui.h"
#include "../../vendor/raylib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static inline uint8_t ResolveItemMaterialForJobs(const Item* item) {
    if (!item) return MAT_NONE;
    uint8_t mat = (item->material == MAT_NONE) ? DefaultMaterialForItemType(item->type) : item->material;
    return (mat < MAT_COUNT) ? mat : MAT_NONE;
}

static inline bool ItemTypeIsValidForJobs(ItemType type) {
    return type >= 0 && type < ITEM_TYPE_COUNT;
}

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

// Dig ramp designation cache (miner stands adjacent to wall being carved)
static AdjacentDesignationEntry digRampCache[MAX_DESIGNATION_CACHE];
static int digRampCacheCount = 0;

// Tree work caches (chop, chop felled, gather sapling, plant sapling)
static AdjacentDesignationEntry chopCache[MAX_DESIGNATION_CACHE];
static int chopCacheCount = 0;

static AdjacentDesignationEntry chopFelledCache[MAX_DESIGNATION_CACHE];
static int chopFelledCacheCount = 0;

static AdjacentDesignationEntry gatherSaplingCache[MAX_DESIGNATION_CACHE];
static int gatherSaplingCacheCount = 0;

static OnTileDesignationEntry plantSaplingCache[MAX_DESIGNATION_CACHE];
static int plantSaplingCacheCount = 0;

static OnTileDesignationEntry gatherGrassCache[MAX_DESIGNATION_CACHE];
static int gatherGrassCacheCount = 0;

// Dirty flags - mark caches that need rebuilding
static bool mineCacheDirty = true;
static bool channelCacheDirty = true;
static bool digRampCacheDirty = true;
static bool removeFloorCacheDirty = true;
static bool removeRampCacheDirty = true;
static bool chopCacheDirty = true;
static bool chopFelledCacheDirty = true;
static bool gatherSaplingCacheDirty = true;
static bool plantSaplingCacheDirty = true;
static bool gatherGrassCacheDirty = true;

// Forward declarations for designation cache rebuild functions
static void RebuildChannelDesignationCache(void);
static void RebuildRemoveFloorDesignationCache(void);
static void RebuildRemoveRampDesignationCache(void);
static void RebuildDigRampDesignationCache(void);
static void RebuildChopDesignationCache(void);
static void RebuildChopFelledDesignationCache(void);
static void RebuildGatherSaplingDesignationCache(void);
static void RebuildPlantSaplingDesignationCache(void);
static void RebuildGatherGrassDesignationCache(void);

// Designation Job Specification - consolidates designation type with job handling
typedef struct {
    DesignationType desigType;
    JobType jobType;
    void (*RebuildCache)(void);
    int (*WorkGiver)(int moverIdx);
    void* cacheArray;     // pointer to the static cache array (AdjacentDesignationEntry[] or OnTileDesignationEntry[])
    int* cacheCount;      // pointer to the static count
    bool* cacheDirty;     // pointer to the static dirty flag
} DesignationJobSpec;

// Table of all designation types (defines coordination between designation, job, cache, and workgiver)
static DesignationJobSpec designationSpecs[] = {
    {DESIGNATION_MINE, JOBTYPE_MINE, RebuildMineDesignationCache, WorkGiver_Mining, 
     mineCache, &mineCacheCount, &mineCacheDirty},
    {DESIGNATION_CHANNEL, JOBTYPE_CHANNEL, RebuildChannelDesignationCache, WorkGiver_Channel,
     channelCache, &channelCacheCount, &channelCacheDirty},
    {DESIGNATION_DIG_RAMP, JOBTYPE_DIG_RAMP, RebuildDigRampDesignationCache, WorkGiver_DigRamp,
     digRampCache, &digRampCacheCount, &digRampCacheDirty},
    {DESIGNATION_REMOVE_FLOOR, JOBTYPE_REMOVE_FLOOR, RebuildRemoveFloorDesignationCache, WorkGiver_RemoveFloor,
     removeFloorCache, &removeFloorCacheCount, &removeFloorCacheDirty},
    {DESIGNATION_REMOVE_RAMP, JOBTYPE_REMOVE_RAMP, RebuildRemoveRampDesignationCache, WorkGiver_RemoveRamp,
     removeRampCache, &removeRampCacheCount, &removeRampCacheDirty},
    {DESIGNATION_CHOP, JOBTYPE_CHOP, RebuildChopDesignationCache, WorkGiver_Chop,
     chopCache, &chopCacheCount, &chopCacheDirty},
    {DESIGNATION_CHOP_FELLED, JOBTYPE_CHOP_FELLED, RebuildChopFelledDesignationCache, WorkGiver_ChopFelled,
     chopFelledCache, &chopFelledCacheCount, &chopFelledCacheDirty},
    {DESIGNATION_GATHER_SAPLING, JOBTYPE_GATHER_SAPLING, RebuildGatherSaplingDesignationCache, WorkGiver_GatherSapling,
     gatherSaplingCache, &gatherSaplingCacheCount, &gatherSaplingCacheDirty},
    {DESIGNATION_PLANT_SAPLING, JOBTYPE_PLANT_SAPLING, RebuildPlantSaplingDesignationCache, WorkGiver_PlantSapling,
     plantSaplingCache, &plantSaplingCacheCount, &plantSaplingCacheDirty},
    {DESIGNATION_GATHER_GRASS, JOBTYPE_GATHER_GRASS, RebuildGatherGrassDesignationCache, WorkGiver_GatherGrass,
     gatherGrassCache, &gatherGrassCacheCount, &gatherGrassCacheDirty},
};

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
    if (!mineCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_MINE, mineCache, &mineCacheCount);
    mineCacheDirty = false;
}

static void RebuildChannelDesignationCache(void) {
    if (!channelCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_CHANNEL, channelCache, &channelCacheCount);
    channelCacheDirty = false;
}

static void RebuildRemoveFloorDesignationCache(void) {
    if (!removeFloorCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_REMOVE_FLOOR, removeFloorCache, &removeFloorCacheCount);
    removeFloorCacheDirty = false;
}

static void RebuildRemoveRampDesignationCache(void) {
    if (!removeRampCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_REMOVE_RAMP, removeRampCache, &removeRampCacheCount);
    removeRampCacheDirty = false;
}

static void RebuildDigRampDesignationCache(void) {
    if (!digRampCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_DIG_RAMP, digRampCache, &digRampCacheCount);
    digRampCacheDirty = false;
}

static void RebuildChopDesignationCache(void) {
    if (!chopCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_CHOP, chopCache, &chopCacheCount);
    chopCacheDirty = false;
}

static void RebuildChopFelledDesignationCache(void) {
    if (!chopFelledCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_CHOP_FELLED, chopFelledCache, &chopFelledCacheCount);
    chopFelledCacheDirty = false;
}

static void RebuildGatherSaplingDesignationCache(void) {
    if (!gatherSaplingCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_GATHER_SAPLING, gatherSaplingCache, &gatherSaplingCacheCount);
    gatherSaplingCacheDirty = false;
}

static void RebuildPlantSaplingDesignationCache(void) {
    if (!plantSaplingCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_PLANT_SAPLING, plantSaplingCache, &plantSaplingCacheCount);
    plantSaplingCacheDirty = false;
}

static void RebuildGatherGrassDesignationCache(void) {
    if (!gatherGrassCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_GATHER_GRASS, gatherGrassCache, &gatherGrassCacheCount);
    gatherGrassCacheDirty = false;
}

// Invalidate designation caches - call when designations are added/removed/completed
void InvalidateDesignationCache(DesignationType type) {
    for (int i = 0; i < (int)(sizeof(designationSpecs) / sizeof(designationSpecs[0])); i++) {
        if (designationSpecs[i].desigType == type) {
            *designationSpecs[i].cacheDirty = true;
            break;
        }
    }
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
    job->targetAdjX = -1;
    job->targetAdjY = -1;
    job->targetWorkshop = -1;
    job->targetBillIdx = -1;
    job->workRequired = 0.0f;
    job->progress = 0.0f;
    job->carryingItem = -1;
    job->fuelItem = -1;
    job->targetItem2 = -1;

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
                RemoveItemFromStockpileSlot(item->x, item->y, (int)item->z);
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
        if (!StockpileAcceptsItem(job->targetStockpile, items[itemIdx].type, items[itemIdx].material)) {
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
    CellType ct = grid[job->targetMineZ][job->targetMineY][job->targetMineX];
    if (!CellIsSolid(ct)) {
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

// Dig ramp job driver: move adjacent to wall -> carve into ramp
JobRunResult RunJob_DigRamp(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;  // Reusing mine target fields
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_DIG_RAMP) {
        return JOBRUN_FAIL;
    }

    // Check if the wall still exists
    CellType ct = grid[tz][ty][tx];
    if (!CellIsSolid(ct)) {
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

        // Final approach
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
        // Progress digging
        job->progress += dt / DIG_RAMP_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Dig ramp complete!
            CompleteDigRampDesignation(tx, ty, tz, job->assignedMover);
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

// Chop felled trunk job driver: move to adjacent tile -> chop up fallen trunk
JobRunResult RunJob_ChopFelled(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHOP_FELLED) {
        return JOBRUN_FAIL;
    }

    if (grid[tz][ty][tx] != CELL_TREE_FELLED) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        int adjX = job->targetAdjX;
        int adjY = job->targetAdjY;

        if (mover->goal.x != adjX || mover->goal.y != adjY || mover->goal.z != tz) {
            mover->goal = (Point){adjX, adjY, tz};
            mover->needsRepath = true;
        }

        float goalX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == tz;

        if (correctZ) TryFinalApproach(mover, goalX, goalY, adjX, adjY, PICKUP_RADIUS);

        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }

        return JOBRUN_RUNNING;
    } else if (job->step == STEP_WORKING) {
        job->progress += dt / CHOP_FELLED_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            CompleteChopFelledDesignation(tx, ty, tz, job->assignedMover);
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
            if (itemIdx >= 0 && items[itemIdx].active) {
                MaterialType treeMat = TreeTypeFromSaplingItem(items[itemIdx].type);
                PlaceSapling(tx, ty, tz, treeMat);
            } else {
                return JOBRUN_FAIL;
            }

            // Consume the sapling item
            if (itemIdx >= 0 && items[itemIdx].active) {
                DeleteItem(itemIdx);
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

// Gather sapling job driver: move to sapling cell -> dig up -> creates item
JobRunResult RunJob_GatherSapling(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_GATHER_SAPLING) {
        return JOBRUN_FAIL;
    }

    // Check if sapling cell still exists
    if (grid[tz][ty][tx] != CELL_SAPLING) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
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

        // Final approach
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
        // Progress gathering
        job->progress += dt / GATHER_SAPLING_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            // Gathering complete - convert sapling cell to item
            CompleteGatherSaplingDesignation(tx, ty, tz, job->assignedMover);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Gather grass job driver: walk to grass cell, work, spawn item
JobRunResult RunJob_GatherGrass(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    int tx = job->targetMineX;
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_GATHER_GRASS) {
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_WORK) {
        // Set goal to the tile itself (it's walkable)
        if (mover->goal.x != tx || mover->goal.y != ty || mover->goal.z != tz) {
            mover->goal = (Point){tx, ty, tz};
            mover->needsRepath = true;
        }

        float goalX = tx * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = ty * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == tz;

        if (correctZ) TryFinalApproach(mover, goalX, goalY, tx, ty, PICKUP_RADIUS);

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
        job->progress += dt / GATHER_GRASS_WORK_TIME;
        d->progress = job->progress;

        if (job->progress >= 1.0f) {
            CompleteGatherGrassDesignation(tx, ty, tz, job->assignedMover);
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
                // Clear source stockpile slot when picking up
                RemoveItemFromStockpileSlot(item->x, item->y, (int)item->z);
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

            // Update carried item position to follow mover
            if (job->carryingItem >= 0 && items[job->carryingItem].active) {
                items[job->carryingItem].x = mover->x;
                items[job->carryingItem].y = mover->y;
                items[job->carryingItem].z = mover->z;
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
                // Deposit first input at workshop work tile
                if (job->carryingItem >= 0 && items[job->carryingItem].active) {
                    Item* carried = &items[job->carryingItem];
                    carried->state = ITEM_ON_GROUND;
                    carried->x = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                    carried->y = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                    carried->z = (float)ws->z;
                    // Keep it reserved so nobody else grabs it
                }
                // Remember deposited input in targetItem so WORKING step can consume it
                job->targetItem = job->carryingItem;
                job->carryingItem = -1;

                // Check if we need to fetch a second input
                if (recipe->inputType2 != ITEM_NONE && job->targetItem2 >= 0) {
                    job->step = CRAFT_STEP_MOVING_TO_INPUT2;
                }
                // Otherwise, check if recipe needs fuel
                else if (recipe->fuelRequired > 0 && job->fuelItem >= 0) {
                    job->step = CRAFT_STEP_MOVING_TO_FUEL;
                } else {
                    job->step = CRAFT_STEP_WORKING;
                    job->progress = 0.0f;
                    job->workRequired = recipe->workRequired;
                    mover->timeWithoutProgress = 0.0f;
                }
            }
            break;
        }

        case CRAFT_STEP_MOVING_TO_INPUT2: {
            // Walk to second input item
            int item2Idx = job->targetItem2;
            if (item2Idx < 0 || !items[item2Idx].active) {
                return JOBRUN_FAIL;
            }
            Item* item2 = &items[item2Idx];
            if (item2->reservedBy != moverIdx) {
                return JOBRUN_FAIL;
            }

            int item2CellX = (int)(item2->x / CELL_SIZE);
            int item2CellY = (int)(item2->y / CELL_SIZE);
            int item2CellZ = (int)(item2->z);

            if (mover->goal.x != item2CellX || mover->goal.y != item2CellY || mover->goal.z != item2CellZ) {
                mover->goal = (Point){item2CellX, item2CellY, item2CellZ};
                mover->needsRepath = true;
            }

            float dx = mover->x - item2->x;
            float dy = mover->y - item2->y;
            float distSq2 = dx*dx + dy*dy;

            TryFinalApproach(mover, item2->x, item2->y, item2CellX, item2CellY, PICKUP_RADIUS);

            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                SetItemUnreachableCooldown(item2Idx, UNREACHABLE_COOLDOWN);
                return JOBRUN_FAIL;
            }

            if (distSq2 < PICKUP_RADIUS * PICKUP_RADIUS) {
                job->step = CRAFT_STEP_PICKING_UP_INPUT2;
            }
            break;
        }

        case CRAFT_STEP_PICKING_UP_INPUT2: {
            int item2Idx = job->targetItem2;
            if (item2Idx < 0 || !items[item2Idx].active) {
                return JOBRUN_FAIL;
            }
            Item* item2 = &items[item2Idx];

            // Pick up from stockpile if needed
            if (item2->state == ITEM_IN_STOCKPILE) {
                RemoveItemFromStockpileSlot(item2->x, item2->y, (int)item2->z);
            }

            item2->state = ITEM_CARRIED;
            job->carryingItem = item2Idx;
            job->targetItem2 = -1;  // Clear target now that we're carrying
            job->step = CRAFT_STEP_CARRYING_INPUT2;
            break;
        }

        case CRAFT_STEP_CARRYING_INPUT2: {
            // Walk back to workshop work tile
            if (mover->goal.x != ws->workTileX || mover->goal.y != ws->workTileY || mover->goal.z != ws->z) {
                mover->goal = (Point){ws->workTileX, ws->workTileY, ws->z};
                mover->needsRepath = true;
            }

            // Update carried item position
            if (job->carryingItem >= 0 && items[job->carryingItem].active) {
                items[job->carryingItem].x = mover->x;
                items[job->carryingItem].y = mover->y;
                items[job->carryingItem].z = mover->z;
            }

            float targetX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx = mover->x - targetX;
            float dy = mover->y - targetY;
            float distSq2 = dx*dx + dy*dy;

            TryFinalApproach(mover, targetX, targetY, ws->workTileX, ws->workTileY, PICKUP_RADIUS);

            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                return JOBRUN_FAIL;
            }

            if (distSq2 < PICKUP_RADIUS * PICKUP_RADIUS) {
                // Deposit second input at workshop
                if (job->carryingItem >= 0 && items[job->carryingItem].active) {
                    Item* carried = &items[job->carryingItem];
                    carried->state = ITEM_ON_GROUND;
                    carried->x = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                    carried->y = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                    carried->z = (float)ws->z;
                }
                // Store second input in targetItem2 for consumption
                job->targetItem2 = job->carryingItem;
                job->carryingItem = -1;

                // Check if we need fuel
                if (recipe->fuelRequired > 0 && job->fuelItem >= 0) {
                    job->step = CRAFT_STEP_MOVING_TO_FUEL;
                } else {
                    job->step = CRAFT_STEP_WORKING;
                    job->progress = 0.0f;
                    job->workRequired = recipe->workRequired;
                    mover->timeWithoutProgress = 0.0f;
                }
            }
            break;
        }

        case CRAFT_STEP_MOVING_TO_FUEL: {
            // Walk to fuel item
            int fuelIdx = job->fuelItem;
            if (fuelIdx < 0 || !items[fuelIdx].active) {
                return JOBRUN_FAIL;
            }
            Item* fuelItem = &items[fuelIdx];
            if (fuelItem->reservedBy != moverIdx) {
                return JOBRUN_FAIL;
            }

            int fuelCellX = (int)(fuelItem->x / CELL_SIZE);
            int fuelCellY = (int)(fuelItem->y / CELL_SIZE);
            int fuelCellZ = (int)(fuelItem->z);

            if (mover->goal.x != fuelCellX || mover->goal.y != fuelCellY || mover->goal.z != fuelCellZ) {
                mover->goal = (Point){fuelCellX, fuelCellY, fuelCellZ};
                mover->needsRepath = true;
            }

            float dx = mover->x - fuelItem->x;
            float dy = mover->y - fuelItem->y;
            float distSq = dx*dx + dy*dy;

            TryFinalApproach(mover, fuelItem->x, fuelItem->y, fuelCellX, fuelCellY, PICKUP_RADIUS);

            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                SetItemUnreachableCooldown(fuelIdx, UNREACHABLE_COOLDOWN);
                return JOBRUN_FAIL;
            }

            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                job->step = CRAFT_STEP_PICKING_UP_FUEL;
            }
            break;
        }

        case CRAFT_STEP_PICKING_UP_FUEL: {
            int fuelIdx = job->fuelItem;
            if (fuelIdx < 0 || !items[fuelIdx].active) {
                return JOBRUN_FAIL;
            }
            Item* fuelItem = &items[fuelIdx];

            // Pick up from stockpile if needed
            if (fuelItem->state == ITEM_IN_STOCKPILE) {
                RemoveItemFromStockpileSlot(fuelItem->x, fuelItem->y, (int)fuelItem->z);
            }

            fuelItem->state = ITEM_CARRIED;
            job->step = CRAFT_STEP_CARRYING_FUEL;
            break;
        }

        case CRAFT_STEP_CARRYING_FUEL: {
            // Walk back to workshop work tile
            if (mover->goal.x != ws->workTileX || mover->goal.y != ws->workTileY || mover->goal.z != ws->z) {
                mover->goal = (Point){ws->workTileX, ws->workTileY, ws->z};
                mover->needsRepath = true;
            }

            // Update carried fuel item position to follow mover
            if (job->fuelItem >= 0 && items[job->fuelItem].active) {
                items[job->fuelItem].x = mover->x;
                items[job->fuelItem].y = mover->y;
                items[job->fuelItem].z = mover->z;
            }

            float targetX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx = mover->x - targetX;
            float dy = mover->y - targetY;
            float distSq = dx*dx + dy*dy;

            TryFinalApproach(mover, targetX, targetY, ws->workTileX, ws->workTileY, PICKUP_RADIUS);

            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                return JOBRUN_FAIL;
            }

            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                job->step = CRAFT_STEP_WORKING;
                job->progress = 0.0f;
                job->workRequired = recipe->workRequired;
                mover->timeWithoutProgress = 0.0f;
            }
            break;
        }

        case CRAFT_STEP_WORKING: {
            // Progress crafting — mover is stationary but making progress
            job->progress += dt / job->workRequired;
            mover->timeWithoutProgress = 0.0f;
            ws->lastWorkTime = (float)gameTime;

            // Fire-based workshops emit smoke while working
            bool emitsSmoke = (ws->type == WORKSHOP_KILN ||
                               ws->type == WORKSHOP_CHARCOAL_PIT ||
                               ws->type == WORKSHOP_HEARTH);
            if (emitsSmoke && ws->fuelTileX >= 0) {
                if (GetRandomValue(0, 3) == 0) {
                    AddSmoke(ws->fuelTileX, ws->fuelTileY, ws->z, 5);
                }
            }

            if (job->progress >= 1.0f) {
                // Crafting complete!

                // Find the input item - either still carried (no-fuel path) or deposited (fuel path, stored in targetItem)
                int inputItemIdx = job->carryingItem >= 0 ? job->carryingItem : job->targetItem;
                MaterialType inputMat = MAT_NONE;
                if (inputItemIdx >= 0 && items[inputItemIdx].active) {
                    inputMat = (MaterialType)items[inputItemIdx].material;
                    if (inputMat == MAT_NONE) {
                        inputMat = (MaterialType)DefaultMaterialForItemType(items[inputItemIdx].type);
                    }
                }

                // Consume first input item
                if (inputItemIdx >= 0 && items[inputItemIdx].active) {
                    DeleteItem(inputItemIdx);
                }
                job->carryingItem = -1;
                job->targetItem = -1;

                // Consume second input item (if any)
                if (job->targetItem2 >= 0 && items[job->targetItem2].active) {
                    DeleteItem(job->targetItem2);
                }
                job->targetItem2 = -1;

                // Consume fuel item (if any - don't preserve its material)
                if (job->fuelItem >= 0 && items[job->fuelItem].active) {
                    DeleteItem(job->fuelItem);
                }
                job->fuelItem = -1;

                // Spawn output items at workshop output tile
                float outX = ws->outputTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                float outY = ws->outputTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                for (int i = 0; i < recipe->outputCount; i++) {
                    // Preserve input material for items like planks/blocks (pine log -> pine planks)
                    // Use default material for transformed items like bricks/charcoal
                    uint8_t outMat;
                    if (ItemTypeUsesMaterialName(recipe->outputType) && inputMat != MAT_NONE) {
                        outMat = (uint8_t)inputMat;
                    } else {
                        outMat = DefaultMaterialForItemType(recipe->outputType);
                    }
                    SpawnItemWithMaterial(outX, outY, (float)ws->z, recipe->outputType, outMat);
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

// Deliver-to-workshop job driver: pick up item -> carry to workshop work tile -> drop
JobRunResult RunJob_DeliverToWorkshop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    (void)dt;

    // Validate workshop still exists
    if (job->targetWorkshop < 0 || job->targetWorkshop >= MAX_WORKSHOPS ||
        !workshops[job->targetWorkshop].active) {
        return JOBRUN_FAIL;
    }
    Workshop* ws = &workshops[job->targetWorkshop];

    if (job->step == STEP_MOVING_TO_PICKUP) {
        int itemIdx = job->targetItem;

        if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;

        Item* item = &items[itemIdx];
        int itemCellX = (int)(item->x / CELL_SIZE);
        int itemCellY = (int)(item->y / CELL_SIZE);
        int itemCellZ = (int)item->z;
        if (!IsCellWalkableAt(itemCellZ, itemCellY, itemCellX)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }

        float dx = mover->x - item->x;
        float dy = mover->y - item->y;
        float distSq = dx*dx + dy*dy;

        if (IsPathExhausted(mover) && distSq >= PICKUP_RADIUS * PICKUP_RADIUS) {
            mover->goal = (Point){itemCellX, itemCellY, itemCellZ};
            mover->needsRepath = true;
        }

        TryFinalApproach(mover, item->x, item->y, itemCellX, itemCellY, PICKUP_RADIUS);

        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }

        if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            if (item->state == ITEM_IN_STOCKPILE) {
                RemoveItemFromStockpileSlot(item->x, item->y, (int)item->z);
            }
            item->state = ITEM_CARRIED;
            job->carryingItem = itemIdx;
            job->targetItem = -1;
            job->step = STEP_CARRYING;

            mover->goal = (Point){ws->workTileX, ws->workTileY, ws->z};
            mover->needsRepath = true;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_CARRYING) {
        int itemIdx = job->carryingItem;
        if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;

        float targetX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float targetY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - targetX;
        float dy = mover->y - targetY;
        float distSq = dx*dx + dy*dy;

        if (IsPathExhausted(mover) && distSq >= DROP_RADIUS * DROP_RADIUS) {
            mover->goal = (Point){ws->workTileX, ws->workTileY, ws->z};
            mover->needsRepath = true;
        }

        TryFinalApproach(mover, targetX, targetY, ws->workTileX, ws->workTileY, DROP_RADIUS);

        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }

        // Update carried item position
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;

        if (distSq < DROP_RADIUS * DROP_RADIUS) {
            Item* item = &items[itemIdx];
            item->state = ITEM_ON_GROUND;
            item->x = targetX;
            item->y = targetY;
            item->z = ws->z;
            item->reservedBy = -1;
            job->carryingItem = -1;
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Ignite workshop job driver: walk to workshop work tile, do short active work, set passiveReady
JobRunResult RunJob_IgniteWorkshop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;

    if (job->targetWorkshop < 0 || job->targetWorkshop >= MAX_WORKSHOPS ||
        !workshops[job->targetWorkshop].active) {
        return JOBRUN_FAIL;
    }
    Workshop* ws = &workshops[job->targetWorkshop];

    if (job->step == STEP_MOVING_TO_WORK) {
        if (mover->goal.x != ws->workTileX || mover->goal.y != ws->workTileY || mover->goal.z != ws->z) {
            mover->goal = (Point){ws->workTileX, ws->workTileY, ws->z};
            mover->needsRepath = true;
        }

        float goalX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;

        bool correctZ = (int)mover->z == ws->z;
        if (correctZ) TryFinalApproach(mover, goalX, goalY, ws->workTileX, ws->workTileY, PICKUP_RADIUS);

        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }

        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
            job->progress = 0.0f;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        job->progress += dt / job->workRequired;
        mover->timeWithoutProgress = 0.0f;

        if (job->progress >= 1.0f) {
            ws->passiveReady = true;
            ws->assignedCrafter = -1;
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
    [JOBTYPE_MINE] = RunJob_Mine,
    [JOBTYPE_CHANNEL] = RunJob_Channel,
    [JOBTYPE_DIG_RAMP] = RunJob_DigRamp,
    [JOBTYPE_REMOVE_FLOOR] = RunJob_RemoveFloor,
    [JOBTYPE_HAUL_TO_BLUEPRINT] = RunJob_HaulToBlueprint,
    [JOBTYPE_BUILD] = RunJob_Build,
    [JOBTYPE_CRAFT] = RunJob_Craft,
    [JOBTYPE_REMOVE_RAMP] = RunJob_RemoveRamp,
    [JOBTYPE_CHOP] = RunJob_Chop,
    [JOBTYPE_GATHER_SAPLING] = RunJob_GatherSapling,
    [JOBTYPE_PLANT_SAPLING] = RunJob_PlantSapling,
    [JOBTYPE_CHOP_FELLED] = RunJob_ChopFelled,
    [JOBTYPE_GATHER_GRASS] = RunJob_GatherGrass,
    [JOBTYPE_DELIVER_TO_WORKSHOP] = RunJob_DeliverToWorkshop,
    [JOBTYPE_IGNITE_WORKSHOP] = RunJob_IgniteWorkshop,
};

// Compile-time check: ensure jobDrivers[] covers all job types
_Static_assert(sizeof(jobDrivers) / sizeof(jobDrivers[0]) > JOBTYPE_IGNITE_WORKSHOP,
               "jobDrivers[] must have an entry for every JobType");

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
            // Record diagnostics before releasing
            m->lastJobType = job->type;
            m->lastJobResult = 0;  // DONE
            m->lastJobTargetX = job->targetMineX;
            m->lastJobTargetY = job->targetMineY;
            m->lastJobTargetZ = job->targetMineZ;
            m->lastJobEndTick = currentTick;
            // Job completed successfully - release job and return mover to idle
            ReleaseJob(m->currentJobId);
            m->currentJobId = -1;
            m->needsRepath = false;
            m->timeWithoutProgress = 0.0f;
            AddMoverToIdleList(i);
        }
        else if (result == JOBRUN_FAIL) {
            // Record diagnostics before cancelling
            m->lastJobType = job->type;
            m->lastJobResult = 1;  // FAIL
            m->lastJobTargetX = job->targetMineX;
            m->lastJobTargetY = job->targetMineY;
            m->lastJobTargetZ = job->targetMineZ;
            m->lastJobEndTick = currentTick;
            // Job failed - cancel releases all reservations
            CancelJob(m, i);
        }
        // JOBRUN_RUNNING - continue next tick
    }

    PassiveWorkshopsTick(gameDeltaTime);
    UpdateWorkshopDiagnostics(gameDeltaTime);
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
    int itemZ;              // Z-level of item — skip movers on different z-levels
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

    // Skip movers on different z-level (prevents cross-z-level cooldown poisoning)
    Mover* m = &movers[moverIdx];
    if (ctx->itemZ >= 0 && (int)m->z != ctx->itemZ) return;

    // Check capabilities
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
    RemoveItemFromStockpileSlot(item->x, item->y, (int)item->z);
}

// Helper: safe-drop an item near a mover at a walkable cell
// Searches 8 neighbors (cardinal + diagonal) if mover position is not walkable
static void SafeDropItem(int itemIdx, Mover* m) {
    if (itemIdx < 0 || !items[itemIdx].active) return;
    
    Item* item = &items[itemIdx];
    item->state = ITEM_ON_GROUND;
    item->reservedBy = -1;

    int cellX = (int)(m->x / CELL_SIZE);
    int cellY = (int)(m->y / CELL_SIZE);
    int cellZ = (int)m->z;

    if (IsCellWalkableAt(cellZ, cellY, cellX)) {
        // Drop at mover position
        item->x = m->x;
        item->y = m->y;
        item->z = m->z;
    } else {
        // Find nearest walkable neighbor (cardinal + diagonal)
        int dx[] = {0, 0, -1, 1, -1, 1, -1, 1};
        int dy[] = {-1, 1, 0, 0, -1, -1, 1, 1};
        bool found = false;
        for (int d = 0; d < 8; d++) {
            int nx = cellX + dx[d];
            int ny = cellY + dy[d];
            if (IsCellWalkableAt(cellZ, ny, nx)) {
                item->x = nx * CELL_SIZE + CELL_SIZE / 2.0f;
                item->y = ny * CELL_SIZE + CELL_SIZE / 2.0f;
                item->z = cellZ;
                found = true;
                break;
            }
        }
        if (!found) {
            // Last resort: drop at mover position anyway
            item->x = m->x;
            item->y = m->y;
            item->z = m->z;
        }
    }
}

// Cancel job and release all reservations (public, called from workshops.c and internally)
void CancelJob(void* moverPtr, int moverIdx) {
    Mover* m = (Mover*)moverPtr;
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

        // If carrying, safe-drop the item at a walkable cell
        SafeDropItem(job->carryingItem, m);

        // Release mine designation reservation
        if (job->targetMineX >= 0 && job->targetMineY >= 0 && job->targetMineZ >= 0) {
            Designation* d = GetDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
            if (d && d->assignedMover == moverIdx) {
                d->assignedMover = -1;
                d->progress = 0.0f;  // Reset progress when cancelled
                InvalidateDesignationCache(d->type);
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

        // Release second input item reservation (for multi-input craft jobs)
        if (job->targetItem2 >= 0 && items[job->targetItem2].active) {
            // Only safe-drop if carried; otherwise just release reservation
            if (items[job->targetItem2].state == ITEM_CARRIED) {
                SafeDropItem(job->targetItem2, m);
            } else {
                items[job->targetItem2].reservedBy = -1;
            }
        }

        // Release fuel item reservation (for craft jobs with fuel)
        if (job->fuelItem >= 0 && items[job->fuelItem].active) {
            // Only safe-drop if carried; otherwise just release reservation
            if (items[job->fuelItem].state == ITEM_CARRIED) {
                SafeDropItem(job->fuelItem, m);
            } else {
                items[job->fuelItem].reservedBy = -1;
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
    ClearMoverPath(moverIdx);
    m->needsRepath = false;
    m->timeWithoutProgress = 0.0f;

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
            .itemZ = (int)item->z,
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
        int itemZ = (int)item->z;
        for (int i = 0; i < idleMoverCount; i++) {
            int idx = idleMoverList[i];
            // Skip movers on different z-level
            if ((int)movers[idx].z != itemZ) continue;
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
        if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx, item->type, item->material)) {
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

// Core haulability check — single source of truth for all haul paths.
// Call sites add their own context-specific checks (z-level, type/material matching) after.
static bool IsItemHaulable(Item* item, int itemIdx) {
    (void)itemIdx;
    if (!item->active) return false;
    if (item->reservedBy != -1) return false;
    if (item->state != ITEM_ON_GROUND) return false;
    if (item->unreachableCooldown > 0.0f) return false;
    if (!ItemTypeIsValidForJobs(item->type)) return false;
    if (!IsItemInGatherZone(item->x, item->y, (int)item->z)) return false;
    int cellX = (int)(item->x / CELL_SIZE);
    int cellY = (int)(item->y / CELL_SIZE);
    int cellZ = (int)(item->z);
    if (!IsCellWalkableAt(cellZ, cellY, cellX)) return false;
    if (IsPassiveWorkshopWorkTile(cellX, cellY, cellZ)) return false;
    return true;
}

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

    // Check which item types + materials have available stockpiles (from cache)
    bool typeMatHasStockpile[ITEM_TYPE_COUNT][MAT_COUNT] = {false};
    bool anyTypeHasSlot = false;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        for (int m = 0; m < MAT_COUNT; m++) {
            if (stockpileSlotCache[t][m].stockpileIdx >= 0) {
                typeMatHasStockpile[t][m] = true;
                anyTypeHasSlot = true;
            }
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
            // Try absorbing into the same slot (item matches stockpile filter)
            spIdx = spOnItem;
            slotX = (int)(items[itemIdx].x / CELL_SIZE);
            slotY = (int)(items[itemIdx].y / CELL_SIZE);

            // Check if slot is full — if so, treat as clear instead
            Stockpile* sp = &stockpiles[spOnItem];
            int lx = slotX - sp->x;
            int ly = slotY - sp->y;
            int idx = ly * sp->width + lx;
            if (sp->slotCounts[idx] + sp->reservedBy[idx] >= sp->maxStackSize) {
                // Slot full, find another stockpile
                absorb = false;
                spIdx = FindStockpileForItemCached(items[itemIdx].type, items[itemIdx].material, &slotX, &slotY);
                if (spIdx < 0) safeDrop = true;
            }
        } else {
            spIdx = FindStockpileForItemCached(items[itemIdx].type, items[itemIdx].material, &slotX, &slotY);
            if (spIdx < 0) safeDrop = true;
        }

        if (!TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, safeDrop)) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        } else if (!safeDrop) {
            // Slot was reserved, invalidate cache for this type
            InvalidateStockpileSlotCache(items[itemIdx].type, items[itemIdx].material);
        }
    }

    // =========================================================================
    // PRIORITY 2: Crafting - before hauling so crafters can claim materials
    // Optimization: Only check as many movers as we have workshops needing crafters
    // =========================================================================
    if (idleMoverCount > 0) {
        // Count workshops needing crafters
        int workshopsNeedingCrafters = 0;
        for (int w = 0; w < MAX_WORKSHOPS; w++) {
            Workshop* ws = &workshops[w];
            if (!ws->active) continue;
            if (ws->assignedCrafter >= 0) continue;
            if (ws->billCount > 0) workshopsNeedingCrafters++;
        }

        if (workshopsNeedingCrafters > 0) {
            // Only check this many movers (no point checking more)
            int moversToCheck = workshopsNeedingCrafters;
            if (moversToCheck > idleMoverCount) moversToCheck = idleMoverCount;

            // Copy idle list since WorkGiver modifies it
            int* idleCopy = (int*)malloc(moversToCheck * sizeof(int));
            if (idleCopy) {
                memcpy(idleCopy, idleMoverList, moversToCheck * sizeof(int));

                for (int i = 0; i < moversToCheck && idleMoverCount > 0; i++) {
                    int moverIdx = idleCopy[i];
                    if (!moverIsInIdleList[moverIdx]) continue;
                    WorkGiver_Craft(moverIdx);
                }
                free(idleCopy);
            }
        }
    }

    // =========================================================================
    // PRIORITY 2b: Passive workshop delivery - haulers deliver input items
    // =========================================================================
    if (idleMoverCount > 0) {
        // Check if any passive workshops need inputs
        bool anyPassiveNeedsInput = false;
        for (int w = 0; w < MAX_WORKSHOPS; w++) {
            Workshop* ws = &workshops[w];
            if (!ws->active) continue;
            if (!workshopDefs[ws->type].passive) continue;
            if (ws->billCount > 0) { anyPassiveNeedsInput = true; break; }
        }

        if (anyPassiveNeedsInput) {
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            if (idleCopy) {
                int idleCopyCount = idleMoverCount;
                memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));

                for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                    int moverIdx = idleCopy[i];
                    if (!moverIsInIdleList[moverIdx]) continue;
                    WorkGiver_DeliverToPassiveWorkshop(moverIdx);
                }
                free(idleCopy);
            }
        }
    }

    // =========================================================================
    // PRIORITY 2c: Semi-passive ignition - crafter activates workshops with inputs
    // =========================================================================
    if (idleMoverCount > 0) {
        bool anyNeedsIgnition = false;
        for (int w = 0; w < MAX_WORKSHOPS; w++) {
            Workshop* ws = &workshops[w];
            if (!ws->active) continue;
            if (!workshopDefs[ws->type].passive) continue;
            if (ws->passiveReady) continue;
            if (ws->assignedCrafter >= 0) continue;
            if (ws->billCount > 0) { anyNeedsIgnition = true; break; }
        }

        if (anyNeedsIgnition) {
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            if (idleCopy) {
                int idleCopyCount = idleMoverCount;
                memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));

                for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                    int moverIdx = idleCopy[i];
                    if (!moverIsInIdleList[moverIdx]) continue;
                    WorkGiver_IgniteWorkshop(moverIdx);
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

                for (int m = 0; m < MAT_COUNT && idleMoverCount > 0; m++) {
                    if (!typeMatHasStockpile[t][m]) continue;

                    // Find a free slot for this type + material in this stockpile
                    int slotX, slotY;
                    if (!FindFreeStockpileSlot(spIdx, (ItemType)t, (uint8_t)m, &slotX, &slotY)) continue;

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

                                    if (!IsItemHaulable(item, itemIdx)) continue;
                                    if (item->type != (ItemType)t) continue;
                                    if (ResolveItemMaterialForJobs(item) != (uint8_t)m) continue;

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

                if (!IsItemHaulable(item, itemIdx)) continue;
                uint8_t mat = ResolveItemMaterialForJobs(item);
                if (!typeMatHasStockpile[item->type][mat]) continue;

                int slotX, slotY;
                int spIdx = FindStockpileForItemCached(item->type, item->material, &slotX, &slotY);
                if (spIdx < 0) continue;

                TryAssignItemToMover(itemIdx, spIdx, slotX, slotY, false);
                InvalidateStockpileSlotCache(item->type, item->material);
            }
        } else {
            // Fallback: linear scan
            for (int j = 0; j < itemHighWaterMark && idleMoverCount > 0; j++) {
                Item* item = &items[j];

                if (!IsItemHaulable(item, j)) continue;
                uint8_t mat = ResolveItemMaterialForJobs(item);
                if (!typeMatHasStockpile[item->type][mat]) continue;

                int slotX, slotY;
                int spIdx = FindStockpileForItemCached(item->type, item->material, &slotX, &slotY);
                if (spIdx < 0) continue;

                TryAssignItemToMover(j, spIdx, slotX, slotY, false);
                InvalidateStockpileSlotCache(item->type, item->material);
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

            bool noLongerAllowed = !StockpileAcceptsItem(currentSp, items[j].type, items[j].material);

            if (noLongerAllowed) {
                destSp = FindStockpileForItemCached(items[j].type, items[j].material, &destSlotX, &destSlotY);
            } else if (IsSlotOverfull(currentSp, itemSlotX, itemSlotY)) {
                destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
            } else {
                destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
            }

            if (destSp < 0) continue;

            if (TryAssignItemToMover(j, destSp, destSlotX, destSlotY, false)) {
                if (noLongerAllowed) {
                    InvalidateStockpileSlotCache(items[j].type, items[j].material);
                }
            }
        }
    }

    // =========================================================================
    // PRIORITY 3c: Consolidate fragmented stacks - ITEM-CENTRIC
    // Move items from small partial stacks into larger ones of same type.
    // Limited to one consolidation per stockpile to avoid congestion.
    // =========================================================================
    if (idleMoverCount > 0) {
        for (int spIdx = 0; spIdx < MAX_STOCKPILES && idleMoverCount > 0; spIdx++) {
            if (!stockpiles[spIdx].active) continue;
            
            // Find one item to consolidate in this stockpile
            for (int j = 0; j < itemHighWaterMark; j++) {
                if (!items[j].active) continue;
                if (items[j].reservedBy != -1) continue;
                if (items[j].state != ITEM_IN_STOCKPILE) continue;
                
                int itemSp = -1;
                if (!IsPositionInStockpile(items[j].x, items[j].y, (int)items[j].z, &itemSp)) continue;
                if (itemSp != spIdx) continue;
                
                int itemSlotX = (int)(items[j].x / CELL_SIZE);
                int itemSlotY = (int)(items[j].y / CELL_SIZE);
                
                int destSlotX, destSlotY;
                if (FindConsolidationTarget(spIdx, itemSlotX, itemSlotY, &destSlotX, &destSlotY)) {
                    if (TryAssignItemToMover(j, spIdx, destSlotX, destSlotY, false)) {
                        break;  // One consolidation per stockpile
                    }
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
        // Rebuild all dirty designation caches (table-driven)
        int designationSpecCount = sizeof(designationSpecs) / sizeof(designationSpecs[0]);
        for (int i = 0; i < designationSpecCount; i++) {
            if (*designationSpecs[i].cacheDirty) {
                designationSpecs[i].RebuildCache();
            }
        }
        
        // Check if any designation work exists
        bool hasDesignationWork = false;
        for (int i = 0; i < designationSpecCount; i++) {
            if (*designationSpecs[i].cacheCount > 0) {
                hasDesignationWork = true;
                break;
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
        if (hasDesignationWork || hasBlueprintWork) {
            // Copy idle list since WorkGivers modify it
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            if (!idleCopy) return;
            int idleCopyCount = idleMoverCount;
            memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));

            for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                int moverIdx = idleCopy[i];

                // Skip if already assigned by hauling above
                if (!moverIsInIdleList[moverIdx]) continue;

                // Try designation WorkGivers in priority order (table order = priority)
                int jobId = -1;
                for (int j = 0; j < designationSpecCount && jobId < 0; j++) {
                    if (*designationSpecs[j].cacheCount > 0) {
                        jobId = designationSpecs[j].WorkGiver(moverIdx);
                    }
                }

                // Try blueprint work (haul materials or build)
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
// NOTE: Only used by tests. AssignJobs uses inline haul loops that call IsItemHaulable.
// Filter context for WorkGiver_Haul spatial search
typedef struct {
    bool (*typeMatHasStockpile)[MAT_COUNT];  // Pointer to 2D array
    int moverZ;                               // Z-level of mover — skip items on different z-levels
} HaulFilterContext;

static bool HaulItemFilter(int itemIdx, void* userData) {
    HaulFilterContext* ctx = (HaulFilterContext*)userData;
    Item* item = &items[itemIdx];
    if (!IsItemHaulable(item, itemIdx)) return false;
    if ((int)item->z != ctx->moverZ) return false;
    uint8_t mat = ResolveItemMaterialForJobs(item);
    if (!ctx->typeMatHasStockpile[item->type][mat]) return false;
    return true;
}

int WorkGiver_Haul(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability
    if (!m->capabilities.canHaul) return -1;

    // Cache which item types + materials have available stockpiles
    // (Use direct queries here so tests that call WorkGiver_Haul without
    // RebuildStockpileSlotCache still behave correctly.)
    bool typeMatHasStockpile[ITEM_TYPE_COUNT][MAT_COUNT] = {false};
    bool anyTypeHasSlot = false;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        for (int m = 0; m < MAT_COUNT; m++) {
            int slotX, slotY;
            if (FindStockpileForItem((ItemType)t, (uint8_t)m, &slotX, &slotY) >= 0) {
                typeMatHasStockpile[t][m] = true;
                anyTypeHasSlot = true;
            }
        }
    }

    if (!anyTypeHasSlot) {
        // Stockpile free slot counts can be stale when WorkGiver_Haul is called directly
        // (tests do this without the usual AssignJobs rebuild pass).
        RebuildStockpileFreeSlotCounts();
        memset(typeMatHasStockpile, 0, sizeof(typeMatHasStockpile));
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            for (int m = 0; m < MAT_COUNT; m++) {
                int slotX, slotY;
                if (FindStockpileForItem((ItemType)t, (uint8_t)m, &slotX, &slotY) >= 0) {
                    typeMatHasStockpile[t][m] = true;
                    anyTypeHasSlot = true;
                }
            }
        }
        if (!anyTypeHasSlot) return -1;
    }

    int moverTileX = (int)(m->x / CELL_SIZE);
    int moverTileY = (int)(m->y / CELL_SIZE);
    int moverZ = (int)m->z;

    int bestItemIdx = -1;

    // Use spatial grid with expanding radius if available
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        HaulFilterContext ctx = { .typeMatHasStockpile = typeMatHasStockpile, .moverZ = moverZ };

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
            if (!IsItemHaulable(item, j)) continue;
            if ((int)item->z != moverZ) continue;
            uint8_t mat = ResolveItemMaterialForJobs(item);
            if (!typeMatHasStockpile[item->type][mat]) continue;

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
    int spIdx = FindStockpileForItem(item->type, item->material, &slotX, &slotY);
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
    if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx, item->type, item->material)) {
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

// WorkGiver_DeliverToPassiveWorkshop: Find passive workshop needing input items
// Returns job ID if successful, -1 if no job available
int WorkGiver_DeliverToPassiveWorkshop(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canHaul) return -1;

    int moverZ = (int)m->z;

    // Scan passive workshops for one with a runnable bill that needs input
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->z != moverZ) continue;
        if (!workshopDefs[ws->type].passive) continue;
        if (ws->passiveReady) continue;  // Skip workshops that are already burning
        if (ws->billCount == 0) continue;

        // Find first runnable bill
        int billIdx = -1;
        for (int b = 0; b < ws->billCount; b++) {
            if (ShouldBillRun(ws, &ws->bills[b])) {
                billIdx = b;
                break;
            }
        }
        if (billIdx < 0) continue;

        const Recipe* recipe = &workshopDefs[ws->type].recipes[ws->bills[billIdx].recipeIdx];

        // Count items already on work tile + reserved for delivery to this workshop
        int inputOnTile = 0;
        for (int j = 0; j < itemHighWaterMark; j++) {
            Item* item = &items[j];
            if (!item->active) continue;
            if (!RecipeInputMatches(recipe, item)) continue;

            // Item already on work tile
            int ix = (int)(item->x / CELL_SIZE);
            int iy = (int)(item->y / CELL_SIZE);
            if (ix == ws->workTileX && iy == ws->workTileY && (int)item->z == ws->z &&
                item->state == ITEM_ON_GROUND) {
                inputOnTile++;
            }

            // Item reserved for delivery to this workshop (being fetched or carried)
            if (item->reservedBy >= 0 && item->reservedBy < moverCount) {
                Mover* carrier = &movers[item->reservedBy];
                if (carrier->currentJobId >= 0) {
                    Job* cjob = GetJob(carrier->currentJobId);
                    if (cjob && cjob->type == JOBTYPE_DELIVER_TO_WORKSHOP &&
                        cjob->targetWorkshop == w) {
                        inputOnTile++;
                    }
                }
            }
        }

        if (inputOnTile >= recipe->inputCount) continue;  // Already have enough


        // Find nearest matching unreserved item
        int bestItemIdx = -1;
        float bestDistSq = 1e30f;

        for (int j = 0; j < itemHighWaterMark; j++) {
            Item* item = &items[j];
            if (!item->active) continue;
            if (item->reservedBy != -1) continue;
            if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
            if (item->unreachableCooldown > 0.0f) continue;
            if ((int)item->z != moverZ) continue;
            if (!RecipeInputMatches(recipe, item)) continue;

            int cellX = (int)(item->x / CELL_SIZE);
            int cellY = (int)(item->y / CELL_SIZE);
            if (!IsCellWalkableAt((int)item->z, cellY, cellX)) continue;

            // Skip items on passive workshop work tiles (they're in use or waiting for ignition)
            if (IsPassiveWorkshopWorkTile(cellX, cellY, (int)item->z)) continue;

            // Skip items already on the work tile — they're already delivered
            if (cellX == ws->workTileX && cellY == ws->workTileY && (int)item->z == ws->z) continue;

            float dx = item->x - m->x;
            float dy = item->y - m->y;
            float distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestItemIdx = j;
            }
        }

        if (bestItemIdx < 0) continue;

        // Check reachability
        Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), moverZ };
        Point itemCell = { (int)(items[bestItemIdx].x / CELL_SIZE),
                           (int)(items[bestItemIdx].y / CELL_SIZE), moverZ };
        Point tempPath[MAX_PATH];
        int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
        if (tempLen == 0) {
            SetItemUnreachableCooldown(bestItemIdx, UNREACHABLE_COOLDOWN);
            continue;
        }

        // Reserve item
        if (!ReserveItem(bestItemIdx, moverIdx)) continue;

        // Create job
        int jobId = CreateJob(JOBTYPE_DELIVER_TO_WORKSHOP);
        if (jobId < 0) {
            ReleaseItemReservation(bestItemIdx);
            return -1;
        }

        Job* job = GetJob(jobId);
        job->assignedMover = moverIdx;
        job->targetItem = bestItemIdx;
        job->targetWorkshop = w;
        job->targetBillIdx = billIdx;
        job->step = STEP_MOVING_TO_PICKUP;

        m->currentJobId = jobId;
        m->goal = itemCell;
        m->needsRepath = true;

        RemoveMoverFromIdleList(moverIdx);

        return jobId;
    }

    return -1;
}

// WorkGiver_IgniteWorkshop: Find semi-passive workshop needing ignition
// Returns job ID if successful, -1 if no job available
int WorkGiver_IgniteWorkshop(int moverIdx) {
    Mover* m = &movers[moverIdx];
    int moverZ = (int)m->z;

    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->z != moverZ) continue;
        if (!workshopDefs[ws->type].passive) continue;
        if (ws->passiveReady) continue;           // Already ignited
        if (ws->assignedCrafter >= 0) continue;   // Someone already assigned
        if (ws->billCount == 0) continue;

        // Find first runnable bill
        int billIdx = -1;
        for (int b = 0; b < ws->billCount; b++) {
            if (ShouldBillRun(ws, &ws->bills[b])) {
                billIdx = b;
                break;
            }
        }
        if (billIdx < 0) continue;

        const Recipe* recipe = &workshopDefs[ws->type].recipes[ws->bills[billIdx].recipeIdx];

        // Only semi-passive recipes (with active work phase)
        if (recipe->workRequired <= 0) continue;

        // Check: are required inputs present on work tile?
        int inputCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            Item* item = &items[i];
            if (!item->active) continue;
            if (item->state != ITEM_ON_GROUND) continue;
            int tileX = (int)(item->x / CELL_SIZE);
            int tileY = (int)(item->y / CELL_SIZE);
            if (tileX != ws->workTileX || tileY != ws->workTileY) continue;
            if ((int)item->z != ws->z) continue;
            if (!RecipeInputMatches(recipe, item)) continue;
            inputCount++;
            if (inputCount >= recipe->inputCount) break;
        }
        if (inputCount < recipe->inputCount) continue;

        // Check reachability
        Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), moverZ };
        Point workCell = { ws->workTileX, ws->workTileY, ws->z };
        Point tempPath[MAX_PATH];
        int tempLen = FindPath(moverPathAlgorithm, moverCell, workCell, tempPath, MAX_PATH);
        if (tempLen == 0) continue;

        // Create job
        int jobId = CreateJob(JOBTYPE_IGNITE_WORKSHOP);
        if (jobId < 0) return -1;

        Job* job = GetJob(jobId);
        job->assignedMover = moverIdx;
        job->targetWorkshop = w;
        job->targetBillIdx = billIdx;
        job->step = STEP_MOVING_TO_WORK;
        job->progress = 0.0f;
        job->workRequired = recipe->workRequired;

        ws->assignedCrafter = moverIdx;

        m->currentJobId = jobId;
        m->goal = workCell;
        m->needsRepath = true;

        RemoveMoverFromIdleList(moverIdx);

        return jobId;
    }

    return -1;
}

// WorkGiver_GatherSapling: Find a gather sapling designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_GatherSapling(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability - uses plant skill for gathering too
    if (!m->capabilities.canPlant) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned gather sapling designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < gatherSaplingCacheCount; i++) {
        AdjacentDesignationEntry* entry = &gatherSaplingCache[i];

        // Only same z-level for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned and correct type
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_GATHER_SAPLING || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Check if sapling cell still exists
        if (grid[entry->z][entry->y][entry->x] != CELL_SAPLING) continue;

        // Distance to adjacent tile (already cached)
        float adjPosX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float adjPosY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float distX = adjPosX - m->x;
        float distY = adjPosY - m->y;
        float distSq = distX * distX + distY * distY;

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

    // Check reachability
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    if (!FindReachableAdjacentTile(bestDesigX, bestDesigY, bestDesigZ, moverCell, &bestAdjX, &bestAdjY)) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_GATHER_SAPLING);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->targetAdjX = bestAdjX;
    job->targetAdjY = bestAdjY;
    job->step = STEP_MOVING_TO_WORK;
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

// WorkGiver_PlantSapling: Find a plant sapling designation and a sapling item to plant
// Returns job ID if successful, -1 if no job available
int WorkGiver_PlantSapling(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability
    if (!m->capabilities.canPlant) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned plant sapling designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDesigDistSq = 1e30f;

    for (int i = 0; i < plantSaplingCacheCount; i++) {
        OnTileDesignationEntry* entry = &plantSaplingCache[i];

        // Only same z-level for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned and correct type
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_PLANT_SAPLING || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Distance to designation
        float tileX = entry->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float distX = tileX - m->x;
        float distY = tileY - m->y;
        float distSq = distX * distX + distY * distY;

        if (distSq < bestDesigDistSq) {
            bestDesigDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
        }
    }

    if (bestDesigX < 0) return -1;

    // Find nearest available sapling item
    int bestItemIdx = -1;
    float bestItemDistSq = 1e30f;

    for (int j = 0; j < itemHighWaterMark; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (!IsSaplingItem(item->type)) continue;
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

// WorkGiver_GatherGrass: Find a grass gathering designation to work on
int WorkGiver_GatherGrass(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canPlant) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned gather grass designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < gatherGrassCacheCount; i++) {
        OnTileDesignationEntry* entry = &gatherGrassCache[i];

        if (entry->z != moverZ) continue;

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_GATHER_GRASS || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        float tileX = entry->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float distX = tileX - m->x;
        float distY = tileY - m->y;
        float distSq = distX * distX + distY * distY;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
        }
    }

    if (bestDesigX < 0) return -1;

    // Check reachability
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestDesigX, bestDesigY, bestDesigZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_GATHER_GRASS);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Update mover
    m->currentJobId = jobId;
    m->goal = (Point){ bestDesigX, bestDesigY, bestDesigZ };
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

    // Early exit: check if any workshops need crafters on this z-level
    bool anyAvailable = false;
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->z != moverZ) continue;
        if (workshopDefs[ws->type].passive) continue;
        if (ws->assignedCrafter >= 0) continue;
        if (ws->billCount > 0) {
            anyAvailable = true;
            break;
        }
    }
    if (!anyAvailable) return -1;

    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->z != moverZ) continue;  // Same z-level
        if (workshopDefs[ws->type].passive) continue;  // Passive workshops don't use crafters
        if (ws->assignedCrafter >= 0) continue;  // Already has crafter

        // Find first non-suspended bill that can run
        for (int b = 0; b < ws->billCount; b++) {
            Bill* bill = &ws->bills[b];

            // Auto-resume bills that were suspended due to no storage
            if (bill->suspended && bill->suspendedNoStorage) {
                // Check if storage is now available for any available input material
                int resumeRecipeCount;
                Recipe* resumeRecipes = GetRecipesForWorkshop(ws->type, &resumeRecipeCount);
                if (bill->recipeIdx >= 0 && bill->recipeIdx < resumeRecipeCount) {
                    Recipe* resumeRecipe = &resumeRecipes[bill->recipeIdx];
                    for (int i = 0; i < itemHighWaterMark; i++) {
                        if (!items[i].active) continue;
                        if (!RecipeInputMatches(resumeRecipe, &items[i])) continue;
                        if (items[i].reservedBy != -1) continue;
                        if ((int)items[i].z != ws->z) continue;
                        uint8_t mat = items[i].material;
                        if (mat == MAT_NONE) mat = DefaultMaterialForItemType(items[i].type);
                        int outSlotX, outSlotY;
                        if (FindStockpileForItem(resumeRecipe->outputType, mat, &outSlotX, &outSlotY) >= 0) {
                            bill->suspended = false;
                            bill->suspendedNoStorage = false;
                            break;
                        }
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

            // Find an input item first (search nearby or all if radius = 0)
            int searchRadius = bill->ingredientSearchRadius;
            if (searchRadius == 0) searchRadius = 100;  // Large default

            int itemIdx = -1;
            int bestDistSq = searchRadius * searchRadius;

            // Search for closest unreserved item of the required type
            for (int i = 0; i < itemHighWaterMark; i++) {
                Item* item = &items[i];
                if (!item->active) continue;
                if (!RecipeInputMatches(recipe, item)) continue;
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

            // Check if there's stockpile space for the output (using input's material)
            uint8_t outputMat = item->material;
            if (outputMat == MAT_NONE) {
                outputMat = DefaultMaterialForItemType(item->type);
            }
            int outSlotX, outSlotY;
            if (FindStockpileForItem(recipe->outputType, outputMat, &outSlotX, &outSlotY) < 0) {
                bill->suspended = true;
                bill->suspendedNoStorage = true;
                continue;
            }

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

            // Find second input item if recipe requires it
            int item2Idx = -1;
            if (recipe->inputType2 != ITEM_NONE && recipe->inputCount2 > 0) {
                int best2DistSq = searchRadius * searchRadius;
                for (int i = 0; i < itemHighWaterMark; i++) {
                    Item* item2 = &items[i];
                    if (!item2->active) continue;
                    if (item2->type != recipe->inputType2) continue;
                    if (item2->reservedBy != -1) continue;
                    if (item2->unreachableCooldown > 0.0f) continue;
                    if ((int)item2->z != ws->z) continue;
                    if (i == itemIdx) continue;  // Can't use same item as first input

                    int item2TileX = (int)(item2->x / CELL_SIZE);
                    int item2TileY = (int)(item2->y / CELL_SIZE);
                    int dx2 = item2TileX - ws->x;
                    int dy2 = item2TileY - ws->y;
                    int dist2Sq = dx2 * dx2 + dy2 * dy2;
                    if (dist2Sq > best2DistSq) continue;

                    best2DistSq = dist2Sq;
                    item2Idx = i;
                }
                if (item2Idx < 0) continue;  // Second input not available

                // Verify second input is reachable from workshop
                Point item2Cell = { (int)(items[item2Idx].x / CELL_SIZE), (int)(items[item2Idx].y / CELL_SIZE), (int)items[item2Idx].z };
                int item2PathLen = FindPath(moverPathAlgorithm, workCell, item2Cell, tempPath, MAX_PATH);
                if (item2PathLen == 0) {
                    SetItemUnreachableCooldown(item2Idx, UNREACHABLE_COOLDOWN);
                    continue;
                }
            }

            // Check fuel availability for fuel-requiring recipes
            int fuelIdx = -1;
            if (recipe->fuelRequired > 0) {
                if (!WorkshopHasFuelForRecipe(ws, searchRadius)) continue;
                fuelIdx = FindNearestFuelItem(ws, searchRadius);
                if (fuelIdx < 0 || fuelIdx == itemIdx) continue;
                // Verify fuel item is reachable from workshop
                Point fuelCell = { (int)(items[fuelIdx].x / CELL_SIZE), (int)(items[fuelIdx].y / CELL_SIZE), (int)items[fuelIdx].z };
                Point workCell2 = { ws->workTileX, ws->workTileY, ws->z };
                int fuelPathLen = FindPath(moverPathAlgorithm, workCell2, fuelCell, tempPath, MAX_PATH);
                if (fuelPathLen == 0) continue;
            }

            // Reserve items and workshop
            item->reservedBy = moverIdx;
            ws->assignedCrafter = moverIdx;
            if (item2Idx >= 0) items[item2Idx].reservedBy = moverIdx;
            if (fuelIdx >= 0) items[fuelIdx].reservedBy = moverIdx;

            // Create job
            int jobId = CreateJob(JOBTYPE_CRAFT);
            if (jobId < 0) {
                item->reservedBy = -1;
                ws->assignedCrafter = -1;
                if (item2Idx >= 0) items[item2Idx].reservedBy = -1;
                if (fuelIdx >= 0) items[fuelIdx].reservedBy = -1;
                return -1;
            }

            Job* job = GetJob(jobId);
            job->assignedMover = moverIdx;
            job->targetWorkshop = w;
            job->targetBillIdx = b;
            job->targetItem = itemIdx;
            job->targetItem2 = item2Idx;
            job->step = CRAFT_STEP_MOVING_TO_INPUT;
            job->progress = 0.0f;
            job->carryingItem = -1;
            job->fuelItem = fuelIdx;

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

        // Check if slot is full — if so, treat as clear instead
        Stockpile* sp = &stockpiles[spOnItem];
        int lx = slotX - sp->x;
        int ly = slotY - sp->y;
        int idx = ly * sp->width + lx;
        if (sp->slotCounts[idx] + sp->reservedBy[idx] >= sp->maxStackSize) {
            absorb = false;
            spIdx = FindStockpileForItem(item->type, item->material, &slotX, &slotY);
            if (spIdx < 0) {
                safeDrop = true;
            }
        }
    } else {
        // Clear: find destination stockpile or safe-drop location
        spIdx = FindStockpileForItem(item->type, item->material, &slotX, &slotY);
        if (spIdx < 0) {
            safeDrop = true;  // No stockpile accepts this type, safe-drop it
        }
    }

    // Reserve item
    if (!ReserveItem(itemIdx, moverIdx)) return -1;

    // Reserve stockpile slot (unless safeDrop)
    if (!safeDrop) {
        if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx, item->type, item->material)) {
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
        bool noLongerAllowed = !StockpileAcceptsItem(currentSp, items[j].type, items[j].material);

        if (noLongerAllowed) {
            // Find any stockpile that accepts this item type
            destSp = FindStockpileForItem(items[j].type, items[j].material, &destSlotX, &destSlotY);
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
    if (!ReserveStockpileSlot(bestDestSp, bestDestSlotX, bestDestSlotY, moverIdx, item->type, item->material)) {
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

        // Check if still unassigned, correct type, and not marked unreachable
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_MINE || d->assignedMover != -1) continue;
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

        // Check if still unassigned, correct type, and not marked unreachable
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_CHANNEL || d->assignedMover != -1) continue;
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

// WorkGiver_DigRamp: Find a dig ramp designation to work on
// Returns job ID if successful, -1 if no job available
// Mover stands adjacent to the wall and carves it into a ramp
int WorkGiver_DigRamp(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability - uses mining skill
    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    // Find nearest unassigned dig ramp designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < digRampCacheCount; i++) {
        AdjacentDesignationEntry* entry = &digRampCache[i];

        // Only same z-level for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned, correct type, and not marked unreachable
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_DIG_RAMP || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        float workPosX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float workPosY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = workPosX - m->x;
        float dy = workPosY - m->y;
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
    int jobId = CreateJob(JOBTYPE_DIG_RAMP);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;  // Reusing mine target fields
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

        // Check if still unassigned, correct type, and not marked unreachable
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_REMOVE_FLOOR || d->assignedMover != -1) continue;
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

        // Check if still unassigned, correct type, and not marked unreachable
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_REMOVE_RAMP || d->assignedMover != -1) continue;
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

    // Find nearest unassigned chop designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < chopCacheCount; i++) {
        AdjacentDesignationEntry* entry = &chopCache[i];

        // Only same z-level for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned and correct type
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_CHOP || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Distance to adjacent tile (already cached)
        float adjPosX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float adjPosY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float distX = adjPosX - m->x;
        float distY = adjPosY - m->y;
        float distSq = distX * distX + distY * distY;

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

// WorkGiver_ChopFelled: Find a felled trunk designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_ChopFelled(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canMine) return -1;

    int moverZ = (int)m->z;

    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < chopFelledCacheCount; i++) {
        AdjacentDesignationEntry* entry = &chopFelledCache[i];

        // Only same z-level for now
        if (entry->z != moverZ) continue;

        // Check if still unassigned and correct type
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_CHOP_FELLED || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Verify felled trunk still exists
        if (grid[entry->z][entry->y][entry->x] != CELL_TREE_FELLED) continue;

        // Distance to adjacent tile (already cached)
        float adjPosX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float adjPosY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float distX = adjPosX - m->x;
        float distY = adjPosY - m->y;
        float distSq = distX * distX + distY * distY;

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

    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    if (!FindReachableAdjacentTile(bestDesigX, bestDesigY, bestDesigZ, moverCell, &bestAdjX, &bestAdjY)) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    int jobId = CreateJob(JOBTYPE_CHOP_FELLED);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->targetAdjX = bestAdjX;
    job->targetAdjY = bestAdjY;
    job->step = 0;
    job->progress = 0.0f;

    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

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
// Filter for blueprint haul items (building materials, optionally filtered by type)
static bool BlueprintHaulItemFilter(int itemIdx, void* userData) {
    ItemType required = *(ItemType*)userData;
    Item* item = &items[itemIdx];

    if (!item->active) return false;
    if (!ItemIsBuildingMat(item->type)) return false;
    if (required != ITEM_TYPE_COUNT && item->type != required) return false;
    if (item->reservedBy != -1) return false;
    if (item->state != ITEM_ON_GROUND) return false;
    if (item->unreachableCooldown > 0.0f) return false;

    return true;
}

// Find nearest matching building material for a blueprint's requiredItemType
static int FindNearestBuildingMat(int moverTileX, int moverTileY, int moverZ, float mx, float my, ItemType required) {
    int bestItemIdx = -1;
    float bestDistSq = 1e30f;

    // Try spatial grid first for ground items
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        int radii[] = {10, 25, 50, 100};
        int numRadii = sizeof(radii) / sizeof(radii[0]);

        for (int r = 0; r < numRadii && bestItemIdx < 0; r++) {
            bestItemIdx = FindFirstItemInRadius(moverTileX, moverTileY, moverZ, radii[r],
                                                BlueprintHaulItemFilter, &required);
        }

        if (bestItemIdx >= 0) {
            float dx = items[bestItemIdx].x - mx;
            float dy = items[bestItemIdx].y - my;
            bestDistSq = dx * dx + dy * dy;
        }
    }

    // Linear scan fallback (for tests, plus checks stockpile items)
    for (int j = 0; j < itemHighWaterMark; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (!ItemIsBuildingMat(item->type)) continue;
        if (required != ITEM_TYPE_COUNT && item->type != required) continue;
        if (item->reservedBy != -1) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        if ((int)item->z != moverZ) continue;

        float dx = item->x - mx;
        float dy = item->y - my;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestItemIdx = j;
        }
    }

    return bestItemIdx;
}

int WorkGiver_BlueprintHaul(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Check capability
    if (!m->capabilities.canHaul) return -1;

    int moverZ = (int)m->z;
    int moverTileX = (int)(m->x / CELL_SIZE);
    int moverTileY = (int)(m->y / CELL_SIZE);
    Point moverCell = { moverTileX, moverTileY, moverZ };
    Point tempPath[MAX_PATH];

    // Blueprint-driven: for each blueprint needing materials, find a matching item
    int bestBpIdx = -1;
    int bestItemIdx = -1;

    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;
        if (bp->reservedItem >= 0) continue;

        // Quick z-level reachability check
        if (bp->z != moverZ) {
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

        // Find nearest item matching this blueprint's required type
        int itemIdx = FindNearestBuildingMat(moverTileX, moverTileY, moverZ, m->x, m->y, bp->requiredItemType);
        if (itemIdx < 0) continue;

        // Check blueprint reachability via pathfinding
        int tempLen = 0;
        if (IsCellWalkableAt(bp->z, bp->y, bp->x)) {
            Point bpCell = { bp->x, bp->y, bp->z };
            tempLen = FindPath(moverPathAlgorithm, moverCell, bpCell, tempPath, MAX_PATH);
        }
        if (tempLen == 0) {
            int dx[] = {1, -1, 0, 0};
            int dy[] = {0, 0, 1, -1};
            for (int i = 0; i < 4; i++) {
                int ax = bp->x + dx[i];
                int ay = bp->y + dy[i];
                if (ax < 0 || ax >= gridWidth || ay < 0 || ay >= gridHeight) continue;
                if (!IsCellWalkableAt(bp->z, ay, ax)) continue;
                Point adjCell = { ax, ay, bp->z };
                tempLen = FindPath(moverPathAlgorithm, moverCell, adjCell, tempPath, MAX_PATH);
                if (tempLen > 0) break;
            }
        }
        if (tempLen == 0) continue;

        // Check item reachability
        Point itemCell = { (int)(items[itemIdx].x / CELL_SIZE), (int)(items[itemIdx].y / CELL_SIZE), (int)items[itemIdx].z };
        int itemPathLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
        if (itemPathLen == 0) continue;

        // Found a valid blueprint + item pair
        bestBpIdx = bpIdx;
        bestItemIdx = itemIdx;
        break;
    }

    if (bestBpIdx < 0 || bestItemIdx < 0) return -1;

    Blueprint* bp = &blueprints[bestBpIdx];

    // Reserve item
    if (!ReserveItem(bestItemIdx, moverIdx)) return -1;
    bp->reservedItem = bestItemIdx;

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

    Point itemCell = { (int)(items[bestItemIdx].x / CELL_SIZE), (int)(items[bestItemIdx].y / CELL_SIZE), (int)items[bestItemIdx].z };
    m->currentJobId = jobId;
    m->goal = itemCell;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}
