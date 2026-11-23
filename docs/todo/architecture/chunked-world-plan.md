# From fixed arrays to a chunked (and eventually endless) world

> Status: partial (updated 2026-11-22). Decided: big bounded maps (target 2048x2048),
> endless world deferred. Steps 1-2 done, step 3 (sparse layers) partly; 2048 runs. Latest
> measurements and what's next: "5000 movers on a 1024 hill map" at the end. Ties together
> `large-worlds.md` (what big maps cost, measured) and `../../vision/infinite-world-exploration.md`
> (endless world research). Numbers are measured unless marked as estimates.
>
> **Key finding (2026-11-23): a much taller world costs little more.** On generated terrain the
> content of the world sits in a band 2-4 levels thick around the ground; everything above is
> air and below is rock. With per-block palettes, memory follows the land surface, not the
> volume, so 64+ levels need not cost 4x. See "Taller worlds: block palettes (measured)".

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
- Verified: byte-identical saves vs step 1 for world 512/100, ladders, crafting, hubs,
  survival (fixed headless seed and `--seed 7`), dollhouse, sandbox; v93 save loads identically.
  **Correction (2026-11-21):** `train` was reported identical too, wrongly: it differs in 10,240
  empty water cells' `stable` bit (step 1: 0, step 2: 1). The train map (80x20) is set up after
  the 32x32 startup map without a `ClearWater`; step 1 left cells beyond the earlier map's
  high-water mark at 0, the arena gives fresh layers their empty value. No behaviour change:
  with no water the water sim never looks at them (`waterActiveCells == 0`).
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

**Zero-page clearing (2026-11-21).** The arena is an anonymous `mmap` (POSIX) with every layer
page-aligned; `ClearGridLayer(..., 0)` replaces the layer's pages with fresh zero pages
(`MAP_FIXED`) instead of writing zeros. Also stopped three loops writing every cell: `InitTrees`
(now three zero-page clears), the snow-accumulator reset and `ClearBlockLight` (write only cells
that hold a value). Note the layout: a 16x16 chunk column (all z) of a 4-byte layer is one 16 KB
page, so one write per column (e.g. a surface sweep) dirties the whole layer.

| peak footprint | before | after |
|---|---|---|
| 128 map, cap 512 / cap 1024 | ~50 MB | 35 / 49 MB |
| 1024 map | 1.81 GB | 1.40 GB |
| 2048 map | 6.8 GB | 5.9 GB |

2048 breakdown (`footprint <pid>`, 45 s in): layer arena 3.5 GB dirty of 5.2 GB mapped,
`MALLOC_LARGE` 1.06 GB (HPA* graph etc.), `MALLOC_SMALL` 0.63 GB. The arena is now dirtied by
real data and by the non-zero empty-value fills when the map size changes: designations alone
are 16 B x 69M cells = 1.1 GB written just to set `assignedMover = -1`, plus water (stable bit),
source items (0xFF) and explored (1). That is step 3's job: sparse layers whose empty value is
"no block". `GridLayersReport()` (printed by headless `--profile`) lists resident MB per layer,
but `mincore` doesn't count pages macOS has compressed, so use footprint for totals.

Note (found 2026-11-21, fixed the same day by the UI agent): `test_workshop_deconstruction`
segfaulted (Stove/Counter had no construction recipe) and `make test` hid it; `make test` now
fails on any crashed or failing suite.

## Step 3: sparse layers

Water, fire, smoke, steam, snow, farming, designations are empty almost everywhere.
With chunk-major storage (layout B), give a chunk memory for such a layer only when
something lives there. Estimate (`large-worlds.md`): ~15 bytes per cell dense, so
2048x2048x16 at ~1-1.5 GB instead of 5 GB. Pairs with active-chunk simulation (sims
iterate chunks that have something, not the whole map).

### Step 3 progress

**Reading untouched memory is not free on macOS** (measured 2026-11-21): reading every page of a
fresh 2 GB anonymous mapping gave 650 MB resident / 1.95 GB footprint, almost as much as writing
it. So a dense layer is only cheap if nothing *reads* it everywhere either; sweeps that read a
whole layer (e.g. `ClearMovers` over all designations) need real sparse storage.

**Sparse layer** (`SparseLayer` in `grid.h`, `grid_layers.c`): one block per 16x16 chunk column
(4096 cells, all z), `NULL` = every cell empty. `SparseLayerRead` returns the shared empty value
for a missing block and never allocates; `SparseLayerWrite` allocates the block (filled with
the empty value). Blocks are freed by `SparseLayerClear` and on map-size change.

**Designations (done, 2026-11-21):** `DESIGNATION(x, y, z)` is now read-only (`const`), writes
go through `DESIGNATION_MUT(x, y, z)`. Making the read macro `const` let the compiler find every
write site (115 in src, 11 in tests). `GetDesignation()` returns NULL for empty cells, so its
callers only touch existing blocks. Save/load: `LOAD_ROW_SPARSE` writes only non-empty cells
into a cleared layer; the file format is unchanged.
Verified: byte-identical saves vs step 1 (world 512/100, ladders, crafting, hubs, mood, survival,
dollhouse, sandbox; a crafting save with active mining designations loaded and run 600 ticks;
the v93 save). 512 tick time unchanged within noise.

| peak footprint | before | after |
|---|---|---|
| 1024 map | 1.40 GB | 1.11 GB |
| 2048 map | 5.9 GB | 4.65 GB (world scenario: 0 of 16,641 blocks allocated) |

**Water, fire, smoke, steam, farm (done, 2026-11-21).** Same pattern: `LAYER(x, y, z)` read-only,
`LAYER_MUT` writes (compiler-found), read-only users (tooltips, rendering, counters, jobs) use
const pointers so looking never allocates. Sweeps skip chunks without a block: water with
`FOR_X_IN_SPARSE_ROW` (same x order), fire/smoke/steam/farm with `SparseLayerHasBlock` per cell
(smoke and steam alternate scan direction). Water's empty value is "dry and stable", so its
whole-layer stable fill is gone. Fire/smoke/steam's empty value is all zero = *unstable*: the old
sweeps processed every never-touched empty cell once any fire/smoke/steam existed anywhere, only
to mark it stable, each using a slot of the per-tick update cap. Skipping chunks without a block
drops that work, so fire/smoke/steam timing can differ from step 1 once they burn; the test
suites (fire, steam, smoke via wind/snow) pass. Smoke/steam "destabilize all after load" loops
only touch existing blocks.
Saves byte-identical to step 1 for world 512/100, ladders, crafting, hubs, mood, survival,
dollhouse, sandbox (train: the known empty-water stable bit). 512 tick 3.27 -> 3.21 ms.

| peak footprint | before | after |
|---|---|---|
| 1024 map | 1.11 GB | 0.90 GB |
| 2048 map | 4.65 GB | 3.83 GB (water 476 of 16,641 blocks = 3.7 MB; fire/smoke/steam/farm 0) |

**Last non-zero empty fills (done, 2026-11-21).** Explored is stored inverted (`unexploredGrid`,
zero = explored, the sandbox default; `EXPLORED(x, y, z)` is a value, write with `SET_EXPLORED`;
survival start clears it to 1). Source items are stored XOR 0xFF behind `Get/SetWallSourceItem`
(zero = none). Saves keep the plain values (`SAVE_ROW_XOR` / `LOAD_ROW_XOR`). Fresh layers now
need no fill at all. Saves byte-identical to step 1 (all scenarios and games, incl. loading a
mid-game survival save and a crafting save).

| peak footprint | before | after |
|---|---|---|
| 1024 map | 0.90 GB | 0.87 GB |
| 2048 map | 3.83 GB | 3.57 GB |

Next candidates (2048): tree data (growth timer 4 B +
height + harvest, written in every chunk with a tree), then the HPA* graph (~1.7 GB of malloc).

### 2048 in the normal build (2026-11-21)

`MAX_GRID_WIDTH/HEIGHT` default to 2048 (`path1024` target removed): the cap only sizes static
scratch buffers that cost address space, not RAM. Measured, normal vs 2048 cap: 128 map 3.08 /
3.06 ms per tick, 27 / 28 MB footprint; 512 map 3.25 / 3.25 ms, 269 / 266 MB.

**GUI at 2048** (`./build/bin/path --scenario world --set scenarioSize 2048`): loads in ~70 s,
renders normally, 20 FPS vs 24 FPS for a 512 world: drawing is culled to the view, so its cost
depends on what is on screen, not on map size (`DrawCells` ~25 ms either way: a separate,
general render-speed topic). Unverified: the 2048 view shows black patches that look like
depressions deeper than the levels the renderer shades.

**Saves at 2048 were the blocker**: a world save was 3.65 GB (every cell of every layer,
uncompressed; the game writes raw `.bin`, `.gz` is only unpacked on load).

**Run-length coded layers, save v95 (done, 2026-11-21)** (`src/core/save_rle.h`): each layer is
still one `[z][y][x]` stream, coded as runs (`varint (n<<1)|1` + element) and literal stretches
(`varint n<<1` + n elements; runs under 4 go into literals, so noisy data costs little). Rows go
through the same `SAVE_ROW`/`LOAD_ROW` macros; a layer's stream starts at its first row and ends
after its last. Loading clears each layer first and writes only non-zero cells. v94 and older
load as plain rows; `inspect.c` reads both (`InspReadLayer`/`InspSkipLayer`).

| save size | v94 | v95 |
|---|---|---|
| world 100 | 12.2 MB | 3.2 MB |
| world 512 | 242 MB | 6.4 MB |
| world 2048 | 3.65 GB | **50.5 MB** |
| small scenarios / games | 2-17 MB | ~1.8-2.1 MB (the non-grid sections) |

2048: save ~7.7 s (was ~5 s), and saving raises the footprint by ~850 MB because it reads every
cell of the dense layers (reading untouched memory costs footprint on macOS); load ~42 s total.
Verified by state comparison (the format changed, so files can't match): old build v94 run and
new build v95 run of the same scenario, both loaded and re-saved by the new build (same
conversion on both sides: a load clears item reservations, so converting only one side shows a
false difference), byte-identical for world 512/100, crafting, mood, train, ladders, survival,
dollhouse, sandbox; `--inspect` output identical for v94 and v95 files; the v93 save migrates
identically.

### The HPA* graph at big sizes (measured 2026-11-21)

`--headless --profile` now also prints a nav memory breakdown (entrances, edges, vector
capacity, flat cache, chunk index, scratch, regions) and how many entrances sit on chunk borders
vs at ladders/ramps. `--set scenarioChunk N` sets the HPA* chunk size of sized scenarios.

world scenario, 1024: 297,922 entrances, 6.6M edges, nav 215 MB allocated (+45 MB unused edge
vector capacity); 2048: 1.2M entrances, 27M edges (~4x). Graph build 4.0 s at 1024, 16.6 s at
2048. First-tick spike at 2048 (~16 s in Move): 100 movers' abstract searches (`nav_AbsRelax`),
~160 ms each, to random goals across the map.

**85% of the entrances are ramps**: at 1024 only 45,504 entrances are on chunk borders; 252,418
are ladder/ramp entrances (126,198 ramp links from the hills' natural slopes, 2 entrances each).
The search inside a chunk is per z-level, so every ramp, even one entirely inside a chunk, must
be a graph node to connect two levels, and gets edges to the other entrances of its chunk. That
drives the edge count, the build time (all entrance pairs per chunk) and the abstract search
(it crawls through a million ramp nodes).

Chunk size 32 instead of 16 (1024): entrances 298K -> 274K only, edges 6.6M -> 10.1M, build 4.0
-> 8.6 s, tick 9.0 -> 10.0 ms: worse. Bigger chunks are not the fix.

### Plan: chunk-local 3D search (ramps and ladders inside a chunk stop being graph nodes)

Order: (1) safety net, (2) the 3D search in sub-steps, each committed after `make test` +
scenarios-check: a. per-chunk distances over all z of the chunk column, ramp/ladder entrances
still there (paths and costs must not change); b. drop the entrances of ramps/ladders inside a
chunk; c. incremental chunk updates and regions; d. path refinement. (3) Measure. (4) Threaded
graph build only if the build is still too slow. (5) A coarser level only if long searches still
spike.

**Step 1 done: safety net.** The nav fuzz test has a `hills` generator (game-style heightmap:
one walkable cell per column, ramps on slopes, ladders on cliffs, ladder shafts, caves; up to 6
levels, chunks often 8 or 16 so whole slopes sit inside a chunk). Its summary line counts ramps
inside chunks and A* paths that leave a level and come back within one chunk (the case the 3D
search must handle by itself). Deep sweep `test_nav_fuzz 2000 1000` on today's code: 0 failures,
43,541 inner ramps, 5,595 of 206,327 paths with such an excursion.

Baseline, world scenario 1024, 300 ticks, `--profile`: 297,922 entrances (45,504 on borders),
6.6M edges, nav 215 MB (+45 MB vector slack), graph build 3.97 s, setup 5.07 s; 133 FindPath
calls 3,441 ms (25.9 ms each); 15.2 ms/tick; peak footprint 0.96 GB. 2048: see above (build
16.6 s, first-tick spike ~16 s).

**Step 2 done: HPA* chunks are 3D columns** (`nav/nav.c`). A chunk is chunkW x chunkH over all
levels (chunk id = `cy * chunksX + cx`); every search inside a chunk (edges, start/goal
connection, refinement, hierarchical chunk fields, rescue) is 3D with A*'s exact step rules
(`nav_Steps`, `nav_LocalSearch`). Entrances: border cells, ramps whose exit is in the next chunk,
and **ladders** (kept as graph links: their cost changes at query time with the queue,
`NAV_CLIMB_EXTRA`, and the abstract search must see it; edge searches skip ladder steps, and edge
costs leave climb extra out so the graph depends on terrain only). Every ladder/ramp is still
listed in `ladderList`/`rampList` (counts keep their meaning); internal ramps have entrance -1.
Edge searches build each chunk's walkable cells and steps once as a compact graph
(`nav_BoxGraph`) and run all its entrances' Dijkstras on it: the 3D searches asking the host per
cell had made the build 2x slower than before (7.8 s at 1024); with the box graph it is 1.15 s.
The incremental update works per column (an edit rebuilds its column and neighbours, all levels).

Results (same runs as the baseline):

| | before | after |
|---|---|---|
| 1024 entrances / edges | 298K / 6.6M | 60K / 2.6M |
| 1024 nav memory | 215 MB | 80 MB |
| 1024 graph build / setup | 3.97 s / 5.07 s | 1.15 s / 2.15 s |
| 1024 FindPath avg | 25.9 ms | 7.3 ms |
| 1024 tick (300 ticks) | 15.2 ms | 7.0 ms |
| 1024 peak footprint | 0.96 GB | 0.69 GB |
| 2048 graph build / setup | 16.6 s / 21.4 s | 4.9 s / 9.4 s |
| 2048 first-tick spike (Move) | ~16 s | ~4.2 s |
| 2048 peak footprint | 3.57 GB | 2.97 GB |
| 512 hills+water (`bench_nav_update`) full build | 878 ms | 254 ms |
| incremental update (3 walls) | 18.6 ms | 11.8 ms |
| HPA* short / medium / long | 0.80 / 3.78 / 6.83 ms | 0.43 / 1.26 / 2.36 ms |

Path lengths are unchanged (avg steps 94/281/497 -> 94/282/499). Nav fuzz deep sweep (2000 worlds),
make test and scenarios-check pass.

**The rest of the 2048 first-tick spike (measured, 2026-11-21).** The `SLOW PATH ... len=0` lines
were not failed searches: with coarse paths the route comes back separately and `len` is always 0
(unreachable goals are already answered by the region check). Two causes:
1. The reachability regions were built lazily by the first path request: 949 ms at 2048. The game's
   `BuildGraph()` now builds them with the graph (setup +0.9 s, first tick -0.9 s).
2. Cross-map routes (~190 waypoints) searched 70-108 ms each, visiting ~78% of all entrances: the
   heuristic assumes 8 per cell (constructed floor) while natural terrain costs >= 10, so A* is close
   to Dijkstra. New `NavWorld.absHeuristicPct` (library default 100 = exact); the game sets 125.
   92 cross-map routes at 2048: abstract search 3,101 -> 2,263 ms, nodes visited 60.9K -> 43.8K per
   route, route costs identical. (150 would halve it but cost +2% overall, worst route +5.4%.)

2048, 300 ticks: FindPath total 4.97 -> 3.35 s, Move 16.0 -> 10.9 ms/tick. The remaining slow
requests (200-370 ms) are long cross-map routes and a shared-goal field over its build budget. A
coarser abstraction level (step 5) is what would make cross-map routes cheap; not needed yet.

## Taller worlds: block palettes (measured 2026-11-23)

**The realization:** height is nearly free. Today every column stores all 16 levels at ~75-87
bytes per cell, so a 64-level world would cost 4x. But generated terrain is a thin skin: what
varies is a band 2-4 levels thick around the ground, with uniform air above and uniform rock
below. Store the world in blocks with a **palette** (each block keeps its few distinct cell
values once, plus a small index per cell; a block with one value stores no index at all), and
extra levels above or below the ground cost next to nothing. Memory then scales with the land
**surface**, not the volume. That opens real relief (valleys, cliffs, mountains several times
today's 11 land levels) without the 4x.

**Measured** (1024x1024x16, island seeds 3 and 5, hills seed 3; throwaway tool linked against
`test_unity.o`). "Cell value" = cell type + wall material + floor material + water level + cell
flags + vegetation (the layers that say what a cell *is*; simulation layers not included):

| block (x, y, z) | single-value blocks: island / hills | mean distinct values (island) | index bits per cell (island) |
|---|---|---|---|
| 16x16x2 | 59-62% / 18% | | |
| 16x16x4 | 50-52% / 7% | 2.8-3.0 (max 22) | 1.1 |
| 16x16x8 | 32-34% / 0% | | |
| 16x16x16 | 0% / 0% | 7.0-7.5 (max 24) | 2.9 |

- Island 16x16x4 blocks by palette size: 1 value 50%, 2-4 values 31%, 5-16 values 19%, over 16
  values 0.2%.
- Per column, the band that is neither plain bottom nor plain air: 2-3 levels for 85% of island
  columns (mean 3.8, up to 15 under trees and in the mountains); hills mean 8.5 (its ground sits
  high in a 16-level world).
- With 16 levels, a 16-tall block always contains the ground, so single-value blocks alone save
  little today. Palettes are what work: these layers take ~9-10 bytes per cell dense against
  ~1-3 bits plus a few palette entries per block, about 25-70x smaller (estimate from the
  numbers above).
- **Estimate for a taller world:** levels added above or below the ground become single-value
  blocks (no index). A 2048-wide, 64-level island looks feasible for these layers.

**Caveats (not measured):**
- **Simulation layers** (temperature, light, wear, snow, ...) vary from cell to cell and don't
  palette-compress. They are a large part of today's 75-87 B/cell. They need to exist only where
  something happens (sparse, active blocks) or at a coarser resolution. This is the real work.
- **Steep relief** in a taller world spreads the ground over more blocks than here.
- **Colonies**: dug tunnels and buildings add mixed blocks; only generated terrain was measured.
- **Layered geology** (strata) adds a few palette values per block; still cheap.
- Sims that sweep cells (water, temperature, trees) would have to visit only non-uniform /
  active blocks. `CELL_AT` is still the single place that knows the layout, which is what makes
  a storage change like this possible at all.

### A lived-in island (measured 2026-11-23)

Scenario `island-colony` (scripted, `src/core/scenarios.c`): the seed-3 island at 256, 20
settlers at the landing beach, 80 trees to chop, 40 cells to dig, 150 grass cells, a stockpile,
two stone huts with thatch floors (58 blueprints); needs on, nobody dies. Three game days =
64,800 ticks: `--headless --seed 3 --scenario island-colony --ticks 64800 --profile --save f`.

**Tick cost (re-measured 2026-11-23 by navkit-8e on clean builds):** world simulation
(`TickWithDt`) about 2.9 ms per tick on master (water 2.1 of it, worst tick 224 ms), the biggest
cost. Jobs only 0.18 ms per tick at 82eca465 (worst 77 ms, 466 ticks over 5 ms) and 0.29 ms on
master (worst 105 ms, 728 ticks over 5 ms); both runs end with 58 blueprints still open. The
remaining job spikes are in `AssignJobs_P4_Designations` (blueprint hauls running path searches).

**Job spikes fixed (2026-11-23):** all of them came from one idle settler stranded in a pocket on
the mountain (117,101,z13). It walked up a ramp and left it from the side; ramps can be entered only
from their low or high side, so the way back down does not exist (A* agrees), but reachability
regions treat every connection as two-way and said "reachable". Each assignment round it ran ~5
failing path searches per blueprint (49 blueprints). `WorkGiver_BlueprintHaul` now remembers "no
way" per mover until it moves, a cell's walkability changes, or 10 s pass. From the day-3 save,
3000 ticks, clean builds: Jobs 1.0 -> 0.14 ms per tick, ticks over 5 ms 215 -> 5, worst 94 -> 16 ms.
Still open: one-way pockets themselves (settlers can strand themselves; regions can't see them).

*Retracted:* the first measurement here said Jobs 7.3 ms per tick (worst 458 ms). That binary
was built from the shared main tree while other sessions had uncommitted changes in it, so it
did not match any commit and does not reproduce. Measure only from a clean build (a worktree at
a known commit).

**Blocks after three days** (same tool, `--save` loaded): 16x16x4 blocks with one value 43%
(fresh island 44%), mean 5.0 distinct values per block (fresh 4.4), index 1.7 bits per cell:
a young colony barely changes the palette picture. Simulation layers, share of 16x16x4 blocks
holding any non-default value: water 29%, tree growth timers 24%, tree target height 10%, tree
harvest state 8%, ground wear 6%, designations 0.8%, floor dirt 0.1%; temperature offsets,
torch light, snow, rooms, fire and smoke 0%. So in this colony every simulation layer lives in
under a third of the blocks, most in under a tenth: active-block storage fits them well.

**Not covered by this colony** (see the winter colony below): fire, campfires and torches
(temperature and light), winter (snow). Still not covered by either: rooms, digging deep, a
colony months old.

**The winter colony** (scenario `island-winter`, measured 2026-11-23 on a clean build of
b7204887 + the scenario): the same colony started on the first day of winter (day 22) while it
snows, with a fire pit that burns for good (`SetFireSource` on fuel-free ground) and 12 torches
at the stockpile and hut corners. Three game days, same command. Share of 16x16x4 blocks with
any non-default value: **snow 54%** (6.25% of cells: exactly one cell per column), water 27%,
growth timers 24%, sky partly lit 9%, smoke 2.8%, wear 1.5%, torch light 1.4%, temperature
0.2%, fire 0.1%; rooms 0% (the huts never got their stone). Palette: 44% of 16x16x4 blocks hold
one value, mean 4.7 values per block, 1.7 bits per cell, the same as summer.
Tick cost: world simulation 1.7 ms per tick (smoke 0.58, temperature 0.47, water 0.41, worst
tick 84 ms); jobs 0.38 ms (worst 165 ms). One fire and 12 torches cost little; smoke and
temperature appear only around the fire.

What it means for the storage design: **every layer except snow stays under a third of the
blocks**, and heat, light, fire and smoke stay in a few percent around their sources, so
active-block storage fits them. **Snow is a surface layer**: it covers half the blocks but only
one cell per column, so it belongs in a 2D per-column map (or the surface block), not a 3D
layer. A taller world does not make it bigger.

Side finding: snow lies on the top non-air cell of every column, including the sea bed under
open sea (water is not a cell type), so all 65,536 columns had snow; snow should skip columns
with water on top (`SnowRow` in `src/simulation/weather.c`, not fixed here). The light grid
is derived, not saved: measuring light from a save needs `RecomputeLighting()` after loading.

**Side findings:** with work times in game hours a tree takes 10-15 real seconds to chop, so the
`--check` stuck rule (no movement for 2 s) counts working settlers as stuck: 7-12 of 20 early on,
0 at the end. The scenario is therefore not in `scenarios-check`. `--save f.bin.gz` used to write an
uncompressed file while `--load` gunzips by the `.gz` extension; fixed 2026-11-23 (a `.gz` name
now gets a real gzip file).

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
4. **Taller worlds (2026-11-23):** block palettes make height cheap (see above). Decide
   whether more levels (64+) is a goal; if so, palettes for the "what a cell is" layers and
   active-block storage for simulation layers belong in the storage design from the start.

## References

- `large-worlds.md`: measured costs per map size and terrain, what was fixed.
- `../../vision/infinite-world-exploration.md`: streaming, delta saves, entity abstraction research.
- `../pathfinding/per-chunk-edge-storage.md`: the local graph updates.
- `unified-roadmap.md`: puts the infinite world in Era 3+.

### Steady-state tick at 2048 (2026-11-21)

`--headless --scenario world --set scenarioSize 2048 --ticks 600 --profile`: 21.2 ms/tick average.
Lighting 4.8, Trees 4.7, Move 5.9 (mostly the first-tick spike), Grid 1.8, GroundWear 1.3, Rooms 0.8
(spikes: Lighting 750 ms, Rooms 487, GroundWear 405, Repath 363).

**Lighting relights locally** (`simulation/lighting.c`). Every cell change reported to the
pathfinder (`NavOnChunkDirty`) and every light source add/remove used to trigger a full relight
of the map (~750 ms at 2048). Now they mark their 16x16 chunk (`InvalidateLightingAt`), and
`UpdateLighting` relights only around dirty chunks: sky columns within SKY_LIGHT_MAX, spread from
those plus the unchanged ring around them; block light cleared within twice the largest source
radius and re-added by the sources reaching in (writes clipped, BFS unchanged). More than 1/8 of
the chunks dirty, or `InvalidateLighting()` (loads, settings, scenarios): full recompute as before.
`test_lighting` checks local == full after 60 rounds of random walls/floors/torches. Lighting 4.8 ->
1.2 ms/tick, tick 21.2 -> 16.5 ms.

**Trees** (`simulation/trees.c`), 4.7 -> 1.2 ms/tick at 2048 (tick 16.5 -> 12.4 ms):
- A sample showed ~55K leaf decay checks (`IsConnectedToTrunk`) per tick on the forested 2048 map,
  all but 7 of 33M "still connected": every leaf checked every LEAF_DECAY_GH. A leaf can only lose
  its trunk when a trunk/branch cell within 3 cells changes, and every such change goes through
  `MarkChunkDirty` (solid cells: the pathfinder must know), whose host hook now calls
  `TreesNoteCellChanged`: it marks the 16x16 chunk and its neighbours for one decay period plus two
  sweeps. Leaves elsewhere just restart their timer (what a passing check did). Full rebuilds
  (`BuildEntrances`: loads, new worlds) mark everything once. Saves after survival 8000 / world-256
  8000 / crafting 6000 / dollhouse 6000 ticks are byte-identical to before. `test_trees` "leaves of
  a tree cut long after setup decay" fails without the hook.
- 16-cell runs without any tree cell are skipped with a 64-bit "byte in range" test (8 cells at a
  time; checked exhaustively).
- Band cap 64 -> 256: above 1024x1024x16 each cell is visited every 256 ticks (~4 s) with the time
  accumulated meanwhile (growth thresholds are game-hours). Maps up to 1024 are unaffected.

### Loading a 2048 save (2026-11-21)

Headless `--load` of a 2048 world save (53 MB): 19.7 s (42 s before the 3D graph). Phases of
`LoadWorld`: reading the grid layers 6.3 s, activity counts/ramp check/entrances 1.2 s, graph +
regions 5.7 s; and headless `main.c` rebuilt the graph a second time after `LoadWorld` (5 s).
- Duplicate headless rebuild removed (the GUI paths only ever built once).
- RLE runs are filled with one memset (or a doubling memcpy) instead of a memcpy per element;
  rows are scattered into the chunk-major layer per 16-cell run (address once per run), and
  all-zero runs are skipped; varint bytes are read with the unlocked getc (`_getc_nolock` on
  Windows). Grid layers 6.3 -> 2.6 s.
Load 19.7 -> 10.0 s. Loading the 2048 save and saving it again gives the same bytes as the old
code; survival and crafting saves loaded and run 500 ticks match too. What's left: graph build
4.8 s (threading the per-chunk edge searches would cut it most), grid layers 2.6 s, regions
0.8 s, activity counts 0.55 s.

### Threaded graph build (2026-11-21)

The per-chunk edge searches (box graph + Dijkstra per entrance) only read the world, so
`NavWorld.parallelFor` (a host hook; the nav library itself has no threads) runs them in
parallel. Each chunk writes its edges to its own buffer; the buffers are added to the graph in
chunk order after each batch of 1024 chunks, so the graph is identical with any thread count.
The game's hook uses pthreads, one thread per core (setting `navThreads`: 0 = per core, 1 = off).
Incremental updates use it too when more than 2 chunk regions need new edges.
- 1024 world: graph build 1155 -> 308 ms. 2048: 4.8 -> 1.3 s (8 cores); headless load of the
  2048 save ~11 -> ~7 s (back to back on the same machine). Incremental update on
  `bench_nav_update` (512, 3 walls): 11.8 -> 4.7 ms.
- Peak RSS of the 2048 load varies 0.89-1.02 GB (0.78 GB serial); on the 1024 world the threaded
  run was lower than serial, so this looks like per-thread allocator caching, not the buffers
  (< 16 MB per batch).
- Checks: the fuzz builds odd seeds through a `parallelFor` that runs jobs backwards and compares
  every entrance's edge list with a serial build (mutation: skipping one job fails 48/60
  worlds); built with `-DFZ_THREADS -fsanitize=thread` it uses real threads, 200 worlds clean.

### Hitches during play at 2048 (2026-11-21)

`--profile` now also prints `later` (worst tick after the first) and `slow` (ticks after the
first over 5 ms). Over 1200 ticks of the 2048 world, Rooms (458 ms) and Lighting (669 ms) only
spike on the first tick (one-off full computations). Repeating hitches: GroundWear 361 ms (4x),
Repath 246 ms (5x), Move 170 ms (10x), Snow 116 ms (1x).
- GroundWear swept all 67M cells in one tick per wear interval (~5 ns per cell, spread over
  several layers: no single hot spot). Maps over 1M cells now spread the sweep over the
  interval (rows due = rows x elapsed / interval); every cell is still processed once per
  interval. The position follows from the saved accumulator, so the save format is unchanged.
  Smaller maps sweep at the interval's end as before. Worst tick 361 -> 5.2 ms.

### Spawning many movers at 2048 (2026-11-21)

`spawnmovers 3000` (console; headless `--exec "spawnmovers 3000"`) took ~35-100 s: every new
mover got a full HPA* path (no region check, so unreachable goals searched the whole graph) to
a random goal anywhere on the map. Fixes:
- Spawns plan with `MoverPlanPath` (region check, coarse routes) within a budget of 300K
  explored abstract nodes; later movers get paths from the repath queue. 3000 movers: 0.45 s.
- `ZLevelIsConnected` scanned all ~500K ramp links per random-cell attempt (levels without
  ramps scanned them all); now one pass per graph version.
- `wanderRadius` setting (default 256): on maps bigger than that, spawned and endless movers
  pick goals within it (reachable by region). 0 = anywhere.
- Path planning has a per-tick budget of explored abstract nodes (`REPATH_EXPLORE_BUDGET`,
  10K ~ 7 ms), shared by the repath queue and endless movers' new goals (the first repath of
  a tick is always served). Nodes, not ms, so runs stay reproducible.
- A wanderer whose repath fails picks a new goal: goals in the same region without a path
  (regions ignore deep water) searched all 238K entrances every cooldown.
- Abstract search: the heuristic is computed only when a node improves; node state uses a
  generation stamp instead of a reset of every entrance (same results, small gain).
3000 movers on the 2048 world: 66 -> 28 ms per tick. What's left is the abstract graph itself:
on ramp-heavy hills ~200-cell routes have 90-210 waypoints and explore 30K-150K nodes (almost
every ramp is an entrance). That's the "coarser level" decision.

### Slow path requests with 3000 movers at 2048 (2026-11-21)

`SLOW PATH` lines now also print the route length, nodes explored and the abstract search time,
so a slow request shows where its time went. Three kinds:
- **A. Region join after any cell change (~250 ms, outside the search).** Relabelling the dirty
  chunks was cheap, but the join walked every chunk-border cell of the map (~8M cells x 3 step
  checks). Each chunk now keeps its east/south border steps, one cell pair per (component,
  component) pair (`regBorder`), recomputed when it or its east/south/south-east neighbour is
  relabelled. The join only unions those pairs plus the ladder/ramp links. Gone from the slow list.
  The nav fuzz test now checks that regions after incremental edits split cells exactly like a
  fresh build.
- **B. Failed searches over the whole graph (~200-250 ms, 238K nodes).** Regions ignore deep water
  (water never notifies the pathfinder), so a goal across a lake looks reachable but the graph
  has no route. An exhaustive failed abstract search reaches exactly everything reachable from
  its start; the library remembers that set (`absFailMark`, valid while `graphVersion` is
  unchanged). A later search whose start entrances all lie in it and whose goal entrances don't
  answers "no route" at once. Exact; nav fuzz deep sweep: 56,975 such answers, 0 misses.
  (Making water notify the graph when a cell crosses the walking depth would fix the regions
  themselves; not done: water moves through many code paths and flickers near the threshold.)
- **C. Real long searches (~100-150K nodes, 165-190 waypoints for ~250-cell routes).** The
  coarser-level question.
Same conditions back to back (3000 movers, 3000 ticks): path time 21.5 -> 17.6 s, worst later
tick 291 -> 195 ms, requests over 200 ms 8 -> 2, same arrivals.

**C measured (same run, every request logged; 9,045 requests, 22 s).** Cost is not about
distance: requests under 128 cells straight-line average ~1 ms or less (11% of the time). 97%
of requests explore under 5K nodes. The 3% that explore over 20K nodes take 65% of the time,
and they are not longer trips (~195 cells straight-line, like the rest). They are detours:
107-152 waypoints vs ~25 for a normal route of that length, around lakes and cliffs, where the
straight-line heuristic pulls the search the wrong way and it floods. So the lever, if ever
needed, is a heuristic that knows about detours (landmarks / ALT: exact distances to ~16 fixed
entrances, ~16 MB at 2048) or a coarser level. Not needed for normal play (trips mostly under
~100 cells); revisit if real games show detour stalls.

### Shared goal fields: removed from the game (2026-11-21)

**Removed** (option B below, same day as A): the game-side code (pathfinding.c shared-goal
section, console `sharedGoalFields`, the stats line) is gone; paths are always HPA*/A*. The
library keeps `NavHFlow` (nav/nav.c, checked by the nav fuzz test) in case many movers to one
point ever need it. hubs 512 after removal: 414 arrivals, path time 97 ms (fields-off numbers).
First it was switched off by default; the measurements: Measured with
the hubs scenario (200 movers between 6 hubs, 3600 ticks), fields on -> off:

| map | path time | worst later tick | arrivals |
|---|---|---|---|
| 128 | 808 -> 158 ms | 12.9 -> 2.2 ms | 1159 / 1158 |
| 512 | 5530 -> 92 ms | 348 -> 6 ms | 397 / 414 |
| 2048 | 1618 -> 556 ms | 293 -> 140 ms | 209 / 209 |

Ladder scenarios: identical with and without (HPA* already includes the ladder queue cost).
Why fields lose now: HPA* got cheap (0.1-1.6 ms per request after coarse routes, region checks,
3D chunks); a field's first walk builds chunk fields in a cascade (480-1435 of 1024 on a 512
map, 20-70 ms); the 2 s refresh drops them and repeats it; on 2048 the whole-graph part alone is
70-100 ms and the walk always goes over the 1000-chunk limit, so the field is dropped (retried
per busy goal every 60 s).

Options considered: (A, done) default off, code and toggle kept, scenarios-check runs hubs with
fields on so the path doesn't rot; (B) also remove the game-side plumbing (~150 lines in
pathfinding.c), keep the library's NavHFlow (fuzz-tested); (C) repair the fields (cached incoming
index, refresh only on change, build only along the walked path): large, risky, and little to
gain while a search costs ~1 ms. Revisit only for thousands of movers to one point.

### 5000 movers on a 1024 hill map (2026-11-22)

A 1024 hill save, sandbox, 5000 wandering movers, 0 jobs. Fully zoomed out the game ran at
30-40 fps: drawing ~28 ms per frame plus a sim tick of ~10-13 ms.

**Rendering:** a per-chunk terrain cache (each 64x64 chunk drawn once into a render texture
when zoomed out) cut a zoom-0.1 frame from 28.5 to 13 ms with identical pixels. **Parked** on
branch `terrain-cache-experiment` for its complexity (render textures, staleness rules, raylib
HiDPI workarounds). The notes there (`docs/todo/architecture/terrain-cache-experiment.md` on that
branch) list what is left to finish it. Kept on master: `--screenshot` times frames without the
fps cap and prints the whole frame time and the zoom/viewZ line.

**Sim, `Move` section** (`--headless --load <save> --ticks N --profile`, plus `sample`):
- **Tunnel yielding: done.** `MoverShouldYield` did ~15 walkability lookups per mover per tick
  (is a narrow stretch ahead?) and, at narrow entrances, searched a ~42-cell circle of movers.
  The "nothing narrow ahead" answer is now kept per mover, keyed on its cell, the next two
  waypoints and `walkabilityChangeCount` (bumped by `MarkChunkDirty` / `BuildEntrances`; usable
  by anything else computed from walkability). The neighbour query covers only the stretch's
  cells. Old and new query side by side: 0 different answers. Yield 1.4 -> 0.27 ms, Move 5.7 -> 4.5
  ms (600 ticks).
- **Spawning on cut-off patches: fixed.** Spawns took any walkable cell, so ~12% of the movers
  landed on hilltops nobody can leave and failed a goal search every 1-2 s, forever.
  `GetMainRegionAnchor` (the region most of 256 random walkable cells fall in) and
  `GetRandomWalkableCellReachableFrom` now pick spawn cells (spawnmovers/add movers, the world
  scenario, the hubs anchor). Fresh 1024 world, 5000 movers: cut off 635 -> 0. Move is ~0.3 ms
  higher afterwards: those movers now walk too (the old numbers flattered).
- **Tried and dropped:**
  - Skipping a goal search the regions already call unreachable. No measurable gain: a search
    from a tiny patch fails within a few nodes.
  - Idle movers pausing ~3 s after arriving. Move -5% only (a 256-cell wander trip takes ~55 s),
    and it changes how idle movers behave.
- **Measuring trap:** the first ~600 ticks after a load include the burst where every mover
  plans a path at once. Compare steady state over 3000 ticks: there Move is ~3.8 ms, not 4.5-5.7.

**Steady state, what's left** (3000 ticks, ~9 ms per tick): Move ~3.8 ms, of which ~44% is
wander path planning (~9 HPA* plans per tick at ~0.14 ms, half the abstract search, half
refining the first legs). Then water ~1.6-1.8 ms, trees ~1.2, needs/mood ~1.2, repath ~0.8.
Smaller items inside Move: ground trampling ~0.5 ms, line of sight ~0.45, ladders ~0.2.
None of this is waste any more: making it cheaper means cheaper HPA* plans (library work) or
cheaper water.

**Where this leaves the big-map work:** the stress test (thousands of wandering movers, no jobs)
no longer shows waste. Next step is a target from real play, not a bigger stress test:
measure a survival save with a realistic colony (hundreds of movers doing jobs) on a 1024 map
against "keeps 60 fps", and attack only what that shows.
