# Instant Reassignment

When a mover completes a job mid-tick, immediately assign the next job instead of waiting for next `AssignJobs()` call.

## Implementation

```c
// In JobsTick, when a job completes:
if (result == JOBRUN_DONE) {
    ReleaseJob(m->currentJobId);
    m->currentJobId = -1;
    
    // Try to immediately assign next job
    int jobId = WorkGiver_Haul(moverIdx);
    if (jobId < 0) jobId = WorkGiver_Mining(moverIdx);
    // etc.
    
    if (jobId < 0) {
        AddMoverToIdleList(moverIdx);
    }
}
```

## Benefit

Reduces idle time between jobs.
