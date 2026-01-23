# Pathing System Optimization Plan

This document outlines planned optimizations for the pathing subsystem, mapped to patterns from [game-mechanics-optimizations](https://github.com/raduacg/game-mechanics-optimizations).

---

## Overview

The pathing system handles movers, items, stockpiles, and job assignment. Current bottlenecks involve O(n) scans over large arrays (up to 25,000 items, 10,000 movers) where spatial data structures or state-based filtering could reduce complexity significantly.

### Current State
- `MoverSpatialGrid` - ✅ Well-implemented for mover-mover queries
- `ItemSpatialGrid` - ✅ Implemented but underutilized
- Idle mover cache - ✅ Already avoids full mover scans
- Stockpile slot searches - ❌ Brute force nested loops
- Item state filtering - ❌ Full array scans

---

## Planned Optimizations

### Phase 1: Quick Wins (Trivial Changes)

#### 1.1 ItemsTick - Iterate Active Items Only
| | |
|---|---|
| **File** | `pathing/items.c:97-107` |
| **Pattern** | Sparse Data Structure |
| **Current** | `for (int i = 0; i < MAX_ITEMS; i++)` - iterates 25,000 slots |
| **Change** | Iterate only active items using `itemCount` or active list |
| **Complexity** | O(MAX_ITEMS) → O(itemCount) |
| **Lines** | ~5 |

```c
// Before
for (int i = 0; i < MAX_ITEMS; i++) {
    if (!items[i].active) continue;
    // ...
}

// After - need to ensure itemCount is accurate
// Option A: Just use itemCount as upper bound (items may be sparse)
// Option B: Maintain dense active list (more invasive)
```

#### 1.2 BuildItemSpatialGrid - Same Fix
| | |
|---|---|
| **File** | `pathing/items.c:272-330` |
| **Pattern** | Sparse Data Structure |
| **Current** | Iterates `MAX_ITEMS` in two phases |
| **Change** | Same approach as 1.1 |
| **Lines** | ~5 |

---

### Phase 2: Spatial Grid Utilization (Low Effort, High Impact)

#### 2.1 FindNearestUnreservedItem → Spatial Query ⭐
| | |
|---|---|
| **File** | `pathing/items.c:78-95` |
| **Pattern** | **Spatial Partitioning** (pattern #02) |
| **Current** | O(25,000) brute force distance check to all items |
| **Change** | Use `IterateItemsInRadius` with expanding radius search |
| **Complexity** | O(MAX_ITEMS) → O(radius² × items_per_cell) |
| **Lines** | ~25-30 |

**Implementation approach:**
1. Start with small radius (e.g., 10 tiles)
2. Query items in radius using existing `IterateItemsInRadius`
3. Filter for unreserved items, track nearest
4. If none found, expand radius and retry (or fall back to full scan)

```c
// New callback context
typedef struct {
    float searchX, searchY, searchZ;
    int nearestIdx;
    float nearestDistSq;
} NearestItemContext;

// Callback for spatial iteration
static bool NearestUnreservedCallback(int itemIdx, float distSq, void* userData) {
    NearestItemContext* ctx = (NearestItemContext*)userData;
    if (items[itemIdx].reservedBy != -1) return true;  // skip reserved
    if (distSq < ctx->nearestDistSq) {
        ctx->nearestDistSq = distSq;
        ctx->nearestIdx = itemIdx;
    }
    return true;  // continue searching
}

int FindNearestUnreservedItem(float x, float y, float z) {
    int tileX = (int)(x / CELL_SIZE);
    int tileY = (int)(y / CELL_SIZE);
    int tileZ = (int)z;
    
    NearestItemContext ctx = { x, y, z, -1, 1e30f };
    
    // Try expanding radii
    int radii[] = {10, 25, 50, 100};
    for (int r = 0; r < 4; r++) {
        IterateItemsInRadius(tileX, tileY, tileZ, radii[r], 
                             NearestUnreservedCallback, &ctx);
        if (ctx.nearestIdx >= 0) return ctx.nearestIdx;
    }
    
    // Fallback to full scan only if spatial search fails
    // (shouldn't happen with reasonable radius)
    return -1;
}
```

#### 2.2 FindGroundItemOnStockpile → Spatial Query
| | |
|---|---|
| **File** | `pathing/stockpiles.c:429-456` |
| **Pattern** | **Spatial Partitioning** (pattern #02) |
| **Current** | O(64 stockpiles × 256 slots) nested loops |
| **Change** | Query `ItemSpatialGrid` for each stockpile's bounding box |
| **Complexity** | O(16,384) → O(stockpiles × items_in_bounds) |
| **Lines** | ~30-40 |

**Implementation approach:**
1. For each active stockpile, compute tile bounds
2. Use `IterateItemsInRadius` centered on stockpile with radius = max(width, height)
3. Check if found items are actually on stockpile tiles

---

### Phase 3: State-Based Filtering (Medium Effort)

#### 3.1 Stockpiled Item List for Re-haul Jobs
| | |
|---|---|
| **File** | `pathing/jobs.c:342-363` + `pathing/items.c` |
| **Pattern** | **Dirty Flags** (pattern #04) / State Lists |
| **Current** | O(25,000) scan filtering for `ITEM_IN_STOCKPILE` |
| **Change** | Maintain separate list of items in stockpiles |
| **Complexity** | O(MAX_ITEMS) → O(stockpiledItemCount) |
| **Lines** | ~60-80 |

**Implementation approach:**
1. Add to `items.h`:
   ```c
   extern int stockpiledItemList[MAX_ITEMS];
   extern int stockpiledItemCount;
   ```
2. Update list when item state changes to/from `ITEM_IN_STOCKPILE`
3. In `AssignJobs` Priority 3, iterate `stockpiledItemList` instead of all items

**State transitions to hook:**
- `JobsTick` when item dropped at stockpile → add to list
- `AssignJobs` when item picked up for re-haul → remove from list
- Item deletion → remove from list if present

---

### Phase 4: Stockpile Slot Caching (Medium Effort)

#### 4.1 Free Slot Tracking per Stockpile
| | |
|---|---|
| **File** | `pathing/stockpiles.c:139-177` |
| **Pattern** | **Object Pooling** (pattern #01) / Free Lists |
| **Current** | O(512) two-pass scan per stockpile |
| **Change** | Track free slot count, optionally maintain free list |
| **Complexity** | O(256) → O(1) for "has free slot?" check |
| **Lines** | ~80-100 |

**Implementation approach (simple version):**
1. Add to `Stockpile` struct:
   ```c
   int freeSlotCount;      // slots with count < maxStackSize
   int emptySlotCount;     // slots with count == 0
   ```
2. Update counts on:
   - `ReserveStockpileSlot` → decrement
   - `ReleaseStockpileSlot` → increment
   - `SetStockpileMaxStackSize` → recalculate
3. In `FindStockpileForItem`, skip stockpiles with `freeSlotCount == 0`

**Implementation approach (full free list):**
1. Maintain linked list of free slot indices per stockpile
2. O(1) allocation and deallocation
3. More invasive but eliminates slot scanning entirely

#### 4.2 Cascading Benefits
Once 4.1 is implemented:
- `FindStockpileForItem` - skip full stockpiles in O(1)
- `FindStockpileForOverfullItem` - same benefit
- `FindHigherPriorityStockpile` - same benefit

---

### Phase 5: Spatial Hashing for Zones (Medium Effort)

#### 5.1 Tile → Stockpile Lookup Grid
| | |
|---|---|
| **File** | `pathing/stockpiles.c:195-210` |
| **Pattern** | **Spatial Hashing** (pattern #07) |
| **Current** | O(64) linear scan of all stockpiles |
| **Change** | 2D grid mapping tile → stockpile index |
| **Complexity** | O(MAX_STOCKPILES) → O(1) |
| **Lines** | ~50-60 |

**Implementation approach:**
1. Add grid: `int8_t stockpileTileOwner[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH]`
   - Value = stockpile index, or -1 if no stockpile
2. Populate on `CreateStockpile`, clear on `DeleteStockpile`
3. `IsPositionInStockpile` becomes simple array lookup

**Memory consideration:** 
- With 128×128×8 grid = 128KB
- Could use sparse representation if memory is concern

#### 5.2 Tile → Gather Zone Lookup (Lower Priority)
| | |
|---|---|
| **File** | `pathing/stockpiles.c:247-262` |
| **Pattern** | **Spatial Hashing** (pattern #07) |
| **Current** | O(32) linear scan |
| **Change** | Same approach as 5.1 |
| **Lines** | ~40-50 |

Lower priority because only 32 gather zones max, and the scan is simpler.

---

### Phase 6: LOS Caching (Medium Effort, Situational Benefit)

#### 6.1 Line-of-Sight Result Cache
| | |
|---|---|
| **File** | `pathing/mover.c:571-612` |
| **Pattern** | **Dirty Flags** (pattern #04) |
| **Current** | Bresenham check per mover per frame (staggered) |
| **Change** | Cache LOS results, invalidate on wall changes |
| **Lines** | ~60-80 |

**Implementation approach:**
1. Hash (startCell, endCell) → bool visible
2. On wall change, invalidate affected entries (or clear all)
3. Check cache before running Bresenham

**Tradeoffs:**
- Memory for cache
- Cache invalidation complexity
- May not be worth it if walls change frequently

---

## Summary Table

| # | Optimization | Pattern | Lines | Impact | Priority |
|---|--------------|---------|-------|--------|----------|
| 1.1 | ItemsTick iterate active | Sparse Data | ~5 | Medium | High |
| 1.2 | BuildItemSpatialGrid iterate active | Sparse Data | ~5 | Medium | High |
| 2.1 | FindNearestUnreservedItem spatial | Spatial Partitioning | ~25 | **High** | **Critical** |
| 2.2 | FindGroundItemOnStockpile spatial | Spatial Partitioning | ~35 | Medium | Medium |
| 3.1 | Stockpiled item list | Dirty Flags | ~70 | High | High |
| 4.1 | Free slot tracking | Object Pooling | ~90 | High | Medium |
| 5.1 | Stockpile tile lookup | Spatial Hashing | ~55 | Medium | Low |
| 5.2 | Gather zone tile lookup | Spatial Hashing | ~45 | Low | Low |
| 6.1 | LOS caching | Dirty Flags | ~70 | Low-Medium | Low |

---

## Recommended Implementation Order

1. **Phase 1** (1.1, 1.2) - ~10 lines, immediate small gains
2. **Phase 2.1** - ~25 lines, biggest single improvement
3. **Phase 3.1** - ~70 lines, eliminates O(MAX_ITEMS) in job assignment
4. **Phase 4.1** - ~90 lines, speeds up all stockpile queries
5. **Phase 2.2** - ~35 lines, uses spatial grid for stockpile ground items
6. **Phase 5+** - Lower priority, diminishing returns

Total for phases 1-4: ~195 lines for major performance improvements.

---

## References

- [Spatial Partitioning](https://github.com/raduacg/game-mechanics-optimizations) - Pattern #02
- [Dirty Flags](https://github.com/raduacg/game-mechanics-optimizations) - Pattern #04
- [Object Pooling](https://github.com/raduacg/game-mechanics-optimizations) - Pattern #01
- [Spatial Hashing](https://github.com/raduacg/game-mechanics-optimizations) - Pattern #07
