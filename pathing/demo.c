#include "../vendor/raylib.h"
#include "grid.h"
#include "terrain.h"
#include "pathfinding.h"
#include "mover.h"
#include "items.h"
#include "jobs.h"
#include "stockpiles.h"
#include "designations.h"
#include "water.h"
#include "fire.h"
#include "smoke.h"
#include "groundwear.h"
#include "inspect.h"
#define PROFILER_IMPLEMENTATION
#include "../shared/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

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

// Forward declarations for save/load
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// Room drawing state (R key to activate, drag to draw)
bool drawingRoom = false;
int roomStartX = 0, roomStartY = 0;

// Floor drawing state (F key to activate, drag to draw)
bool drawingFloor = false;
int floorStartX = 0, floorStartY = 0;

// Stockpile drawing state (S key to activate, left-drag to draw, right-drag to erase)
bool drawingStockpile = false;
bool erasingStockpile = false;
int stockpileStartX = 0, stockpileStartY = 0;

// Mining designation state (M key to activate, left-drag to designate, right-drag to cancel)
bool designatingMining = false;
bool cancellingMining = false;
int miningStartX = 0, miningStartY = 0;

// Build designation state (B key to activate, left-drag to designate, right-drag to cancel)
bool designatingBuild = false;
bool cancellingBuild = false;
int buildStartX = 0, buildStartY = 0;

// Water placement state (W key to activate, left-drag for source, right-drag for drain)
bool placingWaterSource = false;
bool placingWaterDrain = false;
int waterStartX = 0, waterStartY = 0;

// Fire placement state (F key to activate, left-drag to ignite, right-drag to extinguish)
bool placingFireSource = false;
bool extinguishingFire = false;
int fireStartX = 0, fireStartY = 0;

// Gather zone state (G key to activate, left-drag to draw, right-drag to erase)
bool drawingGatherZone = false;
bool erasingGatherZone = false;
int gatherZoneStartX = 0, gatherZoneStartY = 0;

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
bool sectionWater = false;
bool sectionFire = false;
bool sectionSmoke = false;
bool sectionEntropy = false;
bool sectionMoverWalls = false;
bool sectionMoverDebug = false;

bool sectionProfiler = false;
bool sectionMemory = false;
bool sectionJobs = true;
bool paused = false;
bool showItems = true;
bool showHelpPanel = false;

// Stockpile hover state
int hoveredStockpile = -1;  // Index of stockpile under mouse, -1 if none

// Mover hover state (only when paused)
int hoveredMover = -1;  // Index of mover under mouse, -1 if none

// Item hover state (only when paused)
int hoveredItemCell[16];  // Item indices at hovered cell (max 16)
int hoveredItemCount = 0;  // Number of items at hovered cell

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
        case CELL_WALKABLE:     return SPRITE_grass;  // Legacy alias
        case CELL_GRASS:        return SPRITE_grass;
        case CELL_DIRT:         return SPRITE_dirt;
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

void DrawWater(void) {
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

        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX > gridWidth) maxX = gridWidth;
        if (maxY > gridHeight) maxY = gridHeight;
    }

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetWaterLevel(x, y, z);
            if (level <= 0) continue;

            // Alpha based on water level (deeper = more opaque)
            int alpha = 80 + (level * 15);  // 80-230 range
            if (alpha > 230) alpha = 230;
            
            Color waterColor = (Color){30, 100, 200, alpha};
            
            // Draw water overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, waterColor);
            
            // Mark sources with a brighter center
            if (waterGrid[z][y][x].isSource) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){100, 180, 255, 200});
            }
            
            // Mark drains with a dark center
            if (waterGrid[z][y][x].isDrain) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){20, 40, 80, 200});
            }
        }
    }
}

void DrawFire(void) {
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

        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX > gridWidth) maxX = gridWidth;
        if (maxY > gridHeight) maxY = gridHeight;
    }

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            FireCell* cell = &fireGrid[z][y][x];
            int level = cell->level;
            
            // Draw burned cells with a darker tint
            if (level == 0 && HAS_CELL_FLAG(x, y, z, CELL_FLAG_BURNED)) {
                Color burnedColor = (Color){40, 30, 20, 100};
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, burnedColor);
                continue;
            }
            
            if (level <= 0) continue;

            // Color based on fire level (darker orange to bright yellow)
            // Level 1-2: dark orange/red (embers)
            // Level 3-4: orange
            // Level 5-7: bright orange/yellow
            int r, g, b, alpha;
            if (level <= 2) {
                r = 180; g = 60; b = 20;
                alpha = 120 + level * 20;
            } else if (level <= 4) {
                r = 220; g = 100; b = 30;
                alpha = 150 + (level - 2) * 15;
            } else {
                r = 255; g = 150 + (level - 4) * 20; b = 50;
                alpha = 180 + (level - 4) * 15;
            }
            if (alpha > 230) alpha = 230;
            
            Color fireColor = (Color){r, g, b, alpha};
            
            // Draw fire overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, fireColor);
            
            // Mark sources with a brighter center
            if (cell->isSource) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){255, 220, 100, 200});
            }
        }
    }
}

void DrawSmoke(void) {
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

        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX > gridWidth) maxX = gridWidth;
        if (maxY > gridHeight) maxY = gridHeight;
    }

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetSmokeLevel(x, y, z);
            if (level <= 0) continue;

            // Gray smoke, opacity based on level
            // Level 1-2: light haze
            // Level 3-5: moderate smoke
            // Level 6-7: thick smoke
            int alpha = 30 + (level * 25);  // 55-205 range
            if (alpha > 205) alpha = 205;
            
            Color smokeColor = (Color){80, 80, 90, alpha};
            
            // Draw smoke overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, smokeColor);
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
        Job* moverJob = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;
        int carryingItem = moverJob ? moverJob->carryingItem : -1;
        if (carryingItem >= 0 && items[carryingItem].active) {
            Item* item = &items[carryingItem];
            int sprite;
            switch (item->type) {
                case ITEM_RED:    sprite = SPRITE_crate_red;    break;
                case ITEM_GREEN:  sprite = SPRITE_crate_green;  break;
                case ITEM_BLUE:   sprite = SPRITE_crate_blue;   break;
                case ITEM_ORANGE: sprite = SPRITE_crate_orange; break;
                default:          sprite = SPRITE_apple;        break;
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
    
    // Draw hovered mover's path (always, even if showMoverPaths is off)
    if (hoveredMover >= 0 && !showMoverPaths) {
        Mover* m = &movers[hoveredMover];
        if (m->active && m->pathIndex >= 0) {
            float sx = offset.x + m->x * zoom;
            float sy = offset.y + m->y * zoom;
            Color pathColor = YELLOW;

            // Line from mover to next waypoint (if on same z)
            Point next = m->path[m->pathIndex];
            if (next.z == viewZ) {
                float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){sx, sy}, (Vector2){tx, ty}, 2.0f, pathColor);
            }

            // Rest of path
            for (int j = m->pathIndex; j > 0; j--) {
                if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
                float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 2.0f, Fade(pathColor, 0.6f));
            }
        }
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
            case ITEM_RED:    sprite = SPRITE_crate_red;    break;
            case ITEM_GREEN:  sprite = SPRITE_crate_green;  break;
            case ITEM_BLUE:   sprite = SPRITE_crate_blue;   break;
            case ITEM_ORANGE: sprite = SPRITE_crate_orange; break;
            default:          sprite = SPRITE_apple;        break;
        }

        float itemSize = size * ITEM_SIZE_GROUND;
        Rectangle src = SpriteGetRect(sprite);
        Rectangle dest = { sx - itemSize/2, sy - itemSize/2, itemSize, itemSize };

        // Tint reserved items slightly darker
        Color tint = (item->reservedBy >= 0) ? (Color) { 200, 200, 200, 255 } : WHITE;
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
    }
}

void DrawGatherZones(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_GATHER_ZONES; i++) {
        GatherZone* gz = &gatherZones[i];
        if (!gz->active) continue;
        if (gz->z != viewZ) continue;

        float sx = offset.x + gz->x * size;
        float sy = offset.y + gz->y * size;
        float w = gz->width * size;
        float h = gz->height * size;

        // Draw orange tinted overlay
        DrawRectangle((int)sx, (int)sy, (int)w, (int)h, (Color){255, 180, 50, 40});
        DrawRectangleLinesEx((Rectangle){sx, sy, w, h}, 2.0f, (Color){255, 180, 50, 180});
    }
}

void DrawStockpileTiles(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z != viewZ) continue;

        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int slotIdx = dy * sp->width + dx;
                if (!sp->cells[slotIdx]) continue;

                int gx = sp->x + dx;
                int gy = sp->y + dy;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, WHITE);

                // Highlight hovered stockpile with pulsing green overlay
                if (i == hoveredStockpile) {
                    float pulse = (sinf(GetTime() * 4.0f) + 1.0f) * 0.5f;
                    unsigned char alpha = (unsigned char)(40 + pulse * 60);
                    DrawRectangle((int)sx, (int)sy, (int)size, (int)size, (Color){100, 255, 100, alpha});
                }
            }
        }
    }
}

void DrawStockpileItems(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z != viewZ) continue;

        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int slotIdx = dy * sp->width + dx;
                if (!sp->cells[slotIdx]) continue;

                int count = sp->slotCounts[slotIdx];
                if (count <= 0) continue;

                int gx = sp->x + dx;
                int gy = sp->y + dy;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                ItemType type = sp->slotTypes[slotIdx];
                int sprite;
                switch (type) {
                    case ITEM_RED:    sprite = SPRITE_crate_red;    break;
                    case ITEM_GREEN:  sprite = SPRITE_crate_green;  break;
                    case ITEM_BLUE:   sprite = SPRITE_crate_blue;   break;
                    case ITEM_ORANGE: sprite = SPRITE_crate_orange; break;
                    default:          sprite = SPRITE_apple;        break;
                }

                int visibleCount = count > 5 ? 5 : count;
                float itemSize = size * ITEM_SIZE_STOCKPILE;
                float stackOffset = size * 0.08f;

                for (int s = 0; s < visibleCount; s++) {
                    float itemX = sx + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                    float itemY = sy + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                    Rectangle srcItem = SpriteGetRect(sprite);
                    Rectangle destItem = { itemX, itemY, itemSize, itemSize };
                    DrawTexturePro(atlas, srcItem, destItem, (Vector2){0, 0}, 0, WHITE);
                }
            }
        }
    }
}

void DrawHaulDestinations(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    // Iterate through active jobs looking for haul jobs with a destination
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_HAUL) continue;
        if (job->targetStockpile < 0) continue;
        
        Stockpile* sp = &stockpiles[job->targetStockpile];
        if (!sp->active) continue;
        if (sp->z != viewZ) continue;
        
        // targetSlotX/Y are already world coordinates
        float sx = offset.x + job->targetSlotX * size;
        float sy = offset.y + job->targetSlotY * size;
        
        // Draw stockpile checkerboard pattern tinted to show incoming item
        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 100, 180});
    }
}

void DrawMiningDesignations(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            Designation* d = GetDesignation(x, y, viewZ);
            if (!d || d->type != DESIGNATION_DIG) continue;

            float sx = offset.x + x * size;
            float sy = offset.y + y * size;

            // Draw stockpile tile pattern tinted cyan for mining
            Rectangle src = SpriteGetRect(SPRITE_stockpile);
            Rectangle dest = { sx, sy, size, size };
            DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){100, 220, 255, 200});

            // If being worked on, draw progress bar
            if (d->progress > 0.0f) {
                float barWidth = size * 0.8f;
                float barHeight = 4.0f;
                float barX = sx + size * 0.1f;
                float barY = sy + size - 8.0f;
                DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
                DrawRectangle((int)barX, (int)barY, (int)(barWidth * d->progress), (int)barHeight, SKYBLUE);
            }
        }
    }

    // Highlight designations that have an active dig job (mover assigned)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_DIG) continue;
        if (job->targetDigZ != viewZ) continue;

        float sx = offset.x + job->targetDigX * size;
        float sy = offset.y + job->targetDigY * size;

        // Orange tint to show "mover incoming"
        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 100, 180});
    }
}

void DrawBlueprints(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        Blueprint* bp = &blueprints[i];
        if (!bp->active || bp->z != viewZ) continue;

        float sx = offset.x + bp->x * size;
        float sy = offset.y + bp->y * size;

        // Draw stockpile tile pattern tinted blue for blueprints
        // Color varies by state: blue (awaiting), cyan (ready), green (building)
        Color tint;
        if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
            tint = (Color){100, 150, 255, 200};  // Blue
        } else if (bp->state == BLUEPRINT_READY_TO_BUILD) {
            tint = (Color){100, 220, 255, 200};  // Cyan
        } else {
            tint = (Color){100, 255, 150, 200};  // Green
        }

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);

        // If being built, draw progress bar
        if (bp->state == BLUEPRINT_BUILDING && bp->progress > 0.0f) {
            float barWidth = size * 0.8f;
            float barHeight = 4.0f;
            float barX = sx + size * 0.1f;
            float barY = sy + size - 8.0f;
            DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
            DrawRectangle((int)barX, (int)barY, (int)(barWidth * bp->progress), (int)barHeight, GREEN);
        }

        // Show material count if awaiting
        if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
            const char* text = TextFormat("%d/%d", bp->deliveredMaterials, bp->requiredMaterials);
            int textW = MeasureTextUI(text, 10);
            DrawTextShadow(text, (int)(sx + size/2 - textW/2), (int)(sy + 2), 10, WHITE);
        }
    }

    // Highlight blueprints that have an active job (haul-to-blueprint or build)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_HAUL_TO_BLUEPRINT && job->type != JOBTYPE_BUILD) continue;
        if (job->targetBlueprint < 0 || job->targetBlueprint >= MAX_BLUEPRINTS) continue;
        
        Blueprint* bp = &blueprints[job->targetBlueprint];
        if (!bp->active || bp->z != viewZ) continue;

        float sx = offset.x + bp->x * size;
        float sy = offset.y + bp->y * size;

        // Orange tint to show "mover incoming"
        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 100, 180});
    }
}

// Helper: spawn a 3x3 stockpile with specified type filters
static void SpawnStockpileWithFilters(bool allowRed, bool allowGreen, bool allowBlue) {
    int attempts = 100;
    while (attempts-- > 0) {
        int gx = GetRandomValue(0, gridWidth - 4);
        int gy = GetRandomValue(0, gridHeight - 4);
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
void DrawStockpileTooltip(int spIdx, Vector2 mouse, Vector2 mouseGrid) {
    if (spIdx < 0 || spIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[spIdx];
    if (!sp->active) return;

    // Count items and capacity (only active cells)
    int totalItems = 0;
    int totalSlots = sp->width * sp->height;
    int activeCells = 0;
    for (int i = 0; i < totalSlots; i++) {
        if (sp->cells[i]) {
            activeCells++;
            totalItems += sp->slotCounts[i];
        }
    }
    int maxCapacity = activeCells * sp->maxStackSize;

    // Get hovered cell info within stockpile
    int cellX = (int)mouseGrid.x;
    int cellY = (int)mouseGrid.y;
    int localX = cellX - sp->x;
    int localY = cellY - sp->y;
    int slotIdx = localY * sp->width + localX;
    int cellCount = (slotIdx >= 0 && slotIdx < totalSlots) ? sp->slotCounts[slotIdx] : 0;

    // Build tooltip text
    const char* titleText = TextFormat("Stockpile #%d", spIdx);
    const char* priorityText = TextFormat("Priority: %d", sp->priority);
    const char* stackText = TextFormat("Stack size: %d", sp->maxStackSize);
    const char* storageText = TextFormat("Storage: %d/%d (%d cells)", totalItems, maxCapacity, activeCells);
    const char* cellText = TextFormat("Cell (%d,%d): %d/%d items", cellX, cellY, cellCount, sp->maxStackSize);
    const char* helpText = "+/- priority, [/] stack, R/G/B filter";

    // Measure text
    int w0 = MeasureText(titleText, 14);
    int w1 = MeasureText(priorityText, 14);
    int w2 = MeasureText(stackText, 14);
    int w3 = MeasureText(storageText, 14);
    int w4 = MeasureText(cellText, 14);
    int w5 = MeasureText("Filters: R G B O", 14);
    int w6 = MeasureText(helpText, 12);
    int maxW = w0;
    if (w1 > maxW) maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;
    if (w6 > maxW) maxW = w6;

    int padding = 6;
    int boxW = maxW + padding * 2;
    int boxH = 14 * 6 + 12 + padding * 2 + 10;  // 7 lines + padding + spacing

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
    DrawTextShadow(titleText, tx + padding, y, 14, YELLOW);
    y += 16;

    DrawTextShadow(priorityText, tx + padding, y, 14, WHITE);
    y += 16;

    DrawTextShadow(stackText, tx + padding, y, 14, WHITE);
    y += 16;

    // Storage line - red if overfull
    bool overfull = totalItems > maxCapacity;
    DrawTextShadow(storageText, tx + padding, y, 14, overfull ? RED : WHITE);
    y += 16;

    // Cell info - highlight if cell is full
    bool cellFull = cellCount >= sp->maxStackSize;
    DrawTextShadow(cellText, tx + padding, y, 14, cellFull ? ORANGE : WHITE);
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
    fx += MeasureText("B", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_ORANGE] ? "O" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_ORANGE] ? ORANGE : DARKGRAY);
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

    // Get job info from Job struct
    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    // Job type names
    const char* jobTypeNames[] = {"NONE", "HAUL", "CLEAR", "DIG", "HAUL_TO_BP", "BUILD"};
    const char* jobTypeName = job ? (job->type < 6 ? jobTypeNames[job->type] : "?") : "IDLE";

    // Extract job data
    int carryingItem = job ? job->carryingItem : -1;
    int targetStockpile = job ? job->targetStockpile : -1;
    int targetSlotX = job ? job->targetSlotX : -1;
    int targetSlotY = job ? job->targetSlotY : -1;
    int targetItem = job ? job->targetItem : -1;
    int jobStep = job ? job->step : 0;

    // Build tooltip lines
    char line1[64], line2[64], line3[64], line4[64], line5[64], line6[64];
    char line7[64] = "";  // Debug: distance to target item
    char line8[64] = "";  // Debug: pickup radius info
    snprintf(line1, sizeof(line1), "Mover #%d", moverIdx);
    snprintf(line2, sizeof(line2), "Pos: (%.1f, %.1f, %.0f)", m->x, m->y, m->z);
    snprintf(line3, sizeof(line3), "Job: %s (step %d)", jobTypeName, jobStep);
    snprintf(line4, sizeof(line4), "Carrying: %s", carryingItem >= 0 ? TextFormat("#%d", carryingItem) : "none");
    snprintf(line5, sizeof(line5), "Path: %d/%d, Goal: (%d,%d)",
        m->pathIndex >= 0 ? m->pathIndex + 1 : 0, m->pathLength, m->goal.x, m->goal.y);
    snprintf(line6, sizeof(line6), "Target SP: %d, Slot: (%d,%d)",
        targetStockpile, targetSlotX, targetSlotY);

    // Debug info when moving to item (step 0 of haul/clear/haul_to_bp jobs)
    int numLines = 6;
    float pickupRadius = CELL_SIZE * 0.75f;
    if (job && jobStep == 0 && targetItem >= 0 && items[targetItem].active) {
        Item* item = &items[targetItem];
        float dx = m->x - item->x;
        float dy = m->y - item->y;
        float dist = sqrtf(dx*dx + dy*dy);
        snprintf(line7, sizeof(line7), "Item #%d at (%.1f,%.1f) dist=%.1f",
            targetItem, item->x, item->y, dist);
        snprintf(line8, sizeof(line8), "Pickup radius: %.1f %s",
            pickupRadius, dist < pickupRadius ? "IN RANGE" : "OUT OF RANGE");
        numLines = 8;
    }

    // Measure text widths
    int w1 = MeasureText(line1, 14);
    int w2 = MeasureText(line2, 14);
    int w3 = MeasureText(line3, 14);
    int w4 = MeasureText(line4, 14);
    int w5 = MeasureText(line5, 14);
    int w6 = MeasureText(line6, 14);
    int w7 = MeasureText(line7, 14);
    int w8 = MeasureText(line8, 14);
    int maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;
    if (w6 > maxW) maxW = w6;
    if (w7 > maxW) maxW = w7;
    if (w8 > maxW) maxW = w8;

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * numLines + padding * 2;

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
    DrawTextShadow(line3, tx + padding, y, 14, !job ? GRAY : GREEN);
    y += lineH;
    DrawTextShadow(line4, tx + padding, y, 14, carryingItem >= 0 ? ORANGE : GRAY);
    y += lineH;
    DrawTextShadow(line5, tx + padding, y, 14, m->pathLength > 0 ? WHITE : RED);
    y += lineH;
    DrawTextShadow(line6, tx + padding, y, 14, targetStockpile >= 0 ? WHITE : GRAY);

    // Debug lines for moving-to-item state
    if (numLines == 8 && targetItem >= 0 && items[targetItem].active) {
        y += lineH;
        DrawTextShadow(line7, tx + padding, y, 14, SKYBLUE);
        y += lineH;
        // Color based on whether in range
        float dx = m->x - items[targetItem].x;
        float dy = m->y - items[targetItem].y;
        float dist = sqrtf(dx*dx + dy*dy);
        DrawTextShadow(line8, tx + padding, y, 14, dist < pickupRadius ? GREEN : RED);
    }
}

// Get items at a grid cell, returns count and fills outItems array (max 16)
int GetItemsAtCell(int cellX, int cellY, int cellZ, int* outItems, int maxItems) {
    int count = 0;
    for (int i = 0; i < MAX_ITEMS && count < maxItems; i++) {
        if (!items[i].active) continue;
        int ix = (int)(items[i].x / CELL_SIZE);
        int iy = (int)(items[i].y / CELL_SIZE);
        int iz = (int)(items[i].z);
        if (ix == cellX && iy == cellY && iz == cellZ) {
            outItems[count++] = i;
        }
    }
    return count;
}

// Draw item tooltip (only shown when paused)
void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY) {
    if (itemCount <= 0) return;

    const char* typeNames[] = {"Red", "Green", "Blue"};
    const char* stateNames[] = {"Ground", "Carried", "Stockpile"};

    // Build tooltip lines
    char lines[17][64];  // Header + up to 16 items
    snprintf(lines[0], sizeof(lines[0]), "Cell (%d,%d): %d item%s", cellX, cellY, itemCount, itemCount == 1 ? "" : "s");

    int lineCount = 1;
    for (int i = 0; i < itemCount && lineCount < 17; i++) {
        int idx = itemIndices[i];
        Item* item = &items[idx];
        const char* typeName = (item->type >= 0 && item->type < 3) ? typeNames[item->type] : "?";
        const char* stateName = (item->state >= 0 && item->state < 3) ? stateNames[item->state] : "?";
        snprintf(lines[lineCount], sizeof(lines[lineCount]), "#%d: %s (%s)", idx, typeName, stateName);
        lineCount++;
    }

    // Measure max width
    int maxW = 0;
    for (int i = 0; i < lineCount; i++) {
        int w = MeasureText(lines[i], 14);
        if (w > maxW) maxW = w;
    }

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * lineCount + padding * 2;

    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){40, 30, 20, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){150, 100, 50, 255});

    // Draw text
    int y = ty + padding;
    DrawTextShadow(lines[0], tx + padding, y, 14, YELLOW);
    y += lineH;

    for (int i = 1; i < lineCount; i++) {
        int idx = itemIndices[i - 1];
        Color col = WHITE;
        if (items[idx].type == ITEM_RED) col = RED;
        else if (items[idx].type == ITEM_GREEN) col = GREEN;
        else if (items[idx].type == ITEM_BLUE) col = (Color){100, 150, 255, 255};
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}

// Draw water tooltip when hovering over water
void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    WaterCell* cell = &waterGrid[cellZ][cellY][cellX];
    if (cell->level == 0 && !cell->isSource && !cell->isDrain) return;

    // Build tooltip lines
    char lines[8][64];
    int lineCount = 0;

    snprintf(lines[lineCount++], sizeof(lines[0]), "Water (%d,%d,z%d)", cellX, cellY, cellZ);
    snprintf(lines[lineCount++], sizeof(lines[0]), "Level: %d/7", cell->level);
    
    float speedMult = GetWaterSpeedMultiplier(cellX, cellY, cellZ);
    snprintf(lines[lineCount++], sizeof(lines[0]), "Speed: %.0f%%", speedMult * 100);
    
    if (cell->isSource) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "SOURCE");
    }
    if (cell->isDrain) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "DRAIN");
    }
    if (cell->hasPressure) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Pressure (src z=%d)", cell->pressureSourceZ);
        snprintf(lines[lineCount++], sizeof(lines[0]), "Max rise: z=%d", cell->pressureSourceZ - 1);
    }
    if (cell->stable) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "[stable]");
    }

    // Measure max width
    int maxW = 0;
    for (int i = 0; i < lineCount; i++) {
        int w = MeasureText(lines[i], 14);
        if (w > maxW) maxW = w;
    }

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * lineCount + padding * 2;

    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    // Draw background (blue tint for water)
    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 40, 60, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){50, 100, 200, 255});

    // Draw text
    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        Color col = WHITE;
        if (i == 0) col = (Color){100, 180, 255, 255};  // Header
        else if (strstr(lines[i], "SOURCE")) col = (Color){150, 220, 255, 255};
        else if (strstr(lines[i], "DRAIN")) col = (Color){80, 80, 120, 255};
        else if (strstr(lines[i], "Pressure")) col = YELLOW;
        else if (strstr(lines[i], "stable")) col = GRAY;
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
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

    // Update item hover state (only when paused)
    if (paused) {
        int cellX = (int)mouseGrid.x;
        int cellY = (int)mouseGrid.y;
        hoveredItemCount = GetItemsAtCell(cellX, cellY, currentViewZ, hoveredItemCell, 16);
    } else {
        hoveredItemCount = 0;
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
            return;  // Don't trigger Z-layer change
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            int newSize = sp->maxStackSize - 1;
            if (newSize >= 1) {
                SetStockpileMaxStackSize(hoveredStockpile, newSize);
                AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE);
            }
            return;  // Don't trigger Z-layer change
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
        if (IsKeyPressed(KEY_O)) {
            sp->allowedTypes[ITEM_ORANGE] = !sp->allowedTypes[ITEM_ORANGE];
            AddMessage(TextFormat("Orange filter: %s", sp->allowedTypes[ITEM_ORANGE] ? "ON" : "OFF"), ORANGE);
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

    // Floor/Tile drawing mode (T key + drag) - fills entire area with floor
    if (IsKeyDown(KEY_T)) {
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

    // Stockpile drawing mode (S key + left-drag to draw, right-drag to erase)
    if (IsKeyDown(KEY_S)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - draw stockpile
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingStockpile = true;
            stockpileStartX = x;
            stockpileStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingStockpile) {
            drawingStockpile = false;
            int x1 = stockpileStartX < x ? stockpileStartX : x;
            int y1 = stockpileStartY < y ? stockpileStartY : y;
            int x2 = stockpileStartX > x ? stockpileStartX : x;
            int y2 = stockpileStartY > y ? stockpileStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int width = x2 - x1 + 1;
            int height = y2 - y1 + 1;

            // Clamp to MAX_STOCKPILE_SIZE
            if (width > MAX_STOCKPILE_SIZE) width = MAX_STOCKPILE_SIZE;
            if (height > MAX_STOCKPILE_SIZE) height = MAX_STOCKPILE_SIZE;

            if (width > 0 && height > 0) {
                // Remove overlapping cells from existing stockpiles first
                for (int i = 0; i < MAX_STOCKPILES; i++) {
                    if (!stockpiles[i].active || stockpiles[i].z != z) continue;
                    RemoveStockpileCells(i, x1, y1, x2, y2);
                }

                int idx = CreateStockpile(x1, y1, z, width, height);
                if (idx >= 0) {
                    AddMessage(TextFormat("Created stockpile %d (%dx%d)", idx, width, height), GREEN);
                } else {
                    AddMessage(TextFormat("Failed to create stockpile (max %d)", MAX_STOCKPILES), RED);
                }
            }
        }

        // Right mouse - erase stockpiles in area
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            erasingStockpile = true;
            stockpileStartX = x;
            stockpileStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && erasingStockpile) {
            erasingStockpile = false;
            int x1 = stockpileStartX < x ? stockpileStartX : x;
            int y1 = stockpileStartY < y ? stockpileStartY : y;
            int x2 = stockpileStartX > x ? stockpileStartX : x;
            int y2 = stockpileStartY > y ? stockpileStartY : y;

            // Remove cells from any stockpiles that overlap with the erase area
            int erasedCells = 0;
            for (int i = MAX_STOCKPILES - 1; i >= 0; i--) {
                if (!stockpiles[i].active || stockpiles[i].z != z) continue;

                int sx1 = stockpiles[i].x;
                int sy1 = stockpiles[i].y;
                int sx2 = sx1 + stockpiles[i].width - 1;
                int sy2 = sy1 + stockpiles[i].height - 1;

                // Check overlap
                if (x1 <= sx2 && x2 >= sx1 && y1 <= sy2 && y2 >= sy1) {
                    int beforeCount = GetStockpileActiveCellCount(i);
                    RemoveStockpileCells(i, x1, y1, x2, y2);
                    int afterCount = GetStockpileActiveCellCount(i);
                    erasedCells += beforeCount - afterCount;
                }
            }
            if (erasedCells > 0) {
                AddMessage(TextFormat("Erased %d cell%s", erasedCells, erasedCells > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while S is held
    } else {
        drawingStockpile = false;
        erasingStockpile = false;
    }

    // Gather zone mode (G key + left-drag to draw, right-drag to erase)
    if (IsKeyDown(KEY_G)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - draw gather zone
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingGatherZone = true;
            gatherZoneStartX = x;
            gatherZoneStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingGatherZone) {
            drawingGatherZone = false;
            int x1 = gatherZoneStartX < x ? gatherZoneStartX : x;
            int y1 = gatherZoneStartY < y ? gatherZoneStartY : y;
            int x2 = gatherZoneStartX > x ? gatherZoneStartX : x;
            int y2 = gatherZoneStartY > y ? gatherZoneStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int width = x2 - x1 + 1;
            int height = y2 - y1 + 1;

            int idx = CreateGatherZone(x1, y1, z, width, height);
            if (idx >= 0) {
                AddMessage(TextFormat("Created gather zone %dx%d", width, height), ORANGE);
            } else {
                AddMessage("Max gather zones reached!", RED);
            }
        }

        // Right mouse - erase gather zones
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            erasingGatherZone = true;
            gatherZoneStartX = x;
            gatherZoneStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && erasingGatherZone) {
            erasingGatherZone = false;
            int x1 = gatherZoneStartX < x ? gatherZoneStartX : x;
            int y1 = gatherZoneStartY < y ? gatherZoneStartY : y;
            int x2 = gatherZoneStartX > x ? gatherZoneStartX : x;
            int y2 = gatherZoneStartY > y ? gatherZoneStartY : y;

            // Delete any gather zones that overlap with the erase area
            int deleted = 0;
            for (int i = MAX_GATHER_ZONES - 1; i >= 0; i--) {
                if (!gatherZones[i].active || gatherZones[i].z != z) continue;

                int gx1 = gatherZones[i].x;
                int gy1 = gatherZones[i].y;
                int gx2 = gx1 + gatherZones[i].width - 1;
                int gy2 = gy1 + gatherZones[i].height - 1;

                // Check overlap
                if (x1 <= gx2 && x2 >= gx1 && y1 <= gy2 && y2 >= gy1) {
                    DeleteGatherZone(i);
                    deleted++;
                }
            }
            if (deleted > 0) {
                AddMessage(TextFormat("Deleted %d gather zone%s", deleted, deleted > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while G is held
    } else {
        drawingGatherZone = false;
        erasingGatherZone = false;
    }

    // Mining designation mode (M key + left-drag to designate, right-drag to cancel)
    if (IsKeyDown(KEY_M)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - designate for mining
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            designatingMining = true;
            miningStartX = x;
            miningStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && designatingMining) {
            designatingMining = false;
            int x1 = miningStartX < x ? miningStartX : x;
            int y1 = miningStartY < y ? miningStartY : y;
            int x2 = miningStartX > x ? miningStartX : x;
            int y2 = miningStartY > y ? miningStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int designated = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (DesignateDig(dx, dy, z)) {
                        designated++;
                    }
                }
            }
            if (designated > 0) {
                AddMessage(TextFormat("Designated %d cell%s for mining", designated, designated > 1 ? "s" : ""), ORANGE);
            }
        }

        // Right mouse - cancel mining designations in area
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            cancellingMining = true;
            miningStartX = x;
            miningStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && cancellingMining) {
            cancellingMining = false;
            int x1 = miningStartX < x ? miningStartX : x;
            int y1 = miningStartY < y ? miningStartY : y;
            int x2 = miningStartX > x ? miningStartX : x;
            int y2 = miningStartY > y ? miningStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int cancelled = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (HasDigDesignation(dx, dy, z)) {
                        CancelDesignation(dx, dy, z);
                        cancelled++;
                    }
                }
            }
            if (cancelled > 0) {
                AddMessage(TextFormat("Cancelled %d mining designation%s", cancelled, cancelled > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while M is held
    } else {
        designatingMining = false;
        cancellingMining = false;
    }

    // Build designation mode (B key + left-drag to designate, right-drag to cancel)
    if (IsKeyDown(KEY_B)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - designate for building
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            designatingBuild = true;
            buildStartX = x;
            buildStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && designatingBuild) {
            designatingBuild = false;
            int x1 = buildStartX < x ? buildStartX : x;
            int y1 = buildStartY < y ? buildStartY : y;
            int x2 = buildStartX > x ? buildStartX : x;
            int y2 = buildStartY > y ? buildStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int designated = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (CreateBuildBlueprint(dx, dy, z) >= 0) {
                        designated++;
                    }
                }
            }
            if (designated > 0) {
                AddMessage(TextFormat("Created %d blueprint%s for building", designated, designated > 1 ? "s" : ""), BLUE);
            }
        }

        // Right mouse - cancel build blueprints in area
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            cancellingBuild = true;
            buildStartX = x;
            buildStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && cancellingBuild) {
            cancellingBuild = false;
            int x1 = buildStartX < x ? buildStartX : x;
            int y1 = buildStartY < y ? buildStartY : y;
            int x2 = buildStartX > x ? buildStartX : x;
            int y2 = buildStartY > y ? buildStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int cancelled = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    int bpIdx = GetBlueprintAt(dx, dy, z);
                    if (bpIdx >= 0) {
                        CancelBlueprint(bpIdx);
                        cancelled++;
                    }
                }
            }
            if (cancelled > 0) {
                AddMessage(TextFormat("Cancelled %d blueprint%s", cancelled, cancelled > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while B is held
    } else {
        designatingBuild = false;
        cancellingBuild = false;
    }

    // Water placement mode (W key)
    // W + left-drag = place water 7/7
    // W + right-drag = remove water
    // W + Shift + left-drag = place water source
    // W + Shift + right-drag = place drain
    if (IsKeyDown(KEY_W)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // Left mouse - place water or source
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            placingWaterSource = true;
            waterStartX = x;
            waterStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && placingWaterSource) {
            placingWaterSource = false;
            int x1 = waterStartX < x ? waterStartX : x;
            int y1 = waterStartY < y ? waterStartY : y;
            int x2 = waterStartX > x ? waterStartX : x;
            int y2 = waterStartY > y ? waterStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int placed = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + left = water source
                        SetWaterSource(dx, dy, z, true);
                    }
                    SetWaterLevel(dx, dy, z, WATER_MAX_LEVEL);
                    placed++;
                }
            }
            if (placed > 0) {
                if (shift) {
                    AddMessage(TextFormat("Placed %d water source%s", placed, placed > 1 ? "s" : ""), BLUE);
                } else {
                    AddMessage(TextFormat("Placed water (7/7) in %d cell%s", placed, placed > 1 ? "s" : ""), SKYBLUE);
                }
            }
        }

        // Right mouse - remove water or place drain
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            placingWaterDrain = true;
            waterStartX = x;
            waterStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && placingWaterDrain) {
            placingWaterDrain = false;
            int x1 = waterStartX < x ? waterStartX : x;
            int y1 = waterStartY < y ? waterStartY : y;
            int x2 = waterStartX > x ? waterStartX : x;
            int y2 = waterStartY > y ? waterStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int placedDrains = 0;
            int removedSources = 0;
            int removedDrains = 0;
            int removedWater = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + right = place drain
                        SetWaterDrain(dx, dy, z, true);
                        placedDrains++;
                    } else {
                        // Right = remove in priority: source -> drain -> water
                        WaterCell* cell = &waterGrid[z][dy][dx];
                        if (cell->isSource) {
                            SetWaterSource(dx, dy, z, false);
                            removedSources++;
                        } else if (cell->isDrain) {
                            SetWaterDrain(dx, dy, z, false);
                            removedDrains++;
                        } else if (cell->level > 0) {
                            SetWaterLevel(dx, dy, z, 0);
                            removedWater++;
                        }
                    }
                }
            }
            if (placedDrains > 0) {
                AddMessage(TextFormat("Placed %d drain%s", placedDrains, placedDrains > 1 ? "s" : ""), DARKBLUE);
            }
            if (removedSources > 0) {
                AddMessage(TextFormat("Removed %d source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
            }
            if (removedDrains > 0) {
                AddMessage(TextFormat("Removed %d drain%s", removedDrains, removedDrains > 1 ? "s" : ""), ORANGE);
            }
            if (removedWater > 0) {
                AddMessage(TextFormat("Removed water from %d cell%s", removedWater, removedWater > 1 ? "s" : ""), GRAY);
            }
        }

        return;  // Skip normal tool interactions while W is held
    } else {
        placingWaterSource = false;
        placingWaterDrain = false;
    }

    // Fire placement mode (F key)
    // F + left-drag = place fire (ignite at level 7)
    // F + right-drag = extinguish fire
    // F + Shift + left-drag = place fire source (permanent)
    // F + Shift + right-drag = remove fire source
    if (IsKeyDown(KEY_F)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // Left mouse - place fire or source
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            placingFireSource = true;
            fireStartX = x;
            fireStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && placingFireSource) {
            placingFireSource = false;
            int x1 = fireStartX < x ? fireStartX : x;
            int y1 = fireStartY < y ? fireStartY : y;
            int x2 = fireStartX > x ? fireStartX : x;
            int y2 = fireStartY > y ? fireStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int placed = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + left = fire source
                        SetFireSource(dx, dy, z, true);
                        placed++;
                    } else {
                        // Normal left = ignite if flammable
                        if (GetBaseFuelForCellType(grid[z][dy][dx]) > 0 && 
                            !HAS_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED)) {
                            IgniteCell(dx, dy, z);
                            placed++;
                        }
                    }
                }
            }
            if (placed > 0) {
                if (shift) {
                    AddMessage(TextFormat("Placed %d fire source%s", placed, placed > 1 ? "s" : ""), RED);
                } else {
                    AddMessage(TextFormat("Ignited %d cell%s", placed, placed > 1 ? "s" : ""), ORANGE);
                }
            }
        }

        // Right mouse - extinguish fire or remove source
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            extinguishingFire = true;
            fireStartX = x;
            fireStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && extinguishingFire) {
            extinguishingFire = false;
            int x1 = fireStartX < x ? fireStartX : x;
            int y1 = fireStartY < y ? fireStartY : y;
            int x2 = fireStartX > x ? fireStartX : x;
            int y2 = fireStartY > y ? fireStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int removedSources = 0;
            int extinguished = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    FireCell* cell = &fireGrid[z][dy][dx];
                    if (shift) {
                        // Shift + right = remove fire source
                        if (cell->isSource) {
                            SetFireSource(dx, dy, z, false);
                            removedSources++;
                        }
                    } else {
                        // Right = extinguish fire
                        if (cell->isSource) {
                            SetFireSource(dx, dy, z, false);
                            removedSources++;
                        }
                        if (cell->level > 0) {
                            ExtinguishCell(dx, dy, z);
                            extinguished++;
                        }
                    }
                }
            }
            if (removedSources > 0) {
                AddMessage(TextFormat("Removed %d fire source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
            }
            if (extinguished > 0) {
                AddMessage(TextFormat("Extinguished %d cell%s", extinguished, extinguished > 1 ? "s" : ""), GRAY);
            }
        }

        return;  // Skip normal tool interactions while F is held
    } else {
        placingFireSource = false;
        extinguishingFire = false;
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
                        // Clear water in this cell and destabilize neighbors
                        SetWaterLevel(x, y, z, 0);
                        SetWaterSource(x, y, z, false);
                        SetWaterDrain(x, y, z, false);
                        DestabilizeWater(x, y, z);
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
                            // Wake up water in this cell and neighbors (wall removal)
                            DestabilizeWater(x, y, z);
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
                    // Wake up water in this cell and neighbors (wall removal)
                    DestabilizeWater(x, y, z);
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

    // Z-level switching (, and . keys)
    if (IsKeyPressed(KEY_PERIOD)) {  // .
        if (currentViewZ < gridDepth - 1) currentViewZ++;
    }
    if (IsKeyPressed(KEY_COMMA)) {  // ,
        if (currentViewZ > 0) currentViewZ--;
    }

    // Pause toggle (Space)
    if (IsKeyPressed(KEY_SPACE)) {
        paused = !paused;
    }
    
    // Quick save (F5)
    if (IsKeyPressed(KEY_F5)) {
        if (SaveWorld("debug_save.bin")) {
            AddMessage("World saved to debug_save.bin", GREEN);
            
            // Also archive a gzipped copy with timestamp
            system("mkdir -p saves");
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char cmd[256];
            snprintf(cmd, sizeof(cmd), 
                "gzip -c debug_save.bin > saves/%04d-%02d-%02d_%02d-%02d-%02d.bin.gz",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
            system(cmd);
        }
    }
    
    // Quick load (F6)
    if (IsKeyPressed(KEY_F6)) {
        if (LoadWorld("debug_save.bin")) {
            AddMessage("World loaded from debug_save.bin", GREEN);
        }
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
            InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
            BuildEntrances();
            BuildGraph();
            offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
            offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
        }
        y += 22;
        if (PushButton(x, y, "Medium Grid (64x64)")) {
            InitGridWithSizeAndChunkSize(64, 64, 8, 8);
            gridDepth = 6;
            // Set z>0 to air (z=0 already walkable from init)
            for (int z = 1; z < gridDepth; z++)
                for (int gy = 0; gy < gridHeight; gy++)
                    for (int gx = 0; gx < gridWidth; gx++)
                        grid[z][gy][gx] = CELL_AIR;
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
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
            InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
            BuildEntrances();
            BuildGraph();
            offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
            offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
        }
        y += 22;
        if (PushButton(x, y, "Fill with Walls")) {
            // Fill current z-level with walls (for mining testing)
            for (int gy = 0; gy < gridHeight; gy++) {
                for (int gx = 0; gx < gridWidth; gx++) {
                    grid[currentViewZ][gy][gx] = CELL_WALL;
                    // Clear water in filled cells
                    SetWaterLevel(gx, gy, currentViewZ, 0);
                    SetWaterSource(gx, gy, currentViewZ, false);
                    SetWaterDrain(gx, gy, currentViewZ, false);
                }
            }
            // Mark all chunks dirty
            for (int cy = 0; cy < chunksY; cy++) {
                for (int cx = 0; cx < chunksX; cx++) {
                    MarkChunkDirty(cx * chunkWidth, cy * chunkHeight, currentViewZ);
                }
            }
            BuildEntrances();
            BuildGraph();
            InitDesignations();  // Clear any existing designations
            AddMessage(TextFormat("Filled z=%d with walls", currentViewZ), GREEN);
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

            AddMessage(TextFormat("Map copied to clipboard (%d floors)", gridDepth),ORANGE);

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
                    int gx = GetRandomValue(0, gridWidth - 1);
                    int gy = GetRandomValue(0, gridHeight - 1);
                    if (IsCellWalkableAt(currentViewZ, gy, gx)) {
                        float px = gx * CELL_SIZE + CELL_SIZE * 0.5f;
                        float py = gy * CELL_SIZE + CELL_SIZE * 0.5f;
                        ItemType type = GetRandomValue(0, 2);  // ITEM_RED, ITEM_GREEN, or ITEM_BLUE
                        SpawnItem(px, py, (float)currentViewZ, type);
                        break;
                    }
                }
            }
        }
        y += 22;
        if (PushButton(x, y, "Spawn Red")) {
            for (int i = 0; i < itemCountSetting; i++) {
                int attempts = 100;
                while (attempts-- > 0) {
                    int gx = GetRandomValue(0, gridWidth - 1);
                    int gy = GetRandomValue(0, gridHeight - 1);
                    if (IsCellWalkableAt(currentViewZ, gy, gx)) {
                        float px = gx * CELL_SIZE + CELL_SIZE * 0.5f;
                        float py = gy * CELL_SIZE + CELL_SIZE * 0.5f;
                        SpawnItem(px, py, (float)currentViewZ, ITEM_RED);
                        break;
                    }
                }
            }
        }
        y += 22;
        if (PushButton(x, y, "Spawn Green")) {
            for (int i = 0; i < itemCountSetting; i++) {
                int attempts = 100;
                while (attempts-- > 0) {
                    int gx = GetRandomValue(0, gridWidth - 1);
                    int gy = GetRandomValue(0, gridHeight - 1);
                    if (IsCellWalkableAt(currentViewZ, gy, gx)) {
                        float px = gx * CELL_SIZE + CELL_SIZE * 0.5f;
                        float py = gy * CELL_SIZE + CELL_SIZE * 0.5f;
                        SpawnItem(px, py, (float)currentViewZ, ITEM_GREEN);
                        break;
                    }
                }
            }
        }
        y += 22;
        if (PushButton(x, y, "Spawn Blue")) {
            for (int i = 0; i < itemCountSetting; i++) {
                int attempts = 100;
                while (attempts-- > 0) {
                    int gx = GetRandomValue(0, gridWidth - 1);
                    int gy = GetRandomValue(0, gridHeight - 1);
                    if (IsCellWalkableAt(currentViewZ, gy, gx)) {
                        float px = gx * CELL_SIZE + CELL_SIZE * 0.5f;
                        float py = gy * CELL_SIZE + CELL_SIZE * 0.5f;
                        SpawnItem(px, py, (float)currentViewZ, ITEM_BLUE);
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

    // === WATER ===
    y += 8;
    if (SectionHeader(x, y, "Water", &sectionWater)) {
        y += 18;
        ToggleBool(x, y, "Enabled", &waterEnabled);
        y += 22;
        ToggleBool(x, y, "Evaporation", &waterEvaporationEnabled);
        y += 22;
        DraggableInt(x, y, "Evap Rate (1/N)", &waterEvapChance, 1.0f, 1, 1000);
        y += 22;
        if (PushButton(x, y, "Clear Water")) {
            ClearWater();
        }
    }
    y += 22;

    // === FIRE ===
    y += 8;
    if (SectionHeader(x, y, "Fire", &sectionFire)) {
        y += 18;
        ToggleBool(x, y, "Enabled", &fireEnabled);
        y += 22;
        DraggableInt(x, y, "Spread Chance (1/N)", &fireSpreadChance, 1.0f, 1, 50);
        y += 22;
        DraggableInt(x, y, "Fuel Consumption", &fireFuelConsumption, 1.0f, 1, 50);
        y += 22;
        DraggableInt(x, y, "Water Reduction %", &fireWaterReduction, 1.0f, 1, 100);
        y += 22;
        if (PushButton(x, y, "Clear Fire")) {
            ClearFire();
        }
    }
    y += 22;

    // === SMOKE ===
    y += 8;
    if (SectionHeader(x, y, "Smoke", &sectionSmoke)) {
        y += 18;
        ToggleBool(x, y, "Enabled", &smokeEnabled);
        y += 22;
        DraggableInt(x, y, "Rise Chance (1/N)", &smokeRiseChance, 1.0f, 1, 20);
        y += 22;
        DraggableInt(x, y, "Dissipation Rate", &smokeDissipationRate, 1.0f, 1, 100);
        y += 22;
        DraggableInt(x, y, "Generation Rate", &smokeGenerationRate, 1.0f, 1, 10);
        y += 22;
        if (PushButton(x, y, "Clear Smoke")) {
            ClearSmoke();
        }
    }
    y += 22;

    // === ENTROPY (Ground Wear) ===
    y += 8;
    if (SectionHeader(x, y, "Entropy", &sectionEntropy)) {
        y += 18;
        ToggleBool(x, y, "Enabled", &groundWearEnabled);
        y += 22;
        DraggableInt(x, y, "Trample Amount", &wearTrampleAmount, 1.0f, 1, 100);
        y += 22;
        DraggableInt(x, y, "Grass->Dirt Threshold", &wearGrassToDirt, 50.0f, 100, 10000);
        y += 22;
        DraggableInt(x, y, "Dirt->Grass Threshold", &wearDirtToGrass, 50.0f, 0, 5000);
        y += 22;
        DraggableInt(x, y, "Decay Rate", &wearDecayRate, 1.0f, 1, 100);
        y += 22;
        DraggableInt(x, y, "Decay Interval", &wearDecayInterval, 5.0f, 1, 500);
        y += 22;
        if (PushButton(x, y, "Clear Wear")) {
            ClearGroundWear();
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
            // Calculate sizes of major static arrays
            // Grid & terrain
            size_t gridSize = sizeof(CellType) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t designationsSize = sizeof(Designation) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t waterSize = sizeof(WaterCell) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t groundWearSize = sizeof(int) * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            
            // Pathfinding
            size_t entrancesSize = sizeof(Entrance) * MAX_ENTRANCES;
            size_t pathSize = sizeof(Point) * MAX_PATH;
            size_t edgesSize = sizeof(GraphEdge) * MAX_EDGES;
            
            // Entities
            size_t moversSize = sizeof(Mover) * MAX_MOVERS;
            size_t itemsSize = sizeof(Item) * MAX_ITEMS;
            size_t jobsSize = sizeof(Job) * MAX_JOBS;
            size_t stockpilesSize = sizeof(Stockpile) * MAX_STOCKPILES;
            size_t blueprintsSize = sizeof(Blueprint) * MAX_BLUEPRINTS;
            size_t gatherZonesSize = sizeof(GatherZone) * MAX_GATHER_ZONES;
            
            // Spatial grids (heap allocated)
            size_t moverSpatialGrid = (moverGrid.cellCount + 1) * sizeof(int) * 2 + MAX_MOVERS * sizeof(int);
            size_t itemSpatialGrid = (itemGrid.cellCount + 1) * sizeof(int) * 2 + MAX_ITEMS * sizeof(int);
            
            size_t totalGrid = gridSize + designationsSize + waterSize + groundWearSize;
            size_t totalPathfinding = entrancesSize + pathSize + edgesSize;
            size_t totalEntities = moversSize + itemsSize + jobsSize + stockpilesSize + blueprintsSize + gatherZonesSize;
            size_t totalSpatial = moverSpatialGrid + itemSpatialGrid;
            size_t total = totalGrid + totalPathfinding + totalEntities + totalSpatial;

            DrawTextShadow("-- Grid Data --", x, y, 14, GRAY); y += 16;
            DrawTextShadow(TextFormat("  Cells:        %5.1f MB", gridSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Designations: %5.1f MB", designationsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Water:        %5.1f MB", waterSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  GroundWear:   %5.1f MB", groundWearSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            
            DrawTextShadow("-- Pathfinding --", x, y, 14, GRAY); y += 16;
            DrawTextShadow(TextFormat("  Entrances:    %5.1f MB", entrancesSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Path:         %5.1f MB", pathSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Edges:        %5.1f MB", edgesSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            
            DrawTextShadow("-- Entities --", x, y, 14, GRAY); y += 16;
            DrawTextShadow(TextFormat("  Movers:       %5.1f MB", moversSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Items:        %5.1f MB", itemsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Jobs:         %5.1f MB", jobsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Stockpiles:   %5.1f KB", stockpilesSize / 1024.0f), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  Blueprints:   %5.1f KB", blueprintsSize / 1024.0f), x, y, 14, WHITE); y += 16;
            
            DrawTextShadow("-- Spatial --", x, y, 14, GRAY); y += 16;
            DrawTextShadow(TextFormat("  MoverGrid:    %5.1f KB", moverSpatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
            DrawTextShadow(TextFormat("  ItemGrid:     %5.1f KB", itemSpatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
            
            DrawTextShadow(TextFormat("TOTAL:          %5.1f MB", total / (1024.0f * 1024.0f)), x, y, 14, PINK); y += 20;
        }

        // Build hierarchical render order (parents before children, children grouped under parent)
        int renderOrder[PROFILER_MAX_SECTIONS];
        int renderCount = 0;

        // Add section and all its descendants recursively (depth-first)
        for (int i = 0; i < profilerSectionCount; i++) {
            if (profilerSections[i].parent == -1) {
                // Use a stack to avoid actual recursion
                int stack[PROFILER_MAX_SECTIONS];
                int stackSize = 1;
                stack[0] = i;

                while (stackSize > 0) {
                    int current = stack[--stackSize];
                    renderOrder[renderCount++] = current;

                    // Push children in reverse order so they come out in forward order
                    for (int j = profilerSectionCount - 1; j >= 0; j--) {
                        if (profilerSections[j].parent == current) {
                            stack[stackSize++] = j;
                        }
                    }
                }
            }
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
        for (int r = 0; r < renderCount; r++) {
            int i = renderOrder[r];
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

// ============================================================================
// Save/Load World (for debugging)
// ============================================================================

#define SAVE_VERSION 2  // Added fire, smoke, and cellFlags
#define SAVE_MAGIC 0x4E41564B  // "NAVK"

bool SaveWorld(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        AddMessage(TextFormat("Failed to open %s for writing", filename), RED);
        return false;
    }
    
    // Header
    uint32_t magic = SAVE_MAGIC;
    uint32_t version = SAVE_VERSION;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    
    // Grid dimensions
    fwrite(&gridWidth, sizeof(gridWidth), 1, f);
    fwrite(&gridHeight, sizeof(gridHeight), 1, f);
    fwrite(&gridDepth, sizeof(gridDepth), 1, f);
    fwrite(&chunkWidth, sizeof(chunkWidth), 1, f);
    fwrite(&chunkHeight, sizeof(chunkHeight), 1, f);
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(grid[z][y], sizeof(CellType), gridWidth, f);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(waterGrid[z][y], sizeof(WaterCell), gridWidth, f);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(fireGrid[z][y], sizeof(FireCell), gridWidth, f);
        }
    }
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(smokeGrid[z][y], sizeof(SmokeCell), gridWidth, f);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(designations[z][y], sizeof(Designation), gridWidth, f);
        }
    }
    
    // Items
    fwrite(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    fwrite(items, sizeof(Item), itemHighWaterMark, f);
    
    // Stockpiles
    fwrite(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    
    // Gather zones
    fwrite(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fwrite(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fwrite(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Movers
    fwrite(&moverCount, sizeof(moverCount), 1, f);
    fwrite(movers, sizeof(Mover), moverCount, f);
    
    // Jobs
    fwrite(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fwrite(&activeJobCount, sizeof(activeJobCount), 1, f);
    fwrite(jobs, sizeof(Job), jobHighWaterMark, f);
    fwrite(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fwrite(activeJobList, sizeof(int), activeJobCount, f);
    
    // View state
    fwrite(&currentViewZ, sizeof(currentViewZ), 1, f);
    fwrite(&zoom, sizeof(zoom), 1, f);
    fwrite(&offset, sizeof(offset), 1, f);
    
    fclose(f);
    return true;
}

bool LoadWorld(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        AddMessage(TextFormat("Failed to open %s", filename), RED);
        return false;
    }
    
    // Check header
    uint32_t magic, version;
    fread(&magic, sizeof(magic), 1, f);
    fread(&version, sizeof(version), 1, f);
    
    if (magic != SAVE_MAGIC) {
        AddMessage("Invalid save file (bad magic)", RED);
        fclose(f);
        return false;
    }
    
    if (version != SAVE_VERSION) {
        AddMessage(TextFormat("Save version mismatch (file: %d, current: %d)", version, SAVE_VERSION), RED);
        fclose(f);
        return false;
    }
    
    // Grid dimensions
    int newWidth, newHeight, newDepth, newChunkW, newChunkH;
    fread(&newWidth, sizeof(newWidth), 1, f);
    fread(&newHeight, sizeof(newHeight), 1, f);
    fread(&newDepth, sizeof(newDepth), 1, f);
    fread(&newChunkW, sizeof(newChunkW), 1, f);
    fread(&newChunkH, sizeof(newChunkH), 1, f);
    
    // Reinitialize grid if dimensions don't match
    if (newWidth != gridWidth || newHeight != gridHeight || 
        newChunkW != chunkWidth || newChunkH != chunkHeight) {
        InitGridWithSizeAndChunkSize(newWidth, newHeight, newChunkW, newChunkH);
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    }
    gridDepth = newDepth;
    
    // Clear current state
    ClearMovers();
    ClearJobs();
    ClearGatherZones();
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(grid[z][y], sizeof(CellType), gridWidth, f);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(waterGrid[z][y], sizeof(WaterCell), gridWidth, f);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(fireGrid[z][y], sizeof(FireCell), gridWidth, f);
        }
    }
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(smokeGrid[z][y], sizeof(SmokeCell), gridWidth, f);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(designations[z][y], sizeof(Designation), gridWidth, f);
        }
    }
    
    // Items
    fread(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    fread(items, sizeof(Item), itemHighWaterMark, f);
    
    // Stockpiles
    fread(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    
    // Gather zones
    fread(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fread(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fread(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Movers
    fread(&moverCount, sizeof(moverCount), 1, f);
    fread(movers, sizeof(Mover), moverCount, f);
    
    // Jobs
    fread(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fread(&activeJobCount, sizeof(activeJobCount), 1, f);
    fread(jobs, sizeof(Job), jobHighWaterMark, f);
    fread(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fread(activeJobList, sizeof(int), activeJobCount, f);
    
    // View state
    fread(&currentViewZ, sizeof(currentViewZ), 1, f);
    fread(&zoom, sizeof(zoom), 1, f);
    fread(&offset, sizeof(offset), 1, f);
    
    fclose(f);
    
    // Rebuild HPA* graph after loading - mark all chunks dirty
    hpaNeedsRebuild = true;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y += chunkHeight) {
            for (int x = 0; x < gridWidth; x += chunkWidth) {
                MarkChunkDirty(x, y, z);
            }
        }
    }
    
    // Rebuild spatial grids
    BuildMoverSpatialGrid();
    BuildItemSpatialGrid();
    
    return true;
}

int main(int argc, char** argv) {
    // Check for --inspect mode (runs without GUI)
    if (argc >= 2 && strcmp(argv[1], "--inspect") == 0) {
        return InspectSaveFile(argc, argv);
    }
    
    int screenWidth = 1280, screenHeight = 800;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
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
    InitDesignations();
    BuildEntrances();
    BuildGraph();
    offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;

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
            PROFILE_END(Tick);
            PROFILE_BEGIN(ItemsTick);
            ItemsTick(TICK_DT);
            PROFILE_END(ItemsTick);
            DesignationsTick(TICK_DT);
            PROFILE_BEGIN(AssignJobs);
            AssignJobs();
            PROFILE_END(AssignJobs);
            PROFILE_BEGIN(JobsTick);
            JobsTick();
            PROFILE_END(JobsTick);
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
        DrawWater();
        DrawFire();
        DrawSmoke();
        PROFILE_END(DrawCells);
        if (IsKeyDown(KEY_G)) {
            DrawGatherZones();
        }
        DrawStockpileTiles();
        DrawHaulDestinations();
        DrawStockpileItems();
        DrawMiningDesignations();
        DrawBlueprints();
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
        if (drawingFloor && IsKeyDown(KEY_T)) {
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

        // Draw stockpile preview while dragging (S key)
        if (IsKeyDown(KEY_S) && (drawingStockpile || erasingStockpile)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = stockpileStartX < x ? stockpileStartX : x;
            int y1 = stockpileStartY < y ? stockpileStartY : y;
            int x2 = stockpileStartX > x ? stockpileStartX : x;
            int y2 = stockpileStartY > y ? stockpileStartY : y;
            float size = CELL_SIZE * zoom;

            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;

            if (drawingStockpile) {
                // Green for creating stockpile
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){0, 200, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, GREEN);
            } else {
                // Red for erasing stockpiles
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

        // Draw gather zone preview while dragging (G key)
        if (IsKeyDown(KEY_G) && (drawingGatherZone || erasingGatherZone)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = gatherZoneStartX < x ? gatherZoneStartX : x;
            int y1 = gatherZoneStartY < y ? gatherZoneStartY : y;
            int x2 = gatherZoneStartX > x ? gatherZoneStartX : x;
            int y2 = gatherZoneStartY > y ? gatherZoneStartY : y;
            float size = CELL_SIZE * zoom;

            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;

            if (drawingGatherZone) {
                // Orange for creating gather zone
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){255, 180, 50, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, ORANGE);
            } else {
                // Red for erasing gather zones
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

        // Draw mining designation preview while dragging (M key)
        if (IsKeyDown(KEY_M) && (designatingMining || cancellingMining)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = miningStartX < x ? miningStartX : x;
            int y1 = miningStartY < y ? miningStartY : y;
            int x2 = miningStartX > x ? miningStartX : x;
            int y2 = miningStartY > y ? miningStartY : y;
            float size = CELL_SIZE * zoom;

            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;

            if (designatingMining) {
                // Orange for designating mining
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){255, 150, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, ORANGE);
            } else {
                // Red for cancelling mining designations
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

        // Draw build designation preview while dragging (B key)
        if (IsKeyDown(KEY_B) && (designatingBuild || cancellingBuild)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = buildStartX < x ? buildStartX : x;
            int y1 = buildStartY < y ? buildStartY : y;
            int x2 = buildStartX > x ? buildStartX : x;
            int y2 = buildStartY > y ? buildStartY : y;
            float size = CELL_SIZE * zoom;

            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;

            if (designatingBuild) {
                // Cyan for designating build
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){0, 200, 200, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, (Color){0, 255, 255, 255});
            } else {
                // Red for cancelling build designations
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

        // Stats display
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 5, 5, 18, LIME);
        DrawTextShadow(TextFormat("Z: %d/%d  </>", currentViewZ, gridDepth - 1), 30, GetScreenHeight() - 20, 18, SKYBLUE);

        // Help button (?) in bottom-right corner
        Rectangle helpBtn = {GetScreenWidth() - 25, GetScreenHeight() - 22, 20, 20};
        bool helpHovered = CheckCollisionPointRec(GetMousePosition(), helpBtn);
        DrawRectangleRec(helpBtn, helpHovered ? DARKGRAY : (Color){40, 40, 40, 255});
        DrawRectangleLinesEx(helpBtn, 1, GRAY);
        DrawTextShadow("?", GetScreenWidth() - 19, GetScreenHeight() - 20, 18, helpHovered ? WHITE : LIGHTGRAY);
        if (helpHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            showHelpPanel = !showHelpPanel;
        }

        // Help panel with keyboard shortcuts
        if (showHelpPanel) {
            const char* shortcuts[] = {
                "R + drag      Draw room (walls + floor)",
                "F + drag      Draw floor",
                "L + drag      Draw ladder",
                "S + L-drag    Draw stockpile",
                "S + R-drag    Erase stockpile",
                "G + L-drag    Draw gather zone",
                "G + R-drag    Erase gather zone",
                "M + L-drag    Designate mining",
                "M + R-drag    Cancel mining",
                "B + L-drag    Designate building",
                "B + R-drag    Cancel building",
                "W + L-drag    Place water",
                "W + R-drag    Remove water/source/drain",
                "W+Shift+L     Place water source",
                "W+Shift+R     Place water drain",
                "< / >         Change Z level",
                "Space         Pause/Resume",
                "Scroll        Zoom in/out",
                "Middle-drag   Pan view",
                "F5            Quick save",
                "F6            Quick load",
            };
            int shortcutCount = sizeof(shortcuts) / sizeof(shortcuts[0]);
            int panelW = 260;
            int panelH = 20 + shortcutCount * 18 + 10;
            int panelX = GetScreenWidth() - panelW - 5;
            int panelY = GetScreenHeight() - 28 - panelH;

            DrawRectangle(panelX, panelY, panelW, panelH, (Color){30, 30, 30, 240});
            DrawRectangleLinesEx((Rectangle){panelX, panelY, panelW, panelH}, 1, GRAY);
            DrawTextShadow("Keyboard Shortcuts", panelX + 8, panelY + 5, 14, WHITE);
            for (int i = 0; i < shortcutCount; i++) {
                DrawTextShadow(shortcuts[i], panelX + 8, panelY + 22 + i * 18, 12, LIGHTGRAY);
            }
        }

#if PROFILER_ENABLED
        DrawProfilerPanel(GetScreenWidth() - 50, 5);
#endif

        // Draw UI
        PROFILE_BEGIN(DrawUI);
        DrawUI();
        PROFILE_END(DrawUI);

        // Draw message stack
        DrawMessages(GetScreenWidth(), GetScreenHeight());

        // Draw stockpile tooltip when hovering
        if (hoveredStockpile >= 0) {
            DrawStockpileTooltip(hoveredStockpile, GetMousePosition(), ScreenToGrid(GetMousePosition()));
        }

        // Draw mover tooltip when hovering (only when paused)
        if (hoveredMover >= 0) {
            DrawMoverTooltip(hoveredMover, GetMousePosition());
        }

        // Draw item tooltip when hovering (only when paused, and no mover/stockpile tooltip)
        if (hoveredItemCount > 0 && hoveredMover < 0 && hoveredStockpile < 0) {
            Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
            DrawItemTooltip(hoveredItemCell, hoveredItemCount, GetMousePosition(), (int)mouseGrid.x, (int)mouseGrid.y);
        }

        // Draw water tooltip when hovering over water (and no other tooltips active)
        if (hoveredStockpile < 0 && hoveredMover < 0 && hoveredItemCount == 0) {
            Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
            int cellX = (int)mouseGrid.x;
            int cellY = (int)mouseGrid.y;
            if (cellX >= 0 && cellX < gridWidth && cellY >= 0 && cellY < gridHeight) {
                if (HasWater(cellX, cellY, currentViewZ) || 
                    waterGrid[currentViewZ][cellY][cellX].isSource ||
                    waterGrid[currentViewZ][cellY][cellX].isDrain) {
                    DrawWaterTooltip(cellX, cellY, currentViewZ, GetMousePosition());
                }
            }
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
