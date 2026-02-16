# Balance & Rate Tuning — Systems Design Guide

Date: 2026-02-16
Scope: Patterns for tuning interconnected rate-based systems (hunger, energy, curing, growth, decay) without breaking balance when adding new systems or changing time settings.

---

## The Core Problem

NavKit has growing number of rate-based systems: hunger drain, energy drain, rest recovery, food spoilage, wood seasoning, mud drying, crop growth, snow accumulation. Each has a "drain rate" and "recovery rate" expressed in per-second floats scattered across different headers.

When you change one (make sleep faster), it cascades: movers sleep less → work more → get hungry faster → need more food → farms need higher yield. Tuning one constant means auditing five others.

---

## Principle 1: Game-Hours, Not Real-Seconds

**The single biggest improvement.** Define all rates relative to game-time, convert to real-time in one place.

### Why

`dayLength` is configurable (24s fast / 60s normal / 720s slow). Any rate defined in real-seconds breaks when dayLength changes. A mover that sleeps 18 real-seconds at 60s days sleeps 18 real-seconds at 720s days too — but that's 7 game-hours vs 0.6 game-hours. Completely different feel.

### How

```c
// One conversion function, used everywhere
static inline float GameHoursToRealSeconds(float gameHours) {
    return gameHours * (dayLength / 24.0f);
}

// Rate conversion: "drain X per game-hour" → "drain Y per real-second"
static inline float RatePerGameHour(float ratePerGH) {
    return ratePerGH / (dayLength / 24.0f);
}
```

### Before vs After

```
BEFORE (fragile):
  #define HUNGER_DRAIN_RATE 0.002f  // per real-second, means nothing without knowing dayLength

AFTER (self-documenting):
  #define HUNGER_HOURS_TO_STARVE 8.0f  // 8 game-hours from full to zero
  // Drain rate derived: 1.0 / GameHoursToRealSeconds(8.0) per real-second
```

Every rate constant now reads as a game-design statement: "it takes 8 game-hours to starve." Change dayLength from 60s to 720s and the statement remains true.

---

## Principle 2: Design the Day Budget First

Don't pick rates and see what happens. Pick the schedule you want, then derive every rate from it.

### The Budget

```
Ideal Mover Day (24 game-hours):
  Work:    14h   (58%)
  Travel:   2h   (8%)    ← walking to jobs, food, bed
  Eat:      1h   (4%)    ← seeking food + eating
  Sleep:    7h   (30%)   ← in a plank bed (best tier)

Total: 24h
```

### Deriving Rates From Budget

| System | Budget says | Derived rate |
|--------|------------|--------------|
| Energy drain (working) | Exhaust in ~16h of work | 1.0 / 16 = 0.0625 per game-hour |
| Energy drain (idle) | Exhaust in ~28h idle | 1.0 / 28 = 0.036 per game-hour |
| Bed recovery | Full rest in 7h | 0.7 / 7 = 0.10 per game-hour (0.3→1.0) |
| Ground recovery | Full rest in 23h | 0.7 / 23 = 0.030 per game-hour |
| Hunger drain | Starve in 8h | 1.0 / 8 = 0.125 per game-hour |
| Eating duration | 30 min game-time | 0.5 game-hours |

Every number traces to one line in the budget. When sleep feels too long, you change the budget line "Sleep: 7h → 5h" and recalculate the bed recovery rate. Nothing else changes.

### Budget Validation

The budget must add up to 24h. If you add a new need (hygiene: 0.5h), something else shrinks. This forces you to think about tradeoffs explicitly:

```
Work:    14h → 13.5h   (hygiene steals from work time)
Hygiene:  0.5h          (new)
```

If needs eat too much of the day, workers become inefficient. That's a design signal, not a bug.

---

## Principle 3: One Tuning Table

All rate-defining constants live in one file. No hunting through mover.h, needs.c, weather.c.

### Structure

```c
// src/simulation/balance.h

typedef struct {
    // === TIME BUDGETS (game-hours) — the source of truth ===
    float workHoursPerDay;         // 14.0
    float sleepHoursInBed;         //  7.0
    float sleepOnGround;           // 23.0
    float sleepOnLeafPile;         // 14.0
    float sleepOnGrassMat;         // 10.0
    float sleepInChair;            // 14.0
    float hoursToStarve;           //  8.0
    float hoursToExhaustWorking;   // 16.0
    float hoursToExhaustIdle;      // 28.0
    float eatingDurationGH;        //  0.5

    // === THRESHOLDS (0-1 scale) ===
    float hungerSeekThreshold;     // 0.3  (seek food when idle)
    float hungerCriticalThreshold; // 0.1  (cancel job)
    float energyTiredThreshold;    // 0.3  (seek rest when idle)
    float energyExhaustedThreshold;// 0.1  (cancel job)
    float energyWakeThreshold;     // 0.8  (stop sleeping)

    // === DERIVED RATES (per game-hour, computed by RecalcBalanceTable) ===
    float hungerDrainPerGH;
    float energyDrainWorkPerGH;
    float energyDrainIdlePerGH;
    float bedRecoveryPerGH;
    float groundRecoveryPerGH;
    float leafPileRecoveryPerGH;
    float grassMatRecoveryPerGH;
    float chairRecoveryPerGH;

    // === MULTIPLIERS (for system interactions) ===
    float workEnergyMult;          // 1.0 (base, modified below)
    float nightEnergyMult;         // 1.2 (20% faster drain at night)
    float carryingEnergyMult;      // 1.1 (10% faster when hauling)
    float hungerSpeedPenaltyMin;   // 0.5 (50% speed when starving)
    float hungerPenaltyThreshold;  // 0.2
} BalanceTable;

extern BalanceTable balance;

// Recompute derived rates from budget values. Call after changing any budget field.
void RecalcBalanceTable(void);

// Convert game-hours to real-seconds (uses current dayLength)
float GameHoursToRealSeconds(float gameHours);

// Convert per-game-hour rate to per-real-second rate
float RatePerRealSecond(float ratePerGH);
```

### RecalcBalanceTable

```c
void RecalcBalanceTable(void) {
    balance.hungerDrainPerGH        = 1.0f / balance.hoursToStarve;
    balance.energyDrainWorkPerGH    = 1.0f / balance.hoursToExhaustWorking;
    balance.energyDrainIdlePerGH    = 1.0f / balance.hoursToExhaustIdle;
    balance.bedRecoveryPerGH        = (balance.energyWakeThreshold - balance.energyExhaustedThreshold)
                                      / balance.sleepHoursInBed;
    balance.groundRecoveryPerGH     = (balance.energyWakeThreshold - balance.energyExhaustedThreshold)
                                      / balance.sleepOnGround;
    balance.leafPileRecoveryPerGH   = (balance.energyWakeThreshold - balance.energyExhaustedThreshold)
                                      / balance.sleepOnLeafPile;
    balance.grassMatRecoveryPerGH   = (balance.energyWakeThreshold - balance.energyExhaustedThreshold)
                                      / balance.sleepOnGrassMat;
    balance.chairRecoveryPerGH      = (balance.energyWakeThreshold - balance.energyExhaustedThreshold)
                                      / balance.sleepInChair;
}
```

Change `sleepHoursInBed` from 7 to 5, call `RecalcBalanceTable()`, done. Everything downstream updates.

---

## Principle 4: Multipliers for Interactions

When systems affect each other, use multipliers on the base rate. Each multiplier is independent and small.

### Pattern

```c
float drainPerGH = balance.energyDrainIdlePerGH;

// Each modifier is independent
if (m->currentJobId >= 0)  drainPerGH = balance.energyDrainWorkPerGH;
if (IsNightTime())         drainPerGH *= balance.nightEnergyMult;     // 1.2
if (IsCarryingItem(m))     drainPerGH *= balance.carryingEnergyMult;  // 1.1

float drainPerSec = RatePerRealSecond(drainPerGH);
m->energy -= drainPerSec * gameDeltaTime;
```

### Why Multipliers

- **Additive interactions are dangerous**: drain = base + work_penalty + night_penalty can easily overshoot
- **Multiplicative interactions are bounded**: 0.0625 × 1.5 × 1.2 = 0.1125 (still reasonable)
- **Each multiplier can be toggled independently** for testing
- **Tooltips can show the breakdown**: "Energy drain: 0.06/gh × 1.5 (working) × 1.2 (night) = 0.11/gh"

### Future Systems That Would Use This

| System | Multiplier on | Value | Effect |
|--------|--------------|-------|--------|
| Working | energy drain | 1.5× | Jobs tire faster |
| Night time | energy drain | 1.2× | Natural tiredness at night |
| Carrying heavy items | energy drain | 1.1× | Hauling is tiring |
| Rain (exposed) | energy drain | 1.3× | Getting wet is exhausting |
| Cold temperature | hunger drain | 1.4× | Cold makes you hungrier |
| Hot temperature | energy drain | 1.2× | Heat is tiring |
| Covered/sheltered | curing speed | 0.7× | Slower but safer curing |
| Wind (exposed) | drying speed | 1.5× | Wind accelerates drying |

Each row is one float in the balance table. Adding "rain makes you tired" is adding one multiplier, not redesigning the energy system.

---

## Principle 5: Tooltip Projections

The most effective tuning tool is showing *when* thresholds will be hit, not just current values.

### Mover Tooltip

```
Hunger: 85%  ████████████░░  starving in 4.1h
Energy: 72%  ██████████░░░░  tired in 6.2h (working)
                              plank bed: 3.5h to full
```

### Calculation

```c
float hoursToThreshold = (currentValue - threshold) / drainRatePerGH;
float hoursToRecover = (wakeThreshold - currentValue) / recoveryRatePerGH;
```

Display in game-hours. Consistent regardless of dayLength or game speed.

### Why This Matters for Tuning

Without projections, you change a rate and play for 10 minutes to see the effect. With projections, you change a rate and immediately see "tired in 2.1h" vs "tired in 6.2h" in the tooltip. Tuning iteration goes from minutes to seconds.

---

## Principle 6: Day Schedule Forecast (Debug Tool)

A debug panel or test that simulates N game-days and prints the activity schedule:

```
=== Mover Day Forecast (plank bed available) ===
06:00  Wake (energy 0.82)
06:00  Work
20:24  Tired (energy 0.30) — seek bed
20:30  Arrive at bed, sleep
21:15  Hungry (hunger 0.30) — wake, seek food
21:30  Eat berries (hunger → 0.60)
21:45  Still tired (energy 0.38) — return to bed
03:30  Rested (energy 0.80) — wake
03:30  Work (early morning)
06:00  Day 2 starts

Day efficiency: 17.5h worked / 24h = 73%
Interruptions: 1 (hunger during sleep)
```

### What This Reveals

- **Collision detection**: hunger and energy thresholds fire at the same time → mover thrashes
- **Efficiency**: too much sleep = low productivity, too little = constant exhaustion interrupts
- **Edge cases**: mover sleeps on ground (23h!) → only works 1h → starves → death spiral
- **Furniture impact**: ground rest = 42% efficiency, plank bed = 73% efficiency → beds feel meaningful

This can be a unit test that runs without graphics:

```c
void test_day_schedule_forecast(void) {
    // Simulate 72 game-hours (3 days) with ticking energy + hunger
    // Print state transitions
    // Assert: mover works at least 12h per day with plank bed
    // Assert: mover works at least 4h per day on ground (survival minimum)
}
```

---

## Principle 7: Sensitivity Matrix

When you have 5+ interacting systems, build a matrix showing what changes when you tweak each knob:

| If you change... | Hunger | Energy | Sleep time | Work output | Food demand |
|-----------------|--------|--------|------------|-------------|-------------|
| **hoursToStarve** ↓ | Faster | — | Interrupts sleep more | Less (more eating) | Higher |
| **hoursToExhaust** ↓ | — | Faster | More sleep needed | Less (more sleeping) | Same |
| **sleepHoursInBed** ↑ | Drains more during sleep | Recovers same | Longer | Less | Higher |
| **eatingDuration** ↑ | — | Drains during eating | — | Less (more eating time) | Same |
| **dayLength** ↑ | Same (game-hours) | Same (game-hours) | Same (game-hours) | Same | Same |

The last row is the key validation: changing dayLength should affect **nothing** in the game-hours column. If it does, you have a real-seconds leak.

---

## Application to NavKit Systems

### Current Systems (need conversion)

| System | Current unit | Should be |
|--------|-------------|-----------|
| Hunger drain | per real-second (mover.h) | per game-hour |
| Energy drain | per real-second (feature-02 doc) | per game-hour |
| Food spoilage | (planned, not implemented) | per game-hour |
| Drying rack | passive timer (workshops.c) | per game-hour |
| Plant growth | per tick (plants.c) | per game-hour with season multiplier |

### Future Systems (design in game-hours from the start)

| System | Budget parameter | Rate |
|--------|-----------------|------|
| Wood seasoning | "2 seasons to season a log" | condition/game-hour |
| Clay curing | "3 days to bone-dry" | condition/game-hour |
| Crop growth | "1 season seed-to-harvest" | growth/game-hour |
| Food spoilage | "2 days fresh, 8 days dried" | decay/game-hour |
| Mover healing | "3 days to heal minor wound" | hp/game-hour |

All use the same `RatePerRealSecond(ratePerGH)` conversion. All scale with dayLength. All live in the balance table.

---

## Migration Path (Incremental)

Don't refactor everything at once. Adopt gradually:

### Step 1: Add balance.h with conversion functions
- `GameHoursToRealSeconds()`, `RatePerRealSecond()`
- Keep existing constants but document their game-hour equivalents in comments

### Step 2: Convert hunger to game-hours
- Replace `HUNGER_DRAIN_RATE` with `balance.hungerDrainPerGH` + conversion
- Verify: changing dayLength doesn't change gameplay feel

### Step 3: Implement energy using game-hours from the start (Feature 02)
- All energy rates in the balance table
- Tooltip shows game-hour projections

### Step 4: Consolidate remaining systems
- Plant growth, workshop timers, weather rates
- Each system conversion is small and independent

### Step 5: Debug panel
- Sliders for budget values
- Live recalc
- Day schedule forecast

---

## Summary

| Principle | One-liner |
|-----------|-----------|
| Game-hours not real-seconds | Rates scale with dayLength automatically |
| Day budget first | Pick the schedule, derive every rate |
| One tuning table | All constants in one file, derived from budgets |
| Multipliers for interactions | Independent, bounded, tooltip-friendly |
| Tooltip projections | Show "tired in Xh" not just "energy: 72%" |
| Day schedule forecast | Simulate 3 days, see the whole picture |
| Sensitivity matrix | Know what breaks when you touch a knob |
