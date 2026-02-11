# Cutscene System - Integration Guide

Quick guide for hooking the cutscene system into your main game loop.

---

## Files Created

✅ `src/ui/cutscene.h` - Header with public API
✅ `src/ui/cutscene.c` - Implementation with 2 test panels
✅ `src/unity.c` - Added `#include "ui/cutscene.c"`

---

## Integration Steps

### 1. Include Header in main.c

Add near top of `main.c`:

```c
#include "ui/cutscene.h"
```

---

### 2. Hook Update Loop

In your main update function (wherever you handle frame updates):

```c
void UpdateGame(float dt) {
    // Check if cutscene is active
    if (IsCutsceneActive()) {
        UpdateCutscene(dt);
        return;  // Don't update game simulation during cutscene
    }

    // ... your normal game update code ...
}
```

---

### 3. Hook Render Loop

In your main render function:

```c
void RenderGame(void) {
    // Check if cutscene is active
    if (IsCutsceneActive()) {
        RenderCutscene();
        return;  // Only render cutscene, skip game render
    }

    // ... your normal game render code ...
}
```

---

### 4. Add Test Key

Add somewhere in your input handling (probably in `input.c` or main loop):

```c
// Test key: Press I to play intro cutscene
if (IsKeyPressed(KEY_I)) {
    PlayTestCutscene();
}
```

---

## Testing

1. Build: `make path`
2. Run: `./bin/path`
3. Press `I` key → cutscene should start
4. Watch text type out character-by-character
5. Press `SPACE` → skips to full text
6. Press `SPACE` again → advances to panel 2
7. Press `SPACE` → cutscene ends, back to game
8. Press `ESC` anytime → skip entire cutscene

---

## Controls

- `SPACE` or `ENTER` - Skip typewriter / Advance to next panel
- `ESC` or `Q` - Exit cutscene immediately

---

## Using Your BMFont (Optional)

If you want to use your Comic Sans BMFont instead of raylib's default:

### In cutscene.c, find RenderCutscene():

```c
// Current code (uses raylib default):
DrawText(buffer, textX, textY, fontSize, RAYWHITE);

// Replace with your font:
extern Font yourGameFont;  // Whatever it's called in your codebase
DrawTextEx(yourGameFont, buffer, (Vector2){textX, textY}, fontSize, spacing, RAYWHITE);
```

---

## Adding More Cutscenes

### Create New Panel Arrays

In `cutscene.c`, add new panel arrays:

```c
static Panel firstWorkshopPanels[] = {
    {
        .text = "A workshop stands.\n\nWhat hands alone could not make,\ntools and time now will.",
        .typewriterSpeed = 35.0f,
    }
};

void PlayFirstWorkshopCutscene(void) {
    InitCutscene(firstWorkshopPanels, 1);
}
```

### Declare in cutscene.h:

```c
void PlayFirstWorkshopCutscene(void);
```

### Trigger from Game Code

```c
// Example: after player places first workshop
void OnWorkshopPlaced(WorkshopType type) {
    if (!hasSeenFirstWorkshop) {
        PlayFirstWorkshopCutscene();
        hasSeenFirstWorkshop = true;
    }
}
```

---

## Customization

### Adjust Typewriter Speed

In panel definitions, change `typewriterSpeed`:
- `20.0f` - slow (dramatic)
- `40.0f` - medium (default)
- `60.0f` - fast (snappy)
- `0.0f` - instant (no typing effect)

### Adjust Colors

In `cutscene.c`, `RenderCutscene()` function:

```c
// Background color
ClearBackground((Color){20, 20, 25, 255});  // Dark blue-gray

// Border colors
DrawRectangleLinesEx(..., RAYWHITE);  // Outer border
DrawRectangleLinesEx(..., GRAY);      // Inner border

// Text color
DrawText(..., RAYWHITE);

// Prompt color
DrawText(..., GRAY);
```

Try these alternatives:
- Amber terminal: `(Color){255, 176, 0, 255}`
- Green terminal: `(Color){0, 255, 0, 255}`
- Off-white: `(Color){220, 220, 220, 255}`

### Adjust Layout

In `cutscene.c`, `RenderCutscene()` function:

```c
// Margins (distance from screen edge)
int marginX = 60;  // Increase for smaller panel
int marginY = 60;

// Text position
int textX = marginX + 40;  // Inset from border
int textY = marginY + panelH / 2 - 60;  // Vertical center

// Font size
int fontSize = 24;  // Increase for larger text
```

---

## Next Steps

Once basic cutscene works:

1. **Add milestone triggers**
   - First workshop placed
   - First wall built
   - First charcoal made
   - etc.

2. **Track seen cutscenes**
   - Add `bool seenCutscenes[CUTSCENE_COUNT]` to save file
   - Only show each milestone once per save

3. **Add more content**
   - Workshop introductions (one per type)
   - Progression milestones (Era 0→1→2)
   - Tutorial hints

4. **Polish**
   - Panel transitions (fade in/out)
   - Sound effects (typewriter tick, panel advance)
   - Color tinting per cutscene (fire = warm, water = cool)

---

## Troubleshooting

### Cutscene doesn't appear
- Check that `PlayTestCutscene()` is being called
- Verify `IsCutsceneActive()` returns true
- Make sure render/update hooks are in place

### Text doesn't type out
- Check `typewriterSpeed > 0.0f`
- Verify `UpdateCutscene(dt)` is being called with valid dt

### Input doesn't work
- Make sure input handling happens before cutscene update
- Check that KEY_SPACE/KEY_ENTER are defined (raylib keys)

### Compilation errors
- Verify `src/ui/cutscene.c` is included in `src/unity.c`
- Check that raylib.h is in include path

---

## Summary

**What works now:**
- 2-panel test cutscene (intro)
- Typewriter effect (30-40 chars/sec)
- SPACE advances, ESC skips
- Double border with raylib primitives
- Text-only (no ASCII art yet)

**To test:**
- Build and run
- Press `I` key
- Watch intro panels

**To extend:**
- Add more panel arrays in cutscene.c
- Trigger from game events (workshop placed, wall built, etc.)
- Track seen cutscenes in save file
