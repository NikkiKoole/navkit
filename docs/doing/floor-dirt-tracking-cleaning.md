# Floor Dirt Tracking & Cleaning System

## Overview

Movers walking from natural terrain (dirt, mud, grass) onto constructed floors track dirt inside, creating a maintenance burden. Floors accumulate dirt over time and require Cleaner jobs to maintain. This creates emergent gameplay around entrance design, traffic flow, and maintenance labor.

---

## Vision (From entropy.md)

```
Floor (clean) → [movers walk from dirt] → Floor (dirty/tracked) → [cleaner mops] → Floor (clean)
```

**Gameplay Loop:**
- Dirt tracks onto floors when movers walk from outdoor terrain to indoor floors
- Mud when raining (wet dirt tracks worse than dry dirt)
- Cleaners have endless work if you don't put paths in high-traffic areas
- Lazy player = dirty base, movers tracking mud inside
- Invested player = clean paths, cleaner jobs, tidy workshops

**Strategic Tension:**
- Stone paths at entrances = no dirt tracking but costs materials
- Dirt/grass paths = free but requires cleaning maintenance
- Roads = stone floors = never get dirty, but expensive
- Grass = free but degrades and tracks dirt inside

---

## Existing Infrastructure

### Ground Wear System (src/simulation/groundwear.c)
✅ Already exists - handles grass trampling on natural terrain
- `wearGrid[z][y][x]` tracks wear on natural dirt tiles
- Grass → trampled → bare dirt with traffic
- Natural recovery over time
- **Does NOT handle built floors**

### Terrain Wetness Flags (src/world/grid.h)
✅ Already defined but **completely unused**
- `GET_CELL_WETNESS(x,y,z)` - returns 0-3 (dry, damp, wet, soaked)
- `SET_CELL_WETNESS(x,y,z,w)` - sets wetness level
- Packed into `cellFlags[z][y][x]` bits 1-2
- **Ready to use for mud tracking integration**

### Water System (src/simulation/water.c)
✅ Fully functional water simulation
- Water grid with 0-7 water levels per cell
- Flow, evaporation, freezing all working
- **Not connected to wetness flags** - that's the missing link

---

## Feature Requirements

### Phase 1: Basic Floor Dirt Tracking (Minimal Viable)

**Goal:** Movers track dirt from natural terrain onto built floors. Floors visually show dirt accumulation.

#### 1.1 Dirt Grid for Floors
- New grid: `int floorDirtGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH]`
- Parallel to main grid, tracks dirt level on **built floors only**
- Range: 0 (clean) to `FLOOR_DIRT_MAX` (very dirty)
- Similar pattern to `wearGrid` in groundwear.c

#### 1.2 Dirt Tracking Logic
When a mover takes a step, check:
1. **Previous cell**: Was it natural terrain (dirt, grass, mud)?
2. **Current cell**: Is it a built floor (HAS_FLOOR flag)?
3. **If yes to both**: Add dirt to `floorDirtGrid[current_z][current_y][current_x]`

**Dirt Transfer Amount:**
- Base: `DIRT_TRACK_AMOUNT = 1` (configurable)
- Modifiers:
  - Source is wet/muddy: multiply by 2-3x
  - Source is grass: multiply by 0.5x (less dirt than bare dirt)
  - Source is trampled/bare: multiply by 1x (normal)

**Dirt Sources** (natural terrain types):
- CELL_WALL with MAT_DIRT (natural dirt)
- CELL_WALL with MAT_CLAY/SAND/GRAVEL/PEAT (other soils)
- Any cell with grass vegetation (VEG_GRASS_*)
- Any cell with wetness > 0 (muddy terrain)

**Dirt Targets** (built floors only):
- Any cell with `HAS_FLOOR` flag set
- Ignores natural dirt (that uses groundwear system)

#### 1.3 Visual Rendering
- Add dirt overlay sprite (brown/muddy texture)
- Render on top of floor sprite, alpha based on dirt level
- Dirt levels:
  - 0-25%: Clean (no overlay)
  - 25-50%: Light dirt (faint overlay)
  - 50-75%: Dirty (visible overlay)
  - 75-100%: Very dirty (heavy overlay)

#### 1.4 Constants & Configuration
```c
// Floor dirt thresholds
#define FLOOR_DIRT_MAX 1000              // Maximum dirt accumulation
#define FLOOR_DIRT_TRACK_AMOUNT 1        // Base dirt added per step
#define FLOOR_DIRT_LIGHT_THRESHOLD 250   // 25% dirty
#define FLOOR_DIRT_MEDIUM_THRESHOLD 500  // 50% dirty
#define FLOOR_DIRT_HEAVY_THRESHOLD 750   // 75% dirty

// Dirt tracking modifiers
#define DIRT_MULTIPLIER_WET 2.0f         // Wet/muddy terrain tracks 2x dirt
#define DIRT_MULTIPLIER_GRASS 0.5f       // Grass tracks 0.5x dirt
#define DIRT_MULTIPLIER_BARE 1.0f        // Bare dirt tracks 1x dirt
```

---

### Phase 2: Wetness Integration & Mud Tracking

**Goal:** Connect water system to wetness flags. Wet dirt = mud. Mud tracks worse than dry dirt.

#### 2.1 Water → Wetness Connection
In `UpdateWater()` (water.c):
- After water simulation, update wetness flags based on water level
- Water level 0 → wetness 0 (dry)
- Water level 1-2 → wetness 1 (damp)
- Water level 3-4 → wetness 2 (wet)
- Water level 5-7 → wetness 3 (soaked)

#### 2.2 Wetness → Drying Over Time
In new function `UpdateWetness()`:
- Wetness decays over time when no water present
- Decay rate based on temperature (future integration)
- Faster decay in sunlight/heat, slower in shade/cold
- Pattern similar to `wearRecoveryAccum` in groundwear.c

#### 2.3 Mud Tracking Multiplier
When tracking dirt from terrain to floor:
```c
int wetness = GET_CELL_WETNESS(source_x, source_y, source_z);
float wetMult = 1.0f + (wetness * 0.5f);  // 1x, 1.5x, 2x, 2.5x
int dirtToAdd = FLOOR_DIRT_TRACK_AMOUNT * wetMult * terrainMult;
```

#### 2.4 Mud Slows Movement (Bonus)
Already have water speed penalties. Extend to mud:
- Wetness 2+ on dirt/grass = muddy terrain
- Apply movement speed penalty (similar to shallow water)

---

### Phase 3: Cleaner Job

**Goal:** Add Cleaner job that finds dirty floors and cleans them.

#### 3.1 Job Type
```c
typedef enum {
    // ... existing job types
    JOB_CLEAN_FLOOR,
} JobType;
```

#### 3.2 Job Assignment (AssignJobs)
Priority: **Low** (after hauling, crafting, but still maintenance)
- Scan all floors for dirt > `FLOOR_DIRT_CLEAN_THRESHOLD` (e.g., 200)
- Prioritize highest dirt levels first
- Assign Cleaner job to idle mover

#### 3.3 Cleaner Job Steps
```c
typedef enum {
    CLEAN_STEP_WALK_TO_FLOOR,   // Walk to dirty floor tile
    CLEAN_STEP_CLEANING,        // Stand and clean (animation/timer)
    CLEAN_STEP_DONE,            // Finish job
} CleanStep;
```

**Cleaning Logic:**
1. Walk to target floor tile
2. Stand and clean for `CLEAN_DURATION` seconds (e.g., 2-3 seconds)
3. Reduce `floorDirtGrid[z][y][x]` by `FLOOR_DIRT_CLEAN_AMOUNT` (e.g., -500)
4. If dirt > 0 after cleaning, may require multiple cleanings
5. Job completes when dirt reaches 0

#### 3.4 Cleaner Tool/Item (Optional Future)
- ITEM_BROOM or ITEM_MOP required for cleaning job
- Cleaner must fetch tool before cleaning
- Tool has durability, needs replacement/repair

#### 3.5 Constants
```c
#define FLOOR_DIRT_CLEAN_THRESHOLD 200   // Minimum dirt to trigger cleaning job
#define FLOOR_DIRT_CLEAN_AMOUNT 500      // Dirt removed per cleaning action
#define CLEAN_DURATION 3.0f              // Seconds to clean one tile
```

---

### Phase 4: Natural Dirt Decay (Optional)

**Goal:** Floors slowly get less dirty over time (dust settles, evaporates).

#### 4.1 Passive Decay
Similar to `UpdateGroundWear()` pattern:
- Interval-based decay (every 5-10 game seconds)
- Reduce `floorDirtGrid[z][y][x]` by small amount
- Only when no recent traffic (track last trample time?)

#### 4.2 Configuration
```c
#define FLOOR_DIRT_DECAY_RATE 5          // Dirt removed per decay interval
#define FLOOR_DIRT_DECAY_INTERVAL 10.0f  // Game seconds between decay
```

**Balancing:**
- High traffic areas stay dirty (tracking > decay)
- Low traffic areas slowly self-clean (decay > tracking)
- Still faster to have Cleaner than wait for natural decay

---

## Implementation Plan

### Files to Create/Modify

#### New Files:
- `src/simulation/floordirt.h` - Header for floor dirt system
- `src/simulation/floordirt.c` - Implementation
- `tests/test_floordirt.c` - Unit tests

#### Modified Files:
- `src/entities/mover.c` - Call `TrackDirtFromStep()` in mover update
- `src/entities/jobs.h` - Add `JOB_CLEAN_FLOOR` enum
- `src/entities/jobs.c` - Add cleaner job logic
- `src/render/rendering.c` - Render dirt overlay on floors
- `src/core/saveload.c` - Save/load floorDirtGrid (bump SAVE_VERSION)
- `src/core/inspect.c` - Save migration for floorDirtGrid
- `src/simulation/water.c` - Connect water levels to wetness flags (Phase 2)
- `src/unity.c` - Include new floordirt.c file

---

## API Design

### floordirt.h
```c
#ifndef FLOORDIRT_H
#define FLOORDIRT_H

#include <stdbool.h>
#include "../world/grid.h"

// Floor dirt thresholds (scaled 10x for precision)
#define FLOOR_DIRT_MAX 1000
#define FLOOR_DIRT_TRACK_AMOUNT 1
#define FLOOR_DIRT_LIGHT_THRESHOLD 250
#define FLOOR_DIRT_MEDIUM_THRESHOLD 500
#define FLOOR_DIRT_HEAVY_THRESHOLD 750
#define FLOOR_DIRT_CLEAN_THRESHOLD 200
#define FLOOR_DIRT_CLEAN_AMOUNT 500
#define CLEAN_DURATION 3.0f

// Dirt tracking multipliers
#define DIRT_MULTIPLIER_WET 2.0f
#define DIRT_MULTIPLIER_GRASS 0.5f
#define DIRT_MULTIPLIER_BARE 1.0f

// Floor dirt grid (parallel to main grid)
extern int floorDirtGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
extern bool floorDirtEnabled;

// Initialize floor dirt system
void InitFloorDirt(void);

// Clear all dirt values
void ClearFloorDirt(void);

// Called when a mover steps from terrain onto floor
// Checks previous/current position and tracks dirt if applicable
void TrackDirtFromStep(int prevX, int prevY, int prevZ, int currX, int currY, int currZ);

// Update dirt decay (call from main tick) - Phase 4
void UpdateFloorDirt(void);

// Get current dirt value at position
int GetFloorDirt(int x, int y, int z);

// Clean floor at position (called by Cleaner job)
void CleanFloor(int x, int y, int z, int amount);

// Check if cell is a dirt source (natural terrain)
bool IsDirtSource(int x, int y, int z);

// Check if cell is a dirt target (built floor)
bool IsDirtTarget(int x, int y, int z);

// Calculate dirt tracking multiplier for source terrain
float GetDirtTrackingMultiplier(int x, int y, int z);

#endif // FLOORDIRT_H
```

---

## Testing Strategy

### Unit Tests (test_floordirt.c)

#### Basic Tests:
- `should_initialize_dirt_grid_with_zeros`
- `should_clear_all_dirt_when_ClearFloorDirt_called`
- `should_track_dirt_from_dirt_to_floor`
- `should_not_track_dirt_from_floor_to_floor`
- `should_not_track_dirt_from_air_to_floor`
- `should_accumulate_dirt_over_multiple_steps`
- `should_cap_dirt_at_FLOOR_DIRT_MAX`

#### Dirt Source Detection:
- `should_detect_natural_dirt_as_dirt_source`
- `should_detect_grass_as_dirt_source`
- `should_detect_soil_types_as_dirt_source` (clay, sand, gravel, peat)
- `should_not_detect_stone_as_dirt_source`
- `should_not_detect_air_as_dirt_source`
- `should_not_detect_built_floor_as_dirt_source`

#### Wetness Integration (Phase 2):
- `should_track_more_dirt_from_wet_terrain`
- `should_track_normal_dirt_from_dry_terrain`
- `should_track_most_dirt_from_soaked_terrain`

#### Multiplier Tests:
- `should_apply_grass_multiplier_correctly` (0.5x)
- `should_apply_bare_dirt_multiplier_correctly` (1x)
- `should_apply_wet_multiplier_correctly` (2x+)

#### Cleaning Tests:
- `should_reduce_dirt_when_CleanFloor_called`
- `should_clamp_dirt_to_zero_when_overcleaning`
- `should_clean_specific_z_level_only`

#### Edge Cases:
- `should_handle_out_of_bounds_gracefully`
- `should_work_at_all_z_levels`
- `should_not_track_when_disabled`

---

## Integration Points

### Mover Update (src/entities/mover.c)
```c
void UpdateMover(Mover* m) {
    // ... existing movement code ...
    
    // Track previous position
    int prevX = (int)m->prevX;
    int prevY = (int)m->prevY;
    int prevZ = (int)m->prevZ;
    
    // Current position after movement
    int currX = (int)m->x;
    int currY = (int)m->y;
    int currZ = (int)m->z;
    
    // Track dirt if moved to new cell
    if (prevX != currX || prevY != currY || prevZ != currZ) {
        TrackDirtFromStep(prevX, prevY, prevZ, currX, currY, currZ);
    }
    
    // ... rest of update ...
}
```

### Job Assignment (src/entities/jobs.c)
```c
void AssignJobs(void) {
    // ... existing priorities ...
    
    // Priority: Cleaning (after hauling, before luxury tasks)
    AssignCleaningJobs();
    
    // ... rest of assignments ...
}

static void AssignCleaningJobs(void) {
    if (!floorDirtEnabled) return;
    
    // Find idle mover
    int moverIdx = FindIdleMover();
    if (moverIdx < 0) return;
    
    // Find dirtiest floor above threshold
    int maxDirt = FLOOR_DIRT_CLEAN_THRESHOLD;
    int targetX = -1, targetY = -1, targetZ = -1;
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (!HAS_FLOOR(x, y, z)) continue;
                int dirt = GetFloorDirt(x, y, z);
                if (dirt > maxDirt) {
                    maxDirt = dirt;
                    targetX = x; targetY = y; targetZ = z;
                }
            }
        }
    }
    
    if (targetX < 0) return;  // No dirty floors found
    
    // Create cleaning job
    int jobIdx = CreateJob(JOB_CLEAN_FLOOR, targetX, targetY, targetZ);
    if (jobIdx >= 0) {
        AssignJobToMover(jobIdx, moverIdx);
    }
}
```

### Rendering (src/render/rendering.c)
```c
// In floor rendering loop:
if (HAS_FLOOR(x, y, z)) {
    DrawFloorSprite(x, y, z);  // Existing floor rendering
    
    // Draw dirt overlay if dirty
    int dirt = GetFloorDirt(x, y, z);
    if (dirt > FLOOR_DIRT_LIGHT_THRESHOLD) {
        float alpha = (float)dirt / FLOOR_DIRT_MAX;
        DrawDirtOverlay(x, y, z, alpha);
    }
}
```

---

## Balancing Considerations

### Dirt Accumulation Rate
- Too fast: Cleaners overwhelmed, floors always dirty
- Too slow: Cleaning feels pointless, no urgency
- **Target**: High-traffic areas (10+ movers/day) should need cleaning every 1-2 game days

### Cleaning Efficiency
- Too slow: Cleaners can't keep up with traffic
- Too fast: No reason to optimize entrance layout
- **Target**: One cleaner should handle ~50 tiles of moderate traffic

### Stone Path Cost vs Cleaning Labor
- Stone floors should be worth the material cost
- Dirt paths + cleaner labor should be viable early game
- Late game: transition to stone paths as materials become abundant

### Visual Feedback
- Dirt should be **noticeable** but not ugly
- Clean base should feel satisfying
- Dirty base should feel neglected (negative mood effect?)

---

## Future Enhancements

### Mood Effects
- Clean floors: +1 mood bonus when nearby
- Dirty floors: -1 mood penalty when nearby
- Very dirty floors: -3 mood penalty, "disgusted" thought

### Room Cleanliness
- Track average dirt level per room
- "Clean Room" need for movers
- Dirty bedrooms = bad sleep quality

### Disease/Pests
- Very dirty floors (75%+) attract pests (rats, insects)
- Dirty kitchens = food poisoning risk
- Dirty hospitals = infection risk

### Advanced Cleaning Tools
- ITEM_BROOM: Basic cleaning, slow
- ITEM_MOP: Better cleaning, requires water bucket
- ITEM_VACUUM: Best cleaning (late-game tech)

### Cleaning Zones
- Designate "Keep Clean" zones
- Cleaners prioritize these zones
- Higher cleanliness threshold for designated areas

### Footwear
- Movers with boots track less dirt
- "Wipe feet" behavior at entrances (future AI)

---

## Open Questions

1. **Should dirt decay naturally?** 
   - Pro: Reduces cleaner burden for low-traffic areas
   - Con: Reduces strategic tension, makes cleaning less important
   - **Decision**: Yes, but very slow (Phase 4). Natural decay = 5-10x slower than cleaning.

2. **Should stone floors never get dirty?**
   - Pro: Clear strategic value, matches real life
   - Con: Removes cleaning gameplay for wealthy colonies
   - **Decision**: Stone floors get dirty, but 50% slower accumulation. Still need occasional cleaning.

3. **Should wetness flags dry over time or only when water leaves?**
   - Pro (time-based): More realistic evaporation
   - Con (time-based): Requires temperature system integration
   - **Decision**: Phase 2 starts simple (water level → wetness). Phase 2.5 adds time-based drying with temperature.

4. **Should movers track dirt between floors?**
   - Example: Dirty workshop floor → clean bedroom floor
   - Pro: More realistic, creates cleaning chains
   - Con: More complex tracking, may be too harsh
   - **Decision**: Start simple (only natural terrain → floor). Revisit if too easy.

5. **Should cleaning consume water?**
   - Pro: Ties into water system, adds resource cost
   - Con: More complex job logic (fetch water, then clean)
   - **Decision**: Phase 3 simple (no water). Phase 3.5 adds water requirement for mopping.

---

## Success Metrics

How do we know this feature is working well?

### Technical:
- ✅ Floors accumulate dirt proportional to traffic
- ✅ Wet terrain tracks more dirt than dry terrain
- ✅ Cleaner jobs successfully reduce dirt levels
- ✅ Visual overlay clearly shows dirt levels
- ✅ No performance impact (<1% CPU increase)
- ✅ All tests passing (50+ test cases)

### Gameplay:
- ✅ Players notice dirt accumulation in high-traffic areas
- ✅ Players build stone paths at entrances to reduce cleaning
- ✅ Cleaners provide value (visible improvement when working)
- ✅ Dirty floors create maintenance pressure (not overwhelming)
- ✅ Clean base feels rewarding, dirty base feels neglected

---

## References

- **Design Doc**: `docs/vision/entropy-and-simulation/entropy.md` - "Wear and Cleaning" section
- **Existing System**: `src/simulation/groundwear.c` - Pattern for wear grids and decay
- **Wetness Flags**: `src/world/grid.h:65-104` - Unused wetness system ready for integration
- **Water System**: `src/simulation/water.c` - Water simulation to drive wetness
- **Save Version**: Current = 33, will bump to 34 for floorDirtGrid
