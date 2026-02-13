# Blind Spots & Fresh Todo Ideas

*Generated 2026-02-13 after comprehensive codebase and documentation analysis*

This document catalogs **features and systems that haven't been thought about yet** - gaps between what's implemented and what would make the game more complete, polished, and playable. Focus is on "implementable now" ideas that fit the current scope, not far-future vision items.

---

## The Blind Spots (What's Missing)

### 1. User Experience / Quality of Life (Almost completely unaddressed)
- No "undo" system for misplaced buildings
- No copy/paste for repeated patterns (place 5 sawmills = 5 manual operations)
- No building rotation
- No keyboard shortcuts for switching item types while drawing
- No quick-select for "build 5 of X"
- No visual feedback when action is blocked (why can't I build here?)
- No tutorial/onboarding system
- No save slots or save management UI

### 2. Mover Behavior & Personality (Completely missing)
- No mover names or identity
- No skill levels or specialization (all movers identical)
- No preferences (favorite food, likes/dislikes)
- No relationships between movers
- No mood system (all movers emotionless)
- No fatigue/rest mechanics
- No injury/health system
- No "thoughts" display (why is this mover idle?)

### 3. Feedback & Observation (Major gaps)
- No production statistics (items crafted per minute/hour)
- No resource usage graphs
- No alerts/notifications (stockpile empty, workshop idle)
- No mover activity summary (what are my 20 movers doing right now?)
- No bottleneck detection UI
- No "where did all my logs go?" tracing
- No consumption vs production dashboard

### 4. Immediate Gameplay Loops (Surprising gaps given the depth)
- **No food/eating**: Huge gap - movers don't eat
- **No sleep/beds**: Movers never rest
- **No storage furniture** (chests, barrels, bins) - everything on ground/stockpiles
- **No doors/access control**: Can't restrict areas
- **No zones beyond stockpiles** (meeting areas, forbidden zones)
- **No seasonal gameplay** (winter = different challenges)
- **No threats/challenges**: Nothing attacks, no accidents, no disasters

### 5. Environmental Interaction (Partially done)
- No rain/weather affecting work speed or visibility
- No wind (despite being mentioned in vision docs)
- No snow accumulation in winter
- No mud (dirt + water interaction missing despite both systems existing)
- No evaporation visual feedback (puddles shrinking)
- No plant wilting from lack of water
- No flooding damage to items/workshops

### 6. Intermediate Production Chains (Clear gaps in current item roster)
- **No glass production** (sand + fuel → glass documented but not implemented)
- **No tool items** (axes, pickaxes, hammers) - mentioned in vision but absent
- **No containers** (barrels, crates, bags)
- **No furniture items** (beds, chairs, tables exist in vision but not in items.c)
- **No textiles beyond cordage** (cloth, fabric, thatch missing)
- **No food items** (berries, fruit, crops completely absent)

### 7. Workshop Usability (System complete, UX not)
- No workshop naming/labels
- No "forbid" toggles on specific workshops
- No priority slider for bills (all bills equal priority within workshop)
- No "use only materials from stockpile X" bill constraints
- No visual indication of recipe options when hovering workshop type
- No workshop stats panel (items produced, time active vs idle)

### 8. Construction Ergonomics (Building is clunky)
- No building templates/blueprints that can be saved and reused
- No "plan" mode (designate construction without requiring materials upfront)
- No construction priority system (build this wall first)
- No multi-select for designations (must drag each designation separately)
- No area commands (build walls around this rectangle)

### 9. Polish & Juice (Almost completely absent)
- No sound effects (footsteps, hammering, chopping, crackling fire)
- No particle effects (sawdust, wood chips, steam puffs, dirt clouds)
- No animations beyond mover movement (trees sway? workshops have moving parts?)
- No ambient environmental sounds (wind, water, birds)
- No camera shake or screen effects
- No day/night transition effects beyond color
- No seasonal palette changes

### 10. Meta-Game Loops (Nothing here)
- No victory conditions
- No challenges/scenarios
- No progression unlocks
- No achievements/milestones
- No difficulty settings
- No game modes (sandbox vs survival vs campaign)

---

## Fresh Todo Ideas (Implementable Now)

### High Priority UX Wins

**1. Undo system**
- Single-level undo for last building placed (fixes misclicks)
- Store last action (type, position, materials consumed)
- Keybind: Ctrl+Z

**2. Building labels**
- Right-click workshop/stockpile to name it ("Oak Sawmill", "Stone Storage")
- Display label on hover or always visible
- 32-char max per label

**3. Visual why-blocked feedback**
- Red overlay + tooltip when can't build ("No materials", "Blocked by wall", "Too close to other workshop")
- Shows during drag preview before placement

**4. Mover activity panel**
- Shows list of all movers with current job type in real-time
- Click mover in list to center camera on them
- Color-code by job category (hauling=blue, crafting=yellow, idle=red)

**5. Idle mover indicator**
- Visual highlight (bouncing? different color?) for movers with no job
- Option to auto-pause game when N movers idle (configurable threshold)

---

### Immediate Gameplay Depth

**6. Hunger system (MVP)**
- Movers need food every 30 game-minutes
- JOBTYPE_EAT: walk to stockpile with food items → consume 1 food item
- New item: ITEM_BERRIES (gathered from berry bushes)
- Starvation: after 60 minutes without food, mover moves 50% slower

**7. Sleep system (MVP)**
- Movers need 8 hours rest per 24 game-hours
- JOBTYPE_SLEEP: walk to bed (or ground if no bed) → sleep for duration
- Sleeping on ground = 8 hours needed; on bed = 6 hours needed
- Sleep deprivation: after 48 hours, mover moves 30% slower

**8. Weather system (visual only)**
- Rain overlay (animated rain sprites)
- Rain slows outdoor work by 20% (indoor workshops unaffected)
- Rain chance: 10% per game-hour, lasts 5-15 minutes
- Future: rain adds water to cells (integrates with water system)

**9. Seasons**
- Four seasons (spring/summer/autumn/winter), 7 game-days each
- Palette shifts: spring=green, summer=bright, autumn=orange, winter=blue-white
- Winter: tree growth 50% slower, grass doesn't regrow
- Summer: evaporation 2x faster

**10. Item decay on ground**
- Items left outside (not in stockpile) for 24+ game-hours start decaying
- Decay stages: fresh → weathered (50% transparency) → gone
- Organic items (logs, grass, food) decay faster (12 hours)
- Encourages proper storage and stockpile use

---

### Production Chain Gaps

**11. Implement glass kiln**
- Recipe: SAND (1) + fuel (1) → GLASS (2) | 6s work
- Glass used for future recipes (windows, bottles, lenses)
- Closes autarky loop for sand (currently sand has no sink)

**12. Add berry bushes**
- New vegetation type: CELL_BERRY_BUSH
- Produces ITEM_BERRIES every 60 game-seconds (max 3 berries per bush)
- JOBTYPE_GATHER_BERRIES: walk to bush → pick berries (2s)
- Can be planted (ITEM_BERRY_BUSH_SAPLING from mature bushes)

**13. Add basic tools**
- ITEM_AXE (recipe: ROCK + STICK → AXE | stonecutter, 4s)
- ITEM_PICKAXE (recipe: ROCK + STICK → PICKAXE | stonecutter, 4s)
- Movers with tools get 20% speed bonus on relevant jobs (axe=chop, pickaxe=mine)
- Tools have durability: 100 uses before breaking

**14. Add storage containers**
- ITEM_CRATE (recipe: PLANKS (3) → CRATE | carpentry bench, 5s)
- Placeable furniture: stores 10 items in 1 tile (acts as mini-stockpile)
- Can be carried (haul job moves crate + contents)
- Future: barrels (for liquids), bags (for powders)

**15. Implement mud**
- Dirt cell + water (level 3+) = mud (cell flag: HAS_MUD)
- Mud slows movement by 50%
- Mud dries to dirt after 5 game-minutes without water
- Future: mud + straw → cob (building material)

---

### Feedback & Observation

**16. Production stats panel**
- Panel showing items crafted in last game-hour per workshop type
- Bar graph: items produced vs items consumed
- Click item type to highlight all producers/consumers

**17. Stockpile alerts**
- Yellow outline when stockpile < 20% capacity
- Red outline when stockpile empty for 5+ minutes and workshop needs input
- Click alert to jump to problem stockpile

**18. Item consumption tracer**
- Click item type in UI → highlights all workshops that use it as input
- Shows flow: source (tree/mine) → stockpile → workshop → output
- Helps answer "where did all my logs go?"

**19. Path failure notification**
- When mover can't reach job target 3+ times, show alert
- Alert shows: mover name, job type, unreachable tile (highlighted in red)
- Click alert to center camera on problem area

**20. Workshop efficiency meter**
- Per-workshop stat: % time workshop had active worker
- Low efficiency (<50%) = needs more crafters or better stockpile placement
- Shown in workshop tooltip

---

### Polish & Juice

**21. Footstep sounds**
- Simple step sounds when movers walk
- Varies by terrain: soft (grass), hard (stone), splash (water)
- Pitch variation for variety

**22. Construction particles**
- Brief dust puff when building placed
- Color matches material (brown for dirt, gray for stone)
- Fades over 0.5 seconds

**23. Workshop work sounds**
- Hammering for stonecutter (rhythmic metal clinks)
- Sawing for sawmill (wood scraping)
- Crackling for kiln (fire sounds)
- Loops while workshop active

**24. Seasonal ambience**
- Bird sounds in spring (chirping)
- Crickets in summer (evening)
- Wind in winter (howling)
- Rain sounds during weather

**25. Camera pan to event**
- Double-click notification/alert to center camera on problem area
- Smooth camera transition (0.5s lerp)
- Highlight target with pulsing circle for 2 seconds

---

## Priority Matrix (Impact vs Effort)

### Quick Wins (High Impact, Low Effort):
- #1 Undo system
- #3 Visual why-blocked feedback
- #5 Idle mover indicator
- #17 Stockpile alerts
- #22 Construction particles

### Core Gameplay (High Impact, Medium Effort):
- #6 Hunger system
- #7 Sleep system
- #11 Glass kiln
- #12 Berry bushes
- #15 Mud implementation

### Quality of Life (Medium Impact, Low Effort):
- #2 Building labels
- #4 Mover activity panel
- #19 Path failure notification
- #20 Workshop efficiency meter
- #21 Footstep sounds

### Polish (Medium Impact, Medium Effort):
- #8 Weather system
- #9 Seasons
- #23 Workshop work sounds
- #24 Seasonal ambience
- #25 Camera pan to event

### Foundation for Future (Low Impact Now, High Impact Later):
- #10 Item decay (enables food spoilage later)
- #13 Basic tools (enables tool progression)
- #14 Storage containers (enables inventory system)
- #16 Production stats panel (enables economy balancing)
- #18 Item consumption tracer (enables supply chain optimization)

---

## Notes

- This list focuses on **near-term implementable features** that fill current gaps
- Excludes far-future vision items (monorails, choppers, infinite worlds)
- Prioritizes gameplay feel and usability over grand systems
- Many items have natural expansion paths (MVP → full feature)
- Cross-references with existing systems where possible (mud uses water+dirt, tools use existing items)

**Next Steps:**
1. Pick 3-5 items from "Quick Wins" or "Core Gameplay"
2. Expand into full implementation plans
3. Implement and test
4. Iterate based on gameplay feel

---

## NICHE BLIND SPOTS: The Really Weird Stuff

*Even MORE specific edge cases, quirks, and undocumented behaviors that would surprise both users and developers*

### 1. MICRO-INTERACTIONS & EMERGENT BEHAVIORS

**Knot Bug System** (Undocumented Edge Case)
- Movers can oscillate near waypoints due to sub-cell positioning conflicts with avoidance
- The "Knot Fix" system exists but is undocumented: `KNOT_FIX_ARRIVAL_RADIUS`, `KNOT_NEAR_RADIUS`, `KNOT_STUCK_TIME`
- Avoidance scales **quadratically** near waypoints (distance²) creating "patience zones"
- At 16px arrival radius, movers advance without snapping position but tolerate more congestion
- **Missing Feature**: No visual indicator when movers are in "knot" state

**Mover Self-Deactivation** (Silent Failure)
- Movers stuck in solid cells with no escape route **silently disappear**
- Only logs `LOG_WARNING` to console - no UI feedback
- Escape attempts: z+1 (up), then 8 adjacent neighbors (cardinal + diagonal)
- **Missing Feature**: Alert when mover deactivates, or auto-rescue system

**Falling Mechanics** (Complex & Undocumented)
- Movers can fall sideways through up to 4 neighbors at each z-level (emergency escape)
- Fall detection: "actual fall" (needs ground) vs "ramp descent" (no penalty)
- `fallTimer` is purely visual - doesn't affect gameplay
- **Missing Feature**: Fall damage, items dropped when falling

**Wall Repulsion Can Push Into Air** (Intentional Design?)
- Wall repulsion pushes movers away from solid cells
- Can push movers into air cells causing falls - intentional deadlock breaker
- `allowFallingFromAvoidance` bypasses safety filters
- **Missing Feature**: Movers avoid edges/cliffs unless intentionally jumping

---

### 2. TERRAIN & PHYSICS QUIRKS

**Ground Wear Creates Natural Paths** (Subtle Beauty)
- Movers trample: tall grass → short grass → trampled → bare → dirt
- Saplings trampled after **half max wear** accumulation
- Creates visible traffic patterns showing mover routes
- Fire consumes grass overlays as fuel (accelerates spread on meadows)
- **Missing Features**: 
  - Time-lapse view of path formation
  - Intentional path designation (flagstone paths that resist wear)
  - Mover preference for existing paths (faster movement)

**Water Displacement When Building** (Undocumented)
- Placing walls in water pushes water to horizontal neighbors first, then up
- Can accidentally flood adjacent areas when building
- **Missing Features**:
  - Warning before displacing large amounts of water
  - Drainage planning tools
  - "What would flood if I remove this wall?" preview

**Dirt Tracking Stone Floor Discount** (Hidden Mechanic)
- Stone floors accumulate 50% dirt rate vs other materials (hardcoded)
- No UI indication that stone is "better" for high-traffic areas
- **Missing Features**:
  - Material properties tooltip (show dirt resistance, wear resistance)
  - Floor material matters for other stats (construction time, beauty, temperature)

**Frozen Water Blocks Flow Via Flag** (No Ice Cells)
- Frozen water is just a flag on WaterCell, not separate ice cell type
- Ice doesn't become walkable terrain - just stops water flow
- **Missing Features**:
  - Ice as walkable surface (slippery movement?)
  - Breaking ice to release water underneath
  - Ice harvesting for cold storage

---

### 3. PATHFINDING QUIRKS THAT BREAK INTUITION

**String Pulling Corridor Check** (Subtle Bug Prevention)
- Path simplification checks **4 cardinal neighbors** of start point, not just direct LOS
- Prevents corner-grazing at sub-cell positions (classic DF bug)
- Creates asymmetry: path valid even if center-to-center fails
- **Missing Feature**: Visual debug mode showing why path was/wasn't simplified

**Ramp Direction Mismatches** (Silent Failure)
- Can place ramps facing wrong direction (south ramp accessed from north)
- Pathfinder finds route, but movement system can't execute
- Mover gets stuck repeatedly trying invalid ramp transition
- **Missing Features**:
  - Ramp placement validation (must have floor above in direction it points)
  - Visual arrows showing ramp direction
  - Auto-rotate ramps to face placement direction

**HPA* Silent Fallback to A*** (Performance Trap)
- If HPA* fails and ramps exist, automatically tries A* (no UI indication)
- Slow paths (>50ms) logged to console but game continues
- Creates invisible performance problems
- **Missing Features**:
  - Performance warning when fallback engaged
  - "Why is this path slow?" diagnostic tool
  - Auto-rebuild HPA graph when many fallbacks occur

---

### 4. JOB SYSTEM EDGE CASES

**timeWithoutProgress Doesn't Reset on Repath**
- Persists across path updates - only resets on actual movement (>1px)
- Slow movers can be falsely detected as stuck (3-second timeout)
- **Missing Feature**: Separate timers for "not moving" vs "path not progressing"

**Multi-Output Recipe Partial Delivery** (Silent Failure)
- Second output can silently fail if ANY of 3 storage checks miss it
- Item lost forever, game continues normally
- Storage checks in: diagnostics, bill resume, bill assignment
- **Missing Features**:
  - Alert when output can't be stored (not just suspend bill)
  - Temporary ground storage when stockpiles full
  - Overflow handling (store at workshop output tile)

**Item Pickup Radius vs Path Ending** (Micro-Positioning Bug)
- Pickup radius: 0.75 * CELL_SIZE (covers whole cell)
- Path can end mid-cell, final approach is direct movement (not pathfinding)
- Causes backward/sideways movement if already in cell with target
- **Missing Feature**: Snap to pickup position if within radius (no awkward repositioning)

---

### 5. HIDDEN DEVELOPER TOOLS & DEBUG MODES

**Console System** (Powerful But Undocumented)
- Full autocomplete (Tab key), arrow key history (ephemeral - not saved)
- Commands: `spawn`, `clear`, `list`, `help`, variable modification
- Output to screen AND stderr (terminal capture)
- **Missing Features**:
  - Command history persistence
  - Macro/script support for repetitive dev tasks
  - Console command to dump game state for bug reports

**Profiler UI** (Compile-Time Toggled)
- Hierarchical profiler with 120-frame history per section
- Nestable sections, collapsible UI, min/max/avg/last stats
- Can be compiled out entirely (`PROFILER_ENABLED = 0`)
- **Missing Features**:
  - Export profiler data to CSV for analysis
  - Frame-by-frame scrubbing
  - Automatic spike detection and alerting

**Debug Visualization Modes** (Scattered Implementation)
- `showKnotDetection`, `showStuckDetection`, `showOpenArea`, `showMoverPaths`
- `followMoverIdx` exists but camera tracking is instant, not smooth
- **Missing Features**:
  - Debug mode overlay panel (toggle all visualizations from one place)
  - Per-mover debug info (show path, job, reservations, stuck timers)
  - Time-travel debugging (scrub through last 60 seconds of simulation)

---

### 6. SENSORY FEEDBACK GAPS (The Feel Is Missing)

**No Physics Feedback**:
- Trees fall instantly (no animation, no screen shake, no dust)
- Items dropped from height snap to ground (no bounce, no fall time)
- Walls demolished instantly (no rubble, no collapse animation)
- Water placed at height doesn't splash down
- **Would Make It Feel Better**: Screen shake, particle effects, sound, animation tweens

**No Camera Juice**:
- Camera follow mode (`followMoverIdx`) is instant, not smooth
- No camera shake on impacts
- No zoom-in when selecting objects
- No "focus pull" when important events occur
- **Would Make It Feel Better**: Smooth lerp camera, easing functions, attention-grabbing zoom

**No Mover Personality**:
- All movers identical speed (1.0x base), no variance
- No idle animations (all frozen when not moving)
- No emotes or status indicators
- **Would Make It Feel Better**: Speed variance (0.9-1.1x), idle fidgeting, thought bubbles

---

### 7. EMERGENT SYSTEMS WITH MISSING PIECES

**Accidental Fire Chains** (Almost There)
- Fire accelerates (spread chance = base + level * perLevel)
- Grass adds fuel (+16 units) enabling meadow wildfires
- Water reduces spread by 50% but only if adjacent
- **Missing Features**:
  - Firebreaks (cleared ground stops fire spread)
  - Fire alerts when spreading near structures
  - Smoke accumulation choking movers in enclosed fires
  - Panic behavior (movers flee fire)

**Water Dams Work Naturally** (But No Triggers)
- Water pressure propagates via cardinal neighbors only
- Walls block pressure - natural dam containment
- **Missing Features**:
  - Floodgates (toggleable water barriers)
  - Lever system (remote triggers for gates)
  - Water wheels (power generation from pressure/flow)
  - Moat mechanics (defensive water barriers)

**Sapling Regrowth Disabled** (Complete But OFF)
- System exists: 0.05% per interval per tile, 4-tile min distance
- Material-aware: peat→willow, sand→birch, gravel→pine, dirt→oak
- Disabled by default (`saplingRegrowthEnabled = false`)
- **Missing Features**:
  - UI toggle for enabling regrowth
  - Regrowth rate slider (customize wilderness reclamation speed)
  - "Forest management" designation (prevent regrowth in cleared areas)

---

### 8. QUALITY OF LIFE MICRO-FEATURES (Death By 1000 Clicks)

**Missing Multi-Select**:
- Must designate every tree individually for chopping
- Must place every stockpile square one at a time
- Can't batch-cancel designations
- **Would Save Time**: Shift+click to select all of type in radius, drag-select for multiple

**No Designation Preview**:
- Can't see what will be mined/built before confirming
- No "ghost" preview of completed construction
- **Would Prevent Mistakes**: Transparent overlay showing result before committing

**No Undo/Redo**:
- Misclick = permanent (must manually fix)
- Accidentally delete workshop = lose all bills
- **Would Reduce Frustration**: Single-level undo for last 5 actions

**No Auto-Repeat/Chain Mode**:
- Must drag every floor tile individually
- Must click every workshop placement separately
- **Would Speed Up Building**: Hold shift = chain placement mode (auto-continue after placing)

---

### 9. QUIRKY PHYSICS CONSTRAINTS (The Devil's in the Details)

**Workshop Blocking** (Invisible Collision Layer)
- Workshops have `CELL_FLAG_WORKSHOP_BLOCK` (movers can't path through)
- Wall repulsion DOES push away from workshop blocks
- Creates scenarios where movers clip through corners but can't walk through center
- **Missing Feature**: Visual indication of workshop collision bounds

**Corner-Cutting Diagonal Movement**:
- Diagonal movement requires BOTH adjacent cells walkable
- Bresenham LOS has special diagonal corner check
- Prevents shortcuts through narrow gaps
- **Missing Feature**: Configurable collision size (fat movers vs thin movers)

**Ramp Placement Validation** (None Exists)
- Can place ramps in illogical configurations (pointing into walls)
- Pathfinder assumes valid setup but doesn't enforce
- **Missing Feature**: Auto-validate ramp placement or show error

---

### 10. PERFORMANCE TRICKS AFFECTING GAMEPLAY

**Staggered Mover Updates** (Default ON)
- Each mover updates on different frame (moverIdx % 3)
- LOS, avoidance staggered but movement always runs
- Creates micro-frame-rate wobble (frame 1: 50 movers, frame 2: 17 movers)
- Can disable for deterministic testing (`useStaggeredUpdates = false`)
- **Hidden Consequence**: Movers react to each other with 1-2 frame delay

**Randomized Repath Cooldowns** (Anti-Thundering-Herd)
- Prevents all movers repathing simultaneously (performance spike)
- Randomizes by ±TICK_RATE frames when path fails
- **Hidden Consequence**: Non-deterministic behavior even with fixed seed

**Spatial Grid Neighbor Limits**:
- Max 48 neighbors scanned, max 10 repulsion sources applied
- Guards against O(n²) neighbor checks
- **Hidden Consequence**: Dense mover crowds have weaker avoidance (some neighbors ignored)

---

### 11. SAVE/LOAD EDGE CASES (Dragons Here)

**Dual Migration Requirement** (Easy to Forget)
- Both `saveload.c` AND `inspect.c` need parallel updates
- Forgetting inspect.c causes silent corruption on old save loads
- **Missing Feature**: Automated migration test suite (load all old save versions)

**Legacy Struct Hardcoding**:
- Old versions use hardcoded counts: `V32_ITEM_TYPE_COUNT = 22`
- Adding items requires new version constant + full migration path
- **Missing Feature**: Schema version compatibility matrix (what changed in each version)

**Incomplete Migration = Silent Data Loss**:
- Missing second output storage check = items vanish forever
- No validation that migration preserved all data
- **Missing Feature**: Post-migration validation (checksum, entity counts match)

---

## TOP 10 MOST SURPRISING NICHE BLIND SPOTS

1. **Movers silently disappear when trapped** - No UI feedback, just console log
2. **Stone floors accumulate 50% less dirt** - Hidden material property advantage
3. **Ramps can be placed backwards** - Pathfinder finds them but movement fails
4. **Multi-output recipes can lose items silently** - Second output storage check can fail
5. **Water displacement when building** - Can accidentally flood adjacent areas
6. **Knot detection uses quadratic avoidance** - Creates invisible "patience zones"
7. **HPA* silently falls back to A*** - Performance trap with no warning
8. **Staggered updates cause frame-to-frame wobble** - Makes performance profiling misleading
9. **Sapling regrowth is complete but disabled** - Full forest regeneration exists but OFF
10. **Frozen water is just a flag** - Ice doesn't become walkable terrain

---

## NEW TODO IDEAS FROM NICHE ANALYSIS

### Edge Case Fixes (Prevent Weird Bugs):
26. **Ramp placement validation** - Error when placing ramp into wall/wrong direction
27. **Mover rescue system** - Auto-teleport stuck movers instead of deactivating
28. **Multi-output storage guarantee** - Suspend bill if EITHER output can't be stored
29. **Water displacement preview** - Show where water will go before placing walls
30. **Path failure diagnostic** - "Why can't mover reach target?" tooltip

### Micro-Interactions (Add Polish):
31. **Screen shake on tree fall** - Small camera shake + dust particles
32. **Item bounce physics** - Items dropped from height bounce once before settling
33. **Smooth camera follow** - Lerp camera position when following mover (0.3s ease)
34. **Mover speed variance** - Randomize 0.9-1.1x speed per mover for personality
35. **Idle animations** - Movers fidget/look around when waiting

### Hidden Features Exposed (Make Discoverable):
36. **Material properties tooltip** - Show dirt resistance, wear resistance when hovering
37. ~~**Ramp direction arrows**~~ ✅ DONE - Visual arrows on ramps showing up/down direction
38. **Debug mode panel** - Single UI panel to toggle all visualization modes
39. ~~**Sapling regrowth toggle**~~ ✅ DONE - UI option to enable forest regeneration (ui_panels.c line 765)
40. **Workshop collision bounds** - Show invisible blocking area when placing workshop

### Emergent Gameplay (Enable Creativity):
41. **Floodgates** - Toggleable water barriers for dam management
42. **Firebreaks** - Designated cleared areas that stop fire spread
43. **Intentional paths** - Flagstone paths resist wear, guide mover movement
44. **Water wheels** - Generate power from water pressure/flow
45. **Ice harvesting** - Mine frozen water for cold storage material

### Quality of Life Micro-Features:
46. **Shift+click select all of type** - Select all trees in radius for batch operations
47. **Drag-select designations** - Multi-select for batch cancel/modify
48. **Ghost preview** - Show transparent result before confirming construction
49. **Chain placement mode** - Hold shift to auto-continue placing after each building
50. **Auto-rotate ramps** - Ramps automatically face placement direction

---

## Conclusion

The truly niche blind spots reveal a game with **deep systems that lack discoverability and polish**. Many powerful features exist but are hidden (sapling regrowth, console system, profiler). Many edge cases create frustrating surprises (trapped movers disappearing, ramps placed backwards, silent item loss). And many micro-interactions are missing the "juice" that would make the game feel responsive and alive (screen shake, particles, smooth camera, physics feedback).

The **highest-impact niche improvements** would be:
1. **Expose hidden features** (make sapling regrowth, material properties, debug tools discoverable)
2. **Fix silent failures** (mover rescue, path diagnostics, multi-output guarantees)
3. **Add sensory feedback** (screen shake, particles, smooth camera, sound)
4. **Enable emergent gameplay** (floodgates, firebreaks, water wheels unlock creative solutions)
5. **Smooth rough edges** (ramp validation, water displacement preview, batch operations)

These aren't grand new systems - they're the **connective tissue** that transforms a collection of features into a cohesive, polished experience.
