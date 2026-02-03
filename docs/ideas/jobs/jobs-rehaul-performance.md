# AssignJobs Performance

## Current Status (February 2026)

We now have a single `AssignJobs()` implementation using the hybrid approach:
item-centric for hauling (fast), mover-centric for sparse jobs (mining, building, crafting, chopping).

Legacy and WorkGivers-only variants have been removed.

**Benchmark results (500 items, 100 iterations):**
```
--- AssignJobs (Cold vs Warm) ---
  Cold: pathfind every item each iteration (worst case)
  Warm: skip items marked unreachable (steady state)

   10 movers: Cold=173ms  Warm=18ms
   50 movers: Cold=189ms  Warm=21ms
  100 movers: Cold=236ms  Warm=18ms
```

**What Cold vs Warm means:**
- **Cold:** Every iteration resets `unreachableCooldown`, forcing pathfinding on all items
- **Warm:** Items marked unreachable stay marked, so they're skipped quickly

The Warm case (~18-21ms) represents steady-state gameplay where unreachable items accumulate cooldowns over time.

**Historical comparison:**
The old benchmark showed Hybrid at ~48ms, but this was misleading - Legacy ran first and marked unreachable items, so Hybrid benefited from "free" warmup. True steady-state performance is actually ~18-21ms, roughly **2.5x faster** than previously reported.

---

## Architecture

### Job Assignment (AssignJobs)

Single hybrid implementation:
- **Item-centric for hauling** (most jobs, many items) - O(items)
- **Mover-centric for sparse jobs** (mining, building, crafting) - only runs if work exists

```c
void AssignJobs(void) {
    // PRIORITY 1: Stockpile maintenance (absorb/clear)
    // PRIORITY 2: Crafting
    // PRIORITY 3a: Stockpile-centric hauling
    // PRIORITY 3b: Item-centric fallback
    // PRIORITY 4: Re-haul from overfull stockpiles
    // PRIORITY 5-8: Mining, Channel, RemoveFloor, RemoveRamp, Chop (via WorkGivers)
    // PRIORITY 9-10: Blueprint haul, Build
}
```

### Job Execution (JobsTick)

Uses Job Drivers for clean per-type logic:
- `RunJob_Haul`, `RunJob_Clear`, `RunJob_Mine`, `RunJob_Channel`
- `RunJob_RemoveFloor`, `RunJob_RemoveRamp`, `RunJob_Chop`
- `RunJob_HaulToBlueprint`, `RunJob_Build`, `RunJob_Craft`

All job state lives in `Job` struct, referenced by `mover.currentJobId`.

---

## Why Hybrid Works

The key insight: **hauling dominates job count** (hundreds of items) while mining/building targets are sparse (tens of designations/blueprints).

**Item-centric (for hauling):**
```c
for each item:                         // 500 items
    find nearest idle mover            // O(1) with spatial grid
    create job
// Total: ~500 operations
```

**Mover-centric (for sparse jobs):**
```c
for each idle mover:                   // Only runs if sparse work exists
    WorkGiver_Mining(mover)            // Few dig designations
    WorkGiver_Build(mover)             // Few blueprints
```

This gives us the best of both worlds:
- Fast O(items) iteration for the bulk of jobs
- WorkGiver modularity for sparse job types

---

## Future Improvement Ideas

### Instant Reassignment

When a mover completes a job mid-tick, instead of waiting for next `AssignJobs()` call:

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

This would reduce idle time between jobs.

---

## Files

- `src/entities/jobs.c` - AssignJobs, WorkGivers, Job Drivers
- `src/entities/jobs.h` - Public declarations
- `tests/bench_jobs.c` - Benchmarks
- `tests/test_jobs.c` - 137 tests
