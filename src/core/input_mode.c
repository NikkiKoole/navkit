// core/input_mode.c - Hierarchical input mode system
#include "input_mode.h"
#include "action_registry.h"
#include "../game_state.h"  // Includes raylib.h
#include "../simulation/lighting.h"
#include <stdio.h>

// State
InputMode inputMode = MODE_NORMAL;
WorkSubMode workSubMode = SUBMODE_NONE;
InputAction inputAction = ACTION_NONE;
int selectedMaterial = 1;

// Terrain brush state
int terrainBrushRadius = 1;  // Default 3x3
int lastBrushX = -1;
int lastBrushY = -1;
bool brushStrokeActive = false;

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
    linkingWorkshopIdx = -1;  // Reset linking state
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
    const ActionDef* def = GetActionDef(inputAction);
    return def->name;
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
                return "DRAW: [W]all  [F]loor  [L]adder  [R]amp  [S]tockpile  s[O]il  workshop([T])    [ESC]Back";
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
                        return "WORK > HARVEST: [C]hop tree  chop [F]elled  gather [G]rass  gather [S]apling  gather [T]ree  [P]lant sapling    [ESC]Back";
                    default:
                        return "[ESC]Back";
                }
            case MODE_SANDBOX:
                return "SANDBOX: [W]ater  [F]ire  [H]eat  c[O]ld  s[M]oke  s[T]eam  [G]rass  t[R]ee  s[C]ulpt    [ESC]Back";
            default:
                return "[ESC]Back";
        }
    }
    
    // Action selected - use registry for bar text
    const ActionDef* def = GetActionDef(inputAction);
    const char* actionName = def->name;
    const char* modeName = InputMode_GetModeName();
    const char* subModeName = GetSubModeName();
    
    // Build prefix: "MODE > SUBMODE > ACTION:" or "MODE > ACTION:"
    char prefix[64];
    if (subModeName) {
        snprintf(prefix, sizeof(prefix), "%s > %s > %s", modeName, subModeName, actionName);
    } else {
        snprintf(prefix, sizeof(prefix), "%s > %s", modeName, actionName);
    }
    
    // Special handling for actions with dynamic content
    if (inputAction == ACTION_DRAW_WALL || inputAction == ACTION_DRAW_FLOOR) {
        // Material selection with markers
        snprintf(barTextBuffer, sizeof(barTextBuffer), "%s: ", prefix);
        // barText is a data-driven format string (e.g. "Stone%s Plank%s Clay%s")
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        snprintf(barTextBuffer + strlen(barTextBuffer), sizeof(barTextBuffer) - strlen(barTextBuffer),
            def->barText,
            selectedMaterial == 1 ? "<" : "",
            selectedMaterial == 2 ? "<" : "",
            selectedMaterial == 3 ? "<" : "");
#pragma clang diagnostic pop
    } else {
        // Standard action - just prepend prefix
        snprintf(barTextBuffer, sizeof(barTextBuffer), "%s: %s", prefix, def->barText);
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

// Helper to add bar items from the action registry for a given context
static int AddRegistryItems(BarItem* items, int n, InputMode mode, WorkSubMode subMode, InputAction parent) {
    const ActionDef* defs[MAX_BAR_ITEMS];
    int count = GetActionsForContext(mode, subMode, parent, defs, MAX_BAR_ITEMS);
    for (int i = 0; i < count; i++) {
        n = AddItem(items, n, defs[i]->barDisplayText, KeyFromChar(defs[i]->barKey),
                    defs[i]->barUnderlinePos, false, false, false);
    }
    return n;
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
        // In a mode, no action selected — generate menu from registry
        switch (inputMode) {
            case MODE_DRAW:
                n = AddExitHeader(items, n, "DRAW:", KEY_D, 0);
                n = AddRegistryItems(items, n, MODE_DRAW, SUBMODE_NONE, ACTION_NONE);
                break;
            case MODE_WORK:
                if (workSubMode == SUBMODE_NONE) {
                    n = AddExitHeader(items, n, "WORK:", KEY_W, 0);
                    // Submodes are hardcoded (not actions)
                    n = AddItem(items, n, "Dig", KEY_D, 0, false, false, false);
                    n = AddItem(items, n, "Build", KEY_B, 0, false, false, false);
                    n = AddItem(items, n, "Harvest", KEY_H, 0, false, false, false);
                    // Top-level WORK actions from registry (Clean, Gather, etc.)
                    n = AddRegistryItems(items, n, MODE_WORK, SUBMODE_NONE, ACTION_NONE);
                } else {
                    n = AddExitHeader(items, n, "WORK >", KEY_W, 0);
                    switch (workSubMode) {
                        case SUBMODE_DIG:
                            n = AddItem(items, n, "DIG:", KEY_D, 0, true, false, false);
                            break;
                        case SUBMODE_BUILD:
                            n = AddItem(items, n, "BUILD:", KEY_B, 0, true, false, false);
                            break;
                        case SUBMODE_HARVEST:
                            n = AddItem(items, n, "HARVEST:", KEY_H, 0, true, false, false);
                            break;
                        default:
                            break;
                    }
                    n = AddRegistryItems(items, n, MODE_WORK, workSubMode, ACTION_NONE);
                }
                break;
            case MODE_SANDBOX:
                n = AddExitHeader(items, n, "SANDBOX:", KEY_S, 0);
                n = AddRegistryItems(items, n, MODE_SANDBOX, SUBMODE_NONE, ACTION_NONE);
                break;
            default:
                break;
        }
        n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
        return n;
    }
    
    // Action selected — show mode header, submode header, action header from registry
    switch (inputMode) {
        case MODE_DRAW:    n = AddExitHeader(items, n, "DRAW >", KEY_D, 0); break;
        case MODE_WORK:    n = AddExitHeader(items, n, "WORK >", KEY_W, 0); break;
        case MODE_SANDBOX: n = AddExitHeader(items, n, "SANDBOX >", KEY_S, 0); break;
        default: break;
    }
    
    // Submode header for WORK mode
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
    
    // Action header — key and underline from registry
    const ActionDef* def = GetActionDef(inputAction);
    int actionKey = KeyFromChar(def->barKey);
    int actionUnderline = def->barUnderlinePos;
    const char* actionName = GetActionName();
    char actionHeader[32];
    snprintf(actionHeader, sizeof(actionHeader), "%s:", actionName);
    n = AddItem(items, n, actionHeader, actionKey, actionUnderline, true, false, false);
    
    // Check if this action has children (is a category like WORKSHOP/SOIL)
    const ActionDef* children[MAX_BAR_ITEMS];
    int childCount = GetActionsForContext(def->requiredMode, def->requiredSubMode, inputAction, children, MAX_BAR_ITEMS);
    if (childCount > 0) {
        // Category action — show children from registry
        for (int i = 0; i < childCount; i++) {
            n = AddItem(items, n, children[i]->barDisplayText, KeyFromChar(children[i]->barKey),
                        children[i]->barUnderlinePos, false, false, false);
        }
    } else {
        // Leaf action — show action-specific UI (special cases that depend on runtime state)
        switch (inputAction) {
            case ACTION_DRAW_WALL:
            case ACTION_DRAW_FLOOR:
                n = AddItem(items, n, "1:Stone", KEY_ONE, 0, false, false, selectedMaterial == 1);
                n = AddItem(items, n, "2:Wood", KEY_TWO, 0, false, false, selectedMaterial == 2);
                n = AddItem(items, n, "3:Dirt", KEY_THREE, 0, false, false, selectedMaterial == 3);
                n = AddItem(items, n, "4:Oak plank", KEY_FOUR, 0, false, false, selectedMaterial == 4);
                n = AddItem(items, n, "5:Pine plank", KEY_FIVE, 0, false, false, selectedMaterial == 5);
                n = AddItem(items, n, "6:Birch plank", KEY_SIX, 0, false, false, selectedMaterial == 6);
                n = AddItem(items, n, "7:Willow plank", KEY_SEVEN, 0, false, false, selectedMaterial == 7);
                n = AddItem(items, n, "8:Sandstone", KEY_EIGHT, 0, false, false, selectedMaterial == 8);
                n = AddItem(items, n, "9:Slate", KEY_NINE, 0, false, false, selectedMaterial == 9);
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
            case ACTION_DRAW_STOCKPILE:
                n = AddItem(items, n, "L-drag create", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag erase", 0, -1, false, true, false);
                break;
            case ACTION_DRAW_WORKSHOP_STONECUTTER:
            case ACTION_DRAW_WORKSHOP_SAWMILL:
            case ACTION_DRAW_WORKSHOP_KILN:
            case ACTION_DRAW_WORKSHOP_CHARCOAL_PIT:
            case ACTION_DRAW_WORKSHOP_HEARTH:
            case ACTION_DRAW_WORKSHOP_DRYING_RACK:
            case ACTION_DRAW_WORKSHOP_ROPE_MAKER:
                n = AddItem(items, n, "L-click place", 0, -1, false, true, false);
                break;
            case ACTION_DRAW_SOIL_DIRT:
            case ACTION_DRAW_SOIL_CLAY:
            case ACTION_DRAW_SOIL_GRAVEL:
            case ACTION_DRAW_SOIL_SAND:
            case ACTION_DRAW_SOIL_PEAT:
            case ACTION_DRAW_SOIL_ROCK:
                n = AddItem(items, n, "L-drag place", 0, -1, false, true, false);
                n = AddItem(items, n, "+Shift=pile mode", 0, -1, false, true, false);
                break;

            case ACTION_WORK_CONSTRUCT:
                n = AddItem(items, n, TextFormat("Recipe: %s", GetSelectedWallRecipeName()), KEY_R, 0, false, false, false);
                n = AddItem(items, n, "L-drag designate", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag cancel", 0, -1, false, true, false);
                break;
            case ACTION_WORK_LADDER:
                n = AddItem(items, n, TextFormat("Recipe: %s", GetSelectedLadderRecipeName()), KEY_R, 0, false, false, false);
                n = AddItem(items, n, "L-drag designate", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag cancel", 0, -1, false, true, false);
                break;
            case ACTION_WORK_FLOOR:
                n = AddItem(items, n, TextFormat("Recipe: %s", GetSelectedFloorRecipeName()), KEY_R, 0, false, false, false);
                n = AddItem(items, n, "L-drag designate", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag cancel", 0, -1, false, true, false);
                break;
            case ACTION_WORK_RAMP:
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
            case ACTION_SANDBOX_TREE:
                n = AddItem(items, n, TextFormat("Cycle tree (%s)", TreeTypeName(currentTreeType)), KEY_T, 0, false, false, false);
                n = AddItem(items, n, "L-click place", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag remove", 0, -1, false, true, false);
                break;
            case ACTION_SANDBOX_SCULPT:
                n = AddItem(items, n, "1:1x1", KEY_ONE, 0, false, false, terrainBrushRadius == 0);
                n = AddItem(items, n, "2:3x3", KEY_TWO, 0, false, false, terrainBrushRadius == 1);
                n = AddItem(items, n, "3:5x5", KEY_THREE, 0, false, false, terrainBrushRadius == 2);
                n = AddItem(items, n, "4:7x7", KEY_FOUR, 0, false, false, terrainBrushRadius == 3);
                n = AddItem(items, n, "L-drag raise", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag lower", 0, -1, false, true, false);
                break;
            case ACTION_SANDBOX_LIGHT:
                n = AddItem(items, n, "1:Warm", KEY_ONE, 0, false, false, currentTorchPreset == TORCH_PRESET_WARM);
                n = AddItem(items, n, "2:Cool", KEY_TWO, 0, false, false, currentTorchPreset == TORCH_PRESET_COOL);
                n = AddItem(items, n, "3:Fire", KEY_THREE, 0, false, false, currentTorchPreset == TORCH_PRESET_FIRE);
                n = AddItem(items, n, "4:Green", KEY_FOUR, 0, false, false, currentTorchPreset == TORCH_PRESET_GREEN);
                n = AddItem(items, n, "5:White", KEY_FIVE, 0, false, false, currentTorchPreset == TORCH_PRESET_WHITE);
                n = AddItem(items, n, "L-drag place", 0, -1, false, true, false);
                n = AddItem(items, n, "R-drag remove", 0, -1, false, true, false);
                break;
            default:
                // Generic: show drag/cancel hints based on registry canDrag/canErase
                if (def->canDrag) {
                    n = AddItem(items, n, "L-drag designate", 0, -1, false, true, false);
                }
                if (def->canErase) {
                    n = AddItem(items, n, "R-drag cancel", 0, -1, false, true, false);
                }
                break;
        }
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
