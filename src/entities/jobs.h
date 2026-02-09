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
} JobType;

// Job step constants (used in job->step field)
#define STEP_MOVING_TO_PICKUP  0  // Haul/Clear/HaulToBlueprint: moving to item
#define STEP_CARRYING          1  // Haul/Clear/HaulToBlueprint: carrying item to destination
#define STEP_MOVING_TO_WORK    0  // Dig/Build: moving to work site
#define STEP_WORKING           1  // Dig/Build: performing work
#define STEP_PLANTING          2  // PlantSapling: working after carrying

// Craft job steps (10-step state machine)
// Steps 4-6 only used when recipe has second input
// Steps 7-9 only used when recipe needs fuel
#define CRAFT_STEP_MOVING_TO_INPUT   0  // Walking to first input item
#define CRAFT_STEP_PICKING_UP        1  // At first input, picking up
#define CRAFT_STEP_MOVING_TO_WORKSHOP 2 // Carrying first input to workshop
#define CRAFT_STEP_WORKING           3  // At workshop, crafting
#define CRAFT_STEP_MOVING_TO_INPUT2  4  // Walking to second input (after depositing first)
#define CRAFT_STEP_PICKING_UP_INPUT2 5  // At second input, picking up
#define CRAFT_STEP_CARRYING_INPUT2   6  // Carrying second input to workshop
#define CRAFT_STEP_MOVING_TO_FUEL    7  // Walking to fuel item
#define CRAFT_STEP_PICKING_UP_FUEL   8  // At fuel item, picking up
#define CRAFT_STEP_CARRYING_FUEL     9  // Carrying fuel to workshop

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
JobRunResult RunJob_RemoveFloor(Job* job, void* mover, float dt);
JobRunResult RunJob_RemoveRamp(Job* job, void* mover, float dt);
JobRunResult RunJob_HaulToBlueprint(Job* job, void* mover, float dt);
JobRunResult RunJob_Build(Job* job, void* mover, float dt);
JobRunResult RunJob_Craft(Job* job, void* mover, float dt);
JobRunResult RunJob_GatherSapling(Job* job, void* mover, float dt);
JobRunResult RunJob_PlantSapling(Job* job, void* mover, float dt);



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
int WorkGiver_BlueprintHaul(int moverIdx);
int WorkGiver_Build(int moverIdx);
int WorkGiver_Craft(int moverIdx);
int WorkGiver_GatherSapling(int moverIdx);
int WorkGiver_PlantSapling(int moverIdx);

// Job cancellation (releases all reservations, safe-drops carried items, returns mover to idle)
void CancelJob(void* mover, int moverIdx);  // void* to avoid circular dependency with mover.h

#endif
