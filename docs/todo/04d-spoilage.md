# 04d: Spoilage

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Deps**: None (infrastructure feature), but most useful after 04a adds meat
> **Opens**: Preservation pressure (drying, cooking, container quality)

---

## Goal

Items with IF_SPOILS decay over time and eventually get deleted (or turn into rot). This creates pressure to cook, dry, and store food properly. Containers with lower spoilageModifier preserve food longer.

---

## What Needs Building

### 1. Spoilage Timer on Item Struct

Add to Item struct in `items.h`:
```c
float spoilageTimer;    // Game-seconds since spawn (0 = fresh)
```

Save version bump required. Migration: set spoilageTimer = 0 for all existing items.

### 2. Spoilage Tick

New function in items.c (or a dedicated spoilage.c):

```
SpoilageTick(float dt):
  For each active item with IF_SPOILS:
    spoilageTimer += dt
    Apply container modifier if containedIn >= 0
    Apply weather modifier if exposed to sky (rain accelerates spoilage)
    If spoilageTimer >= spoilageLimit for item type:
      DeleteItem(idx)  // or convert to ITEM_ROT for composting
```

**Tick frequency**: Every game-second is fine (not per-frame). Use the sim_manager interval pattern.

### 3. Spoilage Rates

| Item | Spoilage Time | Notes |
|------|--------------|-------|
| ITEM_CARCASS | 60s (1 game-hour) | Must butcher quickly |
| ITEM_RAW_MEAT | 120s (2 game-hours) | Cook or dry it |
| ITEM_COOKED_MEAT | 300s (5 game-hours) | Lasts a while |
| ITEM_BERRIES | 480s (8 game-hours) | Slow spoilage |
| ITEM_ROOT | 480s (8 game-hours) | Same as berries |
| ITEM_ROASTED_ROOT | 300s (5 game-hours) | Same as cooked meat |

Non-spoiling foods: ITEM_DRIED_BERRIES, ITEM_DRIED_ROOT — that's the point of drying.

### 4. Container Modifiers (already defined)

Container system already has spoilage properties:
- Basket: 1.0x (no benefit)
- Chest: 0.7x (30% slower spoilage)
- Clay pot: 0.5x (50% slower — best preservation)

These just need to be read during the spoilage tick.

### 5. ItemDef Extension

Add spoilage limit to ItemDef:
```c
float spoilageLimit;    // Game-seconds until spoiled (0 = doesn't spoil)
```

Or derive from IF_SPOILS flag + a lookup table. The ItemDef approach is cleaner.

---

## Visual Feedback

- Items approaching spoilage could get a color tint (greenish)
- Tooltip shows freshness: "Fresh" / "Stale" / "Spoiling"
- Optional: spoiled items turn into ITEM_ROT instead of being deleted (future composting)

---

## Implementation Phases

### Phase 1: Timer + Tick
- Add spoilageTimer to Item struct
- Add spoilageLimit to ItemDef
- Implement SpoilageTick (interval-based)
- Save version bump + migration (zero-init)
- **Test**: Spawn item with IF_SPOILS, advance time, verify deletion

### Phase 2: Container Integration
- Read container spoilageModifier during tick
- **Test**: Same item in pot spoils 2x slower than on ground

### Phase 3: Flag Existing Items
- Add IF_SPOILS to berries (existing)
- Add IF_SPOILS + spoilageLimit to new food items from 04a/04c
- Verify dried items do NOT have IF_SPOILS

---

## Design Notes

- **Spoilage is game-seconds, not real-time** — pausing stops spoilage
- **Stacked items**: Whole stack spoils together (same timer). Don't track per-unit.
- **Weather**: Rain on exposed food could accelerate spoilage (future, ties into weather system)
- **Temperature**: Hot weather could accelerate spoilage (future, ties into temperature)
- **Keep it simple first**: Just timer → delete. Rot item + composting can come in 09 (loop closers).
