#include "console.h"
#include "../game_state.h"         // raylib, Color, etc.
#include "../../shared/ui.h"       // DrawTextShadow, MeasureTextUI
#include "../entities/items.h"     // SpawnItem, ITEM_TYPE_COUNT
#include "../entities/item_defs.h" // itemDefs[]
#include "../world/cell_defs.h"    // IsCellWalkableAt
#include <string.h>
#include <stdio.h>
#include <stdlib.h>                // atoi, atof
#include <ctype.h>                 // tolower for case-insensitive command matching

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static struct {
    bool open;

    // Scrollback ring buffer
    char lines[CON_MAX_SCROLLBACK][CON_MAX_LINE];
    Color lineColors[CON_MAX_SCROLLBACK];
    int lineHead;       // next write slot
    int lineCount;      // total stored (capped at CON_MAX_SCROLLBACK)
    int scrollOffset;   // 0 = bottom, positive = scrolled up

    // Input line
    char input[CON_MAX_LINE];
    int inputLen;
    int cursor;

    // Command history
    char history[CON_MAX_HISTORY][CON_MAX_LINE];
    int historyCount;   // total stored
    int historyHead;    // next write slot
    int historyIdx;     // browsing index (-1 = not browsing)
    char inputSaved[CON_MAX_LINE]; // saved input when browsing history

    // Cursor blink
    float blinkTimer;
    bool blinkOn;

    // Autocomplete (Phase 5)
    char acMatches[CON_MAX_COMMANDS + CON_MAX_VARS][64];  // matched names
    int acMatchCount;
    int acSelectedIdx;  // -1 = none, 0+ = cycling through matches
    char acPrefix[CON_MAX_LINE];  // what user typed before Tab

    // Saved autocomplete state (for history browsing)
    int acSelectedIdxSaved;
    int acMatchCountSaved;
} con;

// ---------------------------------------------------------------------------
// Command Registry
// ---------------------------------------------------------------------------
typedef struct {
    const char* name;
    ConsoleCmdFn fn;
    const char* help;
} ConsoleCmd;

static ConsoleCmd commands[CON_MAX_COMMANDS];
static int commandCount = 0;

// Forward declarations for built-in commands
static void Cmd_Help(int argc, const char** argv);
static void Cmd_List(int argc, const char** argv);
static void Cmd_Get(int argc, const char** argv);
static void Cmd_Set(int argc, const char** argv);
static void Cmd_Spawn(int argc, const char** argv);
static void Cmd_Clear(int argc, const char** argv);
static void Cmd_Tp(int argc, const char** argv);
static void Cmd_Pause(int argc, const char** argv);

// ---------------------------------------------------------------------------
// Variable Registry
// ---------------------------------------------------------------------------
typedef struct {
    const char* name;
    void* ptr;
    CVarType type;
} CVar;

static CVar cvars[CON_MAX_VARS];
static int cvarCount = 0;

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void Console_Init(void) {
    memset(&con, 0, sizeof(con));
    con.historyIdx = -1;
    con.blinkOn = true;
    con.acSelectedIdx = -1;

    // Register built-in commands
    Console_RegisterCmd("help", Cmd_Help, "Show available commands or help for a specific command");
    Console_RegisterCmd("list", Cmd_List, "List all registered variables");
    Console_RegisterCmd("get", Cmd_Get, "Get variable value: get <varname>");
    Console_RegisterCmd("set", Cmd_Set, "Set variable value: set <varname> <value>");

    // Game commands (Phase 4)
    Console_RegisterCmd("spawn", Cmd_Spawn, "Spawn items at mouse: spawn <count> <item>");
    Console_RegisterCmd("clear", Cmd_Clear, "Clear entities: clear items|movers");
    // Console_RegisterCmd("tp", Cmd_Tp, "Teleport camera: tp <x> <y> <z>");
    // Console_RegisterCmd("pause", Cmd_Pause, "Toggle pause");
}

void Console_RegisterCmd(const char* name, ConsoleCmdFn fn, const char* help) {
    if (commandCount >= CON_MAX_COMMANDS) {
        Console_Printf(RED, "ERROR: Command registry full (max %d)", CON_MAX_COMMANDS);
        return;
    }
    commands[commandCount].name = name;
    commands[commandCount].fn = fn;
    commands[commandCount].help = help;
    commandCount++;
}

void Console_RegisterVar(const char* name, void* ptr, CVarType type) {
    if (cvarCount >= CON_MAX_VARS) {
        Console_Printf(RED, "ERROR: Variable registry full (max %d)", CON_MAX_VARS);
        return;
    }
    cvars[cvarCount].name = name;
    cvars[cvarCount].ptr = ptr;
    cvars[cvarCount].type = type;
    cvarCount++;
}

void Console_Toggle(void) {
    con.open = !con.open;
    if (con.open) {
        con.scrollOffset = 0;
        con.blinkTimer = 0;
        con.blinkOn = true;
    }
}

bool Console_IsOpen(void) {
    return con.open;
}

// ---------------------------------------------------------------------------
// Scrollback
// ---------------------------------------------------------------------------
void Console_Print(const char* text, Color color) {
    // Handle multi-line text (split on newlines)
    const char* start = text;
    while (*start) {
        const char* nl = strchr(start, '\n');
        int len = nl ? (int)(nl - start) : (int)strlen(start);
        if (len >= CON_MAX_LINE) len = CON_MAX_LINE - 1;

        char* slot = con.lines[con.lineHead];
        memcpy(slot, start, len);
        slot[len] = '\0';
        con.lineColors[con.lineHead] = color;

        con.lineHead = (con.lineHead + 1) % CON_MAX_SCROLLBACK;
        if (con.lineCount < CON_MAX_SCROLLBACK) con.lineCount++;

        if (!nl) break;
        start = nl + 1;
    }
}

void Console_Printf(Color color, const char* fmt, ...) {
    char buf[CON_MAX_LINE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Console_Print(buf, color);
}

// ---------------------------------------------------------------------------
// TraceLog callback
// ---------------------------------------------------------------------------
void Console_LogCallback(int logLevel, const char* text, va_list args) {
    char buf[CON_MAX_LINE];
    vsnprintf(buf, sizeof(buf), text, args);

    // Filter out harmless macOS clipboard errors
    if (strstr(buf, "Failed to retrieve string from pasteboard") != NULL) {
        return;
    }

    Color color;
    switch (logLevel) {
        case LOG_ERROR:   color = RED;       break;
        case LOG_WARNING: color = YELLOW;    break;
        case LOG_DEBUG:   color = GRAY;      break;
        default:          color = LIGHTGRAY; break;
    }
    Console_Print(buf, color);

    // Also print to stderr so terminal output still works
    fprintf(stderr, "%s\n", buf);
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------
static void HistoryPush(const char* line) {
    if (line[0] == '\0') return;
    strncpy(con.history[con.historyHead], line, CON_MAX_LINE - 1);
    con.history[con.historyHead][CON_MAX_LINE - 1] = '\0';
    con.historyHead = (con.historyHead + 1) % CON_MAX_HISTORY;
    if (con.historyCount < CON_MAX_HISTORY) con.historyCount++;
    con.historyIdx = -1;
}

static const char* HistoryGet(int idx) {
    // idx 0 = most recent, 1 = previous, etc.
    if (idx < 0 || idx >= con.historyCount) return NULL;
    int slot = (con.historyHead - 1 - idx + CON_MAX_HISTORY) % CON_MAX_HISTORY;
    return con.history[slot];
}

static void HistoryBrowse(int direction) {
    // direction: +1 = older, -1 = newer
    int newIdx = con.historyIdx + direction;

    if (newIdx < -1) newIdx = -1;
    if (newIdx >= con.historyCount) return; // can't go further back

    // If already at current input and trying to go newer, do nothing
    if (con.historyIdx == -1 && newIdx == -1) return;

    if (con.historyIdx == -1 && direction > 0) {
        // Save current input AND autocomplete state before browsing
        strncpy(con.inputSaved, con.input, CON_MAX_LINE);
        con.acSelectedIdxSaved = con.acSelectedIdx;
        con.acMatchCountSaved = con.acMatchCount;
    }

    con.historyIdx = newIdx;

    if (con.historyIdx == -1) {
        // Returned to current input - restore everything
        strncpy(con.input, con.inputSaved, CON_MAX_LINE);
        con.inputLen = (int)strlen(con.input);
        con.acSelectedIdx = con.acSelectedIdxSaved;
        con.acMatchCount = con.acMatchCountSaved;

        // Re-show autocomplete context if we were cycling
        if (con.acSelectedIdx >= 0 && con.acMatchCount > 1) {
            int windowSize = 3;
            int startIdx = con.acSelectedIdx - windowSize;
            int endIdx = con.acSelectedIdx + windowSize;
            if (startIdx < 0) startIdx = 0;
            if (endIdx >= con.acMatchCount) endIdx = con.acMatchCount - 1;

            Console_Print("Matches:", GRAY);
            if (startIdx > 0) {
                Console_Print("  ...", DARKGRAY);
            }
            for (int i = startIdx; i <= endIdx; i++) {
                if (i == con.acSelectedIdx) {
                    Console_Printf(GREEN, "  %s  <--", con.acMatches[i]);
                } else {
                    Console_Printf(SKYBLUE, "  %s", con.acMatches[i]);
                }
            }
            if (endIdx < con.acMatchCount - 1) {
                Console_Print("  ...", DARKGRAY);
            }
            Console_Printf(GRAY, "  (%d of %d)", con.acSelectedIdx + 1, con.acMatchCount);
        }
    } else {
        const char* hist = HistoryGet(con.historyIdx);
        if (hist) {
            strncpy(con.input, hist, CON_MAX_LINE - 1);
            con.input[CON_MAX_LINE - 1] = '\0';
            con.inputLen = (int)strlen(con.input);
        }
        con.acSelectedIdx = -1;  // Reset autocomplete when viewing history
    }
    con.cursor = con.inputLen;
}

// ---------------------------------------------------------------------------
// Command Parsing & Execution
// ---------------------------------------------------------------------------
#define MAX_ARGS 16

static int ParseCommand(const char* input, const char* argv[MAX_ARGS]) {
    static char buf[CON_MAX_LINE];
    strncpy(buf, input, CON_MAX_LINE - 1);
    buf[CON_MAX_LINE - 1] = '\0';

    int argc = 0;
    char* p = buf;

    while (*p && argc < MAX_ARGS) {
        // Skip leading whitespace
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p) break;

        // Handle quoted strings
        if (*p == '"') {
            p++; // skip opening quote
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') {
                *p = '\0';
                p++;
            }
        } else {
            // Regular token
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) {
                *p = '\0';
                p++;
            }
        }
    }

    return argc;
}

static void ExecuteCommand(const char* input) {
    const char* argv[MAX_ARGS];
    int argc = ParseCommand(input, argv);

    if (argc == 0) return;

    // Find command (case-insensitive)
    const char* cmdName = argv[0];
    for (int i = 0; i < commandCount; i++) {
        // Simple case-insensitive compare
        const char* a = cmdName;
        const char* b = commands[i].name;
        bool match = true;
        while (*a && *b) {
            if (tolower(*a) != tolower(*b)) {
                match = false;
                break;
            }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            // Found it - execute
            commands[i].fn(argc, argv);
            return;
        }
    }

    // Command not found
    Console_Printf(YELLOW, "Unknown command: %s (type 'help' for list)", cmdName);
}

// ---------------------------------------------------------------------------
// Built-in Commands
// ---------------------------------------------------------------------------
static void Cmd_Help(int argc, const char** argv) {
    if (argc == 1) {
        // List all commands
        Console_Print("Available commands:", LIGHTGRAY);
        for (int i = 0; i < commandCount; i++) {
            Console_Printf(SKYBLUE, "  %s", commands[i].name);
            Console_Printf(LIGHTGRAY, "    %s", commands[i].help);
        }
    } else {
        // Show help for specific command
        const char* cmdName = argv[1];
        for (int i = 0; i < commandCount; i++) {
            const char* a = cmdName;
            const char* b = commands[i].name;
            bool match = true;
            while (*a && *b) {
                if (tolower(*a) != tolower(*b)) {
                    match = false;
                    break;
                }
                a++; b++;
            }
            if (match && *a == '\0' && *b == '\0') {
                Console_Printf(SKYBLUE, "%s", commands[i].name);
                Console_Printf(LIGHTGRAY, "  %s", commands[i].help);
                return;
            }
        }
        Console_Printf(YELLOW, "Unknown command: %s", cmdName);
    }
}

static void Cmd_List(int argc, const char** argv) {
    (void)argc; (void)argv; // unused
    if (cvarCount == 0) {
        Console_Print("No variables registered", GRAY);
        return;
    }

    Console_Printf(LIGHTGRAY, "Registered variables (%d):", cvarCount);
    for (int i = 0; i < cvarCount; i++) {
        const char* name = cvars[i].name;
        void* ptr = cvars[i].ptr;

        switch (cvars[i].type) {
            case CVAR_BOOL:
                Console_Printf(SKYBLUE, "  %s = %s", name, *(bool*)ptr ? "true" : "false");
                break;
            case CVAR_INT:
                Console_Printf(SKYBLUE, "  %s = %d", name, *(int*)ptr);
                break;
            case CVAR_FLOAT:
                Console_Printf(SKYBLUE, "  %s = %.3f", name, *(float*)ptr);
                break;
        }
    }
}

static void Cmd_Get(int argc, const char** argv) {
    if (argc < 2) {
        Console_Print("Usage: get <varname>", YELLOW);
        return;
    }

    const char* varName = argv[1];

    // Find variable (case-insensitive)
    for (int i = 0; i < cvarCount; i++) {
        const char* a = varName;
        const char* b = cvars[i].name;
        bool match = true;
        while (*a && *b) {
            if (tolower(*a) != tolower(*b)) {
                match = false;
                break;
            }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            // Found it
            void* ptr = cvars[i].ptr;
            switch (cvars[i].type) {
                case CVAR_BOOL:
                    Console_Printf(GREEN, "%s = %s", cvars[i].name, *(bool*)ptr ? "true" : "false");
                    break;
                case CVAR_INT:
                    Console_Printf(GREEN, "%s = %d", cvars[i].name, *(int*)ptr);
                    break;
                case CVAR_FLOAT:
                    Console_Printf(GREEN, "%s = %.3f", cvars[i].name, *(float*)ptr);
                    break;
            }
            return;
        }
    }

    Console_Printf(YELLOW, "Unknown variable: %s", varName);
}

static void Cmd_Set(int argc, const char** argv) {
    if (argc < 3) {
        Console_Print("Usage: set <varname> <value>", YELLOW);
        return;
    }

    const char* varName = argv[1];
    const char* valueStr = argv[2];

    // Find variable (case-insensitive)
    for (int i = 0; i < cvarCount; i++) {
        const char* a = varName;
        const char* b = cvars[i].name;
        bool match = true;
        while (*a && *b) {
            if (tolower(*a) != tolower(*b)) {
                match = false;
                break;
            }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            // Found it - parse and set value
            void* ptr = cvars[i].ptr;
            switch (cvars[i].type) {
                case CVAR_BOOL: {
                    bool val;
                    if (strcmp(valueStr, "true") == 0 || strcmp(valueStr, "1") == 0) {
                        val = true;
                    } else if (strcmp(valueStr, "false") == 0 || strcmp(valueStr, "0") == 0) {
                        val = false;
                    } else {
                        Console_Printf(YELLOW, "Invalid bool value (use true/false or 1/0)");
                        return;
                    }
                    *(bool*)ptr = val;
                    Console_Printf(GREEN, "%s = %s", cvars[i].name, val ? "true" : "false");
                    break;
                }
                case CVAR_INT: {
                    int val = atoi(valueStr);
                    *(int*)ptr = val;
                    Console_Printf(GREEN, "%s = %d", cvars[i].name, val);
                    break;
                }
                case CVAR_FLOAT: {
                    float val = (float)atof(valueStr);
                    *(float*)ptr = val;
                    Console_Printf(GREEN, "%s = %.3f", cvars[i].name, val);
                    break;
                }
            }
            return;
        }
    }

    Console_Printf(YELLOW, "Unknown variable: %s", varName);
}

// ---------------------------------------------------------------------------
// Game Commands (Phase 4)
// ---------------------------------------------------------------------------
static void Cmd_Spawn(int argc, const char** argv) {
    if (argc < 3) {
        Console_Print("Usage: spawn <count> <item>", YELLOW);
        Console_Print("Example: spawn 10 rock", GRAY);
        return;
    }

    int count = atoi(argv[1]);
    if (count <= 0 || count > 100) {
        Console_Print("Count must be 1-100", YELLOW);
        return;
    }

    // Find item by name (case-insensitive substring match)
    const char* itemName = argv[2];
    ItemType foundType = ITEM_NONE;

    for (int i = 0; i < ITEM_TYPE_COUNT; i++) {
        const char* defName = itemDefs[i].name;
        // Case-insensitive substring search
        bool match = true;
        const char* p = defName;
        const char* q = itemName;

        while (*p && *q) {
            if (tolower(*p) != tolower(*q)) {
                match = false;
                break;
            }
            p++; q++;
        }

        if (match && *q == '\0') {
            foundType = (ItemType)i;
            break;
        }
    }

    if (foundType == ITEM_NONE) {
        Console_Printf(YELLOW, "Unknown item: %s", itemName);
        return;
    }

    // Get mouse world position
    Vector2 mousePos = GetMousePosition();
    Vector2 gridPos = ScreenToGrid(mousePos);
    int mx = (int)gridPos.x;
    int my = (int)gridPos.y;
    int mz = currentViewZ;

    // Check bounds
    if (mx < 0 || mx >= gridWidth || my < 0 || my >= gridHeight || mz < 0 || mz >= gridDepth) {
        Console_Print("Mouse position out of bounds", YELLOW);
        return;
    }

    // Spawn items at/near mouse position
    int spawned = 0;
    uint8_t mat = DefaultMaterialForItemType(foundType);

    for (int i = 0; i < count; i++) {
        // Try to find a walkable cell near the mouse
        int bestX = mx, bestY = my, bestZ = mz;
        float bestDist = 9999.0f;

        // Search in a small radius
        for (int dz = 0; dz >= -1 && dz + mz >= 0; dz--) {
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int tx = mx + dx;
                    int ty = my + dy;
                    int tz = mz + dz;

                    if (tx < 0 || tx >= gridWidth || ty < 0 || ty >= gridHeight || tz < 0 || tz >= gridDepth)
                        continue;

                    if (IsCellWalkableAt(tz, ty, tx)) {
                        float dist = (float)(dx*dx + dy*dy + dz*dz);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestX = tx;
                            bestY = ty;
                            bestZ = tz;
                        }
                    }
                }
            }
            if (bestDist < 9999.0f) break; // Found a spot on this z-level
        }

        if (bestDist < 9999.0f) {
            float wx = bestX * CELL_SIZE + CELL_SIZE * 0.5f;
            float wy = bestY * CELL_SIZE + CELL_SIZE * 0.5f;
            float wz = (float)bestZ;
            SpawnItemWithMaterial(wx, wy, wz, foundType, mat);
            spawned++;
        }
    }

    Console_Printf(GREEN, "Spawned %d %s at mouse position", spawned, itemDefs[foundType].name);
}

static void Cmd_Clear(int argc, const char** argv) {
    if (argc < 2) {
        Console_Print("Usage: clear <items|movers>", YELLOW);
        return;
    }

    const char* what = argv[1];

    if (strcmp(what, "items") == 0) {
        ClearItems();
        Console_Print("Cleared all items", GREEN);
    } else if (strcmp(what, "movers") == 0) {
        ClearMovers();
        Console_Print("Cleared all movers", GREEN);
    } else {
        Console_Printf(YELLOW, "Unknown type: %s (use items or movers)", what);
    }
}

static void Cmd_Tp(int argc, const char** argv) {
    if (argc < 4) {
        Console_Print("Usage: tp <x> <y> <z>", YELLOW);
        Console_Print("Example: tp 100 50 2", GRAY);
        return;
    }

    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    int z = atoi(argv[3]);

    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        Console_Print("Coordinates out of bounds", YELLOW);
        return;
    }

    // Center camera on the target cell
    offset.x = GetScreenWidth() / 2.0f - x * CELL_SIZE * zoom;
    offset.y = GetScreenHeight() / 2.0f - y * CELL_SIZE * zoom;
    currentViewZ = z;

    Console_Printf(GREEN, "Teleported to (%d, %d, %d)", x, y, z);
}

static void Cmd_Pause(int argc, const char** argv) {
    (void)argc; (void)argv; // unused
    paused = !paused;
    Console_Printf(GREEN, "Pause: %s", paused ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Autocomplete
// ---------------------------------------------------------------------------
typedef enum { AC_COMMANDS, AC_VARIABLES, AC_ITEMS, AC_BOTH } AutocompleteMode;

// Case-insensitive substring search
static bool ContainsSubstring(const char* haystack, const char* needle) {
    int needleLen = (int)strlen(needle);
    int haystackLen = (int)strlen(haystack);

    for (int i = 0; i <= haystackLen - needleLen; i++) {
        bool match = true;
        for (int j = 0; j < needleLen; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static void FindMatches(const char* prefix, AutocompleteMode mode) {
    con.acMatchCount = 0;
    int prefixLen = (int)strlen(prefix);
    // Allow empty prefix for items (show all items)
    if (prefixLen == 0 && mode != AC_ITEMS) return;

    // Match commands (prefix first, then substring)
    if (mode == AC_COMMANDS || mode == AC_BOTH) {
        // First pass: prefix matches
        for (int i = 0; i < commandCount && con.acMatchCount < (CON_MAX_COMMANDS + CON_MAX_VARS); i++) {
            if (strncmp(commands[i].name, prefix, prefixLen) == 0) {
                strncpy(con.acMatches[con.acMatchCount], commands[i].name, 63);
                con.acMatches[con.acMatchCount][63] = '\0';
                con.acMatchCount++;
            }
        }
        // Second pass: substring matches (skip if already matched)
        if (prefixLen > 0) {
            for (int i = 0; i < commandCount && con.acMatchCount < (CON_MAX_COMMANDS + CON_MAX_VARS); i++) {
                if (strncmp(commands[i].name, prefix, prefixLen) != 0 &&
                    ContainsSubstring(commands[i].name, prefix)) {
                    strncpy(con.acMatches[con.acMatchCount], commands[i].name, 63);
                    con.acMatches[con.acMatchCount][63] = '\0';
                    con.acMatchCount++;
                }
            }
        }
    }

    // Match variables (prefix first, then substring)
    if (mode == AC_VARIABLES || mode == AC_BOTH) {
        // First pass: prefix matches
        for (int i = 0; i < cvarCount && con.acMatchCount < (CON_MAX_COMMANDS + CON_MAX_VARS); i++) {
            if (strncmp(cvars[i].name, prefix, prefixLen) == 0) {
                strncpy(con.acMatches[con.acMatchCount], cvars[i].name, 63);
                con.acMatches[con.acMatchCount][63] = '\0';
                con.acMatchCount++;
            }
        }
        // Second pass: substring matches (skip if already matched)
        if (prefixLen > 0) {
            for (int i = 0; i < cvarCount && con.acMatchCount < (CON_MAX_COMMANDS + CON_MAX_VARS); i++) {
                if (strncmp(cvars[i].name, prefix, prefixLen) != 0 &&
                    ContainsSubstring(cvars[i].name, prefix)) {
                    strncpy(con.acMatches[con.acMatchCount], cvars[i].name, 63);
                    con.acMatches[con.acMatchCount][63] = '\0';
                    con.acMatchCount++;
                }
            }
        }
    }

    // Match item names (prefix first, then substring)
    if (mode == AC_ITEMS) {
        // First pass: prefix matches
        for (int i = 0; i < ITEM_TYPE_COUNT && con.acMatchCount < (CON_MAX_COMMANDS + CON_MAX_VARS); i++) {
            const char* itemName = itemDefs[i].name;
            bool match = true;
            for (int j = 0; j < prefixLen; j++) {
                if (j >= (int)strlen(itemName) || tolower(prefix[j]) != tolower(itemName[j])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                strncpy(con.acMatches[con.acMatchCount], itemName, 63);
                con.acMatches[con.acMatchCount][63] = '\0';
                con.acMatchCount++;
            }
        }
        // Second pass: substring matches (skip if already matched as prefix)
        if (prefixLen > 0) {
            for (int i = 0; i < ITEM_TYPE_COUNT && con.acMatchCount < (CON_MAX_COMMANDS + CON_MAX_VARS); i++) {
                const char* itemName = itemDefs[i].name;
                // Check if it's NOT a prefix match
                bool isPrefix = true;
                for (int j = 0; j < prefixLen; j++) {
                    if (j >= (int)strlen(itemName) || tolower(prefix[j]) != tolower(itemName[j])) {
                        isPrefix = false;
                        break;
                    }
                }
                if (!isPrefix && ContainsSubstring(itemName, prefix)) {
                    strncpy(con.acMatches[con.acMatchCount], itemName, 63);
                    con.acMatches[con.acMatchCount][63] = '\0';
                    con.acMatchCount++;
                }
            }
        }
    }
}

static void HandleTab(void) {
    // If we're in the middle of text, don't autocomplete
    if (con.cursor < con.inputLen) return;

    // Determine what to autocomplete based on context
    AutocompleteMode mode = AC_BOTH;
    const char* searchPrefix = con.input;
    int prefixStart = 0;

    // Check if we're typing after "get " or "set "
    if (strncmp(con.input, "get ", 4) == 0 && con.inputLen > 4) {
        mode = AC_VARIABLES;
        searchPrefix = con.input + 4;
        prefixStart = 4;
    } else if (strncmp(con.input, "set ", 4) == 0 && con.inputLen > 4) {
        mode = AC_VARIABLES;
        searchPrefix = con.input + 4;
        prefixStart = 4;
    } else if (strncmp(con.input, "spawn ", 6) == 0 && con.inputLen > 6) {
        // After "spawn ", skip the count argument and autocomplete item names
        const char* afterSpawn = con.input + 6;
        // Find the space after the count
        const char* spaceAfterCount = strchr(afterSpawn, ' ');
        if (spaceAfterCount && (spaceAfterCount - con.input) < con.inputLen) {
            // We're past the count, autocomplete items
            mode = AC_ITEMS;
            searchPrefix = spaceAfterCount + 1;
            prefixStart = (int)(spaceAfterCount - con.input) + 1;
        }
    }

    // First Tab press - find matches and save prefix
    if (con.acSelectedIdx == -1) {
        strncpy(con.acPrefix, searchPrefix, CON_MAX_LINE - 1);
        con.acPrefix[CON_MAX_LINE - 1] = '\0';
        FindMatches(con.acPrefix, mode);

        if (con.acMatchCount == 0) {
            // No matches
            return;
        } else if (con.acMatchCount == 1) {
            // Single match - autocomplete it
            if (prefixStart > 0) {
                // We're completing after a command (e.g., "set ")
                strncpy(con.input + prefixStart, con.acMatches[0], CON_MAX_LINE - prefixStart - 1);
                con.input[CON_MAX_LINE - 1] = '\0';
            } else {
                // We're completing the first word
                strncpy(con.input, con.acMatches[0], CON_MAX_LINE - 1);
                con.input[CON_MAX_LINE - 1] = '\0';
            }
            con.inputLen = (int)strlen(con.input);
            con.cursor = con.inputLen;
            con.acSelectedIdx = -1;  // Reset
        } else {
            // Multiple matches - autocomplete UI will display them (don't print to scrollback)

            // Find common prefix
            int commonLen = (int)strlen(con.acMatches[0]);
            for (int i = 1; i < con.acMatchCount; i++) {
                int j = 0;
                while (j < commonLen && con.acMatches[0][j] == con.acMatches[i][j]) {
                    j++;
                }
                commonLen = j;
            }

            // Complete to common prefix
            if (commonLen > (int)strlen(con.acPrefix)) {
                if (prefixStart > 0) {
                    // Preserve command part
                    strncpy(con.input + prefixStart, con.acMatches[0], commonLen);
                    con.input[prefixStart + commonLen] = '\0';
                } else {
                    strncpy(con.input, con.acMatches[0], commonLen);
                    con.input[commonLen] = '\0';
                }
                con.inputLen = (int)strlen(con.input);
                con.cursor = con.inputLen;
            }

            // Start cycling
            con.acSelectedIdx = 0;
        }
    } else {
        // Subsequent Tab - cycle (autocomplete UI will update display)
        con.acSelectedIdx = (con.acSelectedIdx + 1) % con.acMatchCount;

        if (prefixStart > 0) {
            // Preserve command part
            strncpy(con.input + prefixStart, con.acMatches[con.acSelectedIdx], CON_MAX_LINE - prefixStart - 1);
            con.input[CON_MAX_LINE - 1] = '\0';
        } else {
            strncpy(con.input, con.acMatches[con.acSelectedIdx], CON_MAX_LINE - 1);
            con.input[CON_MAX_LINE - 1] = '\0';
        }
        con.inputLen = (int)strlen(con.input);
        con.cursor = con.inputLen;
    }

    con.blinkOn = true;
    con.blinkTimer = 0;
}

// ---------------------------------------------------------------------------
// Submit
// ---------------------------------------------------------------------------
static void Submit(void) {
    if (con.inputLen == 0) return;

    // Echo to scrollback (in cyan to show it's a command)
    char echo[CON_MAX_LINE + 4];
    snprintf(echo, sizeof(echo), "> %s", con.input);
    Console_Print(echo, SKYBLUE);

    // Push to history
    HistoryPush(con.input);

    // Execute command
    ExecuteCommand(con.input);

    // Clear input
    con.input[0] = '\0';
    con.inputLen = 0;
    con.cursor = 0;
    con.scrollOffset = 0;
    con.acSelectedIdx = -1;  // Reset autocomplete
}

// ---------------------------------------------------------------------------
// Register Game Variables (call from main.c after init)
// ---------------------------------------------------------------------------
void Console_RegisterGameVars(void) {
    // Rendering & Display (from main.c)
    Console_RegisterVar("zoom", &zoom, CVAR_FLOAT);
    Console_RegisterVar("currentViewZ", &currentViewZ, CVAR_INT);
    Console_RegisterVar("showGraph", &showGraph, CVAR_BOOL);
    Console_RegisterVar("showEntrances", &showEntrances, CVAR_BOOL);
    Console_RegisterVar("showChunkBoundaries", &showChunkBoundaries, CVAR_BOOL);
    Console_RegisterVar("showMovers", &showMovers, CVAR_BOOL);
    Console_RegisterVar("showMoverPaths", &showMoverPaths, CVAR_BOOL);
    Console_RegisterVar("showJobLines", &showJobLines, CVAR_BOOL);
    Console_RegisterVar("showNeighborCounts", &showNeighborCounts, CVAR_BOOL);
    Console_RegisterVar("showOpenArea", &showOpenArea, CVAR_BOOL);
    Console_RegisterVar("showKnotDetection", &showKnotDetection, CVAR_BOOL);
    Console_RegisterVar("showStuckDetection", &showStuckDetection, CVAR_BOOL);
    Console_RegisterVar("showItems", &showItems, CVAR_BOOL);
    Console_RegisterVar("showSimSources", &showSimSources, CVAR_BOOL);
    Console_RegisterVar("cullDrawing", &cullDrawing, CVAR_BOOL);
    Console_RegisterVar("usePixelPerfectMovers", &usePixelPerfectMovers, CVAR_BOOL);

    // Pathfinding & Movement (from mover.c)
    Console_RegisterVar("useStringPulling", &useStringPulling, CVAR_BOOL);
    Console_RegisterVar("endlessMoverMode", &endlessMoverMode, CVAR_BOOL);
    Console_RegisterVar("useMoverAvoidance", &useMoverAvoidance, CVAR_BOOL);
    Console_RegisterVar("preferDifferentZ", &preferDifferentZ, CVAR_BOOL);
    Console_RegisterVar("allowFallingFromAvoidance", &allowFallingFromAvoidance, CVAR_BOOL);
    Console_RegisterVar("useKnotFix", &useKnotFix, CVAR_BOOL);
    Console_RegisterVar("useWallRepulsion", &useWallRepulsion, CVAR_BOOL);
    Console_RegisterVar("useWallSliding", &useWallSliding, CVAR_BOOL);
    Console_RegisterVar("useDirectionalAvoidance", &useDirectionalAvoidance, CVAR_BOOL);
    Console_RegisterVar("avoidStrengthOpen", &avoidStrengthOpen, CVAR_FLOAT);
    Console_RegisterVar("avoidStrengthClosed", &avoidStrengthClosed, CVAR_FLOAT);
    Console_RegisterVar("wallRepulsionStrength", &wallRepulsionStrength, CVAR_FLOAT);

    // Time (from time.c)
    Console_RegisterVar("useFixedTimestep", &useFixedTimestep, CVAR_BOOL);
}

// ---------------------------------------------------------------------------
// Update (call when console is open)
// ---------------------------------------------------------------------------
void Console_Update(void) {
    if (!con.open) return;

    // Blink cursor
    con.blinkTimer += GetFrameTime();
    if (con.blinkTimer >= 0.5f) {
        con.blinkTimer -= 0.5f;
        con.blinkOn = !con.blinkOn;
    }

    // Character input
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        // Ignore backtick (would re-toggle console)
        if (ch == '`' || ch == '~') continue;
        if (ch < 32 || ch > 126) continue; // printable ASCII only

        if (con.inputLen < CON_MAX_LINE - 1) {
            // Insert at cursor
            memmove(&con.input[con.cursor + 1], &con.input[con.cursor],
                    con.inputLen - con.cursor + 1);
            con.input[con.cursor] = (char)ch;
            con.inputLen++;
            con.cursor++;
            con.blinkOn = true;
            con.blinkTimer = 0;
            con.acSelectedIdx = -1;  // Reset autocomplete on typing
        }
    }

    // Tab - autocomplete
    if (IsKeyPressed(KEY_TAB)) {
        HandleTab();
    }

    // Special keys
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (con.cursor > 0) {
            memmove(&con.input[con.cursor - 1], &con.input[con.cursor],
                    con.inputLen - con.cursor + 1);
            con.cursor--;
            con.inputLen--;
            con.blinkOn = true;
            con.blinkTimer = 0;
            con.acSelectedIdx = -1;  // Reset autocomplete
        }
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (con.cursor < con.inputLen) {
            memmove(&con.input[con.cursor], &con.input[con.cursor + 1],
                    con.inputLen - con.cursor);
            con.inputLen--;
            con.acSelectedIdx = -1;  // Reset autocomplete
        }
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        if (con.cursor > 0) {
            con.cursor--;
            con.blinkOn = true;
            con.blinkTimer = 0;
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        if (con.cursor < con.inputLen) {
            con.cursor++;
            con.blinkOn = true;
            con.blinkTimer = 0;
        }
    }

    if (IsKeyPressed(KEY_HOME)) {
        con.cursor = 0;
        con.blinkOn = true;
        con.blinkTimer = 0;
    }

    if (IsKeyPressed(KEY_END)) {
        con.cursor = con.inputLen;
        con.blinkOn = true;
        con.blinkTimer = 0;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        Submit();
    }

    if (IsKeyPressed(KEY_UP)) {
        HistoryBrowse(+1);
    }

    if (IsKeyPressed(KEY_DOWN)) {
        HistoryBrowse(-1);
    }

    // Paste (Ctrl+V or Cmd+V on Mac)
    bool ctrlOrCmd = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    #ifdef __APPLE__
    ctrlOrCmd = ctrlOrCmd || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    #endif

    if (ctrlOrCmd && IsKeyPressed(KEY_V)) {
        const char* clipText = GetClipboardText();
        if (clipText && clipText[0] != '\0') {
            int clipLen = (int)strlen(clipText);
            int spaceLeft = (CON_MAX_LINE - 1) - con.inputLen;
            if (clipLen > spaceLeft) clipLen = spaceLeft;

            if (clipLen > 0) {
                // Make room at cursor
                memmove(&con.input[con.cursor + clipLen], &con.input[con.cursor],
                        con.inputLen - con.cursor + 1);
                // Insert clipboard text
                memcpy(&con.input[con.cursor], clipText, clipLen);
                con.cursor += clipLen;
                con.inputLen += clipLen;
                con.blinkOn = true;
                con.blinkTimer = 0;
            }
        }
    }

    // Scroll
    int wheel = (int)GetMouseWheelMove();
    if (wheel != 0) {
        con.scrollOffset += wheel;
        if (con.scrollOffset < 0) con.scrollOffset = 0;
        int maxScroll = con.lineCount > 0 ? con.lineCount - 1 : 0;
        if (con.scrollOffset > maxScroll) con.scrollOffset = maxScroll;
    }

    // ESC closes console
    if (IsKeyPressed(KEY_ESCAPE)) {
        con.open = false;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void Console_Draw(void) {
    if (!con.open) return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int consoleH = screenH * 2 / 5;  // 40% of screen
    int fontSize = 16;
    int lineH = fontSize + 2;
    int padding = 8;

    // Background
    DrawRectangle(0, 0, screenW, consoleH, (Color){0, 0, 0, 200});
    DrawRectangle(0, consoleH, screenW, 2, (Color){100, 100, 100, 255}); // separator

    // Input line
    int inputY = consoleH - lineH - padding;
    DrawTextShadow(">", padding, inputY, fontSize, GREEN);
    int promptW = MeasureTextUI("> ", fontSize);

    // Draw input text
    if (con.inputLen > 0) {
        DrawTextShadow(con.input, padding + promptW, inputY, fontSize, WHITE);
    }

    // Draw autocomplete preview (grey hint of what Tab will complete)
    if (con.inputLen > 0 && con.cursor == con.inputLen) {
        char preview[CON_MAX_LINE] = {0};
        const char* searchPrefix = con.input;
        int searchPrefixLen = con.inputLen;
        bool varsOnly = false;
        bool itemsOnly = false;

        // Check if we're typing after "get " or "set "
        if (strncmp(con.input, "get ", 4) == 0 && con.inputLen > 4) {
            searchPrefix = con.input + 4;
            searchPrefixLen = con.inputLen - 4;
            varsOnly = true;
        } else if (strncmp(con.input, "set ", 4) == 0 && con.inputLen > 4) {
            searchPrefix = con.input + 4;
            searchPrefixLen = con.inputLen - 4;
            varsOnly = true;
        } else if (strncmp(con.input, "spawn ", 6) == 0 && con.inputLen > 6) {
            // After "spawn ", skip the count argument
            const char* afterSpawn = con.input + 6;
            const char* spaceAfterCount = strchr(afterSpawn, ' ');
            if (spaceAfterCount && (spaceAfterCount - con.input) < con.inputLen) {
                searchPrefix = spaceAfterCount + 1;
                searchPrefixLen = con.inputLen - (int)(spaceAfterCount - con.input) - 1;
                itemsOnly = true;
            }
        }

        // Check items (if after spawn)
        if (itemsOnly) {
            for (int i = 0; i < ITEM_TYPE_COUNT; i++) {
                const char* itemName = itemDefs[i].name;
                // Case-insensitive prefix match
                bool match = true;
                for (int j = 0; j < searchPrefixLen; j++) {
                    if (j >= (int)strlen(itemName) || tolower(searchPrefix[j]) != tolower(itemName[j])) {
                        match = false;
                        break;
                    }
                }
                if (match && (int)strlen(itemName) > searchPrefixLen) {
                    strncpy(preview, itemName + searchPrefixLen, CON_MAX_LINE - 1);
                    break;
                }
            }
        }
        // Check commands (unless we're after get/set/spawn)
        else if (!varsOnly) {
            for (int i = 0; i < commandCount; i++) {
                if (strncmp(commands[i].name, searchPrefix, searchPrefixLen) == 0 &&
                    strlen(commands[i].name) > (size_t)searchPrefixLen) {
                    strncpy(preview, commands[i].name + searchPrefixLen, CON_MAX_LINE - 1);
                    break;
                }
            }
        }

        // Check variables (if not after spawn)
        if (preview[0] == '\0' && !itemsOnly) {
            for (int i = 0; i < cvarCount; i++) {
                if (strncmp(cvars[i].name, searchPrefix, searchPrefixLen) == 0 &&
                    strlen(cvars[i].name) > (size_t)searchPrefixLen) {
                    strncpy(preview, cvars[i].name + searchPrefixLen, CON_MAX_LINE - 1);
                    break;
                }
            }
        }

        // Draw preview in grey
        if (preview[0] != '\0') {
            int inputW = MeasureTextUI(con.input, fontSize);
            DrawTextShadow(preview, padding + promptW + inputW, inputY, fontSize, DARKGRAY);
        }
    }

    // Draw cursor
    if (con.blinkOn) {
        // Measure text up to cursor position
        char tmp[CON_MAX_LINE];
        strncpy(tmp, con.input, con.cursor);
        tmp[con.cursor] = '\0';
        int cursorX = padding + promptW + MeasureTextUI(tmp, fontSize);
        DrawRectangle(cursorX, inputY, 2, fontSize, GREEN);
    }

    // Draw autocomplete matches (separate floating area, not in scrollback)
    if (con.acSelectedIdx >= 0 && con.acMatchCount > 0) {
        int acY = inputY - lineH;  // Start above input line
        int windowSize = 3;
        int startIdx = con.acSelectedIdx - windowSize;
        int endIdx = con.acSelectedIdx + windowSize;
        if (startIdx < 0) startIdx = 0;
        if (endIdx >= con.acMatchCount) endIdx = con.acMatchCount - 1;

        // Position on right side of console
        int acWidth = 300;  // Fixed width for autocomplete box
        int acX = screenW - acWidth - padding;  // Right-aligned with padding
        int acHeight = (endIdx - startIdx + 3) * lineH;  // +3 for header and counter

        // Draw dark background for autocomplete area
        DrawRectangle(acX, acY - acHeight, acWidth, acHeight, (Color){20, 20, 20, 240});

        // Draw matches header
        DrawTextShadow("Matches:", acX + padding, acY - acHeight + padding, fontSize, GRAY);
        int currentY = acY - acHeight + padding + lineH;

        // Draw matches with context window
        if (startIdx > 0) {
            DrawTextShadow("  ...", acX + padding, currentY, fontSize - 2, DARKGRAY);
            currentY += lineH;
        }
        for (int i = startIdx; i <= endIdx; i++) {
            if (i == con.acSelectedIdx) {
                DrawTextShadow(TextFormat("  %s  <--", con.acMatches[i]), acX + padding, currentY, fontSize, GREEN);
            } else {
                DrawTextShadow(TextFormat("  %s", con.acMatches[i]), acX + padding, currentY, fontSize, SKYBLUE);
            }
            currentY += lineH;
        }
        if (endIdx < con.acMatchCount - 1) {
            DrawTextShadow("  ...", acX + padding, currentY, fontSize - 2, DARKGRAY);
            currentY += lineH;
        }
        DrawTextShadow(TextFormat("  (%d of %d)", con.acSelectedIdx + 1, con.acMatchCount),
                       acX + padding, currentY, fontSize - 2, GRAY);
    }

    // Scrollback lines (draw bottom-up from above input line)
    int y = inputY - lineH;
    int visibleLines = (y - padding) / lineH;

    for (int i = 0; i < visibleLines && i + con.scrollOffset < con.lineCount; i++) {
        int lineIdx = (con.lineHead - 1 - i - con.scrollOffset + CON_MAX_SCROLLBACK) % CON_MAX_SCROLLBACK;
        DrawTextShadow(con.lines[lineIdx], padding, y, fontSize, con.lineColors[lineIdx]);
        y -= lineH;
        if (y < padding) break;
    }

    // Scroll indicator (at top of console)
    if (con.scrollOffset > 0) {
        const char* indicator = TextFormat("-- scrolled %d --", con.scrollOffset);
        int tw = MeasureTextUI(indicator, fontSize);
        DrawTextShadow(indicator, (screenW - tw) / 2, padding + 2, fontSize - 2, YELLOW);
    }
}
