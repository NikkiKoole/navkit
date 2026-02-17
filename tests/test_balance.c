#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include <math.h>

static bool test_verbose = false;

// Helper: float approximate equality
static bool approx(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

describe(balance_init) {
    it("should set default budget values") {
        InitBalance();

        expect(balance.workHoursPerDay == 14.0f);
        expect(balance.sleepHoursInBed == 7.0f);
        expect(balance.sleepOnGround == 23.0f);
        expect(balance.hoursToStarve == 8.0f);
        expect(balance.hoursToExhaustWorking == 16.0f);
        expect(balance.hoursToExhaustIdle == 28.0f);
        expect(balance.eatingDurationGH == 0.5f);
    }

    it("should set default thresholds") {
        InitBalance();

        expect(balance.hungerSeekThreshold == 0.3f);
        expect(balance.hungerCriticalThreshold == 0.1f);
        expect(balance.energyTiredThreshold == 0.3f);
        expect(balance.energyExhaustedThreshold == 0.1f);
        expect(balance.energyWakeThreshold == 0.8f);
    }

    it("should set default multipliers") {
        InitBalance();

        expect(balance.nightEnergyMult == 1.2f);
        expect(balance.carryingEnergyMult == 1.1f);
        expect(balance.hungerSpeedPenaltyMin == 0.5f);
        expect(balance.hungerPenaltyThreshold == 0.2f);
    }
}

describe(balance_derived_rates) {
    it("should derive hunger drain from hoursToStarve") {
        InitBalance();

        // 1.0 / 8.0 = 0.125 per game-hour
        expect(approx(balance.hungerDrainPerGH, 1.0f / 8.0f, 0.0001f));
    }

    it("should derive energy drain rates") {
        InitBalance();

        expect(approx(balance.energyDrainWorkPerGH, 1.0f / 16.0f, 0.0001f));
        expect(approx(balance.energyDrainIdlePerGH, 1.0f / 28.0f, 0.0001f));
    }

    it("should derive recovery rates from sleep hours and recovery range") {
        InitBalance();

        float range = balance.energyWakeThreshold - balance.energyExhaustedThreshold;
        // range = 0.8 - 0.1 = 0.7
        expect(approx(range, 0.7f, 0.0001f));
        expect(approx(balance.bedRecoveryPerGH, 0.7f / 7.0f, 0.0001f));
        expect(approx(balance.groundRecoveryPerGH, 0.7f / 23.0f, 0.0001f));
    }

    it("should recalculate after changing budgets") {
        InitBalance();

        balance.hoursToStarve = 4.0f;
        RecalcBalanceTable();

        expect(approx(balance.hungerDrainPerGH, 1.0f / 4.0f, 0.0001f));
    }
}

describe(balance_time_conversion) {
    it("should convert game-hours to game-seconds at default dayLength") {
        InitBalance();
        dayLength = 60.0f;

        // 1 game-hour = 60/24 = 2.5 game-seconds
        float gs = GameHoursToGameSeconds(1.0f);
        expect(approx(gs, 60.0f / 24.0f, 0.0001f));

        // 24 game-hours = 60 game-seconds (one full day)
        float fullDay = GameHoursToGameSeconds(24.0f);
        expect(approx(fullDay, 60.0f, 0.0001f));
    }

    it("should scale with dayLength at 720s") {
        InitBalance();
        dayLength = 720.0f;

        // 1 game-hour = 720/24 = 30 game-seconds
        float gs = GameHoursToGameSeconds(1.0f);
        expect(approx(gs, 30.0f, 0.0001f));

        // 24 game-hours = 720 game-seconds
        float fullDay = GameHoursToGameSeconds(24.0f);
        expect(approx(fullDay, 720.0f, 0.0001f));
    }

    it("should scale with dayLength at 24s (fast)") {
        InitBalance();
        dayLength = 24.0f;

        // 1 game-hour = 24/24 = 1 game-second
        float gs = GameHoursToGameSeconds(1.0f);
        expect(approx(gs, 1.0f, 0.0001f));
    }

    it("should convert rate per game-hour to rate per game-second") {
        InitBalance();
        dayLength = 60.0f;

        // rate of 1.0/GH at dayLength=60: 1.0 * 24/60 = 0.4/game-second
        float rps = RatePerGameSecond(1.0f);
        expect(approx(rps, 24.0f / 60.0f, 0.0001f));
    }

    it("RatePerGameSecond should be inverse of GameHoursToGameSeconds") {
        InitBalance();

        float dayLengths[] = { 24.0f, 60.0f, 720.0f, 3600.0f };
        for (int i = 0; i < 4; i++) {
            dayLength = dayLengths[i];

            // rate * interval_in_game_seconds = rate_per_GH * 1_GH = rate_per_GH
            float rate = 0.125f;  // hunger drain per GH
            float rps = RatePerGameSecond(rate);
            float oneGHinGS = GameHoursToGameSeconds(1.0f);
            float product = rps * oneGHinGS;
            expect(approx(product, rate, 0.0001f));
        }
    }

    it("drain over full starvation period should equal 1.0 regardless of dayLength") {
        InitBalance();

        float dayLengths[] = { 24.0f, 60.0f, 720.0f };
        for (int i = 0; i < 3; i++) {
            dayLength = dayLengths[i];

            float rps = RatePerGameSecond(balance.hungerDrainPerGH);
            float starvationGS = GameHoursToGameSeconds(balance.hoursToStarve);
            float totalDrain = rps * starvationGS;

            if (test_verbose) {
                printf("  dayLength=%.0f: rps=%.6f, starvGS=%.2f, drain=%.6f\n",
                       dayLength, rps, starvationGS, totalDrain);
            }
            expect(approx(totalDrain, 1.0f, 0.001f));
        }
    }

    it("energy recovery should reach wake threshold regardless of dayLength") {
        InitBalance();

        float dayLengths[] = { 24.0f, 60.0f, 720.0f };
        for (int i = 0; i < 3; i++) {
            dayLength = dayLengths[i];

            float rps = RatePerGameSecond(balance.bedRecoveryPerGH);
            float sleepGS = GameHoursToGameSeconds(balance.sleepHoursInBed);
            float recovered = rps * sleepGS;
            float expectedRange = balance.energyWakeThreshold - balance.energyExhaustedThreshold;

            expect(approx(recovered, expectedRange, 0.001f));
        }
    }
}

describe(balance_budget_consistency) {
    it("work + sleep + eat should approximate 24 hours") {
        InitBalance();

        // With 0.5h eating, 2 meals = 1h eating. work(14) + sleep(7) + eat(1) = 22h
        // Remaining 2h is unscheduled/travel time — that's fine, just check it's <= 24
        float scheduled = balance.workHoursPerDay + balance.sleepHoursInBed +
                         (balance.eatingDurationGH * 2.0f);  // assume 2 meals
        expect(scheduled <= 24.0f);
        expect(scheduled >= 20.0f);  // sanity: not wildly underbudgeted

        if (test_verbose) {
            printf("  scheduled: %.1fh (work=%.1f, sleep=%.1f, eat=%.1f)\n",
                   scheduled, balance.workHoursPerDay, balance.sleepHoursInBed,
                   balance.eatingDurationGH * 2.0f);
        }
    }

    it("starvation should be reachable within a day without eating") {
        InitBalance();
        expect(balance.hoursToStarve <= 24.0f);
    }

    it("exhaustion while working should take longer than one work period") {
        InitBalance();
        // Movers shouldn't collapse during a normal work day
        expect(balance.hoursToExhaustWorking > balance.workHoursPerDay);
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') set_quiet_mode(1);
    }

    // Restore dayLength after tests
    float savedDayLength = dayLength;

    test(balance_init);
    test(balance_derived_rates);
    test(balance_time_conversion);
    test(balance_budget_consistency);

    dayLength = savedDayLength;
    return 0;
}
