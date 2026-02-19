// balance.h - Game balance tuning table and time conversion utilities
//
// All rate-based systems should express budgets in game-hours, then convert
// to real-seconds or game-seconds using the functions here. This ensures
// gameplay scales correctly when dayLength changes.
//
// Two conversion paths:
//   1. Rate x dt pattern (hunger, energy): use RatePerGameSecond()
//   2. Accumulator + interval pattern (fire, water): use GameHoursToGameSeconds()

#ifndef BALANCE_H
#define BALANCE_H

typedef struct {
    // === TIME BUDGETS (game-hours) — source of truth ===
    float workHoursPerDay;          // 14.0
    float sleepHoursInBed;          //  7.0
    float sleepOnGround;            // 23.0
    float hoursToStarve;            //  8.0
    float hoursToExhaustWorking;    // 16.0
    float hoursToExhaustIdle;       // 28.0
    float eatingDurationGH;         //  0.5
    float foodSearchCooldownGH;     //  0.25
    float foodSeekTimeoutGH;        //  0.5
    float starvationDeathGH;        //  2.0  (game-hours at hunger 0 before death)
    float restSearchCooldownGH;     //  2.0
    float restSeekTimeoutGH;        //  4.0

    // === THRESHOLDS (0-1 scale, not time-dependent) ===
    float hungerSeekThreshold;      // 0.3
    float hungerCriticalThreshold;  // 0.1
    float energyTiredThreshold;     // 0.3
    float energyExhaustedThreshold; // 0.1
    float energyWakeThreshold;      // 0.8

    // === DERIVED RATES (per game-hour, set by RecalcBalanceTable) ===
    float hungerDrainPerGH;
    float energyDrainWorkPerGH;
    float energyDrainIdlePerGH;
    float bedRecoveryPerGH;
    float groundRecoveryPerGH;

    // === MOVEMENT ===
    float baseMoverSpeed;           // 200.0 (pixels/sec, before variance)
    float moverSpeedVariance;       // 0.25 (±25% random per mover)

    // === BODY TEMPERATURE (Celsius) ===
    float bodyTempNormal;            // 37.0
    float bodyTempCoolingRatePerGH;  // 3.0 (°C per game-hour when cooling)
    float bodyTempWarmingRatePerGH;  // 8.0 (°C per game-hour when warming)
    float metabolicHeatBonus;        // 20.0 (°C added to ambient, scaled by hunger)
    float mildColdThreshold;         // 35.0
    float moderateColdThreshold;     // 33.0
    float severeColdThreshold;       // 30.0
    float coldSpeedPenaltyMin;       // 0.6
    float coldEnergyDrainMult;       // 2.0 (energy drains 2x when moderate cold)
    float hypothermiaDeathGH;        // 4.0
    float heatThreshold;             // 40.0
    float heatSpeedPenaltyMin;       // 0.7

    // === MULTIPLIERS (for system interactions) ===
    float nightEnergyMult;          // 1.2
    float carryingEnergyMult;       // 1.1
    float hungerSpeedPenaltyMin;    // 0.5
    float hungerPenaltyThreshold;   // 0.2
} BalanceTable;

extern BalanceTable balance;

// Initialize balance table with defaults and derive rates.
// Call once at startup.
void InitBalance(void);

// Recalculate derived rates from budget values.
// Call after changing any budget field at runtime.
void RecalcBalanceTable(void);

// Convert game-hours to game-seconds (for interval/accumulator systems).
// 1 game-hour = dayLength / 24.0 game-seconds.
float GameHoursToGameSeconds(float gameHours);

// Convert a per-game-hour rate to per-game-second rate (for rate*dt systems).
// Inverse relationship: RatePerGameSecond(r) * GameHoursToGameSeconds(1) == r
float RatePerGameSecond(float ratePerGH);

#endif
