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
// Water network (supply): component per pipe cell (index into pipeCells), openings with theirs
static int* pipeComp = NULL;
static int pipeCompCap = 0;
typedef struct { int16_t x, y, z; int comp; } PipeOpening;
static PipeOpening* pipeOpenings = NULL;
static int pipeOpeningCount = 0, pipeOpeningCap = 0;
static void PipeNetworkPass(void);

void ClearPipes(void) {
    if (pipeGrid) CLEAR_GRID_LAYER(pipeGrid, 0);
    if (pipeConnGrid) CLEAR_GRID_LAYER(pipeConnGrid, 0);
    pipeCellCount = 0;
    drainCells = 0;
    pipeOpeningCount = 0;
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
    pipeOpeningCount = 0;
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

int PipeCellCount(void) { return pipeCellCount; }
int PipeDrainCellCount(void) { return drainCells; }

static int PipeListIndexOf(int x, int y, int z);

// The water (supply) network pass: label the connected supply pipes (4 neighbours on a level,
// STACK wall pipes up and down), a shut valve is reached but passes nothing on (a network of
// its own beyond it). Openings = supply ENDs; cells of a network with 2+ openings are
// PIPE_CONN_SUPPLY (water can go through).
static void PipeWaterPass(void) {
    pipeOpeningCount = 0;
    if (pipeCompCap < pipeCellCount) {
        pipeCompCap = pipeCellCount + 256;
        pipeComp = (int*)realloc(pipeComp, (size_t)pipeCompCap * sizeof(int));
    }
    for (int i = 0; i < pipeCellCount; i++) pipeComp[i] = -1;
    int* queue = (int*)malloc((size_t)(pipeCellCount > 0 ? pipeCellCount : 1) * sizeof(int));
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int comps = 0;
    for (int s = 0; s < pipeCellCount; s++) {
        PipeCell c0 = pipeCells[s];
        if (!(PIPE(c0.x, c0.y, c0.z) & PIPE_SUPPLY) || pipeComp[s] >= 0) continue;
        int head = 0, tail = 0, comp = comps++;
        pipeComp[s] = comp;
        queue[tail++] = s;
        while (head < tail) {
            PipeCell c = pipeCells[queue[head++]];
            uint8_t b = PIPE(c.x, c.y, c.z);
            if (b & PIPE_VALVE_CLOSED) continue;
            for (int d = 0; d < 6; d++) {
                int nx = c.x, ny = c.y, nz = c.z;
                if (d < 4) { nx += dx[d]; ny += dy[d]; }
                else {
                    if (!((b & PIPE_STACK) && (b & PIPE_WALL_SUPPLY))) continue;
                    nz += d == 4 ? 1 : -1;
                }
                uint8_t nb = GetPipe(nx, ny, nz);
                if (!(nb & PIPE_SUPPLY)) continue;
                if (d >= 4 && !((nb & PIPE_STACK) && (nb & PIPE_WALL_SUPPLY))) continue;
                if (nb & PIPE_VALVE_CLOSED) continue;   // a shut valve: not entered from here
                int ni = PipeListIndexOf(nx, ny, nz);
                if (ni < 0 || pipeComp[ni] >= 0) continue;
                pipeComp[ni] = comp;
                queue[tail++] = ni;
            }
        }
    }
    free(queue);
    int* openingsPer = (int*)calloc((size_t)(comps > 0 ? comps : 1), sizeof(int));
    for (int i = 0; i < pipeCellCount; i++) {
        PipeCell c = pipeCells[i];
        uint8_t b = PIPE(c.x, c.y, c.z);
        if (!(b & PIPE_SUPPLY) || !(b & PIPE_END) || pipeComp[i] < 0) continue;
        if (pipeOpeningCount == pipeOpeningCap) {
            pipeOpeningCap = pipeOpeningCap ? pipeOpeningCap * 2 : 32;
            pipeOpenings = (PipeOpening*)realloc(pipeOpenings, (size_t)pipeOpeningCap * sizeof(PipeOpening));
        }
        pipeOpenings[pipeOpeningCount++] = (PipeOpening){ c.x, c.y, c.z, pipeComp[i] };
        openingsPer[pipeComp[i]]++;
        PIPE_CONN(c.x, c.y, c.z) |= PIPE_CONN_OPENING;
    }
    for (int i = 0; i < pipeCellCount; i++)
        if (pipeComp[i] >= 0 && openingsPer[pipeComp[i]] >= 2)
            PIPE_CONN(pipeCells[i].x, pipeCells[i].y, pipeCells[i].z) |= PIPE_CONN_SUPPLY;
    free(openingsPer);
    // A network changed (a valve opened, a pipe laid): water resting at the openings looks again
    for (int i = 0; i < pipeOpeningCount; i++) DestabilizeWater(pipeOpenings[i].x, pipeOpenings[i].y, pipeOpenings[i].z);
}

static int PipeListIndexOf(int x, int y, int z) {
    for (int i = 0; i < pipeCellCount; i++)
        if (pipeCells[i].x == x && pipeCells[i].y == y && pipeCells[i].z == z) return i;
    return -1;
}

int PipeOpeningCount(void) {
    if (pipesDirty) PipeNetworkPass();
    return pipeOpeningCount;
}

bool PipeIsOpening(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    if (pipesDirty) PipeNetworkPass();
    return (PIPE_CONN(x, y, z) & PIPE_CONN_OPENING) != 0;
}

int PipeOtherOpenings(int x, int y, int z, int* ox, int* oy, int* oz, int max) {
    if (!PipeIsOpening(x, y, z)) return 0;
    int comp = -1;
    for (int i = 0; i < pipeOpeningCount; i++)
        if (pipeOpenings[i].x == x && pipeOpenings[i].y == y && pipeOpenings[i].z == z) { comp = pipeOpenings[i].comp; break; }
    int n = 0;
    for (int i = 0; i < pipeOpeningCount && n < max; i++) {
        const PipeOpening* o = &pipeOpenings[i];
        if (o->comp != comp || (o->x == x && o->y == y && o->z == z)) continue;
        ox[n] = o->x; oy[n] = o->y; oz[n] = o->z; n++;
    }
    return n;
}

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
    PipeWaterPass();
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

static int LayPipeCell(int x, int y, int z, uint8_t bit) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return 0;
    uint8_t b = PIPE(x, y, z);
    if (b & bit) return 0;
    SetPipe(x, y, z, b | bit);
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

static int LayFloorPipe(int x1, int y1, int x2, int y2, int z, uint8_t bit) {
    int n = 0;
    int sx = x2 >= x1 ? 1 : -1, sy = y2 >= y1 ? 1 : -1;
    for (int x = x1;; x += sx) {
        n += LayPipeCell(x, y1, z, bit);
        if (x == x2) break;
    }
    for (int y = y1;; y += sy) {
        n += LayPipeCell(x2, y, z, bit);
        if (y == y2) break;
    }
    return n;
}

int LayFloorDrain(int x1, int y1, int x2, int y2, int z) { return LayFloorPipe(x1, y1, x2, y2, z, PIPE_FLOOR_DRAIN); }
int LayFloorSupply(int x1, int y1, int x2, int y2, int z) { return LayFloorPipe(x1, y1, x2, y2, z, PIPE_FLOOR_SUPPLY); }
