# Entropy Systems Brainstorm

Ideas for cellular automata and simulation systems that create emergent behavior.

## Current Systems

| System | Grid? | Description |
|--------|-------|-------------|
| Water | Yes (3D) | DF-style 1-7 levels, pressure, sources/drains, evaporation |
| Fire | Yes (3D) | Fuel-based spread, extinguished by water, generates smoke |
| Smoke | Yes (3D) | Rises, spreads, dissipates, fills down when trapped |

## Planned

### Temperature (grid)
Foundation for heat-based interactions:
- Per-cell temperature value that diffuses to neighbors
- Fire raises temperature, water absorbs/lowers it
- Ambient temperature pulls toward baseline
- Enables: water↔steam, ice, hypothermia, heat damage

### Steam (grid)
Water in gaseous state:
- Generated when water + high temperature
- Rises like smoke
- Condenses back to water when cooled (hits ceiling, distance from heat)
- Requires temperature system first

## Ideas

### Natural

| System | Grid? | Notes |
|--------|-------|-------|
| Ice | No (cell type) | Water at low temp freezes, blocks flow, melts when heated |
| Mud | No (cell flag) | Water + dirt = mud, slows movers, dries over time based on temp |
| Vegetation | No (cell type + flag?) | Grass spreads slowly, dies from fire/traffic, regrows. Maybe growth value per cell |
| Erosion | No (event) | Water flow wears down terrain over time |

### Atmospheric

| System | Grid? | Notes |
|--------|-------|-------|
| Wind | Maybe (directional field) | Pushes smoke/steam/fire spread direction |
| Humidity | Maybe (grid) | Affects evaporation, fire spread, condensation. Could diffuse |
| Rain | No (spawner) | Spawns water from above, driven by humidity |

### Decay & Life

| System | Grid? | Notes |
|--------|-------|-------|
| Rot/decay | No (item timer) | Dead things decompose over time, become fertilizer |
| Fertilizer | No (cell flag) | Affects vegetation growth rate |
| Food→shit→fertilizer | No (item states) | Mover digestion cycle, items transform |
| Mold/fungus | No (cell type or sparse list) | Spreads in dark, damp areas |

### Light & Energy

| System | Grid? | Notes |
|--------|-------|-------|
| Light/shadow | Yes (grid) or raycast | Every cell has light level, or recalculate on demand |
| Electricity | Sparse/graph | Conducts through water/metal, not air |
| Air pressure | Maybe (grid) | Sealed rooms, explosive decompression |

### Dangerous

| System | Grid? | Notes |
|--------|-------|-------|
| Gas/poison | Yes (grid) | Like smoke but heavier (sinks), damages movers |
| Lava | Yes (grid) | Like water but hot, ignites things, cools to rock |
| Acid | Yes (grid) | Like water but dissolves walls/items over time |

## Recommended Build Order

1. **Temperature** — Foundation for heat-based systems. No dependencies. Enables steam, ice, mud drying, etc.
2. **Steam** — Requires temperature. Completes water cycle (water↔steam).
3. **Light/shadow** — Independent system. High gameplay value. Enables mold/fungus.
4. **Mud** — Easy win. Cell flag + drying based on temperature. Slows movers.
5. **Gas/poison** — Reuse smoke patterns but inverted (sinks). Danger element.
6. **Mold/fungus** — Requires light + dampness. Fun emergent spread behavior.
7. **Vegetation** — Benefits from fertilizer, temperature, light. More complex lifecycle.

## Design Principles

### When to use a 3D grid
- Value varies continuously per cell
- Needs to flow/diffuse to neighbors
- Examples: water, smoke, temperature, light

### When to use cell flags/types
- Binary or enum states (wet/dry, fertile/barren)
- Doesn't flow, just transforms in place
- Examples: mud, ice, burned, fertilized

### When to use item/entity state
- Property of a thing, not a location
- Timer-based decay
- Examples: rot, food→shit cycle, rust on items

### When to use sparse lists
- Rare occurrences, don't need per-cell storage
- Examples: mold colonies, electricity sources
