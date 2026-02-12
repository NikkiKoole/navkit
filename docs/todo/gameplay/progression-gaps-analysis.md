# Progression Gaps Analysis

An analysis of what's missing from the current progression system and how to complete it into a coherent bare-hands → stone fortress arc.

**Date:** 2026-02-11
**Updated:** 2026-02-12
**Context:** Based on `progression-story.md` and current codebase state

---

## Critical Gaps in Current Progression

### 1. ~~**Rock Gathering is Unrealistic**~~ RESOLVED

**Original concern:** "mining with bare hands" not believable.

**Resolution:** Mining is bare-hands by default — all movers have `canMine=true` with no tool requirement. Rocks are regular ground items that movers haul normally. This is consistent with the game's current "no tools" state. When tools are added later, mining could be gated behind a pickaxe, at which point surface scatter/boulders would become relevant as an early-game source.

**Optional future enhancement:** Surface scatter, boulders, or river stones could add variety to rock sourcing even without tool gating.

---

### 2. ~~**Cordage Has No Purpose**~~ RESOLVED

**Original problem:** SHORT_STRING and CORDAGE existed but weren't used anywhere.

**Resolution (2026-02-12):** Construction staging implemented. Cordage is now consumed by:
- Wattle & daub wall, frame stage: 2 STICKS + 1 CORDAGE
- Plank wall, frame stage: 2 STICKS + 1 CORDAGE

Fiber loop is now **CLOSED**: bark/dried grass → short string → cordage → construction.

---

### 3. ~~**No Primitive Shelter**~~ MOSTLY RESOLVED

**Original problem:** No intermediate shelter between nothing and stone blocks.

**Resolution (2026-02-12):** Two primitive construction options now exist:
- **Wattle & daub wall** (2-stage): frame (sticks + cordage) → fill (dirt or clay). Uses only gathered materials.
- **Thatch floor** (2-stage): base (dirt/gravel/sand) → finish (dried grass).

**Remaining:** Lean-to structure (POLES + CORDAGE + DRIED_GRASS) not yet implemented. POLES still have no construction sink.

---

### 4. **Sand/Dirt/Leaves Are Dead Ends** 🟡 (PARTIALLY ADDRESSED)

**Problem:** These items are gathered but have limited recipe sinks.

**Current State (updated 2026-02-12):**
- ITEM_SAND - now used in thatch floor base (construction sink). No workshop recipe sink yet.
- ITEM_DIRT - now used in wattle & daub fill + thatch floor base (construction sinks). No workshop recipe sink yet.
- ITEM_LEAVES - gatherable from trees, only use is fuel (IF_FUEL flag). No recipe sink.

**Remaining solutions:**
- **Sand** → Glass Kiln (SAND + fuel → GLASS) for windows. Simplest next workshop.
- **Dirt** → Farming system (future) or Mud Mixer (DIRT + CLAY + water → MUD).
- **Leaves** → Composting (LEAVES + DIRT → COMPOST) or Mulch recipe.

**Implementation Priority:**
1. Glass Kiln (simple, closes sand loop fully)
2. Leaf mulching (simple workshop recipe)
3. Composting (ties dirt + leaves together, enables farming later)

---

### 5. ~~**Construction Is Too Simple**~~ RESOLVED

**Original problem:** Place blueprint → haul 1 item → wall appears instantly.

**Resolution (2026-02-12):** Construction staging fully implemented via ConstructionRecipe system:
- 10 recipes total, 3 are multi-stage (wattle & daub, plank wall, thatch floor).
- Blueprint struct has `stage`, per-stage delivery tracking, consumed item records.
- Each stage is a separate hauling burst + work time with per-stage build times.
- Lossy refund (75%) for completed stages on cancellation.
- UI: "S1/S2" stage overlay, "Stage: X/Y" tooltip.
- Deferred: curing/settling timers, tool checks, multi-colonist stages.

---

### 6. **No Water Interaction** 🟡 (PARTIALLY ADDRESSED)

**Problem:** Water exists (flows, visible) but movers never use it for crafting.

**Current State (updated 2026-02-12):**
- Water simulation works (cellular automata, flow, evaporation)
- Water placement tools now in sandbox UI: ACTION_SANDBOX_WATER (click=water, shift+click=source, right=remove, shift+right=drain)
- But: no crafting uses water, no hauling water, no water-dependent recipes

**Remaining:**
- **Mud Mixer workshop** - DIRT + CLAY + water tile → MUD (location-based, no container needed)
- **Simple container** (future) - ITEM_BASKET (woven from cordage) for hauling water
- Water placement in survival mode (not just sandbox) could use designation-based actions

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

**Current state (updated 2026-02-12):**
- ✅ Gather → workshops → construction works
- ✅ Cordage closes fiber loop (bark/grass → string → cordage → construction)
- ✅ Rock gathering works (bare-hands mining, canMine=true by default)
- ✅ Construction feels earned (multi-stage frame+fill, hauling bursts)
- ✅ Primitive shelter exists (wattle & daub walls, thatch floors)
- 🟡 Sand/dirt partially addressed (construction sinks, no workshop sinks)
- ❌ Water is decorative only (placement tools in sandbox, no crafting uses)
- ❌ Leaves are dead ends (fuel only)
- ❌ ASH, POLES have no sinks
- ❌ No tool progression

**After remaining phases:**
- ✅ All gathered items have purpose (no dead ends)
- ✅ Sand → glass, leaves → mulch, dirt → mud (loops closed)
- ✅ Water is interactive (placement, mud mixer, future hauling)
- ✅ Tool progression (bare hands → stone tools → efficiency boost)
- ✅ Material differentiation (wood/brick/stone feel different)

**Progression arc:**
1. Bare hands gathering → 2. Cordage/primitive shelter → 3. Stone tools → 4. Wood processing → 5. Mud/clay/fire → 6. Stone fortifications

Steps 1-2 are now implemented. Steps 3+ remain future work.

---

## Smallest Complete Next Step

~~**Construction Staging + Cordage Use** — DONE.~~ This was the highest-impact feature and is now fully implemented.

### **Next highest-impact feature: Glass Kiln** (~1 day)

**Why this next:**
- Closes the sand loop (SAND currently only has construction sink)
- Adds first tier 2 workshop
- GLASS enables future windows, containers
- Simple implementation: follows existing Kiln pattern (fuel-based workshop)

**What it includes:**
1. Add WORKSHOP_GLASS_KILN (3x3, fuel-based)
2. Add ITEM_GLASS to items
3. Recipe: SAND x2 + 1 fuel → GLASS x1
4. Add stockpile filter for GLASS

**Alternative next step: Fire auto-light** (~half day)
- Small integration: fire cells emit proportional block light
- Makes fire system feel more complete
- Connects two existing systems (fire + lighting)

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

**Implementation Status (updated 2026-02-12):**
- Done: Fiber chain complete: bark/dried grass → string → cordage → construction (fiber loop CLOSED)
- Done: Construction staging — 10 recipes (3 multi-stage: wattle & daub, plank wall, thatch floor)
- Done: Cordage sinks — used in wattle & daub + plank wall frame stages
- Done: Sand/dirt partial sinks — used in construction recipes (thatch floor base, wattle & daub fill)
- Done: Water placement in sandbox UI (ACTION_SANDBOX_WATER)
- Done: Hearth (fuel sink → ash), Charcoal Pit (semi-passive)
- Done: Terrain sculpting (sandbox mode, circular brush, raise/lower)
- Done: Sapling/leaf unification (material-based), tree harvest system (sticks/poles/leaves from living trees)
- Done: Floor dirt tracking + cleaning, lighting system (sky+block light, torch placement, save/load), vegetation grid
- Done: Data-driven stockpile filters (25 item type filters, 4 material sub-filters)
- Done: Primitive shelter — wattle & daub walls + thatch floors exist
- Done: Rock gathering — mining is bare-hands (canMine=true by default, no tools)
- TODO: Tool system — no tools, no speed bonuses, no durability
- TODO: Glass/composting — sand/leaves have no workshop recipe sinks
- TODO: Water-dependent crafting — no mud mixer, no moisture states
- TODO: ASH, POLES still need sinks
- TODO: Fire auto-light — fire cells don't illuminate surroundings
