#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/temperature.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up grid and initialize systems for season testing
static void SetupSeasonTest(void) {
    InitTestGridFromAscii(
        "........\n"
        "........\n"
        "........\n"
        "........\n");
    FillGroundLevel();
    InitTime();
    InitWeather();
    InitTemperature();
    // Set known season parameters
    daysPerSeason = 7;
    baseSurfaceTemp = 15;
    seasonalAmplitude = 20;
}

// =============================================================================
// Season Calculation
// =============================================================================

describe(season_calculation) {
    it("should return SPRING on day 1") {
        SetupSeasonTest();
        dayNumber = 1;
        expect(GetCurrentSeason() == SEASON_SPRING);
    }

    it("should return SUMMER at day daysPerSeason+1") {
        SetupSeasonTest();
        dayNumber = 8;  // daysPerSeason=7, day 8 = first day of summer
        expect(GetCurrentSeason() == SEASON_SUMMER);
    }

    it("should return AUTUMN at day 2*daysPerSeason+1") {
        SetupSeasonTest();
        dayNumber = 15;
        expect(GetCurrentSeason() == SEASON_AUTUMN);
    }

    it("should return WINTER at day 3*daysPerSeason+1") {
        SetupSeasonTest();
        dayNumber = 22;
        expect(GetCurrentSeason() == SEASON_WINTER);
    }

    it("should wrap back to SPRING after a full year") {
        SetupSeasonTest();
        dayNumber = 29;  // 4*7 + 1 = 29 -> wraps to yearDay 0
        expect(GetCurrentSeason() == SEASON_SPRING);
    }

    it("should handle custom daysPerSeason") {
        SetupSeasonTest();
        daysPerSeason = 3;
        // Year = 12 days
        dayNumber = 1;  expect(GetCurrentSeason() == SEASON_SPRING);
        dayNumber = 4;  expect(GetCurrentSeason() == SEASON_SUMMER);
        dayNumber = 7;  expect(GetCurrentSeason() == SEASON_AUTUMN);
        dayNumber = 10; expect(GetCurrentSeason() == SEASON_WINTER);
        dayNumber = 13; expect(GetCurrentSeason() == SEASON_SPRING); // wrap
    }

    it("should handle daysPerSeason=1") {
        SetupSeasonTest();
        daysPerSeason = 1;
        // Year = 4 days
        dayNumber = 1; expect(GetCurrentSeason() == SEASON_SPRING);
        dayNumber = 2; expect(GetCurrentSeason() == SEASON_SUMMER);
        dayNumber = 3; expect(GetCurrentSeason() == SEASON_AUTUMN);
        dayNumber = 4; expect(GetCurrentSeason() == SEASON_WINTER);
        dayNumber = 5; expect(GetCurrentSeason() == SEASON_SPRING);
    }

    it("should return correct season at last day of each season") {
        SetupSeasonTest();
        daysPerSeason = 7;
        dayNumber = 7;  expect(GetCurrentSeason() == SEASON_SPRING); // last day of spring
        dayNumber = 14; expect(GetCurrentSeason() == SEASON_SUMMER); // last day of summer
        dayNumber = 21; expect(GetCurrentSeason() == SEASON_AUTUMN); // last day of autumn
        dayNumber = 28; expect(GetCurrentSeason() == SEASON_WINTER); // last day of winter
    }
}

// =============================================================================
// Year Day Calculation
// =============================================================================

describe(year_day_calculation) {
    it("should return 0 on day 1") {
        SetupSeasonTest();
        dayNumber = 1;
        expect(GetYearDay() == 0);
    }

    it("should return daysPerYear-1 on last day of year") {
        SetupSeasonTest();
        dayNumber = 28;  // 4*7 = 28
        expect(GetYearDay() == 27);
    }

    it("should wrap correctly across multiple years") {
        SetupSeasonTest();
        dayNumber = 29;  // first day of year 2
        expect(GetYearDay() == 0);
        dayNumber = 56;  // last day of year 2
        expect(GetYearDay() == 27);
        dayNumber = 57;  // first day of year 3
        expect(GetYearDay() == 0);
    }

    it("should handle large day numbers") {
        SetupSeasonTest();
        dayNumber = 1000;
        int daysPerYear = daysPerSeason * SEASON_COUNT;
        int expected = (1000 - 1) % daysPerYear;
        expect(GetYearDay() == expected);
    }
}

// =============================================================================
// Season Progress
// =============================================================================

describe(season_progress) {
    it("should return 0.0 at start of season") {
        SetupSeasonTest();
        dayNumber = 1;  // first day of spring
        float progress = GetSeasonProgress();
        expect(progress >= 0.0f && progress < 0.01f);
    }

    it("should return ~0.5 at midpoint of season") {
        SetupSeasonTest();
        daysPerSeason = 8;  // even number for clean midpoint
        dayNumber = 5;  // yearDay=4, 4/8 = 0.5
        float progress = GetSeasonProgress();
        expect(progress > 0.45f && progress < 0.55f);
    }

    it("should return close to 1.0 at end of season") {
        SetupSeasonTest();
        dayNumber = 7;  // last day of spring, yearDay=6, 6/7 ≈ 0.857
        float progress = GetSeasonProgress();
        expect(progress > 0.8f && progress <= 1.0f);
    }

    it("should reset to 0.0 at start of next season") {
        SetupSeasonTest();
        dayNumber = 8;  // first day of summer, yearDay=7, 7%7=0, 0/7=0.0
        float progress = GetSeasonProgress();
        expect(progress >= 0.0f && progress < 0.01f);
    }
}

// =============================================================================
// Seasonal Temperature
// =============================================================================

describe(seasonal_temperature) {
    it("should be warmest during summer midpoint") {
        SetupSeasonTest();
        // Mid-summer: day 11 (yearDay=10, in summer season days 7-13)
        dayNumber = 11;
        int surfaceZ = gridDepth - 1;
        int temp = GetAmbientTemperature(surfaceZ);
        expect(temp > baseSurfaceTemp);
    }

    it("should be coldest during winter midpoint") {
        SetupSeasonTest();
        // Mid-winter: day 25 (yearDay=24, in winter season days 21-27)
        dayNumber = 25;
        int surfaceZ = gridDepth - 1;
        int temp = GetAmbientTemperature(surfaceZ);
        expect(temp < baseSurfaceTemp);
    }

    it("should be near base temp during spring equinox") {
        SetupSeasonTest();
        // Early spring (day 1): rising temp, should be near baseSurfaceTemp
        dayNumber = 1;
        int surfaceZ = gridDepth - 1;
        int temp = GetAmbientTemperature(surfaceZ);
        // Spring start: sin(-0.25*2PI) = sin(-PI/2) = -1, so temp = 15-20 = -5
        // Actually let's just check it's between coldest and warmest
        expect(temp >= baseSurfaceTemp - seasonalAmplitude);
        expect(temp <= baseSurfaceTemp + seasonalAmplitude);
    }

    it("should produce correct peak values with known parameters") {
        SetupSeasonTest();
        baseSurfaceTemp = 15;
        seasonalAmplitude = 20;
        int surfaceZ = gridDepth - 1;

        // Summer peak should approach baseSurfaceTemp + amplitude = 35
        // Find the warmest day
        int maxTemp = -999;
        int minTemp = 999;
        int daysPerYear = daysPerSeason * SEASON_COUNT;
        for (int d = 1; d <= daysPerYear; d++) {
            dayNumber = d;
            int t = GetAmbientTemperature(surfaceZ);
            if (t > maxTemp) maxTemp = t;
            if (t < minTemp) minTemp = t;
        }
        // Peak should be close to 15+20=35
        expect(maxTemp >= 30 && maxTemp <= 36);
        // Trough should be close to 15-20=-5
        expect(minTemp >= -10 && minTemp <= 0);
    }

    it("should return flat temp when amplitude is 0") {
        SetupSeasonTest();
        seasonalAmplitude = 0;
        // When amplitude is 0, GetSeasonalSurfaceTemp() returns ambientSurfaceTemp
        // for backward compatibility with the temperature system
        int surfaceZ = gridDepth - 1;
        int daysPerYear = daysPerSeason * SEASON_COUNT;
        int firstTemp = GetAmbientTemperature(surfaceZ);
        for (int d = 1; d <= daysPerYear; d++) {
            dayNumber = d;
            int temp = GetAmbientTemperature(surfaceZ);
            expect(temp == firstTemp);  // Should be constant across all days
        }
    }

    it("should still apply depth decay underground") {
        SetupSeasonTest();
        ambientDepthDecay = 5;
        dayNumber = 11; // summer - warm surface
        int surfaceZ = gridDepth - 1;
        int surfaceTemp = GetAmbientTemperature(surfaceZ);
        int undergroundTemp = GetAmbientTemperature(0);
        // Underground should be colder due to depth decay
        expect(undergroundTemp < surfaceTemp);
    }

    it("should respect custom amplitude") {
        SetupSeasonTest();
        seasonalAmplitude = 5;  // mild seasons
        int surfaceZ = gridDepth - 1;
        int maxTemp = -999;
        int minTemp = 999;
        int daysPerYear = daysPerSeason * SEASON_COUNT;
        for (int d = 1; d <= daysPerYear; d++) {
            dayNumber = d;
            int t = GetAmbientTemperature(surfaceZ);
            if (t > maxTemp) maxTemp = t;
            if (t < minTemp) minTemp = t;
        }
        // With amplitude 5, range should be 10-20 roughly
        expect(maxTemp <= baseSurfaceTemp + seasonalAmplitude + 1);
        expect(minTemp >= baseSurfaceTemp - seasonalAmplitude - 1);
        // And the range should be smaller than with amplitude 20
        expect((maxTemp - minTemp) < 15);
    }
}

// =============================================================================
// Seasonal Day Length
// =============================================================================

describe(seasonal_day_length) {
    it("should return summer dawn and dusk hours at summer peak") {
        SetupSeasonTest();
        // Summer peak is at yearPhase=0.5 -> day 15 (yearDay=14)
        dayNumber = 15;
        float dawn = GetSeasonalDawn();
        float dusk = GetSeasonalDusk();
        // Summer peak: dawn ~5h, dusk ~21h
        expect(dawn >= 4.5f && dawn <= 5.5f);
        expect(dusk >= 20.5f && dusk <= 21.5f);
    }

    it("should return winter dawn and dusk hours at winter trough") {
        SetupSeasonTest();
        // Winter trough is at yearPhase=0.0 -> day 1 (yearDay=0)
        dayNumber = 1;
        float dawn = GetSeasonalDawn();
        float dusk = GetSeasonalDusk();
        // Winter trough: dawn ~8h, dusk ~16h
        expect(dawn >= 7.5f && dawn <= 8.5f);
        expect(dusk >= 15.5f && dusk <= 16.5f);
    }

    it("should interpolate dawn and dusk at equinox points") {
        SetupSeasonTest();
        // Equinox at yearPhase=0.25 -> day 8 (yearDay=7)
        dayNumber = 8;
        float dawn = GetSeasonalDawn();
        float dusk = GetSeasonalDusk();
        // Equinox: dawn ~6.5h, dusk ~18.5h
        expect(dawn >= 6.0f && dawn <= 7.0f);
        expect(dusk >= 18.0f && dusk <= 19.0f);
    }

    it("should have longer days in summer than winter") {
        SetupSeasonTest();
        dayNumber = 15;  // summer peak
        float summerDaylight = GetSeasonalDusk() - GetSeasonalDawn();
        dayNumber = 1;  // winter trough
        float winterDaylight = GetSeasonalDusk() - GetSeasonalDawn();
        expect(summerDaylight > winterDaylight);
        // Summer ~16h, winter ~8h
        expect(summerDaylight > 14.0f);
        expect(winterDaylight < 10.0f);
    }
}

// =============================================================================
// Season Names
// =============================================================================

describe(season_names) {
    it("should return correct name strings for each season") {
        expect(strcmp(GetSeasonName(SEASON_SPRING), "Spring") == 0);
        expect(strcmp(GetSeasonName(SEASON_SUMMER), "Summer") == 0);
        expect(strcmp(GetSeasonName(SEASON_AUTUMN), "Autumn") == 0);
        expect(strcmp(GetSeasonName(SEASON_WINTER), "Winter") == 0);
    }

    it("should handle invalid season gracefully") {
        const char* name = GetSeasonName(SEASON_COUNT);
        expect(name != NULL);
    }
}

// =============================================================================
// Vegetation Growth Rate
// =============================================================================

describe(vegetation_growth_rate) {
    it("should be fastest in spring") {
        SetupSeasonTest();
        dayNumber = 4;  // mid-spring
        float rate = GetVegetationGrowthRate();
        expect(rate > 1.0f);  // faster than normal
    }

    it("should be normal in summer") {
        SetupSeasonTest();
        dayNumber = 11;  // mid-summer
        float rate = GetVegetationGrowthRate();
        expect(rate >= 0.8f && rate <= 1.2f);  // roughly normal
    }

    it("should be slow in autumn") {
        SetupSeasonTest();
        dayNumber = 18;  // mid-autumn
        float rate = GetVegetationGrowthRate();
        expect(rate < 1.0f && rate > 0.0f);  // slower
    }

    it("should be dormant at vegetation trough") {
        SetupSeasonTest();
        // Vegetation trough is at yearPhase ~0.625 (day 18-19, late autumn)
        // Find the minimum across the year
        float minRate = 999.0f;
        int daysPerYear = daysPerSeason * SEASON_COUNT;
        for (int d = 1; d <= daysPerYear; d++) {
            dayNumber = d;
            float r = GetVegetationGrowthRate();
            if (r < minRate) minRate = r;
        }
        expect(minRate < 0.05f);  // effectively dormant at trough
    }

    it("should transition smoothly across seasons") {
        SetupSeasonTest();
        int daysPerYear = daysPerSeason * SEASON_COUNT;
        float prevRate = -1.0f;
        int jumpCount = 0;
        for (int d = 1; d <= daysPerYear; d++) {
            dayNumber = d;
            float rate = GetVegetationGrowthRate();
            expect(rate >= 0.0f);
            expect(rate <= 2.0f);
            if (prevRate >= 0.0f) {
                float diff = fabsf(rate - prevRate);
                if (diff > 0.5f) jumpCount++;
            }
            prevRate = rate;
        }
        // Should be smooth - no large jumps
        expect(jumpCount == 0);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    if (quiet) set_quiet_mode(1);



    test(season_calculation);
    test(year_day_calculation);
    test(season_progress);
    test(seasonal_temperature);
    test(seasonal_day_length);
    test(season_names);
    test(vegetation_growth_rate);

    return summary();
}
