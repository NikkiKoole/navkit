// games.c - The games this engine hosts (see games.h)
#include "games.h"
#include "scenarios.h"
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../world/designations.h"
#include "../simulation/plants.h"
#include "../simulation/farming.h"
#include "../simulation/floordirt.h"
#include "../simulation/groundwear.h"
#include "../simulation/lighting.h"
#include "../simulation/weather.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/balance.h"
#include "../simulation/rooms.h"
#include "../simulation/mood.h"
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../entities/containers.h"
#include "../entities/tool_quality.h"
#include "../core/sim_manager.h"
#include "../core/input_mode.h"
#include "../ui/cutscene.h"
#include <strings.h>
#include <time.h>

void SpawnMoversDemo(int count);   // main.c

// ---------------------------------------------------------------------------
// survival: one settler on 128x128 hills, fog of war, death by hunger/thirst/cold
// ---------------------------------------------------------------------------

// Randomize seed, generate 128x128 HillsSoilsWater, spawn 1 mover with supplies
static void StartSurvival(void) {
    // Randomize seed
    worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);

    // Set grid to 128x128 before generating (InitGrid inside generator preserves these)
    InitGridWithSizeAndChunkSize(128, 128, 8, 8);

    // Clear entities not covered by InitGrid()
    ClearAnimals();
    ClearFurniture();
    InitPlants();
    ClearFarming();

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

    // Fog of war: start with everything unexplored, then reveal around spawn
    CLEAR_GRID_LAYER(exploredGrid, 0);
    if (moverCount > 0) {
        Mover* m = &movers[0];
        int spawnX = (int)(m->x / CELL_SIZE);
        int spawnY = (int)(m->y / CELL_SIZE);
        int spawnZ = (int)m->z;
        RevealAroundPoint(spawnX, spawnY, spawnZ, balance.spawnVisionRadius);
    }

    // Spawn starting supplies near the mover
    if (moverCount > 0) {
        Mover* m = &movers[0];
        float sx = m->x, sy = m->y, sz = m->z;

        // 5 berries (immediate food)
        for (int i = 0; i < 5; i++) {
            SpawnItem(sx + CELL_SIZE * (i % 3), sy + CELL_SIZE, sz, ITEM_BERRIES);
        }

        // 5 sticks (basic crafting material)
        for (int i = 0; i < 5; i++) {
            SpawnItem(sx + CELL_SIZE * (i % 3), sy + CELL_SIZE * 2, sz, ITEM_STICKS);
        }

        // 1 clay pot filled with water
        int potIdx = SpawnItem(sx + CELL_SIZE * 3, sy + CELL_SIZE, sz, ITEM_CLAY_POT);
        if (potIdx >= 0) {
            int fillCount = 5; // clay pot maxContents
            for (int i = 0; i < fillCount; i++) {
                int waterIdx = SpawnItem(sx, sy, sz, ITEM_WATER);
                if (waterIdx >= 0) {
                    PutItemInContainer(waterIdx, potIdx);
                }
            }
        }
    }

    // Center camera on the mover and follow it
    if (moverCount > 0) {
        Mover* m = &movers[0];
        currentViewZ = (int)m->z;
        offset.x = GetScreenWidth() / 2.0f - m->x * zoom;
        offset.y = GetScreenHeight() / 2.0f - m->y * zoom;
        followMoverIdx = 0;
    }

    ResetTime();
    dayLength = 120.0f;  // Slower day cycle for survival
    balance.baseMoverSpeed = 400.0f;  // Faster movement for survival
    // Update already-spawned movers to new speed
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            movers[i].speed = balance.baseMoverSpeed * (1.0f + balance.moverSpeedVariance * (GetRandomValue(-100, 100) / 100.0f));
        }
    }
    gameMode = GAME_MODE_SURVIVAL;
    hungerEnabled = true;
    energyEnabled = true;
    bodyTempEnabled = true;
    thirstEnabled = true;
    bladderEnabled = false;   // set explicitly: dollhouse leaves it on
    moodEnabled = true;       // startup default
    toolRequirementsEnabled = true;
    gameOverTriggered = false;
    survivalStartTime = 0.0;
    survivalDuration = 0.0;
    PlaySurvivalIntroCutscene();
}

// ---------------------------------------------------------------------------
// dollhouse: two people in a small apartment, needs + mood, no death
// ---------------------------------------------------------------------------

// Small apartment, 2 movers, kitchen, bathroom, food
static void StartDollhouse(void) {
    worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);

    // Small grid — apartment is ~12x12, tight margin
    InitGridWithSizeAndChunkSize(16, 16, 8, 8);
    ClearAnimals();
    ClearFurniture();
    InitPlants();
    ClearFarming();

    // Flat terrain: solid ground at z=0, air + grass at z=1+
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            grid[0][y][x] = CELL_WALL;
            wallMaterial[0][y][x] = MAT_DIRT;
            SetWallNatural(x, y, 0);
            for (int z = 1; z < gridDepth; z++) {
                grid[z][y][x] = CELL_AIR;
            }
            SetVegetation(x, y, 0, VEG_GRASS_TALL);
        }
    }

    // Build apartment at z=1 (on top of ground)
    // Layout (10x8):
    //   Walls around perimeter, internal walls for rooms
    //   Room 1 (left): Kitchen/Dining — stove, counter, table area, chairs
    //   Room 2 (right-top): Bathroom — toilet
    //   Room 3 (right-bottom): Bedroom — beds
    int ox = 3, oy = 3; // offset from grid edge
    int z = 1;

    // Floor for entire apartment (10x8)
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 10; dx++) {
            SET_FLOOR(ox + dx, oy + dy, z);
            floorMaterial[z][oy + dy][ox + dx] = MAT_OAK;
        }
    }

    // Outer walls
    for (int dx = 0; dx < 10; dx++) {
        PlaceCellFull(ox + dx, oy, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, 0, true, false));
        PlaceCellFull(ox + dx, oy + 7, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, 0, true, false));
    }
    for (int dy = 1; dy < 7; dy++) {
        PlaceCellFull(ox, oy + dy, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, 0, true, false));
        PlaceCellFull(ox + 9, oy + dy, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, 0, true, false));
    }

    // Front door (bottom wall, kitchen side)
    grid[z][oy + 7][ox + 3] = CELL_DOOR;

    // Pond outside the door — dug into z=0, filled with water
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            int px = ox + 2 + dx, py = oy + 9 + dy;
            if (px >= 0 && px < gridWidth && py >= 0 && py < gridHeight) {
                grid[0][py][px] = CELL_AIR;  // dig out the ground
                SetVegetation(px, py, 0, VEG_NONE);
                SetWaterLevel(px, py, 0, 7); // fill with water at z=0
            }
        }
    }

    // Internal wall: kitchen (left 6 cols) | bathroom+bedroom (right 3 cols)
    for (int dy = 1; dy < 7; dy++) {
        if (dy == 3 || dy == 4) continue; // door gap
        PlaceCellFull(ox + 6, oy + dy, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, 0, true, false));
    }

    // Internal wall: bathroom (top-right) | bedroom (bottom-right)
    for (int dx = 7; dx < 9; dx++) {
        PlaceCellFull(ox + dx, oy + 4, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, 0, true, false));
    }
    // Door gap in bathroom/bedroom divider at x+8
    grid[z][oy + 4][ox + 8] = CELL_AIR;

    // Init all systems
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
    InitRooms();
    BuildEntrances();
    BuildGraph();

    // Place furniture — Kitchen
    SpawnFurniture(ox + 2, oy + 1, z, FURNITURE_CHAIR, MAT_OAK);   // chair at table
    SpawnFurniture(ox + 3, oy + 1, z, FURNITURE_CHAIR, MAT_OAK);   // chair at table

    // Place furniture — Bathroom
    SpawnFurniture(ox + 8, oy + 1, z, FURNITURE_TOILET, MAT_GRANITE);

    // Place furniture — Bedroom
    SpawnFurniture(ox + 7, oy + 5, z, FURNITURE_PLANK_BED, MAT_OAK);
    SpawnFurniture(ox + 8, oy + 5, z, FURNITURE_PLANK_BED, MAT_OAK);

    // Place workshops — Kitchen
    CreateWorkshop(ox + 1, oy + 3, z, WORKSHOP_STOVE);    // stove
    CreateWorkshop(ox + 3, oy + 3, z, WORKSHOP_COUNTER);  // counter (provides CAP_PREP_SURFACE)

    // Create stockpile (2x2 in kitchen for food)
    CreateStockpile(ox + 1, oy + 5, z, 2, 2);

    // Spawn 2 movers inside the apartment
    ClearMovers();
    SpawnMoverAt(ox + 3, oy + 2, z);
    SpawnMoverAt(ox + 4, oy + 2, z);

    // Spawn food supplies in stockpile area
    float sx = (ox + 1) * CELL_SIZE + CELL_SIZE * 0.5f;
    float sy = (oy + 5) * CELL_SIZE + CELL_SIZE * 0.5f;
    {
        int idx = SpawnItem(sx, sy, (float)z, ITEM_COOKED_MEAT);
        if (idx >= 0) items[idx].stackCount = 5;
    }
    {
        int idx = SpawnItem(sx + CELL_SIZE, sy, (float)z, ITEM_BREAD);
        if (idx >= 0) items[idx].stackCount = 5;
    }
    // Water pots (two so both movers can drink)
    for (int p = 0; p < 2; p++) {
        int potIdx = SpawnItem(sx + p * CELL_SIZE, sy + CELL_SIZE, (float)z, ITEM_CLAY_POT);
        if (potIdx >= 0) {
            for (int i = 0; i < 5; i++) {
                int waterIdx = SpawnItem(sx, sy, (float)z, ITEM_WATER);
                if (waterIdx >= 0) PutItemInContainer(waterIdx, potIdx);
            }
        }
    }

    // Camera: center on apartment, follow first mover
    if (moverCount > 0) {
        Mover* m = &movers[0];
        currentViewZ = z;
        offset.x = GetScreenWidth() / 2.0f - m->x * zoom;
        offset.y = GetScreenHeight() / 2.0f - m->y * zoom;
        followMoverIdx = 0;
    }

    // Settings
    ResetTime();
    dayLength = 120.0f;
    balance.baseMoverSpeed = 300.0f;
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            movers[i].speed = balance.baseMoverSpeed * (1.0f + balance.moverSpeedVariance * (GetRandomValue(-100, 100) / 100.0f));
        }
    }
    gameMode = GAME_MODE_SANDBOX;
    hungerEnabled = true;
    energyEnabled = true;
    thirstEnabled = true;
    bladderEnabled = true;
    moodEnabled = true;
    bodyTempEnabled = false; // no weather in dollhouse for now
    toolRequirementsEnabled = false;
    gameOverTriggered = false;
}

// ---------------------------------------------------------------------------
// sandbox: the flat startup map, no needs, every tool; free drawing and testing
// ---------------------------------------------------------------------------

static void StartSandbox(void) {
    ScenarioResetWorldSized(NULL, 32, 32);   // flat 32x32 dirt map, same as startup
    ResetTime();
    balance.baseMoverSpeed = 200.0f;         // balance.c default (survival/dollhouse change it)
    gameMode = GAME_MODE_SANDBOX;
    hungerEnabled = false;
    energyEnabled = false;
    thirstEnabled = false;
    bladderEnabled = false;
    bodyTempEnabled = false;
    moodEnabled = true;                      // startup default
    toolRequirementsEnabled = false;
    gameOverTriggered = false;
    currentViewZ = 1;
    followMoverIdx = -1;
    offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

static const GameDef games[] = {
    {"survival",  "Survival",  "One settler, 128x128 wilderness, fog of war: starve, dehydrate or freeze",
                  StartSurvival,  false},
    {"dollhouse", "Dollhouse", "Two people in a small apartment: needs and mood, nobody dies",
                  StartDollhouse, true},
    {"sandbox",   "Sandbox",   "Flat empty map, no needs, every tool: draw, build and test freely",
                  StartSandbox,   true},
};
#define GAME_COUNT ((int)(sizeof(games) / sizeof(games[0])))

static const GameDef* currentGame = NULL;

int GameCount(void) { return GAME_COUNT; }

const GameDef* GameAt(int index) {
    return (index >= 0 && index < GAME_COUNT) ? &games[index] : NULL;
}

const GameDef* GameFind(const char* name) {
    for (int i = 0; i < GAME_COUNT; i++)
        if (strcasecmp(games[i].name, name) == 0) return &games[i];
    return NULL;
}

const char** GameNames(void) {
    static const char* names[GAME_COUNT];
    for (int i = 0; i < GAME_COUNT; i++) names[i] = games[i].name;
    return names;
}

const GameDef* GameCurrent(void) { return currentGame; }

void GamePrintList(void) {
    printf("Games:\n");
    for (int i = 0; i < GAME_COUNT; i++)
        printf("  %-10s %s\n", games[i].name, games[i].description);
}

bool StartGame(const char* name) {
    const GameDef* g = GameFind(name);
    if (!g) {
        printf("Unknown game '%s'\n", name);
        GamePrintList();
        return false;
    }
    g->start();
    devUI = g->devUI;
    if (!devUI) InputMode_ExitToNormal();
    currentGame = g;
    TraceLog(LOG_INFO, "Game '%s': %d movers on %dx%dx%d", g->name, moverCount, gridWidth, gridHeight, gridDepth);
    return true;
}
