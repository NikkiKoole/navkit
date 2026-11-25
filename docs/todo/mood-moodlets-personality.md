# Mood, Moodlets & Personality

> Status: partial

> Foundation shipped and on in every game (`moodEnabled` true since save v90): mood.h/mood.c, moodlet array on Mover, 9 rolled traits (+ Scavenger, set by a game or scenario), squared-curve continuous mood, speed multiplier, 31 tests passing. This doc tracks remaining moodlet triggers and integration work.

---

## What exists (mood saved since v90)

- `float mood` on Mover (-1.0 to 1.0), smoothed rolling average
- `Moodlet moodlets[8]` — type, value, remainingTime
- `uint8_t traits[2]` — TraitType personality traits
- Continuous inputs: hunger, thirst, energy, bodyTemp feed mood via squared curve
- Traits (`src/simulation/mood.h`): Hardy, Picky Eater, Outdoorsy, Glutton, Stoic; added later by other work: Patient / Impatient (how long they wait in lines), Tidy / Slob (trash: walk to a bin or drop it), Scavenger (not rolled; City 1999)
- Speed multiplier: 0.7x (miserable) to 1.2x (happy)
- `MoodTick()` called per frame, `AddMoodlet()` at event transition points
- Trait multiplier table modifies moodlet intensity per-mover
- Tests: test_mood.c (31 tests, 50 assertions)
- `moodEnabled` global toggle (on by default; every game's start sets it)

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
- [x] **Eating location**: room with a dining set (table + chair next to it; was: any chair) → ATE_AT_TABLE (+2, 6h), otherwise → ATE_WITHOUT_TABLE (-3, 6h)
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

### Phase 3b: Room quality moodlets ✅

Room detection system (`rooms.h`/`rooms.c`) with flood-fill per z-level, room typing, quality scoring. Feeds into mood via periodic check.

- [x] Room detection: BFS flood-fill bounded by walls/doors/windows, per-cell roomGrid
- [x] Room typing: Kitchen > Workshop > Barracks > Bedroom > Dining > Storage > Generic
- [x] Quality scoring: light, furniture count/variety, constructed floors/walls, windows, dirt, size
- [x] **Nice room** (+1, 4h) — quality > 0.3 threshold, checked every 0.5 game-hours
- [x] **Ugly room** (-1, 4h) — quality < -0.3 threshold
- [x] Room info in cell tooltip ("Room: Bedroom (q: +0.45)")
- [x] InvalidateRooms() at all mutation points (construction, mining, furniture, workshops, stockpiles)
- [x] 25 black-box tests (test_rooms.c)
- [ ] Future: recheck on door transition (track lastRoomId per mover)
- [ ] Future: bedroom privacy bonus (private > barracks > outdoors) on sleep completion

### Phase 4: Weather moodlets (new triggers, existing data)

All data exists in weather.c. Triggers go in `UpdateContinuousMoodlets()` or a new periodic check.

- [x] **Rained on** (-1, 0.5h, refreshed while it rains) — `IsExposedToSky(cx,cy,cz) && (weatherType == RAIN || HEAVY_RAIN)` (2026-11-22)
- [x] **Caught in storm** (-2, 2h) — `IsExposedToSky && weatherType == THUNDERSTORM` (replaces rained on)
- [x] **Enjoying the sun** (+1, 2h) — `IsExposedToSky && weatherType == CLEAR && 7:00-19:00`; rain removes it
- [ ] **Snow** (+1 or -1, trait-dependent) — exposed during WEATHER_SNOW (not yet)

Done as one periodic check per mover (every ~0.25 game-hours, spread by mover index) in
`UpdateWeatherAndDirtMoodlets`. Traits: Outdoorsy rain x0.3 / storm x0.5 / sun x1.5, Hardy
rain x0.5. Note: the weather simulation is off unless turned on (`set weather 1` or dev UI
"Weather Enabled"; `InitWeather()` is never called), so these only show with weather on.

### Phase 5: Light moodlets (new triggers, existing data)

Wait for schedules (sleep/wake): without them movers work at night and "worked in the dark"
would fire for everyone every night. Or limit it to indoors.

`GetLightColor()` exists per cell. Check at job completion or periodically.

- [ ] **Worked in the dark** (-1, 4h) — completed a job in a cell with very low light level
- [ ] **Well-lit workspace** (+0.5, 4h) — completed a job in a well-lit cell

### Phase 6: Environment moodlets (new triggers, existing data)

- [x] **Filthy surroundings** (-1, 2h) — floor dirt under the mover >= 80 (cleaners start at 30); Hardy x0.5
- [x] **Ate stale food** (-1, 6h) — `item.condition >= CONDITION_STALE` at eating completion; Picky Eater x1.5
- [ ] **Varied diet** (+1, 12h) — ate 3+ different food types recently (needs small tracking array)
- [ ] **Monotonous diet** (-1, 12h) — ate same food type 3 times in a row

### Phase 7: Activity moodlets (new triggers, existing data)

- [ ] **Overworked** (-1, 4h) — mover worked N game-hours without rest break
- [ ] **Idle too long** (-1, 4h) — mover had no job for extended period
- [ ] **Company** (+0.5, 2h) — other movers within N cells while resting/eating (spatial grid query)
- [ ] **Alone** (-0.5, 2h) — no other mover within N cells for extended time

### Phase 8: Room quality moodlets — superseded by Phase 3b (Nice/Ugly room built there)

Depends on `village-next-steps.md` Step 1 (room detection + quality scoring).

- [x] **Nice room** (+1, 4h) — built in Phase 3b
- [x] **Ugly room** (-1, 4h) — built in Phase 3b
- [x] **Ate at table** (+2, 6h) — ate in a room with a dining set: FURNITURE_TABLE with a chair on a side cell (2026-11-22). Room-based because movers eat where the food lies; with sit-then-eat (dollhouse Phase 3c) it can become "sat at that table"
- [x] **Ate without table** (-3, 6h) — ate standing/outdoors/no table

### Phase 9: More personality traits

- [ ] **Night owl** — less penalty from working at night, more from early morning
- [ ] **Early bird** — opposite of night owl
- [ ] **Social** — amplified Company/Alone moodlets
- [ ] **Loner** — reversed Company/Alone (likes being alone, dislikes crowds)
- [ ] **Neat** — amplified Filthy Surroundings penalty, bonus from clean rooms
- [ ] **Slob** — ignores dirt entirely. Name taken: the code's `TRAIT_SLOB` is about trash (drops it where they stand); a dirt trait needs another name or should extend that one

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
