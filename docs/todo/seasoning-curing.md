# Seasoning & Curing System (State‑Based Materials)

Date: 2026-02-07
Scope: Turn time into a resource by adding **material states** (seasoning/curing) that integrate with stockpiles and staged construction.

## Why This Matters
Seasoning/curring makes DF‑style stockpiles meaningful and supports the CDDA‑style multi‑stage construction loop. It creates a real **pipeline**: harvest → prepare → build, instead of instant placement.

## Goals
- Add **stateful materials** (wood, clay/mud) without a massive refactor.
- Make stockpiles feel like **conditioning zones** (dry/wet/covered).
- Tie directly into staged construction (frame → fill → cure).

## Non‑Goals
- Full weather system or detailed humidity simulation.
- Deep item quality systems beyond a few states.
- Full tool requirements for every stage (can come later).

---

## Minimal State Model (Actionable)

### Wood Seasoning States
- **Green** (fresh, wet): flexible, heavy, poor burn.
- **Seasoned** (ready): strong, stable; preferred for framing.
- **Brittle** (over‑dry): less ideal for load‑bearing.
- **Rotten** (too wet too long): unusable, becomes mulch/fuel only.

### Clay/Mud Curing States
- **Wet (slurry)**: usable for daubing or molding.
- **Leathery**: trim/shape window holes, surface smoothing.
- **Bone‑Dry**: ready for firing or final finish.
- **Cracked**: cured too fast; reduced quality/insulation.

These are intentionally **small sets**; they map cleanly to staged builds.

---

## Integration with Current Systems

### Items
- Items already tick via `ItemsTick` and have material/natural flags.
- Add a lightweight **condition enum** + **age timer** to `Item`:
  - `ItemCondition woodCond; ItemCondition clayCond; float conditionTimer;`
- Keep this data small and optional — only used on specific item types (logs, clay, mud blocks, etc.).

### Stockpiles as Conditioning Zones
Introduce a **stockpile environment tag**:
- `Dry / Covered / Wet / Open`

**Examples**:
- **Seasoning Shed** = Covered + Dry → speeds green → seasoned.
- **Riverbank pile** = Wet → delays seasoning, may rot.
- **Indoor pile** = Dry but slow (less airflow).

This uses existing stockpile zoning without new map systems.

### Staged Construction (ties into construction-staging.md)
- **Stage requirements** can specify **required condition**:
  - Frame stage requires **Seasoned wood**.
  - Daub stage requires **Wet clay**.
  - Finish stage prefers **Bone‑Dry clay**.
- If the wrong condition is delivered, either block the stage or apply a quality penalty.

---

## Intermezzo Jobs (Low‑Complexity, High Flavor)
These are periodic jobs that **nudge state** rather than fully transform it.

- **Log‑Turner**: visits seasoning stockpile, speeds green → seasoned.
- **Mud‑Mister**: if curing too fast (hot area), prevents “cracked” state.
- **Bark‑Presser**: keeps bark flat while drying (future shingles).

These jobs can be implemented as lightweight “maintenance” tasks tied to a stockpile type or WIP tile.

---

## Failure States (Optional, Later)
- **Soggy Bottom**: wood stored on wet tiles never seasons and can rot.
- **Wash‑Out**: partially cured walls degrade if exposed to rain/water.

These can be added once the base system is stable.

---

## Phased Implementation (Small Steps)

### Phase 0: Add conditions + timers
- Add condition enum + timers to `Item`.
- Add a **single update loop** that advances conditions based on simple rules (dry/wet flags).

### Phase 1: Stockpile conditioning tags
- Add a `StockpileEnv` flag (dry/wet/covered).
- Speed up/down the condition timer based on pile type.

### Phase 2: Construction gating
- Add simple checks in staged construction: require `Seasoned` logs for frames; `Wet` clay for daub.

### Phase 3: Intermezzo jobs
- Add periodic tending jobs to improve outcomes (optional).

---

## UI / Feedback (Lightweight)
- Stockpile lists show counts by condition (e.g., “12x Logs (Green), 8x Logs (Seasoned)”).
- Tooltip on items shows condition state.
- WIP tiles display “Curing” progress and risk (e.g., “Crack risk: high”).

---

## Open Questions
- Should conditions apply to **all** items or only key ones (logs, clay, mud)?
- Do we block stage progress when conditions are wrong, or allow with penalties?
- Do we want **covered vs open** piles, or just **dry vs wet** for a first pass?

---

## Weather Requirement (Clarification)
You **do not** need a full weather system to make seasoning/curing work.

**Minimal approach (recommended first):**\n
- Use **stockpile environment tags** (dry/wet/covered/open) to drive condition timers.\n
- This already creates the “pipeline” feel without global weather.\n

**Optional next step:**\n
- Add a single global flag like `isRaining` or `weatherWetness`.\n
- Apply it only to **open** stockpiles and **WIP** tiles.\n

**Full weather system** can come later if you want seasonal depth, crop interaction, or more systemic risk.\n
