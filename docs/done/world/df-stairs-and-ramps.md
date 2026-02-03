# Plan: DF-Style Stairs and Ramps

**Status: ARCHIVED** - Design doc for Directional ladders and Ramps (both in todo.md). Contains detailed implementation plan.

## Goal
Add Dwarf Fortress-style vertical movement: directional stairs (up-only, down-only, bidirectional) and ramps, with least resistance path.

## Current State

### Existing Z-Transition Systems (Two Separate Implementations)

**1. HPA* System (`LadderLink` in pathfinding.h:39-47)**
```c
typedef struct {
    int x, y;           // Position
    int zLow, zHigh;    // The two z-levels connected
    int entranceLow;    // Entrance index on lower z
    int entranceHigh;   // Entrance index on upper z  
    int cost;           // Cost to climb (default 10)
} LadderLink;
```
- Built in `BuildEntrances()` - scans grid for `CELL_LADDER && CELL_LADDER` pairs
- Creates **bidirectional** edges in `BuildGraph()`
- No directionality field

**2. JPS+ System (`LadderEndpoint` + `JpsLadderGraph` in pathfinding.h:110-148)**
```c
typedef struct {
    int x, y, z;           // Position
    int ladderIndex;       // Which ladder this belongs to
    bool isLow;            // Is this the lower or upper endpoint?
} LadderEndpoint;
```
- Built in `BuildJpsLadderGraph()` - **independently** scans grid (duplicates HPA* work)
- Uses Floyd-Warshall for all-pairs shortest paths
- Creates **bidirectional** climb edges in Step 3
- No directionality field

**3. A* Direct (`RunAStar()` lines ~1240-1290)**
- Checks `grid[z][y][x] == CELL_LADDER` directly
- Tries both z+1 and z-1 unconditionally (bidirectional)

### Key Insight
Both HPA* and JPS+ duplicate the ladder scanning. Neither has directionality support. Adding directional stairs means modifying:
1. Cell type detection (what cells allow up/down)
2. Edge creation (uni vs bidirectional)

---

## Least Resistance Path

### Phase 1: Directional Stairs (Easiest)
Add three stair types. Direction is encoded in cell type, detected at scan time.

**New cell types:**
```c
CELL_STAIRS_UP,    // Can only go UP from here (like DF '<')
CELL_STAIRS_DOWN,  // Can only go DOWN from here (like DF '>')
CELL_STAIRS_BOTH,  // Bidirectional (like current CELL_LADDER)
```

**How direction detection works:**
```
Floor z+1:  [STAIRS_DOWN]  ← "I can receive from below"
                 ↑
Floor z:    [STAIRS_UP  ]  ← "I can send upward"

Result: UP-ONLY connection (can go z→z+1, cannot go z+1→z)
```

The rule: `CanGoUp(lowCell) && CanGoDown(highCell)` = valid upward edge

**Changes needed:**

| File | Location | Change |
|------|----------|--------|
| `grid.h` | enum CellType | Add STAIRS_UP, STAIRS_DOWN, STAIRS_BOTH |
| `grid.c` | IsCellWalkableAt() | Include new stair types |
| `grid.c` | ASCII parser | Add `<`, `>`, `X` symbols |
| `pathfinding.c` | ~line 50 | Add `CanGoUp()`, `CanGoDown()` helpers |
| `pathfinding.c` | RunAStar() ~1240 | Use helpers instead of `== CELL_LADDER` |
| `pathfinding.c` | RunJPS() ~2050 | Same change |
| `pathfinding.c` | BuildEntrances() ~460 | Detect direction, store in LadderLink |
| `pathfinding.c` | BuildGraph() ~620 | Create uni/bidirectional edges |
| `pathfinding.c` | BuildJpsLadderGraph() ~2700 | Create uni/bidirectional edges |

**Estimated changes:** ~80 lines

### Phase 2: Ramps (Medium)
Ramps have **horizontal entry direction** - you enter from one side on z, exit to z+1.

**New cell types:**
```c
CELL_RAMP_N,  // Enter from north (y-1), exit up
CELL_RAMP_E,  // Enter from east (x+1), exit up  
CELL_RAMP_S,  // Enter from south (y+1), exit up
CELL_RAMP_W,  // Enter from west (x-1), exit up
```

**How ramps work:**
```
Z=1:  [floor] [floor] [floor]
                 ↑ exit here
Z=0:  [floor] [RAMP_S] [floor]
                 ↑ enter from south (y+1)
```

**Changes needed:**
1. Add cell types to enum
2. In A*/JPS z-transition: check if movement direction matches ramp orientation
3. In HPA*/JPS+ graph building: create directed edges based on ramp direction
4. Ramps are inherently one-way up (enter low side, exit high side)

**Estimated changes:** ~100 additional lines

### Phase 3: Multi-cell Stairs (Later)
Stairs spanning 2-3 horizontal cells. Deferred - needs more design.

---

## Detailed Implementation: Phase 1

### 1. grid.h - Add cell types
```c
typedef enum {
    CELL_WALKABLE,
    CELL_WALL,
    CELL_LADDER,        // Keep for backward compat
    CELL_AIR,
    CELL_FLOOR,
    CELL_STAIRS_UP,     // Can go UP only (z -> z+1)
    CELL_STAIRS_DOWN,   // Can go DOWN only (z+1 -> z)
    CELL_STAIRS_BOTH,   // Bidirectional
} CellType;
```

### 2. grid.c - Update walkability
```c
bool IsCellWalkableAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || ...) return false;
    CellType c = grid[z][y][x];
    return c == CELL_WALKABLE || c == CELL_FLOOR || 
           c == CELL_LADDER || c == CELL_STAIRS_UP || 
           c == CELL_STAIRS_DOWN || c == CELL_STAIRS_BOTH;
}
```

### 3. grid.c - ASCII parser
```c
case '<': grid[z][row][col] = CELL_STAIRS_UP; break;
case '>': grid[z][row][col] = CELL_STAIRS_DOWN; break;
case 'X': grid[z][row][col] = CELL_STAIRS_BOTH; break;
case 'L': grid[z][row][col] = CELL_LADDER; break;  // Keep working
```

### 4. pathfinding.c - Direction helpers (add near top, ~line 50)
```c
// Check if cell type allows upward z-transition
static bool CanGoUp(CellType cell) {
    return cell == CELL_LADDER || cell == CELL_STAIRS_UP || cell == CELL_STAIRS_BOTH;
}

// Check if cell type allows downward z-transition  
static bool CanGoDown(CellType cell) {
    return cell == CELL_LADDER || cell == CELL_STAIRS_DOWN || cell == CELL_STAIRS_BOTH;
}

// Check if vertical transition is valid between two cells
static bool CanTransitionUp(int x, int y, int zLow) {
    if (zLow + 1 >= gridDepth) return false;
    CellType lowCell = grid[zLow][y][x];
    CellType highCell = grid[zLow + 1][y][x];
    return CanGoUp(lowCell) && CanGoDown(highCell);
}

static bool CanTransitionDown(int x, int y, int zHigh) {
    if (zHigh - 1 < 0) return false;
    CellType highCell = grid[zHigh][y][x];
    CellType lowCell = grid[zHigh - 1][y][x];
    return CanGoDown(highCell) && CanGoUp(lowCell);
}
```

### 5. pathfinding.c - RunAStar() z-transitions (~line 1240)
Replace:
```c
if (grid[bestZ][bestY][bestX] == CELL_LADDER) {
    // Try going up (z+1)
    int nz = bestZ + 1;
    if (nz < gridDepth && grid[nz][bestY][bestX] == CELL_LADDER) {
```

With:
```c
// Try going up (z+1)
if (CanTransitionUp(bestX, bestY, bestZ)) {
    int nz = bestZ + 1;
    // ... existing transition logic
}

// Try going down (z-1)
if (CanTransitionDown(bestX, bestY, bestZ)) {
    int nz = bestZ - 1;
    // ... existing transition logic
}
```

### 6. pathfinding.c - BuildEntrances() ladder detection (~line 460)
Replace:
```c
if (grid[z][y][x] == CELL_LADDER && grid[z + 1][y][x] == CELL_LADDER) {
```

With:
```c
CellType lowCell = grid[z][y][x];
CellType highCell = grid[z + 1][y][x];
bool canUp = CanGoUp(lowCell) && CanGoDown(highCell);
bool canDown = CanGoDown(highCell) && CanGoUp(lowCell);

if (canUp || canDown) {
    // Create ladder link, store direction info
    // ... 
}
```

### 7. pathfinding.c - BuildGraph() edge creation (~line 620)
Replace unconditional bidirectional edges:
```c
graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};
```

With directional edges:
```c
if (canUp) {
    graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};  // low->high
    // Add to adjList[e1]
}
if (canDown) {
    graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};  // high->low
    // Add to adjList[e2]
}
```

### 8. pathfinding.c - BuildJpsLadderGraph() Step 3 (~line 2700)
Same change - create directional edges in `allPairs`:
```c
if (canUp) {
    g->allPairs[lowIdx][highIdx] = climbCost;
    g->next[lowIdx][highIdx] = highIdx;
}
if (canDown) {
    g->allPairs[highIdx][lowIdx] = climbCost;
    g->next[highIdx][lowIdx] = lowIdx;
}
```

---

## Detailed Implementation: Phase 2 (Ramps)

### 1. grid.h - Add ramp cell types
```c
typedef enum {
    // ... existing types ...
    CELL_RAMP_N,  // Enter from north (y-1) on z, exit to z+1
    CELL_RAMP_E,  // Enter from east (x+1) on z, exit to z+1
    CELL_RAMP_S,  // Enter from south (y+1) on z, exit to z+1
    CELL_RAMP_W,  // Enter from west (x-1) on z, exit to z+1
} CellType;
```

### 2. grid.c - ASCII parser for ramps
```c
case '^': grid[z][row][col] = CELL_RAMP_N; break;  // Points up = enter from south
case 'v': grid[z][row][col] = CELL_RAMP_S; break;  // Points down = enter from north
case '{': grid[z][row][col] = CELL_RAMP_W; break;  // Points left = enter from east
case '}': grid[z][row][col] = CELL_RAMP_E; break;  // Points right = enter from west
```

### 3. pathfinding.c - Ramp direction helpers
```c
static bool IsRamp(CellType cell) {
    return cell == CELL_RAMP_N || cell == CELL_RAMP_E || 
           cell == CELL_RAMP_S || cell == CELL_RAMP_W;
}

// Get the entry direction for a ramp (which XY direction you must come from)
static void GetRampEntryDir(CellType cell, int* dx, int* dy) {
    switch (cell) {
        case CELL_RAMP_N: *dx = 0;  *dy = -1; break;  // Enter from north (y-1)
        case CELL_RAMP_E: *dx = 1;  *dy = 0;  break;  // Enter from east (x+1)
        case CELL_RAMP_S: *dx = 0;  *dy = 1;  break;  // Enter from south (y+1)
        case CELL_RAMP_W: *dx = -1; *dy = 0;  break;  // Enter from west (x-1)
        default: *dx = 0; *dy = 0; break;
    }
}

// Check if moving onto ramp from (fromX, fromY) allows z+1 transition
static bool CanEnterRampUp(int rampX, int rampY, int rampZ, int fromX, int fromY) {
    CellType cell = grid[rampZ][rampY][rampX];
    if (!IsRamp(cell)) return false;
    
    int entryDx, entryDy;
    GetRampEntryDir(cell, &entryDx, &entryDy);
    
    // Check if we're coming from the correct direction
    int actualDx = rampX - fromX;
    int actualDy = rampY - fromY;
    
    if (actualDx == entryDx && actualDy == entryDy) {
        // Correct entry direction - check if z+1 is walkable
        if (rampZ + 1 < gridDepth && IsCellWalkableAt(rampZ + 1, rampY, rampX)) {
            return true;
        }
    }
    return false;
}
```

### 4. pathfinding.c - A* ramp handling in neighbor expansion
In RunAStar(), when expanding XY neighbors, check for ramp transitions:
```c
// After normal XY movement to (nx, ny, bestZ):
if (IsRamp(grid[bestZ][ny][nx])) {
    // Check if this movement direction allows going up
    if (CanEnterRampUp(nx, ny, bestZ, bestX, bestY)) {
        // Add z+1 as potential neighbor
        int nz = bestZ + 1;
        // ... add (nx, ny, nz) to open list with appropriate cost
    }
}
```

### 5. HPA*/JPS+ ramp handling
Ramps are trickier for abstract pathfinding because the entry direction matters.

**Option A:** Treat ramps as special entrances that have prerequisite edges
- Ramp at (5,5,0) facing S creates entrance that's only reachable from (5,6,0)
- More complex graph structure

**Option B:** Don't include ramps in abstract graph, handle during refinement
- Simpler, but may miss optimal paths through ramps
- Good enough for initial implementation

Recommend Option B for Phase 2, revisit if needed.

---

## Files to Modify

| File | Lines Changed | Purpose |
|------|---------------|---------|
| `src/grid.h` | ~8 | Add STAIRS_*, RAMP_* to enum |
| `src/grid.c` | ~20 | Update IsCellWalkableAt(), ASCII parser |
| `src/pathfinding.c` | ~100 | Add helpers, update A*, HPA*, JPS+ transitions |

---

## Verification

### Phase 1 Tests (Stairs)

1. **Unit tests:** 
   - Path through STAIRS_UP only goes up
   - Path through STAIRS_DOWN only goes down  
   - STAIRS_BOTH works like current CELL_LADDER
   - Mismatched stairs (UP on both levels) = no connection
   - Invalid direction returns no path

2. **Test map for one-way stairs:**
```
floor:0
##########
#........#
#...<....#   <- STAIRS_UP at (3,2,0)
#........#
##########

floor:1
##########
#........#
#...>....#   <- STAIRS_DOWN at (3,2,1) - matches!
#........#
##########
```
- Path from (1,1,0) to (8,3,1) should go through the stairs UP
- Path from (8,3,1) to (1,1,0) should FAIL (one-way)

3. **Test map for bidirectional:**
```
floor:0
##########
#..X.....#   <- STAIRS_BOTH at (3,1,0)
##########

floor:1
##########
#..X.....#   <- STAIRS_BOTH at (3,1,1)
##########
```
- Both directions should work

### Phase 2 Tests (Ramps)

1. **Test map for ramp:**
```
floor:0
##########
#........#
#..^.....#   <- RAMP_N at (3,2,0) - enter from north (y=1)
#........#
##########

floor:1
##########
#........#
#...F....#   <- FLOOR at (3,2,1) - exit point
#........#
##########
```
- Path from (3,1,0) moving south onto ramp should allow going to (3,2,1)
- Path from (3,3,0) moving north onto ramp should NOT allow z-transition

2. **HPA* test:** Verify abstract graph has correct unidirectional edges

3. **Backward compat:** Existing maps with `L` (CELL_LADDER) still work

---

## Optional Future Work: Unify Ladder Scanning

Currently both `BuildEntrances()` and `BuildJpsLadderGraph()` scan the grid independently. Could extract to shared function:

```c
typedef struct {
    int x, y, zLow, zHigh;
    bool canUp, canDown;
} VerticalLink;

int ScanVerticalLinks(VerticalLink* outLinks, int maxLinks);
```

Both HPA* and JPS+ would call this. Lower priority but reduces duplication (~50 lines of duplicate scanning code).

---

## Summary

| Phase | Feature | Effort | Value |
|-------|---------|--------|-------|
| 1 | Directional stairs (UP/DOWN/BOTH) | ~80 lines | One-way traffic flow, base funneling |
| 2 | Ramps (N/E/S/W entry) | ~100 lines | Natural terrain, hillsides, cliffs |
| 3 | Multi-cell stairs | TBD | Realistic buildings, wider flow |

**Recommended order:** Phase 1 → Phase 2 → Phase 3

Phase 1 is the foundation - directional vertical movement. Phase 2 adds terrain flavor. Phase 3 needs more design work and can wait.
