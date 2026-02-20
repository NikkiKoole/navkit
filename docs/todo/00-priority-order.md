# Survival Feature Priority Order

> Master priority list for the survival mode feature roadmap. Updated 2026-02-20.

---

## Done (moved to `docs/done/`)

- **debugging-workflow.md** — Event log + state audit + F5/F7/F8 keys
- **medium-features-backlog.md** — Mud + cleaning done (weeding needs farming)
- **hunger-and-food.md** — Core hunger, auto-eat, starvation death, berries

## Reference Docs (not actionable tasks)

- **naked-in-the-grass.md** — Gap analysis / vision doc with updated addendum
- **naked-in-the-grass-details.md** — Deep dives on tool qualities + animals design
- **workshop-evolution-plan.md** — Architecture roadmap (templates → stations → workspaces)

---

## Build Order

### Tier 0: Prerequisites (unblock everything)

| # | Doc | Effort | Deps | What It Does |
|---|-----|--------|------|-------------|
| 01 | `01-workshop-construction-costs.md` | ~1 session | None | Workshops cost materials + build time. Fixes free-workshop problem. Resolves bootstrap chicken-and-egg (primitive workshops = sticks only). |
| 02 | `02-tool-quality-and-digging-stick.md` | ~2.5 sessions | None | Quality framework (5 types, 3 levels), IF_TOOL flag, equippedTool on Mover, speed formula. Tool items: sharp stone gains qualities, + digging stick, stone axe, stone pick, stone hammer. Every job gets speed scaling. |

### Tier 1: Core Survival Loop (playable day-1)

| # | Doc | Effort | Deps | What It Does |
|---|-----|--------|------|-------------|
| 03 | `03-primitive-shelter-and-doors.md` | ~2 sessions | 01, 02 | CELL_DOOR + stick walls, leaf roofs, leaf doors. Ground fire (risky, no tools) vs fire pit (safe, needs digging stick). Completes shelter + warmth loop. |
| 04 | `04-cooking-and-hunting.md` | ~2 sessions | 02 | Animals become food: hunt → butcher → cook → eat. Raw/cooked meat, carcass, hide. Cutting quality gates butchering. |
| 05 | `05-fog-of-war-exploration.md` | ~3.5 sessions | None* | Map starts hidden in survival. Vision radius reveals terrain. Straight-line explore designation. Pathfinding untouched (exploration = job assignment filter only). *Best after 01-04 exist. |

### Tier 2: Expansion (deeper gameplay)

| # | Doc | Effort | Deps | What It Does |
|---|-----|--------|------|-------------|
| 06 | `06-farming-and-crops.md` | ~2.5 sessions | 02, 04 | Farm plots, tilling (needs digging quality), planting, growth stages. Seasons affect crops. Scalable food source. |
| 07 | `07-clothing-and-textiles.md` | ~2 sessions | 03, 04 | Loom + tailor. Grass/cordage → cloth → clothing. Hides → leather. Cold resistance on movers. |
| 08 | `08-thirst-and-water.md` | ~1.5 sessions | 02 | Thirst need + water carrying in pots. Containers already done — just needs thirst mechanic + water item. |

### Tier 3: Polish & Depth

| # | Doc | Effort | Deps | What It Does |
|---|-----|--------|------|-------------|
| 09 | `09-loop-closers.md` | ~2 sessions | 06 | Glass, compost, ash/lye, scaffolding. Closes open material loops. |
| 10 | `10-personality-and-skills.md` | ~3 sessions | All | Names, skill progression (8 types), moods. The "character" layer. |
| 11 | `11-stockpile-filter-redesign.md` | ~1 session | More items | Hierarchical filter UI. Only matters with many more item types. |

### Standalone

| Doc | Notes |
|-----|-------|
| `feature-03-doors-shelter.md` | Original F03 design. Superseded by `03-primitive-shelter-and-doors.md` which adds primitive tier. Keep as reference for CELL_DOOR technical design. |
| `feature-04-tools-knapping.md` | Original F04 design. Superseded by `02-tool-quality-and-digging-stick.md` which includes CDDA analysis. Keep as reference for phase details. |

---

## Session Plan

```
Session 1-2:    Workshop construction costs
                → Workshops require materials + build time
                → Primitive workshops (campfire, drying rack, rope maker) cost only sticks

Session 2-4:    Tool quality framework + tool items
                → Quality enum, IF_TOOL, equippedTool on Mover, speed formula
                → Sharp stone gains cutting:1, craft digging stick/axe/pick/hammer
                → Every existing job immediately benefits from tools

Session 5-6:    Primitive shelter + CELL_DOOR + ground fire / fire pit
                → Stick walls, leaf roofs, leaf doors from gathered materials
                → Ground fire (3 sticks, dangerous) vs fire pit (needs digging stick, safe)
                → Day-1 loop complete: gather → shelter → fire → sleep → survive

Session 7-8:    Cooking & hunting
                → Hunt animals → carcass → butcher (needs cutting tool) → raw meat
                → Cook at hearth/campfire → cooked meat (best food)
                → Animals are now a food source, fire is useful

Session 9-12:   Fog of war
                → Map starts hidden, vision radius reveals terrain
                → Straight-line explore designation (no pathfinding)
                → Job assignment filters by explored state, HPA* untouched

Session 13+:    Farming → clothing → thirst → loop closers → personality
```

## The Day-1 Survival Arc (after sessions 1-6)

```
Minute 1:     Pick up sticks from ground
Minute 2:     Build GROUND FIRE on bare rock (3 sticks)           → warmth! (risky)
Minute 3:     Gather leaves, build leaf pile bed (4 leaves)        → can rest
Minute 5:     Build drying rack (4 sticks), build rope maker (4 sticks)
Minute 8:     Gather grass → dry → twist → braid → first cordage
Minute 12:    Knap sharp stone at stone wall                       → cutting:1
Minute 15:    Build stick walls + leaf roof + leaf door            → shelter!
Minute 20:    Craft digging stick (sticks + cordage, needs cutting:1)
Minute 22:    Build FIRE PIT with rocks (needs digging:1)          → safe warmth!
Minute 25:    Move bed inside shelter, fire pit inside or adjacent
              → Sleep warm, sheltered, fed (berries), with tools
```
