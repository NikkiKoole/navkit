# 3D crowds: ladder bottlenecks and queue-aware routing (2026-11-19)

> Status: findings. Experiment: `experiments/flow/demo3d.c` (`make flow3d`).

## Setup

- **Building:** 3 floors of 96×72 cells, exit on the ground floor.
- **Connections:**
  - 3 ladders: single file, one climber per ladder segment at a time, 0.8 s per floor.
  - 2 ramps, 3 and 2 cells wide: walked across, changing floor on the way.
- **Agents:** 1500, starting on floors 1 and 2. They separate from and collide (hard bodies)
  only with agents on their own floor.
- **Navigation:** a full `nav/` 3D flow field from the exit, rebuilt 4×/s when crowd costs
  are on. Its routes use exactly the same ladder and ramp rules as A*.
- **Crowd costs** (added to `extraCost`):
  - **density:** smoothed agents per cell, 3×3 blur.
  - **queue:** a ladder cell costs the expected wait, i.e. the agents within 2 cells of its
    base × climb time × (10 cost per cell × walking speed).
- One run per row; fixed seed and timestep.

## Results

| Crowd costs | Everyone out after | Longest ladder queue | 3-wide ramp crossings |
|---|---|---|---|
| none | not done after 360 s (357 still inside) | 202 | 691 |
| density only | 245 s | 142 | 1190 |
| queue cost only | 89.8 s | 25 | 1438 |
| **density + queue cost** | **52.7 s** | 36 | 1413 |
| hierarchical flow, no crowd costs | 226 s | 125 | 1231 |
| hierarchical flow, density only | 349 s | 128 | n/a |
| hierarchical flow, queue cost only | **44.6 s** | 23 | n/a |
| hierarchical flow, density + queue | 52.7 s | 29 | n/a |

Simulation cost was about 0.2 ms per frame for 1500 agents, and total navigation time under
0.25 s per run.

## Observations

- **Naive shortest-path routing sends everyone to the nearest connector.** A single ladder
  ended up with a disc of 500+ agents around it, draining at 1.25 agents/s.
- **Density alone helps, but it only sees the people standing in the way.** It doesn't know
  that a ladder serves one person at a time.
- **Pricing the expected wait into the ladder cell fixes the routing decision.** Agents spread
  to the ramps and ladder queues stay short: a 4.6× faster evacuation than density alone.
  Combining both costs is best.
- **The hierarchical flow field (`NavHFlow`) takes extra costs too** (added after the first
  runs). Local chunk fields add them per cell; the entrance graph adds the extra cost of the
  entrance cell each edge leads into, which is exactly where the ladder queue cost sits. With
  queue cost it evacuates as well as or better than the full field (44.6 s).
- **Density alone can hurt the hierarchical field:** 349 s, worse than no costs at all. At the
  graph level it mostly sees crowding at entrance cells. A busy ramp entrance looks expensive,
  so people get pushed onto the ladders, whose real cost (one person at a time) density
  doesn't capture. The queue cost is the term that matters for connectors.

## Relation to plans

This validates two Phase 1 items in `docs/todo/pathfinding/social-navigation.md`:

- one mover per ladder segment, with emergent queues;
- congestion-aware path cost at bottlenecks.

It also gives a concrete cost model for both: expected wait = queue × service time.
