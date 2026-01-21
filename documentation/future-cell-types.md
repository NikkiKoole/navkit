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

## Priority Order

The 6 tiles below create maximum variety with minimum effort:
1. Door - creates rooms
2. Road/Path - creates routes/spines
3. Stairs - multi-level urban flow
4. Rubble - cataclysm/broken routes
5. Fence - soft boundaries
6. Water - hard edges

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

---

## Biggest Architectural Change Needed

**Variable movement costs.** Currently hardcoded to 10/14.

To support road (cheaper) and rubble (expensive):
```c
// New helper function
int GetCellMoveCost(CellType cell) {
    switch (cell) {
        case CELL_ROAD: return 5;      // Faster
        case CELL_RUBBLE: return 20;   // Slower
        default: return 10;            // Normal
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

---

## Future Ideas (later phases)

### Connectivity
- Corner walls (oriented by neighbors)
- Void / Cliff / Pit (map edges other than straight walls)

### Function Tiles (colony/life sim hooks)
- Storage - reason for pathing and space planning
- Workshop - generic "work" tile that branches into stations
- Bed / Housing marker - measure "livability" per block

### Landmarks
- Tower base, crane base, statue, antenna
- One per district makes navigation and identity pop

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
4. Update `IsCellWalkableAt()` to include `CELL_DOOR_OPEN`
5. Update terrain generators to use new types

### Phase 2: Movement costs
1. Add `GetCellMoveCost()` function
2. Update A* in pathfinding.c (~6-8 places)
3. Update HPA* cost calculations
4. Update JPS/JPS+ (may need more thought)
5. Add `CELL_ROAD` and `CELL_RUBBLE`

### Phase 3: Stairs
1. Design how stairs span cells (2-3 cells per z-level?)
2. Update z-transition logic
3. Update terrain generators

---

## Maybe Later (more tile ideas)

### Terrain/Ground types
- **Grass vs Dirt** - distinguish from CELL_WALKABLE for visuals or cost later
- **Sand** - slower movement, beaches/desert areas
- **Ice** - slippery? momentum-based movement? Or just visual
- **Mud** - slow terrain, appears after rain/floods

### Vertical stuff
- **Ramp** - like stairs but for vehicles/carts if you ever have those
- **Bridge** - walkable over water/void, needs z-level awareness
- **Hole/Trapdoor** - like ladder but one-way down, or openable

### Urban/Building
- **Window** - blocks movement, doesn't block sight (like fence but for buildings)
- **Counter/Table** - blocks movement, partial cover, could be vaulted over
- **Elevator** - like ladder but for multi-floor jumps, colony sim vibes

### Functional (colony sim)
- **Spawn point** - where movers appear
- **Goal/Target** - destinations for testing

### Hazards
- **Fire** - damages, spreads, blocks movement or high cost
- **Electric/Danger** - instant death or avoidance zone

Most of these are variations on: blocking, cost modifier, or z-transition.

---

## Big Picture

1. Abstract city grid with district types
2. Districts have connection rules
3. Fill in with more detail
4. Eventually use WFC to fill everything
5. Start small - things you can draw that have immediate effect
