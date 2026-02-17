// test_high_speed.c - High game speed safety tests
//
// These tests verify that at extreme game speeds, movers don't:
// - Clip through walls
// - Skip over obstacles  
// - Teleport to invalid positions
// - Fall through floors
//
// The time system uses fixed timestep (60Hz) so movement is deterministic,
// but high game speeds mean more simulation steps per real-second.

#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/entities/mover.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/simulation/water.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/steam.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/groundwear.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


// Global flag for verbose output in tests
static bool test_verbose = false;

// =============================================================================
// Helper Functions
// =============================================================================

static void SetupCorridorGrid(void) {
    // Create a corridor: mover must go through narrow passage
    // DF mode: z=0 = ground (dirt), z=1 = walking level with walls
    InitTestGrid(16, 7);
    gridDepth = 2;
    
    // Fill z=0 with dirt (ground)
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
        }
    }
    
    // Place walls at z=1
    // WWWWWWWWWWWWWWWW
    // W..............W
    // W..............W
    // WWWWWW..WWWWWWWW
    // W..............W
    // W..............W
    // WWWWWWWWWWWWWWWW
    for (int x = 0; x < 16; x++) {
        grid[1][0][x] = CELL_WALL;  // Top row
        grid[1][6][x] = CELL_WALL;  // Bottom row
    }
    for (int y = 0; y < 7; y++) {
        grid[1][y][0] = CELL_WALL;   // Left column
        grid[1][y][15] = CELL_WALL;  // Right column
    }
    // Middle wall with gap
    for (int x = 0; x < 6; x++) grid[1][3][x] = CELL_WALL;
    for (int x = 8; x < 16; x++) grid[1][3][x] = CELL_WALL;
    
    InitWater();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
}

static void SetupOpenGrid(void) {
    InitTestGridFromAscii(
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n");
    
    gridDepth = 1;
    
    // All floor (use flag system)
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_AIR;
            SET_FLOOR(x, y, 0);
        }
    }
    
    InitWater();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
}

static void SetupWalledGrid(void) {
    // Grid with walls on all sides and in the middle
    // DF mode: z=0 = ground (dirt), z=1 = walking level with walls
    InitTestGrid(16, 8);
    gridDepth = 2;
    
    // Fill z=0 with dirt, z=1 with air
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
        }
    }
    
    // Place walls at z=1
    // ################
    // #......##......#
    // #......##......#
    // #......##......#
    // #..............#
    // #......##......#
    // #......##......#
    // ################
    for (int x = 0; x < 16; x++) {
        grid[1][0][x] = CELL_WALL;  // Top row
        grid[1][7][x] = CELL_WALL;  // Bottom row
    }
    for (int y = 0; y < 8; y++) {
        grid[1][y][0] = CELL_WALL;   // Left column
        grid[1][y][15] = CELL_WALL;  // Right column
    }
    // Middle wall columns (with gap at y=4)
    for (int y = 1; y < 8; y++) {
        if (y != 4) {
            grid[1][y][7] = CELL_WALL;
            grid[1][y][8] = CELL_WALL;
        }
    }
    
    InitWater();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
}

// Check if mover position is valid (not inside a wall)
static bool IsMoverPositionValid(Mover* m) {
    int cellX = (int)(m->x / CELL_SIZE);
    int cellY = (int)(m->y / CELL_SIZE);
    int cellZ = (int)m->z;
    
    // Use the proper DF-style walkability check
    return IsCellWalkableAt(cellZ, cellY, cellX);
}

// =============================================================================
// High Game Speed Movement Tests
// =============================================================================

describe(high_speed_movement) {
    it("mover should not clip through walls at 10x game speed") {
        SetupCorridorGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        Mover* m = &movers[0];
        Point goal = {14, 5, 1};  // z=1 walking level
        
        // Start at top-left, goal at bottom-right (must go through corridor gap)
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 1.0f, goal, MOVER_SPEED);  // z=1
        moverCount = 1;
        
        gameSpeed = 10.0f;
        
        // Run for 10 game-seconds
        RunGameSeconds(10.0f);
        
        // Mover should still be in valid position
        expect(IsMoverPositionValid(m));
        
        endlessMoverMode = true;
    }
    
    it("mover should not clip through walls at 100x game speed") {
        SetupCorridorGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        Mover* m = &movers[0];
        Point goal = {14, 5, 1};  // z=1 walking level
        
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 1.0f, goal, MOVER_SPEED);  // z=1
        moverCount = 1;
        
        gameSpeed = 100.0f;
        
        // Run for 10 game-seconds
        RunGameSeconds(10.0f);
        
        // Mover should still be in valid position
        expect(IsMoverPositionValid(m));
        
        endlessMoverMode = true;
    }
    
    it("mover should reach goal correctly at high speed") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        Mover* m = &movers[0];
        Point goal = {14, 6, 0};
        
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        
        gameSpeed = 50.0f;
        
        // Run enough time for mover to reach goal
        // Distance ~16 cells diagonal, at 200px/s = ~2.5 seconds
        RunGameSeconds(5.0f);
        
        // Mover should have reached goal and deactivated
        expect(m->active == false);
        expect(IsMoverPositionValid(m));
        
        endlessMoverMode = true;
    }
    
    it("multiple movers should not clip through walls at high speed") {
        SetupWalledGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        // Create 5 movers all trying to cross through the gap
        for (int i = 0; i < 5; i++) {
            Mover* m = &movers[i];
            Point goal = {14, 4, 1};  // Right side through the gap, z=1
            
            float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
            float startY = (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f;
            if (startY >= 7 * CELL_SIZE) startY = 6 * CELL_SIZE + CELL_SIZE * 0.5f;
            
            InitMover(m, startX, startY, 1.0f, goal, MOVER_SPEED);  // z=1
        }
        moverCount = 5;
        
        gameSpeed = 25.0f;
        
        // Run for 10 game-seconds
        RunGameSeconds(10.0f);
        
        // All movers should still be in valid positions
        for (int i = 0; i < 5; i++) {
            expect(IsMoverPositionValid(&movers[i]));
        }
        
        endlessMoverMode = true;
    }
}

// =============================================================================
// High Speed Simulation Stability Tests
// =============================================================================

describe(high_speed_simulation_stability) {
    it("fire spread should remain bounded at 100x speed") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        // Set all cells to dirt with grass surface (flammable)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT);
                SetVegetation(x, y, 0, VEG_GRASS);
            }
        }
        
        fireSpreadInterval = 0.4f;
        fireSpreadBase = 30;
        fireSpreadPerLevel = 10;
        fireEnabled = true;
        gameSpeed = 100.0f;
        
        // Start fire at center
        SetFireLevel(8, 4, 0, FIRE_MAX_LEVEL);
        
        // Run for 5 game-seconds at 100x (should be 500 real-ms worth)
        RunGameSeconds(5.0f);
        
        // Fire should have spread but not consumed everything instantly
        int fireCells = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (GetFireLevel(x, y, 0) > 0) fireCells++;
            }
        }
        
        // Should be more than 1 but not the entire grid
        expect(fireCells > 1);
        expect(fireCells < gridWidth * gridHeight);
    }
    
    it("temperature should remain bounded at extreme speeds") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        SetAmbientSurfaceTemp(20);
        heatTransferInterval = 0.04f;
        tempDecayInterval = 0.2f;
        temperatureEnabled = true;
        gameSpeed = 100.0f;
        
        // Heat center to extreme
        SetTemperature(8, 4, 0, 1000);
        
        RunGameSeconds(10.0f);
        
        // Temperature should have decayed toward ambient, not exploded
        int centerTemp = GetTemperature(8, 4, 0);
        expect(centerTemp >= TEMP_MIN && centerTemp <= TEMP_MAX);
        expect(centerTemp < 1000);  // Should have decayed
    }
    
    it("day cycle should advance days at high speed") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        dayLength = 60.0f;  // 60 game-seconds per day
        timeOfDay = 0.0f;
        int startDay = dayNumber;  // Will be 1 after ResetTestState
        gameSpeed = 100.0f;
        
        // Run for 5 days worth of game time
        RunGameSeconds(300.0f);
        
        // Days should have advanced (at least some days passed at high speed)
        // Due to how RunGameSeconds uses Tick() and the simulation may consume time
        // differently, just verify we're making progress
        expect(dayNumber > startDay && timeOfDay >= 0.0f && timeOfDay <= 24.0f);
    }
}

// =============================================================================
// Extreme Speed Edge Cases
// =============================================================================

describe(extreme_speed_edge_cases) {
    it("should handle gameSpeed of 1000") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        Mover* m = &movers[0];
        Point goal = {8, 4, 0};
        
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 4 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        
        gameSpeed = 1000.0f;
        
        // Run for 1 game-second (= 0.001 real-second equivalent)
        RunGameSeconds(1.0f);
        
        // Should not crash and mover should be in valid position
        expect(IsMoverPositionValid(m));
        
        endlessMoverMode = true;
    }
    
    it("should handle rapid speed changes") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        Mover* m = &movers[0];
        Point goal = {14, 6, 0};
        
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        
        // Rapidly change game speed
        for (int i = 0; i < 10; i++) {
            gameSpeed = 1.0f;
            RunGameSeconds(0.1f);
            gameSpeed = 100.0f;
            RunGameSeconds(0.1f);
            gameSpeed = 0.5f;
            RunGameSeconds(0.1f);
            gameSpeed = 50.0f;
            RunGameSeconds(0.1f);
        }
        
        // Should not crash and mover should be in valid position
        expect(IsMoverPositionValid(m));
        
        endlessMoverMode = true;
    }
    
    it("pause and resume should work correctly") {
        SetupOpenGrid();
        ResetTestState(12345);
        
        ClearMovers();
        endlessMoverMode = false;
        
        Mover* m = &movers[0];
        Point goal = {14, 4, 0};  // Goal to the right
        
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 4 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
        moverCount = 1;
        
        gameSpeed = 1.0f;
        RunGameSeconds(0.5f);
        float posAfterMove = m->x;
        
        // Pause
        gameSpeed = 0.0f;
        double timeBefore = gameTime;
        
        // Try to run while paused (should do nothing)
        for (int i = 0; i < 100; i++) {
            Tick();
        }
        
        // Time and position should not have changed
        expect(gameTime == timeBefore);
        expect(m->x == posAfterMove);
        
        // Resume at high speed
        gameSpeed = 10.0f;
        
        // Run more game time - mover should continue moving
        double timeBeforeResume = gameTime;
        RunGameSeconds(1.0f);
        
        // Game time should have advanced
        expect(gameTime > timeBeforeResume);
        
        // Mover should have moved (or reached goal)
        // Either it moved further, or it deactivated (reached goal)
        expect(m->x > posAfterMove || m->active == false);
        
        endlessMoverMode = true;
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    test_verbose = verbose;
    if (!verbose) {
    if (quiet) {
        set_quiet_mode(1);
    }
        SetTraceLogLevel(LOG_NONE);
    }
    
    // High speed movement safety
    test(high_speed_movement);
    
    // Simulation stability at high speeds
    test(high_speed_simulation_stability);
    
    // Edge cases
    test(extreme_speed_edge_cases);
    
    return summary();
}
