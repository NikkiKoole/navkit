# Per-chunk edge storage (incremental graph updates on big maps)

> Status: idea (measured, not started)

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
