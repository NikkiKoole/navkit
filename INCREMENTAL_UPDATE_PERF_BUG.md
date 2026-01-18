# Incremental Update Performance Bug

## Status: FIXED

The fix was implemented by building a hash table for O(1) entrance position lookups, with a precomputed old→new index mapping array. This reduces the complexity from O(edges × entrances) to O(edges + entrances).

## Problem (Original)

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

We were seeing **10-15x slower** than expected.

## Root Cause

The bottleneck was in `RebuildAffectedEdges()` in `pathing/pathfinding.c`.

### The Expensive Operation (Before Fix)

```c
// Step 1: Keep edges where both entrances don't touch any affected chunk
for (int i = 0; i < graphEdgeCount; i++) {  // ~111,000 edges
    int oldE1 = graphEdges[i].from;
    int oldE2 = graphEdges[i].to;
    
    // These two calls were O(n) where n = entranceCount (~2880)
    int newE1 = FindEntranceByPosition(oldEntrances[oldE1].x, oldEntrances[oldE1].y);
    int newE2 = FindEntranceByPosition(oldEntrances[oldE2].x, oldEntrances[oldE2].y);
    ...
}
```

### The Math (Before Fix)

- 111,000 edges × 2 lookups = 222,000 calls
- Each lookup scans ~2,880 entrances
- Total: **~640 million comparisons**

## The Fix

The key insight is that we can build the old→new index mapping **once** instead of doing O(n) lookups for every edge.

### Implementation

1. **Hash table for entrance positions** - Built AFTER `RebuildAffectedEntrances()` completes, using the NEW entrances array. Uses linear probing for collision resolution.

2. **Precomputed oldToNew mapping** - A single O(oldEntranceCount) pass builds an array mapping old indices to new indices.

3. **O(1) lookups in Step 1** - Instead of `FindEntranceByPosition()`, we now use `oldToNewEntranceIndex[oldE1]`.

### Code Changes

```c
// Step 0: Build hash table and old→new mapping (NEW)
BuildEntranceHash();  // O(entranceCount)

for (int i = 0; i < oldEntranceCount; i++) {
    oldToNewEntranceIndex[i] = HashLookupEntrance(oldEntrances[i].x, oldEntrances[i].y);
}

// Step 1: Now O(1) lookups instead of O(n)
for (int i = 0; i < graphEdgeCount; i++) {
    int newE1 = oldToNewEntranceIndex[graphEdges[i].from];  // O(1)!
    int newE2 = oldToNewEntranceIndex[graphEdges[i].to];    // O(1)!
    ...
}
```

### Why Previous Attempts Failed

1. **Hash table alone wasn't enough** - Previous attempts built the hash table but still called it for every edge (222,000 hash lookups). The mapping array approach does only ~3000 lookups.

2. **Skipping unaffected chunks broke connectivity** - Entrances sit on borders between two chunks. If chunk A is affected and chunk B isn't, an entrance E on their border "touches" both. Skipping chunk B meant edges from E to other entrances in B were never rebuilt.

3. **The correct approach keeps Step 3 iterating all chunks** - This ensures boundary edges are rebuilt. The "exists" check in Step 3 prevents duplicate work for kept edges.

## Files Involved

- `pathing/pathfinding.c`:
  - `BuildEntranceHash()` - builds hash table from entrances array
  - `HashLookupEntrance()` - O(1) position lookup
  - `oldToNewEntranceIndex[]` - precomputed mapping array
  - `RebuildAffectedEdges()` - now uses O(1) lookups

## Test Case

1. Run `./path`
2. Generate terrain (e.g., City or Caves)
3. Spawn 1000+ movers with endless mode
4. Draw walls on the map
5. Observe log messages showing ~10-15ms per update (was ~167ms)
