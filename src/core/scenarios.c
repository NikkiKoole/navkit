// scenarios.c - Built-in demo/test scenarios (see scenarios.h)
#include "scenarios.h"
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../world/designations.h"
#include "../simulation/plants.h"
#include "../simulation/farming.h"
#include "../simulation/floordirt.h"
#include "../simulation/lighting.h"
#include "../simulation/weather.h"
#include "../simulation/balance.h"
#include "../simulation/rooms.h"
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../core/sim_manager.h"
#include <strings.h>

// ---------------------------------------------------------------------------
// Settings scenarios change. Snapshotted before the first scenario runs and
// restored by every ScenarioResetWorld, so one scenario's tweaks (e.g. mood's
// needs + speed) don't leak into the next.
// ---------------------------------------------------------------------------
typedef struct {
    bool hunger, thirst, energy, endless, preferDiffZ;
    float dayLength, baseMoverSpeed;
} ScenarioSettings;

static ScenarioSettings savedSettings;
static bool haveSavedSettings = false;
static const Scenario* currentScenario = NULL;

// Mover count override for scenarios that support it (0 = the scenario's default).
// Console/--set var scenarioMovers, e.g. --set scenarioMovers 96 --scenario ladder-choice
int scenarioMovers = 0;

// West part of the ground (z=1), 15 per row, rows 2 apart: crowd-size experiments
static void SpawnGroundCrowd(int n) {
    for (int i = 0; i < n; i++) SpawnMoverAt(2 + (i % 15), 2 + ((i / 15) * 2) % 28 + (i / 210), 1);
}

static void SnapshotOrRestoreSettings(void) {
    if (!haveSavedSettings) {
        savedSettings = (ScenarioSettings){hungerEnabled, thirstEnabled, energyEnabled,
                                           endlessMoverMode, preferDifferentZ,
                                           dayLength, balance.baseMoverSpeed};
        haveSavedSettings = true;
        return;
    }
    hungerEnabled = savedSettings.hunger;
    thirstEnabled = savedSettings.thirst;
    energyEnabled = savedSettings.energy;
    endlessMoverMode = savedSettings.endless;
    preferDifferentZ = savedSettings.preferDiffZ;
    dayLength = savedSettings.dayLength;
    balance.baseMoverSpeed = savedSettings.baseMoverSpeed;
}

// The startup map: 32x32x16, z=0 natural dirt with tall grass, air above (walk on z=1).
static void GenerateFlatDefaultWorld(void) {
    InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    gridDepth = 16;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
            SetVegetation(x, y, 0, VEG_GRASS_TALLER);
        }
    for (int z = 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[z][y][x] = CELL_AIR;
}

static int FindTerrainIndex(const char* name) {
    for (int i = 0; i < terrainCount; i++)
        if (strcasecmp(terrainNames[i], name) == 0) return i;
    return -1;
}

void ScenarioResetWorld(const char* terrain) {
    SnapshotOrRestoreSettings();

    // Entities first (ClearMovers also drops jobs and reservations)
    ClearMovers();
    ClearAnimals();
    ClearTrains();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearFurniture();
    ClearWater();
    // Designations + blueprints before generating: some generators place them
    InitDesignations();
    InitPlants();
    ClearFarming();

    int t = terrain ? FindTerrainIndex(terrain) : -1;
    if (t >= 0) {
        currentTerrain = t;
        GenerateCurrentTerrain();
    } else {
        if (terrain) TraceLog(LOG_WARNING, "Scenario terrain '%s' not found, using flat map", terrain);
        GenerateFlatDefaultWorld();
    }

    // Derived state for the new grid (same list as the Generate Terrain button,
    // minus InitDesignations, which would wipe generator-placed designations)
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
    RebuildSimActivityCounts();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitFloorDirt();
    InitSnow();
    InitRooms();
    InvalidateLighting();
    BuildEntrances();
    BuildGraph();
}

// ---------------------------------------------------------------------------
// ladders: single-file ladder demo
// ---------------------------------------------------------------------------

// Ground at z=1, a plank deck at z=2 over the east half, ONE ladder between
// them, 16 movers wandering (endless mode, preferring goals on the other level),
// so a queue forms at the ladder. Toggle the rule with the "Single-file Ladders"
// switch or the console var singleFileLadders.
static void ResetLadderStats(void);

static void BuildLadderDeck(void) {
    const int deckX = 18;
    for (int y = 0; y < gridHeight; y++)
        for (int x = deckX; x < gridWidth; x++) {
            SET_FLOOR(x, y, 2);
            SetFloorMaterial(x, y, 2, MAT_OAK);
        }
}

static void AddLadderAt(int x, int y) {
    grid[1][y][x] = CELL_LADDER_BOTH;
    grid[2][y][x] = CELL_LADDER_BOTH;
}

static void SetupLadders(void) {
    ResetLadderStats();
    BuildLadderDeck();
    AddLadderAt(18, 16);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    preferDifferentZ = true;
    if (scenarioMovers > 0) SpawnGroundCrowd(scenarioMovers);
    else for (int i = 0; i < 16; i++) SpawnMoverAt(4 + (i % 8) * 2, 10 + (i / 8) * 10, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Ladder scenario: 16 movers, one ladder (single-file %s)",
                          useSingleFileLadders ? "on" : "off"), GREEN);
}

// ladder-choice: the same deck with TWO ladders, one next to where the movers
// start (18,16) and a second 6 cells north (18,10). Compare the ladder queue cost
// on/off ("Ladder Queue Cost" / console var ladderQueueCost); scenarioMovers sets
// the crowd size. Arrivals per mover, 6000 ticks, queue cost on vs off:
// 16 movers 18.6 vs 17.6 (queues stay short), 48 movers 14.5 vs 12.6 (+15%),
// 96 movers 7.3 vs 6.9 (both ladders saturated). With the second ladder far away
// (18,2) a detour never paid.
static void SetupLadderChoice(void) {
    ResetLadderStats();
    BuildLadderDeck();
    AddLadderAt(18, 16);
    AddLadderAt(18, 10);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    preferDifferentZ = true;
    if (scenarioMovers > 0) SpawnGroundCrowd(scenarioMovers);
    else for (int i = 0; i < 16; i++) SpawnMoverAt(8 + (i % 8), 14 + (i / 8) * 4, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Ladder choice: 16 movers, near + far ladder (queue cost %s)",
                          useLadderQueueCost ? "on" : "off"), GREEN);
}

// Queue stats: a mover "at" a ladder stands on a ladder cell with its next
// waypoint straight up/down (single-file waiters are held there); a climber is
// between two levels on a ladder cell.
#define LADDER_STAT_CELLS 64
static int ladderLongestQueue, ladderLongestX, ladderLongestY, ladderLongestZ;
static int ladderMostClimbers, ladderSharedTicks;  // ticks with 2+ climbers on one segment
static double ladderWaitSum;   // sum over ticks of movers at a ladder
static int ladderTicks;

// Climbs started per ladder (x,y), counted when a mover goes from standing to
// between levels on a ladder cell
#define LADDER_STAT_LADDERS 16
typedef struct { int x, y, climbs; } LadderUse;
static LadderUse ladderUses[LADDER_STAT_LADDERS];
static int ladderUseCount;
static bool moverWasClimbing[MAX_MOVERS];

static void ResetLadderStats(void) {
    ladderUseCount = 0;
    memset(moverWasClimbing, 0, sizeof(moverWasClimbing));
    ladderLongestQueue = ladderMostClimbers = ladderSharedTicks = ladderTicks = 0;
    ladderLongestX = ladderLongestY = ladderLongestZ = -1;
    ladderWaitSum = 0.0;
}

typedef struct { int x, y, z, n; } LadderCount;

static void BumpCount(LadderCount* list, int* count, int x, int y, int z, int* outN) {
    for (int k = 0; k < *count; k++)
        if (list[k].x == x && list[k].y == y && list[k].z == z) { *outN = ++list[k].n; return; }
    if (*count < LADDER_STAT_CELLS) list[(*count)++] = (LadderCount){x, y, z, 1};
    *outN = 1;
}

static void LadderTickStats(void) {
    LadderCount queues[LADDER_STAT_CELLS], climbs[LADDER_STAT_CELLS];
    int queueCount = 0, climbCount = 0, waitingNow = 0;
    bool shared = false;
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        bool wasClimbing = moverWasClimbing[i];
        moverWasClimbing[i] = false;
        if (!m->active) continue;
        int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE);
        int z = (int)floorf(m->z);
        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight || z < 0 || z >= gridDepth) continue;
        if (!IsLadderCell(grid[z][y][x])) continue;
        int n;
        if (m->z - (float)z > 0.0001f) {
            moverWasClimbing[i] = true;
            if (!wasClimbing) {
                int k = 0;
                while (k < ladderUseCount && (ladderUses[k].x != x || ladderUses[k].y != y)) k++;
                if (k == ladderUseCount && ladderUseCount < LADDER_STAT_LADDERS)
                    ladderUses[ladderUseCount++] = (LadderUse){x, y, 0};
                if (k < ladderUseCount) ladderUses[k].climbs++;
            }
            BumpCount(climbs, &climbCount, x, y, z, &n);
            if (n > ladderMostClimbers) ladderMostClimbers = n;
            if (n > 1) shared = true;
            continue;
        }
        if (m->pathLength <= 0 || m->pathIndex < 0) continue;
        Point next = moverPaths[i][m->pathIndex];
        if (next.x != x || next.y != y || next.z == z) continue;
        waitingNow++;
        BumpCount(queues, &queueCount, x, y, z, &n);
        if (n > ladderLongestQueue) {
            ladderLongestQueue = n;
            ladderLongestX = x; ladderLongestY = y; ladderLongestZ = z;
        }
    }
    ladderWaitSum += waitingNow;
    if (shared) ladderSharedTicks++;
    ladderTicks++;
}

static void LadderPrintStats(void) {
    printf("Single-file ladders: %s, ladder queue cost: %s\n", useSingleFileLadders ? "on" : "off",
           useLadderQueueCost ? "on" : "off");
    for (int k = 0; k < ladderUseCount; k++)
        printf("Climbs on ladder at (%d,%d): %d\n", ladderUses[k].x, ladderUses[k].y, ladderUses[k].climbs);
    if (ladderLongestQueue > 0)
        printf("Longest queue at a ladder: %d (cell %d,%d,z%d)\n",
               ladderLongestQueue, ladderLongestX, ladderLongestY, ladderLongestZ);
    else
        printf("Longest queue at a ladder: 0\n");
    printf("Avg movers at a ladder per tick: %.2f\n", ladderTicks ? ladderWaitSum / ladderTicks : 0.0);
    printf("Most climbers on one ladder segment at once: %d (%d of %d ticks with 2+)\n",
           ladderMostClimbers, ladderSharedTicks, ladderTicks);
}

// ---------------------------------------------------------------------------
// mood: comfort vs rough camps (was the dev-UI "Mood Scenario" button)
// ---------------------------------------------------------------------------
static void SetupMood(void) {
    // Enable needs + slower day cycle with proportional speed
    hungerEnabled = true;
    thirstEnabled = true;
    energyEnabled = true;
    dayLength = 333.0f;
    balance.baseMoverSpeed = 1066.0f;
    // 10 movers spread across both camps; walking level z=2 (ground is z=0+z=1)
    for (int i = 0; i < 5; i++) {
        SpawnMoverAt(4 + i, 8, 2);    // comfort camp
        SpawnMoverAt(34 + i, 8, 2);   // rough camp
    }
    currentViewZ = 2;
    AddMessage("Mood scenario: 10 movers, needs ON", GREEN);
}

// ---------------------------------------------------------------------------
// crafting / train: the CraftTest / TrainTest / TrainLong terrains plus movers
// ---------------------------------------------------------------------------
static void SpawnMoverRow(int x0, int y, int z, int count) {
    for (int i = 0; i < count; i++) SpawnMoverAt(x0 + i, y, z);
}

static void SetupCrafting(void) {
    SpawnMoverRow(8, 20, 1, 4);
    currentViewZ = 1;
    AddMessage("Crafting scenario: mine -> stonecutter -> wall blueprint, 4 movers", GREEN);
}

static void SetupTrain(void) {
    SpawnMoverRow(4, 3, 1, 4);
    currentViewZ = 1;
    AddMessage("Train scenario: rocks at station A, stockpile at B, 4 movers", GREEN);
}

static void SetupTrainLong(void) {
    SpawnMoverRow(12, 5, 1, 4);
    currentViewZ = 1;
    AddMessage("Train (long) scenario: 3 stations, 4-car train, 4 movers", GREEN);
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
static const Scenario scenarios[] = {
    {"ladders",    "Ground + deck joined by one ladder, 16 wandering movers: watch the queue",
                   NULL,        SetupLadders,  LadderTickStats, LadderPrintStats},
    {"ladder-choice", "Deck with two ladders 6 cells apart, 16 movers: compare ladderQueueCost on/off",
                   NULL,        SetupLadderChoice, LadderTickStats, LadderPrintStats},
    {"mood",       "Comfort camp vs rough camp, 10 movers, needs on, slow days",
                   "MoodTest",  SetupMood,     NULL, NULL},
    {"crafting",   "Mine walls -> stonecutter -> stockpiles -> wall blueprint, 4 movers",
                   "CraftTest", SetupCrafting, NULL, NULL},
    {"train",      "Track loop, two stations, rocks to haul by train, 4 movers",
                   "TrainTest", SetupTrain,    NULL, NULL},
    {"train-long", "Long platforms, three stations, 4-car train, 4 movers",
                   "TrainLong", SetupTrainLong, NULL, NULL},
};
#define SCENARIO_COUNT ((int)(sizeof(scenarios) / sizeof(scenarios[0])))

int ScenarioCount(void) { return SCENARIO_COUNT; }

const Scenario* ScenarioAt(int index) {
    return (index >= 0 && index < SCENARIO_COUNT) ? &scenarios[index] : NULL;
}

const Scenario* ScenarioFind(const char* name) {
    for (int i = 0; i < SCENARIO_COUNT; i++)
        if (strcasecmp(scenarios[i].name, name) == 0) return &scenarios[i];
    return NULL;
}

const char** ScenarioNames(void) {
    static const char* names[SCENARIO_COUNT];
    for (int i = 0; i < SCENARIO_COUNT; i++) names[i] = scenarios[i].name;
    return names;
}

const Scenario* ScenarioCurrent(void) { return currentScenario; }

void ScenarioPrintList(void) {
    printf("Scenarios:\n");
    for (int i = 0; i < SCENARIO_COUNT; i++)
        printf("  %-11s %s\n", scenarios[i].name, scenarios[i].description);
}

bool RunScenario(const char* name) {
    const Scenario* s = ScenarioFind(name);
    if (!s) {
        printf("Unknown scenario '%s'\n", name);
        ScenarioPrintList();
        return false;
    }
    ScenarioResetWorld(s->terrain);
    s->setup();
    currentScenario = s;
    // Centre the camera on the new map (no-op without a window)
    offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;
    TraceLog(LOG_INFO, "Scenario '%s': %d movers on %dx%dx%d", s->name, moverCount, gridWidth, gridHeight, gridDepth);
    return true;
}
