# Future Cell Types

Ideas for new cell types to make worlds more interesting.

## Door
- A cell type that can be open/closed, affecting pathfinding
- When closed: blocks movement (like CELL_WALL)
- When open: allows movement (like CELL_FLOOR)
- Could have state that changes at runtime, requiring path recalculation
- Question: Do you want doors that NPCs can open, or just static open/closed states?

## Road/Path
- A walkable surface with lower movement cost than grass/dirt
- Encourages pathfinding to prefer roads when available
- Visually distinct from regular CELL_WALKABLE
- Question: Should this affect pathfinding cost, or just be visual?

## Stairs
- Different from ladders - gradual slope between z-levels
- Could span multiple cells horizontally while going up one z-level
- More realistic for buildings than instant ladders
- Question: How many cells should a staircase span? 2-3 cells per z-level?

## Rubble
- Walkable but with higher movement cost (slow terrain)
- Could block some paths while allowing others
- Good for destroyed buildings, construction sites
- Question: Should it be passable but slow, or completely blocking like walls?

## Fence
- Blocks movement but doesn't block line of sight (if that matters)
- Could be jumpable/climbable by some movers?
- Lower than walls visually
- Question: Just a wall variant, or special behavior?

## Water
- Blocks normal movement
- Could have shallow (wadeable, slow) vs deep (impassable)
- Rivers, ponds, canals for the estate
- Question: Any swimming/boat mechanics, or just impassable?
