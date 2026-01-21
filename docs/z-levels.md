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

**Phase 3: HPA* Z-Level Support (Completed)**
- Added `LadderLink` structure for cross-level connections
- Updated chunk functions (`GetChunk`, `GetChunkBounds`) to include z in chunk IDs
- Updated `BuildEntrances()` to scan all z-levels and detect ladders
- Updated `BuildGraph()` to build per-level graphs with ladder link edges
- Updated `AStarChunk()` and `AStarChunkMultiTarget()` with z parameter
- Updated `ReconstructLocalPathWithBounds()` to operate on correct z-level
- Updated `RunHPAStar()` refinement phase to handle z-level transitions
- All 4 HPA* ladder tests pass (47 total tests, 76 assertions)

### What Works Now

```
floor:0          floor:1
S.....           ......G
.L....           .L....
......           ......
```

Both A* and HPA* find path: S → ladder → climb → walk to G

**Phase 4: Mover Z-Level Support (Completed)**
- Movers track z-coordinate during movement
- Movers climb ladders and transition between z-levels
- `ProcessMoverRepaths()` uses mover's z in `startPos`
- String pulling respects z-level transitions (won't skip ladder waypoints)
- "Prefer Diff Z" toggle for testing (movers prefer goals on different z-levels)

**Phase 5: Demo Visualization (Completed)**
- Layer switching UI with keyboard shortcuts
- Ladder cells drawn distinctly
- Multi-floor path display with visual indicators

### What Doesn't Work Yet

- **RunAStar** performance is slow (O(n³) best-node search across all z-levels)

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

### ✅ Step 1: Add LadderLink Structure (DONE)
- Added to pathfinding.h
- `MAX_LADDERS` = 1024
- `ladderLinks[]` array and `ladderLinkCount`

### ✅ Step 2: Update BuildEntrances() (DONE)
- Scans all z-levels for chunk border entrances
- Detects ladder pairs and creates LadderLinks
- Creates ladder entrances on both z-levels

### ✅ Step 3: Update BuildGraph() (DONE)
- Iterates over all z-level chunks
- Connects entrances within same z-level via local A*
- Adds ladder link edges for cross-level connections

### ✅ Step 4: Update RunHPAStar() (DONE)
- Works with z-level aware chunk functions
- Refinement handles z-level transitions correctly

### ✅ Step 5: Tests (DONE)
- HPA* finds path across z-levels ✓
- HPA* produces same z-level transitions as A* ✓
- Ladder links are built correctly ✓
- Graph edges include ladder connections ✓

---

## Next Steps

### Phase 6: Performance Optimization
`RunAStar()` is slow with multiple z-levels due to O(n³) best-node search:

1. **Add priority queue for 3D A***
   - Replace linear scan with heap
   - Currently iterates all `gridDepth * gridHeight * gridWidth` nodes each step

2. **Consider lazy initialization**
   - Only initialize nodeData for z-levels actually used
   - Most paths stay on one level

### Future Considerations

- **One-way transitions**: Drop-down holes, up-only jumps (add direction to LadderLink)
- **Multi-level ladders**: Elevators going z=0 to z=5 directly
- **Incremental updates for ladders**: Currently ladder changes require full rebuild
- **JPS/JPS+ 3D support**: See below

### JPS and JPS+ Z-Level Support

Currently `RunJPS()` and `RunJpsPlus()` only work on z=0. To support multi-floor:

1. **Ladders as forced jump points**
   - In JPS, a "jump point" is where the optimal path might change direction
   - Ladders are natural jump points: you MUST stop there to decide whether to climb
   - Treat `CELL_LADDER` as a forced neighbor in the jump scan
   - When jumping in a direction and hitting a ladder, stop and return it as a jump point

2. **Per-level jump point preprocessing (JPS+)**
   - JPS+ precomputes jump distances for each cell in 8 directions
   - With z-levels: precompute per z-level (like we did with HPA* chunks)
   - Ladder cells get special handling: jump distance = 0 (forced stop)
   - Cross-level jumps not needed - ladders handle transitions

3. **Implementation approach**
   - Update `PrecomputeJpsPlus()` to iterate all z-levels
   - Update jump scanning to treat ladders as walls (stop points)
   - After finding path, post-process to insert ladder transitions
   - Or: let A* handle ladder expansion, JPS just finds same-level segments

4. **Alternative: Hybrid approach**
   - Use JPS/JPS+ for fast same-level pathfinding
   - Use HPA* for cross-level routing decisions
   - Best of both: JPS speed + HPA* hierarchical z-level handling

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
