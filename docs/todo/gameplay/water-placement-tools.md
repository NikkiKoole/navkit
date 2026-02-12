# Water Placement Tools

Date: 2026-02-11
Status: Design proposal - needed for water-dependent workshops

---

## Context

Water-dependent crafting (mud mixer, clay pit, etc.) requires players to have **control over water placement**. Currently:
- ✅ Water simulation exists (`src/simulation/water.c`)
- ✅ Functions exist: `SetWaterSource()`, `SetWaterDrain()`, `SetWaterLevel()`
- ❌ No player actions to place water sources/drains
- ❌ No tool to draw rivers/channels

**Dependency:** Water-dependent workshops (mud mixer on riverbed) require this feature.

---

## Proposed Tools

### 1. Place Water Source (Spring)
**Key:** `S` (when in appropriate mode)
**Effect:** Sets `isSource = true` at target cell
**Description:** Creates infinite water source at a point

**Behavior:**
- Click empty floor/ground → water spawns continuously
- Water flows naturally via existing CA system
- Source persists until removed
- Visual indicator: bubbling/spring sprite

**Use cases:**
- Create artificial river on flat terrain
- Fill moat or reservoir
- Position water near workshops (mud mixer)

**Implementation:**
```c
// In HandleInputActions() or similar
if (action == ACTION_PLACE_WATER_SOURCE) {
    if (!HAS_FLOOR(cellX, cellY, cellZ)) {
        AddMessage("Water source requires floor", RED);
        return;
    }
    SetWaterSource(cellX, cellY, cellZ, true);
    SetWaterLevel(cellX, cellY, cellZ, WATER_MAX_LEVEL); // Start full
    AddMessage("Water source placed", BLUE);
}
```

### 2. Place Water Drain (Outflow)
**Key:** `D` (when in appropriate mode)
**Effect:** Sets `isDrain = true` at target cell
**Description:** Removes water that flows to this point

**Behavior:**
- Click ground → water that reaches this cell disappears
- Prevents flooding by giving water an exit
- Drain persists until removed
- Visual indicator: drain grate/whirlpool sprite

**Use cases:**
- Control flooding from springs
- Create stable water level (source + drain = equilibrium)
- Prevent workshop flooding

**Implementation:**
```c
if (action == ACTION_PLACE_WATER_DRAIN) {
    SetWaterDrain(cellX, cellY, cellZ, true);
    AddMessage("Water drain placed", DARKBLUE);
}
```

### 3. Remove Water Feature
**Key:** `X` or same key as placement (toggle)
**Effect:** Removes source/drain at target cell
**Description:** Clears water source or drain

**Behavior:**
- Click source → removes `isSource`, water stops spawning
- Click drain → removes `isDrain`, water no longer disappears here
- Water level remains (use evaporation or manual removal)

**Implementation:**
```c
if (action == ACTION_REMOVE_WATER_FEATURE) {
    if (IsWaterSourceAt(cellX, cellY, cellZ)) {
        SetWaterSource(cellX, cellY, cellZ, false);
        AddMessage("Water source removed", GRAY);
    } else if (IsWaterDrainAt(cellX, cellY, cellZ)) {
        SetWaterDrain(cellX, cellY, cellZ, false);
        AddMessage("Water drain removed", GRAY);
    }
}
```

### 4. Draw River/Channel (Paint Mode)
**Key:** `R` + drag (like draw wall/floor)
**Effect:** Carves channel and places water along path
**Description:** Creates flowing river by designating channel

**Two approaches:**

#### Approach A: Instant River (Simple)
- Drag to paint water directly
- Sets water level to max along path
- No terrain modification (water sits on existing ground)
- **Pro:** Immediate, simple
- **Con:** Water will flow away if ground not suitable

#### Approach B: Channel Designation (Realistic)
- Drag to designate "dig channel"
- Creates mining jobs: dig 1 z-level down along path
- Place water source at start, drain at end (optional)
- Water flows naturally once channel complete
- **Pro:** Realistic, uses existing job system
- **Con:** Takes time, requires movers

**Recommended: Hybrid Approach**
- `MODE_PLACE_WATER` has "Paint Water" (instant, for testing/creative)
- `MODE_WORK` → Designate → "Dig Channel" (realistic, for survival)

**Implementation (Paint Water):**
```c
// Similar to DrawWalls/DrawFloors pattern
if (action == ACTION_PAINT_WATER) {
    if (isMouseDown) {
        // Get cells along drag path (like DrawWalls)
        for (each cell in path) {
            if (CellIsWalkable(x, y, z)) {
                SetWaterLevel(x, y, z, WATER_MAX_LEVEL);
            }
        }
    }
}
```

**Implementation (Dig Channel):**
```c
// Create new designation type
typedef enum {
    // ... existing types
    DESIGNATION_DIG_CHANNEL,  // Dig 1 z-level down
} DesignationType;

// In designation handler
if (action == ACTION_DESIGNATE_CHANNEL) {
    for (each cell in path) {
        if (IsNaturalGround(x, y, z)) {
            AddDesignation(DESIGNATION_DIG_CHANNEL, x, y, z);
        }
    }
}

// In job system (similar to mining)
void CompleteDig Channel(Job* job) {
    int x = job->x, y = job->y, z = job->z;

    // Remove cell above (like channeling in DF)
    SetCell(x, y, z + 1, CELL_AIR);

    // Result: 1 z-level lower terrain (channel bed)
    // Water will naturally flow into depression
}
```

---

## Input Mode Organization

### Option 1: New Water Mode
```
MODE_PLACE_WATER
  ├─ S: Place Water Source
  ├─ D: Place Water Drain
  ├─ R: Paint Water (instant)
  └─ X: Remove Feature
```

### Option 2: Extend Existing Modes
**MODE_WORK → Designate submenu:**
```
Designate
  ├─ ... existing (mine, chop, plant)
  ├─ C: Dig Channel (new)
  └─ S: Place Spring (creative mode only?)
```

**Recommended:** Option 1 (dedicated water mode) for clarity, especially if adding more water features later (pumps, aqueducts, waterwheels).

---

## Visual Feedback

### Water Source Sprite
- Bubbling spring animation (2-3 frames)
- Blue glow or sparkle effect
- Renders above water surface

### Water Drain Sprite
- Whirlpool/vortex animation
- Drain grate icon
- Water visibly flowing toward drain

### Channel Designation
- Dashed outline along designated path (like blueprint preview)
- Shows "DIG" icon at each cell
- Clears when job completes

### Tooltip Information
When hovering over water:
```
Water (Level 45/100)
- Source: Yes (infinite)
- Drain: No
- Flow direction: East
```

---

## Terrain Interaction

### Water on Different Surfaces

**Natural ground (CELL_WALL, natural):**
- Water sits on top (z-level above)
- Can designate "dig channel" to lower terrain

**Constructed floor (HAS_FLOOR):**
- Water sits on floor surface
- Cannot dig through (would destroy floor)

**Air cells:**
- Water falls down (existing pressure/fall behavior)

### Channel Depth Options (Future)

Start simple: channels are 1 z-level deep.

Later, could add depth parameter:
- Shallow channel (1 z-level): streams, irrigation
- Deep channel (2-3 z): rivers, moats
- Trench (4+ z): defensive moats, reservoirs

---

## Integration with Water-Dependent Workshops

### Mud Mixer Placement
With water placement tools:
1. Player places water source where they want river
2. Water flows and settles naturally
3. Mud mixer can be placed adjacent to stable water

**Alternative workflow:**
1. Player places mud mixer on dry ground
2. Game shows error: "Requires water access"
3. Player places water source next to mixer
4. Mixer becomes functional

### Spring Reliability
Water sources are **infinite** - they don't dry up. This means:
- Mud mixer never runs out of water (once placed correctly)
- Players can safely plan workshops around artificial springs
- No need for complex water table/aquifer system (yet)

---

## Gameplay Considerations

### Early Game
- Water sources let players create localized water without large rivers
- Useful for small mud-brick operations
- Don't need to find natural river biome

### Mid Game
- Dig channels to redirect natural rivers
- Create moat around base (source at one end, drain at other)
- Irrigate crop fields (future farming)

### Late Game (Future)
- Pump systems move water uphill
- Aqueducts carry water long distances
- Waterwheels for power (future mechanical system)

### Creative vs Survival Balance
**Creative mode:**
- Instant water painting (instant gratification)
- Free springs/drains (no material cost)

**Survival mode:**
- Channel designation requires mover labor
- Springs might require ITEM_CLAY or ITEM_BLOCKS to "seal" (material cost)
- Drains might require ITEM_GRAVEL as filter material

**Recommendation:** Start with creative-style placement (no cost) to enable water-dependent workshops. Add material costs later if needed for balance.

---

## Implementation Phases

### Phase 1: Basic Placement (Minimal Viable)
1. Add `ACTION_PLACE_WATER_SOURCE` and `ACTION_PLACE_WATER_DRAIN` to action enum
2. Add keybinds in new `MODE_PLACE_WATER` (or extend existing mode)
3. Implement click handlers that call `SetWaterSource()` / `SetWaterDrain()`
4. Add tooltip display for source/drain status
5. **Test:** Place spring, watch water flow; place drain, watch water disappear

**Estimated time:** 2-3 hours

### Phase 2: Visual Feedback
1. Add spring sprite (bubbling animation)
2. Add drain sprite (whirlpool/grate)
3. Render sprites above water surface
4. Add tooltip info (source/drain status, water level)

**Estimated time:** 1-2 hours

### Phase 3: Paint Water (Creative Tool)
1. Add `ACTION_PAINT_WATER` with drag-to-paint
2. Similar to draw walls/floors pattern
3. Sets water level along path
4. **Test:** Drag across terrain, creates river-like water

**Estimated time:** 2-3 hours

### Phase 4: Dig Channel Designation (Survival Tool)
1. Add `DESIGNATION_DIG_CHANNEL` designation type
2. Add `JOBTYPE_DIG_CHANNEL` job type
3. Implement terrain removal (1 z-level down)
4. Visual: dashed designation outline
5. **Test:** Designate channel path, movers dig it out, water flows into channel

**Estimated time:** 4-6 hours

### Phase 5: Polish
1. Remove water feature action (undo placement)
2. Better error messages (can't place source on wall, etc.)
3. Prevent accidental placement on top of workshops
4. UI for choosing source/drain when in water mode

**Estimated time:** 2-3 hours

**Total estimated time:** 11-17 hours for full implementation

---

## Testing Scenarios

### Scenario 1: Simple Spring
1. Place spring on flat ground
2. Water should spread outward until it reaches edges or equilibrium
3. Verify water persists across save/load

### Scenario 2: Source + Drain Loop
1. Place spring at point A
2. Place drain at point B (lower z-level or same)
3. Water should flow from A to B continuously
4. Water level stabilizes (doesn't flood)

### Scenario 3: Mud Mixer Integration
1. Create artificial river (spring + channel)
2. Place mud mixer on riverbed
3. Verify mixer shows "has water access"
4. Craft mud (DIRT + CLAY → MUD)
5. Verify recipe completes successfully

### Scenario 4: Channel Digging
1. Designate channel across flat terrain
2. Movers dig channel (terrain drops 1 z)
3. Place spring at channel start
4. Water flows along channel path naturally

### Scenario 5: Flood Control
1. Place spring (water starts spreading)
2. Water floods workshop area
3. Place drain to stop flood
4. Water level stabilizes

---

## Future Extensions

### Advanced Features (Later)

**Aqueduct Tiles:**
- Elevated water channels (bridges for water)
- Water flows through constructed ducts
- Enables long-distance water transport

**Water Pumps:**
- Moves water uphill (against gravity)
- Requires power source (mover labor, wind, waterwheel)
- Enables hilltop workshops with water access

**Irrigation Channels:**
- Shallow channels for farming
- Water slowly flows to crop fields
- Integrates with future farming system

**Water Quality:**
- Fresh vs stagnant water
- Springs produce fresh water
- Standing water becomes stagnant over time
- Affects mud quality, health, etc.

**Wells:**
- Dig deep, hit water table
- Produces water source at depth
- More realistic than surface springs

---

## Code Files to Modify

**Actions:**
- `src/core/input_mode.h` - add MODE_PLACE_WATER (or extend existing)
- `src/core/action_registry.c` - add ACTION_PLACE_WATER_SOURCE, etc.

**Input handlers:**
- `src/core/input.c` - add click handlers for water placement

**Visual:**
- `src/render/rendering.c` - render spring/drain sprites
- `src/render/tooltips.c` - show source/drain status

**Simulation:**
- No changes needed - `SetWaterSource()` / `SetWaterDrain()` already exist!

**Save/Load:**
- Water grid already saved (`waterGrid[z][y][x].isSource`, `.isDrain`)
- No changes needed

**Designations (Phase 4):**
- `src/world/designations.h` - add DESIGNATION_DIG_CHANNEL
- `src/entities/jobs.c` - add JOBTYPE_DIG_CHANNEL handler

---

## Open Questions

1. **Should springs have material cost?** (CLAY to seal spring, or free?)
2. **Should drains require maintenance?** (get clogged, need clearing?)
3. **Spring flow rate:** All infinite, or adjustable (weak spring vs strong)?
4. **Channel designation:** Always 1 z-level deep, or depth parameter?
5. **Visual clarity:** How to distinguish source/drain at a glance?
6. **Multi-z springs:** Can spring be placed at z > 0? (elevated water source)
7. **Remove vs toggle:** X key removes feature, or same key toggles on/off?

---

## Summary

**What this enables:**
- Players can create water wherever needed (mud mixer, clay pit, moats)
- No dependency on terrain generator placing rivers correctly
- Base planning: position workshops, then add water as needed
- Creative control: shape rivers, create fountains, design irrigation

**What this unlocks:**
- Water-dependent workshops (mud mixer, clay pit)
- Moats and defensive water features
- Future irrigation for farming
- Aesthetic water features (fountains, ponds)

**Recommended first implementation:**
- Phase 1 (basic placement) + Phase 2 (visual feedback) = ~5 hours
- Gets mud mixer workshop unblocked immediately
- Can add dig channel designation later for survival mode

**Key insight:** By giving players direct water placement control, we remove the terrain generator as a bottleneck. Players can create mud/cob workshops anywhere, not just where rivers happen to spawn.
