# Test Suite Analysis Report

**Generated:** 2026-02-13
**Purpose:** Identify pointless tests, well-designed tests, and extractable common patterns

## Analysis Criteria

### Pointless Tests (Red Flags)
- Tests that assert things that are always true by definition
- Tests that just mirror the implementation without testing behavior
- Tests with no assertions or trivial assertions
- Tests that test getters/setters with no logic

### Well-Designed Tests (Good Examples)
- Tests that describe player-facing behavior
- Tests that would catch real bugs if the behavior changed
- Tests that set up realistic scenarios and verify outcomes
- Tests with clear arrange-act-assert structure

### Common Patterns to Extract
- Repetitive setup code
- Common assertion patterns
- Helper functions that appear in multiple files
- Grid initialization patterns

---

## Analysis Results

### Executive Summary

**Total Test Files Analyzed:** 27
**Total Tests:** ~2000+ across all files (1200+ in soundsystem alone)

**Key Findings:**
- **30-40% of tests are pointless** - testing trivial initialization, getters/setters, or mirroring implementation
- **60-70% are well-designed** - testing player-facing behavior, integration, edge cases
- **Major opportunity:** Extract common setup patterns (grid init, clear functions) into shared helpers
- **Biggest issues:** Duplicate tests, initialization-only tests, tests with no real assertions

---

## 1. Core Systems

### test_pathfinding.c

**Pointless Tests:**
- `"should mark chunks as dirty when walls are placed"` - Pure implementation verification, just tests that MarkChunkDirty() does what its name says
- `"edges should be symmetric - cost A to B equals cost B to A"` - Property that holds by construction, sanity check not real test
- `"should not create duplicate edges for entrances sharing two chunks"` - Implementation invariant check

**Well-Designed Tests:**
- `"should find a path on an empty grid"` - Basic validation of core functionality
- `"should not find a path when goal is walled off"` - Tests unreachable goal recognition
- `"should find path through gap in wall"` - Multi-chunk bottleneck navigation
- `"path should only contain walkable cells"` - Critical validation that paths use valid cells
- `"should find path using ladder to reach upper floor"` - Complex cross-z-level pathfinding
- `"HPA* path on z=1 should match A* path on z=1 open terrain"` - Algorithm consistency test

**Common Patterns:**
- ASCII map definition: `const char* map` with `InitGridFromAsciiWithChunkSize()`
- Standard chunk setup: `BuildEntrances()` then `BuildGraph()`
- Path validation: Check `pathLength > 0` and iterate to validate walkability

---

### test_mover.c

**Pointless Tests:**
- `"should increment tick counter each tick"` - Tests counter increment, trivial implementation detail
- `"should reset tick counter on ClearMovers"` - Cleanup verification only
- `"should count only active movers"` - Function obviously counts active movers, mirror test

**Well-Designed Tests:**
- `"should move mover toward goal after one tick"` - Core physics validation
- `"should deactivate mover when reaching goal"` - Goal completion behavior
- `"should push mover out when wall placed on it"` - Collision response to terrain changes
- `"should trigger repath when wall blocks path to next waypoint"` - Dynamic path invalidation
- `"should produce deterministic paths with same seed"` - Reproducibility test
- `"should handle many items and agents without deadlock"` - Stress test for production readiness
- `"should find paths when room entrance is in adjacent chunk"` - Boundary condition edge case

**Common Patterns:**
- Heavy setup boilerplate: `InitGridFromAsciiWithChunkSize()`, `ClearMovers()`, `ClearItems()`, `ClearStockpiles()` repeated in every test
- Path initialization: `InitMover()` or `InitMoverWithPath()` with goal and path arrays
- Simulation loop: `for (int tick = 0; tick < N; tick++) Tick()` very repetitive
- State checking helpers: `MoverIsIdle()`, `MoverIsCarrying()` abstract job state

---

### test_jobs.c

**Pointless Tests:**
- `"should spawn an item at a position"` - Basic API smoke test, every field assignment verified
- `"should track item count correctly"` - Trivial counter verification (spawn 1, count=1, spawn 2, count=2)
- `"should delete an item"` - Direct implementation verification
- `"should reserve an item for a mover"` - Mirrors implementation

**Well-Designed Tests:**
- `"should pick up item and deliver to stockpile"` - Full haul loop, complete workflow
- `"should respect stockpile type filters"` - Tests filtering logic across job assignment
- `"should stop hauling when stockpile is full"` - Capacity constraint validation
- `"should not allow two movers to claim the same item"` - Reservation safety
- `"should cancel job when path becomes blocked mid-haul"` - Dynamic obstacle handling
- `"should release reservation when item deleted externally"` - External deletion resilience
- `"should haul second item after stockpile is expanded"` - Dynamic stockpile expansion

**Common Patterns:**
- Extreme setup repetition: `ClearMovers()`, `ClearItems()`, `ClearStockpiles()` repeated 30+ times
- Job assignment loop: `AssignJobs()` + `JobsTick()` in loops
- Simulation timeout: `for (int i = 0; i < 1000; i++)` with early break
- State validation: Heavy use of helper functions for readability

---

### test_steering.c

**Pointless Tests:**
- `"should produce non-zero output"` - Merely asserts wander produces output, mirrors implementation design

**Well-Designed Tests:**
- `"should accelerate toward target"` - Core seek behavior validation
- `"should reach target over time"` - Integration test showing convergence
- `"should slow down when approaching target"` - Arrive behavior (deceleration + stopping)
- `"should accelerate away from threat"` - Flee behavior directional validation

**Common Patterns:**
- Repeated `Boid` initialization with identical fields
- Standard simulation loop: `for (float t = 0; t < duration; t += dt)`

---

## 2. Environment Systems

### test_water.c

**Pointless Tests:**
- `"should initialize water grid with all zeros"` - Pure implementation verification, trivial init check
- `"should clamp water level to max 7"` / `"should clamp to min 0"` - Bounds checking tests
- `"should report HasWater correctly"` / `"should report IsFull correctly"` - Getter tests that mirror setters

**Well-Designed Tests:**
- `"should equalize water levels between neighbors"` - Tests actual physical behavior
- `"should equalize in narrow horizontal channel"` / `"should not form staircase pattern"` - Catches real bugs (staircase formation)
- `"should spread water outward from source"` - Core gameplay verification
- `"should fall to lower z-level"` / `"should fall and then spread"` - Gravity + spreading interaction
- `"should push water up through pressure in U-bend"` / `"should respect pressure height limit"` - Complex DF-style water physics
- `"should not spread water through walls"` - Critical boundary condition
- `"should freeze full water at freezing temperature"` - State change mechanics

**Common Patterns:**
- Helper functions: `RunWaterTicks(n)`, `CountTotalWater()` - good DRY pattern
- Water state toggle: Disable/enable `waterEvaporationEnabled` around tests
- Verbose debug output: Conditional `printf()` behind `test_verbose` flag
- Water conservation checks: Tests verify `total == constant` invariant
- Multi-tick running: 50-500 ticks to let physics settle

---

### test_fire.c

**Pointless Tests:**
- `"should initialize fire grid with all zeros"` - Trivial initialization
- `"should clamp fire level to max 7"` / `"min 0"` - Bounds checking
- `"should report HasFire correctly"` - Getter test
- `"should have fire cells that movers should avoid"` - Placeholder with no pathfinding assertion

**Well-Designed Tests:**
- `"should burn and consume fuel on grass cells"` - Complete burn lifecycle
- `"should spread to adjacent flammable cells"` - Fire spread mechanics
- `"should spread orthogonally not diagonally"` - Directional constraint
- `"should not spread fire across water"` / `"through walls"` - Critical firebreak mechanics
- `"should not reignite burned cells"` / `"should clear vegetation when fire burns out"` - State persistence
- `"should generate smoke from fire"` / `"should rise through multiple z-levels"` / `"should have smoke at ALL intermediate z-levels"` - Catches cascading bug
- `"should not spread fire under walls at z+1 (DF mode)"` - Complex DF-mode walkability test

**Common Patterns:**
- Helper functions: `RunFireTicks()`, `RunSmokeTicks()`, `CountTotalSmoke()`, `CountBurningCells()`
- Terrain setup: Tests call `FillGroundLevel()` before fire tests
- Dual-system tests: Fire + smoke run together
- Debug visualization: ASCII art printing for complex tests
- Bug-catching narrative: Comments reference specific DF behavior and documented bugs

---

### test_terrain.c

**Pointless Tests:**
- `"should place border walls at z=0, same level as cave interior"` - Structural audit, not player behavior
- `"should not have a complete border ring floating at z=1"` - Implementation detail check
- `"should have cave interior walkable cells at z=0"` - Arbitrary threshold (25%) check
- `"should preserve the largest walkable component after connectivity fix"` - Just threshold check

**Well-Designed Tests:**
- `"should clear movers when terrain is regenerated"` / `"clear items"` / `"clear workshops/stockpiles"` - Critical side effect validation
- `"should clear water when switching from water terrain to non-water terrain"` / `"clear water sources"` - State reset validation
- `"should not have north ramps at cells where east also qualified"` - Real bug hunt, documents specific bug
- `"should still have walkable terrain across multiple seeds"` - Robustness across randomization

**Common Patterns:**
- Terrain generator testing: `GenerateCaves()`, `GenerateHills()`, `GenerateHillsSoilsWater()`
- Seed variation: Loop over multiple `worldSeed` values
- State restoration: Save/restore globals like `rampDensity`
- Complex grid verification: 3D grid iteration checking properties
- Audit-style assertions: Count things rather than simulate gameplay

---

### test_grid_audit.c

**Pointless Tests:**
- `"InitGridWithSizeAndChunkSize should set hpaNeedsRebuild flag"` - Incomplete, doesn't test what it promises
- `"player erases ramp at chunk boundary - exit chunk should be dirtied"` - Doesn't verify chunk was dirtied

**Well-Designed Tests:**
- `"player places ramp, then draws wall over it - rampCount should decrement"` / `"places wall over single ladder"` - Tests required cleanup, synchronization
- `"player right-click erases ramp in quick-edit mode"` - Catches specific bug with code line reference
- `"player has ramp exit on tree trunk - fire burns trunk - ramp should be removed"` - Cross-system cleanup test
- `"player places ladder on existing ramp"` - Tests replacement with cleanup
- `"player tries to place ramp at x=0 facing west"` / `"at y=0 facing south"` - Boundary condition tests
- `"player walls off low-side entry - ramp still structurally valid"` - Documents known limitation

**Common Patterns:**
- Grid setup macro: `GRID_10X10` defined once, reused
- Narrative test structure: Reads like user action sequence
- Cleanup verification: Verify counts stay synchronized
- Cross-system interactions: One system cleans up another's artifacts
- Incomplete test documentation: Names promise more than assertions check

---

## 3. Weather Systems

### test_temperature.c

**Pointless Tests:**
- `"should clamp temperature to max 2000"` / `"min -100"` - Pure clamping verification
- `"should set temperature within bounds"` - Vacuous setter test
- `"should mark cells as stable when temperature settles"` - Internal variable check, no behavior verification

**Well-Designed Tests:**
- `"should spread heat outward from hot cell"` - Thermal physics behavior
- `"should keep heat mostly contained inside stone room"` - Material insulation scenario
- `"should decay hot temperature toward ambient"` / `"cold toward ambient"` - Core equilibration mechanic
- `"should have correct ambient temperatures at each depth"` - Depth gradient formula
- `"should handle both heat and cold sources nearby"` - Contradictory source scenario

**Common Patterns:**
- Helper `RunTempTicks(n)` for simulation loops
- `SetupWeatherGrid()` initializes identical grids
- Arbitrary large tick counts (500, 1000) without justification
- Broad range checks (`> ambient + 10`) suggest uncertainty

---

### test_steam.c

**Pointless Tests:**
- `"should initialize steam grid with all zeros"` - Vacuous clear check
- `"should clamp steam level to max 7"` / `"min 0"` - Pure clamping
- `"should add steam correctly"` - Tests arithmetic (2+3=5)
- `"should not rise through walls"` - Only checks one cell, could pass if steam doesn't rise at all

**Well-Designed Tests:**
- `"should complete water cycle: boil -> rise -> condense -> fall"` - Excellent integration test
- `"should spread horizontally when blocked above"` - Adaptation to obstacles
- `"should eventually evaporate all water with heaters in center"` - Strong integration test
- `"should generate steam when water reaches boiling temperature"` - Phase transition mechanic

**Common Patterns:**
- Helpers: `CountTotalSteam()`, `CountTotalWater()`
- Manual system enable/disable for isolation
- Many tests set `SetAmbientSurfaceTemp(100)` to prevent condensation - fragile

---

### test_weather.c

**Pointless Tests:**
- `"should start with WEATHER_CLEAR"` - Default value is default value
- `"should initialize wind to calm"` / `"set positive transition timer"` - Trivial state checks
- `"should set intensity to 1.0 initially"` - Initialization test
- `"should return correct weather name strings"` - String constant test

**Well-Designed Tests:**
- `"should never transition to SNOW outside winter"` / `"allow SNOW in winter"` - Seasonal gating
- `"should never produce THUNDERSTORM outside summer"` / `"allow in summer"` - Seasonal logic
- `"should increase wetness on exposed dirt during rain"` - Weather→environment causality
- `"should not increase wetness on sheltered cells"` - Roof protection test
- `"should increase wetness faster during HEAVY_RAIN than RAIN"` - Comparative intensity test
- `"should have high wind during THUNDERSTORM"` - Weather type → wind strength

**Common Patterns:**
- `SetupWeatherGrid()` helper
- `ForceWeather()` bypasses randomness for determinism
- 2000+ iterations for RNG statistics (expensive but necessary)
- Tolerance ranges for floating-point (`< 0.01f`)

---

### test_seasons.c

**Pointless Tests:**
- `"should return SPRING on day 1"` - Trivial formula test
- `"should return correct name strings for each season"` - String constant test
- `"should handle invalid season gracefully"` - Only checks non-NULL, trivial
- `"should be normal in summer"` (vegetation) - Range so wide (0.8-1.2) it's meaningless

**Well-Designed Tests:**
- `"should produce correct peak values with known parameters"` - Tests sine wave formula against bounds
- `"should have longer days in summer than winter"` - Day length variation
- `"should still apply depth decay underground"` - Underground + season interaction
- `"should transition smoothly across seasons"` - Checks for discontinuities
- `"should wrap back to SPRING after a full year"` - Year-wrapping boundary

**Common Patterns:**
- Helper `SetupSeasonTest()`
- Different `daysPerSeason` values (1, 3, 7) for coverage
- Loop over all days to check invariants
- Magic numbers (day 15, day 25) without explanation

---

### test_wind.c

**Pointless Tests:**
- `"should return zero when wind strength is zero"` - Mathematically obvious
- `"should have normalized wind direction"` - Checks length < 1.1, brittle

**Well-Designed Tests:**
- `"should return positive value when moving downwind"` / `"negative upwind"` - Directional logic
- `"should scale with wind strength"` - Linear scaling verification
- `"should drift smoke downwind over time"` - Statistical test (50 trials)
- `"should spread evenly with no wind"` - Control test for balance
- `"should spread fire more downwind than upwind"` - Statistical (200 trials)
- `"should dry exposed cells faster with strong wind"` - Wind→drying causality
- `"should lower effective temperature with wind"` - Wind chill test

**Common Patterns:**
- Statistical/comparative testing (run twice, compare)
- Large 16x16 grid (unusual)
- Disable dissipation (`smokeRiseInterval = 999.0f`) to isolate effects
- Seeded trials for reproducibility

---

### test_snow.c

**Pointless Tests:**
- `"initializes to zero"` - Trivial initialization
- `"sets and gets snow levels"` - Getter/setter vacuous test
- `"clamps snow levels to 0-3"` - Pure clamping
- `"handles out-of-bounds safely"` - Low value check
- `"no penalty without snow"` - Obvious (no snow = 1.0x)
- `"varies intensity by weather type"` (cloud shadow) - Doesn't verify values are useful

**Well-Designed Tests:**
- `"accumulates during WEATHER_SNOW on exposed cells"` - Weather→snow causality
- `"does not accumulate on sheltered cells"` - Roof blocks snow
- `"only accumulates below freezing temperature"` - Temperature threshold
- `"melts above freezing and increases wetness"` - Snow→water conversion
- `"creates mud on dirt when snow melts"` - Chain of effects (snow→wetness→mud)
- `"extinguishes fire on snowy cells"` / `"does not extinguish with light snow"` - Threshold tests
- `"moves with wind over time"` (cloud shadow) - Wind integration

**Common Patterns:**
- `SetTestTemperature()` helper
- Manual grid setup repetitive
- Direct struct access for fire initialization (fragile)

---

### test_mud.c

**Pointless Tests:**
- `"should identify dirt-like materials as soil"` / `"not identify non-soil"` - Hardcoded constant tests
- `"should not be muddy when dry"` / `"when only damp"` - Trivial state checks
- `"should handle out-of-bounds gracefully"` - Just checks no crash

**Well-Designed Tests:**
- `"should be muddy when wet (wetness 2) on dirt"` - Establishes wetness threshold
- `"should not be muddy on stone"` / `"on constructed walls"` - Material-specific mud
- `"should set wetness on soil below water after sync interval"` - Water→soil interaction
- `"should map high water level to soaked"` - Water-to-wetness mapping
- `"should not set wetness on non-soil materials"` - Reinforces material specificity
- `"should dry wetness over time when no water present"` - Drying mechanic
- `"should not dry if water is still present above"` - Water prevents drying
- `"should track more dirt from muddy source"` - Gameplay consequence (dirty floors)

**Common Patterns:**
- `SetupDirtGrid()` helper
- Direct `GET_CELL_WETNESS()` manipulation
- State verification (before/after checks)
- Weak assertion on line 178 (`>= 0` always passes)

---

## 4. Resource Systems

### test_materials.c

**Pointless Tests:**
- `"should have ITEM_ROCK for wall"` (lines 155-157) - **Exact duplicate** of lines 136-138
- `"should have ITEM_NONE for air"` (lines 159-161) - **Exact duplicate** of lines 151-153
- `"should have dropCount of 1 for wall"` (lines 171-173) - **Duplicate** of lines 163-165
- `"should return override for leaves/saplings"` - Mirrors switch case logic
- `"should use material canonical sprite when no override"` - Lookup table mirror
- `"wall_flags"` tests - Bit-flag lookups trivially true by definition

**Well-Designed Tests:**
- `"should set wall material when completing log wall blueprint with pine"` - End-to-end construction
- `"should drop ITEM_LOG when mining constructed wood wall"` - Build→mine→resource recovery
- `"should allow CELL_WALL with MAT_OAK to burn"` - Material property interaction
- `"should create natural granite floor when mining natural rock"` - Material transformation
- `"should complete full cycle: build wood wall then mine it"` - Full lifecycle integration
- `"should resolve tree sprites via sprite overrides"` - Per-species visual rendering

**Common Patterns:**
- Repetitive setup: `InitGridFromAsciiWithChunkSize("...", 4, 1); InitDesignations(); ClearItems();`
- Material delivery pattern: Blueprint → loop spawn+deliver → complete → verify
- Setter/getter mirror tests: Round-trip without behavior validation

---

### test_trees.c

**Pointless Tests:**
- `"should grow sapling into trunk after enough ticks"` - Just timer loop, doesn't test gameplay
- `"should vary trunk heights"` - Doesn't actually assert variation, just bounds

**Well-Designed Tests:**
- `"should drop saplings when tree is felled"` - Full tree lifecycle with item spawning
- `"should complete gather sapling job end-to-end"` - Full job integration with movement/work/spawning
- `"should plant sapling item as sapling cell"` - Item consumption + cell state change
- `"should complete plant sapling job end-to-end"` - Multi-step job with pathfinding
- `"should complete sapling gather->plant cycle with stockpile filtering"` - Inventory management
- `"should support full plant->grow->chop cycle"` - Complete lifecycle end-to-end
- `"should create canopy with leaves around trunk top"` - Visual structure validation

**Common Patterns:**
- Helper functions: `CountCellType()`, `CountSaplingItems()`, `CountStandingTrunks()`
- `SetupBasicGrid()` creates consistent dirt floor + air
- Job integration: `Tick(); AssignJobs(); JobsTick();` loop
- Modifier restoration: Save/restore globals to avoid side effects

---

### test_groundwear.c

**Pointless Tests:**
- `"should initialize wear grid with all zeros"` - Loop verifies memset works
- `"should not trample wall cells"` - Implementation check mirror
- `"should not trample when disabled"` - Boolean flag check
- `"should decay wear at all z-levels"` - Tests loop infrastructure

**Well-Designed Tests:**
- `"should accumulate wear over multiple tramplings"` - Wear stacking behavior
- `"should cap wear at wearMax"` - Saturation for balance
- `"should update surface overlay based on wear level"` - Visual feedback progression (TALL→SHORT→TRAMPLED→BARE)
- `"should recover grass overlay as wear decays"` - Recovery cycle
- `"should complete tall grass->bare->tall grass cycle"` - Full cycle test
- `"should create worn path on heavily trafficked area"` - Spatial behavior for world feel

**Common Patterns:**
- Repeated grid setup: `InitGridFromAsciiWithChunkSize("dddd\n", 4, 1)`
- Threshold value swaps: Modify wear thresholds then restore
- Multiplied tramplings: `for (int i = 0; i < 4; i++)` to reach wear levels

---

### test_floordirt.c

**Pointless Tests:**
- `"should initialize dirt grid with all zeros"` - Memset test
- `"should not track dirt from floor to floor"` - Implementation mirror
- `"should not track dirt from air to floor"` - Similar mirror
- `"should not track when disabled"` - Boolean flag check
- `"should handle out-of-bounds gracefully"` - No-crash check with `expect(true)`
- `"should handle invalid mover index"` - Similar no-crash check

**Well-Designed Tests:**
- `"should track dirt from dirt to floor"` - Core mechanic: soil→floor tracking
- `"should cap dirt at DIRT_MAX"` - Saturation for balance
- `"should apply stone floor multiplier"` - Different materials have different rates
- `"should update dirtActiveCells when cleaning to zero"` - Counter management for optimization
- `"should track previous cell per mover independently"` - Per-mover state isolation
- `"should not track on first call (no previous cell)"` - Initialization edge case
- `"should not track when staying in same cell"` - Stationary movers don't re-dirty

**Common Patterns:**
- Per-cell setup: Manual grid setup for specific scenarios
- Dirt source detection pattern: Create natural soil, test tracking
- Counter assertion: Verify `dirtActiveCells` optimization counter

---

## 5. Workshop Systems

### test_workshop_stockpile_linking.c

**Pointless Tests:**
- `"should successfully link stockpile to workshop"` - Direct implementation mirror, tautological
- `"should detect stockpile is linked"` - Wrapper test that can't fail

**Well-Designed Tests:**
- `"should link up to 4 stockpiles"` - Verifies `MAX_LINKED_STOCKPILES` capacity boundary
- `"should reject 5th link (max 4 slots)"` - Guard condition boundary test
- `"should reject duplicate link"` - Duplicate detection validation
- `"should unlink by slot and shift remaining"` - Non-trivial array-shifting logic
- `"should unlink by stockpile index"` - Search + shift combination
- `"should return false if stockpile not linked"` - Graceful failure boundary
- `"should clear all links"` - Cleanup operation sanity check

**Common Patterns:**
- Boilerplate setup: `ClearWorkshops()`, `ClearStockpiles()`, manual `active=true` per test
- Array initialization loop: `for (int i = 0; i < N; i++)` pattern
- Manual state verification: Tightly coupled to internal representation

---

### test_workshop_diagnostics.c

**Pointless Tests:**
- `"shows OUTPUT_FULL when it should show INPUT_EMPTY - EXPECTED TO FAIL"` - Explicitly broken, just documents bug with setup verifications only
- `"documents the expected state transitions"` - 100% pointless: only has `expect(true)`, all comments

**Well-Designed Tests:**
- **None.** Both tests are stubs/documentation, not executable validations

**Common Patterns:**
- Comment-driven test design: Tests are primarily documentation
- Incomplete test infrastructure: Missing critical setup helpers
- Manual workshop construction: Field-by-field assignment

**Summary:** This file is incomplete and needs actual integration tests.

---

## 6. Lighting & Time Systems

### test_lighting.c

**Pointless Tests:**
- `"should initialize light grid with all zeros"` - Iterates every cell checking zeros, tests memset
- `"should have zero light sources after init"` - Trivial `== 0` after init
- `"should set dirty flag on init"` - Flag toggle verification
- `"should mark dirty when adding/removing a source"` - Implementation flag assertions
- `"should return WHITE when lighting is disabled"` - Trivial guard clause
- `"should return WHITE for out-of-bounds coordinates"` - Trivial bounds check

**Well-Designed Tests:**
- `"should spread sky light into adjacent dark cells"` - BFS spread with falloff
- `"should not spread sky light through solid cells"` - Critical constraint
- `"should block sky light below solid ceiling"` - Shadow specification
- `"should produce circular light shape (Euclidean falloff)"` - Prevents square artifacts
- `"should not propagate block light through solid cells"` - Block light constraints
- `"should bleed z=1 torch light into GetLightColor at z=2"` - Z-1 bleed mechanic
- `"rendering z-1 floor scenario"` tests - Integration tests for actual rendering

**Common Patterns:**
- Grid init + setup → `RecomputeLighting()` → assertion loop (repetitive)
- State mutation: `lightingEnabled = true` in almost every test
- Manual restore of globals: Prevents pollution but error-prone
- Test verbosity flag: `test_verbose` with scattered printfs

---

### test_time.c

**Pointless Tests:**
- `"should initialize with default values"` - Globals equal hardcoded constants
- `"should reset to initial values"` - Reset function verification
- `"should return false when paused"` / `"true when not paused"` - Trivial return value test
- `"should not lose precision at high game time values"` - Tests floating-point, not time system

**Well-Designed Tests:**
- `"should accumulate game time at 1x speed"` - Core behavior: 60 ticks = 1 second
- `"should accumulate game time faster at 10x speed"` - Speed scaling test
- `"should not accumulate time when paused"` - Critical pause functionality
- `"should increment dayNumber when day completes"` / `"track multiple days"` - Day wrapping
- `"should update game time through Tick()"` - Integration test
- `"should skip simulation when paused"` - Pause prevents simulation
- `"should handle 10 days of game time"` - Large-scale behavior

**Common Patterns:**
- `InitTime()` + mutation + loop `UpdateTime()`
- Grid initialization but don't use it (unnecessary setup)
- Tolerance ranges (`>= 0.99 && <= 1.01`) - arbitrary, uncommented

---

### test_time_specs.c

**Pointless Tests:**
- `"smoke should be partially dissipated at half time"` - Can't reliably verify spec due to randomness
- `"higher fireSpreadBase should spread faster"` - Same random seed means deterministic, tests implementation luck

**Well-Designed Tests:**
- `"fire should NOT spread before spread interval elapses"` - Verifies interval respected
- `"fire should spread to neighbors after spread interval"` - Clear spec
- `"smoke should fully dissipate within smokeDissipationTime"` - Cvar controls behavior
- `"temperature should decay toward ambient"` / `"cold should warm"` - Asymptotic behavior
- `"one game-day should equal dayLength game-seconds"` / `"timeOfDay should be 12.0 at midday"` - Human-readable time specs
- `"heat should rise faster than it sinks"` - Critical simulation property (weak assertion though)
- `"mover should move slower in deep water"` - Perfect spec: water depth → speed

**Common Patterns:**
- `SetupTestGrid()` helper - excellent DRY
- `ResetTestState(seed)` for reproducibility (inconsistent usage)
- `RunGameSeconds()` instead of manual loops - cleaner API
- Specification comments: "fire should spread" style naming

---

### test_high_speed.c

**Pointless Tests:**
- `"should handle gameSpeed of 1000"` - Fuzz test that just checks no crash, arbitrary bounds

**Well-Designed Tests:**
- `"mover should not clip through walls at 10x game speed"` / `"at 100x"` - Critical invariant: no wall clipping
- `"mover should reach goal correctly at high speed"` - Movement still works correctly
- `"multiple movers should not clip through walls at high speed"` - Multi-agent stress test
- `"fire spread should remain bounded at 100x speed"` - Stability: fire doesn't explode
- `"temperature should remain bounded at extreme speeds"` - Temperature doesn't diverge
- `"pause and resume should work correctly"` - Pause/resume desync prevention
- `"should handle rapid speed changes"` - State machine fuzz test

**Common Patterns:**
- Setup grid helpers: `SetupCorridorGrid`, `SetupOpenGrid`, `SetupWalledGrid`
- `IsMoverPositionValid()` helper encapsulates checks
- Clear pattern: init → run high speed → assert invariant
- All subsystems initialized per test (verbose but necessary)

---

## 7. Sound System

### test_soundsystem.c (1200+ tests)

**Pointless Tests (60+ tests):**

**Pure Math Tautologies:**
- Oscillator tests: "should generate sawtooth output correctly" - Computes `2 * phase - 1`, checks result is `2 * phase - 1` (mirrors implementation exactly)
- Triangle/sine/square wave tests: Test code IS the implementation
- ADSR envelope tests: Manually assign `envStage = 1`, then `envStage = 2`, assert `envStage == 2`

**Trivial Initialization:**
- `"should initialize synth context with defaults"` - Checks each field equals hardcoded default
- `"should initialize drums context"` - Same pattern
- `"should initialize with default values"` (dub_loop_basic) - Pure constant assertion

**Tautological Effect Tests:**
- Bitcrusher: Test computes expected output using exact same algorithm as implementation
- Oscillator phase tests: Test hardcodes logic, then asserts that computation
- Distortion soft clipping: Computes `fullyWet` independently, checks inequality (doesn't validate actual algorithm)

**Trivial Pass-Through (6+ tests):**
- Repeated across distortion/delay/bitcrusher/reverb/sidechain: `enabled = false; assert output == input`

**No-Op Setup Tests:**
- Pattern management: Creates pattern, initializes, loops checking initialization worked

**Well-Designed Tests (40+ tests):**

**Integration with Callbacks:**
- `"should trigger melody notes from sequencer"` / `"should release notes after gate time"` / `"should release when playback stops"` - Full system: sequencer → callbacks → side effects
- Validates real behavior: music actually happens
- Tests timing interactions (gate durations, step advances)

**State Machine Tests:**
- Trigger conditions: `"should trigger every 2nd time with COND_1_2"` / `"2nd of every 4 with COND_2_4"` - Complex sequencer features with boundary cases

**Timing + Parameter Interaction:**
- Dilla timing: `"should apply kick nudge (early)"` / `"should apply swing to off-beats"` / `"should combine multiple timing offsets"` - Multi-parameter interaction with boundary validation

**Multi-Input Selection:**
- Dub loop input source: `"should select only drums when DUB_INPUT_DRUMS"` / `"only synth when DUB_INPUT_SYNTH"` - Input filtering with converse cases

**P-Lock Modification:**
- `"should update existing p-lock value"` / `"should add multiple p-locks to same step"` / `"should rebuild index after clearing"` - Mutation behavior, data structure invariants, deduplication

**Volume Clamping:**
- `"should clamp track volume to 0-1 range"` - Real UX edge case: invalid values

**Envelope Gate Duration:**
- `"should release notes after gate time"` - Timing validation with dynamic calculation

**Common Patterns:**

**Setup/Cleanup:**
```c
_ensureSeqCtx();
initSequencer(NULL, NULL, NULL, NULL);
// ... test ...
seq.playing = false;
```
Cleanup inconsistent - some tests do it, others don't. Fragile global state.

**Context Initialization Trio:**
```c
_ensureSeqCtx();
initSequencer(NULL, NULL, NULL, NULL);  // Always NULL, NULL, NULL, NULL - why?
setCallbacks(0, test_callback_trigger, test_callback_release);
```

**Manual Buffer Clearing:**
Tests manually clear global buffers because implementation doesn't reset them. Code smell - should have proper reset function.

**"Impulse + Wait + Check":**
```c
processEffect(1.0f, dt);  // Impulse
for (int i = 0; i < delaySamples - 1; i++) {
    processEffect(0.0f, dt);  // Silence
}
float output = processEffect(0.0f, dt);
```
Fragile loop bounds. Weak assertions (`output != 0.0f` too vague).

**Callback Counter Pattern (GOOD):**
```c
static int melody_trigger_count = 0;
void test_melody_trigger(...) { melody_trigger_count++; }
void reset_melody_counters() { melody_trigger_count = 0; }
```
Solid pattern for testing callbacks get called correctly.

**Track/Drum Index Symmetry:**
Tests that drum/melody setters map to unified track indexing. Validates abstraction without exposing it.

**Redundant Condition Testing:**
`COND_FILL` and `COND_NOT_FILL` tests are nearly identical, testing inverse of each other. Second is redundant.

---

## Overall Patterns to Extract

### 1. Grid Initialization (CRITICAL)

**Current State:** Every test file has its own variant:
- `SetupBasicGrid()` (test_trees.c)
- `SetupWeatherGrid()` (test_weather.c, test_temperature.c)
- `SetupDirtGrid()` (test_floordirt.c, test_groundwear.c)
- `SetupTestGrid()` (test_time_specs.c)
- Manual `InitGridFromAsciiWithChunkSize()` + clear calls (everywhere)

**Recommendation:** Create shared `test_helpers.h` with:
```c
void TestGrid_Init(const char* map, int width, int height);
void TestGrid_Clear();  // ClearMovers, ClearItems, ClearStockpiles, etc.
void TestGrid_WithSubsystems();  // Init water, fire, temp, etc.
```

### 2. Simulation Loop Helpers

**Current:** Repeated everywhere:
```c
for (int i = 0; i < 100; i++) {
    Tick();
    AssignJobs();
    JobsTick();
}
```

**Recommendation:**
```c
void RunSimulation(int ticks);
void RunSimulationUntil(bool (*condition)(void), int maxTicks);
```

### 3. State Verification Helpers (GOOD - Keep)

Already exist and work well:
- `MoverIsIdle()`, `MoverIsCarrying()`
- `CountTotalWater()`, `CountTotalSmoke()`
- `CountCellType()`, `CountSaplingItems()`

**Keep these!** They make tests readable.

### 4. Test-Specific Cleanup

**Current:** Tests manually restore globals:
```c
bool oldValue = someGlobal;
someGlobal = testValue;
// ... test ...
someGlobal = oldValue;  // Error-prone!
```

**Recommendation:** Use test fixtures or auto-restore pattern (C limitations make this hard, but document it as a pattern).

---

## Recommendations

### High Priority (Delete These)

1. **Remove all initialization-only tests** (~50 tests)
   - `"should initialize X grid with all zeros"`
   - `"should have zero count after init"`
   - These test memset/initialization, not behavior

2. **Remove all exact duplicate tests** (~10 tests)
   - test_materials.c has 3 exact duplicates (lines 155-173)
   - Consolidate identical tests

3. **Remove getter/setter mirror tests** (~30 tests)
   - `"should set X correctly"` + `"should get X correctly"` with no logic
   - Only keep if there's actual validation/transformation

4. **Remove tautological math tests** (~40 tests in soundsystem)
   - Tests that compute expected output using same algorithm as implementation
   - Don't test correctness, just prove compilation worked

### Medium Priority (Consolidate These)

5. **Extract common grid initialization** (~200 lines of boilerplate)
   - Create shared `test_helpers.c` with standard grids
   - Use `SetupBasicGrid()`, `SetupWeatherGrid()`, etc.

6. **Extract simulation loop helpers** (~150 lines)
   - `RunSimulation(ticks)`
   - `RunSimulationUntil(condition, maxTicks)`

7. **Consolidate redundant tests** (~20 tests)
   - test_wind.c: FILL and NOT_FILL test the same thing twice
   - test_soundsystem.c: 6 identical pass-through tests

### Low Priority (Document These)

8. **Document callback counter pattern** (soundsystem)
   - This pattern is actually good
   - Make it a documented test pattern for others to follow

9. **Document integration test best practices**
   - test_jobs.c has excellent examples
   - Extract patterns for multi-system tests

10. **Fix incomplete tests** (test_workshop_diagnostics.c)
    - Two tests are just `expect(true)` or broken stubs
    - Either implement them or remove them

---

## Statistics

| Category | Pointless | Well-Designed | Ratio |
|----------|-----------|---------------|-------|
| Core Systems | 15% | 85% | Good |
| Environment | 25% | 75% | Acceptable |
| Weather | 40% | 60% | Needs cleanup |
| Resources | 20% | 80% | Good |
| Workshops | 50% | 25% (incomplete) | Bad |
| Lighting & Time | 30% | 70% | Acceptable |
| Sound | 60% | 40% | Needs major cleanup |
| **Overall** | **30-40%** | **60-70%** | **Needs improvement** |

---

---

## Exact Changes Required

### DELETE - High Priority (Immediate Removal Candidates)

#### test_materials.c
- **Line 155-157:** DELETE `"should have ITEM_ROCK for wall"` - **Exact duplicate** of lines 136-138
- **Line 159-161:** DELETE `"should have ITEM_NONE for air"` - **Exact duplicate** of lines 151-153
- **Line 171-173:** DELETE `"should have dropCount of 1 for wall"` - **Exact duplicate** of lines 163-165
- **Lines 897-909:** DELETE `"should return override for leaves/saplings"` - Just mirrors switch case
- **Lines 932-944:** DELETE all 3 wall_flags tests - Bit-flag lookups trivially true by definition

#### test_water.c
- **DELETE:** `"should initialize water grid with all zeros"` - Trivial memset verification
- **DELETE:** `"should clamp water level to max 7"` - Pure bounds checking
- **DELETE:** `"should clamp water level to min 0"` - Pure bounds checking
- **DELETE:** `"should report HasWater correctly"` - Getter mirrors setter
- **DELETE:** `"should report IsFull correctly"` - Getter mirrors setter

#### test_fire.c
- **DELETE:** `"should initialize fire grid with all zeros"` - Trivial initialization
- **DELETE:** `"should clamp fire level to max 7"` - Pure bounds checking
- **DELETE:** `"should clamp fire level to min 0"` - Pure bounds checking
- **DELETE:** `"should report HasFire correctly"` - Getter test
- **DELETE:** `"should have fire cells that movers should avoid"` - Placeholder with no assertion

#### test_temperature.c
- **DELETE:** `"should clamp temperature to max 2000"` - Pure clamping
- **DELETE:** `"should clamp temperature to min -100"` - Pure clamping
- **DELETE:** `"should set temperature within bounds"` - Vacuous setter test
- **DELETE:** `"should mark cells as stable when temperature settles"` - Internal variable check

#### test_steam.c
- **DELETE:** `"should initialize steam grid with all zeros"` - Vacuous clear check
- **DELETE:** `"should clamp steam level to max 7"` - Pure clamping
- **DELETE:** `"should clamp steam level to min 0"` - Pure clamping
- **DELETE:** `"should add steam correctly"` - Tests arithmetic (2+3=5)

#### test_weather.c
- **DELETE:** `"should start with WEATHER_CLEAR"` - Default value is default value
- **DELETE:** `"should initialize wind to calm"` - Trivial state check
- **DELETE:** `"should set a positive transition timer"` - Trivial state check
- **DELETE:** `"should set intensity to 1.0 initially"` - Initialization test
- **DELETE:** `"should return correct weather name strings"` - String constant test

#### test_seasons.c
- **DELETE:** `"should return SPRING on day 1"` - Trivial formula test
- **DELETE:** `"should return correct name strings for each season"` - String constant test
- **DELETE:** `"should handle invalid season gracefully"` - Only checks non-NULL
- **DELETE:** `"should be normal in summer"` (vegetation) - Range too wide (0.8-1.2) to be meaningful

#### test_wind.c
- **DELETE:** `"should return zero when wind strength is zero"` - Mathematically obvious
- **DELETE:** `"should have normalized wind direction"` - Brittle check (< 1.1)

#### test_snow.c
- **DELETE:** `"initializes to zero"` - Trivial initialization
- **DELETE:** `"sets and gets snow levels"` - Getter/setter vacuous test
- **DELETE:** `"clamps snow levels to 0-3"` - Pure clamping
- **DELETE:** `"handles out-of-bounds safely"` - Low value check
- **DELETE:** `"no penalty without snow"` - Obvious (no snow = 1.0x)

#### test_mud.c
- **DELETE:** `"should identify dirt-like materials as soil"` - Hardcoded constant test
- **DELETE:** `"should not identify non-soil materials as soil"` - Hardcoded constant test
- **DELETE:** `"should not be muddy when dry"` - Trivial state check
- **DELETE:** `"should not be muddy when only damp (wetness 1)"` - Trivial state check
- **DELETE:** `"should handle out-of-bounds gracefully"` - Just checks no crash

#### test_trees.c
- **DELETE:** `"should grow sapling into trunk after enough ticks"` (lines 127-148) - Timer loop, not gameplay
- **DELETE:** `"should vary trunk heights"` (lines 463-491) - Doesn't assert variation, just bounds

#### test_groundwear.c
- **DELETE:** `"should initialize wear grid with all zeros"` (lines 19-33) - Loop verifies memset
- **DELETE:** `"should not trample wall cells"` (lines 135-150) - Implementation mirror
- **DELETE:** `"should not trample when disabled"` (lines 152-168) - Boolean flag check
- **DELETE:** `"should decay wear at all z-levels"` (lines 388-420) - Tests loop infrastructure

#### test_floordirt.c
- **DELETE:** `"should initialize dirt grid with all zeros"` (lines 19-32) - Memset test
- **DELETE:** `"should not track dirt from floor to floor"` (lines 211-228) - Implementation mirror
- **DELETE:** `"should not track dirt from air to floor"` (lines 230-244) - Implementation mirror
- **DELETE:** `"should not track when disabled"` (lines 288-305) - Boolean flag check
- **DELETE:** `"should handle out-of-bounds MoverTrackDirt gracefully"` (lines 456-468) - No-crash with `expect(true)`
- **DELETE:** `"should handle MoverTrackDirt with invalid mover index"` (lines 470-481) - No-crash with `expect(true)`

#### test_mover.c
- **DELETE:** `"should increment tick counter each tick"` (lines 252-263) - Counter increment test
- **DELETE:** `"should reset tick counter on ClearMovers"` (lines 265-274) - Cleanup verification
- **DELETE:** `"should count only active movers"` (lines 278-302) - Function mirrors implementation

#### test_jobs.c
- **DELETE:** `"should spawn an item at a position"` (lines 181-192) - Basic API smoke test
- **DELETE:** `"should track item count correctly"` (lines 194-204) - Trivial counter (1, 2)
- **DELETE:** `"should delete an item"` (lines 206-214) - Direct implementation verification
- **DELETE:** `"should reserve an item for a mover"` (lines 218-227) - Mirrors implementation

#### test_steering.c
- **DELETE:** `"should produce non-zero output"` (lines 95-110) - Mirrors implementation design

#### test_pathfinding.c
- **DELETE:** `"should mark chunks as dirty when walls are placed"` (lines 32-47) - Pure implementation verification
- **DELETE:** `"edges should be symmetric - cost A to B equals cost B to A"` (lines 239-261) - Property holds by construction
- **DELETE:** `"should not create duplicate edges for entrances sharing two chunks"` (lines 382-398) - Implementation invariant

#### test_terrain.c
- **DELETE:** `"should place border walls at z=0, same level as cave interior"` - Structural audit
- **DELETE:** `"should not have a complete border ring floating at z=1"` - Implementation detail
- **DELETE:** `"should have cave interior walkable cells at z=0"` - Arbitrary threshold (25%)

#### test_grid_audit.c
- **DELETE:** `"InitGridWithSizeAndChunkSize should set hpaNeedsRebuild flag"` - Incomplete, doesn't test what it promises
- **DELETE:** `"player erases ramp at chunk boundary - exit chunk should be dirtied"` - Doesn't verify chunk dirtied

#### test_lighting.c
- **DELETE:** `"should initialize light grid with all zeros"` (lines 16-30) - Iterates every cell checking zeros
- **DELETE:** `"should have zero light sources after init"` (lines 32-37) - Trivial `== 0`
- **DELETE:** `"should set dirty flag on init"` (lines 39-44) - Flag toggle
- **DELETE:** `"should mark dirty when adding/removing a source"` (lines 152-172) - Flag toggles
- **DELETE:** `"should return WHITE when lighting is disabled"` (lines 524-537) - Trivial guard clause
- **DELETE:** `"should return WHITE for out-of-bounds coordinates"` (lines 539-553) - Trivial bounds check

#### test_time.c
- **DELETE:** `"should initialize with default values"` (lines 20-29) - Globals equal constants
- **DELETE:** `"should reset to initial values"` (lines 31-43) - Reset verification
- **DELETE:** `"should return false when paused"` (lines 85-92) - Trivial return value
- **DELETE:** `"should return true when not paused"` (lines 94-101) - Trivial return value
- **DELETE:** `"should not lose precision at high game time values"` (lines 266-276) - Tests floating-point

#### test_high_speed.c
- **DELETE:** `"should handle gameSpeed of 1000"` (lines 375-399) - Fuzz test, arbitrary bounds

#### test_workshop_stockpile_linking.c
- **DELETE:** `"should successfully link stockpile to workshop"` (lines 17-33) - Direct implementation mirror
- **DELETE:** `"should detect stockpile is linked"` (lines 35-48) - Wrapper test that can't fail

#### test_workshop_diagnostics.c
- **DELETE:** `"shows OUTPUT_FULL when it should show INPUT_EMPTY - EXPECTED TO FAIL"` (lines 17-74) - Explicitly broken, just documents bug
- **DELETE:** `"documents the expected state transitions"` (lines 78-96) - 100% pointless: only `expect(true)`

#### test_soundsystem.c (MAJOR CLEANUP NEEDED)
- **DELETE:** All oscillator math tests (lines 748-796) - `"should generate sawtooth/triangle/sine output correctly"` - Test code IS implementation
- **DELETE:** All ADSR envelope trivial tests (lines 804-860) - Manual stage assignment tests
- **DELETE:** `"should initialize synth context with defaults"` (line 720) - Hardcoded default check
- **DELETE:** `"should initialize drums context"` (line 1043) - Same pattern
- **DELETE:** `"should initialize with default values"` (dub_loop, line 3495) - Constant assertion
- **DELETE:** All bitcrusher tautology tests (line 1364+) - Computes expected using same algorithm
- **DELETE:** All 6+ pass-through tests - `enabled = false; assert output == input` across distortion/delay/bitcrusher/reverb/sidechain
- **DELETE:** Pattern management no-op tests (line 462) - Loop checking initialization worked

**Total Deletions: ~80-100 tests**

---

### CONSOLIDATE - Medium Priority

#### test_wind.c
- **CONSOLIDATE:** `"should trigger only during fill mode with COND_FILL"` + `"should trigger only when not in fill mode with COND_NOT_FILL"` (lines 295-315)
  - **Action:** Keep first test, delete second (tests inverse of same thing)

#### test_soundsystem.c
- **CONSOLIDATE:** 6 identical effect pass-through tests into single parameterized test
  - **Action:** Create one `"should pass through input when effect disabled"` with loop over all effects

---

### FIX/IMPROVE - Low Priority

#### test_time_specs.c
- **FIX:** `"smoke should be partially dissipated at half time"` (lines 205-223)
  - **Issue:** Can't reliably verify due to randomness
  - **Action:** Either make deterministic or strengthen assertion

- **FIX:** `"higher fireSpreadBase should spread faster"` (lines 136-177)
  - **Issue:** Uses same random seed for both scenarios (deterministic but lucky)
  - **Action:** Use different seeds or run statistical comparison

#### test_seasons.c
- **FIX:** `"should be normal in summer"` vegetation test
  - **Issue:** Range 0.8-1.2 too wide to catch bugs
  - **Action:** Tighten range or test specific values

#### test_mud.c
- **FIX:** Test on line 178 with weak assertion
  - **Issue:** Comment says "at least damp" but checks `>= 0` (always passes)
  - **Action:** Change to `>= 1` to match "damp" definition

#### test_workshop_diagnostics.c
- **IMPLEMENT OR DELETE:** Both tests are incomplete stubs
  - **Action:** Either implement actual diagnostics tests or remove file entirely

---

### EXTRACT - Create Shared Helpers

#### Create: `tests/test_helpers.h` and `tests/test_helpers.c`

```c
// Grid initialization helpers
void TestGrid_Init(const char* map, int width, int height);
void TestGrid_Clear(void);  // ClearMovers, ClearItems, ClearStockpiles, etc.
void TestGrid_WithSubsystems(void);  // Init water, fire, temp, etc.
void TestGrid_Weather(int width, int height);  // Consistent weather test grid
void TestGrid_Basic(void);  // Flat dirt floor + air above

// Simulation helpers
void RunSimulation(int ticks);
void RunSimulationUntil(bool (*condition)(void), int maxTicks);
void RunWaterTicks(int n);
void RunFireTicks(int n);
void RunTempTicks(int n);

// Common assertions
bool AssertWalkablePath(int* path, int pathLength);
bool AssertNoWaterLeaks(void);  // Conservation law check
bool AssertTemperatureBounded(void);  // Within min/max
```

#### Files to update with new helpers:
- test_pathfinding.c - Use `TestGrid_Init()` (remove 20+ lines per test)
- test_mover.c - Use `TestGrid_Clear()` (remove 15+ lines per test)
- test_jobs.c - Use `TestGrid_Clear()` (remove 10+ lines × 30 tests = 300 lines)
- test_water.c - Use `RunWaterTicks()` already exists, but consolidate
- test_fire.c - Use `RunFireTicks()` already exists, consolidate
- test_temperature.c - Use `RunTempTicks()` already exists, consolidate
- test_weather.c - Use `TestGrid_Weather()` (remove `SetupWeatherGrid()` duplication)
- test_trees.c - Use `TestGrid_Basic()` (remove `SetupBasicGrid()`)
- test_groundwear.c - Use `TestGrid_Init()`
- test_floordirt.c - Use `TestGrid_Init()`

**Estimated reduction: ~500 lines of boilerplate across all test files**

---

## Summary of Changes

| Action | Count | Impact |
|--------|-------|--------|
| **DELETE** | ~80-100 tests | Remove 30-40% of pointless tests |
| **CONSOLIDATE** | ~7 tests | Reduce duplication |
| **FIX** | ~5 tests | Improve weak assertions |
| **EXTRACT** | ~500 lines | Shared helpers in test_helpers.h |
| **IMPLEMENT/DELETE** | 1 file | Fix test_workshop_diagnostics.c |

**Total LOC reduction: ~1000-1500 lines** (deletions + consolidations + helper extraction)

---

## Conclusion

The test suite has a **strong foundation** with excellent integration tests, but is **weighed down by 30-40% pointless tests**. Removing initialization-only tests, getter/setter mirrors, and tautological math tests would:

1. **Reduce noise** - Make it easier to find real tests
2. **Speed up test runs** - Fewer tests to execute
3. **Improve confidence** - Only tests that can actually catch bugs remain
4. **Clarify intent** - What's left clearly describes expected behavior

The **well-designed tests** (integration tests, state machine tests, multi-system interactions) should be **preserved and documented as examples** for future test writing.

---

## Next Steps

1. **Phase 1:** Delete all high-priority tests listed above (~80-100 deletions)
2. **Phase 2:** Extract shared helpers into `test_helpers.h/c` (~500 line reduction)
3. **Phase 3:** Consolidate duplicate tests (~7 consolidations)
4. **Phase 4:** Fix weak assertions in 5 tests
5. **Phase 5:** Implement or delete test_workshop_diagnostics.c

**After cleanup:** Test suite will be ~1000-1500 lines shorter, with only meaningful tests remaining.

