# Reverse Time / Rewind Feature Research

> **See also**: [Infinite World Exploration](../vision/infinite-world-exploration.md) - the delta persistence infrastructure planned for chunk streaming would make rewind snapshots dramatically lighter (estimated 0.5-2 MB per tick vs 150 MB for full snapshots).

## Executive Summary

Implementing a reverse time/rewind feature for NavKit is **architecturally challenging but feasible**. The codebase has several characteristics that make this difficult:

1. **Non-deterministic simulation**: Random number generation is used throughout physics systems (water, fire, smoke, steam)
2. **Multiple mutable state sources**: ~37K lines with state spread across grids, arrays, and complex entity systems
3. **Tightly integrated systems**: Simulation systems depend on each other (fire→temp→water freezing, water→steam)
4. **No command pattern**: Input actions directly modify world state; no action log exists
5. **Pathfinding cache state**: Entrances, abstract graphs, and chunk dirty flags all cache dynamic information

**Estimated scope**: 900-1100 new lines of code + targeted refactoring across 6-8 system files.

---

## Current State Management

### Global State Sources

**Game State Header** (`game_state.h`):
- Display toggles: 15+ boolean flags (showMovers, showItems, etc.) - non-critical for rewind
- UI state: zoom, offset, currentViewZ, followMoverIdx - non-critical
- Settings: pathAlgorithm, currentTool, currentTerrain - non-critical

**Main.c Global Definitions** (133 globals):
- Critical state: `paused`, `gameSpeed`
- Agent/Mover counts: Settings, not simulation state

### Per-Tick Mutable State

From `main.c` TickWithDt():
```c
UpdateWater()        // waterGrid[z][y][x]
UpdateFire()         // fireGrid[z][y][x], fuel consumption
UpdateSmoke()        // smokeGrid[z][y][x]
UpdateSteam()        // steamGrid[z][y][x]
UpdateTemperature()  // temperatureGrid[z][y][x]
UpdateWaterFreezing()
ItemsTick(dt)        // Item positions, reserved status
DesignationsTick(dt) // Unreachable cooldowns
AssignJobs()         // Job assignments, idle list
JobsTick()           // Job progress, mover task state
TreesTick(dt)        // growthTimer[], targetHeight[]
UpdateMovers()       // Mover positions, paths, avoidance state
currentTick++        // Global tick counter
```

---

## Simulation Systems Analysis

### Determinism Summary

| System | Deterministic | Random Calls | Reversibility |
|--------|---------------|--------------|---------------|
| Temperature | Yes | None | Easy |
| Trees | Yes | Position-based hash | Easy |
| Grid/Cells | Yes | None | Easy |
| Designations | Yes | None | Easy |
| Jobs | Yes | None | Medium |
| Items | Yes | None | Medium |
| Stockpiles | Yes | None | Medium |
| Workshops | Yes | None | Medium |
| Movers | Partial | Optional cooldowns | Hard |
| Steam | No | Neighbor shuffle, condensation | Hard |
| Smoke | No | Neighbor shuffle, escape chance | Hard |
| Water | No | Neighbor shuffle | Hard |
| Fire | No | Spread direction, ignition chance | Hard |

### Non-Deterministic Systems Detail

**Water System** (`simulation/water.c`):
- Uses `rand()` for direction randomization (line 319): `int j = rand() % (i + 1);`
- Each UpdateWater() call produces different flow patterns due to randomized neighbor order

**Fire System** (`simulation/fire.c`):
- Uses `rand()` for spread direction (line 272)
- Uses `rand()` for spread chance (line 303): `if ((rand() % 100) < spreadPercent)`
- Fuel consumption is probabilistic (line 395)

**Smoke System** (`simulation/smoke.c`):
- Uses `rand()` for neighbor order (line 236)
- Uses `rand()` for escape chance (line 433): `if ((rand() % 3) == 0)`

**Steam System** (`simulation/steam.c`):
- Uses `rand()` for neighbor order (line 234)
- Uses `rand()` for condensation chance (line 294)

**Ground Wear** (`simulation/groundwear.c`):
- Uses `rand()` for sapling regrowth (line 195)

---

## Complete Mutable State Inventory

### Grids (all saved in saveload.c)

| Grid | Size per cell | Purpose |
|------|---------------|---------|
| `grid[z][y][x]` | 1 byte | CellType (walls, air, ladders) |
| `cellFlags[z][y][x]` | 1 byte | Burned, wetness, surface, floor, workshop block |
| `wallMaterial[z][y][x]` | 1 byte | Material type for walls |
| `floorMaterial[z][y][x]` | 1 byte | Material type for floors |
| `wallNatural[z][y][x]` | 1 byte | Natural vs constructed |
| `floorNatural[z][y][x]` | 1 byte | Natural vs constructed |
| `wallFinish[z][y][x]` | 1 byte | Finish type |
| `floorFinish[z][y][x]` | 1 byte | Finish type |
| `waterGrid[z][y][x]` | 2 bytes | WaterCell |
| `fireGrid[z][y][x]` | 2 bytes | FireCell |
| `smokeGrid[z][y][x]` | 1 byte | SmokeCell |
| `steamGrid[z][y][x]` | 1 byte | SteamCell |
| `temperatureGrid[z][y][x]` | 4 bytes | TempCell |
| `designations[z][y][x]` | 14 bytes | Designation struct |
| `wearGrid[z][y][x]` | 4 bytes | Wear amount |
| `growthTimer[z][y][x]` | 4 bytes | Tree growth |
| `targetHeight[z][y][x]` | 4 bytes | Tree target height |

**Total per cell**: ~35 bytes
**For 512x512x16 grid**: ~150MB

### Entity Arrays (all saved)

| Array | Size | Purpose |
|-------|------|---------|
| `items[MAX_ITEMS]` | ~800KB | 25000 items × 32 bytes |
| `jobs[MAX_JOBS]` | ~600KB | 10000 jobs × 60 bytes |
| `movers[MAX_MOVERS]` | ~1MB | 10000 movers × 104 bytes |
| `stockpiles[MAX_STOCKPILES]` | ~77KB | 64 stockpiles |
| `blueprints[MAX_BLUEPRINTS]` | ~50KB | 1000 blueprints |
| `workshops[MAX_WORKSHOPS]` | ~51KB | 256 workshops |

**Total entities**: ~2.5MB

### Simulation Accumulators (all saved)

```c
fireSpreadAccum, fireFuelAccum
waterEvapAccum
smokeRiseAccum, smokeDissipationAccum
steamRiseAccum
heatTransferAccum, tempDecayAccum
wearRecoveryAccum
```

### Transient Caches (NOT saved, can be rebuilt)

- `moverGrid` - Spatial grid for mover queries (~400KB)
- `itemGrid` - Spatial grid for item queries (~100KB)
- `chunkDirty[]` - Pathfinding chunk invalidation flags
- `AStarNode nodeData[]` - Search scratch space
- `abstractNodes[]` - HPA* search scratch

**Total Rewindable State**: ~153MB per snapshot

---

## Memory Requirements

| Approach | Memory for 60s history | Pros | Cons |
|----------|------------------------|------|------|
| Full snapshot every tick | 9 GB | Simple, exact | Too much memory |
| Snapshot every 10 ticks + replay | 900 MB | Reasonable memory | Replay latency |
| Delta compression | ~90 MB | Excellent memory | Complex change tracking |
| Snapshot every 600 ticks (10s) | 90 MB | Very low memory | Must replay up to 10s |

---

## Architectural Approaches

### Option A: Snapshot Approach (Simplest)

Save complete game state at intervals, restore on rewind.

```c
typedef struct {
    unsigned long tickNumber;
    CellType* gridSnapshot;
    WaterCell* waterSnapshot;
    FireCell* fireSnapshot;
    // ... all other grids and entity arrays
    double gameTime;
    float fireSpreadAccum, fireFuelAccum, waterEvapAccum;
    // ... all accumulators
} GameSnapshot;

#define MAX_SNAPSHOTS 60
GameSnapshot snapshots[MAX_SNAPSHOTS];
```

**Pros**: Trivial to implement, works with non-deterministic systems, exact restoration
**Cons**: High memory cost, limited rewind distance

### Option B: Delta Compression (Medium Complexity)

Store only cells that changed each tick, plus full snapshots at intervals.

**Pros**: Much smaller memory footprint, can store longer history
**Cons**: Complex change tracking, slower to restore

### Option C: Deterministic Replay (Maximum Complexity)

Make simulation deterministic by capturing RNG seed per frame, replay from snapshots.

```c
typedef struct {
    uint64_t state;
} DeterministicRNG;

// Replace all rand() calls
uint32_t DeterministicRandom() {
    rng.state = rng.state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(rng.state >> 33);
}
```

**Pros**: Can rewind to any tick, no additional memory overhead
**Cons**: Requires replaying simulation, must refactor ~30 rand() calls

### Recommended: Hybrid Approach

1. Save full snapshot every 600 ticks (~10 seconds)
2. Use replay for intermediate ticks
3. Store RNG seed per snapshot for deterministic replay
4. Memory: ~90MB for 60-second history

---

## Implementation Roadmap

### Phase 1: Infrastructure (200-300 LOC)

**New files**:
- `core/rewind.h` - API definitions
- `core/rewind.c` - Snapshot management

**Core functions**:
```c
void SaveSnapshot(unsigned long tick);
bool RestoreSnapshot(unsigned long targetTick);
void RewindOneTick(void);
void InvalidateTransientCaches(void);
```

### Phase 2: Snapshot System (400-500 LOC)

- Circular buffer of snapshots
- Per-tick or per-interval saving
- Memory pool for snapshot data

### Phase 3: Deterministic RNG (50-100 LOC)

Replace `rand()` calls in:
- `simulation/water.c` (1 call)
- `simulation/fire.c` (3 calls)
- `simulation/smoke.c` (2 calls)
- `simulation/steam.c` (2 calls)
- `simulation/groundwear.c` (1 call)
- `entities/mover.c` (optional, 1 call)

### Phase 4: Cache Invalidation (70-100 LOC)

On restore:
```c
void RestoreGameState(GameSnapshot* snap) {
    // ... restore all cell/entity state ...

    // Invalidate pathfinding caches
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < chunksY; y++)
            for (int x = 0; x < chunksX; x++)
                chunkDirty[z][y][x] = true;

    // Rebuild spatial grids
    BuildMoverSpatialGrid();
    BuildItemSpatialGrid();

    // Mark movers for repath
    for (int i = 0; i < moverCount; i++)
        if (movers[i].active)
            movers[i].needsRepath = true;
}
```

### Phase 5: UI & Controls (200-300 LOC)

- Key binding (e.g., `Z` for rewind)
- Timeline visualization
- History length indicator

---

## Estimated Effort

| Component | New/Changed Lines | Difficulty |
|-----------|-------------------|------------|
| `core/rewind.c/h` | ~500-700 new | Medium |
| Deterministic RNG | ~50 changes across 6 files | Easy |
| `main.c` integration | ~50 | Easy |
| Cache invalidation | ~70 across mover/items/pathfinding | Easy |
| UI (timeline bar, controls) | ~200 | Easy |
| **Total** | **~900-1100 lines** | **Medium** |

---

## Potential Pitfalls

### 1. Non-Deterministic Physics
**Problem**: Rewind then play forward produces different results.
**Solution**: Accept it for MVP, or implement deterministic RNG.

### 2. Spatial Grid Cache Invalidation
**Problem**: Mover/Item spatial grids cache neighbor relationships.
**Solution**: Rebuild grids on restore (~10-20ms).

### 3. Job System References
**Problem**: Jobs hold references to items, movers, stockpiles.
**Solution**: Restore entire job array atomically; references are implicit.

### 4. Path Cache Staleness
**Problem**: Movers have cached paths that become invalid.
**Solution**: Set `needsRepath = true` for all movers on restore.

### 5. Accumulators
**Problem**: Simulation accumulators are in separate files.
**Solution**: Already handled - saveload.c has getter/setter functions for all accumulators.

---

## Conclusion

**Implementing reverse time/rewind is feasible without a major rewrite.**

### Key Advantages
- Save system already captures all mutable state
- Transient caches can be safely rebuilt
- Most game systems are deterministic

### Key Challenges
- Fire, water, smoke, steam use randomness (affects replay fidelity)
- Memory cost for full snapshots is high (~150MB each)
- Trade-off between history length and memory/performance

### Recommended MVP
1. Snapshot-based rewind with full state copying
2. Circular buffer of 6 snapshots (one per 10 seconds = 60s history)
3. Accept non-deterministic replay for fire/water/smoke
4. ~900MB memory cost, ~900-1100 lines of new code

---

## Synergy with Infinite World / Delta Persistence

The [Infinite World Exploration](../vision/infinite-world-exploration.md) research proposes delta-based chunk storage for streaming. This same infrastructure would transform the rewind feature:

### Shared Infrastructure

| Component | Infinite World Use | Rewind Use |
|-----------|-------------------|------------|
| Delta format | Diff chunk vs procedural baseline | Diff tick N vs tick N-1 |
| Change tracking | Know which cells modified | Know which cells changed per tick |
| Sparse designations | Reduce 64MB → ~1KB | Smaller snapshots |
| Deterministic RNG | Regenerate unmodified chunks | Replay forward from snapshot |

### Impact on Rewind Memory

With delta tracking in place:

| What Changed | Typical Size |
|--------------|--------------|
| Water cells moved | 50-200 cells × 3 bytes = ~600 bytes |
| Fire spread | 10-50 cells × 3 bytes = ~150 bytes |
| Mover positions | 100 movers × 12 bytes = ~1.2 KB |
| Job state changes | 20 jobs × 8 bytes = ~160 bytes |
| **Total per tick** | **~2-5 KB** |

At 60 ticks/second:
- 1 second = 120-300 KB
- 60 seconds = **7-18 MB** (vs 9 GB for full snapshots)

This makes multi-minute rewind practical without massive memory.

### Implementation Order

**If planning both features:**
1. Implement delta tracking infrastructure first (for infinite world)
2. Rewind becomes almost trivial - just store N ticks of deltas
3. Reverse = apply deltas in reverse order

**If only doing rewind:**
1. Simpler to do interval snapshots + replay
2. But building delta tracking is still valuable prep work
