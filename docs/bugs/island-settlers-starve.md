# Island settlers starve by day three

**Status: MOSTLY FIXED 2026-11-26** (found 2026-11-25): 6 of 8 seeds keep all three settlers alive 10 days without a player; seed 7 open (see the end)

All three settlers of the `island` game die of starvation on day 3, in every run tried. It is
not the social need (same with `socialNeed` on and off) and not the games (they don't start:
nobody idles near a friend).

## Repro

```bash
./build/bin/path --headless --game island --seed 3 --ticks 64800 --events ev.log
grep -iE "died|ate item" ev.log
```

| seed | deaths (event log) | meals in 3 days (3 settlers) | drinks |
|---|---|---|---|
| 3 | D3 10:18, D3 19:45, D3 19:47, all starvation | 5 | 46 |
| 1 | D3 00:34, D3 09:53, D3 10:03, all starvation | 2 | 37 |

They drink plenty (the river) but hardly eat: every meal is raw food standing up ("Ate raw
food", "Ate without table"). So food is found rarely or not at all. Places to look first: what
food the island has near the landing (berry bushes, forage), whether the settlers' jobs leave
them time to look for it, and whether food searches fail (unreachable, timeouts).

## Also seen

The 2,000-mover load test on the island (`--game island --exec "spawnmovers 2000 add"`, one
game day, `--profile`) ended with every mover dead (mostly starvation, some thirst), so that load
test no longer measures a busy colony over a whole day. The action-queue doc's measurement
(`../todo/architecture/action-queue-and-need-index.md`, ~940 movers) was taken when they lived.
Unknown whether the island used to keep its settlers alive (worth checking an older commit
before digging into the food code).

## Cause, part 1: too few bushes (fixed 2026-11-26)

The island placed bushes only where the tree roll fell just short (~0.1% of grass/scrub, none
in forest, none near ramps): 27 bushes on a 256 island (seed 3), two thirds bare (96 game hours
to ripe). Survival's hills place 211 on 128x128. After the 5 starting berries nobody found food
again: `FindForageBush` (48 cells of walking) found no ripe bush. Now `island_gen.c` places them
as survival does (forest edges 8%, open 2%, clumped; a third ripe, a third budding): 167 on seed 3.

| run | before | after |
|---|---|---|
| seed 3, 3 days | 5 meals, all dead D3 | 18 meals, all alive |
| seed 1, 3 days | 2 meals, all dead D3 | 18 meals, all alive |
| seeds 0-7, 10 days | - | all starve D3-D9 (seed 7 first, 8 meals) |

## Part 2: survival's other food (2026-11-26)

The island now also has what survival's map has: wild crops (wheat in grass, lentils in scrub
and moor, flax in marsh and by water; ripe; 186 on seed 3; they give seeds to start farms) and
8 grazers to hunt, placed on reachable ground at the start (`SpawnAnimalAt`, animals.c).
Without a player both stay unused (harvest and hunting are orders), so the 10-day no-player run
is the same: seeds 0-7 starve between D3 and D9, 7-45 meals.

Still open (not a generator bug any more): three settlers left alone eat the bushes within
their 48-cell forage search bare (a bush ripens again after ~4 game days); survival's single
settler lives 10 days on 211 bushes. Options: settlers forage further when nothing is near,
bushes give more than one handful, or accept that the island needs a player. Also: animal
respawns come from the map edge (sea on the island), so hunted game doesn't come back yet.

## Part 3: forage further, bushes give more (2026-11-26, user picked both)

- A bush gives `bushBerries` berries when picked (setting, default 3; was 1). The rest lies by
  the bush and is the next meal (food on the ground comes first).
- No ripe bush within ~48 cells: one more search as far as `forageFarCells` (setting
  `forageFar`, default 160; only bushes the settler has seen).

| island, 10 days, no player | meals (3 settlers) | deaths |
|---|---|---|
| seeds 0-4, 6 | 72 each (2.4 per settler per day) | 0 |
| seed 5 | 64 | 1 (D10) |
| seed 7 | 14 | 3 (D4-D6) |

Survival seeds 0 and 3, 10 days: 24 meals, alive.

**Left: seed 7.** Only 7 bushes are ever discovered; one settler forages far, the other two
only eat what's left by the bushes it picked and starve. Bushes under the fog can't be found, and
exploring was left out on purpose (2026-11-23: it pulled settlers away from their water). If
wanted: explore only when hungry with no food known.
