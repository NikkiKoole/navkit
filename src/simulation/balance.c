// balance.c - Game balance tuning table and time conversion utilities

#include "balance.h"
#include "../core/time.h"

BalanceTable balance;

void InitBalance(void) {
    // Time budgets (game-hours)
    balance.workHoursPerDay       = 14.0f;
    balance.sleepHoursInBed       =  7.0f;
    balance.sleepOnGround         = 17.0f;
    balance.hoursToStarve         = 24.0f;
    balance.hoursToExhaustWorking = 20.0f;
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
    balance.energyTiredThreshold     = 0.2f;
    balance.energyExhaustedThreshold = 0.1f;
    balance.energyWakeThreshold      = 0.8f;

    // Movement
    balance.baseMoverSpeed       = 200.0f;
    balance.moverSpeedVariance   = 0.25f;

    // Body temperature
    balance.bodyTempNormal           = 37.0f;
    balance.bodyTempCoolingRatePerGH =  3.0f;
    balance.bodyTempWarmingRatePerGH =  8.0f;
    balance.metabolicHeatBonus       = 30.0f;
    balance.mildColdThreshold        = 35.0f;
    balance.moderateColdThreshold    = 33.0f;
    balance.severeColdThreshold      = 30.0f;
    balance.coldSpeedPenaltyMin      =  0.6f;
    balance.coldEnergyDrainMult      =  2.0f;
    balance.hypothermiaDeathGH       =  4.0f;
    balance.heatThreshold            = 40.0f;
    balance.heatSpeedPenaltyMin      =  0.7f;

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
