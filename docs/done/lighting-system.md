# Lighting & Darkness System

## Overview

A per-cell lighting system with two light channels: **sky light** (from above)
and **block light** (from placed sources like torches). Light has color and
intensity, propagates through open cells, and is blocked by walls. Rendering
multiplies the cell's existing tint by its light color, so dark areas appear
dark and torch-lit areas get a warm glow.

## Light Model

### Two channels

**Sky light** — comes from above, represents sunlight/moonlight.
- Any cell open to the sky (no solid cells or floors above it all the way to
  the top of the map) receives full sky light.
- Sky light color changes with time of day via the existing
  `GetSkyColorForTime(timeOfDay)` — bright white at noon, orange at sunset,
  deep blue at night.
- Sky light propagates downward and sideways through air. Each cell of
  propagation reduces intensity. This means a shaft to the surface lights a
  room below it, with falloff away from the shaft opening.

**Block light** — comes from placed light sources (torches, furnaces, etc).
- Each source has a color (RGB) and intensity (radius/brightness).
- Propagates in all directions (including up/down through z-levels) through
  non-solid cells. Blocked by walls.
- Multiple colored lights mix additively: red torch + blue crystal = brighter
  purple area. Clamped to 255 per channel.

### Combined light

The final light at a cell = max(sky_light, block_light) per channel. Using max
(not add) prevents outdoor areas from being blown out by nearby torches while
still letting torches dominate in dark areas.

Alternatively: add both but with a soft clamp. Worth experimenting.

### Ambient minimum

A small ambient minimum (e.g. RGB 15,15,20) prevents pitch black — the player
can always see *something*, just very dimly. This is the roguelike convention
and prevents frustration.

## Data Structures

```c
// Per-cell light data
typedef struct {
    uint8_t r, g, b;          // Final combined light color (0-255 per channel)
    uint8_t skyLevel;         // Sky light intensity (0 = fully blocked, 15 = open sky)
} LightCell;

// Light source definition
typedef struct {
    uint8_t r, g, b;          // Light color
    uint8_t intensity;        // Max radius / brightness (e.g. 0-15)
    bool active;
} LightSource;

// Grids
LightCell    lightGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
LightSource  lightSourceGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
```

The `LightCell` is what rendering reads. It's the precomputed result. The
`LightSource` grid stores where torches etc are placed.

### Memory cost

- LightCell: 4 bytes/cell. At 512x512x16 = 16MB. Acceptable.
- LightSource: 5 bytes/cell. Same size. Could optimize with a sparse list
  instead since sources are rare, but grid is simpler and matches existing
  patterns.

Alternative: store LightSource as a sparse list (max 1024 sources). Saves
memory, slightly more complex iteration. Probably worth it since there might
be only 20-50 torches on a map vs millions of cells.

## Sky Light Propagation

### Column-based (fast)

Sky light is inherently top-down. For each (x,y) column:
1. Start at the top z-level with full sky intensity (15).
2. Scan downward. If the cell is air (no wall, no floor), propagate full sky
   level to the cell below.
3. If the cell has a wall or floor, sky light stops for this column.

This gives binary "open to sky" per column. Then do a horizontal BFS spread
from sky-lit cells into adjacent dark cells with attenuation (each step reduces
level by 1). This simulates light spilling into rooms from windows/openings.

### Time of day

The sky light *intensity grid* (0-15) is static — it only changes when terrain
changes. The sky light *color* changes every frame based on `timeOfDay`:

```c
Color skyColor = GetSkyColorForTime(timeOfDay);
// Scale by cell's sky level: skyLevel/15 * skyColor
```

This means the grid only needs recomputation when terrain is modified (dig,
build, place floor), not every frame. The time-of-day color is applied at
render time by multiplying the stored sky level with the current sky color.

## Block Light Propagation

BFS flood fill from each source, similar to the existing water/smoke systems.

For each light source at (sx, sy, sz) with intensity I and color (R,G,B):
1. Start BFS at (sx, sy, sz) with level = I.
2. For each neighbor (orthogonal + up/down): if not solid and not already
   brighter from this source, set level = parentLevel - 1, enqueue.
3. Each cell accumulates: `lightGrid[z][y][x].r += (R * level) / maxIntensity`
   (additive, clamped to 255).

### Diagonal light

Orthogonal neighbors get full propagation. Diagonal could be included at
reduced intensity (like temperature does at 70%) for rounder light shapes.
Start without diagonals, add later if lights look too diamond-shaped.

### When to recompute

- **Full recompute**: on terrain change (dig, build, place/remove floor).
  Clear all light, re-propagate from all sources + sky. This is the simple
  approach.
- **Incremental**: only update affected area when a source is added/removed
  or terrain changes nearby. More complex but better for large maps.

Start with full recompute. If it's too slow (likely for 512x512x16), switch
to incremental. The temperature system's "destabilize neighbors" pattern
could work here.

### Optimization: dirty flag

Track a `lightingDirty` bool. Set it when terrain changes or sources are
added/removed. Only recompute when dirty. Most frames, lighting is static
and costs nothing.

## Rendering Integration

In `DrawCellGrid`, after computing the depth tint, multiply by light:

```c
Color tint = GetDepthTintDarkened(zDepth, z);

// Apply lighting
LightCell* lc = &lightGrid[zDepth][y][x];
Color light = ComputeFinalLight(lc, skyColor);  // combines sky + block
tint = MultiplyColor(tint, light);
```

Where `ComputeFinalLight` takes the stored light cell and current sky color:
```c
Color ComputeFinalLight(LightCell* lc, Color skyColor) {
    // Sky contribution: skyLevel/15 * skyColor
    int sr = (skyColor.r * lc->skyLevel) / 15;
    int sg = (skyColor.g * lc->skyLevel) / 15;
    int sb = (skyColor.b * lc->skyLevel) / 15;

    // Block contribution: stored directly in r,g,b
    int r = sr > lc->r ? sr : lc->r;  // max per channel
    int g = sg > lc->g ? sg : lc->g;
    int b = sb > lc->b ? sb : lc->b;

    // Ambient minimum
    if (r < 15) r = 15;
    if (g < 15) g = 15;
    if (b < 20) b = 20;  // slight blue tint in darkness

    return (Color){ r, g, b, 255 };
}
```

This also applies to movers, items, workshops, etc — everything that currently
uses `GetDepthTint` or `GetDepthTintDarkened` would also get the light
multiplied in.

### Interaction with depth tinting

The brown depth tint and the light tint serve different purposes:
- **Depth tint**: "this is far below you" — visual depth cue
- **Light tint**: "this area is dark/lit" — environmental lighting

They multiply together. A deep, torch-lit cell would be brown (depth) * warm
orange (torch) = darker warm tone. A deep, unlit cell would be brown * near-
black = very dark. This should look natural.

## Torch Placement

### Via draw mode

Add `ACTION_DRAW_TORCH` to the action registry. L-drag to place, R-drag to
remove. Could go under a "Furniture" or "Lighting" parent category, or
directly under draw mode.

Torches could be:
- **Cell flag/overlay**: just a flag on the cell + a sprite overlay. Simplest.
  No item involved, no construction. Like designations.
- **Buildable structure**: requires materials (sticks + charcoal?), placed as
  blueprint, mover builds it. More gameplay depth.
- **Item on wall**: placed against a wall cell, occupies that cell. Would need
  a new cell type or cell flag.

Recommendation: start with **cell flag** approach for fast iteration. A torch
is just `lightSourceGrid[z][y][x] = { 255, 200, 100, 8, true }` and a sprite
overlay. Can add construction requirements later.

### Via sandbox mode

Also add `ACTION_SANDBOX_LIGHT` for testing. L-drag adds light source, R-drag
removes. Shift could toggle color (white/warm/cool/red/green/blue).

### Torch sprite

Draw a small torch sprite on walls that have a light source. The existing
finish overlay pattern works — draw the torch sprite on top of the wall tile.

## Light Source Types

Start with just one (warm torch), expand later:

| Source     | Color        | Intensity | Notes                      |
|------------|-------------|-----------|----------------------------|
| Torch      | 255,200,100 | 8         | Warm yellow, medium radius |
| Furnace    | 255,150,50  | 5         | Already exists as workshop |
| Campfire   | 255,180,80  | 10        | From fire system?          |
| Crystal    | 100,150,255 | 6         | Cool blue, future          |
| Skylight   | per time    | 15        | Open to sky                |

Furnaces/kilns could automatically register as light sources when active
(passiveProgress > 0 or has fuel). Fire cells already exist — they could
automatically emit light at their fire level.

## Integration with Existing Systems

### Fire = light

Active fire cells (`fireGrid[z][y][x].level > 0`) automatically emit warm
light proportional to their fire level. No extra placement needed. A burning
tree lights up the area.

### Temperature connection

Torches could optionally be heat sources too (they're fire). This is a nice
emergent interaction — torches warm rooms. Already have SetHeatSource().

### Workshop glow

Active workshops (kilns, furnaces) could emit dim light when working. Check
`ws->passiveProgress > 0` or `ws->assignedCrafter >= 0`.

## Implementation Order

1. ~~**Data structures**: LightCell grid, light source grid (or sparse list)~~ DONE
2. ~~**Sky light column scan**: precompute skyLevel per cell~~ DONE
3. ~~**Sky light horizontal spread**: BFS from sky-lit cells~~ DONE
4. ~~**Rendering integration**: multiply light into cell tint~~ DONE
5. ~~**Sandbox action**: ACTION_SANDBOX_LIGHT for testing~~ DONE
6. ~~**Block light propagation**: BFS from sources~~ DONE
7. ~~**Colored light mixing**: additive RGB from multiple sources~~ DONE
8. ~~**Torch placement**: visible sprite + 5 color presets (1-5 keys)~~ DONE
9. ~~**Fire auto-light**: fire cells emit proportional light~~ DONE
10. ~~**Dirty tracking**: only recompute when terrain/sources change~~ DONE

Steps 1-5 give a playable day/night cycle with underground darkness.
Steps 6-8 add player-placed lighting.
Steps 9-10 add polish and performance.

### What's implemented (as of 2026-02-13)

**Sky light**: Column scan + horizontal BFS spread. Time-of-day color applied
at render time. Ambient minimum (15,15,20) prevents pitch black.

**Block light**: Euclidean distance falloff, 4-cardinal BFS on same z-level.
Writes to solid surfaces but doesn't propagate through them. Visited grid
prevents duplicate BFS accumulation.

**Color mixing**: Additive per-source (red + blue = purple). GetLightColor
uses dominant-brightness selection (not per-channel max) to preserve torch
color identity against sky light.

**Torch presets** (1-5 keys in sandbox light mode):
1. Warm Torch (255,180,100) — default
2. Cool Crystal (100,150,255)
3. Fire (255,100,50)
4. Green Torch (100,255,100)
5. White Lantern (255,255,255)

**Rendering**: Torch sprite (middle-dot, 60% cell size, color-tinted) drawn
at each light source position. GetLightColor called 20+ places in rendering.c
for walls, floors, items, movers, workshops, etc.

**Z-1 bleed**: Air cells get 50% of block light from the level below, so
torches placed underground glow upward through openings.

**Save/load**: Save version 37. Light sources serialized in ENTITIES section.

**Fire auto-light**: All fire level changes go through `SetFireLevel()`, which
syncs to `AddLightSource`/`RemoveLightSource`. Color scales warm with level:
R=255, G=140+level*10, B=30+level*5, intensity=2+level. `SyncFireLighting()`
rebuilds all fire lights on save load.

**Tests**: 48 tests, 5245 assertions in tests/test_lighting.c.

## Open Questions

- **Does light affect gameplay?** Movers slower in dark? Enemies spawn in
  darkness? Plants need light? Or purely cosmetic for now?
- **How dark is dark?** The ambient minimum controls this. Too dark = annoying,
  too bright = no point. Needs playtesting.
- **Light through floors?** Constructed floors (HAS_FLOOR) could block light
  or transmit partial light (like frosted glass). Natural ground definitely
  blocks.
- **Light through ramps/ladders?** These are non-solid, so yes — light should
  pass through them. A ramp shaft to the surface would bring down sky light.
- **Colored sky light?** Sunset orange sky light tinting underground areas
  that are open to sky would look beautiful but might be confusing. Worth
  trying.
- **Performance budget?** Full recompute of 512x512x16 = 4M cells BFS could
  be expensive. May need to limit propagation radius or use incremental
  updates from the start.
