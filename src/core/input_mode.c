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
        case MODE_NORMAL:  return "NORMAL";
        case MODE_DRAW:    return "DRAW";
        case MODE_WORK:    return "WORK";
        case MODE_SANDBOX: return "SANDBOX";
        default:           return "???";
    }
}

static const char* GetActionName(void) {
    switch (inputAction) {
        case ACTION_DRAW_WALL:       return "WALL";
        case ACTION_DRAW_FLOOR:      return "FLOOR";
        case ACTION_DRAW_LADDER:     return "LADDER";
        case ACTION_DRAW_RAMP:       return "RAMP";
        case ACTION_DRAW_STOCKPILE:  return "STOCKPILE";
        case ACTION_DRAW_DIRT:       return "DIRT";
        case ACTION_DRAW_WORKSHOP:   return "WORKSHOP";
        case ACTION_WORK_MINE:       return "DIG";
        case ACTION_WORK_CONSTRUCT:  return "CONSTRUCT";
        case ACTION_WORK_GATHER:     return "GATHER";
        case ACTION_SANDBOX_WATER:   return "WATER";
        case ACTION_SANDBOX_FIRE:    return "FIRE";
        case ACTION_SANDBOX_HEAT:    return "HEAT";
        case ACTION_SANDBOX_COLD:    return "COLD";
        case ACTION_SANDBOX_SMOKE:   return "SMOKE";
        case ACTION_SANDBOX_STEAM:   return "STEAM";
        case ACTION_SANDBOX_GRASS:   return "GRASS";
        default:                     return NULL;
    }
}

// Static buffer for bar text
static char barTextBuffer[256];

const char* InputMode_GetBarText(void) {
    if (inputMode == MODE_NORMAL) {
        return "[D]raw  [W]ork  [S]andbox";
    }
    
    if (inputAction == ACTION_NONE) {
        // In a mode, no action selected - show available actions
        switch (inputMode) {
            case MODE_DRAW:
                return "DRAW: [W]all  [F]loor  [L]adder  [S]tockpile    [ESC]Back";
            case MODE_WORK:
                return "WORK: [M]ine  [C]onstruct  [G]ather    [ESC]Back";
            case MODE_SANDBOX:
                return "SANDBOX: [W]ater  [F]ire  [H]eat  [C]old  s[M]oke  s[T]eam  [G]rass    [ESC]Back";
            default:
                return "[ESC]Back";
        }
    }
    
    // Action selected - show material options or instructions
    const char* actionName = GetActionName();
    const char* modeName = InputMode_GetModeName();
    
    switch (inputAction) {
        case ACTION_DRAW_WALL:
            snprintf(barTextBuffer, sizeof(barTextBuffer), 
                "%s > %s: [1]Stone%s [2]Wood%s    L-drag place  R-drag erase  [ESC]Back",
                modeName, actionName,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "");
            break;
        case ACTION_DRAW_FLOOR:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: [1]Stone%s [2]Wood%s    L-drag place  [ESC]Back",
                modeName, actionName,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "");
            break;
        case ACTION_DRAW_LADDER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag place  [ESC]Back", modeName, actionName);
            break;
        case ACTION_DRAW_DIRT:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag place  R-drag erase  [ESC]Back", modeName, actionName);
            break;
        case ACTION_DRAW_STOCKPILE:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag create  R-drag erase  [ESC]Back", modeName, actionName);
            break;
        case ACTION_WORK_MINE:
        case ACTION_WORK_CONSTRUCT:
        case ACTION_WORK_GATHER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag designate  R-drag cancel  [ESC]Back", modeName, actionName);
            break;
        case ACTION_SANDBOX_WATER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag add  R-drag remove  +Shift=source/drain  [ESC]Back", modeName, actionName);
            break;
        case ACTION_SANDBOX_FIRE:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag ignite  R-drag extinguish  +Shift=source  [ESC]Back", modeName, actionName);
            break;
        case ACTION_SANDBOX_HEAT:
        case ACTION_SANDBOX_COLD:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag add  R-drag remove  [ESC]Back", modeName, actionName);
            break;
        case ACTION_SANDBOX_SMOKE:
        case ACTION_SANDBOX_STEAM:
        case ACTION_SANDBOX_GRASS:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag add  R-drag remove  [ESC]Back", modeName, actionName);
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
        case MODE_DRAW:    return key == KEY_D;
        case MODE_WORK:    return key == KEY_W;
        case MODE_SANDBOX: return key == KEY_S;
        default:           return false;
    }
}

// Helper to add a bar item
// underlinePos: character index to underline (-1 for none)
static int AddItem(BarItem* items, int count, const char* text, int key, int underlinePos, bool isHeader, bool isHint, bool isSelected) {
    if (count >= MAX_BAR_ITEMS) return count;
    strncpy(items[count].text, text, sizeof(items[count].text) - 1);
    items[count].text[sizeof(items[count].text) - 1] = '\0';
    items[count].key = key;
    items[count].underlinePos = underlinePos;
    items[count].isHeader = isHeader;
    items[count].isHint = isHint;
    items[count].isSelected = isSelected;
    items[count].isExit = false;
    return count + 1;
}

// Helper to add a header with exit key
static int AddExitHeader(BarItem* items, int count, const char* text, int key, int underlinePos) {
    if (count >= MAX_BAR_ITEMS) return count;
    strncpy(items[count].text, text, sizeof(items[count].text) - 1);
    items[count].text[sizeof(items[count].text) - 1] = '\0';
    items[count].key = key;
    items[count].underlinePos = underlinePos;
    items[count].isHeader = true;
    items[count].isHint = false;
    items[count].isSelected = false;
    items[count].isExit = true;
    return count + 1;
}

int InputMode_GetBarItems(BarItem* items) {
    int n = 0;
    
    if (inputMode == MODE_NORMAL) {
        n = AddItem(items, n, "Draw", KEY_D, 0, false, false, false);
        n = AddItem(items, n, "Work", KEY_W, 0, false, false, false);
        n = AddItem(items, n, "Sandbox", KEY_S, 0, false, false, false);
        return n;
    }
    
    if (inputAction == ACTION_NONE) {
        // In a mode, no action selected
        switch (inputMode) {
            case MODE_DRAW:
                n = AddExitHeader(items, n, "DRAW:", KEY_D, 0);
                n = AddItem(items, n, "Wall", KEY_W, 0, false, false, false);
                n = AddItem(items, n, "Floor", KEY_F, 0, false, false, false);
                n = AddItem(items, n, "Ladder", KEY_L, 0, false, false, false);
                n = AddItem(items, n, "Ramp", KEY_R, 0, false, false, false);
                n = AddItem(items, n, "Stockpile", KEY_S, 0, false, false, false);
                n = AddItem(items, n, "dIrt", KEY_I, 1, false, false, false);
                n = AddItem(items, n, "sTone workshop", KEY_T, 1, false, false, false);
                n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
                break;
            case MODE_WORK:
                n = AddExitHeader(items, n, "WORK:", KEY_W, 0);
                n = AddItem(items, n, "Dig", KEY_D, 0, false, false, false);
                n = AddItem(items, n, "Construct", KEY_C, 0, false, false, false);
                n = AddItem(items, n, "Gather", KEY_G, 0, false, false, false);
                n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
                break;
            case MODE_SANDBOX:
                n = AddExitHeader(items, n, "SANDBOX:", KEY_S, 0);
                n = AddItem(items, n, "Water", KEY_W, 0, false, false, false);
                n = AddItem(items, n, "Fire", KEY_F, 0, false, false, false);
                n = AddItem(items, n, "Heat", KEY_H, 0, false, false, false);
                n = AddItem(items, n, "Cold", KEY_C, 0, false, false, false);
                n = AddItem(items, n, "sMoke", KEY_M, 1, false, false, false);
                n = AddItem(items, n, "sTeam", KEY_T, 1, false, false, false);
                n = AddItem(items, n, "Grass", KEY_G, 0, false, false, false);
                n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
                break;
            default:
                n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
                break;
        }
        return n;
    }
    
    // Action selected - show mode header and action header separately
    // Mode header (click to exit to normal)
    switch (inputMode) {
        case MODE_DRAW:    n = AddExitHeader(items, n, "DRAW >", KEY_D, 0); break;
        case MODE_WORK:    n = AddExitHeader(items, n, "WORK >", KEY_W, 0); break;
        case MODE_SANDBOX: n = AddExitHeader(items, n, "SANDBOX >", KEY_S, 0); break;
        default: break;
    }
    
    // Action header (click to go back one level)
    int actionKey = 0;
    int actionUnderline = 0;
    switch (inputAction) {
        case ACTION_DRAW_WALL:      actionKey = KEY_W; break;
        case ACTION_DRAW_FLOOR:     actionKey = KEY_F; break;
        case ACTION_DRAW_LADDER:    actionKey = KEY_L; break;
        case ACTION_DRAW_RAMP:      actionKey = KEY_R; break;
        case ACTION_DRAW_STOCKPILE: actionKey = KEY_S; break;
        case ACTION_DRAW_DIRT:      actionKey = KEY_I; actionUnderline = 1; break;
        case ACTION_DRAW_WORKSHOP:  actionKey = KEY_T; actionUnderline = 1; break;
        case ACTION_WORK_MINE:      actionKey = KEY_D; break;
        case ACTION_WORK_CONSTRUCT: actionKey = KEY_C; break;
        case ACTION_WORK_GATHER:    actionKey = KEY_G; break;
        case ACTION_SANDBOX_WATER:  actionKey = KEY_W; break;
        case ACTION_SANDBOX_FIRE:   actionKey = KEY_F; break;
        case ACTION_SANDBOX_HEAT:   actionKey = KEY_H; break;
        case ACTION_SANDBOX_COLD:   actionKey = KEY_C; break;
        case ACTION_SANDBOX_SMOKE:  actionKey = KEY_M; actionUnderline = 1; break;
        case ACTION_SANDBOX_STEAM:  actionKey = KEY_T; actionUnderline = 1; break;
        case ACTION_SANDBOX_GRASS:  actionKey = KEY_G; break;
        default: break;
    }
    const char* actionName = GetActionName();
    char actionHeader[32];
    snprintf(actionHeader, sizeof(actionHeader), "%s:", actionName);
    n = AddItem(items, n, actionHeader, actionKey, actionUnderline, true, false, false);
    
    switch (inputAction) {
        case ACTION_DRAW_WALL:
        case ACTION_DRAW_FLOOR:
            n = AddItem(items, n, "1:Stone", KEY_ONE, 0, false, false, selectedMaterial == 1);
            n = AddItem(items, n, "2:Wood", KEY_TWO, 0, false, false, selectedMaterial == 2);
            n = AddItem(items, n, "L-drag place", 0, -1, false, true, false);
            if (inputAction == ACTION_DRAW_WALL) {
                n = AddItem(items, n, "R-drag erase", 0, -1, false, true, false);
            }
            break;
        case ACTION_DRAW_LADDER:
            n = AddItem(items, n, "L-drag place", 0, -1, false, true, false);
            break;
        case ACTION_DRAW_RAMP:
            n = AddItem(items, n, "Auto", KEY_A, -1, false, false, selectedRampDirection == CELL_AIR);
            n = AddItem(items, n, "N", KEY_UP, -1, false, false, selectedRampDirection == CELL_RAMP_N);
            n = AddItem(items, n, "E", KEY_RIGHT, -1, false, false, selectedRampDirection == CELL_RAMP_E);
            n = AddItem(items, n, "S", KEY_DOWN, -1, false, false, selectedRampDirection == CELL_RAMP_S);
            n = AddItem(items, n, "W", KEY_LEFT, -1, false, false, selectedRampDirection == CELL_RAMP_W);
            n = AddItem(items, n, "L-drag place", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag erase", 0, -1, false, true, false);
            break;
        case ACTION_DRAW_DIRT:
            n = AddItem(items, n, "L-drag place", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag erase", 0, -1, false, true, false);
            break;
        case ACTION_DRAW_STOCKPILE:
            n = AddItem(items, n, "L-drag create", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag erase", 0, -1, false, true, false);
            break;
        case ACTION_DRAW_WORKSHOP:
            n = AddItem(items, n, "L-click place", 0, -1, false, true, false);
            break;
        case ACTION_WORK_MINE:
        case ACTION_WORK_CONSTRUCT:
        case ACTION_WORK_GATHER:
            n = AddItem(items, n, "L-drag designate", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag cancel", 0, -1, false, true, false);
            break;
        case ACTION_SANDBOX_WATER:
            n = AddItem(items, n, "L-drag add", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, -1, false, true, false);
            n = AddItem(items, n, "+Shift=source/drain", 0, -1, false, true, false);
            break;
        case ACTION_SANDBOX_FIRE:
            n = AddItem(items, n, "L-drag ignite", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag extinguish", 0, -1, false, true, false);
            n = AddItem(items, n, "+Shift=source", 0, -1, false, true, false);
            break;
        case ACTION_SANDBOX_HEAT:
        case ACTION_SANDBOX_COLD:
        case ACTION_SANDBOX_SMOKE:
        case ACTION_SANDBOX_STEAM:
        case ACTION_SANDBOX_GRASS:
            n = AddItem(items, n, "L-drag add", 0, -1, false, true, false);
            n = AddItem(items, n, "R-drag remove", 0, -1, false, true, false);
            break;
        default:
            n = AddItem(items, n, "L-drag", 0, -1, false, true, false);
            break;
    }
    
    n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
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
