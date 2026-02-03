# Social Navigation

Crowd behavior and social movement features.

---

## Phase 1 - Queuing (foundation)

- [ ] Mark cells as queue points (ladders, doorways, narrow passages)
- [ ] Track waiting movers per queue point (first-come-first-served)
- [ ] Movers wait at nearby positions instead of crowding
- [ ] Signal next mover when queue point clears

## Phase 2 - Full Social Navigation (requires queuing)

- [ ] **Yielding**: Agents give way to each other
- [ ] **Lane Preference**: Keep-right/left for counter-flow
- [ ] **Personal Space**: Context-dependent spacing

---

## Dependencies

Queuing is a prerequisite for:
- [elevators.md](elevators.md) - Elevator wait queues
- Full social navigation features above
