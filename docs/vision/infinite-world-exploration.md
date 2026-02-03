# Transforming a Static Colony Sim into an Infinite Open World

Research into strategies for adding Cataclysm DDA-style exploration to a Dwarf Fortress-like colony simulation. Core challenges: memory, performance, consistency, and persistence.

---

## 1. World Structure: Chunk-Based Architecture

The universal solution is a **multi-layered chunk system**:

### Overmap (Macro Scale)
- A coarse grid where each cell represents a large area (e.g., a town, forest biome, dungeon)
- Generated deterministically from a world seed using noise functions
- Stored as lightweight metadata (terrain type, settlement flag, danger level)
- [Cataclysm DDA uses this exact approach](https://readmex.com/en-US/CleverRaven/Cataclysm-DDA/world-generationd3520819-2964-4ed3-9ff4-af5ba7792edd)

### Local Map (Micro Scale)  
- Detailed tile-by-tile data for the area the player is in
- Only a few chunks loaded at once (typically 9 around player)
- [This is the standard approach for infinite worlds](https://spin.atomicobject.com/2015/05/03/infinite-procedurally-generated-world/)

### Implementation Notes
The existing chunk-based grid architecture can be extended:
- Add an `Overmap` struct holding macro-level terrain types
- Each overmap cell maps to a local chunk that gets generated/loaded on demand

---

## 2. Persistence: Delta Storage

The key insight: **don't store unmodified chunks**.

### Three-Tier Persistence
1. **Procedural chunks**: Regenerated identically from seed - zero storage cost
2. **Modified chunks**: Store only the *differences* (deltas) from procedural baseline
3. **Heavily modified chunks**: Store full chunk data

[GameDev.net discussion on storing player changes](https://www.gamedev.net/forums/topic/647176-storing-player-changes-to-procedurally-generated-worlds/5089840/)

### Implementation Pattern
```c
ChunkData:
  - seed: u64 (for regeneration)
  - deltas: HashMap<(x,y,z), CellModification>
  - dirty_flag: bool

on_load(chunk_pos):
  base = generate_from_seed(chunk_pos, world_seed)
  apply_deltas(base, stored_deltas[chunk_pos])
  
on_save(chunk):
  if chunk.dirty_flag:
    diff = compute_delta(chunk, generate_from_seed(...))
    store(diff)  // Much smaller than full chunk
```

### Notes
Current save system saves the full world. Extension would:
- Generate a "pristine" reference chunk
- Diff against current state
- Only serialize the changes

---

## 3. Deterministic Generation: Seed Everything

[Every procedural system must be seeded consistently](https://grokipedia.com/page/Map_seed):

```c
chunk_seed = hash(world_seed, chunk_x, chunk_y, chunk_z)
```

Use a fast hash like **FNV-1a** or **MurmurHash** (not cryptographic, just well-distributed).

Existing Perlin noise terrain generators already support seeding. The key is ensuring:
- Chunk boundaries align seamlessly
- Same seed → identical chunk every time
- No external state leaks into generation

---

## 4. Memory Management: Spatial Hashing + Streaming

### Spatial Partitioning
[Spatial hashing](https://gameprogrammingpatterns.com/spatial-partition.html) is critical for large worlds:
- Hash entities to grid cells: `cell = (x / cell_size, y / cell_size)`
- Only query nearby cells for collision/interaction
- Existing mover and items spatial grids can be extended

### Streaming Strategy
Keep a "ring" of chunks:
- **Active zone** (1 chunk): Full simulation with A*/pathfinding
- **Buffer zone** (8 chunks around): Loaded but simplified simulation
- **Unloaded**: Regenerated on demand, deltas stored to disk

[Chunk streaming techniques](https://appijo.com/?p=268)

---

## 5. Simulation LOD: The Hard Problem

Full simulation of distant areas is impossible, but there are options:

### Option A: "Fake It" (Kenshi/Most Games)
- Don't simulate offscreen at all
- When player returns, "fast-forward" using statistical models
- NPCs calculate their probable state based on elapsed time
- [Most open world games use this approach](https://www.quora.com/How-do-open-world-game-manage-to-have-NPCs-do-different-things-at-different-times-without-overloading-the-game-with-concurrent-scripts)

### Option B: Abstract Simulation (Dwarf Fortress World Gen)
- Run a simplified "strategy game" for distant regions
- Track population counts, resource totals, faction relationships
- No individual pathfinding or detailed physics
- [Dwarf Fortress history generation works this way](https://dwarffortresswiki.org/index.php/World_generation)

### Option C: AI LOD (Research Approach)
- [Research shows 99% CPU reduction possible](https://www.researchgate.net/publication/221252089_Level_of_Detail_AI_for_Virtual_Characters_in_Games_and_Simulation)
- Nearby: Full pathfinding, detailed behavior
- Medium: Simplified state machines, less frequent updates  
- Far: Statistical/probabilistic behavior, aggregate populations

### Recommendation
Start with **Option A** (fake it), with groundwork for **Option B**:
- Store per-chunk "settlement state" (population, resources, faction)
- When unloading: snapshot NPC state to abstract representation
- When loading: restore NPCs based on abstract state + elapsed time

---

## 6. Entity-Component System (ECS) Considerations

[ECS 2.0 architectures](https://www.daydreamsoft.com/blog/ecs-2-0-data-oriented-micro-kernel-architectures-for-massive-persistent-game-worlds) are built for this:

Current architecture in `entities/` (movers, jobs, items) could evolve to:
- **Chunk-local entity storage**: Entities belong to chunks
- **Streaming archetypes**: Load/unload entity data with chunks  
- **System locality**: Jobs/pathfinding only run on loaded chunks

This is less urgent than the persistence/streaming work but would help with memory.

---

## Recommended Implementation Order

| Priority | System | Effort | Impact |
|----------|--------|--------|--------|
| 1 | **Overmap structure** | Medium | Enables infinite world |
| 2 | **Deterministic chunk generation** | Low | Required for persistence |
| 3 | **Delta-based persistence** | Medium | Keeps save files small |
| 4 | **Chunk streaming** | High | Memory management |
| 5 | **Abstract settlement simulation** | High | Living world feeling |
| 6 | **AI LOD system** | High | Performance at scale |

---

## Addressing Specific Challenges

### Memory
- Delta storage dramatically reduces save size
- Stream only 9-25 chunks max
- Spatial hashing for entity queries

### Performance  
- Simulation LOD is mandatory - can't pathfind entire world
- Chunk-based HPA* scales well
- Consider simulation tick budget: X ms/frame for off-screen

### Consistency
- Seed-based determinism is solved problem
- Store chunk-level state hashes for validation
- Delta format must be stable across versions

### Persistence
- Compress deltas (saves already use gzip)
- Lazy write: only flush dirty chunks
- Consider SQLite for random chunk access vs flat files

---

## Key Techniques Summary

### Cataclysm DDA Architecture
- **OvermapBuffer**: Manages loading/unloading of overmap data chunks as player explores
- **MapBuffer**: Manages loading/unloading of detailed map data for individual overmap cells
- **JSON Definitions**: Overmap terrain, special locations, mapgen rules, regional settings
- World is persistent between characters (same world, different playthrough)

### Minecraft-Style Chunk Seeds
```
chunk_seed = hash(world_seed + chunk_x + chunk_y)
```
- Deterministic regeneration from seed
- Only modified chunks need storage
- Players can share seeds for identical worlds

### No Man's Sky Approach
- Single random seed for entire universe
- All players see identical content at same coordinates
- Content generated on-the-fly as you visit
- Minimal server storage needed

### Dwarf Fortress History Generation
- "Giant zero-player strategy game with thousands of agents"
- History is a log of simulation events
- Abstract groups can affect named creatures
- Simulation of narratively interesting parts, not atoms

---

## Sources

- [CDDA World Generation Architecture](https://readmex.com/en-US/CleverRaven/Cataclysm-DDA/world-generationd3520819-2964-4ed3-9ff4-af5ba7792edd)
- [Building Infinite Procedural Worlds](https://spin.atomicobject.com/2015/05/03/infinite-procedurally-generated-world/)
- [Storing Player Changes to Procedural Worlds](https://www.gamedev.net/forums/topic/647176-storing-player-changes-to-procedurally-generated-worlds/5089840/)
- [Map Seeds and Determinism](https://grokipedia.com/page/Map_seed)
- [Spatial Partition Pattern](https://gameprogrammingpatterns.com/spatial-partition.html)
- [Dwarf Fortress World Generation](https://dwarffortresswiki.org/index.php/World_generation)
- [LOD AI for Virtual Characters](https://www.researchgate.net/publication/221252089_Level_of_Detail_AI_for_Virtual_Characters_in_Games_and_Simulation)
- [ECS 2.0 for Persistent Worlds](https://www.daydreamsoft.com/blog/ecs-2-0-data-oriented-micro-kernel-architectures-for-massive-persistent-game-worlds)
- [Chunk Streaming Techniques](https://appijo.com/?p=268)
- [Three Approaches to Procedural Generation](https://www.gamedeveloper.com/design/procedural-world-generation-the-simulation-functional-and-planning-approaches)
- [Procedural Generation Complete Guide](https://generalistprogrammer.com/procedural-generation-games)
- [Roguelike Overmap Design](https://www.gridsagegames.com/blog/2020/11/exploring-concept-terminal-roguelike-overmap/)

---

## 7. World-Bound vs Entity-Bound Simulation

A fundamental architectural distinction for streaming worlds.

### Two Categories of Simulation

#### World-Bound (Cellular Automata)
Water, fire, smoke, vegetation growth, erosion, temperature...

**Characteristics:**
- State lives in the grid cells themselves
- Neighbors affect neighbors (local rules)
- Can run without any entities present
- Deterministic given initial state
- "The world simulating itself"

#### Entity-Bound (Agents)
Movers, jobs, pathfinding, AI decisions, inventory...

**Characteristics:**
- State lives in discrete entities
- Entities query the world, don't define it
- Need goals, memory, planning
- Can cross chunk boundaries
- "Things moving through the world"

---

### Why This Matters for Streaming

#### World-Bound is Easier

When you unload a chunk:
- Snapshot the cell states (water levels, fire fuel, growth stage)
- On reload: restore snapshot, resume automata
- Or: don't snapshot, just regenerate + apply time-based approximation

The key insight: **cellular automata are spatially contained**. Fire in chunk A doesn't care about chunk B (except at boundaries).

```c
// Unloaded chunk approach:
// - Store: water_level[x,y,z], fire_fuel[x,y,z], plant_growth[x,y,z]
// - On reload: resume from snapshot
// - Optional: fast-forward N ticks with simplified rules
```

Boundary problem is solvable - you simulate a 1-cell border overlap, or accept minor discontinuities.

#### Entity-Bound is Hard

When you unload a chunk with a mover in it:
- Where does the mover go?
- What if their job target is in a loaded chunk?
- What if they're mid-path crossing multiple chunks?
- What about their social relationships, memory, inventory?

Entities have **identity and continuity** that cells don't.

---

### Approaches for World-Bound Simulation in Streaming World

**Option 1: Only simulate loaded chunks**
- Simple, cheap
- Discontinuity when player returns (water froze in time)
- Acceptable for many games

**Option 2: Time-skip approximation**
```c
on_chunk_load(chunk, time_since_unload):
    restore_snapshot(chunk)
    // Don't run 10000 water ticks - approximate
    approximate_water_flow(chunk, time_since_unload)
    approximate_fire_spread(chunk, time_since_unload)
    approximate_plant_growth(chunk, time_since_unload)
```

For water: calculate equilibrium state directly (water finds its level)
For fire: probabilistic burn-out based on fuel + time
For plants: growth_stage += time * growth_rate

**Option 3: Coarse background simulation**
- Run automata at 1/10th resolution for unloaded chunks
- Or run every Nth tick
- More accurate, more CPU

### Approaches for Entity-Bound Simulation in Streaming World

**Option 1: Entities stick to loaded chunks**
- Unloaded chunk = entities freeze/despawn
- Simple but breaks immersion
- NPCs only exist near player

**Option 2: Abstract entity state when unloading**
```c
on_chunk_unload(chunk):
    for each mover in chunk:
        mover.abstract_state = {
            .last_position = mover.pos,
            .last_activity = mover.current_job,
            .needs_snapshot = mover.needs,
            .timestamp = now()
        }
        move_to_abstract_pool(mover)

on_chunk_load(chunk):
    for each abstract_mover belonging to chunk:
        restore_mover(abstract_mover)
        fast_forward_needs(abstract_mover, time_elapsed)
        // Place somewhere sensible
        mover.pos = find_valid_position_near(abstract_mover.last_position)
```

**Option 3: Let entities migrate (hard mode)**
- Entities can move between chunks while unloaded
- Track at overmap level: "5 dwarves traveling from A to B"
- Resolve actual positions on load
- This is what Dwarf Fortress worldgen does with armies/migrants

---

### The Boundary Problem

#### For Cellular Automata
Water at chunk edge needs to know neighbor chunk's water level.

Solutions:
- **Ghost cells**: Load 1-cell border of adjacent chunks
- **Deferred updates**: Queue cross-boundary flows, apply on load
- **Accept discontinuity**: It's usually fine

#### For Entities
Mover pathfinding across chunk boundaries.

Solutions:
- **HPA* already solves this**: Abstract graph connects chunks
- **Path caching**: Store the abstract path, resolve details when chunks load
- **Handoff zones**: Entity enters "traveling" state between chunks

---

### Practical Architecture

```
┌─────────────────────────────────────────────────────┐
│                    OVERMAP                          │
│  (always in memory, lightweight)                    │
│  - terrain types, settlement flags                  │
│  - abstract entity counts per cell                  │
│  - coarse resource totals                           │
└─────────────────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│   LOADED    │  │   LOADED    │  │  UNLOADED   │
│   CHUNK     │  │   CHUNK     │  │   CHUNK     │
├─────────────┤  ├─────────────┤  ├─────────────┤
│ Full grid   │  │ Full grid   │  │ Snapshot:   │
│ cells       │  │ cells       │  │ - cell data │
├─────────────┤  ├─────────────┤  │ - timestamp │
│ Water/fire  │  │ Water/fire  │  │             │
│ simulation  │  │ simulation  │  │ Abstract:   │
│ (per tick)  │  │ (per tick)  │  │ - N movers  │
├─────────────┤  ├─────────────┤  │ - resources │
│ Entities:   │  │ Entities:   │  │ - last jobs │
│ - movers    │  │ - movers    │  └─────────────┘
│ - items     │  │ - items     │
│ - jobs      │  │ - jobs      │
│ (full sim)  │  │ (full sim)  │
└─────────────┘  └─────────────┘
```

---

### System Classification for navkit

| System | Type | Streaming Approach |
|--------|------|-------------------|
| Water pressure/flow | World-bound | Snapshot + equilibrium approximation |
| Fire spread | World-bound | Snapshot + probabilistic burnout |
| Smoke | World-bound | Don't persist (ephemeral) |
| Ground wear | World-bound | Snapshot directly (slow-changing) |
| Movers | Entity-bound | Abstract state + fast-forward needs |
| Jobs | Entity-bound | Pause/resume or abstract "working" state |
| Items | Hybrid | Stay with chunk (world-bound) but track ownership (entity-bound) |
| Stockpiles | World-bound | Snapshot contents |
| Pathfinding | Entity-bound | HPA* handles cross-chunk naturally |

---

### The Hybrid Cases

**Items** are interesting - they're placed in the world (world-bound) but have ownership, reservations, and get carried (entity-bound).

Solution: Items stay with their chunk. If a mover carrying an item crosses to unloaded chunk:
- Item goes to abstract inventory on the abstract mover
- On reload: item reappears with mover

**Jobs** spanning chunks:
- Hauling from chunk A to stockpile in chunk B
- If A unloads mid-haul: job pauses, mover remembers intent
- On reload: resume or reassign

---

### Summary: World-Bound vs Entity-Bound

| | World-Bound | Entity-Bound |
|---|---|---|
| State location | Grid cells | Discrete objects |
| Scope | Local (neighbors) | Global (goals anywhere) |
| Identity | None (cells are fungible) | Strong (this specific dwarf) |
| Streaming | Snapshot + approximate | Abstract + restore |
| Cross-chunk | Boundary sync | Path/goal tracking |

**The world simulates itself. Entities simulate their goals within the world.**

---

## 8. Memory Analysis: Current State & Infinite World Projections

Detailed memory breakdown based on actual navkit data structures.

### Current Static Allocation (Max Size: 512×512×16)

#### World-Bound Data (Grid Arrays)

| Array | Element Size | Dimensions | Total Size |
|-------|-------------|------------|------------|
| `grid[z][y][x]` (CellType enum) | 4 bytes | 16×512×512 | **16.0 MB** |
| `cellFlags[z][y][x]` | 1 byte | 16×512×512 | **4.0 MB** |
| `waterGrid[z][y][x]` (WaterCell) | 2 bytes | 16×512×512 | **8.0 MB** |
| `fireGrid[z][y][x]` (FireCell) | 2 bytes | 16×512×512 | **8.0 MB** |
| `smokeGrid[z][y][x]` (SmokeCell) | 1 byte | 16×512×512 | **4.0 MB** |
| `designations[z][y][x]` | ~16 bytes | 16×512×512 | **64.0 MB** |
| `nodeData[z][y][x]` (AStarNode) | ~20 bytes | 16×512×512 | **80.0 MB** |

**World-bound subtotal: ~184 MB**

#### Pathfinding Infrastructure

| Array | Element Size | Count | Total Size |
|-------|-------------|-------|------------|
| `entrances[]` | 20 bytes | 16,384 | **320 KB** |
| `graphEdges[]` | 12 bytes | 262,144 | **3.0 MB** |
| `path[]` | 12 bytes | 131,072 | **1.5 MB** |
| `abstractNodes[]` | ~20 bytes | 16,386 | **320 KB** |
| `chunkDirty[z][y][x]` | 1 byte | 16×64×64 | **64 KB** |
| `ladderLinks[]` | ~28 bytes | 1,024 | **28 KB** |
| `rampLinks[]` | ~32 bytes | 1,024 | **32 KB** |
| `jpsLadderGraph` | varies | 1 | **~2 MB** |

**Pathfinding subtotal: ~7.3 MB**

#### Entity-Bound Data

| Array | Element Size | Count | Total Size |
|-------|-------------|-------|------------|
| `movers[]` | ~12,400 bytes* | 10,000 | **124.0 MB** |
| `items[]` | ~32 bytes | 25,000 | **800 KB** |
| `jobs[]` | ~64 bytes | 10,000 | **640 KB** |
| `stockpiles[]` | ~10 KB | 64 | **640 KB** |
| `blueprints[]` | ~40 bytes | 1,000 | **40 KB** |
| `gatherZones[]` | ~24 bytes | 32 | **768 bytes** |

*Mover size breakdown:
- Base Mover fields: ~100 bytes
- `path[MAX_MOVER_PATH]`: 1024 × 12 bytes = 12,288 bytes
- **Total per mover: ~12,400 bytes**

**Entity-bound subtotal: ~126 MB**

#### Spatial Grids (Dynamic)

| Structure | Approximate Size |
|-----------|-----------------|
| `moverGrid` (spatial hash) | ~2-4 MB |
| `itemGrid` (spatial hash) | ~2-4 MB |

**Spatial grids subtotal: ~6 MB**

---

### Current Total: ~323 MB

This aligns with observed ~350 MB runtime (difference is runtime overhead, raylib, stack, etc.).

---

### Breakdown by Category

```
World-bound grids:     184 MB  (57%)
Entity arrays:         126 MB  (39%)  ← movers dominate due to inline paths
Pathfinding:             7 MB  (2%)
Spatial grids:           6 MB  (2%)
─────────────────────────────────────
Total:                 323 MB
```

**The elephant in the room**: Each mover carries a 12KB path buffer. With 10,000 movers, that's 120MB just for path storage.

---

### Memory for Infinite World: Chunk-Based Projections

Assume we stream chunks and keep N chunks loaded at once.

#### Chunk Size Options

| Chunk Size | Cells per Chunk (×16 z-levels) |
|------------|-------------------------------|
| 16×16×16 | 4,096 |
| 32×32×16 | 16,384 |
| 64×64×16 | 65,536 |

#### Per-Chunk Memory (World-Bound Only)

For a **32×32×16** chunk:

| Data | Per Cell | Per Chunk |
|------|----------|-----------|
| CellType | 4 bytes | 64 KB |
| cellFlags | 1 byte | 16 KB |
| waterGrid | 2 bytes | 32 KB |
| fireGrid | 2 bytes | 32 KB |
| smokeGrid | 1 byte | 16 KB |
| designations | 16 bytes | 256 KB |
| **Total** | **26 bytes** | **416 KB** |

Without designations (make sparse): **160 KB per chunk**

Without A* nodeData (compute on demand): saves 320 KB per chunk

#### Loaded Chunk Scenarios

| Chunks Loaded | Chunk Size | World-Bound Memory |
|---------------|------------|-------------------|
| 9 (3×3) | 32×32×16 | 3.7 MB |
| 25 (5×5) | 32×32×16 | 10.4 MB |
| 9 (3×3) | 64×64×16 | 14.8 MB |
| 25 (5×5) | 64×64×16 | 41.6 MB |

**This is a massive reduction from 184 MB!**

---

### Entity Memory in Streaming World

#### The Mover Path Problem

Current: 12KB per mover × 10,000 = 120MB

Solutions:
1. **Dynamic path allocation**: Only allocate path when needed, free when done
2. **Shorter max path**: 256 points (3KB) is usually enough for local navigation
3. **HPA* abstract paths**: Store abstract path (entrance indices), refine locally
4. **Path sharing**: Multiple movers to same destination share path prefix

With 256-point paths: 3KB × 10,000 = **30 MB** (saves 90 MB)

#### Entity Caps per Chunk

For streaming, cap entities per loaded area:

| Entity Type | Per Chunk | 9 Chunks | 25 Chunks |
|-------------|-----------|----------|-----------|
| Movers (3KB path) | 100 | 2.7 MB | 7.5 MB |
| Items | 500 | 144 KB | 400 KB |
| Jobs | 200 | 115 KB | 320 KB |

---

### Projected Streaming World Memory Budget

#### Minimal Config (9 chunks, 32×32×16)

| Category | Memory |
|----------|--------|
| World grids (9 chunks) | 3.7 MB |
| Movers (900, short paths) | 2.7 MB |
| Items (4,500) | 144 KB |
| Pathfinding (per-chunk HPA) | 2 MB |
| Overmap (lightweight) | 1 MB |
| Spatial grids | 1 MB |
| **Total** | **~11 MB** |

#### Comfortable Config (25 chunks, 32×32×16)

| Category | Memory |
|----------|--------|
| World grids (25 chunks) | 10.4 MB |
| Movers (2,500, short paths) | 7.5 MB |
| Items (12,500) | 400 KB |
| Pathfinding | 4 MB |
| Overmap | 2 MB |
| Spatial grids | 2 MB |
| **Total** | **~26 MB** |

#### Generous Config (25 chunks, 64×64×16)

| Category | Memory |
|----------|--------|
| World grids (25 chunks) | 41.6 MB |
| Movers (2,500, short paths) | 7.5 MB |
| Items (12,500) | 400 KB |
| Pathfinding | 8 MB |
| Overmap (larger) | 4 MB |
| Spatial grids | 4 MB |
| **Total** | **~66 MB** |

---

### Key Optimizations Needed

#### 1. Sparse Designations
Current: 64 MB for designation grid (mostly empty)
Solution: HashMap<(x,y,z), Designation> = ~1 KB per 100 active designations

#### 2. On-Demand A* NodeData
Current: 80 MB static allocation
Solution: Allocate per-search, reuse buffer, or use chunk-local arrays

#### 3. Dynamic Mover Paths
Current: 12 KB per mover (mostly wasted)
Solution: Pool allocator, shorter paths, or pointer to shared path

#### 4. Compressed Cellular Automata
Current: WaterCell = 2 bytes, FireCell = 2 bytes, SmokeCell = 1 byte
Already efficient! Could pack tighter but diminishing returns.

#### 5. Delta Storage for Persistence
Unmodified chunks: 0 bytes stored
Modified chunks: Only store deltas (~1-10% of full chunk typically)

---

### Memory Bandwidth Considerations

#### Chunk Load/Unload Cost

| Operation | Data Size | At 1GB/s disk | At 100MB/s disk |
|-----------|-----------|---------------|-----------------|
| Load 32×32×16 chunk | 160 KB | 0.16 ms | 1.6 ms |
| Load 64×64×16 chunk | 640 KB | 0.64 ms | 6.4 ms |
| Generate chunk (CPU) | N/A | 1-5 ms | 1-5 ms |

Generation is likely slower than loading, so caching helps.

#### Per-Frame Memory Access

Simulation touches every cell in loaded chunks:
- 9 chunks × 32×32×16 = 147,456 cells
- 25 chunks × 32×32×16 = 409,600 cells

At 60 FPS with water+fire+smoke:
- ~3-5 bytes touched per cell per system
- 9 chunks: ~2 MB/frame = 120 MB/s
- 25 chunks: ~6 MB/frame = 360 MB/s

This fits comfortably in L3 cache for modern CPUs.

---

### Summary: Memory Comparison

| Scenario | Memory | vs Current 350MB |
|----------|--------|------------------|
| Current static (512×512×16) | ~323 MB | baseline |
| Streaming 9×32×32 chunks | ~11 MB | **97% reduction** |
| Streaming 25×32×32 chunks | ~26 MB | **92% reduction** |
| Streaming 25×64×64 chunks | ~66 MB | **80% reduction** |

The infinite world can actually use **less memory** than the current static allocation, while supporting unlimited exploration. The key tradeoffs are:

1. **Complexity**: Chunk load/unload logic, delta persistence
2. **Latency**: Brief hitch when loading new chunks (mitigate with async/background loading)
3. **Entity limits**: Cap movers/items per loaded area (usually fine for gameplay)

---

## 9. Deep Dive: Overmap Generation

### What is an Overmap?

An overmap is a **macro-scale representation** of your game world. Each cell in the overmap represents a larger area (a "chunk" or "local map") that gets generated in detail when the player visits.

Think of it as:
- **Overmap cell** = "there's a forest here"
- **Local map** = the actual trees, rocks, creatures in that forest

The overmap answers: *What's the world made of?* The local mapgen answers: *What does it look like up close?*

---

### Generation Order Matters

Based on [CDDA's documentation](https://docs.cataclysmdda.org/JSON/OVERMAP.html), generation follows a strict sequence:

```
1. Connections from neighboring overmaps (continuity)
2. Rivers and lakes (water features)
3. Forests, swamps, ravines (natural terrain)
4. Cities (settlements)
5. Roads and trails (connections)
6. Special locations (dungeons, labs, unique sites)
7. Monster groups and details
```

**Why this order?**
- Water defines major geographic constraints
- Natural terrain fills the "background"
- Cities need to avoid water/mountains
- Roads connect cities (need cities first)
- Specials fill remaining interesting spots

---

### Core Techniques

#### 1. Noise-Based Terrain Fields

[Dwarf Fortress](https://dwarffortresswiki.org/index.php/Advanced_world_generation) seeds basic values along a grid and fills fractally:

```
elevation    → mountains, plains, valleys
rainfall     → forests, deserts, swamps
temperature  → snow, temperate, tropical
drainage     → rivers, lakes, wetlands
```

[Multiple octaves of noise](https://www.redblobgames.com/maps/terrain-from-noise/) create natural-looking variation:

```c
float terrain_height(float x, float y) {
    float value = 0;
    float amplitude = 1.0;
    float frequency = 1.0;
    
    for (int octave = 0; octave < NUM_OCTAVES; octave++) {
        value += amplitude * noise2d(x * frequency, y * frequency);
        amplitude *= persistence;  // typically 0.5
        frequency *= lacunarity;   // typically 2.0
    }
    return value;
}
```

**Biome assignment** combines multiple noise layers:

```c
Biome get_biome(float x, float y) {
    float elevation = elevation_noise(x, y);
    float moisture = moisture_noise(x, y);
    float temperature = temperature_noise(x, y);
    
    // High elevation = mountain/snow
    if (elevation > 0.8) return temperature < 0.3 ? SNOW : MOUNTAIN;
    // Low elevation + high moisture = swamp
    if (elevation < 0.3 && moisture > 0.6) return SWAMP;
    // etc...
}
```

---

#### 2. Voronoi-Based Regions

[Voronoi diagrams](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/) partition space into organic-looking regions:

```
1. Scatter seed points (Poisson disk for even spacing)
2. Each cell = area closest to its seed
3. Assign properties per cell (biome, faction, etc.)
4. Use Lloyd relaxation for smoother shapes
5. Add noise to edges for organic boundaries
```

**Advantages:**
- Natural-looking region shapes
- Easy to assign properties per-region
- Good for political boundaries, biome zones

**Combine with noise:**
```c
// Voronoi determines region, noise adds local variation
Region region = voronoi_lookup(x, y);
float local_variation = noise2d(x * 10, y * 10) * 0.2;
float final_elevation = region.base_elevation + local_variation;
```

---

#### 3. Settlement Placement

[Settlements need geographic logic](https://jdookeran.medium.com/day-11-settlement-generator-1dc29ea0be18):

**Good settlement locations:**
- River mouths (trade)
- Coastal areas (ports)
- Defensible highlands
- Resource proximity
- Trade route intersections

**Poisson Disk Sampling** for minimum distance:

```c
// Bridson's algorithm ensures minimum spacing
PlacementResult place_settlements(int count, float min_distance) {
    List<Point> settlements;
    List<Point> active;
    
    // Start with random point
    active.add(random_valid_point());
    
    while (!active.empty() && settlements.count < count) {
        Point p = active.random_pick();
        
        for (int i = 0; i < k; i++) {  // k attempts
            Point candidate = random_point_in_annulus(p, min_distance, 2*min_distance);
            
            if (is_valid_location(candidate) && 
                no_neighbors_within(candidate, min_distance)) {
                settlements.add(candidate);
                active.add(candidate);
                break;
            }
        }
        
        if (no_valid_found) active.remove(p);
    }
    return settlements;
}
```

**Distance field scoring:**

```c
float score_location(int x, int y) {
    float score = 0;
    score += distance_to_water[x][y] < 3 ? 10 : 0;   // Near water is good
    score += distance_to_water[x][y] == 0 ? -100 : 0; // In water is bad
    score += elevation[x][y] > 0.7 ? -5 : 0;          // Mountains are bad
    score += distance_to_road[x][y] < 5 ? 5 : 0;      // Near roads is good
    return score;
}
```

---

#### 4. Road Networks

[Roads connect settlements](https://martindevans.me/game-development/2015/12/11/Procedural-Generation-For-Dummies-Roads/) with cost-based pathfinding:

**Terrain costs:**
```c
int road_cost(CellType terrain) {
    switch (terrain) {
        case FIELD:   return 1;   // Easy
        case FOREST:  return 3;   // Harder
        case SWAMP:   return 5;   // Difficult
        case MOUNTAIN: return 10; // Very hard
        case WATER:   return 999; // Need bridge
    }
}
```

**Connection strategies:**

1. **Minimum Spanning Tree** - connects all settlements with minimum total road
2. **Add some redundancy** - randomly add extra edges for loops
3. **Hierarchical** - major roads between cities, minor roads to villages

```c
void generate_road_network(List<Settlement> settlements) {
    // 1. Build MST for essential connectivity
    Graph mst = minimum_spanning_tree(settlements, road_cost);
    
    // 2. Add some extra edges (20% more) for redundancy
    for (each pair not in mst) {
        if (random() < 0.2 && distance < threshold) {
            add_road(pair);
        }
    }
    
    // 3. A* pathfind each road respecting terrain
    for (each edge in graph) {
        Path road = astar(edge.from, edge.to, road_cost);
        mark_cells_as_road(road);
    }
}
```

---

#### 5. Special Location Placement (CDDA-Style)

[CDDA uses sector-based placement](https://docs.cataclysmdda.org/JSON/OVERMAP.html):

```c
#define SECTOR_SIZE 15  // 180x180 overmap / 15 = 12x12 sectors = 144 sectors

void place_specials(Overmap* om) {
    List<Special> batch = build_eligible_specials(om);
    
    for (each sector in overmap) {
        if (sector.used) continue;
        
        Point pos = random_point_in_sector(sector);
        Special special = random_weighted_pick(batch);
        Rotation rot = random_rotation();
        
        if (can_place(special, pos, rot, om)) {
            place_special(special, pos, rot, om);
            sector.used = true;
            update_batch_counts(batch, special);
        }
    }
}
```

**Placement constraints:**
- `min_city_distance` / `max_city_distance`
- Required terrain types (forest, field, water edge)
- Avoid overlap with other specials
- Occurrence limits (min/max per overmap)

---

#### 6. Multi-Chunk Structures

Large structures (cities, dungeons) span multiple overmap cells:

**CDDA's approach - "Overmap Specials":**

```json
{
  "type": "overmap_special",
  "id": "large_mansion",
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "mansion_north" },
    { "point": [1, 0, 0], "overmap": "mansion_east" },
    { "point": [0, 1, 0], "overmap": "mansion_south" },
    { "point": [1, 1, 0], "overmap": "mansion_southeast" }
  ],
  "connections": [
    { "point": [0, -1, 0], "terrain": "road" }
  ]
}
```

**The challenge:** [Multi-chunk generation](https://github.com/Zylann/godot_voxel/issues/545) requires either:

1. **Pre-plan large structures** at overmap level (CDDA approach)
2. **Multi-pass generation** where chunks know about neighbors
3. **Separate "structure layer"** that sits above terrain chunks

---

### Practical Architecture for navkit

#### Overmap Data Structure

```c
typedef struct {
    uint8_t terrain_type;      // forest, plains, mountain, etc.
    uint8_t special_id;        // dungeon, town, etc. (0 = none)
    uint8_t road_flags;        // N/E/S/W road connections
    uint8_t river_flags;       // N/E/S/W river connections
    uint16_t settlement_id;    // which settlement this belongs to (0 = none)
    uint8_t danger_level;      // for spawning
    uint8_t flags;             // explored, has_player_changes, etc.
} OvermapCell;  // 8 bytes per cell

typedef struct {
    OvermapCell cells[OVERMAP_SIZE][OVERMAP_SIZE];
    uint64_t seed;
    int origin_x, origin_y;    // world position of this overmap
    // Settlement data
    Settlement settlements[MAX_SETTLEMENTS_PER_OVERMAP];
    int settlement_count;
} Overmap;
```

**Memory for 180×180 overmap:** 180 × 180 × 8 = **259 KB**

#### Generation Pipeline

```c
void generate_overmap(Overmap* om, uint64_t world_seed) {
    om->seed = hash(world_seed, om->origin_x, om->origin_y);
    RNG rng = seed_rng(om->seed);
    
    // 1. Terrain base layer (noise)
    for (y = 0; y < OVERMAP_SIZE; y++) {
        for (x = 0; x < OVERMAP_SIZE; x++) {
            float wx = om->origin_x * OVERMAP_SIZE + x;
            float wy = om->origin_y * OVERMAP_SIZE + y;
            om->cells[y][x].terrain_type = sample_biome(wx, wy, world_seed);
        }
    }
    
    // 2. Rivers (trace from high to low elevation)
    generate_rivers(om, &rng);
    
    // 3. Settlements (Poisson disk + scoring)
    place_settlements(om, &rng);
    
    // 4. Roads (connect settlements)
    generate_roads(om);
    
    // 5. Special locations (sector-based)
    place_specials(om, &rng);
    
    // 6. Ensure connectivity to neighbor overmaps
    connect_to_neighbors(om);
}
```

#### Chunk Generation from Overmap

```c
void generate_chunk(Chunk* chunk, Overmap* om, int local_x, int local_y) {
    OvermapCell* cell = &om->cells[local_y][local_x];
    uint64_t chunk_seed = hash(om->seed, local_x, local_y);
    
    switch (cell->terrain_type) {
        case TERRAIN_FOREST:
            generate_forest(chunk, chunk_seed);
            break;
        case TERRAIN_PLAINS:
            generate_plains(chunk, chunk_seed);
            break;
        // ...
    }
    
    // Overlay roads if present
    if (cell->road_flags) {
        carve_roads(chunk, cell->road_flags);
    }
    
    // Overlay rivers if present
    if (cell->river_flags) {
        carve_river(chunk, cell->river_flags);
    }
    
    // Generate special if present
    if (cell->special_id) {
        generate_special(chunk, cell->special_id, chunk_seed);
    }
}
```

---

### Wave Function Collapse Alternative

[WFC](https://github.com/mxgmn/WaveFunctionCollapse) can generate coherent overmaps with automatic transitions:

**Advantages:**
- Automatic biome transitions
- Guarantees connectivity
- Easy to add new tile types

**For biomes:**
```
Define adjacency rules:
- FOREST can neighbor: FOREST, PLAINS, MOUNTAIN_EDGE
- DESERT can neighbor: DESERT, PLAINS, BADLANDS
- OCEAN can neighbor: OCEAN, BEACH
- etc.

Run WFC → coherent map with natural transitions
```

**Hybrid approach:**
1. Use noise for broad regions (continent shape, temperature zones)
2. Use WFC within regions for detailed terrain
3. Use Poisson disk for settlement placement
4. Use A* for road routing

---

### Key Considerations for navkit

#### Z-Levels in Overmap

The game has 16 z-levels. Options:

1. **2D overmap, 3D chunks** - overmap is surface only, chunks extend up/down
2. **3D overmap** - track underground features (caves, dungeons) at overmap level
3. **Layered overmaps** - separate surface/underground/deep overmaps

CDDA uses approach 1 with special handling for multi-level structures.

#### Determinism

Critical for streaming worlds:
```c
// Same inputs → same outputs, always
uint64_t chunk_seed = hash(world_seed, chunk_x, chunk_y, chunk_z);
RNG rng = seed_rng(chunk_seed);
// Only use rng for this chunk's generation
// Never leak state between chunks
```

#### Neighbor Coherence

Features crossing chunk boundaries:
```c
// Rivers: trace at overmap level, carve at chunk level
// Roads: plan route at overmap level, carve at chunk level
// Large buildings: mark all cells at overmap level, generate pieces per chunk
```

---

### Summary: Recommended Approach

| Layer | Technique | Purpose |
|-------|-----------|---------|
| **Biome regions** | Multi-octave noise | Elevation, moisture, temperature |
| **Region boundaries** | Voronoi + noise edges | Political/biome zones |
| **Settlements** | Poisson disk + scoring | Evenly spaced, sensible locations |
| **Roads** | MST + A* routing | Connect settlements naturally |
| **Rivers** | Trace high→low elevation | Water features |
| **Specials** | Sector-based placement | Dungeons, unique locations |
| **Local terrain** | Seeded from overmap cell | Detailed chunk content |

---

### Overmap Generation Sources

- [CDDA Overmap Documentation](https://docs.cataclysmdda.org/JSON/OVERMAP.html)
- [Red Blob Games: Polygonal Map Generation](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/)
- [Red Blob Games: Terrain from Noise](https://www.redblobgames.com/maps/terrain-from-noise/)
- [Dwarf Fortress Advanced World Generation](https://dwarffortresswiki.org/index.php/Advanced_world_generation)
- [Procedural Road Generation](https://martindevans.me/game-development/2015/12/11/Procedural-Generation-For-Dummies-Roads/)
- [Poisson Disk Sampling](http://devmag.org.za/2009/05/03/poisson-disk-sampling/)
- [Wave Function Collapse](https://github.com/mxgmn/WaveFunctionCollapse)
- [Infinite WFC](https://marian42.de/article/infinite-wfc/)
- [Voronoi for Procedural Maps](https://squeakyspacebar.github.io/2017/07/12/Procedural-Map-Generation-With-Voronoi-Diagrams.html)
- [Settlement Placement](https://jdookeran.medium.com/day-11-settlement-generator-1dc29ea0be18)
- [Procedural City Generation](https://www.tmwhere.com/city_generation.html)
- [Multi-Chunk Structures Discussion](https://github.com/Zylann/godot_voxel/issues/545)

---

## 10. Gap Analysis: Research vs Current Codebase

A reality check comparing this research document against the actual navkit implementation.

### What Already Aligns Well

#### Chunked Grid + HPA* Foundation
The codebase already computes chunk IDs, entrances, and edges (`pathfinding.c`: `GetChunk`, `BuildEntrances`, `BuildGraph`). This matches the doc's "overmap + local chunk" architecture and provides the right abstraction for streaming later.

#### World-Bound vs Entity-Bound Split
The doc's classification maps cleanly to existing modules:
- **World-bound**: `water.c`, `fire.c`, `smoke.c`, `groundwear.c`, `grid.c`
- **Entity-bound**: `mover.c`, `jobs.c`, `items.c`, `stockpiles.c`

This is a solid conceptual split that will help when implementing streaming/LOD.

---

### Gaps the Doc Calls Out (Not Yet Implemented)

#### 1. No Overmap / Streaming System
The doc proposes an overmap and chunk streaming. Current grid is static and global (`grid.c` global arrays), with no chunk load/unload logic.

#### 2. Persistence is Full-World Only
`saveload.c` writes entire grids and entity arrays. There's no delta-based chunk storage or "pristine vs modified" diffing.

#### 3. Deterministic Chunk Seeding Not Implemented
Terrain generation uses random calls; there's no per-chunk seed or `--seed` plumbing in `main.c`. Regeneration won't be deterministic by chunk yet.

#### 4. Simulation LOD Doesn't Exist
`TickWithDt` updates everything every tick. There's no "loaded ring" vs "unloaded/abstract" simulation split.

---

### Where the Doc Needs Tightening

#### Global Static Arrays Are the Real Blocker
The doc correctly calls out mover paths as a huge memory sink, but the bigger blocker for infinite worlds is that **everything is fixed-size global arrays** (`grid`, `waterGrid`, `nodeData`, `items`, `movers`). This will need a chunk-owned data model before any streaming can work.

#### HPA* is Present But Not Streaming-Aware
The doc says "HPA* already solves boundary crossing." It does for a static world, but streaming means the abstract graph must handle missing/unloaded chunks and lazy edges. That layer doesn't exist yet.

---

### Low-Hanging Next Steps (Fit Current Architecture)

#### 1. Introduce World Seed + Chunk Seed Now
Even before streaming, add a deterministic seed path in `main.c` and route it into `terrain.c`. This makes future delta persistence viable.

#### 2. Decouple Pathfinding Buffers from Global Arrays
The doc mentions "on-demand nodeData." This is very actionable: isolate `nodeData` and path buffers into per-search scratch buffers. This will ease chunk-local allocation later.

#### 3. Make Designations Sparse Early
The `designations` grid is huge (64 MB); converting to a sparse map is a clear win and prepares for chunk serialization.

---

### Concrete Refactor Options

If implementing any of these, here are file-level starting points:

| Refactor | Files Affected | Complexity |
|----------|---------------|------------|
| Add deterministic world seed + chunk seed | `main.c`, `terrain.c`, `pathfinding.c` | Low |
| Introduce chunk container struct | `grid.h`, `grid.c`, `water.c`, `fire.c`, `smoke.c` | Medium |
| Sparse designations | `designations.h`, `designations.c`, `saveload.c` | Medium |
| Per-search pathfinding buffers | `pathfinding.h`, `pathfinding.c` | Medium |
| Chunk load/unload skeleton | New `streaming.c`, `grid.c`, `mover.c` | High |

---

The key takeaway: an infinite streaming world can use **97% less memory** than your current static allocation (11 MB vs 323 MB), while supporting unlimited exploration. The biggest wins come from:

1. Only loading 9-25 chunks instead of the full 512×512×16 grid
2. Making designations sparse (saves 64 MB)
3. Shrinking or dynamically allocating mover paths (saves 90 MB)


> great from the beginning of the reseacrh what are some key parts we can deep dive further in ?

Good question. Looking back at the research, here are the areas that would benefit from deeper investigation:

---

## Candidates for Deep Dive

### 1. **Overmap Generation & Structure**
*What makes a good overmap?*
- How does CDDA decide what goes where? (cities, roads, dungeons, wilderness)
- Biome generation at macro scale
- How to make it feel coherent (rivers connect, roads make sense, settlements near resources)
- The relationship between overmap cell and local chunk generation

**Why it matters**: This is the foundation - get it wrong and the world feels random/incoherent.

---

### 2. **Cross-Chunk Pathfinding**
*HPA* across streaming chunks*
- You have HPA* working for static world - how does it adapt to streaming?
- What happens to the abstract graph when chunks load/unload?
- Lazy edge computation vs precomputed
- Path caching strategies for entities traveling long distances

**Why it matters**: Your movers need to path across chunk boundaries seamlessly.

---

### 3. **Delta Persistence Format**
*Practical implementation of chunk diffs*
- What's the actual data format? (sparse arrays, RLE, custom binary)
- How to efficiently diff a chunk against its procedural baseline
- Versioning - what happens when generation algorithm changes?
- Compression strategies for deltas

**Why it matters**: This determines save file size and load times.

---

### 4. **Entity Abstraction & Restoration**
*The hardest problem*
- Exactly how do you snapshot a mover to abstract state?
- How do you "fast-forward" needs, relationships, inventory?
- What about entities mid-task when chunk unloads?
- Entity migration between unloaded chunks

**Why it matters**: This is where the "living world" feeling comes from or breaks.

---

### 5. **Time-Skip Approximation for Cellular Automata**
*Fast-forwarding water/fire/growth*
- Water equilibrium calculation (find the level without simulating)
- Fire burnout probability models
- Vegetation growth curves
- When approximation diverges too much from "real" simulation

**Why it matters**: Determines whether returning to an area feels natural or jarring.

---

### 6. **Chunk Boundary Synchronization**
*The edge cases (literally)*
- Water flowing across chunk boundaries
- Fire spreading to unloaded chunks
- Entities at the exact boundary during save
- Ghost cells / border overlap implementation

**Why it matters**: Boundaries are where bugs hide.

---

### 7. **Async Chunk Loading**
*Avoiding hitches*
- Background thread generation/loading
- Double-buffering chunk data
- Priority queue (load where player is heading)
- What to do if player moves faster than loading

**Why it matters**: User experience - hitches break immersion.

---

### 8. **Sparse Data Structures**
*Replacing those big static arrays*
- Hash maps for designations, blueprints, special cells
- Octrees/quadtrees for 3D sparse data
- Memory pool allocators for entities
- Cache-friendly sparse iteration

**Why it matters**: Enables the memory savings we calculated.

---

### 9. **Settlement/Faction Simulation**
*The abstract layer*
- What state do you track per settlement? (population, resources, mood)
- How do factions interact when player isn't watching?
- Event generation (raids, trade caravans, migrations)
- Dwarf Fortress "legends mode" style history

**Why it matters**: Makes the world feel alive beyond player's view.

---

### 10. **Procedural Content at Multiple Scales**
*Coherent generation from macro to micro*
- World seed → region seed → chunk seed → cell details
- Ensuring features span chunk boundaries correctly (rivers, roads, large buildings)
- "Planning" approach for multi-chunk structures
- Templates/prefabs vs pure procedural

**Why it matters**: Avoids the "chunky" feel of naive generation.

---

## My Recommendations (Priority Order)

| Priority | Topic | Reason |
|----------|-------|--------|
| 1 | **Overmap Generation** | Foundation for everything else |
| 2 | **Cross-Chunk Pathfinding** | You have HPA*, need to extend it |
| 3 | **Delta Persistence** | Required for any persistence at all |
| 4 | **Sparse Data Structures** | Enables the memory model |
| 5 | **Chunk Boundary Sync** | Your water/fire systems will need this |
| 6 | **Entity Abstraction** | Can start simple, iterate |

The others (async loading, settlement sim, time-skip) are important but can be layered on later.

---

Which one(s) interest you most? I can do a deep research dive on any of these.

---

## 11. Future Research: Topics Awaiting Deep Dive

This section tracks which topics have been fully researched and which still need deep dives.

### Completed Deep Dives

| Topic | Section | Status |
|-------|---------|--------|
| Overmap Generation | Section 9 | **Done** - noise, Voronoi, settlements, roads, specials, WFC |

### Awaiting Deep Dive (Priority Order)

#### Priority 1: Cross-Chunk Pathfinding
*Extend HPA* for streaming worlds*

Research questions:
- How does the abstract graph handle chunks loading/unloading?
- Lazy edge computation vs precomputed edges
- Path invalidation when terrain changes in unloaded chunks
- Long-distance path caching (abstract path stored, local path computed on demand)
- What happens when a mover's path goes through an unloaded chunk?
- Handling "traveling" state for entities between chunks

Relevant navkit code: `pathfinding.c`, `BuildEntrances()`, `BuildGraph()`, `FindPathHPA()`

---

#### Priority 2: Delta Persistence Format
*Efficient chunk diff storage*

Research questions:
- Data format options: sparse arrays, RLE, custom binary, protocol buffers
- How to efficiently diff current chunk vs procedurally regenerated baseline
- Versioning strategy when generation algorithm changes
- Compression (already using gzip, but chunk-level compression?)
- SQLite vs flat files for random chunk access
- Handling entity references across chunks in save format

Relevant navkit code: `saveload.c`, `SaveWorld()`, `LoadWorld()`

---

#### Priority 3: Sparse Data Structures
*Replace fixed-size global arrays*

Research questions:
- Hash map implementations for C (designations, blueprints)
- Spatial hash vs octree for 3D sparse data
- Memory pool allocators for entities (movers, items, jobs)
- Cache-friendly iteration over sparse data
- Chunk-owned vs global sparse containers
- Migration path from current static arrays

Relevant navkit code: `designations.c` (64 MB array), `nodeData` in `pathfinding.c` (80 MB array)

---

#### Priority 4: Chunk Boundary Synchronization
*Seamless simulation across edges*

Research questions:
- Ghost cells / halo regions for cellular automata
- Water pressure across chunk boundaries
- Fire spread to unloaded neighbors (queue events?)
- Entity position exactly on boundary during save/load
- HPA* entrance recalculation when neighbor chunks change
- Deterministic boundary behavior (same result regardless of load order)

Relevant navkit code: `water.c`, `fire.c`, `smoke.c`, chunk boundary logic in `pathfinding.c`

---

#### Priority 5: Entity Abstraction & Restoration
*Snapshot and restore entities across chunk boundaries*

Research questions:
- Minimal abstract state for a mover (position, job type, needs, inventory)
- Fast-forward algorithms for needs/hunger/rest
- Handling entities mid-task when chunk unloads
- Entity migration between unloaded chunks (abstract travel)
- Restoring entity to valid position when chunk reloads
- Items carried vs items on ground (different handling)

Relevant navkit code: `mover.c`, `jobs.c`, `items.c`

---

#### Priority 6: Time-Skip Approximation
*Fast-forward cellular automata without full simulation*

Research questions:
- Water: calculate equilibrium directly (find the level)
- Fire: probabilistic burnout based on fuel + elapsed time
- Vegetation: growth curves, seasonal cycles
- Ground wear: decay over time without foot traffic
- When does approximation diverge too much from "real" simulation?
- Hybrid: approximate for long skips, simulate for short ones

Relevant navkit code: `water.c`, `fire.c`, `smoke.c`, `groundwear.c`

---

#### Priority 7: Async Chunk Loading
*Background loading without frame hitches*

Research questions:
- Thread pool for chunk generation/loading
- Double-buffering chunk data structures
- Priority queue based on player movement direction
- Handling player moving faster than load speed
- Synchronization: when is chunk "ready" for simulation?
- Platform-specific threading (pthreads, C11 threads, platform APIs)

Relevant navkit code: Would be new `streaming.c` or similar

---

#### Priority 8: Settlement/Faction Simulation
*Abstract world simulation layer*

Research questions:
- Per-settlement state: population, resources, mood, faction relations
- Faction interactions when player not present
- Event generation: raids, trade caravans, migrations, disasters
- Dwarf Fortress "legends mode" style history tracking
- How events in abstract layer affect concrete chunks when loaded
- Balance between simulation depth and performance

Relevant navkit code: Would be new system, ties into overmap

---

#### Priority 9: Procedural Content at Multiple Scales
*Coherent generation from world to cell*

Research questions:
- Seed hierarchy: world → region → chunk → cell
- Features spanning chunk boundaries (rivers, roads, large buildings)
- "Planning" pass for multi-chunk structures before detail generation
- Templates/prefabs vs pure procedural
- Biome transition zones
- Determinism verification (same seed = same world, always)

Relevant navkit code: `terrain.c`, would need new overmap system

---

### Quick Reference: What's Where

| Section | Content |
|---------|---------|
| 1-6 | Core concepts (chunks, persistence, memory, simulation LOD, ECS) |
| 7 | World-bound vs entity-bound distinction |
| 8 | Memory analysis with concrete numbers |
| 9 | **Deep dive: Overmap generation** |
| 10 | Gap analysis: research vs current code |
| 11 | This section: future research tracking |

---

### Suggested Reading Order for Implementation

1. Read Section 9 (Overmap) - understand the macro structure
2. Deep dive Cross-Chunk Pathfinding - extend existing HPA*
3. Deep dive Delta Persistence - enable saves
4. Deep dive Sparse Data Structures - unlock memory model
5. Implement basic streaming with above pieces
6. Layer on remaining topics as needed
