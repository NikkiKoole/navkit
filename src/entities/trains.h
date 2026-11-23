#ifndef TRAINS_H
#define TRAINS_H

#include <stdbool.h>
#include "../world/grid.h"  // For Point

#define MAX_TRAINS 32
#define MAX_STATIONS 64
#define MAX_STATION_WAITING 16
#define MAX_CART_CAPACITY 8
#define MAX_PLATFORM_CELLS 8
#define MAX_TRAIL_LENGTH 8
#define TRAIN_DOOR_TIME 3.0f
#define TRAIN_CARGO_PER_CAR 4   // loads (stacks) per car
#define MAX_TRAIN_CARGO 32
#define TRAIN_CARGO_REACH 2     // a stockpile this close to a platform is that stop's cargo stockpile
#define TRAIN_DEFAULT_SPEED 25.0f  // Cells per second (4x walking: worth it on long trips, see train-double)

// Transport thresholds
#define TRANSPORT_FAR_THRESHOLD 40
#define TRANSPORT_STATION_RADIUS 20
#define TRANSPORT_WAIT_TIMEOUT 60.0f
#define TRAIN_TRANSFER_WALK 50   // most cells walked (pathfinder cost, any level) between two lines' platforms to change

typedef enum { CART_MOVING, CART_DOORS_OPEN } TrainCartState;

typedef enum {
    TRANSPORT_NONE,
    TRANSPORT_WALKING_TO_STATION,
    TRANSPORT_WAITING,
    TRANSPORT_RIDING,
} TransportState;

typedef struct {
    int trackX, trackY, z;    // The track cell
    int platX, platY;         // Adjacent platform cell (mover entry/exit point)
    bool active;
    // Multi-cell platform data
    int platformCells[MAX_PLATFORM_CELLS][2];  // (x,y) pairs, ordered front-to-back
    int platformCellCount;
    int queueDirX, queueDirY;                 // Unit vector away from track (-1/0/1)
    // WaitingSet (inline)
    int waitingMovers[MAX_STATION_WAITING];
    float waitingSince[MAX_STATION_WAITING];
    int waitingCount;
} TrainStation;

typedef struct {
    float x, y;                // Pixel position (smooth interpolation)
    int z;                     // Z-level
    int cellX, cellY;          // Current target cell
    int prevCellX, prevCellY;  // Previous cell (don't reverse into this)
    float speed;               // Cells per second
    float progress;            // 0.0-1.0 interpolation between prev and current cell
    int lightCellX, lightCellY; // Last cell where we placed a light (for removal)
    bool active;
    // Station/transport fields (v86+)
    int cartState;             // TrainCartState
    float stateTimer;          // Countdown for doors-open
    int atStation;             // Station index when stopped, -1 when moving
    int ridingMovers[MAX_CART_CAPACITY];
    int ridingCount;
    // Multi-car train (v88+)
    int carCount;                       // 1 = just locomotive, 2+ = locomotive + trailing cars
    int trailCellX[MAX_TRAIL_LENGTH];   // Previous cell positions (trail[0] = most recent)
    int trailCellY[MAX_TRAIL_LENGTH];
    int trailCount;                     // How many trail entries are filled (builds up from 0)
    // Freight (v97+): item indices riding (ITEM_ON_TRAIN) and the priority of the
    // stockpile each was loaded from (it is unloaded only into a higher one)
    int cargo[MAX_TRAIN_CARGO];
    unsigned char cargoFromPriority[MAX_TRAIN_CARGO];
    int cargoCount;
} Train;

extern Train trains[MAX_TRAINS];
extern int trainCount;

extern TrainStation stations[MAX_STATIONS];
extern int stationCount;

extern float trainSpeed;        // cells/s (TRAIN_DEFAULT_SPEED), tunable
extern bool trainQueueEnabled;  // Toggle: queue line vs spread-out avoidance

void InitTrains(void);
void ClearTrains(void);
void TrainsTick(float dt);
int  SpawnTrain(int x, int y, int z);
int  SpawnTrainWithCars(int x, int y, int z, int carCount);
int  SpawnTrainHeading(int x, int y, int z, int carCount, int dx, int dy);   // runs toward (dx, dy) first

// Lines: each connected piece of track is one line (derived, not saved).
// RebuildStations also rebuilds the lines.
#define TRACK_LINE_COLORS 8
void RebuildTrackLines(void);
void TrainsAfterLoad(void);   // derived train state after LoadWorld
int  TrackLineCount(void);
int  TrackLineAt(int x, int y, int z);   // -1 = no track
int  StationLine(int stationIdx);
int  TrainLine(int trainIdx);
const char* TrackLineName(int line);    // "Red", "Blue", ...
void TrackLineRGB(int line, unsigned char* r, unsigned char* g, unsigned char* b);

// Station management
void RebuildStations(void);
int  GetStationAt(int x, int y, int z);
int  FindNearestStation(int x, int y, int z, int maxRadius);
void GetNearestPlatformCell(int stationIdx, int x, int y, int* outX, int* outY);

// WaitingSet operations
void StationAddWaiter(int stationIdx, int moverIdx);
void StationRemoveWaiter(int stationIdx, int moverIdx);
int  StationGetNextBoarder(int stationIdx);

// Queue position
void StationGetQueuePosition(int stationIdx, int slotIndex, float* outX, float* outY);

// Transport heuristic
bool ShouldUseTrain(int moverIdx);
void TrainsNoteGaveUp(int moverIdx);
// Trip time estimates (seconds): by train (walk, measured wait, ride, doors, walk)
// and walking the mover's current path. TrainNoteDecision records both for a trip
// the current rule sends by train (headless stats).
float TrainTripSeconds(int moverIdx, int entry, int exit, Point goal);
float WalkTripSeconds(int moverIdx);
void TrainNoteDecision(int moverIdx, int entry, int exit);
// ShouldUseTrain picks the (entry, exit) stations too; read them after it returned true.
// useTrainTimeChoice: by trip time in the direction trains run (else: the old
// "goal 40+ cells away, a station near both ends" rule).
void TrainChosenStations(int* entry, int* exit);
int StationBoardings(int stationIdx);
// A train trip for the mover's own goal (no job): not stale transport state
void TrainsMarkPersonalTrip(int moverIdx, bool personal);
bool TrainsIsPersonalTrip(int moverIdx);   // movers boarded here since stations were (re)built
extern bool useTrainTimeChoice;
extern bool useTrainTransfers;    // trip choice may plan a change of line (two rides)
extern int trainTransferPlans;     // trips chosen that plan a change of line (stats)
#define TRAIN_PLAN_CHANGES_MAX 7
extern int trainPlanChanges[TRAIN_PLAN_CHANGES_MAX + 1];   // trips chosen by number of changes (last: or more)
// Lines (TrackLineAt ids) of the mover's planned train trip, in order; 0 if none
int TrainPlannedRoute(int moverIdx, int* lines, int maxLines);
extern int trainChoicesConsidered;
extern int trainDecisions, trainDecisionsFaster;
extern double trainDecisionTrainSec, trainDecisionWalkSec;   // gave up waiting: walk, no train for a while

// Freight. A cargo stockpile is one with a cell within TRAIN_CARGO_REACH of a station's
// platform. A train stopping there unloads loads into it when it has a higher priority
// than the one they came from, and loads whole stacks that a stop on its line has a
// higher-priority cargo stockpile for (StockpileTakeStack / StockpilePutLoad). Haulers
// that would ride the train with an item drop it at the boarding stop's cargo stockpile
// instead (TrainCargoRedirectHaul), and don't rehaul what the train will carry on.
int  StationCargoStockpiles(int stationIdx, int* out, int maxOut);
bool TrainCargoWillForward(int stockpileIdx, int itemIdx);   // the train takes it on from here
bool TrainCargoRedirectHaul(int moverIdx, int entryStation);  // true: the mover delivers to the stop
void DropTrainCargo(int trainIdx);                            // on the ground (train removed)
extern int trainCargoLoaded, trainCargoUnloaded, trainCargoHaulDrops;
extern bool trainFreight;   // setting: trains carry freight between cargo stockpiles

// Board/exit
void BoardMoverOnTrain(int trainIdx, int moverIdx, int stationIdx);
void ExitMoverFromTrain(int trainIdx, int riderSlot, int stationIdx);
void DismountAllRiders(int trainIdx);

#endif // TRAINS_H
