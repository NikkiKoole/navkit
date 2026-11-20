// grid_layers.c - allocation of every per-cell layer (step 2 of
// docs/todo/architecture/chunked-world-plan.md).
//
// All layers live in one "world arena": a single zeroed allocation sized for the current map
// (gridLayerCells elements per layer, see CellIndex in grid.h). When the map size changes,
// AllocGridLayers drops the whole arena and allocates a new one. Big calloc'd blocks are fresh
// zero pages that only cost RAM once touched; after a size change, layers whose empty value
// isn't zero are filled with it.
//
// The first arena is allocated before main() (GridLayersAtStartup) for the default map size and
// left all zero, exactly like the static arrays it replaces: some code (tests, ClearMovers)
// touches layers before any InitGrid call.
//
// Adding a layer: declare `T* name` (allocated here), add its LAYER(x, y, z) macro next to the
// extern, and add a row to gridLayerDefs below.
#include "grid.h"
#include "designations.h"
#include "material.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/weather.h"
#include "../simulation/groundwear.h"
#include "../simulation/trees.h"
#include "../simulation/floordirt.h"
#include "../simulation/farming.h"
#include "../simulation/lighting.h"
#include "../simulation/rooms.h"
#include "../entities/furniture.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Scratch layers that are private to their .c file (no header declares them).
extern float* snowAccumGrid;
extern uint16_t* pressureVisitedGen;
extern uint16_t* smokePressureVisited;
extern uint16_t* smokeHasRisen;
extern uint16_t* steamHasRisen;

int storeChunksX, storeChunksY;
size_t gridLayerCells;
int cellIndexTabX[MAX_GRID_WIDTH + 1];
int cellIndexTabY[MAX_GRID_HEIGHT + 1];

typedef struct {
    void** ptr;             // the layer's global pointer
    size_t elemSize;
    int fill;               // byte to fill a fresh layer with (0 = nothing to do)
    void (*fillFn)(void);   // or a function for non-byte empty values (NULL = use fill)
    const char* name;
} GridLayerDef;

static void FillEmptyWater(void) {
    WaterCell emptyStable;
    memset(&emptyStable, 0, sizeof(emptyStable));
    emptyStable.stable = 1;
    FILL_GRID_LAYER(waterGrid, &emptyStable);
}

static void FillEmptyDesignations(void) {
    Designation empty;
    memset(&empty, 0, sizeof(empty));
    empty.assignedMover = -1;
    FILL_GRID_LAYER(designations, &empty);
}

#define LAYER_DEF(name, fill) { (void**)&(name), sizeof(*(name)), (fill), NULL, #name }
#define LAYER_DEF_FN(name, fn) { (void**)&(name), sizeof(*(name)), 0, (fn), #name }

// Empty values match what each system's own Clear/Init function writes.
static const GridLayerDef gridLayerDefs[] = {
    LAYER_DEF(grid, 0),
    LAYER_DEF(cellFlags, 0),
    LAYER_DEF(vegetationGrid, 0),
    LAYER_DEF(trackConnections, 0),
    LAYER_DEF(exploredGrid, 1),
    LAYER_DEF(wallMaterial, MAT_NONE),
    LAYER_DEF(floorMaterial, MAT_NONE),
    LAYER_DEF(wallNatural, 0),
    LAYER_DEF(floorNatural, 0),
    LAYER_DEF(wallFinish, FINISH_ROUGH),
    LAYER_DEF(floorFinish, FINISH_ROUGH),
    LAYER_DEF(wallSourceItem, 0xFF),
    LAYER_DEF(floorSourceItem, 0xFF),
    LAYER_DEF_FN(waterGrid, FillEmptyWater),
    LAYER_DEF(pressureVisitedGen, 0),
    LAYER_DEF(fireGrid, 0),
    LAYER_DEF(smokeGrid, 0),
    LAYER_DEF(smokePressureVisited, 0),
    LAYER_DEF(smokeHasRisen, 0),
    LAYER_DEF(steamGrid, 0),
    LAYER_DEF(steamHasRisen, 0),
    LAYER_DEF(temperatureGrid, 0),
    LAYER_DEF(snowGrid, 0),
    LAYER_DEF(snowAccumGrid, 0),
    LAYER_DEF_FN(designations, FillEmptyDesignations),
    LAYER_DEF(wearGrid, 0),
    LAYER_DEF(growthTimer, 0),
    LAYER_DEF(targetHeight, 0),
    LAYER_DEF(treeHarvestState, 0),
    LAYER_DEF(floorDirtGrid, 0),
    LAYER_DEF(farmGrid, 0),
    LAYER_DEF(lightGrid, 0),
    LAYER_DEF(roomGrid, 0),
    LAYER_DEF(furnitureMoveCostGrid, 0),
    LAYER_DEF(capabilityGrid, 0),
};
#define GRID_LAYER_COUNT ((int)(sizeof(gridLayerDefs) / sizeof(gridLayerDefs[0])))

// The world arena: one mapping holding every layer.
static void* worldArena = NULL;
static size_t worldArenaBytes = 0;
static int arenaGridW = -1, arenaGridH = -1;

static size_t AlignUp(size_t n, size_t a) { return (n + a - 1) & ~(a - 1); }

static void BuildCellIndexTables(void) {
    for (int x = 0; x <= MAX_GRID_WIDTH; x++)
        cellIndexTabX[x] = (x >> STORE_CHUNK_SHIFT) * STORE_CHUNK_STRIDE + (x & STORE_MASK);
    for (int y = 0; y <= MAX_GRID_HEIGHT; y++)
        cellIndexTabY[y] = (y >> STORE_CHUNK_SHIFT) * storeChunksX * STORE_CHUNK_STRIDE +
                           ((y & STORE_MASK) << STORE_CHUNK_SHIFT);
}

void AllocGridLayers(void) {
    if (worldArena && gridWidth == arenaGridW && gridHeight == arenaGridH) return;

    storeChunksX = STORE_CHUNKS_FOR(gridWidth);
    storeChunksY = STORE_CHUNKS_FOR(gridHeight);
    gridLayerCells = (size_t)storeChunksX * storeChunksY * STORE_CHUNK_STRIDE;
    BuildCellIndexTables();

    size_t total = 0;
    for (int i = 0; i < GRID_LAYER_COUNT; i++) {
        total = AlignUp(total, 64) + gridLayerCells * gridLayerDefs[i].elemSize;
    }
    total = AlignUp(total, 64);

    bool firstArena = (worldArena == NULL);
    free(worldArena);
    worldArena = calloc(1, total);
    if (!worldArena) {
        fprintf(stderr, "AllocGridLayers: cannot map %zu MB for a %dx%d map\n",
                total >> 20, gridWidth, gridHeight);
        abort();
    }
    worldArenaBytes = total;
    arenaGridW = gridWidth;
    arenaGridH = gridHeight;

    unsigned char* p = (unsigned char*)worldArena;
    for (int i = 0; i < GRID_LAYER_COUNT; i++) {
        p = (unsigned char*)AlignUp((size_t)p, 64);
        *gridLayerDefs[i].ptr = p;
        p += gridLayerCells * gridLayerDefs[i].elemSize;
    }
    // Fresh pages are zero. After a size change, give the other layers their empty value (the
    // startup arena stays all zero, like the static arrays did before any Init call).
    if (firstArena) return;
    for (int i = 0; i < GRID_LAYER_COUNT; i++) {
        if (gridLayerDefs[i].fillFn) gridLayerDefs[i].fillFn();
        else if (gridLayerDefs[i].fill) {
            ClearGridLayer(*gridLayerDefs[i].ptr, gridLayerDefs[i].elemSize, gridLayerDefs[i].fill);
        }
    }
}

size_t GridLayersBytes(void) { return worldArenaBytes; }

__attribute__((constructor)) static void GridLayersAtStartup(void) { AllocGridLayers(); }
