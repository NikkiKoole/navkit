# Future Cell Types

Ideas for new cell types to make worlds more interesting.

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

## Door
- A cell type that can be open/closed, affecting pathfinding
- When closed: blocks movement (like CELL_WALL)
- When open: allows movement (like CELL_FLOOR)
- Could have state that changes at runtime, requiring path recalculation
- Treat as a special wall edge that can connect spaces
- Instantly makes interiors feel intentional
- Question: Do you want doors that NPCs can open, or just static open/closed states?

## Road/Path
- A walkable surface with lower movement cost than grass/dirt
- Encourages pathfinding to prefer roads when available
- Gives you a "spine" for navigation
- Visually distinct from regular CELL_WALKABLE
- Question: Should this affect pathfinding cost, or just be visual?

## Stairs
- Different from ladders - gradual slope between z-levels
- Could span multiple cells horizontally while going up one z-level
- More realistic for buildings than instant ladders
- Reads more "urban" and allows wider flows than ladders
- Question: How many cells should a staircase span? 2-3 cells per z-level?

## Rubble
- Walkable but with higher movement cost (slow terrain)
- A "mostly impassable" tile to create broken routes / repairs
- Good for destroyed buildings, construction sites
- Question: Should it be passable but slow, or completely blocking like walls?

## Fence
- Blocks movement but doesn't block line of sight
- A soft boundary - makes estates/industry feel real
- Could be jumpable/climbable by some movers?
- Lower than walls visually
- Question: Just a wall variant, or special behavior?

## Water
- Blocks normal movement - gives hard edges
- Could have shallow (wadeable, slow) vs deep (impassable)
- Rivers, ponds, canals - instant "post-cataclysm" feel
- Question: Any swimming/boat mechanics, or just impassable?

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

## Big Picture

1. Abstract city grid with district types
2. Districts have connection rules
3. Fill in with more detail
4. Eventually use WFC to fill everything
5. Start small - things you can draw that have immediate effect
