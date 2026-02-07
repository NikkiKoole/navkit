#ifndef ACTION_REGISTRY_H
#define ACTION_REGISTRY_H

#include "input_mode.h"

// Metadata for each InputAction
typedef struct {
    InputAction action;
    const char* name;           // Uppercase name for display (e.g., "WALL")
    const char* barText;        // Instructions shown in bar when action selected
    char barKey;                // Key to activate this action (0 if none)
    int barUnderlinePos;        // Position in parent menu text to underline (-1 if none)
    InputMode requiredMode;     // Mode this action belongs to
    WorkSubMode requiredSubMode; // Submode required (SUBMODE_NONE if mode-level action)
    bool canDrag;               // Can be executed with drag
    bool canErase;              // Right-click erases (for draw actions)
} ActionDef;

extern const ActionDef ACTION_REGISTRY[];
extern const int ACTION_REGISTRY_COUNT;

// Lookup helper
const ActionDef* GetActionDef(InputAction action);

#endif // ACTION_REGISTRY_H
