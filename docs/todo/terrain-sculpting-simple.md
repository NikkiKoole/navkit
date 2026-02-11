# Terrain Sculpting - Simple Draw Mode

Date: 2026-02-11
Status: Implementation ready - instant sculpting only

---

## Scope

**Simple instant terrain editing** - paint terrain up/down with adjustable brush, no jobs/designations.

**What's included:**
- ✅ Lower terrain brush (instant carve)
- ✅ Raise terrain brush (instant build, free material)
- ✅ Adjustable brush diameter (1x1, 3x3, 5x5, 7x7)
- ✅ Freehand mouse drag (like drawing walls/floors)
- ✅ Visual preview of brush area

**What's NOT included:**
- ❌ Worker jobs/designations (no movers involved)
- ❌ Material consumption (raising terrain is free)
- ❌ Material drops (carving doesn't spawn items)
- ❌ Complex brush shapes (just circular/square)

**Goal:** Get basic terrain sculpting working quickly (~6-8 hours) to enable river carving for mud mixer.

---

## User Interface

### New Input Mode: MODE_SCULPT_TERRAIN

**Activation:** Press `T` key (or available key)

**Controls:**
```
L           - Lower terrain (carve down)
R           - Raise terrain (build up)
[ ]         - Decrease/increase brush size
1-4         - Quick size presets (1x1, 3x3, 5x5, 7x7)
Mouse drag  - Paint terrain along path
ESC         - Exit sculpt mode
```

### Visual Feedback

**Brush preview:**
- Circular outline at cursor position
- Highlighted tiles show affected area
- Size indicator text ("3x3" or "Radius: 1")
- Color: Red for lower, Green for raise

**Active drawing:**
- Tiles change immediately on mouse drag
- Smooth continuous stroke (no gaps)

---

## Brush Sizes

| Size | Radius | Tiles | Use Case |
|------|--------|-------|----------|
| 1x1 | 0 | 1 | Precise single-tile edits |
| 3x3 | 1 | 9 | Small channels, detail work |
| 5x5 | 2 | 25 | Medium channels, moats |
| 7x7 | 3 | 49 | Large rivers, leveling |

**Formula:** `tiles = (radius * 2 + 1)²`

---

## Implementation

### 1. Add Input Mode

**File:** `src/core/input_mode.h`
```c
typedef enum {
    // ... existing modes
    MODE_SCULPT_TERRAIN,
} InputMode;
```

### 2. Add Actions

**File:** `src/core/action_registry.c`
```c
typedef enum {
    // ... existing actions
    ACTION_SCULPT_LOWER,
    ACTION_SCULPT_RAISE,
    ACTION_SCULPT_BRUSH_DECREASE,
    ACTION_SCULPT_BRUSH_INCREASE,
    ACTION_SCULPT_BRUSH_SIZE_1,
    ACTION_SCULPT_BRUSH_SIZE_2,
    ACTION_SCULPT_BRUSH_SIZE_3,
    ACTION_SCULPT_BRUSH_SIZE_4,
} InputAction;
```

### 3. Core Sculpting Functions

**File:** `src/world/terrain_sculpt.c` (new file)

```c
#include "terrain_sculpt.h"
#include "grid.h"

// Current brush state
static int brushRadius = 1;  // Default 3x3 (radius 1)
static bool isLowerMode = true;

void SetBrushRadius(int radius) {
    if (radius < 0) radius = 0;
    if (radius > 3) radius = 3;  // Max 7x7
    brushRadius = radius;
}

int GetBrushRadius(void) {
    return brushRadius;
}

void SetBrushMode(bool lower) {
    isLowerMode = lower;
}

bool IsBrushLowerMode(void) {
    return isLowerMode;
}

// Apply brush at a single point (called for each cell in drag path)
void ApplyBrushAt(int centerX, int centerY, int z) {
    for (int dy = -brushRadius; dy <= brushRadius; dy++) {
        for (int dx = -brushRadius; dx <= brushRadius; dx++) {
            int x = centerX + dx;
            int y = centerY + dy;

            // Check if within circular brush
            float distSq = (float)(dx * dx + dy * dy);
            float radiusSq = (float)(brushRadius * brushRadius);
            if (distSq > radiusSq + 0.5f) continue;

            // Bounds check
            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) continue;
            if (z < 0 || z >= gridDepth) continue;

            if (isLowerMode) {
                // Lower terrain: remove top cell if natural
                if (IsWallNatural(x, y, z)) {
                    SetCell(x, y, z, CELL_AIR);
                    SetWallNatural(x, y, z, false);
                    // Drop items that were on this cell
                    DropItemsInCell(x, y, z);
                }
            } else {
                // Raise terrain: add cell if air
                if (grid[z][y][x] == CELL_AIR) {
                    SetCell(x, y, z, CELL_WALL);
                    SetWallMaterial(x, y, z, MAT_DIRT);  // Default material
                    SetWallNatural(x, y, z, true);
                }
            }
        }
    }
}
```

**File:** `src/world/terrain_sculpt.h` (new file)
```c
#ifndef TERRAIN_SCULPT_H
#define TERRAIN_SCULPT_H

#include <stdbool.h>

void SetBrushRadius(int radius);
int GetBrushRadius(void);
void SetBrushMode(bool lower);
bool IsBrushLowerMode(void);
void ApplyBrushAt(int centerX, int centerY, int z);

#endif
```

### 4. Input Handler

**File:** `src/core/input.c`

Add to input handler:
```c
#include "../world/terrain_sculpt.h"

// Track brush state for continuous drawing
static bool isBrushActive = false;
static int lastBrushX = -1, lastBrushY = -1;

void HandleSculptTerrainMode(void) {
    // Brush size controls
    if (IsKeyPressed(KEY_LEFT_BRACKET)) {
        SetBrushRadius(GetBrushRadius() - 1);
        AddMessage(TextFormat("Brush: %dx%d",
                   GetBrushRadius()*2+1, GetBrushRadius()*2+1), GRAY);
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
        SetBrushRadius(GetBrushRadius() + 1);
        AddMessage(TextFormat("Brush: %dx%d",
                   GetBrushRadius()*2+1, GetBrushRadius()*2+1), GRAY);
    }

    // Quick size presets
    if (IsKeyPressed(KEY_ONE))   { SetBrushRadius(0); }
    if (IsKeyPressed(KEY_TWO))   { SetBrushRadius(1); }
    if (IsKeyPressed(KEY_THREE)) { SetBrushRadius(2); }
    if (IsKeyPressed(KEY_FOUR))  { SetBrushRadius(3); }

    // Mode selection
    if (IsKeyPressed(KEY_L)) {
        SetBrushMode(true);  // Lower
        AddMessage("Lower terrain mode", RED);
    }
    if (IsKeyPressed(KEY_R)) {
        SetBrushMode(false);  // Raise
        AddMessage("Raise terrain mode", GREEN);
    }

    // Get cursor position in grid coords
    int cursorX = (int)(mouseWorldX / CELL_SIZE);
    int cursorY = (int)(mouseWorldY / CELL_SIZE);
    int cursorZ = cameraZ;

    // Mouse drawing
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        isBrushActive = true;
        ApplyBrushAt(cursorX, cursorY, cursorZ);
        lastBrushX = cursorX;
        lastBrushY = cursorY;
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && isBrushActive) {
        // Only apply if cursor moved
        if (cursorX != lastBrushX || cursorY != lastBrushY) {
            // Interpolate between last and current position
            DrawLineBetween(lastBrushX, lastBrushY, cursorX, cursorY, cursorZ);
            lastBrushX = cursorX;
            lastBrushY = cursorY;
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        isBrushActive = false;
    }
}

// Bresenham line drawing to fill gaps
void DrawLineBetween(int x0, int y0, int x1, int y1, int z) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int x = x0, y = y0;
    while (true) {
        ApplyBrushAt(x, y, z);

        if (x == x1 && y == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}
```

### 5. Visual Rendering

**File:** `src/render/rendering.c`

Add brush preview rendering:
```c
void RenderBrushPreview(void) {
    if (inputMode != MODE_SCULPT_TERRAIN) return;

    int cursorX = (int)(mouseWorldX / CELL_SIZE);
    int cursorY = (int)(mouseWorldY / CELL_SIZE);
    int radius = GetBrushRadius();
    bool isLower = IsBrushLowerMode();

    Color brushColor = isLower ? ColorAlpha(RED, 0.3f) : ColorAlpha(GREEN, 0.3f);
    Color outlineColor = isLower ? RED : GREEN;

    // Draw affected tiles
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            float distSq = (float)(dx * dx + dy * dy);
            float radiusSq = (float)(radius * radius);
            if (distSq > radiusSq + 0.5f) continue;

            int x = cursorX + dx;
            int y = cursorY + dy;

            // Convert to screen space
            Vector2 screenPos = WorldToScreen(
                x * CELL_SIZE,
                y * CELL_SIZE,
                cameraZ * CELL_SIZE
            );

            DrawRectangle(
                (int)screenPos.x, (int)screenPos.y,
                (int)CELL_SIZE, (int)CELL_SIZE,
                brushColor
            );
            DrawRectangleLines(
                (int)screenPos.x, (int)screenPos.y,
                (int)CELL_SIZE, (int)CELL_SIZE,
                outlineColor
            );
        }
    }

    // Draw size indicator
    Vector2 centerScreen = WorldToScreen(
        cursorX * CELL_SIZE,
        cursorY * CELL_SIZE,
        cameraZ * CELL_SIZE
    );
    DrawText(TextFormat("%dx%d", radius*2+1, radius*2+1),
             (int)centerScreen.x, (int)centerScreen.y - 20,
             16, WHITE);
}
```

Call this in main render loop after drawing grid.

---

## Testing

### Test 1: Basic Carving
1. Enter sculpt mode (`T` key)
2. Select lower mode (`L` key)
3. Click and drag across terrain
4. Verify cells are removed along path
5. No gaps in carved line

### Test 2: Brush Sizes
1. Press `[` to decrease size (1x1)
2. Carve single-tile line
3. Press `]` to increase size (3x3)
4. Carve wider line
5. Press `4` for max size (7x7)
6. Carve very wide path

### Test 3: Raising Terrain
1. Select raise mode (`R` key)
2. Drag over carved area
3. Verify terrain fills back in
4. Terrain appears as natural dirt walls

### Test 4: River Creation
1. Lower brush, 3x3 size
2. Carve winding channel across map
3. Place water source at start (manual for now)
4. Verify water flows through carved channel

### Test 5: Smooth Strokes
1. Drag mouse quickly across screen
2. Verify no gaps in carved/raised terrain
3. Line interpolation works correctly

---

## Water Integration

Once terrain sculpting works:

1. **Carve channel** with lower brush
2. **Add water source** (temporary: use existing debug commands, or add simple "place water source" action)
3. Water flows into carved channel naturally
4. **Place mud mixer** on channel bed
5. Workshop has water access ✅

Later, can add proper water source/drain placement UI, but terrain sculpting is the critical piece.

---

## File Structure

New files:
```
src/world/
  ├─ terrain_sculpt.c  (core brush logic)
  └─ terrain_sculpt.h  (public API)
```

Modified files:
```
src/core/
  ├─ input_mode.h       (add MODE_SCULPT_TERRAIN)
  ├─ action_registry.c  (add sculpt actions)
  └─ input.c            (add HandleSculptTerrainMode)

src/render/
  └─ rendering.c        (add RenderBrushPreview)

src/unity.c             (include terrain_sculpt.c)
```

---

## Limitations (Acceptable for v1)

**What this DOESN'T do:**
- No material drops when carving (items don't spawn)
- No material consumption when raising (free building)
- No undo/redo
- Only circular brush shape
- Only 1 z-level at a time
- No worker jobs/designations

**Why these are OK:**
- Goal is river carving for workshops, not full simulation
- Can add material economy later if needed
- Instant feedback is valuable for creative mode
- Simple = faster to implement and debug

---

## Estimated Implementation Time

**Core functionality:**
- terrain_sculpt.c/h: 2 hours
- Input handling: 2 hours
- Visual preview: 1 hour
- Testing & polish: 2 hours

**Total: ~7 hours**

**Deliverable:** Working terrain sculpting that enables river carving for mud mixer workshops.

---

## Future Enhancements (Not Now)

- Material drops (carving spawns items)
- Material costs (raising consumes items)
- Undo/redo system
- Worker designations (survival mode)
- Multiple z-levels per stroke
- Alternative brush shapes (square, line tool)
- Gradient/slope tool
- Copy/paste terrain regions

These can all be added later if needed. Start simple!

---

## Success Criteria

✅ Can enter sculpt mode
✅ Can switch between lower/raise modes
✅ Can adjust brush size with keys
✅ Can drag mouse to carve/raise terrain
✅ Strokes are smooth (no gaps)
✅ Brush preview shows affected area
✅ Can carve a river channel
✅ Water flows through carved channel
✅ Mud mixer can be placed on carved terrain

If all ✅, terrain sculpting is complete!
