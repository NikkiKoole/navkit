// pipes.c - the pipe layer and its network pass (see pipes.h)
#include "pipes.h"
#include "grid.h"
#include <stdlib.h>
#include <string.h>

uint8_t* pipeGrid = NULL;       // allocated in grid_layers.c
uint8_t* pipeConnGrid = NULL;
bool pipeOverlay = false;       // setting pipeOverlay: draw pipes under floors too

// Every cell with a pipe byte, so the network pass never sweeps the whole map
typedef struct { int16_t x, y, z; } PipeCell;
static PipeCell* pipeCells = NULL;
static int pipeCellCount = 0, pipeCellCap = 0;
static int drainCells = 0;
static bool pipesDirty = false;

void ClearPipes(void) {
    if (pipeGrid) CLEAR_GRID_LAYER(pipeGrid, 0);
    if (pipeConnGrid) CLEAR_GRID_LAYER(pipeConnGrid, 0);
    pipeCellCount = 0;
    drainCells = 0;
    pipesDirty = false;
}

uint8_t GetPipe(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return 0;
    return PIPE(x, y, z);
}

static void PipeListAdd(int x, int y, int z) {
    if (pipeCellCount == pipeCellCap) {
        pipeCellCap = pipeCellCap ? pipeCellCap * 2 : 256;
        pipeCells = (PipeCell*)realloc(pipeCells, (size_t)pipeCellCap * sizeof(PipeCell));
    }
    pipeCells[pipeCellCount++] = (PipeCell){ (int16_t)x, (int16_t)y, (int16_t)z };
}

static void PipeListRemove(int x, int y, int z) {
    for (int i = 0; i < pipeCellCount; i++)
        if (pipeCells[i].x == x && pipeCells[i].y == y && pipeCells[i].z == z) {
            pipeCells[i] = pipeCells[--pipeCellCount];
            return;
        }
}

void SetPipe(int x, int y, int z, uint8_t bits) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return;
    uint8_t old = PIPE(x, y, z);
    if (old == bits) return;
    if (!old) PipeListAdd(x, y, z);
    else if (!bits) PipeListRemove(x, y, z);
    drainCells += ((bits & PIPE_DRAIN) != 0) - ((old & PIPE_DRAIN) != 0);
    PIPE(x, y, z) = bits;
    pipesDirty = true;
}

int PipeDrainCellCount(void) { return drainCells; }

// The drain network pass: from every outlet (END + a drain bit) outward over drain cells.
// Same level: the 4 neighbours with a drain bit. A wall drain with STACK runs on to the STACK
// wall drain above. Never down from where the search is: a drain doesn't run uphill on its way
// to the outlet (searching from the outlet, the fixture side may only lie level or higher).
// A shut valve passes nothing on (its own cell still counts as reached).
static void PipeNetworkPass(void) {
    pipesDirty = false;
    for (int i = 0; i < pipeCellCount; i++) PIPE_CONN(pipeCells[i].x, pipeCells[i].y, pipeCells[i].z) = 0;
    if (pipeCellCount == 0) return;
    PipeCell* queue = (PipeCell*)malloc((size_t)pipeCellCount * sizeof(PipeCell));
    int head = 0, tail = 0;
    for (int i = 0; i < pipeCellCount; i++) {
        PipeCell c = pipeCells[i];
        uint8_t b = PIPE(c.x, c.y, c.z);
        if ((b & PIPE_END) && (b & PIPE_DRAIN)) {
            PIPE_CONN(c.x, c.y, c.z) |= PIPE_CONN_DRAIN;
            queue[tail++] = c;
        }
    }
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    while (head < tail) {
        PipeCell c = queue[head++];
        uint8_t b = PIPE(c.x, c.y, c.z);
        if (b & PIPE_VALVE_CLOSED) continue;
        for (int d = 0; d < 5; d++) {
            int nx = c.x, ny = c.y, nz = c.z;
            if (d < 4) { nx += dx[d]; ny += dy[d]; }
            else {
                if (!((b & PIPE_STACK) && (b & PIPE_WALL_DRAIN))) continue;
                nz = c.z + 1;   // up the stack only (see above)
            }
            uint8_t nb = GetPipe(nx, ny, nz);
            if (!(nb & PIPE_DRAIN) || (PIPE_CONN(nx, ny, nz) & PIPE_CONN_DRAIN)) continue;
            if (d == 4 && !((nb & PIPE_STACK) && (nb & PIPE_WALL_DRAIN))) continue;
            PIPE_CONN(nx, ny, nz) |= PIPE_CONN_DRAIN;
            queue[tail++] = (PipeCell){ (int16_t)nx, (int16_t)ny, (int16_t)nz };
        }
    }
    free(queue);
}

bool PipeDrainConnected(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    if (pipesDirty) PipeNetworkPass();
    return (PIPE_CONN(x, y, z) & PIPE_CONN_DRAIN) != 0;
}

bool PipeFixtureOnDrain(int x, int y, int z) {
    if ((GetPipe(x, y, z) & PIPE_FLOOR_DRAIN) && PipeDrainConnected(x, y, z)) return true;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int d = 0; d < 4; d++) {
        int nx = x + dx[d], ny = y + dy[d];
        if ((GetPipe(nx, ny, z) & PIPE_WALL_DRAIN) && PipeDrainConnected(nx, ny, z)) return true;
    }
    return false;
}

static int LayDrainCell(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return 0;
    uint8_t b = PIPE(x, y, z);
    if (b & PIPE_FLOOR_DRAIN) return 0;
    SetPipe(x, y, z, b | PIPE_FLOOR_DRAIN);
    return 1;
}

int LayFloorDrain(int x1, int y1, int x2, int y2, int z) {
    int n = 0;
    int sx = x2 >= x1 ? 1 : -1, sy = y2 >= y1 ? 1 : -1;
    for (int x = x1;; x += sx) {
        n += LayDrainCell(x, y1, z);
        if (x == x2) break;
    }
    for (int y = y1;; y += sy) {
        n += LayDrainCell(x2, y, z);
        if (y == y2) break;
    }
    return n;
}
