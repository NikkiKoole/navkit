# Per-chunk edge storage (incremental graph updates on big maps)

> Status: step 1 done (2026-11-20): per-entrance edge lists. Step 2 (stable entrance ids) next.

## Progress

Benchmark: `make bench_nav_update` (512x512x16 hills+water, seed 16: 53K entrances, 1.2M
edges; `./build/bin/bench_nav_update 512 20 hillswater 16 1` for 1 wall per update, the
realistic mining case). It also checks incremental == full build.

Profile before (1 wall, 19.4 ms): finalize (sort/merge/dedupe all edges) 8.4, hash +
index + remap 1.8, edge filter 1.6, entrance rebuild 1.8, link edges 0.7, local edge
searches in affected chunks 4.4 (the only part that doesn't grow with the map).

**Step 1 (done):** each entrance keeps its own out-edge list sorted by target
(`NavWorld.adj`, read with `NavOutEdges`); inserts keep the cheapest copy of a pair, so
there is no global sort/merge/dedupe. Kept entrances carry their lists to their new
index; ladder/ramp link edges are kept unless an end is in an affected chunk.
`NavEdgesFlat` gives the old flat sorted list on demand (debug view, tests).
1 wall: 19.4 -> 13.8 ms; 3 walls: 27.9 -> 21.7 ms. Deep fuzz (2000 worlds) clean.

Left (grows with the map): remapping every kept edge's target (~4.8 ms, the old->new
entrance index shift), rebuilding the entrance hash + chunk index (~1.8 ms), copying and
compacting all entrances (~1.9 ms). Stable entrance ids remove the remap and allow
updating hash/index for touched chunks only.

Also found: `NavBuildGraph` scans all entrances for every chunk (16K chunks x 53K
entrances at 512): most of the ~1 s full build; the chunk entrance index would fix it.

## Problem

The HPA* graph keeps all edges in one flat list sorted by source entrance
(`nav/nav.c`: `edgeList`, `adjStart`). An incremental update (`NavUpdateDirty`: mining,
building) filters, re-maps and merges *every* edge, however small the change.

Measured (2026-11-20, `make path` -O2, 3 random wall placements per update):

| map | edges | incremental update |
|-----|-------|--------------------|
| 256x256x16 hills | 450k | 42 ms |
| 512x512x16 hills | 1.86M | 67 ms |
| 512x512x16 spiral3d | 436k | 43 ms |

On 512x512 maps that is a visible hitch on every frame with terrain changes. Profile:
roughly 35% merging new edges into the sorted list, 40% the keep-unaffected filter
(both over all edges), 10% re-mapping all entrances.

## Idea

Store each chunk's edges separately (they are already computed per chunk region:
`NavGraphEdge.chunk`; ladder/ramp link edges have chunk -1 and could live with their
lower end's chunk). An update then drops and recomputes only the affected chunks'
lists. HPA* and the hierarchical flow need out-edges per entrance across chunk lists
(an entrance lives in up to 4 chunk regions): keep a small per-entrance index of
(chunk, range) or rebuild adjacency only for touched entrances.

Entrance indices also shift on every update (kept entrances are compacted). Stable
entrance ids (free list instead of compaction) would remove the re-mapping pass.

## Guard rails

- `make test_nav_fuzz` (and a deep run: `./build/bin/test_nav_fuzz 2000 1000`) checks
  that the incrementally updated graph equals a fresh full build.
- `docs/research/nav-fuzz-findings.md` ("Scaling to 512x512") has the measurement setup.
