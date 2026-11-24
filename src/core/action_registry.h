#ifndef ACTION_REGISTRY_H
#define ACTION_REGISTRY_H

#include "input_mode.h"
#include "../game_state.h"  // For raylib KEY_A

// Metadata for each InputAction
typedef struct {
    InputAction action;
    const char* name;           // Uppercase name for display (e.g., "WALL")
    const char* barDisplayText; // Mixed case for bar button (e.g., "Wall", "cHannel", "dig Ramp")
    char barKey;                // Key to activate this action (0 if none)
    int barUnderlinePos;        // Position in barDisplayText to underline (-1 if none)
    InputMode requiredMode;     // Mode this action belongs to
    WorkSubMode requiredSubMode; // Submode required (SUBMODE_NONE if mode-level action)
    InputAction parentAction;   // ACTION_NONE for top-level, or parent category action
    bool canDrag;               // Can be executed with drag
    bool canErase;              // Right-click erases (for draw actions)
    // Mouse hints shown after the tool's option in the bar ("L-drag place", "R-drag erase",
    // "+Shift=source"). NULL: "L-drag designate" / "R-drag cancel" from canDrag / canErase.
    const char* hints[4];
} ActionDef;

// A tool's option (docs/todo/ui/find-palette.md "Data tidying" step 2): what the tool can be
// set to (draw material, recipe, ramp / belt direction, brush size, torch colour, crop, tree).
// The option bar and the keys that change it are both made from this (input_mode.c, input.c),
// and a palette can show it as a chooser or child rows. Options are indexed by action next to
// the tool handlers in input.c (their state lives there): GetActionOption.
typedef enum {
    OPT_NUMBERED,   // keys 1..count, bar "1:Stone 2:Wood ..." (count <= 9)
    OPT_CYCLE,      // one key steps through, bar "Recipe: Wood wall" (cycleKey)
    OPT_KEYED,      // a key per choice (keys / altKeys), bar shows each choice, or keysHint
} ActionOptionStyle;

#define ACTION_OPTION_MAX_KEYED 8
typedef struct ActionOption {
    const char* label;                 // "Recipe", "Dir", "Tree"
    ActionOptionStyle style;
    int ctx;                           // passed to the callbacks (e.g. the recipe category)
    int  (*count)(int ctx);
    const char* (*nameAt)(int ctx, int i);
    int  (*get)(int ctx);              // index of the current choice
    void (*set)(int ctx, int i);       // choose (and say so, if the tool did before)
    int cycleKey;                      // OPT_CYCLE
    int keys[ACTION_OPTION_MAX_KEYED];     // OPT_KEYED: key per choice
    int altKeys[ACTION_OPTION_MAX_KEYED];  // OPT_KEYED: second key per choice (HJKL), 0 = none
    const char* keysHint;              // OPT_KEYED: show "Label: name" + this instead of every choice
} ActionOption;

const ActionOption* GetActionOption(InputAction action);   // NULL = the tool has none (input.c)

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
