# Survival Feature Priority Order

> Master priority list for the survival mode feature roadmap. Updated 2026-02-25.

---

## Done (moved to `docs/done/`)

- **debugging-workflow.md** — Event log + state audit + F5/F7/F8 keys
- **medium-features-backlog.md** — Mud + cleaning done (weeding needs farming)
- **hunger-and-food.md** — Core hunger, auto-eat, starvation death, berries
- **01-workshop-construction-costs.md** — Workshop material costs + build time + deconstruction (save v63-v64)
- **02-tool-quality-and-digging-stick.md** — Quality framework (5 types, 3 levels), 4 tool items, speed scaling, tool seeking, full bootstrap (save v65-v66, 112 tests)
- **03-primitive-shelter-and-doors.md** — CELL_DOOR, leaf/stick walls, leaf/bark roofs, leaf/plank doors, ground fire + fire pit workshops (save v67, 30 tests). Deferred: fire spread + fire pit tool gate → `docs/todo/environmental/fire-improvements.md`
- **04a-butchering-and-cooking.md** — Carcass → butcher workshop → raw meat → cook at fire pit → eat. 4 new items (carcass, raw meat, cooked meat, hide)
- **04b-hunting.md** — Hunt designation (W→U), chase & kill job driver, flee behavior, WorkGiver + 14 tests
- **04c-root-foraging.md** — 3 root items (raw/roasted/dried), dig designation on dirt/clay/peat, roast at fire pit, dry at drying rack (save v70)
- **04d-spoilage.md** — Spoilage timer, ItemCondition (FRESH/STALE/ROTTEN), container modifiers, rotten=fuel, refuse pile (save v71-v73)
- **04e-animal-respawn.md** — Edge-spawn from map edges on timer, population cap + UI toggle (save v74)
- **05a-explore-designation.md** — Straight-line Bresenham scouting job (save v75)
- **05b-fog-of-war-exploration.md** — exploredGrid, soft borders, discovery events, blocks jobs/hauling in unexplored areas (save v75)
- **06a-farming-land-work.md** — Farm grid, tilling, weeding, composting, fertilizing. FarmCell struct, 5 soil types, compost pile workshop (save v76, 35 tests)
- **06b-farming-crops-processing.md** — 3 crops (wheat/lentils/flax), growth system with 5 modifiers, planting+harvesting jobs, wild plants in worldgen, quern workshop, cooking recipes. 9 new items, seed stack splitting (save v77, 35 farming tests)
- **07-clothing-and-textiles.md** — Loom + tailor workshops, 7 clothing items (grass/hide/cloth/linen), equip system, temperature integration, upgrade logic (save v78, 30+ tests)
- **08-thirst-and-water.md** — Thirst need, 3 liquid items (water/tea/juice), drink from pots + natural water fallback, fill water pot job, beverage recipes at campfire/hearth (save v79, 43 tests)
- **09-loop-closers.md** — Glass (sand→kiln→window), lye (ash→hearth), mortar (lye+sand→stonecutter→wall). Mud/cob crafting, reeds. CELL_WINDOW transmits light. (save v80-v82, 14+18+15 tests)

## Reference Docs (not actionable tasks)

- **naked-in-the-grass.md** — Gap analysis / vision doc with updated addendum
- **naked-in-the-grass-details.md** — Deep dives on tool qualities + animals design
- **workshop-evolution-plan.md** — Architecture roadmap (templates → stations → workspaces)

---

## Build Order

### ~~Tier 0: Prerequisites (unblock everything)~~ — COMPLETE ✅

| # | Doc | Effort | Deps | Status |
|---|-----|--------|------|--------|
| 01 | `01-workshop-construction-costs.md` | ~1 session | None | ✅ Done (save v63-v64) |
| 02 | `02-tool-quality-and-digging-stick.md` | ~2.5 sessions | None | ✅ Done (save v65-v66) |

### ~~Tier 1: Core Survival Loop (playable day-1)~~ — COMPLETE ✅

| # | Doc | Effort | Deps | Status |
|---|-----|--------|------|--------|
| 03 | `03-primitive-shelter-and-doors.md` | ~2 sessions | ~~01, 02~~ ✅ | ✅ Done (save v67) |
| 04a | `04a-butchering-and-cooking.md` | ~1 session | 03 ✅ | ✅ Done. Carcass → butcher → raw meat → cook → eat. |
| 04b | `04b-hunting.md` | ~1 session | 04a ✅ | ✅ Done. Hunt designation (W→U), chase & kill, flee behavior, 14 tests. |
| 04c | `04c-root-foraging.md` | ~1 session | 03 ✅ | ✅ Done. 3 root items, dig designation on soil, roast at fire pit, dry at drying rack. |
| 04d | `04d-spoilage.md` | ~0.5 session | 04a ✅ | ✅ Done. Spoilage timer, ItemCondition, container modifiers, rotten=fuel, refuse pile (save v71-v73). |
| 04e | `04e-animal-respawn.md` | ~0.5 session | None | ✅ Done. Edge-spawn, population cap, UI toggle (save v74). |
| 05a | `05a-explore-designation.md` | ~1 session | None | ✅ Done. Bresenham scouting job. |
| 05b | `05-fog-of-war-exploration.md` | ~2.5 sessions | 05a ✅ | ✅ Done. exploredGrid, soft borders, discovery events (save v75). |

### ~~Tier 2a: Farming~~ — COMPLETE ✅

| # | Doc | Effort | Deps | Status |
|---|-----|--------|------|--------|
| 06a | `06a-farming-land-work.md` | ~1.5 sessions | 02, 04d | ✅ Done. Farm grid, tilling, weeding, compost pile, fertilizing (save v76). |
| 06b | `06b-farming-crops-processing.md` | ~2 sessions | 06a | ✅ Done. 3 crops, growth, plant/harvest jobs, wild plants, quern, cooking (save v77). |

### ~~Tier 2b: Expansion~~ — COMPLETE ✅

| # | Doc | Effort | Deps | What It Does | Status |
|---|-----|--------|------|-------------|--------|
| 06c | `06c-farm-watering.md` | ~0.5 session | 06a | Manual water collection + carrying to farms. Optional — rain works for now. | Todo (deferred) |
| 07 | `07-clothing-and-textiles.md` | ~2 sessions | 03, 06b | Loom + tailor. Grass/cordage → cloth → clothing. Hides → leather. Flax fiber from 06b → linen. Cold resistance on movers. | ✅ Done (save v78) |
| 08 | `08-thirst-and-water.md` | ~1.5 sessions | 02 | Thirst need + water carrying in pots + beverages (tea, juice) + natural water fallback. | ✅ Done (save v79) |

### Tier 3: Polish & Depth

| # | Doc | Effort | Deps | What It Does |
|---|-----|--------|------|-------------|
| 09a | `09-loop-closers.md` | ~2 sessions | 06 | ✅ Done. Glass (sand→kiln→window), lye (ash→hearth), mortar (lye+sand→stonecutter→wall). CELL_WINDOW transmits light. Save v80-v82. |
| 09b | `09-onboarding-and-progression.md` | ~1-2 sessions | 07, 08 | Starting scenarios, progressive need unlocks, difficulty, visual priority. Reduces early-game overwhelm. |
| 10 | `10-personality-and-skills.md` | ~3 sessions | All | Names, skill progression (8 types), moods. The "character" layer. |
| 11 | `11-stockpile-filter-redesign.md` | ~1 session | More items | Hierarchical filter UI. Only matters with many more item types. |

### Standalone

| Doc | Notes |
|-----|-------|
| `feature-03-doors-shelter.md` | Original F03 design. Superseded by `docs/done/03-primitive-shelter-and-doors.md`. Keep as reference. |
| `feature-04-tools-knapping.md` | Original F04 design. Superseded by `02-tool-quality-and-digging-stick.md`. Keep as reference. |
| `04-cooking-and-hunting.md` | Original F04-food design. Split into 04a-04e on 2026-02-22. Keep as reference. |

---

## Session Plan

```
Session 1-2:    Workshop construction costs                        ✅ DONE
                → Workshops require materials + build time
                → Primitive workshops (campfire, drying rack, rope maker) cost only sticks

Session 2-4:    Tool quality framework + tool items                ✅ DONE
                → Quality enum, IF_TOOL, equippedTool on Mover, speed formula
                → Sharp stone gains cutting:1, craft digging stick/axe/pick/hammer
                → Every existing job immediately benefits from tools

Session 5-6:    Primitive shelter + CELL_DOOR + ground fire / fire pit  ✅ DONE
                → Stick walls, leaf roofs, leaf doors from gathered materials
                → Ground fire (3 sticks) + fire pit (5 sticks + 3 rocks)
                → Day-1 loop complete: gather → shelter → fire → sleep → survive

Session 7:      Butchering & cooking (04a)                             ✅ DONE
                → Carcass → butcher workshop → raw meat → cook at fire pit → eat
                → 4 new items (carcass, raw meat, cooked meat, hide)

Session 7-8:    Hunting (04b)                                          ✅ DONE
                → Hunt designation (W→U), chase & kill job, flee behavior, 14 tests

Session 8:      Root foraging (04c)                                      ✅ DONE
                → 3 root items, dig designation on soil, roast/dry roots

Session 8-9:    Spoilage + animal respawn (04d, 04e)                     ✅ DONE
                → Spoilage timer + ItemCondition + animal respawn all complete

Session 9:      Explore designation (05a)                                    ✅ DONE
                → Straight-line Bresenham scouting, no pathfinding

Session 10-11:  Fog of war (05b)                                             ✅ DONE
                → All phases complete (grid, rendering, designations, hauling, soft borders, discovery events)

Session 12-13:  Farming land work (06a) + crops & processing (06b)           ✅ DONE
                → Farm grid, tilling, weeding, compost pile, fertilizing (06a)
                → 3 crops (wheat/lentils/flax), growth system, wild plants, quern, cooking (06b)
                → 9 new items, 35 farming tests, save v76-v77

Session 14:     Clothing & textiles (07)                                        ✅ DONE
                → Loom + tailor, 7 clothing items, equip system, temp integration

Session 15:     Thirst & water (08)                                             ✅ DONE
                → Thirst need, 3 liquids, drink from pots/nature, fill pot job, beverages

Session 16:     Loop closers (09a)                                                ✅ DONE
                → Mud/cob crafting, reeds, glass/lye/mortar. CELL_WINDOW. Save v80-v82.

Session 17+:    Onboarding → personality
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
