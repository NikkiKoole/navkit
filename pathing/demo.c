#include "../vendor/raylib.h"
#include "grid.h"
#include "terrain.h"
#include "pathfinding.h"
#include "mover.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define UI_IMPLEMENTATION
#include "../common/ui.h"

#define MAX_AGENTS  50

float zoom = 1.0f;
Vector2 offset = {0, 0};
Texture2D texGrass;
Texture2D texWall;
bool showGraph = false;
bool showEntrances = false;

// Pathfinding settings
int pathAlgorithm = 1;  // Default to HPA*
const char* algorithmNames[] = {"A*", "HPA*", "JPS", "JPS+"};
int currentDirection = 1;  // 0 = 4-dir, 1 = 8-dir
const char* directionNames[] = {"4-dir", "8-dir"};

// Tool selection: 0=Draw Walls, 1=Erase Walls, 2=Set Start, 3=Set Goal
int currentTool = 0;
const char* toolNames[] = {"Draw Walls", "Erase Walls", "Set Start", "Set Goal"};

// Terrain selection
int currentTerrain = 0;
const char* terrainNames[] = {"Clear", "Sparse", "City", "Mixed", "Perlin", "Maze", "Dungeon", "Caves", "Drunkard", "Tunneler", "MixMax", "NarrowGaps"};

// UI section collapse state
bool sectionView = false;
bool sectionPathfinding = false;
bool sectionMapEditing = true;
bool sectionAgents = false;
bool sectionMovers = true;
bool sectionDebug = false;

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
int moverCountSetting = 1000;
bool showMoverPaths = false;
bool showNeighborCounts = false;

// Extended mover struct for rendering (adds Color)
typedef struct {
    Color color;
} MoverRenderData;
MoverRenderData moverRenderData[MAX_MOVERS];

void DrawCellGrid(void) {
    Rectangle src = {0, 0, 16, 16};
    float size = CELL_SIZE * zoom;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if (grid[y][x] == CELL_WALKABLE) {
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawTexturePro(texGrass, src, dest, (Vector2){0,0}, 0, WHITE);
            }
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if (grid[y][x] == CELL_WALL) {
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawTexturePro(texWall, src, dest, (Vector2){0,0}, 0, WHITE);
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
    if (startPos.x >= 0)
        DrawRectangle((int)(offset.x + startPos.x * size), (int)(offset.y + startPos.y * size), (int)size, (int)size, GREEN);
    if (goalPos.x >= 0)
        DrawRectangle((int)(offset.x + goalPos.x * size), (int)(offset.y + goalPos.y * size), (int)size, (int)size, RED);
    for (int i = 0; i < pathLength; i++) {
        float px = offset.x + path[i].x * size + size * 0.25f;
        float py = offset.y + path[i].y * size + size * 0.25f;
        DrawRectangle((int)px, (int)py, (int)(size * 0.5f), (int)(size * 0.5f), BLUE);
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
    startPos = (Point){-1, -1};
    goalPos = (Point){-1, -1};
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
    startPos = (Point){-1, -1};
    goalPos = (Point){-1, -1};
    pathLength = 0;

    double totalTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "RepathAgents: %d agents in %.2fms (avg %.2fms per agent)", agentCount, totalTime, totalTime / agentCount);
}

void DrawAgents(void) {
    float size = CELL_SIZE * zoom;
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];
        if (!a->active) continue;

        // Draw start (circle)
        float sx = offset.x + a->start.x * size + size / 2;
        float sy = offset.y + a->start.y * size + size / 2;
        DrawCircle((int)sx, (int)sy, size * 0.4f, a->color);

        // Draw goal (square outline)
        float gx = offset.x + a->goal.x * size;
        float gy = offset.y + a->goal.y * size;
        DrawRectangleLines((int)gx, (int)gy, (int)size, (int)size, a->color);

        // Draw path
        for (int j = 0; j < a->pathLength; j++) {
            float px = offset.x + a->path[j].x * size + size * 0.35f;
            float py = offset.y + a->path[j].y * size + size * 0.35f;
            DrawRectangle((int)px, (int)py, (int)(size * 0.3f), (int)(size * 0.3f), a->color);
        }
    }
}

// ============== MOVERS ==============

void SpawnMoversDemo(int count) {
    double startTime = GetTime();

    // Ensure HPA* graph is built
    if (graphEdgeCount == 0) {
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
        float speed = MOVER_SPEED + GetRandomValue(-30, 30);

        // Compute initial path using HPA*
        startPos = start;
        goalPos = goal;
        RunHPAStar();

        if (pathLength > 0) {
            InitMoverWithPath(m, x, y, goal, speed, path, pathLength);

            // Apply string pulling to smooth path
            if (useStringPulling && m->pathLength > 2) {
                StringPullPath(m->path, &m->pathLength);
                m->pathIndex = m->pathLength - 1;
            }

        } else {
            // No path found, spawn anyway - endless mode will assign a new goal
            InitMover(m, x, y, goal, speed);
            TraceLog(LOG_WARNING, "Mover %d spawned without path: (%d,%d) to (%d,%d)", moverCount, start.x, start.y, goal.x, goal.y);
        }

        // Store render data (color)
        moverRenderData[moverCount].color = GetRandomColor();
        moverCount++;
    }

    // Clear global state
    startPos = (Point){-1, -1};
    goalPos = (Point){-1, -1};
    pathLength = 0;

    double elapsed = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "SpawnMovers: %d movers in %.2fms", moverCount, elapsed);
}

void DrawMovers(void) {
    float size = CELL_SIZE * zoom;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;

        // Screen position
        float sx = offset.x + m->x * zoom;
        float sy = offset.y + m->y * zoom;

        // Choose color based on mover state or neighbor count
        Color moverColor;
        if (showNeighborCounts) {
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
            moverColor = GREEN;   // Normal, has valid path
        }

        // Draw mover as colored rectangle
        float moverSize = size * 0.5f;
        DrawRectangle((int)(sx - moverSize/2), (int)(sy - moverSize/2), (int)moverSize, (int)moverSize, moverColor);

        // Optionally draw remaining path
        if (showMoverPaths && m->pathIndex >= 0) {
            Color color = moverRenderData[i].color;
            // Line to next waypoint
            Point next = m->path[m->pathIndex];
            float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
            float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
            DrawLineEx((Vector2){sx, sy}, (Vector2){tx, ty}, 2.0f, color);

            // Rest of path
            for (int j = m->pathIndex; j > 0; j--) {
                float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 1.0f, Fade(color, 0.4f));
            }
        }
    }
}

Vector2 ScreenToGrid(Vector2 screen) {
    float size = CELL_SIZE * zoom;
    return (Vector2){(screen.x - offset.x) / size, (screen.y - offset.y) / size};
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
    }
}

void HandleInput(void) {
    // Zoom with mouse wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mw = ScreenToGrid(GetMousePosition());
        zoom += wheel * 0.1f;
        if (zoom < 0.2f) zoom = 0.2f;
        if (zoom > 3.0f) zoom = 3.0f;
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

    // Tool-based interactions
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            switch (currentTool) {
                case 0:  // Draw Walls
                    if (grid[y][x] != CELL_WALL) {
                        grid[y][x] = CELL_WALL;
                        MarkChunkDirty(x, y);
                        // Mark movers whose path crosses this cell for replanning
                        for (int i = 0; i < moverCount; i++) {
                            Mover* m = &movers[i];
                            if (!m->active) continue;
                            for (int j = m->pathIndex; j >= 0; j--) {
                                if (m->path[j].x == x && m->path[j].y == y) {
                                    m->needsRepath = true;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                case 1:  // Erase Walls
                    if (grid[y][x] != CELL_WALKABLE) {
                        grid[y][x] = CELL_WALKABLE;
                        MarkChunkDirty(x, y);
                    }
                    break;
                case 2:  // Set Start
                    if (grid[y][x] == CELL_WALKABLE) {
                        startPos = (Point){x, y};
                        pathLength = 0;
                    }
                    break;
                case 3:  // Set Goal
                    if (grid[y][x] == CELL_WALKABLE) {
                        goalPos = (Point){x, y};
                        pathLength = 0;
                    }
                    break;
            }
        }
    }

    // Right-click erases (convenience shortcut)
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight && grid[y][x] != CELL_WALKABLE) {
            grid[y][x] = CELL_WALKABLE;
            MarkChunkDirty(x, y);
        }
    }

    // Reset view
    if (IsKeyPressed(KEY_R)) {
        zoom = 1.0f;
        offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
        offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
    }
}

void DrawUI(void) {
    ui_begin_frame();
    float y = 70.0f;
    float x = 10.0f;

    // === VIEW ===
    if (SectionHeader(x, y, "View", &sectionView)) {
        y += 18;
        ToggleBool(x, y, "Show Graph", &showGraph);
        y += 22;
        ToggleBool(x, y, "Show Entrances", &showEntrances);
    }
    y += 22;

    // === PATHFINDING ===
    y += 8;
    if (SectionHeader(x, y, "Pathfinding", &sectionPathfinding)) {
        y += 18;
        CycleOption(x, y, "Algo", algorithmNames, 4, &pathAlgorithm);
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
        CycleOption(x, y, "Tool", toolNames, 4, &currentTool);
        y += 22;
        CycleOption(x, y, "Terrain", terrainNames, 12, &currentTerrain);
        y += 22;
        if (PushButton(x, y, "Generate Terrain")) {
            GenerateCurrentTerrain();
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            BuildEntrances();
            BuildGraph();
        }
        y += 22;
        if (PushButton(x, y, "Small Grid (32x32)")) {
            InitGridWithSizeAndChunkSize(32, 32, 8, 8);
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            BuildEntrances();
            BuildGraph();
            offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
            offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
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
    if (SectionHeader(x, y, "Movers", &sectionMovers)) {
        y += 18;
        DraggableInt(x, y, "Count", &moverCountSetting, 1.0f, 1, MAX_MOVERS);
        y += 22;
        if (PushButton(x, y, "Spawn Movers")) {
            SpawnMoversDemo(moverCountSetting);
        }
        y += 22;
        if (PushButton(x, y, "Clear Movers")) {
            ClearMovers();
        }
        y += 22;
        ToggleBool(x, y, "Show Paths", &showMoverPaths);
        y += 22;
        ToggleBool(x, y, "String Pulling", &useStringPulling);
        y += 22;
        ToggleBool(x, y, "Endless Mode", &endlessMoverMode);
        y += 22;
        ToggleBool(x, y, "Show Neighbors", &showNeighborCounts);
    }
    y += 22;

    // === DEBUG ===
    y += 8;
    if (SectionHeader(x, y, "Debug", &sectionDebug)) {
        y += 18;
        if (PushButton(x, y, "Copy Map ASCII")) {
            // Copy map to clipboard as ASCII
            char* buffer = malloc(gridWidth * gridHeight + gridHeight + 1);
            int idx = 0;
            for (int row = 0; row < gridHeight; row++) {
                for (int col = 0; col < gridWidth; col++) {
                    buffer[idx++] = (grid[row][col] == CELL_WALL) ? '#' : '.';
                }
                buffer[idx++] = '\n';
            }
            buffer[idx] = '\0';
            SetClipboardText(buffer);
            free(buffer);
            TraceLog(LOG_INFO, "Map copied to clipboard");
        }
    }
}

int main(void) {
    int screenWidth = 1280, screenHeight = 800;
    InitWindow(screenWidth, screenHeight, "HPA* Pathfinding");
    texGrass = LoadTexture("assets/grass.png");
    texWall = LoadTexture("assets/wall.png");
    Font comicFont = LoadFont("assets/comic.fnt");
    ui_init(&comicFont);
    SetTargetFPS(60);
    use8Dir = true;  // Default to 8-dir
    InitGridWithSizeAndChunkSize(512, 512, 16, 16);  // 16x16 chunks
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    offset.x = (screenWidth - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (screenHeight - gridHeight * CELL_SIZE * zoom) / 2.0f;

    float accumulator = 0.0f;

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        accumulator += frameTime;

        ui_update();
        HandleInput();

        // Fixed timestep update (Factorio-style: max 1 tick per frame, slowdown if behind)
        if (accumulator >= TICK_DT) {
            Tick();
            accumulator -= TICK_DT;

            // Drain excess - don't try to catch up, just slow down
            if (accumulator > TICK_DT) {
                accumulator = TICK_DT;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawCellGrid();
        DrawChunkBoundaries();
        DrawGraph();
        if (showEntrances) DrawEntrances();
        DrawPath();
        DrawAgents();
        DrawMovers();

        // Stats display
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 5, 5, 18, LIME);
        DrawTextShadow(TextFormat("Entrances: %d | Edges: %d | Agents: %d | Movers: %d/%d | Grid: %.3fms",
                       entranceCount, graphEdgeCount, agentCount, CountActiveMovers(), moverCount, moverGridBuildTimeMs), 5, 25, 16, WHITE);
        if (pathAlgorithm == 1 && hpaAbstractTime > 0) {
            DrawTextShadow(TextFormat("Path: %d | Explored: %d | Time: %.2fms (abstract: %.2fms, refine: %.2fms)",
                     pathLength, nodesExplored, lastPathTime, hpaAbstractTime, hpaRefinementTime), 5, 45, 16, WHITE);
        } else {
            DrawTextShadow(TextFormat("Path: %d | Explored: %d | Time: %.2fms", pathLength, nodesExplored, lastPathTime), 5, 45, 16, WHITE);
        }

        // Draw UI
        DrawUI();

        EndDrawing();
    }
    UnloadTexture(texGrass);
    UnloadTexture(texWall);
    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
