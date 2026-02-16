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

### Food Spoilage States
Food spoilage is the same system — an item condition that advances over time, affected by environment.

- **Fresh**: just harvested/crafted, full nutrition.
- **Stale**: still edible, maybe reduced nutrition or mood penalty later.
- **Rotten**: inedible, must be hauled away (compost?).

**Rates (game-hours):**
- Fresh berries: ~48h (2 days) fresh → rotten
- Dried berries: ~336h (2 seasons) — whole point of drying

**Container modifiers** (already defined in containers.c):
- Basket: 1.0× (open, no benefit)
- Chest: 0.7× (enclosed)
- Clay pot: 0.5× (sealed, best preservation)

**Weather**: rain on exposed food = 1.5× spoilage. Containers with `weatherProtection` block this.

This shares the exact same tick loop, environment modifiers, and stacking concerns as wood/clay conditioning. Implementing food spoilage as "just another condition type" in this system avoids building two parallel aging systems.

---

## Stacks and Condition State

### The Problem
Items have `stackCount` (e.g. 20 berries = 1 Item struct with stackCount=20). A stack shares a single condition state. But what happens when:
- A mover delivers 10 fresh berries to a stockpile slot that already has 10 stale berries?
- A stack of green logs is split — 5 go to a seasoning shed, 5 stay in the rain?

### Solution: Discrete Condition Steps + Split on Mismatch

Conditions are **discrete states** (enums), not continuous floats. A stack of berries is either Fresh, Stale, or Rotten — never "37% spoiled".

**Progression**: each condition state has a **duration** (game-hours in that state before advancing to the next). A simple `conditionTimer` float tracks progress within the current state. When timer >= threshold, state advances and timer resets.

**Merge rule: never merge different conditions.** When delivering an item to a stockpile slot:
- If slot has matching type + material + condition → merge (MergeItemIntoStack, timers average or take worse)
- If slot has matching type + material but **different condition** → treat as different item, find another slot

This means a stockpile might have: slot 1 = "Fresh Berries ×15", slot 2 = "Stale Berries ×8". They're the same ItemType but different condition states, so they don't stack together.

**Split rule: split inherits condition + timer.** When SplitStack is called, the new item gets the same condition state and conditionTimer as the original. Both halves continue aging independently from that point.

**Stockpile slot cache impact**: the existing `stockpileSlotCache[ITEM_TYPE_COUNT][MAT_COUNT]` lookup needs a third dimension (condition), or the cache falls back to scanning. Since condition states are small enums (3-4 values), this is bounded.

**Display**: stockpile tooltips and item names include condition: "Fresh Berries ×15", "Seasoned Oak Log ×4", "Bone-Dry Clay ×10".

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
