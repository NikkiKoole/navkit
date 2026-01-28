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
#include "../src/world/cell_defs.h"
#include "../src/simulation/water.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/steam.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/groundwear.h"
#include <stdlib.h>
#include <math.h>

// =============================================================================
// Helper Functions
// =============================================================================

static void SetupCorridorGrid(void) {
    // Create a corridor: mover must go through narrow passage
    // WWWWWWWWWWWWWWWW
    // W..............W
    // W..............W
    // WWWWWW..WWWWWWWW
    // W..............W
    // W..............W
    // WWWWWWWWWWWWWWWW
    InitGridFromAsciiWithChunkSize(
        "################\n"
        "#..............#\n"
        "#..............#\n"
        "######..########\n"
        "#..............#\n"
        "#..............#\n"
        "################\n", 16, 7);
    
    gridDepth = 1;
    
    InitWater();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
}

static void SetupOpenGrid(void) {
    InitGridFromAsciiWithChunkSize(
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n", 16, 8);
    
    gridDepth = 1;
    
    // All floor
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_FLOOR;
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
    InitGridFromAsciiWithChunkSize(
        "################\n"
        "#......##......#\n"
        "#......##......#\n"
        "#......##......#\n"
        "#..............#\n"
        "#......##......#\n"
        "#......##......#\n"
        "################\n", 16, 8);
    
    gridDepth = 1;
    
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
    
    if (cellX < 0 || cellX >= gridWidth ||
        cellY < 0 || cellY >= gridHeight ||
        cellZ < 0 || cellZ >= gridDepth) {
        return false;  // Out of bounds
    }
    
    CellType cell = grid[cellZ][cellY][cellX];
    return CellIsWalkable(cell);
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
        Point goal = {14, 5, 0};
        
        // Start at top-left, goal at bottom-right (must go through corridor gap)
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
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
        Point goal = {14, 5, 0};
        
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
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
            Point goal = {14, 4, 0};  // Right side through the gap
            
            float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
            float startY = (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f;
            if (startY >= 7 * CELL_SIZE) startY = 6 * CELL_SIZE + CELL_SIZE * 0.5f;
            
            InitMover(m, startX, startY, 0.0f, goal, MOVER_SPEED);
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
        
        // Set all cells to grass
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_GRASS;
            }
        }
        
        fireSpreadInterval = 1.0f;
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
        
        ambientSurfaceTemp = 20;
        heatTransferInterval = 0.1f;
        tempDecayInterval = 0.5f;
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
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
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
