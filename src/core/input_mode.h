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
    ACTION_DESIGNATE_UNBURN,
    // Zone actions
    ACTION_ZONE_STOCKPILE,
    ACTION_ZONE_GATHER,
    // Place actions (debug/sandbox)
    ACTION_PLACE_WATER,
    ACTION_PLACE_FIRE,
    ACTION_PLACE_HEAT,
} InputAction;

// Current input state
extern InputMode inputMode;
extern InputAction inputAction;
extern int selectedMaterial;  // 1-based material index

// Drag state
extern bool isDragging;
extern int dragStartX;
extern int dragStartY;

// Mode management
void InputMode_Reset(void);
void InputMode_Back(void);           // Go back one level (ESC / right-click)
void InputMode_ExitToNormal(void);   // Go directly to normal (re-tap mode key)

// Get display strings for input bar
const char* InputMode_GetModeName(void);
const char* InputMode_GetBarText(void);

// Check if a key would conflict with current mode
bool InputMode_IsReservedKey(int key);

#endif
