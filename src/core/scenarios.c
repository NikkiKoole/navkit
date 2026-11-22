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
// --set scenarioSize N: square map size for scenarios that have a size (world, hubs, ...)
int scenarioSize = 0;
// --set scenarioChunk N: HPA* chunk size for sized scenarios (default 16)
int scenarioChunk = 16;

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
static int flatWorldW = 32, flatWorldH = 32;   // set by RunScenario (scenario size, else 32x32)

static void GenerateFlatDefaultWorld(void) {
    if (flatWorldW == 32 && flatWorldH == 32) InitGridWithSizeAndChunkSize(32, 32, 8, 8);
    else InitGridWithSizeAndChunkSize(flatWorldW, flatWorldH, 16, 16);
    gridDepth = 16;
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
}

static int FindTerrainIndex(const char* name) {
    for (int i = 0; i < terrainCount; i++)
        if (strcasecmp(terrainNames[i], name) == 0) return i;
    return -1;
}

void ScenarioResetWorld(const char* terrain) {
    SnapshotOrRestoreSettings();
    moverGoalPicker = NULL;
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

void ScenarioResetWorldSized(const char* terrain, int w, int h) {
    int cs = scenarioChunk > 0 ? scenarioChunk : 16;
    if (w > 0 && h > 0 && (gridWidth != w || gridHeight != h || chunkWidth != cs || chunkHeight != cs))
        InitGridWithSizeAndChunkSize(w, h, cs, cs);
    flatWorldW = w > 0 ? w : 32;
    flatWorldH = h > 0 ? h : 32;
    ScenarioResetWorld(terrain);
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
    GRID(x, y, 1) = CELL_LADDER_BOTH;
    GRID(x, y, 2) = CELL_LADDER_BOTH;
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
        if (!IsLadderCell(GRID(x, y, z))) continue;
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

// --check: single-file must hold (never two climbers on one segment)
static bool LadderCheck(char* why, int whyLen) {
    if (useSingleFileLadders && ladderSharedTicks > 0) {
        snprintf(why, (size_t)whyLen, "two or more climbers on one ladder segment in %d ticks", ladderSharedTicks);
        return false;
    }
    return true;
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
    int n = scenarioMovers > 0 ? scenarioMovers : 4;
    SpawnMoverRow(4, 3, 1, n);
    currentViewZ = 1;
    AddMessage(TextFormat("Train scenario: rocks at station A, stockpile at B, %d movers", n), GREEN);
}

static void SetupTrainLong(void) {
    int n = scenarioMovers > 0 ? scenarioMovers : 4;
    SpawnMoverRow(12, 5, 1, n);
    currentViewZ = 1;
    AddMessage(TextFormat("Train (long) scenario: 3 stations, 4-car train, %d movers", n), GREEN);
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// hubs: many movers sharing a few destinations (pathfinding load test)
// ---------------------------------------------------------------------------

// 128x128 hills (levels, ramps), movers shuttle between 6 hubs spread over the
// map: the colony pattern (stockpiles, kitchen, beds) where many movers go to
// the same few places. Default 200 movers (scenarioMovers overrides).
#define HUB_COUNT 6
static Point hubs[HUB_COUNT];
static int hubCount;

static bool HubGoalPicker(int moverIdx, Point* goal) {
    if (hubCount == 0) return false;
    const Mover* m = &movers[moverIdx];
    int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE), cz = (int)m->z;
    for (int tries = 0; tries < 8; tries++) {
        Point h = hubs[GetRandomValue(0, hubCount - 1)];
        if (abs(h.x - cx) + abs(h.y - cy) < 4 && h.z == cz) continue;   // not the hub it is at
        *goal = h;   // the shared destination: the hub cell itself
        return true;
    }
    return false;
}

// World: a generated map with water, movers wandering to random goals. A load
// test for map size: ./build/bin/path --headless --scenario world --set scenarioSize 512 --profile
static void SetupWorld(void) {
    endlessMoverMode = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 100, spawned = 0;
    Point mainArea = GetMainRegionAnchor();   // not on hilltops nobody can leave
    for (int tries = 0; spawned < n && tries < n * 200; tries++) {
        int x = GetRandomValue(0, gridWidth - 1), y = GetRandomValue(0, gridHeight - 1);
        for (int z = 1; z < gridDepth; z++) {
            if (!IsCellWalkableAt(z, y, x)) continue;
            if (mainArea.x < 0 || IsReachable(mainArea, (Point){x, y, z})) { SpawnMoverAt(x, y, z); spawned++; }
            break;
        }
    }
    AddMessage(TextFormat("World: %d movers on %dx%d", spawned, gridWidth, gridHeight), GREEN);
}

static void SetupHubs(void) {
    // Hubs: walkable cells in the biggest reachability region, spread out (a
    // jittered grid over the map, each snapped to the nearest good cell)
    hubCount = 0;
    Point anchor = GetMainRegionAnchor();
    for (int k = 0; k < HUB_COUNT && anchor.x >= 0; k++) {
        int tx = (k % 3) * gridWidth / 3 + gridWidth / 6, ty = (k / 3) * gridHeight / 2 + gridHeight / 4;
        Point best = {-1, -1, -1};
        int bestD = 1 << 30;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y += 2)
                for (int x = 0; x < gridWidth; x += 2) {
                    if (!IsCellWalkableAt(z, y, x)) continue;
                    int d = abs(x - tx) + abs(y - ty);
                    if (d >= bestD || !IsReachable(anchor, (Point){x, y, z})) continue;
                    bestD = d;
                    best = (Point){x, y, z};
                }
        if (best.x >= 0) hubs[hubCount++] = best;
    }
    moverGoalPicker = HubGoalPicker;
    endlessMoverMode = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 200;
    for (int i = 0; i < n; i++) {
        Point p = hubs[i % (hubCount > 0 ? hubCount : 1)];
        Point s = {p.x + GetRandomValue(-3, 3), p.y + GetRandomValue(-3, 3), p.z};
        if (s.x < 0 || s.y < 0 || s.x >= gridWidth || s.y >= gridHeight || !IsCellWalkableAt(s.z, s.y, s.x)) s = p;
        SpawnMoverAt(s.x, s.y, s.z);
    }
    currentViewZ = hubCount > 0 ? hubs[0].z : 1;
    AddMessage(TextFormat("Hubs: %d movers between %d hubs on %dx%d hills", n, hubCount, gridWidth, gridHeight), GREEN);
}

// ---------------------------------------------------------------------------
// train-double: two tracks (one per direction), platforms on both sides
// ---------------------------------------------------------------------------

// A narrow loop reads as a double track: the north line runs east, the south
// line west (all trains run the same way round). Three stops, each with a
// platform on both outer sides, so a mover can queue on the side whose trains
// go its way. Towns north and south of each stop; movers travel between them.
// Compare the trip choice with --set useTrainTimeChoice 0 (old 40-cell rule),
// and train speed with --set trainSpeed N.
#define TD_TOWNS 6
static Point tdTowns[TD_TOWNS];

static bool TrainDoubleGoalPicker(int moverIdx, Point* goal) {
    const Mover* m = &movers[moverIdx];
    int cx = (int)(m->x / CELL_SIZE);
    for (int tries = 0; tries < 8; tries++) {
        Point t = tdTowns[GetRandomValue(0, TD_TOWNS - 1)];
        if (abs(t.x - cx) < 20) continue;                 // another town, not this stop
        *goal = (Point){t.x + GetRandomValue(-3, 3), t.y + GetRandomValue(-2, 2), 1};
        return true;
    }
    return false;
}

static void SetupTrainDouble(void) {
    const int x0 = 4, x1 = gridWidth - 5, yN = 10, yS = 13;
    for (int x = x0; x <= x1; x++) {
        GRID(x, yN, 1) = CELL_TRACK; TRACK_CONN(x, yN, 1) = TRACK_E | TRACK_W;
        GRID(x, yS, 1) = CELL_TRACK; TRACK_CONN(x, yS, 1) = TRACK_E | TRACK_W;
    }
    for (int y = yN; y <= yS; y++) {
        GRID(x0, y, 1) = CELL_TRACK; TRACK_CONN(x0, y, 1) = TRACK_N | TRACK_S;
        GRID(x1, y, 1) = CELL_TRACK; TRACK_CONN(x1, y, 1) = TRACK_N | TRACK_S;
    }
    TRACK_CONN(x0, yN, 1) = TRACK_E | TRACK_S;
    TRACK_CONN(x1, yN, 1) = TRACK_W | TRACK_S;
    TRACK_CONN(x0, yS, 1) = TRACK_E | TRACK_N;
    TRACK_CONN(x1, yS, 1) = TRACK_W | TRACK_N;
    const int stops[3] = {gridWidth / 6, gridWidth / 2, gridWidth * 5 / 6};
    for (int k = 0; k < 3; k++)
        for (int x = stops[k] - 3; x <= stops[k] + 2; x++) {
            GRID(x, yN - 1, 1) = CELL_PLATFORM;   // north side: eastbound trains
            GRID(x, yS + 1, 1) = CELL_PLATFORM;   // south side: westbound trains
        }
    RebuildStations();
    // Clockwise: east along the north line, west along the south line
    SpawnTrainHeading(stops[0] - 10, yN, 1, 4, 1, 0);
    SpawnTrainHeading(stops[2] + 10, yS, 1, 4, -1, 0);
    BuildEntrances();
    BuildGraph();
    for (int k = 0; k < 3; k++) {
        tdTowns[2 * k] = (Point){stops[k], 4, 1};                   // town north of the tracks
        tdTowns[2 * k + 1] = (Point){stops[k], gridHeight - 5, 1};  // town south
    }
    moverGoalPicker = TrainDoubleGoalPicker;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 32;
    for (int i = 0; i < n; i++) {
        Point t = tdTowns[i % TD_TOWNS];
        SpawnMoverAt(t.x + GetRandomValue(-4, 4), t.y + GetRandomValue(-2, 2), 1);
    }
    currentViewZ = 1;
    AddMessage(TextFormat("Double track: %d movers, 3 stops with a platform per direction", n), GREEN);
}

static void TrainDoublePrintStats(void) {
    printf("Stations (platform side, boardings):\n");
    for (int i = 0; i < stationCount; i++)
        printf("  station %d at x=%d %s: %d boarded\n", i, stations[i].platX,
               stations[i].platY < stations[i].trackY ? "north (eastbound)" : "south (westbound)",
               StationBoardings(i));
}

static const Scenario scenarios[] = {
    {"ladders",    "Ground + deck joined by one ladder, 16 wandering movers: watch the queue",
                   NULL,        SetupLadders,  LadderTickStats, LadderPrintStats, LadderCheck, 0, 0},
    {"ladder-choice", "Deck with two ladders 6 cells apart, 16 movers: compare ladderQueueCost on/off",
                   NULL,        SetupLadderChoice, LadderTickStats, LadderPrintStats, LadderCheck, 0, 0},
    {"mood",       "Comfort camp vs rough camp, 10 movers, needs on, slow days",
                   "MoodTest",  SetupMood,     NULL, NULL, NULL, 0, 0},
    {"crafting",   "Mine walls -> stonecutter -> stockpiles -> wall blueprint, 4 movers",
                   "CraftTest", SetupCrafting, NULL, NULL, NULL, 0, 0},
    {"train",      "Track loop, two stations, rocks to haul by train, 4 movers",
                   "TrainTest", SetupTrain,    NULL, NULL, NULL, 0, 0},
    {"train-long", "Long platforms, three stations, 4-car train, 4 movers",
                   "TrainLong", SetupTrainLong, NULL, NULL, NULL, 0, 0},
    {"hubs",       "128x128 hills, 200 movers shuttling between 6 hubs: pathfinding load",
                   "Hills",     SetupHubs,     NULL, NULL, NULL, 128, 128},
    {"world",      "Hills with soils and water, 100 wandering movers; scenarioSize sets the map (for --profile)",
                   "HillsSoilsWater", SetupWorld, NULL, NULL, NULL, 256, 256},
    {"maze",       "3D labyrinth, 100 wandering movers: long paths (coarse paths, step cap); scenarioSize sets the map",
                   "Labyrinth3D", SetupWorld, NULL, NULL, NULL, 256, 256},
    {"train-double", "Double track (one per direction), platforms on both sides, 3 stops, 32 movers",
                   NULL,        SetupTrainDouble, NULL, TrainDoublePrintStats, NULL, 240, 24},
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
    int w = s->gridW, h = s->gridH;
    if (w > 0 && h > 0 && scenarioSize > 0) {
        w = h = scenarioSize < MAX_GRID_WIDTH ? scenarioSize : MAX_GRID_WIDTH;
    }
    ScenarioResetWorldSized(s->terrain, w, h);
    s->setup();
    currentScenario = s;
    // Centre the camera on the new map (no-op without a window)
    offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;
    TraceLog(LOG_INFO, "Scenario '%s': %d movers on %dx%dx%d", s->name, moverCount, gridWidth, gridHeight, gridDepth);
    return true;
}
