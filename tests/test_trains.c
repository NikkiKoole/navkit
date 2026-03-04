#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/trains.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/simulation/lighting.h"
#include "../src/core/time.h"
#include "../src/world/designations.h"
#include "test_helpers.h"
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: flat 40x40 grid at z=1 (air above solid ground)
static void SetupFlatGrid40(void) {
    InitTestGrid(40, 40);
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            grid[1][y][x] = CELL_AIR;
            SET_FLOOR(x, y, 1);
        }
    }
}

static void SetupClean(void) {
    SetupFlatGrid40();
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearJobs();
    ClearTrains();
    InitDesignations();
    InitLighting();
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    dayLength = 60.0f;
    gameTime = 0.0;
    useStringPulling = false;
    endlessMoverMode = false;
    useRandomizedCooldowns = false;
    useStaggeredUpdates = false;
}

// Helper: create a straight horizontal track from (x1,y) to (x2,y) at z
static void PlaceTrackLine(int x1, int x2, int y, int z) {
    for (int x = x1; x <= x2; x++) {
        grid[z][y][x] = CELL_TRACK;
    }
}

// Helper: place platform at (x,y,z)
static void PlacePlatform(int x, int y, int z) {
    grid[z][y][x] = CELL_PLATFORM;
}

// ============================================================================
// Station Detection
// ============================================================================

describe(station_detection) {
    it("detects station when platform adjacent to track") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        expect(stations[0].trackX == 10);
        expect(stations[0].trackY == 5);
        expect(stations[0].platX == 11);
        expect(stations[0].platY == 5);
        expect(stations[0].z == 1);
        expect(stations[0].active == true);
    }

    it("no station when platform not adjacent to track") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][12] = CELL_PLATFORM;  // 2 cells away
        RebuildStations();
        expect(stationCount == 0);
    }

    it("detects multiple stations") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        grid[1][15][20] = CELL_TRACK;
        grid[1][14][20] = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 2);
    }

    it("one station per track cell") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][4][10] = CELL_PLATFORM;  // North
        grid[1][5][11] = CELL_PLATFORM;  // East
        RebuildStations();
        expect(stationCount == 1);
    }

    it("GetStationAt finds station by track coords") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();
        int idx = GetStationAt(10, 5, 1);
        expect(idx == 0);
        int none = GetStationAt(20, 20, 1);
        expect(none == -1);
    }

    it("FindNearestStation returns closest") {
        SetupClean();
        grid[1][5][5] = CELL_TRACK;
        grid[1][5][6] = CELL_PLATFORM;
        grid[1][5][30] = CELL_TRACK;
        grid[1][5][31] = CELL_PLATFORM;
        RebuildStations();
        int near = FindNearestStation(7, 5, 1, 20);
        expect(near >= 0);
        expect(stations[near].platX == 6);
        near = FindNearestStation(32, 5, 1, 20);
        expect(near >= 0);
        expect(stations[near].platX == 31);
    }

    it("rebuild preserves waiters at existing stations") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();
        StationAddWaiter(0, 42);
        expect(stations[0].waitingCount == 1);
        RebuildStations();
        expect(stations[0].waitingCount == 1);
        expect(stations[0].waitingMovers[0] == 42);
    }
}

// ============================================================================
// WaitingSet Operations
// ============================================================================

describe(waiting_set) {
    it("add and remove waiter") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        StationAddWaiter(0, 3);
        expect(stations[0].waitingCount == 1);
        expect(stations[0].waitingMovers[0] == 3);

        StationRemoveWaiter(0, 3);
        expect(stations[0].waitingCount == 0);
    }

    it("FIFO ordering via GetNextBoarder") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        gameTime = 1.0;
        StationAddWaiter(0, 5);
        gameTime = 2.0;
        StationAddWaiter(0, 8);
        gameTime = 3.0;
        StationAddWaiter(0, 2);

        int first = StationGetNextBoarder(0);
        expect(first == 5);
    }

    it("no duplicate waiters") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        StationAddWaiter(0, 3);
        StationAddWaiter(0, 3);
        expect(stations[0].waitingCount == 1);
    }

    it("remove nonexistent waiter is no-op") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        StationAddWaiter(0, 3);
        StationRemoveWaiter(0, 99);
        expect(stations[0].waitingCount == 1);
    }
}

// ============================================================================
// Train Stops at Station
// ============================================================================

describe(train_stopping) {
    it("train stops at station with waiters") {
        SetupClean();
        PlaceTrackLine(5, 15, 10, 1);
        PlacePlatform(10, 11, 1);
        RebuildStations();
        expect(stationCount == 1);

        StationAddWaiter(0, 0);

        int trainIdx = SpawnTrain(5, 10, 1);
        expect(trainIdx >= 0);
        Train* t = &trains[trainIdx];
        expect(t->cartState == CART_MOVING);

        for (int tick = 0; tick < 1000; tick++) {
            TrainsTick(TICK_DT);
            if (t->cartState == CART_DOORS_OPEN) break;
        }
        expect(t->cartState == CART_DOORS_OPEN);
        expect(t->atStation == 0);
    }

    it("train resumes after door time") {
        SetupClean();
        PlaceTrackLine(5, 15, 10, 1);
        PlacePlatform(10, 11, 1);
        RebuildStations();

        StationAddWaiter(0, 0);

        int trainIdx = SpawnTrain(5, 10, 1);
        Train* t = &trains[trainIdx];

        for (int tick = 0; tick < 1000; tick++) {
            TrainsTick(TICK_DT);
            if (t->cartState == CART_DOORS_OPEN) break;
        }
        expect(t->cartState == CART_DOORS_OPEN);

        StationRemoveWaiter(0, 0);

        float ticksForDoor = (TRAIN_DOOR_TIME / (TICK_DT * gameSpeed)) + 10;
        for (int tick = 0; tick < (int)ticksForDoor; tick++) {
            TrainsTick(TICK_DT);
            if (t->cartState == CART_MOVING) break;
        }
        expect(t->cartState == CART_MOVING);
        expect(t->atStation == -1);
    }

    it("train does not stop at station with no waiters or riders") {
        SetupClean();
        PlaceTrackLine(5, 15, 10, 1);
        PlacePlatform(10, 11, 1);
        RebuildStations();

        int trainIdx = SpawnTrain(5, 10, 1);
        Train* t = &trains[trainIdx];

        for (int tick = 0; tick < 1000; tick++) {
            TrainsTick(TICK_DT);
        }
        expect(t->cartState == CART_MOVING);
    }
}

// ============================================================================
// ShouldUseTrain
// ============================================================================

describe(should_use_train) {
    it("returns false when no stations") {
        SetupClean();
        moverCount = 1;
        InitMover(&movers[0], 5 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){35, 35, 1}, MOVER_SPEED);
        expect(ShouldUseTrain(0) == false);
    }

    it("returns false when goal too close") {
        SetupClean();
        grid[1][5][5] = CELL_TRACK; grid[1][5][6] = CELL_PLATFORM;
        grid[1][5][15] = CELL_TRACK; grid[1][5][16] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 7 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){15, 5, 1}, MOVER_SPEED);
        expect(ShouldUseTrain(0) == false);
    }

    it("returns true when goal is far with stations near both ends") {
        SetupClean();
        // Station A near mover start at (5,5), platform at (6,5)
        grid[1][5][5] = CELL_TRACK; grid[1][5][6] = CELL_PLATFORM;
        // Station B near goal at (35,5), platform at (36,5)
        grid[1][5][35] = CELL_TRACK; grid[1][5][36] = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 2);

        moverCount = 1;
        // Mover at (7,5), goal at (37,5) — manhattan = 30 < 40, too close
        // Goal at (37,25) — manhattan = 30+20 = 50 > 40, station B platform (36,5) to goal (37,25) = 1+20 = 21 > 20
        // Need goal within TRANSPORT_STATION_RADIUS of exit station's platform
        // Goal at (37,10) — manhattan from mover = 30+5 = 35 < 40, still too close
        // Goal at (38,30) — manhattan = 31+25 = 56 > 40, dist to platform (36,5) = 2+25 = 27 > 20
        // Use closer stations: both on same row, far apart
        // Actually let's just put goal near station B: (37,5) — dist to plat(36,5) = 1, manhattan from mover = 30 < 40
        // Need more distance: mover at (3,3), goal at (38,3)
        InitMover(&movers[0], 3 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){38, 5, 1}, MOVER_SPEED);
        // Manhattan from (3,5) to (38,5) = 35 < 40... still not enough
        // Let's make it 45+ manhattan: mover at (3,5), goal at (38,15)
        movers[0].goal = (Point){38, 15, 1};
        // Manhattan = 35 + 10 = 45 > 40
        // Entry station: nearest to (3,5) is plat(6,5), dist = 3 < 20 ✓
        // Exit station: nearest to (38,15) is plat(36,5), dist = 2+10 = 12 < 20 ✓
        expect(ShouldUseTrain(0) == true);
    }

    it("returns false for cross-z goals") {
        SetupClean();
        grid[1][5][5] = CELL_TRACK; grid[1][5][6] = CELL_PLATFORM;
        grid[1][5][35] = CELL_TRACK; grid[1][5][36] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 7 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){37, 35, 2}, MOVER_SPEED);
        expect(ShouldUseTrain(0) == false);
    }
}

// ============================================================================
// Boarding and Exiting
// ============================================================================

describe(boarding_exiting) {
    it("BoardMoverOnTrain removes from waiting, adds to riding") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WAITING;
        movers[0].transportStation = 0;

        int trainIdx = SpawnTrain(10, 5, 1);
        StationAddWaiter(0, 0);
        expect(stations[0].waitingCount == 1);

        BoardMoverOnTrain(trainIdx, 0, 0);
        expect(stations[0].waitingCount == 0);
        expect(trains[trainIdx].ridingCount == 1);
        expect(trains[trainIdx].ridingMovers[0] == 0);
        expect(movers[0].transportState == TRANSPORT_RIDING);
        expect(movers[0].transportTrainIdx == trainIdx);
    }

    it("ExitMoverFromTrain places mover at platform, restores goal") {
        SetupClean();
        grid[1][15][20] = CELL_TRACK;
        grid[1][15][21] = CELL_PLATFORM;
        RebuildStations();
        int exitSt = GetStationAt(20, 15, 1);
        expect(exitSt >= 0);

        moverCount = 1;
        InitMover(&movers[0], 10 * CELL_SIZE + 16, 10 * CELL_SIZE + 16, 1.0f,
                   (Point){25, 25, 1}, MOVER_SPEED);
        movers[0].transportFinalGoal = (Point){25, 25, 1};

        int trainIdx = SpawnTrain(20, 15, 1);
        trains[trainIdx].ridingMovers[0] = 0;
        trains[trainIdx].ridingCount = 1;
        movers[0].transportState = TRANSPORT_RIDING;
        movers[0].transportTrainIdx = trainIdx;
        movers[0].transportExitStation = exitSt;

        ExitMoverFromTrain(trainIdx, 0, exitSt);

        expect(trains[trainIdx].ridingCount == 0);
        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(movers[0].goal.x == 25);
        expect(movers[0].goal.y == 25);
        expect(movers[0].needsRepath == true);
        float expectedX = 21 * CELL_SIZE + CELL_SIZE * 0.5f;
        float expectedY = 15 * CELL_SIZE + CELL_SIZE * 0.5f;
        expect(fabsf(movers[0].x - expectedX) < 1.0f);
        expect(fabsf(movers[0].y - expectedY) < 1.0f);
    }

    it("DismountAllRiders removes all riders") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 3;
        for (int i = 0; i < 3; i++) {
            InitMover(&movers[i], 10 * CELL_SIZE, 10 * CELL_SIZE, 1.0f,
                       (Point){30, 30, 1}, MOVER_SPEED);
            movers[i].transportState = TRANSPORT_RIDING;
            movers[i].transportFinalGoal = (Point){30, 30, 1};
        }

        int trainIdx = SpawnTrain(10, 5, 1);
        trains[trainIdx].ridingMovers[0] = 0;
        trains[trainIdx].ridingMovers[1] = 1;
        trains[trainIdx].ridingMovers[2] = 2;
        trains[trainIdx].ridingCount = 3;

        DismountAllRiders(trainIdx);
        expect(trains[trainIdx].ridingCount == 0);
        for (int i = 0; i < 3; i++) {
            expect(movers[i].transportState == TRANSPORT_NONE);
            expect(movers[i].needsRepath == true);
        }
    }

    it("train boards waiters during doors-open") {
        SetupClean();
        PlaceTrackLine(5, 15, 10, 1);
        PlacePlatform(10, 11, 1);
        RebuildStations();

        moverCount = 1;
        float platCX = 11 * CELL_SIZE + CELL_SIZE * 0.5f;
        float platCY = 10 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(&movers[0], platCX, platCY, 1.0f, (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WAITING;
        movers[0].transportStation = 0;
        movers[0].transportExitStation = -1;
        movers[0].transportFinalGoal = (Point){30, 30, 1};
        StationAddWaiter(0, 0);

        int trainIdx = SpawnTrain(5, 10, 1);

        // Tick until doors open, then tick once more for boarding to happen
        for (int tick = 0; tick < 1000; tick++) {
            TrainsTick(TICK_DT);
            if (trains[trainIdx].cartState == CART_DOORS_OPEN) break;
        }
        expect(trains[trainIdx].cartState == CART_DOORS_OPEN);
        // One more tick for the doors-open handler to board
        TrainsTick(TICK_DT);
        expect(trains[trainIdx].ridingCount == 1);
        expect(stations[0].waitingCount == 0);
        expect(movers[0].transportState == TRANSPORT_RIDING);
    }

    it("train exits riders at their exit station") {
        SetupClean();
        PlaceTrackLine(5, 15, 10, 1);
        PlacePlatform(5, 11, 1);
        PlacePlatform(14, 11, 1);
        RebuildStations();
        expect(stationCount == 2);

        int stB = GetStationAt(14, 10, 1);
        expect(stB >= 0);

        moverCount = 1;
        InitMover(&movers[0], 5 * CELL_SIZE + 16, 10 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_RIDING;
        movers[0].transportExitStation = stB;
        movers[0].transportFinalGoal = (Point){30, 30, 1};

        int trainIdx = SpawnTrain(14, 10, 1);
        Train* t = &trains[trainIdx];
        t->ridingMovers[0] = 0;
        t->ridingCount = 1;
        movers[0].transportTrainIdx = trainIdx;

        t->cartState = CART_DOORS_OPEN;
        t->stateTimer = TRAIN_DOOR_TIME;
        t->atStation = stB;

        TrainsTick(TICK_DT);
        expect(t->ridingCount == 0);
        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(movers[0].goal.x == 30);
        expect(movers[0].goal.y == 30);
    }

    it("capacity: excess waiters remain") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        int trainIdx = SpawnTrain(10, 5, 1);
        Train* t = &trains[trainIdx];

        moverCount = MAX_CART_CAPACITY + 2;
        for (int i = 0; i < moverCount; i++) {
            InitMover(&movers[i], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                       (Point){30, 30, 1}, MOVER_SPEED);
            movers[i].transportState = TRANSPORT_WAITING;
            movers[i].transportStation = 0;
            movers[i].transportFinalGoal = (Point){30, 30, 1};
            StationAddWaiter(0, i);
        }

        t->cartState = CART_DOORS_OPEN;
        t->stateTimer = TRAIN_DOOR_TIME;
        t->atStation = 0;

        TrainsTick(TICK_DT);

        expect(t->ridingCount == MAX_CART_CAPACITY);
        expect(stations[0].waitingCount == 2);
    }
}

// ============================================================================
// Transport State in Movers
// ============================================================================

describe(mover_transport_state) {
    it("waiting mover not assigned jobs") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WAITING;

        RebuildIdleMoverList();
        expect(idleMoverCount == 0);
    }

    it("riding mover resets stuck timer") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 10 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_RIDING;

        UpdateMovers();
        expect(movers[0].timeWithoutProgress == 0.0f);
    }

    it("walking-to-station mover transitions to waiting on arrival") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        float platX = 11 * CELL_SIZE + CELL_SIZE * 0.5f;
        float platY = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(&movers[0], platX, platY, 1.0f, (Point){11, 5, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WALKING_TO_STATION;
        movers[0].transportStation = 0;
        movers[0].transportExitStation = -1;
        movers[0].transportFinalGoal = (Point){30, 30, 1};
        movers[0].pathLength = 0;
        movers[0].pathIndex = -1;

        UpdateMovers();

        expect(movers[0].transportState == TRANSPORT_WAITING);
        expect(stations[0].waitingCount == 1);
    }
}

// ============================================================================
// CancelJob Transport Cleanup
// ============================================================================

describe(cancel_job_transport) {
    it("cleans up waiting state on cancel") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WAITING;
        movers[0].transportStation = 0;
        movers[0].transportFinalGoal = (Point){30, 30, 1};
        movers[0].currentJobId = -1;
        StationAddWaiter(0, 0);

        CancelJob(&movers[0], 0);

        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(stations[0].waitingCount == 0);
        expect(movers[0].goal.x == 30);
    }

    it("cleans up riding state on cancel") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 10 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);

        int trainIdx = SpawnTrain(10, 5, 1);
        trains[trainIdx].ridingMovers[0] = 0;
        trains[trainIdx].ridingCount = 1;
        movers[0].transportState = TRANSPORT_RIDING;
        movers[0].transportTrainIdx = trainIdx;
        movers[0].transportFinalGoal = (Point){30, 30, 1};
        movers[0].currentJobId = -1;

        CancelJob(&movers[0], 0);

        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(trains[trainIdx].ridingCount == 0);
        expect(movers[0].goal.x == 30);
    }
}

// ============================================================================
// Platform Destroyed
// ============================================================================

describe(platform_destroyed) {
    it("station removed when platform destroyed, waiters ejected") {
        SetupClean();
        grid[1][5][10] = CELL_TRACK;
        grid[1][5][11] = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);

        moverCount = 1;
        InitMover(&movers[0], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WAITING;
        movers[0].transportStation = 0;
        movers[0].transportExitStation = -1;
        movers[0].transportFinalGoal = (Point){30, 30, 1};
        StationAddWaiter(0, 0);

        grid[1][5][11] = CELL_AIR;
        RebuildStations();

        expect(stationCount == 0);
        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(movers[0].goal.x == 30);
        expect(movers[0].needsRepath == true);
    }
}

// ============================================================================
// Track Destroyed Under Train
// ============================================================================

describe(track_destroyed) {
    it("dismounts all riders when track removed") {
        SetupClean();
        PlaceTrackLine(5, 15, 10, 1);

        moverCount = 2;
        for (int i = 0; i < 2; i++) {
            InitMover(&movers[i], 10 * CELL_SIZE, 10 * CELL_SIZE, 1.0f,
                       (Point){30, 30, 1}, MOVER_SPEED);
            movers[i].transportState = TRANSPORT_RIDING;
            movers[i].transportFinalGoal = (Point){30, 30, 1};
        }

        int trainIdx = SpawnTrain(10, 10, 1);
        trains[trainIdx].ridingMovers[0] = 0;
        trains[trainIdx].ridingMovers[1] = 1;
        trains[trainIdx].ridingCount = 2;

        grid[1][10][10] = CELL_AIR;
        TrainsTick(TICK_DT);

        expect(trains[trainIdx].active == false);
        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(movers[1].transportState == TRANSPORT_NONE);
    }
}

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

    test(station_detection);
    test(waiting_set);
    test(train_stopping);
    test(should_use_train);
    test(boarding_exiting);
    test(mover_transport_state);
    test(cancel_job_transport);
    test(platform_destroyed);
    test(track_destroyed);

    return summary();
}
