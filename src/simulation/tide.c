// tide.c - Tide (experiment, see tide.h and docs/todo/world/tide.md)
#include "tide.h"
#include "water.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/pathfinding.h"
#include "../core/time.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool tideEnabled = false;
float tidePeriodHours = 12.42f;
int tideLowLevel = 0;
int tideLevel = WATER_MAX_LEVEL;
int tideSeaZ = -1;
int tideSeaCells = 0;
int tideSteps = 0;
double tideLastStepMs = 0.0, tideMaxStepMs = 0.0;

// Sea cells of the top level (x, y; z = tideSeaZ), for the level changes
typedef struct { short x, y; } TideCell;
static TideCell* seaCells = NULL;
static int seaCap = 0;

static double TideNowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void SeaPush(int x, int y) {
    if (tideSeaCells >= seaCap) {
        int cap = seaCap ? seaCap * 2 : 4096;
        TideCell* n = realloc(seaCells, sizeof(TideCell) * (size_t)cap);
        if (!n) return;
        seaCells = n;
        seaCap = cap;
    }
    seaCells[tideSeaCells++] = (TideCell){(short)x, (short)y};
}

void ClearTide(void) {
    tideEnabled = false;
    tideSeaZ = -1;
    tideSeaCells = 0;
    tideSteps = 0;
    tideLevel = WATER_MAX_LEVEL;
    tideLastStepMs = tideMaxStepMs = 0.0;
}

int TideMarkSea(int z) {
    tideSeaCells = 0;
    tideSeaZ = -1;
    if (z < 0 || z >= gridDepth) return 0;
    // Flood fill over water at level z from the map edge; seaCells doubles as the queue
    int head = 0;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            if (x != 0 && y != 0 && x != gridWidth - 1 && y != gridHeight - 1) continue;
            if (WATER(x, y, z).level == 0 || WATER(x, y, z).isSea) continue;
            WATER_MUT(x, y, z).isSea = 1;
            SeaPush(x, y);
        }
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    while (head < tideSeaCells) {
        TideCell c = seaCells[head++];
        for (int d = 0; d < 4; d++) {
            int nx = c.x + DX[d], ny = c.y + DY[d];
            if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
            if (WATER(nx, ny, z).level == 0 || WATER(nx, ny, z).isSea) continue;
            WATER_MUT(nx, ny, z).isSea = 1;
            SeaPush(nx, ny);
        }
    }
    tideSeaZ = z;
    tideSteps = 0;
    tideLevel = WATER(seaCells[0].x, seaCells[0].y, z).level;
    return tideSeaCells;
}

void TideAfterLoad(void) {
    tideSeaCells = 0;
    tideSeaZ = -1;
    tideSteps = 0;
    tideLastStepMs = tideMaxStepMs = 0.0;
    for (int z = 0; z < gridDepth && tideSeaZ < 0; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                if (WATER(x, y, z).isSea) {
                    if (tideSeaZ < 0) tideSeaZ = z;
                    SeaPush(x, y);
                }
    tideLevel = tideSeaCells > 0 ? WATER(seaCells[0].x, seaCells[0].y, tideSeaZ).level : WATER_MAX_LEVEL;
}

int TideLevelAt(float gameHours) {
    float period = tidePeriodHours > 0.1f ? tidePeriodHours : 0.1f;
    float high = 0.5f + 0.5f * cosf(2.0f * 3.14159265f * gameHours / period);   // 1 = high water
    int low = tideLowLevel < 0 ? 0 : tideLowLevel > WATER_MAX_LEVEL ? WATER_MAX_LEVEL : tideLowLevel;
    return low + (int)lroundf(high * (float)(WATER_MAX_LEVEL - low));
}

void TideTick(void) {
    if (!tideEnabled || tideSeaZ < 0 || tideSeaCells == 0) return;
    // The world was replaced under us (scenario reset, load): forget the sea
    if (tideSeaZ >= gridDepth || !WATER(seaCells[0].x, seaCells[0].y, tideSeaZ).isSea) { ClearTide(); return; }
    int want = TideLevelAt((float)dayNumber * 24.0f + timeOfDay);
    if (want == tideLevel) return;
    // One step at a time (a big time jump still walks the levels in order)
    int next = want > tideLevel ? tideLevel + 1 : tideLevel - 1;
    double t0 = TideNowMs();
    int z = tideSeaZ;
    for (int i = 0; i < tideSeaCells; i++) {
        int x = seaCells[i].x, y = seaCells[i].y;
        const WaterCell* wc = &WATER(x, y, z);
        if (!wc->isSea || wc->isFrozen) continue;
        int old = wc->level;
        if (old == next) continue;
        SetWaterLevel(x, y, z, next);
        // Crossing wading depth changes who can walk there
        if ((old >= WATER_BLOCKS_MOVEMENT) != (next >= WATER_BLOCKS_MOVEMENT)) MarkChunkDirty(x, y, z);
    }
    tideLevel = next;
    tideSteps++;
    tideLastStepMs = TideNowMs() - t0;
    if (tideLastStepMs > tideMaxStepMs) tideMaxStepMs = tideLastStepMs;
}
