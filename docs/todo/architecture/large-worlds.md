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

## Next steps

1. Shared-goal field cascade (above). **Done (2026-11-20):** a walk may build at most 1000
   chunk fields (`NavHFlow.buildBudget`); over it the request uses HPA* (seed 16: 5 ms instead
   of seconds), the field is dropped and the goal isn't promoted for 60 game-seconds.
   Promotion/refresh use game time, so runs are reproducible. Hubs-style goals are unaffected
   (hubs: 970 field hits, 0 too big). **Later:** build fields only along the abstract route
   (nextEnt chain), seeding other exits from abstract dist + a safety margin, so far goals on
   big maps can use shared fields too (right now they are simply too big).
2. Cheap wins: tree tick and item spatial grid not sweeping the whole map.
3. Then per-chunk edges + layers sized to the map (improvements 1 and 2), and re-measure
   with a test build at 1024.
