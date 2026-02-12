# Terrain Sculpting Brushes

Date: 2026-02-11
Status: Design proposal - general terrain editing tools

---

## Concept

**Freehand terrain editing** with adjustable brush size for raising/lowering ground. More flexible than specific "dig channel" or "place water" tools.

**Key Features:**
- **Lower Terrain Brush** - carve channels, dig moats, create valleys
- **Raise Terrain Brush** - build hills, dams, embankments
- **Adjustable brush diameter** - 1x1, 3x3, 5x5, 7x7 radius
- **Freehand drawing** - paint by dragging mouse (like drawing walls/floors)

**Unlocks:**
- River/channel carving (for water-dependent workshops)
- Defensive moats
- Terrain leveling for construction
- Dams and water control
- Terraced hillsides
- Aesthetic landscaping

---

## Brush Design

### Lower Terrain Brush

**Effect:** Removes top z-level of terrain in brush area

**Behavior:**
- Click & drag to carve terrain
- Each cell in brush radius drops 1 z-level
- Natural walls (dirt, clay, rock) → air
- Constructed walls/floors remain unchanged (safety)
- Items on removed terrain fall down 1 z-level

**Material handling:**
- Carved material drops as items (like mining)
- DIRT → ITEM_DIRT
- CLAY → ITEM_CLAY
- Rock materials → ITEM_ROCK

**Visual feedback:**
- Red outline shows brush area
- Dashed lines show affected cells
- Number indicator shows brush size (3x3, 5x5, etc.)

### Raise Terrain Brush

**Effect:** Adds z-level of terrain in brush area

**Behavior:**
- Click & drag to build up terrain
- Each cell in brush radius rises 1 z-level
- Air cells → natural wall (material from context)
- Cannot raise through existing structures (blocked)
- Requires material source (see Material System below)

**Material handling:**
- Consumes items from nearby stockpiles
- ITEM_DIRT → MAT_DIRT terrain
- ITEM_CLAY → MAT_CLAY terrain
- ITEM_ROCK → MAT_GRANITE terrain (or match existing)

**Visual feedback:**
- Green outline shows brush area
- Preview shows raised terrain (ghost sprite)
- Material cost shown in tooltip

---

## Brush Size Control

### Size Options
```
1x1: Single tile (precise editing)
3x3: Small brush (9 tiles, radius 1)
5x5: Medium brush (25 tiles, radius 2)
7x7: Large brush (49 tiles, radius 3)
9x9: Extra large (81 tiles, radius 4) [optional]
```

### UI Controls

**Keybinds:**
```
[ (left bracket)  - Decrease brush size
] (right bracket) - Increase brush size
1-4 number keys   - Quick size presets (1x1, 3x3, 5x5, 7x7)
```

**Visual indicator:**
- Cursor shows brush circle/square at target location
- Size number displayed ("3x3" or "Radius: 1")
- Affected tiles highlighted

**Tooltip:**
```
Lower Terrain [3x3 brush]
- Will remove 9 cells
- Materials: Dirt (estimated 18 items)
- Depth: 1 z-level
Click and drag to carve
```

---

## Drawing Mechanics

### Freehand Mode (Recommended)

**Pattern: Similar to wall/floor drawing**

```c
// In input handler
static bool isTerrainBrushActive = false;
static int lastBrushX = -1, lastBrushY = -1;

void HandleTerrainBrush() {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        isTerrainBrushActive = true;
        ApplyBrush(cursorX, cursorY, cursorZ, brushRadius);
        lastBrushX = cursorX;
        lastBrushY = cursorY;
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && isTerrainBrushActive) {
        // Draw along path (interpolate if moved)
        if (cursorX != lastBrushX || cursorY != lastBrushY) {
            // Get cells along line from last to current
            ForEachCellInLine(lastBrushX, lastBrushY, cursorX, cursorY,
                              ApplyBrush, brushRadius);
            lastBrushX = cursorX;
            lastBrushY = cursorY;
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        isTerrainBrushActive = false;
    }
}
```

**Features:**
- Smooth continuous strokes
- No gaps when dragging quickly
- Interpolates between cursor positions
- Undo-friendly (single operation per stroke)

### Alternative: Click-Stamp Mode

Single click applies brush once (no dragging).

**Use case:** Precise single-tile edits, careful placement

### Shape Options (Future)

**Current:** Circular/square brush
**Future:**
- Line tool (straight channels)
- Rectangle fill (level large areas)
- Gradient tool (smooth slopes)

---

## Material System

### Lower Terrain (Simple)

When carving terrain:
1. Check material of cell being removed (`GetWallMaterial(x, y, z)`)
2. Spawn corresponding item type
3. Item falls to ground at new lower z-level

**Example:**
- Remove MAT_DIRT wall → spawn ITEM_DIRT
- Remove MAT_CLAY wall → spawn ITEM_CLAY
- Remove MAT_GRANITE wall → spawn ITEM_ROCK

**No material input needed** - just carving what's already there.

### Raise Terrain (Complex)

Two approaches for material source:

#### Approach A: Free Material (Creative Mode)
- Raised terrain uses default material (MAT_DIRT)
- No item consumption
- Instant gratification
- **Pro:** Fast, enables quick river carving + dam building
- **Con:** No resource management

#### Approach B: Consume Items (Survival Mode)
- Requires items from nearby stockpiles (radius 10-20)
- ITEM_DIRT → raises as MAT_DIRT
- ITEM_CLAY → raises as MAT_CLAY
- ITEM_ROCK → raises as MAT_GRANITE
- Each cell consumes 1 item (or more for thick walls)
- **Pro:** Realistic, resource-gated
- **Con:** Requires stockpile logistics

#### Recommended: Hybrid System

**Creative terrain mode** (instant, free):
- `MODE_SCULPT_TERRAIN` - free raise/lower for base planning
- Enabled in settings or cheat menu
- Use case: Testing layouts, creative builds

**Survival terrain mode** (designations):
- Lower brush → creates mining designations (movers dig)
- Raise brush → creates construction designations (movers haul dirt/clay)
- Use case: Realistic gameplay, resource management

**Toggle:** `Shift+T` or setting in pause menu

---

## Input Mode Organization

### New Mode: MODE_SCULPT_TERRAIN

```
MODE_SCULPT_TERRAIN
  ├─ L: Lower terrain (carve)
  ├─ R: Raise terrain (build up)
  ├─ [/]: Adjust brush size
  ├─ 1-4: Quick size presets
  └─ M: Toggle material type (dirt/clay/rock) [for raise tool]
```

**Mode toggle:** `T` key (or available key)

**Visual:** Cursor shows brush circle, affected tiles highlighted

### Alternative: Extend MODE_WORK

Add to existing Work > Designate submenu:
```
Designate
  ├─ ... existing (mine, chop, plant)
  ├─ Shift+L: Lower terrain
  └─ Shift+R: Raise terrain
```

**Recommended:** Dedicated mode for clear UX and easier brush size control.

---

## Use Cases

### 1. Carve River Channel
**Goal:** Create flowing river for mud mixer workshop

**Steps:**
1. Enter sculpt mode, select Lower Terrain
2. Set brush size to 3x3
3. Drag along desired river path
4. Terrain carves 1 z-level down
5. Place water source at start → water flows into channel
6. Place mud mixer on channel bed

**Result:** Custom river exactly where needed

### 2. Dig Defensive Moat
**Goal:** Protect base with water-filled trench

**Steps:**
1. Lower terrain around base perimeter (5x5 brush for speed)
2. Channel is 1 z-level deep, 3-5 tiles wide
3. Place water source at corner → fills moat
4. Optional: Place drain at opposite corner (controlled water level)

**Result:** Impassable water barrier around base

### 3. Level Ground for Construction
**Goal:** Flatten hillside for workshop placement

**Steps:**
1. Use Lower brush to remove high spots
2. Use Raise brush to fill low spots
3. Result: Flat platform at desired z-level

**Result:** Even terrain for large buildings

### 4. Terraced Hillside
**Goal:** Create stepped farming terraces (future)

**Steps:**
1. Carve horizontal shelves into hillside (Lower brush)
2. Build retaining walls at shelf edges (Raise brush)
3. Each terrace at different z-level

**Result:** Multi-level terraced terrain

### 5. Dam Construction
**Goal:** Block natural river, create reservoir

**Steps:**
1. Raise terrain across river path (5x5 brush)
2. Build dam 2-3 z-levels high
3. Water backs up behind dam

**Result:** Controlled water storage

### 6. Smooth Terrain Transition
**Goal:** Create gradual slope instead of cliff

**Steps:**
1. Lower terrain at cliff top (1 z-level)
2. Repeat at intervals moving downward
3. Creates stepped slope (ramps can be added for walkability)

**Result:** Gentle grade instead of sheer drop

---

## Technical Implementation

### Core Function: ApplyBrush

```c
void ApplyLowerBrush(int centerX, int centerY, int z, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int x = centerX + dx;
            int y = centerY + dy;

            // Check if within circular brush (optional)
            float distSq = dx*dx + dy*dy;
            if (distSq > radius*radius) continue;

            // Skip if out of bounds
            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) continue;

            // Only affect natural ground
            if (!IsNaturalGround(x, y, z)) continue;

            // Get material before removal
            MaterialType mat = GetWallMaterial(x, y, z);

            // Remove top cell (lower terrain)
            SetCell(x, y, z, CELL_AIR);
            SetWallNatural(x, y, z, false); // No longer has wall

            // Spawn dropped item at lower z-level
            ItemType dropItem = MaterialDropsItem(mat);
            if (dropItem != ITEM_NONE) {
                float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                SpawnItemWithMaterial(spawnX, spawnY, (float)(z - 1),
                                       dropItem, (uint8_t)mat);
            }

            // Drop items that were on this cell
            DropItemsInCell(x, y, z);
        }
    }
}

void ApplyRaiseBrush(int centerX, int centerY, int z, int radius, MaterialType mat) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int x = centerX + dx;
            int y = centerY + dy;

            // Check if within circular brush
            float distSq = dx*dx + dy*dy;
            if (distSq > radius*radius) continue;

            // Skip if out of bounds
            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) continue;

            // Only raise in air cells (can't build through structures)
            if (grid[z][y][x] != CELL_AIR) continue;

            // Add terrain at this z-level
            SetCell(x, y, z, CELL_WALL);
            SetWallMaterial(x, y, z, mat);
            SetWallNatural(x, y, z, true); // Natural terrain

            // TODO: Consume items from nearby stockpile (survival mode)
        }
    }
}
```

### Brush Preview Rendering

```c
void RenderBrushPreview(int centerX, int centerY, int radius, bool isLower) {
    Color previewColor = isLower ? ColorAlpha(RED, 0.3f) : ColorAlpha(GREEN, 0.3f);

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            float distSq = dx*dx + dy*dy;
            if (distSq > radius*radius) continue;

            int x = centerX + dx;
            int y = centerY + dy;

            // Draw tile highlight
            Rectangle tileRect = {
                x * CELL_SIZE,
                y * CELL_SIZE,
                CELL_SIZE,
                CELL_SIZE
            };
            DrawRectangleRec(tileRect, previewColor);
            DrawRectangleLinesEx(tileRect, 1.0f, previewColor);
        }
    }

    // Draw brush size text
    DrawText(TextFormat("%dx%d", radius*2+1, radius*2+1),
             centerX * CELL_SIZE, centerY * CELL_SIZE, 20, WHITE);
}
```

### Interpolation for Smooth Strokes

```c
void ForEachCellInLine(int x0, int y0, int x1, int y1,
                        void (*callback)(int, int, int, int),
                        int brushRadius, int z) {
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int x = x0, y = y0;
    while (true) {
        callback(x, y, z, brushRadius);

        if (x == x1 && y == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}
```

---

## Integration with Existing Systems

### Mining System
Lower brush produces same drops as mining:
- Uses `MaterialDropsItem()` function
- Items spawn at lower z-level
- Respects material types

### Construction System
Raise brush could eventually create construction jobs:
- Blueprint at target location
- Requires ITEM_DIRT / ITEM_CLAY delivery
- Movers haul materials and place terrain

### Water System
Carved channels interact naturally:
- Lower terrain → water flows into depression
- Raised terrain → dams block water flow
- No special water code needed

### Pathfinding
Terrain changes update pathfinding automatically:
- Lowered cells may become cliffs (not walkable)
- Raised cells block paths
- Existing `DestabilizeAround()` pattern applies

---

## Survival Mode: Designation-Based

Instead of instant terrain editing, create designations for movers to execute.

### Lower Terrain → Mining Designation
```c
void DesignateLowerTerrain(int centerX, int centerY, int z, int radius) {
    for (each cell in brush) {
        if (IsNaturalGround(x, y, z)) {
            AddDesignation(DESIGNATION_MINE, x, y, z);
        }
    }
}
```

**Result:** Orange mining designations appear, movers dig them out.

### Raise Terrain → Construction Designation
```c
void DesignateRaiseTerrain(int centerX, int centerY, int z, int radius, MaterialType mat) {
    for (each cell in brush) {
        if (grid[z][y][x] == CELL_AIR) {
            // Create blueprint for natural wall
            Blueprint bp;
            bp.x = x; bp.y = y; bp.z = z;
            bp.type = BLUEPRINT_WALL;
            bp.itemType = MaterialToItem(mat); // ITEM_DIRT, ITEM_CLAY, etc.
            bp.material = mat;
            AddBlueprint(bp);
        }
    }
}
```

**Result:** Green construction blueprints appear, movers haul dirt/clay and place terrain.

**Benefits:**
- Resource-gated (requires items)
- Time-gated (movers must work)
- Realistic simulation
- Integrates with existing job system

**Drawbacks:**
- Slower than instant (but that's the point)
- Requires stockpiles near work site

---

## UI/UX Details

### Cursor Display

**Brush preview:**
- Circle/square outline at cursor
- Tiles within brush highlighted
- Size indicator ("3x3")

**Material preview (raise tool):**
- Ghost sprites show what will be placed
- Color-coded by material (brown=dirt, gray=clay, etc.)

**Cost estimate (raise tool, survival mode):**
- "Materials needed: Dirt x9"
- Red text if insufficient materials nearby
- Green text if materials available

### Tooltips

**Lower brush:**
```
Lower Terrain [3x3 brush]
Click and drag to carve
↓ 1 z-level deep
Drops: Dirt x9 (estimated)
```

**Raise brush:**
```
Raise Terrain [5x5 brush]
Click and drag to build up
↑ 1 z-level high
Cost: Dirt x25
[Insufficient materials] (in red if needed)
```

### Settings

**Terrain Sculpting Options:**
- `Enable instant sculpting` (creative mode, default OFF)
- `Default brush size` (1, 3, 5, or 7)
- `Material consumption` (free vs survival)
- `Preview opacity` (0.1 - 1.0)

---

## Advantages Over Specific Water Tools

### Generality
- Lower brush → river channels, moats, basins, valleys
- Raise brush → dams, hills, embankments, platforms
- Not limited to water features

### Flexibility
- Any shape, any size, freehand control
- Adjustable brush = coarse work (large brush) + fine detail (small brush)
- Player creativity enabled

### Simplicity
- One tool, many uses
- Intuitive: "paint terrain up/down"
- No need for separate "dig channel", "place spring", "build dam" actions

### Natural Water Integration
- Carve channel → water flows in naturally (no special code)
- Build dam → water blocked naturally (no special code)
- Water simulation does the work

---

## Implementation Phases

### Phase 1: Creative Instant Sculpting (Proof of Concept)
1. Add `MODE_SCULPT_TERRAIN` input mode
2. Implement `ApplyLowerBrush()` (instant removal)
3. Implement `ApplyRaiseBrush()` (instant placement, free material)
4. Add brush size control (`[` / `]` keys)
5. Add brush preview rendering
6. **Test:** Carve river channel, build dam, level ground

**Estimated time:** 6-8 hours

**Deliverable:** Creative mode terrain sculpting works, enables river/moat carving.

### Phase 2: Polish & Feedback
1. Smooth stroke interpolation (no gaps when dragging)
2. Better brush preview (circular highlight, size indicator)
3. Material drops on lower (spawn items correctly)
4. Tooltips with cost/drops estimates
5. Visual feedback (particles on carve/raise)

**Estimated time:** 3-4 hours

### Phase 3: Survival Mode (Designations)
1. Lower brush → creates mining designations
2. Raise brush → creates construction blueprints
3. Material consumption from stockpiles
4. Job system integration (movers dig/build)
5. **Test:** Designate channel, movers dig it, water flows

**Estimated time:** 8-10 hours

### Phase 4: Advanced Features
1. Alternative brush shapes (line tool, rectangle fill)
2. Material selection UI (choose dirt/clay/rock for raise)
3. Undo/redo for terrain changes
4. Brush strength (multiple z-levels at once)

**Estimated time:** 6-8 hours

**Total estimated time:** 23-30 hours for full system

---

## Open Questions

1. **Max brush size?** 7x7, 9x9, or larger for massive earthworks?
2. **Multiple z-levels?** Should brush affect multiple z at once, or always 1 z-level per stroke?
3. **Brush shape?** Circular vs square (or both with toggle)?
4. **Material mixing?** If carving mixed terrain (dirt + clay), which material drops?
5. **Cost per cell?** Should raising 1 cell consume 1 item, or more for thicker walls?
6. **Undo system?** Track terrain changes for undo/redo?
7. **Limits?** Should there be a max depth/height to prevent extreme sculpting?
8. **Sound effects?** Digging sound when carving, placing sound when raising?

---

## Summary

**What this enables:**
- General-purpose terrain editing (not just water)
- River/channel carving for mud mixer workshops
- Moats, dams, levees, terraces
- Base planning and aesthetic landscaping
- Creative freedom in shaping the world

**Key advantages:**
- More flexible than specific "place spring" or "dig channel" tools
- Freehand drawing with adjustable brush = intuitive and powerful
- Works in both creative (instant) and survival (designations) modes
- Natural integration with water simulation (no special water code)

**Recommended first implementation:**
- Phase 1 (creative instant sculpting) = ~8 hours
- Immediately enables river carving for mud mixer
- Can add survival mode + polish later

**Key insight:** By giving players direct terrain sculpting control, we enable water features naturally (carve low ground → water fills it). More general and powerful than water-specific tools, and unlocks many other use cases (moats, leveling, dams, terraces).
