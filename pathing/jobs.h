#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>

// =============================================================================
// Job Pool System
// =============================================================================

// Job types - each type has its own driver function
typedef enum {
    JOBTYPE_NONE,
    JOBTYPE_HAUL,              // Pick up item, deliver to stockpile
    JOBTYPE_CLEAR,             // Pick up item, drop outside stockpile (safe-drop)
    JOBTYPE_DIG,               // Mine a wall
    JOBTYPE_HAUL_TO_BLUEPRINT, // Pick up item, deliver to blueprint
    JOBTYPE_BUILD,             // Construct at blueprint
} JobType;

// Job step constants (used in job->step field)
#define STEP_MOVING_TO_PICKUP  0  // Haul/Clear/HaulToBlueprint: moving to item
#define STEP_CARRYING          1  // Haul/Clear/HaulToBlueprint: carrying item to destination
#define STEP_MOVING_TO_WORK    0  // Dig/Build: moving to work site
#define STEP_WORKING           1  // Dig/Build: performing work

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
    int targetDigX;         // Dig designation coordinates
    int targetDigY;
    int targetDigZ;
    int targetBlueprint;    // Blueprint index
    
    // Progress for timed work (digging, building)
    float progress;
    
    // Carried item (for haul jobs)
    int carryingItem;
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
JobRunResult RunJob_Dig(Job* job, void* mover, float dt);
JobRunResult RunJob_HaulToBlueprint(Job* job, void* mover, float dt);
JobRunResult RunJob_Build(Job* job, void* mover, float dt);

// New tick function using job drivers (runs alongside legacy JobsTick during migration)
void JobsTickWithDrivers(void);

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
void AssignJobs(void);   // Match idle movers with available items
void JobsTick(void);     // Update job state machines (check arrivals, handle failures)

// =============================================================================
// WorkGivers (Phase 4 of Jobs Refactor)
// =============================================================================

// WorkGiver functions - each tries to create a job for a specific mover
// Returns job ID if successful, -1 if no job available
// These check capabilities internally
int WorkGiver_Haul(int moverIdx);           // Find ground item to haul to stockpile
int WorkGiver_Mining(int moverIdx);         // Find dig designation to work on
int WorkGiver_Build(int moverIdx);          // Find blueprint to build
int WorkGiver_BlueprintHaul(int moverIdx);  // Find material to haul to blueprint

// New AssignJobs using WorkGiver system (runs alongside legacy during migration)
void AssignJobsWithWorkGivers(void);

#endif
