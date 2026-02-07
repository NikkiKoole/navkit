# Fire Ignition + Material Fuel Rules

## Goal
Make fire behavior match player intuition:
- Wood constructions burn (walls, floors, ladders/ramps).
- Stone constructions do not.
- Igniting nearby grass should realistically spread to adjacent burnable structures.
- Tree trunks should burn more consistently (not only the top segment).

## Current Behavior (Observed)
- **Ignite tool** only ignites cells where `CellFuel(cell) > 0` (`ExecutePlaceFire`).
  - `CELL_WALL` fuel is 0, so **wood walls won’t ignite when clicked**.
- **Material fuel only applies to constructed walls** in `GetFuelAt`.
  - Floors ignore `floorMaterial`, so **wood floors do not burn**.
- **Fire only spreads orthogonally on the same z**.
  - If you click grass on z and it places fire on z-1, it won’t spread to walls on z.
- **`CanBurn` blocks ignition if the cell above blocks fluids**.
  - For **tree trunks**, this prevents burning except at the topmost segment.

## Problems
1. Clicking to ignite a **wood wall** fails (fuel=0 at cell type).
2. **Wood floors** never burn (material fuel ignored).
3. **Tree trunks** appear non-flammable unless the top segment is burning.
4. **Fire placed on grass** doesn’t ignite adjacent walls/trunks when z differs.

## Proposed Changes

### 1. Ignite Tool Uses `CanBurn`
Replace base-fuel check with `CanBurn` (or `GetFuelAt > 0`) so material fuel counts.
- Result: wood walls/floors/ramps ignite when clicked.

### 2. Fuel Source Logic Includes Floors
Update `GetFuelAt`:
- If `HAS_FLOOR(x,y,z)` and no blocking wall, use `floorMaterial` fuel.
- This makes **wood floors burn**.

### 3. Tree Trunk Burning
Consider relaxing the “cell above blocks fluids” check for trunks:
- Option A: apply the ceiling check only for **spread**, not initial ignition.
- Option B: allow ignition if `GetFuelAt` > 0 regardless of ceiling.
- Option C: special-case `CELL_TREE_TRUNK` to ignore the ceiling check.

### 4. Optional: Same-Click Z Logic
For the fire tool, choose a single target:
- If clicked cell is a wall/trunk/ladder/ramp → ignite that cell.
- Else if air above solid → ignite `z-1`.
- Else ignite clicked cell.

This prevents “fire went to z-1” surprises when trying to burn a wall.

## Implementation Touchpoints
- `src/core/input.c` `ExecutePlaceFire` (ignition logic)
- `src/simulation/fire.c` `GetFuelAt`, `CanBurn`
- `src/world/material.c` (material fuel values)
- `src/world/cell_defs.c` (cell base fuel values)

## Open Questions
- Should fire spread vertically (z+1) to adjacent walls/trunks?
- Should ladders/ramps burn at full wood rate or reduced?
- Should grass ignition prefer the surface cell or underlying floor?

## Minimal Test Checklist
- Ignite a **wood wall** directly: should catch.
- Ignite a **wood floor** directly: should catch.
- Ignite **grass next to wood wall**: wall should catch within a few spread ticks.
- Ignite **tree trunk mid-height**: should catch.
- Stone walls remain non-flammable.
