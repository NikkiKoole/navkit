// render/ui_panels.c - UI panel drawing functions
#include "../game_state.h"
#include "play_hud.h"
#include "../ui/commands.h"   // command buttons run the command table (src/ui/commands.c)
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../world/biome.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/lighting.h"
#include "../simulation/weather.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/balance.h"
#include "../simulation/floordirt.h"
#include "../core/sim_manager.h"
#include "../core/input_mode.h"
#include "../world/designations.h"
#include "../ui/cutscene.h"
#include "../core/scenarios.h"
#include "../core/games.h"
#include "../core/settings.h"
#include <time.h>

// Forward declarations
void DrawTimeOfDayWidget(float x, float y);
void DrawUI(void);
void DrawProfilerPanel(float rightEdge, float y);

// ============================================================================
// TIME OF DAY WIDGET
// ============================================================================

// Get sky color based on time of day (0-24 hours)
Color GetSkyColorForTime(float hour) {
    // Time periods and their colors:
    // 0-4:   Night (dark blue)
    // 4-6:   Dawn (orange/pink gradient)
    // 6-8:   Morning (light blue warming up)
    // 8-12:  Late morning (bright sky blue)
    // 12-16: Afternoon (bright sky, slightly warmer)
    // 16-18: Golden hour (warm golden)
    // 18-20: Dusk (orange/red/purple)
    // 20-24: Night (dark blue)

    // Define key colors at specific hours
    typedef struct { float hour; Color color; } TimeColor;
    TimeColor keyframes[] = {
        { 0.0f,  (Color){ 15,  25,  50, 255} },  // Midnight - deep night blue
        { 4.0f,  (Color){ 25,  35,  65, 255} },  // Late night - slightly lighter
        { 5.0f,  (Color){ 70,  50,  80, 255} },  // Pre-dawn - purple hint
        { 6.0f,  (Color){180, 100,  80, 255} },  // Dawn - orange/pink
        { 7.0f,  (Color){220, 190, 160, 255} },  // Early morning - warm
        { 8.0f,  (Color){210, 220, 240, 255} },  // Morning - hint of blue
        { 9.0f,  (Color){245, 245, 240, 255} },  // Late morning - near white
        {12.0f,  (Color){255, 255, 250, 255} },  // Noon - neutral white
        {16.0f,  (Color){250, 245, 235, 255} },  // Afternoon - barely warm
        {17.0f,  (Color){220, 190, 150, 255} },  // Golden hour start
        {18.0f,  (Color){230, 140,  80, 255} },  // Golden hour - orange
        {19.0f,  (Color){180,  80,  70, 255} },  // Sunset - red/orange
        {20.0f,  (Color){ 80,  50,  90, 255} },  // Dusk - purple
        {21.0f,  (Color){ 35,  40,  70, 255} },  // Early night
        {24.0f,  (Color){ 15,  25,  50, 255} },  // Midnight again
    };
    int numKeyframes = sizeof(keyframes) / sizeof(keyframes[0]);

    // Find the two keyframes to interpolate between
    int i;
    for (i = 0; i < numKeyframes - 1; i++) {
        if (hour < keyframes[i + 1].hour) break;
    }

    // Interpolate
    float t = (hour - keyframes[i].hour) / (keyframes[i + 1].hour - keyframes[i].hour);
    Color c1 = keyframes[i].color;
    Color c2 = keyframes[i + 1].color;

    return (Color){
        (unsigned char)(c1.r + t * (c2.r - c1.r)),
        (unsigned char)(c1.g + t * (c2.g - c1.g)),
        (unsigned char)(c1.b + t * (c2.b - c1.b)),
        255
    };
}

// Draw the time-of-day widget at the specified position
void DrawTimeOfDayWidget(float x, float y) {
    // Widget dimensions - fixed size to prevent resizing
    int paddingY = 4;
    int fontSize = 16;
    int boxWidth = 140;

    // Get current time info
    int hours = (int)timeOfDay;
    int minutes = (int)((timeOfDay - hours) * 60);

    // Line 1: Season + Day, Line 2: Time + Weather, Line 3: Temp + Wind
    char line1[32];
    char line2[32];
    char line3[32];
    snprintf(line1, sizeof(line1), "%s Day %d", GetSeasonName(GetCurrentSeason()), dayNumber);
    snprintf(line2, sizeof(line2), "%02d:%02d %s", hours, minutes, GetWeatherName(weatherState.current));

    // Wind direction arrow from vector
    int temp = GetSeasonalSurfaceTemp();
    float ws = weatherState.windStrength;
    const char* windArrow = "";
    if (ws > 0.1f) {
        float wx = weatherState.windDirX;
        float wy = weatherState.windDirY;
        float angle = atan2f(wy, wx);  // radians
        // 8 compass directions
        if      (angle < -2.75f || angle > 2.75f)  windArrow = "W";
        else if (angle < -1.96f) windArrow = "NW";
        else if (angle < -1.18f) windArrow = "N";
        else if (angle < -0.39f) windArrow = "NE";
        else if (angle <  0.39f) windArrow = "E";
        else if (angle <  1.18f) windArrow = "SE";
        else if (angle <  1.96f) windArrow = "S";
        else                     windArrow = "SW";
    }
    if (ws > 0.1f) {
        snprintf(line3, sizeof(line3), "%dC  %s %.0f", temp, windArrow, ws);
    } else {
        snprintf(line3, sizeof(line3), "%dC  Calm", temp);
    }

    int line1Width = MeasureTextUI(line1, fontSize);
    int line2Width = MeasureTextUI(line2, fontSize);
    int line3Width = MeasureTextUI(line3, fontSize);
    int lineSpacing = 2;
    int boxHeight = fontSize * 3 + lineSpacing * 2 + paddingY * 2;

    // Widen box if needed
    int maxTextWidth = line1Width;
    if (line2Width > maxTextWidth) maxTextWidth = line2Width;
    if (line3Width > maxTextWidth) maxTextWidth = line3Width;
    if (maxTextWidth + 16 > boxWidth) boxWidth = maxTextWidth + 16;

    // Draw sky-colored background rectangle (white when force daylight is on)
    Color skyColor = forceDaylight ? WHITE : GetSkyColorForTime(timeOfDay);
    DrawRectangle((int)x, (int)y, boxWidth, boxHeight, skyColor);
    Color borderColor = forceDaylight ? (Color){255, 220, 50, 255} : (Color){100, 100, 100, 255};
    DrawRectangleLines((int)x, (int)y, boxWidth, boxHeight, borderColor);

    // Draw line 1 (season + day) centered
    int text1X = (int)x + (boxWidth - line1Width) / 2;
    int text1Y = (int)y + paddingY;
    DrawTextShadow(line1, text1X, text1Y, fontSize, WHITE);

    // Draw line 2 (time + weather) centered
    int text2X = (int)x + (boxWidth - line2Width) / 2;
    int text2Y = text1Y + fontSize + lineSpacing;
    DrawTextShadow(line2, text2X, text2Y, fontSize, (Color){200, 200, 220, 255});

    // Draw line 3 (temp + wind) centered
    int text3X = (int)x + (boxWidth - line3Width) / 2;
    int text3Y = text2Y + fontSize + lineSpacing;
    DrawTextShadow(line3, text3X, text3Y, fontSize, (Color){180, 200, 180, 255});

    // Click to toggle force daylight
    Vector2 mouse = GetMousePosition();
    Rectangle bounds = {x, y, (float)boxWidth, (float)boxHeight};
    if (CheckCollisionPointRec(mouse, bounds)) {
        ui_set_hovered();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            forceDaylight = !forceDaylight;
        }
    }
}

// Forward declarations for demo functions
void SpawnAgents(int count);
void RepathAgents(void);
void AddMoversDemo(int count);
void SpawnMoversDemo(int count);
void SpawnStockpileWithFilters(bool allowRed, bool allowGreen, bool allowBlue);

// Draw every setting in one group (src/core/settings.c), in table order, one row
// each, 22px apart. Like the hand-written rows: *y ends ON the last row, so the
// caller adds the gap before what comes next.
static void DrawSettingsGroup(const char* group, float x, float* y) {
    bool first = true;
    for (int i = 0; i < SettingCount(); i++) {
        const Setting* s = SettingAt(i);
        if (strcmp(s->group, group) != 0) continue;
        if (!first) *y += 22;
        first = false;
        const char* tip = SettingTooltip(s);
        float before = SettingValue(s);
        switch (s->type) {
            case SETTING_BOOL:
                ToggleBoolT(x, *y, s->label, (bool*)s->ptr, tip);
                break;
            case SETTING_INT:
                DraggableIntT(x, *y, s->label, (int*)s->ptr, s->speed, (int)s->min, (int)s->max, tip);
                break;
            case SETTING_FLOAT:
                DraggableFloatT(x, *y, s->label, (float*)s->ptr, s->speed, s->min, s->max, tip);
                break;
            case SETTING_CHOICE:
                CycleOption(x, *y, s->label, s->options, s->optionCount, (int*)s->ptr);
                break;
        }
        if (s->onChange && SettingValue(s) != before) s->onChange(before);
    }
}

// Minimal player HUD — speed controls, new game; tools + roster in the tab bar (play_hud.c)
static void DrawPlayerHUD(void) {
    ui_begin_frame();

    // Speed controls top-left, below FPS
    float y = 30;
    float bx = 10;
    bx += CommandButtonInline(bx, y, paused ? ">>" : "||", "speed toggle") + 4;
    bx += CommandButtonInline(bx, y, "1x", "speed 1") + 4;
    bx += CommandButtonInline(bx, y, "2x", "speed 2") + 4;
    CommandButtonInline(bx, y, "3x", "speed 3");
    y += 22;

    // Tools and the mover roster: tab bar bottom left (src/render/play_hud.c)
    DrawPlayHudTabs();
    DrawTownOverview();   // town games: the town in one line, top centre
    y += 4;

    // New game: one button per game (src/core/games.c)
    for (int gi = 0; gi < GameCount(); gi++) {
        const GameDef* g = GameAt(gi);
        Rectangle r = {10, y, (float)MeasureTextUI(TextFormat("[%s]", g->label), UI_FONT_MEDIUM) + 10, (float)(UI_FONT_MEDIUM + 2)};
        if (CheckCollisionPointRec(GetMousePosition(), r)) ui_set_tooltip(g->description);
        CommandButton(10, y, g->label, TextFormat("game %s", g->name));
        y += 22;
    }

    // Game Over overlay
    if (gameOverTriggered && !IsCutsceneActive()) {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 120});
        Font font = (g_cutscene_font && g_cutscene_font->texture.id > 0)
            ? *g_cutscene_font : GetFontDefault();
        const char* text = "GAME OVER";
        float fontSize = 60.0f;
        float spacing = 3.0f;
        Vector2 textSize = MeasureTextEx(font, text, fontSize, spacing);
        Vector2 textPos = { (screenW - textSize.x) / 2, screenH / 3.0f };
        DrawTextEx(font, text, textPos, fontSize, spacing, (Color){160, 82, 45, 255});
        // Survival time score
        int gameDays = (int)(survivalDuration / dayLength);
        int gameHours = (int)(fmod(survivalDuration, dayLength) / dayLength * 24.0);
        char scoreText[64];
        if (gameDays > 0) {
            snprintf(scoreText, sizeof(scoreText), "You survived %d day%s, %d hour%s",
                     gameDays, gameDays == 1 ? "" : "s", gameHours, gameHours == 1 ? "" : "s");
        } else {
            snprintf(scoreText, sizeof(scoreText), "You survived %d hour%s", gameHours, gameHours == 1 ? "" : "s");
        }
        float scoreSize = 28.0f;
        Vector2 scoreMeasure = MeasureTextEx(font, scoreText, scoreSize, 2.0f);
        Vector2 scorePos = { (screenW - scoreMeasure.x) / 2, screenH / 3.0f + 70 };
        DrawTextEx(font, scoreText, scorePos, scoreSize, 2.0f, (Color){242, 229, 215, 255});

        const char* hint = "Press Survival to try again";
        float hintSize = 20.0f;
        Vector2 hintMeasure = MeasureTextEx(font, hint, hintSize, 2.0f);
        Vector2 hintPos = { (screenW - hintMeasure.x) / 2, screenH / 3.0f + 110 };
        DrawTextEx(font, hint, hintPos, hintSize, 2.0f, (Color){150, 150, 150, 255});
    }

    // Hint text
    if (inputAction != ACTION_NONE) {
        DrawTextShadow("L-drag designate  R-drag cancel  ESC: back", 10, (int)y, 12, (Color){180, 180, 100, 255});
        y += 16;
    }
    DrawTextShadow("TAB: dev UI", 10, (int)y, 12, (Color){100, 100, 100, 255});
}

void DrawUI(void) {
    if (!devUI) {
        DrawPlayerHUD();
        return;
    }

    ui_begin_frame();
    float y = 30.0f;
    float x = 10.0f;
    float ix = x + 10;  // indented x for sub-sections

    // ========================================================================
    // [+] Pathfinding  (standalone — small, 4 controls)
    // ========================================================================
    if (SectionHeader(x, y, "Pathfinding", &sectionPathfinding)) {
        y += 18;
        DrawSettingsGroup("Pathfinding", x, &y);   // Algo, Dir (src/core/settings.c)
        y += 22;
        CommandButton(x, y, "Build HPA Graph", "graph");
        y += 22;
        CommandButton(x, y, "Find Path", "findpath");
    }
    y += 22;

    // ========================================================================
    // [+] World  (super-group: Map Editing, Trees, Entropy)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "World", &sectionWorld)) {
        y += 18;

        // --- Map Editing ---
        if (SectionHeader(ix, y, "Map Editing", &sectionMapEditing)) {
            y += 18;
            CycleOption(ix, y, "Tool", toolNames, 6, &currentTool);
            y += 22;
            CycleOption(ix, y, "Terrain", terrainNames, terrainCount, &currentTerrain);
            y += 22;
            bool isHillsTerrain = (currentTerrain == 17 || currentTerrain == 18 || currentTerrain == 19);
            bool isHillsWater = (currentTerrain == 19);
            // Generator parameters (src/core/settings.c), only for the terrains that use them
            if (isHillsTerrain) {
                DrawSettingsGroup("World/Terrain/Hills", ix, &y);
                y += 22;
            }
            if (isHillsWater) {
                DrawSettingsGroup("World/Terrain/HillsWater", ix, &y);
                y += 22;
                DrawSettingsGroup("World/Terrain/Biome", ix, &y);   // loads river/lake counts
                y += 22;
                DrawTextShadow(biomePresets[selectedBiome].description, ix, y, 11, GRAY);
                y += 16;
            }
            CommandButton(ix, y, "Randomize Seed", "seed random");
            y += 22;
            CommandButton(ix, y, "Generate Terrain", "regen");
            y += 22;
            // Games (src/core/games.c): start one, description on hover
            DrawTextShadow("Games:", ix, y, 14, GRAY);
            y += 18;
            for (int gi = 0; gi < GameCount(); gi++) {
                const GameDef* g = GameAt(gi);
                Rectangle r = {(float)ix, (float)y, (float)MeasureTextUI(TextFormat("[%s]", g->name), UI_FONT_MEDIUM) + 10, (float)(UI_FONT_MEDIUM + 2)};
                if (CheckCollisionPointRec(GetMousePosition(), r)) ui_set_tooltip(g->description);
                CommandButton(ix, y, g->name, TextFormat("game %s", g->name));
                y += 22;
            }
            // Scenarios (src/core/scenarios.c): one button each, description on hover
            DrawTextShadow("Scenarios:", ix, y, 14, GRAY);
            y += 18;
            for (int si = 0; si < ScenarioCount(); si++) {
                const Scenario* sc = ScenarioAt(si);
                Rectangle r = {(float)ix, (float)y, (float)MeasureTextUI(TextFormat("[%s]", sc->name), UI_FONT_MEDIUM) + 10, (float)(UI_FONT_MEDIUM + 2)};
                if (CheckCollisionPointRec(GetMousePosition(), r)) ui_set_tooltip(sc->description);
                CommandButton(ix, y, sc->name, TextFormat("scenario %s", sc->name));
                y += 22;
            }
            // Grid size buttons — inline row
            DrawTextShadow("Grid:", ix, y, 14, GRAY);
            {
                float bx = ix + 34;
                // The grid command (src/ui/commands.c)
                bx += CommandButtonInline(bx, y, "32x32", "grid 32");
                bx += CommandButtonInline(bx, y, "64x64", "grid 64");
                bx += CommandButtonInline(bx, y, "128x128", "grid 128");
                CommandButtonInline(bx, y, "256x256", "grid 256");
            }
            y += 22;
            CommandButton(ix, y, "Fill with Walls", "fillwalls");
            y += 22;
            CommandButton(ix, y, "Copy Map ASCII", "copymap");
        }
        y += 22;

        // --- Trees ---
        if (SectionHeader(ix, y, "Trees", &sectionTrees)) {
            y += 18;
            DrawSettingsGroup("World/Trees", ix, &y);   // src/core/settings.c
            y += 22;
            DrawTextShadow(TextFormat("Sandbox Tree Type: %s", TreeTypeName(currentTreeType)), ix, y, 14, GRAY);
        }
        y += 22;

        // --- Entropy (Ground Wear) ---
        if (SectionHeader(ix, y, "Entropy", &sectionEntropy)) {
            y += 18;
            DrawSettingsGroup("World/Entropy", ix, &y);   // src/core/settings.c
            y += 22;
            CommandButton(ix, y, "Clear Wear", "clear wear");
            y += 22;

            // Advanced sub-section
            if (SectionHeader(ix + 10, y, "Advanced", &sectionEntropyAdvanced)) {
                y += 18;
                DrawSettingsGroup("World/Entropy/Advanced", ix + 10, &y);
                y += 22;
                int decaySteps = (wearMax - wearDirtToGrass) / wearDecayRate;
                float gameSecondsToRegrow = decaySteps * wearRecoveryInterval;
                if (gameSecondsToRegrow < 60) {
                    DrawTextShadow(TextFormat("Regrow time: %.1fs game-time", gameSecondsToRegrow), ix + 10, y, 14, GRAY);
                } else {
                    DrawTextShadow(TextFormat("Regrow time: %.1fm game-time", gameSecondsToRegrow / 60.0f), ix + 10, y, 14, GRAY);
                }
            }
        }
    }
    y += 22;

    // ========================================================================
    // [+] Entities  (super-group: Movers, Animals inline, Agents inline, Jobs)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Entities", &sectionEntities)) {
        y += 18;

        // --- Movers ---
        CommandButton(ix + 140, y, "+", TextFormat("spawnmovers %d add", moverCountSetting));
        if (SectionHeader(ix, y, TextFormat("Movers (%d/%d)", CountActiveMovers(), moverCount), &sectionMovers)) {
            y += 18;
            DraggableIntLog(ix, y, "Count", &moverCountSetting, 1.0f, 1, MAX_MOVERS);
            y += 22;
            CommandButton(ix, y, "Spawn Movers", TextFormat("spawnmovers %d", moverCountSetting));
            y += 22;
            CommandButton(ix, y, "Clear Movers", "clear movers");
            y += 22;
            // Rows from src/core/settings.c
            DrawSettingsGroup("Entities/Movers", ix, &y);

            // Needs subsection (rule toggles; games set these when they start)
            y += 22;
            if (SectionHeader(ix + 10, y, "Needs", &sectionMoverNeeds)) {
                y += 18;
                DrawSettingsGroup("Entities/Movers/Needs", ix + 10, &y);
            }

            // Avoidance subsection
            y += 22;
            if (SectionHeader(ix + 10, y, "Avoidance", &sectionMoverAvoidance)) {
                y += 18;
                DrawSettingsGroup("Entities/Movers/Avoidance", ix + 10, &y);
            }

            // Walls subsection
            y += 22;
            if (SectionHeader(ix + 10, y, "Walls", &sectionMoverWalls)) {
                y += 18;
                DrawSettingsGroup("Entities/Movers/Walls", ix + 10, &y);
            }
        }
        y += 22;

        // --- Animals (inline — no own accordion) ---
        DrawTextShadow(TextFormat("Animals (%d)", CountActiveAnimals()), ix, y, 14, GRAY);
        {
            float bx = ix + 80;
            bx += CommandButtonInline(bx, y, "+", "animal grazer");
            bx += CommandButtonInline(bx, y, "+S", "animal steering");
            bx += CommandButtonInline(bx, y, "+W", "animal predator");
            CommandButtonInline(bx, y, "Clear", "clear animals");
        }
        y += 22;
        DrawSettingsGroup("Entities/Animals", ix + 10, &y);
        y += 22;

        // --- Agents (inline — no own accordion) ---
        DrawTextShadow("Agents:", ix, y, 14, GRAY);
        y += 18;
        DraggableInt(ix, y, "Count", &agentCountSetting, 1.0f, 1, MAX_AGENTS);
        y += 22;
        {
            float bx = ix;
            bx += CommandButtonInline(bx, y, "Spawn", TextFormat("agents spawn %d", agentCountSetting));
            CommandButtonInline(bx, y, "Repath", "agents repath");
        }
        y += 22;

        // --- Trains ---
        DrawTextShadow("Trains:", ix, y, 14, GRAY);
        y += 18;
        DrawSettingsGroup("Entities/Trains", ix + 10, &y);
        y += 22;

        // --- Elevators ---
        DrawTextShadow("Elevators:", ix, y, 14, GRAY);
        y += 18;
        DrawSettingsGroup("Entities/Elevators", ix + 10, &y);
        y += 22;

        // --- Belts ---
        DrawTextShadow("Belts:", ix, y, 14, GRAY);
        y += 18;
        DrawSettingsGroup("Entities/Belts", ix + 10, &y);
        y += 22;

        // --- Jobs ---
        if (SectionHeader(ix, y, TextFormat("Jobs (%d items)", itemCount), &sectionJobs)) {
            y += 18;
            DraggableIntLog(ix, y, "Count", &itemCountSetting, 1.0f, 1, MAX_ITEMS);
            y += 22;
            // Spawn item buttons (packed in rows)
            {
                struct { const char* label; ItemType type; uint8_t mat; } spawnDefs[] = {
                    {"Red",     ITEM_RED,         MAT_NONE},
                    {"Green",   ITEM_GREEN,       MAT_NONE},
                    {"Blue",    ITEM_BLUE,        MAT_NONE},
                    {"Rocks",   ITEM_ROCK,        MAT_NONE},
                    {"Blocks",  ITEM_BLOCKS,      MAT_GRANITE},
                    {"Logs",    ITEM_LOG,         MAT_OAK},
                    {"Planks",  ITEM_PLANKS,      MAT_OAK},
                    {"Sticks",  ITEM_STICKS,      MAT_NONE},
                    {"Cordage", ITEM_CORDAGE,     MAT_NONE},
                    {"Dirt",    ITEM_DIRT,         MAT_NONE},
                    {"Bricks",  ITEM_BRICKS,      MAT_BRICK},
                    {"D.Grass", ITEM_DRIED_GRASS, MAT_NONE},
                };
                int spawnDefCount = sizeof(spawnDefs) / sizeof(spawnDefs[0]);
                float bx = (float)ix;
                for (int d = 0; d < spawnDefCount; d++) {
                    float w = CommandButtonInline(bx, (float)y, spawnDefs[d].label,
                                                  TextFormat("scatter %d %s", itemCountSetting, itemDefs[spawnDefs[d].type].name));
                    bx += w;
                    if (bx > ix + 170) { bx = (float)ix; y += 22; }
                }
                if (bx > (float)ix) y += 22;
            }

            CommandButton(ix, y, "Clear Items", "clear items");
            y += 22;
            CommandButton(ix, y, "Stockpile: All", "stockpile all");
            y += 22;
            CommandButton(ix, y, "Stockpile: Red", "stockpile red");
            y += 22;
            CommandButton(ix, y, "Stockpile: Green", "stockpile green");
            y += 22;
            CommandButton(ix, y, "Stockpile: Blue", "stockpile blue");
            y += 22;
            CommandButton(ix, y, "Clear Stockpiles", "clear stockpiles");
        }
    }
    y += 22;

    // ========================================================================
    // [+] Simulation  (super-group: Water, Fire, Smoke, Steam, Temperature)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Simulation", &sectionSimulation)) {
        y += 18;

        // Clear All Sim — consolidated button at top
        CommandButton(ix, y, "Clear All Sim", "clear all");
        y += 22;

        // Rows come from the settings table (src/core/settings.c); buttons and
        // notes around them are drawn here.

        // --- Water ---
        if (SectionHeader(ix, y, "Water", &sectionWater)) {
            y += 18;
            DrawSettingsGroup("Simulation/Water", ix, &y);
            y += 22;
            CommandButton(ix, y, "Clear Water", "clear water");
            y += 22;
            DrawTextShadow(IsRaining() ? "Rain (active):" : "Rain:", ix, y, 14, IsRaining() ? BLUE : GRAY);
            y += 18;
            {
                float bx = (float)ix;
                bx += CommandButtonInline(bx, (float)y, "Light", "rain light");
                bx += CommandButtonInline(bx, (float)y, "Medium", "rain medium");
                bx += CommandButtonInline(bx, (float)y, "Heavy", "rain heavy");
                if (IsRaining()) CommandButtonInline(bx, (float)y, "Stop", "rain stop");
            }
            y += 22;

            // Advanced: mover speed in water, soil wetness
            if (SectionHeader(ix + 10, y, "Advanced", &sectionWaterAdvanced)) {
                y += 18;
                DrawSettingsGroup("Simulation/Water/Advanced", ix + 10, &y);
            }
        }
        y += 22;

        // --- Fire ---
        if (SectionHeader(ix, y, "Fire", &sectionFire)) {
            y += 18;
            DrawSettingsGroup("Simulation/Fire", ix, &y);
            y += 22;
            CommandButton(ix, y, "Clear Fire", "clear fire");
            y += 22;
            DrawTextShadow("Wetness: damp -50% spread, wet -90%, soaked blocks", ix, y, 14, GRAY);
            y += 16;
            DrawTextShadow("Rain: exposed fires lose levels (20%/40% per fuel tick)", ix, y, 14, GRAY);
            y += 18;

            // Advanced: spread chance formula
            if (SectionHeader(ix + 10, y, "Advanced", &sectionFireAdvanced)) {
                y += 18;
                DrawTextShadow("Spread Chance Formula:", ix + 10, y, 14, GRAY);
                y += 18;
                DrawSettingsGroup("Simulation/Fire/Advanced", ix + 10, &y);
                y += 22;
                int minSpread = fireSpreadBase + (FIRE_MIN_SPREAD_LEVEL * fireSpreadPerLevel);
                int maxSpread = fireSpreadBase + (FIRE_MAX_LEVEL * fireSpreadPerLevel);
                DrawTextShadow(TextFormat("Level 2: %d%%, Level 7: %d%%", minSpread, maxSpread), ix + 10, y, 14, GRAY);
            }
        }
        y += 22;

        // --- Smoke ---
        if (SectionHeader(ix, y, "Smoke", &sectionSmoke)) {
            y += 18;
            DrawSettingsGroup("Simulation/Smoke", ix, &y);
            y += 22;
            CommandButton(ix, y, "Clear Smoke", "clear smoke");
            y += 22;
            DrawTextShadow("Wet cells: 2-3x smoke. Rain: slower rise/dissipation", ix, y, 14, GRAY);
        }
        y += 22;

        // --- Steam ---
        if (SectionHeader(ix, y, "Steam", &sectionSteam)) {
            y += 18;
            DrawSettingsGroup("Simulation/Steam", ix, &y);
            y += 22;
            CommandButton(ix, y, "Clear Steam", "clear steam");
            y += 22;

            // Advanced: condensation/generation parameters
            if (SectionHeader(ix + 10, y, "Advanced", &sectionSteamAdvanced)) {
                y += 18;
                DrawSettingsGroup("Simulation/Steam/Advanced", ix + 10, &y);
            }
        }
        y += 22;

        // --- Temperature ---
        if (SectionHeader(ix, y, "Temperature", &sectionTemperature)) {
            y += 18;
            DrawSettingsGroup("Simulation/Temperature", ix, &y);
            y += 22;
            CommandButton(ix, y, "Reset to Ambient", "clear temp");
            y += 22;

            // Advanced: insulation, heat sources, physics
            if (SectionHeader(ix + 10, y, "Advanced", &sectionTemperatureAdvanced)) {
                y += 18;
                DrawSettingsGroup("Simulation/Temperature/Advanced", ix + 10, &y);
            }
        }
    }
    y += 22;

    // ========================================================================
    // [+] Weather & Time  (super-group)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Weather & Time", &sectionWeatherTime)) {
        y += 18;

        // --- Weather ---
        if (SectionHeader(ix, y, TextFormat("Weather: %s %.0f%%", GetWeatherName(weatherState.current),
                          weatherState.intensity * 100.0f), &sectionWeather)) {
            y += 18;

            // Compact status (2 lines instead of 3)
            DrawTextShadow(TextFormat("Wind: %.1f @ (%.1f, %.1f)  Timer: %.0f/%.0fs",
                           weatherState.windStrength, weatherState.windDirX, weatherState.windDirY,
                           weatherState.transitionTimer, weatherState.transitionDuration), ix, y, 12, GRAY);
            y += 18;

            // Weather trigger buttons — 2 rows instead of 3
            DrawTextShadow("Force:", ix, y, 14, GRAY);
            {
                float bx = ix + 40;
                bx += CommandButtonInline(bx, y, "Clear", "weather clear");
                bx += CommandButtonInline(bx, y, "Cloudy", "weather cloudy");
                bx += CommandButtonInline(bx, y, "Mist", "weather mist");
                CommandButtonInline(bx, y, "Rain", "weather rain");
            }
            y += 22;
            {
                float bx = ix;
                bx += CommandButtonInline(bx, y, "Heavy", "weather heavy");
                bx += CommandButtonInline(bx, y, "Thunder", "weather thunder");
                CommandButtonInline(bx, y, "Snow", "weather snow");
            }
            y += 22;

            // Settings (src/core/settings.c)
            DrawSettingsGroup("Weather & Time/Weather", ix, &y);
        }
        y += 22;

        // --- Time ---
        if (SectionHeader(ix, y, "Time", &sectionTime)) {
            y += 18;

            // Game time status (compact — widget already shows season/day/time)
            DrawTextShadow(TextFormat("Game time: %.1fs  |  Temp: %dC", (float)gameTime, GetSeasonalSurfaceTemp()), ix, y, 14, GRAY);
            y += 22;

            // Game speed, day length, fixed timestep (src/core/settings.c)
            DrawSettingsGroup("Weather & Time/Time", ix, &y);
            y += 22;

            // Speed presets — compact: Pause + speeds on 2 rows
            {
                float bx = ix;
                bx += CommandButtonInline(bx, y, "Pause", "speed pause");
                bx += CommandButtonInline(bx, y, "1x", "speed 1");
                bx += CommandButtonInline(bx, y, "2x", "speed 2");
                CommandButtonInline(bx, y, "3x", "speed 3");
            }
            y += 22;
            {
                float bx = ix;
                bx += CommandButtonInline(bx, y, "5x", "speed 5");
                bx += CommandButtonInline(bx, y, "10x", "speed 10");
                CommandButtonInline(bx, y, "50x", "speed 50");
            }
            y += 22;

            // Day length presets — inline
            DrawTextShadow("Day presets:", ix, y, 14, GRAY);
            {
                float bx = ix + 76;
                bx += CommandButtonInline(bx, y, "Fast", "daylength fast");
                bx += CommandButtonInline(bx, y, "Normal", "daylength normal");
                CommandButtonInline(bx, y, "Slow", "daylength slow");
            }
            y += 22;

            // Seasons
            DrawTextShadow("Jump to Season:", ix, y, 14, GRAY);
            y += 18;
            {
                float bx = ix;
                bx += CommandButtonInline(bx, y, "Spring", "season spring");
                bx += CommandButtonInline(bx, y, "Summer", "season summer");
                bx += CommandButtonInline(bx, y, "Autumn", "season autumn");
                CommandButtonInline(bx, y, "Winter", "season winter");
            }
            y += 22;

            DrawSettingsGroup("Weather & Time/Seasons", ix, &y);
        }
        y += 22;

        // --- Balance ---
        // Rows from src/core/settings.c; changing one recalculates the derived rates
        // (RecalcBalanceTable) and Base Speed rescales existing movers.
        if (SectionHeader(ix, y, "Mover Balance", &sectionBalance)) {
            y += 18;
            DrawSettingsGroup("Mover Balance", ix, &y);
            y += 22;

            DrawTextShadow("Day Budget (game-hours):", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Mover Balance/Day Budget", ix, &y);
            y += 22;

            y += 4;
            DrawTextShadow("Thresholds (0-1):", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Mover Balance/Thresholds", ix, &y);
            y += 22;

            y += 4;
            DrawTextShadow("Multipliers:", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Mover Balance/Multipliers", ix, &y);
            y += 22;

            y += 4;
            DrawTextShadow("Body Temperature:", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Mover Balance/Body Temperature", ix, &y);
            y += 22;

            y += 4;
            DrawTextShadow("Sleep Rhythm:", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Mover Balance/Sleep Rhythm", ix, &y);
            y += 22;

            // Derived rates (read-only)
            y += 4;
            DrawTextShadow("Derived Rates (per game-hour):", ix, y, 14, GRAY);
            y += 18;
            DrawTextShadow(TextFormat("Hunger drain:      %.4f/gh", balance.hungerDrainPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Energy drain (work): %.4f/gh", balance.energyDrainWorkPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Energy drain (idle): %.4f/gh", balance.energyDrainIdlePerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Bed recovery:      %.4f/gh", balance.bedRecoveryPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Ground recovery:   %.4f/gh", balance.groundRecoveryPerGH), ix, y, 14, WHITE);
            y += 16;
            DrawTextShadow(TextFormat("Thirst drain:      %.4f/gh", balance.thirstDrainPerGH), ix, y, 14, WHITE);
            y += 22;

            // Reset button
            CommandButton(ix, y, "Reset Defaults", "balance reset");
            y += 22;
        }
        y += 22;
    }
    y += 22;

    // ========================================================================
    // [+] Rendering & Debug  (super-group: View inline, Lighting, Debug)
    // ========================================================================
    y += 4;
    if (SectionHeader(x, y, "Rendering & Debug", &sectionRenderDebug)) {
        y += 18;

        // View — inline buttons, no accordion
        DrawTextShadow("View:", ix, y, 14, GRAY);
        {
            float bx = ix + 34;
            bx += CommandButtonInline(bx, y, "Cutscene", "cutscene");
            bx += CommandButtonInline(bx, y, "Shk S", "shake small");
            bx += CommandButtonInline(bx, y, "Shk M", "shake medium");
            CommandButtonInline(bx, y, "Shk L", "shake large");
        }
        y += 22;

        // --- Lighting ---
        if (SectionHeader(ix, y, TextFormat("Lighting (%d src)", lightSourceCount), &sectionLighting)) {
            y += 18;
            // Rows from src/core/settings.c (the toggles re-light the map when changed)
            DrawSettingsGroup("Rendering & Debug/Lighting", ix, &y);
            y += 22;

            // Ambient minimum
            DrawTextShadow("Ambient Minimum:", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Rendering & Debug/Lighting/Ambient", ix, &y);
            y += 22;

            // Torch defaults
            DrawTextShadow("Torch Defaults:", ix, y, 14, GRAY);
            y += 18;
            DrawSettingsGroup("Rendering & Debug/Lighting/Torch", ix, &y);
            y += 22;

            // Color preview
            {
                Color preview = { (uint8_t)lightDefaultR, (uint8_t)lightDefaultG, (uint8_t)lightDefaultB, 255 };
                DrawRectangle((int)ix, (int)y, 60, 14, preview);
                DrawRectangleLinesEx((Rectangle){ix, y, 60, 14}, 1, GRAY);
                DrawTextShadow("Preview", ix + 65, y, 14, GRAY);
            }
            y += 18;

            CommandButton(ix, y, "Clear Lights", "clear lights");
        }
        y += 22;

        // --- Debug ---
        if (SectionHeader(ix, y, "Debug", &sectionDebug)) {
            y += 18;

            // Rendering
            if (SectionHeader(ix + 10, y, "Rendering", &sectionDebugRendering)) {
                y += 18;
                DrawSettingsGroup("Rendering & Debug/Debug/Rendering", ix + 10, &y);
            }
            y += 22;

            // Pathfinding
            if (SectionHeader(ix + 10, y, "Pathfinding", &sectionDebugPathfinding)) {
                y += 18;
                DrawSettingsGroup("Rendering & Debug/Debug/Pathfinding", ix + 10, &y);
            }
            y += 22;

            // Mover Diagnostics
            if (SectionHeader(ix + 10, y, "Mover Diagnostics", &sectionDebugMovers)) {
                y += 18;
                DrawSettingsGroup("Rendering & Debug/Debug/Mover Diagnostics", ix + 10, &y);
            }
            y += 22;

            // Overlays
            if (SectionHeader(ix + 10, y, "Overlays", &sectionDebugOverlays)) {
                y += 18;
                DrawSettingsGroup("Rendering & Debug/Debug/Overlays", ix + 10, &y);
            }
        }
    }
    y += 22;

}

#if PROFILER_ENABLED
void DrawProfilerPanel(float rightEdge, float y) {
    float panelW = 220;
    float x = rightEdge - panelW;
    Vector2 mouse = GetMousePosition();

    // Block click-through for entire panel area
    float panelH = sectionProfiler ? (300 + (sectionCounters ? profilerCounterCount * 16 + 24 : 24)) : 20;
    if (mouse.x >= x && mouse.x < rightEdge && mouse.y >= y && mouse.y < y + panelH) {
        ui_set_hovered();
    }

    // Draw right-aligned header
    const char* headerText = sectionProfiler ? "[-] Profiler" : "[+] Profiler";
    int headerWidth = MeasureText(headerText, 14);
    float headerX = rightEdge - headerWidth;
    bool hovered = mouse.x >= headerX && mouse.x < headerX + headerWidth + 10 &&
                   mouse.y >= y && mouse.y < y + 18;

    DrawTextShadow(headerText, (int)headerX, (int)y, 14, hovered ? YELLOW : GRAY);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        sectionProfiler = !sectionProfiler;
    }

    if (sectionProfiler) {
        y += 18;

        // Counters section
        if (profilerCounterCount > 0) {
            const char* ctrHeader = sectionCounters ? "[-] Counters" : "[+] Counters";
            int ctrHeaderWidth = MeasureText(ctrHeader, 14);
            float ctrHeaderX = rightEdge - ctrHeaderWidth;
            bool ctrHovered = mouse.x >= ctrHeaderX && mouse.x < ctrHeaderX + ctrHeaderWidth + 10 &&
                              mouse.y >= y && mouse.y < y + 18;
            DrawTextShadow(ctrHeader, (int)ctrHeaderX, (int)y, 14, ctrHovered ? YELLOW : GRAY);
            if (ctrHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                sectionCounters = !sectionCounters;
            }
            y += 18;

            if (sectionCounters) {
                float nameW = 120;
                float valueW = 60;
                float sparkW = 60;
                float sparkH = 12;

                for (int i = 0; i < profilerCounterCount; i++) {
                    ProfileCounter* c = &profilerCounters[i];

                    // Name (left-aligned)
                    DrawTextShadow(c->name, (int)x, (int)y, 12, LIGHTGRAY);

                    // Last value (right-aligned)
                    const char* valStr = TextFormat("%llu", (unsigned long long)ProfileCountGetLast(i));
                    int valW = MeasureText(valStr, 12);
                    DrawTextShadow(valStr, (int)(x + nameW + valueW - valW), (int)y, 12, WHITE);

                    // Mini sparkline
                    float sparkX = x + nameW + valueW + 4;
                    float sparkY = y;

                    // Find local max for scaling
                    uint64_t localMax = 1;
                    for (int f = 0; f < c->historyCount; f++) {
                        if (c->countHistory[f] > localMax) localMax = c->countHistory[f];
                    }

                    // Draw sparkline bars
                    float barW = sparkW / (float)PROFILER_HISTORY_FRAMES;
                    if (barW < 1.0f) barW = 1.0f;

                    for (int f = 0; f < c->historyCount && f < PROFILER_HISTORY_FRAMES; f++) {
                        int idx = (c->historyIndex + f) % PROFILER_HISTORY_FRAMES;
                        float val = (float)c->countHistory[idx] / (float)localMax;
                        float barH = val * sparkH;
                        if (barH < 1.0f && c->countHistory[idx] > 0) barH = 1.0f;

                        float bx = sparkX + f * (sparkW / PROFILER_HISTORY_FRAMES);
                        DrawRectangle((int)bx, (int)(sparkY + sparkH - barH), (int)barW, (int)barH, SKYBLUE);
                    }

                    y += 16;
                }
            }
        }

        // Memory section
        const char* memHeader = sectionMemory ? "[-] Memory" : "[+] Memory";
        int memHeaderWidth = MeasureText(memHeader, 14);
        float memHeaderX = rightEdge - memHeaderWidth;
        bool memHovered = mouse.x >= memHeaderX && mouse.x < memHeaderX + memHeaderWidth + 10 &&
                          mouse.y >= y && mouse.y < y + 18;
        DrawTextShadow(memHeader, (int)memHeaderX, (int)y, 14, memHovered ? YELLOW : GRAY);
        if (memHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            sectionMemory = !sectionMemory;
        }
        y += 18;

        if (sectionMemory) {
            // Calculate sizes of major static arrays
            // Grid & terrain
            size_t gridSize = sizeof(*grid) * gridLayerCells;
            size_t designationsSize = sizeof(Designation) * gridLayerCells;
            size_t waterSize = sizeof(WaterCell) * gridLayerCells;
            size_t fireSize = sizeof(FireCell) * gridLayerCells;
            size_t smokeSize = sizeof(SmokeCell) * gridLayerCells;
            size_t steamSize = sizeof(SteamCell) * gridLayerCells;
            size_t temperatureSize = sizeof(TempCell) * gridLayerCells;
            size_t cellFlagsSize = sizeof(uint8_t) * gridLayerCells;
            size_t groundWearSize = sizeof(*wearGrid) * gridLayerCells;

            // Pathfinding (NavWorld is heap-allocated, sized to the current map)
            size_t navWorldSize = NavMemoryUsage(NavGame());
            size_t pathSize = sizeof(Point) * MAX_PATH;  // Global debug path buffer

            // Entities
            size_t moversSize = sizeof(Mover) * MAX_MOVERS;
            size_t moverPathsSize = sizeof(Point) * MAX_MOVER_PATH * MAX_MOVERS;
            size_t moverRenderSize = sizeof(MoverRenderData) * MAX_MOVERS;
            size_t itemsSize = sizeof(Item) * MAX_ITEMS;
            size_t jobsSize = sizeof(Job) * MAX_JOBS;
            size_t stockpilesSize = sizeof(Stockpile) * MAX_STOCKPILES;
            size_t blueprintsSize = sizeof(Blueprint) * MAX_BLUEPRINTS;
            size_t gatherZonesSize = sizeof(GatherZone) * MAX_GATHER_ZONES;
            size_t workshopsSize = sizeof(Workshop) * MAX_WORKSHOPS;

            // Spatial grids (heap allocated)
            size_t moverSpatialGrid = (moverGrid.cellCount + 1) * sizeof(int) * 2 + MAX_MOVERS * sizeof(int);
            size_t itemSpatialGrid = (itemGrid.cellCount + 1) * sizeof(int) * 2 + MAX_ITEMS * sizeof(int);

            size_t totalGrid = gridSize + designationsSize + waterSize + fireSize + smokeSize + steamSize + temperatureSize + cellFlagsSize + groundWearSize;
            size_t totalPathfinding = navWorldSize + pathSize;
            size_t totalEntities = moversSize + moverPathsSize + moverRenderSize + itemsSize + jobsSize + stockpilesSize + blueprintsSize + gatherZonesSize + workshopsSize;
            size_t totalSpatial = moverSpatialGrid + itemSpatialGrid;
            size_t total = totalGrid + totalPathfinding + totalEntities + totalSpatial;

            // Helper macro for subsection headers
            #define MEM_SUBSECTION(label, sizeVar, boolVar) do { \
                const char* hdr = boolVar ? TextFormat("[-] %s (%.1f MB)", label, (sizeVar) / (1024.0f * 1024.0f)) \
                                          : TextFormat("[+] %s (%.1f MB)", label, (sizeVar) / (1024.0f * 1024.0f)); \
                int hdrW = MeasureText(hdr, 14); \
                float hdrX = rightEdge - hdrW; \
                bool hovered = mouse.x >= hdrX && mouse.x < hdrX + hdrW + 10 && mouse.y >= y && mouse.y < y + 16; \
                DrawTextShadow(hdr, (int)hdrX, (int)y, 14, hovered ? YELLOW : GRAY); \
                if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) boolVar = !boolVar; \
                y += 16; \
            } while(0)

            // Grid Data subsection
            MEM_SUBSECTION("Grid", totalGrid, sectionMemGrid);
            if (sectionMemGrid) {
                DrawTextShadow(TextFormat("  Cells:        %5.1f MB", gridSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Designations: %5.1f MB", designationsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Water:        %5.1f MB", waterSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Fire:         %5.1f MB", fireSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Smoke:        %5.1f MB", smokeSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Steam:        %5.1f MB", steamSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Temperature:  %5.1f MB", temperatureSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  CellFlags:    %5.1f MB", cellFlagsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  GroundWear:   %5.1f MB", groundWearSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            }

            // Pathfinding subsection
            MEM_SUBSECTION("Pathfinding", totalPathfinding, sectionMemPath);
            if (sectionMemPath) {
                DrawTextShadow(TextFormat("  NavWorld:     %5.1f MB", navWorldSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Path:         %5.1f MB", pathSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
            }

            // Entities subsection
            MEM_SUBSECTION("Entities", totalEntities, sectionMemEntities);
            if (sectionMemEntities) {
                DrawTextShadow(TextFormat("  Movers:       %5.1f MB", moversSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  MoverPaths:   %5.1f MB", moverPathsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  MoverRender:  %5.1f KB", moverRenderSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Items:        %5.1f MB", itemsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Jobs:         %5.1f MB", jobsSize / (1024.0f * 1024.0f)), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Workshops:    %5.1f KB", workshopsSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Stockpiles:   %5.1f KB", stockpilesSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  Blueprints:   %5.1f KB", blueprintsSize / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  GatherZones:  %5.1f KB", gatherZonesSize / 1024.0f), x, y, 14, WHITE); y += 16;
            }

            // Spatial subsection
            MEM_SUBSECTION("Spatial", totalSpatial, sectionMemSpatial);
            if (sectionMemSpatial) {
                DrawTextShadow(TextFormat("  MoverGrid:    %5.1f KB", moverSpatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
                DrawTextShadow(TextFormat("  ItemGrid:     %5.1f KB", itemSpatialGrid / 1024.0f), x, y, 14, WHITE); y += 16;
            }

            #undef MEM_SUBSECTION

            DrawTextShadow(TextFormat("TOTAL:          %5.1f MB", total / (1024.0f * 1024.0f)), x, y, 14, PINK); y += 20;
        }

        // Build hierarchical render order (parents before children, children grouped under parent)
        int renderOrder[PROFILER_MAX_SECTIONS];
        int renderCount = 0;

        // Add section and all its descendants recursively (depth-first)
        for (int i = 0; i < profilerSectionCount; i++) {
            if (profilerSections[i].parent == -1) {
                // Use a stack to avoid actual recursion
                int stack[PROFILER_MAX_SECTIONS];
                int stackSize = 1;
                stack[0] = i;

                while (stackSize > 0) {
                    int current = stack[--stackSize];
                    renderOrder[renderCount++] = current;

                    // Push children in reverse order so they come out in forward order
                    for (int j = profilerSectionCount - 1; j >= 0; j--) {
                        if (profilerSections[j].parent == current) {
                            stack[stackSize++] = j;
                        }
                    }
                }
            }
        }

        // Find max value for scaling bars
        float maxMs = 1.0f;  // Minimum scale of 1ms
        for (int i = 0; i < profilerSectionCount; i++) {
            float last = (float)ProfileGetLast(i);
            if (last > maxMs) maxMs = last;
        }

        // Bar graph settings
        int barMaxWidth = 100;
        int labelWidth = 110;
        int indentPerLevel = 12;

        // Colors for each section (shared with line graph)
        Color sectionColors[] = {GREEN, YELLOW, ORANGE, SKYBLUE, PINK, PURPLE, RED, LIME};
        int numColors = sizeof(sectionColors) / sizeof(sectionColors[0]);

        // Check for hover on section labels
        int labelHoveredSection = -1;
        int labelStartY = y;  // Remember starting Y for label hover detection

        int visibleRow = 0;
        for (int r = 0; r < renderCount; r++) {
            int i = renderOrder[r];
            ProfileSection* s = &profilerSections[i];

            // Skip hidden sections (collapsed ancestors)
            if (ProfileIsHidden(i)) continue;

            float last = (float)ProfileGetLast(i);
            float avg = (float)ProfileGetAvg(i);
            Color sectionColor = sectionColors[i % numColors];
            int indent = s->depth * indentPerLevel;
            bool hasChildren = ProfileHasChildren(i);

            // Check if mouse is hovering this label row
            int rowY = labelStartY + visibleRow * 18;
            bool hoveringLabel = (mouse.x >= x && mouse.x < x + labelWidth &&
                                  mouse.y >= rowY && mouse.y < rowY + 18);
            if (hoveringLabel) {
                labelHoveredSection = i;
                // Block click-through when hovering collapsible items
                if (hasChildren) {
                    ui_set_hovered();
                    // Click to toggle collapse
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        s->collapsed = !s->collapsed;
                    }
                }
            }

            // Draw collapse indicator for sections with children
            if (hasChildren) {
                const char* arrow = s->collapsed ? "+" : "-";
                Color arrowColor = s->collapsed ? YELLOW : GRAY;
                DrawTextShadow(arrow, x + indent, y, 14, arrowColor);
            }

            // Draw color indicator square (indented based on depth, dimmed if collapsed)
            Color squareColor = sectionColor;
            if (s->collapsed) squareColor = (Color){sectionColor.r/2, sectionColor.g/2, sectionColor.b/2, 255};
            DrawRectangle(x + indent + (hasChildren ? 10 : 0), y + 3, 10, 10, squareColor);

            // Draw label (highlight if hovered, indented based on depth, show ... if collapsed)
            Color labelColor = hoveringLabel ? sectionColor : (s->collapsed ? GRAY : WHITE);
            const char* displayName = s->collapsed ? TextFormat("%s ...", s->name) : s->name;
            DrawTextShadow(displayName, x + 14 + indent + (hasChildren ? 10 : 0), y, 14, labelColor);

            // Draw bar background
            int barX = x + labelWidth;
            DrawRectangle(barX, y + 2, barMaxWidth, 12, (Color){40, 40, 40, 255});

            // Draw bar (colored by intensity)
            int barWidth = (int)(last / maxMs * barMaxWidth);
            if (barWidth < 1 && last > 0) barWidth = 1;

            // Color: green for low, yellow for medium, red for high (relative to max)
            float ratio = last / maxMs;
            Color barColor;
            if (ratio < 0.3f) {
                barColor = GREEN;
            } else if (ratio < 0.6f) {
                barColor = YELLOW;
            } else {
                barColor = (Color){255, 100, 100, 255};  // Light red
            }
            DrawRectangle(barX, y + 2, barWidth, 12, barColor);

            // Draw avg marker line
            int avgX = barX + (int)(avg / maxMs * barMaxWidth);
            DrawLine(avgX, y + 1, avgX, y + 14, WHITE);

            // Draw value
            DrawTextShadow(TextFormat("%.2f", last), barX + barMaxWidth + 5, y, 14, WHITE);

            y += 18;
            visibleRow++;
        }

        // Line graph showing history
        y += 10;
        int graphW = labelWidth + barMaxWidth;  // Match width of bars above
        int graphX = x + labelWidth + barMaxWidth - graphW;  // Right-align with bars
        int graphY = y;
        int graphH = 60;

        // Find max across all history for scaling
        float graphMax = 1.0f;
        for (int i = 0; i < profilerSectionCount; i++) {
            ProfileSection* s = &profilerSections[i];
            for (int f = 0; f < s->historyCount; f++) {
                if (s->history[f] > graphMax) graphMax = (float)s->history[f];
            }
        }

        // Draw background
        DrawRectangle(graphX, graphY, graphW, graphH, (Color){30, 30, 30, 255});
        DrawRectangleLines(graphX, graphY, graphW, graphH, GRAY);

        // Draw horizontal guide lines
        for (int i = 1; i < 4; i++) {
            int lineY = graphY + (graphH * i / 4);
            DrawLine(graphX, lineY, graphX + graphW, lineY, (Color){50, 50, 50, 255});
        }

        // Check if mouse is in graph area
        bool mouseInGraph = (mouse.x >= graphX && mouse.x < graphX + graphW &&
                             mouse.y >= graphY && mouse.y < graphY + graphH);

        // Find which section is closest to mouse (if hovering on graph)
        // Or use labelHoveredSection if hovering on a label
        int hoveredSection = labelHoveredSection;
        float hoveredValue = 0;
        if (mouseInGraph && labelHoveredSection < 0) {
            // Figure out which frame the mouse is over
            int mouseFrame = (int)((mouse.x - graphX) * PROFILER_HISTORY_FRAMES / graphW);
            if (mouseFrame < 0) mouseFrame = 0;
            if (mouseFrame >= PROFILER_HISTORY_FRAMES) mouseFrame = PROFILER_HISTORY_FRAMES - 1;

            float minDist = 999999.0f;
            for (int i = 0; i < profilerSectionCount; i++) {
                ProfileSection* s = &profilerSections[i];
                if (s->historyCount <= mouseFrame) continue;

                int idx = (s->historyIndex + mouseFrame) % PROFILER_HISTORY_FRAMES;
                float val = (float)s->history[idx];
                int valY = graphY + graphH - (int)(val / graphMax * graphH);
                float dist = fabsf(mouse.y - valY);

                if (dist < minDist && dist < 15) {  // Within 15 pixels
                    minDist = dist;
                    hoveredSection = i;
                    hoveredValue = val;
                }
            }
        }

        // Draw lines for each section
        for (int i = 0; i < profilerSectionCount; i++) {
            ProfileSection* s = &profilerSections[i];
            if (s->historyCount < 2) continue;

            Color col = sectionColors[i % numColors];

            // Dim non-hovered sections when hovering
            if (hoveredSection >= 0 && hoveredSection != i) {
                col.a = 60;
            }

            for (int f = 0; f < s->historyCount - 1; f++) {
                // Read from oldest to newest
                int idx0 = (s->historyIndex + f) % PROFILER_HISTORY_FRAMES;
                int idx1 = (s->historyIndex + f + 1) % PROFILER_HISTORY_FRAMES;

                float v0 = (float)s->history[idx0];
                float v1 = (float)s->history[idx1];

                int x0 = graphX + (f * graphW / PROFILER_HISTORY_FRAMES);
                int x1 = graphX + ((f + 1) * graphW / PROFILER_HISTORY_FRAMES);
                int y0 = graphY + graphH - (int)(v0 / graphMax * graphH);
                int y1 = graphY + graphH - (int)(v1 / graphMax * graphH);

                DrawLine(x0, y0, x1, y1, col);
            }
        }

        // Draw tooltip for hovered section (only when hovering graph, not label)
        if (hoveredSection >= 0 && labelHoveredSection < 0) {
            ProfileSection* s = &profilerSections[hoveredSection];
            const char* tooltip = TextFormat("%s: %.2fms", s->name, hoveredValue);
            int tooltipW = MeasureText(tooltip, 14) + 10;
            int tooltipX = (int)mouse.x + 10;
            int tooltipY = (int)mouse.y - 20;

            // Keep tooltip in screen
            if (tooltipX + tooltipW > graphX + graphW) tooltipX = (int)mouse.x - tooltipW - 5;

            DrawRectangle(tooltipX - 2, tooltipY - 2, tooltipW, 18, (Color){20, 20, 20, 230});
            DrawTextShadow(tooltip, tooltipX, tooltipY, 14, sectionColors[hoveredSection % numColors]);
        }

        // Draw scale label
        DrawTextShadow(TextFormat("%.1fms", graphMax), graphX + graphW + 5, graphY, 12, WHITE);
        DrawTextShadow("0", graphX + graphW + 5, graphY + graphH - 12, 12, WHITE);
    }
}
#endif
