# Progression Gaps Analysis

An analysis of what's missing from the current progression system and how to complete it into a coherent bare-hands → stone fortress arc.

**Date:** 2026-02-11
**Context:** Based on `progression-story.md` and current codebase state

---

## Critical Gaps in Current Progression

### 1. **Rock Gathering is Unrealistic** 🔴

**Problem:** Story says "mining with bare hands" but that's not believable for the first minutes of play.

**Solutions (from docs/todo/design/primitives-missing.md):**
- **Surface scatter** - spawn loose ITEM_ROCK on ground near stone during worldgen (finite, free pickup)
- **Boulders** - lone 1-cell natural stone walls on surface → designate → gather → 3-5 rocks, wall consumed
- **River stones** - rocks near water, bare-hands gathering

**Impact:** Enables stonecutter without mining, makes early game feel grounded.

**Implementation Notes:**
- Surface scatter: worldgen spawns random ITEM_ROCK near exposed MAT_GRANITE/SANDSTONE/SLATE cells
- Boulder designation: reuse DESIGNATION_GATHER pattern, new JOBTYPE_GATHER_BOULDER
- River stones: similar to surface scatter but near water cells

---

### 2. **Cordage Has No Purpose** 🔴

**Problem:** SHORT_STRING and CORDAGE exist in code but aren't used anywhere. The rope maker produces them, then... nothing.

**Current State:**
- ITEM_SHORT_STRING exists (from bark x2 → 3 string, or dried grass x4 → 2 string)
- ITEM_CORDAGE exists (from short string x3 → 1 cordage)
- Rope Maker workshop implemented with recipes
- But NO recipes or construction uses cordage as input

**Solutions (from docs/todo/design/construction-staging.md):**
- **Construction staging** - walls need cordage to bind frame together
  - Frame stage: STICKS + CORDAGE → frame
  - Fill stage: PLANKS/BRICKS/BLOCKS → finished wall
- **Primitive construction** - lean-to, wattle-and-daub needs cordage as lashing

**Impact:** Closes the fiber loop, makes cordage meaningful, adds construction depth.

**Implementation Notes:**
- Add `stage` field to Blueprint struct
- Add per-stage input requirements to blueprint system
- Frame stage recipe: 2 STICKS + 1 CORDAGE (creates frame structure)
- Fill stage recipe: varies by material (4 PLANKS, 6 BRICKS, 4 BLOCKS)

---

### 3. **No Primitive Shelter** 🔴

**Problem:** Mover goes from nothing → stone blocks. No intermediate shelter tier.

**Current Gap:**
- Smallest buildable structure uses PLANKS (requires sawmill) or BLOCKS (requires stonecutter + mining)
- No "day 1" shelter option using only gathered materials

**Solutions (from docs/todo/design/primitives-missing.md + workshops-master.md):**
- **Thatch roofing** - DRIED_GRASS as roofing material (item already exists)
- **Wattle-and-daub walls** - STICKS + CORDAGE frame + DIRT/CLAY daub
- **Lean-to structure** - POLES + CORDAGE + DRIED_GRASS (simple 1x2 shelter)

**Impact:** Natural progression: lean-to → wattle-daub hut → wood cabin → stone keep.

**Implementation Notes:**
- New wall type: CELL_WALL with MAT_WATTLE_DAUB material
- Construction recipe: Frame stage (STICKS + CORDAGE) + Daub stage (DIRT or CLAY x3)
- New roof/floor type using DRIED_GRASS (thatch material)
- Lean-to as single-tile angled structure (new cell type or special blueprint)

---

### 4. **Sand/Dirt/Leaves Are Dead Ends** 🟡

**Problem:** These items are gathered but have no recipe sinks.

**Current State:**
- ITEM_SAND - gatherable from MAT_SAND cells, no recipe use
- ITEM_DIRT - gatherable from MAT_DIRT cells, no recipe use
- ITEM_LEAVES - gatherable from trees, only use is fuel (IF_FUEL flag)

**Solutions:**
- **Sand** → Glass Kiln (SAND + fuel → GLASS) for windows
  - New workshop: WORKSHOP_GLASS_KILN
  - Recipe: SAND x2 + 1 fuel → GLASS x1
  - GLASS used for windows, containers (future)
- **Dirt** → Farming system (plant crops in dirt)
  - Requires full farming system (future, deferred)
  - Alternative: composting (DIRT + LEAVES → COMPOST for enriched soil)
- **Leaves** → Composting (LEAVES + DIRT → COMPOST) for farming
  - Or: Mulch recipe (LEAVES x5 → MULCH x1 at workshop)
  - MULCH used as soil amendment (future farming) or bedding

**Impact:** Closes gathering loops, enables farming/glass systems.

**Implementation Priority:**
1. Glass Kiln (simple, closes sand loop immediately)
2. Leaf mulching (simple workshop recipe)
3. Composting (ties dirt + leaves together, enables farming later)

---

### 5. **Construction Is Too Simple** 🟡

**Problem:** Place blueprint → haul 1 item → wall appears instantly. No time investment, no staging.

**Current System:**
- Blueprint placed → JOBTYPE_BUILD created → mover walks to site → 3s work → wall appears
- No material preparation, no multi-step process, no visible construction phases

**Solutions (from docs/todo/design/construction-staging.md):**
- **Frame stage** - build skeleton first (fast, uses sticks/poles/cordage)
  - Visible as transparent/ghosted structure
  - Walkable (or not, depending on design)
- **Fill stage** - add walls/floors (slower, uses planks/bricks/blocks)
  - Each stage = separate hauling burst + work time
- **Curing/settling stage** (optional, future) - wait time before usable

**Impact:** Construction feels earned, stockpile logistics matter, progression is visible.

**Implementation Notes:**
- Add `uint8_t stage` to Blueprint struct (0 = frame, 1 = fill, 2 = complete)
- Add `Recipe frameRecipe` and `Recipe fillRecipe` to construction system
- JOBTYPE_BUILD checks current stage, uses appropriate recipe
- Visual: render partial structures differently per stage
- Storage checks need to verify space for BOTH stages' outputs

---

### 6. **No Water Interaction** 🟡

**Problem:** Water exists (flows, visible) but movers never use it.

**Current State:**
- Water simulation works (cellular automata, flow, evaporation)
- Water tiles render correctly
- But: no crafting uses water, no hauling water, no water-dependent recipes

**Solutions (from docs/todo/design/water-dependent-crafting.md):**
- **Water placement tools** - SetWaterSource/SetWaterDrain already exist, just need UI actions
  - ACTION_PLACE_WATER_SOURCE
  - ACTION_PLACE_WATER_DRAIN
- **Mud Mixer workshop** - DIRT + CLAY + water tile → MUD (location-based, no container needed)
  - Must be placed on water cell or adjacent to water
  - Recipe checks for water at work tile
  - MUD used for wattle-daub construction
- **Simple container** (future) - ITEM_BASKET (woven from cordage) for hauling water later

**Impact:** Water becomes gameplay element, enables mud/cob construction, unlocks clay-working area.

**Implementation Notes:**
- Add water placement actions to ACTION_REGISTRY
- Add WORKSHOP_MUD_MIXER with placement validator (CheckAdjacentWater)
- Recipe: DIRT x2 + CLAY x1 → MUD x3 (no fuel, location provides water)
- Future: add ITEM_WATER (requires containers) for hauling to non-water locations

---

### 7. **No Tools = No Progression** 🟡

**Problem:** Everything is done with bare hands at same speed forever.

**Current State:**
- All jobs (mining, chopping, crafting) have fixed work times
- No equipment/tool system
- No job speed modifiers
- ITEM_AXE and ITEM_PICKAXE mentioned in docs but not in items.h

**Solutions (from docs/todo/jobs/overview.md):**
- **Stone hammer** - ROCK + STICKS → faster mining (2x speed)
- **Stone axe** - ROCK + STICKS → faster chopping (2x speed)
- **Tools system** - durability, job speed multipliers
  - Add `int heldItem` to Mover struct (-1 = bare hands)
  - Add tool requirements to job types (mining prefers pickaxe, chopping prefers axe)
  - Jobs check for tool, apply speed multiplier
  - Tools have durability, break after N uses

**Impact:** Progression from slow bare-hands → efficient tooled work. Closes stone→stick loop.

**Implementation Priority:**
- Phase 1: Basic tools (stone hammer/axe) with speed bonuses, no durability
- Phase 2: Tool durability and breakage
- Phase 3: Tool tiers (stone → bronze → iron)

---

### 8. **Block Types Feel Identical** 🟡

**Problem:** Stone blocks, brick blocks, wood planks all build the same way with same effort.

**Current State:**
- ITEM_BLOCKS (stone), ITEM_BRICKS, ITEM_PLANKS all have IF_BUILDING_MAT flag
- Construction blueprint consumes 1 item, places wall instantly
- No material differentiation in construction time, staging, or properties

**Solutions:**
- **Material properties**
  - Stone walls are fireproof, wood walls burn, brick is middle ground
  - Already partially implemented: material system exists, fire system exists
  - Need: wall flammability based on wallMaterial
- **Construction time**
  - Stone takes 2x longer to place than wood (BLOCKS: 6s work, PLANKS: 3s work)
  - Brick is middle (4s work)
- **Staging requirements**
  - Stone needs frame + fill (2 stages)
  - Wood needs frame + fill (2 stages)
  - Wattle-daub needs frame + daub (2 stages)
- **Visual differentiation**
  - Distinct sprites for block walls vs brick walls (mentioned as missing in overview.md)
  - Currently: blocks/bricks use generic material sprite
  - Need: dedicated SPRITE_wall_stone_block, SPRITE_wall_brick, etc.

**Impact:** Material choice matters, construction feels different per tier.

**Implementation Notes:**
- Add per-material work time multipliers to construction jobs
- Fire system already checks wallMaterial, just needs flammability table
- Sprite atlas needs dedicated wall sprites (currently reusing item sprites)

---

## Suggested Priority Order (Most Impact First)

### Phase A: Complete the Fiber Loop ⭐⭐⭐
**Why:** Cordage exists but is useless. This is the biggest immediate gap.

**Tasks:**
1. **Construction staging** (frame + fill) - uses cordage, sticks, poles
   - Add `stage` field to Blueprint struct
   - Frame recipe: STICKS x2 + CORDAGE x1
   - Fill recipe: material-dependent (PLANKS/BRICKS/BLOCKS)
2. **Primitive shelter tiles** - lean-to, wattle-daub using cordage
   - New material: MAT_WATTLE_DAUB
   - Construction: frame (STICKS + CORDAGE) + daub (DIRT/CLAY)

**Time Estimate:** ~2-3 days
**Unlocks:** Cordage purpose, early shelter, construction depth
**Closes Loops:** Bark → string → cordage → construction

---

### Phase B: Bare-Hands Rock Gathering ⭐⭐
**Why:** Makes first 5 minutes believable.

**Tasks:**
1. **Surface scatter** - spawn loose rocks during worldgen
   - Worldgen: place ITEM_ROCK near exposed stone (1-3 rocks per stone patch)
2. **Boulder designation** - gather job on natural stone walls
   - DESIGNATION_GATHER_BOULDER on surface stone walls
   - JOBTYPE_GATHER_BOULDER: 5s work → 3-5 ITEM_ROCK, wall consumed
3. **River stones** - gather job near water
   - Worldgen: scatter ITEM_ROCK near water cells
   - Or: DESIGNATION_GATHER_RIVER_STONE (similar to boulder)

**Time Estimate:** ~1 day
**Unlocks:** Realistic early game, no "mining with bare hands"
**Closes Loops:** N/A (enables existing loops earlier)

---

### Phase C: Water Access ⭐⭐
**Why:** Water exists but is ignored. Small effort, big payoff.

**Tasks:**
1. **Water placement UI** - actions for SetWaterSource/SetWaterDrain (functions exist)
   - ACTION_PLACE_WATER_SOURCE and ACTION_PLACE_WATER_DRAIN
   - Add to ACTION_REGISTRY, wire to input handler
2. **Mud Mixer workshop** - DIRT + CLAY + water → MUD (no container needed yet)
   - WORKSHOP_MUD_MIXER with template
   - Placement validator: CheckAdjacentWater (work tile must be water or adjacent)
   - Recipe: DIRT x2 + CLAY x1 → MUD x3

**Time Estimate:** ~1-2 days
**Unlocks:** Water gameplay, mud/cob construction, clay-working areas
**Closes Loops:** Dirt + clay → mud (partial, needs mud sink)

---

### Phase D: Close Open Loops ⭐
**Why:** Items with no purpose feel incomplete.

**Tasks:**
1. **Glass Kiln** - SAND + fuel → GLASS (closes sand loop)
   - WORKSHOP_GLASS_KILN (similar to kiln, needs fuel)
   - Recipe: SAND x2 + 1 fuel → GLASS x1
   - ITEM_GLASS added to items.h
2. **Leaves composting** - LEAVES → COMPOST or MULCH (closes leaf loop)
   - Recipe at new workshop or existing (Hearth? Drying Rack?)
   - LEAVES x5 → MULCH x1 (no fuel, passive or short work)
3. **Thatch usage** - DRIED_GRASS as roofing material in construction
   - Add MAT_THATCH to materials
   - Roof construction: DRIED_GRASS x4 → thatch roof tile

**Time Estimate:** ~2-3 days
**Unlocks:** All gatherable items have purpose, no dead ends
**Closes Loops:** Sand → glass, leaves → mulch, dried grass → roofing

---

### Phase E: Tools (Later) ⭐
**Why:** Bigger system, can wait until loops are closed.

**Tasks:**
1. Tools system (durability, speed bonuses)
   - Add `int heldItem` to Mover struct
   - Add tool requirements to JobType
   - Add speed multipliers (bare hands: 1.0x, stone tools: 2.0x)
2. Stone hammer, stone axe recipes
   - ITEM_STONE_HAMMER, ITEM_STONE_AXE
   - Recipes at Stonecutter: ROCK + STICKS x2 → tool
3. Tool durability (optional, phase 2)
   - Add `uint16_t durability` to Item struct
   - Tools break after N uses, spawn broken item

**Time Estimate:** ~3-5 days
**Unlocks:** Efficiency progression, tool economy
**Closes Loops:** Rock + sticks → tools (closes multi-loop)

---

## Complete Progression Arc (After All Phases)

### Era 0: Bare Hands (0-10 minutes)
**Available actions:**
- Gather surface rocks (surface scatter, boulders, river stones)
- Gather grass (tall grass → ITEM_GRASS)
- Gather sticks/leaves from living trees (tree harvest system)
- Gather saplings (dig up)
- Place hearth, burn fuel → ash

**Outputs:**
- Rock, grass, sticks, leaves, saplings, poles, ash

---

### Era 0.5: Cordage & Primitive Shelter (10-30 minutes)
**Unlocked by:** Drying Rack + Rope Maker

**Available actions:**
- Dry grass → dried grass
- Twist bark/dried grass → short string
- Braid string → cordage
- Build lean-to (POLES + CORDAGE + DRIED_GRASS)
- Build wattle-daub walls (frame: STICKS + CORDAGE, daub: DIRT/CLAY)

**Outputs:**
- Dried grass, short string, cordage, primitive shelter

**Loop closure:** Bark → string → cordage → construction (fiber loop complete)

---

### Era 1: Stone Tools (30-60 minutes)
**Unlocked by:** Stonecutter + surface rock gathering

**Available actions:**
- Cut stone blocks (ROCK → BLOCKS x2)
- Craft stone hammer (ROCK + STICKS → faster mining)
- Craft stone axe (ROCK + STICKS → faster chopping)
- Build stone walls (staged: frame + stone fill)

**Outputs:**
- Stone blocks, stone tools, stone construction

**Loop closure:** Rock + sticks → tools (closes tool loop)

---

### Era 2: Wood Processing (1-2 hours)
**Unlocked by:** Sawmill + efficient logging (stone axe)

**Available actions:**
- Saw planks (LOG → PLANKS x4)
- Cut sticks (LOG → STICKS x8)
- Strip bark (LOG → STRIPPED_LOG + BARK x2)
- Build wood structures (staged: frame + plank fill)
- Twist bark → string → cordage (fiber loop sustained)

**Outputs:**
- Planks, sticks, bark, cordage, wood construction

**Loop closure:** Logs → planks/bark → construction/cordage (wood loop complete)

---

### Era 3: Mud/Clay & Fire (2-4 hours)
**Unlocked by:** Water access + Mud Mixer + Kiln

**Available actions:**
- Mix mud (DIRT + CLAY + water → MUD)
- Build mud/cob structures (MUD + DRIED_GRASS → cob)
- Fire bricks (CLAY + fuel → BRICKS x2)
- Make charcoal (LOG → CHARCOAL x3 at kiln or charcoal pit)
- Build brick walls (staged: frame + brick fill)

**Outputs:**
- Mud, cob, bricks, charcoal, brick construction

**Loop closure:** Charcoal → fuel → more charcoal (fuel loop self-sustaining)

---

### Era 4: Stone & Glass (4+ hours)
**Unlocked by:** Efficient mining (stone hammer) + Glass Kiln

**Available actions:**
- Mine stone efficiently (2x speed with hammer)
- Melt glass (SAND + fuel → GLASS)
- Build stone fortifications (staged: frame + block fill)
- Install windows (GLASS in construction)
- Build multi-story structures (ladders, ramps, floors)

**Outputs:**
- Stone blocks, glass, advanced construction, fortified base

**Loop closure:** All loops complete, sustainable production chains

---

## What This Achieves

**Before (current state):**
- ✅ Gather → workshops → construction works
- ❌ Cordage is pointless
- ❌ Rock gathering is unrealistic (bare-hands mining)
- ❌ Construction is instant/identical for all materials
- ❌ Sand/dirt/leaves are dead ends
- ❌ Water is decorative only
- ❌ No primitive shelter
- ❌ No tool progression

**After (complete progression):**
- ✅ All gathered items have purpose (no dead ends)
- ✅ Cordage closes fiber loop (bark/grass → string → cordage → construction)
- ✅ Realistic rock gathering (surface scatter, boulders, river stones)
- ✅ Construction feels earned (staging, time investment, visible phases)
- ✅ Sand → glass, leaves → mulch, dirt → mud (loops closed)
- ✅ Water is interactive (placement, mud mixer, future hauling)
- ✅ Primitive shelter (lean-to, wattle-daub) bridges bare hands → stone
- ✅ Tool progression (bare hands → stone tools → efficiency boost)
- ✅ Material differentiation (wood/brick/stone feel different)

**Progression arc:**
1. Bare hands gathering → 2. Cordage/primitive shelter → 3. Stone tools → 4. Wood processing → 5. Mud/clay/fire → 6. Stone fortifications

Each era builds on the previous, no gaps, all items meaningful.

---

## Smallest Complete Next Step

If you want **one feature that does the most**, I recommend:

### **Construction Staging + Cordage Use** (~2-3 days)

**Why this first:**
- Fixes the biggest gap: cordage exists but is useless
- Transforms construction from instant → multi-phase process
- Enables primitive shelter (wattle-daub)
- Makes stockpile logistics meaningful
- Sets foundation for material differentiation

**What it includes:**
1. Add `stage` field to Blueprint struct
2. Frame stage recipe: STICKS x2 + CORDAGE x1
3. Fill stage recipe: PLANKS/BRICKS/BLOCKS (material-dependent)
4. Visual: render partial structures differently
5. Wattle-daub variant: frame + DIRT/CLAY daub

**What it unlocks:**
- Cordage loop complete (bark → string → cordage → construction)
- Primitive shelter (wattle-daub hut)
- Construction progression (lean-to → hut → cabin → fortress)
- Foundation for future staging complexity (curing, multi-colonist, etc.)

This single feature would transform the progression from "instant walls" to "build shelter, then upgrade to cabin, then fortress."

---

## Cross-References

**Related Documents:**
- `progression-story.md` - Current progression narrative (what works today)
- `overview.md` - Design overview and document map
- `primitives-missing.md` - Early-game items needed (cordage, dried grass, poles)
- `construction-staging.md` - Multi-phase construction system
- `water-dependent-crafting.md` - Mud mixer, moisture states, water placement
- `workshops-master.md` - Workshop/item loop master doc
- `tech-tree-outline.md` - Era progression framework

**Implementation Status:**
- ✅ Fiber chain partial: bark/grass → string → cordage (Rope Maker exists)
- ❌ Cordage sink: no recipes use it yet
- ✅ Water simulation: works
- ❌ Water interaction: no placement UI, no water-dependent crafting
- ❌ Construction staging: instant single-step only
- ❌ Primitive shelter: no wattle-daub, lean-to, or thatch
- ❌ Tool system: no tools, no speed bonuses, no durability
- ❌ Glass/composting: sand/leaves have no sinks
