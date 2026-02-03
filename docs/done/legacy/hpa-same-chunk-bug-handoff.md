# HPA* Same-Chunk Cross-Z Bug - Handoff Document

## Summary

HPA* fails to find paths when start and goal are in the same chunk but require crossing z-levels via ramps or ladders. This is a fundamental limitation in the same-chunk optimization, not specific to ramps.

## Root Cause

Location: `src/world/pathfinding.c:1947-1955`

```c
// Special case: start and goal in same chunk (and same z-level) - just do local A*
if (startChunk == goalChunk) {
    int minX, minY, maxX, maxY, chunkZ;
    GetChunkBounds(startChunk, &minX, &minY, &maxX, &maxY, &chunkZ);
    resultLen = ReconstructLocalPath(start.x, start.y, start.z, 
                                     goal.x, goal.y, goal.z, outPath, maxLen);
    lastPathTime = (GetTime() - startTime) * 1000.0;
    return resultLen;
}
```

When `startChunk == goalChunk`, HPA* does local A* within that chunk and returns immediately. If the local path fails (returns 0), it does NOT fall through to try the full abstract graph search.

The local A* (`ReconstructLocalPath`) only works within a single chunk and cannot cross z-levels. So any path that requires:
- Going down a ramp/ladder to another z-level
- Walking across
- Coming back up via another ramp/ladder

...will fail even though A* would find it.

## Why Simply Falling Through Won't Work

Initially we thought: just remove the early return when local A* fails, let it fall through to full HPA* search.

But the **connect phase** (which finds paths from start/goal to nearby entrances) has the same limitation:

1. It gathers entrances where `entrance.chunk1 == startChunk || entrance.chunk2 == startChunk`
2. Ramp entrances are assigned to chunks by their z-level: `chunk = z * (chunksX * chunksY) + ...`
3. So a ramp entrance at z=0 is in a different chunk than start/goal at z=1
4. The connect phase won't even find the ramp entrance as a target

The `AStarChunkMultiTarget` function used in the connect phase also only searches at a single z-level (`nodeData[sz][y][x]`), so it can't reach cross-z entrances anyway.

## Test Cases

Added tests in `tests/test_pathfinding.c` under `describe(hpa_ramp_pathfinding)`:

### 1. Ramp Test (line ~2383)
`"HPA* fails when start/goal in same chunk but cross-z path needed (KNOWN BUG)"`

Setup:
- 32x32 grid with 8x8 chunks
- Air gap at z0 spanning full grid height (y=0-31) for x=10-13
- West ramp at (10,17,z0) → exit at (9,17,z1)
- East ramp at (13,17,z0) → exit at (14,17,z1)
- Start: (8,17,z1), Goal: (15,17,z1) — both in chunk (1,2)

Result:
- A* finds 8-step path (down west ramp, walk z0, up east ramp)
- HPA* returns 0 (KNOWN BUG)

### 2. Ladder Test (line ~2440)
`"HPA* fails with ladders when start/goal in same chunk but cross-z needed (KNOWN BUG)"`

Setup:
- 32x32 grid, z0 is air (walkable via implicit bedrock)
- Wall at z0 spanning full grid height at x=12
- Dirt at z0 for x=10-14 (except wall) to make z1 walkable
- West ladder at x=10 (z0 and z1)
- East ladder at x=14 (z0 and z1)
- Start: (8,17,z0), Goal: (15,17,z0) — both in same chunk

Result:
- A* finds 10-step path (up west ladder, walk z1, down east ladder)
- HPA* returns 0 (KNOWN BUG)

## Current Workaround

Location: `src/entities/mover.c:1243`

```c
if (len == 0 && algo == PATH_ALGO_HPA && rampCount > 0) {
    len = FindPath(PATH_ALGO_ASTAR, start, m->goal, tempPath, MAX_PATH);
}
```

When HPA* fails and ramps exist, the mover falls back to A*. This is a workaround, not a proper fix — it adds latency and doesn't address the root cause.

Note: This workaround only triggers when `rampCount > 0`, so ladder-only maps may still have issues.

## Proposed Fix

Keep the same-chunk optimization but fall through to full search only when cross-z connections exist:

```c
if (startChunk == goalChunk) {
    resultLen = ReconstructLocalPath(start.x, start.y, start.z, 
                                     goal.x, goal.y, goal.z, outPath, maxLen);
    if (resultLen > 0) {
        lastPathTime = (GetTime() - startTime) * 1000.0;
        return resultLen;  // Found path locally, done
    }
    // Local failed - check if cross-z connections might help
    if (rampCount == 0 && ladderLinkCount == 0) {
        lastPathTime = (GetTime() - startTime) * 1000.0;
        return 0;  // No cross-z connections exist anywhere, no path possible
    }
    // Fall through to full HPA* search - cross-z might provide a path
}
```

This preserves performance for the common case:
- Simple same-chunk paths still get fast local A* (no change)
- Only when local fails AND cross-z connections exist do we do the full search
- If local fails and no ramps/ladders exist, return 0 quickly

**However**, this still requires fixing the connect phase to gather cross-z entrances. The full search needs to find ramp/ladder entrances at adjacent z-levels to work.

### Connect Phase Fix Needed

When falling through to full search, the entrance gathering needs to also include entrances from adjacent z-level chunks that have ramp/ladder connections into the start/goal chunk's x,y region. This is the remaining piece to implement.

## Session Work Done

- Added `--algo` flag to inspect mode for testing different pathfinding algorithms
- Added designation type display (MINE/CHANNEL/etc instead of just "DIG")
- Created reproducible test cases documenting the bug for both ramps and ladders
- Confirmed the limitation affects both ramps and ladders equally
- Analyzed why simple fall-through won't work (connect phase limitation)
- Identified proposed fix approach that preserves performance
