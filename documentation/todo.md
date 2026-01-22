# NavKit TODO

Small remaining items and future improvements.

---

## Code Quality (Low Priority)

- [ ] **ChunkHeap z-level comment**: Add comment explaining why `nodeData[0]` is safe in heap functions (only used within single z-level `AStarChunk` calls)
  - Location: `pathfinding.c:257-295`

- [ ] **RunAStar performance**: Replace O(n^3) best-node scan with priority queue for 3D A*
  - Currently iterates all `gridDepth * gridHeight * gridWidth` nodes each step
  - Only matters for large multi-level maps

---

## Future Features

### Pathfinding Optimizations
- [ ] **Lazy Refinement**: Only refine 1-2 path segments ahead; faster response, less wasted work
- [ ] **Path Caching**: Cache recent paths with LRU eviction
- [ ] **Parallel Pathfinding**: Thread-local node data for concurrent queries

### Blocked Movers Handling
When walls are drawn that block movers or their goals, movers become stuck (orange). Options to handle this:
- [ ] **Report Only**: Show "N movers blocked" in status bar
- [ ] **Auto New Goals**: Blocked movers automatically get new reachable goals
- [ ] **Stop & Idle**: Blocked movers become idle (gray), button to reassign
- [ ] **Region Islands**: Track connected regions, only assign goals within same region
- [ ] **Report + Button**: Show count + "Reassign Blocked" button (recommended)

### Social/Crowd Features
- [ ] **Queuing**: Orderly lines at bottlenecks
- [ ] **Yielding**: Agents give way to each other
- [ ] **Lane Preference**: Keep-right/left for counter-flow
- [ ] **Personal Space**: Context-dependent spacing

---

## See Also

- `decoupled-simulation-plan.md` - Headless sim with 2.5D views (future)
- `pathfinding-ideas.md` - More detailed design notes
- `roadmap.md` - Full feature roadmap
