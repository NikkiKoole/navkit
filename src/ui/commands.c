// commands.c - the command table and what each command does (see commands.h)
// Moved here from console.c's Cmd_* functions (behaviour kept) and from the dev sidebar's
// hand-written buttons (ui_panels.c), which now run these rows.
#include "commands.h"
#include "find.h"
#include "console.h"
#include "../game_state.h"
#include "../../shared/ui.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/animals.h"
#include "../entities/stockpiles.h"
#include "../entities/throwing.h"
#include "../world/cell_defs.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/groundwear.h"
#include "../simulation/lighting.h"
#include "../simulation/weather.h"
#include "../simulation/floordirt.h"
#include "../simulation/explosion.h"
#include "../world/support.h"
#include "../core/time.h"
#include "../core/sim_manager.h"
#include "../world/designations.h"
#include "../core/scenarios.h"
#include "../core/games.h"
#include "../core/input_mode.h"
#include "../core/selection.h"
#include "../core/state_audit.h"
#include "../core/event_log.h"
#include "../simulation/plants.h"
#include "../simulation/farming.h"
#include "../simulation/balance.h"
#include "../world/pathfinding.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "../entities/town.h"
#include <stdarg.h>
#include <string.h>
#include <strings.h>

void SpawnMoversDemo(int count);   // main.c
void AddMoversDemo(int count);     // main.c
void SpawnAgents(int count);       // main.c
void RepathAgents(void);           // main.c
void SpawnStockpileWithFilters(bool allowRed, bool allowGreen, bool allowBlue);   // main.c
void TriggerScreenShake(float intensity, float duration);                         // main.c
void PlayTestCutscene(void);       // ui/cutscene.c
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// Feedback: always in the console log; on screen too when the console is closed (a sidebar
// button pressed, a command run from elsewhere)
__attribute__((format(printf, 2, 3)))
static void Say(Color color, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Console_Print(buf, color);
    if (!Console_IsOpen()) AddMessage(buf, color);
}

// ---- name lists from other tables ----
static int TerrainNameCount(void) { return terrainCount; }
static const char* TerrainNameAt(int i) { return terrainNames[i]; }
static int ScenarioNameCount(void) { return ScenarioCount(); }
static const char* ScenarioNameAt(int i) { return ScenarioAt(i)->name; }
static int GameNameCount(void) { return GameCount(); }
static const char* GameNameAt(int i) { return GameAt(i)->name; }

// ---- here: the place commands act on ----
static struct { bool set; int x, y, z; } here;
void CommandSetHere(int x, int y, int z) { here.set = true; here.x = x; here.y = y; here.z = z; }
void CommandClearHere(void) { here.set = false; }
bool CommandHere(int* x, int* y, int* z) {
    if (here.set) { *x = here.x; *y = here.y; *z = here.z; }
    else {
        Vector2 g = ScreenToGrid(GetMousePosition());
        *x = (int)g.x; *y = (int)g.y; *z = currentViewZ;
    }
    return *x >= 0 && *x < gridWidth && *y >= 0 && *y < gridHeight && *z >= 0 && *z < gridDepth;
}

// ---- spawn <count> <item> ----
static void RunSpawn(const CmdValue* v) {
    int count = v[0].i;
    ItemType type = (ItemType)v[1].i;
    int mx, my, mz;
    if (!CommandHere(&mx, &my, &mz)) {
        Say(YELLOW, "Mouse position out of bounds");
        return;
    }
    int spawned = 0;
    uint8_t mat = DefaultMaterialForItemType(type);
    for (int i = 0; i < count; i++) {
        // The nearest walkable cell within 2 of the mouse, this level or one down
        int bestX = mx, bestY = my, bestZ = mz;
        float bestDist = 9999.0f;
        for (int dz = 0; dz >= -1 && dz + mz >= 0; dz--) {
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int tx = mx + dx, ty = my + dy, tz = mz + dz;
                    if (tx < 0 || tx >= gridWidth || ty < 0 || ty >= gridHeight || tz < 0 || tz >= gridDepth) continue;
                    if (!IsCellWalkableAt(tz, ty, tx)) continue;
                    float dist = (float)(dx * dx + dy * dy + dz * dz);
                    if (dist < bestDist) { bestDist = dist; bestX = tx; bestY = ty; bestZ = tz; }
                }
            if (bestDist < 9999.0f) break;
        }
        if (bestDist < 9999.0f) {
            SpawnItemWithMaterial(bestX * CELL_SIZE + CELL_SIZE * 0.5f, bestY * CELL_SIZE + CELL_SIZE * 0.5f,
                                  (float)bestZ, type, mat);
            spawned++;
        }
    }
    Say(GREEN, "Spawned %d %s at %d,%d,%d", spawned, itemDefs[type].name, mx, my, mz);
}

// ---- clear <what> ----
static const char* const clearChoices[] = { "items", "movers", "fire", "smoke", "steam", "water", "temp",
                                            "wear", "lights", "animals", "stockpiles", "all" };
static void RunClear(const CmdValue* v) {
    switch (v[0].choice) {
        case 0: ClearItems(); Say(GREEN, "Cleared all items"); break;
        case 1: ClearMovers(); Say(GREEN, "Cleared all movers"); break;
        case 2: ClearFire(); Say(GREEN, "Cleared fire"); break;
        case 3: ClearSmoke(); Say(GREEN, "Cleared smoke"); break;
        case 4: ClearSteam(); Say(GREEN, "Cleared steam"); break;
        case 5: ClearWater(); Say(GREEN, "Cleared water"); break;
        case 6: ClearTemperature(); Say(GREEN, "Reset temperature to ambient"); break;
        case 7: ClearGroundWear(); Say(GREEN, "Cleared ground wear"); break;
        case 8: ClearLightSources(); InvalidateLighting(); Say(GREEN, "Cleared light sources"); break;
        case 9: ClearAnimals(); Say(GREEN, "Cleared animals"); break;
        case 10: ClearStockpiles(); Say(GREEN, "Cleared stockpiles"); break;
        case 11:
            ClearWater(); ClearFire(); ClearSmoke(); ClearSteam(); ClearTemperature(); ClearGroundWear();
            ClearLightSources(); InvalidateLighting();
            Say(GREEN, "Cleared all simulation state");
            break;
    }
}

// ---- tp <x> <y> [z] ----
static void RunTp(const CmdValue* v) {
    int x = v[0].i, y = v[1].i, z = v[2].given ? v[2].i : currentViewZ;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        Say(YELLOW, "Out of bounds: %d,%d,z%d (grid is %dx%dx%d)", x, y, z, gridWidth, gridHeight, gridDepth);
        return;
    }
    offset.x = GetScreenWidth() * 0.5f - (x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
    offset.y = GetScreenHeight() * 0.5f - (y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
    currentViewZ = z;
    Say(GREEN, "Teleported to %d, %d, z%d", x, y, z);
}

// ---- reveal ----
static void RunReveal(const CmdValue* v) {
    (void)v;
    CLEAR_GRID_LAYER(unexploredGrid, 0);
    BuildEntrances();
    BuildGraph();
    Say(GREEN, "Fog of war lifted: everything explored");
}

// ---- town road|home|work x1 y1 x2 y2 | town remove x1 y1 x2 y2 | town list ----
// Town roads and lots (town.c) at the viewed level; the play tools do the same by dragging
static void RunTown(const CmdValue* v) {
    char what[16] = "";
    int x1, y1, x2, y2;
    int n = v[0].s ? sscanf(v[0].s, "%15s %d %d %d %d", what, &x1, &y1, &x2, &y2) : 0;
    int z = currentViewZ;
    if (n >= 1 && strcasecmp(what, "list") == 0) {
        Say(WHITE, "%d roads, %d homes, %d workplaces", townRoadCount, TownLotCount(LOT_HOME), TownLotCount(LOT_WORK));
        for (int i = 0; i < townLotHighWater; i++) {
            const TownLot* L = &townLots[i];
            if (!L->active) continue;
            Say(WHITE, "lot %d: %s %d,%d-%d,%d door %d,%d %s %d", i, TownLotKindName(L->kind), L->x1, L->y1, L->x2, L->y2,
                L->doorX, L->doorY, L->kind == LOT_HOME ? "beds" : "work spots", L->beds);
        }
        return;
    }
    if (n < 5) { Say(RED, "usage: town road|home|work|remove x1 y1 x2 y2, or town list"); return; }
    const char* why = NULL;
    if (strcasecmp(what, "road") == 0) {
        if (TownAddRoad(x1, y1, x2, y2, z, &why) < 0) Say(RED, "No road: %s", why);
        else Say(GREEN, "Road %d,%d-%d,%d", x1, y1, x2, y2);
    } else if (strcasecmp(what, "home") == 0 || strcasecmp(what, "work") == 0) {
        LotKind k = strcasecmp(what, "home") == 0 ? LOT_HOME : LOT_WORK;
        int lot = TownAddLot(k, x1, y1, x2, y2, z, &why);
        if (lot < 0) Say(RED, "No %s lot: %s", what, why);
        else Say(GREEN, "%s lot %d: %d %s", TownLotKindName(k), lot, townLots[lot].beds, k == LOT_HOME ? "beds" : "work spots");
    } else if (strcasecmp(what, "remove") == 0) {
        int lots = TownRemoveLotsIn(x1, y1, x2, y2, z), roads = TownRemoveRoadsIn(x1, y1, x2, y2, z);
        Say(ORANGE, "Removed %d lots, %d roads", lots, roads);
    } else {
        Say(RED, "town: road, home, work, remove or list");
        return;
    }
    BuildEntrances();
    BuildGraph();
}

// ---- rain [light|medium|heavy|stop|1-100] ----
static const char* const rainChoices[] = { "light", "medium", "heavy", "stop" };
static void RunRain(const CmdValue* v) {
    static const int coverage[] = { 5, 20, 50 };
    if (v[0].choice == 3) { StopRain(); Say(GRAY, "Rain stopped"); return; }
    int c = v[0].choice >= 0 ? coverage[v[0].choice] : (v[0].given ? v[0].i : 20);   // default: medium
    SpawnSkyWater(c);
    Say(BLUE, "Rain started (%d%% coverage, 30s)", c);
}

// ---- weather <kind> ----
static const char* const weatherChoices[] = { "clear", "cloudy", "rain", "heavy", "thunder", "snow", "mist" };
static void WeatherStatus(void) {
    Console_Printf(WHITE, "Current: %s (%.0f%%)", GetWeatherName(weatherState.current), weatherState.intensity * 100.0f);
}
static void RunWeather(const CmdValue* v) {
    static const WeatherType types[] = { WEATHER_CLEAR, WEATHER_CLOUDY, WEATHER_RAIN, WEATHER_HEAVY_RAIN,
                                         WEATHER_THUNDERSTORM, WEATHER_SNOW, WEATHER_MIST };
    weatherState.current = types[v[0].choice];
    weatherState.intensity = 1.0f;
    Say(GREEN, "Weather set to %s", GetWeatherName(weatherState.current));
}

// ---- season <name> ----
static const char* const seasonChoices[] = { "spring", "summer", "autumn", "winter" };
static void SeasonStatus(void) {
    Console_Printf(WHITE, "Current: %s (day %d)", GetSeasonName(GetCurrentSeason()), dayNumber);
}
static void RunSeason(const CmdValue* v) {
    dayNumber = daysPerSeason * v[0].choice + 1;
    if (seasonalAmplitude == 0) seasonalAmplitude = 20;
    Say(GREEN, "Jumped to %s (day %d)", GetSeasonName(GetCurrentSeason()), dayNumber);
}

// ---- terrain <name> ----
static void TerrainList(void) {
    Console_Print("Available terrains:", WHITE);
    for (int i = 0; i < terrainCount; i++) Console_Printf(GRAY, "  %s", terrainNames[i]);
}
static void RunTerrain(const CmdValue* v) {
    if (v[0].i < 0) { TerrainList(); return; }   // "terrain list"
    currentTerrain = v[0].i;
    InitDesignations();   // before generating: some generators place designations (CraftTest)
    GenerateCurrentTerrain();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    RebuildSimActivityCounts();
    InitFire(); InitSmoke(); InitSteam(); InitTemperature(); InitGroundWear(); InitFloorDirt();
    InitSnow(); InitLighting();
    BuildEntrances();
    BuildGraph();
    Say(GREEN, "Generated terrain: %s", terrainNames[currentTerrain]);
}

// ---- grid <size> ----
static const char* const gridChoices[] = { "32", "64", "128", "256" };
static void GridStatus(void) { Console_Printf(WHITE, "Current grid: %dx%d (depth %d)", gridWidth, gridHeight, gridDepth); }
static void RunGrid(const CmdValue* v) {
    static const int size[] = { 32, 64, 128, 256 }, chunk[] = { 8, 8, 16, 16 }, depth[] = { 6, 6, 6, 3 };
    int k = v[0].choice;
    InitGridWithSizeAndChunkSize(size[k], size[k], chunk[k], chunk[k]);
    gridDepth = depth[k];
    for (int z = 1; z < gridDepth; z++)
        for (int gy = 0; gy < gridHeight; gy++)
            for (int gx = 0; gx < gridWidth; gx++)
                GRID(gx, gy, z) = CELL_AIR;
    FillGroundLevel();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    InitItemSpatialGrid(gridWidth, gridHeight, gridDepth);
    InitDesignations();
    InitSimActivity();
    InitWater(); InitFire(); InitSmoke(); InitSteam(); InitTemperature(); InitGroundWear();
    InitFloorDirt(); InitSnow(); InitLighting();
    BuildEntrances();
    BuildGraph();
    offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
    offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
    Say(GREEN, "Grid resized to %dx%d (depth %d)", gridWidth, gridHeight, gridDepth);
}

// ---- scenario <name> / game <name> ----
static void ScenarioList(void) {
    Console_Print("Scenarios:", WHITE);
    for (int i = 0; i < ScenarioCount(); i++)
        Console_Printf(GRAY, "  %-11s %s", ScenarioAt(i)->name, ScenarioAt(i)->description);
}
static void RunScenarioCmd(const CmdValue* v) {
    if (v[0].i < 0) { ScenarioList(); return; }
    const char* name = ScenarioAt(v[0].i)->name;
    RunScenario(name);
    Say(GREEN, "Started scenario: %s (%d movers)", name, moverCount);
}
static void GameList(void) {
    Console_Print("Games:", WHITE);
    for (int i = 0; i < GameCount(); i++)
        Console_Printf(GRAY, "  %-10s %s", GameAt(i)->name, GameAt(i)->description);
}
static void RunGameCmd(const CmdValue* v) {
    if (v[0].i < 0) { GameList(); return; }
    const char* name = GameAt(v[0].i)->name;
    StartGame(name);
    Say(GREEN, "Started game: %s (%d movers)", name, moverCount);
}

// ---- spawnmovers <n> [add] ----
static const char* const addChoices[] = { "add" };
static void RunSpawnMovers(const CmdValue* v) {
    if (v[1].choice == 0) AddMoversDemo(v[0].i);
    else SpawnMoversDemo(v[0].i);
}

// find: search every table row (src/ui/find.c), or browse its tree with a leading '/'
static void FindSay(const char* line) { Say(WHITE, "%s", line); printf("%s\n", line); }
static void RunFind(const CmdValue* v) {
    FindBuildIndex();
    const char* text = v[0].given ? v[0].s : "";
    char line[200];
    if (text[0] == '\0' || text[0] == '/') {
        const char* path = text[0] == '/' ? text + 1 : "";
        FindChild kids[256];
        int n = FindTreeChildren(path, kids, 256);
        snprintf(line, sizeof(line), "find /%s: %d entries (%d rows in all)", path, n, FindCount());
        FindSay(line);
        for (int i = 0; i < n; i++) {
            if (kids[i].isBranch) snprintf(line, sizeof(line), "  %s/", kids[i].name);
            else { char row[160]; FindRowLine(kids[i].row, row, sizeof(row)); snprintf(line, sizeof(line), "  %s", row); }
            FindSay(line);
        }
        return;
    }
    int hits[12];
    bool near = false;
    int n = FindSearch(text, hits, 12, &near);
    snprintf(line, sizeof(line), n == 0 ? "find %s: nothing" : near ? "find %s: did you mean" : "find %s:", text);
    FindSay(line);
    for (int i = 0; i < n; i++) {
        char row[160];
        FindRowLine(hits[i], row, sizeof(row));
        snprintf(line, sizeof(line), "  %s", row);
        FindSay(line);
    }
}

static const char* const oneWayChoices[] = { "up", "down", "both", "n", "e", "s", "w" };
// One-way ladder or door: the one at the mouse (or x y), on the viewed level (ladders: or
// the one below). Ladders take up/down/both, doors n/e/s/w (the way through) or both.
static void RunOneWay(const CmdValue* v) {
    int x, y;
    if (v[1].given && v[2].given) { x = v[1].i; y = v[2].i; }
    else { Vector2 g = ScreenToGrid(GetMousePosition()); x = (int)g.x; y = (int)g.y; }
    static const OneWay dirs[] = { ONEWAY_UP, ONEWAY_DOWN, ONEWAY_NONE, ONEWAY_N, ONEWAY_E, ONEWAY_S, ONEWAY_W };
    OneWay dir = dirs[v[0].choice];
    if (x >= 0 && y >= 0 && x < gridWidth && y < gridHeight && currentViewZ >= 0 && currentViewZ < gridDepth &&
        GRID(x, y, currentViewZ) == CELL_DOOR) {
        if (SetDoorOneWay(x, y, currentViewZ, dir)) Say(GREEN, "Door at %d,%d: %s", x, y, oneWayChoices[v[0].choice]);
        else Say(YELLOW, "Doors take n, e, s, w or both");
        return;
    }
    if (dir != ONEWAY_UP && dir != ONEWAY_DOWN && dir != ONEWAY_NONE) { Say(YELLOW, "Ladders take up, down or both"); return; }
    int n = SetLadderOneWay(x, y, currentViewZ, dir);
    if (n == 0) n = SetLadderOneWay(x, y, currentViewZ - 1, dir);
    if (n == 0) { Say(YELLOW, "No ladder or door at %d,%d (level %d)", x, y, currentViewZ); return; }
    Say(GREEN, "Ladder at %d,%d: %s (%d pieces)", x, y, oneWayChoices[v[0].choice], n);
}

// ---- time: speed, daylength ----
static const char* const speedChoices[] = { "pause", "resume", "toggle" };
static void RunSpeed(const CmdValue* v) {
    switch (v[0].choice) {
        case 0: paused = true; break;
        case 1: paused = false; break;
        case 2: paused = !paused; break;
        default: gameSpeed = (float)v[0].i; paused = false; Say(GREEN, "Speed %dx", v[0].i); break;
    }
}
static const char* const dayChoices[] = { "fast", "normal", "slow" };
static void RunDayLength(const CmdValue* v) {
    static const float lengths[] = { 120.0f, 360.0f, 1080.0f };
    dayLength = lengths[v[0].choice];
    Say(GREEN, "Day length: %s (%.0f s)", dayChoices[v[0].choice], dayLength);
}

// ---- save / load / audit / events (F5-F8) ----
static void RunSave(const CmdValue* v) {
    (void)v;
    if (system("mkdir -p saves") != 0) { Say(RED, "Could not make saves/"); return; }
    if (!SaveWorld("saves/debug_save.bin")) return;
    Say(GREEN, "World saved");
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char cmd[256], timestamp[32], logPath[128];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d_%02d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    snprintf(cmd, sizeof(cmd), "gzip -c saves/debug_save.bin > saves/%s.bin.gz", timestamp);
    if (system(cmd) != 0) Say(YELLOW, "gzip copy failed");
    snprintf(logPath, sizeof(logPath), "saves/%s.events.log", timestamp);   // event log alongside
    EventLogDump(logPath);
    int violations = RunStateAudit(true);   // auto-audit: warn if violations found
    if (violations > 0) Say(RED, "Audit: %d violations (see console)", violations);
}
static void RunLoad(const CmdValue* v) {
    (void)v;
    if (LoadWorld("saves/debug_save.bin")) { InputMode_Reset(); Say(GREEN, "World loaded"); }
}
static void RunAudit(const CmdValue* v) {
    (void)v;
    int violations = RunStateAudit(true);
    if (violations == 0) Say(GREEN, "Audit: 0 violations");
    else Say(RED, "Audit: %d violations (see console)", violations);
}
static void RunEvents(const CmdValue* v) {
    (void)v;
    EventLogDump("navkit_events.log");
    Say(GREEN, "Event log dumped (%d entries)", EventLogCount());
}

// ---- view: level, overlay, follow ----
static const char* const levelChoices[] = { "up", "down" };
static void RunLevel(const CmdValue* v) {
    if (v[0].choice == 0 && currentViewZ < gridDepth - 1) currentViewZ++;
    else if (v[0].choice == 1 && currentViewZ > 0) currentViewZ--;
    else if (v[0].choice < 0 && v[0].i >= 0 && v[0].i < gridDepth) currentViewZ = v[0].i;
}
static void RunOverlay(const CmdValue* v) {
    (void)v;
    devUI = !devUI;
    if (!devUI) InputMode_ExitToNormal();   // the dev overlay sits on top of any game (games.h)
}
static void RunFollow(const CmdValue* v) {
    (void)v;
    // The hovered mover, else the selected one; nothing there: follow off
    int target = (hoveredMover >= 0 && hoveredMover < moverCount) ? hoveredMover
               : (selection.kind == SEL_MOVER && SelectionValid()) ? selection.idx : -1;
    if (target >= 0) ToggleMoverFollow(target);
    else if (followMoverIdx >= 0) { followMoverIdx = -1; Say(GRAY, "Follow: off"); }
}

// ---- world editing ----
static void RunGraph(const CmdValue* v) {
    (void)v;
    BuildEntrances();
    BuildGraph();
    Say(GREEN, "HPA graph built (%d edges)", graphEdgeCount);
}
static void RunFindPath(const CmdValue* v) {
    (void)v;
    if (pathAlgorithm == 1) {
        if (graphEdgeCount == 0) { BuildEntrances(); BuildGraph(); }
        else if (needsRebuild) UpdateDirtyChunks();
    }
    switch (pathAlgorithm) {
        case 0: RunAStar(); break;
        case 1: RunHPAStar(); break;
    }
}
static const char* const seedChoices[] = { "random" };
static void RunSeed(const CmdValue* v) {
    if (v[0].choice == 0) worldSeed = (uint64_t)time(NULL) ^ ((uint64_t)GetRandomValue(0, 0x7FFFFFFF) << 16);
    else worldSeed = (uint64_t)v[0].i;
    Say(GREEN, "New seed: %llu", (unsigned long long)worldSeed);
}
static void RunRegen(const CmdValue* v) {
    (void)v;
    InitPlants();
    ClearFarming();
    InitDesignations();   // before generating: some generators place designations (CraftTest)
    GenerateCurrentTerrain();
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
    InvalidateLighting();
    BuildEntrances();
    BuildGraph();
    Say(GREEN, "Generated terrain: %s", terrainNames[currentTerrain]);
}
static void RunFillWalls(const CmdValue* v) {
    (void)v;
    for (int gy = 0; gy < gridHeight; gy++)
        for (int gx = 0; gx < gridWidth; gx++) {
            GRID(gx, gy, currentViewZ) = CELL_WALL;
            SetWaterLevel(gx, gy, currentViewZ, 0);
            SetWaterSource(gx, gy, currentViewZ, false);
            SetWaterDrain(gx, gy, currentViewZ, false);
        }
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++) MarkChunkDirty(cx * chunkWidth, cy * chunkHeight, currentViewZ);
    BuildEntrances();
    BuildGraph();
    InitDesignations();
    Say(GREEN, "Filled z=%d with walls", currentViewZ);
}
static void RunCopyMap(const CmdValue* v) {
    (void)v;
    int floorDataSize = gridWidth * gridHeight + gridHeight;
    int bufferSize = gridDepth * (16 + floorDataSize) + 1;
    char* buffer = malloc((size_t)bufferSize);
    if (!buffer) { Say(RED, "Failed to allocate clipboard buffer"); return; }
    int idx = 0;
    for (int z = 0; z < gridDepth; z++) {
        idx += snprintf(buffer + idx, (size_t)(bufferSize - idx), "floor:%d\n", z);
        for (int row = 0; row < gridHeight; row++) {
            for (int col = 0; col < gridWidth; col++) {
                char c;
                switch (GRID(col, row, z)) {
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
    Say(ORANGE, "Map copied to clipboard (%d floors)", gridDepth);
}

// ---- entities: animals, agents, scattered items, stockpiles ----
static const char* const animalChoices[] = { "grazer", "steering", "predator" };
// SpawnAnimal picks a random dry walkable cell on one level and gives up quietly when that
// level has none (air, ladders, water): try the viewed level first, then the nearest others
static bool SpawnAnimalNear(int kind, int z0) {
    for (int d = 0; d < gridDepth; d++) {
        for (int sgn = 0; sgn < (d ? 2 : 1); sgn++) {
            int z = sgn ? z0 - d : z0 + d;
            if (z < 0 || z >= gridDepth) continue;
            int before = CountActiveAnimals();
            switch (kind) {
                case 0: SpawnAnimal(ANIMAL_GRAZER, z, BEHAVIOR_SIMPLE_GRAZER); break;
                case 1: SpawnAnimal(ANIMAL_GRAZER, z, BEHAVIOR_STEERING_GRAZER); break;
                case 2: SpawnAnimal(ANIMAL_PREDATOR, z, BEHAVIOR_PREDATOR); break;
            }
            if (CountActiveAnimals() > before) return true;
        }
    }
    return false;
}
static void RunAnimal(const CmdValue* v) {
    int n = v[1].given ? v[1].i : 1;
    int placed = 0;
    for (int i = 0; i < n; i++) if (SpawnAnimalNear(v[0].choice, currentViewZ)) placed++;
    if (placed > 0) Say(GREEN, "Spawned %d %s (the viewed level, else the nearest with dry ground)", placed, animalChoices[v[0].choice]);
    else Say(YELLOW, "No animal placed: no dry walkable cell on any level (or the animal limit is reached)");
}
static const char* const agentChoices[] = { "spawn", "repath" };
static void RunAgents(const CmdValue* v) {
    if (v[0].choice == 0) {
        if (graphEdgeCount == 0) { BuildEntrances(); BuildGraph(); }
        SpawnAgents(v[1].given ? v[1].i : 10);
    } else {
        if (pathAlgorithm == 1 && graphEdgeCount == 0) { BuildEntrances(); BuildGraph(); }
        RepathAgents();
    }
}
// scatter <count> <item>: on random walkable cells of the viewed level
static void RunScatter(const CmdValue* v) {
    ItemType type = (ItemType)v[1].i;
    int placed = 0;
    for (int i = 0; i < v[0].i; i++) {
        for (int attempts = 100; attempts > 0; attempts--) {
            int gx = GetRandomValue(0, gridWidth - 1), gy = GetRandomValue(0, gridHeight - 1);
            if (!IsCellWalkableAt(currentViewZ, gy, gx)) continue;
            SpawnItemWithMaterial(gx * CELL_SIZE + CELL_SIZE * 0.5f, gy * CELL_SIZE + CELL_SIZE * 0.5f,
                                  (float)currentViewZ, type, DefaultMaterialForItemType(type));
            placed++;
            break;
        }
    }
    Say(GREEN, "Scattered %d %s", placed, itemDefs[type].name);
}
static const char* const stockpileChoices[] = { "all", "red", "green", "blue" };
static void RunStockpile(const CmdValue* v) {
    int c = v[0].choice;
    SpawnStockpileWithFilters(c == 0 || c == 1, c == 0 || c == 2, c == 0 || c == 3);
}
static const char* const balanceChoices[] = { "reset" };
static void RunBalance(const CmdValue* v) { (void)v; InitBalance(); Say(GREEN, "Mover balance: defaults"); }

// ---- explosions ----
// Cell from x y z if given, else the mouse on the viewed level
static bool ExplodeTarget(const CmdValue* v, int* x, int* y, int* z) {
    if (v[0].given && v[1].given) { *x = v[0].i; *y = v[1].i; }
    else { Vector2 g = ScreenToGrid(GetMousePosition()); *x = (int)g.x; *y = (int)g.y; }
    *z = v[2].given ? v[2].i : currentViewZ;
    if (*x < 0 || *y < 0 || *z < 0 || *x >= gridWidth || *y >= gridHeight || *z >= gridDepth) {
        Say(YELLOW, "Outside the map");
        return false;
    }
    return true;
}
static void RunExplode(const CmdValue* v) {
    int x, y, z;
    if (!ExplodeTarget(v + 1, &x, &y, &z)) return;
    ExplosionResult r = Explode(x, y, z, v[0].f);
    Say(ORANGE, "Boom at %d,%d,%d (power %.0f): %d walls, %d floors, %d items, %d movers knocked, %d fires, %d gunpowder set off",
        x, y, z, v[0].f, r.cellsBroken, r.floorsBroken, r.itemsPushed, r.moversKnocked, r.cellsIgnited, r.powderSetOff);
}
static void RunCharge(const CmdValue* v) {
    int x, y, z;
    if (!ExplodeTarget(v + 2, &x, &y, &z)) return;
    if (ExplosionLightFuse(x, y, z, v[0].f, v[1].f)) Say(ORANGE, "Fuse lit at %d,%d,%d: power %.0f in %.0fs", x, y, z, v[0].f, v[1].f);
    else Say(YELLOW, "Too many lit fuses");
}

// ---- throwing ----
// throw [mover] [x y z]: the mover (else the selected one) throws something small at the cell
// (else the mouse on the viewed level)
static void RunThrow(const CmdValue* v) {
    int mi = v[0].given ? v[0].i : (SelectionValid() && selection.kind == SEL_MOVER ? selection.idx : -1);
    if (mi < 0) { Say(YELLOW, "Which mover? Select one or give its number"); return; }
    int x, y, z;
    if (v[1].given && v[2].given) { x = v[1].i; y = v[2].i; }
    else { Vector2 g = ScreenToGrid(GetMousePosition()); x = (int)g.x; y = (int)g.y; }
    z = v[3].given ? v[3].i : currentViewZ;
    const char* why = "";
    int job = ThrowOrder(mi, -1, x, y, z, &why);
    if (job < 0) { Say(YELLOW, "Can't throw: %s", why); return; }
    int it = jobs[job].targetItem;
    Say(GREEN, "Mover %d throws %s at %d,%d,%d (range %.0f cells)", mi, ItemName(items[it].type), x, y, z, ThrowRange(it));
}

static void RunSupport(const CmdValue* v) {
    (void)v;
    int n = SupportCountUnsupported();
    Say(n ? ORANGE : GREEN, "Unsupported structure cells: %d (collapses waiting: %d)", n, SupportPendingCount());
    printf("Unsupported structure cells: %d\n", n);
}

// ---- rendering toys ----
static void RunCutscene(const CmdValue* v) { (void)v; PlayTestCutscene(); }
static const char* const shakeChoices[] = { "small", "medium", "large" };
static void RunShake(const CmdValue* v) {
    static const float strength[] = { 2.0f, 4.0f, 8.0f }, duration[] = { 0.2f, 0.4f, 0.6f };
    TriggerScreenShake(strength[v[0].choice], duration[v[0].choice]);
}

#define ARG_CHOICE(n, arr) { .name = (n), .type = CARG_CHOICE, .choices = (arr), .choiceCount = (int)(sizeof(arr) / sizeof((arr)[0])) }
#define ARG_NAME(n, cnt, at) { .name = (n), .type = CARG_NAME, .nameCount = (cnt), .nameAt = (at) }

static const CommandDef commandTable[] = {
    { "spawn", "Entities/Items", "Spawn items at the mouse",
      2, { { .name = "count", .type = CARG_INT, .lo = 1, .hi = 100 }, { .name = "item", .type = CARG_ITEM } },
      RunSpawn, NULL },
    { "clear", "World/Clear", "Clear entities or simulation state",
      1, { ARG_CHOICE("what", clearChoices) }, RunClear, NULL },
    { "tp", "View", "Centre the camera on a cell",
      3, { { .name = "x", .type = CARG_INT }, { .name = "y", .type = CARG_INT },
           { .name = "z", .type = CARG_INT, .optional = true, .def = -1 } },
      RunTp, NULL },
    { "oneway", "World/Pathfinding", "One-way ladder (up/down) or door (n/e/s/w: the way through) at the mouse or x y; both = two-way",
      3, { ARG_CHOICE("dir", oneWayChoices), { .name = "x", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "y", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 } },
      RunOneWay, NULL },
    { "rain", "Simulation/Water", "Start or stop rain (coverage in %)",
      1, { { .name = "amount", .type = CARG_CHOICE, .optional = true, .choices = rainChoices, .choiceCount = 4, .lo = 1, .hi = 100 } },
      RunRain, NULL },
    { "town", "World/Town", "Town roads and lots at the viewed level: town road|home|work|remove x1 y1 x2 y2, town list",
      1, { { .name = "what x1 y1 x2 y2", .type = CARG_TEXT, .optional = true } }, RunTown, NULL },
    { "reveal", "World", "Lift the fog of war: every cell explored (dev cheat; survival/island)",
      0, { { 0 } }, RunReveal, NULL },
    { "weather", "Weather & Time/Weather", "Force the weather",
      1, { ARG_CHOICE("kind", weatherChoices) }, RunWeather, WeatherStatus },
    { "season", "Weather & Time/Seasons", "Jump to the first day of a season",
      1, { ARG_CHOICE("season", seasonChoices) }, RunSeason, SeasonStatus },
    { "terrain", "World/Terrain", "Generate a terrain",
      1, { ARG_NAME("terrain", TerrainNameCount, TerrainNameAt) }, RunTerrain, TerrainList },
    { "grid", "World/Grid", "Resize the grid (empty flat world)",
      1, { ARG_CHOICE("size", gridChoices) }, RunGrid, GridStatus },
    { "scenario", "Games", "Start a demo scenario",
      1, { ARG_NAME("scenario", ScenarioNameCount, ScenarioNameAt) }, RunScenarioCmd, ScenarioList },
    { "game", "Games", "Start a game",
      1, { ARG_NAME("game", GameNameCount, GameNameAt) }, RunGameCmd, GameList },
    { "speed", "Time", "Game speed: pause, resume, toggle or a multiplier 1-50",
      1, { { .name = "x", .type = CARG_CHOICE, .choices = speedChoices, .choiceCount = 3, .lo = 1, .hi = 50 } },
      RunSpeed, NULL },
    { "daylength", "Time", "How long a day takes: fast 2 min, normal 6 min, slow 18 min",
      1, { ARG_CHOICE("length", dayChoices) }, RunDayLength, NULL },
    { "save", "Save", "Quick save (saves/debug_save.bin, a dated .gz copy, the event log, an audit)",
      0, { { 0 } }, RunSave, NULL },
    { "load", "Save", "Quick load (saves/debug_save.bin)", 0, { { 0 } }, RunLoad, NULL },
    { "audit", "Save", "Run the state audit (consistency checks, see the console)", 0, { { 0 } }, RunAudit, NULL },
    { "events", "Save", "Dump the event log to navkit_events.log", 0, { { 0 } }, RunEvents, NULL },
    { "level", "View", "View level up, down or a number",
      1, { { .name = "z", .type = CARG_CHOICE, .choices = levelChoices, .choiceCount = 2, .lo = 0, .hi = 255 } },
      RunLevel, NULL },
    { "overlay", "View", "Dev overlay on / off (tools, settings; the game's rules stay)", 0, { { 0 } }, RunOverlay, NULL },
    { "follow", "View", "Camera follows the hovered (or selected) mover; again: off", 0, { { 0 } }, RunFollow, NULL },
    { "graph", "Pathfinding", "Build the HPA* graph", 0, { { 0 } }, RunGraph, NULL },
    { "findpath", "Pathfinding", "Find a path start -> goal with the current algorithm", 0, { { 0 } }, RunFindPath, NULL },
    { "seed", "World/Terrain", "World seed: random or a number",
      1, { { .name = "seed", .type = CARG_CHOICE, .choices = seedChoices, .choiceCount = 1, .lo = 0, .hi = 2000000000 } },
      RunSeed, NULL },
    { "regen", "World/Terrain", "Generate the current terrain again (with the current seed)", 0, { { 0 } }, RunRegen, NULL },
    { "fillwalls", "World/Map Editing", "Fill the viewed level with walls", 0, { { 0 } }, RunFillWalls, NULL },
    { "copymap", "World/Map Editing", "Copy the map as ASCII to the clipboard", 0, { { 0 } }, RunCopyMap, NULL },
    { "animal", "Entities/Animals", "Spawn animals: grazer, steering (grazer), predator",
      2, { ARG_CHOICE("kind", animalChoices), { .name = "n", .type = CARG_INT, .optional = true, .lo = 1, .hi = 500 } },
      RunAnimal, NULL },
    { "agents", "Entities/Agents", "Steering agents: spawn [n] or repath",
      2, { ARG_CHOICE("what", agentChoices), { .name = "n", .type = CARG_INT, .optional = true, .lo = 1, .hi = 10000 } },
      RunAgents, NULL },
    { "scatter", "Entities/Items", "Spawn items on random walkable cells of the viewed level",
      2, { { .name = "count", .type = CARG_INT, .lo = 1, .hi = 10000 }, { .name = "item", .type = CARG_ITEM } },
      RunScatter, NULL },
    { "stockpile", "Entities/Items", "Spawn a test stockpile: all, red, green or blue",
      1, { ARG_CHOICE("filter", stockpileChoices) }, RunStockpile, NULL },
    { "balance", "Mover Balance", "Mover balance back to its defaults", 1, { ARG_CHOICE("what", balanceChoices) }, RunBalance, NULL },
    { "explode", "World/Explosions", "Blast at the mouse (or x y z): breaks walls and floors (rock too), pushes items by weight, knocks movers back",
      4, { { .name = "power", .type = CARG_FLOAT, .optional = true, .lo = 1, .hi = EXPLOSION_MAX_POWER, .def = 6 },
           { .name = "x", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "y", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "z", .type = CARG_INT, .optional = true, .lo = 0, .hi = 255 } },
      RunExplode, NULL },
    { "charge", "World/Explosions", "Lit charge at the mouse (or x y z): explodes after the fuse (game seconds)",
      5, { { .name = "power", .type = CARG_FLOAT, .optional = true, .lo = 1, .hi = EXPLOSION_MAX_POWER, .def = 6 },
           { .name = "fuse", .type = CARG_FLOAT, .optional = true, .lo = 0, .hi = 120, .def = 3 },
           { .name = "x", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "y", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "z", .type = CARG_INT, .optional = true, .lo = 0, .hi = 255 } },
      RunCharge, NULL },
    { "throw", "Entities/Movers", "A mover (number, else the selected one) fetches something small (its hands, pockets, else the nearest) and throws it at the cell at the mouse (or x y z)",
      4, { { .name = "mover", .type = CARG_INT, .optional = true, .lo = 0, .hi = MAX_MOVERS - 1 },
           { .name = "x", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "y", .type = CARG_INT, .optional = true, .lo = 0, .hi = 4095 },
           { .name = "z", .type = CARG_INT, .optional = true, .lo = 0, .hi = 255 } },
      RunThrow, NULL },
    { "support", "World/Explosions", "Count structure cells not connected to the ground (they fall when something next to them changes)",
      0, { { 0 } }, RunSupport, NULL },
    { "cutscene", "Rendering & Debug", "Play the test cutscene", 0, { { 0 } }, RunCutscene, NULL },
    { "shake", "Rendering & Debug", "Screen shake: small, medium, large", 1, { ARG_CHOICE("size", shakeChoices) }, RunShake, NULL },
    { "find", "Dev", "Search every tool, command, setting, item ... (find /path browses)",
      1, { { .name = "text", .type = CARG_TEXT, .optional = true } }, RunFind, NULL },
    { "spawnmovers", "Entities/Movers", "Replace all movers with N wandering ones (add: keep the current ones)",
      2, { { .name = "n", .type = CARG_INT, .lo = 1, .hi = 100000 }, { .name = "mode", .type = CARG_CHOICE, .optional = true,
           .choices = addChoices, .choiceCount = 1 } },
      RunSpawnMovers, NULL },
};
#define COMMAND_COUNT ((int)(sizeof(commandTable) / sizeof(commandTable[0])))

int CommandCount(void) { return COMMAND_COUNT; }
const CommandDef* CommandAt(int i) { return (i >= 0 && i < COMMAND_COUNT) ? &commandTable[i] : NULL; }
const CommandDef* CommandFind(const char* name) {
    for (int i = 0; i < COMMAND_COUNT; i++) if (strcasecmp(commandTable[i].name, name) == 0) return &commandTable[i];
    return NULL;
}

// Parse argv by the row's spec and run it; a bad line prints what was wrong and the usage
static bool RunArgv(const CommandDef* d, int argc, const char** argv) {
    CmdValue v[CMD_MAX_ARGS];
    char err[160], usage[256];
    if (!CommandParse(d, argc, argv, v, err, sizeof(err))) {
        CommandUsage(d, usage, sizeof(usage));
        Say(YELLOW, "%s. Usage: %s", err, usage);
        if (d->status) d->status();
        return false;
    }
    d->run(v);
    return true;
}

static void ConsoleTrampoline(int argc, const char** argv) {
    const CommandDef* d = CommandFind(argv[0]);
    if (d) RunArgv(d, argc, argv);
}

void Commands_RegisterWithConsole(void) {
    // The console keeps a pointer to the help text: usage + help, made once per row
    static char helpText[COMMAND_COUNT][320];
    for (int i = 0; i < COMMAND_COUNT; i++) {
        char usage[256];
        CommandUsage(&commandTable[i], usage, sizeof(usage));
        snprintf(helpText[i], sizeof(helpText[i]), "%s: %s", commandTable[i].help, usage);
        Console_RegisterCmd(commandTable[i].name, ConsoleTrampoline, helpText[i]);
    }
}

bool CommandRunLine(const char* line) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", line);
    const char* argv[CMD_MAX_ARGS + 8];
    int argc = 0;
    for (char* tok = strtok(buf, " \t"); tok && argc < (int)(sizeof(argv) / sizeof(argv[0])); tok = strtok(NULL, " \t"))
        argv[argc++] = tok;
    if (argc == 0) return false;
    const CommandDef* d = CommandFind(argv[0]);
    if (!d) { Say(YELLOW, "Unknown command: %s", argv[0]); return false; }
    return RunArgv(d, argc, argv);
}

bool CommandButton(float x, float y, const char* label, const char* line) {
    if (!PushButton(x, y, label)) return false;
    CommandRunLine(line);
    return true;
}

float CommandButtonInline(float x, float y, const char* label, const char* line) {
    bool clicked = false;
    float w = PushButtonInline(x, y, label, &clicked);
    if (clicked) CommandRunLine(line);
    return w;
}
