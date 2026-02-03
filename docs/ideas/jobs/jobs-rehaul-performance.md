# AssignJobs Performance

## Current Status (February 2026)

We now have a single `AssignJobs()` implementation using the hybrid approach:
item-centric for hauling (fast), mover-centric for sparse jobs (mining, building, chopping).

**Benchmark results (500 items, 100 iterations):**
```
           Old Legacy  Old WorkGivers  Old Hybrid   Current (Feb 2026)
10 mov     ~256ms      ~422ms          ~48ms        ~170ms
50 mov     ~50ms       ~2100ms         ~49ms        ~188ms
100 mov    ~48ms       ~4200ms         ~49ms        ~239ms
```

The current implementation is slightly slower than the old Hybrid but still fast.
The old Legacy and WorkGivers variants have been removed.

## The Problem (WorkGivers)

`AssignJobsWorkGivers()` is 40-100x slower than Legacy due to iteration order:

- **Legacy/Hybrid (fast):** Item-centric - O(items)
  ```
  for each item:
      find nearest idle mover via spatial grid (O(1))
      assign job
  ```

- **WorkGivers (slow):** Mover-centric - O(movers × items)
  ```
  for each mover:
      for each WorkGiver:
          search all items to find best job
  ```

With 100 movers and 500 items, WorkGivers do 50,000 item checks per WorkGiver type.

---

## Option 1: Hybrid Iteration (Recommended)

Keep WorkGivers but use the right iteration order for each job type:

```c
void AssignJobsHybrid(void) {
    // Item-centric for hauling (most jobs, many items)
    for each ground item:
        find nearest capable idle mover via spatial grid
        WorkGiver_Haul or WorkGiver_StockpileMaintenance creates job
    
    for each stockpile slot needing rehaul:
        find nearest capable idle mover
        WorkGiver_Rehaul creates job
    
    // Mover-centric only for sparse targets (few designations/blueprints)
    for each idle mover:
        WorkGiver_Mining(mover)      // few dig designations
        WorkGiver_BlueprintHaul(mover)
        WorkGiver_Build(mover)
}
```

**Why this works:**
- Hauling dominates job count (hundreds of ground items)
- Mining/building targets are sparse (tens of designations)
- Item-centric for bulk, mover-centric for sparse = best of both

**Complexity:** Medium - restructure iteration but keep WorkGiver functions.

---

## Option 2: Shared Job Queue

WorkGivers produce candidate jobs instead of assigning directly:

```c
void AssignJobsQueued(void) {
    JobCandidate candidates[MAX_CANDIDATES];
    int count = 0;
    
    // Phase 1: All WorkGivers emit candidates (item-centric)
    for each item:
        emit HaulJob candidate with (mover, item, priority, distance)
    for each designation:
        emit MiningJob candidate
    for each blueprint:
        emit BuildJob candidate
    
    // Phase 2: Sort and assign
    sort candidates by priority (descending), then distance (ascending)
    for each candidate:
        if mover still idle && target still available:
            assign job
}
```

**Pros:**
- Clean separation of "find work" and "assign work"
- Global priority ordering across job types
- Easy to add priority weights

**Cons:**
- Memory allocation for candidate queue
- Two-pass algorithm
- More complex than hybrid

**Complexity:** High

---

## Option 3: Keep Legacy for Assignment

Accept that item-centric assignment is fundamentally faster:

```c
void AssignJobs(void) {
    AssignJobsLegacy();  // Item-centric, battle-tested, fast
}

// WorkGivers remain as:
// - Unit tests for job creation logic
// - Reference implementation
// - Special cases (manual job assignment)
```

**Pros:**
- Already working and fast
- No new code to maintain
- WorkGivers still available for testing

**Cons:**
- Two systems to understand
- WorkGiver modularity unused in production

**Complexity:** None (current state)

---

## Option 4: Spatial Caching for WorkGivers

Make WorkGivers fast by caching spatial queries:

```c
// Pre-build once per tick
SpatialCache itemsByType[ITEM_TYPE_COUNT];
SpatialCache idleMoversByCapability[CAP_COUNT];

void WorkGiver_Haul(Mover* m) {
    // O(1) lookup: nearest item this mover can haul
    Item* item = SpatialNearest(itemsByType[ANY], m->x, m->y, m->capabilities);
    ...
}
```

**Pros:**
- Keeps mover-centric WorkGiver API
- O(1) lookups after cache build

**Cons:**
- Complex cache invalidation
- Cache rebuild cost per tick
- Still slower than item-centric for hauling

**Complexity:** High

---

## Current Architecture

### What's IN USE (production)

**Job Execution:**
- `JobsTick()` runs Job Drivers for all active jobs
- Job Drivers: `RunJob_Haul`, `RunJob_Clear`, `RunJob_Dig`, `RunJob_HaulToBlueprint`, `RunJob_Build`
- All job state lives in `Job` struct, referenced by `mover.currentJobId`

**Job Assignment:**
- `AssignJobs()` delegates to `AssignJobsLegacy()` (item-centric, fast)
- Creates `Job` structs **inline** without calling WorkGiver functions

### What EXISTS but is NOT used in production

**WorkGiver Functions (dead code when using Legacy):**
- `WorkGiver_Haul()`, `WorkGiver_Rehaul()`, `WorkGiver_StockpileMaintenance()` - hauling jobs
- `WorkGiver_Mining()`, `WorkGiver_BlueprintHaul()`, `WorkGiver_Build()` - sparse jobs

These are mover-centric: given a mover, find work for it. They work correctly but are slower for hauling because hauling has many targets (items).

If you stick with `AssignJobsLegacy()`, the WorkGiver code is essentially dead code that only runs in tests/benchmarks.

**AssignJobsHybrid():**
- Uses item-centric iteration for hauling (inline, like Legacy)
- Uses WorkGivers only for sparse jobs (mining, building)
- Performance matches Legacy
- Could replace Legacy if we want cleaner WorkGiver usage for sparse jobs

## Why WorkGivers Are Slower

**Item-centric (Legacy):**
```c
for each item:                         // 500 items
    find nearest idle mover            // O(1) with spatial grid
    create job
// Total: ~500 operations
```

**Mover-centric (WorkGivers):**
```c
for each mover:                        // 100 movers
    WorkGiver_Haul(mover):
        for each item:                 // 500 items
            is this item valid?
            is it closer than best?
        create job for best item
// Total: 100 × 500 = 50,000 operations
```

Plus each WorkGiver rebuilds caches on every call.

**The fundamental issue:**
- Item-centric: "Here's an item, who's closest?" → One search per item
- Mover-centric: "Here's a mover, what's best for them?" → Search ALL items per mover

With 100 movers and 500 items: 500 vs 50,000 comparisons.

---

## Future Improvement Ideas

### Split into two parts

**Part 1: Batch Assignment (current)**
- Runs every tick via `AssignJobs()`
- Item-centric, handles bulk hauling work
- Keep as optimized as possible

**Part 2: Single-Mover Assignment (new idea)**
- Runs when a mover becomes idle mid-tick (after completing a job)
- Mover-centric is fine here - it's ONE mover, not 100
- Call WorkGivers directly for instant reassignment

```c
// In JobsTick, when a job completes:
if (result == JOBRUN_DONE) {
    ReleaseJob(m->currentJobId);
    m->currentJobId = -1;
    
    // NEW: Try to immediately assign next job
    int jobId = WorkGiver_Haul(moverIdx);
    if (jobId < 0) jobId = WorkGiver_Mining(moverIdx);
    // etc.
    
    if (jobId < 0) {
        AddMoverToIdleList(moverIdx);  // Only idle if no work found
    }
}
```

This way:
- Batch assignment stays fast (item-centric)
- Individual movers get instant reassignment (WorkGivers finally useful!)
- WorkGivers have a purpose beyond "slow alternative for benchmarking"

### Optimize WorkGiver_Haul for single-mover use

Current `WorkGiver_Haul` rebuilds caches every call. For single-mover use, we could:
1. Pass pre-built caches as parameters
2. Or have a "light" version that assumes caches are fresh

---

## Files

- `src/jobs.c` - All three AssignJobs variants, all WorkGivers, all Job Drivers
- `src/jobs.h` - Public declarations
- `tests/test_jobs.c` - 120 tests + benchmarks (`./bin/test_jobs -b`)
