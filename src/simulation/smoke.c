#include "smoke.h"
#include "weather.h"
#include "balance.h"
#include "../core/sim_manager.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include <string.h>
#include <stdlib.h>

// Smoke grid storage
SmokeCell smokeGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Global state
bool smokeEnabled = true;
int smokeUpdateCount = 0;

// Tweakable parameters (game-hours, converted to game-seconds at point of use)
float smokeRiseInterval = 0.04f;     // Rise attempt every 0.04 game-hours
float smokeDissipationTime = 2.0f;   // Smoke dissipates over 2.0 game-hours per level
int smokeGenerationRate = 3;         // Fire level / 3 = smoke generated

// Internal accumulators for game-time
static float smokeRiseAccum = 0.0f;
static float smokeDissipationAccum = 0.0f;

// BFS queue for pressure propagation (fill down)
typedef struct {
    int x, y, z;
} SmokePos;

static SmokePos smokePressureQueue[SMOKE_PRESSURE_SEARCH_LIMIT];

// Generation counter for visited tracking (avoids expensive memset)
static uint16_t smokePressureGeneration = 0;
static uint16_t smokePressureVisited[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Track cells that have already risen this tick (prevents cascading through multiple z-levels)
static uint16_t smokeRiseGeneration = 0;
static uint16_t smokeHasRisen[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize smoke system
void InitSmoke(void) {
    ClearSmoke();
}

// Clear all smoke
void ClearSmoke(void) {
    memset(smokeGrid, 0, sizeof(smokeGrid));
    smokeUpdateCount = 0;
    smokeRiseAccum = 0.0f;
    smokeDissipationAccum = 0.0f;
    smokeActiveCells = 0;
}

// Reset accumulators (call after loading smoke grid from save)
void ResetSmokeAccumulators(void) {
    smokeRiseAccum = 0.0f;
    smokeDissipationAccum = 0.0f;

    // Also destabilize all cells so they get processed after load
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                smokeGrid[z][y][x].stable = false;
            }
        }
    }
}

// Bounds check helper
static inline bool InBounds(int x, int y, int z) {
    return x >= 0 && x < gridWidth &&
           y >= 0 && y < gridHeight &&
           z >= 0 && z < gridDepth;
}

// Check if smoke can exist in a cell (not blocked by walls)
static inline bool CanHoldSmoke(int x, int y, int z) {
    if (!InBounds(x, y, z)) return false;
    CellType cell = grid[z][y][x];
    return CellAllowsFluids(cell);
}

// Mark cell and neighbors as unstable
void DestabilizeSmoke(int x, int y, int z) {
    if (InBounds(x, y, z)) {
        smokeGrid[z][y][x].stable = false;
    }

    // 4 horizontal neighbors
    if (InBounds(x-1, y, z)) smokeGrid[z][y][x-1].stable = false;
    if (InBounds(x+1, y, z)) smokeGrid[z][y][x+1].stable = false;
    if (InBounds(x, y-1, z)) smokeGrid[z][y-1][x].stable = false;
    if (InBounds(x, y+1, z)) smokeGrid[z][y+1][x].stable = false;

    // Above and below
    if (InBounds(x, y, z-1)) smokeGrid[z-1][y][x].stable = false;
    if (InBounds(x, y, z+1)) smokeGrid[z+1][y][x].stable = false;
}

// Set smoke level at a cell
void SetSmokeLevel(int x, int y, int z, int level) {
    if (!InBounds(x, y, z)) return;
    if (level < 0) level = 0;
    if (level > SMOKE_MAX_LEVEL) level = SMOKE_MAX_LEVEL;

    int oldLevel = smokeGrid[z][y][x].level;
    smokeGrid[z][y][x].level = (uint8_t)level;

    // Update presence tracking
    if (oldLevel == 0 && level > 0) {
        smokeActiveCells++;
    } else if (oldLevel > 0 && level == 0) {
        smokeActiveCells--;
    }

    if (oldLevel != level) {
        DestabilizeSmoke(x, y, z);
    }
}

// Add smoke to a cell
void AddSmoke(int x, int y, int z, int amount) {
    if (!InBounds(x, y, z)) return;
    int newLevel = smokeGrid[z][y][x].level + amount;
    SetSmokeLevel(x, y, z, newLevel);
}

// Query functions
int GetSmokeLevel(int x, int y, int z) {
    if (!InBounds(x, y, z)) return 0;
    return smokeGrid[z][y][x].level;
}

bool HasSmoke(int x, int y, int z) {
    return GetSmokeLevel(x, y, z) > 0;
}

// Generate smoke from fire
void GenerateSmokeFromFire(int x, int y, int z, int fireLevel) {
    if (!InBounds(x, y, z)) return;
    if (fireLevel <= 0) return;

    // Generate smoke based on fire level
    int smokeAmount = fireLevel / smokeGenerationRate;
    if (smokeAmount < 1 && fireLevel > 0) smokeAmount = 1;

    // Wet cells produce more smoke (smoldering)
    {
        int wetness = GET_CELL_WETNESS(x, y, z);
        if (wetness == 1) smokeAmount *= 2;          // Damp: 2x
        else if (wetness >= 2) smokeAmount *= 3;     // Wet/soaked: 3x
    }

    // Add smoke to current cell and cell above
    AddSmoke(x, y, z, smokeAmount);

    // Smoke rises - add more to cell above if possible
    if (CanHoldSmoke(x, y, z + 1)) {
        AddSmoke(x, y, z + 1, smokeAmount);
        smokeGrid[z + 1][y][x].pressureSourceZ = z;  // Track origin
    }
}

// Phase 1: RISING - Smoke moves up if there's space above
static int TryRise(int x, int y, int z) {
    if (z >= gridDepth - 1) {
        return 0;  // At top
    }
    if (!CanHoldSmoke(x, y, z + 1)) {
        return 0;  // Blocked above
    }

    SmokeCell* src = &smokeGrid[z][y][x];
    SmokeCell* dst = &smokeGrid[z+1][y][x];

    if (src->level == 0) return 0;

    // Don't rise if this cell's smoke already rose into it this tick
    // This prevents smoke from cascading through multiple z-levels in one tick
    if (smokeHasRisen[z][y][x] == smokeRiseGeneration) {
        return 0;
    }

    int space = SMOKE_MAX_LEVEL - dst->level;
    if (space <= 0) {
        // Cell above is full - create pressure (smoke wants to rise but can't)
        src->hasPressure = true;
        if (src->pressureSourceZ == 0) src->pressureSourceZ = z;
        return 0;
    }

    int flow = 1;  // Move 1 unit at a time for gradual rising
    if (flow > src->level) flow = src->level;
    if (flow > space) flow = space;

    // Track counter transitions
    bool srcWasActive = src->level > 0;
    bool dstWasActive = dst->level > 0;

    src->level -= flow;
    dst->level += flow;

    // Update counters for transitions
    if (srcWasActive && src->level == 0) {
        smokeActiveCells--;
    }
    if (!dstWasActive && dst->level > 0) {
        smokeActiveCells++;
    }

    // Mark destination as having received risen smoke this tick
    // This prevents the smoke from immediately rising again when z+1 is processed
    smokeHasRisen[z+1][y][x] = smokeRiseGeneration;

    // Track pressure source
    if (dst->pressureSourceZ == 0 || dst->pressureSourceZ > z) {
        dst->pressureSourceZ = z;
    }

    // If destination is now full, mark for pressure
    if (dst->level == SMOKE_MAX_LEVEL) {
        dst->hasPressure = true;
    }

    DestabilizeSmoke(x, y, z);
    DestabilizeSmoke(x, y, z + 1);

    return flow;
}

// Phase 2: SPREADING - Equalize smoke levels with horizontal neighbors
static bool SmokeTrySpread(int x, int y, int z) {
    SmokeCell* cell = &smokeGrid[z][y][x];
    if (cell->level == 0) return false;

    // Orthogonal neighbors - randomize order
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    // Fisher-Yates shuffle
    int order[] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    // Wind bias: sort by descending wind dot product (downwind neighbors first)
    if (weatherState.windStrength > 0.5f) {
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 4; j++) {
                float dotI = GetWindDotProduct(dx[order[i]], dy[order[i]]);
                float dotJ = GetWindDotProduct(dx[order[j]], dy[order[j]]);
                if (dotJ > dotI) {
                    int tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }
            }
        }
    }

    bool moved = false;

    for (int i = 0; i < 4; i++) {
        int dir = order[i];
        int nx = x + dx[dir];
        int ny = y + dy[dir];

        if (!CanHoldSmoke(nx, ny, z)) continue;

        SmokeCell* neighbor = &smokeGrid[z][ny][nx];
        int diff = cell->level - neighbor->level;

        // Spread to lower neighbors
        if (diff >= 2) {
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;

            // Track neighbor becoming active
            if (neighborWasEmpty) {
                smokeActiveCells++;
            }

            DestabilizeSmoke(x, y, z);
            DestabilizeSmoke(nx, ny, z);
            moved = true;

            if (cell->level <= 1) break;
        } else if (diff == 1 && cell->level > 1) {
            bool neighborWasEmpty = neighbor->level == 0;
            cell->level -= 1;
            neighbor->level += 1;

            // Track neighbor becoming active
            if (neighborWasEmpty) {
                smokeActiveCells++;
            }

            DestabilizeSmoke(x, y, z);
            DestabilizeSmoke(nx, ny, z);
            moved = true;
            break;  // Only give to one neighbor when diff=1
        }
    }

    return moved;
}

// Phase 3: FILL DOWN - When trapped, smoke fills downward (inverse pressure)
static bool TryFillDown(int x, int y, int z) {
    SmokeCell* cell = &smokeGrid[z][y][x];

    // Need full smoke with pressure to fill down
    if (cell->level < SMOKE_MAX_LEVEL) return false;
    if (!cell->hasPressure) return false;

    int minZ = cell->pressureSourceZ;
    if (minZ >= z) minZ = 0;  // Can fill all the way down if no source tracked

    // BFS to find nearest non-full cell through full cells (going down and horizontal)
    // Use generation counter instead of memset for O(1) reset
    smokePressureGeneration++;
    if (smokePressureGeneration == 0) {
        // Handle wraparound (rare) - clear array once
        memset(smokePressureVisited, 0, sizeof(smokePressureVisited));
        smokePressureGeneration = 1;
    }

    int queueHead = 0;
    int queueTail = 0;

    int dx[] = {-1, 1, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0};
    int dz[] = {0, 0, 0, 0, -1};  // Include down direction

    smokePressureVisited[z][y][x] = smokePressureGeneration;

    // Add initial neighbors to queue (prioritize going down)
    for (int i = 4; i >= 0; i--) {  // Start with down
        int nx = x + dx[i];
        int ny = y + dy[i];
        int nz = z + dz[i];

        if (nz < minZ) continue;
        if (!CanHoldSmoke(nx, ny, nz)) continue;
        if (smokePressureVisited[nz][ny][nx] == smokePressureGeneration) continue;

        smokePressureVisited[nz][ny][nx] = smokePressureGeneration;
        smokePressureQueue[queueTail++] = (SmokePos){nx, ny, nz};

        if (queueTail >= SMOKE_PRESSURE_SEARCH_LIMIT) break;
    }

    // BFS through full cells looking for non-full cell
    while (queueHead < queueTail) {
        SmokePos pos = smokePressureQueue[queueHead++];
        SmokeCell* current = &smokeGrid[pos.z][pos.y][pos.x];

        // Found a non-full cell - push smoke here
        if (current->level < SMOKE_MAX_LEVEL) {
            int space = SMOKE_MAX_LEVEL - current->level;
            int transfer = 1;
            if (transfer > space) transfer = space;
            if (transfer > cell->level) transfer = cell->level;

            if (transfer > 0) {
                bool currentWasEmpty = current->level == 0;
                cell->level -= transfer;
                current->level += transfer;

                // Track destination becoming active
                if (currentWasEmpty) {
                    smokeActiveCells++;
                }

                DestabilizeSmoke(x, y, z);
                DestabilizeSmoke(pos.x, pos.y, pos.z);

                // Clear pressure if we're no longer full
                if (cell->level < SMOKE_MAX_LEVEL) {
                    cell->hasPressure = false;
                }

                return true;
            }
        }

        // Cell is full - continue searching through it
        if (current->level >= SMOKE_MAX_LEVEL) {
            for (int i = 4; i >= 0; i--) {
                int nx = pos.x + dx[i];
                int ny = pos.y + dy[i];
                int nz = pos.z + dz[i];

                if (nz < minZ) continue;
                if (!CanHoldSmoke(nx, ny, nz)) continue;
                if (smokePressureVisited[nz][ny][nx] == smokePressureGeneration) continue;

                smokePressureVisited[nz][ny][nx] = smokePressureGeneration;

                if (queueTail < SMOKE_PRESSURE_SEARCH_LIMIT) {
                    smokePressureQueue[queueTail++] = (SmokePos){nx, ny, nz};
                }
            }
        }
    }

    return false;
}

// Process a single smoke cell
// doRise: interval has elapsed for rising
// doDissipate: interval has elapsed for dissipation
static bool ProcessSmokeCell(int x, int y, int z, bool doRise, bool doDissipate) {
    SmokeCell* cell = &smokeGrid[z][y][x];
    bool moved = false;

    // No smoke to process
    if (cell->level == 0) {
        // Mark stable - DestabilizeSmoke will clear this if smoke arrives nearby
        cell->stable = true;
        cell->hasPressure = false;
        return false;
    }

    // Phase 1: Try to rise (highest priority for smoke)
    if (doRise) {
        int rose = TryRise(x, y, z);
        if (rose > 0) moved = true;
    }

    // Phase 2: Try to spread horizontally (if we still have smoke)
    // Spreading happens every tick for smooth equalization
    if (cell->level > 0) {
        if (SmokeTrySpread(x, y, z)) moved = true;
    }

    // Phase 3: Try fill down (if full and pressurized)
    if (cell->level >= SMOKE_MAX_LEVEL && cell->hasPressure) {
        if (TryFillDown(x, y, z)) moved = true;
    }

    // Dissipation: smoke gradually fades
    // Skip dissipation for cells that just received risen smoke this tick
    // This prevents smoke from dissipating immediately after rising, which caused z-level gaps
    bool justRose = (smokeHasRisen[z][y][x] == smokeRiseGeneration);
    if (doDissipate && cell->level > 0 && !justRose) {
        // Smoke at lower z-levels (in open air) dissipates faster
        // Smoke at higher z-levels or trapped dissipates slower
        bool isTrapped = cell->hasPressure || (z > 0 && !CanHoldSmoke(x, y, z + 1));

        if (!isTrapped || (rand() % 3) == 0) {
            cell->level--;
            if (cell->level == 0) {
                cell->hasPressure = false;
                cell->pressureSourceZ = 0;
            }
            DestabilizeSmoke(x, y, z);
            moved = true;
        }
    }

    // Clear pressure if we're no longer full
    if (cell->level < SMOKE_MAX_LEVEL) {
        cell->hasPressure = false;
    }

    // Mark stable if nothing is happening
    if (!moved && cell->level == 0) {
        cell->stable = true;
    }

    return moved;
}

// Internal tick counter for scan direction alternation
static int smokeTick = 0;

// Main smoke update - process from BOTTOM to TOP (smoke rises)
void UpdateSmoke(void) {
    if (!smokeEnabled) return;

    // Early exit: no smoke activity at all
    if (smokeActiveCells == 0) {
        smokeUpdateCount = 0;
        return;
    }

    smokeUpdateCount = 0;
    smokeTick++;

    // Accumulate game time for interval-based actions
    smokeRiseAccum += gameDeltaTime;
    smokeDissipationAccum += gameDeltaTime;

    // Check if intervals have elapsed
    float riseIntervalGS = GameHoursToGameSeconds(smokeRiseInterval);
    // Dissipation interval is per level, so we check against smokeDissipationTime / SMOKE_MAX_LEVEL
    float dissipationInterval = GameHoursToGameSeconds(smokeDissipationTime) / (float)SMOKE_MAX_LEVEL;

    // Rain slows smoke rise and dissipation (humid air)
    {
        WeatherType w = weatherState.current;
        if (w == WEATHER_HEAVY_RAIN || w == WEATHER_THUNDERSTORM) {
            riseIntervalGS *= 2.0f;       // Heavy rain: 2x slower rise
            dissipationInterval *= 1.5f;   // Humid air holds smoke
        } else if (w == WEATHER_RAIN) {
            riseIntervalGS *= 1.5f;        // Light rain: 1.5x slower rise
            dissipationInterval *= 1.5f;
        } else if (w == WEATHER_MIST) {
            dissipationInterval *= 1.5f;   // Mist: slower dissipation only
        }
    }

    bool doRise = smokeRiseAccum >= riseIntervalGS;
    bool doDissipate = smokeDissipationAccum >= dissipationInterval;

    // Reset accumulators when intervals elapse
    if (doRise) {
        smokeRiseAccum -= riseIntervalGS;
        // Increment generation to reset "has risen" tracking for this tick
        smokeRiseGeneration++;
        if (smokeRiseGeneration == 0) {
            // Handle wraparound - clear the array
            memset(smokeHasRisen, 0, sizeof(smokeHasRisen));
            smokeRiseGeneration = 1;
        }
    }
    if (doDissipate) smokeDissipationAccum -= dissipationInterval;

    // Alternate scan direction each tick to avoid directional bias
    bool reverseX = (smokeTick & 1);
    bool reverseY = (smokeTick & 2);

    // Process from bottom to top (simple iteration)
    for (int z = 0; z < gridDepth; z++) {
        for (int yi = 0; yi < gridHeight; yi++) {
            int y = reverseY ? (gridHeight - 1 - yi) : yi;
            for (int xi = 0; xi < gridWidth; xi++) {
                int x = reverseX ? (gridWidth - 1 - xi) : xi;
                
                SmokeCell* cell = &smokeGrid[z][y][x];

                // Skip stable empty cells
                if (cell->stable && cell->level == 0) {
                    continue;
                }

                ProcessSmokeCell(x, y, z, doRise, doDissipate);
                smokeUpdateCount++;

                // Cap updates per tick
                if (smokeUpdateCount >= SMOKE_MAX_UPDATES_PER_TICK) {
                    return;
                }
            }
        }
    }
}

float GetSmokeRiseAccum(void) { return smokeRiseAccum; }
float GetSmokeDissipationAccum(void) { return smokeDissipationAccum; }
void SetSmokeRiseAccum(float v) { smokeRiseAccum = v; }
void SetSmokeDissipationAccum(float v) { smokeDissipationAccum = v; }
