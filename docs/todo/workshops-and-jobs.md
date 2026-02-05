# New Workshops & Jobs: Production Loops

Building on our terrain cells (dirt, clay, gravel, sand, peat, wood, leaves) and autarky philosophy.

---

## Design Principles

Every addition must have:
1. **Source** - where does it come from?
2. **Sink** - where does it go?
3. **Feedback** - does it strengthen existing loops?

Prefer small complete loops over long incomplete chains.

## Current Limitations

**We only have "goods" - flat items with no special behavior.**

Things we do NOT have yet:
- **Containers** - no inventory system, no "basket holds 10 berries", no bags
- **Tools** - no equipment slots, no "worker uses axe to chop faster"
- **Durability** - items don't wear out or break
- **Quality** - no good/poor/excellent variants
- **Fuel over time** - workshops consume inputs per recipe, not continuous fuel burn
- **Health/injury** - no medical system
- **Farming** - no crops, fertilizer use, or plowing
- **Animals** - no hunting, hides, or meat
- **Water as resource** - water exists in world but not as item input

All items are currently equal: they exist, can be hauled, stored, and used as recipe inputs. That's it.

**Implication for this document:** Many ideas below describe *future* systems. For now, mark with:
- `[future:tools]` - requires tool/equipment system
- `[future:containers]` - requires inventory/container system
- `[future:farming]` - requires farming system
- `[future:medical]` - requires health system
- `[future:animals]` - requires animal/hunting system
- `[future:fuel-over-time]` - requires continuous fuel consumption
- `[future:durability]` - requires item wear/degradation
- `[future:water-input]` - requires water as craftable input
- `[needs:CELL_*]` - requires new cell type
- `[needs:ITEM_*]` - requires new item type
- `[OPEN LOOP]` - violates autarky (no current sink)

---

## Current State

### Terrain Sources (Cells)
| Cell | Mining Yields | Notes |
|------|---------------|-------|
| CELL_WALL | ITEM_ROCK | Basic stone |
| CELL_DIRT | ITEM_DIRT | Abundant, low value |
| CELL_CLAY | ITEM_CLAY | Pottery/brick potential |
| CELL_GRAVEL | ITEM_GRAVEL | Drainage, paths |
| CELL_SAND | ITEM_SAND | Glass, mortar |
| CELL_PEAT | ITEM_PEAT | Fuel source (fuel: 6) |
| CELL_TREE_TRUNK | ITEM_WOOD | Chop job (fuel: 64) |
| CELL_TREE_LEAVES | ITEM_LEAVES_* | Gather job (oak/pine/birch/willow) |
| CELL_SAPLING | ITEM_SAPLING_* | Gather job |

### Existing Items (Needing Sinks)
- `ITEM_LEAVES_OAK`, `ITEM_LEAVES_PINE`, `ITEM_LEAVES_BIRCH`, `ITEM_LEAVES_WILLOW` - Currently no use

### Existing Workshops
- **Stonecutter**: ITEM_ROCK (1) -> ITEM_STONE_BLOCKS (2)

### Existing Jobs
- Mining, Channeling, Digging ramps
- Hauling, Building
- Chopping trees, Gathering/Planting saplings
- Crafting (at workshop)

---

## Phase 1: Wood Processing Loop

**Goal:** Make wood useful beyond raw logs.

### Workshop: Sawmill

```
Layout (3x3):
  # # O
  # X .
  . . .

# = machinery (blocks movement)
X = work tile
O = output tile
```

**Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 1 ITEM_WOOD | 4 ITEM_PLANKS | 4s | Basic lumber |
| 1 ITEM_WOOD | 8 ITEM_STICKS | 2s | Small pieces |

**New Items:**
- `ITEM_PLANKS` - Building material, furniture
- `ITEM_STICKS` - Crafting component, kindling

**Sinks for planks:**
- Build wooden walls (cheaper than stone, flammable)
- Build wooden floors
- Furniture (future)
- Fuel (burns faster than logs)

**Sinks for sticks:**
- Kindling for fire starting
- Crafting component (tool handles, arrows)
- Fencing/palisades

---

## Phase 2: Kiln Loop (Fire Processing)

**Goal:** Transform materials through heat.

### Workshop: Kiln

```
Layout (3x3):
  # F #
  # X O
  . . .

F = fuel input tile (special)
```

**Fuel Consumption:** Kiln requires fuel items in input tile. Each recipe consumes fuel as an extra input (like a multi-input recipe). This works with current workshop system - no new "fuel over time" mechanic needed.

**Recipes:**
| Input | Output | Time | Fuel | Notes |
|-------|--------|------|------|-------|
| 1 ITEM_CLAY | 2 ITEM_BRICKS | 5s | 1 | Fired clay |
| 1 ITEM_CLAY | 1 ITEM_POTTERY | 4s | 1 | Storage vessels |
| 2 ITEM_WOOD | 3 ITEM_CHARCOAL | 6s | 0 | Wood burns itself, net gain |
| 4 ITEM_SAND + 1 ITEM_PEAT | 1 ITEM_GLASS | 8s | 2 | Basic glass |

**Fuel items (any of):**
- ITEM_WOOD (raw logs) - high heat
- ITEM_PLANKS - good heat
- ITEM_STICKS - kindling, low heat
- ITEM_LEAVES_* (any type) - kindling, very low heat
- ITEM_PEAT - good heat, smoky
- ITEM_CHARCOAL (most efficient) - high heat, clean

**New Items:**
- `ITEM_BRICKS` - Building material (fire-resistant walls)
- `ITEM_POTTERY` - [future:containers] Storage vessels; for now: trade good, furniture
- `ITEM_CHARCOAL` - Efficient fuel, smelting requirement
- `ITEM_GLASS` - Building material (windows), crafting input

**Production Loops Created:**
```
CELL_CLAY -> mine -> ITEM_CLAY -> kiln -> ITEM_BRICKS -> build brick walls
                                       -> ITEM_POTTERY -> storage

CELL_TREE_TRUNK -> chop -> ITEM_WOOD -> kiln -> ITEM_CHARCOAL -> fuel for kiln/smelter
                                             -> (or direct use as fuel)

CELL_SAND + CELL_PEAT -> mine -> kiln -> ITEM_GLASS -> windows, containers
```

---

## Phase 3: Fuel & Heat Integration

**Goal:** Make fuel meaningful beyond kilns.

### Workshop: Firepit / Hearth

Simple heat source for colony.

```
Layout (2x2):
  F X
  . .

F = fuel input
X = work/heat tile
```

**Functions:** [future:fuel-over-time]
- Provides warmth radius (temperature system integration)
- Cooking [future]
- Light source at night

**Note:** Unlike Kiln (fuel per recipe), Firepit needs continuous fuel consumption over time. 

**Possible approach:** Could use bill system (see `docs/todo/jobs/bill-modes.md`):
- "Do forever" bill that consumes 1 fuel -> produces heat/light + ash
- Bill runs continuously as long as fuel is available
- "Tend Fire" job = hauling fuel to firepit input (like Load Kiln)

This would reuse existing bill modes rather than a new "fuel-over-time" mechanic.

**Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 1 ITEM_WOOD | Heat + Light + 1 ITEM_ASH | 30s | Burns one log, repeat via "Do forever" bill |
| 1 ITEM_CHARCOAL | Heat + Light | 45s | Burns longer, no ash |
| 4 ITEM_STICKS | Heat + Light | 15s | Quick burn, kindling |

**New Items:**
- `ITEM_ASH` - Byproduct of burning wood [OPEN LOOP - no current sink]
  - Sink: fertilizer [future:farming]
  - Sink: soap making (with fat) [future:animals]
  - Sink: pottery glaze component [future]

---

## Phase 4: Stone Refinement Loop

**Goal:** Expand stoneworking options.

### Existing Workshop Enhancement: Stonecutter

**Additional Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 1 ITEM_ROCK | 2 ITEM_STONE_BLOCKS | 3s | (existing) |
| 2 ITEM_GRAVEL | 1 ITEM_STONE_BLOCKS | 4s | Reconstituted |
| 1 ITEM_ROCK | 4 ITEM_GRAVEL | 2s | Crush for paths |

**New Uses for Gravel:**
- Path/road construction (movement speed bonus)
- Drainage layer (water flow improvement)
- Concrete component (future)

---

## Phase 5: Earthworks & Construction

**Goal:** Use dirt and terrain materials.

### Workshop: Mason's Bench

For construction materials from earth.

```
Layout (3x3):
  # # O
  # X .
  . . .
```

**Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 2 ITEM_DIRT + 1 ITEM_GRAVEL | 2 ITEM_PACKED_EARTH | 3s | Rammed earth blocks |
| 1 ITEM_CLAY + 2 ITEM_STICKS | 4 ITEM_WATTLE_DAUB | 4s | Cheap walls |
| 4 ITEM_SAND + 1 ITEM_GRAVEL + water | 2 ITEM_CONCRETE | 6s | [future:water-input] |

**New Items:**
- `ITEM_PACKED_EARTH` - Cheap building material, degrades over time [future:durability]
- `ITEM_WATTLE_DAUB` - Very cheap walls, low durability [future:durability], insulating
- `ITEM_CONCRETE` - Strong, slow to make [future:water-input]

---

## Phase 6: Textile & Fiber Loop

**Goal:** Use plant materials for soft goods, including tree leaves.

### New Job: Gather Tall Grass

Designate tall grass -> worker gathers -> ITEM_TALL_GRASS
[needs:CELL_TALL_GRASS] [needs:ITEM_TALL_GRASS]

### New Job: Gather Leaves

Designate tree leaves -> worker gathers -> ITEM_LEAVES_* (already exists)

### New Job: Plow Ground

Designate dirt -> worker plows -> CELL_PLOWED_DIRT
[needs:CELL_PLOWED_DIRT] [future:farming]

### Workshop: Fiber Works

```
Layout (2x3):
  # X O
  . . .
```

**Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 2 ITEM_TALL_GRASS | 1 ITEM_PLANT_FIBER | 2s | Process grass into fiber |
| 2 ITEM_LEAVES_* | 1 ITEM_PLANT_FIBER | 2s | Process any leaf type |
| 4 ITEM_PLANT_FIBER | 1 ITEM_ROPE | 3s | Twisted fiber |
| 8 ITEM_PLANT_FIBER | 1 ITEM_CLOTH | 6s | Woven fabric |
| 4 ITEM_LEAVES_* | 1 ITEM_MULCH | 2s | Any leaf type (direct) |
| 8 ITEM_LEAVES_* | 1 ITEM_THATCH | 4s | Roofing material |

**New Items:**
- `ITEM_TALL_GRASS` - Raw material from gathering [needs:CELL_TALL_GRASS]
- `ITEM_PLANT_FIBER` - Processed from tall grass or leaves
- `ITEM_ROPE` - Construction component, crafting input
- `ITEM_CLOTH` - Building material, crafting input
- `ITEM_MULCH` - Fertilizer [OPEN LOOP - future:farming]
- `ITEM_THATCH` - Roofing material

**Leaf Uses (Sinks for ITEM_LEAVES_*):**
- Fiber Works: leaves -> plant fiber -> rope/cloth (main use)
- Fiber Works: leaves -> mulch (fertilizer) [future:farming]
- Fiber Works: leaves -> thatch (roofing)
- Kiln/Firepit: leaves as low-grade kindling fuel
- Compost: leaves -> soil enrichment [future:farming]
- Bedding: insulation, animal bedding [future:animals]

**Sources:**
- Tall grass (gather designation) -> ITEM_TALL_GRASS [needs:CELL_TALL_GRASS]
- Tree leaves (gather designation) -> ITEM_LEAVES_* (already exists)
- Future: flax/hemp crops [future:farming]

---

## Phase 7: Tool Making Loop

**Goal:** Tools that enable other jobs.

### Workshop: Toolmaker's Bench

```
Layout (3x3):
  # # O
  # X .
  . . .
```

**Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 2 ITEM_ROCK | 1 ITEM_STONE_TOOL | 4s | Basic cutting tool |
| 1 ITEM_STONE_TOOL + 2 ITEM_STICKS + 1 ITEM_ROPE | 1 ITEM_AXE | 5s | Tree chopping |
| 1 ITEM_STONE_TOOL + 1 ITEM_STICKS + 1 ITEM_ROPE | 1 ITEM_PICKAXE | 5s | Mining |

**New Items:**
- `ITEM_STONE_TOOL` - Knapped stone, basic cutting edge
- `ITEM_AXE` - [future:tools] Speeds up tree chopping; for now: crafting input or trade good
- `ITEM_PICKAXE` - [future:tools] Speeds up mining; for now: crafting input or trade good

**Tool Effects:** [future:tools]
- Workers with access to appropriate tools work faster
- Tools stored in tool stockpile near work area
- Tools degrade with use

---

## Job Types to Add

### Gathering Jobs
| Job | Source | Yields | Notes |
|-----|--------|--------|-------|
| Gather Tall Grass | CELL_TALL_GRASS | ITEM_TALL_GRASS | [needs:CELL_TALL_GRASS] |
| Gather Leaves | CELL_TREE_LEAVES | ITEM_LEAVES_* | Already have items |

### Terrain Jobs
| Job | Input | Output | Notes |
|-----|-------|--------|-------|
| Plow Ground | CELL_DIRT | CELL_PLOWED_DIRT | [needs:CELL_PLOWED_DIRT] [future:farming] |
| Uproot Stump | CELL_TREE_STUMP | ITEM_ROOTS + clear | [needs:CELL_TREE_STUMP] |

### Processing Jobs
| Job | Notes |
|-----|-------|
| Load Kiln | Haul fuel items to kiln input tile (kiln consumes fuel per recipe) |
| Tend Fire | Haul fuel to firepit input tile (uses "Do forever" bill, see `docs/todo/jobs/bill-modes.md`) |

### Construction Jobs
| Job | Notes |
|-----|-------|
| Build Road | Place gravel/stone for paths |
| Build Wooden Wall | Uses ITEM_PLANKS |
| Build Brick Wall | Uses ITEM_BRICKS |

---

## Complete Production Chains

### Chain 1: Stone Construction
```
CELL_WALL -> [mine] -> ITEM_ROCK -> [stonecutter] -> ITEM_STONE_BLOCKS -> [build] -> Stone Wall
```

### Chain 2: Wood Construction  
```
CELL_TREE_TRUNK -> [chop] -> ITEM_WOOD -> [sawmill] -> ITEM_PLANKS -> [build] -> Wood Wall/Floor
                                       -> ITEM_STICKS -> [toolmaker] -> Tools
```

### Chain 3: Brick Construction
```
CELL_CLAY -> [mine] -> ITEM_CLAY -+
                                   +-> [kiln] -> ITEM_BRICKS -> [build] -> Brick Wall
CELL_TREE_TRUNK -> [chop] -> ITEM_WOOD (fuel) -+
```

### Chain 4: Fuel Cycle
```
CELL_TREE_TRUNK -> [chop] -> ITEM_WOOD -> [kiln] -> ITEM_CHARCOAL -> fuel for kiln/smelter
                                       -> ITEM_ASH -> fertilizer (future)

CELL_PEAT -> [mine] -> ITEM_PEAT -> direct fuel use
```

### Chain 5: Glass Production
```
CELL_SAND -> [mine] -> ITEM_SAND -+
                                   +-> [kiln] -> ITEM_GLASS -> windows, containers
CELL_PEAT -> [mine] -> ITEM_PEAT (flux + fuel) -+
```

### Chain 6: Fiber to Goods
```
CELL_TALL_GRASS -> [gather] -> ITEM_TALL_GRASS -> [fiber works] -> ITEM_PLANT_FIBER -> ITEM_ROPE -> construction
                                                                                    -> ITEM_CLOTH -> building
[needs:CELL_TALL_GRASS]
```

### Chain 7: Tool Production
```
CELL_WALL -> [mine] -> ITEM_ROCK -> [toolmaker] -> ITEM_STONE_TOOL -+
                                                                      +-> ITEM_AXE [future:tools]
CELL_TREE_TRUNK -> [chop] -> ITEM_WOOD -> [sawmill] -> ITEM_STICKS -+
                                                                      +-> ITEM_PICKAXE [future:tools]
CELL_TALL_GRASS -> [gather] -> ITEM_TALL_GRASS -> [fiber works] -> ITEM_PLANT_FIBER -> ITEM_ROPE -+
[needs:CELL_TALL_GRASS]
```

### Chain 8: Leaves Processing
```
CELL_TREE_LEAVES -> [gather leaves] -> ITEM_LEAVES_* -> [fiber works] -> ITEM_PLANT_FIBER -> rope, cloth
                                                                      -> ITEM_MULCH -> fertilizer [OPEN LOOP]
                                                                      -> ITEM_THATCH -> roofing
                                    -> [kiln/firepit] -> kindling fuel (low heat value)
```

### Chain 10: Wicker Weaving
```
Willow trees -> [harvest withies] -> ITEM_WITHIES -> [weaving bench] -> ITEM_WOVEN_BASKET
                                                                     -> ITEM_WICKER_FENCE
CELL_TALL_GRASS -> [gather] -> ITEM_TALL_GRASS -> [weaving bench] -> ITEM_WOVEN_MAT
[needs:ITEM_WITHIES source] [needs:CELL_TALL_GRASS]
```

### Chain 9: Tree Roots
```
CELL_TREE_TRUNK -> [chop] -> ITEM_WOOD + CELL_TREE_STUMP (roots remain)
                                              |
                    [leave stump] -> tree regrows faster (coppicing)
                                              |
                    [uproot job] -> ITEM_ROOTS + cleared ground for building
                                        |
                        -> [fiber works] -> ITEM_ROOT_FIBER (strong cordage)
                        -> [kiln] -> fuel / charcoal
                        -> [tanning] -> ITEM_TANNIN (oak roots) [OPEN LOOP - future:animals for leather]
                        -> [herbalist] -> ITEM_MEDICINE (willow roots) [OPEN LOOP - future:medical]
[needs:CELL_TREE_STUMP]
```

---

## Phase 8: Tree Roots System

**Goal:** Add depth to forestry with stumps and roots.

### New Cell: CELL_TREE_STUMP

Left behind after chopping a tree trunk. Blocks building but allows walking.

**Properties:**
- Walkable (doesn't block movement)
- Blocks construction (can't build walls/floors on stumps)
- Enables tree regrowth if left alone (coppicing - faster than sapling)
- Must be uprooted to fully clear

### New Job: Uproot Stump

**Designation:** Mark stump for removal
**Work time:** 4s (slower than chopping - roots go deep)
**Yields:** ITEM_ROOTS (1-2 depending on tree type)
**Result:** Ground becomes buildable, no regrowth

### New Item: ITEM_ROOTS

**Sinks:**
| Use | Workshop | Output | Notes |
|-----|----------|--------|-------|
| Strong fiber | Fiber Works | ITEM_ROOT_FIBER | Tougher than leaf fiber |
| Fuel | Kiln/Firepit | heat | Burns slow, smoky |
| Tannin | Tanning Vat | ITEM_TANNIN | Oak roots especially [OPEN LOOP - future:animals] |
| Medicine | Herbalist | ITEM_MEDICINE | Willow roots [OPEN LOOP - future:medical] |

### Regrowth Mechanic (Coppicing)

| Action | Result |
|--------|--------|
| Chop tree, leave stump | Tree regrows in ~50% time (vs sapling) [needs tree growth timing changes] |
| Chop tree, uproot stump | Ground cleared, regrowth only via new sapling |
| Chop tree, plant sapling on stump | Not allowed (must uproot first) |

**Strategic choice:** Fast wood cycle (coppice) vs clear land for building

---

## Phase 9: Wicker Weaving

**Goal:** Direct crafting with flexible branches (withies), not leaves.

**Note:** Real basket weaving uses flexible young branches (withies), not leaves. 
Willow withies are the classic material. This requires a new item ITEM_WITHIES.

### New Job: Harvest Withies

Cut flexible young branches from certain trees (especially willow).
[needs:ITEM_WITHIES] - could come from CELL_TREE_TRUNK (willow) or a new mechanic

### Workshop: Weaving Bench

Simple workshop for hand-weaving plant materials.

```
Layout (2x2):
  # X
  O .
```

**Recipes:**
| Input | Output | Time | Notes |
|-------|--------|------|-------|
| 6 ITEM_WITHIES | 1 ITEM_WOVEN_BASKET | 5s | Willow withies best |
| 8 ITEM_TALL_GRASS | 1 ITEM_WOVEN_MAT | 4s | Grass mat |
| 4 ITEM_WITHIES + 2 ITEM_STICKS | 1 ITEM_FISH_TRAP | 6s | [future] fishing |
| 10 ITEM_WITHIES | 1 ITEM_WICKER_FENCE | 5s | Light fencing |

**New Items:**
- `ITEM_WITHIES` - Flexible young branches [needs:ITEM_WITHIES] - source: willow trees
- `ITEM_WOVEN_BASKET` - [future:containers] Portable storage; for now: trade good, furniture
- `ITEM_WOVEN_MAT` - Floor covering, building material
- `ITEM_FISH_TRAP` - [future] Passive fishing structure
- `ITEM_WICKER_FENCE` - Building material (light fencing)

**Workshop Distinction:**
- **Fiber Works** - Processing: leaves/grass -> fiber -> rope/cloth (twisted/spun)
- **Weaving Bench** - Weaving: withies/grass -> baskets/mats/fencing (interlaced)

---

## Implementation Priority

### Tier 1 (Foundation)
1. **Sawmill** - ITEM_PLANKS, ITEM_STICKS
2. **Kiln** - ITEM_BRICKS, ITEM_CHARCOAL, fuel system

### Tier 2 (Expansion)
3. **Firepit** - Heat/light source, ITEM_ASH
4. **Stonecutter recipes** - Gravel processing
5. **Wooden construction** - Wood walls/floors

### Tier 3 (Forestry Depth)
6. **Weaving Bench** - Direct leaf weaving (baskets, mats, fencing)
7. **Tree stumps** - CELL_TREE_STUMP after chopping
8. **Uproot job** - ITEM_ROOTS from stumps
9. **Coppicing mechanic** - Fast regrowth from stumps

### Tier 4 (Fiber & Textiles)
10. **Fiber gathering** - New job type (grass)
11. **Fiber Works** - ITEM_ROPE, ITEM_CLOTH, leaves->fiber
12. **Root processing** - ITEM_ROOT_FIBER (strong cordage)

### Tier 5 (Construction Materials)
13. **Mason's Bench** - Earth-based materials
14. **Tanning Vat** - ITEM_TANNIN from oak roots

### Tier 6 (Tools & Specialization)
15. **Toolmaker's Bench** - Stone tools, axes, pickaxes
16. **Tool effects** - Speed bonuses

---

## Items Summary

### Existing Items Now With Sinks

| Item | Source | New Sinks |
|------|--------|-----------|
| ITEM_LEAVES_OAK | Gather leaves | Plant fiber, mulch, thatch, kindling |
| ITEM_LEAVES_PINE | Gather leaves | Plant fiber, mulch, thatch, kindling |
| ITEM_LEAVES_BIRCH | Gather leaves | Plant fiber, mulch, thatch, kindling |
| ITEM_LEAVES_WILLOW | Gather leaves | Plant fiber, mulch, thatch, kindling |

### New Cells Needed

| Cell | Source | Notes |
|------|--------|-------|
| CELL_TALL_GRASS | Terrain gen, grass growth | Harvestable for ITEM_TALL_GRASS |
| CELL_PLOWED_DIRT | Plow job on CELL_DIRT | [future:farming] |
| CELL_TREE_STUMP | Left after chopping trunk | Allows coppicing or uproot |

### New Items Needed

| Item | Source | Sinks | Notes |
|------|--------|-------|-------|
| ITEM_PLANKS | Sawmill (wood) | Building, fuel | |
| ITEM_STICKS | Sawmill (wood) | Crafting, kindling | |
| ITEM_BRICKS | Kiln (clay) | Brick walls | Fire-resistant |
| ITEM_POTTERY | Kiln (clay) | Trade good, furniture | [future:containers] |
| ITEM_CHARCOAL | Kiln (wood) | Fuel, smelting | Most efficient fuel |
| ITEM_GLASS | Kiln (sand+peat) | Building (windows) | |
| ITEM_ASH | Burning wood | None currently | [OPEN LOOP] |
| ITEM_PACKED_EARTH | Mason (dirt+gravel) | Cheap walls | [future:durability] |
| ITEM_WATTLE_DAUB | Mason (clay+sticks) | Cheap walls | [future:durability] |
| ITEM_CONCRETE | Mason (sand+gravel+water) | Strong walls | [future:water-input] |
| ITEM_TALL_GRASS | Gather job | Plant fiber, weaving | [needs:CELL_TALL_GRASS] |
| ITEM_PLANT_FIBER | Fiber Works (grass/leaves) | Rope, cloth | |
| ITEM_ROPE | Fiber Works | Construction, crafting | |
| ITEM_CLOTH | Fiber Works | Building, crafting | |
| ITEM_MULCH | Fiber Works (leaves) | None currently | [OPEN LOOP - future:farming] |
| ITEM_THATCH | Fiber Works (leaves) | Roofing | |
| ITEM_STONE_TOOL | Toolmaker | Crafting component | |
| ITEM_AXE | Toolmaker | None currently | [OPEN LOOP - future:tools] |
| ITEM_PICKAXE | Toolmaker | None currently | [OPEN LOOP - future:tools] |
| ITEM_ROOTS | Uproot stump | Fiber, fuel | [needs:CELL_TREE_STUMP] |
| ITEM_ROOT_FIBER | Fiber Works (roots) | Strong rope, crafting | |
| ITEM_TANNIN | Tanning (oak roots) | None currently | [OPEN LOOP - future:animals] |
| ITEM_MEDICINE | Herbalist (willow roots) | None currently | [OPEN LOOP - future:medical] |
| ITEM_WITHIES | Harvest from willow | Weaving | [needs:ITEM_WITHIES source] |
| ITEM_WOVEN_BASKET | Weaving Bench (withies) | Trade good, furniture | [future:containers] |
| ITEM_WOVEN_MAT | Weaving Bench (grass) | Floor covering | |
| ITEM_FISH_TRAP | Weaving Bench | None currently | [OPEN LOOP - future] |
| ITEM_WICKER_FENCE | Weaving Bench (withies) | Building (fencing) | |

---

## Workshops Summary

| Workshop | Primary Function | Key Recipes |
|----------|------------------|-------------|
| Stonecutter | Stone processing | Rock -> Blocks, Gravel |
| Sawmill | Wood processing | Wood -> Planks, Sticks |
| Kiln | Fire processing | Clay -> Bricks, Wood -> Charcoal |
| Firepit | Heat/light | Continuous fuel burn |
| Mason's Bench | Earth construction | Dirt/Clay -> Building materials |
| Fiber Works | Textile processing | Leaves/Fiber -> Rope, Cloth, Mulch |
| Weaving Bench | Hand weaving | Leaves -> Baskets, Mats, Fencing |
| Toolmaker's Bench | Tool crafting | Stone + Wood -> Tools |
| Tanning Vat | Leather processing | Roots -> Tannin (future: hides -> leather) |

---

## Notes

- All loops should be completable with starting terrain
- No item should exist without both source AND sink
- Fuel consumption adds strategic depth (manage wood supply)
- Tools create specialization without hard locks
- Building materials have tradeoffs (cost, durability, fire resistance)
- Future: smelting loop requires charcoal (kiln dependency)

---

## Open Loops Summary (Autarky Violations)

Items with no current sink - defer until system exists:

| Item | Needs System | Notes |
|------|--------------|-------|
| ITEM_ASH | Farming or crafting recipes | Could add to pottery glaze recipe |
| ITEM_MULCH | Farming | Fertilizer has no use without crops |
| ITEM_AXE | Tools | No equipment system |
| ITEM_PICKAXE | Tools | No equipment system |
| ITEM_TANNIN | Animals + Leather | No hides to tan |
| ITEM_MEDICINE | Medical | No health/injury system |
| ITEM_FISH_TRAP | Fishing | No fishing system |
| ITEM_CONCRETE | Water as input | No water item system |

**Recommendation:** Don't implement these items until their sinks exist. Focus on closed loops first:
- Stone loop (complete)
- Wood -> Planks -> Building (ready)
- Clay -> Bricks -> Building (ready)
- Wood -> Charcoal -> Fuel (ready)
- Leaves -> Fiber -> Rope/Cloth -> Crafting (ready once CELL_TALL_GRASS exists)
- Withies -> Baskets/Fencing (ready once ITEM_WITHIES source exists)
