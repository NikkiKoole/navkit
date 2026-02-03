# Unified Spatial Queries Layer

Unified interface for "find nearest X", k-nearest neighbors, range queries.

## Current State (Feb 2026)

### What Exists

**Items** - Good spatial grid in `items.c`:
- `ItemSpatialGrid` with prefix sums, built once per frame
- `FindNearestUnreservedItem()` - expanding radius search
- `QueryItemsInRadius()` - range query with callback
- `FindFirstItemInRadius()` - find first match

**Movers** - Good spatial grid in `mover.c`:
- `MoverSpatialGrid` with prefix sums, built once per frame
- `QueryMoverNeighbors()` - range query with callback
- `ComputeMoverAvoidance()` - boids-style separation

### What's Duplicated/Scattered

| System | Approach | Problem |
|--------|----------|---------|
| Mine/Channel designations | Cache + linear search | Separate from items/movers |
| Gather/Plant Sapling | Full grid scan O(W×H×D) | Slow, no spatial structure |
| Chop designations | Full grid scan | Same issue |
| Stockpiles | Cache only | Different pattern again |

### What's Missing

- **No single API** - each system has its own functions
- **No k-nearest neighbors** - only "find first" or "find all in radius"
- **Distance calculations duplicated** across all systems
- **Inconsistent caching** - some systems cache, others do full scans

## Proposed Unified API

```c
// Generic spatial query interface
int SpatialFindNearest(SpatialQueryType type, float x, float y, int z, FilterFunc filter);
int SpatialFindKNearest(SpatialQueryType type, float x, float y, int z, int k, int* results, FilterFunc filter);
int SpatialQueryRadius(SpatialQueryType type, float x, float y, int z, float radius, QueryCallback cb, void* userData);
```

## Performance Hotspots to Fix

1. **Gather Sapling** - full O(gridW × gridH × gridD) scan in `WorkGiver_GatherSapling()`
2. **Plant Sapling** - full grid scan in `WorkGiver_PlantSapling()`
3. **Chop designations** - full grid scan (comment says "rare" but still wasteful)

## Files Reference

- Items spatial grid: `src/entities/items.h` (51-104), `src/entities/items.c` (82-556)
- Movers spatial grid: `src/entities/mover.h` (108-127), `src/entities/mover.c` (111-431)
- Designation caches: `src/entities/jobs.c` (64-169, 2248-2252)
- Full grid scans: `src/entities/jobs.c` (2503-2543, 3391-3428)
