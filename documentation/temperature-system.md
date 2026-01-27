# Temperature System

A per-cell temperature grid that other systems (fire, water, weather) can read from and write to.

## Overview

Temperature is a foundational system - it doesn't simulate itself much, but provides data for other systems to react to.

## Temperature Scale

TBD - options:
- **Simple:** 0-255 uint8 (0=frozen, 128=ambient, 255=burning)
- **Realistic:** Actual celsius/kelvin values (more complex, maybe overkill)

## Data Structure

```c
// temperature.h
uint8_t temperatureGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Or if we need more data per cell:
typedef struct {
    uint8_t current;     // current temperature
    uint8_t ambient;     // what it decays toward (based on biome/depth?)
} TempCell;
```

## Temperature Sources

Things that affect temperature:

| Source | Effect |
|--------|--------|
| Fire | Increases temp in cell and neighbors |
| Lava | High constant temperature |
| Water | Cools nearby cells slightly |
| Ice | Keeps cells frozen |
| Depth (z-level) | Underground = cooler? Or warmer? (design choice) |
| Sun/weather | Surface cells affected by time of day / season (future) |

## Temperature Decay

Each tick, temperature moves toward ambient:
- Hot cells cool down
- Cold cells warm up
- Rate is tweakable

## System Interactions

### Fire + Temperature
- Fire increases cell temperature
- Higher temperature = easier to ignite (lower fuel threshold?)
- Fire in hot areas spreads faster
- Fire in cold/wet areas spreads slower

### Water + Temperature
- High temp (near fire) evaporates water faster
- Low temp freezes water (ice = solid, blocks flow)
- Frozen water could thaw when temperature rises

### Terrain + Temperature
- Hot terrain is more flammable
- Frozen terrain could have different movement cost
- Visual changes (frost overlay, heat shimmer)

## Potential Features (Future)

- **Heat transfer:** Temperature spreads to neighbors (conduction)
- **Insulation:** Some materials (stone) transfer heat slower than others (metal)
- **Seasons:** Ambient temperature changes over time
- **Creature effects:** Movers generate body heat, can freeze/overheat

## Open Questions

1. Should temperature spread horizontally, or only be set by sources?
2. How fast should decay be? Instant? Gradual?
3. Do we need separate "heat" and "cold" values, or is one temperature enough?
4. Should frozen water be a water state or a separate cell type?

## Files (Planned)

- `pathing/temperature.h` - Data structure and API
- `pathing/temperature.c` - Update logic, decay, queries

## Implementation Priority

This system is **lower priority** than fire/smoke. Fire can work without temperature initially (just use fixed spread rates). Temperature adds nuance and cross-system interactions later.
