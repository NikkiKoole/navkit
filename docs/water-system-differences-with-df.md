# Water System: Dwarf Fortress Comparison

This document outlines the differences between our simplified water system and Dwarf Fortress's full fluid simulation.

## Features We Implement

| Feature | Our Implementation |
|---------|-------------------|
| Water levels | 1-7 scale (same as DF) |
| Gravity/falling | Water drops to cells below |
| Horizontal spreading | Equalizes with orthogonal neighbors |
| Pressure | BFS through full cells, water rises to sourceZ - 1 |
| Sources/drains | User-placed infinite water/removal |
| Evaporation | 1/100 chance for level-1 water |
| Movement penalty | Speed multiplier based on depth |
| Stability optimization | Skip processing settled water |

## Features We Skip

### 1. Temperature System
- **DF**: Every tile and material has a temperature. Water freezes to ice below 10000°U, ice melts above that, water boils to steam at high temperatures.
- **Ours**: No temperature. Water is always liquid. Evaporation is random chance, not heat-based.

### 2. Material Interactions
- **DF**: Water + magma creates obsidian (water on top) or steam (magma on top). Different liquids interact chemically.
- **Ours**: Water exists in isolation. No magma, no reactions.

### 3. Diagonal Flow
- **DF**: Water can flow diagonally in certain conditions.
- **Ours**: Strictly orthogonal movement (N/S/E/W) for both spreading and pressure propagation.

### 4. Mud and Contamination
- **DF**: Water flowing over soil creates mud. Water can carry contaminants (blood, vomit, syndromes). Muddy tiles allow underground farming.
- **Ours**: Water is pure. No terrain modification, no contamination tracking.

### 5. Creature Interactions
- **DF**: Creatures can drown (lungs fill over time), swim (skill-based), get pushed by water currents, and some species breathe underwater.
- **Ours**: Only movement speed penalty. No drowning, swimming, or current forces.

### 6. Aquifers
- **DF**: Underground geological layers that continuously seep water into adjacent tiles. Major obstacle in fortress design.
- **Ours**: No natural water generation. Only manual user-placed sources.

### 7. Pressure Complexity
- **DF**: Pressure can stack through pump systems. Complex calculations for pressure equalization across large connected bodies.
- **Ours**: Simplified pressure with hard limits:
  - 64-cell BFS search limit
  - Water rises to sourceZ - 1 only
  - Single pressure source tracked per cell

### 8. Flow Volume and Multi-Tick Settling
- **DF**: Large water movements calculate over multiple ticks. Flow rate depends on pressure differential.
- **Ours**: Aggressive single-tick processing:
  - 4096 cell update cap per tick
  - Transfer amounts are fixed (1 unit for spreading, half-difference for equalization)
  - Randomized spread direction prevents oscillation

### 9. Cave-ins and Structural Damage
- **DF**: Water pressure can cause cave-ins. Flooding can destroy buildings.
- **Ours**: Water has no structural effects.

### 10. Save/Load Complexity
- **DF**: Must serialize temperature, contamination, pressure state for every water tile.
- **Ours**: Only need level, source/drain flags, and pressure info (2 bytes per cell).

## Design Rationale

Our system prioritizes:
1. **Performance**: Stability tracking and update caps keep CPU usage bounded
2. **Predictability**: Simple rules make behavior easy to understand
3. **Core feel**: Gravity + spreading + pressure captures the essence of DF water

The skipped features (temperature, contamination, drowning) could be added incrementally if needed.
