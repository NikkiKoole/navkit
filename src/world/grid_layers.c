// grid_layers.c - allocation of every per-cell layer (step 2 of
// docs/todo/architecture/chunked-world-plan.md).
//
// All layers live in one "world arena": a single zeroed allocation sized for the current map
// (gridLayerCells elements per layer, see CellIndex in grid.h). When the map size changes,
// AllocGridLayers drops the whole arena and allocates a new one. Fresh zero pages only cost RAM
// once touched; after a size change, layers whose empty value isn't zero are filled with it.
//
// On POSIX the arena is an anonymous mmap with every layer page-aligned, so clearing a layer to
// zero (ClearGridLayer with 0, which every world reset does for most layers) replaces its pages
// with fresh zero pages instead of writing zeros: the old pages go back to the OS and the layer
// costs nothing until something is written. A 2048 map's layers are ~5 GB; memset-clearing them
// all made the process footprint 6.8 GB. Windows keeps calloc + memset.
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
#include "../simulation/vapour.h"
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
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

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


// ---- Sparse layers ----
static const Designation emptyDesignation = { .type = DESIGNATION_NONE, .assignedMover = -1 };
SparseLayer designationLayer = { NULL, 0, sizeof(Designation), &emptyDesignation, 0, "designations" };

static const WaterCell emptyWater = { .stable = 1 };   // dry and stable: UpdateWater skips it
SparseLayer waterLayer = { NULL, 0, sizeof(WaterCell), &emptyWater, 0, "water" };

// Layers whose empty value is all zero share one zero block as their empty value.
static const unsigned char zeroElem[64];
_Static_assert(sizeof(FireCell) <= 64 && sizeof(SmokeCell) <= 64 && sizeof(SteamCell) <= 64 &&
               sizeof(FarmCell) <= 64, "zeroElem too small");
SparseLayer fireLayer  = { NULL, 0, sizeof(FireCell),  zeroElem, 0, "fire" };
SparseLayer smokeLayer = { NULL, 0, sizeof(SmokeCell), zeroElem, 0, "smoke" };
SparseLayer steamLayer = { NULL, 0, sizeof(SteamCell), zeroElem, 0, "steam" };
SparseLayer vapourLayer = { NULL, 0, sizeof(VapourCell), zeroElem, 0, "vapour" };
SparseLayer farmLayer  = { NULL, 0, sizeof(FarmCell),  zeroElem, 0, "farm" };

SparseLayer waterFlowLayer = { NULL, 0, sizeof(WaterFlowCell), zeroElem, 0, "waterFlow" };
SparseLayer oneWayLayer    = { NULL, 0, sizeof(uint8_t), zeroElem, 0, "oneWay" };

static SparseLayer* const sparseLayers[] = { &designationLayer, &waterLayer, &fireLayer, &smokeLayer,
                                             &steamLayer, &farmLayer, &waterFlowLayer, &oneWayLayer, &vapourLayer };
#define SPARSE_LAYER_COUNT ((int)(sizeof(sparseLayers) / sizeof(sparseLayers[0])))

void* SparseLayerAllocBlock(SparseLayer* L, int chunk) {
    size_t bytes = (size_t)STORE_CHUNK_CELLS * L->elemSize;
    unsigned char* b = (unsigned char*)malloc(bytes);
    if (!b) { fprintf(stderr, "SparseLayerAllocBlock: out of memory (%s)\n", L->name); abort(); }
    for (int i = 0; i < STORE_CHUNK_CELLS; i++) memcpy(b + (size_t)i * L->elemSize, L->empty, L->elemSize);
    L->blocks[chunk] = b;
    L->allocatedBlocks++;
    return b;
}

void SparseLayerClear(SparseLayer* L) {
    for (int i = 0; i < L->blockCount; i++) { free(L->blocks[i]); L->blocks[i] = NULL; }
    L->allocatedBlocks = 0;
}

// New map size: drop every block and size the block table to the new chunk count.
static void ResizeSparseLayers(void) {
    int count = storeChunksX * storeChunksY;
    for (int s = 0; s < SPARSE_LAYER_COUNT; s++) {
        SparseLayer* L = sparseLayers[s];
        SparseLayerClear(L);
        free(L->blocks);
        L->blocks = (void**)calloc((size_t)count, sizeof(void*));
        if (!L->blocks) { fprintf(stderr, "ResizeSparseLayers: out of memory\n"); abort(); }
        L->blockCount = count;
    }
}

#define LAYER_DEF(name, fill) { (void**)&(name), sizeof(*(name)), (fill), NULL, #name }
#define LAYER_DEF_FN(name, fn) { (void**)&(name), sizeof(*(name)), 0, (fn), #name }

// Empty values match what each system's own Clear/Init function writes.
static const GridLayerDef gridLayerDefs[] = {
    LAYER_DEF(grid, 0),
    LAYER_DEF(cellFlags, 0),
    LAYER_DEF(vegetationGrid, 0),
    LAYER_DEF(trackConnections, 0),
    LAYER_DEF(unexploredGrid, 0),
    LAYER_DEF(wallMaterial, MAT_NONE),
    LAYER_DEF(floorMaterial, MAT_NONE),
    LAYER_DEF(wallNatural, 0),
    LAYER_DEF(floorNatural, 0),
    LAYER_DEF(wallFinish, FINISH_ROUGH),
    LAYER_DEF(floorFinish, FINISH_ROUGH),
    LAYER_DEF(wallSourceItem, 0),     // stored XOR 0xFF: 0 = none
    LAYER_DEF(floorSourceItem, 0),
    LAYER_DEF(pressureVisitedGen, 0),
    LAYER_DEF(smokePressureVisited, 0),
    LAYER_DEF(smokeHasRisen, 0),
    LAYER_DEF(steamHasRisen, 0),
    LAYER_DEF(temperatureGrid, 0),
    LAYER_DEF(snowGrid, 0),
    LAYER_DEF(snowAccumGrid, 0),
    LAYER_DEF(wearGrid, 0),
    LAYER_DEF(growthTimer, 0),
    LAYER_DEF(targetHeight, 0),
    LAYER_DEF(treeHarvestState, 0),
    LAYER_DEF(floorDirtGrid, 0),
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

#ifndef _WIN32
static size_t ArenaPageSize(void) {
    static size_t page = 0;
    if (!page) page = (size_t)getpagesize();
    return page;
}
static void* ArenaMap(size_t bytes) {
    void* p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
static void ArenaUnmap(void* p, size_t bytes) { if (p) munmap(p, bytes); }

// Zero [start, start + bytes) inside the arena: whole pages are replaced with fresh zero pages,
// the partial pages at either end are memset. Returns false if remapping failed (caller memsets).
static bool ArenaZero(unsigned char* start, size_t bytes) {
    size_t page = ArenaPageSize();
    unsigned char* first = (unsigned char*)AlignUp((size_t)start, page);
    unsigned char* last = (unsigned char*)(((size_t)(start + bytes)) & ~(page - 1));
    if (last <= first) return false;   // smaller than a page: just memset
    void* r = mmap(first, (size_t)(last - first), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    if (r == MAP_FAILED) return false;
    memset(start, 0, (size_t)(first - start));
    memset(last, 0, (size_t)(start + bytes - last));
    return true;
}
#else
static size_t ArenaPageSize(void) { return 64; }
static void* ArenaMap(size_t bytes) { return calloc(1, bytes); }
static void ArenaUnmap(void* p, size_t bytes) { (void)bytes; free(p); }
static bool ArenaZero(unsigned char* start, size_t bytes) { (void)start; (void)bytes; return false; }
#endif

// Called by ClearGridLayer for fill byte 0 (grid.c): true if the layer was zeroed here.
bool GridLayerZeroFast(void* layer, size_t bytes) {
    unsigned char* p = (unsigned char*)layer;
    unsigned char* a = (unsigned char*)worldArena;
    if (!a || p < a || p + bytes > a + worldArenaBytes) return false;
    return ArenaZero(p, bytes);
}

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
    ResizeSparseLayers();

    // Every layer starts on a page boundary, so a whole layer can be remapped to zero pages.
    size_t page = ArenaPageSize();
    size_t total = 0;
    for (int i = 0; i < GRID_LAYER_COUNT; i++) {
        total = AlignUp(total, page) + gridLayerCells * gridLayerDefs[i].elemSize;
    }
    total = AlignUp(total, page);

    bool firstArena = (worldArena == NULL);
    ArenaUnmap(worldArena, worldArenaBytes);
    worldArena = ArenaMap(total);
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
        p = (unsigned char*)AlignUp((size_t)p, page);
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

// Resident memory per layer (pages actually backed by RAM), largest first. POSIX only.
void GridLayersReport(void) {
#ifndef _WIN32
    size_t page = ArenaPageSize();
    double residentMB[GRID_LAYER_COUNT], totalRes = 0;
    int order[GRID_LAYER_COUNT];
    for (int i = 0; i < GRID_LAYER_COUNT; i++) {
        unsigned char* start = (unsigned char*)*gridLayerDefs[i].ptr;
        size_t bytes = gridLayerCells * gridLayerDefs[i].elemSize;
        size_t pages = (bytes + page - 1) / page;
        char* vec = malloc(pages);
        size_t res = 0;
        if (vec && mincore(start, pages * page, vec) == 0) {
            for (size_t k = 0; k < pages; k++) res += (vec[k] & 1);
        }
        free(vec);
        residentMB[i] = (double)(res * page) / (1024.0 * 1024.0);
        totalRes += residentMB[i];
        order[i] = i;
    }
    for (int a = 0; a < GRID_LAYER_COUNT; a++)
        for (int b = a + 1; b < GRID_LAYER_COUNT; b++)
            if (residentMB[order[b]] > residentMB[order[a]]) { int t = order[a]; order[a] = order[b]; order[b] = t; }
    printf("Grid layers: %.0f MB resident of %.0f MB arena (%zu cells/layer)\n",
           totalRes, (double)worldArenaBytes / (1024.0 * 1024.0), gridLayerCells);
    for (int s = 0; s < SPARSE_LAYER_COUNT; s++) {
        const SparseLayer* L = sparseLayers[s];
        printf("  sparse %-15s %6d of %6d blocks, %.1f MB\n", L->name, L->allocatedBlocks, L->blockCount,
               (double)L->allocatedBlocks * STORE_CHUNK_CELLS * L->elemSize / (1024.0 * 1024.0));
    }
    for (int k = 0; k < GRID_LAYER_COUNT; k++) {
        int i = order[k];
        if (residentMB[i] < 0.5) break;
        double full = (double)(gridLayerCells * gridLayerDefs[i].elemSize) / (1024.0 * 1024.0);
        printf("  %-22s %8.1f / %8.1f MB\n", gridLayerDefs[i].name, residentMB[i], full);
    }
#endif
}

__attribute__((constructor)) static void GridLayersAtStartup(void) { AllocGridLayers(); }
