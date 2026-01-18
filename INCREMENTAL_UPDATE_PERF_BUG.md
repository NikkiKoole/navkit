# Incremental Update Performance Bug

## Problem

When drawing walls on a large map (512x512) with movers active, the incremental HPA* graph update takes ~167ms per dirty chunk. This makes interactive editing feel sluggish.

```
INFO: Incremental update: 1 dirty, 5 affected chunks in 167.62ms
```

## Expected Performance

Based on the actual work needed:
- 5 affected chunks (1 dirty + 4 neighbors)
- ~100 entrances to rebuild (fast, <1ms)
- ~1000 A* calls within 32x32 chunks (~10ms)
- **Expected total: 10-15ms**

We're seeing **10-15x slower** than expected.

## Root Cause

The bottleneck is in `RebuildAffectedEdges()` in `pathing/pathfinding.c`.

### The Expensive Operation

```c
// Step 1: Keep edges where both entrances don't touch any affected chunk
for (int i = 0; i < graphEdgeCount; i++) {  // ~111,000 edges
    int oldE1 = graphEdges[i].from;
    int oldE2 = graphEdges[i].to;
    
    // These two calls are O(n) where n = entranceCount (~2880)
    int newE1 = FindEntranceByPosition(oldEntrances[oldE1].x, oldEntrances[oldE1].y);
    int newE2 = FindEntranceByPosition(oldEntrances[oldE2].x, oldEntrances[oldE2].y);
    ...
}
```

`FindEntranceByPosition` is O(n) linear search:
```c
static int FindEntranceByPosition(int x, int y) {
    for (int i = 0; i < entranceCount; i++) {
        if (entrances[i].x == x && entrances[i].y == y) {
            return i;
        }
    }
    return -1;
}
```

### The Math

- 111,000 edges × 2 lookups = 222,000 calls
- Each lookup scans ~2,880 entrances
- Total: **~640 million comparisons**

Even at 1 nanosecond per comparison = 640ms theoretical minimum.

## Attempted Fixes

### 1. Hash Table for Entrance Lookup

Added O(1) hash table lookup instead of O(n) linear search.

**Result:** Made it slower (2.3 seconds!) and broke the graph connectivity. HPA* could no longer find paths.

The problem: Step 3 still iterates ALL chunks (not just affected), and the edge keeping logic has subtle dependencies on entrance indices that broke when the hash changed the order of operations.

### 2. Skip Unaffected Chunks in Step 3

Added `if (!affectedChunks[cy][cx]) continue;` to only rebuild edges for affected chunks.

**Result:** Combined with hash table, still broke graph connectivity. Edges between affected and unaffected chunks were lost.

### 3. Smaller Chunks (16x16)

More chunks = fewer entrances per chunk = faster per-chunk A*.

**Result:** Slower (288ms) because more total entrances (5952 vs 2880), making the O(n) lookups even worse.

### 4. Larger Chunks (64x64)

Fewer chunks = fewer entrances = faster lookups.

**Result:** Much slower (420ms) because A* within larger chunks is expensive.

## Correct Fix (Not Yet Implemented)

The fix needs to address multiple issues carefully:

1. **Hash table for entrance lookup** - but must be built at the right time and used correctly

2. **Only iterate affected chunks in Step 3** - but must ensure edges crossing affected/unaffected boundaries are handled

3. **Consider alternative approach:** Instead of remapping all edges by position lookup, could:
   - Store edges by entrance position, not index (no remapping needed)
   - Keep a chunk→edges index to quickly find which edges to invalidate
   - Lazy rebuild: queue dirty chunks, rebuild on next pathfind request

## Files Involved

- `pathing/pathfinding.c`:
  - `FindEntranceByPosition()` - the slow function
  - `RebuildAffectedEdges()` - the main bottleneck
  - `UpdateDirtyChunks()` - the entry point

## Test Case

1. Run `./path`
2. Generate terrain (e.g., City or Caves)
3. Spawn 1000+ movers with endless mode
4. Draw walls on the map
5. Observe log messages showing ~167ms per update
