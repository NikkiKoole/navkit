# 06c: Farm Watering

> Extracted from 06a on 2026-02-24. Optional enhancement — farms work with rain alone (06a/06b).
> **Deps**: 06a (farm grid, tilled cells exist), wetness system (mud)
> **Opens**: 08 (thirst — water collection reused for drinking)

---

## Goal

Add manual water collection and carrying to farms. Farms already work with rain from the weather system, but sandy soil in dry seasons needs manual watering. This also lays groundwork for 08 (thirst — movers drinking water).

---

## New Items

| Item | Flags | Stack | Notes |
|------|-------|-------|-------|
| ITEM_WATER | IF_STACKABLE | 5 | Abstraction for "a pot of water." No container tracking — placeholder until 08 adds proper liquid-in-container support. |

**Design note**: ITEM_WATER is a simplification. It represents water carried in a pot without tracking the pot itself. When 08 (thirst) arrives and containers get liquid support, this will need revisiting.

---

## New Jobs

### JOBTYPE_FETCH_WATER

Mover collects water from a designated water source.

| Property | Value |
|----------|-------|
| Work time | 0.5 game-hours |
| Designation | DESIGNATION_WATER_SOURCE (placed on cell adjacent to CELL_WATER) |
| Result | Spawns ITEM_WATER at cell |
| Repeatable | Yes (designation persists, like berry harvesting) |

Mark a spot by the river, movers periodically collect water there.

### JOBTYPE_WATER_CROP

Mover carries water to a dry farm cell.

| Property | Value |
|----------|-------|
| Work time | 0.2 game-hours (pouring) |
| Requires | ITEM_WATER |
| Auto-assigned | Yes, when tilled farm cell wetness < FARM_DRY_THRESHOLD and ITEM_WATER available |
| Result | Cell wetness increased by WATER_POUR_AMOUNT, water item consumed |

**WorkGiver_Water**: scan tilled farm cells → find dry cells (wetness < threshold) → find available ITEM_WATER → assign mover.

**Job steps:**
1. Walk to water item, pick up
2. Walk to dry farm cell
3. Work (pour water, 0.2 game-hours)
4. On completion: increase cell wetness, delete water item

---

## Designation

New designation: `DESIGNATION_WATER_SOURCE` — placed on walkable cell adjacent to CELL_WATER.

- Mode: W → F → W (work → farm → water source) or similar
- Single-cell placement (not drag)
- Validates adjacency to CELL_WATER

---

## Sprites

| Item | Sprite | Tint |
|------|--------|------|
| ITEM_WATER | `SPRITE_division` (reuse) | Blue {50, 120, 200} |

---

## When to Build This

Build 06c when:
- Sandy soil farms are starving for water in dry seasons
- 08 (thirst) is approaching and water collection is needed anyway
- Players are asking for more farm control

Not needed for the core farming loop — rain handles normal cases.

---

## E2E Test Stories

### Story 1: Water collection
- Place CELL_WATER, designate adjacent cell as water source
- Spawn mover
- Tick until fetch water job completes
- **Verify**: ITEM_WATER spawned at designation cell

### Story 2: Water dry farm
- Till farm cell, set wetness to 0
- Spawn ITEM_WATER nearby, spawn mover
- Tick until water crop job completes
- **Verify**: farm cell wetness increased, ITEM_WATER consumed

### Story 3: Auto-assignment only when dry
- Till farm cell with good wetness (60)
- Spawn ITEM_WATER nearby, spawn mover
- **Verify**: no watering job assigned (wetness above threshold)
