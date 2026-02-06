// main.c - Entry point and game loop
//
// Contains global state definitions, helper functions, and the main game loop.
// Included by unity.c - do not compile separately.

#include "game_state.h"
#include "world/cell_defs.h"
#include "world/designations.h"
#include "core/input_mode.h"
#include "entities/workshops.h"
#include "assets/fonts/comic_embedded.h"
#include "sound/sound_phrase.h"
#include "sound/sound_synth_bridge.h"

#ifdef USE_SOUNDSYSTEM
#include "soundsystem/soundsystem.h"
#endif

// ============================================================================
// Global State Definitions
// ============================================================================

// World seed for reproducible terrain generation
uint64_t worldSeed = 0;

float zoom = 1.0f;
Vector2 offset = {0, 0};
Texture2D atlas;
int currentViewZ = 1;  // Default to z=1 for DF-style (walking level above ground)

bool showGraph = false;
bool showEntrances = false;
bool showChunkBoundaries = false;
bool showMovers = true;
bool usePixelPerfectMovers = true;
bool showMoverPaths = false;
bool showJobLines = false;
bool showNeighborCounts = false;
bool showOpenArea = false;
bool showKnotDetection = false;
bool showStuckDetection = false;
bool cullDrawing = true;
bool showItems = true;
bool showSimSources = false;
bool showHelpPanel = false;
bool paused = false;
int followMoverIdx = -1;

// Sound debug (phrase/songs experiments)
static bool soundDebugEnabled = false;
static bool soundDebugAuto = true;
static float soundDebugTimer = 0.0f;
static float soundDebugInterval = 2.5f;
static uint32_t soundDebugSeed = 1;
static SoundSynth* soundDebugSynth = NULL;
static const char* soundPalettePath = "assets/sound/phrase_palette.cfg";
static bool soundDebugResponsePending = false;
static float soundDebugResponseTimer = 0.0f;
static SoundPhrase soundDebugResponse;

int pathAlgorithm = 1;
const char* algorithmNames[] = {"A*", "HPA*", "JPS", "JPS+"};
int currentDirection = 1;
const char* directionNames[] = {"4-dir", "8-dir"};

int currentTool = 0;
const char* toolNames[] = {"Draw Wall", "Draw Floor", "Draw Ladder", "Erase", "Set Start", "Set Goal"};

int currentTerrain = 0;
// NOTE: When adding new terrains, also update the count in ui_panels.c CycleOption() for "Terrain"
const char* terrainNames[] = {"Clear", "Sparse", "City", "Mixed", "Perlin", "Maze", "Dungeon", "Caves", "Drunkard", "Tunneler", "MixMax", "Towers3D", "GalleryFlat", "Castle", "Labyrinth3D", "Spiral3D", "Council", "Hills", "HillsSoils", "HillsSoilsWater", "CraftTest"};

TreeType currentTreeType = TREE_TYPE_OAK;
const char* treeTypeNames[] = {"None", "Oak", "Pine", "Birch", "Willow"};

// Ramp placement controls (used by hills generators)
float rampNoiseScale = 0.04f;  // Larger = bigger ramp clusters
float rampDensity = 0.6f;      // 0..1, probability-like density

// HillsSoilsWater controls
int hillsWaterRiverCount = 2;
int hillsWaterRiverWidth = 2;     // radius-ish (1-4)
int hillsWaterLakeCount = 2;
int hillsWaterLakeRadius = 6;     // 3-12
float hillsWaterWetnessBias = 0.25f; // additive near-water wetness boost
bool hillsWaterConnectivityReport = true;
bool hillsWaterConnectivityFixSmall = false;
int hillsWaterConnectivitySmallThreshold = 30;

bool sectionView = false;
bool sectionPathfinding = false;
bool sectionMapEditing = false;
bool sectionAgents = false;
bool sectionMovers = false;
bool sectionMoverAvoidance = false;
bool sectionWater = false;
bool sectionFire = false;
bool sectionSmoke = false;
bool sectionSteam = false;
bool sectionTemperature = false;
bool showTemperatureOverlay = false;
bool sectionEntropy = false;
bool sectionTrees = false;
bool sectionMoverWalls = false;
bool sectionMoverDebug = false;
bool sectionProfiler = false;
bool sectionMemory = false;
bool sectionMemGrid = false;
bool sectionMemPath = false;
bool sectionMemEntities = false;
bool sectionMemSpatial = false;
bool sectionJobs = false;
bool sectionTime = false;

int hoveredStockpile = -1;
int hoveredWorkshop = -1;
int hoveredMover = -1;
int hoveredItemCell[16];
int hoveredItemCount = 0;
int hoveredDesignationX = -1;
int hoveredDesignationY = -1;
int hoveredDesignationZ = -1;

int agentCountSetting = 10;
Agent agents[MAX_AGENTS];
int agentCount = 0;

int moverCountSetting = 10;
int itemCountSetting = 10;
MoverRenderData moverRenderData[MAX_MOVERS];

// ============================================================================
// Helper Functions
// ============================================================================

static void SoundDebugEnsure(void) {
    if (!soundDebugSynth) {
        soundDebugSynth = SoundSynthCreate();
    }
    if (soundDebugSynth && !SoundSynthInitAudio(soundDebugSynth, 44100, 512)) {
        AddMessage("Sound debug: failed to init audio", RED);
        return;
    }
    SoundPaletteLoadDefault(soundPalettePath);
}

static void SoundDebugToggle(void) {
    soundDebugEnabled = !soundDebugEnabled;
    if (soundDebugEnabled) {
        SoundDebugEnsure();
        AddMessage("Sound debug: on (KP2 call, KP3 song, KP5 bird, KP6 vowel, KP7-9 call/response)", GREEN);
    } else {
        if (soundDebugSynth) {
            SoundSynthShutdownAudio(soundDebugSynth);
        }
        AddMessage("Sound debug: off", GRAY);
    }
}

static void SoundDebugPlayCall(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundPhrase phrase = SoundMakeCall(soundDebugSeed++);
    SoundSynthPlayPhrase(soundDebugSynth, &phrase);
}

static void SoundDebugPlayBirdOnly(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundPhrase phrase = SoundMakeCallBirdOnly(soundDebugSeed++);
    SoundSynthPlayPhrase(soundDebugSynth, &phrase);
}

static void SoundDebugPlayVowelOnly(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundPhrase phrase = SoundMakeCallVowelOnly(soundDebugSeed++);
    SoundSynthPlayPhrase(soundDebugSynth, &phrase);
}

static void SoundDebugPlaySong(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundSong song = SoundMakeSong(soundDebugSeed++);
    SoundSynthPlaySong(soundDebugSynth, &song);
}

static void SoundDebugScheduleResponse(const SoundPhrase* call, SoundPhrase response) {
    soundDebugResponse = response;
    soundDebugResponsePending = (response.count > 0);
    soundDebugResponseTimer = call ? (call->totalDuration + 0.15f) : 0.3f;
}

static void SoundDebugCallResponseVariation(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundPhrase call = SoundMakeCall(soundDebugSeed++);
    SoundPhrase response = SoundMakeResponseVariation(&call, soundDebugSeed++);
    SoundSynthPlayPhrase(soundDebugSynth, &call);
    SoundDebugScheduleResponse(&call, response);
}

static void SoundDebugCallResponseContrast(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundPhrase call = SoundMakeCall(soundDebugSeed++);
    SoundPhrase response = SoundMakeResponseContrast(&call, soundDebugSeed++);
    SoundSynthPlayPhrase(soundDebugSynth, &call);
    SoundDebugScheduleResponse(&call, response);
}

static void SoundDebugCallResponseMirror(void) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundPhrase call = SoundMakeCall(soundDebugSeed++);
    SoundPhrase response = SoundMakeResponseMirror(&call, soundDebugSeed++);
    SoundSynthPlayPhrase(soundDebugSynth, &call);
    SoundDebugScheduleResponse(&call, response);
}

static void SoundDebugUpdate(float dt) {
    if (!soundDebugEnabled || !soundDebugSynth) return;
    SoundSynthUpdate(soundDebugSynth, dt);
    if (!soundDebugAuto) return;
    soundDebugTimer += dt;
    if (soundDebugTimer >= soundDebugInterval) {
        SoundDebugPlayCall();
        soundDebugTimer = 0.0f;
    }
}

static void SoundDebugUpdateResponse(float dt) {
    if (!soundDebugResponsePending || !soundDebugSynth) return;
    soundDebugResponseTimer -= dt;
    if (soundDebugResponseTimer <= 0.0f) {
        SoundSynthPlayPhrase(soundDebugSynth, &soundDebugResponse);
        soundDebugResponsePending = false;
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
        case 17: GenerateHills(); break;
        case 18: GenerateHillsSoils(); break;
        case 19: GenerateHillsSoilsWater(); break;
        case 20: GenerateCraftingTest(); break;
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
void DrawGrassOverlay(void);
void DrawWater(void);
void DrawFire(void);
void DrawSmoke(void);
void DrawSteam(void);
void DrawTemperature(void);
void DrawSimSources(void);
void DrawChunkBoundaries(void);
void DrawEntrances(void);
void DrawGraph(void);
void DrawPath(void);
void DrawAgents(void);
void DrawMovers(void);
void DrawJobLines(void);
void DrawItems(void);
void DrawGatherZones(void);
void DrawStockpileTiles(void);
void DrawStockpileItems(void);
void DrawWorkshops(void);
void DrawHaulDestinations(void);
void DrawMiningDesignations(void);
void DrawBlueprints(void);

// From render/tooltips.c
void DrawStockpileTooltip(int spIdx, Vector2 mouse, Vector2 mouseGrid);
void DrawWorkshopTooltip(int wsIdx, Vector2 mouse);
void DrawMoverTooltip(int moverIdx, Vector2 mouse);
void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY);
void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);
void DrawCellTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);
void DrawBlueprintTooltip(int bpIdx, Vector2 mouse);
void DrawMiningTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);
void DrawDesignationTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);

// From render/ui_panels.c
void DrawUI(void);
void DrawTimeOfDayWidget(float x, float y);
#if PROFILER_ENABLED
void DrawProfilerPanel(float rightEdge, float y);
#endif

// From core/input.c
void HandleInput(void);

// From core/saveload.c
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// ============================================================================
// Headless Mode - Run simulation without GUI
// ============================================================================

static int RunHeadless(const char* loadFile, int ticks, int argc, char** argv) {
    // Initialize minimal state (no window)
    use8Dir = true;
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    gridDepth = 16;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_DIRT;
        }
    for (int z = 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[z][y][x] = CELL_AIR;
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitDesignations();
    InitSimActivity();
    InitTemperature();
    InitSteam();
    
    // Handle .gz decompression
    const char* actualFile = loadFile;
    char tempFile[512] = {0};
    size_t len = strlen(loadFile);
    if (len > 3 && strcmp(loadFile + len - 3, ".gz") == 0) {
        const char* base = strrchr(loadFile, '/');
        base = base ? base + 1 : loadFile;
        snprintf(tempFile, sizeof(tempFile), "/tmp/%.*s", (int)(strlen(base) - 3), base);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "gunzip -c '%s' > '%s'", loadFile, tempFile);
        if (system(cmd) == 0) {
            actualFile = tempFile;
            printf("(decompressed to %s)\n", tempFile);
        } else {
            printf("Failed to decompress: %s\n", loadFile);
            return 1;
        }
    }
    
    if (!LoadWorld(actualFile)) {
        printf("Failed to load: %s\n", loadFile);
        return 1;
    }
    printf("Loaded: %s\n", loadFile);
    printf("Running %d ticks headless...\n", ticks);
    
    // Build pathfinding structures
    BuildEntrances();
    BuildGraph();
    
    // Snapshot state before
    int stuckBefore = 0;
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            int mx = (int)(movers[i].x / CELL_SIZE);
            int my = (int)(movers[i].y / CELL_SIZE);
            int mz = (int)movers[i].z;
            if (!IsCellWalkableAt(mz, my, mx)) stuckBefore++;
        }
    }
    
    // Run simulation
    clock_t startClock = clock();
    for (int t = 0; t < ticks; t++) {
        double tickStart = GetTime();
        TickWithDt(TICK_DT);
        ItemsTick(TICK_DT);
        DesignationsTick(TICK_DT);
        AssignJobs();
        JobsTick();
        PROFILE_FRAME_END();  // Store profiler data for this tick
        double tickTime = (GetTime() - tickStart) * 1000.0;
        if (tickTime > 100.0) {
            TraceLog(LOG_WARNING, "SLOW TICK %d: %.1fms", t, tickTime);
        }
    }
    double elapsed = (double)(clock() - startClock) / CLOCKS_PER_SEC;
    
    // Snapshot state after
    int stuckAfter = 0;
    int moversInNonWalkable = 0;
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            int mx = (int)(movers[i].x / CELL_SIZE);
            int my = (int)(movers[i].y / CELL_SIZE);
            int mz = (int)movers[i].z;
            if (!IsCellWalkableAt(mz, my, mx)) {
                moversInNonWalkable++;
            }
            if (movers[i].timeWithoutProgress > 2.0f) {
                stuckAfter++;
            }
        }
    }
    
    printf("\n=== HEADLESS RESULTS ===\n");
    printf("Ticks: %d (%.2f ms, %.2f ms/tick)\n", ticks, elapsed * 1000.0, (elapsed * 1000.0) / ticks);
    printf("Movers: %d\n", moverCount);
    printf("Stuck in non-walkable: %d before -> %d after\n", stuckBefore, moversInNonWalkable);
    printf("Stuck (no progress): %d\n", stuckAfter);
    extern int repathFallbackCount, repathHpaSuccessCount;
    printf("Repath stats: HPA success=%d, A* fallback=%d (%.1f%% fallback)\n", 
           repathHpaSuccessCount, repathFallbackCount,
           (repathFallbackCount + repathHpaSuccessCount) > 0 
               ? 100.0 * repathFallbackCount / (repathFallbackCount + repathHpaSuccessCount) 
               : 0.0);
    
    // Simulation stats (last tick's update counts)
    extern int waterUpdateCount, fireUpdateCount, steamUpdateCount, smokeUpdateCount, tempUpdateCount;
    printf("\n=== SIMULATION STATS (last tick) ===\n");
    printf("Update counts: Water=%d, Fire=%d, Steam=%d, Smoke=%d, Temp=%d\n",
           waterUpdateCount, fireUpdateCount, steamUpdateCount, smokeUpdateCount, tempUpdateCount);
    
    // Simulation timing from profiler
    extern ProfileSection profilerSections[];
    extern int profilerSectionCount;
    printf("Timing (avg ms): ");
    const char* simNames[] = {"Water", "Fire", "Smoke", "Steam", "Temperature"};
    for (int s = 0; s < 5; s++) {
        for (int i = 0; i < profilerSectionCount; i++) {
            if (strcmp(profilerSections[i].name, simNames[s]) == 0) {
                printf("%s=%.2f ", simNames[s], ProfileGetAvg(i));
                break;
            }
        }
    }
    printf("\n");
    
    // Active cell counts
    extern WaterCell waterGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
    extern FireCell fireGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
    extern SteamCell steamGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
    int waterCells = 0, fireCells = 0, steamCells = 0, smokeCells = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (waterGrid[z][y][x].level > 0) waterCells++;
                if (fireGrid[z][y][x].level > 0) fireCells++;
                if (steamGrid[z][y][x].level > 0) steamCells++;
                if (smokeGrid[z][y][x].level > 0) smokeCells++;
            }
        }
    }
    printf("Active cells: Water=%d, Fire=%d, Steam=%d, Smoke=%d\n", 
           waterCells, fireCells, steamCells, smokeCells);
    
    // Smoke per z-level with cell locations
    extern SmokeCell smokeGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
    int smokeTotalLevel = 0;
    int smokeMaxZ = -1;
    int highestSmokeX = -1, highestSmokeY = -1;
    printf("Smoke per z-level: ");
    for (int z = 0; z < gridDepth; z++) {
        int levelSum = 0;
        int cellCount = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (smokeGrid[z][y][x].level > 0) {
                    levelSum += smokeGrid[z][y][x].level;
                    cellCount++;
                    if (z > smokeMaxZ) {
                        highestSmokeX = x;
                        highestSmokeY = y;
                    }
                }
            }
        }
        smokeTotalLevel += levelSum;
        if (levelSum > 0) {
            printf("z%d=%d(%dc) ", z, levelSum, cellCount);
            smokeMaxZ = z;
        }
    }
    printf("(total=%d, maxZ=%d at %d,%d)\n", smokeTotalLevel, smokeMaxZ, highestSmokeX, highestSmokeY);
    

    
    // Check if --mover flag is present for quick mover summary
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mover") == 0 && i + 1 < argc) {
            const char* arg = argv[i + 1];
            printf("\n=== MOVER STATE AFTER HEADLESS ===\n");
            if (strcmp(arg, "all") == 0) {
                for (int m = 0; m < moverCount; m++) {
                    Mover* mv = &movers[m];
                    if (!mv->active) continue;
                    int mx = (int)(mv->x / CELL_SIZE);
                    int my = (int)(mv->y / CELL_SIZE);
                    int mz = (int)mv->z;
                    bool walkable = IsCellWalkableAt(mz, my, mx);
                    printf("Mover %d: cell (%d,%d,z%d) %s goal=(%d,%d,z%d) path=%d\n",
                           m, mx, my, mz, walkable ? "OK" : "STUCK!",
                           mv->goal.x, mv->goal.y, mv->goal.z, mv->pathLength);
                }
            } else {
                int idx = atoi(arg);
                if (idx >= 0 && idx < moverCount) {
                    Mover* mv = &movers[idx];
                    int mx = (int)(mv->x / CELL_SIZE);
                    int my = (int)(mv->y / CELL_SIZE);
                    int mz = (int)mv->z;
                    printf("Mover %d:\n", idx);
                    printf("  Position: (%.2f, %.2f, z%d) -> cell (%d, %d)\n", mv->x, mv->y, mz, mx, my);
                    printf("  Walkable: %s\n", IsCellWalkableAt(mz, my, mx) ? "YES" : "NO");
                    printf("  Goal: (%d, %d, z%d)\n", mv->goal.x, mv->goal.y, mv->goal.z);
                    printf("  Path length: %d, index: %d\n", mv->pathLength, mv->pathIndex);
                    printf("  Time without progress: %.2f\n", mv->timeWithoutProgress);
                }
            }
            break;
        }
    }
    
    // Check if --save specified
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) {
            const char* outFile = argv[i + 1];
            if (SaveWorld(outFile)) {
                printf("Saved to: %s\n", outFile);
            } else {
                printf("Failed to save to: %s\n", outFile);
            }
            break;
        }
    }
    
    return 0;
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    // Check for --inspect mode (runs without GUI)
    if (argc >= 2 && strcmp(argv[1], "--inspect") == 0) {
        return InspectSaveFile(argc, argv);
    }

    // Check for --headless mode
    const char* headlessFile = NULL;
    int headlessTicks = 100;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            // Find the load file and tick count
            for (int j = 1; j < argc; j++) {
                if (strcmp(argv[j], "--load") == 0 && j + 1 < argc) {
                    headlessFile = argv[j + 1];
                }
                if (strcmp(argv[j], "--ticks") == 0 && j + 1 < argc) {
                    headlessTicks = atoi(argv[j + 1]);
                }
            }
            if (!headlessFile) {
                printf("Usage: %s --headless --load <savefile> [--ticks N] [--save <outfile>] [--mover N] [--cell X,Y,Z] ...\n", argv[0]);
                return 1;
            }
            return RunHeadless(headlessFile, headlessTicks, argc, argv);
        }
    }

    // Check for --load option
    const char* loadFile = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--load") == 0) {
            if (i + 1 < argc) {
                loadFile = argv[i + 1];
            } else {
                printf("Warning: --load requires a filename\n");
                printf("Usage: %s --load <savefile>\n", argv[0]);
            }
            break;
        }
    }

    // Check for --seed option (for reproducible terrain generation)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0) {
            if (i + 1 < argc) {
                worldSeed = (uint64_t)strtoull(argv[i + 1], NULL, 10);
            } else {
                printf("Warning: --seed requires a number\n");
                printf("Usage: %s --seed <number>\n", argv[0]);
            }
            break;
        }
    }

    // Generate seed from time if not specified
    if (worldSeed == 0) {
        worldSeed = (uint64_t)time(NULL);
    }
    printf("World seed: %llu\n", (unsigned long long)worldSeed);

    int screenWidth = 1280, screenHeight = 800;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "HPA* Pathfinding");
    SetExitKey(0);  // Disable ESC closing the window - we use ESC for input navigation
    atlas = AtlasLoadEmbedded();
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    Font comicFont = LoadEmbeddedFont();
    ui_init(&comicFont);
    SetTargetFPS(60);
    use8Dir = true;
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    gridDepth = 16;
    // z=0: dirt (solid ground) with grass overlay, z=1+: air (DF-style)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_DIRT;
            SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);
        }
    for (int z = 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[z][y][x] = CELL_AIR;
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitDesignations();
    InitSimActivity();
    InitTemperature();
    InitSteam();
    BuildEntrances();
    BuildGraph();
    offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;

    // Load save file if specified via --load
    if (loadFile) {
        const char* actualFile = loadFile;
        char tempFile[512] = {0};

        // Handle .gz files by decompressing to /tmp
        size_t len = strlen(loadFile);
        if (len > 3 && strcmp(loadFile + len - 3, ".gz") == 0) {
            // Extract basename without .gz
            const char* base = strrchr(loadFile, '/');
            base = base ? base + 1 : loadFile;
            snprintf(tempFile, sizeof(tempFile), "/tmp/%.*s", (int)(strlen(base) - 3), base);

            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "gunzip -c '%s' > '%s'", loadFile, tempFile);
            if (system(cmd) == 0) {
                actualFile = tempFile;
                printf("Decompressed to: %s\n", tempFile);
            } else {
                printf("Failed to decompress: %s\n", loadFile);
            }
        }

        if (LoadWorld(actualFile)) {
            printf("Loaded: %s\n", loadFile);
            fflush(stdout);
        } else {
            printf("Failed to load: %s\n", loadFile);
            fflush(stdout);
        }
    }

    float accumulator = 0.0f;

    while (!WindowShouldClose() && !shouldQuit) {
        float frameTime = GetFrameTime();
        accumulator += frameTime;

        ui_update();

        // Register UI block areas BEFORE HandleInput so clicks are blocked immediately
        ui_clear_block_rects();
        {
            // Bottom bar area
            int barH = 28;
            int barY = GetScreenHeight() - barH - 6;
            int barX = 150;
            ui_add_block_rect((Rectangle){barX, barY, GetScreenWidth() - barX, barH});
        }

        HandleInput();

        // Sound debug toggles (independent from input modes)
        if (IsKeyPressed(KEY_KP_1)) {
            SoundDebugToggle();
        }
        if (IsKeyPressed(KEY_KP_4)) {
            soundDebugAuto = !soundDebugAuto;
            AddMessage(TextFormat("Sound debug auto: %s", soundDebugAuto ? "ON" : "OFF"),
                       soundDebugAuto ? GREEN : GRAY);
        }
        if (IsKeyPressed(KEY_KP_2)) {
            SoundDebugPlayCall();
        }
        if (IsKeyPressed(KEY_KP_3)) {
            SoundDebugPlaySong();
        }
        if (IsKeyPressed(KEY_KP_5)) {
            SoundDebugPlayBirdOnly();
        }
        if (IsKeyPressed(KEY_KP_6)) {
            SoundDebugPlayVowelOnly();
        }
        if (IsKeyPressed(KEY_KP_7)) {
            soundDebugAuto = false;
            SoundDebugCallResponseVariation();
        }
        if (IsKeyPressed(KEY_KP_8)) {
            soundDebugAuto = false;
            SoundDebugCallResponseContrast();
        }
        if (IsKeyPressed(KEY_KP_9)) {
            soundDebugAuto = false;
            SoundDebugCallResponseMirror();
        }

        // Handle drag-and-drop of save files
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0) {
                const char* path = droppedFiles.paths[0];
                const char* actualFile = path;
                char tempFile[512] = {0};

                // Handle .gz files by decompressing to /tmp
                size_t len = strlen(path);
                if (len > 3 && strcmp(path + len - 3, ".gz") == 0) {
                    const char* base = strrchr(path, '/');
                    base = base ? base + 1 : path;
                    snprintf(tempFile, sizeof(tempFile), "/tmp/%.*s", (int)(strlen(base) - 3), base);

                    char cmd[1024];
                    snprintf(cmd, sizeof(cmd), "gunzip -c '%s' > '%s'", path, tempFile);
                    if (system(cmd) == 0) {
                        actualFile = tempFile;
                    } else {
                        AddMessage("Failed to decompress file", RED);
                        actualFile = NULL;
                    }
                }

                if (actualFile && LoadWorld(actualFile)) {
                    AddMessage(TextFormat("Loaded: %s", path), GREEN);
                    paused = true;
                } else if (actualFile) {
                    AddMessage("Failed to load save file", RED);
                }
            }
            UnloadDroppedFiles(droppedFiles);
        }

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
        SoundDebugUpdate(frameTime);
        SoundDebugUpdateResponse(frameTime);

        if (!paused) {
            bool shouldTick = useFixedTimestep ? (accumulator >= TICK_DT) : true;
            float tickDt = useFixedTimestep ? TICK_DT : frameTime;
            
            if (shouldTick) {
                PROFILE_BEGIN(Tick);
                TickWithDt(tickDt);
                PROFILE_END(Tick);
                PROFILE_BEGIN(ItemsTick);
                ItemsTick(tickDt);
                PROFILE_END(ItemsTick);
                DesignationsTick(tickDt);
                PROFILE_BEGIN(AssignJobs);
                AssignJobs();
                PROFILE_END(AssignJobs);
                PROFILE_BEGIN(JobsTick);
                JobsTick();
                PROFILE_END(JobsTick);
                
                if (useFixedTimestep) {
                    accumulator -= TICK_DT;
                    if (accumulator > TICK_DT) {
                        accumulator = TICK_DT;
                    }
                }
            }
        }

        if (followMoverIdx >= 0) {
            if (followMoverIdx >= moverCount || !movers[followMoverIdx].active) {
                followMoverIdx = -1;
            } else {
                float centerX = GetScreenWidth() * 0.5f;
                float centerY = GetScreenHeight() * 0.5f;
                offset.x = centerX - movers[followMoverIdx].x * zoom;
                offset.y = centerY - movers[followMoverIdx].y * zoom;
                currentViewZ = (int)movers[followMoverIdx].z;
            }
        }

        PROFILE_BEGIN(Render);
        BeginDrawing();
        ClearBackground(BLACK);
        PROFILE_BEGIN(DrawCells);
        DrawCellGrid();
        DrawGrassOverlay();
        DrawWater();
        DrawFrozenWater();
        DrawFire();
        DrawSmoke();
        DrawSteam();
        DrawTemperature();
        DrawSimSources();
        PROFILE_END(DrawCells);
        if (inputAction == ACTION_WORK_GATHER) {
            DrawGatherZones();
        }
        DrawStockpileTiles();
        DrawWorkshops();
        DrawHaulDestinations();
        DrawStockpileItems();
        DrawMiningDesignations();
        DrawBlueprints();
        if (showChunkBoundaries) {
            DrawChunkBoundaries();
        }
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
        DrawJobLines();

        // Draw workshop preview (3x3) when in workshop placement mode
        if ((inputAction == ACTION_DRAW_WORKSHOP_STONECUTTER ||
             inputAction == ACTION_DRAW_WORKSHOP_SAWMILL ||
             inputAction == ACTION_DRAW_WORKSHOP_KILN) && !isDragging) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            float size = CELL_SIZE * zoom;
            float px = offset.x + x * size;
            float py = offset.y + y * size;
            float pw = 3 * size;
            float ph = 3 * size;
            
            // Check if placement is valid
            bool valid = true;
            for (int dy = 0; dy < 3 && valid; dy++) {
                for (int dx = 0; dx < 3 && valid; dx++) {
                    int cx = x + dx;
                    int cy = y + dy;
                    if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) valid = false;
                    else if (!IsCellWalkableAt(currentViewZ, cy, cx)) valid = false;
                    else if (FindWorkshopAt(cx, cy, currentViewZ) >= 0) valid = false;
                }
            }
            
            Color fillColor = valid ? (Color){180, 140, 80, 80} : (Color){200, 50, 50, 80};
            Color lineColor = valid ? (Color){200, 160, 100, 255} : RED;
            DrawRectangle((int)px, (int)py, (int)pw, (int)ph, fillColor);
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, lineColor);
        }
        
        // Draw drag preview rectangle when dragging
        if (isDragging && inputAction != ACTION_NONE) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            int x1 = dragStartX < x ? dragStartX : x;
            int y1 = dragStartY < y ? dragStartY : y;
            int x2 = dragStartX > x ? dragStartX : x;
            int y2 = dragStartY > y ? dragStartY : y;
            float size = CELL_SIZE * zoom;
            float px = offset.x + x1 * size;
            float py = offset.y + y1 * size;
            float pw = (x2 - x1 + 1) * size;
            float ph = (y2 - y1 + 1) * size;

            // Choose colors based on action and mouse button
            Color fillColor = {100, 200, 100, 80};
            Color lineColor = GREEN;
            bool isRightDrag = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

            if (isRightDrag) {
                fillColor = (Color){200, 0, 0, 80};
                lineColor = RED;
            } else {
                switch (inputAction) {
                    // Draw actions
                    case ACTION_DRAW_WALL:
                    case ACTION_DRAW_FLOOR:
                    case ACTION_DRAW_LADDER:
                        fillColor = (Color){100, 200, 100, 80};
                        lineColor = GREEN;
                        break;
                    case ACTION_DRAW_STOCKPILE:
                        fillColor = (Color){0, 200, 0, 80};
                        lineColor = GREEN;
                        break;
                    // Work actions
                    case ACTION_WORK_MINE:
                        fillColor = (Color){255, 150, 0, 80};
                        lineColor = ORANGE;
                        break;
                    case ACTION_WORK_CONSTRUCT:
                        fillColor = (Color){0, 200, 200, 80};
                        lineColor = (Color){0, 255, 255, 255};
                        break;
                    case ACTION_WORK_GATHER:
                        fillColor = (Color){255, 180, 50, 80};
                        lineColor = ORANGE;
                        break;
                    // Sandbox actions
                    case ACTION_SANDBOX_WATER:
                        fillColor = (Color){0, 100, 200, 80};
                        lineColor = SKYBLUE;
                        break;
                    case ACTION_SANDBOX_FIRE:
                        fillColor = (Color){200, 50, 0, 80};
                        lineColor = RED;
                        break;
                    case ACTION_SANDBOX_HEAT:
                        fillColor = (Color){200, 100, 0, 80};
                        lineColor = ORANGE;
                        break;
                    default:
                        break;
                }
            }

            DrawRectangle((int)px, (int)py, (int)pw, (int)ph, fillColor);
            DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, lineColor);
        }

        // Stats and HUD
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 5, 5, 18, LIME);
        if (paused) {
            int fpsW = MeasureTextUI(TextFormat("FPS: %d", GetFPS()), 18);
            DrawTextShadow("*paused*", 5 + fpsW + 10, 5, 18, YELLOW);
        }
        // Z level display with clickable < > buttons
        {
            int zY = GetScreenHeight() - 28 - 6 + 4;
            const char* zText = TextFormat("Z: %d/%d", currentViewZ, gridDepth - 1);
            int zTextW = MeasureTextUI(zText, 18);
            DrawTextShadow(zText, 5, zY, 18, SKYBLUE);

            int btnX = 5 + zTextW + 8;
            int btnW = 24;
            int btnH = 24;
            int fontSize = 22;
            int textOffsetY = -2;  // Adjust text position within button

            // < button (decrease Z)
            Rectangle btnDown = {btnX, zY + textOffsetY, btnW, btnH};
            bool downHovered = CheckCollisionPointRec(GetMousePosition(), btnDown);
            Color downColor = (currentViewZ > 0) ? (downHovered ? WHITE : SKYBLUE) : DARKGRAY;
            DrawTextShadow("<", btnX, zY, fontSize, downColor);
            if (downHovered && currentViewZ > 0) {
                ui_set_hovered();
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentViewZ--;
                }
            }

            // > button (increase Z)
            Rectangle btnUp = {btnX + btnW + 4, zY + textOffsetY, btnW, btnH};
            bool upHovered = CheckCollisionPointRec(GetMousePosition(), btnUp);
            Color upColor = (currentViewZ < gridDepth - 1) ? (upHovered ? WHITE : SKYBLUE) : DARKGRAY;
            DrawTextShadow(">", btnX + btnW + 4, zY, fontSize, upColor);
            if (upHovered && currentViewZ < gridDepth - 1) {
                ui_set_hovered();
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentViewZ++;
                }
            }
        }

        // Quit confirmation popup
        if (showQuitConfirm) {
            const char* msg = "Press ESC again to quit";
            int msgW = MeasureTextUI(msg, 20);
            int popW = msgW + 40;
            int popH = 40;
            int popX = (GetScreenWidth() - popW) / 2;
            int popY = (GetScreenHeight() - popH) / 2;
            DrawRectangle(popX, popY, popW, popH, (Color){40, 40, 40, 240});
            DrawRectangleLinesEx((Rectangle){popX, popY, popW, popH}, 2, WHITE);
            DrawTextShadow(msg, popX + 20, popY + 10, 20, WHITE);
        }

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
                "--- Navigation ---",
                "ESC           Back / Exit menu",
                "< / >         Change Z level",
                "Scroll        Zoom in/out",
                "Middle-drag   Pan view",
                "C             Center view",
                "Space         Pause/Resume",
                "",
                "--- Quick Edit (Normal mode) ---",
                "L-click       Place wall",
                "R-click       Erase",
                "2 + L-click   Place wood wall",
                "",
                "--- General ---",
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

        // Time of day widget (top right)
        {
            int widgetWidth = 140;
            int widgetX = GetScreenWidth() - widgetWidth - 10;
            int widgetY = 10;
            DrawTimeOfDayWidget(widgetX, widgetY);
        }

#if PROFILER_ENABLED
        DrawProfilerPanel(GetScreenWidth() - 50, 45);  // Below time widget
#endif

        PROFILE_BEGIN(DrawUI);
        DrawUI();
        PROFILE_END(DrawUI);

        // Input mode bar at bottom - individual buttons (after DrawUI so ui_begin_frame has run)
        {
            int barH = 28;
            int barY = GetScreenHeight() - barH - 6;
            int barX = 150;  // After Z level display
            int padding = 12;
            int spacing = 12;
            int fontSize = 16;

            BarItem items[MAX_BAR_ITEMS];
            int itemCount = InputMode_GetBarItems(items);

            int x = barX;
            for (int i = 0; i < itemCount; i++) {
                int textW = MeasureTextUI(items[i].text, fontSize);
                int btnW = textW + padding * 2 + 8;  // Extra width for right margin
                int textY = barY + (barH - fontSize) / 2 - 4;  // Shift up more for bottom margin

                bool isClickable = items[i].key != 0 && !items[i].isHint;
                Rectangle btnRect = {x, barY, btnW, barH};
                bool hovered = isClickable && CheckCollisionPointRec(GetMousePosition(), btnRect);

                // Choose colors based on type
                Color bgColor = {30, 30, 30, 220};
                Color textColor = WHITE;

                if (items[i].isHeader) {
                    textColor = YELLOW;
                    if (hovered) bgColor = (Color){60, 60, 40, 220};
                } else if (items[i].isHint) {
                    textColor = GRAY;
                } else if (items[i].isSelected) {
                    bgColor = (Color){60, 60, 80, 220};
                    textColor = WHITE;
                } else if (hovered) {
                    bgColor = (Color){60, 60, 60, 220};
                }

                // Draw button background for clickable items
                if (isClickable) {
                    DrawRectangle(x, barY, btnW, barH, bgColor);
                    DrawRectangleLinesEx(btnRect, 1, hovered ? WHITE : (items[i].isHeader ? (Color){80, 80, 40, 255} : GRAY));
                }

                // Draw text
                DrawTextShadow(items[i].text, x + padding, textY, fontSize, textColor);
                
                // Draw underline under the shortcut character
                if (items[i].underlinePos >= 0 && items[i].underlinePos < (int)strlen(items[i].text)) {
                    // Measure width up to the underlined character
                    char before[32];
                    strncpy(before, items[i].text, items[i].underlinePos);
                    before[items[i].underlinePos] = '\0';
                    int beforeW = MeasureTextUI(before, fontSize);
                    
                    // Measure the underlined character
                    char underlineChar[2] = {items[i].text[items[i].underlinePos], '\0'};
                    int charW = MeasureTextUI(underlineChar, fontSize);
                    
                    // Draw underline
                    int underlineX = x + padding + beforeW;
                    int underlineY = textY + fontSize + 4;
                    DrawRectangle(underlineX, underlineY, charW, 2, textColor);
                }

                // Handle hover and click
                if (hovered) {
                    ui_set_hovered();
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        InputMode_TriggerKey(items[i].key);
                        ui_consume_click();
                    }
                }

                x += btnW + spacing;
            }
            
            // Pile radius control (show when in soil drawing mode)
            if (inputAction >= ACTION_DRAW_SOIL_DIRT && inputAction <= ACTION_DRAW_SOIL_PEAT) {
                x += 40;  // Extra spacing before control
                DraggableFloatT(x, barY + 5, "Pile Radius", &soilPileRadius, 0.5f, 1.0f, 10.0f,
                    "Shift+draw: How far soil spreads when piling (1-10)");
            }
        }

        DrawMessages(GetScreenWidth(), GetScreenHeight());

        if (hoveredStockpile >= 0) {
            DrawStockpileTooltip(hoveredStockpile, GetMousePosition(), ScreenToGrid(GetMousePosition()));
        }

        if (hoveredWorkshop >= 0 && hoveredStockpile < 0) {
            DrawWorkshopTooltip(hoveredWorkshop, GetMousePosition());
        }

        if (hoveredMover >= 0) {
            DrawMoverTooltip(hoveredMover, GetMousePosition());
        }

        if (hoveredItemCount > 0 && hoveredMover < 0 && hoveredStockpile < 0) {
            Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
            DrawItemTooltip(hoveredItemCell, hoveredItemCount, GetMousePosition(), (int)mouseGrid.x, (int)mouseGrid.y);
        }

        if (hoveredStockpile < 0 && hoveredMover < 0 && hoveredItemCount == 0 && hoveredWorkshop < 0) {
            Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
            int cellX = (int)mouseGrid.x;
            int cellY = (int)mouseGrid.y;
            if (cellX >= 0 && cellX < gridWidth && cellY >= 0 && cellY < gridHeight) {
                // Check for blueprint at this cell
                int bpIdx = GetBlueprintAt(cellX, cellY, currentViewZ);
                if (bpIdx >= 0) {
                    DrawBlueprintTooltip(bpIdx, GetMousePosition());
                }
                // Check for any designation at this cell
                else if (GetDesignation(cellX, cellY, currentViewZ) != NULL) {
                    DrawDesignationTooltip(cellX, cellY, currentViewZ, GetMousePosition());
                }
                else if (paused) {
                    // When paused, show comprehensive cell info
                    DrawCellTooltip(cellX, cellY, currentViewZ, GetMousePosition());
                } else if (HasWater(cellX, cellY, currentViewZ) ||
                    waterGrid[currentViewZ][cellY][cellX].isSource ||
                    waterGrid[currentViewZ][cellY][cellX].isDrain) {
                    // When running, only show water tooltip if there's water
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
    if (soundDebugSynth) {
        SoundSynthDestroy(soundDebugSynth);
        soundDebugSynth = NULL;
    }
    CloseWindow();
    return 0;
}
