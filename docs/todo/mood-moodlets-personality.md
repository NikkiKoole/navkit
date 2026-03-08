# Mood, Moodlets & Personality

> Status: partial

> Foundation shipped: mood.h/mood.c, moodlet array on Mover, 5 traits, squared-curve continuous mood, speed multiplier, 31 tests passing. This doc tracks remaining moodlet triggers and integration work.

---

## What exists (save version TBD)

- `float mood` on Mover (-1.0 to 1.0), smoothed rolling average
- `Moodlet moodlets[8]` — type, value, remainingTime
- `uint8_t traits[2]` — TraitType personality traits
- Continuous inputs: hunger, thirst, energy, bodyTemp feed mood via squared curve
- 5 traits: Hardy, Picky Eater, Outdoorsy, Glutton, Stoic
- Speed multiplier: 0.7x (miserable) to 1.2x (happy)
- `MoodTick()` called per frame, `AddMoodlet()` at event transition points
- Trait multiplier table modifies moodlet intensity per-mover
- Tests: test_mood.c (31 tests, 50 assertions)
- `moodEnabled` global toggle (default false, not yet wired into game)

## TODO: Wire into game

### Phase 1: Enable and integrate ✅

- [x] Wire `MoodTick(dt)` into main game loop + headless tick (after ProcessFreetimeNeeds)
- [x] Wire `GetMoodSpeedMult()` into `RunWorkProgress` (all jobs get mood-affected speed)
- [x] Call `AssignRandomTraits()` in mover spawn (SpawnMover + SpawnMoversDemo)
- [x] `moodEnabled = true` by default (was false)
- [x] Save version 89→90, MoverV89 migration, fixup loop for all old saves
- [x] Mood tooltip: mood bar (colored), active moodlets with values/time, personality traits

### Phase 2: Hook existing moodlet triggers into needs.c ✅

- [x] **Eating completion**: good food (cooked meat/bread/lentils/roasted root) → ATE_GOOD_FOOD, raw food → ATE_RAW_FOOD
- [x] **Sleep completion**: ground → SLEPT_ON_GROUND, leaf/grass pile → SLEPT_POORLY, plank bed → SLEPT_WELL
- [x] **Warmth completion**: warmed up at fire → WARM_AND_COZY
- [x] **Drinking completion**: tea/juice → DRANK_GOOD, natural water → DRANK_DIRTY_WATER
- [x] **Cold** (continuous): already in UpdateContinuousMoodlets in mood.c

### Phase 3: Scenery appreciation (new triggers, existing data) ✅

Movers passively "notice" nearby natural beauty — trees, water, plants. Small positive boost from pleasant surroundings, mild negative from barren/ugly ones.

- [x] **Pleasant view** (+0.5, 2h) — periodic scan (every 0.5 game-hours) within radius 6
  - Trees: CELL_TREE_TRUNK/BRANCH/LEAVES/SAPLING at z±1
  - Water: `GetWaterLevel(x,y,z) > 0` at z±1
  - Plants: `GetPlantAt(x,y,z)` at walking level
  - Threshold: 3+ beauty sources to trigger
- [x] **Bleak surroundings** (-0.5, 2h) — 2+ game-hours with zero beauty sources nearby
- [x] Trait: `TRAIT_OUTDOORSY` 1.5x on both pleasant view and bleak surroundings
- [x] `sceneryEnabled` toggle, `InitSceneryState()`, `CountBeautySources()` API
- [x] 13 new tests (6 beauty count + 7 moodlet behavior)
- [ ] Future: flowers, decorated items, room decorations could count as beauty sources

### Phase 4: Weather moodlets (new triggers, existing data)

All data exists in weather.c. Triggers go in `UpdateContinuousMoodlets()` or a new periodic check.

- [ ] **Rained on** (-1, while active) — `IsExposedToSky(cx,cy,cz) && (weatherType == RAIN || HEAVY_RAIN)`
- [ ] **Caught in storm** (-2, 2h) — `IsExposedToSky && weatherType == THUNDERSTORM`
- [ ] **Enjoying the sun** (+1, 2h) — `IsExposedToSky && weatherType == CLEAR && daytime`
- [ ] **Snow** (+1 or -1, trait-dependent) — exposed during WEATHER_SNOW

### Phase 5: Light moodlets (new triggers, existing data)

`GetLightColor()` exists per cell. Check at job completion or periodically.

- [ ] **Worked in the dark** (-1, 4h) — completed a job in a cell with very low light level
- [ ] **Well-lit workspace** (+0.5, 4h) — completed a job in a well-lit cell

### Phase 6: Environment moodlets (new triggers, existing data)

- [ ] **Filthy surroundings** (-1, 2h) — mover on cell with high `floorDirtGrid` value
- [ ] **Ate stale food** (-1, 6h) — `item.condition == CONDITION_STALE` at eating completion
- [ ] **Varied diet** (+1, 12h) — ate 3+ different food types recently (needs small tracking array)
- [ ] **Monotonous diet** (-1, 12h) — ate same food type 3 times in a row

### Phase 7: Activity moodlets (new triggers, existing data)

- [ ] **Overworked** (-1, 4h) — mover worked N game-hours without rest break
- [ ] **Idle too long** (-1, 4h) — mover had no job for extended period
- [ ] **Company** (+0.5, 2h) — other movers within N cells while resting/eating (spatial grid query)
- [ ] **Alone** (-0.5, 2h) — no other mover within N cells for extended time

### Phase 8: Room quality moodlets (needs room detection system)

Depends on `village-next-steps.md` Step 1 (room detection + quality scoring).

- [ ] **Nice room** (+1, 4h) — spent time in room with quality > threshold
- [ ] **Ugly room** (-1, 4h) — spent time in low-quality room
- [ ] **Ate at table** (+2, 6h) — ate in room with table+chair (needs FURNITURE_TABLE)
- [ ] **Ate without table** (-3, 6h) — ate standing/outdoors/no table

### Phase 9: More personality traits

- [ ] **Night owl** — less penalty from working at night, more from early morning
- [ ] **Early bird** — opposite of night owl
- [ ] **Social** — amplified Company/Alone moodlets
- [ ] **Loner** — reversed Company/Alone (likes being alone, dislikes crowds)
- [ ] **Neat** — amplified Filthy Surroundings penalty, bonus from clean rooms
- [ ] **Slob** — ignores dirt entirely

---

## Design notes

### Moodlet trigger pattern
Every moodlet fires at an existing state transition (eating done, sleeping done, job done) or as a continuous check in `UpdateContinuousMoodlets()`. No event bus needed — just direct `AddMoodlet()` calls.

### Trait-moodlet interaction
Traits are a multiplier table: `traitDefs[trait].moodletMult[moodletType]`. 0.0 = use default 1.0. This means adding a new moodlet or trait is just adding a row/column to the table.

### What makes this system work for character
Two movers in identical circumstances feel differently about it. The player reads the moodlet list and trait names and understands *why*. "Kira doesn't mind the rain but Thrak ran inside" — that's personality expressed through mechanics.

### Squared curve (continuous mood)
One catastrophically bad need dominates mood. Slightly hungry barely hurts, but starving is devastating. This matches player intuition and creates urgency.

### Dependencies
- Phase 1-2: no deps, can ship now
- Phase 3: no deps (trees/water/plants data exists)
- Phase 4-5: no deps (weather/light data exists)
- Phase 6-7: no deps (dirt/food/spatial data exists), varied diet needs small tracking
- Phase 8: depends on room detection (village-next-steps step 1)
- Phase 9: no deps (just trait table entries)

---

## Files

- `src/simulation/mood.h` — types, enums, API
- `src/simulation/mood.c` — implementation, tables, tick
- `src/entities/mover.h` — mood/moodlet/trait fields on Mover struct
- `tests/test_mood.c` — 31 tests
