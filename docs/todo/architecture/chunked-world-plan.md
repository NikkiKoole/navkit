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

**What is actually resident (2026-11-20, measured with `mincore` per static array, 128 map in
the 1024 build, 600 ticks):** the big temporaries (`bfsQueue`, `roomBfsQueue`, `roomCellBuffer`,
`visited` planes) are under 0.5 MB each: untouched static pages cost no RAM, so making them
map-sized saves nothing now. They only matter for the static-data size (link limit), so they
move to runtime allocation together with the layers (step 2, scratch arena) instead of a
separate step. `waterGrid` was fully resident (32 MB): `ClearWater` filled the whole array with
its "stable" pattern; now `FILL_GRID_LAYER` fills only the used region (peak 207 -> 171 MB).
What remains is row spacing: every layer pays ~8x its dense size on a 128 map because each row
is 1024 cells apart (e.g. `designations` 32 MB resident, ~4 MB of real data). Only runtime
sizing (step 2) fixes that.

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

## Step 1: one accessor for every per-cell layer (design, 2026-11-20)

C can't keep `grid[z][y][x]` for an array whose size is chosen at runtime, so every
access changes once, to a macro that hides the layout.

### Access sites (counted 2026-11-20, `name[..][..][..]`)

| layer | src | tests | files |
|---|---|---|---|
| `grid` | 664 | 1,072 | 77 |
| `designations` | 188 | 18 | 7 |
| `trackConnections` | 44 | 53 | 8 |
| `lightGrid` | 22 | 78 | 4 |
| `farmGrid` | 18 | 74 | 9 |
| `waterGrid`, `fireGrid`, `smokeGrid`, `steamGrid`, `temperatureGrid` | 177 | 22 | |
| 29 other layers (materials, trees, wear, flags, sim scratch, ...) | ~200 | ~40 | |

Row pointers (`waterGrid[z][y]` passed to `fwrite`/`fread`) only appear in `saveload.c`
(48). The 2D planes (`blockVisited`, `visited`) are scratch and move to the scratch arena
in step 2, not to the accessor.

### The macros

One macro per layer, coordinates in **(x, y, z)** order like the existing accessor
functions (`GetWallMaterial(x, y, z)`), usable as an lvalue:

```c
// grid.h
#define GRID(x, y, z)        (grid CELL_AT(x, y, z))       // CELL_AT supplies the brackets
#define CELL_FLAGS(x, y, z)  (cellFlags CELL_AT(x, y, z))
#define WATER(x, y, z)       (waterGrid CELL_AT(x, y, z))
...                          // one per layer, next to its extern
```

Existing helpers (`GetWaterLevel`, `GetWallMaterial`, ...) keep their names and use the
macros inside.

### Two phases: rewrite with no change in behaviour, then flip the storage

**Phase A, rewrite (no behaviour change).** While the layers are still static
`[z][y][x]` arrays, the macros map straight onto them:

```c
#define CELL_AT(x, y, z)  [(z)][(y)][(x)]    // phase A: GRID(x,y,z) -> (grid[z][y][x])
```

so every rewrite commit compiles to the same code. Rewrite with ast-grep, one layer (or
a batch of small layers) per commit, `make test` + `scenarios-check` after each:

```bash
sg run -p 'grid[$Z][$Y][$X]' -r 'GRID($X, $Y, $Z)' -l c src/ tests/ --update-all
```

(note the order swap: the pattern binds z, y, x and the macro takes x, y, z). Save/load
row loops become a helper that writes/reads one row element by element
(`WriteLayerRow(f, layer, elemSize, y, z)`), so the save format stays the same. A guard
test greps that no `name[..][..][..]` access is left for a finished layer.

Batches: `grid` src (664), `grid` tests (1,072), `designations`, the fluid/heat sims
(water, fire, smoke, steam, temperature), materials + flags + vegetation, trees + wear +
farm + light + snow + rooms + tracks + furniture, then saveload/inspect. Each batch is
reviewable and reversible on its own.

**Phase B, flip (one small commit).** Declarations become flat arrays and `CELL_AT`
becomes an index:

```c
#define CELL_AT(x, y, z)  [CellIndex((x), (y), (z))]
```

Still statically sized at the cap in this commit (allocation is step 2), so the only
change is the layout; with the debug assert below, any surprise shows up here.

### Layout: chunk-major with a fixed 16x16 storage chunk

```c
#define STORE_CHUNK_SHIFT 4                    // 16x16 cells per storage chunk
#define STORE_CHUNK       (1 << STORE_CHUNK_SHIFT)
#define STORE_MASK        (STORE_CHUNK - 1)
extern int storeChunksX;                       // ceil(gridWidth / 16)
extern int storeDepth;                         // gridDepth

static inline int CellIndex(int x, int y, int z) {
    int chunk = (y >> STORE_CHUNK_SHIFT) * storeChunksX + (x >> STORE_CHUNK_SHIFT);
    int local = ((z * STORE_CHUNK) + (y & STORE_MASK)) * STORE_CHUNK + (x & STORE_MASK);
    return chunk * (STORE_CHUNK * STORE_CHUNK * storeDepth) + local;
}
```

- **The storage chunk is fixed at 16x16, separate from `chunkWidth`/`chunkHeight`** (the
  HPA* chunk, chosen at runtime; scenarios use 8 and 16). A fixed power of two keeps
  `CellIndex` to shifts, masks and one multiply. The two don't need to line up.
- **A chunk holds the full column (all z)**: 16x16x16 = 4,096 cells per chunk. That is
  the unit the sparse layers allocate (step 3, one pool block per chunk per layer) and
  the unit a streamed world would load. Columns also keep the common `z +- 1` lookups
  (support below, fluids rising) in the same block.
- Storage dimensions round up to whole chunks, so a 100-wide map stores 112 columns;
  cells in that margin are valid memory (like today's padding) but never "on the map".
- Cost: one multiply-add and a few shifts per access instead of compile-time strides.
  Measure `--profile` before and after the flip; expected a few percent on cell-heavy
  loops at most. If a hot loop suffers, iterate it per chunk (`FOR_EACH_STORE_CHUNK`)
  rather than per row.

### Edge reads (the main risk)

Today `grid[z][y][x + 1]` at the last column reads padding (zero). After the flip, at a
chunk border it reads a cell in another chunk: a silent change in logic, not a crash.

- `CellIndex` gets a **debug-build assert**: `0 <= x < storeWidth`, same for y, z
  (`make debug`, tests and `asan`). Every hit is a missing bounds check: fix it at the
  call site with an explicit check, not by widening the assert.
- Run the asserting build over `make test`, `make scenarios-check` and a few minutes of
  each scenario headless, before the flip lands.
- After step 2 the arrays are exactly sized, so an unchecked read past the edge becomes an
  out-of-bounds access that asan reports: a second net.

### What else changes

- `sizeof(layer)` users (memory panel in `ui_panels.c`) and the duplicate `extern`
  declarations (e.g. `main.c` re-declares `waterGrid`) move to the shared headers.
- `ClearGridLayer` / `FillGridLayer` become "clear `storeCells` elements" after the flip:
  the high-water logic goes away once layers are sized to the map (step 2).
- `nav/` doesn't see the layers: it asks through `src/world/nav_host.h`, which changes
  with everything else.
- Save format unchanged throughout (rows are still written in `[z][y][x]` order).

### Phase A: done (2026-11-20)

Commits `2b5fecf`..`44e5a6b`. 35 layers have macros; ~2,700 sites rewritten (src + tests),
`saveload.c` rows go through `SAVE_ROW`/`LOAD_ROW`, `make test` runs
`tools/check_layer_access.sh`. Verified: src and every test file compile to byte-identical
machine code (`otool -t` on unity.c and each test .o, before vs after); the saveload change
writes byte-identical saves, also after load + 200 ticks and for a v93 migration.
Gotcha: ast-grep can't see inside macro bodies (helper macros in `grid.h`/`material.h`, a
`#define` in `mover.c`, one in `test_mover.c`): those were done by hand/regex.

### Phase B: done (2026-11-21), as built

Differs from the sketch above where measurements said so:

- **Fixed chunk-row stride.** `STORE_CHUNKS_X` is a compile-time constant (largest map), not
  derived from `gridWidth`: a runtime stride cost ~20-30% on whole-map sweeps. Chunks a small
  map doesn't use are never touched (no RAM). Step 2 can keep this by reserving the layers in
  virtual memory at the largest size, or rebuild the tables below for a runtime size.
- **Lookup tables.** `CellIndex(x, y, z) = cellIndexTabX[x] + cellIndexTabY[y] + (z << 8)`
  (`InitCellIndexTables`, called from `InitGridWithDimensions`). Faster than the shift/mask
  formula (3.64 vs 3.93 ms/tick at 512).
- **Padded chunks.** Chunks are `STORE_CHUNK_STRIDE` = 4096 + 64 elements apart: a power-of-two
  stride put the same row of every chunk in the same L1 sets (3.80 -> 3.64 ms).
- **Margin.** The map spans `(gridWidth + 16) / 16` chunks, so reads at x == gridWidth /
  y == gridHeight land in cleared margin cells like the old padding did.
- **Sweeps.** `FOR_X_IN_ROW(x, i, y, z)` walks a row with an incrementing index (same order as a
  plain x loop) and `LAYER_AT(layer, i)` reads it. Water and freezing sweeps skip whole 16-cell
  runs with nothing to do; TreesTick skips non-tree cells with one range test. All exact: same
  result as before.
- Tried and rejected (measured): row-major with runtime stride (4.05 ms), z-plane chunks
  (z, cy, cx, ly, lx) (3.72-3.82), visiting chunks in storage order (no gain, changes order).

Results (world scenario, old static build vs new, interleaved runs):

| | old | new |
|---|---|---|
| 512 map, ms/tick (3 runs avg) | 3.44 | **3.17** (Water 1.75 -> 1.50) |
| 1024 map, ms/tick | 9.45 | 9.51 (Water 1.76 -> 1.0, Trees 0.90 -> 0.97) |
| 128 map peak RSS, cap 512 / cap 1024 | 109 / 171 MB | **50 / 50 MB** |

Verification: old and new builds write byte-identical saves after 1200 ticks (world 512 and
100, ladders, crafting, hubs) and for the dollhouse game. Survival can't be compared: it picks a
random seed every run, even with `--seed` (being fixed in `games.c` by the UI agent; it is also
the likely cause of a rare `scenarios-check` failure). A `-DNAVKIT_CELL_CHECK` build reports
every out-of-range cell access: over all test suites, every scenario (incl. map sizes 100 and
200), all games and all terrain generators it found one (a workshop footprint past the map
edge), now guarded.

### Order and rules

1. Wait until `ui-gamedef` (another agent's branch) is merged; then nobody else edits
   `src/` or `tests/` until phase A is done.
2. Phase A batches as above, each its own commit, pushed.
3. Phase B flip with the asserting build green; profile before/after.
4. Then step 2 (runtime sizing, world arena).

## Step 2: layers sized to the map

With the accessor in place, allocate layers at world creation/load (`InitGridWith...`,
`LoadWorld`) instead of statically. Removes the compile-time cap (`make path1024` no
longer needed; the limit becomes RAM). A 128 map's layers drop to a few tens of MB;
a 1024 map still costs what it costs (~75 bytes per cell dense, ~1.2 GB).

### Step 2: done (2026-11-21), as built

- Layers are pointers (`T* name`) into one **world arena** (`src/world/grid_layers.c`): a
  single `calloc` sized for the current map (`gridLayerCells` elements per layer). A registry
  table lists every layer with its element size and empty value; adding a layer = one row.
- `AllocGridLayers()` (called by `InitGridWithDimensions`, and by `inspect.c`) reallocates only
  when the map size changes, rebuilds the `CellIndex` tables for the real chunk count
  (`storeChunksX/Y`), and fills fresh layers with their empty value (water stable bit,
  designation `assignedMover = -1`, source items 0xFF, explored 1).
- A first arena is allocated before `main()` (constructor) for the default size and left all
  zero, like the static arrays were: some code (tests, `ClearMovers`) touches layers before any
  `InitGrid`.
- `ClearGridLayer` / `FillGridLayer` clear the whole (exact-size) layer; the high-water logic
  is gone. `sizeof(layer)` is now a pointer size: `tools/check_layer_access.sh` rejects it and
  raw `mem*` calls on layers (caught one in `test_fog.c`).
- **The compile-time cap no longer limits layers**: a `-DMAX_GRID_WIDTH=2048` build links
  (static data 548 MB, mostly untouched scratch: `bfsQueue` 256 MB, `moverPaths` 117 MB).
  `MAX_GRID_WIDTH/HEIGHT` still size those scratch buffers and the index tables.
- Cost vs step 1: ~3% per tick at 512 (3.22 -> 3.32 ms; still below the original 3.44),
  setup +7%: every access loads the layer pointer.
- Verified: byte-identical saves vs step 1 for world 512/100, ladders, crafting, hubs, train,
  survival (fixed headless seed and `--seed 7`), dollhouse, sandbox; v93 save loads identically.
- 1024 world: 8.6 ms/tick, setup 5.0 s (graph 3.9 s), peak RSS 1.03 GB, footprint 1.81 GB.

**First 2048x2048 run (2026-11-21)**, `-DMAX_GRID_WIDTH=2048 -DMAX_GRID_HEIGHT=2048` build,
`--headless --scenario world --set scenarioSize 2048 --ticks 1500 --profile`:

| | |
|---|---|
| setup | 21.4 s (HPA* graph 15-16 s: 1.2M entrances, 27M edges) |
| tick | 23.1 ms avg; ~14 ms without the one 13.5 s first-tick spike (Move: first path refinement) |
| largest steady sections | Trees 4.4, Water 2.8, Grid 1.6, Lighting 1.5 ms |
| peak RSS | 1.68 GB |
| peak memory footprint | **6.8 GB** |

The footprint is the problem on an 8 GB machine: world reset clears every layer with memset(0),
which dirties all ~5 GB of fresh zero pages (compressed by macOS, still counted). Next fixes,
in order of payoff: (1) clear zero layers by replacing their pages (fresh zero pages, no
footprint) instead of memset; (2) sparse layers (step 3), designations first (16 B/cell, and
its non-zero empty value forces a full write); (3) graph build time and the first-tick spike.

Note (pre-existing, found 2026-11-21): `test_workshop_deconstruction` segfaults (a workshop type
without a construction recipe; `GetConstructionRecipe` returns NULL), and `make test` hides it:
every suite runs with `-@`, so crashes and failures don't fail the target.

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
