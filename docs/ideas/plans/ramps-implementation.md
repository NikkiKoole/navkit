# Ramps Implementation Plan

Step-by-step plan for implementing directional ramps with bidirectional z-level transitions.

## Summary

- 4 ramp types: `CELL_RAMP_N`, `CELL_RAMP_E`, `CELL_RAMP_S`, `CELL_RAMP_W`
- Ramp placed on lower z-level, connects to z+1
- Bidirectional: can walk up AND down
- Triangle indicator pointing toward high side (direction of ascent)
- Key 'R' in draw mode for ramp placement

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

## Ramp Movement Rules

### Same Z-Level Movement
- Ramps are walkable cells - movers can step onto them from any direction at the same z-level
- Walking onto a ramp from the side (perpendicular to ramp direction) is blocked
  - Example: `CELL_RAMP_E` (high side east) blocks entry from north or south
  - This prevents awkward "climbing from the side" behavior
- Walking off a ramp to any adjacent walkable cell at the same z is allowed

### Z-Level Transitions
- **Going UP**: Stand on ramp, move toward high side → teleport to exit tile at z+1
- **Going DOWN**: Stand on exit tile at z+1, move toward ramp → teleport onto ramp at z

### Direction Examples
```
CELL_RAMP_N (high side north, y-1):
  - Enter from south (y+1) at z, exit to north (y-1) at z+1
  - Or: enter from north at z+1, exit onto ramp at z
  - Blocked: entering from east or west at same z

CELL_RAMP_E (high side east, x+1):
  - Enter from west (x-1) at z, exit to east (x+1) at z+1
  - Or: enter from east at z+1, exit onto ramp at z
  - Blocked: entering from north or south at same z
```

## Phase 1: Cell Types (~20 lines)

### 1.1 Add ramp cell types to grid.h

```c
// In CellType enum, add:
CELL_RAMP_N,  // High side north - enter from south on z, exit north on z+1
CELL_RAMP_E,  // High side east - enter from west on z, exit east on z+1
CELL_RAMP_S,  // High side south - enter from north on z, exit south on z+1
CELL_RAMP_W,  // High side west - enter from east on z, exit west on z+1
```

### 1.2 CF_RAMP flag already exists

The flag is already defined in `cell_defs.h`:
```c
#define CF_RAMP             (1 << 3)  // Vertical walking - normal speed (stairs)
```

And the accessor macro exists:
```c
#define CellIsRamp(c)       CellHasFlag(c, CF_RAMP)
```

**However:** The current `CellIsRamp()` won't detect directional ramps by default. Update it:

```c
// In cell_defs.h, update or add:
static inline bool CellIsDirectionalRamp(CellType cell) {
    return cell == CELL_RAMP_N || cell == CELL_RAMP_E || 
           cell == CELL_RAMP_S || cell == CELL_RAMP_W;
}
```

### 1.3 Add cell definitions to cell_defs.c

Add entries to the `cellDefs[]` array:

```c
// In cell_defs.c, after ladder definitions:
[CELL_RAMP_N] = {"ramp north", SPRITE_ramp_n, CF_RAMP, INSULATION_TIER_STONE, 0, CELL_RAMP_N},
[CELL_RAMP_E] = {"ramp east",  SPRITE_ramp_e, CF_RAMP, INSULATION_TIER_STONE, 0, CELL_RAMP_E},
[CELL_RAMP_S] = {"ramp south", SPRITE_ramp_s, CF_RAMP, INSULATION_TIER_STONE, 0, CELL_RAMP_S},
[CELL_RAMP_W] = {"ramp west",  SPRITE_ramp_w, CF_RAMP, INSULATION_TIER_STONE, 0, CELL_RAMP_W},
```

**Note:** CF_RAMP alone is sufficient - ramps are self-supporting like ladders (don't need CF_SOLID below).

### 1.4 Walkability already handles CF_RAMP

In `IsCellWalkableAt_Standard()`, ramps are already integrated:
```c
// Ramps are always walkable (special case)
if (CellIsRamp(cellHere)) return true;
```

**No changes needed** - just ensure the new cell types have CF_RAMP flag set.

## Phase 2: Pathfinding Helpers (~60 lines)

### 2.1 Add helper functions to cell_defs.h (near ladder helpers)

```c
// Check if cell is a ramp
static inline bool CellIsRamp(CellType cell) {
    return cell == CELL_RAMP_N || cell == CELL_RAMP_E || 
           cell == CELL_RAMP_S || cell == CELL_RAMP_W;
}

// Get the direction offset for the HIGH side of the ramp (where z+1 exit is)
static inline void GetRampHighSideOffset(CellType cell, int* dx, int* dy) {
    switch (cell) {
        case CELL_RAMP_N: *dx = 0;  *dy = -1; break;  // North (y-1)
        case CELL_RAMP_E: *dx = 1;  *dy = 0;  break;  // East (x+1)
        case CELL_RAMP_S: *dx = 0;  *dy = 1;  break;  // South (y+1)
        case CELL_RAMP_W: *dx = -1; *dy = 0;  break;  // West (x-1)
        default: *dx = 0; *dy = 0; break;
    }
}

// Check if we can walk UP the ramp at (x,y,z) to exit at z+1
// Similar pattern to CanClimbUpAt() for ladders
static inline bool CanWalkUpRampAt(int x, int y, int z) {
    if (z + 1 >= gridDepth) return false;
    
    CellType cell = grid[z][y][x];
    if (!CellIsRamp(cell)) return false;
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    int exitX = x + highDx;
    int exitY = y + highDy;
    
    // Check bounds
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight) {
        return false;
    }
    
    // Exit tile at z+1 must be walkable (works for both walkability modes)
    return IsCellWalkableAt(z + 1, exitY, exitX);
}

// Check if we can walk DOWN onto the ramp at (x,y,z) from z+1
// We're standing at the exit tile at z+1, checking if we can descend
static inline bool CanWalkDownRampAt(int x, int y, int z) {
    if (z < 0) return false;
    
    CellType cell = grid[z][y][x];
    if (!CellIsRamp(cell)) return false;
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    int exitX = x + highDx;
    int exitY = y + highDy;
    
    // Check bounds for where we'd be coming from
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight) {
        return false;
    }
    
    // The ramp itself must be walkable (should always be true for valid ramps)
    return IsCellWalkableAt(z, y, x);
}

// Check if moving from (fromX, fromY) to ramp at (rampX, rampY) at same z is allowed
// Blocks side entry (perpendicular to ramp direction)
static inline bool CanEnterRampFromSide(int rampX, int rampY, int z, int fromX, int fromY) {
    CellType cell = grid[z][rampY][rampX];
    if (!CellIsRamp(cell)) return true;  // Not a ramp, allow normal movement
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    // Calculate movement direction
    int moveDx = rampX - fromX;
    int moveDy = rampY - fromY;
    
    // Allow entry from low side (opposite of high) or high side
    // Block entry from perpendicular directions
    // Low side: (-highDx, -highDy), High side: (highDx, highDy)
    bool fromLowSide = (moveDx == highDx && moveDy == highDy);   // Moving toward high = from low
    bool fromHighSide = (moveDx == -highDx && moveDy == -highDy); // Moving toward low = from high
    
    return fromLowSide || fromHighSide;
}
```

## Phase 3: A* Support (~50 lines)

### 3.1 Modify RunAStar() neighbor expansion

In the A* neighbor loop, add ramp handling similar to ladder handling.

```c
// Inside RunAStar(), after the existing ladder z-transition code:

// === RAMP Z-TRANSITIONS ===

// Check if current cell is a ramp - can go UP to z+1
if (CanWalkUpRampAt(bestX, bestY, bestZ)) {
    int highDx, highDy;
    GetRampHighSideOffset(grid[bestZ][bestY][bestX], &highDx, &highDy);
    
    int exitX = bestX + highDx;
    int exitY = bestY + highDy;
    int exitZ = bestZ + 1;
    
    // Add exit tile as neighbor
    // Cost: 14 (diagonal cost) since we move both XY and Z
    if (!nodeData[exitZ][exitY][exitX].closed) {
        int moveCost = 14;
        int ng = nodeData[bestZ][bestY][bestX].g + moveCost;
        if (ng < nodeData[exitZ][exitY][exitX].g) {
            nodeData[exitZ][exitY][exitX].g = ng;
            nodeData[exitZ][exitY][exitX].f = ng + Heuristic3D(exitX, exitY, exitZ, goalX, goalY, goalZ);
            nodeData[exitZ][exitY][exitX].parentX = bestX;
            nodeData[exitZ][exitY][exitX].parentY = bestY;
            nodeData[exitZ][exitY][exitX].parentZ = bestZ;
            nodeData[exitZ][exitY][exitX].open = true;
            // Add to open list...
        }
    }
}

// Check if there's a ramp below we can descend onto
// For each ramp at z-1, check if we're standing on its high-side exit tile
if (bestZ > 0) {
    // Check the 4 potential ramp positions that could connect to us
    int checkOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // N, E, S, W
    CellType matchingRamps[4] = {CELL_RAMP_S, CELL_RAMP_W, CELL_RAMP_N, CELL_RAMP_E};
    
    for (int i = 0; i < 4; i++) {
        int rampX = bestX + checkOffsets[i][0];
        int rampY = bestY + checkOffsets[i][1];
        int rampZ = bestZ - 1;
        
        if (rampX < 0 || rampX >= gridWidth || rampY < 0 || rampY >= gridHeight) continue;
        
        CellType below = grid[rampZ][rampY][rampX];
        
        // Check if this ramp's high side points to our current position
        if (below == matchingRamps[i]) {
            // We can descend onto this ramp
            if (!nodeData[rampZ][rampY][rampX].closed && IsCellWalkableAt(rampZ, rampY, rampX)) {
                int moveCost = 14;
                int ng = nodeData[bestZ][bestY][bestX].g + moveCost;
                if (ng < nodeData[rampZ][rampY][rampX].g) {
                    nodeData[rampZ][rampY][rampX].g = ng;
                    nodeData[rampZ][rampY][rampX].f = ng + Heuristic3D(rampX, rampY, rampZ, goalX, goalY, goalZ);
                    nodeData[rampZ][rampY][rampX].parentX = bestX;
                    nodeData[rampZ][rampY][rampX].parentY = bestY;
                    nodeData[rampZ][rampY][rampX].parentZ = bestZ;
                    nodeData[rampZ][rampY][rampX].open = true;
                    // Add to open list...
                }
            }
        }
    }
}

// When expanding XY neighbors at same z, block side entry to ramps
// In the existing neighbor loop, add this check:
if (!CanEnterRampFromSide(nx, ny, bestZ, bestX, bestY)) {
    continue;  // Skip this neighbor - can't enter ramp from the side
}
```

## Phase 4: Drawing/Input (~60 lines)

### 4.1 Add ramp drawing mode to input.c

Currently used keys in MODE_DRAW: W (Wall), F (Floor), L (Ladder), S (Stockpile), I (Dirt), T (Workshop).

**Use 'R' for Ramp mode**, then sub-keys for direction:

```c
// In input.c, MODE_DRAW section:
static CellType selectedRampDirection = CELL_RAMP_N;  // Default direction

if (CheckKey(KEY_R)) {
    inputAction = ACTION_DRAW_RAMP;
    
    // Sub-keys to select direction (while R is held or after R pressed)
    if (CheckKey(KEY_UP) || CheckKey(KEY_K))    selectedRampDirection = CELL_RAMP_N;
    if (CheckKey(KEY_RIGHT) || CheckKey(KEY_L)) selectedRampDirection = CELL_RAMP_E;
    if (CheckKey(KEY_DOWN) || CheckKey(KEY_J))  selectedRampDirection = CELL_RAMP_S;
    if (CheckKey(KEY_LEFT) || CheckKey(KEY_H))  selectedRampDirection = CELL_RAMP_W;
}

// Alternative: cycle through directions with repeated R presses
// Or: R+1=N, R+2=E, R+3=S, R+4=W
```

### 4.2 Validate ramp placement

Only allow placing a ramp if the terrain supports it:

```c
bool CanPlaceRamp(int x, int y, int z, CellType rampType) {
    // 1. Current cell must be walkable (floor, not wall/water)
    if (!IsCellWalkableAt(z, y, x)) return false;
    
    // 2. Current cell must not already be a ramp or ladder
    CellType current = grid[z][y][x];
    if (CellIsRamp(current) || CellIsLadder(current)) return false;
    
    // 3. Exit tile at z+1 must be walkable
    int highDx, highDy;
    GetRampHighSideOffset(rampType, &highDx, &highDy);
    int exitX = x + highDx;
    int exitY = y + highDy;
    
    // Check bounds
    if (z + 1 >= gridDepth) return false;
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight) return false;
    
    // Exit must be walkable
    if (!IsCellWalkableAt(z + 1, exitY, exitX)) return false;
    
    // 4. Low side at same z should be walkable (so you can enter the ramp)
    int lowX = x - highDx;
    int lowY = y - highDy;
    if (lowX >= 0 && lowX < gridWidth && lowY >= 0 && lowY < gridHeight) {
        if (!IsCellWalkableAt(z, lowY, lowX)) return false;
    }
    
    return true;
}
```

### 4.3 Push movers/items when ramp is placed

When a ramp is placed (similar to wall placement in `CompleteBlueprint`):

```c
void PlaceRamp(int x, int y, int z, CellType rampType) {
    if (!CanPlaceRamp(x, y, z, rampType)) return;
    
    // Call the existing push functions before placing the ramp
    PushMoversOutOfCell(x, y, z);
    PushItemsOutOfCell(x, y, z);
    
    // Place the ramp
    grid[z][y][x] = rampType;
    MarkChunkDirty(x, y, z);
}
```

## Phase 5: Rendering (~40 lines)

Simple triangle rendering pointing toward the high side (direction of ascent):

```c
void DrawRampCell(int x, int y, int z, CellType rampType) {
    float cellX = x * CELL_SIZE;
    float cellY = y * CELL_SIZE;
    float cx = cellX + CELL_SIZE * 0.5f;  // Center x
    float cy = cellY + CELL_SIZE * 0.5f;  // Center y
    
    Color rampColor = (Color){139, 119, 101, 255};  // Brownish
    
    // Draw base rectangle
    DrawRectangle(cellX, cellY, CELL_SIZE, CELL_SIZE, rampColor);
    
    // Draw triangle pointing toward high side
    Color arrowColor = (Color){80, 60, 40, 255};  // Darker brown
    float halfSize = CELL_SIZE * 0.3f;
    
    Vector2 p1, p2, p3;  // Triangle points: tip at high side, base at low side
    
    switch (rampType) {
        case CELL_RAMP_N:  // Points north (up on screen, y-)
            p1 = (Vector2){cx, cellY + CELL_SIZE * 0.2f};           // Tip (north)
            p2 = (Vector2){cx - halfSize, cellY + CELL_SIZE * 0.8f}; // Base left
            p3 = (Vector2){cx + halfSize, cellY + CELL_SIZE * 0.8f}; // Base right
            break;
        case CELL_RAMP_S:  // Points south (down on screen, y+)
            p1 = (Vector2){cx, cellY + CELL_SIZE * 0.8f};           // Tip (south)
            p2 = (Vector2){cx - halfSize, cellY + CELL_SIZE * 0.2f}; // Base left
            p3 = (Vector2){cx + halfSize, cellY + CELL_SIZE * 0.2f}; // Base right
            break;
        case CELL_RAMP_E:  // Points east (right on screen, x+)
            p1 = (Vector2){cellX + CELL_SIZE * 0.8f, cy};           // Tip (east)
            p2 = (Vector2){cellX + CELL_SIZE * 0.2f, cy - halfSize}; // Base top
            p3 = (Vector2){cellX + CELL_SIZE * 0.2f, cy + halfSize}; // Base bottom
            break;
        case CELL_RAMP_W:  // Points west (left on screen, x-)
            p1 = (Vector2){cellX + CELL_SIZE * 0.2f, cy};           // Tip (west)
            p2 = (Vector2){cellX + CELL_SIZE * 0.8f, cy - halfSize}; // Base top
            p3 = (Vector2){cellX + CELL_SIZE * 0.8f, cy + halfSize}; // Base bottom
            break;
        default:
            return;
    }
    
    DrawTriangle(p1, p2, p3, arrowColor);
}
```

## Phase 6: HPA*/JPS+ Support (~80 lines)

This is the most complex part. Follow the ladder pattern closely.

### 6.1 Add RampLink structure (pathfinding.h)

```c
// Similar to LadderLink but with direction info
typedef struct {
    int rampX, rampY, rampZ;  // Ramp position (on lower z)
    int exitX, exitY;          // Exit position at z+1 (high side)
    int entranceRamp;          // Entrance index for ramp cell
    int entranceExit;          // Entrance index for exit cell at z+1
    int cost;                  // Movement cost (14 for diagonal-equivalent)
    CellType rampType;         // Which direction ramp
} RampLink;

extern RampLink rampLinks[MAX_RAMPS];
extern int rampLinkCount;
```

### 6.2 Update BuildEntrances() for ramps

Use the same pattern as `AddLadderEntrance()`:

```c
// AddLadderEntrance pattern (from pathfinding.c):
static int AddLadderEntrance(int x, int y, int z) {
    if (entranceCount >= MAX_ENTRANCES) return -1;
    int chunk = z * (chunksX * chunksY) + (y / chunkHeight) * chunksX + (x / chunkWidth);
    entrances[entranceCount] = (Entrance){x, y, z, chunk, chunk};
    return entranceCount++;
}

// For ramps, reuse the same function or create AddRampEntrance with identical logic:
static int AddRampEntrance(int x, int y, int z) {
    if (entranceCount >= MAX_ENTRANCES) return -1;
    int chunk = z * (chunksX * chunksY) + (y / chunkHeight) * chunksX + (x / chunkWidth);
    entrances[entranceCount] = (Entrance){x, y, z, chunk, chunk};
    return entranceCount++;
}
```

In BuildEntrances(), after ladder detection:

```c
// Reset ramp link count
rampLinkCount = 0;

// Detect ramps and create ramp links
for (int z = 0; z < gridDepth - 1; z++) {
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (CanWalkUpRampAt(x, y, z)) {
                CellType cell = grid[z][y][x];
                int highDx, highDy;
                GetRampHighSideOffset(cell, &highDx, &highDy);
                
                int exitX = x + highDx;
                int exitY = y + highDy;
                
                if (rampLinkCount < MAX_RAMPS) {
                    // Create entrances for both the ramp and exit tile
                    int entRamp = AddRampEntrance(x, y, z);
                    int entExit = AddRampEntrance(exitX, exitY, z + 1);
                    
                    if (entRamp >= 0 && entExit >= 0) {
                        rampLinks[rampLinkCount++] = (RampLink){
                            .rampX = x, .rampY = y, .rampZ = z,
                            .exitX = exitX, .exitY = exitY,
                            .entranceRamp = entRamp,
                            .entranceExit = entExit,
                            .cost = 14,
                            .rampType = cell
                        };
                    }
                }
            }
        }
    }
}
```

### 6.3 Update BuildGraph() for ramp edges

```c
// Add edges for ramp links (bidirectional cross z-level connections)
for (int i = 0; i < rampLinkCount; i++) {
    RampLink* link = &rampLinks[i];
    int e1 = link->entranceRamp;
    int e2 = link->entranceExit;
    int cost = link->cost;
    
    // Add bidirectional edges (same pattern as ladders)
    if (graphEdgeCount < MAX_EDGES - 1) {
        graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
        graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};
        
        if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
            adjList[e1][adjListCount[e1]++] = graphEdgeCount - 2;
        }
        if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
            adjList[e2][adjListCount[e2]++] = graphEdgeCount - 1;
        }
    }
}
```

### 6.4 Update JPS+ ladder graph for ramps

Extend `BuildJpsLadderGraph()` to also detect ramps (or create separate function):

```c
// In BuildJpsLadderGraph(), after ladder endpoint creation:
// Add ramp endpoints using the same pattern

int rampIndex = ladderIndex;  // Continue indexing from ladders

for (int z = 0; z < gridDepth - 1; z++) {
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (CanWalkUpRampAt(x, y, z)) {
                if (g->endpointCount + 2 > MAX_LADDER_ENDPOINTS) {
                    TraceLog(LOG_WARNING, "JPS+ graph: too many endpoints (ramps)");
                    return;
                }
                
                CellType cell = grid[z][y][x];
                int highDx, highDy;
                GetRampHighSideOffset(cell, &highDx, &highDy);
                
                int exitX = x + highDx;
                int exitY = y + highDy;
                
                // Create endpoint for ramp position (lower z)
                int idxRamp = g->endpointCount++;
                g->endpoints[idxRamp] = (LadderEndpoint){x, y, z, rampIndex, true};
                int levelLow = z;
                if (g->endpointsPerLevelCount[levelLow] < MAX_ENDPOINTS_PER_LEVEL) {
                    g->endpointsByLevel[levelLow][g->endpointsPerLevelCount[levelLow]++] = idxRamp;
                }
                
                // Create endpoint for exit position (upper z)
                int idxExit = g->endpointCount++;
                g->endpoints[idxExit] = (LadderEndpoint){exitX, exitY, z + 1, rampIndex, false};
                int levelHigh = z + 1;
                if (g->endpointsPerLevelCount[levelHigh] < MAX_ENDPOINTS_PER_LEVEL) {
                    g->endpointsByLevel[levelHigh][g->endpointsPerLevelCount[levelHigh]++] = idxExit;
                }
                
                // Bidirectional edge between endpoints (cost 14)
                if (g->edgeCount + 2 < MAX_LADDER_EDGES) {
                    g->edges[g->edgeCount++] = (JpsLadderEdge){idxRamp, idxExit, 14};
                    g->edges[g->edgeCount++] = (JpsLadderEdge){idxExit, idxRamp, 14};
                }
                
                rampIndex++;
            }
        }
    }
}
```

### 6.5 Trigger graph rebuild when ramps change

Graph rebuilds are triggered automatically via `MarkChunkDirty()`:

```c
// When placing a ramp:
void PlaceRamp(int x, int y, int z, CellType rampType) {
    // ... validation and placement ...
    grid[z][y][x] = rampType;
    MarkChunkDirty(x, y, z);  // This triggers HPA* incremental update
}

// When erasing a ramp:
void EraseRamp(int x, int y, int z) {
    // ... validation ...
    grid[z][y][x] = g_legacyWalkability ? CELL_WALKABLE : CELL_AIR;
    MarkChunkDirty(x, y, z);  // This triggers HPA* incremental update
}
```

The existing update loop handles the rest:
```c
// In main update loop (already exists):
if (moverPathAlgorithm == PATH_ALGO_HPA && hpaNeedsRebuild) {
    UpdateDirtyChunks();  // Rebuilds affected chunks including ramp links
}
```

### 6.6 Add EraseRamp() function

Simpler than ladders (no cascading needed since ramps are single-cell):

```c
void EraseRamp(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return;  // Only erase if it's a directional ramp
    
    // Replace with appropriate floor type
    if (g_legacyWalkability) {
        grid[z][y][x] = CELL_WALKABLE;
    } else {
        grid[z][y][x] = CELL_AIR;  // DF-style: air above solid is walkable
    }
    
    MarkChunkDirty(x, y, z);
}
```

## Phase 7: Tests (~250 lines)

Tests mirror the ladder test structure for consistency.

### 7.1 Basic A* Ramp Pathfinding (test_pathfinding.c)

```c
describe(ramp_pathfinding) {
    // Ramp tests use legacy walkability - they test ramp mechanics, not DF walkability
    
    it("should find path using ramp to reach upper floor") {
        // CELL_RAMP_N at (5,5,0) - high side north
        // Start south of ramp on z=0, goal north of ramp on z=1
        // Path: (5,6,0) -> (5,5,0) -> (5,4,1)
    }
    
    it("should find path down ramp to reach lower floor") {
        // Reverse: start at z=1 north of ramp, goal at z=0 south of ramp
        // Path: (5,4,1) -> (5,5,0) -> (5,6,0)
    }
    
    it("should stay on same floor when ramp not needed") {
        // Start and goal on floor 0, ramp exists but not needed
        // Verify path stays on z=0
    }
    
    it("should not find path when exit tile is blocked") {
        // Ramp at (5,5,0) with wall at exit tile (5,4,1)
        // Should not find path to z=1
    }
    
    it("should choose closer ramp when multiple exist") {
        // Two ramps - start near left, goal near right ramp
        // Verify path uses the closer ramp
    }
    
    it("should find path when ramp destination has alternate route") {
        // Ramp leads to area with walls but can go around
    }
    
    it("should block side entry to ramp") {
        // CELL_RAMP_E at (5,5,0) - high side east
        // Try to path from (5,4,0) directly to ramp
        // Should go around via low side (4,5,0) or high side (6,5,0)
    }
    
    it("should allow entry from low side") {
        // CELL_RAMP_E at (5,5,0)
        // Path from (4,5,0) to ramp should succeed
    }
    
    it("should allow entry from high side at same z") {
        // CELL_RAMP_E at (5,5,0)
        // Path from (6,5,0) to ramp at same z should succeed
    }
    
    it("should find path with ramp switchback (multiple z-levels)") {
        // CELL_RAMP_E at (5,5,0), CELL_RAMP_W at (5,5,1)
        // Path from z=0 to z=2 using two ramps
    }
    
    it("should find path using both ramp and ladder") {
        // Ramp at one location, ladder at another
        // Verify path can use either/both
    }
    
    it("should test all four ramp directions") {
        // CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W
        // Verify each direction works correctly
    }
}
```

### 7.2 HPA* Ramp Pathfinding (test_pathfinding.c)

```c
describe(hpa_ramp_pathfinding) {
    it("should build ramp links when entrances are built") {
        // Place ramp, build entrances
        // Verify rampLinkCount == 1
        // Verify rampLinks[0] has correct position and exit
    }
    
    it("should connect ramp entrances in graph") {
        // Build entrances and graph
        // Verify bidirectional edges exist crossing z-levels
        // Should have 2 edges (up and down)
    }
    
    it("should find HPA* path using ramp to reach upper floor") {
        // Grid with multiple chunks to ensure HPA* uses abstract graph
        // Verify path crosses z-level via ramp
    }
    
    it("should match A* path for simple ramp crossing") {
        // Run both A* and HPA* on same map
        // Both should find path with exactly 1 z-transition
    }
    
    it("should find path after ramp added via incremental update") {
        // Start with no ramp - path between floors should fail
        // Add ramp, mark chunk dirty, update
        // Path should now succeed
    }
    
    it("should update path when ramp is removed via incremental update") {
        // Start with ramp - path works
        // Remove ramp, mark chunk dirty, update
        // Path should now fail
    }
    
    it("incremental ramp update should match full rebuild") {
        // Add ramp via incremental update
        // Compare rampLinkCount and positions to full rebuild
        // Should match exactly
    }
    
    it("repeated wall edits near ramps should not grow entrance count") {
        // Bug prevention: ensure entrance count stays stable
        // Add ramp, repeatedly add/remove walls nearby
        // entranceCount and rampLinkCount should not leak
    }
}
```

### 7.3 JPS+ 3D Ramp Pathfinding (test_pathfinding.c)

```c
describe(jps_plus_ramp_pathfinding) {
    it("should find path across z-levels using ramp graph") {
        // Multi-floor map with ramp
        // Verify JPS+ finds path with z-transition
    }
    
    it("JPS+ should find same route as A* for ramp crossing") {
        // Run both pathfinders on same map
        // Start and end positions should match
        // Both should have z-transitions
    }
    
    it("should not find path when no ramp connects levels") {
        // Two floors with no ramp
        // JPS+ should return pathLength == 0
    }
}
```

### 7.4 Ramp Mover Tests (test_mover.c)

```c
describe(ramp_mover) {
    it("mover walks up ramp correctly") {
        // Create mover, set goal on z+1
        // Simulate movement, verify z-level changes
    }
    
    it("mover walks down ramp correctly") {
        // Create mover at z+1, set goal on z
        // Verify mover ends up at correct z-level
    }
    
    it("mover pushed out when ramp placed") {
        // Mover standing at (5,5,0)
        // Place ramp at (5,5,0)
        // Verify mover pushed to adjacent walkable cell
        // Verify mover.needsRepath == true
    }
    
    it("item pushed out when ramp placed") {
        // Item at (5,5,0)
        // Place ramp at (5,5,0)
        // Verify item pushed to adjacent walkable cell
    }
}
```

### 7.5 DF-Style Walkability Ramp Tests (test_pathfinding.c)

```c
describe(df_ramp_pathfinding) {
    // These tests only run when g_legacyWalkability = false
    
    it("should make ramp cells walkable regardless of below") {
        // Ramp at z=1 with no solid below
        // Ramp itself should be walkable (CF_WALKABLE)
    }
    
    it("should find path up ramp in DF mode") {
        // DF-style terrain: ground at z=0, walk at z=1
        // Ramp connects z=1 to z=2
        // Exit tile at z=2 needs solid below
        // Verify path works
    }
    
    it("should not find path when ramp exit has no floor in DF mode") {
        // Ramp at z=1, exit tile at z=2 has no solid below
        // Path should fail (exit tile not walkable in DF mode)
    }
    
    it("should find path when ramp exit is above solid") {
        // Ramp at z=1, wall at z=1 under exit position
        // Exit tile at z=2 is walkable (has solid below)
        // Path should succeed
    }
}
```

### 7.6 Edge Cases (test_pathfinding.c)

```c
describe(ramp_edge_cases) {
    it("ramp at map edge should not crash") {
        // CELL_RAMP_N at y=0 - exit would be y=-1
        // Should gracefully fail, not crash
    }
    
    it("ramp pointing out of map bounds") {
        // CELL_RAMP_E at x=gridWidth-1
        // CanWalkUpRampAt should return false
    }
    
    it("ramp at z=gridDepth-1 should not allow up") {
        // Ramp at top z-level
        // No z+1 exists, should not allow transition
    }
    
    it("multiple ramps adjacent to each other") {
        // Two ramps side by side
        // Verify pathfinding handles correctly
    }
    
    it("ramp with blocked low side entry") {
        // CELL_RAMP_E at (5,5,0), wall at (4,5,0)
        // Can only enter from high side or z+1
    }
}
```

## Phase 8: Inspect Support (~15 lines)

Update `src/core/inspect.c` to display ramp information.

### 8.1 Add ramp names to cellTypeNames array

```c
static const char* cellTypeNames[] = {
    "WALKABLE",      // 0
    "WALL",          // 1
    "LADDER",        // 2
    // ... existing entries ...
    "RAMP_N",        // Add after existing entries
    "RAMP_E",
    "RAMP_S",
    "RAMP_W",
};
```

**Note:** The array indices must match the CellType enum values.

### 8.2 Walkability output already handles ramps

The code already has:
```c
if (CellIsLadder(cell)) printf(" (ladder)");
else if (CellIsRamp(cell)) printf(" (ramp)");
```

This will work if `CellIsRamp()` returns true for directional ramps (via CF_RAMP flag).

### 8.3 Add ramp character to ASCII map visualization

```c
// In the switch for cell visualization:
case CELL_RAMP_N:
case CELL_RAMP_E:
case CELL_RAMP_S:
case CELL_RAMP_W:
    c = '^';  // Or use direction-specific: ^>v<
    break;
```

Alternative: use direction-specific characters:
```c
case CELL_RAMP_N: c = '^'; break;  // Points up (north)
case CELL_RAMP_E: c = '>'; break;  // Points right (east)
case CELL_RAMP_S: c = 'v'; break;  // Points down (south)
case CELL_RAMP_W: c = '<'; break;  // Points left (west)
```

### 8.4 Update legend

```c
printf("\nLegend: # wall, . floor, , grass, : dirt, ~ water, X dig, H ladder, ^>v< ramp, @ center\n");
```

## Save/Load

**No changes needed.** The grid is saved/loaded as a flat array of CellType values:

```c
// In saveload.c - cells are saved directly:
for (int z = 0; z < gridDepth; z++) {
    for (int y = 0; y < gridHeight; y++) {
        fwrite(grid[z][y], sizeof(CellType), gridWidth, f);
    }
}
```

New cell types (CELL_RAMP_N, etc.) are automatically included since they're just enum values.

## Files to Modify

| File | Changes |
|------|---------|
| `src/world/grid.h` | Add CELL_RAMP_N/E/S/W to CellType enum |
| `src/world/grid.c` | Add PlaceRamp(), EraseRamp() functions |
| `src/world/cell_defs.h` | Add CellIsDirectionalRamp(), GetRampHighSideOffset(), CanWalkUpRampAt(), CanWalkDownRampAt(), CanEnterRampFromSide() |
| `src/world/cell_defs.c` | Add cellDefs entries for CELL_RAMP_N/E/S/W |
| `src/world/pathfinding.h` | Add RampLink struct, MAX_RAMPS, rampLinks array, rampLinkCount |
| `src/world/pathfinding.c` | Add A* ramp transitions, AddRampEntrance(), ramp detection in BuildEntrances(), ramp edges in BuildGraph(), ramp endpoints in BuildJpsLadderGraph() |
| `src/core/input.c` | Add ACTION_DRAW_RAMP, KEY_R handling, direction selection |
| `src/core/inspect.c` | Add RAMP_N/E/S/W to cellTypeNames[], add ^>v< visualization, update legend |
| `src/render/rendering.c` | Add DrawRampCell() with triangle rendering |
| `tests/test_pathfinding.c` | Add ramp_pathfinding, hpa_ramp_pathfinding, jps_plus_ramp_pathfinding, df_ramp_pathfinding, ramp_edge_cases test suites |
| `tests/test_mover.c` | Add ramp_mover test suite |

## Implementation Order

1. [ ] Add CELL_RAMP_N/E/S/W to CellType enum in grid.h
2. [ ] Add cellDefs entries in cell_defs.c (name, sprite placeholder, CF_RAMP flag)
3. [ ] Add CellIsDirectionalRamp() helper to cell_defs.h
4. [ ] Add GetRampHighSideOffset(), CanWalkUpRampAt(), CanWalkDownRampAt() to cell_defs.h
5. [ ] Add CanEnterRampFromSide() for side-entry blocking
6. [ ] Update RunAStar() for ramp z-transitions (up and down)
7. [ ] Add CanPlaceRamp(), PlaceRamp(), EraseRamp() to grid.c
8. [ ] Add KEY_R handling and direction selection to input.c
9. [ ] Add DrawRampCell() with triangle rendering to rendering.c
10. [ ] Update inspect.c: cellTypeNames[], ^>v< visualization, legend
11. [ ] Test manually - place ramps, walk up/down, verify directions, inspect
12. [ ] Add RampLink struct, MAX_RAMPS, rampLinks[], rampLinkCount to pathfinding.h
13. [ ] Add AddRampEntrance() and ramp detection to BuildEntrances()
14. [ ] Add ramp edges to BuildGraph()
15. [ ] Add ramp endpoints to BuildJpsLadderGraph()
16. [ ] Write automated tests (A*, HPA*, JPS+, mover, DF-style, edge cases)
17. [ ] Test both walkability modes
18. [ ] Polish and edge cases

## Estimated Effort

| Phase | Lines | Complexity |
|-------|-------|------------|
| Cell types & definitions | ~30 | Low |
| Pathfinding helpers | ~70 | Medium |
| A* support | ~50 | Medium |
| Drawing/input | ~60 | Low |
| Rendering | ~40 | Low |
| Place/Erase functions | ~40 | Low |
| Inspect support | ~15 | Low |
| HPA*/JPS+ | ~100 | High |
| Tests | ~250 | Medium |
| **Total** | ~655 | |
