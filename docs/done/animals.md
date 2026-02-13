# Animals

Wildlife system for the colony sim. Animals are autonomous agents driven by steering behaviors, not the mover job system.

---

## Core Concept

Animals are NOT movers. They share some infrastructure (pixel-coord position, grid interaction) but are fundamentally different:

- No jobs, no designations, no player control (initially)
- Autonomous AI: graze, wander, flee, herd
- Different body sizes and movement constraints
- Can be obstacles, resources, or (eventually) tamed

---

## Body Size: The Two-Cell Problem

A cow is big. On an 8x8 sprite grid, a cow doesn't fit in one cell.

**Multi-cell body:** Large animals occupy 2 cells in a line. The animal has a **heading** (N/E/S/W) and occupies `(x,y)` + the cell behind it based on heading. Like a roguelike dragon that fills 2 tiles.

```c
typedef struct Animal {
    float x, y, z;          // Front cell (head) position, pixel coords
    Direction heading;       // N, E, S, W — determines which 2 cells are occupied
    AnimalType type;
    // ...
} Animal;

// Occupied cells:
// heading=N: (x,y) and (x, y+1)    // head north, body south
// heading=E: (x,y) and (x-1, y)    // head east, body west
// heading=S: (x,y) and (x, y-1)    // head south, body north
// heading=W: (x,y) and (x+1, y)    // head west, body east
```

**Turning:** The cow doesn't snap-rotate. To turn from heading=N to heading=E:
1. The body cell (south) must be free to the west
2. The cow "pivots" — front stays, back swings around
3. During the turn animation, briefly occupies 3 cells (L-shape)
4. If the destination cell is blocked, the cow can't turn that way

This gives large animals a real *physical presence*. They can block corridors. They need wider paths. They feel heavy and real.

**Small animals** (chickens, rabbits, dogs) are 1-cell. Only large grazers/predators get the 2-cell treatment.

### Rendering: 3 Sprites, 2 Cells

Large animals are drawn with 3 layered sprites across 2 cells:

```
Cow facing North:

Cell (x, y):    [body-front] + [head] drawn on top  ← two sprites layered
Cell (x, y+1):  [body-rear]                          ← one sprite
```

The head sprite overlaps the front body cell. Visually it's 3 sprites, physically it's 2 cells.

**Uses existing atlas sprites, no new tiles needed:**

- **Body (both cells):** `SPRITE_full_block` — solid █, color-tinted per species
- **Head (overlaid on front body cell):** `SPRITE_head_inverse` — the Π shape (horns/ears + skull), drawn on top of the front body's full_block

Both already exist in `atlas.h`. Species identity comes from color tint alone.

Examples:
```
Cow:    full_block brown  + head_inverse brown (darker/lighter variant)
Deer:   full_block tan    + head_inverse tan
```

The `head_inverse` sprite is rotated per facing direction (N/E/S/W) so the horns/ears point the right way. The full_block body doesn't need rotation — it's solid.

**Turning animation:** During the L-shaped pivot, the head sprite can already face the new direction while the rear body swings around. Head turns first, body follows — looks natural and alive.

**Small animals (1-cell):** Single glyph per direction, color-tinted. Chicken = `c` yellow, rabbit = `r` grey, etc.

---

## Movement Constraints

Animals have a `MovementProfile` that determines what terrain they can traverse:

```c
typedef struct {
    bool canUseLadders;     // false for most animals
    bool canUseRamps;       // true for most land animals
    bool canSwim;           // depth tolerance
    int maxWaterDepth;      // 0 = avoids all water, 3 = can wade, 7 = can swim
    int bodyWidth;          // 1 = single cell, 2 = two-cell
} MovementProfile;

// Cow:     { ladders=false, ramps=true, swim=false, maxWater=2, width=2 }
// Deer:    { ladders=false, ramps=true, swim=true,  maxWater=4, width=2 }
// Chicken: { ladders=false, ramps=true, swim=false, maxWater=1, width=1 }
// Rabbit:  { ladders=false, ramps=true, swim=false, maxWater=1, width=1 }
// Wolf:    { ladders=false, ramps=true, swim=true,  maxWater=3, width=1 }
// Cat:     { ladders=true,  ramps=true, swim=false, maxWater=1, width=1 }
```

Key constraint: **most animals can't use ladders.** A cow on z=0 with only ladder access to z=1 is *stuck*. The player has to build ramps to move livestock between levels. This creates interesting building challenges.

---

## Movement: Steering, Not Pathfinding

### Why Not A*

A cow doesn't plan a route through a maze. A cow sees grass, walks toward grass. Hits a wall, turns. That's the entire algorithm. Real grazers navigate by local sensing, not mental maps.

Also: A* in a 3D chunked world is expensive. Running it for 50 grazing animals every few ticks would kill the framerate.

### The Steering Library Already Exists

There's a complete steering behaviors library in `experiments/steering/` with exactly the primitives needed:

| Animal Behavior | Steering Function | Notes |
|----------------|-------------------|-------|
| Grazing | `steering_forage()` | Wander until resource detected, then seek |
| Herd cohesion | `steering_flocking()` | Separation + cohesion + alignment |
| Gentle fleeing | `steering_departure()` | Flee with deceleration at distance |
| Multi-threat fleeing | `steering_evade_multiple()` | Distance-weighted evasion |
| Hiding behind cover | `steering_hide()` | Use obstacles to hide from pursuer |
| Predator stalking | `steering_pursuit()` | Chase predicted position of prey |
| Pack flanking | `steering_offset_pursuit()` | Pursue with lateral offset |
| Pack coordination | `steering_couzin()` | Zone-based collective motion (ZOR/ZOO/ZOA) |
| Random wandering | `steering_wander()` | Naturalistic random movement |
| Guarding territory | `steering_guard()` | Protect position, wander nearby |
| Avoiding walls/fences | `steering_wall_avoid()` | Feeler rays to steer away from walls |
| Personal space | `steering_separation()` | Repel from nearby agents |
| Staying in area | `steering_containment()` | Stay within rectangular bounds |

The library also has **Context Steering** (`ctx_interest_*` / `ctx_danger_*`) — interest/danger maps with temporal smoothing, hysteresis, and sub-slot interpolation.

### Bridging Steering to the Grid World

The steering library works in continuous 2D space (Vector2 pixel coordinates). The game world is a 3D cell grid. The bridge:

1. **Animal position is in pixel coords** (like movers already are — `float x, y, z`)
2. **Grid queries feed steering inputs:**
   - Scan vegetationGrid in perception radius -> resource positions for `steering_forage()`
   - Scan for movers in radius -> threat positions for `steering_evade_multiple()`
   - Sample nearby walls/solid cells -> Wall segments for `steering_wall_avoid()`
   - Other herd members -> neighbor positions for `steering_flocking()`
3. **Steering output is a force vector** -> apply to velocity -> update pixel position
4. **Grid occupancy updated** from new pixel position (snap to cell for animalOccupancy grid)

The steering library operates per-tick in continuous space. The grid is just the data source. No pathfinding needed.

### When Animals DO Need A Path (Rare)

- **Player-directed movement** (future taming: "go to this pen")
- **Returning to den** through complex terrain (wolf navigating a canyon)

For these rare cases, use A* with movement flags. See the `PathFlags` section in `naked-in-the-grass-details.md` for how to add `canUseLadders=false` to `FindPath()`. But 99% of the time, steering handles everything.

---

## Intelligence Tiers

Not all animals are equally dumb. The steering library supports a spectrum:

### Tier 1 — Cow Brain (reactive grazer)

```
steering_forage()           // wander until grass, then seek grass
+ steering_flocking()       // stay near herd
+ steering_wall_avoid()     // don't walk into walls
+ steering_containment()    // don't leave home range
+ steering_departure()      // flee if mover gets very close (radius=3)
```

Blend with `steering_blend()` or use `steering_priority()` (flee overrides graze). The cow never plans, never solves mazes, gets stuck in corners — all correct behavior. Build a three-sided fence and the cow wanders in but can't find the exit. Free pen behavior.

### Tier 2 — Deer Brain (alert prey)

```
steering_forage()           // graze
+ steering_couzin()         // zone-based herd: repel close, align medium, attract far
+ steering_evade_multiple() // flee from ALL nearby movers (radius=8)
+ steering_hide()           // use trees/rocks as cover when fleeing
+ steering_wall_avoid()     // don't run into walls
```

Smarter than cow. Uses Couzin zones for more realistic herding (biologically grounded — based on real animal research). Hides behind obstacles when fleeing. Still no global planning.

### Tier 3 — Wolf Brain (smart predator)

```
steering_pursuit()          // chase predicted position of prey
+ steering_offset_pursuit() // flank: each pack member offsets laterally
+ steering_couzin()         // pack cohesion with zones
+ steering_wall_avoid()     // navigate around obstacles
+ steering_collision_avoid()// don't collide with pack mates
```

Wolves predict where the deer *will be*. They flank — each wolf offsets laterally from the prey, surrounding it. But they still don't A*. A wolf chases what it can see. If the deer runs around a cliff, the wolf follows scent (last known position), not a precomputed optimal path.

### Tier 4 — Future: Tamed Animals (player-directed)

When the player tells a tamed cow to go somewhere specific, THEN use A* with `PathFlags` to plan a route. The cow follows the path like a mover, but with `canUseLadders=false` so the player has to build ramps.

---

## Context Steering as the Unified Approach

For the cleanest implementation, **Context Steering** might be the best unified model for all tiers. Instead of blending force vectors (which can cancel out — seek + flee = freeze), context steering writes to interest and danger maps:

```c
ContextSteering ctx;
ctx_init(&ctx, 16);  // 16-direction resolution

// Every tick:
ctx_clear(&ctx);

// Interest: where do I want to go?
ctx_interest_seek(&ctx, animalPos, nearestGrass, 0.5f);        // graze
ctx_interest_velocity(&ctx, animalVel, 0.3f);                   // momentum

// Danger: what do I want to avoid?
ctx_danger_walls(&ctx, animalPos, bodyRadius, nearbyWalls, wallCount, 3.0f);
ctx_danger_threats(&ctx, animalPos, moverPositions, moverCount, panicRadius, awareRadius);
ctx_danger_bounds(&ctx, animalPos, homeRange, 2.0f);            // don't stray

// Resolve: pick best direction that has interest but no danger
float speed;
Vector2 dir = ctx_get_direction_smooth(&ctx, &speed);
```

No vector cancellation. The cow can flee from a wolf AND avoid a wall simultaneously without the forces fighting. Temporal smoothing reduces oscillation. Hysteresis prevents flip-flopping between directions.

---

## State Machine

With steering behaviors, the state machine is thin — it just controls which behaviors are active:

```
GRAZING:    forage + flock + wall_avoid + containment
FLEEING:    evade/departure + wall_avoid  (priority: override everything)
WANDERING:  wander + flock + wall_avoid + containment
IDLE:       nothing (standing still, waiting for grass regrowth)
```

Transitions:
- `GRAZING -> FLEEING`: threat enters perception radius
- `FLEEING -> WANDERING`: threat leaves perception radius + cooldown timer
- `WANDERING -> GRAZING`: found grass within perception radius
- `GRAZING -> IDLE`: no grass within perception radius, all eaten
- `IDLE -> GRAZING`: grass regrows nearby (check every N ticks)

---

## Animal Types

### Starting Set

**Large grazers (2-cell):**
- **Cow** — Tier 1 brain. Slow, docile, blocks paths. Lots of meat/hide (future). Milk (future).
- **Deer** — Tier 2 brain. Faster, flees from movers at range. Can swim shallow rivers.

**Small animals (1-cell):**
- **Chicken** — Tier 1 brain. Wanders near home. Eggs (future food source).
- **Rabbit** — Tier 1 brain. Wild, fast, hard to catch. Nibbles crops (future pest).

**Predators (1-cell, future):**
- **Wolf** — Tier 3 brain. Pack hunter. Hunts deer, threatens colony.

Start with cow + deer + chicken + rabbit. They cover: 2-cell vs 1-cell, domestic vs wild, docile vs skittish. Wolves come later when there's something worth protecting.

---

## Grid Presence

Animals block movement (you can't walk through a cow):

```c
// animalOccupancy[z][y][x] — index of animal occupying this cell, or -1
// Updated whenever an animal moves or turns
// Checked by IsCellWalkableAt() for movers
// Checked by steering wall_avoid for other animals
```

Similar to the existing `CELL_FLAG_WORKSHOP_BLOCK` check. Could reuse the same flag mechanism or a parallel grid.

For 2-cell animals, two cells are marked occupied. During a turn, briefly three cells (L-shape).

---

## Interaction With Existing Systems

**Vegetation/Groundwear:** Large animals cause heavy groundwear. Grazing reduces vegetationGrid level. A cow herd's grazing area naturally becomes a trampled, bare-earth paddock. This emerges from the existing groundwear + vegetation systems with zero extra code.

**Water:** Animals with `canSwim=false` treat deep water as danger in context steering. Deer can ford shallow rivers. Rivers act as natural barriers — keeps cows contained without fences.

**Fire:** Animals flee from fire (fire cells are maximum-danger in context steering). A grass fire sends the herd stampeding. This is just `ctx_danger_threats()` with fire cell positions.

**Temperature:** Future — animals seek shade in heat, huddle in cold.

---

## What Animals Enable (Future)

Once animals exist as physical entities in the world, they unlock:

- **Hunting** — designate an animal for hunting, mover approaches and kills it (drops meat, hide, bones)
- **Taming/penning** — build fences (wall variants), lure animals with food, domesticate over time
- **Livestock** — tamed animals produce resources (milk, eggs, wool) on a timer (like passive workshops!)
- **Draft animals** — a tamed cow could pull a cart (carry multiple items between stockpiles)
- **Threats** — wolves use the same system but with ATTACKING instead of FLEEING behavior
- **Leather/bone crafting** — hide -> tanning rack (passive workshop!) -> leather -> clothing
- **Food chain** — wolves hunt deer, deer eat grass, grass regrows — a living ecosystem

The animal system is the gateway to the entire food/clothing/threat layer the game is missing.
