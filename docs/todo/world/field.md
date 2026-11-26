# The field: a 64x64 starting map with some nature in it

Status: built 2026-11-27. Game `field` (`src/core/games.c`), generator `src/world/field_gen.c`,
scenario `field` (checks), settings `fieldRelief` (flat / gentle) and `fieldWater` (none / ponds /
stream), sandbox rules. The user asked for a "simple but useful and varied" start instead of the
empty grass field (which stays as it is: the sandbox and many scenarios use it).

## What it makes

- **Ground**: flat (top z 2) or gentle (z 2-4 from smooth noise); every neighbour pair at most one
  level apart, one-cell bumps flattened.
- **Water**: ponds (3-5 round hollows one level down, puddles of level 2 to ponds of 3-4, clay beds)
  or a stream (two-wide channel meandering west to east, gravel bed, a trickling spring at its west
  end, running off the east edge; on flat ground its bed never rises, on gentle ground it follows
  the land and its pools spill on). Land is lowered toward the water with a slope of one level per
  cell. `waterEdgeOutflow` on.
- **Ramps**: carved into the higher cell of every slope step whose foot and far end stay plain
  ground and whose sides pass the island's side rule (`IslandRampSidesOk`); along the stream only
  every 6th cell (fords).
- **Soil**: loam (dirt) with patches of sand, gravel and clay from noise, peat and clay in the wet
  ground near water.
- **Plants**: grass of mixed height on loam by noise (short to taller), lush on clay and peat,
  sparse on sand, mostly bare on gravel; reeds at the water's edge; ~1% of good soil away from water
  gets a tree (`PickTreeTypeForSoilSimple`); loose rocks on gravel.
- **Soil moisture** (`moisture.h`) set from the soil (sand 70, gravel 55, loam 100, clay 130, peat
  170), +60/+35/+15 within 1/2/3 cells of water, -8 per level above the lowest ground, plus noise.
- Day length 360 s like the other games (on 60 s the stream evaporated faster than its spring ran).

## Connectivity: everything walkable is one piece

The hard part. Checked by `FieldWalkableComponents`: dry ground (no tree tops, nothing under water)
that the pathfinder can reach from the main land and get back from (`IsNavBfs` both ways, the
island's helper). Fixes, in order: the placement rules above; `FieldJoinPockets` (dead ramps back to
ground; a ramp up or down to the main land, carved or built on top, both ends plain and sides ok;
else level the cell with a neighbour; else a ramp beside it that only lets one step up onto it
becomes ground); then the island's `IslandFixPockets`. Scenario `field` fails `--check` if anything
is cut off as generated (later the stream can pool too deep to wade and cut a few cells off: a
stream crossed at its fords). 192 maps (32 seeds x 6 kinds) pass; `tools/scenarios_check.sh` runs
four.

What kept failing on the way, for the next generator: ramps placed half the time (gaps made ramp
sides one-way traps), ramps ending on other ramps (crossed ramps), a stream bed that never rises
cutting a stepped valley through gentle ground (terraces), a flood-fill check that counted one-way
pockets as connected, and tree roots and canopies counted as ground.

## Open

- Without rain (weather is off in every game) the stream slowly shrinks (flat ~570 -> 360 units in
  3 days, gentle more as its pools evaporate) and ponds dry over days. With two spring cells it piled
  up too deep to wade near the spring. Options: weather on in the field (rain refills it), a
  stronger spring with a deeper outlet, or accept it.
- Grass is only drawn on loam: clay, peat and sand show their bare sprite even with plants on them.
- Silt along the stream and rocky soil on slopes (`soil-moisture.md` talk about the soil matrix).
