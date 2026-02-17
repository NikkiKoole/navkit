# Feature 03: Doors & Shelter

> **Priority**: Tier 1 — Survival Loop
> **Why now**: Buildings are open mazes. Weather/temperature exist but have no gameplay impact on colonists.
> **Systems used**: Construction, weather, temperature, lighting, wind
> **Depends on**: Feature 2 (furniture makes rooms useful) — COMPLETE
> **Opens**: Clothing matters more (F7), room quality for moods (F10)

---

## What Changes

**Doors** are a new cell type: always walkable by movers but block fluids, light, and temperature exchange. **Enclosed rooms** are detected via flood-fill — a room bounded by walls, doors, floors, and ceilings becomes **sheltered**. Shelter protects from rain/snow, blocks wind effects, retains heat, and gives a rest bonus for beds.

This is the feature that makes *building* meaningful. You're not just placing walls — you're creating shelter from the storm.

---

## Design Decisions

### 1. Doors are always walkable (no open/close state)

**Decision**: Doors have NO open/close state. They are always passable by movers but always block fluids, light, and temperature.

**Rationale**: Adding per-cell door state would require:
- A new `doorStateGrid[z][y][x]` (512KB+ memory)
- Conditional checks in every fluid/light/pathfinding function
- Mover proximity detection for auto-open/close
- Visual state sync

This is massive complexity for cosmetic benefit. The gameplay value is shelter detection, not door animation. Movers walking through a "closed-looking" door is a standard colony sim convention (DF, RimWorld).

**Implementation**: `CELL_DOOR` gets flags: walkable (no `CF_BLOCKS_MOVEMENT`), blocks fluids (`CF_BLOCKS_FLUIDS`), solid for support (`CF_SOLID`). Single static cell type, no dynamic state.

**Revisit when**: Lock/unlock mechanics are needed (prison, security).

### 2. Doors are a cell type, not furniture

**Decision**: `CELL_DOOR` in the CellType enum, placed via `BUILD_DOOR` construction category.

**Rationale**: Doors fill a cell slot like walls. They need to interact with the cell grid for pathfinding, fluid blocking, sky exposure checks, temperature insulation, and lighting. Making them furniture would require special-casing every system that reads the cell grid.

**Consequence**: A cell can be either a door OR have furniture, not both (same as walls).

### 3. Shelter = "not exposed to sky" (no room detection in Phase 1)

**Decision**: Defer full room/region flood-fill detection. Instead, leverage the existing `IsExposedToSky()` function which already scans upward for solid cells/floors.

**Rationale**: `IsExposedToSky()` (weather.c:247-258) already does exactly what shelter needs — it traces upward from z+1 and returns false if any solid cell or floor exists above. Rain (`ApplyRainWetness`), snow, and wind drying already check this. A room with walls + ceiling (floor on z+1) is already sheltered from rain/snow without ANY code changes to the weather system.

The missing piece is **horizontal** shelter: wind chill and temperature insulation. Wind is a global vector (not per-cell propagation), so "wind blocked by doors" just means "mover is not exposed to sky." Temperature already uses insulation tiers per cell — doors just need an appropriate tier.

**Full room detection** (flood-fill, region IDs, enclosed flag) is deferred to a later phase or feature. The 80% gameplay value comes from roof + walls + doors blocking vertical weather, which already works.

**Revisit when**: Room quality system (F10) needs to know room boundaries, size, and contents.

### 4. Door placement: walkable cell with wall neighbors

**Decision**: Doors require the cell to be walkable AND have at least one wall neighbor (cardinal). This prevents placing doors in open fields.

**Rationale**: A door in the middle of a field makes no sense. Requiring a wall neighbor ensures doors are placed in wall gaps — the natural pattern. This is a soft constraint (1+ wall neighbor, not exactly 2) to support corner doors and wide doorways.

### 5. No save version bump needed for CELL_DOOR

**Decision**: Adding a new CellType enum value doesn't affect save format.

**Rationale**: The grid is saved as `CellType grid[z][y][x]` — it's an enum stored as int. New enum values are backward-compatible (old saves just won't have any CELL_DOOR cells). Construction recipes don't affect save format. The carpenter recipe addition is just a code change.

**Exception**: If we add a `shelterGrid` or `regionGrid`, that would need a save version bump. But per Decision 3, shelter is derived on-demand (not persisted).

---

## Codebase Analysis

### What already works for shelter (no changes needed)

| System | Why it works | Key function |
|--------|-------------|--------------|
| Rain wetness | Checks `IsExposedToSky()` — roof blocks rain | `ApplyRainWetness()` weather.c:284 |
| Snow accumulation | Checks `IsExposedToSky()` — roof blocks snow | `UpdateSnow()` |
| Wind drying | Checks `IsExposedToSky()` | groundwear.c:231 |
| Cloud shadows | Applied to surface — interior cells don't get shadows | rendering.c |

### What needs changes

| System | Current behavior | Change needed | Effort |
|--------|-----------------|---------------|--------|
| Cell types | No CELL_DOOR | Add enum + cellDefs entry | Small |
| `IsExposedToSky()` | Checks `cell != CELL_AIR` | Already blocks for CELL_DOOR (it's not CELL_AIR) — **works automatically** | None |
| Pathfinding | Doors don't exist | CELL_DOOR is walkable (no CF_BLOCKS_MOVEMENT) | Small |
| Fluid blocking | Checks `CellAllowsFluids()` | CELL_DOOR has CF_BLOCKS_FLUIDS — **works automatically** | None |
| Sky light | Blocks at solid/floor | Need to block at CELL_DOOR too | Small |
| Block light | Stops at `CellIsSolid()` | Need to also stop at CELL_DOOR | Small |
| Temperature | Insulation tiers per cell | Add door insulation tier (between air and wood) | Small |
| Construction | No BUILD_DOOR | Add category + recipe + placement handler | Medium |
| Rest bonus | Fixed rate per furniture | Check `IsExposedToSky()` for shelter bonus | Small |
| Rendering | No door sprite | Need door sprite + draw logic | Small |

### Key insight: `IsExposedToSky()` does the heavy lifting

```c
bool IsExposedToSky(int x, int y, int z) {
    for (int zz = z + 1; zz < gridDepth; zz++) {
        CellType cell = grid[zz][y][x];
        if (cell != CELL_AIR) return false;  // ANY non-air cell blocks
        if (HAS_FLOOR(x, y, zz)) return false;
    }
    return true;
}
```

Since `CELL_DOOR != CELL_AIR`, a door placed above a cell would block sky exposure. But doors are placed horizontally (in walls), not as ceilings. The roof is what blocks sky — typically a floor on z+1 or solid terrain. Doors contribute to **horizontal** enclosure, not vertical.

For weather protection, the roof is what matters. Doors matter for:
1. Temperature insulation (reducing heat transfer at doorways)
2. Visual/logical completeness (a "room" has doors)
3. Future room detection (doors as boundary markers)

---

## Implementation Phases

### Phase 1: Door Cell Type + Construction (~1 session)

**Goal**: Players can build doors. Doors are walkable, block fluids, block light.

**Files changed**:
- `src/world/grid.h` — Add `CELL_DOOR` to CellType enum
- `src/world/cell_defs.c` — Add cellDefs entry: no CF_BLOCKS_MOVEMENT, has CF_BLOCKS_FLUIDS, has CF_SOLID (so floor above works as ceiling)
- `src/world/construction.h` — Add `BUILD_DOOR` to BuildCategory, `CONSTRUCTION_DOOR` to recipe enum
- `src/world/construction.c` — Door recipe: 3 planks, 3s build time, material inherited from planks
- `src/world/designations.c`:
  - `CreateRecipeBlueprint()`: BUILD_DOOR precondition — walkable + at least 1 cardinal wall neighbor
  - `CompleteBlueprint()`: BUILD_DOOR handler — set `grid[z][y][x] = CELL_DOOR`, set wall material, invalidate paths
- `src/core/input_mode.h` — Add `ACTION_WORK_DOOR`
- `src/core/action_registry.c` — Registry entry (key 'd' under SUBMODE_BUILD)
- `src/core/input.c` — Execution handler + placement function
- `src/core/input_mode.c` — Bar item
- `src/simulation/lighting.c` — Block sky light and block light at CELL_DOOR cells (2 checks)
- `src/render/rendering.c` — Door sprite rendering

**Walkability detail**: CELL_DOOR must NOT have CF_BLOCKS_MOVEMENT (movers walk through). It SHOULD have CF_SOLID so that the cell above it has support (a floor can rest on a door like on a wall — this makes ceiling detection work). It has CF_BLOCKS_FLUIDS so water/smoke/steam don't pass through.

**`GetCellMoveCost` consideration**: Doors could have a small movement cost penalty (e.g., 11 instead of 10) to make movers slightly prefer open paths. Optional — start with baseline cost 10.

**Lighting changes** (lighting.c):
- Sky light column scan: treat CELL_DOOR like solid (blocks sky light propagation downward)
- Block light BFS: treat CELL_DOOR as blocking (light doesn't pass through closed doors)

**Tests** (~10 assertions):
- Door cell is walkable (mover can path through)
- Door blocks fluids (water doesn't spread through)
- Door blocks sky light
- Door blocks block light
- `IsExposedToSky()` returns false for cell with door above (already works since CELL_DOOR != CELL_AIR)
- Blueprint placement requires wall neighbor
- Blueprint placement rejected in open field (no wall neighbors)
- Completed blueprint places CELL_DOOR in grid

### Phase 2: Temperature Insulation (~0.5 session)

**Goal**: Doors act as thermal insulators. Rooms with hearths retain heat.

**Files changed**:
- `src/simulation/temperature.c` — Add door insulation tier lookup
- `src/simulation/temperature.h` — Add `INSULATION_TIER_DOOR` constant

**How temperature already works** (temperature.c):
- `GetInsulationTier(x, y, z)` returns tier based on cell type
- Tiers: AIR=100% transfer, WOOD=20%, STONE=5%
- Heat transfer between neighbors: `transfer = (tempDiff * transferRate) / 100`
- Higher insulation tier = less transfer

**Door insulation**: Add `INSULATION_TIER_DOOR` = 40 (between air at 100 and wood at 20). Doors insulate less than walls but still significantly reduce heat flow. This means a room with walls + doors retains hearth heat, but doorways leak some warmth.

**`GetInsulationTier` change**: Add `case CELL_DOOR: return INSULATION_TIER_DOOR;`

**Tests** (~6 assertions):
- Door cell has correct insulation tier
- Room with walls + door + hearth stays warmer than outside
- Doorway leaks some heat (temp at door < room center, > outside)

### Phase 3: Rest Bonus in Sheltered Cells (~0.5 session)

**Goal**: Beds under a roof give 1.25x energy recovery.

**Files changed**:
- `src/simulation/needs.c` — In FREETIME_RESTING, multiply recovery rate by 1.25 if mover cell is not exposed to sky
- `src/render/tooltips.c` — Show "Sheltered" indicator in furniture/mover tooltip

**Implementation**: In the FREETIME_RESTING case (needs.c:294), after computing `rate`:
```c
// Shelter bonus: roof overhead means better rest
int moverCellX = (int)(m->x / CELL_SIZE);
int moverCellY = (int)(m->y / CELL_SIZE);
int moverCellZ = (int)m->z;
if (!IsExposedToSky(moverCellX, moverCellY, moverCellZ)) {
    rate *= 1.25f;
}
```

**Tests** (~4 assertions):
- Mover resting under roof recovers faster than outside
- Mover resting in open air gets base rate
- Bonus stacks with furniture rate (bed in shelter = bed rate * 1.25)

### Phase 4 (Deferred): Full Room Detection

**Goal**: Flood-fill to detect enclosed rooms, assign region IDs, track room properties.

**Why deferred**: The 80% gameplay value (weather protection + thermal insulation + rest bonus) is achieved with phases 1-3 using `IsExposedToSky()`. Full room detection is needed for:
- Room quality scoring (F10 moods)
- "This room needs a door" warnings
- Per-room temperature averaging
- Room size limits for insulation

**Sketch**: New `src/world/rooms.c` with flood-fill BFS. `regionGrid[z][y][x]` stores region ID. Walls/doors are boundaries. Recompute when terrain changes (similar to `RecomputeLighting()`). Max 100 cells per room.

---

## Carpenter Recipe Addition

The door recipe goes on the existing carpenter's bench (WORKSHOP_CARPENTER from F2):

```c
{ "Craft Door", ITEM_PLANKS, 3, ITEM_NONE, 0, ITEM_DOOR, 1, ITEM_NONE, 0, 5.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
```

**Wait** — this means we need `ITEM_DOOR` as a new item type. The door is crafted as an item, then placed via construction (like ITEM_PLANK_BED → BUILD_FURNITURE pattern from F2).

**Alternative**: Door construction directly consumes planks (no intermediate item). Like wall/floor construction — haul 3 planks to blueprint site, build door.

**Recommendation**: Direct construction from planks (no ITEM_DOOR). Simpler. Matches wall construction pattern. The carpenter recipe is unnecessary — planks → door at the build site, just like planks → plank wall.

**Decision**: No ITEM_DOOR, no carpenter recipe. Door is a **direct construction recipe**: 3 planks, 3s build time, BUILD_DOOR category. Same as plank wall/floor. This avoids a save version bump (no new item type).

---

## Door Removal

Doors should be removable. Follow the existing wall removal pattern:
- Designation: ACTION_WORK_REMOVE_DOOR (or reuse ACTION_WORK_REMOVE_WALL if the logic generalizes)
- Job: walk to door, remove it (2s), drop planks
- Cell reverts to CELL_AIR

**Simplest approach**: Check if existing "remove wall" logic can handle CELL_DOOR. If `ExecuteDesignateRemoveWall` checks for `IsWallCell()`, we'd need to extend it or add a separate action.

---

## Rendering

Door sprite: use a simple vertical bar sprite (like a gate). Tinted by material (oak door = wood color). Rendered at full cell height.

For Phase 1, a placeholder sprite (reuse SPRITE_wall or similar with distinct tint) is fine.

---

## Summary: What's Actually Needed

| Phase | Files | New lines (est.) | Save version? |
|-------|-------|-------------------|---------------|
| Phase 1: Door cell + construction | grid.h, cell_defs.c, construction.h/.c, designations.c, input*.c/.h, action_registry.c, lighting.c, rendering.c | ~150 | No |
| Phase 2: Temperature insulation | temperature.c/.h | ~15 | No |
| Phase 3: Rest bonus | needs.c, tooltips.c | ~15 | No |
| **Total** | ~12 files | ~180 lines | **No** |

## Test Expectations

- Phase 1: ~10 assertions (walkability, fluid blocking, light blocking, construction)
- Phase 2: ~6 assertions (insulation tier, room heating)
- Phase 3: ~4 assertions (rest bonus under roof)
- **Total**: ~20 new assertions across 3 phases

## Connections

- **Uses construction**: Doors built like walls (direct plank consumption)
- **Uses weather**: Rain/snow already respect `IsExposedToSky()` — roof is the key, doors complete the enclosure
- **Uses temperature**: Existing insulation tier system — doors get their own tier
- **Uses lighting**: Sky light + block light already propagate through BFS — doors block both
- **Uses F2 (furniture)**: Rest bonus for beds in sheltered cells
- **Opens for F7 (Clothing)**: Shelter vs clothing as two ways to handle cold
- **Opens for F10 (Moods)**: Room detection + quality scoring (deferred Phase 4)
