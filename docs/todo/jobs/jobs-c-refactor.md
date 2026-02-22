# jobs.c Refactor Analysis

**Date:** 2026-02-21
**File:** `src/entities/jobs.c` — 6,044 lines, ~85 functions

## Current Structure

| Group | LOC | Description |
|-------|-----|-------------|
| Job Pool | ~105 | Init/Create/Release/GetJob |
| Designation Cache | ~301 | 18 cache rebuild functions (mostly 5-line wrappers) |
| Shared Step Helpers | ~194 | RunPickupStep, RunCarryStep, RunWalkTo*, RunToolFetchStep |
| Job Drivers | ~1,174 | 21 `RunJob_*` functions (state machines per job type) |
| Job Tick + Idle List | ~95 | JobsTick dispatch, idle mover tracking |
| Cancel/Unassign | ~280 | CancelJob (142 LOC), UnassignJob (133 LOC) |
| Assignment Orchestration | ~478 | AssignJobs + P1-P4 priority phases |
| WorkGivers | ~2,274 | 27 `WorkGiver_*` functions (job discovery) |

## Biggest Functions

| Rank | Function | LOC | Notes |
|------|----------|-----|-------|
| 1 | `RunJob_Craft()` | 599 | 13-step state machine, largest in file |
| 2 | `WorkGiver_Craft()` | 306 | Validates 3-5 inputs + fuel + tool + reachability |
| 3 | `RunJob_HaulToBlueprint()` | 154 | Multi-step haul to construction site |
| 4 | `WorkGiver_DeliverToPassiveWorkshop()` | 144 | Deliver to passive workshop inputs |
| 5 | `CancelJob()` | 142 | Releases all item/slot/workshop reservations |
| 6 | `UnassignJob()` | 133 | Preserves progress, releases reservations |
| 7 | `WorkGiver_Haul()` | 132 | Find ground items for stockpiles |
| 8 | `TryAssignItemToMover()` | 127 | Generic item-to-mover haul assignment |
| 9 | `WorkGiver_Knap()` | 121 | Find rock walls to knap |
| 10 | `WorkGiver_PlantSapling()` | 119 | Find valid planting locations |

## The Problem

The file does three distinct jobs:
1. **Job discovery** — 27 WorkGiver functions find available work (2,274 LOC)
2. **Job assignment** — Priority-phased orchestration matches movers to jobs (478 LOC)
3. **Job execution** — 21 RunJob drivers implement per-type state machines (1,174 LOC)

Plus shared infrastructure (pool, cache, cancel/unassign).

## Extraction Options

### Option A — Minimal: Extract Craft Only

Move `RunJob_Craft` + `WorkGiver_Craft` to a new `craft.c` (~905 LOC).

- **Result:** jobs.c drops to ~5,100 lines
- **Risk:** Low — craft logic is self-contained, no other job type touches it
- **Dependencies:** workshops.h, items.h, recipes (via workshops.h)
- **Benefit:** Removes the two largest functions from the file

### Option B — Moderate: Craft + Job Drivers (Recommended)

Also move all 21 `RunJob_*` functions + shared step helpers to `job_drivers.c` (~1,174 LOC).

- **Result:** jobs.c drops to ~3,900 lines
- **Risk:** Medium — shared helpers (RunPickupStep, etc.) need to be exported
- **Benefit:** Clean separation of "execute jobs" vs "discover + assign jobs"
- **New files:**
  - `src/entities/craft.c` (~905 LOC) — craft driver + craft WorkGiver
  - `src/entities/job_drivers.c` (~1,174 LOC) — all other RunJob_* + shared helpers

### Option C — Aggressive: Craft + Drivers + WorkGivers

Also move all 27 `WorkGiver_*` functions to `workgivers.c` (~2,274 LOC).

- **Result:** jobs.c drops to ~1,800 lines (pool + assignment + cache only)
- **Risk:** Higher — creates coupling between jobs.c and workgivers.c
- **Benefit:** Each file has a single responsibility (discover / assign / execute)
- **New files:**
  - `src/entities/craft.c` (~905 LOC)
  - `src/entities/job_drivers.c` (~1,174 LOC)
  - `src/entities/workgivers.c` (~2,274 LOC)

## Other Observations

- **Designation cache** (301 LOC) has 14 nearly-identical 5-line wrapper functions that could be table-driven, but the size win is marginal.
- **CancelJob/UnassignJob** are complex (142/133 LOC) but tightly coupled to assignment logic — best left in jobs.c.
- **Idle mover tracking** is only 61 LOC and tied to JobsTick — not worth extracting.
- For context, sibling files are also large: `mover.c` (64KB), `stockpiles.c` (53KB), `workshops.c` (41KB).

## Unity Build Notes

Any new `.c` files must be added to both `src/unity.c` and `tests/test_unity.c`. Headers are picked up automatically.
