# Future Cell Types

Ideas for new cell types to make worlds more interesting.

## Current State

**Existing cell types:**
- `CELL_WALKABLE` - outdoor ground
- `CELL_WALL` - blocks movement
- `CELL_LADDER` - connects z-levels
- `CELL_AIR` - empty space (fall through)
- `CELL_FLOOR` - indoor walkable

**How walkability works:**
```c
IsCellWalkableAt() returns true for: WALKABLE, FLOOR, or LADDER
```

**Movement cost is hardcoded:**
```c
int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
```

---

## Complete Tile List by Category

### Terrain & Ground
| Tile | Notes |
|------|-------|
| Dirt / Path | Distinguish from CELL_WALKABLE for visuals or cost |
| Grass | Visual variant, slightly higher cost than road |
| Sand | Slower movement, beaches/desert areas |
| Snow | Cold areas, similar to sand for movement |
| Ice | Slippery? momentum-based movement? Or just visual |
| Mud / Swamp | Slow terrain, appears after rain/floods |
| Stone / Rock ground | Solid terrain variant |
| Road | Lower movement cost, creates navigation spines |
| Rubble | Broken routes, higher movement cost |
| Cliff edges | Top, bottom, corners - map boundaries |
| Water (shallow) | Wadeable, slow movement |
| Water (deep) | Impassable, hard map edges |

### Obstacles (Blocked)
| Tile | Notes |
|------|-------|
| Trees (trunk) | Natural obstacle |
| Rocks / Boulders | Natural obstacle |
| Walls (stone, wood, brick) | Building boundaries |
| Fences / Gates | Soft boundaries, blocks movement not sight |
| Bushes / Hedges | Natural soft boundaries |
| Deep water | Hard map edges |

### Building & Interior
| Tile | Notes |
|------|-------|
| Wall tiles | Top, bottom, corners, inner corners |
| Doors (open/closed) | State affects pathfinding |
| Windows | Blocks movement, doesn't block sight |
| Roofs | Building tops |
| Wood floor | Interior walkable |
| Carpet | Interior walkable variant |
| Tile floor | Interior walkable variant |
| Counter / Table | Blocks movement, partial cover |

### Vertical Navigation
| Tile | Notes |
|------|-------|
| Ladder | Current - connects z-levels (single cell) |
| Stairs | Gradual slope, spans multiple cells |
| Bridge | Walkable over water/void, z-level aware |
| Ramps / Slopes | For vehicles/carts, gradual incline |
| Cliff ladders | Vertical navigation on natural terrain |
| Teleport / Warp | Instant position change |
| Hole / Trapdoor | One-way down, or openable |
| Elevator | Multi-floor jumps, colony sim vibes |

### Interactive
| Tile | Notes |
|------|-------|
| Switches | Trigger state changes |
| Pressure plates | Movement-activated triggers |
| Chests | Container interaction |
| Breakable tiles | Cracked walls, rocks - destructible |
| Farm tiles | Plowed soil, crops - simulation |
| Traps | Spikes, lava, poison - hazards |

### Decoration (No Collision)
| Tile | Notes |
|------|-------|
| Flowers | Visual variety |
| Grass tufts | Visual variety |
| Puddles | Environmental detail |
| Floor cracks | Damage/age indication |
| Signs | Information/wayfinding |
| Torches / Lamps | Lighting sources |
| Furniture | Tables, chairs, beds |

### Logic / System
| Tile | Notes |
|------|-------|
| Spawn points | Player, enemy, NPC origins |
| Goal / Target | Destinations for testing |
| Save point | Game state persistence |
| Heal tile | Health restoration |
| Damage tile | Hazard zone |
| Trigger tile | Cutscenes, dialogue activation |

### Functional (Colony Sim)
| Tile | Notes |
|------|-------|
| Storage | Reason for pathing and space planning |
| Workshop | Generic "work" tile, branches into stations |
| Bed / Housing marker | Measure "livability" per block |

### Hazards
| Tile | Notes |
|------|-------|
| Fire | Damages, spreads, blocks or high cost |
| Electric / Danger | Instant death or avoidance zone |

### Landmarks
| Tile | Notes |
|------|-------|
| Tower base | District identity |
| Crane base | Construction/industrial |
| Statue | Navigation landmark |
| Antenna | Urban landmark |

---

## Pathfinding Tile Properties

```c
typedef struct {
    bool walkable;
    float movementCost;
    int heightLevel;
    bool allowsUp;       // Can move to higher cell from here
    bool allowsDown;     // Can move to lower cell from here  
    Direction oneWay;    // Optional: N/E/S/W or NONE
    HazardType hazard;   // Optional: NONE, FIRE, POISON, etc.
    bool blocksSight;    // For future LOS
} TileProperties;
```

### Movement Cost Examples
| Tile | Cost | Notes |
|------|------|-------|
| Road | 0.5 (5) | Preferred path |
| Floor | 1.0 (10) | Normal indoor |
| Grass | 1.2 (12) | Slight penalty |
| Dirt | 1.0 (10) | Normal outdoor |
| Sand | 1.5 (15) | Medium penalty |
| Snow | 1.5 (15) | Medium penalty |
| Mud/Swamp | 2.0–3.5 (20-35) | Heavily avoided |
| Rubble | 2.0 (20) | Obstacle terrain |
| Shallow water | 2.5 (25) | Very slow |

*Values in parentheses are integer equivalents for current 10/14 system*

---

## Height Level System

Use **discrete integers** for pathfinding (not continuous meters).

| Visual Height | heightLevel |
|---------------|-------------|
| 0–2.9 m | 0 |
| 3–5.9 m | 1 |
| 6–8.9 m | 2 |
| 9–11.9 m | 3 |

**Movement rule:**
```c
bool CanMoveVertically(Cell* from, Cell* to, Agent* agent) {
    int heightDiff = abs(from->heightLevel - to->heightLevel);
    
    if (heightDiff <= agent->stepHeight) {
        // Normal movement with climb cost
        return true;
    }
    
    // Check for vertical connectors (ladder, stairs)
    if (from->type == CELL_LADDER || from->type == CELL_STAIRS) {
        return true;
    }
    
    return false;
}
```

Ladders/stairs **override** height restrictions and connect levels.

---

## Priority Order

The 6 tiles below create maximum variety with minimum effort:
1. **Door** - creates rooms
2. **Road/Path** - creates routes/spines
3. **Stairs** - multi-level urban flow
4. **Rubble** - cataclysm/broken routes
5. **Fence** - soft boundaries
6. **Water** - hard edges

These enable:
- Estate courtyards (fence + grass + doors)
- High street spines (road + doors)
- Cataclysm edges (water + rubble)
- Multi-level routes (stairs + ladders)

---

## Implementation Difficulty

| Tile | Difficulty | Why |
|------|------------|-----|
| **Fence** | Easy | Just another wall type, add to enum |
| **Water (deep)** | Easy | Just another blocking type like wall |
| **Door (static)** | Easy | Two cell types: DOOR_OPEN, DOOR_CLOSED |
| **Door (dynamic)** | Medium | Need state change + mark chunks dirty for rebuild |
| **Road/Path** | Medium-Hard | Needs variable movement cost system |
| **Rubble** | Medium | Same - needs variable movement cost |
| **Stairs** | Medium | Z-transition logic is ladder-specific, stairs span multiple cells |
| **Trees/Rocks** | Easy | Blocking obstacles, add to enum |
| **Decorations** | Easy | No collision, purely visual layer |
| **Interactive tiles** | Hard | Need game state, events, triggers |

---

## Biggest Architectural Change Needed

**Variable movement costs.** Currently hardcoded to 10/14.

To support road (cheaper) and rubble (expensive):
```c
// New helper function
int GetCellMoveCost(CellType cell) {
    switch (cell) {
        case CELL_ROAD: return 5;       // Faster
        case CELL_RUBBLE: return 20;    // Slower
        case CELL_MUD: return 25;       // Very slow
        case CELL_SHALLOW_WATER: return 25;
        default: return 10;             // Normal
    }
}

// In pathfinding (many places in pathfinding.c)
int baseCost = GetCellMoveCost(grid[z][ny][nx]);
int moveCost = (dx[i] != 0 && dy[i] != 0) ? baseCost * 14 / 10 : baseCost;
```

This change touches ~6-8 places in pathfinding.c but is straightforward.

---

## Easy Wins (no architecture changes)

### Fence
```c
CELL_FENCE  // Treat like CELL_WALL in pathfinding
```
- Blocks movement
- Doesn't block line of sight (LOS isn't in pathfinding anyway)
- Just visual/semantic difference from wall

### Water (deep)
```c
CELL_WATER  // Treat like CELL_WALL in pathfinding
```
- Blocks movement completely
- Creates hard map edges
- Instant "post-cataclysm" feel

### Door (static version)
```c
CELL_DOOR_OPEN    // Add to IsCellWalkableAt() 
CELL_DOOR_CLOSED  // Treat like CELL_WALL
```
- No runtime state changes
- Terrain generator decides open/closed at generation time
- Still makes interiors feel intentional

### Natural Obstacles
```c
CELL_TREE    // Treat like CELL_WALL
CELL_ROCK    // Treat like CELL_WALL
CELL_BUSH    // Treat like CELL_WALL (or walkable with cost)
```

---

## Detailed Tile Notes

### Door
- A cell type that can be open/closed, affecting pathfinding
- When closed: blocks movement (like CELL_WALL)
- When open: allows movement (like CELL_FLOOR)
- Could have state that changes at runtime, requiring path recalculation
- Treat as a special wall edge that can connect spaces
- Instantly makes interiors feel intentional
- **Static version:** Two cell types, no runtime changes
- **Dynamic version:** Need separate state array or runtime toggle + chunk dirty marking

### Road/Path
- A walkable surface with lower movement cost than grass/dirt
- Encourages pathfinding to prefer roads when available
- Gives you a "spine" for navigation
- Visually distinct from regular CELL_WALKABLE
- **Requires:** Variable movement cost system

### Stairs
- Different from ladders - gradual slope between z-levels
- Could span multiple cells horizontally while going up one z-level
- More realistic for buildings than instant ladders
- Reads more "urban" and allows wider flows than ladders
- **Challenge:** Current z-transition is ladder-specific (single cell)
- **Options:** 
  - Treat as multiple ladder cells in a row
  - Or special "stair entry/exit" cells that connect

### Rubble
- Walkable but with higher movement cost (slow terrain)
- A "mostly impassable" tile to create broken routes / repairs
- Good for destroyed buildings, construction sites
- **Requires:** Variable movement cost system
- **Alternative:** Make it fully blocking (simpler, less interesting)

### Fence
- Blocks movement but doesn't block line of sight
- A soft boundary - makes estates/industry feel real
- Could be jumpable/climbable by some movers (future feature)
- Lower than walls visually
- **Easy to add:** Just another wall variant in pathfinding

### Water
- Blocks normal movement - gives hard edges
- Could have shallow (wadeable, slow) vs deep (impassable)
- Rivers, ponds, canals - instant "post-cataclysm" feel
- **Easy version:** Deep water only, blocks like wall
- **Advanced version:** Shallow water with movement cost (needs cost system)

### Bridge
- Walkable surface over water/void
- Needs z-level awareness (bridge at z=1 over water at z=0)
- Creates connectivity across obstacles
- **Challenge:** Need to handle "under bridge" vs "on bridge"

### Breakable Tiles
- Cracked walls, weak rocks that can be destroyed
- Requires game state tracking (health/durability)
- Creates dynamic pathfinding (new routes open up)
- **Complexity:** Medium-high due to state management

### Farm Tiles
- Plowed soil, planted crops at various growth stages
- Simulation-oriented feature
- Movement cost could vary by crop state
- **Complexity:** High due to time/state simulation

---

## Future Ideas (later phases)

### Connectivity
- Corner walls (oriented by neighbors)
- Void / Cliff / Pit (map edges other than straight walls)
- One-way tiles (escalators, slides, currents)

### Decorative Layer
- Separate layer for non-collision visuals
- Flowers, grass tufts, puddles, debris
- Doesn't affect pathfinding
- Adds visual richness cheaply

### Height Transitions
- Discrete height levels (0, 1, 2, 3...)
- Agents have stepHeight property
- Automatic cost for small height changes
- Block movement for large height gaps (unless ladder/stairs)

---

## WFC Edge Types (for future wave function collapse)

Instead of lots of tiles, give each tile connectors:
- `walkable`: yes/no
- `block_sight`: yes/no
- `outside`: yes/no
- `connect_N/E/S/W`: wall/door/open/road
- `level`: 0/1 (for ladder/stairs)

One new logical tile (like "road") creates tons of new layouts.

---

## Suggested Implementation Order

### Phase 1: Easy wins (no pathfinding changes)
1. Add `CELL_FENCE` - blocks movement
2. Add `CELL_WATER` - blocks movement  
3. Add `CELL_DOOR_OPEN` / `CELL_DOOR_CLOSED`
4. Add `CELL_TREE`, `CELL_ROCK` - natural obstacles
5. Update `IsCellWalkableAt()` to include `CELL_DOOR_OPEN`
6. Update terrain generators to use new types

### Phase 2: Movement costs
1. Add `GetCellMoveCost()` function
2. Update A* in pathfinding.c (~6-8 places)
3. Update HPA* cost calculations
4. **Disable JPS/JPS+ for variable-cost maps** (see note below)
5. Add `CELL_ROAD` and `CELL_RUBBLE`
6. Add `CELL_MUD`, `CELL_SAND`, `CELL_SHALLOW_WATER`

> **JPS/JPS+ Limitation:** JPS and JPS+ do NOT support variable cost terrain. They rely on grid symmetry—assuming all walkable cells have identical cost—to prune neighbors and jump to decision points. With variable costs, a detour through cheaper terrain might beat a direct path, but JPS would skip it entirely. Options: fall back to A* for variable-cost searches, or use JPS only for uniform-cost layers.

### Phase 3: Stairs & Height
1. Design how stairs span cells (2-3 cells per z-level?)
2. Update z-transition logic
3. Add height level property to cells
4. Implement height-based movement rules
5. Update terrain generators

### Phase 4: Interactive & Dynamic
1. Add dynamic door state system
2. Add breakable tile system
3. Add trigger/event tiles
4. Integrate with game state

### Phase 5: Decorations & Polish
1. Add separate decoration layer
2. Implement visual-only tiles
3. Add landmarks

---

## Maybe Later (more tile ideas)

### Terrain variations
- Ice with momentum/sliding mechanics
- Lava (impassable + damage nearby)
- Quicksand (very slow, possible trap)

### Special movement
- Conveyor belts (forced movement direction)
- Teleporters (paired destinations)
- Jump pads (skip cells)
- Ziplines (fast diagonal movement)

### Building interiors
- Furniture as obstacles with partial cover
- Shelving (blocks, but climbable?)
- Counters (vaultable obstacles)

### Natural terrain
- Different tree types (visual variety)
- Crop fields (seasonal changes)
- Rivers with current (affects movement direction)

### Urban features
- Street lights (decoration + lighting)
- Benches (small obstacles)
- Vehicles (large obstacles, possibly movable)
- Construction scaffolding (climbable structure)

---

## Big Picture

1. Abstract city grid with district types
2. Districts have connection rules
3. Fill in with more detail
4. Eventually use WFC to fill everything
5. Start small - things you can draw that have immediate effect

The key insight: most tiles are variations on three properties:
1. **Blocking** - can you walk through it?
2. **Cost modifier** - how fast can you move?
3. **Z-transition** - does it connect height levels?

Everything else is visual/semantic flavor on top of these fundamentals.
