# Feature 01: Hunger & Foraging

> **Priority**: Tier 1 — Survival Loop
> **Why now**: Colonists are immortal robots. This is THE fundamental missing loop.
> **Systems used**: Berry bushes (CELL_BUSH exists), stockpiles, drying rack, seasons, weather
> **Opens**: Need for cooking (F5), farming (F6), food storage (F8)

---

## What Changes

Movers get a **hunger** stat that drains over time. When hungry, they interrupt work to find food. Food comes from **berry bushes** (wild foraging) and eventually stockpiled food items. Seasons affect berry availability — winter is lean.

This single feature transforms the game from "watch logistics" to "keep people alive."

---

## Design

### Hunger Mechanic

```
hunger: float 0.0 (starving) → 1.0 (full)
Drain rate: ~0.002/sec (≈ 8 minutes real-time from full to starving)
```

**Thresholds:**
- 1.0–0.3: **Full** — normal work
- 0.3–0.1: **Hungry** — seek food when idle (between jobs)
- <0.1: **Starving** — interrupt current job, seek food immediately

### Freetime State Machine

Needs bypass the job system entirely. They're personal interrupts.

```
MoverUpdate:
  1. if starving → cancel job, enter FREETIME_SEEKING_FOOD
  2. if hungry && no job → enter FREETIME_SEEKING_FOOD
  3. if in freetime state → continue freetime behavior
  4. else → normal job assignment
```

States: `FREETIME_NONE → SEEKING_FOOD → EATING → NONE`

### Food Sources (Fallback Chain)

When seeking food, movers check in order:
1. **Stockpile** with edible items (closest walkable)
2. **Food items on ground** (closest walkable)
3. **Berry bushes** (closest walkable CELL_BUSH with berries)
4. **Nothing found** — mover wanders, keeps looking

### Berry Bushes

CELL_BUSH already exists as walkable vegetation. Extend it:

- **Berry state**: 0 (bare), 1 (budding), 2 (ripe) — stored in cell flags or separate grid
- **Ripening**: Timer-based, affected by season (fast in summer, none in winter)
- **Harvest**: Mover walks to bush, brief harvest action (1s), gains ITEM_BERRIES
- **Regrowth**: Bush returns to bare after harvest, regrows on timer
- **Seasonal modulation**: Spring = slow ripening, Summer = fast, Autumn = moderate, Winter = no ripening

### New Items

| Item | Flags | Stack | Weight | Notes |
|------|-------|-------|--------|-------|
| ITEM_BERRIES | IF_EDIBLE, IF_STACKABLE, IF_SPOILS | 10 | 1 | Raw, can be eaten directly |
| ITEM_DRIED_BERRIES | IF_EDIBLE, IF_STACKABLE | 20 | 1 | From drying rack, don't spoil |

### Eating Behavior

- Mover walks to food source
- Brief eating action (2s)
- Hunger restored: raw berries +0.3, dried berries +0.25
- Item consumed (DeleteItem)

### Drying Rack Integration

The drying rack already exists as a passive workshop. Add recipe:
- **Input**: ITEM_BERRIES × 3
- **Output**: ITEM_DRIED_BERRIES × 2
- **Passive time**: Same as dried grass

This closes the "berries spoil" loop — preserving food for winter becomes a real task.

### Seasonal Pressure

With weather/seasons already implemented:
- **Summer**: Bushes produce frequently, food is easy
- **Autumn**: Slowing down, smart players stockpile dried berries
- **Winter**: No new berries. Colony lives off stockpile. This is the first real tension.
- **Spring**: Berries slowly return

---

## Implementation Phases

### Phase 1: Hunger Stat & Starvation (no food yet)
1. Add `float hunger` to Mover struct (init 1.0)
2. Add `FreetimeState freetimeState` enum to Mover
3. Add `int needTarget` and `float needProgress` to Mover
4. Drain hunger each tick in MoverUpdate
5. Add `MoverAvailableForWork()` check — false when in freetime
6. Bump save version, add migration
7. **Test**: Mover hunger drains over time, reaches 0

### Phase 2: Berry Bushes & Harvesting
1. Add berry state tracking (separate grid or cell flag extension)
2. Add berry ripening tick (season-modulated)
3. Add ITEM_BERRIES to item_defs
4. Add JOBTYPE_FORAGE or reuse gather pattern
5. Add foraging designation or auto-harvest behavior
6. **Test**: Bushes ripen, can be harvested, yield berries, regrow

### Phase 3: Eating Behavior
1. Implement FREETIME_SEEKING_FOOD state (fallback chain search)
2. Implement FREETIME_EATING state (walk to food, eat, restore hunger)
3. Hungry movers seek food between jobs
4. Starving movers cancel jobs and seek food
5. **Test**: Mover finds berries, eats, hunger restores, returns to work

### Phase 4: Drying & Preservation
1. Add ITEM_DRIED_BERRIES to item_defs
2. Add drying rack recipe: berries → dried berries
3. IF_SPOILS flag implementation (berries decay over time if not dried)
4. **Test**: Berries can be dried, dried berries don't spoil

### Phase 5: Seasonal Integration
1. Berry ripening rate modulated by current season
2. Winter: no ripening (existing bushes stay bare)
3. **Test**: Berries ripen faster in summer, not at all in winter

---

## Connections

- **Uses weather/seasons**: Berry growth rates, winter food pressure
- **Uses stockpiles**: Storing dried berries for winter
- **Uses drying rack**: Preserving berries (existing workshop, new recipe)
- **Uses CELL_BUSH**: Already exists, extend with berry state
- **Opens for F5 (Cooking)**: Raw vs cooked food quality difference
- **Opens for F6 (Farming)**: Foraging doesn't scale past 10 movers
- **Opens for F8 (Containers)**: Can't carry water to wash berries (future)

---

## Save Version Impact

- New fields on Mover: hunger, freetimeState, needTarget, needProgress
- New item types: ITEM_BERRIES, ITEM_DRIED_BERRIES
- New grid: berry state (or cell flag extension)
- Bump save version, legacy migration struct needed

## Test Expectations

- ~30-40 new test assertions
- Hunger drain, threshold detection
- Berry ripening, harvesting, regrowth
- Eating behavior, hunger restoration
- Spoilage timer, drying preservation
- Seasonal modulation of berry growth
- Freetime state machine transitions
- Starving interrupts job correctly
