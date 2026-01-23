#include "../vendor/raylib.h"
#include "grid.h"
#include "terrain.h"
#include "pathfinding.h"
#include "mover.h"
#include "items.h"
#include "jobs.h"
#include "stockpiles.h"
#define PROFILER_IMPLEMENTATION
#include "../shared/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define UI_IMPLEMENTATION
#include "../shared/ui.h"
#include "../assets/atlas.h"

#define MAX_AGENTS  50

// Item rendering sizes (fraction of CELL_SIZE)
#define ITEM_SIZE_GROUND    0.6f
#define ITEM_SIZE_CARRIED   0.5f
#define ITEM_SIZE_STOCKPILE 0.6f

// Mover rendering
#define MOVER_SIZE          0.75f

float zoom = 1.0f;
Vector2 offset = {0, 0};
Texture2D atlas;
bool showGraph = false;
bool showEntrances = false;
int currentViewZ = 0;  // Which z-level we're viewing

// Room drawing state (R key to activate, drag to draw)
bool drawingRoom = false;
int roomStartX = 0, roomStartY = 0;

// Floor drawing state (F key to activate, drag to draw)
bool drawingFloor = false;
int floorStartX = 0, floorStartY = 0;

// Pathfinding settings
int pathAlgorithm = 1;  // Default to HPA*
const char* algorithmNames[] = {"A*", "HPA*", "JPS", "JPS+"};
int currentDirection = 1;  // 0 = 4-dir, 1 = 8-dir
const char* directionNames[] = {"4-dir", "8-dir"};

// Tool selection: 0=Draw Walls, 1=Erase Walls, 2=Set Start, 3=Set Goal
int currentTool = 0;
const char* toolNames[] = {"Draw Wall", "Draw Floor", "Draw Ladder", "Erase", "Set Start", "Set Goal"};

// Terrain selection
int currentTerrain = 0;
const char* terrainNames[] = {"Clear", "Sparse", "City", "Mixed", "Perlin", "Maze", "Dungeon", "Caves", "Drunkard", "Tunneler", "MixMax", "NarrowGaps", "Towers3D", "GalleryFlat", "Castle", "Labyrinth3D", "Spiral3D", "Council"};

// UI section collapse state
bool sectionView = false;
bool sectionPathfinding = false;
bool sectionMapEditing = true;
bool sectionAgents = false;
bool sectionMovers = false;
bool sectionMoverAvoidance = false;
bool sectionMoverWalls = false;
bool sectionMoverDebug = false;

bool sectionProfiler = false;
bool sectionMemory = false;
bool sectionJobs = true;
bool paused = false;
bool showItems = true;

// Stockpile hover state
int hoveredStockpile = -1;  // Index of stockpile under mouse, -1 if none

// Mover hover state (only when paused)
int hoveredMover = -1;  // Index of mover under mouse, -1 if none

// Test map: Narrow gaps (from test_mover.c)
const char* narrowGapsMap =
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "................................\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "#########.#############.#####.##\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#...............#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "###.#######.##########.####.####\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#...............#.......\n"
    "........#.......#.......#.......\n"
    "................#.......#.......\n"
    "........#.......#...............\n"
    "........#.......#.......#.......\n"
    "#.#########.#######.#########.##\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n"
    "........#.......#.......#.......\n";

// Agents
int agentCountSetting = 10;

// Multi-agent paths
typedef struct {
    Point start;
    Point goal;
    Point path[MAX_PATH];
    int pathLength;
    Color color;
    bool active;
} Agent;

Agent agents[MAX_AGENTS];
int agentCount = 0;

// Mover UI settings (movers themselves are in mover.c)
int moverCountSetting = 10;
int itemCountSetting = 10;
bool showMovers = true;
bool showMoverPaths = false;
bool showNeighborCounts = false;
bool showOpenArea = false;
bool showKnotDetection = false;  // Highlight movers stuck near waypoints
bool showStuckDetection = false; // Highlight movers not making progress
bool cullDrawing = true;  // Toggle view frustum culling

// Extended mover struct for rendering (adds Color)
typedef struct {
    Color color;
} MoverRenderData;
MoverRenderData moverRenderData[MAX_MOVERS];

static int GetCellSprite(CellType cell) {
    switch (cell) {
        case CELL_WALKABLE:     return SPRITE_grass;
        case CELL_WALL:         return SPRITE_wall;
        case CELL_LADDER:       return SPRITE_ladder;  // Legacy: same as BOTH
        case CELL_LADDER_BOTH:  return SPRITE_ladder;
        case CELL_LADDER_UP:    return SPRITE_ladder_up;
        case CELL_LADDER_DOWN:  return SPRITE_ladder_down;
        case CELL_FLOOR:        return SPRITE_floor;
        case CELL_AIR:          return SPRITE_air;
    }
    return SPRITE_grass;
}

void DrawCellGrid(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX = 0, minY = 0;
    int maxX = gridWidth, maxY = gridHeight;

    // Calculate visible cell range (view frustum culling)
    if (cullDrawing) {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        minX = (int)((-offset.x) / size);
        maxX = (int)((-offset.x + screenW) / size) + 1;
        minY = (int)((-offset.y) / size);
        maxY = (int)((-offset.y + screenH) / size) + 1;

        // Clamp to grid bounds
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX > gridWidth) maxX = gridWidth;
        if (maxY > gridHeight) maxY = gridHeight;
    }

    // Draw layer below with transparency (if viewing z > 0)
    if (z > 0) {
        Color tint = (Color){255, 255, 255, 128};
        int zBelow = z - 1;
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                CellType cell = grid[zBelow][y][x];
                if (cell == CELL_AIR) continue;  // Don't draw air from below
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(GetCellSprite(cell));
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
            }
        }
    }

    // Draw current layer
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            Rectangle src = SpriteGetRect(GetCellSprite(grid[z][y][x]));
            DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, WHITE);
        }
    }
}

void DrawChunkBoundaries(void) {
    float cellSize = CELL_SIZE * zoom;
    float chunkPixelsX = chunkWidth * cellSize;
    float chunkPixelsY = chunkHeight * cellSize;
    for (int cy = 0; cy <= chunksY; cy++) {
        Vector2 s = {offset.x, offset.y + cy * chunkPixelsY};
        Vector2 e = {offset.x + chunksX * chunkPixelsX, offset.y + cy * chunkPixelsY};
        DrawLineEx(s, e, 3.0f, RED);
    }
    for (int cx = 0; cx <= chunksX; cx++) {
        Vector2 s = {offset.x + cx * chunkPixelsX, offset.y};
        Vector2 e = {offset.x + cx * chunkPixelsX, offset.y + chunksY * chunkPixelsY};
        DrawLineEx(s, e, 3.0f, RED);
    }
}

void DrawEntrances(void) {
    float size = CELL_SIZE * zoom;
    float ms = size * 0.5f;
    for (int i = 0; i < entranceCount; i++) {
        float px = offset.x + entrances[i].x * size + (size - ms) / 2;
        float py = offset.y + entrances[i].y * size + (size - ms) / 2;
        DrawRectangle((int)px, (int)py, (int)ms, (int)ms, WHITE);
    }
}

void DrawGraph(void) {
    if (!showGraph) return;
    float size = CELL_SIZE * zoom;
    for (int i = 0; i < graphEdgeCount; i += 2) {
        int e1 = graphEdges[i].from, e2 = graphEdges[i].to;
        Vector2 p1 = {offset.x + entrances[e1].x * size + size/2, offset.y + entrances[e1].y * size + size/2};
        Vector2 p2 = {offset.x + entrances[e2].x * size + size/2, offset.y + entrances[e2].y * size + size/2};
        DrawLineEx(p1, p2, 2.0f, YELLOW);
    }
}

void DrawPath(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    // Draw start (green) - full opacity on same z, faded on different z
    if (startPos.x >= 0) {
        Color col = (startPos.z == z) ? GREEN : (Color){0, 228, 48, 80};
        DrawRectangle((int)(offset.x + startPos.x * size), (int)(offset.y + startPos.y * size), (int)size, (int)size, col);
    }

    // Draw goal (red) - full opacity on same z, faded on different z
    if (goalPos.x >= 0) {
        Color col = (goalPos.z == z) ? RED : (Color){230, 41, 55, 80};
        DrawRectangle((int)(offset.x + goalPos.x * size), (int)(offset.y + goalPos.y * size), (int)size, (int)size, col);
    }

    // Draw path - full opacity on same z, faded on different z
    for (int i = 0; i < pathLength; i++) {
        float px = offset.x + path[i].x * size + size * 0.25f;
        float py = offset.y + path[i].y * size + size * 0.25f;
        Color col = (path[i].z == z) ? BLUE : (Color){0, 121, 241, 80};
        DrawRectangle((int)px, (int)py, (int)(size * 0.5f), (int)(size * 0.5f), col);
    }
}

Color GetRandomColor(void) {
    return (Color){
        GetRandomValue(100, 255),
        GetRandomValue(100, 255),
        GetRandomValue(100, 255),
        255
    };
}

void SpawnAgents(int count) {
    double startTime = GetTime();
    agentCount = 0;
    for (int i = 0; i < count && i < MAX_AGENTS; i++) {
        Agent* a = &agents[agentCount];
        a->start = GetRandomWalkableCell();
        a->goal = GetRandomWalkableCell();
        a->color = GetRandomColor();

        // Run pathfinding with selected algorithm
        startPos = a->start;
        goalPos = a->goal;
        switch (pathAlgorithm) {
            case 0: RunAStar(); break;
            case 1: RunHPAStar(); break;
            case 2: RunJPS(); break;
            case 3: RunJpsPlus(); break;
        }

        // Copy path to agent
        a->pathLength = pathLength;
        for (int j = 0; j < pathLength; j++) {
            a->path[j] = path[j];
        }
        a->active = (pathLength > 0);
        agentCount++;
    }

    // Clear global path
    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;

    double totalTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "SpawnAgents: %d agents in %.2fms (avg %.2fms per agent)", count, totalTime, totalTime / count);
}

void RepathAgents(void) {
    if (agentCount == 0) return;

    double startTime = GetTime();
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];

        // Run pathfinding with selected algorithm (keep same start/goal)
        startPos = a->start;
        goalPos = a->goal;
        switch (pathAlgorithm) {
            case 0: RunAStar(); break;
            case 1: RunHPAStar(); break;
            case 2: RunJPS(); break;
            case 3: RunJpsPlus(); break;
        }

        // Copy path to agent
        a->pathLength = pathLength;
        for (int j = 0; j < pathLength; j++) {
            a->path[j] = path[j];
        }
        a->active = (pathLength > 0);
    }

    // Clear global path
    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;

    double totalTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "RepathAgents: %d agents in %.2fms (avg %.2fms per agent)", agentCount, totalTime, totalTime / agentCount);
}

void DrawAgents(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];
        if (!a->active) continue;

        // Draw start (circle) - only if on current z-level
        if (a->start.z == z) {
            float sx = offset.x + a->start.x * size + size / 2;
            float sy = offset.y + a->start.y * size + size / 2;
            DrawCircle((int)sx, (int)sy, size * 0.4f, a->color);
        }

        // Draw goal (square outline) - only if on current z-level
        if (a->goal.z == z) {
            float gx = offset.x + a->goal.x * size;
            float gy = offset.y + a->goal.y * size;
            DrawRectangleLines((int)gx, (int)gy, (int)size, (int)size, a->color);
        }

        // Draw path - only segments on current z-level
        for (int j = 0; j < a->pathLength; j++) {
            if (a->path[j].z != z) continue;
            float px = offset.x + a->path[j].x * size + size * 0.35f;
            float py = offset.y + a->path[j].y * size + size * 0.35f;
            DrawRectangle((int)px, (int)py, (int)(size * 0.3f), (int)(size * 0.3f), a->color);
        }
    }
}

// ============== MOVERS ==============

void AddMoversDemo(int count) {
    // Sync mover algorithm with UI selection
    moverPathAlgorithm = (PathAlgorithm)pathAlgorithm;

    // Ensure HPA* graph is built if using HPA*
    if (pathAlgorithm == 1 && graphEdgeCount == 0) {
        BuildEntrances();
        BuildGraph();
    }

    for (int i = 0; i < count && moverCount < MAX_MOVERS; i++) {
        Mover* m = &movers[moverCount];

        Point start = GetRandomWalkableCell();
        Point goal = GetRandomWalkableCell();

        float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
        float z = (float)start.z;
        float speed = MOVER_SPEED + GetRandomValue(-30, 30);

        startPos = start;
        goalPos = goal;
        switch (pathAlgorithm) {
            case 0: RunAStar(); break;
            case 1: RunHPAStar(); break;
            case 2: RunJPS(); break;
            case 3: RunJpsPlus(); break;
        }

        if (pathLength > 0) {
            InitMoverWithPath(m, x, y, z, goal, speed, path, pathLength);
            if (useStringPulling && m->pathLength > 2) {
                StringPullPath(m->path, &m->pathLength);
                m->pathIndex = m->pathLength - 1;
            }
        } else {
            InitMover(m, x, y, z, goal, speed);
        }

        moverRenderData[moverCount].color = GetRandomColor();
        moverCount++;
    }

    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;
}

void SpawnMoversDemo(int count) {
    double startTime = GetTime();

    // Sync mover algorithm with UI selection
    moverPathAlgorithm = (PathAlgorithm)pathAlgorithm;

    // Ensure HPA* graph is built if using HPA*
    if (pathAlgorithm == 1 && graphEdgeCount == 0) {
        BuildEntrances();
        BuildGraph();
    }

    ClearMovers();
    for (int i = 0; i < count && i < MAX_MOVERS; i++) {
        Mover* m = &movers[moverCount];

        Point start = GetRandomWalkableCell();
        Point goal = GetRandomWalkableCell();

        // Initial position (center of start cell)
        float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
        float z = (float)start.z;
        float speed = MOVER_SPEED + GetRandomValue(-30, 30);

        // Compute initial path using selected algorithm
        startPos = start;
        goalPos = goal;
        switch (pathAlgorithm) {
            case 0: RunAStar(); break;
            case 1: RunHPAStar(); break;
            case 2: RunJPS(); break;
            case 3: RunJpsPlus(); break;
        }

        if (pathLength > 0) {
            InitMoverWithPath(m, x, y, z, goal, speed, path, pathLength);

            // Apply string pulling to smooth path
            if (useStringPulling && m->pathLength > 2) {
                StringPullPath(m->path, &m->pathLength);
                m->pathIndex = m->pathLength - 1;
            }

        } else {
            // No path found, spawn anyway - endless mode will assign a new goal
            InitMover(m, x, y, z, goal, speed);
            TraceLog(LOG_WARNING, "Mover %d spawned without path: (%d,%d,%d) to (%d,%d,%d)", moverCount, start.x, start.y, start.z, goal.x, goal.y, goal.z);
        }

        // Store render data (color)
        moverRenderData[moverCount].color = GetRandomColor();
        moverCount++;
    }

    // Clear global state
    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;

    double elapsed = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "SpawnMovers: %d movers in %.2fms", moverCount, elapsed);
}

void DrawMovers(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;

        // Only draw movers on the current z-level
        if ((int)m->z != viewZ) continue;

        // Screen position
        float sx = offset.x + m->x * zoom;
        float sy = offset.y + m->y * zoom;

        // Choose color based on mover state or debug mode
        Color moverColor;
        if (showStuckDetection) {
            // Color by how long mover hasn't made progress
            if (m->timeWithoutProgress > STUCK_REPATH_TIME) {
                moverColor = MAGENTA;    // Completely stuck, needs intervention
            } else if (m->timeWithoutProgress > STUCK_REPATH_TIME * 0.5f) {
                moverColor = RED;        // Very stuck
            } else if (m->timeWithoutProgress > STUCK_CHECK_INTERVAL) {
                moverColor = ORANGE;     // Getting stuck
            } else {
                moverColor = GREEN;      // Moving normally
            }
        } else if (showKnotDetection) {
            // Color by how long mover has been stuck near waypoint
            if (m->timeNearWaypoint > KNOT_STUCK_TIME) {
                moverColor = RED;        // Stuck! Likely in a knot
            } else if (m->timeNearWaypoint > KNOT_STUCK_TIME * 0.5f) {
                moverColor = ORANGE;     // Getting stuck
            } else if (m->timeNearWaypoint > 0.0f) {
                moverColor = YELLOW;     // Near waypoint, still progressing
            } else {
                moverColor = GREEN;      // Moving normally
            }
        } else if (showOpenArea) {
            // Color by whether mover is in open area (can avoid freely)
            bool open = IsMoverInOpenArea(m->x, m->y, (int)m->z);
            moverColor = open ? SKYBLUE : MAGENTA;
        } else if (showNeighborCounts) {
            // Color by neighbor count (green=0, yellow=few, red=many)
            int neighbors = QueryMoverNeighbors(m->x, m->y, MOVER_AVOID_RADIUS, i, NULL, NULL);
            if (neighbors == 0) {
                moverColor = GREEN;
            } else if (neighbors <= 3) {
                moverColor = YELLOW;
            } else if (neighbors <= 6) {
                moverColor = ORANGE;
            } else {
                moverColor = RED;
            }
        } else if (m->repathCooldown > 0 && m->pathLength == 0) {
            moverColor = ORANGE;  // On cooldown, waiting to retry
        } else if (m->pathLength == 0) {
            moverColor = RED;     // No path
        } else if (m->needsRepath) {
            moverColor = YELLOW;  // Needs repath
        } else {
            moverColor = WHITE;   // Normal, has valid path
        }

        // Override color if mover just fell
        if (m->fallTimer > 0) {
            moverColor = BLUE;
        }

        // Draw mover as head sprite with color tint
        float moverSize = size * MOVER_SIZE;
        Rectangle src = SpriteGetRect(SPRITE_head);
        Rectangle dest = { sx - moverSize/2, sy - moverSize/2, moverSize, moverSize };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, moverColor);

        // Draw carried item above mover's head
        if (m->carryingItem >= 0 && items[m->carryingItem].active) {
            Item* item = &items[m->carryingItem];
            int sprite;
            switch (item->type) {
                case ITEM_RED:   sprite = SPRITE_crate_red;   break;
                case ITEM_GREEN: sprite = SPRITE_crate_green; break;
                case ITEM_BLUE:  sprite = SPRITE_crate_blue;  break;
                default:         sprite = SPRITE_apple;       break;
            }
            float itemSize = size * ITEM_SIZE_CARRIED;
            float itemY = sy - moverSize/2 - itemSize + moverSize * 0.2f;  // 20% overlap with head
            Rectangle itemSrc = SpriteGetRect(sprite);
            Rectangle itemDest = { sx - itemSize/2, itemY, itemSize, itemSize };
            DrawTexturePro(atlas, itemSrc, itemDest, (Vector2){0, 0}, 0, WHITE);
        }
    }

    // Draw mover paths in separate loop for profiling
    if (showMoverPaths) {
        PROFILE_BEGIN(MoverPaths);
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (!m->active || m->pathIndex < 0) continue;
            if ((int)m->z != viewZ) continue;

            float sx = offset.x + m->x * zoom;
            float sy = offset.y + m->y * zoom;
            Color color = moverRenderData[i].color;

            // Line to next waypoint (if on same z)
            Point next = m->path[m->pathIndex];
            if (next.z == viewZ) {
                float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){sx, sy}, (Vector2){tx, ty}, 2.0f, color);
            }

            // Rest of path (only segments where both points are on current z)
            for (int j = m->pathIndex; j > 0; j--) {
                if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
                float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 1.0f, Fade(color, 0.4f));
            }
        }
        PROFILE_END(MoverPaths);
    }
}

void DrawItems(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_ITEMS; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (item->state == ITEM_CARRIED) continue;     // drawn above mover's head instead
        if (item->state == ITEM_IN_STOCKPILE) continue; // drawn as part of stockpile stack

        // Only draw items on the current z-level
        if ((int)item->z != viewZ) continue;

        // Screen position
        float sx = offset.x + item->x * zoom;
        float sy = offset.y + item->y * zoom;

        // Choose sprite based on item type
        int sprite;
        switch (item->type) {
            case ITEM_RED:   sprite = SPRITE_crate_red;   break;
            case ITEM_GREEN: sprite = SPRITE_crate_green; break;
            case ITEM_BLUE:  sprite = SPRITE_crate_blue;  break;
            default:         sprite = SPRITE_apple;       break;
        }

        float itemSize = size * ITEM_SIZE_GROUND;
        Rectangle src = SpriteGetRect(sprite);
        Rectangle dest = { sx - itemSize/2, sy - itemSize/2, itemSize, itemSize };

        // Tint reserved items slightly darker
        Color tint = (item->reservedBy >= 0) ? GRAY : WHITE;
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
    }
}

void DrawStockpiles(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;

        // Only draw stockpiles on the current z-level
        if (sp->z != viewZ) continue;

        // Draw each tile of the stockpile
        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int gx = sp->x + dx;
                int gy = sp->y + dy;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, WHITE);

                // Draw stacked items in this slot
                int slotIdx = dy * sp->width + dx;
                int count = sp->slotCounts[slotIdx];
                if (count > 0) {
                    ItemType type = sp->slotTypes[slotIdx];
                    int sprite;
                    switch (type) {
                        case ITEM_RED:   sprite = SPRITE_crate_red;   break;
                        case ITEM_GREEN: sprite = SPRITE_crate_green; break;
                        case ITEM_BLUE:  sprite = SPRITE_crate_blue;  break;
                        default:         sprite = SPRITE_apple;       break;
                    }

                    // Draw up to 5 visible items with diagonal offset (bottom to top)
                    int visibleCount = count > 5 ? 5 : count;
                    float itemSize = size * ITEM_SIZE_STOCKPILE;
                    float stackOffset = size * 0.08f;  // Offset per item

                    // Draw oldest first (bottom-right), newest last (up-left, on top)
                    for (int s = 0; s < visibleCount; s++) {
                        float itemX = sx + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                        float itemY = sy + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                        Rectangle srcItem = SpriteGetRect(sprite);
                        Rectangle destItem = { itemX, itemY, itemSize, itemSize };
                        DrawTexturePro(atlas, srcItem, destItem, (Vector2){0, 0}, 0, WHITE);
                    }

                    // for (int pos = 0; pos < visibleCount; pos++) {
                    //     float itemX = sx + size * 0.5f - itemSize * 0.5f - pos * stackOffset;
                    //     float itemY = sy + size * 0.5f - itemSize * 0.5f - pos * stackOffset;

                    //     Rectangle srcItem = SpriteGetRect(sprite);
                    //     Rectangle destItem = { itemX, itemY, itemSize, itemSize };
                    //     DrawTexturePro(atlas, srcItem, destItem, (Vector2){0, 0}, 0, WHITE);
                    // }

                }
            }
        }
    }
}

// Helper: spawn a 3x3 stockpile with specified type filters
static void SpawnStockpileWithFilters(bool allowRed, bool allowGreen, bool allowBlue) {
    int attempts = 100;
    while (attempts-- > 0) {
        int gx = rand() % (gridWidth - 3);
        int gy = rand() % (gridHeight - 3);
        bool valid = true;
        for (int dy = 0; dy < 3 && valid; dy++)
            for (int dx = 0; dx < 3 && valid; dx++)
                if (!IsCellWalkableAt(currentViewZ, gy + dy, gx + dx)) valid = false;
        if (valid) {
            int spIdx = CreateStockpile(gx, gy, currentViewZ, 3, 3);
            if (spIdx >= 0) {
                SetStockpileFilter(spIdx, ITEM_RED, allowRed);
                SetStockpileFilter(spIdx, ITEM_GREEN, allowGreen);
                SetStockpileFilter(spIdx, ITEM_BLUE, allowBlue);
            }
            break;
        }
    }
}

Vector2 ScreenToGrid(Vector2 screen) {
    float size = CELL_SIZE * zoom;
    return (Vector2){(screen.x - offset.x) / size, (screen.y - offset.y) / size};
}

Vector2 ScreenToWorld(Vector2 screen) {
    // Convert screen coords to world coords (pixels, not grid cells)
    return (Vector2){(screen.x - offset.x) / zoom, (screen.y - offset.y) / zoom};
}

// Find stockpile at grid position, returns index or -1 if none
int GetStockpileAtGrid(int gx, int gy, int gz) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z != gz) continue;
        if (gx >= sp->x && gx < sp->x + sp->width &&
            gy >= sp->y && gy < sp->y + sp->height) {
            return i;
        }
    }
    return -1;
}

// Draw stockpile tooltip at mouse position
void DrawStockpileTooltip(int spIdx, Vector2 mouse) {
    if (spIdx < 0 || spIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[spIdx];
    if (!sp->active) return;

    // Count items and capacity
    int totalItems = 0;
    int totalSlots = sp->width * sp->height;
    for (int i = 0; i < totalSlots; i++) {
        totalItems += sp->slotCounts[i];
    }
    int maxCapacity = totalSlots * sp->maxStackSize;

    // Build tooltip text
    const char* priorityText = TextFormat("Priority: %d", sp->priority);
    const char* stackText = TextFormat("Stack size: %d", sp->maxStackSize);
    const char* storageText = TextFormat("Storage: %d/%d (%d slots)", totalItems, maxCapacity, totalSlots);
    const char* helpText = "+/- priority, [/] stack, R/G/B filter";

    // Measure text
    int w1 = MeasureText(priorityText, 14);
    int w2 = MeasureText(stackText, 14);
    int w3 = MeasureText(storageText, 14);
    int w4 = MeasureText("Filters: R G B", 14);
    int w5 = MeasureText(helpText, 12);
    int maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;

    int padding = 6;
    int boxW = maxW + padding * 2;
    int boxH = 14 * 4 + 12 + padding * 2 + 10;  // 5 lines + padding + spacing

    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 20, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){80, 80, 80, 255});

    // Draw text
    int y = ty + padding;
    DrawTextShadow(priorityText, tx + padding, y, 14, WHITE);
    y += 16;

    DrawTextShadow(stackText, tx + padding, y, 14, WHITE);
    y += 16;

    // Storage line - red if overfull
    bool overfull = totalItems > maxCapacity;
    DrawTextShadow(storageText, tx + padding, y, 14, overfull ? RED : WHITE);
    y += 16;

    // Draw filters with color coding
    int fx = tx + padding;
    DrawTextShadow("Filters: ", fx, y, 14, WHITE);
    fx += MeasureText("Filters: ", 14);
    DrawTextShadow(sp->allowedTypes[ITEM_RED] ? "R" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_RED] ? RED : DARKGRAY);
    fx += MeasureText("R", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_GREEN] ? "G" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_GREEN] ? GREEN : DARKGRAY);
    fx += MeasureText("G", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_BLUE] ? "B" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_BLUE] ? BLUE : DARKGRAY);
    y += 18;

    DrawTextShadow(helpText, tx + padding, y, 12, GRAY);
}

// Get mover index at world position (within radius), or -1 if none
int GetMoverAtWorldPos(float wx, float wy, int wz) {
    float bestDist = 999999.0f;
    int bestIdx = -1;
    float radius = CELL_SIZE * 0.6f;  // Click radius

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if ((int)m->z != wz) continue;

        float dx = m->x - wx;
        float dy = m->y - wy;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist < radius && dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// Draw mover debug tooltip (only shown when paused)
void DrawMoverTooltip(int moverIdx, Vector2 mouse) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    if (!m->active) return;

    // Job state names
    const char* jobStateNames[] = {"IDLE", "MOVING_TO_ITEM", "MOVING_TO_STOCKPILE"};
    const char* jobStateName = (m->jobState >= 0 && m->jobState <= 2) ? jobStateNames[m->jobState] : "?";

    // Build tooltip lines
    char line1[64], line2[64], line3[64], line4[64], line5[64], line6[64];
    snprintf(line1, sizeof(line1), "Mover #%d", moverIdx);
    snprintf(line2, sizeof(line2), "Pos: (%.1f, %.1f, %.0f)", m->x, m->y, m->z);
    snprintf(line3, sizeof(line3), "Job: %s", jobStateName);
    snprintf(line4, sizeof(line4), "Carrying: %s", m->carryingItem >= 0 ? TextFormat("#%d", m->carryingItem) : "none");
    snprintf(line5, sizeof(line5), "Path: %d/%d, Goal: (%d,%d)",
        m->pathIndex >= 0 ? m->pathIndex + 1 : 0, m->pathLength, m->goal.x, m->goal.y);
    snprintf(line6, sizeof(line6), "Target SP: %d, Slot: (%d,%d)",
        m->targetStockpile, m->targetSlotX, m->targetSlotY);

    // Measure text widths
    int w1 = MeasureText(line1, 14);
    int w2 = MeasureText(line2, 14);
    int w3 = MeasureText(line3, 14);
    int w4 = MeasureText(line4, 14);
    int w5 = MeasureText(line5, 14);
    int w6 = MeasureText(line6, 14);
    int maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;
    if (w6 > maxW) maxW = w6;

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * 6 + padding * 2;

    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 40, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){100, 100, 150, 255});

    // Draw text
    int y = ty + padding;
    DrawTextShadow(line1, tx + padding, y, 14, YELLOW);
    y += lineH;
    DrawTextShadow(line2, tx + padding, y, 14, WHITE);
    y += lineH;
    DrawTextShadow(line3, tx + padding, y, 14, m->jobState == JOB_IDLE ? GRAY : GREEN);
    y += lineH;
    DrawTextShadow(line4, tx + padding, y, 14, m->carryingItem >= 0 ? ORANGE : GRAY);
    y += lineH;
    DrawTextShadow(line5, tx + padding, y, 14, m->pathLength > 0 ? WHITE : RED);
    y += lineH;
    DrawTextShadow(line6, tx + padding, y, 14, m->targetStockpile >= 0 ? WHITE : GRAY);
}

void GenerateCurrentTerrain(void) {
    TraceLog(LOG_INFO, "Generating terrain: %s", terrainNames[currentTerrain]);
    switch (currentTerrain) {
        case 0: InitGrid(); break;
        case 1: GenerateSparse(0.10f); break;
        case 2: GenerateCity(); break;
        case 3: GenerateMixed(); break;
        case 4: GeneratePerlin(); break;
        case 5: GenerateConcentricMaze(); break;
        case 6: GenerateDungeonRooms(); break;
        case 7: GenerateCaves(); break;
        case 8: GenerateDrunkard(); break;
        case 9: GenerateTunneler(); break;
        case 10: GenerateMixMax(); break;
        case 11: InitGridFromAsciiWithChunkSize(narrowGapsMap, 8, 8); break;
        case 12: GenerateTowers(); break;
        case 13: GenerateGalleryFlat(); break;
        case 14: GenerateCastle(); break;
        case 15: GenerateLabyrinth3D(); break;
        case 16: GenerateSpiral3D(); break;
        case 17: GenerateCouncilEstate(); break;
    }
}

void HandleInput(void) {
    // Update stockpile hover state
    Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
    hoveredStockpile = GetStockpileAtGrid((int)mouseGrid.x, (int)mouseGrid.y, currentViewZ);

    // Update mover hover state (only when paused, for debugging)
    if (paused) {
        Vector2 mouseWorld = ScreenToWorld(GetMousePosition());
        hoveredMover = GetMoverAtWorldPos(mouseWorld.x, mouseWorld.y, currentViewZ);
    } else {
        hoveredMover = -1;
    }

    // Stockpile editing controls (when hovering)
    if (hoveredStockpile >= 0) {
        Stockpile* sp = &stockpiles[hoveredStockpile];

        // Priority: +/- or =/- keys
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
            if (sp->priority < 9) {
                sp->priority++;
                AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE);
            }
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            if (sp->priority > 1) {
                sp->priority--;
                AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE);
            }
        }

        // Stack size: [ / ] keys
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            int newSize = sp->maxStackSize + 1;
            if (newSize <= MAX_STACK_SIZE) {
                SetStockpileMaxStackSize(hoveredStockpile, newSize);
                AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE);
            }
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            int newSize = sp->maxStackSize - 1;
            if (newSize >= 1) {
                SetStockpileMaxStackSize(hoveredStockpile, newSize);
                AddMessage(TextFormat("Stack size: %d (excess ejected)", sp->maxStackSize), ORANGE);
            }
        }

        // Filter toggles: R/G/B keys
        if (IsKeyPressed(KEY_R)) {
            sp->allowedTypes[ITEM_RED] = !sp->allowedTypes[ITEM_RED];
            AddMessage(TextFormat("Red filter: %s", sp->allowedTypes[ITEM_RED] ? "ON" : "OFF"), RED);
            return;  // Don't trigger room drawing
        }
        if (IsKeyPressed(KEY_G)) {
            sp->allowedTypes[ITEM_GREEN] = !sp->allowedTypes[ITEM_GREEN];
            AddMessage(TextFormat("Green filter: %s", sp->allowedTypes[ITEM_GREEN] ? "ON" : "OFF"), GREEN);
        }
        if (IsKeyPressed(KEY_B)) {
            sp->allowedTypes[ITEM_BLUE] = !sp->allowedTypes[ITEM_BLUE];
            AddMessage(TextFormat("Blue filter: %s", sp->allowedTypes[ITEM_BLUE] ? "ON" : "OFF"), BLUE);
        }
    }

    // Zoom with mouse wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mw = ScreenToGrid(GetMousePosition());
        zoom += wheel * 0.1f;
        if (zoom < 0.1f) zoom = 0.1f;
        if (zoom > 5.0f) zoom = 5.0f;
        float size = CELL_SIZE * zoom;
        offset.x = GetMousePosition().x - mw.x * size;
        offset.y = GetMousePosition().y - mw.y * size;
    }

    // Pan with middle mouse
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        offset.x += d.x;
        offset.y += d.y;
    }

    // Skip grid interactions if UI wants mouse
    if (ui_wants_mouse()) return;

    // Room drawing mode (R key + drag) - check before normal tools
    if (IsKeyDown(KEY_R)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingRoom = true;
            roomStartX = x;
            roomStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingRoom) {
            drawingRoom = false;
            int x1 = roomStartX < x ? roomStartX : x;
            int y1 = roomStartY < y ? roomStartY : y;
            int x2 = roomStartX > x ? roomStartX : x;
            int y2 = roomStartY > y ? roomStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            for (int ry = y1; ry <= y2; ry++) {
                for (int rx = x1; rx <= x2; rx++) {
                    bool isBorder = (rx == x1 || rx == x2 || ry == y1 || ry == y2);
                    grid[z][ry][rx] = isBorder ? CELL_WALL : CELL_FLOOR;
                    MarkChunkDirty(rx, ry, z);
                }
            }
        }
        return;  // Skip normal tool interactions while R is held
    } else {
        drawingRoom = false;
    }

    // Floor drawing mode (F key + drag) - fills entire area with floor
    if (IsKeyDown(KEY_F)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingFloor = true;
            floorStartX = x;
            floorStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingFloor) {
            drawingFloor = false;
            int x1 = floorStartX < x ? floorStartX : x;
            int y1 = floorStartY < y ? floorStartY : y;
            int x2 = floorStartX > x ? floorStartX : x;
            int y2 = floorStartY > y ? floorStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            for (int ry = y1; ry <= y2; ry++) {
                for (int rx = x1; rx <= x2; rx++) {
                    grid[z][ry][rx] = CELL_FLOOR;
                    MarkChunkDirty(rx, ry, z);
                }
            }
        }
        return;  // Skip normal tool interactions while F is held
    } else {
        drawingFloor = false;
    }

    // Ladder drawing shortcut (L key + click/drag)
    // Uses PlaceLadder for auto-connection behavior
    if (IsKeyDown(KEY_L) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            PlaceLadder(x, y, z);
        }
        return;
    }

    // Tool-based interactions
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            switch (currentTool) {
                case 0:  // Draw Wall
                    if (grid[z][y][x] != CELL_WALL) {
                        grid[z][y][x] = CELL_WALL;
                        MarkChunkDirty(x, y, z);
                        // Mark movers whose path crosses this cell for replanning
                        for (int i = 0; i < moverCount; i++) {
                            Mover* m = &movers[i];
                            if (!m->active) continue;
                            for (int j = m->pathIndex; j >= 0; j--) {
                                if (m->path[j].x == x && m->path[j].y == y && m->path[j].z == z) {
                                    m->needsRepath = true;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                case 1:  // Draw Floor
                    if (grid[z][y][x] != CELL_FLOOR) {
                        grid[z][y][x] = CELL_FLOOR;
                        MarkChunkDirty(x, y, z);
                    }
                    break;
                case 2:  // Draw Ladder (auto-connects with level above)
                    PlaceLadder(x, y, z);
                    break;
                case 3:  // Erase (handles ladder cascade, air on z>0, grass on z=0)
                    if (IsLadderCell(grid[z][y][x])) {
                        EraseLadder(x, y, z);
                    } else {
                        CellType eraseType = (z > 0) ? CELL_AIR : CELL_WALKABLE;
                        if (grid[z][y][x] != eraseType) {
                            grid[z][y][x] = eraseType;
                            MarkChunkDirty(x, y, z);
                        }
                    }
                    break;
                case 4:  // Set Start
                    if (IsCellWalkableAt(z, y, x)) {
                        startPos = (Point){x, y, z};
                        pathLength = 0;
                    }
                    break;
                case 5:  // Set Goal
                    if (IsCellWalkableAt(z, y, x)) {
                        goalPos = (Point){x, y, z};
                        pathLength = 0;
                    }
                    break;
            }
        }
    }

    // Right-click erases (convenience shortcut, handles ladder cascade)
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            if (IsLadderCell(grid[z][y][x])) {
                EraseLadder(x, y, z);
            } else {
                CellType eraseType = (z > 0) ? CELL_AIR : CELL_WALKABLE;
                if (grid[z][y][x] != eraseType) {
                    grid[z][y][x] = eraseType;
                    MarkChunkDirty(x, y, z);
                }
            }
        }
    }

    // Reset view (C key)
    if (IsKeyPressed(KEY_C)) {
        zoom = 1.0f;
        offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
        offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
    }

    // Z-level switching (< > like Dwarf Fortress, or [ ])
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_PERIOD) && shift) {  // >
        if (currentViewZ < gridDepth - 1) currentViewZ++;
    }
    if (IsKeyPressed(KEY_COMMA) && shift) {  // <
        if (currentViewZ > 0) currentViewZ--;
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
        if (currentViewZ < gridDepth - 1) currentViewZ++;
    }
    if (IsKeyPressed(KEY_LEFT_BRACKET)) {
        if (currentViewZ > 0) currentViewZ--;
    }

    // Pause toggle (Space)
    if (IsKeyPressed(KEY_SPACE)) {
        paused = !paused;
    }
}

void DrawUI(void) {
    ui_begin_frame();
    float y = 30.0f;
    float x = 10.0f;

    // === VIEW ===
    if (SectionHeader(x, y, "View", &sectionView)) {
        y += 18;
        ToggleBool(x, y, "Show Graph", &showGraph);
        y += 22;
        ToggleBool(x, y, "Show Entrances", &showEntrances);
        y += 22;
        ToggleBool(x, y, "Cull Drawing", &cullDrawing);
    }
    y += 22;

    // === PATHFINDING ===
    y += 8;
    if (SectionHeader(x, y, "Pathfinding", &sectionPathfinding)) {
        y += 18;
        int prevAlgo = pathAlgorithm;
        CycleOption(x, y, "Algo", algorithmNames, 4, &pathAlgorithm);
        if (pathAlgorithm != prevAlgo) ResetPathStats();
        y += 22;
        CycleOption(x, y, "Dir", directionNames, 2, &currentDirection);
        use8Dir = (currentDirection == 1);  // Sync with pathfinding
        y += 22;
        if (PushButton(x, y, "Build HPA Graph")) {
            BuildEntrances();
            BuildGraph();
        }
        y += 22;
        if (PushButton(x, y, "Find Path")) {
            if (pathAlgorithm == 1) {
                if (graphEdgeCount == 0) {
                    BuildEntrances();
                    BuildGraph();
                } else if (needsRebuild) {
                    UpdateDirtyChunks();
                }
            }
            switch (pathAlgorithm) {
                case 0: RunAStar(); break;
                case 1: RunHPAStar(); break;
                case 2: RunJPS(); break;
                case 3: RunJpsPlus(); break;
            }
        }
    }
    y += 22;

    // === MAP EDITING ===
    y += 8;
    if (SectionHeader(x, y, "Map Editing", &sectionMapEditing)) {
        y += 18;
        CycleOption(x, y, "Tool", toolNames, 6, &currentTool);
        y += 22;
        CycleOption(x, y, "Terrain", terrainNames, 18, &currentTerrain);
        y += 22;
        if (PushButton(x, y, "Generate Terrain")) {
            GenerateCurrentTerrain();
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            BuildEntrances();
            BuildGraph();
            AddMessage(TextFormat("Generated terrain: %s", terrainNames[currentTerrain]), GREEN);
        }
        y += 22;
        if (PushButton(x, y, "Small Grid (32x32)")) {
            InitGridWithSizeAndChunkSize(32, 32, 8, 8);
            gridDepth = 6;
            // Set z>0 to air (z=0 already walkable from init)
            for (int z = 1; z < gridDepth; z++)
                for (int gy = 0; gy < gridHeight; gy++)
                    for (int gx = 0; gx < gridWidth; gx++)
                        grid[z][gy][gx] = CELL_AIR;
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            BuildEntrances();
            BuildGraph();
            offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
            offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
        }
        y += 22;
        if (PushButton(x, y, "Big Grid (256x256)")) {
            InitGridWithSizeAndChunkSize(256, 256, 16, 16);
            gridDepth = 3;
            // Set z>0 to air (z=0 already walkable from init)
            for (int z = 1; z < gridDepth; z++)
                for (int gy = 0; gy < gridHeight; gy++)
                    for (int gx = 0; gx < gridWidth; gx++)
                        grid[z][gy][gx] = CELL_AIR;
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            BuildEntrances();
            BuildGraph();
            offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
            offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
        }
        y += 22;
        if (PushButton(x, y, "Copy Map ASCII")) {
            // Copy map to clipboard as ASCII (supports multiple floors)
            // Calculate buffer size: for each floor, "floor:N\n" + grid data + newlines
            // "floor:" = 6, max digits for floor number ~3, "\n" = 1, so ~10 chars per floor header
            int floorDataSize = gridWidth * gridHeight + gridHeight;  // grid + newlines per floor
            int bufferSize = gridDepth * (10 + floorDataSize) + 1;
            char* buffer = malloc(bufferSize);
            int idx = 0;

            for (int z = 0; z < gridDepth; z++) {
                // Write floor marker
                idx += snprintf(buffer + idx, bufferSize - idx, "floor:%d\n", z);

                // Write grid data for this floor
                for (int row = 0; row < gridHeight; row++) {
                    for (int col = 0; col < gridWidth; col++) {
                        char c;
                        switch (grid[z][row][col]) {
                            case CELL_WALL:        c = '#'; break;
                            case CELL_LADDER:      c = 'L'; break;  // Legacy
                            case CELL_LADDER_UP:   c = '<'; break;
                            case CELL_LADDER_DOWN: c = '>'; break;
                            case CELL_LADDER_BOTH: c = 'X'; break;
                            default:               c = '.'; break;
                        }
                        buffer[idx++] = c;
                    }
                    buffer[idx++] = '\n';
                }
            }
            buffer[idx] = '\0';
            SetClipboardText(buffer);
            free(buffer);
            TraceLog(LOG_INFO, "Map copied to clipboard (%d floors)", gridDepth);
        }
    }
    y += 22;

    // === AGENTS ===
    y += 8;
    if (SectionHeader(x, y, "Agents", &sectionAgents)) {
        y += 18;
        DraggableInt(x, y, "Count", &agentCountSetting, 1.0f, 1, MAX_AGENTS);
        y += 22;
        if (PushButton(x, y, "Spawn Agents")) {
            if (graphEdgeCount == 0) {
                BuildEntrances();
                BuildGraph();
            }
            SpawnAgents(agentCountSetting);
        }
        y += 22;
        if (PushButton(x, y, "Repath Agents")) {
            if (pathAlgorithm == 1 && graphEdgeCount == 0) {
                BuildEntrances();
                BuildGraph();
            }
            RepathAgents();
        }
    }
    y += 22;

    // === MOVERS ===
    y += 8;
    if (PushButton(x + 150, y, "+")) {
        AddMoversDemo(moverCountSetting);
    }
    if (SectionHeader(x, y, TextFormat("Movers (%d/%d)", CountActiveMovers(), moverCount), &sectionMovers)) {
        y += 18;
        DraggableIntLog(x, y, "Count", &moverCountSetting, 1.0f, 1, MAX_MOVERS);
        y += 22;
        if (PushButton(x, y, "Spawn Movers")) {
            SpawnMoversDemo(moverCountSetting);
        }
        y += 22;
        if (PushButton(x, y, "Clear Movers")) {
            ClearMovers();
        }
        y += 22;
        ToggleBool(x, y, "Show Movers", &showMovers);
        y += 22;
        ToggleBool(x, y, "Show Paths", &showMoverPaths);
        y += 22;
        ToggleBool(x, y, "String Pulling", &useStringPulling);
        y += 22;
        ToggleBool(x, y, "Endless Mode", &endlessMoverMode);
        y += 22;
        ToggleBool(x, y, "Prefer Diff Z", &preferDifferentZ);
        y += 22;
        ToggleBool(x, y, "Allow Falling", &allowFallingFromAvoidance);

        // Avoidance subsection
        y += 22;
        if (SectionHeader(x + 10, y, "Avoidance", &sectionMoverAvoidance)) {
            y += 18;
            ToggleBool(x + 10, y, "Enabled", &useMoverAvoidance);
            y += 22;
            ToggleBool(x + 10, y, "Directional", &useDirectionalAvoidance);
            y += 22;
            DraggableFloat(x + 10, y, "Open Strength", &avoidStrengthOpen, 0.01f, 0.0f, 2.0f);
            y += 22;
            DraggableFloat(x + 10, y, "Closed Strength", &avoidStrengthClosed, 0.01f, 0.0f, 2.0f);
        }

        // Walls subsection
        y += 22;
        if (SectionHeader(x + 10, y, "Walls", &sectionMoverWalls)) {
            y += 18;
            ToggleBool(x + 10, y, "Repulsion", &useWallRepulsion);
            y += 22;
            DraggableFloat(x + 10, y, "Repel Strength", &wallRepulsionStrength, 0.01f, 0.0f, 2.0f);
            y += 22;
            ToggleBool(x + 10, y, "Sliding", &useWallSliding);
            y += 22;
            ToggleBool(x + 10, y, "Knot Fix", &useKnotFix);
        }

        // Debug views subsection
        y += 22;
        if (SectionHeader(x + 10, y, "Debug Views", &sectionMoverDebug)) {
            y += 18;
            ToggleBool(x + 10, y, "Show Neighbors", &showNeighborCounts);
            y += 22;
            ToggleBool(x + 10, y, "Show Open Area", &showOpenArea);
            y += 22;
            ToggleBool(x + 10, y, "Show Knots", &showKnotDetection);
            y += 22;
            ToggleBool(x + 10, y, "Show Stuck", &showStuckDetection);
        }
    }
    y += 22;

    // === JOBS ===
    y += 8;
    if (SectionHeader(x, y, TextFormat("Jobs (%d items)", itemCount), &sectionJobs)) {
        y += 18;
        DraggableIntLog(x, y, "Count", &itemCountSetting, 1.0f, 1, MAX_ITEMS);
        y += 22;
        if (PushButton(x, y, "Spawn Items")) {
            for (int i = 0; i < itemCountSetting; i++) {
                // Find a random walkable cell on current z-level
                int attempts = 100;
                while (attempts-- > 0) {
                    int gx = rand() % gridWidth;
                    int gy = rand() % gridHeight;
                    if (IsCellWalkableAt(currentViewZ, gy, gx)) {
                        float px = gx * CELL_SIZE + CELL_SIZE * 0.5f;
                        float py = gy * CELL_SIZE + CELL_SIZE * 0.5f;
                        ItemType type = rand() % 3;  // ITEM_RED, ITEM_GREEN, or ITEM_BLUE
                        SpawnItem(px, py, (float)currentViewZ, type);
                        break;
                    }
                }
            }
        }
        y += 22;
        if (PushButton(x, y, "Clear Items")) {
            ClearItems();
        }
        y += 22;
        ToggleBool(x, y, "Show Items", &showItems);
        y += 22;
        if (PushButton(x, y, "Stockpile: All"))   SpawnStockpileWithFilters(true, true, true);
        y += 22;
        if (PushButton(x, y, "Stockpile: Red"))   SpawnStockpileWithFilters(true, false, false);
        y += 22;
        if (PushButton(x, y, "Stockpile: Green")) SpawnStockpileWithFilters(false, true, false);
        y += 22;
        if (PushButton(x, y, "Stockpile: Blue"))  SpawnStockpileWithFilters(false, false, true);
        y += 22;
        if (PushButton(x, y, "Clear Stockpiles")) {
            ClearStockpiles();
        }
    }
    y += 22;

}

#if PROFILER_ENABLED
void DrawProfilerPanel(float rightEdge, float y) {
    float panelW = 220;
    float x = rightEdge - panelW;
    Vector2 mouse = GetMousePosition();

    // Block click-through for entire panel area
    float panelH = sectionProfiler ? 300 : 20;
    if (mouse.x >= x && mouse.x < rightEdge && mouse.y >= y && mouse.y < y + panelH) {
        ui_set_hovered();
    }

    // Draw right-aligned header
    const char* headerText = sectionProfiler ? "[-] Profiler" : "[+] Profiler";
    int headerWidth = MeasureText(headerText, 14);
    float headerX = rightEdge - headerWidth;
    bool hovered = mouse.x >= headerX && mouse.x < headerX + headerWidth + 10 &&
                   mouse.y >= y && mouse.y < y + 18;

    DrawTextShadow(headerText, (int)headerX, (int)y, 14, hovered ? YELLOW : GRAY);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        sectionProfiler = !sectionProfiler;
    }

    if (sectionProfiler) {
        y += 18;

        // Memory section
        const char* memHeader = sectionMemory ? "[-] Memory" : "[+] Memory";
        int memHeaderWidth = MeasureText(memHeader, 14);
        float memHeaderX = rightEdge - memHeaderWidth;
        bool memHovered = mouse.x >= memHeaderX && mouse.x < memHeaderX + memHeaderWidth + 10 &&
                          mouse.y >= y && mouse.y < y + 18;
        DrawTextShadow(memHeader, (int)memHeaderX, (int)y, 14, memHovered ? YELLOW : GRAY);
        if (memHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            sectionMemory = !sectionMemory;
        }
        y += 18;

        if (sectionMemory) {
            // Calculate sizes of major static arrays (in KB)
            size_t gridSize = sizeof(CellType) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t moversSize = sizeof(Mover) * MAX_MOVERS;
            size_t entrancesSize = sizeof(Entrance) * MAX_ENTRANCES;
            size_t pathSize = sizeof(Point) * MAX_PATH;
            size_t edgesSize = sizeof(GraphEdge) * MAX_EDGES;
            size_t spatialGrid = (moverGrid.cellCount + 1) * sizeof(int) * 2 + MAX_MOVERS * sizeof(int);
            size_t total = gridSize + moversSize + entrancesSize + pathSize + edgesSize + spatialGrid;

            DrawTextShadow(TextFormat("Grid:       %5.1f MB", gridSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("Movers:     %5.1f MB", moversSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("Entrances:  %5.1f MB", entrancesSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("Path:       %5.1f MB", pathSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("Edges:      %5.1f MB", edgesSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("SpatialGrid:%5.1f KB", spatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("Total:      %5.1f MB", total / (1024.0f * 1024.0f)), x, y, 14, PINK); y += 20;
        }

        // Find max value for scaling bars
        float maxMs = 1.0f;  // Minimum scale of 1ms
        for (int i = 0; i < profilerSectionCount; i++) {
            float last = (float)ProfileGetLast(i);
            if (last > maxMs) maxMs = last;
        }

        // Bar graph settings
        int barMaxWidth = 100;
        int labelWidth = 110;
        int indentPerLevel = 12;

        // Colors for each section (shared with line graph)
        Color sectionColors[] = {GREEN, YELLOW, ORANGE, SKYBLUE, PINK, PURPLE, RED, LIME};
        int numColors = sizeof(sectionColors) / sizeof(sectionColors[0]);

        // Check for hover on section labels
        int labelHoveredSection = -1;
        int labelStartY = y;  // Remember starting Y for label hover detection

        int visibleRow = 0;
        for (int i = 0; i < profilerSectionCount; i++) {
            ProfileSection* s = &profilerSections[i];

            // Skip hidden sections (collapsed ancestors)
            if (ProfileIsHidden(i)) continue;

            float last = (float)ProfileGetLast(i);
            float avg = (float)ProfileGetAvg(i);
            Color sectionColor = sectionColors[i % numColors];
            int indent = s->depth * indentPerLevel;
            bool hasChildren = ProfileHasChildren(i);

            // Check if mouse is hovering this label row
            int rowY = labelStartY + visibleRow * 18;
            bool hoveringLabel = (mouse.x >= x && mouse.x < x + labelWidth &&
                                  mouse.y >= rowY && mouse.y < rowY + 18);
            if (hoveringLabel) {
                labelHoveredSection = i;
                // Block click-through when hovering collapsible items
                if (hasChildren) {
                    ui_set_hovered();
                    // Click to toggle collapse
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        s->collapsed = !s->collapsed;
                    }
                }
            }

            // Draw collapse indicator for sections with children
            if (hasChildren) {
                const char* arrow = s->collapsed ? "+" : "-";
                Color arrowColor = s->collapsed ? YELLOW : GRAY;
                DrawTextShadow(arrow, x + indent, y, 14, arrowColor);
            }

            // Draw color indicator square (indented based on depth, dimmed if collapsed)
            Color squareColor = sectionColor;
            if (s->collapsed) squareColor = (Color){sectionColor.r/2, sectionColor.g/2, sectionColor.b/2, 255};
            DrawRectangle(x + indent + (hasChildren ? 10 : 0), y + 3, 10, 10, squareColor);

            // Draw label (highlight if hovered, indented based on depth, show ... if collapsed)
            Color labelColor = hoveringLabel ? sectionColor : (s->collapsed ? GRAY : WHITE);
            const char* displayName = s->collapsed ? TextFormat("%s ...", s->name) : s->name;
            DrawTextShadow(displayName, x + 14 + indent + (hasChildren ? 10 : 0), y, 14, labelColor);

            // Draw bar background
            int barX = x + labelWidth;
            DrawRectangle(barX, y + 2, barMaxWidth, 12, (Color){40, 40, 40, 255});

            // Draw bar (colored by intensity)
            int barWidth = (int)(last / maxMs * barMaxWidth);
            if (barWidth < 1 && last > 0) barWidth = 1;

            // Color: green for low, yellow for medium, red for high (relative to max)
            float ratio = last / maxMs;
            Color barColor;
            if (ratio < 0.3f) {
                barColor = GREEN;
            } else if (ratio < 0.6f) {
                barColor = YELLOW;
            } else {
                barColor = (Color){255, 100, 100, 255};  // Light red
            }
            DrawRectangle(barX, y + 2, barWidth, 12, barColor);

            // Draw avg marker line
            int avgX = barX + (int)(avg / maxMs * barMaxWidth);
            DrawLine(avgX, y + 1, avgX, y + 14, WHITE);

            // Draw value
            DrawTextShadow(TextFormat("%.2f", last), barX + barMaxWidth + 5, y, 14, WHITE);

            y += 18;
            visibleRow++;
        }

        // Line graph showing history
        y += 10;
        int graphW = labelWidth + barMaxWidth;  // Match width of bars above
        int graphX = x + labelWidth + barMaxWidth - graphW;  // Right-align with bars
        int graphY = y;
        int graphH = 60;

        // Find max across all history for scaling
        float graphMax = 1.0f;
        for (int i = 0; i < profilerSectionCount; i++) {
            ProfileSection* s = &profilerSections[i];
            for (int f = 0; f < s->historyCount; f++) {
                if (s->history[f] > graphMax) graphMax = (float)s->history[f];
            }
        }

        // Draw background
        DrawRectangle(graphX, graphY, graphW, graphH, (Color){30, 30, 30, 255});
        DrawRectangleLines(graphX, graphY, graphW, graphH, GRAY);

        // Draw horizontal guide lines
        for (int i = 1; i < 4; i++) {
            int lineY = graphY + (graphH * i / 4);
            DrawLine(graphX, lineY, graphX + graphW, lineY, (Color){50, 50, 50, 255});
        }

        // Check if mouse is in graph area
        bool mouseInGraph = (mouse.x >= graphX && mouse.x < graphX + graphW &&
                             mouse.y >= graphY && mouse.y < graphY + graphH);

        // Find which section is closest to mouse (if hovering on graph)
        // Or use labelHoveredSection if hovering on a label
        int hoveredSection = labelHoveredSection;
        float hoveredValue = 0;
        if (mouseInGraph && labelHoveredSection < 0) {
            // Figure out which frame the mouse is over
            int mouseFrame = (int)((mouse.x - graphX) * PROFILER_HISTORY_FRAMES / graphW);
            if (mouseFrame < 0) mouseFrame = 0;
            if (mouseFrame >= PROFILER_HISTORY_FRAMES) mouseFrame = PROFILER_HISTORY_FRAMES - 1;

            float minDist = 999999.0f;
            for (int i = 0; i < profilerSectionCount; i++) {
                ProfileSection* s = &profilerSections[i];
                if (s->historyCount <= mouseFrame) continue;

                int idx = (s->historyIndex + mouseFrame) % PROFILER_HISTORY_FRAMES;
                float val = (float)s->history[idx];
                int valY = graphY + graphH - (int)(val / graphMax * graphH);
                float dist = fabsf(mouse.y - valY);

                if (dist < minDist && dist < 15) {  // Within 15 pixels
                    minDist = dist;
                    hoveredSection = i;
                    hoveredValue = val;
                }
            }
        }

        // Draw lines for each section
        for (int i = 0; i < profilerSectionCount; i++) {
            ProfileSection* s = &profilerSections[i];
            if (s->historyCount < 2) continue;

            Color col = sectionColors[i % numColors];

            // Dim non-hovered sections when hovering
            if (hoveredSection >= 0 && hoveredSection != i) {
                col.a = 60;
            }

            for (int f = 0; f < s->historyCount - 1; f++) {
                // Read from oldest to newest
                int idx0 = (s->historyIndex + f) % PROFILER_HISTORY_FRAMES;
                int idx1 = (s->historyIndex + f + 1) % PROFILER_HISTORY_FRAMES;

                float v0 = (float)s->history[idx0];
                float v1 = (float)s->history[idx1];

                int x0 = graphX + (f * graphW / PROFILER_HISTORY_FRAMES);
                int x1 = graphX + ((f + 1) * graphW / PROFILER_HISTORY_FRAMES);
                int y0 = graphY + graphH - (int)(v0 / graphMax * graphH);
                int y1 = graphY + graphH - (int)(v1 / graphMax * graphH);

                DrawLine(x0, y0, x1, y1, col);
            }
        }

        // Draw tooltip for hovered section (only when hovering graph, not label)
        if (hoveredSection >= 0 && labelHoveredSection < 0) {
            ProfileSection* s = &profilerSections[hoveredSection];
            const char* tooltip = TextFormat("%s: %.2fms", s->name, hoveredValue);
            int tooltipW = MeasureText(tooltip, 14) + 10;
            int tooltipX = (int)mouse.x + 10;
            int tooltipY = (int)mouse.y - 20;

            // Keep tooltip in screen
            if (tooltipX + tooltipW > graphX + graphW) tooltipX = (int)mouse.x - tooltipW - 5;

            DrawRectangle(tooltipX - 2, tooltipY - 2, tooltipW, 18, (Color){20, 20, 20, 230});
            DrawTextShadow(tooltip, tooltipX, tooltipY, 14, sectionColors[hoveredSection % numColors]);
        }

        // Draw scale label
        DrawTextShadow(TextFormat("%.1fms", graphMax), graphX + graphW + 5, graphY, 12, WHITE);
        DrawTextShadow("0", graphX + graphW + 5, graphY + graphH - 12, 12, WHITE);
    }
}
#endif

int main(void) {
    int screenWidth = 1280, screenHeight = 800;
    InitWindow(screenWidth, screenHeight, "HPA* Pathfinding");
    atlas = LoadTexture(ATLAS_PATH);
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);  // Crisp pixels, no bleeding
    Font comicFont = LoadFont("assets/fonts/comic.fnt");
    ui_init(&comicFont);
    SetTargetFPS(60);
    use8Dir = true;  // Default to 8-dir
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);  // 32x32 grid, 8x8 chunks
    gridDepth = 6;
    // Initialize z=0 as walkable (ground), z>0 as air
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[0][y][x] = CELL_WALKABLE;
    for (int z = 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[z][y][x] = CELL_AIR;
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    BuildEntrances();
    BuildGraph();
    offset.x = (screenWidth - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (screenHeight - gridHeight * CELL_SIZE * zoom) / 2.0f;

    float accumulator = 0.0f;

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        accumulator += frameTime;

        ui_update();
        HandleInput();
        if (!paused) {
            UpdatePathStats();
            if (pathStatsUpdated) {
                // Count blocked movers (active but no valid path)
                int blockedCount = 0;
                for (int i = 0; i < moverCount; i++) {
                    if (movers[i].active && movers[i].pathLength == 0) {
                        blockedCount++;
                    }
                }
                if (blockedCount > 0) {
                    AddMessage(TextFormat("%d mover%s blocked", blockedCount, blockedCount == 1 ? "" : "s"), ORANGE);
                }

                pathStatsUpdated = false;
            }
        }
        UpdateMessages(frameTime, paused);

        // Fixed timestep update (Factorio-style: max 1 tick per frame, slowdown if behind)
        if (!paused && accumulator >= TICK_DT) {
            PROFILE_BEGIN(Tick);
            Tick();
            AssignJobs();
            JobsTick();
            PROFILE_END(Tick);
            accumulator -= TICK_DT;

            // Drain excess - don't try to catch up, just slow down
            if (accumulator > TICK_DT) {
                accumulator = TICK_DT;
            }
        }

        PROFILE_BEGIN(Render);
        BeginDrawing();
        ClearBackground(BLACK);
        PROFILE_BEGIN(DrawCells);
        DrawCellGrid();
        PROFILE_END(DrawCells);
        DrawStockpiles();
        DrawChunkBoundaries();
        PROFILE_BEGIN(DrawGraph);
        DrawGraph();
        PROFILE_END(DrawGraph);
        if (showEntrances) {
            DrawEntrances();
        }
        DrawPath();
        DrawAgents();
        if (showItems) {
            DrawItems();
        }
        if (showMovers) {
            PROFILE_BEGIN(DrawMovers);
            DrawMovers();
            PROFILE_END(DrawMovers);
        }

        // Draw room preview while dragging
        if (drawingRoom && IsKeyDown(KEY_R)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = roomStartX < x ? roomStartX : x;
            int y1 = roomStartY < y ? roomStartY : y;
            int x2 = roomStartX > x ? roomStartX : x;
            int y2 = roomStartY > y ? roomStartY : y;
            float size = CELL_SIZE * zoom;

            // Draw preview outline
            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, YELLOW);
        }

        // Draw floor preview while dragging
        if (drawingFloor && IsKeyDown(KEY_F)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = floorStartX < x ? floorStartX : x;
            int y1 = floorStartY < y ? floorStartY : y;
            int x2 = floorStartX > x ? floorStartX : x;
            int y2 = floorStartY > y ? floorStartY : y;
            float size = CELL_SIZE * zoom;

            // Draw preview as filled semi-transparent rectangle
            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;
            DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){139, 90, 43, 100});
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, BROWN);
        }

        // Stats display
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 5, 5, 18, LIME);
        DrawTextShadow(TextFormat("Z: %d/%d  </>", currentViewZ, gridDepth - 1), 5, screenHeight - 20, 18, SKYBLUE);

#if PROFILER_ENABLED
        DrawProfilerPanel(screenWidth - 50, 5);
#endif

        // Draw UI
        PROFILE_BEGIN(DrawUI);
        DrawUI();
        PROFILE_END(DrawUI);

        // Draw message stack
        DrawMessages(screenWidth, screenHeight);

        // Draw stockpile tooltip when hovering
        if (hoveredStockpile >= 0) {
            DrawStockpileTooltip(hoveredStockpile, GetMousePosition());
        }

        // Draw mover tooltip when hovering (only when paused)
        if (hoveredMover >= 0) {
            DrawMoverTooltip(hoveredMover, GetMousePosition());
        }

        PROFILE_BEGIN(EndDraw);
        EndDrawing();
        PROFILE_END(EndDraw);
        PROFILE_END(Render);
        if (!paused) PROFILE_FRAME_END();
    }
    UnloadTexture(atlas);
    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
