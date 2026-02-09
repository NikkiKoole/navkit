#include "terrain.h"
#include "cell_defs.h"
#include "material.h"
#include "../../vendor/raylib.h"
#include "../entities/workshops.h"
#include "../entities/stockpiles.h"
#include "../entities/items.h"
#include "../entities/mover.h"
#include "designations.h"
#include "../simulation/water.h"
#include "../simulation/trees.h"
#include "../game_state.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Simple Perlin-like noise
static int permutation[512];

static inline float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline int ClampInt(int v, int minV, int maxV) {
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

static bool CanPlaceWorldGenTreeAt(int x, int y, int baseZ) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;
    if (baseZ < 0 || baseZ + 1 >= gridDepth) return false;
    if (!CellIsSolid(grid[baseZ][y][x])) return false;
    if (GetWallMaterial(x, y, baseZ) == MAT_GRANITE) return false;  // Trees avoid rock
    if (grid[baseZ + 1][y][x] != CELL_AIR) return false;
    return true;
}

static MaterialType PickTreeTypeForSoilSimple(MaterialType mat) {
    switch (mat) {
        case MAT_PEAT: return MAT_WILLOW;
        case MAT_SAND: return MAT_BIRCH;
        case MAT_GRAVEL: return MAT_PINE;
        case MAT_CLAY: return MAT_OAK;
        case MAT_DIRT:
        default: return MAT_OAK;
    }
}

static MaterialType PickTreeTypeForWorldGen(MaterialType mat, float wetness, int slope, bool nearWater, float noise) {
    if (nearWater || wetness > 0.7f) {
        if (mat == MAT_PEAT || mat == MAT_DIRT) return MAT_WILLOW;
    }

    if ((mat == MAT_GRAVEL || slope >= 1) && wetness < 0.45f) {
        return MAT_PINE;
    }

    if (mat == MAT_SAND || mat == MAT_GRAVEL) {
        return MAT_BIRCH;
    }

    if (mat == MAT_CLAY || mat == MAT_DIRT) {
        return MAT_OAK;
    }

    if (mat == MAT_PEAT) {
        return MAT_WILLOW;
    }

    return (noise < 0.5f) ? MAT_OAK : MAT_BIRCH;
}

static void PlaceWorldGenTree(int x, int y, int baseZ, MaterialType treeMat, bool growFull) {
    if (!CanPlaceWorldGenTreeAt(x, y, baseZ)) return;
    int z = baseZ + 1;
    if (growFull) {
        TreeGrowFull(x, y, z, treeMat);
    } else {
        PlaceSapling(x, y, z, treeMat);
    }
}

static inline bool ShouldPlaceRampAt(int x, int y) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;

    float density = Clamp01(rampDensity);
    if (density <= 0.0f) return false;
    if (density >= 0.999f) return true;

    float scale = rampNoiseScale;
    if (scale <= 0.0f) scale = 0.01f;

    float n = OctavePerlin(x * scale, y * scale, 2, 0.5f);
    return n <= density;
}

// Seed the random number generator with worldSeed
// Call at the start of each terrain generator for reproducible results
static void SeedTerrain(void) {
    SetRandomSeed((unsigned int)worldSeed);
}

// Room structure for dungeon generators
typedef struct {
    int x, y, w, h;
} Room;

void InitGrid(void) {
    // Re-initialize grid with current dimensions (clears cells and flags)
    InitGridWithSizeAndChunkSize(gridWidth, gridHeight, chunkWidth, chunkHeight);

    // Clear all entity and simulation state so nothing leaks across terrain regeneration
    ClearMovers();
    ClearItems();
    ClearWorkshops();
    ClearStockpiles();
    ClearWater();
}

void GenerateSparse(float density) {
    InitGrid();
    SeedTerrain();
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if ((float)GetRandomValue(0, 100) / 100.0f < density) {
                grid[1][y][x] = CELL_WALL;  // Natural rock at z=1 (walking level)
                SetWallMaterial(x, y, 1, MAT_GRANITE);
            }
    needsRebuild = true;
}

// ============================================================================
// Floor Placement Helper
// Uses CELL_AIR + HAS_FLOOR flag (for balconies/bridges)
// ============================================================================
static void PlaceFloor(int x, int y, int z, MaterialType mat) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    grid[z][y][x] = CELL_AIR;
    SET_FLOOR(x, y, z);
    SetFloorMaterial(x, y, z, mat);
    // Clear grass/surface overlay on floors
    SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
}

// Helper to check if a cell is a floor (HAS_FLOOR flag)
static bool IsFloorCell(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    return HAS_FLOOR(x, y, z);
}

// Forward declaration (defined later in file)
static void BuildTowerBlockAt(int baseX, int baseY, int width, int height, int floors, bool vertical, int baseZ, int* surface);

static int FindSurfaceZAt(int x, int y) {
    for (int z = gridDepth - 1; z >= 0; z--) {
        if (CellIsSolid(grid[z][y][x])) return z;
    }
    return 0;
}

static bool AreaIsFlatAndDry(int baseX, int baseY, int w, int h, int maxDelta,
                             const int* surface, const bool* waterMask, int* outBaseZ) {
    int minZ = 9999;
    int maxZ = -9999;
    for (int y = baseY; y < baseY + h; y++) {
        for (int x = baseX; x < baseX + w; x++) {
            int idx = y * gridWidth + x;
            if (waterMask[idx]) return false;
            int z = surface[idx];
            if (z < minZ) minZ = z;
            if (z > maxZ) maxZ = z;
        }
    }
    if (maxZ - minZ > maxDelta) return false;
    if (outBaseZ) *outBaseZ = minZ;
    return true;
}

static void FlattenAreaTo(int baseX, int baseY, int w, int h, int targetZ, int* surface) {
    for (int y = baseY; y < baseY + h; y++) {
        for (int x = baseX; x < baseX + w; x++) {
            int idx = y * gridWidth + x;
            int current = surface[idx];
            if (current > targetZ) {
                for (int z = targetZ + 1; z <= current; z++) {
                    grid[z][y][x] = CELL_AIR;
                    SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
                }
            } else if (current < targetZ) {
                for (int z = current + 1; z <= targetZ; z++) {
                    grid[z][y][x] = CELL_WALL;
                    SetWallMaterial(x, y, z, MAT_DIRT);
                    SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
                }
            }
            surface[idx] = targetZ;
        }
    }
}

static void CarveEntryApron(int outsideX, int outsideY, int baseZ, int* surface) {
    int startX = outsideX - 1;
    int startY = outsideY - 1;
    int endX = outsideX + 1;
    int endY = outsideY + 1;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;
    if (endX >= gridWidth) endX = gridWidth - 1;
    if (endY >= gridHeight) endY = gridHeight - 1;
    int w = endX - startX + 1;
    int h = endY - startY + 1;

    if (surface) {
        FlattenAreaTo(startX, startY, w, h, baseZ, surface);
    } else {
        for (int y = startY; y <= endY; y++) {
            for (int x = startX; x <= endX; x++) {
                if (baseZ >= 0 && baseZ < gridDepth) {
                    grid[baseZ][y][x] = CELL_WALL;
                    SetWallMaterial(x, y, baseZ, MAT_DIRT);
                    SET_CELL_SURFACE(x, y, baseZ, SURFACE_BARE);
                }
            }
        }
    }

    int airZ = baseZ + 1;
    if (airZ >= gridDepth) return;
    for (int y = startY; y <= endY; y++) {
        for (int x = startX; x <= endX; x++) {
            grid[airZ][y][x] = CELL_AIR;
            SET_CELL_SURFACE(x, y, airZ, SURFACE_BARE);
        }
    }
}

static void PlaceMiniTowerAt(int baseX, int baseY, int baseZ, int w, int h, int floors, int* surface) {
    int maxFloors = gridDepth - (baseZ + 1);
    if (floors > maxFloors) floors = maxFloors;
    if (floors < 1) floors = 1;

    int ladderX = baseX + w / 2;
    int ladderY = baseY + h / 2;

    for (int z = 0; z < floors; z++) {
        int buildZ = baseZ + 1 + z;
        for (int py = baseY; py < baseY + h; py++) {
            for (int px = baseX; px < baseX + w; px++) {
                bool isBorder = (px == baseX || px == baseX + w - 1 ||
                                 py == baseY || py == baseY + h - 1);
                bool isLadder = (px == ladderX && py == ladderY);
                if (isBorder) {
                    grid[buildZ][py][px] = CELL_WALL;
                } else if (!isLadder) {
                    PlaceFloor(px, py, buildZ, MAT_GRANITE);
                }
            }
        }
    }

    for (int z = 0; z < floors; z++) {
        grid[baseZ + 1 + z][ladderY][ladderX] = CELL_LADDER_BOTH;
    }

    int doorSide = GetRandomValue(0, 3);
    if (surface) {
        int bestSide = doorSide;
        int bestScore = 999999;
        int candidates[4][2] = {
            {baseX + w / 2, baseY - 1},         // North outside
            {baseX + w, baseY + h / 2},         // East outside
            {baseX + w / 2, baseY + h},         // South outside
            {baseX - 1, baseY + h / 2}          // West outside
        };
        for (int s = 0; s < 4; s++) {
            int ox = candidates[s][0];
            int oy = candidates[s][1];
            if (ox < 0 || ox >= gridWidth || oy < 0 || oy >= gridHeight) continue;
            int oidx = oy * gridWidth + ox;
            int outsideZ = surface[oidx];
            int heightDiff = outsideZ - baseZ;
            if (heightDiff < 0) heightDiff = 0;
            int score = heightDiff * 100 + outsideZ;
            if (score < bestScore) {
                bestScore = score;
                bestSide = s;
            }
        }
        doorSide = bestSide;
    }

    int doorX = baseX + w / 2;
    int doorY = baseY + h / 2;
    int outsideX = doorX;
    int outsideY = doorY;
    switch (doorSide) {
        case 0:
            doorX = baseX + w / 2; doorY = baseY;
            outsideX = doorX; outsideY = baseY - 1;
            break;
        case 1:
            doorX = baseX + w - 1; doorY = baseY + h / 2;
            outsideX = baseX + w; outsideY = doorY;
            break;
        case 2:
            doorX = baseX + w / 2; doorY = baseY + h - 1;
            outsideX = doorX; outsideY = baseY + h;
            break;
        case 3:
            doorX = baseX; doorY = baseY + h / 2;
            outsideX = baseX - 1; outsideY = doorY;
            break;
    }
    PlaceFloor(doorX, doorY, baseZ + 1, MAT_GRANITE);
    if (surface && outsideX >= 0 && outsideX < gridWidth && outsideY >= 0 && outsideY < gridHeight) {
        CarveEntryApron(outsideX, outsideY, baseZ, surface);
    }
}

static void PlaceMiniGalleryFlatAt(int baseX, int baseY, int baseZ, int numApartments, int numFloors, int* surface) {
    int apartmentWidth = 4;
    int apartmentDepth = 4;
    int corridorWidth = 2;
    int stairWidth = 2;

    if (numApartments < 2) numApartments = 2;
    if (numApartments > 4) numApartments = 4;

    int buildingWidth = stairWidth + numApartments * apartmentWidth + stairWidth;
    int buildingDepth = apartmentDepth + corridorWidth;

    int maxFloors = gridDepth - (baseZ + 1);
    if (numFloors > maxFloors) numFloors = maxFloors;
    if (numFloors < 1) numFloors = 1;

    for (int floor = 0; floor < numFloors; floor++) {
        int z = baseZ + 1 + floor;

        for (int x = baseX; x < baseX + buildingWidth; x++) {
            grid[z][baseY][x] = CELL_WALL;
            grid[z][baseY + buildingDepth - 1][x] = CELL_WALL;
        }
        for (int y = baseY; y < baseY + buildingDepth; y++) {
            grid[z][y][baseX] = CELL_WALL;
            grid[z][y][baseX + buildingWidth - 1] = CELL_WALL;
        }

        for (int y = baseY + 1; y < baseY + buildingDepth - 1; y++) {
            for (int x = baseX + 1; x < baseX + buildingWidth - 1; x++) {
                PlaceFloor(x, y, z, MAT_GRANITE);
            }
        }

        int apartmentStartX = baseX + stairWidth;
        for (int apt = 0; apt < numApartments; apt++) {
            int aptX = apartmentStartX + apt * apartmentWidth;
            if (apt > 0) {
                for (int y = baseY; y < baseY + apartmentDepth; y++) {
                    grid[z][y][aptX] = CELL_WALL;
                }
            }
            for (int x = aptX; x < aptX + apartmentWidth && x < baseX + buildingWidth - stairWidth; x++) {
                grid[z][baseY + apartmentDepth - 1][x] = CELL_WALL;
            }
            int doorX = aptX + apartmentWidth / 2;
            if (doorX < baseX + buildingWidth - stairWidth) {
                PlaceFloor(doorX, baseY + apartmentDepth - 1, z, MAT_GRANITE);
            }
        }

        int lastWallX = apartmentStartX + numApartments * apartmentWidth;
        if (lastWallX < baseX + buildingWidth - 1) {
            for (int y = baseY; y < baseY + apartmentDepth; y++) {
                grid[z][y][lastWallX] = CELL_WALL;
            }
        }

        int westStairX = baseX + 1;
        int stairY = baseY + 1;
        grid[z][stairY][westStairX] = CELL_LADDER_BOTH;

        int eastStairX = baseX + buildingWidth - 2;
        grid[z][stairY][eastStairX] = CELL_LADDER_BOTH;
    }

    int entranceX = baseX + buildingWidth / 2;
    PlaceFloor(entranceX, baseY + buildingDepth - 1, baseZ + 1, MAT_GRANITE);
    PlaceFloor(entranceX + 2, baseY + buildingDepth - 1, baseZ + 1, MAT_GRANITE);

    int outsideY = baseY + buildingDepth;
    if (outsideY < gridHeight) {
        CarveEntryApron(entranceX, outsideY, baseZ, surface);
        CarveEntryApron(entranceX + 2, outsideY, baseZ, surface);
    }
}

// ============================================================================
// Connectivity Check (Flood-fill)
// ============================================================================

typedef struct {
    int x, y, z;
} FloodNode;

typedef struct {
    int size;
    int minZ;
    int maxZ;
    int floors;
    int ladders;
    int ramps;
    int waterAdj;
    int boundary;
    FloodNode sample;
} ComponentInfo;

static ComponentInfo* componentSortInfos = NULL;

static int CompareComponentSizeAsc(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    int sa = componentSortInfos[ia].size;
    int sb = componentSortInfos[ib].size;
    return (sa > sb) - (sa < sb);
}

static inline int FloodIndex(int x, int y, int z) {
    return (z * gridHeight + y) * gridWidth + x;
}

static bool FindRampBelowExitTo(int x, int y, int z, int* outX, int* outY) {
    if (z - 1 < 0) return false;
    // For each direction, check if ramp at z-1 points to (x,y)
    static const int rampDirs[4][3] = {
        {0, -1, CELL_RAMP_S}, // Ramp north (y-1) pointing south to us
        {0, 1, CELL_RAMP_N},  // Ramp south (y+1) pointing north to us
        {-1, 0, CELL_RAMP_E}, // Ramp west (x-1) pointing east to us
        {1, 0, CELL_RAMP_W}   // Ramp east (x+1) pointing west to us
    };
    for (int i = 0; i < 4; i++) {
        int rx = x + rampDirs[i][0];
        int ry = y + rampDirs[i][1];
        if (rx < 0 || rx >= gridWidth || ry < 0 || ry >= gridHeight) continue;
        if (grid[z - 1][ry][rx] == (CellType)rampDirs[i][2]) {
            if (outX) *outX = rx;
            if (outY) *outY = ry;
            return true;
        }
    }
    return false;
}

static bool CanStepTo(int fromX, int fromY, int fromZ, int toX, int toY, int toZ) {
    if (toX < 0 || toX >= gridWidth || toY < 0 || toY >= gridHeight || toZ < 0 || toZ >= gridDepth) return false;
    if (!IsCellWalkableAt(toZ, toY, toX)) return false;

    if (toZ == fromZ) {
        return CanEnterRampFromSide(toX, toY, toZ, fromX, fromY);
    }

    if (toX != fromX || toY != fromY) {
        // Vertical transitions only support same x,y (ladders) or ramp exits (handled below)
        // We'll allow ramp up separately by matching explicit exit coords.
    }

    if (toZ == fromZ + 1) {
        if (toX == fromX && toY == fromY) {
            if (CanClimbUpAt(fromX, fromY, fromZ)) return true;
            if (HasRampPointingTo(fromX, fromY, fromZ) && IsCellWalkableAt(toZ, toY, toX)) return true;
        }

        CellType cell = grid[fromZ][fromY][fromX];
        if (CellIsDirectionalRamp(cell)) {
            int dx, dy;
            GetRampHighSideOffset(cell, &dx, &dy);
            if (fromX + dx == toX && fromY + dy == toY) {
                return true;
            }
        }
    } else if (toZ == fromZ - 1) {
        if (toX == fromX && toY == fromY) {
            if (CanClimbDownAt(fromX, fromY, fromZ)) return true;
        }
        int rx, ry;
        if (FindRampBelowExitTo(fromX, fromY, fromZ, &rx, &ry)) {
            if (rx == toX && ry == toY) return true;
        }
    }

    return false;
}

static int FloodFillComponent(int startX, int startY, int startZ, uint8_t* visited, FloodNode* queue,
                              ComponentInfo* info) {
    int head = 0;
    int tail = 0;
    queue[tail++] = (FloodNode){startX, startY, startZ};
    visited[FloodIndex(startX, startY, startZ)] = 1;

    if (info) {
        info->size = 0;
        info->minZ = gridDepth;
        info->maxZ = -1;
        info->floors = 0;
        info->ladders = 0;
        info->ramps = 0;
        info->waterAdj = 0;
        info->boundary = 0;
        info->sample = (FloodNode){startX, startY, startZ};
    }

    while (head < tail) {
        FloodNode node = queue[head++];
        if (info) {
            info->size++;
            if (node.z < info->minZ) info->minZ = node.z;
            if (node.z > info->maxZ) info->maxZ = node.z;
            if (HAS_FLOOR(node.x, node.y, node.z)) info->floors++;
            if (CellIsLadder(grid[node.z][node.y][node.x])) info->ladders++;
            if (CellIsRamp(grid[node.z][node.y][node.x])) info->ramps++;
            if (HasWater(node.x, node.y, node.z)) info->waterAdj = 1;
            if (node.x == 0 || node.x == gridWidth - 1 || node.y == 0 || node.y == gridHeight - 1) {
                info->boundary = 1;
            }
        }

        static const int dxs[4] = {-1, 1, 0, 0};
        static const int dys[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nx = node.x + dxs[i];
            int ny = node.y + dys[i];
            int nz = node.z;
            if (!CanStepTo(node.x, node.y, node.z, nx, ny, nz)) continue;
            int nidx = FloodIndex(nx, ny, nz);
            if (visited[nidx]) continue;
            visited[nidx] = 1;
            queue[tail++] = (FloodNode){nx, ny, nz};
        }

        if (CanStepTo(node.x, node.y, node.z, node.x, node.y, node.z + 1)) {
            int nidx = FloodIndex(node.x, node.y, node.z + 1);
            if (!visited[nidx]) {
                visited[nidx] = 1;
                queue[tail++] = (FloodNode){node.x, node.y, node.z + 1};
            }
        }
        if (CanStepTo(node.x, node.y, node.z, node.x, node.y, node.z - 1)) {
            int nidx = FloodIndex(node.x, node.y, node.z - 1);
            if (!visited[nidx]) {
                visited[nidx] = 1;
                queue[tail++] = (FloodNode){node.x, node.y, node.z - 1};
            }
        }

        // Ramp up from ramp cell to exit
        CellType cell = grid[node.z][node.y][node.x];
        if (CellIsDirectionalRamp(cell)) {
            int dx, dy;
            GetRampHighSideOffset(cell, &dx, &dy);
            int nx = node.x + dx;
            int ny = node.y + dy;
            int nz = node.z + 1;
            if (CanStepTo(node.x, node.y, node.z, nx, ny, nz)) {
                int nidx = FloodIndex(nx, ny, nz);
                if (!visited[nidx]) {
                    visited[nidx] = 1;
                    queue[tail++] = (FloodNode){nx, ny, nz};
                }
            }
        }

        // Ramp down to ramp cell below (if exit here)
        int rx, ry;
        if (FindRampBelowExitTo(node.x, node.y, node.z, &rx, &ry)) {
            int nz = node.z - 1;
            if (CanStepTo(node.x, node.y, node.z, rx, ry, nz)) {
                int nidx = FloodIndex(rx, ry, nz);
                if (!visited[nidx]) {
                    visited[nidx] = 1;
                    queue[tail++] = (FloodNode){rx, ry, nz};
                }
            }
        }
    }

    return tail;
}

static void ReportWalkableComponents(const char* tag) {
    bool doReport = hillsWaterConnectivityReport;
    bool doFix = hillsWaterConnectivityFixSmall;
    int smallThreshold = hillsWaterConnectivitySmallThreshold;
    if (!doReport && !doFix) return;

    int cellCount = gridWidth * gridHeight * gridDepth;
    uint8_t* visited = (uint8_t*)calloc(cellCount, sizeof(uint8_t));
    if (!visited) return;

    FloodNode* queue = (FloodNode*)malloc(sizeof(FloodNode) * cellCount);
    if (!queue) {
        free(visited);
        return;
    }

    int totalWalkable = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (IsCellWalkableAt(z, y, x)) totalWalkable++;
            }
        }
    }

    ComponentInfo* infos = (ComponentInfo*)malloc(sizeof(ComponentInfo) * cellCount);
    if (!infos) {
        free(queue);
        free(visited);
        return;
    }

    int components = 0;
    int largest = 0;
    int largestIndex = -1;
    int smallComponents = 0;
    int smallCells = 0;
    int floorHeavy = 0;
    int rampComponents = 0;
    int ladderComponents = 0;
    int waterAdjComponents = 0;
    int boundaryComponents = 0;

    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (!IsCellWalkableAt(z, y, x)) continue;
                int startIdx = FloodIndex(x, y, z);
                if (visited[startIdx]) continue;

                ComponentInfo info;
                int size = FloodFillComponent(x, y, z, visited, queue, &info);
                infos[components] = info;

                if (size > largest) {
                    largest = size;
                    largestIndex = components;
                }
                if (size < smallThreshold) {
                    smallComponents++;
                    smallCells += size;
                }
                if (info.floors * 10 >= size * 6) floorHeavy++;
                if (info.ramps > 0) rampComponents++;
                if (info.ladders > 0) ladderComponents++;
                if (info.waterAdj) waterAdjComponents++;
                if (info.boundary) boundaryComponents++;

                components++;
            }
        }
    }

    int unreachable = totalWalkable - largest;
    float pct = (totalWalkable > 0) ? (100.0f * (float)unreachable / (float)totalWalkable) : 0.0f;
    if (doReport) {
        TraceLog(LOG_INFO, "%s connectivity: components=%d largest=%d unreachable=%d (%.1f%%) small=%d cells=%d",
                 tag, components, largest, unreachable, pct, smallComponents, smallCells);
        TraceLog(LOG_INFO, "%s connectivity detail: floor-heavy=%d ladders=%d ramps=%d water-adj=%d boundary=%d",
                 tag, floorHeavy, ladderComponents, rampComponents, waterAdjComponents, boundaryComponents);

        if (components > 0) {
            int* order = (int*)malloc(sizeof(int) * components);
            if (order) {
                for (int i = 0; i < components; i++) order[i] = i;
                componentSortInfos = infos;
                qsort(order, components, sizeof(int), CompareComponentSizeAsc);

                int reportSmall = components < 5 ? components : 5;
                for (int i = 0; i < reportSmall; i++) {
                    int idx = order[i];
                    ComponentInfo* ci = &infos[idx];
                    TraceLog(LOG_INFO, "%s small[%d]: size=%d z=%d..%d floors=%d ramps=%d ladders=%d water=%d boundary=%d sample=(%d,%d,z%d)",
                             tag, i, ci->size, ci->minZ, ci->maxZ, ci->floors, ci->ramps, ci->ladders,
                             ci->waterAdj, ci->boundary, ci->sample.x, ci->sample.y, ci->sample.z);
                }

                int reportLarge = components < 3 ? components : 3;
                for (int i = 0; i < reportLarge; i++) {
                    int idx = order[components - 1 - i];
                    ComponentInfo* ci = &infos[idx];
                    TraceLog(LOG_INFO, "%s large[%d]: size=%d z=%d..%d floors=%d ramps=%d ladders=%d sample=(%d,%d,z%d)",
                             tag, i, ci->size, ci->minZ, ci->maxZ, ci->floors, ci->ramps, ci->ladders,
                             ci->sample.x, ci->sample.y, ci->sample.z);
                }
                free(order);
            }
        }
    }

    if (doFix && smallThreshold > 0 && components > 1) {
        memset(visited, 0, cellCount);
        int fixedComponents = 0;
        int fixedCells = 0;
        int compIndex = 0;

        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    if (!IsCellWalkableAt(z, y, x)) continue;
                    int startIdx = FloodIndex(x, y, z);
                    if (visited[startIdx]) continue;

                    int size = FloodFillComponent(x, y, z, visited, queue, NULL);
                    if (compIndex != largestIndex && size < smallThreshold) {
                        for (int i = 0; i < size; i++) {
                            FloodNode node = queue[i];
                            CLEAR_FLOOR(node.x, node.y, node.z);
                            if (node.z == 0) {
                                grid[node.z][node.y][node.x] = CELL_WALL;
                                SetWallMaterial(node.x, node.y, node.z, MAT_DIRT);
                            } else {
                                grid[node.z][node.y][node.x] = CELL_WALL;
                                SetWallMaterial(node.x, node.y, node.z, MAT_GRANITE);
                            }
                            SET_CELL_SURFACE(node.x, node.y, node.z, SURFACE_BARE);
                            if (HasWater(node.x, node.y, node.z)) {
                                SetWaterLevel(node.x, node.y, node.z, 0);
                                SetWaterSource(node.x, node.y, node.z, false);
                                SetWaterDrain(node.x, node.y, node.z, false);
                            }
                        }
                        fixedComponents++;
                        fixedCells += size;
                    }
                    compIndex++;
                }
            }
        }

        if (doReport) {
            TraceLog(LOG_INFO, "%s connectivity fix: filled %d components (%d cells) under %d",
                     tag, fixedComponents, fixedCells, smallThreshold);
        }
    }

    free(infos);
    free(queue);
    free(visited);
}

// ============================================================================
// Labyrinth3D Generator
// Creates a multi-level maze where each level has passages in different 
// orientations. Ladders are placed to force long detours - you often need
// to travel in the "wrong" direction to find the ladder to the next level.
//
// This creates pathological cases where 2D heuristics are completely wrong:
// - Start and goal may be close in XY but require traversing the entire map
// - The "obvious" nearby ladder often leads to a dead end
// ============================================================================

// Helper: Place a ladder connecting zLow and zHigh near (targetX, targetY)
// Searches within radius for a spot where both levels have floor
static void PlaceLadderNear(int targetX, int targetY, int zLow, int zHigh, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int x = targetX + dx;
            int y = targetY + dy;
            if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
                if (IsFloorCell(x, y, zLow) && IsFloorCell(x, y, zHigh)) {
                    grid[zLow][y][x] = CELL_LADDER_BOTH;
                    grid[zHigh][y][x] = CELL_LADDER_BOTH;
                    return;
                }
            }
        }
    }
}

void GenerateLabyrinth3D(void) {
    InitGrid();  // Clear cells and flags
    FillGroundLevel();
    
    // z=0 is ground (dirt), buildings start at z=1
    int baseZ = 1;
    int numLevels = 4;
    int maxLevels = gridDepth - baseZ;
    if (numLevels > maxLevels) numLevels = maxLevels;
    
    // Fill labyrinth levels with rock (to carve passages)
    for (int z = baseZ; z < baseZ + numLevels; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_GRANITE);
            }
        }
    }
    
    int passageWidth = 2;
    int wallThickness = 3;
    int spacing = passageWidth + wallThickness;
    
    int z0 = baseZ + 0;
    int z1 = baseZ + 1;
    int z2 = baseZ + 2;
    int z3 = baseZ + 3;
    
    // Level 0: Horizontal passages (East-West) with west-side vertical connector
    for (int y = spacing; y < gridHeight - spacing; y += spacing) {
        for (int x = 1; x < gridWidth - 1; x++) {
            for (int w = 0; w < passageWidth && y + w < gridHeight - 1; w++) {
                PlaceFloor(x, y + w, z0, MAT_GRANITE);
            }
        }
    }
    int westConnectorX = gridWidth / 6;
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int w = 0; w < passageWidth && westConnectorX + w < gridWidth; w++) {
            PlaceFloor(westConnectorX + w, y, z0, MAT_GRANITE);
        }
    }
    
    // Level 1: Vertical passages (North-South) with south-side horizontal connector
    for (int x = spacing; x < gridWidth - spacing; x += spacing) {
        for (int y = 1; y < gridHeight - 1; y++) {
            for (int w = 0; w < passageWidth && x + w < gridWidth - 1; w++) {
                PlaceFloor(x + w, y, z1, MAT_GRANITE);
            }
        }
    }
    int southConnectorY = gridHeight - gridHeight / 6;
    for (int x = 1; x < gridWidth - 1; x++) {
        for (int w = 0; w < passageWidth && southConnectorY + w < gridHeight; w++) {
            PlaceFloor(x, southConnectorY + w, z1, MAT_GRANITE);
        }
    }
    
    // Level 2: Horizontal passages (offset from level 0) with east-side vertical connector
    int offset = spacing / 2;
    for (int y = spacing + offset; y < gridHeight - spacing; y += spacing) {
        for (int x = 1; x < gridWidth - 1; x++) {
            for (int w = 0; w < passageWidth && y + w < gridHeight - 1; w++) {
                PlaceFloor(x, y + w, z2, MAT_GRANITE);
            }
        }
    }
    int eastConnectorX = gridWidth - gridWidth / 6;
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int w = 0; w < passageWidth && eastConnectorX + w < gridWidth; w++) {
            PlaceFloor(eastConnectorX + w, y, z2, MAT_GRANITE);
        }
    }
    
    // Level 3: Open grid pattern (destination level)
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int x = 1; x < gridWidth - 1; x++) {
            if ((y % spacing) < passageWidth || (x % spacing) < passageWidth) {
                PlaceFloor(x, y, z3, MAT_GRANITE);
            }
        }
    }
    
    // Place ladders to force West→South→East traversal pattern
    // z=0→z=1: West region (forces westward travel on level 0)
    PlaceLadderNear(gridWidth / 8, gridHeight / 2, z0, z1, 5);
    PlaceLadderNear(gridWidth / 5, gridHeight * 3 / 4, z0, z1, 5);
    
    // z=1→z=2: South region (forces southward travel on level 1)
    PlaceLadderNear(gridWidth / 2, gridHeight - gridHeight / 8, z1, z2, 5);
    PlaceLadderNear(gridWidth / 4, gridHeight - gridHeight / 6, z1, z2, 5);
    
    // z=2→z=3: East region (forces eastward travel on level 2)
    PlaceLadderNear(gridWidth - gridWidth / 8, gridHeight / 2, z2, z3, 5);
    PlaceLadderNear(gridWidth - gridWidth / 6, gridHeight / 4, z2, z3, 5);
    
    needsRebuild = true;
}

// ============================================================================
// Spiral3D Generator
// Creates concentric rings on each level, where the exit from each ring
// is on a different side. Combined with ladders at specific positions,
// this forces a spiral traversal pattern through the levels.
//
// Level 0: Exit NORTH
// Level 1: Exit EAST  
// Level 2: Exit SOUTH
// Level 3: Goal in center
// ============================================================================

void GenerateSpiral3D(void) {
    InitGrid();  // Clear cells and flags
    FillGroundLevel();
    
    // z=0 is ground (dirt), buildings start at z=1
    int baseZ = 1;
    int numLevels = 4;
    int maxLevels = gridDepth - baseZ;
    if (numLevels > maxLevels) numLevels = maxLevels;
    
    // Fill spiral levels with floor
    for (int z = baseZ; z < baseZ + numLevels; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                PlaceFloor(x, y, z, MAT_GRANITE);
            }
        }
    }
    
    int centerX = gridWidth / 2;
    int centerY = gridHeight / 2;
    
    // Ring parameters
    int ringSpacing = 8;
    int wallThickness = 2;
    int gapSize = 3;
    
    int numRings = (gridWidth < gridHeight ? gridWidth : gridHeight) / (2 * ringSpacing) - 1;
    if (numRings < 3) numRings = 3;
    if (numRings > 8) numRings = 8;
    
    // Build rings on levels 0, 1, 2 (relative to baseZ)
    for (int level = 0; level < 3 && level < numLevels; level++) {
        int z = baseZ + level;
        // Determine which side has the gap for this level
        // 0 = North, 1 = East, 2 = South, 3 = West
        int gapSide = level;  // Level 0: North, Level 1: East, Level 2: South
        
        for (int ring = 0; ring < numRings; ring++) {
            int ringDist = (ring + 1) * ringSpacing;
            
            int left = centerX - ringDist;
            int right = centerX + ringDist;
            int top = centerY - ringDist;
            int bottom = centerY + ringDist;
            
            // Clamp to grid
            if (left < 1) left = 1;
            if (top < 1) top = 1;
            if (right >= gridWidth - 1) right = gridWidth - 2;
            if (bottom >= gridHeight - 1) bottom = gridHeight - 2;
            
            // Calculate gap position for this side
            int gapCenter;
            
            // North wall
            gapCenter = centerX;
            for (int x = left; x <= right; x++) {
                bool isGap = (gapSide == 0) && (x >= gapCenter - gapSize/2) && (x <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && top + t < gridHeight; t++) {
                        grid[z][top + t][x] = CELL_WALL;
                        SetWallMaterial(x, top + t, z, MAT_GRANITE);
                    }
                }
            }
            
            // South wall
            gapCenter = centerX;
            for (int x = left; x <= right; x++) {
                bool isGap = (gapSide == 2) && (x >= gapCenter - gapSize/2) && (x <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && bottom - t >= 0; t++) {
                        grid[z][bottom - t][x] = CELL_WALL;
                        SetWallMaterial(x, bottom - t, z, MAT_GRANITE);
                    }
                }
            }
            
            // West wall
            gapCenter = centerY;
            for (int y = top; y <= bottom; y++) {
                bool isGap = (gapSide == 3) && (y >= gapCenter - gapSize/2) && (y <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && left + t < gridWidth; t++) {
                        grid[z][y][left + t] = CELL_WALL;
                        SetWallMaterial(left + t, y, z, MAT_GRANITE);
                    }
                }
            }
            
            // East wall
            gapCenter = centerY;
            for (int y = top; y <= bottom; y++) {
                bool isGap = (gapSide == 1) && (y >= gapCenter - gapSize/2) && (y <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && right - t >= 0; t++) {
                        grid[z][y][right - t] = CELL_WALL;
                        SetWallMaterial(right - t, y, z, MAT_GRANITE);
                    }
                }
            }
        }
    }
    
    // Level 3: Open center area (the goal destination)
    // Just leave it as floor (already initialized)
    // Add some decorative walls to make it interesting
    int z3 = baseZ + 3;
    int innerRing = ringSpacing;
    for (int x = centerX - innerRing; x <= centerX + innerRing; x++) {
        if (x > 0 && x < gridWidth - 1) {
            grid[z3][centerY - innerRing][x] = CELL_WALL;
            SetWallMaterial(x, centerY - innerRing, z3, MAT_GRANITE);
            grid[z3][centerY + innerRing][x] = CELL_WALL;
            SetWallMaterial(x, centerY + innerRing, z3, MAT_GRANITE);
        }
    }
    for (int y = centerY - innerRing; y <= centerY + innerRing; y++) {
        if (y > 0 && y < gridHeight - 1) {
            grid[z3][y][centerX - innerRing] = CELL_WALL;
            SetWallMaterial(centerX - innerRing, y, z3, MAT_GRANITE);
            grid[z3][y][centerX + innerRing] = CELL_WALL;
            SetWallMaterial(centerX + innerRing, y, z3, MAT_GRANITE);
        }
    }
    // Gaps on all sides for level 3
    for (int i = -gapSize/2; i <= gapSize/2; i++) {
        if (centerX + i > 0 && centerX + i < gridWidth - 1) {
            PlaceFloor(centerX + i, centerY - innerRing, z3, MAT_GRANITE);
            PlaceFloor(centerX + i, centerY + innerRing, z3, MAT_GRANITE);
        }
        if (centerY + i > 0 && centerY + i < gridHeight - 1) {
            PlaceFloor(centerX - innerRing, centerY + i, z3, MAT_GRANITE);
            PlaceFloor(centerX + innerRing, centerY + i, z3, MAT_GRANITE);
        }
    }
    
    // Place ladders at specific positions to force the spiral
    // Use z0, z1, z2, z3 for the actual z indices
    int z0 = baseZ + 0;
    int z1 = baseZ + 1;
    int z2 = baseZ + 2;
    // z3 already defined above
    
    // Ladder 0→1: Must be accessed from NORTH (outside the outermost ring, north side)
    // After exiting north on level 0, climb to level 1
    int outerRingDist = numRings * ringSpacing;
    
    int ladder01_x = centerX;
    int ladder01_y = centerY - outerRingDist - ringSpacing/2;
    if (ladder01_y < 2) ladder01_y = 2;
    // Place ladder
    grid[z0][ladder01_y][ladder01_x] = CELL_LADDER_BOTH;
    grid[z1][ladder01_y][ladder01_x] = CELL_LADDER_BOTH;
    // Ensure path to ladder is clear
    for (int y = ladder01_y; y < centerY - outerRingDist; y++) {
        if (grid[z0][y][ladder01_x] != CELL_LADDER_BOTH) PlaceFloor(ladder01_x, y, z0, MAT_GRANITE);
        if (grid[z1][y][ladder01_x] != CELL_LADDER_BOTH) PlaceFloor(ladder01_x, y, z1, MAT_GRANITE);
    }
    
    // Ladder 1→2: Must be accessed from EAST (outside the outermost ring, east side)
    int ladder12_x = centerX + outerRingDist + ringSpacing/2;
    int ladder12_y = centerY;
    if (ladder12_x >= gridWidth - 2) ladder12_x = gridWidth - 3;
    grid[z1][ladder12_y][ladder12_x] = CELL_LADDER_BOTH;
    grid[z2][ladder12_y][ladder12_x] = CELL_LADDER_BOTH;
    // Ensure path to ladder is clear
    for (int x = centerX + outerRingDist; x <= ladder12_x; x++) {
        if (grid[z1][ladder12_y][x] != CELL_LADDER_BOTH) PlaceFloor(x, ladder12_y, z1, MAT_GRANITE);
        if (grid[z2][ladder12_y][x] != CELL_LADDER_BOTH) PlaceFloor(x, ladder12_y, z2, MAT_GRANITE);
    }
    
    // Ladder 2→3: Must be accessed from SOUTH (outside the outermost ring, south side)
    int ladder23_x = centerX;
    int ladder23_y = centerY + outerRingDist + ringSpacing/2;
    if (ladder23_y >= gridHeight - 2) ladder23_y = gridHeight - 3;
    grid[z2][ladder23_y][ladder23_x] = CELL_LADDER_BOTH;
    grid[z3][ladder23_y][ladder23_x] = CELL_LADDER_BOTH;
    // Ensure path to ladder is clear (from outer ring to ladder)
    for (int y = centerY + outerRingDist; y <= ladder23_y; y++) {
        if (grid[z2][y][ladder23_x] != CELL_LADDER_BOTH) PlaceFloor(ladder23_x, y, z2, MAT_GRANITE);
        if (grid[z3][y][ladder23_x] != CELL_LADDER_BOTH) PlaceFloor(ladder23_x, y, z3, MAT_GRANITE);
    }
    // On z3: ensure path from ladder to the inner ring (south gap)
    // The inner ring is at centerY + innerRing, so connect from ladder to there
    for (int y = centerY + innerRing; y <= ladder23_y; y++) {
        if (grid[z3][y][ladder23_x] != CELL_LADDER_BOTH) PlaceFloor(ladder23_x, y, z3, MAT_GRANITE);
    }
    
    // Add a few "decoy" ladders that lead to dead ends or longer routes
    // Decoy 1: East side of level 0 - goes up but you're stuck
    int decoy1_x = centerX + outerRingDist + ringSpacing/2;
    int decoy1_y = centerY - ringSpacing;
    if (decoy1_x < gridWidth - 2 && decoy1_y > 1) {
        grid[z0][decoy1_y][decoy1_x] = CELL_LADDER_BOTH;
        grid[z1][decoy1_y][decoy1_x] = CELL_LADDER_BOTH;
        // Make sure there's floor around it
        if (grid[z0][decoy1_y][decoy1_x - 1] == CELL_WALL && GetWallMaterial(decoy1_x - 1, decoy1_y, z0) == MAT_GRANITE) PlaceFloor(decoy1_x - 1, decoy1_y, z0, MAT_GRANITE);
        if (grid[z1][decoy1_y][decoy1_x - 1] == CELL_WALL && GetWallMaterial(decoy1_x - 1, decoy1_y, z1) == MAT_GRANITE) PlaceFloor(decoy1_x - 1, decoy1_y, z1, MAT_GRANITE);
    }
    
    // Decoy 2: Center area ladder that skips a level but puts you in a bad spot
    int decoy2_x = centerX - ringSpacing;
    int decoy2_y = centerY - ringSpacing;
    if (decoy2_x > 1 && decoy2_y > 1) {
        grid[z0][decoy2_y][decoy2_x] = CELL_LADDER_BOTH;
        grid[z1][decoy2_y][decoy2_x] = CELL_LADDER_BOTH;
    }
    
    needsRebuild = true;
}

// ============================================================================
// Feature-based Dungeon Generator (Rooms & Corridors)
// ============================================================================

#define MAX_ROOMS 256
#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 12
#define CORRIDOR_WIDTH 2

static Room dungeonRooms[MAX_ROOMS];
static int dungeonRoomCount = 0;

static void CarveRoom(int x, int y, int w, int h) {
    for (int py = y; py < y + h && py < gridHeight; py++) {
        for (int px = x; px < x + w && px < gridWidth; px++) {
            if (px >= 0 && py >= 0) {
                grid[1][py][px] = CELL_AIR;  // Carve at z=1 (walking level)
            }
        }
    }
}

static void CarveCorridor(int x1, int y1, int x2, int y2) {
    int x = x1, y = y1;
    
    // Randomly choose horizontal or vertical first
    if (GetRandomValue(0, 1) == 0) {
        // Horizontal then vertical
        while (x != x2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y + w >= 0 && y + w < gridHeight && x >= 0 && x < gridWidth)
                    grid[1][y + w][x] = CELL_AIR;
            }
            x += (x2 > x) ? 1 : -1;
        }
        while (y != y2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y >= 0 && y < gridHeight && x + w >= 0 && x + w < gridWidth)
                    grid[1][y][x + w] = CELL_AIR;
            }
            y += (y2 > y) ? 1 : -1;
        }
    } else {
        // Vertical then horizontal
        while (y != y2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y >= 0 && y < gridHeight && x + w >= 0 && x + w < gridWidth)
                    grid[1][y][x + w] = CELL_AIR;
            }
            y += (y2 > y) ? 1 : -1;
        }
        while (x != x2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y + w >= 0 && y + w < gridHeight && x >= 0 && x < gridWidth)
                    grid[1][y + w][x] = CELL_AIR;
            }
            x += (x2 > x) ? 1 : -1;
        }
    }
}

static bool RoomOverlaps(int x, int y, int w, int h, int margin) {
    if (x - margin < 0 || y - margin < 0 || 
        x + w + margin >= gridWidth || y + h + margin >= gridHeight) {
        return true;
    }
    for (int i = 0; i < dungeonRoomCount; i++) {
        Room* r = &dungeonRooms[i];
        if (x < r->x + r->w + margin && x + w + margin > r->x &&
            y < r->y + r->h + margin && y + h + margin > r->y) {
            return true;
        }
    }
    return false;
}

void GenerateDungeonRooms(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    
    // Fill z=1 with rock (to carve rooms)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[1][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_GRANITE);
        }
    
    dungeonRoomCount = 0;
    
    // Place first room in center
    int firstW = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
    int firstH = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
    int firstX = (gridWidth - firstW) / 2;
    int firstY = (gridHeight - firstH) / 2;
    
    CarveRoom(firstX, firstY, firstW, firstH);
    dungeonRooms[dungeonRoomCount++] = (Room){firstX, firstY, firstW, firstH};
    
    // Try to add more rooms
    int attempts = 500;
    int maxRooms = 30 + (gridWidth * gridHeight) / 500;
    if (maxRooms > MAX_ROOMS) maxRooms = MAX_ROOMS;
    
    for (int i = 0; i < attempts && dungeonRoomCount < maxRooms; i++) {
        Room* source = &dungeonRooms[GetRandomValue(0, dungeonRoomCount - 1)];
        
        int newW = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
        int newH = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
        int side = GetRandomValue(0, 3);
        int newX, newY;
        int corridorLen = GetRandomValue(2, 8);
        
        switch (side) {
            case 0: // North
                newX = source->x + GetRandomValue(0, source->w - 1) - newW / 2;
                newY = source->y - corridorLen - newH;
                break;
            case 1: // East
                newX = source->x + source->w + corridorLen;
                newY = source->y + GetRandomValue(0, source->h - 1) - newH / 2;
                break;
            case 2: // South
                newX = source->x + GetRandomValue(0, source->w - 1) - newW / 2;
                newY = source->y + source->h + corridorLen;
                break;
            default: // West
                newX = source->x - corridorLen - newW;
                newY = source->y + GetRandomValue(0, source->h - 1) - newH / 2;
                break;
        }
        
        if (!RoomOverlaps(newX, newY, newW, newH, 2)) {
            CarveRoom(newX, newY, newW, newH);
            dungeonRooms[dungeonRoomCount++] = (Room){newX, newY, newW, newH};
            
            int srcCenterX = source->x + source->w / 2;
            int srcCenterY = source->y + source->h / 2;
            int newCenterX = newX + newW / 2;
            int newCenterY = newY + newH / 2;
            CarveCorridor(srcCenterX, srcCenterY, newCenterX, newCenterY);
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Cellular Automata Cave Generator
// ============================================================================

void GenerateCaves(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    
    // Start with random noise
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Border is always rock
            if (x == 0 || y == 0 || x == gridWidth - 1 || y == gridHeight - 1) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_GRANITE);
            } else {
                if (GetRandomValue(0, 100) < 45) {
                    grid[0][y][x] = CELL_WALL;
                    SetWallMaterial(x, y, 0, MAT_GRANITE);
                } else {
                    grid[0][y][x] = CELL_WALL;
                    SetWallMaterial(x, y, 0, MAT_DIRT);
                }
            }
        }
    }
    
    // Temporary buffer for cellular automata
    MaterialType* temp = (MaterialType*)malloc(gridWidth * gridHeight * sizeof(MaterialType));
    
    // Run cellular automata iterations
    for (int iter = 0; iter < 5; iter++) {
        for (int y = 1; y < gridHeight - 1; y++) {
            for (int x = 1; x < gridWidth - 1; x++) {
                // Count neighboring rock
                int walls = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (GetWallMaterial(x + dx, y + dy, 0) == MAT_GRANITE) walls++;
                    }
                }
                // 4-5 rule: become rock if >= 5 neighbors are rock
                temp[y * gridWidth + x] = (walls >= 5) ? MAT_GRANITE : MAT_DIRT;
            }
        }
        // Copy back
        for (int y = 1; y < gridHeight - 1; y++) {
            for (int x = 1; x < gridWidth - 1; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, (MaterialType)temp[y * gridWidth + x]);
            }
        }
    }
    
    free(temp);
    
    // Ensure connectivity by flood-filling from center and connecting islands
    // (Simple approach: just carve a path from center to ensure some walkable area)
    int cx = gridWidth / 2, cy = gridHeight / 2;
    for (int r = 0; r < 5; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int nx = cx + dx, ny = cy + dy;
                if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
                    grid[0][ny][nx] = CELL_WALL;
                    SetWallMaterial(nx, ny, 0, MAT_DIRT);
                }
            }
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Drunkard's Walk Generator
// ============================================================================

void GenerateDrunkard(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    
    // Fill z=1 with rock (to carve)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[1][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_GRANITE);
        }
    
    // Start from center
    int x = gridWidth / 2;
    int y = gridHeight / 2;
    
    // Target: carve out ~40% of the map
    int targetFloor = (gridWidth * gridHeight * 40) / 100;
    int floorCount = 0;
    
    int maxSteps = gridWidth * gridHeight * 10;  // Prevent infinite loop
    
    for (int step = 0; step < maxSteps && floorCount < targetFloor; step++) {
        // Carve current position
        if (grid[1][y][x] == CELL_WALL && GetWallMaterial(x, y, 1) == MAT_GRANITE) {
            grid[1][y][x] = CELL_AIR;
            floorCount++;
        }
        
        // Random walk
        int dir = GetRandomValue(0, 3);
        switch (dir) {
            case 0: if (y > 1) y--; break;              // North
            case 1: if (x < gridWidth - 2) x++; break;  // East
            case 2: if (y < gridHeight - 2) y++; break; // South
            case 3: if (x > 1) x--; break;              // West
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Tunneler Algorithm (Rooms and Corridors)
// Based on classic roguelike approach: place rooms, connect with corridors
// ============================================================================

typedef struct {
    int x, y, w, h;
} TunnelRoom;

static bool TunnelRoomsIntersect(TunnelRoom* a, TunnelRoom* b) {
    // Add 1 tile padding between rooms
    return (a->x <= b->x + b->w + 1 && a->x + a->w + 1 >= b->x &&
            a->y <= b->y + b->h + 1 && a->y + a->h + 1 >= b->y);
}

static void CarveTunnelRoom(TunnelRoom* room) {
    for (int y = room->y; y < room->y + room->h; y++) {
        for (int x = room->x; x < room->x + room->w; x++) {
            if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
                grid[1][y][x] = CELL_AIR;
            }
        }
    }
}

static void CarveHorizontalTunnel(int x1, int x2, int y) {
    int minX = x1 < x2 ? x1 : x2;
    int maxX = x1 > x2 ? x1 : x2;
    for (int x = minX; x <= maxX; x++) {
        if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
            grid[1][y][x] = CELL_AIR;
        }
    }
}

static void CarveVerticalTunnel(int y1, int y2, int x) {
    int minY = y1 < y2 ? y1 : y2;
    int maxY = y1 > y2 ? y1 : y2;
    for (int y = minY; y <= maxY; y++) {
        if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
            grid[1][y][x] = CELL_AIR;
        }
    }
}

void GenerateTunneler(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    
    // Fill z=1 with rock (to carve)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[1][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_GRANITE);
        }
    
    // Scale room count based on world size
    // Roughly 1 room per 150 tiles, with min 5 and max 100
    int maxRooms = (gridWidth * gridHeight) / 150;
    if (maxRooms < 5) maxRooms = 5;
    if (maxRooms > 100) maxRooms = 100;
    
    TunnelRoom rooms[100];
    int roomCount = 0;
    
    // Try to place rooms
    for (int i = 0; i < maxRooms * 3; i++) {  // More attempts than max rooms
        if (roomCount >= maxRooms) break;
        
        // Random room size
        int w = 4 + GetRandomValue(0, 6);
        int h = 4 + GetRandomValue(0, 6);
        
        // Random position (with margin from edges)
        int rx = 2 + GetRandomValue(0, gridWidth - w - 4);
        int ry = 2 + GetRandomValue(0, gridHeight - h - 4);
        
        TunnelRoom newRoom = {rx, ry, w, h};
        
        // Check for overlaps with existing rooms
        bool overlaps = false;
        for (int r = 0; r < roomCount; r++) {
            if (TunnelRoomsIntersect(&newRoom, &rooms[r])) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            // Carve the room
            CarveTunnelRoom(&newRoom);
            
            // Connect to previous room with corridors
            if (roomCount > 0) {
                int newCenterX = newRoom.x + newRoom.w / 2;
                int newCenterY = newRoom.y + newRoom.h / 2;
                int prevCenterX = rooms[roomCount - 1].x + rooms[roomCount - 1].w / 2;
                int prevCenterY = rooms[roomCount - 1].y + rooms[roomCount - 1].h / 2;
                
                // Randomly choose horizontal-first or vertical-first
                if (GetRandomValue(0, 1) == 0) {
                    CarveHorizontalTunnel(prevCenterX, newCenterX, prevCenterY);
                    CarveVerticalTunnel(prevCenterY, newCenterY, newCenterX);
                } else {
                    CarveVerticalTunnel(prevCenterY, newCenterY, prevCenterX);
                    CarveHorizontalTunnel(prevCenterX, newCenterX, newCenterY);
                }
            }
            
            rooms[roomCount++] = newRoom;
        }
    }
    
    needsRebuild = true;
}

void GenerateMixMax(void) {
    // First run tunneler (rooms + corridors)
    GenerateTunneler();
    
    // Then add more rooms on top using dungeon style
    // (Don't fill with walls - just carve more rooms)
    int extraRooms = (gridWidth * gridHeight) / 300;  // Half as many extra rooms
    if (extraRooms < 3) extraRooms = 3;
    if (extraRooms > 50) extraRooms = 50;
    
    int added = 0;
    for (int i = 0; i < extraRooms * 5 && added < extraRooms; i++) {
        int w = 4 + GetRandomValue(0, 8);
        int h = 4 + GetRandomValue(0, 8);
        int rx = 2 + GetRandomValue(0, gridWidth - w - 4);
        int ry = 2 + GetRandomValue(0, gridHeight - h - 4);
        
        // Just carve it - overlaps are fine, creates more interesting shapes
        for (int y = ry; y < ry + h && y < gridHeight - 1; y++) {
            for (int x = rx; x < rx + w && x < gridWidth - 1; x++) {
                if (x > 0 && y > 0) {
                    grid[1][y][x] = CELL_AIR;
                }
            }
        }
        added++;
    }
    
    needsRebuild = true;
}

void GenerateConcentricMaze(void) {
    InitGrid();
    
    // Create concentric rectangular rings with gaps
    // Each ring has a gap on alternating sides to force spiraling
    int minDim = gridWidth < gridHeight ? gridWidth : gridHeight;
    int ringSpacing = 4;  // space between rings
    int wallThickness = 2;
    int gapSize = 3;
    
    int ringCount = (minDim / 2) / ringSpacing;
    
    for (int ring = 0; ring < ringCount; ring++) {
        int offset = ring * ringSpacing;
        int left = offset;
        int right = gridWidth - 1 - offset;
        int top = offset;
        int bottom = gridHeight - 1 - offset;
        
        // Skip if ring is too small
        if (right - left < gapSize * 2 || bottom - top < gapSize * 2) break;
        
        // Determine gap position - alternate sides for each ring
        // Ring 0: gap on right, Ring 1: gap on bottom, Ring 2: gap on left, Ring 3: gap on top, etc.
        int gapSide = ring % 4;
        int gapStart;
        
        // Draw top wall
        if (gapSide == 3) {
            gapStart = left + (right - left) / 2 - gapSize / 2;
        }
        for (int x = left; x <= right; x++) {
            if (gapSide == 3 && x >= gapStart && x < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && top + t < gridHeight; t++) {
                grid[1][top + t][x] = CELL_WALL;
                SetWallMaterial(x, top + t, 1, MAT_GRANITE);
            }
        }
        
        // Draw bottom wall
        if (gapSide == 1) {
            gapStart = left + (right - left) / 2 - gapSize / 2;
        }
        for (int x = left; x <= right; x++) {
            if (gapSide == 1 && x >= gapStart && x < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && bottom - t >= 0; t++) {
                grid[1][bottom - t][x] = CELL_WALL;
                SetWallMaterial(x, bottom - t, 1, MAT_GRANITE);
            }
        }
        
        // Draw left wall
        if (gapSide == 2) {
            gapStart = top + (bottom - top) / 2 - gapSize / 2;
        }
        for (int y = top; y <= bottom; y++) {
            if (gapSide == 2 && y >= gapStart && y < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && left + t < gridWidth; t++) {
                grid[1][y][left + t] = CELL_WALL;
                SetWallMaterial(left + t, y, 1, MAT_GRANITE);
            }
        }
        
        // Draw right wall
        if (gapSide == 0) {
            gapStart = top + (bottom - top) / 2 - gapSize / 2;
        }
        for (int y = top; y <= bottom; y++) {
            if (gapSide == 0 && y >= gapStart && y < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && right - t >= 0; t++) {
                grid[1][y][right - t] = CELL_WALL;
                SetWallMaterial(right - t, y, 1, MAT_GRANITE);
            }
        }
    }
    
    needsRebuild = true;
}

void InitPerlin(int seed) {
    SetRandomSeed(seed);
    for (int i = 0; i < 256; i++) permutation[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int tmp = permutation[i];
        permutation[i] = permutation[j];
        permutation[j] = tmp;
    }
    for (int i = 0; i < 256; i++) permutation[256 + i] = permutation[i];
}

static float MyFade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static float MyLerp(float a, float b, float t) { return a + t * (b - a); }
static float Grad(int hash, float x, float y) {
    int h = hash & 3;
    float u = h < 2 ? x : y;
    float v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Perlin(float x, float y) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float u = MyFade(xf);
    float v = MyFade(yf);
    int aa = permutation[permutation[xi] + yi];
    int ab = permutation[permutation[xi] + yi + 1];
    int ba = permutation[permutation[xi + 1] + yi];
    int bb = permutation[permutation[xi + 1] + yi + 1];
    float x1 = MyLerp(Grad(aa, xf, yf), Grad(ba, xf - 1, yf), u);
    float x2 = MyLerp(Grad(ab, xf, yf - 1), Grad(bb, xf - 1, yf - 1), u);
    return (MyLerp(x1, x2, v) + 1) / 2;
}

float OctavePerlin(float x, float y, int octaves, float persistence) {
    float total = 0, freq = 1, amp = 1, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        total += Perlin(x * freq, y * freq) * amp;
        maxVal += amp;
        amp *= persistence;
        freq *= 2;
    }
    return total / maxVal;
}

// ============================================================================
// 3D Perlin Noise (for volumetric terrain)
// ============================================================================

static float Grad3D(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Perlin3D(float x, float y, float z) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;
    int zi = (int)floorf(z) & 255;
    
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float zf = z - floorf(z);
    
    float u = MyFade(xf);
    float v = MyFade(yf);
    float w = MyFade(zf);
    
    int aaa = permutation[permutation[permutation[xi] + yi] + zi];
    int aba = permutation[permutation[permutation[xi] + yi + 1] + zi];
    int aab = permutation[permutation[permutation[xi] + yi] + zi + 1];
    int abb = permutation[permutation[permutation[xi] + yi + 1] + zi + 1];
    int baa = permutation[permutation[permutation[xi + 1] + yi] + zi];
    int bba = permutation[permutation[permutation[xi + 1] + yi + 1] + zi];
    int bab = permutation[permutation[permutation[xi + 1] + yi] + zi + 1];
    int bbb = permutation[permutation[permutation[xi + 1] + yi + 1] + zi + 1];
    
    float x1 = MyLerp(Grad3D(aaa, xf, yf, zf), Grad3D(baa, xf - 1, yf, zf), u);
    float x2 = MyLerp(Grad3D(aba, xf, yf - 1, zf), Grad3D(bba, xf - 1, yf - 1, zf), u);
    float y1 = MyLerp(x1, x2, v);
    
    float x3 = MyLerp(Grad3D(aab, xf, yf, zf - 1), Grad3D(bab, xf - 1, yf, zf - 1), u);
    float x4 = MyLerp(Grad3D(abb, xf, yf - 1, zf - 1), Grad3D(bbb, xf - 1, yf - 1, zf - 1), u);
    float y2 = MyLerp(x3, x4, v);
    
    return (MyLerp(y1, y2, w) + 1.0f) / 2.0f;
}

float OctavePerlin3D(float x, float y, float z, int octaves, float persistence) {
    float total = 0, freq = 1, amp = 1, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        total += Perlin3D(x * freq, y * freq, z * freq) * amp;
        maxVal += amp;
        amp *= persistence;
        freq *= 2;
    }
    return total / maxVal;
}

// ============================================================================
// Hills/Mountains Generator
// Uses 2D Perlin noise for heightmap, fills with dirt up to that height
// Creates natural rolling hills and mountains
// ============================================================================

void GenerateHills(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    InitPerlin((int)worldSeed);
    
    // Randomized parameters (defaults: scale=0.02, octaves=4, persistence=0.5, height=1-14)
    // scale: 0.01 = very large rolling hills, 0.05 = small bumpy terrain
    float scale = 0.01f + (GetRandomValue(0, 40) / 1000.0f);  // 0.01 to 0.05
    
    // octaves: more = more detail/roughness, fewer = smoother
    int octaves = 2 + GetRandomValue(0, 4);  // 2 to 6
    
    // persistence: higher = rougher terrain, lower = smoother
    float persistence = 0.3f + (GetRandomValue(0, 40) / 100.0f);  // 0.3 to 0.7
    
    // height range
    int maxHeight = gridDepth - 2 - GetRandomValue(0, 3);  // Leave 2-5 at top
    int minHeight = 1 + GetRandomValue(0, 2);  // 1 to 3
    if (maxHeight <= minHeight) maxHeight = minHeight + 2;
    
    TraceLog(LOG_INFO, "GenerateHills: scale=%.3f, octaves=%d, persistence=%.2f, height=%d-%d",
             scale, octaves, persistence, minHeight, maxHeight);
    
    // Store heightmap for ramp placement pass
    int* heightmap = (int*)malloc(gridWidth * gridHeight * sizeof(int));
    
    // Generate heightmap using 2D Perlin noise
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Multi-octave noise for natural terrain
            float n = OctavePerlin(x * scale, y * scale, octaves, persistence);
            
            // Map noise to height (0-1 -> minHeight to maxHeight)
            int height = minHeight + (int)(n * (maxHeight - minHeight));
            if (height < minHeight) height = minHeight;
            if (height >= gridDepth) height = gridDepth - 1;
            
            heightmap[y * gridWidth + x] = height;
            
            // Fill with dirt from z=0 up to height
            for (int z = 0; z <= height; z++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_DIRT);
            }
            
            // Add grass surface on top
            SET_CELL_SURFACE(x, y, height, SURFACE_TALL_GRASS);
        }
    }

    // Rock layer: keep a soil band on top, rock below
    const int soilDepth = 4;  // Topsoil + subsoil depth
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int height = heightmap[y * gridWidth + x];
            if (height < soilDepth) continue;  // too shallow for rock
            int rockStartZ = height - soilDepth;
            for (int z = 0; z <= rockStartZ; z++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_GRANITE);
            }
        }
    }
    
    // Second pass: Place ramps by carving into hillsides
    // For natural-looking hills, carve the ramp INTO the higher terrain.
    // 
    // If cell A has height 5 and neighbor B (north) has height 6:
    //   - B has dirt at z6, A has dirt at z5
    //   - Carve RAMP_S into B at z6 (replacing dirt with ramp)
    //   - The ramp points SOUTH toward A
    //   - Enter from north (B's surface at z7), exit south to z6
    //   - Exit at z6 is A's walking surface (air above z5 dirt) - walkable!
    //
    // This places the ramp visually embedded in the hillside.
    
    int rampCandidates = 0;
    int rampPlaced = 0;
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int myHeight = heightmap[y * gridWidth + x];
            
            // Check each neighbor - if THEY are higher, carve a ramp into THEIR dirt
            // The ramp points toward us (the lower cell) for going DOWN
            // Ramps are bidirectional so this also allows going UP
            
            // Check North (y-1): if north is higher, carve ramp into north cell
            // Ramp points NORTH (high side away from lower cell)
            // Enter from south (lower cell side) at rampZ, exit north at rampZ+1
            if (y > 0) {
                int northHeight = heightmap[(y - 1) * gridWidth + x];
                if (northHeight == myHeight + 1) {
                    int rampZ = northHeight;  // Carve into their top dirt level
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y - 1][x]) &&
                        ShouldPlaceRampAt(x, y - 1)) {
                        MaterialType rampMat = GetWallMaterial(x, y - 1, rampZ);
                        grid[rampZ][y - 1][x] = CELL_RAMP_N;
                        SetWallMaterial(x, y - 1, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            
            // Check East (x+1): if east is higher, carve ramp pointing EAST
            if (x < gridWidth - 1) {
                int eastHeight = heightmap[y * gridWidth + (x + 1)];
                if (eastHeight == myHeight + 1) {
                    int rampZ = eastHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y][x + 1]) &&
                        ShouldPlaceRampAt(x + 1, y)) {
                        MaterialType rampMat = GetWallMaterial(x + 1, y, rampZ);
                        grid[rampZ][y][x + 1] = CELL_RAMP_E;
                        SetWallMaterial(x + 1, y, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            
            // Check South (y+1): if south is higher, carve ramp pointing SOUTH
            if (y < gridHeight - 1) {
                int southHeight = heightmap[(y + 1) * gridWidth + x];
                if (southHeight == myHeight + 1) {
                    int rampZ = southHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y + 1][x]) &&
                        ShouldPlaceRampAt(x, y + 1)) {
                        MaterialType rampMat = GetWallMaterial(x, y + 1, rampZ);
                        grid[rampZ][y + 1][x] = CELL_RAMP_S;
                        SetWallMaterial(x, y + 1, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            
            // Check West (x-1): if west is higher, carve ramp pointing WEST
            if (x > 0) {
                int westHeight = heightmap[y * gridWidth + (x - 1)];
                if (westHeight == myHeight + 1) {
                    int rampZ = westHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y][x - 1]) &&
                        ShouldPlaceRampAt(x - 1, y)) {
                        MaterialType rampMat = GetWallMaterial(x - 1, y, rampZ);
                        grid[rampZ][y][x - 1] = CELL_RAMP_W;
                        SetWallMaterial(x - 1, y, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
        }
    }

    TraceLog(LOG_INFO, "GenerateHills: ramps %d/%d (density=%.2f scale=%.3f)",
             rampPlaced, rampCandidates, rampDensity, rampNoiseScale);
    
    free(heightmap);
    needsRebuild = true;
}

// ============================================================================
// Hills + Soils Generator
// Uses the Hills heightmap but adds clay/gravel/sand/peat distribution
// ============================================================================

void GenerateHillsSoils(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    InitPerlin((int)worldSeed);

    float scale = 0.01f + (GetRandomValue(0, 40) / 1000.0f);  // 0.01 to 0.05
    int octaves = 2 + GetRandomValue(0, 4);                  // 2 to 6
    float persistence = 0.3f + (GetRandomValue(0, 40) / 100.0f);  // 0.3 to 0.7

    int maxHeight = gridDepth - 2 - GetRandomValue(0, 3);  // Leave 2-5 at top
    int minHeight = 1 + GetRandomValue(0, 2);              // 1 to 3
    if (maxHeight <= minHeight) maxHeight = minHeight + 2;

    TraceLog(LOG_INFO, "GenerateHillsSoils: scale=%.3f, octaves=%d, persistence=%.2f, height=%d-%d",
             scale, octaves, persistence, minHeight, maxHeight);

    int* heightmap = (int*)malloc(gridWidth * gridHeight * sizeof(int));

    // Generate heightmap and fill with dirt
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            float n = OctavePerlin(x * scale, y * scale, octaves, persistence);

            int height = minHeight + (int)(n * (maxHeight - minHeight));
            if (height < minHeight) height = minHeight;
            if (height >= gridDepth) height = gridDepth - 1;

            heightmap[y * gridWidth + x] = height;

            for (int z = 0; z <= height; z++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_DIRT);
            }
        }
    }

    // Soil distribution parameters (tune as needed)
    float clayScale = 0.03f;
    float sandScale = 0.02f;
    float gravelScale = 0.02f;
    float peatScale = 0.02f;
    float clayThreshold = 0.58f;
    float sandNoise = 0.55f;
    float gravelNoise = 0.62f;
    float peatNoise = 0.55f;
    int topsoilDepth = 2;
    int clayDepth = 2;
    int soilDepth = topsoilDepth + clayDepth;

    // Rock layer: keep a soil band on top, rock below
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int height = heightmap[y * gridWidth + x];
            if (height < soilDepth) continue;  // too shallow for rock
            int rockStartZ = height - soilDepth;
            for (int z = 0; z <= rockStartZ; z++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_GRANITE);
            }
        }
    }

    // Apply surface soils and subsoil clay blobs
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int height = heightmap[y * gridWidth + x];

            // Slope proxy (max neighbor delta)
            int slope = 0;
            if (x > 0) slope = (abs(height - heightmap[y * gridWidth + (x - 1)]) > slope) ? abs(height - heightmap[y * gridWidth + (x - 1)]) : slope;
            if (x < gridWidth - 1) slope = (abs(height - heightmap[y * gridWidth + (x + 1)]) > slope) ? abs(height - heightmap[y * gridWidth + (x + 1)]) : slope;
            if (y > 0) slope = (abs(height - heightmap[(y - 1) * gridWidth + x]) > slope) ? abs(height - heightmap[(y - 1) * gridWidth + x]) : slope;
            if (y < gridHeight - 1) slope = (abs(height - heightmap[(y + 1) * gridWidth + x]) > slope) ? abs(height - heightmap[(y + 1) * gridWidth + x]) : slope;

            float heightNorm = (maxHeight == minHeight) ? 0.0f : (float)(height - minHeight) / (float)(maxHeight - minHeight);
            float wetness = 1.0f - heightNorm;

            float sandN = OctavePerlin(x * sandScale, y * sandScale, 3, 0.5f);
            float gravelN = OctavePerlin(x * gravelScale, y * gravelScale, 3, 0.5f);
            float peatN = OctavePerlin(x * peatScale, y * peatScale, 3, 0.5f);

            MaterialType surfaceMat = MAT_DIRT;
            if (wetness > 0.65f && peatN > peatNoise) {
                surfaceMat = MAT_PEAT;
            } else if (wetness < 0.35f && sandN > sandNoise) {
                surfaceMat = MAT_SAND;
            } else {
                bool rockBelow = (height >= soilDepth);
                float gravelThreshold = gravelNoise - (rockBelow ? 0.08f : 0.0f);
                if (gravelThreshold < 0.4f) gravelThreshold = 0.4f;
                if (slope >= 2 || gravelN > gravelThreshold) {
                    surfaceMat = MAT_GRAVEL;
                }
            }

            grid[height][y][x] = CELL_WALL;
            SetWallMaterial(x, y, height, surfaceMat);
            if (surfaceMat == MAT_DIRT) {
                SET_CELL_SURFACE(x, y, height, SURFACE_TALL_GRASS);
            } else {
                SET_CELL_SURFACE(x, y, height, SURFACE_BARE);
            }

            // Clay blobs in subsoil band
            float clayN = OctavePerlin(x * clayScale, y * clayScale, 3, 0.5f);
            if (clayN > clayThreshold) {
                int bandEnd = height - topsoilDepth;
                if (bandEnd > 0) {
                    int bandStart = bandEnd - clayDepth + 1;
                    if (bandStart < 0) bandStart = 0;
                    for (int z = bandStart; z <= bandEnd && z < gridDepth; z++) {
                        if (GetWallMaterial(x, y, z) == MAT_DIRT) {
                            SetWallMaterial(x, y, z, MAT_CLAY);
                        }
                    }
                }
            }
        }
    }

    // Ramp placement pass (same as GenerateHills)
    int rampCandidates = 0;
    int rampPlaced = 0;
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int myHeight = heightmap[y * gridWidth + x];

            if (y > 0) {
                int northHeight = heightmap[(y - 1) * gridWidth + x];
                if (northHeight == myHeight + 1) {
                    int rampZ = northHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y - 1][x]) &&
                        ShouldPlaceRampAt(x, y - 1)) {
                        MaterialType rampMat = GetWallMaterial(x, y - 1, rampZ);
                        grid[rampZ][y - 1][x] = CELL_RAMP_N;
                        SetWallMaterial(x, y - 1, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            if (x < gridWidth - 1) {
                int eastHeight = heightmap[y * gridWidth + (x + 1)];
                if (eastHeight == myHeight + 1) {
                    int rampZ = eastHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y][x + 1]) &&
                        ShouldPlaceRampAt(x + 1, y)) {
                        MaterialType rampMat = GetWallMaterial(x + 1, y, rampZ);
                        grid[rampZ][y][x + 1] = CELL_RAMP_E;
                        SetWallMaterial(x + 1, y, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            if (y < gridHeight - 1) {
                int southHeight = heightmap[(y + 1) * gridWidth + x];
                if (southHeight == myHeight + 1) {
                    int rampZ = southHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y + 1][x]) &&
                        ShouldPlaceRampAt(x, y + 1)) {
                        MaterialType rampMat = GetWallMaterial(x, y + 1, rampZ);
                        grid[rampZ][y + 1][x] = CELL_RAMP_S;
                        SetWallMaterial(x, y + 1, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            if (x > 0) {
                int westHeight = heightmap[y * gridWidth + (x - 1)];
                if (westHeight == myHeight + 1) {
                    int rampZ = westHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y][x - 1]) &&
                        ShouldPlaceRampAt(x - 1, y)) {
                        MaterialType rampMat = GetWallMaterial(x - 1, y, rampZ);
                        grid[rampZ][y][x - 1] = CELL_RAMP_W;
                        SetWallMaterial(x - 1, y, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
        }
    }

    TraceLog(LOG_INFO, "GenerateHillsSoils: ramps %d/%d (density=%.2f scale=%.3f)",
             rampPlaced, rampCandidates, rampDensity, rampNoiseScale);

    free(heightmap);
    needsRebuild = true;
}

// ============================================================================
// Hills + Soils + Water (Rivers/Lakes) Generator
// Adds rivers and shallow lakes, and biases clay/peat toward wetter areas
// ============================================================================

void GenerateHillsSoilsWater(void) {
    InitGrid();
    SeedTerrain();
    InitPerlin((int)worldSeed);
    InitWater();

    float scale = 0.01f + (GetRandomValue(0, 40) / 1000.0f);          // 0.01 to 0.05
    int octaves = 2 + GetRandomValue(0, 4);                           // 2 to 6
    float persistence = 0.3f + (GetRandomValue(0, 40) / 100.0f);      // 0.3 to 0.7

    int maxHeight = gridDepth - 3 - GetRandomValue(0, 3);            // Leave 3-6 at top
    int minHeight = 2 + GetRandomValue(0, 2);                        // 2 to 4
    if (maxHeight <= minHeight) maxHeight = minHeight + 3;

    TraceLog(LOG_INFO, "GenerateHillsSoilsWater: scale=%.3f, octaves=%d, persistence=%.2f, height=%d-%d",
             scale, octaves, persistence, minHeight, maxHeight);

    int cellCount = gridWidth * gridHeight;
    int* heightmap = (int*)malloc(cellCount * sizeof(int));
    bool* waterMask = (bool*)calloc(cellCount, sizeof(bool));
    bool* riverMask = (bool*)calloc(cellCount, sizeof(bool));
    bool* lakeMask = (bool*)calloc(cellCount, sizeof(bool));
    if (!heightmap || !waterMask || !riverMask || !lakeMask) {
        if (heightmap) free(heightmap);
        if (waterMask) free(waterMask);
        if (riverMask) free(riverMask);
        if (lakeMask) free(lakeMask);
        return;
    }

    // Generate heightmap and fill with dirt
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            float n = OctavePerlin(x * scale, y * scale, octaves, persistence);

            int height = minHeight + (int)(n * (maxHeight - minHeight));
            if (height < minHeight) height = minHeight;
            if (height >= gridDepth) height = gridDepth - 1;

            int idx = y * gridWidth + x;
            heightmap[idx] = height;

            for (int z = 0; z <= height; z++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_DIRT);
            }
        }
    }

    // Soil distribution parameters
    float clayScale = 0.03f;
    float sandScale = 0.02f;
    float gravelScale = 0.02f;
    float peatScale = 0.02f;
    float clayThreshold = 0.58f;
    float sandNoise = 0.55f;
    float gravelNoise = 0.62f;
    float peatNoise = 0.55f;
    int topsoilDepth = 2;
    int clayDepth = 2;
    int soilDepth = topsoilDepth + clayDepth;

    // Rock layer: keep a soil band on top, rock below
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int height = heightmap[y * gridWidth + x];
            if (height < soilDepth) continue;
            int rockStartZ = height - soilDepth;
            for (int z = 0; z <= rockStartZ; z++) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_GRANITE);
            }
        }
    }

    // --------------------------------------------------------------------
    // Rivers
    // --------------------------------------------------------------------
    float meanderScale = 0.015f;
    int riverCount = ClampInt(hillsWaterRiverCount, 0, 6);
    if (gridWidth * gridHeight <= 80 * 80 && riverCount > 1) riverCount = 1;
    int maxSteps = gridWidth + gridHeight;

    for (int r = 0; r < riverCount; r++) {
        int sourceX = -1, sourceY = -1;
        for (int attempts = 0; attempts < 200; attempts++) {
            int x = GetRandomValue(2, gridWidth - 3);
            int y = GetRandomValue(2, gridHeight - 3);
            int h = heightmap[y * gridWidth + x];
            float hNorm = (float)(h - minHeight) / (float)(maxHeight - minHeight);
            if (hNorm > 0.7f) {
                sourceX = x;
                sourceY = y;
                break;
            }
        }
        if (sourceX < 0) continue;

        int cx = sourceX;
        int cy = sourceY;
        int riverSeed = GetRandomValue(0, 10000);

        for (int step = 0; step < maxSteps; step++) {
            int idx = cy * gridWidth + cx;

            int baseRadius = ClampInt(hillsWaterRiverWidth, 1, 4);
            int radius = baseRadius + GetRandomValue(-1, 1);
            radius = ClampInt(radius, 1, 4);
            int depth = 1 + (radius >= 3 ? 1 : 0);

            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    if (dx*dx + dy*dy > radius*radius) continue;
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (nx < 1 || nx >= gridWidth - 1 || ny < 1 || ny >= gridHeight - 1) continue;

                    int nidx = ny * gridWidth + nx;
                    int currentHeight = heightmap[nidx];
                    int bedZ = currentHeight - 1 - depth;
                    if (bedZ < 0) bedZ = 0;

                    for (int z = bedZ + 1; z <= currentHeight; z++) {
                        grid[z][ny][nx] = CELL_AIR;
                        SET_CELL_SURFACE(nx, ny, z, SURFACE_BARE);
                    }

                    heightmap[nidx] = bedZ;
                    waterMask[nidx] = true;
                    riverMask[nidx] = true;

                    int waterZ = bedZ + 1;
                    if (waterZ < gridDepth) {
                        SetWaterLevel(nx, ny, waterZ, WATER_MAX_LEVEL);
                    }
                }
            }

            int sourceWaterZ = heightmap[idx] + 1;
            if (step == 0 && sourceWaterZ < gridDepth) {
                SetWaterSource(cx, cy, sourceWaterZ, true);
            }

            if (cx == 0 || cy == 0 || cx == gridWidth - 1 || cy == gridHeight - 1) {
                break;
            }

            int bestX = cx;
            int bestY = cy;
            float bestScore = 1e9f;
            for (int d = 0; d < 4; d++) {
                int nx = cx + (d == 0 ? -1 : d == 1 ? 1 : 0);
                int ny = cy + (d == 2 ? -1 : d == 3 ? 1 : 0);
                if (nx < 1 || nx >= gridWidth - 1 || ny < 1 || ny >= gridHeight - 1) continue;
                int nidx = ny * gridWidth + nx;
                float meander = OctavePerlin((nx + riverSeed) * meanderScale, (ny + riverSeed) * meanderScale, 2, 0.5f);
                float score = (float)heightmap[nidx] + meander * 2.0f;
                if (score < bestScore) {
                    bestScore = score;
                    bestX = nx;
                    bestY = ny;
                }
            }

            if (bestX == cx && bestY == cy) break;
            cx = bestX;
            cy = bestY;
        }
    }

    // --------------------------------------------------------------------
    // Shallow Lakes
    // --------------------------------------------------------------------
    int lakeCount = ClampInt(hillsWaterLakeCount, 0, 6);
    if (gridWidth * gridHeight <= 80 * 80 && lakeCount > 1) lakeCount = 1;
    for (int l = 0; l < lakeCount; l++) {
        int centerX = -1, centerY = -1;
        for (int attempts = 0; attempts < 200; attempts++) {
            int x = GetRandomValue(4, gridWidth - 5);
            int y = GetRandomValue(4, gridHeight - 5);
            int h = heightmap[y * gridWidth + x];
            float hNorm = (float)(h - minHeight) / (float)(maxHeight - minHeight);
            if (hNorm < 0.35f) {
                centerX = x;
                centerY = y;
                break;
            }
        }
        if (centerX < 0) continue;

        int baseRadius = ClampInt(hillsWaterLakeRadius, 3, 12);
        int radius = baseRadius + GetRandomValue(-2, 2);
        radius = ClampInt(radius, 3, 12);
        int centerIdx = centerY * gridWidth + centerX;
        int lakeSurfaceZ = ClampInt(heightmap[centerIdx] - 1, 1, gridDepth - 2);
        int lakeBedZ = lakeSurfaceZ - 1;

        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx*dx + dy*dy > radius*radius) continue;
                int x = centerX + dx;
                int y = centerY + dy;
                if (x < 1 || x >= gridWidth - 1 || y < 1 || y >= gridHeight - 1) continue;

                int idx = y * gridWidth + x;
                int currentHeight = heightmap[idx];
                if (currentHeight < lakeBedZ) continue;

                for (int z = lakeBedZ + 1; z <= currentHeight; z++) {
                    grid[z][y][x] = CELL_AIR;
                    SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
                }

                heightmap[idx] = lakeBedZ;
                waterMask[idx] = true;
                lakeMask[idx] = true;

                if (lakeSurfaceZ < gridDepth) {
                    SetWaterLevel(x, y, lakeSurfaceZ, WATER_MAX_LEVEL);
                }
            }
        }

        if (lakeSurfaceZ < gridDepth) {
            SetWaterSource(centerX, centerY, lakeSurfaceZ, true);
        }
    }

    // --------------------------------------------------------------------
    // Apply surface soils + subsoil clay
    // --------------------------------------------------------------------
    int nearWaterRadius = 3;
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int idx = y * gridWidth + x;
            int height = heightmap[idx];

            int slope = 0;
            if (x > 0) slope = (abs(height - heightmap[y * gridWidth + (x - 1)]) > slope) ? abs(height - heightmap[y * gridWidth + (x - 1)]) : slope;
            if (x < gridWidth - 1) slope = (abs(height - heightmap[y * gridWidth + (x + 1)]) > slope) ? abs(height - heightmap[y * gridWidth + (x + 1)]) : slope;
            if (y > 0) slope = (abs(height - heightmap[(y - 1) * gridWidth + x]) > slope) ? abs(height - heightmap[(y - 1) * gridWidth + x]) : slope;
            if (y < gridHeight - 1) slope = (abs(height - heightmap[(y + 1) * gridWidth + x]) > slope) ? abs(height - heightmap[(y + 1) * gridWidth + x]) : slope;

            bool nearWater = false;
            for (int dy = -nearWaterRadius; dy <= nearWaterRadius && !nearWater; dy++) {
                for (int dx = -nearWaterRadius; dx <= nearWaterRadius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                    if (waterMask[ny * gridWidth + nx]) {
                        nearWater = true;
                        break;
                    }
                }
            }

            float heightNorm = (maxHeight == minHeight) ? 0.0f : (float)(height - minHeight) / (float)(maxHeight - minHeight);
            float wetness = 1.0f - heightNorm;
            if (nearWater) wetness += hillsWaterWetnessBias;
            wetness = Clamp01(wetness);

            float sandN = OctavePerlin(x * sandScale, y * sandScale, 3, 0.5f);
            float gravelN = OctavePerlin(x * gravelScale, y * gravelScale, 3, 0.5f);
            float peatN = OctavePerlin(x * peatScale, y * peatScale, 3, 0.5f);

            MaterialType surfaceMat = MAT_DIRT;
            if (waterMask[idx]) {
                surfaceMat = (riverMask[idx] || slope >= 2 || gravelN > 0.55f) ? MAT_GRAVEL : MAT_SAND;
            } else if (wetness > 0.7f && peatN > peatNoise) {
                surfaceMat = MAT_PEAT;
            } else if (wetness < 0.35f && sandN > sandNoise) {
                surfaceMat = MAT_SAND;
            } else {
                bool rockBelow = (height >= soilDepth);
                float gravelThreshold = gravelNoise - (rockBelow ? 0.08f : 0.0f);
                if (gravelThreshold < 0.4f) gravelThreshold = 0.4f;
                if (slope >= 2 || gravelN > gravelThreshold) {
                    surfaceMat = MAT_GRAVEL;
                }
            }

            grid[height][y][x] = CELL_WALL;
            SetWallMaterial(x, y, height, surfaceMat);
            if (surfaceMat == MAT_DIRT && !nearWater) {
                SET_CELL_SURFACE(x, y, height, SURFACE_TALL_GRASS);
            } else if (surfaceMat == MAT_DIRT) {
                SET_CELL_SURFACE(x, y, height, SURFACE_GRASS);
            } else {
                SET_CELL_SURFACE(x, y, height, SURFACE_BARE);
            }

            float clayN = OctavePerlin(x * clayScale, y * clayScale, 3, 0.5f);
            if (clayN > clayThreshold || (nearWater && wetness > 0.6f)) {
                int bandEnd = height - topsoilDepth;
                if (bandEnd > 0) {
                    int bandStart = bandEnd - clayDepth + 1;
                    if (bandStart < 0) bandStart = 0;
                    for (int z = bandStart; z <= bandEnd && z < gridDepth; z++) {
                        if (GetWallMaterial(x, y, z) == MAT_DIRT) {
                            SetWallMaterial(x, y, z, MAT_CLAY);
                        }
                    }
                }
            }
        }
    }

    // --------------------------------------------------------------------
    // Ramp placement pass (same as GenerateHills)
    // --------------------------------------------------------------------
    int rampCandidates = 0;
    int rampPlaced = 0;
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int myHeight = heightmap[y * gridWidth + x];

            if (y > 0) {
                int northHeight = heightmap[(y - 1) * gridWidth + x];
                if (northHeight == myHeight + 1) {
                    int rampZ = northHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y - 1][x]) &&
                        ShouldPlaceRampAt(x, y - 1)) {
                        MaterialType rampMat = GetWallMaterial(x, y - 1, rampZ);
                        grid[rampZ][y - 1][x] = CELL_RAMP_N;
                        SetWallMaterial(x, y - 1, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            if (x < gridWidth - 1) {
                int eastHeight = heightmap[y * gridWidth + (x + 1)];
                if (eastHeight == myHeight + 1) {
                    int rampZ = eastHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y][x + 1]) &&
                        ShouldPlaceRampAt(x + 1, y)) {
                        MaterialType rampMat = GetWallMaterial(x + 1, y, rampZ);
                        grid[rampZ][y][x + 1] = CELL_RAMP_E;
                        SetWallMaterial(x + 1, y, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            if (y < gridHeight - 1) {
                int southHeight = heightmap[(y + 1) * gridWidth + x];
                if (southHeight == myHeight + 1) {
                    int rampZ = southHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y + 1][x]) &&
                        ShouldPlaceRampAt(x, y + 1)) {
                        MaterialType rampMat = GetWallMaterial(x, y + 1, rampZ);
                        grid[rampZ][y + 1][x] = CELL_RAMP_S;
                        SetWallMaterial(x, y + 1, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
            if (x > 0) {
                int westHeight = heightmap[y * gridWidth + (x - 1)];
                if (westHeight == myHeight + 1) {
                    int rampZ = westHeight;
                    rampCandidates++;
                    if (rampZ < gridDepth && !CellIsRamp(grid[rampZ][y][x - 1]) &&
                        ShouldPlaceRampAt(x - 1, y)) {
                        MaterialType rampMat = GetWallMaterial(x - 1, y, rampZ);
                        grid[rampZ][y][x - 1] = CELL_RAMP_W;
                        SetWallMaterial(x - 1, y, rampZ, rampMat);
                        rampPlaced++;
                    }
                }
            }
        }
    }

    TraceLog(LOG_INFO, "GenerateHillsSoilsWater: ramps %d/%d (density=%.2f scale=%.3f)",
             rampPlaced, rampCandidates, rampDensity, rampNoiseScale);

    // --------------------------------------------------------------------
    // Small buildings (towers + gallery flats + council blocks)
    // --------------------------------------------------------------------
    int* surface = (int*)malloc(cellCount * sizeof(int));
    if (surface) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                surface[y * gridWidth + x] = FindSurfaceZAt(x, y);
            }
        }

        int buildingAttempts = 60;
        int placed = 0;
        for (int attempt = 0; attempt < buildingAttempts && placed < 6; attempt++) {
            int kind = GetRandomValue(0, 2);  // 0: tower, 1: gallery, 2: council
            int w = 0, h = 0;
            int galleryApts = 0;
            int galleryFloors = 0;
            if (kind == 0) {
                w = 3 + GetRandomValue(0, 2);
                h = 3 + GetRandomValue(0, 2);
            }
            if (kind == 1) {
                int apartmentWidth = 4;
                int apartmentDepth = 4;
                int corridorWidth = 2;
                int stairWidth = 2;
                galleryApts = 2 + GetRandomValue(0, 2);
                w = stairWidth + galleryApts * apartmentWidth + stairWidth;
                h = apartmentDepth + corridorWidth;
                galleryFloors = 2 + GetRandomValue(0, 1);
            }
            if (kind == 2) {
                w = 16 + GetRandomValue(0, 6);
                h = 8 + GetRandomValue(0, 4);
            }

            int x = GetRandomValue(2, gridWidth - w - 2);
            int y = GetRandomValue(2, gridHeight - h - 2);

            int baseZ = 0;
            if (!AreaIsFlatAndDry(x, y, w, h, 1, surface, waterMask, &baseZ)) continue;
            if (baseZ + 2 >= gridDepth) continue;
            FlattenAreaTo(x, y, w, h, baseZ, surface);

            if (kind == 0) {
                PlaceMiniTowerAt(x, y, baseZ, w, h, 2 + GetRandomValue(0, 1), surface);
            } else if (kind == 1) {
                PlaceMiniGalleryFlatAt(x, y, baseZ, galleryApts, galleryFloors, surface);
            } else {
                int floors = 3 + GetRandomValue(0, 1);
                bool vertical = (GetRandomValue(0, 1) == 1);
                BuildTowerBlockAt(x, y, w, h, floors, vertical, baseZ + 1, surface);
            }
            placed++;
        }

        // ----------------------------------------------------------------
        // Tree placement (typed, bias by wetness/slope/near-water)
        // ----------------------------------------------------------------
        int treePlaced = 0;
        float treeScale = 0.028f;
        int nearWaterRadius = 3;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                int idx = y * gridWidth + x;
                if (waterMask[idx]) continue;

                int baseZ = surface[idx];
                if (!CanPlaceWorldGenTreeAt(x, y, baseZ)) continue;

                int height = heightmap[idx];
                int slope = 0;
                if (x > 0) slope = (abs(height - heightmap[y * gridWidth + (x - 1)]) > slope) ? abs(height - heightmap[y * gridWidth + (x - 1)]) : slope;
                if (x < gridWidth - 1) slope = (abs(height - heightmap[y * gridWidth + (x + 1)]) > slope) ? abs(height - heightmap[y * gridWidth + (x + 1)]) : slope;
                if (y > 0) slope = (abs(height - heightmap[(y - 1) * gridWidth + x]) > slope) ? abs(height - heightmap[(y - 1) * gridWidth + x]) : slope;
                if (y < gridHeight - 1) slope = (abs(height - heightmap[(y + 1) * gridWidth + x]) > slope) ? abs(height - heightmap[(y + 1) * gridWidth + x]) : slope;

                bool nearWater = false;
                for (int dy = -nearWaterRadius; dy <= nearWaterRadius && !nearWater; dy++) {
                    for (int dx = -nearWaterRadius; dx <= nearWaterRadius; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                        if (waterMask[ny * gridWidth + nx]) {
                            nearWater = true;
                            break;
                        }
                    }
                }

                float heightNorm = (maxHeight == minHeight) ? 0.0f : (float)(height - minHeight) / (float)(maxHeight - minHeight);
                float wetness = 1.0f - heightNorm;
                if (nearWater) wetness += hillsWaterWetnessBias;
                wetness = Clamp01(wetness);

                float n = OctavePerlin(x * treeScale, y * treeScale, 3, 0.5f);
                float density = 0.10f;
                if (wetness > 0.75f) density *= 0.75f;
                if (wetness < 0.30f) density *= 0.65f;
                if (slope >= 2) density *= 0.6f;
                if (n > density) continue;

                MaterialType treeMat = PickTreeTypeForWorldGen(GetWallMaterial(x, y, baseZ), wetness, slope, nearWater, n);
                PlaceWorldGenTree(x, y, baseZ, treeMat, true);
                treePlaced++;
            }
        }
        TraceLog(LOG_INFO, "GenerateHillsSoilsWater: placed %d trees", treePlaced);

        free(surface);
    }

    ReportWalkableComponents("HillsSoilsWater");

    free(heightmap);
    free(waterMask);
    free(riverMask);
    free(lakeMask);

    needsRebuild = true;
}

void GeneratePerlin(void) {
    InitGrid();
    FillGroundLevel();
    InitPerlin((int)worldSeed);
    float scale = 0.015f;

    // First pass: terrain noise for trees
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            float n = OctavePerlin(x * scale, y * scale, 4, 0.5f);
            // n < 0.45 = forest, n > 0.55 = city, between = transition
            float density;
            if (n < 0.45f) {
                density = 0.08f + (0.45f - n) * 0.3f;  // 8-20% trees in forest
            } else {
                density = 0.02f;  // light debris in city
            }
            if ((float)GetRandomValue(0, 100) / 100.0f < density) {
                MaterialType treeMat = PickTreeTypeForSoilSimple(GetWallMaterial(x, y, 0));
                PlaceWorldGenTree(x, y, 0, treeMat, true);
            }
        }
    }

    // Second pass: city walls where noise > 0.5
    for (int wy = chunkHeight/2; wy < gridHeight; wy += chunkHeight / 2) {
        for (int wx = 0; wx < gridWidth;) {
            float n = OctavePerlin(wx * scale, wy * scale, 4, 0.5f);
            if (n < 0.5f) { wx += 6; continue; }

            // Higher noise = longer walls, smaller gaps
            float intensity = (n - 0.5f) * 2.0f;  // 0-1
            int wallLen = (int)(4 + intensity * 12);   // 4-16
            int gapSize = (int)(5 - intensity * 2);    // 5-3
            if (gapSize < 2) gapSize = 2;

            for (int x = wx; x < wx + wallLen && x < gridWidth; x++) {
                float n2 = OctavePerlin(x * scale, wy * scale, 4, 0.5f);
                if (n2 > 0.48f) {
                    grid[1][wy][x] = CELL_WALL;
                    if (wy + 1 < gridHeight) grid[1][wy + 1][x] = CELL_WALL;
                }
            }
            wx += wallLen + gapSize;
        }
    }

    // Vertical walls
    for (int wx = chunkWidth/2; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight;) {
            float n = OctavePerlin(wx * scale, wy * scale, 4, 0.5f);
            if (n < 0.5f) { wy += 6; continue; }

            float intensity = (n - 0.5f) * 2.0f;
            int wallLen = (int)(4 + intensity * 12);
            int gapSize = (int)(5 - intensity * 2);
            if (gapSize < 2) gapSize = 2;

            for (int y = wy; y < wy + wallLen && y < gridHeight; y++) {
                float n2 = OctavePerlin(wx * scale, y * scale, 4, 0.5f);
                if (n2 > 0.48f) {
                    grid[1][y][wx] = CELL_WALL;
                    if (wx + 1 < gridWidth) grid[1][y][wx + 1] = CELL_WALL;
                }
            }
            wy += wallLen + gapSize;
        }
    }

    needsRebuild = true;
}

void GenerateCity(void) {
    InitGrid();
    SeedTerrain();
    FillGroundLevel();
    for (int wy = chunkHeight; wy < gridHeight; wy += chunkHeight / 2) {
        for (int wx = 0; wx < gridWidth; wx++) {
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int x = wx; x < wx + gapPos && x < gridWidth; x++) {
                grid[1][wy][x] = CELL_WALL;
                if (wy + 1 < gridHeight) grid[1][wy + 1][x] = CELL_WALL;
            }
            wx += gapPos + gapSize;
        }
    }
    for (int wx = chunkWidth; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight; wy++) {
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int y = wy; y < wy + gapPos && y < gridHeight; y++) {
                grid[1][y][wx] = CELL_WALL;
                if (wx + 1 < gridWidth) grid[1][y][wx + 1] = CELL_WALL;
            }
            wy += gapPos + gapSize;
        }
    }
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][y][x] == CELL_WALL && GetWallMaterial(x, y, 0) == MAT_DIRT && GetRandomValue(0, 100) < 5) {
                MaterialType treeMat = PickTreeTypeForSoilSimple(GetWallMaterial(x, y, 0));
                PlaceWorldGenTree(x, y, 0, treeMat, true);
            }
        }
    }
    needsRebuild = true;
}

// ============================================================================
// 3D Towers with Bridges Generator
// Creates towers (vertical structures) with bridges connecting them at higher levels
// ============================================================================

#define MAX_TOWERS 50

typedef struct {
    int x, y;       // Tower base position
    int w, h;       // Tower footprint
    int height;     // Tower height (z-levels)
} Tower;

// Union-Find for connectivity checking
static int towerParent[MAX_TOWERS];

static int TowerFind(int i) {
    if (towerParent[i] != i)
        towerParent[i] = TowerFind(towerParent[i]);
    return towerParent[i];
}

static void TowerUnion(int i, int j) {
    int pi = TowerFind(i);
    int pj = TowerFind(j);
    if (pi != pj) towerParent[pi] = pj;
}

// Build a bridge between two towers
static void BuildBridge(Tower* t1, Tower* t2) {
    int c1x = t1->x + t1->w / 2;
    int c1y = t1->y + t1->h / 2;
    int c2x = t2->x + t2->w / 2;
    int c2y = t2->y + t2->h / 2;
    int dx = c2x - c1x;
    int dy = c2y - c1y;
    
    // Build bridge at z=1 (only use z=2 if both towers are already 3 levels)
    int bridgeZ = (t1->height >= 3 && t2->height >= 3 && GetRandomValue(0, 1)) ? 2 : 1;
    
    // Extend towers to reach bridge level if needed
    // This adds walls and air up to bridgeZ+2 (walking level above bridge)
    Tower* towersToExtend[2] = {t1, t2};
    for (int ti = 0; ti < 2; ti++) {
        Tower* t = towersToExtend[ti];
        int newHeight = bridgeZ + 2;
        if (t->height < newHeight) {
            // Extend tower structure up to new height
            for (int z = t->height + 1; z <= newHeight; z++) {
                for (int py = t->y; py < t->y + t->h; py++) {
                    for (int px = t->x; px < t->x + t->w; px++) {
                        bool isBorder = (px == t->x || px == t->x + t->w - 1 || 
                                        py == t->y || py == t->y + t->h - 1);
                        grid[z][py][px] = isBorder ? CELL_WALL : CELL_AIR;
                    }
                }
            }
            // Update tower height
            t->height = newHeight;
        }
        // Always ensure ladders go through all levels
        // Ladders must exist on BOTH z-levels to allow climbing between them
        int ladderX = t->x + t->w / 2;
        int ladderY = t->y + t->h / 2;
        for (int z = 1; z <= t->height; z++) {
            grid[z][ladderY][ladderX] = CELL_LADDER_BOTH;
        }
    }
    
    // Find bridge start and end points (on tower edges)
    int startX, startY, endX, endY;
    if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)) {
        // Horizontal bridge
        if (dx > 0) {
            startX = t1->x + t1->w - 1;
            endX = t2->x;
        } else {
            startX = t1->x;
            endX = t2->x + t2->w - 1;
        }
        startY = t1->y + t1->h / 2;
        endY = t2->y + t2->h / 2;
    } else {
        // Vertical bridge
        if (dy > 0) {
            startY = t1->y + t1->h - 1;
            endY = t2->y;
        } else {
            startY = t1->y;
            endY = t2->y + t2->h - 1;
        }
        startX = t1->x + t1->w / 2;
        endX = t2->x + t2->w / 2;
    }
    
    // Carve bridge (simple L-shape)
    int x = startX, y = startY;
    
    // Open the tower walls at bridge connection points (at walking level bridgeZ+1)
    grid[bridgeZ + 1][startY][startX] = CELL_AIR;
    grid[bridgeZ + 1][endY][endX] = CELL_AIR;
    
    // Horizontal segment - solid floor at bridgeZ, air at bridgeZ+1
    while (x != endX) {
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            grid[bridgeZ][y][x] = CELL_WALL;      // Floor (solid to walk on)
            grid[bridgeZ + 1][y][x] = CELL_AIR;   // Walking level
        }
        x += (endX > x) ? 1 : -1;
    }
    // Vertical segment  
    while (y != endY) {
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            grid[bridgeZ][y][x] = CELL_WALL;      // Floor (solid to walk on)
            grid[bridgeZ + 1][y][x] = CELL_AIR;   // Walking level
        }
        y += (endY > y) ? 1 : -1;
    }
}

void GenerateTowers(void) {
    InitGrid();  // Clear cells and flags
    SeedTerrain();
    FillGroundLevel();
    
    // Place towers
    Tower towers[MAX_TOWERS];
    int towerCount = 0;
    
    int attempts = 200;
    int targetTowers = (gridWidth * gridHeight) / 200;  // Roughly 1 tower per 200 tiles
    if (targetTowers < 5) targetTowers = 5;
    if (targetTowers > MAX_TOWERS) targetTowers = MAX_TOWERS;
    
    for (int i = 0; i < attempts && towerCount < targetTowers; i++) {
        int tw = 3 + GetRandomValue(0, 3);  // Tower size 3-6
        int th = 3 + GetRandomValue(0, 3);
        int tx = 2 + GetRandomValue(0, gridWidth - tw - 4);
        int ty = 2 + GetRandomValue(0, gridHeight - th - 4);
        int tHeight = 2 + GetRandomValue(0, 1);  // Height 2-3 z-levels
        
        // Check for overlap with existing towers (with margin)
        bool overlaps = false;
        for (int t = 0; t < towerCount; t++) {
            Tower* other = &towers[t];
            int margin = 4;  // Space between towers for bridges
            if (tx < other->x + other->w + margin && tx + tw + margin > other->x &&
                ty < other->y + other->h + margin && ty + th + margin > other->y) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            towers[towerCount++] = (Tower){tx, ty, tw, th, tHeight};
            
            // Calculate ladder position
            int ladderX = tx + tw / 2;
            int ladderY = ty + th / 2;
            
            // Build the tower: walls on border, floor inside (shifted +1 for DF)
            // Skip ladder position when placing floors
            for (int z = 0; z < tHeight; z++) {
                for (int py = ty; py < ty + th; py++) {
                    for (int px = tx; px < tx + tw; px++) {
                        bool isBorder = (px == tx || px == tx + tw - 1 || 
                                        py == ty || py == ty + th - 1);
                        bool isLadderPos = (px == ladderX && py == ladderY);
                        if (isBorder) {
                            grid[z + 1][py][px] = CELL_WALL;
                        } else if (!isLadderPos) {
                            PlaceFloor(px, py, z + 1, MAT_GRANITE);
                        }
                    }
                }
            }
            
            // Add ladder inside tower (connects all levels)
            // Place ladder from bottom to top
            for (int z = 0; z < tHeight - 1; z++) {
                PlaceLadder(ladderX, ladderY, z + 1);
            }
            
            // Add door at z=1 (opening in wall)
            int doorSide = GetRandomValue(0, 3);
            switch (doorSide) {
                case 0: PlaceFloor(tx + tw / 2, ty, 1, MAT_GRANITE); break;          // North
                case 1: PlaceFloor(tx + tw - 1, ty + th / 2, 1, MAT_GRANITE); break; // East
                case 2: PlaceFloor(tx + tw / 2, ty + th - 1, 1, MAT_GRANITE); break; // South
                case 3: PlaceFloor(tx, ty + th / 2, 1, MAT_GRANITE); break;          // West
            }
        }
    }
    
    // Initialize union-find: all towers connected via ground (z=0)
    for (int i = 0; i < towerCount; i++) {
        towerParent[i] = 0;  // All towers are connected at ground level
    }
    
    // Connect some towers with bridges at z=1 or z=2
    for (int i = 0; i < towerCount; i++) {
        Tower* t1 = &towers[i];
        if (t1->height < 2) continue;  // Need at least 2 levels for bridge
        
        // Try to connect to 1-2 nearby towers
        int connections = 0;
        for (int j = 0; j < towerCount && connections < 2; j++) {
            if (i == j) continue;
            Tower* t2 = &towers[j];
            if (t2->height < 2) continue;
            
            // Calculate distance
            int c1x = t1->x + t1->w / 2;
            int c1y = t1->y + t1->h / 2;
            int c2x = t2->x + t2->w / 2;
            int c2y = t2->y + t2->h / 2;
            int dx = c2x - c1x;
            int dy = c2y - c1y;
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            
            // Only connect nearby towers (manhattan distance 8-20)
            if (dist < 8 || dist > 20) continue;
            
            // Random chance to skip
            if (GetRandomValue(0, 100) < 50) continue;
            
            BuildBridge(t1, t2);
            TowerUnion(i, j);
            connections++;
        }
    }
    
    // Ensure all towers with height >= 2 are connected via bridges
    // Find towers that aren't in the main connected component
    for (int i = 1; i < towerCount; i++) {
        if (towers[i].height < 2) continue;
        
        // Find first tower with height >= 2 in a different component
        if (TowerFind(i) != TowerFind(0)) {
            // Find nearest tower in the main component to connect to
            int nearest = -1;
            int nearestDist = 999999;
            for (int j = 0; j < towerCount; j++) {
                if (i == j || towers[j].height < 2) continue;
                if (TowerFind(j) != TowerFind(0)) continue;  // Must be in main component
                
                int dx = (towers[i].x + towers[i].w/2) - (towers[j].x + towers[j].w/2);
                int dy = (towers[i].y + towers[i].h/2) - (towers[j].y + towers[j].h/2);
                int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearest = j;
                }
            }
            
            if (nearest >= 0) {
                BuildBridge(&towers[i], &towers[nearest]);
                TowerUnion(i, nearest);
            }
        }
    }
    
    // Final pass: ensure ALL towers have proper ladders through all levels
    // Ladders must exist on BOTH z-levels to allow climbing between them
    for (int i = 0; i < towerCount; i++) {
        Tower* t = &towers[i];
        if (t->height < 2) continue;
        
        int ladderX = t->x + t->w / 2;
        int ladderY = t->y + t->h / 2;
        // Ladders on ALL levels of the tower (starting at z=1)
        for (int z = 1; z <= t->height; z++) {
            grid[z][ladderY][ladderX] = CELL_LADDER_BOTH;
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Gallery Flat (Galerij Flat) Generator
// Long rectangular apartment building with external corridor (gallery) on one side
// Staircases at both ends connecting all floors
// ============================================================================

void GenerateGalleryFlat(void) {
    InitGrid();  // Clear cells and flags
    FillGroundLevel();
    
    // Building parameters
    int apartmentWidth = 4;
    int apartmentDepth = 4;
    int corridorWidth = 2;
    int stairWidth = 2;
    int numFloors = gridDepth;  // Use all available z-levels
    
    // Calculate building dimensions
    int numApartments = (gridWidth - 4 - 2 * stairWidth) / apartmentWidth;
    if (numApartments < 2) numApartments = 2;
    
    int buildingWidth = stairWidth + numApartments * apartmentWidth + stairWidth;
    int buildingDepth = apartmentDepth + corridorWidth;
    
    // Center the building
    int buildingX = (gridWidth - buildingWidth) / 2;
    int buildingY = (gridHeight - buildingDepth) / 2;
    
    // Build each floor (shifted +1 for DF-style: z=0 is ground, building starts at z=1)
    for (int floor = 0; floor < numFloors - 1; floor++) {
        int z = floor + 1;  // Actual z-level (floor 0 -> z=1, etc.)
        if (z >= gridDepth) break;
        
        // Outer walls of the building
        for (int x = buildingX; x < buildingX + buildingWidth; x++) {
            grid[z][buildingY][x] = CELL_WALL;                           // North wall
            grid[z][buildingY + buildingDepth - 1][x] = CELL_WALL;       // South wall
        }
        for (int y = buildingY; y < buildingY + buildingDepth; y++) {
            grid[z][y][buildingX] = CELL_WALL;                           // West wall
            grid[z][y][buildingX + buildingWidth - 1] = CELL_WALL;       // East wall
        }
        
        // Fill building interior with floor
        for (int y = buildingY + 1; y < buildingY + buildingDepth - 1; y++) {
            for (int x = buildingX + 1; x < buildingX + buildingWidth - 1; x++) {
                PlaceFloor(x, y, z, MAT_GRANITE);
            }
        }
        
        // Gallery corridor (south side, inside the building)
        // Already filled with floor above
        
        // Apartment walls (north side)
        int apartmentStartX = buildingX + stairWidth;
        for (int apt = 0; apt < numApartments; apt++) {
            int aptX = apartmentStartX + apt * apartmentWidth;
            
            // Wall between apartments (except first one which uses building wall)
            if (apt > 0) {
                for (int y = buildingY; y < buildingY + apartmentDepth; y++) {
                    grid[z][y][aptX] = CELL_WALL;
                }
            }
            
            // Back wall of apartment (separating from corridor)
            for (int x = aptX; x < aptX + apartmentWidth && x < buildingX + buildingWidth - stairWidth; x++) {
                grid[z][buildingY + apartmentDepth - 1][x] = CELL_WALL;
            }
            
            // Door to corridor (middle of back wall)
            int doorX = aptX + apartmentWidth / 2;
            if (doorX < buildingX + buildingWidth - stairWidth) {
                PlaceFloor(doorX, buildingY + apartmentDepth - 1, z, MAT_GRANITE);
            }
        }
        
        // Last apartment wall
        int lastWallX = apartmentStartX + numApartments * apartmentWidth;
        if (lastWallX < buildingX + buildingWidth - 1) {
            for (int y = buildingY; y < buildingY + apartmentDepth; y++) {
                grid[z][y][lastWallX] = CELL_WALL;
            }
        }
        
        // West staircase area
        int westStairX = buildingX + 1;
        int stairY = buildingY + 1;
        // Ladder in west staircase (on all floors)
        grid[z][stairY][westStairX] = CELL_LADDER_BOTH;
        
        // East staircase area  
        int eastStairX = buildingX + buildingWidth - 2;
        // Ladder in east staircase (on all floors)
        grid[z][stairY][eastStairX] = CELL_LADDER_BOTH;
    }
    
    // Ground floor entrance at z=1 (door in south wall)
    int entranceX = buildingX + buildingWidth / 2;
    PlaceFloor(entranceX, buildingY + buildingDepth - 1, 1, MAT_GRANITE);
    // Second entrance on the other side
    PlaceFloor(entranceX + 2, buildingY + buildingDepth - 1, 1, MAT_GRANITE);
    
    needsRebuild = true;
}

// ============================================================================
// Castle Generator
// Medieval British walled castle with:
// - Rectangular outer wall with corner towers
// - Wall walk (z=2) with crenellations (merlons and crenels)
// - 2 stair towers for access to wall walk
// - Main gate on one side
// - Courtyard with small buildings (some 2-story)
// ============================================================================

void GenerateCastle(void) {
    InitGrid();  // Clear cells and flags
    FillGroundLevel();
    
    // Castle dimensions - centered in grid
    int wallThickness = 2;
    int towerSize = 5;  // Corner towers are 5x5
    int stairTowerSize = 4;  // Stair towers are 4x4
    
    // Calculate castle size based on grid (leave margin around edges)
    int margin = 4;
    int castleWidth = gridWidth - 2 * margin;
    int castleHeight = gridHeight - 2 * margin;
    
    // Minimum size check
    if (castleWidth < 30) castleWidth = 30;
    if (castleHeight < 30) castleHeight = 30;
    
    int castleX = (gridWidth - castleWidth) / 2;
    int castleY = (gridHeight - castleHeight) / 2;
    
    // ========================================
    // Build outer walls (z=1, z=2, z=3 for DF-style)
    // ========================================
    for (int floor = 0; floor < 3; floor++) {
        int z = floor + 1;
        if (z >= gridDepth) break;
        
        // North wall
        for (int x = castleX; x < castleX + castleWidth; x++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][castleY + t][x] = CELL_WALL;
            }
        }
        // South wall
        for (int x = castleX; x < castleX + castleWidth; x++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][castleY + castleHeight - 1 - t][x] = CELL_WALL;
            }
        }
        // West wall
        for (int y = castleY; y < castleY + castleHeight; y++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][y][castleX + t] = CELL_WALL;
            }
        }
        // East wall
        for (int y = castleY; y < castleY + castleHeight; y++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][y][castleX + castleWidth - 1 - t] = CELL_WALL;
            }
        }
    }
    
    // ========================================
    // Wall walk at z=2 (solid floor to stand on in DF-style)
    // In DF mode, movers at z=2 need solid below, so we place floors at z=2
    // which makes z=2 walkable (standing on wall tops)
    // ========================================
    if (gridDepth > 2) {
        // North wall walk floor
        for (int x = castleX + wallThickness; x < castleX + castleWidth - wallThickness; x++) {
            PlaceFloor(x, castleY + wallThickness, 2, MAT_GRANITE);
        }
        // South wall walk floor
        for (int x = castleX + wallThickness; x < castleX + castleWidth - wallThickness; x++) {
            PlaceFloor(x, castleY + castleHeight - 1 - wallThickness, 2, MAT_GRANITE);
        }
        // West wall walk floor
        for (int y = castleY + wallThickness; y < castleY + castleHeight - wallThickness; y++) {
            PlaceFloor(castleX + wallThickness, y, 2, MAT_GRANITE);
        }
        // East wall walk floor
        for (int y = castleY + wallThickness; y < castleY + castleHeight - wallThickness; y++) {
            PlaceFloor(castleX + castleWidth - 1 - wallThickness, y, 2, MAT_GRANITE);
        }
        
        // Crenellations (merlons on outer edge at z=2)
        for (int x = castleX; x < castleX + castleWidth; x++) {
            // North crenellations
            if ((x - castleX) % 2 == 0) {
                grid[2][castleY][x] = CELL_WALL;  // Merlon
            }
            // South crenellations
            if ((x - castleX) % 2 == 0) {
                grid[2][castleY + castleHeight - 1][x] = CELL_WALL;  // Merlon
            }
        }
        for (int y = castleY; y < castleY + castleHeight; y++) {
            // West crenellations
            if ((y - castleY) % 2 == 0) {
                grid[2][y][castleX] = CELL_WALL;  // Merlon
            }
            // East crenellations
            if ((y - castleY) % 2 == 0) {
                grid[2][y][castleX + castleWidth - 1] = CELL_WALL;  // Merlon
            }
        }
    }
    
    // ========================================
    // Corner towers (all 4 corners, 3 levels at z=1,2,3)
    // ========================================
    int cornerPositions[4][2] = {
        {castleX, castleY},                                              // NW
        {castleX + castleWidth - towerSize, castleY},                    // NE
        {castleX, castleY + castleHeight - towerSize},                   // SW
        {castleX + castleWidth - towerSize, castleY + castleHeight - towerSize}  // SE
    };
    
    for (int t = 0; t < 4; t++) {
        int tx = cornerPositions[t][0];
        int ty = cornerPositions[t][1];
        
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            
            for (int py = ty; py < ty + towerSize; py++) {
                for (int px = tx; px < tx + towerSize; px++) {
                    bool isBorder = (px == tx || px == tx + towerSize - 1 ||
                                    py == ty || py == ty + towerSize - 1);
                    if (isBorder) {
                        grid[z][py][px] = CELL_WALL;
                    } else {
                        PlaceFloor(px, py, z, MAT_GRANITE);
                    }
                }
            }
        }
        
        // Ladder in center of tower (connects z=1,2,3)
        int ladderX = tx + towerSize / 2;
        int ladderY = ty + towerSize / 2;
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            grid[z][ladderY][ladderX] = CELL_LADDER_BOTH;
        }
        
        // Door from tower to courtyard at z=1 (toward inside of castle)
        int doorY = (t < 2) ? (ty + towerSize - 1) : ty;  // NW/NE: south, SW/SE: north
        PlaceFloor(tx + towerSize / 2, doorY, 1, MAT_GRANITE);
        
        // Connect tower to wall walk at z=2
        if (gridDepth > 2) {
            // Vertical connection (N/S): NW/NE open south, SW/SE open north
            int connY = (t < 2) ? (ty + towerSize - 1) : ty;
            PlaceFloor(tx + towerSize / 2, connY, 2, MAT_GRANITE);
            
            // Horizontal connection (E/W): NW/SW open east, NE/SE open west
            int connX = (t == 0 || t == 2) ? (tx + towerSize - 1) : tx;
            PlaceFloor(connX, ty + towerSize / 2, 2, MAT_GRANITE);
        }
    }
    
    // ========================================
    // Stair towers (2 towers on opposite walls for wall walk access)
    // ========================================
    // Stair tower 1: West wall, middle
    int stair1X = castleX;
    int stair1Y = castleY + castleHeight / 2 - stairTowerSize / 2;
    
    // Stair tower 2: East wall, middle  
    int stair2X = castleX + castleWidth - stairTowerSize;
    int stair2Y = castleY + castleHeight / 2 - stairTowerSize / 2;
    
    // Build stair towers (3 levels at z=1,2,3)
    int stairTowers[2][2] = {{stair1X, stair1Y}, {stair2X, stair2Y}};
    
    for (int s = 0; s < 2; s++) {
        int sx = stairTowers[s][0];
        int sy = stairTowers[s][1];
        
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            
            for (int py = sy; py < sy + stairTowerSize; py++) {
                for (int px = sx; px < sx + stairTowerSize; px++) {
                    bool isBorder = (px == sx || px == sx + stairTowerSize - 1 ||
                                    py == sy || py == sy + stairTowerSize - 1);
                    if (isBorder) {
                        grid[z][py][px] = CELL_WALL;
                    } else {
                        PlaceFloor(px, py, z, MAT_GRANITE);
                    }
                }
            }
        }
        
        // Ladder in stair tower (connects z=1,2,3)
        int ladderX = sx + stairTowerSize / 2;
        int ladderY = sy + stairTowerSize / 2;
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            grid[z][ladderY][ladderX] = CELL_LADDER_BOTH;
        }
        
        // Door to courtyard at z=1 and connection to wall walk at z=2
        int doorX = (s == 0) ? (sx + stairTowerSize - 1) : sx;  // West: east side, East: west side
        PlaceFloor(doorX, sy + stairTowerSize / 2, 1, MAT_GRANITE);
        if (gridDepth > 2) {
            PlaceFloor(doorX, sy + stairTowerSize / 2, 2, MAT_GRANITE);
        }
    }
    
    // ========================================
    // Main gate (large opening on south wall at z=1)
    // ========================================
    int gateWidth = 4;
    int gateX = castleX + castleWidth / 2 - gateWidth / 2;
    
    // Clear the gate opening at z=1 only (wall above remains)
    for (int x = gateX; x < gateX + gateWidth; x++) {
        for (int t = 0; t < wallThickness; t++) {
            PlaceFloor(x, castleY + castleHeight - 1 - t, 1, MAT_GRANITE);
        }
    }
    
    // ========================================
    // Courtyard floor (z=1, inside the walls)
    // ========================================
    int courtyardX = castleX + wallThickness;
    int courtyardY = castleY + wallThickness;
    int courtyardW = castleWidth - 2 * wallThickness;
    int courtyardH = castleHeight - 2 * wallThickness;
    
    for (int y = courtyardY; y < courtyardY + courtyardH; y++) {
        for (int x = courtyardX; x < courtyardX + courtyardW; x++) {
            if (grid[1][y][x] != CELL_LADDER_BOTH) {
                PlaceFloor(x, y, 1, MAT_GRANITE);
            }
        }
    }
    
    // ========================================
    // Interior buildings in courtyard (DF-style: z=1+)
    // ========================================
    
    // 2-story buildings: barracks (NW) and armory (NE)
    int buildings[2][4] = {
        {courtyardX + 3, courtyardY + 3, 6, 5},                    // barracks
        {courtyardX + courtyardW - 9, courtyardY + 3, 6, 5}        // armory
    };
    
    for (int b = 0; b < 2; b++) {
        int bx = buildings[b][0], by = buildings[b][1];
        int bw = buildings[b][2], bh = buildings[b][3];
        
        for (int floor = 0; floor < 2 && floor + 1 < gridDepth; floor++) {
            int z = floor + 1;
            for (int py = by; py < by + bh; py++) {
                for (int px = bx; px < bx + bw; px++) {
                    bool isBorder = (px == bx || px == bx + bw - 1 ||
                                    py == by || py == by + bh - 1);
                    if (isBorder) {
                        grid[z][py][px] = CELL_WALL;
                    } else {
                        PlaceFloor(px, py, z, MAT_GRANITE);
                    }
                }
            }
        }
        // Door on south, ladder inside
        PlaceFloor(bx + bw / 2, by + bh - 1, 1, MAT_GRANITE);
        int lx = (b == 0) ? (bx + 1) : (bx + bw - 2);
        for (int floor = 0; floor < 2 && floor + 1 < gridDepth; floor++) {
            grid[floor + 1][by + 1][lx] = CELL_LADDER_BOTH;
        }
    }
    
    // 1-story building: stables (center-south)
    int b3x = courtyardX + courtyardW / 2 - 4;
    int b3y = courtyardY + courtyardH - 10;
    for (int py = b3y; py < b3y + 4; py++) {
        for (int px = b3x; px < b3x + 8; px++) {
            bool isBorder = (px == b3x || px == b3x + 7 || py == b3y || py == b3y + 3);
            if (isBorder) {
                grid[1][py][px] = CELL_WALL;
            } else {
                PlaceFloor(px, py, 1, MAT_GRANITE);
            }
        }
    }
    PlaceFloor(b3x + 4, b3y, 1, MAT_GRANITE);  // Door on north
    
    needsRebuild = true;
}

// ============================================================================
// Council Estate Generator
// UK-style council estate with:
// - Tower blocks (tall apartment buildings with corridors and stairwells)
// - Low-rise terraced housing (rows of small connected units)
// - Winding paths and alleyways
// - Courtyards and green spaces
// - Multi-level with ladders for vertical connections
// ============================================================================

// Helper: Build a tower block at position with specified floors
// UK council tower blocks are large with multiple stairwell cores
// Features external gallery walkways (balconies) running the length of the building
// If vertical=true, the building is rotated 90 degrees (gallery on east side instead of south)
// width/height are the TOTAL footprint dimensions (X and Y) including gallery
// baseZ is the first building floor level (ground is baseZ-1)
static void BuildTowerBlockAt(int baseX, int baseY, int width, int height, int floors, bool vertical, int baseZ, int* surface) {
    int maxFloors = gridDepth - baseZ;
    if (floors > maxFloors) floors = maxFloors;
    if (floors < 1) floors = 1;
    
    // Gallery walkway width
    int galleryWidth = 2;
    
    // For horizontal: width is X (long), height is Y (short, includes gallery)
    // For vertical: width is X (short, includes gallery), height is Y (long)
    int length;         // The long axis (along the gallery corridor)
    int buildingShort;  // The short axis excluding gallery
    
    if (!vertical) {
        length = width;
        buildingShort = height - galleryWidth;
    } else {
        length = height;
        buildingShort = width - galleryWidth;
    }
    
    // Unit dimensions
    int unitSize = 5;  // Each flat is 5 cells along the corridor
    
    // Calculate number of units and stairwell cores based on length
    int numUnits = (length - 4) / unitSize;
    int numStairCores = 1 + length / 20;
    if (numStairCores < 2) numStairCores = 2;
    if (numStairCores > 5) numStairCores = 5;
    
    // Stairwell positions (evenly distributed along length)
    int stairPositions[5];
    for (int s = 0; s < numStairCores; s++) {
        stairPositions[s] = 2 + (s * (length - 4)) / (numStairCores - 1 > 0 ? numStairCores - 1 : 1);
    }
    
    for (int floor = 0; floor < floors; floor++) {
            int z = baseZ + floor;
        if (!vertical) {
            // === HORIZONTAL ORIENTATION (gallery on south) ===
            // X extent: baseX to baseX + width
            // Y extent: baseY to baseY + height (buildingShort + gallery)
            
            // North wall (back of building)
            for (int x = baseX; x < baseX + width; x++) {
                grid[z][baseY][x] = CELL_WALL;
            }
            
            // South wall of main building (separates from gallery)
            for (int x = baseX; x < baseX + width; x++) {
                grid[z][baseY + buildingShort - 1][x] = CELL_WALL;
            }
            
            // East and west end walls
            for (int y = baseY; y < baseY + buildingShort; y++) {
                grid[z][y][baseX] = CELL_WALL;
                grid[z][y][baseX + width - 1] = CELL_WALL;
            }
            
            // Interior floor of main building
            for (int y = baseY + 1; y < baseY + buildingShort - 1; y++) {
                for (int x = baseX + 1; x < baseX + width - 1; x++) {
                    PlaceFloor(x, y, z, MAT_GRANITE);
                }
            }
            
            // Divide into flats with internal walls
            for (int u = 1; u < numUnits; u++) {
                int wallX = baseX + 1 + u * unitSize;
                if (wallX >= baseX + width - 1) break;
                
                bool isStairwell = false;
                for (int s = 0; s < numStairCores; s++) {
                    if (abs(wallX - baseX - stairPositions[s]) < 3) {
                        isStairwell = true;
                        break;
                    }
                }
                
                if (!isStairwell) {
                    for (int y = baseY + 1; y < baseY + buildingShort - 2; y++) {
                        grid[z][y][wallX] = CELL_WALL;
                    }
                }
            }
            
            // Gallery walkway (south side)
            int galleryY = baseY + buildingShort;
            for (int x = baseX; x < baseX + width; x++) {
                for (int gy = 0; gy < galleryWidth; gy++) {
                    if (galleryY + gy < gridHeight) {
                        PlaceFloor(x, galleryY + gy, z, MAT_GRANITE);
                    }
                }
            }
            
            // Gallery railing
            if (galleryY + galleryWidth < gridHeight) {
                for (int x = baseX; x < baseX + width; x++) {
                    if ((x - baseX) % 8 != 0) {
                        grid[z][galleryY + galleryWidth - 1][x] = CELL_WALL;
                    }
                }
            }
            
            // Gallery end walls
            for (int gy = 0; gy < galleryWidth; gy++) {
                if (galleryY + gy < gridHeight) {
                    grid[z][galleryY + gy][baseX] = CELL_WALL;
                    grid[z][galleryY + gy][baseX + width - 1] = CELL_WALL;
                }
            }
            
            // Doors from flats to gallery
            for (int u = 0; u < numUnits; u++) {
                int doorX = baseX + 2 + u * unitSize + unitSize / 2;
                if (doorX >= baseX + width - 1) break;
                PlaceFloor(doorX, baseY + buildingShort - 1, z, MAT_GRANITE);
            }
            
            // Stairwell cores
            for (int s = 0; s < numStairCores; s++) {
                int stairX = baseX + stairPositions[s];
                int stairY = baseY + 1;
                
                for (int sy = 0; sy < 3 && stairY + sy < baseY + buildingShort - 1; sy++) {
                    for (int sx = 0; sx < 3 && stairX + sx < baseX + width - 1; sx++) {
                        PlaceFloor(stairX + sx, stairY + sy, z, MAT_GRANITE);
                    }
                }
                
                if (stairX > baseX + 1) {
                    grid[z][stairY][stairX - 1] = CELL_WALL;
                    grid[z][stairY + 1][stairX - 1] = CELL_WALL;
                }
                if (stairX + 3 < baseX + width - 1) {
                    grid[z][stairY][stairX + 3] = CELL_WALL;
                    grid[z][stairY + 1][stairX + 3] = CELL_WALL;
                }
                
                grid[z][stairY + 1][stairX + 1] = CELL_LADDER_BOTH;
                PlaceFloor(stairX + 1, baseY + buildingShort - 1, z, MAT_GRANITE);
            }
        } else {
            // === VERTICAL ORIENTATION (gallery on east) ===
            // X extent: baseX to baseX + width (buildingShort + gallery)
            // Y extent: baseY to baseY + height
            
            // West wall (back of building)
            for (int y = baseY; y < baseY + height; y++) {
                grid[z][y][baseX] = CELL_WALL;
            }
            
            // East wall of main building (separates from gallery)
            for (int y = baseY; y < baseY + height; y++) {
                grid[z][y][baseX + buildingShort - 1] = CELL_WALL;
            }
            
            // North and south end walls
            for (int x = baseX; x < baseX + buildingShort; x++) {
                grid[z][baseY][x] = CELL_WALL;
                grid[z][baseY + height - 1][x] = CELL_WALL;
            }
            
            // Interior floor of main building
            for (int y = baseY + 1; y < baseY + height - 1; y++) {
                for (int x = baseX + 1; x < baseX + buildingShort - 1; x++) {
                    PlaceFloor(x, y, z, MAT_GRANITE);
                }
            }
            
            // Divide into flats with internal walls (horizontal walls)
            for (int u = 1; u < numUnits; u++) {
                int wallY = baseY + 1 + u * unitSize;
                if (wallY >= baseY + height - 1) break;
                
                bool isStairwell = false;
                for (int s = 0; s < numStairCores; s++) {
                    if (abs(wallY - baseY - stairPositions[s]) < 3) {
                        isStairwell = true;
                        break;
                    }
                }
                
                if (!isStairwell) {
                    for (int x = baseX + 1; x < baseX + buildingShort - 2; x++) {
                        grid[z][wallY][x] = CELL_WALL;
                    }
                }
            }
            
            // Gallery walkway (east side)
            int galleryX = baseX + buildingShort;
            for (int y = baseY; y < baseY + height; y++) {
                for (int gx = 0; gx < galleryWidth; gx++) {
                    if (galleryX + gx < gridWidth) {
                        PlaceFloor(galleryX + gx, y, z, MAT_GRANITE);
                    }
                }
            }
            
            // Gallery railing (east edge)
            if (galleryX + galleryWidth < gridWidth) {
                for (int y = baseY; y < baseY + height; y++) {
                    if ((y - baseY) % 8 != 0) {
                        grid[z][y][galleryX + galleryWidth - 1] = CELL_WALL;
                    }
                }
            }
            
            // Gallery end walls
            for (int gx = 0; gx < galleryWidth; gx++) {
                if (galleryX + gx < gridWidth) {
                    grid[z][baseY][galleryX + gx] = CELL_WALL;
                    grid[z][baseY + height - 1][galleryX + gx] = CELL_WALL;
                }
            }
            
            // Doors from flats to gallery
            for (int u = 0; u < numUnits; u++) {
                int doorY = baseY + 2 + u * unitSize + unitSize / 2;
                if (doorY >= baseY + height - 1) break;
                PlaceFloor(baseX + buildingShort - 1, doorY, z, MAT_GRANITE);
            }
            
            // Stairwell cores
            for (int s = 0; s < numStairCores; s++) {
                int stairY = baseY + stairPositions[s];
                int stairX = baseX + 1;
                
                for (int sy = 0; sy < 3 && stairY + sy < baseY + height - 1; sy++) {
                    for (int sx = 0; sx < 3 && stairX + sx < baseX + buildingShort - 1; sx++) {
                        PlaceFloor(stairX + sx, stairY + sy, z, MAT_GRANITE);
                    }
                }
                
                if (stairY > baseY + 1) {
                    grid[z][stairY - 1][stairX] = CELL_WALL;
                    grid[z][stairY - 1][stairX + 1] = CELL_WALL;
                }
                if (stairY + 3 < baseY + height - 1) {
                    grid[z][stairY + 3][stairX] = CELL_WALL;
                    grid[z][stairY + 3][stairX + 1] = CELL_WALL;
                }
                
                grid[z][stairY + 1][stairX + 1] = CELL_LADDER_BOTH;
                PlaceFloor(baseX + buildingShort - 1, stairY + 1, z, MAT_GRANITE);
            }
        }
    }
    
    // === Ground floor entrances ===
    int groundZ = baseZ - 1;
    if (groundZ < 0) groundZ = 0;
    if (!vertical) {
        int galleryY = baseY + buildingShort;
        for (int s = 0; s < numStairCores; s++) {
            int stairX = baseX + stairPositions[s];
            if (galleryY + galleryWidth < gridHeight) {
                PlaceFloor(stairX + 1, galleryY + galleryWidth - 1, groundZ, MAT_GRANITE);
            }
        }
        PlaceFloor(baseX, galleryY, groundZ, MAT_GRANITE);
        PlaceFloor(baseX + width - 1, galleryY, groundZ, MAT_GRANITE);

        int outsideY = galleryY + galleryWidth;
        if (outsideY < gridHeight) {
            for (int s = 0; s < numStairCores; s++) {
                int stairX = baseX + stairPositions[s];
                CarveEntryApron(stairX + 1, outsideY, groundZ, surface);
            }
            CarveEntryApron(baseX, outsideY, groundZ, surface);
            CarveEntryApron(baseX + width - 1, outsideY, groundZ, surface);
        }
    } else {
        int galleryX = baseX + buildingShort;
        for (int s = 0; s < numStairCores; s++) {
            int stairY = baseY + stairPositions[s];
            if (galleryX + galleryWidth < gridWidth) {
                PlaceFloor(galleryX + galleryWidth - 1, stairY + 1, groundZ, MAT_GRANITE);
            }
        }
        PlaceFloor(galleryX, baseY, groundZ, MAT_GRANITE);
        PlaceFloor(galleryX, baseY + height - 1, groundZ, MAT_GRANITE);

        int outsideX = galleryX + galleryWidth;
        if (outsideX < gridWidth) {
            for (int s = 0; s < numStairCores; s++) {
                int stairY = baseY + stairPositions[s];
                CarveEntryApron(outsideX, stairY + 1, groundZ, surface);
            }
            CarveEntryApron(outsideX, baseY, groundZ, surface);
            CarveEntryApron(outsideX, baseY + height - 1, groundZ, surface);
        }
    }
}

static void BuildTowerBlock(int baseX, int baseY, int width, int height, int floors, bool vertical) {
    BuildTowerBlockAt(baseX, baseY, width, height, floors, vertical, 1, NULL);
}

// Helper: Build 2-story terraced housing row where each unit has its own internal staircase
// If vertical=true, the row runs north-south instead of east-west
static void BuildTerraceRow(int baseX, int baseY, int numUnits, int unitWidth, int unitDepth, bool doorsNorth, bool vertical) {
    int numFloors = 2;  // 2-story terraced houses
    if (numFloors > gridDepth) numFloors = gridDepth;
    
    if (!vertical) {
        // === HORIZONTAL ORIENTATION (row runs east-west) ===
        int totalWidth = numUnits * unitWidth;
        
        for (int z = 0; z < numFloors; z++) {
            // Outer walls
            for (int x = baseX; x < baseX + totalWidth; x++) {
                grid[z][baseY][x] = CELL_WALL;
                grid[z][baseY + unitDepth - 1][x] = CELL_WALL;
            }
            for (int y = baseY; y < baseY + unitDepth; y++) {
                grid[z][y][baseX] = CELL_WALL;
                grid[z][y][baseX + totalWidth - 1] = CELL_WALL;
            }
            
            // Interior floor
            for (int y = baseY + 1; y < baseY + unitDepth - 1; y++) {
                for (int x = baseX + 1; x < baseX + totalWidth - 1; x++) {
                    PlaceFloor(x, y, z, MAT_GRANITE);
                }
            }
            
            // Internal walls between units
            for (int u = 1; u < numUnits; u++) {
                int wallX = baseX + u * unitWidth;
                for (int y = baseY; y < baseY + unitDepth; y++) {
                    grid[z][y][wallX] = CELL_WALL;
                }
            }
        }
        
        // Ground floor doors for each unit
        for (int u = 0; u < numUnits; u++) {
            int doorX = baseX + u * unitWidth + unitWidth / 2;
            if (doorsNorth) {
                PlaceFloor(doorX, baseY, 0, MAT_GRANITE);
            } else {
                PlaceFloor(doorX, baseY + unitDepth - 1, 0, MAT_GRANITE);
            }
        }
        
        // Internal staircase (ladder) in each unit
        for (int u = 0; u < numUnits; u++) {
            int ladderX = baseX + u * unitWidth + 1;
            int ladderY = doorsNorth ? (baseY + unitDepth - 2) : (baseY + 1);
            
            for (int z = 0; z < numFloors; z++) {
                grid[z][ladderY][ladderX] = CELL_LADDER_BOTH;
            }
        }
    } else {
        // === VERTICAL ORIENTATION (row runs north-south) ===
        int totalHeight = numUnits * unitWidth;  // unitWidth becomes the height of each unit
        
        for (int z = 0; z < numFloors; z++) {
            // Outer walls
            for (int y = baseY; y < baseY + totalHeight; y++) {
                grid[z][y][baseX] = CELL_WALL;
                grid[z][y][baseX + unitDepth - 1] = CELL_WALL;
            }
            for (int x = baseX; x < baseX + unitDepth; x++) {
                grid[z][baseY][x] = CELL_WALL;
                grid[z][baseY + totalHeight - 1][x] = CELL_WALL;
            }
            
            // Interior floor
            for (int y = baseY + 1; y < baseY + totalHeight - 1; y++) {
                for (int x = baseX + 1; x < baseX + unitDepth - 1; x++) {
                    PlaceFloor(x, y, z, MAT_GRANITE);
                }
            }
            
            // Internal walls between units (horizontal walls)
            for (int u = 1; u < numUnits; u++) {
                int wallY = baseY + u * unitWidth;
                for (int x = baseX; x < baseX + unitDepth; x++) {
                    grid[z][wallY][x] = CELL_WALL;
                }
            }
        }
        
        // Ground floor doors for each unit (on west or east side)
        for (int u = 0; u < numUnits; u++) {
            int doorY = baseY + u * unitWidth + unitWidth / 2;
            if (doorsNorth) {  // doorsNorth means doors on west side for vertical
                PlaceFloor(baseX, doorY, 0, MAT_GRANITE);
            } else {
                PlaceFloor(baseX + unitDepth - 1, doorY, 0, MAT_GRANITE);
            }
        }
        
        // Internal staircase (ladder) in each unit
        for (int u = 0; u < numUnits; u++) {
            int ladderY = baseY + u * unitWidth + 1;
            int ladderX = doorsNorth ? (baseX + unitDepth - 2) : (baseX + 1);
            
            for (int z = 0; z < numFloors; z++) {
                grid[z][ladderY][ladderX] = CELL_LADDER_BOTH;
            }
        }
    }
}

void GenerateCouncilEstate(void) {
    InitGrid();  // Clear cells and flags properly
    SeedTerrain();
    FillGroundLevel();
    
    // Scale building count based on grid size
    int numTerraceRows = 4 + (gridWidth * gridHeight) / 6000;
    if (numTerraceRows > 12) numTerraceRows = 12;
    
    typedef struct { int x, y, w, h; } Rect;
    Rect placed[30];
    int placedCount = 0;
    
    // Place ONE massive central tower block that scales with the world
    // This creates the dominant tower block typical of UK council estates
    // Gallery adds 2 cells to total footprint
    int galleryExtra = 2;
    
    {
        // Randomly choose orientation for main tower
        bool mainVertical = GetRandomValue(0, 1) == 0;
        
        // Tower length scales with world size - can be very long (this is the long axis)
        int maxLength = mainVertical ? gridHeight : gridWidth;
        int maxTowerLength = (maxLength * 65) / 100;  // Up to 65% of grid dimension
        int minTowerLength = 40;
        if (minTowerLength > maxTowerLength) minTowerLength = maxTowerLength;
        int towerLength = minTowerLength + GetRandomValue(0, maxTowerLength - minTowerLength);
        
        // Building depth (perpendicular to gallery, excluding gallery)
        int buildingDepth = 13 + GetRandomValue(0, 8);  // 13-21 cells
        int maxDepthAvail = (mainVertical ? gridWidth : gridHeight) / 3;
        if (buildingDepth > maxDepthAvail) buildingDepth = maxDepthAvail;
        
        // Total depth including gallery
        int totalDepth = buildingDepth + galleryExtra;
        
        // Use all z-levels for the main tower
        int towerFloors = gridDepth;
        
        // Actual footprint for collision detection
        // Horizontal: width=towerLength, height=totalDepth
        // Vertical: width=totalDepth, height=towerLength
        int footprintW = mainVertical ? totalDepth : towerLength;
        int footprintH = mainVertical ? towerLength : totalDepth;
        
        // Center the main tower block
        int tx = (gridWidth - footprintW) / 2;
        int ty = (gridHeight - footprintH) / 2;
        if (!mainVertical) {
            ty -= gridHeight / 6;  // Slightly north of center for horizontal
        } else {
            tx -= gridWidth / 6;   // Slightly west of center for vertical
        }
        
        // Clamp to valid positions
        if (tx < 5) tx = 5;
        if (ty < 5) ty = 5;
        if (tx + footprintW > gridWidth - 5) footprintW = gridWidth - tx - 5;
        if (ty + footprintH > gridHeight - 5) footprintH = gridHeight - ty - 5;
        
        // BuildTowerBlock params: width is first param (X size), depth is second (Y size for horiz)
        // For horizontal: pass (length, totalDepth)
        // For vertical: pass (totalDepth, length) - but function swaps internally based on vertical flag
        if (!mainVertical) {
            BuildTowerBlock(tx, ty, footprintW, footprintH, towerFloors, false);
        } else {
            BuildTowerBlock(tx, ty, footprintW, footprintH, towerFloors, true);
        }
        placed[placedCount++] = (Rect){tx, ty, footprintW, footprintH};
    }
    
    // Optionally add 1-2 smaller secondary tower blocks if space permits
    int numSecondaryTowers = (gridWidth > 150 && gridHeight > 150) ? 1 + GetRandomValue(0, 1) : 0;
    for (int t = 0; t < numSecondaryTowers; t++) {
        // Randomly choose orientation
        bool vertical = GetRandomValue(0, 1) == 0;
        
        // Secondary towers are smaller
        int towerLength = 25 + GetRandomValue(0, 20);   // 25-45 cells long
        int buildingDepth = 10 + GetRandomValue(0, 6);  // 10-16 cells deep (excluding gallery)
        int towerFloors = gridDepth - GetRandomValue(1, gridDepth / 2);
        if (towerFloors < 2) towerFloors = 2;
        
        int maxLen = vertical ? gridHeight / 3 : gridWidth / 3;
        int maxDep = (vertical ? gridWidth : gridHeight) / 4;
        if (towerLength > maxLen) towerLength = maxLen;
        if (buildingDepth > maxDep) buildingDepth = maxDep;
        
        int totalDepth = buildingDepth + galleryExtra;
        int footprintW = vertical ? totalDepth : towerLength;
        int footprintH = vertical ? towerLength : totalDepth;
        
        int attempts = 100;
        while (attempts-- > 0) {
            int tx = 5 + GetRandomValue(0, gridWidth - footprintW - 10);
            int ty = 5 + GetRandomValue(0, gridHeight - footprintH - 10);
            
            // Check overlap with existing buildings
            bool overlaps = false;
            for (int p = 0; p < placedCount; p++) {
                int margin = 10;
                if (tx < placed[p].x + placed[p].w + margin && tx + footprintW + margin > placed[p].x &&
                    ty < placed[p].y + placed[p].h + margin && ty + footprintH + margin > placed[p].y) {
                    overlaps = true;
                    break;
                }
            }
            
            if (!overlaps) {
                BuildTowerBlock(tx, ty, footprintW, footprintH, towerFloors, vertical);
                placed[placedCount++] = (Rect){tx, ty, footprintW, footprintH};
                break;
            }
        }
    }
    
    // Place terraced housing rows (2-story, multiple units)
    // These fill in the spaces around the massive tower blocks
    // Randomly oriented horizontally or vertically
    int terraceUnitSize = 5;    // Size along the row
    int terraceUnitDepth = 6;   // Depth perpendicular to row
    
    for (int r = 0; r < numTerraceRows && placedCount < 30; r++) {
        // Randomly choose orientation
        bool vertical = GetRandomValue(0, 1) == 0;
        
        // Vary the number of units per row
        int unitsPerRow = 5 + GetRandomValue(0, 5);  // 5-10 units per row
        int rowLength = unitsPerRow * terraceUnitSize;
        
        // Ensure row fits in grid
        int maxLen = vertical ? gridHeight - 10 : gridWidth - 10;
        if (rowLength > maxLen) {
            unitsPerRow = maxLen / terraceUnitSize;
            rowLength = unitsPerRow * terraceUnitSize;
        }
        
        // Calculate bounding box based on orientation
        int rowWidth = vertical ? terraceUnitDepth : rowLength;
        int rowHeight = vertical ? rowLength : terraceUnitDepth;
        
        int attempts = 80;
        while (attempts-- > 0) {
            int rx = 5 + GetRandomValue(0, gridWidth - rowWidth - 10);
            int ry = 5 + GetRandomValue(0, gridHeight - rowHeight - 10);
            
            // Check overlap
            bool overlaps = false;
            for (int p = 0; p < placedCount; p++) {
                int margin = 5;
                if (rx < placed[p].x + placed[p].w + margin && rx + rowWidth + margin > placed[p].x &&
                    ry < placed[p].y + placed[p].h + margin && ry + rowHeight + margin > placed[p].y) {
                    overlaps = true;
                    break;
                }
            }
            
            if (!overlaps) {
                bool doorsNorth = GetRandomValue(0, 1) == 0;
                BuildTerraceRow(rx, ry, unitsPerRow, terraceUnitSize, terraceUnitDepth, doorsNorth, vertical);
                placed[placedCount++] = (Rect){rx, ry, rowWidth, rowHeight};
                break;
            }
        }
    }
    
    // Scatter trees/debris in open areas (CELL_WALL with MAT_DIRT only, not inside buildings)
    // This adds visual interest to the green spaces around the estate
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][y][x] == CELL_WALL && GetWallMaterial(x, y, 0) == MAT_DIRT && GetRandomValue(0, 100) < 3) {
                MaterialType treeMat = PickTreeTypeForSoilSimple(GetWallMaterial(x, y, 0));
                PlaceWorldGenTree(x, y, 0, treeMat, true);
            }
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
void GenerateMixed(void) {
    InitGrid();
    SeedTerrain();
    FillGroundLevel();
    int zoneSize = chunkWidth * 4;
    int zonesX = (gridWidth + zoneSize - 1) / zoneSize;
    int zonesY = (gridHeight + zoneSize - 1) / zoneSize;
    int zones[16][16];
    for (int zy = 0; zy < zonesY && zy < 16; zy++)
        for (int zx = 0; zx < zonesX && zx < 16; zx++)
            zones[zy][zx] = GetRandomValue(0, 100) < 50 ? 1 : 0;

    for (int wy = chunkHeight; wy < gridHeight; wy += chunkHeight / 2) {
        for (int wx = 0; wx < gridWidth; wx++) {
            int zx = wx / zoneSize, zy = wy / zoneSize;
            if (zx >= 16 || zy >= 16 || zones[zy][zx] == 0) { wx += GetRandomValue(10, 30); continue; }
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int x = wx; x < wx + gapPos && x < gridWidth; x++) {
                int zx2 = x / zoneSize;
                if (zx2 < 16 && zones[zy][zx2] == 1) {
                    grid[1][wy][x] = CELL_WALL;
                    if (wy + 1 < gridHeight) grid[1][wy + 1][x] = CELL_WALL;
                }
            }
            wx += gapPos + gapSize;
        }
    }
    for (int wx = chunkWidth; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight; wy++) {
            int zx = wx / zoneSize, zy = wy / zoneSize;
            if (zx >= 16 || zy >= 16 || zones[zy][zx] == 0) { wy += GetRandomValue(10, 30); continue; }
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int y = wy; y < wy + gapPos && y < gridHeight; y++) {
                int zy2 = y / zoneSize;
                if (zy2 < 16 && zones[zy2][zx] == 1) {
                    grid[1][y][wx] = CELL_WALL;
                    if (wx + 1 < gridWidth) grid[1][y][wx + 1] = CELL_WALL;
                }
            }
            wy += gapPos + gapSize;
        }
    }
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][y][x] == CELL_WALL && GetWallMaterial(x, y, 0) == MAT_DIRT) {
                int zx = x / zoneSize, zy = y / zoneSize;
                bool isCity = (zx < 16 && zy < 16 && zones[zy][zx] == 1);
                int chance = isCity ? 3 : 15;
                if (GetRandomValue(0, 100) < chance) {
                    MaterialType treeMat = PickTreeTypeForSoilSimple(GetWallMaterial(x, y, 0));
                    PlaceWorldGenTree(x, y, 0, treeMat, true);
                }
            }
        }
    }
    needsRebuild = true;
}


// ============================================================================
// Crafting Test Scenario
// Small map with stonecutter workshop, stockpiles, and walls to mine
// Tests the full crafting loop: mine -> haul -> craft -> haul -> build
// ============================================================================

void GenerateCraftingTest(void) {
    // Use small grid for testing
    InitGridWithSizeAndChunkSize(30, 30, 16, 16);
    
    // Clear entities from previous map
    ClearWorkshops();
    ClearStockpiles();
    ClearItems();
    InitDesignations();
    
    // Clear blueprints
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        blueprints[i].active = false;
    }
    blueprintCount = 0;
    
    // Fill z=0 with dirt (ground) - this is the walkable level
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
        }
    }
    
    // Add minable walls in a cluster (top-left area) at z=1 (above ground)
    for (int y = 2; y <= 5; y++) {
        for (int x = 2; x <= 5; x++) {
            grid[1][y][x] = CELL_WALL;
        }
    }
    
    // Pre-designate some walls for mining
    for (int y = 2; y <= 4; y++) {
        for (int x = 2; x <= 4; x++) {
            DesignateMine(x, y, 1);
        }
    }
    
    // Create stonecutter workshop at (15, 15) on z=1 (walking level)
    // Workshop is 3x3, work tile at center, output tile to the right
    int wsIdx = CreateWorkshop(15, 15, 1, WORKSHOP_STONECUTTER);
    if (wsIdx >= 0) {
        // Add a "Cut Stone Blocks" bill - DO_FOREVER
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);
    }
    
    // Create input stockpile (2x2) to the left of workshop - accepts ORANGE only
    int inputSp = CreateStockpile(12, 15, 1, 2, 2);
    if (inputSp >= 0) {
        // Disable all types except ORANGE
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(inputSp, t, false);
        }
        SetStockpileFilter(inputSp, ITEM_ROCK, true);
        SetStockpilePriority(inputSp, 7);  // High priority
    }
    
    // Create output stockpile (2x2) to the right of workshop - accepts STONE_BLOCKS only
    int outputSp = CreateStockpile(19, 15, 1, 2, 2);
    if (outputSp >= 0) {
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(outputSp, t, false);
        }
        SetStockpileFilter(outputSp, ITEM_BLOCKS, true);
        SetStockpilePriority(outputSp, 7);
    }
    
    // Create construction stockpile (2x2) near build site - accepts STONE_BLOCKS
    int buildSp = CreateStockpile(8, 10, 1, 2, 2);
    if (buildSp >= 0) {
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(buildSp, t, false);
        }
        SetStockpileFilter(buildSp, ITEM_BLOCKS, true);
        SetStockpilePriority(buildSp, 5);  // Normal priority
    }
    
    // Create wall blueprint at (6, 10) - requires STONE_BLOCKS
    CreateBuildBlueprint(6, 10, 1);
    
    needsRebuild = true;
}
