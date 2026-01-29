# Vertical Slice: Crafting System

Minimal playable scenario to prove the crafting system works end-to-end.

---

## Goal

Prove this loop runs reliably:

```
Mine wall → ORANGE spawns → Hauler stocks near workshop → Crafter fetches → Crafts → STONE_BLOCKS spawn → Hauler stocks → Builder uses for wall
```

If this works, we've validated: job discovery, reservations, DF-style crafter fetch, workshop interaction, material transformation, and construction material requirements.

---

## Test Scenario Setup

### Map
- 30x30 tiles
- Flat terrain (z=1 walkable, z=0 dirt)
- 6-8 minable walls in a cluster (enough for several craft cycles)

### Pre-placed Structures

**Stonecutter Workshop (3x3)**
- Position: (15, 15)
- Work tile: (16, 15)
- Output tile: (17, 15)
- Bill: "Cut Stone Blocks" - BILL_DO_FOREVER

**Input Stockpile (2x2)**
- Position: (12, 15) - adjacent to workshop
- Accepts: ITEM_ORANGE only
- Priority: high

**Output Stockpile (2x2)**
- Position: (18, 15) - other side of workshop
- Accepts: ITEM_STONE_BLOCKS only
- Priority: high

**Construction Stockpile (2x2)**
- Position: (10, 10) - near build site
- Accepts: ITEM_STONE_BLOCKS only
- Priority: normal

**Wall Blueprint**
- Position: (8, 10)
- Requires: 2 ITEM_STONE_BLOCKS

### Movers
- 4-5 movers, all with all capabilities (no professions yet)
- Starting positions spread around map

---

## Expected Flow

### Phase A: Mining
1. Player designates walls to mine (M+drag) - or pre-designated
2. Mover picks up mining job
3. Mover walks to wall, mines it
4. ITEM_ORANGE spawns on ground
5. Mover becomes idle

### Phase B: Input Hauling
1. Hauler sees ITEM_ORANGE on ground
2. Hauler picks it up, delivers to Input Stockpile (nearest accepting stockpile)
3. ITEM_ORANGE now in stockpile near workshop

### Phase C: Crafting (DF-style)
1. Crafter sees workshop has runnable bill + materials available
2. Crafter reserves ITEM_ORANGE + workshop
3. Crafter walks to Input Stockpile (where ORANGE is)
4. Crafter picks up ORANGE (now carrying)
5. Crafter walks to workshop work tile
6. Crafter works for 3 seconds
7. ORANGE consumed, 2x ITEM_STONE_BLOCKS spawn at output tile
8. Workshop + item reservations released
9. Crafter becomes idle (or starts next craft if more ORANGE available)

### Phase D: Output Hauling
1. Hauler sees STONE_BLOCKS on ground (at workshop output)
2. Hauler picks up, delivers to Output Stockpile
3. Later: another hauler moves to Construction Stockpile (if priority higher, or if needed for blueprint)

### Phase E: Construction
1. Builder sees blueprint needs STONE_BLOCKS
2. Builder hauls STONE_BLOCKS from stockpile to blueprint
3. Builder builds wall
4. Blueprint becomes solid wall

### Loop Continues
- More walls mined → more ORANGE → more crafting → more STONE_BLOCKS → more building possible

---

## What We're Testing

| System | What to verify |
|--------|----------------|
| Mining | ORANGE spawns when wall mined |
| Hauling | ORANGE reaches input stockpile |
| Workshop | Bill runs when materials present |
| Craft job | Crafter walks to item, picks up, walks to workshop, works |
| Material consumption | ORANGE disappears, STONE_BLOCKS appear |
| Output hauling | STONE_BLOCKS reach stockpile |
| Construction | Builder uses STONE_BLOCKS (not ORANGE) |
| Reservations | No double-pickup, no double-craft |

---

## New Code Required

### New Item Type
```c
ITEM_STONE_BLOCKS  // Add to ItemType enum
```

### Workshop System
- `Workshop` struct + array
- `WorkshopType` enum (just STONECUTTER for now)
- `Recipe` struct + stonecutter recipes
- `Bill` struct (just BILL_DO_FOREVER for now)

### Craft Job
- `JOBTYPE_CRAFT` 
- `CraftJobStep` enum
- `RunJob_Craft()` driver
- Job assignment in AssignJobs (or WorkGiver_Craft)

### Construction Change
- Wall blueprint requires ITEM_STONE_BLOCKS instead of ITEM_ORANGE

### Test Scenario
- Hardcoded setup function that creates the test map
- Or: terrain generator option that sets up this scenario

---

## Success Criteria

The slice is done when:

- [ ] Mining produces ORANGE (already works)
- [ ] Haulers move ORANGE to input stockpile
- [ ] Crafter walks to stockpile, picks up ORANGE
- [ ] Crafter carries ORANGE to workshop
- [ ] Crafter works, ORANGE consumed, STONE_BLOCKS spawn
- [ ] Haulers move STONE_BLOCKS to stockpiles
- [ ] Builder uses STONE_BLOCKS to build wall
- [ ] Loop runs multiple times without breaking
- [ ] No reservation leaks (items stuck reserved, workshop stuck busy)

---

## Debugging Aids

### Visual
- [ ] Show workshop footprint on map
- [ ] Show work tile / output tile markers
- [ ] Show crafter carrying item (already exists?)
- [ ] Show craft progress bar at workshop

### Tooltip / Debug Info
- [ ] Workshop hover: show bill status, assigned crafter, materials nearby
- [ ] Item hover: show reservedBy
- [ ] Mover hover: show current job + step

---

## Simplifications for Slice

Things we're NOT doing yet:

- No workshop placement UI (hardcoded position)
- No bill UI (hardcoded BILL_DO_FOREVER)
- No linked stockpiles (use radius search)
- No multiple recipes per workshop
- No BILL_DO_UNTIL_X or BILL_DO_X_TIMES
- No crafter skill/speed modifiers
- No workshop construction (workshop just exists)

---

## Implementation Order

1. **Add ITEM_STONE_BLOCKS** - new item type, stockpile filter support
2. **Add Workshop struct** - basic struct, hardcoded stonecutter
3. **Add Recipe + Bill** - minimal, just one recipe, DO_FOREVER
4. **Add JOBTYPE_CRAFT** - enum value, job struct fields
5. **Add craft job driver** - the 4-step state machine
6. **Add craft job assignment** - find workshop + bill + item, reserve
7. **Change wall blueprint** - require STONE_BLOCKS
8. **Create test scenario** - hardcoded map setup
9. **Test + debug** - run the loop, fix issues

---

## Decisions Made

**Where does test scenario setup live?**
→ New terrain generator option. Add a crafting test scenario to the terrain generators.

**How to spawn the workshop?**
→ Hardcoded first (in test scenario), then add to build menu. Use generic tile for workshop parts initially.

**Pre-designate mining area, or let player do it?**
→ Pre-designate in test scenario (for automated testing). Player designates in normal play.

**How many movers? All same capabilities?**
→ 10 identical movers, all with same capabilities.
