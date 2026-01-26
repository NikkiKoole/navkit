# Mining Implementation Plan

## Summary
Add mining first, then fix performance, then refactor architecture. This validates the architecture design with a real second job type before committing to abstractions.

## Rationale
- Performance fixes require architectural changes anyway (dirty-marking, connectivity regions)
- Refactoring without a second job type risks over/under-designing
- Mining is small (~200-300 lines), reuses 90% of existing code
- Technical debt is bounded: worst case ~100 lines need rewriting later

## Prerequisites
- [ ] **Minable wall tiles** - new terrain type (CELL_STONE_WALL, CELL_ORE_WALL?)
- [ ] **Designation visual** - how to show "marked for mining" on a tile
- [ ] **Mined item type** - what drops when you mine (stone, ore?)

## Recommended Order

### Step 1: Mining (~1-2 days)
Add mining as a quick prototype using the current architecture:
- `Designation` struct (x, y, z, type, assignedMover, progress)
- New states: `JOB_MOVING_TO_DESIGNATION`, `JOB_WORKING`
- New priority pass in `AssignJobs()` before hauling
- Mining state machine in `JobsTick()`
- Terrain change: wall → floor + spawn item

**Files:**
- `pathing/jobs.h` - add states, Designation struct
- `pathing/jobs.c` - add assignment pass and state machine
- `pathing/mover.h` - add targetDesignation, workProgress fields
- `pathing/grid.c` - terrain modification function
- `pathing/demo.c` - designation UI (click-drag to mark tiles)

### Step 2: Targeted Performance Fix (~1 day)
After mining works, apply minimal fix to the rehaul bottleneck:
- Dirty-mark items when stockpile filters change
- Only scan marked items in rehaul pass (~50 lines)
- Skip full refactor for now

### Step 3: Architecture Refactor (~2-3 days)
With two job types to validate against:
1. Introduce Job pool + `mover.currentJobId`
2. Port hauling to `RunJob_Haul()` driver
3. Port mining to `RunJob_Mine()` driver
4. Split `AssignJobs()` into WorkGivers  -- we did this has performance issue. 
5. Remove old job fields from Mover

### Step 4: Construction
With clean architecture, construction becomes straightforward.

## Why Not Fix Performance First?
- The 89x slowdown is in the rehaul loop
- Proper fix requires dirty-marking (architectural change)
- Without mining, you'd be optimizing blind
- Mining doesn't make the rehaul problem worse (separate priority pass)

## Verification
After Step 1 (mining):
- Player can click-drag to designate tiles for mining
- Movers walk to adjacent tiles and "work" (progress bar)
- Completed mining changes wall → floor and spawns item
- Spawned items get hauled to stockpiles (existing system)
