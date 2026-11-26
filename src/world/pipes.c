// pipes.c - the pipe layer and its network pass (see pipes.h)
#include "pipes.h"
#include "grid.h"
#include "../core/time.h"
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
    memset(pipePulses, 0, sizeof(pipePulses));
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

void PipesAfterLoad(void) {
    pipeCellCount = 0;
    drainCells = 0;
    memset(pipePulses, 0, sizeof(pipePulses));
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                uint8_t b = PIPE(x, y, z);
                if (!b) continue;
                PipeListAdd(x, y, z);
                if (b & PIPE_DRAIN) drainCells++;
            }
    pipesDirty = true;
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

// ---- Flush pulses (drawing only) ----
PipePulse pipePulses[PIPE_PULSE_MAX];

static int PipeListIndex(int x, int y, int z) {
    for (int i = 0; i < pipeCellCount; i++)
        if (pipeCells[i].x == x && pipeCells[i].y == y && pipeCells[i].z == z) return i;
    return -1;
}

void PipeFlushPulse(int x, int y, int z) {
    // The fixture's drain cell: its own floor drain, else a connected wall drain next to it
    int sx = -1, sy = -1;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    if ((GetPipe(x, y, z) & PIPE_FLOOR_DRAIN) && PipeDrainConnected(x, y, z)) { sx = x; sy = y; }
    for (int d = 0; d < 4 && sx < 0; d++)
        if ((GetPipe(x + dx[d], y + dy[d], z) & PIPE_WALL_DRAIN) && PipeDrainConnected(x + dx[d], y + dy[d], z)) { sx = x + dx[d]; sy = y + dy[d]; }
    if (sx < 0 || pipeCellCount == 0) return;
    // Breadth first over connected drain cells to the nearest outlet (level, or down a stack)
    int n = pipeCellCount;
    int* parent = (int*)malloc((size_t)n * sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) parent[i] = -2;
    int start = PipeListIndex(sx, sy, z), head = 0, tail = 0, goal = -1;
    if (start >= 0) { parent[start] = -1; queue[tail++] = start; }
    while (head < tail && goal < 0) {
        int ci = queue[head++];
        PipeCell c = pipeCells[ci];
        uint8_t b = PIPE(c.x, c.y, c.z);
        if (b & PIPE_END) { goal = ci; break; }
        for (int d = 0; d < 5; d++) {
            int nx = c.x, ny = c.y, nz = c.z;
            if (d < 4) { nx += dx[d]; ny += dy[d]; }
            else { if (!((b & PIPE_STACK) && (b & PIPE_WALL_DRAIN))) continue; nz = c.z - 1; }
            if (!(GetPipe(nx, ny, nz) & PIPE_DRAIN) || !PipeDrainConnected(nx, ny, nz)) continue;
            int ni = PipeListIndex(nx, ny, nz);
            if (ni < 0 || parent[ni] != -2) continue;
            parent[ni] = ci;
            queue[tail++] = ni;
        }
    }
    if (goal >= 0) {
        int k = 0;   // a free slot, else the oldest
        for (int i = 0; i < PIPE_PULSE_MAX; i++) {
            if (!pipePulses[i].active) { k = i; break; }
            if (pipePulses[i].start < pipePulses[k].start) k = i;
        }
        PipePulse* p = &pipePulses[k];
        int len = 0;
        for (int ci = goal; ci >= 0 && len < PIPE_PULSE_CELLS; ci = parent[ci]) len++;
        p->len = len;
        int i = len - 1;   // walked back from the outlet: fill from the end
        for (int ci = goal; ci >= 0 && i >= 0; ci = parent[ci], i--) {
            p->x[i] = pipeCells[ci].x; p->y[i] = pipeCells[ci].y; p->z[i] = pipeCells[ci].z;
        }
        p->start = gameTime;
        p->active = true;
    }
    free(parent);
    free(queue);
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
