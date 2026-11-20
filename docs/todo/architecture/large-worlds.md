# Large worlds: what bigger maps cost, and what should be achievable

> Status: idea / plan (2026-11-20). Measurements are real; the "achievable" numbers are
> broad-stroke estimates, not promises. Each one says how sure we are.

## Where we are (512x512x16 cap, ~4.2M cells)

**Memory** — every per-cell layer is a static array at the maximum size, whatever map is
loaded. The binary reserves ~570 MB at startup:

- ~250 MB mover tables (10,000 movers x 12.4 KB struct incl. a 1,024-step path, plus
  `moverPaths`). Doesn't grow with the world.
- ~320 MB per-cell layers (water, fire, smoke, steam, temperature, light, rooms, wear,
  farming, snow, vegetation, explored, ...): roughly **75 bytes per cell**.

How we got the number: `size -m build/bin/path` (`__bss` + `__common`).

**Pathfinding** — measured on 512 maps (`docs/research/nav-fuzz-findings.md`, "Scaling to 512x512"):

| | 512 hills (1.86M edges) |
|---|---|
| full HPA* graph build | 2-2.5 s |
| incremental update (mining/building) | ~67 ms, every change |
| long cross-map HPA* path | ~5 ms |

All of it grows with map area: the update and the build touch every edge.

**Simulations** — water, temperature, etc. sweep large parts of their layers on timers.
Cost grows with area. **Not measured at 512 yet.**

## Naive scaling (change nothing, raise the cap)

| map | cells | layer memory | graph update | full build | long path |
|---|---|---|---|---|---|
| 512x512x16 | 4.2M | 320 MB | 67 ms | 2.5 s | 5 ms |
| 1024x1024x16 | 16.8M | 1.3 GB | ~270 ms | ~10 s | ~10 ms |
| 2048x2048x16 | 67M | 5 GB | ~1 s | ~40 s | ~20 ms |

(4x area each row; path time grows with the number of chunks crossed, roughly 2x.)
1024 would stutter on every dig; 2048 isn't playable.

## Improvements, in order

1. **Per-chunk edge storage** — `docs/todo/pathfinding/per-chunk-edge-storage.md`.
   A terrain change only rebuilds the touched chunks' edges, so its cost no longer depends
   on map size.
2. **Size layers to the real map** — allocate at load/generation time instead of static
   max-size arrays. Mostly mechanical (all those `[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH]`
   arrays become pointers with an index macro). The memory cost becomes real cost.
3. **Sparse per-chunk layers** — water, fire, smoke, steam, snow and farming are empty
   almost everywhere. Give a chunk memory for a layer only when something lives there.
   Keep dense only what every cell needs (cell type, flags, materials, light?).
4. **Active-chunk simulation** — simulate chunks that changed recently, plus neighbours;
   sleep the rest. Pairs naturally with 3.
5. **Third pathfinding level** — group chunks into regions (the way HPA* groups cells into
   chunks). Long paths search regions, then chunks, then cells. Shared-goal fields already
   cover many-movers-one-destination.
6. **Smaller items**
   - Coarse mover paths refined as the mover walks: removes the 1,024-step cap (too short
     for big maps) and most of the 123 MB of path storage.
   - Build the full graph per chunk on several threads (chunks are independent), or save it
     with the world so loading doesn't rebuild.
   - Save files: write sparse layers per chunk, skip empty ones.

## What should be achievable (broad-stroke estimates)

**After 1 + 2 (weeks of work, fairly sure):** 1024x1024x16 plays like 512 does today.

| 1024x1024x16 | today (naive) | after 1 + 2 |
|---|---|---|
| graph update per change | ~270 ms | **~1-5 ms** (only touched chunks; fairly sure) |
| memory, per-cell layers | 1.3 GB | 1.3 GB (but a 256 map costs only ~80 MB) |
| full build | ~10 s | ~10 s, **~2 s** if threaded (likely) |
| long path | ~10 ms | ~10 ms (fine for occasional long trips) |

**After 3 + 4 + 5 (a larger rework, less sure):** 2048x2048x16 becomes realistic.

| 2048x2048x16 | naive | after 1-5 |
|---|---|---|
| memory | 5 GB | **~1-1.5 GB** (~15 bytes/cell dense, sparse layers mostly empty; medium confidence) |
| graph update | ~1 s | **~1-5 ms** |
| long path | ~20 ms | **~1-3 ms** with regions (medium confidence) |
| simulation cost | grows with area | grows with **activity** (how much water/fire there is), not size |
| full build / load | ~40 s | **~5 s** threaded, near-instant if saved (medium) |

**Beyond 2048** — 4096x4096x16 is about 270M cells: even at ~15 bytes per cell, the dense
part alone is ~4 GB. That needs chunk streaming (keeping only part of the world in memory)
or fewer bytes per cell. Not planned; listed so the ceiling is clear.

**What doesn't change with map size:** mover count. 10,000 movers is a separate limit
(avoidance, repaths, jobs), measured in the hubs and ladder scenarios, not here.

## Measured: simulation cost per map size (2026-11-20)

Tool: the `world` scenario (HillsSoilsWater terrain, 100 movers wandering to random goals)
plus the headless `--profile` report (CPU ms per tick, averaged over the whole run):

```bash
./build/bin/path --headless --scenario world --set scenarioSize 512 --ticks 1200 --profile
```

| ms per tick (avg) | 128 | 256 | 512 |
|---|---|---|---|
| **whole tick** | **4.7** | **7.2** | **27.8** |
| Trees (`TreesTick`) | 0.5 | 2.1 | **7.9** |
| Grid (mover + item spatial grids) | 0.3 | 1.2 | **6.1** |
| Water | 3.3 | 2.5 | 2.7 |
| Temperature (+ water freezing) | 0.2 | 0.6 | 1.9 |
| Snow | 0.1 | 0.2 | 0.8 |
| Move / avoidance / LOS | 0.1 | 0.3 | 1.0 |
| Repath | ~0 | ~0 | ~0 normally (see spike below) |

At 512: scenario setup (world gen + graph) 2.4 s, full graph rebuild 2.07 s
(73,280 entrances, 1.59M edges). Normal path requests: 240 per run, ~6.4 ms each.

What it says:
- **The tick is already 28 ms at 512 (over a 16.7 ms frame at 60 ticks/s), and it's not
  pathfinding.** The top two are cheap wins that grow with area for no reason:
  - `TreesTick` (sampled as the top function) sweeps the map; tree growth only needs a
    list of growing trees/saplings, or a slow rotating sweep.
  - `BuildItemSpatialGrid` (second in the sample, plus `bzero`): rebuilds/clears a whole-map
    grid every tick with almost no items. Clear only touched cells, or rebuild on change.
  Fixing those two should bring 512 to roughly 12-14 ms per tick (estimate).
  **Done (2026-11-20), better than estimated:** 512 is now **5.1 ms per tick** (was 27.8).
  - Trees: a rotating sweep, one band of rows per tick with the time built up since its
    last turn; bands = cells / 256K (maps up to 64x64x16 stay one band, as before).
    7.9 -> 0.4 ms. Growth lags by at most one sweep (16 ticks at 512).
  - Item grid: kept the sorted item list (same order for the haul scan) but replaced the
    full-map count/prefix-sum arrays with a per-cell first-entry index; a rebuild clears
    only the cells it filled last time and radix-sorts the ground items. 6.1 -> 0.05 ms.

  | ms per tick (avg), after | 128 | 256 | 512 |
  |---|---|---|---|
  | **whole tick** | **2.7** | **2.9** | **5.1** |
  | Trees | 0.4 | 0.4 | 0.4 |
  | Grid | 0.01 | 0.01 | 0.05 |
  | Water | 2.0 | 1.5 | 1.7 |
  | Temperature | 0.1 | 0.5 | 1.6 |
  | Snow | 0.05 | 0.2 | 0.6 |

  Now the biggest area-scaled costs are temperature and snow (active-chunk candidates).
- Water stays ~2.5-3 ms at every size: it already works on active cells only.
  Temperature/freezing and snow grow with area: candidates for active-chunk simulation.

**Found: the multi-second path search at 512 (2026-11-20).** Repro:
`--headless --seed 16 --scenario world --set scenarioSize 512 --ticks 1200` (run alone)
-> `SLOW PATH ... (177,275,8) -> (159,419,10) len=149`. (Headless ignored `--seed` before
this, so every headless run was the same world; now fixed.)
- It's a **shared-goal field** walk, not HPA*: the goal got promoted (3 requests within 10 s),
  building the field took 13 ms, but walking 149 steps built **59,082 chunk fields** (of 65,536
  on the map). Building one chunk's field seeds each exit with the *continuing* chunk's value,
  which builds that chunk's field, and so on: the whole downstream map.
- And each chunk build scanned **all 73K entrances** for its seeds. Fixed: candidates come from
  the per-chunk entrance index (3x3 neighbours), same order, so identical fields:
  4150 ms -> ~700 ms. The cascade itself is now capped (see next steps).
- Intermittent because promotion uses **wall-clock** time (`SHARED_GOAL_WINDOW`): under load
  (parallel runs) the 3 requests fall outside the window and no field is built.
- The ~1.2 s Move max in every 512 run is just the first tick: all 100 movers get a goal at
  once (100 x ~8 ms FindPath, counted under Move because `AssignNewMoverGoal` runs there).

## Measured: 1024x1024x16 test build (2026-11-20)

`make path1024` builds `build/bin/path1024` with the map cap raised to 1024 (static
layers 4x: 1.7 GB reserved). Same `world` scenario, seed 16, 1200 ticks:

| | 512 | 1024 |
|---|---|---|
| ms per tick (avg) | 5.3 | **14.3** (frame budget 16.7) |
| Temperature (+ water freezing) | 1.6 | **5.8** |
| Snow | 0.7 | **2.6** |
| Water | 1.6 | 2.2 |
| Trees | 0.7 | 1.0 |
| Needs | 0.15 | 0.6 |
| peak memory (RSS) | 427 MB | **1.13 GB** |
| scenario setup (world gen + graph) | 0.85 s | 3.7 s (graph build 2.8 s) |
| graph update, 1 wall (`bench_nav_update`) | 5.1 ms | **6.3 ms** (naive estimate was ~270) |
| HPA* query, random pairs | ~4 ms | **~20 ms** avg, worst 226 ms (cross-map, ~1000 steps) |

Graph: 215K entrances, 5.0M edges. Found on the way: `roomGrid`'s extern was hard-coded
`[16][512][512]` (fixed: uses the MAX_GRID_* constants).

What grows with area now:
- Water freezing (`UpdateWaterFreezing`) sweeps the whole map every tick whenever any water
  exists (rivers: always). Probably most of "Temperature".
- Snow sweeps every surface column every 0.04 game-hours, even when it isn't snowing and
  there's no snow.
- Long paths: random cross-map HPA* requests take ~20 ms at 1024 (a third pathfinding level
  is improvement 5); mover paths are capped at 1024 steps (MAX_MOVER_PATH), about one
  crossing of a 1024 map.
- Memory: 1.13 GB resident at 1024; the normal build reserves ~570 MB for any map size
  (improvement 2: size layers to the map).

## Measured: path searches per terrain (2026-11-20)

`./build/bin/bench_nav_update <size> 10 <terrain> 16 1` (1024: same bench built with the
1024 cap). HPA* searches between random walkable cells, grouped by straight-line trip
length; time split into the abstract search (chunk graph) and refinement (cells).

| 1024x1024x16 | entrances | update (1 wall) | short <64 | medium 64-256 | long >256 (worst) | long: steps |
|---|---|---|---|---|---|---|
| hillswater | 215K | 6.1 ms | 0.57 ms | 4.4 ms | **18.9 ms (53)** | 720 |
| hills | 285K | 8.5 ms | 0.79 ms | 3.0 ms | **21.4 ms (57)** | 569 |
| spiral3d | 96K | 8.4 ms | 2.2 ms | 2.3 ms | 2.5 ms (7) | 700 |
| labyrinth3d | 87K | 2.4 ms | 3.4 ms | 4.1 ms | 4.9 ms (10) | 1863 |
| council | 32K | 4.7 ms | 0.25 ms | 0.58 ms | 1.9 ms (4.5) | 580 |
| city | 23K | 1.4 ms | 0.15 ms | 0.32 ms | 1.0 ms (2) | 794 |

At 512 all non-hills terrains (city, castle, dungeon, caves, towers, perlin, council,
spiral3d, labyrinth3d) stay under ~1 ms even for long trips; hills ~6.7 ms long.

What it says:
- **The abstract search is the cost** on long trips over hills (ramps everywhere: 4-9M edges
  at 1024). Refinement is tiny everywhere (<= 0.6 ms). A third pathfinding level (regions)
  is the fix for long hill trips; coarse mover paths would not help search time.
- **Every search scans all entrances twice** to connect start and goal to the graph
  (`nav_ConnectToChunk`): ~0.45 ms of the 0.57 ms for short trips on 1024 hillswater.
  **Fixed (2026-11-20):** candidates come from the chunk entrance index (3x3 chunks,
  same order): short trips on 1024 hillswater 0.57 -> 0.25 ms, same paths.
- **The 1024-step mover path cap bites on maze terrains**: labyrinth3d paths between
  cells < 64 apart average 1438 steps at 1024.
- Full graph build at 1024: up to 4.2 s (spiral3d, hills).

## Next steps

0. Water freezing and snow sweeps: **done (2026-11-20).** Freezing checks one band of rows
   per tick (256K cells; maps up to 64x64x16 stay one band), boiling scaled to match.
   Snow skips its pass while it isn't snowing and no snow or snowfall progress lies
   anywhere (conservative flag, set on SetSnowLevel/init/load). Per tick: 512 5.3 -> 3.1 ms,
   **1024 14.3 -> 6.0 ms** (freezing 5.8 -> 0.1, snow 2.6 -> 0.02).
   Needs (0.6 ms at 1024): every idle mover scanned all items each tick for stray
   reservations; now once a second per mover (staggered): 0.6 -> 0.02 ms. 1024: 5.9 ms/tick.
   **Next:** sizing layers to the map (memory), then long paths at 1024.
1. Shared-goal field cascade (above). **Done (2026-11-20):** a walk may build at most 1000
   chunk fields (`NavHFlow.buildBudget`); over it the request uses HPA* (seed 16: 5 ms instead
   of seconds), the field is dropped and the goal isn't promoted for 60 game-seconds.
   Promotion/refresh use game time, so runs are reproducible. Hubs-style goals are unaffected
   (hubs: 970 field hits, 0 too big). **Later:** build fields only along the abstract route
   (nextEnt chain), seeding other exits from abstract dist + a safety margin, so far goals on
   big maps can use shared fields too (right now they are simply too big).
2. Cheap wins: tree tick and item spatial grid. **Done (2026-11-20)**, see above.
3. Then per-chunk edges + layers sized to the map (improvements 1 and 2), and re-measure
   with a test build at 1024.
