#ifndef TRAINS_H
#define TRAINS_H

#include <stdbool.h>

#define MAX_TRAINS 32
#define TRAIN_DEFAULT_SPEED 3.0f  // Cells per second

typedef struct {
    float x, y;                // Pixel position (smooth interpolation)
    int z;                     // Z-level
    int cellX, cellY;          // Current target cell
    int prevCellX, prevCellY;  // Previous cell (don't reverse into this)
    float speed;               // Cells per second
    float progress;            // 0.0-1.0 interpolation between prev and current cell
    int lightCellX, lightCellY; // Last cell where we placed a light (for removal)
    bool active;
} Train;

extern Train trains[MAX_TRAINS];
extern int trainCount;

void InitTrains(void);
void ClearTrains(void);
void TrainsTick(float dt);
int  SpawnTrain(int x, int y, int z);

#endif // TRAINS_H
