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
#include "state_audit.h"
#include "../entities/inventory.h"
#include "../simulation/temperature.h"
#include <string.h>
#include <strings.h>
#include "../entities/notice.h"
#include "../simulation/fire.h"
#include "../simulation/explosion.h"
#include "../simulation/vapour.h"
#include "../world/support.h"
#include "../entities/talk.h"
#include "../entities/relations.h"
#include "../entities/chase.h"
#include "../entities/activity.h"
#include "../entities/town.h"
#include "../entities/action_queue.h"
#include "../entities/throwing.h"

// ---------------------------------------------------------------------------
// Settings scenarios change. Snapshotted before the first scenario runs and
// restored by every ScenarioResetWorld, so one scenario's tweaks (e.g. mood's
// needs + speed) don't leak into the next.
// ---------------------------------------------------------------------------
typedef struct {
    bool hunger, thirst, energy, endless, preferDiffZ, mood, talk, autoGather;
    float wanderPause, dayLength, baseMoverSpeed, jobDistanceWeight, trainSpeed;
    bool support;
    float animalFlee; bool huntThrow, animalRespawn;
    int baseTemp, seasonAmp, dayAmp;   // climate (town sets a mild one)
    bool weather;
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
                                           endlessMoverMode, preferDifferentZ, moodEnabled, talkEnabled, autoGatherings,
                                           moverWanderPause, dayLength, balance.baseMoverSpeed, jobDistanceWeight,
                                           trainSpeed, supportEnabled, animalFleeSpeed, huntThrowStones,
                                           animalRespawnEnabled,
                                           baseSurfaceTemp, seasonalAmplitude, diurnalAmplitude, weatherEnabled};
        haveSavedSettings = true;
        return;
    }
    hungerEnabled = savedSettings.hunger;
    thirstEnabled = savedSettings.thirst;
    energyEnabled = savedSettings.energy;
    endlessMoverMode = savedSettings.endless;
    preferDifferentZ = savedSettings.preferDiffZ;
    moodEnabled = savedSettings.mood;
    talkEnabled = savedSettings.talk;
    autoGatherings = savedSettings.autoGather;
    autoGames = false;   // movers starting games by themselves: a scenario that wants it sets it
    moverWanderPause = savedSettings.wanderPause;
    dayLength = savedSettings.dayLength;
    balance.baseMoverSpeed = savedSettings.baseMoverSpeed;
    jobDistanceWeight = savedSettings.jobDistanceWeight;
    trainSpeed = savedSettings.trainSpeed;
    supportEnabled = savedSettings.support;
    animalFleeSpeed = savedSettings.animalFlee;
    huntThrowStones = savedSettings.huntThrow;
    animalRespawnEnabled = savedSettings.animalRespawn;
    baseSurfaceTemp = savedSettings.baseTemp;
    seasonalAmplitude = savedSettings.seasonAmp;
    diurnalAmplitude = savedSettings.dayAmp;
    weatherEnabled = savedSettings.weather;
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
    springTrickle = false;   // sources are taps unless the terrain says otherwise (island)
    buildNow = true;         // dev setups: the build tools work instantly (I flips to order)
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
    ClearTown();
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

// faces: 16 movers wandering a flat field with fixed moods (mood system off), so every
// head sprite shows: the view follows the walking direction (front when standing), the
// mouth the mood (sad, flat, grin, smile). Setting moverFaces turns it off.
static void SetupFaces(void) {
    endlessMoverMode = true;
    moodEnabled = false;
    hungerEnabled = thirstEnabled = energyEnabled = false;
    static const float moods[4] = { -0.8f, -0.3f, 0.0f, 0.8f };
    for (int i = 0; i < 16; i++) {
        int before = moverCount;
        SpawnMoverAt(6 + (i % 4) * 6, 6 + (i / 4) * 6, 1);
        if (moverCount > before) movers[moverCount - 1].mood = moods[i % 4];
    }
    currentViewZ = 1;
    AddMessage("Faces: columns are sad, flat, grin, smile; they look where they walk", GREEN);
}

// social: 10 movers wandering a plaza with two wall pieces (line of sight), noticing each
// other (notice.c). Standing between goals they look at whoever they noticed most.
// Headless stats: noticed per mover, how often attention switches, standing movers looking.
static long socialTalkTicks, socialIntrusions, socialGroupTicks, socialBigGroupTicks;
static long socialTicks, socialNoticedSum, socialSwitches, socialFlickBacks, socialStanding, socialStandingLooking;
static int socialPrevTop[MAX_MOVERS], socialPrevPrevTop[MAX_MOVERS];

static void SetupSocial(void) {
    socialTalkTicks = socialIntrusions = socialGroupTicks = socialBigGroupTicks = 0;
    socialTicks = socialNoticedSum = socialSwitches = socialFlickBacks = socialStanding = socialStandingLooking = 0;
    for (int i = 0; i < MAX_MOVERS; i++) socialPrevTop[i] = socialPrevPrevTop[i] = -1;
    for (int y = 6; y <= 12; y++) { GRID(16, y, 1) = CELL_WALL; SetWallMaterial(16, y, 1, MAT_GRANITE); }
    for (int y = 20; y <= 26; y++) { GRID(16, y, 1) = CELL_WALL; SetWallMaterial(16, y, 1, MAT_GRANITE); }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    talkEnabled = true;
    moverWanderPause = 3.0f;   // stand a while between goals: that is when they look around
    int n = scenarioMovers > 0 ? scenarioMovers : 10;
    for (int i = 0; i < n; i++) SpawnMoverAt(4 + (i % 5) * 6, 4 + (i / 5) * 20 % 28, 1);
    currentViewZ = 1;
    AddMessage("Social: movers notice each other; standing still they look at who they noticed", GREEN);
}

static void SocialTickStats(void) {
    socialTicks++;
    for (int t = 0; t < MAX_TALKS; t++) {
        if (!talks[t].active) continue;
        socialGroupTicks++;
        if (talks[t].count >= 3) socialBigGroupTicks++;
    }
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        socialNoticedSum += moverNotice[i].count;
        if (MoverTalk(i) >= 0) socialTalkTicks++;
        else if (movers[i].pathIndex >= 0) {   // a walker inside a conversation's circle
            for (int t = 0; t < MAX_TALKS; t++) {
                if (!talks[t].active || talks[t].z != (int)movers[i].z) continue;
                float dx = movers[i].x - talks[t].centerX, dy = movers[i].y - talks[t].centerY;
                float in = talks[t].radius * 0.8f;
                if (dx * dx + dy * dy < in * in) socialIntrusions++;
            }
        }
        int top = NoticeTop(i);
        if (top != socialPrevTop[i] && socialPrevTop[i] >= 0 && top >= 0) {
            socialSwitches++;
            if (top == socialPrevPrevTop[i]) socialFlickBacks++;   // A -> B -> A
        }
        if (top != socialPrevTop[i]) { socialPrevPrevTop[i] = socialPrevTop[i]; socialPrevTop[i] = top; }
        if (movers[i].pathLength <= 0 || movers[i].pathIndex < 0) {
            socialStanding++;
            if (top >= 0) socialStandingLooking++;
        }
    }
}

static void SocialPrintStats(void) {
    int active = CountActiveMovers();
    double minutes = socialTicks / (double)TICK_RATE / 60.0;
    printf("Noticing: %s, radius %.1f cells, every %d ticks\n", noticeEnabled ? "on" : "off",
           noticeRadius, noticeEveryTicks);
    printf("Avg movers noticed per mover: %.2f\n",
           socialTicks && active ? socialNoticedSum / (double)(socialTicks * active) : 0.0);
    printf("Attention switches per mover per minute: %.1f\n",
           minutes > 0 && active ? socialSwitches / (minutes * active) : 0.0);
    {   // Relations after the run
        int known = 0, familiar = 0, liked = 0, disliked = 0;
        for (int i = 0; i < moverCount; i++)
            for (int k = 0; k < moverRelations[i].count; k++) {
                const Relation* r = RelationGet(i, moverRelations[i].r[k].other);
                if (!r) continue;
                known++;
                if (r->familiarity >= 15.0f) familiar++;
                if (r->like >= 20.0f) liked++;
                if (r->like <= -20.0f) disliked++;
            }
        printf("Relations: %d known (%.1f per mover), %d familiar, %d liked, %d disliked\n", known,
               active ? known / (double)active : 0.0, familiar, liked, disliked);
    }
    printf("Talks started: %ld (%.1f per mover per minute), time spent talking: %.0f%%\n", talkStartedCount,
           minutes > 0 && active ? talkStartedCount / (minutes * active) : 0.0,
           socialTicks && active ? 100.0 * socialTalkTicks / (socialTicks * active) : 0.0);
    printf("Joined a running talk: %ld; talks with 3+ people: %.0f%% of talk time\n", talkJoinedCount,
           socialGroupTicks ? 100.0 * socialBigGroupTicks / socialGroupTicks : 0.0);
    printf("Walkers inside a conversation's circle (give room %s): %ld ticks\n", talkGiveRoom ? "on" : "off",
           socialIntrusions);
    printf("  of which back to the one before (A->B->A): %.1f\n",
           minutes > 0 && active ? socialFlickBacks / (minutes * active) : 0.0);
    printf("Standing mover-ticks: %ld, ", socialStanding);
    printf("Standing still looking at someone: %.0f%% of standing mover-ticks\n",
           socialStanding ? 100.0 * socialStandingLooking / socialStanding : 0.0);
}

static bool SocialCheck(char* why, int whyLen) {
    if (noticeEnabled && socialNoticedSum == 0) {
        snprintf(why, (size_t)whyLen, "nobody noticed anybody");
        return false;
    }
    return true;
}

// hallway: a 3-wide corridor 30 long, 12 movers wandering it: two-way traffic all the time.
// Compare politePassing on/off: close contacts (two walking movers within half a cell) and
// arrivals (throughput).
static long hallTicks, hallContacts;

static void SetupHallway(void) {
    hallTicks = hallContacts = 0;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            if (x >= 1 && x <= 30 && y >= 14 && y <= 16) continue;
            GRID(x, y, 1) = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_GRANITE);
        }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 12;
    for (int i = 0; i < n; i++) SpawnMoverAt(1 + (i * 29) / (n > 1 ? n - 1 : 1), 14 + i % 3, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Hallway: %d movers in a 3-wide corridor (polite passing %s)", n,
                          politePassing ? "on" : "off"), GREEN);
}

static void HallwayTickStats(void) {
    hallTicks++;
    float r = CELL_SIZE * 0.5f;
    for (int i = 0; i < moverCount; i++) {
        const Mover* a = &movers[i];
        if (!a->active || a->pathIndex < 0) continue;
        for (int j = i + 1; j < moverCount; j++) {
            const Mover* b = &movers[j];
            if (!b->active || b->pathIndex < 0 || (int)b->z != (int)a->z) continue;
            float dx = a->x - b->x, dy = a->y - b->y;
            if (dx * dx + dy * dy < r * r) hallContacts++;
        }
    }
}

static void HallwayPrintStats(void) {
    int active = CountActiveMovers();
    double minutes = hallTicks / (double)TICK_RATE / 60.0;
    printf("Polite passing: %s (radius %.1f, sideways %.2f, slow %.2f)\n", politePassing ? "on" : "off",
           passRadius, passStrength, passSlow);
    printf("Close contacts (walking pairs within half a cell): %ld ticks, %.1f per mover per minute\n",
           hallContacts, minutes > 0 && active ? hallContacts / (minutes * active) : 0.0);
}

// doorway: two rooms split by a wall with one door (a 1-wide gap), 16 movers wandering both:
// traffic through the door both ways. Tunnel yielding makes one wait; compare yieldLetPass
// on/off: bumping the one it waited for in the second after the wait, and arrivals.
static const int doorwayX = 16, doorwayY = 16;
static long doorwayTicks, doorwayContacts, doorwayWaitTicks, doorwayPassContacts, doorwayWaits, doorwayNods;
static bool doorwayNodding[MAX_MOVERS];
static int doorwayLastFor[MAX_MOVERS], doorwaySince[MAX_MOVERS];   // whom it last waited for, ticks since

static void SetupDoorway(void) {
    doorwayTicks = doorwayContacts = doorwayWaitTicks = doorwayPassContacts = doorwayWaits = doorwayNods = 0;
    memset(doorwayNodding, 0, sizeof(doorwayNodding));
    for (int i = 0; i < MAX_MOVERS; i++) { doorwayLastFor[i] = -1; doorwaySince[i] = 1 << 20; }
    for (int y = 0; y < gridHeight; y++) {
        GRID(doorwayX, y, 1) = CELL_WALL;
        SetWallMaterial(doorwayX, y, 1, MAT_GRANITE);
    }
    GRID(doorwayX, doorwayY, 1) = CELL_DOOR;
    SetWallMaterial(doorwayX, doorwayY, 1, MAT_OAK);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 16;
    for (int i = 0; i < n; i++) SpawnMoverAt(i % 2 ? 20 + (i / 2) % 8 : 4 + (i / 2) % 8, 8 + (i / 2) * 2 % 16, 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Doorway: %d movers, two rooms, one door (let them pass %s)", n,
                          yieldLetPass ? "on" : "off"), GREEN);
}

static void DoorwayTickStats(void) {
    doorwayTicks++;
    float r = CELL_SIZE * 0.5f;
    float dcx = (doorwayX + 0.5f) * CELL_SIZE, dcy = (doorwayY + 0.5f) * CELL_SIZE, near = CELL_SIZE * 2.5f;
    for (int i = 0; i < moverCount; i++) {
        const Mover* a = &movers[i];
        if (!a->active) continue;
        bool nodding = MoverNodTime(i) > 0.0f;
        if (nodding && !doorwayNodding[i]) doorwayNods++;
        doorwayNodding[i] = nodding;
        int yf = MoverYieldingFor(i);
        if (yf >= 0) {
            if (doorwaySince[i] > 0) doorwayWaits++;   // a new wait
            doorwayWaitTicks++;
            doorwayLastFor[i] = yf;
            doorwaySince[i] = 0;
        } else if (doorwayLastFor[i] >= 0 && ++doorwaySince[i] <= TICK_RATE) {
            // the second after a wait ends: do the two bump while passing?
            const Mover* b = &movers[doorwayLastFor[i]];
            float dx = a->x - b->x, dy = a->y - b->y;
            if (b->active && dx * dx + dy * dy < r * r) doorwayPassContacts++;
        }
        if (a->pathIndex < 0) continue;
        if (fabsf(a->x - dcx) > near || fabsf(a->y - dcy) > near) continue;
        for (int j = i + 1; j < moverCount; j++) {
            const Mover* b = &movers[j];
            if (!b->active || b->pathIndex < 0) continue;
            float dx = a->x - b->x, dy = a->y - b->y;
            if (dx * dx + dy * dy < r * r) doorwayContacts++;
        }
    }
}

static void DoorwayPrintStats(void) {
    printf("Tunnel yielding: %s, let them pass: %s\n", useTunnelYielding ? "on" : "off", yieldLetPass ? "on" : "off");
    printf("Mover-ticks waiting at the door: %ld\n", doorwayWaitTicks);
    printf("\"After you\" nods: %ld\n", doorwayNods);
    printf("Waits: %ld; bumping the one it waited for in the second after: %ld ticks\n", doorwayWaits,
           doorwayPassContacts);
    printf("Close contacts near the door (walking pairs within half a cell): %ld ticks\n", doorwayContacts);
}

// gathering: a performer in the middle of the plaza, 30 movers wandering: onlookers come and
// stand in rings round it (talk.c, TALK_WATCH), walkers steer round the crowd.
static long gatherTicks, gatherWatchSum, gatherWatchMax, gatherRingCount[4], gatherIntrusions;

static void SetupGathering(void) {
    gatherTicks = gatherWatchSum = gatherWatchMax = gatherIntrusions = 0;
    memset(gatherRingCount, 0, sizeof(gatherRingCount));
    endlessMoverMode = true;
    noticeEnabled = true;
    talkEnabled = true;
    moverWanderPause = 2.0f;
    SpawnMoverAt(16, 16, 1);   // mover 0: the performer
    int n = scenarioMovers > 0 ? scenarioMovers : 30;
    for (int i = 0; i < n; i++) SpawnMoverAt(2 + (i % 6) * 5, 2 + (i / 6) * 6 % 28, 1);
    GatheringStart(0, 1e9f);
    currentViewZ = 1;
    AddMessage(TextFormat("Gathering: a performer and %d movers on a plaza", n), GREEN);
}

static void GatheringTickStats(void) {
    gatherTicks++;
    for (int t = 0; t < MAX_TALKS; t++) {
        const Talk* tk = &talks[t];
        if (!tk->active || tk->kind != TALK_WATCH) continue;
        gatherWatchSum += tk->count;
        if (tk->count > gatherWatchMax) gatherWatchMax = tk->count;
        for (int k = 0; k < tk->count; k++) {
            float dx = movers[tk->members[k]].x - tk->slotX[k], dy = movers[tk->members[k]].y - tk->slotY[k];
            if (dx * dx + dy * dy < 4.0f && tk->spot[k] / 64 < 4) gatherRingCount[tk->spot[k] / 64]++;
        }
        float in = 1.5f * CELL_SIZE * 0.8f;   // inside the first ring
        for (int i = 0; i < moverCount; i++) {
            if (!movers[i].active || MoverTalk(i) >= 0 || movers[i].pathIndex < 0) continue;
            float dx = movers[i].x - tk->centerX, dy = movers[i].y - tk->centerY;
            if (dx * dx + dy * dy < in * in) gatherIntrusions++;
        }
    }
}

static void GatheringPrintStats(void) {
    printf("Onlookers: avg %.1f, most %ld; came to watch: %ld times\n",
           gatherTicks ? gatherWatchSum / (double)gatherTicks : 0.0, gatherWatchMax, watchJoinedCount);
    printf("Standing in place by ring (mover-ticks): 1st %ld, 2nd %ld, 3rd %ld\n", gatherRingCount[0],
           gatherRingCount[1], gatherRingCount[2]);
    printf("Walkers inside the first ring (give room %s): %ld ticks\n", talkGiveRoom ? "on" : "off", gatherIntrusions);
}

static bool GatheringCheck(char* why, int whyLen) {
    if (watchJoinedCount == 0) { snprintf(why, (size_t)whyLen, "nobody came to watch"); return false; }
    return true;
}

// social-ties: 12 movers on the plaza with relations set up front: two groups of four friends
// (like 60), four strangers, and two pairs of enemies across the groups (like -60). Compare
// socialSeek / socialAvoid on/off: how far apart friends, enemies and strangers stay.
static double tiesFriendSum, tiesEnemySum, tiesStrangerSum;
static long tiesFriendN, tiesEnemyN, tiesStrangerN, tiesEnemyClose, tiesTicks;
static int tiesKind[16][16];   // 1 friends, -1 enemies, 0 strangers
static int tiesBiggestWalk;
static double tiesSlotDistSum; static long tiesSlotDistN;

static void SetupSocialTies(void) {
    tiesFriendSum = tiesEnemySum = tiesStrangerSum = 0.0;
    tiesFriendN = tiesEnemyN = tiesStrangerN = tiesEnemyClose = tiesTicks = 0;
    tiesBiggestWalk = 0;
    tiesSlotDistSum = 0.0; tiesSlotDistN = 0;
    memset(tiesKind, 0, sizeof(tiesKind));
    endlessMoverMode = true;
    noticeEnabled = true;
    talkEnabled = true;
    moverWanderPause = 3.0f;
    for (int i = 0; i < 12; i++) SpawnMoverAt(4 + (i % 4) * 7, 4 + (i / 4) * 10, 1);
    for (int g = 0; g < 2; g++)
        for (int a = g * 4; a < g * 4 + 4; a++)
            for (int b = a + 1; b < g * 4 + 4; b++) {
                RelationSetPair(a, b, 60.0f, 80.0f);
                tiesKind[a][b] = tiesKind[b][a] = 1;
            }
    RelationSetPair(0, 4, -60.0f, 60.0f); tiesKind[0][4] = tiesKind[4][0] = -1;
    RelationSetPair(1, 5, -60.0f, 60.0f); tiesKind[1][5] = tiesKind[5][1] = -1;
    currentViewZ = 1;
    AddMessage("Social ties: two groups of friends (0-3, 4-7), strangers 8-11, enemies 0-4 and 1-5", GREEN);
}

static void SocialTiesTickStats(void) {
    tiesTicks++;
    for (int t = 0; t < MAX_TALKS; t++)
        if (talks[t].active && talks[t].kind == TALK_WALK) {
            if (talks[t].count > tiesBiggestWalk) tiesBiggestWalk = talks[t].count;
            for (int k = 1; k < talks[t].count; k++) {   // how far each follower is from its spot
                float sx, sy;
                if (!TalkWalkSlot(t, k, &sx, &sy)) continue;
                const Mover* f = &movers[talks[t].members[k]];
                tiesSlotDistSum += sqrtf((f->x - sx) * (f->x - sx) + (f->y - sy) * (f->y - sy)) / CELL_SIZE;
                tiesSlotDistN++;
            }
        }
    for (int a = 0; a < 12 && a < moverCount; a++)
        for (int b = a + 1; b < 12 && b < moverCount; b++) {
            if (!movers[a].active || !movers[b].active) continue;
            float dx = movers[a].x - movers[b].x, dy = movers[a].y - movers[b].y;
            float d = sqrtf(dx * dx + dy * dy) / CELL_SIZE;
            if (tiesKind[a][b] == 1) { tiesFriendSum += d; tiesFriendN++; }
            else if (tiesKind[a][b] == -1) { tiesEnemySum += d; tiesEnemyN++; if (d < 1.5f) tiesEnemyClose++; }
            else { tiesStrangerSum += d; tiesStrangerN++; }
        }
}

static void SocialTiesPrintStats(void) {
    printf("Seek friends: %s, avoid the disliked: %s\n", socialSeek ? "on" : "off", socialAvoid ? "on" : "off");
    printf("Avg distance (cells): friends %.1f, strangers %.1f, enemies %.1f\n",
           tiesFriendN ? tiesFriendSum / tiesFriendN : 0.0, tiesStrangerN ? tiesStrangerSum / tiesStrangerN : 0.0,
           tiesEnemyN ? tiesEnemySum / tiesEnemyN : 0.0);
    printf("Enemy pairs within 1.5 cells: %ld pair-ticks\n", tiesEnemyClose);
    printf("Talks started: %ld, walks: %ld (joined by %ld more), biggest walking group: %d\n", talkStartedCount,
           walkStartedCount, walkJoinedCount, tiesBiggestWalk);
    printf("Walking followers: %.2f cells from their spot on average\n", tiesSlotDistN ? tiesSlotDistSum / tiesSlotDistN : 0.0);
    printf("Glances: %ld, greetings: %ld\n", glanceCount, greetCount);
}

// garden-party: a lawn with a stone patio, a campfire in the middle, two tables with chairs and a
// buffet (food stockpile). 14 guests who know each other (four couples, a trio of friends,
// acquaintances, one feud): they get hungry and eat at the buffet, talk, seek their friends,
// keep away from the one they fell out with, and gather round the fire.
#define GP_FIRE_X 16
#define GP_FIRE_Y 16
static long gpTicks, gpAtFireSum, gpAtFireMax, gpTalkSum, gpMeals, gpDarkTicks, gpDarkFireSum;
static long gpMealsSeated;
static double gpTableLikeSum, gpFireLikeSum; static long gpTableLikeN, gpFireLikeN;
static int gpLastDined[MAX_MOVERS];   // gpTicks when it last sat eating at a table
static long gpWalkTicks, gpWalkDistN; static double gpWalkDistSum; static long gpWalkSide, gpWalkBehind;
static float gpPrevHunger[MAX_MOVERS];

static void SetupGardenParty(void) {
    gpTicks = gpAtFireSum = gpAtFireMax = gpTalkSum = gpMeals = gpDarkTicks = gpDarkFireSum = 0;
    gpWalkTicks = gpWalkDistN = gpWalkSide = gpWalkBehind = 0; gpWalkDistSum = 0.0;
    gpMealsSeated = 0;
    gpTableLikeSum = gpFireLikeSum = 0.0; gpTableLikeN = gpFireLikeN = 0;
    for (int i = 0; i < MAX_MOVERS; i++) gpLastDined[i] = -100;
    hungerEnabled = true;
    thirstEnabled = energyEnabled = bladderEnabled = bodyTempEnabled = false;
    dayLength = 360.0f;                 // the games' pace: 6-minute day, walking 3x
    balance.baseMoverSpeed = 600.0f;    // (before the guests spawn: they take it from here)
    endlessMoverMode = true;
    noticeEnabled = talkEnabled = socialSeek = socialAvoid = true;
    moverWanderPause = 4.0f;
    // Patio: stone ground, no grass (nothing for the fire to catch on) round the fire
    for (int y = GP_FIRE_Y - 5; y <= GP_FIRE_Y + 5; y++)
        for (int x = GP_FIRE_X - 5; x <= GP_FIRE_X + 5; x++) {
            SetWallMaterial(x, y, 0, MAT_GRANITE);
            SetVegetation(x, y, 0, VEG_NONE);
        }
    CreateWorkshop(GP_FIRE_X, GP_FIRE_Y, 1, WORKSHOP_GROUND_FIRE);   // the campfire (its sprite)...
    SetFireSource(GP_FIRE_X, GP_FIRE_Y, 1, true);                      // ...burning for good
    // Two tables with chairs on the lawn, a buffet (food stockpile) between them
    for (int t = 0; t < 2; t++) {   // two tables, a chair on each side (eat sitting: sitToEat, table talk)
        int tx = t ? 24 : 7, ty = 7;
        SpawnFurniture(tx, ty, 1, FURNITURE_TABLE, MAT_OAK);
        SpawnFurniture(tx - 1, ty, 1, FURNITURE_CHAIR, MAT_OAK);
        SpawnFurniture(tx + 1, ty, 1, FURNITURE_CHAIR, MAT_OAK);
        SpawnFurniture(tx, ty - 1, 1, FURNITURE_CHAIR, MAT_OAK);
        SpawnFurniture(tx, ty + 1, 1, FURNITURE_CHAIR, MAT_OAK);
    }
    CreateStockpile(13, 5, 1, 6, 1);
    for (int i = 0; i < 6; i++) {
        int idx = SpawnItem((13 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 1,
                            i % 2 ? ITEM_BREAD : ITEM_DRIED_BERRIES);
        if (idx >= 0) items[idx].stackCount = 20;
    }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    for (int i = 0; i < 14; i++) SpawnMoverAt(4 + (i % 7) * 4, 24 + (i / 7) * 3, 1);
    for (int i = 0; i < moverCount; i++) {
        movers[i].hunger = 0.35f + 0.3f * (i % 5) / 4.0f;   // some peckish already
        gpPrevHunger[i] = movers[i].hunger;
    }
    for (int i = 0; i < moverCount; i++)            // everyone has met
        for (int j = i + 1; j < moverCount; j++) RelationSetPair(i, j, 10.0f, 30.0f);
    for (int c = 0; c < 8; c += 2) RelationSetPair(c, c + 1, 70.0f, 100.0f);   // four couples
    RelationSetPair(8, 9, 45.0f, 80.0f); RelationSetPair(8, 10, 45.0f, 80.0f); RelationSetPair(9, 10, 45.0f, 80.0f);
    RelationSetPair(0, 2, 35.0f, 70.0f); RelationSetPair(4, 6, 35.0f, 70.0f);   // couples who are friends
    RelationSetPair(3, 8, -45.0f, 60.0f);                                       // the feud
    GatheringStartAt((GP_FIRE_X + 0.5f) * CELL_SIZE, (GP_FIRE_Y + 0.5f) * CELL_SIZE, 1, 1e9f, "the fire", 30.0f, 90.0f);
    currentViewZ = 1;
    AddMessage("Garden party: a campfire, two tables, a buffet, 14 guests (4 couples, a trio, a feud: 3 and 8)", GREEN);
}

static void GardenPartyTickStats(void) {
    gpTicks++;
    int atFire = 0;
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        if (MoverIsWatching(i)) atFire++;
        if (MoverTalk(i) >= 0 && !MoverIsWatching(i) && !MoverIsWalkingWith(i)) gpTalkSum++;
        if (MoverIsWalkingWith(i)) gpWalkTicks++;
        if (MoverDiningAt(i) >= 0) gpLastDined[i] = (int)gpTicks;
        if (movers[i].hunger > gpPrevHunger[i] + 0.05f) {
            gpMeals++;
            if (gpTicks - gpLastDined[i] <= 2) gpMealsSeated++;   // eaten in a seat at a table
        }
        gpPrevHunger[i] = movers[i].hunger;
    }
    for (int t = 0; t < MAX_TALKS; t++) {   // walking pairs: how far apart, side by side or one behind
        const Talk* tk = &talks[t];
        if (!tk->active || tk->kind != TALK_WALK || tk->count < 2) continue;
        const Mover* L = &movers[tk->members[0]]; const Mover* F = &movers[tk->members[1]];
        float dx = F->x - L->x, dy = F->y - L->y;
        gpWalkDistSum += sqrtf(dx * dx + dy * dy) / CELL_SIZE; gpWalkDistN++;
        int li = tk->members[0];
        if (L->pathIndex >= 0) {
            Point wp = moverPaths[li][L->pathIndex];
            float hx = wp.x * CELL_SIZE + CELL_SIZE * 0.5f - L->x, hy = wp.y * CELL_SIZE + CELL_SIZE * 0.5f - L->y;
            float hl = sqrtf(hx * hx + hy * hy);
            if (hl > 0.5f) { if (fabsf((dx * hx + dy * hy) / hl) < CELL_SIZE * 0.4f) gpWalkSide++; else gpWalkBehind++; }
        }
    }
    // neighbours: diners at the same table, people next to each other round the fire (how they like them)
    for (int a = 0; a < moverCount; a++)
        for (int b = 0; b < moverCount; b++) {
            if (a == b || !movers[a].active || !movers[b].active) continue;
            if (MoverDiningAt(a) >= 0 && MoverDiningAt(a) == MoverDiningAt(b)) {
                gpTableLikeSum += RelationLike(a, b); gpTableLikeN++;
            } else if (MoverIsWatching(a) && MoverIsWatching(b) && MoverTalk(a) == MoverTalk(b)) {
                float dx = movers[a].x - movers[b].x, dy = movers[a].y - movers[b].y;
                if (dx * dx + dy * dy < (1.6f * CELL_SIZE) * (1.6f * CELL_SIZE)) { gpFireLikeSum += RelationLike(a, b); gpFireLikeN++; }
            }
        }
    gpAtFireSum += atFire;
    if (IsSkyDark()) { gpDarkTicks++; gpDarkFireSum += atFire; }
    if (atFire > gpAtFireMax) gpAtFireMax = atFire;
}

static void GardenPartyPrintStats(void) {
    int active = CountActiveMovers();
    printf("At the fire: avg %.1f, most %ld; in a chat (not the fire): %.0f%% of the time; meals: %ld\n",
           gpTicks ? gpAtFireSum / (double)gpTicks : 0.0, gpAtFireMax,
           gpTicks && active ? 100.0 * gpTalkSum / ((double)gpTicks * active) : 0.0, gpMeals);
    printf("At the fire by day: avg %.1f, after dark: avg %.1f\n",
           gpTicks > gpDarkTicks ? (gpAtFireSum - gpDarkFireSum) / (double)(gpTicks - gpDarkTicks) : 0.0,
           gpDarkTicks ? gpDarkFireSum / (double)gpDarkTicks : 0.0);
    printf("Talks started: %ld, joined: %ld; glances: %ld, greetings: %ld\n", talkStartedCount, talkJoinedCount,
           glanceCount, greetCount);
    printf("Spots by friends: %s; how much neighbours like each other: at a table %+.1f, next to each other at the fire %+.1f\n",
           spotByFriends ? "on" : "off", gpTableLikeN ? gpTableLikeSum / gpTableLikeN : 0.0,
           gpFireLikeN ? gpFireLikeSum / gpFireLikeN : 0.0);
    printf("Meals at a table: %ld of %ld (sit to eat %s); table talk: %.1f diner-minutes\n", gpMealsSeated, gpMeals,
           sitToEat ? "on" : "off", tableTalkTicks / (double)TICK_RATE / 60.0);
    printf("Walks: %ld started, %.0f%% of the time walking with someone; pair %.2f cells apart, side by side %.0f%%\n",
           walkStartedCount, gpTicks && active ? 100.0 * gpWalkTicks / ((double)gpTicks * active) : 0.0,
           gpWalkDistN ? gpWalkDistSum / gpWalkDistN : 0.0,
           gpWalkSide + gpWalkBehind ? 100.0 * gpWalkSide / (gpWalkSide + gpWalkBehind) : 0.0);
    int fuel = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) fuel += GetFuelAt(GP_FIRE_X + dx, GP_FIRE_Y + dy, 0) + GetFuelAt(GP_FIRE_X + dx, GP_FIRE_Y + dy, 1);
    printf("Fuel round the fire (should be 0 apart from the fire itself): %d\n", fuel);
}

static bool GardenPartyCheck(char* why, int whyLen) {
    if (gpMeals == 0) { snprintf(why, (size_t)whyLen, "nobody ate"); return false; }
    if (gpAtFireMax == 0) { snprintf(why, (size_t)whyLen, "nobody came to the fire"); return false; }
    return true;
}

// tag: a plaza with stone pillars, 8 players: "it" chases the nearest player it notices,
// the others run when it comes close (chase.c). Compare fleeSmart on/off: how long a chase
// lasts, how far the runners keep from "it".
static long tagTicks, tagFleeTicks, tagDistN;
static double tagDistSum;

static void SetupTag(void) {
    tagTicks = tagFleeTicks = tagDistN = 0;
    tagDistSum = 0.0;
    for (int py = 6; py < gridHeight - 4; py += 8)    // 2x2 pillars to dodge round
        for (int px = 6; px < gridWidth - 4; px += 8)
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    GRID(px + dx, py + dy, 1) = CELL_WALL;
                    SetWallMaterial(px + dx, py + dy, 1, MAT_GRANITE);
                }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    moverWanderPause = 1.0f;
    int n = scenarioMovers > 0 ? scenarioMovers : 8;
    for (int i = 0; i < n; i++) SpawnMoverAt(3 + (i % 4) * 8, 3 + (i / 4) * 8 % 28, 1);
    for (int i = 0; i < moverCount; i++) ChaseSetPlayer(i, true);
    ChaseSetIt(0);
    currentViewZ = 1;
    AddMessage(TextFormat("Tag: %d players, mover 0 is it (smart fleeing %s)", n, fleeSmart ? "on" : "off"), GREEN);
}

// play-hungry: a game of tag with hunger on and berries lying by the side of the field. Playing
// is a free-time state: a player that gets hungry leaves the game to eat; the game goes on with
// the others and ends when one is left. Check: players left, they ate, nobody starved.
static long phMeals, phPlayersEnd;
static float phLowestHunger;
static int phPrevState[MAX_MOVERS];

static void SetupPlayHungry(void) {
    phMeals = phPlayersEnd = 0;
    phLowestHunger = 1.0f;
    hungerEnabled = true;
    thirstEnabled = energyEnabled = bladderEnabled = bodyTempEnabled = false;
    dayLength = 360.0f;
    endlessMoverMode = true;
    noticeEnabled = true;
    CreateStockpile(2, 2, 1, 4, 4);
    for (int i = 0; i < 16; i++) {   // 4 px into the cell: guards food reach (MoverCanReach)
        int idx = SpawnItem((2 + i % 4) * CELL_SIZE + 4, (2 + i / 4) * CELL_SIZE + 4, 1, ITEM_DRIED_BERRIES);
        if (idx >= 0) items[idx].stackCount = 20;
    }
    int n = scenarioMovers > 0 ? scenarioMovers : 8;
    for (int i = 0; i < n; i++) {
        SpawnMoverAt(12 + (i % 4) * 3, 12 + (i / 4) * 4, 1);
        movers[moverCount - 1].hunger = 0.33f + 0.05f * i;   // hungry one after the other
        phPrevState[moverCount - 1] = FREETIME_NONE;
    }
    for (int i = 0; i < moverCount; i++) ChaseSetPlayer(i, true);
    ChaseSetIt(0);
    currentViewZ = 1;
    AddMessage(TextFormat("Tag with hunger on: %d players leave the game to eat (berries top left)", n), GREEN);
}

static void PlayHungryTickStats(void) {
    phPlayersEnd = 0;
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        if (movers[i].freetimeState == FREETIME_EATING && phPrevState[i] != FREETIME_EATING) phMeals++;
        phPrevState[i] = movers[i].freetimeState;
        if (movers[i].hunger < phLowestHunger) phLowestHunger = movers[i].hunger;
        if (ChaseIsPlaying(i)) phPlayersEnd++;
    }
}

static void PlayHungryPrintStats(void) {
    printf("Play: left the game %ld times, meals %ld, still playing %ld, lowest hunger %.2f\n",
           playLeftCount, phMeals, phPlayersEnd, phLowestHunger);
}

static bool PlayHungryCheck(char* why, int whyLen) {
    if (playLeftCount == 0) { snprintf(why, (size_t)whyLen, "nobody left the game to eat"); return false; }
    if (phMeals == 0) { snprintf(why, (size_t)whyLen, "nobody ate"); return false; }
    // 0.1: food out of reach (a mover standing by it until the seek timed out) gave 0.05
    if (phLowestHunger <= 0.1f) { snprintf(why, (size_t)whyLen, "someone went hungry while playing (%.2f)", phLowestHunger); return false; }
    return true;
}

static void TagTickStats(void) {
    tagTicks++;
    int it = ChaseIt();
    if (it < 0) return;
    for (int i = 0; i < moverCount; i++) {
        if (!ChaseIsPlaying(i) || i == it) continue;
        if (ChaseIsFleeing(i)) tagFleeTicks++;
        float dx = movers[i].x - movers[it].x, dy = movers[i].y - movers[it].y;
        tagDistSum += sqrtf(dx * dx + dy * dy) / CELL_SIZE;
        tagDistN++;
    }
}

static void TagPrintStats(void) {
    double minutes = tagTicks / (double)TICK_RATE / 60.0;
    printf("Smart fleeing: %s; tags: %ld (%.1f per minute), a chase lasts %.1f s on average\n", fleeSmart ? "on" : "off",
           tagCount, minutes > 0 ? tagCount / minutes : 0.0, tagCount ? tagChaseSecondsSum / tagCount : 0.0);
    printf("Runners: %.0f%% of the time fleeing, %.1f cells from it on average\n",
           tagDistN ? 100.0 * tagFleeTicks / tagDistN : 0.0, tagDistN ? tagDistSum / tagDistN : 0.0);
}

static bool TagCheck(char* why, int whyLen) {
    if (tagCount == 0) { snprintf(why, (size_t)whyLen, "nobody got tagged"); return false; }
    return true;
}

// hide-and-seek: a yard with wall pieces, corners, little huts and pillars; 8 players, the
// seeker counts in the middle (chase.c). Compare hideSmart on/off: how long hiders stay hidden.
static long hsTicks, hsHiddenSeenFromHome, hsHiddenTicks;

static void HsWall(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            GRID(x, y, 1) = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_OAK);
        }
}

static void SetupHideAndSeek(void) {
    hsTicks = hsHiddenSeenFromHome = hsHiddenTicks = 0;
    HsWall(4, 4, 9, 4);   HsWall(4, 5, 4, 8);          // an L corner
    HsWall(22, 4, 27, 4); HsWall(27, 5, 27, 8);        // and another
    HsWall(4, 22, 4, 27); HsWall(5, 27, 9, 27);
    HsWall(22, 27, 27, 27); HsWall(27, 22, 27, 26);
    // two huts (3x3 inside) with one opening each
    HsWall(12, 2, 16, 2); HsWall(12, 3, 12, 6); HsWall(16, 3, 16, 6); HsWall(12, 6, 13, 6); HsWall(15, 6, 16, 6);
    HsWall(15, 25, 19, 25); HsWall(15, 26, 15, 29); HsWall(19, 26, 19, 29); HsWall(15, 29, 19, 29);
    GRID(17, 25, 1) = CELL_AIR;   // its door
    HsWall(9, 13, 9, 19); HsWall(23, 13, 23, 19);       // long walls either side
    HsWall(13, 10, 14, 11); HsWall(19, 20, 20, 21);     // pillars
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 8;
    SpawnMoverAt(16, 16, 1);   // mover 0: the first seeker
    for (int i = 1; i < n; i++) SpawnMoverAt(14 + i % 5, 14 + (i / 5) * 4, 1);
    for (int i = 0; i < moverCount; i++) ChaseSetPlayer(i, true);
    HideSeekStart(0);
    currentViewZ = 1;
    AddMessage(TextFormat("Hide and seek: %d players (smart hiding %s)", n, hideSmart ? "on" : "off"), GREEN);
}

static void HideAndSeekTickStats(void) {
    hsTicks++;
    int s = HideSeeker();
    if (s < 0) return;
    for (int i = 0; i < moverCount; i++) {
        if (!HideIsHidden(i)) continue;
        hsHiddenTicks++;
        int x = (int)(movers[i].x / CELL_SIZE), y = (int)(movers[i].y / CELL_SIZE);
        if (HasLineOfSight(16, 16, x, y, 1)) hsHiddenSeenFromHome++;
    }
}

static void HideAndSeekPrintStats(void) {
    printf("Smart hiding: %s; rounds: %ld, found: %ld, found after %.1f s of seeking on average\n",
           hideSmart ? "on" : "off", hideRounds, hideFoundCount,
           hideFoundCount ? hideFoundSecondsSum / hideFoundCount : 0.0);
    printf("Hidden in sight of home (the middle): %.0f%% of hidden time\n",
           hsHiddenTicks ? 100.0 * hsHiddenSeenFromHome / hsHiddenTicks : 0.0);
}

static bool HideAndSeekCheck(char* why, int whyLen) {
    if (hideFoundCount == 0) { snprintf(why, (size_t)whyLen, "nobody was found"); return false; }
    return true;
}

// mingle: 20 guests on a lawn: two circles of six friends, eight strangers, four friendships
// across the circles. Talks form and break up; with mingling people move between groups.
// Compare setting mingle on/off: switches, how many different people each guest talks with,
// time spent in a group.
#define MINGLE_GUESTS 20
static long mgTicks, mgInGroup;
static unsigned char mgTalked[MINGLE_GUESTS][MINGLE_GUESTS];

static void SetupMingle(void) {
    mgTicks = mgInGroup = 0;
    memset(mgTalked, 0, sizeof(mgTalked));
    endlessMoverMode = true;
    noticeEnabled = talkEnabled = true;
    walkChance = 0.0f;                    // walks off: mingling on its own
    moverWanderPause = 3.0f;
    talkChance = 0.1f;
    for (int i = 0; i < MINGLE_GUESTS; i++) SpawnMoverAt(4 + (i % 5) * 6, 4 + (i / 5) * 7, 1);
    for (int g = 0; g < 2; g++)
        for (int a = g * 6; a < g * 6 + 6; a++)
            for (int b = a + 1; b < g * 6 + 6; b++) RelationSetPair(a, b, 50.0f, 80.0f);
    RelationSetPair(0, 6, 45.0f, 70.0f); RelationSetPair(1, 7, 45.0f, 70.0f);
    RelationSetPair(2, 12, 40.0f, 60.0f); RelationSetPair(8, 15, 40.0f, 60.0f);
    currentViewZ = 1;
    AddMessage("Mingle: two circles of friends (0-5, 6-11), strangers 12-19, a few friendships across", GREEN);
}

static void MingleTickStats(void) {
    mgTicks++;
    for (int a = 0; a < MINGLE_GUESTS && a < moverCount; a++) {
        int t = MoverTalk(a);
        if (t < 0 || talks[t].kind == TALK_WALK) continue;
        mgInGroup++;
        for (int k = 0; k < talks[t].count; k++) {
            int b = talks[t].members[k];
            if (b != a && b < MINGLE_GUESTS) mgTalked[a][b] = 1;
        }
    }
}

static void MinglePrintStats(void) {
    int partners = 0, n = moverCount < MINGLE_GUESTS ? moverCount : MINGLE_GUESTS;
    for (int a = 0; a < n; a++) for (int b = 0; b < n; b++) partners += mgTalked[a][b];
    printf("Mingle: %s; moves to another group: %ld; talks started: %ld\n", mingleEnabled ? "on" : "off", mingleCount,
           talkStartedCount);
    printf("Different people each guest talked with: %.1f on average; time in a group: %.0f%%\n",
           n ? partners / (double)n : 0.0, mgTicks && n ? 100.0 * mgInGroup / ((double)mgTicks * n) : 0.0);
}

// personal-space: 16 on a lawn, 0-7 old friends, 8-15 strangers to everyone. They wander,
// talk and pass each other. Compare setting personalSpace on/off: how far apart talkers stand,
// and how often two walkers come within a cell when they pass, friends vs strangers.
enum { PS_FRIENDS, PS_KNOWN, PS_STRANGERS, PS_CLASSES };
static double psTalkDist[PS_CLASSES];
static long psTalkN[PS_CLASSES], psNear[PS_CLASSES], psClose[PS_CLASSES];

static int PsClass(int a, int b) {
    float like = RelationLike(a, b), fam = RelationFamiliarity(a, b);
    if (like >= 20.0f) return PS_FRIENDS;
    return fam < 20.0f ? PS_STRANGERS : PS_KNOWN;
}

static void SetupPersonalSpace(void) {
    memset(psTalkDist, 0, sizeof(psTalkDist));
    memset(psTalkN, 0, sizeof(psTalkN)); memset(psNear, 0, sizeof(psNear)); memset(psClose, 0, sizeof(psClose));
    endlessMoverMode = true;
    noticeEnabled = talkEnabled = true;
    walkChance = 0.0f;
    moverWanderPause = 2.0f;
    talkChance = 0.1f;
    for (int i = 0; i < 16; i++) SpawnMoverAt(4 + (i % 4) * 7, 4 + (i / 4) * 7, 1);
    for (int a = 0; a < 8; a++)
        for (int b = a + 1; b < 8; b++) RelationSetPair(a, b, 50.0f, 80.0f);
    currentViewZ = 1;
    AddMessage(TextFormat("Personal space %s: friends 0-7, strangers 8-15", personalSpace ? "on" : "off"), GREEN);
}

static void PersonalSpaceTickStats(void) {
    for (int a = 0; a < moverCount; a++) {
        if (!movers[a].active) continue;
        int ta = MoverTalk(a);
        for (int b = a + 1; b < moverCount; b++) {
            if (!movers[b].active || (int)movers[b].z != (int)movers[a].z) continue;
            float dx = (movers[b].x - movers[a].x) / CELL_SIZE, dy = (movers[b].y - movers[a].y) / CELL_SIZE;
            float d = sqrtf(dx * dx + dy * dy);
            int c = PsClass(a, b);
            if (ta >= 0 && MoverTalk(b) == ta && talks[ta].kind == TALK_CHAT && talks[ta].count == 2 &&
                movers[a].pathIndex < 0 && movers[b].pathIndex < 0) {   // a pair, both in their spot
                psTalkDist[c] += d; psTalkN[c]++;
            } else if (ta < 0 && MoverTalk(b) < 0 && movers[a].pathIndex >= 0 && movers[b].pathIndex >= 0 && d < 2.5f) {
                psNear[c]++;                   // two walkers within 2.5 cells ...
                if (d < 1.0f) psClose[c]++;    // ... and within one
            }
        }
    }
}

static void PersonalSpacePrintStats(void) {
    static const char* names[PS_CLASSES] = {"friends", "known", "strangers"};
    printf("Personal space: %s\n", personalSpace ? "on" : "off");
    for (int c = 0; c < PS_CLASSES; c++)
        printf("  %-9s talking pair %.2f cells apart (%ld ticks); passing: within 1 cell %.0f%% of the time within 2.5 (%ld ticks)\n",
               names[c], psTalkN[c] ? psTalkDist[c] / psTalkN[c] : 0.0, psTalkN[c],
               psNear[c] ? 100.0 * psClose[c] / psNear[c] : 0.0, psNear[c]);
}

// campfire: a ground fire burning for good and a burning barrel on a lawn, 12 wanderers: the
// world starts gatherings round them by itself (game rule autoGatherings, talk.c).
static long cfTicks, cfAtFire, cfAtBarrel;

static void SetupCampfire(void) {
    cfTicks = cfAtFire = cfAtBarrel = 0;
    endlessMoverMode = true;
    noticeEnabled = true;
    autoGatherings = true;
    moverWanderPause = 3.0f;
    for (int y = 10; y <= 18; y++)          // fuel-free ground round the fire
        for (int x = 6; x <= 14; x++) { SetWallMaterial(x, y, 0, MAT_GRANITE); SetVegetation(x, y, 0, VEG_NONE); }
    int ws = CreateWorkshop(10, 14, 1, WORKSHOP_GROUND_FIRE);
    if (ws >= 0 && workshops[ws].fuelTileX >= 0)
        SetFireSource(workshops[ws].fuelTileX, workshops[ws].fuelTileY, 1, true);
    SpawnFurniture(24, 14, 1, FURNITURE_FIRE_BARREL, MAT_IRON);
    for (int k = 0; k < 2; k++) {   // sticks by the barrel: it burns fuel (2 h a bundle), the wanderers top it up
        int sticks = SpawnItem(26.5f * CELL_SIZE, (14.5f + k) * CELL_SIZE, 1.0f, ITEM_STICKS);
        if (sticks >= 0) items[sticks].stackCount = 20;
    }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    for (int i = 0; i < 12; i++) SpawnMoverAt(4 + (i % 6) * 4, 4 + (i / 6) * 20, 1);
    currentViewZ = 1;
    AddMessage("Campfire: a fire and a burning barrel; people gather round them by themselves", GREEN);
}

static void CampfireTickStats(void) {
    cfTicks++;
    for (int i = 0; i < moverCount; i++) {
        int t = MoverTalk(i);
        if (t < 0 || !MoverIsWatching(i)) continue;
        if (talks[t].centerX < 17 * CELL_SIZE) cfAtFire++; else cfAtBarrel++;
    }
}

static void CampfirePrintStats(void) {
    printf("Gatherings started by the world: %ld; round the fire avg %.1f, round the barrel avg %.1f\n",
           autoGatheringsStarted, cfTicks ? cfAtFire / (double)cfTicks : 0.0, cfTicks ? cfAtBarrel / (double)cfTicks : 0.0);
}

static bool CampfireCheck(char* why, int whyLen) {
    if (autoGatheringsStarted < 2) { snprintf(why, (size_t)whyLen, "no gathering round the fire and the barrel"); return false; }
    return true;
}

// follow-leader: the tag plaza (pillars), 6 players in a line: the leader winds round, the others
// follow its route (chase.c). Compare followTrail on/off: how far followers stray from the route.
static long flTicks, flDistN, flGapN;
static double flDistSum, flGapSum, flDistMax;

static void SetupFollowLeader(void) {
    flTicks = flDistN = flGapN = 0;
    flDistSum = flGapSum = flDistMax = 0.0;
    for (int py = 6; py < gridHeight - 4; py += 8)
        for (int px = 6; px < gridWidth - 4; px += 8)
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    GRID(px + dx, py + dy, 1) = CELL_WALL;
                    SetWallMaterial(px + dx, py + dy, 1, MAT_GRANITE);
                }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 6, players[FOLLOW_MAX];
    if (n > FOLLOW_MAX) n = FOLLOW_MAX;
    for (int i = 0; i < n; i++) { SpawnMoverAt(4 + i, 4, 1); players[i] = moverCount - 1; }
    FollowLeaderStart(players, n);
    currentViewZ = 1;
    AddMessage(TextFormat("Follow the leader: %d players (following the trail %s)", n, followTrail ? "on" : "off"), GREEN);
}

static void FollowLeaderTickStats(void) {
    flTicks++;
    for (int i = 0; i < moverCount; i++) {
        float d;
        if (FollowTrailDistance(i, &d)) { flDistSum += d; flDistN++; if (d > flDistMax) flDistMax = d; }
        int k = FollowPlace(i);
        if (k > 0) {
            for (int j = 0; j < moverCount; j++)
                if (FollowPlace(j) == k - 1) {
                    float dx = movers[i].x - movers[j].x, dy = movers[i].y - movers[j].y;
                    flGapSum += sqrtf(dx * dx + dy * dy) / CELL_SIZE; flGapN++;
                }
        }
    }
}

static void FollowLeaderPrintStats(void) {
    printf("Following the trail: %s; followers %.2f cells from the leader's route on average (worst %.1f)\n",
           followTrail ? "on" : "off", flDistN ? flDistSum / flDistN : 0.0, flDistMax);
    printf("Gap to the one ahead: %.2f cells on average; leader changes: %ld\n", flGapN ? flGapSum / flGapN : 0.0,
           followLeaderChanges);
}

// pack-hunt: an arena with pillars and railings (half-height: see across, can't cross), 4
// hunters on 1 runner (chase.c). The runner is faster when fresh and tires; caught, it turns up
// again far away. Compare packTactics on/off: seconds to a catch.
static void PhWall(int x0, int y0, int x1, int y1, CellType c) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) { GRID(x, y, 1) = c; SetWallMaterial(x, y, 1, c == CELL_RAILING ? MAT_OAK : MAT_GRANITE); }
}

static void SetupPackHunt(void) {
    PhWall(7, 7, 8, 8, CELL_WALL);   PhWall(23, 7, 24, 8, CELL_WALL);
    PhWall(7, 23, 8, 24, CELL_WALL); PhWall(23, 23, 24, 24, CELL_WALL);
    PhWall(15, 15, 16, 16, CELL_WALL);                       // pillars
    PhWall(11, 4, 20, 4, CELL_RAILING);  PhWall(11, 27, 20, 27, CELL_RAILING);
    PhWall(4, 11, 4, 20, CELL_RAILING);  PhWall(27, 11, 27, 20, CELL_RAILING);   // railings
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    moverWanderPause = 1.0f;
    int hunters[4];
    for (int k = 0; k < 4; k++) { SpawnMoverAt(2 + k, 2, 1); hunters[k] = moverCount - 1; }
    SpawnMoverAt(16, 12, 1);
    for (int k = 0; k < 4; k++) ChaseSetPlayer(hunters[k], true);
    ChaseSetPlayer(moverCount - 1, true);
    PackHuntStart(hunters, 4, moverCount - 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Pack hunt: 4 hunters (0-3) on 1 runner (4), tactics %s", packTactics ? "on" : "off"), GREEN);
}

static void PackHuntPrintStats(void) {
    printf("Pack tactics: %s; catches: %ld, a catch takes %.1f s on average\n", packTactics ? "on" : "off",
           packCatches, packCatches ? packCatchSecondsSum / packCatches : 0.0);
    printf("Only by sight: %s; got away: %ld; ducked behind cover: %ld (crouched %.1f s each), flushed out: %ld\n",
           packStealth ? "on" : "off", packEscapes, packDucks, packDucks ? packDuckSecondsSum / packDucks : 0.0, packFlushed);
}

static bool PackHuntCheck(char* why, int whyLen) {
    if (packCatches == 0) { snprintf(why, (size_t)whyLen, "the pack never caught the runner"); return false; }
    return true;
}

// cover: hide and seek in a garden with only low cover (railing pieces, bush clumps, tables):
// nothing to hide behind but things you can see over. Hiders crouch in their spot. Compare
// setting cover on/off: how long hiders stay hidden, and how much of their hidden time the
// seeker had them in plain sight (in range and line of sight, but they were behind cover).
static long cvHiddenTicks, cvInSightTicks, cvCoveredTicks;

static void SetupCover(void) {
    cvHiddenTicks = cvInSightTicks = cvCoveredTicks = 0;
    PhWall(5, 6, 8, 6, CELL_RAILING);   PhWall(23, 6, 26, 6, CELL_RAILING);
    PhWall(5, 25, 8, 25, CELL_RAILING); PhWall(23, 25, 26, 25, CELL_RAILING);
    PhWall(13, 9, 18, 9, CELL_RAILING); PhWall(13, 22, 18, 22, CELL_RAILING);
    PhWall(9, 13, 9, 18, CELL_RAILING); PhWall(22, 13, 22, 18, CELL_RAILING);
    static const int bushes[][2] = {{3, 15}, {3, 16}, {4, 16}, {28, 15}, {28, 16}, {27, 15},
                                    {15, 3}, {16, 3}, {16, 4}, {15, 28}, {16, 28}, {15, 27},
                                    {11, 11}, {20, 20}, {11, 20}, {20, 11}};
    for (int k = 0; k < (int)(sizeof(bushes) / sizeof(bushes[0])); k++) GRID(bushes[k][0], bushes[k][1], 1) = CELL_BUSH;
    BuildEntrances();
    BuildGraph();
    SpawnFurniture(6, 11, 1, FURNITURE_TABLE, MAT_OAK);  SpawnFurniture(25, 11, 1, FURNITURE_TABLE, MAT_OAK);
    SpawnFurniture(6, 20, 1, FURNITURE_TABLE, MAT_OAK);  SpawnFurniture(25, 20, 1, FURNITURE_TABLE, MAT_OAK);
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 8;
    SpawnMoverAt(16, 16, 1);   // mover 0: the first seeker
    for (int i = 1; i < n; i++) SpawnMoverAt(14 + i % 5, 14 + (i / 5) * 4, 1);
    for (int i = 0; i < moverCount; i++) ChaseSetPlayer(i, true);
    HideSeekStart(0);
    currentViewZ = 1;
    AddMessage(TextFormat("Cover: hide and seek with only low cover, %d players (crouching hides: %s)", n, coverEnabled ? "on" : "off"), GREEN);
}

static void CoverTickStats(void) {
    int s = HideSeeker();
    if (s < 0 || HideIsCounting(s)) return;
    for (int i = 0; i < moverCount; i++) {
        if (!HideIsHidden(i)) continue;
        cvHiddenTicks++;
        float dx = (movers[i].x - movers[s].x) / CELL_SIZE, dy = (movers[i].y - movers[s].y) / CELL_SIZE;
        if (dx * dx + dy * dy > noticeRadius * noticeRadius) continue;
        int sx = (int)(movers[s].x / CELL_SIZE), sy = (int)(movers[s].y / CELL_SIZE);
        if (!CanSeeCell(sx, sy, (int)(movers[i].x / CELL_SIZE), (int)(movers[i].y / CELL_SIZE), 1)) continue;
        cvInSightTicks++;   // the seeker is in range with a clear line
        if (!CanSeeMover(s, i)) cvCoveredTicks++;   // ... but it's crouched behind cover
    }
}

static void CoverPrintStats(void) {
    printf("Cover: %s; rounds: %ld, found: %ld, found after %.1f s of seeking on average\n",
           coverEnabled ? "on" : "off", hideRounds, hideFoundCount,
           hideFoundCount ? hideFoundSecondsSum / hideFoundCount : 0.0);
    printf("Seeker in range with a clear line: %.1f%% of hidden time; of that, behind cover: %.0f%%\n",
           cvHiddenTicks ? 100.0 * cvInSightTicks / cvHiddenTicks : 0.0,
           cvInSightTicks ? 100.0 * cvCoveredTicks / cvInSightTicks : 0.0);
}

// hunt-cover: the pack hunt in an overgrown garden: wall pieces to break sight, bush clumps,
// railings and tables to duck behind. Compare packStealth on/off: catches, how long a catch
// takes, how often the runner ducks, is flushed out, gets away.
static long hcTicks, hcLost;
static void HuntCoverTickStats(void) {
    hcTicks++;
    if (PackLost(NULL)) hcLost++;
}

static void HuntCoverPrintStats(void) {
    PackHuntPrintStats();
    printf("Out of the pack's sight: %.1f%% of the time\n", hcTicks ? 100.0 * hcLost / hcTicks : 0.0);
}

static void SetupHuntCover(void) {
    hcTicks = hcLost = 0;
    PhWall(6, 6, 6, 10, CELL_WALL);    PhWall(25, 21, 25, 25, CELL_WALL);
    PhWall(12, 14, 16, 14, CELL_WALL); PhWall(17, 19, 21, 19, CELL_WALL);
    PhWall(21, 5, 25, 5, CELL_WALL);   PhWall(6, 26, 10, 26, CELL_WALL);   // wall pieces
    PhWall(10, 9, 13, 9, CELL_RAILING); PhWall(19, 24, 22, 24, CELL_RAILING);
    PhWall(27, 11, 27, 15, CELL_RAILING); PhWall(3, 17, 3, 21, CELL_RAILING);   // railings
    static const int bushes[][2] = {{8, 8}, {9, 8}, {8, 9}, {23, 7}, {24, 7}, {23, 8},
                                    {7, 23}, {8, 23}, {8, 24}, {23, 22}, {23, 23}, {22, 23},
                                    {15, 16}, {16, 17}, {16, 16}, {4, 12}, {28, 19}, {14, 28}, {18, 3}};
    for (int k = 0; k < (int)(sizeof(bushes) / sizeof(bushes[0])); k++) GRID(bushes[k][0], bushes[k][1], 1) = CELL_BUSH;
    for (int k = 0; k < 6; k++) {   // hedges along the wall pieces
        GRID(7, 6 + k, 1) = CELL_BUSH;  GRID(24, 20 + k, 1) = CELL_BUSH;
        GRID(11 + k, 15, 1) = CELL_BUSH; GRID(16 + k, 18, 1) = CELL_BUSH;
    }
    BuildEntrances();
    BuildGraph();
    SpawnFurniture(12, 21, 1, FURNITURE_TABLE, MAT_OAK); SpawnFurniture(20, 11, 1, FURNITURE_TABLE, MAT_OAK);
    InvalidateLighting();
    endlessMoverMode = true;
    noticeEnabled = true;
    moverWanderPause = 1.0f;
    int hunters[4];
    for (int k = 0; k < 4; k++) { SpawnMoverAt(2 + k, 2, 1); hunters[k] = moverCount - 1; }
    SpawnMoverAt(16, 12, 1);
    for (int k = 0; k < 4; k++) ChaseSetPlayer(hunters[k], true);
    ChaseSetPlayer(moverCount - 1, true);
    PackHuntStart(hunters, 4, moverCount - 1);
    currentViewZ = 1;
    AddMessage(TextFormat("Hunt in cover: 4 hunters (0-3) on 1 runner (4), only by sight %s", packStealth ? "on" : "off"), GREEN);
}

// Six friends on a lawn with hiding places, food in a corner: they start games by themselves
// (autoGames: tag, hide and seek, follow the leader), leave them to eat, play again later
static long ptTicks, ptPlaying, ptMeals;
static float ptLowestHunger;
static int ptPrevState[MAX_MOVERS];

static void SetupPlaytime(void) {
    ptTicks = ptPlaying = ptMeals = 0;
    ptLowestHunger = 1.0f;
    hungerEnabled = true;
    thirstEnabled = energyEnabled = bladderEnabled = bodyTempEnabled = false;
    dayLength = 360.0f;
    endlessMoverMode = true;
    moverWanderPause = 1.0f;
    noticeEnabled = true;
    talkEnabled = true;
    autoGatherings = false;
    autoGames = true;
    PhWall(9, 9, 9, 13, CELL_WALL); PhWall(20, 18, 24, 18, CELL_WALL);   // places to hide behind
    PhWall(14, 24, 18, 24, CELL_RAILING);
    int bushes[][2] = {{6, 20}, {7, 20}, {25, 8}, {25, 9}, {16, 6}, {28, 26}};
    for (int k = 0; k < (int)(sizeof(bushes) / sizeof(bushes[0])); k++) GRID(bushes[k][0], bushes[k][1], 1) = CELL_BUSH;
    SpawnFurniture(13, 15, 1, FURNITURE_TABLE, MAT_OAK);
    InvalidateLighting();
    CreateStockpile(2, 2, 1, 3, 3);
    for (int i = 0; i < 9; i++) {
        int idx = SpawnItem((2 + i % 3) * CELL_SIZE + 4, (2 + i / 3) * CELL_SIZE + 4, 1, ITEM_DRIED_BERRIES);
        if (idx >= 0) items[idx].stackCount = 20;
    }
    int n = scenarioMovers > 0 ? scenarioMovers : 6;
    int first = moverCount;
    for (int i = 0; i < n; i++) {
        SpawnMoverAt(13 + (i % 3) * 2, 11 + (i / 3) * 2, 1);
        movers[moverCount - 1].hunger = 0.45f + 0.08f * (i % 6);
        ptPrevState[moverCount - 1] = FREETIME_NONE;
    }
    for (int a = first; a < moverCount; a++)   // they all know and like each other
        for (int b = a + 1; b < moverCount; b++) RelationSetPair(a, b, 60.0f, 60.0f);
    currentViewZ = 1;
    AddMessage(TextFormat("Playtime: %d friends start games by themselves (autoGames %s)", n, autoGames ? "on" : "off"), GREEN);
}

static void PlaytimeTickStats(void) {
    ptTicks++;
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        if (movers[i].freetimeState == FREETIME_EATING && ptPrevState[i] != FREETIME_EATING) ptMeals++;
        ptPrevState[i] = movers[i].freetimeState;
        if (movers[i].hunger < ptLowestHunger) ptLowestHunger = movers[i].hunger;
        if (MoverActivity(i) >= 0) ptPlaying++;
    }
}

static void PlaytimePrintStats(void) {
    long starts = 0;
    for (int k = 0; k < ACT_KIND_COUNT; k++) starts += autoGameStarts[k];
    printf("Games by themselves: %s; started %ld (tag %ld, hide and seek %ld, follow the leader %ld), declined %ld\n",
           autoGames ? "on" : "off", starts, autoGameStarts[ACT_TAG], autoGameStarts[ACT_HIDE_SEEK],
           autoGameStarts[ACT_FOLLOW_LEADER], autoGameDeclined);
    printf("Ended %ld, lasting %.0f s on average; playing %.0f%% of the time; left a game %ld times\n",
           autoGameEnded, autoGameEnded ? autoGameSecondsSum / autoGameEnded : 0.0,
           ptTicks && moverCount ? 100.0 * ptPlaying / ((double)ptTicks * moverCount) : 0.0, playLeftCount);
    printf("Meals %ld, lowest hunger %.2f, talks started %ld\n", ptMeals, ptLowestHunger, talkStartedCount);
}

static bool PlaytimeCheck(char* why, int whyLen) {
    int kinds = 0;
    long starts = 0;
    for (int k = 0; k < ACT_KIND_COUNT; k++) { starts += autoGameStarts[k]; if (autoGameStarts[k] > 0) kinds++; }
    bool day = ptTicks >= 21600;   // a game day (dayLength 360): several games, more than one kind
    if (starts < (day ? 3 : 1)) { snprintf(why, (size_t)whyLen, "only %ld games started", starts); return false; }
    if (day && kinds < 2) { snprintf(why, (size_t)whyLen, "only one kind of game played"); return false; }
    if (day && ptMeals == 0) { snprintf(why, (size_t)whyLen, "nobody ate"); return false; }
    if (ptLowestHunger <= 0.1f) { snprintf(why, (size_t)whyLen, "someone went hungry (%.2f)", ptLowestHunger); return false; }
    return true;
}

static void SetupLadders(void) {
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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

// ladder-oneway: the deck with an up-only ladder (18,16) and a down-only one (18,6),
// 16 movers wandering between the levels: every climb up uses the first, every climb
// down the second (--check fails on a climb against the direction).
static void SetupLadderOneWay(void) {
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
    ResetLadderStats();
    BuildLadderDeck();
    AddLadderAt(18, 16);
    AddLadderAt(18, 6);
    SetLadderOneWay(18, 16, 1, ONEWAY_UP);
    SetLadderOneWay(18, 6, 1, ONEWAY_DOWN);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    preferDifferentZ = true;
    if (scenarioMovers > 0) SpawnGroundCrowd(scenarioMovers);
    else for (int i = 0; i < 16; i++) SpawnMoverAt(4 + (i % 8) * 2, 10 + (i / 8) * 10, 1);
    currentViewZ = 1;
    AddMessage("One-way ladders: up at (18,16), down at (18,6), 16 movers", GREEN);
}

// oneway-tiers: a stepped tower of plank decks east of the ground (z=2 from x=10, z=3 from
// x=16, z=4 from x=22). Each tier joins the next by an up-only ladder on the north side and
// a down-only ladder on the south side, and a down-only "fire pole" drops from the top deck
// to the ground (28,16). Movers wander between levels: up the north ladders tier by tier,
// down the south ladders or the pole. --check fails on a climb against a ladder.
static void SetupOneWayTiers(void) {
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
    ResetLadderStats();
    static const int tierX[3] = {10, 16, 22};
    for (int t = 0; t < 3; t++) {
        int z = 2 + t;
        for (int y = 4; y < 28; y++)
            for (int x = tierX[t]; x < 30; x++) {
                SET_FLOOR(x, y, z);
                SetFloorMaterial(x, y, z, t == 0 ? MAT_OAK : t == 1 ? MAT_PINE : MAT_BIRCH);
            }
        // ladders at the deck's west edge, from the level below (z-1) up onto it (z)
        GRID(tierX[t], 6, z - 1) = CELL_LADDER_BOTH;  GRID(tierX[t], 6, z) = CELL_LADDER_BOTH;
        GRID(tierX[t], 25, z - 1) = CELL_LADDER_BOTH; GRID(tierX[t], 25, z) = CELL_LADDER_BOTH;
        RecalculateLadderColumn(tierX[t], 6);
        RecalculateLadderColumn(tierX[t], 25);
        SetLadderOneWay(tierX[t], 6, z, ONEWAY_UP);
        SetLadderOneWay(tierX[t], 25, z, ONEWAY_DOWN);
    }
    // the fire pole: one ladder through every deck, top to ground, down only
    for (int z = 1; z <= 4; z++) GRID(28, 16, z) = CELL_LADDER_BOTH;
    RecalculateLadderColumn(28, 16);
    SetLadderOneWay(28, 16, 4, ONEWAY_DOWN);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    preferDifferentZ = true;
    int n = scenarioMovers > 0 ? scenarioMovers : 24;
    for (int i = 0; i < n; i++) SpawnMoverAt(2 + (i % 6), 6 + (i / 6) * 5, 1);
    currentViewZ = 4;
    AddMessage("One-way tiers: up the north ladders, down the south ladders or the fire pole", GREEN);
}

// door-oneway: a building with a door in (south wall, passed going north) and a door out
// (north wall, also going north), 16 wandering movers: everyone goes in at the south and
// out at the north, like a shop with turnstiles. --check fails on a pass the wrong way.
static int doorPasses[2], doorWrongWay;
static int doorPrevX[MAX_MOVERS], doorPrevY[MAX_MOVERS];
static const int doorInX = 16, doorInY = 22, doorOutX = 16, doorOutY = 8;

static void SetupDoorOneWay(void) {
    for (int y = 8; y <= 22; y++)
        for (int x = 8; x <= 24; x++) {
            if (x != 8 && x != 24 && y != 8 && y != 22) continue;
            GRID(x, y, 1) = CELL_WALL;
            SetWallMaterial(x, y, 1, MAT_OAK);
        }
    GRID(doorInX, doorInY, 1) = CELL_DOOR;   SetWallMaterial(doorInX, doorInY, 1, MAT_OAK);
    GRID(doorOutX, doorOutY, 1) = CELL_DOOR; SetWallMaterial(doorOutX, doorOutY, 1, MAT_OAK);
    SetDoorOneWay(doorInX, doorInY, 1, ONEWAY_N);
    SetDoorOneWay(doorOutX, doorOutY, 1, ONEWAY_N);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    doorPasses[0] = doorPasses[1] = doorWrongWay = 0;
    for (int i = 0; i < MAX_MOVERS; i++) doorPrevX[i] = -1;
    endlessMoverMode = true;
    preferDifferentZ = false;
    int n = scenarioMovers > 0 ? scenarioMovers : 16;
    for (int i = 0; i < n; i++) SpawnMoverAt(2 + (i % 8) * 3, i < 8 ? 3 : 27, 1);
    currentViewZ = 1;
    AddMessage("One-way doors: in at the south, out at the north, 16 movers", GREEN);
}

// Every change of cell into or out of a one-way door must go its way
static void DoorTickStats(void) {
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        if (!m->active) { doorPrevX[i] = -1; continue; }
        int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE), z = (int)floorf(m->z);
        int px = doorPrevX[i], py = doorPrevY[i];
        doorPrevX[i] = x; doorPrevY[i] = y;
        if (px < 0 || (px == x && py == y) || abs(px - x) > 1 || abs(py - y) > 1) continue;
        if (z < 0 || z >= gridDepth) continue;
        if (x == doorInX && y == doorInY) doorPasses[0]++;
        if (x == doorOutX && y == doorOutY) doorPasses[1]++;
        if (!CanStepThroughDoors(px, py, x, y, z)) doorWrongWay++;
    }
}

static bool DoorCheck(char* why, int whyLen) {
    if (doorWrongWay > 0) {
        snprintf(why, (size_t)whyLen, "%d steps the wrong way through a one-way door", doorWrongWay);
        return false;
    }
    return true;
}

static void DoorPrintStats(void) {
    printf("Passes through the door in: %d, the door out: %d\n", doorPasses[0], doorPasses[1]);
    printf("Steps the wrong way through a one-way door: %d\n", doorWrongWay);
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
typedef struct { int x, y, climbs, up, down; } LadderUse;
static LadderUse ladderUses[LADDER_STAT_LADDERS];
static int ladderUseCount;
static bool moverWasClimbing[MAX_MOVERS];
static int ladderWrongWay;   // climbs against a one-way ladder

static void ResetLadderStats(void) {
    ladderUseCount = 0;
    ladderWrongWay = 0;
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
                    ladderUses[ladderUseCount++] = (LadderUse){x, y, 0, 0, 0};
                // direction: the waypoint is the cell being climbed to (z+1 going up, z going down)
                int up = -1;
                if (m->pathLength > 0 && m->pathIndex >= 0) up = moverPaths[i][m->pathIndex].z > z;
                if (k < ladderUseCount) {
                    ladderUses[k].climbs++;
                    if (up == 1) ladderUses[k].up++;
                    if (up == 0) ladderUses[k].down++;
                }
                if (up == 1 && !CanClimbUpAt(x, y, z)) ladderWrongWay++;
                if (up == 0 && !CanClimbDownAt(x, y, z + 1)) ladderWrongWay++;
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
    if (ladderWrongWay > 0) {
        snprintf(why, (size_t)whyLen, "%d climbs against a one-way ladder", ladderWrongWay);
        return false;
    }
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
        printf("Climbs on ladder at (%d,%d): %d (up %d, down %d)\n", ladderUses[k].x, ladderUses[k].y,
               ladderUses[k].climbs, ladderUses[k].up, ladderUses[k].down);
    printf("Climbs against a one-way ladder: %d\n", ladderWrongWay);
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
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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
static int esTripLeaves[MAX_MOVERS];       // ElevatorMoverLinesLeft at the trip's start
static double esTripSec[3][12];            // [0 stairs/walk only, 1 rode, 2 left a line and didn't ride]
static int esTripN[3][12];

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
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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
                int kind = esTripRode[i] ? 1 : (ElevatorMoverLinesLeft(i) > esTripLeaves[i] ? 2 : 0);
                esTripSec[kind][floors] += gameTime - esTripStart[i];
                esTripN[kind][floors]++;
            }
            esTripLeaves[i] = ElevatorMoverLinesLeft(i);
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
    printf("\n  left a line, no ride: ");
    for (int f = 0; f <= 10; f++) printf(" %4.1f", esTripN[2][f] ? esTripSec[2][f] / esTripN[2][f] : 0.0);
    printf("\n  ... counted:          ");
    for (int f = 0; f <= 10; f++) printf(" %4d", esTripN[2][f]);
    printf("\n");
    // What leaving costs: those trips vs a stairs/walk trip over as many floors
    double extra = 0.0, total = 0.0; int nLeft = 0;
    for (int f = 0; f <= 10; f++) {
        for (int k = 0; k < 3; k++) total += esTripSec[k][f];
        if (!esTripN[2][f] || !esTripN[0][f]) continue;
        extra += esTripSec[2][f] - esTripN[2][f] * (esTripSec[0][f] / esTripN[0][f]);
        nLeft += esTripN[2][f];
    }
    printf("Leaving a line: %d trips, %.1f s more each than by stairs (%.0f s in all = %.1f%% of all trip time)\n",
           nLeft, nLeft ? extra / nLeft : 0.0, extra, total > 0.0 ? 100.0 * extra / total : 0.0);
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
    supportEnabled = false;   // a pathing rig: its decks stand on nothing (ladders, shafts)
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

// ---- drift: logs and sticks dropped in the island's rivers go with the current (ItemsDriftTick) ----
#define DRIFT_SPOTS 60
static int driftItem[DRIFT_SPOTS * 2], driftCount = 0;
static ItemType driftType[DRIFT_SPOTS * 2];
static float driftStartX[DRIFT_SPOTS * 2], driftStartY[DRIFT_SPOTS * 2];

static void SetupDrift(void) {
    driftCount = 0;
    itemsDrift = true;
    // River cells: water at least 2 deep, above the sea, not a spring. Every k-th one, so the spots
    // spread over all the rivers (scan order: bottom to top, row by row; deterministic). A log and
    // a bundle of sticks on each spot: wood needs water 2 deep, sticks float in any
    int candidates = 0;
    for (int z = waterEdgeSeaZ + 1; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                const WaterCell* w = &WATER(x, y, z);
                if (w->level >= 2 && !w->isSource) candidates++;
            }
    int stride = candidates / DRIFT_SPOTS;
    if (stride < 1) stride = 1;
    int k = 0, spots = 0;
    for (int z = waterEdgeSeaZ + 1; z < gridDepth && spots < DRIFT_SPOTS; z++)
        for (int y = 0; y < gridHeight && spots < DRIFT_SPOTS; y++)
            for (int x = 0; x < gridWidth && spots < DRIFT_SPOTS; x++) {
                const WaterCell* w = &WATER(x, y, z);
                if (w->level < 2 || w->isSource) continue;
                if (k++ % stride != 0) continue;
                spots++;
                float px = (x + 0.5f) * CELL_SIZE, py = (y + 0.5f) * CELL_SIZE;
                static const ItemType kinds[2] = {ITEM_LOG, ITEM_STICKS};
                for (int t = 0; t < 2; t++) {
                    int idx = SpawnItemWithMaterial(px, py, (float)z, kinds[t], MAT_OAK);
                    if (idx < 0) continue;
                    driftItem[driftCount] = idx;
                    driftType[driftCount] = kinds[t];
                    driftStartX[driftCount] = px;
                    driftStartY[driftCount] = py;
                    driftCount++;
                }
            }
}

// Did tracked item i drift a cell or more (or off the map: gone, or its slot reused)?
static bool DriftMoved(int i, float* dist) {
    const Item* it = &items[driftItem[i]];
    if (!it->active || it->type != driftType[i]) { *dist = 0.0f; return true; }
    float dx = it->x - driftStartX[i], dy = it->y - driftStartY[i];
    *dist = sqrtf(dx * dx + dy * dy) / CELL_SIZE;
    return *dist >= 1.0f;
}

static void DriftPrintStats(void) {
    static const ItemType kinds[2] = {ITEM_LOG, ITEM_STICKS};
    for (int t = 0; t < 2; t++) {
        int n = 0, moved = 0, drifting = 0, resting = 0, atSea = 0, lost = 0;
        float sum = 0.0f, best = 0.0f;
        for (int i = 0; i < driftCount; i++) {
            if (driftType[i] != kinds[t]) continue;
            n++;
            float d;
            if (DriftMoved(i, &d)) moved++;
            const Item* it = &items[driftItem[i]];
            if (!it->active || it->type != driftType[i]) { lost++; continue; }
            sum += d;
            if (d > best) best = d;
            int cx = (int)(it->x / CELL_SIZE), cy = (int)(it->y / CELL_SIZE), cz = (int)it->z;
            float fx, fy, gx = 0.0f, gy = 0.0f;
            if (GetWaterFlow(cx, cy, cz, &fx, &fy)) { gx += fx; gy += fy; }
            if (cz > 0 && GetWaterFlow(cx, cy, cz - 1, &fx, &fy)) { gx += fx; gy += fy; }
            int depth = WATER(cx, cy, cz).level + (cz > 0 && WATER(cx, cy, cz - 1).level == WATER_MAX_LEVEL ? WATER_MAX_LEVEL : 0);
            int need = kinds[t] == ITEM_STICKS ? 1 : 2;
            if (cz <= waterEdgeSeaZ && WATER(cx, cy, cz).level > 0) atSea++;
            else if (depth >= need && gx * gx + gy * gy >= 0.25f) drifting++;   // as ItemsDriftTick
            else resting++;   // washed up, or afloat in still water
        }
        printf("Drift %-6s: %d; moved >= 1 cell %d (avg %.1f, max %.1f cells); now %d drifting, %d resting, %d on the sea, %d lost off the map\n",
               ItemName(kinds[t]), n, moved, n - lost ? sum / (n - lost) : 0.0f, best, drifting, resting, atSea, lost);
    }
}

static bool DriftCheck(char* why, int whyLen) {
    // Most logs land in still water (lakes, pools at river heads) and rightly stay; ~10 drift in
    // the first 3000 ticks on seed 1. Broken drift gives 0
    int moved = 0;
    float d;
    for (int i = 0; i < driftCount; i++) if (driftType[i] == ITEM_LOG && DriftMoved(i, &d)) moved++;
    if (moved < 6) {
        snprintf(why, (size_t)whyLen, "only %d logs drifted a cell (want >= 6)", moved);
        return false;
    }
    return true;
}

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
    printf("Surf: %d flats cells, %d beach cells, %d changed last tick (tideSurf %d)\n", tideSurfCells, tideBeachCells, tideSurfChanges, tideSurf);
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

// trash: the city's trash loop (docs/vision/city-1999.md "Trash, bins and deposit bottles").
// A snack stand (stockpile of snacks and sodas, topped up like a delivery) and three trash
// bins: A by the stand, B 12 cells off, C at the far end (out of reach for most). 10 movers
// wander about (foot traffic), start hungry and thirsty, eat and drink at the stand and take
// the can or wrapper to the nearest bin with room within trashBinWalk cells; nothing in
// reach: litter. Mover 0 is a Slob (drops it on the spot), mover 1 Tidy (walks twice as far).
// Expected: A fills first, then B; litter once both are full (and the slob's at once).
#define TRASH_STAND_X 4
#define TRASH_STAND_Y 10
static int trashBins[3];
static const int trashBinX[3] = { 8, 18, 36 };

// The stand: a 3x2 stockpile, top row `top`, bottom row `bottom`; an empty slot gets a full
// stack (a delivery). Items go into the stockpile slots (in the shop), not on the ground.
static int trashStand = -1;
static void StandRestock(ItemType top, ItemType bottom) {
    if (trashStand < 0 || !stockpiles[trashStand].active) return;
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 3; dx++) {
            int x = TRASH_STAND_X + dx, y = TRASH_STAND_Y + dy;
            const Stockpile* sp = &stockpiles[trashStand];
            int slot = (y - sp->y) * sp->width + (x - sp->x);
            if (sp->slots[slot] >= 0 && sp->slotCounts[slot] > 0) continue;
            ItemType t = dy == 0 ? top : bottom;
            int idx = SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, 1, t);
            if (idx < 0) continue;
            items[idx].stackCount = ItemMaxStack(t);
            PlaceItemInStockpile(trashStand, x, y, idx);
        }
}

static void TrashRestock(void) { StandRestock(ITEM_SNACK, ITEM_SODA); }

static void SetupTrash(void) {
    hungerEnabled = true; thirstEnabled = true;
    energyEnabled = false; bladderEnabled = false; bodyTempEnabled = false;
    dayLength = 360.0f;                 // the games' pace: 6-minute day, walking 3x
    balance.baseMoverSpeed = 600.0f;    // (before the movers spawn: they take it from here)
    endlessMoverMode = true;            // wander between meals: foot traffic
    trashStand = CreateStockpile(TRASH_STAND_X, TRASH_STAND_Y, 1, 3, 2);
    TrashRestock();
    for (int b = 0; b < 3; b++)
        trashBins[b] = SpawnItem(trashBinX[b] * CELL_SIZE + CELL_SIZE * 0.5f, 11 * CELL_SIZE + CELL_SIZE * 0.5f, 1, ITEM_TRASH_BIN);
    BuildEntrances(); BuildGraph();
    for (int i = 0; i < 10; i++) {
        SpawnMoverAt(2 + i * 3, 4 + (i % 3) * 7, 1);
        Mover* m = &movers[moverCount - 1];
        m->hunger = 0.15f + 0.02f * i;   // all below the seek thresholds, a little apart
        m->thirst = 0.20f + 0.02f * i;
        if (i == 0) { m->traits[0] = TRAIT_SLOB; m->traits[1] = TRAIT_NONE; }
        if (i == 1) { m->traits[0] = TRAIT_TIDY; m->traits[1] = TRAIT_NONE; }
    }
    currentViewZ = 1;
}

static void TrashTickStats(void) {
    if (currentTick % 600 == 0) TrashRestock();
}

static int TrashInBin(int bin) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].containedIn == bin) n += items[i].stackCount;
    return n;
}

static int TrashLitter(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].state == ITEM_ON_GROUND &&
            (items[i].type == ITEM_EMPTY_CAN || items[i].type == ITEM_WRAPPER)) n += items[i].stackCount;
    return n;
}

static void TrashPrintStats(void) {
    int held = 0;
    for (int i = 0; i < moverCount; i++) if (movers[i].active && movers[i].heldTrash != ITEM_NONE) held++;
    printf("Trash: bin A (by the stand) %d, bin B (12 cells) %d, bin C (far end) %d, litter %d, in hand %d\n",
           TrashInBin(trashBins[0]), TrashInBin(trashBins[1]), TrashInBin(trashBins[2]), TrashLitter(), held);
}

static bool TrashCheck(char* why, int whyLen) {
    int a = TrashInBin(trashBins[0]), total = a + TrashInBin(trashBins[1]) + TrashInBin(trashBins[2]) + TrashLitter();
    if (total < 10 || a < 5) {
        snprintf(why, (size_t)whyLen, "trash: %d pieces, %d in bin A (want >= 10 and >= 5)", total, a);
        return false;
    }
    return true;
}

// heat: the stations "smallest proof" (docs/todo/architecture/capability-based-tasks.md):
// heat as a capability with levels, from providers that aren't workshops. A winter street
// (snowing) with a heating grate (walk on it) at the west end and a burning barrel (stand
// next to it) at the east end, both heat 1. 8 movers wander, start chilled; a cold mover
// walks to the nearest warm spot (IsWarmSpot: by walking, any provider), warms up, wanders on.
// Stats: warm-ups at each, coldest body temperature, how many are cold at the end.
static Point heatGrate, heatBarrel;
static int heatWarmUps[2], heatWarmOther;
static int heatPrevState[MAX_MOVERS];
static float heatColdest;

static void SetupHeat(void) {
    hungerEnabled = false; thirstEnabled = false; energyEnabled = false; bladderEnabled = false;
    bodyTempEnabled = true; temperatureEnabled = true;
    dayLength = 360.0f;
    balance.baseMoverSpeed = 600.0f;
    endlessMoverMode = true;
    InitWeather();
    dayNumber = 3 * daysPerSeason + 1;   // winter
    weatherState.current = weatherState.previous = WEATHER_SNOW;
    weatherState.transitionTimer = weatherState.transitionDuration = GameHoursToGameSeconds(48.0f);
    weatherState.intensity = 1.0f;
    heatGrate = (Point){6, 12, 1};
    heatBarrel = (Point){33, 12, 1};
    SpawnFurniture(heatGrate.x, heatGrate.y, 1, FURNITURE_HEAT_GRATE, MAT_IRON);
    SpawnFurniture(heatBarrel.x, heatBarrel.y, 1, FURNITURE_FIRE_BARREL, MAT_IRON);
    for (int k = 0; k < 2; k++) {   // sticks by the barrel: it burns fuel (2 h a bundle), the movers top it up
        int sticks = SpawnItem((heatBarrel.x + 2.5f) * CELL_SIZE, (heatBarrel.y + 0.5f + k) * CELL_SIZE, 1.0f, ITEM_STICKS);
        if (sticks >= 0) items[sticks].stackCount = 20;
    }
    BuildEntrances(); BuildGraph();
    for (int i = 0; i < 8; i++) {
        SpawnMoverAt(4 + i * 4, 4 + (i % 2) * 14, 1);
        movers[moverCount - 1].bodyTemp = 34.0f + 0.2f * i;   // chilled, a little apart
        heatPrevState[moverCount - 1] = FREETIME_NONE;
    }
    heatWarmUps[0] = heatWarmUps[1] = heatWarmOther = 0;
    heatColdest = 99.0f;
    InvalidateLighting();
    currentViewZ = 1;
}

static void HeatTickStats(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if (m->bodyTemp < heatColdest) heatColdest = m->bodyTemp;
        if (m->freetimeState == FREETIME_WARMING && heatPrevState[i] != FREETIME_WARMING) {
            int x = (int)(m->x / CELL_SIZE), y = (int)(m->y / CELL_SIZE);
            if (abs(x - heatGrate.x) <= 1 && abs(y - heatGrate.y) <= 1) heatWarmUps[0]++;
            else if (abs(x - heatBarrel.x) <= 1 && abs(y - heatBarrel.y) <= 1) heatWarmUps[1]++;
            else heatWarmOther++;
        }
        heatPrevState[i] = m->freetimeState;
    }
}

static int HeatColdNow(void) {
    int n = 0;
    for (int i = 0; i < moverCount; i++)
        if (movers[i].active && movers[i].bodyTemp < balance.moderateColdThreshold) n++;
    return n;
}

static void HeatPrintStats(void) {
    printf("Heat: %s, air %dC; warm-ups at the grate %d, at the barrel %d, elsewhere %d; coldest body %.1fC, %d of %d cold now\n",
           GetSeasonName(GetCurrentSeason()), GetAmbientTemperature(1), heatWarmUps[0], heatWarmUps[1],
           heatWarmOther, heatColdest, HeatColdNow(), moverCount);
    printf("  grate cell %dC (heat %d), barrel neighbour %dC (heat %d)\n",
           GetTemperature(heatGrate.x, heatGrate.y, 1), HeatLevelAt(heatGrate.x, heatGrate.y, 1),
           GetTemperature(heatBarrel.x - 1, heatBarrel.y, 1), HeatLevelAt(heatBarrel.x, heatBarrel.y, 1));
}

static bool HeatCheck(char* why, int whyLen) {
    if (heatWarmUps[0] < 1 || heatWarmUps[1] < 1) {
        snprintf(why, (size_t)whyLen, "warm-ups: grate %d, barrel %d (want >= 1 each)", heatWarmUps[0], heatWarmUps[1]);
        return false;
    }
    return true;
}

// inventory: what movers carry (inventory.c): two hands (a log takes both), a back slot,
// pockets on a pocket jacket. Six wandering movers, each kitted out differently; click one
// to see "Hands / Back / Clothing" in its description. Held and worn things walk along.
// Check: every held/worn item is still on its mover (position) and the state audit is clean.
static void SetupInventory(void) {
    hungerEnabled = false; thirstEnabled = false; energyEnabled = false; bladderEnabled = false;
    bodyTempEnabled = false;
    dayLength = 360.0f;
    balance.baseMoverSpeed = 600.0f;
    endlessMoverMode = true;
    BuildEntrances(); BuildGraph();
    for (int i = 0; i < 6; i++) {
        SpawnMoverAt(4 + i * 5, 10, 1);
        int mi = moverCount - 1;
        float x = movers[mi].x, y = movers[mi].y;
        #define INV_SPAWN(t) SpawnItem(x, y, 1, (t))
        switch (i) {
            case 0: MoverWear(mi, INV_SPAWN(ITEM_POCKET_JACKET)); MoverStow(mi, INV_SPAWN(ITEM_SODA)); break;
            case 1: MoverWear(mi, INV_SPAWN(ITEM_BACKPACK)); MoverStow(mi, INV_SPAWN(ITEM_SNACK)); MoverStow(mi, INV_SPAWN(ITEM_SODA)); break;
            case 2: MoverHold(mi, INV_SPAWN(ITEM_PLASTIC_BAG)); MoverStow(mi, INV_SPAWN(ITEM_SNACK)); MoverHold(mi, INV_SPAWN(ITEM_SODA)); break;
            case 3: MoverHold(mi, INV_SPAWN(ITEM_LOG)); break;   // both hands
            case 4: MoverWear(mi, INV_SPAWN(ITEM_POCKET_JACKET)); MoverWear(mi, INV_SPAWN(ITEM_BACKPACK));
                    MoverHold(mi, INV_SPAWN(ITEM_PLASTIC_BAG));
                    for (int k = 0; k < 6; k++) MoverStow(mi, INV_SPAWN(k % 2 ? ITEM_SODA : ITEM_SNACK));
                    break;
            default: break;   // empty-handed
        }
        #undef INV_SPAWN
    }
    currentViewZ = 1;
}

static void InventoryPrintStats(void) {
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        printf("Inventory: mover %d hands %d/%d back %d clothing %d (free hands %d)\n",
               i, m->hands[0], m->hands[1], m->wornBack, m->equippedClothing, MoverFreeHands(i));
    }
}

static bool InventoryCheck(char* why, int whyLen) {
    for (int i = 0; i < moverCount; i++) {
        const Mover* m = &movers[i];
        int slots[3] = { m->hands[0], m->hands[1], m->wornBack };
        for (int k = 0; k < 3; k++) {
            int it = slots[k];
            if (it < 0) continue;
            if ((int)(items[it].x / CELL_SIZE) != (int)(m->x / CELL_SIZE) || (int)(items[it].y / CELL_SIZE) != (int)(m->y / CELL_SIZE)) {
                snprintf(why, (size_t)whyLen, "mover %d: item %d is not where the mover is", i, it);
                return false;
            }
        }
    }
    if (movers[3].hands[0] < 0 || movers[3].hands[0] != movers[3].hands[1]) {
        snprintf(why, (size_t)whyLen, "mover 3 should hold the log in both hands");
        return false;
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// scrounge: hunger-driven bin searching (docs/vision/city-1999.md "Scrounging"). The trash
// street, but the stand sells burgers (a big portion) and sodas. 10 NPCs eat when hungry;
// whoever isn't starving can't finish a burger and throws the leftovers in a bin with the
// wrapper. Two Scavengers (no money: they take nothing from the stand) live off the bins:
// search, eat the best, keep the rest in a pocket jacket (scavenger 0) or a hand (1).
// Stats: bin searches and meals per scavenger, their lowest hunger, leftovers in bins.
static int scrScav[2], scrSearches[2], scrMeals[2], scrPrevState[2];
static float scrMinHunger[2];

static void ScroungeRestock(void) { StandRestock(ITEM_BURGER, ITEM_SODA); }

static void SetupScrounge(void) {
    SetupTrash();
    // The stand sells burgers instead of snacks: empty the top row, restock
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].type == ITEM_SNACK && items[i].state == ITEM_IN_STOCKPILE) {
            RemoveItemFromStockpileSlot(items[i].x, items[i].y, (int)items[i].z);
            DeleteItem(i);
        }
    ScroungeRestock();
    for (int k = 0; k < 2; k++) {
        SpawnMoverAt(30 + k * 4, 20, 1);
        int mi = moverCount - 1;
        Mover* m = &movers[mi];
        m->traits[0] = TRAIT_SCAVENGER; m->traits[1] = TRAIT_NONE;
        m->hunger = 0.25f; m->thirst = 1.0f;
        if (k == 0) MoverWear(mi, SpawnItem(m->x, m->y, 1, ITEM_POCKET_JACKET));
        scrScav[k] = mi; scrSearches[k] = scrMeals[k] = 0; scrPrevState[k] = FREETIME_NONE;
        scrMinHunger[k] = 1.0f;
    }
}

static void ScroungeTickStats(void) {
    if (currentTick % 600 == 0) ScroungeRestock();
    for (int k = 0; k < 2; k++) {
        const Mover* m = &movers[scrScav[k]];
        if (m->freetimeState == FREETIME_SCROUNGING && scrPrevState[k] != FREETIME_SCROUNGING) scrSearches[k]++;
        if (m->freetimeState == FREETIME_EATING && scrPrevState[k] != FREETIME_EATING) scrMeals[k]++;
        if (m->hunger < scrMinHunger[k]) scrMinHunger[k] = m->hunger;
        scrPrevState[k] = m->freetimeState;
    }
}

static int ScroungeLeftoversIn(bool bins) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active || items[i].type != ITEM_LEFTOVERS) continue;
        int top = GetOutermostContainer(i);
        bool inBin = items[i].containedIn >= 0 && items[top].type == ITEM_TRASH_BIN;
        if (inBin == bins) n += items[i].stackCount;
    }
    return n;
}

static void ScroungePrintStats(void) {
    for (int k = 0; k < 2; k++) {
        const Mover* m = &movers[scrScav[k]];
        printf("Scavenger %d: %d bin searches, %d meals, hunger now %.0f%% (lowest %.0f%%), hands %d/%d\n",
               k, scrSearches[k], scrMeals[k], m->hunger * 100.0f, scrMinHunger[k] * 100.0f, m->hands[0], m->hands[1]);
    }
    printf("Leftovers: %d units in bins, %d elsewhere (pockets, hands, ground)\n",
           ScroungeLeftoversIn(true), ScroungeLeftoversIn(false));
}

// ---- bottlerun: City 1999's bottle run on orders (the action queue, player commands) ----
// A street with five bins holding empty cans (and a wrapper each, not worth taking); one person
// with a backpack. The setup queues a run past the first four bins as the player would with
// E / Shift+E (src/entities/action_queue.c); the fifth bin is left for you.
#define BOTTLE_BINS 5
static int bottleBins[BOTTLE_BINS];
static const int bottleBinX[BOTTLE_BINS] = { 7, 13, 20, 27, 35 };
static const int bottleBinCans[BOTTLE_BINS] = { 2, 0, 3, 1, 2 };

static int BottleCansCarried(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].type == ITEM_EMPTY_CAN && ItemIsInMoverInventory(0, i)) n += items[i].stackCount;
    return n;
}

static void SetupBottleRun(void) {
    hungerEnabled = false; thirstEnabled = false; energyEnabled = false; bladderEnabled = false;
    bodyTempEnabled = false;
    dayLength = 360.0f;
    balance.baseMoverSpeed = 600.0f;
    endlessMoverMode = true;   // off, a mover with nothing left to do at its goal is removed (pathing demos)
    for (int b = 0; b < BOTTLE_BINS; b++) {
        float bx = bottleBinX[b] * CELL_SIZE + CELL_SIZE * 0.5f, by = 13 * CELL_SIZE + CELL_SIZE * 0.5f;
        bottleBins[b] = SpawnItem(bx, by, 1, ITEM_TRASH_BIN);
        for (int k = 0; k < bottleBinCans[b]; k++) PutItemInContainer(SpawnItem(bx, by, 1, ITEM_EMPTY_CAN), bottleBins[b]);
        PutItemInContainer(SpawnItem(bx, by, 1, ITEM_WRAPPER), bottleBins[b]);
    }
    BuildEntrances(); BuildGraph();
    SpawnMoverAt(3, 11, 1);
    int mi = moverCount - 1;
    MoverWear(mi, SpawnItem(movers[mi].x, movers[mi].y, 1, ITEM_BACKPACK));
    char said[128];
    for (int b = 0; b < 4; b++)   // as E on the first bin, then Shift+E on the next three
        QueueCommandAt(mi, bottleBinX[b], 13, 1, b > 0, said, sizeof(said));
    currentViewZ = 1;
}

static void BottleRunPrintStats(void) {
    printf("Bottle run: %d cans carried, queue %s; cans left per bin:", BottleCansCarried(), QueueBusy(0) ? "still running" : "done");
    for (int b = 0; b < BOTTLE_BINS; b++) {
        int n = 0;
        for (int c = FirstInContainer(bottleBins[b]); c >= 0; c = NextInContainer(c)) if (items[c].type == ITEM_EMPTY_CAN) n += items[c].stackCount;
        printf(" %d", n);
    }
    printf("\n");
}

static bool BottleRunCheck(char* why, int whyLen) {
    int want = bottleBinCans[0] + bottleBinCans[1] + bottleBinCans[2] + bottleBinCans[3];
    if (!movers[0].active) { snprintf(why, (size_t)whyLen, "the collector is gone"); return false; }
    if (QueueBusy(0)) { snprintf(why, (size_t)whyLen, "the bottle run didn't finish"); return false; }
    if (BottleCansCarried() != want) { snprintf(why, (size_t)whyLen, "%d cans carried, expected %d", BottleCansCarried(), want); return false; }
    return true;
}

static bool ScroungeCheck(char* why, int whyLen) {
    for (int k = 0; k < 2; k++) {
        if (scrSearches[k] < 1 || scrMeals[k] < 1) {
            snprintf(why, (size_t)whyLen, "scavenger %d: %d searches, %d meals (want >= 1 each)", k, scrSearches[k], scrMeals[k]);
            return false;
        }
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// hands-jobs: jobs carry in the hands (inventory.c MoverMakeHandsFor). Two haulers hold a
// plastic bag with a can in it; there are logs (two-handed) and berries (one hand) to haul
// to a stockpile. For a log the bag goes down at the pickup spot (reserved: nobody takes
// it); berries fit in the free hand. With no work left, they go back for their bags.
// Check: everything hauled, both bags held again (or still its own, being fetched), audit clean.
static int hjBag[2], hjSetDowns, hjPrev[2];

static void SetupHandsJobs(void) {
    hungerEnabled = false; thirstEnabled = false; energyEnabled = false; bladderEnabled = false;
    bodyTempEnabled = false;
    dayLength = 360.0f;
    balance.baseMoverSpeed = 600.0f;
    CreateStockpile(30, 10, 1, 4, 4);
    for (int k = 0; k < 3; k++) SpawnItem((6 + k * 3) * CELL_SIZE + 4, 6 * CELL_SIZE + 4, 1, ITEM_LOG);
    for (int k = 0; k < 3; k++) {
        int b = SpawnItem((6 + k * 3) * CELL_SIZE + 4, 16 * CELL_SIZE + 4, 1, ITEM_BERRIES);
        if (b >= 0) items[b].stackCount = 5;
    }
    BuildEntrances(); BuildGraph();
    for (int i = 0; i < 2; i++) {
        SpawnMoverAt(4, 10 + i * 2, 1);
        int mi = moverCount - 1;
        movers[mi].traits[0] = movers[mi].traits[1] = TRAIT_NONE;
        hjBag[i] = SpawnItem(movers[mi].x, movers[mi].y, 1, ITEM_PLASTIC_BAG);
        MoverHold(mi, hjBag[i]);
        MoverStow(mi, SpawnItem(movers[mi].x, movers[mi].y, 1, ITEM_EMPTY_CAN));
        hjPrev[i] = -1;
    }
    hjSetDowns = 0;
    currentViewZ = 1;
}

static void HandsJobsTick(void) {
    for (int i = 0; i < 2; i++) {
        int now = MoverSetAsideItem(i);
        if (now >= 0 && hjPrev[i] < 0) hjSetDowns++;
        hjPrev[i] = now;
    }
}

static int HandsJobsLeft(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].state != ITEM_IN_STOCKPILE && (items[i].type == ITEM_LOG || items[i].type == ITEM_BERRIES)) n++;
    return n;
}

static void HandsJobsPrintStats(void) {
    printf("Hands/jobs: %d bag set-downs, %d loads not in the stockpile; bags held: %s, %s\n", hjSetDowns, HandsJobsLeft(),
           ItemInMoverHands(0, hjBag[0]) ? "yes" : "no", ItemInMoverHands(1, hjBag[1]) ? "yes" : "no");
}

static bool HandsJobsCheck(char* why, int whyLen) {
    if (HandsJobsLeft() > 0) { snprintf(why, (size_t)whyLen, "%d loads not hauled", HandsJobsLeft()); return false; }
    if (hjSetDowns < 1) { snprintf(why, (size_t)whyLen, "no bag was put down for a log"); return false; }
    for (int i = 0; i < 2; i++) {
        // held again, or still its own (set aside, reserved) and on the way back for it
        bool held = ItemInMoverHands(i, hjBag[i]);
        bool own = MoverSetAsideItem(i) == hjBag[i];
        if ((!held && !own) || items[hjBag[i]].contentCount != 1) {
            snprintf(why, (size_t)whyLen, "mover %d lost its bag (or the can in it)", i);
            return false;
        }
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// railings: every rail-* tile on show (RailingSpriteAt picks one per N/E/S/W mask),
// with 6 movers wandering round them. West to east: a pen with a gate gap (corners,
// dead ends), a 3x3 window frame (T-junctions, the cross), a lone post and a stub, a
// railing butting a wall, and a zigzag along the bottom (corners both ways).
static void Rail(int x, int y, MaterialType mat) {
    GRID(x, y, 1) = CELL_RAILING;
    SetWallMaterial(x, y, 1, mat);
    ClearWallNatural(x, y, 1);
}

static void SetupRailings(void) {
    hungerEnabled = thirstEnabled = energyEnabled = bladderEnabled = false;
    for (int x = 2; x <= 10; x++) { Rail(x, 2, MAT_IRON); if (x != 6) Rail(x, 8, MAT_IRON); }   // the pen
    for (int y = 3; y <= 7; y++) { Rail(2, y, MAT_IRON); Rail(10, y, MAT_IRON); }
    for (int y = 2; y <= 8; y++)                                                         // window frame
        for (int x = 13; x <= 21; x++)
            if (x == 13 || x == 17 || x == 21 || y == 2 || y == 5 || y == 8) Rail(x, y, MAT_OAK);
    Rail(25, 3, MAT_GRANITE);                                                            // lone post
    for (int x = 24; x <= 27; x++) Rail(x, 7, MAT_GRANITE);                              // a stub
    for (int x = 30; x <= 36; x++) { GRID(x, 2, 1) = CELL_WALL; SetWallMaterial(x, 2, 1, MAT_BRICK); ClearWallNatural(x, 2, 1); }
    for (int y = 3; y <= 6; y++) Rail(33, y, MAT_IRON);                                  // off the wall
    Rail(30, 3, MAT_IRON);                                                               // alone, under the wall
    for (int k = 0; k < 6; k++) {                                                        // zigzag
        int x0 = 3 + k * 6, y0 = (k % 2) ? 12 : 18;
        for (int i = 0; i <= 6; i++) Rail(x0, y0 + ((k % 2) ? i : -i), MAT_OAK);
        for (int i = 1; i <= 5; i++) Rail(x0 + i, (k % 2) ? 18 : 12, MAT_OAK);
    }
    BuildEntrances(); BuildGraph();
    InvalidateLighting();
    endlessMoverMode = true;
    for (int i = 0; i < 6; i++) SpawnMoverAt(4 + i * 6, 10, 1);
    currentViewZ = 1;
    AddMessage("Railings: every rail tile (corners, T-junctions, cross, dead ends, a lone post, one against a wall)", GREEN);
}

// Movers walk round railings, never through them
static bool RailingsCheck(char* why, int whyLen) {
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        int x = (int)(movers[i].x / CELL_SIZE), y = (int)(movers[i].y / CELL_SIZE), z = (int)movers[i].z;
        if (GRID(x, y, z) == CELL_RAILING) { snprintf(why, (size_t)whyLen, "mover %d stands in a railing at %d,%d", i, x, y); return false; }
    }
    return true;
}

// explosions: a flat field with two lit charges. Left: a charge on open ground (power 6, 2 s)
// with loose things round it at 3 cells (leaves, sticks, planks, a rock, a log), a mover in the
// open, a mover behind a granite wall (sheltered), and a wooden fence 2 cells away (breaks).
// Right: a granite hill (z1-3) with a charge drilled into its face (power 8, 4 s): blast mining.
// Check: light things fly further than heavy ones, the open mover is knocked back, the sheltered
// one isn't, the fence and some granite broke, the state audit is clean.
#define BOOM_ITEMS 5
static int boomItem[BOOM_ITEMS];
static float boomItemX[BOOM_ITEMS], boomItemY[BOOM_ITEMS];
static int boomOpenMover, boomShelteredMover;
static bool boomKnocked[2];
static float boomCalmX[2], boomCalmY[2], boomStartX[2], boomStartY[2], boomThrown[2];   // thrown: cells, when the knock ended
static int boomGraniteBefore;

static int BoomCountGranite(void) {
    int n = 0;
    for (int z = 1; z <= 3; z++)
        for (int y = 8; y <= 24; y++)
            for (int x = 30; x <= 44; x++)
                if (GRID(x, y, z) == CELL_WALL && GetWallMaterial(x, y, z) == MAT_GRANITE) n++;
    return n;
}

static void SetupExplosions(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;   // they stand where they are put
    // Granite wall shielding the mover north of the field charge (2 high)
    for (int x = 8; x <= 16; x++)
        for (int z = 1; z <= 2; z++) { GRID(x, 12, z) = CELL_WALL; SetWallMaterial(x, 12, z, MAT_GRANITE); }
    // Wooden fence 2 cells south of the charge
    for (int x = 11; x <= 13; x++) { GRID(x, 18, 1) = CELL_WALL; SetWallMaterial(x, 18, 1, MAT_OAK); }
    // Granite hill
    for (int z = 1; z <= 3; z++)
        for (int y = 8; y <= 24; y++)
            for (int x = 30; x <= 44; x++) {
                GRID(x, y, z) = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_GRANITE);
                SetWallNatural(x, y, z);
            }
    boomGraniteBefore = BoomCountGranite();
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();

    // Loose things 3 cells from the charge at (12,16,1), each in its own direction
    static const ItemType kinds[BOOM_ITEMS] = { ITEM_LEAVES, ITEM_STICKS, ITEM_PLANKS, ITEM_ROCK, ITEM_LOG };
    static const int ix[BOOM_ITEMS] = { 9, 15, 9, 15, 12 }, iy[BOOM_ITEMS] = { 16, 16, 14, 14, 13 };
    for (int i = 0; i < BOOM_ITEMS; i++) {
        boomItemX[i] = (ix[i] + 0.5f) * CELL_SIZE;
        boomItemY[i] = (iy[i] + 0.5f) * CELL_SIZE;
        boomItem[i] = SpawnItemWithMaterial(boomItemX[i], boomItemY[i], 1.0f, kinds[i], DefaultMaterialForItemType(kinds[i]));
    }
    // The log at (12,13) lies against the shelter wall: the blast pushes it into the wall, it stays
    // Movers: one in the open (3 cells east, 2 south), one behind the granite wall
    boomOpenMover = moverCount;
    SpawnMoverAt(14, 18, 1);
    boomShelteredMover = moverCount;
    SpawnMoverAt(12, 10, 1);
    for (int k = 0; k < 2; k++) {
        int m = k == 0 ? boomOpenMover : boomShelteredMover;
        boomKnocked[k] = false;
        boomThrown[k] = 0.0f;
        boomCalmX[k] = boomStartX[k] = movers[m].x;
        boomCalmY[k] = boomStartY[k] = movers[m].y;
    }
    ExplosionLightFuse(12, 16, 1, 6.0f, 2.0f);
    ExplosionLightFuse(30, 16, 2, 8.0f, 4.0f);   // drilled into the hill's face
    currentViewZ = 1;
    AddMessage("Explosions: a charge in the field (2 s) and one drilled into the granite hill (4 s)", ORANGE);
}

static void ExplosionsTickStats(void) {
    for (int k = 0; k < 2; k++) {
        int m = k == 0 ? boomOpenMover : boomShelteredMover;
        if (m >= moverCount || !movers[m].active) continue;
        if (MoverKnocked(m)) {
            if (!boomKnocked[k]) { boomStartX[k] = boomCalmX[k]; boomStartY[k] = boomCalmY[k]; }
            boomKnocked[k] = true;
            float dx = movers[m].x - boomStartX[k], dy = movers[m].y - boomStartY[k];
            boomThrown[k] = sqrtf(dx * dx + dy * dy) / CELL_SIZE;
        } else if (!boomKnocked[k]) { boomCalmX[k] = movers[m].x; boomCalmY[k] = movers[m].y; }
    }
}

static float BoomItemMoved(int i) {
    const Item* it = &items[boomItem[i]];
    if (boomItem[i] < 0 || !it->active) return -1.0f;
    float dx = it->x - boomItemX[i], dy = it->y - boomItemY[i];
    return sqrtf(dx * dx + dy * dy) / CELL_SIZE;
}

static float BoomMoverMoved(int k) { return boomThrown[k]; }

static void ExplosionsPrintStats(void) {
    static const char* names[BOOM_ITEMS] = { "leaves", "sticks", "planks", "rock", "log" };
    printf("Explosions: items moved (cells):");
    for (int i = 0; i < BOOM_ITEMS; i++) printf(" %s %.1f", names[i], BoomItemMoved(i));
    printf("\n  open mover knocked %s (%.1f cells), sheltered mover knocked %s\n",
           boomKnocked[0] ? "yes" : "no", BoomMoverMoved(0), boomKnocked[1] ? "yes" : "no");
    int fence = 0;
    for (int x = 11; x <= 13; x++) if (GRID(x, 18, 1) != CELL_WALL) fence++;
    printf("  fence cells broken %d/3, granite cells blasted %d, fuses left %d\n",
           fence, boomGraniteBefore - BoomCountGranite(), ExplosionFuseCount());
}

static bool ExplosionsCheck(char* why, int whyLen) {
    if (ExplosionFuseCount() != 0) { snprintf(why, (size_t)whyLen, "a fuse never went off"); return false; }
    float leaves = BoomItemMoved(0), rock = BoomItemMoved(3);
    if (leaves < 0.0f || rock < 0.0f || leaves <= rock + 1.0f) {
        snprintf(why, (size_t)whyLen, "leaves moved %.1f cells, rock %.1f (want leaves > rock + 1)", leaves, rock);
        return false;
    }
    if (!boomKnocked[0] || BoomMoverMoved(0) < 0.5f) {
        snprintf(why, (size_t)whyLen, "the open mover wasn't knocked back (%.1f cells)", BoomMoverMoved(0));
        return false;
    }
    if (boomKnocked[1]) { snprintf(why, (size_t)whyLen, "the mover behind the granite wall was knocked"); return false; }
    int fence = 0;
    for (int x = 11; x <= 13; x++) if (GRID(x, 18, 1) != CELL_WALL) fence++;
    if (fence == 0) { snprintf(why, (size_t)whyLen, "the wooden fence held"); return false; }
    int blasted = boomGraniteBefore - BoomCountGranite();
    if (blasted < 5) { snprintf(why, (size_t)whyLen, "only %d granite cells blasted (want >= 5)", blasted); return false; }
    if (GRID(12, 12, 1) != CELL_WALL) { snprintf(why, (size_t)whyLen, "the granite shelter wall broke"); return false; }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// throwing: four lanes, a mover at the west end of each with a sharp stone, throwing it at a
// goal cell again and again (it fetches the stone where it came to rest and throws from there).
// 0 open ground, 8 cells. 1 far (13 cells) with a granite wall 3 cells behind the goal: long
// throws bounce off it. 2 a 2-high wall in the way: the thrower lobs over it. 3 the goal on top
// of a 2-high granite block (z3): throwing uphill. Prints per lane how close the stones come
// down to the goal; --check: every lane threw, lane 0 mostly lands near, nothing in a wall,
// the state audit is clean.
#define THROW_LANES 4
static int throwLaneMover[THROW_LANES], throwLaneItem[THROW_LANES];
static int throwLaneGoal[THROW_LANES][3];
static bool throwLaneFlying[THROW_LANES];
static int throwLaneThrows[THROW_LANES], throwLaneNear[THROW_LANES], throwLaneOnLevel[THROW_LANES];
static float throwLaneMissSum[THROW_LANES];
static int throwInWall;
static const int throwLaneStartY[THROW_LANES] = { 5, 12, 19, 27 };

static void ThrowWallBox(int x0, int y0, int x1, int y1, int z0, int z1) {
    for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) { GRID(x, y, z) = CELL_WALL; SetWallMaterial(x, y, z, MAT_GRANITE); }
}

static void ThrowLaneOrder(int k) {
    ThrowOrder(throwLaneMover[k], throwLaneItem[k], throwLaneGoal[k][0], throwLaneGoal[k][1], throwLaneGoal[k][2], NULL);
}

static void SetupThrowing(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;   // they stand where they are put
    ThrowWallBox(20, 9, 20, 15, 1, 2);    // lane 1: the wall behind the goal
    ThrowWallBox(8, 17, 8, 21, 1, 2);     // lane 2: the wall in the way
    ThrowWallBox(12, 25, 16, 29, 1, 2);   // lane 3: the block (top at z3)
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    const int* laneY = throwLaneStartY;
    static const int goal[THROW_LANES][3] = { { 12, 5, 1 }, { 17, 12, 1 }, { 13, 19, 1 }, { 14, 27, 3 } };
    for (int k = 0; k < THROW_LANES; k++) {
        throwLaneMover[k] = moverCount;
        SpawnMoverAt(4, laneY[k], 1);
        throwLaneItem[k] = SpawnItemWithMaterial(3.5f * CELL_SIZE, (laneY[k] + 0.5f) * CELL_SIZE, 1.0f,
                                                 ITEM_SHARP_STONE, DefaultMaterialForItemType(ITEM_SHARP_STONE));
        for (int a = 0; a < 3; a++) throwLaneGoal[k][a] = goal[k][a];
        throwLaneFlying[k] = false;
        throwLaneThrows[k] = throwLaneNear[k] = throwLaneOnLevel[k] = 0;
        throwLaneMissSum[k] = 0.0f;
        ThrowLaneOrder(k);
    }
    throwInWall = 0;
    currentViewZ = 1;
    AddMessage("Throwing: four movers throw a stone at a goal again and again (open, far + wall behind, over a wall, uphill)", GREEN);
}

static void ThrowingTickStats(void) {
    for (int k = 0; k < THROW_LANES; k++) {
        int it = throwLaneItem[k], m = throwLaneMover[k];
        if (it < 0 || !items[it].active || m >= moverCount || !movers[m].active) continue;
        if (items[it].state == ITEM_IN_FLIGHT) { throwLaneFlying[k] = true; continue; }
        if (throwLaneFlying[k]) {   // came to rest: how close?
            throwLaneFlying[k] = false;
            const Item* t = &items[it];
            float dx = t->x / CELL_SIZE - (throwLaneGoal[k][0] + 0.5f), dy = t->y / CELL_SIZE - (throwLaneGoal[k][1] + 0.5f);
            float d = sqrtf(dx * dx + dy * dy);
            bool onLevel = (int)t->z == throwLaneGoal[k][2];
            throwLaneThrows[k]++;
            throwLaneMissSum[k] += d;
            if (onLevel) throwLaneOnLevel[k]++;
            if (onLevel && d <= 1.5f) throwLaneNear[k]++;
            int cx = (int)(t->x / CELL_SIZE), cy = (int)(t->y / CELL_SIZE), cz = (int)t->z;
            if (CellIsSolid(GRID(cx, cy, cz)) || CellBlocksMovement(GRID(cx, cy, cz))) throwInWall++;
            // Back to the throwing spot for the next one (the mover walks back to fetch it)
            items[it].x = 3.5f * CELL_SIZE;
            items[it].y = (throwLaneStartY[k] + 0.5f) * CELL_SIZE;
            items[it].z = 1.0f;
        }
        if (movers[m].currentJobId < 0 && items[it].reservedBy < 0) ThrowLaneOrder(k);
    }
}

static void ThrowingPrintStats(void) {
    static const char* names[THROW_LANES] = { "open 8", "far 13 + wall behind", "over a wall", "uphill (z3)" };
    printf("Throwing: %ld throws, %ld bounces, %d stones ended in a wall\n", throwCount, throwBounceCount, throwInWall);
    for (int k = 0; k < THROW_LANES; k++) {
        int n = throwLaneThrows[k];
        printf("  lane %d %-22s throws %3d  within 1.5 cells %3d (%3.0f%%)  on the goal's level %3d  mean miss %.1f cells\n",
               k, names[k], n, throwLaneNear[k], n ? 100.0f * throwLaneNear[k] / n : 0.0f, throwLaneOnLevel[k],
               n ? throwLaneMissSum[k] / n : 0.0f);
    }
}

static bool ThrowingCheck(char* why, int whyLen) {
    for (int k = 0; k < THROW_LANES; k++)
        if (throwLaneThrows[k] < 3) { snprintf(why, (size_t)whyLen, "lane %d threw only %d times", k, throwLaneThrows[k]); return false; }
    if (throwLaneNear[0] * 2 < throwLaneThrows[0]) {
        snprintf(why, (size_t)whyLen, "open lane: only %d of %d throws within 1.5 cells", throwLaneNear[0], throwLaneThrows[0]);
        return false;
    }
    if (throwInWall > 0) { snprintf(why, (size_t)whyLen, "%d stones came to rest inside a wall", throwInWall); return false; }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// throwscare: three movers each with a sharp stone and a grazer about 8 cells east of them. A
// mover throws at the cell its grazer stands on; a hit dazes it, then it runs off east (away
// from the thrower); a stone coming down next to it scares it right away. Headless: the stone goes back to the thrower and it throws again at wherever the
// grazer is now (out of range: it walks closer first). Prints throws, hits and how far the grazers
// got; --check: stones hit, hit grazers ran, nothing in a wall, the state audit is clean.
#define SCARE_LANES 3
static int scareMover[SCARE_LANES], scareItem[SCARE_LANES], scareAnimal[SCARE_LANES];
static int scareThrows[SCARE_LANES], scareHits[SCARE_LANES];
static bool scareFlying[SCARE_LANES];
static long scareHitsSeen;
static float scareStartX[SCARE_LANES], scareFarthest[SCARE_LANES];
static const int scareLaneY[SCARE_LANES] = { 6, 16, 26 };

static void ScareOrder(int k) {
    const Animal* a = &animals[scareAnimal[k]];
    if (!a->active) return;
    ThrowOrder(scareMover[k], scareItem[k], (int)(a->x / CELL_SIZE), (int)(a->y / CELL_SIZE), (int)a->z, NULL);
}

static void SetupThrowScare(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    animalRespawnEnabled = false;
    for (int k = 0; k < SCARE_LANES; k++) {
        int y = scareLaneY[k];
        scareMover[k] = moverCount;
        SpawnMoverAt(4, y, 1);
        scareItem[k] = SpawnItemWithMaterial(4.5f * CELL_SIZE, (y + 0.5f) * CELL_SIZE, 1.0f,
                                             ITEM_SHARP_STONE, DefaultMaterialForItemType(ITEM_SHARP_STONE));
        SpawnAnimal(ANIMAL_GRAZER, 1, BEHAVIOR_SIMPLE_GRAZER);
        scareAnimal[k] = animalCount - 1;
        Animal* a = &animals[scareAnimal[k]];
        a->x = 12.5f * CELL_SIZE; a->y = (y + 0.5f) * CELL_SIZE; a->z = 1.0f;
        a->targetCellX = 12; a->targetCellY = y;
        a->state = ANIMAL_IDLE; a->stateTimer = 0.0f;
        scareStartX[k] = scareFarthest[k] = 12.5f;
        scareThrows[k] = scareHits[k] = 0;
        scareFlying[k] = false;
        ScareOrder(k);
    }
    scareHitsSeen = 0;
    throwInWall = 0;
    currentViewZ = 1;
    AddMessage("Throwing at animals: a hit grazer runs away from the thrower", GREEN);
}

static void ThrowScareTickStats(void) {
    for (int k = 0; k < SCARE_LANES; k++) {
        int it = scareItem[k], m = scareMover[k];
        const Animal* a = &animals[scareAnimal[k]];
        if (a->active && a->x / CELL_SIZE > scareFarthest[k]) scareFarthest[k] = a->x / CELL_SIZE;
        if (it < 0 || !items[it].active || !movers[m].active) continue;
        if (items[it].state == ITEM_IN_FLIGHT) { scareFlying[k] = true; continue; }
        if (scareFlying[k]) {
            scareFlying[k] = false;
            scareThrows[k]++;
            if (AnimalDazed(scareAnimal[k]) && throwHitCount > scareHitsSeen) scareHits[k]++;
            scareHitsSeen = throwHitCount;
            const Item* t = &items[it];
            int cx = (int)(t->x / CELL_SIZE), cy = (int)(t->y / CELL_SIZE), cz = (int)t->z;
            if (CellIsSolid(GRID(cx, cy, cz)) || CellBlocksMovement(GRID(cx, cy, cz))) throwInWall++;
            items[it].x = movers[m].x; items[it].y = movers[m].y; items[it].z = movers[m].z;   // hand it back
        }
        if (movers[m].currentJobId < 0 && items[it].reservedBy < 0 && !AnimalScared(scareAnimal[k]) &&
            !AnimalDazed(scareAnimal[k])) ScareOrder(k);
    }
}

static void ThrowScarePrintStats(void) {
    printf("Throwing at animals: %ld throws, %ld hits, %ld near misses that scared, %d stones ended in a wall\n",
           throwCount, throwHitCount, throwStartleCount, throwInWall);
    for (int k = 0; k < SCARE_LANES; k++)
        printf("  lane %d: throws %3d  hits %3d  grazer ran from x %.1f to x %.1f\n",
               k, scareThrows[k], scareHits[k], scareStartX[k], scareFarthest[k]);
}

static bool ThrowScareCheck(char* why, int whyLen) {
    int hit = 0;
    for (int k = 0; k < SCARE_LANES; k++) {
        if (scareThrows[k] < 2) { snprintf(why, (size_t)whyLen, "lane %d threw only %d times", k, scareThrows[k]); return false; }
        if (scareHits[k] > 0) {
            hit++;
            if (scareFarthest[k] < scareStartX[k] + 4.0f) {
                snprintf(why, (size_t)whyLen, "lane %d: grazer hit but only got to x %.1f", k, scareFarthest[k]);
                return false;
            }
        }
    }
    if (hit < 2) { snprintf(why, (size_t)whyLen, "only %d of %d grazers were hit", hit, SCARE_LANES); return false; }
    if (throwInWall > 0) { snprintf(why, (size_t)whyLen, "%d stones came to rest inside a wall", throwInWall); return false; }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// throwhunt: hunting deer that outrun people (animalFleeSpeed 4). Two lanes walled apart, a
// hunter at the west end and a marked deer 13 cells east in each. Lane 0 has a pile of sharp
// stones by the hunter: it fetches one, throws from a distance (a hit dazes the deer), walks
// in and kills it, fetching the stone again after a miss. Lane 1 has no stones: it can only
// run after the deer. Headless: each kill puts a new marked deer back in its lane. Prints
// kills, throws and hits per lane; --check: the stone lane killed, more than the chase lane,
// no stone stuck in a wall, the state audit is clean.
#define HUNTLANES 2
static int huntLaneMover[HUNTLANES], huntLaneDeer[HUNTLANES], huntLaneKills[HUNTLANES];
static int huntLaneCarcasses[HUNTLANES];
static long huntLaneThrows0;
static const int huntLaneY[HUNTLANES] = { 7, 23 };

static void ThrowHuntDeer(int k) {
    SpawnAnimal(ANIMAL_GRAZER, 1, BEHAVIOR_SIMPLE_GRAZER);
    int d = animalCount - 1;
    for (int i = 0; i < animalCount; i++)   // SpawnAnimal reuses the first free slot
        if (animals[i].active && !animals[i].markedForHunt && i != huntLaneDeer[1 - k]) { d = i; break; }
    Animal* a = &animals[d];
    // 13 cells from the hunter, at the end of the lane away from it
    const Mover* h = huntLaneMover[k] < moverCount ? &movers[huntLaneMover[k]] : NULL;
    float hx = h ? h->x / CELL_SIZE : 4.5f;
    int ax = hx < gridWidth / 2 ? (int)hx + 13 : (int)hx - 13;
    a->x = (ax + 0.5f) * CELL_SIZE; a->y = (huntLaneY[k] + 0.5f) * CELL_SIZE; a->z = 1.0f;
    a->targetCellX = ax; a->targetCellY = huntLaneY[k];
    a->state = ANIMAL_IDLE; a->stateTimer = 0.0f;
    a->markedForHunt = true;
    a->reservedByHunter = -1;
    huntLaneDeer[k] = d;
}

static void SetupThrowHunt(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    animalRespawnEnabled = false;
    animalFleeSpeed = 4.0f;
    huntThrowStones = true;
    ThrowWallBox(0, 15, gridWidth - 1, 16, 1, 2);   // keeps the lanes apart
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    huntLaneDeer[0] = huntLaneDeer[1] = -1;
    for (int k = 0; k < HUNTLANES; k++) {
        huntLaneMover[k] = moverCount;
        SpawnMoverAt(4, huntLaneY[k], 1);
        huntLaneMover[k] = moverCount - 1;
        huntLaneKills[k] = huntLaneCarcasses[k] = 0;
        ThrowHuntDeer(k);
    }
    int pile = SpawnItemWithMaterial(3.5f * CELL_SIZE, (huntLaneY[0] + 1.5f) * CELL_SIZE, 1.0f,
                                     ITEM_SHARP_STONE, DefaultMaterialForItemType(ITEM_SHARP_STONE));
    if (pile >= 0) items[pile].stackCount = 5;
    huntLaneThrows0 = 0;
    currentViewZ = 1;
    AddMessage("Hunting fast deer: the hunter with stones (top) dazes them from a distance; the other (bottom) can only chase", GREEN);
}

static void ThrowHuntTickStats(void) {
    for (int k = 0; k < HUNTLANES; k++) {
        int d = huntLaneDeer[k];
        if (d >= 0 && animals[d].active) continue;
        huntLaneKills[k]++;
        ThrowHuntDeer(k);
    }
}

static void ThrowHuntPrintStats(void) {
    printf("Hunting fast deer (flee speed %.1f): %ld throws, %ld hits (dazed), %ld near misses\n",
           animalFleeSpeed, throwCount, throwHitCount, throwStartleCount);
    printf("  lane 0 (stones):    kills %d\n", huntLaneKills[0]);
    printf("  lane 1 (no stones): kills %d\n", huntLaneKills[1]);
}

static bool ThrowHuntCheck(char* why, int whyLen) {
    if (huntLaneKills[0] < 2) { snprintf(why, (size_t)whyLen, "the stone hunter killed only %d deer", huntLaneKills[0]); return false; }
    if (huntLaneKills[0] <= huntLaneKills[1]) {
        snprintf(why, (size_t)whyLen, "stones didn't help: %d kills vs %d chasing", huntLaneKills[0], huntLaneKills[1]);
        return false;
    }
    if (throwHitCount < 2) { snprintf(why, (size_t)whyLen, "only %ld hits", throwHitCount); return false; }
    for (int i = 0; i < itemHighWaterMark; i++) {
        const Item* t = &items[i];
        if (!t->active || t->type != ITEM_SHARP_STONE || t->state != ITEM_ON_GROUND) continue;
        int cx = (int)(t->x / CELL_SIZE), cy = (int)(t->y / CELL_SIZE), cz = (int)t->z;
        if (CellIsSolid(GRID(cx, cy, cz)) || CellBlocksMovement(GRID(cx, cy, cz))) {
            snprintf(why, (size_t)whyLen, "a stone came to rest inside a wall at (%d,%d,%d)", cx, cy, cz);
            return false;
        }
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// collapse: structural support. Three charges on fuses take the foot out of three structures:
// a granite pillar with an arm at the top (2 s), a granite roof slab on one pillar with a
// mover under it (3 s), a wooden deck on one post with a log on it (4 s). Each piece creaks
// (dust), falls collapseWarning seconds later and lands as walls; the deck breaks into planks.
// Check: nothing left hanging, the slab came down and its mover was pushed out (not buried),
// the deck and the log came down, the state audit is clean.
static int collapseMover;
static int collapseLog;
static bool collapseKnocked;

static int CollapseCount(int x0, int x1, int y0, int y1, int z, MaterialType m) {
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (GRID(x, y, z) == CELL_WALL && GetWallMaterial(x, y, z) == m) n++;
    return n;
}

static void SetupCollapse(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    supportEnabled = true;
    collapseKnocked = false;
    // A: pillar with an arm
    for (int z = 1; z <= 5; z++) { GRID(8, 8, z) = CELL_WALL; SetWallMaterial(8, 8, z, MAT_GRANITE); }
    for (int x = 9; x <= 11; x++) { GRID(x, 8, 5) = CELL_WALL; SetWallMaterial(x, 8, 5, MAT_GRANITE); }
    // B: roof slab (z3, 15 x 3) on one pillar at its east end
    for (int z = 1; z <= 2; z++) { GRID(34, 11, z) = CELL_WALL; SetWallMaterial(34, 11, z, MAT_GRANITE); }
    for (int y = 10; y <= 12; y++)
        for (int x = 20; x <= 34; x++) { GRID(x, y, 3) = CELL_WALL; SetWallMaterial(x, y, 3, MAT_GRANITE); }
    // C: wooden deck (z4) on one post, a log on it
    for (int z = 1; z <= 3; z++) { GRID(40, 22, z) = CELL_WALL; SetWallMaterial(40, 22, z, MAT_OAK); }
    for (int y = 20; y <= 24; y++)
        for (int x = 40; x <= 44; x++) { SET_FLOOR(x, y, 4); SetFloorMaterial(x, y, 4, MAT_OAK); }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    collapseLog = SpawnItemWithMaterial(43.5f * CELL_SIZE, 23.5f * CELL_SIZE, 4.0f, ITEM_LOG, MAT_OAK);
    collapseMover = moverCount;
    SpawnMoverAt(22, 11, 1);
    ExplosionLightFuse(8, 8, 1, 5.0f, 2.0f);
    ExplosionLightFuse(34, 11, 1, 5.0f, 3.0f);
    ExplosionLightFuse(40, 22, 1, 4.0f, 4.0f);
    currentViewZ = 1;
    AddMessage("Collapse: three charges take the foot out of a pillar (2 s), a roof slab (3 s) and a deck (4 s)", ORANGE);
}

static void CollapseTickStats(void) {
    if (collapseMover < moverCount && MoverKnocked(collapseMover)) collapseKnocked = true;
}

static void CollapsePrintStats(void) {
    printf("Collapse: pillar+arm granite at z5 %d (was 4), roof granite at z3 %d (was 45) / at z1 %d\n",
           CollapseCount(8, 11, 8, 8, 5, MAT_GRANITE), CollapseCount(20, 34, 10, 12, 3, MAT_GRANITE),
           CollapseCount(20, 34, 10, 12, 1, MAT_GRANITE));
    int deck = 0;
    for (int y = 20; y <= 24; y++) for (int x = 40; x <= 44; x++) if (HAS_FLOOR(x, y, 4)) deck++;
    printf("  deck floors left %d (was 25), log at z%d, roof mover knocked %s, unsupported cells %d, pending %d\n",
           deck, collapseLog >= 0 ? (int)items[collapseLog].z : -1, collapseKnocked ? "yes" : "no",
           SupportCountUnsupported(), SupportPendingCount());
}

static bool CollapseCheck(char* why, int whyLen) {
    if (ExplosionFuseCount() != 0 || SupportPendingCount() != 0) {
        snprintf(why, (size_t)whyLen, "still waiting: %d fuses, %d collapses", ExplosionFuseCount(), SupportPendingCount());
        return false;
    }
    int un = SupportCountUnsupported();
    if (un != 0) { snprintf(why, (size_t)whyLen, "%d structure cells left hanging", un); return false; }
    if (CollapseCount(8, 11, 8, 8, 5, MAT_GRANITE) != 0) { snprintf(why, (size_t)whyLen, "the pillar's arm is still up"); return false; }
    if (CollapseCount(20, 34, 10, 12, 3, MAT_GRANITE) != 0) { snprintf(why, (size_t)whyLen, "the roof slab is still up"); return false; }
    if (CollapseCount(20, 33, 10, 12, 1, MAT_GRANITE) < 30) { snprintf(why, (size_t)whyLen, "the roof slab didn't land on the ground"); return false; }
    for (int y = 20; y <= 24; y++)
        for (int x = 40; x <= 44; x++)
            if (HAS_FLOOR(x, y, 4)) { snprintf(why, (size_t)whyLen, "deck floor at %d,%d still up", x, y); return false; }
    if (collapseLog < 0 || !items[collapseLog].active || (int)items[collapseLog].z >= 4) {
        snprintf(why, (size_t)whyLen, "the log on the deck didn't come down"); return false;
    }
    if (!collapseKnocked) { snprintf(why, (size_t)whyLen, "the mover under the roof wasn't pushed aside"); return false; }
    const Mover* m = &movers[collapseMover];
    if (CellIsSolid(GRID((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z))) {
        snprintf(why, (size_t)whyLen, "the mover under the roof is buried"); return false;
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// blasting: blast mining end to end. A granite hill (z1-3) with 4 walls on its face marked
// for blasting; 3 gunpowder lie by the stockpile and a hearth + quern make 2 more: the hearth
// leaches one saltpeter from compost, ash and water, the quern grinds it and the saltpeter
// already there, each with a charcoal (Leach Saltpeter, Grind Gunpowder). 3 miners fetch a charge each,
// set it, light it and get clear before the bang. Check: every mark blasted (the 4th charge
// came from the chain), nobody was caught by a blast, nothing hanging, the state audit is clean.
static int blastGraniteBefore;
static bool blastSomeoneKnocked;

static int BlastCountGranite(void) {
    int n = 0;
    for (int z = 1; z <= 3; z++)
        for (int y = 4; y <= 28; y++)
            for (int x = 30; x <= 44; x++)
                if (GRID(x, y, z) == CELL_WALL && GetWallMaterial(x, y, z) == MAT_GRANITE) n++;
    return n;
}

static int RecipeByName(WorkshopType type, const char* name) {
    for (int i = 0; i < workshopDefs[type].recipeCount; i++)
        if (strcmp(workshopDefs[type].recipes[i].name, name) == 0) return i;
    return -1;
}

// One stack of n on (x, y): a recipe takes its count of an input from one stack
static void SpawnPile(int x, int y, ItemType type, int n) {
    int idx = SpawnItem((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, 1.0f, type);
    if (idx >= 0) items[idx].stackCount = n;
}

static void SetupBlasting(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    toolRequirementsEnabled = false;
    blastSomeoneKnocked = false;
    for (int z = 1; z <= 3; z++)
        for (int y = 4; y <= 28; y++)
            for (int x = 30; x <= 44; x++) {
                GRID(x, y, z) = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_GRANITE);
                SetWallNatural(x, y, z);
            }
    blastGraniteBefore = BlastCountGranite();
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    // Saltpeter and gunpowder only: the raw things lie by their workshop, rubble stays where it lands
    int sp = CreateStockpile(4, 4, 1, 6, 6);
    for (int t = 0; t < ITEM_TYPE_COUNT && sp >= 0; t++)
        SetStockpileFilter(sp, (ItemType)t, t == ITEM_SALTPETER || t == ITEM_GUNPOWDER);
    int hearth = CreateWorkshop(14, 5, 1, WORKSHOP_HEARTH);
    int quern = CreateWorkshop(14, 12, 1, WORKSHOP_QUERN);
    if (hearth >= 0) AddBill(hearth, RecipeByName(WORKSHOP_HEARTH, "Leach Saltpeter"), BILL_DO_X_TIMES, 1);
    if (quern >= 0) AddBill(quern, RecipeByName(WORKSHOP_QUERN, "Grind Gunpowder"), BILL_DO_X_TIMES, 2);
    SpawnPile(11, 5, ITEM_COMPOST, 2);
    SpawnPile(11, 6, ITEM_ASH, 1);
    SpawnPile(12, 6, ITEM_WATER, 1);
    SpawnPile(11, 12, ITEM_CHARCOAL, 2);
    SpawnPile(5, 5, ITEM_SALTPETER, 1);    // + the one leached: two grinds
    SpawnPile(5, 7, ITEM_GUNPOWDER, 3);
    for (int k = 0; k < 4; k++) DesignateBlast(30, 7 + k * 6, 1);   // one more than the gunpowder ready
    for (int i = 0; i < 3; i++) SpawnMoverAt(20, 8 + i * 6, 1);
    currentViewZ = 1;
    AddMessage("Blasting: 4 walls marked, 3 gunpowder ready, a hearth and a quern make 2 more", ORANGE);
}

static void BlastingTickStats(void) {
    for (int i = 0; i < moverCount; i++) if (movers[i].active && MoverKnocked(i)) blastSomeoneKnocked = true;
}

static int BlastCountItems(ItemType t) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == t) n += items[i].stackCount;
    return n;
}

static void BlastingPrintStats(void) {
    EventLogDump("/private/tmp/claude-501/-Users-nikkikoole-Projects-navkit/3a468705-3aac-436e-805d-780b1cc7def2/scratchpad/blast_events.log");   // DBG
    printf("Blasting: marks left %d, fuses %d, granite blasted %d, gunpowder left %d, saltpeter %d, compost %d, "
           "someone caught by a blast %s, unsupported %d\n",
           CountBlastDesignations(), ExplosionFuseCount(), blastGraniteBefore - BlastCountGranite(),
           BlastCountItems(ITEM_GUNPOWDER), BlastCountItems(ITEM_SALTPETER), BlastCountItems(ITEM_COMPOST),
           blastSomeoneKnocked ? "yes" : "no", SupportCountUnsupported());
}

static bool BlastingCheck(char* why, int whyLen) {
    if (CountBlastDesignations() != 0) { snprintf(why, (size_t)whyLen, "%d walls still marked for blasting", CountBlastDesignations()); return false; }
    if (ExplosionFuseCount() != 0) { snprintf(why, (size_t)whyLen, "a fuse is still burning"); return false; }
    int blasted = blastGraniteBefore - BlastCountGranite();
    if (blasted < 20) { snprintf(why, (size_t)whyLen, "only %d granite cells blasted (want >= 20 for 4 charges)", blasted); return false; }
    if (BlastCountItems(ITEM_COMPOST) > 0) { snprintf(why, (size_t)whyLen, "the hearth didn't leach the compost"); return false; }
    if (blastSomeoneKnocked) { snprintf(why, (size_t)whyLen, "a blaster was caught by a blast"); return false; }
    int un = SupportCountUnsupported();
    if (un) { snprintf(why, (size_t)whyLen, "%d structure cells hanging", un); return false; }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// powder-fire: gunpowder and fire. A grass fire is lit 5 cells west of a single gunpowder (A);
// 3 cells east of A lies a pile of 8 (B, one charge of twice the power); far east, past a
// firebreak (no grass) and behind a granite wall, one more (C). The fire creeps through the
// grass to A and B (a spark, or A's blast reaching B: whichever comes first). Check: A and B
// went off, C is still there, the state audit is clean.
static int powderC;

static int PowderCount(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++) if (items[i].active && items[i].type == ITEM_GUNPOWDER) n += items[i].stackCount;
    return n;
}

static void SetupPowderFire(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    fireEnabled = true;
    for (int y = 8; y <= 24; y++)
        for (int z = 1; z <= 3; z++) { GRID(36, y, z) = CELL_WALL; SetWallMaterial(36, y, z, MAT_GRANITE); }
    // A firebreak: no grass in x 30..34, so the fire stops there
    for (int y = 0; y < gridHeight; y++)
        for (int x = 30; x <= 34; x++) SetVegetation(x, y, 0, VEG_NONE);
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    SpawnItem(20.5f * CELL_SIZE, 16.5f * CELL_SIZE, 1.0f, ITEM_GUNPOWDER);
    int b = SpawnItem(23.5f * CELL_SIZE, 16.5f * CELL_SIZE, 1.0f, ITEM_GUNPOWDER);
    if (b >= 0) items[b].stackCount = 8;
    powderC = SpawnItem(42.5f * CELL_SIZE, 16.5f * CELL_SIZE, 1.0f, ITEM_GUNPOWDER);
    IgniteCell(15, 16, 0);   // the grass burns in the soil cell (z0) under the walking level
    currentViewZ = 1;
    AddMessage("Powder fire: a grass fire creeps toward gunpowder (1, a pile of 8, and 1 behind a wall)", ORANGE);
}

static void PowderFirePrintStats(void) {
    printf("Powder fire: explosions %d, gunpowder left %d (want 1: the one behind the wall), fuses %d, fire cells %d\n",
           ExplosionCount(), PowderCount(), ExplosionFuseCount(), fireActiveCells);
}

static bool PowderFireCheck(char* why, int whyLen) {
    if (ExplosionCount() < 2) { snprintf(why, (size_t)whyLen, "only %d explosions (want the single one and the pile)", ExplosionCount()); return false; }
    if (PowderCount() != 1 || !items[powderC].active || items[powderC].type != ITEM_GUNPOWDER) {
        snprintf(why, (size_t)whyLen, "gunpowder left %d: want only the one behind the wall", PowderCount()); return false;
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// flood: blasting into water. A granite hill holds a pond (3 levels deep, open to the sky) and,
// behind a wall 2 cells thick, a dry tunnel out to the field. A charge drilled into the wall
// (3 s) opens it: the pond runs into the tunnel and out. A second pond dug into the field gets a
// charge over its middle (1.5 s): the water is thrown out over the bank and runs back.
// Check: water in the tunnel, the field pond's water thrown out, state audit clean.
static int floodSplashThrown, floodTunnelMost;

static int FloodWaterIn(int x0, int x1, int y0, int y1, int z0, int z1) {
    int n = 0;
    for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) n += GetWaterLevel(x, y, z);
    return n;
}

static void SetupFlood(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    waterEnabled = true;
    floodSplashThrown = floodTunnelMost = 0;
    for (int z = 1; z <= 4; z++)
        for (int y = 6; y <= 26; y++)
            for (int x = 16; x <= 40; x++) {
                GRID(x, y, z) = CELL_WALL; SetWallMaterial(x, y, z, MAT_GRANITE); SetWallNatural(x, y, z);
            }
    // The pond: x 18..24, y 10..22, open to the sky, water 3 levels deep
    for (int z = 1; z <= 4; z++)
        for (int y = 10; y <= 22; y++)
            for (int x = 18; x <= 24; x++) {
                GRID(x, y, z) = CELL_AIR; SetWallMaterial(x, y, z, MAT_NONE); ClearWallNatural(x, y, z);
                if (z <= 3) SetWaterLevel(x, y, z, WATER_MAX_LEVEL);
            }
    // The tunnel: x 27..40 at y 15..16, z1, out to the field (the wall x 25..26 holds the pond)
    for (int y = 15; y <= 16; y++)
        for (int x = 27; x <= 40; x++) { GRID(x, y, 1) = CELL_AIR; SetWallMaterial(x, y, 1, MAT_NONE); ClearWallNatural(x, y, 1); }
    // The field pond: dug into the ground (z0), x 4..10, y 12..20, full
    for (int y = 12; y <= 20; y++)
        for (int x = 4; x <= 10; x++) {
            GRID(x, y, 0) = CELL_AIR; SetWallMaterial(x, y, 0, MAT_NONE); SetVegetation(x, y, 0, VEG_NONE);
            SetWaterLevel(x, y, 0, WATER_MAX_LEVEL);
        }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    ExplosionLightFuse(26, 16, 1, 5.0f, 3.0f);   // drilled into the wall's tunnel face
    ExplosionLightFuse(7, 16, 1, 5.0f, 1.5f);    // just over the field pond
    currentViewZ = 1;
    AddMessage("Flood: a charge over a pond in the field (1.5 s), one drilled into the wall holding a pond back from a tunnel (3 s)", ORANGE);
}

static void FloodTickStats(void) {
    // Before the wall goes (the hill pond's water reaches the whole field later)
    if (ExplosionCount() == 1) {
        int thrown = FloodWaterIn(0, 15, 0, gridHeight - 1, 0, 3) - FloodWaterIn(4, 10, 12, 20, 0, 0);
        if (thrown > floodSplashThrown) floodSplashThrown = thrown;
    }
    int tunnel = FloodWaterIn(27, 40, 15, 16, 1, 1);
    if (tunnel > floodTunnelMost) floodTunnelMost = tunnel;
}

static void FloodPrintStats(void) {
    printf("Flood: explosions %d, water in the tunnel %d (most %d), in the hill pond %d (was %d), field pond %d (was %d), thrown out of it %d\n",
           ExplosionCount(), FloodWaterIn(27, 40, 15, 16, 1, 1), floodTunnelMost, FloodWaterIn(18, 24, 10, 22, 1, 4),
           7 * 13 * 3 * 7, FloodWaterIn(4, 10, 12, 20, 0, 0), 7 * 7 * 9, floodSplashThrown);
}

static bool FloodCheck(char* why, int whyLen) {
    if (ExplosionCount() < 2) { snprintf(why, (size_t)whyLen, "only %d explosions", ExplosionCount()); return false; }
    if (floodTunnelMost < 20) { snprintf(why, (size_t)whyLen, "the tunnel stayed dry"); return false; }
    if (FloodWaterIn(18, 24, 10, 22, 1, 4) > 7 * 13 * 3 * 7 / 2) { snprintf(why, (size_t)whyLen, "the hill pond didn't run out"); return false; }
    if (floodSplashThrown < 20) { snprintf(why, (size_t)whyLen, "the field charge threw only %d water out of its pond", floodSplashThrown); return false; }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// landslide: a flat-topped dirt hill (4 levels) with a sheer west face and a sand bank by it;
// a charge on top near the edge (2 s) shakes the face loose: dirt slides off the edge, lands at
// the foot and sets off a bit more. A mover stands at the foot (pushed aside when dirt lands on
// him), one on top. The granite hill further on has the same face and never slides.
// Check: some dirt slid (not the whole face), nobody inside a wall, granite face whole, audit clean.
static int SlideWallsAt(int x0, int x1, int y0, int y1, int z0, int z1, MaterialType m) {
    int n = 0;
    for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if (GRID(x, y, z) == CELL_WALL && GetWallMaterial(x, y, z) == m) n++;
    return n;
}

static void SetupLandslide(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    landslidesEnabled = true;
    supportEnabled = true;
    for (int z = 1; z <= 4; z++)
        for (int y = 4; y <= 27; y++) {
            for (int x = 16; x <= 28; x++) { GRID(x, y, z) = CELL_WALL; SetWallMaterial(x, y, z, MAT_DIRT); SetWallNatural(x, y, z); }
            for (int x = 36; x <= 44; x++) { GRID(x, y, z) = CELL_WALL; SetWallMaterial(x, y, z, MAT_GRANITE); SetWallNatural(x, y, z); }
        }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    SpawnMoverAt(15, 16, 1);   // at the foot of the face
    SpawnMoverAt(22, 20, 5);   // on top
    ExplosionLightFuse(18, 16, 5, 4.0f, 2.0f);   // on top, 2 cells from the edge
    ExplosionLightFuse(38, 16, 5, 4.0f, 2.5f);   // the same on the granite hill
    currentViewZ = 2;
    AddMessage("Landslide: a charge on a dirt hill near its edge (2 s), the same on a granite hill (2.5 s)", ORANGE);
}

static void LandslidePrintStats(void) {
    printf("Landslide: explosions %d, slides %d (pending %d), dirt beyond the face %d, granite beyond its face %d\n",
           ExplosionCount(), LandslideCount(), LandslidePendingCount(),
           SlideWallsAt(0, 15, 0, gridHeight - 1, 1, 4, MAT_DIRT), SlideWallsAt(30, 35, 0, gridHeight - 1, 1, 4, MAT_GRANITE));
}

static bool LandslideCheck(char* why, int whyLen) {
    if (ExplosionCount() < 2) { snprintf(why, (size_t)whyLen, "only %d explosions", ExplosionCount()); return false; }
    if (SlideWallsAt(0, 15, 0, gridHeight - 1, 1, 4, MAT_DIRT) < 3) { snprintf(why, (size_t)whyLen, "hardly any dirt slid off the face"); return false; }
    if (LandslideCount() > 150) { snprintf(why, (size_t)whyLen, "%d slides: the whole face came down", LandslideCount()); return false; }
    if (SlideWallsAt(30, 35, 0, gridHeight - 1, 1, 4, MAT_GRANITE) > 0) { snprintf(why, (size_t)whyLen, "granite slid"); return false; }
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        int mx = (int)(movers[i].x / CELL_SIZE), my = (int)(movers[i].y / CELL_SIZE), mz = (int)movers[i].z;
        if (CellIsSolid(GRID(mx, my, mz))) { snprintf(why, (size_t)whyLen, "mover %d is inside a wall", i); return false; }
    }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

// flashfire: petrol vapour. A leak on bare ground (8 s) by a pit dug into the field: the fumes
// spread over the ground, run into the pit and pool there. At 10 s a spark at the far edge of the
// cloud (someone lights a pipe): the flame runs back through the whole cloud and down into the
// pit (a mover stands in it, one by the leak: both pushed), leaving the ground hot and a little
// smoke. Check: the cloud reached the pit, it burned through (more than 40 cells), nothing is
// left, the movers were pushed, audit clean.
static int flashMostCells, flashPitMost;
static bool flashKnocked[2];
static int flashMover[2];

static void SetupFlashFire(void) {
    hungerEnabled = thirstEnabled = energyEnabled = false;
    endlessMoverMode = false;
    fireEnabled = true;
    vapourEnabled = true;
    flashMostCells = flashPitMost = 0;
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) SetVegetation(x, y, 0, VEG_NONE);   // bare: no grass fire
    // The pit: x 16..23, y 12..20, one level into the ground
    for (int y = 12; y <= 20; y++)
        for (int x = 16; x <= 23; x++) { GRID(x, y, 0) = CELL_AIR; SetWallMaterial(x, y, 0, MAT_NONE); }
    BuildEntrances();
    BuildGraph();
    InvalidateLighting();
    for (int k = 0; k < 2; k++) { flashKnocked[k] = false; flashMover[k] = moverCount; SpawnMoverAt(k == 0 ? 18 : 12, k == 0 ? 16 : 18, k == 0 ? 0 : 1); }
    VapourLeak(13, 16, 1, 20.0f, 8.0f);
    VapourSpark(7, 16, 1, 10.0f);   // someone lights a pipe at the edge of the cloud
    currentViewZ = 1;
    AddMessage("Flash fire: petrol fumes leak by a pit (8 s); a spark at the cloud's edge at 10 s", ORANGE);
}

static void FlashFireTickStats(void) {
    int n = VapourCellCount();
    if (n > flashMostCells) flashMostCells = n;
    int pit = 0;
    for (int y = 12; y <= 20; y++) for (int x = 16; x <= 23; x++) pit += GetVapourLevel(x, y, 0);
    if (pit > flashPitMost) flashPitMost = pit;
    for (int k = 0; k < 2; k++) if (MoverKnocked(flashMover[k])) flashKnocked[k] = true;
}

static void FlashFirePrintStats(void) {
    printf("Flash fire: vapour cells %d (most %d), in the pit most %d, burned %d, burning %d, leaks %d, movers pushed %d %d\n",
           VapourCellCount(), flashMostCells, flashPitMost, VapourBurnedCells(), VapourBurningCount(), VapourLeakCount(),
           flashKnocked[0], flashKnocked[1]);
}

static bool FlashFireCheck(char* why, int whyLen) {
    if (flashPitMost < 20) { snprintf(why, (size_t)whyLen, "the fumes didn't pool in the pit (%d)", flashPitMost); return false; }
    if (VapourBurnedCells() < 40) { snprintf(why, (size_t)whyLen, "only %d cells burned", VapourBurnedCells()); return false; }
    if (VapourCellCount() > 0 || VapourBurningCount() > 0) { snprintf(why, (size_t)whyLen, "vapour left: %d cells", VapourCellCount()); return false; }
    if (!flashKnocked[0] || !flashKnocked[1]) { snprintf(why, (size_t)whyLen, "a mover in the cloud wasn't pushed"); return false; }
    int v = RunStateAudit(false);
    if (v != 0) { RunStateAudit(true); snprintf(why, (size_t)whyLen, "state audit: %d violations", v); return false; }
    return true;
}

static const Scenario scenarios[] = {
    {"ladders",    "Ground + deck joined by one ladder, 16 wandering movers: watch the queue",
                   NULL,        SetupLadders,  LadderTickStats, LadderPrintStats, LadderCheck, 0, 0},
    {"ladder-choice", "Deck with two ladders 6 cells apart, 16 movers: compare ladderQueueCost on/off",
                   NULL,        SetupLadderChoice, LadderTickStats, LadderPrintStats, LadderCheck, 0, 0},
    {"ladder-oneway", "Deck with an up-only and a down-only ladder, 16 movers: traffic goes round",
                   NULL,        SetupLadderOneWay, LadderTickStats, LadderPrintStats, LadderCheck, 0, 0},
    {"oneway-tiers", "Stepped tower: up-only ladders north, down-only south, a fire pole down; 24 movers",
                   NULL,        SetupOneWayTiers, LadderTickStats, LadderPrintStats, LadderCheck, 0, 0},
    {"door-oneway", "Building with a one-way door in and a one-way door out, 16 movers",
                   NULL,        SetupDoorOneWay, DoorTickStats, DoorPrintStats, DoorCheck, 0, 0},
    {"faces",      "16 movers wandering with fixed moods: head sprites by walking direction and mood",
                   NULL,        SetupFaces,    NULL, NULL, NULL, 0, 0},
    {"pack-hunt",  "4 hunters on 1 runner in an arena with pillars and railings: drive, cut off, flank (setting packTactics)",
                   NULL,        SetupPackHunt, NULL, PackHuntPrintStats, PackHuntCheck, 0, 0},
    {"personal-space", "16 on a lawn, 8 old friends and 8 strangers: how close they stand and pass (setting personalSpace)",
                   NULL,        SetupPersonalSpace, PersonalSpaceTickStats, PersonalSpacePrintStats, NULL, 0, 0},
    {"cover",      "Hide and seek with only low cover (railings, bushes, tables): hiders crouch behind it (setting cover)",
                   NULL,        SetupCover,    CoverTickStats, CoverPrintStats, HideAndSeekCheck, 0, 0},
    {"hunt-cover", "The pack hunt in an overgrown garden: the runner breaks sight and ducks behind cover, the pack searches (setting packStealth)",
                   NULL,        SetupHuntCover, HuntCoverTickStats, HuntCoverPrintStats, PackHuntCheck, 0, 0},
    {"play-hungry", "Tag with hunger on: players leave the game to eat, the game goes on without them (playing is a free-time state)",
                   NULL,        SetupPlayHungry, PlayHungryTickStats, PlayHungryPrintStats, PlayHungryCheck, 0, 0},
    {"playtime",   "Six friends on a lawn start games by themselves (tag, hide and seek, follow the leader), leave to eat, play again (setting autoGames)",
                   NULL,        SetupPlaytime, PlaytimeTickStats, PlaytimePrintStats, PlaytimeCheck, 0, 0},
    {"follow-leader", "Follow the leader round the pillars, 6 players in a line (setting followTrail)",
                   NULL,        SetupFollowLeader, FollowLeaderTickStats, FollowLeaderPrintStats, NULL, 0, 0},
    {"campfire",   "A fire and a burning barrel on a lawn, 12 wanderers: gatherings the world starts by itself (autoGatherings)",
                   NULL,        SetupCampfire, CampfireTickStats, CampfirePrintStats, CampfireCheck, 0, 0},
    {"mingle",     "20 guests, two circles of friends and strangers: talks form, break up, people move between groups (setting mingle)",
                   NULL,        SetupMingle,   MingleTickStats, MinglePrintStats, NULL, 0, 0},
    {"hide-and-seek", "A yard with walls, huts and pillars, 8 players: hiding out of sight and seeking (setting hideSmart)",
                   NULL,        SetupHideAndSeek, HideAndSeekTickStats, HideAndSeekPrintStats, HideAndSeekCheck, 0, 0},
    {"tag",        "Tag on a plaza with pillars, 8 players: chasing and fleeing (setting fleeSmart)",
                   NULL,        SetupTag,      TagTickStats, TagPrintStats, TagCheck, 0, 0},
    {"garden-party", "A lawn, a campfire, tables, a buffet, 14 guests who know each other: eating, talking, gathering round the fire",
                   NULL,        SetupGardenParty, GardenPartyTickStats, GardenPartyPrintStats, GardenPartyCheck, 0, 0},
    {"social-ties", "12 movers: two groups of friends, strangers, two enemy pairs: who stays near whom (socialSeek, socialAvoid)",
                   NULL,        SetupSocialTies, SocialTiesTickStats, SocialTiesPrintStats, NULL, 0, 0},
    {"gathering",  "A performer on a plaza, 30 movers: onlookers stand in rings round it (settings watch*)",
                   NULL,        SetupGathering, GatheringTickStats, GatheringPrintStats, GatheringCheck, 0, 0},
    {"doorway",    "Two rooms, one door, 16 movers wandering both: waiting at the door (setting yieldLetPass)",
                   NULL,        SetupDoorway,  DoorwayTickStats, DoorwayPrintStats, NULL, 0, 0},
    {"hallway",    "12 movers wandering a 3-wide corridor: two-way traffic (compare setting politePassing)",
                   NULL,        SetupHallway,  HallwayTickStats, HallwayPrintStats, NULL, 0, 0},
    {"social",     "10 movers wandering a plaza with two wall pieces: they notice each other (setting notice)",
                   NULL,        SetupSocial,   SocialTickStats, SocialPrintStats, SocialCheck, 0, 0},
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
    {"powder-fire", "Gunpowder and fire: a grass fire creeps to a gunpowder and a pile of 8 (twice the bang); one more past a firebreak and a granite wall stays",
                   NULL,        SetupPowderFire, NULL, PowderFirePrintStats, PowderFireCheck, 48, 32},
    {"blasting",   "Blast mining: 4 walls of a granite hill marked; 3 gunpowder ready, a hearth + quern make 2 more (saltpeter from compost, ash, water; ground with charcoal); 3 miners set, light and run",
                   NULL,        SetupBlasting, BlastingTickStats, BlastingPrintStats, BlastingCheck, 48, 32},
    {"collapse",   "Structural support: charges take the foot out of a granite pillar with an arm, a roof slab over a mover and a wooden deck with a log on it; each creaks, falls and lands as walls",
                   NULL,        SetupCollapse, CollapseTickStats, CollapsePrintStats, CollapseCheck, 48, 32},
    {"flood",      "Blasting into water: a charge opens the wall between a pond in a granite hill and a dry tunnel (the pond runs out through it); one over a field pond throws the water out over the bank",
                   NULL,        SetupFlood, FloodTickStats, FloodPrintStats, FloodCheck, 48, 32},
    {"landslide",  "Landslides: a charge near the sheer edge of a dirt hill shakes it loose (dirt slides off and lands at its foot, by a mover); the same on a granite hill holds",
                   NULL,        SetupLandslide, NULL, LandslidePrintStats, LandslideCheck, 48, 32},
    {"flashfire",  "Fuel vapour: petrol fumes leak by a pit, run into it and spread over the ground; a spark at the cloud's edge at 10 s and the flame runs through the whole cloud and into the pit (a flash fire), pushing the movers in it",
                   NULL,        SetupFlashFire, FlashFireTickStats, FlashFirePrintStats, FlashFireCheck, 48, 32},
    {"explosions", "Two lit charges: one in a field (items fly by weight, a mover knocked back, one sheltered behind granite, a fence breaks), one drilled into a granite hill (blast mining)",
                   NULL,        SetupExplosions, ExplosionsTickStats, ExplosionsPrintStats, ExplosionsCheck, 48, 32},
    {"throwing",   "Four movers throw a stone at a goal again and again: open ground, far with a wall behind, over a wall, uphill onto a block",
                   NULL,        SetupThrowing, ThrowingTickStats, ThrowingPrintStats, ThrowingCheck, 40, 32},
    {"throwscare", "Three movers throw stones at grazers: a hit dazes, then it runs from the thrower; a near miss scares",
                   NULL,        SetupThrowScare, ThrowScareTickStats, ThrowScarePrintStats, ThrowScareCheck, 40, 32},
    {"throwhunt",  "Hunting deer that outrun people: a hunter with stones dazes them from a distance, one without can only chase",
                   NULL,        SetupThrowHunt, ThrowHuntTickStats, ThrowHuntPrintStats, ThrowHuntCheck, 40, 32},
    {"drift",      "The island with a log and a bundle of sticks on 60 spots in its rivers, no settlers: where does the current take them?",
                   "Island",    SetupDrift, NULL, DriftPrintStats, DriftCheck, 256, 256},
    {"tide",       "Tide experiment: mud flats fall dry twice a day, logs on an islet reachable at low tide, a channel with a ford, 4 movers",
                   NULL,        SetupTide, TideTickStats, TidePrintStats, TideCheck, 64, 40},
    {"tide-island", "The island with the tide on its sea: flats fall dry, ramps from the beach down, 100 wandering movers; scenarioSize sets the map",
                   "Island",    SetupTideIsland, TideTickStats, TideIslandPrintStats, TideCheck, 256, 256},
    {"trash",      "City trash loop: a snack stand, three bins at 4/14/32 cells, 10 wandering movers eat and drink, bins fill, then litter (setting trashBinWalk)",
                   NULL,        SetupTrash, TrashTickStats, TrashPrintStats, TrashCheck, 40, 24},
    {"heat",       "Winter street: a heating grate and a burning barrel (heat 1), 8 chilled movers wander and warm up at the nearest warm spot",
                   NULL,        SetupHeat, HeatTickStats, HeatPrintStats, HeatCheck, 40, 24},
    {"inventory",  "Six wandering movers kitted out: pocket jacket, backpack, a plastic bag + a can, a log in both hands, all of it, nothing (click them)",
                   NULL,        SetupInventory, NULL, InventoryPrintStats, InventoryCheck, 40, 24},
    {"scrounge",   "The trash street selling burgers: diners leave leftovers in the bins, two Scavengers (no money) live off them",
                   NULL,        SetupScrounge, ScroungeTickStats, ScroungePrintStats, ScroungeCheck, 40, 24},
    {"bottlerun",  "City 1999 bottle run on orders: five bins with empty cans, a person with a backpack searches four of them (E / Shift+E queued), the fifth is yours",
                   NULL,        SetupBottleRun, NULL, BottleRunPrintStats, BottleRunCheck, 40, 24},
    {"hands-jobs", "Two haulers holding a plastic bag: a log takes both hands (the bag goes down, fetched after), berries fit beside it",
                   NULL,        SetupHandsJobs, HandsJobsTick, HandsJobsPrintStats, HandsJobsCheck, 40, 24},
    {"railings",   "Every railing tile: a pen with a gate, a window frame (T-junctions, cross), a lone post, one off a wall, a zigzag; 6 wandering movers",
                   NULL,        SetupRailings, NULL, NULL, RailingsCheck, 40, 24},
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
