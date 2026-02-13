# Ramps Design & Improvement Plan

## Current State Analysis

### What We Have

Ramps are **fully implemented** in the codebase with 4 directional types: `CELL_RAMP_N/E/S/W` (high side points north/east/south/west). They work with all pathfinding algorithms (A*, HPA*, JPS+).

### Three Ways to Create Ramps

#### 1. DIG RAMP (Designation)
**Command:** `ACTION_WORK_DIG_RAMP` via designation system
- **Function:** `CompleteDigRampDesignation()` in designations.c:589
- **What it does:** Carves a ramp out of existing wall/dirt/rock
- **Material:** Inherits from the wall being carved (dirt, stone, clay, etc.)
- **Outputs:**
  - Ramp at z (with wallMaterial set to source material)
  - Floor at z+1 (with floorMaterial set to source material)
  - Item drop based on material (rock, dirt chunk, etc.)
- **Natural flag:** Preserves IsWallNatural status
- **Direction:** Auto-detected via `AutoDetectDigRampDirection()`

#### 2. BUILD RAMP (Construction Blueprint)
**Command:** `ACTION_DRAW_RAMP` creates blueprints via `CreateRecipeBlueprint(CONSTRUCTION_RAMP)`
- **Recipe:** `CONSTRUCTION_RAMP` in construction.c:106-123
- **Requirements:**
  - 1 any-building-mat item (rock, planks, logs, bricks)
  - Must have floor to build on (`HAS_FLOOR` check at line 1755)
  - Cell must be CELL_AIR (can't build over existing stuff)
- **Build time:** 2.0s
- **Material:** Inherited from delivered item
- **Completion:** `CompleteBlueprint()` at designations.c:2117-2143
- **Outputs:**
  - Ramp at z (with wallMaterial from delivered item)
  - Floor at z+1 (with floorMaterial from delivered item)
  - WallSourceItem tracked for refunds
- **Natural flag:** Cleared (constructed ramp)
- **Direction:** Auto-detected based on adjacent walls (lines 2121-2130)
- **Finish:** FINISH_SMOOTH

#### 3. REMOVE RAMP (Designation)
**Command:** `ACTION_WORK_REMOVE_RAMP` via designation system
- **What it does:** Removes any ramp (natural or constructed)
- **Outputs:** Spawns item based on ramp's material and natural status

### Material System

Ramps use the **wallMaterial grid** to store their composition:
- `SetWallMaterial(x, y, z, mat)` - stores the material (MAT_DIRT, MAT_GRANITE, MAT_OAK, etc.)
- `SetWallNatural()` / `ClearWallNatural()` - distinguishes dug vs built
- `SetWallSourceItem()` - tracks what item was used (for constructed ramps)
- `SetWallFinish()` - visual finish (FINISH_ROUGH for natural, FINISH_SMOOTH for constructed)

The floor created on z+1 mirrors the ramp's material.

### Direction Auto-Detection

Two different algorithms exist:

**For Dig Ramp** (`AutoDetectDigRampDirection()` in designations.c):
- Looks for adjacent walkable floor to determine "low side"
- Ramp points toward wall (high side)
- Returns first valid direction found (N→E→S→W priority)

**For Build Ramp** (in `CompleteBlueprint()` at line 2121-2130):
- Looks for adjacent walls
- If wall to north → ramp points south (CELL_RAMP_S)
- If wall to east → ramp points west (CELL_RAMP_W)
- Etc.
- **Opposite logic from dig ramp!**

## Issues & Inconsistencies

### 1. **Confusing Direction Auto-Detection**
The two auto-detect systems use opposite logic:
- Dig ramp: points **toward** wall
- Build ramp: points **away from** wall

This is confusing and may produce unexpected results.

### 2. **No Manual Direction Control for BUILD RAMP**
- ACTION_DRAW_RAMP supports manual direction selection via arrow keys (input.c:1994-2012)
- `selectedRampDirection` global controls which direction
- But blueprint creation doesn't use this - it always auto-detects at completion time
- User picks direction, but it's ignored!

### 3. **Build Ramp Placement Validation is Weak**
Line 1753-1756 checks:
```c
if (grid[z][y][x] != CELL_AIR) return -1;
if (!HAS_FLOOR(x, y, z)) return -1;
```

But doesn't check:
- Is the high-side exit at z+1 valid/walkable?
- Will the ramp actually connect to anything?
- Does the chosen direction make sense?

### 4. **No Visual Distinction**
Currently no way to visually distinguish:
- Natural ramps (dug from dirt/stone)
- Constructed ramps (built from materials)

Both render identically despite having different material properties and refund behavior.

### 5. **Material Not Visible**
Ramp material is stored but not displayed:
- Stone ramps vs wood ramps look the same
- No sprite variation based on material
- Tooltip doesn't show material (needs verification)

### 6. **Floor Auto-Creation May Be Unexpected**
Both dig and build automatically create floor at z+1 if space exists.
- May surprise users who didn't want floor there
- No way to prevent this
- Could interfere with multi-level designs

### 7. **Direction Override System Not Integrated**
The manual direction selection (arrow keys while ACTION_DRAW_RAMP) exists but:
- Only works for sandbox direct placement
- Not used by blueprint system
- Users expect it to work, but it doesn't for constructed ramps

## Proposed Improvements

### Priority 1: Unify Direction Logic

**Goal:** Make direction auto-detection consistent and predictable

**Option A - Always Point Toward Wall (DF-style):**
```c
// Both dig and build use same logic
// Ramp points toward solid wall (high side = wall side)
// Makes sense: you climb up TOWARD the wall
```

**Option B - Always Point Away From Wall:**
```c
// Ramp points away from wall (low side = wall side)
// Makes sense: wall "supports" the low side
```

**Recommendation:** Option A (point toward wall)
- More intuitive: "climb toward the wall"
- Matches Dwarf Fortress behavior
- Current dig ramp already does this

**Changes needed:**
1. Update `CompleteBlueprint()` BUILD_RAMP section (lines 2121-2130) to use same logic as `AutoDetectDigRampDirection()`
2. Extract common function: `AutoDetectRampDirection(int x, int y, int z)` used by both

### Priority 2: Respect Manual Direction Selection

**Goal:** When user picks direction with arrow keys, use it!

**Implementation:**
1. Pass `selectedRampDirection` to blueprint creation
2. Add field to Blueprint struct: `CellType requestedRampType`
3. At completion, use `bp->requestedRampType` instead of auto-detect
4. If user pressed 'A' for auto → use `CELL_AIR` as sentinel value → auto-detect at completion

**Benefits:**
- User control over ramp direction
- Matches existing UI affordance (arrow keys shown in tooltips)
- Backward compatible (auto-detect still default)

### Priority 3: Better Placement Validation

**Goal:** Validate ramp will actually work before creating blueprint

**Add checks in `CreateRecipeBlueprint()` for BUILD_RAMP:**
```c
// 1. Current cell is air + has floor (already checked)

// 2. Get intended direction (manual or auto-detect)
CellType rampType = GetIntendedRampDirection(x, y, z, selectedRampDirection);
if (rampType == CELL_AIR) return -1;  // No valid direction

// 3. Check exit tile at z+1 is valid
int highDx, highDy;
GetRampHighSideOffset(rampType, &highDx, &highDy);
int exitX = x + highDx;
int exitY = y + highDy;

// Bounds check
if (z+1 >= gridDepth) return -1;
if (exitX < 0 || exitX >= gridWidth) return -1;
if (exitY < 0 || exitY >= gridHeight) return -1;

// Exit must be walkable or empty
if (!IsCellWalkableAt(z+1, exitY, exitX) && grid[z+1][exitY][exitX] != CELL_AIR) {
    return -1;
}

// 4. Low side should be accessible (not blocked)
int lowX = x - highDx;
int lowY = y - highDy;
if (lowX >= 0 && lowX < gridWidth && lowY >= 0 && lowY < gridHeight) {
    if (IsWallCell(grid[z][lowY][lowX])) return -1;  // Can't enter from blocked side
}
```

**Benefits:**
- Prevents creating useless blueprints
- Clear feedback when placement invalid
- Matches ladder placement strictness

### Priority 4: Visual Material Distinction

**Goal:** Show what material the ramp is made of

**Option A - Sprite Variations:**
- Add sprites: `SPRITE_ramp_n_stone`, `SPRITE_ramp_n_wood`, etc.
- Select sprite based on wallMaterial
- Most visual clarity

**Option B - Color Tinting:**
```c
Color GetRampColor(int x, int y, int z) {
    MaterialType mat = GetWallMaterial(x, y, z);
    bool natural = IsWallNatural(x, y, z);

    Color base = GetMaterialColor(mat);  // Stone=grey, wood=brown, etc.
    if (natural) {
        base = ColorBrightness(base, -0.1f);  // Slightly darker for natural
    }
    return base;
}
```
- Easier to implement
- Works with existing sprites

**Option C - Finish Overlay:**
- Draw base ramp sprite
- Add finish sprite overlay (rough vs smooth)
- Shows natural vs constructed

**Recommendation:** Start with Option B (color tinting)
- Quick to implement
- Doesn't require new sprites
- Later add Option C for polish

### Priority 5: Tooltip Material Display

**Goal:** Show ramp material when hovering

**Add to tooltips.c:**
```c
if (CellIsDirectionalRamp(cell)) {
    MaterialType mat = GetWallMaterial(x, y, z);
    bool natural = IsWallNatural(x, y, z);
    const char* matName = MaterialName(mat);
    const char* source = natural ? "natural" : "constructed";

    DrawTextShadow(TextFormat("%s ramp (%s)", matName, source), x, y, fontSize, color);

    // Show direction
    const char* dir = "north";  // Based on ramp type
    DrawTextShadow(TextFormat("Leads %s", dir), x, y+lineH, fontSize, GRAY);
}
```

### Priority 6: Optional Floor Creation

**Goal:** Give user control over whether floor is auto-created at z+1

**Implementation:**
- Add global toggle: `bool autoCreateRampFloor = true`
- Add UI checkbox in construction panel
- Console variable: `rampAutoFloor`
- At completion, only create floor if enabled

**Alternative:**
- Always skip auto-floor for constructed ramps
- Only auto-create for dug ramps (makes sense - you're excavating)

### Priority 7: Better User Communication

**Goal:** Make ramp behavior obvious and predictable

**Documentation:**
- In-game help text for ramp actions
- Tooltip showing intended direction while placing
- Message when ramp blueprint created: "Oak ramp (pointing north)"

**Visual Preview:**
- When ACTION_DRAW_RAMP active, show ghost arrow at cursor indicating direction
- Update arrow in real-time as user presses arrow keys
- Show red X if placement invalid

## Implementation Roadmap

### Phase 1: Core Fixes (High Priority)
- [ ] Extract `AutoDetectRampDirection()` common function
- [ ] Fix BUILD_RAMP direction logic to match DIG_RAMP
- [ ] Add manual direction support to blueprint system
- [ ] Improve placement validation

**Effort:** ~150 lines
**Impact:** Fixes broken behavior, better UX

### Phase 2: Visual Clarity (Medium Priority)
- [ ] Add material color tinting to ramp rendering
- [ ] Add material info to tooltips
- [ ] Show natural vs constructed distinction

**Effort:** ~100 lines
**Impact:** Better feedback, easier to understand

### Phase 3: Polish (Low Priority)
- [ ] Optional floor auto-creation toggle
- [ ] Direction preview while placing
- [ ] In-game help text
- [ ] Finish overlays

**Effort:** ~150 lines
**Impact:** Nice-to-have improvements

## Testing Checklist

After implementing improvements:

- [ ] Dig ramp in dirt - material is dirt, natural flag set
- [ ] Build ramp with rock - material is rock, constructed flag set
- [ ] Build ramp with planks - material is wood, constructed flag set
- [ ] Manual direction (arrow keys) works for built ramps
- [ ] Auto-direction is consistent between dig and build
- [ ] Can't build ramp with invalid exit (blocked z+1)
- [ ] Can't build ramp with blocked low side
- [ ] Material shows in tooltip
- [ ] Color tint matches material
- [ ] Natural vs constructed is visually distinct
- [ ] Remove ramp spawns correct item based on material
- [ ] Floor auto-creation respects toggle (if implemented)
- [ ] All pathfinding still works (A*, HPA*, JPS+)

## Questions for User

1. **Direction logic:** Should ramps point toward wall or away from wall?
2. **Floor auto-creation:** Always create? Never create? User toggle?
3. **Visual priority:** Material color more important, or natural/constructed distinction?
4. **Manual direction:** Should it be required, or auto-detect with override?
5. **Invalid placement:** Hard block, or allow with warning?

## Related Files

- `src/world/construction.c` - CONSTRUCTION_RAMP recipe, CompleteBlueprint()
- `src/world/designations.c` - Dig ramp, remove ramp, auto-detection
- `src/core/input.c` - ACTION_DRAW_RAMP, selectedRampDirection
- `src/render/rendering.c` - Ramp rendering
- `src/render/tooltips.c` - Hover tooltips
- `docs/done/pathfinding/ramps-implementation.md` - Full implementation doc
