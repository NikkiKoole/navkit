# Primitive Shelter & Doors — COMPLETE

> Implemented 2026-02-22. Save version 67.

---

## What Was Built

### CELL_DOOR (new cell type)
- Added `CELL_DOOR` to CellType enum (value 17)
- Flags: `CF_BLOCKS_FLUIDS | CF_SOLID` (blocks fluids/light, provides support above, but walkable)
- Insulation: WOOD tier (20%)
- Walkability: explicit early return in `IsCellWalkableAt` before `CellIsSolid` check
- Rendering: uses `SPRITE_door`, bypasses material sprite cascade (added to ramp early-return in `GetSpriteForCellMat`), tinted by wall material
- Placement constraint: requires at least 1 cardinal wall/door neighbor

### BUILD_DOOR category
- New `BuildCategory` enum value
- R-key cycling between door recipes
- Action: W → B → D (work → build → door)
- L-drag to place, R-drag to erase

### Construction Recipes Added

| Recipe | Materials | Category | Build Time |
|--------|-----------|----------|------------|
| Leaf Wall | 4 sticks + 4 leaves | BUILD_WALL | 2s |
| Stick Wall | 4 sticks + 2 cordage | BUILD_WALL | 2s |
| Leaf Roof | 2 poles + 3 leaves | BUILD_FLOOR | 2s |
| Bark Roof | 2 poles + 2 bark | BUILD_FLOOR | 2s |
| Leaf Door | 2 poles + 2 leaves | BUILD_DOOR | 2s |
| Plank Door | 3 planks | BUILD_DOOR | 3s |

### Ground Fire Workshop
- New `WORKSHOP_GROUND_FIRE` (1x1, template "F", passive)
- Construction cost: 3 sticks, 1s
- Shares campfire recipes (burns sticks/logs)
- Action: W → B → W → G (workshop → ground fire)

### Fire Pit (renamed campfire)
- `WORKSHOP_CAMPFIRE` renamed to "Fire Pit" / "FIRE_PIT"
- Updated construction cost: 5 sticks + 3 rocks, 2s

### Save Version
- Bumped to 67
- `V66_CELL_TYPE_COUNT = 17` defined for future migration if needed
- No migration code needed (CELL_DOOR never exists in old saves)

### Tests
- `tests/test_doors.c`: 30 tests, 59 assertions
- Covers: cell properties, walkability, sky exposure, recipe validation, workshop defs, blueprint placement

---

## Files Changed
- `src/world/grid.h` — CELL_DOOR enum + name
- `src/world/cell_defs.c` — door cell definition
- `src/world/cell_defs.h` — walkability early return for doors
- `src/world/construction.h` — BUILD_DOOR, 7 new recipe enums
- `src/world/construction.c` — 7 recipe definitions, fire pit cost update
- `src/world/designations.c` — door blueprint precondition + completion, ground fire workshop
- `src/world/material.c` — door sprite cascade bypass
- `src/render/rendering.c` — door material tint (2 locations)
- `src/entities/workshops.h` — WORKSHOP_GROUND_FIRE
- `src/entities/workshops.c` — ground fire def, campfire rename
- `src/core/input_mode.h` — ACTION_WORK_DOOR, ground fire actions
- `src/core/input_mode.c` — door recipe display
- `src/core/action_registry.c` — door + ground fire action entries
- `src/core/input.c` — door/ground fire execution handlers
- `src/core/save_migrations.h` — version 67
- `tests/test_doors.c` — new test file
- `Makefile` — test_doors target

---

## Deferred
- Ground fire spread mechanic → `docs/todo/environmental/fire-improvements.md`
- Fire pit tool requirement (QUALITY_DIGGING:1) → `docs/todo/environmental/fire-improvements.md`
- New insulation tier (INSULATION_TIER_LEAF) — decided to keep 3 tiers, not worth the complexity
