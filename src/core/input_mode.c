// core/input_mode.c - Hierarchical input mode system
#include "input_mode.h"
#include "action_registry.h"
#include "../game_state.h"  // Includes raylib.h
#include "../simulation/lighting.h"
#include "../entities/workshops.h"
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
int dragStartZ = 0;

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
        // If this action has a parent action, go back to the parent (not to ACTION_NONE)
        const ActionDef* def = GetActionDef(inputAction);
        if (def->parentAction != ACTION_NONE) {
            inputAction = def->parentAction;
        } else {
            inputAction = ACTION_NONE;
        }
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

void InputMode_StartLinking(int wsIdx) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS || !workshops[wsIdx].active) return;
    workSubMode = SUBMODE_LINKING_STOCKPILES;
    linkingWorkshopIdx = wsIdx;
    AddMessage("LINKING MODE: Click stockpile to link (ESC to cancel)", YELLOW);
}

void InputMode_SelectAction(InputAction action) {
    const ActionDef* def = GetActionDef(action);
    inputMode = def->requiredMode;
    workSubMode = def->requiredSubMode;
    inputAction = action;
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
    items[count].backSteps = 0;
    return count + 1;
}

// Helper to add a clickable header that calls InputMode_Back() N times when clicked
static int AddBackHeader(BarItem* items, int count, const char* text, int backSteps) {
    if (count >= MAX_BAR_ITEMS) return count;
    strncpy(items[count].text, text, sizeof(items[count].text) - 1);
    items[count].text[sizeof(items[count].text) - 1] = '\0';
    items[count].key = 0;  // No key — click calls Back() directly
    items[count].underlinePos = -1;
    items[count].isHeader = true;
    items[count].isHint = false;
    items[count].isSelected = false;
    items[count].isExit = true;
    items[count].backSteps = backSteps;
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
                n = AddBackHeader(items, n, "DRAW:", 1);
                n = AddRegistryItems(items, n, MODE_DRAW, SUBMODE_NONE, ACTION_NONE);
                break;
            case MODE_WORK:
                if (workSubMode == SUBMODE_NONE) {
                    n = AddBackHeader(items, n, "WORK:", 1);
                    // Submodes are hardcoded (not actions)
                    n = AddItem(items, n, "Dig", KEY_D, 0, false, false, false);
                    n = AddItem(items, n, "Build", KEY_B, 0, false, false, false);
                    n = AddItem(items, n, "Harvest", KEY_H, 0, false, false, false);
                    // Top-level WORK actions from registry (Clean, Gather, etc.)
                    n = AddRegistryItems(items, n, MODE_WORK, SUBMODE_NONE, ACTION_NONE);
                } else {
                    n = AddBackHeader(items, n, "WORK >", 2);  // back past submode to WORK:
                    switch (workSubMode) {
                        case SUBMODE_DIG:
                            n = AddBackHeader(items, n, "DIG:", 1);
                            break;
                        case SUBMODE_BUILD:
                            n = AddBackHeader(items, n, "BUILD:", 1);
                            break;
                        case SUBMODE_HARVEST:
                            n = AddBackHeader(items, n, "HARVEST:", 1);
                            break;
                        default:
                            break;
                    }
                    n = AddRegistryItems(items, n, MODE_WORK, workSubMode, ACTION_NONE);
                }
                break;
            case MODE_SANDBOX:
                n = AddBackHeader(items, n, "SANDBOX:", 1);
                n = AddRegistryItems(items, n, MODE_SANDBOX, SUBMODE_NONE, ACTION_NONE);
                break;
            default:
                break;
        }
        n = AddItem(items, n, "Esc", KEY_ESCAPE, -1, false, false, false);
        return n;
    }
    
    // Action selected — show mode header, submode header, action header from registry
    // Count depth to calculate backSteps for each header
    const ActionDef* def = GetActionDef(inputAction);
    bool hasParentAction = (def->parentAction != ACTION_NONE);
    bool hasSubmode = (workSubMode != SUBMODE_NONE);
    
    // Mode header: backs out to mode level (past action, parent action, and submode)
    int modeBackSteps = 1 + (hasParentAction ? 1 : 0) + (hasSubmode ? 1 : 0);
    switch (inputMode) {
        case MODE_DRAW:    n = AddBackHeader(items, n, "DRAW >", modeBackSteps); break;
        case MODE_WORK:    n = AddBackHeader(items, n, "WORK >", modeBackSteps); break;
        case MODE_SANDBOX: n = AddBackHeader(items, n, "SANDBOX >", modeBackSteps); break;
        default: break;
    }
    
    // Submode header for WORK mode: backs out to submode level (past action and parent action)
    if (hasSubmode) {
        int subBackSteps = 1 + (hasParentAction ? 1 : 0);
        const char* subName = NULL;
        switch (workSubMode) {
            case SUBMODE_DIG:     subName = "DIG >"; break;
            case SUBMODE_BUILD:   subName = "BUILD >"; break;
            case SUBMODE_HARVEST: subName = "HARVEST >"; break;
            default: break;
        }
        if (subName) {
            n = AddBackHeader(items, n, subName, subBackSteps);
        }
    }
    
    // Parent action header (e.g. "SOIL >" when in DRAW > SOIL > DIRT)
    if (hasParentAction) {
        const ActionDef* parentDef = GetActionDef(def->parentAction);
        char parentHeader[32];
        snprintf(parentHeader, sizeof(parentHeader), "%s >", parentDef->name);
        n = AddBackHeader(items, n, parentHeader, 1);  // back to parent category
    }
    
    // Action header — backs out one level (to parent action or to action=NONE)
    const char* actionName = GetActionName();
    char actionHeader[32];
    snprintf(actionHeader, sizeof(actionHeader), "%s:", actionName);
    n = AddBackHeader(items, n, actionHeader, 1);
    // The same tool the other way: I flips now (instant) <-> order (workers) (registry twin)
    if (def->twin != ACTION_NONE)
        n = AddItem(items, n, def->requiredMode == MODE_DRAW ? "I:order" : "I:instant", KEY_I, 0, false, false, false);
    
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
        // Leaf action — the tool's option (ActionOption, input.c) and its mouse hints (registry)
        const ActionOption* o = GetActionOption(inputAction);
        if (o) {
            int count = o->count(o->ctx), cur = o->get(o->ctx);
            if (o->style == OPT_NUMBERED) {
                for (int i = 0; i < count && i < 9; i++)
                    n = AddItem(items, n, TextFormat("%d:%s", i + 1, o->nameAt(o->ctx, i)), KEY_ONE + i, 0, false, false, cur == i);
            } else if (o->style == OPT_CYCLE) {
                n = AddItem(items, n, TextFormat("%s: %s", o->label, cur >= 0 ? o->nameAt(o->ctx, cur) : "?"), o->cycleKey, 0, false, false, false);
            } else if (o->keysHint) {   // OPT_KEYED with a hint instead of every choice
                n = AddItem(items, n, TextFormat("%s: %s", o->label, cur >= 0 ? o->nameAt(o->ctx, cur) : "?"), 0, -1, false, true, false);
                n = AddItem(items, n, o->keysHint, 0, -1, false, true, false);
            } else {
                for (int i = 0; i < count && i < ACTION_OPTION_MAX_KEYED; i++)
                    n = AddItem(items, n, o->nameAt(o->ctx, i), o->keys[i], -1, false, false, cur == i);
            }
        }
        if (def->hints[0]) {
            for (int i = 0; i < 4 && def->hints[i]; i++) n = AddItem(items, n, def->hints[i], 0, -1, false, true, false);
        } else {
            // Generic: drag/cancel hints from the registry's canDrag/canErase
            if (def->canDrag) n = AddItem(items, n, "L-drag designate", 0, -1, false, true, false);
            if (def->canErase) n = AddItem(items, n, "R-drag cancel", 0, -1, false, true, false);
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
