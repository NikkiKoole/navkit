# Trees System - COMPLETED

## Completed Features

### Core Tree System (Previously Done)
- Cell types: CELL_SAPLING, CELL_TREE_TRUNK, CELL_TREE_LEAVES
- Tree growth: sapling -> trunk -> leaves (cellular automaton)
- Leaf decay when disconnected from trunk
- Chopping designation (DESIGNATION_CHOP)
- Tree felling with wood drops (ITEM_WOOD)
- PlaceSapling() and TreeGrowFull() functions

### Sapling Item + Jobs
- ITEM_SAPLING item type added
- DESIGNATION_GATHER_SAPLING - gather saplings from cells
- DESIGNATION_PLANT_SAPLING - plant sapling items
- JOBTYPE_GATHER_SAPLING and JOBTYPE_PLANT_SAPLING
- WorkGiver_GatherSapling and WorkGiver_PlantSapling
- UI actions: ACTION_WORK_GATHER_SAPLING (key S), ACTION_WORK_PLANT_SAPLING (key P)
- Stockpile filtering for saplings (key T to toggle)

### Sapling Drops from Chopped Trees
- FellTree() now drops saplings based on leaf count
- ~1 sapling per 5 leaves (minimum 1 if any leaves)
- Saplings scattered around tree base using golden angle distribution

### Organic Tree Shapes (Oak Style)
- Variable tree heights (MIN_TREE_HEIGHT=3 to MAX_TREE_HEIGHT=6)
- Position-based deterministic randomness (PositionHash)
- Oak-style canopy with variable radius based on trunk height
- Multi-level canopy (1-2 levels above trunk top)
- Organic edges with random variation
- Sparse "skirt" at trunk level (~60% fill)

### Sapling Regrowth (Entropy)
- Natural sapling spawning on untrampled grass (tall grass surface)
- Integrated with ground wear system (UpdateGroundWear)
- Configurable parameters:
  - saplingRegrowthEnabled (default: true)
  - saplingRegrowthChance (default: 5 per 10000 = 0.05%)
  - saplingMinTreeDistance (default: 4 tiles)
- Respects existing trees/saplings (minimum distance check)

### Growth Blocking
- Items on tile prevent sapling spawn (QueryItemAtTile check in groundwear.c)
- Items on tile prevent sapling->trunk growth (QueryItemAtTile check in trees.c)
- Sapling trampling: movers destroy saplings they walk through (TrampleGround)

### UI: Growth Time Setting
- Trees section in UI panel (sectionTrees)
- Runtime configurable parameters:
  - saplingGrowTicks (default: 100, range: 10-1000)
  - trunkGrowTicks (default: 50, range: 5-500)
  - saplingRegrowthEnabled toggle
  - saplingRegrowthChance (range: 0-100)
  - saplingMinTreeDistance (range: 1-10)

## Files Modified
- src/entities/items.h - ITEM_SAPLING enum
- src/entities/item_defs.c - sapling item definition
- src/world/designations.h - gather/plant sapling designations
- src/world/designations.c - designation functions, FellTree sapling drops
- src/entities/jobs.h - job types and function declarations
- src/entities/jobs.c - job drivers and WorkGivers
- src/core/input_mode.h - UI actions
- src/core/input_mode.c - bar items and action names
- src/core/input.c - key bindings and stockpile filter toggle
- src/render/tooltips.c - stockpile filter display
- src/simulation/trees.h - runtime growth parameters
- src/simulation/trees.c - organic tree shapes, item blocking
- src/simulation/groundwear.h - sapling regrowth parameters
- src/simulation/groundwear.c - sapling regrowth, trampling
- src/game_state.h - sectionTrees
- src/main.c - sectionTrees initialization
- src/render/ui_panels.c - Trees UI section
