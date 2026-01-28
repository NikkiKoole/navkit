Estimated: ~49 small getter functions across production code
Prioritized Test Fix Plan

---

## COMPLETED WORK

### Phase 1A: Production Getters ✅ DONE (22 functions)

Added `static inline` getter functions (zero overhead) to these headers:

**pathfinding.h** (8 getters):
- `GetEntranceX/Y/Z(idx)` - entrance coordinates
- `GetEntranceChunk1/Chunk2(idx)` - entrance chunk IDs
- `GetGraphEdgeFrom/To/Cost(idx)` - graph edge data

**mover.h** (5 getters/setters):
- `GetMoverPathLength/PathIndex(idx)` - path state
- `GetMoverNeedsRepath(idx)`, `SetMoverNeedsRepath(idx, val)` - repath flag
- `ClearMoverPath(idx)` - reset path state

**items.h** (6 getters):
- `IsItemActive(idx)` - active flag
- `GetItemX/Y/Z(idx)` - item position
- `GetItemType(idx)` - item type enum
- `GetItemReservedBy(idx)` - reservation

**water.h** (2 getters):
- `IsWaterStable(x, y, z)` - stability flag
- `HasWaterPressure(x, y, z)` - pressure flag

**temperature.h** (1 getter):
- `IsTemperatureStable(x, y, z)` - stability flag

### Phase 1A-continued: Updated Tests to Use Getters ✅ DONE

All 5 test files updated to use new getters instead of direct array/struct access:
- `test_pathfinding.c` (~40 replacements)
- `test_mover.c` (~15 replacements)
- `test_jobs.c` (~60 replacements)
- `test_water.c` (~6 replacements)
- `test_temperature.c` (~5 replacements)

All tests pass (0 failures).

---

Here's your actionable roadmap, organized by priority:

---

## PHASE 1: IMMEDIATE (Foundation Work)

### 1A. Add Missing Production APIs First
These are blocking - tests can't be fixed properly without them:

| System | APIs to Add | Why |
|--------|-------------|-----|
| **Pathfinding** | `GetCellType(x,y,z)`, `GetEntrance(idx)`, `GetEntranceCount()`, `GetChunkWidth()` | Tests directly access `grid[][]` and `entrances[]` |
| **Water** | `IsWaterEvaporationEnabled()`, `GetTotalWater()`, `GetWaterStabilizationTicks()` | Tests access globals directly |
| **Fire** | `IsCellBurned(x,y,z)`, `GetCellFuel(x,y,z)` | Tests use `HAS_CELL_FLAG` macro directly |
| **Temperature** | `GetAmbientSurfaceTemp()`, `IsTemperatureStable(x,y,z)`, `CelsiusToTempIndex()` | Tests read `ambientSurfaceTemp` directly |
| **Mover** | `IsMoverIdle(id)`, `GetMoverJob(id)`, `IsMoverActive(id)` | Tests have 8 helper functions that duplicate this |
| **Jobs** | `GetMoverJobState(id)`, `GetItemReservation(itemId)` | 30+ helper functions in test file |
| **Groundwear** | `GetTrampleAmount()`, `GetWearMax()`, `GetGrassToDirtThreshold()` | Tests modify constants directly |

**Estimated: ~49 small getter functions across production code**

### 1B. Define Constants for Magic Numbers
Create a `simulation_constants.h` or similar:

```c
#define WATER_STABILITY_TICKS 500
#define FIRE_SPREAD_TEST_TICKS 100
#define TEMPERATURE_SETTLE_TICKS 200
#define HAUL_COMPLETE_TICKS 500
// etc.
```

---

## PHASE 2: DELETE/REWRITE (19 tests)

These tests are fundamentally broken - either testing implementation details or meaningless:

### DELETE ENTIRELY (can't be salvaged):
| File | Test | Line | Reason |
|------|------|------|--------|
| `test_pathfinding.c` | `dijkstra_vs_astar_consistency` | 693-771 | 160 lines of hardcoded random noise map data |
| `test_mover.c` | `string_pulling_narrow_gaps` | 1134-1204 | Comments say "expect FAILS... uncomment to verify bug" |
| `test_steam.c` | `heaters_evaporate_water` | 205-267 | 10,000 tick load test, not a unit test |

### REWRITE FROM SCRATCH (with proper requirements):
| File | Test | Issue | Rewrite As |
|------|------|-------|------------|
| `test_pathfinding.c:522-582` | `incremental_updates` | Only checks edge count, not correctness | "After wall added, path avoids wall" |
| `test_water.c:218-254` | `equalize in channel` | Has printf debug, tests algorithm | "Water reaches both ends of channel" |
| `test_water.c:325-395` | `pressure height limit` | Contradictory comments | Pick one behavior and test it |
| `test_fire.c:104-148` | `burn and consume fuel` | 500-tick loop waiting for fire to die | "After burn, cell has BURNED flag" |
| `test_fire.c:193-222` | `spread orthogonally` | Fragile random-seed dependent | "Fire at (5,5) spreads to (5,6), not (6,6)" |
| `test_temperature.c:214-256` | `circular pattern` | Checks 1 neighbor pair | "Heat reaches all adjacent cells equally" |
| `test_temperature.c:258-316` | `stone insulation` | Magic thresholds | "Inside temp > outside temp after N ticks" |
| `test_temperature.c:466-527` | `stability` | Uses uninitialized variable | "Cell marked stable when temp unchanged for X ticks" |
| `test_mover.c:30-71` | `fixed timestep determinism` | Tests against itself | Test against known golden positions |
| `test_mover.c:308-356` | `goal becomes wall` | Modifies globals | Use proper test fixtures |
| `test_jobs.c:1018-1070` | `filter change mid-haul` | 1000-tick emergent behavior | Break into smaller state tests |
| `test_groundwear.c:128-175` | `full cycle` | Changes 4 constants, simulation not test | Separate tests per conversion |
| `test_high_speed.c:101-152` | `fire bounded` | Sets fire constants in test | Use production constants |

---

## PHASE 3: FIX (40 tests)
These are mostly fine but need cleanup:

### Pattern A: Replace direct variable access with getters (17 tests)
```c
// BEFORE (bad)
waterEvaporationEnabled = true;
wearTrampleAmount = 5;
ambientSurfaceTemp = 20;

// AFTER (good)
// Use production getters, or if testing with different values:
SetWearTrampleAmount(5);  // setter for test override
```

### Pattern B: Replace magic numbers with constants (15 tests)
```c
// BEFORE (bad)
for(int i = 0; i < 500; i++) { UpdateWater(); }

// AFTER (good)
for(int i = 0; i < WATER_STABILITY_TICKS; i++) { UpdateWater(); }
```

### Pattern C: Use production helpers instead of test reimplementations (8 tests)
Especially in `test_jobs.c` - delete the 30 helper functions and use the new production APIs.

---

## PHASE 4: STRUCTURAL IMPROVEMENTS

### Create Test Fixtures
For tests that need different constants:
```c
void SetupHighWearTest() {
    SaveWearConstants();
    SetWearTrampleAmount(50);
    SetGrassToDirtThreshold(100);
}

void TeardownHighWearTest() {
    RestoreWearConstants();
}
```

### Document Simulation Contracts
Each simulation system needs a spec doc defining:
- What intervals mean (in game-time, not ticks)
- What thresholds trigger state changes
- What invariants must hold

`test_time_specs.c` is already doing this well - use it as the template.
SUMMARY: Work Order

| Phase | What | Effort | Impact |
|-------|------|--------|--------|
| **1A** | Add 49 production getters | Medium | Unlocks proper testing |
| **1B** | Define magic number constants | Small | Makes tests readable |
| **2** | Delete 3, rewrite 16 tests | Medium | Removes false confidence |
| **3** | Fix 40 tests | Medium | Makes tests maintainable |
| **4** | Fixtures + docs | Small | Prevents future issues |

---

## Good News

**80+ tests are already solid** - especially:
- `test_time_specs.c` - excellent spec-based testing
- `test_high_speed.c` - good black-box safety tests
- Most initialization and boundary tests

These can serve as templates for the rewrites.

Want me to start on any specific phase? I'd recommend starting with **Phase 1A** (the production APIs) since everything else depends on having proper getters in place.
