# Rendering Optimization Candidates

Date: 2026-02-16
Status: Reference — no active bottleneck, general analysis for when needed

---

## Context

NavKit renders a 3D grid (up to 512x512x16) using raylib with a single 8x8 sprite atlas. The renderer uses a painter's algorithm with per-z-level passes. Each simulation overlay (grass, mud, snow, water, fire, smoke) is a separate full-screen pass. Viewport culling reduces the grid to ~60x34 visible cells (~98% skip), but the remaining work is still substantial due to redundant computation across passes.

Current render loop order (main.c:1231+):
```
DrawCellGrid()          — terrain walls/floors (9 depth levels)
DrawGrassOverlay()      — vegetation sprites
DrawPlantOverlay()      — saplings/trees
DrawCloudShadows()
DrawMud()               — brown overlay on muddy cells
DrawSnow()              — snow accumulation
DrawWater()             — water levels
DrawFrozenWater()
DrawFire()              — fire particles
DrawSmoke()             — smoke overlay
DrawSteam()             — steam overlay
DrawTemperature()       — debug heat overlay
DrawStockpileTiles()    — stockpile zone markers
DrawWorkshops()         — workshop footprints
DrawStockpileItems()    — items sitting in stockpiles
DrawItems()             — ground items
DrawMovers()            — colonists
DrawAnimals()           — animals
DrawJobLines()          — debug job paths
DrawMist()              — fog overlay
```

---

## Per-Frame Iteration Counts (estimated, with culling enabled)

| Category | Iterations/frame | Notes |
|----------|-----------------|-------|
| Grid cell iterations (all passes) | ~140,000 | ~20k cells × 6-7 overlay passes |
| `IsCellVisibleFromAbove()` calls | ~330,000 | O(depth) loop, repeated per overlay |
| Color calculations (MultiplyColor) | ~60,000 | GetLightColor + tint per cell per pass |
| Draw calls (DrawTexturePro etc.) | 35,000-55,000 | All sprites + rectangles |
| Entity iterations | ~25,100 | Items (25k hwm) + movers (~100) |

---

## Tier 1: High ROI

### Cache Visibility Bitmap

**Problem:** `IsCellVisibleFromAbove()` (rendering.c:301-308) checks whether a cell is occluded by solid cells above it. It loops upward from the cell's z to the view z — up to 9 iterations. This function is called independently by every overlay pass for the same cells.

```c
// Called ~330,000 times per frame across all overlay functions
static bool IsCellVisibleFromAbove(int x, int y, int cellZ, int viewZ) {
    for (int zCheck = cellZ; zCheck < viewZ; zCheck++) {
        if (CellIsSolid(grid[zCheck][y][x]) || HAS_FLOOR(x, y, zCheck)) {
            return false;
        }
    }
    return true;
}
```

Called from: DrawDeeperLevelCells, DrawGrassOverlay, DrawMud, DrawSnow, DrawWater, DrawFire, DrawSmoke, DrawSteam.

**Fix:** Compute a `bool visibleFromAbove[MAX_VISIBLE_DEPTH][maxY][maxX]` bitmap once at the start of each frame (or when the view z changes). All overlay passes read from the bitmap instead of recomputing.

**Cost:** One extra pass over visible cells (~20k iterations, trivial).
**Savings:** Eliminates ~310k redundant z-column checks per frame.

---

### Pre-Compute Cell Tints

**Problem:** Every cell in every overlay pass computes the same lighting and tint:

```c
Color lightTint = GetLightColor(x, y, z, skyColor);   // grid lookup + color math
Color tint = MultiplyColor(depthTint, lightTint);      // 4 muls + 4 divs
tint = MultiplyColor(tint, MaterialTint(mat));          // 4 muls + 4 divs
```

`MultiplyColor` alone is 8 multiplications + 8 divisions per call, called 2-3 times per cell per overlay. The light value and depth tint don't change between overlays for the same cell.

**Fix:** Pre-compute `cellTint[z][y][x]` for all visible cells once per frame. Overlay passes look up the pre-computed tint instead of recalculating.

Material tint varies per cell type, so the pre-computed value would be `lightTint × depthTint`. Each overlay then does one `MultiplyColor` with its material tint instead of two or three.

**Cost:** ~20k tint calculations per frame (once).
**Savings:** Eliminates ~40-60k redundant color multiplications per frame.

---

### Add Frustum Culling to DrawItems()

**Problem:** `DrawItems()` iterates up to `itemHighWaterMark` (max 25,000) with only a z-depth range check. No screen-space frustum culling.

```c
// rendering.c:1695
for (int i = 0; i < itemHighWaterMark; i++) {
    if (!items[i].active) continue;
    if (items[i].state == ITEM_CARRIED) continue;
    if (items[i].state == ITEM_IN_STOCKPILE) continue;
    // depth check, then draw
}
```

Movers already have frustum culling (rendering.c:1315-1319) — items don't.

**Fix options:**
1. Add the same screen-margin frustum cull that movers use (simple, small win).
2. Use the already-built item spatial grid to only iterate items in visible cells (bigger win, avoids touching inactive item slots entirely).

**Savings:** At 5k+ items, skipping off-screen items avoids thousands of unnecessary active/state checks.

---

## Tier 2: Moderate ROI

### Merge Ground Surface Overlay Passes

**Status:** Not planned yet — research/brainstorm only. Lower priority than visibility cache and tint pre-computation (which reduce the cost of separate passes enough that merging becomes less urgent).

**Problem:** Each overlay (grass, mud, snow, water, fire, smoke) is a separate function that independently iterates all visible cells with the same loop structure:

```c
for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++)
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++)
            if (!IsCellVisibleFromAbove(...)) continue;
            // check if this overlay applies, draw
```

Six passes × ~20k cells = ~120k iterations, most of which early-exit because the overlay doesn't apply to that cell.

**Draw order analysis:** The overlays layer on top of each other at the same (x,y,z) — grass under mud under snow. A merged loop drawing them in the same order per-cell produces identical visual output, because the painter's algorithm operates within a single cell's stack, not across cells. Z-ordering between cells is preserved by the outer z back-to-front loop.

**What to merge (and what not to):**

Only the **ground surface overlays** are good merge candidates: grass, mud, snow. They share:
- Same preconditions (checking the wall/floor below the current z)
- Same z-range (deeper levels + current z-1)
- Same visibility check
- Conceptually "what's on the ground surface"

Keep **water, fire, smoke, steam as separate passes**. They have:
- Different early-exit conditions (`waterActiveCells == 0`, `fireActiveCells == 0`)
- Different z-range logic (water draws at current z too, fire has burn marks)
- More complex per-cell logic (water sources/drains, fire animation)

**What a merged ground overlay would look like:**

```c
static void DrawGroundOverlays(void) {
    // ... setup, GetVisibleCellRange ...

    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++) {
        Color depthTint = GetDepthTint(zDepth, z);
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (!visibilityBitmap[zDepth][y][x]) continue;  // cached
                Color tint = precomputedTint[zDepth][y][x];      // cached
                Rectangle dest = { ... };                         // computed once

                // --- grass (bottom layer) ---
                if (IsWallNatural(x,y,zDepth) && GetWallMaterial(x,y,zDepth) == MAT_DIRT) {
                    VegetationType veg = GetVegetation(x, y, zDepth);
                    // ... draw grass sprite with tint
                }
                // --- mud (middle layer) ---
                if (IsMuddy(x, y, zDepth)) {
                    // ... draw mud rect with tint
                }
                // --- snow (top layer) ---
                uint8_t snow = GetSnowLevel(x, y, zDepth);
                if (snow > 0) {
                    // ... draw snow rect with tint
                }
            }
        }
    }
    // ... same for current z-1 (surface level) ...
}
```

**What this saves (after visibility cache + tint pre-computation are already in place):**
- 2 fewer full-screen iteration passes (~40k fewer iterations)
- 2 fewer bitmap lookups per cell (minor)
- `dest` rectangle computed once instead of 3 times
- Better cache locality — `grid[z][y][x]` fetched once, stays hot for all 3 checks

**What this costs:**
- Merges 3 clean self-contained functions (~60 lines each) into one ~120 line function
- Harder to toggle individual overlays for debugging
- Every new ground overlay means editing the merged function instead of adding a new one

**Verdict:** The win is real but modest — especially after visibility cache and tint pre-computation eliminate the expensive redundant work. The current separate functions are clean and easy to maintain. We'd only pursue this if profiling shows the remaining loop overhead still matters, or if we end up with many more ground overlays.

---

### Sprite Lookup Tables

**Problem:** `ItemSpriteForTypeMaterial()` (rendering.c:162-221) uses nested if/switch chains to map (ItemType, MaterialType) to a sprite index. Called per drawn item.

```c
if (type == ITEM_LOG) {
    switch ((MaterialType)material) {
        case MAT_PINE: return SPRITE_tree_trunk_pine;
        case MAT_BIRCH: return SPRITE_tree_trunk_birch;
        // ...
    }
}
if (type == ITEM_PLANKS) { ... }
```

**Fix:** Replace with a 2D lookup table `int itemSprite[ITEM_TYPE_COUNT][MAT_COUNT]`, initialized once at startup. Lookup becomes a single array access.

The existing `spriteOverrides[CellType][MaterialType]` pattern already does this for cells — items should follow the same approach.

**Savings:** Eliminates branch mispredictions in tight draw loops. Modest per-call improvement but adds up over thousands of items.

---

### Work Animation Trig

**Problem:** `CalcWorkAnimOffset()` (rendering.c:1217-1269) calls `sinf()` and `sqrtf()` per visible working mover.

```c
float wave = sinf(m->workAnimPhase * WORK_SWAY_FREQ * 2.0f * PI);
float dist = sqrtf(ddx * ddx + ddy * ddy);
```

**Fix:** Use a lookup table for sin (256 entries covers smooth animation) and fast inverse sqrt or skip the normalize entirely (direction only needs to be approximate for a visual sway).

**Impact:** Low at current mover counts (~100-500 visible), but relevant at 10k scale.

---

## What's Already Good

These existing optimizations are solid and should be preserved:

- **Viewport culling** — `GetVisibleCellRange()` reduces 512x512 to ~60x34 (98% skip)
- **Single texture atlas** — no texture state changes between draws
- **Mover/animal frustum culling** — screen-margin check skips off-screen entities
- **`itemHighWaterMark`** — avoids iterating all 25k item slots
- **Painter's algorithm** — z-back-to-front ordering avoids sorting
- **Depth limit** — `MAX_VISIBLE_DEPTH = 9` caps vertical iteration
- **Staggered mover updates** — LOS/avoidance spread across 3 frames (not rendering, but reduces data staleness)

---

## Recommendation Summary

| Priority | What | Approach | Effort | Gain |
|----------|------|----------|--------|------|
| 1 | Visibility bitmap cache | Compute once, reuse across overlays | Small | Eliminates ~310k z-checks/frame |
| 2 | Pre-compute cell tints | Light × depth once, reuse | Small | Eliminates ~50k color muls/frame |
| 3 | Item frustum culling | Screen-space check or use spatial grid | Small | Skips thousands of inactive checks |
| 4 | Merge overlay passes | Single cell iteration for ground overlays | Moderate | ~60-80k fewer iterations/frame |
| 5 | Sprite lookup tables | Array replaces if/switch | Small | Cleaner + fewer branch misses |
| 6 | Trig lookup for animations | Sin table, fast sqrt | Small | Minor, scales with mover count |
