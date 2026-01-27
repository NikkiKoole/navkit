# Temperature System

A per-cell temperature grid that other systems (fire, water, weather) can read from and write to.

## Overview

Temperature is a foundational system that enables:
- Cold rooms for storage (underground or ice-cooled)
- Warm rooms for comfort (heated by fire)
- Frozen water mechanics (ice blocks as items)
- Realistic heat transfer with insulation

## Temperature Scale (0-255)

| Range | Description | Effects |
|-------|-------------|---------|
| 0-30 | Deep freeze | Water frozen solid, pottery kilns won't work |
| 31-50 | Freezing | Water freezes |
| 51-100 | Cold | Cold storage range, food preserved |
| 101-150 | Comfortable | Ambient/neutral zone |
| 151-200 | Hot | Uncomfortable, faster evaporation |
| 201-230 | Very hot | Fire territory, can ignite flammables |
| 231-255 | Extreme | Pottery kiln range, lava |

**Key thresholds:**
- `TEMP_WATER_FREEZES`: 50 (water freezes at or below this)
- `TEMP_AMBIENT`: 128 (default surface temperature)
- `TEMP_FIRE_MIN`: 200 (minimum temperature fire produces)

## Data Structure

```c
// temperature.h
typedef struct {
    uint8_t current;    // current temperature (0-255)
    uint8_t stable : 1; // skip processing if stable
    // 7 bits spare for future use
} TempCell;

extern TempCell temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
```

## Ambient Temperature

- Surface ambient: 128 (tweakable: `ambientSurfaceTemp`)
- Underground decay: -5 per Z-level (tweakable: `ambientDepthDecay`)
- Formula: `ambient(z) = max(0, ambientSurfaceTemp - (z * ambientDepthDecay))`

Example with defaults:
| Z-level | Ambient Temp | Zone |
|---------|--------------|------|
| 0 (surface) | 128 | Comfortable |
| -5 | 103 | Cool |
| -10 | 78 | Cold storage |
| -15 | 53 | Near freezing |
| -20 | 28 | Deep freeze |

## Heat Transfer

### Gameplay-Friendly Model
- **Air/empty cells:** Heat flows freely (fast equalization within rooms)
- **Walls:** Act as barriers, slow/block heat transfer based on insulation tier

### 3-Tier Insulation System

| Tier | Material | Heat Transfer Rate | Description |
|------|----------|-------------------|-------------|
| 0 | Air/empty | 100% | Heat flows freely |
| 1 | Wood wall | 20% | Some insulation |
| 2 | Stone wall | 5% | Strong insulation |

Heat transfer formula:
```
transfer_amount = (temp_diff * transfer_rate * insulationMultiplier) / divisor
```

### Transfer Rules
- Transfer happens orthogonally (4 directions, not diagonal)
- Each tick, cells average with neighbors based on insulation
- Walls DO transfer heat, just very slowly (a stone wall next to fire will eventually warm the other side)
- Transfer rate is tweakable: `heatTransferSpeed`

### Temperature Decay
- Cells gradually move toward their ambient temperature
- Decay rate is tweakable: `tempDecayRate`
- Hot cells cool down, cold cells warm up (unless held by a source)

## Temperature Sources

| Source | Temperature | Behavior |
|--------|-------------|----------|
| Fire (active) | 200-255 based on level | Heats cell and neighbors |
| Fire source (torch) | 220 constant | Permanent heat source |
| Cooling source | 0-30 | For testing, permanent cold source |
| Ice block (item) | Cools surroundings | Melts based on surrounding temp |
| Lava (future) | 255 | Extreme heat source |

### Fire Integration
- Fire now heats the cell it occupies: `temp = max(temp, 200 + fireLevel * 7)`
- Fire spreads faster in hot areas (temperature affects spread chance)
- Fire spreads slower in cold areas

## Water + Temperature

### Freezing
- Water freezes when cell temperature <= `TEMP_WATER_FREEZES` (50)
- Only full water cells (level 7) can freeze
- Frozen water is stored as a flag on WaterCell: `isFrozen : 1`
- Frozen water blocks water flow (acts like a wall for water simulation)
- Frozen water is walkable (future: walking ON ice, not implemented yet)

### Ice Block Extraction
- Player can "harvest" frozen water to create an ice block item
- Harvesting removes the full 7/7 frozen water from the cell
- Ice block item spawns at that location

### Ice Block Item Behavior
- Ice block has internal temperature (starts at ~20)
- While existing, it cools surrounding cells (acts as cold source)
- Melting speed based on surrounding temperature:
  - In freezing temps: doesn't melt
  - In cold temps: melts very slowly
  - In warm temps: melts faster
  - In hot temps: melts quickly
- When fully melted: item despawns, water 7/7 placed at item location
- If item location is a wall: water placed in nearest valid cell (or lost)

### Evaporation (existing system)
- High temperature increases evaporation chance
- `evapChance = baseEvapChance * (temp / TEMP_AMBIENT)`

## Warm/Cold Rooms

Rooms emerge naturally from the system:

### Cold Room (Storage)
- Build underground (natural cold ambient)
- OR place ice blocks inside
- Insulated walls (stone) keep cold in
- Use case: food preservation (future)

### Warm Room (Comfort)
- Place fire source (torch) inside
- Insulated walls keep heat in
- Big rooms take longer to heat (emergent from cell count)
- Use case: livable spaces in cold environments

## UI Controls

### Temperature Placement (T key?)
- **T + left-drag**: Place heat source (for testing)
- **T + Shift + left-drag**: Place permanent heat source
- **T + right-drag**: Place cooling source (for testing)
- **T + Shift + right-drag**: Place permanent cooling source

### Temperature Overlay
- Toggle colored overlay showing temperature
- Blue (cold) -> White (neutral) -> Red (hot)
- Gradient based on 0-255 scale

### Tweakable Parameters
- `ambientSurfaceTemp`: Default 128
- `ambientDepthDecay`: Default 5 per Z-level
- `heatTransferSpeed`: How fast heat moves through air
- `tempDecayRate`: How fast temps return to ambient
- `insulationTier1`: Wood transfer rate (default 20%)
- `insulationTier2`: Stone transfer rate (default 5%)
- `waterFreezeTemp`: Default 50
- `iceMeltSpeed`: Base melt rate for ice blocks

## Performance

- Uses stability pattern like water/fire/smoke
- Cells marked stable skip processing
- `DestabilizeTemperature()` marks cell + neighbors unstable
- Update cap: `TEMP_MAX_UPDATES_PER_TICK` (tweakable)

## Files

- `src/simulation/temperature.h` - Data structure, constants, API
- `src/simulation/temperature.c` - Update logic, heat transfer, decay

---

## Required Art Assets

### New Tiles to Draw (5 total)

| Tile | Purpose | Notes |
|------|---------|-------|
| `ice_block` | Item sprite for harvested ice block | Distinct item, not overlay |
| `wood_wall` | Wall with tier 1 insulation | New CellType needed |
| `stone_wall` | Wall with tier 2 insulation | New CellType needed |
| `wood_floor` | Wooden floor variant | New CellType needed |
| `stone_floor` | Stone floor variant | New CellType needed |

### Existing Tiles (unchanged)

| Tile | Status |
|------|--------|
| `wall` | Keep as generic wall |
| `floor` | Keep as generic floor |

### Overlays (no tiles needed)

| Visual | Approach |
|--------|----------|
| Frozen water | Light whitish-blue tint overlay on existing water |
| Temperature | Blue→white→red gradient rectangle overlay (like fire/smoke) |
| Heat source marker | Bright center dot (like water source) |
| Cold source marker | Bright center dot (like water drain) |

---

## Test Cases

Tests use ASCII maps. Optional material map specifies wall types.

### Map Legend
**Walkability map:**
- `#` = Wall (uses material map or default stone)
- `.` = Floor/air
- `~` = Water (level 7)
- `*` = Fire source
- `C` = Cooling source
- `H` = Heat source

**Material map (optional, same dimensions):**
- `s` = Stone wall (tier 2 insulation)
- `w` = Wood wall (tier 1 insulation)
- `.` = Air/floor (tier 0)

If no material map provided, all `#` = stone (current default).

---

### Test 1: Basic Heat Spread in Open Air
```
// Walkability
.....
..H..
.....

// Expected: After N ticks, heat spreads outward in circular pattern
// Center hottest, edges approach ambient
```

### Test 2: Stone Wall Insulation
```
// Walkability       // Materials
.....                .....
.###.                .sss.
.#H#.                .sHs.
.###.                .sss.
.....                .....

// Expected: Heat mostly contained inside stone room
// Outside cells stay near ambient
// Wall cells warm slightly on inside, barely on outside
```

### Test 3: Wood vs Stone Insulation
```
// Walkability       // Materials
.......              .......
.##.##.              .ss.ww.
.#H.H#.              .sH.Hw.
.##.##.              .ss.ww.
.......              .......

// Expected: Wood room (right) leaks more heat than stone room (left)
// Measure temp at equivalent outside positions
```

### Test 4: Water Freezing
```
// Walkability
...
.~.
...

// Setup: Set cell temps to 40 (below freeze threshold)
// Expected: Water cell gains isFrozen flag
// Water stops flowing/spreading
```

### Test 5: Ice Block Melting
```
// Walkability
.....
..I..    (I = ice block item at temp 20)
.....

// Setup: Ambient temp = 128 (warm)
// Expected: 
// - Ice block cools surrounding cells
// - Ice block gradually warms
// - When ice block temp > threshold, it melts
// - Water 7/7 appears at ice block location
// - Ice block item despawns
```

### Test 6: Cold Storage Room Underground
```
// Walkability (Z = -15, ambient = 53)
#####
#...#
#...#
#####

// Expected: Room interior stays at ~53 (near freezing)
// Good for cold storage
```

### Test 7: Heated Room in Cold Environment
```
// Walkability (Z = -15, ambient = 53)
#####
#.*.#    (* = fire source, temp 220)
#...#
#####

// Expected: Room interior warms up over time
// Equilibrium somewhere between 53 and 220
// Stone walls keep heat in
```

### Test 8: Fire Heats Cells
```
// Walkability
...
.*.    (* = fire, level 7)
...

// Expected: Fire cell temp >= 200 + (7 * 7) = 249
// Adjacent cells heated by transfer
```

### Test 9: Large Room vs Small Room Heating Time
```
// Small room         // Large room
####                  ########
#*#                   #......#
####                  #...*..#
                      #......#
                      ########

// Expected: Small room reaches equilibrium faster than large room
// (emergent from cell count)
```

### Test 10: Temperature Decay to Ambient
```
// Walkability
...
.H.    (H = temporary heat burst to 200, then removed)
...

// Expected: After heat source removed, cell cools toward ambient (128)
// Decay rate controlled by tempDecayRate
```

### Test 11: Underground Ambient Gradient
```
// Vertical slice showing Z-levels
Z=0:  ... (ambient 128)
Z=-5: ... (ambient 103)
Z=-10:... (ambient 78)
Z=-15:... (ambient 53)
Z=-20:... (ambient 28)

// Expected: Each level naturally colder
// No heat sources needed for cold storage deep underground
```

### Test 12: Ice Block Prevents Melting in Cold
```
// Walkability (Z = -20, ambient = 28)
...
.I.    (I = ice block)
...

// Expected: Ice block does NOT melt (ambient below freeze temp)
// Ice block maintains low temperature indefinitely
```

---

## Implementation Priority

1. **Phase 1: Core Temperature Grid**
   - TempCell struct and grid
   - Ambient calculation based on depth
   - Basic decay toward ambient
   - Temperature overlay visualization

2. **Phase 2: Heat Transfer**
   - Air-to-air transfer (fast)
   - Wall insulation (3-tier system)
   - Stability optimization

3. **Phase 3: Fire Integration**
   - Fire heats cells
   - Fire sources as permanent heaters
   - Temperature affects fire spread

4. **Phase 4: Water/Ice Integration**
   - Water freezing based on temperature
   - Frozen water blocks flow
   - Ice block item (harvest, melt, spawn water)

5. **Phase 5: UI & Polish**
   - Heat/cooling source placement (T key)
   - Temperature overlay toggle
   - Tweakable parameters in UI panel
