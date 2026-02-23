# Performance & Data-Oriented Design Audit

Codebase analysis through the lens of Casey Muratori-style performance-oriented programming. Findings ranked by impact.

---

## CRITICAL

### 1. Mover Struct is 12.4KB (99% Cache Waste)

**Location:** `src/entities/mover.h:48-89`

The `Mover` struct embeds `Point path[MAX_MOVER_PATH]` (1024 × 12 bytes = 12,288 bytes) inline. With `MAX_MOVERS=10000`, the movers array is **124 MB**.

Every loop that touches movers (spatial grid build, avoidance, movement, rendering) only needs ~30 bytes (position, speed, active flag) but fetches 12.4KB per mover. That's a **387× waste factor** — 194 cache line misses just to read one mover's position.

**Affected hot paths:**
- `BuildMoverSpatialGrid()` — reads `active`, `x`, `y`, `z`
- `ComputeMoverAvoidance()` — reads position + avoidance fields
- `UpdateMovers()` movement phase — reads position, speed, pathIndex
- Rendering — reads position + visual fields
- `RebuildIdleMoverList()` — reads active, jobId, freetimeState

**Fix:** Extract paths to a separate array. Mover hot data drops to ~200 bytes, fitting 3+ movers per cache line. Estimated **10-50× cache improvement** for mover iteration loops.

**Optional further split:**
```
MoverHot (~34 bytes, 1 cache line):
  x, y, z, speed, pathIndex, avoidX, avoidY, active, needsRepath

MoverCold (~120 bytes, rarely accessed per-frame):
  goal, currentJobId, hunger, energy, bodyTemp, starvationTimer, etc.

MoverPath (separate pool, variable length):
  path points, only accessed during movement phase
```

### 2. AStarNode Dense 3D Grid (~117 MB)

**Location:** `src/world/pathfinding.c` (nodeData array)

`nodeData[z][y][x]` is a full 3D grid (~20 bytes per cell). On a 512×512×16 map that's 117 MB. A single A* search typically touches ~100 nodes out of 4,194,304 total. **99.99% of allocated memory is never read.**

**Fix:** Use sparse allocation — hash map or pool allocator for visited nodes. Only allocate node data for cells actually expanded during search.

---

## HIGH

### 3. Designation Cache Full-Grid Rebuild (14× per frame)

**Location:** `src/entities/jobs.c:242-361`

`RebuildAdjacentDesignationCache()` iterates the **entire 3D grid** once per designation type. With 14+ designation types (mine, channel, dig_ramp, chop, gather, clean, etc.), that's 14+ full grid scans in `AssignJobs()` every frame. Most cells have no designation.

`GetDesignation(x,y,z)` is a function call per cell inside these loops, and each found designation also does 4-directional `FindAdjacentWalkable()` checks.

**Fix:** Maintain a per-designation-type dirty list. When a designation is added/removed, mark the cache dirty. Only rebuild dirty caches. Store `adjX, adjY` in the `Designation` struct at creation time.

### 4. Per-Frame malloc/free in Job Assignment

**Location:** `src/entities/jobs.c:2987-3290`

Four job assignment phases each `malloc` a copy of the idle mover list, iterate it, and `free` it — every single frame:
- `AssignJobs_P2_Crafting()` (line ~2987)
- `AssignJobs_P2b_PassiveDelivery()` (line ~3012)
- `AssignJobs_P2c_Ignition()` (line ~3040)
- `AssignJobs_P4_Designations()` (line ~3261)

**Fix:** Pre-allocate a static work buffer at job system init (`static int idleWorkBuffer[MAX_MOVERS]`). Reuse it each phase. Zero per-frame allocation.

### 5. Simulation Loops Iterate Entire Grid

**Location:** `src/simulation/water.c:649-678`, `fire.c:557-581`, `smoke.c:485+`, `temperature.c`

Water, fire, and smoke all triple-nest `for(z) for(y) for(x)` and check stability/level per cell. Most cells are stable/empty — the cache miss just to read "skip me" dominates.

The codebase already tracks active cell counts (e.g., `waterActiveCells`) but doesn't use them for iteration.

**Fix:** Maintain a sparse active cell list per simulation. Add cells when they become active, remove when stable. Iterate only active cells. Estimated **2-10× improvement** on sparse grids.

---

## MEDIUM

### 6. Spatial Grid Full Rebuild Every Frame

**Location:** `src/entities/mover.c:144-185`, called from `main.c:1639`

`BuildMoverSpatialGrid()` does 3 full passes over all movers + memset every tick, even if only 1 mover moved. With the current bloated Mover struct this is especially painful.

**Fix:** Incremental updates — on mover movement, remove from old cell and add to new cell. Full rebuild only if coherence drops below threshold. Note: fixing issue #1 (Mover struct size) reduces the urgency here.

### 7. Function Pointer Dispatch for Job Execution

**Location:** `src/entities/jobs.c:2282-2339`

Job execution uses a `jobDrivers[]` function pointer table — `jobDrivers[job->type](job, mover, dt)` per active job per frame. Indirect calls hurt branch prediction, and function bodies are scattered in memory.

**Fix:** Group jobs by type into batches, process each batch with a direct call: `RunAllHaulJobs()`, `RunAllCraftJobs()`, etc. Better instruction cache, better branch prediction.

### 8. IsCellVisibleFromAbove is O(z) Per Cell in Render Loop

**Location:** `src/render/rendering.c:324-482`

`DrawDeeperLevelCells()` calls `IsCellVisibleFromAbove()` for every potentially visible cell. This function loops from `cellZ` up to `viewZ` checking solidity — an inner loop hidden inside the render loop.

**Fix:** Pre-compute visibility masks when cells change (dig, build, etc.), not per-frame. Store `bool visibleFromAbove[z][y][x]` and invalidate on cell modification.

### 9. Idle Mover List Full Rebuild + Memset

**Location:** `src/entities/jobs.c:2429-2446`

`RebuildIdleMoverList()` does `memset(moverIsInIdleList, 0, ...)` (10KB+ with MAX_MOVERS=10000) every frame, then full scan of all movers with `IsCellWalkableAt()` per mover.

**Fix:** Track idle state incrementally — add to list on job release, remove on job assignment. Only full-rebuild when walkability changes near movers.

### 10. Line-of-Sight Checks Too Frequent

**Location:** `src/entities/mover.c:979-1007`

`HasLineOfSightLenient()` runs 5 Bresenham line traces per mover. Currently staggered to 1/3 movers per frame. Each trace does distance-dependent iterations with multiple `IsCellWalkableAt()` calls.

**Fix:** Increase stagger factor (1/10 or 1/20 per mover). Use cheaper approximate checks (AABB) as early-out before full Bresenham. Only do full LOS when mover is near an obstacle.

---

## LOW

### 11. Avoidance fastInvSqrt Misuse

**Location:** `src/entities/mover.c:237`

Computes `fastInvSqrt(distSq)` then immediately multiplies `distSq * invDist` to recover the distance — defeating the purpose of avoiding sqrt. Either use regular `sqrtf()` or restructure to only use the reciprocal.

### 12. TextFormat() in Rendering Loops

**Location:** `src/render/rendering.c:2499,2508`, `src/render/ui_panels.c:98-122,262-263`

`TextFormat()` does string formatting every frame for blueprint counts, stage indicators, and panel text. Modest cost but adds up.

**Fix:** Cache formatted strings, only update when underlying value changes.

### 13. Haul Assignment Double-Scanning

**Location:** `src/entities/jobs.c:3056-3093, 3424-3441`

Haul assignment does expanding radius searches per idle mover, plus a full `itemHighWaterMark` linear scan fallback. Each mover independently searches items.

**Fix:** Single global pass over items, assign each to nearest idle mover (data-parallel, not per-mover).

---

## What's Already Good

These patterns align well with performance-oriented design:

- **Water/Fire/Smoke bitfield packing** — compact structs (8-16 bit), good for cellular automata
- **Spatial grid prefix-sum layout** — O(1) cell range lookup after build
- **Active job list indirection** — `activeJobList[]` avoids scanning inactive jobs
- **Staggered mover updates** — amortizes LOS/avoidance across frames
- **No over-abstraction** — direct C, no needless wrapper layers
- **Unity build** — single translation unit, compiler sees everything for optimization
- **Idle mover list** — separate tracking avoids scanning all movers for job assignment
- **Packed simulation grids** — WaterCell/FireCell/SmokeCell use minimal memory

---

## Estimated Impact Summary

| # | Issue | Memory/Cache Impact | Effort |
|---|-------|-------------------|--------|
| 1 | Mover path extraction | 124MB → 2MB, 10-50× cache improvement | Medium |
| 2 | Sparse A* nodes | 117MB → <1MB per search | Small-Medium |
| 3 | Designation dirty lists | 14 grid scans → 0 (when clean) | Medium |
| 4 | Static job buffers | 4+ malloc/free per frame → 0 | Small |
| 5 | Sparse sim iteration | Full grid scan → active cells only | Medium |
| 6 | Incremental spatial grid | 3 full passes → delta updates | Medium |
| 7 | Batched job dispatch | Indirect calls → direct batch calls | Medium |
| 8 | Cached visibility masks | O(z) per cell per frame → O(1) lookup | Medium |
| 9 | Incremental idle list | 10KB memset + scan → delta tracking | Small |
| 10 | Reduced LOS frequency | 5 traces/3 frames → 5 traces/20 frames | Small |
