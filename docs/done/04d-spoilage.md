# 04d: Spoilage

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Status**: ✅ Done (save v71 for timer, v72 for ITEM_ROT, v73 for ItemCondition refactor)
> **Deps**: None (infrastructure feature), but most useful after 04a adds meat
> **Opens**: Preservation pressure (drying, cooking, container quality)
> **See also**: `04d2-item-rot.md` — ItemCondition refactoring that replaced ITEM_ROT

---

## Goal

Items with IF_SPOILS decay over time and transition through condition states (FRESH → STALE → ROTTEN). Rotten items keep their original type but can't be eaten and are accepted as fuel. This creates pressure to cook, dry, and store food properly. Containers with lower spoilageModifier preserve food longer.

---

## What Needs Building

### 1. Item Struct — Add spoilageTimer (`items.h`)

```c
float spoilageTimer;    // Game-seconds elapsed (0 = fresh)
```

Also update `ClearItems()` (items.c:17) to zero-init the new field, and `SpawnItem()`/`SpawnItemWithMaterial()` to set it to 0.

### 2. ItemDef — Add spoilageLimit (`item_defs.h`)

```c
float spoilageLimit;    // Game-seconds until spoiled (0 = doesn't spoil)
```

Add `ItemSpoilageLimit(t)` accessor macro alongside existing `ItemSpoils(t)`.

`IF_SPOILS` flag (bit 3) already exists in item_defs.h but isn't applied to any items yet.

### 3. Spoilage Rates

| Item | spoilageLimit | Notes |
|------|--------------|-------|
| ITEM_CARCASS | 60s | Must butcher quickly |
| ITEM_RAW_MEAT | 120s | Cook or dry it |
| ITEM_COOKED_MEAT | 300s | Lasts a while |
| ITEM_BERRIES | 480s | Slow spoilage |
| ITEM_ROOT | 480s | Same as berries |
| ITEM_ROASTED_ROOT | 300s | Same as cooked meat |

**Non-spoiling**: ITEM_DRIED_BERRIES, ITEM_DRIED_ROOT, ITEM_HIDE — no IF_SPOILS flag.

### 4. Container Modifiers (already defined in `containers.c`)

```c
[ITEM_BASKET]   = { .spoilageModifier = 1.0f },  // open, no benefit
[ITEM_CHEST]    = { .spoilageModifier = 0.7f },  // enclosed
[ITEM_CLAY_POT] = { .spoilageModifier = 0.5f },  // sealed, best
```

These just need to be read during the spoilage tick.

### 5. SpoilageTick Function

Add to `items.c` (inside `ItemsTick`, or as a separate function called from `main.c` after `ItemsTick`).

```
SpoilageTick(float dt):
  For each active item (i < itemHighWaterMark):
    Skip if !IF_SPOILS or spoilageLimit == 0
    Skip if condition == CONDITION_ROTTEN (already done)
    Skip if state == ITEM_CARRIED (don't spoil mid-job, see Design Notes)

    rate = dt
    If containedIn >= 0:
      Walk containedIn chain to outermost container
      If outermost is ITEM_CARRIED → skip
      rate *= GetContainerDef(outermost.type).spoilageModifier

    spoilageTimer += rate
    ratio = spoilageTimer / spoilageLimit
    If ratio >= 1.0: condition = CONDITION_ROTTEN, EventLog(...)
    Else if ratio >= 0.5: condition = CONDITION_STALE
```

**Tick frequency**: Call every frame with `dt` (same as `ItemsTick` which already iterates items). No need for a separate interval — the per-item work is trivial (flag check + float add).

**Container modifier**: Walk `containedIn` to outermost container (use existing `GetOutermostContainer()` from containers.c). Apply that container's `spoilageModifier`. If nested (basket inside chest), use outermost — the outer container determines environment.

### 6. Stack Merge — spoilageTimer handling (`stacking.c`)

`MergeItemIntoStack()` currently ignores spoilageTimer. When merging two stacks of spoilable items:
- **Take the worse (higher) timer** — conservative, simple, no exploit potential.
- This means merging fresh berries into an old stack makes the whole stack older.

### 7. Stack Split — copy spoilageTimer (`stacking.c`)

`SplitStack()` spawns via `SpawnItemWithMaterial()` which sets timer to 0. After spawn, **copy spoilageTimer from the original**:
```c
items[newIdx].spoilageTimer = items[itemIdx].spoilageTimer;
```

### 8. Save/Load — version bump

- Current version: v70 → bump to v71
- Add `ItemV70` typedef in `save_migrations.h` (old Item layout without spoilageTimer)
- Migration: set `spoilageTimer = 0.0f` for all loaded items
- Update `inspect.c` to match

### 9. Visual Feedback

**Tooltip** (tooltips.c `DrawItemTooltip`): Show freshness label after item name for IF_SPOILS items.

Helper function:
```c
const char* GetFreshnessLabel(float timer, float limit) {
    float ratio = timer / limit;
    if (ratio < 0.5f) return "Fresh";
    if (ratio < 0.8f) return "Stale";
    return "Spoiling";
}
```

**Color tint** (rendering.c): Optional — tint item sprite slightly green/brown when ratio > 0.5. Keep it subtle.

### 10. Event Log

Log spoilage events:
```c
EventLog("SPOILED item %d (%s x%d) at (%.0f,%.0f,z%.0f) after %.0fs",
         idx, ItemName(items[idx].type), items[idx].stackCount,
         items[idx].x, items[idx].y, items[idx].z, items[idx].spoilageTimer);
```

---

## Edge Cases & Design Decisions

### Carried items: don't spoil
Items with `state == ITEM_CARRIED` skip spoilage. Rationale:
- A mover carrying raw meat to a fire pit shouldn't have it vanish mid-walk.
- Keeps job logic simple — no mid-job item deletion to handle.
- Minimal exploit potential (carry durations are short).

### Reserved items: CAN spoil
Items with `reservedBy != -1` but `state != ITEM_CARRIED` (i.e., a mover is pathing toward them) **do** spoil. When this happens:
- `DeleteItem` clears the item.
- The mover's job will fail on next tick when it finds the target item inactive.
- `CancelJob` handles cleanup (already robust).
- This is the desired behavior: "you were too slow, it rotted."

### Stockpiled items: CAN spoil
Items in stockpiles (`state == ITEM_IN_STOCKPILE`) spoil normally. `DeleteItem` already calls `RemoveItemFromStockpileSlot` — cleanup is automatic.

### Container contents: spoil with modifier
Items inside containers spoil at `dt * container.spoilageModifier`. Containers inside stockpiles still apply the modifier. Items inside a carried container: the container itself is ITEM_CARRIED, but the contents are ITEM_IN_CONTAINER — skip them too (check if outermost container is carried).

### Items being processed (cooking, drying)
Items inside workshops (fuel slot, work tile) have `state == ITEM_IN_STOCKPILE` or `ITEM_ON_GROUND`. They can spoil. This is fine — it creates urgency to process food. If a workshop queue is backed up, inputs can rot. Passive workshops (drying rack) already produce non-spoiling output.

### Stacking with different freshness
When `MergeItemIntoStack` runs (e.g., hauler delivers berries to a stockpile slot that already has berries), take the **worse timer** (higher value = more spoiled). Don't block merging based on freshness — that's a future seasoning-curing feature.

### No spoilage while paused
`SpoilageTick` is called from the game loop inside the `!paused` check — automatic.

---

## Implementation Phases

### Phase 1: Data model + save/load
Pure structural changes — no behavior yet. Compiles and runs identically to before.

1. Add `spoilageTimer` to Item struct in `items.h`
2. Add `spoilageLimit` to ItemDef struct in `item_defs.h`, add `ItemSpoilageLimit(t)` accessor macro
3. Zero-init `spoilageTimer` in `ClearItems()`, `SpawnItem()`, `SpawnItemWithMaterial()`
4. Set `spoilageLimit = 0` for all items in `item_defs.c` (no spoilage yet)
5. Save version bump v70→v71, add `ItemV70` migration struct in `save_migrations.h`
6. Migration in `saveload.c`: load old items, set `spoilageTimer = 0.0f`
7. Update `inspect.c` to match new Item layout

**Verify**: `make path`, load an existing save, F5 re-save, `--inspect` works.

### Phase 2: Flag food items
Apply `IF_SPOILS` and set `spoilageLimit` on food items in `item_defs.c`:

| Item | IF_SPOILS | spoilageLimit |
|------|-----------|---------------|
| ITEM_CARCASS | yes | 60.0f |
| ITEM_RAW_MEAT | yes | 120.0f |
| ITEM_COOKED_MEAT | yes | 300.0f |
| ITEM_BERRIES | yes | 480.0f |
| ITEM_ROOT | yes | 480.0f |
| ITEM_ROASTED_ROOT | yes | 300.0f |
| ITEM_DRIED_BERRIES | **no** | 0 |
| ITEM_DRIED_ROOT | **no** | 0 |
| ITEM_HIDE | **no** | 0 |

**Tests**: `describe(spoilage_item_defs)` — verify flags and limits are correct, preserved foods don't have IF_SPOILS.

### Phase 3: Core spoilage tick
The main behavior — items on the ground rot and get deleted.

1. Implement `SpoilageTick(float dt)` in `items.c`
   - Iterate active items up to `itemHighWaterMark`
   - Skip if `!IF_SPOILS` or `spoilageLimit == 0`
   - Skip if `state == ITEM_CARRIED`
   - Advance `spoilageTimer += dt`
   - If `spoilageTimer >= spoilageLimit`: EventLog + `DeleteItem(idx)`
2. Call `SpoilageTick(dt)` from `ItemsTick()` (already iterates items, already called in game loop)

**Tests**: `describe(spoilage_timer)` — timer advances, items delete at limit, carried items skip, reserved items spoil, stockpiled items spoil and free slot, whole stack deletes.

### Phase 4: Container integration
Items in containers spoil slower based on container's `spoilageModifier`.

1. In `SpoilageTick`, when `containedIn >= 0`:
   - Use `GetOutermostContainer()` to find top-level container
   - If outermost container `state == ITEM_CARRIED` → skip (don't spoil)
   - Otherwise: `rate = dt * GetContainerDef(outermost.type).spoilageModifier`
2. Advance `spoilageTimer += rate` instead of `+= dt`

**Tests**: `describe(spoilage_containers)` — clay pot halves rate, basket no benefit, nested uses outermost, carried container skips.

### Phase 5: Stack handling
Spoilage timer interacts correctly with stack merge and split.

1. `MergeItemIntoStack()` in `stacking.c`: after merge, take `fmaxf(existing.spoilageTimer, incoming.spoilageTimer)`
2. `SplitStack()` in `stacking.c`: after spawn, copy `spoilageTimer` from original to new item

**Tests**: `describe(spoilage_stacking)` — merge takes worse timer, split copies timer.

### Phase 6: Visual feedback
Player can see how fresh their food is.

1. Add `GetFreshnessLabel(float timer, float limit)` helper (returns "Fresh"/"Stale"/"Spoiling")
2. Show label in item tooltip (`tooltips.c` `DrawItemTooltip`) for IF_SPOILS items
3. Optional: subtle color tint on item sprites when ratio > 0.5

**Tests**: `describe(spoilage_event_log)` — verify EventLog fires on spoilage.

### End-to-end tests (after all phases)
`describe(spoilage_e2e)` — drying preserves food, cooking extends shelf life, container chain (pot > chest > basket > ground), spoiled item cancels hauler job, spoiled stockpile item frees slot.

---

## Tests

### Unit tests (`test_spoilage.c`)

```
describe(spoilage_timer)
  it("should not advance timer for non-spoilable items")
    Spawn ITEM_ROCK (no IF_SPOILS), tick 1000s, verify item still active.

  it("should advance timer for spoilable items")
    Spawn ITEM_BERRIES, tick 100s, verify spoilageTimer == 100.

  it("should delete item when timer exceeds limit")
    Spawn ITEM_BERRIES (limit=480), tick 480s, verify item deleted.

  it("should delete item exactly at limit")
    Spawn ITEM_BERRIES, set timer to 479, tick 1s, verify deleted.

  it("should not delete item just under limit")
    Spawn ITEM_BERRIES, set timer to 479, tick 0.5s, verify still active.

  it("should not advance timer for carried items")
    Spawn ITEM_RAW_MEAT, set state=ITEM_CARRIED, tick 200s, verify timer==0.

  it("should spoil reserved-but-not-carried items")
    Spawn ITEM_RAW_MEAT, set reservedBy=0 (state stays ITEM_ON_GROUND),
    tick 120s, verify item deleted.

  it("should spoil stockpiled items")
    Spawn ITEM_BERRIES in stockpile slot, tick 480s, verify deleted
    and stockpile slot cleared.

  it("should delete entire stack when spoiled")
    Spawn ITEM_BERRIES with stackCount=10, tick 480s, verify all gone (one deletion).

describe(spoilage_containers)
  it("should apply container modifier — clay pot halves rate")
    Spawn ITEM_BERRIES inside clay pot (modifier=0.5),
    tick 480s, verify still active (effective limit = 960s).
    Tick 480 more, verify deleted.

  it("should apply basket modifier — no benefit")
    Spawn ITEM_BERRIES inside basket (modifier=1.0),
    tick 480s, verify deleted (same as ground).

  it("should use outermost container modifier for nested items")
    Spawn ITEM_BERRIES inside basket inside chest (0.7),
    verify chest modifier applies (not basket).

  it("should not spoil items in a carried container")
    Spawn ITEM_RAW_MEAT inside basket, set basket state=ITEM_CARRIED,
    tick 200s, verify meat still active.

describe(spoilage_stacking)
  it("should take worse timer on stack merge")
    Spawn ITEM_BERRIES A with timer=300, ITEM_BERRIES B with timer=100,
    merge B into A, verify result timer==300.

  it("should copy timer on stack split")
    Spawn ITEM_BERRIES with timer=200 and stackCount=10,
    split 5, verify new item timer==200.

describe(spoilage_item_defs)
  it("should have IF_SPOILS on all raw food items")
    Verify ITEM_CARCASS, ITEM_RAW_MEAT, ITEM_COOKED_MEAT, ITEM_BERRIES,
    ITEM_ROOT, ITEM_ROASTED_ROOT all have IF_SPOILS flag set.

  it("should NOT have IF_SPOILS on preserved foods")
    Verify ITEM_DRIED_BERRIES, ITEM_DRIED_ROOT do NOT have IF_SPOILS.

  it("should have correct spoilage limits")
    Verify each food item's spoilageLimit matches the table above.

describe(spoilage_event_log)
  it("should log when item spoils")
    Spawn ITEM_BERRIES, tick past limit, verify EventLog entry contains "SPOILED".
```

### End-to-end tests (`test_spoilage.c`)

```
describe(spoilage_e2e)
  it("drying preserves food — dried berries survive past fresh berry limit")
    Spawn ITEM_DRIED_BERRIES, tick 1000s (well past 480s berry limit),
    verify still active.

  it("cooking extends shelf life — cooked meat lasts longer than raw")
    Spawn ITEM_RAW_MEAT (limit=120), spawn ITEM_COOKED_MEAT (limit=300),
    tick 120s, verify raw deleted + cooked still active.
    Tick 180 more (total 300s), verify cooked now deleted.

  it("container storage chain — pot > chest > basket > ground")
    Spawn 4x ITEM_RAW_MEAT: ground, in basket, in chest, in clay pot.
    Tick 120s. Ground=dead, basket=dead, chest=alive (0.7x → 171s limit),
    pot=alive (0.5x → 240s limit).
    Tick 60s (total 180s). Chest=dead, pot=alive.
    Tick 60s (total 240s). Pot=dead.

  it("spoiled item cancels incoming hauler job")
    Place stockpile. Spawn ITEM_RAW_MEAT on ground.
    Create haul job targeting the meat (reservedBy set).
    Tick 120s. Meat spoils → deleted.
    Next JobsTick: job fails (target inactive), mover returns to idle.

  it("spoiled stockpile item frees the slot")
    Create stockpile, place ITEM_BERRIES in slot 0.
    Verify freeSlotCount reflects occupied slot.
    Tick 480s. Berries spoil.
    Verify slot is now free and freeSlotCount updated.
```

---

## Deferred (not in this feature)

- **Composting**: Rotten items could be composted into fertilizer. Comes with 09 (loop closers).
- **Weather modifier**: Rain accelerating spoilage needs weather integration. Future.
- **Temperature modifier**: Hot weather accelerating spoilage. Future.
- **Eat-oldest-first priority**: Movers could prefer eating food closest to spoiling. Nice optimization but not needed now.
- **Spoilage UI bar**: A visual freshness bar on items. Tooltip label is enough for now.
- **Wood/clay seasoning states**: Broader material conditioning in `gameplay/seasoning-curing.md`. Different system (curing would add more ItemCondition states).
