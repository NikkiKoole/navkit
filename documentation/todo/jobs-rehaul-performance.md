# WorkGiver Performance Optimization

## Current Status

We have three `AssignJobs` variants:

| Variant | Speed | Used In Production | Notes |
|---------|-------|-------------------|-------|
| `AssignJobsLegacy()` | Fast | **Yes** (via `AssignJobs()`) | Item-centric, inline job creation |
| `AssignJobsHybrid()` | Fast | No (available) | Item-centric hauling + WorkGivers for sparse jobs |
| `AssignJobsWorkGivers()` | Slow | No (for testing) | Mover-centric, uses all WorkGivers |

**Benchmark results (500 items):**
```
           Legacy      WorkGivers    Hybrid
10 mov     ~256ms      ~422ms        ~48ms   (Hybrid 5x faster!)
50 mov     ~50ms       ~2100ms       ~49ms   (equal)
100 mov    ~48ms       ~4200ms       ~49ms   (equal)
```

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
- Creates `Job` structs inline without calling WorkGiver functions

### What EXISTS but is NOT used in production

**WorkGiver Functions:**
- `WorkGiver_Haul()`, `WorkGiver_Rehaul()`, `WorkGiver_StockpileMaintenance()` - hauling jobs
- `WorkGiver_Mining()`, `WorkGiver_BlueprintHaul()`, `WorkGiver_Build()` - sparse jobs

These are mover-centric: given a mover, find work for it. They work correctly but are slower for hauling because hauling has many targets (items).

**AssignJobsHybrid():**
- Uses item-centric iteration for hauling (inline, like Legacy)
- Uses WorkGivers only for sparse jobs (mining, building)
- Performance matches Legacy
- Could replace Legacy if we want cleaner WorkGiver usage for sparse jobs

---

## Future Improvement Ideas

### Use WorkGivers for targeted job assignment

Currently we never call `WorkGiver_Haul()` in production. But there's a use case:

**Scenario:** A mover just finished a job and is now idle. Instead of waiting for next `AssignJobs()` tick, we could immediately call:
```c
// Mover just became idle - find work immediately
int jobId = WorkGiver_Haul(moverIdx);
if (jobId < 0) jobId = WorkGiver_Mining(moverIdx);
// etc.
```

This would give instant job assignment for individual movers. The WorkGiver functions are designed for exactly this - they're just slow when called for ALL movers in a loop.

### Optimize WorkGiver_Haul for single-mover use

Current `WorkGiver_Haul` rebuilds caches every call. For single-mover use, we could:
1. Pass pre-built caches as parameters
2. Or have a "light" version that assumes caches are fresh

---

## Files

- `pathing/jobs.c` - All three AssignJobs variants, all WorkGivers, all Job Drivers
- `pathing/jobs.h` - Public declarations
- `tests/test_jobs.c` - 120 tests + benchmarks (`./bin/test_jobs -b`)
