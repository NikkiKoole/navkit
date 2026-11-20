# Nav library fuzzing: what it found (2026-11-19)

> Status: findings + fixes. Test: `tests/test_nav_fuzz.c` (`make test_nav_fuzz`, part of `make test`).

After the terrain sweep (`bench_nav_terrains`) found hierarchical-flow loops, a randomized
adversarial test was written to look for related problems. It generates small worlds aimed at
the awkward cases and checks every algorithm against ground truth.

**The worlds:**
- walls and gaps on chunk borders and corners
- mazes, serpentines, rooms
- "terraces": several levels joined by dozens of ramps
- ladders and one-way ramp entries on borders and corners
- mud
- 4- and 8-direction movement
- chunk sizes that don't divide the map

**What it checks:**
- **A\*:** cost equals the exact flow-field optimum; every step is legal.
- **Flow fields:** exact.
- **Hierarchical flow, from every cell:**
  - it reaches the goal exactly when the goal is reachable;
  - it never loops;
  - every step is legal;
  - optimal ≤ walked cost ≤ the cost the field promised.
- **HPA\*:** reaches the goal exactly when A* does; legal steps; cost at least optimal.
- **Regions:** never "unreachable" when a path exists.
- **Nearest by walking distance:** exact.
- **After random edits:** incremental updates must give the same graph as a fresh full build.

Every failure prints its seed. `test_nav_fuzz 1 <seed> -v` reproduces it, and environment
switches dump maps, edges and edits (see the file header).

## Bugs found (all fixed)

| # | Bug | Effect | Fix |
|---|-----|--------|-----|
| 1 | HPA* edges were stored both ways with one cost, but ramps can only be entered from their low or high side | HPA* "found" paths A* says don't exist, or produced illegal jumps; hierarchical-flow cells had no route | Directed edges. Reverse-direction connection searches for goals. The hierarchical Dijkstra follows incoming edges. |
| 2 | Point entrances (ladder/ramp cells) in a chunk's first row/column were invisible to the chunk above/left, whose search area includes that row | Ramps reachable only from the neighbour's side had no route | Every chunk lists every entrance inside its search area (`nav_EdgeChunksOfCell`). The same rule applies to dirty marking and hierarchical seeds. |
| 3 | A border run of open cells was represented by its middle cell, even when that cell was a ramp | The ordinary crossing next to the ramp vanished from the graph | Runs split at restricted cells; a ramp on a border gets its own entrance. |
| 4 | 3D A* heuristic charged a full cell cost per level on top of horizontal distance; a ramp moves across *and* up for 1.4× cell cost | Inadmissible heuristic: A* returned slightly longer paths (in the game too) | 0.4× minimum cell cost per level. |
| 5 | `NavFindPathHPA` returned "no path" when the world had no entrances, before trying the same-chunk search | One-chunk maps, or chunks walled off from each other, had no paths at all | Local search first. |
| 6 | Hierarchical-flow fixed point capped at 64 sweeps | Long serpentines left some cells without a route | Dependency worklist: only fields whose inputs changed are rebuilt. |
| 7 | Incremental entrance update: the affected-chunk closure ran once in z order, then link loops marked more chunks affected after entrances were already kept | Duplicate ladder/ramp entrances | Stale link ends found before the keep step; kept link ends reused by position. |
| 8 | Old→new entrance mapping keyed on position + "is ladder" | Two entrances on one chunk-corner cell (or the middle of a 3-level ladder) were confused; edges attached to the wrong one | Key on position + both chunks; duplicates mapped in order. |
| 9 | Ladder/ramp link edges kept across updates | Edges of links that no longer exist survived | Link edges always re-added from the rescanned links. |
| 10 | Per-entrance edge cap (64) and per-chunk entrance cap (64 incremental vs 128 full) | Edges silently dropped. 256×256 hills was already cut off at the old 262k total-edge cap; it really has 449k edges. | No per-entrance cap (edges sorted by source, contiguous ranges, list grows as needed). Per-chunk cap 128 for both builds, with a warning. |
| 11 | Two existing tests passed by accident: a mover "blocked by a wall" only because HPA* had no path (bug 5) | — | The test now checks the mover never enters the wall cell. |

The test was checked against itself: re-introducing bug 4, or the old "is the route inside
this chunk?" rule, makes the default 60-world run fail.

## Performance (256×256×16, chunk 16; `make path` -O2)

| map | full graph build | incremental update (3 cell changes) | HPA* query |
|-----|------------------|--------------------------------------|------------|
| labyrinth3d | 241 → 130 ms | 20 → 19 ms | 0.69 → 0.55 ms |
| spiral3d | 323 → 389 ms | 34 → 39 ms | 0.91 → 0.77 ms |
| council | 353 → 179 ms | 25 → 23 ms | 0.65 → 0.56 ms |
| hills* | 1444 → 498 ms | 134 → 60 ms | 0.22 → 2.55 ms |

\* Before the fixes, hills hit the entrance and edge caps: its graph was incomplete, so the old
numbers aren't comparable. With all 449k edges present, HPA* queries there are slower; dense
ramps give entrances many edges.

**Why it is not slower overall:**
- Full builds run one Dijkstra per entrance instead of A* per pair.
- A pair whose search path can be walked backwards gets the reverse edge mirrored (HPA*'s usual
  cost approximation). Only pairs that cross a ramp the wrong way need a second search.
- Incremental updates no longer sort every edge, and they reuse kept ladder/ramp entrances
  instead of rebuilding both ends of every link they touch.

A* explores 1–4% more cells with the corrected (weaker but admissible) heuristic.

## Not addressed

- 256×256×16 hills has ~14.5k entrances, close to `NAV_MAX_ENTRANCES` (16384). Bigger
  ramp-heavy maps will hit it (a warning is logged).
- The full and incremental builds still scan the whole world for ladders and ramps (~13 ms on
  256×256×16). That's most of an incremental update on small changes.
