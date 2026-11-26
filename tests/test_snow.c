// tests/test_snow.c
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/groundwear.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/entities/mover.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int test_verbose = 0;

// Helper to set ambient temperature for tests
static void SetTestTemperature(int celsius) {
    SetAmbientSurfaceTemp(celsius);
}

// ========== Snow Grid Basics ==========
describe(snow_grid_basics) {
    it("initializes to zero") {
        InitTestGrid(10, 10);
        InitSnow();
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(GetSnowLevel(x, y, z) == 0);
                }
            }
        }
    }
    
    it("sets and gets snow levels") {
        InitTestGrid(10, 10);
        InitSnow();
        
        SetSnowLevel(5, 5, 2, 1);
        expect(GetSnowLevel(5, 5, 2) == 1);
        
        SetSnowLevel(3, 7, 1, 2);
        expect(GetSnowLevel(3, 7, 1) == 2);
        
        SetSnowLevel(1, 1, 0, 3);
        expect(GetSnowLevel(1, 1, 0) == 3);
    }
    
    it("clamps snow levels to 0-3") {
        InitTestGrid(10, 10);
        InitSnow();
        
        SetSnowLevel(5, 5, 2, 5);  // Should clamp to 3
        expect(GetSnowLevel(5, 5, 2) == 3);
        
        SetSnowLevel(3, 3, 1, 255);  // Should clamp to 3
        expect(GetSnowLevel(3, 3, 1) == 3);
    }
    
    it("handles out-of-bounds safely") {
        InitTestGrid(10, 10);
        InitSnow();
        
        // Should not crash
        expect(GetSnowLevel(-1, 5, 2) == 0);
        expect(GetSnowLevel(5, -1, 2) == 0);
        expect(GetSnowLevel(5, 5, -1) == 0);
        expect(GetSnowLevel(100, 5, 2) == 0);
        expect(GetSnowLevel(5, 100, 2) == 0);
        expect(GetSnowLevel(5, 5, 100) == 0);
    }
}

// ========== Snow Accumulation ==========
describe(snow_accumulation) {
    it("accumulates during WEATHER_SNOW on exposed cells") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        seasonalAmplitude = 0;
        diurnalAmplitude = 0;
        
        // Set up exposed dirt cell (CELL_WALL with dirt material)
        GRID(5, 5, 0) = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        
        // Force snowy weather and cold temperature
        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);  // Below freezing
        
        // Before snow
        expect(GetSnowLevel(5, 5, 0) == 0);
        
        // Run multiple ticks: 12 game-seconds = 4.8 game-hours (a level takes 4; snow is
        // updated every SNOW_TICK_HOURS = 0.25 game-hours, so exactly 4 can land just short)
        for (int i = 0; i < 120; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Should have accumulated snow
        expect(GetSnowLevel(5, 5, 0) > 0);
    }
    
    it("does not accumulate on sheltered cells") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        
        // Set up dirt with roof above
        GRID(5, 5, 0) = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        GRID(5, 5, 1) = CELL_WALL;  // Roof
        
        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);
        
        for (int i = 0; i < 100; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Should not accumulate under roof
        expect(GetSnowLevel(5, 5, 0) == 0);
    }

    // Water isn't a cell type: a sea is air with water in it. Snow used to settle through it
    // onto the bed below (the island had snow on every sea column)
    it("does not settle on ground under water") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWater();
        InitWeather();
        seasonalAmplitude = 0;
        diurnalAmplitude = 0;

        GRID(5, 5, 0) = CELL_WALL;                 // bed under water
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetWaterLevel(5, 5, 1, 7);
        GRID(6, 5, 0) = CELL_WALL;                 // dry ground beside it
        SetWallMaterial(6, 5, 0, MAT_DIRT);

        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);
        for (int i = 0; i < 120; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }

        expect(GetSnowLevel(6, 5, 0) > 0);         // it did snow
        expect(GetSnowLevel(5, 5, 0) == 0);
        InitWater();                               // later tests use this cell dry
    }

    it("clears snow that water has covered") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWater();
        InitWeather();
        seasonalAmplitude = 0;
        diurnalAmplitude = 0;

        GRID(5, 5, 0) = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 2);
        SetWaterLevel(5, 5, 1, 4);                 // a flood or the tide comes in

        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);
        for (int i = 0; i < 20; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }

        expect(GetSnowLevel(5, 5, 0) == 0);
        InitWater();                               // later tests use this cell dry
    }

    // Ice you can walk on is the surface: snow lies on it (and goes when it thaws)
    it("settles on ice you can walk on, and goes when the ice thaws") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWater();
        InitWeather();
        seasonalAmplitude = 0;
        diurnalAmplitude = 0;

        GRID(5, 5, 0) = CELL_WALL;                 // lake bed
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetWaterLevel(5, 5, 1, 7);
        FreezeWater(5, 5, 1);
        GRID(4, 5, 0) = CELL_WALL;                 // thin ice: shallow frozen water, no snow
        SetWallMaterial(4, 5, 0, MAT_DIRT);
        SetWaterLevel(4, 5, 1, 2);
        FreezeWater(4, 5, 1);

        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);
        for (int i = 0; i < 120; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        expect(GetSnowLevel(5, 5, 1) > 0);         // on the ice
        expect(GetSnowLevel(5, 5, 0) == 0);        // not on the bed under it
        expect(GetSnowLevel(4, 5, 1) == 0 && GetSnowLevel(4, 5, 0) == 0);

        ThawWater(5, 5, 1);
        for (int i = 0; i < 20; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        expect(GetSnowLevel(5, 5, 1) == 0);
        InitWater();                               // later tests use these cells dry
    }
    
    it("accumulates faster during high intensity snow") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        
        // Two exposed cells
        GRID(3, 5, 0) = CELL_WALL; SetWallMaterial(3, 5, 0, MAT_DIRT);
        GRID(7, 5, 0) = CELL_WALL; SetWallMaterial(7, 5, 0, MAT_DIRT);
        
        SetTestTemperature(-5);
        
        // Light snow on one cell
        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 0.3f;
        for (int i = 0; i < 50; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
            // Only update cell (3,5)
            weatherState.intensity = (i < 50) ? 0.3f : 1.0f;
        }
        int lightSnow = GetSnowLevel(3, 5, 0);
        
        // Reset
        SetSnowLevel(3, 5, 0, 0);
        SetSnowLevel(7, 5, 0, 0);
        
        // Heavy snow
        weatherState.intensity = 1.0f;
        for (int i = 0; i < 50; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        int heavySnow = GetSnowLevel(7, 5, 0);
        
        // Heavy should accumulate more
        expect(heavySnow >= lightSnow);
    }
    
    it("only accumulates below freezing temperature") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        
        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        
        // Warm temperature - should not accumulate
        SetTestTemperature(5);  // Above freezing
        
        for (int i = 0; i < 100; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        expect(GetSnowLevel(5, 5, 0) == 0);
    }
}

// ========== Snow Melting ==========
describe(snow_melting) {
    it("melts snow that appears after a pass found none (SetSnowLevel or a load)") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        GRID(2, 2, 0) = CELL_WALL; SetWallMaterial(2, 2, 0, MAT_DIRT);
        SetTestTemperature(5);
        for (int i = 0; i < 50; i++) { gameDeltaTime = 0.1f; UpdateSnow(); }   // no snow: passes go idle
        SetSnowLevel(5, 5, 0, 2);
        SNOW(2, 2, 0) = 2; SnowGridChanged();    // what loading a save does
        for (int i = 0; i < 250; i++) { gameDeltaTime = 0.1f; UpdateSnow(); }
        expect(GetSnowLevel(5, 5, 0) < 2);
        expect(GetSnowLevel(2, 2, 0) < 2);
    }

    it("melts above freezing and increases wetness") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        
        // Set up dirt with snow (natural soil: melt water soaks into it, moisture.c)
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        WALL_NATURAL(5, 5, 0) = 1;
        SetSnowLevel(5, 5, 0, 2);
        
        // Warm temperature
        SetTestTemperature(5);
        
        uint8_t initialSnow = GetSnowLevel(5, 5, 0);
        uint8_t initialWetness = GET_CELL_WETNESS(5, 5, 0);
        
        // Run enough ticks for melting (1/0.05 = 20s per level, so 200 ticks of 0.1s)
        for (int i = 0; i < 250; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Snow should decrease
        expect(GetSnowLevel(5, 5, 0) < initialSnow);
        
        // Wetness should increase
        expect(GET_CELL_WETNESS(5, 5, 0) > initialWetness);
    }
    
    it("creates mud on dirt when snow melts") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        InitGroundWear();  // Need this for IsMuddy to work
        
        // Dirt cell with heavy snow
        GRID(5, 5, 0) = CELL_WALL; 
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        WALL_NATURAL(5, 5, 0) = 1;  // Mark as natural soil
        SetSnowLevel(5, 5, 0, 3);
        
        expect(!IsMuddy(5, 5, 0));  // Not muddy initially
        
        // Warm temperature to melt
        SetTestTemperature(5);
        
        // Run enough ticks to melt all 3 levels (60s = 600 ticks)
        for (int i = 0; i < 700; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Should be muddy from melted snow (wetness >= 2)
        expect(IsMuddy(5, 5, 0));
    }
    
    it("persists at or below freezing") {
        InitTestGrid(10, 10);
        InitSnow();
        InitWeather();
        
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 2);
        
        // Cold temperature
        SetTestTemperature(-5);
        
        for (int i = 0; i < 100; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Snow should not melt
        expect(GetSnowLevel(5, 5, 0) == 2);
    }
}

// ========== Snow Movement Penalty ==========
describe(snow_movement) {
    it("applies light snow penalty (0.85x speed)") {
        InitTestGrid(10, 10);
        InitSnow();
        
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 1);  // Light snow
        
        float mult = GetSnowSpeedMultiplier(5, 5, 0);
        expect(fabs(mult - 0.85f) < 0.01f);
    }
    
    it("applies heavy snow penalty (0.6x speed)") {
        InitTestGrid(10, 10);
        InitSnow();
        
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 3);  // Heavy snow
        
        float mult = GetSnowSpeedMultiplier(5, 5, 0);
        expect(fabs(mult - 0.6f) < 0.01f);
    }
    
    it("no penalty without snow") {
        InitTestGrid(10, 10);
        InitSnow();
        
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 0);  // No snow
        
        float mult = GetSnowSpeedMultiplier(5, 5, 0);
        expect(mult == 1.0f);
    }
}

// ========== Snow Fire Interaction ==========
describe(snow_fire_interaction) {
    it("extinguishes fire on snowy cells") {
        InitTestGrid(10, 10);
        InitSnow();
        InitFire();
        
        // Flammable cell with fire
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetFireLevel(5, 5, 0, 5);
        FIRE_MUT(5, 5, 0).fuel = 100;  // Give it fuel so it doesn't die on its own
        
        // Add moderate snow
        SetSnowLevel(5, 5, 0, 2);
        
        // Run fire update
        UpdateFire();
        
        // Fire should be immediately extinguished by snow
        expect(GetFireLevel(5, 5, 0) == 0);
    }
    
    it("does not extinguish fire with light snow") {
        InitTestGrid(10, 10);
        InitSnow();
        InitFire();
        
        GRID(5, 5, 0) = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetFireLevel(5, 5, 0, 5);
        FIRE_MUT(5, 5, 0).fuel = 100;  // Give it fuel
        SetSnowLevel(5, 5, 0, 1);  // Light snow (< 2, shouldn't extinguish)
        
        // Run fire update
        UpdateFire();
        
        // Fire should still exist (light snow doesn't extinguish)
        expect(GetFireLevel(5, 5, 0) > 0);
    }
}

// ========== Cloud Shadows ==========
describe(cloud_shadow) {
    // Shadows are the cloud map's density (weather.c GetCloudDensity) times how dark the weather's
    // clouds are; the clouds move by drifting with the wind (UpdateWeather), not with the clock
    it("varies intensity by weather type") {
        InitTestGrid(64, 64);
        InitWeather();
        float most[3] = {0};
        static const WeatherType w[3] = { WEATHER_CLEAR, WEATHER_CLOUDY, WEATHER_RAIN };
        for (int k = 0; k < 3; k++) {
            weatherState.current = w[k];
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++) if (GetCloudShadow(x, y, 0.0f) > most[k]) most[k] = GetCloudShadow(x, y, 0.0f);
        }
        expect(most[0] == 0.0f);         // clear: no shadows
        expect(most[1] > 0.0f);          // cloudy: some
        expect(most[2] >= most[1]);      // rain: darker
    }

    it("varies by position") {
        InitTestGrid(128, 128);
        InitWeather();
        weatherState.current = WEATHER_CLOUDY;
        int shaded = 0, open = 0;
        for (int y = 0; y < 128; y += 4)
            for (int x = 0; x < 128; x += 4) {
                if (GetCloudShadow(x, y, 0.0f) > 0.0f) shaded++; else open++;
            }
        expect(shaded > 0);
        expect(open > 0);
    }

    it("moves with wind over time") {
        InitTestGrid(128, 128);
        InitWeather();
        weatherState.current = WEATHER_CLOUDY;
        weatherState.transitionTimer = 1e9f;
        weatherState.windDirX = 1.0f;
        weatherState.windDirY = 0.0f;
        weatherState.windStrength = 2.0f;
        float before[16];
        for (int i = 0; i < 16; i++) before[i] = GetCloudShadow(i * 8, 64, 0.0f);
        for (int t = 0; t < 600; t++) { gameDeltaTime = TICK_DT; UpdateWeather(); }
        int anyDiffers = 0;
        for (int i = 0; i < 16; i++) if (GetCloudShadow(i * 8, 64, 0.0f) != before[i]) anyDiffers = 1;
        expect(anyDiffers);
    }
}

// ========== Main ==========
int main(int argc, char** argv) {
    test_verbose = c89spec_parse_args(argc, argv);
    
    test(snow_grid_basics);
    test(snow_accumulation);
    test(snow_melting);
    test(snow_movement);
    test(snow_fire_interaction);
    test(cloud_shadow);
    
    return summary();
}
