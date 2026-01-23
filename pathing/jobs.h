#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>

// JobState is defined in mover.h to avoid circular dependency

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

#endif
