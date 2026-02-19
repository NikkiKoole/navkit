# Cross-Z-Level Job Assignment Fix

## Context

On hilly terrain, adjacent walkable cells can be at different z-levels (e.g., mover at z=2, grass at z=1). Nearly every WorkGiver and item-finding function filters candidates by exact z-match (`moverZ == targetZ`), preventing movers from finding reachable work/items at different z-levels. These filters are redundant — `FindPath` already handles reachability. This was a performance optimization from the flat-terrain era that became a bug source on hills.

**Already fixed** (earlier in this session): 13 designation WorkGiver z-filters, `IdleMoverSearchCallback`, `TryAssignItemToMover` fallback, `HaulItemFilter`.

## Plan

### Step 1: Extend `IterateItemsInRadius` to search z-1, z, z+1

**File**: `src/entities/items.c` (~line 425)

The spatial grid is indexed by z-level, so `FindFirstItemInRadius` inherently only searches one z. Add an outer loop over `z-1, z, z+1`:

```c
// Current: single z search
int cellIdx = z * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;

// New: loop zMin..zMax
int zMin = (z > 0) ? z - 1 : 0;
int zMax = (z < itemGrid.gridD - 1) ? z + 1 : z;
for (int sz = zMin; sz <= zMax; sz++) {
    int cellIdx = sz * (itemGrid.gridW * itemGrid.gridH) + ty * itemGrid.gridW + tx;
    // ... existing inner loop
}
```

This automatically fixes all callers: `FindFirstItemInRadius`, `QueryItemsInRadius`, `FindNearestUnreservedItem`.

### Step 2: Extend `FindItemInContainers` to search z-1, z, z+1

**File**: `src/entities/containers.c` (~line 282)

```c
// Current:
if ((int)items[i].z != z) continue;

// New:
int iz = (int)items[i].z;
if (iz < z - 1 || iz > z + 1) continue;
```

### Step 3: Remove remaining WorkGiver z-filters (14 locations)

**File**: `src/entities/jobs.c`

Remove these lines (simple deletions):

| Line | Function | Filter |
|------|----------|--------|
| 3739 | `WorkGiver_Haul` | `if ((int)item->z != moverZ) continue;` |
| 3819 | `WorkGiver_DeliverToPassiveWorkshop` | `if (ws->z != moverZ) continue;` |
| 3879 | `WorkGiver_DeliverToPassiveWorkshop` | `if ((int)item->z != moverZ) continue;` |
| 4539 | `WorkGiver_Craft` | `if (ws->z != moverZ) continue;` |
| 4552 | `WorkGiver_Craft` | `if (ws->z != moverZ) continue;` |
| 4611 | `WorkGiver_Craft` | `if ((int)item->z != ws->z) continue;` |
| 4682 | `WorkGiver_Craft` | `if ((int)item2->z != ws->z) continue;` |
| 4893 | `WorkGiver_PlantSapling` | `if ((int)items[j].z != moverZ) continue;` |
| 5558 | `WorkGiver_Build` | `if (bp->z != moverZ) continue;` |
| 5794 | `FindNearestRecipeItem` | `if ((int)item->z != moverZ) continue;` |

Also remove item-vs-workshop z-checks in `WorkGiver_DeliverToPassiveWorkshop`:
| 3847 | check `(int)item->z == ws->z` in the "already at workshop" skip |
| 3890 | check `(int)item->z == ws->z` in the "already at workshop" skip |

And in `WorkGiver_IgniteWorkshop`:
| 3992 | `if ((int)item->z != ws->z) continue;` |

Clean up any resulting unused `moverZ` variables.

### Step 4: Fix `FindNearestFuelItem` in workshops.c

**File**: `src/entities/workshops.c` (~line 275)

```c
// Remove:
if ((int)item->z != ws->z) continue;

// Replace with z±1 range check:
int iz = (int)item->z;
if (iz < ws->z - 1 || iz > ws->z + 1) continue;
```

Also update the `FindItemInContainers` call to pass workshop z (already done — the infrastructure fix in Step 2 handles this).

### Step 5: Write comprehensive test suite

**New file**: `tests/test_cross_z.c`
**Add include to**: `tests/test_unity.c`

**Grid setup** — a hillside with ramp connecting two z-levels:
```
z=0: all CELL_WALL (bedrock)
z=1: x=0..4 CELL_WALL (ground), x=5 CELL_RAMP_E, x=6..9 CELL_AIR
z=2: x=0..4 CELL_AIR (walkable above z1 walls), x=5 CELL_AIR, x=6..9 CELL_WALL (high ground)  
z=3: x=0..5 CELL_AIR, x=6..9 CELL_AIR (walkable above z2 walls)
```
Walkable surface: z=2 on left (x=0..4), z=3 on right (x=6..9), ramp at x=5 z=1 connects them.

**Test cases** (each places mover on one side, target on other):

1. **cross_z_haul_item** — item at z=3, stockpile at z=2, mover at z=2 → AssignJobs assigns haul
2. **cross_z_gather_grass** — grass designation at z=3, mover at z=2 → job assigned (already fixed, regression test)
3. **cross_z_workshop_craft** — workshop at z=3, mover at z=2 → craft job assigned
4. **cross_z_deliver_passive_workshop** — passive workshop at z=3, item at z=2, mover at z=2 → delivery job
5. **cross_z_build_blueprint** — blueprint at z=3, materials at z=2, mover at z=2 → build job
6. **cross_z_plant_sapling** — plant designation at z=3, sapling item at z=2, mover at z=2 → plant job
7. **cross_z_find_item_in_radius** — item at z=3, search from z=2 → FindFirstItemInRadius finds it
8. **cross_z_find_item_in_container** — container at z=3, search from z=2 → FindItemInContainers finds it
9. **cross_z_ignite_workshop** — workshop at z=3, fuel at z=2 → ignition job assigned
10. **cross_z_mine_designation** — mine designation at z=3, mover at z=2 → mining job assigned (regression)

### What we KEEP (Category D — runtime arrival checks)

All `correctZ` checks in `RunJob_*` functions are KEPT. These verify the mover has physically arrived at the target z-level before starting work. They are NOT assignment filters:
- `RunJob_Mine`, `RunJob_Channel`, `RunJob_DigRamp`, `RunJob_RemoveFloor`, `RunJob_RemoveRamp`, `RunJob_Chop`, `RunJob_ChopFelled`, `RunJob_GatherSapling`, `RunJob_GatherGrass`, `RunJob_HarvestBerry`, `RunJob_GatherTree`, `RunJob_Clean`, `RunJob_IgniteWorkshop`

## Verification

1. `make test` — all 31 existing test suites pass (regression)
2. New `test_cross_z` suite passes (10 tests)
3. Manual: load hilly save, verify movers find work across z-levels
