# Weather & Seasons Implementation Status

Last updated: 2026-02-13

## Completed

### Phase 0: Mud
- Complete (was done before this work started)

### Phase 1: Seasons + Temperature Curve
- **test_seasons.c**: 34 tests, all passing
- **weather.h**: Season enum, season calc functions, seasonal modulation API
- **weather.c**: Sine-curve temperature, dawn/dusk, vegetation growth rate
- **temperature.c**: `GetAmbientTemperature()` uses `GetSeasonalSurfaceTemp()`
- **groundwear.c**: Vegetation recovery modulated by `GetVegetationGrowthRate()`
- **saveload.c**: `daysPerSeason`, `baseSurfaceTemp`, `seasonalAmplitude` in SETTINGS_TABLE
- **save_migrations.h**: v43
- **ui_panels.c**: Season name in time widget, season sliders in Time panel

Key design: `seasonalAmplitude=0` (default) returns `ambientSurfaceTemp` unchanged, `GetVegetationGrowthRate()` returns 1.0 — backward compatible with all existing tests.

### Phase 2: Weather State Machine + Rain
- **test_weather.c**: 31 tests, all passing
- **weather.h**: WeatherType enum (7 types), WeatherState struct, weather API
- **weather.c**: Transition probability tables (season-modulated), state machine, roof detection (`IsExposedToSky`), rain wetness on exposed soil, rain spawning (connects to existing `SpawnSkyWater`/`StopRain`), wind smoothing
- **mover.c**: `UpdateWeather()` call in `TickWithDt()` before `UpdateRain()`
- **saveload.c**: 6 weather tunables in SETTINGS_TABLE + WeatherState struct save/load + 2 accumulators
- **save_migrations.h**: v44
- **ui_panels.c**: Weather status display (type, intensity, wind, timer) + tunable sliders. Time widget is 2-line: "Spring Day 1" / "16:00 Clear"

Key design: `weatherEnabled=false` by default (set to `true` by `InitWeather()`). Tests that don't call `InitWeather()` are unaffected.

## In Progress

### Phase 3: Wind Effects

**Tests written** (tests/test_wind.c - NOT YET COMPILED):
- `wind_dot_product`: 5 tests (positive downwind, negative upwind, zero perpendicular, scales with strength, zero when no wind)
- `wind_smoke_bias`: 2 statistical tests (drift downwind, even spread with no wind)
- `wind_fire_spread`: 2 statistical tests (more ignitions downwind, equal with no wind)
- `wind_drying`: 2 tests (faster drying with wind, no acceleration on sheltered cells)
- `wind_chill`: 4 tests (lowers temp, no effect sheltered, scales with strength, zero with no wind)

**API added to weather.h** (already written):
- `GetWindDotProduct(dx, dy)` — dot product of (dx,dy) with wind vector * strength
- `GetWindChillTemp(baseTemp, windStrength, exposed)` — effective temp with wind chill
- `windDryingMultiplier` — tunable for wind drying rate

**API implemented in weather.c** (already written):
- `GetWindDotProduct()` — returns 0 when windStrength < 0.01
- `GetWindChillTemp()` — -2C per unit of wind strength when exposed

**Still TODO for Phase 3:**

1. **Add wind bias to smoke spread** (smoke.c)
   - `#include "weather.h"` — DONE
   - In `SmokeTrySpread()` (line ~225): After Fisher-Yates shuffle of `order[]`, sort by wind dot product so downwind neighbors come first
   - Pattern: compute `GetWindDotProduct(dx[i], dy[i])` for each neighbor, bubble sort `order[]` by descending dot product when `windStrength > 0.5`

2. **Add wind bias to steam spread** (steam.c)
   - `#include "weather.h"`
   - In `SteamTrySpread()` (line ~162): Same pattern as smoke

3. **Add wind bias to fire spread** (fire.c)
   - `#include "weather.h"`
   - In `FireTrySpread()` (line ~280): TWO modifications:
     a. Sort `order[]` by wind dot product (same as smoke)
     b. Modify `spreadPercent`: add `+15% * windDot` for downwind bonus, `-10% * windDot` for upwind penalty
   - Only apply to horizontal neighbors (not upward dz=1)

4. **Add wind drying to groundwear.c**
   - Already includes `weather.h`
   - In wetness drying section (line ~217-226): After `!waterPresent` check, multiply drying by wind factor
   - If `IsExposedToSky(x, y, z) && weatherState.windStrength > 0.5`: chance to dry extra unit
   - Pattern: `if (exposed && windStrength > 0.5) { extra dry chance }`

5. **Add `windDryingMultiplier` to saveload.c SETTINGS_TABLE**

6. **Add `test_wind` target to Makefile** (test_wind_SRC, rule, add to test: list, .PHONY)

7. **`make clean && make test_wind`** — fix any failures

8. **`make test`** — full regression check

9. **`make path`** — verify game builds

## Not Started

### Phase 4: Snow + Cloud Shadows
Per plan: separate `snowGrid` (uint8_t 3D), accumulation during WEATHER_SNOW below freezing, melting above freezing adds wetness, snow speed penalty, fire extinguish on snowy cells, cloud shadow noise function. Version bump to v45.

### Phase 5: Thunderstorms + Mist
Per plan: lightning strikes on exposed flammable cells during THUNDERSTORM, flash timer, mist intensity by weather type/time, distance-based grey overlay.

## File Change Summary

| File | Phase 1 | Phase 2 | Phase 3 (remaining) |
|------|---------|---------|---------------------|
| src/simulation/weather.h | Created | Rewritten | Wind API added |
| src/simulation/weather.c | Created | Rewritten | Wind funcs added |
| src/simulation/smoke.c | — | — | Wind bias TODO |
| src/simulation/steam.c | — | — | Wind bias TODO |
| src/simulation/fire.c | — | — | Wind bias TODO |
| src/simulation/groundwear.c | Modified | — | Wind drying TODO |
| src/simulation/temperature.c | Modified | — | — |
| src/entities/mover.c | — | Modified | — |
| src/render/ui_panels.c | Modified | Modified | — |
| src/core/saveload.c | Modified | Modified | windDryingMultiplier TODO |
| src/core/save_migrations.h | v43 | v44 | — |
| src/unity.c | Modified | — | — |
| tests/test_unity.c | Modified | — | — |
| tests/test_seasons.c | Created | — | — |
| tests/test_weather.c | — | Created | — |
| tests/test_wind.c | — | — | Created (not compiled) |
| Makefile | Modified | Modified | test_wind target TODO |

## Key Patterns to Remember
- **Backward compat**: `seasonalAmplitude=0` → flat temp, `weatherEnabled=false` → no weather ticks, `windStrength=0` → no wind bias
- **Test setup**: `SetupWindGrid()` calls `InitWeather()` which enables weather; tests that don't call it get `weatherEnabled=false`
- **Fisher-Yates wind bias**: After shuffle, sort `order[]` by descending `GetWindDotProduct(dx[dir], dy[dir])` — only when `windStrength > 0.5`
- **Statistical tests**: Run 50-200 trials with seeded RNG, compare directional counts
- **Save version**: Don't bump for Phase 3 unless adding new grids (just adding `windDryingMultiplier` to existing SETTINGS_TABLE is fine at v44)
