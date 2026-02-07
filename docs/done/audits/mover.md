Here's the audit of `src/entities/mover.c`:

---

**Finding 1: CreateJob missing field initialization — stale fuelItem causes cross-job reservation theft**
- **What**: `CreateJob()` doesn't initialize `fuelItem`, `targetWorkshop`, `targetBillIdx`, `targetAdjX`, `targetAdjY`, or `workRequired`. Job slots are reused via free list. `ReleaseJob` doesn't clear them either.
- **How it breaks**: A craft job with `fuelItem = 7` completes. Slot is reused for a haul job. When that haul job is cancelled, `CancelJob` sees stale `fuelItem = 7` and releases its reservation — stealing it from whatever mover actually reserved item 7.
- **Player impact**: Crafters mysteriously fail — their reserved items get snatched by unrelated job cancellations.
- **Severity**: **HIGH**

**Finding 2: ClearMovers does not release workshop assignedCrafter**
- **What**: `ClearMovers()` releases item reservations, stockpile slots, designations, blueprints — but NOT `workshop.assignedCrafter`.
- **How it breaks**: After load/clear, workshops retain ghost crafter assignments. `WorkGiver_Craft` skips them because `assignedCrafter >= 0`. Workshops become permanently locked.
- **Player impact**: Workshops refuse to accept crafters after loading a save. They appear perpetually "busy" with no worker.
- **Severity**: **HIGH**

**Finding 3: Mover avoidance is 2D-only — cross-z phantom repulsion**
- **What**: `BuildMoverSpatialGrid()` and `ComputeMoverAvoidance()` have no z-level awareness. Movers on different floors repel each other.
- **How it breaks**: Two movers on z=0 and z=1 at the same x/y get strong repulsion forces, pushing both sideways near ladders/stairs.
- **Player impact**: Movers jitter near ladders, especially in vertical builds. Could push movers into walls.
- **Severity**: **MEDIUM**

**Finding 4: Stuck detection ignores z-changes — ladder descent triggers false stuck**
- **What**: Stuck detection only tracks `lastX`/`lastY`. A mover descending a ladder barely moves in x/y but makes real z progress. `lastZ` exists but is never read or updated after init.
- **How it breaks**: Mover descending 3+ z-levels accumulates `timeWithoutProgress`, triggers repath, job gets cancelled after 3 seconds despite making real progress.
- **Player impact**: Jobs cancelled mid-ladder-descent. Movers oscillate on deep maps.
- **Severity**: **MEDIUM**

**Finding 5: TryFinalApproach movement invisible to stuck detector**
- **What**: `UpdateMovers` (stuck detection) runs before `JobsTick` (`TryFinalApproach`). Final approach nudges aren't seen as progress.
- **How it breaks**: At very low speeds, the tiny nudge might not register as movement, causing false stuck detection near pickup targets.
- **Severity**: **LOW**

**Finding 6: Deactivated mover drops carried items inside walls**
- **What**: When a mover is stuck in a blocked cell with no adjacent walkable cell, it gets deactivated. `CancelJob` drops the carried item at the mover's (unwalkable) position.
- **How it breaks**: Item ends up inside a wall — unreachable, unhaulable, permanently lost.
- **Player impact**: Items silently vanish. Rare but permanent loss.
- **Severity**: **MEDIUM**

**Finding 7: PushMoversOutOfCell doesn't reset lastX/lastY**
- **What**: After teleporting a mover, `lastX`/`lastY` still point to the old position. False stuck time could accumulate briefly.
- **How it breaks**: Self-correcting after one repath cycle. Minimal practical impact.
- **Severity**: **LOW**

---

| # | Finding | Severity |
|---|---------|----------|
| 1 | CreateJob stale field initialization — reservation theft | **HIGH** |
| 2 | ClearMovers doesn't release workshop assignments | **HIGH** |
| 3 | Mover avoidance 2D-only — cross-z repulsion | **MEDIUM** |
| 4 | Stuck detection ignores z-changes — false stuck on ladders | **MEDIUM** |
| 5 | TryFinalApproach invisible to stuck detector | LOW |
| 6 | Deactivated mover drops items in walls | **MEDIUM** |
| 7 | PushMoversOutOfCell doesn't reset stuck tracking
