# Feature 03: Doors & Shelter

> **Priority**: Tier 1 — Survival Loop
> **Why now**: Buildings are open mazes. Weather/temperature exist but have no gameplay impact on colonists.
> **Systems used**: Construction, weather, temperature, lighting, wind
> **Depends on**: Feature 2 (furniture makes rooms useful)
> **Opens**: Clothing matters more (F7), room quality for moods (F10)

---

## What Changes

**Doors** are a new cell type: walkable by movers but block wind, rain, and temperature exchange. **Enclosed rooms** are detected automatically — a room with walls, floor, ceiling, and doors becomes **sheltered**. Shelter protects from rain/snow, reduces wind chill, and holds heat better. Beds inside shelter give bonus rest.

This is the feature that makes *building* meaningful. You're not just placing walls — you're creating shelter from the storm.

---

## Design

### Doors

A door is a **cell type** (CELL_DOOR) that:
- Is **walkable** (movers and animals pass through)
- **Blocks wind** (wind chill doesn't pass through)
- **Blocks rain/snow** (IsExposedToSky returns false for cells behind doors at same z)
- **Reduces temperature transfer** (acts as insulator between inside/outside)
- Can be **opened/closed** visually (door sprite changes when mover passes)

**Construction recipe**: 3 planks → door (at carpenter's bench from F2)

### Enclosed Room Detection

An enclosed room is a connected set of air cells on the same z-level where every boundary is either:
- A wall (CELL_WALL)
- A door (CELL_DOOR)
- A floor above (ceiling)
- A floor below (ground)

**Algorithm**: Flood-fill from any interior cell. If the fill reaches map edge or open sky without hitting wall/door on all sides → not enclosed. Cache result per room, invalidate when walls/doors/floors change.

**Room properties:**
- `isEnclosed`: bool
- `cellCount`: number of cells in room
- `hasDoor`: at least one door in boundary
- `temperature`: average interior temperature (insulated from exterior)

### Shelter Effects

| Condition | Outside | Sheltered Room |
|-----------|---------|----------------|
| Rain wetness | Full | None |
| Snow accumulation | Full | None |
| Wind chill | Full | None (doors block) |
| Temperature | Ambient | Insulated (slower exchange with outside) |
| Rest bonus (bed) | 1.0× | 1.25× (faster energy recovery) |
| Cloud shadow | Yes | No |

### Weather Protection

The weather system already tracks `IsExposedToSky()`. Extend this:
- Enclosed rooms: all cells marked as sheltered
- Rain doesn't increase wetness on sheltered cells
- Snow doesn't accumulate on sheltered cells
- Wind chill doesn't apply to movers in sheltered cells

### Temperature Insulation

Rooms with doors act as thermal zones:
- Temperature inside changes slower (walls + doors = insulation)
- A hearth inside a room heats the room effectively (heat stays in)
- This makes the hearth workshop functionally useful — it heats your home

---

## Implementation Phases

### Phase 1: Door Cell Type
1. Add CELL_DOOR to cell types
2. Walkable, but blocks fluids (wind, rain) like walls
3. Door construction recipe (3 planks at carpenter's bench)
4. Door sprite (open/closed variants)
5. Place via blueprint system
6. **Test**: Door is walkable, blocks IsExposedToSky for cells behind it

### Phase 2: Room Detection
1. Flood-fill algorithm for enclosed space detection
2. Room struct: cell list, enclosed flag, properties
3. Cache rooms, invalidate on construction changes
4. Mark cells as `sheltered` when inside enclosed room
5. **Test**: Room with walls + door is detected as enclosed, open room is not

### Phase 3: Weather Protection
1. Rain skips sheltered cells (no wetness increase)
2. Snow skips sheltered cells (no accumulation)
3. Wind chill doesn't apply to movers in sheltered cells
4. **Test**: Rain outside, dry inside. Snow outside, clear inside.

### Phase 4: Temperature Insulation
1. Doors reduce temperature transfer rate (like walls but less)
2. Enclosed rooms retain heat from hearth
3. Hearth in room → room stays warm
4. **Test**: Room with hearth stays warmer than outside

### Phase 5: Rest Bonus
1. Beds in sheltered rooms give 1.25× energy recovery
2. Visual indicator: room state shown in tooltip
3. **Test**: Sleeping in sheltered room restores energy faster

---

## Connections

- **Uses construction**: Doors built like walls
- **Uses weather**: Rain, snow, wind already simulated — doors gate their indoor effects
- **Uses temperature**: Hearth already produces heat — rooms retain it
- **Uses F2 (furniture)**: Beds inside rooms get rest bonus
- **Opens for F7 (Clothing)**: Shelter vs clothing as two ways to handle cold
- **Opens for F10 (Moods)**: Room quality (size, furniture, light) affects mood

---

## Design Notes

### Why Not Windows?

Windows could come later as a variant: blocks rain but not light, partial wind reduction. For now, doors are the critical missing piece. Windows are polish.

### Room Size Limits

Don't over-engineer room detection. A simple flood fill with a max cell count (e.g., 100) prevents pathological cases. Rooms larger than that are "open halls" — sheltered from rain but not insulated.

### Door Auto-Open

Doors should visually open when a mover approaches and close after they pass. This is purely cosmetic — for pathfinding purposes, doors are always walkable. No lock/unlock mechanic needed yet.

---

## Save Version Impact

- New cell type: CELL_DOOR
- Room detection cache (rebuilt on load, not saved)
- Sheltered flag per cell (derived, not saved)
- Carpenter's bench recipe addition

## Test Expectations

- ~25-35 new assertions
- Door walkability and fluid blocking
- Room detection (enclosed vs open)
- Rain/snow protection inside rooms
- Temperature insulation
- Rest bonus in sheltered rooms
