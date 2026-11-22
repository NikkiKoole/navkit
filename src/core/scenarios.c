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
#include "../entities/containers.h"
#include "../core/sim_manager.h"
#include "../simulation/needs.h"
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
    ClearBelts();
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
    {"transit-chain", "Six lines in a staircase: end to end is five changes of line, 48 movers",
                   NULL,        SetupTransitChain, MetroTickStats, TransferPrintStats, TransitChainCheck, 700, 700},
    {"belt",       "Conveyor spike: movers stock a log pile, a belt carries the stacks (and a crate) to a second pile, 3 movers",
                   NULL,        SetupBelt, NULL, BeltPrintStats, BeltCheck, 48, 24},
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
