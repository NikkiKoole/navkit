# 09 — Onboarding & Progressive Complexity

*The game is getting overwhelming to start. Every need (hunger, thirst, energy, temperature, clothing) hits at once. This doc plans how to ease players into survival systems gradually.*

---

## Research: What Colony Games Do

### RimWorld
- **Storyteller difficulty**: Randy/Cassandra/Phoebe control event frequency and severity
- **Starting scenario**: Crashlanded (3 colonists, supplies, steel, meals) vs Naked Brutality (1 colonist, nothing)
- **Grace period**: No raids for ~5 in-game days. First events are mild (self-taming animal, cargo pod)
- **Needs unlock gradually**: Hunger is immediate, mood/comfort matter later, ideology needs are late-game
- **Tutorial**: "Learning helper" popups explain systems when first encountered (can disable)

### Oxygen Not Included
- **Starting resources**: 3 dupes + buried food/water near spawn — buys ~10 cycles
- **Maslow's hierarchy pacing**: Oxygen → Food → Stress → Plumbing → Power → Heat/Cold → Radiation (DLC)
- **Research tree gates complexity**: Can't build advanced systems until researched
- **Difficulty**: Survival vs No Sweat (relaxed timers, more resources)

### Going Medieval
- **Starting loadout scales with difficulty**: More settlers + food + weapons on easier settings
- **Needs ramp up**: Food is immediate, shelter matters in rain/winter, mood comes later
- **Season 1 is always temperate**: No winter threat for first in-game season

### Banished
- **No combat at all**: Purely logistics/survival against nature
- **Starting conditions**: Choose easy/medium/hard (affects starting people, seeds, tools, terrain)
- **First-year buffer**: Start in spring with stockpiled food, winter is the first real test
- **Single resource chains**: Food (gather → farm → market), warmth (firewood → coats), health (herbs → hospital)

### Timberborn
- **Pre-placed buildings**: Tutorial maps start with basic infrastructure already built
- **Drought cycle**: First drought is short and gentle, gets longer each cycle
- **Visual tutoring**: Needs bars are prominently displayed, color-coded, animated when critical

### Songs of Syx
- **Massive starting population** (100+ settlers) smooths individual failures
- **Room-based needs**: Morale from room quality/variety is the core challenge, not raw survival
- **Gradual unlocks**: Buildings unlock through population milestones

### Against the Storm
- **Run-based**: Each settlement is ~30min, fail-and-retry is cheap
- **Blueprint drafting**: Choose 2 of 3 random building options — limits complexity per run
- **Reputation timer**: External pressure increases gradually, not all at once

### Stonehearth
- **Campaign mode**: Guided objectives introduce one system at a time
- **Footman → Hearthling pipeline**: Combat unlocks only after food/shelter established
- **Cheerful tone**: Low-stakes framing reduces anxiety about optimization

### Settlement Survival
- **Advisor quests**: Step-by-step objectives ("Build 2 houses", "Assign 3 gatherers")
- **First year is forgiving**: Abundant wild food, mild weather, no disasters
- **Difficulty slider**: Affects resource abundance, need drain speed, event frequency

### Common Patterns (ranked by frequency)

1. **Starting supplies buy time** (9/10 games) — food/tools/shelter for day 1
2. **First season/period is forgiving** (8/10) — mild weather, no threats, generous timers
3. **Difficulty selection** (7/10) — affects timers, resources, event frequency
4. **Needs unlock progressively** (6/10) — not all survival systems active from minute one
5. **Visual priority cues** (6/10) — critical needs are obvious, less urgent ones are subtle
6. **Tutorial/advisor system** (5/10) — objectives or popups introducing systems one at a time
7. **Research/tech gates complexity** (4/10) — can't encounter advanced problems without advanced buildings
8. **Grace period for threats** (4/10) — no attacks/disasters for initial period

---

## Current NavKit State

In survival mode, a mover spawns and immediately faces:
- **Hunger** draining (24h to starvation, 12h death timer)
- **Thirst** draining (16h to dehydration, 8h death timer)
- **Energy** draining (sleep cycle)
- **Temperature** effects (clothing system)
- **No starting supplies** — everything must be gathered/crafted from scratch

All systems hit simultaneously. The player must figure out food (forage → stockpile → cook), water (find pot clay → build kiln → fire pot → fill at river), shelter (walls → roof → floor → door), and clothing (harvest → process → craft) all at the same time.

---

## Proposal: Progressive Needs Unlock

### Core Idea

Don't activate all survival systems at once. Stagger them so each need becomes relevant only after the player has had time to address the previous one.

### Implementation: Unlock Tiers

Each need has an **unlock condition** — it starts disabled and activates when the condition is met. Once unlocked, it stays on permanently (saved with game state).

| Tier | Need | Unlock Trigger | Rationale |
|------|------|---------------|-----------|
| 0 | Hunger | Immediate | Food is intuitive — forage berries, eat |
| 1 | Thirst | First food eaten OR 2 game-hours | Water is next priority after food |
| 2 | Energy | First shelter built (4+ walls + roof) | Sleep matters once you have somewhere to sleep |
| 3 | Temperature | First clothing crafted OR first winter | Cold/heat matter once you can do something about it |

**Alternative: Time-based unlock** (simpler to implement)

| Tier | Need | Unlock After | Grace Period Purpose |
|------|------|-------------|---------------------|
| 0 | Hunger | Immediate | — |
| 1 | Thirst | 4 game-hours | Time to find water, make first pot |
| 2 | Energy | 8 game-hours | Time to build basic shelter |
| 3 | Temperature | 16 game-hours | Time to set up clothing workshop |

When a need unlocks: brief notification ("Your settlers are getting thirsty — they'll need water soon"), need bar fades in (not instant), timer starts from full.

### Starting Supplies

Spawn the player's initial movers with:
- 5x berries (immediate food, ~2 meals)
- 1x clay pot with water (immediate drink source)
- 5x dried grass (crafting material head start)

This buys ~4 game-hours of breathing room. Player sees the items, understands the pattern (berries = food, water in pot = drink), and can focus on exploration before scrambling.

**Implementation**: In `InitSurvivalMode()`, call `SpawnItem()` for starting supplies near the mover spawn point.

### Gentler Early Timers

First-day drain rates are slower, then ramp to normal:

```
effectiveDrain = baseDrain * lerp(gentleFactor, 1.0, gameTime / rampDuration)
```

- `gentleFactor = 0.5` (half drain speed at start)
- `rampDuration = 8 game-hours` (fully normal by mid-day-1)

This is invisible — no UI needed. Player just has more time on day 1.

### Difficulty Presets

Add to game start / settings:

| Preset | Drain Multiplier | Starting Supplies | Grace Period | Death |
|--------|-----------------|-------------------|-------------|-------|
| Relaxed | 0.5x | Generous (10 food, 2 pots, tools) | All needs delayed 8h | Incapacitated, not killed |
| Standard | 1.0x | Modest (5 food, 1 pot) | Thirst delayed 4h | Death after timer |
| Harsh | 1.5x | None | No grace | Faster death timer |

**Implementation**: A single `difficultyPreset` enum that sets multiple balance values at once. Individual values still tweakable in dev UI.

---

## Starting Scenarios

Instead of (or in addition to) difficulty sliders, let the player pick a **scenario** that defines the entire starting situation. This is how most colony games handle it — the scenario *is* the difficulty setting, but it also tells a story.

### "Settled Camp" (Easy)

*A small group arrives at a prepared campsite with supplies left by a previous expedition.*

| Aspect | Details |
|--------|---------|
| Movers | 4 |
| Pre-built | Campfire, kiln, 2 stockpiles (food + materials) |
| Starting items | 15 berries, 2 clay pots (filled with water), 10 dried grass, 5 cordage, 3 logs, 1 stone axe, 1 digging stick |
| Spawn rules | Near river (within 10 tiles), flat terrain, trees nearby |
| Needs | Hunger only at start; thirst after 6h, energy after 12h, temperature after first clothing |
| Drain rates | 0.7x base |

The pre-built campfire means the player can immediately queue cooking. The kiln means they can make more pots. Two stockpiles teach the storage concept. Generous supplies mean no one dies while the player learns the UI.

### "Fresh Start" (Standard)

*Two settlers strike out on their own with basic provisions.*

| Aspect | Details |
|--------|---------|
| Movers | 2 |
| Pre-built | None |
| Starting items | 5 berries, 1 clay pot (filled with water), 5 dried grass |
| Spawn rules | Within 15 tiles of river, some trees nearby |
| Needs | Hunger immediate; thirst after 4h, energy after 8h, temperature after 16h |
| Drain rates | 1.0x base |

The player must build everything but has enough food/water for the first few hours. Two movers means one can gather while the other builds — but losing one is a serious blow.

### "Naked in the Grass" (Hard)

*One person wakes up alone with nothing. Good luck.*

| Aspect | Details |
|--------|---------|
| Movers | 1 |
| Pre-built | None |
| Starting items | None |
| Spawn rules | Random (may or may not be near water) |
| Needs | All active immediately |
| Drain rates | 1.0x base |

This is the "Naked Brutality" experience. Every second counts. The player must immediately forage, find water, and start building before nightfall. Losing your one mover is game over.

### "River Valley" (Medium, Sandbox-ish)

*A band of settlers finds a fertile valley. Resources are plentiful but winter is coming.*

| Aspect | Details |
|--------|---------|
| Movers | 6 |
| Pre-built | None |
| Starting items | 20 berries, 3 clay pots (filled), 10 logs, 10 rocks, 2 stone axes, 1 stone pick |
| Spawn rules | River valley terrain (river through center, forest on both sides) |
| Needs | Hunger + thirst immediate; energy after 8h, temperature at first winter |
| Drain rates | 0.8x base |

More movers + more supplies, but temperature kicks in hard at winter. This scenario is about building up infrastructure before the season turns. Good for players who like logistics over survival pressure.

### Implementation

```c
typedef enum {
    SCENARIO_SETTLED_CAMP,
    SCENARIO_FRESH_START,
    SCENARIO_NAKED_IN_THE_GRASS,
    SCENARIO_RIVER_VALLEY,
    SCENARIO_COUNT
} StartScenario;

typedef struct {
    const char *name;
    const char *description;
    int moverCount;
    float drainMultiplier;
    bool allNeedsImmediate;
    float thirstUnlockGH;    // 0 = immediate
    float energyUnlockGH;
    float tempUnlockGH;      // -1 = unlock at first winter
    // Starting items as array of {type, count, material} tuples
    // Pre-built structures as array of {workshopType, offsetX, offsetY}
    // Spawn rules as terrain constraints
} ScenarioDef;
```

- Scenario selection screen before game start (or in settings for sandbox)
- `ApplyScenario()` called from `InitSurvivalMode()` — sets balance values, spawns movers/items/buildings, applies terrain constraints to spawn point selection
- Scenario choice saved with game state (for display in load screen)
- Sandbox mode ignores scenarios (or has a "Custom" option with all sliders)

### Scenario vs Difficulty

These replace the Relaxed/Standard/Harsh presets from the earlier section. A scenario is more expressive than a difficulty slider because it changes the *situation*, not just the numbers. "Settled Camp" isn't just "easy mode" — it's a different starting story.

If we also want a separate difficulty modifier on top of scenarios (e.g., "Fresh Start but with 1.5x drain"), that's possible but probably unnecessary for now. Keep it simple: pick a scenario, play.

---

## Visual Priority System

### Need Bar Improvements

Currently all need bars look the same. Proposal:

- **Critical need** (< 15%): bar pulses red, mover portrait has warning icon
- **Urgent need** (< 40%): bar turns orange, slightly larger
- **Fine** (> 40%): bar is normal color, no special treatment
- **Locked need**: bar is hidden entirely (not grayed out — invisible)

When a need unlocks, the bar slides in with a subtle animation + one-time notification text.

### "What To Do Next" Hint

A small, dismissible hint in the corner during early game:

```
Day 1: "Forage berries (B) to feed your settlers"
Day 1 (after eating): "Find water — build a clay pot at the kiln"
Day 2: "Build shelter before nightfall"
etc.
```

These are triggered by game state (no food stockpiled → show food hint, no water → show water hint). Can be disabled in settings. Not a full tutorial — just gentle nudges.

---

## Scope & Phasing

### Phase 1: Starting Supplies + Gentle Timers (small, high impact)
- Spawn starting items in survival mode
- Add `earlyGameDrainMultiplier` to balance table
- ~50 lines of code, immediate improvement

### Phase 2: Progressive Need Unlock (medium)
- `needUnlockTime[]` array or condition checks
- Need bar show/hide logic in rendering
- Unlock notification system
- Save/load the unlock state
- ~200 lines

### Phase 3: Difficulty Presets (medium)
- Enum + preset application function
- UI for selection (game start screen or settings)
- ~150 lines

### Phase 4: Visual Priority + Hints (polish)
- Bar pulsing/color changes
- Hint text system with state-based triggers
- ~200 lines

---

## Playtesting Scenarios (Feature Stress-Tests)

*The existing scenarios above are player-facing difficulty options. These are developer playtesting scenarios designed to expose gaps in upcoming features (F10 personality/skills, F11 stockpile filters, F12 biomes). Each scenario puts pressure on a different system.*

### "The Settlers" — Colony Management (tests F10: Personality & Skills)

*A small group arrives with supplies. The survival scramble is over — now delegate.*

| Aspect | Details |
|--------|---------|
| Movers | 4-5 |
| Pre-built | Campfire, drying rack, rope maker |
| Starting items | 10 berries, 2 water pots, 5 sticks, 3 cordage, 1 stone axe, 1 digging stick |
| Spawn rules | Near river, flat terrain, trees nearby |
| Needs | All active (survival basics are covered by supplies) |
| Drain rates | 1.0x base |

**Why this scenario**: With 4-5 movers, you immediately feel the absence of F10. All movers are identical — you can't tell them apart, can't specialize them, can't see who's good at what. You want to say "you mine, you build, you cook" but there's no skill system to make that meaningful. It also tests job prioritization with multiple workers competing for the same tasks.

**What it exposes**:
- No names on movers (who is who?)
- No skill differentiation (everyone is equally good at everything)
- No mood system (movers don't care about conditions)
- Job contention (3 movers trying to haul the same item)
- No scheduling (everyone works/sleeps at random times)

### "The Village" — Mid-Game Systems (tests F11: Stockpile Filters + polish)

*An established colony with infrastructure. Focus is on logistics, not survival.*

| Aspect | Details |
|--------|---------|
| Movers | 6-8 |
| Pre-built | Full shelter (walls, door, roof), fire pit, kiln, stonecutter, 2 sawmills, 3 stockpiles (food, materials, tools), farm plot (tilled, 2x2) |
| Starting items | 20 berries, 4 water pots, 10 logs, 10 rocks, 20 sticks, 10 cordage, 5 dried grass, 2 stone axes, 1 stone pick, 1 stone hammer, seeds (wheat, lentils) |
| Spawn rules | River valley, trees on both sides |
| Needs | All active |
| Drain rates | 0.8x base |

**Why this scenario**: Skips the bootstrap entirely. With 6-8 movers, many item types, and multiple workshops, this puts maximum pressure on the stockpile system. Are items ending up where they should? Is hauling efficient? Do movers eat/drink/sleep without micromanagement? The sheer item variety (food, tools, materials, clothing, containers) will make stockpile filter pain obvious.

**What it exposes**:
- Stockpile filter limitations (too many item types for current flat filter)
- Hauling efficiency with many movers + many items
- Whether workshops stay supplied when demand is high
- Whether movers self-manage needs when work is available
- Missing furniture/room quality (all rooms feel the same)

### "The Expedition" — Biome Pressure (tests F12: Biomes)

*A small party in a harsh biome. Different land, different strategy.*

| Aspect | Details |
|--------|---------|
| Movers | 3 |
| Pre-built | None |
| Starting items | 5 berries, 1 water pot, 1 stone axe |
| Spawn rules | Biome-dependent (see variants below) |
| Needs | All active immediately |
| Drain rates | 1.0x base |

**Biome variants**:

| Variant | Biome | Key challenge | Tests |
|---------|-------|---------------|-------|
| Boreal Expedition | Boreal / Taiga | Cold + short growing season | Temperature system pressure, peat as fuel, clothing urgency |
| Desert Expedition | Desert | No water, extreme heat | Oasis-seeking gameplay, water scarcity, temperature extremes |
| Beach Expedition | Beach / Coastal | No wood (palm only), sand farmland | Resource scarcity in new biome, palm tree as sole wood source |
| Highland Expedition | Highland / Rocky | Steep terrain, thin soil | Building site scarcity, stone-heavy construction, vertical navigation |

**Why this scenario**: Tests whether different biomes actually create different gameplay strategies, not just different colors. If you play the same way in boreal as temperate, the biome system isn't working. Each variant should force a fundamentally different opening: in desert you rush for the oasis, in boreal you rush for fire + clothing, on the beach you scavenge driftwood.

**What it exposes**:
- Whether biome temperature/climate creates real survival pressure
- Whether soil/tree distribution forces different material strategies
- Whether grass tinting + stone type actually make biomes feel distinct
- Water scarcity in arid biomes (is the thirst system meaningful?)
- Whether palm/cactus feel like real alternatives to oak/pine

### Implementation Notes

These are **dev-only scenarios** — not shipped to players, just used for internal testing. Implementation is simple:

```c
typedef enum {
    // Player-facing scenarios (from above)
    SCENARIO_SETTLED_CAMP,
    SCENARIO_FRESH_START,
    SCENARIO_NAKED_IN_THE_GRASS,
    SCENARIO_RIVER_VALLEY,
    // Dev testing scenarios
    SCENARIO_TEST_SETTLERS,     // 5 movers, basic supplies
    SCENARIO_TEST_VILLAGE,      // 8 movers, full camp
    SCENARIO_TEST_EXPEDITION,   // 3 movers, harsh biome
    SCENARIO_COUNT
} StartScenario;
```

Dev scenarios show in sandbox mode only (or behind a debug flag). The key value is having reproducible test setups that don't require 20 minutes of bootstrapping before you reach the system you actually want to test.

### Priority for Playtesting

1. **"The Settlers" first** — simplest to implement (just spawn 5 movers + items), immediately validates whether F10 is needed next
2. **"The Expedition" second** — needs biome presets (F12) to be meaningful, but even with just temperate-vs-boreal it's useful
3. **"The Village" third** — most setup work (pre-built structures), but most valuable for stockpile/logistics testing

---

## Open Questions

1. **Condition-based vs time-based unlocks?** Condition-based is more elegant (unlock thirst when you eat) but harder to implement and test. Time-based is dead simple.
2. **Should sandbox mode also use progressive unlocks?** Probably not — sandbox is for testing, all systems should be available immediately.
3. **Incapacitation instead of death on Relaxed?** RimWorld does "downed" state. Could be a nice safety net but adds complexity (rescue job, recovery timer).
4. **How to handle mid-game saves?** If loading a save where gameTime > 8h, all needs should be unlocked regardless of conditions.
5. **First-winter trigger for temperature**: Do we have season tracking robust enough to detect "first winter"? (Yes — `currentSeason` + a `firstWinterReached` bool would work.)
