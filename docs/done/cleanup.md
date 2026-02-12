Stale/Outdated (features listed as TODO but already implemented)

**HIGH priority:**

| File | What's stale |
|------|-------------|
| `jobs/jobs-roadmap.md` | Bill modes listed as "pending" — all 3 (DO_X_TIMES, DO_UNTIL_X, DO_FOREVER) are implemented |
| `jobs/jobs-roadmap.md` | "More workshops" listed as pending — 7 workshops exist (stonecutter, sawmill, kiln, charcoal pit, hearth, drying rack, rope maker) |
| `jobs/jobs-checklist.md` | Same stale pending items duplicated from roadmap |
| `jobs/overview.md` | Passive workshop system described as future work — fully implemented |
| `jobs/overview.md` | Bill modes described as "critical bottleneck" — already working |
| `jobs/overview.md` | Construction table uses old item→cell mapping, not recipe system |
| `architecture/blueprint-material-selection.md` | Entire doc superseded by ConstructionRecipe system |
| `rendering/material-based-rendering.md` | Marked TODO but spriteOverrides + ItemSpriteForTypeMaterial already exist |

**MEDIUM priority:**

| File | What's stale |
|------|-------------|
| `jobs/jobs-from-terrain.md` | Tree growth + grass gathering described as future — both implemented |
| `nature/brush-bushes.md` | Doesn't account for vegetationGrid system |
| `nature/wild-plants.md` | Vague "Gatherer" job — actual system uses JOBTYPE_GATHER_GRASS |
| `world/stairs.md` | Ladders + ramps already exist as construction recipes |
| `vision-extracted-todos.md` | References `architecture/data-driven-jobs-materials.md` which doesn't exist |

---

## Duplicated Content

| Files | Overlap |
|-------|---------|
| `world/stairs.md` + `world/multi-cell-stairs.md` | Both describe z-level transitions, unclear distinction |
| `nature/wild-plants.md` + `nature/brush-bushes.md` | Nearly identical mechanics (spawn on grass, gatherer job, food items, regrowth) |
| `nature/soil-fertility.md` + `nature/moisture-irrigation.md` | Both describe "near water" plant growth modifiers |
| `sprite-cleanup.md` + `sprite-table.md` | Sequential designs for same system, overlapping problem statements |
| `sounds/sound-and-mind.md` | Raw AI chat transcript — the 4 other sound-and-mind-*.md files are the real design docs |
| `sounds/sound-concept-generator.md` | Repeats purpose/scope from sound-and-mind-design.md |
| `jobs/jobs-from-terrain.md` + `jobs/overview.md` | Overlapping fire system + material tables |

---

## Contradictions

| Files | Issue |
|-------|-------|
| `sprite-cleanup.md` vs `sprite-table.md` | Disagree on where leavesSprite/saplingSprite live (MaterialDef vs overrides table) |
| `world/elevators.md` | Recommends "start with Option C" then gives detailed SCAN algorithm assuming Option B |
| `ui/pie-menu.md` | Proposed right-click behavior contradicts existing sticky-mode input model |
| `world/unify-rock-with-soil.md` | Recommends Option A but unresolved questions undermine it |

---

## Broken References

| File | Broken ref |
|------|-----------|
| `pathfinding/social-navigation.md` | Links to `elevators.md` (wrong relative path after folder move) |
| `vision-extracted-todos.md` | References `architecture/data-driven-jobs-materials.md` — doesn't exist |
| `sounds/sound-concept-generator.md` | References `concepts/coverage.md` and `concepts/config.json` — don't exist |
| `audits/input_mode-outcome.md` | Uses line numbers in input.c that have likely shifted |
| `architecture/blueprint-material-selection.md` | Shows Blueprint struct fields that don't match actual code |

---

## Stub Files (too minimal to be useful)

9 pathfinding docs are one-liners, plus `items/containers.md`, `items/requester-stockpiles.md`, `items/tool-qualities.md`, `jobs/zone-activities.md`, `jobs/zone-designations.md`, `nature/soil-fertility.md`.

---

## Suggested Cleanup

1. **Delete or archive**: `blueprint-material-selection.md` (superseded), `sound-and-mind.md` (raw transcript), `jobs-from-terrain.md` (conversation dump, not design doc)
2. **Merge**: stairs + multi-cell-stairs, wild-plants + brush-bushes, soil-fertility + moisture-irrigation, sprite-cleanup + sprite-table
3. **Update**: jobs-roadmap.md and jobs-overview.md to reflect implemented features (bill modes, workshops, passive system, construction recipes)
4. **Move to done/**: `architecture/roadmap-test-improvements.md` (documents completed work)

Want me to start cleaning any of these up?
