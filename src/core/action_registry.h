#ifndef ACTION_REGISTRY_H
#define ACTION_REGISTRY_H

#include "input_mode.h"
#include "../game_state.h"  // For raylib KEY_A

// Metadata for each InputAction
typedef struct {
    InputAction action;
    const char* name;           // Uppercase name for display (e.g., "WALL")
    const char* barDisplayText; // Mixed case for bar button (e.g., "Wall", "cHannel", "dig Ramp")
    const char* barText;        // Instructions shown in bar when action selected
    char barKey;                // Key to activate this action (0 if none)
    int barUnderlinePos;        // Position in barDisplayText to underline (-1 if none)
    InputMode requiredMode;     // Mode this action belongs to
    WorkSubMode requiredSubMode; // Submode required (SUBMODE_NONE if mode-level action)
    InputAction parentAction;   // ACTION_NONE for top-level, or parent category action
    bool canDrag;               // Can be executed with drag
    bool canErase;              // Right-click erases (for draw actions)
} ActionDef;

extern const ActionDef ACTION_REGISTRY[];
extern const int ACTION_REGISTRY_COUNT;

// Lookup helper
const ActionDef* GetActionDef(InputAction action);

// Convert barKey char to raylib KEY_* constant
static inline int KeyFromChar(char c) {
    if (c >= 'a' && c <= 'z') return KEY_A + (c - 'a');
    return 0;
}

// Get all actions matching a mode/submode/parent combination
// Returns count, fills output array with pointers to matching ActionDefs
int GetActionsForContext(InputMode mode, WorkSubMode subMode, InputAction parent,
                         const ActionDef** out, int maxOut);

#endif // ACTION_REGISTRY_H
