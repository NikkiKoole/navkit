# core/

Save/load, input handling, time, inspector, simulation manager.

## Save System

**Save version**: v52 (see `CURRENT_SAVE_VERSION` in `save_migrations.h`). Strict version matching during dev — old saves error out cleanly.

### Grid Write Order (saveload.c)

Grids are written/read in this exact order. **inspect.c must match this order exactly** or data silently corrupts:

```
grid, waterGrid, fireGrid, smokeGrid, steamGrid, cellFlags,
wallMaterial, floorMaterial, wallNatural, floorNatural,
wallFinish, floorFinish, wallSourceItem, floorSourceItem,
vegetationGrid, snowGrid, temperatureGrid, designations,
wearGrid, growthTimer, targetHeight, treeHarvestState, floorDirtGrid
```

### Section Markers

Save files use hex markers for readability: `MARKER_GRIDS` (0x47524944), `MARKER_ENTITIES` (0x454E5449), `MARKER_VIEW`, `MARKER_SETTINGS`, `MARKER_END`.

### Adding a New Grid

1. Add grid declaration in the appropriate header
2. Add `fwrite` in `SaveWorld()` — after the last grid, before `MARKER_ENTITIES`
3. Add matching `fread` in `LoadWorld()` — same position
4. Add matching read in `inspect.c` — same position
5. Bump `CURRENT_SAVE_VERSION`
6. Add migration if old saves need default values

### Adding New Saved Fields

1. Bump `CURRENT_SAVE_VERSION` in `save_migrations.h`
2. If struct changed (Mover, Item, Stockpile): add `StructVxx` typedef in `save_migrations.h`
3. In `LoadWorld()`: `if (version >= xx) { fread new struct } else { fread old struct, migrate }`
4. Mirror in `inspect.c`

### Settings X-Macro

`SETTINGS_TABLE(X)` in saveload.c auto-generates save/load for all tweakable float/int vars. To add a new setting: add one line to the table.

## Input System

- Action registry: hierarchical mode/submode/parent system in `ACTION_REGISTRY[]`
- Draw mode materials: keys 1-9 mapped via `GetDrawMaterial()` in `input.c`
- Recipe cycling: M key per category, tracked via `selectedWallRecipe` / `selectedFloorRecipe` / `selectedLadderRecipe`

## Time System

- `gameTime`: real seconds elapsed
- `timeOfDay`: 0.0-24.0 (wraps)
- `dayNumber`: integer day count
- `currentTick`: 60fps fixed timestep counter
- Fixed timestep uses accumulator pattern — don't call `Tick()` from render loop

## Gotchas

- **Grid read/write order mismatch** between saveload.c and inspect.c causes silent data corruption
- **Enum shifts break saves**: adding MaterialType/ItemType shifts array sizes in Stockpile — needs migration struct
- **Missing save version bump**: if you change any saved struct without bumping version, old and new saves silently load wrong data
