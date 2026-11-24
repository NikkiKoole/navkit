// commands.c - the command table and what each command does (see commands.h)
// Moved here from console.c's Cmd_* functions (behaviour kept) and from the dev sidebar's
// hand-written buttons (ui_panels.c), which now run these rows.
#include "commands.h"
#include "console.h"
#include "../game_state.h"
#include "../../shared/ui.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/animals.h"
#include "../entities/stockpiles.h"
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
#include "../core/time.h"
#include "../core/sim_manager.h"
#include "../world/designations.h"
#include "../core/scenarios.h"
#include "../core/games.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

void SpawnMoversDemo(int count);   // main.c
void AddMoversDemo(int count);     // main.c

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

// ---- spawn <count> <item> ----
static void RunSpawn(const CmdValue* v) {
    int count = v[0].i;
    ItemType type = (ItemType)v[1].i;
    Vector2 gridPos = ScreenToGrid(GetMousePosition());
    int mx = (int)gridPos.x, my = (int)gridPos.y, mz = currentViewZ;
    if (mx < 0 || mx >= gridWidth || my < 0 || my >= gridHeight || mz < 0 || mz >= gridDepth) {
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
    Say(GREEN, "Spawned %d %s at mouse position", spawned, itemDefs[type].name);
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
    { "rain", "Simulation/Water", "Start or stop rain (coverage in %)",
      1, { { .name = "amount", .type = CARG_CHOICE, .optional = true, .choices = rainChoices, .choiceCount = 4, .lo = 1, .hi = 100 } },
      RunRain, NULL },
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
