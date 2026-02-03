# Vertical Slice: Crafting System - COMPLETE

The minimal playable scenario is implemented and working.

---

## Status: ALL CRITERIA MET

### Success Criteria

- [x] Mining produces ORANGE (already worked)
- [x] Haulers move ORANGE to input stockpile
- [x] Crafter walks to stockpile, picks up ORANGE
- [x] Crafter carries ORANGE to workshop
- [x] Crafter works, ORANGE consumed, STONE_BLOCKS spawn
- [x] Haulers move STONE_BLOCKS to stockpiles
- [x] Builder uses STONE_BLOCKS to build wall
- [x] Loop runs multiple times without breaking
- [x] No reservation leaks (items stuck reserved, workshop stuck busy)

### Debugging Aids

- [x] Show workshop footprint on map (template-based tinted tiles)
- [x] Show work tile / output tile markers (green/blue tints)
- [x] Show crafter carrying item (already existed)
- [x] Show craft progress bar at workshop
- [x] Workshop hover: show bill status, assigned crafter
- [x] Item hover: show reservedBy
- [x] Mover hover: show current job + step

---

## The Working Loop

```
Mine wall -> ORANGE spawns -> Hauler takes to stockpile
                                        |
              Crafter picks up from stockpile -> Brings to workshop -> Crafts
                                                                          |
                                              STONE_BLOCKS spawn -> Hauler takes to stockpile
                                                                          |
                                                    Builder hauls to blueprint -> Builds wall
```

---

## What Was Implemented

### New Item Type
```c
ITEM_STONE_BLOCKS  // Added to ItemType enum
```

### Workshop System
- Workshop struct + array (MAX_WORKSHOPS = 256)
- WorkshopType enum (WORKSHOP_STONECUTTER)
- Recipe struct + stonecutter recipes
- Bill struct with 3 modes (DO_X_TIMES, DO_UNTIL_X, DO_FOREVER)
- Workshop ASCII templates for layout

### Craft Job
- JOBTYPE_CRAFT
- CraftJobStep enum (FETCH, PICKUP, MOVING, WORKING)
- RunJob_Craft() driver
- WorkGiver_Craft for job assignment

### Construction
- Wall blueprint requires ITEM_STONE_BLOCKS

### UI (Beyond Original Scope)
- T key to place workshop in draw mode
- B/X/P/D keys for bill management and workshop delete
- Workshop tooltip with status
- 3x3 preview overlay when placing

### Infrastructure
- Save/load support (version 8)
- Inspect support (--workshop N)
- CELL_FLAG_WORKSHOP_BLOCK for O(1) pathfinding

---

## Test Setup

The system can be tested with any map:

1. Have some ORANGE items (from mining walls)
2. Press D for draw mode, then T for workshop
3. Click to place a 3x3 stonecutter workshop
4. Hover workshop and press B to add a "Do Forever" bill
5. Watch crafters fetch ORANGE and produce STONE_BLOCKS

No special test terrain generator was needed - the system works with existing maps.

---

## Original Simplifications (Now Resolved)

| Simplification | Status |
|----------------|--------|
| No workshop placement UI | DONE - T key in draw mode |
| No bill UI | DONE - B/X/P keys |
| Hardcoded BILL_DO_FOREVER | DONE - all 3 modes work |
| No linked stockpiles | Still not implemented (future) |
| No multiple recipes | Still single recipe (future) |
| No workshop construction | Still instant placement (future) |

---

## Files Changed

| File | Changes |
|------|---------|
| src/entities/items.h | Added ITEM_STONE_BLOCKS |
| src/entities/workshops.h | New file - Workshop, Recipe, Bill structs |
| src/entities/workshops.c | New file - workshop logic, templates |
| src/entities/jobs.h | Added JOBTYPE_CRAFT, CraftJobStep |
| src/entities/jobs.c | Craft job driver, WorkGiver_Craft |
| src/core/input.c | Workshop placement, bill controls |
| src/core/input_mode.h | Added ACTION_DRAW_WORKSHOP |
| src/core/input_mode.c | T key binding |
| src/render/rendering.c | DrawWorkshops |
| src/render/tooltips.c | Workshop tooltip |
| src/world/grid.h | CELL_FLAG_WORKSHOP_BLOCK |
| src/world/cell_defs.h | Pathfinding check for blocked tiles |
| src/core/saveload.c | Workshop save/load |
| src/core/inspect.c | Workshop inspection |
