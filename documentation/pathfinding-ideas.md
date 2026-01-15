# HPA* Pathfinding - Future Ideas

## JPS+ (Jump Point Search Plus)

### What is JPS?
Regular JPS exploits grid symmetry to skip intermediate nodes. Instead of expanding all neighbors like A*, it "jumps" in straight lines until hitting a wall or a "jump point" - a cell where the optimal path might change direction.

- Much faster than A* on open grids
- Similar performance to A* on maze-like terrain
- Only works with 8-directional movement

### What is JPS+?
JPS+ pre-computes jump distances for every cell in every direction:
- For each cell, stores "how far can I jump in direction X before hitting a wall or jump point"
- Turns runtime jumping into a simple table lookup
- Tradeoff: preprocessing time + memory for faster queries
- Memory cost: ~16 bytes per cell (8 directions × 2 bytes)

### How we might use JPS+

**Option A: Standalone pathfinder (replace HPA*)**
- JPS+ might be fast enough without hierarchical abstraction
- Simpler architecture - no entrances, no abstract graph
- Preprocessing must be redone when terrain changes
- Good for: static maps or infrequent changes

**Option B: Inside HPA* for connect/refine steps**
- Keep the hierarchical structure for long-distance planning
- Use JPS+ instead of A* for:
  - Connect step: finding path from start/goal to nearby entrances
  - Refine step: reconstructing cell-level paths between entrances
- Could pre-compute JPS+ tables per chunk
- When chunk changes, only recompute that chunk's JPS+ table
- Good for: dynamic maps with localized changes

**Option C: Hybrid approach**
- Use JPS+ for same-chunk paths (short distances)
- Use HPA* for cross-chunk paths (long distances)
- Best of both worlds?

### Considerations
- JPS+ only works with 8-directional movement (we support both 4 and 8)
- Need to handle the 4-dir case separately or disable JPS+ for 4-dir

### JPS+ and Z-Levels (without HPA*)
If using JPS+ standalone (no HPA*), how to handle stairs/ladders?

Z-transitions break the grid symmetry that JPS+ exploits - they're like "portals" at specific cells.

**Options:**
1. Treat Z-transitions as forced jump points - any cell with stairs forces a stop
2. **2D JPS+ per level, stitch at transitions** (preferred) - run JPS+ on each level independently, handle Z-transitions as special edges
3. Full 3D JPS - extend jump directions to include up/down (complex, probably not worth it)

**Option 2 is cleanest:** JPS+ stays 2D (what it's designed for), and Z-transitions are just special edges. When expanding a node, check for z-transitions at that cell and add them as neighbors:

```c
void ExpandNode(int x, int y, int z) {
    // Normal JPS+ jumping on this level
    for (int dir = 0; dir < 8; dir++) {
        JumpInDirection(x, y, z, dir);
    }
    
    // Z-transitions at this cell
    if (hasStairs[z][y][x]) {
        if (z + 1 < maxZ) TryMove(x, y, z + 1, STAIR_COST);
        if (z - 1 >= 0)   TryMove(x, y, z - 1, STAIR_COST);
    }
}
```


## Z-Levels / 3D Pathfinding

### Core Insight
A staircase/ladder/elevator between Z levels is conceptually the same as an entrance between chunks:

```
Regular entrance:
  - Connects two adjacent chunks on same level
  - Has a cost (crossing the border)

Z-transition "entrance":
  - Connects same (x,y) position on different Z levels
  - Has a cost (climbing stairs, using elevator)
```

### Grid Structure Options

**Option A: 3D array**
```c
CellType grid[MAX_Z][MAX_Y][MAX_X];
```
- Simple indexing
- Memory allocated for all levels even if sparse

**Option B: Separate grid per level + transition table**
```c
CellType grid[MAX_Y][MAX_X];        // per level
ZTransition transitions[MAX_TRANSITIONS];
```
- More flexible for sparse levels
- Transition table lists connections between levels

**Option C: Flat array with z-offset**
```c
CellType grid[MAX_Z * MAX_Y * MAX_X];
#define CELL(x,y,z) grid[(z)*MAX_Y*MAX_X + (y)*MAX_X + (x)]
```
- Cache-friendly for single-level access
- Easy to extend existing code

### Chunk Numbering Across Levels

**Option A: 3D chunk IDs**
```c
chunkId = z * (chunksX * chunksY) + cy * chunksX + cx;
```
- Unified chunk space
- Entrances naturally connect chunks on different levels

**Option B: Keep chunks 2D, handle Z separately**
- Each level has its own chunk grid
- Z-transitions are a separate list, not part of entrance array
- Might be cleaner separation of concerns

### Transition Types
Different costs and rules for different transition types:

| Type     | Direction    | Cost   | Notes                    |
|----------|--------------|--------|--------------------------|
| Stairs   | Bidirectional| Medium | Standard floor connector |
| Ladder   | Bidirectional| Slow   | Narrow, single-file      |
| Elevator | Bidirectional| Fast   | Maybe wait time?         |
| Jump down| One-way (down)| Fast  | Could cause damage       |
| Hole/pit | One-way (down)| Instant| Trap, damage on landing  |
| Ramp     | Bidirectional| Low    | Smooth transition        |

### Implementation Steps
1. Extend grid to support Z levels (pick option A, B, or C)
2. Add Z-transitions as special entrances or separate list
3. Update chunk calculation to include Z
4. Update entrance building to create Z-transitions where stairs/ladders exist
5. Graph building already handles entrances - should "just work"
6. Visualization: layer toggle or isometric view?


## Lazy Refinement

### The Problem
Currently we refine the entire abstract path upfront:
- Long path = many segments = many A* calls
- Most of this work is wasted if:
  - Agent gets interrupted
  - Target moves
  - Path becomes invalid due to terrain changes
  - Agent only needs to know the next few steps

### The Solution
Refine only what we need, when we need it:

```
Full path:      [start] -> E1 -> E2 -> E3 -> E4 -> E5 -> [goal]
                   |        |    |    |    |    |    |
Refined:        [====]   [===]  [ ]  [ ]  [ ]  [ ]  [ ]
                 done    done   not yet...
```

### Implementation Concept
```c
typedef struct {
    // Abstract path (always computed)
    int abstractPath[MAX_ENTRANCES];
    int abstractPathLength;
    int abstractIndex;  // Current position in abstract path
    
    // Refined segments (computed on demand)
    Point refinedSegment[MAX_PATH];
    int refinedLength;
    int refinedIndex;   // Current position in refined segment
    
    // How many segments ahead to refine
    int lookahead;  // e.g., 2 segments
} LazyPath;

// Called each frame as agent moves
Point GetNextStep(LazyPath* path) {
    // If we've used up the refined segment, refine next one
    if (path->refinedIndex >= path->refinedLength) {
        RefineNextSegment(path);
    }
    return path->refinedSegment[path->refinedIndex++];
}
```

### Benefits
- Faster initial pathfinding (only refine first 1-2 segments)
- Less wasted work when paths change
- Memory efficient - don't store full cell-level path
- Natural fit for moving agents

### When to Refine More
- Agent approaching end of refined segment
- Background thread during idle time
- When agent stops (waiting, idle animation)

### Considerations
- Need to handle path invalidation gracefully
- What if a future segment becomes blocked? Re-plan from current position
- Works great with incremental HPA* updates - abstract path stays valid longer


## Other Ideas

### Path Caching
- Cache recently computed paths (hash of start+goal -> path)
- LRU cache with configurable size
- Invalidate when terrain changes in relevant chunks

### Spatial Hashing for Entrances
- Instead of iterating all entrances to find ones in a chunk
- Use spatial hash: `entrancesByChunk[chunkId]` -> list of entrance indices
- Faster connect step

### Parallel Pathfinding
- Multiple agents can pathfind simultaneously on different threads
- Need thread-local nodeData arrays
- Abstract graph search is read-only after BuildGraph()

### Flow Fields
- For many agents going to same destination
- Compute once, all agents follow the flow
- Good for: RTS games, crowd simulation to exits

### Territory/Ownership Costs (far future)
Path cost that depends on who is pathing - agents prefer public roads over walking through someone else's house.

```c
int GetMoveCost(int x, int y, int agentOwner) {
    int cost = baseCost[y][x];  // terrain: road=1, mud=3
    int owner = zoneOwner[y][x]; // -1=public, 0+=owned
    if (owner >= 0 && owner != agentOwner) {
        cost += TRESPASS_PENALTY;  // prefer going around
    }
    return cost;
}
```

Rimworld-style: colonists avoid others' bedrooms, visitors stick to public areas.
