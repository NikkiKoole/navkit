// render/ui_panels.c - UI panel drawing functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/lighting.h"
#include "../simulation/weather.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/balance.h"
#include "../simulation/floordirt.h"
#include "../core/sim_manager.h"
#include "../core/input_mode.h"
#include "../world/designations.h"
#include "../ui/cutscene.h"
#include <time.h>

// Forward declarations
void DrawTimeOfDayWidget(float x, float y);
void DrawUI(void);
void DrawProfilerPanel(float rightEdge, float y);

// ============================================================================
// TIME OF DAY WIDGET
// ============================================================================

// Get sky color based on time of day (0-24 hours)
Color GetSkyColorForTime(float hour) {
    // Time periods and their colors:
    // 0-4:   Night (dark blue)
    // 4-6:   Dawn (orange/pink gradient)
    // 6-8:   Morning (light blue warming up)
    // 8-12:  Late morning (bright sky blue)
    // 12-16: Afternoon (bright sky, slightly warmer)
    // 16-18: Golden hour (warm golden)
    // 18-20: Dusk (orange/red/purple)
    // 20-24: Night (dark blue)

    // Define key colors at specific hours
    typedef struct { float hour; Color color; } TimeColor;
    TimeColor keyframes[] = {
        { 0.0f,  (Color){ 15,  25,  50, 255} },  // Midnight - deep night blue
        { 4.0f,  (Color){ 25,  35,  65, 255} },  // Late night - slightly lighter
        { 5.0f,  (Color){ 70,  50,  80, 255} },  // Pre-dawn - purple hint
        { 6.0f,  (Color){180, 100,  80, 255} },  // Dawn - orange/pink
        { 7.0f,  (Color){220, 190, 160, 255} },  // Early morning - warm
        { 8.0f,  (Color){210, 220, 240, 255} },  // Morning - hint of blue
        { 9.0f,  (Color){245, 245, 240, 255} },  // Late morning - near white
        {12.0f,  (Color){255, 255, 250, 255} },  // Noon - neutral white
        {16.0f,  (Color){250, 245, 235, 255} },  // Afternoon - barely warm
        {17.0f,  (Color){220, 190, 150, 255} },  // Golden hour start
        {18.0f,  (Color){230, 140,  80, 255} },  // Golden hour - orange
        {19.0f,  (Color){180,  80,  70, 255} },  // Sunset - red/orange
        {20.0f,  (Color){ 80,  50,  90, 255} },  // Dusk - purple
        {21.0f,  (Color){ 35,  40,  70, 255} },  // Early night
        {24.0f,  (Color){ 15,  25,  50, 255} },  // Midnight again
    };
    int numKeyframes = sizeof(keyframes) / sizeof(keyframes[0]);

    // Find the two keyframes to interpolate between
    int i;
    for (i = 0; i < numKeyframes - 1; i++) {
        if (hour < keyframes[i + 1].hour) break;
    }

    // Interpolate
    float t = (hour - keyframes[i].hour) / (keyframes[i + 1].hour - keyframes[i].hour);
    Color c1 = keyframes[i].color;
    Color c2 = keyframes[i + 1].color;

    return (Color){
        (unsigned char)(c1.r + t * (c2.r - c1.r)),
        (unsigned char)(c1.g + t * (c2.g - c1.g)),
        (unsigned char)(c1.b + t * (c2.b - c1.b)),
        255
    };
}

// Draw the time-of-day widget at the specified position
void DrawTimeOfDayWidget(float x, float y) {
    // Widget dimensions - fixed size to prevent resizing
    int paddingY = 4;
    int fontSize = 16;
    int boxWidth = 140;

    // Get current time info
    int hours = (int)timeOfDay;
    int minutes = (int)((timeOfDay - hours) * 60);

    // Line 1: Season + Day, Line 2: Time + Weather, Line 3: Temp + Wind
    char line1[32];
    char line2[32];
    char line3[32];
    snprintf(line1, sizeof(line1), "%s Day %d", GetSeasonName(GetCurrentSeason()), dayNumber);
    snprintf(line2, sizeof(line2), "%02d:%02d %s", hours, minutes, GetWeatherName(weatherState.current));

    // Wind direction arrow from vector
    int temp = GetSeasonalSurfaceTemp();
    float ws = weatherState.windStrength;
    const char* windArrow = "";
    if (ws > 0.1f) {
        float wx = weatherState.windDirX;
        float wy = weatherState.windDirY;
        float angle = atan2f(wy, wx);  // radians
        // 8 compass directions
        if      (angle < -2.75f || angle > 2.75f)  windArrow = "W";
        else if (angle < -1.96f) windArrow = "NW";
        else if (angle < -1.18f) windArrow = "N";
        else if (angle < -0.39f) windArrow = "NE";
        else if (angle <  0.39f) windArrow = "E";
        else if (angle <  1.18f) windArrow = "SE";
        else if (angle <  1.96f) windArrow = "S";
        else                     windArrow = "SW";
    }
    if (ws > 0.1f) {
        snprintf(line3, sizeof(line3), "%dC  %s %.0f", temp, windArrow, ws);
    } else {
        snprintf(line3, sizeof(line3), "%dC  Calm", temp);
    }

    int line1Width = MeasureTextUI(line1, fontSize);
    int line2Width = MeasureTextUI(line2, fontSize);
    int line3Width = MeasureTextUI(line3, fontSize);
    int lineSpacing = 2;
    int boxHeight = fontSize * 3 + lineSpacing * 2 + paddingY * 2;

    // Widen box if needed
    int maxTextWidth = line1Width;
    if (line2Width > maxTextWidth) maxTextWidth = line2Width;
    if (line3Width > maxTextWidth) maxTextWidth = line3Width;
    if (maxTextWidth + 16 > boxWidth) boxWidth = maxTextWidth + 16;

    // Draw sky-colored background rectangle
    Color skyColor = GetSkyColorForTime(timeOfDay);
    DrawRectangle((int)x, (int)y, boxWidth, boxHeight, skyColor);
    DrawRectangleLines((int)x, (int)y, boxWidth, boxHeight, (Color){100, 100, 100, 255});

    // Draw line 1 (season + day) centered
    int text1X = (int)x + (boxWidth - line1Width) / 2;
    int text1Y = (int)y + paddingY;
    DrawTextShadow(line1, text1X, text1Y, fontSize, WHITE);

    // Draw line 2 (time + weather) centered
    int text2X = (int)x + (boxWidth - line2Width) / 2;
    int text2Y = text1Y + fontSize + lineSpacing;
    DrawTextShadow(line2, text2X, text2Y, fontSize, (Color){200, 200, 220, 255});

    // Draw line 3 (temp + wind) centered
    int text3X = (int)x + (boxWidth - line3Width) / 2;
    int text3Y = text2Y + fontSize + lineSpacing;
    DrawTextShadow(line3, text3X, text3Y, fontSize, (Color){180, 200, 180, 255});

    // Block mouse clicks on widget area
    Vector2 mouse = GetMousePosition();
    Rectangle bounds = {x, y, (float)boxWidth, (float)boxHeight};
    if (CheckCollisionPointRec(mouse, bounds)) {
        ui_set_hovered();
    }
}

// Forward declarations for demo functions
void SpawnAgents(int count);
void RepathAgents(void);
void AddMoversDemo(int count);
void SpawnMoversDemo(int count);
void SpawnStockpileWithFilters(bool allowRed, bool allowGreen, bool allowBlue);


// Start a new game: randomize seed, generate 128x128 HillsSoilsWater, spawn 1 mover
static void StartNewGame(void) {
    // Randomize seed
    worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);

    // Set grid to 128x128 before generating (InitGrid inside generator preserves these)
    InitGridWithSizeAndChunkSize(128, 128, 8, 8);

    // Clear entities not covered by InitGrid()
    ClearAnimals();
    ClearFurniture();
    InitPlants();

    // Generate HillsSoilsWater terrain (calls InitGrid internally, which clears movers/items/etc)
    currentTerrain = 19;
    hillsSkipBuildings = true;
    GenerateCurrentTerrain();
    hillsSkipBuildings = false;

    // Init all systems — water/sim counters rebuilt AFTER generation to preserve placed water
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
    InitDesignations();
    RebuildSimActivityCounts();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitFloorDirt();
    InitSnow();
    InitLighting();
    BuildEntrances();
    BuildGraph();

    // Spawn 1 mover at a walkable cell
    SpawnMoversDemo(1);

    // Center camera on the mover and follow it
    if (moverCount > 0) {
        Mover* m = &movers[0];
        currentViewZ = (int)m->z;
        offset.x = GetScreenWidth() / 2.0f - m->x * zoom;
        offset.y = GetScreenHeight() / 2.0f - m->y * zoom;
        followMoverIdx = 0;
    }

    AddMessage("New game started", GREEN);
}

// Minimal player HUD — speed controls, mover count, new game
static void DrawPlayerHUD(void) {
    ui_begin_frame();

    // Speed controls top-left, below FPS
    float y = 30;
    float bx = 10;
    bool clicked = false;
    bx += PushButtonInline(bx, y, paused ? ">>" : "||", &clicked);
    if (clicked) paused = !paused;
    bx += 4;
    clicked = false; bx += PushButtonInline(bx, y, "1x", &clicked); if (clicked) { gameSpeed = 1.0f; paused = false; }
    bx += 4;
    clicked = false; bx += PushButtonInline(bx, y, "2x", &clicked); if (clicked) { gameSpeed = 2.0f; paused = false; }
    bx += 4;
    clicked = false; bx += PushButtonInline(bx, y, "3x", &clicked); if (clicked) { gameSpeed = 3.0f; paused = false; }
    y += 22;

    // Mover count + Find button
    int active = CountActiveMovers();
    if (active > 0) {
        bx = 10;
        bx += MeasureText(TextFormat("Movers: %d", active), 14) + 4;
        DrawTextShadow(TextFormat("Movers: %d", active), 10, (int)y, 14, LIGHTGRAY);
        clicked = false;
        bx += 4;
        PushButtonInline(bx, y, "Find", &clicked);
        if (clicked && moverCount > 0) {
            // Find first active mover and center camera on them
            for (int i = 0; i < moverCount; i++) {
                if (movers[i].active) {
                    followMoverIdx = i;
                    currentViewZ = (int)movers[i].z;
                    offset.x = GetScreenWidth() / 2.0f - movers[i].x * zoom;
                    offset.y = GetScreenHeight() / 2.0f - movers[i].y * zoom;
                    break;
                }
            }
        }
        y += 18;
    }

    // Designation buttons
    y += 4;
    clicked = false;
    bool harvestActive = (inputAction == ACTION_WORK_HARVEST_BERRY);
    if (PushButton(10, y, harvestActive ? "* Harvest Berries *" : "Harvest Berries")) {
        if (harvestActive) {
            InputMode_ExitToNormal();
        } else {
            inputMode = MODE_WORK;
            workSubMode = SUBMODE_HARVEST;
            inputAction = ACTION_WORK_HARVEST_BERRY;
        }
    }
    y += 22;

    bool gatherTreeActive = (inputAction == ACTION_WORK_GATHER_TREE);
    if (PushButton(10, y, gatherTreeActive ? "* Gather Tree *" : "Gather Tree")) {
        if (gatherTreeActive) {
            InputMode_ExitToNormal();
        } else {
            inputMode = MODE_WORK;
            workSubMode = SUBMODE_HARVEST;
            inputAction = ACTION_WORK_GATHER_TREE;
        }
    }
    y += 22;

    bool stockpileActive = (inputAction == ACTION_DRAW_STOCKPILE);
    if (PushButton(10, y, stockpileActive ? "* Place Stockpile *" : "Place Stockpile")) {
        if (stockpileActive) {
            InputMode_ExitToNormal();
        } else {
            inputMode = MODE_DRAW;
            inputAction = ACTION_DRAW_STOCKPILE;
        }
    }
    y += 22;

    bool gatherGrassActive = (inputAction == ACTION_WORK_GATHER_GRASS);
    if (PushButton(10, y, gatherGrassActive ? "* Gather Grass *" : "Gather Grass")) {
        if (gatherGrassActive) {
            InputMode_ExitToNormal();
        } else {
            inputMode = MODE_WORK;
            workSubMode = SUBMODE_HARVEST;
            inputAction = ACTION_WORK_GATHER_GRASS;
        }
    }
    y += 26;

    // New Game button
    if (PushButton(10, y, "New Game")) {
        StartNewGame();
    }
    y += 22;

    // Hint text
    if (inputAction != ACTION_NONE) {
        DrawTextShadow("L-drag designate  R-drag cancel  ESC: back", 10, (int)y, 12, (Color){180, 180, 100, 255});
        y += 16;
    }
    DrawTextShadow("F1: dev UI", 10, (int)y, 12, (Color){100, 100, 100, 255});
}

void DrawUI(void) {
    if (!devUI) {
        DrawPlayerHUD();
        return;
    }

    ui_begin_frame();
    float y = 30.0f;
    float x = 10.0f;
    float ix = x + 10;  // indented x for sub-sections

    // ========================================================================
    // [+] Pathfinding  (standalone — small, 4 controls)
    // ========================================================================
    if (SectionHeader(x, y, "Pathfinding", &sectionPathfinding)) {
        y += 18;
        int prevAlgo = pathAlgorithm;
        CycleOption(x, y, "Algo", algorithmNames, 4, &pathAlgorithm);
        if (pathAlgorithm != prevAlgo) ResetPathStats();
        y += 22;
        CycleOption(x, y, "Dir", directionNames, 2, &currentDirection);
        use8Dir = (currentDirection == 1);
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

    // ========================================================================
    // [+] World  (super-group: Map Editing, Trees, Entropy)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "World", &sectionWorld)) {
        y += 18;

        // --- Map Editing ---
        if (SectionHeader(ix, y, "Map Editing", &sectionMapEditing)) {
            y += 18;
            CycleOption(ix, y, "Tool", toolNames, 6, &currentTool);
            y += 22;
            CycleOption(ix, y, "Terrain", terrainNames, 21, &currentTerrain);
            y += 22;
            bool isHillsTerrain = (currentTerrain == 17 || currentTerrain == 18 || currentTerrain == 19);
            bool isHillsWater = (currentTerrain == 19);
            if (isHillsTerrain) {
                DraggableFloatT(ix, y, "Ramp Density", &rampDensity, 0.02f, 0.0f, 1.0f,
                    "Hills ramp placement density (0=none, 1=all). Lower reduces HPA* graph size.");
                y += 22;
                DraggableFloatT(ix, y, "Ramp Noise Scale", &rampNoiseScale, 0.005f, 0.005f, 0.2f,
                    "Controls ramp cluster size for hills generators (higher = larger clusters).");
                y += 22;
            }
            if (isHillsWater) {
                DraggableIntT(ix, y, "River Count", &hillsWaterRiverCount, 1.0f, 0, 6,
                    "HillsSoilsWater: number of rivers (0 disables rivers).");
                y += 22;
                DraggableIntT(ix, y, "River Width", &hillsWaterRiverWidth, 1.0f, 1, 4,
                    "HillsSoilsWater: river width radius (1-4).");
                y += 22;
                DraggableIntT(ix, y, "Lake Count", &hillsWaterLakeCount, 1.0f, 0, 6,
                    "HillsSoilsWater: number of lakes (0 disables lakes).");
                y += 22;
                DraggableIntT(ix, y, "Lake Radius", &hillsWaterLakeRadius, 1.0f, 3, 12,
                    "HillsSoilsWater: lake radius (3-12).");
                y += 22;
                DraggableFloatT(ix, y, "Wetness Bias", &hillsWaterWetnessBias, 0.05f, 0.0f, 1.0f,
                    "HillsSoilsWater: wetness boost near water (peat/clay bias).");
                y += 22;
                ToggleBoolT(ix, y, "Conn Report", &hillsWaterConnectivityReport,
                    "Logs walkability connectivity stats after generation.");
                y += 22;
                ToggleBoolT(ix, y, "Fix Tiny Pockets", &hillsWaterConnectivityFixSmall,
                    "Fills tiny disconnected walkable pockets after generation.");
                y += 22;
                DraggableIntT(ix, y, "Tiny Size", &hillsWaterConnectivitySmallThreshold, 1.0f, 5, 200,
                    "Size threshold for tiny pocket fill (cells).");
                y += 22;
            }
            if (PushButton(ix, y, "Randomize Seed")) {
                worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);
                AddMessage(TextFormat("New seed: %llu", (unsigned long long)worldSeed), GREEN);
            }
            y += 22;
            if (PushButton(ix, y, "Generate Terrain")) {
                InitPlants();
                GenerateCurrentTerrain();
                InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
                InitDesignations();
                RebuildSimActivityCounts();
                InitFire();
                InitSmoke();
                InitSteam();
                InitTemperature();
                InitGroundWear();
                InitFloorDirt();
                InitSnow();
                InitLighting();
                BuildEntrances();
                BuildGraph();
                AddMessage(TextFormat("Generated terrain: %s", terrainNames[currentTerrain]), GREEN);
            }
            y += 22;
            // Grid size buttons — inline row
            DrawTextShadow("Grid:", ix, y, 14, GRAY);
            {
                float bx = ix + 34;
                bool clicked = false;
                bx += PushButtonInline(bx, y, "32x32", &clicked);
                if (clicked) {
                    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
                    gridDepth = 6;
                    for (int z = 1; z < gridDepth; z++)
                        for (int gy = 0; gy < gridHeight; gy++)
                            for (int gx = 0; gx < gridWidth; gx++)
                                grid[z][gy][gx] = CELL_AIR;
                    FillGroundLevel();
                    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
                    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
                    InitDesignations();
                    InitSimActivity();
                    InitWater();
                    InitFire();
                    InitSmoke();
                    InitSteam();
                    InitTemperature();
                    InitGroundWear();
                    InitFloorDirt();
                    InitSnow();
                    InitLighting();
                    BuildEntrances();
                    BuildGraph();
                    offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
                    offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
                }
                clicked = false;
                bx += PushButtonInline(bx, y, "64x64", &clicked);
                if (clicked) {
                    InitGridWithSizeAndChunkSize(64, 64, 8, 8);
                    gridDepth = 6;
                    for (int z = 1; z < gridDepth; z++)
                        for (int gy = 0; gy < gridHeight; gy++)
                            for (int gx = 0; gx < gridWidth; gx++)
                                grid[z][gy][gx] = CELL_AIR;
                    FillGroundLevel();
                    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
                    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
                    InitDesignations();
                    InitSimActivity();
                    InitWater();
                    InitFire();
                    InitSmoke();
                    InitSteam();
                    InitTemperature();
                    InitGroundWear();
                    InitFloorDirt();
                    InitSnow();
                    InitLighting();
                    BuildEntrances();
                    BuildGraph();
                    offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
                    offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
                }
                clicked = false;
                bx += PushButtonInline(bx, y, "128x128", &clicked);
                if (clicked) {
                    InitGridWithSizeAndChunkSize(128, 128, 16, 16);
                    gridDepth = 6;
                    for (int z = 1; z < gridDepth; z++)
                        for (int gy = 0; gy < gridHeight; gy++)
                            for (int gx = 0; gx < gridWidth; gx++)
                                grid[z][gy][gx] = CELL_AIR;
                    FillGroundLevel();
                    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
                    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
                    InitDesignations();
                    InitSimActivity();
                    InitWater();
                    InitFire();
                    InitSmoke();
                    InitSteam();
                    InitTemperature();
                    InitGroundWear();
                    InitFloorDirt();
                    InitSnow();
                    InitLighting();
                    BuildEntrances();
                    BuildGraph();
                    offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
                    offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
                }
                clicked = false;
                PushButtonInline(bx, y, "256x256", &clicked);
                if (clicked) {
                    InitGridWithSizeAndChunkSize(256, 256, 16, 16);
                    gridDepth = 3;
                    for (int z = 1; z < gridDepth; z++)
                        for (int gy = 0; gy < gridHeight; gy++)
                            for (int gx = 0; gx < gridWidth; gx++)
                                grid[z][gy][gx] = CELL_AIR;
                    FillGroundLevel();
                    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
                    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
                    InitDesignations();
                    InitSimActivity();
                    InitWater();
                    InitFire();
                    InitSmoke();
                    InitSteam();
                    InitTemperature();
                    InitGroundWear();
                    InitFloorDirt();
                    InitSnow();
                    InitLighting();
                    BuildEntrances();
                    BuildGraph();
                    offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
                    offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
                }
            }
            y += 22;
            if (PushButton(ix, y, "Fill with Walls")) {
                for (int gy = 0; gy < gridHeight; gy++) {
                    for (int gx = 0; gx < gridWidth; gx++) {
                        grid[currentViewZ][gy][gx] = CELL_WALL;
                        SetWaterLevel(gx, gy, currentViewZ, 0);
                        SetWaterSource(gx, gy, currentViewZ, false);
                        SetWaterDrain(gx, gy, currentViewZ, false);
                    }
                }
                for (int cy = 0; cy < chunksY; cy++) {
                    for (int cx = 0; cx < chunksX; cx++) {
                        MarkChunkDirty(cx * chunkWidth, cy * chunkHeight, currentViewZ);
                    }
                }
                BuildEntrances();
                BuildGraph();
                InitDesignations();
                AddMessage(TextFormat("Filled z=%d with walls", currentViewZ), GREEN);
            }
            y += 22;
            if (PushButton(ix, y, "Copy Map ASCII")) {
                int floorDataSize = gridWidth * gridHeight + gridHeight;
                int bufferSize = gridDepth * (16 + floorDataSize) + 1;
                char* buffer = malloc(bufferSize);
                if (!buffer) {
                    AddMessage("Failed to allocate clipboard buffer", RED);
                } else {
                    int idx = 0;
                    for (int z = 0; z < gridDepth; z++) {
                        idx += snprintf(buffer + idx, bufferSize - idx, "floor:%d\n", z);
                        for (int row = 0; row < gridHeight; row++) {
                            for (int col = 0; col < gridWidth; col++) {
                                char c;
                                switch (grid[z][row][col]) {
                                    case CELL_WALL:        c = '#'; break;
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
                    AddMessage(TextFormat("Map copied to clipboard (%d floors)", gridDepth), ORANGE);
                }
            }
        }
        y += 22;

        // --- Trees ---
        if (SectionHeader(ix, y, "Trees", &sectionTrees)) {
            y += 18;
            DraggableFloatT(ix, y, "Sapling Grow", &saplingGrowGH, 0.05f, 0.01f, 100.0f,
                TextFormat("Time for sapling to become trunk: %.2f game-hours (%.1fs)", saplingGrowGH, GameHoursToGameSeconds(saplingGrowGH)));
            y += 22;
            DraggableFloatT(ix, y, "Trunk Grow", &trunkGrowGH, 0.02f, 0.01f, 50.0f,
                TextFormat("Time between trunk growth stages: %.2f game-hours (%.1fs)", trunkGrowGH, GameHoursToGameSeconds(trunkGrowGH)));
            y += 22;
            ToggleBoolT(ix, y, "Sapling Regrowth", &saplingRegrowthEnabled,
                "Enable natural sapling spawning on untrampled grass. Saplings appear over time in wilderness areas.");
            y += 22;
            DraggableIntT(ix, y, "Regrowth Chance", &saplingRegrowthChance, 1.0f, 0, 100,
                TextFormat("Chance per 10000 per interval for sapling to spawn on tall grass. At %d, roughly %.2f%% chance.",
                           saplingRegrowthChance, saplingRegrowthChance / 100.0f));
            y += 22;
            DraggableIntT(ix, y, "Min Tree Distance", &saplingMinTreeDistance, 1.0f, 1, 10,
                TextFormat("Minimum tiles from existing trees/saplings for new sapling to spawn. At %d, trees spread more slowly.", saplingMinTreeDistance));
            y += 22;
            DrawTextShadow(TextFormat("Sandbox Tree Type: %s", TreeTypeName(currentTreeType)), ix, y, 14, GRAY);
        }
        y += 22;

        // --- Entropy (Ground Wear) ---
        if (SectionHeader(ix, y, "Entropy", &sectionEntropy)) {
            y += 18;
            ToggleBoolT(ix, y, "Enabled", &groundWearEnabled,
                "Creates emergent paths: grass becomes dirt when trampled, dirt recovers to grass when left alone.");
            y += 22;
            if (PushButton(ix, y, "Clear Wear")) {
                ClearGroundWear();
            }
            y += 22;

            // Advanced sub-section
            if (SectionHeader(ix + 10, y, "Advanced", &sectionEntropyAdvanced)) {
                y += 18;
                {
                    int stepsToPath = wearTrampleAmount > 0 ? wearGrassToDirt / wearTrampleAmount : 9999;
                    DraggableIntT(ix + 10, y, "Trample Amount", &wearTrampleAmount, 1.0f, 1, 100,
                        TextFormat("%d wear per step. %d steps to form a path.", wearTrampleAmount, stepsToPath));
                }
                y += 22;
                DraggableIntT(ix + 10, y, "Grass->Dirt Threshold", &wearGrassToDirt, 50.0f, 100, 10000,
                    TextFormat("Grass becomes dirt at %d wear.", wearGrassToDirt));
                y += 22;
                DraggableIntT(ix + 10, y, "Dirt->Grass Threshold", &wearDirtToGrass, 50.0f, 0, 5000,
                    TextFormat("Dirt regrows grass below %d wear. Gap of %d creates path persistence.",
                               wearDirtToGrass, wearGrassToDirt - wearDirtToGrass));
                y += 22;
                DraggableIntT(ix + 10, y, "Decay Rate", &wearDecayRate, 1.0f, 1, 100,
                    TextFormat("%d wear recovered per tick.", wearDecayRate));
                y += 22;
                DraggableFloatT(ix + 10, y, "Recovery Interval (s)", &wearRecoveryInterval, 0.5f, 0.1f, 60.0f,
                    TextFormat("Wear decays every %.1f game-seconds.", wearRecoveryInterval));
                y += 22;
                int decaySteps = (wearMax - wearDirtToGrass) / wearDecayRate;
                float gameSecondsToRegrow = decaySteps * wearRecoveryInterval;
                if (gameSecondsToRegrow < 60) {
                    DrawTextShadow(TextFormat("Regrow time: %.1fs game-time", gameSecondsToRegrow), ix + 10, y, 14, GRAY);
                } else {
                    DrawTextShadow(TextFormat("Regrow time: %.1fm game-time", gameSecondsToRegrow / 60.0f), ix + 10, y, 14, GRAY);
                }
            }
        }
    }
    y += 22;

    // ========================================================================
    // [+] Entities  (super-group: Movers, Animals inline, Agents inline, Jobs)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Entities", &sectionEntities)) {
        y += 18;

        // --- Movers ---
        if (PushButton(ix + 140, y, "+")) {
            AddMoversDemo(moverCountSetting);
        }
        if (SectionHeader(ix, y, TextFormat("Movers (%d/%d)", CountActiveMovers(), moverCount), &sectionMovers)) {
            y += 18;
            DraggableIntLog(ix, y, "Count", &moverCountSetting, 1.0f, 1, MAX_MOVERS);
            y += 22;
            if (PushButton(ix, y, "Spawn Movers")) {
                SpawnMoversDemo(moverCountSetting);
            }
            y += 22;
            if (PushButton(ix, y, "Clear Movers")) {
                ClearMovers();
            }
            y += 22;
            ToggleBool(ix, y, "String Pulling", &useStringPulling);
            y += 22;
            ToggleBool(ix, y, "Endless Mode", &endlessMoverMode);
            y += 22;
            ToggleBool(ix, y, "Prefer Diff Z", &preferDifferentZ);
            y += 22;
            ToggleBool(ix, y, "Allow Falling", &allowFallingFromAvoidance);

            // Avoidance subsection
            y += 22;
            if (SectionHeader(ix + 10, y, "Avoidance", &sectionMoverAvoidance)) {
                y += 18;
                ToggleBool(ix + 10, y, "Enabled", &useMoverAvoidance);
                y += 22;
                ToggleBool(ix + 10, y, "Directional", &useDirectionalAvoidance);
                y += 22;
                DraggableFloat(ix + 10, y, "Open Strength", &avoidStrengthOpen, 0.01f, 0.0f, 2.0f);
                y += 22;
                DraggableFloat(ix + 10, y, "Closed Strength", &avoidStrengthClosed, 0.01f, 0.0f, 2.0f);
            }

            // Walls subsection
            y += 22;
            if (SectionHeader(ix + 10, y, "Walls", &sectionMoverWalls)) {
                y += 18;
                ToggleBool(ix + 10, y, "Repulsion", &useWallRepulsion);
                y += 22;
                DraggableFloat(ix + 10, y, "Repel Strength", &wallRepulsionStrength, 0.01f, 0.0f, 2.0f);
                y += 22;
                ToggleBool(ix + 10, y, "Sliding", &useWallSliding);
                y += 22;
                ToggleBool(ix + 10, y, "Knot Fix", &useKnotFix);
            }
        }
        y += 22;

        // --- Animals (inline — no own accordion) ---
        DrawTextShadow(TextFormat("Animals (%d)", CountActiveAnimals()), ix, y, 14, GRAY);
        {
            float bx = ix + 80;
            bool clicked = false;
            bx += PushButtonInline(bx, y, "+", &clicked);
            if (clicked) SpawnAnimal(ANIMAL_GRAZER, currentViewZ, BEHAVIOR_SIMPLE_GRAZER);
            clicked = false;
            bx += PushButtonInline(bx, y, "+S", &clicked);
            if (clicked) SpawnAnimal(ANIMAL_GRAZER, currentViewZ, BEHAVIOR_STEERING_GRAZER);
            clicked = false;
            bx += PushButtonInline(bx, y, "+W", &clicked);
            if (clicked) SpawnAnimal(ANIMAL_PREDATOR, currentViewZ, BEHAVIOR_PREDATOR);
            clicked = false;
            PushButtonInline(bx, y, "Clear", &clicked);
            if (clicked) ClearAnimals();
        }
        y += 22;

        // --- Agents (inline — no own accordion) ---
        DrawTextShadow("Agents:", ix, y, 14, GRAY);
        y += 18;
        DraggableInt(ix, y, "Count", &agentCountSetting, 1.0f, 1, MAX_AGENTS);
        y += 22;
        {
            float bx = ix;
            bool clicked = false;
            bx += PushButtonInline(bx, y, "Spawn", &clicked);
            if (clicked) {
                if (graphEdgeCount == 0) { BuildEntrances(); BuildGraph(); }
                SpawnAgents(agentCountSetting);
            }
            clicked = false;
            PushButtonInline(bx, y, "Repath", &clicked);
            if (clicked) {
                if (pathAlgorithm == 1 && graphEdgeCount == 0) { BuildEntrances(); BuildGraph(); }
                RepathAgents();
            }
        }
        y += 22;

        // --- Jobs ---
        if (SectionHeader(ix, y, TextFormat("Jobs (%d items)", itemCount), &sectionJobs)) {
            y += 18;
            DraggableIntLog(ix, y, "Count", &itemCountSetting, 1.0f, 1, MAX_ITEMS);
            y += 22;
            // Spawn item buttons (packed in rows)
            {
                struct { const char* label; ItemType type; uint8_t mat; } spawnDefs[] = {
                    {"Red",     ITEM_RED,         MAT_NONE},
                    {"Green",   ITEM_GREEN,       MAT_NONE},
                    {"Blue",    ITEM_BLUE,        MAT_NONE},
                    {"Rocks",   ITEM_ROCK,        MAT_NONE},
                    {"Blocks",  ITEM_BLOCKS,      MAT_GRANITE},
                    {"Logs",    ITEM_LOG,         MAT_OAK},
                    {"Planks",  ITEM_PLANKS,      MAT_OAK},
                    {"Sticks",  ITEM_STICKS,      MAT_NONE},
                    {"Cordage", ITEM_CORDAGE,     MAT_NONE},
                    {"Dirt",    ITEM_DIRT,         MAT_NONE},
                    {"Bricks",  ITEM_BRICKS,      MAT_BRICK},
                    {"D.Grass", ITEM_DRIED_GRASS, MAT_NONE},
                };
                int spawnDefCount = sizeof(spawnDefs) / sizeof(spawnDefs[0]);
                float bx = (float)ix;
                for (int d = 0; d < spawnDefCount; d++) {
                    bool clicked = false;
                    float w = PushButtonInline(bx, (float)y, spawnDefs[d].label, &clicked);
                    if (clicked) {
                        for (int i = 0; i < itemCountSetting; i++) {
                            int attempts = 100;
                            while (attempts-- > 0) {
                                int gx = GetRandomValue(0, gridWidth - 1);
                                int gy = GetRandomValue(0, gridHeight - 1);
                                if (IsCellWalkableAt(currentViewZ, gy, gx)) {
                                    float px = gx * CELL_SIZE + CELL_SIZE * 0.5f;
                                    float py = gy * CELL_SIZE + CELL_SIZE * 0.5f;
                                    if (spawnDefs[d].mat != MAT_NONE)
                                        SpawnItemWithMaterial(px, py, (float)currentViewZ, spawnDefs[d].type, spawnDefs[d].mat);
                                    else
                                        SpawnItem(px, py, (float)currentViewZ, spawnDefs[d].type);
                                    break;
                                }
                            }
                        }
                    }
                    bx += w;
                    if (bx > ix + 170) { bx = (float)ix; y += 22; }
                }
                if (bx > (float)ix) y += 22;
            }

            if (PushButton(ix, y, "Clear Items")) {
                ClearItems();
            }
            y += 22;
            if (PushButton(ix, y, "Stockpile: All"))   SpawnStockpileWithFilters(true, true, true);
            y += 22;
            if (PushButton(ix, y, "Stockpile: Red"))   SpawnStockpileWithFilters(true, false, false);
            y += 22;
            if (PushButton(ix, y, "Stockpile: Green")) SpawnStockpileWithFilters(false, true, false);
            y += 22;
            if (PushButton(ix, y, "Stockpile: Blue"))  SpawnStockpileWithFilters(false, false, true);
            y += 22;
            if (PushButton(ix, y, "Clear Stockpiles")) {
                ClearStockpiles();
            }
        }
    }
    y += 22;

    // ========================================================================
    // [+] Simulation  (super-group: Water, Fire, Smoke, Steam, Temperature)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Simulation", &sectionSimulation)) {
        y += 18;

        // Clear All Sim — consolidated button at top
        if (PushButton(ix, y, "Clear All Sim")) {
            ClearWater();
            ClearFire();
            ClearSmoke();
            ClearSteam();
            ClearTemperature();
            ClearGroundWear();
        }
        y += 22;

        // --- Water ---
        if (SectionHeader(ix, y, "Water", &sectionWater)) {
            y += 18;
            ToggleBoolT(ix, y, "Enabled", &waterEnabled,
                "Master toggle for water simulation. Water flows down, spreads horizontally, and uses pressure to rise through U-bends.");
            y += 22;
            ToggleBoolT(ix, y, "Evaporation", &waterEvaporationEnabled,
                "When enabled, shallow water (level 1) has a chance to evaporate each tick. Disable for testing water mechanics.");
            y += 22;
            {
                float evapGH = waterEvapInterval / (dayLength / 24.0f);
                DraggableFloatT(ix, y, "Evap Interval (s)", &waterEvapInterval, 1.0f, 1.0f, 120.0f,
                    TextFormat("Puddles evaporate every %.1f game-seconds (%.2f game-hours).",
                               waterEvapInterval, evapGH));
            }
            y += 22;
            if (PushButton(ix, y, "Clear Water")) {
                ClearWater();
            }
            y += 22;
            DrawTextShadow(IsRaining() ? "Rain (active):" : "Rain:", ix, y, 14, IsRaining() ? BLUE : GRAY);
            y += 18;
            {
                float bx = (float)ix;
                bool clicked = false;
                bx += PushButtonInline(bx, (float)y, "Light", &clicked);
                if (clicked) { SpawnSkyWater(5); AddMessage("Light rain started", BLUE); }
                clicked = false;
                bx += PushButtonInline(bx, (float)y, "Medium", &clicked);
                if (clicked) { SpawnSkyWater(20); AddMessage("Medium rain started", BLUE); }
                clicked = false;
                bx += PushButtonInline(bx, (float)y, "Heavy", &clicked);
                if (clicked) { SpawnSkyWater(50); AddMessage("Heavy rain started", BLUE); }
                if (IsRaining()) {
                    clicked = false;
                    PushButtonInline(bx, (float)y, "Stop", &clicked);
                    if (clicked) { StopRain(); AddMessage("Rain stopped", GRAY); }
                }
            }
            y += 22;

            // Advanced: speed multipliers
            if (SectionHeader(ix + 10, y, "Advanced", &sectionWaterAdvanced)) {
                y += 18;
                DrawTextShadow("Mover Speed in Water:", ix + 10, y, 14, GRAY);
                y += 18;
                {
                    float baseTPS = balance.baseMoverSpeed / (float)CELL_SIZE;
                    DraggableFloatT(ix + 10, y, "Shallow (1-2)", &waterSpeedShallow, 0.05f, 0.1f, 1.0f,
                        TextFormat("%.0f%% speed in shallow water (%.1f tiles/gs).",
                                   waterSpeedShallow * 100.0f, baseTPS * waterSpeedShallow));
                    y += 22;
                    DraggableFloatT(ix + 10, y, "Medium (3-4)", &waterSpeedMedium, 0.05f, 0.1f, 1.0f,
                        TextFormat("%.0f%% speed in medium water (%.1f tiles/gs).",
                                   waterSpeedMedium * 100.0f, baseTPS * waterSpeedMedium));
                    y += 22;
                    DraggableFloatT(ix + 10, y, "Deep (5-7)", &waterSpeedDeep, 0.05f, 0.1f, 1.0f,
                        TextFormat("%.0f%% speed in deep water (%.1f tiles/gs).",
                                   waterSpeedDeep * 100.0f, baseTPS * waterSpeedDeep));
                    y += 22;
                    DraggableFloatT(ix + 10, y, "Mud Speed", &mudSpeedMultiplier, 0.05f, 0.1f, 1.0f,
                        TextFormat("%.0f%% speed on mud (%.1f tiles/gs).",
                                   mudSpeedMultiplier * 100.0f, baseTPS * mudSpeedMultiplier));
                }
                y += 22;
                DraggableFloatT(ix + 10, y, "Wetness Sync", &wetnessSyncInterval, 0.5f, 0.5f, 30.0f,
                    TextFormat("Soil absorbs water every %.1fs. Lower = ground gets wet faster.", wetnessSyncInterval));
            }
        }
        y += 22;

        // --- Fire ---
        if (SectionHeader(ix, y, "Fire", &sectionFire)) {
            y += 18;
            ToggleBoolT(ix, y, "Enabled", &fireEnabled,
                "Master toggle for fire simulation. Fire consumes fuel, spreads to neighbors, and generates smoke.");
            y += 22;
            DraggableFloatT(ix, y, "Spread Interval (s)", &fireSpreadInterval, 0.1f, 0.1f, 10.0f,
                TextFormat("Fire tries to spread every %.1f game-seconds (%.1f attempts/gs).",
                           fireSpreadInterval, 1.0f / fireSpreadInterval));
            y += 22;
            {
                // Wood fuel ~10, so burn time = fuel * fuelInterval
                float woodBurnGS = 10.0f * fireFuelInterval;
                float woodBurnGH = woodBurnGS / (dayLength / 24.0f);
                DraggableFloatT(ix, y, "Fuel Interval (s)", &fireFuelInterval, 0.1f, 0.1f, 10.0f,
                    TextFormat("Fuel consumed every %.1f game-seconds.\nWood wall burns for ~%.0fs (~%.1f game-hours).",
                               fireFuelInterval, woodBurnGS, woodBurnGH));
            }
            y += 22;
            DraggableIntT(ix, y, "Water Reduction %", &fireWaterReduction, 1.0f, 1, 100,
                TextFormat("Fire spreads %.1fx slower near water (%d%% chance reduction).",
                           100.0f / (float)fireWaterReduction, fireWaterReduction));
            y += 22;
            if (PushButton(ix, y, "Clear Fire")) {
                ClearFire();
            }
            y += 22;
            DrawTextShadow("Wetness: damp -50% spread, wet -90%, soaked blocks", ix, y, 14, GRAY);
            y += 16;
            DrawTextShadow("Rain: exposed fires lose levels (20%/40% per fuel tick)", ix, y, 14, GRAY);
            y += 18;

            // Advanced: spread chance formula
            if (SectionHeader(ix + 10, y, "Advanced", &sectionFireAdvanced)) {
                y += 18;
                DrawTextShadow("Spread Chance Formula:", ix + 10, y, 14, GRAY);
                y += 18;
                DraggableIntT(ix + 10, y, "Base Chance %", &fireSpreadBase, 1.0f, 0, 50,
                    "Base spread chance before fire level bonus. Formula: base + (level * perLevel).");
                y += 22;
                DraggableIntT(ix + 10, y, "Per Level %", &fireSpreadPerLevel, 1.0f, 0, 30,
                    "Additional spread chance per fire level.");
                y += 22;
                int minSpread = fireSpreadBase + (FIRE_MIN_SPREAD_LEVEL * fireSpreadPerLevel);
                int maxSpread = fireSpreadBase + (FIRE_MAX_LEVEL * fireSpreadPerLevel);
                DrawTextShadow(TextFormat("Level 2: %d%%, Level 7: %d%%", minSpread, maxSpread), ix + 10, y, 14, GRAY);
            }
        }
        y += 22;

        // --- Smoke ---
        if (SectionHeader(ix, y, "Smoke", &sectionSmoke)) {
            y += 18;
            ToggleBoolT(ix, y, "Enabled", &smokeEnabled,
                "Master toggle for smoke simulation. Smoke rises, spreads horizontally, fills enclosed spaces, and gradually dissipates.");
            y += 22;
            {
                float dissipGH = smokeDissipationTime / (dayLength / 24.0f);
                DraggableFloatT(ix, y, "Rise Interval (s)", &smokeRiseInterval, 0.01f, 0.01f, 2.0f,
                    TextFormat("Smoke rises one Z-level every %.2f game-seconds.", smokeRiseInterval));
                y += 22;
                DraggableFloatT(ix, y, "Dissipation Time", &smokeDissipationTime, 0.1f, 0.5f, 30.0f,
                    TextFormat("Smoke fully clears in %.1f game-seconds (%.2f game-hours).",
                               smokeDissipationTime, dissipGH));
                y += 22;
                DraggableIntT(ix, y, "Generation Rate", &smokeGenerationRate, 1.0f, 1, 10,
                    TextFormat("Smoke per tick = fire level / %d. A level-6 fire produces %d smoke.",
                               smokeGenerationRate, 6 / smokeGenerationRate));
            }
            y += 22;
            if (PushButton(ix, y, "Clear Smoke")) {
                ClearSmoke();
            }
            y += 22;
            DrawTextShadow("Wet cells: 2-3x smoke. Rain: slower rise/dissipation", ix, y, 14, GRAY);
        }
        y += 22;

        // --- Steam ---
        if (SectionHeader(ix, y, "Steam", &sectionSteam)) {
            y += 18;
            ToggleBoolT(ix, y, "Enabled", &steamEnabled,
                "Master toggle for steam simulation. Steam rises from boiling water, spreads, and condenses back to water when cooled.");
            y += 22;
            DraggableFloatT(ix, y, "Rise Interval (s)", &steamRiseInterval, 0.01f, 0.01f, 2.0f,
                TextFormat("Steam rises one Z-level every %.2f game-seconds.", steamRiseInterval));
            y += 22;
            if (PushButton(ix, y, "Clear Steam")) {
                ClearSteam();
            }
            y += 22;

            // Advanced: condensation/generation parameters
            if (SectionHeader(ix + 10, y, "Advanced", &sectionSteamAdvanced)) {
                y += 18;
                DraggableIntT(ix + 10, y, "Condensation Temp", &steamCondensationTemp, 5.0f, 0, 100,
                    TextFormat("Steam condenses to water below %dC.", steamCondensationTemp));
                y += 22;
                DraggableIntT(ix + 10, y, "Generation Temp", &steamGenerationTemp, 5.0f, 80, 150,
                    TextFormat("Water boils to steam above %dC.", steamGenerationTemp));
                y += 22;
                DraggableIntT(ix + 10, y, "Condensation Chance", &steamCondensationChance, 1.0f, 1, 600,
                    TextFormat("1-in-%d chance per tick. Higher = steam lingers longer.", steamCondensationChance));
                y += 22;
                DraggableIntT(ix + 10, y, "Rise Flow", &steamRiseFlow, 1.0f, 1, STEAM_MAX_LEVEL,
                    TextFormat("%d units rise per tick. Higher = faster vertical movement.", steamRiseFlow));
            }
        }
        y += 22;

        // --- Temperature ---
        // NOTE: When adding tweakable values here, also update save/load logic in saveload.c
        if (SectionHeader(ix, y, "Temperature", &sectionTemperature)) {
            y += 18;
            ToggleBoolT(ix, y, "Enabled", &temperatureEnabled,
                "Master toggle for temperature simulation. Heat transfers between cells, affected by insulation.");
            y += 22;
            DraggableIntT(ix, y, "Surface Ambient", &ambientSurfaceTemp, 1.0f, -50, 200,
                TextFormat("%dC. (0=freeze, 20=room temp, 100=boiling).", ambientSurfaceTemp));
            y += 22;
            DraggableIntT(ix, y, "Depth Decay", &ambientDepthDecay, 1.0f, 0, 20,
                TextFormat("%dC per Z-level underground. z=-10 = %dC.",
                           ambientDepthDecay, ambientSurfaceTemp - ambientDepthDecay * 10));
            y += 22;
            DraggableFloatT(ix, y, "Transfer Interval (s)", &heatTransferInterval, 0.1f, 0.1f, 60.0f,
                TextFormat("Heat spreads between cells every %.1f game-seconds. Lower = faster heat flow.",
                           heatTransferInterval));
            y += 22;
            DraggableFloatT(ix, y, "Decay Interval (s)", &tempDecayInterval, 0.1f, 0.1f, 60.0f,
                TextFormat("Temperature decays toward ambient every %.1f game-seconds. Lower = faster cooling.",
                           tempDecayInterval));
            y += 22;
            if (PushButton(ix, y, "Reset to Ambient")) {
                ClearTemperature();
            }
            y += 22;

            // Advanced: insulation, heat sources, physics
            if (SectionHeader(ix + 10, y, "Advanced", &sectionTemperatureAdvanced)) {
                y += 18;
                DraggableIntT(ix + 10, y, "Wood Insulation %", &insulationTier1Rate, 1.0f, 1, 100,
                    TextFormat("Wood passes %d%% of heat (blocks %d%%).", insulationTier1Rate, 100 - insulationTier1Rate));
                y += 22;
                DraggableIntT(ix + 10, y, "Stone Insulation %", &insulationTier2Rate, 1.0f, 1, 100,
                    TextFormat("Stone passes %d%% of heat (blocks %d%%).", insulationTier2Rate, 100 - insulationTier2Rate));
                y += 22;
                DraggableIntT(ix + 10, y, "Heat Source Temp", &heatSourceTemp, 5.0f, 100, 1000,
                    TextFormat("Fire/furnace temperature: %dC.", heatSourceTemp));
                y += 22;
                DraggableIntT(ix + 10, y, "Cold Source Temp", &coldSourceTemp, 5.0f, -100, 0,
                    TextFormat("Ice/freezer temperature: %dC.", coldSourceTemp));
                y += 22;
                DrawTextShadow("Heat Physics:", ix + 10, y, 14, GRAY);
                y += 18;
                DraggableIntT(ix + 10, y, "Heat Rise Boost %", &heatRiseBoost, 5.0f, 50, 300,
                    TextFormat("%d%% upward transfer bonus (hot air rises).", heatRiseBoost));
                y += 22;
                DraggableIntT(ix + 10, y, "Heat Sink Reduction %", &heatSinkReduction, 5.0f, 10, 100,
                    TextFormat("%d%% downward transfer rate. Lower = heat stays up longer.", heatSinkReduction));
                y += 22;
                DraggableIntT(ix + 10, y, "Decay Rate %", &heatDecayPercent, 1.0f, 1, 50,
                    TextFormat("%d%% of excess heat lost per interval. Higher = faster cooling.", heatDecayPercent));
                y += 22;
                DraggableIntT(ix + 10, y, "Diagonal Transfer %", &diagonalTransferPercent, 5.0f, 30, 100,
                    TextFormat("%d%% of orthogonal transfer rate diagonally (~1.4x distance).", diagonalTransferPercent));
            }
        }
    }
    y += 22;

    // ========================================================================
    // [+] Weather & Time  (super-group)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Weather & Time", &sectionWeatherTime)) {
        y += 18;

        // --- Weather ---
        if (SectionHeader(ix, y, TextFormat("Weather: %s %.0f%%", GetWeatherName(weatherState.current),
                          weatherState.intensity * 100.0f), &sectionWeather)) {
            y += 18;

            // Compact status (2 lines instead of 3)
            DrawTextShadow(TextFormat("Wind: %.1f @ (%.1f, %.1f)  Timer: %.0f/%.0fs",
                           weatherState.windStrength, weatherState.windDirX, weatherState.windDirY,
                           weatherState.transitionTimer, weatherState.transitionDuration), ix, y, 12, GRAY);
            y += 18;

            // Weather trigger buttons — 2 rows instead of 3
            DrawTextShadow("Force:", ix, y, 14, GRAY);
            {
                float bx = ix + 40;
                bool clicked = false;
                bx += PushButtonInline(bx, y, "Clear", &clicked);
                if (clicked) { weatherState.current = WEATHER_CLEAR; weatherState.intensity = 1.0f; }
                clicked = false;
                bx += PushButtonInline(bx, y, "Cloudy", &clicked);
                if (clicked) { weatherState.current = WEATHER_CLOUDY; weatherState.intensity = 1.0f; }
                clicked = false;
                bx += PushButtonInline(bx, y, "Mist", &clicked);
                if (clicked) { weatherState.current = WEATHER_MIST; weatherState.intensity = 1.0f; }
                clicked = false;
                PushButtonInline(bx, y, "Rain", &clicked);
                if (clicked) { weatherState.current = WEATHER_RAIN; weatherState.intensity = 1.0f; }
            }
            y += 22;
            {
                float bx = ix;
                bool clicked = false;
                bx += PushButtonInline(bx, y, "Heavy", &clicked);
                if (clicked) { weatherState.current = WEATHER_HEAVY_RAIN; weatherState.intensity = 1.0f; }
                clicked = false;
                bx += PushButtonInline(bx, y, "Thunder", &clicked);
                if (clicked) { weatherState.current = WEATHER_THUNDERSTORM; weatherState.intensity = 1.0f; }
                clicked = false;
                PushButtonInline(bx, y, "Snow", &clicked);
                if (clicked) { weatherState.current = WEATHER_SNOW; weatherState.intensity = 1.0f; }
            }
            y += 22;

            // Settings
            ToggleBool(ix, y, "Weather Enabled", &weatherEnabled);
            y += 22;
            DraggableFloatT(ix, y, "Min Duration", &weatherMinDuration, 1.0f, 5.0f, 300.0f,
                TextFormat("Shortest weather spell: %.0f game-hours.", weatherMinDuration));
            y += 22;
            DraggableFloatT(ix, y, "Max Duration", &weatherMaxDuration, 1.0f, 10.0f, 600.0f,
                TextFormat("Longest weather spell: %.0f game-hours.", weatherMaxDuration));
            y += 22;
            DraggableFloatT(ix, y, "Rain Wetness Interval", &rainWetnessInterval, 0.5f, 0.5f, 30.0f,
                TextFormat("Ground gets wetter every %.1f game-hours during rain.", rainWetnessInterval));
            y += 22;
            DraggableFloatT(ix, y, "Heavy Rain Interval", &heavyRainWetnessInterval, 0.5f, 0.5f, 15.0f,
                TextFormat("Ground gets wetter every %.1f game-hours during heavy rain.", heavyRainWetnessInterval));
            y += 22;
            DraggableFloatT(ix, y, "Lightning Interval", &lightningInterval, 1.0f, 0.5f, 30.0f,
                TextFormat("Lightning strikes every %.1f game-hours in thunderstorms.", lightningInterval));
            y += 22;
            DraggableFloatT(ix, y, "Snow Accumulation", &snowAccumulationRate, 0.01f, 0.01f, 1.0f,
                TextFormat("Snow builds at %.2f levels/gh. Full cover (7) in ~%.0f game-hours.",
                           snowAccumulationRate, snowAccumulationRate > 0 ? 7.0f / snowAccumulationRate : 999.0f));
            y += 22;
            DraggableFloatT(ix, y, "Snow Melting", &snowMeltingRate, 0.01f, 0.01f, 0.5f,
                TextFormat("Snow melts at %.2f levels/gh. Full melt in ~%.0f game-hours.",
                           snowMeltingRate, snowMeltingRate > 0 ? 7.0f / snowMeltingRate : 999.0f));
        }
        y += 22;

        // --- Time ---
        if (SectionHeader(ix, y, "Time", &sectionTime)) {
            y += 18;

            // Game time status (compact — widget already shows season/day/time)
            DrawTextShadow(TextFormat("Game time: %.1fs  |  Temp: %dC", (float)gameTime, GetSeasonalSurfaceTemp()), ix, y, 14, GRAY);
            y += 22;

            // Game speed control
            DraggableFloatT(ix, y, "Game Speed", &gameSpeed, 0.1f, 0.0f, 100.0f,
                TextFormat("Simulation speed multiplier. At %.1fx, 1 real-second = %.1f game-seconds.%s",
                           gameSpeed, gameSpeed, gameSpeed == 0.0f ? " (PAUSED)" : ""));
            y += 22;

            // Speed presets — compact: Pause + speeds on 2 rows
            {
                bool clicked = false;
                float bx = ix;
                bx += PushButtonInline(bx, y, "Pause", &clicked); if (clicked) gameSpeed = 0.0f;
                clicked = false;
                bx += PushButtonInline(bx, y, "1x", &clicked); if (clicked) gameSpeed = 1.0f;
                clicked = false;
                bx += PushButtonInline(bx, y, "2x", &clicked); if (clicked) gameSpeed = 2.0f;
                clicked = false;
                PushButtonInline(bx, y, "3x", &clicked); if (clicked) gameSpeed = 3.0f;
            }
            y += 22;
            {
                bool clicked = false;
                float bx = ix;
                bx += PushButtonInline(bx, y, "5x", &clicked); if (clicked) gameSpeed = 5.0f;
                clicked = false;
                bx += PushButtonInline(bx, y, "10x", &clicked); if (clicked) gameSpeed = 10.0f;
                clicked = false;
                PushButtonInline(bx, y, "50x", &clicked); if (clicked) gameSpeed = 50.0f;
            }
            y += 22;

            // Day length
            float realDuration = gameSpeed > 0 ? dayLength / gameSpeed : 0;
            DraggableFloatT(ix, y, "Day Length", &dayLength, 10.0f, 10.0f, 3600.0f,
                TextFormat("Game-seconds per full day.\nAt 1x speed: %.1f real-%s per day.\nAt current %.1fx: %.1f real-%s per day.",
                           dayLength < 60 ? dayLength : dayLength / 60.0f, dayLength < 60 ? "seconds" : "minutes",
                           gameSpeed, realDuration < 60 ? realDuration : realDuration / 60.0f, realDuration < 60 ? "seconds" : "minutes"));
            y += 22;

            // Day length presets — inline
            DrawTextShadow("Day presets:", ix, y, 14, GRAY);
            {
                float bx = ix + 76;
                bool clicked = false;
                bx += PushButtonInline(bx, y, "Fast", &clicked); if (clicked) dayLength = 24.0f;
                clicked = false;
                bx += PushButtonInline(bx, y, "Normal", &clicked); if (clicked) dayLength = 60.0f;
                clicked = false;
                PushButtonInline(bx, y, "Slow", &clicked); if (clicked) dayLength = 720.0f;
            }
            y += 22;

            ToggleBool(ix, y, "Fixed Timestep", &useFixedTimestep);
            y += 22;

            // Seasons
            DrawTextShadow("Jump to Season:", ix, y, 14, GRAY);
            y += 18;
            {
                bool clicked = false;
                float bx = ix;
                bx += PushButtonInline(bx, y, "Spring", &clicked);
                if (clicked) { dayNumber = 1; if (seasonalAmplitude == 0) seasonalAmplitude = 20; }
                clicked = false;
                bx += PushButtonInline(bx, y, "Summer", &clicked);
                if (clicked) { dayNumber = daysPerSeason + 1; if (seasonalAmplitude == 0) seasonalAmplitude = 20; }
                clicked = false;
                bx += PushButtonInline(bx, y, "Autumn", &clicked);
                if (clicked) { dayNumber = daysPerSeason * 2 + 1; if (seasonalAmplitude == 0) seasonalAmplitude = 20; }
                clicked = false;
                PushButtonInline(bx, y, "Winter", &clicked);
                if (clicked) { dayNumber = daysPerSeason * 3 + 1; if (seasonalAmplitude == 0) seasonalAmplitude = 20; }
            }
            y += 22;

            DraggableIntT(ix, y, "Days per Season", &daysPerSeason, 1.0f, 1, 30,
                TextFormat("Days per season. Year = %d days (4 seasons). Current: %s (day %d of season).",
                           daysPerSeason * SEASON_COUNT, GetSeasonName(GetCurrentSeason()), (GetYearDay() % daysPerSeason) + 1));
            y += 22;
            DraggableIntT(ix, y, "Base Temp (C)", &baseSurfaceTemp, 1.0f, -20, 40,
                TextFormat("Base surface temperature before seasonal modulation. Current seasonal temp: %dC.", GetSeasonalSurfaceTemp()));
            y += 22;
            DraggableIntT(ix, y, "Seasonal Amplitude", &seasonalAmplitude, 1.0f, 0, 40,
                TextFormat("Temperature swing above/below base. 0=flat. Range: %dC to %dC.",
                           baseSurfaceTemp - seasonalAmplitude, baseSurfaceTemp + seasonalAmplitude));
        }
        y += 22;

        // --- Balance ---
        if (SectionHeader(ix, y, "Mover Balance", &sectionBalance)) {
            y += 18;
            bool changed = false;

            // Movement speed
            float tilesPerGameSec = balance.baseMoverSpeed / (float)CELL_SIZE;
            float oldBaseSpeed = balance.baseMoverSpeed;
            DraggableFloatT(ix, y, "Base Speed (px/s)", &balance.baseMoverSpeed, 10.0f, 50.0f, 800.0f,
                TextFormat("Base mover speed in pixels/sec. = %.1f tiles/game-sec.", tilesPerGameSec));
            y += 22;
            // Rescale all existing movers proportionally when base speed changes
            if (balance.baseMoverSpeed != oldBaseSpeed && oldBaseSpeed > 0) {
                float scale = balance.baseMoverSpeed / oldBaseSpeed;
                for (int i = 0; i < moverCount; i++) {
                    if (movers[i].active) movers[i].speed *= scale;
                }
            }
            DraggableFloatT(ix, y, "Speed Variance", &balance.moverSpeedVariance, 0.05f, 0.0f, 0.5f,
                TextFormat("Random speed spread per mover: +/-%.0f%%. Range: %.1f - %.1f tiles/game-sec.",
                           balance.moverSpeedVariance * 100.0f,
                           tilesPerGameSec * (1.0f - balance.moverSpeedVariance),
                           tilesPerGameSec * (1.0f + balance.moverSpeedVariance)));
            y += 22;

            // Day budget (game-hours)
            DrawTextShadow("Day Budget (game-hours):", ix, y, 14, GRAY);
            y += 18;
            changed |= DraggableFloatT(ix, y, "Work Hours/Day", &balance.workHoursPerDay, 0.5f, 1.0f, 24.0f,
                "Design target: how many hours per day a mover should work.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Sleep (Plank Bed)", &balance.sleepHoursInBed, 0.5f, 1.0f, 24.0f,
                "Game-hours to recover from exhausted to rested in a plank bed.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Sleep (Ground)", &balance.sleepOnGround, 0.5f, 1.0f, 48.0f,
                "Game-hours to recover from exhausted to rested on bare ground.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Hours to Starve", &balance.hoursToStarve, 0.5f, 1.0f, 48.0f,
                "Game-hours from full hunger to starvation.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Exhaust (Working)", &balance.hoursToExhaustWorking, 0.5f, 1.0f, 48.0f,
                "Game-hours of continuous work before exhaustion.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Exhaust (Idle)", &balance.hoursToExhaustIdle, 0.5f, 1.0f, 48.0f,
                "Game-hours of idling before exhaustion.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Eating Duration", &balance.eatingDurationGH, 0.05f, 0.05f, 4.0f,
                "Game-hours spent eating a meal.");
            y += 22;

            // Thresholds
            y += 4;
            DrawTextShadow("Thresholds (0-1):", ix, y, 14, GRAY);
            y += 18;
            changed |= DraggableFloatT(ix, y, "Hunger Seek", &balance.hungerSeekThreshold, 0.05f, 0.05f, 0.9f,
                "Seek food when hunger drops below this (idle).");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Hunger Critical", &balance.hungerCriticalThreshold, 0.01f, 0.01f, 0.5f,
                "Cancel current job to find food.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Energy Tired", &balance.energyTiredThreshold, 0.05f, 0.05f, 0.9f,
                "Seek rest when energy drops below this (idle).");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Energy Exhausted", &balance.energyExhaustedThreshold, 0.01f, 0.01f, 0.5f,
                "Cancel current job to find rest.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Energy Wake", &balance.energyWakeThreshold, 0.05f, 0.3f, 1.0f,
                "Stop sleeping when energy reaches this.");
            y += 22;

            // Multipliers
            y += 4;
            DrawTextShadow("Multipliers:", ix, y, 14, GRAY);
            y += 18;
            changed |= DraggableFloatT(ix, y, "Night Energy", &balance.nightEnergyMult, 0.05f, 0.5f, 3.0f,
                "Energy drain multiplier at night.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Carrying Energy", &balance.carryingEnergyMult, 0.05f, 0.5f, 3.0f,
                "Energy drain multiplier when hauling.");
            y += 22;
            changed |= DraggableFloatT(ix, y, "Hunger Speed Min", &balance.hungerSpeedPenaltyMin, 0.05f, 0.1f, 1.0f,
                "Minimum speed multiplier when starving.");
            y += 22;

            // Recalc derived rates if anything changed
            if (changed) RecalcBalanceTable();

            // Derived rates (read-only)
            y += 4;
            DrawTextShadow("Derived Rates (per game-hour):", ix, y, 14, GRAY);
            y += 18;
            DrawTextShadow(TextFormat("Hunger drain:      %.4f/gh", balance.hungerDrainPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Energy drain (work): %.4f/gh", balance.energyDrainWorkPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Energy drain (idle): %.4f/gh", balance.energyDrainIdlePerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Bed recovery:      %.4f/gh", balance.bedRecoveryPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Ground recovery:   %.4f/gh", balance.groundRecoveryPerGH), ix, y, 14, WHITE);
            y += 22;

            // Reset button
            {
                bool clicked = false;
                PushButtonInline(ix, y, "Reset Defaults", &clicked);
                if (clicked) InitBalance();
            }
            y += 22;
        }
        y += 22;
    }
    y += 22;

    // ========================================================================
    // [+] Rendering & Debug  (super-group: View inline, Lighting, Debug)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Rendering & Debug", &sectionRenderDebug)) {
        y += 18;

        // View — inline buttons, no accordion
        DrawTextShadow("View:", ix, y, 14, GRAY);
        {
            float bx = ix + 34;
            bool clicked = false;
            bx += PushButtonInline(bx, y, "Cutscene", &clicked); if (clicked) PlayTestCutscene();
            clicked = false;
            bx += PushButtonInline(bx, y, "Shk S", &clicked); if (clicked) TriggerScreenShake(2.0f, 0.2f);
            clicked = false;
            bx += PushButtonInline(bx, y, "Shk M", &clicked); if (clicked) TriggerScreenShake(4.0f, 0.4f);
            clicked = false;
            PushButtonInline(bx, y, "Shk L", &clicked); if (clicked) TriggerScreenShake(8.0f, 0.6f);
        }
        y += 22;

        // --- Lighting ---
        if (SectionHeader(ix, y, TextFormat("Lighting (%d src)", lightSourceCount), &sectionLighting)) {
            y += 18;
            {
                bool wasEnabled = lightingEnabled;
                ToggleBoolT(ix, y, "Enabled", &lightingEnabled,
                    "Master toggle for lighting system. When off, all tiles render at full brightness (no light/dark cycle).");
                if (wasEnabled != lightingEnabled) InvalidateLighting();
            }
            y += 22;
            {
                bool was = skyLightEnabled;
                ToggleBoolT(ix, y, "Sky Light", &skyLightEnabled,
                    "Compute sky light from open columns. Sky light intensity follows time-of-day sky color.");
                if (was != skyLightEnabled) InvalidateLighting();
            }
            y += 22;
            {
                bool was = blockLightEnabled;
                ToggleBoolT(ix, y, "Block Light", &blockLightEnabled,
                    "Compute light from placed sources (torches). Colored BFS flood fill through open cells.");
                if (was != blockLightEnabled) InvalidateLighting();
            }
            y += 22;

            // Ambient minimum
            DrawTextShadow("Ambient Minimum:", ix, y, 14, GRAY);
            y += 18;
            DraggableIntT(ix, y, "Red", &lightAmbientR, 1.0f, 0, 255,
                "Minimum red component. Prevents completely black tiles.");
            y += 22;
            DraggableIntT(ix, y, "Green", &lightAmbientG, 1.0f, 0, 255,
                "Minimum green component. Prevents completely black tiles.");
            y += 22;
            DraggableIntT(ix, y, "Blue", &lightAmbientB, 1.0f, 0, 255,
                "Minimum blue component. Slightly higher default gives a cool moonlight feel in darkness.");
            y += 22;

            // Torch defaults
            DrawTextShadow("Torch Defaults:", ix, y, 14, GRAY);
            y += 18;
            DraggableIntT(ix, y, "Intensity", &lightDefaultIntensity, 1.0f, 1, 15,
                TextFormat("Propagation radius for new torches. At %d, light reaches %d tiles away.", lightDefaultIntensity, lightDefaultIntensity));
            y += 22;
            DraggableIntT(ix, y, "R", &lightDefaultR, 1.0f, 0, 255, "Red component of new torches.");
            y += 22;
            DraggableIntT(ix, y, "G", &lightDefaultG, 1.0f, 0, 255, "Green component of new torches.");
            y += 22;
            DraggableIntT(ix, y, "B", &lightDefaultB, 1.0f, 0, 255, "Blue component of new torches.");
            y += 22;

            // Color preview
            {
                Color preview = { (uint8_t)lightDefaultR, (uint8_t)lightDefaultG, (uint8_t)lightDefaultB, 255 };
                DrawRectangle((int)ix, (int)y, 60, 14, preview);
                DrawRectangleLinesEx((Rectangle){ix, y, 60, 14}, 1, GRAY);
                DrawTextShadow("Preview", ix + 65, y, 14, GRAY);
            }
            y += 18;

            if (PushButton(ix, y, "Clear Lights")) {
                ClearLightSources();
                InvalidateLighting();
            }
        }
        y += 22;

        // --- Debug ---
        if (SectionHeader(ix, y, "Debug", &sectionDebug)) {
            y += 18;

            // Rendering
            if (SectionHeader(ix + 10, y, "Rendering", &sectionDebugRendering)) {
                y += 18;
                ToggleBool(ix + 10, y, "Show Movers", &showMovers);
                y += 22;
                ToggleBool(ix + 10, y, "Pixel Perfect", &usePixelPerfectMovers);
                y += 22;
                ToggleBool(ix + 10, y, "Show Items", &showItems);
                y += 22;
                ToggleBool(ix + 10, y, "Cull Drawing", &cullDrawing);
            }
            y += 22;

            // Pathfinding
            if (SectionHeader(ix + 10, y, "Pathfinding", &sectionDebugPathfinding)) {
                y += 18;
                ToggleBool(ix + 10, y, "Show Graph", &showGraph);
                y += 22;
                ToggleBool(ix + 10, y, "Show Entrances", &showEntrances);
                y += 22;
                ToggleBool(ix + 10, y, "Show Chunks", &showChunkBoundaries);
                y += 22;
                ToggleBool(ix + 10, y, "Show Paths", &showMoverPaths);
                y += 22;
                ToggleBool(ix + 10, y, "Show Job Lines", &showJobLines);
            }
            y += 22;

            // Mover Diagnostics
            if (SectionHeader(ix + 10, y, "Mover Diagnostics", &sectionDebugMovers)) {
                y += 18;
                ToggleBool(ix + 10, y, "Show Neighbors", &showNeighborCounts);
                y += 22;
                ToggleBool(ix + 10, y, "Show Open Area", &showOpenArea);
                y += 22;
                ToggleBool(ix + 10, y, "Show Knots", &showKnotDetection);
                y += 22;
                ToggleBool(ix + 10, y, "Show Stuck", &showStuckDetection);
            }
            y += 22;

            // Overlays
            if (SectionHeader(ix + 10, y, "Overlays", &sectionDebugOverlays)) {
                y += 18;
                ToggleBool(ix + 10, y, "Sim Sources", &showSimSources);
                y += 22;
                ToggleBool(ix + 10, y, "Temperature", &showTemperatureOverlay);
            }
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
            size_t fireSize = sizeof(FireCell) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t smokeSize = sizeof(SmokeCell) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t steamSize = sizeof(SteamCell) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t temperatureSize = sizeof(TempCell) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t cellFlagsSize = sizeof(uint8_t) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t groundWearSize = sizeof(int) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;

            // Pathfinding
            size_t entrancesSize = sizeof(Entrance) * MAX_ENTRANCES;
            size_t pathSize = sizeof(Point) * MAX_PATH;  // Global path scratch buffer
            size_t edgesSize = sizeof(GraphEdge) * MAX_EDGES;
            size_t nodeDataSize = sizeof(AStarNode) * MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH;
            size_t chunkDirtySize = sizeof(bool) * MAX_GRID_DEPTH * MAX_CHUNKS_Y * MAX_CHUNKS_X;
            size_t ladderLinksSize = sizeof(LadderLink) * MAX_LADDERS;
            size_t rampLinksSize = sizeof(RampLink) * MAX_RAMP_LINKS;
            size_t abstractNodesSize = sizeof(AbstractNode) * MAX_ABSTRACT_NODES;
            size_t abstractPathSize = sizeof(int) * (MAX_ENTRANCES + 2);
            size_t adjListSize = sizeof(int) * MAX_ENTRANCES * MAX_EDGES_PER_NODE;  // adjList[MAX_ENTRANCES][MAX_EDGES_PER_NODE]
            size_t adjListCountSize = sizeof(int) * MAX_ENTRANCES;
            size_t entranceHashSize = 16 * 32768;  // EntranceHashEntry (16 bytes) * ENTRANCE_HASH_SIZE

            // Entities
            size_t moversSize = sizeof(Mover) * MAX_MOVERS;
            size_t moverRenderSize = sizeof(MoverRenderData) * MAX_MOVERS;
            size_t itemsSize = sizeof(Item) * MAX_ITEMS;
            size_t jobsSize = sizeof(Job) * MAX_JOBS;
            size_t stockpilesSize = sizeof(Stockpile) * MAX_STOCKPILES;
            size_t blueprintsSize = sizeof(Blueprint) * MAX_BLUEPRINTS;
            size_t gatherZonesSize = sizeof(GatherZone) * MAX_GATHER_ZONES;
            size_t workshopsSize = sizeof(Workshop) * MAX_WORKSHOPS;

            // Spatial grids (heap allocated)
            size_t moverSpatialGrid = (moverGrid.cellCount + 1) * sizeof(int) * 2 + MAX_MOVERS * sizeof(int);
            size_t itemSpatialGrid = (itemGrid.cellCount + 1) * sizeof(int) * 2 + MAX_ITEMS * sizeof(int);

            size_t totalGrid = gridSize + designationsSize + waterSize + fireSize + smokeSize + steamSize + temperatureSize + cellFlagsSize + groundWearSize;
            size_t totalPathfinding = entrancesSize + pathSize + edgesSize + nodeDataSize + chunkDirtySize + ladderLinksSize + rampLinksSize + abstractNodesSize + abstractPathSize + adjListSize + adjListCountSize + entranceHashSize;
            size_t totalEntities = moversSize + moverRenderSize + itemsSize + jobsSize + stockpilesSize + blueprintsSize + gatherZonesSize + workshopsSize;
            size_t totalSpatial = moverSpatialGrid + itemSpatialGrid;
            size_t total = totalGrid + totalPathfinding + totalEntities + totalSpatial;

            // Helper macro for subsection headers
            #define MEM_SUBSECTION(label, sizeVar, boolVar) do { \
                const char* hdr = boolVar ? TextFormat("[-] %s (%.1f MB)", label, (sizeVar) / (1024.0f * 1024.0f)) \
                                          : TextFormat("[+] %s (%.1f MB)", label, (sizeVar) / (1024.0f * 1024.0f)); \
                int hdrW = MeasureText(hdr, 14); \
                float hdrX = rightEdge - hdrW; \
                bool hovered = mouse.x >= hdrX && mouse.x < hdrX + hdrW + 10 && mouse.y >= y && mouse.y < y + 16; \
                DrawTextShadow(hdr, (int)hdrX, (int)y, 14, hovered ? YELLOW : GRAY); \
                if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) boolVar = !boolVar; \
                y += 16; \
            } while(0)

            // Grid Data subsection
            MEM_SUBSECTION("Grid", totalGrid, sectionMemGrid);
            if (sectionMemGrid) {
                DrawTextShadow(TextFormat("  Cells:        %5.1f MB", gridSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Designations: %5.1f MB", designationsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Water:        %5.1f MB", waterSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Fire:         %5.1f MB", fireSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Smoke:        %5.1f MB", smokeSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Steam:        %5.1f MB", steamSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Temperature:  %5.1f MB", temperatureSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  CellFlags:    %5.1f MB", cellFlagsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  GroundWear:   %5.1f MB", groundWearSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            }

            // Pathfinding subsection
            MEM_SUBSECTION("Pathfinding", totalPathfinding, sectionMemPath);
            if (sectionMemPath) {
                DrawTextShadow(TextFormat("  NodeData:     %5.1f MB", nodeDataSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  AdjList:      %5.1f MB", adjListSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Path:         %5.1f MB", pathSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Entrances:    %5.1f MB", entrancesSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  EntranceHash: %5.1f KB", entranceHashSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Edges:        %5.1f MB", edgesSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  LadderLinks:  %5.1f KB", ladderLinksSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  RampLinks:    %5.1f KB", rampLinksSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  AbstractNodes:%5.1f KB", abstractNodesSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  AdjListCount: %5.1f KB", adjListCountSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  ChunkDirty:   %5.1f KB", chunkDirtySize / 1024.0f), x, y, 14, WHITE); y += 16;
            }

            // Entities subsection
            MEM_SUBSECTION("Entities", totalEntities, sectionMemEntities);
            if (sectionMemEntities) {
                DrawTextShadow(TextFormat("  Movers:       %5.1f MB", moversSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  MoverRender:  %5.1f KB", moverRenderSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Items:        %5.1f MB", itemsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Jobs:         %5.1f MB", jobsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Workshops:    %5.1f KB", workshopsSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Stockpiles:   %5.1f KB", stockpilesSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Blueprints:   %5.1f KB", blueprintsSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  GatherZones:  %5.1f KB", gatherZonesSize / 1024.0f), x, y, 14, WHITE); y += 16;
            }

            // Spatial subsection
            MEM_SUBSECTION("Spatial", totalSpatial, sectionMemSpatial);
            if (sectionMemSpatial) {
                DrawTextShadow(TextFormat("  MoverGrid:    %5.1f KB", moverSpatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  ItemGrid:     %5.1f KB", itemSpatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
            }

            #undef MEM_SUBSECTION

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
