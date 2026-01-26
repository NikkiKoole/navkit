# Water System

A Dwarf Fortress-style water simulation with gravity, spreading, and pressure mechanics.

## Controls

- **W + left-drag** - Place water sources (bright blue center)
- **W + right-drag** - Place drains (dark center)

## Water Levels

Water uses a 1-7 scale (like DF):
- **0** - Dry
- **1-2** - Shallow (slight speed penalty: 0.85x)
- **3-4** - Medium (noticeable slowdown: 0.6x)
- **5-7** - Deep (major slowdown: 0.35x)
- **7** - Full (required for pressure to work)

## Three Phases of Water Movement

Water processes in priority order each tick:

### Phase 1: Falling (Gravity)
Water drops into the cell below if it's not full.

**Key behavior:** When water falls onto a full (7/7) cell, it creates *pressure*. This is how pressure enters the system.

### Phase 2: Spreading (Equalization)  
Water spreads horizontally to orthogonal neighbors (N/S/E/W, no diagonals).

- Only flows to neighbors with lower water levels
- Transfers half the difference (smooths oscillation)
- Requires at least 2 level difference to trigger

### Phase 3: Pressure
When a cell is full (7/7) AND has pressure, it searches for an outlet.

**How pressure works:**
1. BFS (breadth-first search) through adjacent full cells
2. Searches orthogonally only (no diagonal pressure propagation)
3. When it finds a non-full cell, pushes 1 unit of water there
4. Water can rise UP through pressure, but only to `sourceZ - 1`

**The sourceZ - 1 rule:** If water falls from z=5 onto full water at z=4, the pressure can push water up to z=4 (one below the source). This is a deliberate DF design choice for CPU optimization.

## Sources and Drains

**Sources:**
- Refill to level 7 every tick
- Generate pressure at their z-level
- Visualized with bright blue center

**Drains:**
- Remove all water every tick
- Visualized with dark center

## Evaporation

Water at level 1 has a 1/100 chance per tick to evaporate and disappear. Sources don't evaporate.

## Test Scenarios

### Test 1: Basic Flow
1. Generate flat terrain
2. Place a water source (W + left-drag) somewhere
3. Watch water spread outward and settle

**Expected:** Water spreads in a circle, levels equalize, edges are shallow (level 1-2), center stays full (7) near source.

### Test 2: Waterfall
1. Go to z=1 (press `.` to go up)
2. Place a water source
3. Go to z=0 (press `,` to go down)
4. Watch water fall down

**Expected:** Water falls from z=1 to z=0 and spreads at ground level.

### Test 3: Filling a Pool
1. At z=0, draw walls (R + drag) to create a rectangular room
2. Place a water source inside
3. Watch it fill up

**Expected:** Water fills the room to level 7, then spreads if there's an opening.

### Test 4: Pressure / U-Bend
This tests pressure pushing water upward.

```
Setup (side view):

z=2:  [SOURCE]        [   ]     <- water should rise here
z=1:  [WALL  ]        [WALL]    <- walls block horizontal flow  
z=0:  [      ][      ][    ]    <- open channel at bottom
```

**Steps:**
1. At z=0: clear/walkable ground
2. At z=1: build walls on left and right, leave middle open
3. At z=2: place water source on left side

**Expected:** 
- Water falls from source (z=2) to z=1, hits wall
- Falls to z=0, flows along the channel
- On the right side, pressure pushes water UP to z=1 (one below source at z=2)

### Test 5: Pressure Height Limit
Same as Test 4, but make the source at z=3.

**Expected:** Water rises to z=2 on the other side (sourceZ - 1 = 3 - 1 = 2).

### Test 6: Drain
1. Create a pool with water
2. Place a drain (W + right-drag) at one edge
3. Watch water flow toward drain and disappear

**Expected:** Water level drops as drain removes water. Flow pattern shows water moving toward drain.

### Test 7: Evaporation
1. Place a small water source
2. Remove the source (place drain on it, then remove drain)
3. Wait and watch the shallow edges

**Expected:** Level 1 water slowly disappears over time (1/100 chance per tick).

## Performance Notes

- Only "unstable" cells are processed (cells that recently changed or have changing neighbors)
- When water settles, cells become "stable" and skip processing
- Maximum 4096 cell updates per tick to prevent lag
- Pressure search limited to 64 cells to prevent pathological cases

## Debugging

The `waterUpdateCount` global tracks how many cells were processed last tick. High numbers indicate lots of active water movement.

## Files

- `pathing/water.h` - Data structures and API
- `pathing/water.c` - Simulation logic
- `pathing/demo.c` - W key handler and rendering (DrawWater function)
