// core/input_mode.c - Hierarchical input mode system
#include "input_mode.h"
#include "../game_state.h"  // Includes raylib.h
#include <stdio.h>

// State
InputMode inputMode = MODE_NORMAL;
WorkSubMode workSubMode = SUBMODE_NONE;
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
    workSubMode = SUBMODE_NONE;
    inputAction = ACTION_NONE;
    selectedMaterial = 1;
    isDragging = false;
}

void InputMode_Back(void) {
    if (isDragging) return;  // Don't back out while dragging
    
    if (inputAction != ACTION_NONE) {
        inputAction = ACTION_NONE;
        selectedMaterial = 1;
    } else if (workSubMode != SUBMODE_NONE) {
        workSubMode = SUBMODE_NONE;
    } else if (inputMode != MODE_NORMAL) {
        inputMode = MODE_NORMAL;
    }
}

void InputMode_ExitToNormal(void) {
    if (isDragging) return;
    inputMode = MODE_NORMAL;
    workSubMode = SUBMODE_NONE;
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
        case ACTION_WORK_MINE:         return "MINE";
        case ACTION_WORK_CHANNEL:      return "CHANNEL";
        case ACTION_WORK_DIG_RAMP:     return "DIG RAMP";
        case ACTION_WORK_REMOVE_FLOOR: return "REMOVE FLOOR";
        case ACTION_WORK_REMOVE_RAMP:  return "REMOVE RAMP";
        case ACTION_WORK_CONSTRUCT:    return "WALL";
        case ACTION_WORK_FLOOR:        return "FLOOR";
        case ACTION_WORK_LADDER:       return "LADDER";
        case ACTION_WORK_RAMP:         return "RAMP";
        case ACTION_WORK_CHOP:         return "CHOP TREE";
        case ACTION_WORK_CHOP_FELLED:  return "CHOP FELLED";
        case ACTION_WORK_GATHER_SAPLING: return "GATHER SAPLING";
        case ACTION_WORK_PLANT_SAPLING:  return "PLANT SAPLING";
        case ACTION_WORK_GATHER:       return "GATHER";
        case ACTION_SANDBOX_WATER:   return "WATER";
        case ACTION_SANDBOX_FIRE:    return "FIRE";
        case ACTION_SANDBOX_HEAT:    return "HEAT";
        case ACTION_SANDBOX_COLD:    return "COLD";
        case ACTION_SANDBOX_SMOKE:   return "SMOKE";
        case ACTION_SANDBOX_STEAM:   return "STEAM";
        case ACTION_SANDBOX_GRASS:   return "GRASS";
        case ACTION_SANDBOX_TREE:    return "TREE";
        default:                     return NULL;
    }
}

static const char* GetSubModeName(void) {
    switch (workSubMode) {
        case SUBMODE_DIG:     return "DIG";
        case SUBMODE_BUILD:   return "BUILD";
        case SUBMODE_HARVEST: return "HARVEST";
        default:              return NULL;
    }
}

// Static buffer for bar text
static char barTextBuffer[256];

const char* InputMode_GetBarText(void) {
    if (inputMode == MODE_NORMAL) {
        return "[D]raw  [W]ork  [S]andbox";
    }
    
    if (inputAction == ACTION_NONE) {
        // In a mode, no action selected - show available actions/submodes
        switch (inputMode) {
            case MODE_DRAW:
                return "DRAW: [W]all  [F]loor  [L]adder  [S]tockpile    [ESC]Back";
            case MODE_WORK:
                if (workSubMode == SUBMODE_NONE) {
                    return "WORK: [D]ig  [B]uild  [H]arvest  [G]ather    [ESC]Back";
                }
                switch (workSubMode) {
                    case SUBMODE_DIG:
                        return "WORK > DIG: [M]ine  c[H]annel  dig [R]amp  remove [F]loor  remove ramp[Z]    [ESC]Back";
                    case SUBMODE_BUILD:
                        return "WORK > BUILD: [W]all  [F]loor  [L]adder  [R]amp    [ESC]Back";
                    case SUBMODE_HARVEST:
                        return "WORK > HARVEST: [C]hop tree  chop [F]elled  gather [S]apling  [P]lant sapling    [ESC]Back";
                    default:
                        return "[ESC]Back";
                }
            case MODE_SANDBOX:
                return "SANDBOX: [W]ater  [F]ire  [H]eat  [C]old  s[M]oke  s[T]eam  [G]rass  t[R]ee    [ESC]Back";
            default:
                return "[ESC]Back";
        }
    }
    
    // Action selected - show material options or instructions
    const char* actionName = GetActionName();
    const char* modeName = InputMode_GetModeName();
    const char* subModeName = GetSubModeName();
    
    // Build prefix: "MODE > SUBMODE > ACTION:" or "MODE > ACTION:"
    char prefix[64];
    if (subModeName) {
        snprintf(prefix, sizeof(prefix), "%s > %s > %s", modeName, subModeName, actionName);
    } else {
        snprintf(prefix, sizeof(prefix), "%s > %s", modeName, actionName);
    }
    
    switch (inputAction) {
        case ACTION_DRAW_WALL:
            snprintf(barTextBuffer, sizeof(barTextBuffer), 
                "%s: [1]Stone%s [2]Wood%s [3]Dirt%s    L-drag place  R-drag erase  [ESC]Back",
                prefix,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "",
                selectedMaterial == 3 ? "<" : "");
            break;
        case ACTION_DRAW_FLOOR:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: [1]Stone%s [2]Wood%s [3]Dirt%s    L-drag place  [ESC]Back",
                prefix,
                selectedMaterial == 1 ? "<" : "",
                selectedMaterial == 2 ? "<" : "",
                selectedMaterial == 3 ? "<" : "");
            break;
        case ACTION_DRAW_LADDER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag place  [ESC]Back", prefix);
            break;
        case ACTION_DRAW_DIRT:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag place  R-drag erase  [ESC]Back", prefix);
            break;
        case ACTION_DRAW_STOCKPILE:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag create  R-drag erase  [ESC]Back", prefix);
            break;
        case ACTION_WORK_MINE:
        case ACTION_WORK_CHANNEL:
        case ACTION_WORK_DIG_RAMP:
        case ACTION_WORK_REMOVE_FLOOR:
        case ACTION_WORK_REMOVE_RAMP:
        case ACTION_WORK_CONSTRUCT:
        case ACTION_WORK_FLOOR:
        case ACTION_WORK_LADDER:
        case ACTION_WORK_RAMP:
        case ACTION_WORK_CHOP:
        case ACTION_WORK_CHOP_FELLED:
        case ACTION_WORK_GATHER_SAPLING:
        case ACTION_WORK_PLANT_SAPLING:
        case ACTION_WORK_GATHER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag designate  R-drag cancel  [ESC]Back", prefix);
            break;
        case ACTION_SANDBOX_WATER:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag add  R-drag remove  +Shift=source/drain  [ESC]Back", prefix);
            break;
        case ACTION_SANDBOX_FIRE:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag ignite  R-drag extinguish  +Shift=source  [ESC]Back", prefix);
            break;
        case ACTION_SANDBOX_HEAT:
        case ACTION_SANDBOX_COLD:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag add  R-drag remove  [ESC]Back", prefix);
            break;
        case ACTION_SANDBOX_SMOKE:
        case ACTION_SANDBOX_STEAM:
        case ACTION_SANDBOX_GRASS:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag add  R-drag remove  [ESC]Back", prefix);
            break;
        case ACTION_SANDBOX_TREE:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-click place  R-drag remove  [ESC]Back", prefix);
            break;
        default:
            snprintf(barTextBuffer, sizeof(barTextBuffer),
                "%s: L-drag  [ESC]Back", prefix);
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
                if (workSubMode == SUBMODE_NONE) {
                    // Top-level WORK menu
                    n = AddExitHeader(items, n, "WORK:", KEY_W, 0);
                    n = AddItem(items, n, "Dig", KEY_D, 0, false, false, false);
                    n = AddItem(items, n, "Build", KEY_B, 0, false, false, false);
                    n = AddItem(items, n, "Harvest", KEY_H, 0, false, false, false);
                    n = AddItem(items, n, "Gather", KEY_G, 0, false, false, false);
                    n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
                } else {
                    // Sub-mode selected
                    n = AddExitHeader(items, n, "WORK >", KEY_W, 0);
                    switch (workSubMode) {
                        case SUBMODE_DIG:
                            n = AddItem(items, n, "DIG:", KEY_D, 0, true, false, false);
                            n = AddItem(items, n, "Mine", KEY_M, 0, false, false, false);
                            n = AddItem(items, n, "cHannel", KEY_H, 1, false, false, false);
                            n = AddItem(items, n, "dig Ramp", KEY_R, 4, false, false, false);
                            n = AddItem(items, n, "remove Floor", KEY_F, 7, false, false, false);
                            n = AddItem(items, n, "remove ramp(Z)", KEY_Z, 12, false, false, false);
                            break;
                        case SUBMODE_BUILD:
                            n = AddItem(items, n, "BUILD:", KEY_B, 0, true, false, false);
                            n = AddItem(items, n, "Wall", KEY_W, 0, false, false, false);
                            n = AddItem(items, n, "Floor", KEY_F, 0, false, false, false);
                            n = AddItem(items, n, "Ladder", KEY_L, 0, false, false, false);
                            n = AddItem(items, n, "Ramp", KEY_R, 0, false, false, false);
                            break;
                        case SUBMODE_HARVEST:
                            n = AddItem(items, n, "HARVEST:", KEY_H, 0, true, false, false);
                            n = AddItem(items, n, "Chop tree", KEY_C, 0, false, false, false);
                            n = AddItem(items, n, "Chop felled", KEY_F, 0, false, false, false);
                            n = AddItem(items, n, "gather Sapling", KEY_S, 7, false, false, false);
                            n = AddItem(items, n, "Plant sapling", KEY_P, 0, false, false, false);
                            break;
                        default:
                            break;
                    }
                    n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
                }
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
                n = AddItem(items, n, "tRee", KEY_R, 1, false, false, false);
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
    
    // Submode header for WORK mode (click to go back to submode selection)
    if (workSubMode != SUBMODE_NONE) {
        int subKey = 0;
        const char* subName = NULL;
        switch (workSubMode) {
            case SUBMODE_DIG:     subKey = KEY_D; subName = "DIG >"; break;
            case SUBMODE_BUILD:   subKey = KEY_B; subName = "BUILD >"; break;
            case SUBMODE_HARVEST: subKey = KEY_H; subName = "HARVEST >"; break;
            default: break;
        }
        if (subName) {
            n = AddItem(items, n, subName, subKey, 0, true, false, false);
        }
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
        // Dig actions
        case ACTION_WORK_MINE:         actionKey = KEY_M; break;
        case ACTION_WORK_CHANNEL:      actionKey = KEY_H; actionUnderline = 1; break;
        case ACTION_WORK_DIG_RAMP:     actionKey = KEY_R; break;
        case ACTION_WORK_REMOVE_FLOOR: actionKey = KEY_F; break;
        case ACTION_WORK_REMOVE_RAMP:  actionKey = KEY_Z; break;
        // Build actions
        case ACTION_WORK_CONSTRUCT:    actionKey = KEY_W; break;
        case ACTION_WORK_FLOOR:        actionKey = KEY_F; break;
        case ACTION_WORK_LADDER:       actionKey = KEY_L; break;
        case ACTION_WORK_RAMP:         actionKey = KEY_R; break;
        // Harvest actions
        case ACTION_WORK_CHOP:         actionKey = KEY_C; break;
        case ACTION_WORK_CHOP_FELLED:  actionKey = KEY_F; break;
        case ACTION_WORK_GATHER_SAPLING: actionKey = KEY_S; break;
        case ACTION_WORK_PLANT_SAPLING:  actionKey = KEY_P; break;
        // Gather (top-level)
        case ACTION_WORK_GATHER:       actionKey = KEY_G; break;
        // Sandbox actions
        case ACTION_SANDBOX_WATER:  actionKey = KEY_W; break;
        case ACTION_SANDBOX_FIRE:   actionKey = KEY_F; break;
        case ACTION_SANDBOX_HEAT:   actionKey = KEY_H; break;
        case ACTION_SANDBOX_COLD:   actionKey = KEY_C; break;
        case ACTION_SANDBOX_SMOKE:  actionKey = KEY_M; actionUnderline = 1; break;
        case ACTION_SANDBOX_STEAM:  actionKey = KEY_T; actionUnderline = 1; break;
        case ACTION_SANDBOX_GRASS:  actionKey = KEY_G; break;
        case ACTION_SANDBOX_TREE:   actionKey = KEY_R; actionUnderline = 1; break;
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
        case ACTION_WORK_CHANNEL:
        case ACTION_WORK_REMOVE_FLOOR:
        case ACTION_WORK_REMOVE_RAMP:
        case ACTION_WORK_CONSTRUCT:
        case ACTION_WORK_LADDER:
        case ACTION_WORK_FLOOR:
        case ACTION_WORK_GATHER:
        case ACTION_WORK_CHOP:
        case ACTION_WORK_CHOP_FELLED:
        case ACTION_WORK_GATHER_SAPLING:
        case ACTION_WORK_PLANT_SAPLING:
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
        case ACTION_SANDBOX_TREE:
            n = AddItem(items, n, "L-click place", 0, -1, false, true, false);
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
