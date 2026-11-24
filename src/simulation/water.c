#include "water.h"
#include "steam.h"
#include "temperature.h"
#include "balance.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include "tide.h"
#include <string.h>
#include <stdlib.h>

// Water grid storage - single buffer (falling sand style)

// Global state
bool waterEnabled = true;
bool waterEvaporationEnabled = true;
bool waterEdgeOutflow = false;
int waterEdgeSeaZ = -1;
float waterEvapInterval = 4.0f;  // 4.0 game-hours (was 10.0 game-seconds at dayLength=60)
int waterUpdateCount = 0;

// Speed multipliers for movers walking through water
float waterSpeedShallow = 0.85f;    // Level 1-2: slight slowdown (15%)
float waterSpeedMedium = 0.6f;      // Level 3-4: noticeable slowdown (40%)
float waterSpeedDeep = 0.35f;       // Level 5-7: major slowdown (65%)

// Mud speed multiplier
float mudSpeedMultiplier = 0.6f;    // 40% slowdown on muddy terrain

// Wetness sync interval (game-hours, how often water sets cell wetness on soil)
float wetnessSyncInterval = 0.8f;  // 0.8 game-hours (was 2.0 game-seconds)

// Internal accumulators
static float waterEvapAccum = 0.0f;
static float wetnessSyncAccum = 0.0f;
// Fair update order: the row (z * gridHeight + y) where the last tick ran out of budget. The
// next tick starts there, so with more unstable cells than WATER_MAX_UPDATES_PER_TICK every
// cell still gets its turn (a fixed bottom-to-top scan starved everything above the low ground)
static int waterResumeRow = 0;
static void WaterNavReset(void);   // forget pending nav reports (water depth crossings)

// Run bits: one per 16-cell run of a row (the unit of the scan's pre-check), set when a cell in it
// is woken (DestabilizeWater & co.), cleared by the scan when the run has nothing to do. The scan
// skips runs whose bit is clear, so stable water costs a bit test instead of reading 16 cells:
// the island scanned ~30,000 runs a tick to find its 4096 updates. Every run is woken on clear,
// on load and every WATER_RUN_RESCAN water ticks, a safety net for a wake that bypasses
// DestabilizeWater (tests set .stable directly).
#define WATER_RUN_RESCAN 256
static uint32_t* waterRunBits = NULL;
static size_t waterRunBitCount = 0;   // rows * storeChunksX for the map it was sized for
static bool waterRunAllAwake = true;  // set every bit before the next scan

static inline size_t WaterRunIndex(int x, int y, int z) {
    return ((size_t)z * gridHeight + y) * storeChunksX + (x >> STORE_CHUNK_SHIFT);
}
static inline void WaterWakeRun(int x, int y, int z) {
    size_t i = WaterRunIndex(x, y, z);
    if (i < waterRunBitCount) waterRunBits[i >> 5] |= 1u << (i & 31);
}
void WaterWakeAll(void) { waterRunAllAwake = true; }

// BFS queue for pressure propagation
typedef struct {
    int x, y, z;
} WaterPos;

static WaterPos pressureQueue[WATER_PRESSURE_SEARCH_LIMIT];
static int pressureParent[WATER_PRESSURE_SEARCH_LIMIT];   // queue index it was reached from, -1 = the start cell

// Generation-based visited tracking (avoids expensive memset each call)
uint16_t* pressureVisitedGen = NULL;   // per-cell scratch layer (allocated in grid_layers.c)
#define WATER_PRESSURE_VISITED(x, y, z) (pressureVisitedGen CELL_AT((x), (y), (z)))
static uint16_t currentPressureGen = 0;

// ---- Water flow (current), see water.h ----
float waterFlowHalfLife = 120.0f;
uint32_t waterFlowTick = 0;

// Fraction of a flow value left after `ticks` water ticks
static inline float WaterFlowFade(uint32_t ticks) {
    if (ticks == 0) return 1.0f;
    float hl = waterFlowHalfLife > 1.0f ? waterFlowHalfLife : 1.0f;
    if ((float)ticks > hl * 16.0f) return 0.0f;
    return exp2f(-(float)ticks / hl);
}

// One unit of water moved (dx, dy) through this cell
static void WaterFlowAdd(int x, int y, int z, int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    WaterFlowCell* f = (WaterFlowCell*)SparseLayerWrite(&waterFlowLayer, x, y, z);
    float fade = WaterFlowFade(waterFlowTick - f->tick);
    float fx = f->fx * fade + dx * WATER_FLOW_SCALE;
    float fy = f->fy * fade + dy * WATER_FLOW_SCALE;
    if (fx > 32767.0f) fx = 32767.0f; else if (fx < -32767.0f) fx = -32767.0f;
    if (fy > 32767.0f) fy = 32767.0f; else if (fy < -32767.0f) fy = -32767.0f;
    f->fx = (int16_t)fx;
    f->fy = (int16_t)fy;
    f->tick = waterFlowTick;
}

// Water moved one unit from (x, y, z) to the neighbour (nx, ny, nz): both cells carry it
static inline void WaterFlowMove(int x, int y, int z, int nx, int ny, int nz) {
    int dx = nx - x, dy = ny - y;
    if (dx == 0 && dy == 0) return;
    WaterFlowAdd(x, y, z, dx, dy);
    WaterFlowAdd(nx, ny, nz, dx, dy);
}

bool GetWaterFlow(int x, int y, int z, float* fx, float* fy) {
    *fx = *fy = 0.0f;
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return false;
    const WaterFlowCell* f = (const WaterFlowCell*)SparseLayerRead(&waterFlowLayer, x, y, z);
    if (f->fx == 0 && f->fy == 0) return false;
    float fade = WaterFlowFade(waterFlowTick - f->tick) / WATER_FLOW_SCALE;
    *fx = f->fx * fade;
    *fy = f->fy * fade;
    return *fx != 0.0f || *fy != 0.0f;
}

void ClearWaterFlow(void) {
    SparseLayerClear(&waterFlowLayer);
    waterFlowTick = 0;
}

// Initialize water system
void InitWater(void) {
    ClearWater();
}

// Clear all water
void ClearWater(void) {
    // Initialize with stable=1 so empty cells are skipped in UpdateWater.
    // WaterCell is a uint16_t bitfield: stable is bit 3, so value 0x0008.
    // Every cell empty and stable (the layer's empty value): drop all blocks
    SparseLayerClear(&waterLayer);
    ClearWaterFlow();
    waterUpdateCount = 0;
    waterEvapAccum = 0.0f;
    wetnessSyncAccum = 0.0f;
    waterActiveCells = 0;
    waterResumeRow = 0;
    waterRunAllAwake = true;
    WaterNavReset();
}

// Bounds check helper
static inline bool WaterInBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth && 
           y >= 0 && y < gridHeight && 
           z >= 0 && z < gridDepth;
}

// Check if water can exist in a cell (and flow into it)
static inline bool CanHoldWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    CellType cell = GRID(x, y, z);
    if (!CellAllowsFluids(cell)) return false;
    
    // Frozen water acts as a solid block for water flow
    if (WATER(x, y, z).isFrozen) return false;
    
    return true;
}

// Mark cell and neighbors as unstable
// A cell's water crossed wading depth (WATER_BLOCKS_MOVEMENT): who can walk there changed, so the
// pathfinder must hear of it. Every level write goes through here (the tide and freezing did
// this themselves; ordinary flow didn't, so a flood or a draining pool left the nav graph stale)
// A cell's water crossed wading depth (WATER_BLOCKS_MOVEMENT): who can walk there changed, so the
// pathfinder must hear of it (the tide and freezing did this themselves; ordinary flow didn't, so
// a flood or a draining pool left the nav graph stale: tide settlers waited for a path that was
// open). Flowing water flickers across the depth (a river cell 3 -> 4 -> 3 as units pass, ~180
// times a tick in survival), and each report re-plans nav chunks, so reports are batched: the
// first crossing in a window remembers what the pathfinder knew, and every WATER_NAV_FLUSH water
// ticks only cells whose state really differs are reported. A full table reports at once.
#define WATER_NAV_FLUSH 60
#define WATER_NAV_SLOTS 8192   // power of two
typedef struct { int x, y, z; uint32_t gen; bool knownDeep; } WaterNavPending;
static WaterNavPending waterNavPending[WATER_NAV_SLOTS];
static uint32_t waterNavGen = 1;     // slots with another gen are free
static int waterNavCount = 0;
long waterNavReports = 0;            // MarkChunkDirty calls made for water depth (stats)

static void WaterNavReset(void) {
    waterNavCount = 0;
    if (++waterNavGen == 0) { memset(waterNavPending, 0, sizeof(waterNavPending)); waterNavGen = 1; }
}

static void WaterNavFlush(void) {
    if (waterNavCount > 0) {
        for (int i = 0; i < WATER_NAV_SLOTS; i++) {
            WaterNavPending* e = &waterNavPending[i];
            if (e->gen != waterNavGen) continue;
            bool deep = WATER(e->x, e->y, e->z).level >= WATER_BLOCKS_MOVEMENT;
            if (deep != e->knownDeep) { MarkChunkDirty(e->x, e->y, e->z); waterNavReports++; }
        }
    }
    WaterNavReset();
}

static void WaterDepthChanged(int x, int y, int z, int oldLevel, int newLevel) {
    bool wasDeep = oldLevel >= WATER_BLOCKS_MOVEMENT;
    if (wasDeep == (newLevel >= WATER_BLOCKS_MOVEMENT)) return;
    uint32_t h = ((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u) ^ ((uint32_t)z * 83492791u);
    for (int probe = 0; probe < 16; probe++) {
        WaterNavPending* e = &waterNavPending[(h + (uint32_t)probe) & (WATER_NAV_SLOTS - 1)];
        if (e->gen == waterNavGen) {
            if (e->x == x && e->y == y && e->z == z) return;   // already pending: keeps what nav knew
            continue;
        }
        *e = (WaterNavPending){ x, y, z, waterNavGen, wasDeep };
        waterNavCount++;
        return;
    }
    MarkChunkDirty(x, y, z);   // no room: report now
    waterNavReports++;
}

void DestabilizeWater(int x, int y, int z) {
    if (WaterInBounds(x, y, z)) {
        WATER_MUT(x, y, z).stable = false;
        WaterWakeRun(x, y, z);
        // the neighbours' runs: the same row (x +-1 may cross into the next run), rows y +-1, z +-1
        if (x > 0) WaterWakeRun(x - 1, y, z);
        if (x < gridWidth - 1) WaterWakeRun(x + 1, y, z);
        if (y > 0) WaterWakeRun(x, y - 1, z);
        if (y < gridHeight - 1) WaterWakeRun(x, y + 1, z);
        if (z > 0) WaterWakeRun(x, y, z - 1);
        if (z < gridDepth - 1) WaterWakeRun(x, y, z + 1);
    }
    
    // 4 horizontal neighbors (orthogonal only, like DF pressure)
    if (WaterInBounds(x-1, y, z)) WATER_MUT(x-1, y, z).stable = false;
    if (WaterInBounds(x+1, y, z)) WATER_MUT(x+1, y, z).stable = false;
    if (WaterInBounds(x, y-1, z)) WATER_MUT(x, y-1, z).stable = false;
    if (WaterInBounds(x, y+1, z)) WATER_MUT(x, y+1, z).stable = false;
    
    // Above and below
    if (WaterInBounds(x, y, z-1)) WATER_MUT(x, y, z-1).stable = false;
    if (WaterInBounds(x, y, z+1)) WATER_MUT(x, y, z+1).stable = false;
}

// Displace water from a cell before placing a wall
void DisplaceWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return;
    
    int level = WATER(x, y, z).level;
    if (level == 0) return;
    
    // Push water to horizontal neighbors first, then up
    int dx[] = {-1, 1, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0};
    int dz[] = {0, 0, 0, 0, 1};
    
    for (int i = 0; i < 5 && level > 0; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        
        if (!WaterInBounds(nx, ny, nz)) continue;
        if (CellBlocksFluids(GRID(nx, ny, nz))) continue;
        
        int space = WATER_MAX_LEVEL - WATER(nx, ny, nz).level;
        if (space > 0) {
            int transfer = (level < space) ? level : space;
            AddWater(nx, ny, nz, transfer);
            level -= transfer;
        }
    }
    
    // Clear remaining water and destabilize neighbors
    SetWaterLevel(x, y, z, 0);
    SetWaterSource(x, y, z, false);
    SetWaterDrain(x, y, z, false);
    DestabilizeWater(x, y, z);
}

// Helper to check if a water cell is "active" (needs processing)
static inline bool WaterCellIsActive(int x, int y, int z) {
    const WaterCell *wc = &WATER(x, y, z);
    return wc->level > 0 || wc->isSource || wc->isDrain;
}

// Set water level at a cell
void SetWaterLevel(int x, int y, int z, int level) {
    if (!WaterInBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > WATER_MAX_LEVEL) level = WATER_MAX_LEVEL;
    
    int oldLevel = WATER(x, y, z).level;
    WATER_MUT(x, y, z).level = (uint8_t)level;
    WaterDepthChanged(x, y, z, oldLevel, level);
    
    // Update presence grid: track transition from inactive to active or vice versa
    bool wasActive = oldLevel > 0 || WATER(x, y, z).isSource || WATER(x, y, z).isDrain;
    bool isActive = level > 0 || WATER(x, y, z).isSource || WATER(x, y, z).isDrain;
    
    if (!wasActive && isActive) {
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        waterActiveCells--;
    }
    
    if (oldLevel != level) {
        WATER_MUT(x, y, z).pressureDirty = 1;
        DestabilizeWater(x, y, z);
    }
}

void SetSeaLevel(int x, int y, int z, int level) {
    if (!WaterInBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > WATER_MAX_LEVEL) level = WATER_MAX_LEVEL;
    WaterCell* c = &WATER_MUT(x, y, z);
    int oldLevel = c->level;
    if (oldLevel == level) return;
    bool wasActive = oldLevel > 0 || c->isSource || c->isDrain;
    c->level = (uint8_t)level;
    WaterDepthChanged(x, y, z, oldLevel, level);
    bool isActive = level > 0 || c->isSource || c->isDrain;
    if (!wasActive && isActive) waterActiveCells++;
    else if (wasActive && !isActive) waterActiveCells--;
    c->pressureDirty = 1;
    bool spill = false;
    static const int ndx[6] = {-1, 1, 0, 0, 0, 0}, ndy[6] = {0, 0, -1, 1, 0, 0}, ndz[6] = {0, 0, 0, 0, -1, 1};
    for (int i = 0; i < 6; i++) {
        int nx = x + ndx[i], ny = y + ndy[i], nz = z + ndz[i];
        if (!WaterInBounds(nx, ny, nz)) continue;
        const WaterCell* n = &WATER(nx, ny, nz);
        if (n->isSea) continue;
        if (n->level > 0) { WATER_MUT(nx, ny, nz).stable = false; WaterWakeRun(nx, ny, nz); }
        // An empty or lower non-sea neighbour it could flow into (a pool behind the beach, a
        // ramp at high tide, a hole below): the sea cell itself must run to spread into it
        if (nz <= z && n->level < level && CanHoldWater(nx, ny, nz)) spill = true;
    }
    c->stable = !spill;
    if (spill) WaterWakeRun(x, y, z);
}

// Add water to a cell
void AddWater(int x, int y, int z, int amount) {
    if (!WaterInBounds(x, y, z)) return;
    int newLevel = WATER(x, y, z).level + amount;
    SetWaterLevel(x, y, z, newLevel);
}

// Remove water from a cell
void RemoveWater(int x, int y, int z, int amount) {
    if (!WaterInBounds(x, y, z)) return;
    int newLevel = WATER(x, y, z).level - amount;
    SetWaterLevel(x, y, z, newLevel);
}

// Set water source
void SetWaterSource(int x, int y, int z, bool isSource) {
    if (!WaterInBounds(x, y, z)) return;
    
    bool wasActive = WaterCellIsActive(x, y, z);
    WATER_MUT(x, y, z).isSource = isSource;
    bool isActive = WaterCellIsActive(x, y, z);
    
    if (!wasActive && isActive) {
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        waterActiveCells--;
    }
    
    if (isSource) {
        WATER_MUT(x, y, z).pressureDirty = 1;
        DestabilizeWater(x, y, z);
    }
}

// Set water drain
void SetWaterDrain(int x, int y, int z, bool isDrain) {
    if (!WaterInBounds(x, y, z)) return;
    
    bool wasActive = WaterCellIsActive(x, y, z);
    WATER_MUT(x, y, z).isDrain = isDrain;
    WaterWakeRun(x, y, z);
    bool isActive = WaterCellIsActive(x, y, z);
    
    if (!wasActive && isActive) {
        waterActiveCells++;
    } else if (wasActive && !isActive) {
        waterActiveCells--;
    }
    
    if (isDrain) {
        WATER_MUT(x, y, z).pressureDirty = 1;
        DestabilizeWater(x, y, z);
    }
}

// Get water level
int GetWaterLevel(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return 0;
    return WATER(x, y, z).level;
}

// Check if cell has water
bool HasWater(int x, int y, int z) {
    return GetWaterLevel(x, y, z) > 0;
}

// Check if cell is full (7/7)
bool IsFull(int x, int y, int z) {
    return GetWaterLevel(x, y, z) >= WATER_MAX_LEVEL;
}

// Check if underwater at minimum depth
bool IsUnderwater(int x, int y, int z, int minDepth) {
    return GetWaterLevel(x, y, z) >= minDepth;
}

bool IsWaterSourceAt(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    return WATER(x, y, z).isSource;
}

bool IsWaterDrainAt(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    return WATER(x, y, z).isDrain;
}

// Get speed multiplier for water (DF-style 1-7 scale)
float GetWaterSpeedMultiplier(int x, int y, int z) {
    int level = GetWaterLevel(x, y, z);
    if (level == 0) return 1.0f;
    
    // 1-2: shallow, slight slowdown
    // 3-4: medium, noticeable slowdown
    // 5-7: deep, major slowdown
    if (level <= 2) return waterSpeedShallow;
    if (level <= 4) return waterSpeedMedium;
    return waterSpeedDeep;
}

// =============================================================================
// FALLING SAND STYLE WATER SIMULATION
// Single buffer, randomized spread direction, bottom-to-top processing
// Priority: 1. Falling  2. Spreading  3. Pressure
// =============================================================================

// Phase 1: FALLING - Water drops down if cell below isn't full
// Returns amount of water that fell
static int TryFall(int x, int y, int z) {
    if (z <= 0) return 0;  // Already at bottom
    if (HAS_FLOOR(x, y, z)) return 0;  // Constructed floor blocks falling
    if (!CanHoldWater(x, y, z - 1)) return 0;  // Wall below
    
    WaterCell* src = &WATER_MUT(x, y, z);
    WaterCell* dst = &WATER_MUT(x, y, z-1);
    
    if (src->level == 0) return 0;
    
    int space = WATER_MAX_LEVEL - dst->level;
    if (space <= 0) {
        // Cell below is full - create pressure (water wants to fall but can't). Only when
        // that is news: re-marking a cell that already has exactly this pressure woke it
        // and its neighbours on every visit, so a full lake or sea never came to rest (on
        // the island 2.4 M of 4.1 M cell updates per 1000 ticks were full cells doing
        // nothing). A different height is still written (also a lower one: keeping a
        // higher one pushed water up hills)
        if (!dst->hasPressure || dst->pressureSourceZ != z) {
            dst->hasPressure = true;
            dst->pressureDirty = 1;
            dst->pressureSourceZ = z;
            DestabilizeWater(x, y, z - 1);
        }
        return 0;
    }
    
    int flow = src->level;
    if (flow > space) flow = space;
    
    // Track transitions for counter updates
    bool srcWasActive = src->level > 0;
    bool dstWasActive = dst->level > 0;
    
    src->level -= flow;
    dst->level += flow;
    WaterDepthChanged(x, y, z, src->level + flow, src->level);
    WaterDepthChanged(x, y, z - 1, dst->level - flow, dst->level);
    
    // Update counters for transitions
    if (srcWasActive && src->level == 0) {
        waterActiveCells--;
    }
    if (!dstWasActive && dst->level > 0) {
        waterActiveCells++;
    }
    
    // Falling water onto full water creates pressure
    if (dst->level == WATER_MAX_LEVEL) {
        dst->hasPressure = true;
        dst->pressureDirty = 1;
        dst->pressureSourceZ = z;
    }
    
    // Water moved — mark both cells pressure-dirty for nearby pressure recalc
    src->pressureDirty = 1;
    dst->pressureDirty = 1;
    DestabilizeWater(x, y, z);
    DestabilizeWater(x, y, z - 1);
    
    return flow;
}

// A one-level step (giver at level+1, receiver at level) is only worth a move when the unit can
// keep going: the receiver has a lower neighbour (not counting the giver), or water at the giver
// has been flowing that way (momentum, from the flow layer). So a slope of terraces
// (6 5 5 4 4 ...) still flattens, while a lone bump on a flat sheet runs on in a straight line
// until it meets lower water (and spreads) or a wall (and rests) instead of bouncing forever.
#define WATER_FLOW_MOMENTUM 0.5f   // current along the step (units per half-life) that carries it
static bool WaterStepCarries(int gx, int gy, int rx, int ry, int z, int level) {
    static const int ndx[4] = {-1, 1, 0, 0}, ndy[4] = {0, 0, -1, 1};
    for (int i = 0; i < 4; i++) {
        int nx = rx + ndx[i], ny = ry + ndy[i];
        if (nx == gx && ny == gy) continue;
        if (!CanHoldWater(nx, ny, z)) continue;
        const WaterCell* n = &WATER(nx, ny, z);
        if (!n->isDrain && n->level < level) return true;
    }
    float fx, fy;
    if (!GetWaterFlow(gx, gy, z, &fx, &fy)) return false;
    return fx * (rx - gx) + fy * (ry - gy) >= WATER_FLOW_MOMENTUM;
}

// Phase 2: SPREADING - Equalize water levels with orthogonal neighbors
// Falling sand style: randomize direction order to prevent directional bias
// Returns true if water moved
static bool WaterTrySpread(int x, int y, int z) {
    WaterCell* cell = &WATER_MUT(x, y, z);
    if (cell->level == 0) return false;
    
    // Orthogonal neighbors - randomize order each call
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    // Fisher-Yates shuffle for direction order
    int order[] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    
    bool moved = false;
    
    for (int i = 0; i < 4; i++) {
        int dir = order[i];
        int nx = x + dx[dir];
        int ny = y + dy[dir];
        
        if (!CanHoldWater(nx, ny, z)) continue;
        
        WaterCell* neighbor = &WATER_MUT(nx, ny, z);
        
        // Don't spread water into drains
        if (neighbor->isDrain) continue;
        
        int diff = cell->level - neighbor->level;
        
        // Spread to lower neighbors
        if (diff >= 2) {
            // Big difference: give 1 unit, keep going
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;
            WaterDepthChanged(x, y, z, cell->level + 1, cell->level);
            WaterDepthChanged(nx, ny, z, neighbor->level - 1, neighbor->level);
            
            // Track neighbor becoming active
            if (neighborWasEmpty) {
                waterActiveCells++;
            }
            
            cell->pressureDirty = 1;
            neighbor->pressureDirty = 1;
            DestabilizeWater(x, y, z);
            DestabilizeWater(nx, ny, z);
            WaterFlowMove(x, y, z, nx, ny, z);
            moved = true;
            
            // Stop if we're now at level 1
            if (cell->level <= 1) break;
        } else if (diff == 1 && cell->level > 1 &&
                   WaterStepCarries(x, y, nx, ny, z, neighbor->level)) {
            // Small difference: give 1 unit to ONE neighbor only (randomized), and only when the
            // unit can keep going downhill from there. This breaks the staircase pattern over
            // time; on flat water (a 2 among 1s) it would only swap the two cells forever
            // (WaterStepCarries)
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;
            WaterDepthChanged(x, y, z, cell->level + 1, cell->level);
            WaterDepthChanged(nx, ny, z, neighbor->level - 1, neighbor->level);
            
            // Track neighbor becoming active
            if (neighborWasEmpty) {
                waterActiveCells++;
            }
            
            cell->pressureDirty = 1;
            neighbor->pressureDirty = 1;
            DestabilizeWater(x, y, z);
            DestabilizeWater(nx, ny, z);
            WaterFlowMove(x, y, z, nx, ny, z);
            moved = true;
            
            // Only give to one neighbor when diff=1 to prevent oscillation
            break;
        }
    }
    
    return moved;
}

// Phase 3: PRESSURE - When water is full and under pressure, trace through
// full cells to find an outlet (BFS). Water can rise up to pressureSourceZ - 1.
// Returns true if water moved via pressure
static bool TryPressure(int x, int y, int z) {
    WaterCell* cell = &WATER_MUT(x, y, z);
    
    // Need full water with pressure to propagate
    if (cell->level < WATER_MAX_LEVEL) return false;
    if (!cell->hasPressure) return false;
    
    int maxZ = cell->pressureSourceZ - 1;
    if (maxZ < 0) maxZ = 0;
    
    // BFS to find nearest non-full cell through full cells
    // Use generation counter instead of memset (much faster)
    currentPressureGen++;
    if (currentPressureGen == 0) {
        // Handle wrap-around (rare) - clear the array once
        CLEAR_GRID_LAYER(pressureVisitedGen, 0);
        currentPressureGen = 1;
    }
    
    int queueHead = 0;
    int queueTail = 0;
    
    int dx[] = {-1, 1, 0, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0, 0};
    int dz[] = {0, 0, 0, 0, -1, 1};
    
    WATER_PRESSURE_VISITED(x, y, z) = currentPressureGen;
    
    // Add initial neighbors to queue
    for (int i = 0; i < 6; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];
        
        if (nz > maxZ) continue;
        if (!CanHoldWater(nx, ny, nz)) continue;
        if (WATER_PRESSURE_VISITED(nx, ny, nz) == currentPressureGen) continue;
        
        WATER_PRESSURE_VISITED(nx, ny, nz) = currentPressureGen;
        pressureParent[queueTail] = -1;
        pressureQueue[queueTail++] = (WaterPos){nx, ny, nz};
        
        if (queueTail >= WATER_PRESSURE_SEARCH_LIMIT) break;
    }
    
    // BFS through full cells looking for non-full cell
    while (queueHead < queueTail) {
        WaterPos pos = pressureQueue[queueHead++];
        WaterCell* current = &WATER_MUT(pos.x, pos.y, pos.z);
        
        // Found a non-full cell - push water here!
        if (current->level < WATER_MAX_LEVEL) {
            int space = WATER_MAX_LEVEL - current->level;
            int transfer = 1;
            if (transfer > space) transfer = space;
            if (transfer > cell->level) transfer = cell->level;
            
            if (transfer > 0) {
                bool currentWasEmpty = current->level == 0;
                cell->level -= transfer;
                current->level += transfer;
                WaterDepthChanged(x, y, z, cell->level + transfer, cell->level);
                WaterDepthChanged(pos.x, pos.y, pos.z, current->level - transfer, current->level);
                
                // Track destination becoming active
                if (currentWasEmpty) {
                    waterActiveCells++;
                }
                
                // Propagate pressure info
                if (current->level == WATER_MAX_LEVEL) {
                    current->hasPressure = true;
                    current->pressureDirty = 1;
                    current->pressureSourceZ = cell->pressureSourceZ;
                }
                
                cell->pressureDirty = 1;
                current->pressureDirty = 1;
                DestabilizeWater(x, y, z);
                DestabilizeWater(pos.x, pos.y, pos.z);

                // The unit went from the start cell through full water to here: every step of
                // that path carries the current (sideways steps only, WaterFlowMove skips dz)
                for (int k = queueHead - 1; k >= 0; k = pressureParent[k]) {
                    WaterPos to = pressureQueue[k];
                    WaterPos from = pressureParent[k] < 0 ? (WaterPos){x, y, z} : pressureQueue[pressureParent[k]];
                    WaterFlowMove(from.x, from.y, from.z, to.x, to.y, to.z);
                }
                
                // Clear pressure if we're no longer full
                if (cell->level < WATER_MAX_LEVEL) {
                    cell->hasPressure = false;
                }
                
                return true;
            }
        }
        
        // Cell is full - continue searching through it
        if (current->level >= WATER_MAX_LEVEL) {
            for (int i = 0; i < 6; i++) {
                int nx = pos.x + dx[i];
                int ny = pos.y + dy[i];
                int nz = pos.z + dz[i];
                
                if (nz > maxZ) continue;
                if (!CanHoldWater(nx, ny, nz)) continue;
                if (WATER_PRESSURE_VISITED(nx, ny, nz) == currentPressureGen) continue;
                
                WATER_PRESSURE_VISITED(nx, ny, nz) = currentPressureGen;
                
                if (queueTail < WATER_PRESSURE_SEARCH_LIMIT) {
                    pressureParent[queueTail] = queueHead - 1;
                    pressureQueue[queueTail++] = (WaterPos){nx, ny, nz};
                }
            }
        }
    }
    
    return false;
}

// Level of the water beyond the map edge at height z: a sea up to waterEdgeSeaZ, else none
static inline int WaterEdgeLevel(int z) {
    return z <= waterEdgeSeaZ ? WATER_MAX_LEVEL : 0;
}

static inline bool WaterOnEdge(int x, int y) {
    return x == 0 || y == 0 || x == gridWidth - 1 || y == gridHeight - 1;
}

// An edge cell trades with the water beyond the edge (one unit per visit, like spreading):
// above that level it flows off the map, below it it fills up. true = the level changed
static bool WaterEdgeExchange(int x, int y, int z) {
    if (WATER(x, y, z).isSea && tideEnabled) return false;   // the tide sets the sea's top level
    int level = WATER(x, y, z).level, beyond = WaterEdgeLevel(z);
    if (level == beyond || (level < beyond && !CanHoldWater(x, y, z))) return false;
    SetWaterLevel(x, y, z, level > beyond ? level - 1 : level + 1);
    // The current at the edge points off the map (outflow) or onto it (filling up)
    int ox = x == 0 ? -1 : x == gridWidth - 1 ? 1 : 0, oy = y == 0 ? -1 : y == gridHeight - 1 ? 1 : 0;
    if (level < beyond) { ox = -ox; oy = -oy; }
    WaterFlowAdd(x, y, z, ox, oy);
    return true;
}

// Process a single water cell with DF-style rules
static bool ProcessWaterCell(int x, int y, int z) {
    WaterCell* cell = &WATER_MUT(x, y, z);
    bool moved = false;

    // Beach surf (high tide): the tide sets it; it doesn't flow or evaporate (or it would creep
    // inland over the flat beach)
    if (cell->isSurf && tideEnabled) {
        cell->stable = true;
        return false;
    }

    // The sea follows the tide: back to its level (what flowed in or out is the sea's)
    if (cell->isSea && tideEnabled) {
        int lo, hi;
        TideCellRange(x, y, &lo, &hi);   // tide level .. tide + surf
        int l = cell->level, want = l < lo ? lo : l > hi ? hi : l;
        if (want != l) { SetSeaLevel(x, y, z, want); moved = true; }
    }
    
    // Handle sources - refill to max and add pressure
    if (cell->isSource) {
        if (cell->level < WATER_MAX_LEVEL) {
            WaterDepthChanged(x, y, z, cell->level, WATER_MAX_LEVEL);
            cell->level = WATER_MAX_LEVEL;
            cell->hasPressure = true;
            cell->pressureDirty = 1;
            cell->pressureSourceZ = z;
            DestabilizeWater(x, y, z);
            moved = true;
        }
    }
    
    // Handle drains - remove water
    if (cell->isDrain) {
        if (cell->level > 0) {
            WaterDepthChanged(x, y, z, cell->level, 0);
            cell->level = 0;
            cell->hasPressure = false;
            DestabilizeWater(x, y, z);
            return true;
        }
    }
    
    // The map edge: water above the level beyond it flows off, below it fills up
    if (waterEdgeOutflow && !cell->isSource && WaterOnEdge(x, y) && WaterEdgeExchange(x, y, z)) moved = true;

    // No water to process
    if (cell->level == 0) {
        if (moved) return true;
        cell->stable = true;
        cell->hasPressure = false;
        return false;
    }
    
    // Phase 1: Try to fall (highest priority)
    int fell = TryFall(x, y, z);
    if (fell > 0) moved = true;
    
    // Phase 2: Try to spread horizontally (if we still have water)
    if (cell->level > 0) {
        if (WaterTrySpread(x, y, z)) moved = true;
    }
    
    // Phase 3: Try pressure propagation (if full, pressurized, and dirty)
    if (cell->level >= WATER_MAX_LEVEL && cell->hasPressure && cell->pressureDirty) {
        if (TryPressure(x, y, z)) moved = true;
        cell->pressureDirty = 0;
    }
    
    // Sources refill AFTER spreading to maintain level 7
    if (cell->isSource) {
        WaterDepthChanged(x, y, z, cell->level, WATER_MAX_LEVEL);
        cell->level = WATER_MAX_LEVEL;
        cell->hasPressure = true;
        return true;
    }
    
    // Clear pressure if we're no longer full
    if (cell->level < WATER_MAX_LEVEL) {
        cell->hasPressure = false;
    }
    
    // Mark stable if nothing moved AND neighbors are balanced
    if (!moved) {
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        bool balanced = true;
        
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (WaterInBounds(nx, ny, z) && CanHoldWater(nx, ny, z)) {
                int neighborLevel = WATER(nx, ny, z).level;
                int diff = neighborLevel - cell->level;
                // Unbalanced if the neighbour would give us water (the same rule as spreading)
                if (diff >= 2 || (diff == 1 && neighborLevel > 1 &&
                                  WaterStepCarries(nx, ny, x, y, z, cell->level))) {
                    balanced = false;
                    break;
                }
            }
        }
        
        if (waterEdgeOutflow && !cell->isSource && WaterOnEdge(x, y) &&
            cell->level != WaterEdgeLevel(z) && !(cell->isSea && tideEnabled)) balanced = false;

        // Level-1 water may rest too: evaporation is a sweep of its own (WaterEvaporateSweep)
        if (balanced) cell->stable = true;
    }
    
    return moved;
}

// Evaporation, once per waterEvapInterval: every level-1 cell (a puddle, the edge of a film) has
// an even chance to dry up. A sweep of its own, so level-1 water can rest between sweeps
// instead of being processed every tick, and every puddle gets its chance (the update cap
// used to skip the ones late in the scan).
static void WaterEvaporateSweep(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            FOR_X_IN_SPARSE_ROW(waterLayer, WaterCell, x, wp, y, z) {
                if (wp->level != 1 || wp->isSource || wp->isFrozen) continue;
                if ((wp->isSea || wp->isSurf) && tideEnabled) continue;   // the tide's water
                if (rand() % 100 >= 50) continue;
                SetWaterLevel(x, y, z, 0);
                WATER_MUT(x, y, z).hasPressure = false;
            }
        }
    }
}

// Main water update - process all unstable cells
// Falling sand style: bottom-to-top, single buffer, randomized spread
void UpdateWater(void) {
    if (!waterEnabled) return;
    waterFlowTick++;
    if (waterFlowTick % WATER_NAV_FLUSH == 0) WaterNavFlush();
    TideTick();
    
    // Early exit: no water activity at all
    if (waterActiveCells == 0) {
        waterUpdateCount = 0;
        return;
    }
    
    waterUpdateCount = 0;
    
    // Accumulate game time for interval-based evaporation
    waterEvapAccum += gameDeltaTime;
    
    // Check if evaporation interval has elapsed
    float evapIntervalGS = GameHoursToGameSeconds(waterEvapInterval);
    bool doEvap = waterEvapAccum >= evapIntervalGS;
    if (doEvap) waterEvapAccum -= evapIntervalGS;
    if (doEvap && waterEvaporationEnabled) WaterEvaporateSweep();
    
    // Process rows bottom to top, starting where the last tick ran out of budget (fair update
    // order: see waterResumeRow). Within a tick the order stays bottom to top, wrapping once.
    int rows = gridDepth * gridHeight;
    size_t runBits = (size_t)rows * storeChunksX;
    if (runBits != waterRunBitCount) {   // first tick, or the map size changed
        free(waterRunBits);
        waterRunBits = (uint32_t*)malloc(((runBits + 31) / 32) * sizeof(uint32_t));
        waterRunBitCount = waterRunBits ? runBits : 0;
        waterRunAllAwake = true;
    }
    if (waterFlowTick % WATER_RUN_RESCAN == 0) waterRunAllAwake = true;
    if (waterRunAllAwake && waterRunBits) {
        memset(waterRunBits, 0xFF, ((waterRunBitCount + 31) / 32) * sizeof(uint32_t));
        waterRunAllAwake = false;
    }
    int start = (waterResumeRow >= 0 && waterResumeRow < rows) ? waterResumeRow : 0;
    waterResumeRow = 0;   // a full pass without running out: next tick starts at the bottom
    for (int k = 0; k < rows; k++) {
        int row = start + k < rows ? start + k : start + k - rows;
        int z = row / gridHeight, y = row - z * gridHeight;
        {
            // Chunks without a water block are all empty and stable: skipped by the iterator.
            FOR_X_IN_SPARSE_ROW(waterLayer, WaterCell, x, wp, y, z) {
                // At the start of each 16-cell run, skip the whole run if no cell in it would get
                // past the three skips below (frozen, or stable and neither source nor drain).
                if ((x & STORE_MASK) == 0) {
                    int runLen = gridWidth - x < STORE_CHUNK ? gridWidth - x : STORE_CHUNK;
                    size_t ri = WaterRunIndex(x, y, z);
                    if (waterRunBits && !(waterRunBits[ri >> 5] & (1u << (ri & 31)))) {
                        x += runLen - 1;   // asleep: nothing here was woken since it was found idle
                        continue;
                    }
                    unsigned anyWork = 0;
                    for (int k2 = 0; k2 < runLen; k2++) {
                        WaterCell c = wp[k2];
                        anyWork |= !c.isFrozen & (!c.stable | c.isSource | c.isDrain);
                    }
                    if (!anyWork) {
                        if (waterRunBits) waterRunBits[ri >> 5] &= ~(1u << (ri & 31));
                        x += runLen - 1;
                        continue;
                    }
                }
                WaterCell* cell = wp;
                
                // Skip frozen water - it doesn't flow
                if (cell->isFrozen) {
                    continue;
                }
                
                // Skip stable empty cells
                if (cell->stable && cell->level == 0 && !cell->isSource) {
                    continue;
                }
                
                // Skip stable cells (unless source/drain)
                if (cell->stable && !cell->isSource && !cell->isDrain) {
                    continue;
                }
                
                ProcessWaterCell(x, y, z);
                waterUpdateCount++;
                
                // Out of budget: the next tick starts at this row
                if (waterUpdateCount >= WATER_MAX_UPDATES_PER_TICK) {
                    waterResumeRow = row;
                    goto budgetSpent;
                }
            }
        }
    }
budgetSpent:
    
    // Sync water presence to cell wetness on soil (interval-based)
    wetnessSyncAccum += gameDeltaTime;
    float wetnessIntervalGS = GameHoursToGameSeconds(wetnessSyncInterval);
    if (wetnessSyncAccum >= wetnessIntervalGS) {
        wetnessSyncAccum -= wetnessIntervalGS;
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                FOR_X_IN_SPARSE_ROW(waterLayer, WaterCell, x, wp, y, z) {
                    int level = wp->level;
                    if (level <= 0) continue;
                    
                    // Set wetness on the soil cell below the water
                    // Water sits in the air cell; soil is at z-1
                    int soilZ = z - 1;
                    if (soilZ < 0) continue;
                    if (!CellIsSolid(GRID(x, y, soilZ))) continue;
                    if (!IsWallNatural(x, y, soilZ)) continue;
                    if (!IsSoilMaterial(GetWallMaterial(x, y, soilZ))) continue;
                    
                    // Map water level to wetness: 1→damp, 2-3→wet, 4+→soaked
                    int wetness = (level <= 1) ? 1 : (level <= 3) ? 2 : 3;
                    int current = GET_CELL_WETNESS(x, y, soilZ);
                    if (wetness > current) {
                        SET_CELL_WETNESS(x, y, soilZ, wetness);
                    }
                }
            }
        }
    }
}

// =============================================================================
// WATER FREEZING
// =============================================================================

// Check if water is frozen at a cell
bool IsWaterFrozen(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return false;
    return WATER(x, y, z).isFrozen;
}

// Freeze water at a cell (any water level can freeze)
void FreezeWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return;
    
    WaterCell* cell = &WATER_MUT(x, y, z);
    
    // No water to freeze
    if (cell->level == 0) return;
    
    // Already frozen
    if (cell->isFrozen) return;
    
    cell->isFrozen = true;
    cell->stable = true;  // Frozen water doesn't need processing
    cell->pressureDirty = 1;
    DestabilizeWater(x, y, z);  // But neighbors might be affected
    // Deep ice can be walked on: the cell above became walkable
    if (cell->level >= WATER_BLOCKS_MOVEMENT && z + 1 < gridDepth) MarkChunkDirty(x, y, z + 1);
}

// Thaw frozen water
void ThawWater(int x, int y, int z) {
    if (!WaterInBounds(x, y, z)) return;
    
    WaterCell* cell = &WATER_MUT(x, y, z);
    
    if (!cell->isFrozen) return;
    
    cell->isFrozen = false;
    cell->stable = false;  // Water can flow again
    cell->pressureDirty = 1;
    DestabilizeWater(x, y, z);
    // The ice is gone: the cell above isn't walkable any more
    if (cell->level >= WATER_BLOCKS_MOVEMENT && z + 1 < gridDepth) MarkChunkDirty(x, y, z + 1);
}

// Update water freezing and boiling based on temperature
// Call this after UpdateTemperature() in the game loop
// Big maps: checking every cell every tick cost ~6 ms at 1024x1024x16, so each tick
// checks one band of rows (all z levels); a cell freezes or thaws at most a sweep
// later. Maps up to FREEZE_CELLS_PER_TICK cells are one band: every cell, every tick.
#define FREEZE_CELLS_PER_TICK (256 * 1024)
#define FREEZE_MAX_BANDS 64
static int freezeBandNext = 0;

void UpdateWaterFreezing(void) {
    if (!waterEnabled) return;
    if (!temperatureEnabled) return;
    
    // Early exit: no water means nothing to freeze/thaw
    if (waterActiveCells == 0) return;
    
    long cells = (long)gridWidth * gridHeight * gridDepth;
    int bands = (int)((cells + FREEZE_CELLS_PER_TICK - 1) / FREEZE_CELLS_PER_TICK);
    if (bands < 1) bands = 1;
    if (bands > FREEZE_MAX_BANDS) bands = FREEZE_MAX_BANDS;
    if (bands > gridHeight) bands = gridHeight;
    int band = freezeBandNext % bands;
    freezeBandNext = (band + 1) % bands;
    int yStart = (int)((long)gridHeight * band / bands);
    int yEnd = (int)((long)gridHeight * (band + 1) / bands);
    
    for (int z = 0; z < gridDepth; z++) {
        // GetTemperature is ambient(z) + the cell's offset, clamped: ambient once per level
        int ambient = GetAmbientTemperature(z);
        for (int y = yStart; y < yEnd; y++) {
            FOR_X_IN_SPARSE_ROW(waterLayer, WaterCell, x, wp, y, z) {
                // Most of the map is dry: at the start of each 16-cell run, skip the whole run
                // if none of it holds water (same result as testing each cell below).
                if ((x & STORE_MASK) == 0) {
                    int runLen = gridWidth - x < STORE_CHUNK ? gridWidth - x : STORE_CHUNK;
                    unsigned anyLevel = 0;
                    for (int k = 0; k < runLen; k++) anyLevel |= wp[k].level;
                    if (anyLevel == 0) { x += runLen - 1; continue; }
                }
                WaterCell* cell = wp;
                
                // Skip empty cells
                if (cell->level == 0) continue;
                
                int temp = ambient + TEMPERATURE(x, y, z).offset;   // = GetTemperature (Celsius)
                if (temp < TEMP_MIN) temp = TEMP_MIN;
                if (temp > TEMP_MAX) temp = TEMP_MAX;
                // The sea doesn't freeze (salt, waves): the tide's sea, or water at or below the
                // sea beyond the map edge (the island's sea; lakes and rivers lie above it)
                bool sea = cell->isSea || z <= waterEdgeSeaZ;
                
                if (cell->isFrozen) {
                    // Check if should thaw (sea ice from before this rule thaws too)
                    if (temp > TEMP_WATER_FREEZES || sea) {
                        ThawWater(x, y, z);
                    }
                } else {
                    // Check if should freeze (any water level)
                    if (temp <= TEMP_WATER_FREEZES && cell->level > 0 && !sea) {
                        FreezeWater(x, y, z);
                    }
                    // Check if should boil (convert to steam)
                    else if (temp >= TEMP_BOILING && cell->level > 0) {
                        // Boil faster at higher temperatures
                        // At 100C: 1 level, at 200C: 2 levels, at 300C: 3 levels, etc.
                        // (visited once per sweep: boil a sweep's worth)
                        int boilRate = (1 + (temp - TEMP_BOILING) / 100) * bands;
                        if (boilRate > 3 * bands) boilRate = 3 * bands;  // Cap at 3 per tick
                        if (boilRate > cell->level) boilRate = cell->level;
                        
                        int newLevel = cell->level - boilRate;
                        SetWaterLevel(x, y, z, newLevel);
                        GenerateSteamFromBoilingWater(x, y, z, boilRate);
                    }
                }
            }
        }
    }
}

// =============================================================================
// SKY WATER (rain simulation)
// =============================================================================

// Ongoing rain state
static float rainTimeRemaining = 0.0f;
static float rainSpawnAccum = 0.0f;
static int rainIntensity = 0;         // cells per spawn wave
static float rainSpawnInterval = 0.12f; // game-hours between waves (was 0.3 game-seconds)

bool IsRaining(void) { return rainTimeRemaining > 0.0f; }

// Spawn a single wave of rain drops at high z
static void SpawnRainWave(int count) {
    int skyZ = gridDepth - 1;
    if (skyZ < 2) return;
    
    for (int i = 0; i < count; i++) {
        int gx = GetRandomValue(0, gridWidth - 1);
        int gy = GetRandomValue(0, gridHeight - 1);
        
        for (int z = skyZ; z >= skyZ - 1 && z >= 1; z--) {
            if (GRID(gx, gy, z) == CELL_AIR) {
                int level = GetRandomValue(4, 7);
                AddWater(gx, gy, z, level);
                break;
            }
        }
    }
}

void SpawnSkyWater(int coverage) {
    if (coverage < 1) coverage = 1;
    if (coverage > 100) coverage = 100;
    
    // Set up ongoing rain: spawns waves over 30 game-seconds
    int totalCells = gridWidth * gridHeight;
    // Cells per wave = coverage% of map / number of waves in duration
    float duration = 30.0f;
    float rainSpawnIntervalGS = GameHoursToGameSeconds(rainSpawnInterval);
    int totalWaves = (int)(duration / rainSpawnIntervalGS);
    if (totalWaves < 1) totalWaves = 1;
    int totalDrops = (totalCells * coverage) / 100;
    
    rainIntensity = totalDrops / totalWaves;
    if (rainIntensity < 1) rainIntensity = 1;
    rainTimeRemaining = duration;
    rainSpawnAccum = 0.0f;
}

void UpdateRain(void) {
    if (rainTimeRemaining <= 0.0f) return;
    
    rainTimeRemaining -= gameDeltaTime;
    rainSpawnAccum += gameDeltaTime;
    
    float rainSpawnIntervalGS = GameHoursToGameSeconds(rainSpawnInterval);
    if (rainSpawnAccum >= rainSpawnIntervalGS) {
        rainSpawnAccum -= rainSpawnIntervalGS;
        SpawnRainWave(rainIntensity);
    }
    
    if (rainTimeRemaining <= 0.0f) {
        rainTimeRemaining = 0.0f;
        rainIntensity = 0;
    }
}

void StopRain(void) {
    rainTimeRemaining = 0.0f;
    rainIntensity = 0;
}

float GetWaterEvapAccum(void) { return waterEvapAccum; }
void SetWaterEvapAccum(float v) { waterEvapAccum = v; }
