# 04c: Root Foraging

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Deps**: 03 (fire pit/ground fire for roasting)
> **Independent**: Can be done before or after 04a/04b

---

## Goal

Add roots as a food source dug from soil. Roots fill the gap between "I can only forage berries" and "I can hunt and cook meat." They make the digging stick and fire feel essential — raw roots are barely worth eating, but roasted roots are decent food.

---

## New Items

| Item | Flags | Stack | Nutrition | Notes |
|------|-------|-------|-----------|-------|
| ITEM_ROOT | IF_EDIBLE, IF_STACKABLE | 20 | +0.1 | Barely nutritious raw |
| ITEM_ROASTED_ROOT | IF_EDIBLE, IF_STACKABLE | 20 | +0.35 | Roasted on fire, decent food |
| ITEM_DRIED_ROOT | IF_EDIBLE, IF_STACKABLE | 20 | +0.2 | Preserved, winter food |

3 new item types. Save version bump required.

---

## Digging Up Roots

New designation: **DESIGNATION_DIG_ROOTS**

**Where**: Natural soil cells — MAT_DIRT, MAT_CLAY, MAT_PEAT (not sand/gravel — too dry/rocky)

**How**:
- Player designates soil cells (W → D → R or similar, new action)
- WorkGiver assigns movers to dig
- Job: walk to cell → dig action → spawn ITEM_ROOT x1-2
- Soil cell is NOT consumed (roots regrow)
- Peat yields 2 roots, dirt/clay yield 1

**Tool scaling**:
- Bare-handed: very slow (0.5x speed)
- Digging stick (QUALITY_DIGGING:1): normal speed
- Stone pick (QUALITY_DIGGING:2): fast

**Cooldown**: After digging, cell can't be dug again for some time (prevents infinite root farming). Track with a per-cell timer or a simple "depleted" flag that resets after N game-hours.

---

## Recipes

**Roasting** (fire pit + ground fire, passive):
- 1x ROOT + 1x STICKS (skewer) → 1x ROASTED_ROOT
- Active work: 1s, Passive time: 10s
- Stick is consumed (burns as skewer)

**Drying** (drying rack, passive):
- 2x ROOT → 1x DRIED_ROOT
- Pure passive, same pattern as dried berries/grass

---

## Why This Matters

- **Berries are seasonal** and limited (bush regrowth time)
- **Roots are year-round** in soil (except frozen ground)
- Raw roots are barely worth eating (+0.1) — cooking triples the value (+0.35)
- This makes fire useful for food BEFORE hunting exists
- Digging stick's second purpose (after fire pit): food production
- Soil type matters for foraging: peat is best, dirt is fine, sand/gravel have none

---

## Food Progression (with roots)

```
Day 1:  Eat raw berries (+0.3) — easy, no tools
Day 1:  Dig roots bare-handed, eat raw (+0.1) — desperate
Day 1:  Build ground fire, roast roots (+0.35) — first cooking!
Day 2:  Craft digging stick — dig roots faster
Day 2:  Dry roots at drying rack — winter stockpile
Day 3+: Hunt animals — cooked meat (+0.5) replaces roots
```

---

## Implementation Phases

### Phase 1: Items
- Add 3 item types
- Define flags, nutrition, stack sizes
- Stockpile filter keys
- Save version bump

### Phase 2: Dig Roots Designation + Job
- Add DESIGNATION_DIG_ROOTS
- Add JOBTYPE_DIG_ROOTS (or reuse JOBTYPE_MINE variant)
- Soil type check (dirt/clay/peat only)
- Tool speed scaling via digging quality
- Root spawn on completion
- Cell cooldown tracking

### Phase 3: Recipes
- Add roasting recipe to fire pit/ground fire recipes
- Add drying recipe to drying rack recipes
- **Test**: Root + stick → roasted root at fire pit

---

## Design Notes

- **No new workshop needed** — uses existing fire pit + drying rack
- **Cell cooldown** keeps roots balanced — can't farm one spot infinitely
- **Frozen ground** (future): roots can't be dug in winter if ground freezes (ties into seasons)
- **Stick consumption** in roasting recipe is intentional — gives sticks a food-related use
