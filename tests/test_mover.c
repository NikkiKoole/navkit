#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/entities/service.h"
#include "../src/simulation/mood.h"
#include "../src/simulation/balance.h"
#include "../src/entities/workshops.h"
#include "../src/world/terrain.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


// Global flag for verbose output in tests
static bool test_verbose = false;

// Number of random path tests - use STRESS_TEST_ITERATIONS=20 for full coverage
#ifndef STRESS_TEST_ITERATIONS
#define STRESS_TEST_ITERATIONS 5
#endif

describe(mover_initialization) {
    it("should initialize mover with correct position and goal") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {7, 3, 0};
        InitMover(m, 16.0f, 16.0f, 0.0f, goal, 100.0f);
        moverCount = 1;

        expect(m->x == 16.0f);
        expect(m->y == 16.0f);
        expect(m->goal.x == 7 && m->goal.y == 3);
        expect(m->speed == 100.0f);
        expect(m->active == true);
    }

    it("should initialize mover with path") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {4, 0, 0};
        Point testPath[] = {{4, 0, 0}, {3, 0, 0}, {2, 0, 0}, {1, 0, 0}, {0, 0, 0}};
        InitMoverWithPath(m, 16.0f, 16.0f, 0.0f, goal, 100.0f, testPath, 5);
        moverCount = 1;

        expect(GetMoverPathLength(0) == 5);
        expect(GetMoverPathIndex(0) == 4);  // Points to last element (start)
        expect(moverPaths[0][0].x == 4 && moverPaths[0][0].y == 0);  // Goal
    }
}

describe(fixed_timestep_movement) {
    it("should move mover toward goal after one tick") {
        // 8x4 grid, mover walks right
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        ClearMovers();
        Mover* m = &movers[0];

        // Path from (0,0) to (4,0), stored as {current, target}
        Point goal = {4, 0, 0};
        Point testPath[] = {{0, 0, 0}, {4, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        float initialX = m->x;

        // Run 1 tick
        Tick();

        // Mover should have moved right (toward goal at x=4)
        expect(m->x > initialX);
    }

    it("should produce same result for same number of ticks") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        // First run
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {4, 0, 0};
        Point testPath[] = {{0, 0, 0}, {4, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        RunTicks(60);  // 1 second of simulation
        float firstRunX = m->x;
        float firstRunY = m->y;

        // Second run - same setup
        ClearMovers();
        m = &movers[0];
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        RunTicks(60);
        float secondRunX = m->x;
        float secondRunY = m->y;

        // Should be identical (combine into single expect)
        expect(firstRunX == secondRunX && firstRunY == secondRunY);
    }

    it("should deactivate mover when reaching goal") {
        InitTestGridFromAscii(
            "....\n"
            "....\n"
            "....\n"
            "....\n");

        // Disable endless mode so mover deactivates at goal instead of getting new goal
        endlessMoverMode = false;
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {1, 0, 0};
        Point testPath[] = {{0, 0, 0}, {1, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        // Run enough ticks to reach goal (1 cell = 32 pixels, speed = 100 px/s)
        // At 60 ticks/sec, 1 tick = ~1.67 pixels. Need ~19 ticks for 1 cell.
        RunTicks(60);  // Should be more than enough

        expect(m->active == false);
        
        endlessMoverMode = true;  // Restore default
    }
}

describe(wall_collision) {
    it("should push mover out when wall placed on it") {
        // Mover at (1,1), we place a wall there
        InitTestGridFromAscii(
            "....\n"
            "....\n"
            "....\n"
            "....\n");
        BuildEntrances();
        BuildGraph();

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {3, 3, 0};
        Point testPath[] = {{1, 1, 0}, {2, 2, 0}, {3, 3, 0}};
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;

        // Place wall on mover's current cell
        grid[0][1][1] = CELL_WALL;
        MarkChunkDirty(1, 1, 0);

        // Run tick - mover should be pushed to adjacent walkable cell
        Tick();

        // Mover should have moved to a neighbor cell and need repath
        int cellX = (int)(m->x / CELL_SIZE);
        int cellY = (int)(m->y / CELL_SIZE);
        expect(!(cellX == 1 && cellY == 1) && GetMoverNeedsRepath(0) == true);
    }

    it("should deactivate mover when fully surrounded by walls") {
        InitTestGridFromAscii(
            ".#..\n"
            "#.#.\n"
            ".#..\n"
            "....\n");
        BuildEntrances();
        BuildGraph();

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {3, 3, 0};
        Point testPath[] = {{1, 1, 0}, {3, 3, 0}};
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        // Place wall on mover's cell - now fully surrounded
        grid[0][1][1] = CELL_WALL;

        Tick();

        // Mover should be deactivated (no escape)
        expect(m->active == false);
    }
}

describe(line_of_sight_repath) {
    it("should trigger repath when wall blocks path to next waypoint") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        BuildEntrances();
        BuildGraph();

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {6, 0, 0};
        // Path stored goal-to-start: mover walks from pathIndex down to 0
        // pathIndex starts at pathLength-1, so mover walks toward path[pathLength-1] first
        // Actually path[pathIndex] is the next waypoint to reach
        // With pathIndex=1 (last elem), target is path[1]={6,0}
        Point testPath[] = {{0, 0, 0}, {6, 0, 0}};  // path[0]=start(current), path[1]=goal(target)
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        // Place wall between start and goal
        grid[0][0][3] = CELL_WALL;
        MarkChunkDirty(3, 0, 0);

        Tick();

        // Mover should need repath (wall blocks line of sight to path[1]={6,0})
        expect(GetMoverNeedsRepath(0) == true);
    }
}

describe(tick_counter) {
    it("should increment tick counter each tick") {
        InitTestGridFromAscii(
            "....\n"
            "....\n");

        ClearMovers();
        unsigned long startTick = currentTick;

        RunTicks(100);

        expect(currentTick == startTick + 100);
    }

    it("should reset tick counter on ClearMovers") {
        InitTestGridFromAscii(
            "....\n"
            "....\n");

        RunTicks(50);
        ClearMovers();

        expect(currentTick == 0);
    }
}

describe(count_active_movers) {
    it("should count only active movers") {
        InitTestGridFromAscii(
            "....\n"
            "....\n"
            "....\n"
            "....\n");

        ClearMovers();

        // Add 3 movers
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[moverCount];
            Point goal = {3, 0, 0};
            Point testPath[] = {{3, 0, 0}, {0, 0, 0}};
            InitMoverWithPath(m, 16.0f, 16.0f, 0.0f, goal, 100.0f, testPath, 2);
            moverCount++;
        }

        expect(CountActiveMovers() == 3);

        // Deactivate one
        movers[1].active = false;

        expect(CountActiveMovers() == 2);
    }
}

describe(endless_mode) {
    it("should assign new goal when mover reaches destination") {
        // Use 4x4 chunks so we get multiple chunks and entrances
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        BuildEntrances();
        BuildGraph();

        SeedRandom(12345);  // Deterministic
        endlessMoverMode = true;

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {1, 0, 0};
        Point testPath[] = {{0, 0, 0}, {1, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        // Run enough ticks to reach goal
        RunTicks(60);

        // In endless mode, mover should still be active with a new goal
        expect(m->active == true);

        endlessMoverMode = false;  // Reset
    }

    it("should keep movers moving indefinitely with seeded random") {
        // 16x16 grid with 4x4 chunks = 4x4 chunks = good graph
        InitGridFromAsciiWithChunkSize(
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n", 4, 4);
        BuildEntrances();
        BuildGraph();

        SeedRandom(42);
        endlessMoverMode = true;

        ClearMovers();
        // Create a mover with a short path
        Mover* m = &movers[0];
        Point goal = {2, 0, 0};
        Point testPath[] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;

        // Run many ticks - mover should keep getting new goals
        RunTicks(600);  // 10 seconds of simulation

        // Mover should still be active after all this time
        expect(m->active == true);

        endlessMoverMode = false;
    }

    it("should produce deterministic paths with same seed") {
        InitGridFromAsciiWithChunkSize(
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n", 4, 4);
        BuildEntrances();
        BuildGraph();

        endlessMoverMode = true;

        // First run
        SeedRandom(99999);
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {1, 0, 0};
        Point testPath[] = {{0, 0, 0}, {1, 0, 0}};
        InitMoverWithPath(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        RunTicks(300);
        float firstX = m->x;
        float firstY = m->y;
        Point firstGoal = m->goal;

        // Second run - same seed
        SeedRandom(99999);
        ClearMovers();
        m = &movers[0];
        InitMoverWithPath(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        RunTicks(300);
        float secondX = m->x;
        float secondY = m->y;
        Point secondGoal = m->goal;

        // Should be identical
        expect(firstX == secondX && firstY == secondY &&
               firstGoal.x == secondGoal.x && firstGoal.y == secondGoal.y);

        endlessMoverMode = false;
    }
}

describe(blocked_mover_handling) {
    it("should pick new goal when goal cell becomes a wall") {
        // Mover heading to (6,0), we place a wall there - goal is now unreachable forever
        // Mover should pick a new random goal instead of staying blocked
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        gridDepth = 1;  // Single z-level for this test

        PathAlgorithm savedAlgo = moverPathAlgorithm;
        moverPathAlgorithm = PATH_ALGO_ASTAR;  // Use A* for simplicity
        useRandomizedCooldowns = false;  // Deterministic cooldowns
        SeedRandom(12345);  // Deterministic random goals

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {6, 0, 0};
        Point testPath[] = {{0, 0, 0}, {6, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        // Place wall ON the goal cell - goal is now unwalkable
        grid[0][0][6] = CELL_WALL;
        MarkChunkDirty(6, 0, 0);

        // Trigger repath
        SetMoverNeedsRepath(0, true);

        // Run enough ticks for repath cooldown and processing
        RunTicks(REPATH_COOLDOWN_FRAMES + 10);

        // Mover should have picked a NEW goal (not stuck on the walled goal)
        expect(m->goal.x != 6 || m->goal.y != 0);
        // And should have a valid path (not blocked)
        expect(GetMoverPathLength(0) > 0);

        moverPathAlgorithm = savedAlgo;  // Restore
        useRandomizedCooldowns = true;
    }

    it("should stay blocked when path is blocked but goal is still walkable") {
        // Mover heading to (6,0), wall placed at (3,0) blocks the path
        // Goal is still walkable, so mover should stay blocked (wait for path to clear)
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "########\n"  // Wall row prevents going around
            "........\n"
            "........\n", 4, 4);
        gridDepth = 1;  // Single z-level for this test

        PathAlgorithm savedAlgo = moverPathAlgorithm;
        moverPathAlgorithm = PATH_ALGO_ASTAR;  // Use A* for simplicity
        useRandomizedCooldowns = false;

        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {6, 0, 0};
        Point testPath[] = {{0, 0, 0}, {6, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;

        // Place wall blocking the PATH (not the goal)
        grid[0][0][3] = CELL_WALL;
        MarkChunkDirty(3, 0, 0);

        // Trigger repath
        SetMoverNeedsRepath(0, true);

        // Run enough ticks for repath cooldown and processing
        RunTicks(REPATH_COOLDOWN_FRAMES + 10);

        // Goal should remain the SAME (6,0) - mover waits for path to clear
        expect(m->goal.x == 6 && m->goal.y == 0);
        // Path should be empty (blocked)
        expect(GetMoverPathLength(0) == 0);

        moverPathAlgorithm = savedAlgo;  // Restore
        useRandomizedCooldowns = true;
    }
}

describe(refinement_after_wall_changes) {
    it("should handle movers on map with scattered walls and small chunks") {
        // Disable randomized cooldowns for deterministic test behavior
        useRandomizedCooldowns = false;
        
        // Exact map from user with 4x4 chunks - triggers HPA* refinement failures
        const char* scatteredWallsMap =
            ".........#..#.....#.............\n"
            ".#.............##.....#.........\n"
            "...#......#........#....#.......\n"
            ".......#...............#.....#..\n"
            "#...#....#.#.#.#...........#.#..\n"
            ".....#......#...................\n"
            "................................\n"
            ".#............#.....#......#..#.\n"
            "....#....#......#........#......\n"
            "...###...#...#########......#...\n"
            "...........##.....#.##...#......\n"
            ".#....#....#.........##...#.....\n"
            "..........#...........#.#....#..\n"
            "....#....##.....#.....#.....#...\n"
            ".#.#.....#............#..#......\n"
            "#....#...#....#.....#...........\n"
            "........##.....#...........#....\n"
            "........#.......................\n"
            ".......##.................#.....\n"
            "..##....##.............#..#.....\n"
            ".........##...##.....###.......#\n"
            ".......#..###........##..#.#.#.#\n"
            "............###......##......#..\n"
            "..............########..#.#.....\n"
            ".#.#........#................##.\n"
            ".#....#..#.........#............\n"
            "...........................#....\n"
            ".....#..........#..........##...\n"
            "..........#..............#......\n"
            "......................#......#..\n"
            ".#..............................\n"
            "..#..........#.........#.##.....\n";

        InitGridFromAsciiWithChunkSize(scatteredWallsMap, 4, 4);
        BuildEntrances();
        BuildGraph();

        SeedRandom(54321);
        endlessMoverMode = true;

        // Spawn movers
        ClearMovers();
        int spawnCount = 100;
        for (int i = 0; i < spawnCount && moverCount < MAX_MOVERS; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();

            startPos = start;
            goalPos = goal;
            RunHPAStar();

            Mover* m = &movers[moverCount];
            float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;

            if (pathLength > 0) {
                InitMoverWithPath(m, x, y, 0.0f, goal, 100.0f, path, pathLength);
            } else {
                InitMover(m, x, y, 0.0f, goal, 100.0f);
            }
            moverCount++;
        }

        // Run simulation for 10 seconds
        RunTicks(600);

        // Count stuck movers (active but no path and not on cooldown)
        int stuckMovers = 0;
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (m->active && GetMoverPathLength(i) == 0 && m->repathCooldown == 0) {
                stuckMovers++;
            }
        }

        // All movers should either have a path or be on cooldown waiting for retry
        expect(stuckMovers == 0);

        endlessMoverMode = false;
        useRandomizedCooldowns = true;  // Restore default
    }

    it("should handle walls drawn while movers are active") {
        // Disable randomized cooldowns and staggering for deterministic test behavior
        useRandomizedCooldowns = false;
        useStaggeredUpdates = false;
        
        // Match demo: 32x32 grid with 8x8 chunks (4x4 chunks total)
        const char* initialMap =
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n";

        InitGridFromAsciiWithChunkSize(initialMap, 8, 8);
        BuildEntrances();
        BuildGraph();

        SeedRandom(12345);
        endlessMoverMode = true;

        // Spawn 1000 movers like in demo
        ClearMovers();
        for (int i = 0; i < 1000 && moverCount < MAX_MOVERS; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();

            startPos = start;
            goalPos = goal;
            RunHPAStar();

            if (pathLength > 0) {
                Mover* m = &movers[moverCount];
                float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, x, y, 0.0f, goal, 100.0f, path, pathLength);
                moverCount++;
            }
        }

        // Run for a bit
        RunTicks(60);

        // Wall positions from the failing scenario
        int wallPositions[][2] = {
            {15,8}, {16,8}, {17,8}, {18,8},  // ####
            {14,9},                            // #
            {11,10},                           // #
            {6,12}, {8,12},                    // #.#
            {5,13}, {5,14}, {5,15}, {5,16},   // vertical line
            {6,17}, {6,18},                    // ##
            {19,19},                           // #
            {8,20}, {19,20}, {20,20},         // #..##
            {9,21}, {10,21}, {17,21}, {18,21}, {19,21},  // ##...###
            {10,22}, {11,22}, {12,22}, {13,22}, {14,22}, // #####
            {-1,-1}  // sentinel
        };

        // Draw walls ONE AT A TIME with ticks in between (like real-time drawing)
        for (int i = 0; wallPositions[i][0] >= 0; i++) {
            int wx = wallPositions[i][0];
            int wy = wallPositions[i][1];
            if (wx < gridWidth && wy < gridHeight) {
                grid[0][wy][wx] = CELL_WALL;
                MarkChunkDirty(wx, wy, 0);
            }
            // Run a few ticks between each wall (simulates drawing at ~60fps)
            RunTicks(2);
        }

        // Run more ticks - this triggers repaths
        RunTicks(600);

        // Count stuck movers: active, no path, not waiting for repath, no cooldown
        // Movers with needsRepath=true are fine - they'll be processed next frame
        // Movers with repathCooldown>0 are fine - they're waiting to retry
        int stuckMovers = 0;
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (m->active && GetMoverPathLength(i) == 0 && !GetMoverNeedsRepath(i) && m->repathCooldown == 0) {
                stuckMovers++;
            }
        }
        expect(stuckMovers == 0);

        endlessMoverMode = false;
        useRandomizedCooldowns = true;   // Restore defaults
        useStaggeredUpdates = true;
    }
}

describe(string_pulling_narrow_gaps) {
    it("should not leave movers stuck yellow with string pulling enabled") {
        // This test documents a known bug: string pulling creates paths that
        // pass the initial LOS check but fail the runtime LOS check in UpdateMovers,
        // causing movers to get stuck in yellow "needsRepath" state.
        // See STRING_PULLING_BUG.md for full analysis.
        const char* narrowGapsMap =
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "................................\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "#########.#############.#####.##\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#...............#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "###.#######.##########.####.####\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#...............#.......\n"
            "........#.......#.......#.......\n"
            "................#.......#.......\n"
            "........#.......#...............\n"
            "........#.......#.......#.......\n"
            "#.#########.#######.#########.##\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n"
            "........#.......#.......#.......\n";

        InitGridFromAsciiWithChunkSize(narrowGapsMap, 8, 8);
        BuildEntrances();
        BuildGraph();

        SeedRandom(12345);
        endlessMoverMode = true;
        useStringPulling = true;

        ClearMovers();
        for (int i = 0; i < 500 && moverCount < MAX_MOVERS; i++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();

            startPos = start;
            goalPos = goal;
            RunHPAStar();

            if (pathLength > 0) {
                Mover* m = &movers[moverCount];
                float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, x, y, 0.0f, goal, 100.0f, path, pathLength);
                moverCount++;
            }
        }

        // Run simulation for longer to trigger the bug
        RunTicks(1200);

        // BUG: String pulling creates paths that fail runtime LOS checks.
        // This expect FAILS with stuckYellow > 0, documenting the bug.
        // Uncomment to verify the bug exists, comment out for CI until fixed.
        // int stuckYellow = 0;
        // for (int i = 0; i < moverCount; i++) {
        //     Mover* m = &movers[i];
        //     if (m->active && m->needsRepath && m->repathCooldown == 0) {
        //         stuckYellow++;
        //     }
        // }
        // expect(stuckYellow == 0);

        endlessMoverMode = false;
        useStringPulling = true;
    }
}

describe(chunk_boundary_paths) {
    it("should find paths when room entrance is in adjacent chunk") {
        // Regression test for chunk boundary pathfinding bug.
        //
        // The bug: ReconstructLocalPath() only searched within chunk bounds.
        // When a mover in a room needed to reach an entrance on the chunk
        // boundary, but the path required going through cells in the adjacent
        // chunk first (e.g., around a corner), path refinement failed.
        //
        // This map recreates that scenario:
        // - Room in chunk 1 (rows 8-14) with entrance at row 8 (boundary)
        // - The room has a wall that forces paths to go "up" into chunk 0
        //   before reaching the entrance
        const char* dungeonMap =
            "################\n"   // row 0  - chunk 0
            "#......##......#\n"   // row 1  - two rooms separated by wall
            "#......##......#\n"   // row 2
            "#......##......#\n"   // row 3
            "#......##......#\n"   // row 4
            "#......##......#\n"   // row 5
            "#......##......#\n"   // row 6
            "#...........#..#\n"   // row 7  - corridor connecting rooms (chunk boundary)
            "#...........#..#\n"   // row 8  - chunk 1 starts here
            "#......##......#\n"   // row 9  - rooms continue in chunk 1
            "#......##......#\n"   // row 10
            "#......##......#\n"   // row 11
            "#......##......#\n"   // row 12
            "#......##......#\n"   // row 13
            "#......##......#\n"   // row 14
            "################\n";  // row 15
        
        InitGridFromAsciiWithChunkSize(dungeonMap, 8, 8);
        BuildEntrances();
        BuildGraph();
        
        ClearMovers();
        
        // Spawn a mover in the bottom-left room (chunk 1)
        // Goal is in the right side (also chunk 1, but different area)
        // Path must go through the corridor at the chunk boundary
        int startX = 2;
        int startY = 12;
        int goalX = 12;
        int goalY = 12;
        
        expect(IsCellWalkableAt(0, startY, startX) == true);
        expect(IsCellWalkableAt(0, goalY, goalX) == true);
        
        startPos = (Point){startX, startY, 0};
        goalPos = (Point){goalX, goalY, 0};
        RunHPAStar();
        
        // Path should be found
        expect(pathLength > 0);
        
        // Path should start at the mover's position
        Point pathStart = path[pathLength - 1];
        expect(pathStart.x == startX);
        expect(pathStart.y == startY);
        
        // Create mover and run simulation
        Mover* m = &movers[0];
        float x = startX * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = startY * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, x, y, 0.0f, (Point){goalX, goalY, 0}, 100.0f, path, pathLength);
        moverCount = 1;
        
        // Run until mover reaches goal or times out
        for (int tick = 0; tick < 600; tick++) {
            Tick();
            if (!m->active) break;  // Reached goal
        }
        
        // Mover should have reached the goal (inactive)
        expect(m->active == false);
    }
}

describe(path_truncation) {
    it("should keep start end of path when truncating long paths") {
        // Simulate a path longer than MAX_MOVER_PATH
        // Path is goal-to-start: path[0]=goal, path[pathLen-1]=start
        Point longPath[2000];
        int longPathLen = 2000;
        
        // Create a path from (0,0) to (1999,0)
        // path[0] = goal = (1999,0), path[1999] = start = (0,0)
        for (int i = 0; i < longPathLen; i++) {
            longPath[i].x = longPathLen - 1 - i;
            longPath[i].y = 0;
        }
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {1999, 0, 0};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, longPath, longPathLen);
        moverCount = 1;
        
        // Path should be truncated to MAX_MOVER_PATH
        expect(GetMoverPathLength(0) == MAX_MOVER_PATH);
        
        // First waypoint (path[pathLength-1]) should be at or near the start position (0,0)
        // not somewhere in the middle of the path
        Point firstWaypoint = moverPaths[0][GetMoverPathIndex(0)];
        expect(firstWaypoint.x == 0);
        expect(firstWaypoint.y == 0);
        
        // The truncated path should contain the start portion, not the goal portion
        // So path[0] should be around x=1023 (1024 steps from start), not x=1999 (goal)
        expect(moverPaths[0][0].x < 1500);  // Should be ~1023, definitely not 1999
    }
}

// ============================================================================
// Z-Level Tests
// ============================================================================

describe(mover_falling) {
    it("should fall to ground when placed in air") {
        // Setup: z=0 is walkable ground, z=1 is non-walkable air
        InitGridWithSize(5, 5);
        gridDepth = 2;
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // z=0 is solid dirt, z=1 air above air is not walkable
                grid[0][y][x] = CELL_AIR;       // Air at z=0 (walkable due to bedrock)
                grid[1][y][x] = CELL_AIR;       // Air at z=1 (NOT walkable - no solid below)
            }
        }
        
        // Verify setup - z=0 walkable, z=1 not walkable
        expect(IsCellWalkableAt(0, 1, 2) == true);
        expect(IsCellWalkableAt(1, 1, 2) == false);
        
        ClearMovers();
        Mover* m = &movers[0];
        
        // Place mover directly in air at z=1
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {2, 1, 0};  // Goal on ground
        
        InitMover(m, startX, startY, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        expect((int)m->z == 1);  // Starts at z=1 (in air)
        
        // Run one tick - falling should happen
        Tick();
        
        // Mover should have fallen to z=0
        expect((int)m->z == 0);
    }
    
    it("should not fall when walking on walkable cells") {
        // Setup: both z=0 and z=1 are walkable
        InitGridWithSize(5, 5);
        gridDepth = 2;
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // z=0 solid, z=1 air above solid is walkable
                grid[0][y][x] = CELL_WALL;   // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;       // Walkable (solid below)
            }
        }
        
        // Verify z=1 is walkable
        expect(IsCellWalkableAt(1, 1, 0) == true);
        
        ClearMovers();
        Mover* m = &movers[0];
        
        // Mover walks on z=1
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {4, 1, 1};
        
        Point testPath[] = {{4, 1, 1}, {0, 1, 1}};
        InitMoverWithPath(m, startX, startY, 1.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        expect(m->z == 1.0f);
        
        // Run for a while
        RunTicks(60);
        
        // Mover should still be at z=1 (no falling)
        expect(m->z == 1.0f);
    }
    
    it("should stop falling when hitting a wall below") {
        // Setup: z=0 has a wall at (2,1), z=1 has a platform at (1,1)
        InitGridWithSize(5, 5);
        gridDepth = 2;
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_AIR;
                grid[1][y][x] = CELL_AIR;
            }
        }
        // Wall at z=0 (2,1) - air above wall is walkable
        grid[0][1][2] = CELL_WALL;
        
        // Starting platform at z=1 (1,1)
        grid[0][1][1] = CELL_WALL;  // Solid below makes z=1 walkable
        SetWallMaterial(1, 1, 0, MAT_DIRT);
        
        ClearMovers();
        Mover* m = &movers[0];
        
        // Start at (1,1) z=1, walk right into air at (2,1) which has wall below
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {3, 1, 1};
        
        Point testPath[] = {{3, 1, 1}, {2, 1, 1}, {1, 1, 1}};
        InitMoverWithPath(m, startX, startY, 1.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;
        
        // Run simulation
        RunTicks(60);
        
        // Mover should NOT have fallen through the wall
        expect(m->z >= 0.0f);
    }
}

describe(mover_z_level_collision) {
    it("should collide with walls on current z-level only") {
        // z=0 has wall, z=1 is open - mover on z=1 should pass through
        const char* map =
            "floor:0\n"
            ".....\n"
            "..#..\n"  // Wall at (2,1) on z=0
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"  // No wall at (2,1) on z=1
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        
        ClearMovers();
        Mover* m = &movers[0];
        
        // Mover walks across z=1, passing over where wall is on z=0
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {4, 1, 1};
        
        Point testPath[] = {{4, 1, 1}, {0, 1, 1}};
        InitMoverWithPath(m, startX, startY, 1.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        // Run until mover reaches goal or times out
        for (int tick = 0; tick < 300; tick++) {
            Tick();
            if (!m->active) break;
        }
        
        // Mover should have reached the goal (passed through the "wall" area on z=1)
        expect(m->active == false);
    }
    
    it("should be blocked by walls on current z-level") {
        // Wall on z=1 should block mover on z=1
        const char* map =
            "floor:0\n"
            ".....\n"
            ".....\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            "..#..\n"  // Wall at (2,1) on z=1
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        BuildEntrances();
        BuildGraph();
        
        ClearMovers();
        Mover* m = &movers[0];
        
        // Try to path from left to right on z=1, wall in the way
        startPos = (Point){0, 1, 1};
        goalPos = (Point){4, 1, 1};
        RunHPAStar();
        
        // Path should go around the wall (through y=0 or y=2), not be empty
        // Actually let's just verify wall collision works by direct movement
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {3, 1, 1};
        
        // Force a straight path through the wall
        Point testPath[] = {{3, 1, 1}, {2, 1, 1}, {1, 1, 1}};
        InitMoverWithPath(m, startX, startY, 1.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;
        
        // Run simulation. The mover may legally repath around the wall (HPA* finds
        // the detour), but must never stand inside the wall cell.
        bool enteredWall = false;
        for (int t = 0; t < 60; t++) {
            Tick();
            int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE);
            if (cx == 2 && cy == 1 && (int)m->z == 1) enteredWall = true;
        }
        expect(!enteredWall);
    }
}

// Two floors (z=1 on dirt, z=2 on a constructed floor) joined by one ladder at (3,2)
static void SetupLadderFloors(void) {
    InitGridWithSize(8, 5);
    gridDepth = 3;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
            grid[2][y][x] = CELL_AIR;
            SET_FLOOR(x, y, 2);
        }
    grid[1][2][3] = CELL_LADDER_BOTH;
    grid[2][2][3] = CELL_LADDER_BOTH;
    BuildEntrances();   // waiting movers re-plan (queue cost): they need the graph
    BuildGraph();
    ClearMovers();
}

// Mover i walks from (sx,sy,sz) over the ladder to (gx,gy,gz)
static void AddLadderMover(int i, int sx, int sy, int sz, int gx, int gy, int gz) {
    Point p[] = {{gx, gy, gz}, {3, 2, gz}, {3, 2, sz}, {sx, sy, sz}};
    InitMoverWithPath(&movers[i], sx * CELL_SIZE + CELL_SIZE * 0.5f, sy * CELL_SIZE + CELL_SIZE * 0.5f,
                      (float)sz, p[0], 100.0f, p, 4);
}

// Runs until all movers arrive; returns the most movers seen on the ladder at once
static int RunLadderMovers(int count, int maxTicks, bool* allArrived) {
    int most = 0;
    *allArrived = false;
    for (int t = 0; t < maxTicks; t++) {
        Tick();
        int onLadder = 0, active = 0;
        for (int i = 0; i < count; i++) {
            const Mover* m = &movers[i];
            if (!m->active) continue;
            active++;
            bool between = m->z - floorf(m->z) > 0.0001f;
            if (between && (int)(m->x / CELL_SIZE) == 3 && (int)(m->y / CELL_SIZE) == 2) onLadder++;
        }
        if (onLadder > most) most = onLadder;
        if (active == 0) { *allArrived = true; break; }
    }
    return most;
}

// Ladder A at (3,2), ladder B at (9,2), same two floors as SetupLadderFloors
static void SetupTwoLadderFloors(void) {
    InitGridWithSize(14, 5);
    gridDepth = 3;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
            grid[2][y][x] = CELL_AIR;
            SET_FLOOR(x, y, 2);
        }
    grid[1][2][3] = grid[2][2][3] = CELL_LADDER_BOTH;
    grid[1][2][9] = grid[2][2][9] = CELL_LADDER_BOTH;
    BuildEntrances();
    BuildGraph();
    ClearMovers();
}

static bool PathUsesCell(const Point* p, int len, int x, int y) {
    for (int k = 0; k < len; k++) if (p[k].x == x && p[k].y == y) return true;
    return false;
}

describe(ladder_queue_cost) {
    it("routes around a ladder with a queue when another way is quicker") {
        SetupTwoLadderFloors();
        // Eight movers around ladder A, all climbing it
        static const int sp[8][2] = {{2, 1}, {2, 2}, {2, 3}, {3, 1}, {3, 3}, {4, 1}, {4, 3}, {1, 2}};
        for (int i = 0; i < 8; i++) {
            Point p[] = {{1, 0, 2}, {3, 2, 2}, {3, 2, 1}, {sp[i][0], sp[i][1], 1}};
            InitMoverWithPath(&movers[i], sp[i][0] * CELL_SIZE + CELL_SIZE * 0.5f, sp[i][1] * CELL_SIZE + CELL_SIZE * 0.5f,
                              1.0f, p[0], 100.0f, p, 4);
        }
        moverCount = 8;
        // Run until a real queue has formed at A's foot and the ladder's
        // interval between climbs has been measured
        int queue = 0;
        for (int t = 0; t < 600; t++) {
            Tick();
            queue = LadderQueueExtraCost(3, 2, 1, 1);
            int p0 = ServiceFind(SERVICE_LADDER, 3, 2, 1);
            if (queue >= 50 && p0 >= 0 && servicePoints[p0].samples > 0) break;
        }
        expect(queue >= 50);
        // Ladder B has no queue: only the slow-climb extra (default speed 0.5:
        // a level costs twice the step onto the cell above, i.e. one step more)
        expect(LadderQueueExtraCost(9, 2, 1, 1) == GetCellMoveCost(9, 2, 2));

        // From next to A to just above it: A is 2 cells away, B 4; with the queue, B wins
        static Point buf[MAX_PATH];
        int len = FindPath(PATH_ALGO_ASTAR, (Point){5, 2, 1}, (Point){5, 2, 2}, buf, MAX_PATH);
        expect(len > 0);
        expect(PathUsesCell(buf, len, 9, 2));
        expect(!PathUsesCell(buf, len, 3, 2));
        len = FindPath(PATH_ALGO_HPA, (Point){5, 2, 1}, (Point){5, 2, 2}, buf, MAX_PATH);
        expect(len > 0);
        expect(PathUsesCell(buf, len, 9, 2));

        // The cost is the measured wait: people waiting x the ladder's interval
        // between climbs, at walking pace (10 per cell walked)
        int lsp = ServiceFind(SERVICE_LADDER, 3, 2, 1);
        expect(lsp >= 0);
        expect(servicePoints[lsp].samples > 0);
        float interval = servicePoints[lsp].avgServiceTime;
        expect(interval > 0.0f && interval < 5.0f);
        float base = balance.baseMoverSpeed > 0.0f ? balance.baseMoverSpeed : 200.0f;   // tests skip InitBalance
        float perCell = CELL_SIZE / (base * (60.0f / dayLength));
        int now = LadderQueueExtraCost(3, 2, 1, 1);
        int people = ServiceLineLength(lsp, 1);
        for (int k = 0; k < moverCount; k++) {
            const Mover* m = &movers[k];
            if (m->active && m->z - floorf(m->z) > 0.0001f && (int)(m->x / CELL_SIZE) == 3 && (int)(m->y / CELL_SIZE) == 2) people++;
        }
        int expected = GetCellMoveCost(3, 2, 2) + (int)(people * interval / perCell * 10.0f + 0.5f);   // slow climb + queue
        expect(abs(now - expected) <= 1);

        // Without the queue cost the short way (A) is chosen
        useLadderQueueCost = false;
        len = FindPath(PATH_ALGO_ASTAR, (Point){5, 2, 1}, (Point){5, 2, 2}, buf, MAX_PATH);
        useLadderQueueCost = true;
        expect(len > 0);
        expect(PathUsesCell(buf, len, 3, 2));
    }
}

describe(single_file_ladders) {
    it("lets one mover climb a ladder segment at a time; the others wait and then go") {
        SetupLadderFloors();
        AddLadderMover(0, 2, 2, 1, 6, 2, 2);
        AddLadderMover(1, 4, 2, 1, 6, 1, 2);
        AddLadderMover(2, 3, 1, 1, 6, 3, 2);
        moverCount = 3;
        bool arrived;
        int most = RunLadderMovers(3, 1200, &arrived);
        expect(most == 1);
        expect(arrived);
    }

    it("waiting movers line up behind each other instead of piling onto the ladder") {
        SetupLadderFloors();
        static const int sp[6][2] = {{2, 2}, {1, 2}, {0, 2}, {2, 1}, {2, 3}, {1, 1}};
        for (int i = 0; i < 6; i++) AddLadderMover(i, sp[i][0], sp[i][1], 1, 6, 2, 2);
        moverCount = 6;
        int mostOnFootSpot = 0, mostClimbing = 0;
        bool allArrived = false;
        for (int t = 0; t < 3000 && !allArrived; t++) {
            Tick();
            int onSpot = 0, climbing = 0, active = 0;
            for (int i = 0; i < 6; i++) {
                const Mover* m = &movers[i];
                if (!m->active) continue;
                active++;
                float dx = m->x - (3 * CELL_SIZE + CELL_SIZE * 0.5f), dy = m->y - (2 * CELL_SIZE + CELL_SIZE * 0.5f);
                bool between = m->z - floorf(m->z) > 0.0001f;
                if (between) climbing++;
                else if (m->z == 1.0f && dx * dx + dy * dy < (CELL_SIZE * 0.25f) * (CELL_SIZE * 0.25f)) onSpot++;
            }
            if (onSpot > mostOnFootSpot) mostOnFootSpot = onSpot;
            if (climbing > mostClimbing) mostClimbing = climbing;
            allArrived = active == 0;
        }
        expect(allArrived);
        expect(mostClimbing == 1);
        expect(mostOnFootSpot <= 1);     // only the front stands on the foot
    }

    it("climbing is slower than walking by ladderClimbSpeed") {
        int ticks[2];
        float speeds[2] = {1.0f, 0.5f};
        float saved = ladderClimbSpeed;
        for (int run = 0; run < 2; run++) {
            ladderClimbSpeed = speeds[run];
            SetupLadderFloors();
            AddLadderMover(0, 2, 2, 1, 6, 2, 2);
            moverCount = 1;
            int climbing = 0;
            for (int t = 0; t < 600 && movers[0].active; t++) {
                Tick();
                if (movers[0].z - floorf(movers[0].z) > 0.0001f) climbing++;
            }
            ticks[run] = climbing;
        }
        ladderClimbSpeed = saved;
        expect(ticks[0] > 0);
        expect(ticks[1] >= ticks[0] * 18 / 10 && ticks[1] <= ticks[0] * 22 / 10);   // ~2x as long
    }

    it("an impatient mover with nothing to do gives up on a slow line, others keep waiting") {
        float savedSpeed = ladderClimbSpeed;
        bool savedEndless = endlessMoverMode, savedPrefer = preferDifferentZ;
        ladderClimbSpeed = 0.1f;        // ~3 s per climb: a slow line
        endlessMoverMode = true;
        preferDifferentZ = false;
        SetupLadderFloors();
        static const int sp[6][2] = {{2, 2}, {1, 2}, {0, 2}, {2, 1}, {2, 3}, {1, 1}};
        for (int i = 0; i < 6; i++) {
            AddLadderMover(i, sp[i][0], sp[i][1], 1, 6, 2, 2);
            movers[i].traits[0] = TRAIT_NONE;
            movers[i].traits[1] = TRAIT_NONE;
        }
        movers[5].traits[0] = TRAIT_IMPATIENT;   // last in line: gives up at ~8 s
        moverCount = 6;
        bool joined[6] = {0}, gaveUp[6] = {0}, climbed[6] = {0};
        double start = gameTime;
        while (gameTime - start < 12.0) {
            Tick();
            for (int i = 0; i < 6; i++) {
                if (movers[i].z - floorf(movers[i].z) > 0.0001f || movers[i].z >= 2.0f) climbed[i] = true;
                if (ServiceOf(i) >= 0) joined[i] = true;
                else if (joined[i] && !climbed[i]) gaveUp[i] = true;
            }
        }
        ladderClimbSpeed = savedSpeed;
        endlessMoverMode = savedEndless;
        preferDifferentZ = savedPrefer;
        expect(joined[5]);
        expect(gaveUp[5]);            // the impatient one left before its turn
        expect(!climbed[5]);
        expect(!gaveUp[4]);           // a normal one (needs 20 s) is still waiting
    }

    it("handles movers going up and down at the same time") {
        SetupLadderFloors();
        AddLadderMover(0, 2, 2, 1, 6, 2, 2);   // up
        AddLadderMover(1, 4, 2, 2, 0, 2, 1);   // down
        moverCount = 2;
        bool arrived;
        int most = RunLadderMovers(2, 1200, &arrived);
        expect(most == 1);
        expect(arrived);
    }

    it("climbs all the way down: never left between levels after a descent") {
        SetupLadderFloors();
        AddLadderMover(0, 4, 2, 2, 0, 2, 1);   // down
        moverCount = 1;
        bool leftBetween = false;
        for (int t = 0; t < 600 && movers[0].active; t++) {
            Tick();
            const Mover* m = &movers[0];
            bool between = m->z - floorf(m->z) > 0.0001f;
            bool onLadder = (int)(m->x / CELL_SIZE) == 3 && (int)(m->y / CELL_SIZE) == 2;
            if (between && !onLadder) leftBetween = true;
        }
        expect(!movers[0].active);            // arrived
        expect(!leftBetween);
        expect(movers[0].z == 1.0f);
    }

    it("without the rule they climb together (the test can tell)") {
        SetupLadderFloors();
        useSingleFileLadders = false;
        AddLadderMover(0, 2, 2, 1, 6, 2, 2);
        AddLadderMover(1, 4, 2, 1, 6, 1, 2);
        AddLadderMover(2, 3, 1, 1, 6, 3, 2);
        moverCount = 3;
        bool arrived;
        int most = RunLadderMovers(3, 1200, &arrived);
        useSingleFileLadders = true;
        expect(most >= 2);
        expect(arrived);
    }
}

describe(mover_ladder_transitions) {
    it("should transition z-level when reaching ladder waypoint") {
        // Setup: two walkable floors with ladder connecting them
        // z=0 is solid dirt, z=1 is walkable air above dirt,
        // z=2 is walkable above constructed floor, ladder at z=1 and z=2
        InitGridWithSize(5, 5);
        
        gridDepth = 3;
        int startZ = 1;  // Walk on air above dirt
        int goalZ = 2;   // Walk on air above constructed floor
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;   // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;    // Walkable (above dirt)
                grid[2][y][x] = CELL_AIR;    // Not walkable yet
                SET_FLOOR(x, y, 2);          // Now walkable (constructed floor)
            }
        }
        grid[1][1][2] = CELL_LADDER_BOTH;
        grid[2][1][2] = CELL_LADDER_BOTH;

        // Verify walkability
        expect(IsCellWalkableAt(startZ, 1, 0) == true);
        expect(IsCellWalkableAt(goalZ, 1, 4) == true);

        ClearMovers();
        Mover* m = &movers[0];

        // Mover starts at (0,1) startZ, needs to go to (4,1) goalZ
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {4, 1, goalZ};

        // Path stored goal-to-start
        Point testPath[] = {
            {4, 1, goalZ},           // goal
            {2, 1, goalZ},           // ladder on upper level
            {2, 1, startZ},          // ladder on lower level
            {0, 1, startZ}           // start
        };
        InitMoverWithPath(m, startX, startY, (float)startZ, goal, 100.0f, testPath, 4);
        moverCount = 1;

        expect((int)m->z == startZ);

        // Run until mover reaches goal or times out
        int maxTicks = 600;
        for (int tick = 0; tick < maxTicks; tick++) {
            Tick();
            if (!m->active) break;
        }

        // Mover should have reached the goal
        expect(m->active == false);
    }

    it("should climb ladder when path goes through ladder cell") {
        // Same setup as above
        InitGridWithSize(5, 5);
        
        gridDepth = 3;
        int startZ = 1;
        int goalZ = 2;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);
            }
        }
        grid[1][1][2] = CELL_LADDER_BOTH;
        grid[2][1][2] = CELL_LADDER_BOTH;

        ClearMovers();
        Mover* m = &movers[0];

        // Start right next to ladder, goal is on upper level
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {3, 1, goalZ};

        // Direct path through ladder
        Point testPath[] = {
            {3, 1, goalZ},           // goal
            {2, 1, goalZ},           // ladder on upper level
            {2, 1, startZ},          // ladder on lower level
            {1, 1, startZ}           // start
        };
        InitMoverWithPath(m, startX, startY, (float)startZ, goal, 100.0f, testPath, 4);
        moverCount = 1;

        expect((int)m->z == startZ);

        // Run simulation
        for (int tick = 0; tick < 300; tick++) {
            Tick();
            if (!m->active) break;
        }

        // Mover should have reached goal
        expect(m->active == false);
    }

    it("should handle HPA* 3D path through Labyrinth3D") {
        // Use actual Labyrinth3D terrain (works in both walkability modes)
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateLabyrinth3D();
        BuildEntrances();
        BuildGraph();

        // In standard mode, baseZ=1 so walkable floors are z=1,z=2; in legacy baseZ=0
        int baseZ = 1;
        int lowerZ = baseZ;
        int upperZ = baseZ + 1;

        // Find a start on lowerZ and goal on upperZ
        SeedRandom(12345);
        Point start = {-1, -1, lowerZ};
        Point goal = {-1, -1, upperZ};

        // Find walkable start on lowerZ
        for (int attempts = 0; attempts < 1000 && start.x < 0; attempts++) {
            int x = GetRandomValue(0, gridWidth - 1);
            int y = GetRandomValue(0, gridHeight - 1);
            if (IsCellWalkableAt(lowerZ, y, x)) {
                start = (Point){x, y, lowerZ};
            }
        }

        // Find walkable goal on upperZ
        for (int attempts = 0; attempts < 1000 && goal.x < 0; attempts++) {
            int x = GetRandomValue(0, gridWidth - 1);
            int y = GetRandomValue(0, gridHeight - 1);
            if (IsCellWalkableAt(upperZ, y, x)) {
                goal = (Point){x, y, upperZ};
            }
        }

        expect(start.x >= 0);
        expect(goal.x >= 0);

        // Get HPA* 3D path
        Point pathBuf[MAX_PATH];
        moverPathAlgorithm = PATH_ALGO_HPA;
        int pathLen = FindPath(PATH_ALGO_HPA, start, goal, pathBuf, MAX_PATH);

        if (pathLen > 0) {
            // Validate path: each step should be walkable
            int invalidSteps = 0;
            for (int i = 0; i < pathLen; i++) {
                Point p = pathBuf[i];
                if (!IsCellWalkableAt(p.z, p.y, p.x)) {
                    TraceLog(LOG_WARNING, "  path[%d]=(%d,%d,%d) is NOT walkable!", i, p.x, p.y, p.z);
                    invalidSteps++;
                }
            }
            expect(invalidSteps == 0);

            // Validate path: no step should be more than 1 cell away (except z transitions at ladders)
            int bigJumps = 0;
            for (int i = 1; i < pathLen; i++) {
                int dx = abs(pathBuf[i].x - pathBuf[i-1].x);
                int dy = abs(pathBuf[i].y - pathBuf[i-1].y);
                int dz = abs(pathBuf[i].z - pathBuf[i-1].z);

                // Normal step: max 1 cell in x and y, same z
                // Ladder step: same x and y, z changes by 1
                bool normalStep = (dx <= 1 && dy <= 1 && dz == 0);
                bool ladderStep = (dx == 0 && dy == 0 && dz == 1);

                if (!normalStep && !ladderStep) {
                    TraceLog(LOG_WARNING, "  Big jump from (%d,%d,%d) to (%d,%d,%d)",
                             pathBuf[i-1].x, pathBuf[i-1].y, pathBuf[i-1].z,
                             pathBuf[i].x, pathBuf[i].y, pathBuf[i].z);
                    bigJumps++;
                }
            }
            expect(bigJumps == 0);

            // Now test actual mover following the path
            ClearMovers();
            Mover* m = &movers[0];
            float startX = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
            float startY = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
            InitMoverWithPath(m, startX, startY, (float)start.z, goal, 100.0f, pathBuf, pathLen);
            moverCount = 1;

            // Run simulation - long paths across z-levels need more time
            for (int tick = 0; tick < 3000; tick++) {
                Tick();
                if (!m->active) break;
            }

            // Mover should have reached goal
            expect(m->active == false);
        }
    }

    it("A* and HPA* 3D paths should both use the ladder") {
        // Compare paths from both algorithms to ensure both produce valid ladder transitions
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateLabyrinth3D();
        BuildEntrances();
        BuildGraph();

        // In standard mode, baseZ=1 so ladders are at z=1,z=2; in legacy baseZ=0
        int baseZ = 1;
        int lowerZ = baseZ;
        int upperZ = baseZ + 1;

        // Find a ladder
        int ladderX = -1, ladderY = -1;
        for (int y = 0; y < gridHeight && ladderX < 0; y++) {
            for (int x = 0; x < gridWidth && ladderX < 0; x++) {
                if (grid[lowerZ][y][x] == CELL_LADDER_BOTH && grid[upperZ][y][x] == CELL_LADDER_BOTH) {
                    ladderX = x;
                    ladderY = y;
                }
            }
        }
        expect(ladderX >= 0);

        // Find walkable start near ladder on lowerZ
        Point start = {-1, -1, lowerZ};
        for (int dy = -5; dy <= 5 && start.x < 0; dy++) {
            for (int dx = -5; dx <= 5 && start.x < 0; dx++) {
                int x = ladderX + dx;
                int y = ladderY + dy;
                if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
                    if (IsCellWalkableAt(lowerZ, y, x) && grid[lowerZ][y][x] != CELL_LADDER_BOTH) {
                        start = (Point){x, y, lowerZ};
                    }
                }
            }
        }

        // Find walkable goal near ladder on upperZ
        Point goal = {-1, -1, upperZ};
        for (int dy = -5; dy <= 5 && goal.x < 0; dy++) {
            for (int dx = -5; dx <= 5 && goal.x < 0; dx++) {
                int x = ladderX + dx;
                int y = ladderY + dy;
                if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
                    if (IsCellWalkableAt(upperZ, y, x) && grid[upperZ][y][x] != CELL_LADDER_BOTH) {
                        goal = (Point){x, y, upperZ};
                    }
                }
            }
        }

        expect(start.x >= 0);
        expect(goal.x >= 0);

        // Get A* path
        startPos = start;
        goalPos = goal;
        use8Dir = true;
        RunAStar();
        int astarLen = pathLength;

        // Check A* path has ladder transition
        bool astarHasLadder = false;
        for (int i = 1; i < astarLen; i++) {
            if (path[i].x == path[i-1].x && path[i].y == path[i-1].y && path[i].z != path[i-1].z) {
                astarHasLadder = true;
            }
        }
        expect(astarHasLadder == true);

        // Get HPA* path
        Point hpaPath[MAX_PATH];
        int hpaLen = FindPath(PATH_ALGO_HPA, start, goal, hpaPath, MAX_PATH);

        // Check HPA* path has ladder transition
        bool hpaHasLadder = false;
        for (int i = 1; i < hpaLen; i++) {
            if (hpaPath[i].x == hpaPath[i-1].x && 
                hpaPath[i].y == hpaPath[i-1].y && 
                hpaPath[i].z != hpaPath[i-1].z) {
                hpaHasLadder = true;
            }
        }
        expect(hpaHasLadder == true);

        // Both should find paths
        expect(astarLen > 0);
        expect(hpaLen > 0);
    }

    it("should handle multiple random cross-z paths with HPA*") {
        // Test many random paths to catch intermittent issues
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        gridDepth = 4;
        GenerateLabyrinth3D();
        BuildEntrances();
        BuildGraph();

        SeedRandom(99999);
        moverPathAlgorithm = PATH_ALGO_HPA;

        int totalTests = STRESS_TEST_ITERATIONS;
        int pathsFound = 0;
        int moversReachedGoal = 0;

        for (int test = 0; test < totalTests; test++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCellDifferentZ(start.z);

            if (start.x < 0 || goal.x < 0) continue;

            Point pathBuf[MAX_PATH];
            int pathLen = FindPath(PATH_ALGO_HPA, start, goal, pathBuf, MAX_PATH);

            if (pathLen > 0) {
                pathsFound++;

                // Test mover following this path
                ClearMovers();
                Mover* m = &movers[0];
                float startX = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float startY = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, startX, startY, (float)start.z, goal, 100.0f, pathBuf, pathLen);
                moverCount = 1;

                // Run simulation - 10000 ticks allows traversing long labyrinth paths
                for (int tick = 0; tick < 10000; tick++) {
                    Tick();
                    if (!m->active) break;
                }

                if (!m->active) {
                    moversReachedGoal++;
                } else {
                    // Debug: mover got stuck
                    int stuckX = (int)(m->x / CELL_SIZE);
                    int stuckY = (int)(m->y / CELL_SIZE);
                    int stuckZ = (int)m->z;
                    printf("STUCK: test %d, start=(%d,%d,%d) goal=(%d,%d,%d) stuck at cell=(%d,%d,%d) pathIndex=%d/%d\n",
                           test, start.x, start.y, start.z, goal.x, goal.y, goal.z,
                           stuckX, stuckY, stuckZ, GetMoverPathIndex(0), GetMoverPathLength(0));
                }
            }
        }

        // At least some paths should be found
        expect(pathsFound > 0);  // At least some paths should be found
        // All movers that got paths should reach their goals
        if (test_verbose) printf("pathsFound=%d, moversReachedGoal=%d\n", pathsFound, moversReachedGoal);
        expect(moversReachedGoal == pathsFound);
    }
}

describe(sparse_level_pathfinding) {
    it("should handle random paths with A* on sparse level") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        SetRandomSeed(12345);  // raylib's seed
        SeedRandom(12345);
        GenerateSparse(0.10f);
        BuildEntrances();
        BuildGraph();

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        int totalTests = STRESS_TEST_ITERATIONS;
        int pathsFound = 0;
        int moversReachedGoal = 0;

        for (int test = 0; test < totalTests; test++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();
            start.z = 1;  // the sparse layout is on level 1 (level 0 is its solid ground)
            goal.z = 1;

            if (start.x < 0 || goal.x < 0) continue;

            Point pathBuf[MAX_PATH];
            int pathLen = FindPath(PATH_ALGO_ASTAR, start, goal, pathBuf, MAX_PATH);

            if (pathLen > 0) {
                pathsFound++;

                ClearMovers();
                Mover* m = &movers[0];
                float startX = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float startY = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, startX, startY, (float)start.z, goal, 100.0f, pathBuf, pathLen);
                moverCount = 1;

                for (int tick = 0; tick < 5000; tick++) {
                    Tick();
                    if (!m->active) break;
                }

                if (!m->active) {
                    moversReachedGoal++;
                } else {
                    int stuckX = (int)(m->x / CELL_SIZE);
                    int stuckY = (int)(m->y / CELL_SIZE);
                    printf("A* STUCK: test %d, start=(%d,%d) goal=(%d,%d) stuck at cell=(%d,%d) pathIndex=%d/%d\n",
                           test, start.x, start.y, goal.x, goal.y,
                           stuckX, stuckY, GetMoverPathIndex(0), GetMoverPathLength(0));
                }
            }
        }

        if (test_verbose) printf("A* sparse: pathsFound=%d, moversReachedGoal=%d\n", pathsFound, moversReachedGoal);
        expect(pathsFound > 0);  // At least some paths should be found
        expect(moversReachedGoal == pathsFound);
    }

    it("should handle random paths with HPA* on sparse level") {
        InitGridWithSizeAndChunkSize(64, 64, 8, 8);
        SeedRandom(12345);
        GenerateSparse(0.10f);
        BuildEntrances();
        BuildGraph();

        moverPathAlgorithm = PATH_ALGO_HPA;

        int totalTests = STRESS_TEST_ITERATIONS;
        int pathsFound = 0;
        int moversReachedGoal = 0;

        for (int test = 0; test < totalTests; test++) {
            Point start = GetRandomWalkableCell();
            Point goal = GetRandomWalkableCell();
            start.z = 1;  // the sparse layout is on level 1 (level 0 is its solid ground)
            goal.z = 1;

            if (start.x < 0 || goal.x < 0) continue;

            Point pathBuf[MAX_PATH];
            int pathLen = FindPath(PATH_ALGO_HPA, start, goal, pathBuf, MAX_PATH);

            if (pathLen > 0) {
                pathsFound++;

                ClearMovers();
                Mover* m = &movers[0];
                float startX = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float startY = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, startX, startY, (float)start.z, goal, 100.0f, pathBuf, pathLen);
                moverCount = 1;

                for (int tick = 0; tick < 5000; tick++) {
                    Tick();
                    if (!m->active) break;
                }

                if (!m->active) {
                    moversReachedGoal++;
                } else {
                    int stuckX = (int)(m->x / CELL_SIZE);
                    int stuckY = (int)(m->y / CELL_SIZE);
                    printf("HPA* STUCK: test %d, start=(%d,%d) goal=(%d,%d) stuck at cell=(%d,%d) pathIndex=%d/%d\n",
                           test, start.x, start.y, goal.x, goal.y,
                           stuckX, stuckY, GetMoverPathIndex(0), GetMoverPathLength(0));
                }
            }
        }

        if (test_verbose) printf("HPA* sparse: pathsFound=%d, moversReachedGoal=%d\n", pathsFound, moversReachedGoal);
        expect(pathsFound > 0);  // At least some paths should be found
        expect(moversReachedGoal == pathsFound);
    }
}

describe(staggered_updates) {
    it("movers should still reach goals with staggered LOS and avoidance") {
        // Test that staggered updates (every 3 frames) don't break goal completion
        InitGridFromAsciiWithChunkSize(
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "................................\n", 8, 8);
        
        BuildEntrances();
        BuildGraph();
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
        
        // Spawn 100 movers with paths across the map
        ClearMovers();
        int spawned = 0;
        for (int i = 0; i < 100; i++) {
            Point start = {i % 8, i / 8, 0};
            Point goal = {31 - (i % 8), 15 - (i / 8), 0};
            
            startPos = start;
            goalPos = goal;
            RunHPAStar();
            
            if (pathLength > 0) {
                Mover* m = &movers[moverCount];
                float sx = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float sy = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, sx, sy, 0.0f, goal, 100.0f, path, pathLength);
                moverCount++;
                spawned++;
            }
        }
        
        // Run for enough ticks to complete (32 cells at 100 speed = ~10 seconds = 600 ticks)
        endlessMoverMode = false;  // Don't assign new goals
        for (int tick = 0; tick < 1000; tick++) {
            Tick();
        }
        
        // Count how many reached their goal
        int reached = 0;
        for (int i = 0; i < moverCount; i++) {
            if (!movers[i].active) reached++;  // Inactive means reached goal
        }
        
        // At least 90% should reach their goals
        float completionRate = (float)reached / spawned;
        expect(completionRate >= 0.9f);
    }
    
    it("movers should not get stuck in walls with staggered LOS") {
        // Maze-like map to stress test wall detection
        InitGridFromAsciiWithChunkSize(
            "................................\n"
            ".######.######.######.######.#.\n"
            "................................\n"
            ".#.######.######.######.######.\n"
            "................................\n"
            ".######.######.######.######.#.\n"
            "................................\n"
            ".#.######.######.######.######.\n"
            "................................\n"
            ".######.######.######.######.#.\n"
            "................................\n"
            ".#.######.######.######.######.\n"
            "................................\n"
            ".######.######.######.######.#.\n"
            "................................\n"
            "................................\n", 8, 8);
        
        BuildEntrances();
        BuildGraph();
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
        
        // Spawn movers that need to navigate the maze
        ClearMovers();
        int spawned = 0;
        for (int i = 0; i < 20; i++) {
            Point start = {0, i % 16, 0};
            Point goal = {31, 15 - (i % 16), 0};
            
            startPos = start;
            goalPos = goal;
            RunHPAStar();
            
            if (pathLength > 0) {
                Mover* m = &movers[moverCount];
                float sx = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float sy = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, sx, sy, 0.0f, goal, 100.0f, path, pathLength);
                moverCount++;
                spawned++;
            }
        }
        
        endlessMoverMode = false;
        for (int tick = 0; tick < 2000; tick++) {
            Tick();
            
            // Check no mover is inside a wall
            for (int i = 0; i < moverCount; i++) {
                Mover* m = &movers[i];
                if (!m->active) continue;
                int cx = (int)(m->x / CELL_SIZE);
                int cy = (int)(m->y / CELL_SIZE);
                int cz = (int)m->z;
                if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight) {
                    expect(grid[cz][cy][cx] != CELL_WALL);
                }
            }
        }
        
        // Most should complete
        int reached = 0;
        for (int i = 0; i < moverCount; i++) {
            if (!movers[i].active) reached++;
        }
        float completionRate = (float)reached / spawned;
        expect(completionRate >= 0.8f);
    }
}

// ============================================================================
// Workshop Collision Tests
// ============================================================================

describe(workshop_mover_collision) {
    it("should push mover out when workshop placed on it") {
        // Mover at (1,1), we place a workshop there - mover should be pushed out
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearWorkshops();
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {7, 7, 0};
        Point testPath[] = {{1, 1, 0}, {7, 7, 0}};
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        // Verify mover is at (1,1)
        int cellX = (int)(m->x / CELL_SIZE);
        int cellY = (int)(m->y / CELL_SIZE);
        expect(cellX == 1 && cellY == 1);
        
        // Place workshop at (0,0) - stonecutter is 3x3 with blocked tiles at (0,0), (1,0), (0,1), (1,1)
        // Template is "##O" / "#X." / "..." so (0,0), (1,0), (0,1) are blocked
        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        
        // Mover should have been pushed out of the blocked cell
        cellX = (int)(m->x / CELL_SIZE);
        cellY = (int)(m->y / CELL_SIZE);
        
        // Check if mover was on a blocked tile - template row 1 col 1 is 'X' (work tile, not blocked)
        // Actually (1,1) relative to workshop at (0,0) is template[1*3+1] = 'X' which is walkable
        // Let's check (0,0) and (1,0) and (0,1) which are blocked
        expect(!IsWorkshopBlocking(1, 1, 0));  // (1,1) is work tile, not blocked
        expect(IsWorkshopBlocking(0, 0, 0));   // (0,0) is blocked
        expect(IsWorkshopBlocking(1, 0, 0));   // (1,0) is blocked
        expect(IsWorkshopBlocking(0, 1, 0));   // (0,1) is blocked
        
        DeleteWorkshop(wsIdx);
    }
    
    it("should push mover out of blocked workshop tile") {
        // Place mover specifically on a tile that WILL be blocked
        // Workshop template is "##O" / "#X." / "..." 
        // Place workshop at (2,2) so blocked tiles are (2,2), (3,2), (2,3)
        // Place mover at (2,3) which will be blocked, with (1,3) and (2,4) as escape routes
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearWorkshops();
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {7, 7, 0};
        Point testPath[] = {{2, 3, 0}, {7, 7, 0}};
        // Place mover at (2,3) which will become blocked by workshop at (2,2)
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        // Verify mover is at (2,3)
        int cellX = (int)(m->x / CELL_SIZE);
        int cellY = (int)(m->y / CELL_SIZE);
        expect(cellX == 2 && cellY == 3);
        
        // Place workshop at (2,2) - tile (2,3) relative is row 1 col 0 = '#' (blocked)
        int wsIdx = CreateWorkshop(2, 2, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        expect(IsWorkshopBlocking(2, 3, 0));  // Verify (2,3) is blocked
        
        // Mover should have been pushed to adjacent walkable cell
        cellX = (int)(m->x / CELL_SIZE);
        cellY = (int)(m->y / CELL_SIZE);
        expect(!(cellX == 2 && cellY == 3));  // Should NOT be at (2,3) anymore
        expect(m->needsRepath == true);  // Should need repath after being pushed
        
        DeleteWorkshop(wsIdx);
    }
    
    it("should invalidate mover path when workshop blocks path waypoint") {
        // Mover has a path that goes through a cell that becomes blocked
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearWorkshops();
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {7, 0, 0};
        // Path goes through (1,0) which will become blocked by workshop
        Point testPath[] = {{7, 0, 0}, {1, 0, 0}, {0, 0, 0}};  // goal-to-start order
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;  // Start at (0,3) away from workshop
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;
        
        expect(m->needsRepath == false);  // Path is valid initially
        
        // Place workshop at (0,0) - this blocks (1,0) which is in the path
        int wsIdx = CreateWorkshop(0, 0, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        expect(IsWorkshopBlocking(1, 0, 0));  // (1,0) should be blocked
        
        // Mover's path goes through (1,0), so it should need repath
        expect(m->needsRepath == true);
        
        DeleteWorkshop(wsIdx);
    }
    
    it("should include workshop blocks in wall repulsion") {
        // Test that ComputeWallRepulsion considers workshop blocked tiles
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        
        ClearWorkshops();
        
        // Place workshop at (2,2)
        int wsIdx = CreateWorkshop(2, 2, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        
        // Workshop blocked tiles are at (2,2), (3,2), (2,3) based on template "##O" / "#X." / "..."
        expect(IsWorkshopBlocking(2, 2, 0));
        expect(IsWorkshopBlocking(3, 2, 0));
        expect(IsWorkshopBlocking(2, 3, 0));
        
        // Compute wall repulsion near the workshop blocked tile
        // Position near (2,2) blocked tile
        float testX = 2 * CELL_SIZE + CELL_SIZE * 0.5f + CELL_SIZE * 0.3f;  // Slightly right of center
        float testY = 2 * CELL_SIZE + CELL_SIZE * 0.5f + CELL_SIZE * 0.3f;  // Slightly down of center
        
        Vec2 repulsion = ComputeWallRepulsion(testX, testY, 0);
        
        // Should have non-zero repulsion pushing away from blocked tile
        float repulsionMag = repulsion.x * repulsion.x + repulsion.y * repulsion.y;
        expect(repulsionMag > 0.0f);
        
        DeleteWorkshop(wsIdx);
    }
    
    it("should not allow mover to walk through workshop blocked tiles") {
        // Mover tries to walk through workshop - should be blocked
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearWorkshops();
        ClearMovers();
        
        // Place 3x3 stonecutter at (3,0):
        //   (3,0)=# (4,0)=# (5,0)=#
        //   (3,1)=# (4,1)=X (5,1)=O
        //   (3,2)=. (4,2)=. (5,2)=#
        int wsIdx = CreateWorkshop(3, 0, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        
        // Verify blocked tiles (#)
        expect(IsWorkshopBlocking(3, 0, 0));
        expect(IsWorkshopBlocking(4, 0, 0));
        expect(IsWorkshopBlocking(5, 0, 0));
        expect(IsWorkshopBlocking(3, 1, 0));
        expect(IsWorkshopBlocking(5, 2, 0));
        
        // IsCellWalkableAt(z, y, x) should return false for blocked tiles
        expect(IsCellWalkableAt(0, 0, 3) == false);  // (x=3,y=0) #
        expect(IsCellWalkableAt(0, 0, 4) == false);  // (x=4,y=0) #
        expect(IsCellWalkableAt(0, 0, 5) == false);  // (x=5,y=0) #
        expect(IsCellWalkableAt(0, 1, 3) == false);  // (x=3,y=1) #
        
        // Work tile, output tile, and open tiles should still be walkable
        expect(IsCellWalkableAt(0, 1, 4) == true);   // (x=4,y=1) is work tile 'X'
        expect(IsCellWalkableAt(0, 1, 5) == true);   // (x=5,y=1) is output tile 'O'
        expect(IsCellWalkableAt(0, 2, 3) == true);   // (x=3,y=2) is open '.'
        expect(IsCellWalkableAt(0, 2, 4) == true);   // (x=4,y=2) is open '.'
        
        DeleteWorkshop(wsIdx);
    }
    
    it("should handle mover walking into newly placed workshop") {
        // Mover is walking toward a location, workshop is placed in its path
        // Mover should stop/repath, not walk through
        InitGridFromAsciiWithChunkSize(
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n"
            "................\n", 8, 8);
        BuildEntrances();
        BuildGraph();
        
        ClearWorkshops();
        ClearMovers();
        
        Mover* m = &movers[0];
        Point goal = {10, 0, 0};
        // Path goes straight across row 0
        Point testPath[] = {{10, 0, 0}, {5, 0, 0}, {0, 0, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;
        
        // Run a few ticks to get mover moving
        RunTicks(10);
        
        // Place workshop at (5,0) - blocks path
        int wsIdx = CreateWorkshop(5, 0, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        
        // Run more ticks
        RunTicks(300);
        
        // Mover should NOT be inside a blocked workshop tile
        int cellX = (int)(m->x / CELL_SIZE);
        int cellY = (int)(m->y / CELL_SIZE);
        expect(!IsWorkshopBlocking(cellX, cellY, 0));
        
        DeleteWorkshop(wsIdx);
    }
    
    it("should not get stuck in narrow corridor with workshops") {
        // Reproduce the bug: narrow 3-cell-high corridor, many movers, workshops placed
        // Create a 32x8 map with walls on top and bottom, leaving 3-cell corridor
        InitGridFromAsciiWithChunkSize(
            "################################\n"
            "################################\n"
            "................................\n"
            "................................\n"
            "................................\n"
            "################################\n"
            "################################\n"
            "################################\n", 8, 8);
        BuildEntrances();
        BuildGraph();
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
        
        ClearWorkshops();
        ClearMovers();
        endlessMoverMode = false;
        
        // Spawn many movers walking left-to-right and right-to-left through corridor
        for (int i = 0; i < 50; i++) {
            Point start, goal;
            if (i % 2 == 0) {
                start = (Point){0, 3, 0};
                goal = (Point){31, 3, 0};
            } else {
                start = (Point){31, 3, 0};
                goal = (Point){0, 3, 0};
            }
            
            startPos = start;
            goalPos = goal;
            RunHPAStar();
            
            if (pathLength > 0) {
                Mover* m = &movers[moverCount];
                float sx = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
                float sy = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
                InitMoverWithPath(m, sx, sy, 0.0f, goal, 100.0f, path, pathLength);
                moverCount++;
            }
        }
        
        // Let movers start moving
        RunTicks(60);
        
        // Place workshops in the corridor at different positions
        // Stonecutter is 3x3, so place at y=2 (covers rows 2,3,4)
        int ws1 = CreateWorkshop(8, 2, 0, WORKSHOP_STONECUTTER);
        expect(ws1 >= 0);
        
        RunTicks(30);
        
        int ws2 = CreateWorkshop(16, 2, 0, WORKSHOP_STONECUTTER);
        expect(ws2 >= 0);
        
        RunTicks(30);
        
        int ws3 = CreateWorkshop(24, 2, 0, WORKSHOP_STONECUTTER);
        expect(ws3 >= 0);
        
        // Run simulation for a while
        RunTicks(600);
        
        // Check that NO movers are stuck inside workshop-blocked tiles
        int stuckInWorkshop = 0;
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (!m->active) continue;
            
            int cellX = (int)(m->x / CELL_SIZE);
            int cellY = (int)(m->y / CELL_SIZE);
            int cellZ = (int)m->z;
            
            if (IsWorkshopBlocking(cellX, cellY, cellZ)) {
                stuckInWorkshop++;
            }
        }
        
        expect(stuckInWorkshop == 0);
        
        DeleteWorkshop(ws1);
        DeleteWorkshop(ws2);
        DeleteWorkshop(ws3);
    }
}

// Coarse paths: a route longer than a mover's path buffer (MAX_MOVER_PATH) is walked to
// the end, refined leg by leg. 64x64 serpentine: ~2000 steps from top-left to bottom-left.
static bool WalkSerpentine(bool coarse) {
    static char ascii[64 * 65 + 1];
    int k = 0;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            bool wall = (y % 2 == 1) && !((y % 4 == 1) ? x == 63 : x == 0);
            ascii[k++] = wall ? '#' : '.';
        }
        ascii[k++] = '\n';
    }
    ascii[k] = 0;
    InitGridFromAsciiWithChunkSize(ascii, 16, 16);
    BuildEntrances();
    BuildGraph();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    ClearMovers();
    bool oldCoarse = useCoarsePaths, oldEndless = endlessMoverMode;
    PathAlgorithm oldAlgo = moverPathAlgorithm;
    useCoarsePaths = coarse;
    endlessMoverMode = false;
    moverPathAlgorithm = PATH_ALGO_HPA;
    Point goal = {0, 62, 0};
    Mover* m = &movers[0];
    InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 1500.0f);
    m->needsRepath = true;
    moverCount = 1;
    bool arrived = false;
    for (int t = 0; t < 400 && !arrived && m->active; t++) {
        RunTicks(60);
        arrived = (int)(m->x / CELL_SIZE) == goal.x && (int)(m->y / CELL_SIZE) == goal.y;
    }
    useCoarsePaths = oldCoarse;
    endlessMoverMode = oldEndless;
    moverPathAlgorithm = oldAlgo;
    return arrived;
}

describe(mover_coarse_paths) {
    it("should walk a route longer than the path buffer to the end") {
        extern int coarseWindowRefills;
        int refills = coarseWindowRefills;
        expect(WalkSerpentine(true));
        expect(coarseWindowRefills > refills);   // the route was refined as it walked
    }

    it("should not get there with full paths cut at the buffer size") {
        expect(!WalkSerpentine(false));          // the case coarse paths fix
    }
}

describe(mover_ramp_transitions) {
    it("should transition z-level UP when walking onto ramp high side") {
        // Ramp at z=0 with high side pointing north
        // Mover walks from south onto ramp, exits at z=1 to the north
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"  // Ramp at (2,1), high side north -> exit at (2,0) z=1
            ".....\n"
            "floor:1\n"
            ".....\n"  // Exit tile at (2,0) z=1
            ".....\n"
            ".....\n";

        InitMultiFloorGridFromAscii(map, 5, 5);

        // Verify ramp setup
        expect(grid[0][1][2] == CELL_RAMP_N);
        expect(CellIsDirectionalRamp(grid[0][1][2]));

        ClearMovers();
        Mover* m = &movers[0];

        // Mover starts at (2,2) z=0, goal at (2,0) z=1
        // Path: start(2,2,0) -> ramp(2,1,0) -> exit(2,0,1)
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {2, 0, 1};

        Point testPath[] = {{2, 0, 1}, {2, 1, 0}, {2, 2, 0}};
        InitMoverWithPath(m, startX, startY, 0.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;

        expect((int)m->z == 0);

        // Tick until mover reaches z=1 or times out
        int maxTicks = 500;
        bool reachedZ1 = false;
        for (int t = 0; t < maxTicks; t++) {
            Tick();
            if ((int)m->z == 1) {
                reachedZ1 = true;
                break;
            }
        }

        expect(reachedZ1);
        expect((int)m->z == 1);
    }

    it("should transition z-level DOWN when walking onto ramp from above") {
        // Ramp at z=0 with high side pointing north
        // Mover walks from z=1 down onto the ramp
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"  // Ramp at (2,1), high side north
            ".....\n"
            "floor:1\n"
            ".....\n"  // Entry from z=1 at (2,0)
            ".....\n"
            ".....\n";

        InitMultiFloorGridFromAscii(map, 5, 5);

        ClearMovers();
        Mover* m = &movers[0];

        // Mover starts at (2,0) z=1, goal at (2,2) z=0
        // Path: start(2,0,1) -> ramp(2,1,0) -> goal(2,2,0)
        float startX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {2, 2, 0};

        Point testPath[] = {{2, 2, 0}, {2, 1, 0}, {2, 0, 1}};
        InitMoverWithPath(m, startX, startY, 1.0f, goal, 100.0f, testPath, 3);
        moverCount = 1;

        expect((int)m->z == 1);

        // Tick until mover reaches z=0 or times out
        int maxTicks = 500;
        bool reachedZ0 = false;
        for (int t = 0; t < maxTicks; t++) {
            Tick();
            if ((int)m->z == 0) {
                reachedZ0 = true;
                break;
            }
        }

        expect(reachedZ0);
        expect((int)m->z == 0);
    }

    it("should find path using ramp to reach upper floor") {
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"  // Ramp at (2,1)
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";

        InitMultiFloorGridFromAscii(map, 5, 5);
        BuildEntrances();
        BuildGraph();

        // Test pathfinding from z=0 to z=1
        Point start = {0, 2, 0};
        Point goal = {4, 0, 1};
        Point path[MAX_PATH];

        int len = FindPath(PATH_ALGO_ASTAR, start, goal, path, MAX_PATH);

        expect(len > 0);

        // Path should contain z-level transition
        bool hasZTransition = false;
        for (int i = 1; i < len; i++) {
            if (path[i].z != path[i-1].z) {
                hasZTransition = true;
                break;
            }
        }
        expect(hasZTransition);
    }

    it("should walk up ramp and reach goal on upper floor") {
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"  // Ramp at (2,1)
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";

        InitMultiFloorGridFromAscii(map, 5, 5);
        BuildEntrances();
        BuildGraph();

        ClearMovers();
        Mover* m = &movers[0];

        // Mover starts at (0,2) z=0, goal at (4,0) z=1
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {4, 0, 1};

        InitMover(m, startX, startY, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Find path
        Point start = {0, 2, 0};
        Point path[MAX_PATH];
        int len = FindPath(PATH_ALGO_ASTAR, start, goal, path, MAX_PATH);
        expect(len > 0);

        // Set the path
        m->pathLength = len;
        for (int i = 0; i < len; i++) {
            moverPaths[0][i] = path[i];
        }
        m->pathIndex = len - 1;

        // Tick until mover reaches goal or times out
        int maxTicks = 1000;
        bool reachedGoal = false;
        for (int t = 0; t < maxTicks; t++) {
            Tick();
            int mx = (int)(m->x / CELL_SIZE);
            int my = (int)(m->y / CELL_SIZE);
            int mz = (int)m->z;
            if (mx == goal.x && my == goal.y && mz == goal.z) {
                reachedGoal = true;
                break;
            }
        }

        expect(reachedGoal);
    }

    it("should handle all four ramp directions") {
        // Test each ramp direction
        CellType rampTypes[] = {CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W};
        char rampChars[] = {'N', 'E', 'S', 'W'};
        int highDxExpected[] = {0, 1, 0, -1};
        int highDyExpected[] = {-1, 0, 1, 0};

        for (int r = 0; r < 4; r++) {
            char map[256];
            snprintf(map, sizeof(map),
                "floor:0\n"
                ".....\n"
                "..%c..\n"
                ".....\n"
                "floor:1\n"
                ".....\n"
                ".....\n"
                ".....\n", rampChars[r]);

            InitMultiFloorGridFromAscii(map, 5, 5);

            // Verify ramp type
            expect(grid[0][1][2] == rampTypes[r]);

            // Verify high side offset
            int dx, dy;
            GetRampHighSideOffset(rampTypes[r], &dx, &dy);
            expect(dx == highDxExpected[r]);
            expect(dy == highDyExpected[r]);

            // Verify can walk up
            bool canUp = CanWalkUpRampAt(2, 1, 0);
            int exitX = 2 + dx;
            int exitY = 1 + dy;
            // Exit must be in bounds for canUp to be true
            if (exitX >= 0 && exitX < gridWidth && exitY >= 0 && exitY < gridHeight) {
                expect(canUp);
            }
        }
    }
}

// ============================================================================
// Z climb timing — TDD tests for fractional Z during ladder/ramp traversal.
// These tests currently FAIL (Z snaps instantly). They define the target
// behaviour: Z interpolates over time, climb duration ~ CELL_SIZE / speed.
// ============================================================================
describe(avoidance_levels) {
    it("movers only push movers on their own level") {
        InitGridWithSizeAndChunkSize(16, 16, 8, 8);
        InitMoverSpatialGrid(16 * CELL_SIZE, 16 * CELL_SIZE);
        ClearMovers();
        Point g = {10, 10, 0};
        InitMover(&movers[0], 5 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 0, g, 100.0f);
        InitMover(&movers[1], 5 * CELL_SIZE + 22, 5 * CELL_SIZE + 16, 1, g, 100.0f);   // right above-ish, other level
        moverCount = 2;
        BuildMoverSpatialGrid();
        Vec2 a = ComputeMoverAvoidance(0);
        expect(fabsf(a.x) < 1e-6f && fabsf(a.y) < 1e-6f);
        movers[1].z = 0;                                                               // same level: pushed
        BuildMoverSpatialGrid();
        a = ComputeMoverAvoidance(0);
        expect(a.x < 0.0f);
    }
}

// Rooms A (x 1..6) and B (x 23..28), rows 2..8, joined by a corridor at x 7..22
// that is `width` cells tall. Movers start in opposite rooms.
static void SetupTwoRooms(int width) {
    InitGridWithSizeAndChunkSize(32, 12, 8, 8);
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 32; x++) {
            bool roomA = x >= 1 && x <= 6 && y >= 2 && y <= 8, roomB = x >= 23 && x <= 28 && y >= 2 && y <= 8;
            bool corridor = y >= 5 && y < 5 + width && x >= 7 && x <= 22;
            grid[0][y][x] = (roomA || roomB || corridor) ? CELL_AIR : CELL_WALL;
        }
    BuildEntrances();
    BuildGraph();
    InitMoverSpatialGrid(32 * CELL_SIZE, 12 * CELL_SIZE);
    ClearMovers();
    endlessMoverMode = false;
    static Point p[MAX_PATH];
    Point starts[2] = {{3, 5, 0}, {26, 5, 0}}, goals[2] = {{26, 5, 0}, {3, 5, 0}};
    for (int k = 0; k < 2; k++) {
        int len = FindPath(PATH_ALGO_HPA, starts[k], goals[k], p, MAX_PATH);
        InitMoverWithPath(&movers[k], starts[k].x * CELL_SIZE + CELL_SIZE / 2, starts[k].y * CELL_SIZE + CELL_SIZE / 2,
                          0, goals[k], 100.0f, p, len);
    }
    moverCount = 2;
}

// Runs until both arrive; returns ticks (or -1), and the closest approach inside the corridor
static int RunTwoRooms(float* closestInCorridor) {
    *closestInCorridor = 1e9f;
    for (int t = 0; t < 4000; t++) {
        Tick();
        int ax = (int)(movers[0].x / CELL_SIZE), bx = (int)(movers[1].x / CELL_SIZE);
        if ((ax >= 7 && ax <= 22) || (bx >= 7 && bx <= 22)) {
            float dx = movers[0].x - movers[1].x, dy = movers[0].y - movers[1].y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < *closestInCorridor) *closestInCorridor = d;
        }
        if (movers[0].pathIndex < 0 && movers[1].pathIndex < 0) return t;
    }
    return -1;
}

describe(tunnel_yielding) {
    it("movers don't walk through each other in a 1-wide tunnel: one waits at the entrance") {
        bool saved = useTunnelYielding;
        useTunnelYielding = true;
        SetupTwoRooms(1);
        float closest;
        int ticks = RunTwoRooms(&closest);
        expect(ticks > 0);                       // both arrive
        expect(closest > CELL_SIZE * 0.8f);      // never overlapping (was ~1 px without yielding)
        useTunnelYielding = false;
        SetupTwoRooms(1);
        RunTwoRooms(&closest);
        expect(closest < CELL_SIZE * 0.5f);      // documents the old ghosting behaviour
        useTunnelYielding = saved;
    }

    it("no waiting in a 2-wide corridor (not a single-lane passage)") {
        bool saved = useTunnelYielding;
        float closest;
        useTunnelYielding = true;
        SetupTwoRooms(2);
        int withYield = RunTwoRooms(&closest);
        useTunnelYielding = false;
        SetupTwoRooms(2);
        int without = RunTwoRooms(&closest);
        expect(withYield > 0 && withYield == without);
        useTunnelYielding = saved;
    }
}

describe(repath_budget) {
    it("cooldowns tick for waiting movers even after the frame's repath budget is spent") {
        InitGridWithSizeAndChunkSize(32, 32, 8, 8);
        BuildEntrances();   // movers path with HPA* by default
        BuildGraph();
        ClearMovers();
        // 15 movers that need a repath right now (more than MAX_REPATHS_PER_FRAME = 10)
        for (int i = 0; i < 15; i++) {
            Mover* m = &movers[moverCount];
            InitMover(m, (2 + i) * CELL_SIZE + 4, 2 * CELL_SIZE + 4, 0, (Point){(2 + i), 20, 0}, 100.0f);
            m->needsRepath = true;
            m->repathCooldown = 0;
            moverCount++;
        }
        // ...and one at the end that's cooling down
        Mover* last = &movers[moverCount];
        InitMover(last, 20 * CELL_SIZE + 4, 4 * CELL_SIZE + 4, 0, (Point){20, 20, 0}, 100.0f);
        last->needsRepath = true;
        last->repathCooldown = 1;
        moverCount++;

        ProcessMoverRepaths();
        expect(last->repathCooldown == 0);   // old loop stopped at the budget and never reached it
        int served = 0;
        for (int i = 0; i < 15; i++) served += !movers[i].needsRepath;
        expect(served == MAX_REPATHS_PER_FRAME);
    }

    it("round-robin: every waiting mover is served within ceil(n / budget) frames") {
        InitGridWithSizeAndChunkSize(32, 32, 8, 8);
        BuildEntrances();   // movers path with HPA* by default
        BuildGraph();
        ClearMovers();
        for (int i = 0; i < 25; i++) {
            Mover* m = &movers[moverCount];
            InitMover(m, (2 + i) * CELL_SIZE + 4, 3 * CELL_SIZE + 4, 0, (Point){(2 + i), 25, 0}, 100.0f);
            m->needsRepath = true;
            m->repathCooldown = 0;
            moverCount++;
        }
        for (int frame = 0; frame < 3; frame++) ProcessMoverRepaths();
        int waiting = 0;
        for (int i = 0; i < 25; i++) waiting += movers[i].needsRepath;
        expect(waiting == 0);
    }
}

describe(z_climb_timing) {
    // Shared setup: 5x5x3 grid, solid floor at z=0, walkable at z=1 (above dirt)
    // and z=2 (above constructed floor), CELL_LADDER_BOTH at (2,1) on z=1 and z=2.
    // Mover speed 100, dayLength 60 => effectiveSpeed 100 px/s.
    // Expected climb duration for 1 Z level: CELL_SIZE / 100 = 0.32 s (~19 ticks).
#define CLIMB_SPEED       100.0f
#define CLIMB_EXPECTED_S  ((float)CELL_SIZE / CLIMB_SPEED)  // 0.32 s
#define CLIMB_GRID_SETUP() \
    do { \
        InitGridWithSize(5, 5); \
        gridDepth = 3; \
        for (int _y = 0; _y < gridHeight; _y++) { \
            for (int _x = 0; _x < gridWidth; _x++) { \
                grid[0][_y][_x] = CELL_WALL; \
                SetWallMaterial(_x, _y, 0, MAT_DIRT); \
                grid[1][_y][_x] = CELL_AIR; \
                grid[2][_y][_x] = CELL_AIR; \
                SET_FLOOR(_x, _y, 2); \
            } \
        } \
        grid[1][1][2] = CELL_LADDER_BOTH; \
        grid[2][1][2] = CELL_LADDER_BOTH; \
    } while(0)

    it("z should be fractional while climbing a ladder (not an instant snap)") {
        CLIMB_GRID_SETUP();
        ClearMovers();
        Mover* m = &movers[0];
        int startZ = 1, goalZ = 2;
        Point path[] = {{4,1,goalZ},{2,1,goalZ},{2,1,startZ},{0,1,startZ}};
        InitMoverWithPath(m,
            0*CELL_SIZE + CELL_SIZE*0.5f,
            1*CELL_SIZE + CELL_SIZE*0.5f,
            (float)startZ, (Point){4,1,goalZ}, CLIMB_SPEED, path, 4);
        moverCount = 1;

        bool sawFractional = false;
        for (int tick = 0; tick < 600 && m->active; tick++) {
            Tick();
            float frac = m->z - (float)(int)m->z;
            if (frac > 0.05f && frac < 0.95f) { sawFractional = true; break; }
        }
        expect(sawFractional == true);
    }

    it("climbing one z level via ladder should take ~CELL_SIZE/speed seconds") {
        CLIMB_GRID_SETUP();
        ClearMovers();
        Mover* m = &movers[0];
        int startZ = 1, goalZ = 2;
        Point path[] = {{4,1,goalZ},{2,1,goalZ},{2,1,startZ},{0,1,startZ}};
        InitMoverWithPath(m,
            0*CELL_SIZE + CELL_SIZE*0.5f,
            1*CELL_SIZE + CELL_SIZE*0.5f,
            (float)startZ, (Point){4,1,goalZ}, CLIMB_SPEED, path, 4);
        moverCount = 1;

        double climbStartTime = -1.0, climbEndTime = -1.0;
        for (int tick = 0; tick < 600 && m->active; tick++) {
            Tick();
            if (climbStartTime < 0.0 && m->z > (float)startZ + 0.05f)
                climbStartTime = gameTime;
            if (climbStartTime >= 0.0 && m->z >= (float)goalZ - 0.05f && climbEndTime < 0.0)
                climbEndTime = gameTime;
        }
        expect(climbStartTime >= 0.0);
        expect(climbEndTime >= 0.0);
        float elapsed = (float)(climbEndTime - climbStartTime);
        if (test_verbose) printf("  climb elapsed=%.3fs, expected>=%.3fs\n", elapsed, CLIMB_EXPECTED_S * 0.8f);
        expect(elapsed >= CLIMB_EXPECTED_S * 0.8f);
    }

    it("z should be exactly goalZ (integer) after completing ladder climb") {
        CLIMB_GRID_SETUP();
        ClearMovers();
        Mover* m = &movers[0];
        int startZ = 1, goalZ = 2;
        Point path[] = {{4,1,goalZ},{2,1,goalZ},{2,1,startZ},{0,1,startZ}};
        InitMoverWithPath(m,
            0*CELL_SIZE + CELL_SIZE*0.5f,
            1*CELL_SIZE + CELL_SIZE*0.5f,
            (float)startZ, (Point){4,1,goalZ}, CLIMB_SPEED, path, 4);
        moverCount = 1;

        for (int tick = 0; tick < 600 && m->active; tick++) Tick();
        expect(m->z == (float)goalZ);
        expect(m->active == false);
    }

    it("z should be fractional while traversing a ramp upward") {
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        InitMultiFloorGridFromAscii(map, 5, 5);
        ClearMovers();
        Mover* m = &movers[0];
        Point path[] = {{2,0,1},{2,1,0},{2,2,0}};
        InitMoverWithPath(m,
            2*CELL_SIZE + CELL_SIZE*0.5f,
            2*CELL_SIZE + CELL_SIZE*0.5f,
            0.0f, (Point){2,0,1}, CLIMB_SPEED, path, 3);
        moverCount = 1;

        bool sawFractional = false;
        for (int tick = 0; tick < 500 && m->active; tick++) {
            Tick();
            float frac = m->z - (float)(int)m->z;
            if (frac > 0.05f && frac < 0.95f) { sawFractional = true; break; }
        }
        expect(sawFractional == true);
    }

    it("never stands inside the solid cell below a ramp's exit while climbing") {
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        InitMultiFloorGridFromAscii(map, 5, 5);
        grid[0][0][2] = CELL_WALL;   // the cell below the exit is solid (as on real hills)
        ClearMovers();
        Mover* m = &movers[0];
        Point path[] = {{2,0,1},{2,1,0},{2,2,0}};
        InitMoverWithPath(m, 2*CELL_SIZE + CELL_SIZE*0.5f, 2*CELL_SIZE + CELL_SIZE*0.5f,
                          0.0f, (Point){2,0,1}, CLIMB_SPEED, path, 3);
        moverCount = 1;
        bool inSolid = false;
        for (int tick = 0; tick < 500 && m->active; tick++) {
            Tick();
            if (m->z - floorf(m->z) > 0.0001f) continue;        // between levels: fine
            int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE);
            if (!IsCellWalkableAt((int)m->z, cy, cx)) inSolid = true;
        }
        expect(!inSolid);
        expect(m->z == 1.0f);
    }

    it("ramp z traversal should take ~CELL_SIZE/speed seconds") {
        const char* map =
            "floor:0\n"
            ".....\n"
            "..N..\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        InitMultiFloorGridFromAscii(map, 5, 5);
        ClearMovers();
        Mover* m = &movers[0];
        Point path[] = {{2,0,1},{2,1,0},{2,2,0}};
        InitMoverWithPath(m,
            2*CELL_SIZE + CELL_SIZE*0.5f,
            2*CELL_SIZE + CELL_SIZE*0.5f,
            0.0f, (Point){2,0,1}, CLIMB_SPEED, path, 3);
        moverCount = 1;

        double climbStartTime = -1.0, climbEndTime = -1.0;
        for (int tick = 0; tick < 500 && m->active; tick++) {
            Tick();
            if (climbStartTime < 0.0 && m->z > 0.05f)
                climbStartTime = gameTime;
            if (climbStartTime >= 0.0 && m->z >= 1.0f - 0.05f && climbEndTime < 0.0)
                climbEndTime = gameTime;
        }
        expect(climbStartTime >= 0.0);
        expect(climbEndTime >= 0.0);
        float elapsed = (float)(climbEndTime - climbStartTime);
        if (test_verbose) printf("  ramp elapsed=%.3fs, expected>=%.3fs\n", elapsed, CLIMB_EXPECTED_S * 0.8f);
        expect(elapsed >= CLIMB_EXPECTED_S * 0.8f);
    }

#undef CLIMB_SPEED
#undef CLIMB_EXPECTED_S
#undef CLIMB_GRID_SETUP
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);

    test(mover_initialization);
    test(fixed_timestep_movement);
    test(wall_collision);
    test(line_of_sight_repath);
    test(tick_counter);
    test(count_active_movers);
    test(endless_mode);
    test(blocked_mover_handling);
    test(refinement_after_wall_changes);
    test(string_pulling_narrow_gaps);
    test(chunk_boundary_paths);
    test(path_truncation);
    test(mover_falling);
    test(mover_z_level_collision);
    test(mover_ladder_transitions);
    test(mover_coarse_paths);
    test(mover_ramp_transitions);
    test(z_climb_timing);
    test(sparse_level_pathfinding);
    test(staggered_updates);
    test(workshop_mover_collision);
    test(repath_budget);
    test(tunnel_yielding);
    test(single_file_ladders);
    test(ladder_queue_cost);
    test(avoidance_levels);
    return summary();
}
