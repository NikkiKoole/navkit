# Trees & Vegetation

Cellular automaton approach to tree growth. Inspired by Minecraft growth mechanics, with Dwarf Fortress chopping/woodcutter behavior.

## Cell Types

| Cell | Walkable | Description |
|------|----------|-------------|
| `CELL_TREE_TRUNK` | No | Solid wood, blocks movement, choppable |
| `CELL_TREE_LEAVES` | ? | Canopy, decays without trunk support |
| `CELL_SAPLING` | Yes? | Young tree, grows into trunk over time |

## Growth Rules (Cellular Automaton)

### Sapling → Trunk
- Sapling placed on grass/dirt
- After N ticks, becomes `CELL_TREE_TRUNK`
- Trunk spawns another trunk above (z+1) if air
- Repeats until max height reached

### Trunk → Leaves
- When trunk reaches target height (or stops growing)
- Spawns `CELL_TREE_LEAVES` in radius around top
- Leaves spread to adjacent air cells (limited radius)

### Leaf Decay
- Leaves check: am I connected to a trunk?
- If no trunk within N cells → decay over time
- Decaying leaves may drop saplings

## Decisions Made

### Walkability
- **Leaves are NOT walkable** - you can't walk on them, can't walk through them
- They block movement but aren't solid platforms

### Height
- Trees grow up to ~10 cells max (1 cell ≈ 3m, so ~30m tall trees)
- Not all trees reach max height - random variation

### Light & Shadows
- Leaves will block light and affect grass below (when light system exists)
- For now: just track that leaves are "overhead" for future use

### Chopping Behavior
- **Cut base trunk → whole tree falls** (like DF)
  - All trunk cells above become air
  - All connected leaves become air
  - Drops wood items (based on trunk height)
  - Drops saplings (random, from leaves)
- **Cut higher trunk → prune** (unlike DF)
  - Only trunk at cut point and above removed
  - Leaves connected to removed section also removed
  - Lower trunk stays, tree survives (but shorter)
  - Drops wood/saplings for removed portion only
- Allows both quick felling and selective pruning

## Dwarf Fortress Reference

How DF handles trees (for comparison):

### DF Tree Structure
- Multi-tile: trunk, thick branches (walkable), regular branches (walkable), twigs (climbable only), leaves
- Dense forest canopy can form continuous walkable surface
- Trunks block construction and pathing

### DF Growth
- Saplings spawn randomly on suitable biome tiles
- **Saplings won't mature if items/buildings on tile** (stockpiles block growth)
- **Heavy foot traffic kills saplings**
- Chopped forest regrows in ~2 years
- Dead/burned trees can rejuvenate after years

### DF Chopping
- Cut ANY trunk tile → whole tree falls (can't prune)
- Logs fall in direction opposite from woodcutter
- Objects on tree fall with damage (logs themselves don't hurt)
- Cutting from height can cause woodcutter to fall

### DF Woodcutter Job
- Requires equipped axe (battle axe works)
- **Mutually exclusive with mining** (both need equipped tools)
- Skill increases speed, axe quality doesn't matter
- Some dwarves happy felling trees, others sad (nature values)

## Conflicts & Decisions

| Aspect | Dwarf Fortress | Our Design | Decision |
|--------|----------------|------------|----------|
| **Branches walkable?** | Yes (thick/regular) | No | **Ours: simpler, leaves block all movement** |
| **Sapling trampling** | Killed by foot traffic | ? | **Adopt from DF: integrate with ground wear** |
| **Items block growth** | Yes | ? | **Adopt from DF: saplings won't grow if items on tile** |
| **Log fall direction** | Away from cutter | N/A | **Simplify: just spawn items at tree base** |
| **Cut from any height** | Yes (whole tree falls) | Yes (prune or fell) | **Hybrid: cut base → whole tree; cut higher → prune** |
| **Woodcutter + Mining** | Mutually exclusive | ? | **Consider: same mover can't do both (tool equip)** |
| **Tree rejuvenation** | Burned trees regrow | ? | **Skip for now: adds complexity** |

## Open Questions

### Sapling Sources (DF: random spawn + regrowth)
- Random spawn on grass (like DF, like current grass spread)
- Dropped when tree falls
- Planted by movers
- All of the above?

### Sapling Blocking (adopt from DF)
- Items on tile prevent sapling growth
- Buildings/stockpiles prevent sapling growth
- Removes item → sapling can suddenly mature

### Foot Traffic (adopt from DF)
- Heavy trampling kills saplings
- Integrate with ground wear system
- High wear on sapling tile → sapling dies
- Protects paths from tree overgrowth naturally

### Fire Interaction
- Leaves flammable? (fast burn)
- Trunk flammable? (slow burn)
- Forest fires spread through leaf canopy?
- DF: burned trees can rejuvenate (skip for now?)

### Woodcutter Labor
- Separate from mining labor? (DF does this)
- Requires axe item to be equipped?
- Or: simplified, any mover can chop?

## Implementation Sketch

### New Cell Types (grid.h)
```c
CELL_SAPLING,      // Young tree, will grow
CELL_TREE_TRUNK,   // Solid wood
CELL_TREE_LEAVES,  // Canopy block
```

### Growth Tick (new file? trees.c)
```c
void TreesTick(float dt) {
    // For each sapling: check growth timer, convert to trunk
    // For each trunk: check if should grow upward or spawn leaves
    // For each leaf: check trunk connection, decay if orphaned
}
```

### Trunk Connection Check
```c
bool IsConnectedToTrunk(int x, int y, int z, int maxDist) {
    // BFS/flood fill from leaf position
    // Return true if trunk found within maxDist
}
```

### Chopping (jobs.c)
```c
// New job type: JOBTYPE_CHOP
// Mover walks to base trunk (z-level where trunk meets ground)
// On complete: call FellTree(x, y, z)

void FellTree(int x, int y, int z) {
    int woodCount = 0;
    int leafCount = 0;
    
    // Remove all trunk cells going upward
    for (int tz = z; tz < gridDepth; tz++) {
        if (grid[tz][y][x] != CELL_TREE_TRUNK) break;
        grid[tz][y][x] = CELL_AIR;
        woodCount++;
        MarkChunkDirty(x, y, tz);
    }
    
    // Remove all connected leaves (flood fill from trunk positions)
    // ... BFS to find and remove CELL_TREE_LEAVES
    // leafCount = number removed
    
    // Spawn wood items (1 per trunk cell, or scaled)
    for (int i = 0; i < woodCount; i++) {
        SpawnItem(..., ITEM_WOOD);
    }
    
    // Spawn saplings (random chance per leaf)
    int saplings = leafCount / 10;  // ~10% of leaves drop sapling
    for (int i = 0; i < saplings; i++) {
        SpawnItem(..., ITEM_SAPLING);
    }
}
```

## Growth Patterns

### Simple (Start Here)
```
Tick 0:    Tick 100:   Tick 200:   Tick 300:
   .          .          LLL         LLL
   .          .          LTL         LTL
   .          T          .T.         LTL
   S          T           T           T
```
S=sapling, T=trunk, L=leaves

### Pine Style (Later?)
```
    L
   LTL
  LLTLL
 LLLLLLL
    T
    T
    T
```

### Oak Style (Later?)
```
  LLLLL
 LLLLLLL
LLLLTLLLL
 LLLLLLL
    T
    T
```

## Interaction with Other Systems

### Ground Wear
- Trees don't grow on heavily worn paths
- Or: saplings get trampled (destroyed by mover walking over)

### Water
- Trees near water grow faster?
- Or: require minimum moisture?

### Seasons (Future)
- Leaves change color in autumn
- Leaves fall off in winter (deciduous)
- Or: evergreen trees keep leaves

### Items
- `ITEM_WOOD` - from chopping trunk
- `ITEM_SAPLING` - from decaying leaves, plantable
- `ITEM_LEAVES`? - maybe not needed

## Estimated Effort

| Component | Lines | Notes |
|-----------|-------|-------|
| Cell types | ~20 | Enum + walkability |
| Growth tick | ~100-150 | Automaton rules |
| Trunk connection | ~50 | BFS for leaf decay |
| Chopping job | ~100 | Reuse job pattern |
| Rendering | ~50 | New cell visuals |
| **Total** | ~350-400 | |

## Build Order

1. Add cell types (trunk, leaves, sapling)
2. Manual placement for testing
3. Growth tick: sapling → trunk
4. Growth tick: trunk grows upward
5. Growth tick: top trunk spawns leaves
6. Leaf decay when orphaned
7. Chopping job + wood item
8. Sapling drops from leaves
