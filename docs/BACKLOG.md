# NavKit Development Backlog

> Last updated: 2026-02-15
> Philosophy: **Autarkic development** — add things because the world needs them, use what we build.

---

## The Arc

The game has strong industrial simulation (workshops, items, stockpiles, pathfinding) and rich environmental systems (water, fire, temperature, lighting, weather, seasons, wind, snow, mud). But **nobody eats, sleeps, gets cold, uses tools, or lives in buildings that matter**. The next 10 features fix this by chaining each system into the ones before it.

```
Feature 1:  Hunger & Foraging ──── uses berry bushes, stockpiles, seasons
    │
Feature 1B: Containers & Storage ─ uses rope maker, kiln, construction
    │
Feature 2:  Sleep & Furniture ──── uses sawmill, planks, construction
    │
Feature 3:  Doors & Shelter ────── uses construction, weather, temperature
    │
Feature 4:  Tools & Knapping ───── uses rocks, sticks, cordage, all jobs
    │
Feature 5:  Cooking & Hunting ──── uses fire, hearth, animals, hunger
    │
Feature 6:  Farming & Crops ────── uses weather, seasons, water, soil, tools
    │
Feature 7:  Clothing & Textiles ── uses cordage, rope maker, temperature
    │
Feature 8:  Water Carrying ─────── uses clay pots (from 1B), water system
    │
Feature 9:  Loop Closers ───────── uses sand, leaves, ash (glass, compost, lye)
    │
Feature 10: Personality & Skills ── uses all needs, all jobs, all workshops
```

Each feature **closes loops** opened by previous work and **opens loops** that later features close. No feature exists in isolation.

---

## Tier 1: Survival Loop (Features 1-3)

These make the colony a place where *staying alive* matters.

| # | Feature | Closes | Opens | Existing Systems Used | Doc |
|---|---------|--------|-------|----------------------|-----|
| 1 | **Hunger & Foraging** | Movers are immortal robots | Need for food production | Berry bushes, stockpiles, drying rack, seasons | `doing/feature-01-hunger-foraging.md` |
| 1B | **Containers & Storage** | Food rots on dirt, no storage | Preservation, stockpile upgrades | Rope maker, kiln, construction | `doing/feature-1b-containers-storage.md` |
| 2 | **Sleep & Furniture** | Movers never rest | Need for buildings with purpose | Sawmill, planks, construction system | `doing/feature-02-sleep-furniture.md` |
| 3 | **Doors & Shelter** | Buildings are open mazes | Weather matters for interior spaces | Construction, weather, temperature, lighting | `doing/feature-03-doors-shelter.md` |

## Tier 2: Tool & Production Loop (Features 4-6)

These make *how* you do things matter — progression through better tools and food chains.

| # | Feature | Closes | Opens | Existing Systems Used | Doc |
|---|---------|--------|-------|----------------------|-----|
| 4 | **Tools & Knapping** | Everything done bare-handed | Speed progression, tool recipes | Rocks, sticks, cordage, all job types | `doing/feature-04-tools-knapping.md` |
| 5 | **Cooking & Hunting** | Animals are decoration, raw food only | Meat economy, fuel demand | Fire, hearth, animals, hunger system | `doing/feature-05-cooking-hunting.md` |
| 6 | **Farming & Crops** | Foraging doesn't scale | Seasonal food pressure, fertilizer demand | Weather, seasons, water, soil, mud, tools | `doing/feature-06-farming-crops.md` |

## Tier 3: Material & Comfort Loop (Features 7-9)

These close open material loops and make the environment affect colonists.

| # | Feature | Closes | Opens | Existing Systems Used | Doc |
|---|---------|--------|-------|----------------------|-----|
| 7 | **Clothing & Textiles** | Temperature doesn't affect colonists | Textile production chain | Cordage, rope maker, drying rack, temperature | `doing/feature-07-clothing-textiles.md` |
| 8 | **Water Carrying & Thirst** | Water is decoration, nobody drinks | Water logistics, settlement location | Clay pots (from 1B), water system | `doing/feature-08-containers-water.md` |
| 9 | **Glass, Compost & Loop Closers** | Sand, leaves, ash have no sinks | Closes ALL open item loops | Sand, leaves, ash, kiln, farming | `doing/feature-09-loop-closers.md` |

## Tier 4: Living Colony (Feature 10)

This makes the colonists feel alive.

| # | Feature | Closes | Opens | Existing Systems Used | Doc |
|---|---------|--------|-------|----------------------|-----|
| 10 | **Personality, Skills & Moods** | Movers are identical robots | Storytelling, emergent drama | All needs, all jobs, all workshops | `doing/feature-10-personality-skills.md` |

---

## Open Bugs (Carry Forward)

| Bug | Status | Impact | Doc |
|-----|--------|--------|-----|
| Wall-build mover pop | OPEN | Minor (visual glitch) | `bugs/wall-build-pop.md` |
| Stockpile priority performance | DEFERRED | Medium (wrong haul targets) | `done/design/stockpile-priority-performance.md` |
| High-speed stuck movers | OPEN | Minor (speed 100 only) | `bugs/high-speed-stuck.md` |

## Small Items & Polish (Do Anytime)

These are independent improvements that can slot in between features:

- [ ] Move lighting system docs to done/ (already complete)
- [ ] Merge duplicate docs (stairs, wild plants, soil fertility)
- [ ] Remove ACTION_DRAW_DIRT/ROCK duplication (keep soil submenu only)
- [ ] Update stale docs (jobs-roadmap, overview, material-based-rendering)
- [ ] Add visual "why blocked" feedback for workshops
- [ ] Smooth camera movement
- [ ] Particle effects for fire/smoke/rain

## Research & Long-Term Vision

These are bigger architectural changes for later:

| Project | Scope | Depends On | Doc |
|---------|-------|-----------|-----|
| Reverse-time / Rewind | ~1000 LOC | Deterministic RNG | `research/reverse-time-feature.md` |
| Infinite world / Chunks | Large | Delta persistence | `vision/infinite-world-exploration.md` |
| Steam power & automation | Large | Tools, fuel loops | `doing/discussions with ai/steampower.md` |
| Conveyor belts | Large | Steam power, workshops | `doing/discussions with ai/grand colony.md` |
| Freeform workshops | Design shift | More workshop experience | `doing/discussions with ai/notdfworkshops.md` |
| Waste & fertilizer loop | Medium | Hunger, farming | `doing/discussions with ai/wate into resurce.md` |
| Sound & communication | Medium | Personality system | `todo/sounds/` |
| Colony cycle / Exodus | Vision | Most systems mature | `vision/vision.md` |

---

## Material Loop Status

Quick reference: which items have sources and sinks?

| Item | Source | Sink | Status |
|------|--------|------|--------|
| ROCK | Mining | Stonecutter → blocks, construction | ✅ Closed |
| LOG | Chopping | Sawmill → planks/sticks/bark/stripped | ✅ Closed |
| PLANKS | Sawmill | Construction | ✅ Closed |
| STICKS | Sawmill | Charcoal pit → charcoal | ✅ Closed |
| BARK | Sawmill (strip) | Rope maker → string | ✅ Closed |
| STRIPPED_LOG | Sawmill (strip) | Sawmill → planks | ✅ Closed |
| SHORT_STRING | Rope maker | Rope maker → cordage | ✅ Closed |
| CORDAGE | Rope maker | Construction | ✅ Closed |
| CLAY | Mining | Kiln → bricks | ✅ Closed |
| BRICKS | Kiln | Construction | ✅ Closed |
| BLOCKS | Stonecutter | Construction | ✅ Closed |
| GRASS | Harvesting | Drying rack → dried grass | ✅ Closed |
| DRIED_GRASS | Drying rack | Rope maker → string | ✅ Closed |
| CHARCOAL | Charcoal pit | Fuel (kiln, hearth) | ✅ Closed |
| DIRT | Mining | Construction (floors) | ✅ Closed |
| GRAVEL | Mining | Stonecutter (crush/bind) | ✅ Closed |
| SAPLING | Gathering | Planting | ✅ Closed |
| LEAVES | Tree gathering | **No sink** | ❌ Needs: composting (F9) |
| ASH | Hearth | **No sink** | ❌ Needs: lye/cement (F9) |
| POLES | Branch chopping | **No sink** | ❌ Needs: construction/scaffolding |
| SAND | Mining | **No sink** | ❌ Needs: glass kiln (F9) |
| PEAT | Mining | Charcoal pit / Kiln fuel | ✅ Closed |

---

## How to Use This Document

1. **Pick the next feature** from the tier you're in (go in order within tiers)
2. **Read the feature doc** in `docs/doing/feature-XX-*.md`
3. **Implement** following the phases in the doc
4. **Test** with `make test`
5. **Move the feature doc** to `docs/done/` when complete
6. **Update this backlog** (check off, update material loop status)
