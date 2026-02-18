// balance.c - Game balance tuning table and time conversion utilities

#include "balance.h"
#include "../core/time.h"

BalanceTable balance;

void InitBalance(void) {
    // Time budgets (game-hours)
    balance.workHoursPerDay       = 14.0f;
    balance.sleepHoursInBed       =  7.0f;
    balance.sleepOnGround         = 23.0f;
    balance.hoursToStarve         = 24.0f;
    balance.hoursToExhaustWorking = 16.0f;
    balance.hoursToExhaustIdle    = 28.0f;
    balance.eatingDurationGH      =  0.5f;
    balance.foodSearchCooldownGH  =  0.25f;
    balance.foodSeekTimeoutGH     =  0.5f;
    balance.starvationDeathGH     = 16.0f;
    balance.restSearchCooldownGH  =  2.0f;
    balance.restSeekTimeoutGH     =  4.0f;

    // Thresholds (0-1 scale)
    balance.hungerSeekThreshold      = 0.3f;
    balance.hungerCriticalThreshold  = 0.1f;
    balance.energyTiredThreshold     = 0.3f;
    balance.energyExhaustedThreshold = 0.1f;
    balance.energyWakeThreshold      = 0.8f;

    // Movement
    balance.baseMoverSpeed       = 200.0f;
    balance.moverSpeedVariance   = 0.25f;

    // Multipliers
    balance.nightEnergyMult       = 1.2f;
    balance.carryingEnergyMult    = 1.1f;
    balance.hungerSpeedPenaltyMin = 0.5f;
    balance.hungerPenaltyThreshold = 0.2f;

    RecalcBalanceTable();
}

void RecalcBalanceTable(void) {
    balance.hungerDrainPerGH     = 1.0f / balance.hoursToStarve;
    balance.energyDrainWorkPerGH = 1.0f / balance.hoursToExhaustWorking;
    balance.energyDrainIdlePerGH = 1.0f / balance.hoursToExhaustIdle;

    float recoveryRange = balance.energyWakeThreshold - balance.energyExhaustedThreshold;
    balance.bedRecoveryPerGH    = recoveryRange / balance.sleepHoursInBed;
    balance.groundRecoveryPerGH = recoveryRange / balance.sleepOnGround;
}

float GameHoursToGameSeconds(float gameHours) {
    return gameHours * (dayLength / 24.0f);
}

float RatePerGameSecond(float ratePerGH) {
    return ratePerGH * 24.0f / dayLength;
}
