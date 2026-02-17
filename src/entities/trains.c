#include "trains.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/lighting.h"
#include "../core/time.h"
#include <stdlib.h>

Train trains[MAX_TRAINS];
int trainCount = 0;

void InitTrains(void) {
    ClearTrains();
}

void ClearTrains(void) {
    for (int i = 0; i < MAX_TRAINS; i++) {
        trains[i].active = false;
    }
    trainCount = 0;
}

int SpawnTrain(int x, int y, int z) {
    if (trainCount >= MAX_TRAINS) return -1;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return -1;
    if (grid[z][y][x] != CELL_TRACK) return -1;

    int idx = -1;
    for (int i = 0; i < MAX_TRAINS; i++) {
        if (!trains[i].active) { idx = i; break; }
    }
    if (idx < 0) return -1;

    Train* t = &trains[idx];
    t->x = x * CELL_SIZE + CELL_SIZE * 0.5f;
    t->y = y * CELL_SIZE + CELL_SIZE * 0.5f;
    t->z = z;
    t->cellX = x;
    t->cellY = y;
    t->prevCellX = x;
    t->prevCellY = y;
    t->speed = TRAIN_DEFAULT_SPEED;
    t->progress = 1.0f;  // Start "arrived" so first tick picks a direction
    t->lightCellX = -1;
    t->lightCellY = -1;
    t->active = true;
    trainCount++;
    return idx;
}

// Find the next track cell to move to
// Returns true if a next cell was found, writes to outX/outY
static bool FindNextTrackCell(Train* t, int* outX, int* outY) {
    static const int dx[] = { 0, 1, 0, -1 };  // N, E, S, W
    static const int dy[] = { -1, 0, 1, 0 };

    int options[4][2];
    int optionCount = 0;

    for (int d = 0; d < 4; d++) {
        int nx = t->cellX + dx[d];
        int ny = t->cellY + dy[d];
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
        if (grid[t->z][ny][nx] != CELL_TRACK) continue;
        // Don't reverse into previous cell (unless it's the only option)
        if (nx == t->prevCellX && ny == t->prevCellY) continue;
        options[optionCount][0] = nx;
        options[optionCount][1] = ny;
        optionCount++;
    }

    if (optionCount > 0) {
        if (optionCount == 1) {
            *outX = options[0][0];
            *outY = options[0][1];
        } else {
            // Prefer going straight (same direction as current heading)
            int headingX = t->cellX - t->prevCellX;
            int headingY = t->cellY - t->prevCellY;
            int straightX = t->cellX + headingX;
            int straightY = t->cellY + headingY;

            // Check if straight-ahead is one of the options
            int straightIdx = -1;
            for (int i = 0; i < optionCount; i++) {
                if (options[i][0] == straightX && options[i][1] == straightY) {
                    straightIdx = i;
                    break;
                }
            }

            if (straightIdx >= 0 && (rand() % 100) >= 10) {
                // Go straight (90% chance; 10% chance to pick randomly instead)
                *outX = options[straightIdx][0];
                *outY = options[straightIdx][1];
            } else {
                // No straight option (T-junction) — pick random among remaining
                int pick = rand() % optionCount;
                *outX = options[pick][0];
                *outY = options[pick][1];
            }
        }
        return true;
    }

    // Dead end: reverse
    if (t->prevCellX != t->cellX || t->prevCellY != t->cellY) {
        *outX = t->prevCellX;
        *outY = t->prevCellY;
        return true;
    }

    return false;  // Isolated single track cell
}

void TrainsTick(float dt) {
    float gdt = dt * gameSpeed;
    if (gdt <= 0.0f) return;

    for (int i = 0; i < MAX_TRAINS; i++) {
        Train* t = &trains[i];
        if (!t->active) continue;

        // Check track still exists under us
        if (grid[t->z][t->cellY][t->cellX] != CELL_TRACK) {
            if (t->lightCellX >= 0) RemoveLightSource(t->lightCellX, t->lightCellY, t->z);
            t->active = false;
            trainCount--;
            continue;
        }

        t->progress += t->speed * (60.0f / dayLength) * gdt;

        while (t->progress >= 1.0f) {
            t->progress -= 1.0f;

            // Arrived at cellX/cellY — pick next cell
            int nextX, nextY;
            if (FindNextTrackCell(t, &nextX, &nextY)) {
                t->prevCellX = t->cellX;
                t->prevCellY = t->cellY;
                t->cellX = nextX;
                t->cellY = nextY;
            } else {
                // Stuck on isolated cell
                t->progress = 0.0f;
                break;
            }
        }

        // Interpolate pixel position between prev cell and current cell
        float fromX = t->prevCellX * CELL_SIZE + CELL_SIZE * 0.5f;
        float fromY = t->prevCellY * CELL_SIZE + CELL_SIZE * 0.5f;
        float toX = t->cellX * CELL_SIZE + CELL_SIZE * 0.5f;
        float toY = t->cellY * CELL_SIZE + CELL_SIZE * 0.5f;
        t->x = fromX + (toX - fromX) * t->progress;
        t->y = fromY + (toY - fromY) * t->progress;

        // Update locomotive light at current cell
        int curCellX = (int)(t->x / CELL_SIZE);
        int curCellY = (int)(t->y / CELL_SIZE);
        if (curCellX != t->lightCellX || curCellY != t->lightCellY) {
            if (t->lightCellX >= 0) RemoveLightSource(t->lightCellX, t->lightCellY, t->z);
            AddLightSource(curCellX, curCellY, t->z, 200, 180, 120, 8);
            t->lightCellX = curCellX;
            t->lightCellY = curCellY;
        }
    }
}
