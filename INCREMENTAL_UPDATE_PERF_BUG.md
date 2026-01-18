# Incremental Update Performance Bug

## Status: FIXED

Performance improved from ~167ms to ~20ms per incremental update (8x speedup).

## Problem (Original)

When drawing walls on a large map (512x512) with movers active, the incremental HPA* graph update took ~167ms per dirty chunk. This made interactive editing feel sluggish.

```
INFO: Incremental update: 1 dirty, 5 affected chunks in 167.62ms
```

## Expected Performance

Based on the actual work needed:
- 5 affected chunks (1 dirty + 4 neighbors)
- ~100 entrances to rebuild (fast, <1ms)
- Edge cost calculations within 32x32 chunks (~10-20ms)
- **Expected total: 10-20ms**

We were seeing **8-15x slower** than expected.

## Root Cause

Multiple O(n) and O(n^2) bottlenecks in `RebuildAffectedEdges()`:

1. **O(edges x entrances) lookups** - `FindEntranceByPosition()` was O(n), called for every edge
2. **O(chunks x entrances) scanning** - Finding entrances per chunk required full scan
3. **O(entrances^2) A* calls per chunk** - Each pair of entrances required separate A* search

## The Fix (4 Phases)

### Phase 1: Hash Table for O(1) Lookups (~167ms → ~24ms)

Built a hash table for entrance positions with a precomputed `oldToNew` mapping array:

```c
// Before: O(entrances) per lookup
int newE1 = FindEntranceByPosition(oldEntrances[oldE1].x, oldEntrances[oldE1].y);

// After: O(1) via precomputed mapping
int newE1 = oldToNewEntranceIndex[graphEdges[i].from];
```

### Phase 2: Skip Unaffected Chunks (minor improvement)

Only process chunks that are affected OR have entrances touching affected chunks.

### Phase 3: Chunk-to-Entrances Index (~24ms → ~22ms)

Built an index `chunkEntrances[chunk][i]` to avoid scanning all entrances per chunk:

```c
// Before: scan all ~2880 entrances per chunk
for (int i = 0; i < entranceCount; i++) {
    if (entrances[i].chunk1 == chunk || entrances[i].chunk2 == chunk) ...
}

// After: direct lookup
for (int i = 0; i < chunkEntranceCount[chunk]; i++) {
    int entIdx = chunkEntrances[chunk][i];
}
```

### Phase 4: Multi-Target Dijkstra (~22ms → ~20ms)

Instead of O(n^2) individual A* calls per chunk, run ONE multi-target Dijkstra per entrance that needs edges rebuilt:

```c
// Before: 2472 separate A* calls
for (int i = 0; i < numEnts; i++) {
    for (int j = i + 1; j < numEnts; j++) {
        int cost = AStarChunk(e1.x, e1.y, e2.x, e2.y, ...);
    }
}

// After: ~279 multi-target Dijkstra calls
for (int i = 0; i < numEnts; i++) {
    // Build target list of entrances that need edges to e1
    AStarChunkMultiTarget(e1.x, e1.y, targetX, targetY, outCosts, numTargets, ...);
}
```

## Performance Summary

| Phase | Time | Search Calls | Improvement |
|-------|------|--------------|-------------|
| Original | ~167ms | 222,000 lookups + 2472 A* | - |
| Phase 1 (hash table) | ~24ms | O(1) lookups | 7x faster |
| Phase 2-3 (indexes) | ~22ms | indexed access | minor |
| Phase 4 (multi-target) | ~20ms | ~279 Dijkstra | 10% faster |

**Total improvement: 8x faster (167ms → 20ms)**

## Files Changed

- `pathing/pathfinding.c`:
  - `entranceHash[]` - hash table for position lookups
  - `BuildEntranceHash()`, `HashLookupEntrance()` - hash operations
  - `oldToNewEntranceIndex[]` - precomputed old→new mapping
  - `chunkEntrances[][]`, `chunkEntranceCount[]` - chunk→entrances index
  - `BuildChunkEntranceIndex()` - builds chunk index
  - `RebuildAffectedEdges()` - uses all optimizations
  - `AStarChunkMultiTarget()` - existing function, now used for edge rebuilding

## Test Verification

All tests pass, including the critical `"incremental update should produce same result as full rebuild"` test that validates graph correctness.
