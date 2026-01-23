#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include "../shared/profiler.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Fast inverse square root (Quake III algorithm)
static inline float fastInvSqrt(float x) {
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f375a86 - (i >> 1);  // Initial guess
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);  // Newton-Raphson iteration
    return x;
}

// Globals
Mover movers[MAX_MOVERS];
int moverCount = 0;
unsigned long currentTick = 0;
bool useStringPulling = true;
bool endlessMoverMode = true;
bool useMoverAvoidance = true;
bool preferDifferentZ = true;
bool allowFallingFromAvoidance = false;
bool useKnotFix = true;
bool useWallRepulsion = true;
float wallRepulsionStrength = 0.5f;
bool useWallSliding = true;
float avoidStrengthOpen = 0.5f;
float avoidStrengthClosed = 0.0f;
bool useDirectionalAvoidance = true;
PathAlgorithm moverPathAlgorithm = PATH_ALGO_HPA;  // Default to HPA*
bool useRandomizedCooldowns = true;   // Default on for demo perf, tests can disable
bool useStaggeredUpdates = true;      // Default on for demo perf, tests can disable

// Spatial grid
MoverSpatialGrid moverGrid = {0};

static inline int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Try to make a mover fall to ground. Returns true if mover fell.
// Searches downward from current z for a walkable cell, stops at walls.
static bool TryFallToGround(Mover* m, int cellX, int cellY) {
    int currentZ = (int)m->z;
    
    for (int checkZ = currentZ - 1; checkZ >= 0; checkZ--) {
        if (IsCellWalkableAt(checkZ, cellY, cellX)) {
            m->z = (float)checkZ;
            m->needsRepath = true;
            m->fallTimer = 1.0f;
            return true;
        }
        if (grid[checkZ][cellY][cellX] == CELL_WALL) {
            break;  // Can't fall through walls
        }
    }
    return false;
}

void InitMoverSpatialGrid(int worldPixelWidth, int worldPixelHeight) {
    FreeMoverSpatialGrid();
    
    moverGrid.invCellSize = 1.0f / MOVER_GRID_CELL_SIZE;
    moverGrid.gridW = (int)ceilf(worldPixelWidth * moverGrid.invCellSize);
    moverGrid.gridH = (int)ceilf(worldPixelHeight * moverGrid.invCellSize);
    moverGrid.cellCount = moverGrid.gridW * moverGrid.gridH;
    
    moverGrid.cellCounts = (int*)calloc(moverGrid.cellCount, sizeof(int));
    moverGrid.cellStarts = (int*)calloc(moverGrid.cellCount + 1, sizeof(int));
    moverGrid.moverIndices = (int*)malloc(MAX_MOVERS * sizeof(int));
}

void FreeMoverSpatialGrid(void) {
    free(moverGrid.cellCounts);
    free(moverGrid.cellStarts);
    free(moverGrid.moverIndices);
    moverGrid.cellCounts = NULL;
    moverGrid.cellStarts = NULL;
    moverGrid.moverIndices = NULL;
}

void BuildMoverSpatialGrid(void) {
    if (!moverGrid.cellCounts) return;
    
    // Clear counts
    memset(moverGrid.cellCounts, 0, moverGrid.cellCount * sizeof(int));
    
    // Count movers per cell
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        
        int cx = (int)(movers[i].x * moverGrid.invCellSize);
        int cy = (int)(movers[i].y * moverGrid.invCellSize);
        cx = clampi(cx, 0, moverGrid.gridW - 1);
        cy = clampi(cy, 0, moverGrid.gridH - 1);
        
        moverGrid.cellCounts[cy * moverGrid.gridW + cx]++;
    }
    
    // Build prefix sum
    moverGrid.cellStarts[0] = 0;
    for (int c = 0; c < moverGrid.cellCount; c++) {
        moverGrid.cellStarts[c + 1] = moverGrid.cellStarts[c] + moverGrid.cellCounts[c];
    }
    
    // Reset counts to use as write cursors
    for (int c = 0; c < moverGrid.cellCount; c++) {
        moverGrid.cellCounts[c] = moverGrid.cellStarts[c];
    }
    
    // Scatter mover indices into cells
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        
        int cx = (int)(movers[i].x * moverGrid.invCellSize);
        int cy = (int)(movers[i].y * moverGrid.invCellSize);
        cx = clampi(cx, 0, moverGrid.gridW - 1);
        cy = clampi(cy, 0, moverGrid.gridH - 1);
        
        int cellIdx = cy * moverGrid.gridW + cx;
        moverGrid.moverIndices[moverGrid.cellCounts[cellIdx]++] = i;
    }
}



Vec2 ComputeMoverAvoidance(int moverIndex) {
    Vec2 avoidance = {0.0f, 0.0f};
    
    // Guard: return early if spatial grid not initialized
    if (!moverGrid.cellCounts) return avoidance;
    
    Mover* m = &movers[moverIndex];
    if (!m->active) return avoidance;
    
    float radius = MOVER_AVOID_RADIUS;
    float radiusSq = radius * radius;
    float invRadius = 1.0f / radius;
    
    int found = 0;
    int scanned = 0;
    
    // Compute cell range to search
    int radCells = (int)ceilf(radius * moverGrid.invCellSize);
    int cx = (int)(m->x * moverGrid.invCellSize);
    int cy = (int)(m->y * moverGrid.invCellSize);
    
    int minCx = clampi(cx - radCells, 0, moverGrid.gridW - 1);
    int maxCx = clampi(cx + radCells, 0, moverGrid.gridW - 1);
    int minCy = clampi(cy - radCells, 0, moverGrid.gridH - 1);
    int maxCy = clampi(cy + radCells, 0, moverGrid.gridH - 1);
    
    for (int gy = minCy; gy <= maxCy; gy++) {
        for (int gx = minCx; gx <= maxCx; gx++) {
            int cellIdx = gy * moverGrid.gridW + gx;
            int start = moverGrid.cellStarts[cellIdx];
            int end = moverGrid.cellStarts[cellIdx + 1];
            
            for (int t = start; t < end; t++) {
                int j = moverGrid.moverIndices[t];
                if (j == moverIndex) continue;
                
                scanned++;
                if (scanned >= AVOID_MAX_SCAN) {
                    return avoidance;
                }
                
                float dx = m->x - movers[j].x;
                float dy = m->y - movers[j].y;
                float distSq = dx * dx + dy * dy;
                
                if (distSq < 1e-10f || distSq >= radiusSq) continue;
                
                float invDist = fastInvSqrt(distSq);
                float dist = distSq * invDist;
                
                // Strength increases quadratically as distance decreases
                float u = 1.0f - dist * invRadius;
                float strength = u * u;
                
                // Add repulsion (direction * strength / distance)
                float k = strength * invDist;
                avoidance.x += dx * k;
                avoidance.y += dy * k;
                
                found++;
                if (found >= AVOID_MAX_NEIGHBORS) {
                    return avoidance;
                }
            }
        }
    }
    
    return avoidance;
}

bool IsMoverInOpenArea(float x, float y, int z) {
    // Convert pixel position to grid cell
    int cellX = (int)(x / CELL_SIZE);
    int cellY = (int)(y / CELL_SIZE);
    
    // Check 3x3 area around the mover's cell
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cx = cellX + dx;
            int cy = cellY + dy;
            
            // Non-walkable (wall, air, out of bounds) = not open
            if (!IsCellWalkableAt(z, cy, cx)) {
                return false;
            }
        }
    }
    
    return true;
}

// Check if there's clearance in a direction
// Checks a 3x2 strip: current cell + neighbor in that direction, for 3 rows
// dir: 0=up (-y), 1=right (+x), 2=down (+y), 3=left (-x)
bool HasClearanceInDirection(float x, float y, int z, int dir) {
    int cellX = (int)(x / CELL_SIZE);
    int cellY = (int)(y / CELL_SIZE);
    
    // Direction offsets for the "forward" cell
    int fdx[] = {0, 1, 0, -1};  // up, right, down, left
    int fdy[] = {-1, 0, 1, 0};
    
    // Perpendicular offsets (to check 3 cells along the edge)
    int pdx[] = {1, 0, 1, 0};   // perpendicular x
    int pdy[] = {0, 1, 0, 1};   // perpendicular y
    
    int fx = fdx[dir];
    int fy = fdy[dir];
    int px = pdx[dir];
    int py = pdy[dir];
    
    // Check 3 cells in the forward direction (at -1, 0, +1 perpendicular)
    for (int p = -1; p <= 1; p++) {
        int cx = cellX + fx + p * px;
        int cy = cellY + fy + p * py;
        
        // Non-walkable (wall, air, out of bounds) = no clearance
        if (!IsCellWalkableAt(z, cy, cx)) {
            return false;
        }
    }
    
    return true;
}

// Compute wall repulsion force - pushes mover away from walls only
// Air cells do NOT repel - movers can be pushed into air and will fall
Vec2 ComputeWallRepulsion(float x, float y, int z) {
    Vec2 repulsion = {0.0f, 0.0f};
    
    int cellX = (int)(x / CELL_SIZE);
    int cellY = (int)(y / CELL_SIZE);
    
    // Check 3x3 area around mover for walls only (not air - we allow falling into air)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cx = cellX + dx;
            int cy = cellY + dy;
            
            // Skip out of bounds
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) continue;
            
            // Only repel from walls, not air (movers can fall through air)
            if (grid[z][cy][cx] != CELL_WALL) continue;
            
            // Wall cell center
            float wallX = cx * CELL_SIZE + CELL_SIZE * 0.5f;
            float wallY = cy * CELL_SIZE + CELL_SIZE * 0.5f;
            
            // Vector from wall to mover
            float dirX = x - wallX;
            float dirY = y - wallY;
            float distSq = dirX * dirX + dirY * dirY;
            
            if (distSq < 1e-10f) continue;
            
            // Only repel within radius (compare squared to avoid sqrt)
            if (distSq >= WALL_REPULSION_RADIUS * WALL_REPULSION_RADIUS) continue;
            
            float invDist = fastInvSqrt(distSq);
            float dist = distSq * invDist;
            
            // Strength increases as mover gets closer (quadratic falloff)
            float t = 1.0f - dist / WALL_REPULSION_RADIUS;
            float strength = t * t;
            
            // Add repulsion (normalized direction * strength)
            repulsion.x += dirX * invDist * strength;
            repulsion.y += dirY * invDist * strength;
        }
    }
    
    return repulsion;
}

Vec2 FilterAvoidanceByWalls(float x, float y, int z, Vec2 avoidance) {
    // If falling from avoidance is allowed, don't filter - let them fall
    if (allowFallingFromAvoidance) {
        return avoidance;
    }
    
    Vec2 result = avoidance;
    
    // Check if avoidance pushes in a direction without clearance
    // and zero that component if so
    
    if (avoidance.x > 0.01f) {
        // Pushing right
        if (!HasClearanceInDirection(x, y, z, 1)) {
            result.x = 0.0f;
        }
    } else if (avoidance.x < -0.01f) {
        // Pushing left
        if (!HasClearanceInDirection(x, y, z, 3)) {
            result.x = 0.0f;
        }
    }
    
    if (avoidance.y > 0.01f) {
        // Pushing down
        if (!HasClearanceInDirection(x, y, z, 2)) {
            result.y = 0.0f;
        }
    } else if (avoidance.y < -0.01f) {
        // Pushing up
        if (!HasClearanceInDirection(x, y, z, 0)) {
            result.y = 0.0f;
        }
    }
    
    return result;
}

int QueryMoverNeighbors(float x, float y, float radius, int excludeIndex,
                        MoverNeighborCallback callback, void* userData) {
    if (!moverGrid.cellCounts) return 0;
    
    float radiusSq = radius * radius;
    int found = 0;
    
    // Compute cell range to search
    int radCells = (int)ceilf(radius * moverGrid.invCellSize);
    int cx = (int)(x * moverGrid.invCellSize);
    int cy = (int)(y * moverGrid.invCellSize);
    
    int minCx = clampi(cx - radCells, 0, moverGrid.gridW - 1);
    int maxCx = clampi(cx + radCells, 0, moverGrid.gridW - 1);
    int minCy = clampi(cy - radCells, 0, moverGrid.gridH - 1);
    int maxCy = clampi(cy + radCells, 0, moverGrid.gridH - 1);
    
    for (int gy = minCy; gy <= maxCy; gy++) {
        for (int gx = minCx; gx <= maxCx; gx++) {
            int cellIdx = gy * moverGrid.gridW + gx;
            int start = moverGrid.cellStarts[cellIdx];
            int end = moverGrid.cellStarts[cellIdx + 1];
            
            for (int t = start; t < end; t++) {
                int moverIdx = moverGrid.moverIndices[t];
                if (moverIdx == excludeIndex) continue;
                
                float dx = movers[moverIdx].x - x;
                float dy = movers[moverIdx].y - y;
                float distSq = dx * dx + dy * dy;
                
                if (distSq < radiusSq) {
                    if (callback) {
                        callback(moverIdx, distSq, userData);
                    }
                    found++;
                }
            }
        }
    }
    
    return found;
}

// Check if there's line-of-sight between two points (Bresenham)
bool HasLineOfSight(int x0, int y0, int x1, int y1, int z) {
    // Bounds check
    if (x0 < 0 || x0 >= gridWidth || y0 < 0 || y0 >= gridHeight ||
        x1 < 0 || x1 >= gridWidth || y1 < 0 || y1 >= gridHeight) {
        return false;
    }
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0, y = y0;
    while (1) {
        if (!IsCellWalkableAt(z, y, x)) return false;
        if (x == x1 && y == y1) return true;

        int e2 = 2 * err;

        // For diagonal movement, check corner cutting
        if (e2 > -dy && e2 < dx) {
            // Moving diagonally - check both adjacent cells
            int nx = x + sx;
            int ny = y + sy;
            if (!IsCellWalkableAt(z, y, nx) || !IsCellWalkableAt(z, ny, x)) {
                return false;
            }
        }

        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// Check if there's a clear corridor between two points (for string pulling).
// Instead of just checking center-to-center LOS (which can graze corners),
// we verify LOS from all 4 cardinal neighbors of the start point.
// This ensures the path is safe regardless of sub-cell mover position.
// The runtime check (HasLineOfSightLenient) is the inverse: it passes if
// ANY neighbor has LOS, preventing movers from getting stuck on valid paths.
static bool HasClearCorridor(int x0, int y0, int x1, int y1, int z) {
    // First check the center line
    if (!HasLineOfSight(x0, y0, x1, y1, z)) return false;
    
    int dx = x1 - x0;
    int dy = y1 - y0;
    
    // Skip extra checks for short cardinal moves (1 cell orthogonal)
    if ((dx == 0 && abs(dy) <= 1) || (dy == 0 && abs(dx) <= 1)) {
        return true;
    }
    
    // Check LOS from all 4 cardinal neighbors of start point
    int ndx[] = {0, 0, -1, 1};
    int ndy[] = {-1, 1, 0, 0};
    
    for (int i = 0; i < 4; i++) {
        int nx = x0 + ndx[i];
        int ny = y0 + ndy[i];
        if (IsCellWalkableAt(z, ny, nx)) {
            if (!HasLineOfSight(nx, ny, x1, y1, z)) return false;
        }
    }
    
    return true;
}

// Lenient LOS check for runtime - returns true if LOS exists from current cell
// OR any cardinal neighbor. Matches the corridor check used in string pulling.
static bool HasLineOfSightLenient(int x0, int y0, int x1, int y1, int z) {
    // First try from current position
    if (HasLineOfSight(x0, y0, x1, y1, z)) return true;
    
    // Try from all 4 cardinal neighbors - if ANY has LOS, we're okay
    int ndx[] = {0, 0, -1, 1};
    int ndy[] = {-1, 1, 0, 0};
    
    for (int i = 0; i < 4; i++) {
        int nx = x0 + ndx[i];
        int ny = y0 + ndy[i];
        if (IsCellWalkableAt(z, ny, nx)) {
            if (HasLineOfSight(nx, ny, x1, y1, z)) return true;
        }
    }
    
    return false;
}

// String pulling: remove unnecessary waypoints from path
// Uses HasClearCorridor instead of HasLineOfSight to avoid corner-grazing paths
void StringPullPath(Point* pathArr, int* pathLen) {
    if (*pathLen <= 2) return;

    Point result[MAX_PATH];
    int resultLen = 0;

    result[resultLen++] = pathArr[*pathLen - 1];
    int current = *pathLen - 1;

    while (current > 0) {
        int furthest = current - 1;
        for (int i = 0; i < current; i++) {
            // Don't string-pull across z-level changes (would skip ladder waypoints)
            if (pathArr[current].z != pathArr[i].z) continue;
            
            if (HasClearCorridor(pathArr[current].x, pathArr[current].y,
                                 pathArr[i].x, pathArr[i].y, pathArr[current].z)) {
                furthest = i;
                break;
            }
        }
        result[resultLen++] = pathArr[furthest];
        current = furthest;
    }

    for (int i = 0; i < resultLen; i++) {
        pathArr[i] = result[resultLen - 1 - i];
    }
    *pathLen = resultLen;
}

void InitMover(Mover* m, float x, float y, float z, Point goal, float speed) {
    m->x = x;
    m->y = y;
    m->z = z;
    m->goal = goal;
    m->speed = speed;
    m->active = true;
    m->needsRepath = false;
    m->repathCooldown = 0;
    m->pathLength = 0;
    m->pathIndex = -1;
    m->timeNearWaypoint = 0.0f;
    m->lastX = x;
    m->lastY = y;
    m->lastZ = z;
    m->timeWithoutProgress = 0.0f;
    // Job system fields
    m->jobState = JOB_IDLE;
    m->targetItem = -1;
    m->carryingItem = -1;
    m->targetStockpile = -1;
    m->targetSlotX = -1;
    m->targetSlotY = -1;
}

void InitMoverWithPath(Mover* m, float x, float y, float z, Point goal, float speed, Point* pathArr, int pathLen) {
    InitMover(m, x, y, z, goal, speed);
    m->pathLength = (pathLen > MAX_MOVER_PATH) ? MAX_MOVER_PATH : pathLen;
    
    // Path is stored goal-to-start: path[0]=goal, path[pathLen-1]=start
    // If truncating, keep the START end (high indices), not the goal end
    int srcOffset = pathLen - m->pathLength;
    for (int i = 0; i < m->pathLength; i++) {
        m->path[i] = pathArr[srcOffset + i];
    }
    m->pathIndex = m->pathLength - 1;
}

void ClearMovers(void) {
    moverCount = 0;
    currentTick = 0;
    // Initialize spatial grid if grid dimensions are set
    if (gridWidth > 0 && gridHeight > 0) {
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    }
}

int CountActiveMovers(void) {
    int count = 0;
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) count++;
    }
    return count;
}

// Assign a new random goal to a mover and compute path
static void AssignNewMoverGoal(Mover* m) {
    Point newGoal;
    if (preferDifferentZ && gridDepth > 1) {
        newGoal = GetRandomWalkableCellDifferentZ((int)m->z);
    } else {
        newGoal = GetRandomWalkableCell();
    }
    m->goal = newGoal;

    int currentX = (int)(m->x / CELL_SIZE);
    int currentY = (int)(m->y / CELL_SIZE);
    int currentZ = (int)m->z;
    Point start = {currentX, currentY, currentZ};

    Point tempPath[MAX_PATH];
    int len = FindPath(moverPathAlgorithm, start, newGoal, tempPath, MAX_PATH);

    m->pathLength = (len > MAX_MOVER_PATH) ? MAX_MOVER_PATH : len;
    // Path is stored goal-to-start: path[0]=goal, path[pathLen-1]=start
    // If truncating, keep the START end (high indices), not the goal end
    int srcOffset = len - m->pathLength;
    for (int j = 0; j < m->pathLength; j++) {
        m->path[j] = tempPath[srcOffset + j];
    }

    if (useStringPulling && m->pathLength > 2) {
        StringPullPath(m->path, &m->pathLength);
    }

    m->pathIndex = m->pathLength - 1;
    m->needsRepath = false;
}

void UpdateMovers(void) {
    float dt = TICK_DT;
    
    // Phase 1: LOS checks (optionally staggered - each mover checks every 3 frames)
    PROFILE_BEGIN(LOS);
    for (int i = 0; i < moverCount; i++) {
        // Stagger: each mover checks on a different frame (if enabled)
        if (useStaggeredUpdates && (currentTick % 3) != (i % 3)) continue;
        
        Mover* m = &movers[i];
        if (!m->active || m->needsRepath) continue;
        if (m->pathIndex < 0 || m->pathLength == 0) continue;
        
        int currentX = (int)(m->x / CELL_SIZE);
        int currentY = (int)(m->y / CELL_SIZE);
        int currentZ = (int)m->z;
        
        // Skip movers in air or on walls (handled in phase 3)
        if (IsCellAirAt(currentZ, currentY, currentX)) continue;
        if (grid[currentZ][currentY][currentX] == CELL_WALL) continue;
        
        Point target = m->path[m->pathIndex];
        if (target.z == currentZ) {
            if (!HasLineOfSightLenient(currentX, currentY, target.x, target.y, currentZ)) {
                m->needsRepath = true;
            }
        }
    }
    PROFILE_END(LOS);
    
    // Phase 2: Avoidance computation (just compute, don't move yet)
    // Only recompute every 3 frames per mover, staggered by mover index
    static Vec2 avoidVectors[MAX_MOVERS];
    PROFILE_BEGIN(Avoid);
    if (useMoverAvoidance || useWallRepulsion) {
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            
            if (!m->active || m->needsRepath) {
                avoidVectors[i] = (Vec2){0, 0};
                continue;
            }
            if (m->pathIndex < 0 || m->pathLength == 0) {
                avoidVectors[i] = (Vec2){0, 0};
                continue;
            }
            
            // Stagger: each mover recomputes on a different frame (based on index, if enabled)
            if (!useStaggeredUpdates || (currentTick % 3) == (i % 3)) {
                // Recompute and cache
                Vec2 avoid = {0, 0};
                if (useMoverAvoidance) {
                    avoid = ComputeMoverAvoidance(i);
                    int moverZ = (int)m->z;
                    if (useDirectionalAvoidance) {
                        avoid = FilterAvoidanceByWalls(m->x, m->y, moverZ, avoid);
                    }
                }
                if (useWallRepulsion) {
                    Vec2 wallRepel = ComputeWallRepulsion(m->x, m->y, (int)m->z);
                    avoid.x += wallRepel.x * wallRepulsionStrength;
                    avoid.y += wallRepel.y * wallRepulsionStrength;
                }
                m->avoidX = avoid.x;
                m->avoidY = avoid.y;
            }
            
            // Use cached value
            avoidVectors[i] = (Vec2){m->avoidX, m->avoidY};
        }
    }
    PROFILE_END(Avoid);
    
    // Phase 3: Movement (all the rest)
    PROFILE_BEGIN(Move);
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        
        // Decrement fall timer for visual feedback
        if (m->fallTimer > 0) {
            m->fallTimer -= dt;
        }
        
        int currentX = (int)(m->x / CELL_SIZE);
        int currentY = (int)(m->y / CELL_SIZE);
        int currentZ = (int)m->z;

        // Check if mover is standing in air - fall to ground (check first, before any other logic)
        if (IsCellAirAt(currentZ, currentY, currentX)) {
            TryFallToGround(m, currentX, currentY);
            continue;
        }
        
        // Don't move movers that are waiting for a repath - they'd walk on stale paths
        if (m->needsRepath) continue;

        // Handle movers that need a new goal (reached destination or have no path)
        if (m->pathIndex < 0 || m->pathLength == 0) {
            if (endlessMoverMode) {
                // Wait for cooldown before trying a new goal
                if (m->repathCooldown > 0) {
                    m->repathCooldown--;
                    continue;
                }
                AssignNewMoverGoal(m);
                if (m->pathLength == 0) {
                    // No path found, wait before retrying
                    if (useRandomizedCooldowns) {
                        // Randomize to avoid synchronized retries causing spikes
                        m->repathCooldown = TICK_RATE + (rand() % TICK_RATE);
                    } else {
                        // Deterministic for tests
                        m->repathCooldown = REPATH_COOLDOWN_FRAMES;
                    }
                }
            } else {
                m->active = false;
            }
            continue;
        }
        
        // Check if mover is standing on a wall - push to nearest walkable
        if (grid[currentZ][currentY][currentX] == CELL_WALL) {
            int dx[] = {0, 0, -1, 1};
            int dy[] = {-1, 1, 0, 0};
            bool pushed = false;
            for (int d = 0; d < 4; d++) {
                int nx = currentX + dx[d];
                int ny = currentY + dy[d];
                if (IsCellWalkableAt(currentZ, ny, nx)) {
                    m->x = nx * CELL_SIZE + CELL_SIZE * 0.5f;
                    m->y = ny * CELL_SIZE + CELL_SIZE * 0.5f;
                    pushed = true;
                    break;
                }
            }
            if (!pushed) {
                m->active = false;
                printf("WARNING: Mover %d deactivated: stuck in wall with no escape\n", i);
            }
            m->needsRepath = true;
            continue;
        }

        Point target = m->path[m->pathIndex];
        
        // Skip if marked for repath (by LOS check in phase 1, or wall-push above)
        if (m->needsRepath) continue;

        float tx = target.x * CELL_SIZE + CELL_SIZE * 0.5f;
        float ty = target.y * CELL_SIZE + CELL_SIZE * 0.5f;

        float dxf = tx - m->x;
        float dyf = ty - m->y;
        float distSq = dxf*dxf + dyf*dyf;
        float dist = distSq * fastInvSqrt(distSq);

        // Waypoint arrival check
        // Original: snap to waypoint when very close (m->speed * dt, ~1.67px)
        // Knot fix: advance to next waypoint at larger radius without snapping position
        float arrivalRadius = m->speed * dt;
        bool shouldSnap = true;
        
        if (useKnotFix && dist < KNOT_FIX_ARRIVAL_RADIUS) {
            // Knot fix: advance waypoint at larger radius, no position snap
            arrivalRadius = KNOT_FIX_ARRIVAL_RADIUS;
            shouldSnap = false;
        }
        
        if (dist < arrivalRadius) {
            if (shouldSnap) {
                m->x = tx;
                m->y = ty;
            }
            // Only update z-level when target waypoint is a ladder cell
            if (target.z != (int)m->z) {
                // Changing z-level - use the waypoint's coordinates (not mover's position)
                // The pathfinder guaranteed this waypoint is a ladder cell
                int cellZ = (int)m->z;
                if (IsLadderCell(grid[cellZ][target.y][target.x]) && 
                    IsLadderCell(grid[target.z][target.y][target.x])) {
                    m->z = (float)target.z;
                    // Snap to ladder cell center for clean transition
                    m->x = target.x * CELL_SIZE + CELL_SIZE * 0.5f;
                    m->y = target.y * CELL_SIZE + CELL_SIZE * 0.5f;
                }
            }
            m->pathIndex--;
            m->timeNearWaypoint = 0.0f;  // Reset on waypoint arrival
        } else {
            // Track time near waypoint (for knot detection)
            if (dist < KNOT_NEAR_RADIUS) {
                m->timeNearWaypoint += dt;
            } else {
                m->timeNearWaypoint = 0.0f;  // Reset when far from waypoint
            }
            float invDist = 1.0f / dist;
            
            // Base velocity toward waypoint
            float vx = dxf * invDist * m->speed;
            float vy = dyf * invDist * m->speed;
            
            // Apply precomputed avoidance from phase 2
            if (useMoverAvoidance || useWallRepulsion) {
                float avoidScale = m->speed * avoidStrengthOpen;
                
                // Knot fix: reduce avoidance near waypoint so mover can reach it
                if (useKnotFix && dist < KNOT_FIX_ARRIVAL_RADIUS * 2.0f) {
                    float t = dist / (KNOT_FIX_ARRIVAL_RADIUS * 2.0f);
                    avoidScale *= t * t;
                }
                
                vx += avoidVectors[i].x * avoidScale;
                vy += avoidVectors[i].y * avoidScale;
            }
            
            // Apply movement with wall sliding (but allow falling through air)
            float newX = m->x + vx * dt;
            float newY = m->y + vy * dt;
            int mz = (int)m->z;
            
            // For z-level transitions, check if target cell is a ladder (walkable on both levels)
            bool targetIsLadderTransition = (target.z != mz);
            
            if (useWallSliding) {
                int newCellX = (int)(newX / CELL_SIZE);
                int newCellY = (int)(newY / CELL_SIZE);
                
                // Check walkability - for ladder transitions, also accept if it's a ladder on target z
                bool canMove = IsCellWalkableAt(mz, newCellY, newCellX);
                if (!canMove && targetIsLadderTransition) {
                    // Moving toward a ladder on different z-level
                    // Allow if the cell is a ladder on the target z-level
                    canMove = IsLadderCell(grid[target.z][newCellY][newCellX]);
                }
                
                if (canMove) {
                    // Normal movement
                    m->x = newX;
                    m->y = newY;
                } else if (IsCellAirAt(mz, newCellY, newCellX)) {
                    // Moving into air - allow it, then fall
                    m->x = newX;
                    m->y = newY;
                    TryFallToGround(m, newCellX, newCellY);
                } else {
                    // Wall or out of bounds - try sliding
                    int xOnlyCellY = (int)(m->y / CELL_SIZE);
                    int yOnlyCellX = (int)(m->x / CELL_SIZE);
                    bool xOnlyOk = IsCellWalkableAt(mz, xOnlyCellY, newCellX);
                    bool yOnlyOk = IsCellWalkableAt(mz, newCellY, yOnlyCellX);
                    
                    if (xOnlyOk && yOnlyOk) {
                        if (fabsf(vx) > fabsf(vy)) {
                            m->x = newX;
                        } else {
                            m->y = newY;
                        }
                    } else if (xOnlyOk) {
                        m->x = newX;
                    } else if (yOnlyOk) {
                        m->y = newY;
                    }
                }
            } else {
                m->x = newX;
                m->y = newY;
            }
            
            // Track progress for stuck detection
            float dx = m->x - m->lastX;
            float dy = m->y - m->lastY;
            float movedDistSq = dx * dx + dy * dy;
            
            if (movedDistSq >= STUCK_MIN_DISTANCE * STUCK_MIN_DISTANCE) {
                // Made progress, reset timer and update last position
                m->timeWithoutProgress = 0.0f;
                m->lastX = m->x;
                m->lastY = m->y;
            } else {
                // No significant movement, accumulate stuck time
                m->timeWithoutProgress += dt;
                
                // If stuck too long, trigger repath
                if (m->timeWithoutProgress > STUCK_REPATH_TIME) {
                    m->needsRepath = true;
                    m->timeWithoutProgress = 0.0f;
                    m->lastX = m->x;
                    m->lastY = m->y;
                }
            }
        }
    }
    PROFILE_END(Move);
}

void ProcessMoverRepaths(void) {
    int repathsThisFrame = 0;

    for (int i = 0; i < moverCount && repathsThisFrame < MAX_REPATHS_PER_FRAME; i++) {
        Mover* m = &movers[i];
        if (!m->active || !m->needsRepath) continue;

        if (m->repathCooldown > 0) {
            m->repathCooldown--;
            continue;
        }

        int currentX = (int)(m->x / CELL_SIZE);
        int currentY = (int)(m->y / CELL_SIZE);
        int currentZ = (int)m->z;

        Point start = {currentX, currentY, currentZ};
        Point tempPath[MAX_PATH];
        int len = FindPath(moverPathAlgorithm, start, m->goal, tempPath, MAX_PATH);

        m->pathLength = (len > MAX_MOVER_PATH) ? MAX_MOVER_PATH : len;
        // Path is stored goal-to-start: path[0]=goal, path[pathLen-1]=start
        // If truncating, keep the START end (high indices), not the goal end
        int srcOffset = len - m->pathLength;
        for (int j = 0; j < m->pathLength; j++) {
            m->path[j] = tempPath[srcOffset + j];
        }

        if (m->pathLength == 0) {
            // Repath failed - goal may be unreachable, retry after cooldown
            m->pathIndex = -1;
            m->needsRepath = true;  // Keep trying
            if (useRandomizedCooldowns) {
                m->repathCooldown = TICK_RATE + (rand() % TICK_RATE);
            } else {
                m->repathCooldown = REPATH_COOLDOWN_FRAMES;
            }
            repathsThisFrame++;
            continue;
        }

        if (useStringPulling && m->pathLength > 2) {
            StringPullPath(m->path, &m->pathLength);
        }

        m->pathIndex = m->pathLength - 1;
        m->needsRepath = false;
        m->repathCooldown = REPATH_COOLDOWN_FRAMES;

        repathsThisFrame++;
    }
}

void Tick(void) {
    // Update HPA* graph if any chunks are dirty (do this first, before any pathfinding)
    if (moverPathAlgorithm == PATH_ALGO_HPA && hpaNeedsRebuild) {
        UpdateDirtyChunks();
    }
    
    PROFILE_BEGIN(Grid);
    BuildMoverSpatialGrid();
    PROFILE_END(Grid);
    
    PROFILE_BEGIN(Repath);
    ProcessMoverRepaths();
    PROFILE_END(Repath);
    
    UpdateMovers();  // Already profiled internally (LOS, Avoid, Move)
    
    currentTick++;
}

void RunTicks(int count) {
    for (int i = 0; i < count; i++) {
        Tick();
    }
}
