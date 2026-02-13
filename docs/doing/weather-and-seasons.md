# Weather, Seasons & Wind

Date: 2026-02-13
Status: In Progress (Phase 0 complete, implementing Phase 1)

## Overview

A weather/seasons system that ties together existing simulations (temperature, water, fire, smoke, steam, vegetation, lighting) through a **weather state machine** and a **global wind vector**. Seasons modulate weather probabilities and ambient temperature. Weather affects the world through existing mechanisms — rain increases cell wetness, wetness suppresses fire, wind biases smoke/steam drift, etc.

**Key principle:** effects are emergent, not hardcoded per-season. Fire doesn't spread less "because it's winter" — fire spreads less because it's been raining and cells are wet. The season drives the weather, the weather drives the cell state, the cell state drives the simulation.

Flow fields (per-cell vector grids for wind and water) are a natural extension but are **deferred to a later phase** — see `docs/todo/gameplay/flow-simulation.md`. The early phases use a single global wind vector for the whole map.

---

## 1. Mud (Prerequisite)

Mud is a significant gameplay gap (dirt + water exist but don't interact). It should be implemented before or alongside weather, since rain's primary terrain effect is creating mud.

### Concept

Dirt cells with high wetness become mud. Mud is not a new cell type — it's an emergent state from existing systems.

```
Rain → increases CELL_WETNESS on dirt cells → wetness 2+ on dirt = mud
Mud = movement penalty + visual overlay + tracked indoors by movers
Drying = wetness decreases over time when rain stops → mud reverts to dirt
```

### Implementation

- Use existing `GET_CELL_WETNESS` / `SET_CELL_WETNESS` (2-bit: dry/damp/wet/soaked, defined in grid.h but currently unused).
- A dirt-type cell (CELL_DIRT, CELL_CLAY, CELL_PEAT) with wetness >= 2 (wet) counts as mud.
- `IsMuddy(x,y,z)` = `IsGroundCell(type) && GET_CELL_WETNESS(x,y,z) >= 2`
- Movement cost: mud cells have higher pathfinding cost (pathfinding.c already anticipates variable costs).
- Visual: brown/dark overlay sprite on muddy cells.
- Dirt tracking: movers walking from mud onto floors track more dirt (existing floor-dirt system, `DIRT_MULTIPLIER_WET`).
- Drying: wetness decreases by 1 every N game-seconds on non-submerged cells. Faster in sun/wind, slower in shade.
- Future: mud + straw = cob (building material), mud bricks.

---

## 2. Seasons

### Time Model

The existing time system tracks `dayNumber` and `timeOfDay` (0-24h). Seasons layer on top:

```c
#define DAYS_PER_SEASON  7      // Configurable via UI slider
#define SEASONS_PER_YEAR 4
#define DAYS_PER_YEAR    (DAYS_PER_SEASON * SEASONS_PER_YEAR)  // 28

typedef enum {
    SEASON_SPRING = 0,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT
} Season;
```

Derived from `dayNumber`:
```c
int yearDay    = (dayNumber - 1) % DAYS_PER_YEAR;        // 0-27
Season season  = (Season)(yearDay / DAYS_PER_SEASON);     // 0-3
float seasonProgress = (float)(yearDay % DAYS_PER_SEASON) / DAYS_PER_SEASON; // 0.0-1.0
```

Start day: day 1 = first day of spring (fresh start for the colony).

`daysPerSeason` is configurable (UI slider in Time section). Shorter = faster cycling for testing, longer = more strategic gameplay.

### Ambient Temperature Curve

`GetAmbientTemperature(z)` currently returns a flat `ambientSurfaceTemp`. Add a seasonal sine curve:

```c
float yearPhase = (float)yearDay / DAYS_PER_YEAR;           // 0.0 to 1.0
float seasonalOffset = sinf((yearPhase - 0.25f) * 2 * PI);  // peak at summer midpoint
// -0.25 shift so spring=rising, summer=peak, autumn=falling, winter=trough
int surfaceTemp = baseSurfaceTemp + (int)(seasonalOffset * seasonalAmplitude);
```

With `baseSurfaceTemp = 15` and `seasonalAmplitude = 20`:
- Winter trough: -5 C
- Summer peak: +35 C
- Spring/autumn: ~15 C

Underground remains stable (existing `ambientDepthDecay` handles this).

### Season Effects (All Emergent)

Seasons don't directly modify simulation parameters. Instead:

| Season | Weather tendency | Emergent effects |
|--------|-----------------|------------------|
| **Spring** | Frequent rain, rising temp | Snow melts (water), mud everywhere, vegetation grows fast |
| **Summer** | Mostly clear, hot, rare thunderstorms | Cells dry out, fire spreads more easily (dry cells), long days |
| **Autumn** | Moderate rain, falling temp | Mud returns, vegetation slows, shorter days |
| **Winter** | Snow, cold, calm wind | Snow accumulates, everything frozen/dormant, short days |

Day length affects the lighting system — `GetSkyColorForTime()` already maps hour to color; shift dawn/dusk hours per season:
- Summer: dawn 5h, dusk 21h (16h daylight)
- Winter: dawn 8h, dusk 16h (8h daylight)
- Spring/Autumn: dawn 6h, dusk 18h (12h daylight)

---

## 3. Weather State Machine

### Weather Types

```c
typedef enum {
    WEATHER_CLEAR,          // Sun, no precipitation
    WEATHER_CLOUDY,         // Overcast, cloud shadows
    WEATHER_RAIN,           // Rain falling, increases cell wetness
    WEATHER_HEAVY_RAIN,     // Downpour, rapid wetness, puddle/mud formation
    WEATHER_THUNDERSTORM,   // Heavy rain + lightning strikes
    WEATHER_SNOW,           // Snow falling, accumulates, cold
    WEATHER_MIST,           // Fog, reduced visibility
    WEATHER_COUNT
} WeatherType;
```

### State Transitions

Weather changes happen on a timer (e.g., every 30-120 game-seconds, tunable). Each weather type has a transition probability table modulated by season:

```
CLEAR → CLOUDY (common), MIST (rare, spring/autumn)
CLOUDY → CLEAR (common), RAIN (moderate), SNOW (winter only)
RAIN → HEAVY_RAIN (moderate), CLOUDY (common), CLEAR (rare)
HEAVY_RAIN → THUNDERSTORM (summer only), RAIN (common)
THUNDERSTORM → HEAVY_RAIN (common), RAIN (common)
SNOW → CLOUDY (common), CLEAR (moderate)
MIST → CLEAR (common), CLOUDY (moderate)
```

Season modulates probabilities — e.g., SNOW transitions have weight 0 outside winter, THUNDERSTORM weight 0 outside summer.

### Weather State

```c
typedef struct {
    WeatherType current;
    WeatherType previous;       // For interpolated transitions
    float transitionTimer;      // Countdown to next weather change
    float transitionDuration;   // How long this weather lasts
    float intensity;            // 0.0-1.0, ramps up/down during transitions

    // Wind (single global vector for the whole map)
    float windDirX, windDirY;   // Normalized direction
    float windStrength;         // 0=calm, 1=breeze, 2=wind, 3=strong, 4=storm
    float windChangeTimer;      // Wind shifts gradually
} WeatherState;
```

Wind direction and strength change with weather — storms bring strong wind, clear weather has calm or light breeze. Wind direction shifts slowly over time (random walk with momentum).

### Wind

Wind is a **single global vector** (direction + strength), not a per-cell grid. This is simple and sufficient for biasing smoke/steam drift, fire spread, and wind chill.

**Wind shadow** is computed per-cell but cached:
- Outdoor cells: full wind exposure.
- Indoor cells (enclosed by walls): no wind.
- Cells behind walls relative to wind direction: reduced wind. Computed by ray-marching a few cells from wind direction; cached and only recalculated when wind direction changes significantly (e.g., > 45 degrees).
- Wind shadow affects: wind chill on movers, smoke/steam drift strength, drying speed.

**Wind integration points:**

| System | How wind affects it |
|--------|--------------------|
| **Smoke** (`SmokeTrySpread`) | Bias neighbor order by global wind direction |
| **Steam** (`SteamTrySpread`) | Same as smoke |
| **Fire** (`FireTrySpread`) | Increase spread chance downwind, decrease upwind |
| **Temperature** | Wind chill: exposed outdoor cells feel colder for movers |
| **Drying** | Higher wind = faster wetness decrease on exposed cells |

For smoke/steam: simply sort the 4 neighbors by dot product with global wind vector before the existing Fisher-Yates shuffle. When `windStrength == 0`, keep existing random behavior.

---

## 4. Weather Effects

### Rain

- Increases `CELL_WETNESS` on exposed surface cells (no roof above).
- Light rain: slow wetness increase (dry → damp over ~30s).
- Heavy rain: fast wetness increase, cells can reach soaked, puddles form.
- Wet dirt = mud (see section 1).
- Existing wetness flags: `GET_CELL_WETNESS` / `SET_CELL_WETNESS` (2-bit: dry/damp/wet/soaked) — defined in grid.h, currently unused.
- Roof detection: trace upward from cell; if any solid cell above on same XY, cell is sheltered.
- Fire interaction: wet/soaked cells are harder to ignite (check wetness in `FireTrySpread`). This is why fire "spreads less in rainy seasons" — emergent, not hardcoded.

### Rain on Items + Stockpile Sheltering

Items on exposed cells (no roof) get rained on. This ties into the seasoning/curing system (`docs/todo/gameplay/seasoning-curing.md`):

- Add an `exposed` flag (or compute from position) so items know if they're under a roof.
- Stockpile environment tags from the curing doc: `Dry / Covered / Wet / Open`.
- Open stockpiles in rain: wood doesn't season properly, food spoils faster, metal rusts.
- Covered stockpiles: sheltered from rain, proper curing conditions.
- This creates gameplay incentive to build roofs over stockpiles and plan storage areas.

For now: track `isExposed` per item based on roof detection. Actual degradation effects can come with the seasoning/curing system.

### Snow

- Winter-only precipitation (when temperature < 0).
- Snow tracked as a **2-bit cell flag** in cellFlags (none/light/moderate/heavy), same pattern as wetness.
- Light snow: visual overlay, slight movement penalty.
- Moderate/heavy snow: significant movement penalty, needs clearing (JOBTYPE_CLEAR_SNOW).
- Snow + warm temperature > 0 → melt → increases wetness (which can create mud on dirt).
- Snow on fire → extinguishes.

```c
// Snow flags (2-bit, in cellFlags alongside wetness)
#define CELL_SNOW_MASK   (3 << SNOW_SHIFT)
#define CELL_SNOW_SHIFT  // next available bit position
#define GET_CELL_SNOW(x,y,z)    ((cellFlags[z][y][x] & CELL_SNOW_MASK) >> CELL_SNOW_SHIFT)
#define SET_CELL_SNOW(x,y,z,s)  (cellFlags[z][y][x] = (cellFlags[z][y][x] & ~CELL_SNOW_MASK) | ((s) << CELL_SNOW_SHIFT))
```

### Cloud Shadows

- During CLOUDY/RAIN/HEAVY_RAIN: darken patches of the surface.
- Implementation: a scrolling noise value sampled per cell, applied as a color darkening.
- Moves with wind direction. Speed proportional to wind strength.
- Cloud density varies with weather type (CLOUDY = patchy shadows, RAIN = mostly dark, CLEAR = none).
- Integrates with existing `GetSkyColorForTime` — overall ambient dimmer during cloud cover.

### Mist/Fog

- Reduced visibility: darken or fade distant cells with a translucent overlay color.
- Common in spring mornings, autumn evenings, near water bodies.
- Simple implementation: tint cells based on distance from camera center, heavier tint = further away. Or: ground-level translucent color overlay on low cells near water.

### Thunderstorms

- Heavy rain + lightning strikes.
- Lightning: random exposed surface cell struck every N seconds.
  - Ignites flammable cells (starts fire via existing fire system).
  - Flash: brief full-brightness lighting pulse (1-2 frames, set sky color to white momentarily).
  - Sound: thunder rumble (stretch goal).
- Rare event — only during summer heavy rain.

### Wind Shadow Behind Walls

Walls cast a wind shadow in the downwind direction. Cells in the shadow have reduced wind exposure.

Computation (cached, rebuild when wind shifts > 45 degrees):
- For each wall cell on the surface z-level, mark N cells downwind as "sheltered" (reduced wind strength).
- Shadow length: 3-5 cells (tunable). Strength tapers linearly.
- Indoor cells (fully enclosed) always have zero wind.
- Used for: wind chill reduction, slower drying, reduced smoke/steam drift.
- Debug overlay: show wind exposure per cell (color gradient, toggled in Debug section).

---

## 5. Mover/Gameplay Integration

### Movement

| Condition | Effect |
|-----------|--------|
| Mud (wetness >= 2 on dirt) | Movement speed penalty, higher path cost |
| Rain | Slight work speed penalty outdoors (-10%) |
| Heavy rain | Larger work penalty outdoors (-25%) |
| Snow (light) | Movement speed -15% |
| Snow (heavy) | Movement speed -40%, pathfinding cost increase |
| Wind chill | Shifts effective temperature colder for exposed movers |

### Jobs

| Job | Trigger |
|-----|---------|
| Clear snow | Snow level > threshold on designated paths |
| Chop firewood | Seasonal — stockpile fuel before winter |

### Vegetation

- Growth rate modulated by season: spring fast, summer normal, autumn slow, winter dormant.
- Rain increases growth slightly (wetness on grass cells).
- Snow suppresses growth entirely.
- Existing `vegetationGrid` already tracks VEG_NONE through VEG_GRASS_TALLER — seasonal modulation just scales the growth tick rate in groundwear.c.

---

## 6. Rendering

Keep rendering simple — cell-level color overlays, not particles. Can be improved later.

### Rain

- Occasional blue-tinted cells on exposed surface tiles during rain. Density scales with intensity.
- Or: semi-transparent blue tint applied to exposed cells (simple DrawRectangle overlay per cell).
- Direction: no diagonal rain sprites for now, just tinted cells.

### Snow

- White overlay color on cells with snow. Opacity scales with snow level (light = faint, heavy = solid white).
- Ground cells: white tint/overlay.

### Cloud Shadows

- Per-cell darkening based on scrolling noise. Darken cell color by 20-40%.
- Noise scrolls with wind direction.

### Mist/Fog

- Fade/tint distant cells toward a grey-white color.
- Intensity based on distance from camera center.

### UI

- Current weather name + season in top bar (next to day/time display).
- Wind direction arrow + strength text (optional, in debug panel or top bar).
- Temperature shown alongside (already exists).

---

## 7. Implementation Phases

### Phase 0: Mud
- Activate existing `CELL_WETNESS` macros.
- Add `IsMuddy()` helper (ground cell + wetness >= 2).
- Mud visual overlay (brown tint on dirt cells).
- Movement penalty for muddy cells (pathfinding cost).
- Wetness drying over time (decrease by 1 every N game-seconds).
- Dirt tracking: movers on mud track more dirt indoors.
- **No weather yet** — can test by manually setting cell wetness or using water.

### Phase 1: Seasons + Temperature Curve
- Add season calculation to time system (`yearDay`, `season`, `seasonProgress`).
- Add configurable `daysPerSeason` (UI slider in Time section, default 7).
- Modulate `GetAmbientTemperature` with seasonal sine curve.
- Display season name in UI.
- Modulate day length by season (dawn/dusk hours).
- Seasonal vegetation growth rate modifier.

### Phase 2: Weather State Machine + Rain
- Add `WeatherState` struct to game state.
- Implement transition logic (timer-based, season-modulated probabilities).
- Add global wind direction + strength to weather state.
- Rain writes to `CELL_WETNESS` on exposed cells → creates mud on dirt (phase 0).
- Roof detection (trace upward for solid cell).
- Rain visual: blue-tinted cells on exposed surfaces.
- Display current weather in UI.
- Save/load weather state.
- Item exposure tracking (`isExposed` based on roof detection).

### Phase 3: Wind Effects + Wind Shadow
- Bias `SmokeTrySpread` and `SteamTrySpread` neighbor order by global wind direction.
- Bias `FireTrySpread` spread chance by wind alignment.
- Compute and cache wind shadow map (cells sheltered behind walls).
- Wind chill modifier for mover temperature comfort.
- Wind affects drying speed (exposed + windy = faster dry).
- Debug overlay: wind exposure per cell.

### Phase 4: Snow + Cloud Shadows
- Add 2-bit snow flags to cellFlags.
- Snow accumulation during WEATHER_SNOW on exposed cells.
- Snow melt when temperature > 0 (increases wetness → can create mud).
- Snow movement penalty + pathfinding cost.
- Snow clearing job.
- Snow visual: white cell overlay.
- Cloud shadow overlay (scrolling noise-based darkening).

### Phase 5: Thunderstorms + Mist
- Lightning strikes during thunderstorms (random exposed cell, ignite via fire system).
- Lightning flash visual (brief white sky pulse).
- Mist/fog rendering (distance-based cell fading).

### Phase 6 (Later, Separate Feature): Flow Fields
- Per-cell wind flow field for fine-grained wind simulation.
- Per-cell water flow field for rivers and drainage.
- See `docs/todo/gameplay/flow-simulation.md` for full design.
- Replaces the single global wind vector with per-cell variation.
- Enables directional water flow (rivers).

---

## 8. Data Flow Summary

```
dayNumber
    ↓
yearDay → season → weather transition probabilities
    ↓                   ↓
daysPerSeason      WeatherState
(configurable)      ├── current weather type
                    ├── global wind direction + strength
                    │       ├── bias smoke/steam drift
                    │       ├── bias fire spread
                    │       ├── wind chill on movers
                    │       └── wind shadow (cached per-cell)
                    │
                    ├── rain → cell wetness → mud (on dirt)
                    │                       → fire suppression (on all cells)
                    │                       → item exposure
                    ├── snow → 2-bit snow flags
                    ├── clouds → shadow overlay (cell darkening)
                    └── lightning → fire ignition

season → ambient temperature curve
       → vegetation growth rate
       → day length (dawn/dusk hours)
```

---

## 9. Files to Create/Modify

### New Files
- `src/simulation/weather.h` — WeatherType, WeatherState, Season enums + API
- `src/simulation/weather.c` — state machine, transitions, wind updates, wind shadow cache

### Modified Files
- `src/core/time.h/.c` — add season calculation helpers, configurable daysPerSeason
- `src/simulation/temperature.c` — seasonal ambient curve in GetAmbientTemperature
- `src/simulation/smoke.c` — bias SmokeTrySpread by global wind
- `src/simulation/steam.c` — bias SteamTrySpread by global wind
- `src/simulation/fire.c` — check cell wetness in FireTrySpread; bias spread by wind
- `src/simulation/groundwear.c` — wetness drying, seasonal vegetation growth rate
- `src/world/grid.h` — add snow flag macros (2-bit), IsMuddy helper
- `src/render/rendering.c` — rain/snow/cloud/mist cell overlays
- `src/render/ui_panels.c` — weather + season display, debug toggles, daysPerSeason slider
- `src/core/saveload.c` — weather state, snow flags (already in cellFlags)
- `src/core/inspect.c` — weather state display
- `src/core/sim_manager.c` — tick weather state machine
- `src/entities/mover.c` — movement cost for mud/snow, wind chill

---

## 10. Resolved Questions

- **Snow storage**: 2-bit cell flag (none/light/moderate/heavy). Enough granularity for gameplay; melting steps down one level at a time.
- **Wind shadow cost**: cached, recalculated only when wind direction changes > 45 degrees. Indoor = no wind. Outdoor walls cast shadow downwind.
- **Rain on items**: track `isExposed` per item. Actual degradation ties into seasoning/curing system. Covered stockpiles protect items. Open stockpiles = risk.
- **Frozen water**: out of scope for this doc. Should be walkable + slippery eventually, but that's a separate water-system feature.
- **Season length**: configurable via `daysPerSeason` UI slider (default 7).
- **Mud**: prerequisite — implemented in Phase 0 before weather. Dirt + wetness >= 2 = mud. Movement penalty, visual overlay, dirt tracking.
- **Fire + seasons**: no direct season→fire link. Rain→wetness→fire suppression is the correct causal chain. Dry summer cells burn more because they haven't been rained on, not because of a season flag.

## Open Questions

- **Mud as building material**: mud + straw = cob, mud bricks. How does this interact with the seasoning/curing system? Probably a separate feature that builds on both.
- **Stockpile environment tags**: the curing doc proposes `Dry/Covered/Wet/Open` per stockpile. Weather provides the `isExposed` signal; stockpile tags are part of the curing feature. Should they share the same roof detection?
- **Wind shadow resolution**: how many cells of shadow behind a wall? 3-5 seems reasonable. Linear taper? Should multi-story walls cast longer shadows?

---

## References

- `docs/todo/gameplay/flow-simulation.md` — per-cell flow field design (Phase 6, later)
- `docs/todo/gameplay/seasoning-curing.md` — item conditioning, stockpile environment tags
- `docs/vision/entropy-and-simulation/entropy.md` — seasons, temperature, wind, fire vision
- `docs/todo/medium-features-backlog.md` section 7 — seasonal ambient temperature curve
- `docs/blind-spots-and-fresh-todos.md` — gaps: no rain/weather, no wind, no snow, no mud

---

## 11. TDD Implementation Plan

Strict TDD: tests written FIRST per phase, then implementation to make them pass. Each phase is a self-contained chunk.

**Updated design decision**: Snow uses separate `snowGrid` (uint8_t 3D array, like `vegetationGrid`) instead of cellFlags -- only 1 bit free in cellFlags, need 2 bits for snow levels 0-3.

### Phase 0: Mud -- COMPLETE

All implemented: CELL_WETNESS macros, IsMuddy(), IsSoilMaterial(), DrawMud(), mudSpeedMultiplier, water-to-wetness sync, drying, 3x dirt tracking, test_mud.c.

### Phase 1: Seasons + Temperature Curve

**Tests first** (`tests/test_seasons.c`):

| describe | test cases |
|----------|-----------|
| `season_calculation` | SPRING on day 1, SUMMER at day 8, AUTUMN at 15, WINTER at 22, wrap to SPRING at 29, custom daysPerSeason |
| `year_day_calculation` | 0 on day 1, last day of year, wrap across years |
| `season_progress` | 0.0 at season start, ~0.5 midpoint, ~1.0 end |
| `seasonal_temperature` | Warmest mid-summer, coldest mid-winter, base temp at equinoxes, amplitude=0 means flat, depth decay still applies |
| `seasonal_day_length` | Summer dawn 5h/dusk 21h, winter 8h/16h, spring/autumn interpolated |
| `season_names` | Correct strings for all 4 seasons |

**Implementation**:

| File | Change |
|------|--------|
| `src/simulation/weather.h` | **CREATE** - Season enum, `daysPerSeason` (7), `baseSurfaceTemp` (15), `seasonalAmplitude` (20), pure functions: `GetYearDay()`, `GetCurrentSeason()`, `GetSeasonProgress()`, `GetSeasonName()`, `GetSeasonalSurfaceTemp()`, `GetSeasonalDawn()`, `GetSeasonalDusk()`, `GetVegetationGrowthRate()` |
| `src/simulation/weather.c` | **CREATE** - Season functions. Sine curve: `baseSurfaceTemp + sinf((yearPhase - 0.25f) * 2*PI) * seasonalAmplitude` |
| `src/simulation/temperature.c` | **MODIFY** `GetAmbientTemperature()` - use `GetSeasonalSurfaceTemp()` instead of flat `ambientSurfaceTemp` |
| `src/simulation/groundwear.c` | **MODIFY** - multiply vegetation recovery by `GetVegetationGrowthRate()` |
| `src/render/ui_panels.c` | **MODIFY** Time section - show season name, sliders for `daysPerSeason`, `baseSurfaceTemp`, `seasonalAmplitude` |
| `src/core/saveload.c` | **MODIFY** SETTINGS_TABLE - add `daysPerSeason`, `baseSurfaceTemp`, `seasonalAmplitude` |
| `src/core/save_migrations.h` | Bump to v43 |
| `tests/test_unity.c` | Add `#include "../src/simulation/weather.c"` |
| `Makefile` | Add `test_seasons` target |

**Risk mitigation**: `InitWeather()` sets `seasonalAmplitude=0`, so existing tests get flat temp -- no breakage.

### Phase 2: Weather State Machine + Rain

**Tests first** (`tests/test_weather.c`):

| describe | test cases |
|----------|-----------|
| `weather_initialization` | Starts CLEAR, wind calm, positive timer |
| `weather_transitions` | Transitions after timer expires, not before, no SNOW outside winter, no THUNDERSTORM outside summer, tracks previous type |
| `weather_transition_probabilities` | Statistical: CLOUDY most common from CLEAR, RAIN favored from CLOUDY in spring (1000 transitions, seeded RNG) |
| `weather_intensity` | Ramps from 0 after transition, reaches 1.0 |
| `roof_detection` | Exposed with clear sky, sheltered under wall/floor, top z always exposed |
| `rain_wetness` | Increases on exposed dirt, not sheltered, creates mud during heavy rain, not on non-soil, faster during HEAVY_RAIN |
| `rain_water_spawning` | Spawns sky water during rain, stops when CLEAR, heavier in HEAVY_RAIN |
| `weather_wind_basics` | Low wind CLEAR, moderate RAIN, high THUNDERSTORM, gradual direction shift |
| `weather_display` | Correct name strings |
| `weather_save_restore` | WeatherState survives save/load cycle |

**Implementation**:

| File | Change |
|------|--------|
| `src/simulation/weather.h` | **ADD** WeatherType enum (7 types), WeatherState struct, tunables, `InitWeather()`, `UpdateWeather()`, `IsExposedToSky()`, `GetWeatherName()` |
| `src/simulation/weather.c` | **ADD** Transition tables (season-modulated), state machine, `IsExposedToSky()` upward trace, rain wetness on exposed soil, connect to `SpawnSkyWater()`/`StopRain()` |
| `src/entities/mover.c` | **MODIFY** `TickWithDt()` - add `UpdateWeather()` before `UpdateRain()` |
| `src/render/rendering.c` | **ADD** `DrawRain()` - blue overlay on exposed cells |
| `src/render/ui_panels.c` | **ADD** Weather section: name, wind, toggle, sliders, force-weather buttons |
| `src/core/saveload.c` | **MODIFY** SETTINGS_TABLE + WeatherState save/load |
| `src/core/save_migrations.h` | Bump to v44 |
| `Makefile` | Add `test_weather` target |

### Phase 3: Wind Effects

**Tests first** (`tests/test_wind.c`):

| describe | test cases |
|----------|-----------|
| `wind_dot_product` | Positive downwind, negative upwind, zero perpendicular, scales with strength |
| `wind_smoke_bias` | Statistical: smoke drifts downwind over N ticks, even spread with no wind, steam same |
| `wind_fire_spread` | Statistical: more ignitions downwind, no effect at windStrength=0 |
| `wind_drying` | Faster drying with strong wind on exposed cells, no acceleration sheltered |
| `wind_chill` | Lower effective temp with wind, no chill sheltered, increases with strength |

**Implementation**:

| File | Change |
|------|--------|
| `src/simulation/weather.h` | **ADD** `GetWindDotProduct()`, `GetWindExposure()`, `GetWindChillTemp()`, `windDryingMultiplier` |
| `src/simulation/smoke.c` | **MODIFY** `SmokeTrySpread()` - sort `order[]` by wind dot product after Fisher-Yates |
| `src/simulation/steam.c` | **MODIFY** `SteamTrySpread()` - same wind bias |
| `src/simulation/fire.c` | **MODIFY** `FireTrySpread()` - +15% downwind, -10% upwind |
| `src/simulation/groundwear.c` | **MODIFY** drying - multiply by wind exposure |
| `src/entities/mover.c` | **MODIFY** - wind chill on exposed tiles |
| `src/core/saveload.c` | **MODIFY** SETTINGS_TABLE - `windDryingMultiplier` |
| `Makefile` | Add `test_wind` target |

### Phase 4: Snow + Cloud Shadows

**Tests first** (`tests/test_snow.c`):

| describe | test cases |
|----------|-----------|
| `snow_grid_basics` | Init zero, set/get, clamp to 3, out-of-bounds safe |
| `snow_accumulation` | During WEATHER_SNOW on exposed cells, not sheltered, only below freezing |
| `snow_melting` | Above freezing, converts to wetness, creates mud on dirt, persists at/below freezing |
| `snow_movement` | Light 0.85x, heavy 0.6x, none 1.0x |
| `snow_fire_interaction` | Extinguishes fire on snowy cells |
| `snow_clearing` | Designatable, removes snow |
| `cloud_shadow_basics` | Intensity by weather type, varies by position+time, moves with wind |

**Implementation**:

| File | Change |
|------|--------|
| `src/simulation/weather.h` | **ADD** `snowGrid`, `GetSnowLevel/SetSnowLevel`, `InitSnow()`, `UpdateSnow()`, `GetSnowSpeedMultiplier()`, `GetCloudShadow()` |
| `src/simulation/weather.c` | **ADD** snowGrid, accumulation + melting, cloud shadow noise |
| `src/simulation/fire.c` | **MODIFY** - extinguish fire on snow >= 2 |
| `src/entities/mover.c` | **MODIFY** - snow speed penalty |
| `src/render/rendering.c` | **ADD** `DrawSnow()`, `DrawCloudShadows()` |
| `src/core/saveload.c` | **MODIFY** GRID section snowGrid + SETTINGS_TABLE |
| `src/core/save_migrations.h` | Bump to v45 |
| `Makefile` | Add `test_snow` target |

### Phase 5: Thunderstorms + Mist

**Tests first** (`tests/test_thunderstorm.c`):

| describe | test cases |
|----------|-----------|
| `lightning_strike` | Only THUNDERSTORM, only exposed, ignites flammable, not stone, configurable interval, deterministic |
| `lightning_flash` | Sets flash timer, decays |
| `mist_intensity` | High WEATHER_MIST, zero CLEAR, modulates with time of day |
| `weather_full_cycle` | Full year: multiple types, SNOW only winter, THUNDERSTORM only summer |

**Implementation**:

| File | Change |
|------|--------|
| `src/simulation/weather.h` | **ADD** `lightningInterval`, `GetLightningFlashIntensity()`, `GetMistIntensity()` |
| `src/simulation/weather.c` | **ADD** `UpdateLightning()`, `UpdateMist()` |
| `src/render/rendering.c` | **ADD** `DrawMist()`, lightning flash |
| `src/render/ui_panels.c` | **ADD** lightning/mist sliders |
| `src/core/saveload.c` | **MODIFY** SETTINGS_TABLE |
| `Makefile` | Add `test_thunderstorm` target |

### Verification Per Phase

1. `make clean && make test_<phase>` -- new tests pass
2. `make test` -- full suite, no regressions
3. `make path` -- visual verification in game

### Key Risk Mitigations

- `InitWeather()` sets `seasonalAmplitude=0` and `windStrength=0` -- existing tests unaffected
- Snow uses separate `snowGrid` (4MB, same as `vegetationGrid`) -- no cellFlags changes
- Save version bumps v43/v44/v45 -- strict matching, fine in dev
- `make clean` needed after Phase 1 (new .c in test_unity.c)
