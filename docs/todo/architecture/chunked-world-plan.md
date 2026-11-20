# From fixed arrays to a chunked (and eventually endless) world

> Status: plan / decision doc (2026-11-20). Decided: big bounded maps (target 2048x2048),
> endless world deferred. Cheap win 1 done; step 1 next. Ties together
> `large-worlds.md` (what big maps cost, measured) and `../../vision/infinite-world-exploration.md`
> (endless world research). Numbers are measured unless marked as estimates.

## The decision in one paragraph

Every per-cell layer is a fixed-size static array (`[16][512][512]`), so every map pays
for the biggest allowed map and the cap is fixed when compiling. The infinite-world doc
calls these arrays "the real blocker" for streaming. The first step for **both** goals
(any map size, and later an endless world) is the same: route every per-cell access
through one accessor, so where the data lives can change without touching the call
sites again. If an endless world is a real goal, do that rewrite once, designed for
chunks. If it isn't (the roadmap puts it in Era 3+), take the two cheap memory wins
and leave the rest.

## Decision (2026-11-20)

Target: **bounded maps up to 2048x2048 with lots of structures.** An endless world is maybe
later, not now. So: steps 1-3, with chunk-major storage (layout B) from the start (we are not
going row-major on the way: converting once means the edge-read bugs surface once, and
step 3 comes almost free). Steps 4-9 stay parked.

**2048 can't be a compile-time cap.** A `-DMAX_GRID_WIDTH=2048` build fails to link on arm64:
the static data passes 4 GB and ADRP can't reach it (`ADRP out of range ... '_trackConnections'`).
Step 2 (layers allocated at runtime) is required, not optional.

**Dense is too big for the dev machine (8 GB RAM).** The 1024 build's static data is 1,658 MB:
~87 bytes per cell for grid layers (16M cells), plus mover paths/routes/agents. At 2048
(67M cells) that is ~5.8 GB of layers alone, before the HPA* graph. Sparse layers (step 3)
or narrower types are needed, not just runtime sizing.

Largest layers per cell (from the 1024 symbol table): `designations` 16 B (rare data, ideal
for sparse), `farmGrid` 8 B (sparse), 4 B each for `temperatureGrid`, `wearGrid` (int),
`targetHeight` (int), `growthTimer` (float, trees only), `snowAccumGrid` (float), `lightGrid`,
`grid` (enum stored as int), `bfsQueue`; 2 B each for the sim visited/risen generation arrays,
`waterGrid`, `roomGrid`, `fireGrid`; 1 B for the ~20 others. Narrowing `grid` (to uint8),
`targetHeight`, `wearGrid` and making designations/farm/tree layers sparse removes ~35 of the 87 B.

**Done (2026-11-20, save v94):** `grid` and `targetHeight` are `uint8_t`, `wearGrid` is `uint16_t`
(wear writes capped by `WearCap()`): -8 B per cell. v93 saves migrate on load (checked: old and
new build give identical inspect/audit/mover output after 300 ticks from the same v93 save).
1024 world: peak RSS 1.13 -> 1.05 GB, footprint 1.70 -> 1.60 GB, tick time unchanged (6.8 ms).

**1024 baseline after cheap win 1** (`path1024 --headless --scenario world --set scenarioSize 1024
--ticks 1200 --profile`): 6.7 ms/tick average (Move 2.9, Water 2.1, Trees 0.9, Jobs 0.4);
peak RSS 1.13 GB, peak footprint 1.70 GB; HPA* full graph build 3.0 s (298K entrances,
6.6M edges), world gen + graph 4.0 s. Expect ~4x at 2048 for anything still sweeping the
whole map, and for the graph build (~12 s).

## Where we are (2026-11-20)

**Memory, measured** (peak resident memory, `world` scenario, 60 ticks):

| build | 128 map | 256 map | 512 map | 1024 map |
|---|---|---|---|---|
| normal (cap 512) | 260 MB | 295 MB | 355 MB | n/a |
| `make path1024` (cap 1024) | **852 MB** | — | 927 MB | 919 MB (1.13 GB after 1200 ticks) |

A small map in the 1024-cap build costs ~850 MB: whole arrays are cleared at startup,
and rows are spaced by the cap width.

**The arrays:** 35 per-cell layers across the game, ~2,700 direct `name[z][y][x]`
accesses in 96 files; 1,750 of them are `grid[...]` (1,090 in tests). Most other layers
are already read through helpers (`GetWaterLevel`, `GetWallMaterial`, ...).

**Already done toward a chunked world** (all 2026-11-20, see `large-worlds.md`):
- *Coarse mover paths*: a mover keeps its route as waypoints and refines the next legs
  as it walks. This is the infinite-world doc's "path caching: abstract path stored,
  local path computed on demand". A route can point into an area that isn't loaded.
- *Local graph updates*: per-entrance edge lists, entrance ids that stay put, a chunk
  entrance index kept up to date. Changing one chunk's part of the HPA* graph is a
  local operation, which is what loading/unloading a chunk does to the graph.
- *Seeds*: `--seed` works headless; generation is reproducible per world (not per chunk).
- *Activity-driven sims*: trees and water freezing sweep in bands, snow skips idle
  passes. A small step toward "simulate only what's active".
- *Per-tick cost* no longer grows much with area: 1024 runs at ~6 ms per tick.

**Outdated in the infinite-world doc:** its memory table (the 80 MB `nodeData` and the
64 MB designations figure no longer match; use the table above), and "no --seed plumbing".

## Step 1: one accessor for every per-cell layer (the shared first step)

C can't keep `grid[z][y][x]` for an array whose size is chosen at runtime, so every
access changes once. Make it an accessor that hides the layout:

```c
// grid.h (sketch)
static inline int CellIndex(int x, int y, int z);      // layout decided here only
#define GRID(z, y, x)        grid[CellIndex((x), (y), (z))]
#define WATER(z, y, x)       waterGrid[CellIndex((x), (y), (z))]
...
```

- **Layout A, row-major** `((z * H) + y) * W + x`: simplest, one array per layer sized
  to the map. Enough for "any map size" (step 2).
- **Layout B, chunk-major** (chunk id, then the cell inside the chunk): neighbouring cells
  of a chunk are contiguous, and a chunk's slice of every layer can later live in its
  own block. This is the layout streaming needs. Same accessor, different `CellIndex`.
- Do the rewrite with A, keep `CellIndex` the only place that knows; switching to B is
  then a contained change.

**How:** ast-grep rewrites (`grid[$Z][$Y][$X]` -> `GRID($Z, $Y, $X)`), layer by layer,
tests after each. Row pointers (`&grid[z][y][0]` in save/load and inspect) become loops
or `CellIndex(0, y, z)`; the save format stays the same.

**Risk: reads one past the edge.** Today `grid[z][y][x + 1]` at the last column reads
padding (harmless zero). With exact-size arrays it reads the next row (layout A) or
another chunk (B): silent logic changes. Catch them with an assert in `CellIndex` in
debug builds plus `make asan` over the test suites and `make scenarios-check`.

**Cost:** one multiply-add per access instead of compile-time strides (512 is a power of
two today). Estimate: a few percent on cell-heavy loops at most; measure with
`--profile` before/after.

**Size:** the largest single change in the plan: mechanical but touches almost every
system, so do it in a session where nobody else is editing (not in parallel with
another agent). Estimate: a long session for the rewrite, plus debugging the edge reads.

## Step 2: layers sized to the map

With the accessor in place, allocate layers at world creation/load (`InitGridWith...`,
`LoadWorld`) instead of statically. Removes the compile-time cap (`make path1024` no
longer needed; the limit becomes RAM). A 128 map's layers drop to a few tens of MB;
a 1024 map still costs what it costs (~75 bytes per cell dense, ~1.2 GB).

## Step 3: sparse layers

Water, fire, smoke, steam, snow, farming, designations are empty almost everywhere.
With chunk-major storage (layout B), give a chunk memory for such a layer only when
something lives there. Estimate (`large-worlds.md`): ~15 bytes per cell dense, so
2048x2048x16 at ~1-1.5 GB instead of 5 GB. Pairs with active-chunk simulation (sims
iterate chunks that have something, not the whole map).

## Allocation model (for steps 2-3)

Group memory by lifetime instead of one allocation per layer. Three small pieces, no
general-purpose allocator (~150 lines total):

| lifetime | what | allocator |
|---|---|---|
| program | atlas, fonts, tables | static, as now |
| world (new game / load / scenario reset) | per-cell layers, HPA* graph, spatial grids, later entity arrays | **world arena**: allocate at world creation, drop all at once on reset |
| chunk (comes and goes on its own) | sparse layer blocks: water, fire, designations, farm, ... | **fixed-size block pool** with a free list (`NULL` block = empty) |
| tick / temporary | `bfsQueue`, `visited` planes, sim generation arrays, search scratch | **scratch arena**, reset per tick or per call |

- Reserve a large virtual range (e.g. 8 GB, `mmap`) and let the OS commit pages on first
  touch: untouched memory costs nothing (the same effect cheap win 1 relies on), and
  pointers never move.
- Arenas don't make 2048 fit by themselves: the density work (narrower types, sparse layers)
  decides that.
- Only useful from step 2 on: before the accessor, every layer is still a fixed-size array.

## Steps toward an endless world (after 1-3)

Each is its own project; order follows the infinite-world doc, adjusted for what exists.

| step | what | depends on | size (estimate) |
|---|---|---|---|
| 4 | **World coordinates beyond the array**: chunk lookup by world chunk coordinates (hash map), cells addressed by world position | layout B | medium |
| 5 | **Per-chunk generation**: `chunk_seed = hash(world_seed, cx, cy)`; today's generators work on the whole map (rivers, caves, connectivity fix all look across it), so this is a new generator family, likely driven by an overview map ("overmap") | 4 | large |
| 6 | **Load/unload ring around the colony**: unloaded chunks saved and dropped; the HPA* graph removes/adds a chunk's entrances (now a local operation) and treats unloaded neighbours as borders | 4, 5 | large |
| 7 | **Entities when their chunk unloads**: movers/items/jobs paused or abstracted ("the hardest problem" in the vision doc). Coarse routes already let a mover hold a route through unloaded chunks | 6 | large |
| 8 | **Saves that store only changes** per chunk against the regenerated original | 5, 6 | medium |
| 9 | **Fast-forward on reload**: water settles to its level, plants grow by elapsed time, fires burn out | 6 | medium |

## Cheap wins that don't need any of this

- ~~**Clear only what the map uses** at startup~~ **Done** (2026-11-20, `ClearGridLayer` in
  `grid.c`): peak memory for a 128 map 282 -> 109 MB (cap 512), 825 -> 207 MB (cap 1024).
- **Shrink mover path buffers** now that coarse paths exist: `MAX_MOVER_PATH` 1024 -> 256
  saves ~90 MB for every map (needs a save version bump and migration).

## Decision points

1. **Is an endless world a goal before Era 3?** If not: do the cheap wins, stop here.
2. **If yes (or if 1024+ maps should be normal):** step 1 with the accessor, then step 2.
   Choose layout B early if streaming is the target, so step 3 comes almost free.
3. **Before step 5:** decide the overview-map design (see the infinite-world doc, section 9);
   it decides how per-chunk generation works.

## References

- `large-worlds.md`: measured costs per map size and terrain, what was fixed.
- `../../vision/infinite-world-exploration.md`: streaming, delta saves, entity abstraction research.
- `../pathfinding/per-chunk-edge-storage.md`: the local graph updates.
- `unified-roadmap.md`: puts the infinite world in Era 3+.
