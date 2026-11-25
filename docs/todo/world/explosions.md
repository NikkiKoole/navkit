# Explosions (and structural support)

Status: **idea / investigation** (2026-11-25). No code yet. Nothing in `src/` knows about
force, blasts or support; this doc maps what exists to build on, the gaps, and a proposed
first slice. Open questions for the user at the bottom.

Earlier mentions: boiler explosions (`discussions with ai/steampower.md`), saltpeter for
gunpowder (`discussions with ai/wate into resurce.md`), firedamp gas
(`../vision/far future /gates-and-processors.md`), "no cave-ins" (`../vision/entropy-and-simulation/water-system-differences-with-df.md` §9).

## What exists to build on

| Need | Already there |
|---|---|
| Items that move by themselves | `ItemsDriftTick` (`src/entities/items.c`): float positions, a direction, slide along walls when blocked, go down a drop (`DriftSurfaceZ`), lost off the map edge. Skips reserved / stored / contained items. A blast push = the same loop with a decaying impulse instead of a current. |
| Weight | `itemDefs[t].weight` (kg), `ItemWeight(t)`; container weight is recursive (`GetContainerTotalWeight`). |
| Falling | `DropItemsInCell` (items fall to the lowest open z), `PushItemsOutOfCell`; movers: `TryFallToGround` + `fallTimer` (`mover.c`), setting `allowFallingFromAvoidance`. |
| Destroying cells | `CompleteMineDesignation` (wall -> floor + drop items, `MarkChunkDirty`, `DestabilizeWater`, `ValidateAndCleanupRamps`, `InvalidateRooms`). Fire burns solid cells into non-solid ones (`fire.c`, then re-checks ramps). |
| Directional debris | `FellTree` (`designations.c`) lays felled trunk segments away from the chopper. |
| Secondary effects | `IgniteCell`, smoke, temperature grid; `SteamCell.pressure:3` is unused ("Phase 2"), a natural hook for boiler explosions. |
| Juice | `TriggerScreenShake`, lightning flash (`DrawLightningFlash`), `TryLightningStrike` (`weather.c`) as a one-off point event; ideas in `../juice-effects.md` (particles not built). |
| Death | `KillMover` (`mover.c`, needs only today). |

## Gaps

1. **Walls float.** Mining turns a wall into floor and never looks at what's above. The only
   support check in the game is `ValidateAndCleanupRamps` (`grid.c`). Fire does the same.
2. **Movers have no health.** Alive or dead; no injuries, no stun.
3. **Materials have no strength.** `MaterialDef` has fuel, ignition resistance, insulation;
   nothing for "survives a blast" or "spans N cells".

## Proposal

### 1. Explosion = event, not a new sim layer

`Explode(x, y, z, power)` runs once (new file `src/simulation/explosion.c`):

- **Spread outward from the centre** through open cells (a capped BFS over the 3D grid) with power
  falling off per step. A wall soaks power equal to its strength: weaker than the remaining
  power, it breaks and lets the rest through; stronger, the blast stops there.
- Enclosed spaces come out right without special code: a tunnel carries a blast far, an open
  field fades it fast, a stone wall shelters what's behind it.
- Local and bounded (radius from power), so it fits the chunked / large-world work.
- Many cells break at once: mark chunks dirty and update pathfinding **once per blast**,
  not per cell (`pathfinding/per-chunk-edge-storage.md`: one mining update can cost 67 ms at 512).

### 2. Loose things get pushed

- Per item reached: impulse = local power / weight, direction away from the centre. Leaves and
  sticks fly, stones shift, logs barely move.
- A short-lived velocity per item, moved by a tick shaped like `ItemsDriftTick` (slide on
  walls, fall into holes, lose speed). Small list of "flying" items, not a field on all 25k.
- Unlike drift, a blast can't skip stored / reserved items: take them out of the stockpile
  slot, release reservations / cancel the jobs holding them. The state audit (item<->stockpile,
  reservations<->jobs) will catch mistakes; test with `--audit`.
- Save: transient velocities can be dropped on save (items land where they are), see
  `../transient-state-save.md`.

### 3. Movers

- Knockback + short stun; pushed over a ledge they fall (`TryFallToGround`).
- Death near the centre: a decision (survival only?). Injuries would be a separate system.
- Animals the same way (they have `velX/velY` already for steering).

### 4. Breaking walls

- Broken cell -> rubble: drop items like mining (maybe fewer, scattered by the blast), or a new
  rubble cell (walkable, slow) that has to be cleared.
- Built walls weaker than natural rock; per-material strength as a new `MaterialDef` field.

### 5. Side effects

Ignite flammables near the centre, puff smoke / dust, add heat, flash + screen shake + sound.
Water: leave alone at first.

### 6. Structural support (its own feature)

Explosions make floating walls common, so this belongs next to them. Options, simplest first:

- **(a) Column rule:** a built wall needs solid below, or a supported wall within N cells
  sideways; N by material (wood short, stone longer).
- **(b) Connected pieces (DF-like):** after removing a cell, flood-fill nearby solid cells; any
  piece no longer connected to the ground collapses. Elegant, but the fill needs a budget on
  big maps.
- **(c) Stress values per cell** (Valheim / 7 Days style): support decreases away from columns.
  Most flexible, most work and memory.

Collapsing cells fall to the lowest open z as rubble and hit what's below (items squashed?
movers knocked / killed?).

Recommendation: **(a), built walls only.** Natural rock and hillsides stay as they are,
otherwise mining on the island turns into constant cave-ins.

### 7. Sources (gameplay)

- Gunpowder: saltpeter (piss + manure + straw) + charcoal + sulfur?
- Boiler running dry / overheating (steam pressure bits).
- Firedamp gas in caves.
- Lightning into a powder store.
- Blast mining: fast, dangerous digging.
- Dev: `explode x y z power` as a row in `src/ui/commands.c`, a click tool in the dev UI.

## Proposed order

1. `explode` command + blast spread + wall breaking by strength + item scatter by weight + shake/flash/fire;
   scenario `explosions` (headless-checkable: cells broken, items moved, audit clean).
2. Mover knockback + stun (+ animals).
3. Structural support, column rule, built walls only.
4. Gameplay sources: gunpowder, blast mining, boiler.

Touches busy shared files (`items.c`, `designations.c`, `grid.c`, `mover.c`): keep the bulk in
`explosion.c` with small hooks.

## Open questions (user)

- Which games get it? (survival + island natural, dollhouse probably not, sandbox for testing)
- Can blasts kill, or only knock back / stun?
- Collapses: built walls only, or natural rock too (real cave-ins)?
- Tone: rare dramatic event, or a regular mining tool?
