# Hauling System - Next Features

This document describes features not yet implemented in the hauling/stockpile system.

## Current State (Implemented)

- Items spawn on map (RED, GREEN, BLUE types)
- Stockpiles with type filters
- Movers pick up items, carry them, deliver to stockpiles
- Reservation system (item + stockpile slot)
- Safe-drop on cancellation (stockpile deleted, filter changed)
- 29 tests passing

---

## 1. Unreachable Item Cooldown

### Problem
If an item is behind a wall (unreachable), movers will repeatedly try to path to it every tick and fail. This wastes CPU and can cause movers to ignore reachable items while they spam-retry unreachable ones.

### Solution
Track failed path attempts per item. After N failures (or 1 failure), mark the item with a cooldown timestamp. Skip items in cooldown during job assignment.

### Data Changes
```c
// Add to Item struct:
float unreachableCooldown;  // seconds until we retry this item
```

### Logic Changes
In `AssignJobs()`:
- Skip items where `unreachableCooldown > 0`
- Decrement cooldowns each tick
- When pathfinding to item fails, set `unreachableCooldown = 5.0f` (or similar)

### Test (from convo-jobs.md, Test 8)
```
Setup:
- Map with walled pocket (items inside, agent outside)
- 1 agent, 1 unreachable item, valid stockpile

Assert:
- Agent ends idle
- System doesn't spam-retry every tick
- Item remains on ground
```

---

## 2. Gather Zones

### Problem
Currently movers haul ANY item on the map. Player may want to designate specific areas to collect from (mining zone, farm, battlefield loot area).

### Solution
Add "gather zone" regions. Only items inside a gather zone (or items with no zone requirement) are eligible for hauling.

### Data Changes
```c
typedef struct {
    int x, y, z;
    int width, height;
    bool active;
} GatherZone;

#define MAX_GATHER_ZONES 32
extern GatherZone gatherZones[MAX_GATHER_ZONES];
```

### Logic Changes
In `AssignJobs()`:
- If gather zones exist, only consider items inside one of them
- If no gather zones exist, all items are eligible (current behavior)

### Test (from convo-jobs.md, Test 10)
```
Setup:
- 2 items: one inside gather zone, one outside
- 1 agent, valid stockpile

Assert:
- Only in-zone item is hauled
- Out-of-zone item stays on ground
```

### UI Needed
- Button or draw mode to create gather zones
- Visual overlay showing gather zone boundaries

---

## 3. Stacking / Merging

### Problem
Currently each stockpile tile holds exactly 1 item. This wastes space and feels unrealistic. Real stockpiles stack items of the same type.

### Solution
Allow multiple items (or a stack count) per tile, up to a max. When delivering, prefer tiles with existing partial stacks of the same type.

### Design Choice A: Stack Count
```c
// Stockpile slot becomes:
int slotItemType;   // which type, or -1 if empty
int slotCount;      // how many (1-10)
int slotMax;        // max per slot (e.g., 10)
```
Items "merge" into the count. Individual item entities are consumed.

### Design Choice B: Multiple Items Per Slot
```c
int slots[MAX_SLOTS][MAX_STACK];  // array of item indices per slot
int slotCounts[MAX_SLOTS];        // how many in each slot
```
Each item remains a separate entity.

### Logic Changes
In `FindFreeStockpileSlot()`:
1. First, find slot with same type that isn't full (merge target)
2. If none, find empty slot
3. If none, stockpile is full

In drop logic:
- If merging, increment stack count (or add to slot array)
- If new slot, place normally

### Tests
```
Test: Merge into existing stack
- Stockpile has slot with 3 red items
- Mover delivers 1 red item
- Slot now has 4 red items

Test: Don't merge different types
- Stockpile has slot with 3 red items
- Mover delivers green item
- Green goes to different slot

Test: Stack full triggers new slot
- Slot has 10/10 red items
- Mover delivers red item
- Goes to new empty slot
```

---

## 4. Stockpile Priority

### Problem
Player may want a "dump zone" for quick unloading and a "proper storage" for long-term. Items should eventually migrate from low-priority to high-priority stockpiles.

### Solution
Add priority field to stockpiles. Generate "re-haul" jobs to move items from low to high priority storage.

### Data Changes
```c
// Add to Stockpile struct:
int priority;  // higher = better storage (1-9)
```

### Logic Changes
New job type or extended haul logic:
1. Find item in low-priority stockpile
2. Find slot in higher-priority stockpile that accepts it
3. Haul from stockpile A to stockpile B

### When to Generate Re-haul Jobs
- When movers are idle and no ground items need hauling
- Or on a slower tick (every few seconds scan for re-haul opportunities)

### Tests
```
Test: Re-haul to better storage
- Stockpile A (priority 1) has red item
- Stockpile B (priority 5) accepts red, has space
- Idle mover moves item from A to B

Test: Don't re-haul if same priority
- Both stockpiles priority 3
- No re-haul jobs generated

Test: Don't re-haul to worse storage
- Item in priority 5 stockpile
- Empty priority 1 stockpile
- Item stays put
```

---

## Implementation Order Suggestion

1. **Unreachable cooldown** - Small change, big CPU savings, no UI needed
2. **Stacking/merging** - Medium effort, big UX improvement
3. **Stockpile priority** - Medium effort, needs re-haul job logic
4. **Gather zones** - Needs UI, can defer

---

## Notes

- These features are independent and can be implemented in any order
- All have clear test cases that can be written first (TDD)
- Stacking requires deciding between "stack count" vs "multiple items" model
