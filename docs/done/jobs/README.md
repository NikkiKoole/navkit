# Crafting System - COMPLETE

The DF-style crafting system has been implemented and is working.

---

## Status: DONE

The vertical slice is complete. The crafting loop works end-to-end:

```
Mine wall -> ITEM_ORANGE -> Hauler stocks near workshop
                                    |
                    Crafter fetches -> Crafts -> ITEM_STONE_BLOCKS
                                                      |
                              Hauler stocks -> Builder uses for walls
```

---

## What Was Built

### Core System (Original Scope)
- ITEM_STONE_BLOCKS item type
- Workshop struct with Stonecutter type
- Recipe + Bill system (all 3 modes: DO_X_TIMES, DO_UNTIL_X, DO_FOREVER)
- JOBTYPE_CRAFT with 4-step state machine
- WorkGiver_Craft for job assignment
- Wall blueprint requires STONE_BLOCKS

### Beyond Original Scope
- Workshop placement UI (T key in draw mode, 3x3 preview)
- Bill management (B=add, X=remove, P=pause)
- Workshop delete (D key)
- Workshop ASCII template system (##O / #X. / ...)
- Tinted tile rendering based on template type
- Blocked tiles prevent pathfinding (O(1) via CELL_FLAG_WORKSHOP_BLOCK)
- Movers pushed out when workshop placed on them
- Workshop tooltip with bill status
- Save/load support (version 8)
- Inspect support (--workshop N)

---

## Key Files

| File | What it does |
|------|--------------|
| `src/entities/workshops.h` | Workshop, Recipe, Bill structs, template types |
| `src/entities/workshops.c` | Workshop creation, deletion, bill management, templates |
| `src/entities/jobs.c` | JOBTYPE_CRAFT driver, WorkGiver_Craft |
| `src/core/input.c` | Workshop placement, bill controls (B/X/P/D keys) |
| `src/render/rendering.c` | DrawWorkshops with template-based tinting |
| `src/render/tooltips.c` | Workshop tooltip |
| `src/world/grid.h` | CELL_FLAG_WORKSHOP_BLOCK for pathfinding |
| `src/core/saveload.c` | Workshop save/load |
| `src/core/inspect.c` | Workshop inspection |

---

## How It Works

### Workshop Templates
Workshops use ASCII templates to define their layout:
```
##O   # = machinery (blocked, dark brown)
#X.   X = work tile (green tint)
...   O = output tile (blue tint)
      . = walkable floor (light brown)
```

### Job Flow
1. WorkGiver_Craft finds workshop with runnable bill + available materials
2. Reserves item + workshop
3. CRAFT_STEP_FETCH: Walk to item location
4. CRAFT_STEP_PICKUP: Pick up item
5. CRAFT_STEP_MOVING: Carry to workshop work tile
6. CRAFT_STEP_WORKING: Work until complete
7. Consume input, spawn output at output tile

### Pathfinding Integration
- Blocked tiles (#) set CELL_FLAG_WORKSHOP_BLOCK in cellFlags grid
- IsCellWalkableAt_Standard() checks this flag (O(1) lookup)
- Movers path around workshop machinery

---

## Future Work (Not Started)

From materials-and-workshops.md:
- Phase 2: Food chain (farms, cooking)
- Phase 3: Wood industry (forestry, sawmill)
- Multiple workshop types
- Linked stockpiles (workshop-specific input sources)
- Workshop construction (blueprint + materials to build)
- Crafter skill/speed modifiers
- Quality system

---

## Files in This Folder

| File | Purpose |
|------|---------|
| README.md | This file - status overview |
| vertical-slice.md | Original test scenario spec (now complete) |
| crafting-plan.md | Technical spec reference |
| materials-and-workshops.md | Future phases roadmap |
