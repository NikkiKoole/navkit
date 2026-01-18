#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../pathing/grid.h"
#include "../pathing/pathfinding.h"
#include "../pathing/mover.h"
#include <stdlib.h>

describe(mover_initialization) {
    it("should initialize mover with correct position and goal") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {7, 3};
        InitMover(m, 16.0f, 16.0f, goal, 100.0f);
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
        Point goal = {4, 0};
        Point testPath[] = {{4, 0}, {3, 0}, {2, 0}, {1, 0}, {0, 0}};
        InitMoverWithPath(m, 16.0f, 16.0f, goal, 100.0f, testPath, 5);
        moverCount = 1;
        
        expect(m->pathLength == 5);
        expect(m->pathIndex == 4);  // Points to last element (start)
        expect(m->path[0].x == 4 && m->path[0].y == 0);  // Goal
    }
}

describe(fixed_timestep_movement) {
    it("should move mover toward goal after one tick") {
        // 8x4 grid, mover walks right
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        ClearMovers();
        Mover* m = &movers[0];
        
        // Path from (0,0) to (4,0), stored as {current, target}
        Point goal = {4, 0};
        Point testPath[] = {{0, 0}, {4, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        float initialX = m->x;
        
        // Run 1 tick
        Tick();
        
        // Mover should have moved right (toward goal at x=4)
        expect(m->x > initialX);
    }
    
    it("should produce same result for same number of ticks") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        // First run
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {4, 0};
        Point testPath[] = {{0, 0}, {4, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        RunTicks(60);  // 1 second of simulation
        float firstRunX = m->x;
        float firstRunY = m->y;
        
        // Second run - same setup
        ClearMovers();
        m = &movers[0];
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        RunTicks(60);
        float secondRunX = m->x;
        float secondRunY = m->y;
        
        // Should be identical (combine into single expect)
        expect(firstRunX == secondRunX && firstRunY == secondRunY);
    }
    
    it("should deactivate mover when reaching goal") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n"
            "....\n", 4, 4);
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {1, 0};
        Point testPath[] = {{0, 0}, {1, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        // Run enough ticks to reach goal (1 cell = 32 pixels, speed = 100 px/s)
        // At 60 ticks/sec, 1 tick = ~1.67 pixels. Need ~19 ticks for 1 cell.
        RunTicks(60);  // Should be more than enough
        
        expect(m->active == false);
    }
}

describe(wall_collision) {
    it("should push mover out when wall placed on it") {
        // Mover at (1,1), we place a wall there
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n"
            "....\n", 4, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {3, 3};
        Point testPath[] = {{1, 1}, {2, 2}, {3, 3}};
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 3);
        moverCount = 1;
        
        // Place wall on mover's current cell
        grid[1][1] = CELL_WALL;
        MarkChunkDirty(1, 1);
        
        // Run tick - mover should be pushed to adjacent walkable cell
        Tick();
        
        // Mover should have moved to a neighbor cell and need repath
        int cellX = (int)(m->x / CELL_SIZE);
        int cellY = (int)(m->y / CELL_SIZE);
        expect(!(cellX == 1 && cellY == 1) && m->needsRepath == true);
    }
    
    it("should deactivate mover when fully surrounded by walls") {
        InitGridFromAsciiWithChunkSize(
            ".#..\n"
            "#.#.\n"
            ".#..\n"
            "....\n", 4, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {3, 3};
        Point testPath[] = {{1, 1}, {3, 3}};
        float startX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        // Place wall on mover's cell - now fully surrounded
        grid[1][1] = CELL_WALL;
        
        Tick();
        
        // Mover should be deactivated (no escape)
        expect(m->active == false);
    }
}

describe(line_of_sight_repath) {
    it("should trigger repath when wall blocks path to next waypoint") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        BuildEntrances();
        BuildGraph();
        
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {6, 0};
        // Path stored goal-to-start: mover walks from pathIndex down to 0
        // pathIndex starts at pathLength-1, so mover walks toward path[pathLength-1] first
        // Actually path[pathIndex] is the next waypoint to reach
        // With pathIndex=1 (last elem), target is path[1]={6,0}
        Point testPath[] = {{0, 0}, {6, 0}};  // path[0]=start(current), path[1]=goal(target)
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        // Place wall between start and goal
        grid[0][3] = CELL_WALL;
        MarkChunkDirty(3, 0);
        
        Tick();
        
        // Mover should need repath (wall blocks line of sight to path[1]={6,0})
        expect(m->needsRepath == true);
    }
}

describe(tick_counter) {
    it("should increment tick counter each tick") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        ClearMovers();
        unsigned long startTick = currentTick;
        
        RunTicks(100);
        
        expect(currentTick == startTick + 100);
    }
    
    it("should reset tick counter on ClearMovers") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        RunTicks(50);
        ClearMovers();
        
        expect(currentTick == 0);
    }
}

describe(count_active_movers) {
    it("should count only active movers") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n"
            "....\n", 4, 4);
        
        ClearMovers();
        
        // Add 3 movers
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[moverCount];
            Point goal = {3, 0};
            Point testPath[] = {{3, 0}, {0, 0}};
            InitMoverWithPath(m, 16.0f, 16.0f, goal, 100.0f, testPath, 2);
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
        Point goal = {1, 0};
        Point testPath[] = {{0, 0}, {1, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 2);
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
        Point goal = {2, 0};
        Point testPath[] = {{0, 0}, {1, 0}, {2, 0}};
        float startX = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        float startY = 0 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMoverWithPath(m, startX, startY, goal, 100.0f, testPath, 3);
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
        Point goal = {1, 0};
        Point testPath[] = {{0, 0}, {1, 0}};
        InitMoverWithPath(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, goal, 100.0f, testPath, 2);
        moverCount = 1;
        
        RunTicks(300);
        float firstX = m->x;
        float firstY = m->y;
        Point firstGoal = m->goal;
        
        // Second run - same seed
        SeedRandom(99999);
        ClearMovers();
        m = &movers[0];
        InitMoverWithPath(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, goal, 100.0f, testPath, 2);
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

describe(refinement_after_wall_changes) {
    it("should handle movers on map with scattered walls and small chunks") {
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
                InitMoverWithPath(m, x, y, goal, 100.0f, path, pathLength);
            } else {
                InitMover(m, x, y, goal, 100.0f);
            }
            moverCount++;
        }
        
        // Run simulation for 10 seconds
        RunTicks(600);
        
        // Count stuck movers (active but no path and not on cooldown)
        int stuckMovers = 0;
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (m->active && m->pathLength == 0 && m->repathCooldown == 0) {
                stuckMovers++;
            }
        }
        
        // All movers should either have a path or be on cooldown waiting for retry
        expect(stuckMovers == 0);
        
        endlessMoverMode = false;
    }
    
    it("should handle walls drawn while movers are active") {
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
                InitMoverWithPath(m, x, y, goal, 100.0f, path, pathLength);
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
                grid[wy][wx] = CELL_WALL;
                MarkChunkDirty(wx, wy);
            }
            // Run a few ticks between each wall (simulates drawing at ~60fps)
            RunTicks(2);
        }
        
        // Run more ticks - this triggers repaths
        RunTicks(300);
        
        // Count stuck movers (active but no path and not on cooldown)
        int stuckMovers = 0;
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (m->active && m->pathLength == 0 && m->repathCooldown == 0) {
                stuckMovers++;
            }
        }
        
        // Movers should either have a path or be on cooldown waiting for retry
        // (some may be deactivated if completely surrounded by walls, that's ok)
        expect(stuckMovers == 0);
        
        endlessMoverMode = false;
    }
}

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }

    test(mover_initialization);
    test(fixed_timestep_movement);
    test(wall_collision);
    test(line_of_sight_repath);
    test(tick_counter);
    test(count_active_movers);
    test(endless_mode);
    test(refinement_after_wall_changes);
    return summary();
}
