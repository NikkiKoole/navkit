# A* Priority Queue + Pathfinder Extraction Readiness

> Status: spec

Two related findings from a code review session on 2026-08-01. Both are about making
`src/world/pathfinding.c` library-grade. They're filed together because the A* fix is a
prerequisite for extraction, and both were measured in the same sitting.

**Nothing here is implemented.** This doc exists so a fresh session has the full picture
without re-deriving it.

---

## Part 1 — `RunAStar()` has no priority queue

### The bug

`RunAStar()` (`src/world/pathfinding.c:1616`) finds the next node to expand by linear-scanning
the **entire** `nodeData` array on every single pop:

```c
// pathfinding.c:1648-1656
for (int z = 0; z < gridDepth; z++)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if (nodeData[z][y][x].open && nodeData[z][y][x].f < bestF) { ... }
```

It also clears the full `nodeData` array on entry (`:1623-1626`) — at max dims that's
16 × 512 × 512 × 24 bytes = **100 MB memset per call**.

Total cost is `pops × (W × H × D)`. Doubling the map edge multiplies runtime ~16x.

A working binary heap already exists in the same file (`HeapPush`/`HeapPop`, `:227`) and is
used by `AStarChunk` and HPA*'s abstract search. `RunAStar` just never got converted.

Aggravating factor: **`gridDepth` is hardcoded to 16 for every world** regardless of what the
caller asks for — `grid.c:47`, `gridDepth = MAX_GRID_DEPTH; // Always use max depth for now`.
So a 32×32 grid scans 16 z-levels, most of them empty air. That's a flat 16x multiplier on
all of the above and is independently worth fixing.

### Measured impact

Reproduce with **`make bench_astar_heap`** (`tests/bench_astar_heap.c`).

A/B harness with three variants over identical grids. **All three produced identical goal
g-costs at every size** (`406/406/406`, `19484/19484/19484`, …) — same search, different data
structure. The bench asserts this and prints `*** MISMATCH ***` if a variant diverges.

- **V0** — current `RunAStar()`: linear scan + full clear
- **V1** — binary heap, still full clear (isolates the priority-queue win)
- **V2** — heap + generation stamps (also removes the clear)

```
                                V0 (current)      V1 (heap)      V2 (heap+gen)
open        32x32    pops=400        4.485 ms       0.087 ms         0.078 ms
open        64x64    pops=1680      76.767 ms       0.463 ms         0.344 ms
open       128x128   pops=6872    2017.753 ms       2.730 ms         1.517 ms
serpentine  64x64    pops=2968     124.378 ms       0.500 ms         0.474 ms
serpentine 128x128   pops=13088   3824.882 ms       2.303 ms         2.036 ms

extrapolated (V0 too slow to run):
open       256x256   pops=27773    ~31,700 ms       8.135 ms         6.736 ms
open       512x512   pops=111728  ~510,000 ms      37.512 ms        30.164 ms
```

**In game terms:** the UI offers world presets up to 256×256 (`ui_panels.c:986`). At that size
a single A* path currently costs **~32 seconds**. At the 128×128 preset, ~2 seconds. Per path.

The heap is the bulk of the win, but the clear is not negligible — on open 128×128, V1→V2 is
another **1.8x** on top.

### Why it hasn't bitten yet

A* is **not** on the hot path. `moverPathAlgorithm` defaults to HPA* (`mover.c:68`), and HPA*'s
internals already use the heap. This is latent cost, not burning frames today.

### What fixing it buys

- **Unblocks the disabled correctness fallback.** `mover.c:1733` has the HPA*→A* fallback
  commented out, with the note *"burning 6-14s on large grids confirming unreachable paths."*
  That is this bug. With a heap the fallback costs single-digit ms and HPA*'s safety net
  comes back. See `test_pathfinding.c` `hpa_fallback`.
- Makes the A* option in the algorithm switcher actually usable rather than a soft hang.
- Restores a trustworthy reference implementation — several `test_pathfinding.c` cases
  cross-validate HPA*/JPS output against A*.
- Prerequisite for Part 2.

### Implementation notes

Three independent changes, roughly half a day total:

1. **Binary heap** — reuse the existing `HeapPush`/`HeapPop` at `:227`, or an indexed heap
   keyed on `NodeId(x,y,z) = (z*gridHeight + y)*gridWidth + x` with a `heapIdx[]` map for
   decrease-key. Expansion logic (4/8-dir, ladders, ramp up, ramp down) is unchanged —
   this is largely transcription.
2. **Generation stamps** — add `uint32_t gen[]` + a `curGen` counter. On touch, if
   `gen[id] != curGen`, treat the node as fresh and initialize it. Removes the clear entirely.
3. **Stop forcing `gridDepth = 16`** (`grid.c:47`) — separate, cheap, benefits everything that
   iterates z.

**A complete, validated reference implementation of steps 1 and 2 already exists** in
`tests/bench_astar_heap.c` as `HeapAStar()` (~150 lines, `useGen` toggles between V1 and V2
behaviour). It mirrors `RunAStar`'s expansion exactly — 4/8-dir with corner-cut checks,
`CanEnterRampFromSide`, ladders, ramp up, ramp down — and produces identical costs. Porting it
into `pathfinding.c` is most of the job.

### Also worth doing

`tests/bench_pathfinding.c` **never benchmarks `RunAStar`** — it only measures `AStarChunk`,
which already uses the heap. That's why this never surfaced. Once fixed, fold a `RunAStar` case
into `bench_pathfinding.c` and add `bench_astar_heap` to the aggregate `bench:` target (it's
excluded today only because V0 is slow enough to dominate the run).

---

## Part 2 — Extraction into another project

Question asked: how hard would it be to lift this pathfinder into a different project?

**Answer: easier than the 3,700-line file suggests, for A\* and HPA\*.** Verified, not guessed —
`pathfinding.c` compiles cleanly as a standalone translation unit.

### Actual dependency surface

The entire external symbol set is 32 entries:

```
raylib (4):   GetTime, TraceLog, GetRandomValue, SetRandomSeed
libc (5):     memcpy, memset, abs, stack guards
grid shape:   gridWidth/Height/Depth, chunkWidth/Height, chunksX/Y
grid data:    grid, cellFlags
dirty flags:  needsRebuild, hpaNeedsRebuild, jpsNeedsRebuild
game leakage: cellDefs, wallMaterial, wallNatural, waterGrid,
              furnitureMoveCostGrid, exploredGrid, gameMode,
              GetWaterLevel, GetSnowLevel, rampCount, InvalidateLighting
```

Reproduce with:
```bash
cc -c src/world/pathfinding.c -o /tmp/pf.o -I. -Ivendor -include src/game_state.h
nm -u /tmp/pf.o
```

The raylib coupling is 4 functions — a `#define` away from `clock_gettime` + `printf` + `rand`.
Not the problem.

### What actually binds it

Only **two `static inline` functions in `cell_defs.h`** drag in the whole game:

- `IsCellWalkableAt()` — `cell_defs.h:89`
- `GetCellMoveCost()` — `cell_defs.h:366`

Everything in the "game leakage" list arrives through those two, because they're inline in a
header — fog of war, water depth, snow, furniture, mud, doors, ladders, ramps all get inlined
straight into the pathfinder.

Someone already anticipated this. `cell_defs.h:133` carries the header comment
*"Pathfinder-agnostic helpers (allow pathfinder extraction without DF knowledge)"*. The seam is
half-cut already.

**Extraction = replace ~7 inlines with a host-supplied callback header:**
`IsCellWalkableAt`, `GetCellMoveCost`, `CanClimbUpAt`, `CanClimbDownAt`, `CanWalkUpRampAt`,
`GetRampHighSideOffset`, `CanEnterRampFromSide`.

One genuine layering violation to delete: `MarkChunkDirty()` calls `InvalidateLighting()` at
`pathfinding.c:471`. The pathfinder should not know lighting exists.

### The real obstacle: static memory

```
__bss:     183 MB
__common:  628 MB   →  811 MB static data segment
```

Everything is fixed-size file-scope arrays sized to compile-time maxima (512×512×16):

| Table | Size |
|---|---|
| `nodeData[16][512][512]` (A*) | ~100 MB |
| `jpsDist[16][512][512][8]` (JPS+) | ~67 MB |
| `jpsLadderGraph` (2048² all-pairs ×2) | ~46 MB |
| `adjList[16384][64]` | ~4 MB |

For a library this is a non-starter. Wants a context struct with heap allocation sized to the
actual world. That's also what makes it non-reentrant today: single implicit world, globals
throughout, not thread-safe, can't run two searches concurrently.

### Recommendation

**Take A\* and HPA\*. Leave JPS and JPS+ behind.** They're ~700 lines, own ~113 MB of the static
footprint, and **never call `GetCellMoveCost`** — so with variable-cost terrain live they
produce wrong-cost paths. The file's own header comment (`pathfinding.c:30`) already says they
become useless once variable cost lands. That's now.

Rough effort:

| Step | Effort | Notes |
|---|---|---|
| Callback seam (~7 functions) | ~1 day | Worth doing in-place regardless — improves this codebase too |
| Drop JPS/JPS+ | hours | Mostly deletion |
| Context struct + heap allocation | 2–3 days | The actual work |
| Fix A* priority queue (Part 1) | ~0.5 day | Would not want to ship the current version |

The ~4,800-line `test_pathfinding.c` comes along nearly free — it already tests through the
public API.

---

## Stale comment to fix while in here

`pathfinding.c:37-45` lists variable-cost terrain as a TODO. **It's implemented** —
`GetCellMoveCost()` in `cell_defs.h:366` is the single source of truth, shared by pathfinding
and the movement layer (`speed = 10.0f / cost`), covering water depth, mud, snow, furniture,
bush, and constructed-floor bonus. What's still true is the *consequence* the comment
describes: JPS/JPS+ ignore it.

---

## Related

- `pathfinding/future-ideas.md` — flow fields, variable cost notes
- `architecture/decoupled-simulation-plan.md` — decoupling sim from rendering (same spirit)
