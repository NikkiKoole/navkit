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
bool useMoverAvoidance = false;
float avoidStrengthOpen = 0.5f;
float avoidStrengthClosed = 0.0f;

// Spatial grid
MoverSpatialGrid moverGrid = {0};
double moverGridBuildTimeMs = 0.0;

static inline int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
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
            if (grid[cy][cx] == CELL_WALL) {
                return false;
            }
        }
    }
    
    return true;
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
        if (grid[y][x] == CELL_WALL) return false;
        if (x == x1 && y == y1) return true;

        int e2 = 2 * err;

        // For diagonal movement, check corner cutting
        if (e2 > -dy && e2 < dx) {
            // Moving diagonally - check both adjacent cells
            int nx = x + sx;
            int ny = y + sy;
            if (grid[y][nx] == CELL_WALL || grid[ny][x] == CELL_WALL) {
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
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight &&
            grid[ny][nx] == CELL_WALKABLE) {
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
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight &&
            grid[ny][nx] == CELL_WALKABLE) {
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

void InitMover(Mover* m, float x, float y, Point goal, float speed) {
    m->x = x;
    m->y = y;
    m->goal = goal;
    m->speed = speed;
    m->active = true;
    m->needsRepath = false;
    m->repathCooldown = 0;
    m->pathLength = 0;
    m->pathIndex = -1;
}

void InitMoverWithPath(Mover* m, float x, float y, Point goal, float speed, Point* pathArr, int pathLen) {
    InitMover(m, x, y, goal, speed);
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

    startPos = (Point){currentX, currentY};
    goalPos = newGoal;
    RunHPAStar();

    m->pathLength = (pathLength > MAX_MOVER_PATH) ? MAX_MOVER_PATH : pathLength;
    // Path is stored goal-to-start: path[0]=goal, path[pathLen-1]=start
    // If truncating, keep the START end (high indices), not the goal end
    int srcOffset = pathLength - m->pathLength;
    for (int j = 0; j < m->pathLength; j++) {
        m->path[j] = path[srcOffset + j];
    }

    if (useStringPulling && m->pathLength > 2) {
        StringPullPath(m->path, &m->pathLength);
    }

    m->pathIndex = m->pathLength - 1;
    m->needsRepath = false;

    startPos = (Point){-1, -1};
    goalPos = (Point){-1, -1};
    pathLength = 0;
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

        // Check if mover is standing on a wall - push to nearest walkable
        if (grid[currentY][currentX] == CELL_WALL) {
            int dx[] = {0, 0, -1, 1};
            int dy[] = {-1, 1, 0, 0};
            bool pushed = false;
            for (int d = 0; d < 4; d++) {
                int nx = currentX + dx[d];
                int ny = currentY + dy[d];
                if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight &&
                    grid[ny][nx] == CELL_WALKABLE) {
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

        if (dist < m->speed * dt) {
            m->x = tx;
            m->y = ty;
            m->pathIndex--;
        } else {
            float invDist = 1.0f / dist;
            
            // Base velocity toward waypoint
            float vx = dxf * invDist * m->speed;
            float vy = dyf * invDist * m->speed;
            
            // Add avoidance if enabled
            if (useMoverAvoidance) {
                bool inOpen = IsMoverInOpenArea(m->x, m->y);
                float strength = inOpen ? avoidStrengthOpen : avoidStrengthClosed;
                float avoidScale = m->speed * strength;
                
                if (avoidScale > 0.0f) {
                    Vec2 avoid = ComputeMoverAvoidance(i);
                    vx += avoid.x * avoidScale;
                    vy += avoid.y * avoidScale;
                }
            }
            
            m->x += vx * dt;
            m->y += vy * dt;
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

        if (hpaNeedsRebuild) {
            UpdateDirtyChunks();
        }

        startPos = (Point){currentX, currentY};
        goalPos = m->goal;
        RunHPAStar();

        m->pathLength = (pathLength > MAX_MOVER_PATH) ? MAX_MOVER_PATH : pathLength;
        // Path is stored goal-to-start: path[0]=goal, path[pathLen-1]=start
        // If truncating, keep the START end (high indices), not the goal end
        int srcOffset = pathLength - m->pathLength;
        for (int j = 0; j < m->pathLength; j++) {
            m->path[j] = path[srcOffset + j];
        }

        if (m->pathLength == 0) {
            // Repath failed - goal may be unreachable, clear path and let UpdateMovers assign new goal
            m->pathIndex = -1;
            m->needsRepath = false;
            m->repathCooldown = TICK_RATE;  // Wait 1 second before trying new goal
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

    startPos = (Point){-1, -1};
    goalPos = (Point){-1, -1};
    pathLength = 0;
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
