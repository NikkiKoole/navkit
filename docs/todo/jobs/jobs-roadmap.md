# Jobs System — Future Ideas

Extracted from the original roadmap (now in `docs/done/jobs/jobs-roadmap.md`).
Everything below is **not yet implemented**.

---

## Farming

Multi-stage, seasonal, growth simulation.

- Farm plot: player-designated rectangle
- Per-tile state: empty → tilled → planted → growing → ready → harvest
- Job types: tilling, planting, tending (watering/weeding), harvesting
- Auto-replant option per plot
- Crop type selection per plot
- Seed items consumed on planting, crop items spawned on harvest
- Soil fertility: grid similar to wearGrid pattern, depleted by harvesting, recovers when fallow
- Moisture/irrigation: tiles near water grow faster, Water Carrier job for dry fields
- Trampled paths = lower fertility, proximity to water = higher fertility

---

## Zone Activities

Zones that actively generate jobs (beyond passive filters).

### Autosort Zone
- Mark an "Unsorted" zone + several typed stockpiles
- Items in unsorted zone automatically get hauled to matching stockpiles
- Dump loot somewhere → organized base without micromanagement

### Work Area Zones
- Assign movers to prefer jobs within a zone
- "Miners work the north tunnel", "Haulers prioritize the workshop area"
- Jobs within preferred zone get distance bonus in job scoring

---

## Requester Stockpiles

Stockpile with a target count — generates hauling jobs to maintain that amount.

- Example: stockpile near forge set to "keep 30 coal"
- Haulers automatically refill when count drops below target
- In AssignJobs: check stockpiles with targetCount >= 0 and currentCount < targetCount

---

## Per-Mover Job Priorities

RimWorld-style priority grid. Each mover has priority 1-4 for each job type:

```
         Mining  Hauling  Building  Crafting
Mover 1:   1       3         2        -
Mover 2:   -       1         1        -
Mover 3:   2       2         2        1
```

Movers do ALL priority-1 work before priority-2. Dash = disabled.

---

## Connectivity Regions

Cheap reachability check before expensive pathfinding.

- Flood-fill connectivity IDs per tile
- Recompute on terrain changes (only affected chunks)
- Different region = definitely unreachable, skip pathfinding entirely
- Same region = probably reachable, run real pathfind to confirm

---

## Job Interruption / Resumption

What happens when a mover gets hungry mid-job, gets attacked, or a higher priority job appears?

- Simple: cancel current job, release reservations, handle need, re-find job later
- Complex: pause/resume jobs with saved state (probably overkill for now)

---

## Skill Levels (Future)

Beyond current boolean capabilities (canHaul/canMine/canBuild/canCraft/canPlant):

- Per-mover skill levels 0-20 for each job category
- Affects speed, yield, quality
- Skills improve with practice
