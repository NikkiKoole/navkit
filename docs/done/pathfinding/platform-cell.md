# CELL_PLATFORM — Drawable & Renderable Platform Cell

> Status: spec

First step toward train stations (transport Layer 1+2). Platforms are walkable cells placed next to track. Once this is in, build a save with tracks + platforms to test transport logic against.

---

## Changes

### 1. Add `CELL_PLATFORM` to CellType enum
**File:** `src/world/grid.h` (~line 49, before CELL_TYPE_COUNT)
- Add `CELL_PLATFORM` with comment
- Update `CellTypeName()` names array to include "PLATFORM"

### 2. Add cellDef entry
**File:** `src/world/cell_defs.c` (~line 36)
- Add `[CELL_PLATFORM]` entry: sprite = `SPRITE_dark_shade`, flags = `0` (walkable like track/air), insulation = AIR, no fuel, burns into AIR, drops nothing
- Platform is walkable, not solid, not blocking — just a visual marker next to track

### 3. Add `ACTION_DRAW_PLATFORM`
**File:** `src/core/input_mode.h` (~line 62)
- Add `ACTION_DRAW_PLATFORM` to the action enum, near the other transport actions

### 4. Register platform in action registry
**File:** `src/core/action_registry.c` (~line 491, after DRAW_TRAIN)
- Add entry with `parentAction = ACTION_DRAW_TRANSPORT`, key `'p'`, barDisplayText `"Platform"`
- canDrag = true, canErase = true (same as track)
- Update parent `ACTION_DRAW_TRANSPORT` barText to include `[P]latform`

### 5. Add place/remove functions in input.c
**File:** `src/core/input.c`
- Add `TryPlacePlatformAt()` — same pattern as `TryPlaceTrackAt()`: place on walkable CELL_AIR cells
- Add `ExecutePlacePlatform()` — fill mode (not outline like track), drag to place rectangle
- Add `ExecuteRemovePlatform()` — remove CELL_PLATFORM → CELL_AIR
- Add `case ACTION_DRAW_PLATFORM:` in the main dispatch (~line 3205)

### 6. Render platform with dark shade sprite
**File:** `src/render/rendering.c`
- In `GetWallSpriteAt()` (or equivalent): if cell == CELL_PLATFORM, return `SPRITE_dark_shade`
- No autotile needed for now — just a flat dark rectangle per cell

### 7. Save/load compatibility
- CELL_PLATFORM is just a new enum value in the grid. Old saves won't have any CELL_PLATFORM cells.
- **Bump save version** in `save_migrations.h` (v83 → v84) since CELL_TYPE_COUNT changes

---

## Files touched
- `src/world/grid.h` — enum + name
- `src/world/cell_defs.c` — cellDef entry
- `src/core/input_mode.h` — action enum
- `src/core/action_registry.c` — registry entry + parent barText
- `src/core/input.c` — place/remove/dispatch
- `src/render/rendering.c` — sprite return for platform
- `src/core/save_migrations.h` — version bump

## Verification
1. `make path` — builds clean
2. Run game, Draw menu → traiN → P for Platform
3. Drag to place platforms — dark shade squares appear on walkable ground
4. Right-drag to remove them
5. Place next to track — visually looks like a platform beside rails
6. F5 save, F6 load — platforms persist
