#include "mover.h"
#include "grid.h"
#include "pathfinding.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Globals
Mover movers[MAX_MOVERS];
int moverCount = 0;
unsigned long currentTick = 0;
bool useStringPulling = true;
bool endlessMoverMode = false;
bool useMoverAvoidance = true;
bool useKnotFix = true;
bool useWallRepulsion = true;
float wallRepulsionStrength = 0.5f;
bool useWallSliding = true;
float avoidStrengthOpen = 0.5f;
float avoidStrengthClosed = 0.0f;
bool useDirectionalAvoidance = true;

// Spatial grid
MoverSpatialGrid moverGrid = {0};
double moverGridBuildTimeMs = 0.0;

static inline int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Helper to check if a cell is walkable
static inline bool IsCellWalkableAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    CellType cell = grid[z][y][x];
    return cell == CELL_WALKABLE || cell == CELL_FLOOR || cell == CELL_LADDER;
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
    
    clock_t start = clock();
    
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
    
    clock_t end = clock();
    moverGridBuildTimeMs = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
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
                
                float invDist = 1.0f / sqrtf(distSq);
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

bool IsMoverInOpenArea(float x, float y) {
    // Convert pixel position to grid cell
    int cellX = (int)(x / CELL_SIZE);
    int cellY = (int)(y / CELL_SIZE);
    
    // Check 3x3 area around the mover's cell
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cx = cellX + dx;
            int cy = cellY + dy;
            
            // Out of bounds = not open
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) {
                return false;
            }
            
            // Wall = not open
            if (grid[0][cy][cx] == CELL_WALL) {
                return false;
            }
        }
    }
    
    return true;
}

// Check if there's clearance in a direction
// Checks a 3x2 strip: current cell + neighbor in that direction, for 3 rows
// dir: 0=up (-y), 1=right (+x), 2=down (+y), 3=left (-x)
bool HasClearanceInDirection(float x, float y, int dir) {
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
        
        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) {
            return false;
        }
        if (grid[0][cy][cx] == CELL_WALL) {
            return false;
        }
    }
    
    return true;
}

// Compute wall repulsion force - pushes mover away from nearby walls
Vec2 ComputeWallRepulsion(float x, float y) {
    Vec2 repulsion = {0.0f, 0.0f};
    
    int cellX = (int)(x / CELL_SIZE);
    int cellY = (int)(y / CELL_SIZE);
    
    // Check 3x3 area around mover for walls
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cx = cellX + dx;
            int cy = cellY + dy;
            
            // Skip out of bounds
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) continue;
            
            // Only care about walls
            if (grid[0][cy][cx] != CELL_WALL) continue;
            
            // Wall cell center
            float wallX = cx * CELL_SIZE + CELL_SIZE * 0.5f;
            float wallY = cy * CELL_SIZE + CELL_SIZE * 0.5f;
            
            // Vector from wall to mover
            float dirX = x - wallX;
            float dirY = y - wallY;
            float distSq = dirX * dirX + dirY * dirY;
            
            if (distSq < 1e-10f) continue;
            
            float dist = sqrtf(distSq);
            
            // Only repel within radius
            if (dist >= WALL_REPULSION_RADIUS) continue;
            
            // Strength increases as mover gets closer (quadratic falloff)
            float t = 1.0f - dist / WALL_REPULSION_RADIUS;
            float strength = t * t;
            
            // Add repulsion (normalized direction * strength)
            float invDist = 1.0f / dist;
            repulsion.x += dirX * invDist * strength;
            repulsion.y += dirY * invDist * strength;
        }
    }
    
    return repulsion;
}

Vec2 FilterAvoidanceByWalls(float x, float y, Vec2 avoidance) {
    Vec2 result = avoidance;
    
    // Check if avoidance pushes in a direction without clearance
    // and zero that component if so
    
    if (avoidance.x > 0.01f) {
        // Pushing right
        if (!HasClearanceInDirection(x, y, 1)) {
            result.x = 0.0f;
        }
    } else if (avoidance.x < -0.01f) {
        // Pushing left
        if (!HasClearanceInDirection(x, y, 3)) {
            result.x = 0.0f;
        }
    }
    
    if (avoidance.y > 0.01f) {
        // Pushing down
        if (!HasClearanceInDirection(x, y, 2)) {
            result.y = 0.0f;
        }
    } else if (avoidance.y < -0.01f) {
        // Pushing up
        if (!HasClearanceInDirection(x, y, 0)) {
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
bool HasLineOfSight(int x0, int y0, int x1, int y1) {
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
        if (grid[0][y][x] == CELL_WALL) return false;
        if (x == x1 && y == y1) return true;

        int e2 = 2 * err;

        // For diagonal movement, check corner cutting
        if (e2 > -dy && e2 < dx) {
            // Moving diagonally - check both adjacent cells
            int nx = x + sx;
            int ny = y + sy;
            if (grid[0][y][nx] == CELL_WALL || grid[0][ny][x] == CELL_WALL) {
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
static bool HasClearCorridor(int x0, int y0, int x1, int y1) {
    // First check the center line
    if (!HasLineOfSight(x0, y0, x1, y1)) return false;
    
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
        if (IsCellWalkableAt(0, ny, nx)) {
            if (!HasLineOfSight(nx, ny, x1, y1)) return false;
        }
    }
    
    return true;
}

// Lenient LOS check for runtime - returns true if LOS exists from current cell
// OR any cardinal neighbor. Matches the corridor check used in string pulling.
static bool HasLineOfSightLenient(int x0, int y0, int x1, int y1) {
    // First try from current position
    if (HasLineOfSight(x0, y0, x1, y1)) return true;
    
    // Try from all 4 cardinal neighbors - if ANY has LOS, we're okay
    int ndx[] = {0, 0, -1, 1};
    int ndy[] = {-1, 1, 0, 0};
    
    for (int i = 0; i < 4; i++) {
        int nx = x0 + ndx[i];
        int ny = y0 + ndy[i];
        if (IsCellWalkableAt(0, ny, nx)) {
            if (HasLineOfSight(nx, ny, x1, y1)) return true;
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
            if (HasClearCorridor(pathArr[current].x, pathArr[current].y,
                                 pathArr[i].x, pathArr[i].y)) {
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
    Point newGoal = GetRandomWalkableCell();
    m->goal = newGoal;

    int currentX = (int)(m->x / CELL_SIZE);
    int currentY = (int)(m->y / CELL_SIZE);
    int currentZ = (int)m->z;
    Point start = {currentX, currentY, currentZ};

    Point tempPath[MAX_PATH];
    int len = FindPathHPA(start, newGoal, tempPath, MAX_PATH);

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
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        
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
                    // No path found, wait 1 second before trying again
                    m->repathCooldown = TICK_RATE;
                }
            } else {
                m->active = false;
            }
            continue;
        }

        int currentX = (int)(m->x / CELL_SIZE);
        int currentY = (int)(m->y / CELL_SIZE);
        int currentZ = (int)m->z;

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

        // Check line-of-sight to next waypoint (lenient - also checks from neighbors)
        // This detects when a wall is placed across the mover's path
        if (!HasLineOfSightLenient(currentX, currentY, target.x, target.y)) {
            m->needsRepath = true;
            continue;
        }

        float tx = target.x * CELL_SIZE + CELL_SIZE * 0.5f;
        float ty = target.y * CELL_SIZE + CELL_SIZE * 0.5f;

        float dxf = tx - m->x;
        float dyf = ty - m->y;
        float dist = sqrtf(dxf*dxf + dyf*dyf);

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
            // Only update z-level when on a ladder cell (z-transitions require ladders)
            if (target.z != (int)m->z) {
                // Changing z-level - must be on a ladder
                int cellX = (int)(m->x / CELL_SIZE);
                int cellY = (int)(m->y / CELL_SIZE);
                int cellZ = (int)m->z;
                if (grid[cellZ][cellY][cellX] == CELL_LADDER && 
                    grid[target.z][cellY][cellX] == CELL_LADDER) {
                    m->z = (float)target.z;
                }
                // If not on ladder, don't change z - path may be stale
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
            
            // Add avoidance if enabled
            if (useMoverAvoidance) {
                Vec2 avoid = ComputeMoverAvoidance(i);
                
                if (useDirectionalAvoidance) {
                    // Filter avoidance by directional wall clearance
                    avoid = FilterAvoidanceByWalls(m->x, m->y, avoid);
                    float avoidScale = m->speed * avoidStrengthOpen;
                    
                    // Knot fix: reduce avoidance near waypoint so mover can reach it
                    if (useKnotFix && dist < KNOT_FIX_ARRIVAL_RADIUS * 2.0f) {
                        float t = dist / (KNOT_FIX_ARRIVAL_RADIUS * 2.0f);  // 0 at waypoint, 1 at edge
                        avoidScale *= t * t;  // Quadratic falloff
                    }
                    
                    vx += avoid.x * avoidScale;
                    vy += avoid.y * avoidScale;
                } else {
                    // Original open/closed area check
                    bool inOpen = IsMoverInOpenArea(m->x, m->y);
                    float strength = inOpen ? avoidStrengthOpen : avoidStrengthClosed;
                    float avoidScale = m->speed * strength;
                    
                    // Knot fix: reduce avoidance near waypoint so mover can reach it
                    if (useKnotFix && dist < KNOT_FIX_ARRIVAL_RADIUS * 2.0f) {
                        float t = dist / (KNOT_FIX_ARRIVAL_RADIUS * 2.0f);  // 0 at waypoint, 1 at edge
                        avoidScale *= t * t;  // Quadratic falloff
                    }
                    
                    if (avoidScale > 0.0f) {
                        vx += avoid.x * avoidScale;
                        vy += avoid.y * avoidScale;
                    }
                }
            }
            
            // Add wall repulsion if enabled
            if (useWallRepulsion) {
                Vec2 wallRepel = ComputeWallRepulsion(m->x, m->y);
                float repelScale = m->speed * wallRepulsionStrength;
                vx += wallRepel.x * repelScale;
                vy += wallRepel.y * repelScale;
            }
            
            // Apply movement with optional wall sliding
            float newX = m->x + vx * dt;
            float newY = m->y + vy * dt;
            
            if (useWallSliding) {
                int newCellX = (int)(newX / CELL_SIZE);
                int newCellY = (int)(newY / CELL_SIZE);
                
                // Check if new position would be in a wall
                bool newPosInWall = (newCellX < 0 || newCellX >= gridWidth ||
                                     newCellY < 0 || newCellY >= gridHeight ||
                                     grid[0][newCellY][newCellX] == CELL_WALL);
                
                if (newPosInWall) {
                    // Try sliding: move only in X
                    int xOnlyCellX = (int)(newX / CELL_SIZE);
                    int xOnlyCellY = (int)(m->y / CELL_SIZE);
                    bool xOnlyOk = (xOnlyCellX >= 0 && xOnlyCellX < gridWidth &&
                                    xOnlyCellY >= 0 && xOnlyCellY < gridHeight &&
                                    grid[0][xOnlyCellY][xOnlyCellX] != CELL_WALL);
                    
                    // Try sliding: move only in Y
                    int yOnlyCellX = (int)(m->x / CELL_SIZE);
                    int yOnlyCellY = (int)(newY / CELL_SIZE);
                    bool yOnlyOk = (yOnlyCellX >= 0 && yOnlyCellX < gridWidth &&
                                    yOnlyCellY >= 0 && yOnlyCellY < gridHeight &&
                                    grid[0][yOnlyCellY][yOnlyCellX] != CELL_WALL);
                    
                    if (xOnlyOk && yOnlyOk) {
                        // Both work - pick the one more aligned with velocity
                        if (fabsf(vx) > fabsf(vy)) {
                            m->x = newX;
                        } else {
                            m->y = newY;
                        }
                    } else if (xOnlyOk) {
                        m->x = newX;  // Slide horizontally
                    } else if (yOnlyOk) {
                        m->y = newY;  // Slide vertically
                    }
                    // else: both blocked, don't move
                } else {
                    m->x = newX;
                    m->y = newY;
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

        if (hpaNeedsRebuild) {
            UpdateDirtyChunks();
        }

        Point start = {currentX, currentY, currentZ};
        Point tempPath[MAX_PATH];
        int len = FindPathHPA(start, m->goal, tempPath, MAX_PATH);

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
            m->repathCooldown = TICK_RATE;  // Wait 1 second before retry
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
    BuildMoverSpatialGrid();
    ProcessMoverRepaths();
    UpdateMovers();
    currentTick++;
}

void RunTicks(int count) {
    for (int i = 0; i < count; i++) {
        Tick();
    }
}
