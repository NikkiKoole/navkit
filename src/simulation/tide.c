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
bool islandTide = false;
float tidePeriodHours = 12.42f;
int tideLowLevel = 0;
int tideLevel = WATER_MAX_LEVEL;
int tideSeaZ = -1;
int tideSeaCells = 0;
int tideSteps = 0;
double tideLastStepMs = 0.0, tideMaxStepMs = 0.0;
float tideWaveHours = 2.0f;
float tideWaveAngle = 0.0f;

// The tide wave: the sea is cut into strips across the map (along tideWaveAngle); each strip
// follows the tide tideWaveHours * (its position) later than the first, so the water crosses
// wading depth strip by strip and each step rebuilds only the chunks under the front.
// seaCells is sorted by strip: strip s is seaCells[stripStart[s] .. stripStart[s+1]).
// Strips are about a chunk wide (16 cells): each chunk is rebuilt about once per crossing.
#define TIDE_STRIPS 64
static int stripCount = 1;
static int stripLevel[TIDE_STRIPS];
static int stripStart[TIDE_STRIPS + 1];
static float stripDirX = 1.0f, stripDirY = 0.0f, stripMin = 0.0f, stripSpan = 1.0f;

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

static void StripSetup(void) {
    float a = tideWaveAngle * 3.14159265f / 180.0f;
    stripDirX = cosf(a); stripDirY = sinf(a);
    float lo = 1e30f, hi = -1e30f;
    for (int c = 0; c < 4; c++) {   // the map's corners bound the projection
        float p = (c & 1 ? (float)gridWidth : 0.0f) * stripDirX + (c & 2 ? (float)gridHeight : 0.0f) * stripDirY;
        if (p < lo) lo = p;
        if (p > hi) hi = p;
    }
    stripMin = lo;
    stripSpan = hi - lo > 1.0f ? hi - lo : 1.0f;
    stripCount = (int)(stripSpan / 16.0f + 0.5f);
    if (stripCount < 1) stripCount = 1;
    if (stripCount > TIDE_STRIPS) stripCount = TIDE_STRIPS;
}

static int StripOf(int x, int y) {
    // Wavy strip borders: a straight border on the chunk grid looked like a drawn line; the
    // offset varies along the border (q runs along it), so the front comes in as a wavy line
    float q = -(x + 0.5f) * stripDirY + (y + 0.5f) * stripDirX;
    float wobble = 5.0f * sinf(q * 0.13f) + 2.5f * sinf(q * 0.37f + 1.3f);
    float p = ((x + 0.5f) * stripDirX + (y + 0.5f) * stripDirY + wobble - stripMin) / stripSpan;
    int s = (int)(p * stripCount);
    return s < 0 ? 0 : s >= stripCount ? stripCount - 1 : s;
}

// Sort seaCells by strip (counting sort) and take each strip's level from its first cell
// The level of the first strip that has sea in it (the wave's front), for stats and tests
static int TideFrontLevel(void) {
    for (int s = 0; s < stripCount; s++)
        if (stripStart[s] < stripStart[s + 1]) return stripLevel[s];
    return WATER_MAX_LEVEL;
}

static void StripSort(void) {
    StripSetup();
    int count[TIDE_STRIPS] = {0};
    for (int i = 0; i < tideSeaCells; i++) count[StripOf(seaCells[i].x, seaCells[i].y)]++;
    stripStart[0] = 0;
    for (int s = 0; s < TIDE_STRIPS; s++) stripStart[s + 1] = stripStart[s] + count[s];
    TideCell* sorted = malloc(sizeof(TideCell) * (size_t)(tideSeaCells > 0 ? tideSeaCells : 1));
    if (!sorted) return;
    int fill[TIDE_STRIPS];
    memcpy(fill, stripStart, sizeof(fill));
    for (int i = 0; i < tideSeaCells; i++) sorted[fill[StripOf(seaCells[i].x, seaCells[i].y)]++] = seaCells[i];
    memcpy(seaCells, sorted, sizeof(TideCell) * (size_t)tideSeaCells);
    free(sorted);
    for (int s = 0; s < TIDE_STRIPS; s++)
        stripLevel[s] = stripStart[s] < stripStart[s + 1]
            ? WATER(seaCells[stripStart[s]].x, seaCells[stripStart[s]].y, tideSeaZ).level : WATER_MAX_LEVEL;
    tideLevel = TideFrontLevel();
}

// The level every strip wants now, set at once (no chunk marking: the caller builds the nav
// graph after). For setup: otherwise the first ticks step the whole sea together.
void TideSettle(void) {
    if (tideSeaZ < 0 || tideSeaCells == 0) return;
    float hours = (float)dayNumber * 24.0f + timeOfDay;
    for (int s = 0; s < stripCount; s++) {
        float lag = stripCount > 1 ? tideWaveHours * (float)s / (float)(stripCount - 1) : 0.0f;
        int want = TideLevelAt(hours - lag);
        for (int i = stripStart[s]; i < stripStart[s + 1]; i++)
            SetWaterLevel(seaCells[i].x, seaCells[i].y, tideSeaZ, want);
        stripLevel[s] = want;
    }
    tideLevel = TideFrontLevel();
}

int TideCellLevel(int x, int y) {
    return stripLevel[StripOf(x, y)];
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
    StripSort();
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
    if (tideSeaCells > 0) StripSort();
    else tideLevel = WATER_MAX_LEVEL;
}

int TideLevelAt(float gameHours) {
    float period = tidePeriodHours > 0.1f ? tidePeriodHours : 0.1f;
    float high = 0.5f + 0.5f * cosf(2.0f * 3.14159265f * gameHours / period);   // 1 = high water
    int low = tideLowLevel < 0 ? 0 : tideLowLevel > WATER_MAX_LEVEL ? WATER_MAX_LEVEL : tideLowLevel;
    return low + (int)lroundf(high * (float)(WATER_MAX_LEVEL - low));
}

// A flat: sea cell on the top level over solid floor (walkable once the tide is out)
static bool TideFlatAt(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) return false;
    return WATER(x, y, z).isSea && GRID(x, y, z) == CELL_AIR && CellIsSolid(GRID(x, y, z - 1));
}

int TidePrepareShore(void) {
    int z = tideSeaZ;
    if (z < 1 || z + 1 >= gridDepth) return 0;
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};   // rising N, E, S, W
    static const CellType RAMP[4] = {CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W};
    int placed = 0;
    for (int y = 1; y < gridHeight - 1; y++)
        for (int x = 1; x < gridWidth - 1; x++) {
            if (GRID(x, y, z) != CELL_WALL || GRID(x, y, z + 1) != CELL_AIR) continue;
            for (int d = 0; d < 4; d++) {
                int sx = x - DX[d], sy = y - DY[d];       // low side: a flat
                int ex = x + DX[d], ey = y + DY[d];       // high side: land at the beach level
                if (!WATER(sx, sy, z).isSea || GRID(sx, sy, z) != CELL_AIR) continue;
                if (!CellIsSolid(GRID(sx, sy, z - 1))) continue;
                if (!CellIsSolid(GRID(ex, ey, z)) || GRID(ex, ey, z + 1) != CELL_AIR) continue;
                // Leaving a ramp sideways is one-way (nobody enters a ramp from the side): a flat
                // beside it must lead back to the ramp's foot, or it is a pocket (the rule of
                // island_gen.c IslandRampSidesOk)
                bool sidesOk = true;
                for (int side = -1; side <= 1 && sidesOk; side += 2) {
                    int px = DY[d] * side, py = -DX[d] * side;
                    if (TideFlatAt(x + px, y + py, z) && !TideFlatAt(x + px - DX[d], y + py - DY[d], z)) sidesOk = false;
                }
                if (!sidesOk) continue;
                GRID(x, y, z) = RAMP[d];
                placed++;
                break;
            }
        }
    return placed;
}

void TideTick(void) {
    if (!tideEnabled || tideSeaZ < 0 || tideSeaCells == 0) return;
    // The world was replaced under us (scenario reset, load): forget the sea
    if (tideSeaZ >= gridDepth || !WATER(seaCells[0].x, seaCells[0].y, tideSeaZ).isSea) { ClearTide(); return; }
    float hours = (float)dayNumber * 24.0f + timeOfDay;
    int z = tideSeaZ;
    double t0 = 0.0;
    bool stepped = false;
    for (int s = 0; s < TIDE_STRIPS; s++) {
        if (stripStart[s] == stripStart[s + 1]) continue;
        float lag = stripCount > 1 ? tideWaveHours * (float)s / (float)(stripCount - 1) : 0.0f;
        int want = TideLevelAt(hours - lag);
        if (want == stripLevel[s]) continue;
        // One step at a time (a big time jump still walks the levels in order)
        int next = want > stripLevel[s] ? stripLevel[s] + 1 : stripLevel[s] - 1;
        if (!stepped) { t0 = TideNowMs(); stepped = true; }
        for (int i = stripStart[s]; i < stripStart[s + 1]; i++) {
            int x = seaCells[i].x, y = seaCells[i].y;
            const WaterCell* wc = &WATER(x, y, z);
            if (!wc->isSea || wc->isFrozen) continue;
            int old = wc->level;
            if (old == next) continue;
            SetWaterLevel(x, y, z, next);
            // Crossing wading depth changes who can walk there
            if ((old >= WATER_BLOCKS_MOVEMENT) != (next >= WATER_BLOCKS_MOVEMENT)) MarkChunkDirty(x, y, z);
        }
        stripLevel[s] = next;
        tideSteps++;
    }
    tideLevel = TideFrontLevel();
    if (stepped) {
        tideLastStepMs = TideNowMs() - t0;
        if (tideLastStepMs > tideMaxStepMs) tideMaxStepMs = tideLastStepMs;
    }
}
