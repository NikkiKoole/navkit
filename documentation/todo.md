# NavKit TODO

Small remaining items and future improvements.

---

## Prioritized TODO

### Quick Wins (small, high value)
- [ ] **Blocked movers handling** - show count + reassign button
- [ ] **Easy cell types** - add CELL_FENCE, CELL_WATER, CELL_DOOR_OPEN/CLOSED (no pathfinding changes needed)

### Short-term (meaningful improvements)
- [ ] **Directional ladders** - LADDER_UP, LADDER_DOWN, LADDER_BOTH (~80 lines, enables one-way vertical flow)
- [ ] **Variable movement costs** - road (cheap), rubble/mud (expensive) - NOTE: disables JPS/JPS+

### Medium-term (new capabilities)
- [ ] **Ramps** - directional entry (N/E/S/W), single cell z-transition
- [ ] **Multi-cell stairs** - horizontal offset while changing z-level (enter at one x,y, exit at different x,y)
- [ ] **Queuing system** - orderly waiting at bottlenecks (ladders, doors, narrow passages). Prerequisite for elevators and other social navigation features.
- [ ] **Refactoring** - dedupe heap implementations, direction arrays, entrance building (~200 lines saved)

### Larger efforts (build on queuing)
- [ ] **Elevators (full sim)** - moving elevator with state, queuing, capacity, wait times (requires queuing)
- [ ] **Social Navigation (full)** - yielding, lane preference, personal space (requires queuing)
- [ ] **Decoupled Simulation** - headless backend + 2.5D view (multi-phase)
- [ ] **Flow Fields** - for many agents sharing one goal

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

## See Also

- `decoupled-simulation-plan.md` - Headless sim with 2.5D views (future)
- `pathfinding-ideas.md` - More detailed design notes
- `roadmap.md` - Full feature roadmap
