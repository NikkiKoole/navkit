# Explosions (and structural support)

Status: **partial** (2026-11-25). Built: blasts break walls and floors (natural rock too: blast
mining), throw loose items by weight, knock movers back (slice 1); structural support: what
loses its connection to the ground creaks and falls, landing as walls (slice 2); gunpowder and
blast mining as a job (slice 3); gunpowder goes off in fire and chains (slice 4); dithered
blast / dust / chunk / spark effects (slice 5). Not yet: animals, overhang limits.

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

## Built: structural support (slice 2, 2026-11-25)

`src/world/support.c` / `.h`, test `tests/test_support.c`, scenario `collapse`, settings
`structuralSupport` (on) and `collapseWarning` (2 game s), group World/Explosions.

Measured first (generated worlds, 2 seeds each): every world is fully connected to the ground
(only Towers3D seed 1 has 10 floating cells) and every upper-storey floor hangs off grounded
walls; overhangs reach up to 6 cells (HillsSoilsWater cave roofs) and 7+ (Towers3D bridges).
So the rule is connection, not overhang length: a length limit would bring fresh worlds down.
Generated cells are all marked natural (castle walls too), so natural vs built can't steer it.

- Structure: solid cells except trees (walls, rock, soil, windows, doors) and floors. Solids hold
  each other face to face; a floor hangs off a solid in the 3x3 on its level or the one below,
  or off a floor beside it. Anchors: z = 0, bedrock, anything touching a tree (soil on roots;
  trees never fall here). A piece that can't reach an anchor is unsupported.
- Trigger: `MarkChunkDirty` (every cell change the pathfinder hears of) calls
  `SupportNoteChange`; a cell that is open now (and holds no water: that's water reporting its
  depth) is checked next tick, up to 256 per tick. The search goes lowest level first, stops at
  the first anchor (or a cell already found grounded this tick), gives up after 20000 cells
  (then it's anchored). Cost: island with tide, survival, 512 world all under 0.03 ms/tick;
  worlds identical with it on and off.
- Fall (user's calls): a warning first (dust trickles from the underside, `collapseWarning`
  seconds; propped up in time it holds), then each wall drops straight down (through floors,
  which break) and lands as a wall again, same material, rough; floors become rubble items; a
  door / window or a wall landing on a ramp, ladder ... becomes its drop item. Items on top come
  down. Whoever a wall lands on is moved out (beside, or up on the pile) and knocked + dazed
  (`ExplosionKnockMover`). Dust, a shake, the event log.
- Pending collapses are transient (cleared with explosions; not saved).
- New worlds settle: `GenerateCurrentTerrain` ends with `SupportSettleWorld()`, which drops
  whatever hangs at once, without warning or effects (whole-world scan: one sweep marking cells
  on grounded columns, a flood into the rest; 2048 world ~0.2 s). Survival's HillsSoilsWater
  left a 9-cell floating lump at z7-8 that a settler digging nearby would have brought down.
  Games set `supportEnabled = true` before generating (their start()).
- Pathing rigs whose decks stand on nothing (ladders, ladder-choice, ladder-oneway,
  oneway-tiers, belt-lift, elevator, elevator-tall, elevator-stairs, elevator-build) set
  `supportEnabled = false`; scenario reset restores it. Without that elevator-build's decks fell
  when the shaft cut its hole (arrivals 46 -> 35). Ladders deliberately hold nothing.
- Console `support`: counts structure cells not connected to the ground.
- Scenario `collapse`: charges take the foot out of a granite pillar with an arm (lands as a
  stump), a 15x3 roof slab on one pillar with a mover under it (36 of 45 cells land on the
  ground, the rest in the blast crater; the mover is pushed out and dazed) and a wooden deck on
  one post with a log on it (25 floors -> planks, the log on the ground). Nothing left hanging.

Found while building: soil resting on a tree root counted as hanging and "fell" in place forever
(island): trees are anchors now.

Not done: overhang / span limits (a 15-cell slab on one pillar stands as long as it touches it),
falling pieces don't hurt or crush items (pushed out), no sound or proper dust particles, no
warning drawn on the cells themselves (only dust), animals aren't pushed out.

## Built: gunpowder and blast mining (slice 3, 2026-11-25)

Save v114 (items ITEM_SALTPETER, ITEM_GUNPOWDER; stockpile allowedTypes 83 -> 85, StockpileV113).
Scenario `blasting` (in scenarios-check), tests in test_explosion "blast_designation".

- Chain (user's pick): hearth "Leach Saltpeter" 2 compost + 1 ash + 1 water -> 1 saltpeter
  (4 h); quern "Grind Gunpowder" 1 saltpeter + 1 charcoal -> 1 gunpowder (1 h). Planned as 2
  saltpeter + 1 charcoal -> 2, changed to one of each: a recipe takes an input's count from one
  stack, and two saltpeter often sit in two stockpile slots (the quern never started). No sulfur.
- `DESIGNATION_BLAST` (adjacent, like mining; any wall the blast can break: not bedrock or trees)
  -> `JOBTYPE_BLAST` (miners, `canMine`): fetch one gunpowder (one off a stack, split when the
  job is given so other blasters take the rest), carry it to the wall, set it (0.3 h), light it
  (`ExplosionLightFuse` in the wall cell: drilled in), walk to the nearest cell at least
  `blastPower` + 2 away on that level (`FindBlastShelter`), job done there or when the fuse
  must have burned down. Settings `blastPower` 5 (granite: its cell + the 6 around it; ~20 cells
  of soil), `blastFuse` 5 s.
- Tools: dev Work bar Dig -> Blast (b), drag, right-drag cancels; play HUD (survival, island):
  "Blast Rock" once there is gunpowder (`GameToolContext.hasGunpowder`).
- Headless `--events FILE` writes the event log after the run (added to find the bug below).

Found while building: a blaster picked up the whole stockpile stack of 3 gunpowder and setting the
charge deleted all three; one blast job at a time held the stack. Both fixed by splitting one off.

Not done (slice 3): blasters don't warn others (only themselves get
clear); no charge item with its own fuse length; no sprite of its own (gravel / dirt sprites).

## Built: gunpowder and fire (slice 4, 2026-11-25)

Scenario `powder-fire` (in scenarios-check), tests in test_explosion "gunpowder_fire".

- Every `gunpowderFireCheck` (0.25 s) while anything burns (`fireActiveCells > 0`): gunpowder in
  a burning cell, or lying on burning ground (grass burns in the soil cell below), goes off
  after a short fizz (0.5-1.1 s); next to a fire (the 8 around on its level or the level below)
  a spark sets it off with chance `gunpowderSpark` (0.25) per look. Lying, stored, in a
  container, or carried for a job (the carrier's job is cancelled, it drops it, it goes off).
- A blast sets off gunpowder where at least 1 power reaches (0.15-0.3 s later): stockpiles chain.
- A pile of n goes off as one charge of `blastPower * cbrt(n)` (8 = twice the power, capped).
- `ExplosionCount()`; the console `explode` reports how much gunpowder it set off.
- Headless runs now check the simulation activity counters at the end (as the windowed loop does
  every 60 s): found `docs/bugs/smoke-activity-counter-drift.md` (smoke.c, not explosions).

## Built: blast and dust effects (slice 5, 2026-11-25)

Procedural, no sprites (user's call: dithering and circles changing colour). `src/core/effects.c`
(data, spawn, aged on sim ticks in SimStep: frozen while paused, same in headless runs and
`--screenshot-ticks`), `src/render/effects_draw.c` (drawing). Setting `effects` (Rendering & Debug).

- Dithered discs, one quad each through a small shader: snapped to the 8x8 sprite pixel grid,
  a 4x4 Bayer threshold anchored to the world (doesn't crawl with the camera), density falling
  toward the rim plus value noise (clumps, not a checkerboard), a ragged rim in 2-pixel blocks,
  hard colour steps (inner / outer). Fading = fewer pixels, never alpha.
- Blast: grows to 0.8 x power cells, white -> yellow -> orange -> red -> smoke over 0.45 + 0.05 x
  power s, thinning after 30 %. Flash: a white halo for 0.1 s. Smoke (user: "a little more"):
  10 + 2 x power puffs over the blast, showing as the fireball dies, rising and swelling for
  2.5-4.5 s, thick at first and thinning late, two shades of grey.
- After a short web look at how others do it (user: "not this perfect disc"): the fireball is a
  cluster of 4-9 lumps (a big one in the middle, smaller ones off centre, each with its own size,
  start and length); every disc's outline bulges by smooth noise around it; the dither noise
  drifts with age (boils); a big blast (power 6+: 2 frames, 8+: 4) holds the picture (hit stop,
  windowed loop only). A shockwave ring out to the real reach was tried and dropped (user: "a bit
  much"; the crater and the thrown things already show the reach; the shader's `ring` band stays).
  Sources: PixelJoint (lumpy circle, fast then slow, smoke from the lumps), Quirkbit's stylised
  explosions (noise clipping, rotating noise), Vlambeer's screenshake talk (hit stop), NVIDIA's
  "Bigger bang with fewer sprites" (shockwave). Not done: one shared density buffer for smoke
  (puffs merge instead of stacking), a light flash through the lighting system, a boom sound.
- Real smoke: the blast fills the open cells of its inner half with simulated smoke (level 2-7,
  thickest at the centre); the smoke simulation takes it up level by level (top of a 16-level map
  in about a second), spreads it and clears it slowly (user: looking up, there was no smoke).
- The smoke and steam simulations draw as dithered clouds too (effects on): one texel per
  visible cell (r = the viewed level, g = the strongest below), sampled smoothly so cells blend,
  two layers of drifting noise, Bayer dither on the sprite pixel grid; smoke below the viewed
  level is the same grey, thinner. Drawn after the world batch (main.c `DrawGasClouds`, the
  batch would paint over it); effects off or no shader: the old squares.
- Chunks (1-2 sprite pixels, the broken wall's material colour) fly out in an arc with a bounce;
  dust puffs from broken walls, where a collapse lands, and trickling under a creaking piece
  (shown on the floor it falls to); sparks from a burning fuse.
- Effects show on the viewed level, the one below and the one above. With none alive nothing is
  drawn (pictures byte-identical to before).
- Look at them: `--scenario explosions --look-at 12,16 --exec "set zoom 2" --screenshot-ticks 130
  --screenshot f.png` (the field charge goes off at tick 120).

Next: animals knocked too, movers keeping away from a lit charge or a burning powder store, a
gunpowder / saltpeter / charge sprite, maybe span limits per material for built things.

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
- Collapse: falling mass lands as walls; whoever is under is pushed aside + dazed; a short
  warning before the fall.

Still open: which games get it; rare event or regular mining tool.
