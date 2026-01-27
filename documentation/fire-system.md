# Fire System

A simplified fire simulation with fuel consumption, spreading, and smoke.

## Controls

- **F + left-drag** - Place fire (level 7)
- **F + right-drag** - Remove fire (extinguish)
- **F + Shift + left-drag** - Place permanent fire source (torch/lava, always burns)
- **F + Shift + right-drag** - Remove fire source

Note: T key now handles tile painting (previously F).

## Fire Levels

Fire uses a 1-7 scale (like water):
- **0** - No fire
- **1-2** - Embers (low intensity, orange)
- **3-4** - Medium fire (orange-red)
- **5-7** - Intense fire (bright red/yellow)

Fire is **not walkable** - movers will path around it.

## Fuel

Each cell type has a built-in fuel amount. When fuel reaches 0, the fire dies.

| Cell Type | Fuel | Burn Time |
|-----------|------|-----------|
| Grass | 2-3 | Short |
| Wood floor | 5-6 | Medium |
| Wood wall | 7 | Long |
| Stone/wall | 0 | Won't burn |

A cell with fuel = 0 is not flammable.

### Burned Cells

When a cell's fuel is exhausted:
- Cell gets a `burned` flag via `cellFlags` (terrain property, not fire system)
- Renders with a darker/charred tint
- Cannot catch fire again (no fuel left)

**Implementation:** Burned state lives in a shared `cellFlags` grid in `grid.h`:

```c
// grid.h
uint8_t cellFlags[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Bit layout:
// Bit 0:    CELL_FLAG_BURNED
// Bits 1-2: CELL_WETNESS (0=dry, 1=damp, 2=wet, 3=soaked)
// Bits 3-7: reserved for future flags

#define CELL_FLAG_BURNED    (1 << 0)
#define CELL_WETNESS_MASK   (3 << 1)
#define CELL_WETNESS_SHIFT  1

#define HAS_FLAG(x,y,z,f)     (cellFlags[z][y][x] & (f))
#define SET_FLAG(x,y,z,f)     (cellFlags[z][y][x] |= (f))
#define CLEAR_FLAG(x,y,z,f)   (cellFlags[z][y][x] &= ~(f))
#define GET_WETNESS(x,y,z)    ((cellFlags[z][y][x] & CELL_WETNESS_MASK) >> CELL_WETNESS_SHIFT)
#define SET_WETNESS(x,y,z,w)  (cellFlags[z][y][x] = (cellFlags[z][y][x] & ~CELL_WETNESS_MASK) | ((w) << CELL_WETNESS_SHIFT))
```

This keeps terrain state separate from the fire simulation and allows future flags (frozen, corrupted, etc.) to share the same byte.

## Fire Behavior

Note: Unlike water, fire has no "drain" equivalent. Fire is extinguished by water or fuel exhaustion, not by dedicated drain cells.

### Fuel Consumption
Each tick, fire consumes fuel from the cell. Rate is tweakable via UI.

### Spreading
Fire spreads to adjacent flammable cells (orthogonal: N/S/E/W).

- Spread chance based on fire intensity and neighbor's fuel
- Spread speed is tweakable via UI
- Water-adjacent cells have reduced spread chance (tweakable factor)

### Water Interaction
- Water extinguishes fire immediately
- Fire cannot spread across water
- Adjacent water slows spread to nearby cells

## Smoke

Smoke is a separate grid system with its own data structure:

```c
// smoke.h
typedef struct {
    uint8_t level         : 3;  // 0-7 density
    uint8_t stable        : 1;
    uint8_t hasPressure   : 1;  // trapped smoke that wants to escape
    uint8_t pressureSourceZ : 3;  // z-level where smoke originated (can fill down to here)
} SmokeCell;

SmokeCell smokeGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
```

### Smoke Levels
- **0** - No smoke
- **1-7** - Density (affects visual opacity)

### Smoke Behavior (Inverse Fluid Model)

Smoke behaves like an inverted fluid - it wants to rise, but accumulates and fills downward when trapped.

**Processing order:** Top-to-bottom (opposite of water/fire)

**Phases (in priority order):**

1. **Rising** (priority 1) - Smoke moves up to z+1 if there's space
2. **Horizontal spread** (priority 2) - If can't rise (ceiling/wall), spread to orthogonal neighbors at same z
3. **Filling down** (priority 3) - If ceiling and horizontal are full/blocked, push smoke down to z-1
4. **Dissipation** - Smoke level decreases over time, faster in open air, slower when trapped

**Generation:** Fire produces smoke (higher fire level = more smoke)

**Pressure tracking:** Similar to water, smoke needs to track density/pressure:

```c
typedef struct {
    uint8_t level       : 3;  // 0-7 density
    uint8_t stable      : 1;
    uint8_t hasPressure : 1;  // trapped smoke that wants to escape
    uint8_t pressureSourceZ : 3;  // z-level where smoke originated (can fill down to here)
} SmokeCell;
```

**Closed room example:**
1. Fire at z=0 generates smoke
2. Smoke rises to z=1, z=2, z=3 (ceiling)
3. Ceiling blocks it, smoke spreads horizontally at z=3
4. z=3 fills up, smoke pressure-fills z=2
5. Eventually the whole room fills with smoke from top to bottom

**Chimney/ventilation:**
- Opening a hole in the ceiling releases pressure
- Smoke escapes upward, room gradually clears
- Smoke prefers rising over filling down, so vents work naturally

### Smoke Rendering
Semi-transparent gray overlay. Opacity based on smoke level.

### Smoke After Fire
When fire is extinguished (by water or fuel exhaustion), existing smoke lingers and dissipates naturally.

## Rendering

### Fire
Orange/red colored overlay on cell, same style as water blue overlay.
- Low intensity (1-2): darker orange
- Medium (3-4): orange-red
- High (5-7): bright red/yellow, slight flicker

### Smoke
Gray semi-transparent overlay. Higher smoke level = more opaque.

## Tweakable Values (UI Dragfloats)

| Parameter | Description | Default |
|-----------|-------------|---------|
| fireSpreadSpeed | Ticks between spread attempts | TBD |
| fuelConsumptionRate | Fuel consumed per tick | TBD |
| waterSpreadReduction | Spread chance multiplier near water (0-1) | TBD |
| smokeRiseSpeed | How fast smoke moves up | TBD |
| smokeDissipationRate | Smoke level decrease per tick | TBD |
| smokeGenerationRate | Smoke produced per fire level | TBD |

## Test Scenarios

### Test 1: Basic Burning
1. Place some grass cells
2. Start a fire (F + left-drag) on one cell
3. Watch it burn and consume fuel

**Expected:** Fire burns, intensity may fluctuate, fire dies when fuel exhausted. Cell shows burned tint.

### Test 2: Spreading
1. Create a patch of grass or wood floor
2. Start fire on one edge

**Expected:** Fire spreads to adjacent flammable cells. Spread speed matches tweakable value.

### Test 3: Smoke Rising
1. Start a fire at z=0
2. Go to z=1 (press `.`)

**Expected:** Smoke visible rising from fire below, spreading horizontally, gradually dissipating.

### Test 4: Water Extinguishing
1. Create fire
2. Place water on the fire (W + left-drag)

**Expected:** Fire immediately extinguished. Smoke lingers briefly then dissipates.

### Test 5: Water Barrier
1. Create a line of water
2. Place flammable cells on both sides
3. Start fire on one side

**Expected:** Fire spreads on its side but cannot cross water. Spread to cells adjacent to water is slower.

### Test 6: Non-Flammable Cells
1. Place stone walls
2. Try to ignite them

**Expected:** Fire cannot start on stone (fuel = 0).

### Test 7: Burned Cells Don't Reignite
1. Let fire burn out on grass
2. Try to start fire on the burned cell

**Expected:** Cannot ignite - fuel exhausted, cell shows burned tint.

### Test 8: Permanent Fire Source
1. Place a fire source (F + Shift + left-drag)

**Expected:** Fire burns indefinitely, continues producing smoke, never runs out of fuel.

### Test 9: Pathfinding
1. Create a narrow passage
2. Set it on fire

**Expected:** Movers path around the fire, treating it as impassable.

### Test 10: Multi-Z Smoke Rising
1. Start a fire at z=0
2. Observe smoke at z=1, z=2, z=3

**Expected:** Smoke rises through multiple z-levels, spreads horizontally at each level, and dissipates more at higher levels.

### Test 11: Closed Room Smoke Filling
1. Create a sealed room spanning z=0 to z=3 (walls on all sides, ceiling at z=3)
2. Start a fire inside at z=0

**Expected:** Smoke rises to ceiling (z=3), fills horizontally, then fills downward. Eventually the entire room is filled with smoke from top to bottom.

### Test 12: Chimney Ventilation
1. Create a closed room with smoke (from Test 11)
2. Open a hole in the ceiling

**Expected:** Smoke escapes through the hole, room clears from bottom to top (smoke prefers rising over staying).

## Files (Planned)

- `src/fire.h` - Data structures and API
- `src/fire.c` - Fire simulation logic
- `src/smoke.h` - Smoke data structures and API
- `src/smoke.c` - Smoke simulation logic
- `src/demo.c` - F key handler, rendering (DrawFire, DrawSmoke functions)

## Performance Notes

Similar approach to water:
- Stability tracking for cells that aren't changing
- Cap updates per tick to prevent lag
- Process fire bottom-to-top, smoke top-to-bottom (opposite directions)
