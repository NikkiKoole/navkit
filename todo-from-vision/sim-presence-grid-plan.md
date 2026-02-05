# Plan: Simulation Presence Grid Optimization

Based on `docs/doing/sim-presence-grid.md` and current code state.

## Goals
- Keep the **active cell counters + early exits** (big win with zero overhead).
- Keep **per-cell presence flags disabled** by default (`USE_PRESENCE_GRID=0`).
- Ensure counters are accurate across all sim systems and save/load.
- Add lightweight instrumentation + tests to avoid regressions.

## Current State (Observed)
- `sim_presence.h/.c` exists, with `USE_PRESENCE_GRID` defaulting to `0`.
- Counters (`waterActiveCells`, `steamActiveCells`, etc.) are updated regardless of `USE_PRESENCE_GRID`.
- `UpdateWater/Fire/Steam/Smoke` already early-exit when active count is zero.
- Presence-flag operations are compile-guarded (`#if USE_PRESENCE_GRID`).
- The doc’s A/B results conclude inner-loop presence grid was slower and broke spreading, so it should remain off.

## Plan (Concrete Steps)

### 1) Verify and Hardening Pass (No behavior change)
- Confirm each sim updates active counters on **all** transitions:
  - `SetWaterLevel`, `SetWaterSource`, `SetWaterDrain`
  - `SetFireLevel`, `SetFireSource`
  - `SetSteamLevel`
  - `SetSmokeLevel`
  - `SetHeatSource`, `SetColdSource`
- Confirm `ClearWater/Fire/Steam/Smoke` zero counts.
- Confirm `RebuildSimPresence()` is called after load (or add it if missing).
- Confirm per-cell **presence grid checks are not used** inside sim update loops.

### 2) Early-Exit Coverage Audit
- Ensure these functions short-circuit when active count is zero:
  - `UpdateWater()`
  - `UpdateFire()`
  - `UpdateSteam()`
  - `UpdateSmoke()`
  - `UpdateWaterFreezing()` (already present, verify)
- Keep `UpdateTemperature()` behavior as-is (cannot early-exit just on `tempSourceCount==0`).

### 3) Instrumentation (Optional, Debug)
- Add headless output for:
  - Update counts per sim
  - Active cell counts per sim
- Keep behind a `--stats` flag or `#ifdef` to avoid noise.

### 4) Tests (Minimal but Protective)
- Add a small test file focused on **counter correctness** only:
  - Water: add/remove level, source/drain toggles
  - Fire: add/remove level, source toggles
  - Steam: add/remove
  - Smoke: add/remove
  - Temp: heat/cold source toggles
- Ensure counts never double-increment on level changes that stay non-zero.

### 5) Performance Validation
- Run headless benchmarks on:
  - Empty sim save (should near-zero cost)
  - Mixed sim save (cost proportional to active cells)
- Compare update counts and timings pre/post changes.

## Explicit Non-Goals (For Now)
- Re-enable `USE_PRESENCE_GRID` (the doc’s A/B shows it slows down active sims).
- Inner-loop skipping by presence flags (breaks spreading behavior).

## Future Options (If Needed Later)
- Chunk-level dirty bitfield (skip empty chunks without touching every cell).
- Active-cell list for very sparse sims.
- Two-pass “frontier” marking if per-cell flags are revisited.

