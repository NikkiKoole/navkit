# Item Spatial Optimization

Notes on optimizing item lookups in the job assignment system.

## Current Problem

`AssignJobs()` scans all `MAX_ITEMS` for each idle mover:
- `FindGroundItemOnStockpile()` - scans all stockpiles and their tiles
- Ground item loop - O(MAX_ITEMS) per mover
- Re-haul item loop - O(MAX_ITEMS) per mover

With many movers and items, this becomes O(movers * items) per tick.

## Existing Pattern: MoverSpatialGrid

NavKit already has a spatial grid for mover neighbor queries (`mover.h:116`):

```c
typedef struct {
    int* cellCounts;    // Number of movers per cell
    int* cellStarts;    // Prefix sum: start index for each cell in moverIndices
    int* moverIndices;  // Mover indices sorted by cell
    int gridW, gridH;   // Grid dimensions in cells
    int cellCount;      // Total cells (gridW * gridH)
    float invCellSize;  // 1.0 / MOVER_GRID_CELL_SIZE for fast division
} MoverSpatialGrid;
```

Uses counting sort with prefix sums - O(n) rebuild, O(1) cell lookup.

## Proposed: ItemSpatialGrid

Same pattern for items:

```c
typedef struct {
    int* cellCounts;
    int* cellStarts;
    int* itemIndices;   // Item indices sorted by cell
    int gridW, gridH;
    int cellCount;
    float invCellSize;
} ItemSpatialGrid;
```

### Benefits

1. **FindNearestItem** - only check cells near the mover, not all items
2. **FindGroundItemOnStockpile** - could use stockpile bounds to query relevant cells
3. **Cache locality** - items in same area stored contiguously

### Rebuild Frequency

Items move less frequently than movers:
- Ground items: stationary until picked up
- Carried items: follow mover (don't need grid lookup)
- Stockpiled items: stationary

Could rebuild only when items are dropped/placed, or lazily once per tick if any item moved.

## Profiling Results

Added accumulating profilers in `jobs.c` (using new `PROFILE_ACCUM_BEGIN`/`PROFILE_ACCUM_END`):
- `Jobs_FindStockpileItem` - FindGroundItemOnStockpile()
- `Jobs_FindGroundItem` - ground item scan loop **<-- BOTTLENECK**
- `Jobs_FindRehaulItem` - re-haul candidate scan
- `Jobs_FindDestination` - stockpile slot finding
- `Jobs_ReachabilityCheck` - FindPath() for reachability

**Result:** `Jobs_FindGroundItem` is the bottleneck. It scans all MAX_ITEMS for every idle mover, and inside that calls `FindStockpileForItem()` which scans stockpiles.

Worst case complexity: O(idle_movers * MAX_ITEMS * stockpiles) per tick.

**Scaling problem:** Current MAX_ITEMS is 1024. Want to support 10k+ items eventually. With O(n) scans, this doesn't scale - need spatial indexing to keep job assignment fast regardless of total item count.

**Target performance:**
- 10,000 movers, 100,000 items
- `AssignJobs()` should complete in <5ms per tick (currently 300+ms with just 1024 items and ~100 movers)
- Grid rebuild: <2ms for 100k items
- Individual query (nearest item to mover): <0.01ms

## Key Queries Needed

From `convo-jobs.md`:

> A way to query: "items at tile" and "all items needing hauling"

These are the two fundamental queries the item system needs to support efficiently:

1. **Items at tile** - for stockpile slot checking, absorb/clear jobs
2. **All items needing hauling** - ground items in gather zones, not reserved, no cooldown

Currently both are O(n) scans. With proper indexing:
- "Items at tile" becomes O(1) with a tile-to-item map or spatial grid
- "Items needing hauling" could use a separate list/set maintained on state changes

## Recommended Approach: ItemSpatialGrid

Implement spatial grid for items, following the existing `MoverSpatialGrid` pattern.

**Why spatial grid over a "haulable items list":**
- Solves both queries: "items at tile" (O(1)) and "nearest item to mover" (O(nearby cells))
- No separate list to maintain - just query the grid with filters
- Matches natural behavior: a person looks around them, grabs what's close
- Already have working pattern in `MoverSpatialGrid`

**Implementation steps:**
1. Add `ItemSpatialGrid` struct to `items.h` (copy `MoverSpatialGrid` pattern)
2. Add `InitItemSpatialGrid()`, `FreeItemSpatialGrid()`, `BuildItemSpatialGrid()`
3. Add `QueryItemsInRadius()` or `QueryItemsAtTile()` functions
4. Replace `FindGroundItemAtTile()` to use grid lookup
5. Replace item scan in `AssignJobs()` to query nearby cells instead of all items

**Expected improvement:**
- "Items at tile" goes from O(MAX_ITEMS) to O(items in cell)
- "Nearest haulable item" goes from O(MAX_ITEMS) to O(items in nearby cells)
- Rebuild cost is O(items) once per tick, but could optimize with dirty flags later

## Other Optimizations (lower priority)

1. **Early-out on first valid item** - don't find "nearest", just find "any valid"
2. **Round-robin item assignment** - track last assigned item index, continue from there
3. **Dirty flags** - only rebuild grid when items change state
4. **Per-stockpile item lists** - stockpiles track which items are on their tiles
