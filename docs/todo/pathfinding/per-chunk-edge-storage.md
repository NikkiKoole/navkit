# Per-chunk edge storage (incremental graph updates on big maps)

> Status: DONE (2026-11-20). Incremental updates no longer scale with the graph: 1 wall
> at 512: 19.4 -> 5.1 ms (128: 4.0, 256: 4.7). See Progress.

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

**Step 2 (done): entrance ids stay put.** Removed entrances' slots are filled from the
end of the list (swap-remove), so only those few entrances change id; references to them
are fixed locally (edges into an entrance come only from entrances in the same chunk
regions, found through the chunk index, or from its ladder/ramp partner, which links back).
The chunk index is kept up to date instead of rebuilt; the position hash and the copy of
all old entrances are gone (a link end in an unaffected chunk finds a leftover entrance
through the chunk index). If the index is incomplete (a chunk region over
NAV_MAX_ENTRANCES_PER_CHUNK, which already warns), the update does a full rebuild.

| 1 wall per update | before | step 1 | step 2 |
|---|---|---|---|
| 128x128x16 (60K edges) | | | 4.0 ms |
| 256x256x16 (280K edges) | | | 4.7 ms |
| 512x512x16 (1.2M edges) | 19.4 ms | 13.8 ms | 5.1 ms |

3 walls at 512: 27.9 -> 21.7 -> 14.3 ms. Deep fuzz (3000 worlds) clean. What's left is
mostly the local searches in affected chunks (one change touches ~14 chunks: the dirty
chunk at z and z+1, their 4 neighbours, up-left, and z-1/z+2); narrowing that set is the
next lever if needed.

Full build: `NavBuildGraph` scanned all entrances for every chunk (16K chunks x 53K
entrances at 512). It now builds the chunk entrance index first and reads from it:
graph build 994 -> 677 ms (entrances: 10 ms). The rest is the local searches themselves
(~41 us per chunk); threads would be the next step if load times matter.

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
