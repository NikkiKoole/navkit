# HPA* Cross-Z Performance Fix

## Status: FIXED

**Root Cause**: MAX_RAMP_LINKS was set to 1024, but the hilly terrain had 4096+ valid ramp connections. Ramps at higher y-coordinates weren't being added to the HPA* graph, causing disconnected regions.

**Fix**: Increased MAX_RAMP_LINKS from 1024 to 4096 in `pathfinding.h`.

**Results**:
- A* fallback rate: **0%** (was 1.1%)
- Average tick time: **10.84 ms** (was 27ms with spikes to seconds)
- All pathfinding tests pass

---

## Original Problem Statement

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

### ROOT CAUSE IDENTIFIED

**The bug is in the connect phase of HPA*** - when connecting start/goal to the abstract graph.

#### The Failing Scenario

Testing same-z path `(184,177,z7) -> (180,130,z7)`:
- A* finds path: 48 steps, **z7 to z9** (goes UP over a hill, then back DOWN)
- HPA* fails: abstract search finds no path

#### Why It Fails

1. **Chunk IDs include z-level**: `chunk = z * (chunksX * chunksY) + cy * chunksX + cx`
2. **Connect phase only looks at same-z entrances**: When gathering entrances for startChunk, it only finds entrances at z=7
3. **Terrain has a hill between start and goal**: At y=150-175, z=7 is solid dirt (inside hill), walkable surface is at z=8-9
4. **No z=7 entrances cross the hill**: The z=7 abstract graph is fragmented into multiple disconnected regions
5. **The solution requires going UP**: Must use a ramp at z=7 to reach z=8, cross the hill at z=8, then come back down

#### Debug Evidence

```
HPA*: (184,177,z7) -> (180,130,z7)
  startChunk=1979, entrances found: 25
  goalChunk=1931, entrances found: 28
  start connects to 25 entrances
  goal connects to 28 entrances
  start z=7 has 18 cross-z edges (up:0, down:18)  <-- NO UP edges!
  goal z=7 has 23 (up:23, down:0)                 <-- has UP edges but wrong direction
  FAILED: abstract search found no path
```

The start entrances only have edges going DOWN to z=6 (18 edges, 0 up).
The goal entrances only have edges going UP to z=8 (23 edges).
But the path needs to go: z=7 → UP to z=8 → across → DOWN to z=7.

#### Key Insight

The z=7 level is topologically divided by hills into separate regions:
- Region A (y > 150): walkable z=7 surface, ramps go DOWN to z=6
- Region B (y < 145): walkable z=7 surface, ramps go UP to z=8
- Between them: solid dirt at z=7, walkable surface at z=8-9

A* can traverse this because it searches 3D space. HPA* fails because:
1. Start at z=7 can only reach z=7 and z=6 entrances locally
2. Goal at z=7 can only reach z=7 and z=8 entrances locally  
3. There's no z=7 path connecting them (hill blocks it)
4. The z=8 level could connect them, but HPA* doesn't look there

---

## The Fix

### Problem Restatement

HPA* connect phase only considers entrances in the start/goal chunk. Since chunks are 3D (chunk ID includes z), a mover at z=7 only connects to z=7 entrances, even if they need to go to z=8 first.

### Solution: Include Cross-Z Entrances in Connect Phase

When connecting start to the abstract graph:
1. Find entrances in startChunk (current behavior)
2. **Also find ramp/ladder entrances that connect TO this z-level from adjacent z-levels**

Specifically, if start is at z=7:
- Include z=7 border entrances (current)
- Include ramp exits at z=7 (these connect from z=6 ramps going up)
- Include ladder tops at z=7 (these connect from z=6 ladders going up)

This allows the abstract search to find: start → entrance at z=7 → ramp edge to z=8 → ... → goal

### Implementation Plan

1. **Modify entrance gathering** in `FindPathHPA()` connect phase (~line 2000):
   - After gathering entrances for `startChunk`, also check `rampLinks` and `ladderLinks`
   - If a ramp/ladder exit is near the start position (within reach), add it as a valid start entrance
   
2. **Same for goal**: If a ramp/ladder entry is near the goal, include it

3. **Alternative approach**: Instead of modifying connect phase, ensure that when we gather entrances for a chunk at z=N, we also include ramp entrances in the same XY chunk at z=N±1 that have edges connecting to z=N.

### Simplest Fix

The simplest fix might be to expand the entrance gathering to look at adjacent z-levels:

```c
// Current: only look at entrances in startChunk (same z)
for (int i = 0; i < entranceCount && startTargetCount < 128; i++) {
    if (entrances[i].chunk1 == startChunk || entrances[i].chunk2 == startChunk) {
        // ...
    }
}

// New: also look at entrances in z±1 chunks that have cross-z edges
int startChunkBelow = GetChunk(start.x, start.y, start.z - 1);
int startChunkAbove = GetChunk(start.x, start.y, start.z + 1);
for (int i = 0; i < entranceCount && startTargetCount < 128; i++) {
    if (entrances[i].chunk1 == startChunk || entrances[i].chunk2 == startChunk ||
        entrances[i].chunk1 == startChunkBelow || entrances[i].chunk2 == startChunkBelow ||
        entrances[i].chunk1 == startChunkAbove || entrances[i].chunk2 == startChunkAbove) {
        // Add entrance if reachable
    }
}
```

But this is too broad - we'd be adding entrances that aren't actually reachable from the start.

### Better Fix: Add Virtual Start/Goal Edges to Adjacent Z Ramps

The connect phase already does multi-target Dijkstra to find costs to entrances. The issue is it only searches at the start's z-level.

**Solution**: After the current connect phase, also check if there are ramp/ladder entrances in adjacent z-levels that are actually reachable:

1. For each ramp link where `rampZ == start.z - 1` (ramp below going up):
   - The ramp exit is at `start.z`
   - Check if start can walk to the ramp exit position
   - If yes, add the ramp entrance (at z-1) as a valid start connection with appropriate cost

2. For each ramp link where `rampZ == start.z` (ramp at same level going up):
   - Already handled by current entrance gathering

This is getting complex. Let me think of the cleanest approach...

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
 