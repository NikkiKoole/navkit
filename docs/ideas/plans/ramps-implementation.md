# Ramps Implementation Plan

Step-by-step plan for implementing directional ramps with bidirectional z-level transitions.

## Summary

- 4 ramp types: `CELL_RAMP_N`, `CELL_RAMP_E`, `CELL_RAMP_S`, `CELL_RAMP_W`
- Ramp placed on lower z-level, connects to z+1
- Bidirectional: can walk up AND down
- Gradient shading for visuals
- Drawable like ladders (for now)

## Walkability Modes

**Important:** The codebase has two walkability modes that affect ramp behavior.

### Legacy Mode (`g_legacyWalkability = true`)
- Cell itself determines walkability (CELL_FLOOR = walkable, CELL_WALL = blocked)
- Ramps are walkable cells
- Exit tile at z+1 just needs to be a walkable cell type

### DF-Style Mode (`g_legacyWalkability = false`)
- Walkability depends on cell below having floor (`HAS_FLOOR`)
- Air above solid ground is walkable
- **Question:** Does stepping off a ramp onto z+1 require:
  - A) The exit tile has its own floor/solid below? (stricter)
  - B) The ramp itself "provides" walkability to the exit tile? (more lenient)

### Recommendation
Option A (stricter) - the exit tile at z+1 must be independently walkable:
- In DF-style: needs solid below or own floor
- This matches DF behavior where ramps need adjacent wall to "lean against"
- Keeps walkability logic simpler (no special ramp exceptions)

### Code Impact
- `IsCellWalkableAt()` already handles both modes
- Ramp transition check just needs to verify exit tile is walkable
- No special walkability exceptions needed for ramps
- Test both modes in automated tests

## Phase 1: Cell Types (~20 lines)

### 1.1 Add ramp cell types to grid.h

```c
// In CellType enum, add:
CELL_RAMP_N,  // High side north - enter from south on z, exit north on z+1
CELL_RAMP_E,  // High side east - enter from west on z, exit east on z+1
CELL_RAMP_S,  // High side south - enter from north on z, exit south on z+1
CELL_RAMP_W,  // High side west - enter from east on z, exit west on z+1
```

### 1.2 Add ramp flag to cell_defs.h

```c
// Add flag:
#define CF_RAMP 0x10  // Cell is a ramp

// Update CELL_FLAGS array to include ramps with CF_RAMP flag
// Ramps should be walkable on their z-level
```

### 1.3 Update IsCellWalkableAt() in cell_defs.h

Ramps are walkable - movers can stand on them.

```c
// Ramps are walkable (you can stand on them)
// Add CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W to walkable check
```

## Phase 2: Pathfinding Helpers (~50 lines)

### 2.1 Add helper functions to pathfinding.c (near top)

```c
static bool IsRamp(CellType cell) {
    return cell == CELL_RAMP_N || cell == CELL_RAMP_E || 
           cell == CELL_RAMP_S || cell == CELL_RAMP_W;
}

// Get the direction offset for the HIGH side of the ramp (where z+1 exit is)
static void GetRampHighSideOffset(CellType cell, int* dx, int* dy) {
    switch (cell) {
        case CELL_RAMP_N: *dx = 0;  *dy = -1; break;  // North (y-1)
        case CELL_RAMP_E: *dx = 1;  *dy = 0;  break;  // East (x+1)
        case CELL_RAMP_S: *dx = 0;  *dy = 1;  break;  // South (y+1)
        case CELL_RAMP_W: *dx = -1; *dy = 0;  break;  // West (x-1)
        default: *dx = 0; *dy = 0; break;
    }
}

// Check if moving FROM (fromX, fromY, fromZ) TO ramp allows z-transition
// Returns: 0 = no transition, 1 = go up (z+1), -1 = go down (z-1)
static int GetRampZTransition(int rampX, int rampY, int rampZ, int fromX, int fromY, int fromZ) {
    CellType cell = grid[rampZ][rampY][rampX];
    if (!IsRamp(cell)) return 0;
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    int highX = rampX + highDx;
    int highY = rampY + highDy;
    int lowX = rampX - highDx;
    int lowY = rampY - highDy;
    
    // Coming from high side at z+1 -> go down to ramp at z
    if (fromX == highX && fromY == highY && fromZ == rampZ + 1) {
        return -1;  // Going down
    }
    
    // Coming from low side at z -> can go up to z+1
    if (fromX == lowX && fromY == lowY && fromZ == rampZ) {
        // Check if high side at z+1 is walkable
        if (rampZ + 1 < gridDepth && IsCellWalkableAt(rampZ + 1, highY, highX)) {
            return 1;  // Going up
        }
    }
    
    // On ramp, moving to high side -> go up
    if (fromX == rampX && fromY == rampY && fromZ == rampZ) {
        // Moving onto high side tile at z+1
        return 1;
    }
    
    return 0;  // No z-transition
}
```

## Phase 3: A* Support (~40 lines)

### 3.1 Modify RunAStar() neighbor expansion

In the A* neighbor loop, after checking regular XY neighbors, add ramp handling:

```c
// Inside RunAStar(), in the neighbor expansion section:

// Check if current cell is a ramp - add z+1 neighbor
if (IsRamp(grid[bestZ][bestY][bestX])) {
    int highDx, highDy;
    GetRampHighSideOffset(grid[bestZ][bestY][bestX], &highDx, &highDy);
    
    int exitX = bestX + highDx;
    int exitY = bestY + highDy;
    int exitZ = bestZ + 1;
    
    if (exitZ < gridDepth && IsCellWalkableAt(exitZ, exitY, exitX)) {
        // Add (exitX, exitY, exitZ) as neighbor with appropriate cost
        // Cost: diagonal move cost (14) since we move both XY and Z
    }
}

// Check if neighbor is a ramp we can descend onto
// When at z+1 adjacent to high side of ramp at z, can step down onto ramp
for each neighbor (nx, ny) at bestZ {
    // Check if there's a ramp below at z-1 whose high side we're on
    if (bestZ > 0) {
        CellType below = grid[bestZ - 1][ny][nx];
        if (IsRamp(below)) {
            int highDx, highDy;
            GetRampHighSideOffset(below, &highDx, &highDy);
            // If we're on the high side, we can descend
            if (bestX == nx - highDx && bestY == ny - highDy) {
                // Add ramp cell at z-1 as reachable neighbor
            }
        }
    }
}
```

## Phase 4: Drawing/Input (~30 lines)

### 4.1 Add ramp drawing mode to input.c

Similar to ladder drawing, add a ramp mode:

```c
// Add drawing mode for ramps
// Key: 'R' for ramp mode?
// Cycle through directions with additional key or auto-detect based on drag direction?

// Simple approach: R + click places ramp, direction based on which edge you click near
// Or: R cycles through RAMP_N -> RAMP_E -> RAMP_S -> RAMP_W, click to place
```

## Phase 5: Rendering (~40 lines)

### 5.1 Add ramp rendering to rendering.c

```c
// Draw gradient for ramp based on direction
// Darker at low side, lighter at high side

void DrawRampCell(int x, int y, int z, CellType rampType) {
    float cellX = x * CELL_SIZE;
    float cellY = y * CELL_SIZE;
    
    // Base colors
    Color darkColor = (Color){80, 60, 40, 255};   // Low side
    Color lightColor = (Color){160, 140, 120, 255}; // High side
    
    // Draw gradient quad based on direction
    switch (rampType) {
        case CELL_RAMP_N:
            // Dark at south (y+1), light at north (y)
            DrawRectangleGradientV(cellX, cellY, CELL_SIZE, CELL_SIZE, lightColor, darkColor);
            break;
        case CELL_RAMP_S:
            // Dark at north (y), light at south (y+1)
            DrawRectangleGradientV(cellX, cellY, CELL_SIZE, CELL_SIZE, darkColor, lightColor);
            break;
        case CELL_RAMP_E:
            // Dark at west (x), light at east (x+1)
            DrawRectangleGradientH(cellX, cellY, CELL_SIZE, CELL_SIZE, darkColor, lightColor);
            break;
        case CELL_RAMP_W:
            // Dark at east (x+1), light at west (x)
            DrawRectangleGradientH(cellX, cellY, CELL_SIZE, CELL_SIZE, lightColor, darkColor);
            break;
    }
}
```

## Phase 6: HPA*/JPS+ Support (~50 lines)

### 6.1 Update BuildEntrances() for ramps

Ramps create cross-z connections like ladders. Add ramp detection in entrance building:

```c
// In BuildEntrances(), similar to ladder detection:
if (IsRamp(grid[z][y][x])) {
    // Create entrance for the ramp
    // Link to z+1 at the high-side position
}
```

### 6.2 Update graph building for bidirectional edges

```c
// Ramps create bidirectional edges:
// - From ramp (x,y,z) to high-side (x+dx, y+dy, z+1)
// - From high-side (x+dx, y+dy, z+1) to ramp (x,y,z)
```

## Phase 7: Tests (~100 lines)

### 7.1 Add ramp pathfinding tests to test_pathfinding.c

```c
// Test: path up ramp
// Test: path down ramp  
// Test: path blocked when approaching from wrong direction
// Test: ramp at edge of map
// Test: multiple ramps in sequence (switchback)
// Test: ramp + ladder combination paths
```

### 7.2 Add ramp mover tests to test_mover.c

```c
// Test: mover walks up ramp
// Test: mover walks down ramp
// Test: mover pushed out when ramp placed on them
```

### 7.3 Test both walkability modes

```c
// Run ramp tests in BOTH modes:
// - Legacy walkability (g_legacyWalkability = true)
// - DF-style walkability (g_legacyWalkability = false)

// DF-style specific tests:
// Test: ramp exit tile needs solid below to be walkable
// Test: ramp exit onto air (no floor) should fail
// Test: ramp exit onto floor above wall works
```

## Files to Modify

| File | Changes |
|------|---------|
| `src/world/grid.h` | Add CELL_RAMP_N/E/S/W to enum |
| `src/world/cell_defs.h` | Add CF_RAMP flag, update walkability |
| `src/world/pathfinding.c` | Add helpers, update A*, update HPA*/JPS+ |
| `src/core/input.c` | Add ramp drawing mode |
| `src/render/rendering.c` | Add gradient ramp rendering |
| `tests/test_pathfinding.c` | Add ramp tests |
| `tests/test_mover.c` | Add ramp mover tests |

## Implementation Order

1. [ ] Add cell types to grid.h
2. [ ] Add CF_RAMP flag and walkability to cell_defs.h
3. [ ] Add pathfinding helpers (IsRamp, GetRampHighSideOffset, GetRampZTransition)
4. [ ] Update A* for ramp transitions
5. [ ] Add basic ramp rendering (gradient)
6. [ ] Add ramp drawing to input.c
7. [ ] Test manually - walk up/down ramps
8. [ ] Update HPA*/JPS+ for ramp edges
9. [ ] Write automated tests
10. [ ] Polish and edge cases

## Estimated Effort

| Phase | Lines | Time |
|-------|-------|------|
| Cell types | ~20 | Quick |
| Pathfinding helpers | ~50 | Medium |
| A* support | ~40 | Medium |
| Drawing/input | ~30 | Quick |
| Rendering | ~40 | Quick |
| HPA*/JPS+ | ~50 | Medium |
| Tests (both modes) | ~100 | Medium |
| **Total** | ~330 | |
