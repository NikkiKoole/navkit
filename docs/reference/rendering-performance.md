# Rendering Performance — Remaining Opportunities

Based on profiling (Instruments, Feb 2026). Numbers are from a session with
active water, no smoke/steam/fire, fully grown trees, moderate map size.

## Current Hot Spots

| Function         | % of frame | Notes                                    |
|------------------|-----------|------------------------------------------|
| DrawCellGrid     | 46.2%     | Dominates rendering                      |
| TickWithDt       | 24.0%     | Simulation (separate concern)            |
| DrawGrassOverlay | 16.4%     | Second biggest draw cost                 |
| DrawWater        | 11.1%     | Only when water is active                |

### Inside DrawCellGrid (46.2% total)

| Sub-cost              | %    | What it does                                          |
|-----------------------|------|-------------------------------------------------------|
| DrawTexturePro        | 21.1%| raylib draw calls — hard floor, can't reduce per-tile |
| FinishOverlayTint     | 5.0% | Extra DrawTexturePro per wall for finish overlay      |
| IsCellVisibleFromAbove| 4.1% | Redundant per-depth-level visibility checks           |
| GetWallSpriteAt       | 1.1% | Sprite lookup per cell                                |
| GetDepthTintDarkened  | 0.3% | Already optimized (pre-baked lookup)                  |

## Optimization Ideas (ranked by expected impact)

### 1. Skip finish overlay on deep levels (saves ~3-4%)

`FinishOverlayTint` adds a second `DrawTexturePro` for every wall — that's
roughly doubling draw calls for walls. On deep levels (z-4 and below) the
finish is barely visible through the brown depth tint. Skipping it for
`depthIndex >= 3` or so would halve the DrawTexturePro calls for deep terrain.

**Complexity:** trivial — one `if` guard  
**Risk:** subtle visual change on deep levels

### 2. Column-first visibility in DrawCellGrid (saves ~3-4%)

`IsCellVisibleFromAbove` is called for every (x,y) at every depth level. But
visibility is monotonic: if z-5 is visible, z-4 through z-2 are guaranteed
visible too. Instead of checking each depth level independently:

- Iterate (x,y) in the outer loop
- Scan downward from z-1, stop at the first blocker
- Draw all visible depth levels for that column

This turns O(8 * depth_check_per_level) into O(1 scan per column). The
tradeoff is restructuring the loop order (currently depth-first, would become
column-first), which touches the core DrawCellGrid structure.

**Complexity:** moderate — loop restructure  
**Risk:** must preserve exact draw order (deeper first for correct overlap)

### 3. DrawGrassOverlay: skip depth levels without natural dirt (saves ~5-10%)

DrawGrassOverlay iterates all visible cells at all 8 depth levels, checking
`IsWallNatural` + `GetWallMaterial == MAT_DIRT` for each. On maps with lots of
stone/rock at depth, most of these checks return false.

Options:
- **Per-z-level dirt flag**: track whether any natural dirt exists at each z.
  Skip entire depth levels that have no dirt. Cheap to maintain (set flag when
  placing dirt, rebuild on load).
- **Combined terrain+grass pass**: merge the grass overlay into DrawCellGrid's
  depth loop so we only iterate the grid once instead of twice. The grass sprite
  would be drawn immediately after the terrain sprite for cells that qualify.

**Complexity:** easy (flag) or moderate (merged pass)  
**Risk:** flag could drift if not maintained at all dirt placement/removal sites

### 4. Reduce DrawTexturePro calls in DrawCellGrid (saves draw call overhead)

Each visible cell generates 1-2 `DrawTexturePro` calls (terrain + optional
finish overlay). raylib batches these internally, but the function call overhead
and vertex submission (rlVertex3f at 1.1% + 1.9%) still adds up.

Ideas:
- **Hybrid instancing**: for large uniform areas (same sprite, same tint),
  batch them manually. Unlikely to help much given the variety of materials.
- **Texture atlas stride**: ensure sprites are arranged to minimize texture
  switches (rlSetTexture at 0.6%). Already using a single atlas, so this is
  probably already optimal.
- **Shader-based approach**: render depth terrain as a single fullscreen quad
  with a shader that samples the grid. Major architectural change, probably
  not worth it for a 2D tile game.

**Complexity:** high  
**Risk:** diminishing returns, raylib already batches well

### 5. Frustum culling for depth levels

DrawCellGrid already has frustum culling for the visible cell range
(`GetVisibleCellRange`). But all 8 depth levels use the same range. If the
camera is zoomed in, deeper levels could potentially use a tighter range (since
they're "further away" they cover less screen space). In practice this probably
doesn't help since we're top-down 2D — all depth levels project to the same
screen area.

**Complexity:** low  
**Risk:** no gain for top-down view

## Already Done (for reference)

- Early exit for DrawSmoke/DrawSteam/DrawFire/DrawWater (active cell counters)
- GetDepthTintDarkened pre-baked lookup (eliminated FloorDarkenTint per tile)
- TreesTick early exit via treeRegenCells counter
- Helper extraction (GetDepthTint, IsCellVisibleFromAbove, FireLevelColor)

### 6. Periodic mover sorting (cache locality)

Reorder mover array by cell every N frames for cache locality. Movers that are
spatially close would be adjacent in memory, improving cache hit rates during
per-mover updates and rendering.

**Complexity:** low  
**Risk:** may cause subtle ordering changes in update/draw

### 7. Y-band sorting for movers (depth sorting)

Hybrid bucket+insertion sort for 2.5D depth sorting. Group movers into
horizontal bands, sort within each band. Reduces sort cost from O(n log n)
to O(n) for mostly-sorted data.

**Complexity:** moderate  
**Risk:** must preserve correct overlap for same-tile movers

## What NOT to optimize

- **DrawTexturePro itself** (21.1%): this is raylib's internal draw function.
  Can't make it faster without modifying raylib or switching renderers.
- **Temperature updates**: already interval-gated (0.1s) with per-tick cap
  (4096 cells) and dual early-exit counters.
- **Chunk-based dirty tracking**: was tried and removed — caused visual
  artifacts at chunk boundaries. Don't re-attempt without a different approach.
