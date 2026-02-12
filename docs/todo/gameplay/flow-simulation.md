# Flow Simulation Systems: Water Flow Field + Wind Field

Date: 2026-02-07
Scope: Add two vector‑field systems that bias existing simulations without rewriting them.

## Goals
- Provide deterministic **directional river flow** on small maps (e.g., 64×64) while preserving current water behavior.
- Enable **pond circulation** and **vortex** effects via local flow vectors.
- Reuse the same mechanism for **wind‑driven smoke/steam drift** and **fire spread bias**.
- Keep changes incremental: bias the existing neighbor selection instead of replacing the simulation.

## Non‑Goals
- Full fluid dynamics or continuous velocity fields.
- Removing existing source/drain or pressure behavior.
- Heavy performance regressions in water/smoke/steam/fire loops.

---

## System A: Water Flow Field

### Concept
Add a per‑cell vector field that biases horizontal spreading decisions. It does **not** replace water physics; it only nudges which neighbor receives water first.

### Data Shape (lightweight)
- `flowDx[z][y][x]`: int8 in range ‑127..127
- `flowDy[z][y][x]`: int8 in range ‑127..127
- `flowStrength[z][y][x]`: uint8 0..255 (0 = no bias)

Memory cost (256×256×16): ~3 MB for 3 bytes/cell. For 64×64, trivial.

### Where it plugs in
**Current behavior** (water):
- `WaterTrySpread()` shuffles the 4 neighbors randomly (Fisher‑Yates) to avoid directional bias.

**New behavior** (bias only):
- Keep the randomized list, but **sort by dot‑product** with the flow vector:
  - `score = dot(flowDir, dirToNeighbor)`
  - Higher score = earlier in order
- If `flowStrength == 0`, keep existing randomness.

This preserves the “don’t skip neighbors” rule that broke the previous presence‑grid experiment.

### Expected behavior
- Rivers flow downstream consistently even on flat terrain.
- Ponds can have gentle circulation (slow swirl).
- Vortices become possible by stamping a rotational flow field.

### Example: river generation (small map)
1. Choose upstream and downstream edges.
2. Generate a centerline (straight + noise or midpoint displacement).
3. Carve a channel in terrain along that path.
4. Stamp flow vectors along the path pointing downstream.
5. Place **water sources** near the upstream edge and **drains** at the downstream edge (existing API: `SetWaterSource`, `SetWaterDrain`).

This yields a clear directional river with minimal new systems.

### Example: vortex
For a center `(cx, cy)`:
- Tangential vector: `t = normalize(-dy, dx)`
- Radial vector: `r = normalize(dx, dy)`
- `flow = normalize(t * swirlStrength + r * inwardStrength)`
- `inwardStrength < 0` pulls inward (whirlpool), `> 0` pushes outward.

---

## System B: Wind Field

### Concept
Use a similar vector field to bias **airborne** simulations. This is not a full atmosphere model — it’s a deterministic “push” that can vary by region.

### Where it plugs in
- **Smoke** (`smoke.c`): bias `SmokeTrySpread()` neighbor order using wind direction.
- **Steam** (`steam.c`): same as smoke (horizontal drift + rising).
- **Fire** (`fire.c`): bias `FireTrySpread()` by increasing spread chance **downwind** and decreasing **upwind**.

### Why a separate field
- Water flow direction is often **terrain‑dependent** (riverbeds).
- Wind is **global or regional**, changes over time, and should be adjustable without touching terrain.

### Data Shape options
1. **Reuse the same vector grid** (rename to `flowField` and give each system its own strength scalar).
2. **Separate wind field** (clearer semantics, slightly more memory).

Recommended: separate **WindField** to avoid confusing water flow with air movement.

---

## Interop with Existing Systems

### Water system (current code)
- `UpdateWater()` iterates all cells, calls `WaterTrySpread()` to equalize.
- We only change **neighbor ordering**, not the level transfer logic.

### Smoke/Steam systems
- Both use randomized neighbor order; can be biased the same way as water.
- Wind does **not** need to override rising behavior — just bias lateral movement.

### Fire system
- `FireTrySpread()` already computes spread chance; wind can add a multiplier:
  - `spreadPercent += windBonus` for downwind
  - `spreadPercent -= windPenalty` for upwind

### Terrain generator
- Rivers already exist in `terrain.c` (hills water). Add a flow‑field stamp there.
- For small 64×64 maps, create a **river‑preset generator** that stamps:
  - channel, flow vectors, sources, drains

### Save/Load
- If fields are **procedural only**, rebuild them at load time (no save change).
- If players can **edit/paint** flow or wind, then:
  - add flow grids to save format
  - update `saveload.c` and `inspect.c` in parallel

---

## API Sketch

```c
// flow_field.h
void InitFlowField(void);
void ClearFlowField(void);
void SetFlowVector(int x, int y, int z, int8_t dx, int8_t dy, uint8_t strength);
void GetFlowVector(int x, int y, int z, int8_t* dx, int8_t* dy, uint8_t* strength);

// wind_field.h
void InitWindField(void);
void ClearWindField(void);
void SetWindVector(int x, int y, int z, int8_t dx, int8_t dy, uint8_t strength);
void GetWindVector(int x, int y, int z, int8_t* dx, int8_t* dy, uint8_t* strength);
```

Helper for neighbor ordering:
```c
void OrderDirsByVector(int* order, const int* dx, const int* dy, int8_t vx, int8_t vy, uint8_t strength);
```

---

## Implementation Plan (Incremental)

### Phase 0: Water flow bias (minimal)
- Add `flowField` grid + accessors.
- In `WaterTrySpread()`, bias neighbor order by flow vector.
- Add a simple river generator for 64×64 map or hook into existing river pass.

### Phase 1: Wind bias for smoke/steam
- Add wind field grid.
- Bias `SmokeTrySpread()` and `SteamTrySpread()` neighbor order.

### Phase 2: Wind bias for fire
- Add wind‑based spread modifiers in `FireTrySpread()`.

### Phase 3: Authoring / tooling
- Debug overlay for arrows (flow and wind).
- Optional sandbox tools to paint flow/wind.

---

## Validation
- River test: place sources upstream and drains downstream, check persistent flow direction.
- Pond vortex test: stamp circular field; verify visible swirl (smoke/foam optional).
- Smoke drift test: place smoke and confirm consistent lateral movement with wind.
- Fire spread test: verify higher spread probability downwind.

---

## Open Questions
- Do we want a **single shared vector field** with per‑system strength, or separate water + wind fields?
- Should flow/wind be saved, or rebuilt procedurally on load?
- How much bias is “enough” before it breaks the natural feel of the existing simulations?
