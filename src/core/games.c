// games.c - The games this engine hosts (see games.h)
#include "../entities/talk.h"
#include "../world/support.h"
#include "../entities/relations.h"
#include "../entities/town.h"
#include "games.h"
#include "scenarios.h"
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../world/designations.h"
#include "../world/terrain.h"
#include "../world/pathfinding.h"
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
#include "../simulation/needs.h"
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../entities/containers.h"
#include "../entities/tool_quality.h"
#include "../entities/items.h"
#include "../entities/belts.h"
#include "../simulation/tide.h"
#include "../entities/elevators.h"
#include "../world/construction.h"
#include "../core/sim_manager.h"
#include "../core/input_mode.h"
#include "../ui/cutscene.h"
#include <strings.h>
#include <stdlib.h>
#include <time.h>

void SpawnMoversDemo(int count);   // main.c

// ---------------------------------------------------------------------------
// Shared by the wilderness games (survival, island)
// ---------------------------------------------------------------------------

// After a generator built the map: every system that sizes itself from the map, then the
// HPA* graph. Water/sim counters are rebuilt AFTER generation to keep the placed water.
static void InitWorldAfterGeneration(void) {
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
}

// Where to put a starting supply: the walkable cell nearest to (cell + dx, dy), on the
// mover's level or one up/down, within 4 cells; else the mover's own cell. Fixed offsets
// used to drop supplies into a hillside, where nobody could reach them.
static void SupplySpot(float sx, float sy, float sz, int dx, int dy, float* ox, float* oy, float* oz) {
    int cx = (int)(sx / CELL_SIZE), cy = (int)(sy / CELL_SIZE), cz = (int)sz;
    int tx = cx + dx, ty = cy + dy;
    int bestD = 1 << 30;
    *ox = sx; *oy = sy; *oz = sz;
    for (int r = 0; r <= 4; r++) {
        for (int y = ty - r; y <= ty + r; y++)
            for (int x = tx - r; x <= tx + r; x++) {
                if (abs(x - tx) != r && abs(y - ty) != r) continue;   // ring only
                if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
                const int dzs[3] = {0, -1, 1};
                for (int k = 0; k < 3; k++) {
                    int z = cz + dzs[k];
                    if (z < 0 || z >= gridDepth || !IsCellWalkableAt(z, y, x)) continue;
                    int d = (x - tx) * (x - tx) + (y - ty) * (y - ty) + k;
                    if (d < bestD) {
                        bestD = d;
                        *ox = x * CELL_SIZE + CELL_SIZE * 0.5f;
                        *oy = y * CELL_SIZE + CELL_SIZE * 0.5f;
                        *oz = (float)z;
                    }
                    break;
                }
            }
        if (bestD < (1 << 30)) return;
    }
}

// 5 berries, 5 sticks and a clay pot of water around (sx, sy, sz), on walkable cells
// The survival settler starts within ~24 cells' walk of water it can reach, preferably
// below it. A settler only finds water it has seen (fog of war): seed 3 started 40-75
// cells from the river, never came across it, and died of thirst on day 3 once its pot
// was empty. The search here looks past the fog (the map is known to the game).
#define SPAWN_WATER_NEAR (24 * 10)   // walking cost: ~24 cells is near enough
#define SPAWN_WATER_TRIES 64
// Lower is better: no water = worst, water on the spawn's level costs extra
static int SpawnWaterScore(Point p) {
    int wz = 0;
    int c = WalkCostToWater(p.x, p.y, p.z, NEEDS_WATER_SEARCH_COST, &wz);
    if (c < 0) return INT_MAX;
    return (wz < p.z) ? c : c + 10000;
}
static int PickSpawnNearWater(Point cur, int best, Mover* m);
static void MoveSettlerNearWater(Mover* m) {
    GameMode savedMode = gameMode;
    gameMode = GAME_MODE_SANDBOX;            // see through the fog of war (IsExplored)
    NavRegionsInvalidate(NavGame());
    Point cur = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
    int best = SpawnWaterScore(cur);
    if (best > SPAWN_WATER_NEAR) best = PickSpawnNearWater(cur, best, m);
    gameMode = savedMode;
    NavRegionsInvalidate(NavGame());
}

// Try spots in the main area, move the settler to the best one. Returns its score.
static int PickSpawnNearWater(Point cur, int best, Mover* m) {
    Point anchor = GetMainRegionAnchor();
    Point bestP = cur;
    for (int t = 0; t < SPAWN_WATER_TRIES; t++) {
        Point p = GetRandomWalkableCellReachableFrom(anchor);
        if (p.x < 0) continue;
        int c = SpawnWaterScore(p);
        if (c < best) {
            best = c; bestP = p;
            if (c <= SPAWN_WATER_NEAR) break;
        }
    }
    if (best == INT_MAX) {
        TraceLog(LOG_WARNING, "Survival: no water the settler can walk to found near any spawn spot");
        return best;
    }
    if (bestP.x == cur.x && bestP.y == cur.y && bestP.z == cur.z) return best;
    TraceLog(LOG_INFO, "Survival: settler moved from (%d,%d,z%d) to (%d,%d,z%d), %d cells walk from water%s",
             cur.x, cur.y, cur.z, bestP.x, bestP.y, bestP.z, (best % 10000) / 10,
             best >= 10000 ? " on its own level" : " below it");
    m->x = bestP.x * CELL_SIZE + CELL_SIZE * 0.5f;
    m->y = bestP.y * CELL_SIZE + CELL_SIZE * 0.5f;
    m->z = (float)bestP.z;
    m->goal = bestP;
    m->pathLength = 0;
    m->pathIndex = -1;
    m->needsRepath = false;
    return best;
}

static void SpawnStartingSupplies(float sx, float sy, float sz) {
    float x, y, z;
    // 5 berries (immediate food)
    for (int i = 0; i < 5; i++) {
        SupplySpot(sx, sy, sz, i % 3, 1, &x, &y, &z);
        SpawnItem(x, y, z, ITEM_BERRIES);
    }

    // 5 sticks (basic crafting material)
    for (int i = 0; i < 5; i++) {
        SupplySpot(sx, sy, sz, i % 3, 2, &x, &y, &z);
        SpawnItem(x, y, z, ITEM_STICKS);
    }

    // 1 clay pot filled with water
    SupplySpot(sx, sy, sz, 3, 1, &x, &y, &z);
    int potIdx = SpawnItem(x, y, z, ITEM_CLAY_POT);
    if (potIdx >= 0) {
        int fillCount = 5; // clay pot maxContents
        for (int i = 0; i < fillCount; i++) {
            int waterIdx = SpawnItem(x, y, z, ITEM_WATER);
            if (waterIdx >= 0) {
                PutItemInContainer(waterIdx, potIdx);
            }
        }
    }
}

// Survival rules and pace: time from zero, a 6-minute day, needs and death on (fog of war
// is set up by each game around its settlers)
static void SetSurvivalRules(void) {
    ResetTime();
    // A 6-minute day, walking 3x the old 400 px/s: same speed on screen as a 2-minute day
    // at 400, but per game hour movers cover 3x the ground (--day-budget: walking halves)
    dayLength = 360.0f;
    balance.baseMoverSpeed = 1200.0f;
    // Update already-spawned movers to new speed
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            movers[i].speed = balance.baseMoverSpeed * (1.0f + balance.moverSpeedVariance * (GetRandomValue(-100, 100) / 100.0f));
        }
    }
    gameMode = GAME_MODE_SURVIVAL;
    talkEnabled = false;      // idle movers stopping to chat: dollhouse only for now
    autoGatherings = true;    // ... but settlers do gather round a burning fire
    hungerEnabled = true;
    energyEnabled = true;
    bodyTempEnabled = true;
    thirstEnabled = true;
    bladderEnabled = true;    // bush, pit latrine, toilet (docs/todo/gameplay/survival-toilet.md)
    toiletsPlumbed = false;   // no mains: a toilet flushes only with water close by
    moodEnabled = true;       // startup default
    toolRequirementsEnabled = true;
    jobDistanceWeight = 0.2f;   // movers prefer work near them (one priority level = 5 cells)
    gameOverTriggered = false;
    survivalStartTime = 0.0;
    survivalDuration = 0.0;
}

// ---------------------------------------------------------------------------
// survival: one settler on 128x128 hills, fog of war, death by hunger/thirst/cold
// ---------------------------------------------------------------------------

// Randomize seed, generate 128x128 HillsSoilsWater, spawn 1 mover with supplies
static void StartSurvival(void) {
    supportEnabled = true;   // structural support (support.c); set before the world is generated: it settles it
    // Randomize seed
    if (!worldSeedFixed) worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);

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

    InitWorldAfterGeneration();

    // Spawn 1 mover at a walkable cell
    SpawnMoversDemo(1);
    if (moverCount > 0) MoveSettlerNearWater(&movers[0]);

    // Fog of war: start with everything unexplored, then reveal around spawn
    CLEAR_GRID_LAYER(unexploredGrid, 1);   // all unexplored
    if (moverCount > 0) {
        Mover* m = &movers[0];
        int spawnX = (int)(m->x / CELL_SIZE);
        int spawnY = (int)(m->y / CELL_SIZE);
        int spawnZ = (int)m->z;
        RevealAroundPoint(spawnX, spawnY, spawnZ, balance.spawnVisionRadius);
    }

    // Spawn starting supplies near the mover
    if (moverCount > 0) SpawnStartingSupplies(movers[0].x, movers[0].y, movers[0].z);

    // Center camera on the mover and follow it, zoomed out to see the surroundings
    zoom = 0.5f;
    if (moverCount > 0) {
        Mover* m = &movers[0];
        currentViewZ = (int)m->z;
        offset.x = GetScreenWidth() / 2.0f - m->x * zoom;
        offset.y = GetScreenHeight() / 2.0f - m->y * zoom;
        followMoverIdx = 0;
    }

    SetSurvivalRules();
    PlaySurvivalIntroCutscene();
}

// ---------------------------------------------------------------------------
// island: a few settlers wash ashore on a finite island (src/world/island_gen.c)
// ---------------------------------------------------------------------------

#define ISLAND_SIZE     256
#define ISLAND_SETTLERS 3

static int TerrainIndexByName(const char* name) {
    for (int i = 0; i < terrainCount; i++) if (strcasecmp(terrainNames[i], name) == 0) return i;
    return 0;
}

// The walkable cell nearest (want) that movers can get from to the rest of the island,
// searched in growing squares; any reachable cell if there is none within 48 cells
static Point IslandStartCell(Point want, Point anchor) {
    if (want.x >= 0) {
        for (int r = 0; r <= 48; r++)
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    if (abs(dx) != r && abs(dy) != r) continue;   // ring only
                    int x = want.x + dx, y = want.y + dy;
                    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
                    for (int z = want.z - 1; z <= want.z + 2; z++) {
                        if (z < 1 || z >= gridDepth || !IsCellWalkableAt(z, y, x)) continue;
                        Point p = {x, y, z};
                        if (anchor.x < 0 || IsReachable(anchor, p)) return p;
                    }
                }
    }
    return GetRandomWalkableCellReachableFrom(anchor);
}

static void StartIsland(void) {
    supportEnabled = true;   // structural support (support.c); set before the world is generated: it settles it
    if (!worldSeedFixed) worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);
    InitGridWithSizeAndChunkSize(ISLAND_SIZE, ISLAND_SIZE, 16, 16);
    ClearAnimals();
    ClearFurniture();
    InitPlants();
    ClearFarming();
    currentTerrain = TerrainIndexByName("Island");
    GenerateCurrentTerrain();
    InitWorldAfterGeneration();

    // Ashore on the beach nearest fresh water (a river mouth), side by side
    Point want = {-1, -1, -1};
    int lx, ly, lz;
    if (IslandLandingSpot(&lx, &ly, &lz)) want = (Point){lx, ly, lz};
    Point anchor = GetMainRegionAnchor();
    Point start = IslandStartCell(want, anchor);
    CLEAR_GRID_LAYER(unexploredGrid, 1);   // fog of war: all unexplored
    for (int i = 0; i < ISLAND_SETTLERS && start.x >= 0; i++) {
        Point p = IslandStartCell((Point){start.x + (i % 2 ? i : -i), start.y, start.z}, anchor);
        if (p.x < 0) p = start;
        SpawnMoverAt(p.x, p.y, p.z);
        RevealAroundPoint(p.x, p.y, p.z, balance.spawnVisionRadius);
    }
    if (moverCount > 0) SpawnStartingSupplies(movers[0].x, movers[0].y, movers[0].z);

    // The tide (setting islandTide): only now, so the settlers and their supplies were placed
    // with the sea full (never on flats that flood later). Ramps from the beach down to the
    // flats, the sea set to the current tide, the nav graph again.
    tideEnabled = false;
    if (islandTide && TideMarkSea(waterEdgeSeaZ) > 0) {
        TidePrepareShore();
        tideEnabled = true;
        tideLowLevel = 0;
        TideSettle();
        BuildEntrances();
        BuildGraph();
    }

    zoom = 0.5f;
    if (moverCount > 0) {
        Mover* m = &movers[0];
        currentViewZ = (int)m->z;
        offset.x = GetScreenWidth() / 2.0f - m->x * zoom;
        offset.y = GetScreenHeight() / 2.0f - m->y * zoom;
        followMoverIdx = 0;
    }
    SetSurvivalRules();
}

// ---------------------------------------------------------------------------
// dollhouse: two people in a small apartment, needs + mood, no death
// ---------------------------------------------------------------------------

// Small apartment, 2 movers, kitchen, bathroom, food
static void StartDollhouse(void) {
    supportEnabled = true;   // structural support (support.c); set before the world is generated: it settles it
    if (!worldSeedFixed) worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);

    // A city flat (concrete) on a small plot: 14x10 under a roof, a railed balcony, a pond
    InitGridWithSizeAndChunkSize(20, 18, 10, 9);
    ClearAnimals();
    ClearFurniture();
    InitPlants();
    ClearFarming();

    // Flat terrain: solid ground at z=0, air + grass at z=1+
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            GRID(x, y, 0) = CELL_WALL;
            WALL_MAT(x, y, 0) = MAT_DIRT;
            SetWallNatural(x, y, 0);
            for (int z = 1; z < gridDepth; z++) {
                GRID(x, y, z) = CELL_AIR;
            }
            SetVegetation(x, y, 0, VEG_GRASS_TALL);
        }
    }

    // Layout at z=1, local (dx, dy), 14x10. # wall, D door, w window, = railing:
    //
    //      0 1 2 3 4 5 6 7 8 9 0 1 2 3
    //   0  # # w # w # # # # # # w # #
    //   1  # h T h . . #   # S . . t #     kitchen: table + chairs, stove, counter, sink, tap,
    //   2  # . . . . . D   D . . . . #              food stockpile with the fridge in it
    //   3  # s c k p . #   # . . . W #     hall (1 wide) between everything, front door below
    //   4  # . F F F f #   # # # # # #     bathroom: sink, toilet, shower (W)
    //   5  # # # # # # #   # . . . d #     bedroom: double bed, desk + chair, bookshelf
    //   6  # . o o . . #   D . . . h #     living room: sofa (2 seats), bookshelf
    //   7  w . . . . . D   # . . B . w
    //   8  # l . . . . #   # l . . . #
    //   9  # # # D # # # D # # # # # #
    //  10  = . . . . . =                   balcony: no roof, railing round it (outdoors)
    //  11  = . . . . . =     pond
    //  12  = = = = = = =
    int ox = 3, oy = 2; // offset from grid edge
    int z = 1;

    // Floor, and a roof over the flat (the balcony has none: it reads as outdoors)
    for (int dy = 0; dy < 10; dy++) {
        for (int dx = 0; dx < 14; dx++) {
            SET_FLOOR(ox + dx, oy + dy, z);
            FLOOR_MAT(ox + dx, oy + dy, z) = MAT_OAK;
            SET_FLOOR(ox + dx, oy + dy, z + 1);
            FLOOR_MAT(ox + dx, oy + dy, z + 1) = MAT_CONCRETE;
        }
    }

    #define DH_WALL(dx, dy) PlaceCellFull(ox + (dx), oy + (dy), z, NaturalTerrainSpec(CELL_WALL, MAT_CONCRETE, 0, true, false))
    #define DH_DOOR(dx, dy) (GRID(ox + (dx), oy + (dy), z) = CELL_DOOR)
    #define DH_WINDOW(dx, dy) do { GRID(ox + (dx), oy + (dy), z) = CELL_WINDOW; SetWallMaterial(ox + (dx), oy + (dy), z, MAT_GLASS); ClearWallNatural(ox + (dx), oy + (dy), z); } while (0)
    for (int dx = 0; dx < 14; dx++) { DH_WALL(dx, 0); DH_WALL(dx, 9); }
    for (int dy = 1; dy < 9; dy++)  { DH_WALL(0, dy); DH_WALL(13, dy); }
    for (int dy = 1; dy < 9; dy++)  { DH_WALL(6, dy); DH_WALL(8, dy); }   // hall walls
    for (int dx = 1; dx < 6; dx++)  DH_WALL(dx, 5);                         // kitchen | living
    for (int dx = 9; dx < 13; dx++) DH_WALL(dx, 4);                         // bathroom | bedroom
    DH_DOOR(7, 9);                  // front door
    DH_DOOR(3, 9);                  // living room -> balcony
    DH_DOOR(6, 2); DH_DOOR(6, 7);   // hall -> kitchen, living room
    DH_DOOR(8, 2); DH_DOOR(8, 6);   // hall -> bathroom, bedroom
    DH_WINDOW(2, 0); DH_WINDOW(4, 0); DH_WINDOW(11, 0); DH_WINDOW(0, 7); DH_WINDOW(13, 7);
    #undef DH_WALL
    #undef DH_DOOR
    #undef DH_WINDOW

    // Balcony: concrete floor, a railing round it (stops walking, not light or the view)
    for (int dy = 10; dy <= 12; dy++) {
        for (int dx = 0; dx <= 6; dx++) {
            int x = ox + dx, y = oy + dy;
            bool edge = dx == 0 || dx == 6 || dy == 12;
            SET_FLOOR(x, y, z);
            FLOOR_MAT(x, y, z) = MAT_CONCRETE;
            ClearFloorNatural(x, y, z);
            SetVegetation(x, y, 0, VEG_NONE);   // a concrete slab, no grass
            if (edge) { GRID(x, y, z) = CELL_RAILING; SetWallMaterial(x, y, z, MAT_IRON); ClearWallNatural(x, y, z); }
        }
    }

    // Pond outside the front door — dug into z=0, filled with water
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            int px = ox + 9 + dx, py = oy + 11 + dy;
            if (px >= 0 && px < gridWidth && py >= 0 && py < gridHeight) {
                GRID(px, py, 0) = CELL_AIR;  // dig out the ground
                SetVegetation(px, py, 0, VEG_NONE);
                SetWaterLevel(px, py, 0, 7); // fill with water at z=0
            }
        }
    }

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

    #define DH_FURN(dx, dy, type, mat) SpawnFurniture(ox + (dx), oy + (dy), z, (type), (mat))
    // Kitchen
    DH_FURN(1, 1, FURNITURE_CHAIR, MAT_OAK);    // chair, table, chair
    DH_FURN(2, 1, FURNITURE_TABLE, MAT_OAK);
    DH_FURN(3, 1, FURNITURE_CHAIR, MAT_OAK);
    DH_FURN(3, 3, FURNITURE_SINK, MAT_GRANITE);
    DH_FURN(4, 3, FURNITURE_TAP, MAT_GRANITE);  // clean water that doesn't run out
    DH_FURN(5, 4, FURNITURE_FRIDGE, MAT_IRON);  // food kept on it spoils slower
    CreateWorkshop(ox + 1, oy + 3, z, WORKSHOP_STOVE);
    CreateWorkshop(ox + 2, oy + 3, z, WORKSHOP_COUNTER);  // provides CAP_PREP_SURFACE
    // Bathroom
    DH_FURN(9, 1, FURNITURE_SINK, MAT_GRANITE);
    DH_FURN(12, 1, FURNITURE_TOILET, MAT_GRANITE);
    DH_FURN(12, 3, FURNITURE_SHOWER, MAT_GRANITE);
    // Bedroom
    DH_FURN(11, 7, FURNITURE_DOUBLE_BED, MAT_OAK);   // both sleep in it
    DH_FURN(12, 5, FURNITURE_DESK, MAT_OAK);
    DH_FURN(12, 6, FURNITURE_CHAIR, MAT_OAK);
    DH_FURN(9, 8, FURNITURE_SHELF, MAT_OAK);         // bookshelf
    // Living room
    DH_FURN(2, 6, FURNITURE_SOFA, MAT_OAK);
    DH_FURN(3, 6, FURNITURE_SOFA, MAT_OAK);
    DH_FURN(1, 8, FURNITURE_SHELF, MAT_OAK);
    #undef DH_FURN

    // Food stockpile along the kitchen's south wall, the fridge at its end
    CreateStockpile(ox + 2, oy + 4, z, 4, 1);

    // Spawn 2 movers in the kitchen
    ClearMovers();
    SpawnMoverAt(ox + 3, oy + 2, z);
    SpawnMoverAt(ox + 4, oy + 2, z);
    RelationSetPair(moverCount - 2, moverCount - 1, 60.0f, 100.0f);   // they live together: partners

    // Food supplies: meat on the fridge, bread and water pots next to it
    float sx = (ox + 2) * CELL_SIZE + CELL_SIZE * 0.5f;
    float sy = (oy + 4) * CELL_SIZE + CELL_SIZE * 0.5f;
    {
        int idx = SpawnItem(sx + 3 * CELL_SIZE, sy, (float)z, ITEM_COOKED_MEAT);   // on the fridge
        if (idx >= 0) items[idx].stackCount = 5;
    }
    {
        int idx = SpawnItem(sx + 2 * CELL_SIZE, sy, (float)z, ITEM_BREAD);
        if (idx >= 0) items[idx].stackCount = 5;
    }
    // Water pots (two so both movers can drink)
    for (int p = 0; p < 2; p++) {
        int potIdx = SpawnItem(sx + p * CELL_SIZE, sy, (float)z, ITEM_CLAY_POT);
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
    dayLength = 360.0f;               // 6-minute day, walking 3x: see survival
    balance.baseMoverSpeed = 900.0f;
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].active) {
            movers[i].speed = balance.baseMoverSpeed * (1.0f + balance.moverSpeedVariance * (GetRandomValue(-100, 100) / 100.0f));
        }
    }
    gameMode = GAME_MODE_SANDBOX;
    talkEnabled = true;       // the residents stop to chat (talk.c)
    autoGatherings = true;    // and gather round the cooking, a fire, a heater
    hungerEnabled = true;
    energyEnabled = true;
    thirstEnabled = true;
    bladderEnabled = true;
    toiletsPlumbed = true;    // mains water: every toilet flushes
    moodEnabled = true;
    bodyTempEnabled = false; // no weather in dollhouse for now
    toolRequirementsEnabled = false;
    jobDistanceWeight = 0.2f;   // movers prefer work near them (one priority level = 5 cells)
    gameOverTriggered = false;
}

// ---------------------------------------------------------------------------
// town: draw roads and lots, buildings appear at once (docs/vision/town.md)
// ---------------------------------------------------------------------------

#define TOWN_SIZE 128

static void StartTown(void) {
    ScenarioResetWorldSized(NULL, TOWN_SIZE, TOWN_SIZE);   // flat grass, walking level z=1
    ResetTime();
    // A main street in from the west edge: where newcomers will arrive
    int midY = TOWN_SIZE / 2;
    TownAddRoad(0, midY - 1, TOWN_SIZE / 2 + 8, midY + 1, 1, NULL);
    // Starter homes (setting townStarter): 7x6 lots along the street, alternating sides
    for (int i = 0; i < townStarter; i++) {
        int x = 6 + (i / 2) * 9;
        if (i % 2 == 0) TownAddLot(LOT_HOME, x, midY - 7, x + 6, midY - 2, 1, NULL);
        else            TownAddLot(LOT_HOME, x, midY + 2, x + 6, midY + 7, 1, NULL);
    }
    if (townStarter > 0) TownAddLot(LOT_WORK, 59, midY - 8, 70, midY - 2, 1, NULL);   // and a bakery
    BuildEntrances();
    BuildGraph();

    dayLength = 360.0f;               // 6-minute day, walking 3x: as the dollhouse
    balance.baseMoverSpeed = 900.0f;
    gameMode = GAME_MODE_SANDBOX;     // nobody dies
    talkEnabled = true;
    autoGatherings = true;
    hungerEnabled = true;
    energyEnabled = true;
    thirstEnabled = true;
    bladderEnabled = true;
    toiletsPlumbed = true;            // mains water
    moodEnabled = true;
    bodyTempEnabled = false;
    toolRequirementsEnabled = false;
    jobDistanceWeight = 0.2f;
    gameOverTriggered = false;
    currentViewZ = 1;
    followMoverIdx = -1;
    zoom = 1.0f;
    offset.x = GetScreenWidth() / 2.0f - (TOWN_SIZE / 2) * CELL_SIZE * zoom;
    offset.y = GetScreenHeight() / 2.0f - midY * CELL_SIZE * zoom;
}

// ---------------------------------------------------------------------------
// sandbox: the flat startup map, no needs, every tool; free drawing and testing
// ---------------------------------------------------------------------------

static void StartSandbox(void) {
    supportEnabled = true;   // structural support (support.c); set before the world is generated: it settles it
    ScenarioResetWorldSized(NULL, 32, 32);   // flat 32x32 dirt map, same as startup
    ResetTime();
    balance.baseMoverSpeed = 200.0f;         // balance.c default (survival/dollhouse change it)
    gameMode = GAME_MODE_SANDBOX;
    talkEnabled = false;
    autoGatherings = false;
    hungerEnabled = false;
    energyEnabled = false;
    thirstEnabled = false;
    bladderEnabled = false;
    toiletsPlumbed = true;    // mains water: every toilet flushes
    bodyTempEnabled = false;
    moodEnabled = true;                      // startup default
    toolRequirementsEnabled = false;
    jobDistanceWeight = 0.0f;                // strict job priority order
    gameOverTriggered = false;
    currentViewZ = 1;
    followMoverIdx = -1;
    offset.x = (GetScreenWidth() - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (GetScreenHeight() - gridHeight * CELL_SIZE * zoom) / 2.0f;
}

// ---------------------------------------------------------------------------
// Play HUD toolsets (dev UI off). A tool with an `available` rule only shows
// once the world has what it needs (progressive unlocks).
// ---------------------------------------------------------------------------

GameToolContext GameToolContextNow(void) {
    GameToolContext c = {itemCount > 0, false, false, false, false, false, false, false};
    for (int i = 0; i < itemHighWaterMark && (!c.hasGrass || !c.hasLeaves || !c.hasRocks || !c.hasPlanks ||
                                              !c.hasSticks || !c.hasClay || !c.hasGunpowder); i++) {
        if (!items[i].active) continue;
        if (items[i].type == ITEM_GRASS) c.hasGrass = true;
        if (items[i].type == ITEM_LEAVES) c.hasLeaves = true;
        if (items[i].type == ITEM_ROCK) c.hasRocks = true;
        if (items[i].type == ITEM_PLANKS) c.hasPlanks = true;
        if (items[i].type == ITEM_STICKS) c.hasSticks = true;
        if (items[i].type == ITEM_CLAY) c.hasClay = true;
        if (items[i].type == ITEM_GUNPOWDER) c.hasGunpowder = true;
    }
    return c;
}

static bool HasItems(const GameToolContext* c)  { return c->hasItems; }
static bool HasRocks(const GameToolContext* c)  { return c->hasRocks; }
static bool HasLeaves(const GameToolContext* c) { return c->hasLeaves; }
static bool HasGrass(const GameToolContext* c)  { return c->hasGrass; }
static bool HasPlanks(const GameToolContext* c) { return c->hasPlanks; }
static bool HasSticks(const GameToolContext* c) { return c->hasSticks; }
static bool HasPlanksOrClay(const GameToolContext* c) { return c->hasPlanks || c->hasClay; }
static bool HasGunpowder(const GameToolContext* c) { return c->hasGunpowder; }

// The early-game designations every game offers in its play HUD
#define BASIC_TOOLS \
    {HUD_TAB_NATURE, "Harvest Berries",  ACTION_WORK_HARVEST_BERRY,     -1, NULL}, \
    {HUD_TAB_NATURE, "Gather Tree",      ACTION_WORK_GATHER_TREE,       -1, NULL}, \
    {HUD_TAB_NATURE, "Gather Grass",     ACTION_WORK_GATHER_GRASS,      -1, NULL}, \
    {HUD_TAB_NATURE, "Knap Stone",       ACTION_WORK_KNAP,              -1, HasRocks}, \
    {HUD_TAB_BUILD,  "Place Stockpile",  ACTION_DRAW_STOCKPILE,         -1, HasItems}, \
    {HUD_TAB_BUILD,  "Gather Zone",      ACTION_WORK_GATHER,            -1, HasItems}, \
    {HUD_TAB_BUILD,  "Build Campfire",   ACTION_WORK_WORKSHOP_CAMPFIRE, -1, HasSticks}, \
    {HUD_TAB_BUILD,  "Build Leaf Pile",  ACTION_WORK_FURNITURE, CONSTRUCTION_LEAF_PILE,  HasLeaves}, \
    {HUD_TAB_BUILD,  "Build Grass Pile", ACTION_WORK_FURNITURE, CONSTRUCTION_GRASS_PILE, HasGrass}, \
    {HUD_TAB_BUILD,  "Build Pit Latrine", ACTION_WORK_FURNITURE, CONSTRUCTION_PIT_LATRINE, HasSticks}, \
    {HUD_TAB_BUILD,  "Build Toilet",     ACTION_WORK_FURNITURE, CONSTRUCTION_TOILET,      HasPlanksOrClay}, \
    {HUD_TAB_BUILD,  "Build Belt",       ACTION_WORK_BELT,              -1, HasPlanks}, \
    {HUD_TAB_BUILD,  "Build Elevator",   ACTION_WORK_SHAFT,             -1, HasPlanks}

static const GameTool survivalTools[] = {
    {HUD_TAB_NATURE, "Explore", ACTION_WORK_EXPLORE, -1, NULL},   // fog of war
    {HUD_TAB_NATURE, "Blast Rock", ACTION_WORK_BLAST, -1, HasGunpowder},   // once there's gunpowder
    BASIC_TOOLS,
};
static const GameTool dollhouseTools[] = { BASIC_TOOLS };
static const GameTool sandboxTools[]   = { BASIC_TOOLS };
static const GameTool townTools[] = {
    {HUD_TAB_BUILD, "Road", ACTION_DRAW_ROAD,     -1, NULL},
    {HUD_TAB_BUILD, "Home", ACTION_DRAW_LOT_HOME, -1, NULL},
    {HUD_TAB_BUILD, "Bakery", ACTION_DRAW_LOT_WORK, -1, NULL},
};
#undef BASIC_TOOLS
#define TOOLS(list) list, (int)(sizeof(list) / sizeof(list[0]))

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

static const GameDef games[] = {
    {"survival",  "Survival",  "One settler, 128x128 wilderness, fog of war: starve, dehydrate or freeze",
                  StartSurvival,  false, false, TOOLS(survivalTools)},
    {"dollhouse", "Dollhouse", "Two people in a small apartment: needs and mood, nobody dies",
                  StartDollhouse, true,  true,  TOOLS(dollhouseTools)},
    {"island",    "Island",    "Three settlers wash ashore on an island grown from the seed: rivers, forest, moor; survival rules",
                  StartIsland,    false, false, TOOLS(survivalTools)},
    {"sandbox",   "Sandbox",   "Flat empty map, no needs, every tool: draw, build and test freely",
                  StartSandbox,   true,  true,  TOOLS(sandboxTools)},
    {"town",      "Town",      "Draw roads, homes and bakeries: they appear at once, people move in and bake their bread",
                  StartTown,      false, true,  TOOLS(townTools)},
};
#undef TOOLS
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

const GameDef* GameActive(void) {
    if (currentGame) return currentGame;
    return GameFind(gameMode == GAME_MODE_SURVIVAL ? "survival" : "sandbox");
}

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
    ClearBelts();   // games that don't go through ScenarioResetWorld (survival, island, dollhouse)
    ClearTide();
    ClearElevators();
    g->start();
    devUI = g->devUI;
    buildNow = g->buildNow;
    if (!devUI) InputMode_ExitToNormal();
    currentGame = g;
    TraceLog(LOG_INFO, "Game '%s': %d movers on %dx%dx%d", g->name, moverCount, gridWidth, gridHeight, gridDepth);
    return true;
}
