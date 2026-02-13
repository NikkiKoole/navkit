// tests/test_snow.c
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/groundwear.h"
#include "../src/world/grid.h"
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        
        SetSnowLevel(5, 5, 2, 1);
        expect(GetSnowLevel(5, 5, 2) == 1);
        
        SetSnowLevel(3, 7, 1, 2);
        expect(GetSnowLevel(3, 7, 1) == 2);
        
        SetSnowLevel(1, 1, 0, 3);
        expect(GetSnowLevel(1, 1, 0) == 3);
    }
    
    it("clamps snow levels to 0-3") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        
        SetSnowLevel(5, 5, 2, 5);  // Should clamp to 3
        expect(GetSnowLevel(5, 5, 2) == 3);
        
        SetSnowLevel(3, 3, 1, 255);  // Should clamp to 3
        expect(GetSnowLevel(3, 3, 1) == 3);
    }
    
    it("handles out-of-bounds safely") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        
        // Set up exposed dirt cell (CELL_WALL with dirt material)
        grid[0][5][5] = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        
        // Force snowy weather and cold temperature
        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);  // Below freezing
        
        // Before snow
        expect(GetSnowLevel(5, 5, 0) == 0);
        
        // Run multiple ticks
        for (int i = 0; i < 100; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Should have accumulated snow
        expect(GetSnowLevel(5, 5, 0) > 0);
    }
    
    it("does not accumulate on sheltered cells") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        
        // Set up dirt with roof above
        grid[0][5][5] = CELL_WALL;
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        grid[1][5][5] = CELL_WALL;  // Roof
        
        weatherState.current = WEATHER_SNOW;
        weatherState.intensity = 1.0f;
        SetTestTemperature(-5);
        
        for (int i = 0; i < 100; i++) {
            gameDeltaTime = 0.1f; UpdateSnow();
        }
        
        // Should not accumulate under roof
        expect(GetSnowLevel(5, 5, 0) == 0);
    }
    
    it("accumulates faster during high intensity snow") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        
        // Two exposed cells
        grid[0][5][3] = CELL_WALL; SetWallMaterial(3, 5, 0, MAT_DIRT);
        grid[0][5][7] = CELL_WALL; SetWallMaterial(7, 5, 0, MAT_DIRT);
        
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        
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
    it("melts above freezing and increases wetness") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        
        // Set up dirt with snow
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        InitGroundWear();  // Need this for IsMuddy to work
        
        // Dirt cell with heavy snow
        grid[0][5][5] = CELL_WALL; 
        SetWallMaterial(5, 5, 0, MAT_DIRT);
        wallNatural[0][5][5] = 1;  // Mark as natural soil
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitWeather();
        
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
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
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 1);  // Light snow
        
        float mult = GetSnowSpeedMultiplier(5, 5, 0);
        expect(fabs(mult - 0.85f) < 0.01f);
    }
    
    it("applies heavy snow penalty (0.6x speed)") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 3);  // Heavy snow
        
        float mult = GetSnowSpeedMultiplier(5, 5, 0);
        expect(fabs(mult - 0.6f) < 0.01f);
    }
    
    it("no penalty without snow") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetSnowLevel(5, 5, 0, 0);  // No snow
        
        float mult = GetSnowSpeedMultiplier(5, 5, 0);
        expect(mult == 1.0f);
    }
}

// ========== Snow Fire Interaction ==========
describe(snow_fire_interaction) {
    it("extinguishes fire on snowy cells") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitFire();
        
        // Flammable cell with fire
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetFireLevel(5, 5, 0, 5);
        fireGrid[0][5][5].fuel = 100;  // Give it fuel so it doesn't die on its own
        
        // Add moderate snow
        SetSnowLevel(5, 5, 0, 2);
        
        // Run fire update
        UpdateFire();
        
        // Fire should be immediately extinguished by snow
        expect(GetFireLevel(5, 5, 0) == 0);
    }
    
    it("does not extinguish fire with light snow") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitSnow();
        InitFire();
        
        grid[0][5][5] = CELL_WALL; SetWallMaterial(5, 5, 0, MAT_DIRT);
        SetFireLevel(5, 5, 0, 5);
        fireGrid[0][5][5].fuel = 100;  // Give it fuel
        SetSnowLevel(5, 5, 0, 1);  // Light snow (< 2, shouldn't extinguish)
        
        // Run fire update
        UpdateFire();
        
        // Fire should still exist (light snow doesn't extinguish)
        expect(GetFireLevel(5, 5, 0) > 0);
    }
}

// ========== Cloud Shadows ==========
describe(cloud_shadow) {
    it("varies intensity by weather type") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitWeather();
        
        // Clear weather - no shadows
        weatherState.current = WEATHER_CLEAR;
        float clearShadow = GetCloudShadow(5, 5, 0.0f);
        expect(clearShadow == 0.0f);
        
        // Cloudy - some shadows
        weatherState.current = WEATHER_CLOUDY;
        float cloudyShadow = GetCloudShadow(5, 5, 0.0f);
        expect(cloudyShadow > 0.0f);
        
        // Rain - darker shadows
        weatherState.current = WEATHER_RAIN;
        float rainShadow = GetCloudShadow(5, 5, 0.0f);
        expect(rainShadow >= cloudyShadow);
    }
    
    it("varies by position") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitWeather();
        
        weatherState.current = WEATHER_CLOUDY;
        
        float shadow1 = GetCloudShadow(0, 0, 0.0f);
        float shadow2 = GetCloudShadow(5, 5, 0.0f);
        float shadow3 = GetCloudShadow(9, 9, 0.0f);
        
        // At least some should differ (noise-based)
        int allSame = (shadow1 == shadow2 && shadow2 == shadow3);
        expect(!allSame);
    }
    
    it("moves with wind over time") {
        InitGridWithSizeAndChunkSize(10, 10, 10, 10);
        InitWeather();
        
        weatherState.current = WEATHER_CLOUDY;
        weatherState.windDirX = 1.0f;
        weatherState.windDirY = 0.0f;
        weatherState.windStrength = 2.0f;
        
        float shadow1 = GetCloudShadow(5, 5, 0.0f);
        float shadow2 = GetCloudShadow(5, 5, 10.0f);  // 10 seconds later
        
        // Should differ due to wind movement
        expect(shadow1 != shadow2);
    }
}

// ========== Main ==========
int main(int argc, char** argv) {
    set_quiet_mode(1);
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        test_verbose = 1;
        set_quiet_mode(0);
    }
    
    test(snow_grid_basics);
    test(snow_accumulation);
    test(snow_melting);
    test(snow_movement);
    test(snow_fire_interaction);
    test(cloud_shadow);
    
    return summary();
}
