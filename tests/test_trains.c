#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include <stdlib.h>
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../assets/atlas.h"
#include "../src/entities/mover.h"
#include "../src/entities/trains.h"
#include "../src/simulation/mood.h"
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
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

static bool test_verbose = false;

// Helper: flat 40x40 grid at z=1 (air above solid ground)
static void SetupFlatGrid40(void) {
    InitTestGrid(40, 40);
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
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
    useTrainTimeChoice = false;   // most tests here check the classic rule; time-based has its own
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
        GRID(x, y, z) = CELL_TRACK;
        TRACK_CONN(x, y, z) = TRACK_E | TRACK_W;
    }
    // End caps: remove outward connections
    TRACK_CONN(x1, y, z) &= ~TRACK_W;
    TRACK_CONN(x2, y, z) &= ~TRACK_E;
}

// Helper: place platform at (x,y,z)
static void PlacePlatform(int x, int y, int z) {
    GRID(x, y, z) = CELL_PLATFORM;
}

// ============================================================================
// Station Detection
// ============================================================================

describe(station_detection) {
    it("detects station when platform adjacent to track") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(12, 5, 1) = CELL_PLATFORM;  // 2 cells away
        RebuildStations();
        expect(stationCount == 0);
    }

    it("detects multiple stations") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        GRID(20, 15, 1) = CELL_TRACK;
        GRID(20, 14, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 2);
    }

    it("one station per track cell") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(10, 4, 1) = CELL_PLATFORM;  // North
        GRID(11, 5, 1) = CELL_PLATFORM;  // East
        RebuildStations();
        expect(stationCount == 1);
    }

    it("GetStationAt finds station by track coords") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();
        int idx = GetStationAt(10, 5, 1);
        expect(idx == 0);
        int none = GetStationAt(20, 20, 1);
        expect(none == -1);
    }

    it("FindNearestStation returns closest") {
        SetupClean();
        GRID(5, 5, 1) = CELL_TRACK;
        GRID(6, 5, 1) = CELL_PLATFORM;
        GRID(30, 5, 1) = CELL_TRACK;
        GRID(31, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        StationAddWaiter(0, 3);
        expect(stations[0].waitingCount == 1);
        expect(stations[0].waitingMovers[0] == 3);

        StationRemoveWaiter(0, 3);
        expect(stations[0].waitingCount == 0);
    }

    it("FIFO ordering via GetNextBoarder") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        StationAddWaiter(0, 3);
        StationAddWaiter(0, 3);
        expect(stations[0].waitingCount == 1);
    }

    it("remove nonexistent waiter is no-op") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(5, 5, 1) = CELL_TRACK; GRID(6, 5, 1) = CELL_PLATFORM;
        GRID(15, 5, 1) = CELL_TRACK; GRID(16, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 7 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){15, 5, 1}, MOVER_SPEED);
        expect(ShouldUseTrain(0) == false);
    }

    it("returns true when goal is far with stations near both ends") {
        SetupClean();
        // Station A near mover start at (5,5), platform at (6,5)
        GRID(5, 5, 1) = CELL_TRACK; GRID(6, 5, 1) = CELL_PLATFORM;
        // Station B near goal at (35,5), platform at (36,5)
        GRID(35, 5, 1) = CELL_TRACK; GRID(36, 5, 1) = CELL_PLATFORM;
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
        GRID(5, 5, 1) = CELL_TRACK; GRID(6, 5, 1) = CELL_PLATFORM;
        GRID(35, 5, 1) = CELL_TRACK; GRID(36, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(20, 15, 1) = CELL_TRACK;
        GRID(21, 15, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
    it("waiting mover with active job not assigned new jobs") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WAITING;
        movers[0].currentJobId = 0;  // has active job

        RebuildIdleMoverList();
        expect(idleMoverCount == 0);
    }

    it("jobless mover with stale transport state gets cleared and becomes idle") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        moverCount = 1;
        InitMover(&movers[0], 11 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){30, 30, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WALKING_TO_STATION;
        movers[0].currentJobId = -1;  // no job — stale transport

        RebuildIdleMoverList();
        expect(idleMoverCount == 1);
        expect(movers[0].transportState == TRANSPORT_NONE);
    }

    it("riding mover resets stuck timer") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
// Transport + Haul Job Integration
// ============================================================================

// Helper: 80x20 flat grid for transport tests
static void SetupWideGrid(void) {
    InitTestGrid(80, 20);
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 80; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
            SET_FLOOR(x, y, 1);
        }
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearJobs();
    ClearTrains();
    InitDesignations();
    InitLighting();
    gameMode = GAME_MODE_SANDBOX;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    dayLength = 60.0f;
    gameTime = 0.0;
    useStringPulling = false;
    endlessMoverMode = false;
    useRandomizedCooldowns = false;
    useStaggeredUpdates = false;
}

describe(transport_haul_integration) {
    // Layout (z=1, 80 wide):
    //   Station A: track(5,10), platform(5,11)   — left side
    //   Station B: track(75,10), platform(75,11)  — right side
    //   Items: at (5,5) — near station A
    //   Stockpile: at (75,5) — near station B
    //
    // Mover positions vary per test, always within TRANSPORT_STATION_RADIUS of a station.
    // Key constraint: mover and goal must each be within 20 cells of a station,
    // and manhattan distance between them must be > 40.

    it("haul step 0 (pickup walk) should not trigger transport") {
        // Mover near station B (right), item near station A (left).
        // Mover's goal is the item at (5,5). Without the fix, ShouldUseTrain
        // would redirect mover to station B, ride to station A, pick up item,
        // then need to carry it all the way back RIGHT — defeating the purpose.
        SetupWideGrid();

        GRID(5, 10, 1) = CELL_TRACK;
        GRID(6, 10, 1) = CELL_PLATFORM;  // Station A platform
        GRID(75, 10, 1) = CELL_TRACK;
        GRID(76, 10, 1) = CELL_PLATFORM; // Station B platform
        RebuildStations();
        expect(stationCount == 2);

        int itemIdx = SpawnItem(5 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f, ITEM_ROCK);
        items[itemIdx].material = MAT_GRANITE;

        CreateStockpile(75, 5, 1, 2, 2);

        // Mover near station B at (70,10), goal = item at (5,5)
        // Manhattan = 65+5 = 70 > 40, entry station = B (dist 6), exit station = A (dist 6)
        moverCount = 1;
        InitMover(&movers[0], 70 * CELL_SIZE + 16, 10 * CELL_SIZE + 16, 1.0f,
                   (Point){5, 5, 1}, MOVER_SPEED);
        movers[0].currentJobId = 0;

        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->targetItem = itemIdx;
        job->targetStockpile = 0;
        job->targetSlotX = 75;
        job->targetSlotY = 5;
        job->step = STEP_MOVING_TO_PICKUP;
        job->assignedMover = 0;
        items[itemIdx].reservedBy = 0;

        // Verify the scenario would normally trigger transport (distance and stations match)
        // but should NOT because this is the pickup leg (step 0)
        expect(ShouldUseTrain(0) == false);
    }

    it("haul step 1 (carrying to stockpile) should trigger transport") {
        // Mover near station A (left) carrying item, goal = stockpile near station B (right).
        // This IS the delivery leg — train should be used.
        SetupWideGrid();

        GRID(5, 10, 1) = CELL_TRACK;
        GRID(6, 10, 1) = CELL_PLATFORM;
        GRID(75, 10, 1) = CELL_TRACK;
        GRID(76, 10, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 2);

        int itemIdx = SpawnItem(5 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f, ITEM_ROCK);
        items[itemIdx].material = MAT_GRANITE;
        items[itemIdx].state = ITEM_CARRIED;

        CreateStockpile(75, 5, 1, 2, 2);

        // Mover near station A at (6,10), goal = stockpile at (75,5)
        // Manhattan = 69+5 = 74 > 40, entry = A (dist 0), exit = B (dist 6)
        moverCount = 1;
        InitMover(&movers[0], 6 * CELL_SIZE + 16, 10 * CELL_SIZE + 16, 1.0f,
                   (Point){75, 5, 1}, MOVER_SPEED);
        movers[0].currentJobId = 0;

        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->targetItem = itemIdx;
        job->carryingItem = itemIdx;
        job->targetStockpile = 0;
        job->targetSlotX = 75;
        job->targetSlotY = 5;
        job->step = STEP_CARRYING;
        job->assignedMover = 0;
        items[itemIdx].reservedBy = 0;

        expect(ShouldUseTrain(0) == true);
    }

    it("craft job carrying input should trigger transport") {
        // Mover carrying a crafting input to a distant workshop — train is useful.
        // This is a carrying step, not a pickup walk.
        SetupWideGrid();

        GRID(5, 10, 1) = CELL_TRACK;
        GRID(6, 10, 1) = CELL_PLATFORM;
        GRID(75, 10, 1) = CELL_TRACK;
        GRID(76, 10, 1) = CELL_PLATFORM;
        RebuildStations();

        // Mover near station A at (6,10), goal = workshop near station B at (75,10)
        moverCount = 1;
        InitMover(&movers[0], 6 * CELL_SIZE + 16, 10 * CELL_SIZE + 16, 1.0f,
                   (Point){75, 10, 1}, MOVER_SPEED);
        movers[0].currentJobId = 0;

        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->step = CRAFT_STEP_MOVING_TO_WORKSHOP;
        job->assignedMover = 0;

        expect(ShouldUseTrain(0) == true);
    }

    it("stale transport from step 0 is cleared when item is picked up") {
        // Reproduces the real bug: save file had movers already WALKING_TO_STATION
        // from a step-0 transport decision. They pick up item, transition to step 1,
        // but the stale transport (wrong finalGoal, wrong stations) persists.
        // After pickup, transport state must be cleared so ShouldUseTrain
        // can re-evaluate with the correct delivery destination.
        SetupWideGrid();

        GRID(5, 10, 1) = CELL_TRACK;
        GRID(6, 10, 1) = CELL_PLATFORM;   // Station A (left)
        GRID(75, 10, 1) = CELL_TRACK;
        GRID(76, 10, 1) = CELL_PLATFORM;  // Station B (right)
        RebuildStations();
        expect(stationCount == 2);

        // Item near station A
        int itemIdx = SpawnItem(6 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f, ITEM_ROCK);
        items[itemIdx].material = MAT_GRANITE;
        items[itemIdx].reservedBy = 0;

        // Stockpile near station B
        int spIdx = CreateStockpile(75, 5, 1, 2, 2);
        expect(spIdx >= 0);

        // Mover right next to item (will pick it up this tick)
        moverCount = 1;
        InitMover(&movers[0], 6 * CELL_SIZE + 16, 5 * CELL_SIZE + 16, 1.0f,
                   (Point){6, 5, 1}, MOVER_SPEED);
        movers[0].pathLength = 0;
        movers[0].pathIndex = -1;

        // Haul job at step 0 — mover is at the item
        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->targetItem = itemIdx;
        job->targetStockpile = spIdx;
        job->targetSlotX = 75;
        job->targetSlotY = 5;
        job->step = STEP_MOVING_TO_PICKUP;
        job->assignedMover = 0;
        movers[0].currentJobId = jobId;

        // Simulate stale transport state (as if loaded from old save)
        movers[0].transportState = TRANSPORT_WALKING_TO_STATION;
        movers[0].transportStation = 1;       // wrong station
        movers[0].transportExitStation = 0;   // wrong exit
        movers[0].transportFinalGoal = (Point){6, 5, 1}; // stale: item location, not stockpile

        // Verify item is in pickup range
        float dx = movers[0].x - items[itemIdx].x;
        float dy = movers[0].y - items[itemIdx].y;
        float distSq = dx*dx + dy*dy;
        float pickupR = CELL_SIZE * 0.75f;
        expect(distSq < pickupR * pickupR);

        // Run one tick of the haul job — mover is close enough to pick up
        JobRunResult result = RunJob_Haul(job, &movers[0], TICK_DT);
        expect(result == JOBRUN_RUNNING);
        expect(job->step == STEP_CARRYING);       // item was picked up

        // After pickup, stale transport must be cleared
        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(movers[0].transportStation == -1);
    }
}

// ============================================================================
// CancelJob Transport Cleanup
// ============================================================================

describe(cancel_job_transport) {
    it("cleans up waiting state on cancel") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
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

        GRID(11, 5, 1) = CELL_AIR;
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

        GRID(10, 10, 1) = CELL_AIR;
        TrainsTick(TICK_DT);

        expect(trains[trainIdx].active == false);
        expect(movers[0].transportState == TRANSPORT_NONE);
        expect(movers[1].transportState == TRANSPORT_NONE);
    }
}

// ============================================================================
// Multi-Cell Platform Detection
// ============================================================================

describe(multi_cell_platform) {
    it("detects 3 platform cells parallel to horizontal track") {
        SetupClean();
        // Horizontal track at y=5
        for (int x = 8; x <= 14; x++) GRID(x, 5, 1) = CELL_TRACK;
        // Platform row at y=6 (south of track), parallel: x=10,11,12
        GRID(10, 6, 1) = CELL_PLATFORM;
        GRID(11, 6, 1) = CELL_PLATFORM;
        GRID(12, 6, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        expect(stations[0].platformCellCount == 3);
        // platformCells[0] is the boarding cell (adjacent to detecting track cell)
        // All cells should be at y=6
        for (int c = 0; c < 3; c++) {
            expect(stations[0].platformCells[c][1] == 6);
        }
    }

    it("queue direction along horizontal track") {
        SetupClean();
        for (int x = 8; x <= 14; x++) GRID(x, 5, 1) = CELL_TRACK;
        GRID(10, 6, 1) = CELL_PLATFORM;
        GRID(11, 6, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        // Platform is south of track → away=(0,1), trackDir=(−1,0) or (1,0)
        // queueDir should be along x-axis (parallel to track)
        expect(stations[0].queueDirY == 0);
        expect(stations[0].queueDirX != 0);
        expect(stations[0].platformCellCount == 2);
    }

    it("single platform backward compat") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        expect(stations[0].platformCellCount == 1);
        expect(stations[0].platformCells[0][0] == 11);
        expect(stations[0].platformCells[0][1] == 5);
    }

    it("stops at non-platform cell in parallel direction") {
        SetupClean();
        for (int x = 8; x <= 14; x++) GRID(x, 5, 1) = CELL_TRACK;
        GRID(10, 6, 1) = CELL_PLATFORM;
        GRID(11, 6, 1) = CELL_AIR;  // gap stops parallel walk
        RebuildStations();
        expect(stationCount == 1);
        // Should only find 1 platform (gap breaks the line)
        expect(stations[0].platformCellCount == 1);
    }
}

// ============================================================================
// FIFO Shift-Down
// ============================================================================

describe(fifo_shift_down) {
    it("remove middle waiter preserves order") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        gameTime = 1.0;
        StationAddWaiter(0, 5);
        gameTime = 2.0;
        StationAddWaiter(0, 8);
        gameTime = 3.0;
        StationAddWaiter(0, 2);

        // Remove middle waiter (8)
        StationRemoveWaiter(0, 8);
        expect(stations[0].waitingCount == 2);
        expect(stations[0].waitingMovers[0] == 5);
        expect(stations[0].waitingMovers[1] == 2);
    }

    it("remove front waiter shifts all down") {
        SetupClean();
        GRID(10, 5, 1) = CELL_TRACK;
        GRID(11, 5, 1) = CELL_PLATFORM;
        RebuildStations();

        gameTime = 1.0;
        StationAddWaiter(0, 5);
        gameTime = 2.0;
        StationAddWaiter(0, 8);
        gameTime = 3.0;
        StationAddWaiter(0, 2);

        StationRemoveWaiter(0, 5);
        expect(stations[0].waitingCount == 2);
        expect(stations[0].waitingMovers[0] == 8);
        expect(stations[0].waitingMovers[1] == 2);
        // Next boarder should be 8 (new front)
        expect(StationGetNextBoarder(0) == 8);
    }
}

// ============================================================================
// Patience: impatient waiters give up on the train sooner, patient ones later
// ============================================================================

describe(station_patience) {
    it("gives up waiting by patience, then walks without coming straight back") {
        SetupClean();
        GRID(5, 5, 1) = CELL_TRACK; GRID(6, 5, 1) = CELL_PLATFORM;
        GRID(35, 5, 1) = CELL_TRACK; GRID(36, 5, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 2);

        const uint8_t traits[3] = {TRAIT_IMPATIENT, TRAIT_NONE, TRAIT_PATIENT};
        moverCount = 3;
        for (int i = 0; i < 3; i++) {
            InitMover(&movers[i], (7 + i) * CELL_SIZE + 16, 6 * CELL_SIZE + 16, 1.0f, (Point){38, 15, 1}, MOVER_SPEED);
            movers[i].traits[0] = traits[i];
            movers[i].traits[1] = TRAIT_NONE;
            movers[i].transportState = TRANSPORT_WAITING;
            movers[i].transportStation = 0;
            movers[i].transportFinalGoal = (Point){38, 15, 1};
            movers[i].pathLength = 0;
            movers[i].pathIndex = -1;
            StationAddWaiter(0, i);
        }
        // No train comes. Timeout 60 s x patience: impatient 24 s, normal 60, patient 120
        double start = gameTime;
        while (gameTime - start < 30.0) Tick();
        expect(movers[0].transportState != TRANSPORT_WAITING);   // impatient left
        expect(movers[1].transportState == TRANSPORT_WAITING);
        expect(movers[2].transportState == TRANSPORT_WAITING);
        // ...and doesn't head straight back to a station
        movers[0].goal = (Point){38, 15, 1};
        expect(ShouldUseTrain(0) == false);

        while (gameTime - start < 65.0) Tick();
        expect(movers[1].transportState != TRANSPORT_WAITING);   // normal left
        expect(movers[2].transportState == TRANSPORT_WAITING);   // patient still waits
    }
}

// ============================================================================
// Direction-aware trip choice (double track: one line per direction)
// ============================================================================

// Narrow clockwise loop: north line (y=10) runs east, south line (y=13) west.
// Stops at x=8 and x=30 with a platform on both outer sides. Returns station ids
// via the out params (north/south at each stop).
static void SetupDoubleTrack(int* n8, int* s8, int* n30, int* s30) {
    SetupClean();
    for (int x = 2; x <= 37; x++) {
        GRID(x, 10, 1) = CELL_TRACK; TRACK_CONN(x, 10, 1) = TRACK_E | TRACK_W;
        GRID(x, 13, 1) = CELL_TRACK; TRACK_CONN(x, 13, 1) = TRACK_E | TRACK_W;
    }
    for (int y = 10; y <= 13; y++) {
        GRID(2, y, 1) = CELL_TRACK; TRACK_CONN(2, y, 1) = TRACK_N | TRACK_S;
        GRID(37, y, 1) = CELL_TRACK; TRACK_CONN(37, y, 1) = TRACK_N | TRACK_S;
    }
    TRACK_CONN(2, 10, 1) = TRACK_E | TRACK_S;
    TRACK_CONN(37, 10, 1) = TRACK_W | TRACK_S;
    TRACK_CONN(2, 13, 1) = TRACK_E | TRACK_N;
    TRACK_CONN(37, 13, 1) = TRACK_W | TRACK_N;
    for (int x = 7; x <= 9; x++) { GRID(x, 9, 1) = CELL_PLATFORM; GRID(x, 14, 1) = CELL_PLATFORM; }
    for (int x = 29; x <= 31; x++) { GRID(x, 9, 1) = CELL_PLATFORM; GRID(x, 14, 1) = CELL_PLATFORM; }
    RebuildStations();
    *n8 = GetStationAt(8, 10, 1);
    *s8 = GetStationAt(8, 13, 1);
    *n30 = GetStationAt(30, 10, 1);
    *s30 = GetStationAt(30, 13, 1);
}

describe(train_direction_choice) {
    it("going west, the westbound side is the quicker trip (the eastbound one goes round)") {
        int n8, s8, n30, s30;
        SetupDoubleTrack(&n8, &s8, &n30, &s30);
        expect(n8 >= 0 && s8 >= 0 && n30 >= 0 && s30 >= 0);
        expect(n8 != s8 && n30 != s30);
        SpawnTrainHeading(36, 13, 1, 1, -1, 0);   // on the south line, heading west
        moverCount = 1;
        InitMover(&movers[0], 30 * CELL_SIZE + 16, 6 * CELL_SIZE + 16, 1.0f, (Point){8, 6, 1}, MOVER_SPEED);
        Point goal = {8, 6, 1};
        float westbound = TrainTripSeconds(0, s30, s8, goal);
        float eastboundRound = TrainTripSeconds(0, n30, n8, goal);
        expect(westbound < 1e8f);
        expect(eastboundRound < 1e8f);
        expect(westbound < eastboundRound);
    }

    it("doesn't take a slow train for a trip that is quicker on foot") {
        int n8, s8, n30, s30;
        SetupDoubleTrack(&n8, &s8, &n30, &s30);
        SpawnTrainHeading(36, 13, 1, 1, -1, 0);
        useTrainTimeChoice = true;
        moverCount = 1;
        InitMover(&movers[0], 30 * CELL_SIZE + 16, 6 * CELL_SIZE + 16, 1.0f, (Point){8, 6, 1}, MOVER_SPEED);
        movers[0].pathLength = 0;
        movers[0].pathIndex = -1;
        expect(ShouldUseTrain(0) == false);      // ~22 cells: walking wins against doors + wait
        useTrainTimeChoice = false;
    }

    it("a train trip a mover started for its own goal survives the idle cleanup") {
        int n8, s8, n30, s30;
        SetupDoubleTrack(&n8, &s8, &n30, &s30);
        moverCount = 1;
        InitMover(&movers[0], 30 * CELL_SIZE + 16, 6 * CELL_SIZE + 16, 1.0f, (Point){8, 6, 1}, MOVER_SPEED);
        movers[0].transportState = TRANSPORT_WALKING_TO_STATION;
        movers[0].transportStation = s30;
        movers[0].transportExitStation = s8;
        TrainsMarkPersonalTrip(0, true);
        RebuildIdleMoverList();
        expect(movers[0].transportState == TRANSPORT_WALKING_TO_STATION);
        TrainsMarkPersonalTrip(0, false);          // a stale one (e.g. left over from a job) is cleared
        RebuildIdleMoverList();
        expect(movers[0].transportState == TRANSPORT_NONE);
    }
}

// ============================================================================
// Station lines (follow the one ahead)
// ============================================================================

describe(station_lines) {
    it("waiters line up behind each other, front at the boarding spot, never on the track") {
        SetupClean();
        for (int x = 2; x <= 24; x++) GRID(x, 5, 1) = CELL_TRACK;
        GRID(10, 6, 1) = CELL_PLATFORM;
        GRID(11, 6, 1) = CELL_PLATFORM;
        GRID(12, 6, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        trainQueueEnabled = true;

        const int n = 10;
        moverCount = n;
        for (int i = 0; i < n; i++) {
            // arrive spread around the platform's land side, in this order
            float x = (6 + (i * 3) % 11) * CELL_SIZE + 16, y = (7 + i % 3) * CELL_SIZE + 16;
            InitMover(&movers[i], x, y, 1.0f, (Point){30, 30, 1}, MOVER_SPEED);
            movers[i].transportState = TRANSPORT_WAITING;
            movers[i].transportStation = 0;
            movers[i].transportFinalGoal = (Point){30, 30, 1};
            movers[i].pathLength = 0;
            movers[i].pathIndex = -1;
            StationAddWaiter(0, i);
        }
        bool onTrack = false;
        for (int t = 0; t < 900; t++) {
            Tick();
            for (int i = 0; i < n; i++)
                if (GRID((int)(movers[i].x / CELL_SIZE), (int)(movers[i].y / CELL_SIZE), 1) == CELL_TRACK) onTrack = true;
        }
        expect(!onTrack);
        expect(stations[0].waitingCount == n);

        float sx, sy;
        StationGetQueuePosition(0, 0, &sx, &sy);
        const Mover* front = &movers[stations[0].waitingMovers[0]];
        expect(fabsf(front->x - sx) < 4.0f && fabsf(front->y - sy) < 4.0f);

        float minGap = 1e9f, maxToAhead = 0.0f;
        for (int k = 1; k < n; k++) {
            const Mover* a = &movers[stations[0].waitingMovers[k - 1]];
            const Mover* b = &movers[stations[0].waitingMovers[k]];
            float d = sqrtf((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y));
            if (d > maxToAhead) maxToAhead = d;
        }
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                float d = sqrtf((movers[i].x - movers[j].x) * (movers[i].x - movers[j].x) +
                                (movers[i].y - movers[j].y) * (movers[i].y - movers[j].y));
                if (d < minGap) minGap = d;
            }
        expect(maxToAhead < CELL_SIZE * 1.5f);   // a line: everyone close to the one ahead
        expect(minGap > CELL_SIZE * 0.3f);       // nobody on top of anyone
    }
}

// ============================================================================
// Queue Positions
// ============================================================================

describe(queue_positions) {
    it("slot 1 further along platform than slot 0") {
        SetupClean();
        // Horizontal track, platform south of it (parallel)
        for (int x = 8; x <= 14; x++) GRID(x, 5, 1) = CELL_TRACK;
        GRID(10, 6, 1) = CELL_PLATFORM;
        GRID(11, 6, 1) = CELL_PLATFORM;
        GRID(12, 6, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        // Queue extends along x-axis (parallel to track)
        expect(stations[0].queueDirY == 0);

        float x0, y0, x1, y1;
        StationGetQueuePosition(0, 0, &x0, &y0);
        StationGetQueuePosition(0, 1, &x1, &y1);
        // Slot 1 should be offset from slot 0 along the platform
        float dist = fabsf(x1 - x0) + fabsf(y1 - y0);
        expect(dist > 1.0f);
        // Both on same y (along the platform)
        expect(fabsf(y0 - y1) < 0.1f);
    }

    it("positions extend along platform for vertical track") {
        SetupClean();
        // Vertical track, platform east of it (parallel)
        for (int y = 5; y <= 15; y++) {
            GRID(10, y, 1) = CELL_TRACK;
            TRACK_CONN(10, y, 1) = TRACK_N | TRACK_S;
        }
        TRACK_CONN(10, 5, 1) &= ~TRACK_N;
        TRACK_CONN(10, 15, 1) &= ~TRACK_S;
        GRID(11, 8, 1) = CELL_PLATFORM;
        GRID(11, 9, 1) = CELL_PLATFORM;
        GRID(11, 10, 1) = CELL_PLATFORM;
        RebuildStations();
        expect(stationCount == 1);
        // Queue extends along y-axis (parallel to vertical track)
        expect(stations[0].queueDirX == 0);

        float x0, y0, x2, y2;
        StationGetQueuePosition(0, 0, &x0, &y0);
        StationGetQueuePosition(0, 2, &x2, &y2);
        // Slot 2 should be offset from slot 0 along y
        float dist = fabsf(y2 - y0);
        expect(dist > 1.0f);
    }
}

// ============================================================================
// Track Connections
// ============================================================================

// Helper: place vertical track line with proper NS connections
static void PlaceTrackLineVertical(int y1, int y2, int x, int z) {
    for (int y = y1; y <= y2; y++) {
        GRID(x, y, z) = CELL_TRACK;
        TRACK_CONN(x, y, z) = TRACK_N | TRACK_S;
    }
    TRACK_CONN(x, y1, z) &= ~TRACK_N;
    TRACK_CONN(x, y2, z) &= ~TRACK_S;
}

describe(track_connections) {
    it("horizontal track has EW connections only") {
        SetupClean();
        PlaceTrackLine(5, 10, 5, 1);
        // Middle cells should be EW
        expect(TRACK_CONN(7, 5, 1) == (TRACK_E | TRACK_W));
        // End caps: left has E only, right has W only
        expect(TRACK_CONN(5, 5, 1) == TRACK_E);
        expect(TRACK_CONN(10, 5, 1) == TRACK_W);
        // Should NOT have N or S
        expect((TRACK_CONN(7, 5, 1) & (TRACK_N | TRACK_S)) == 0);
    }

    it("vertical track has NS connections only") {
        SetupClean();
        PlaceTrackLineVertical(5, 10, 5, 1);
        expect(TRACK_CONN(5, 7, 1) == (TRACK_N | TRACK_S));
        expect(TRACK_CONN(5, 5, 1) == TRACK_S);
        expect(TRACK_CONN(5, 10, 1) == TRACK_N);
        expect((TRACK_CONN(5, 7, 1) & (TRACK_E | TRACK_W)) == 0);
    }

    it("parallel horizontal tracks do not cross-connect") {
        SetupClean();
        PlaceTrackLine(5, 15, 5, 1);   // top line at y=5
        PlaceTrackLine(5, 15, 6, 1);   // bottom line at y=6 (adjacent)
        // Top line should have NO south connections
        for (int x = 5; x <= 15; x++) {
            expect((TRACK_CONN(x, 5, 1) & TRACK_S) == 0);
        }
        // Bottom line should have NO north connections
        for (int x = 5; x <= 15; x++) {
            expect((TRACK_CONN(x, 6, 1) & TRACK_N) == 0);
        }
    }

    it("parallel vertical tracks do not cross-connect") {
        SetupClean();
        PlaceTrackLineVertical(5, 15, 10, 1);  // left line at x=10
        PlaceTrackLineVertical(5, 15, 11, 1);  // right line at x=11
        for (int y = 5; y <= 15; y++) {
            expect((TRACK_CONN(10, y, 1) & TRACK_E) == 0);
            expect((TRACK_CONN(11, y, 1) & TRACK_W) == 0);
        }
    }

    it("sprite reflects connections not adjacency") {
        SetupClean();
        PlaceTrackLine(5, 15, 5, 1);
        PlaceTrackLine(5, 15, 6, 1);
        // Middle of top line should be EW sprite, NOT a crossroads
        int sprite = GetTrackSpriteAt(10, 5, 1);
        expect(sprite == SPRITE_track_ew);
        int sprite2 = GetTrackSpriteAt(10, 6, 1);
        expect(sprite2 == SPRITE_track_ew);
    }

    it("track removal disconnects from neighbors") {
        SetupClean();
        PlaceTrackLine(5, 10, 5, 1);
        expect(TRACK_CONN(7, 5, 1) == (TRACK_E | TRACK_W));
        // Simulate DisconnectTrackFromNeighbors for cell (7,5)
        uint8_t conn = TRACK_CONN(7, 5, 1);
        if (conn & TRACK_W) TRACK_CONN(6, 5, 1) &= ~TRACK_E;
        if (conn & TRACK_E) TRACK_CONN(8, 5, 1) &= ~TRACK_W;
        TRACK_CONN(7, 5, 1) = 0;
        GRID(7, 5, 1) = CELL_AIR;
        // Neighbors lost their inward connection
        expect((TRACK_CONN(6, 5, 1) & TRACK_E) == 0);
        expect((TRACK_CONN(8, 5, 1) & TRACK_W) == 0);
        // But keep their other connections
        expect((TRACK_CONN(6, 5, 1) & TRACK_W) != 0);
        expect((TRACK_CONN(8, 5, 1) & TRACK_E) != 0);
    }

    it("rectangle outline creates proper corners") {
        SetupClean();
        // Build a track rectangle like terrain generators do
        // Top edge EW
        for (int x = 5; x <= 10; x++) {
            GRID(x, 5, 1) = CELL_TRACK;
            TRACK_CONN(x, 5, 1) = TRACK_E | TRACK_W;
        }
        // Bottom edge EW
        for (int x = 5; x <= 10; x++) {
            GRID(x, 8, 1) = CELL_TRACK;
            TRACK_CONN(x, 8, 1) = TRACK_E | TRACK_W;
        }
        // Left edge NS
        for (int y = 5; y <= 8; y++) {
            GRID(5, y, 1) = CELL_TRACK;
            TRACK_CONN(5, y, 1) = TRACK_N | TRACK_S;
        }
        // Right edge NS
        for (int y = 5; y <= 8; y++) {
            GRID(10, y, 1) = CELL_TRACK;
            TRACK_CONN(10, y, 1) = TRACK_N | TRACK_S;
        }
        // Fix corners
        TRACK_CONN(5, 5, 1)  = TRACK_E | TRACK_S;
        TRACK_CONN(10, 5, 1) = TRACK_W | TRACK_S;
        TRACK_CONN(5, 8, 1)  = TRACK_E | TRACK_N;
        TRACK_CONN(10, 8, 1) = TRACK_W | TRACK_N;

        // Verify corners
        expect(TRACK_CONN(5, 5, 1) == (TRACK_E | TRACK_S));
        expect(TRACK_CONN(10, 5, 1) == (TRACK_W | TRACK_S));
        expect(TRACK_CONN(5, 8, 1) == (TRACK_E | TRACK_N));
        expect(TRACK_CONN(10, 8, 1) == (TRACK_W | TRACK_N));
        // Verify mid-edges
        expect(TRACK_CONN(7, 5, 1) == (TRACK_E | TRACK_W));
        expect(TRACK_CONN(5, 7, 1) == (TRACK_N | TRACK_S));
    }

    it("isolated single track cell has no connections") {
        SetupClean();
        GRID(10, 10, 1) = CELL_TRACK;
        TRACK_CONN(10, 10, 1) = 0;
        expect(GetTrackSpriteAt(10, 10, 1) == SPRITE_track_isolated);
    }
}

describe(track_lines) {
    it("each connected piece of track is its own line; stations and trains know theirs") {
        SetupClean();
        PlaceTrackLine(2, 20, 5, 1);
        PlaceTrackLine(2, 20, 15, 1);
        PlacePlatform(10, 6, 1);
        PlacePlatform(10, 16, 1);
        RebuildStations();
        expect(TrackLineCount() == 2);
        int a = TrackLineAt(2, 5, 1), b = TrackLineAt(20, 15, 1);
        expect(a >= 0 && b >= 0 && a != b);
        expect(TrackLineAt(20, 5, 1) == a);
        expect(TrackLineAt(3, 6, 1) == -1);   // not track
        expect(StationLine(GetStationAt(10, 5, 1)) == a);
        expect(StationLine(GetStationAt(10, 15, 1)) == b);
        int t = SpawnTrain(5, 15, 1);
        expect(TrainLine(t) == b);
    }

    it("track side by side without a connection stays two lines; joining makes one") {
        SetupClean();
        PlaceTrackLine(2, 20, 5, 1);
        PlaceTrackLine(2, 20, 6, 1);   // touching, but no N/S connection
        RebuildStations();
        expect(TrackLineCount() == 2);
        TRACK_CONN(20, 5, 1) |= TRACK_S;   // a corner joins them
        TRACK_CONN(20, 6, 1) |= TRACK_N;
        RebuildStations();
        expect(TrackLineCount() == 1);
        expect(TrackLineAt(2, 5, 1) == TrackLineAt(2, 6, 1));
    }

    it("lines and the stations' lines come back after save + load") {
        SetupClean();
        PlaceTrackLine(2, 20, 5, 1);
        PlaceTrackLine(2, 20, 15, 1);
        PlacePlatform(10, 6, 1);
        PlacePlatform(10, 16, 1);
        RebuildStations();
        expect(TrackLineCount() == 2);
        expect(SaveWorld("/tmp/test_trains_lines_save.bin"));
        ClearTrains();   // a fresh session: nothing derived left over
        expect(TrackLineCount() == 0);
        expect(LoadWorld("/tmp/test_trains_lines_save.bin"));
        expect(TrackLineCount() == 2);
        int a = StationLine(GetStationAt(10, 5, 1)), b = StationLine(GetStationAt(10, 15, 1));
        expect(a >= 0 && b >= 0 && a != b);
        remove("/tmp/test_trains_lines_save.bin");
    }

    it("ClearTrains forgets the lines") {
        SetupClean();
        PlaceTrackLine(2, 20, 5, 1);
        RebuildStations();
        expect(TrackLineCount() == 1);
        ClearTrains();
        expect(TrackLineCount() == 0);
        expect(TrackLineAt(5, 5, 1) == -1);
    }
}

describe(train_transfers) {
    // Line A east-west at y=10, line B north-south at x=205 starting just below A's
    // east stop. Mover near A's west stop, goal near B's south stop: no single line
    // goes there, two do.
    it("plans a change of line when no single line goes there, rides the first line to the change") {
        SetupClean();
        InitTestGrid(240, 120);
        for (int y = 0; y < 120; y++)
            for (int x = 0; x < 240; x++) {
                GRID(x, y, 0) = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                GRID(x, y, 1) = CELL_AIR;
            }
        PlaceTrackLine(2, 230, 10, 1);
        for (int y = 16; y <= 115; y++) { GRID(205, y, 1) = CELL_TRACK; TRACK_CONN(205, y, 1) = TRACK_N | TRACK_S; }
        TRACK_CONN(205, 16, 1) &= ~TRACK_N;
        TRACK_CONN(205, 115, 1) &= ~TRACK_S;
        PlacePlatform(10, 11, 1);
        PlacePlatform(200, 11, 1);
        PlacePlatform(206, 18, 1);
        PlacePlatform(206, 110, 1);
        RebuildStations();
        expect(TrackLineCount() == 2);
        int aWest = GetStationAt(10, 10, 1), aEast = GetStationAt(200, 10, 1);
        expect(aWest >= 0 && aEast >= 0);
        SpawnTrainHeading(60, 10, 1, 1, -1, 0);   // reaches the west stop just after the mover
        SpawnTrainHeading(205, 17, 1, 1, 0, 1);
        float oldSpeed = trainSpeed;
        trainSpeed = 50.0f;
        useTrainTimeChoice = true;
        moverCount = 1;
        InitMover(&movers[0], 10 * CELL_SIZE + 16, 14 * CELL_SIZE + 16, 1.0f, (Point){210, 112, 1}, MOVER_SPEED);
        movers[0].pathLength = 0;
        movers[0].pathIndex = -1;

        useTrainTransfers = false;
        expect(ShouldUseTrain(0) == false);   // no station of one line near both ends

        useTrainTransfers = true;
        int plans = trainTransferPlans;
        expect(ShouldUseTrain(0) == true);
        int entry, exit;
        TrainChosenStations(&entry, &exit);
        expect(entry == aWest);
        expect(exit == aEast);                 // get off at the change, plan the rest there
        expect(trainTransferPlans == plans + 1);

        trainSpeed = oldSpeed;
        useTrainTimeChoice = false;
    }

    it("plans a trip over three lines in a row (two changes), first ride to the first change") {
        SetupClean();
        InitTestGrid(320, 40);
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 320; x++) {
                GRID(x, y, 0) = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                GRID(x, y, 1) = CELL_AIR;
            }
        PlaceTrackLine(2, 100, 10, 1);     // A
        PlaceTrackLine(106, 204, 10, 1);   // B
        PlaceTrackLine(210, 310, 10, 1);   // C
        PlacePlatform(10, 11, 1);  PlacePlatform(99, 11, 1);
        PlacePlatform(107, 11, 1); PlacePlatform(203, 11, 1);
        PlacePlatform(211, 11, 1); PlacePlatform(300, 11, 1);
        RebuildStations();
        expect(TrackLineCount() == 3);
        int aWest = GetStationAt(10, 10, 1), aEast = GetStationAt(99, 10, 1);
        SpawnTrainHeading(40, 10, 1, 1, -1, 0);
        SpawnTrainHeading(150, 10, 1, 1, -1, 0);
        SpawnTrainHeading(250, 10, 1, 1, -1, 0);
        float oldSpeed = trainSpeed;
        trainSpeed = 50.0f;
        useTrainTimeChoice = true;
        useTrainTransfers = true;
        moverCount = 1;
        InitMover(&movers[0], 10 * CELL_SIZE + 16, 14 * CELL_SIZE + 16, 1.0f, (Point){295, 14, 1}, MOVER_SPEED);
        movers[0].pathLength = 0;
        movers[0].pathIndex = -1;
        int twoChanges = trainPlanChanges[2];
        expect(ShouldUseTrain(0) == true);
        int entry, exit;
        TrainChosenStations(&entry, &exit);
        expect(entry == aWest);
        expect(exit == aEast);
        expect(trainPlanChanges[2] == twoChanges + 1);
        movers[0].transportState = TRANSPORT_WALKING_TO_STATION;   // the route shows while travelling
        int lines[8];
        expect(TrainPlannedRoute(0, lines, 8) == 3);
        expect(lines[0] == TrackLineAt(50, 10, 1));
        expect(lines[2] == TrackLineAt(250, 10, 1));
        movers[0].transportState = TRANSPORT_NONE;
        trainSpeed = oldSpeed;
        useTrainTimeChoice = false;
    }
}

// Count of `type` units in a stockpile's slots
static int CountInStockpile(int sp, ItemType type) {
    int n = 0;
    const Stockpile* s = &stockpiles[sp];
    for (int i = 0; i < s->width * s->height; i++)
        if (s->cells[i] && s->slots[i] >= 0 && items[s->slots[i]].type == type) n += s->slotCounts[i];
    return n;
}

static int SetupRockStockpile(int x, int y, int w, int h, int priority) {
    int sp = CreateStockpile(x, y, 1, w, h);
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) SetStockpileFilter(sp, (ItemType)t, false);
    SetStockpileFilter(sp, ITEM_ROCK, true);
    SetStockpilePriority(sp, priority);
    return sp;
}

describe(train_freight) {
    // A shuttle between a west stop (x=5) and an east stop (x=34), rock stockpiles two
    // rows from each platform: west priority 2 with rocks in it, east priority 7
    it("carries loads from a stop's cargo stockpile to a higher-priority one down the line, and not back") {
        SetupClean();
        PlaceTrackLine(2, 37, 10, 1);
        PlacePlatform(5, 11, 1);
        PlacePlatform(34, 11, 1);
        RebuildStations();
        int west = SetupRockStockpile(3, 13, 4, 2, 2);
        int east = SetupRockStockpile(32, 13, 4, 2, 7);
        int sps[8];
        expect(StationCargoStockpiles(GetStationAt(5, 10, 1), sps, 8) == 1 && sps[0] == west);
        expect(StationCargoStockpiles(GetStationAt(34, 10, 1), sps, 8) == 1 && sps[0] == east);
        for (int i = 0; i < 3; i++) {
            int it = SpawnItem((3 + i + 0.5f) * CELL_SIZE, 13.5f * CELL_SIZE, 1.0f, ITEM_ROCK);
            PlaceItemInStockpile(west, 3 + i, 13, it);
        }
        expect(CountInStockpile(west, ITEM_ROCK) == 3);
        int t = SpawnTrainHeading(20, 10, 1, 2, -1, 0);   // heads west first
        expect(t >= 0);
        bool sawAboard = false;
        for (int k = 0; k < 60 * 60 && CountInStockpile(east, ITEM_ROCK) < 3; k++) {
            TrainsTick(TICK_DT);
            if (trains[t].cargoCount > 0) sawAboard = true;
        }
        expect(sawAboard);
        expect(CountInStockpile(east, ITEM_ROCK) == 3);
        expect(CountInStockpile(west, ITEM_ROCK) == 0);
        // a few more round trips: nothing goes back (west has the lower priority)
        for (int k = 0; k < 60 * 30; k++) TrainsTick(TICK_DT);
        expect(CountInStockpile(east, ITEM_ROCK) == 3);
        expect(trains[t].cargoCount == 0);
    }

    it("removing the train drops its freight on the ground, and trainFreight off loads nothing") {
        SetupClean();
        PlaceTrackLine(2, 37, 10, 1);
        PlacePlatform(5, 11, 1);
        PlacePlatform(34, 11, 1);
        RebuildStations();
        int west = SetupRockStockpile(3, 13, 4, 2, 2);
        SetupRockStockpile(32, 13, 4, 2, 7);
        int it = SpawnItem(3.5f * CELL_SIZE, 13.5f * CELL_SIZE, 1.0f, ITEM_ROCK);
        PlaceItemInStockpile(west, 3, 13, it);
        trainFreight = false;
        int t = SpawnTrainHeading(20, 10, 1, 2, -1, 0);
        for (int k = 0; k < 60 * 10; k++) TrainsTick(TICK_DT);
        expect(trains[t].cargoCount == 0);
        expect(CountInStockpile(west, ITEM_ROCK) == 1);
        trainFreight = true;
        bool aboard = false;
        for (int k = 0; k < 60 * 20 && !aboard; k++) { TrainsTick(TICK_DT); aboard = trains[t].cargoCount > 0; }
        expect(aboard);
        int carried = trains[t].cargo[0];
        expect(items[carried].state == ITEM_ON_TRAIN);
        DropTrainCargo(t);
        expect(trains[t].cargoCount == 0);
        expect(items[carried].state == ITEM_ON_GROUND);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);

    test(station_detection);
    test(waiting_set);
    test(train_stopping);
    test(should_use_train);
    test(boarding_exiting);
    test(mover_transport_state);
    test(transport_haul_integration);
    test(cancel_job_transport);
    test(platform_destroyed);
    test(track_destroyed);
    test(multi_cell_platform);
    test(fifo_shift_down);
    test(train_direction_choice);
    test(station_patience);
    test(station_lines);
    test(queue_positions);
    test(track_connections);
    test(track_lines);
    test(train_transfers);
    test(train_freight);

    return summary();
}
