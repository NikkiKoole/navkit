// core/input_mode.h - Hierarchical input mode system
#ifndef INPUT_MODE_H
#define INPUT_MODE_H

#include <stdbool.h>

// Top-level input modes
typedef enum {
    MODE_NORMAL,
    MODE_BUILD,
    MODE_DESIGNATE,
    MODE_ZONES,
    MODE_PLACE,
    MODE_VIEW,
} InputMode;

// Actions within modes
typedef enum {
    ACTION_NONE,
    // Build actions
    ACTION_BUILD_WALL,
    ACTION_BUILD_FLOOR,
    ACTION_BUILD_LADDER,
    ACTION_BUILD_ROOM,
    // Designate actions
    ACTION_DESIGNATE_MINE,
    ACTION_DESIGNATE_BUILD,
    // Zone actions
    ACTION_ZONE_STOCKPILE,
    ACTION_ZONE_GATHER,
    // Place actions (debug/sandbox)
    ACTION_PLACE_WATER,
    ACTION_PLACE_FIRE,
    ACTION_PLACE_HEAT,
    ACTION_PLACE_COLD,
    ACTION_PLACE_SMOKE,
    ACTION_PLACE_STEAM,
    ACTION_PLACE_GRASS,
} InputAction;

// Current input state
extern InputMode inputMode;
extern InputAction inputAction;
extern int selectedMaterial;  // 1-based material index

// Drag state
extern bool isDragging;
extern int dragStartX;
extern int dragStartY;

// Quick edit mode - left/right click draws/erases walls in normal mode
extern bool quickEditEnabled;

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
    char text[32];      // Display text like "[B]uild"
    int key;            // Raylib key code to trigger (0 if not clickable)
    bool isHeader;      // True if this is a mode header like "BUILD:"
    bool isHint;        // True if this is a hint like "L-drag place"
    bool isSelected;    // True if this option is currently selected
    bool isExit;        // True if this key exits the current mode (show in red)
} BarItem;

#define MAX_BAR_ITEMS 12
int InputMode_GetBarItems(BarItem* items);  // Returns count of items

// Trigger a bar item action (called when button clicked)
void InputMode_TriggerKey(int key);
int InputMode_GetPendingKey(void);  // Returns and clears pending key

// Check if a key would conflict with current mode
bool InputMode_IsReservedKey(int key);

#endif
