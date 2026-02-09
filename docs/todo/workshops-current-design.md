# Current Workshop Design Notes (As Implemented)

Date: 2026-02-07
Scope: What the existing workshops *actually* are in code, and how they should be interpreted in‑world.

## Shared Workshop Anatomy (Current Implementation)
Workshops are 3×3 templates with a small set of tile roles:
- `#` = **Block tile** (non‑walkable machinery, sets `CELL_FLAG_WORKSHOP_BLOCK`)
- `X` = **Work tile** (crafter stands here; input is deposited here)
- `O` = **Output tile** (finished items appear here)
- `F` = **Fuel tile** (currently used for kiln smoke visual; fuel still carried to work tile)

There is **no explicit input tile**; inputs are fetched by the job system from nearby items/stockpiles.

## Workshop: Stonecutter
**Template**:
```
##O
#X.
...
```
**Inputs**: `ITEM_ROCK`
**Outputs**: `ITEM_BLOCKS`

**World interpretation**: A stone‑cutting bench with a tool rack. Output stack is at `O`.

**Autarky role**: Converts raw stone into a primary building material.

## Workshop: Sawmill
**Template**:
```
##O
#X.
...
```
**Inputs**: `ITEM_LOG`
**Outputs**: `ITEM_PLANKS`, `ITEM_STICKS`

**World interpretation**: A simple saw rig / log deck. Output stack at `O`.

**Autarky role**: Converts logs into structural and fuel materials.

## Workshop: Kiln
**Template**:
```
#F#
#XO
...
```
**Inputs**: `ITEM_CLAY` (plus fuel when recipe requires it)
**Outputs**: `ITEM_BRICKS`, `ITEM_CHARCOAL`

**World interpretation**: Kiln body (`#`), fuel mouth (`F`), work spot (`X`), output stack (`O`).

**Autarky role**: Clay → bricks (durable structures); logs → charcoal (efficient fuel).

**Note**: The fuel tile is currently cosmetic (smoke anchor). Fuel items are carried to the work tile and consumed there.

## What’s Missing (But Not Required Yet)
- **Input tile**: There is no dedicated input zone or log deck yet.
- **Orientation/rotation**: All workshops are fixed 3×3 layouts.
- **Workshop‑specific behaviors**: Only recipes differ; visuals/sounds are minimal.

## Optional Near‑Term Improvements
- Differentiate workshop layouts (still 3×3) so each reads distinctly.
- Add optional “input tile” support if you want visible log/clay staging.
- Make fuel tile functional (deposit fuel there, consume from `F`).

## Layout Options (If You Want More Variety)

### A) Distinct 3×3 layouts (no code changes)
These are drop‑in replacements that keep the current 3×3 template contract.

**Stonecutter (dense/solid)**:
```
###
#XO
..#
```

**Sawmill (open lane)**:
```
#O#
.X.
#..
```

**Kiln (hot core)**:
```
#F#
#XO
###
```

### B) Variable sizes (small + large workshops)
Requires a small refactor:
- Add `width`/`height` to `WorkshopDef`.
- Use `width * height` instead of a fixed 3×3 in `CreateWorkshop()`.

**Example 2×2 (small workshop)**:
```
XO
#.
```

**Example 4×3 (larger workshop)**:
```
##O.
#X..
##F.
```

### C) Minimal input‑tile system
If you want “log decks” / “clay piles” to be real:
- Add `WT_INPUT = 'I'` tile type.
- Store `inputTileX/Y` on the workshop (like work/output/fuel).
- When a job deposits input, drop it at `I` instead of `X`.

This keeps the current job logic but makes layouts feel more physical.
