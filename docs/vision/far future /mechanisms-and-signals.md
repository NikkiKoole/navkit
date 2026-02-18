# Mechanisms & Signals — The Road to Logic

Companion doc to `gates-and-processors.md`. That doc dreams big (processors, punch cards, 13 eras). This doc answers: **what do we actually build first, and how does each layer work under the hood?**

---

## The Three Layers

The vision doc's automation dream breaks down into three distinct implementation layers. Each is useful on its own. Each uses a completely different execution model.

```
Layer 3: PROCESSOR          (far future)
         Emulator with opcodes, registers, program counter
         Runs N instructions per game tick
         "READ thermometer → CMP → JIF → WRITE valve"

Layer 2: LOGIC GATES         (future)
         Signal propagation grid, like fire spread but for on/off
         Each tick: gates read inputs, compute, set outputs
         AND, OR, NOT as placeable blocks

Layer 1: MECHANISMS           (next)
         Event-driven links. Source toggles → targets react.
         No tick cost, no propagation, no program.
         Levers, pressure plates, doors, floodgates.
```

These are **separate systems**, not one system at different speeds. You don't need an emulator to open a door. You don't need signal propagation to pull a lever.

---

## Layer 1: Mechanisms (Build This First)

### What It Is

The Dwarf Fortress "Manual Era." A lever connects to a door. Pull the lever, door opens. That's it.

No signals, no wires, no logic. Just a links table:

```
links[] = {
    { source: lever_at(5,3,1), targets: [door_at(10,3,1), floodgate_at(10,4,1)] },
    { source: plate_at(8,3,1), targets: [door_at(12,3,1)] },
}
```

When a source changes state, iterate its targets and flip them. This is a function call, not a simulation.

### The Components

**Door** (new cell type: CELL_DOOR)
- Two states: open (walkable, transparent) and closed (blocks movement, blocks fluids)
- Movers can operate doors manually (walk up, toggle, walk through)
- Can also be toggled by linked mechanisms
- Interacts with existing sims:
  - Closed door blocks water spread (water.c checks cell walkability already)
  - Closed door blocks fire/smoke spread
  - Closed door provides temperature insulation
  - Open door lets everything through
- Pathfinding: movers treat closed doors as passable-with-cost (they know they can open it)

**Lever** (new furniture type: FURNITURE_LEVER)
- Two states: on / off (stays in position until pulled again)
- A mover must walk to it and pull it (job: walk → operate 1s)
- Visual: distinct sprite for on/off state
- Placed via construction recipe (sticks + cordage? stone + sticks?)

**Pressure Plate** (cell overlay or cell flag)
- Two states: depressed / released
- Triggers based on **weight on the cell** (not just mover presence)
- Weight sources: movers standing on it, items placed on it, water level
- Configurable threshold: light (mover footstep), medium (heavy item), heavy (multiple items or loaded container)
- Auto-resets when weight drops below threshold
- Could also trigger from water level (water >= 4 on the cell)
- Visual: subtle floor sprite change (depressed vs raised)

### Placing Items on Pressure Plates (The Stockpile Trick)

How does a player get a heavy rock onto a pressure plate? **They don't need a new job type.**

A pressure plate is just a floor cell with special properties. The player can place a **1x1 stockpile** on top of the plate and filter it to accept rocks/blocks. The existing haul system does the rest:

1. Player builds pressure plate at (5, 3, 1)
2. Player places 1x1 stockpile on same cell, filters to ITEM_ROCK only
3. Existing WorkGiver_Haul finds a rock on the ground
4. Existing JOBTYPE_HAUL picks it up, delivers it to the stockpile slot
5. Rock sits on the cell → plate checks item weight → threshold met → triggers

**No new haul code.** The plate doesn't care how the item got there. It just checks weight on the cell each time something changes (item placed, item removed, mover arrives, mover leaves).

This also means:
- Player can **remove** the weight by un-stockpiling or clearing the stockpile → plate releases → linked doors close
- Heavier items (ITEM_BLOCKS, full ITEM_CHEST) trigger higher thresholds
- Stacked items add weight (stackCount matters)
- A loaded container (basket full of rocks) weighs more than an empty one (contentCount contributes)

**Precedent in other games:**
- **Dwarf Fortress**: Pressure plates trigger on weight thresholds. Minecarts loaded with stone can hold plates down. Water weight also counts.
- **Minecraft**: Wooden plates trigger on ANY entity (including dropped items). Stone plates trigger only on mobs. Weighted plates (gold/iron) output signal proportional to entity count.
- **Zelda/puzzle games**: Push a heavy block onto a switch to hold it down. Classic.

**Why this works well here:**
- Reuses the existing haul + stockpile system entirely
- Makes heavy items (rocks, blocks, filled containers) strategically valuable beyond building
- Creates puzzle gameplay: "I need something heavy enough to hold this plate"
- The weight system naturally scales with item stacking and containers (both already implemented)

**Floodgate** (variant of door, or separate cell type)
- Like a door but specifically for fluid control
- Closed: blocks water. Open: lets water through.
- Movers can always walk through (it's a grate, not a wall)
- Key difference from door: floodgate blocks water but not movers; door blocks both

### The Links System

Modeled after the existing workshop-stockpile linking:

```c
typedef struct {
    int sourceX, sourceY, sourceZ;    // lever or pressure plate position
    MechanismType sourceType;          // MECH_LEVER, MECH_PRESSURE_PLATE
    int targetCount;
    struct {
        int x, y, z;
        MechanismType targetType;      // MECH_DOOR, MECH_FLOODGATE
    } targets[MAX_MECHANISM_LINKS];    // 4-8 targets per source
} MechanismLink;
```

**Link UI**: Same pattern as workshop linking.
- Select lever → enter "link mode" → click doors/floodgates to link → confirm
- Visual: draw lines from source to targets while linking (like stockpile link display)

**Execution**: Zero cost per tick. Only runs when a source changes state:

```c
void OnMechanismToggled(int sourceX, int sourceY, int sourceZ) {
    MechanismLink* link = FindLink(sourceX, sourceY, sourceZ);
    if (!link) return;
    for (int i = 0; i < link->targetCount; i++) {
        ToggleMechanism(link->targets[i].x, ...);
    }
}
```

Called from:
- `JobDriver_PullLever()` when mover finishes pulling
- `UpdatePressurePlates()` when mover enters/exits plate cell

### What Players Can Do With Just This

**Shelter** (door alone):
- Build walls + door. Close door. Rain stays out, heat stays in. Survival value.

**Manual defense** (lever + door):
- Lever in your base linked to a door in a hallway. Pull lever, seal the corridor.

**Water control** (lever + floodgate):
- Dam a river with walls + floodgate. Lever controls irrigation to your crops.
- Fill a reservoir, link floodgate to lever, release water on demand.

**Auto-door** (pressure plate + door):
- Plate outside door, linked to door. Step on plate → door opens. Walk away → closes.
- Simple but satisfying. "I built an automatic door!"

**Flood trap** (pressure plate + floodgate):
- Plate in a hallway, linked to floodgate holding back a water reservoir.
- Enemy steps on plate → water floods hallway.
- This is DF's signature move and it works with only Layer 1.

**Weighted switch** (stockpile + pressure plate):
- Place plate, place 1x1 stockpile on it filtered to rocks. Haul system delivers rock automatically.
- Rock holds plate down permanently → linked door stays open (or closed).
- Remove the stockpile (or clear it) → rock gets hauled away → plate releases → door toggles back.
- Puzzle element: "I need to hold 3 plates down to keep the floodgates open for irrigation. That's 3 rocks I can't use for building."

**Temperature management** (door as insulator):
- Open doors during hot days to vent heat, close at night to retain warmth.
- Floodgate releases cold water through a channel under the floor to cool a room.

### Implementation Cost

- 1 new cell type (CELL_DOOR), maybe CELL_FLOODGATE or just a flag variant
- 1 new furniture type (FURNITURE_LEVER)
- 1 cell flag or overlay for pressure plates
- 1 new data structure: MechanismLink array (like lightSources[], saved/loaded)
- 2 new job types (JOBTYPE_OPERATE_DOOR, JOBTYPE_PULL_LEVER)
- Door checks in water/fire/smoke spread functions (if cell is closed door → block)
- Pathfinding: doors as passable-with-extra-cost
- Link UI (reuse workshop linking pattern)
- Save version bump

---

## Layer 2: Logic Gates (Future)

### What Changes

Layer 1 links are instant and unconditional: lever flips → door flips. Always.

Layer 2 adds **conditional logic**: door flips only if lever A **AND** lever B are both on.

### The Signal Grid

A new grid (like waterGrid, fireGrid):

```c
// One bit per cell is enough for basic signals
uint8_t signalGrid[MAX_DEPTH][MAX_HEIGHT][MAX_WIDTH];
// 0 = off, 1 = on (could later use 0-15 for signal strength)
```

Each tick, the signal grid updates. But unlike fire/water (which spread organically), signals propagate along **placed wire** cells:

**Wire** (new cell type or overlay: CELL_WIRE or a cell flag)
- Carries signal from one cell to adjacent wire cells
- 1-tick propagation delay per cell (gives the system timing)
- Visual: wire on floor, glows when signal is on

**NOT gate** (placeable block)
- Input side, output side
- If input is on → output is off. If input is off → output is on.
- This single gate, combined with wires, enables all other logic (NAND-complete)

**AND gate** (placeable block)
- Two input sides, one output side
- Output is on only if both inputs are on

**OR gate** (placeable block)
- Two input sides, one output side
- Output is on if either input is on

### How It Connects to Layer 1

- Levers and pressure plates **emit signal** to adjacent wire cells
- Doors and floodgates **read signal** from adjacent wire cells
- So Layer 1 mechanisms become Layer 2 signal sources/sinks
- The "link" system from Layer 1 still works for direct connections (no wire needed)
- Wire-based connections are the new option for when you need logic in between

### What Players Can Do With This

- **Airlock**: door A opens only when door B is closed (NOT gate between them)
- **Safety interlock**: forge only fires when BOTH the fuel lever AND the ventilation lever are on (AND gate)
- **Multi-entrance alarm**: any of 3 pressure plates triggers the alert bell (OR gate with 3 inputs)
- **Toggle flip-flop**: press plate once → on, press again → off (two NOT gates in a loop with careful wiring)
- **Timer**: long wire loop creates delay. Signal takes N ticks to travel N cells of wire.
- **DF-style fluid computing**: pressure plates that detect water level + floodgates + wire logic = water-powered calculator

### Implementation Model

Unlike Layer 1 (event-driven), Layer 2 runs per-tick like other sims:

```c
void UpdateSignals(float dt) {
    // Only update if any signals are active (like dirtActiveCells pattern)
    if (signalActiveCells == 0) return;

    // Propagate: for each wire cell, check neighbors
    // Gates: for each gate, read input wires, compute, write output wire
    // Sources: levers/plates write their state to adjacent wire
    // Sinks: doors/floodgates read adjacent wire state
}
```

Follows the existing sim_manager pattern (active cell counter, skip when idle).

### Implementation Cost (on top of Layer 1)

- Signal grid (uint8_t 3D, same as snowGrid)
- Wire cell type or flag
- 3 gate types (furniture or cell type)
- Per-tick update function (pattern: fire/water spread)
- Wire rendering (floor overlay, like floor dirt)
- Gate rendering (furniture sprites with state indication)

---

## Layer 3: Processor (Far Future)

### When Do You Need This?

When players want to do things that **conditional gates can't express well**:
- "Turn on the pump ONLY IF the tank is below 20% AND it's not raining AND the steam pressure is above 50"
- "Cycle through 4 different outputs in sequence, 10 seconds each"
- "Count how many movers have passed through this door today"

Gates can technically do all of this, but it requires huge Rube Goldberg circuits. A processor compresses that into a small block with a program.

### How It Works (The Emulator)

See `gates-and-processors.md` for the full vision. Summary:

```c
typedef struct {
    int registers[8];       // R0-R7, temporary storage
    int pc;                 // program counter
    Instruction program[MAX_INSTRUCTIONS];  // the "punch cards"
    float inputPorts[8];    // connected sensors read into these
    float outputPorts[8];   // connected actuators read from these
    int clockSpeed;         // instructions per game tick (era-dependent)
} VirtualProcessor;
```

6 core opcodes: READ, WRITE, SET, ADD/SUB, CMP, JMP/JIF.

Runs N instructions per game tick (clockSpeed depends on tech era).

### How It Connects to Layers 1 & 2

- **Input ports** read from: pressure plates, temperature grid, water level, stockpile item counts, weather state, signal wire state
- **Output ports** write to: doors, floodgates, signal wires, workshop bill enable/disable
- The processor is just another signal source/sink, but programmable

### UI Options (No Text Coding)

The vision doc explores several. Most fitting for this game:

1. **Punch cards / instruction slots**: Physical cards crafted at a workshop, inserted into slots. Each card = one opcode. Drag to reorder. Thematic, resource-cost to "program."

2. **Gambit rows**: Condition → Target → Action spreadsheet. Simpler, more accessible. Good for a first pass.

3. **Visual wiring**: Breadboard UI with logic tiles. Most powerful, most complex to build.

Recommendation: start with gambit rows (simplest UI), optionally add punch cards for flavor.

### Implementation Cost (on top of Layers 1 & 2)

- VirtualProcessor struct + execution loop
- Sensor bridge (read game state into input ports)
- Actuator bridge (write output ports to game objects)
- UI for programming (the big effort — card slots or gambit rows or visual board)
- Save/load for programs
- Per-tick cost: small per processor, but need to cap instruction count for performance

---

## Build Order

```
NOW (fits current "survive one day" focus):
├── Doors                    ← needed for shelter anyway
├── Floodgates               ← water control, free fun with existing water sim
├── Levers + pressure plates ← minimal mechanism components
└── Links table              ← connect them, reuse workshop-link UI pattern

LATER (when colony has multiple movers, more infrastructure):
├── Signal wire              ← cell overlay, 1-tick propagation
├── NOT gate                 ← the universal gate, enables everything
├── AND / OR gates           ← convenience, not strictly needed with NOT
└── Signal-aware doors       ← doors read adjacent wire, no link needed

FAR FUTURE (when players want compact automation):
├── Sensor blocks            ← read temperature, water level, item counts
├── Processor block          ← small emulator, 6 opcodes
├── Programming UI           ← gambit rows or punch cards
└── Era-based constraints    ← clock speed, instruction limit, fuel cost
```

Each step is independently useful. You never build something that only pays off two layers later.

---

## Comparison: Vision Doc vs This Plan

| Vision Doc Says | This Plan Says |
|:---|:---|
| 13 eras of automation | Start with 1 era (wood/stone). Mechanisms are era-agnostic. |
| Trained crows as CPUs | Fun flavor, but not a system. Save for cosmetic theming. |
| Punch card crafting | Layer 3 concern. Don't design the UI until Layers 1-2 prove the concept. |
| Sensors reading world state | Layer 3. But note: the world state is already there (temp, water, weather). |
| "Processor without programming" | Gambit rows (condition/action lists) are the right answer. Defer this. |
| Physical gates shrinking over eras | Layer 2 gates can just be 1x1 blocks. Era-sizing is cosmetic, not mechanical. |
| Signal medium (steam, wire, etc.) | Just use a signal grid. The visual theme can change per era without changing the system. |

The vision doc is a great north star. This plan is the road that gets there.

---

## Key Design Principle

**Mechanisms are not a simulation.** They're an **event system**.

Water simulates — it flows, finds levels, pressurizes. Fire simulates — it spreads, consumes fuel, interacts with wind.

Mechanisms don't simulate. They react. Lever pulled → look up targets → toggle them. Done. No per-tick cost, no propagation, no emergent behavior from the mechanism itself.

The emergent behavior comes from **combining mechanisms with simulations**: a floodgate releasing water IS emergent, but the emergence is in the water sim, not the mechanism. The mechanism just opened the gate.

This keeps mechanisms cheap, predictable, and debuggable — exactly what players need when they're building contraptions. The "unpredictable fun" comes from the physics, not the wiring.

Layer 2 (signal wire + gates) adds a small simulation layer, but it's still boolean and deterministic — no randomness, no decay, no fluid dynamics. Just on/off propagating through wire at 1 cell per tick.

Layer 3 (processor) adds sequencing and math, but it's still deterministic — same inputs always produce same outputs.

The chaos and emergence stay in the physical sims (water, fire, temperature). The logic layers are the player's tool for **taming** that chaos.
