# HPA* Cross-Z Performance Fix

## Problem Statement

With 1683 movers on a 256x256x16 hilly terrain map, the game experiences **multi-second hangs** caused by pathfinding.

## Verified Root Cause

### The Smoking Gun

Console logs during spikes show:
```
WARNING: SLOW A* fallback: mover 127, 611.1ms, start(187,84,z9)->goal(247,124,z7), len=63
WARNING: SLOW A* fallback: mover 1476, 451.8ms, start(162,56,z9)->goal(75,55,z7), len=89
WARNING: SLOW A* fallback: mover 1121, 1660.5ms, start(109,61,z9)->goal(112,215,z8), len=155
```

**A* fallback is taking 450ms to 1.6 SECONDS per path** for cross-z queries.

### Why This Happens

1. **HPA* returns 0** for certain cross-z paths
2. **A* fallback triggers** - code at mover.c:1245-1254 calls full A* when HPA* returns 0
3. **A* searches entire 3D grid** - 256×256×16 = 1,048,576 cells
4. **Catastrophic slowdown** - even finding a valid path takes 0.5-1.6 seconds

### Key Data Points

From headless testing (600 ticks):
- **HPA* success rate: 98.9%** (922 successes)
- **A* fallback rate: 1.1%** (10 fallbacks)
- **Average tick time: 27ms** (acceptable)

But those 1.1% of fallbacks cause the spikes because each one can take **seconds**.

---

## Code Analysis: What We Found

### Good News: The Architecture is Already Correct

After deep-diving into `pathfinding.c`, we discovered that **HPA* already has inter-level edges** for ramps and ladders. The architecture is sound:

#### Entrance Types (pathfinding.c lines 486-506, 565-624)

1. **Horizontal border entrances** - where chunks meet on same z-level
2. **Ladder entrances** - one entrance at z, one at z+1, connected by inter-level edge
3. **Ramp entrances** - one at ramp position (z), one at exit position (z+1)

```c
// AddLadderEntrance creates entrance at specific z-level
static int AddLadderEntrance(int x, int y, int z) {
    int chunk = z * (chunksX * chunksY) + (y / chunkHeight) * chunksX + (x / chunkWidth);
    entrances[entranceCount] = (Entrance){x, y, z, chunk, chunk};
    return entranceCount++;
}
```

#### Edge Types (pathfinding.c lines 1380-1500)

1. **Intra-edges** - connect entrances within same chunk via 2D Dijkstra
2. **Inter-edges** - connect chunk border entrances (cost 1)
3. **Ladder edges** - connect ladder entrances at z ↔ z+1 (cost 10)
4. **Ramp edges** - connect ramp entrance at z ↔ exit at z+1 (cost 14)

```c
// Ladder links added to abstract graph (lines 1450-1478)
ladderLinks[i] = (LadderLink){
    .entranceLow = entLow,
    .entranceHigh = entHigh,
    .cost = 10
};
```

#### Path Refinement Already Handles Cross-Z (lines 2179-2188)

```c
// Check if this is a ladder transition (z-level change)
if (fz != tz) {
    // Ladder transition: just add the destination point (same x,y, different z)
    // The mover will handle the actual ladder climbing
    if (resultLen < maxLen) {
        outPath[resultLen++] = (Point){tx, ty, tz};
    }
    continue;  // Skip local refinement for cross-z segments
}
```

This is exactly right! Cross-z segments don't need local refinement - they're single-step transitions.

### The Mystery: Why Does HPA* Still Fail?

The architecture is correct, but HPA* returns 0 for ~1.1% of cross-z paths. A* then finds a valid path, proving one exists.

#### Hypothesis: Disconnected Regions on Hilly Terrain

On a 256×256×16 hilly map, a mover at position A (z=9) might be in a situation where:

1. **No ramp/ladder going DOWN is reachable** via same-z walkable cells
2. **But there IS a valid path**: go UP first via ramp to z=10, walk across, then go DOWN

Example scenario:
```
z=10:  [    ]  [RAMP↓]  [    ]  [RAMP↓]  [    ]
z=9:   [WALL]  [RAMP↑]  [WALL]  [RAMP↑]  [MOVER_A]
z=8:   [    ]  [    ]   [    ]  [GOAL]   [WALL]
```

Mover A can't reach the left ramp on z=9 (wall blocks), but CAN reach the right ramp, go up to z=10, walk left, then go down to z=8.

**The question**: Does HPA* correctly find this path? Or does the connect phase fail because:
- Start connects to entrances at z=9
- But no z=9 entrance can reach the goal at z=8 without going UP first?

#### Key Code to Investigate: Connect Phase (lines 1983-2040)

```c
// Gather entrances for start chunk
for (int i = 0; i < entranceCount && startTargetCount < 128; i++) {
    if (entrances[i].chunk1 == startChunk || entrances[i].chunk2 == startChunk) {
        startTargetX[startTargetCount] = entrances[i].x;
        startTargetY[startTargetCount] = entrances[i].y;
        startTargetIdx[startTargetCount] = i;
        startTargetCount++;
    }
}
```

This gathers entrances belonging to `startChunk`. Since `startChunk` includes z in its ID, this only finds entrances at the start's z-level.

**This is correct** - the abstract graph's inter-level edges should handle z-transitions.

#### Key Code: AStarChunkMultiTarget is 2D (lines 722-752)

```c
int AStarChunkMultiTarget(int sx, int sy, int sz, ...) {
    // Operates on single z-level: nodeData[sz][y][x]
    // Uses 2D 4-dir or 8-dir movement only
}
```

This finds costs from start to all entrances **on the same z-level**. Again, this is correct by design.

### What Could Be Wrong?

Possibilities to investigate:

1. **Abstract search not finding path** - The abstract A* (lines 2052-2130) might fail to find a route through the graph even when one exists

2. **Missing edges in abstract graph** - Some ramp/ladder connections might not be properly added during `BuildGraph()`

3. **Heuristic issues** - The abstract search uses 2D heuristic `Heuristic(start.x, start.y, goal.x, goal.y)` which ignores z-distance. This is admissible but might cause issues?

4. **Edge case in connect phase** - If start can't reach ANY entrance at its z-level (completely isolated), HPA* correctly returns 0. But maybe some entrances should be reachable and aren't?

---

## Original Proposed Fix (May Not Be Needed)

The original diagnosis was wrong. We thought `ReconstructLocalPath()` failing on cross-z was the problem, but actually:

1. Cross-z segments in refinement are handled correctly (just add destination point)
2. The issue is earlier - either in connect phase or abstract search

This section is preserved for reference but may not be the actual fix needed.

### Why Full A* Fallback is Wrong

The A* fallback searches the **entire** 256×256×16 grid. But we only need to search a **local area** around the ramp/ladder connecting the z-levels.

### Originally Proposed Solution: Bounded 3D Local Search

Create `ReconstructLocalPathCrossZ()` that:
1. Uses bounded 3D A* (chunk + 1 neighbor = ~32×32×2 cells max)
2. Handles ramp up/down transitions (copy logic from RunAStar lines 1688-1759)
3. Handles ladder up/down transitions

**Search space comparison:**
- Current A* fallback: 1,048,576 cells (256×256×16)
- New local 3D search: ~2,048 cells (32×32×2)
- **500x smaller search space**

**However**: After code analysis, we found that cross-z refinement is already handled correctly. The issue is likely earlier in the pipeline.

---

## Next Steps: Debug Investigation

Since the architecture is correct but something is failing, we need to add debug logging to find out exactly where HPA* fails for cross-z paths.

### Step 1: Add Debug Logging

Add logging to `FindPathHPA()` to trace:

```c
// At start of FindPathHPA:
bool debugCrossZ = (start.z != goal.z);
if (debugCrossZ) {
    TraceLog(LOG_DEBUG, "HPA* cross-z: (%d,%d,z%d) -> (%d,%d,z%d)",
             start.x, start.y, start.z, goal.x, goal.y, goal.z);
}

// After gathering entrances:
if (debugCrossZ) {
    TraceLog(LOG_DEBUG, "  startChunk=%d, entrances found: %d", startChunk, startTargetCount);
    TraceLog(LOG_DEBUG, "  goalChunk=%d, entrances found: %d", goalChunk, goalTargetCount);
}

// After connect phase:
if (debugCrossZ) {
    TraceLog(LOG_DEBUG, "  start connects to %d entrances", startEdgeCount);
    TraceLog(LOG_DEBUG, "  goal connects to %d entrances", goalEdgeCount);
}

// After abstract search:
if (debugCrossZ) {
    if (abstractPathLength > 0) {
        TraceLog(LOG_DEBUG, "  abstract path length: %d", abstractPathLength);
    } else {
        TraceLog(LOG_DEBUG, "  abstract search FAILED - no path found");
    }
}
```

### Step 2: Run With Failing Path

Use the inspector to find a specific failing path:
```bash
./bin/path --inspect saves/2026-02-04_16-51-54.bin.gz --stuck
```

Or run headless and capture the WARNING logs to get start/goal coordinates of failing paths.

### Step 3: Reproduce in Inspector

```bash
./bin/path --inspect saves/2026-02-04_16-51-54.bin.gz --path 187,84,9 247,124,7 --algo hpa
```

This should show exactly where HPA* fails.

### Step 4: Identify Root Cause

Based on debug output, determine if:

1. **Connect phase fails** - Start/goal can't reach any entrances
   - Fix: Check if mover is isolated on their z-level
   
2. **Abstract search fails** - Graph is disconnected
   - Fix: Check if ramp/ladder edges are properly built
   
3. **Refinement fails** - Abstract path exists but refinement returns 0
   - Fix: Check refinement logic for edge cases

### Step 5: Implement Fix

Once root cause is identified, implement targeted fix.

---

## Success Criteria

1. No more multi-second spikes in stress test
2. All existing pathfinding tests pass
3. Headless 600-tick test: < 30ms/tick average (currently 27ms)
4. A* fallback rate drops to near-zero (currently 1.1%)

---

## Research Notes: HPA* and 3D

### What Standard HPA* Says

The original HPA* paper (Botea et al. 2004) only covers 2D. Extensions to 3D are implementation-specific.

From [Alexandru Ene's blog](https://alexene.dev/2019/06/02/Hierarchical-pathfinding.html):
> "For each cell (not only the edges) we check if there is a cell below or above that we are connected to."

### How Dwarf Fortress Does It

From [DF Wiki](https://dwarffortresswiki.org/index.php/Path):
- Uses A* with "connected region" precomputation (flood fill)
- Ramps are **diagonal z-transitions** (1 step changes XY and Z)
- Stairs are **pure vertical** (1 step changes only Z)
- Uses region connectivity to quickly reject impossible paths

### Key Insight

Rather than bounded 3D search for refinement, the better approach is:

**The abstract graph should contain inter-level edges at ramp/ladder positions.** When the abstract path includes a cross-z edge:
- That edge represents a 1-tile transition (walk up ramp, climb ladder)
- No local search needed - just add the destination point
- All other segments are same-z and use existing 2D refinement

**This is exactly what our code already does!** So the bug must be elsewhere.

---

## Files Reference

| File | Relevant Code |
|------|---------------|
| `src/world/pathfinding.c` | HPA* implementation |
| - lines 486-506 | `AddLadderEntrance()`, `AddRampEntrance()` |
| - lines 565-624 | Entrance detection for ladders/ramps |
| - lines 722-752 | `AStarChunkMultiTarget()` (2D) |
| - lines 1380-1500 | `BuildGraph()` edge creation |
| - lines 1450-1500 | Ladder/ramp edge addition |
| - lines 1940-2050 | `FindPathHPA()` connect phase |
| - lines 2052-2130 | Abstract A* search |
| - lines 2150-2220 | Path refinement |
| - lines 2179-2188 | Cross-z segment handling |
| `src/entities/mover.c` | |
| - lines 1245-1254 | A* fallback when HPA* returns 0 |

---

## Test Save File

`saves/2026-02-04_16-51-54.bin.gz` - stress test scenario:
- 1683 movers
- 256×256×16 hilly terrain
- No items, jobs, or stockpiles - just movers wandering

Use with:
```bash
./bin/path --headless --load saves/2026-02-04_16-51-54.bin.gz --ticks 600
```

### Known Failing Paths (Reproduce Quickly)

Three movers quickly hit the problem after loading the save:

| Mover | Start | Goal | Time | Path Len |
|-------|-------|------|------|----------|
| 127 | (187, 84, z9) | (247, 124, z7) | 602ms | 63 |
| 1476 | (162, 56, z9) | (75, 55, z7) | 452ms | 89 |
| 1121 | (109, 61, z9) | (112, 215, z8) | 1656ms | 155 |

Use these for debugging:
```bash
# Test specific failing path with HPA*
./bin/path --inspect saves/2026-02-04_16-51-54.bin.gz --path 187,84,9 247,124,7 --algo hpa

# Compare with A* (should succeed)
./bin/path --inspect saves/2026-02-04_16-51-54.bin.gz --path 187,84,9 247,124,7 --algo astar
```

---

## Summary: What We Know

### The Architecture is Correct

After deep code analysis, we confirmed HPA* already has the right design:

1. **Inter-level edges exist** - Ramps and ladders create entrance pairs connected by edges in the abstract graph
2. **Refinement handles cross-z** - Cross-z segments just add the destination point (no local search needed)
3. **This matches best practice** - Standard HPA* extension for 3D

### But Something is Failing

HPA* returns 0 for ~1.1% of cross-z paths. A* then finds a valid path (proving one exists), but takes 0.5-1.6 seconds.

### Likely Culprits

1. **Connect phase** - Start/goal can't reach any entrances on their z-level
2. **Abstract search** - Graph is disconnected (missing edges)
3. **Edge building** - Some ramp/ladder edges not properly added

### The Hypothesis

On hilly terrain, a mover might need to go UP before going DOWN:
- Mover at z=9, goal at z=7
- No direct ramp down reachable on z=9
- But CAN go: up to z=10 → walk across → down to z=8 → down to z=7

The question: Does HPA* find this path, or does something break?

---

## Action Plan

### Phase 1: Add Debug Logging

Add tracing to `FindPathHPA()` in `src/world/pathfinding.c`:

```c
// At function start (~line 1938)
bool debugCrossZ = (start.z != goal.z);
if (debugCrossZ) {
    TraceLog(LOG_DEBUG, "HPA* cross-z: (%d,%d,z%d) -> (%d,%d,z%d)",
             start.x, start.y, start.z, goal.x, goal.y, goal.z);
}

// After gathering entrances (~line 2005)
if (debugCrossZ) {
    TraceLog(LOG_DEBUG, "  startChunk=%d, entrances found: %d", startChunk, startTargetCount);
    TraceLog(LOG_DEBUG, "  goalChunk=%d, entrances found: %d", goalChunk, goalTargetCount);
}

// After connect phase (~line 2050)
if (debugCrossZ) {
    TraceLog(LOG_DEBUG, "  start connects to %d entrances", startEdgeCount);
    TraceLog(LOG_DEBUG, "  goal connects to %d entrances", goalEdgeCount);
}

// After abstract search (~line 2130)
if (debugCrossZ) {
    if (abstractPathLength > 0) {
        TraceLog(LOG_DEBUG, "  abstract path found, length: %d", abstractPathLength);
    } else {
        TraceLog(LOG_DEBUG, "  FAILED: abstract search found no path");
    }
}

// After refinement (~line 2220)
if (debugCrossZ && abstractPathLength > 0) {
    if (resultLen > 0) {
        TraceLog(LOG_DEBUG, "  refined path length: %d", resultLen);
    } else {
        TraceLog(LOG_DEBUG, "  FAILED: refinement returned 0");
    }
}
```

### Phase 2: Reproduce and Capture

```bash
# Build and run with one of the known failing paths
make
./bin/path --inspect saves/2026-02-04_16-51-54.bin.gz --path 187,84,9 247,124,7 --algo hpa
```

The debug output will show exactly where it fails:
- "entrances found: 0" → No entrances in chunk
- "start connects to 0 entrances" → Can't reach any entrances (isolated)
- "FAILED: abstract search found no path" → Graph disconnected
- "FAILED: refinement returned 0" → Refinement bug

### Phase 3: Fix Based on Findings

| Debug Output | Root Cause | Fix |
|--------------|------------|-----|
| "entrances found: 0" | No entrances in start/goal chunk | Check entrance detection |
| "connects to 0 entrances" | Start/goal isolated on z-level | May need to expand search to adjacent chunks |
| "abstract search found no path" | Graph disconnected | Check ramp/ladder edge building |
| "refinement returned 0" | Refinement bug | Check refinement logic |

### Phase 4: Verify Fix

```bash
# Run stress test - should have no SLOW warnings
./bin/path --headless --load saves/2026-02-04_16-51-54.bin.gz --ticks 600

# Check fallback rate dropped to ~0%
# Check average tick time still < 30ms
```

---

## Quick Reference: Key Code Locations

| What | File | Lines |
|------|------|-------|
| Entrance creation | pathfinding.c | 486-506, 565-624 |
| Edge building | pathfinding.c | 1380-1500 |
| Ladder/ramp edges | pathfinding.c | 1450-1500 |
| FindPathHPA | pathfinding.c | 1938-2230 |
| Connect phase | pathfinding.c | 1983-2050 |
| Abstract search | pathfinding.c | 2052-2130 |
| Refinement | pathfinding.c | 2150-2220 |
| Cross-z handling | pathfinding.c | 2179-2188 |
| A* fallback | mover.c | 1245-1254 |
 