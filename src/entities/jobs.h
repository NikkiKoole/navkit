#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>
#include "../world/designations.h"

// =============================================================================
// Job Pool System
// =============================================================================

// Job types - each type has its own driver function
// NOTE: New job types should be added at the END to maintain save compatibility!
typedef enum {
    JOBTYPE_NONE,
    JOBTYPE_HAUL,              // Pick up item, deliver to stockpile
    JOBTYPE_CLEAR,             // Pick up item, drop outside stockpile (safe-drop)
    JOBTYPE_MINE,              // Mine a wall
    JOBTYPE_CHANNEL,           // Channel (vertical dig) - removes floor, mines below
    JOBTYPE_DIG_RAMP,          // Carve a ramp out of wall/dirt
    JOBTYPE_REMOVE_FLOOR,      // Remove constructed floor (mover may fall!)
    JOBTYPE_HAUL_TO_BLUEPRINT, // Pick up item, deliver to blueprint
    JOBTYPE_BUILD,             // Construct at blueprint
    JOBTYPE_CRAFT,             // Fetch materials, craft at workshop
    JOBTYPE_REMOVE_RAMP,       // Remove ramps (natural or carved)
    JOBTYPE_CHOP,              // Chop down tree
    JOBTYPE_GATHER_SAPLING,    // Gather a sapling (dig up, creates item)
    JOBTYPE_PLANT_SAPLING,     // Plant a sapling (haul item, place cell)
    JOBTYPE_CHOP_FELLED,       // Chop up fallen trunks
    JOBTYPE_GATHER_GRASS,      // Gather tall grass (creates grass item)
    JOBTYPE_GATHER_TREE,       // Gather materials from living tree (sticks, leaves)
    JOBTYPE_DELIVER_TO_WORKSHOP, // Haul item to passive workshop work tile
    JOBTYPE_IGNITE_WORKSHOP,     // Walk to semi-passive workshop, do short active work, set passiveReady
    JOBTYPE_CLEAN,               // Clean a dirty floor tile
    JOBTYPE_HARVEST_BERRY,       // Harvest ripe berry bush
    JOBTYPE_KNAP,                // Knap stone at rock wall
    JOBTYPE_DECONSTRUCT_WORKSHOP,  // Tear down a workshop for material refund
    JOBTYPE_HUNT,                    // Hunt a designated animal
    JOBTYPE_DIG_ROOTS,               // Dig roots from natural soil
    JOBTYPE_EXPLORE,                 // Walk straight line toward target (scouting)
    JOBTYPE_TILL,                    // Till soil for farming
    JOBTYPE_TEND_CROP,               // Weed a farm cell
    JOBTYPE_FERTILIZE,               // Apply compost to farm cell
    JOBTYPE_PLANT_CROP,              // Plant seed on tilled farm cell
    JOBTYPE_HARVEST_CROP,            // Harvest ripe crop from farm cell
    JOBTYPE_EQUIP_CLOTHING,          // Pick up clothing item and equip it
    JOBTYPE_COUNT
} JobType;

static inline const char* JobTypeName(int type) {
    static const char* names[JOBTYPE_COUNT] = {
        [JOBTYPE_NONE]                = "NONE",
        [JOBTYPE_HAUL]                = "HAUL",
        [JOBTYPE_CLEAR]               = "CLEAR",
        [JOBTYPE_MINE]                = "MINE",
        [JOBTYPE_CHANNEL]             = "CHANNEL",
        [JOBTYPE_DIG_RAMP]            = "DIG_RAMP",
        [JOBTYPE_REMOVE_FLOOR]        = "REMOVE_FLOOR",
        [JOBTYPE_HAUL_TO_BLUEPRINT]   = "HAUL_TO_BP",
        [JOBTYPE_BUILD]               = "BUILD",
        [JOBTYPE_CRAFT]               = "CRAFT",
        [JOBTYPE_REMOVE_RAMP]         = "REMOVE_RAMP",
        [JOBTYPE_CHOP]                = "CHOP",
        [JOBTYPE_GATHER_SAPLING]      = "GATHER_SAPLING",
        [JOBTYPE_PLANT_SAPLING]       = "PLANT_SAPLING",
        [JOBTYPE_CHOP_FELLED]         = "CHOP_FELLED",
        [JOBTYPE_GATHER_GRASS]        = "GATHER_GRASS",
        [JOBTYPE_GATHER_TREE]         = "GATHER_TREE",
        [JOBTYPE_DELIVER_TO_WORKSHOP] = "DELIVER_TO_WS",
        [JOBTYPE_IGNITE_WORKSHOP]     = "IGNITE_WS",
        [JOBTYPE_CLEAN]               = "CLEAN",
        [JOBTYPE_HARVEST_BERRY]       = "HARVEST_BERRY",
        [JOBTYPE_KNAP]                = "KNAP",
        [JOBTYPE_DECONSTRUCT_WORKSHOP] = "DECONSTRUCT_WS",
        [JOBTYPE_HUNT]                 = "HUNT",
        [JOBTYPE_DIG_ROOTS]            = "DIG_ROOTS",
        [JOBTYPE_EXPLORE]              = "EXPLORE",
        [JOBTYPE_TILL]                 = "TILL",
        [JOBTYPE_TEND_CROP]            = "TEND_CROP",
        [JOBTYPE_FERTILIZE]            = "FERTILIZE",
        [JOBTYPE_PLANT_CROP]           = "PLANT_CROP",
        [JOBTYPE_HARVEST_CROP]         = "HARVEST_CROP",
        [JOBTYPE_EQUIP_CLOTHING]       = "EQUIP_CLOTHING",
    };
    return (type >= 0 && type < JOBTYPE_COUNT) ? names[type] : "?";
}

// Job step constants (used in job->step field)
#define STEP_MOVING_TO_PICKUP  0  // Haul/Clear/HaulToBlueprint: moving to item
#define STEP_CARRYING          1  // Haul/Clear/HaulToBlueprint: carrying item to destination
#define STEP_MOVING_TO_WORK    0  // Dig/Build: moving to work site
#define STEP_WORKING           1  // Dig/Build: performing work
#define STEP_PLANTING          2  // PlantSapling: working after carrying

// Craft job steps (13-step state machine)
// Steps 4-6 only used when recipe has second input
// Steps 7-9 only used when recipe has third input
// Steps 10-12 only used when recipe needs fuel
#define CRAFT_STEP_MOVING_TO_INPUT   0  // Walking to first input item
#define CRAFT_STEP_PICKING_UP        1  // At first input, picking up
#define CRAFT_STEP_MOVING_TO_WORKSHOP 2 // Carrying first input to workshop
#define CRAFT_STEP_WORKING           3  // At workshop, crafting
#define CRAFT_STEP_MOVING_TO_INPUT2  4  // Walking to second input (after depositing first)
#define CRAFT_STEP_PICKING_UP_INPUT2 5  // At second input, picking up
#define CRAFT_STEP_CARRYING_INPUT2   6  // Carrying second input to workshop
#define CRAFT_STEP_MOVING_TO_INPUT3  7  // Walking to third input
#define CRAFT_STEP_PICKING_UP_INPUT3 8  // At third input, picking up
#define CRAFT_STEP_CARRYING_INPUT3   9  // Carrying third input to workshop
#define CRAFT_STEP_MOVING_TO_FUEL   10  // Walking to fuel item
#define CRAFT_STEP_PICKING_UP_FUEL  11  // At fuel item, picking up
#define CRAFT_STEP_CARRYING_FUEL    12  // Carrying fuel to workshop

// Tool fetch step (shared by designation and craft jobs)
// High value to avoid collision with existing step numbering
#define STEP_FETCHING_TOOL          13 // Walking to tool item, equips on arrival

// Job struct - contains all data for a single job
typedef struct {
    bool active;
    JobType type;
    int assignedMover;      // Mover index, -1 if unassigned
    int step;               // Sub-state within job (0=moving to pickup, 1=carrying, etc)

    // Targets (interpret based on job type)
    int targetItem;         // Item to pick up
    int targetStockpile;    // Destination stockpile
    int targetSlotX;        // Stockpile slot coordinates
    int targetSlotY;
    int targetMineX;        // Mine designation coordinates
    int targetMineY;
    int targetMineZ;
    int targetAdjX;         // Adjacent tile to stand on while mining (cached)
    int targetAdjY;
    int targetBlueprint;    // Blueprint index

    // Craft job targets
    int targetWorkshop;     // Workshop index
    int targetBillIdx;      // Bill index within workshop
    float workRequired;     // Work time from recipe (seconds)

    // Progress for timed work (digging, building, crafting)
    float progress;

    // Carried item (for haul jobs and craft jobs)
    int carryingItem;
    
    // Fuel item (for craft jobs with fuel requirement)
    int fuelItem;  // reserved fuel item index, -1 = none

    // Second input item (for craft jobs with two inputs)
    int targetItem2;  // reserved second input item index, -1 = none

    // Third input item (for craft jobs with three inputs)
    int targetItem3;  // reserved third input item index, -1 = none

    // Tool item (for jobs that need a tool fetch before starting)
    int toolItem;  // reserved tool item to pick up, -1 = none/already equipped

    // Hunt target
    int targetAnimalIdx;  // Animal index for hunt jobs, -1 = none
} Job;

#define MAX_JOBS 10000

// Job pool
extern Job jobs[MAX_JOBS];
extern int jobHighWaterMark;    // Highest index ever used + 1

// Free list for O(1) job allocation
extern int* jobFreeList;        // Stack of free job indices
extern int jobFreeCount;        // Number of free slots available

// Active job tracking for O(n) iteration where n = active jobs
extern int* activeJobList;      // List of active job indices
extern int activeJobCount;      // Number of active jobs
extern bool* jobIsActive;       // Quick lookup: is job in active list?

// Job pool functions
void InitJobPool(void);
void FreeJobPool(void);
void ClearJobs(void);
int CreateJob(JobType type);    // Returns job index, -1 if pool full
void ReleaseJob(int jobId);     // Release job back to pool
Job* GetJob(int jobId);         // Get job by index (NULL if invalid)

// =============================================================================
// Job Drivers (Phase 2 of Jobs Refactor)
// =============================================================================

// Job execution result
typedef enum {
    JOBRUN_RUNNING,     // Job still in progress
    JOBRUN_DONE,        // Job completed successfully
    JOBRUN_FAIL,        // Job failed, should be cancelled
} JobRunResult;

// Note: Include mover.h before jobs.h for Mover type
// Job driver function type (Mover defined in mover.h)
typedef JobRunResult (*JobDriver)(Job* job, void* mover, float dt);

// Per-type job drivers (use void* in header to avoid circular dependency)
// Implementation in jobs.c casts to Mover*
JobRunResult RunJob_Haul(Job* job, void* mover, float dt);
JobRunResult RunJob_Clear(Job* job, void* mover, float dt);
JobRunResult RunJob_Mine(Job* job, void* mover, float dt);
JobRunResult RunJob_Channel(Job* job, void* mover, float dt);
JobRunResult RunJob_DigRamp(Job* job, void* mover, float dt);
JobRunResult RunJob_RemoveFloor(Job* job, void* mover, float dt);
JobRunResult RunJob_RemoveRamp(Job* job, void* mover, float dt);
JobRunResult RunJob_HaulToBlueprint(Job* job, void* mover, float dt);
JobRunResult RunJob_Build(Job* job, void* mover, float dt);
JobRunResult RunJob_Craft(Job* job, void* mover, float dt);
JobRunResult RunJob_Chop(Job* job, void* mover, float dt);
JobRunResult RunJob_ChopFelled(Job* job, void* mover, float dt);
JobRunResult RunJob_GatherSapling(Job* job, void* mover, float dt);
JobRunResult RunJob_PlantSapling(Job* job, void* mover, float dt);
JobRunResult RunJob_GatherTree(Job* job, void* mover, float dt);
JobRunResult RunJob_GatherGrass(Job* job, void* mover, float dt);
JobRunResult RunJob_DeliverToWorkshop(Job* job, void* mover, float dt);
JobRunResult RunJob_IgniteWorkshop(Job* job, void* mover, float dt);
JobRunResult RunJob_Clean(Job* job, void* mover, float dt);
JobRunResult RunJob_Knap(Job* job, void* mover, float dt);
JobRunResult RunJob_DeconstructWorkshop(Job* job, void* mover, float dt);
JobRunResult RunJob_Hunt(Job* job, void* mover, float dt);
JobRunResult RunJob_DigRoots(Job* job, void* mover, float dt);
JobRunResult RunJob_Explore(Job* job, void* mover, float dt);
JobRunResult RunJob_Till(Job* job, void* mover, float dt);
JobRunResult RunJob_TendCrop(Job* job, void* mover, float dt);
JobRunResult RunJob_Fertilize(Job* job, void* mover, float dt);
JobRunResult RunJob_PlantCrop(Job* job, void* mover, float dt);
JobRunResult RunJob_HarvestCrop(Job* job, void* mover, float dt);
JobRunResult RunJob_EquipClothing(Job* job, void* mover, float dt);

// Idle mover cache - maintained incrementally instead of scanning all movers
extern int* idleMoverList;      // Array of mover indices that are idle
extern int idleMoverCount;      // Number of idle movers
extern bool* moverIsInIdleList; // Quick lookup: is mover in idle list?

// Initialize/free the job system caches
void InitJobSystem(int maxMovers);
void FreeJobSystem(void);

// Idle list management (called when mover state changes)
void AddMoverToIdleList(int moverIdx);
void RemoveMoverFromIdleList(int moverIdx);
void RebuildIdleMoverList(void);  // Full rebuild (e.g., after ClearMovers)

// Core functions
void AssignJobs(void);           // Match idle movers with available jobs
void RebuildMineDesignationCache(void);  // Build cache for WorkGiver_Mining (call before mining assignment)
void InvalidateDesignationCache(DesignationType type);  // Mark cache dirty when designation added/removed
void JobsTick(void);             // Update job state machines using per-type drivers

// =============================================================================
// WorkGivers (Phase 4 of Jobs Refactor)
// =============================================================================

// WorkGiver functions - each tries to create a job for a specific mover
// Returns job ID if successful, -1 if no job available
// These check capabilities internally
// Priority order (high to low):
//   1. StockpileMaintenance - absorb/clear ground items on stockpile tiles
//   2. Haul - ground items to stockpiles
//   3. Rehaul - transfer from overfull/low-priority stockpiles
//   4. Mining - mine designations
//   5. BlueprintHaul - materials to blueprints
//   6. Build - construct at blueprints
int WorkGiver_StockpileMaintenance(int moverIdx);
int WorkGiver_Haul(int moverIdx);
int WorkGiver_Rehaul(int moverIdx);
int WorkGiver_Mining(int moverIdx);
int WorkGiver_Channel(int moverIdx);
int WorkGiver_DigRamp(int moverIdx);
int WorkGiver_RemoveFloor(int moverIdx);
int WorkGiver_RemoveRamp(int moverIdx);
int WorkGiver_Chop(int moverIdx);
int WorkGiver_ChopFelled(int moverIdx);
int WorkGiver_BlueprintClear(int moverIdx);
int WorkGiver_BlueprintHaul(int moverIdx);
int WorkGiver_Build(int moverIdx);
int WorkGiver_Craft(int moverIdx);
int WorkGiver_GatherSapling(int moverIdx);
int WorkGiver_PlantSapling(int moverIdx);
int WorkGiver_GatherGrass(int moverIdx);
int WorkGiver_GatherTree(int moverIdx);
int WorkGiver_DeliverToPassiveWorkshop(int moverIdx);
int WorkGiver_IgniteWorkshop(int moverIdx);
int WorkGiver_Clean(int moverIdx);
int WorkGiver_Knap(int moverIdx);
int WorkGiver_DeconstructWorkshop(int moverIdx);
int WorkGiver_Hunt(int moverIdx);
int WorkGiver_DigRoots(int moverIdx);
int WorkGiver_Explore(int moverIdx);
int WorkGiver_Till(int moverIdx);
int WorkGiver_TendCrop(int moverIdx);
int WorkGiver_Fertilize(int moverIdx);
int WorkGiver_PlantCrop(int moverIdx);
int WorkGiver_HarvestCrop(int moverIdx);
int WorkGiver_EquipClothing(int moverIdx);

// Job cancellation (releases all reservations, safe-drops carried items, returns mover to idle)
void CancelJob(void* mover, int moverIdx);  // void* to avoid circular dependency with mover.h

// Job unassignment (like CancelJob but preserves designation progress)
// Use when mover needs to temporarily stop working (hunger/exhaustion interrupt)
void UnassignJob(void* mover, int moverIdx);

#endif
