// core/input_mode.c - Hierarchical input mode system
#include "input_mode.h"
#include "../game_state.h"  // Includes raylib.h
#include <stdio.h>

// State
InputMode inputMode = MODE_NORMAL;
InputAction inputAction = ACTION_NONE;
int selectedMaterial = 1;

bool isDragging = false;
int dragStartX = 0;
int dragStartY = 0;

bool quickEditEnabled = true;  // Left/right click draws/erases walls in normal mode

bool showQuitConfirm = false;
bool shouldQuit = false;

void InputMode_Reset(void) {
    inputMode = MODE_NORMAL;
    inputAction = ACTION_NONE;
    selectedMaterial = 1;
    isDragging = false;
}

void InputMode_Back(void) {
    if (isDragging) return;  // Don't back out while dragging
    
    if (inputAction != ACTION_NONE) {
        inputAction = ACTION_NONE;
        selectedMaterial = 1;
    } else if (inputMode != MODE_NORMAL) {
        inputMode = MODE_NORMAL;
    }
}

void InputMode_ExitToNormal(void) {
    if (isDragging) return;
    inputMode = MODE_NORMAL;
    inputAction = ACTION_NONE;
    selectedMaterial = 1;
}

const char* InputMode_GetModeName(void) {
    switch (inputMode) {
        case MODE_NORMAL:    return "NORMAL";
        case MODE_BUILD:     return "BUILD";
        case MODE_DESIGNATE: return "DESIGNATE";
        case MODE_ZONES:     return "ZONES";
        case MODE_PLACE:     return "SANDBOX";
        case MODE_VIEW:      return "VIEW";
        default:             return "???";
    }
}

static const char* GetActionName(void) {
    switch (inputAction) {
        case ACTION_BUILD_WALL:       return "WALL";
        case ACTION_BUILD_FLOOR:      return "FLOOR";
        case ACTION_BUILD_LADDER:     return "LADDER";
        case ACTION_BUILD_ROOM:       return "ROOM";
        case ACTION_DESIGNATE_MINE:   return "MINE";
        case ACTION_DESIGNATE_BUILD:  return "BUILD";
        case ACTION_ZONE_STOCKPILE:   return "STOCKPILE";
        case ACTION_ZONE_GATHER:      return "GATHER";
        case ACTION_PLACE_WATER:      return "WATER";
        case ACTION_PLACE_FIRE:       return "FIRE";
        case ACTION_PLACE_HEAT:       return "HEAT";
        case ACTION_PLACE_COLD:       return "COLD";
        case ACTION_PLACE_SMOKE:      return "SMOKE";
        case ACTION_PLACE_STEAM:      return "STEAM";
        case ACTION_PLACE_GRASS:      return "GRASS";
        default:                      return NULL;
    }
}

// Static buffer for bar text
static char barTextBuffer[256];

const char* InputMode_GetBarText(void) {
    if (inputMode == MODE_NORMAL) {
        return "[B]uild  [D]esignate  [Z]ones  [P]lace";
    }
    
    if (inputAction == ACTION_NONE) {
        // In a mode, no action selected - show available actions
        switch (inputMode) {
            case MODE_BUILD:
                return "BUILD: [W]all  [F]loor  [L]adder  [R]oom                    [ESC]Back";
            case MODE_DESIGNATE:
                return "DESIGNATE: [M]ine  [B]uild  [U]nburn                        [ESC]Back";
            case MODE_ZONES:
                return "ZONES: [S]tockpile  [G]ather                                [ESC]Back";
            case MODE_PLACE:
                return "PLACE: [W]ater  [F]ire  [H]eat                              [ESC]Back";
            case MODE_VIEW:
                return "VIEW: [T]emperature  [W]ater  [O]zones                      [ESC]Back";
            default:
                return "[ESC]Back";
        }
    }
    
    // Action selected - show material options or instructions
    const char* actionName = GetActionName();
    const char* modeName = InputMode_GetModeName();
    
    switch (inputAction) {
        case ACTION_BUILD_WALL:
            snprintf(barTextBuffer, sizeof(barTextBuffer), 
                "%s > %s: [1]Stone%s [2]Wood%s    L-drag place  R-drag erase  [ESC]Back",
                modeName, actionName,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "");
            break;
        case ACTION_BUILD_FLOOR:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: [1]Stone%s [2]Wood%s    L-drag place  [ESC]Back",
                modeName, actionName,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "");
            break;
        case ACTION_BUILD_LADDER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag place  [ESC]Back", modeName, actionName);
            break;
        case ACTION_BUILD_ROOM:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: [1]Stone%s [2]Wood%s    L-drag place  [ESC]Back",
                modeName, actionName,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "");
            break;
        case ACTION_DESIGNATE_MINE:
        case ACTION_DESIGNATE_BUILD:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag designate  R-drag cancel  [ESC]Back", modeName, actionName);
            break;
        case ACTION_ZONE_STOCKPILE:
        case ACTION_ZONE_GATHER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag create  R-drag erase  [ESC]Back", modeName, actionName);
            break;
        case ACTION_PLACE_WATER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag add  R-drag remove  +Shift=source/drain  [ESC]Back", modeName, actionName);
            break;
        case ACTION_PLACE_FIRE:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag ignite  R-drag extinguish  +Shift=source  [ESC]Back", modeName, actionName);
            break;
        case ACTION_PLACE_HEAT:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag heat  R-drag cold  +Shift=remove  [ESC]Back", modeName, actionName);
            break;
        default:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag  [ESC]Back", modeName, actionName ? actionName : "???");
            break;
    }
    
    return barTextBuffer;
}

bool InputMode_IsReservedKey(int key) {
    // Check if this key is the mode's entry key (reserved for exit)
    switch (inputMode) {
        case MODE_BUILD:     return key == KEY_B;
        case MODE_DESIGNATE: return key == KEY_D;
        case MODE_ZONES:     return key == KEY_Z;
        case MODE_PLACE:     return key == KEY_S;
        case MODE_VIEW:      return key == KEY_V;
        default:             return false;
    }
}

// Helper to add a bar item
static int AddItem(BarItem* items, int count, const char* text, int key, bool isHeader, bool isHint, bool isSelected) {
    if (count >= MAX_BAR_ITEMS) return count;
    strncpy(items[count].text, text, sizeof(items[count].text) - 1);
    items[count].text[sizeof(items[count].text) - 1] = '\0';
    items[count].key = key;
    items[count].isHeader = isHeader;
    items[count].isHint = isHint;
    items[count].isSelected = isSelected;
    items[count].isExit = false;
    return count + 1;
}

// Helper to add a header with exit key
static int AddExitHeader(BarItem* items, int count, const char* text, int key) {
    if (count >= MAX_BAR_ITEMS) return count;
    strncpy(items[count].text, text, sizeof(items[count].text) - 1);
    items[count].text[sizeof(items[count].text) - 1] = '\0';
    items[count].key = key;
    items[count].isHeader = true;
    items[count].isHint = false;
    items[count].isSelected = false;
    items[count].isExit = true;
    return count + 1;
}

int InputMode_GetBarItems(BarItem* items) {
    int n = 0;
    
    if (inputMode == MODE_NORMAL) {
        n = AddItem(items, n, "[B]uild", KEY_B, false, false, false);
        n = AddItem(items, n, "[D]esignate", KEY_D, false, false, false);
        n = AddItem(items, n, "[Z]ones", KEY_Z, false, false, false);
        n = AddItem(items, n, "[S]andbox", KEY_S, false, false, false);
        return n;
    }
    
    if (inputAction == ACTION_NONE) {
        // In a mode, no action selected
        switch (inputMode) {
            case MODE_BUILD:
                n = AddExitHeader(items, n, "[B]UILD:", KEY_B);
                n = AddItem(items, n, "[W]all", KEY_W, false, false, false);
                n = AddItem(items, n, "[F]loor", KEY_F, false, false, false);
                n = AddItem(items, n, "[L]adder", KEY_L, false, false, false);
                n = AddItem(items, n, "[R]oom", KEY_R, false, false, false);
                n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
                break;
            case MODE_DESIGNATE:
                n = AddExitHeader(items, n, "[D]ESIGNATE:", KEY_D);
                n = AddItem(items, n, "[M]ine", KEY_M, false, false, false);
                n = AddItem(items, n, "[B]uild", KEY_B, false, false, false);
                n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
                break;
            case MODE_ZONES:
                n = AddExitHeader(items, n, "[Z]ONES:", KEY_Z);
                n = AddItem(items, n, "[S]tockpile", KEY_S, false, false, false);
                n = AddItem(items, n, "[G]ather", KEY_G, false, false, false);
                n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
                break;
            case MODE_PLACE:
                n = AddExitHeader(items, n, "[S]ANDBOX:", KEY_S);
                n = AddItem(items, n, "[W]ater", KEY_W, false, false, false);
                n = AddItem(items, n, "[F]ire", KEY_F, false, false, false);
                n = AddItem(items, n, "[H]eat", KEY_H, false, false, false);
                n = AddItem(items, n, "[C]old", KEY_C, false, false, false);
                n = AddItem(items, n, "s[M]oke", KEY_M, false, false, false);
                n = AddItem(items, n, "s[T]eam", KEY_T, false, false, false);
                n = AddItem(items, n, "[G]rass", KEY_G, false, false, false);
                n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
                break;
            case MODE_VIEW:
                n = AddExitHeader(items, n, "[V]IEW:", KEY_V);
                n = AddItem(items, n, "[T]emperature", KEY_T, false, false, false);
                n = AddItem(items, n, "[W]ater", KEY_W, false, false, false);
                n = AddItem(items, n, "[O]zones", KEY_O, false, false, false);
                n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
                break;
            default:
                n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
                break;
        }
        return n;
    }
    
    // Action selected - show materials/hints
    const char* actionName = GetActionName();
    char header[64];
    int exitKey = 0;
    switch (inputMode) {
        case MODE_BUILD:     snprintf(header, sizeof(header), "[B]UILD > %s:", actionName); exitKey = KEY_B; break;
        case MODE_DESIGNATE: snprintf(header, sizeof(header), "[D]ESIGNATE > %s:", actionName); exitKey = KEY_D; break;
        case MODE_ZONES:     snprintf(header, sizeof(header), "[Z]ONES > %s:", actionName); exitKey = KEY_Z; break;
        case MODE_PLACE:     snprintf(header, sizeof(header), "[S]ANDBOX > %s:", actionName); exitKey = KEY_S; break;
        case MODE_VIEW:      snprintf(header, sizeof(header), "[V]IEW > %s:", actionName); exitKey = KEY_V; break;
        default:             snprintf(header, sizeof(header), "%s:", actionName); break;
    }
    n = AddExitHeader(items, n, header, exitKey);
    
    switch (inputAction) {
        case ACTION_BUILD_WALL:
        case ACTION_BUILD_FLOOR:
        case ACTION_BUILD_ROOM:
            n = AddItem(items, n, "[1]Stone", KEY_ONE, false, false, selectedMaterial == 1);
            n = AddItem(items, n, "[2]Wood", KEY_TWO, false, false, selectedMaterial == 2);
            n = AddItem(items, n, "L-drag place", 0, false, true, false);
            if (inputAction == ACTION_BUILD_WALL) {
                n = AddItem(items, n, "R-drag erase", 0, false, true, false);
            }
            break;
        case ACTION_BUILD_LADDER:
            n = AddItem(items, n, "L-drag place", 0, false, true, false);
            break;
        case ACTION_DESIGNATE_MINE:
        case ACTION_DESIGNATE_BUILD:
            n = AddItem(items, n, "L-drag designate", 0, false, true, false);
            n = AddItem(items, n, "R-drag cancel", 0, false, true, false);
            break;
        case ACTION_ZONE_STOCKPILE:
        case ACTION_ZONE_GATHER:
            n = AddItem(items, n, "L-drag create", 0, false, true, false);
            n = AddItem(items, n, "R-drag erase", 0, false, true, false);
            break;
        case ACTION_PLACE_WATER:
            n = AddItem(items, n, "L-drag add", 0, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, false, true, false);
            n = AddItem(items, n, "+Shift=source/drain", 0, false, true, false);
            break;
        case ACTION_PLACE_FIRE:
            n = AddItem(items, n, "L-drag ignite", 0, false, true, false);
            n = AddItem(items, n, "R-drag extinguish", 0, false, true, false);
            n = AddItem(items, n, "+Shift=source", 0, false, true, false);
            break;
        case ACTION_PLACE_HEAT:
            n = AddItem(items, n, "L-drag add", 0, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, false, true, false);
            break;
        case ACTION_PLACE_COLD:
            n = AddItem(items, n, "L-drag add", 0, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, false, true, false);
            break;
        case ACTION_PLACE_SMOKE:
            n = AddItem(items, n, "L-drag add", 0, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, false, true, false);
            break;
        case ACTION_PLACE_STEAM:
            n = AddItem(items, n, "L-drag add", 0, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, false, true, false);
            break;
        case ACTION_PLACE_GRASS:
            n = AddItem(items, n, "L-drag grow", 0, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, false, true, false);
            break;
        default:
            n = AddItem(items, n, "L-drag", 0, false, true, false);
            break;
    }
    
    n = AddItem(items, n, "[ESC]Back", KEY_ESCAPE, false, false, false);
    return n;
}

// Pending key to be processed by HandleInput
static int pendingKey = 0;

void InputMode_TriggerKey(int key) {
    pendingKey = key;
}

int InputMode_GetPendingKey(void) {
    int key = pendingKey;
    pendingKey = 0;
    return key;
}
