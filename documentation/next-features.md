# Next Features Roadmap

Ideas for features that build on existing systems (dirt tile, water, entropy/ground wear, emergent paths).

## Quick Wins

### Trees / Brush overlay
- Spawns on grass tiles
- Chopped → produces wood/fuel items
- Uses existing haul/stockpile system
- **Jobs**: Woodcutter, Fuel Gatherer
- **Entropy**: trees slowly regrow on grass (like dirt→grass)

### Wild plants (berries, wild grain)
- Overlay on grass tiles
- Gatherer harvests → food items + seeds
- **Jobs**: Gatherer
- **Entropy**: bushes deplete when harvested, slowly regrow

### Dirty floors + Cleaner job
- Movers walking from dirt onto built floor → floor gains dirt overlay
- Uses wear grid pattern (dirtGrid for floors)
- **Jobs**: Cleaner sweeps dirty floors
- Creates maintenance pressure on buildings

### Item types
- Stone (from mining) → building, tools
- Wood (from trees) → building, fuel, tools
- Food (berries, crops) → eating, cooking
- Fuel (wood/brush) → fires

## Medium Effort

### Mining drops stone
- Mine wall → get stone item → build floor elsewhere
- Instant payoff, creates building material loop
- Dirty rubble on floor until cleaned

### Firepit + Fire system
- Firepit buildable on floor/dirt
- Has `fuel` and `lit` state, consumes fuel per tick
- **Jobs**: Firekeeper (refuel), Cook (requires lit fire)
- **Entropy**: fires go out without maintenance

### Soil fertility grid
- Similar to wearGrid pattern
- Grass near water = higher fertility
- Trampled paths = lower fertility
- **Later**: farming yields depend on fertility
- **Entropy**: harvesting depletes, fallow fields recover

### Gather / Clean zone designations
- Gather zone: only collect plants/items from designated area
- Cleaning zone: prioritize cleaning this area
- Dumping zone: drop unwanted items here
- Extends existing designation system

### Moisture / Irrigation
- Tiles near water get "moist" tag
- Affects plant growth speed
- Could use spread mechanic like water pressure
- **Jobs**: Water Carrier (irrigates dry fields)

## Bigger Undertakings

### Channels (dig from above)
- Dig down from z+1 to create holes/ramps
- Creates: air (nothing below), ramp (access), or pit (danger)
- Different designation mode for Miner job
- Ties into multi-floor system

### Floor removal / building
- **Built floor**: can be removed, get material back, reveals dirt below
- **Dig dirt floor**: creates channel/hole to z-level below
- **Natural stone floor**: requires mining
- Loop: mine wall → stone item → build floor elsewhere

### Stairs (diagonal z-transition)
- Two cells: bottom (x,y,z) and top (x+1,y,z+1)
- Walking onto stair bottom transitions to stair top
- Like ladder but diagonal in x/z space
- Needs direction (N/S/E/W facing)
- Easier than true ramps, similar visual effect

### Real ramps
- True diagonal 3D movement (x+1, z+1 in one move)
- Ramp direction matters (can only walk up from one side)
- Pathfinding adds neighbors like (x+1, y, z+1)
- Visual: how to show ramp going up vs down in top-down?
- Collision: ramp occupies two z-levels
- **Hardest to implement** - consider stairs first

## The Entropy Thread

Ground wear system is a template for:
- **Fertility**: harvesting depletes, time restores
- **Fire fuel**: burns down, needs refueling
- **Tool durability**: use depletes, crafting replaces
- **Food freshness**: ticks toward spoilage
- **Floor cleanliness**: use dirties, cleaning restores

## Suggested Implementation Order

1. Item types (stone, wood, food) - needed for everything else
2. Trees/Brush → woodcutter job, gives fuel
3. Firepit + Firekeeper → uses fuel, unlocks cooking
4. Wild plants (berries) → gatherer job, early food loop
5. Mining drops stone → building material loop
6. Dirty floors + Cleaner → entropy theme
7. Soil fertility grid → location choices matter
8. Farm plots → planting/harvesting with seeds
9. Gather/Clean zones → extends designations
10. Channels → multi-z digging
11. Stairs → diagonal z-transition
12. Ramps → full 3D movement (maybe never needed)
