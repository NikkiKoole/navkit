# Crowd avoidance comparison (2026-11-19)

> Status: findings. Experiment code: `experiments/flow/demo.c` (`make flow`).

Which local-avoidance method should large crowds on `nav/` flow fields use? Six candidates
were measured in the same steady two-way corridor test: two new ones and the four avoidance
behaviours from the earlier `experiments/steering` library.

## Setup

- **Scenario:** `counterflow`. Two rooms joined by a corridor. Agents enter at a fixed rate
  per direction (`--rate`) near the corridor mouth and leave on arrival just inside the far room.
  Statistics start after a warm-up, so they describe steady state.
- **Shared by every method:** `nav/` flow fields (any-angle steering, wall clearance and crowd
  density costs), per-axis wall sliding, speed capped at the agent's own pace, and two passes of
  hard-body overlap resolution.
- **Library methods** are called as-is in a scaled pixel frame (45 px per cell, so an agent is
  about 20 px across, as in the old demos), with their own parameters and `steering_apply`
  rules (500 px/s² max force). The old lane recipe's wall-avoid term keeps its 2.0 weight
  with zero force, so the blend normalisation matches.
- **Command:** `flow --scenario counterflow --corridor 2 --length 40 --speed 1.8
  --avoid <mode> --rate <r> --agents 6000 --frames 6300 --warmup 45`.
  Measured over 60 s, one run (one seed) per cell.

## Results: 2-wide corridor, 1.8 cells/s

Arrivals per second eastbound / westbound, and mean travel time:

| Inflow per direction | 2/s (light) | 4/s (heavy) | 6/s (overloaded) | Agent update at 6/s |
|---|---|---|---|---|
| **separation** | **2.0 / 1.8**, 39 s | **3.6 / 3.3**, 44 s | 2.3 / 2.4 | 0.5 ms |
| predictive (TTC, Karamouzas 2014) | 2.0 / 1.9, 40 s | 2.6 / 2.5, 55 s | 1.2 / 0.4 | 1.0 ms |
| sfm (`steering_social_force`) | 1.0 / 0.3, 69 s | 0 / 3.0 | 5.4 / 0 | 1.1 ms |
| lib-predict (`steering_predictive_avoid` + seek) | 0.3 / 0.4, 55 s | 0.4 / 0.5 | 0.2 / 0.8 | 0.7 ms |
| lanes (old "SFM: Corridor (Lanes)" recipe) | 0 / 0 | 0 / 0 | 0 / 0 | 1.1 ms |
| clearpath (`steering_clearpath`) | 0.3 / 0.3, 58 s | 0 / 0.3 | 0 / 1.3 | 17 ms |

## Observations

- **separation:** the best throughput at every load and the cheapest. Use it as the default.
- **predictive:** equal at light load; forms the cleanest lanes, but knots at the corridor
  entrances under heavy load. Earlier, at 7 cells/s, the same pattern held: 2-wide corridor at
  30/s, 28.4 vs 25.8 per s.
- **lib-predict:** far too polite. Lanes are perfect and contacts are zero, but agents keep so
  much personal space that both rooms fill with waiting lattices and the corridor fills one
  agent at a time.
- **sfm:** one direction takes over the corridor under pressure (0 vs 3.0, then 5.4 vs 0).
  This is a known social force behaviour, and fatal in a game.
- **lanes:** cohesion clumps same-direction agents into blobs that stall. It was tuned for 25
  agents per side in open space.
- **clearpath:** samples every candidate velocity against every neighbour, so it is about 35×
  the cost of separation and still jams. The old demo never used it.

## Lessons about the test itself

- Two crowds spawned at once move as waves and barely meet. You need a steady rate-based
  inflow with a warm-up.
- Without hard bodies and a speed cap nothing ever saturates, because agents squeeze through
  each other and crowd pushing makes them walk faster. Every method looked equal until both
  were fixed.
- The corridor has to be narrow relative to agent size before avoidance quality matters. At
  4 and 12 cells wide, every method kept up.

## Caveats

- One run per cell. The close pairs (separation vs predictive at 2/s) are within noise.
- The library methods were not retuned for density; this measures them "as we had them".
- No method receives wall-repulsion forces; walls are handled by shared collision for all.
