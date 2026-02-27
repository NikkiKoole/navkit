# Unified Architecture Roadmap

> How seven design documents become one build order for Era 2: "The Village."

---

## 1. State of Play

Features 1-9 are complete (save v82, ~50k LOC). The full survival loop is closed: hunger, thirst, temperature, fatigue, foraging, hunting, farming, cooking, crafting, building, clothing, tools, containers, fog of war, spoilage, weather/seasons, mud, glass/lye/mortar. Era 0-1 is playable.

Seven design documents describe what comes next. Each was written in isolation. None of them reference each other or define a build order across features. This roadmap ties them together, identifies the critical path, and lays out a concrete session plan for reaching Era 2 — a colony where movers have names, skills, and moods; where different biomes create different strategies; and where the capability system begins unifying production and daily life.

---

## 2. Document Inventory

| Doc | Type | Horizon | Status | Path |
|-----|------|---------|--------|------|
| [Personality & Skills (F10)](../10-personality-and-skills.md) | Feature spec | Next | Not started | Names → Skills → Mood → Mood UI → Specialization |
| [Biome Presets (F12)](../12-biome-presets.md) | Feature spec | Next | Not started | 6 free biomes, then 2 with new assets |
| [Mover Control](../mover-control-and-work-radius.md) | Architecture | Next | Brainstorm | Draft mode + distance weighting |
| [Onboarding (F09b)](../09-onboarding-and-progression.md) | QoL | Soon | Not started | Starting supplies → progressive unlocks → scenarios |
| [Capability System](./capability-based-tasks.md) | Architecture | Foundation | Design complete | Phase A (data) → B (one recipe) → C (needs) → D (new recipes) → E (stations) |
| [Workshop Evolution](../workshop-evolution-plan.md) | Architecture | Long-term | Phase 1 (templates) complete | Phase 2 = Capability Phase E (stations) |
| [Gap Analysis](../naked-in-the-grass.md) | Vision | Reference | 8/10 done | Remaining: personality, biomes |
| [Sims-Inspired Priorities](../sims-inspired-priorities.md) | Design | Next | New | Reorders S18-S22: mood → visual feedback → comfort/fun → room quality → skills |

---

## 3. Dependency Graph

```
                    ┌─────────────┐
                    │  F10 Names  │ ← critical path starts here
                    │  (Phase 1)  │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              v            v            v
       ┌────────────┐ ┌─────────┐ ┌──────────────┐
       │ Draft Mode │ │F10 Skills│ │ F12 Biomes   │
       │  (Mover    │ │(Phase 2) │ │ (6 free,     │  ← parallel rail
       │  Control)  │ └────┬─────┘ │  then 2 new) │
       └────────────┘      │       └──────────────┘
                           │
                           v
                    ┌─────────────┐
                    │  F10 Mood   │
                    │  (Phase 3)  │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              v            v            v
       ┌────────────┐ ┌─────────┐ ┌──────────────┐
       │ F10 Mood UI│ │Skill Job│ │ Capability   │
       │ (Phase 4)  │ │ Assign  │ │ Phase A      │
       │            │ │(Phase 5)│ │ (data only)  │
       └────────────┘ └─────────┘ └──────┬───────┘
                                         │
                                         v
                                  ┌──────────────┐
                                  │ Capability   │
                                  │ Phase B      │
                                  │ (one recipe) │
                                  └──────┬───────┘
                                         │
                                         v
                                  ┌──────────────┐
                                  │ Capability   │
                                  │ Phase C      │
                                  │ (needs       │
                                  │  resolver)   │
                                  └──────────────┘

    INDEPENDENT (can run any time):
    ┌──────────────────────────────────────────────┐
    │ Onboarding Phase 1: starting supplies (~50L) │
    └──────────────────────────────────────────────┘
    ┌──────────────────────────────────────────────┐
    │ Onboarding Phases 2-3: scenarios + difficulty │
    │ (depends on F10 names + F12 biomes for full  │
    │  playtesting, but code is independent)        │
    └──────────────────────────────────────────────┘
```

**Key insight**: F10 (Personality & Skills) is the critical path. Every other feature either depends on it or can run in parallel. F12 (Biomes) is the parallel rail — zero dependencies on F10, can be done any time. Onboarding Phase 1 (starting supplies) is a ~50-line independent task that can slot in whenever.

---

## 4. Critical Path & Session Plan

~10 sessions (S17-S26), assuming sessions are similar scope to S1-S16.

> **Note**: The [Sims-Inspired Priorities](../sims-inspired-priorities.md) doc proposes reordering S18-S22 to prioritize mood and visual feedback over skills, based on the argument that mood is what turns survival meters into emotional attachment. The reordered sequence: S18 Mood → S19 Visual Feedback → S20 Comfort/Fun → S21 Room Quality → S22 Skills. Biomes and capabilities are unaffected.

### S17: Names + Draft Mode (DONE)

**F10 Phase 1** — Names & Identity:
- Name generation (syllable combiner or word list)
- Add `name`, `age`, `appearanceSeed` to Mover struct
- Display name in tooltip and selection UI
- ~50 new lines in mover, ~30 in rendering
- Tests: name uniqueness

**Mover Control** — Draft Mode:
- `isDrafted` bool on Mover, skip in AssignJobs when true
- Click-to-move when drafted (reuse pathfinding, ignore job system)
- ~20 lines in jobs.c, ~15 in input.c
- Draft mode needs names to be useful ("draft Kira" not "draft mover 3")

**Save bump**: one version for new Mover fields (name, age, appearanceSeed, isDrafted).

### S18-19: Skills + Starting Supplies

**F10 Phase 2** — Skill System:
- 8 skills (mining, woodcutting, construction, crafting, farming, hauling, cooking, hunting)
- Level 0-20 with XP curve
- Starting variance (2-3 aptitudes per mover)
- Speed multiplier: `0.8 + level * 0.06` (level 0 = 0.8x, level 20 = 2.0x)
- XP gain on job completion
- Stacks with tool quality: `toolMultiplier * skillMultiplier`
- ~150 lines in mover + jobs, ~40 tests

**Onboarding Phase 1** — Starting Supplies:
- Spawn starting items near mover in `InitSurvivalMode()`
- 5 berries, 1 clay pot (water), 5 dried grass
- `earlyGameDrainMultiplier` (0.5x ramping to 1.0x over 8 game-hours)
- ~50 lines, immediate playability improvement

### S20: Mood Foundation

**F10 Phase 3** — Mood System:
- `mood` float on Mover (-1.0 to 1.0)
- 8 mood inputs: hunger, energy, thirst, temperature, shelter, clothing, food variety, idle time
- Rolling average smoothing
- Speed multiplier: 0.7x (miserable) to 1.2x (happy)
- ~120 lines in mover/needs, ~30 tests

### S21: Mood UI + Skill Assignment + Capability Phase A

Three smaller tasks that fit one session:

**F10 Phase 4** — Mood UI:
- Color-coded mood indicator in mover tooltip
- Mood breakdown showing contributing factors
- Colony average mood in status bar
- ~60 lines in rendering

**F10 Phase 5** — Skill-Based Job Assignment:
- WorkGivers prefer movers with higher relevant skill
- Distance weighting (from Mover Control doc) as tiebreaker — nearby skilled mover beats distant expert
- Starvation guard: don't let distant high-skill assignments cause movers to starve en route
- ~40 lines in jobs.c WorkGiver changes

**Capability Phase A** — Data Declarations:
- `CapabilityEntry` struct (type + level), `CapabilityType` enum
- Add `capabilities[]` array to `WorkshopDef` and `FurnitureDef`
- Populate for all 17 workshops and all furniture
- No behavior changes — pure data tagging
- ~100 lines across workshops.c, furniture, new capability.h
- Validation: print capability tables at startup

### S22: 6 Free Biomes

**F12 Phase 1** — BiomePreset struct + 6 biomes:
- Temperate Grassland, Arid Scrubland, Boreal/Taiga, Wetland/Marsh, Highland/Rocky, Riverlands
- Wire into `GenerateHillsSoilsWater`: soil weights, tree species, climate, stone type, grass tinting, height variation, water/bush/scatter density
- Biome selection UI in terrain generation panel
- `selectedBiome` saved with world
- All 6 work with existing sprites and materials — no new assets
- Save migration: add `selectedBiome`, default 0
- ~300 lines in terrain.c, ~50 in rendering, ~30 in UI

### S23: Onboarding Scenarios + Dev Playtesting

**Onboarding Phases 2-3** — Scenarios + Difficulty:
- `StartScenario` enum + `ScenarioDef` struct
- 4 player scenarios: Settled Camp, Fresh Start, Naked in the Grass, River Valley
- 3 dev playtesting scenarios: The Settlers (F10 test), The Village (stockpile test), The Expedition (biome test)
- `ApplyScenario()` in `InitSurvivalMode()` — spawns movers/items/buildings, sets drain rates
- Progressive need unlock (thirst after 4h, energy after 8h, temperature after 16h)
- Scenario selection screen
- ~150 lines infrastructure, ~100 per scenario

**Dev playtesting**: use The Settlers scenario to validate F10 feel. Use The Expedition (boreal variant) to validate F12 feel.

### S24: Capability Phase B (One Recipe)

**Capability Phase B** — Resolver for One Recipe:
- Spatial resolver: `FindBestCapabilityProvider(type, minLevel, searchPos, radius)`
- Provider registry: spatial index of placed workshops + furniture (~50-100 entries mid-game)
- Pick one recipe (e.g., "cook meat") and make it work via `requiredCaps` instead of `workshopType`
- Keep `workshopType` fallback — old recipes unchanged
- Verify speed difference from capability level quality
- ~200 lines in new capability.c, recipe struct changes

### S25: Capability Phase C (Needs Resolver)

**Capability Phase C** — Needs Use Capability Resolution:
- Replace bespoke search functions in needs.c:
  - `FindNearestFurnitureForRest` → resolve `CAP_SLEEPING`
  - `FindNearestDrinkable` / `FindNearestNaturalWater` → resolve `CAP_WATER_SOURCE`
  - Warmth search → resolve `CAP_HEAT_SOURCE`
- Need resolution weights distance 2x more than job resolution (urgency over quality)
- Implicit level-0 providers: any walkable cell = `flat_surface:0`, `sleeping:0`; river = `water_source:1`
- Explicit-first search: only check implicit providers if no explicit found within radius
- All existing need tests must pass — behavior should be identical
- ~150 lines refactored in needs.c

### S26+: Beach/Desert Biomes, Capability Phase D, Polish

**F12 Phase 2-3** — Beach + Desert biomes:
- New assets: palm tree (MAT_PALM, 5 sprites), cactus, dead bush, seashell
- ITEM_CACTUS_FRUIT, ITEM_SEASHELL, PLANT_CACTUS
- Beach special worldgen: coastal gradient, driftwood scatter
- Desert special worldgen: oases, sandstone formations, cactus placement
- ~1-2 sessions

**Capability Phase D** — New Recipes:
- New crafting content written with `requiredCaps` — ongoing, per-recipe
- Migrate existing recipes one at a time as warranted

**Polish**:
- Mover Control distance weighting refinement
- Onboarding visual priority system (need bar pulsing, "what to do next" hints)
- Stockpile filter redesign (F11) — becomes more urgent with more item types

---

## 5. System Interconnections

The features aren't just sequenced — they feed each other. These are the key coupling points.

### Speed Stack

Four multipliers combine on every work action:

```
effectiveSpeed = skillMultiplier × capLevelMultiplier × toolQuality × moodMultiplier
```

- **Skill** (F10 Phase 2): 0.8x at level 0, up to 2.0x at level 20
- **Capability level** (Capability Phase B): 0.5x at level 0, up to 1.0x at level 3
- **Tool quality** (already implemented): 1.0x-1.6x based on tool level
- **Mood** (F10 Phase 3): 0.7x (miserable) to 1.2x (happy)

A master miner (skill 20) with a stone pick (tool 1.2x) in a good mood (1.1x) at a proper workshop (cap level 2, 0.83x): `2.0 × 0.83 × 1.2 × 1.1 = 2.19x`. A novice (skill 0) with bare hands (tool 1.0x) in a bad mood (0.85x) on the ground (cap level 0, 0.5x): `0.8 × 0.5 × 1.0 × 0.85 = 0.34x`. That's a 6.4x spread — enough to make progression feel meaningful.

### Mood Loop

```
Better furniture → Better need satisfaction → Better mood → Faster work → More output
                                                         → Better furniture (cycle)
```

This is the core positive feedback loop of Era 2. It only works when all three systems exist:
- **Furniture** provides capability levels (sleeping:2, seating:2)
- **Capability resolver** (Phase C) scores providers by level, affecting satisfaction amount
- **Mood system** (F10 Phase 3) converts satisfaction quality into speed multiplier

Without any one piece, the loop is broken. This is why Capability Phase C comes after Mood (S25 after S20) — the mood system needs to exist before the resolver can meaningfully improve it.

### Biomes x Scenarios

```
8 biomes × 4 scenarios = 32 distinct starting experiences
```

Biomes (F12) and scenarios (Onboarding) are orthogonal by design. "Naked in the Grass" in a desert is a completely different challenge than in temperate grassland. "Settled Camp" in boreal is about surviving winter, while in riverlands it's about managing flooding.

The dev playtesting scenarios (The Expedition) are specifically designed to stress-test biome variety. For this matrix to be meaningful, both F12 and the scenario infrastructure need to exist.

### Draft Mode + Names

Draft mode (Mover Control) is only useful when movers have names. "Draft mover 3" is meaningless; "Draft Kira" is a command you'd actually use. This is why draft mode ships in S17 alongside names — they're a natural pair.

### Provider Registry

The capability resolver (Phase B) needs a spatial index of placed workshops and furniture. This same registry serves:
- **Job system**: finding nearest capable workshop for a bill
- **Need system** (Phase C): finding nearest sleep/warmth/water provider
- **Future tooltip system**: showing "what can I do here?" when hovering a location

Building the provider registry once (~100 lines) avoids duplicating spatial search logic across three systems.

---

## 6. Shared Infrastructure

Four pieces of infrastructure serve multiple features. Building them early pays compound dividends.

### 1. Provider Registry (~100 lines)

**What**: Spatial index of all placed workshops + furniture, queryable by capability type and position.

**Serves**: Capability resolver (Phase B+C), need satisfaction search (Phase C), future tooltip/overlay system.

**When**: Build in S24 alongside Capability Phase B. Could also be built earlier in S21 with Phase A as a validation tool.

### 2. Mover Identity Display (~50 lines)

**What**: Rendering name + mood indicator above movers, name in tooltip, name in job log.

**Serves**: F10 names (S17), draft mode UX, mood UI (S21), skill assignment feedback, event log readability.

**When**: Build in S17 with F10 Phase 1.

### 3. Speed Multiplier Stack Refactor (~30 lines)

**What**: Centralize the `effectiveSpeed = skill × cap × tool × mood` calculation instead of each system applying its own multiplier at different points.

**Serves**: Skill system (S18), mood system (S20), capability quality (S24), future buffs/debuffs.

**When**: Build in S18 when adding skill multiplier. Currently tool quality is applied in `RunWorkProgress()` — extend that call site to accept the full stack.

### 4. Scenario Infrastructure (~150 lines)

**What**: `ScenarioDef` struct + `ApplyScenario()` function that spawns movers/items/buildings and sets balance values.

**Serves**: Player-facing scenarios (S23), dev playtesting scenarios (S23), future tutorial/campaign system.

**When**: Build in S23. The starting supplies from S18 (`InitSurvivalMode` changes) are a precursor — factor them into the scenario struct when building it.

---

## 7. Risks & Decision Points

### 1. Skill Level vs Capability Level — Keep Separate

Skill level (mover property, 0-20) and capability level (provider property, 0-3) are different axes. A master crafter (skill 20) working on the ground (cap 0) is fast but produces crude output. A novice (skill 0) at a proper workshop (cap 3) is slow but the output quality comes from the equipment. They multiply together — don't conflate them.

**Decision**: Already made. The speed stack formula keeps them as separate terms.

### 2. Capability Phase C Timing — After Mood

The needs resolver refactor (Phase C) should come after the mood system (F10 Phase 3), not before. Reason: Phase C changes how need satisfaction quality works (cap level affects satisfaction amount). If mood doesn't exist yet, there's no way to observe or test the quality difference. Building Phase C without mood would mean untestable quality code.

**Decision**: S25 for Phase C, after S20 for mood.

### 3. Biome x Scenario — Orthogonal Selection

Biomes and scenarios should be independently selectable (pick biome, then pick scenario). Don't lock scenarios to biomes ("Desert Expedition" as a single option). This gives 32 combinations from 8+4 definitions instead of needing 32 bespoke scenarios.

**Decision**: Two separate selection screens — biome picker in terrain generation, scenario picker before game start.

### 4. Save Version Pressure — Bundle F10

F10 adds significant new Mover fields (name, skills, mood, age, appearanceSeed). Each phase could bump the save version, but that means 3-5 migrations in quick succession. Better to bundle all F10 Mover struct changes into one save version bump at the end of S17 (names) with stub fields for skills and mood that get populated in later sessions.

**Decision**: One save migration for all F10 Mover fields in S17. Skills default to level 0, mood defaults to 0.0 (neutral). Later phases just start populating them.

### 5. Skill-Based Assignment + Starvation Risk

When WorkGivers prefer high-skill movers, they might send the colony's best miner across the map while closer movers stand idle. If the trip is long enough, the miner could starve en route.

**Mitigation**: Distance weighting (from Mover Control doc) as first tiebreaker. Skill preference only kicks in when two movers are roughly equidistant. Also: the existing need interrupt system already pulls movers off jobs when hunger is critical — but preventing the bad assignment is better than recovering from it.

**Decision**: Implement distance weighting in S21 alongside skill assignment. Formula: `score = skillLevel - (distance / MAX_WORK_DISTANCE) * penaltyWeight`.

### 6. Workshop Evolution Phase 2 = Capability Phase E

The workshop evolution doc's "Phase 2: Station Entities" and the capability doc's "Phase E: Freeform Station Furniture" describe the same thing — single-tile capability providers that cluster spatially. These should be one implementation, not two.

**Decision**: When the time comes (Era 3), merge the two docs into a single station entity design. The capability doc's resolver (built in S24-25) is the foundation. The workshop doc's station concept is the entity model. Neither is needed before S26+.

---

## 8. Explicit Scope Exclusions

This roadmap covers the path to Era 2 ("The Village"). The following are explicitly **not** in scope:

- **Era 3+ content** (steam power, industrial scale, 30+ colonists) — see [Workshop Evolution](../workshop-evolution-plan.md) Phase 2-4
- **Freeform station entities** (Capability Phase E) — foundation laid, not built until Era 3
- **Infinite world / chunk streaming** — current fixed grid is sufficient through Era 2
- **Social system** (relationships, conversations, conflicts) — F10 Phase 2 mentions personality traits but social interactions are Era 3+
- **Health / injury / illness** — no HP system, no healing, no disease. Movers can die from starvation/dehydration/hypothermia but not from combat wounds
- **Stockpile filter redesign (F11)** — the current flat filter works until item type count exceeds ~80-100. Revisit after F10+F12 add their items
- **Raids / hostile NPCs** — predators exist but no intelligent enemies. Era 3+ territory
- **Trade / caravans / recruitment** — no external contact system. Era 3+
- **Stairs** (replacing ladders) — cosmetic improvement, not blocking any feature
- **Multi-crafter workspaces** — Workshop Evolution Phase 4, only needed at 30+ colonist scale
