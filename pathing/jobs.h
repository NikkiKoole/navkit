#ifndef JOBS_H
#define JOBS_H

// JobState is defined in mover.h to avoid circular dependency

// Core functions
void AssignJobs(void);   // Check idle movers and assign items to them
void JobsTick(void);     // Update job state machines (check arrivals, handle failures)

#endif
