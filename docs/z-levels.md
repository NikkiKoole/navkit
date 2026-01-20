# Z-Level Navigation in NavKit

## Progress So Far

### Completed Work

**Phase 1: Foundation (Completed)**
- Changed all data structures from 2D to 3D arrays with z=0
- `grid[y][x]` → `grid[z][y][x]`
- `nodeData[y][x]` → `nodeData[z][y][x]`
- `chunkDirty[y][x]` → `chunkDirty[z][y][x]`
- Added `z` field to `Point`, `Entrance`, `AStarNode`, `Mover` structs
- All existing tests pass, behavior unchanged

**Phase 2: Ladders (Completed)**
- Added `CELL_LADDER` cell type (walkable + vertical connection)
- Created `InitMultiFloorGridFromAscii()` parser with `floor:N` markers
- Updated `RunAStar()` to be fully 3D:
  - Searches across all z-levels
  - Adds z±1 neighbors when standing on a ladder cell
  - Requires ladder on BOTH z-levels to create a connection
  - 3D heuristic includes z-distance
- All 6 ladder tests pass

### What Works Now

```
floor:0          floor:1
S.....           ......G
.L....           .L....
......           ......
```

A* finds path: S → ladder → climb → walk to G

### What Doesn't Work Yet

- **HPA*** only operates on z=0, ignores ladders
- **Movers** don't handle z-coordinate
- **Demo** can't visualize multi-floor maps

---

## Design Decision: Option 2 Chosen

**Decision: Keep chunks 2D, handle ladders as separate cross-level links.**

**Rationale:**
- Target: 16 z-levels
- Typical use case: Most paths stay on 1 level
- Performance priority: Don't pay for 16 levels when you only use 1
- Ladders are rare "portal" events, not regular movement

---

## Architecture: Per-Level Graphs with Ladder Links

### Data Structures

```c
// Entrances now per z-level
// entrances[i].z tells us which level it belongs to
Entrance entrances[MAX_ENTRANCES];
int entranceCount;

// Graph edges - already support any entrance-to-entrance connection
GraphEdge graphEdges[MAX_EDGES];
int graphEdgeCount;

// NEW: Track which entrances are ladder endpoints
// A ladder creates TWO entrances (one per z-level) linked together
typedef struct {
    int x, y;           // Position (same on both levels)
    int zLow, zHigh;    // The two z-levels connected
    int entranceLow;    // Entrance index on lower z
    int entranceHigh;   // Entrance index on upper z
    int cost;           // Cost to climb (default 10)
} LadderLink;
LadderLink ladderLinks[MAX_LADDERS];
int ladderLinkCount;
```

### How It Works

**Building the graph:**

1. **Per-level entrance scan**: For each z-level, scan chunk borders as usual
   - Creates entrances with correct z value
   - Horizontal edges within same z-level

2. **Ladder scan**: Find ladder pairs (CELL_LADDER at z and z+1)
   - Create entrance on z (if not already on chunk border)
   - Create entrance on z+1 (if not already on chunk border)
   - Create LadderLink connecting them
   - Add graph edge between the two entrances (cost = climb cost)

3. **Intra-chunk edges**: For each chunk on each z-level
   - Connect entrances within that chunk (local A* for cost)
   - This includes connecting regular entrances to ladder entrances

**Pathfinding:**

1. **Same z-level**: Normal HPA* - barely changes
2. **Different z-level**: 
   - Abstract search finds path through graph
   - Graph includes ladder edges, so path naturally goes: 
     entrance → ... → ladder_low → ladder_high → ... → entrance
   - Refinement handles each segment

**Key insight**: Ladder links are just graph edges. The abstract search doesn't need special z-level logic - it just follows edges. The "magic" is that ladder edges connect entrances on different z-levels.

---

## Implementation Plan

### Step 1: Add LadderLink Structure
- Add to pathfinding.h
- `MAX_LADDERS` constant (1024?)
- `ladderLinks[]` array and `ladderLinkCount`

### Step 2: Update BuildEntrances()
- Keep existing horizontal border scan (but track z-level)
- Add ladder detection pass:
  - For each (x, y, z) where grid[z][y][x] == CELL_LADDER && grid[z+1][y][x] == CELL_LADDER
  - Create/find entrance at (x, y, z)
  - Create/find entrance at (x, y, z+1)
  - Create LadderLink

### Step 3: Update BuildGraph()
- When building edges, consider all entrances in a chunk (including ladder entrances)
- Ladder entrances connect to other entrances in their chunk via local A*
- The ladder-to-ladder edge is added directly (cost = climb cost)

### Step 4: Update RunHPAStar()
- Should mostly work as-is since it follows graph edges
- May need to handle z in refinement phase

### Step 5: Tests
- HPA* finds path across z-levels
- HPA* stays on one level when optimal
- HPA* handles multiple ladders
- Performance: path on single level should be same speed as before

---

## Edge Cases

### Ladder on Chunk Border
A ladder at (x, y) where x or y is on a chunk border:
- It's already an entrance for horizontal movement
- Also becomes a ladder entrance
- Same entrance serves both purposes
- LadderLink references this existing entrance

### Multiple Ladders in Same Chunk
- Each ladder gets its own entrance (or reuses if on border)
- All connect to each other and to border entrances
- Graph handles this naturally

### Ladder Spanning Multiple Z-Levels
Future: elevator going z=0 to z=5?
- Could be single LadderLink with zLow=0, zHigh=5
- Or chain of links: 0→1, 1→2, ... 4→5
- For now: only adjacent z-levels (z and z+1)

### One-Way Transitions
Future: drop-down holes, up-only jumps
- LadderLink gets direction flag: BOTH, UP_ONLY, DOWN_ONLY
- Graph edge becomes directional
- For now: bidirectional only

---

## Performance Analysis

**Single-level path (common case):**
- Entrance count: same as before (only that z-level's entrances matter for intra-level)
- Graph edges: same as before for that level
- Search: same as before
- **Result: No performance penalty**

**Cross-level path (rare case):**
- Additional entrances: ladder endpoints
- Additional edges: ladder links + connections to ladder entrances
- Search: explores more nodes but still hierarchical
- **Result: Slightly slower, but correct**

**Memory:**
- `LadderLink` array: 1024 * ~24 bytes = 24KB
- Extra entrances: minimal (ladders are rare)
- **Result: Negligible**

---

## Questions Resolved

1. **Ladder entrance position**: Two entrances (one per z-level) linked by LadderLink
2. **Edge cost for climbing**: Stored in LadderLink.cost, default 10
3. **Chunk border ladders**: Reuse existing border entrance, add ladder link
4. **One-way ladders**: Future - add direction field to LadderLink
