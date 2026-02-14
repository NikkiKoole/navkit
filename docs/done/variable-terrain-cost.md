# Variable Terrain Cost Pathfinding

**Status: TODO**

## Goal

Make pathfinding cost-aware so movers prefer cheap terrain (roads, floors) over expensive terrain (mud, shallow water, bushes). Currently the pathfinding layer is completely uniform-cost — movers pick the geometrically shortest path and only slow down at runtime when they walk through mud/water. With variable costs, they'll route *around* expensive terrain when a cheaper path exists.

## Current Architecture (Two-Layer Split)

### Layer 1: Pathfinding (uniform cost)
- Default algorithm: **HPA*** (`moverPathAlgorithm = PATH_ALGO_HPA`)
- All cost calculations use hardcoded `10` (cardinal) / `14` (diagonal)
- Cost is computed in **6-8 places** in `pathfinding.c`:
  - `AStarChunk()` ~line 704 — intra-chunk A* for HPA* graph building
  - `AStarChunkMultiTarget()` ~line 797 — Dijkstra for HPA* multi-target
  - `RunAStar()` ~line 1688 — full-grid A* (XY, ladders, ramps)
  - `ReconstructLocalPathWithBounds()` ~line 1906 — HPA* path refinement
- HPA* graph edges inherit uniform costs from `AStarChunk()`
- JPS/JPS+ exist but are **not used** (default is HPA*)

### Layer 2: Movement (runtime speed multipliers)
Applied in `UpdateMovers()` (`mover.c` ~line 1089):

| Terrain | Speed Mult | Pathfinding Cost (proposed) |
|---------|-----------|---------------------------|
| Constructed floor | 1.25x (bonus) | 8 cardinal / 11 diagonal |
| Grass/dirt (default) | 1.0x | 10 / 14 (baseline) |
| Shallow water (level 1-2) | 0.85x | 12 / 17 |
| Medium water (level 3) | 0.6x | 17 / 24 |
| Deep water (level 4+) | — | Unwalkable (blocked by `IsCellWalkableAt`) |
| Mud (wetness >= 2 on soil) | 0.6x | 17 / 24 |
| Snow level 1 | 0.85x | 12 / 17 |
| Snow level 2 | 0.75x | 13 / 18 |
| Snow level 3 | 0.6x | 17 / 24 |
| Bush (new, walkable vegetation) | ~0.7x | 14 / 20 |

Note: Snow speed (`GetSnowSpeedMultiplier()`) is defined in `weather.c` but NOT currently wired into `UpdateMovers()`. That's a pre-existing bug to fix as part of this work.

## Design

### Phase 1: `GetCellMoveCost()` Function

Add a single cost function that both pathfinding and the movement layer can reference. This is the **source of truth** for terrain expense.

```c
// cell_defs.h or pathfinding.h
// Returns cost multiplier in fixed-point (10 = baseline = 1.0x)
// Higher values = more expensive to traverse
int GetCellMoveCost(int x, int y, int z);
```

The function checks (in priority order):
1. **Water level** — shallow water costs more, deep water blocks (already handled by walkability)
2. **Mud** — `IsMuddy()` at ground level
3. **Snow** — `GetSnowLevel()` at position
4. **Bush/vegetation** — new walkable-but-expensive cell type or flag
5. **Constructed floor** — cheaper than baseline (roads/floors are fast)
6. **Default** — baseline cost 10

This replaces the scattered speed multiplier logic in `UpdateMovers()`. The movement layer can derive its speed mult from `GetCellMoveCost()` too (or keep them separate — see open questions).

### Phase 2: Wire Into A* / HPA*

Replace every `int moveCost = (diagonal) ? 14 : 10` with:

```c
int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
int terrainCost = GetCellMoveCost(nx, ny, nz);
int moveCost = (baseCost * terrainCost) / 10;  // Fixed-point multiply
```

Specific locations in `pathfinding.c`:
1. `AStarChunk()` ~line 704
2. `AStarChunkMultiTarget()` ~line 797
3. `RunAStar()` ~line 1688 (XY movement)
4. `RunAStar()` ~lines 1706, 1722 (ladder up/down)
5. `RunAStar()` ~lines 1745, 1776 (ramp up/down)
6. `ReconstructLocalPathWithBounds()` ~line 1906

For ladders and ramps: use the cost of the destination cell.

**HPA* graph building**: No extra work needed — `BuildGraph()` calls `AStarChunk()` / `AStarChunkMultiTarget()` between entrance pairs. Once those use variable costs, the abstract graph edges automatically reflect terrain expense.

**Heuristic admissibility**: The A* heuristic (Manhattan/Chebyshev × 10) remains admissible as long as the minimum cell cost is >= 10. Constructed floors (cost 8) violate this, so the heuristic must scale by the minimum possible cost:

```c
int heuristic = chebyshev_distance * MIN_CELL_COST;  // MIN_CELL_COST = 8
```

This slightly loosens the heuristic (more nodes explored) but guarantees optimality. In practice the impact is small — floor cells are common so paths through them are found quickly.

### Phase 3: HPA* Dirty Marking for Cost Changes

Currently `MarkChunkDirty()` is only called for structural terrain changes (walls placed/removed). Cost changes from dynamic systems (rain → mud, snow accumulation, water level changes) must also trigger dirty marking.

Add `MarkChunkCostDirty(int x, int y, int z)` — same as `MarkChunkDirty()` but skips JPS rebuild since we don't use JPS. Call it from:
- `UpdateWetness()` in water.c — when wetness crosses the mud threshold (2)
- `UpdateSnow()` in weather.c — when snow level changes
- Water level changes that affect wadeable range (levels 1-3)

**Throttling**: These systems tick frequently. To avoid constant HPA* rebuilds:
- Only mark dirty when the *cost bucket* changes (e.g., wetness going from 1→2 triggers mud, but 2→3 doesn't change cost)
- HPA* rebuild is already batched per-tick (`UpdateDirtyChunks()` runs once), so many dirty marks collapse into one rebuild
- Consider a cost-change cooldown per chunk (e.g., don't re-mark within 5 seconds)

### Phase 4: String Pulling with Variable Costs

**The Problem**: `StringPullPath()` uses `HasClearCorridor()` which only checks walkability (line-of-sight). It will happily pull a path straight through a mud pit if there's geometric LOS, undoing the pathfinder's careful routing around it.

**Solution: Weighted Corridor Check**

When string-pulling, verify that the shortcut is actually cheaper than the original path segment. The check becomes:

```c
// In StringPullPath(), replace:
//   if (HasClearCorridor(from, to, z))
// with:
//   if (HasClearCorridor(from, to, z) && 
//       CorridorCostNotWorse(pathArr, current, i, from, to, z))
```

`CorridorCostNotWorse()` walks the straight line from→to, summing `GetCellMoveCost()` for each cell traversed. It compares this against the sum of costs along the original path segment (current→i). Only allow the shortcut if the straight-line cost is <= the path segment cost (with a small tolerance, e.g. 10%, to avoid overly rigid paths).

This is cheap: string pulling already does Bresenham line traces for LOS. We just accumulate cost along the same trace.

**Alternative considered — cost map blurring**: Pre-blur the cost map with a Gaussian kernel so sharp boundaries (road=8, mud=17) become gradients. A* naturally curves away from high-cost centers, giving string pulling a pre-rounded corridor. Rejected because:
- Adds a separate cost grid that must be maintained
- Blurs the costs for pathfinding too (not just string pulling)
- The weighted corridor check is simpler and more precise

### Phase 5: Testing with Sandbox Drawing — DONE

**Existing tools that already affect cost:**
- Soil drawing (`ACTION_DRAW_SOIL_*`) — dirt/clay create surfaces that can become muddy when wet
- Water placement — shallow water has cost
- Weather panel — can enable rain/snow to create mud naturally

**Bush drawing tool** (S → B) — IMPLEMENTED:
- `CELL_BUSH` cell type added (index 15), `SPRITE_bush`, flags `0` (walkable via DF-model, like saplings)
- `ACTION_SANDBOX_BUSH` in sandbox mode, key `B`. L-drag places, R-drag removes
- Cost of ~14 when variable terrain cost is wired in (future phase)

**Wet mud drawing tool** — REMOVED:
Tried adding `ACTION_DRAW_SOIL_WET_MUD` but the z-level semantics were confusing (mud needs to be on the wall at z-1, not at the viewed z-level). Removed in favor of using existing tools: draw dirt soil + enable rain from weather panel, or use water placement for shallow water costs.

### Phase 6: Movement Layer Alignment

Update `UpdateMovers()` to derive speed from `GetCellMoveCost()` instead of the scattered if/else chain:

```c
int cost = GetCellMoveCost(currentX, currentY, currentZ);
float terrainSpeedMult = 10.0f / (float)cost;  // cost 10 → 1.0x, cost 17 → 0.59x
```

This ensures pathfinding cost and movement speed are always in sync. If a cell costs 17 to pathfind through, the mover also moves at ~0.59x speed there.

Wire in `GetSnowSpeedMultiplier()` at the same time (currently missing).

## Performance Considerations

### HPA* remains fast
- `GetCellMoveCost()` is a handful of comparisons — same order as `IsCellWalkableAt()`
- The grids it reads (`waterGrid`, `grid`, `cellFlags`, `snowGrid`) are already in cache from the walkability check that runs right before it in the A* inner loop — same (x,y,z) coordinates
- HPA* graph building runs `AStarChunk()` within 16x16 chunks. Adding a cost lookup per expansion is negligible
- Abstract search (entrance graph) is unchanged — edge weights just have better values
- Path refinement (`ReconstructLocalPathWithBounds()`) is also bounded to small regions

### Cache note
Compute on the fly — no separate cost grid. The data `GetCellMoveCost()` needs is already cached from the adjacent `IsCellWalkableAt()` call. If profiling ever shows this as a bottleneck, a precomputed `uint8_t costGrid[z][y][x]` (one byte per cell, rebuilt when chunks go dirty) is a straightforward upgrade.

### Incremental rebuild cost — BE PRAGMATIC
- Mud/snow/water changes are spatially coherent (rain affects large areas)
- Worst case: every chunk marked dirty → full rebuild. This already works (initial build)
- Throttling by cost-bucket changes prevents unnecessary rebuilds

**Key insight: uniform cost changes don't create useful gradients.** Rain making everything muddy doesn't change which path is *relatively* better — rebuilding the HPA* graph is wasted work. Variable cost only matters when cheap terrain exists next to expensive terrain (a road through mud, a dry patch in snow).

**Pragmatic approach (implement in order of need):**
1. **Start with no cost-dirty marking at all.** Movers still slow down via the movement layer. They just won't *route around* mud until the next structural rebuild. For widespread weather this is fine — there's no good detour anyway.
2. **If local cost changes matter** (puddle, snow patch near a road): add periodic rolling refresh — rebuild N chunks per tick round-robin instead of event-driven spikes.
3. **If event-driven is ever needed**: skip dirty-marking when the change is global (>50% of surface chunks affected). Only mark when cost changes are spatially local, creating actual gradients worth rerouting for.

Don't over-engineer Phase 3 upfront. Start with option 1 and see if players even notice.

### String pulling overhead
- `CorridorCostNotWorse()` adds one cost accumulation per shortcut attempt
- String pulling typically tests O(pathLen) shortcuts, each tracing O(distance) cells
- Already dominated by `HasLineOfSight()` Bresenham traces — cost accumulation piggybacks on those

### What we're NOT doing
- No JPS/JPS+ changes (not used, and they can't support variable costs)
- No flow fields (future feature, separate from this)
- No per-agent costs (territory/ownership — separate future feature)
- No cost map blurring (overkill for our grid resolution)

## Profiling

Use the existing profiler (`shared/profiler.h`) to measure before/after. Already in use for `HPA`, `Repath`, `Move`, `LOS`, `Avoid` sections.

### Baseline measurements (capture BEFORE any changes)
Run headless benchmark (`-headless -ticks 1000`) with current uniform-cost code:
- `HPA` section — graph rebuild time per tick
- `Repath` section — per-tick repath time (includes `FindPath` calls)
- `Move` section — mover movement update

### New profiler sections to add
```c
// In pathfinding.c, around AStarChunk / AStarChunkMultiTarget:
PROFILE_ACCUM_BEGIN(PathCost);   // Wraps GetCellMoveCost calls
PROFILE_ACCUM_END(PathCost);     // (accum because called many times per frame)

// In mover.c, around StringPullPath:
PROFILE_ACCUM_BEGIN(StringPull);
PROFILE_ACCUM_END(StringPull);

// In pathfinding.c, UpdateDirtyChunks:
PROFILE_BEGIN(HPARebuild);       // Already have HPA section, but this is more specific
PROFILE_END(HPARebuild);
```

`PROFILE_ACCUM_BEGIN/END` is the right fit for `PathCost` and `StringPull` since they're called many times per frame (once per mover repath, once per A* node expansion). The accum variant sums across all calls in a frame.

### What to watch
- **PathCost accum time** — should be tiny (<0.1ms). If it's not, that's the signal to add a precomputed `costGrid`
- **HPA rebuild** — should not spike when mud/snow changes. Throttling working correctly?
- **StringPull accum time** — the weighted corridor check adds work. Compare before/after
- **Repath time** — overall per-tick repath. Variable costs may explore more nodes (weaker heuristic). Monitor for regression

### Headless benchmark comparison
Run before and after with same map + agent count:
```
./bin/navkit -headless -ticks 1000 -movers 500 -algo hpa
```
Compare the profiler output section averages printed at the end.

## Implementation Order

1. ~~**Sandbox: bush cell type**~~ — DONE (`CELL_BUSH` + `ACTION_SANDBOX_BUSH`)
2. **Baseline profiling** — capture current numbers before any changes
3. **`GetCellMoveCost()`** — the function itself, with tests
4. **Wire into A* cost** — the 6-8 locations in pathfinding.c + profiler sections
5. **Heuristic adjustment** — scale by MIN_CELL_COST
6. **HPA* dirty marking** — for mud/snow/water cost changes
7. **String pulling fix** — weighted corridor check + profiler section
8. **Profile checkpoint** — compare against baseline, investigate regressions
9. **Movement layer alignment** — derive speed from cost function
10. **Wire snow speed** into `UpdateMovers()` (pre-existing gap)
11. **Integration tests** — movers avoid mud, prefer roads, etc.
12. **Final profiling** — full comparison against baseline

## Gotchas / Gaps Found During Review

### 1. Runtime LOS re-check ignores cost
During movement, movers do a runtime `HasLineOfSightLenient()` check (mover.c ~line 821) and skip waypoints if they have LOS to a further point. This is a mini string-pull at runtime — same problem as Phase 4. If a mover gains LOS across a mud pit mid-walk, it'll cut straight through.

**Fix**: The runtime LOS skip should also do the `CorridorCostNotWorse()` check, same as string pulling. Otherwise the pathfinder routes around mud, string pulling respects it, but the runtime LOS shortcut undoes it.

### 2. Cost stacking: mud + water + snow
A cell could theoretically have mud AND shallow water AND snow. `GetCellMoveCost()` needs a clear rule: should costs stack multiplicatively, or take the worst? 

**Recommendation**: Take the max cost, don't stack. A muddy cell with shallow water is about as bad as the worst of the two, not twice as bad. Keeps the cost range bounded and predictable.

### 3. `CELL_BUSH` save migration — NOT NEEDED (verified)
`CELL_BUSH` added at enum index 15. CellType is stored directly in the grid (not in indexed save arrays). Older saves won't have any cells with value 15, so they load fine. No save version bump was needed.

### 4. HPA* entrance placement on cost boundaries
HPA* entrances are placed at chunk borders where walkable cells meet. The entrance detection doesn't consider cost — a chunk border running through mud creates entrances there. This is correct (the entrance exists, its edges just have high cost), but worth verifying: a border entirely in mud should still get entrances, just with expensive edges.

No code change needed — `BuildEntrances()` checks walkability only, and `AStarChunk()` computes the actual edge costs. Just a note to test this case.

### 5. Floor speed bonus interacts with cost derivation
Currently floor gives 1.25x speed bonus (mover.c). With cost derivation (`10.0f / cost`), floor cost of 8 gives `10/8 = 1.25x`. This happens to match perfectly. But if floor cost is ever tuned independently, the movement speed changes too. This is intentional (one source of truth) but worth being aware of.

## Decisions

1. **Movement speed derives from pathfinding cost.** One source of truth. `GetCellMoveCost()` drives both path choice and mover speed (`10.0f / cost`).

2. **Bush is a cell type** (`CELL_BUSH`) — DONE. Enum, CellDef, sprite, sandbox draw action all implemented. Just needs cost wiring in `GetCellMoveCost()`.

3. **Cost change throttling: cost-bucket-only marking.** Only dirty-mark a chunk when the cost category actually changes (e.g., wetness crossing the mud threshold). Simpler than per-chunk cooldowns.

4. **No carrying cost in pathfinding.** Carrying weight is per-mover state, not per-cell. Out of scope.
