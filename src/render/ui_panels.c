// render/ui_panels.c - UI panel drawing functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/lighting.h"
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
    int boxWidth = 140;  // Fixed width to accommodate "Day 999  23:59"

    // Get current time info
    int hours = (int)timeOfDay;
    int minutes = (int)((timeOfDay - hours) * 60);

    // Format time string
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Day %d  %02d:%02d", dayNumber, hours, minutes);

    // Measure text width (height is approximately fontSize for most fonts)
    int textWidth = MeasureTextUI(timeStr, fontSize);
    int boxHeight = fontSize + paddingY * 2;

    // Draw sky-colored background rectangle
    Color skyColor = GetSkyColorForTime(timeOfDay);
    DrawRectangle((int)x, (int)y, boxWidth, boxHeight, skyColor);
    DrawRectangleLines((int)x, (int)y, boxWidth, boxHeight, (Color){100, 100, 100, 255});

    // Draw time text centered in box
    int textX = (int)x + (boxWidth - textWidth) / 2;
    int textY = (int)y + paddingY;
    DrawTextShadow(timeStr, textX, textY, fontSize, WHITE);

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


void DrawUI(void) {
    ui_begin_frame();
    float y = 30.0f;
    float x = 10.0f;

    // === VIEW ===
    if (SectionHeader(x, y, "View", &sectionView)) {
        y += 18;
        if (PushButton(x, y, "Cutscene")) {
            PlayTestCutscene();
        }
        y += 22;
        if (PushButton(x, y, "Shake: Small")) {
            TriggerScreenShake(2.0f, 0.2f);
        }
        y += 22;
        if (PushButton(x, y, "Shake: Medium")) {
            TriggerScreenShake(4.0f, 0.4f);
        }
        y += 22;
        if (PushButton(x, y, "Shake: Large")) {
            TriggerScreenShake(8.0f, 0.6f);
        }
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
        CycleOption(x, y, "Terrain", terrainNames, 21, &currentTerrain);
        y += 22;
        bool isHillsTerrain = (currentTerrain == 17 || currentTerrain == 18 || currentTerrain == 19);
        bool isHillsWater = (currentTerrain == 19);
        if (isHillsTerrain) {
            DraggableFloatT(x, y, "Ramp Density", &rampDensity, 0.02f, 0.0f, 1.0f,
                "Hills ramp placement density (0=none, 1=all). Lower reduces HPA* graph size.");
            y += 22;
            DraggableFloatT(x, y, "Ramp Noise Scale", &rampNoiseScale, 0.005f, 0.005f, 0.2f,
                "Controls ramp cluster size for hills generators (higher = larger clusters).");
            y += 22;
        }
        if (isHillsWater) {
            DraggableIntT(x, y, "River Count", &hillsWaterRiverCount, 1.0f, 0, 6,
                "HillsSoilsWater: number of rivers (0 disables rivers).");
            y += 22;
            DraggableIntT(x, y, "River Width", &hillsWaterRiverWidth, 1.0f, 1, 4,
                "HillsSoilsWater: river width radius (1-4).");
            y += 22;
            DraggableIntT(x, y, "Lake Count", &hillsWaterLakeCount, 1.0f, 0, 6,
                "HillsSoilsWater: number of lakes (0 disables lakes).");
            y += 22;
            DraggableIntT(x, y, "Lake Radius", &hillsWaterLakeRadius, 1.0f, 3, 12,
                "HillsSoilsWater: lake radius (3-12).");
            y += 22;
            DraggableFloatT(x, y, "Wetness Bias", &hillsWaterWetnessBias, 0.05f, 0.0f, 1.0f,
                "HillsSoilsWater: wetness boost near water (peat/clay bias).");
            y += 22;
            ToggleBoolT(x, y, "Conn Report", &hillsWaterConnectivityReport,
                "Logs walkability connectivity stats after generation.");
            y += 22;
            ToggleBoolT(x, y, "Fix Tiny Pockets", &hillsWaterConnectivityFixSmall,
                "Fills tiny disconnected walkable pockets after generation.");
            y += 22;
            DraggableIntT(x, y, "Tiny Size", &hillsWaterConnectivitySmallThreshold, 1.0f, 5, 200,
                "Size threshold for tiny pocket fill (cells).");
            y += 22;
        }
        if (PushButton(x, y, "Randomize Seed")) {
            worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);
            AddMessage(TextFormat("New seed: %llu", (unsigned long long)worldSeed), GREEN);
        }
        y += 22;
        if (PushButton(x, y, "Generate Terrain")) {
            GenerateCurrentTerrain();
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            InitSimActivity();
            InitTemperature();
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
            FillGroundLevel();
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
            InitSimActivity();
            InitTemperature();
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
            FillGroundLevel();
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
            BuildEntrances();
            BuildGraph();
            InitSimActivity();
            InitTemperature();
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
            FillGroundLevel();
            InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
            InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
            InitSimActivity();
            InitTemperature();
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
            int bufferSize = gridDepth * (16 + floorDataSize) + 1;  // 16 for "floor:XXX\n" with margin
            char* buffer = malloc(bufferSize);
            if (!buffer) {
                AddMessage("Failed to allocate clipboard buffer", RED);
            } else {
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

    }
    y += 22;

    // === ANIMALS ===
    y += 8;
    if (PushButton(x + 130, y, "+")) {
        SpawnAnimal(ANIMAL_GRAZER, currentViewZ, BEHAVIOR_SIMPLE_GRAZER);
    }
    if (PushButton(x + 150, y, "+S")) {
        SpawnAnimal(ANIMAL_GRAZER, currentViewZ, BEHAVIOR_STEERING_GRAZER);
    }
    if (PushButton(x + 180, y, "+W")) {
        SpawnAnimal(ANIMAL_PREDATOR, currentViewZ, BEHAVIOR_PREDATOR);
    }
    if (SectionHeader(x, y, TextFormat("Animals (%d/%d)", CountActiveAnimals(), MAX_ANIMALS), &sectionAnimals)) {
        y += 18;
        if (PushButton(x, y, "Clear Animals")) {
            ClearAnimals();
        }
    }
    y += 22;

    // === JOBS ===
    y += 8;
    if (SectionHeader(x, y, TextFormat("Jobs (%d items)", itemCount), &sectionJobs)) {
        y += 18;
        DraggableIntLog(x, y, "Count", &itemCountSetting, 1.0f, 1, MAX_ITEMS);
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
            float bx = (float)x;
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
                if (bx > x + 180) { bx = (float)x; y += 22; }
            }
            if (bx > (float)x) y += 22;
        }

        if (PushButton(x, y, "Clear Items")) {
            ClearItems();
        }
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
        ToggleBoolT(x, y, "Enabled", &waterEnabled,
            "Master toggle for water simulation. Water flows down, spreads horizontally, and uses pressure to rise through U-bends.");
        y += 22;
        ToggleBoolT(x, y, "Evaporation", &waterEvaporationEnabled,
            "When enabled, shallow water (level 1) has a chance to evaporate each tick. Disable for testing water mechanics.");
        y += 22;
        DraggableFloatT(x, y, "Evap Interval (s)", &waterEvapInterval, 1.0f, 1.0f, 120.0f,
            TextFormat("Game-seconds between evaporation attempts for shallow water. At %.0fs, puddles last ~%.0f seconds.",
                       waterEvapInterval, waterEvapInterval));
        y += 22;

        // Speed multipliers subsection
        DrawTextShadow("Mover Speed in Water:", x, y, 14, GRAY);
        y += 18;
        DraggableFloatT(x, y, "Shallow (1-2)", &waterSpeedShallow, 0.05f, 0.1f, 1.0f,
            "Speed multiplier in shallow water (levels 1-2). At 0.85, movers move at 85% speed. Lower = slower movement.");
        y += 22;
        DraggableFloatT(x, y, "Medium (3-4)", &waterSpeedMedium, 0.05f, 0.1f, 1.0f,
            "Speed multiplier in medium water (levels 3-4). At 0.6, movers move at 60% speed. Lower = slower movement.");
        y += 22;
        DraggableFloatT(x, y, "Deep (5-7)", &waterSpeedDeep, 0.05f, 0.1f, 1.0f,
            "Speed multiplier in deep water (levels 5-7). At 0.35, movers move at 35% speed. Lower = slower movement.");
        y += 22;
        DraggableFloatT(x, y, "Mud Speed", &mudSpeedMultiplier, 0.05f, 0.1f, 1.0f,
            "Speed multiplier on muddy terrain (soil with wetness >= 2). At 0.6, movers move at 60% speed.");
        y += 22;
        DraggableFloatT(x, y, "Wetness Sync", &wetnessSyncInterval, 0.5f, 0.5f, 30.0f,
            "Game-seconds between water-to-wetness sync. Lower = soil gets wet faster from water. Default: 2.0s.");
        y += 22;

        if (PushButton(x, y, "Clear Water")) {
            ClearWater();
        }
        y += 22;
        DrawTextShadow(IsRaining() ? "Rain (active):" : "Rain:", x, y, 14, IsRaining() ? BLUE : GRAY);
        y += 18;
        {
            float bx = (float)x;
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
    }
    y += 22;

    // === FIRE ===
    y += 8;
    if (SectionHeader(x, y, "Fire", &sectionFire)) {
        y += 18;
        ToggleBoolT(x, y, "Enabled", &fireEnabled,
            "Master toggle for fire simulation. Fire consumes fuel, spreads to neighbors, and generates smoke.");
        y += 22;
        DraggableFloatT(x, y, "Spread Interval (s)", &fireSpreadInterval, 0.1f, 0.1f, 10.0f,
            TextFormat("Game-seconds between fire spread attempts. At %.1fs, fire tries to spread %.1f times per second.",
                       fireSpreadInterval, 1.0f / fireSpreadInterval));
        y += 22;
        DraggableFloatT(x, y, "Fuel Interval (s)", &fireFuelInterval, 0.1f, 0.1f, 10.0f,
            TextFormat("Game-seconds between fuel consumption ticks. At %.1fs, fuel depletes %.1f times per second.",
                       fireFuelInterval, 1.0f / fireFuelInterval));
        y += 22;
        DraggableIntT(x, y, "Water Reduction %", &fireWaterReduction, 1.0f, 1, 100,
            "Spread chance multiplier for cells adjacent to water. At 25%, fire spreads 4x slower near water. Lower = water is more effective.");
        y += 22;

        // Spread chance formula subsection
        DrawTextShadow("Spread Chance Formula:", x, y, 14, GRAY);
        y += 18;
        DraggableIntT(x, y, "Base Chance %", &fireSpreadBase, 1.0f, 0, 50,
            "Base spread chance before fire level bonus. Formula: base + (level * perLevel). At 10%, even weak fires have some spread chance.");
        y += 22;
        DraggableIntT(x, y, "Per Level %", &fireSpreadPerLevel, 1.0f, 0, 30,
            "Additional spread chance per fire level. At 10%, level 2 fire has +20%, level 7 has +70%. Higher = intense fires spread much faster.");
        y += 22;

        // Show calculated spread chances
        int minSpread = fireSpreadBase + (FIRE_MIN_SPREAD_LEVEL * fireSpreadPerLevel);
        int maxSpread = fireSpreadBase + (FIRE_MAX_LEVEL * fireSpreadPerLevel);
        DrawTextShadow(TextFormat("Level 2: %d%%, Level 7: %d%%", minSpread, maxSpread), x, y, 14, GRAY);
        y += 18;

        if (PushButton(x, y, "Clear Fire")) {
            ClearFire();
        }
    }
    y += 22;

    // === SMOKE ===
    y += 8;
    if (SectionHeader(x, y, "Smoke", &sectionSmoke)) {
        y += 18;
        ToggleBoolT(x, y, "Enabled", &smokeEnabled,
            "Master toggle for smoke simulation. Smoke rises, spreads horizontally, fills enclosed spaces, and gradually dissipates.");
        y += 22;
        DraggableFloatT(x, y, "Rise Interval (s)", &smokeRiseInterval, 0.01f, 0.01f, 2.0f,
            TextFormat("Game-seconds between smoke rise attempts. At %.2fs, smoke rises %.1f times per game-second.",
                       smokeRiseInterval, 1.0f / smokeRiseInterval));
        y += 22;
        DraggableFloatT(x, y, "Dissipation Time (s)", &smokeDissipationTime, 0.1f, 0.5f, 30.0f,
            TextFormat("Game-seconds for smoke to fully dissipate (all 7 levels). At %.1fs, each level fades in ~%.2fs.",
                       smokeDissipationTime, smokeDissipationTime / 7.0f));
        y += 22;
        DraggableIntT(x, y, "Generation Rate", &smokeGenerationRate, 1.0f, 1, 10,
            "Smoke generated = fire level / this value. Lower = more smoke per fire. At 3, a level-6 fire produces 2 smoke.");
        y += 22;
        if (PushButton(x, y, "Clear Smoke")) {
            ClearSmoke();
        }
    }
    y += 22;

    // === STEAM ===
    y += 8;
    if (SectionHeader(x, y, "Steam", &sectionSteam)) {
        y += 18;
        ToggleBoolT(x, y, "Enabled", &steamEnabled,
            "Master toggle for steam simulation. Steam rises from boiling water, spreads, and condenses back to water when cooled.");
        y += 22;
        DraggableFloatT(x, y, "Rise Interval (s)", &steamRiseInterval, 0.01f, 0.01f, 2.0f,
            TextFormat("Game-seconds between steam rise attempts. At %.2fs, steam rises %.1f times per game-second.",
                       steamRiseInterval, 1.0f / steamRiseInterval));
        y += 22;
        DraggableIntT(x, y, "Condensation Temp", &steamCondensationTemp, 5.0f, 0, 100,
            TextFormat("Temperature below which steam condenses to water: %d C. Steam lingers longer at higher values.", steamCondensationTemp));
        y += 22;
        DraggableIntT(x, y, "Generation Temp", &steamGenerationTemp, 5.0f, 80, 150,
            TextFormat("Temperature at which water boils to steam: %d C. Standard is 100C (boiling point).", steamGenerationTemp));
        y += 22;
        DraggableIntT(x, y, "Condensation Chance", &steamCondensationChance, 1.0f, 1, 600,
            TextFormat("1 in %d ticks attempts condensation. Higher = slower condensation.", steamCondensationChance));
        y += 22;
        DraggableIntT(x, y, "Rise Flow", &steamRiseFlow, 1.0f, 1, STEAM_MAX_LEVEL,
            TextFormat("Units of steam that rise per attempt: %d. Higher = faster rising.", steamRiseFlow));
        y += 22;
        if (PushButton(x, y, "Clear Steam")) {
            ClearSteam();
        }
    }
    y += 22;

    // === TEMPERATURE ===
    y += 8;
    // NOTE: When adding tweakable values here, also update save/load logic in saveload.c
    if (SectionHeader(x, y, "Temperature", &sectionTemperature)) {
        y += 18;
        ToggleBoolT(x, y, "Enabled", &temperatureEnabled,
            "Master toggle for temperature simulation. Heat transfers between cells, affected by insulation.");
        y += 22;
        DraggableIntT(x, y, "Surface Ambient", &ambientSurfaceTemp, 1.0f, -50, 200,
            TextFormat("Surface temperature: %d C. 0=freeze, 20=room temp, 100=boiling.",
                       ambientSurfaceTemp));
        y += 22;
        DraggableIntT(x, y, "Depth Decay", &ambientDepthDecay, 1.0f, 0, 20,
            "Temperature decrease per Z-level underground. At 5, z=-10 is 50 degrees colder than surface.");
        y += 22;
        DraggableFloatT(x, y, "Transfer Interval (s)", &heatTransferInterval, 0.1f, 0.1f, 60.0f,
            TextFormat("Game-seconds between heat transfer steps. At %.1fs, heat transfers %.1f times per second.",
                       heatTransferInterval, 1.0f / heatTransferInterval));
        y += 22;
        DraggableFloatT(x, y, "Decay Interval (s)", &tempDecayInterval, 0.1f, 0.1f, 60.0f,
            TextFormat("Game-seconds between temperature decay steps. At %.1fs, decay happens %.1f times per second.",
                       tempDecayInterval, 1.0f / tempDecayInterval));
        y += 22;
        DraggableIntT(x, y, "Wood Insulation %", &insulationTier1Rate, 1.0f, 1, 100,
            "Heat transfer rate through wood walls. Lower = better insulation. At 20%, wood blocks 80% of heat.");
        y += 22;
        DraggableIntT(x, y, "Stone Insulation %", &insulationTier2Rate, 1.0f, 1, 100,
            "Heat transfer rate through stone walls. Lower = better insulation. At 5%, stone blocks 95% of heat.");
        y += 22;
        DraggableIntT(x, y, "Heat Source Temp", &heatSourceTemp, 5.0f, 100, 1000,
            TextFormat("Temperature of heat sources (fire/furnace): %d C. Water boils at 100C.", heatSourceTemp));
        y += 22;
        DraggableIntT(x, y, "Cold Source Temp", &coldSourceTemp, 5.0f, -100, 0,
            TextFormat("Temperature of cold sources (ice/freezer): %d C. Water freezes at 0C.", coldSourceTemp));
        y += 22;

        // Heat physics subsection
        DrawTextShadow("Heat Physics:", x, y, 14, GRAY);
        y += 18;
        DraggableIntT(x, y, "Heat Rise Boost %", &heatRiseBoost, 5.0f, 50, 300,
            "Heat transfer boost when rising (hot air rises). At 150%, heat moves 50% faster upward. Higher = stronger convection.");
        y += 22;
        DraggableIntT(x, y, "Heat Sink Reduction %", &heatSinkReduction, 5.0f, 10, 100,
            "Heat transfer reduction when sinking. At 50%, heat moves 50% slower downward. Lower = heat stays up longer.");
        y += 22;
        DraggableIntT(x, y, "Decay Rate %", &heatDecayPercent, 1.0f, 1, 50,
            "Percentage of temperature difference that decays toward ambient each interval. Higher = faster return to ambient.");
        y += 22;
        DraggableIntT(x, y, "Diagonal Transfer %", &diagonalTransferPercent, 5.0f, 30, 100,
            "Diagonal heat transfer as percentage of orthogonal. At 70%, diagonal neighbors receive 70% as much heat. Due to ~1.4x distance.");
        y += 22;

        if (PushButton(x, y, "Reset to Ambient")) {
            ClearTemperature();
        }
    }
    y += 22;

    // === ENTROPY (Ground Wear) ===
    y += 8;
    if (SectionHeader(x, y, "Entropy", &sectionEntropy)) {
        y += 18;
        ToggleBoolT(x, y, "Enabled", &groundWearEnabled,
            "Creates emergent paths: grass becomes dirt when trampled, dirt recovers to grass when left alone.");
        y += 22;
        DraggableIntT(x, y, "Trample Amount", &wearTrampleAmount, 1.0f, 1, 100,
            "Wear added each time a mover steps on a tile. Higher = paths form faster. Combined with thresholds, controls how many passes to create a path.");
        y += 22;
        DraggableIntT(x, y, "Grass->Dirt Threshold", &wearGrassToDirt, 50.0f, 100, 10000,
            "Wear level at which grass becomes dirt. At 1000 with trample=1, it takes 1000 mover steps to wear a path.");
        y += 22;
        DraggableIntT(x, y, "Dirt->Grass Threshold", &wearDirtToGrass, 50.0f, 0, 5000,
            "Wear level below which dirt recovers to grass. Should be lower than Grass->Dirt to create hysteresis and stable paths.");
        y += 22;
        DraggableIntT(x, y, "Decay Rate", &wearDecayRate, 1.0f, 1, 100,
            "Wear removed per decay tick. Higher = faster path recovery. Natural regrowth that competes with trampling.");
        y += 22;
        DraggableFloatT(x, y, "Recovery Interval (s)", &wearRecoveryInterval, 0.5f, 0.1f, 60.0f,
            TextFormat("Game-seconds between wear decay ticks. At %.1fs, wear decays %.1f times per second.",
                       wearRecoveryInterval, 1.0f / wearRecoveryInterval));
        y += 22;

        // Calculate and display regrow time in game-seconds
        int decaySteps = (wearMax - wearDirtToGrass) / wearDecayRate;
        float gameSecondsToRegrow = decaySteps * wearRecoveryInterval;
        if (gameSecondsToRegrow < 60) {
            DrawTextShadow(TextFormat("Regrow time: %.1fs game-time", gameSecondsToRegrow), x, y, 14, GRAY);
        } else {
            DrawTextShadow(TextFormat("Regrow time: %.1fm game-time", gameSecondsToRegrow / 60.0f), x, y, 14, GRAY);
        }
        y += 18;

        if (PushButton(x, y, "Clear Wear")) {
            ClearGroundWear();
        }
    }
    y += 22;

    // === TREES ===
    y += 8;
    if (SectionHeader(x, y, "Trees", &sectionTrees)) {
        y += 18;
        DraggableIntT(x, y, "Sapling Grow", &saplingGrowTicks, 10.0f, 10, 36000,
            TextFormat("Time for sapling to become trunk: %.1f seconds (%d ticks)", saplingGrowTicks / 60.0f, saplingGrowTicks));
        y += 22;
        DraggableIntT(x, y, "Trunk Grow", &trunkGrowTicks, 5.0f, 5, 500,
            TextFormat("Time between trunk growth stages: %.1f seconds (%d ticks)", trunkGrowTicks / 60.0f, trunkGrowTicks));
        y += 22;
        ToggleBoolT(x, y, "Sapling Regrowth", &saplingRegrowthEnabled,
            "Enable natural sapling spawning on untrampled grass. Saplings appear over time in wilderness areas.");
        y += 22;
        DraggableIntT(x, y, "Regrowth Chance", &saplingRegrowthChance, 1.0f, 0, 100,
            TextFormat("Chance per 10000 per interval for sapling to spawn on tall grass. At %d, roughly %.2f%% chance.", 
                       saplingRegrowthChance, saplingRegrowthChance / 100.0f));
        y += 22;
        DraggableIntT(x, y, "Min Tree Distance", &saplingMinTreeDistance, 1.0f, 1, 10,
            TextFormat("Minimum tiles from existing trees/saplings for new sapling to spawn. At %d, trees spread more slowly.", saplingMinTreeDistance));
        y += 22;
        DrawTextShadow(TextFormat("Sandbox Tree Type: %s", TreeTypeName(currentTreeType)), x, y, 14, GRAY);
    }
    y += 22;

    // === LIGHTING ===
    y += 8;
    if (SectionHeader(x, y, TextFormat("Lighting (%d src)", lightSourceCount), &sectionLighting)) {
        y += 18;
        {
            bool wasEnabled = lightingEnabled;
            ToggleBoolT(x, y, "Enabled", &lightingEnabled,
                "Master toggle for lighting system. When off, all tiles render at full brightness (no light/dark cycle).");
            if (wasEnabled != lightingEnabled) InvalidateLighting();
        }
        y += 22;
        {
            bool was = skyLightEnabled;
            ToggleBoolT(x, y, "Sky Light", &skyLightEnabled,
                "Compute sky light from open columns. Sky light intensity follows time-of-day sky color.");
            if (was != skyLightEnabled) InvalidateLighting();
        }
        y += 22;
        {
            bool was = blockLightEnabled;
            ToggleBoolT(x, y, "Block Light", &blockLightEnabled,
                "Compute light from placed sources (torches). Colored BFS flood fill through open cells.");
            if (was != blockLightEnabled) InvalidateLighting();
        }
        y += 22;

        // Ambient minimum
        DrawTextShadow("Ambient Minimum:", x, y, 14, GRAY);
        y += 18;
        DraggableIntT(x, y, "Red", &lightAmbientR, 1.0f, 0, 255,
            "Minimum red component. Prevents completely black tiles. Higher = brighter in total darkness.");
        y += 22;
        DraggableIntT(x, y, "Green", &lightAmbientG, 1.0f, 0, 255,
            "Minimum green component. Prevents completely black tiles.");
        y += 22;
        DraggableIntT(x, y, "Blue", &lightAmbientB, 1.0f, 0, 255,
            "Minimum blue component. Slightly higher default gives a cool moonlight feel in darkness.");
        y += 22;

        // Torch defaults (for sandbox placement)
        DrawTextShadow("Torch Defaults:", x, y, 14, GRAY);
        y += 18;
        DraggableIntT(x, y, "Intensity", &lightDefaultIntensity, 1.0f, 1, 15,
            TextFormat("Propagation radius for new torches. At %d, light reaches %d tiles away.", lightDefaultIntensity, lightDefaultIntensity));
        y += 22;
        DraggableIntT(x, y, "R", &lightDefaultR, 1.0f, 0, 255, "Red component of new torches.");
        y += 22;
        DraggableIntT(x, y, "G", &lightDefaultG, 1.0f, 0, 255, "Green component of new torches.");
        y += 22;
        DraggableIntT(x, y, "B", &lightDefaultB, 1.0f, 0, 255, "Blue component of new torches.");
        y += 22;

        // Color preview
        {
            Color preview = { (uint8_t)lightDefaultR, (uint8_t)lightDefaultG, (uint8_t)lightDefaultB, 255 };
            DrawRectangle((int)x, (int)y, 60, 14, preview);
            DrawRectangleLinesEx((Rectangle){x, y, 60, 14}, 1, GRAY);
            DrawTextShadow("Preview", x + 65, y, 14, GRAY);
        }
        y += 18;

        if (PushButton(x, y, "Clear Lights")) {
            ClearLightSources();
            InvalidateLighting();
        }
    }
    y += 22;

    // === TIME ===
    y += 8;
    if (SectionHeader(x, y, "Time", &sectionTime)) {
        y += 18;

        // Display current time
        int hours = (int)timeOfDay;
        int minutes = (int)((timeOfDay - hours) * 60);
        DrawTextShadow(TextFormat("Day %d, %02d:%02d", dayNumber, hours, minutes), x, y, 14, WHITE);
        y += 18;
        DrawTextShadow(TextFormat("Game time: %.1fs", (float)gameTime), x, y, 14, GRAY);
        y += 22;

        // Game speed control
        DraggableFloatT(x, y, "Game Speed", &gameSpeed, 0.1f, 0.0f, 100.0f,
            TextFormat("Simulation speed multiplier. At %.1fx, 1 real-second = %.1f game-seconds.%s",
                       gameSpeed, gameSpeed, gameSpeed == 0.0f ? " (PAUSED)" : ""));
        y += 22;

        // Speed presets
        if (PushButton(x, y, "Pause")) gameSpeed = 0.0f;
        y += 22;

        // Inline speed buttons: 1x 2x 3x
        {
            bool clicked = false;
            float bx = x;
            bx += PushButtonInline(bx, y, "1x", &clicked); if (clicked) gameSpeed = 1.0f;
            clicked = false;
            bx += PushButtonInline(bx, y, "2x", &clicked); if (clicked) gameSpeed = 2.0f;
            clicked = false;
            PushButtonInline(bx, y, "3x", &clicked); if (clicked) gameSpeed = 3.0f;
        }
        y += 22;

        // Inline speed buttons: 5x 10x 50x
        {
            bool clicked = false;
            float bx = x;
            bx += PushButtonInline(bx, y, "5x", &clicked); if (clicked) gameSpeed = 5.0f;
            clicked = false;
            bx += PushButtonInline(bx, y, "10x", &clicked); if (clicked) gameSpeed = 10.0f;
            clicked = false;
            PushButtonInline(bx, y, "50x", &clicked); if (clicked) gameSpeed = 50.0f;
        }
        y += 22;

        // Day length
        float realDuration = gameSpeed > 0 ? dayLength / gameSpeed : 0;
        DraggableFloatT(x, y, "Day Length", &dayLength, 10.0f, 10.0f, 3600.0f,
            TextFormat("Game-seconds per full day.\nAt 1x speed: %.1f real-%s per day.\nAt current %.1fx: %.1f real-%s per day.",
                       dayLength < 60 ? dayLength : dayLength / 60.0f, dayLength < 60 ? "seconds" : "minutes",
                       gameSpeed, realDuration < 60 ? realDuration : realDuration / 60.0f, realDuration < 60 ? "seconds" : "minutes"));
        y += 22;

        // Day length presets
        DrawTextShadow("Presets:", x, y, 14, GRAY);
        y += 18;
        if (PushButton(x, y, "Fast (24s)")) dayLength = 24.0f;
        y += 22;
        if (PushButton(x, y, "Normal (60s)")) dayLength = 60.0f;
        y += 22;
        if (PushButton(x, y, "Slow (12m)")) dayLength = 720.0f;
        y += 22;
        
        // Timestep mode
        ToggleBool(x, y, "Fixed Timestep", &useFixedTimestep);
    }
    y += 22;

    // === DEBUG VISUALIZATION ===
    y += 8;
    if (SectionHeader(x, y, "Debug", &sectionDebug)) {
        y += 18;

        // Rendering
        if (SectionHeader(x + 10, y, "Rendering", &sectionDebugRendering)) {
            y += 18;
            ToggleBool(x + 10, y, "Show Movers", &showMovers);
            y += 22;
            ToggleBool(x + 10, y, "Pixel Perfect", &usePixelPerfectMovers);
            y += 22;
            ToggleBool(x + 10, y, "Show Items", &showItems);
            y += 22;
            ToggleBool(x + 10, y, "Cull Drawing", &cullDrawing);
        }
        y += 22;

        // Pathfinding
        if (SectionHeader(x + 10, y, "Pathfinding", &sectionDebugPathfinding)) {
            y += 18;
            ToggleBool(x + 10, y, "Show Graph", &showGraph);
            y += 22;
            ToggleBool(x + 10, y, "Show Entrances", &showEntrances);
            y += 22;
            ToggleBool(x + 10, y, "Show Chunks", &showChunkBoundaries);
            y += 22;
            ToggleBool(x + 10, y, "Show Paths", &showMoverPaths);
            y += 22;
            ToggleBool(x + 10, y, "Show Job Lines", &showJobLines);
        }
        y += 22;

        // Mover Diagnostics
        if (SectionHeader(x + 10, y, "Mover Diagnostics", &sectionDebugMovers)) {
            y += 18;
            ToggleBool(x + 10, y, "Show Neighbors", &showNeighborCounts);
            y += 22;
            ToggleBool(x + 10, y, "Show Open Area", &showOpenArea);
            y += 22;
            ToggleBool(x + 10, y, "Show Knots", &showKnotDetection);
            y += 22;
            ToggleBool(x + 10, y, "Show Stuck", &showStuckDetection);
        }
        y += 22;

        // Overlays
        if (SectionHeader(x + 10, y, "Overlays", &sectionDebugOverlays)) {
            y += 18;
            ToggleBool(x + 10, y, "Sim Sources", &showSimSources);
            y += 22;
            ToggleBool(x + 10, y, "Temperature", &showTemperatureOverlay);
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
