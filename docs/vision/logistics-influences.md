# Logistics System Influences

Research on Factorio, belt games (Shapez.io, Mindustry, Satisfactory), Free Enterprise, and SimTower as influences for the navkit job/logistics system.

---

## 1. Belts vs. Haulers: The Core Choice

| Aspect | Belts | Haulers |
|--------|-------|---------|
| Throughput | Fixed, predictable | Variable, depends on count |
| Setup cost | Infrastructure once | Ongoing labor cost |
| Flexibility | Rigid paths | Dynamic routing |
| Visual clarity | Very high (see the flow) | Medium (need to track agents) |
| Bottleneck diagnosis | Obvious (items pile up) | Harder to see |
| Player agency | Design puzzle | Management puzzle |

**Recommendation:** Haulers are the right foundation for a colony sim. The "factory feel" comes from production chains, not necessarily belts. Consider **conveyor segments as a later upgrade** for high-throughput fixed routes.

**Factorio's wisdom:** "Bots for short-medium range and trains for long range is the ultimate logistics strategy."

For our game:
- **Haulers**: Flexible, general purpose, early game
- **Belts/Conveyors**: High-throughput fixed routes, mid game
- **Elevators/Vertical transport**: Bottleneck management, late game

---

## 2. Visual Diagnostics (From Shapez.io)

> "If shapes are packed tightly together and moving slower than full speed, you've got a problem downstream. If shapes are moving at full speed but there is empty space, you've got a problem upstream."

Applied to haulers:
- **Full hands, standing still** = downstream problem (stockpile full, path blocked)
- **Empty hands, standing still** = upstream problem (no items to haul, no jobs)

---

## 3. The Minimal Production Chain

The smallest satisfying production chain:

```
Raw Resource --> Processing --> Product --> Use/Export
     |              |             |            |
   (Mine)      (Workshop)    (Stockpile)   (Construction/Sale)
```

**Concrete example:**
```
Stone Wall (minable)
    |
    v
Stone Chunks (item, spawns on ground)
    |
    v [Hauler carries to Stonecutter Workshop]
    v
Stone Blocks (crafted item)
    |
    v [Hauler carries to Stockpile OR Construction Site]
    v
Wall Construction (uses stone blocks)
```

---

## 4. Workshop Status Indicators (From Factorio Bottleneck Mod)

```c
typedef enum {
    WORKSHOP_WORKING,       // Green - actively crafting
    WORKSHOP_OUTPUT_FULL,   // Yellow - needs hauler to take output
    WORKSHOP_INPUT_EMPTY,   // Red - needs hauler to bring input
    WORKSHOP_NO_WORKER,     // Gray - no crafter assigned
} WorkshopVisualState;
```

**Diagnostics to track:**
```c
typedef struct {
    float inputStarvationTime;   // How long input has been empty
    float outputBlockedTime;     // How long output has been full
    float lastWorkTime;          // When work last happened
} WorkshopDiagnostics;
```

---

## 5. SimTower's Vertical Insights

### Key Patterns

**1. Sky Lobbies / Transfer Floors**
- Standard elevators cover only 30 floors
- Express elevators stop only every 15th floor
- Passengers transfer at lobby floors

For our game: certain floors become "logistics hubs" where items transfer between vertical transport systems.

**2. Stress as a Metric**
Movers could track "frustration" based on:
- Time waiting for elevators
- Failed pathfinding attempts
- Carrying items long distances

**3. Floor Zoning by Function**
```
Floors 0-5:    Production (workshops, heavy industry)
Floors 6-10:   Storage (main stockpiles)
Floors 11-15:  Living (colonist quarters)
Floors 16-20:  Services (medical, dining)
```

Creates predictable vertical traffic patterns.

**4. Express vs. Local Elevators**
```c
typedef enum {
    ELEVATOR_LOCAL,     // Stops at every floor in range
    ELEVATOR_EXPRESS,   // Only stops at designated floors (lobbies)
    ELEVATOR_FREIGHT,   // Prioritizes items over movers
} ElevatorMode;
```

---

## 6. Automation Progression

| Stage | Description | In Our Game |
|-------|-------------|-------------|
| Manual | Player directly controls every action | Click to designate mining, manually place stockpiles |
| Semi-Auto | Agents perform tasks, player designs system | Haulers auto-assign, player designs layout |
| Automated | Systems self-optimize | Work orders ("maintain 50 blocks"), auto-rehaul |
| Meta-Auto | Build systems that build systems | Blueprints, copy-paste workshop+stockpile combos |

**Key insight from Factorio:**
> "If something is feeling too tedious or too repetitive, you can almost certainly automate that thing away."

Introduce automation when manual becomes tedious, not before. Automation should feel like a reward.

---

## 7. Common Pitfalls to Avoid

### Job Thrashing
Movers constantly switching jobs because conditions change mid-task.

**Solution:** Job commitment - once a mover starts, they complete unless target becomes invalid.

### Starvation Cascades
One slow workshop backs up, starves another, cascade failure.

**Solution:**
- Buffer stockpiles between production stages
- Work orders with minimum thresholds ("only craft if input > 5")
- Visual warnings when buffers are low

### The Hauler Death Spiral
Too few haulers → items pile up → production slows → colony fails.

**Solution:**
- Show hauler status ("4/10 idle, 6/10 hauling")
- Warning when ground items exceed threshold

### Router Heresy (From Mindustry)
> "Placing two routers side by side is an infamously inefficient setup that grants you an achievement titled 'Heresy'."

Avoid designs where items can flow backwards or oscillate. Stockpile priorities should form a DAG (directed acyclic graph), not cycles.

---

## 8. Visual Feedback Patterns

### Stockpile Fill Meters
```
[||||||||  ]  80% full (green)
[||||||||||]  100% full (yellow)
[||||||||||+] Overfull (red, items on ground nearby)
```

### Job Lines (Debug Toggle)
Thin lines from mover to target:
- Line to target item (pickup phase)
- Line to target stockpile (delivery phase)

### Warning Icons
Float above problem areas:
- Stockpile full with items nearby
- Workshop starved > N seconds
- Mover stuck/blocked

### Follow Mode
Click mover to follow them with camera. Great for debugging and satisfaction.

---

## 9. Vertical Slice Options

### Option A: "Factory Feel" (Factorio-inspired)
```
1. Designate mining on stone wall
2. Mover works, wall becomes floor, stone chunk spawns
3. Mover delivers chunk to Stonecutter Workshop
4. Workshop crafts: 2 chunks -> 1 block
5. Mover delivers block to stockpile
```

**Satisfying because:** Visible world change, item transformation, continuous flow.

**Estimated:** ~700 lines (mining ~300, workshop ~400)

### Option B: "Tower Management Feel" (SimTower-inspired)
```
1. Build multiple floors connected by ladder
2. Workshop on floor 0, stockpile on floor 2
3. Movers must climb ladder (queue forms)
4. Player adds elevator
5. Throughput increases, player optimizes layout
```

**Satisfying because:** Vertical progression visible, bottleneck obvious, solution tangible.

**Estimated:** ~600 lines (elevator ~500, queue visualization ~100)

---

## 10. Things to Keep in Mind

### Performance at Scale
- **Job assignment:** Already optimized with spatial grids. Consider capping jobs assigned per frame.
- **Pathfinding for validation:** Use connectivity regions (flood-fill IDs) before expensive pathfinding.
- **Workshop ticks:** Only tick active workshops, not all MAX_WORKSHOPS.

### UX Patterns That Work
- **Stockpile-adjacent placement:** Small input/output stockpiles next to workshops
- **Copy settings:** "Make this stockpile have same filters as that one"
- **Bulk designation:** Click-drag to designate mining area
- **Z-level minimap:** Show all floors as small rectangles, click to jump

### Input/Output Stockpile Pattern
```
[Input Stockpile]  ->  [Workshop]  ->  [Output Stockpile]
   (critical priority,    (pulls from     (critical priority,
    only raw stone)        any stone       only blocks)
                          stockpile)
```

Workshop pulls from ANY valid stockpile, but high-priority adjacent stockpiles get filled first by haulers.

---

## Summary: Recommended Build Order

1. **Mining** (~300 lines) - Visible world change, creates items
2. **Simple Workshop** (~400 lines) - Completes production chain
3. **Visual Feedback** (~200 lines) - Status indicators, fill meters
4. **Elevator** (~500 lines) - Vertical optimization puzzle

**Optional later:**
- Conveyor segments for high-throughput fixed routes
- Work orders for automation
- Express/freight elevator variants

---

## References

- Factorio Wiki: Belt Transport System
- Factorio Friday Facts #225: Bots vs Belts
- Shapez.io Strategy Guide
- Satisfactory Conveyor Belts Wiki
- Mindustry Transportation Guide ("Router Heresy")
- SimTower - The Obscuritory
- Factorio Bottleneck Lite Mod
