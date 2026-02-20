# Fog of War & Exploration

> The map starts hidden. You discover it through your mover's eyes.

---

## The Problem

Right now the player sees the entire 128x128 map from the start. The mover will pathfind to a berry bush 80 tiles away that "nobody has ever seen." Items get hauled from places no one has visited. There's no sense of discovery, no unknown, no reason to explore.

For the survival mode — one mover, naked in the hills — exploration should be a core part of the experience. The world should feel vast and unknown. You start seeing only what's around you, and every ridge you crest could reveal resources, danger, or nothing at all.

---

## How Other Games Handle This

### Dwarf Fortress
- Surface is fully visible from the start (you chose the embark site from a world map)
- Underground is hidden behind rock — you only see caverns when you dig into them
- "Fog" is literally stone: you can't see what's behind unmined walls
- Creatures can hide and remain invisible until a dwarf spots them
- Exploratory mining is a real strategy: dig probe tunnels to find caverns, ores, water

### RimWorld (with Fog of War mods)
- Vanilla RimWorld has no fog of war — full map visibility
- Popular mods (Real Fog of War) add it: map starts black, revealed by colonist vision
- Vision is per-colonist, shared among faction — if any colonist sees it, it's revealed
- Three states: **unexplored** (black), **explored** (terrain visible, no dynamic info), **visible** (full info within line of sight)
- Explored areas show terrain/structures but not current enemy positions

### Classic RTS (Warcraft, StarCraft, Age of Empires)
- Three states: black (never seen), fog (seen before, shows terrain), clear (currently visible)
- Units have vision radius — fog returns when units leave
- "Scouting" is a core early-game activity: send a cheap unit to explore

### Key Design Patterns
1. **Three visibility states**: unexplored → explored → currently visible
2. **Vision radius per unit**: each mover reveals a circle around them
3. **Permanent terrain reveal**: once you've seen terrain, it stays visible (but dynamic things may change)
4. **Fog returns**: areas outside current vision revert to "explored but not live" (optional, more for PvP)

---

## Design for NavKit

### Philosophy

- The map starts **fully hidden** in survival mode (sandbox mode keeps full visibility)
- Your mover reveals terrain as they walk around
- You can only designate actions in **explored** areas
- Movers will only pathfind to / haul from **explored** areas
- Exploration itself can be designated: "go explore that direction"

### Two States (not three)

For a single-player colony sim, two states are enough:

| State | Visual | Can Designate? | Can Pathfind To? | Items Visible? |
|-------|--------|---------------|-----------------|----------------|
| **Unexplored** | Dark/black overlay | No | No | No |
| **Explored** | Fully visible | Yes | Yes | Yes |

We skip the "explored but fogged" third state. There are no enemies or PvP — once you've seen a tile, there's no reason to hide it again. Terrain doesn't move. Items on the ground that you've seen stay visible (they won't walk away).

This is simpler and matches the Dwarf Fortress surface approach better than the RTS fog-returns model.

### Vision Radius

Each mover has a **vision radius** — a circle of tiles they can see. As they move, any unexplored tile within this radius becomes explored. Permanently.

```
Vision radius: 8-12 tiles (tunable via balance.h)
Shape: circular
Blocked by: walls, terrain (optional: line-of-sight)
```

**Line-of-sight question**: Do walls block vision? Two options:

**Option A: Simple radius (no LOS blocking)**
- Mover reveals everything within N tiles, regardless of walls
- Simple to implement: just mark tiles within radius as explored
- Feels slightly unrealistic but works fine for surface exploration
- Underground: still hidden behind rock (never in radius if enclosed by walls)

**Option B: Raycasted LOS**
- Walls and solid terrain block vision
- Mover can't see behind a hill or inside a cave until they enter
- More immersive, especially for underground / cave discovery
- More expensive to compute (raycast from mover to each tile in radius)

**Recommendation**: Start with **Option A** (simple radius). It handles the 90% case — surface exploration — perfectly. Underground areas are naturally hidden because they're enclosed by walls that movers can't reach without mining. Line-of-sight can be added later for cave interiors and hilltops without changing the data model.

### The Exploration Grid

A simple per-cell boolean grid:

```c
// One bit per cell, or uint8_t for simplicity
uint8_t exploredGrid[MAX_DEPTH][MAX_HEIGHT][MAX_WIDTH];
// 0 = unexplored, 1 = explored
```

At 128x128x32 this is 512KB (uint8_t) or 64KB (bitpacked). Trivial memory cost.

**Starting state**: In survival mode, all cells are unexplored. A small radius around the mover's spawn point is pre-revealed (so the player can see where they are and what's nearby).

**Sandbox mode**: All cells start explored (no fog of war).

### What Fog of War Blocks

The exploration state must be checked in several systems:

| System | Behavior When Unexplored |
|--------|-------------------------|
| **Rendering** | Draw dark overlay (black or dark tint) over unexplored cells |
| **Designations** | Cannot place mine/chop/build/gather designations on unexplored cells |
| **Pathfinding** | Unexplored cells are treated as impassable (movers won't path through them) |
| **Hauling** | Items in unexplored areas are invisible to haul search |
| **Job assignment** | WorkGiver functions skip unexplored targets |
| **Tooltips** | No tooltip info for unexplored cells |
| **Stockpile placement** | Cannot place stockpiles in unexplored areas |
| **Workshop placement** | Cannot place workshops in unexplored areas |

The key constraint: **movers should never autonomously go somewhere unexplored**. No pathfinding through unexplored tiles, no hauling items they haven't "seen."

### Explore Designation

New designation: **DESIGNATION_EXPLORE**

The player clicks somewhere in the dark (or near the frontier). The mover walks in a **straight line** toward that point. No pathfinding — just a direction.

**How it works**:

1. Player clicks on an unexplored area (or the fog boundary)
2. System computes a **direction vector** from mover to click point
3. Mover walks cell-by-cell along that line (Bresenham or normalized direction)
4. Each step reveals cells within vision radius
5. **If the next cell is blocked** (wall, water, cliff, edge of map) → **job done**, mover stops
6. **If the mover reaches the click point** → job done
7. Player looks at what was revealed, clicks again to continue exploring

This is the only job type that uses **no pathfinding at all** — no A*, no HPA*. The mover walks blind in a direction, discovers what's there, and stops when blocked. The player manually guides the exploration with repeated clicks.

**Why straight-line, not pathfinding?**
- **Narratively honest**: The mover doesn't know what's ahead. They can't plan a route through terrain they've never seen.
- **Zero HPA* cost**: No pathfinding means no chunk invalidation from exploration. The HPA* graph only covers real terrain, never needs to know about explored/unexplored state.
- **Player agency**: Exploration is a series of manual decisions. "Go that way." Hit a cliff. "Okay, try left." This feels like actual scouting, not "computer, find me a route to tile 87,12."
- **Simple to implement**: No special pathfinding mode. Just step along a line, check walkability at each cell.

**Edge case — mover is surrounded by unexplored**: At game start, the mover is in a small revealed circle. They can walk to the edge of it, then the explore job takes over with straight-line movement into the unknown. The transition is seamless.

**Visual feedback**: When the player hovers in explore mode, draw a dotted line from the mover toward the cursor, stopping at the frontier edge. This shows the intended direction clearly.

### Interaction With Existing Systems

**Pathfinding & HPA***: Exploration does NOT touch the pathfinding system at all. The HPA* graph is built on the **full real terrain** — it doesn't know about explored/unexplored state. This means:
- Zero chunk invalidation from exploration (the big HPA* gotcha is completely avoided)
- Normal jobs (haul, mine, chop, build) pathfind normally through explored territory
- The exploration check lives purely in **job assignment**: WorkGiver functions won't create jobs targeting unexplored areas

The `IsExplored()` check is applied at the **job assignment layer**, not the pathfinding layer. This is a critical design choice — see Gotchas section for the full reasoning.

**Hauling / Item search**: `IsItemHaulable()` adds one check: `IsExplored(items[i].x, items[i].y, items[i].z)`. Items in unexplored areas are invisible to the haul system.

**Berry foraging / resource gathering**: Same pattern — skip plants/bushes in unexplored areas.

**Weather / simulation**: Weather, temperature, water, fire all run on the full grid regardless of exploration state. Rain falls on unexplored tiles. Fire spreads in unexplored areas. The player just can't *see* it until they explore there. This means you could explore toward smoke and discover a forest fire, or find a flooded valley — emergent discovery moments.

### Rendering

The rendering system already draws the grid layer by layer. For unexplored cells:

**Option A: Black tiles** — Replace the cell sprite with solid black. Simple, clean, classic fog of war look.

**Option B: Dark overlay** — Draw the normal cell sprite but with a heavy dark tint (alpha 200+ black overlay). This lets terrain "peek through" slightly, like DF's "you know there's stone here but not what kind."

**Option C: Nothing** — Don't draw unexplored cells at all. The map just has black holes.

**Recommendation**: Option A (solid black) for unexplored. Clean, readable, no ambiguity about what's known vs unknown.

**Frontier effect** (optional polish): Tiles at the border between explored and unexplored could have a soft edge / gradient, rather than a hard black line. This makes the map feel more natural.

### Save/Load

The explored grid must be saved:
- Save version bump
- Write/read `exploredGrid` in the grid section of the save file
- Simple: one byte per cell (or bitpacked for efficiency)
- Sandbox saves can skip the grid (all explored)

### Spawn Reveal

When starting a new survival game, reveal a circle around the mover's spawn:

```c
void RevealAroundPoint(int cx, int cy, int cz, int radius) {
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy <= radius*radius) {
                // Reveal all z-levels at this x,y that are visible from surface
                for (int z = 0; z < gridDepth; z++) {
                    exploredGrid[z][y][x] = 1;
                }
            }
        }
    }
}
```

Spawn reveal radius: ~10-15 tiles. Enough to see your immediate surroundings, a few trees, maybe a water source. Not enough to see the whole map.

---

## Implementation Phases

### Phase 1: Exploration Grid + Rendering (~1 session)

1. Add `exploredGrid[z][y][x]` (uint8_t, alongside other grids)
2. Add `IsExplored(x, y, z)` / `SetExplored(x, y, z)` helpers
3. Sandbox mode: init all explored. Survival mode: init all unexplored.
4. Reveal spawn area on new game
5. Mover movement: on each step, reveal cells within vision radius
6. Rendering: draw black overlay on unexplored cells
7. Save/load the explored grid (save version bump)
8. **Test**: Grid init, reveal function, radius calculation

### Phase 2: Block Designations + Pathfinding (~0.5 session)

1. Designation placement: reject if target cell is unexplored
2. Pathfinding: treat unexplored cells as impassable (add check to walkability)
3. Invalidate HPA* chunks when cells become explored (same as terrain change)
4. **Test**: Can't designate unexplored, pathfinding avoids unexplored

### Phase 3: Block Hauling + Job Assignment (~0.5 session)

1. `IsItemHaulable()`: add `IsExplored()` check
2. Berry harvest WorkGiver: skip unexplored bushes
3. All WorkGiver functions: skip targets in unexplored areas
4. Stockpile/workshop placement: reject in unexplored areas
5. Tooltips: no info for unexplored cells
6. **Test**: Items in unexplored areas ignored by haul, jobs don't target unexplored

### Phase 4: Explore Designation (~1 session)

1. Add `DESIGNATION_EXPLORE` and `JOBTYPE_EXPLORE`
2. Add `ACTION_WORK_EXPLORE` to action registry (under work submenu)
3. Player clicks on dark/frontier area → system computes direction vector from mover to click
4. Explore job: mover walks cell-by-cell along straight line (Bresenham), no A*/HPA*
5. Each step reveals cells within vision radius
6. Job ends when: next cell is blocked (wall/water/cliff) OR target reached
7. Visual feedback: dotted line from mover toward cursor in explore mode
8. **Test**: Mover walks straight line, reveals cells, stops at walls

### Phase 5: Polish (~0.5 session)

1. Frontier edge rendering (soft border between explored/unexplored)
2. Discovery events: log when mover discovers water, resources, etc.
3. Mini-map shows explored vs unexplored (if mini-map exists)
4. Balance tuning: vision radius, spawn reveal size

---

## Design Decisions

### Why Not Line-of-Sight?

LOS (raycasting from mover through walls) is more realistic but:
- Expensive per-tick (raycast to every cell in radius)
- Surface exploration doesn't need it (no walls to block vision on open terrain)
- Underground is naturally hidden (behind walls movers can't reach)
- Can be added later without changing the data model

Simple radius handles 90% of cases. LOS is a polish feature.

### Why Not "Fog Returns"?

In PvP games (StarCraft), fog returns when your units leave — enemies could move into fogged areas. NavKit has no enemies (yet). Terrain doesn't move. There's no reason to un-explore an area.

If threats/enemies are added later, a third state ("explored but not currently visible") could be added. The data model supports it — change `uint8_t` from 0/1 to 0/1/2 (unexplored/explored/visible). Movers update visible cells each tick, visible decays to explored when they leave. But this is future work.

### Why Block Pathfinding (not just rendering)?

If you only hide the visuals but let movers pathfind through unexplored areas, the mover would walk through "dark" territory to reach an item they "haven't seen." This breaks the fiction completely.

Treating unexplored cells as impassable means the mover only walks through known territory. When they explore and reveal new cells, those cells become available for pathfinding. This creates a natural frontier that expands as you explore.

Edge case: what if explored territory is a narrow corridor and the mover needs to go "around" through unexplored tiles to reach another explored area? They can't — they'd need to explore the path first. This is intended: it creates exploration as a real activity, not just a visual overlay.

### Sandbox Mode

Fog of war is **survival mode only**. Sandbox mode starts fully explored, no visibility restrictions. This keeps the sandbox as the "god mode" creative tool it is today.

The `gameMode == GAME_MODE_SURVIVAL` check gates the fog of war system. All the checks (designation blocking, pathfinding, hauling) short-circuit to "yes, explored" when not in survival mode.

---

## Connections

- **Survival mode**: Fog of war only activates in survival. Defines the early-game experience.
- **Weather/fire**: Simulations run on full grid. Player discovers their effects on exploration (find a burnt forest, a flooded area, a snow-covered valley).
- **Pathfinding**: Unexplored = impassable. HPA* chunk invalidation on reveal (same pattern as mining).
- **Hauling/jobs**: All WorkGiver functions respect explored state.
- **Future threats**: If enemies are added, the two-state model can extend to three states (unexplored/explored/visible) for dynamic fog.
- **Future: watch towers / elevated vision**: Higher z-levels could grant larger vision radius, making hilltops strategically valuable for scouting.

---

## Gotchas & Tricky Interactions

### 1. HPA* Chunk Invalidation — Solved by Design

This *was* the biggest technical risk, but the straight-line exploration approach avoids it entirely.

**Background**: The map is divided into 16x16 chunks. HPA* precomputes an abstract graph of how chunks connect. When terrain changes (mining a wall), `MarkChunkDirty()` flags that chunk and `UpdateDirtyChunks()` rebuilds it + neighbors. If exploration had been tied to pathfinding (making unexplored cells "impassable"), every mover step would reveal ~314 cells across 4-8 chunks, triggering a rebuild storm every tick.

**How we avoid it**: Exploration state is **not part of the pathfinding layer**. The HPA* graph is built on the real terrain, always. The `IsExplored()` check only lives in:
- WorkGiver job assignment (don't target unexplored areas)
- Item search / hauling (ignore items in unexplored areas)
- Designation placement (can't designate on unexplored cells)
- Rendering (black overlay on unexplored)

The explore job itself uses straight-line movement (no pathfinding), so it never queries HPA* either. Result: zero chunk invalidation from exploration. The HPA* system doesn't even know fog of war exists.

### 2. Mover Spawn Ordering

When starting a new survival game, the spawn reveal must happen *before* the first tick. If the mover spawns and the first tick runs job assignment (auto-eat, seek berries), the pathfinder needs explored cells to exist. Reveal the spawn area during terrain gen / new game init, not on first tick.

### 3. Simulations Crossing the Frontier

Weather, fire, water all simulate on the full grid. Effects can reach the explored/unexplored border:

- **Water** flowing from unexplored into explored territory: looks like water appearing from a black wall. This is actually atmospheric — mysterious water source from the unknown.
- **Fire** spreading invisibly in unexplored areas, then suddenly appearing at the border: could be confusing. Player sees fire pop in from nowhere.
- **Smoke** drifting from an unseen fire into explored territory: great — it's a signal that something is burning in the fog.

**Recommendation**: Don't try to fix this. Let simulations cross the boundary naturally. Water/smoke appearing from the fog is a feature, not a bug — it gives the player clues about what's beyond the frontier. Fire popping in is the one awkward case, but it's rare and the alternative (suppressing fire rendering at the boundary) is more confusing.

### 4. Explore Designation UX — Clicking on Black

The player clicks on black/unexplored territory. With straight-line movement this works naturally — the click just sets a **direction**, not a destination. The mover walks that way until blocked.

**Visual feedback is important**: In explore mode, draw a dotted line from the mover toward the cursor. The line crosses through the darkness, showing the intended direction. When the mover hits an obstacle (wall, water), they stop and the player can see what was revealed. Then they click again in a new direction.

This feels like pointing a scout: "go that way and see what's there." The player doesn't need to know what's in the dark — that's the whole point. They're choosing a direction, not a destination.

**Possible enhancement**: When the mover stops (blocked), briefly flash or highlight the blocking obstacle so the player understands *why* they stopped. "There's a cliff face here" vs "there's a river here."

### 5. Jobs Across Unreachable / Unexplored Gaps

**Scenario**: You're next to a river. Vision reveals cells across the river. You designate "mine that wall" on the other side. The mover can see it, but how do they get there?

**Answer**: This just works. The pathfinder operates on the full real terrain — it doesn't know about fog of war. Two cases:

- **No path exists** (river blocks everything): Same as without fog of war. Job sits pending, no mover can reach it. Not a fog problem — it's a terrain problem.
- **Path exists but goes through unexplored territory** (e.g., a land bridge 30 tiles south that you haven't explored yet): The pathfinder finds it. The mover walks through unexplored tiles to reach the job, revealing them along the way as incidental exploration.

This is intentional and desirable. The player deliberately created a job at a visible location — the mover being resourceful enough to find a route feels natural. Refusing the job because the *path* crosses fog would be frustrating ("I can see it! Just go there!").

**The rule is simple**:
- **Job targets** must be in explored territory (can't designate what you can't see)
- **Job paths** can cross unexplored territory (mover figures out the route, reveals tiles incidentally)
- **Autonomous behavior** (hauling, foraging) only targets explored areas (WorkGiver filter)

This means exploration happens two ways:
1. **Deliberately**: Explore designation — straight line into the dark
2. **Incidentally**: Mover walks through fog to reach a visible job, revealing tiles along the way

Both feel natural, and the implementation stays dead simple — no path-validation against the explored grid.

### 6. Animals Crossing the Boundary

Animals roam the full grid. A deer could be half-visible at the explored/unexplored border, or a wolf could emerge from darkness.

**Recommendation**: Only render animals in explored cells. Animals simulate everywhere but are invisible in unexplored territory. When an animal walks into explored territory, it just appears — like it wandered out of the forest. A predator emerging from the fog is actually a great survival moment. No special logic needed beyond the render check.

### 7. Multi-Mover (Future)

Current survival is single-mover, but when multiple movers exist, they should share one explored grid (faction-wide knowledge). The current design handles this — any mover revealing a cell marks the shared `exploredGrid`. Just noting: vision is not per-mover, it's shared.

### 8. Save File Size

128x128x32 = 524,288 bytes as `uint8_t`. That's ~500KB added to saves. Options:
- **Bitpack**: 1 bit per cell = 65KB. More complex read/write but 8x smaller.
- **RLE**: Explored areas are spatially contiguous (big blob of 1s surrounded by 0s). RLE compresses extremely well here.
- **Skip in sandbox**: Sandbox saves don't need the grid (all explored). Only write in survival mode.
- **Existing compression**: Saves are already `.bin.gz` — gzip will compress the raw grid very well since it's mostly 0s with a contiguous blob of 1s.

**Recommendation**: Write raw `uint8_t` grid, rely on existing gzip compression. It'll compress to nearly nothing. Optimize later only if save size is an actual problem.

---

## Implementation Difficulty Notes

### Straight-Line Explore Job (medium)

The only truly new job pattern. Every other job uses A*/HPA* pathfinding — the explore job has none. It needs custom movement logic: step along a Bresenham line, check walkability per cell, move the mover smoothly. The existing `RunJob_*` / mover movement system is built around "compute path, then follow it." The explore job has no path — it steps one cell at a time blindly.

Likely approach: `RunJob_Explore()` directly sets the mover's next waypoint each tick (one cell along the Bresenham line) rather than computing a full path upfront. Check walkability of the next cell — if blocked, job done.

### Rendering Overlay (medium)

Drawing black over unexplored cells sounds simple, but the renderer draws in layers: ground, walls, items, movers, deeper z-levels. The overlay must cover *everything* — terrain, items, trees, water — without hiding explored cells or z-fighting. The DF-style deeper-level rendering (seeing through floors to levels below) adds complexity: if z=2 is explored but z=1 isn't, what do you draw when looking down? Probably black for the unexplored layer, but draw order needs care.

### WorkGiver Filter Completeness (tedious)

Many places search for items/targets: `IsItemHaulable()`, berry harvest, gather tree, gather grass, knapping, crafting input search, stockpile-centric haul, item-centric haul fallback, passive workshop delivery, etc. Each needs an `IsExplored()` check. Missing even one means a mover runs into the fog to haul a rock they've "never seen."

Not hard per-check, but finding *every* place is tedious. Mitigation: add an exploration invariant to the state audit system ("no active job targets unexplored cells") to catch misses.

### Z-Level Vision Reveal (needs thought)

When the mover at z=2 reveals x=10, y=10 — which z-levels get revealed? Options:

- **All z-levels at that x,y**: Simple but reveals underground caves you're standing above. Wrong.
- **Mover's z-level only**: Misses floors below open air (looking down a cliff) and the sky above.
- **Mover's z + visible z-levels**: Reveal upward through air (sky), downward through air (cliffs, pits). Stop at solid cells. Underground stays hidden until you mine into it.

**Recommendation**: Reveal from the mover's z-level upward (through air to sky) and downward (through air until hitting solid ground). This means standing on a cliff reveals the valley below, but walking over solid ground doesn't reveal caves underneath. Matches how DF handles it — you see what's physically visible from where you stand.

```
z=4: [AIR]     ← revealed (open sky above mover)
z=3: [AIR]     ← revealed (mover is here)
z=2: [AIR]     ← revealed (open air below, can see down)
z=1: [WALL]    ← revealed (the ground surface you can see)
z=0: [WALL]    ← NOT revealed (buried, can't see inside solid rock)
```

---

## Save Version Impact

- New grid: `exploredGrid[z][y][x]` (uint8_t)
- Save version bump for the new grid data
- Sandbox saves: skip grid (or write all-1s)
- Migration: old saves default to all-explored (backward compatible)

## Estimated Scope

| Phase | Effort | Notes |
|-------|--------|-------|
| Phase 1: Grid + rendering | ~1 session | Core data structure + visual |
| Phase 2: Block designations + pathfinding | ~0.5 session | Walkability check + HPA* invalidation |
| Phase 3: Block hauling + jobs | ~0.5 session | IsItemHaulable + WorkGiver checks |
| Phase 4: Explore designation | ~1 session | New designation + job type + frontier logic |
| Phase 5: Polish | ~0.5 session | Edge rendering, discovery events |
| **Total** | **~3.5 sessions** | 1 save version bump |

## Test Expectations

- ~30-40 new assertions
- Grid init (explored vs unexplored per game mode)
- Reveal radius math (circular, correct cells marked)
- Designation blocked on unexplored cells
- Pathfinding treats unexplored as impassable
- Items in unexplored areas not haulable
- Explore job finds frontier and reveals territory
- Save/load round-trip preserves explored state
