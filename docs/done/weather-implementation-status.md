# Weather & Seasons Implementation Status

Last updated: 2026-02-13 (Phase 5 complete - ALL PHASES DONE ✅)

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

### Phase 3: Wind Effects
- **test_wind.c**: 15 tests, all passing (5 dot product, 2 smoke bias, 2 fire spread, 2 wind drying, 4 wind chill)
- **smoke.c**: Wind bias in `SmokeTrySpread()` — Fisher-Yates shuffle followed by wind-based sort
- **steam.c**: Wind bias in `SteamTrySpread()` — same pattern as smoke
- **fire.c**: Wind bias in `FireTrySpread()` — neighbor sorting + spread % modifier (+15% downwind, -10% upwind)
- **groundwear.c**: Wind drying — extra wetness reduction on exposed cells with `windStrength > 0.5`
- **saveload.c**: Added `windDryingMultiplier` to SETTINGS_TABLE
- **Makefile**: Added `test_wind` target, integrated into main `test` suite

Key implementation: Wind bias affects neighbor order (downwind neighbors checked first) when `windStrength > 0.5`. Fire gets additional spread % bonus/penalty based on `GetWindDotProduct()`. Save version still v44 (no new grids, just one new tunable).

Test fix note: Smoke tests must use z=1 (above ground) since ground level (z=0) contains solid floor that blocks fluids. Fire test uses low fire level (2) and few ticks (5) so wind bonus is statistically significant.

### Phase 4: Snow + Cloud Shadows
- **test_snow.c**: 19 tests, all passing (7 test suites: grid basics, accumulation, melting, movement, fire interaction, cloud shadows)
- **weather.h**: Snow functions (`InitSnow`, `GetSnowLevel`, `SetSnowLevel`, `UpdateSnow`, `GetSnowSpeedMultiplier`), cloud shadow function (`GetCloudShadow`), snow tunables
- **weather.c**: Separate `snowGrid` (uint8_t 3D array, 4 levels: 0=none, 1=light, 2=moderate, 3=heavy), per-cell accumulation timers for deterministic behavior, snow accumulation during WEATHER_SNOW below freezing (checks `IsExposedToSky`), melting above freezing (increases wetness → can create mud), cloud shadow noise function with wind-based scrolling
- **fire.c**: Snow extinguishing — moderate+ snow (level >= 2) instantly extinguishes fire
- **rendering.c**: `DrawSnow()` — white overlay with alpha based on snow level (60/120/180), `DrawCloudShadows()` — black overlay darkening based on weather type
- **main.c**: Rendering calls added (DrawCloudShadows before mud, DrawSnow after mud)
- **mover.c**: `UpdateSnow()` call added after `UpdateWeather()` in main sim loop
- **saveload.c**: snowGrid save/load with v45 backward compatibility, `snowAccumulationRate` and `snowMeltingRate` in SETTINGS_TABLE
- **save_migrations.h**: v45
- **Makefile**: Added `test_snow` target

Key implementation: Snow uses deterministic per-cell accumulators (not probabilistic). Accumulation rate: 0.1 = 10 seconds per level at full intensity. Melting rate: 0.05 = 20 seconds per level. Snow extinguishes fire at level 2+. Cloud shadows use multi-scale noise (0.05 + 0.1 scales) scrolling with wind vector, max 80 alpha darkening.

Test patterns: 19 tests total (grid ops, accumulation with 100+ ticks, melting with 250-700 ticks, movement multipliers, fire with fuel, cloud shadows). All tests use proper grid setup (CELL_WALL + MAT_DIRT + wallNatural for natural soil). Fire tests require `fireGrid[z][y][x].fuel = 100` to prevent natural decay.

### Phase 5: Thunderstorms + Mist
- **test_thunderstorm.c**: 12 tests, all passing (5 lightning strike tests, 2 flash tests, 4 mist tests, 1 full year cycle)
- **weather.h**: Lightning API (`UpdateLightning`, `GetLightningFlashIntensity`, `TriggerLightningFlash`, `ResetLightningTimer`, `SetLightningInterval`), mist API (`GetMistIntensity`), tunable `lightningInterval`
- **weather.c**: Lightning strike system — scans for exposed flammable cells (wood floors/walls), random candidate selection, ignites via `SetFireLevel`, triggers white flash. Flash decays at 5x speed. Mist intensity based on weather type + time of day (higher at dawn/dusk)
- **mover.c**: `UpdateLightning(gameDeltaTime)` call after `UpdateSnow()` in main sim loop
- **rendering.c**: `DrawLightningFlash()` — full-screen white overlay scaled by flash intensity (max 180 alpha). `DrawMist()` — distance-based fog effect, grey-white overlay increasing with distance from camera center (max at ~15 cells)
- **main.c**: `DrawMist()` called after `DrawJobLines()` (before UI), `DrawLightningFlash()` called after cutscenes (last visual effect)
- **saveload.c**: Added `lightningInterval` to SETTINGS_TABLE
- **Makefile**: Added `test_thunderstorm` target, integrated into main `test` suite

Key implementation: Lightning only strikes during WEATHER_THUNDERSTORM weather. Scans all exposed cells (via `IsExposedToSky`), checks for flammable materials (wood types), builds candidate list, picks random target. Flash timer set to 1.0 on strike, decays first in `UpdateLightning` (before potential new strike), prevents instant decay. Mist uses `GetMistIntensity()` which returns 0.9 for WEATHER_MIST, 0.2-0.3 for rain/thunderstorm, 0.0 for clear, modulated by time of day (1.5x at dawn/dusk).

Test patterns: Lightning tests use 16x16 grid with wooden floors at z=1 (exposed). Sheltered area created with stone roof to verify strike exclusion. Flash test checks intensity after `UpdateLightning(1.0f)` with interval 0.5s. Mist tests verify intensity by weather type and time modulation. Full year test cycles through 28 days verifying seasonal weather restrictions (snow only winter, thunderstorms only summer).

## All Phases Complete! 🎉

**Total test coverage**: 120+ tests across 5 test files (test_mud, test_seasons, test_weather, test_wind, test_snow, test_thunderstorm)
**Save version**: v45 (includes full weather/seasons system + snow grid + lightning tunables)
**All 24 test suites passing**: Full integration with existing systems (fire, water, temperature, smoke, steam, vegetation)

## File Change Summary

| File | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 |
|------|---------|---------|---------|---------|---------|
| src/simulation/weather.h | Created | Rewritten | Wind API added | Snow/cloud API added | Lightning + mist API |
| src/simulation/weather.c | Created | Rewritten | Wind funcs added | Snow grid + cloud shadows | Lightning strikes + mist |
| src/simulation/smoke.c | — | — | Wind bias added | — | — |
| src/simulation/steam.c | — | — | Wind bias added | — | — |
| src/simulation/fire.c | — | — | Wind bias added | Snow extinguish added | — |
| src/simulation/groundwear.c | Modified | — | Wind drying added | — | — |
| src/simulation/temperature.c | Modified | — | — | — | — |
| src/entities/mover.c | — | Modified | — | UpdateSnow() call | UpdateLightning() call |
| src/render/rendering.c | — | — | — | DrawSnow + DrawCloudShadows | DrawLightningFlash + DrawMist |
| src/render/ui_panels.c | Modified | Modified | — | — | — |
| src/main.c | — | — | — | Render calls added | Mist + flash render calls |
| src/core/saveload.c | Modified | Modified | windDryingMultiplier | Snow grid + tunables, v45 | lightningInterval |
| src/core/save_migrations.h | v43 | v44 | — | v45 | — |
| src/unity.c | Modified | — | — | — | — |
| tests/test_unity.c | Modified | — | — | — | — |
| tests/test_seasons.c | Created | — | — | — | — |
| tests/test_weather.c | — | Created | — | — | — |
| tests/test_wind.c | — | — | Created | — | — |
| tests/test_snow.c | — | — | — | Created | — |
| tests/test_thunderstorm.c | — | — | — | — | Created |
| Makefile | Modified | Modified | test_wind added | test_snow added | test_thunderstorm added |

## Key Patterns to Remember
- **Backward compat**: `seasonalAmplitude=0` → flat temp, `weatherEnabled=false` → no weather ticks, `windStrength=0` → no wind bias, old saves (< v45) initialize snowGrid to zero
- **Test setup**: `SetupWindGrid()` calls `InitWeather()` which enables weather; tests that don't call it get `weatherEnabled=false`
- **Fisher-Yates wind bias**: After shuffle, sort `order[]` by descending `GetWindDotProduct(dx[dir], dy[dir])` — only when `windStrength > 0.5`
- **Statistical tests**: Run 50-200 trials with seeded RNG, compare directional counts
- **Save version**: Bump when adding new grids or changing struct layouts (v43=seasons, v44=weather state, v45=snow grid)
- **Snow accumulation**: Deterministic per-cell timers, not probabilistic — ensures tests are reproducible
- **Fire + snow tests**: Must set `fireGrid[z][y][x].fuel = 100` to prevent natural decay during testing
