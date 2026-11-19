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
- `hier` costs are HPA*-quality (+8% vs optimal on average, see `tests/test_nav_standalone.c`).
