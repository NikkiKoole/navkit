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
#include "../entities/belts.h"
#include "../entities/elevators.h"
#include "../entities/containers.h"
#include "../core/sim_manager.h"
#include "../simulation/needs.h"
#include "../simulation/tide.h"
#include <string.h>
#include <strings.h>

// ---------------------------------------------------------------------------
// Settings scenarios change. Snapshotted before the first scenario runs and
// restored by every ScenarioResetWorld, so one scenario's tweaks (e.g. mood's
// needs + speed) don't leak into the next.
// ---------------------------------------------------------------------------
typedef struct {
    bool hunger, thirst, energy, endless, preferDiffZ;
    float dayLength, baseMoverSpeed, jobDistanceWeight, trainSpeed;
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
                                           dayLength, balance.baseMoverSpeed, jobDistanceWeight,
                                           trainSpeed};
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
    jobDistanceWeight = savedSettings.jobDistanceWeight;
    trainSpeed = savedSettings.trainSpeed;
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
    ClearElevators();
    ClearBelts();
    ClearTide();
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
    ClearLightSources();   // belong to the old world (subway lights its stations)
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
// dayrhythm: 8 movers, 8 beds, a stockpile of dried berries (doesn't spoil), endless walls
// to mine. Headless prints, per day, how many movers are in bed at 03:00 and at 13:00 and
// how many walls were mined: the soft day rhythm (setting sleepRhythm, see needs.h) should
// give ~all in bed at night and ~none at midday, without costing work. Compare with
// --set sleepRhythm 0. scenarioMovers > 0 limits the walls (0 walls: nothing to do).
// ---------------------------------------------------------------------------
#define DR_MAX_DAYS 32
static int drBed3[DR_MAX_DAYS], drBed13[DR_MAX_DAYS], drMined[DR_MAX_DAYS], drFood[DR_MAX_DAYS], drMovers;
static float drPrevHour;

static int DayRhythmWallsLeft(void) {
    int n = 0;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if (DESIGNATION(x, y, 1).type == DESIGNATION_MINE) n++;
    return n;
}

static void SetupDayRhythm(void) {
    hungerEnabled = true; energyEnabled = true;
    thirstEnabled = false; bladderEnabled = false; bodyTempEnabled = false;
    dayLength = 360.0f;                 // the games' pace: 6-minute day, walking 3x
    balance.baseMoverSpeed = 600.0f;    // (before the movers spawn: they take it from here)
    int maxWalls = scenarioMovers > 0 ? scenarioMovers : 1000000;
    int cx = gridWidth / 2;
    for (int i = 0; i < 8; i++) SpawnFurniture(cx - 8 + i * 2, 2, 1, FURNITURE_PLANK_BED, MAT_OAK);
    CreateStockpile(cx - 3, 5, 1, 6, 3);
    for (int i = 0; i < 18; i++) {
        int idx = SpawnItem((cx - 3 + i % 6) * CELL_SIZE + 4, (5 + i / 6) * CELL_SIZE + 4, 1, ITEM_DRIED_BERRIES);
        if (idx >= 0) items[idx].stackCount = 20;
    }
    int n = 0;
    for (int y = 10; y < gridHeight - 2 && n < maxWalls; y += 2)
        for (int x = 2; x < gridWidth - 2 && n < maxWalls; x += 2) {
            GRID(x, y, 1) = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_GRANITE);
            DesignateMine(x, y, 1);
            n++;
        }
    BuildEntrances(); BuildGraph();
    for (int i = 0; i < 8; i++) SpawnMoverAt(cx - 4 + i, 8, 1);
    currentViewZ = 1;
    memset(drBed3, 0, sizeof(drBed3)); memset(drBed13, 0, sizeof(drBed13));
    for (int d = 0; d < DR_MAX_DAYS; d++) drMined[d] = -1;
    drMovers = 8;
    drPrevHour = timeOfDay;
}

static void DayRhythmTickStats(void) {
    int day = dayNumber % DR_MAX_DAYS;
    for (int k = 0; k < 2; k++) {
        float h = k == 0 ? 3.0f : 13.0f;
        if (drPrevHour < h && timeOfDay >= h) {
            int inBed = 0;
            for (int i = 0; i < moverCount; i++)
                if (movers[i].active && (movers[i].freetimeState == FREETIME_RESTING ||
                                         movers[i].freetimeState == FREETIME_SEEKING_REST)) inBed++;
            if (k == 0) drBed3[day] = inBed;
            else {
                drBed13[day] = inBed;
                drMined[day] = DayRhythmWallsLeft();
                drFood[day] = 0;
                for (int i = 0; i < itemHighWaterMark; i++)
                    if (items[i].active && items[i].type == ITEM_DRIED_BERRIES) drFood[day] += items[i].stackCount;
            }
        }
    }
    drPrevHour = timeOfDay;
}

static void DayRhythmPrintStats(void) {
    printf("Day rhythm %s: movers in bed at 03:00 / 13:00, walls and food left at 13:00\n",
           sleepRhythmEnabled ? "ON" : "OFF");
    for (int d = 0; d < DR_MAX_DAYS; d++) {
        if (drMined[d] < 0) continue;
        printf("  day %2d: %d/%d at 03:00, %d/%d at 13:00, walls left %d, food left %d\n",
               d, drBed3[d], drMovers, drBed13[d], drMovers, drMined[d], drFood[d]);
    }
}

// worksplit: two clusters of work far apart (job distance weighting)
// ---------------------------------------------------------------------------
// West cluster A: logs on the ground + a log stockpile (hauling). East cluster B:
// stone walls designated for mining (rocks stay where they fall: no stockpile takes
// rocks). Half the movers start at each cluster. Hauling outranks mining, so with
// jobDistanceWeight 0 the B movers walk west to haul first; with a weight > 0 they
// should mine at home. Stats: distance walked and job starts in the other cluster.
// Default 2+2 movers, 15 jobs per mover; scenarioMovers / scenarioSize scale it up.
static float wsPrevX[MAX_MOVERS], wsPrevY[MAX_MOVERS];
static int wsPrevJob[MAX_MOVERS];
static bool wsHomeWest[MAX_MOVERS];
static double wsWalked, wsWalkedBusy;
static int wsJobStarts, wsCrossStarts, wsAwayStarts, wsLastBusyTick, wsTicks;

static void SetupWorkSplit(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    int perSide = scenarioMovers > 0 ? (scenarioMovers + 1) / 2 : 2;
    int jobs = perSide * 15;
    int cy = gridHeight / 2;
    int ax = gridWidth / 8, bx = gridWidth - gridWidth / 8;   // cluster centres

    // A: log stockpile(s) next to the centre, logs scattered around it
    int spW = 8, spH = 8, made = 0;
    for (int k = 0; made < jobs && k < 16; k++) {
        int sp = CreateStockpile(ax - 4 + (k % 2) * 9, cy - 12 + (k / 2) * 9 + 8, 1, spW, spH);
        if (sp < 0) break;
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) SetStockpileFilter(sp, (ItemType)t, t == ITEM_LOG);
        made += spW * spH * stockpiles[sp].maxStackSize;
    }
    for (int i = 0, tries = 0; i < jobs && tries < jobs * 50; tries++) {
        int x = ax + GetRandomValue(-12, 12), y = cy + GetRandomValue(-12, 12);
        if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
        if (IsPositionInStockpile(x * CELL_SIZE + 4, y * CELL_SIZE + 4, 1, NULL)) continue;
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_LOG);
        i++;
    }

    // B: single stone walls on even cells (every wall has open neighbours), designated
    for (int i = 0, x = bx - 12, y = cy - 6; i < jobs && y < gridHeight; ) {
        if (x >= 0 && x < gridWidth && y >= 0) {
            GRID(x, y, 1) = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_GRANITE);
            DesignateMine(x, y, 1);
            i++;
        }
        x += 2;
        if (x > bx + 12) { x = bx - 12; y += 2; }
    }
    BuildEntrances();
    BuildGraph();

    for (int i = 0; i < perSide; i++) {
        SpawnMoverAt(ax + (i % 5) - 2, cy + 7 + i / 5, 1);
        SpawnMoverAt(bx + (i % 5) - 2, cy + 7 + i / 5, 1);
    }
    for (int i = 0; i < MAX_MOVERS; i++) {
        wsPrevX[i] = i < moverCount ? movers[i].x : 0;
        wsPrevY[i] = i < moverCount ? movers[i].y : 0;
        wsPrevJob[i] = -1;
        wsHomeWest[i] = i < moverCount && movers[i].x / CELL_SIZE < gridWidth / 2;
    }
    wsWalked = wsWalkedBusy = 0;
    wsJobStarts = wsCrossStarts = wsAwayStarts = wsLastBusyTick = wsTicks = 0;
    currentViewZ = 1;
    AddMessage(TextFormat("Work split: %d movers, %d logs to haul west, %d walls to mine east",
                          moverCount, jobs, jobs), GREEN);
}

// Cell where a job starts (what the mover walks to first), -1 if unknown
static int WorkSplitJobStartX(const Job* job) {
    if (job->targetItem >= 0 && items[job->targetItem].active) return (int)(items[job->targetItem].x / CELL_SIZE);
    if (job->targetMineX >= 0) return job->targetMineX;
    return -1;
}

static void WorkSplitTickStats(void) {
    wsTicks++;
    bool busy = false;
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        float dx = m->x - wsPrevX[i], dy = m->y - wsPrevY[i];
        float step = sqrtf(dx * dx + dy * dy) / CELL_SIZE;
        wsWalked += step;
        if (m->currentJobId >= 0) wsWalkedBusy += step;
        wsPrevX[i] = m->x; wsPrevY[i] = m->y;
        if (m->currentJobId >= 0) busy = true;
        if (m->currentJobId >= 0 && m->currentJobId != wsPrevJob[i]) {
            Job* job = GetJob(m->currentJobId);
            int tx = job ? WorkSplitJobStartX(job) : -1;
            if (tx >= 0) {
                wsJobStarts++;
                bool moverWest = m->x / CELL_SIZE < gridWidth / 2, jobWest = tx < gridWidth / 2;
                if (moverWest != jobWest) wsCrossStarts++;
                if (wsHomeWest[i] != jobWest) wsAwayStarts++;
            }
        }
        wsPrevJob[i] = m->currentJobId;
    }
    if (busy) wsLastBusyTick = wsTicks;
}

static void WorkSplitPrintStats(void) {
    int logsLeft = 0, wallsLeft = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].type == ITEM_LOG && items[i].state != ITEM_IN_STOCKPILE) logsLeft++;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                if (DESIGNATION(x, y, z).type == DESIGNATION_MINE) wallsLeft++;
    printf("Job distance weight: %.3f\n", jobDistanceWeight);
    printf("Walked (cells, all movers): %.0f, of that with a job: %.0f\n", wsWalked, wsWalkedBusy);
    printf("Job starts: %d, in the other cluster: %d, away from home cluster: %d\n",
           wsJobStarts, wsCrossStarts, wsAwayStarts);
    printf("Last tick with a job: %d; logs not stored: %d, walls left: %d\n", wsLastBusyTick, logsLeft, wallsLeft);
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

// Natural terrain (island, world, hubs): how much of the dry ground surface movers can reach
// from the main region (the generators' own component report counts every walkable scrap,
// e.g. on tree tops)
static void DryGroundPrintStats(void) {
    Point anchor = GetMainRegionAnchor();
    int dry = 0, reach = 0;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            for (int t = gridDepth - 2; t >= 0; t--) {       // terrain top: natural wall or ramp
                CellType c = GRID(x, y, t);
                if (c != CELL_WALL && !CellIsRamp(c)) continue;
                int z = CellIsRamp(c) ? t : t + 1;
                if (GetWaterLevel(x, y, z) > 0) break;       // under water
                if (GRID(x, y, z) == CELL_TREE_TRUNK || !IsCellWalkableAt(z, y, x)) break;
                dry++;
                if (anchor.x >= 0 && IsReachable(anchor, (Point){x, y, z})) reach++;
                break;
            }
    printf("dry ground: surface %d cells, reachable from the main region %d (%.1f%%)\n",
           dry, reach, dry ? 100.0 * reach / dry : 0.0);
}

// ---------------------------------------------------------------------------
// island-colony: a lived-in island, scripted. 20 settlers at the landing beach with colony
// work around it: trees to chop, a hill to dig into, grass to gather, a stockpile, two
// stone huts with thatch floors. Needs on, nobody dies (sandbox rules). The measuring target
// for big maps (docs/todo/architecture/chunked-world-plan.md): run it for game days with
// --profile, --save the result.
// ---------------------------------------------------------------------------

#define COLONY_SETTLERS 20
#define COLONY_RADIUS   40

// The nearest cell to (cx, cy) at walk level z (or z +-1 when free) reachable from the main area
static Point ColonyCellNear(int cx, int cy, int z, Point anchor) {
    for (int r = 0; r <= 48; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
                for (int zz = z - 1; zz <= z + 1; zz++) {
                    if (zz < 1 || zz >= gridDepth || !IsCellWalkableAt(zz, y, x)) continue;
                    if (GetWaterLevel(x, y, zz) > 0) continue;
                    Point p = {x, y, zz};
                    if (anchor.x < 0 || IsReachable(anchor, p)) return p;
                }
            }
    return (Point){-1, -1, -1};
}

// A w x h area of dry, walkable, empty cells at one level, near (cx, cy), reachable; not
// overlapping 'avoid' (x, y, w, h; w = 0 for none). Top-left in *ox, *oy, *oz.
static bool ColonyFlatArea(int cx, int cy, int w, int h, Point anchor, const int avoid[4],
                           int* ox, int* oy, int* oz) {
    for (int r = 0; r <= COLONY_RADIUS; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int x0 = cx + dx, y0 = cy + dy;
                if (x0 < 1 || y0 < 1 || x0 + w >= gridWidth - 1 || y0 + h >= gridHeight - 1) continue;
                if (avoid[2] > 0 && x0 < avoid[0] + avoid[2] + 2 && x0 + w + 2 > avoid[0] &&
                    y0 < avoid[1] + avoid[3] + 2 && y0 + h + 2 > avoid[1]) continue;
                int z = -1;
                for (int zz = 1; zz < gridDepth && z < 0; zz++)
                    if (IsCellWalkableAt(zz, y0, x0)) z = zz;
                if (z < 0) continue;
                bool ok = true;
                for (int y = y0; y < y0 + h && ok; y++)
                    for (int x = x0; x < x0 + w && ok; x++)
                        ok = IsCellWalkableAt(z, y, x) && GRID(x, y, z) == CELL_AIR &&
                             GetWaterLevel(x, y, z) == 0 && !CellIsRamp(GRID(x, y, z - 1));
                if (!ok || (anchor.x >= 0 && !IsReachable(anchor, (Point){x0, y0, z}))) continue;
                *ox = x0; *oy = y0; *oz = z;
                return true;
            }
    return false;
}

static int colonyStats[6];   // chop, mine, grass, stockpile cells, hut walls, hut floors
static Point colonyLand;     // where the settlers landed
static int colonyRects[3][4];   // stockpile, hut 1, hut 2 (x, y, w, h; w = 0 if not placed)

static void SetupIslandColony(void) {
    // sandbox rules (nobody dies) with the games' needs and pace
    gameMode = GAME_MODE_SANDBOX;
    hungerEnabled = true; thirstEnabled = true; energyEnabled = true; bodyTempEnabled = true;
    bladderEnabled = false; moodEnabled = true; toolRequirementsEnabled = false;
    jobDistanceWeight = 0.2f;
    dayLength = 360.0f;
    balance.baseMoverSpeed = 1200.0f;   // before the settlers spawn: they take it from here
    memset(colonyStats, 0, sizeof colonyStats);
    memset(colonyRects, 0, sizeof colonyRects);
    colonyLand = (Point){-1, -1, -1};

    Point anchor = GetMainRegionAnchor();
    int lx, ly, lz;
    if (!IslandLandingSpot(&lx, &ly, &lz)) { lx = gridWidth / 2; ly = gridHeight / 2; lz = 4; }
    Point land = ColonyCellNear(lx, ly, lz, anchor);
    if (land.x < 0) land = GetRandomWalkableCellReachableFrom(anchor);
    if (land.x < 0) return;
    colonyLand = land;
    int n = scenarioMovers > 0 ? scenarioMovers : COLONY_SETTLERS;
    for (int i = 0; i < n; i++) {
        Point p = ColonyCellNear(land.x + (i % 5) * 2 - 4, land.y + (i / 5) * 2 - 4, land.z, anchor);
        SpawnMoverAt(p.x >= 0 ? p.x : land.x, p.x >= 0 ? p.y : land.y, p.x >= 0 ? p.z : land.z);
    }

    // Stockpile (8x6) and two huts (6x5 stone walls, thatch floor inside), on flat ground
    int none[4] = {0, 0, 0, 0}, sx, sy, sz;
    int stockRect[4] = {0, 0, 0, 0};
    if (ColonyFlatArea(land.x, land.y, 8, 6, anchor, none, &sx, &sy, &sz)) {
        CreateStockpile(sx, sy, sz, 8, 6);
        stockRect[0] = sx; stockRect[1] = sy; stockRect[2] = 8; stockRect[3] = 6;
        memcpy(colonyRects[0], stockRect, sizeof stockRect);
        colonyStats[3] = 48;
    }
    int hutAvoid[4] = {stockRect[0], stockRect[1], stockRect[2], stockRect[3]};
    for (int hut = 0; hut < 2; hut++) {
        int hx, hy, hz;
        if (!ColonyFlatArea(land.x + (hut ? 12 : -12), land.y + 8, 6, 5, anchor, hutAvoid, &hx, &hy, &hz)) continue;
        for (int y = hy; y < hy + 5; y++)
            for (int x = hx; x < hx + 6; x++) {
                bool edge = x == hx || x == hx + 5 || y == hy || y == hy + 4;
                bool door = x == hx + 2 && y == hy + 4;
                if (edge && !door) { if (CreateRecipeBlueprint(x, y, hz, CONSTRUCTION_DRY_STONE_WALL) >= 0) colonyStats[4]++; }
                else if (!edge && CreateRecipeBlueprint(x, y, hz, CONSTRUCTION_THATCH_FLOOR) >= 0) colonyStats[5]++;
            }
        hutAvoid[0] = hx; hutAvoid[1] = hy; hutAvoid[2] = 6; hutAvoid[3] = 5;   // next hut not on this one
        memcpy(colonyRects[1 + hut], hutAvoid, sizeof hutAvoid);
    }

    // Trees to chop, grass to gather (every other cell), within the radius
    for (int y = land.y - COLONY_RADIUS; y <= land.y + COLONY_RADIUS; y++)
        for (int x = land.x - COLONY_RADIUS; x <= land.x + COLONY_RADIUS; x++) {
            if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
            for (int z = 1; z < gridDepth; z++) {
                if (GRID(x, y, z) == CELL_TREE_TRUNK && GRID(x, y, z - 1) != CELL_TREE_TRUNK) {
                    if (colonyStats[0] < 80 && DesignateChop(x, y, z)) colonyStats[0]++;
                    break;
                }
                if (((x + y) & 1) == 0 && colonyStats[2] < 150 && abs(x - land.x) < 20 && abs(y - land.y) < 20 &&
                    IsCellWalkableAt(z, y, x) && DesignateGatherGrass(x, y, z)) { colonyStats[2]++; break; }
            }
        }

    // Dig into the nearest rise: solid ground at the settlers' walk level with an open face
    // beside it, then a 9x9 patch around it at that level and the one above
    int hillX = -1, hillY = -1, best = 1 << 30;
    for (int y = land.y - COLONY_RADIUS; y <= land.y + COLONY_RADIUS; y++)
        for (int x = land.x - COLONY_RADIUS; x <= land.x + COLONY_RADIUS; x++) {
            if (x < 1 || y < 1 || x >= gridWidth - 1 || y >= gridHeight - 1) continue;
            if (GRID(x, y, land.z) != CELL_WALL) continue;   // ground rising above the walk level (the slope cap keeps steps to one level)
            bool face = IsCellWalkableAt(land.z, y, x - 1) || IsCellWalkableAt(land.z, y, x + 1) ||
                        IsCellWalkableAt(land.z, y - 1, x) || IsCellWalkableAt(land.z, y + 1, x);
            int d = abs(x - land.x) + abs(y - land.y);
            if (face && d < best && d > 6) { best = d; hillX = x; hillY = y; }
        }
    if (hillX >= 0)
        for (int z = land.z; z <= land.z + 1 && z < gridDepth; z++)
            for (int y = hillY - 4; y <= hillY + 4; y++)
                for (int x = hillX - 4; x <= hillX + 4; x++) {
                    if (x < 1 || y < 1 || x >= gridWidth - 1 || y >= gridHeight - 1) continue;
                    if (GRID(x, y, z) == CELL_WALL && GetWaterLevel(x, y, z + 1) == 0 && DesignateMine(x, y, z)) colonyStats[1]++;
                }

    currentViewZ = land.z;
    offset.x = GetScreenWidth() / 2.0f - land.x * CELL_SIZE * zoom;
    offset.y = GetScreenHeight() / 2.0f - land.y * CELL_SIZE * zoom;
    AddMessage(TextFormat("Island colony: %d settlers at (%d,%d,z%d); chop %d, mine %d, grass %d, stockpile %d, hut walls %d, floors %d",
               moverCount, land.x, land.y, land.z, colonyStats[0], colonyStats[1], colonyStats[2],
               colonyStats[3], colonyStats[4], colonyStats[5]), GREEN);
}

static void IslandColonyPrintStats(void) {
    int chop = 0, mine = 0, grass = 0, bpLeft = 0, alive = 0;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                const Designation* d = GetDesignation(x, y, z);
                if (!d) continue;
                if (d->type == DESIGNATION_CHOP) chop++;
                else if (d->type == DESIGNATION_MINE) mine++;
                else if (d->type == DESIGNATION_GATHER_GRASS) grass++;
            }
    for (int i = 0; i < MAX_BLUEPRINTS; i++) if (blueprints[i].active) bpLeft++;
    for (int i = 0; i < moverCount; i++) if (movers[i].active) alive++;
    printf("island-colony: designated chop %d, mine %d, grass %d, hut walls %d + floors %d\n",
           colonyStats[0], colonyStats[1], colonyStats[2], colonyStats[4], colonyStats[5]);
    printf("island-colony: still open: chop %d, mine %d, grass %d, blueprints %d; items %d; movers %d active\n",
           chop, mine, grass, bpLeft, itemCount, alive);
    DryGroundPrintStats();
}

// ---------------------------------------------------------------------------
// island-winter: island-colony in winter with a fire pit and torches. It measures the
// simulation layers the summer colony never touched (temperature, light, snow, fire, smoke);
// docs/todo/architecture/chunked-world-plan.md "A lived-in island".
// ---------------------------------------------------------------------------

static int winterTorches;
static Point winterFire;

// No fuel on the cell or its 8 neighbours at level z: a fire here can't spread
static bool WinterFireSafe(int x, int y, int z) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) return false;
            if (GetFuelAt(nx, ny, z) > 0 || GetFuelAt(nx, ny, z - 1) > 0) return false;
        }
    return IsCellWalkableAt(z, y, x) && GRID(x, y, z) == CELL_AIR && GetWaterLevel(x, y, z) == 0;
}

static void SetupIslandWinter(void) {
    SetupIslandColony();
    if (colonyLand.x < 0) return;
    Point land = colonyLand;

    // Winter from day one: the first day of the fourth season, snowing
    InitWeather();
    dayNumber = 3 * daysPerSeason + 1;
    weatherState.current = weatherState.previous = WEATHER_SNOW;
    weatherState.transitionTimer = weatherState.transitionDuration = GameHoursToGameSeconds(48.0f);
    weatherState.intensity = 1.0f;

    // A fire pit that burns for good, on fuel-free ground near the landing spot (sand, stone)
    winterFire = (Point){-1, -1, -1};
    for (int r = 3; r <= 30 && winterFire.x < 0; r++)
        for (int dy = -r; dy <= r && winterFire.x < 0; dy++)
            for (int dx = -r; dx <= r && winterFire.x < 0; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue;
                int x = land.x + dx, y = land.y + dy;
                bool inRect = false;
                for (int k = 0; k < 3; k++) {
                    const int* rc = colonyRects[k];
                    if (rc[2] > 0 && x >= rc[0] - 1 && x <= rc[0] + rc[2] && y >= rc[1] - 1 && y <= rc[1] + rc[3]) inRect = true;
                }
                if (!inRect && WinterFireSafe(x, y, land.z)) winterFire = (Point){x, y, land.z};
            }
    if (winterFire.x >= 0) SetFireSource(winterFire.x, winterFire.y, winterFire.z, true);

    // Torches at the corners of the stockpile and the huts
    winterTorches = 0;
    for (int k = 0; k < 3; k++) {
        const int* rc = colonyRects[k];
        if (rc[2] <= 0) continue;
        int cx[4] = {rc[0] - 1, rc[0] + rc[2], rc[0] - 1, rc[0] + rc[2]};
        int cy[4] = {rc[1] - 1, rc[1] - 1, rc[1] + rc[3], rc[1] + rc[3]};
        for (int c = 0; c < 4; c++) {
            Point p = ColonyCellNear(cx[c], cy[c], land.z, (Point){-1, -1, -1});
            if (p.x >= 0 && AddLightSource(p.x, p.y, p.z, 255, 180, 90, 8) >= 0) winterTorches++;
        }
    }
    InvalidateLighting();
    AddMessage(TextFormat("Island winter: %s, day %d, fire pit at (%d,%d,z%d), %d torches",
               GetSeasonName(GetCurrentSeason()), dayNumber, winterFire.x, winterFire.y, winterFire.z,
               winterTorches), GREEN);
}

static void IslandWinterPrintStats(void) {
    IslandColonyPrintStats();
    int snow = 0, fire = 0, warm = 0;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                if (GetSnowLevel(x, y, z) > 0) snow++;
                if (HasFire(x, y, z)) fire++;
                if (GetTemperature(x, y, z) > GetAmbientTemperature(z) + 2) warm++;
            }
    printf("island-winter: %s day %d, %d torches, fire pit at (%d,%d,z%d); cells with snow %d, on fire %d, warmed >2C %d\n",
           GetSeasonName(GetCurrentSeason()), dayNumber, winterTorches, winterFire.x, winterFire.y, winterFire.z,
           snow, fire, warm);
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

// ---------------------------------------------------------------------------
// metro: four coloured lines (each connected piece of track is one line)
// ---------------------------------------------------------------------------

// A circle line round the city (two trains), an east-west shuttle through the
// middle, and north-south shuttles above and below it. Colours follow the order
// the lines are found (top row first): circle Red, north Blue, east-west Green,
// south Yellow. Lines meet at interchanges: platforms of two lines a few cells
// apart (circle north + north shuttle, centre + both shuttles, circle south +
// south shuttle). A mover picks one
// line per ride; after getting off it plans again and may take another line.
// Each station has a district a few cells off its platform (stations must be within
// TRANSPORT_STATION_RADIUS of a mover and its goal to be considered).

// Level the Lay* helpers build on (1 = the flat map's ground; subway sets its tunnel)
static int layZ = 1;

// Lay track along a polyline of axis-aligned legs; closed = back to the first point
static void LayTrack(const Point* pts, int n, bool closed) {
    int legs = closed ? n : n - 1;
    for (int k = 0; k < legs; k++) {
        Point a = pts[k], b = pts[(k + 1) % n];
        int sx = (b.x > a.x) - (b.x < a.x), sy = (b.y > a.y) - (b.y < a.y);
        uint8_t fwd = sx > 0 ? TRACK_E : sx < 0 ? TRACK_W : sy > 0 ? TRACK_S : TRACK_N;
        uint8_t back = sx > 0 ? TRACK_W : sx < 0 ? TRACK_E : sy > 0 ? TRACK_N : TRACK_S;
        for (int x = a.x, y = a.y; ; x += sx, y += sy) {
            GRID(x, y, layZ) = CELL_TRACK;
            if (x != b.x || y != b.y) TRACK_CONN(x, y, layZ) |= fwd;
            if (x != a.x || y != a.y) TRACK_CONN(x, y, layZ) |= back;
            if (x == b.x && y == b.y) break;
        }
    }
}

static void LayPlatform(int x, int y, int len, bool alongX) {
    for (int i = 0; i < len; i++)
        GRID(alongX ? x + i : x, alongX ? y : y + i, layZ) = CELL_PLATFORM;
}

#define METRO_DISTRICTS MAX_STATIONS
static Point metroDistricts[METRO_DISTRICTS];
static int metroDistrictCount;

static bool MetroGoalPicker(int moverIdx, Point* goal) {
    const Mover* m = &movers[moverIdx];
    int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE);
    for (int tries = 0; tries < 8; tries++) {
        Point d = metroDistricts[GetRandomValue(0, metroDistrictCount - 1)];
        if (abs(d.x - cx) + abs(d.y - cy) < 30) continue;   // another district
        *goal = (Point){d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), 1};
        return true;
    }
    return false;
}

static void MetroResetStats(void);

static void SetupMetro(void) {
    const int W = gridWidth, H = gridHeight, cx = W / 2, cy = H / 2;
    // Circle line
    const int rx0 = 20, ry0 = 20, rx1 = W - 21, ry1 = H - 21;
    Point red[4] = {{rx0, ry0, 1}, {rx1, ry0, 1}, {rx1, ry1, 1}, {rx0, ry1, 1}};
    LayTrack(red, 4, true);
    LayPlatform(cx - 3, ry0 + 1, 6, true);     // north (inside the ring)
    LayPlatform(rx1 - 1, cy - 3, 6, false);    // east
    LayPlatform(cx - 3, ry1 - 1, 6, true);     // south
    LayPlatform(rx0 + 1, cy - 3, 6, false);    // west
    // East-west shuttle through the middle
    Point blue[2] = {{rx0 + 10, cy, 1}, {rx1 - 10, cy, 1}};
    LayTrack(blue, 2, false);
    LayPlatform(rx0 + 10, cy - 1, 6, true);    // west end
    LayPlatform(cx - 3, cy - 1, 6, true);      // centre
    LayPlatform(rx1 - 15, cy - 1, 6, true);    // east end
    // North shuttle: from the Red north stop down to the Blue centre stop
    Point green[2] = {{cx, ry0 + 7, 1}, {cx, cy - 7, 1}};
    LayTrack(green, 2, false);
    LayPlatform(cx + 1, ry0 + 7, 6, false);    // top (Red north interchange)
    LayPlatform(cx + 1, cy - 12, 6, false);    // bottom (Blue centre interchange)
    // South shuttle: from the Blue centre stop down to the Red south stop
    Point yellow[2] = {{cx, cy + 7, 1}, {cx, ry1 - 7, 1}};
    LayTrack(yellow, 2, false);
    LayPlatform(cx + 1, cy + 7, 6, false);     // top (Blue centre interchange)
    LayPlatform(cx + 1, ry1 - 12, 6, false);   // bottom (Red south interchange)
    RebuildStations();

    SpawnTrainHeading(rx0 + 5, ry0, 1, 4, 1, 0);    // circle, clockwise
    SpawnTrainHeading(rx1 - 5, ry1, 1, 4, -1, 0);   // circle, opposite side of the ring
    SpawnTrainHeading(cx + 10, cy, 1, 3, 1, 0);      // east-west
    SpawnTrainHeading(cx, ry0 + 12, 1, 2, 0, 1);     // north
    SpawnTrainHeading(cx, cy + 12, 1, 2, 0, 1);      // south
    BuildEntrances();
    BuildGraph();

    // A district per station: 8 cells off the middle of its platform, away from the track
    metroDistrictCount = 0;
    for (int i = 0; i < stationCount; i++) {
        const TrainStation* st = &stations[i];
        int awayX = st->platX - st->trackX, awayY = st->platY - st->trackY;
        int mid = st->platformCellCount / 2;
        int dx = st->platformCells[mid][0] + awayX * 8, dy = st->platformCells[mid][1] + awayY * 8;
        if (dx < 6) dx = 6;
        if (dy < 6) dy = 6;
        if (dx > W - 7) dx = W - 7;
        if (dy > H - 7) dy = H - 7;
        metroDistricts[metroDistrictCount++] = (Point){dx, dy, 1};
    }
    MetroResetStats();
    moverGoalPicker = MetroGoalPicker;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 48;
    for (int i = 0; i < n; i++) {
        Point d = metroDistricts[i % metroDistrictCount];
        SpawnMoverAt(d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), 1);
    }
    currentViewZ = 1;
    AddMessage(TextFormat("Metro: %d lines, %d stations, %d movers", TrackLineCount(), stationCount, n), GREEN);
}

// Transfers: a mover boards a train of another line within 60 s of leaving one
static int metroLastLine[MAX_MOVERS];
static double metroLastRideEnd[MAX_MOVERS];
static bool metroWasRiding[MAX_MOVERS];
static int metroTransfers, metroRides;
// Journeys: rides joined by changes of line (within 60 s), counted by number of rides
#define METRO_JOURNEY_MAX 8
static int metroChain[MAX_MOVERS];
static int metroJourneys[METRO_JOURNEY_MAX + 1];

static void MetroResetStats(void) {
    for (int i = 0; i < MAX_MOVERS; i++) {
        metroLastLine[i] = -1; metroLastRideEnd[i] = -1e9; metroWasRiding[i] = false; metroChain[i] = 0;
    }
    metroTransfers = metroRides = 0;
    memset(metroJourneys, 0, sizeof(metroJourneys));
}

static void MetroEndJourney(int i) {
    if (metroChain[i] > 0) metroJourneys[metroChain[i] < METRO_JOURNEY_MAX ? metroChain[i] : METRO_JOURNEY_MAX]++;
    metroChain[i] = 0;
}

static void MetroTickStats(void) {
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        bool riding = m->active && m->transportState == TRANSPORT_RIDING;
        if (riding && !metroWasRiding[i]) {
            int line = TrainLine(m->transportTrainIdx);
            metroRides++;
            if (metroLastLine[i] >= 0 && line != metroLastLine[i] && gameTime - metroLastRideEnd[i] < 60.0) {
                metroTransfers++;
                metroChain[i]++;
            } else {
                MetroEndJourney(i);
                metroChain[i] = 1;
            }
            metroLastLine[i] = line;
        } else if (!riding && metroWasRiding[i]) {
            metroLastRideEnd[i] = gameTime;
        }
        metroWasRiding[i] = riding;
    }
}

static void MetroPrintStats(void) {
    printf("Lines: %d\n", TrackLineCount());
    for (int line = 0; line < TrackLineCount(); line++) {
        int st = 0, boarded = 0, tr = 0;
        for (int i = 0; i < stationCount; i++)
            if (StationLine(i) == line) { st++; boarded += StationBoardings(i); }
        for (int t = 0; t < MAX_TRAINS; t++)
            if (TrainLine(t) == line) tr++;
        printf("  %-7s line: %d stations, %d trains, %d boarded\n", TrackLineName(line), st, tr, boarded);
        for (int i = 0; i < stationCount; i++)
            if (StationLine(i) == line)
                printf("    station %d: track (%d,%d), %d boarded\n", i, stations[i].trackX, stations[i].trackY,
                       StationBoardings(i));
    }
    printf("Train trips chosen: %d (faster than walking by estimate: %d)\n", trainDecisions, trainDecisionsFaster);
    printf("Rides: %d, transfers (another line within 60 s): %d\n", metroRides, metroTransfers);
    int journeys[METRO_JOURNEY_MAX + 1];
    memcpy(journeys, metroJourneys, sizeof(journeys));
    for (int i = 0; i < moverCount; i++)   // journeys still going count too
        if (metroChain[i] > 0) journeys[metroChain[i] < METRO_JOURNEY_MAX ? metroChain[i] : METRO_JOURNEY_MAX]++;
    printf("Journeys by rides:");
    for (int k = 1; k <= METRO_JOURNEY_MAX; k++) if (journeys[k]) printf("  %d%s: %d", k, k == METRO_JOURNEY_MAX ? "+" : "", journeys[k]);
    printf("\nTrips planned by changes of line (each stop plans again):");
    for (int k = 0; k <= TRAIN_PLAN_CHANGES_MAX; k++) if (trainPlanChanges[k]) printf("  %d%s: %d", k, k == TRAIN_PLAN_CHANGES_MAX ? "+" : "", trainPlanChanges[k]);
    printf("\n");
}

static bool MetroCheck(char* why, int whyLen) {
    if (TrackLineCount() != 4) {
        snprintf(why, (size_t)whyLen, "expected 4 lines, found %d", TrackLineCount());
        return false;
    }
    for (int i = 0; i < stationCount; i++)
        if (StationLine(i) < 0) {
            snprintf(why, (size_t)whyLen, "station %d is on no line", i);
            return false;
        }
    for (int t = 0; t < MAX_TRAINS; t++)
        if (trains[t].active && TrainLine(t) < 0) {
            snprintf(why, (size_t)whyLen, "train %d is on no line", t);
            return false;
        }
    return true;
}

// ---------------------------------------------------------------------------
// transfer: a T of three lines, trips that need a change of line
// ---------------------------------------------------------------------------

// A long east-west line through the middle, and a north and a south line that end
// next to its centre stop (lines can't cross on one level: crossing track joins
// them into one line). Each line is a narrow loop (one track per direction, like
// train-double) with several trains, so waits are short. Districts at the four outer
// ends: east-west is a direct ride, west to north or north to south needs a change at
// the centre. Trains run at 50 cells/s here (a metro, ~8x walking): at the default
// 25 two rides with their waits and doors lose to walking the diagonal.
// Compare --set useTrainTransfers 0.

// A narrow loop: two parallel tracks `gap` apart joined at the ends; trains run
// clockwise. alongX: from (a, c) to (b, c + gap); else from (c, a) to (c + gap, b).
static void LayLoopLine(int a, int b, int c, int gap, bool alongX) {
    Point p[4];
    if (alongX) { p[0] = (Point){a, c, layZ}; p[1] = (Point){b, c, layZ}; p[2] = (Point){b, c + gap, layZ}; p[3] = (Point){a, c + gap, layZ}; }
    else        { p[0] = (Point){c + gap, a, layZ}; p[1] = (Point){c + gap, b, layZ}; p[2] = (Point){c, b, layZ}; p[3] = (Point){c, a, layZ}; }
    LayTrack(p, 4, true);
}

// A stop on a loop line: a platform on both outer sides at position `at`
static void LayLoopStop(int at, int c, int gap, bool alongX) {
    if (alongX) { LayPlatform(at - 3, c - 1, 6, true); LayPlatform(at - 3, c + gap + 1, 6, true); }
    else        { LayPlatform(c - 1, at - 3, 6, false); LayPlatform(c + gap + 1, at - 3, 6, false); }
}

#define TRANSFER_DISTRICTS 4
static Point transferDistricts[TRANSFER_DISTRICTS];

static bool TransferGoalPicker(int moverIdx, Point* goal) {
    const Mover* m = &movers[moverIdx];
    int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE);
    for (int tries = 0; tries < 8; tries++) {
        Point d = transferDistricts[GetRandomValue(0, TRANSFER_DISTRICTS - 1)];
        if (abs(d.x - cx) + abs(d.y - cy) < 60) continue;   // another district
        *goal = (Point){d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), d.z};
        return true;
    }
    return false;
}

static void SetupTransfer(void) {
    const int W = gridWidth, H = gridHeight, cx = W / 2, cy = H / 2, gap = 3;
    const int ex0 = 20, ex1 = W - 21, ey = cy - 1;           // east-west line: rows ey, ey+gap
    const int nx = cx + 8;                                    // north/south lines: columns nx, nx+gap
    const int ny0 = 20, ny1 = cy - 10, sy0 = cy + 12, sy1 = H - 21;
    LayLoopLine(ex0, ex1, ey, gap, true);
    LayLoopStop(ex0 + 20, ey, gap, true);
    LayLoopStop(cx, ey, gap, true);
    LayLoopStop(ex1 - 20, ey, gap, true);
    LayLoopLine(ny0, ny1, nx, gap, false);
    LayLoopStop(ny0 + 20, nx, gap, false);
    LayLoopStop(ny1 - 5, nx, gap, false);                     // change to the east-west line
    LayLoopLine(sy0, sy1, nx, gap, false);
    LayLoopStop(sy0 + 5, nx, gap, false);                     // change to the east-west line
    LayLoopStop(sy1 - 20, nx, gap, false);
    RebuildStations();

    for (int k = 0; k < 2; k++) {   // spread out round each loop, all clockwise
        SpawnTrainHeading(ex0 + 30 + k * (ex1 - ex0) / 2, ey, 1, 3, 1, 0);
        SpawnTrainHeading(ex1 - 30 - k * (ex1 - ex0) / 2, ey + gap, 1, 3, -1, 0);
    }
    SpawnTrainHeading(nx + gap, ny0 + 10, 1, 3, 0, 1);
    SpawnTrainHeading(nx, ny1 - 10, 1, 3, 0, -1);
    SpawnTrainHeading(nx + gap, sy0 + 10, 1, 3, 0, 1);
    SpawnTrainHeading(nx, sy1 - 10, 1, 3, 0, -1);
    BuildEntrances();
    BuildGraph();

    if (trainSpeed == TRAIN_DEFAULT_SPEED) trainSpeed = 50.0f;   // unless --set trainSpeed chose one
    transferDistricts[0] = (Point){ex0 + 20, ey - 12, 1};         // west
    transferDistricts[1] = (Point){ex1 - 20, ey + gap + 12, 1};   // east
    transferDistricts[2] = (Point){nx + gap + 12, ny0 + 20, 1};   // north
    transferDistricts[3] = (Point){nx - 12, sy1 - 20, 1};         // south
    MetroResetStats();
    moverGoalPicker = TransferGoalPicker;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 40;
    for (int i = 0; i < n; i++) {
        Point d = transferDistricts[i % TRANSFER_DISTRICTS];
        SpawnMoverAt(d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), 1);
    }
    currentViewZ = 1;
    AddMessage(TextFormat("Transfer: %d lines, %d stations, %d movers", TrackLineCount(), stationCount, n), GREEN);
}

static void TransferPrintStats(void) {
    MetroPrintStats();
    printf("Trips planned with a change of line: %d (useTrainTransfers %s)\n", trainTransferPlans,
           useTrainTransfers ? "on" : "off");
}

static bool TransferCheck(char* why, int whyLen) {
    if (TrackLineCount() != 3) {
        snprintf(why, (size_t)whyLen, "expected 3 lines, found %d", TrackLineCount());
        return false;
    }
    if (useTrainTransfers && trainTransferPlans == 0) {
        snprintf(why, (size_t)whyLen, "no trip planned a change of line (%d rides)", metroRides);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// transit-chain: six lines in a staircase, end to end needs five changes
// ---------------------------------------------------------------------------

// Six narrow loop lines (two trains each) going right and down in turn, each ending
// next to the next one's start. Districts at every line's middle stop and at both
// ends of the chain: neighbouring districts are one ride, the two ends five changes.
// The trip choice searches the transit graph (any number of changes); the mover
// rides to its first change and plans again there. Trains at 50 cells/s.
#define CHAIN_LINES 6
#define CHAIN_DISTRICTS (CHAIN_LINES + 2)
static Point chainDistricts[CHAIN_DISTRICTS];

static bool ChainGoalPicker(int moverIdx, Point* goal) {
    const Mover* m = &movers[moverIdx];
    int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE);
    for (int tries = 0; tries < 8; tries++) {
        Point d = chainDistricts[GetRandomValue(0, CHAIN_DISTRICTS - 1)];
        if (abs(d.x - cx) + abs(d.y - cy) < 60) continue;
        *goal = (Point){d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), 1};
        return true;
    }
    return false;
}

static void SetupTransitChain(void) {
    const int L = 200, gap = 3;
    int xs = 20, yh = 20, xv = 0, ys = 0, nd = 0;
    for (int i = 0; i < CHAIN_LINES; i++) {
        if (i % 2 == 0) {   // east-west: rows yh, yh+gap from xs to xs+L
            LayLoopLine(xs, xs + L, yh, gap, true);
            LayLoopStop(xs + 6, yh, gap, true);
            LayLoopStop(xs + L / 2, yh, gap, true);
            LayLoopStop(xs + L - 6, yh, gap, true);
            if (i == 0) chainDistricts[nd++] = (Point){xs + 6, yh + gap + 14, 1};
            chainDistricts[nd++] = (Point){xs + L / 2, yh - 12 > 6 ? yh - 12 : yh + gap + 14, 1};
            xv = xs + L + 4;
            ys = yh + 8;
        } else {            // north-south: columns xv, xv+gap from ys to ys+L
            LayLoopLine(ys, ys + L, xv, gap, false);
            LayLoopStop(ys + 6, xv, gap, false);
            LayLoopStop(ys + L / 2, xv, gap, false);
            LayLoopStop(ys + L - 6, xv, gap, false);
            chainDistricts[nd++] = (Point){xv + gap + 14, ys + L / 2, 1};
            yh = ys + L + 4;
            xs = xv + 8;
        }
    }
    // the far end: next to the last line's end stop
    chainDistricts[nd++] = (CHAIN_LINES % 2 == 0) ? (Point){xv - 14, ys + L - 6, 1} : (Point){xs - 14, yh, 1};
    RebuildStations();

    // two trains per line, opposite sides of the loop, clockwise
    xs = 20; yh = 20;
    for (int i = 0; i < CHAIN_LINES; i++) {
        if (i % 2 == 0) {
            SpawnTrainHeading(xs + 10, yh, 1, 2, 1, 0);
            SpawnTrainHeading(xs + L - 10, yh + gap, 1, 2, -1, 0);
            xv = xs + L + 4; ys = yh + 8;
        } else {
            SpawnTrainHeading(xv + gap, ys + 10, 1, 2, 0, 1);
            SpawnTrainHeading(xv, ys + L - 10, 1, 2, 0, -1);
            yh = ys + L + 4; xs = xv + 8;
        }
    }
    BuildEntrances();
    BuildGraph();

    if (trainSpeed == TRAIN_DEFAULT_SPEED) trainSpeed = 50.0f;   // unless --set trainSpeed chose one
    MetroResetStats();
    moverGoalPicker = ChainGoalPicker;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 48;
    for (int i = 0; i < n; i++) {
        Point d = chainDistricts[i % CHAIN_DISTRICTS];
        SpawnMoverAt(d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), 1);
    }
    currentViewZ = 1;
    AddMessage(TextFormat("Transit chain: %d lines, %d stations, %d movers", TrackLineCount(), stationCount, n), GREEN);
}

static bool TransitChainCheck(char* why, int whyLen) {
    if (TrackLineCount() != CHAIN_LINES) {
        snprintf(why, (size_t)whyLen, "expected %d lines, found %d", CHAIN_LINES, TrackLineCount());
        return false;
    }
    int many = 0;
    for (int k = 3; k <= TRAIN_PLAN_CHANGES_MAX; k++) many += trainPlanChanges[k];
    if (useTrainTransfers && many == 0) {
        snprintf(why, (size_t)whyLen, "no trip planned with 3 or more changes of line");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// subway: the transfer network underground
// ---------------------------------------------------------------------------

// Solid ground five levels deep (street at z=5), the same T of three loop lines as
// "transfer" but in a tunnel at z=2 (two dirt levels above it). Each stop has a hall
// (platforms on both sides, the space between the tracks dug out so movers cross),
// one ramp of three steps up to the street; the centre stop of the east-west line
// and the ends of the north and south lines share passages (change there without
// going up). Movers live on the street at the four ends. Trip choice costs the walk
// down to a platform with the pathfinder. Compare with "transfer" (same network on
// the street). See the tunnel with viewZ 2.
#define SUBWAY_STREET 5
#define SUBWAY_TUNNEL 2
static Point subwayDistricts[4];

static void CarveAir(int x0, int y0, int x1, int y1, int z) {   // only dirt, inclusive
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && y >= 0 && x < gridWidth && y < gridHeight && GRID(x, y, z) == CELL_WALL)
                GRID(x, y, z) = CELL_AIR;
}

// Ramps from (x, y, zFrom) up to zTo, one level per cell, rising towards (dx, dy).
// Returns the cell at zTo where it comes out.
static Point RampUp(int x, int y, int dx, int dy, int zFrom, int zTo) {
    CellType ramp = dy < 0 ? CELL_RAMP_N : dy > 0 ? CELL_RAMP_S : dx > 0 ? CELL_RAMP_E : CELL_RAMP_W;
    for (int k = 0; k < zTo - zFrom; k++) {
        GRID(x + k * dx, y + k * dy, zFrom + k) = ramp;
        SetWallMaterial(x + k * dx, y + k * dy, zFrom + k, MAT_DIRT);
    }
    int k = zTo - zFrom;
    return (Point){x + k * dx, y + k * dy, zTo};
}

static Point RampToStreet(int x, int y, int dx, int dy) {
    return RampUp(x, y, dx, dy, SUBWAY_TUNNEL, SUBWAY_STREET);
}

// A stop's hall at level z: platforms, the cells between the two tracks and two
// cells beyond, with lamps along both outer walls
static void SubwayHallAt(int at, int c, int gap, bool alongX, int z) {
    if (alongX) CarveAir(at - 4, c - 3, at + 3, c + gap + 3, z);
    else        CarveAir(c - 3, at - 4, c + gap + 3, at + 3, z);
    for (int k = -4; k <= 3; k += 7) {
        int a = at + k;
        if (alongX) { AddLightSource(a, c - 3, z, 255, 235, 190, 12);
                      AddLightSource(a, c + gap + 3, z, 255, 235, 190, 12); }
        else        { AddLightSource(c - 3, a, z, 255, 235, 190, 12);
                      AddLightSource(c + gap + 3, a, z, 255, 235, 190, 12); }
    }
}

static void SubwayHall(int at, int c, int gap, bool alongX) { SubwayHallAt(at, c, gap, alongX, SUBWAY_TUNNEL); }

static void LightPassageAt(int x0, int y0, int x1, int y1, int z) {
    for (int y = y0 + 2; y <= y1; y += 7)
        for (int x = x0 + 2; x <= x1; x += 7)
            if (GRID(x, y, z) == CELL_AIR) AddLightSource(x, y, z, 255, 235, 190, 10);
}

static void LightPassage(int x0, int y0, int x1, int y1) { LightPassageAt(x0, y0, x1, y1, SUBWAY_TUNNEL); }

// Solid dirt from z=1 up to below the street, grass on top
static void SolidGroundBelow(int streetZ) {
    for (int z = 1; z < streetZ; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                GRID(x, y, z) = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_DIRT);
                SetWallNatural(x, y, z);
            }
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) SetVegetation(x, y, streetZ - 1, VEG_GRASS_TALLER);
}

static void SetupSubway(void) {
    const int W = gridWidth, H = gridHeight, cx = W / 2, cy = H / 2, gap = 3;
    SolidGroundBelow(SUBWAY_STREET);

    layZ = SUBWAY_TUNNEL;
    const int ex0 = 20, ex1 = W - 21, ey = cy - 1;
    const int nx = cx + 8, ny0 = 20, ny1 = cy - 10, sy0 = cy + 12, sy1 = H - 21;
    LayLoopLine(ex0, ex1, ey, gap, true);
    LayLoopLine(ny0, ny1, nx, gap, false);
    LayLoopLine(sy0, sy1, nx, gap, false);
    const int ew[3] = {ex0 + 20, cx, ex1 - 20};
    for (int k = 0; k < 3; k++) { LayLoopStop(ew[k], ey, gap, true); SubwayHall(ew[k], ey, gap, true); }
    LayLoopStop(ny0 + 20, nx, gap, false); SubwayHall(ny0 + 20, nx, gap, false);
    LayLoopStop(ny1 - 5, nx, gap, false);  SubwayHall(ny1 - 5, nx, gap, false);
    LayLoopStop(sy0 + 5, nx, gap, false);  SubwayHall(sy0 + 5, nx, gap, false);
    LayLoopStop(sy1 - 20, nx, gap, false); SubwayHall(sy1 - 20, nx, gap, false);
    layZ = 1;
    // Interchange passages: centre stop <-> the north line's last stop and the south line's first
    CarveAir(cx - 4, ny1 - 9, nx - 2, ey - 4, SUBWAY_TUNNEL);
    CarveAir(cx - 4, ey + gap + 4, nx - 2, sy0 + 9, SUBWAY_TUNNEL);
    LightPassage(cx - 4, ny1 - 9, nx - 2, ey - 4);
    LightPassage(cx - 4, ey + gap + 4, nx - 2, sy0 + 9);
    // Ramps up to the street: outer stops away from the tracks, centre from its passage
    Point west  = RampToStreet(ew[0], ey - 4, 0, -1);
    Point east  = RampToStreet(ew[2], ey + gap + 4, 0, 1);
    RampToStreet(cx - 5, ny1 - 6, -1, 0);
    Point north = RampToStreet(nx + gap + 4, ny0 + 20, 1, 0);
    Point south = RampToStreet(nx - 4, sy1 - 20, -1, 0);
    RebuildStations();

    for (int k = 0; k < 2; k++) {
        SpawnTrainHeading(ex0 + 30 + k * (ex1 - ex0) / 2, ey, SUBWAY_TUNNEL, 3, 1, 0);
        SpawnTrainHeading(ex1 - 30 - k * (ex1 - ex0) / 2, ey + gap, SUBWAY_TUNNEL, 3, -1, 0);
    }
    SpawnTrainHeading(nx + gap, ny0 + 10, SUBWAY_TUNNEL, 3, 0, 1);
    SpawnTrainHeading(nx, ny1 - 10, SUBWAY_TUNNEL, 3, 0, -1);
    SpawnTrainHeading(nx + gap, sy0 + 10, SUBWAY_TUNNEL, 3, 0, 1);
    SpawnTrainHeading(nx, sy1 - 10, SUBWAY_TUNNEL, 3, 0, -1);
    BuildEntrances();
    BuildGraph();

    if (trainSpeed == TRAIN_DEFAULT_SPEED) trainSpeed = 50.0f;   // unless --set trainSpeed chose one
    subwayDistricts[0] = (Point){west.x, west.y - 12, SUBWAY_STREET};
    subwayDistricts[1] = (Point){east.x, east.y + 12, SUBWAY_STREET};
    subwayDistricts[2] = (Point){north.x + 12, north.y, SUBWAY_STREET};
    subwayDistricts[3] = (Point){south.x - 12, south.y, SUBWAY_STREET};
    for (int i = 0; i < 4; i++) transferDistricts[i] = subwayDistricts[i];   // same goal picker
    MetroResetStats();
    moverGoalPicker = TransferGoalPicker;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 40;
    for (int i = 0; i < n; i++) {
        Point d = subwayDistricts[i % 4];
        SpawnMoverAt(d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), SUBWAY_STREET);
    }
    currentViewZ = SUBWAY_STREET;
    AddMessage(TextFormat("Subway: %d lines, %d stations underground (viewZ %d), %d movers on the street",
                          TrackLineCount(), stationCount, SUBWAY_TUNNEL, n), GREEN);
}

// ---------------------------------------------------------------------------
// subway-deep: two lines at different depths that cross, change by ramps
// ---------------------------------------------------------------------------

// Street at z=6. An east-west line at z=3 and a north-south line at z=1 that runs
// under it. Their centre stops sit near the crossing: from the deep line's hall a
// passage and two ramps lead up to the shallow line's hall (a change across two
// levels). Outer stops have ramps to the street (3 steps from the shallow line, 5
// from the deep one). Movers at the four ends: east-west or north-south is one line,
// west to north and the like needs the change.
#define DEEP_STREET 6
#define DEEP_EW 3
#define DEEP_NS 1

static void SetupSubwayDeep(void) {
    const int W = gridWidth, H = gridHeight, cx = W / 2, cy = H / 2, gap = 3;
    SolidGroundBelow(DEEP_STREET);
    const int ex0 = 20, ex1 = W - 21, ey = cy - 1;     // shallow: rows ey, ey+gap
    const int nx = cx + 20, ny0 = 20, ny1 = H - 21;    // deep: columns nx, nx+gap
    const int eCentre = cx - 14, nCentre = cy + 14;
    layZ = DEEP_EW;
    LayLoopLine(ex0, ex1, ey, gap, true);
    const int ew[3] = {ex0 + 20, eCentre, ex1 - 20};
    for (int k = 0; k < 3; k++) { LayLoopStop(ew[k], ey, gap, true); SubwayHallAt(ew[k], ey, gap, true, DEEP_EW); }
    layZ = DEEP_NS;
    LayLoopLine(ny0, ny1, nx, gap, false);
    const int ns[3] = {ny0 + 20, nCentre, ny1 - 20};
    for (int k = 0; k < 3; k++) { LayLoopStop(ns[k], nx, gap, false); SubwayHallAt(ns[k], nx, gap, false, DEEP_NS); }
    layZ = 1;

    // The change: deep hall -> passage west at z=1 -> two ramps north -> passage at z=3
    // -> the shallow centre hall
    const int rx = eCentre + 6;
    CarveAir(rx, nCentre - 2, nx - 4, nCentre + 1, DEEP_NS);
    LightPassageAt(rx, nCentre - 2, nx - 4, nCentre + 1, DEEP_NS);
    Point top = RampUp(rx, nCentre - 3, 0, -1, DEEP_NS, DEEP_EW);
    CarveAir(eCentre + 2, ey + gap + 4, rx + 1, top.y, DEEP_EW);
    LightPassageAt(eCentre + 2, ey + gap + 4, rx + 1, top.y, DEEP_EW);

    // To the street
    Point west  = RampUp(ew[0], ey - 4, 0, -1, DEEP_EW, DEEP_STREET);
    Point east  = RampUp(ew[2], ey + gap + 4, 0, 1, DEEP_EW, DEEP_STREET);
    Point north = RampUp(nx + gap + 4, ns[0], 1, 0, DEEP_NS, DEEP_STREET);
    Point south = RampUp(nx - 4, ns[2], -1, 0, DEEP_NS, DEEP_STREET);
    RebuildStations();

    for (int k = 0; k < 2; k++) {
        SpawnTrainHeading(ex0 + 30 + k * (ex1 - ex0) / 2, ey, DEEP_EW, 3, 1, 0);
        SpawnTrainHeading(ex1 - 30 - k * (ex1 - ex0) / 2, ey + gap, DEEP_EW, 3, -1, 0);
        SpawnTrainHeading(nx + gap, ny0 + 10 + k * (ny1 - ny0) / 2, DEEP_NS, 3, 0, 1);
        SpawnTrainHeading(nx, ny1 - 10 - k * (ny1 - ny0) / 2, DEEP_NS, 3, 0, -1);
    }
    BuildEntrances();
    BuildGraph();

    if (trainSpeed == TRAIN_DEFAULT_SPEED) trainSpeed = 50.0f;   // unless --set trainSpeed chose one
    transferDistricts[0] = (Point){west.x, west.y - 12, DEEP_STREET};
    transferDistricts[1] = (Point){east.x, east.y + 12, DEEP_STREET};
    transferDistricts[2] = (Point){north.x + 12, north.y, DEEP_STREET};
    transferDistricts[3] = (Point){south.x - 12, south.y, DEEP_STREET};
    MetroResetStats();
    moverGoalPicker = TransferGoalPicker;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 40;
    for (int i = 0; i < n; i++) {
        Point d = transferDistricts[i % 4];
        SpawnMoverAt(d.x + GetRandomValue(-5, 5), d.y + GetRandomValue(-5, 5), DEEP_STREET);
    }
    currentViewZ = DEEP_STREET;
    AddMessage(TextFormat("Deep subway: east-west line at z%d, north-south at z%d under it, %d movers on the street (z%d)",
                          DEEP_EW, DEEP_NS, n, DEEP_STREET), GREEN);
}

static bool SubwayDeepCheck(char* why, int whyLen) {
    if (TrackLineCount() != 2) {
        snprintf(why, (size_t)whyLen, "expected 2 lines, found %d", TrackLineCount());
        return false;
    }
    if (useTrainTransfers && trainTransferPlans == 0) {
        snprintf(why, (size_t)whyLen, "no trip planned a change between the levels (%d rides)", metroRides);
        return false;
    }
    return true;
}

static bool SubwayCheck(char* why, int whyLen) {
    if (TrackLineCount() != 3) {
        snprintf(why, (size_t)whyLen, "expected 3 lines, found %d", TrackLineCount());
        return false;
    }
    if (metroRides == 0) {
        snprintf(why, (size_t)whyLen, "nobody went down to ride the subway");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// belt: conveyor spike (docs/todo/world/conveyors.md). Logs lie round stockpile A
// (west); two movers haul them in. A belt starts on A's east edge, snakes east and
// ends at stockpile B; its first cell pulls whole stacks out of A, its last puts them
// into B's slots (a third mover stands by). A crate of berries rides along.
// ---------------------------------------------------------------------------
static int beltLogsAtStart = 0;

static void BeltRun(int x0, int y0, int x1, int y1, BeltDir dir) {
    int dx, dy;
    BeltDirDelta(dir, &dx, &dy);
    for (int x = x0, y = y0; ; x += dx, y += dy) {
        PlaceBelt(x, y, 1, dir);
        if (x == x1 && y == y1) break;
    }
}

static void SetupBelt(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    int spA = CreateStockpile(3, 7, 1, 4, 4);       // x 3..6, y 7..10
    int spB = CreateStockpile(42, 7, 1, 4, 4);      // x 42..45
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) SetStockpileFilter(spA, (ItemType)t, t == ITEM_LOG);
    SetStockpileMaxContainers(spB, 2);   // B takes anything, the crate too

    // Line: (7,9) east to (20,9), south to (20,16), east to (34,16), north to (34,9),
    // east to (41,9); drops on (42,9) = B's west edge
    BeltRun(7, 9, 19, 9, BELT_E);
    BeltRun(20, 9, 20, 15, BELT_S);
    BeltRun(20, 16, 33, 16, BELT_E);
    BeltRun(34, 16, 34, 10, BELT_N);
    BeltRun(34, 9, 41, 9, BELT_E);

    // Logs to haul into A
    int logs = 0;
    for (int tries = 0; logs < 40 && tries < 2000; tries++) {
        int x = GetRandomValue(1, 14), y = GetRandomValue(1, 22);
        if (BeltAt(x, y, 1) >= 0 || IsPositionInStockpile(x * CELL_SIZE + 4, y * CELL_SIZE + 4, 1, NULL)) continue;
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_LOG);
        logs++;
    }
    beltLogsAtStart = logs;

    // A crate dropped on the belt: the belt picks it up from its cell, contents ride inside
    int chest = SpawnItem(12 * CELL_SIZE + CELL_SIZE * 0.5f, 9 * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_CHEST);
    for (int k = 0; k < 3; k++) {
        int b = SpawnItem(12 * CELL_SIZE + CELL_SIZE * 0.5f, 9 * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_BERRIES);
        if (chest >= 0 && b >= 0 && CanPutItemInContainer(b, chest)) PutItemInContainer(b, chest);
    }

    SpawnMoverAt(8, 4, 1);
    SpawnMoverAt(9, 4, 1);
    SpawnMoverAt(40, 13, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Belt: %d logs to haul into the west stockpile; the belt carries them east", logs), GREEN);
}

static int BeltCountLogsIn(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active || items[i].type != ITEM_LOG) continue;
        int x = (int)(items[i].x / CELL_SIZE), y = (int)(items[i].y / CELL_SIZE);
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) n += items[i].stackCount;
    }
    return n;
}

static void BeltPrintStats(void) {
    int riding = 0;
    for (int b = 0; b < beltCount; b++) if (belts[b].active && belts[b].item >= 0) riding++;
    printf("Belt: %d belt cells, %d loads put on, %d dropped off, %d riding now\n",
           beltCount, beltItemsIn, beltItemsOut, riding);
    printf("Logs: %d at start, %d in/around A (west), %d in/around B (east)\n",
           beltLogsAtStart, BeltCountLogsIn(0, 0, 15, 23), BeltCountLogsIn(41, 0, 47, 23));
}

static bool BeltCheck(char* why, int whyLen) {
    if (beltItemsOut < 2) {
        snprintf(why, (size_t)whyLen, "belt delivered %d loads (want >= 2)", beltItemsOut);
        return false;
    }
    return true;
}

// belt-lift: vertical belts. Ground at z=1, a deck at z=3 (x >= 16) with two shaft
// holes. Lift: logs the movers stock in A (ground, west) ride east, UP two levels at
// (16,9) and on along the deck into B. Chute: rock stacks pre-stocked in C (deck) ride
// east, DOWN two levels at (34,15) and into D on the ground.
static void SetupBeltLift(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    const int deckZ = 3;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 16; x < gridWidth; x++) {
            SET_FLOOR(x, y, deckZ);
            SetFloorMaterial(x, y, deckZ, MAT_OAK);
        }
    CLEAR_FLOOR(16, 9, deckZ);    // lift shaft
    CLEAR_FLOOR(34, 15, deckZ);   // chute shaft

    int spA = CreateStockpile(3, 7, 1, 4, 4);          // ground, x 3..6
    int spB = CreateStockpile(24, 7, deckZ, 4, 4);     // deck, x 24..27
    int spC = CreateStockpile(24, 14, deckZ, 4, 4);    // deck, x 24..27, y 14..17
    int spD = CreateStockpile(40, 13, 1, 4, 4);        // ground under the deck, x 40..43
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        SetStockpileFilter(spA, (ItemType)t, t == ITEM_LOG);
        SetStockpileFilter(spC, (ItemType)t, t == ITEM_ROCK);
    }
    // The deck has no way up for movers: only the lift fills B. Lowest priority keeps
    // haulers from picking B (or C) for a log they could store in A.
    SetStockpilePriority(spB, 1);
    SetStockpilePriority(spC, 1);
    (void)spD;

    // Lift: ground east, up two levels, deck east into B
    BeltRun(7, 9, 15, 9, BELT_E);
    PlaceBelt(16, 9, 1, BELT_UP);
    PlaceBelt(16, 9, 2, BELT_UP);
    for (int x = 16; x <= 23; x++) PlaceBelt(x, 9, deckZ, BELT_E);
    // Chute: from C (deck) east, down two levels, ground east into D
    for (int x = 28; x <= 33; x++) PlaceBelt(x, 15, deckZ, BELT_E);
    PlaceBelt(34, 15, deckZ, BELT_DOWN);
    PlaceBelt(34, 15, 2, BELT_DOWN);
    BeltRun(34, 15, 39, 15, BELT_E);

    // C starts stocked: 8 rock stacks of 5
    for (int k = 0; k < 8; k++) {
        int sx = 24 + k % 4, sy = 14 + k / 4;
        int r = SpawnItem(sx * CELL_SIZE + CELL_SIZE * 0.5f, sy * CELL_SIZE + CELL_SIZE * 0.5f, deckZ, ITEM_ROCK);
        if (r < 0) continue;
        items[r].stackCount = 5;
        PlaceItemInStockpile(spC, sx, sy, r);
    }
    // Logs to haul into A
    int logs = 0;
    for (int tries = 0; logs < 30 && tries < 2000; tries++) {
        int x = GetRandomValue(1, 14), y = GetRandomValue(1, 22);
        if (BeltAt(x, y, 1) >= 0 || IsPositionInStockpile(x * CELL_SIZE + 4, y * CELL_SIZE + 4, 1, NULL)) continue;
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_LOG);
        logs++;
    }
    beltLogsAtStart = logs;
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();

    SpawnMoverAt(8, 4, 1);
    SpawnMoverAt(9, 4, 1);
    currentViewZ = deckZ;
    AddMessage(TextFormat("Belt lift: %d logs go up to the deck, 40 rocks come down a chute", logs), GREEN);
}

static int BeltCountTypeIn(ItemType type, int x0, int y0, int x1, int y1, int z) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active || items[i].type != type || (int)items[i].z != z) continue;
        if (items[i].state == ITEM_ON_BELT) continue;
        int x = (int)(items[i].x / CELL_SIZE), y = (int)(items[i].y / CELL_SIZE);
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) n += items[i].stackCount;
    }
    return n;
}

static void BeltLiftPrintStats(void) {
    int riding = 0;
    for (int b = 0; b < beltCount; b++) if (belts[b].active && belts[b].item >= 0) riding++;
    printf("Belts: %d cells, %d loads put on, %d dropped off, %d riding now\n",
           beltCount, beltItemsIn, beltItemsOut, riding);
    printf("Lift:  %d logs at start, %d up in B (deck)\n", beltLogsAtStart, BeltCountTypeIn(ITEM_LOG, 24, 7, 27, 10, 3));
    printf("Chute: 40 rocks in C at start, %d left in C, %d down in D (ground)\n",
           BeltCountTypeIn(ITEM_ROCK, 24, 14, 27, 17, 3), BeltCountTypeIn(ITEM_ROCK, 40, 13, 43, 16, 1));
}

static bool BeltLiftCheck(char* why, int whyLen) {
    int up = BeltCountTypeIn(ITEM_LOG, 24, 7, 27, 10, 3), down = BeltCountTypeIn(ITEM_ROCK, 40, 13, 43, 16, 1);
    if (up < 1 || down < 1) {
        snprintf(why, (size_t)whyLen, "lift brought %d logs up, chute %d rocks down (want >= 1 each)", up, down);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// cargo: freight from a mine stop to a town stop
// ---------------------------------------------------------------------------

// One narrow loop line (two trains, four cars = 16 loads each) between a mine stop
// (west) and a town stop (east). Rocks lie around the mine. Next to each stop a cargo
// stockpile for rocks: the mine's at priority 2, the town's at 7. Four haulers at the
// mine pick up rocks for the best stockpile (the town); as they would board the train
// with one, they leave it at the mine's cargo stockpile instead and go back for the
// next. Trains load what the town takes and unload it there. Compare --set
// useTrainTimeChoice 0 (haulers walk every rock to town).
static int cargoTown = -1, cargoMine = -1, cargoRocks = 0;

static int StockpileItemCount(int sp) {
    int n = 0;
    if (sp < 0) return 0;
    const Stockpile* s = &stockpiles[sp];
    for (int i = 0; i < s->width * s->height; i++)
        if (s->cells[i] && s->slots[i] >= 0) n += s->slotCounts[i];
    return n;
}

static void SetupCargo(void) {
    const int W = gridWidth, gap = 3, ey = 28;
    const int x0 = 20, x1 = W - 21, mineStop = 32, townStop = W - 33;
    LayLoopLine(x0, x1, ey, gap, true);
    LayLoopStop(mineStop, ey, gap, true);
    LayLoopStop(townStop, ey, gap, true);
    RebuildStations();
    SpawnTrainHeading(x0 + 20, ey, 1, 4, 1, 0);
    SpawnTrainHeading(x1 - 20, ey + gap, 1, 4, -1, 0);
    // cargo stockpiles: two rows from the platforms
    cargoMine = CreateStockpile(mineStop - 4, ey - 6, 1, 8, 4);        // rows ey-6..ey-3 (platform ey-1)
    cargoTown = CreateStockpile(townStop - 4, ey + gap + 3, 1, 8, 4);  // rows ey+gap+3.. (platform ey+gap+1)
    int sps[2] = {cargoMine, cargoTown}, prio[2] = {2, 7};
    for (int k = 0; k < 2; k++) {
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) SetStockpileFilter(sps[k], (ItemType)t, false);
        SetStockpileFilter(sps[k], ITEM_ROCK, true);
        SetStockpilePriority(sps[k], prio[k]);
    }
    InvalidateStockpileSlotCacheAll();
    // a quarry north of the mine stop: every rock within reach of the station, stacks of 1
    cargoRocks = 0;
    for (int y = 15; y <= 19; y += 2)
        for (int x = 20; x <= 42; x++) {
            SpawnItem((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, 1.0f, ITEM_ROCK);
            cargoRocks++;
        }
    BuildEntrances();
    BuildGraph();
    if (trainSpeed == TRAIN_DEFAULT_SPEED) trainSpeed = 50.0f;   // unless --set trainSpeed chose one
    endlessMoverMode = true;   // idle haulers wander (off: an idle mover without a path is switched off)
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 4;
    for (int i = 0; i < n; i++) SpawnMoverAt(24 + i * 4, 12, 1);   // at the quarry
    currentViewZ = 1;
    AddMessage(TextFormat("Cargo: %d rocks at the mine, %d haulers; freight trains to the town stockpile", cargoRocks, n), GREEN);
}

static void CargoPrintStats(void) {
    int riding = 0;
    for (int t = 0; t < MAX_TRAINS; t++) if (trains[t].active) riding += trains[t].cargoCount;
    printf("Cargo: %d rocks; town stockpile %d, mine cargo stockpile %d, on trains %d\n",
           cargoRocks, StockpileItemCount(cargoTown), StockpileItemCount(cargoMine), riding);
    printf("Freight: %d loads loaded, %d unloaded; %d hauls left at the mine stop instead of riding\n",
           trainCargoLoaded, trainCargoUnloaded, trainCargoHaulDrops);
}

static bool CargoCheck(char* why, int whyLen) {
    if (trainCargoUnloaded == 0) {
        snprintf(why, (size_t)whyLen, "no freight reached the town (%d loaded, %d hauls dropped at the stop)",
                 trainCargoLoaded, trainCargoHaulDrops);
        return false;
    }
    return true;
}

// elevator: a tower of six floors on open ground. Two elevator shafts (one car each,
// capacity 4) and one straight staircase (ramps along the west side, from the ground under
// the tower to the top) connect the floors. Movers walk to random goals, half of them on a
// random floor of the tower (up there, most next goals are on the same floor): they line
// up at a door, ride, and get off where their path leaves the shaft, or take the stairs
// when the cars are busy (the expected wait is in the route cost). Compare
// --set elevatorWaitCost 0 (stairs only when shorter on foot).
#define ELEV_TOWER_X0 18
#define ELEV_TOWER_Y0 8
#define ELEV_TOWER_W 12
#define ELEV_TOWER_H 12
#define ELEV_TOP 7
#define ELEV_SHAFTS 2
static int elevShaftX[ELEV_SHAFTS], elevShaftY[ELEV_SHAFTS];
static int elevMaxRiders = 0, elevStairSteps = 0;
static float elevPrevZ[MAX_MOVERS];

static bool ElevIsShaft(int x, int y) {
    for (int s = 0; s < ELEV_SHAFTS; s++) if (elevShaftX[s] == x && elevShaftY[s] == y) return true;
    return false;
}

static Point ElevTowerGoal(int z) {
    for (;;) {
        Point g = {ELEV_TOWER_X0 + 1 + GetRandomValue(0, ELEV_TOWER_W - 2),
                   ELEV_TOWER_Y0 + GetRandomValue(0, ELEV_TOWER_H - 1), z};   // not the stair column
        if (!ElevIsShaft(g.x, g.y)) return g;
    }
}

static bool ElevatorGoalPicker(int moverIdx, Point* goal) {
    int mz = (int)movers[moverIdx].z;
    if (mz >= 2 && GetRandomValue(0, 99) < 70) {   // up in the tower: mostly stay on this floor a while
        *goal = ElevTowerGoal(mz);
        return true;
    }
    if (GetRandomValue(0, 1) == 0) {
        *goal = (Point){GetRandomValue(1, gridWidth - 2), GetRandomValue(1, gridHeight - 2), 1};
        if (goal->x >= ELEV_TOWER_X0 && goal->x < ELEV_TOWER_X0 + ELEV_TOWER_W &&
            goal->y >= ELEV_TOWER_Y0 && goal->y < ELEV_TOWER_Y0 + ELEV_TOWER_H) goal->y = 2;   // not under the tower
    } else {
        *goal = ElevTowerGoal(GetRandomValue(2, ELEV_TOP));
    }
    return true;
}

static void SetupElevator(void) {
    elevShaftX[0] = ELEV_TOWER_X0 + ELEV_TOWER_W / 2;     elevShaftY[0] = ELEV_TOWER_Y0 + ELEV_TOWER_H / 2;
    elevShaftX[1] = ELEV_TOWER_X0 + ELEV_TOWER_W - 3;     elevShaftY[1] = ELEV_TOWER_Y0 + 2;
    for (int z = 2; z <= ELEV_TOP; z++)
        for (int y = ELEV_TOWER_Y0; y < ELEV_TOWER_Y0 + ELEV_TOWER_H; y++)
            for (int x = ELEV_TOWER_X0; x < ELEV_TOWER_X0 + ELEV_TOWER_W; x++) {
                GRID(x, y, z) = CELL_AIR;
                SET_FLOOR(x, y, z);
            }
    for (int s = 0; s < ELEV_SHAFTS; s++)
        for (int z = 1; z <= ELEV_TOP; z++) {
            GRID(elevShaftX[s], elevShaftY[s], z) = CELL_ELEVATOR;
            CELL_FLAGS(elevShaftX[s], elevShaftY[s], z) &= ~CELL_FLAG_HAS_FLOOR;
        }
    // The staircase: a straight flight up the west column, one level per cell, rising north
    // from the ground under the tower to the top floor; each floor steps on and off it
    int sx = ELEV_TOWER_X0, sy = ELEV_TOWER_Y0 + ELEV_TOWER_H - 2;
    Point top = RampUp(sx, sy, 0, -1, 1, ELEV_TOP);
    for (int k = 0; k < ELEV_TOP - 1; k++) {
        CELL_FLAGS(sx, sy - k, 1 + k) &= ~CELL_FLAG_HAS_FLOOR;
        MarkBuiltStairs(sx, sy - k, 1 + k);   // built ramps: stairs (stairsClimbSpeed, stairsEffort)
    }
    (void)top;
    RebuildElevators();
    BuildEntrances();
    BuildGraph();
    endlessMoverMode = true;
    preferDifferentZ = false;
    moverGoalPicker = ElevatorGoalPicker;
    elevMaxRiders = 0;
    elevStairSteps = 0;
    int n = scenarioMovers > 0 ? scenarioMovers : 24;
    for (int i = 0; i < n; i++) SpawnMoverAt(4 + (i * 5) % (gridWidth - 8), 3 + (i / 8) * 13, 1);
    for (int i = 0; i < MAX_MOVERS; i++) elevPrevZ[i] = 1.0f;
    currentViewZ = 1;
    AddMessage(TextFormat("Elevator: a %d-floor tower, two elevators and a staircase, %d movers", ELEV_TOP - 1, n), GREEN);
}

static void ElevatorTickStats(void) {
    for (int k = 0; k < elevatorCount; k++)
        if (elevators[k].riderCount > elevMaxRiders) elevMaxRiders = elevators[k].riderCount;
    for (int i = 0; i < moverCount; i++) {   // a level climbed or descended on the stairs
        const Mover* m = &movers[i];
        if (!m->active) continue;
        if (m->z == floorf(m->z) && m->z != elevPrevZ[i]) {
            if (!ElevIsShaft((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE))) elevStairSteps += (int)fabsf(m->z - elevPrevZ[i]);
            elevPrevZ[i] = m->z;
        }
    }
}

static void ElevatorPrintStats(void) {
    int onLevel[16] = {0};
    for (int i = 0; i < moverCount; i++)
        if (movers[i].active && (int)movers[i].z < 16) onLevel[(int)movers[i].z]++;
    int waiting = 0, riding = 0;
    for (int i = 0; i < moverCount; i++) { waiting += ElevatorMoverWaiting(i); riding += ElevatorMoverAboard(i); }
    printf("Elevator: %d boardings (", elevatorRides);
    for (int k = 0; k < elevatorCount; k++) printf("%scar %d: %d stops", k ? ", " : "", k, elevators[k].trips);
    printf("), most riders at once %d (capacity %d); %d levels on the stairs (wait cost %s)\n",
           elevMaxRiders, elevatorCapacity, elevStairSteps, useElevatorWaitCost ? "on" : "off");
    printf("Now: %d in line for a car, %d aboard\n", waiting, riding);
    printf("Movers per level:");
    for (int z = 1; z <= ELEV_TOP; z++) printf(" z%d=%d", z, onLevel[z]);
    printf("\n");
}

static bool ElevatorCheck(char* why, int whyLen) {
    if (elevatorRides < 10 || elevMaxRiders > elevatorCapacity || elevStairSteps == 0) {
        snprintf(why, (size_t)whyLen, "%d boardings (want 10+), most riders %d (capacity %d), %d levels on the stairs",
                 elevatorRides, elevMaxRiders, elevatorCapacity, elevStairSteps);
        return false;
    }
    return true;
}

// elevator-tall: a ten-floor tower (z2..z11) over the ground, one elevator, no stairs.
// 30 movers go to random levels, up and down, so the car always has calls both ways.
// Shows how the car serves them: average and longest wait in line, and detours (an
// empty car that turns round for someone going the other way while calls further on
// still wait). Compare --set elevatorDirectionalPickup 0.
#define ET_X0 14
#define ET_Y0 6
#define ET_W 10
#define ET_TOP 11
#define ET_SX (ET_X0 + ET_W / 2)
#define ET_SY (ET_Y0 + ET_W / 2)

static bool ElevTallGoalPicker(int moverIdx, Point* goal) {
    (void)moverIdx;
    int z = GetRandomValue(1, ET_TOP);
    for (;;) {
        *goal = (Point){ET_X0 + GetRandomValue(0, ET_W - 1), ET_Y0 + GetRandomValue(0, ET_W - 1), z};
        if (goal->x != ET_SX || goal->y != ET_SY) return true;
    }
}

static void SetupElevatorTall(void) {
    for (int z = 2; z <= ET_TOP; z++)
        for (int y = ET_Y0; y < ET_Y0 + ET_W; y++)
            for (int x = ET_X0; x < ET_X0 + ET_W; x++) { GRID(x, y, z) = CELL_AIR; SET_FLOOR(x, y, z); }
    for (int z = 1; z <= ET_TOP; z++) {
        GRID(ET_SX, ET_SY, z) = CELL_ELEVATOR;
        CELL_FLAGS(ET_SX, ET_SY, z) &= ~CELL_FLAG_HAS_FLOOR;
    }
    RebuildElevators();
    BuildEntrances();
    BuildGraph();
    endlessMoverMode = true;
    preferDifferentZ = false;
    moverGoalPicker = ElevTallGoalPicker;
    int n = scenarioMovers > 0 ? scenarioMovers : 30;
    for (int i = 0; i < n; i++) SpawnMoverAt(ET_X0 + i % ET_W, ET_Y0 - 3 + (i / ET_W) * (ET_W + 5), 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Elevator tall: %d floors, one elevator, %d movers", ET_TOP - 1, n), GREEN);
}

static long etTicks[3], etRiderTicks, etMovingTicks, etWaitersTicks;
static void ElevatorTallTick(void) {
    if (elevatorCount < 1) return;
    const Elevator* e = &elevators[0];
    etTicks[e->state]++;
    if (e->state == CAR_MOVING) { etMovingTicks++; etRiderTicks += e->riderCount; }
    for (int i = 0; i < moverCount; i++) etWaitersTicks += ElevatorMoverWaiting(i);
}

static void ElevatorTallPrintStats(void) {
    long all = etTicks[0] + etTicks[1] + etTicks[2];
    if (all > 0)
        printf("Car time: idle %.0f%%, moving %.0f%%, doors %.0f%%; avg riders while moving %.2f; avg in line %.1f; stops %d\n",
               100.0 * etTicks[0] / all, 100.0 * etTicks[1] / all, 100.0 * etTicks[2] / all,
               etMovingTicks ? (double)etRiderTicks / etMovingTicks : 0.0, (double)etWaitersTicks / all, elevators[0].trips);
    printf("Elevator tall: %d boardings, wait in line avg %.1f s, longest %.1f s; %d detours (sweep pickup %s)\n",
           elevatorRides, elevatorWaitCount > 0 ? elevatorWaitSum / elevatorWaitCount : 0.0, elevatorWaitMax,
           elevatorDetours, elevatorDirectionalPickup ? "on" : "off");
}

static bool ElevatorTallCheck(char* why, int whyLen) {
    if (elevatorRides < 20 || (elevatorDirectionalPickup && elevatorDetours > 0)) {
        snprintf(why, (size_t)whyLen, "%d boardings (want 20+), %d detours (want 0 with sweep pickup)", elevatorRides, elevatorDetours);
        return false;
    }
    return true;
}

// elevator-stairs: ten floors (z2..z11) with one elevator in the middle and one straight
// staircase (ramps up the west side, ground to top). 15 movers go to random levels.
// Counts what each trip used by how far it went: elevator rides and stair climbs by
// number of floors. Shows how the route cost weighs a ride against the stairs.
#define ES_X0 14
#define ES_Y0 6
#define ES_W 12
#define ES_TOP 11
#define ES_SX (ES_X0 + ES_W / 2)
#define ES_SY (ES_Y0 + ES_W / 2)
static int esRide[4], esStair[4];          // by floors: 1-2, 3-5, 6-8, 9-10
static int esRun[MAX_MOVERS], esStill[MAX_MOVERS];
static float esPrevZ[MAX_MOVERS];
static int esPrevRides;
// Whole trips (goal picked -> goal reached): time by how they went and how far
static Point esTripGoal[MAX_MOVERS];
static double esTripStart[MAX_MOVERS];
static int esTripFromZ[MAX_MOVERS];
static bool esTripRode[MAX_MOVERS];
static double esTripSec[2][12];
static int esTripN[2][12];

static bool esAboard[MAX_MOVERS];
static int esBoardZ[MAX_MOVERS];

static int EsBucket(int floors) { return floors <= 2 ? 0 : floors <= 5 ? 1 : floors <= 8 ? 2 : 3; }

static bool ElevStairsGoalPicker(int moverIdx, Point* goal) {
    (void)moverIdx;
    int z = GetRandomValue(1, ES_TOP);
    for (;;) {
        *goal = (Point){ES_X0 + 1 + GetRandomValue(0, ES_W - 2), ES_Y0 + GetRandomValue(0, ES_W - 1), z};
        if (goal->x != ES_SX || goal->y != ES_SY) return true;
    }
}

static void SetupElevatorStairs(void) {
    for (int z = 2; z <= ES_TOP; z++)
        for (int y = ES_Y0; y < ES_Y0 + ES_W; y++)
            for (int x = ES_X0; x < ES_X0 + ES_W; x++) { GRID(x, y, z) = CELL_AIR; SET_FLOOR(x, y, z); }
    for (int z = 1; z <= ES_TOP; z++) {
        GRID(ES_SX, ES_SY, z) = CELL_ELEVATOR;
        CELL_FLAGS(ES_SX, ES_SY, z) &= ~CELL_FLAG_HAS_FLOOR;
    }
    int sx = ES_X0, sy = ES_Y0 + ES_W - 1;
    RampUp(sx, sy, 0, -1, 1, ES_TOP);
    for (int k = 0; k < ES_TOP - 1; k++) {
        CELL_FLAGS(sx, sy - k, 1 + k) &= ~CELL_FLAG_HAS_FLOOR;
        MarkBuiltStairs(sx, sy - k, 1 + k);   // built ramps: stairs
    }
    RebuildElevators();
    BuildEntrances();
    BuildGraph();
    endlessMoverMode = true;
    preferDifferentZ = false;
    moverGoalPicker = ElevStairsGoalPicker;
    memset(esRide, 0, sizeof(esRide)); memset(esStair, 0, sizeof(esStair));
    memset(esRun, 0, sizeof(esRun)); memset(esStill, 0, sizeof(esStill)); memset(esAboard, 0, sizeof(esAboard));
    memset(esTripSec, 0, sizeof(esTripSec)); memset(esTripN, 0, sizeof(esTripN));
    memset(esTripStart, 0, sizeof(esTripStart)); memset(esTripRode, 0, sizeof(esTripRode));
    for (int i = 0; i < MAX_MOVERS; i++) esTripGoal[i] = (Point){-1, -1, -1};
    for (int i = 0; i < MAX_MOVERS; i++) esPrevZ[i] = 1.0f;
    esPrevRides = 0;
    int n = scenarioMovers > 0 ? scenarioMovers : 15;
    for (int i = 0; i < n; i++) SpawnMoverAt(ES_X0 + 1 + i % (ES_W - 1), ES_Y0 - 3, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Elevator + stairs: %d floors, %d movers", ES_TOP - 1, n), GREEN);
}

static void ElevatorStairsTick(void) {
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        if (!m->active) continue;
        if (!PointsEqual(m->goal, esTripGoal[i])) {   // a new goal: the last trip ended
            int floors = abs(esTripGoal[i].z - esTripFromZ[i]);
            if (esTripStart[i] > 0.0 && floors < 12) {
                esTripSec[esTripRode[i]][floors] += gameTime - esTripStart[i];
                esTripN[esTripRode[i]][floors]++;
            }
            esTripGoal[i] = m->goal;
            esTripStart[i] = gameTime;
            esTripFromZ[i] = (int)floorf(m->z + 0.5f);
            esTripRode[i] = false;
        }
        if (ElevatorMoverAboard(i)) esTripRode[i] = true;
        int cx = (int)(m->x / CELL_SIZE), cy = (int)(m->y / CELL_SIZE);
        bool inShaft = cx == ES_SX && cy == ES_SY;
        if (m->z == floorf(m->z) && m->z != esPrevZ[i]) {
            if (!inShaft) esRun[i] += (int)fabsf(m->z - esPrevZ[i]);   // a level on the stairs
            esPrevZ[i] = m->z;
            esStill[i] = 0;
        } else if (esRun[i] > 0 && ++esStill[i] > 90) {   // off the stairs for a while: one trip
            esStair[EsBucket(esRun[i])]++;
            esRun[i] = 0;
        }
        bool aboard = ElevatorMoverAboard(i);
        if (aboard && !esAboard[i]) esBoardZ[i] = (int)floorf(m->z + 0.5f);
        if (!aboard && esAboard[i]) esRide[EsBucket(abs((int)m->z - esBoardZ[i]))]++;
        esAboard[i] = aboard;
        if (aboard) esPrevZ[i] = floorf(m->z + 0.5f);
    }
}

static void ElevatorStairsPrintStats(void) {
    printf("Trips by floors:        1-2   3-5   6-8  9-10\n");
    printf("  elevator rides:     %5d %5d %5d %5d\n", esRide[0], esRide[1], esRide[2], esRide[3]);
    printf("  stair climbs:       %5d %5d %5d %5d\n", esStair[0], esStair[1], esStair[2], esStair[3]);
    printf("Elevator: %d boardings, wait in line avg %.1f s\n", elevatorRides,
           elevatorWaitCount > 0 ? elevatorWaitSum / elevatorWaitCount : 0.0);
    ElevatorPrintWaitStats();
    printf("Trip time (s) by floors:");
    for (int f = 0; f <= 10; f++) printf(" %4d", f);
    printf("\n  by stairs/walk:       ");
    for (int f = 0; f <= 10; f++) printf(" %4.1f", esTripN[0][f] ? esTripSec[0][f] / esTripN[0][f] : 0.0);
    printf("\n  with a ride:          ");
    for (int f = 0; f <= 10; f++) printf(" %4.1f", esTripN[1][f] ? esTripSec[1][f] / esTripN[1][f] : 0.0);
    printf("\n  rides counted:        ");
    for (int f = 0; f <= 10; f++) printf(" %4d", esTripN[1][f]);
    printf("\n");
}

static bool ElevatorStairsCheck(char* why, int whyLen) {
    if (elevatorRides == 0) { snprintf(why, (size_t)whyLen, "nobody rode the elevator"); return false; }
    return true;
}

// elevator-build: movers build an elevator. Two decks (floors at z2 and z3, no stairs)
// stand east of a shaft blueprint on the ground; planks and cordage lie in the west. The
// first blueprint makes a shaft up to z2; once that stands, a second one on its top
// extends it to z3 (built like a ladder, level by level). Then movers ride up to the decks.
#define EB_SHAFT_X 15
#define EB_SHAFT_Y 9
static bool ebExtended = false;

static bool ElevBuildGoalPicker(int moverIdx, Point* goal) {
    (void)moverIdx;
    int k = ElevatorAt(EB_SHAFT_X, EB_SHAFT_Y, 1);
    int top = k >= 0 ? elevators[k].zMax : 1;
    if (top >= 2 && GetRandomValue(0, 99) < 60) {   // up on a deck the shaft reaches
        *goal = (Point){16 + GetRandomValue(1, 11), 5 + GetRandomValue(0, 9), GetRandomValue(2, top < 3 ? top : 3)};
        return true;
    }
    *goal = (Point){GetRandomValue(1, 13), GetRandomValue(1, gridHeight - 2), 1};
    return true;
}

static void SetupElevatorBuild(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    for (int z = 2; z <= 3; z++)
        for (int y = 5; y <= 14; y++)
            for (int x = 16; x <= 27; x++) { GRID(x, y, z) = CELL_AIR; SET_FLOOR(x, y, z); }
    CreateRecipeBlueprint(EB_SHAFT_X, EB_SHAFT_Y, 1, CONSTRUCTION_SHAFT);
    ebExtended = false;
    // Materials for two levels: 4 planks + 2 cordage each
    for (int i = 0; i < 12; i++) {
        int x = 3 + i % 6, y = 3 + i / 6;
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1,
                  (i % 3 == 2) ? ITEM_CORDAGE : ITEM_PLANKS);
    }
    BuildEntrances();
    BuildGraph();
    endlessMoverMode = true;
    preferDifferentZ = false;
    moverGoalPicker = ElevBuildGoalPicker;
    elevatorRides = 0;
    for (int i = 0; i < 3; i++) SpawnMoverAt(5 + i, 10, 1);
    currentViewZ = 1;
    AddMessage("Elevator build: a shaft blueprint by two decks; materials west. Built to z2, then extended to z3", GREEN);
}

static void ElevatorBuildTick(void) {
    // The first level stands: put the next blueprint on its top
    if (!ebExtended && GRID(EB_SHAFT_X, EB_SHAFT_Y, 2) == CELL_ELEVATOR) {
        if (CreateRecipeBlueprint(EB_SHAFT_X, EB_SHAFT_Y, 2, CONSTRUCTION_SHAFT) >= 0) ebExtended = true;
    }
}

static int ElevBuildTop(void) {
    int k = ElevatorAt(EB_SHAFT_X, EB_SHAFT_Y, 1);
    return k >= 0 ? elevators[k].zMax : 0;
}

static void ElevatorBuildPrintStats(void) {
    int pending = 0, up = 0;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) if (blueprints[i].active && blueprints[i].recipeIndex == CONSTRUCTION_SHAFT) pending++;
    for (int i = 0; i < moverCount; i++) if (movers[i].active && movers[i].z >= 2.0f) up++;
    printf("Elevator build: shaft reaches z%d, %d blueprints left; %d rides, %d movers up on the decks\n",
           ElevBuildTop(), pending, elevatorRides, up);
}

static bool ElevatorBuildCheck(char* why, int whyLen) {
    if (ElevBuildTop() < 3 || elevatorRides == 0) {
        snprintf(why, (size_t)whyLen, "shaft reaches z%d (want z3), %d rides", ElevBuildTop(), elevatorRides);
        return false;
    }
    return true;
}

// belt-build: belts built by movers. A 12-cell belt line is only blueprinted; planks
// and cordage lie by the movers. They haul materials and build the cells; as soon as
// the line is whole, the rock stacks pre-stocked in A ride to B.
static int beltBuildCells = 0;

static void SetupBeltBuild(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    int spA = CreateStockpile(3, 7, 1, 4, 4);       // x 3..6
    int spB = CreateStockpile(19, 7, 1, 4, 4);      // x 19..22
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {   // rocks only: planks and cordage stay for the builders
        SetStockpileFilter(spA, (ItemType)t, t == ITEM_ROCK);
        SetStockpileFilter(spB, (ItemType)t, t == ITEM_ROCK);
    }
    // Same priority (default) for A and B: haulers don't move rocks between them, only the belt does
    for (int k = 0; k < 8; k++) {
        int sx = 3 + k % 4, sy = 7 + k / 4;
        int r = SpawnItem(sx * CELL_SIZE + CELL_SIZE * 0.5f, sy * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_ROCK);
        if (r < 0) continue;
        items[r].stackCount = 5;
        PlaceItemInStockpile(spA, sx, sy, r);
    }
    beltBuildCells = 0;
    for (int x = 7; x <= 18; x++) if (CreateBeltBlueprint(x, 9, 1, BELT_E) >= 0) beltBuildCells++;
    // Materials: 2 planks + 1 cordage per cell, in a pile north of the line
    for (int i = 0; i < beltBuildCells * 3; i++) {
        int x = 8 + i % 10, y = 3 + i / 10;
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1,
                  (i % 3 == 2) ? ITEM_CORDAGE : ITEM_PLANKS);
    }
    for (int i = 0; i < 3; i++) SpawnMoverAt(10 + i, 5, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Belt build: %d belt blueprints, materials north; then rocks ride A -> B", beltBuildCells), GREEN);
}

static void BeltBuildPrintStats(void) {
    int built = 0, pending = 0;
    for (int b = 0; b < beltCount; b++) if (belts[b].active) built++;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) if (blueprints[i].active && blueprints[i].recipeIndex == CONSTRUCTION_BELT) pending++;
    printf("Belt build: %d of %d cells built, %d blueprints left; %d loads put on, %d dropped off\n",
           built, beltBuildCells, pending, beltItemsIn, beltItemsOut);
    printf("Rocks: %d in B\n", BeltCountTypeIn(ITEM_ROCK, 19, 7, 22, 10, 1));
}

static bool BeltBuildCheck(char* why, int whyLen) {
    int built = 0;
    for (int b = 0; b < beltCount; b++) if (belts[b].active) built++;
    if (built < beltBuildCells || BeltCountTypeIn(ITEM_ROCK, 19, 7, 22, 10, 1) < 1) {
        snprintf(why, (size_t)whyLen, "%d of %d belt cells built, %d rocks in B", built, beltBuildCells,
                 BeltCountTypeIn(ITEM_ROCK, 19, 7, 22, 10, 1));
        return false;
    }
    return true;
}

// belt-workshop: a belt feeds a sawmill and another carries its planks away. Logs the
// movers stock in A ride east into H, a small hopper stockpile linked to the sawmill;
// the crafter saws them; a belt starting on the sawmill's output tile carries the planks
// north and east into P. Transfer points are stockpiles and the output tile, no new rules.
static int beltWsLogsAtStart = 0;

static void SetupBeltWorkshop(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    int spA = CreateStockpile(3, 7, 1, 4, 4);       // x 3..6, y 7..10
    int spH = CreateStockpile(18, 8, 1, 2, 3);      // hopper x 18..19, y 8..10
    int spP = CreateStockpile(40, 3, 1, 4, 4);      // planks x 40..43, y 3..6
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        SetStockpileFilter(spA, (ItemType)t, t == ITEM_LOG);
        SetStockpileFilter(spH, (ItemType)t, t == ITEM_LOG);
        SetStockpileFilter(spP, (ItemType)t, t == ITEM_PLANKS);
    }
    // Sawmill 3x3 at (20,8): "#O#" / ".X." / "#.." -> output (21,8), work (21,9)
    int ws = CreateWorkshop(20, 8, 1, WORKSHOP_SAWMILL);
    if (ws >= 0) {
        AddBill(ws, 0, BILL_DO_FOREVER, 0);         // Saw Planks: 1 log -> 4 planks
        LinkStockpileToWorkshop(ws, spH);           // the crafter takes logs from the hopper only
    }
    BeltRun(7, 9, 17, 9, BELT_E);                   // A -> hopper (drops on (18,9))
    BeltRun(21, 8, 21, 6, BELT_N);                  // output tile north
    BeltRun(21, 5, 39, 5, BELT_E);                  // east, drops on (40,5) = P
    int logs = 0;
    for (int tries = 0; logs < 30 && tries < 2000; tries++) {
        int x = GetRandomValue(1, 14), y = GetRandomValue(13, 22);
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_LOG);
        logs++;
    }
    beltWsLogsAtStart = logs;
    SpawnMoverAt(8, 14, 1);
    SpawnMoverAt(9, 14, 1);
    SpawnMoverAt(24, 11, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Belt workshop: %d logs; a belt feeds the sawmill, another carries the planks away", logs), GREEN);
}

static int BeltWsCount(ItemType type, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active || items[i].type != type) continue;
        int x = (int)(items[i].x / CELL_SIZE), y = (int)(items[i].y / CELL_SIZE);
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) n += items[i].stackCount;
    }
    return n;
}

static void BeltWorkshopPrintStats(void) {
    int riding = 0;
    for (int b = 0; b < beltCount; b++) if (belts[b].active && belts[b].item >= 0) riding++;
    printf("Belt workshop: %d loads put on, %d dropped off, %d riding now\n", beltItemsIn, beltItemsOut, riding);
    printf("Logs: %d at start, %d in A, %d in hopper; planks: %d in P, %d anywhere\n",
           beltWsLogsAtStart, BeltWsCount(ITEM_LOG, 3, 7, 6, 10), BeltWsCount(ITEM_LOG, 18, 8, 19, 10),
           BeltWsCount(ITEM_PLANKS, 40, 3, 43, 6), BeltWsCount(ITEM_PLANKS, 0, 0, gridWidth - 1, gridHeight - 1));
}

static bool BeltWorkshopCheck(char* why, int whyLen) {
    int inP = BeltWsCount(ITEM_PLANKS, 40, 3, 43, 6);
    if (inP < 4) {
        snprintf(why, (size_t)whyLen, "%d planks reached P by belt (want >= 4)", inP);
        return false;
    }
    return true;
}


// belt-play: a playground for building belts by hand in the play HUD (starts with the dev UI
// off). Logs stocked in A (west), a sawmill with an empty hopper H, an empty plank pile P
// (east), and a store M of planks + cordage for ~30 belts. No belts: Huisje -> Build Belt.
static void PlaceStack(int sp, int x, int y, ItemType type, int count) {
    int it = SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1, type);
    if (it < 0) return;
    items[it].stackCount = count;
    PlaceItemInStockpile(sp, x, y, it);
}

static void SetupBeltPlay(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    int spA = CreateStockpile(3, 7, 1, 4, 4);       // logs, x 3..6, y 7..10
    int spH = CreateStockpile(19, 8, 1, 2, 3);      // hopper, x 19..20, y 8..10
    int spP = CreateStockpile(40, 3, 1, 4, 4);      // planks out, x 40..43, y 3..6
    int spM = CreateStockpile(8, 17, 1, 6, 2);      // building store, x 8..13, y 17..18
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        SetStockpileFilter(spA, (ItemType)t, t == ITEM_LOG);
        SetStockpileFilter(spH, (ItemType)t, t == ITEM_LOG);
        SetStockpileFilter(spP, (ItemType)t, t == ITEM_PLANKS);
        SetStockpileFilter(spM, (ItemType)t, t == ITEM_PLANKS || t == ITEM_CORDAGE);
    }
    for (int k = 0; k < 16; k++) PlaceStack(spA, 3 + k % 4, 7 + k / 4, ITEM_LOG, 2);
    for (int k = 0; k < 6; k++) PlaceStack(spM, 8 + k, 17, ITEM_PLANKS, 10);   // 60 planks
    for (int k = 0; k < 6; k++) PlaceStack(spM, 8 + k, 18, ITEM_CORDAGE, 5);   // 30 cordage
    // Sawmill 3x3 at (22,8): output tile (23,8), work tile (23,9); saws only from the hopper
    int ws = CreateWorkshop(22, 8, 1, WORKSHOP_SAWMILL);
    if (ws >= 0) {
        AddBill(ws, 0, BILL_DO_FOREVER, 0);
        LinkStockpileToWorkshop(ws, spH);           // logs from the hopper only, not from A
    }
    SpawnMoverAt(10, 13, 1);
    SpawnMoverAt(12, 13, 1);
    SpawnMoverAt(26, 12, 1);
    currentViewZ = 1;
    devUI = false;   // the play HUD: Huisje -> Build Belt
    AddMessage("Belt playground: Huisje -> Build Belt. Lay A -> hopper (west of the sawmill), "
               "and from the sawmill's output tile (top middle) to P", GREEN);
}

// tide: the experiment in docs/todo/world/tide.md. Land in the west (walk level 3), mud flats
// (sea floor at z 1, sea surface z 2: dry at low tide, walk level 2), a tidal channel across
// them (x 34..37, two levels deep, never dry) with a shallow ford (y 30..34), an islet in the
// east (walk level 3) with 12 logs, a log stockpile on land, 4 movers. Ramps from the land and
// the islet down to the flats. The tide: 12.42 game hours, full to dry.
#define TIDE_LAND_X 20      // land x 0..20
#define TIDE_ISLET_X0 48
#define TIDE_ISLET_X1 54
#define TIDE_ISLET_Y0 14
#define TIDE_ISLET_Y1 24
static int tideLogsAtStart = 0;
static int tideMaxOnFlats = 0, tideMoverTicksDeep = 0, tideMaxDeep = 0, tideMinLevel = 7, tideMaxDryFlats = 0;
static long tideFlatsMoverTicks = 0;

static bool TideIsIslet(int x, int y) {
    return x >= TIDE_ISLET_X0 && x <= TIDE_ISLET_X1 && y >= TIDE_ISLET_Y0 && y <= TIDE_ISLET_Y1;
}
static bool TideIsChannel(int x, int y) { return x >= 34 && x <= 37 && !(y >= 30 && y <= 34); }

static void SetupTide(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            for (int z = 0; z < gridDepth; z++) { GRID(x, y, z) = CELL_AIR; CLEAR_FLOOR(x, y, z); }
            GRID(x, y, 0) = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT);
            if (!TideIsChannel(x, y)) { GRID(x, y, 1) = CELL_WALL; SetWallMaterial(x, y, 1, MAT_SAND); }
            if (x <= TIDE_LAND_X || TideIsIslet(x, y)) { GRID(x, y, 2) = CELL_WALL; SetWallMaterial(x, y, 2, MAT_DIRT); }
        }
    // Ramps from the flats (z 2) up: three onto the land, one onto the islet
    for (int k = 0; k < 3; k++) RampUp(TIDE_LAND_X + 1, 8 + 12 * k, -1, 0, 2, 3);
    RampUp(TIDE_ISLET_X0 - 1, 19, 1, 0, 2, 3);
    // The sea: channel (z 1) and everything over the flats (z 2), full
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            if (TideIsChannel(x, y)) SetWaterLevel(x, y, 1, WATER_MAX_LEVEL);
            if (GRID(x, y, 2) == CELL_AIR) SetWaterLevel(x, y, 2, WATER_MAX_LEVEL);
        }
    TideMarkSea(2);
    tideEnabled = true;
    tideLowLevel = 0;
    TideSettle();

    int sp = CreateStockpile(4, 16, 3, 4, 4);
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) SetStockpileFilter(sp, (ItemType)t, t == ITEM_LOG);
    tideLogsAtStart = 0;
    for (int k = 0; k < 12; k++) {
        int x = TIDE_ISLET_X0 + 1 + k % 5, y = TIDE_ISLET_Y0 + 2 + (k / 5) * 3;
        if (SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 3, ITEM_LOG) >= 0) tideLogsAtStart++;
    }
    BuildEntrances();
    BuildGraph();
    for (int i = 0; i < 4; i++) SpawnMoverAt(8 + i * 2, 12, 3);
    tideMaxOnFlats = tideMoverTicksDeep = tideMaxDeep = tideMaxDryFlats = 0;
    tideFlatsMoverTicks = 0;
    tideMinLevel = WATER_MAX_LEVEL;
    currentViewZ = 3;
    AddMessage("Tide: logs on the islet, a stockpile on land; the flats fall dry twice a day", GREEN);
}

static void TideTickStats(void) {
    int onFlats = 0, deep = 0;
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        if (!m->active) continue;
        int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)m->z;
        if (z == tideSeaZ && WATER(x, y, z).isSea) onFlats++;
        if (GetWaterLevel(x, y, z) >= WATER_BLOCKS_MOVEMENT) deep++;
    }
    if (getenv("TIDE_TRACE") && currentTick % 100 == 0) {   // experiment trace
        printf("t%5lld lvl %d flats %d deep %d |", (long long)currentTick, tideLevel, onFlats, deep);
        for (int i = 0; i < moverCount && i < 8; i++) {
            const Mover* m = &movers[i];
            int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)m->z;
            const Job* j = m->currentJobId >= 0 ? GetJob(m->currentJobId) : NULL;
            printf(" (%2d,%2d,%d w%d %s)", x, y, z, GetWaterLevel(x, y, z), j ? JobTypeName(j->type) : "-");
        }
        printf("\n");
    }
    if (onFlats > tideMaxOnFlats) tideMaxOnFlats = onFlats;
    tideFlatsMoverTicks += onFlats;
    tideMoverTicksDeep += deep;
    if (deep > tideMaxDeep) tideMaxDeep = deep;
    if (tideLevel < tideMinLevel) tideMinLevel = tideLevel;
    if (tideLevel == tideLowLevel && tideMaxDryFlats == 0 && tideSeaZ >= 0) {
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                if (WATER(x, y, tideSeaZ).isSea && IsCellWalkableAt(tideSeaZ, y, x)) tideMaxDryFlats++;
    }
}

static int TideLogsOnLand(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].type == ITEM_LOG && items[i].x < (TIDE_LAND_X + 1) * CELL_SIZE) n += items[i].stackCount;
    return n;
}

static void TidePrintStats(void) {
    printf("Tide: %d sea cells at z%d, level now %d (lowest %d), %d steps, step %.3f ms (max %.3f)\n",
           tideSeaCells, tideSeaZ, tideLevel, tideMinLevel, tideSteps, tideLastStepMs, tideMaxStepMs);
    printf("Flats walkable at low tide: %d cells; movers on the flats: max %d at once, %ld mover-ticks\n",
           tideMaxDryFlats, tideMaxOnFlats, tideFlatsMoverTicks);
    printf("Movers in deep water: max %d at once, %d mover-ticks", tideMaxDeep, tideMoverTicksDeep);
    if (tideLogsAtStart > 0) printf("; logs on land %d of %d", TideLogsOnLand(), tideLogsAtStart);
    printf("\n");
}

static bool TideCheck(char* why, int whyLen) {
    if (tideSteps < 7) { snprintf(why, (size_t)whyLen, "tide moved %d steps (want >= 7)", tideSteps); return false; }
    return true;
}

// tide-island: the island (scenarioSize, seed) with the tide on its sea. The shallowest sea
// floor band is one level under the surface: those cells fall dry at low tide. The beach is one
// level higher, so TidePrepareShore turns its edge into ramps down to the flats. 100 wandering
// movers (endless mode): some wander out at low tide.
static int tideIslandRamps = 0;
static void SetupTideIsland(void) {
    SetupWorld();
    int sea = TideMarkSea(waterEdgeSeaZ);
    tideIslandRamps = TidePrepareShore();
    tideEnabled = true;
    tideLowLevel = 0;
    TideSettle();
    BuildEntrances();
    BuildGraph();
    tideMaxOnFlats = tideMoverTicksDeep = tideMaxDeep = tideMaxDryFlats = 0;
    tideFlatsMoverTicks = 0;
    tideMinLevel = WATER_MAX_LEVEL;
    tideLogsAtStart = 0;
    AddMessage(TextFormat("Tide island: %d sea cells at z%d, %d shore ramps", sea, tideSeaZ, tideIslandRamps), GREEN);
}

static void TideIslandPrintStats(void) {
    printf("Shore ramps: %d\n", tideIslandRamps);
    TidePrintStats();
    DryGroundPrintStats();
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
                   "Hills",     SetupHubs,     NULL, DryGroundPrintStats, NULL, 128, 128},
    {"world",      "Hills with soils and water, 100 wandering movers; scenarioSize sets the map (for --profile)",
                   "HillsSoilsWater", SetupWorld, NULL, DryGroundPrintStats, NULL, 256, 256},
    {"island",     "A finite island grown from a seed (drainage, rivers, lakes, sea, cover), 100 wandering movers; scenarioSize sets the map",
                   "Island",    SetupWorld, NULL, DryGroundPrintStats, NULL, 256, 256},
    {"island-colony", "Lived-in island: 20 settlers chop, dig, gather and build two huts by the landing beach; needs on, nobody dies (big-map measuring target)",
                   "Island",    SetupIslandColony, NULL, IslandColonyPrintStats, NULL, 256, 256},
    {"island-winter", "island-colony in winter: snowing, a fire pit and torches at the stockpile and hut corners (measures temperature, light, snow, fire, smoke)",
                   "Island",    SetupIslandWinter, NULL, IslandWinterPrintStats, NULL, 256, 256},
    {"maze",       "3D labyrinth, 100 wandering movers: long paths (coarse paths, step cap); scenarioSize sets the map",
                   "Labyrinth3D", SetupWorld, NULL, NULL, NULL, 256, 256},
    {"dayrhythm",  "8 movers, beds, dried berries, endless mining: who is in bed at 03:00 and 13:00 (setting sleepRhythm)",
                   NULL,        SetupDayRhythm, DayRhythmTickStats, DayRhythmPrintStats, NULL, 64, 64},
    {"worksplit",  "Haul work west, mine work east, movers at both: compare jobDistanceWeight 0 vs > 0",
                   NULL,        SetupWorkSplit, WorkSplitTickStats, WorkSplitPrintStats, NULL, 48, 40},
    {"train-double", "Double track (one per direction), platforms on both sides, 3 stops, 32 movers",
                   NULL,        SetupTrainDouble, NULL, TrainDoublePrintStats, NULL, 240, 24},
    {"metro",      "Four coloured lines (circle, east-west, two north-south) with interchanges, 48 movers",
                   NULL,        SetupMetro, MetroTickStats, MetroPrintStats, MetroCheck, 320, 240},
    {"transfer",   "A T of three loop lines: trips between the ends need a change of line, 40 movers (compare useTrainTransfers 0)",
                   NULL,        SetupTransfer, MetroTickStats, TransferPrintStats, TransferCheck, 512, 320},
    {"subway",     "The transfer network in a tunnel two levels under the street: ramps down, halls, changes underground, 40 movers",
                   NULL,        SetupSubway, MetroTickStats, TransferPrintStats, SubwayCheck, 512, 320},
    {"subway-deep", "Two lines at different depths, one crossing under the other; change by ramps between levels, 40 movers",
                   NULL,        SetupSubwayDeep, MetroTickStats, TransferPrintStats, SubwayDeepCheck, 512, 320},
    {"cargo",      "Freight: haulers leave rocks at the mine stop, trains carry them to the town stockpile, 4 haulers",
                   NULL,        SetupCargo, NULL, CargoPrintStats, CargoCheck, 240, 60},
    {"transit-chain", "Six lines in a staircase: end to end is five changes of line, 48 movers",
                   NULL,        SetupTransitChain, MetroTickStats, TransferPrintStats, TransitChainCheck, 700, 700},
    {"belt",       "Conveyor spike: movers stock a log pile, a belt carries the stacks (and a crate) to a second pile, 3 movers",
                   NULL,        SetupBelt, NULL, BeltPrintStats, BeltCheck, 48, 24},
    {"belt-lift",  "Vertical belts: a lift carries logs up to a deck, a chute brings rocks down, 2 movers",
                   NULL,        SetupBeltLift, NULL, BeltLiftPrintStats, BeltLiftCheck, 48, 24},
    {"elevator-tall", "A ten-floor tower with one elevator, 30 movers going up and down: waits and detours (compare elevatorDirectionalPickup 0)",
                   NULL,        SetupElevatorTall, ElevatorTallTick, ElevatorTallPrintStats, ElevatorTallCheck, 40, 28},
    {"elevator-stairs", "Ten floors, one elevator and one staircase, 15 movers: which trips ride and which climb, by distance",
                   NULL,        SetupElevatorStairs, ElevatorStairsTick, ElevatorStairsPrintStats, ElevatorStairsCheck, 40, 28},
    {"elevator-build", "Movers build an elevator shaft from blueprints (planks + cordage), level by level, then ride it up to two decks, 3 movers",
                   NULL,        SetupElevatorBuild, ElevatorBuildTick, ElevatorBuildPrintStats, ElevatorBuildCheck, 32, 20},
    {"belt-build", "Movers build a 12-cell belt from blueprints (planks + cordage), then rocks ride it, 3 movers",
                   NULL,        SetupBeltBuild, NULL, BeltBuildPrintStats, BeltBuildCheck, 32, 20},
    {"elevator",   "A six-floor tower with two elevators and a staircase: movers line up, ride, or take the stairs when the cars are busy, 24 movers",
                   NULL,        SetupElevator, ElevatorTickStats, ElevatorPrintStats, ElevatorCheck, 48, 32},
    {"belt-workshop", "A belt feeds a sawmill's hopper stockpile, a second belt carries the planks from its output tile, 3 movers",
                   NULL,        SetupBeltWorkshop, NULL, BeltWorkshopPrintStats, BeltWorkshopCheck, 48, 24},
    {"belt-play",  "Playground: build belts yourself in the play HUD (Huisje -> Build Belt); logs, a sawmill with an empty hopper, planks + cordage, 3 movers",
                   NULL,        SetupBeltPlay, NULL, BeltWorkshopPrintStats, NULL, 48, 24},
    {"tide",       "Tide experiment: mud flats fall dry twice a day, logs on an islet reachable at low tide, a channel with a ford, 4 movers",
                   NULL,        SetupTide, TideTickStats, TidePrintStats, TideCheck, 64, 40},
    {"tide-island", "The island with the tide on its sea: flats fall dry, ramps from the beach down, 100 wandering movers; scenarioSize sets the map",
                   "Island",    SetupTideIsland, TideTickStats, TideIslandPrintStats, TideCheck, 256, 256},
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
