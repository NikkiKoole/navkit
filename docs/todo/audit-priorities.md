# Audit Priority Assessment

Generated: 2026-02-07

## Already Audited ✅
- items.c
- jobs.c
- mover.c
- rendering.c
- workshops.c
- stockpiles.c

## HIGH PRIORITY 🔴

These files have complex state management, inter-system coordination, and cleanup paths where "correct but stupid" bugs are likely.

### 1. **designations.c** (1,883 lines)
**Why audit**: Core work system with complex lifecycle management
- Bidirectional relationships: designations ↔ movers ↔ jobs
- Multiple completion functions with complex state transitions
- Cache invalidation patterns (`InvalidateDesignationCache()`)
- Active count tracking must stay in sync
- Tree felling: flood-fill algorithm, spawns items, handles leaves
- Channeling: mover descent logic, item dropping, pushing other movers
- Unreachable cooldown timer (similar pattern to items.c bug)
- **Key risk**: CancelDesignation cleanup paths

**Specific checks**:
- Does CancelDesignation() properly clear assignedMover and job references?
- Is unreachableCooldown cleared after ALL terrain changes?
- Tree felling: can flood-fill leave orphaned tree cells if interrupted?
- Does activeDesignationCount++ match activeDesignationCount-- everywhere?

---

### 2. **saveload.c** (1,070 lines) + **inspect.c** (1,627 lines)
**Why audit**: Save/load corruption = catastrophic player data loss
- MEMORY.md warns: "Both saveload.c AND inspect.c need parallel save migration updates"
- Multiple legacy struct definitions with hardcoded counts
- V16/V17 still use `ITEM_TYPE_COUNT` directly (latent bug)
- State reconstruction: RebuildSimActivityCounts(), spatial grids, HPA* graph
- Complex material migration with natural/constructed flags
- Reservation clearing: "not meaningful across save/load" - what else should be cleared?

**Specific checks**:
- Are ALL version branches using hardcoded legacy counts?
- Test save from each old version to verify migration correctness
- Should job assignments be cleared on load? Workshop assignments?
- Do both files handle the same enum changes in parallel?

---

### 3. **pathfinding.c** (3,625 lines)
**Why audit**: Pathfinding bugs = movers get stuck everywhere
- Incremental updates: RebuildAffectedEntrances() with complex entrance filtering
- Entrance hash table with linear probing - hash collisions?
- Graph edge management with MAX_EDGES_PER_NODE limit
- Ladder/ramp link lifecycle: links reference entrance indices
- Chunk-to-entrance index: bidirectional mapping must stay in sync
- 5 different MAX_* limits that can be hit silently

**Specific checks**:
- What happens when chunks become dirty during RebuildAffectedEntrances()?
- Test maps with many hash collisions
- Are ladder/ramp link references updated when entrances are rebuilt?
- What happens when MAX_EDGES_PER_NODE is exceeded?

---

### 4. **terrain.c** (4,044 lines)
**Why audit**: Terrain corruption = world becomes unplayable
- Connectivity validation: ReportWalkableComponents() flood-fill with state modification
- Multi-level coordination: PlaceMiniGalleryFlatAt() across z-levels
- Surface array management: off-by-one errors would corrupt terrain
- Blueprint placement: recursive/iterative building across z-levels
- Water mask coordination: race condition if water simulation runs during generation?

**Specific checks**:
- Does connectivity fix handle multi-z isolated regions?
- Do all surface array modifiers bounds-check?
- Can water/fire spawn during terrain generation?
- Test interrupted terrain operations (user cancels mid-build)

---

## MEDIUM PRIORITY 🟡

Moderate complexity, some state management worth checking.

### 5. **input.c** (2,151 lines)
- Mode state machine with drag state
- Designation batch operations: drag-to-designate partial cleanup?
- Stockpile filter UI: direct state manipulation bypassing validation

### 6. **input_mode.c** (582 lines)
- Hierarchical mode tracking: are inputMode, workSubMode, inputAction always in sync?
- Drag state: what happens if drag never ends (Alt-Tab during drag)?
- Pending key queue: single-slot buffer, can keys be lost?

### 7. **grid.c** (620 lines)
- Ladder cascade logic: infinite loop if circular ladder structure?
- Ramp validation: called reactively, not proactively when terrain changes
- Chunk dimension calculations: off-by-one errors?

---

## LOW PRIORITY 🟢

Simple/isolated logic, minimal state, unlikely to have assumption bugs.

- **sim_manager.c** (87 lines) - Counter aggregation only
- **pie_menu.c** (548 lines) - UI rendering, no persistent state
- **time.c** (65 lines) - Simple tick counter
- **item_defs.c** (35 lines) - Static data table
- **material.c** (104 lines) - Static lookups
- **cell_defs.c** (37 lines) - Static data

---

## SKIPPED (Different bug patterns, less critical)

- **src/simulation/** - Physics simulations (fire, water, steam, smoke, temperature)
  - Different bug patterns (numerical stability, edge propagation)
  - Less critical than gameplay systems
- **src/render/** - Rendering/UI (ui_panels.c, tooltips.c)
  - Visual bugs, not gameplay-breaking
- **src/sound/** - Audio system
  - Isolated from core gameplay

---

## Recommended Audit Order

Based on player impact severity:

1. **designations.c** - Work system bugs = gameplay broken
2. **saveload.c + inspect.c** - Save corruption = data loss
3. **pathfinding.c** - Pathing bugs = movers stuck everywhere
4. **terrain.c** - Terrain corruption = world unplayable
5. **input.c** - Input bugs = frustrating but usually recoverable
6. **input_mode.c** - Mode bugs = confusing but recoverable
7. **grid.c** - Grid bugs usually caught by other systems

---

## Notes

All HIGH priority files share these characteristics:
- Complex state lifecycle (creation → active → cleanup)
- Bidirectional relationships between systems
- Cache invalidation requirements
- Multiple exit paths that need symmetric cleanup
- Active count tracking that must stay in sync

These are exactly the patterns where "correct but stupid" bugs hide.
