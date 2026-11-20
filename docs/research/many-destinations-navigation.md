# Many destinations: full vs hierarchical flow fields vs HPA* (2026-11-19)

> Status: findings. Experiment: `experiments/flow` town scenario (`make flow`, `make flow-big`).

The colony case: thousands of agents, each heading to one of many destinations. Three ways
to navigate were compared on the same agents, goals and steering. All three use separation and
hard bodies, cache each agent's target per cell, and use next-cell steering.

- **full:** one whole-map `NavFlowField` per destination, built when first needed and cached.
- **hier:** one `NavHFlow` per destination: a Dijkstra over the HPA* entrance graph, plus
  16×16 chunk fields built when an agent first needs them, then cached.
- **hpa:** an HPA* path per agent, planned on each new destination and followed waypoint by
  waypoint.

The town is a street grid of 14×11 buildings, each with one door. Agents walk to a random
destination building, then pick another. Each run is 60 s simulated with a 20 s warm-up and
one seed. "Steady" means after the warm-up. "Worst frame" includes start-up, when every agent
asks for directions in the same frame.

## 512×512 (`flow_big`), 5000 agents

| Destinations | Mode | Steady nav time per frame | Worst frame | Total nav time | Nav memory | Process memory |
|---|---|---|---|---|---|---|
| 128 | full | **0.34 ms** | 4028 ms | 5.3 s | 512 MB | 601 MB |
| 128 | hier | 0.80 ms | **304 ms** | **3.9 s** | 232 MB | 372 MB |
| 128 | hpa | 1.94 ms | 3494 ms | 9.4 s | **21 MB** | **110 MB** |
| 924 | full | n/a: about 3.9 GB of fields (924 × 4.2 MB) | | | | |
| 924 | hier | **1.86 ms** | **874 ms** | **7.6 s** | 487 MB | 552 MB |
| 924 | hpa | 2.09 ms | 3787 ms | 10.2 s | **22 MB** | **119 MB** |

Arrivals were about 95 per second and mean travel about 36.5 s in every run, so route quality
is effectively equal.

## 200×125, 3000 agents, 88 destinations

| Mode | Steady per frame | Worst frame | Total | Nav memory |
|---|---|---|---|---|
| full | **0.17 ms** | 214 ms | **0.6 s** | 33.6 MB |
| hier | 0.33 ms | **78 ms** | 0.9 s | 29.1 MB |
| hpa | 0.89 ms | 398 ms | 2.3 s | **4.4 MB** |

Caching each agent's target per cell cut steady nav time from 0.64 to 0.17 ms (full) and from
1.8 to 0.33 ms (hier).

## Conclusions

- **Small maps or few destinations: use full flow fields.** They are the fastest per frame
  and their memory is affordable. On 200×125, hierarchical builds almost every chunk anyway,
  so the hierarchy is pure overhead.
- **Big maps with many destinations: use hierarchical fields.** Full fields stop being
  possible (memory). Hierarchical uses the least CPU in total, has by far the smallest
  start-up spike, and is faster per frame than per-agent HPA*.
- **HPA\* paths have the smallest memory by far** but the highest steady CPU, and a start-up
  spike when every agent plans at once. Staggering plans over frames would fix the spike.
- **Hierarchical's weak spot is memory:** 117k chunk fields at about 4 KB each for 924
  destinations. Two obvious fixes:
  - **Compact chunk fields:** a 16-bit cost and a 1-byte direction instead of three ints, about 3× smaller.
  - **An LRU budget for chunk fields,** evicting cold ones and rebuilding them on demand (chunk builds are cheap).

## Caveats

- One seed per run. The spikes are start-up artefacts; a real game staggers requests.
- The town spreads destinations uniformly. Real colonies concentrate traffic on a few hot
  destinations (stockpiles, kitchen), which favours any caching approach.
- `hier` costs were measured before the terrain fixes below; they are now within about 1%
  of optimal (better than HPA* paths).

## Every terrain generator (`make bench_nav_terrains`, added later)

All 19 game generators at 128×128×16, checked against A*. Before the fixes, `NavHFlow` agents
looped forever or got stuck on concentric (14% reached the goal), spiral3d, towers, castle and
hillswater, and council paths were +10% too long. Three causes, all fixed:

1. **Exit values came from the symmetric abstract graph.** An exit valued lower than what
   walking on actually costs makes a trap that agents step into and back out of. Exits are now
   valued from the field that continues the route (or the ladder/ramp step plus the far side).
   Chunks can depend on each other in a cycle, so the fields involved are rebuilt until their
   values stop changing (values only go down, so this converges). An unknown exit is left out,
   never guessed: overestimates are harmless, underestimates trap agents.
2. **Ambiguous "is the route inside this chunk?".** Two entrances on the same chunk border
   belong to both chunks, but the abstract edge between them was computed in only one.
   `NavGraphEdge.chunk` now records which, and fields use it.
3. **Several entrances on one cell** (a ladder foot on a chunk border): handing over from one
   field to the next could land on another exit. The handover now follows the chain.

Result: 100% reached on every terrain, +0–1.4% cost vs optimal (HPA* paths: +0.6–1.9%).
Regions, flow fields and nearest-by-walking were already exact everywhere.

The older 2D generators (sparse, concentric, dungeon, drunkard, tunneler, mixmax) carve their
layout into level 1 but leave level 0 empty, so in the game they play as an open plain. The
bench fills level 0 for them; the generators themselves are unchanged.

## In the game: shared-goal fields (2026-11-20)

`FindPath` (`src/world/pathfinding.c`) now promotes a goal requested 3+ times within 10 s
to a cached `NavHFlow` (32 slots, least recently used evicted, refreshed every 2 s so
ladder queue costs stay current, rebuilt when the graph changes). HPA* requests for
it walk the field instead of searching; one-off goals keep using HPA*. Toggle:
`useSharedGoalFields` / console `sharedGoalFields`.

`--scenario hubs` (128x128 hills, 200 movers shuttling between 6 hubs), 3000 ticks:

| | FindPath total | per call | tick | arrivals |
|---|---|---|---|---|
| shared-goal fields off (HPA*) | 1239 ms | 1.23 ms | 0.90 ms | 936 |
| on | 357 ms | 0.36 ms | 0.59 ms | 938 |

978 of 990 requests were answered by 6 fields, with no fallbacks. Not measured
yet: memory on 512x512 maps (each field builds chunk fields along the routes walked).

Found on the way: a mover going up a ramp stood inside the solid cell below the
exit for half a cell (its level only rose at the exit's centre). It now rises
while crossing the ramp cell and reaches the new level at the exit's edge.
