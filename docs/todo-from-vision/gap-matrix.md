# Vision Doc → Codebase Gap Matrix

This matrix maps each `docs/vision/**` file to current implementation in `src/**` and calls out gaps.

| Vision doc | Current status in `src/` | Gaps / notes |
|---|---|---|
| `docs/vision/vision.md` | Directional only | No direct implementation tasks (theme/feel) |
| `docs/vision/logistics-influences.md` | Haulers, stockpiles, workshops, crafting, blueprints exist | Missing workshop status UI, stockpile meters, job lines, warning icons, conveyors/elevators, automation UX |
| `docs/vision/needs-vs-jobs.md` | Job system exists | Needs system not implemented (no hunger/sleep/mood) |
| `docs/vision/new-jobs-ideas.md` | Mining, building, channeling, ramps, floor removal, chop/sapling jobs exist | Broad labor list and proficiencies not implemented |
| `docs/vision/stone-aged-dwarfjobs.md` | No labor/skill system | Job categories and skill progression not implemented |
| `docs/vision/terrain-and-tiles/tiles.md` | Core tiles exist (air, wall, dirt, ladders, ramps, trees) | Many tile variants and overlays not yet represented visually |
| `docs/vision/terrain-and-tiles/soils.md` | No soil matrix | Soil richness/wetness and farming soils not implemented |
| `docs/vision/terrain-and-tiles/special-tiles.md` | Sources/drains/heat/cold/fire flags exist; materials tracked | Material-aware rendering and source/drain tile visuals missing |
| `docs/vision/entropy-and-simulation/entropy.md` | Ground wear, grass, water, fire, temp, trees exist | Cleaning/weeding/repair/refuel loops missing; seasons/weather not implemented |
| `docs/vision/entropy-and-simulation/entropy_systems.md` | Water/fire/smoke/steam/temperature implemented | Light, mud, gas, mold, vegetation systems not implemented |
| `docs/vision/entropy-and-simulation/steam-system.md` | Steam implemented | Pressure and machine use not implemented |
| `docs/vision/entropy-and-simulation/water-system-differences-with-df.md` | Some doc drift | Temperature/steam are now present in code |
| `docs/vision/farming/*` | No farming system | Farming plots, crop growth, tending, yields not implemented |
| `docs/vision/infinite-world-exploration.md` | No streaming world | Overmap/delta/streaming not implemented |
