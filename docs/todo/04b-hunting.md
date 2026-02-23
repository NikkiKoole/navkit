# 04b: Hunting

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Deps**: 04a (carcass + butchering chain must exist)
> **Opens**: farming pressure (over-hunting depletes animals)

---

## Goal

Movers can be assigned to hunt animals. This connects the animal system to the butchering chain from 04a. Without this, carcasses only come from predator kills.

---

## Hunt Designation

**Per-animal designation only** (no hunt zones — those can come later).

Player clicks on a specific animal to mark it for hunting. This is different from cell-based designations — it targets an entity.

**UI flow**: W → H (work → hunt) → click animal → animal gets marked

**How it works**:
- New `bool markedForHunt` field on Animal struct
- Clicking an animal in hunt mode toggles the mark
- Marked animals get a visual indicator (red tint or crosshair overlay)
- WorkGiver scans for marked animals when assigning jobs

---

## Hunt Job

New job type: **JOBTYPE_HUNT**

**Steps**:
1. **Walk to animal**: Mover pathfinds toward animal's current position
   - Animal may be moving — mover re-targets periodically (every 1-2s)
   - If animal moves too far (unreachable), job fails gracefully
2. **Attack**: When adjacent (within CELL_SIZE), mover performs attack action
   - Duration: 2s with cutting tool, 4s bare-handed (tool speed via GetJobToolSpeedMultiplier)
   - Mover stands still during attack (reset timeWithoutProgress to avoid stuck detector)
3. **Kill**: On completion, call `KillAnimal(animalIdx)` from 04a
   - Carcass spawns at animal position
   - Job complete — mover returns to other work

**Edge cases**:
- Animal dies before mover reaches it (predator kill): cancel job, no penalty
- Animal marked by another mover: reservation system (one hunter per animal)
- Mover can't reach animal: timeout after 30s, release reservation

---

## WorkGiver

`WorkGiver_Hunt()`:
1. Scan animals[] for `active && markedForHunt && !reservedByHunter`
2. Find nearest marked animal to idle mover
3. Create JOBTYPE_HUNT with target animalIdx
4. Set `reservedByHunter = moverIdx` on animal

**Job priority**: Between crafting and hauling (movers hunt when not busy crafting)

**CancelJob**: Must clear `reservedByHunter` on the target animal.

---

## Animal Struct Changes

```c
bool markedForHunt;      // Designated for hunting by player
int reservedByHunter;    // Mover index hunting this animal (-1 = none)
```

These are runtime-only fields (not saved — hunt designations reset on load, which is fine).

---

## Implementation Phases

### Phase 1: Designation + Job Type
- Add `markedForHunt` and `reservedByHunter` to Animal struct
- Add JOBTYPE_HUNT to job types
- Add ACTION_WORK_HUNT to action registry (key 'h', MODE_WORK)
- Input handler: click animal in hunt mode → toggle markedForHunt
- Visual: tinted overlay on marked animals

### Phase 2: Hunt Job Execution
- Hunt job state machine: walk → re-target → attack → kill
- Attack duration scaled by cutting tool quality
- Stuck detector reset during attack phase
- KillAnimal() call on completion (from 04a)

### Phase 3: WorkGiver
- WorkGiver_Hunt: scan marked animals, assign nearest to idle mover
- Reservation system (one hunter per animal)
- CancelJob cleanup (release reservation)
- **Test**: Mark animal → mover walks to it → attacks → carcass appears

---

## Design Notes

- **No health system**: Animals die in one "attack" action. Simple and sufficient for now. Health/combat is a much bigger feature.
- **No hunt zones**: Per-animal clicking is enough. Zone-based auto-hunting adds complexity (what if friendlies enter zone?) and can come later.
- **No danger**: Predators don't fight back yet. Hunting predators works the same as grazers. Combat injuries are a future feature.
- **Bare-handed hunting**: Allowed but slow (4s vs 2s). No hard tool gate — same philosophy as other jobs.
