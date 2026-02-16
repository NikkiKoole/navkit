# SIMD & Low-Level Optimization Candidates

Date: 2026-02-16
Status: Reference — no active bottleneck, general analysis for when needed

---

## Context

NavKit is ~27k lines of C11, grid sizes up to 512x512x16, up to 10k movers. Most hot paths already have caches and early-exit optimizations (stability flags, spatial grids, high-water marks). This doc catalogs where SIMD or structural optimizations would help if scale increases.

---

## Candidate Assessment

### Tier 1: Best ROI

#### Spatial Grid Build — `BuildMoverSpatialGrid()` / `BuildItemSpatialGrid()`
- **Location**: `mover.c:141-182`, `items.c:598-650`
- **What it does**: Converts every mover/item position to a grid cell index. Two passes: count per cell, then scatter indices.
- **Why it's good**: Pure arithmetic (`float × invCellSize → int → clamp → index`), no dependencies between iterations, called every frame.
- **Current waste**: Coordinate→cell conversion computed twice (count pass + scatter pass).
- **SIMD approach**: Load 4-8 positions, multiply by invCellSize, cast to int, clamp — all in parallel.
- **Cheaper alternative**: Cache cell indices from pass 1, reuse in pass 2. Halves the work without SIMD.
- **Estimated gain**: 2-4× for this function.

#### Mover Movement — `UpdateMovers()`
- **Location**: `mover.c:863+`
- **What it does**: Per-mover: compute direction to waypoint, normalize, apply speed × dt, add avoidance.
- **Core math**: `dx = target - pos`, `dist = sqrt(dx² + dy²)`, `move = (dx/dist) * speed * dt`
- **Why it's good**: Textbook vector math, same operation for every mover.
- **Why it's hard**: Mover struct is ~120 bytes, but movement only touches ~24 bytes (x, y, speed, avoidX, avoidY, pathIndex). Every mover fetch pulls a full cache line of unused data.
- **SIMD approach**: SoA layout — separate `float posX[]`, `float posY[]`, `float speed[]` arrays. Process 4 movers per SSE iteration, 8 per AVX.
- **Cheaper alternative**: Hot/cold split — move position and movement fields to a compact struct, keep diagnostics/job data separate. Same AoS pattern but cache-friendly.
- **Estimated gain**: 2-4× with SoA, 1.5-2× with hot/cold split.
- **Cost**: SoA is a major refactor (every mover access site changes). Hot/cold split is moderate.

---

### Tier 2: Moderate ROI

#### Cellular Automata — Water, Smoke, Steam
- **Location**: `water.c:603-681`, `smoke.c:477-540`, `steam.c:390-451`
- **What they do**: 3D grid scan, per-cell level comparisons and transfers to neighbors.
- **Why it's decent**: Regular access pattern (sequential z/y/x), arithmetic (level diffs, flow amounts).
- **Why it's limited**: Stability-skip optimization means most cells are skipped. Loading 8 cells via SIMD to skip 6 wastes bandwidth. Also: neighbor reads are scattered (±1 in x/y/z).
- **SIMD approach**: Process innermost x-loop in chunks of 8/16. Use SIMD to check stability flags in bulk (`_mm_cmpeq_epi8` for 16 cells at once), then only process unstable ones.
- **Better approach**: Maintain an active-cell list (like `dirtActiveCells` pattern) instead of scanning the full grid. Only iterate cells that need work. This beats SIMD for sparse grids.
- **Estimated gain**: 1.5-2× with SIMD on dense grids, negligible on sparse grids.

#### Temperature — `UpdateTemperature()`
- **Location**: `temperature.c:350-531`
- **What it does**: Heat transfer — each cell averages with 8 neighbors, weighted by transfer rate.
- **Why it's decent**: Regular stencil computation, floating-point arithmetic.
- **Why it's limited**: 8-neighbor stencil means 8 scattered reads per cell. Diagonal neighbors are worst (different cache line).
- **SIMD approach**: Process 4 cells in a row simultaneously. For each batch, load the 4 center values + their shared neighbors. Reuse neighbors between adjacent cells.
- **Better approach**: Red-black or checkerboard update pattern — process even cells, then odd cells. Eliminates write-read hazards and improves cache reuse.
- **Estimated gain**: 2-2.5× with careful implementation.

#### Snow Accumulation — `UpdateSnow()`
- **Location**: `weather.c:449-508`
- **What it does**: Column scan (x,y pairs), find topmost solid cell, accumulate/melt snow.
- **Why it's moderate**: The (x,y) outer loop is parallelizable — each column is independent.
- **Why it's limited**: Inner vertical scan (`for z = top..0, break on solid`) is inherently serial per column. `IsExposedToSky()` does another vertical scan.
- **SIMD approach**: Process 4-8 columns simultaneously. Vectorize the accumulation math after finding topmost solid.
- **Cheaper alternative**: Cache topmost-solid-z per column (update only when terrain changes). Eliminates the vertical scan entirely.
- **Estimated gain**: 1.5-2× with SIMD, 3-5× with cached column heights.

---

### Tier 3: Low ROI (Don't Bother)

#### Lighting BFS — `SpreadSkyLight()`, `PropagateBlockLight()`
- **Location**: `lighting.c:103-155`, `lighting.c:177-252`
- **Why it's bad**: BFS is queue-based. Each cell's processing depends on the previous cell's result. Can't parallelize the propagation itself.
- **The only opportunity**: Seeding phase (finding border cells to start BFS) could scan the grid with SIMD, but it's a tiny fraction of total work.
- **Better approach**: Multi-threaded BFS (process different light sources on different threads) or dirty-region tracking (only re-propagate changed areas).

#### Pathfinding — A*/HPA* Neighbor Expansion
- **Location**: `pathfinding.c`
- **Why it's bad**: Heap operations are inherently sequential. Neighbor expansion is 4-6 iterations with heavy branching (walkability checks, bounds, cost comparisons). Too few iterations per expansion to benefit from SIMD.
- **Better approach**: Already using HPA* (hierarchical), JPS/JPS+. Algorithmic improvements beat SIMD here.

---

## Non-SIMD Optimizations (Often Higher ROI)

These structural changes often outperform SIMD at NavKit's current scale:

### Hot/Cold Struct Splitting
Move rarely-accessed fields out of frequently-iterated structs.

```
Current Mover (120+ bytes, all in one struct):
  HOT:  x, y, z, speed, pathIndex, avoidX, avoidY, active     (~32 bytes)
  COLD: goal, path[1024], capabilities, diagnostics, hunger... (~rest)

Split into:
  MoverPos   moverPos[MAX_MOVERS];    // 32 bytes × 10k = 320KB (fits L2)
  MoverState moverState[MAX_MOVERS];  // everything else
```

Movement loop only touches `moverPos[]` — cache hit rate goes from ~25% to ~100%.

Applies to: Mover, Item, Workshop (anything iterated frequently but only partially read).

### Active-Cell Lists (vs Full Grid Scan)
Instead of scanning 512×512×16 cells and skipping 99% of them:

```c
// Current: O(grid_size), mostly skipping
for z/y/x: if (cell.stable) continue;

// Better: O(active_count), no skipping
for (int i = 0; i < activeCellCount; i++) {
    ActiveCell* c = &activeCells[i];
    // process — every iteration does real work
}
```

Already used for: `dirtActiveCells` (floordirt.c).
Should use for: water, smoke, steam, temperature (when grid is sparse).

### Column Height Cache
Several systems scan vertically to find "topmost solid cell" or check `IsExposedToSky()`:

```c
// Current: O(gridDepth) per column, per tick
for (int z = gridDepth-1; z >= 0; z--) {
    if (IsSolid(grid[z][y][x])) { topZ = z; break; }
}

// Better: O(1) per column
uint8_t columnTop[MAX_HEIGHT][MAX_WIDTH];  // updated on terrain change only
```

Affected systems: snow accumulation, sky light, rain exposure, plant growth.
Update cost: one write per `SetCell()` call (terrain changes are rare vs per-tick reads).

### Batch Stability Checks
For cellular automata, scan stability flags in bulk before processing:

```c
// Check 16 cells at once using byte comparison
// (no SIMD intrinsics needed — compiler auto-vectorizes this pattern with -O2)
for (int x = 0; x < gridWidth; x += 16) {
    bool anyUnstable = false;
    for (int i = 0; i < 16; i++) {
        if (!cells[z][y][x+i].stable) { anyUnstable = true; break; }
    }
    if (!anyUnstable) continue;  // Skip entire 16-cell block
    // Process individually
}
```

---

## Data Layout Reference

Current layouts and SIMD-friendliness:

| System | Struct | Size | Layout | SIMD-Friendly? |
|--------|--------|------|--------|----------------|
| Movers | `Mover` | ~120B | AoS | Poor (cache waste) |
| Items | `Item` | ~40B | AoS | Moderate |
| Water | `WaterCell` | ~8B | AoS 3D grid | Good (compact) |
| Temperature | `TempCell` | ~6B | AoS 3D grid | Good (compact) |
| Smoke/Steam | similar | ~6B | AoS 3D grid | Good (compact) |
| Light | `LightCell` | ~4B | AoS 3D grid | Good (compact) |
| Snow | `uint8_t` | 1B | flat 3D grid | Excellent |
| Mud | `uint8_t` | 1B | flat 3D grid | Excellent |

Grid cells are already compact. The biggest AoS penalty is on Mover and Item — the entity pools, not the grids.

---

## Platform Notes (Apple Silicon / x86)

NavKit runs on macOS (Darwin 23.2.0). Relevant SIMD options:

| Platform | Instruction Set | Width | Header |
|----------|----------------|-------|--------|
| Apple Silicon (M1+) | NEON | 128-bit (4×float) | `<arm_neon.h>` |
| x86_64 | SSE4.2 | 128-bit (4×float) | `<smmintrin.h>` |
| x86_64 | AVX2 | 256-bit (8×float) | `<immintrin.h>` |

For portable code, prefer compiler auto-vectorization (`-O2 -march=native`) over intrinsics. Write simple loops with no aliasing and let the compiler handle it. Only drop to intrinsics if profiling shows the compiler missed an opportunity.

```c
// Compiler-friendly loop (likely auto-vectorized):
for (int i = 0; i < count; i++) {
    outX[i] = inX[i] * scale;
    outY[i] = inY[i] * scale;
}

// NOT compiler-friendly (pointer aliasing, AoS access):
for (int i = 0; i < count; i++) {
    movers[i].x *= scale;  // compiler can't prove movers[i] doesn't alias movers[j]
}
```

Adding `__restrict` qualifiers and `-ffast-math` can unlock auto-vectorization without writing intrinsics.

---

## Recommendation Summary

| Priority | What | Approach | Effort | Gain |
|----------|------|----------|--------|------|
| 1 | Spatial grid double-compute | Cache cell indices between passes | Trivial | 2× for function |
| 2 | Column height cache | `columnTop[y][x]` updated on terrain change | Small | 3-5× for snow/rain/sky |
| 3 | Mover hot/cold split | Separate position struct from state | Moderate | 1.5-2× for movement |
| 4 | Active-cell lists for CA | Track unstable cells explicitly | Moderate | 2-10× for sparse grids |
| 5 | SoA mover positions | Separate x[]/y[]/z[] arrays | Large | 2-4× for movement+spatial |
| 6 | SIMD intrinsics | Manual vectorization of hot loops | Large | 2-4× per loop |

Items 1-4 are structural improvements that work at any scale. Items 5-6 are for when scale demands it (10k+ movers, 1024² grids).
