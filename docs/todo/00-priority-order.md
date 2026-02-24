# Survival Feature Priority Order

> Master priority list for the survival mode feature roadmap. Updated 2026-02-24.

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
| 04d | `04d-spoilage.md` | ~0.5 session | 04a ✅ | ✅ Done. Spoilage timer, ItemCondition (FRESH/STALE/ROTTEN), container modifiers, rotten=fuel, rejectsRotten stockpile + refuse pile shortcut (save v71-v73). See also `gameplay/seasoning-curing.md` for broader material conditioning — future work. |
| 04e | `04e-animal-respawn.md` | ~0.5 session | None | ✅ Done. Edge-spawn from map edges, population cap, UI toggle, save v74. |
| 05a | `05a-explore-designation.md` | ~1 session | None | ✅ Done. Straight-line Bresenham scouting job. Click → mover walks blind toward target, stops at obstacles. No pathfinding. |
| 05b | `05-fog-of-war-exploration.md` | ~2.5 sessions | 05a ✅ | **In progress** — Phase 1 (grid + rendering + save/load) ✅, Phase 2 (block designations) ✅, Phase 3-5 remaining. |

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

Session 10-11:  Fog of war (05b)                                             🔧 IN PROGRESS
                → Phase 1: exploredGrid + rendering + save v75               ✅ DONE
                → Phase 2: block designations on unexplored                  ✅ DONE
                → Phase 3: block hauling + job assignment                    ⬜ TODO
                → Phase 5: polish (frontier edge, discovery events)          ⬜ TODO

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
