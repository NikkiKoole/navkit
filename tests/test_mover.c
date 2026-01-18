#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../pathing/grid.h"
#include "../pathing/pathfinding.h"
#include "../pathing/mover.h"
#include <stdlib.h>

// Stub for GetRandomWalkableCell (not used in deterministic tests)
Point GetRandomWalkableCell(void) {
    return (Point){0, 0};
}

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
    return summary();
}
