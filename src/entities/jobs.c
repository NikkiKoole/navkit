#include "jobs.h"
#include "items.h"
#include "item_defs.h"
#include "stacking.h"
#include "containers.h"
#include "mover.h"
#include "workshops.h"
#include "../core/time.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/pathfinding.h"
#include "stockpiles.h"
#include "../world/designations.h"
#include "../simulation/trees.h"
#include "../simulation/floordirt.h"
#include "../../shared/profiler.h"
#include "../../shared/ui.h"
#include "../../vendor/raylib.h"
#include "../simulation/lighting.h"
#include "../simulation/balance.h"
#include "butchering.h"
#include "tool_quality.h"
#include "../simulation/farming.h"
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

static AdjacentDesignationEntry gatherTreeCache[MAX_DESIGNATION_CACHE];
static int gatherTreeCacheCount = 0;

static OnTileDesignationEntry cleanCache[MAX_DESIGNATION_CACHE];
static int cleanCacheCount = 0;

static OnTileDesignationEntry harvestBerryCache[MAX_DESIGNATION_CACHE];
static int harvestBerryCacheCount = 0;

static AdjacentDesignationEntry knapCache[MAX_DESIGNATION_CACHE];
static int knapCacheCount = 0;

static OnTileDesignationEntry digRootsCache[MAX_DESIGNATION_CACHE];
static int digRootsCacheCount = 0;
static OnTileDesignationEntry exploreCache[MAX_DESIGNATION_CACHE];
static int exploreCacheCount = 0;
static OnTileDesignationEntry tillCache[MAX_DESIGNATION_CACHE];
static int tillCacheCount = 0;

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
static bool gatherTreeCacheDirty = true;
static bool cleanCacheDirty = true;
static bool harvestBerryCacheDirty = true;
static bool knapCacheDirty = true;
static bool digRootsCacheDirty = true;
static bool exploreCacheDirty = true;
static bool tillCacheDirty = true;

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
static void RebuildGatherTreeDesignationCache(void);
static void RebuildCleanDesignationCache(void);
static void RebuildHarvestBerryDesignationCache(void);
static void RebuildKnapDesignationCache(void);
static void RebuildDigRootsDesignationCache(void);
static void RebuildExploreDesignationCache(void);
static void RebuildTillDesignationCache(void);

// Forward declarations for WorkGivers (needed for designationSpecs table)
static int WorkGiver_TillDesignation(int moverIdx);
static int WorkGiver_CleanDesignation(int moverIdx);
static int WorkGiver_HarvestBerry(int moverIdx);
JobRunResult RunJob_HarvestBerry(Job* job, void* moverPtr, float dt);
static int WorkGiver_KnapDesignation(int moverIdx);
static int WorkGiver_DigRootsDesignation(int moverIdx);
static int WorkGiver_ExploreDesignation(int moverIdx);

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
    {DESIGNATION_GATHER_TREE, JOBTYPE_GATHER_TREE, RebuildGatherTreeDesignationCache, WorkGiver_GatherTree,
     gatherTreeCache, &gatherTreeCacheCount, &gatherTreeCacheDirty},
    {DESIGNATION_CLEAN, JOBTYPE_CLEAN, RebuildCleanDesignationCache, WorkGiver_CleanDesignation,
     cleanCache, &cleanCacheCount, &cleanCacheDirty},
    {DESIGNATION_HARVEST_BERRY, JOBTYPE_HARVEST_BERRY, RebuildHarvestBerryDesignationCache, WorkGiver_HarvestBerry,
     harvestBerryCache, &harvestBerryCacheCount, &harvestBerryCacheDirty},
    {DESIGNATION_KNAP, JOBTYPE_KNAP, RebuildKnapDesignationCache, WorkGiver_KnapDesignation,
     knapCache, &knapCacheCount, &knapCacheDirty},
    {DESIGNATION_DIG_ROOTS, JOBTYPE_DIG_ROOTS, RebuildDigRootsDesignationCache, WorkGiver_DigRootsDesignation,
     digRootsCache, &digRootsCacheCount, &digRootsCacheDirty},
    {DESIGNATION_EXPLORE, JOBTYPE_EXPLORE, RebuildExploreDesignationCache, WorkGiver_ExploreDesignation,
     exploreCache, &exploreCacheCount, &exploreCacheDirty},
    {DESIGNATION_FARM, JOBTYPE_TILL, RebuildTillDesignationCache, WorkGiver_TillDesignation,
     tillCache, &tillCacheCount, &tillCacheDirty},
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
                                            int* count,
                                            bool requireExplored) {
    *count = 0;
    if (activeDesignationCount == 0) return;

    for (int z = 0; z < gridDepth && *count < MAX_DESIGNATION_CACHE; z++) {
        for (int y = 0; y < gridHeight && *count < MAX_DESIGNATION_CACHE; y++) {
            for (int x = 0; x < gridWidth && *count < MAX_DESIGNATION_CACHE; x++) {
                Designation* d = GetDesignation(x, y, z);
                if (!d || d->type != type) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;
                if (requireExplored && !IsExplored(x, y, z)) continue;

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
                                          int* count,
                                          bool requireExplored) {
    *count = 0;
    if (activeDesignationCount == 0) return;

    for (int z = 0; z < gridDepth && *count < MAX_DESIGNATION_CACHE; z++) {
        for (int y = 0; y < gridHeight && *count < MAX_DESIGNATION_CACHE; y++) {
            for (int x = 0; x < gridWidth && *count < MAX_DESIGNATION_CACHE; x++) {
                Designation* d = GetDesignation(x, y, z);
                if (!d || d->type != type) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;
                if (requireExplored && !IsExplored(x, y, z)) continue;

                cache[(*count)++] = (OnTileDesignationEntry){x, y, z};
            }
        }
    }
}

void RebuildMineDesignationCache(void) {
    if (!mineCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_MINE, mineCache, &mineCacheCount, true);
    mineCacheDirty = false;
}

static void RebuildChannelDesignationCache(void) {
    if (!channelCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_CHANNEL, channelCache, &channelCacheCount, true);
    channelCacheDirty = false;
}

static void RebuildRemoveFloorDesignationCache(void) {
    if (!removeFloorCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_REMOVE_FLOOR, removeFloorCache, &removeFloorCacheCount, true);
    removeFloorCacheDirty = false;
}

static void RebuildRemoveRampDesignationCache(void) {
    if (!removeRampCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_REMOVE_RAMP, removeRampCache, &removeRampCacheCount, true);
    removeRampCacheDirty = false;
}

static void RebuildDigRampDesignationCache(void) {
    if (!digRampCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_DIG_RAMP, digRampCache, &digRampCacheCount, true);
    digRampCacheDirty = false;
}

static void RebuildChopDesignationCache(void) {
    if (!chopCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_CHOP, chopCache, &chopCacheCount, true);
    chopCacheDirty = false;
}

static void RebuildChopFelledDesignationCache(void) {
    if (!chopFelledCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_CHOP_FELLED, chopFelledCache, &chopFelledCacheCount, true);
    chopFelledCacheDirty = false;
}

static void RebuildGatherSaplingDesignationCache(void) {
    if (!gatherSaplingCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_GATHER_SAPLING, gatherSaplingCache, &gatherSaplingCacheCount, true);
    gatherSaplingCacheDirty = false;
}

static void RebuildPlantSaplingDesignationCache(void) {
    if (!plantSaplingCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_PLANT_SAPLING, plantSaplingCache, &plantSaplingCacheCount, true);
    plantSaplingCacheDirty = false;
}

static void RebuildGatherGrassDesignationCache(void) {
    if (!gatherGrassCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_GATHER_GRASS, gatherGrassCache, &gatherGrassCacheCount, true);
    gatherGrassCacheDirty = false;
}

static void RebuildGatherTreeDesignationCache(void) {
    if (!gatherTreeCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_GATHER_TREE, gatherTreeCache, &gatherTreeCacheCount, true);
    gatherTreeCacheDirty = false;
}

static void RebuildCleanDesignationCache(void) {
    if (!cleanCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_CLEAN, cleanCache, &cleanCacheCount, true);
    cleanCacheDirty = false;
}

static void RebuildHarvestBerryDesignationCache(void) {
    if (!harvestBerryCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_HARVEST_BERRY, harvestBerryCache, &harvestBerryCacheCount, true);
    harvestBerryCacheDirty = false;
}

static void RebuildKnapDesignationCache(void) {
    if (!knapCacheDirty) return;
    RebuildAdjacentDesignationCache(DESIGNATION_KNAP, knapCache, &knapCacheCount, true);
    knapCacheDirty = false;
}

static void RebuildDigRootsDesignationCache(void) {
    if (!digRootsCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_DIG_ROOTS, digRootsCache, &digRootsCacheCount, true);
    digRootsCacheDirty = false;
}

static void RebuildExploreDesignationCache(void) {
    if (!exploreCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_EXPLORE, exploreCache, &exploreCacheCount, false);
    exploreCacheDirty = false;
}

static void RebuildTillDesignationCache(void) {
    if (!tillCacheDirty) return;
    RebuildOnTileDesignationCache(DESIGNATION_FARM, tillCache, &tillCacheCount, true);
    tillCacheDirty = false;
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
        PROFILE_COUNT(pathfinds, 1);
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
        jobs[i].toolItem = -1;
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
    job->targetItem3 = -1;
    job->toolItem = -1;
    job->targetAnimalIdx = -1;

    // Add to active list
    activeJobList[activeJobCount++] = jobId;
    jobIsActive[jobId] = true;

    EventLog("Job %d created type=%s", jobId, JobTypeName(type));
    PROFILE_COUNT(jobs_created, 1);
    return jobId;
}

void ReleaseJob(int jobId) {
    if (jobId < 0 || jobId >= MAX_JOBS) return;
    if (!jobs[jobId].active) return;

    EventLog("Job %d released type=%s mover=%d", jobId, JobTypeName(jobs[jobId].type), jobs[jobId].assignedMover);
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
static void ExtractItemFromContainer(int itemIdx);

// =============================================================================
// Shared Job Step Helpers
// =============================================================================

// Tool fetch step: walk to reserved tool item, equip on arrival.
// On arrival: drops old tool if different, clears stockpile slot if needed,
// sets mover->equippedTool, clears job->toolItem, advances to nextStep.
static JobRunResult RunToolFetchStep(Job* job, Mover* mover, int moverIdx, int nextStep) {
    int toolIdx = job->toolItem;
    if (toolIdx < 0 || !items[toolIdx].active) return JOBRUN_FAIL;
    if (items[toolIdx].reservedBy != moverIdx) return JOBRUN_FAIL;

    Item* tool = &items[toolIdx];
    int toolCellX = (int)(tool->x / CELL_SIZE);
    int toolCellY = (int)(tool->y / CELL_SIZE);
    int toolCellZ = (int)(tool->z);

    // Set goal to tool position
    if (mover->goal.x != toolCellX || mover->goal.y != toolCellY || mover->goal.z != toolCellZ) {
        mover->goal = (Point){toolCellX, toolCellY, toolCellZ};
        mover->needsRepath = true;
    }

    float dx = mover->x - tool->x;
    float dy = mover->y - tool->y;
    float distSq = dx * dx + dy * dy;

    TryFinalApproach(mover, tool->x, tool->y, toolCellX, toolCellY, PICKUP_RADIUS);

    if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
        SetItemUnreachableCooldown(toolIdx, UNREACHABLE_COOLDOWN);
        return JOBRUN_FAIL;
    }

    if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
        // Drop old tool if mover has a different one
        if (mover->equippedTool >= 0 && mover->equippedTool != toolIdx) {
            DropEquippedTool(moverIdx);
        }

        // Clear stockpile slot if tool was in stockpile
        if (tool->state == ITEM_IN_STOCKPILE) {
            ClearSourceStockpileSlot(tool);
        }

        // Equip the tool
        tool->state = ITEM_CARRIED;
        tool->reservedBy = moverIdx;
        tool->x = mover->x;
        tool->y = mover->y;
        tool->z = mover->z;
        mover->equippedTool = toolIdx;

        job->toolItem = -1;  // No longer tracking — it's equipped now
        job->step = nextStep;

        // Set goal for the actual job (mover->goal will be set by the next step's logic)
        mover->needsRepath = true;

        EventLog("Mover %d equipped tool item %d (%s)", moverIdx, toolIdx,
                 itemDefs[tool->type].name);
    }

    return JOBRUN_RUNNING;
}

// Walk to item, pick it up, advance to STEP_CARRYING, set next goal.
// Returns JOBRUN_RUNNING while walking, JOBRUN_FAIL on error.
// On successful pickup: sets step=STEP_CARRYING, mover->goal=nextGoal, returns JOBRUN_RUNNING.
static JobRunResult RunPickupStep(Job* job, Mover* mover, Point nextGoal) {
    int itemIdx = job->targetItem;
    if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;

    Item* item = &items[itemIdx];
    int itemCellX = (int)(item->x / CELL_SIZE);
    int itemCellY = (int)(item->y / CELL_SIZE);
    int itemCellZ = (int)(item->z);

    if (!IsCellWalkableAt(itemCellZ, itemCellY, itemCellX)) {
        SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
        return JOBRUN_FAIL;
    }

    float dx = mover->x - item->x;
    float dy = mover->y - item->y;
    float distSq = dx * dx + dy * dy;

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
            ClearSourceStockpileSlot(item);
        }
        item->state = ITEM_CARRIED;
        EventLog("Item %d (%s x%d) picked up by mover %d for job %d",
                 itemIdx, ItemName(item->type), item->stackCount,
                 (int)(mover - movers), (int)(job - jobs));
        job->carryingItem = itemIdx;
        job->targetItem = -1;
        job->step = STEP_CARRYING;

        mover->goal = nextGoal;
        mover->needsRepath = true;
    }

    return JOBRUN_RUNNING;
}

// Carry item toward destination, updating item position.
// Returns JOBRUN_RUNNING while walking, JOBRUN_FAIL on error, JOBRUN_DONE on arrival.
static JobRunResult RunCarryStep(Job* job, Mover* mover, int destX, int destY, int destZ) {
    int itemIdx = job->carryingItem;
    if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;

    float targetX = destX * CELL_SIZE + CELL_SIZE * 0.5f;
    float targetY = destY * CELL_SIZE + CELL_SIZE * 0.5f;
    float dx = mover->x - targetX;
    float dy = mover->y - targetY;
    float distSq = dx * dx + dy * dy;

    if (IsPathExhausted(mover) && distSq >= DROP_RADIUS * DROP_RADIUS) {
        mover->goal = (Point){destX, destY, destZ};
        mover->needsRepath = true;
    }

    bool correctZ = (int)mover->z == destZ;
    if (correctZ) TryFinalApproach(mover, targetX, targetY, destX, destY, DROP_RADIUS);

    if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
        return JOBRUN_FAIL;
    }

    // Update carried item position (including container contents)
    items[itemIdx].x = mover->x;
    items[itemIdx].y = mover->y;
    items[itemIdx].z = mover->z;
    if (items[itemIdx].contentCount > 0) MoveContainer(itemIdx, mover->x, mover->y, mover->z);

    if (correctZ && distSq < DROP_RADIUS * DROP_RADIUS) {
        return JOBRUN_DONE;
    }

    return JOBRUN_RUNNING;
}

// Walk to adjacent tile (targetAdjX/Y at targetMineZ) for wall-adjacent jobs.
// Returns JOBRUN_RUNNING while walking, JOBRUN_FAIL if stuck.
// On arrival: sets step=STEP_WORKING, returns JOBRUN_RUNNING.
static JobRunResult RunWalkToAdjacentStep(Job* job, Mover* mover) {
    int adjX = job->targetAdjX;
    int adjY = job->targetAdjY;
    int z = job->targetMineZ;

    if (mover->goal.x != adjX || mover->goal.y != adjY || mover->goal.z != z) {
        mover->goal = (Point){adjX, adjY, z};
        mover->needsRepath = true;
    }

    float goalX = adjX * CELL_SIZE + CELL_SIZE * 0.5f;
    float goalY = adjY * CELL_SIZE + CELL_SIZE * 0.5f;
    float dx = mover->x - goalX;
    float dy = mover->y - goalY;
    float distSq = dx * dx + dy * dy;

    bool correctZ = (int)mover->z == z;
    if (correctZ) TryFinalApproach(mover, goalX, goalY, adjX, adjY, PICKUP_RADIUS);

    if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
        Designation* d = GetDesignation(job->targetMineX, job->targetMineY, z);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return JOBRUN_FAIL;
    }

    if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
        job->step = STEP_WORKING;
    }

    return JOBRUN_RUNNING;
}

// Walk to tile (targetMineX/Y at targetMineZ) for on-tile jobs.
// Returns JOBRUN_RUNNING while walking, JOBRUN_FAIL if stuck.
// On arrival: sets step=STEP_WORKING, returns JOBRUN_RUNNING.
static JobRunResult RunWalkToTileStep(Job* job, Mover* mover) {
    int tx = job->targetMineX;
    int ty = job->targetMineY;
    int tz = job->targetMineZ;

    if (mover->goal.x != tx || mover->goal.y != ty || mover->goal.z != tz) {
        mover->goal = (Point){tx, ty, tz};
        mover->needsRepath = true;
    }

    float goalX = tx * CELL_SIZE + CELL_SIZE * 0.5f;
    float goalY = ty * CELL_SIZE + CELL_SIZE * 0.5f;
    float dx = mover->x - goalX;
    float dy = mover->y - goalY;
    float distSq = dx * dx + dy * dy;

    bool correctZ = (int)mover->z == tz;
    if (correctZ) TryFinalApproach(mover, goalX, goalY, tx, ty, PICKUP_RADIUS);

    if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
        Designation* d = GetDesignation(tx, ty, tz);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return JOBRUN_FAIL;
    }

    if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
        job->step = STEP_WORKING;
    }

    return JOBRUN_RUNNING;
}

// Progress work accumulation. Returns JOBRUN_DONE when complete, JOBRUN_RUNNING otherwise.
// speedMultiplier: 1.0 = normal, 0.5 = half speed (bare hands), 2.0 = double speed, etc.
static JobRunResult RunWorkProgress(Job* job, Designation* d, Mover* mover,
                                     float dt, float workTimeGH, bool resetStuck,
                                     float speedMultiplier) {
    if (resetStuck) mover->timeWithoutProgress = 0.0f;
    job->progress += (dt * speedMultiplier) / GameHoursToGameSeconds(workTimeGH);
    if (d) d->progress = job->progress;
    return (job->progress >= 1.0f) ? JOBRUN_DONE : JOBRUN_RUNNING;
}

// =============================================================================
// Job Drivers
// =============================================================================

// Haul job driver: pick up item -> carry to stockpile -> drop
JobRunResult RunJob_Haul(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    (void)dt;
    int spIdx = job->targetStockpile;

    if (job->step == STEP_MOVING_TO_PICKUP) {
        if (spIdx >= 0 && !stockpiles[spIdx].active) return JOBRUN_FAIL;
        return RunPickupStep(job, mover, (Point){job->targetSlotX, job->targetSlotY, stockpiles[spIdx].z});
    }
    if (job->step == STEP_CARRYING) {
        if (!stockpiles[spIdx].active) return JOBRUN_FAIL;
        int itemIdx = job->carryingItem;
        if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;
        if (!StockpileAcceptsItem(spIdx, items[itemIdx].type, items[itemIdx].material)) return JOBRUN_FAIL;

        JobRunResult r = RunCarryStep(job, mover, job->targetSlotX, job->targetSlotY, stockpiles[spIdx].z);
        if (r == JOBRUN_DONE) {
            Item* item = &items[itemIdx];
            float targetX = job->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY = job->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
            item->x = targetX;
            item->y = targetY;
            item->reservedBy = -1;
            PlaceItemInStockpile(spIdx, job->targetSlotX, job->targetSlotY, itemIdx);
            job->carryingItem = -1;
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Clear job driver: pick up item -> carry to safe drop location outside stockpile
JobRunResult RunJob_Clear(Job* job, void* moverPtr, float dt) {
    (void)dt;
    Mover* mover = (Mover*)moverPtr;

    if (job->step == STEP_MOVING_TO_PICKUP) {
        int itemIdx = job->targetItem;
        if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;

        Item* item = &items[itemIdx];
        int itemCellX = (int)(item->x / CELL_SIZE);
        int itemCellY = (int)(item->y / CELL_SIZE);
        int itemCellZ = (int)(item->z);
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
            if (item->state == ITEM_IN_STOCKPILE) ClearSourceStockpileSlot(item);
            item->state = ITEM_CARRIED;
            EventLog("Item %d (%s x%d) picked up by mover %d for job %d",
                     itemIdx, ItemName(item->type), item->stackCount, (int)(mover - movers), (int)(job - jobs));
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
                        int checkX = moverTileX + dx2, checkY = moverTileY + dy2;
                        if (checkX < 0 || checkY < 0 || checkX >= gridWidth || checkY >= gridHeight) continue;
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
            if (!foundDrop) { job->targetSlotX = moverTileX; job->targetSlotY = moverTileY; }

            mover->goal = (Point){job->targetSlotX, job->targetSlotY, (int)mover->z};
            mover->needsRepath = true;
        }
        return JOBRUN_RUNNING;
    }
    if (job->step == STEP_CARRYING) {
        JobRunResult r = RunCarryStep(job, mover, job->targetSlotX, job->targetSlotY, (int)mover->z);
        if (r == JOBRUN_DONE) {
            int itemIdx = job->carryingItem;
            Item* item = &items[itemIdx];
            item->state = ITEM_ON_GROUND;
            item->x = job->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f;
            item->y = job->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f;
            item->reservedBy = -1;
            job->carryingItem = -1;
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Mine job driver: move to adjacent tile -> mine wall
JobRunResult RunJob_Mine(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    if (job->step == STEP_FETCHING_TOOL) return RunToolFetchStep(job, mover, moverIdx, STEP_MOVING_TO_WORK);
    Designation* d = GetDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
    if (!d || d->type != DESIGNATION_MINE) return JOBRUN_FAIL;
    if (!CellIsSolid(grid[job->targetMineZ][job->targetMineY][job->targetMineX])) {
        CancelDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
        return JOBRUN_FAIL;
    }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        MaterialType mat = GetWallMaterial(job->targetMineX, job->targetMineY, job->targetMineZ);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_MINE, mat, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, MINE_WORK_TIME, false, speed);
        if (r == JOBRUN_DONE) CompleteMineDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
        return r;
    }
    return JOBRUN_FAIL;
}

// Channel job driver: move to tile -> channel (remove floor, mine below, create ramp)
JobRunResult RunJob_Channel(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    if (job->step == STEP_FETCHING_TOOL) return RunToolFetchStep(job, mover, moverIdx, STEP_MOVING_TO_WORK);
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHANNEL) return JOBRUN_FAIL;
    bool hasFloor = HAS_FLOOR(tx, ty, tz) || (tz > 0 && CellIsSolid(grid[tz-1][ty][tx]));
    if (!hasFloor) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        // Channel digs into cell below — check material at z-1 for the quality requirement
        MaterialType mat = (tz > 0) ? GetWallMaterial(tx, ty, tz - 1) : MAT_DIRT;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_CHANNEL, mat, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, CHANNEL_WORK_TIME, false, speed);
        if (r == JOBRUN_DONE) CompleteChannelDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Dig ramp job driver: move adjacent to wall -> carve into ramp
JobRunResult RunJob_DigRamp(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    if (job->step == STEP_FETCHING_TOOL) return RunToolFetchStep(job, mover, moverIdx, STEP_MOVING_TO_WORK);
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_DIG_RAMP) return JOBRUN_FAIL;
    if (!CellIsSolid(grid[tz][ty][tx])) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        MaterialType mat = GetWallMaterial(tx, ty, tz);
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_DIG_RAMP, mat, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, DIG_RAMP_WORK_TIME, false, speed);
        if (r == JOBRUN_DONE) CompleteDigRampDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Remove floor job driver: move to tile -> remove floor (mover may fall!)
JobRunResult RunJob_RemoveFloor(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_REMOVE_FLOOR) return JOBRUN_FAIL;
    if (!HAS_FLOOR(tx, ty, tz)) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, REMOVE_FLOOR_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) CompleteRemoveFloorDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Remove ramp job driver: move to adjacent tile -> remove ramp
JobRunResult RunJob_RemoveRamp(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_REMOVE_RAMP) return JOBRUN_FAIL;
    if (!CellIsRamp(grid[tz][ty][tx])) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, REMOVE_RAMP_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) CompleteRemoveRampDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Chop tree job driver: move to adjacent tile -> chop down tree
JobRunResult RunJob_Chop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    if (job->step == STEP_FETCHING_TOOL) return RunToolFetchStep(job, mover, moverIdx, STEP_MOVING_TO_WORK);
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHOP) return JOBRUN_FAIL;
    if (grid[tz][ty][tx] != CELL_TREE_TRUNK) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        float workTime = IsYoungTreeBase(tx, ty, tz) ? CHOP_YOUNG_WORK_TIME : CHOP_WORK_TIME;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_CHOP, MAT_NONE, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, workTime, false, speed);
        if (r == JOBRUN_DONE) CompleteChopDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Chop felled trunk job driver: move to adjacent tile -> chop up fallen trunk
JobRunResult RunJob_ChopFelled(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    if (job->step == STEP_FETCHING_TOOL) return RunToolFetchStep(job, mover, moverIdx, STEP_MOVING_TO_WORK);
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHOP_FELLED) return JOBRUN_FAIL;
    if (grid[tz][ty][tx] != CELL_TREE_FELLED) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_CHOP_FELLED, MAT_NONE, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, CHOP_FELLED_WORK_TIME, false, speed);
        if (r == JOBRUN_DONE) CompleteChopFelledDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Plant sapling job driver: pick up sapling -> carry to designation -> plant
JobRunResult RunJob_PlantSapling(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_PLANT_SAPLING) return JOBRUN_FAIL;

    if (job->step == STEP_MOVING_TO_PICKUP)
        return RunPickupStep(job, mover, (Point){tx, ty, tz});
    if (job->step == STEP_CARRYING) {
        JobRunResult r = RunCarryStep(job, mover, tx, ty, tz);
        if (r == JOBRUN_DONE) {
            job->step = STEP_PLANTING;
            job->progress = 0.0f;
            return JOBRUN_RUNNING;
        }
        return r;
    }
    if (job->step == STEP_PLANTING) {
        int itemIdx = job->carryingItem;
        JobRunResult r = RunWorkProgress(job, d, mover, dt, PLANT_SAPLING_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) {
            if (itemIdx >= 0 && items[itemIdx].active) {
                PlaceSapling(tx, ty, tz, (MaterialType)items[itemIdx].material);
                DeleteItem(itemIdx);
            } else {
                return JOBRUN_FAIL;
            }
            job->carryingItem = -1;
            CancelDesignation(tx, ty, tz);
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Gather sapling job driver: move to sapling cell -> dig up -> creates item
JobRunResult RunJob_GatherSapling(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_GATHER_SAPLING) return JOBRUN_FAIL;
    if (grid[tz][ty][tx] != CELL_SAPLING) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, GATHER_SAPLING_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) CompleteGatherSaplingDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Gather grass job driver: walk to grass cell, work, spawn item
JobRunResult RunJob_GatherGrass(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_GATHER_GRASS) return JOBRUN_FAIL;
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, GATHER_GRASS_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) CompleteGatherGrassDesignation(tx, ty, tz, job->assignedMover);
        return r;
    }
    return JOBRUN_FAIL;
}

// Harvest berry job driver: walk to tile, work, harvest plant
JobRunResult RunJob_HarvestBerry(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_HARVEST_BERRY) return JOBRUN_FAIL;
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, HARVEST_BERRY_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) CompleteHarvestBerryDesignation(tx, ty, tz);
        return r;
    }
    return JOBRUN_FAIL;
}

// Dig roots job driver: walk to soil cell, work, spawn root items
JobRunResult RunJob_DigRoots(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_DIG_ROOTS) return JOBRUN_FAIL;
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        int moverIdx = (int)(mover - movers);
        MaterialType targetMat = (tz > 0) ? GetWallMaterial(tx, ty, tz - 1) : MAT_DIRT;
        float speedMul = GetJobToolSpeedMultiplier(JOBTYPE_DIG_ROOTS, targetMat, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, DIG_ROOTS_WORK_TIME, false, speedMul);
        if (r == JOBRUN_DONE) CompleteDigRootsDesignation(tx, ty, tz, moverIdx);
        return r;
    }
    return JOBRUN_FAIL;
}

// Trace full Bresenham line from (cx,cy) toward (tx,ty), stopping at first blocked cell.
// Writes walkable cells into outPath, returns count (0 if first step is blocked).
static int BresenhamTrace(int cx, int cy, int tx, int ty, int z, Point* outPath, int maxLen) {
    int dx = abs(tx - cx);
    int dy = abs(ty - cy);
    int sx = (cx < tx) ? 1 : -1;
    int sy = (cy < ty) ? 1 : -1;
    int err = dx - dy;
    int x = cx, y = cy;
    int count = 0;

    while (count < maxLen) {
        if (x == tx && y == ty) break;  // reached target

        int e2 = 2 * err;
        int nx = x, ny = y;
        if (e2 > -dy) { nx += sx; err -= dy; }
        if (e2 < dx)  { ny += sy; err += dx; }

        // Check if next cell is walkable
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) break;
        if (!IsCellWalkableAt(z, ny, nx)) break;

        outPath[count++] = (Point){ nx, ny, z };
        x = nx;
        y = ny;
    }

    // Include the target itself if we reached it and it's walkable
    if (x == tx && y == ty && count < maxLen) {
        // Target already added as last step, or we need to add it
        if (count == 0 || outPath[count-1].x != tx || outPath[count-1].y != ty) {
            if (IsCellWalkableAt(z, ty, tx)) {
                outPath[count++] = (Point){ tx, ty, z };
            }
        }
    }

    return count;
}

// Find the first unexplored cell along the Bresenham line from (cx,cy) to (tx,ty).
// Returns the last explored walkable cell before fog in *outEdge, or false if all explored.
static bool FindFogEdge(int cx, int cy, int tx, int ty, int z, Point* outEdge) {
    int dx = abs(tx - cx);
    int dy = abs(ty - cy);
    int sx = (cx < tx) ? 1 : -1;
    int sy = (cy < ty) ? 1 : -1;
    int err = dx - dy;
    int x = cx, y = cy;
    int prevX = cx, prevY = cy;

    for (int i = 0; i < MAX_MOVER_PATH; i++) {
        if (x == tx && y == ty) break;

        int e2 = 2 * err;
        int nx = x, ny = y;
        if (e2 > -dy) { nx += sx; err -= dy; }
        if (e2 < dx)  { ny += sy; err += dx; }

        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) break;
        if (!IsCellWalkableAt(z, ny, nx)) break;

        if (!IsExplored(nx, ny, z)) {
            // Found fog — return the last explored cell
            *outEdge = (Point){ prevX, prevY, z };
            return true;
        }

        prevX = nx;
        prevY = ny;
        x = nx;
        y = ny;
    }
    return false;  // entire line is explored (or blocked)
}

// Explore job driver: pathfind through explored terrain, then Bresenham into fog
JobRunResult RunJob_Explore(Job* job, void* moverPtr, float dt) {
    (void)dt;
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;

    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_EXPLORE) return JOBRUN_FAIL;

    int cx = (int)(mover->x / CELL_SIZE);
    int cy = (int)(mover->y / CELL_SIZE);
    int cz = (int)mover->z;

    // Designation fulfilled when target cell has been revealed (not just when mover stands on it)
    if (IsExplored(tx, ty, tz)) {
        CompleteExploreDesignation(tx, ty, tz);
        return JOBRUN_DONE;
    }

    // If mover still has path waypoints to walk, let UpdateMovers handle it
    if (mover->pathLength > 0 && mover->pathIndex >= 0 && mover->pathIndex < mover->pathLength) {
        return JOBRUN_RUNNING;
    }

    // Reveal around current position each time we re-trace
    RevealAroundPoint(cx, cy, cz, balance.moverVisionRadius);

    // Find where the Bresenham line enters fog
    Point fogEdge;
    bool hasFogEdge = FindFogEdge(cx, cy, tx, ty, cz, &fogEdge);

    if (hasFogEdge && (fogEdge.x != cx || fogEdge.y != cy)) {
        // Mover is in explored territory — use real pathfinding to reach the fog edge
        Point start = { cx, cy, cz };
        int pathLen = FindPath(moverPathAlgorithm, start, fogEdge, moverPaths[moverIdx], MAX_MOVER_PATH);
        if (pathLen > 0) {
            mover->pathLength = pathLen;
            mover->pathIndex = 0;
            if (mover->pathLength > 2) {
                StringPullPath(moverPaths[moverIdx], &mover->pathLength);
            }
            return JOBRUN_RUNNING;
        }
        // Pathfinding failed — mark unreachable
        d->unreachableCooldown = 10.0f;
        return JOBRUN_FAIL;
    }

    // Mover is at or past the fog edge — use Bresenham into the unknown
    Point tracePath[MAX_MOVER_PATH];
    int traceLen = BresenhamTrace(cx, cy, tx, ty, cz, tracePath, MAX_MOVER_PATH);

    if (traceLen == 0) {
        // First step is blocked — can't proceed toward target
        d->unreachableCooldown = 10.0f;
        return JOBRUN_FAIL;
    }

    // Copy into mover path
    for (int i = 0; i < traceLen; i++) {
        moverPaths[moverIdx][i] = tracePath[i];
    }
    mover->pathLength = traceLen;
    mover->pathIndex = 0;

    if (mover->pathLength > 2) {
        StringPullPath(moverPaths[moverIdx], &mover->pathLength);
    }

    return JOBRUN_RUNNING;
}

// Gather tree job driver: walk to adjacent tile, work, spawn items
JobRunResult RunJob_GatherTree(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_GATHER_TREE) return JOBRUN_FAIL;
    if (grid[tz][ty][tx] != CELL_TREE_TRUNK) { CancelDesignation(tx, ty, tz); return JOBRUN_FAIL; }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToAdjacentStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, GATHER_TREE_WORK_TIME, true, 1.0f);
        if (r == JOBRUN_DONE) CompleteGatherTreeDesignation(tx, ty, tz, job->assignedMover);
        return r;
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
            // Extract from container if needed
            if (item->containedIn != -1) {
                ExtractItemFromContainer(itemIdx);
            }
            // Pick up the item
            else if (item->state == ITEM_IN_STOCKPILE) {
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
            if (items[itemIdx].contentCount > 0) MoveContainer(itemIdx, mover->x, mover->y, mover->z);
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

        // Update carried item position (including container contents)
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;
        if (items[itemIdx].contentCount > 0) MoveContainer(itemIdx, mover->x, mover->y, mover->z);

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

        // Progress building using recipe buildTime, scaled by tool quality
        const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
        float buildTime = recipe ? recipe->stages[bp->stage].buildTime : 2.0f;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_BUILD, MAT_NONE, mover->equippedTool);
        job->progress += dt * speed;
        bp->progress = job->progress / buildTime;

        if (job->progress >= buildTime) {
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

    // Tool fetch phase (before any workshop/bill validation)
    if (job->step == STEP_FETCHING_TOOL) return RunToolFetchStep(job, mover, moverIdx, CRAFT_STEP_MOVING_TO_INPUT);

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
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) {
        return JOBRUN_FAIL;
    }
    const Recipe* recipe = &recipes[bill->recipeIdx];

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

            // Take only what the recipe needs (splits stack if necessary)
            itemIdx = TakeFromStockpileSlot(itemIdx, recipe->inputCount);
            if (itemIdx < 0) return JOBRUN_FAIL;
            item = &items[itemIdx];

            // Extract from container if needed
            if (item->containedIn != -1) {
                ExtractItemFromContainer(itemIdx);
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
                // Check if we need to fetch a third input (skip input2)
                else if (recipe->inputType3 != ITEM_NONE && job->targetItem3 >= 0) {
                    job->step = CRAFT_STEP_MOVING_TO_INPUT3;
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

            // Take only what the recipe needs (splits stack if necessary)
            item2Idx = TakeFromStockpileSlot(item2Idx, recipe->inputCount2);
            if (item2Idx < 0) return JOBRUN_FAIL;
            item2 = &items[item2Idx];

            // Extract from container if needed
            if (item2->containedIn != -1) {
                ExtractItemFromContainer(item2Idx);
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

                // Check if we need to fetch a third input
                if (recipe->inputType3 != ITEM_NONE && job->targetItem3 >= 0) {
                    job->step = CRAFT_STEP_MOVING_TO_INPUT3;
                }
                // Check if we need fuel
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

        case CRAFT_STEP_MOVING_TO_INPUT3: {
            // Walk to third input item
            int item3Idx = job->targetItem3;
            if (item3Idx < 0 || !items[item3Idx].active) {
                return JOBRUN_FAIL;
            }
            Item* item3 = &items[item3Idx];
            if (item3->reservedBy != moverIdx) {
                return JOBRUN_FAIL;
            }

            int item3CellX = (int)(item3->x / CELL_SIZE);
            int item3CellY = (int)(item3->y / CELL_SIZE);
            int item3CellZ = (int)(item3->z);

            if (mover->goal.x != item3CellX || mover->goal.y != item3CellY || mover->goal.z != item3CellZ) {
                mover->goal = (Point){item3CellX, item3CellY, item3CellZ};
                mover->needsRepath = true;
            }

            float dx3m = mover->x - item3->x;
            float dy3m = mover->y - item3->y;
            float distSq3 = dx3m*dx3m + dy3m*dy3m;

            TryFinalApproach(mover, item3->x, item3->y, item3CellX, item3CellY, PICKUP_RADIUS);

            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                SetItemUnreachableCooldown(item3Idx, UNREACHABLE_COOLDOWN);
                return JOBRUN_FAIL;
            }

            if (distSq3 < PICKUP_RADIUS * PICKUP_RADIUS) {
                job->step = CRAFT_STEP_PICKING_UP_INPUT3;
            }
            break;
        }

        case CRAFT_STEP_PICKING_UP_INPUT3: {
            int item3Idx = job->targetItem3;
            if (item3Idx < 0 || !items[item3Idx].active) {
                return JOBRUN_FAIL;
            }
            Item* item3 = &items[item3Idx];

            // Take only what the recipe needs (splits stack if necessary)
            item3Idx = TakeFromStockpileSlot(item3Idx, recipe->inputCount3);
            if (item3Idx < 0) return JOBRUN_FAIL;
            item3 = &items[item3Idx];

            // Extract from container if needed
            if (item3->containedIn != -1) {
                ExtractItemFromContainer(item3Idx);
            }

            item3->state = ITEM_CARRIED;
            job->carryingItem = item3Idx;
            job->targetItem3 = -1;  // Clear target now that we're carrying
            job->step = CRAFT_STEP_CARRYING_INPUT3;
            break;
        }

        case CRAFT_STEP_CARRYING_INPUT3: {
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

            float targetX3 = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
            float targetY3 = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
            float dx3c = mover->x - targetX3;
            float dy3c = mover->y - targetY3;
            float distSq3c = dx3c*dx3c + dy3c*dy3c;

            TryFinalApproach(mover, targetX3, targetY3, ws->workTileX, ws->workTileY, PICKUP_RADIUS);

            if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
                return JOBRUN_FAIL;
            }

            if (distSq3c < PICKUP_RADIUS * PICKUP_RADIUS) {
                // Deposit third input at workshop
                if (job->carryingItem >= 0 && items[job->carryingItem].active) {
                    Item* carried = &items[job->carryingItem];
                    carried->state = ITEM_ON_GROUND;
                    carried->x = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                    carried->y = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                    carried->z = (float)ws->z;
                }
                // Store third input in targetItem3 for consumption
                job->targetItem3 = job->carryingItem;
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

            // Take 1 fuel unit (splits stack if necessary)
            fuelIdx = TakeFromStockpileSlot(fuelIdx, 1);
            if (fuelIdx < 0) return JOBRUN_FAIL;
            fuelItem = &items[fuelIdx];
            job->fuelItem = fuelIdx;

            // Extract from container if needed
            if (fuelItem->containedIn != -1) {
                ExtractItemFromContainer(fuelIdx);
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
            job->progress += dt / GameHoursToGameSeconds(job->workRequired);
            mover->timeWithoutProgress = 0.0f;
            ws->lastWorkTime = (float)gameTime;

            // Fire-based workshops emit smoke and light while working
            bool emitsFire = (ws->type == WORKSHOP_KILN ||
                              ws->type == WORKSHOP_CHARCOAL_PIT ||
                              ws->type == WORKSHOP_HEARTH);
            if (emitsFire && ws->fuelTileX >= 0) {
                if (GetRandomValue(0, 3) == 0) {
                    AddSmoke(ws->fuelTileX, ws->fuelTileY, ws->z, 5);
                }
                AddLightSource(ws->fuelTileX, ws->fuelTileY, ws->z, 255, 140, 50, 8);
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

                // Consume third input item (if any)
                if (job->targetItem3 >= 0 && items[job->targetItem3].active) {
                    DeleteItem(job->targetItem3);
                }
                job->targetItem3 = -1;

                // Consume fuel item (if any - don't preserve its material)
                if (job->fuelItem >= 0 && items[job->fuelItem].active) {
                    DeleteItem(job->fuelItem);
                }
                job->fuelItem = -1;

                // Spawn output items at workshop output tile
                float outX = ws->outputTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                float outY = ws->outputTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                if (ws->type == WORKSHOP_BUTCHER) {
                    // Butcher workshop: use yield table instead of recipe output
                    const ButcherYieldDef* yield = GetButcherYield(inputMat);
                    for (int yi = 0; yi < yield->productCount; yi++) {
                        int outIdx = SpawnItem(outX, outY, (float)ws->z, yield->products[yi].type);
                        if (outIdx >= 0) items[outIdx].stackCount = yield->products[yi].count;
                    }
                } else {
                    {
                        uint8_t outMat;
                        if (ItemTypeUsesMaterialName(recipe->outputType) && inputMat != MAT_NONE) {
                            outMat = (uint8_t)inputMat;
                        } else {
                            outMat = DefaultMaterialForItemType(recipe->outputType);
                        }
                        int outIdx = SpawnItemWithMaterial(outX, outY, (float)ws->z, recipe->outputType, outMat);
                        if (outIdx >= 0) items[outIdx].stackCount = recipe->outputCount;
                    }
                    // Spawn second output if recipe has one (e.g., Strip Bark -> stripped log + bark)
                    if (recipe->outputType2 != ITEM_NONE) {
                        uint8_t outMat2;
                        if (ItemTypeUsesMaterialName(recipe->outputType2) && inputMat != MAT_NONE) {
                            outMat2 = (uint8_t)inputMat;
                        } else {
                            outMat2 = DefaultMaterialForItemType(recipe->outputType2);
                        }
                        int outIdx2 = SpawnItemWithMaterial(outX, outY, (float)ws->z, recipe->outputType2, outMat2);
                        if (outIdx2 >= 0) items[outIdx2].stackCount = recipe->outputCount2;
                    }
                }

                // Auto-suspend bill if output storage is now full
                if (ws->type != WORKSHOP_BUTCHER && recipe->outputType != ITEM_NONE) {
                    int outSlotX, outSlotY;
                    uint8_t outMat = (inputMat != MAT_NONE) ? (uint8_t)inputMat
                        : DefaultMaterialForItemType(recipe->outputType);
                    if (FindStockpileForItem(recipe->outputType, outMat, &outSlotX, &outSlotY) < 0 ||
                        (recipe->outputType2 != ITEM_NONE &&
                         FindStockpileForItem(recipe->outputType2, outMat, &outSlotX, &outSlotY) < 0)) {
                        bill->suspended = true;
                        bill->suspendedNoStorage = true;
                    }
                }

                // Update bill progress
                bill->completedCount++;

                // Remove workshop fire light
                if (emitsFire && ws->fuelTileX >= 0) {
                    RemoveLightSource(ws->fuelTileX, ws->fuelTileY, ws->z);
                }

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
            // Take 1 unit (splits stack if necessary)
            itemIdx = TakeFromStockpileSlot(itemIdx, 1);
            if (itemIdx < 0) return JOBRUN_FAIL;
            item = &items[itemIdx];

            // Extract from container if needed
            if (item->containedIn != -1) {
                ExtractItemFromContainer(itemIdx);
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

        // Update carried item position (including container contents)
        items[itemIdx].x = mover->x;
        items[itemIdx].y = mover->y;
        items[itemIdx].z = mover->z;
        if (items[itemIdx].contentCount > 0) MoveContainer(itemIdx, mover->x, mover->y, mover->z);

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
        job->progress += dt / GameHoursToGameSeconds(job->workRequired);
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

// Clean floor job driver: walk to dirty floor -> clean it
JobRunResult RunJob_Clean(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CLEAN) return JOBRUN_FAIL;
    if (GetFloorDirt(tx, ty, tz) < DIRT_CLEAN_THRESHOLD) {
        CompleteCleanDesignation(tx, ty, tz);
        return JOBRUN_DONE;
    }
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, CLEAN_WORK_TIME, true, 1.0f);
        if (r == JOBRUN_DONE) CompleteCleanDesignation(tx, ty, tz);
        return r;
    }
    return JOBRUN_FAIL;
}

// Deconstruct workshop driver: walk to work tile, tear down, refund materials
JobRunResult RunJob_DeconstructWorkshop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int wsIdx = job->targetWorkshop;

    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS || !workshops[wsIdx].active) {
        return JOBRUN_FAIL;
    }

    Workshop* ws = &workshops[wsIdx];

    if (job->step == STEP_MOVING_TO_WORK) {
        int moverCellX = (int)(mover->x / CELL_SIZE);
        int moverCellY = (int)(mover->y / CELL_SIZE);
        int moverCellZ = (int)mover->z;

        bool atWorkTile = (moverCellX == ws->workTileX && moverCellY == ws->workTileY && moverCellZ == ws->z);
        bool adjacent = (moverCellZ == ws->z &&
            ((abs(moverCellX - ws->workTileX) + abs(moverCellY - ws->workTileY)) == 1));

        if (IsPathExhausted(mover) && !atWorkTile && !adjacent) {
            float goalX = mover->goal.x * CELL_SIZE + CELL_SIZE * 0.5f;
            float goalY = mover->goal.y * CELL_SIZE + CELL_SIZE * 0.5f;
            TryFinalApproach(mover, goalX, goalY, mover->goal.x, mover->goal.y, PICKUP_RADIUS);
        }

        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            return JOBRUN_FAIL;
        }

        if (atWorkTile || adjacent) {
            job->step = STEP_WORKING;
            job->progress = 0.0f;
        }

        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        if (ws->assignedDeconstructor != job->assignedMover) {
            return JOBRUN_FAIL;
        }

        mover->timeWithoutProgress = 0.0f;
        job->progress += dt;

        if (job->progress >= job->workRequired) {
            // Deconstruction complete — refund materials
            int recipeIdx = GetConstructionRecipeForWorkshopType(ws->type);
            if (recipeIdx >= 0) {
                const ConstructionRecipe* recipe = GetConstructionRecipe(recipeIdx);
                if (recipe) {
                    for (int s = 0; s < recipe->stageCount; s++) {
                        const ConstructionStage* stage = &recipe->stages[s];
                        for (int inp = 0; inp < stage->inputCount; inp++) {
                            const ConstructionInput* input = &stage->inputs[inp];
                            ItemType refundType = input->alternatives[0].itemType;
                            for (int c = 0; c < input->count; c++) {
                                if ((GetRandomValue(0, 99)) < CONSTRUCTION_REFUND_CHANCE) {
                                    float sx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
                                    float sy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
                                    SpawnItem(sx, sy, (float)ws->z, refundType);
                                }
                            }
                        }
                    }
                }
            }

            EventLog("Workshop %d (%s) deconstructed by mover %d at (%d,%d,z%d)",
                     wsIdx, workshopDefs[ws->type].displayName, job->assignedMover,
                     ws->x, ws->y, ws->z);

            ws->assignedDeconstructor = -1;
            ws->markedForDeconstruct = false;
            // Clear targetWorkshop before DeleteWorkshop so the job cancellation
            // loop inside DeleteWorkshop won't try to cancel this completing job
            job->targetWorkshop = -1;
            DeleteWorkshop(wsIdx);
            return JOBRUN_DONE;
        }

        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Hunt job steps
#define HUNT_STEP_CHASING  0
#define HUNT_STEP_ATTACKING 1
#define HUNT_CHASE_TIMEOUT 30.0f  // Fail if can't reach animal in 30s
#define HUNT_RETARGET_INTERVAL 1.0f  // Re-path every 1s while chasing

// Hunt job driver: chase animal -> attack when adjacent -> kill
JobRunResult RunJob_Hunt(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int animalIdx = job->targetAnimalIdx;

    // Validate target animal
    if (animalIdx < 0 || animalIdx >= animalCount || !animals[animalIdx].active) {
        return JOBRUN_FAIL;  // Animal died or was removed
    }

    Animal* target = &animals[animalIdx];
    int animalCellX = (int)(target->x / CELL_SIZE);
    int animalCellY = (int)(target->y / CELL_SIZE);
    int animalCellZ = (int)target->z;

    if (job->step == HUNT_STEP_CHASING) {
        // Update goal to animal's current position
        if (mover->goal.x != animalCellX || mover->goal.y != animalCellY || mover->goal.z != animalCellZ) {
            mover->goal = (Point){animalCellX, animalCellY, animalCellZ};
            mover->needsRepath = true;
        }
        // Periodic re-path even if goal cell hasn't changed (animal may have moved within cell)
        if (IsPathExhausted(mover) && mover->timeWithoutProgress > HUNT_RETARGET_INTERVAL) {
            mover->needsRepath = true;
            mover->timeWithoutProgress = 0.0f;
        }

        // Check proximity — adjacent (within CELL_SIZE) triggers attack
        float dx = mover->x - target->x;
        float dy = mover->y - target->y;
        float distSq = dx * dx + dy * dy;
        bool correctZ = (int)mover->z == animalCellZ;

        if (correctZ) {
            float goalX = animalCellX * CELL_SIZE + CELL_SIZE * 0.5f;
            float goalY = animalCellY * CELL_SIZE + CELL_SIZE * 0.5f;
            TryFinalApproach(mover, goalX, goalY, animalCellX, animalCellY, CELL_SIZE);
        }

        if (correctZ && distSq < (float)CELL_SIZE * (float)CELL_SIZE) {
            // Adjacent — start attack
            job->step = HUNT_STEP_ATTACKING;
            job->progress = 0.0f;
            target->state = ANIMAL_BEING_HUNTED;
            target->velX = 0.0f;
            target->velY = 0.0f;
            return JOBRUN_RUNNING;
        }

        // Chase timeout
        if (mover->timeWithoutProgress > HUNT_CHASE_TIMEOUT) {
            return JOBRUN_FAIL;
        }

        return JOBRUN_RUNNING;
    }

    if (job->step == HUNT_STEP_ATTACKING) {
        // Re-validate animal is still alive
        if (!target->active) return JOBRUN_FAIL;

        // Keep animal frozen
        target->state = ANIMAL_BEING_HUNTED;
        target->velX = 0.0f;
        target->velY = 0.0f;

        // Work progress with tool speed
        mover->timeWithoutProgress = 0.0f;
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_HUNT, MAT_NONE, mover->equippedTool);
        job->progress += (dt * speed) / GameHoursToGameSeconds(HUNT_ATTACK_WORK_TIME);

        if (job->progress >= 1.0f) {
            // Kill the animal — spawns carcass
            KillAnimal(animalIdx);
            job->targetAnimalIdx = -1;
            return JOBRUN_DONE;
        }
        return JOBRUN_RUNNING;
    }

    return JOBRUN_FAIL;
}

// Till farm designation job driver: walk to farm cell -> till soil
JobRunResult RunJob_Till(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_FARM) return JOBRUN_FAIL;
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_TILL, MAT_NONE, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, d, mover, dt, TILL_WORK_TIME, true, speed);
        if (r == JOBRUN_DONE) {
            int moverIdx = (int)(mover - movers);
            CompleteFarmDesignation(tx, ty, tz, moverIdx);
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Tend crop job driver: walk to weedy farm cell -> weed it
JobRunResult RunJob_TendCrop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    FarmCell* fc = GetFarmCell(tx, ty, tz);
    if (!fc || !fc->tilled) return JOBRUN_FAIL;
    if (fc->weedLevel < WEED_THRESHOLD) return JOBRUN_DONE;  // weeds already cleared
    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        JobRunResult r = RunWorkProgress(job, NULL, mover, dt, TEND_WORK_TIME, true, 1.0f);
        if (r == JOBRUN_DONE) {
            fc->weedLevel = 0;
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Fertilize job driver: pick up compost -> walk to farm cell -> apply
JobRunResult RunJob_Fertilize(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;

    if (job->step == STEP_MOVING_TO_PICKUP) {
        return RunPickupStep(job, mover, (Point){tx, ty, tz});
    }
    if (job->step == STEP_CARRYING) {
        JobRunResult r = RunCarryStep(job, mover, tx, ty, tz);
        if (r == JOBRUN_DONE) {
            job->step = STEP_PLANTING;
            job->progress = 0.0f;
            return JOBRUN_RUNNING;
        }
        return r;
    }
    if (job->step == STEP_PLANTING) {
        FarmCell* fc = GetFarmCell(tx, ty, tz);
        if (!fc || !fc->tilled) return JOBRUN_FAIL;

        JobRunResult r = RunWorkProgress(job, NULL, mover, dt, FERTILIZE_WORK_TIME, true, 1.0f);
        if (r == JOBRUN_DONE) {
            int newFert = fc->fertility + FERTILIZE_AMOUNT;
            fc->fertility = (newFert > 255) ? 255 : (uint8_t)newFert;
            // Consume compost
            if (job->carryingItem >= 0) {
                DeleteItem(job->carryingItem);
                job->carryingItem = -1;
            }
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Plant crop job driver: pick up seed -> carry to tilled cell -> plant
JobRunResult RunJob_PlantCrop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;

    if (job->step == STEP_MOVING_TO_PICKUP) {
        // Split off 1 seed from stack so mover only carries 1
        int seedIdx = job->targetItem;
        if (seedIdx >= 0 && items[seedIdx].active && items[seedIdx].stackCount > 1) {
            int singleIdx = SplitStack(seedIdx, 1);
            if (singleIdx >= 0) {
                ReleaseItemReservation(seedIdx);
                ReserveItem(singleIdx, (int)(mover - movers));
                job->targetItem = singleIdx;
            }
        }
        return RunPickupStep(job, mover, (Point){tx, ty, tz});
    }
    if (job->step == STEP_CARRYING) {
        JobRunResult r = RunCarryStep(job, mover, tx, ty, tz);
        if (r == JOBRUN_DONE) {
            job->step = STEP_PLANTING;
            job->progress = 0.0f;
            return JOBRUN_RUNNING;
        }
        return r;
    }
    if (job->step == STEP_PLANTING) {
        FarmCell* fc = GetFarmCell(tx, ty, tz);
        if (!fc || !fc->tilled || fc->cropType != CROP_NONE) return JOBRUN_FAIL;

        JobRunResult r = RunWorkProgress(job, NULL, mover, dt, PLANT_CROP_WORK_TIME, true, 1.0f);
        if (r == JOBRUN_DONE) {
            int itemIdx = job->carryingItem;
            if (itemIdx >= 0 && items[itemIdx].active) {
                CropType crop = CropTypeForSeed(items[itemIdx].type);
                if (crop == CROP_NONE) return JOBRUN_FAIL;
                fc->cropType = (uint8_t)crop;
                fc->growthStage = CROP_STAGE_SPROUTED;  // Start at sprouted
                fc->growthProgress = 0;
                fc->frostDamaged = 0;
                DeleteItem(itemIdx);
                job->carryingItem = -1;
                EventLog("Planted crop %d at (%d,%d,z%d)", crop, tx, ty, tz);
            } else {
                return JOBRUN_FAIL;
            }
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// Harvest crop job driver: walk to ripe cell -> harvest -> spawn items
JobRunResult RunJob_HarvestCrop(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;

    FarmCell* fc = GetFarmCell(tx, ty, tz);
    if (!fc || !fc->tilled || fc->growthStage != CROP_STAGE_RIPE) return JOBRUN_FAIL;

    if (job->step == STEP_MOVING_TO_WORK) return RunWalkToTileStep(job, mover);
    if (job->step == STEP_WORKING) {
        float speed = GetJobToolSpeedMultiplier(JOBTYPE_HARVEST_CROP, MAT_NONE, mover->equippedTool);
        JobRunResult r = RunWorkProgress(job, NULL, mover, dt, HARVEST_CROP_WORK_TIME, true, speed);
        if (r == JOBRUN_DONE) {
            CropType crop = (CropType)fc->cropType;
            bool frost = fc->frostDamaged;
            float px = tx * CELL_SIZE + CELL_SIZE * 0.5f;
            float py = ty * CELL_SIZE + CELL_SIZE * 0.5f;

            // Spawn yield items based on crop type
            // Normal yields / frost yields:
            //   Wheat:   4/2 ITEM_WHEAT + 1 seed
            //   Lentils: 3/1 ITEM_LENTILS + 1 seed
            //   Flax:    2/1 ITEM_FLAX_FIBER + 1 seed
            int yieldType = ITEM_NONE;
            int seedType = ITEM_NONE;
            int yieldCount = 0;
            int fertDelta = 0;

            switch (crop) {
                case CROP_WHEAT:
                    yieldType = ITEM_WHEAT;
                    seedType = ITEM_WHEAT_SEEDS;
                    yieldCount = frost ? 2 : 4;
                    fertDelta = WHEAT_FERTILITY_DELTA;
                    break;
                case CROP_LENTILS:
                    yieldType = ITEM_LENTILS;
                    seedType = ITEM_LENTIL_SEEDS;
                    yieldCount = frost ? 1 : 3;
                    fertDelta = LENTIL_FERTILITY_DELTA;
                    break;
                case CROP_FLAX:
                    yieldType = ITEM_FLAX_FIBER;
                    seedType = ITEM_FLAX_SEEDS;
                    yieldCount = frost ? 1 : 2;
                    fertDelta = FLAX_FERTILITY_DELTA;
                    break;
                default:
                    break;
            }

            // Spawn yield
            for (int i = 0; i < yieldCount; i++) {
                SpawnItem(px, py, (float)tz, yieldType);
            }
            // Always spawn 1 seed (self-sustaining)
            if (seedType != ITEM_NONE) {
                SpawnItem(px, py, (float)tz, seedType);
            }

            // Apply fertility delta
            int newFert = (int)fc->fertility + fertDelta;
            if (newFert < 0) newFert = 0;
            if (newFert > 255) newFert = 255;
            fc->fertility = (uint8_t)newFert;

            // Reset cell (stays tilled)
            fc->cropType = CROP_NONE;
            fc->growthStage = CROP_STAGE_BARE;
            fc->growthProgress = 0;
            fc->frostDamaged = 0;

            EventLog("Harvested crop %d at (%d,%d,z%d) yield=%d frost=%d", crop, tx, ty, tz, yieldCount, frost);
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// EquipClothing job driver: walk to clothing item -> pick up and equip
JobRunResult RunJob_EquipClothing(Job* job, void* moverPtr, float dt) {
    (void)dt;
    Mover* mover = (Mover*)moverPtr;
    int moverIdx = (int)(mover - movers);
    int itemIdx = job->targetItem;

    if (itemIdx < 0 || !items[itemIdx].active) return JOBRUN_FAIL;
    Item* clothing = &items[itemIdx];

    if (job->step == STEP_MOVING_TO_PICKUP) {
        // Walk to item
        int itemCellX = (int)(clothing->x / CELL_SIZE);
        int itemCellY = (int)(clothing->y / CELL_SIZE);

        TryFinalApproach(mover, clothing->x, clothing->y, itemCellX, itemCellY, PICKUP_RADIUS);

        if (IsPathExhausted(mover) && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
            return JOBRUN_FAIL;
        }

        float dx = mover->x - clothing->x;
        float dy = mover->y - clothing->y;
        if (dx * dx + dy * dy < PICKUP_RADIUS * PICKUP_RADIUS) {
            // Drop old clothing if any
            if (mover->equippedClothing >= 0) {
                DropEquippedClothing(moverIdx);
            }

            // Clear stockpile slot if clothing was in stockpile
            if (clothing->state == ITEM_IN_STOCKPILE) {
                ClearSourceStockpileSlot(clothing);
            }

            // Equip the clothing
            clothing->state = ITEM_CARRIED;
            clothing->reservedBy = moverIdx;
            clothing->x = mover->x;
            clothing->y = mover->y;
            clothing->z = mover->z;
            mover->equippedClothing = itemIdx;

            EventLog("Mover %d equipped clothing item %d (%s)", moverIdx, itemIdx,
                     itemDefs[clothing->type].name);
            return JOBRUN_DONE;
        }
        return JOBRUN_RUNNING;
    }
    return JOBRUN_FAIL;
}

// WorkGiver_EquipClothing: Find clothing for a mover to equip
// Returns job ID if successful, -1 if no job available
int WorkGiver_EquipClothing(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Current clothing reduction (0.0 if naked)
    float currentReduction = 0.0f;
    if (m->equippedClothing >= 0 && m->equippedClothing < MAX_ITEMS
        && items[m->equippedClothing].active) {
        currentReduction = GetClothingCoolingReduction(items[m->equippedClothing].type);
    }

    int moverCellX = (int)(m->x / CELL_SIZE);
    int moverCellY = (int)(m->y / CELL_SIZE);
    int moverZ = (int)m->z;

    // Find best available clothing item (highest cooling reduction)
    // Must be better than current by at least 0.1 to trigger upgrade
    float minReduction = currentReduction + 0.1f;
    int bestIdx = -1;
    float bestReduction = 0.0f;
    int bestDistSq = 999999;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (!ItemIsClothing(items[i].type)) continue;
        if (items[i].reservedBy >= 0) continue;
        if (items[i].state != ITEM_ON_GROUND && items[i].state != ITEM_IN_STOCKPILE) continue;
        if ((int)items[i].z != moverZ) continue;

        float reduction = GetClothingCoolingReduction(items[i].type);
        if (reduction < minReduction) continue;

        int ix = (int)(items[i].x / CELL_SIZE);
        int iy = (int)(items[i].y / CELL_SIZE);
        int dx = ix - moverCellX;
        int dy = iy - moverCellY;
        int distSq = dx * dx + dy * dy;

        // Prefer higher reduction, then closer distance
        if (reduction > bestReduction || (reduction == bestReduction && distSq < bestDistSq)) {
            bestIdx = i;
            bestReduction = reduction;
            bestDistSq = distSq;
        }
    }

    if (bestIdx < 0) return -1;

    // Create the job
    int jobId = CreateJob(JOBTYPE_EQUIP_CLOTHING);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->targetItem = bestIdx;
    job->step = STEP_MOVING_TO_PICKUP;
    items[bestIdx].reservedBy = moverIdx;

    // Assign to mover
    job->assignedMover = moverIdx;
    m->currentJobId = jobId;
    RemoveMoverFromIdleList(moverIdx);

    // Set goal toward item
    int itemCellX = (int)(items[bestIdx].x / CELL_SIZE);
    int itemCellY = (int)(items[bestIdx].y / CELL_SIZE);
    m->goal = (Point){itemCellX, itemCellY, moverZ};
    m->needsRepath = true;

    EventLog("WorkGiver_EquipClothing: mover %d -> item %d (%s)", moverIdx, bestIdx,
             itemDefs[items[bestIdx].type].name);
    return jobId;
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
    [JOBTYPE_GATHER_TREE] = RunJob_GatherTree,
    [JOBTYPE_DELIVER_TO_WORKSHOP] = RunJob_DeliverToWorkshop,
    [JOBTYPE_IGNITE_WORKSHOP] = RunJob_IgniteWorkshop,
    [JOBTYPE_CLEAN] = RunJob_Clean,
    [JOBTYPE_HARVEST_BERRY] = RunJob_HarvestBerry,
    [JOBTYPE_KNAP] = RunJob_Knap,
    [JOBTYPE_DECONSTRUCT_WORKSHOP] = RunJob_DeconstructWorkshop,
    [JOBTYPE_HUNT] = RunJob_Hunt,
    [JOBTYPE_DIG_ROOTS] = RunJob_DigRoots,
    [JOBTYPE_EXPLORE] = RunJob_Explore,
    [JOBTYPE_TILL] = RunJob_Till,
    [JOBTYPE_TEND_CROP] = RunJob_TendCrop,
    [JOBTYPE_FERTILIZE] = RunJob_Fertilize,
    [JOBTYPE_PLANT_CROP] = RunJob_PlantCrop,
    [JOBTYPE_HARVEST_CROP] = RunJob_HarvestCrop,
    [JOBTYPE_EQUIP_CLOTHING] = RunJob_EquipClothing,
};

// Compile-time check: ensure jobDrivers[] covers all job types
_Static_assert(sizeof(jobDrivers) / sizeof(jobDrivers[0]) > JOBTYPE_EQUIP_CLOTHING,
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
            EventLog("Job %d DONE type=%s mover=%d", m->currentJobId, JobTypeName(job->type), i);
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
            EventLog("Job %d FAIL type=%s mover=%d step=%d", m->currentJobId, JobTypeName(job->type), i, job->step);
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
        if (movers[i].active && movers[i].currentJobId < 0 && movers[i].freetimeState == FREETIME_NONE) {
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
    int itemZ;              // Z-level of item (unused for filtering, kept for context)
    int bestMoverIdx;
    float bestDistSq;
    bool requireCanHaul;
    bool requireCanMine;
    bool requireCanBuild;
    int excludeMovers[3];   // Movers to skip (failed pathfinding), -1 = unused
    int excludeCount;
} IdleMoverSearchContext;

static void IdleMoverSearchCallback(int moverIdx, float distSq, void* userData) {
    IdleMoverSearchContext* ctx = (IdleMoverSearchContext*)userData;

    // Skip if not in idle list (already has a job or not active)
    if (!moverIsInIdleList || !moverIsInIdleList[moverIdx]) return;

    // Skip excluded movers (failed pathfinding on previous attempts)
    for (int i = 0; i < ctx->excludeCount; i++) {
        if (ctx->excludeMovers[i] == moverIdx) return;
    }

    Mover* m = &movers[moverIdx];

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

// Extract item from container before pickup (instant, no work timer).
// Updates stockpile slotCounts if container is in a stockpile slot.
static void ExtractItemFromContainer(int itemIdx) {
    int parentContainer = items[itemIdx].containedIn;
    RemoveItemFromContainer(itemIdx);
    SyncStockpileContainerSlotCount(parentContainer);
}

// Helper: safe-drop an item near a mover at a walkable cell
// Searches 8 neighbors (cardinal + diagonal) if mover position is not walkable
// Wrapper: drop item near mover using shared SafeDropItem from items.c
static void SafeDropItemNearMover(int itemIdx, Mover* m) {
    SafeDropItem(itemIdx, m->x, m->y, (int)m->z);
    // Move container contents to the drop position
    if (itemIdx >= 0 && itemIdx < MAX_ITEMS && items[itemIdx].active && items[itemIdx].contentCount > 0) {
        MoveContainer(itemIdx, items[itemIdx].x, items[itemIdx].y, items[itemIdx].z);
    }
}

// Cancel job and release all reservations (public, called from workshops.c and internally)
void CancelJob(void* moverPtr, int moverIdx) {
    Mover* m = (Mover*)moverPtr;
    // Get job data (all job-specific data now comes from Job struct)
    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    if (job) {
        PROFILE_COUNT(jobs_cancelled, 1);
        EventLog("CancelJob %d type=%s mover=%d item=%d stockpile=%d blueprint=%d",
                 m->currentJobId, JobTypeName(job->type), moverIdx,
                 job->targetItem, job->targetStockpile, job->targetBlueprint);
        // Release item reservation
        if (job->targetItem >= 0) {
            ReleaseItemReservation(job->targetItem);
        }

        // Release stockpile slot reservation
        if (job->targetStockpile >= 0) {
            ReleaseStockpileSlot(job->targetStockpile, job->targetSlotX, job->targetSlotY);
        }

        // If carrying, safe-drop the item at a walkable cell
        SafeDropItemNearMover(job->carryingItem, m);

        // Release animal hunt reservation
        if (job->type == JOBTYPE_HUNT && job->targetAnimalIdx >= 0 && job->targetAnimalIdx < animalCount) {
            Animal* huntTarget = &animals[job->targetAnimalIdx];
            if (huntTarget->active && huntTarget->reservedByHunter == moverIdx) {
                huntTarget->reservedByHunter = -1;
                if (huntTarget->state == ANIMAL_BEING_HUNTED) {
                    huntTarget->state = ANIMAL_IDLE;
                    huntTarget->stateTimer = 0.0f;
                }
            }
        }

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
                // Decrement reservedCount for the cancelled item's slot
                // Note: don't check items[].active — item may have been deleted
                // but reservedCount was incremented at job creation and must be balanced
                if (job->targetItem >= 0) {
                    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
                    if (recipe) {
                        const ConstructionStage* stage = &recipe->stages[bp->stage];
                        ItemType itemType = items[job->targetItem].type;
                        bool decremented = false;
                        for (int s = 0; s < stage->inputCount; s++) {
                            StageDelivery* sd = &bp->stageDeliveries[s];
                            if (sd->reservedCount <= 0) continue;
                            if (!ConstructionInputAcceptsItem(&stage->inputs[s], itemType)) continue;
                            sd->reservedCount--;
                            decremented = true;
                            break;
                        }
                        if (!decremented) {
                            EventLog("WARNING: CancelJob bp %d slot reservedCount NOT decremented! item=%d type=%s active=%d",
                                     job->targetBlueprint, job->targetItem,
                                     items[job->targetItem].active ? ItemName(itemType) : "DELETED",
                                     items[job->targetItem].active);
                        }
                    }
                }
                // If we were building, release the builder assignment
                if (bp->assignedBuilder == moverIdx) {
                    bp->assignedBuilder = -1;
                    bp->state = BLUEPRINT_READY_TO_BUILD;  // Revert to ready state
                    bp->progress = 0.0f;  // Reset progress
                    EventLog("Blueprint %d at (%d,%d,z%d) -> READY_TO_BUILD (build cancelled by mover %d)",
                             job->targetBlueprint, bp->x, bp->y, bp->z, moverIdx);
                }
            }
        }

        // Release second input item reservation (for multi-input craft jobs)
        if (job->targetItem2 >= 0 && items[job->targetItem2].active) {
            // Only safe-drop if carried; otherwise just release reservation
            if (items[job->targetItem2].state == ITEM_CARRIED) {
                SafeDropItemNearMover(job->targetItem2, m);
            } else {
                items[job->targetItem2].reservedBy = -1;
            }
        }

        // Release third input item reservation (for tri-input craft jobs)
        if (job->targetItem3 >= 0 && items[job->targetItem3].active) {
            if (items[job->targetItem3].state == ITEM_CARRIED) {
                SafeDropItemNearMover(job->targetItem3, m);
            } else {
                items[job->targetItem3].reservedBy = -1;
            }
        }

        // Release fuel item reservation (for craft jobs with fuel)
        if (job->fuelItem >= 0 && items[job->fuelItem].active) {
            // Only safe-drop if carried; otherwise just release reservation
            if (items[job->fuelItem].state == ITEM_CARRIED) {
                SafeDropItemNearMover(job->fuelItem, m);
            } else {
                items[job->fuelItem].reservedBy = -1;
            }
        }

        // Release tool item reservation (if mover hasn't picked it up yet)
        if (job->toolItem >= 0 && items[job->toolItem].active) {
            if (m->equippedTool != job->toolItem) {
                items[job->toolItem].reservedBy = -1;
            }
        }

        // Release workshop reservation (for craft jobs)
        if (job->targetWorkshop >= 0 && job->targetWorkshop < MAX_WORKSHOPS) {
            Workshop* ws = &workshops[job->targetWorkshop];
            if (ws->active) {
                if (ws->assignedCrafter == moverIdx) {
                    ws->assignedCrafter = -1;
                }
                if (job->type == JOBTYPE_DECONSTRUCT_WORKSHOP && ws->assignedDeconstructor == moverIdx) {
                    ws->assignedDeconstructor = -1;
                }
                // Remove fire light if workshop was actively burning
                bool isFire = (ws->type == WORKSHOP_KILN ||
                               ws->type == WORKSHOP_CHARCOAL_PIT ||
                               ws->type == WORKSHOP_HEARTH);
                if (isFire && ws->fuelTileX >= 0) {
                    RemoveLightSource(ws->fuelTileX, ws->fuelTileY, ws->z);
                }
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

void UnassignJob(void* moverPtr, int moverIdx) {
    Mover* m = (Mover*)moverPtr;
    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    if (job) {
        EventLog("UnassignJob %d type=%s mover=%d", m->currentJobId, JobTypeName(job->type), moverIdx);
        // Release item reservation
        if (job->targetItem >= 0) {
            ReleaseItemReservation(job->targetItem);
        }

        // Release stockpile slot reservation
        if (job->targetStockpile >= 0) {
            ReleaseStockpileSlot(job->targetStockpile, job->targetSlotX, job->targetSlotY);
        }

        // If carrying, safe-drop the item at a walkable cell
        SafeDropItemNearMover(job->carryingItem, m);

        // Release animal hunt reservation
        if (job->type == JOBTYPE_HUNT && job->targetAnimalIdx >= 0 && job->targetAnimalIdx < animalCount) {
            Animal* huntTarget = &animals[job->targetAnimalIdx];
            if (huntTarget->active && huntTarget->reservedByHunter == moverIdx) {
                huntTarget->reservedByHunter = -1;
                if (huntTarget->state == ANIMAL_BEING_HUNTED) {
                    huntTarget->state = ANIMAL_IDLE;
                    huntTarget->stateTimer = 0.0f;
                }
            }
        }

        // Release mine designation reservation — but PRESERVE progress
        if (job->targetMineX >= 0 && job->targetMineY >= 0 && job->targetMineZ >= 0) {
            Designation* d = GetDesignation(job->targetMineX, job->targetMineY, job->targetMineZ);
            if (d && d->assignedMover == moverIdx) {
                d->assignedMover = -1;
                // NOTE: do NOT reset d->progress — this is the key difference from CancelJob
                InvalidateDesignationCache(d->type);
            }
        }

        // Release blueprint reservation
        if (job->targetBlueprint >= 0 && job->targetBlueprint < MAX_BLUEPRINTS) {
            Blueprint* bp = &blueprints[job->targetBlueprint];
            if (bp->active) {
                // Note: don't check items[].active — item may have been deleted
                // but reservedCount was incremented at job creation and must be balanced
                if (job->targetItem >= 0) {
                    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
                    if (recipe) {
                        const ConstructionStage* stage = &recipe->stages[bp->stage];
                        ItemType itemType = items[job->targetItem].type;
                        bool decremented = false;
                        for (int s = 0; s < stage->inputCount; s++) {
                            StageDelivery* sd = &bp->stageDeliveries[s];
                            if (sd->reservedCount <= 0) continue;
                            if (!ConstructionInputAcceptsItem(&stage->inputs[s], itemType)) continue;
                            sd->reservedCount--;
                            decremented = true;
                            break;
                        }
                        if (!decremented) {
                            EventLog("WARNING: UnassignJob bp %d slot reservedCount NOT decremented! item=%d type=%s active=%d",
                                     job->targetBlueprint, job->targetItem,
                                     items[job->targetItem].active ? ItemName(itemType) : "DELETED",
                                     items[job->targetItem].active);
                        }
                    }
                }
                if (bp->assignedBuilder == moverIdx) {
                    bp->assignedBuilder = -1;
                    bp->state = BLUEPRINT_READY_TO_BUILD;
                    bp->progress = 0.0f;
                    EventLog("Blueprint %d at (%d,%d,z%d) -> READY_TO_BUILD (unassigned mover %d)",
                             job->targetBlueprint, bp->x, bp->y, bp->z, moverIdx);
                }
            }
        }

        // Release second input item reservation
        if (job->targetItem2 >= 0 && items[job->targetItem2].active) {
            if (items[job->targetItem2].state == ITEM_CARRIED) {
                SafeDropItemNearMover(job->targetItem2, m);
            } else {
                items[job->targetItem2].reservedBy = -1;
            }
        }

        // Release third input item reservation
        if (job->targetItem3 >= 0 && items[job->targetItem3].active) {
            if (items[job->targetItem3].state == ITEM_CARRIED) {
                SafeDropItemNearMover(job->targetItem3, m);
            } else {
                items[job->targetItem3].reservedBy = -1;
            }
        }

        // Release fuel item reservation
        if (job->fuelItem >= 0 && items[job->fuelItem].active) {
            if (items[job->fuelItem].state == ITEM_CARRIED) {
                SafeDropItemNearMover(job->fuelItem, m);
            } else {
                items[job->fuelItem].reservedBy = -1;
            }
        }

        // Release tool item reservation (if mover hasn't picked it up yet)
        if (job->toolItem >= 0 && items[job->toolItem].active) {
            if (m->equippedTool != job->toolItem) {
                items[job->toolItem].reservedBy = -1;
            }
        }

        // Release workshop reservation
        if (job->targetWorkshop >= 0 && job->targetWorkshop < MAX_WORKSHOPS) {
            Workshop* ws = &workshops[job->targetWorkshop];
            if (ws->active) {
                if (ws->assignedCrafter == moverIdx) {
                    ws->assignedCrafter = -1;
                }
                bool isFire = (ws->type == WORKSHOP_KILN ||
                               ws->type == WORKSHOP_CHARCOAL_PIT ||
                               ws->type == WORKSHOP_HEARTH);
                if (isFire && ws->fuelTileX >= 0) {
                    RemoveLightSource(ws->fuelTileX, ws->fuelTileY, ws->z);
                }
            }
        }

        // Release Job entry (WorkGiver will create a new one from the designation)
        ReleaseJob(m->currentJobId);
    }

    // Reset mover state
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
    Point itemCell = { (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z };

    // Try up to 3 movers: if the nearest can't path, try the next-nearest.
    // Only set unreachableCooldown if ALL candidates fail.
    #define MAX_MOVER_RETRIES 3
    int excludeMovers[MAX_MOVER_RETRIES];
    int excludeCount = 0;

    for (int attempt = 0; attempt < MAX_MOVER_RETRIES; attempt++) {
        int moverIdx = -1;

        // Use MoverSpatialGrid if available and built
        if (moverGrid.cellCounts && moverGrid.cellStarts[moverGrid.cellCount] > 0) {
            IdleMoverSearchContext ctx = {
                .itemX = item->x,
                .itemY = item->y,
                .itemZ = (int)item->z,
                .bestMoverIdx = -1,
                .bestDistSq = 1e30f,
                .requireCanHaul = true,
                .requireCanMine = false,
                .requireCanBuild = false,
                .excludeMovers = {-1, -1, -1},
                .excludeCount = excludeCount
            };
            for (int e = 0; e < excludeCount; e++) ctx.excludeMovers[e] = excludeMovers[e];

            QueryMoverNeighbors(item->x, item->y, MOVER_SEARCH_RADIUS, -1, IdleMoverSearchCallback, &ctx);
            moverIdx = ctx.bestMoverIdx;
        } else {
            // Fallback: find nearest idle mover (for tests without spatial grid)
            float bestDistSq = 1e30f;
            for (int i = 0; i < idleMoverCount; i++) {
                int idx = idleMoverList[i];
                // Skip excluded movers
                bool excluded = false;
                for (int e = 0; e < excludeCount; e++) {
                    if (excludeMovers[e] == idx) { excluded = true; break; }
                }
                if (excluded) continue;
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

        if (moverIdx < 0) break;  // No more candidate movers
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
        Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };

        PROFILE_ACCUM_BEGIN(Jobs_ReachabilityCheck);
        Point tempPath[MAX_PATH];
        PROFILE_COUNT(pathfinds, 1);
        int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
        PROFILE_ACCUM_END(Jobs_ReachabilityCheck);

        if (tempLen == 0) {
            // This mover can't reach — release and try next
            ReleaseItemReservation(itemIdx);
            if (!safeDrop) {
                ReleaseStockpileSlot(spIdx, slotX, slotY);
            }
            excludeMovers[excludeCount++] = moverIdx;
            continue;
        }

        // Success — create job
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

        m->goal = itemCell;
        m->needsRepath = true;
        RemoveMoverFromIdleList(moverIdx);
        return true;
    }

    // All candidate movers failed to path — mark item unreachable
    if (excludeCount > 0) {
        SetItemUnreachableCooldown(itemIdx, UNREACHABLE_COOLDOWN);
    }
    return false;
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
    if (!IsExplored(cellX, cellY, cellZ)) return false;
    if (IsPassiveWorkshopWorkTile(cellX, cellY, cellZ)) return false;
    return true;
}

// Extracted priority sections as noinline functions so they appear
// individually in Instruments / profiler call trees.

__attribute__((noinline))
static void AssignJobs_P1_StockpileMaintenance(void) {
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

            Stockpile* sp = &stockpiles[spOnItem];
            int lx = slotX - sp->x;
            int ly = slotY - sp->y;
            int idx = ly * sp->width + lx;
            if (sp->slotCounts[idx] + sp->reservedBy[idx] >= sp->maxStackSize) {
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
            InvalidateStockpileSlotCache(items[itemIdx].type, items[itemIdx].material);
        }
    }
}

__attribute__((noinline))
static void AssignJobs_P2_Crafting(void) {
    int workshopsNeedingCrafters = 0;
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->assignedCrafter >= 0) continue;
        if (ws->billCount > 0) workshopsNeedingCrafters++;
    }

    if (workshopsNeedingCrafters > 0) {
        int moversToCheck = workshopsNeedingCrafters;
        if (moversToCheck > idleMoverCount) moversToCheck = idleMoverCount;

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

__attribute__((noinline))
static void AssignJobs_P2b_PassiveDelivery(void) {
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

__attribute__((noinline))
static void AssignJobs_P2c_Ignition(void) {
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

__attribute__((noinline))
static void AssignJobs_P3_ItemHaul(bool typeMatHasStockpile[][MAT_COUNT]) {
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        int totalIndexed = itemGrid.cellStarts[itemGrid.cellCount];
        PROFILE_COUNT(items_scanned, totalIndexed);

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
        PROFILE_COUNT(items_scanned, itemHighWaterMark);
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

__attribute__((noinline))
static void AssignJobs_P3c_Rehaul(void) {
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

        bool noLongerAllowed = !StockpileAcceptsItem(currentSp, items[j].type, items[j].material)
            || (stockpiles[currentSp].rejectsRotten && items[j].condition == CONDITION_ROTTEN);
        bool isOverfull = IsSlotOverfull(currentSp, itemSlotX, itemSlotY);
        int haulItemIdx = j;

        if (noLongerAllowed) {
            // For rotten items, find a stockpile that accepts rotten (rejectsRotten=false)
            if (items[j].condition == CONDITION_ROTTEN) {
                destSp = -1;
                for (int sp = 0; sp < MAX_STOCKPILES; sp++) {
                    if (!stockpiles[sp].active) continue;
                    if (stockpiles[sp].rejectsRotten) continue;
                    if (!StockpileAcceptsItem(sp, items[j].type, items[j].material)) continue;
                    if (stockpiles[sp].freeSlotCount <= 0) continue;
                    if (FindFreeStockpileSlot(sp, items[j].type, items[j].material, &destSlotX, &destSlotY)) {
                        destSp = sp;
                        break;
                    }
                }
            } else {
                destSp = FindStockpileForItemCached(items[j].type, items[j].material, &destSlotX, &destSlotY);
            }
        } else if (isOverfull) {
            destSp = FindStockpileForOverfullItem(j, currentSp, &destSlotX, &destSlotY);
            // Split off the excess from the overfull stack
            if (destSp >= 0) {
                Stockpile* sp = &stockpiles[currentSp];
                int lx = itemSlotX - sp->x;
                int ly = itemSlotY - sp->y;
                int slotIdx = ly * sp->width + lx;
                int excess = items[j].stackCount - sp->maxStackSize;
                if (excess > 0 && excess < items[j].stackCount) {
                    haulItemIdx = SplitStack(j, excess);
                    if (haulItemIdx < 0) continue;
                    // Split item is separate from stockpile — mark as ground item
                    items[haulItemIdx].state = ITEM_ON_GROUND;
                    // Update source slot count after split
                    sp->slotCounts[slotIdx] = items[j].stackCount;
                }
            }
        } else {
            destSp = FindHigherPriorityStockpile(j, currentSp, &destSlotX, &destSlotY);
        }

        if (destSp < 0) continue;

        if (TryAssignItemToMover(haulItemIdx, destSp, destSlotX, destSlotY, false)) {
            if (noLongerAllowed) {
                InvalidateStockpileSlotCache(items[j].type, items[j].material);
            }
        }
    }
}

__attribute__((noinline))
static void AssignJobs_P3e_ContainerCleanup(void) {
    for (int spIdx = 0; spIdx < MAX_STOCKPILES && idleMoverCount > 0; spIdx++) {
        Stockpile* sp = &stockpiles[spIdx];
        if (!sp->active) continue;
        if (sp->maxContainers == 0) continue;

        int totalSlots = sp->width * sp->height;
        for (int slotIdx = 0; slotIdx < totalSlots && idleMoverCount > 0; slotIdx++) {
            if (!sp->slotIsContainer[slotIdx]) continue;
            int containerIdx = sp->slots[slotIdx];
            if (containerIdx < 0 || !items[containerIdx].active) continue;
            if (items[containerIdx].contentCount == 0) continue;

            // Scan items contained in this container
            for (int j = 0; j < itemHighWaterMark && idleMoverCount > 0; j++) {
                if (!items[j].active) continue;
                if (items[j].containedIn != containerIdx) continue;
                if (items[j].reservedBy != -1) continue;

                bool illegal = !StockpileAcceptsItem(spIdx, items[j].type, items[j].material)
                    || (sp->rejectsRotten && items[j].condition == CONDITION_ROTTEN);
                if (illegal) {
                    // Extract from container — item becomes ITEM_ON_GROUND at container pos
                    ExtractItemFromContainer(j);

                    // Try to haul to a stockpile that accepts it, or safe-drop
                    int destSlotX, destSlotY;
                    int destSp = FindStockpileForItemCached(items[j].type, items[j].material, &destSlotX, &destSlotY);
                    if (destSp >= 0) {
                        TryAssignItemToMover(j, destSp, destSlotX, destSlotY, false);
                        InvalidateStockpileSlotCache(items[j].type, items[j].material);
                    } else {
                        TryAssignItemToMover(j, -1, -1, -1, true);
                    }
                }
            }
        }
    }
}

__attribute__((noinline))
static void AssignJobs_P3d_Consolidate(void) {
    for (int spIdx = 0; spIdx < MAX_STOCKPILES && idleMoverCount > 0; spIdx++) {
        if (!stockpiles[spIdx].active) continue;
        
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
                    break;
                }
            }
        }
    }
}

__attribute__((noinline))
static void AssignJobs_P4_Designations(void) {
    int designationSpecCount = sizeof(designationSpecs) / sizeof(designationSpecs[0]);
    for (int i = 0; i < designationSpecCount; i++) {
        if (*designationSpecs[i].cacheDirty) {
            designationSpecs[i].RebuildCache();
        }
    }
    
    bool hasDesignationWork = false;
    for (int i = 0; i < designationSpecCount; i++) {
        if (*designationSpecs[i].cacheCount > 0) {
            hasDesignationWork = true;
            break;
        }
    }

    bool hasBlueprintWork = false;
    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS && !hasBlueprintWork; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state == BLUEPRINT_CLEARING) {
            hasBlueprintWork = true;
        } else if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
            const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
            if (recipe) {
                const ConstructionStage* stage = &recipe->stages[bp->stage];
                for (int s = 0; s < stage->inputCount; s++) {
                    if (bp->stageDeliveries[s].deliveredCount + bp->stageDeliveries[s].reservedCount < stage->inputs[s].count) {
                        hasBlueprintWork = true;
                        break;
                    }
                }
            }
        } else if (bp->state == BLUEPRINT_READY_TO_BUILD && bp->assignedBuilder < 0) {
            hasBlueprintWork = true;
        }
    }

    bool hasDeconstructWork = false;
    for (int w = 0; w < MAX_WORKSHOPS && !hasDeconstructWork; w++) {
        if (workshops[w].active && workshops[w].markedForDeconstruct && workshops[w].assignedDeconstructor < 0) {
            hasDeconstructWork = true;
        }
    }

    if (hasDesignationWork || hasBlueprintWork || hasDeconstructWork) {
        int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
        if (!idleCopy) return;
        int idleCopyCount = idleMoverCount;
        memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));

        for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
            int moverIdx = idleCopy[i];

            if (!moverIsInIdleList[moverIdx]) continue;

            int jobId = -1;
            for (int j = 0; j < designationSpecCount && jobId < 0; j++) {
                if (*designationSpecs[j].cacheCount > 0) {
                    jobId = designationSpecs[j].WorkGiver(moverIdx);
                }
            }

            if (jobId < 0 && hasBlueprintWork) {
                jobId = WorkGiver_BlueprintClear(moverIdx);
                if (jobId < 0) jobId = WorkGiver_BlueprintHaul(moverIdx);
                if (jobId < 0) jobId = WorkGiver_Build(moverIdx);
            }

            if (jobId < 0 && hasDeconstructWork) {
                jobId = WorkGiver_DeconstructWorkshop(moverIdx);
            }

            (void)jobId;
        }

        free(idleCopy);
    }
}

void AssignJobs(void) {
    // Initialize job system if needed
    if (!moverIsInIdleList) {
        InitJobSystem(MAX_MOVERS);
    }

    // Rebuild idle list each frame
    RebuildIdleMoverList();
    PROFILE_COUNT_SET(idle_movers, idleMoverCount);

    // Early exit: no idle movers means no work to do
    if (idleMoverCount == 0) return;

    // Rebuild caches once per frame (same as legacy)
    PROFILE_BEGIN(Jobs_CacheRebuild);
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
    PROFILE_COUNT(cache_rebuilds, 1);
    PROFILE_END(Jobs_CacheRebuild);

    PROFILE_BEGIN(Jobs_P1_Maintenance);
    AssignJobs_P1_StockpileMaintenance();
    PROFILE_END(Jobs_P1_Maintenance);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P2_Crafting);
    AssignJobs_P2_Crafting();
    PROFILE_END(Jobs_P2_Crafting);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P2b_PassiveDelivery);
    AssignJobs_P2b_PassiveDelivery();
    PROFILE_END(Jobs_P2b_PassiveDelivery);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P2c_Ignition);
    AssignJobs_P2c_Ignition();
    PROFILE_END(Jobs_P2c_Ignition);
    if (idleMoverCount == 0) return;

    // Priority 2d: Hunting (between crafting and hauling)
    {
        bool anyMarked = false;
        for (int i = 0; i < animalCount; i++) {
            if (animals[i].active && animals[i].markedForHunt && animals[i].reservedByHunter < 0) {
                anyMarked = true;
                break;
            }
        }
        if (anyMarked) {
            int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
            if (idleCopy) {
                int idleCopyCount = idleMoverCount;
                memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));
                for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                    int moverIdx = idleCopy[i];
                    if (!moverIsInIdleList[moverIdx]) continue;
                    WorkGiver_Hunt(moverIdx);
                }
                free(idleCopy);
            }
        }
    }
    if (idleMoverCount == 0) return;

    // Priority 2e: Equip clothing (between hunting and hauling)
    {
        int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
        if (idleCopy) {
            int idleCopyCount = idleMoverCount;
            memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));
            for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                int moverIdx = idleCopy[i];
                if (!moverIsInIdleList[moverIdx]) continue;
                WorkGiver_EquipClothing(moverIdx);
            }
            free(idleCopy);
        }
    }
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P3_ItemHaul);
    if (anyTypeHasSlot) {
        AssignJobs_P3_ItemHaul(typeMatHasStockpile);
    }
    PROFILE_END(Jobs_P3_ItemHaul);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P3c_Rehaul);
    AssignJobs_P3c_Rehaul();
    PROFILE_END(Jobs_P3c_Rehaul);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P3e_ContainerCleanup);
    AssignJobs_P3e_ContainerCleanup();
    PROFILE_END(Jobs_P3e_ContainerCleanup);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P3d_Consolidate);
    AssignJobs_P3d_Consolidate();
    PROFILE_END(Jobs_P3d_Consolidate);
    if (idleMoverCount == 0) return;

    PROFILE_BEGIN(Jobs_P4_Designations);
    AssignJobs_P4_Designations();
    PROFILE_END(Jobs_P4_Designations);
    if (idleMoverCount == 0) return;

    // Priority 5: Farm work (harvest ripe > plant > tend weeds > fertilize)
    if (farmActiveCells > 0) {
        int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
        if (idleCopy) {
            int idleCopyCount = idleMoverCount;
            memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));
            for (int i = 0; i < idleCopyCount && idleMoverCount > 0; i++) {
                int moverIdx = idleCopy[i];
                if (!moverIsInIdleList[moverIdx]) continue;
                int jobId = WorkGiver_HarvestCrop(moverIdx);
                if (jobId < 0) jobId = WorkGiver_PlantCrop(moverIdx);
                if (jobId < 0) jobId = WorkGiver_TendCrop(moverIdx);
                if (jobId < 0) jobId = WorkGiver_Fertilize(moverIdx);
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
} HaulFilterContext;

static bool HaulItemFilter(int itemIdx, void* userData) {
    HaulFilterContext* ctx = (HaulFilterContext*)userData;
    Item* item = &items[itemIdx];
    if (!IsItemHaulable(item, itemIdx)) return false;
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
        HaulFilterContext ctx = { .typeMatHasStockpile = typeMatHasStockpile };

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
    PROFILE_COUNT(pathfinds, 1);
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

// =============================================================================
// Knap stone job (pick up rock → walk to stone wall → knap → sharp stone)
// =============================================================================

JobRunResult RunJob_Knap(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    int tx = job->targetMineX, ty = job->targetMineY, tz = job->targetMineZ;
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_KNAP) return JOBRUN_FAIL;
    if (!CellIsSolid(grid[tz][ty][tx]) || !IsStoneMaterial(GetWallMaterial(tx, ty, tz))) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }

    if (job->step == STEP_MOVING_TO_PICKUP)
        return RunPickupStep(job, mover, (Point){job->targetAdjX, job->targetAdjY, tz});
    if (job->step == STEP_CARRYING) {
        JobRunResult r = RunCarryStep(job, mover, job->targetAdjX, job->targetAdjY, tz);
        if (r == JOBRUN_DONE) {
            job->step = STEP_PLANTING;
            job->progress = 0.0f;
            return JOBRUN_RUNNING;
        }
        return r;
    }
    if (job->step == STEP_PLANTING) {
        JobRunResult r = RunWorkProgress(job, d, mover, dt, KNAP_WORK_TIME, false, 1.0f);
        if (r == JOBRUN_DONE) {
            MaterialType wallMat = GetWallMaterial(tx, ty, tz);
            int itemIdx = job->carryingItem;
            if (itemIdx >= 0 && items[itemIdx].active) DeleteItem(itemIdx);
            job->carryingItem = -1;
            SpawnItemWithMaterial(mover->x, mover->y, mover->z, ITEM_SHARP_STONE, (uint8_t)wallMat);
            CompleteKnapDesignation(tx, ty, tz, job->assignedMover);
        }
        return r;
    }
    return JOBRUN_FAIL;
}

// WorkGiver_KnapDesignation: Find a knap designation + an ITEM_ROCK to carry
static int WorkGiver_KnapDesignation(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Find nearest unassigned knap designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDesigDistSq = 1e30f;

    for (int i = 0; i < knapCacheCount; i++) {
        AdjacentDesignationEntry* entry = &knapCache[i];

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_KNAP || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        float tileX = entry->adjX * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->adjY * CELL_SIZE + CELL_SIZE * 0.5f;
        float distX = tileX - m->x;
        float distY = tileY - m->y;
        float distSq = distX * distX + distY * distY;

        if (distSq < bestDesigDistSq) {
            bestDesigDistSq = distSq;
            bestDesigX = entry->x;
            bestDesigY = entry->y;
            bestDesigZ = entry->z;
            bestAdjX = entry->adjX;
            bestAdjY = entry->adjY;
        }
    }

    if (bestDesigX < 0) return -1;

    // Find nearest available ITEM_ROCK
    int bestItemIdx = -1;
    float bestItemDistSq = 1e30f;

    for (int j = 0; j < itemHighWaterMark; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (item->type != ITEM_ROCK) continue;
        if (item->reservedBy != -1) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        int ix = (int)(item->x / CELL_SIZE);
        int iy = (int)(item->y / CELL_SIZE);
        if (!IsExplored(ix, iy, (int)item->z)) continue;

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
    if (!IsCellWalkableAt(itemCellZ, itemCellY, itemCellX)) {
        SetItemUnreachableCooldown(bestItemIdx, UNREACHABLE_COOLDOWN);
        return -1;
    }
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point itemCell = { itemCellX, itemCellY, itemCellZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        SetItemUnreachableCooldown(bestItemIdx, UNREACHABLE_COOLDOWN);
        return -1;
    }

    // Check reachability from item to adjacent tile
    Point adjCell = { bestAdjX, bestAdjY, bestDesigZ };
    tempLen = FindPath(moverPathAlgorithm, itemCell, adjCell, tempPath, MAX_PATH);
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
    int jobId = CreateJob(JOBTYPE_KNAP);
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
    job->targetAdjX = bestAdjX;
    job->targetAdjY = bestAdjY;
    job->step = STEP_MOVING_TO_PICKUP;
    job->progress = 0.0f;

    // Update mover
    m->currentJobId = jobId;
    m->goal = (Point){ itemCellX, itemCellY, itemCellZ };
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// Public wrapper for WorkGiver_Knap (declared in jobs.h)
int WorkGiver_Knap(int moverIdx) {
    return WorkGiver_KnapDesignation(moverIdx);
}

// WorkGiver_DigRootsDesignation: Find a dig roots designation to work on (on-tile pattern)
static int WorkGiver_DigRootsDesignation(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canPlant) return -1;

    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < digRootsCacheCount; i++) {
        OnTileDesignationEntry* entry = &digRootsCache[i];

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_DIG_ROOTS || d->assignedMover != -1) continue;
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

    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestDesigX, bestDesigY, bestDesigZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    int jobId = CreateJob(JOBTYPE_DIG_ROOTS);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    m->currentJobId = jobId;
    m->goal = (Point){ bestDesigX, bestDesigY, bestDesigZ };
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// Public wrapper for WorkGiver_DigRoots (declared in jobs.h)
int WorkGiver_DigRoots(int moverIdx) {
    return WorkGiver_DigRootsDesignation(moverIdx);
}

// WorkGiver_ExploreDesignation: Find an explore designation — no pathfinding, no capability check
static int WorkGiver_ExploreDesignation(int moverIdx) {
    Mover* m = &movers[moverIdx];

    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < exploreCacheCount; i++) {
        OnTileDesignationEntry* entry = &exploreCache[i];

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_EXPLORE || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        // Only same z-level
        if (entry->z != (int)m->z) continue;

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

    // No pathfinding reachability check — walking blind is the whole point
    int jobId = CreateJob(JOBTYPE_EXPLORE);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    m->currentJobId = jobId;
    // Do NOT set needsRepath — RunJob_Explore handles movement manually
    m->goal = (Point){ bestDesigX, bestDesigY, bestDesigZ };

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// Public wrapper for WorkGiver_Explore (declared in jobs.h)
int WorkGiver_Explore(int moverIdx) {
    return WorkGiver_ExploreDesignation(moverIdx);
}

// WorkGiver_TillDesignation: Find a farm designation to till (on-tile, canPlant)
static int WorkGiver_TillDesignation(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canPlant) return -1;

    int bestX = -1, bestY = -1, bestZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < tillCacheCount; i++) {
        OnTileDesignationEntry* entry = &tillCache[i];
        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_FARM || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

        float tileX = entry->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float tileY = entry->y * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = tileX - m->x;
        float dy = tileY - m->y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestX = entry->x;
            bestY = entry->y;
            bestZ = entry->z;
        }
    }
    if (bestX < 0) return -1;

    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestX, bestY, bestZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        Designation* d = GetDesignation(bestX, bestY, bestZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    int jobId = CreateJob(JOBTYPE_TILL);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestX;
    job->targetMineY = bestY;
    job->targetMineZ = bestZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    Designation* d = GetDesignation(bestX, bestY, bestZ);
    d->assignedMover = moverIdx;

    m->currentJobId = jobId;
    m->goal = (Point){ bestX, bestY, bestZ };
    m->needsRepath = true;
    RemoveMoverFromIdleList(moverIdx);
    return jobId;
}

// Public wrapper for WorkGiver_Till (declared in jobs.h)
int WorkGiver_Till(int moverIdx) {
    return WorkGiver_TillDesignation(moverIdx);
}

// WorkGiver_TendCrop: Auto-find weedy farm cells that need tending
int WorkGiver_TendCrop(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canPlant) return -1;
    if (farmActiveCells == 0) return -1;

    int bestX = -1, bestY = -1, bestZ = -1;
    float bestDistSq = 1e30f;

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                FarmCell* fc = &farmGrid[z][y][x];
                if (!fc->tilled) continue;
                if (fc->weedLevel < WEED_THRESHOLD) continue;
                if (!IsExplored(x, y, z)) continue;

                float tileX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float tileY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                float dx = tileX - m->x;
                float dy = tileY - m->y;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                }
            }
        }
    }
    if (bestX < 0) return -1;

    // Check no other mover is already tending this cell
    for (int i = 0; i < activeJobCount; i++) {
        Job* j = GetJob(activeJobList[i]);
        if (j && j->type == JOBTYPE_TEND_CROP &&
            j->targetMineX == bestX && j->targetMineY == bestY && j->targetMineZ == bestZ) {
            return -1;
        }
    }

    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestX, bestY, bestZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) return -1;

    int jobId = CreateJob(JOBTYPE_TEND_CROP);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestX;
    job->targetMineY = bestY;
    job->targetMineZ = bestZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    m->currentJobId = jobId;
    m->goal = (Point){ bestX, bestY, bestZ };
    m->needsRepath = true;
    RemoveMoverFromIdleList(moverIdx);
    return jobId;
}

// WorkGiver_Fertilize: Find low-fertility farm cell + available compost
int WorkGiver_Fertilize(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canPlant) return -1;
    if (farmActiveCells == 0) return -1;

    // Find nearest available compost item
    int bestCompostIdx = -1;
    float bestCompostDistSq = 1e30f;
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (item->type != ITEM_COMPOST) continue;
        if (item->state != ITEM_ON_GROUND) continue;
        if (item->reservedBy != -1) continue;
        float dx = item->x - m->x;
        float dy = item->y - m->y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestCompostDistSq) {
            bestCompostDistSq = distSq;
            bestCompostIdx = i;
        }
    }
    if (bestCompostIdx < 0) return -1;

    // Find nearest low-fertility farm cell
    int bestX = -1, bestY = -1, bestZ = -1;
    float bestDistSq = 1e30f;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                FarmCell* fc = &farmGrid[z][y][x];
                if (!fc->tilled) continue;
                if (fc->fertility >= FERTILITY_LOW) continue;
                if (!IsExplored(x, y, z)) continue;

                float tileX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float tileY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                float dx = tileX - m->x;
                float dy = tileY - m->y;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                }
            }
        }
    }
    if (bestX < 0) return -1;

    // Check no other mover is already fertilizing this cell
    for (int i = 0; i < activeJobCount; i++) {
        Job* j = GetJob(activeJobList[i]);
        if (j && j->type == JOBTYPE_FERTILIZE &&
            j->targetMineX == bestX && j->targetMineY == bestY && j->targetMineZ == bestZ) {
            return -1;
        }
    }

    // Reserve compost
    ReserveItem(bestCompostIdx, moverIdx);

    int jobId = CreateJob(JOBTYPE_FERTILIZE);
    if (jobId < 0) {
        ReleaseItemReservation(bestCompostIdx);
        return -1;
    }

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = bestCompostIdx;
    job->targetMineX = bestX;
    job->targetMineY = bestY;
    job->targetMineZ = bestZ;
    job->step = STEP_MOVING_TO_PICKUP;
    job->progress = 0.0f;
    job->carryingItem = -1;

    m->currentJobId = jobId;
    m->goal = (Point){ (int)(items[bestCompostIdx].x / CELL_SIZE),
                        (int)(items[bestCompostIdx].y / CELL_SIZE),
                        (int)items[bestCompostIdx].z };
    m->needsRepath = true;
    RemoveMoverFromIdleList(moverIdx);
    return jobId;
}

// WorkGiver_PlantCrop: Find tilled cells with desiredCropType set and no crop, find matching seed
int WorkGiver_PlantCrop(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canPlant) return -1;
    if (farmActiveCells == 0) return -1;

    // Find nearest tilled cell that wants a crop but has none
    int bestX = -1, bestY = -1, bestZ = -1;
    CropType bestCrop = CROP_NONE;
    float bestDistSq = 1e30f;

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                FarmCell* fc = &farmGrid[z][y][x];
                if (!fc->tilled) continue;
                if (fc->desiredCropType == CROP_NONE) continue;
                if (fc->cropType != CROP_NONE) continue;  // Already has a crop
                if (!IsExplored(x, y, z)) continue;

                float tileX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float tileY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                float dx = tileX - m->x;
                float dy = tileY - m->y;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                    bestCrop = (CropType)fc->desiredCropType;
                }
            }
        }
    }
    if (bestX < 0) return -1;

    // Check no other mover is already planting this cell
    for (int i = 0; i < activeJobCount; i++) {
        Job* j = GetJob(activeJobList[i]);
        if (j && j->type == JOBTYPE_PLANT_CROP &&
            j->targetMineX == bestX && j->targetMineY == bestY && j->targetMineZ == bestZ) {
            return -1;
        }
    }

    // Find matching seed item
    int seedType = SeedTypeForCrop(bestCrop);
    if (seedType == ITEM_NONE) return -1;

    int bestSeedIdx = -1;
    float bestSeedDistSq = 1e30f;
    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (item->type != seedType) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (item->reservedBy != -1) continue;
        float dx = item->x - m->x;
        float dy = item->y - m->y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestSeedDistSq) {
            bestSeedDistSq = distSq;
            bestSeedIdx = i;
        }
    }
    if (bestSeedIdx < 0) return -1;

    // Reserve seed
    ReserveItem(bestSeedIdx, moverIdx);

    int jobId = CreateJob(JOBTYPE_PLANT_CROP);
    if (jobId < 0) {
        ReleaseItemReservation(bestSeedIdx);
        return -1;
    }

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetItem = bestSeedIdx;
    job->targetMineX = bestX;
    job->targetMineY = bestY;
    job->targetMineZ = bestZ;
    job->step = STEP_MOVING_TO_PICKUP;
    job->progress = 0.0f;
    job->carryingItem = -1;

    m->currentJobId = jobId;
    m->goal = (Point){ (int)(items[bestSeedIdx].x / CELL_SIZE),
                        (int)(items[bestSeedIdx].y / CELL_SIZE),
                        (int)items[bestSeedIdx].z };
    m->needsRepath = true;
    RemoveMoverFromIdleList(moverIdx);
    return jobId;
}

// WorkGiver_HarvestCrop: Auto-find ripe farm cells
int WorkGiver_HarvestCrop(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canPlant) return -1;
    if (farmActiveCells == 0) return -1;

    int bestX = -1, bestY = -1, bestZ = -1;
    float bestDistSq = 1e30f;

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                FarmCell* fc = &farmGrid[z][y][x];
                if (!fc->tilled) continue;
                if (fc->growthStage != CROP_STAGE_RIPE) continue;
                if (!IsExplored(x, y, z)) continue;

                float tileX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float tileY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                float dx = tileX - m->x;
                float dy = tileY - m->y;
                float distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                }
            }
        }
    }
    if (bestX < 0) return -1;

    // Check no other mover is already harvesting this cell
    for (int i = 0; i < activeJobCount; i++) {
        Job* j = GetJob(activeJobList[i]);
        if (j && j->type == JOBTYPE_HARVEST_CROP &&
            j->targetMineX == bestX && j->targetMineY == bestY && j->targetMineZ == bestZ) {
            return -1;
        }
    }

    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestX, bestY, bestZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) return -1;

    int jobId = CreateJob(JOBTYPE_HARVEST_CROP);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestX;
    job->targetMineY = bestY;
    job->targetMineZ = bestZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    m->currentJobId = jobId;
    m->goal = (Point){ bestX, bestY, bestZ };
    m->needsRepath = true;
    RemoveMoverFromIdleList(moverIdx);
    return jobId;
}

// WorkGiver_DeconstructWorkshop: Find workshop marked for deconstruction
int WorkGiver_DeconstructWorkshop(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canBuild) return -1;

    int bestWsIdx = -1;
    float bestDistSq = 1e30f;

    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (!ws->markedForDeconstruct) continue;
        if (ws->assignedDeconstructor >= 0) continue;

        float wx = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float wy = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = wx - m->x;
        float dy = wy - m->y;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestWsIdx = w;
        }
    }

    if (bestWsIdx < 0) return -1;

    Workshop* ws = &workshops[bestWsIdx];

    // Check reachability to work tile
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point goalCell = { ws->workTileX, ws->workTileY, ws->z };

    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, goalCell, tempPath, MAX_PATH);
    if (tempLen == 0) return -1;

    // Get deconstruct time = half build time
    float deconstructTime = 1.0f;
    int recipeIdx = GetConstructionRecipeForWorkshopType(ws->type);
    if (recipeIdx >= 0) {
        const ConstructionRecipe* recipe = GetConstructionRecipe(recipeIdx);
        if (recipe && recipe->stageCount > 0) {
            float totalBuildTime = 0.0f;
            for (int s = 0; s < recipe->stageCount; s++) {
                totalBuildTime += recipe->stages[s].buildTime;
            }
            deconstructTime = totalBuildTime * 0.5f;
            if (deconstructTime < 0.5f) deconstructTime = 0.5f;
        }
    }

    int jobId = CreateJob(JOBTYPE_DECONSTRUCT_WORKSHOP);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetWorkshop = bestWsIdx;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;
    job->workRequired = deconstructTime;

    ws->assignedDeconstructor = moverIdx;

    m->currentJobId = jobId;
    m->goal = goalCell;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    EventLog("Workshop %d (%s) deconstruction assigned to mover %d",
             bestWsIdx, workshopDefs[ws->type].displayName, moverIdx);

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
        if (!workshopDefs[ws->type].passive) continue;
        if (ws->passiveReady) continue;  // Skip workshops that are already burning
        if (ws->billCount == 0) continue;

        // Find first runnable, non-suspended bill
        int billIdx = -1;
        for (int b = 0; b < ws->billCount; b++) {
            if (ws->bills[b].suspended) continue;
            if (ShouldBillRun(ws, &ws->bills[b])) {
                billIdx = b;
                break;
            }
        }
        if (billIdx < 0) continue;

        const Recipe* recipe = &workshopDefs[ws->type].recipes[ws->bills[billIdx].recipeIdx];

        // Count units already on work tile + reserved for delivery to this workshop
        int inputOnTile = 0;
        for (int j = 0; j < itemHighWaterMark; j++) {
            Item* item = &items[j];
            if (!item->active) continue;
            if (!RecipeInputMatches(recipe, item)) continue;

            // Item already on work tile — count stackCount
            int ix = (int)(item->x / CELL_SIZE);
            int iy = (int)(item->y / CELL_SIZE);
            if (ix == ws->workTileX && iy == ws->workTileY && (int)item->z == ws->z &&
                item->state == ITEM_ON_GROUND) {
                inputOnTile += item->stackCount;
            }

            // Item reserved for delivery to this workshop (being fetched or carried)
            // Count as 1 unit per delivery job (movers carry 1 at a time)
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
            if (!RecipeInputMatches(recipe, item)) continue;

            int cellX = (int)(item->x / CELL_SIZE);
            int cellY = (int)(item->y / CELL_SIZE);
            if (!IsCellWalkableAt((int)item->z, cellY, cellX)) continue;
            if (!IsExplored(cellX, cellY, (int)item->z)) continue;

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

        // Fallback: search inside containers
        if (bestItemIdx < 0 && recipe->inputItemMatch != ITEM_MATCH_ANY_FUEL) {
            int containerIdx = -1;
            int moverTileX = (int)(m->x / CELL_SIZE);
            int moverTileY = (int)(m->y / CELL_SIZE);
            bestItemIdx = FindItemInContainers(recipe->inputType, moverZ, moverTileX, moverTileY,
                                               100, -1, NULL, NULL, &containerIdx);
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
        if (!workshopDefs[ws->type].passive) continue;
        if (ws->passiveReady) continue;           // Already ignited
        if (ws->assignedCrafter >= 0) continue;   // Someone already assigned
        if (ws->billCount == 0) continue;

        // Find first runnable, non-suspended bill
        int billIdx = -1;
        for (int b = 0; b < ws->billCount; b++) {
            if (ws->bills[b].suspended) continue;
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


    // Find nearest unassigned gather sapling designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < gatherSaplingCacheCount; i++) {
        AdjacentDesignationEntry* entry = &gatherSaplingCache[i];



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

    // Find nearest unassigned plant sapling designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDesigDistSq = 1e30f;

    for (int i = 0; i < plantSaplingCacheCount; i++) {
        OnTileDesignationEntry* entry = &plantSaplingCache[i];



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
        int ix = (int)(item->x / CELL_SIZE);
        int iy = (int)(item->y / CELL_SIZE);
        if (!IsExplored(ix, iy, (int)item->z)) continue;

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


    // Find nearest unassigned gather grass designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < gatherGrassCacheCount; i++) {
        OnTileDesignationEntry* entry = &gatherGrassCache[i];

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

    if (bestDesigX < 0) {
        if (gatherGrassCacheCount > 0) {
            EventLog("WorkGiver_GatherGrass: mover %d at z%d, %d cached desigs (none matched)", moverIdx, (int)m->z, gatherGrassCacheCount);
        }
        return -1;
    }

    // Check reachability
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestDesigX, bestDesigY, bestDesigZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        EventLog("WorkGiver_GatherGrass: desig (%d,%d,z%d) unreachable from mover %d at z%d", bestDesigX, bestDesigY, bestDesigZ, moverIdx, (int)m->z);
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

// WorkGiver_HarvestBerry: Find a harvest berry designation to work on
int WorkGiver_HarvestBerry(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canPlant) return -1;


    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < harvestBerryCacheCount; i++) {
        OnTileDesignationEntry* entry = &harvestBerryCache[i];

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_HARVEST_BERRY || d->assignedMover != -1) continue;
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

    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z };
    Point desigCell = { bestDesigX, bestDesigY, bestDesigZ };
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, desigCell, tempPath, MAX_PATH);
    if (tempLen <= 0) {
        Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }

    int jobId = CreateJob(JOBTYPE_HARVEST_BERRY);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestDesigX;
    job->targetMineY = bestDesigY;
    job->targetMineZ = bestDesigZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;

    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    m->currentJobId = jobId;
    m->goal = (Point){ bestDesigX, bestDesigY, bestDesigZ };
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_GatherTree: Find a tree gather designation to work on
int WorkGiver_GatherTree(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canPlant) return -1;


    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < gatherTreeCacheCount; i++) {
        AdjacentDesignationEntry* entry = &gatherTreeCache[i];

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_GATHER_TREE || d->assignedMover != -1) continue;
        if (d->unreachableCooldown > 0.0f) continue;

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
    int jobId = CreateJob(JOBTYPE_GATHER_TREE);
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

// WorkGiver_CleanDesignation: Find a clean designation to work on (on-tile, no capability check)
static int WorkGiver_CleanDesignation(int moverIdx) {
    Mover* m = &movers[moverIdx];


    // Find nearest unassigned clean designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < cleanCacheCount; i++) {
        OnTileDesignationEntry* entry = &cleanCache[i];

        Designation* d = GetDesignation(entry->x, entry->y, entry->z);
        if (!d || d->type != DESIGNATION_CLEAN || d->assignedMover != -1) continue;
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
    int jobId = CreateJob(JOBTYPE_CLEAN);
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

// Public wrapper for the header declaration
int WorkGiver_Clean(int moverIdx) {
    return WorkGiver_CleanDesignation(moverIdx);
}

// WorkGiver_Craft: Find workshop with runnable bill and available materials
// Returns job ID if successful, -1 if no job available
int WorkGiver_Craft(int moverIdx) {
    Mover* m = &movers[moverIdx];

    // Note: no canCraft capability check for now - any mover can craft

    // Early exit: check if any workshops need crafters
    bool anyAvailable = false;
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
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
        if (workshopDefs[ws->type].passive) continue;  // Passive workshops don't use crafters
        if (ws->assignedCrafter >= 0) continue;  // Already has crafter

        // Find first non-suspended bill that can run
        for (int b = 0; b < ws->billCount; b++) {
            Bill* bill = &ws->bills[b];

            // Auto-resume bills that were suspended due to no storage
            if (bill->suspended && bill->suspendedNoStorage) {
                // Check if storage is now available for any available input material
                int resumeRecipeCount;
                const Recipe* resumeRecipes = GetRecipesForWorkshop(ws->type, &resumeRecipeCount);
                if (bill->recipeIdx >= 0 && bill->recipeIdx < resumeRecipeCount) {
                    const Recipe* resumeRecipe = &resumeRecipes[bill->recipeIdx];
                    for (int i = 0; i < itemHighWaterMark; i++) {
                        if (!items[i].active) continue;
                        if (!RecipeInputMatches(resumeRecipe, &items[i])) continue;
                        if (items[i].reservedBy != -1) continue;
                        uint8_t mat = items[i].material;
                        if (mat == MAT_NONE) mat = DefaultMaterialForItemType(items[i].type);
                        int outSlotX, outSlotY;
                        if (FindStockpileForItem(resumeRecipe->outputType, mat, &outSlotX, &outSlotY) >= 0) {
                            if (resumeRecipe->outputType2 == ITEM_NONE ||
                                FindStockpileForItem(resumeRecipe->outputType2, mat, &outSlotX, &outSlotY) >= 0) {
                                bill->suspended = false;
                                bill->suspendedNoStorage = false;
                                break;
                            }
                        }
                    }
                }
            }

            if (bill->suspended) continue;
            if (!ShouldBillRun(ws, bill)) continue;

            // Get recipe for this bill
            int recipeCount;
            const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
            if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) continue;
            const Recipe* recipe = &recipes[bill->recipeIdx];

            // Check tool requirement for this recipe
            int neededToolIdx = -1;
            if (recipe->requiredQualityLevel > 0 && toolRequirementsEnabled) {
                QualityType reqQuality = (QualityType)recipe->requiredQuality;
                int reqLevel = recipe->requiredQualityLevel;
                int currentLevel = (m->equippedTool >= 0)
                    ? GetItemQualityLevel(items[m->equippedTool].type, reqQuality) : 0;
                if (currentLevel < reqLevel) {
                    neededToolIdx = FindNearestToolForQuality(reqQuality, reqLevel,
                        (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, 50, -1);
                    if (neededToolIdx < 0) continue;  // No tool, skip this bill
                }
            }

            // Find an input item first (search nearby or all if radius = 0)
            int searchRadius = bill->ingredientSearchRadius;
            if (searchRadius == 0) searchRadius = 100;  // Large default

            int itemIdx = -1;
            int bestDistSq = searchRadius * searchRadius;

            // Search for closest unreserved item of the required type with enough stack
            for (int i = 0; i < itemHighWaterMark; i++) {
                Item* item = &items[i];
                if (!item->active) continue;
                if (item->state == ITEM_IN_CONTAINER) continue;
                if (!RecipeInputMatches(recipe, item)) continue;
                if (item->reservedBy != -1) continue;
                if (item->unreachableCooldown > 0.0f) continue;
                if (item->stackCount < recipe->inputCount) continue;

                // Check distance from workshop
                int itemTileX = (int)(item->x / CELL_SIZE);
                int itemTileY = (int)(item->y / CELL_SIZE);
                if (!IsExplored(itemTileX, itemTileY, (int)item->z)) continue;
                int dx = itemTileX - ws->x;
                int dy = itemTileY - ws->y;
                int distSq = dx * dx + dy * dy;
                if (distSq > bestDistSq) continue;

                bestDistSq = distSq;
                itemIdx = i;
            }

            // Fallback: search inside containers
            if (itemIdx < 0 && recipe->inputItemMatch != ITEM_MATCH_ANY_FUEL) {
                int containerIdx = -1;
                itemIdx = FindItemInContainers(recipe->inputType, ws->z, ws->x, ws->y,
                                               searchRadius, -1, NULL, NULL, &containerIdx);
            }

            if (itemIdx < 0) continue;  // No materials available

            Item* item = &items[itemIdx];

            // Check if there's stockpile space for the output (using input's material)
            uint8_t outputMat = item->material;
            if (outputMat == MAT_NONE) {
                outputMat = DefaultMaterialForItemType(item->type);
            }
            int outSlotX, outSlotY;
            if (recipe->outputType != ITEM_NONE) {
                if (FindStockpileForItem(recipe->outputType, outputMat, &outSlotX, &outSlotY) < 0) {
                    bill->suspended = true;
                    bill->suspendedNoStorage = true;
                    continue;
                }
                if (recipe->outputType2 != ITEM_NONE &&
                    FindStockpileForItem(recipe->outputType2, outputMat, &outSlotX, &outSlotY) < 0) {
                    bill->suspended = true;
                    bill->suspendedNoStorage = true;
                    continue;
                }
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
                    if (item2->state == ITEM_IN_CONTAINER) continue;
                    if (item2->type != recipe->inputType2) continue;
                    if (item2->reservedBy != -1) continue;
                    if (item2->unreachableCooldown > 0.0f) continue;
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

                // Fallback: search inside containers for second input
                if (item2Idx < 0) {
                    int containerIdx = -1;
                    item2Idx = FindItemInContainers(recipe->inputType2, ws->z, ws->x, ws->y,
                                                    searchRadius, itemIdx, NULL, NULL, &containerIdx);
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

            // Find third input item if recipe requires it
            int item3Idx = -1;
            if (recipe->inputType3 != ITEM_NONE && recipe->inputCount3 > 0) {
                int best3DistSq = searchRadius * searchRadius;
                for (int i = 0; i < itemHighWaterMark; i++) {
                    Item* item3 = &items[i];
                    if (!item3->active) continue;
                    if (item3->state == ITEM_IN_CONTAINER) continue;
                    if (item3->type != recipe->inputType3) continue;
                    if (item3->reservedBy != -1) continue;
                    if (item3->unreachableCooldown > 0.0f) continue;
                    if (i == itemIdx) continue;      // Can't use same item as first input
                    if (i == item2Idx) continue;     // Can't use same item as second input

                    int item3TileX = (int)(item3->x / CELL_SIZE);
                    int item3TileY = (int)(item3->y / CELL_SIZE);
                    int dx3 = item3TileX - ws->x;
                    int dy3 = item3TileY - ws->y;
                    int dist3Sq = dx3 * dx3 + dy3 * dy3;
                    if (dist3Sq > best3DistSq) continue;

                    best3DistSq = dist3Sq;
                    item3Idx = i;
                }

                // Fallback: search inside containers for third input
                if (item3Idx < 0) {
                    int containerIdx = -1;
                    item3Idx = FindItemInContainers(recipe->inputType3, ws->z, ws->x, ws->y,
                                                    searchRadius, itemIdx, NULL, NULL, &containerIdx);
                    // Also exclude item2Idx
                    if (item3Idx == item2Idx) item3Idx = -1;
                }

                if (item3Idx < 0) continue;  // Third input not available

                // Verify third input is reachable from workshop
                Point item3Cell = { (int)(items[item3Idx].x / CELL_SIZE), (int)(items[item3Idx].y / CELL_SIZE), (int)items[item3Idx].z };
                int item3PathLen = FindPath(moverPathAlgorithm, workCell, item3Cell, tempPath, MAX_PATH);
                if (item3PathLen == 0) {
                    SetItemUnreachableCooldown(item3Idx, UNREACHABLE_COOLDOWN);
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
            if (item3Idx >= 0) items[item3Idx].reservedBy = moverIdx;
            if (fuelIdx >= 0) items[fuelIdx].reservedBy = moverIdx;

            // Create job
            int jobId = CreateJob(JOBTYPE_CRAFT);
            if (jobId < 0) {
                item->reservedBy = -1;
                ws->assignedCrafter = -1;
                if (item2Idx >= 0) items[item2Idx].reservedBy = -1;
                if (item3Idx >= 0) items[item3Idx].reservedBy = -1;
                if (fuelIdx >= 0) items[fuelIdx].reservedBy = -1;
                return -1;
            }

            Job* job = GetJob(jobId);
            job->assignedMover = moverIdx;
            job->targetWorkshop = w;
            job->targetBillIdx = b;
            job->targetItem = itemIdx;
            job->targetItem2 = item2Idx;
            job->targetItem3 = item3Idx;
            job->progress = 0.0f;
            job->carryingItem = -1;
            job->fuelItem = fuelIdx;

            // Set up tool fetch or go directly to input pickup
            if (neededToolIdx >= 0) {
                items[neededToolIdx].reservedBy = moverIdx;
                job->toolItem = neededToolIdx;
                job->step = STEP_FETCHING_TOOL;
                m->goal = (Point){ (int)(items[neededToolIdx].x / CELL_SIZE),
                                    (int)(items[neededToolIdx].y / CELL_SIZE),
                                    (int)items[neededToolIdx].z };
            } else {
                job->step = CRAFT_STEP_MOVING_TO_INPUT;
                m->goal = itemCell;
            }

            // Update mover
            m->currentJobId = jobId;
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

    // Find an item in a stockpile that needs re-hauling
    int bestItemIdx = -1;
    int bestDestSp = -1;
    int bestDestSlotX = -1, bestDestSlotY = -1;
    float bestDistSq = 1e30f;

    for (int j = 0; j < itemHighWaterMark; j++) {
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

    // Find nearest unassigned mine designation from the pre-built cache
    // First pass: entries the mover can do with current tool
    // Second pass: hard-gated entries the mover could do with a nearby tool
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;
    bool bestNeedsTool = false;

    for (int pass = 0; pass < 2 && bestDesigX < 0; pass++) {
        for (int i = 0; i < mineCacheCount; i++) {
            AdjacentDesignationEntry* entry = &mineCache[i];

            // Check if still unassigned, correct type, and not marked unreachable
            Designation* d = GetDesignation(entry->x, entry->y, entry->z);
            if (!d || d->type != DESIGNATION_MINE || d->assignedMover != -1) continue;
            if (d->unreachableCooldown > 0.0f) continue;

            MaterialType mat = GetWallMaterial(entry->x, entry->y, entry->z);
            bool canDo = CanMoverDoJob(JOBTYPE_MINE, mat, m->equippedTool);

            if (pass == 0 && !canDo) continue;       // First pass: skip what we can't do
            if (pass == 1 && canDo) continue;         // Second pass: skip what we already could do
            if (pass == 1 && !toolRequirementsEnabled) continue;

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
                bestNeedsTool = (pass == 1);
            }
        }
    }

    if (bestDesigX < 0) return -1;

    // If second-pass result, find a tool for it
    int neededToolIdx = -1;
    if (bestNeedsTool) {
        MaterialType mat = GetWallMaterial(bestDesigX, bestDesigY, bestDesigZ);
        JobToolReq req = GetJobToolRequirement(JOBTYPE_MINE, mat);
        neededToolIdx = FindNearestToolForQuality(req.qualityType, req.minLevel,
            (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, 50, -1);
        if (neededToolIdx < 0) return -1;  // No tool available
    }

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
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Set up tool fetch or go directly to work
    if (neededToolIdx >= 0) {
        items[neededToolIdx].reservedBy = moverIdx;
        job->toolItem = neededToolIdx;
        job->step = STEP_FETCHING_TOOL;
        m->goal = (Point){ (int)(items[neededToolIdx].x / CELL_SIZE),
                            (int)(items[neededToolIdx].y / CELL_SIZE),
                            (int)items[neededToolIdx].z };
    } else {
        job->step = STEP_MOVING_TO_WORK;
        m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
    }

    // Update mover
    m->currentJobId = jobId;
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

    // Find nearest unassigned channel designation from the pre-built cache
    // Two-pass: first try doable with current tool, then try with tool seeking
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;
    bool bestNeedsTool = false;

    for (int pass = 0; pass < 2 && bestDesigX < 0; pass++) {
        for (int i = 0; i < channelCacheCount; i++) {
            OnTileDesignationEntry* entry = &channelCache[i];

            Designation* d = GetDesignation(entry->x, entry->y, entry->z);
            if (!d || d->type != DESIGNATION_CHANNEL || d->assignedMover != -1) continue;
            if (d->unreachableCooldown > 0.0f) continue;

            MaterialType mat = (entry->z > 0) ? GetWallMaterial(entry->x, entry->y, entry->z - 1) : MAT_DIRT;
            bool canDo = CanMoverDoJob(JOBTYPE_CHANNEL, mat, m->equippedTool);

            if (pass == 0 && !canDo) continue;
            if (pass == 1 && canDo) continue;
            if (pass == 1 && !toolRequirementsEnabled) continue;

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
                bestNeedsTool = (pass == 1);
            }
        }
    }

    if (bestDesigX < 0) return -1;

    // If second-pass result, find a tool for it
    int neededToolIdx = -1;
    if (bestNeedsTool) {
        MaterialType mat = (bestDesigZ > 0) ? GetWallMaterial(bestDesigX, bestDesigY, bestDesigZ - 1) : MAT_DIRT;
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHANNEL, mat);
        neededToolIdx = FindNearestToolForQuality(req.qualityType, req.minLevel,
            (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, 50, -1);
        if (neededToolIdx < 0) return -1;
    }

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
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Set up tool fetch or go directly to work
    if (neededToolIdx >= 0) {
        items[neededToolIdx].reservedBy = moverIdx;
        job->toolItem = neededToolIdx;
        job->step = STEP_FETCHING_TOOL;
        m->goal = (Point){ (int)(items[neededToolIdx].x / CELL_SIZE),
                            (int)(items[neededToolIdx].y / CELL_SIZE),
                            (int)items[neededToolIdx].z };
    } else {
        job->step = STEP_MOVING_TO_WORK;
        m->goal = targetCell;
    }

    // Update mover
    m->currentJobId = jobId;
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

    // Find nearest unassigned dig ramp designation from the pre-built cache
    // Two-pass: first try doable with current tool, then try with tool seeking
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;
    bool bestNeedsTool = false;

    for (int pass = 0; pass < 2 && bestDesigX < 0; pass++) {
        for (int i = 0; i < digRampCacheCount; i++) {
            AdjacentDesignationEntry* entry = &digRampCache[i];

            Designation* d = GetDesignation(entry->x, entry->y, entry->z);
            if (!d || d->type != DESIGNATION_DIG_RAMP || d->assignedMover != -1) continue;
            if (d->unreachableCooldown > 0.0f) continue;

            MaterialType mat = GetWallMaterial(entry->x, entry->y, entry->z);
            bool canDo = CanMoverDoJob(JOBTYPE_DIG_RAMP, mat, m->equippedTool);

            if (pass == 0 && !canDo) continue;
            if (pass == 1 && canDo) continue;
            if (pass == 1 && !toolRequirementsEnabled) continue;

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
                bestNeedsTool = (pass == 1);
            }
        }
    }

    if (bestDesigX < 0) return -1;

    // If second-pass result, find a tool for it
    int neededToolIdx = -1;
    if (bestNeedsTool) {
        MaterialType mat = GetWallMaterial(bestDesigX, bestDesigY, bestDesigZ);
        JobToolReq req = GetJobToolRequirement(JOBTYPE_DIG_RAMP, mat);
        neededToolIdx = FindNearestToolForQuality(req.qualityType, req.minLevel,
            (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, 50, -1);
        if (neededToolIdx < 0) return -1;
    }

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
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Set up tool fetch or go directly to work
    if (neededToolIdx >= 0) {
        items[neededToolIdx].reservedBy = moverIdx;
        job->toolItem = neededToolIdx;
        job->step = STEP_FETCHING_TOOL;
        m->goal = (Point){ (int)(items[neededToolIdx].x / CELL_SIZE),
                            (int)(items[neededToolIdx].y / CELL_SIZE),
                            (int)items[neededToolIdx].z };
    } else {
        job->step = STEP_MOVING_TO_WORK;
        m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
    }

    // Update mover
    m->currentJobId = jobId;
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


    // Find nearest unassigned remove floor designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < removeFloorCacheCount; i++) {
        OnTileDesignationEntry* entry = &removeFloorCache[i];



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


    // Find nearest unassigned remove ramp designation from the pre-built cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < removeRampCacheCount; i++) {
        AdjacentDesignationEntry* entry = &removeRampCache[i];



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

    // Tool gate: chopping requires cutting:1
    // If mover lacks tool, try to find one nearby
    int neededToolIdx = -1;
    if (!CanMoverDoJob(JOBTYPE_CHOP, MAT_NONE, m->equippedTool)) {
        if (!toolRequirementsEnabled) return -1;
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHOP, MAT_NONE);
        neededToolIdx = FindNearestToolForQuality(req.qualityType, req.minLevel,
            (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, 50, -1);
        if (neededToolIdx < 0) return -1;
    }

    // Find nearest unassigned chop designation from cache
    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < chopCacheCount; i++) {
        AdjacentDesignationEntry* entry = &chopCache[i];

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
    job->progress = 0.0f;

    // Reserve designation
    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Set up tool fetch or go directly to work
    if (neededToolIdx >= 0) {
        items[neededToolIdx].reservedBy = moverIdx;
        job->toolItem = neededToolIdx;
        job->step = STEP_FETCHING_TOOL;
        m->goal = (Point){ (int)(items[neededToolIdx].x / CELL_SIZE),
                            (int)(items[neededToolIdx].y / CELL_SIZE),
                            (int)items[neededToolIdx].z };
    } else {
        job->step = STEP_MOVING_TO_WORK;
        m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
    }

    // Update mover
    m->currentJobId = jobId;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_ChopFelled: Find a felled trunk designation to work on
// Returns job ID if successful, -1 if no job available
int WorkGiver_ChopFelled(int moverIdx) {
    Mover* m = &movers[moverIdx];

    if (!m->capabilities.canMine) return -1;

    // Tool gate: chopping felled trunks requires cutting:1
    // If mover lacks tool, try to find one nearby
    int neededToolIdx = -1;
    if (!CanMoverDoJob(JOBTYPE_CHOP_FELLED, MAT_NONE, m->equippedTool)) {
        if (!toolRequirementsEnabled) return -1;
        JobToolReq req = GetJobToolRequirement(JOBTYPE_CHOP_FELLED, MAT_NONE);
        neededToolIdx = FindNearestToolForQuality(req.qualityType, req.minLevel,
            (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, 50, -1);
        if (neededToolIdx < 0) return -1;
    }

    int bestDesigX = -1, bestDesigY = -1, bestDesigZ = -1;
    int bestAdjX = -1, bestAdjY = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < chopFelledCacheCount; i++) {
        AdjacentDesignationEntry* entry = &chopFelledCache[i];

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
    job->progress = 0.0f;

    Designation* d = GetDesignation(bestDesigX, bestDesigY, bestDesigZ);
    d->assignedMover = moverIdx;

    // Set up tool fetch or go directly to work
    if (neededToolIdx >= 0) {
        items[neededToolIdx].reservedBy = moverIdx;
        job->toolItem = neededToolIdx;
        job->step = STEP_FETCHING_TOOL;
        m->goal = (Point){ (int)(items[neededToolIdx].x / CELL_SIZE),
                            (int)(items[neededToolIdx].y / CELL_SIZE),
                            (int)items[neededToolIdx].z };
    } else {
        job->step = STEP_MOVING_TO_WORK;
        m->goal = (Point){ bestAdjX, bestAdjY, bestDesigZ };
    }

    m->currentJobId = jobId;
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

    // Find nearest blueprint ready to build
    int bestBpIdx = -1;
    float bestDistSq = 1e30f;

    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_READY_TO_BUILD) continue;
        if (bp->assignedBuilder >= 0) continue;  // Already has a builder

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
    EventLog("Blueprint %d at (%d,%d,z%d) -> BUILDING by mover %d",
             bestBpIdx, bp->x, bp->y, bp->z, moverIdx);

    // Update mover
    m->currentJobId = jobId;
    m->goal = goalCell;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}

// WorkGiver_BlueprintClear: Haul items away from CLEARING blueprint cells
// Returns job ID if successful, -1 if no job available
int WorkGiver_BlueprintClear(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canHaul) return -1;

    int moverZ = (int)m->z;
    int moverTileX = (int)(m->x / CELL_SIZE);
    int moverTileY = (int)(m->y / CELL_SIZE);
    Point moverCell = { moverTileX, moverTileY, moverZ };
    Point tempPath[MAX_PATH];

    for (int bpIdx = 0; bpIdx < MAX_BLUEPRINTS; bpIdx++) {
        Blueprint* bp = &blueprints[bpIdx];
        if (!bp->active) continue;
        if (bp->state != BLUEPRINT_CLEARING) continue;

        // Find an unreserved item at this blueprint's cell
        int foundItem = -1;
        bool anyItemsLeft = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            if ((int)items[i].z != bp->z) continue;
            if (items[i].state != ITEM_ON_GROUND && items[i].state != ITEM_IN_STOCKPILE) continue;
            int ix = (int)(items[i].x / CELL_SIZE);
            int iy = (int)(items[i].y / CELL_SIZE);
            if (ix != bp->x || iy != bp->y) continue;
            if (!IsExplored(ix, iy, (int)items[i].z)) continue;
            anyItemsLeft = true;
            if (items[i].reservedBy != -1) continue;
            if (items[i].unreachableCooldown > 0.0f) continue;
            foundItem = i;
            break;
        }

        // If no items left at all, transition to AWAITING_MATERIALS
        if (!anyItemsLeft) {
            bp->state = BLUEPRINT_AWAITING_MATERIALS;
            EventLog("Blueprint %d at (%d,%d,z%d) -> AWAITING_MATERIALS (site cleared)",
                     bpIdx, bp->x, bp->y, bp->z);
            continue;
        }

        if (foundItem < 0) continue;  // all items reserved, skip

        // Find stockpile for this item, or safe-drop
        uint8_t mat = ResolveItemMaterialForJobs(&items[foundItem]);
        int slotX, slotY;
        int spIdx = FindStockpileForItem(items[foundItem].type, mat, &slotX, &slotY);
        bool safeDrop = (spIdx < 0);

        // Check item reachability
        Point itemCell = { (int)(items[foundItem].x / CELL_SIZE),
                           (int)(items[foundItem].y / CELL_SIZE),
                           (int)items[foundItem].z };
        int pathLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
        if (pathLen == 0) continue;

        // Reserve item and stockpile slot (unless safe-drop)
        if (!ReserveItem(foundItem, moverIdx)) continue;
        if (!safeDrop) {
            if (!ReserveStockpileSlot(spIdx, slotX, slotY, moverIdx,
                                       items[foundItem].type, items[foundItem].material)) {
                ReleaseItemReservation(foundItem);
                continue;
            }
        }

        // Create haul or clear job
        int jobId = CreateJob(safeDrop ? JOBTYPE_CLEAR : JOBTYPE_HAUL);
        if (jobId < 0) {
            ReleaseItemReservation(foundItem);
            if (!safeDrop) ReleaseStockpileSlot(spIdx, slotX, slotY);
            continue;
        }

        Job* job = GetJob(jobId);
        job->assignedMover = moverIdx;
        job->targetItem = foundItem;
        job->targetStockpile = safeDrop ? -1 : spIdx;
        job->targetSlotX = safeDrop ? -1 : slotX;
        job->targetSlotY = safeDrop ? -1 : slotY;
        job->step = 0;

        m->currentJobId = jobId;
        m->goal = itemCell;
        m->needsRepath = true;
        RemoveMoverFromIdleList(moverIdx);
        return jobId;
    }

    return -1;
}

// WorkGiver_BlueprintHaul: Find material to haul to a blueprint
// Returns job ID if successful, -1 if no job available

// Filter for recipe-based blueprint haul: matches a specific ConstructionInput
typedef struct {
    const ConstructionInput* input;
    const StageDelivery* delivery;  // for checking locked alternative/material
} RecipeHaulFilterData;

static bool RecipeHaulItemFilter(int itemIdx, void* userData) {
    RecipeHaulFilterData* data = (RecipeHaulFilterData*)userData;
    Item* item = &items[itemIdx];

    if (!item->active) return false;
    if (item->reservedBy != -1) return false;
    if (item->state != ITEM_ON_GROUND) return false;
    if (item->unreachableCooldown > 0.0f) return false;
    {
        int ix = (int)(item->x / CELL_SIZE);
        int iy = (int)(item->y / CELL_SIZE);
        if (!IsExplored(ix, iy, (int)item->z)) return false;
    }

    // Check if item type matches this input slot
    if (!ConstructionInputAcceptsItem(data->input, item->type)) return false;

    // Check locking: if slot has a chosen alternative, must match type and material
    if (data->delivery->chosenAlternative >= 0) {
        ItemType lockedType = data->input->alternatives[data->delivery->chosenAlternative].itemType;
        if (item->type != lockedType) return false;
        if (data->delivery->deliveredMaterial != MAT_NONE) {
            MaterialType mat = (MaterialType)item->material;
            if (mat == MAT_NONE) mat = (MaterialType)DefaultMaterialForItemType(item->type);
            if (mat != data->delivery->deliveredMaterial) return false;
        }
    }

    return true;
}

// Find nearest item matching a recipe input slot (with locking checks)
static int FindNearestRecipeItem(int moverTileX, int moverTileY, int moverZ, float mx, float my,
                                  const ConstructionInput* input, const StageDelivery* delivery) {
    RecipeHaulFilterData filterData = { .input = input, .delivery = delivery };
    int bestItemIdx = -1;
    float bestDistSq = 1e30f;

    // Try spatial grid first
    if (itemGrid.cellCounts && itemGrid.groundItemCount > 0) {
        int radii[] = {10, 25, 50, 100};
        int numRadii = sizeof(radii) / sizeof(radii[0]);
        for (int r = 0; r < numRadii && bestItemIdx < 0; r++) {
            bestItemIdx = FindFirstItemInRadius(moverTileX, moverTileY, moverZ, radii[r],
                                                RecipeHaulItemFilter, &filterData);
        }
        if (bestItemIdx >= 0) {
            float dx = items[bestItemIdx].x - mx;
            float dy = items[bestItemIdx].y - my;
            bestDistSq = dx * dx + dy * dy;
        }
    }

    // Linear scan fallback
    for (int j = 0; j < itemHighWaterMark; j++) {
        Item* item = &items[j];
        if (!item->active) continue;
        if (item->reservedBy != -1) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        {
            int ix = (int)(item->x / CELL_SIZE);
            int iy = (int)(item->y / CELL_SIZE);
            if (!IsExplored(ix, iy, (int)item->z)) continue;
        }
        if (!ConstructionInputAcceptsItem(input, item->type)) continue;

        // Check locking
        if (delivery->chosenAlternative >= 0) {
            ItemType lockedType = input->alternatives[delivery->chosenAlternative].itemType;
            if (item->type != lockedType) continue;
            if (delivery->deliveredMaterial != MAT_NONE) {
                MaterialType mat = (MaterialType)item->material;
                if (mat == MAT_NONE) mat = (MaterialType)DefaultMaterialForItemType(item->type);
                if (mat != delivery->deliveredMaterial) continue;
            }
        }

        float dx = item->x - mx;
        float dy = item->y - my;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestItemIdx = j;
        }
    }

    // Fallback: search inside containers
    if (bestItemIdx < 0) {
        // Try each accepted item type via container search
        for (int a = 0; a < input->altCount && bestItemIdx < 0; a++) {
            ItemType altType = input->alternatives[a].itemType;

            // Check locking
            if (delivery->chosenAlternative >= 0) {
                ItemType lockedType = input->alternatives[delivery->chosenAlternative].itemType;
                if (altType != lockedType) continue;
            }

            int containerIdx = -1;
            bestItemIdx = FindItemInContainers(altType, moverZ, moverTileX, moverTileY,
                                               100, -1, NULL, NULL, &containerIdx);

            // Check material locking on found item
            if (bestItemIdx >= 0 && delivery->chosenAlternative >= 0 &&
                delivery->deliveredMaterial != MAT_NONE) {
                MaterialType mat = (MaterialType)items[bestItemIdx].material;
                if (mat == MAT_NONE) mat = (MaterialType)DefaultMaterialForItemType(items[bestItemIdx].type);
                if (mat != delivery->deliveredMaterial) bestItemIdx = -1;
            }
        }
    }

    return bestItemIdx;
}

// Check if blueprint is reachable from mover position (shared helper)
static bool IsBlueprintReachable(Blueprint* bp, Point moverCell, Point tempPath[]) {
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
    return tempLen > 0;
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

        // Find an unfilled slot, then find a matching item
        const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
        if (!recipe) continue;
        const ConstructionStage* stage = &recipe->stages[bp->stage];

        for (int s = 0; s < stage->inputCount; s++) {
            StageDelivery* sd = &bp->stageDeliveries[s];
            if (sd->deliveredCount + sd->reservedCount >= stage->inputs[s].count) continue;  // slot full

            int itemIdx = FindNearestRecipeItem(moverTileX, moverTileY, moverZ, m->x, m->y,
                                                 &stage->inputs[s], sd);
            if (itemIdx < 0) continue;

            // Check reachability
            if (!IsBlueprintReachable(bp, moverCell, tempPath)) break;  // bp unreachable, skip all slots

            Point itemCell = { (int)(items[itemIdx].x / CELL_SIZE), (int)(items[itemIdx].y / CELL_SIZE), (int)items[itemIdx].z };
            int itemPathLen = FindPath(moverPathAlgorithm, moverCell, itemCell, tempPath, MAX_PATH);
            if (itemPathLen == 0) continue;

            bestBpIdx = bpIdx;
            bestItemIdx = itemIdx;
            break;
        }
        if (bestBpIdx >= 0) break;
    }

    if (bestBpIdx < 0 || bestItemIdx < 0) return -1;

    Blueprint* bp = &blueprints[bestBpIdx];

    // Reserve item
    if (!ReserveItem(bestItemIdx, moverIdx)) return -1;

    // Find which slot this reservation is for and increment reservedCount
    {
        const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
        if (recipe) {
            const ConstructionStage* stage = &recipe->stages[bp->stage];
            ItemType itemType = items[bestItemIdx].type;
            for (int s = 0; s < stage->inputCount; s++) {
                StageDelivery* sd = &bp->stageDeliveries[s];
                if (sd->deliveredCount + sd->reservedCount >= stage->inputs[s].count) continue;
                if (!ConstructionInputAcceptsItem(&stage->inputs[s], itemType)) continue;

                sd->reservedCount++;

                // Lock alternative on first reservation
                if (sd->chosenAlternative < 0 && !stage->inputs[s].anyBuildingMat) {
                    for (int a = 0; a < stage->inputs[s].altCount; a++) {
                        if (stage->inputs[s].alternatives[a].itemType == itemType) {
                            sd->chosenAlternative = a;
                            break;
                        }
                    }
                    // Lock material too
                    MaterialType mat = (MaterialType)items[bestItemIdx].material;
                    if (mat == MAT_NONE) mat = (MaterialType)DefaultMaterialForItemType(itemType);
                    sd->deliveredMaterial = mat;
                }
                break;
            }
        }
    }

    // Create job
    int jobId = CreateJob(JOBTYPE_HAUL_TO_BLUEPRINT);
    if (jobId < 0) {
        // Undo the reservedCount++ we did above
        const ConstructionRecipe* undoRecipe = GetConstructionRecipe(bp->recipeIndex);
        if (undoRecipe) {
            const ConstructionStage* undoStage = &undoRecipe->stages[bp->stage];
            ItemType undoItemType = items[bestItemIdx].type;
            for (int s = 0; s < undoStage->inputCount; s++) {
                StageDelivery* sd = &bp->stageDeliveries[s];
                if (sd->reservedCount <= 0) continue;
                if (!ConstructionInputAcceptsItem(&undoStage->inputs[s], undoItemType)) continue;
                sd->reservedCount--;
                break;
            }
        }
        ReleaseItemReservation(bestItemIdx);
        EventLog("WARNING: CreateJob HAUL_TO_BLUEPRINT failed for bp %d, reservations rolled back", bestBpIdx);
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

// WorkGiver_Hunt: Find a marked animal to hunt
// Returns job ID if successful, -1 if no job available
int WorkGiver_Hunt(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (!m->capabilities.canHunt) return -1;

    // Find nearest marked, unreserved animal
    int bestIdx = -1;
    float bestDistSq = 1e30f;
    int moverZ = (int)m->z;

    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active) continue;
        if (!a->markedForHunt) continue;
        if (a->reservedByHunter >= 0) continue;
        if ((int)a->z != moverZ) continue;
        {
            int ax = (int)(a->x / CELL_SIZE);
            int ay = (int)(a->y / CELL_SIZE);
            if (!IsExplored(ax, ay, (int)a->z)) continue;
        }

        float dx = a->x - m->x;
        float dy = a->y - m->y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }

    if (bestIdx < 0) return -1;

    Animal* target = &animals[bestIdx];

    // Check reachability
    int animalCellX = (int)(target->x / CELL_SIZE);
    int animalCellY = (int)(target->y / CELL_SIZE);
    int animalCellZ = (int)target->z;
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), moverZ };
    Point goalCell = { animalCellX, animalCellY, animalCellZ };

    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, goalCell, tempPath, MAX_PATH);
    if (tempLen == 0) return -1;

    // Create job
    int jobId = CreateJob(JOBTYPE_HUNT);
    if (jobId < 0) return -1;

    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetAnimalIdx = bestIdx;
    job->step = 0;  // HUNT_STEP_CHASING

    // Reserve animal
    target->reservedByHunter = moverIdx;

    // Set mover goal
    m->currentJobId = jobId;
    m->goal = goalCell;
    m->needsRepath = true;

    RemoveMoverFromIdleList(moverIdx);

    return jobId;
}
