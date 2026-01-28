# Time System

A unified time architecture that separates engine ticks from game time, enabling game speed control, human-readable configuration, and day/night cycles.

## The Problem

Currently, all simulation rates are expressed in ticks or opaque numbers:

```c
#define TICK_RATE 60
int fireSpreadChance = 10;      // 1 in 10 chance per tick... so ~6 times/sec?
int smokeDissipationRate = 10;  // Every 10 ticks... so ~6 times/sec?
int heatTransferSpeed = 50;     // What does 50 even mean?
#define MOVER_SPEED 200.0f      // 200 pixels/sec... how many tiles is that?
```

Issues:
- **Opaque values**: You need mental math to understand behavior
- **No game speed**: Simulation is locked to real-time
- **Tick-coupled**: Changing tick rate breaks all assumptions
- **No world clock**: No foundation for day/night cycles

## Three-Layer Architecture

| Layer | Unit | Purpose | Rate |
|-------|------|---------|------|
| **Engine tick** | tick | Deterministic physics, fixed timestep | 60 ticks/real-second (fixed) |
| **Game time** | game-seconds | Simulation intervals, speeds | Scaled by `gameSpeed` |
| **World clock** | hours (0-24) | Day/night, time of day | Based on `dayLength` |

### Layer 1: Engine Tick (unchanged)

The 60Hz fixed timestep remains for deterministic behavior:

```c
#define TICK_RATE 60
#define TICK_DT (1.0f / TICK_RATE)  // 0.01666... seconds

// Main loop - unchanged
if (accumulator >= TICK_DT) {
    Tick();
    accumulator -= TICK_DT;
}
```

This guarantees:
- Frame-rate independent simulation
- Deterministic/reproducible behavior
- Stable physics at any render FPS

### Layer 2: Game Time (new)

Game time accumulates each tick, scaled by game speed:

```c
float gameSpeed = 1.0f;     // 1.0 = real-time, 10.0 = 10x speed, 0.1 = slow-mo
float gameTime = 0.0f;      // Total elapsed game-seconds
float gameDeltaTime;        // Game-seconds elapsed this tick

void Tick(void) {
    gameDeltaTime = TICK_DT * gameSpeed;
    gameTime += gameDeltaTime;
    // ...
}
```

| gameSpeed | 60 ticks = | Feel |
|-----------|------------|------|
| 0.1 | 0.1 game-seconds | Slow motion |
| 1.0 | 1.0 game-seconds | Real-time |
| 10.0 | 10.0 game-seconds | Fast-forward |
| 60.0 | 60.0 game-seconds | 1 game-minute per real-second |

### Layer 3: World Clock (new)

Built on game time, tracks time of day:

```c
float timeOfDay = 6.0f;     // 0.0-24.0 hours (start at 6am)
int dayNumber = 1;          // Current day
float dayLength = 1200.0f;  // Game-seconds per full day (20 game-minutes)

void Tick(void) {
    // ... game time update ...
    
    timeOfDay += (gameDeltaTime / dayLength) * 24.0f;
    if (timeOfDay >= 24.0f) {
        timeOfDay -= 24.0f;
        dayNumber++;
    }
}
```

`dayLength` controls how long a day feels, independent of game speed:
- `dayLength = 1200` → 20 game-minutes per day
- `dayLength = 300` → 5 game-minutes per day (fast days for testing)
- `dayLength = 3600` → 1 game-hour per day (slow, realistic)

## Human-Readable Configuration

All simulation parameters expressed in **tiles** and **game-seconds**.

Note: The "New" values below are **design targets to be tuned**, not exact mathematical conversions of current behavior. The goal is human-readable values that produce good gameplay.

### Movement

| Current | New | Meaning |
|---------|-----|---------|
| `MOVER_SPEED = 200.0f` | `moverSpeed = 6.0f` | 6 tiles per game-second |

```c
// Old: pixels per real-second, coupled to tick rate
#define MOVER_SPEED 200.0f
float movement = MOVER_SPEED * TICK_DT;

// New: tiles per game-second, human-readable
float moverSpeed = 6.0f;  // "A mover walks 6 tiles per second"
float movement = moverSpeed * CELL_SIZE * gameDeltaTime;
```

### Fire

| Current | New | Meaning |
|---------|-----|---------|
| `fireSpreadChance = 10` | `fireSpreadInterval = 2.0f` | Fire spreads every 2 game-seconds |
| `fireFuelConsumption = 5` | `fireFuelInterval = 0.5f` | Consumes fuel every 0.5 game-seconds |

### Smoke

| Current | New | Meaning |
|---------|-----|---------|
| `smokeRiseChance = 2` | `smokeRiseInterval = 0.1f` | Smoke rises every 0.1 game-seconds |
| `smokeDissipationRate = 10` | `smokeDissipationTime = 5.0f` | Smoke dissipates over 5 game-seconds |

### Steam

| Current | New | Meaning |
|---------|-----|---------|
| `steamRiseChance = 1` (every tick) | `steamRiseInterval = 0.017f` | Steam rises every tick (~60/sec) |

Note: Current steam rises every single tick. With the new system you'd decide: should steam rise 60 times per second, or is once per second enough? The interval lets you tune this explicitly.

### Temperature

| Current | New | Meaning |
|---------|-----|---------|
| `heatTransferSpeed = 50` | `heatTransferInterval = 60.0f` | Heat conducts once per game-minute |
| `tempDecayRate = 10` | `tempDecayInterval = 30.0f` | Temp decays toward ambient every 30 game-seconds |

### Ground Wear

| Current | New | Meaning |
|---------|-----|---------|
| `wearDecayInterval = 1` | `wearRecoveryTime = 300.0f` | Grass recovers over 5 game-minutes |

### Water

| Current | New | Meaning |
|---------|-----|---------|
| `waterEvapChance = 100` | `waterEvapInterval = 10.0f` | Shallow water evaporates every 10 game-seconds |

## Implementation: Interval-Based Systems

Each system tracks accumulated time and fires when interval is reached:

```c
// Example: Fire spread
float fireSpreadInterval = 2.0f;  // Spread every 2 game-seconds
static float fireSpreadAccum = 0.0f;

void UpdateFire(void) {
    fireSpreadAccum += gameDeltaTime;
    
    if (fireSpreadAccum >= fireSpreadInterval) {
        fireSpreadAccum -= fireSpreadInterval;
        // Do fire spread logic
        for (each fire cell) {
            TrySpreadToNeighbors(cell);
        }
    }
}
```

For probabilistic systems, convert to expected interval:

```c
// Old: 1 in 10 chance per tick = expected every 10 ticks = every 0.167 seconds
// New: explicit interval
float fireSpreadInterval = 2.0f;  // Deterministic: every 2 game-seconds
```

## Day/Night Cycle Integration

Systems can respond to time of day:

```c
// Ambient temperature varies with time of day
int GetAmbientTemperature(int z) {
    int baseTemp = ambientSurfaceTemp - (z * ambientDepthDecay);
    
    // Cooler at night (midnight = 0/24, noon = 12)
    float nightFactor = cosf((timeOfDay / 24.0f) * 2.0f * PI);  // -1 to 1
    int tempVariation = (int)(nightFactor * 10.0f);  // +/- 10 degrees
    
    return clamp(baseTemp + tempVariation, TEMP_MIN, TEMP_MAX);
}

// Lighting
float GetDaylightLevel(void) {
    // 6am-6pm = day, 6pm-6am = night
    if (timeOfDay >= 6.0f && timeOfDay < 18.0f) {
        // Day: ramp up in morning, down in evening
        float dayProgress = (timeOfDay - 6.0f) / 12.0f;  // 0-1
        return sinf(dayProgress * PI);  // 0 -> 1 -> 0
    } else {
        return 0.1f;  // Night: dim ambient
    }
}
```

## UI Exposure

New UI panel for time controls:

```c
void DrawTimePanel(int x, int* y) {
    if (SectionHeader(x, y, "Time", &sectionTime)) {
        // Display
        Label(x, *y, TextFormat("Day %d, %02d:%02d", dayNumber, 
              (int)timeOfDay, (int)((timeOfDay - (int)timeOfDay) * 60)));
        *y += 20;
        
        // Controls
        DraggableFloat(x, y, "Game Speed", &gameSpeed, 0.1f, 0.0f, 100.0f);
        DraggableFloat(x, y, "Day Length (game-sec)", &dayLength, 10.0f, 60.0f, 7200.0f);
        
        // Presets
        if (Button(x, *y, "Pause")) gameSpeed = 0.0f;
        if (Button(x + 60, *y, "1x")) gameSpeed = 1.0f;
        if (Button(x + 100, *y, "10x")) gameSpeed = 10.0f;
        if (Button(x + 150, *y, "60x")) gameSpeed = 60.0f;
        *y += 25;
    }
}
```

## Testing

Tests should use game-time, not ticks:

```c
// Old: opaque tick counts
RunTempTicks(1000);  // How long is this?

// New: human-readable game-time
RunGameSeconds(60.0f);  // Run 1 game-minute of simulation

void RunGameSeconds(float seconds) {
    float targetTime = gameTime + seconds;
    while (gameTime < targetTime) {
        Tick();
    }
}
```

## Migration Path

1. **Add time layer** (non-breaking)
   - Add `gameSpeed`, `gameTime`, `gameDeltaTime`, `timeOfDay`, `dayNumber`, `dayLength`
   - Initialize `gameSpeed = 1.0f` so behavior unchanged

2. **Add interval variables** (parallel to old)
   - Add new `*Interval` variables alongside old tick-based ones
   - Systems can read either during transition

3. **Convert systems one by one**
   - Update each system to use game-time intervals
   - Update corresponding tests
   - Remove old tick-based variables

4. **Update tests**
   - Replace `RunTicks(n)` with `RunGameSeconds(t)`
   - Express expectations in game-time

5. **Add UI controls**
   - Time panel with speed controls
   - Day/night display

## Large Time Scales

Some systems need very long intervals:

| System | Time Scale | Game-Seconds |
|--------|------------|--------------|
| Fire spread | seconds | 2.0 |
| Smoke dissipation | seconds | 5.0 |
| Heat transfer | minutes | 60.0 |
| Ground wear recovery | minutes | 300.0 |
| Grass regrowth | days | 36000.0 (10 days if dayLength=3600) |
| Tree growth | weeks | 252000.0 (70 days if dayLength=3600) |
| Seasons | months | ~1,080,000 (1 year if dayLength=3600) |

Note: These assume `dayLength = 3600` (1 game-hour = 1 day). Adjust accordingly.

### Float Precision

`float` has ~7 significant digits. At `gameTime = 10,000,000` game-seconds (2777 days if dayLength=3600, or 116 real-days), adding small deltas like 0.016 loses precision.

**Solutions:**
1. **Use `double` for gameTime** — 15 significant digits, good for years
2. **Reset gameTime periodically** — systems only care about intervals, not absolute time
3. **Track days separately** — `int dayNumber` + `float timeInDay` avoids huge floats

Recommended: Use `double gameTime` for safety, or split into `dayNumber` + `secondsToday`.

### Fast-Forward Testing

To test 10 days of grass regrowth quickly:

```c
// Option 1: High game speed (still runs all ticks)
// At gameSpeed=600, each tick adds 600/60=10 game-seconds
// So 60 ticks = 600 game-seconds = 10 game-minutes
gameSpeed = 600.0f;
RunGameSeconds(36000.0f);  // 10 days (36000 sec) in 3600 ticks (~60 real-seconds)

// Option 2: Skip ticks entirely for pure time-based systems
void FastForwardGameTime(float seconds) {
    gameTime += seconds;
    // Systems with accumulators will catch up next tick
}
```

**Caveat with Option 1:** At extreme speeds, interval-based systems fire many times per tick:

```c
void UpdateFire(void) {
    fireSpreadAccum += gameDeltaTime;  // Could be 600 game-seconds!
    
    // Need to loop, not just fire once
    while (fireSpreadAccum >= fireSpreadInterval) {
        fireSpreadAccum -= fireSpreadInterval;
        DoFireSpread();
    }
}
```

**Performance cap:** Limit iterations per tick to prevent runaway:

```c
int iterations = 0;
while (fireSpreadAccum >= fireSpreadInterval && iterations < MAX_ITERATIONS_PER_TICK) {
    fireSpreadAccum -= fireSpreadInterval;
    DoFireSpread();
    iterations++;
}
// If still behind, clamp accumulator (accept time loss at extreme speeds)
if (fireSpreadAccum > fireSpreadInterval * 2) {
    fireSpreadAccum = fireSpreadInterval;
}
```

### Specification Tests

These tests read like plain English specs. Each verifies that a human-readable variable does exactly what it says.

```c
describe(spec_mover_speed) {
    // moverSpeed = 2.0f means "a mover walks 2 tiles per second"
    
    it("mover at speed 2 should walk 2 tiles in 1 second") {
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 2.0f;  // 2 tiles per game-second
        SetMoverGoal(m, 50, 0, 0);  // Far away goal
        gameSpeed = 1.0f;
        
        RunGameSeconds(1.0f);
        
        float tilesTraveled = m->x / CELL_SIZE;
        expect(tilesTraveled >= 1.8f && tilesTraveled <= 2.2f);
    }
    
    it("mover at speed 2 should walk 20 tiles in 10 seconds") {
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 2.0f;
        SetMoverGoal(m, 50, 0, 0);
        gameSpeed = 1.0f;
        
        RunGameSeconds(10.0f);
        
        float tilesTraveled = m->x / CELL_SIZE;
        expect(tilesTraveled >= 19.0f && tilesTraveled <= 21.0f);
    }
    
    it("mover at speed 5 should walk 5 tiles in 1 second") {
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 5.0f;  // 5 tiles per game-second
        SetMoverGoal(m, 50, 0, 0);
        gameSpeed = 1.0f;
        
        RunGameSeconds(1.0f);
        
        float tilesTraveled = m->x / CELL_SIZE;
        expect(tilesTraveled >= 4.5f && tilesTraveled <= 5.5f);
    }
}

describe(spec_fire_spread) {
    // fireSpreadInterval = 5.0f means "fire spreads to neighbor after 5 seconds"
    
    it("fire should NOT spread before 5 seconds") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        SetCell(6, 5, 0, CELL_GRASS);
        fireSpreadInterval = 5.0f;
        gameSpeed = 1.0f;
        
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        
        RunGameSeconds(4.0f);  // Just before spread interval
        
        expect(GetFireLevel(6, 5, 0) == 0);  // Not spread yet
    }
    
    it("fire should spread to neighbor within 5 seconds") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        SetCell(6, 5, 0, CELL_GRASS);
        fireSpreadInterval = 5.0f;
        gameSpeed = 1.0f;
        
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        
        RunGameSeconds(6.0f);  // Just after spread interval
        
        expect(GetFireLevel(6, 5, 0) > 0);  // Should have spread
    }
}

describe(spec_smoke_dissipation) {
    // smokeDissipationTime = 10.0f means "smoke fully dissipates in 10 seconds"
    
    it("smoke level 7 should be gone after 10 seconds") {
        InitGridFlat(10, 10, 1);
        smokeDissipationTime = 10.0f;
        gameSpeed = 1.0f;
        
        // Create max level smoke
        SetSmoke(5, 5, 0, SMOKE_MAX_LEVEL);  // Level 7
        expect(GetSmokeLevel(5, 5, 0) == 7);
        
        RunGameSeconds(10.0f);
        
        expect(GetSmokeLevel(5, 5, 0) == 0);  // Fully dissipated
    }
    
    it("smoke should be partially dissipated at 5 seconds") {
        InitGridFlat(10, 10, 1);
        smokeDissipationTime = 10.0f;
        gameSpeed = 1.0f;
        
        SetSmoke(5, 5, 0, SMOKE_MAX_LEVEL);
        
        RunGameSeconds(5.0f);  // Half the dissipation time
        
        int level = GetSmokeLevel(5, 5, 0);
        expect(level > 0 && level < SMOKE_MAX_LEVEL);  // Partially gone
    }
}

describe(spec_heat_transfer) {
    // heatTransferInterval = 60.0f means "heat moves to neighbor after 1 minute"
    
    it("heat should NOT transfer before 60 seconds") {
        InitGridFlat(10, 10, 1);
        heatTransferInterval = 60.0f;
        gameSpeed = 1.0f;
        
        SetTemperature(5, 5, 0, 200);  // Hot
        SetTemperature(6, 5, 0, 20);   // Cold
        
        RunGameSeconds(30.0f);  // Half the interval
        
        expect(GetTemperature(6, 5, 0) == 20);  // No transfer yet
    }
    
    it("heat should transfer to neighbor after 60 seconds") {
        InitGridFlat(10, 10, 1);
        heatTransferInterval = 60.0f;
        gameSpeed = 1.0f;
        
        SetTemperature(5, 5, 0, 200);  // Hot
        SetTemperature(6, 5, 0, 20);   // Cold
        
        RunGameSeconds(61.0f);  // Just past the interval
        
        expect(GetTemperature(6, 5, 0) > 20);  // Heat transferred
    }
}

describe(spec_grass_recovery) {
    // grassRecoveryTime = 300.0f means "worn grass recovers in 5 minutes"
    
    it("fully worn grass should recover in 5 minutes") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        grassRecoveryTime = 300.0f;  // 5 minutes
        gameSpeed = 1.0f;
        
        // Fully wear the grass (turns to dirt)
        SetWear(5, 5, wearMax);
        expect(GetCell(5, 5, 0) == CELL_DIRT);
        
        RunGameSeconds(300.0f);  // 5 minutes
        
        expect(GetWear(5, 5) == 0);
        expect(GetCell(5, 5, 0) == CELL_GRASS);  // Recovered
    }
    
    it("grass should be half-recovered at 2.5 minutes") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        grassRecoveryTime = 300.0f;
        gameSpeed = 1.0f;
        
        SetWear(5, 5, wearMax);
        int initialWear = GetWear(5, 5);
        
        RunGameSeconds(150.0f);  // Half of 5 minutes
        
        int currentWear = GetWear(5, 5);
        // Should be roughly half recovered
        expect(currentWear < initialWear);
        expect(currentWear > initialWear / 4);
        expect(currentWear < initialWear * 3 / 4);
    }
}

describe(spec_water_evaporation) {
    // waterEvapInterval = 30.0f means "shallow water evaporates after 30 seconds"
    
    it("shallow water should evaporate after 30 seconds") {
        InitGridFlat(10, 10, 1);
        waterEvapInterval = 30.0f;
        gameSpeed = 1.0f;
        
        // Level 1 water (shallow)
        SetWater(5, 5, 0, 1);
        expect(GetWaterLevel(5, 5, 0) == 1);
        
        RunGameSeconds(31.0f);
        
        expect(GetWaterLevel(5, 5, 0) == 0);  // Evaporated
    }
    
    it("shallow water should NOT evaporate before 30 seconds") {
        InitGridFlat(10, 10, 1);
        waterEvapInterval = 30.0f;
        gameSpeed = 1.0f;
        
        SetWater(5, 5, 0, 1);
        
        RunGameSeconds(20.0f);  // Before interval
        
        expect(GetWaterLevel(5, 5, 0) == 1);  // Still there
    }
}

describe(spec_steam_rise) {
    // steamRiseInterval = 0.5f means "steam rises one tile every 0.5 seconds"
    
    it("steam should rise one tile in 0.5 seconds") {
        InitGridFlat(10, 10, 3);  // 3 z-levels
        steamRiseInterval = 0.5f;
        gameSpeed = 1.0f;
        
        // Steam at ground level
        SetSteam(5, 5, 0, STEAM_MAX_LEVEL);
        expect(GetSteamLevel(5, 5, 0) > 0);
        expect(GetSteamLevel(5, 5, 1) == 0);
        
        RunGameSeconds(0.5f);
        
        // Should have risen to z=1
        expect(GetSteamLevel(5, 5, 1) > 0);
    }
    
    it("steam should rise 4 tiles in 2 seconds") {
        InitGridFlat(10, 10, 6);  // 6 z-levels
        steamRiseInterval = 0.5f;
        gameSpeed = 1.0f;
        
        SetSteam(5, 5, 0, STEAM_MAX_LEVEL);
        
        RunGameSeconds(2.0f);  // 4 rise intervals
        
        // Should have risen to at least z=4
        expect(GetSteamLevel(5, 5, 4) > 0);
    }
}
```

### Same Tests at Different Game Speeds

The whole point of the time system is that these specs hold true regardless of game speed.

```c
describe(spec_at_different_speeds) {
    it("mover walks 2 tiles in 1 game-second at 1x speed") {
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 2.0f;
        SetMoverGoal(m, 50, 0, 0);
        gameSpeed = 1.0f;  // Real-time
        
        RunGameSeconds(1.0f);
        
        float tiles = m->x / CELL_SIZE;
        expect(tiles >= 1.8f && tiles <= 2.2f);
    }
    
    it("mover walks 2 tiles in 1 game-second at 10x speed") {
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 2.0f;
        SetMoverGoal(m, 50, 0, 0);
        gameSpeed = 10.0f;  // 10x fast-forward
        
        RunGameSeconds(1.0f);  // 1 game-second, but only 0.1 real-seconds
        
        float tiles = m->x / CELL_SIZE;
        expect(tiles >= 1.8f && tiles <= 2.2f);  // Same result
    }
    
    it("mover walks 2 tiles in 1 game-second at 100x speed") {
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 2.0f;
        SetMoverGoal(m, 50, 0, 0);
        gameSpeed = 100.0f;  // 100x fast-forward
        
        RunGameSeconds(1.0f);
        
        float tiles = m->x / CELL_SIZE;
        expect(tiles >= 1.8f && tiles <= 2.2f);  // Same result
    }
    
    it("grass recovers in 5 game-minutes at 1x speed") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        grassRecoveryTime = 300.0f;
        gameSpeed = 1.0f;
        
        SetWear(5, 5, wearMax);
        RunGameSeconds(300.0f);  // Takes 5 real-minutes
        
        expect(GetCell(5, 5, 0) == CELL_GRASS);
    }
    
    it("grass recovers in 5 game-minutes at 100x speed") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        grassRecoveryTime = 300.0f;
        gameSpeed = 100.0f;
        
        SetWear(5, 5, wearMax);
        RunGameSeconds(300.0f);  // Takes only 3 real-seconds!
        
        expect(GetCell(5, 5, 0) == CELL_GRASS);  // Same result
    }
    
    it("grass recovers in 5 game-minutes at 1000x speed") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        grassRecoveryTime = 300.0f;
        gameSpeed = 1000.0f;
        
        SetWear(5, 5, wearMax);
        RunGameSeconds(300.0f);  // Takes only 0.3 real-seconds!
        
        expect(GetCell(5, 5, 0) == CELL_GRASS);  // Same result
    }
    
    it("fire spreads at same game-time at 1x and 50x") {
        // At 1x
        InitGridFlat(10, 10, 1);
        FillRect(0, 0, 10, 10, 0, CELL_GRASS);
        fireSpreadInterval = 2.0f;
        gameSpeed = 1.0f;
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        RunGameSeconds(10.0f);
        int spread1x = CountFireCells();
        
        // At 50x
        InitGridFlat(10, 10, 1);
        FillRect(0, 0, 10, 10, 0, CELL_GRASS);
        ClearFire();
        gameSpeed = 50.0f;
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        RunGameSeconds(10.0f);
        int spread50x = CountFireCells();
        
        // Should be similar (exact match hard due to randomness)
        expect(abs(spread1x - spread50x) <= 3);
    }
}
```

### End-to-End Tests

These tests verify the full chain: game speed → game time → intervals → simulation behavior.

```c
describe(time_system_core) {
    it("should accumulate game time based on game speed") {
        gameTime = 0.0f;
        gameSpeed = 1.0f;
        
        // Run 60 ticks = 1 real-second
        for (int i = 0; i < 60; i++) Tick();
        
        // At 1x speed, 60 ticks = 1 game-second
        expect(gameTime >= 0.99f && gameTime <= 1.01f);
    }
    
    it("should scale game time with game speed") {
        gameTime = 0.0f;
        gameSpeed = 10.0f;
        
        // Run 60 ticks = 1 real-second
        for (int i = 0; i < 60; i++) Tick();
        
        // At 10x speed, 60 ticks = 10 game-seconds
        expect(gameTime >= 9.9f && gameTime <= 10.1f);
    }
    
    it("should advance day number when timeOfDay wraps") {
        dayLength = 60.0f;  // 1 minute per day (fast for testing)
        timeOfDay = 23.5f;  // Near end of day
        dayNumber = 1;
        gameSpeed = 1.0f;
        
        // Run 2 seconds of game time (should wrap past midnight)
        RunGameSeconds(2.0f);
        
        expect(dayNumber == 2);
        expect(timeOfDay >= 0.0f && timeOfDay < 1.0f);
    }
    
    it("should pause simulation when gameSpeed is 0") {
        gameTime = 100.0f;
        gameSpeed = 0.0f;
        
        for (int i = 0; i < 1000; i++) Tick();
        
        expect(gameTime == 100.0f);  // No change
    }
}

describe(time_system_mover_movement) {
    it("mover should travel expected distance in game time") {
        // Setup: mover at origin, goal far away
        InitGridFlat(100, 100, 1);
        Mover* m = CreateMover(0, 0, 0);
        m->speed = 2.0f;  // 2 tiles per game-second
        SetMoverGoal(m, 50, 0, 0);
        gameSpeed = 1.0f;
        
        // Run 5 game-seconds
        RunGameSeconds(5.0f);
        
        // Should have moved ~10 tiles (2 tiles/sec * 5 sec)
        float distanceTraveled = m->x / CELL_SIZE;
        expect(distanceTraveled >= 9.0f && distanceTraveled <= 11.0f);
    }
    
    it("mover should travel same game-distance regardless of game speed") {
        InitGridFlat(100, 100, 1);
        
        // Test at 1x speed
        Mover* m1 = CreateMover(0, 0, 0);
        m1->speed = 2.0f;
        SetMoverGoal(m1, 50, 0, 0);
        gameSpeed = 1.0f;
        RunGameSeconds(5.0f);
        float dist1 = m1->x / CELL_SIZE;
        
        // Reset and test at 10x speed
        ClearMovers();
        gameTime = 0.0f;
        Mover* m2 = CreateMover(0, 0, 0);
        m2->speed = 2.0f;
        SetMoverGoal(m2, 50, 0, 0);
        gameSpeed = 10.0f;
        RunGameSeconds(5.0f);  // Same game-time, 10x faster real-time
        float dist2 = m2->x / CELL_SIZE;
        
        // Both should have traveled same game-distance
        expect(fabsf(dist1 - dist2) < 1.0f);
    }
}

describe(time_system_fire) {
    it("fire should spread after specified interval") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        SetCell(5, 6, 0, CELL_GRASS);
        fireSpreadInterval = 2.0f;  // Spread every 2 game-seconds
        gameSpeed = 1.0f;
        
        // Start fire at center
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        
        // Run 1 game-second — fire should NOT have spread yet
        RunGameSeconds(1.0f);
        expect(GetFireLevel(5, 6, 0) == 0);
        
        // Run 2 more game-seconds — fire should have spread
        RunGameSeconds(2.0f);
        expect(GetFireLevel(5, 6, 0) > 0);
    }
    
    it("fire should spread at same game-time regardless of game speed") {
        // Test at 1x
        InitGridFlat(10, 10, 1);
        FillRect(0, 0, 10, 10, 0, CELL_GRASS);
        fireSpreadInterval = 1.0f;
        gameSpeed = 1.0f;
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        RunGameSeconds(5.0f);
        int spread1 = CountFireCells();
        
        // Test at 100x (same game-time, faster real-time)
        InitGridFlat(10, 10, 1);
        FillRect(0, 0, 10, 10, 0, CELL_GRASS);
        ClearFire();
        gameTime = 0.0f;
        gameSpeed = 100.0f;
        StartFire(5, 5, 0, FIRE_MAX_LEVEL);
        RunGameSeconds(5.0f);
        int spread2 = CountFireCells();
        
        // Should have similar spread (not exact due to randomness)
        expect(abs(spread1 - spread2) < 5);
    }
}

describe(time_system_temperature) {
    it("heat should transfer after specified interval") {
        InitGridFlat(10, 10, 1);
        heatTransferInterval = 1.0f;  // Transfer every 1 game-second
        gameSpeed = 1.0f;
        
        // Hot cell next to cold cell
        SetTemperature(5, 5, 0, 200);
        SetTemperature(5, 6, 0, 20);
        int initialCold = GetTemperature(5, 6, 0);
        
        // Run 0.5 seconds — no transfer yet
        RunGameSeconds(0.5f);
        expect(GetTemperature(5, 6, 0) == initialCold);
        
        // Run 1 more second — should have transferred
        RunGameSeconds(1.0f);
        expect(GetTemperature(5, 6, 0) > initialCold);
    }
}

describe(time_system_ground_wear) {
    it("grass should recover after specified time") {
        InitGridFlat(10, 10, 1);
        SetCell(5, 5, 0, CELL_GRASS);
        wearRecoveryTime = 10.0f;  // Full recovery in 10 game-seconds
        gameSpeed = 1.0f;
        
        // Fully wear the grass
        SetWear(5, 5, wearMax);
        expect(GetCell(5, 5, 0) == CELL_DIRT);  // Worn to dirt
        
        // Run 5 seconds — partial recovery
        RunGameSeconds(5.0f);
        expect(GetWear(5, 5) < wearMax);
        expect(GetWear(5, 5) > 0);
        
        // Run 10 more seconds — full recovery
        RunGameSeconds(10.0f);
        expect(GetWear(5, 5) == 0);
        expect(GetCell(5, 5, 0) == CELL_GRASS);
    }
}

describe(time_system_day_night) {
    it("should complete full day cycle in dayLength game-seconds") {
        dayLength = 100.0f;  // 100 game-seconds per day
        timeOfDay = 0.0f;
        dayNumber = 1;
        gameSpeed = 1.0f;
        
        RunGameSeconds(100.0f);
        
        expect(dayNumber == 2);
        expect(timeOfDay >= 0.0f && timeOfDay < 0.5f);
    }
    
    it("should track multiple days correctly") {
        dayLength = 60.0f;
        timeOfDay = 0.0f;
        dayNumber = 1;
        gameSpeed = 10.0f;
        
        RunGameSeconds(600.0f);  // 10 days worth
        
        expect(dayNumber == 11);
    }
}
```

### Stress/Large Scale Tests

```c
describe(time_system_large_scales) {
    it("should handle 10 days of game time") {
        // dayLength = 3600: 10 days = 36000 game-seconds
        dayLength = 3600.0f;
        gameSpeed = 600.0f;  // 600 game-sec per real-sec
        float startTime = gameTime;
        
        RunGameSeconds(36000.0f);  // 10 game-days = 60 real-seconds
        
        expect(gameTime - startTime >= 35999.0f);
        expect(gameTime - startTime <= 36001.0f);
    }
    
    it("should advance day counter correctly over many days") {
        dayNumber = 1;
        timeOfDay = 0.0f;
        dayLength = 3600.0f;  // 1 game-hour per day
        gameSpeed = 3600.0f;  // 1 day per real-second (3600 game-sec/real-sec)
        
        RunGameSeconds(360000.0f);  // 100 game-days
        
        expect(dayNumber == 101);
    }
    
    it("should regrow grass after 10 days") {
        // Setup: worn grass tile
        SetCell(5, 5, 0, CELL_DIRT);
        SetWear(5, 5, wearMax);  // Fully worn
        dayLength = 3600.0f;
        
        // Fast forward 10 days (36000 game-seconds)
        gameSpeed = 600.0f;  // 10 game-minutes per real-second
        RunGameSeconds(36000.0f);  // Takes 60 real-seconds
        
        // Grass should have recovered
        expect(GetCell(5, 5, 0) == CELL_GRASS);
        expect(GetWear(5, 5) == 0);
    }
    
    it("should not lose precision at high game time values") {
        // 1,000,000 game-seconds = 277 days (if dayLength=3600)
        gameTime = 1000000.0;
        float before = gameTime;
        
        gameSpeed = 1.0f;
        Tick();  // Add TICK_DT * 1.0 = 0.0166...
        
        expect(gameTime > before);  // Should still increment
        expect(gameTime - before > 0.01f);  // With reasonable precision
    }
    
    it("should handle extreme game speeds without hanging") {
        gameSpeed = 10000.0f;  // 10000 game-sec per real-sec
        
        float startReal = GetTime();
        RunGameSeconds(1000000.0f);  // 1 million game-seconds = 100 real-seconds
        float elapsed = GetTime() - startReal;
        
        // Should complete in reasonable real-time
        expect(elapsed < 120.0f);
    }
}
```

## Design Decisions

### Mover Movement at High Game Speeds

At high `gameSpeed`, movers move further per tick. Paths don't go through walls, but avoidance forces could theoretically push movers through walls if movement per tick exceeds wall thickness.

**Decision:** Keep the current avoidance/movement code simple. Don't add expensive sub-stepping or collision sweeps. Accept that at extreme speeds (1000x+) there may be edge cases.

**TODO:** Add benchmark tests to verify performance doesn't degrade at high game speeds. Add tests to check movers don't end up inside walls at various speeds (1x, 10x, 100x, 500x).

```c
describe(mover_high_speed_safety) {
    it("movers should not end up inside walls at 100x speed") {
        InitGridWithWalls();  // Grid with some walls
        SpawnMoversWithGoals(50);
        gameSpeed = 100.0f;
        
        RunGameSeconds(60.0f);
        
        for (int i = 0; i < moverCount; i++) {
            if (!movers[i].active) continue;
            int tx = (int)(movers[i].x / CELL_SIZE);
            int ty = (int)(movers[i].y / CELL_SIZE);
            expect(!IsWall(tx, ty, 0));  // No mover inside a wall
        }
    }
    
    it("movement performance should not degrade significantly at high speed") {
        InitGridFlat(200, 200, 1);
        SpawnMoversWithGoals(500);
        
        // Benchmark at 1x
        gameSpeed = 1.0f;
        float start1x = GetTime();
        for (int i = 0; i < 600; i++) Tick();  // 10 real-seconds of ticks
        float time1x = GetTime() - start1x;
        
        // Benchmark at 100x
        ClearMovers();
        SpawnMoversWithGoals(500);
        gameSpeed = 100.0f;
        float start100x = GetTime();
        for (int i = 0; i < 600; i++) Tick();  // Same number of ticks
        float time100x = GetTime() - start100x;
        
        // Should be similar (within 2x)
        expect(time100x < time1x * 2.0f);
    }
}
```

### Randomness

Current systems use `rand() % chance` for probabilistic behavior. At different game speeds, the number of random calls per game-second changes (more ticks at low speed, fewer at high speed).

**Decision:** We don't need full deterministic replay across different game speeds. However, tests should be reproducible.

**For reproducible tests:** Seed `srand()` at the start of each test. Tests will be deterministic as long as they run at the same `gameSpeed`.

```c
void ResetTestState(unsigned int seed) {
    srand(seed);
    gameTime = 0.0f;
    gameSpeed = 1.0f;
    timeOfDay = 6.0f;
    dayNumber = 1;
    // Clear system accumulators
    fireSpreadAccum = 0.0f;
    smokeAccum = 0.0f;
    // etc...
}

// Usage
it("fire should spread predictably") {
    ResetTestState(12345);  // Same seed = same results every run
    InitGridFlat(10, 10, 1);
    SetCell(5, 5, 0, CELL_GRASS);
    StartFire(5, 5, 0, FIRE_MAX_LEVEL);
    
    RunGameSeconds(10.0f);
    
    // This assertion will pass consistently
    expect(CountFireCells() == 7);
}
```

Note: Running the same test at different `gameSpeed` values may produce different results (different tick count = different `rand()` calls). This is acceptable — just use a consistent speed in tests.

### Pause Behavior

**Decision:** When `gameSpeed = 0`, nothing simulates. No time accumulates, no systems update. True pause.

```c
void Tick(void) {
    if (gameSpeed <= 0.0f) return;  // Early out when paused
    
    gameDeltaTime = TICK_DT * gameSpeed;
    gameTime += gameDeltaTime;
    // ... rest of tick
}
```

### Day Length Presets

Reasonable `dayLength` values for different feels:

| Preset | dayLength | Feel |
|--------|-----------|------|
| Fast (testing) | 24.0 | 24 real-seconds = 1 day |
| Quick | 60.0 | 1 real-minute = 1 day |
| Standard | 720.0 | 12 real-minutes = 1 day |
| Slow | 1440.0 | 24 real-minutes = 1 day |

**Default recommendation:** `dayLength = 60.0f` (1 real-minute = 1 day). Fast enough to see day/night cycles regularly, slow enough to feel meaningful.

UI could offer presets:
```c
if (Button("Fast")) dayLength = 24.0f;
if (Button("Normal")) dayLength = 60.0f;
if (Button("Slow")) dayLength = 720.0f;
```

## Priority

**High priority.** The current opaque tick-based variables are blocking sensible tuning of simulation parameters. Implement soon.

## Summary

| Before | After |
|--------|-------|
| `fireSpreadChance = 10` | `fireSpreadInterval = 2.0f` (every 2 sec) |
| `MOVER_SPEED = 200.0f` | `moverSpeed = 6.0f` (6 tiles/sec) |
| `heatTransferSpeed = 50` | `heatTransferInterval = 60.0f` (every minute) |
| No game speed | `gameSpeed = 1.0f` to `100.0f` |
| No day/night | `timeOfDay`, `dayLength` |
| `RunTempTicks(1000)` (~17 sec) | `RunGameSeconds(17.0f)` |
| `RunTempTicks(3600)` (1 min) | `RunGameSeconds(60.0f)` |
