# HPA* Pathfinding - Future Ideas

**Status: COMPLETED/ARCHIVED** - Most ideas implemented, remaining items moved to todo.md

---

## JPS+ (Jump Point Search Plus) - IMPLEMENTED

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

**Implementation:** `RunJpsPlus()`, `PrecomputeJpsPlus()`, `PATH_ALGO_JPS_PLUS` in pathfinding.c

### JPS+ and Z-Levels (without HPA*) - IMPLEMENTED

Z-transitions handled via `JpsLadderGraph` - 2D JPS+ per level, stitched at transitions.

**Implementation:** `BuildJpsLadderGraph()`, `FindPath3D_JpsPlus()` in pathfinding.c

---

## Z-Levels / 3D Pathfinding - IMPLEMENTED

### Core Insight
A staircase/ladder/elevator between Z levels is conceptually the same as an entrance between chunks.

**Implementation:** 
- 3D grid array: `grid[MAX_Z][MAX_Y][MAX_X]`
- 3D chunk IDs: `chunkId = z * (chunksX * chunksY) + cy * chunksX + cx`
- `LadderLink` struct for z-transitions
- All algorithms (A*, HPA*, JPS, JPS+) support z-levels

---

## Lazy Refinement - NOT NEEDED

### The Problem
Currently we refine the entire abstract path upfront. This could waste work if agents get interrupted.

### Decision
Not implemented - current performance is sufficient. HPA* refinement is fast enough that lazy refinement adds complexity without meaningful benefit for our use case.

---

## Other Ideas

### Path Caching - NOT NEEDED
- Would cache recently computed paths (hash of start+goal -> path)
- Decision: Not needed - pathfinding is fast enough, and terrain changes frequently

### Spatial Hashing for Entrances - IMPLEMENTED
**Implementation:** `entranceHash[]`, `BuildEntranceHash()`, `HashLookupEntrance()` in pathfinding.c
- O(1) entrance position lookup
- Used in incremental updates

### Parallel Pathfinding - NOT NEEDED
- Would allow multiple agents to pathfind simultaneously on different threads
- Decision: Not needed - current single-threaded performance handles 10,000 movers

### Flow Fields - MOVED TO TODO
- For many agents going to same destination
- Moved to todo.md as future feature

### Territory/Ownership Costs - MOVED TO TODO
- Agent-specific movement costs (RimWorld-style)
- Moved to todo.md as future feature
