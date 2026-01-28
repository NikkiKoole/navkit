# Hierarchical Input System

## Problem

Current input system has 30+ top-level keyboard shortcuts:
- Hard to memorize
- Key conflicts (R = room drawing, but also R = red filter on stockpiles)
- No visual feedback about available actions
- Adding new actions requires finding unused keys

## Solution

Hierarchical mode system where you tap to enter a category, then tap for the action:

```
Normal → B → W → drag = build wall
Normal → D → M → drag = designate mining
Normal → Z → S → drag = create stockpile
```

## Navigation

Three ways to go back:

1. **ESC** - back one level (always works)
2. **Right-click** (when not dragging) - back one level
3. **Re-tap mode key** - exits directly to normal

Example from `BUILD > WALL > Wood`:
```
ESC → BUILD > WALL → ESC → BUILD → ESC → NORMAL   (step by step)
B   → NORMAL                                       (quick toggle)
```

**Key reservation rule**: The mode key (B/D/Z/P) cannot be used as a submenu option within that mode, since it's reserved for exiting.

## Mode Hierarchy

```
NORMAL
├── [B] Build
│   ├── [W] Wall
│   ├── [F] Floor
│   ├── [L] Ladder
│   ├── [R] Room (walls + floor)
│   └── [O] Door (future) - O for dOor, since D is Designate
│
├── [D] Designate
│   ├── [M] Mine
│   ├── [B] Build (blueprint)
│   ├── [C] Cut trees (future)
│   └── [U] Unburn
│
├── [Z] Zones
│   ├── [S] Stockpile
│   └── [G] Gather
│
├── [P] Place (debug/sandbox tools)
│   ├── [W] Water
│   │   └── +Shift = source/drain
│   ├── [F] Fire
│   │   └── +Shift = source
│   └── [H] Heat/Cold
│
└── [V] View (future)
    ├── [T] Temperature overlay
    ├── [W] Water overlay
    └── [O] Zone visibility
```

## Material Variants (Number Keys)

Tap a number key to select material, then drag. Selection persists until changed:

```
B → W → 2 → drag, drag, drag    = wood walls (2 stays selected)
        1 → drag                 = back to stone
```

For building 10 wood walls:
```
B → W → 2 → drag, drag, drag, drag, drag...
```

No need to hold the number - just tap once to switch.

Materials are defined per-action where relevant:
- Wall: 1=Stone, 2=Wood
- Floor: 1=Stone, 2=Wood
- Door: 1=Wood, 2=Iron (future)

Default is always 1 when entering an action.

## Visual Feedback

Bottom bar shows current mode and available actions:

```
┌──────────────────────────────────────────────────────────────┐
│ [B]uild  [D]esignate  [Z]ones  [P]lace              [?]Help  │
└──────────────────────────────────────────────────────────────┘
```

After pressing B:
```
┌──────────────────────────────────────────────────────────────┐
│ BUILD: [W]all  [F]loor  [L]adder  [R]oom         [ESC]Back   │
└──────────────────────────────────────────────────────────────┘
```

After pressing W (stone selected by default):
```
┌──────────────────────────────────────────────────────────────┐
│ BUILD > WALL: [1]Stone◄ [2]Wood    L-drag place  [ESC]Back   │
└──────────────────────────────────────────────────────────────┘
```

After tapping 2 (wood now selected):
```
┌──────────────────────────────────────────────────────────────┐
│ BUILD > WALL: [1]Stone [2]Wood◄    L-drag place  [ESC]Back   │
└──────────────────────────────────────────────────────────────┘
```

## Mouse Behavior

- **Left-drag**: Primary action (place, designate, create)
- **Right-drag**: Secondary action (cancel, erase) - context dependent
- **Right-click** (no drag): Back one level
- **Shift+drag**: Modifier (sources, drains, etc.)

Right-drag behavior per mode:
| Mode | Left-drag | Right-drag |
|------|-----------|------------|
| Build > Wall | Place wall | Erase cell |
| Designate > Mine | Designate | Cancel designation |
| Zones > Stockpile | Create | Erase cells |
| Place > Water | Add water | Remove water/source/drain |

## Implementation

### State

```c
typedef enum {
    MODE_NORMAL,
    MODE_BUILD,
    MODE_DESIGNATE,
    MODE_ZONES,
    MODE_PLACE,
    MODE_VIEW,
} InputMode;

typedef enum {
    ACTION_NONE,
    // Build
    ACTION_BUILD_WALL,
    ACTION_BUILD_FLOOR,
    ACTION_BUILD_LADDER,
    ACTION_BUILD_ROOM,
    // Designate
    ACTION_DESIGNATE_MINE,
    ACTION_DESIGNATE_BUILD,
    ACTION_DESIGNATE_UNBURN,
    // Zones
    ACTION_ZONE_STOCKPILE,
    ACTION_ZONE_GATHER,
    // Place
    ACTION_PLACE_WATER,
    ACTION_PLACE_FIRE,
    ACTION_PLACE_HEAT,
} InputAction;

InputMode inputMode = MODE_NORMAL;
InputAction inputAction = ACTION_NONE;
int selectedMaterial = 1;  // 1-based, reset to 1 when action changes
```

### Input Flow

```c
void HandleInput(void) {
    // ESC returns to parent
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (inputAction != ACTION_NONE) {
            inputAction = ACTION_NONE;
            selectedMaterial = 1;
        } else if (inputMode != MODE_NORMAL) {
            inputMode = MODE_NORMAL;
        }
        return;
    }
    
    // Right-click (not dragging) returns to parent
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !isDragging) {
        if (inputAction != ACTION_NONE) {
            inputAction = ACTION_NONE;
            selectedMaterial = 1;
        } else if (inputMode != MODE_NORMAL) {
            inputMode = MODE_NORMAL;
        }
        return;
    }

    // Re-tap mode key exits to normal
    if (inputMode == MODE_BUILD && IsKeyPressed(KEY_B)) {
        inputMode = MODE_NORMAL;
        inputAction = ACTION_NONE;
        selectedMaterial = 1;
        return;
    }
    // ... same for D, Z, P, V

    // Mode selection (only in normal mode)
    if (inputMode == MODE_NORMAL) {
        if (IsKeyPressed(KEY_B)) inputMode = MODE_BUILD;
        if (IsKeyPressed(KEY_D)) inputMode = MODE_DESIGNATE;
        if (IsKeyPressed(KEY_Z)) inputMode = MODE_ZONES;
        if (IsKeyPressed(KEY_P)) inputMode = MODE_PLACE;
        return;
    }

    // Action selection (in a mode, no action selected)
    if (inputAction == ACTION_NONE) {
        switch (inputMode) {
            case MODE_BUILD:
                if (IsKeyPressed(KEY_W)) { inputAction = ACTION_BUILD_WALL; selectedMaterial = 1; }
                if (IsKeyPressed(KEY_F)) { inputAction = ACTION_BUILD_FLOOR; selectedMaterial = 1; }
                // ...
                break;
            // ...
        }
        return;
    }
    
    // Material selection (when action is selected)
    if (IsKeyPressed(KEY_ONE)) selectedMaterial = 1;
    if (IsKeyPressed(KEY_TWO)) selectedMaterial = 2;
    if (IsKeyPressed(KEY_THREE)) selectedMaterial = 3;
    // ...

    // Execute action (action selected, handle mouse)
    HandleActionInput();
}
```

### Rendering

```c
void DrawInputBar(void) {
    // Draw bar at bottom of screen
    int barH = 24;
    int barY = GetScreenHeight() - barH;
    DrawRectangle(0, barY, GetScreenWidth(), barH, (Color){30, 30, 30, 240});

    const char* text = GetInputBarText(); // Returns mode-appropriate string
    DrawTextShadow(text, 8, barY + 4, 14, WHITE);
}
```

## Migration

1. Add new input mode/action enums and state
2. Add input bar rendering
3. Migrate one category at a time (Build first)
4. Remove old boolean flags as each mode is migrated
5. Update help panel to show hierarchy
6. Remove old help panel (bar replaces it)

## Special Cases

### Stockpile Hover Controls
When hovering a stockpile, number keys and R/G/B/O still work for:
- +/- priority
- [ ] stack size  
- R/G/B/O filters

These only trigger when `hoveredStockpile >= 0`, before mode input is processed.

### Global Shortcuts (always active)
- Space: Pause
- < / >: Z-level
- Scroll: Zoom
- Middle-drag: Pan
- F5/F6: Save/Load
- C: Reset view

### Tool Panel
The existing tool panel (wall/floor/ladder/erase/start/goal) can remain for quick access or be deprecated in favor of the new system.

## Future Extensions

Easy to add:
- Build > Door, Window, Furniture
- Designate > Cut, Harvest, Clean
- Zones > Bedroom, Dining, Hospital
- View > Overlays, Filters

Each is just a new enum value and a few lines in the switch statements.
