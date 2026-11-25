# Explosions (and structural support)

Status: **partial** (2026-11-25). Slice 1 built (see "Built"): blasts break walls and floors
(natural rock too: blast mining), throw loose items by weight, knock movers back. Not yet:
structural support, gameplay sources (gunpowder, blast mining as a job), animals, visuals.

Earlier mentions: boiler explosions (`discussions with ai/steampower.md`), saltpeter for
gunpowder (`discussions with ai/wate into resurce.md`), firedamp gas
(`../vision/far future /gates-and-processors.md`), "no cave-ins" (`../vision/entropy-and-simulation/water-system-differences-with-df.md` §9).

## Built (slice 1, 2026-11-25)

`src/simulation/explosion.c` / `.h`, test `tests/test_explosion.c`, scenario `explosions`.

- `Explode(x, y, z, power)`: power in cells (open air: power 6 reaches 6 cells). Flood over
  the 26 neighbours, highest power first, -1 per cell of distance (diagonals 1.41 / 1.73).
  A wall or a built floor over air costs its strength; with enough left it breaks and the rest
  goes through. Diagonal steps can't squeeze past a wall the blast couldn't break, so walls
  shelter what's behind them. A charge inside a wall (drilled in) breaks that cell for free.
- Strength (`ExplosionCellStrength`, table in explosion.c, not in MaterialDef): soil/mud/cob 2,
  wood 2.5, sandstone/brick 3, slate 3.5, granite 4, concrete 4.5, iron 8, glass/window 0.5,
  floors half their material; bedrock, trees, doors: stop it, never break (yet).
- Broken walls go through `CompleteMineDesignation` (drops, ramps, rooms, water, designations);
  a broken cell over a hole the blast made keeps no floor. Floors: `CompleteRemoveFloorDesignation`.
  Stockpile free slots are recounted after.
- Items: push = 2.4 * power / (kg + 0.5)^0.25 cells/s (power where they lay *before* the ground
  broke). Thrown at their height, slide along walls, land (and fall) when slower than 2 cells/s.
  Rubble from broken walls flies too. Stored items leave their slot; an item reserved for a job
  cancels that job; one reserved by a need (on the way to eat it) stays put.
- Movers: push = 1.2 * power / sqrt(kg / 70) (carried weight counts), dazed 0.4 + 0.15 * speed s
  (max 2.5), drawn blue like a fall; `ExplosionMoverTick` in the movement loop; landing over a
  hole they fall, after the knock they repath. No harm (user: knockback only for now).
- Fire on burnable cells within 2.5 of the charge, a puff of smoke, screen shake, event log.
- Fuses: `ExplosionLightFuse(x, y, z, power, seconds)`. Fuses, flying items and knocked movers
  are transient (not saved; cleared with movers / items).
- Console: `explode [power] [x y z]` (mouse cell on the viewed level by default), `charge [power]
  [fuse] [x y z]`. `CMD_MAX_ARGS` went 3 -> 5 for these.
- Scenario `explosions` (in scenarios-check): field charge (power 6): leaves 3.1 cells, sticks
  2.4, planks / log 0.5, rock 0.1; the open mover thrown 0.9 cells, the one behind granite not;
  the oak fence breaks. Charge drilled into a granite hill (power 8): 26 granite cells out.

Seen while building: a charge on open dirt makes a wide shallow crater (~3.5 cells, ~35 dirt
items). Tune soil strength or blast coupling into the ground if that's too much.

Next: structural support (column rule; walls and natural rock both, user's call), gameplay
sources (gunpowder, blast mining job with a charge item), animals knocked too, a fuse sprite and
a flash / debris particles.

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
  not per cell (`../../done/pathfinding/per-chunk-edge-storage.md`: one mining update can cost 67 ms at 512).

### 2. Loose things get pushed

- Per item reached: impulse = local power / weight, direction away from the centre. Leaves and
  sticks fly, stones shift, logs barely move.
- A short-lived velocity per item, moved by a tick shaped like `ItemsDriftTick` (slide on
  walls, fall into holes, lose speed). Small list of "flying" items, not a field on all 25k.
- Unlike drift, a blast can't skip stored / reserved items: take them out of the stockpile
  slot, release reservations / cancel the jobs holding them. The state audit (item<->stockpile,
  reservations<->jobs) will catch mistakes; test with `--audit`.
- Save: transient velocities can be dropped on save (items land where they are), see
  `../../done/transient-state-save.md`.

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

## Decisions (user, 2026-11-25)

- Movers: knockback only for now (no harm).
- Walls and natural rock both break: blast mining wanted.

Still open: which games get it; collapses for natural rock too (cave-ins) or built walls only;
rare event or regular mining tool.
