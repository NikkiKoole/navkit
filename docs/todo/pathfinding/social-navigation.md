# Social Navigation

> Status: spec

Crowd behavior and social movement features.

---

## Phase 1 - Passive Bottleneck Throughput

Ladders, doors, and narrow passages already create natural bottlenecks. Rather than building explicit queue data structures, improve how movers handle these:

- [ ] Ladders: 1 mover per segment, others wait at top/bottom (avoidance handles spatial layout)
- [ ] Doors: congestion-aware pathfinding cost (movers prefer alternate routes when a door is busy)
- [ ] Pathfinder cost adjustment based on mover density near bottlenecks

No queue structs needed — existing avoidance + pathfinder cost feedback handles it. See `transport-layer.md` for the full rationale (research into real-world queuing shows spatial queues are emergent, not explicit).

### Measured so far (2026-11-19)

- Congestion-aware path cost is prototyped in `experiments/flow` as a crowd-density
  `extraCost` on `nav/` flow fields (rebuilt 5x/s), together with a wall-clearance cost.
- Local avoidance comparison: plain separation plus hard bodies beat predictive (time to
  collision), social force, the old steering library's predictive avoidance, its lane recipe
  and clearpath in a saturated two-way corridor. See
  [crowd-avoidance-comparison.md](../../research/crowd-avoidance-comparison.md).
- Ladder bottlenecks in 3D (`experiments/flow/demo3d.c`): one climber per ladder segment
  produces real queues. Pricing the expected wait (queue × climb time) into the ladder's path
  cost cut a 1500-agent evacuation from 245 s (density cost only) to 52.7 s. See
  [3d-crowd-bottlenecks.md](../../research/3d-crowd-bottlenecks.md).

## Phase 2 - Full Social Navigation (future)

- [ ] **Yielding**: Agents give way to each other
- [ ] **Lane Preference**: Keep-right/left for counter-flow
- [ ] **Personal Space**: Context-dependent spacing

---

## Dependencies

Passive bottleneck improvements are a prerequisite for:
- [elevators.md](../../world/elevators.md) - Elevator waiting areas
- [transport-layer.md](transport-layer.md) - Transport layer (Layer 0)
- Full social navigation features above
