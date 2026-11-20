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

## First step before building anything

Measure the simulations at 512, and on a test build with the cap raised to 1024. That tells
us whether the simulations or the graph hurt first, and checks the estimates above.
