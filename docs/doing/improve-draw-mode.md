# Improve Draw Mode - Soil Category & Pile Mode

## Goal
Add a "Soil" category to draw mode that lets you paint different soil types and pile/sculpt terrain naturally.

## Current Soil Types
From `src/world/grid.h` (CellType):
- `CELL_DIRT` - Natural ground (grass/bare controlled by surface overlay)
- `CELL_CLAY` - Clay soil (subsoil blobs)
- `CELL_GRAVEL` - Gravel patches
- `CELL_SAND` - Sandy soil
- `CELL_PEAT` - Peat soil (wetlands)

Note: Rock/stone is not a soil type - it's `CELL_WALL` with material `MAT_GRANITE` (from material system)

## Features to Implement

### 1. Soil Category in Draw Mode
- Add new category alongside existing draw modes
- Allow selecting any of the 5 soil types
- Paint individual cells normally (like current draw behavior)

### 2. Pile Mode (Shift + Draw)
When Shift is held while drawing soil:
- **Accumulation**: If drawing on existing solid ground, soil "piles up" and spreads
  - Find available adjacent cells (up, sides) to place additional soil
  - Create natural mounding/piling effect
  
- **Gravity**: If drawing in open air with nothing below
  - Soil drops down until it finds solid support
  - Settles on first solid cell it encounters
  
- **Spreading Logic**: When piling on existing terrain
  - Check current cell: if occupied, look for adjacent empty cells
  - Priority order: up > horizontal (N/E/S/W) > down (if supported)
  - Create natural spreading effect like pouring sand

### 3. Pile Radius Control
- Add  DragFloat slider for user-controlled pile radius (check how this is odone in the ui of other elements)
- Limits how far soil can spread in a single draw action
- Prevents accidentally filling entire terrain instantly
- Default to something conservative (e.g., radius = 2-3)

### 4. Physics Considerations
- Respect support requirements (can't float in air)
- Soil needs solid ground beneath or adjacent supporting cells
- Visual feedback showing where soil will land/spread

## Key Technical Findings

### Wall vs Floor vs Solid Ground
Understanding from codebase inspection:

1. **CELL_DIRT/CLAY/SAND/GRAVEL/PEAT** are actual cell types with `CF_GROUND` flag
   - `CF_GROUND = CF_WALKABLE | CF_SOLID`
   - These are solid blocks you stand ON TOP of (DF-style)
   - They have both a CellType AND a material (stored in wallMaterial grid)
   
2. **CELL_WALL** is a solid barrier with `CF_WALL` flag
   - `CF_WALL = CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS | CF_SOLID`
   - Material determines appearance and drops (MAT_GRANITE, MAT_OAK, etc.)

3. **Floors** are different - they're a FLAG on CELL_AIR
   - `SET_FLOOR()` marks an air cell as having a constructed floor
   - Used for balconies/bridges - you can walk on air with a floor
   - Floor material stored separately in `floorMaterial` grid

4. **Example from ExecuteBuildDirt():**
   ```c
   grid[z][dy][dx] = CELL_DIRT;           // Set cell type
   SetWallMaterial(dx, dy, z, MAT_DIRT);  // Set material
   SetWallNatural(dx, dy, z);             // Mark as natural terrain
   ```

5. **Soil types are solid blocks**, not floors:
   - They fill the entire cell volume
   - You stand on top of them (at z+1)
   - They block fluids and provide support

### Implementation Strategy
- Soil drawing creates solid ground cells (CELL_DIRT, CELL_CLAY, etc.)
- Each soil needs both CellType AND material set
- Must mark as "natural" terrain
- Clear any floors that existed (soil replaces air+floor)
- Update surface overlays (grass/bare)

## Implementation Progress

### ✅ Completed
- [x] Check current draw mode implementation location
- [x] Add soil category to draw UI/input handling (input_mode.h/c)
- [x] Implement normal soil painting for all 5 types
  - Added `ACTION_DRAW_SOIL` and sub-actions for each soil type
  - Created `ExecuteBuildSoil()` helper function
  - Hooked up keyboard input (KEY_O for soil menu)
  - Added UI bar items showing soil type selection
- [x] All 5 soil types ready: dirt, clay, gravel, sand, peat

### 🚧 Next Steps
- [x] **Test the basic soil drawing** - BUILD IS WORKING! Ready to test in-game
- [ ] Detect Shift modifier for pile mode
- [ ] Write gravity/drop logic (find first solid support below)
- [ ] Write spreading/piling logic (find adjacent available space)
- [ ] Add ImGui DragFloat for pile radius control
- [ ] Ensure proper walkability updates after terrain changes

### 📝 Build Status
✅ **Build is working!** Compiles successfully with our changes.

## Decisions Made
- ✅ User-controlled radius via slider (not automatic limits)
- ✅ All soil types behave the same (no special spreading behaviors)
- ✅ No particles/effects for now (we don't have that system yet)
