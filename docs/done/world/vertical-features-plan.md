# Vertical Features Plan

Planning document for: buildable ladders, mining rename, channeling, and ramps.

## Status: MOSTLY COMPLETE

| Feature | Status | Notes |
|---------|--------|-------|
| **Rename Dig → Mine** | ✅ Done | Completed |
| **Ramps** | ✅ Done | 4 cardinal directions, bidirectional movement |
| **Channeling** | ✅ Done | Creates ramps when wall adjacent below |
| **Buildable Ladders** | ❌ TODO | See `/docs/todo/buildable-ladders.md` |

## Overview

Four related features that expand vertical gameplay:

| Feature | Summary | Dependencies |
|---------|---------|--------------|
| **Buildable Ladders** | Ladders as items that can be crafted, hauled, placed | Item types, construction system |
| **Rename Dig → Mine** | Horizontal wall removal = "mining" | Terminology only |
| **Channeling** | Vertical digging downward from above | New designation type |
| **Ramps** | Terrain-based directional z-transitions | Pathfinding changes |

---

## 1. Buildable Ladders

### Current State
- Ladders exist as cell types: `CELL_LADDER_UP`, `CELL_LADDER_DOWN`, `CELL_LADDER_BOTH`
- Placed instantly via input (drawing mode)
- No material cost, no construction job

### Proposed Change
Ladders become items that are:
1. **Crafted** at a workshop (wood → ladder item)
2. **Hauled** to blueprint location
3. **Installed** by a builder (like wall construction)
4. **Removable** (uninstall → get ladder item back)

### Implementation Steps

1. **Add ladder item type**
   - `ITEM_LADDER` in items.h
   - Stackable? Probably not (bulky)

2. **Add ladder blueprint type**
   - Currently blueprints only build walls
   - Need `BlueprintType` enum: `BLUEPRINT_WALL`, `BLUEPRINT_LADDER`
   - Or: generic blueprint with `targetCellType` field

3. **Modify CompleteBlueprint**
   - Check blueprint type
   - Place appropriate cell type instead of always `CELL_WALL`

4. **Ladder removal**
   - New designation or tool: "deconstruct"
   - Removes ladder cell, spawns ladder item
   - Reuses existing job patterns

### Open Questions
- Should ladder direction (up/down/both) be chosen at placement time?
- Or always place BOTH and let pathfinding handle it?
- Workshop recipe: what input material? Wood seems natural.

---

## 2. Rename Dig → Mine

### Current State
- Designation type: `DESIGNATION_DIG`
- Job type: `JOBTYPE_DIG`
- Removes walls horizontally (same z-level)
- Key: 'M' (already "M for mining")

### Proposed Change
Pure rename for clarity:
- `DESIGNATION_DIG` → `DESIGNATION_MINE`
- `JOBTYPE_DIG` → `JOBTYPE_MINE`
- `JOB_MOVING_TO_DIG` → `JOB_MOVING_TO_MINE`
- `JOB_DIGGING` → `JOB_MINING`
- Function names: `DesignateDig` → `DesignateMine`, etc.

### Why Rename?
- "Dig" will be used for channeling (digging down)
- "Mine" = horizontal wall removal (getting ore/stone)
- Matches Dwarf Fortress terminology

### Implementation
- Global find/replace with care
- Update UI text if any
- Update tests
- Low risk, just tedious

---

## 3. Channeling (Dig Down)

### Current State
- No way to dig downward
- No way to create holes in floors
- Ramps would need channeled areas to connect

### Proposed Behavior
Stand on z+1, designate "channel" on a floor tile:
1. Mover walks to tile on z+1
2. Digs downward, removing the floor
3. Creates either:
   - **Air/hole** at z (can fall through)
   - **Ramp** at z (can walk down)
   - **Stairs** at z (bidirectional)

### Cell Changes
In DF-style walkability:
- Currently floors have `HAS_FLOOR(x,y,z)` flag
- Channeling would `CLEAR_FLOOR(x,y,z)`
- Result: air at z, mover could fall

### Safety Considerations
- Don't channel tile you're standing on (fall damage)
- Must approach from adjacent tile or z+1
- Warning if channeling would trap movers?

### Implementation Steps

1. **New designation type**
   - `DESIGNATION_CHANNEL`
   - Key: 'C' or 'D' for dig?

2. **New job type**
   - `JOBTYPE_CHANNEL`
   - Similar to mining but:
     - Stand on z+1 (or adjacent on same z?)
     - Modify cell below instead of adjacent

3. **Decide output**
   - Option A: Always create air (simplest)
   - Option B: Create ramp (useful but complex)
   - Option C: Player chooses (UI complexity)
   
   Recommend: Start with Option A (air only), add ramp creation later.

4. **Spawned items**
   - Channeling dirt → dirt/soil item?
   - Channeling stone floor → stone item
   - Or just debris that needs cleaning

### Open Questions
- Can you channel from the same z-level (adjacent tile)?
- Or must always be from z+1 looking down?
- What happens to items on the channeled tile? (Push out like walls)

---

## 4. Ramps

### Current State
- Design doc exists: `docs/done/z-levels/df-stairs-and-ramps.md`
- Not implemented
- Ladders handle all z-transitions currently

### Proposed Behavior
Ramps are terrain features with a direction:
- `CELL_RAMP_N` - enter from north, walk up to z+1
- `CELL_RAMP_E` - enter from east, walk up to z+1
- `CELL_RAMP_S` - enter from south, walk up to z+1
- `CELL_RAMP_W` - enter from west, walk up to z+1

Walking onto ramp from correct direction → transition to z+1.
Walking onto ramp from wrong direction → stay on same z (or blocked).

### Natural vs Built Ramps
Two sources:
1. **Natural** - created by channeling (dig creates ramp below)
2. **Built** - construction like walls/ladders

### Pathfinding Changes
From the design doc:

```c
static bool IsRamp(CellType cell);
static void GetRampEntryDir(CellType cell, int* dx, int* dy);
static bool CanEnterRampUp(int rampX, int rampY, int rampZ, int fromX, int fromY);
```

A* needs to check movement direction when entering ramp cells.
HPA*/JPS+ need directed edges for ramps.

### Implementation Steps (from design doc)

**Phase 1: Cell types and walkability (~30 lines)**
- Add `CELL_RAMP_N/E/S/W` to enum
- Add `CF_RAMP` flag handling
- Update `IsCellWalkableAt()`

**Phase 2: A* support (~40 lines)**
- Add direction helpers
- Modify neighbor expansion to check ramp entry direction
- Add z+1 neighbor when entering ramp correctly

**Phase 3: HPA*/JPS+ support (~50 lines)**
- Create directed edges in graph building
- Or: handle ramps only during refinement (simpler)

**Phase 4: Rendering**
- Visual for ramp direction (arrow? slope?)
- Different from ladder visual

### Decisions Made

1. **Directions:** 4 cardinal directions only (N/E/S/W) - start simple, add diagonals later if needed

2. **Rendering:** Gradient shading
   - Darker at low end, lighter at high end (or vice versa)
   - Clean edge-to-edge gradient for cardinal directions
   - Shows slope direction clearly in top-down view

3. **Bidirectional:** Yes - can walk UP and DOWN ramps (like DF)

4. **Creation method:** Drawable for now (like current ladders), channeling creates them later

5. **Z-level placement:** Ramp cell is on the LOWER z-level (like DF)
   - Draw ramp at z=0 → connects z=0 to z=1
   - You place the "bottom" of the ramp
   - Direction determines exit tile on z+1

### How Ramp Movement Works

```
CELL_RAMP_N at (5, 5, z=0):
- Enter from south: (5, 6, z=0) → step onto ramp (5, 5, z=0)
- Exit north at z+1: (5, 5, z=0) → (5, 4, z=1)

Going down (reverse):
- Enter from north at z+1: (5, 4, z=1) → step onto ramp (5, 5, z=0)  
- Exit south: (5, 5, z=0) → (5, 6, z=0)
```

The ramp direction indicates where the "high" side is:
- `RAMP_N` = high side is north, enter/exit from south on z, exit/enter north on z+1
- `RAMP_S` = high side is south, enter/exit from north on z, exit/enter south on z+1
- `RAMP_E` = high side is east, enter/exit from west on z, exit/enter east on z+1
- `RAMP_W` = high side is west, enter/exit from east on z, exit/enter west on z+1

---

## Build Order

```
1. Rename Dig → Mine  (quick cleanup, frees "dig" terminology)
2. Ramps              (cell types + pathfinding, no dependencies)
3. Channeling         (can now create ramps when digging down)
4. Buildable Ladders  (independent, extends construction system)
```

### Dependencies

```
┌─────────────────┐
│ Rename Dig→Mine │  (no dependencies, do first)
└─────────────────┘

┌─────────────────┐
│     Ramps       │  (no dependencies, pathfinding only)
└────────┬────────┘
         │
         v
┌─────────────────┐
│   Channeling    │  (depends on ramps existing to create them)
└─────────────────┘

┌─────────────────┐
│   Buildable     │  (independent, can do anytime)
│    Ladders      │
└─────────────────┘
```

---

## Things to Think About

### Interaction: Channeling + Ramps
- Should channeling automatically create a ramp?
- Or separate "channel" (hole) vs "dig ramp" (ramp)?
- DF has both: channel creates ramp, can also carve ramps from walls

### Interaction: Ramps + Water
- Water flows down ramps?
- Or ramps block water like walls?
- Current water only flows on same z or falls straight down

### Interaction: Buildable Ladders + Existing Ladders
- What happens to "drawn" ladders when we add buildable ones?
- Keep both? (debug/sandbox mode vs survival mode)
- Or deprecate instant placement?

### Visual Clarity
- How to show ramp direction in top-down 2D?
- Arrow overlay? Slope shading? Isometric hint?
- Ladders are easier (just a ladder icon)

### Mover Safety
- Channeling creates fall hazards
- Should movers avoid channel designations while work in progress?
- What if mover falls? Damage? Instant death? Just teleport?

---

## Estimated Effort

| Feature | Code Changes | Test Changes | Risk |
|---------|--------------|--------------|------|
| Rename Dig→Mine | ~50 lines (rename) | ~20 lines | Low |
| Buildable Ladders | ~100 lines | ~50 lines | Low |
| Ramps (pathfinding) | ~150 lines | ~100 lines | Medium |
| Channeling | ~200 lines | ~100 lines | Medium |

Total: ~500 lines of code, ~270 lines of tests

---

## Next Steps

1. Decide on build order
2. Start with first feature
3. Write tests before implementation
4. Update this doc as we learn more
