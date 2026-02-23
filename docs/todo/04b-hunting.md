# 04b: Hunting

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Deps**: 04a (carcass + butchering chain must exist) ✅
> **Opens**: farming pressure (over-hunting depletes animals)

---

## Goal

Movers can be assigned to hunt animals. This connects the animal system to the butchering chain from 04a. Without this, carcasses only come from predator kills.

---

## Hunt Designation

**Entity-based designation** — targets animals, not cells. Supports both single-click and drag-select.

**UI flow**: W → H (work → hunt) → click animal or drag rectangle over herd → animals get marked

**How it works**:
- New `bool markedForHunt` field on Animal struct
- **Single-click**: `GetAnimalAtGrid(x, y, z)` finds animal under cursor, toggles mark
- **Drag-select**: reuse existing rectangle-drag input, but resolve animals within bounds instead of cells
- Marked animals get a visual indicator (red tint or crosshair overlay)
- **Hover tooltip**: show animal type + "Marked for Hunt" status when hovering over animals
- WorkGiver scans for marked animals when assigning jobs

**New function needed**: `GetAnimalAtGrid(int x, int y, int z)` — converts animal float pixel coords to grid cell and matches against query cell. Tolerance of 1 cell to handle animals between cells.

---

## Chase & Kill Mechanic

Hunting is a chase. Animals flee from approaching movers (like they flee predators), but movers are much faster.

**Speed comparison**:
- Mover speed: 200 px/s
- Grazer speed: 60 px/s, fleeing at 1.3x = 78 px/s
- **Movers are ~2.5x faster than fleeing grazers** — always catch up

**Flee behavior**:
- Grazers treat approaching movers-on-hunt-job as threats (like predators)
- Detection radius: same as predator detection (`10 * CELL_SIZE`)
- Only triggered when a mover with an active JOBTYPE_HUNT targeting this animal is nearby
- Animals NOT marked for hunt ignore movers (no random stampedes)

**ANIMAL_STATE_BEING_HUNTED**:
- New animal state, entered when attacking mover is adjacent (within CELL_SIZE)
- Animal freezes in place for the attack duration (2-4s)
- Prevents the "animal walks away during attack" problem
- If attack is interrupted (mover dies, job cancelled), animal resumes fleeing

---

## Hunt Job

New job type: **JOBTYPE_HUNT**

**Job struct addition**: `int targetAnimalIdx` field on Job struct (needed — no existing field targets entities).

**Mover capability**: new `canHunt` flag (like canMine, canHaul, etc.).

**Steps**:
1. **Chase**: Mover pathfinds toward animal's current position
   - Re-targets every 1-2s as animal moves (animal is fleeing)
   - Pathfind to animal's current grid cell, not predicted position (simple and sufficient)
   - If animal becomes unreachable (behind walls, in water), timeout after 30s
2. **Attack**: When adjacent (within CELL_SIZE), mover performs attack action
   - Animal enters ANIMAL_STATE_BEING_HUNTED (frozen)
   - Duration: 2s with cutting tool, 4s bare-handed (tool speed via GetJobToolSpeedMultiplier, QUALITY_CUTTING, soft gate)
   - Reset timeWithoutProgress during attack to avoid stuck detector
3. **Kill**: On completion, call `KillAnimal(animalIdx)` from 04a
   - Carcass spawns at animal position
   - Job complete — mover returns to other work

**Edge cases**:
- Animal dies before mover reaches it (predator kill): cancel job, no penalty
- Animal marked by another mover: reservation system (one hunter per animal)
- Mover can't reach animal: timeout after 30s, release reservation
- Animal enters BEING_HUNTED but attack interrupted: animal unfreezes, resumes flee

---

## WorkGiver

`WorkGiver_Hunt()`:
1. Check `canHunt` capability on mover
2. Scan `animals[]` for `active && markedForHunt && reservedByHunter == -1`
3. Find nearest marked animal to idle mover
4. Create JOBTYPE_HUNT with `targetAnimalIdx` set
5. Set `reservedByHunter = moverIdx` on animal

**Job priority**: Between crafting and hauling (movers hunt when not busy crafting)

**CancelJob**: Must clear `reservedByHunter` on the target animal and unfreeze if BEING_HUNTED.

**Note**: No designation cache needed — scanning 256 animals is trivial. Different pattern from cell-based WorkGivers, but appropriate here.

---

## Animal Struct Changes

```c
bool markedForHunt;      // Designated for hunting by player
int reservedByHunter;    // Mover index hunting this animal (-1 = none)
```

## Job Struct Changes

```c
int targetAnimalIdx;     // For JOBTYPE_HUNT — index into animals[] (-1 = none)
```

## Animal State Addition

```c
ANIMAL_STATE_BEING_HUNTED  // Frozen in place during mover attack
```

**Save/load**: All hunting fields are runtime-only (not saved). Hunt designations and in-progress hunts reset on load. This is consistent with how job progress and other transient state works. Persisting transient state across save/load is a separate future effort that should batch all such fields together.

---

## Implementation Phases

### Phase 1: Infrastructure
- Add `markedForHunt` and `reservedByHunter` to Animal struct
- Add `targetAnimalIdx` to Job struct (default -1)
- Add `ANIMAL_STATE_BEING_HUNTED` to AnimalState enum
- Add `canHunt` to mover capabilities
- Add JOBTYPE_HUNT to job types
- Add `GetAnimalAtGrid(x, y, z)` function

### Phase 2: Designation UI
- Add ACTION_WORK_HUNT to action registry (key 'h', MODE_WORK)
- Single-click: detect animal under cursor via GetAnimalAtGrid, toggle markedForHunt
- Drag-select: resolve animals within rectangle bounds, mark all
- Visual: tinted overlay / crosshair on marked animals
- Hover tooltip: animal type + hunt status

### Phase 3: Chase & Kill
- Hunt job state machine: chase → re-target → attack → kill
- Re-target pathfinding every 1-2s during chase
- ANIMAL_STATE_BEING_HUNTED freeze on adjacent contact
- Attack duration scaled by cutting tool quality (QUALITY_CUTTING, soft gate)
- Stuck detector reset during attack phase
- KillAnimal() call on completion (from 04a)

### Phase 4: Flee Behavior
- Grazers detect movers-on-hunt-job as threats (like predators)
- Use existing flee steering with 1.3x speed boost
- Only flee from movers actively hunting THIS animal (check reservedByHunter)
- Animals not marked for hunt remain oblivious to movers

### Phase 5: WorkGiver + Integration
- WorkGiver_Hunt: check canHunt, scan marked animals, assign nearest
- Reservation system (one hunter per animal)
- CancelJob cleanup (release reservation, unfreeze animal)
- **Tests**:
  - Mark animal → mover chases → animal flees → mover catches → attack → carcass
  - Mark via drag-select → multiple animals marked
  - Animal dies mid-hunt (predator) → job cancels cleanly
  - Cancel job → reservation + freeze cleared

---

## Design Notes

- **No health system**: Animals die in one "attack" action. Simple and sufficient for now. Health/combat is a much bigger feature.
- **No hunt zones**: Per-animal clicking + drag-select is enough. Zone-based auto-hunting adds complexity (what if friendlies enter zone?) and can come later.
- **No danger**: Predators don't fight back yet. Hunting predators works the same as grazers. Combat injuries are a future feature.
- **Bare-handed hunting**: Allowed but slow (4s vs 2s). No hard tool gate — same philosophy as other jobs.
- **Chase is guaranteed**: At 200 vs 78 px/s, movers always catch fleeing grazers. The chase just adds a few seconds of gameplay and visual interest.
- **Entity targeting is new**: This is the first entity-based designation. Cell-based designations use a 3D grid; hunt marks live on the Animal struct directly. GetAnimalAtGrid() bridges the gap between grid-click input and float-position animals.
