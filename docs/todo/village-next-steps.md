# Village Next Steps — Bridge from Survival to Settlement

> Status: spec

> What can we build *now* toward the endgame village vision, given what already exists? Ordered by dependency chain and effort-to-impact ratio.

---

## Where We Stand

The survival loop is complete through Tier 3 (09a loop closers). Movers can eat, drink, sleep, farm, hunt, forage, craft tools, wear clothing, build shelters, and ride trains. The codebase has:

| System | What exists |
|--------|------------|
| Needs | hunger, thirst, energy, body temperature — all functional |
| Shelter | IsExposedToSky, walls/doors/roofs/windows, temperature effects |
| Furniture | 4 types (leaf pile, grass pile, plank bed, chair), occupant tracking, rest rates, material inheritance |
| Lighting | Sky light + colored block light (torches, fire), GetLightColor per cell |
| Floors | Constructed vs natural, floor dirt tracking + cleaning, material types |
| Transport | Trains with stations, mover boarding, auto-decision heuristic (phases 1-4 done) |
| Workshops | 15+ types, recipes, crafting jobs, passive/active/semi-passive |
| Construction | Walls, floors, doors, windows, ramps, ladders, furniture placement |
| Weather | Seasons, rain, snow, wind, temperature, cloud shadows |

What does NOT exist yet: mood, room quality, occupations, schedules, relationships, elevators, staircases, plumbing, heating networks, electricity.

---

## Build Order

### Step 1: Room Detection, Typing & Quality Scoring

**Effort:** ~1-1.5 sessions | **Deps:** none | **Unlocks:** mood, housing tiers, furniture mattering

Two parts: detect rooms (flood-fill), then classify and score them.

#### 1a. Room Detection

Rooms are implicitly detectable — flood-fill from any interior cell, bounded by walls and doors. Per-cell grid of room IDs for fast lookup.

**What this looks like in code:**
- `RoomDetect()` — flood-fill from seed cell, collect bounded region, store room ID per cell
- `GetRoomAt(x, y, z)` — lookup room ID for a cell
- `GetRoomQuality(roomId)` — return cached score
- Trigger recompute on: wall/door/floor build or destroy, furniture place or remove

#### 1b. Room Type Detection

Room type is determined by furniture contents. This is a key lesson from RimWorld, Going Medieval, and ONI — *what kind* of room matters more than a generic quality score. Each type gives a specific mood bonus when movers use it for the right activity.

| Room type | Detection rule | Mood effect |
|-----------|---------------|-------------|
| **Bedroom** | has bed, enclosed, 1 bed = private | sleep quality bonus (private > shared > barracks > outdoors) |
| **Barracks** | has 2+ beds, enclosed | small sleep bonus, but penalty vs private bedroom |
| **Dining room** | has table + chair, enclosed | meal mood bonus ("ate at table") |
| **Kitchen** | has cooking workshop, enclosed | food quality bonus |
| **Workshop** | has crafting workshop, enclosed | work mood bonus (small) |
| **Storage** | has stockpile, enclosed | no mood effect, but keeps items dry/clean |
| **Untyped** | enclosed but no qualifying furniture | no type bonus |

**ONI rule (optional, consider later):** a room that qualifies as two types (bedroom + dining room) could become "miscellaneous" with no bonus, incentivizing dedicated rooms. For v1, simpler to just pick the dominant type based on furniture count.

**Private bedroom bonus:** This is a big one from both RimWorld and ONI. The progression is:

```
Sleeping outdoors         → mood penalty
Sleeping in barracks      → small bonus
Sleeping in shared bedroom → medium bonus
Sleeping in private bedroom → large bonus
```

This naturally incentivizes building individual bedrooms as the settlement grows — from survival lean-to → communal sleeping area → private rooms.

#### 1c. Quality Scoring

Within each room type, quality is scored from the factors we already track:

**Positive factors:**
- **Size** — cell count of enclosed area (diminishing returns past ~12 cells)
- **Furniture count & variety** — more types = better (bed alone < bed + chair + rug)
- **Light level** — average GetLightColor across room cells
- **Floor quality** — constructed floor > natural dirt
- **Material quality** — stone walls > stick walls > leaf walls
- **Window access** — CELL_WINDOW present = bonus

**Negative factors (penalties):**
- **No light source** — dark room = significant penalty (Sims 4: -5 for no lights/windows)
- **Dirty floor** — floorDirtGrid > threshold = penalty (RimWorld: single dirt speck can drop a tier)
- **Unfinished walls** — exposed natural stone/dirt walls = penalty (Sims 4: -15 for uncovered walls)
- **Too small** — below minimum size = cramped penalty
- **No window** — no natural light = minor penalty

**Key design insight from RimWorld:** The impressiveness formula is "weighted toward the lowest stat." One bad factor drags the whole room down. This creates "fix what's wrong" gameplay alongside "add nice things." A beautiful room with a filthy floor should score badly.

Output: `float roomQuality` (0.0 = cave, 1.0 = well-furnished apartment). Stored per-room, recomputed when construction/furniture changes.

**Design questions:**
- Multi-z rooms: a room with a staircase/ladder opening — does it span z-levels? Probably not for v1 (each z-level scored independently).
- Outdoor spaces: parks/courtyards are "rooms" with no roof. Score differently? Or only score enclosed spaces for now.

---

### Step 2: Simple Mood

**Effort:** ~1 session | **Deps:** step 1 (room quality) | **Unlocks:** quality-of-life feedback loop, work speed variation

Mood has two layers: **continuous inputs** (needs satisfaction) and **discrete moodlets** (recent events). Both feed into a single `float mood` (0.0-1.0).

#### Continuous inputs (recomputed periodically)

| Input | Source | Weight | Notes |
|-------|--------|--------|-------|
| Hunger satisfaction | `mover.hunger` | high | >0.5 = good, <0.2 = bad |
| Thirst satisfaction | `mover.thirst` | high | same curve |
| Energy / rest | `mover.energy` | medium | well-rested vs exhausted |
| Temperature comfort | `mover.bodyTemp` | medium | comfortable range = good |
| Bedroom quality | room quality of last-slept room | medium | persists through the day (like RimWorld) |
| Bedroom privacy | private vs shared vs barracks vs outdoors | medium | big swing from outdoor → private room |

#### Discrete moodlets (event-driven, decay over time)

Moodlets are temporary mood modifiers triggered by specific events. Each has a value (+/-) and a duration. The mover carries a small array of active moodlets.

| Moodlet | Trigger | Value | Duration | Notes |
|---------|---------|-------|----------|-------|
| **Ate at table** | ate in dining room with table+chair | +2 | 6h | The classic — drives table placement |
| **Ate without table** | ate standing, outdoors, or no dining room | -3 | 6h | RimWorld's most famous mood modifier |
| **Slept well** | slept in private bedroom, good quality | +2 | 12h | Persists through workday |
| **Slept badly** | slept outdoors or on ground (no bed) | -2 | 12h | Incentivizes building beds early |
| **Nice room** | spent time in impressive room | +1 | 4h | From working/eating in quality space |
| **Ugly room** | spent time in low-quality room | -1 | 4h | Dirty, dark, cramped |
| **Cold** | body temp below comfort range | -2 | while cold | Already tracked via temperature |
| **Warm and cozy** | near fire/hearth in cold weather | +1 | 2h | Reward for heating infrastructure |

**Why moodlets over pure weighted sum:** Moodlets are *legible*. The player hovers a mover and sees "Ate without table (-3)" — they immediately know what to fix. A pure weighted average hides the cause. Every game in the research (RimWorld, Sims, ONI, Going Medieval) uses named moodlets for exactly this reason.

**Implementation:** Small fixed-size array on Mover (8-12 slots). Each moodlet: `{ MoodletType type; float value; float remainingTime; }`. Tick decrements time, expired moodlets removed. Mood = clamp(baseMood + sum(continuous) + sum(moodlets), 0, 1).

#### Effects of mood

- Work speed multiplier: happy movers work faster (1.0x-1.2x), unhappy movers work slower (0.8x-1.0x)
- Visual indicator: sprite tint or thought bubble (simple particle/icon above head)
- Tooltip: show mood bar + list of active moodlets with values and remaining time

#### What this is NOT (yet)

- No personality traits (those come later, see `10-personality-and-skills.md`)
- No social needs (no relationships system yet)
- No fun/boredom (needs leisure activities first)
- No memory beyond moodlet duration — no long-term grudges or attachments

This is minimum viable mood, but with *legible causes*. A player can always answer "why is this mover unhappy?" by reading their moodlet list. That legibility is what makes room quality and furniture placement feel rewarding — you see the direct connection between "I placed a table" and "Kira is happier."

---

### Step 3: Distance Weighting in Job Assignment

**Effort:** ~0.25 session | **Deps:** none | **Unlocks:** movers behaving locally, proto-occupations

Currently `AssignJobs()` finds the nearest idle mover for each job, but movers will walk across the entire map. Adding a distance penalty to job scoring makes movers prefer nearby work:

```
jobScore = basePriority - (distance * DISTANCE_WEIGHT)
```

This is a small change in the WorkGiver scoring path. Immediate effect: movers cluster near their work area instead of crisscrossing the map. A mover sleeping near the farm tends to farm. A mover sleeping near the workshop tends to craft. Emergent locality without explicit role assignment.

**Tuning:** DISTANCE_WEIGHT should be low enough that high-priority jobs still pull movers from far away (starvation, fire), but high enough that equal-priority jobs favor proximity.

Can be built independently of steps 1-2. Could ship in the same session as either.

---

### Step 4: Elevators

**Effort:** ~2 sessions | **Deps:** none (transport layer proven) | **Unlocks:** vertical circulation, multi-story buildings

The transport infrastructure from trains transfers directly:
- **Reuse:** WaitingSet, transport state machine (WALKING→WAITING→RIDING), boarding/exiting, mover position locking, DismountAll, cancel job cleanup, ShouldUseTransport heuristic
- **New:** vertical shaft cell type, floor-stop detection, call/dispatch logic (or simple loop), shaft placement UI

See `world/elevators.md` for full design, `pathfinding/transport-layer.md` Layer 4 for how it fits the pattern.

This is a parallel track — can be built alongside steps 1-3 without conflict. Completes vertical circulation for apartment buildings (ladders are too slow for 3+ stories). Combined with room quality, this means "penthouse apartment with elevator access" becomes a meaningful housing tier.

---

### Step 5: Occupations (light version)

**Effort:** ~1 session | **Deps:** step 3 (distance weighting) | **Unlocks:** movers with identity, schedule system foundation

Each mover gets an `occupation` field (enum: UNASSIGNED, FARMER, CRAFTER, BUILDER, HAULER, MINER, etc.) that biases which WorkGivers they prefer. Not a hard gate — an unassigned mover still does everything, a farmer *prefers* farm work but will haul if nothing else is available.

```c
float occupationBonus = (job matches mover's occupation) ? OCCUPATION_BIAS : 0.0f;
jobScore = basePriority + occupationBonus - (distance * DISTANCE_WEIGHT);
```

**Player interaction:** click mover → assign occupation from dropdown. Or auto-assign based on what they've been doing most (emergent specialization).

**Why before schedules:** occupations give meaning to schedule blocks. "Work block" for a farmer means farming. Without occupations, all work blocks are identical. This also connects to skills (`10-personality-and-skills.md`) — a mover who farms all day gains farming skill, which makes them better at farming, which reinforces the occupation.

---

### Step 6: More Furniture

**Effort:** ~0.5-1 session | **Deps:** step 1 (room quality makes furniture matter) | **Unlocks:** room variety, decoration, housing tiers

Each new furniture type is: enum entry + FurnitureDef row + construction recipe + sprite. Current types:

| Type | Rest rate | Blocking | Exists |
|------|-----------|----------|--------|
| Leaf pile | low | no | yes |
| Grass pile | low | no | yes |
| Plank bed | high | yes | yes |
| Chair | 0 | no | yes |

Candidates for next batch (prioritized by room quality impact):

| Type | Effect | Recipe | Notes |
|------|--------|--------|-------|
| Table | meal quality bonus (eat at table vs standing) | planks | pairs with chair, prerequisite for "sit down meal" |
| Shelf | storage display (cosmetic) or small stockpile | planks | room quality bonus for having storage |
| Rug | floor quality override (covers dirt) | cloth/hide | cleaning alternative — rug over dirty floor |
| Lamp stand | light source (block light) without wall torch | planks + glass | from loop closers — glass exists |
| Stove/hearth | cooking furniture (replace fire pit workshop?) | stone + clay | room heating source |

Don't build all at once. Add 2-3 that make room quality scoring more interesting, see how it plays, add more.

---

## The Arc

```
Step 1 (rooms)     → "this is a nice room" becomes quantifiable
Step 2 (mood)      → movers care about their room
Step 3 (distance)  → movers stay near where they live/work
Step 4 (elevators) → movers can live on floor 3
Step 5 (occupations) → "I'm the farmer, she's the crafter"
Step 6 (furniture) → rooms get personality

Together: movers who live in furnished apartments, commute by elevator/train
to their workplace, prefer nearby jobs, and are happier when their housing
is nice. Not yet a full village — no schedules, no social, no leisure —
but the bones of one.
```

After these steps, the **schedule system** (`schedule-system.md`) becomes the natural next build. It layers time-driven behavior on top of occupations and mood: "it's 7am, time to commute to the workshop" instead of "grab whatever job is nearest." That's where daily rhythm emerges.

---

## The Household Milestone — Prove It Small Before Scaling Up

The endgame vision is 200 movers in apartment buildings with elevators and train commutes. That's far away. The right intermediate target is a **single household**: 3-5 movers living in one house with a few rooms. This is the Sims moment — you should be able to watch one family and feel like they have a life.

### What "household done" looks like

A house with 4-6 rooms (bedroom, kitchen, living area, workshop/storage). 3-5 movers who:

- **Sleep in their own bed.** Each mover has an assigned or preferred bedroom. They go back to it, not a random leaf pile across the map.
- **Eat at a table.** Kitchen has a hearth/stove, a table, chairs. Movers sit down to eat instead of consuming food while standing in a field. Eating at a table gives a mood bonus over eating standing up.
- **Prefer nearby work.** The farmer walks to the farm plot outside. The crafter walks to the workshop room. Nobody treks across the map for a low-priority haul when there's work at home.
- **Have visible mood.** You can hover over a mover and see: "happy — nice bedroom, ate at table, warm." Or: "unhappy — dirty floor, no furniture, cold." The connection between what you built and how they feel is legible.
- **Care about their space.** Placing a rug in the bedroom or a lamp in the kitchen visibly improves a mover's mood. Letting the floor get dirty makes them grumpy. The house is alive — it reflects your choices back at you.

### What the player's experience is

```
Early game (existing):
  Survive. Gather sticks. Build fire. Don't die.

Homestead (this milestone):
  Build a house with rooms. Assign beds. Place a table in the kitchen.
  Watch movers eat together, sleep in their beds, walk to nearby work.
  Notice one mover is unhappy — their room is dark and dirty.
  Place a lamp, clean the floor. Mood improves. They work faster.
  Feel attachment to these 3-5 little people and their home.
```

This is the emotional pivot point. Survival is about *not dying*. The homestead is about *living well*. The player shifts from "how do I get enough food" to "how do I make Kira's bedroom nicer."

### Which steps matter for household

Not all 6 steps are equal for the household milestone:

| Step | Needed? | Why |
|------|---------|-----|
| 1. Room quality | **Yes — core** | The whole point. Rooms must be scoreable. |
| 2. Mood | **Yes — core** | Movers must react to room quality. Without mood, rooms are decorative. |
| 3. Distance weighting | **Yes — important** | Keeps movers near the house. Without it they wander off and the household dissolves. |
| 4. Elevators | No | Household is single-story or ladder-accessible. Elevators are village-scale. |
| 5. Occupations | Nice-to-have | Emergent from distance weighting — the mover near the farm farms. Formal occupations can wait. |
| 6. Furniture | **Yes — a few pieces** | Table + lamp minimum. Rug and shelf are nice. Don't need all of them. |

So the household milestone is: **steps 1 + 2 + 3 + a couple furniture items from step 6.** That's roughly 2.5-3 sessions. Elevators and formal occupations are deferred to the village push.

### Household → village transition

Once the household feels right, scaling up means:

- **Multiple households** — 2-3 houses, each with their own movers. Do they compete for resources? Share a communting workshop?
- **Shared spaces** — a communal kitchen, a market area, a workshop district. Movers from different houses meet here.
- **Transport** — when the second house is far from the farm, trains matter. When buildings go vertical, elevators matter.
- **Schedules** — with multiple households, time-driven behavior creates daily rhythm. Morning commute, lunch break, evening at the pub.

The village grows organically from the household, not as a separate design. If 1 house works, 5 houses work. If 5 houses work, a village with infrastructure works.

### What we're NOT building for household

- No relationships / social needs (movers don't need to "like" each other yet)
- No children / reproduction
- No economy / trade / shops
- No complex schedule system (just need-driven + distance weighting is enough)
- No personality traits (mood covers the essentials)
- No leisure activities (sit in chair = rest, that's enough for now)

These are all village-scale features. The household proves the foundation: rooms, mood, and locality. Everything else layers on top once that feels good.

---

## Softening Survival for the Household

The early game is about *not dying*. The household is about *living well*. But if movers are still desperately eating raw berries every 3 minutes, the player can't focus on room layout and furniture placement. Survival pressure needs to fade into background maintenance once the basics are running.

### The Problem

Right now, need decay rates are constant. A mover with 50 bread in the stockpile gets hungry at the same rate as a mover with nothing. The player is always firefighting needs, which is correct for day 1 survival but wrong for the household phase where the challenge should be quality of life.

### Approach 1: Meal Quality Duration (recommended, build this)

Better food eaten in better conditions satisfies hunger for longer. Same survival loop, but a well-run household is dramatically less frantic.

| Condition | Hunger duration | Why |
|-----------|----------------|-----|
| Raw berries, standing | 3 hours | Survival baseline — desperate, frequent |
| Cooked meal, standing | 5 hours | Better food helps |
| Cooked meal, at table | 8 hours | Full meal in a dining room — the household reward |
| Raw food, in rain | 2 hours | Penalty for bad conditions |

This means a household with a kitchen, a table, and cooked food only needs to eat ~2-3 times per day instead of 6-8 times. The survival loop still exists — you still need farms, cooking, food stockpiles — but movers spend 80% of their time on productive work instead of 50%.

**Implementation:** When a mover eats, check: food quality (raw/cooked) and eating context (at table in dining room / sitting / standing / outdoors). Multiply the hunger restored by a context multiplier. This is just a modifier on the existing eat action — no new systems.

**Connects to mood:** "Ate at table" already gives a mood bonus. Now it also gives a practical bonus (longer satiation). Double incentive to build a proper dining room.

### Approach 2: Comfort Plateau (recommended, build this)

Once all basic needs (hunger, thirst, energy, temperature) are above 70%, a "comfortable" state kicks in and all need decay rates halve. The household becomes self-sustaining — movers rarely dip into urgent need territory.

```
if (hunger > 0.7 && thirst > 0.7 && energy > 0.7 && tempComfortable):
    needDecayRate *= 0.5   // "comfortable" — everything slows down
```

Drop below 70% on anything and full decay resumes. This rewards having a working household without removing the systems. A bad winter that kills the crops breaks the comfort plateau and survival pressure returns — which is exactly right.

**Why this works:** It's an emergent difficulty curve. Early game: needs are aggressive, everything is urgent. Mid game: you build infrastructure, needs stabilize, comfort plateau kicks in. Late game: you're optimizing quality of life, not fighting to survive. The game shifts naturally without a mode switch.

### Approach 3: Homestead Scenario (recommended as dev test bed)

A starting scenario for the household milestone: 3-5 movers in a pre-built house with food and tools. The player's job isn't "survive day 1" — it's "turn this house into a home."

#### What exists already (all APIs are there)

Every piece needed to physically generate a furnished house is in the codebase:

| Thing | API | Notes |
|-------|-----|-------|
| Stone/plank walls | `grid[z][y][x] = CELL_WALL` + `SetWallMaterial()` | Direct grid writes |
| Floors | `SET_FLOOR(x, y, z)` + `SetFloorMaterial()` | Direct grid writes |
| Doors | `grid[z][y][x] = CELL_DOOR` | Direct grid write |
| Windows | `grid[z][y][x] = CELL_WINDOW` | Direct grid write |
| Roof | `SET_FLOOR(x, y, z+1)` | Floor on level above |
| Beds | `SpawnFurniture(x, y, z, FURNITURE_PLANK_BED, mat)` | Works now |
| Chairs | `SpawnFurniture(x, y, z, FURNITURE_CHAIR, mat)` | Works now |
| Workshops | `CreateWorkshop(x, y, z, WORKSHOP_HEARTH)` etc. | Works now |
| Stockpile + food | `CreateStockpile()` + `SpawnItem()` in loop | Works now |
| Torches | `AddLightSource(x, y, z, r, g, b, intensity)` | Works now |
| Movers inside | `SpawnMoversDemo()` or manual spawn at position | Works now |
| Tools + clothing | `SpawnItem()` + equip on mover | Works now |
| Farm plot | Farm grid API (till, fertilize) | Works now |
| Water supply | Spawn filled clay pots via `PutItemInContainer()` | Works now |

A `GenerateHomesteadScenario()` function in terrain.c can be written **today**. No new systems needed for the physical house.

#### What does NOT exist (the "comfortable" part)

The house can be built, but movers won't care about it until these features exist:

| Missing feature | What it does | Doc step |
|-----------------|-------------|----------|
| Table furniture | `FURNITURE_TABLE` + `ITEM_TABLE` + recipe + sprite | Step 6 |
| Room detection | Flood-fill enclosed spaces, assign room IDs | Step 1a |
| Room type classification | Bedroom/dining/kitchen from furniture | Step 1b |
| Room quality scoring | Size + furniture + light + dirt + penalties | Step 1c |
| Mood system | Float on mover + moodlets | Step 2 |
| "Ate at table" moodlet | Mood bonus for eating in dining room | Step 2 |
| Private bedroom bonus | Own enclosed room = better mood | Step 2 |
| Bed assignment | Movers remember "their" bed/room | Step 1b |
| Meal quality duration | Better food + table = longer satiation | Approach 1 |
| Comfort plateau | Needs > 70% → decay halves | Approach 2 |
| Distance weighting | Movers prefer nearby jobs | Step 3 |

#### The gap

The house can be **built** today. But movers won't **care** about it until steps 1-3 are done. Without room/mood, you'd have a nice-looking stone house where movers sleep in random beds, eat standing up, and wander across the map — functionally identical to sleeping on leaves in a field.

#### Scenario variants

- **"The Homestead"** — 4 movers, stone house shell (walls + roof + doors, no internal walls), stockpile of 30 food + 20 planks + tools. Player builds internal rooms, places furniture, sets up kitchen. Goal: make it a home.
- **"The Empty House"** — 3 movers, fully walled house with rooms already divided but zero furniture. Goal: furnish and improve every room.
- **"The Furnished House"** — 3 movers, fully built and furnished house. Goal: pure testing scenario for room quality and mood systems. Player tweaks and observes.

#### Build order for the scenario

1. **Write `GenerateHomestead()`** (~0.5 session) — terrain preset with house, furniture, stockpile, farm. Can be done NOW as a test bed, independent of room/mood. Useful for developing and testing steps 1-3.
2. **Build steps 1+2+3** (~2-3 sessions) — rooms, mood, distance weighting. This makes the house *matter*.
3. **Add table** (~0.25 session) — new furniture type + sprite. Unlocks dining room.

The scenario generator is the recommended **first thing to build**. Having a pre-built house to test against is invaluable while developing room detection and mood. You can see immediately whether rooms are detected correctly, whether movers pick the right beds, whether the "ate at table" moodlet fires. Without it, you'd have to manually build a house every time you restart to test.

#### Codebase audit: what's ready for GenerateHomestead()

Checked every API needed against the existing terrain generators (GenerateCraftingTest, GenerateTrainTest). **Everything exists except the table furniture type.**

**Terrain & structure — all proven in existing generators:**

```c
// Ground: same pattern as every generator
InitGridWithSizeAndChunkSize(64, 64, 8, 8);
grid[0][y][x] = CELL_WALL;  SetWallMaterial(x, y, 0, MAT_DIRT);  // solid ground

// Walls: stone house
grid[1][y][x] = CELL_WALL;  SetWallMaterial(x, y, 1, MAT_GRANITE);

// Constructed floor (not natural)
SET_FLOOR(x, y, 1);  SetFloorMaterial(x, y, 1, MAT_PLANKS);

// Roof (floor on z=2)
SET_FLOOR(x, y, 2);  SetFloorMaterial(x, y, 2, MAT_PLANKS);

// Doors, windows
grid[1][y][x] = CELL_DOOR;
grid[1][y][x] = CELL_WINDOW;
```

**Furniture — SpawnFurniture() works, used by construction system:**

```c
SpawnFurniture(x, y, 1, FURNITURE_PLANK_BED, MAT_OAK);   // bedroom
SpawnFurniture(x, y, 1, FURNITURE_CHAIR, MAT_OAK);        // kitchen
// FURNITURE_TABLE — DOES NOT EXIST YET (only missing piece)
```

**Workshops — CreateWorkshop() proven in GenerateCraftingTest:**

```c
CreateWorkshop(x, y, 1, WORKSHOP_HEARTH);      // kitchen — cooking
CreateWorkshop(x, y, 1, WORKSHOP_CARPENTER);    // workshop room — crafting
```

**Stockpile + items — proven in GenerateCraftingTest/TrainTest:**

```c
int sp = CreateStockpile(x, y, 1, 3, 3);
SetStockpileFilter(sp, ITEM_BREAD, true);       // etc.
SpawnItem(px, py, 1.0f, ITEM_BREAD);            // food
SpawnItem(px, py, 1.0f, ITEM_PLANKS);           // materials
SpawnItem(px, py, 1.0f, ITEM_STONE_AXE);        // tools

// Filled water pot
int pot = SpawnItem(px, py, 1.0f, ITEM_CLAY_POT);
PutItemInContainer(SpawnItem(px, py, 1.0f, ITEM_WATER), pot);
```

**Light — AddLightSource() proven in train/lighting systems:**

```c
AddLightSource(x, y, 1, 200, 180, 120, 8);     // warm torch light
```

**Movers:**

```c
SpawnMoversDemo(4);  // or manually place at specific coords
```

**Outdoors (trees, grass, farm):**

```c
SetVegetation(x, y, 0, VEG_GRASS);             // grass
PlaceWorldGenTree(x, y, 0, MAT_OAK, true);     // trees (static, not exported — may need to use inline placement)
// Farm: farm grid API exists for tilling/fertilizing
```

#### What the house layout could look like

```
 Legend: # = stone wall, . = plank floor, D = door, W = window
         B = bed, C = chair, H = hearth, K = carpenter
         S = stockpile, F = farm plot

                    W
     ####D####D########
     #B .  . #C . H   #
     #  .  . D  .    W #    <- kitchen / dining (hearth + chairs)
     ####D####  .      #
     #B .  . #.........#
     #  .  . D  . K    #    <- workshop (carpenter)
     ####D####  .    W #
     #B .  . #  . S S  #    <- storage (stockpile)
     #  .  . D  . S S  #
     #W      #W        #
     ####################

  3 bedrooms (left)     |  kitchen + workshop + storage (right)

  Outside: farm plot to the south, trees, grass, water source
```

~20x10 interior, 3 private bedrooms with beds, kitchen with hearth + chairs, workshop with carpenter, storage with stockpile. Torches in each room. Front door faces south toward the farm.

#### The one missing piece: FURNITURE_TABLE

Everything else is ready. The table needs:
1. `FURNITURE_TABLE` enum in furniture.h
2. `ITEM_TABLE` enum in items.h
3. Table sprite (8x8 — furniture + item)
4. FurnitureDef row (blocking=true, restRate=0, moveCost=0)
5. Construction recipe (CONSTRUCTION_TABLE, 1 stage: 1 ITEM_TABLE)
6. Carpenter recipe to craft ITEM_TABLE from planks
7. Save version bump + migration

Without the table, the house still works — bedrooms, workshop, storage all function. The table just means no dining room concept until it's added. Could generate the house now and add the table later.

### Approach 4: Sandbox Furniture (creative mode option)

Infinite-supply objects for sandbox/testing:
- **Bottomless pantry** — infinite food source (your fridge idea)
- **Eternal well** — infinite water
- **Self-warming hearth** — keeps temperature comfortable in radius

These don't solve the gameplay question (how does survival soften in normal play?) but they're valuable for testing the household features without survival noise. Could be sandbox-only items, or debug tools.

### What to Build

| Approach | Priority | Effort | When |
|----------|----------|--------|------|
| 1. Meal quality duration | **Build** | ~0.25 session | With mood (step 2) — eating context is already being evaluated |
| 2. Comfort plateau | **Build** | ~0.25 session | With mood (step 2) — needs + mood are being touched anyway |
| 3. Homestead scenario | Nice-to-have | ~0.5 session | With onboarding (09b) — scenario infrastructure needed |
| 4. Sandbox furniture | Nice-to-have | ~0.25 session | Anytime — independent debug tool |

Approaches 1 and 2 together cost half a session and fundamentally change the feel of mid-game. A household with cooked food, a dining table, and all needs above 70% enters a comfortable cruise where movers eat twice a day and spend most of their time working or resting. That's when room quality and mood become the game.

---

## Art Checklist

Everything that needs to be drawn or designed for the household milestone. Grouped by priority.

### Must Draw (blocks gameplay)

| Asset | Type | Size | Notes |
|-------|------|------|-------|
| **Table** | furniture sprite | 8x8 | The single most important new piece — unlocks "ate at table" moodlet. 1x1 cell. |
| **Table** | item sprite | 8x8 | Craftable item (planks at carpenter), placed via construction. Same pattern as ITEM_PLANK_BED / ITEM_CHAIR. |
| **Mood indicator** | mover overlay | tiny | 3 states: happy / neutral / sad. Could be small face icon, colored dot, or simple shape above mover head. Needs to read at zoomed-out scale. |

### Should Draw (makes it feel complete)

| Asset | Type | Size | Notes |
|-------|------|------|-------|
| **Rug** | floor overlay sprite | 8x8 | Drawn on top of floor, like dirt overlay but positive. Covers dirt, boosts room quality. Needs to look distinct from floor dirt. |
| **Rug** | item sprite | 8x8 | Crafted at loom (cloth or hide). |
| **Lamp stand** | furniture sprite | 8x8 | Floor-placed light source. Emits block light like a torch but stands on floor, not wall. |
| **Lamp stand** | item sprite | 8x8 | Crafted at carpenter (planks + glass from loop closers). |
| **Moodlet icons** | tooltip icons | tiny | ~6-8 small icons for the moodlet list in tooltip. Candidates: plate (ate at table), crossed-plate (no table), bed (slept well), broken-bed (slept badly), snowflake (cold), flame (cozy), sparkle (nice room), grime (ugly room). Can be very simple — 4x4 or 5x5 pixel symbols. |

### Nice to Have (adds depth, not blocking)

| Asset | Type | Size | Notes |
|-------|------|------|-------|
| **Shelf** | furniture sprite | 8x8 | Wall-adjacent decor/storage. Room quality filler. |
| **Shelf** | item sprite | 8x8 | Crafted at carpenter (planks). |
| **Room type icons** | tooltip icons | tiny | Small icon per room type for tooltip header: bed (bedroom), fork (dining), hammer (workshop), box (storage). Optional — text labels work fine. |

### Already Exists (no new art needed)

- Plank bed, chair, leaf pile, grass pile — furniture sprites done
- Floor dirt overlay — reused for cleanliness penalty visualization
- Light system — GetLightColor already renders dark/bright rooms
- Walls, doors, windows, floors — construction sprites done
- Mover sprites — existing, mood just adds an overlay

### Total New Sprites

| Priority | Count | What |
|----------|-------|------|
| Must | 3 | table (furniture + item), mood indicator |
| Should | 5 | rug (furniture + item), lamp (furniture + item), moodlet icons sheet |
| Nice | 3 | shelf (furniture + item), room type icons |
| **Total** | **~11** | Minimum viable: just 3. Full set: 11. |

---

## Research: How Other Games Do This

Surveyed 6 games with household/room/mood systems. Key takeaways that shaped the design above.

### RimWorld — Room Impressiveness

Room impressiveness is calculated from 4 stats: wealth, beauty, space, cleanliness. The formula is **weighted toward the lowest stat** — one bad factor tanks the whole score. Impressiveness has sharp tiers (awful/dull/mediocre/slightly impressive/etc.), and crossing a tier boundary causes a big mood swing.

Key lessons:
- **"Ate without table" (-3 mood)** is the game's most iconic moodlet. Simple, legible, fixable. Drives early furniture crafting.
- **Bedroom impressiveness persists all day** — sleep quality is a permanent mood modifier, not just while sleeping. Makes bedroom investment worthwhile.
- **Cleanliness is fragile** — a single dirt tile can drop a room one full tier. Creates continuous maintenance pressure.
- **Private bedroom > barracks** — significant mood gap incentivizes building individual rooms.

### Going Medieval — Impressiveness Formula

Similar to RimWorld: room impressiveness = wealth x beauty x spaciousness (product of multipliers). Quality tiers from "awful" to "palatial" (+12 mood). Four walls + door + roof = "spare room," furniture determines type and score.

Key lesson: **multiplicative formula** means every factor matters. A huge room with no furniture scores badly. A tiny room with expensive furniture scores badly. Balance is rewarded.

### Oxygen Not Included — Room Types & Morale

Strict room type system: a room meeting requirements for TWO types becomes "miscellaneous" with NO bonus. Forces the player to build dedicated rooms. Types include barracks, bedroom, mess hall, great hall, latrine, massage clinic.

Key lessons:
- **Decor is averaged over the whole day** — movers accumulate "decor perception" from every room they visit, averaged at end of cycle. Not just the room they sleep in.
- **Great Hall > Mess Hall** — tiered room types reward investment (table → table + decor)
- **Private bedroom >> barracks** — the mood gap is large enough to justify the space cost

### The Sims 4 — Environment Score

Room-level environment score from all objects. Heavy **penalties for negatives**: puddles (-8), broken objects (-10), no lights/windows (-5), uncovered walls (-15). Positive contributions from furniture, art, plants. Moodlets: "Decorated" / "Nicely Decorated" vs "Dirty Surroundings" / "Filthy Surroundings."

Key lessons:
- **Negatives hit harder than positives** — removing bad things matters more than adding good things
- **"No lights or windows" is an explicit penalty** — dark rooms feel bad regardless of furniture
- **Lot traits** add permanent modifiers — could inspire per-room bonuses later (e.g. "cozy hearth" room trait)

### Clanfolk — Family Household Survival

Scottish Highland family sim. Closest to our household milestone: small group (5-8), survive winter, manage schedules, children grow up and marry. 93% positive on Steam.

Key lessons:
- **Seasonal urgency** drives planning — winter prep creates meaningful time pressure (maps to our seasons system)
- **Visual priority/schedule system** — accessible task management without micromanagement
- **Family bonds emerge from proximity** — small group + shared home = attachment without explicit relationship system
- **Generational play** — not for us yet, but the emotional hook of "these are MY people" comes naturally from household scale

### Banished — Simple Happiness

5-star happiness from: housing, food variety, church, tavern, clothing. No complex formula — just "do they have the things?" Mod adds initial 60% (3-star) happiness so improvements are noticeable.

Key lesson: **Don't over-engineer.** A handful of clear, legible inputs with visible feedback beats 20 subtle factors. The player should always be able to answer "why is this mover unhappy?" in 2 seconds.

### Summary: What We Borrowed

| Insight | From | How it shaped our design |
|---------|------|--------------------------|
| Room types with specific bonuses | ONI, RimWorld, Going Medieval | Step 1b: bedroom/dining/kitchen detection |
| "Ate without table" as iconic moodlet | RimWorld | Step 2: discrete moodlet system |
| Private bedroom >> barracks >> outdoors | RimWorld, ONI | Step 1b: bedroom privacy progression |
| Penalties matter more than bonuses | Sims 4, RimWorld | Step 1c: negative factors (dark, dirty, cramped) |
| Lowest stat drags room score down | RimWorld | Step 1c: weighted-toward-minimum scoring |
| Legible named moodlets over opaque score | All games | Step 2: moodlet list on tooltip |
| Keep it simple, keep it readable | Banished | Overall: few clear inputs, visible feedback |

---

## A Mover's Story: From Surviving to Living

*A feature gap analysis told through the eyes of a household, the sequel to "Waking Up in the Grassy Plains."*

---

### Chapter 1: The House Is Built — "We Have Walls, But Not a Home"

Winter is over. We survived — barely. Three movers in a stone house with a fire pit, a farm plot outside, a carpenter workshop in the corner. The walls are solid. The roof keeps the rain out. There's a stockpile of dried meat and a barrel of water.

But step inside and it's... nothing. An open box. Everyone sleeps on leaf piles scattered on the floor. They eat standing up, wherever they happen to be when hunger hits — sometimes in the rain, sometimes in the middle of the workshop. The floor is tracked with mud from the farm.

**I built a house. But nobody lives in it. They just pass through.**

#### What's missing

- **Rooms don't exist as a concept.** The game sees walls and a roof, but doesn't know "this is a bedroom" or "this is a kitchen." There's no difference between a 4x4 stone box and a lovingly furnished living space.
- **Movers don't care about their surroundings.** They have hunger, thirst, energy, temperature — but no opinion about where they sleep, where they eat, or whether the floor is clean. A leaf pile in a muddy corner works just as well as a plank bed in a furnished room.
- **No feedback loop between building and wellbeing.** I can place furniture, but it doesn't *do* anything beyond its function (bed = sleep faster). There's no reason to make things nice.

---

### Chapter 2: The Kitchen Table — "Sit Down and Eat"

I craft a table at the carpenter workshop. Place it in the main room. Put two chairs next to it. It's the first time this space feels like something — a kitchen, maybe. A place to eat.

But nobody sits down. When lunchtime hits, Kira grabs a piece of bread from the stockpile and eats it while walking to the farm. Torvald eats standing at the workshop. Enna eats outside in the rain.

**I built a dining room. But the game doesn't know what a dining room is.**

#### What's missing

- **No "ate at table" mechanic.** Movers eat wherever they are. There's no concept of seeking a table, sitting down, having a meal. No mood difference between eating at a table and eating in a ditch.
- **No room type detection.** A room with a table and chairs should be recognized as a dining room. A room with a bed should be a bedroom. The game doesn't classify rooms by their furniture.
- **No reason to eat together.** Even if movers could sit at a table, there's no social benefit to shared meals. That's fine for now — but the table is the foundation.

---

### Chapter 3: Bedrooms — "That's Kira's Room"

I divide the house with internal walls. Three small bedrooms, each with a plank bed. I move Kira's leaf pile out and give her a real bed in a real room with a door.

But she doesn't care. She might sleep in her room. She might sleep in Torvald's. She might sleep on the leaf pile by the fire pit because it was closer when she got tired. There's no "this is my room."

**I gave everyone their own bedroom. But nobody knows it's theirs.**

#### What's missing

- **No bed assignment / room ownership.** Movers don't have "their" room. They sleep in the nearest available bed. There's no attachment to a personal space.
- **No private bedroom bonus.** Sleeping in your own enclosed room should feel better than a communal sleeping area. The difference between barracks and bedroom is huge in RimWorld and ONI — it should be here too.
- **No room quality affecting rest.** A dark, dirty, cramped room with a bed restores the same energy as a clean, bright, spacious room. The plank bed is the only thing that matters.

---

### Chapter 4: The Dirty Floor — "It's All Mud In Here"

The movers track dirt in from the farm. The floor is filthy. I designate it for cleaning, and eventually someone sweeps. But five minutes later it's dirty again. Kira walks through the muddy farm, straight into the bedroom, mud everywhere.

I don't really mind. Or rather — **nobody** minds. The movers don't notice. The dirt is cosmetic. I could leave the floor caked in mud forever and nothing would change.

**Dirt exists. Cleaning exists. But neither matters.**

#### What's missing

- **Dirt doesn't affect mood.** Floor dirt is tracked and rendered, cleaning jobs exist, but there's no consequence. A filthy room is functionally identical to a clean one.
- **No room quality penalties.** Dark rooms, dirty floors, unfinished walls — none of these penalize the movers who live there. There's no "fix what's wrong" pressure.
- **No rug to cover the mess.** A rug over the entryway could reduce dirt tracking. It's a nice item that would serve a real purpose.

---

### Chapter 5: What I Want — "Let Them Notice"

I look at this house I've built. Three bedrooms, a kitchen with a table and chairs, a workshop room, a stockpile in the corner. It's the best house I've ever built in this game. And it might as well be an empty cave, because the movers don't know the difference.

Here's what I want:

1. **Kira sleeps in her bedroom. She knows it's hers.** She walks past the other beds and goes to her room. In the morning, she wakes up and the game says: "Slept well (+2) — private bedroom, clean, bright."

2. **Torvald eats at the table.** He walks to the kitchen, sits down, eats his bread. The game says: "Ate at table (+2)." When the kitchen is busy, Enna eats standing by the stockpile: "Ate without table (-3)." I think: I should build a bigger dining room. Or a second table.

3. **Enna notices the dirty floor.** She walks into her bedroom and the game says: "Ugly room (-1) — dirty floor." I designate cleaning. Or I craft a rug to cover the worst of it. Next time: "Nice room (+1)." She works a little faster today.

4. **I place a lamp in the hallway.** The rooms were dark — "No light (-1)" on everyone. Now the light spreads through the house. The penalty disappears. Small thing, big difference.

5. **The movers stay near home.** The farmer walks to the farm. The crafter works in the workshop room. Nobody hikes across the map for a low-priority hauling job. They feel like they *live here*.

That's it. That's the household. Not a village, not an apartment building — just a house where three people live and the game knows they live there.

---

### Chapter 6: What This Unlocks — "The House Is Alive"

Once this works, the house becomes a game in itself:

- **Room planning matters.** Where do I put the kitchen? Near the stockpile for convenience, or near the window for light? The bedroom should be far from the noisy workshop.
- **Furniture is meaningful.** Every piece I craft and place changes someone's mood. A table isn't just decoration — it's +2 for everyone who eats there, every day.
- **Cleaning is a real job.** Not just cosmetic — dirty floors make movers unhappy. I need to balance cleaning time against productive work.
- **Progression is visible.** Day 1: sleeping on leaves in a dirt-floor shack, eating in the rain. Day 30: private bedrooms with plank beds, a kitchen with a table and lamp, clean stone floors. The movers are happier. They work faster. I can *see* the improvement in their moodlet list.
- **Attachment forms.** These aren't three interchangeable workers anymore. That's Kira's room. Torvald always eats at the left chair. Enna's bedroom has the window. When I watch them go about their day — sleep, eat, work, come home — it feels like a tiny Sims household. A home, not a factory.

The village can wait. First, make the house feel like a home.

---

## Relationship to Other Docs

- `endgame-village-vision.md` — the destination picture this builds toward
- `naked-in-the-grass.md` — predecessor story (survival gaps, mostly resolved)
- `schedule-system.md` — next major system after these steps
- `10-personality-and-skills.md` — mood + occupations are prerequisites for personality
- `sims-inspired-priorities.md` — confirms mood-first ordering
- `world/elevators.md` — step 4 detail
- `pathfinding/transport-layer.md` — step 4 infrastructure (Layers 1-2 done, Layer 4 = elevator)
- `mover-control-and-work-radius.md` — step 3 expands on distance weighting and draft mode
- `gameplay/more-tiles-design.md` — step 6 overlaps with hearth/table tiles
