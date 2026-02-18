#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include <math.h>

static bool test_verbose = false;

// Helper: float approximate equality
static bool approx(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

describe(balance_derived_rates) {
    it("should derive hunger drain from hoursToStarve") {
        InitBalance();

        expect(approx(balance.hungerDrainPerGH, 1.0f / balance.hoursToStarve, 0.0001f));
    }

    it("should derive energy drain rates") {
        InitBalance();

        expect(approx(balance.energyDrainWorkPerGH, 1.0f / balance.hoursToExhaustWorking, 0.0001f));
        expect(approx(balance.energyDrainIdlePerGH, 1.0f / balance.hoursToExhaustIdle, 0.0001f));
    }

    it("should derive recovery rates from sleep hours and recovery range") {
        InitBalance();

        float range = balance.energyWakeThreshold - balance.energyExhaustedThreshold;
        expect(range > 0.0f);
        expect(approx(balance.bedRecoveryPerGH, range / balance.sleepHoursInBed, 0.0001f));
        expect(approx(balance.groundRecoveryPerGH, range / balance.sleepOnGround, 0.0001f));
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

// Helper: run a mover for a given number of game-hours and return distance traveled
static float RunMoverForGameHours(float gameHours, float testDayLength) {
    // Set up grid: 64x4 walkable corridor at z=1 with solid floor at z=0
    InitGridWithSizeAndChunkSize(64, 4, 64, 4);
    for (int x = 0; x < 64; x++) {
        for (int y = 0; y < 4; y++) {
            grid[0][y][x] = CELL_WALL;
            grid[1][y][x] = CELL_AIR;
        }
    }
    ClearMovers();
    ClearJobs();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    InitMoverSpatialGrid(64 * CELL_SIZE, 4 * CELL_SIZE);

    dayLength = testDayLength;
    gameSpeed = 1.0f;
    gameDeltaTime = TICK_DT * gameSpeed;

    // Create a long straight path from x=2 to x=60 along y=2, z=1
    Point path[59];
    int pathLen = 0;
    for (int x = 60; x >= 2; x--) {
        path[pathLen++] = (Point){ x, 2, 1 };
    }

    Mover* m = &movers[0];
    float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
    float startY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
    InitMoverWithPath(m, startX, startY, 1.0f, path[0], 200.0f, path, pathLen);
    m->active = true;
    moverCount = 1;

    // Run for gameHours worth of ticks
    float gameSeconds = GameHoursToGameSeconds(gameHours);
    int ticks = (int)(gameSeconds / TICK_DT);

    for (int t = 0; t < ticks; t++) {
        currentTick = t;
        gameDeltaTime = TICK_DT * gameSpeed;
        UpdateMovers();
    }

    float dx = m->x - startX;
    float dy = m->y - startY;
    return sqrtf(dx * dx + dy * dy);
}

describe(balance_movement_scaling) {
    it("mover should travel same distance per game-hour at different dayLengths") {
        InitBalance();

        float dist60 = RunMoverForGameHours(1.0f, 60.0f);
        float dist720 = RunMoverForGameHours(1.0f, 720.0f);

        if (test_verbose) {
            printf("  dayLength=60:  dist=%.1f px in 1 GH\n", dist60);
            printf("  dayLength=720: dist=%.1f px in 1 GH\n", dist720);
            printf("  ratio: %.3f (should be ~1.0)\n", dist720 / dist60);
        }

        // Allow 15% tolerance — waypoint snap overhead differs with arrival radius
        float ratio = dist720 / dist60;
        expect(ratio > 0.85f);
        expect(ratio < 1.15f);
    }

    it("mover distance should scale linearly with game-hours") {
        InitBalance();

        float dist1 = RunMoverForGameHours(1.0f, 60.0f);
        float dist2 = RunMoverForGameHours(2.0f, 60.0f);

        if (test_verbose) {
            printf("  1 GH: %.1f px, 2 GH: %.1f px, ratio: %.3f\n",
                   dist1, dist2, dist2 / dist1);
        }

        // 2 hours should be ~2x distance (within 5%)
        float ratio = dist2 / dist1;
        expect(ratio > 1.90f);
        expect(ratio < 2.10f);
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') set_quiet_mode(1);
    }

    // Restore dayLength after tests
    float savedDayLength = dayLength;

    test(balance_derived_rates);
    test(balance_time_conversion);
    test(balance_budget_consistency);
    test(balance_movement_scaling);

    dayLength = savedDayLength;
    return 0;
}
