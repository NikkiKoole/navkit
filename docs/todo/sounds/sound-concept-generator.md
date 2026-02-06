# Sound and Mind - Concept Generator Spec

Purpose
- Prevent missing concepts as new objects, jobs, and materials are added.
- Provide a small, reliable inventory of "things we could talk about."
- Keep the sound/meaning system data-driven without manual drift.

Scope
- Generate a concept coverage report from authoritative enums.
- Do not auto-create game logic, only surface gaps.
- Allow human overrides for ignored or merged concepts.

Inputs (Authoritative Sources)
- `src/entities/items.h` for ItemType
- `src/entities/jobs.h` for JobType
- `src/entities/workshops.h` for WorkshopType and recipe outputs
- `src/world/grid.h` for CellType
- `src/world/designations.h` for DesignationType
- `src/world/material.h` for MaterialType

Outputs
- `docs/todo/sounds/concepts/coverage.md`
- One row per enum value with:
- Enum source, value name, tags, status, mapped concept id, notes

Status Values
- `mapped`: has a concept id or explicit ignore rule
- `todo`: no mapping yet
- `ignored`: intentionally excluded (noise, placeholder, non-player-facing)

Mapping Configuration
- `docs/todo/sounds/concepts/config.json` (proposed)
- Fields:
- `ignore`: list of enum values to ignore
- `aliases`: map of enum values -> concept id
- `tags`: optional tags for grouping (e.g., `resource`, `terrain`, `job`)

Heuristics (Optional)
- If enum name contains `FOOD`, tag as `need/food`.
- If enum name contains `BED`, tag as `need/rest`.
- If enum name contains `CHAIR` or `TABLE`, tag as `comfort/social`.
- Jobs map to `intent/*`.
- Materials map to `resource/*`.
- Terrain cells map to `location/*`.

CI / Lint Rule
- If any enum value is `todo`, emit a warning.
- Warnings can be whitelisted with ignore/alias rules.

Why This Helps
- When new things are added (chair/bed/table), the generator flags missing mappings.
- You decide whether it should be spoken, sung, or ignored.

Example Coverage Row
| Source | Enum | Tags | Status | Concept |
|---|---|---|---|---|
| ItemType | ITEM_WOOD | resource,material | mapped | resource/wood |
| JobType | JOBTYPE_HAUL | intent,job | mapped | intent/haul |
| ItemType | ITEM_BED | need,rest | todo | - |

Notes
- This generator can be a simple script that parses headers.
- It does not need to be perfect; it just needs to be consistent.
