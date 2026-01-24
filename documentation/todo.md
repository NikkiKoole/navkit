# NavKit TODO

Small remaining items and future improvements.

---

## Prioritized TODO

### Quick Wins (small, high value)
- [x] **Blocked movers handling** - DONE. Movers pick new goal when goal becomes wall. Jobs cancel immediately when item/stockpile blocked. On-screen messages for all cases.
- [ ] **Easy cell types** - add CELL_FENCE, CELL_WATER, CELL_DOOR_OPEN/CLOSED (no pathfinding changes needed)

### Short-term (meaningful improvements)
- [ ] **Variable movement costs** - road (cheap), rubble/mud (expensive) - NOTE: disables JPS/JPS+

### Medium-term (new capabilities)
- [ ] **Ramps** - directional entry (N/E/S/W), single cell z-transition
- [ ] **Multi-cell stairs** - horizontal offset while changing z-level (enter at one x,y, exit at different x,y)
- [ ] **Queuing system** - orderly waiting at bottlenecks (ladders, doors, narrow passages). Prerequisite for elevators and other social navigation features.
- [ ] **Refactoring** - dedupe heap implementations, direction arrays, entrance building (~200 lines saved)
- [x] **Jobs/Hauling system** - DONE. Settlers/DF-style item hauling to stockpiles (46 tests).
  - Items with types (R/G/B), stockpiles with filters, reservation system
  - Stacking/merging, per-stockpile stack limits, overfull handling
  - Stockpile priority with re-hauling, gather zones
  - Unreachable item cooldown, safe-drop on cancellation
  - Debug tooltips (stockpile hover, mover hover when paused)
- [ ] **Jobs next phase** - See [jobs-roadmap.md](jobs-roadmap.md) for full plan
  - [ ] Stockpile/gather zone UI (draw, remove, configure)
  - [ ] Mining/digging (designations, terrain modification)
  - [ ] Architecture refactor (Job pool, WorkGivers) - optional but recommended before construction
  - [ ] Construction (blueprints, material delivery, chained jobs)
  - [ ] Simple crafting (workshops, recipes)
- [ ] **Containers** - Portable storage (chests, barrels, wagons, bin, box). Containers are items themselves (can be hauled, stored in stockpiles) but also hold other items inside. Design questions: filters per container, hauling full containers (weight?), contents vs stockpile stats.

### Larger efforts (build on queuing)
- [ ] **Elevators (full sim)** - moving elevator with state, queuing, capacity, wait times (requires queuing)
- [ ] **Social Navigation (full)** - yielding, lane preference, personal space (requires queuing)
- [ ] **Decoupled Simulation** - headless backend + 2.5D view (multi-phase)
- [ ] **Flow Fields** - for many agents sharing one goal (more efficient than individual pathfinding)
- [ ] **Territory/Ownership Costs** - agent-specific movement costs (RimWorld-style: colonists avoid others' bedrooms, visitors stick to public areas)
- [ ] **Formation System** - units moving in formation (wedge, line, box), leader-follower patterns, formation pathfinding
- [ ] **Spatial Queries Layer** - unified interface for "find nearest X", k-nearest neighbors, range queries. Could add quadtrees, R-trees, or BVH beyond current bucket grids
- [ ] **Terrain Cost / Influence Maps** - threat/danger maps for tactical AI, heat maps showing congestion
- [ ] **Multi-layer Navigation** - bridges, tunnels, teleporters, jump pads (each layer has own grid with connections)

### Optimizations (from crowd-optimization-techniques.md)
- [ ] **View frustum culling for movers** - only draw movers visible on screen
- [ ] **Spatial grid prefix sums for mover avoidance** - O(n) bucket sort for neighbor queries
- [ ] **Periodic mover sorting** - reorder mover array by cell every N frames for cache locality
- [ ] **Y-band sorting for movers** - hybrid bucket+insertion sort for 2.5D depth sorting

---

## Feature Details

### Blocked Movers Handling
When walls are drawn that block movers or their goals, movers become stuck (orange). Options to handle this:
- [ ] **Report Only**: Show "N movers blocked" in status bar
- [ ] **Auto New Goals**: Blocked movers automatically get new reachable goals
- [ ] **Stop & Idle**: Blocked movers become idle (gray), button to reassign
- [ ] **Region Islands**: Track connected regions, only assign goals within same region
- [ ] **Report + Button**: Show count + "Reassign Blocked" button (recommended)

### Social/Crowd Features
**Phase 1 - Queuing (foundation)**
- [ ] Mark cells as queue points (ladders, doorways, narrow passages)
- [ ] Track waiting movers per queue point (first-come-first-served)
- [ ] Movers wait at nearby positions instead of crowding
- [ ] Signal next mover when queue point clears

**Phase 2 - Full Social Navigation (requires queuing)**
- [ ] **Yielding**: Agents give way to each other
- [ ] **Lane Preference**: Keep-right/left for counter-flow
- [ ] **Personal Space**: Context-dependent spacing

---
Pheromones are interesting** because they're emergent - ants don't know the optimal path, but the colony finds it through reinforcement. Heavily-traveled routes get stronger trails.

**For your colony sim**, you could combine:
1. **Flow fields** for common destinations (entrance, food storage, etc.)
2. **Pheromone-like decay** so the flow field weakens on unused paths
3. **Queuing** at actual bottlenecks (ladders, doors)

This avoids the chaos of per-agent steering forces while still getting natural-looking crowd behavior.

Want me to add a note about this flow field / pheromone overlap to the todo or create a small brainstorm doc?
---

## See Also (Design Docs)

- [jobs-roadmap.md](jobs-roadmap.md) - Jobs system expansion (mining, construction, crafting, architecture refactor)
- [needs-vs-jobs.md](needs-vs-jobs.md) - Separating individual needs (eating, sleeping) from colony jobs (future)
- [vision.md](vision.md) - What this game is and isn't (tone, influences, aesthetic)
- [entropy.md](entropy.md) - Decay, growth, maintenance loops, seasons, water flow
- [logistics-influences.md](logistics-influences.md) - Research on Factorio, belt games, SimTower influences
- [next-steps-analysis.md](next-steps-analysis.md) - External review validating our roadmap + mechanics to steal
- [decoupled-simulation-plan.md](decoupled-simulation-plan.md) - Headless sim with 2.5D views (detailed phases)
- [elevators.md](elevators.md) - Full elevator simulation design
- [done/convo-jobs.md](done/convo-jobs.md) - Jobs/Hauling system design conversation (completed)
- [done/hauling-next.md](done/hauling-next.md) - Hauling features checklist (completed)
- [done/pathing-optimizations-plan.md](done/pathing-optimizations-plan.md) - Pathing optimizations (mostly done)
