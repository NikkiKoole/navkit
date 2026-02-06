# Rock vs Soil (Decision + Plan)

## Decision (2026-02-06)
We are going with **Option 1**:
- Add **`CELL_ROCK`** as a natural **terrain** type (a kind of soil/ground).
- **Constructed walls stay `CELL_WALL`** and are a separate concept.
- Rock is **mineable**, works with **pile mode**, and is part of **terrain logic**.
- Trees should **not** prefer rock (but other vegetation may).

This keeps “natural terrain” separate from “constructed walls,” and lets geology live in **materials** instead of proliferating cell types.

---

## Model (What We Mean)

### Terrain vs Constructed
- **Terrain**: dirt, clay, sand, gravel, peat, **rock** → `CELL_*` ground types.
- **Constructed**: walls built by players/structures → `CELL_WALL`.

### Movement / Support Reality
In this engine, **solid cells block movement at the same Z** and **support walking on top**. That applies to **both** terrain and walls. The key difference we care about is **classification** for tools, generation, and semantics.

---

## Material Plan (Rock Types)
- **`CELL_ROCK` uses the wall material grid** to store rock type.
- Rock types (granite, limestone, etc.) live in `MaterialType`.
- When mined:
  - `CELL_ROCK → AIR + FLOOR`
  - Floor material = the rock material
  - Drops stone based on material rules

This matches the existing “wall material → floor material” flow and avoids adding a new material storage path.

---

## Implementation Checklist
- Add `CELL_ROCK` to `CellType` and `cellDefs` with `CF_GROUND | CF_BLOCKS_FLUIDS`.
- Add `CELL_ROCK` to `IsGroundCell()` (or rename to `IsTerrainSolid()` if that reads better).
- Ensure mining accepts `CELL_ROCK` (same path as soil).
- Ensure pile mode accepts `CELL_ROCK`.
- Update terrain generation to place **`CELL_ROCK` for natural geology** instead of `CELL_WALL`.
- Keep `CELL_WALL` for **constructed walls only**.
- Use `MaterialType` for rock types (granite, limestone, etc.).
- Ensure rock floors inherit material when mined (already matches wall material flow).
- Update rendering sprites if needed (rock could reuse wall sprite or use a distinct rock sprite).
- Update UI labels so “rock” is shown as **terrain**, not a constructed wall.

---

## Migration Notes (Where Changes Will Land)
- `src/world/terrain.c`: natural rock generation → `CELL_ROCK`.
- `src/core/input.c`: rock draw key → `CELL_ROCK` (built walls still `CELL_WALL`).
- `src/world/designations.c`: mining acceptance includes `CELL_ROCK`.
- `src/entities/jobs.c`: minable/ground checks include `CELL_ROCK`.
- `src/render/*`: add rock rendering if needed.
- Tests: keep `CELL_WALL` where walls are intended; use `CELL_ROCK` where natural rock is intended.

---

## Open Questions (Non-blocking)
- Do we want **rock hardness** to be material-driven (mining time, drop rate)?
- Should **rock be a distinct sprite** or reuse wall art initially?
- Which vegetation types should tolerate rock (moss, fungus, etc.)?

---

## Summary
- **Rock is a kind of soil** (terrain). **Walls are constructed** (structures).
- This makes tools, generation, and UI more consistent.
- Material types carry the geology detail (granite, limestone, etc.).
