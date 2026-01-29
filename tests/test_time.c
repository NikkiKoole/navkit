#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/entities/mover.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// Core Time System Tests
// =============================================================================

describe(time_initialization) {
    it("should initialize with default values") {
        InitTime();
        
        expect(gameSpeed == 1.0f);
        expect(gameTime == 0.0);
        expect(gameDeltaTime == 0.0f);
        expect(timeOfDay == 6.0f);
        expect(dayNumber == 1);
        expect(dayLength == 60.0f);
    }
    
    it("should reset to initial values") {
        gameSpeed = 10.0f;
        gameTime = 1000.0;
        timeOfDay = 12.0f;
        dayNumber = 5;
        
        ResetTime();
        
        expect(gameSpeed == 1.0f);
        expect(gameTime == 0.0);
        expect(timeOfDay == 6.0f);
        expect(dayNumber == 1);
    }
}

describe(time_accumulation) {
    it("should accumulate game time at 1x speed") {
        InitTime();
        gameSpeed = 1.0f;
        
        // Run 60 ticks = 1 real-second at 60Hz
        for (int i = 0; i < 60; i++) {
            UpdateTime(TICK_DT);
        }
        
        // Should have accumulated ~1 game-second
        expect(gameTime >= 0.99 && gameTime <= 1.01);
    }
    
    it("should accumulate game time faster at 10x speed") {
        InitTime();
        gameSpeed = 10.0f;
        
        // Run 60 ticks = 1 real-second
        for (int i = 0; i < 60; i++) {
            UpdateTime(TICK_DT);
        }
        
        // Should have accumulated ~10 game-seconds
        expect(gameTime >= 9.9 && gameTime <= 10.1);
    }
    
    it("should not accumulate time when paused") {
        InitTime();
        gameTime = 100.0;
        gameSpeed = 0.0f;
        
        for (int i = 0; i < 1000; i++) {
            UpdateTime(TICK_DT);
        }
        
        expect(gameTime == 100.0);
    }
    
    it("should return false when paused") {
        InitTime();
        gameSpeed = 0.0f;
        
        bool result = UpdateTime(TICK_DT);
        
        expect(result == false);
    }
    
    it("should return true when not paused") {
        InitTime();
        gameSpeed = 1.0f;
        
        bool result = UpdateTime(TICK_DT);
        
        expect(result == true);
    }
}

describe(time_day_cycle) {
    it("should advance timeOfDay based on dayLength") {
        InitTime();
        dayLength = 60.0f;  // 60 game-seconds = 1 day
        timeOfDay = 0.0f;
        gameSpeed = 1.0f;
        
        // Run 30 game-seconds (half a day)
        for (int i = 0; i < 30 * 60; i++) {  // 30 seconds * 60 ticks/sec
            UpdateTime(TICK_DT);
        }
        
        // Should be around noon (12.0)
        expect(timeOfDay >= 11.5f && timeOfDay <= 12.5f);
    }
    
    it("should increment dayNumber when day completes") {
        InitTime();
        dayLength = 60.0f;
        timeOfDay = 23.5f;  // Near end of day
        dayNumber = 1;
        gameSpeed = 1.0f;
        
        // Run 2 game-seconds (should wrap past midnight)
        for (int i = 0; i < 2 * 60; i++) {
            UpdateTime(TICK_DT);
        }
        
        expect(dayNumber == 2);
        expect(timeOfDay >= 0.0f && timeOfDay < 1.0f);
    }
    
    it("should track multiple days correctly") {
        InitTime();
        dayLength = 60.0f;
        timeOfDay = 0.0f;
        dayNumber = 1;
        gameSpeed = 10.0f;
        
        // Run 600 game-seconds = 10 days (at 10x speed, this is 60 real-seconds = 3600 ticks)
        for (int i = 0; i < 3600; i++) {
            UpdateTime(TICK_DT);
        }
        
        expect(dayNumber == 11);
    }
}

describe(time_RunGameSeconds) {
    it("should advance exactly the requested game time") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitTime();
        gameSpeed = 1.0f;
        double startTime = gameTime;
        
        RunGameSeconds(5.0f);
        
        double elapsed = gameTime - startTime;
        expect(elapsed >= 4.99 && elapsed <= 5.01);
    }
    
    it("should work at high game speeds") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitTime();
        gameSpeed = 100.0f;
        double startTime = gameTime;
        
        RunGameSeconds(100.0f);
        
        double elapsed = gameTime - startTime;
        expect(elapsed >= 99.0 && elapsed <= 101.0);
    }
}

describe(time_ResetTestState) {
    it("should seed random number generator") {
        ResetTestState(12345);
        int r1 = rand();
        
        ResetTestState(12345);
        int r2 = rand();
        
        expect(r1 == r2);  // Same seed = same result
    }
    
    it("should reset time state") {
        gameSpeed = 50.0f;
        gameTime = 9999.0;
        dayNumber = 100;
        
        ResetTestState(12345);
        
        expect(gameSpeed == 1.0f);
        expect(gameTime == 0.0);
        expect(dayNumber == 1);
    }
}

// =============================================================================
// Integration: Time + Tick
// =============================================================================

describe(time_tick_integration) {
    it("should update game time through Tick()") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitTime();
        gameSpeed = 1.0f;
        
        // Run 60 ticks through the full Tick() function
        for (int i = 0; i < 60; i++) {
            Tick();
        }
        
        // Should have ~1 game-second
        expect(gameTime >= 0.99 && gameTime <= 1.01);
    }
    
    it("should skip simulation when paused") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitTime();
        gameSpeed = 0.0f;
        unsigned long tickBefore = currentTick;
        
        // Try to run ticks while paused
        for (int i = 0; i < 100; i++) {
            Tick();
        }
        
        // currentTick should not advance (simulation skipped)
        // Note: currentTick is incremented at end of Tick(), after UpdateTime check
        expect(currentTick == tickBefore);
    }
}

// =============================================================================
// Large Time Scales
// =============================================================================

describe(time_large_scales) {
    it("should handle 10 days of game time") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitTime();
        dayLength = 3600.0f;  // 1 hour = 1 day
        gameSpeed = 600.0f;   // 600 game-sec per real-sec
        double startTime = gameTime;
        
        RunGameSeconds(36000.0f);  // 10 days
        
        double elapsed = gameTime - startTime;
        expect(elapsed >= 35999.0 && elapsed <= 36001.0);
    }
    
    it("should not lose precision at high game time values") {
        InitTime();
        gameTime = 1000000.0;  // Already ~277 days in (at dayLength=3600)
        double before = gameTime;
        gameSpeed = 1.0f;
        
        UpdateTime(TICK_DT);
        
        expect(gameTime > before);
        expect(gameTime - before > 0.01);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    bool legacyMode = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (strcmp(argv[i], "--legacy") == 0) legacyMode = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }
    
    // Standard (DF-style) walkability is the default
    g_legacyWalkability = legacyMode;
    
    // Core time system
    test(time_initialization);
    test(time_accumulation);
    test(time_day_cycle);
    test(time_RunGameSeconds);
    test(time_ResetTestState);
    
    // Integration
    test(time_tick_integration);
    
    // Large scales
    test(time_large_scales);
    
    return 0;
}
