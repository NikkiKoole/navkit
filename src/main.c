// main.c - Entry point and game loop
//
// Contains global state definitions, helper functions, and the main game loop.
// Included by unity.c - do not compile separately.

#include "game_state.h"
#include "world/cell_defs.h"
#include "world/designations.h"
#include "world/material.h"
#include "core/input_mode.h"
#include "core/pie_menu.h"
#include "entities/jobs.h"
#include "entities/workshops.h"
#include "assets/fonts/comic_embedded.h"
#include "assets/fonts/times_roman_embedded.h"
#include "sound/sound_phrase.h"
#include "sound/sound_synth_bridge.h"
#include "ui/cutscene.h"
#include "ui/console.h"
#include "core/scenarios.h"
#include "core/games.h"
#include "core/describe.h"
#include "core/sim_step.h"
#include "core/day_budget.h"
#include "core/selection.h"
#include "render/inspector.h"
#include "render/world_batch.h"
void rlDrawRenderBatchActive(void);   // rlgl (part of raylib): flush pending draws
#include "render/play_hud.h"

#ifdef USE_SOUNDSYSTEM
#include "soundsystem/soundsystem.h"
#endif

// ============================================================================
// Global State Definitions
// ============================================================================

// World seed for reproducible terrain generation
uint64_t worldSeed = 0;
bool worldSeedFixed = false;


float zoom = 1.0f;
Vector2 offset = {0, 0};
Texture2D atlas;
int currentViewZ = 1;  // Default to z=1 for DF-style (walking level above ground)
bool frontViewMode = false;
int frontViewY = 0;      // Which world-Y slice is the "front" row
int frontViewDepth = 5;  // Number of depth layers visible

// Screen shake
float screenShakeIntensity = 0.0f;
float screenShakeDuration = 0.0f;
float screenShakeTime = 0.0f;

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
bool showCursorDebug = false;
bool showHelpPanel = false;
bool paused = false;
int followMoverIdx = -1;

// Sound debug (phrase/songs experiments)
static bool soundDebugEnabled = false;
static bool soundDebugAuto = true;
static float soundDebugTimer = 0.0f;
static float soundDebugInterval = 2.5f;
static uint32_t soundDebugSeed = 1;
SoundSynth* soundDebugSynth = NULL;  // Non-static so cutscene.c can access it
static const char* soundPalettePath = "assets/sound/phrase_palette.cfg";
static bool soundDebugResponsePending = false;
static float soundDebugResponseTimer = 0.0f;
static SoundPhrase soundDebugResponse;

int pathAlgorithm = 1;
const char* algorithmNames[] = {"A*", "HPA*"};
int currentDirection = 1;
const char* directionNames[] = {"4-dir", "8-dir"};

int currentTool = 0;
const char* toolNames[] = {"Draw Wall", "Draw Floor", "Draw Ladder", "Erase", "Set Start", "Set Goal"};

int currentTerrain = 0;
// NOTE: When adding new terrains, also update the count in ui_panels.c CycleOption() for "Terrain"
const char* terrainNames[] = {"Clear", "Sparse", "City", "Mixed", "Perlin", "Maze", "Dungeon", "Caves", "Drunkard", "Tunneler", "MixMax", "Towers3D", "GalleryFlat", "Castle", "Labyrinth3D", "Spiral3D", "Council", "Hills", "HillsSoils", "HillsSoilsWater", "CraftTest", "TrainTest", "TrainLong", "MoodTest", "Island"};
const int terrainCount = (int)(sizeof(terrainNames) / sizeof(terrainNames[0]));

MaterialType currentTreeType = MAT_OAK;

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

// Game mode
GameMode gameMode = GAME_MODE_SANDBOX;
bool gameOverTriggered = false;
double survivalStartTime = 0.0;
double survivalDuration = 0.0;

// Mover needs toggles (default: off in sandbox)
bool hungerEnabled = false;
bool energyEnabled = false;
bool bodyTempEnabled = false;
bool thirstEnabled = false;
bool bladderEnabled = false;

// UI mode: dev (full panels) vs play (minimal HUD)
bool devUI = true;

// Terrain gen: skip building placement in HillsSoilsWater
bool hillsSkipBuildings = false;

// Super-groups
bool sectionWorld = false;
bool sectionEntities = false;
bool sectionSimulation = false;
bool sectionWeatherTime = false;
bool sectionRenderDebug = false;

// Advanced sub-toggles (hide expert sliders)
bool sectionWaterAdvanced = false;
bool sectionFireAdvanced = false;
bool sectionTemperatureAdvanced = false;
bool sectionEntropyAdvanced = false;
bool sectionSteamAdvanced = false;

bool sectionPathfinding = false;
bool sectionMapEditing = false;
bool sectionMovers = false;
bool sectionMoverNeeds = false;
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
bool sectionDebug = false;
bool sectionDebugRendering = false;
bool sectionDebugPathfinding = false;
bool sectionDebugMovers = false;
bool sectionDebugOverlays = false;
bool sectionProfiler = false;
bool sectionCounters = false;
bool sectionMemory = false;
bool sectionMemGrid = false;
bool sectionMemPath = false;
bool sectionMemEntities = false;
bool sectionMemSpatial = false;
bool sectionJobs = false;
bool sectionTime = false;
bool sectionBalance = false;
bool sectionWeather = false;
bool sectionLighting = false;

int hoveredStockpile = -1;
int hoveredWorkshop = -1;
int workshopSelectedBillIdx = 0;
int linkingWorkshopIdx = -1;  // Which workshop is being linked (for SUBMODE_LINKING_STOCKPILES)
int hoveredMover = -1;
int hoveredAnimal = -1;
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
    SoundSynthPlaySongPhrases(soundDebugSynth, &song);
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
    // Always update synth if it exists (needed for cutscene sounds)
    if (soundDebugSynth) {
        SoundSynthUpdate(soundDebugSynth, dt);
    }

    // Sound debug auto-play (only if enabled)
    if (!soundDebugEnabled || !soundDebugSynth) return;
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

// Warm skin-tone range with seed-based variation
static Color ColorFromSeed(uint32_t seed) {
    uint32_t h = seed * 2654435761u;  // Knuth multiplicative hash
    int r = 160 + (int)(h % 80);         // 160-239
    int g = 120 + (int)((h >> 8) % 60);  // 120-179
    int b = 90  + (int)((h >> 16) % 50); // 90-139
    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
}

static void AssignMoverIdentity(int idx) {
    uint32_t seed = (uint32_t)(worldSeed ^ (uint64_t)idx ^ (uint64_t)GetRandomValue(0, 0x7FFFFFFF));
    Mover* m = &movers[idx];
    m->appearanceSeed = seed;
    m->gender = (seed & 1) ? GENDER_FEMALE : GENDER_MALE;
    m->age = 50 + (seed >> 1) % 21;
    GenerateMoverName(m->name, m->gender, seed);
    moverRenderData[idx].color = ColorFromSeed(seed);
}

Vector2 ScreenToGrid(Vector2 screen) {
    float size = CELL_SIZE * zoom;
    return (Vector2){(screen.x - offset.x) / size, (screen.y - offset.y) / size};
}

Vector2 ScreenToWorld(Vector2 screen) {
    return (Vector2){(screen.x - offset.x) / zoom, (screen.y - offset.y) / zoom};
}

// Front-view coordinate mapping for a specific layer.
// Returns gridX in .x and gridZ in .y (note: .y is Z, not Y!)
// layer=0 is backmost, layer=depthLayers-1 is frontmost.
Vector2 ScreenToGridFrontViewLayer(Vector2 screen, int layer, int depthLayers) {
    float size = CELL_SIZE * zoom;
    float floorThickness = size * 0.25f;
    if (floorThickness < 3.0f) floorThickness = 3.0f;
    float layerOffsetY = (depthLayers - 1 - layer) * floorThickness;
    float gridX = (screen.x - offset.x) / size;
    // Invert: screenY = offset.y + (gridDepth - 1 - z) * size - layerOffsetY
    float gridZ = (float)(gridDepth - 1) - (screen.y + layerOffsetY - offset.y) / size;
    return (Vector2){gridX, gridZ};
}

// Convenience: front layer (layerOffsetY=0)
Vector2 ScreenToGridFrontView(Vector2 screen) {
    int depthLayers = frontViewDepth;
    if (depthLayers < 1) depthLayers = 1;
    return ScreenToGridFrontViewLayer(screen, depthLayers - 1, depthLayers);
}

// Front-view: find the best (x, y, z) under the cursor by iterating layers front-to-back.
// Returns true if a valid cell was found, fills outX/outY/outZ.
// Z is always computed with layerOffset=0 (frontmost layer formula) so it is stable
// and consistent — changing frontViewY only changes which worldY layers are checked,
// not the Z value returned for a given screen position.
bool FrontViewPickCell(Vector2 screen, int *outX, int *outY, int *outZ) {
    int depthLayers = frontViewDepth;
    if (depthLayers < 1) depthLayers = 1;
    int startY = frontViewY - depthLayers + 1;
    if (startY < 0) startY = 0;

    float size = CELL_SIZE * zoom;
    float floorThickness = size * 0.25f;
    if (floorThickness < 3.0f) floorThickness = 3.0f;

    int gx = (int)((screen.x - offset.x) / size);
    if (gx < 0) gx = 0;
    if (gx >= gridWidth) gx = gridWidth - 1;

    // Each depth layer renders with its own vertical offset, so compute gz per-layer.
    // Front layer (depthLayers-1) has offset=0; back layers shift upward by floorThickness each step.
    // screenY = offset.y + (gridDepth-1-z)*size - layerOffsetY
    // => gz = ceil( (gridDepth-1) - (screenY - offset.y + layerOffsetY) / size )
    int fallbackGz = -1;
    for (int layer = depthLayers - 1; layer >= 0; layer--) {
        int worldY = startY + layer;
        if (worldY < 0 || worldY >= gridHeight) continue;

        float layerOffsetY = (depthLayers - 1 - layer) * floorThickness;
        float float_gz = (float)(gridDepth - 1) - (screen.y - offset.y + layerOffsetY) / size;
        int gz = (int)ceilf(float_gz);
        if (gz < 0 || gz >= gridDepth) continue;

        if (fallbackGz < 0) fallbackGz = gz; // use front layer gz as fallback

        CellType cell = GRID(gx, worldY, gz);
        bool hasContent = (cell != CELL_AIR)
            || HAS_FLOOR(gx, worldY, gz)
            || (gz > 0 && CellIsSolid(GRID(gx, worldY, gz - 1)))
            || WATER(gx, worldY, gz).level > 0;

        if (!hasContent) {
            for (int i = 0; i < itemHighWaterMark; i++) {
                Item* it = &items[i];
                if (!it->active) continue;
                if (it->state == ITEM_CARRIED || it->state == ITEM_IN_STOCKPILE) continue;
                if ((int)(it->y / CELL_SIZE) == worldY &&
                    (int)(it->x / CELL_SIZE) == gx &&
                    (int)it->z == gz) { hasContent = true; break; }
            }
        }
        if (!hasContent) {
            for (int i = 0; i < moverCount; i++) {
                Mover* m = &movers[i];
                if (!m->active) continue;
                if ((int)(m->y / CELL_SIZE) == worldY &&
                    (int)(m->x / CELL_SIZE) == gx &&
                    (int)m->z == gz) { hasContent = true; break; }
            }
        }

        if (hasContent) {
            *outX = gx; *outY = worldY; *outZ = gz;
            return true;
        }
    }

    if (fallbackGz < 0) fallbackGz = 0;
    *outX = gx; *outY = frontViewY; *outZ = fallbackGz;
    return false;
}

// Front-view world mapping for a specific layer
static Vector2 ScreenToWorldFrontViewLayer(Vector2 screen, int layer, int depthLayers) {
    float floorThickness = CELL_SIZE * zoom * 0.25f;
    if (floorThickness < 3.0f) floorThickness = 3.0f;
    float layerOffsetY = (depthLayers - 1 - layer) * floorThickness;
    float worldX = (screen.x - offset.x) / zoom;
    float worldZ = (float)(gridDepth - 1) * CELL_SIZE - (screen.y + layerOffsetY - offset.y) / zoom;
    return (Vector2){worldX, worldZ};
}

// Front-view mover finder: match by worldX and worldZ, filter by Y-slice range
// Front-view mover finder: uses screen-space hit testing so hover is stable
// regardless of frontViewY changes. Each mover's screen position is computed
// from its actual layer index and layerOffset, then checked against mouse screen pos.
int GetMoverAtFrontView(float screenX, float screenY, int yStart, int yEnd) {
    float size = CELL_SIZE * zoom;
    int depthLayers = frontViewDepth < 1 ? 1 : frontViewDepth;
    float floorThickness = size * 0.25f;
    if (floorThickness < 3.0f) floorThickness = 3.0f;
    float hitRadius = size * MOVER_SIZE * 0.5f;
    float bestDist = 999999.0f;
    int bestIdx = -1;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        int moverCellY = (int)(m->y / CELL_SIZE);
        if (moverCellY < yStart || moverCellY > yEnd) continue;

        int layer = moverCellY - yStart;
        if (layer < 0 || layer >= depthLayers) continue;
        float layerOff = (depthLayers - 1 - layer) * floorThickness;

        float msx = offset.x + (m->x / CELL_SIZE) * size + size * 0.5f;
        float msy = offset.y + (gridDepth - 1 - (int)m->z) * size - layerOff + size * 0.5f;

        float dx = screenX - msx;
        float dy = screenY - msy;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist < hitRadius && dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// Front-view animal finder: screen-space hit testing, same approach as movers
int GetAnimalAtFrontView(float screenX, float screenY, int yStart, int yEnd) {
    float size = CELL_SIZE * zoom;
    int depthLayers = frontViewDepth < 1 ? 1 : frontViewDepth;
    float floorThickness = size * 0.25f;
    if (floorThickness < 3.0f) floorThickness = 3.0f;
    float hitRadius = size * 0.5f;
    float bestDist = 999999.0f;
    int bestIdx = -1;

    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active) continue;
        int animalCellY = (int)(a->y / CELL_SIZE);
        if (animalCellY < yStart || animalCellY > yEnd) continue;

        int layer = animalCellY - yStart;
        if (layer < 0 || layer >= depthLayers) continue;
        float layerOff = (depthLayers - 1 - layer) * floorThickness;

        float asx = offset.x + (a->x / CELL_SIZE) * size + size * 0.5f;
        float asy = offset.y + (gridDepth - 1 - (int)a->z) * size - layerOff + size * 0.5f;

        float dx = screenX - asx;
        float dy = screenY - asy;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist < hitRadius && dist < bestDist) {
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
        case 21: GenerateTrainTest(); break;
        case 22: GenerateTrainTestLong(); break;
        case 23: GenerateMoodTest(); break;
        case 24: GenerateIsland(); break;
    }
    SyncMaterialsToTerrain();
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

// Abstract nodes one spawn call may explore planning paths inline (~200 ms on a 2048
// map); movers past it are queued for the repath queue (REPATH_EXPLORE_BUDGET per frame)
#define SPAWN_EXPLORE_BUDGET 300000

void AddMoversDemo(int count) {
    long long spawnExplored = 0;
    moverPathAlgorithm = (PathAlgorithm)pathAlgorithm;

    if (pathAlgorithm == 1 && graphEdgeCount == 0) {
        BuildEntrances();
        BuildGraph();
    }
    Point mainArea = GetMainRegionAnchor();   // spawn where movers can get around

    for (int i = 0; i < count && moverCount < MAX_MOVERS; i++) {
        Mover* m = &movers[moverCount];

        Point start = GetRandomWalkableCellReachableFrom(mainArea);
        Point goal = MoverWanderLocal() ? GetRandomGoalNear(start, moverWanderRadius, false) : GetRandomWalkableCell();

        float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
        float z = (float)start.z;
        float speed = balance.baseMoverSpeed * (1.0f + balance.moverSpeedVariance * (GetRandomValue(-100, 100) / 100.0f));

        // The movers' own planner (region check, coarse routes) within a budget; the
        // rest get their paths from the repath queue over the next frames
        InitMover(m, x, y, z, goal, speed);
        if (spawnExplored >= SPAWN_EXPLORE_BUDGET) m->needsRepath = true;
        else {
            if (MoverPlanPath(moverCount, start, goal) == 0) m->needsRepath = true;
            spawnExplored += moverPlanExplored;
        }

        AssignMoverIdentity(moverCount);
        AssignRandomTraits(&movers[moverCount]);
        moverCount++;
    }

    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;
}

static double SpawnClockSec(void) {   // GetTime() needs a window; headless --exec has none
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

void SpawnMoversDemo(int count) {
    double startTime = SpawnClockSec();

    moverPathAlgorithm = (PathAlgorithm)pathAlgorithm;

    if (pathAlgorithm == 1 && graphEdgeCount == 0) {
        BuildEntrances();
        BuildGraph();
    }

    ClearMovers();
    long long spawnExplored = 0;
    Point mainArea = GetMainRegionAnchor();   // spawn where movers can get around
    for (int i = 0; i < count && i < MAX_MOVERS; i++) {
        Mover* m = &movers[moverCount];

        Point start = GetRandomWalkableCellReachableFrom(mainArea);
        Point goal = MoverWanderLocal() ? GetRandomGoalNear(start, moverWanderRadius, false) : GetRandomWalkableCell();

        float x = start.x * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = start.y * CELL_SIZE + CELL_SIZE * 0.5f;
        float z = (float)start.z;
        float speed = balance.baseMoverSpeed * (1.0f + balance.moverSpeedVariance * (GetRandomValue(-100, 100) / 100.0f));

        // The movers' own planner (region check, coarse routes) within a budget; the
        // rest get their paths from the repath queue over the next frames
        InitMover(m, x, y, z, goal, speed);
        if (spawnExplored >= SPAWN_EXPLORE_BUDGET) m->needsRepath = true;
        else {
            if (MoverPlanPath(moverCount, start, goal) == 0) m->needsRepath = true;
            spawnExplored += moverPlanExplored;
        }

        AssignMoverIdentity(moverCount);
        AssignRandomTraits(&movers[moverCount]);
        moverCount++;
    }

    startPos = (Point){-1, -1, 0};
    goalPos = (Point){-1, -1, 0};
    pathLength = 0;

    double elapsed = (SpawnClockSec() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "SpawnMovers: %d movers in %.2fms", moverCount, elapsed);
}

void SpawnMoverAt(int cx, int cy, int cz) {
    if (moverCount >= MAX_MOVERS) return;
    if (!IsCellWalkableAt(cz, cy, cx)) return;

    Mover* m = &movers[moverCount];
    float x = cx * CELL_SIZE + CELL_SIZE * 0.5f;
    float y = cy * CELL_SIZE + CELL_SIZE * 0.5f;
    float speed = balance.baseMoverSpeed;
    Point goal = {cx, cy, cz};

    InitMover(m, x, y, (float)cz, goal, speed);
    AssignMoverIdentity(moverCount);
    AssignRandomTraits(&movers[moverCount]);
    moverCount++;
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
void DrawPlantOverlay(void);
void DrawRampOverlay(void);
void DrawFogOfWar(void);
void DrawMud(void);
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
void DrawAnimals(void);
void DrawTrains(void);
void DrawBelts(void);
void DrawJobLines(void);
void DrawItems(void);
void DrawLightSources(void);
void DrawGatherZones(void);
void DrawStockpileTiles(void);
void DrawStockpileItems(void);
void DrawWorkshops(void);
void DrawFurniture(void);
void DrawLinkingModeHighlights(void);
void DrawWorkshopStatusOverlay(void);
void DrawHaulDestinations(void);
void DrawMiningDesignations(void);
void DrawBlueprints(void);
void DrawTerrainBrushPreview(void);
void DrawWorkshopPlacementPreview(void);
void DrawMaterialHelp(void);
void DrawLightPreview(void);

// From render/tooltips.c
void DrawStockpileTooltip(int spIdx, Vector2 mouse, Vector2 mouseGrid);
void DrawWorkshopTooltip(int wsIdx, Vector2 mouse);
void DrawMoverTooltip(int moverIdx, Vector2 mouse);
void DrawAnimalTooltip(int animalIdx, Vector2 mouse);
void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY);
void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);
void DrawCellTooltip(int cellX, int cellY, int cellZ, Vector2 mouse);
void DrawStationTooltip(int stationIdx, Vector2 mouse);
void DrawBlueprintTooltip(int bpIdx, Vector2 mouse);
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
void RebuildPostLoadState(void);

// ============================================================================
// Headless Mode - Run simulation without GUI
// ============================================================================

// --set <var> <value> (repeatable): set a console variable (see Console_RegisterGameVars)
// at startup, e.g. --set singleFileLadders 0. Applied after --load, before --scenario.
static void ApplySetArgs(int argc, char** argv) {
    for (int i = 1; i + 2 < argc; i++) {
        if (strcmp(argv[i], "--set") != 0) continue;
        if (Console_SetVar(argv[i + 1], argv[i + 2]))
            printf("set %s = %s\n", argv[i + 1], Console_GetVarString(argv[i + 1]));  // after range clamp
        else
            printf("--set: unknown variable '%s' or bad value '%s'\n", argv[i + 1], argv[i + 2]);
        i += 2;
    }
}

// --exec "<console command>" (repeatable): run after the scenario/game is set up,
// e.g. --exec "spawnmovers 3000"
static void ApplyExecArgs(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--exec") != 0) continue;
        Console_Execute(argv[i + 1]);
        i++;
    }
}

// Headless mode: load a save (loadFile) or build a scenario (scenarioName), run N ticks, print stats
// --profile: per-section totals over the whole headless run (the profiler itself
// only keeps the last 120 frames). Called before PROFILE_FRAME_END each tick.
static double WallMs(void) {   // GetTime() needs a window; headless has none
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
// max: worst tick; later: worst tick after the first (which does one-off full computations);
// slow: ticks after the first where the section took over 5 ms (a visible hitch at 60 fps)
static double profTotal[PROFILER_MAX_SECTIONS], profMax[PROFILER_MAX_SECTIONS], profMaxLater[PROFILER_MAX_SECTIONS];
static int profSlow[PROFILER_MAX_SECTIONS], profTicks;
static void HeadlessProfileAccumulate(void) {
    for (int i = 0; i < profilerSectionCount; i++) {
        ProfileSection* s = &profilerSections[i];
        double t = s->accumulating ? s->accumTimeMs : s->lastTimeMs;
        profTotal[i] += t;
        if (t > profMax[i]) profMax[i] = t;
        if (profTicks > 0) {
            if (t > profMaxLater[i]) profMaxLater[i] = t;
            if (t > 5.0) profSlow[i]++;
        }
    }
    profTicks++;
}
static void HeadlessProfilePrint(int ticks) {
    printf("\n=== PROFILE (ms per tick, wall clock) ===\n%-20s %8s %8s %8s %6s\n", "section", "avg", "max", "later", "slow");
    for (int i = 0; i < profilerSectionCount; i++) {
        if (profTotal[i] / ticks < 0.005 && profMax[i] < 0.5) continue;
        printf("%*s%-*s %8.3f %8.2f %8.2f %6d\n", profilerSections[i].depth * 2, "", 20 - profilerSections[i].depth * 2,
               profilerSections[i].name, profTotal[i] / ticks, profMax[i], profMaxLater[i], profSlow[i]);
    }
    printf("(later = worst tick after the first; slow = ticks after the first over 5 ms)\n");
}

static int RunHeadless(const char* loadFile, const char* scenarioName, const char* gameName, int ticks, int argc, char** argv) {
    // Initialize minimal state (no window)
    worldSeedFixed = true;   // headless runs are repeatable (seed 0 unless --seed)
    use8Dir = true;
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    gridDepth = 16;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
        }
    for (int z = 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                GRID(x, y, z) = CELL_AIR;
    InitBalance();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitDesignations();
    InitSimActivity();
    InitTemperature();
    InitSteam();
    InitLighting();
    InitRooms();
    InitPlants();
    InitFarming();

    Console_Init();               // commands for --exec (no window needed)
    Console_RegisterGameVars();   // so --set can reach console variables
    if (loadFile) {
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
        // (LoadWorld already rebuilt the pathfinding graph)
    }
    ApplySetArgs(argc, argv);
    bool doProfile = false;
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--profile") == 0) doProfile = true;
    bool doDayBudget = false;   // --day-budget: where the movers' days go (src/core/day_budget.c)
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--day-budget") == 0) doDayBudget = true;
    double setupMs = 0;
    if (scenarioName) {
        double setupStart = WallMs();
        if (!RunScenario(scenarioName)) return 1;
        setupMs = WallMs() - setupStart;
        printf("Scenario: %s (%d movers, %dx%dx%d)\n", scenarioName, moverCount, gridWidth, gridHeight, gridDepth);
        printf("Running %d ticks headless...\n", ticks);
    }
    if (gameName) {
        double setupStart = WallMs();
        if (!StartGame(gameName)) return 1;
        setupMs = WallMs() - setupStart;
        printf("Game: %s (%d movers, %dx%dx%d)\n", gameName, moverCount, gridWidth, gridHeight, gridDepth);
        printf("Running %d ticks headless...\n", ticks);
    }
    ApplyExecArgs(argc, argv);
    const Scenario* scenario = scenarioName ? ScenarioCurrent() : NULL;

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
    
    // Arrivals: a mover's goal changes while it stands on the old goal
    static Point lastGoal[MAX_MOVERS];
    for (int i = 0; i < moverCount; i++) lastGoal[i] = movers[i].goal;
    int arrivals = 0;

    if (doDayBudget) DayBudgetBegin();

    // Run simulation
    clock_t startClock = clock();
    for (int t = 0; t < ticks; t++) {
        double tickStart = GetTime();
        SimStep(TICK_DT);   // the same tick the windowed game runs (src/core/sim_step.c)
        if (doProfile) HeadlessProfileAccumulate();
        PROFILE_FRAME_END();  // Store profiler data for this tick
        for (int i = 0; i < moverCount; i++) {
            Mover* mv = &movers[i];
            Point g = lastGoal[i];
            if (mv->goal.x == g.x && mv->goal.y == g.y && mv->goal.z == g.z) continue;
            if (mv->active && (int)(mv->x / CELL_SIZE) == g.x && (int)(mv->y / CELL_SIZE) == g.y && (int)mv->z == g.z)
                arrivals++;
            lastGoal[i] = mv->goal;
        }
        if (scenario && scenario->tickStats) scenario->tickStats();
        if (doDayBudget) DayBudgetTick();
        double tickTime = (GetTime() - tickStart) * 1000.0;
        if (tickTime > 100.0) {
            TraceLog(LOG_WARNING, "SLOW TICK %d: %.1fms", t, tickTime);
        }
    }
    double elapsed = (double)(clock() - startClock) / CLOCKS_PER_SEC;
    
    // Snapshot state after
    int stuckAfter = 0;
    int moversInNonWalkable = 0;
    int activeMovers = 0;
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            activeMovers++;
            int mx = (int)(movers[i].x / CELL_SIZE);
            int my = (int)(movers[i].y / CELL_SIZE);
            int mz = (int)movers[i].z;
            // (a mover between levels - mid ladder or ramp - is fine)
            bool betweenLevels = movers[i].z - floorf(movers[i].z) > 0.0001f;
            if (!betweenLevels && !IsCellWalkableAt(mz, my, mx)) {
                moversInNonWalkable++;
            }
            if (movers[i].timeWithoutProgress > 2.0f) {
                stuckAfter++;
            }
        }
    }
    
    // --describe-mover N|all: the mover as the game describes it (same lines as
    // the hover tooltip, src/core/describe.c), after the run. --ticks 0 = as loaded.
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--describe-mover") != 0) continue;
        bool all = strcmp(argv[i + 1], "all") == 0;
        int only = all ? -1 : atoi(argv[i + 1]);
        for (int mi = 0; mi < moverCount; mi++) {
            if ((!all && mi != only) || !movers[mi].active) continue;
            DescLines d;
            DescribeMover(mi, DESC_DEBUG, &d);
            printf("\n");
            for (int l = 0; l < d.count; l++) printf("%s\n", d.lines[l].text);
        }
        if (!all && (only < 0 || only >= moverCount || !movers[only].active))
            printf("\nNo active mover %s (%d movers)\n", argv[i + 1], moverCount);
    }

    printf("\n=== HEADLESS RESULTS ===\n");
    printf("Ticks: %d (%.2f ms, %.2f ms/tick)\n", ticks, elapsed * 1000.0, ticks > 0 ? (elapsed * 1000.0) / ticks : 0.0);
    printf("Movers: %d (%d active)\n", moverCount, activeMovers);
    printf("Stuck in non-walkable: %d before -> %d after\n", stuckBefore, moversInNonWalkable);
    printf("Stuck (no progress > 2s): %d\n", stuckAfter);
    printf("Arrivals (reached goal): %d (%.1f per mover per game-minute)\n", arrivals,
           activeMovers > 0 && ticks > 0 ? arrivals / (double)activeMovers / (ticks * TICK_DT / 60.0) : 0.0);
    printf("Pathfinding: %d FindPath calls, %.1f ms total (%.3f ms each)\n",
           findPathCalls, findPathTotalMs, findPathCalls ? findPathTotalMs / findPathCalls : 0.0);
    if (trainChoicesConsidered > 0 || trainDecisions > 0)
        printf("Train choice: %d trips considered (%s), %d by train\n", trainChoicesConsidered,
               useTrainTimeChoice ? "by trip time" : "40-cell rule", trainDecisions);
    if (trainDecisions > 0)
        printf("Train trips: %d chosen; train faster than walking in %d (%.0f%%); avg %.1f s by train vs %.1f s walking\n",
               trainDecisions, trainDecisionsFaster, 100.0 * trainDecisionsFaster / trainDecisions,
               trainDecisionTrainSec / trainDecisions, trainDecisionWalkSec / trainDecisions);
    extern int repathFallbackCount, repathHpaSuccessCount;
    extern int coarseRoutesPlanned, coarseWindowRefills, coarseLegFails;
    printf("Coarse paths %s: %d routes planned, %d window refills, %d legs blocked (replanned)\n",
           useCoarsePaths ? "on" : "off", coarseRoutesPlanned, coarseWindowRefills, coarseLegFails);
    printf("Repath stats: HPA success=%d, A* fallback=%d (%.1f%% fallback)\n", 
           repathHpaSuccessCount, repathFallbackCount,
           (repathFallbackCount + repathHpaSuccessCount) > 0 
               ? 100.0 * repathFallbackCount / (repathFallbackCount + repathHpaSuccessCount) 
               : 0.0);
    
    if (doProfile) {
        HeadlessProfilePrint(ticks);
        if (setupMs > 0) printf("Scenario setup (world gen + graph): %.0f ms\n", setupMs);
        GridLayersReport();
        {   // HPA* graph and nav scratch: allocated sizes (resident can be less)
            NavWorld* nw = NavGame();
            size_t cells = (size_t)nw->width * nw->height * nw->depth;
            size_t layerCells = (size_t)nw->width * nw->height;
            size_t chunks = (size_t)nw->depth * nw->chunksX * nw->chunksY;
            size_t edgeCapBytes = 0;
            for (int i = 0; i < nw->entranceNum; i++) edgeCapBytes += (size_t)nw->adj[i].cap * sizeof(NavGraphEdge);
            double MB = 1024.0 * 1024.0;
            printf("Nav memory (allocated): total %.0f MB\n", NavMemoryUsage(nw) / MB);
            int sameChunk = 0;   // ladder/ramp entrances have chunk1 == chunk2
            for (int i = 0; i < nw->entranceNum; i++) if (nw->entranceList[i].chunk1 == nw->entranceList[i].chunk2) sameChunk++;
            printf("  entrances on a chunk border: %d, ladder/ramp entrances: %d (%d ramp links, %d ladder links)\n",
                   nw->entranceNum - sameChunk, sameChunk, nw->rampNum, nw->ladderNum);
            printf("  entrances %d (cap %d): %.0f MB   edges %d: %.0f MB (vector capacity %.0f MB)\n",
                   nw->entranceNum, nw->entranceCap,
                   nw->entranceCap * (sizeof(NavEntrance) + sizeof(int) * 2 + sizeof(unsigned) + sizeof(NavEdgeVec)) / MB,
                   nw->edgeNum, nw->edgeNum * sizeof(NavGraphEdge) / MB, edgeCapBytes / MB);
            printf("  flat edge cache: %.0f MB   abstract nodes: %.0f MB   chunk entrance index: %.0f MB\n",
                   nw->flatCap * sizeof(NavGraphEdge) / MB,
                   (nw->entranceCap + 2) * (sizeof(NavAbstractNode) + sizeof(int) * 3) / MB,
                   chunks * sizeof(*nw->chunkEnts) / MB);
            printf("  per-layer scratch: %.0f MB   per-cell A* scratch: %.0f MB   regions: %.0f MB\n",
                   layerCells * (sizeof(NavChunkNode) + sizeof(int) * 2 + sizeof(NavPoint)) / MB,
                   nw->aG ? cells * (sizeof(int) * 5 + 1 + sizeof(uint32_t)) / MB : 0.0,
                   (nw->regLocal ? cells * sizeof(uint16_t) : 0) / MB + nw->regNodeCap * sizeof(int) / MB);
        }
        double gs = WallMs();
        BuildEntrances(); BuildGraph();
        printf("Full graph rebuild: %.0f ms (%d entrances, %d edges)\n", WallMs() - gs,
               entranceCount, graphEdgeCount);
    }

    if (scenario && scenario->printStats) {
        printf("\n=== SCENARIO: %s ===\n", scenario->name);
        scenario->printStats();
    }
    if (doDayBudget) DayBudgetPrint();

    // --check: red flags -> FAIL and exit code 1 (make scenarios-check)
    //   always: no mover stuck (> 2 s without progress), no more movers in
    //   non-walkable cells than at the start (+ --check-max-nonwalkable N allowed)
    //   --check-min-arrivals N: at least N arrivals
    //   plus the scenario's own check (e.g. ladders: never two on a segment)
    bool doCheck = false;
    int minArrivals = 0, allowNonWalkable = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0) doCheck = true;
        else if (strcmp(argv[i], "--check-min-arrivals") == 0 && i + 1 < argc) minArrivals = atoi(argv[++i]);
        else if (strcmp(argv[i], "--check-max-nonwalkable") == 0 && i + 1 < argc) allowNonWalkable = atoi(argv[++i]);
    }
    int checkFailures = 0;
    if (doCheck) {
        printf("\n=== CHECK ===\n");
        if (stuckAfter > 0) { printf("FAIL: %d mover(s) stuck (no progress > 2 s)\n", stuckAfter); checkFailures++; }
        if (moversInNonWalkable > stuckBefore + allowNonWalkable) {
            printf("FAIL: %d mover(s) in non-walkable cells (started with %d, allowed +%d)\n",
                   moversInNonWalkable, stuckBefore, allowNonWalkable);
            checkFailures++;
        }
        if (arrivals < minArrivals) { printf("FAIL: %d arrivals, expected at least %d\n", arrivals, minArrivals); checkFailures++; }
        char why[256] = {0};
        if (scenario && scenario->check && !scenario->check(why, (int)sizeof(why))) {
            printf("FAIL: %s\n", why);
            checkFailures++;
        }
        printf("%s\n", checkFailures ? "CHECK FAILED" : "CHECK PASSED");
    }

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
    int waterCells = 0, fireCells = 0, steamCells = 0, smokeCells = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (WATER(x, y, z).level > 0) waterCells++;
                if (FIRE(x, y, z).level > 0) fireCells++;
                if (STEAM(x, y, z).level > 0) steamCells++;
                if (SMOKE(x, y, z).level > 0) smokeCells++;
            }
        }
    }
    printf("Active cells: Water=%d, Fire=%d, Steam=%d, Smoke=%d\n", 
           waterCells, fireCells, steamCells, smokeCells);
    
    // Smoke per z-level with cell locations
    int smokeTotalLevel = 0;
    int smokeMaxZ = -1;
    int highestSmokeX = -1, highestSmokeY = -1;
    printf("Smoke per z-level: ");
    for (int z = 0; z < gridDepth; z++) {
        int levelSum = 0;
        int cellCount = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (SMOKE(x, y, z).level > 0) {
                    levelSum += SMOKE(x, y, z).level;
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
                    printf("Mover %d (%s): cell (%d,%d,z%d) %s goal=(%d,%d,z%d) path=%d\n",
                           m, MoverDisplayName(m), mx, my, mz, walkable ? "OK" : "STUCK!",
                           mv->goal.x, mv->goal.y, mv->goal.z, mv->pathLength);
                    printf("  H:%.0f%% T:%.0f%% E:%.0f%% B:%.0f%% Mood:%.0f%% Job:%d Freetime:%d\n",
                           mv->hunger*100, mv->thirst*100, mv->energy*100, mv->bladder*100,
                           (mv->mood+1)*50, mv->currentJobId, mv->freetimeState);
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
    
    return checkFailures ? 1 : 0;
}

// ============================================================================
// Screen Shake
// ============================================================================

void TriggerScreenShake(float intensity, float duration) {
    screenShakeIntensity = intensity;
    screenShakeDuration = duration;
    screenShakeTime = 0.0f;
}

void UpdateScreenShake(float dt) {
    if (screenShakeTime < screenShakeDuration) {
        screenShakeTime += dt;
    }
}

Vector2 GetScreenShakeOffset(void) {
    if (screenShakeTime >= screenShakeDuration) {
        return (Vector2){0.0f, 0.0f};
    }

    // Decay shake over time
    float progress = screenShakeTime / screenShakeDuration;
    float decayFactor = 1.0f - progress;

    // Random offset with decay
    float offsetX = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * screenShakeIntensity * decayFactor;
    float offsetY = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * screenShakeIntensity * decayFactor;

    return (Vector2){offsetX, offsetY};
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    // Check for --inspect mode (runs without GUI)
    if (argc >= 2 && strcmp(argv[1], "--inspect") == 0) {
        return InspectSaveFile(argc, argv);
    }

    // --scenario <name> (src/core/scenarios.c); "--scenario list" prints them and exits
    const char* scenarioName = NULL;
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--scenario") == 0) scenarioName = argv[i + 1];
    if (scenarioName && strcmp(scenarioName, "list") == 0) {
        ScenarioPrintList();
        return 0;
    }
    if (scenarioName && !ScenarioFind(scenarioName)) {
        printf("Unknown scenario '%s'\n", scenarioName);
        ScenarioPrintList();
        return 1;
    }

    // --game <name> (src/core/games.c); "--game list" prints them and exits
    const char* gameName = NULL;
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--game") == 0) gameName = argv[i + 1];
    if (gameName && strcmp(gameName, "list") == 0) {
        GamePrintList();
        return 0;
    }
    if (gameName && !GameFind(gameName)) {
        printf("Unknown game '%s'\n", gameName);
        GamePrintList();
        return 1;
    }
    if (gameName && scenarioName) {
        printf("Use --game or --scenario, not both\n");
        return 1;
    }

    // Check for --seed option (for reproducible terrain generation). Parsed before
    // headless, which returns early; headless without --seed keeps seed 0 (deterministic).
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0) {
            if (i + 1 < argc) {
                worldSeed = (uint64_t)strtoull(argv[i + 1], NULL, 10);
                worldSeedFixed = true;
            } else {
                printf("Warning: --seed requires a number\n");
                printf("Usage: %s --seed <number>\n", argv[0]);
            }
            break;
        }
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
            if (!headlessFile && !scenarioName && !gameName) {
                printf("Usage: %s --headless (--load <savefile> | --scenario <name> | --game <name>) [--ticks N] [--set <var> <value>] [--save <outfile>] [--mover N] ...\n", argv[0]);
                return 1;
            }
            return RunHeadless(headlessFile, scenarioName, gameName, headlessTicks, argc, argv);
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

    // Generate seed from time if not specified
    if (worldSeed == 0) {
        worldSeed = (uint64_t)time(NULL);
    }
    printf("World seed: %llu\n", (unsigned long long)worldSeed);

    int screenWidth = 1280, screenHeight = 800;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(screenWidth, screenHeight, "HPA* Pathfinding");
    SetExitKey(0);  // Disable ESC closing the window - we use ESC for input navigation
    atlas = AtlasLoadEmbedded();
    if (!WorldBatchInit(atlas)) TraceLog(LOG_WARNING, "World batch unavailable: terrain drawn through raylib's batch");
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    Font comicFont = LoadEmbeddedFont();
    Font timesRomanFont = LoadEmbeddedTimesRomanFont();
    g_cutscene_font = &timesRomanFont;
    ui_init(&comicFont);
    Console_Init();
    Console_RegisterGameVars();  // Register variables for console access
    SetTraceLogCallback(Console_LogCallback);
    SetTargetFPS(60);
    use8Dir = true;
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    gridDepth = 16;
    // z=0: dirt (solid ground) with grass overlay, z=1+: air (DF-style)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
            SetVegetation(x, y, 0, VEG_GRASS_TALLER);
        }
    for (int z = 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                GRID(x, y, z) = CELL_AIR;
    InitBalance();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitDesignations();
    InitSimActivity();
    InitTemperature();
    InitSteam();
    InitLighting();
    InitRooms();
    InitPlants();
    InitFarming();
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

    // Built-in demo scenarios (src/core/scenarios.c): --scenario <name>
    // Games (src/core/games.c): --game <name>
    ApplySetArgs(argc, argv);
    if (scenarioName) RunScenario(scenarioName);
    if (gameName) StartGame(gameName);
    ApplyExecArgs(argc, argv);

    // --screenshot <file.png>: dev check for rendering changes. The sim stays paused
    // (nothing moves), the camera is centred for the current zoom (--exec "set zoom 0.25",
    // --exec "set viewZ 8"), and the world (without UI, which shows timings) of the third
    // frame is saved, then the game quits. Two builds give the same file for the same view.
    const char* screenshotPath = NULL;
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--screenshot") == 0) screenshotPath = argv[i + 1];
    int lookAtX = -1, lookAtY = -1;
    int screenshotFrame = 0, screenshotTimed = 60;   // --screenshot-frames N: time N frames
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--screenshot-frames") == 0) screenshotTimed = atoi(argv[i + 1]);
    if (screenshotTimed < 1) screenshotTimed = 1;
    // --look-at X,Y: start with the camera centred on that cell (GUI and --screenshot)
    for (int i = 1; i + 1 < argc; i++) {
        int cx, cy;
        if (strcmp(argv[i], "--look-at") == 0 && sscanf(argv[i + 1], "%d,%d", &cx, &cy) == 2) {
            lookAtX = cx; lookAtY = cy;
        }
    }
    if (screenshotPath) {
        SetTargetFPS(0);   // time real frames, not the 60 fps cap
        paused = true;
        UpdateLighting();   // normally computed by the first sim tick, which never runs paused
        offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
        offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;
    }
    if (lookAtX >= 0) {
        offset.x = GetScreenWidth() / 2.0f - (lookAtX + 0.5f) * CELL_SIZE * zoom;
        offset.y = GetScreenHeight() / 2.0f - (lookAtY + 0.5f) * CELL_SIZE * zoom;
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
        {
            // Inspector panel for the selection (src/render/inspector.c)
            Rectangle ip = InspectorPanelRect();
            if (ip.width > 0) ui_add_block_rect(ip);
            // Play HUD tab bar (src/render/play_hud.c)
            Rectangle hud = PlayHudRect();
            if (hud.width > 0) ui_add_block_rect(hud);
        }

        // Developer console
        if (IsKeyPressed(KEY_GRAVE)) Console_Toggle();
        if (Console_IsOpen()) {
            Console_Update();
        } else {
            HandleInput();
        }

        // Sound debug (independent from input modes)
        // KP1 = toggle sound on/off, KP2 = cycle voice demos, KP4 = auto-play
        if (IsKeyPressed(KEY_KP_1)) {
            SoundDebugToggle();
        }
        if (IsKeyPressed(KEY_KP_4)) {
            soundDebugAuto = !soundDebugAuto;
            AddMessage(TextFormat("Sound debug auto: %s", soundDebugAuto ? "ON" : "OFF"),
                       soundDebugAuto ? GREEN : GRAY);
        }
        if (IsKeyPressed(KEY_KP_2)) {
            static int voiceDemoIndex = 0;
            static const char* voiceDemoNames[] = {
                "Call", "Bird", "Vowel", "Phrase Song",
                "Call+Response Variation", "Call+Response Contrast", "Call+Response Mirror"
            };
            soundDebugAuto = false;
            switch (voiceDemoIndex) {
                case 0: SoundDebugPlayCall(); break;
                case 1: SoundDebugPlayBirdOnly(); break;
                case 2: SoundDebugPlayVowelOnly(); break;
                case 3: SoundDebugPlaySong(); break;
                case 4: SoundDebugCallResponseVariation(); break;
                case 5: SoundDebugCallResponseContrast(); break;
                case 6: SoundDebugCallResponseMirror(); break;
            }
            AddMessage(TextFormat("Voice: %s [%d/7] (KP2 next)",
                voiceDemoNames[voiceDemoIndex], voiceDemoIndex + 1), GREEN);
            voiceDemoIndex = (voiceDemoIndex + 1) % 7;
        }
        // Jukebox: KP0 = play/stop, KP+ = next song, KP- = prev song
        if (IsKeyPressed(KEY_KP_0)) {
            SoundDebugEnsure();
            SoundSynthJukeboxToggle(soundDebugSynth);
            if (SoundSynthIsSongPlaying(soundDebugSynth)) {
                int idx = SoundSynthGetCurrentSong(soundDebugSynth);
                AddMessage(TextFormat("Jukebox: %s [%d/%d] (KP0 stop, KP+/- browse)",
                    SoundSynthGetSongName(idx), idx + 1, SoundSynthGetSongCount()), GREEN);
            } else {
                AddMessage("Jukebox: stopped", GRAY);
            }
        }
        if (IsKeyPressed(KEY_KP_ADD)) {
            SoundDebugEnsure();
            SoundSynthJukeboxNext(soundDebugSynth);
            int idx = SoundSynthGetCurrentSong(soundDebugSynth);
            AddMessage(TextFormat("Jukebox: %s [%d/%d]",
                SoundSynthGetSongName(idx), idx + 1, SoundSynthGetSongCount()), GREEN);
        }
        if (IsKeyPressed(KEY_KP_SUBTRACT)) {
            SoundDebugEnsure();
            SoundSynthJukeboxPrev(soundDebugSynth);
            int idx = SoundSynthGetCurrentSong(soundDebugSynth);
            AddMessage(TextFormat("Jukebox: %s [%d/%d]",
                SoundSynthGetSongName(idx), idx + 1, SoundSynthGetSongCount()), GREEN);
        }

        // KP3 = toggle sound log, KP. = dump sound log
        if (IsKeyPressed(KEY_KP_3)) {
            SoundSynthLogToggle();
            AddMessage(TextFormat("Sound log: %s", SoundSynthLogIsEnabled() ? "ON (KP. to dump)" : "OFF"),
                       SoundSynthLogIsEnabled() ? GREEN : GRAY);
        }
        if (IsKeyPressed(KEY_KP_DECIMAL)) {
            SoundSynthLogDump();
            AddMessage("Sound log dumped to navkit_sound.log", GREEN);
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
        UpdateScreenShake(frameTime);
        SoundDebugUpdate(frameTime);
        SoundDebugUpdateResponse(frameTime);

        // Cutscene system (pauses game when active)
        if (IsCutsceneActive()) {
            UpdateCutscene(frameTime);
        }

        if (!paused && !IsCutsceneActive()) {
            bool shouldTick = useFixedTimestep ? (accumulator >= TICK_DT) : true;
            float tickDt = useFixedTimestep ? TICK_DT : frameTime;
            
            if (shouldTick) {
                SimStep(tickDt);   // every system, in order (src/core/sim_step.c)

                // Periodically validate simulation activity counters (every 60 seconds)
                static float simValidationTimer = 0.0f;
                simValidationTimer += tickDt;
                if (simValidationTimer >= 60.0f) {
                    ValidateSimActivityCounts();
                    simValidationTimer = 0.0f;
                }

                if (useFixedTimestep) {
                    accumulator -= TICK_DT;
                    if (accumulator > TICK_DT) {
                        accumulator = TICK_DT;
                    }
                }
            }
        }

        // Game over detection (survival mode)
        if (gameMode == GAME_MODE_SURVIVAL && !gameOverTriggered
            && moverCount > 0 && CountActiveMovers() == 0) {
            survivalDuration = gameTime - survivalStartTime;
            PlayGameOverCutscene();
            gameOverTriggered = true;
        }

        if (followMoverIdx >= 0) {
            if (followMoverIdx >= moverCount || !movers[followMoverIdx].active) {
                followMoverIdx = -1;
            } else {
                float centerX = GetScreenWidth() * 0.5f;
                float centerY = GetScreenHeight() * 0.5f;
                float t = 10.0f * frameTime;
                if (t > 1.0f) t = 1.0f;

                if (frontViewMode) {
                    // Front view: X from mover world X, Y from mover world Z
                    float size = CELL_SIZE * zoom;
                    float targetX = centerX - (movers[followMoverIdx].x / CELL_SIZE) * size;
                    float targetY = centerY - (gridDepth - 1 - movers[followMoverIdx].z) * size;
                    offset.x += (targetX - offset.x) * t;
                    offset.y += (targetY - offset.y) * t;
                    // Track mover's Y-slice as front depth layer
                    frontViewY = (int)(movers[followMoverIdx].y / CELL_SIZE);
                } else {
                    float targetX = centerX - movers[followMoverIdx].x * zoom;
                    float targetY = centerY - movers[followMoverIdx].y * zoom;
                    offset.x += (targetX - offset.x) * t;
                    offset.y += (targetY - offset.y) * t;
                    currentViewZ = (int)movers[followMoverIdx].z;
                }
            }
        }

        PROFILE_BEGIN(Render);
        BeginDrawing();
        double worldDrawStart = GetTime();
        ClearBackground(BLACK);

        // Apply screen shake offset
        Vector2 shakeOffset = GetScreenShakeOffset();
        offset.x += shakeOffset.x;
        offset.y += shakeOffset.y;

        if (frontViewMode) {
            DrawFrontView();
        } else {
        PROFILE_BEGIN(DrawCells);
        WorldBatchSetView(offset.x, offset.y, CELL_SIZE * zoom, (float)GetTime());
        WorldBatchBegin();   // these passes draw only through FastSprite/FastRect (world_batch.h)
        DrawCellGrid();
        DrawGrassOverlay();
        DrawPlantOverlay();
        DrawCloudShadows();  // Draw cloud shadows first (under everything)
        DrawMud();
        DrawSnow();          // Draw snow after mud
        DrawRampOverlay();   // Draw ramps after grass/mud/snow so they're visible
        DrawWater();
        DrawFrozenWater();
        DrawFire();
        DrawSmoke();
        DrawSteam();
        DrawTemperature();
        WorldBatchEnd();
        DrawSimSources();
        PROFILE_END(DrawCells);
        if (inputAction == ACTION_WORK_GATHER) {
            DrawGatherZones();
        }
        DrawStockpileTiles();
        DrawWorkshops();
        DrawFurniture();
        DrawLinkingModeHighlights();  // Highlight workshop and stockpile during linking
        DrawWorkshopStatusOverlay();   // Status icons on workshops
        DrawHaulDestinations();
        DrawStockpileItems();
        DrawMiningDesignations();
        DrawBlueprints();
        DrawTerrainBrushPreview();
        DrawWorkshopPlacementPreview();
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
        DrawBelts();
        if (showItems) {
            WorldBatchBegin();
            DrawItems();
            WorldBatchEnd();
        }
        DrawLightSources();
        DrawTrackLines();
        if (showMovers) {
            PROFILE_BEGIN(DrawMovers);
            DrawMovers();
            PROFILE_END(DrawMovers);
        }
        DrawAnimals();
        DrawTrains();
        DrawJobLines();

        // Phase 5: Mist overlay (after all world elements, before UI)
        DrawMist();
        DrawFogOfWar();
        } // end !frontViewMode

        if (screenshotPath) {
            // Frame 3 is saved; frames 4-63 time the world drawing (CPU side, flushed)
            rlDrawRenderBatchActive();   // flush pending sprites before reading/timing
            static double worldMsSum = 0.0, screenshotFrameMs = 0.0;
            static double screenshotSecMs[PROFILER_MAX_SECTIONS];
            screenshotFrame++;
            if (screenshotFrame == 3) {
                Image shot = LoadImageFromScreen();
                ExportImage(shot, screenshotPath);
                UnloadImage(shot);
            } else if (screenshotFrame > 3) {
                worldMsSum += (GetTime() - worldDrawStart) * 1000.0;
                screenshotFrameMs += GetFrameTime() * 1000.0;
#if PROFILER_ENABLED
                for (int s = 0; s < profilerSectionCount && s < PROFILER_MAX_SECTIONS; s++)
                    screenshotSecMs[s] += profilerSections[s].lastTimeMs;   // Render/EndDraw: the previous frame's
#endif
                if (screenshotFrame == 3 + screenshotTimed) {
                    printf("screenshot: world drawing %.2f ms per frame (avg of %d)\n", worldMsSum / screenshotTimed, screenshotTimed);
                    printf("whole frame %.2f ms (no fps cap, sim paused)\n", screenshotFrameMs / screenshotTimed);
                    printf("zoom %.3f viewZ %d screen %dx%d\n", zoom, currentViewZ, GetScreenWidth(), GetScreenHeight());
#if PROFILER_ENABLED
                    // Per-pass split of the render sections (sum of their last times per frame)
                    for (int s = 0; s < profilerSectionCount; s++) {
                        int a = s; while (a >= 0 && strcmp(profilerSections[a].name, "Render") != 0) a = profilerSections[a].parent;
                        if (a < 0) continue;
                        printf("  %*s%-20s %7.2f ms\n", profilerSections[s].depth * 2, "",
                               profilerSections[s].name, screenshotSecMs[s] / screenshotTimed);
                    }
#endif
                    shouldQuit = true;
                }
            }
        }

        // Draw workshop preview when in workshop placement mode
        // Workshop actions and WorkshopType enums are in the same order, so derive directly
        WorkshopType previewWorkshopType = WORKSHOP_TYPE_COUNT;  // invalid sentinel
        if (inputAction >= ACTION_DRAW_WORKSHOP_STONECUTTER &&
            inputAction <= ACTION_DRAW_WORKSHOP_STONECUTTER + WORKSHOP_TYPE_COUNT - 1) {
            previewWorkshopType = (WorkshopType)(inputAction - ACTION_DRAW_WORKSHOP_STONECUTTER);
        }

        if (previewWorkshopType != WORKSHOP_TYPE_COUNT && !isDragging) {
            int wsWidth = workshopDefs[previewWorkshopType].width;
            int wsHeight = workshopDefs[previewWorkshopType].height;

            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            float size = CELL_SIZE * zoom;
            float px = offset.x + x * size;
            float py = offset.y + y * size;
            float pw = wsWidth * size;
            float ph = wsHeight * size;

            // Check if placement is valid
            bool valid = true;
            for (int dy = 0; dy < wsHeight && valid; dy++) {
                for (int dx = 0; dx < wsWidth && valid; dx++) {
                    int cx = x + dx;
                    int cy = y + dy;
                    if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) valid = false;
                    else if (!IsCellWalkableAt(currentViewZ, cy, cx)) valid = false;
                    else if (FindWorkshopAt(cx, cy, currentViewZ) >= 0) valid = false;
                }
            }
            
            if (!valid) {
                Color fillColor = (Color){200, 50, 50, 80};
                DrawRectangle((int)px, (int)py, (int)pw, (int)ph, fillColor);
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, RED);
            } else {
                const char* tmpl = workshopDefs[previewWorkshopType].template;
                for (int dy = 0; dy < wsHeight; dy++) {
                    for (int dx = 0; dx < wsWidth; dx++) {
                        char tile = tmpl[dy * wsWidth + dx];
                        Color fillColor;
                        switch (tile) {
                            case WT_BLOCK: fillColor = (Color){200, 80, 80, 100}; break;
                            case WT_WORK:  fillColor = (Color){80, 200, 80, 100}; break;
                            case WT_OUTPUT: fillColor = (Color){80, 150, 220, 100}; break;
                            case WT_FUEL:  fillColor = (Color){220, 160, 50, 100}; break;
                            default:       fillColor = (Color){180, 140, 80, 80}; break;
                        }
                        float tx = offset.x + (x + dx) * size;
                        float ty = offset.y + (y + dy) * size;
                        DrawRectangle((int)tx, (int)ty, (int)size, (int)size, fillColor);
                    }
                }
                DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, (Color){200, 160, 100, 255});
            }
        }
        
        // Draw drag preview when dragging (but not for sculpt brush)
        if (isDragging && inputAction != ACTION_NONE &&
            inputAction != ACTION_SANDBOX_SCULPT) {
            float size = CELL_SIZE * zoom;

            if (frontViewMode) {
                // Front view drag: X and Z (height) span the selection, Y (depth) fixed at dragStartY
                int fvGx, fvGy, fvGz;
                FrontViewPickCell(GetMousePosition(), &fvGx, &fvGy, &fvGz);
                (void)fvGy;

                int x1 = dragStartX < fvGx ? dragStartX : fvGx;
                int x2 = dragStartX > fvGx ? dragStartX : fvGx;
                int z1 = dragStartZ < fvGz ? dragStartZ : fvGz;
                int z2 = dragStartZ > fvGz ? dragStartZ : fvGz;
                if (x1 < 0) x1 = 0; if (x2 >= gridWidth)  x2 = gridWidth  - 1;
                if (z1 < 0) z1 = 0; if (z2 >= gridDepth)   z2 = gridDepth  - 1;

                // Layer offset for the fixed Y depth layer (same formula as hover highlight)
                int _dl = frontViewDepth < 1 ? 1 : frontViewDepth;
                int _startY = frontViewY - _dl + 1;
                if (_startY < 0) _startY = 0;
                int _layer = dragStartY - _startY;
                if (_layer < 0) _layer = 0;
                if (_layer >= _dl) _layer = _dl - 1;
                float _ft = size * 0.25f;
                if (_ft < 3.0f) _ft = 3.0f;
                float layerOffsetY = (_dl - 1 - _layer) * _ft;

                // Screen rect: top-left is (x1, z2-top), spans right and downward
                float px = offset.x + x1 * size;
                float py = offset.y + (gridDepth - 1 - z2) * size - layerOffsetY;
                float pw = (x2 - x1 + 1) * size;
                float ph = (z2 - z1 + 1) * size;

                bool isRightDrag = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
                bool useGhostPreview = !isRightDrag && (
                    inputAction == ACTION_DRAW_WALL ||
                    inputAction == ACTION_DRAW_FLOOR ||
                    inputAction == ACTION_DRAW_LADDER ||
                    inputAction == ACTION_DRAW_RAMP);

                if (useGhostPreview) {
                    MaterialType mat = MAT_GRANITE;
                    ItemType source = ITEM_BLOCKS;
                    switch (selectedMaterial) {
                        case 2: mat = MAT_OAK;       source = ITEM_PLANKS; break;
                        case 3: mat = MAT_DIRT;      source = ITEM_DIRT;   break;
                        case 4: mat = MAT_OAK;       source = ITEM_PLANKS; break;
                        case 5: mat = MAT_PINE;      source = ITEM_PLANKS; break;
                        case 6: mat = MAT_BIRCH;     source = ITEM_PLANKS; break;
                        case 7: mat = MAT_WILLOW;    source = ITEM_PLANKS; break;
                        case 8: mat = MAT_SANDSTONE; source = ITEM_BLOCKS; break;
                        case 9: mat = MAT_SLATE;     source = ITEM_BLOCKS; break;
                        default: mat = MAT_GRANITE;  source = ITEM_BLOCKS; break;
                    }
                    if (selectedMaterial == 2) source = ITEM_LOG;

                    for (int dz = z1; dz <= z2; dz++) {
                        for (int dx = x1; dx <= x2; dx++) {
                            if (dx < 0 || dx >= gridWidth || dz < 0 || dz >= gridDepth) continue;
                            float cellScreenY = offset.y + (gridDepth - 1 - dz) * size - layerOffsetY;
                            Rectangle dest = {offset.x + dx * size, cellScreenY, size, size};
                            int sprite = -1;

                            if (inputAction == ACTION_DRAW_WALL) {
                                if (GRID(dx, dragStartY, dz) == CELL_WALL &&
                                    GetWallMaterial(dx, dragStartY, dz) == mat) continue;
                                if (source == ITEM_PLANKS) {
                                    switch (mat) {
                                        case MAT_PINE:   sprite = SPRITE_tree_planks_pine; break;
                                        case MAT_BIRCH:  sprite = SPRITE_tree_planks_birch; break;
                                        case MAT_WILLOW: sprite = SPRITE_tree_planks_willow; break;
                                        default:         sprite = SPRITE_tree_planks_oak; break;
                                    }
                                } else {
                                    sprite = GetSpriteForCellMat(CELL_WALL, mat);
                                }
                            } else if (inputAction == ACTION_DRAW_FLOOR) {
                                if (CellBlocksMovement(GRID(dx, dragStartY, dz))) continue;
                                if (source == ITEM_PLANKS) {
                                    switch (mat) {
                                        case MAT_PINE:   sprite = SPRITE_tree_planks_pine; break;
                                        case MAT_BIRCH:  sprite = SPRITE_tree_planks_birch; break;
                                        case MAT_WILLOW: sprite = SPRITE_tree_planks_willow; break;
                                        default:         sprite = SPRITE_tree_planks_oak; break;
                                    }
                                } else if (IsWoodMaterial(mat)) {
                                    sprite = SPRITE_floor_wood;
                                } else {
                                    int ms = MaterialSprite(mat);
                                    sprite = ms ? ms : SPRITE_floor;
                                }
                            } else if (inputAction == ACTION_DRAW_LADDER) {
                                sprite = CellSprite(CELL_LADDER_UP);
                            } else if (inputAction == ACTION_DRAW_RAMP) {
                                CellType rampDir = selectedRampDirection;
                                if (rampDir == CELL_AIR) {
                                    rampDir = AutoDetectRampDirection(dx, dragStartY, dz);
                                }
                                if (rampDir == CELL_AIR) rampDir = CELL_RAMP_N;
                                sprite = CellSprite(rampDir);
                            }

                            if (sprite >= 0) {
                                Rectangle src = SpriteGetRect(sprite);
                                Color ghostTint = {140, 220, 140, 120};
                                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, ghostTint);
                            }
                        }
                    }
                    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, GREEN);
                } else {
                    Color fillColor = {100, 200, 100, 80};
                    Color lineColor = GREEN;
                    if (isRightDrag) {
                        fillColor = (Color){200, 0, 0, 80};
                        lineColor = RED;
                    } else {
                        switch (inputAction) {
                            case ACTION_DRAW_STOCKPILE:   fillColor = (Color){0, 200, 0, 80};    lineColor = GREEN;                     break;
                            case ACTION_DRAW_REFUSE_PILE: fillColor = (Color){139, 90, 43, 80};  lineColor = BROWN;                     break;
                            case ACTION_WORK_MINE:        fillColor = (Color){255, 150, 0, 80};  lineColor = ORANGE;                    break;
                            case ACTION_WORK_CONSTRUCT:   fillColor = (Color){0, 200, 200, 80};  lineColor = (Color){0, 255, 255, 255}; break;
                            case ACTION_WORK_GATHER:      fillColor = (Color){255, 180, 50, 80}; lineColor = ORANGE;                    break;
                            case ACTION_SANDBOX_WATER:    fillColor = (Color){0, 100, 200, 80};  lineColor = SKYBLUE;                   break;
                            case ACTION_SANDBOX_FIRE:     fillColor = (Color){200, 50, 0, 80};   lineColor = RED;                       break;
                            case ACTION_SANDBOX_HEAT:     fillColor = (Color){200, 100, 0, 80};  lineColor = ORANGE;                    break;
                            default: break;
                        }
                    }
                    DrawRectangle((int)px, (int)py, (int)pw, (int)ph, fillColor);
                    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, lineColor);
                }
            } else {
                // Top-down drag preview
                Vector2 gp = ScreenToGrid(GetMousePosition());
                int x = (int)gp.x, y = (int)gp.y;
                int x1 = dragStartX < x ? dragStartX : x;
                int y1 = dragStartY < y ? dragStartY : y;
                int x2 = dragStartX > x ? dragStartX : x;
                int y2 = dragStartY > y ? dragStartY : y;
                float px = offset.x + x1 * size;
                float py = offset.y + y1 * size;
                float pw = (x2 - x1 + 1) * size;
                float ph = (y2 - y1 + 1) * size;

                bool isRightDrag = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
                bool useGhostPreview = !isRightDrag && (
                    inputAction == ACTION_DRAW_WALL ||
                    inputAction == ACTION_DRAW_FLOOR ||
                    inputAction == ACTION_DRAW_LADDER ||
                    inputAction == ACTION_DRAW_RAMP);

                if (useGhostPreview) {
                    // Ghost preview: draw actual sprites semi-transparent
                    // Determine sprite based on action + selected material
                    MaterialType mat = MAT_GRANITE;
                    ItemType source = ITEM_BLOCKS;
                    // Replicate GetDrawMaterial logic (it's static in input.c)
                    switch (selectedMaterial) {
                        case 2: mat = MAT_OAK;    source = ITEM_PLANKS; break;
                        case 3: mat = MAT_DIRT;    source = ITEM_DIRT;   break;
                        case 4: mat = MAT_OAK;    source = ITEM_PLANKS; break;
                        case 5: mat = MAT_PINE;   source = ITEM_PLANKS; break;
                        case 6: mat = MAT_BIRCH;  source = ITEM_PLANKS; break;
                        case 7: mat = MAT_WILLOW; source = ITEM_PLANKS; break;
                        case 8: mat = MAT_SANDSTONE; source = ITEM_BLOCKS; break;
                        case 9: mat = MAT_SLATE;     source = ITEM_BLOCKS; break;
                        default: mat = MAT_GRANITE;  source = ITEM_BLOCKS; break;
                    }
                    // Material case 2 is log, not planks
                    if (selectedMaterial == 2) source = ITEM_LOG;

                    for (int dy = y1; dy <= y2; dy++) {
                        for (int dx = x1; dx <= x2; dx++) {
                            if (dx < 0 || dx >= gridWidth || dy < 0 || dy >= gridHeight) continue;
                            Rectangle dest = {offset.x + dx * size, offset.y + dy * size, size, size};
                            int sprite = -1;

                            if (inputAction == ACTION_DRAW_WALL) {
                                // Skip cells that already have this wall
                                if (GRID(dx, dy, currentViewZ) == CELL_WALL &&
                                    GetWallMaterial(dx, dy, currentViewZ) == mat) continue;
                                if (source == ITEM_PLANKS) {
                                    // Plank wall sprite per wood species
                                    switch (mat) {
                                        case MAT_PINE:   sprite = SPRITE_tree_planks_pine; break;
                                        case MAT_BIRCH:  sprite = SPRITE_tree_planks_birch; break;
                                        case MAT_WILLOW: sprite = SPRITE_tree_planks_willow; break;
                                        default:         sprite = SPRITE_tree_planks_oak; break;
                                    }
                                } else {
                                    sprite = GetSpriteForCellMat(CELL_WALL, mat);
                                }
                            } else if (inputAction == ACTION_DRAW_FLOOR) {
                                // Skip solid cells
                                if (CellBlocksMovement(GRID(dx, dy, currentViewZ))) continue;
                                if (source == ITEM_PLANKS) {
                                    switch (mat) {
                                        case MAT_PINE:   sprite = SPRITE_tree_planks_pine; break;
                                        case MAT_BIRCH:  sprite = SPRITE_tree_planks_birch; break;
                                        case MAT_WILLOW: sprite = SPRITE_tree_planks_willow; break;
                                        default:         sprite = SPRITE_tree_planks_oak; break;
                                    }
                                } else if (IsWoodMaterial(mat)) {
                                    sprite = SPRITE_floor_wood;
                                } else {
                                    int ms = MaterialSprite(mat);
                                    sprite = ms ? ms : SPRITE_floor;
                                }
                            } else if (inputAction == ACTION_DRAW_LADDER) {
                                sprite = CellSprite(CELL_LADDER_UP);
                            } else if (inputAction == ACTION_DRAW_RAMP) {
                                CellType rampDir = selectedRampDirection;
                                if (rampDir == CELL_AIR) {
                                    rampDir = AutoDetectRampDirection(dx, dy, currentViewZ);
                                }
                                if (rampDir == CELL_AIR) rampDir = CELL_RAMP_N;
                                sprite = CellSprite(rampDir);
                            }

                            if (sprite >= 0) {
                                Rectangle src = SpriteGetRect(sprite);
                                Color ghostTint = {140, 220, 140, 120};
                                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, ghostTint);
                            }
                        }
                    }
                    // Draw border around entire drag area
                    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 2.0f, GREEN);
                } else {
                    // Non-ghost: colored rectangle overlay
                    Color fillColor = {100, 200, 100, 80};
                    Color lineColor = GREEN;

                    if (isRightDrag) {
                        fillColor = (Color){200, 0, 0, 80};
                        lineColor = RED;
                    } else {
                        switch (inputAction) {
                            case ACTION_DRAW_STOCKPILE:
                                fillColor = (Color){0, 200, 0, 80};
                                lineColor = GREEN;
                                break;
                            case ACTION_DRAW_REFUSE_PILE:
                                fillColor = (Color){139, 90, 43, 80};
                                lineColor = BROWN;
                                break;
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
            }
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
            int idleY = zY - 18;
            DrawTextShadow(TextFormat("Idle: %d", idleMoverCount), 5, idleY, 16, LIGHTGRAY);
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
        DrawProfilerPanel(GetScreenWidth() - 50, 95);  // Below time widget
#endif

        DrawSelectionHighlight();   // src/render/inspector.c

        PROFILE_BEGIN(DrawUI);
        DrawUI();
        PROFILE_END(DrawUI);

        DrawInspectorPanel();       // the selected thing (after DrawUI: ui frame has begun)

        // Input mode bar at bottom - individual buttons (after DrawUI so ui_begin_frame has run)
        if (devUI) {
            int barH = 28;
            int barX = 150;  // After Z level display
            int padding = 12;
            int spacing = 12;
            int fontSize = 16;
            int screenW = GetScreenWidth();
            int rightMargin = 10;

            BarItem items[MAX_BAR_ITEMS];
            int itemCount = InputMode_GetBarItems(items);

            // Pre-measure to determine how many rows we need
            int rowCount = 1;
            {
                int testX = barX;
                for (int i = 0; i < itemCount; i++) {
                    int textW = MeasureTextUI(items[i].text, fontSize);
                    int btnW = textW + padding * 2 + 8;
                    if (testX + btnW > screenW - rightMargin && testX > barX) {
                        rowCount++;
                        testX = barX;
                    }
                    testX += btnW + spacing;
                }
            }

            int barY = GetScreenHeight() - (barH + 4) * rowCount - 2;
            int x = barX;
            int currentRow = 0;

            for (int i = 0; i < itemCount; i++) {
                int textW = MeasureTextUI(items[i].text, fontSize);
                int btnW = textW + padding * 2 + 8;  // Extra width for right margin

                // Wrap to next row if this item would overflow
                if (x + btnW > screenW - rightMargin && x > barX) {
                    currentRow++;
                    x = barX;
                }

                int rowY = barY + currentRow * (barH + 4);
                int textY = rowY + (barH - fontSize) / 2 - 4;  // Shift up more for bottom margin

                bool isClickable = (items[i].key != 0 || items[i].backSteps > 0) && !items[i].isHint;
                Rectangle btnRect = {x, rowY, btnW, barH};
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
                    DrawRectangle(x, rowY, btnW, barH, bgColor);
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
                        if (items[i].backSteps > 0) {
                            // Header click: go back N levels directly
                            for (int b = 0; b < items[i].backSteps; b++) {
                                InputMode_Back();
                            }
                        } else if (items[i].key != 0) {
                            InputMode_TriggerKey(items[i].key);
                        }
                        ui_consume_click();
                    }
                }

                x += btnW + spacing;
            }
            
            // Pile radius control (show when in soil drawing mode)
            if (inputAction >= ACTION_DRAW_SOIL_DIRT && inputAction <= ACTION_DRAW_SOIL_PEAT) {
                int lastRowY = barY + currentRow * (barH + 4);
                x += 40;  // Extra spacing before control
                DraggableFloatT(x, lastRowY + 5, "Pile Radius", &soilPileRadius, 0.5f, 1.0f, 10.0f,
                    "Shift+draw: How far soil spreads when piling (1-10)");
            }
        }

        // Pie menu overlay
        PieMenu_Draw();

        DrawMessages(GetScreenWidth(), GetScreenHeight());

        // Front view toggle button (bottom-right corner)
        {
            const char* label = frontViewMode ? "[V] Top Down" : "[V] Front View";
            int fontSize = 16;
            int textW = MeasureText(label, fontSize);
            int btnX = GetScreenWidth() - textW - 20;
            int btnY = GetScreenHeight() - fontSize - 50;
            int btnW = textW + 16;
            int btnH = fontSize + 8;
            Vector2 mouse = GetMousePosition();
            bool hovered = mouse.x >= btnX && mouse.x < btnX + btnW && mouse.y >= btnY && mouse.y < btnY + btnH;
            Color bgColor = hovered ? (Color){80, 80, 80, 200} : (Color){40, 40, 40, 180};
            DrawRectangle(btnX, btnY, btnW, btnH, bgColor);
            DrawRectangleLines(btnX, btnY, btnW, btnH, hovered ? WHITE : GRAY);
            DrawTextShadow(label, btnX + 8, btnY + 4, fontSize, hovered ? WHITE : LIGHTGRAY);
            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                frontViewMode = !frontViewMode;
                if (frontViewMode) {
                    frontViewY = gridHeight / 2;
                }
            }
        }

        // Compute tooltip grid coordinates (view-mode aware)
        {
        int ttX, ttY, ttZ;  // tooltip cell coords
        bool frontPickHit = false;  // true = hit real content, false = fell back to frontViewY plane
        Vector2 ttGridVec;
        if (frontViewMode) {
            frontPickHit = FrontViewPickCell(GetMousePosition(), &ttX, &ttY, &ttZ);
            ttGridVec = (Vector2){(float)ttX, (float)ttY};
        } else {
            ttGridVec = ScreenToGrid(GetMousePosition());
            ttX = (int)ttGridVec.x;
            ttY = (int)ttGridVec.y;
            ttZ = currentViewZ;
        }

        // Debug: show computed grid coords + hovered cell highlight + cursor dot
        if (showCursorDebug) {
            Vector2 mp = GetMousePosition();
            float size = CELL_SIZE * zoom;
            // Highlight hovered cell
            float hsx, hsy;
            if (frontViewMode) {
                // Recompute the layer offset for the picked cell's Y layer so the highlight
                // sits exactly where the renderer drew it.
                int _dl = frontViewDepth < 1 ? 1 : frontViewDepth;
                int _startY = frontViewY - _dl + 1;
                if (_startY < 0) _startY = 0;
                int _layer = ttY - _startY;
                if (_layer < 0) _layer = 0;
                if (_layer >= _dl) _layer = _dl - 1;
                float _ft = size * 0.25f;
                if (_ft < 3.0f) _ft = 3.0f;
                float _layerOffsetY = (_dl - 1 - _layer) * _ft;
                hsx = offset.x + ttX * size;
                hsy = offset.y + (gridDepth - 1 - ttZ) * size - _layerOffsetY;
            } else {
                hsx = offset.x + ttX * size;
                hsy = offset.y + ttY * size;
            }
            // In front view: green = hit real content, orange = default plane (frontViewY)
            Color highlightColor = frontViewMode
                ? (frontPickHit ? (Color){80, 255, 80, 220} : (Color){255, 160, 40, 200})
                : (Color){255, 255, 0, 200};
            DrawRectangleLinesEx((Rectangle){hsx, hsy, size, size}, 2.0f, highlightColor);
            // Top face: small squished rect above the cell to suggest a 3D box
            if (frontViewMode) {
                float topH = size * 0.2f;
                if (topH < 3.0f) topH = 3.0f;
                Color topColor = {highlightColor.r, highlightColor.g, highlightColor.b, 120};
                DrawRectangle((int)hsx, (int)(hsy - topH), (int)size, (int)topH, topColor);
                DrawRectangleLinesEx((Rectangle){hsx, hsy - topH, size, topH}, 1.5f, highlightColor);
            }
            // Coords next to the highlighted cell
            const char* cellText = TextFormat("%d,%d,%d", ttX, ttY, ttZ);
            DrawTextShadow(cellText, (int)(hsx + size + 2), (int)(hsy + 2), 12, YELLOW);
            // Red dot at actual mouse position
            DrawCircle((int)mp.x, (int)mp.y, 4.0f, RED);
            // Coords label
            const char* coordText = TextFormat("x:%d y:%d z:%d", ttX, ttY, ttZ);
            DrawTextShadow(coordText, (int)mp.x + 16, (int)mp.y - 16, 14, YELLOW);
        }

        if (hoveredStockpile >= 0) {
            DrawStockpileTooltip(hoveredStockpile, GetMousePosition(), ttGridVec);
        }

        if (hoveredWorkshop >= 0 && hoveredStockpile < 0) {
            DrawWorkshopTooltip(hoveredWorkshop, GetMousePosition());
        }

        if (hoveredMover >= 0) {
            DrawMoverTooltip(hoveredMover, GetMousePosition());
        }

        if (hoveredAnimal >= 0) {
            DrawAnimalTooltip(hoveredAnimal, GetMousePosition());
        }

        if (hoveredItemCount > 0 && hoveredMover < 0 && hoveredStockpile < 0) {
            DrawItemTooltip(hoveredItemCell, hoveredItemCount, GetMousePosition(), ttX, ttY);
        }

        if (hoveredStockpile < 0 && hoveredMover < 0 && hoveredItemCount == 0 && hoveredWorkshop < 0) {
            if (ttX >= 0 && ttX < gridWidth && ttY >= 0 && ttY < gridHeight &&
                ttZ >= 0 && ttZ < gridDepth) {
                // Check for train station at this cell
                int stIdx = GetStationAt(ttX, ttY, ttZ);
                if (stIdx >= 0) {
                    DrawStationTooltip(stIdx, GetMousePosition());
                }
                // Check for blueprint at this cell
                else if (GetBlueprintAt(ttX, ttY, ttZ) >= 0) {
                    DrawBlueprintTooltip(GetBlueprintAt(ttX, ttY, ttZ), GetMousePosition());
                }
                // Check for any designation at this cell
                else if (GetDesignation(ttX, ttY, ttZ) != NULL) {
                    DrawDesignationTooltip(ttX, ttY, ttZ, GetMousePosition());
                }
                else if (paused) {
                    // When paused, show comprehensive cell info
                    DrawCellTooltip(ttX, ttY, ttZ, GetMousePosition());
                } else if (HasWater(ttX, ttY, ttZ) ||
                    WATER(ttX, ttY, ttZ).isSource ||
                    WATER(ttX, ttY, ttZ).isDrain) {
                    // When running, only show water tooltip if there's water
                    DrawWaterTooltip(ttX, ttY, ttZ, GetMousePosition());
                }
            }
        }

        DrawTooltip();
        }
        Console_Draw();

        // Cutscene system (renders as overlay on top of game)
        if (IsCutsceneActive()) {
            RenderCutscene();
        }
        
        // Phase 5: Lightning flash (full-screen overlay, after everything including UI)
        DrawLightningFlash();

        // Restore offset after screen shake
        offset.x -= shakeOffset.x;
        offset.y -= shakeOffset.y;

        PROFILE_BEGIN(EndDraw);
        EndDrawing();
        PROFILE_END(EndDraw);
        PROFILE_END(Render);
        if (!paused) PROFILE_FRAME_END();
    }
    UnloadTexture(atlas);
    UnloadFont(comicFont);
    UnloadFont(timesRomanFont);
    if (soundDebugSynth) {
        SoundSynthDestroy(soundDebugSynth);
        soundDebugSynth = NULL;
    }
    CloseWindow();
    return 0;
}
