# Steam System

Steam is a phase of water that carries thermal energy. Unlike smoke, steam conserves mass and energy - water becomes steam, steam becomes water again.

## Core Concept

Steam enables:
- **Cooking**: Steam rooms for food processing
- **Heating**: Pipe steam to heat distant rooms
- **Machines**: Pressure-driven mechanisms (future)

## Test Scenario

```
z=3:  [steam cools -> water droplets fall]
z=2:  [steam rises and spreads]
z=1:  [closed room filled with water, boils from heat below]
z=0:  [closed room with fire/heat sources at 200C]
```

The water at z=1 heats up from the fire below. When it reaches boiling point (100C), it converts to steam. Steam rises to z=2 and z=3, spreading and cooling. When steam cools below condensation threshold, it turns back into water droplets that fall.

## Data Structure

```c
typedef struct {
    uint8_t level : 3;        // 0-7 steam density
    uint8_t pressure : 3;     // 0-7 pressure level  
    uint8_t stable : 1;       // Optimization flag
    uint8_t reserved : 1;     // Future use
} SteamCell;
```

Note: Steam temperature uses the existing temperature grid rather than storing its own temperature.

## Key Differences from Smoke

| Aspect | Smoke | Steam |
|--------|-------|-------|
| Source | Combustion byproduct | Water phase change |
| Mass | Dissipates (disappears) | Conserved (becomes water) |
| Energy | None | Carries latent heat |
| Pressure | Visual only | Can do work |
| Condensation | No | Yes, becomes water |

## Mechanics

### Generation
- Water cell at >= 100C starts converting to steam
- Water level decreases, steam level increases above
- Rate depends on temperature (hotter = faster boiling)

### Rising
- Steam rises like smoke (bottom-to-top processing)
- Faster when hot, slower when cooling
- Spreads horizontally when blocked above

### Pressure (Phase 2)
- Builds in enclosed spaces
- Seeks escape routes (gaps, vents)
- Higher pressure = faster flow through openings

### Cooling
- Steam temperature decays toward ambient (uses temperature grid)
- Cooling rate affected by surroundings

### Condensation
- Steam below ~100C turns back to water
- Water appears as droplets that fall
- Releases heat when condensing (warms surroundings)

## Implementation Phases

### Phase 1: Basic Steam
- [ ] SteamCell structure and grid
- [ ] Steam generation from boiling water
- [ ] Steam rising (reuse smoke pattern)
- [ ] Steam condensation back to water
- [ ] Basic rendering

### Phase 2: Pressure
- [ ] Pressure tracking in enclosed spaces
- [ ] Pressure-driven flow
- [ ] Vents and valves

### Phase 3: Machines
- [ ] Steam consumers (turbines, pistons)
- [ ] Pipe system for steam transport
