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
        case MODE_PLACE:     return "PLACE";
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
        case ACTION_DESIGNATE_UNBURN: return "UNBURN";
        case ACTION_ZONE_STOCKPILE:   return "STOCKPILE";
        case ACTION_ZONE_GATHER:      return "GATHER";
        case ACTION_PLACE_WATER:      return "WATER";
        case ACTION_PLACE_FIRE:       return "FIRE";
        case ACTION_PLACE_HEAT:       return "HEAT";
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
        case ACTION_DESIGNATE_UNBURN:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s > %s: L-drag unburn  [ESC]Back", modeName, actionName);
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
        case MODE_PLACE:     return key == KEY_P;
        case MODE_VIEW:      return key == KEY_V;
        default:             return false;
    }
}
