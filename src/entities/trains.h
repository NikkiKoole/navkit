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
#define TRAIN_DEFAULT_SPEED 3.0f  // Cells per second

// Transport thresholds
#define TRANSPORT_FAR_THRESHOLD 40
#define TRANSPORT_STATION_RADIUS 20
#define TRANSPORT_WAIT_TIMEOUT 60.0f

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
} Train;

extern Train trains[MAX_TRAINS];
extern int trainCount;

extern TrainStation stations[MAX_STATIONS];
extern int stationCount;

extern bool trainQueueEnabled;  // Toggle: queue line vs spread-out avoidance

void InitTrains(void);
void ClearTrains(void);
void TrainsTick(float dt);
int  SpawnTrain(int x, int y, int z);
int  SpawnTrainWithCars(int x, int y, int z, int carCount);

// Station management
void RebuildStations(void);
int  GetStationAt(int x, int y, int z);
int  FindNearestStation(int x, int y, int z, int maxRadius);

// WaitingSet operations
void StationAddWaiter(int stationIdx, int moverIdx);
void StationRemoveWaiter(int stationIdx, int moverIdx);
int  StationGetNextBoarder(int stationIdx);

// Queue position
void StationGetQueuePosition(int stationIdx, int slotIndex, float* outX, float* outY);

// Transport heuristic
bool ShouldUseTrain(int moverIdx);

// Board/exit
void BoardMoverOnTrain(int trainIdx, int moverIdx, int stationIdx);
void ExitMoverFromTrain(int trainIdx, int riderSlot, int stationIdx);
void DismountAllRiders(int trainIdx);

#endif // TRAINS_H
