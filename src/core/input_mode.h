// core/input_mode.h - Hierarchical input mode system
#ifndef INPUT_MODE_H
#define INPUT_MODE_H

#include <stdbool.h>

// Top-level input modes
typedef enum {
    MODE_NORMAL,
    MODE_DRAW,      // Direct level editing (walls, floors, ladders, stockpiles)
    MODE_WORK,      // Jobs for movers (mine, construct, gather)
    MODE_SANDBOX,   // Sim systems (water, fire, heat, etc.)
} InputMode;

// Sub-modes within WORK mode
typedef enum {
    SUBMODE_NONE,
    SUBMODE_DIG,      // Mine, Channel, Dig ramp, Remove floor/ramp
    SUBMODE_BUILD,    // Construct wall, Floor, Ladder, Ramp
    SUBMODE_HARVEST,  // Chop tree, Gather/Plant sapling
} WorkSubMode;

// Actions within modes
typedef enum {
    ACTION_NONE,
    // Draw actions (direct level editing)
    ACTION_DRAW_WALL,
    ACTION_DRAW_FLOOR,
    ACTION_DRAW_LADDER,
    ACTION_DRAW_RAMP,
    ACTION_DRAW_STOCKPILE,
    ACTION_DRAW_DIRT,
    ACTION_DRAW_WORKSHOP,
    // Work > Dig actions
    ACTION_WORK_MINE,
    ACTION_WORK_CHANNEL,
    ACTION_WORK_DIG_RAMP,
    ACTION_WORK_REMOVE_FLOOR,
    ACTION_WORK_REMOVE_RAMP,
    // Work > Build actions
    ACTION_WORK_CONSTRUCT,
    ACTION_WORK_FLOOR,
    ACTION_WORK_LADDER,
    ACTION_WORK_RAMP,
    // Work > Harvest actions
    ACTION_WORK_CHOP,
    ACTION_WORK_CHOP_FELLED,
    ACTION_WORK_GATHER_SAPLING,
    ACTION_WORK_PLANT_SAPLING,
    // Work > Gather (top-level, no submode)
    ACTION_WORK_GATHER,
    // Sandbox actions (sim systems)
    ACTION_SANDBOX_WATER,
    ACTION_SANDBOX_FIRE,
    ACTION_SANDBOX_HEAT,
    ACTION_SANDBOX_COLD,
    ACTION_SANDBOX_SMOKE,
    ACTION_SANDBOX_STEAM,
    ACTION_SANDBOX_GRASS,
    ACTION_SANDBOX_TREE,
} InputAction;

// Current input state
extern InputMode inputMode;
extern WorkSubMode workSubMode;  // Sub-mode within WORK
extern InputAction inputAction;
extern int selectedMaterial;  // 1-based material index

// Drag state
extern bool isDragging;
extern int dragStartX;
extern int dragStartY;

// Quick edit mode - left/right click draws/erases walls in normal mode
extern bool quickEditEnabled;

// Ramp direction selection (defined in input.c)
#include "../world/grid.h"  // For CellType
extern CellType selectedRampDirection;

// Quit confirmation
extern bool showQuitConfirm;
extern bool shouldQuit;

// Mode management
void InputMode_Reset(void);
void InputMode_Back(void);           // Go back one level (ESC / right-click)
void InputMode_ExitToNormal(void);   // Go directly to normal (re-tap mode key)

// Get display strings for input bar
const char* InputMode_GetModeName(void);
const char* InputMode_GetBarText(void);

// Individual bar item for separate button rendering
typedef struct {
    char text[32];      // Display text like "Wall"
    int key;            // Raylib key code to trigger (0 if not clickable)
    int underlinePos;   // Character index to underline (-1 for none)
    bool isHeader;      // True if this is a mode header like "DRAW:"
    bool isHint;        // True if this is a hint like "L-drag place"
    bool isSelected;    // True if this option is currently selected
    bool isExit;        // True if this key exits the current mode (show in red)
} BarItem;

#define MAX_BAR_ITEMS 16
int InputMode_GetBarItems(BarItem* items);  // Returns count of items

// Trigger a bar item action (called when button clicked)
void InputMode_TriggerKey(int key);
int InputMode_GetPendingKey(void);  // Returns and clears pending key

// Check if a key would conflict with current mode
bool InputMode_IsReservedKey(int key);

#endif
