# Next Steps Analysis

External review and validation of our jobs-roadmap.md and related documentation.

---

## Validation: Our Roadmap is On Track

The reviewer confirmed our documentation points at the right next moves:

### Agreed Priority Order

1. **UI for existing systems** - Draw/remove stockpiles, gather zones, filter/priority config
   - Fastest way to validate hauling rules before adding more job types

2. **Make current system harder to break**
   - Reservation timeouts / orphan cleanup
   - Cheap reachability gate (connectivity regions)
   - Start moving toward queuing at bottlenecks (matters for doors/elevators/stairs)

3. **Mining as first "real" new job**
   - Designations → job generation → terrain modification → items → hauling

4. **Architecture refactor before Construction**
   - Job pool + drivers + WorkGivers
   - Otherwise every new job explodes Mover fields and JOB_* states

5. **Then:** Construction → Crafting → Farming → Skills/Priorities

---

## Mechanics Worth "Stealing"

### From RimWorld

**1. Bills / Recipes at Workstations**
- Modes like "Do until you have X"
- Ingredient radius limits
- Bills can be reordered/suspended
- Maps directly to our future workbench + hauling pipeline

**2. Stockpile Micro-Logistics as Gameplay**
- High-priority small stockpiles near benches
- Haulers refill them automatically
- We already have stockpile priorities and re-haul — this becomes player strategy

**3. Zones/Areas as Constraints**
- "Allowed area" / "home area" concepts
- Where pawns may go, where they clean/repair
- Adds meaningful player control without new content

### From Cataclysm DDA

**1. Zone Activities (Zone Manager)**
- Define zones and run activities within them (sorting, hauling)
- We have "gather zones" — next leap is "zones that generate jobs"
- Not just filtering hauling, but driving job creation

**2. Tool Qualities (Crafting Prerequisites)**
- Recipes requiring qualities (CUT, SCREW, etc.)
- Tools have charges/durability
- Adds depth without huge item list: the interesting part is the requirements graph

### From Factorio

**1. Requester/Provider Logistics**
- Network where "requesters" pull from providers/storage
- Clear priority rules
- **Colony version:** "Requester stockpiles" that generate jobs to maintain target amounts
- Example: "Keep 30 coal near the forge"

**2. Simple Circuit/Conditions**
- Enable/disable transfers based on conditions
- **Colony version:** 
  - "Only produce if below N"
  - "Only haul if overflow"
  - "Keep 20 meals near dining"

---

## Vertical Slices (Small, Playable Loops)

### Slice 1: Mining → Haul → Stockpile (DF Core Loop)
| | |
|---|---|
| **Player action** | Designate dig area |
| **Systems exercised** | Designation grid, reservations, terrain change, item spawn, hauling, unreachable handling |
| **Win condition** | Mined rocks reliably end up in correct stockpiles with no thrash |

**Status:** This is our Phase 2 (Mining) in jobs-roadmap.md ✓

### Slice 2: Blueprint Build with Delivery (RimWorld Construction)
| | |
|---|---|
| **Player action** | Place wall blueprint requiring X stone |
| **Systems exercised** | Chained jobs (deliver → build), partial progress, cancellation cleanup |
| **Win condition** | Builders never start without materials; haulers restock; build resumes after interruption |

**Status:** This is our Phase 3 (Construction) in jobs-roadmap.md ✓

### Slice 3: Workbench Bills (RimWorld Production)
| | |
|---|---|
| **Player action** | Set bill "Cook until you have 10 meals" |
| **Systems exercised** | Recipe inputs, ingredient search radius, output handling, stockpile counting |
| **Win condition** | Stable production with haulers keeping bench supplied |

**Status:** This is our Phase 4 (Crafting) in jobs-roadmap.md ✓
- Note: "Do until X" mode not explicitly in our doc — should add

### Slice 4: Zone Activities: Autosort (Cataclysm-style)
| | |
|---|---|
| **Player action** | Mark Unsorted zone + typed storage zones |
| **Systems exercised** | Zone-driven job generation, bulk hauling, priority rules |
| **Win condition** | Dumping loot somewhere turns into organized base without micromanagement |

**Status:** NEW — not in our roadmap
- Our gather zones are passive filters, not job generators
- This is a natural extension: zones that create sorting jobs

### Slice 5: Requester Stockpile (Factorio Logistics with Pawns)
| | |
|---|---|
| **Player action** | Set stockpile near forge to "keep 30 coal" |
| **Systems exercised** | Demand-driven hauling, source prioritization, anti-thrash rules |
| **Win condition** | Buffer stays topped up automatically — feels like automation without belts |

**Status:** PARTIALLY in our roadmap
- We have stockpile priorities (pull from lower to higher)
- "Keep N items" mode is new — could be added to stockpile config
- This is Work Orders lite (from jobs-roadmap.md Phase 4.5)

---

## Cross-Reference: What's Missing from Our Docs

| Feature | In Our Docs? | Notes |
|---------|--------------|-------|
| Mining designations | ✓ jobs-roadmap.md | Phase 2 |
| Chained jobs (deliver → build) | ✓ jobs-roadmap.md | Phase 3.3 |
| Workshop bills | Partial | We have recipes, not "do until X" mode |
| Zone-driven job generation | ✗ | Our zones filter, don't generate |
| Requester stockpiles ("keep N") | ✗ | Natural extension of priority system |
| Tool qualities/prerequisites | ✗ | Could add depth to crafting |
| Queuing at bottlenecks | ✓ todo.md | Listed but not detailed |
| Connectivity regions | ✓ jobs-roadmap.md | In Technical Improvements |

---

## Recommended Additions to Roadmap

### Add to Phase 4 (Crafting):
```
- "Do until X" bill mode
- "Do forever" bill mode
- Bill pause/resume
- Ingredient search radius setting
```

### Add as Phase 4.5 or separate feature:
```
- Requester stockpiles ("maintain N items of type X")
- Zone activities (zones that generate jobs, not just filter)
```

### Add to stockpile config (Phase 1 UI):
```
- Target count: "try to keep N items here"
- This drives demand-based hauling without full Work Orders
```

---

## The Cleanest Next Step (Confirmed)

> "Build Stockpile + Gather Zone UI first, then implement Mining as the first designation-driven job."

This vertical slice forces solving the right general problems:
- Designations
- Job generation from designations
- Terrain edits
- Retries and cancellation

Before adding "fancier" job types.

---

## Summary

Our roadmap is validated and on track. Key additions to consider:

1. **Requester stockpiles** ("keep N here") — simple, powerful, no new job types
2. **"Do until X" bills** — makes crafting feel automated
3. **Zone activities** — zones that generate jobs, not just filter hauling

These fit naturally into our existing architecture and make the colony feel "smart" without requiring belts or robots.
