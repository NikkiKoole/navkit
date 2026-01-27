// main.c - Entry point and game loop
//
// Contains global state definitions, helper functions, and the main game loop.
// Included by unity.c - do not compile separately.

#include "game_state.h"

// ============================================================================
// Global State Definitions
// ============================================================================

float zoom = 1.0f;
Vector2 offset = {0, 0};
Texture2D atlas;
int currentViewZ = 0;

bool showGraph = false;
bool showEntrances = false;
bool showMovers = true;
bool showMoverPaths = false;
bool showNeighborCounts = false;
bool showOpenArea = false;
bool showKnotDetection = false;
bool showStuckDetection = false;
bool cullDrawing = true;
bool showItems = true;
bool showHelpPanel = false;
bool paused = false;

bool drawingRoom = false;
int roomStartX = 0, roomStartY = 0;

bool drawingFloor = false;
int floorStartX = 0, floorStartY = 0;

bool drawingStockpile = false;
bool erasingStockpile = false;
int stockpileStartX = 0, stockpileStartY = 0;

bool designatingMining = false;
bool cancellingMining = false;
int miningStartX = 0, miningStartY = 0;

bool designatingBuild = false;
bool cancellingBuild = false;
int buildStartX = 0, buildStartY = 0;

bool placingWaterSource = false;
bool placingWaterDrain = false;
int waterStartX = 0, waterStartY = 0;

bool placingFireSource = false;
bool extinguishingFire = false;
int fireStartX = 0, fireStartY = 0;

bool drawingGatherZone = false;
bool erasingGatherZone = false;
int gatherZoneStartX = 0, gatherZoneStartY = 0;

int pathAlgorithm = 1;
const char* algorithmNames[] = {"A*", "HPA*", "JPS", "JPS+"};
int currentDirection = 1;
const char* directionNames[] = {"4-dir", "8-dir"};

int currentTool = 0;
const char* toolNames[] = {"Draw Wall", "Draw Floor", "Draw Ladder", "Erase", "Set Start", "Set Goal"};

int currentTerrain = 0;
const char* terrainNames[] = {"Clear", "Sparse", "City", "Mixed", "Perlin", "Maze", "Dungeon", "Caves", "Drunkard", "Tunneler", "MixMax", "Towers3D", "GalleryFlat", "Castle", "Labyrinth3D", "Spiral3D", "Council"};

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

int hoveredStockpile = -1;
int hoveredMover = -1;
int hoveredItemCell[16];
int hoveredItemCount = 0;

int agentCountSetting = 10;
Agent agents[MAX_AGENTS];
int agentCount = 0;

int moverCountSetting = 10;
int itemCountSetting = 10;
MoverRenderData moverRenderData[MAX_MOVERS];

// ============================================================================
// Helper Functions
// ============================================================================

int GetCellSprite(CellType cell) {
    switch (cell) {
        case CELL_WALKABLE:     return SPRITE_grass;
        case CELL_GRASS:        return SPRITE_grass;
        case CELL_DIRT:         return SPRITE_dirt;
        case CELL_WALL:         return SPRITE_wall;
        case CELL_LADDER:       return SPRITE_ladder;
        case CELL_LADDER_BOTH:  return SPRITE_ladder;
        case CELL_LADDER_UP:    return SPRITE_ladder_up;
        case CELL_LADDER_DOWN:  return SPRITE_ladder_down;
        case CELL_FLOOR:        return SPRITE_floor;
        case CELL_AIR:          return SPRITE_air;
    }
    return SPRITE_grass;
}

Color GetRandomColor(void) {
    return (Color){
        GetRandomValue(100, 255),
        GetRandomValue(100, 255),
        GetRandomValue(100, 255),
        255
    };
}

Vector2 ScreenToGrid(Vector2 screen) {
    float size = CELL_SIZE * zoom;
    return (Vector2){(screen.x - offset.x) / size, (screen.y - offset.y) / size};
}

Vector2 ScreenToWorld(Vector2 screen) {
    return (Vector2){(screen.x - offset.x) / zoom, (screen.y - offset.y) / zoom};
}

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

int GetMoverAtWorldPos(float wx, float wy, int wz) {
    float bestDist = 999999.0f;
    int bestIdx = -1;
    float radius = CELL_SIZE * 0.6f;

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
        case 11: GenerateTowers(); break;
        case 12: GenerateGalleryFlat(); break;
        case 13: GenerateCastle(); break;
        case 14: GenerateLabyrinth3D(); break;
        case 15: GenerateSpiral3D(); break;
        case 16: GenerateCouncilEstate(); break;
    }
}

// ============================================================================
// Agent/Mover Demo Functions
// ============================================================================

void SpawnAgents(int count) {
    double startTime = GetTime();
    agentCount = 0;
    for (int i = 0; i < count && i < MAX_AGENTS; i++) {
        Agent* a = &agents[agentCount];
        a->start = GetRandomWalkableCell();
        a->goal = GetRandomWalkableCell();
        a->color = GetRandomColor();

        startPos = a->start;
        goalPos = a->goal;
        switch (pathAlgorithm) {
            case 0: RunAStar(); break;
            case 1: RunHPAStar(); break;
            case 2: RunJPS(); break;
            case 3: RunJpsPlus(); break;
        }

        a->pathLength = pathLength;
        for (int j = 0; j < pathLength; j++) {
            a->path[j] = path[j];
        }
        a->active = (pathLength > 0);
        agentCount++;
    }

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

        startPos = a->start;
        goalPos = a->goal;
        switch (pathAlgorithm) {
            case 0: RunAStar(); break;
            case 1: RunHPAStar(); break;
            case 2: RunJPS(); break;
            case 3: RunJpsPlus(); break;
        }

        a->pathLength = pathLength;
        for (int j = 0; j < pathLength; j++) {
            a->path[j] = path[j];
        }
        a->active = (pathLength > 0);
    }

    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;

    double totalTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "RepathAgents: %d agents in %.2fms (avg %.2fms per agent)", agentCount, totalTime, totalTime / agentCount);
}

void AddMoversDemo(int count) {
    moverPathAlgorithm = (PathAlgorithm)pathAlgorithm;

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

    moverPathAlgorithm = (PathAlgorithm)pathAlgorithm;

    if (pathAlgorithm == 1 && graphEdgeCount == 0) {
        BuildEntrances();
        BuildGraph();
    }

    ClearMovers();
    for (int i = 0; i < count && i < MAX_MOVERS; i++) {
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
            TraceLog(LOG_WARNING, "Mover %d spawned without path: (%d,%d,%d) to (%d,%d,%d)", moverCount, start.x, start.y, start.z, goal.x, goal.y, goal.z);
        }

        moverRenderData[moverCount].color = GetRandomColor();
        moverCount++;
    }

    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;

    double elapsed = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "SpawnMovers: %d movers in %.2fms", moverCount, elapsed);
}

void SpawnStockpileWithFilters(bool allowRed, bool allowGreen, bool allowBlue) {
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

// ============================================================================
// Forward declarations for separate modules
// ============================================================================

// From render/rendering.c
void DrawCellGrid(void);
void DrawWater(void);
void DrawFire(void);
void DrawSmoke(void);
void DrawChunkBoundaries(void);
void DrawEntrances(void);
void DrawGraph(void);
void DrawPath(void);
void DrawAgents(void);
void DrawMovers(void);
void DrawItems(void);
void DrawGatherZones(void);
void DrawStockpileTiles(void);
void DrawStockpileItems(void);
void DrawHaulDestinations(void);
void DrawMiningDesignations(void);
void DrawBlueprints(void);

// From render/tooltips.c
void DrawStockpileTooltip(int spIdx, Vector2 mouse, Vector2 mouseGrid);
void DrawMoverTooltip(int moverIdx, Vector2 mouse);
void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY);
void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);

// From render/ui_panels.c
void DrawUI(void);
#if PROFILER_ENABLED
void DrawProfilerPanel(float rightEdge, float y);
#endif

// From core/input.c
void HandleInput(void);

// From core/saveload.c
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    // Check for --inspect mode (runs without GUI)
    if (argc >= 2 && strcmp(argv[1], "--inspect") == 0) {
        return InspectSaveFile(argc, argv);
    }
    
    int screenWidth = 1280, screenHeight = 800;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "HPA* Pathfinding");
    atlas = LoadTexture(ATLAS_PATH);
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    Font comicFont = LoadFont("assets/fonts/comic.fnt");
    ui_init(&comicFont);
    SetTargetFPS(60);
    use8Dir = true;
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    gridDepth = 6;
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

        // Draw previews for various drawing modes
        if (drawingRoom && IsKeyDown(KEY_R)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = roomStartX < x ? roomStartX : x;
            int y1 = roomStartY < y ? roomStartY : y;
            int x2 = roomStartX > x ? roomStartX : x;
            int y2 = roomStartY > y ? roomStartY : y;
            float size = CELL_SIZE * zoom;
            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, YELLOW);
        }

        if (drawingFloor && IsKeyDown(KEY_T)) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = floorStartX < x ? floorStartX : x;
            int y1 = floorStartY < y ? floorStartY : y;
            int x2 = floorStartX > x ? floorStartX : x;
            int y2 = floorStartY > y ? floorStartY : y;
            float size = CELL_SIZE * zoom;
            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;
            DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){139, 90, 43, 100});
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, BROWN);
        }

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
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){0, 200, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, GREEN);
            } else {
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

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
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){255, 180, 50, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, ORANGE);
            } else {
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

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
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){255, 150, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, ORANGE);
            } else {
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

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
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){0, 200, 200, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, (Color){0, 255, 255, 255});
            } else {
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){200, 0, 0, 80});
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            }
        }

        // Stats and HUD
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 5, 5, 18, LIME);
        DrawTextShadow(TextFormat("Z: %d/%d  </>", currentViewZ, gridDepth - 1), 30, GetScreenHeight() - 20, 18, SKYBLUE);

        // Help button
        Rectangle helpBtn = {GetScreenWidth() - 25, GetScreenHeight() - 22, 20, 20};
        bool helpHovered = CheckCollisionPointRec(GetMousePosition(), helpBtn);
        DrawRectangleRec(helpBtn, helpHovered ? DARKGRAY : (Color){40, 40, 40, 255});
        DrawRectangleLinesEx(helpBtn, 1, GRAY);
        DrawTextShadow("?", GetScreenWidth() - 19, GetScreenHeight() - 20, 18, helpHovered ? WHITE : LIGHTGRAY);
        if (helpHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            showHelpPanel = !showHelpPanel;
        }

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

        PROFILE_BEGIN(DrawUI);
        DrawUI();
        PROFILE_END(DrawUI);

        DrawMessages(GetScreenWidth(), GetScreenHeight());

        if (hoveredStockpile >= 0) {
            DrawStockpileTooltip(hoveredStockpile, GetMousePosition(), ScreenToGrid(GetMousePosition()));
        }

        if (hoveredMover >= 0) {
            DrawMoverTooltip(hoveredMover, GetMousePosition());
        }

        if (hoveredItemCount > 0 && hoveredMover < 0 && hoveredStockpile < 0) {
            Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
            DrawItemTooltip(hoveredItemCell, hoveredItemCount, GetMousePosition(), (int)mouseGrid.x, (int)mouseGrid.y);
        }

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

        DrawTooltip();

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
