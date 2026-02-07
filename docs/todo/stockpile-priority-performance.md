# Stockpile Priority for Initial Placement: Performance vs Correctness

**Status**: DEFERRED (Architectural Redesign Required)  
**Priority**: Medium  
**Complexity**: High

## The Problem

Currently, `FindStockpileForItem` uses early-exit optimization: it returns the **first** stockpile with available space, ignoring priority. This is fast (O(n) but exits early) but incorrect from a player perspective.

### Player Expectation
When a player sets stockpile priorities, they expect:
- Items should go to **HIGH priority** stockpiles first
- Only when high-priority stockpiles are full should items go to lower-priority ones
- Priority should work for **initial placement**, not just rehaul operations

### Current Behavior
- Items go to the first stockpile with space (by array index order)
- Priority is ignored during initial placement
- Priority only works for **rehaul** operations (WorkGiver_Rehaul moves items from low→high priority)
- This is a "won't fix" by design due to performance

## Why This Happens

### Current Implementation (Fast but Wrong)
```c
// Pseudocode of current behavior
for (int i = 0; i < MAX_STOCKPILES; i++) {
    if (has_space(i)) {
        return i;  // EARLY EXIT - O(1) to O(n) depending on luck
    }
}
```
**Performance**: Very fast when stockpiles are sparse. Average case O(5-10) iterations.

### Correct Implementation (Slow but Right)
```c
// What we implemented in the fix
int bestIdx = -1;
int bestPriority = 0;
for (int i = 0; i < MAX_STOCKPILES; i++) {
    if (has_space(i) && priority[i] > bestPriority) {
        bestPriority = priority[i];
        bestIdx = i;
    }
}
return bestIdx;  // NO EARLY EXIT - always O(n)
```
**Performance**: Must scan ALL 64 stockpiles every time. Measurably slower.

## Performance Analysis

### Frequency
`FindStockpileForItem` is called:
- Once per item type per frame during `RebuildStockpileSlotCache()` (~25 item types)
- Additional calls during job assignment (high frequency)
- Total: **hundreds to thousands of calls per frame**

### Cost
- Current: ~5-10 iterations per call (early exit)
- With priority: 64 iterations per call (no early exit)
- **6-12x slowdown** in this hot path

### Impact
In playtesting, this caused noticeable frame drops in large bases with many stockpiles and items being hauled simultaneously.

## Previous Attempt

This fix was **already attempted and rolled back** (see `docs/bugs/wrong-stockpile-priority.md`):
- Implemented priority-aware search
- Performance regression was unacceptable
- Rolled back to fast early-exit version
- Documented as "won't fix"

## Possible Solutions

### Option 1: Pre-Sorted Priority List ⭐ RECOMMENDED
**Idea**: Maintain a sorted list of stockpiles by priority, updated only when priorities change.

```c
typedef struct {
    int stockpileIdx;
    int priority;
} StockpilePriorityEntry;

StockpilePriorityEntry sortedStockpiles[MAX_STOCKPILES];
int sortedStockpileCount;
bool priorityListDirty;

// Rebuild only when priorities change or stockpiles added/removed
void RebuildStockpilePriorityList() {
    // Copy active stockpiles
    sortedStockpileCount = 0;
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (stockpiles[i].active) {
            sortedStockpiles[sortedStockpileCount++] = (StockpilePriorityEntry){
                .stockpileIdx = i,
                .priority = stockpiles[i].priority
            };
        }
    }
    // Sort descending by priority (qsort)
    qsort(sortedStockpiles, sortedStockpileCount, sizeof(StockpilePriorityEntry), cmp_priority_desc);
    priorityListDirty = false;
}

// Now iterate in priority order with early exit!
int FindStockpileForItem(ItemType type, uint8_t material, int* outSlotX, int* outSlotY) {
    if (priorityListDirty) RebuildStockpilePriorityList();
    
    // Iterate in PRIORITY ORDER (high to low)
    for (int i = 0; i < sortedStockpileCount; i++) {
        int spIdx = sortedStockpiles[i].stockpileIdx;
        if (has_space(spIdx, type, material)) {
            return spIdx;  // EARLY EXIT - but we checked high priority first!
        }
    }
    return -1;
}
```

**Pros**:
- ✅ Correct priority behavior
- ✅ Early exit optimization preserved
- ✅ Sort cost amortized (only when priorities change)
- ✅ Typical case: iterate 5-10 stockpiles (high priority ones), not all 64

**Cons**:
- Memory: +256 bytes for sorted list
- Complexity: Need to invalidate on priority change, create, delete
- Edge case: Need to handle mid-iteration changes

**Estimated Performance**: ~Same as current (early exit preserved) with correct behavior!

---

### Option 2: Hybrid Approach
Only respect priority when there are **fewer than N stockpiles** (e.g., N=10).

```c
int FindStockpileForItem(...) {
    if (activeStockpileCount <= 10) {
        // Use priority-aware search (scan all)
        return FindBestPriorityStockpile(...);
    } else {
        // Use fast early-exit (ignore priority)
        return FindFirstAvailableStockpile(...);
    }
}
```

**Pros**:
- ✅ Good for small-medium bases (where priority matters most)
- ✅ Doesn't hurt large bases
- ✅ Simple to implement

**Cons**:
- ❌ Inconsistent behavior (confusing for players)
- ❌ Arbitrary threshold
- ❌ Doesn't scale to megabases

---

### Option 3: Priority Tiers (Coarse Granularity)
Instead of 1-9 priorities, use 3 tiers: HIGH (7-9), MEDIUM (4-6), LOW (1-3).

```c
int FindStockpileForItem(...) {
    // Check HIGH priority stockpiles first
    for (tier = HIGH; tier >= LOW; tier--) {
        for each stockpile in tier:
            if (has_space) return stockpile;  // Early exit per tier
    }
}
```

**Pros**:
- ✅ Better than no priority
- ✅ Still allows early exit within each tier
- ✅ Reduces search space

**Cons**:
- ❌ Less granular control
- ❌ Still requires scanning all tiers in worst case
- ❌ Doesn't fully solve the problem

---

### Option 4: Cache Per Priority Level
Cache one available slot per (item type, material, priority) combination.

**Pros**:
- ✅ O(1) lookup
- ✅ Fully correct

**Cons**:
- ❌ Memory explosion: ITEM_TYPE_COUNT × MAT_COUNT × 9 priorities = ~2,000 cache entries
- ❌ Complex invalidation logic
- ❌ Not worth it

---

### Option 5: Accept Current Behavior (Status Quo)
Keep the fast early-exit, document that priority only applies to rehaul.

**Pros**:
- ✅ No work required
- ✅ Fast
- ✅ Rehaul eventually moves items to right place

**Cons**:
- ❌ Confusing for players
- ❌ Items churn (hauled twice instead of once)
- ❌ Priority feels "broken"

---

## Recommendation

**Implement Option 1: Pre-Sorted Priority List**

This gives us the best of both worlds:
1. **Correct behavior**: High-priority stockpiles are checked first
2. **Fast performance**: Early exit preserved, sorted list is small (~10-20 active stockpiles)
3. **Scalable**: Sort cost is amortized, happens rarely (only on priority change)

### Implementation Plan

1. **Phase 1: Data Structure** (1-2 hours)
   - Add `sortedStockpiles[]` array
   - Add `RebuildStockpilePriorityList()` function
   - Call from `CreateStockpile`, `DeleteStockpile`, `SetStockpilePriority`

2. **Phase 2: Update FindStockpileForItem** (1 hour)
   - Iterate `sortedStockpiles` instead of all stockpiles
   - Keep early-exit logic

3. **Phase 3: Testing** (2 hours)
   - Re-enable priority test
   - Performance benchmark: measure frame time with 1000 items + 50 stockpiles
   - Verify no regression vs current implementation

4. **Phase 4: Edge Cases** (1 hour)
   - Handle stockpile create/delete during iteration (mark dirty, rebuild next frame)
   - Handle priority changes mid-frame

**Total Estimate**: 5-6 hours

---

## References

- Original bug report: `docs/bugs/wrong-stockpile-priority.md`
- Audit finding: `docs/todo/audits/stockpiles.md` Finding 9
- Test case: `tests/test_jobs.c` line 8693 (currently disabled)
- Related code: `src/entities/stockpiles.c` `FindStockpileForItem()`

---

## Decision

**Status**: Deferred until Option 1 can be implemented properly.

**Current Workaround**: Players can use rehaul jobs (WorkGiver_Rehaul) to move items from low→high priority stockpiles. This happens automatically when high-priority stockpiles have space. It's not ideal (items are hauled twice), but it works.

**Test Status**: Priority test disabled with `if (0)` to prevent red noise. Re-enable when fixed.
