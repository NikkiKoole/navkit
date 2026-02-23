# Performance & DOD Audit: Job System (`src/entities/jobs.c`)

Deep-dive audit of `jobs.c` through Casey Muratori's data-oriented design lens. This file supplements the main `performance-dod-audit.md` with jobs-specific findings.

**File size:** ~3,800 lines (largest file in the codebase)

---

## CRITICAL (10x+ performance impact)

### 1. WorkGiver_Craft: O(workshops × bills × items) Per Idle Crafter

**Location:** `src/entities/jobs.c:4454-4705`

`WorkGiver_Craft` does a triple-nested iteration: for each workshop (MAX_WORKSHOPS=256), for each bill (up to 10), scan ALL items (`itemHighWaterMark`, up to 25,000) to find matching inputs. This runs once per idle mover with crafting capability.

With 10 idle crafters, 20 active workshops, 3 bills each, and 5,000 items:
- **10 × 20 × 3 × 5,000 = 3,000,000 item checks per frame**

The item scan (line ~4540) touches `Item` structs sequentially but checks `type`, `state`, `reservedBy`, `unreachableCooldown`, `stackCount`, `x`, `y` — that's ~30 bytes of a 48-byte struct, which is okay for cache. The problem is the sheer volume.

**Bytes fetched:** 3M × 48 bytes = **144 MB of item data per frame**, just for craft assignment.

**Fix:** Build a per-type item index: `int itemsByType[ITEM_TYPE_COUNT][maxPerType]`. Craft searches only iterate matching types. If 5,000 items span 40 types, average list length is 125 instead of 5,000 — a **40× reduction**. Rebuild incrementally on item spawn/delete/state change.

**Alternative fix:** Spatial index per item type near workshops. WorkGiver_Craft only needs items within `bill->ingredientSearchRadius` of the workshop.

### 2. WorkGiver_DeliverToPassiveWorkshop: Double Full Item Scan

**Location:** `src/entities/jobs.c:3739-3881`

For each passive workshop, this function scans ALL items twice:
1. Count matching items already on/heading to work tile (line ~3771)
2. Find nearest matching available item (line ~3805)

With 5 passive workshops and 5,000 items: **2 × 5 × 5,000 = 50,000 item checks per mover**.

Called for every idle mover in `AssignJobs_P2b`, so with 20 idle movers: **1,000,000 item checks/frame**.

**Fix:** Same per-type item index as above. Both scans only need items of the recipe's input type.

### 3. Designation Cache Rebuilds Scan Full 3D Grid

**Location:** `src/entities/jobs.c:242-344`

`RebuildAdjacentDesignationCache()` and `RebuildOnTileDesignationCache()` iterate `gridDepth × gridHeight × gridWidth` per designation type. With 15 designation types (mine, channel, dig_ramp, remove_floor, remove_ramp, chop, chop_felled, gather_sapling, plant_sapling, gather_grass, gather_tree, clean, harvest_berry, knap) and a 128×128×16 grid:

- **15 × 128 × 128 × 16 = 3,932,160 cells checked** per full rebuild

The dirty flag system helps — caches only rebuild when designations change. But each `GetDesignation(x,y,z)` is a function call with bounds checking, and `FindAdjacentWalkable` does 4 directional checks with `IsCellWalkableAt` per found designation.

**When it's bad:** Placing/removing a single designation dirties that type's cache, triggering a full grid scan next frame.

**Fix:** Maintain a sparse designation list per type. When a designation is placed, append `{x,y,z}` to the list for its type. When removed, swap-remove from the list. Cache rebuild iterates only the designation list (typically 5-200 entries), not the entire grid. This turns a 4M-cell scan into a 200-entry scan — **20,000× improvement**.

---

## HIGH (2-10x impact)

### 4. Per-Frame malloc/free in 6+ Assignment Phases

**Location:** `src/entities/jobs.c` lines ~2987, 3012, 3040, 3261

Six job assignment phases each `malloc` a snapshot of `idleMoverList`, iterate it, and `free` it:

```c
// Pattern repeated 6 times:
int* idleCopy = (int*)malloc(idleMoverCount * sizeof(int));
memcpy(idleCopy, idleMoverList, idleMoverCount * sizeof(int));
// ... iterate ...
free(idleCopy);
```

With 50 idle movers, each allocation is 200 bytes — tiny, but `malloc`/`free` have overhead (lock acquisition, free-list search, fragmentation). **6-7 heap round-trips per frame** is unnecessary.

**Fix:** Pre-allocate a static buffer at init:
```c
static int idleWorkBuffer[MAX_MOVERS];
// In each phase:
int copyCount = idleMoverCount;
memcpy(idleWorkBuffer, idleMoverList, copyCount * sizeof(int));
```

Zero allocations. Static buffer is 40KB (MAX_MOVERS × 4 bytes), always warm in cache since it's reused every phase.

### 5. TryAssignItemToMover: FindPath as Reachability Check

**Location:** `src/entities/jobs.c:2788-2900`

`TryAssignItemToMover` calls `FindPath()` to verify reachability before creating a job. It allocates a `Point tempPath[MAX_PATH]` (65,536 × 12 bytes = **786KB on the stack**) just to check if a path exists — the actual path is thrown away.

This runs up to 3 times per item (retry with excluded movers). If 50 items need hauling, that's **up to 150 pathfind calls per frame**, each touching 786KB of stack memory that thrashes L1/L2 cache.

**Fix:**
1. Use a reachability-only pathfind mode that returns bool without building the path array (saves 786KB stack per call).
2. Use the HPA* precomputed chunk connectivity for O(1) reachability checks — if two cells are in disconnected abstract regions, skip pathfinding entirely.
3. Cache unreachable pairs with a short TTL instead of retrying every frame.

### 6. AssignJobs_P3e_ContainerCleanup: Triple-Nested Loop

**Location:** `src/entities/jobs.c:3149-3187`

```
for (stockpiles)          // 64 max
  for (slots per stockpile)  // up to 1024
    for (all items)          // up to 25,000
```

Worst case: **64 × 1024 × 25,000 = 1.6 billion iterations**. In practice, only a few stockpiles have containers and the inner loop has early-outs, but the structure is dangerous.

**Fix:** Instead of scanning all items to find container contents, use the `containedIn` field on items. Build a reverse index: for each container, maintain a list of contained item indices. Then the inner loop only checks items actually in that container (typically 1-5), not all 25,000.

### 7. AssignJobs_P3d_Consolidate: O(stockpiles × items)

**Location:** `src/entities/jobs.c:3188-3213`

```
for (stockpiles)    // 64 max
  for (all items)   // up to 25,000
    IsPositionInStockpile()  // coordinate check
```

**64 × 25,000 = 1.6M iterations** to find items in each stockpile. Most items aren't in stockpiles.

**Fix:** Pre-build a per-stockpile item list during cache rebuild. Each stockpile already has `slots[]` pointing to item indices — iterate only occupied slots, not all items.

### 8. RebuildStockpileSlotCache: O(types × materials × stockpiles)

**Location:** Called from `AssignJobs()` line ~3310

```c
for (int t = 0; t < ITEM_TYPE_COUNT; t++)        // 52 types
  for (int m = 0; m < MAT_COUNT; m++)              // 16 materials
    FindStockpileForItem(t, m, &slotX, &slotY);    // iterates all stockpiles
```

`FindStockpileForItem` iterates up to 64 stockpiles checking filters. Total: **52 × 16 × 64 = 53,248 stockpile filter checks** per frame when cache is dirty.

With dirty flag, this only rebuilds when stockpile state changes. But any item assignment calls `InvalidateStockpileSlotCache`, making it dirty again. In practice, **rebuilds almost every frame** during active hauling.

**Fix:** Invalidate per-(type, material) pair instead of the entire cache. When hauling a LOG/OAK, only invalidate `stockpileSlotCache[ITEM_LOG][MAT_OAK]`, not all 832 entries.

---

## MEDIUM (measurable but modest)

### 9. RebuildIdleMoverList: memset + Full Scan Every Frame

**Location:** `src/entities/jobs.c:2429-2446`

```c
memset(moverIsInIdleList, 0, idleMoverCapacity * sizeof(bool));  // 10KB
for (int i = 0; i < moverCount; i++) {
    // Check active, jobId, freetimeState, walkability
}
```

10KB memset + full mover scan with `IsCellWalkableAt()` per mover. With the 12.4KB Mover struct, iterating `moverCount` movers touches `moverCount × 12.4KB` of mover data just to read 3 fields.

**Fix:** Track idle state incrementally (already partially done with `AddMoverToIdleList`/`RemoveMoverFromIdleList`). Only do full rebuild when walkability changes (cell modification events). Save the memset + scan on frames where no cells changed.

### 10. RemoveMoverFromIdleList: O(n) Linear Scan

**Location:** `src/entities/jobs.c:2414-2427`

```c
for (int i = 0; i < idleMoverCount; i++) {
    if (idleMoverList[i] == moverIdx) {
        idleMoverList[i] = idleMoverList[idleMoverCount - 1];
        idleMoverCount--;
        break;
    }
}
```

Uses swap-remove (good) but linear search to find the index (bad). Called every time a mover gets a job. With 50 idle movers, that's 50 comparisons worst case per removal — acceptable, but:

**Fix:** Maintain a reverse index `idleMoverPosition[moverIdx] = positionInList`. Removal becomes O(1): read position, swap with last, update swapped element's position. Add 40KB (MAX_MOVERS × 4 bytes) of memory.

### 11. WorkGiver Functions: Redundant Workshop Scans

Multiple WorkGiver functions independently scan `MAX_WORKSHOPS`:
- `WorkGiver_Craft` (line ~4454)
- `WorkGiver_DeliverToPassiveWorkshop` (line ~3739)
- `WorkGiver_IgniteWorkshop`
- `WorkGiver_Build`
- `WorkGiver_DeconstructWorkshop`

Each does `for (int w = 0; w < MAX_WORKSHOPS; w++)` with `if (!ws->active) continue`.

**Fix:** Build a per-type active workshop list once per frame: `activeWorkshopIndices[]`, `passiveWorkshopIndices[]`, `deconstructWorkshopIndices[]`. Each WorkGiver iterates only the relevant list.

### 12. FindPath Stack Allocation: 786KB Per Call

**Location:** Multiple call sites in WorkGivers

```c
Point tempPath[MAX_PATH];  // 65536 × 12 = 786,432 bytes
int tempLen = FindPath(..., tempPath, MAX_PATH);
```

Each `FindPath` call puts 786KB on the stack. Multiple pathfinds per frame (craft jobs do 3-4 in sequence) repeatedly push/pop this. The stack memory thrashes L1/L2 cache.

**Fix:** Use a single static `Point pathBuffer[MAX_PATH]` shared across all reachability checks (they don't overlap). Or better, implement a reachability-only check that doesn't need a path buffer at all.

---

## LOW (minor or cold-path only)

### 13. JobDriver Function Pointer Dispatch

`jobDrivers[job->type]` indirect call per active job per frame. With ~20 job types, branch prediction handles this reasonably. Only becomes an issue with 1000+ active jobs — currently MAX_JOBS=10000 but typical active count is much lower.

### 14. EventLog() String Formatting in Job Completion

`EventLog("Job %d DONE type=%s mover=%d", ...)` does `vsnprintf` on every job completion/failure. Cold path (only fires on state transitions), negligible impact.

### 15. Designation Cache Static Memory

15 caches × 4096 entries × 20 bytes = **1.2MB** of static memory. Most caches are nearly empty. Could use dynamic sizing, but the memory is pre-allocated and doesn't cause cache issues (only accessed when populated).

---

## ALREADY GOOD

These patterns in jobs.c are well-designed:

- **Idle mover list**: Separate tracking avoids scanning all movers for most operations
- **Designation dirty flags**: Caches only rebuild when state changes (though the rebuild itself is the problem)
- **Early termination**: Every phase checks `idleMoverCount == 0` and returns immediately
- **Spatial grid for mover search**: `QueryMoverNeighbors()` in `TryAssignItemToMover` avoids linear mover scan
- **Item spatial grid (itemGrid)**: `AssignJobs_P3_ItemHaul` uses spatial grid for ground items
- **Stockpile slot cache**: `FindStockpileForItemCached()` avoids repeated stockpile filter iteration
- **Priority-ordered phases**: Highest-priority work (maintenance, crafting) runs first, exits early when all movers busy
- **Unreachable cooldown**: Items that can't be reached get a 5-second cooldown, preventing repeated pathfind failures
- **Compact Job struct**: ~100 bytes, fits 2-3 per cache line. Good density for iteration.
- **Active job list indirection**: `activeJobList[]` mentioned in CLAUDE.md avoids scanning inactive jobs
- **typeMatHasStockpile early-out**: Pre-computed bool array skips items with no valid stockpile destination

---

## IMPACT SUMMARY TABLE

| # | Issue | Memory/Cache Impact | Effort | Estimated Speedup |
|---|-------|-------------------|--------|-------------------|
| 1 | Per-type item index for WorkGiver_Craft | 144MB/frame → <4MB/frame | Medium | 10-40× for craft assignment |
| 2 | Per-type item index for passive delivery | 50K→1K item checks/mover | Medium (same as #1) | 10-40× for delivery |
| 3 | Sparse designation lists | 4M cell scan → 200 entry scan | Medium | 100-20,000× for cache rebuild |
| 4 | Static idle mover buffer | 6-7 malloc/free → 0 | Small | Minor (removes overhead) |
| 5 | Reachability-only pathfind | 786KB stack/call → bool check | Medium | 2-5× for job assignment |
| 6 | Reverse container index | 1.6B worst-case → container size | Medium | 10-1000× for container cleanup |
| 7 | Per-stockpile item list | 1.6M iterations → occupied slots only | Small | 5-50× for consolidation |
| 8 | Per-key slot cache invalidation | 53K checks/frame → ~100/frame | Small | 10-500× for cache rebuild |
| 9 | Incremental idle list | 10KB memset + scan → delta | Small | 2× for idle rebuild |
| 10 | Reverse idle index | O(n) remove → O(1) | Small | Minor |
| 11 | Active workshop lists | 5× MAX_WORKSHOPS scans → list iteration | Small | 2-5× for WorkGiver scans |
| 12 | Static pathfind buffer | 786KB stack thrash → reuse | Small | Cache improvement |

### Priority Order (effort vs impact)

1. **#4 Static idle buffer** — 10 minutes, eliminates all per-frame allocation
2. **#8 Per-key cache invalidation** — 30 minutes, massive reduction in cache rebuilds
3. **#1+#2 Per-type item index** — 2-4 hours, eliminates the biggest CPU sink
4. **#3 Sparse designation lists** — 2-3 hours, eliminates grid scanning
5. **#5 Reachability-only pathfind** — 1-2 hours, halves pathfinding cost
6. **#7 Per-stockpile item list** — 1 hour, eliminates consolidation scanning
7. **#6 Reverse container index** — 1-2 hours, eliminates container cleanup scanning
