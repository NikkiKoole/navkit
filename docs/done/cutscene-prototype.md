# Cutscene Prototype - Implementation Plan

Goal: Get 1-2 hardcoded panels working with typewriter effect to validate the aesthetic and feel.

---

## Design Decisions

### Font Choice
**Use: Existing Comic Sans BMFont from codebase** ⭐

**Why:**
- Already loaded and working
- Consistent with rest of game's UI
- No need to add new fonts or deal with CP437 charset
- Good readability

**Raylib approach:**
```c
// Use whatever Font you're already loading for the game
// Probably something like:
extern Font gameFont;  // or whatever it's called in your codebase
```

---

### Borders: Raylib Primitives (No Box-Drawing Chars Needed)

**Use `DrawRectangleLinesEx()` for borders:**

```c
// Simple single border
DrawRectangleLinesEx((Rectangle){x, y, w, h}, 2, WHITE);

// Double border (draw two rectangles)
DrawRectangleLinesEx((Rectangle){x, y, w, h}, 3, WHITE);
DrawRectangleLinesEx((Rectangle){x+6, y+6, w-12, h-12}, 2, WHITE);
```

**No fancy ASCII art needed for prototype** - just get text panels with typewriter effect working first.

---

### ASCII Art: Defer for Now

**Phase 1: Text only**
- Just typewriter text in panels
- Raylib rectangle borders
- Keep it simple

**Later (optional):**
- Add CP437-style chars to your tile atlas (8x8 sprites)
- Render "ASCII art" using sprite draws instead of text
- Or add Unicode box-drawing chars to BMFont if needed

---

### Colors

**Start simple, add complexity later:**

**Phase 1: Monochrome**
- Background: `BLACK` or `(Color){20, 20, 25, 255}` (dark blue-gray)
- Text: `WHITE` or `(Color){220, 220, 220, 255}` (off-white)
- Border: Same as text, or `GRAY`

**Phase 2: Terminal Colors (optional)**
- Classic amber: `(Color){255, 176, 0, 255}`
- Classic green: `(Color){0, 255, 0, 255}`
- Classic white: `(Color){200, 200, 200, 255}`

**Phase 3: Semantic Colors (optional)**
- Fire/heat panels: warm tint `(Color){255, 200, 150, 255}`
- Water panels: cool tint `(Color){150, 200, 255, 255}`
- Stone panels: gray tint

**Recommendation:** Start with white-on-dark, add color later if it feels right.

---

### Layout: Virtual Character Grid

**Resolution-independent rendering:**
- Virtual grid: 80 columns × 30 rows (classic terminal size)
- Each cell = one character
- Scale grid to fit screen while preserving aspect ratio

**Raylib approach:**
```c
#define GRID_COLS 80
#define GRID_ROWS 30

// Calculate cell size based on screen
int screenW = GetScreenWidth();
int screenH = GetScreenHeight();

// Fit 80x30 grid to screen
float cellW = (float)screenW / GRID_COLS;
float cellH = (float)screenH / GRID_ROWS;

// Use smaller dimension to preserve aspect (don't stretch)
float cellSize = (cellW < cellH) ? cellW : cellH;

// Center the grid
float offsetX = (screenW - cellSize * GRID_COLS) / 2;
float offsetY = (screenH - cellSize * GRID_ROWS) / 2;
```

**Or simpler:** Just use a fixed font size and center the text block.

---

## Minimal Implementation

### File Structure

```
src/
├── ui/
│   ├── cutscene.h
│   └── cutscene.c
```

### cutscene.h

```c
#ifndef CUTSCENE_H
#define CUTSCENE_H

#include <raylib.h>
#include <stdbool.h>

typedef struct {
    const char* text;           // Text to display (can include \n)
    const char* art;            // ASCII art (NULL = text-only panel)
    float typewriterSpeed;      // chars per second (0 = instant)
} Panel;

typedef struct {
    bool active;
    const Panel* panels;
    int panelCount;
    int currentPanel;
    int revealedChars;
    float timer;
    bool skipTypewriter;        // true when user pressed SPACE
} CutsceneState;

// Global state
extern CutsceneState cutsceneState;

// Core functions
void InitCutscene(const Panel* panels, int count);
void UpdateCutscene(float dt);
void RenderCutscene(void);
void CloseCutscene(void);

// Convenience
bool IsCutsceneActive(void);

#endif
```

### cutscene.c (Minimal)

```c
#include "cutscene.h"
#include <string.h>
#include <stdio.h>

CutsceneState cutsceneState = {0};

// Hardcoded test panels (text-only for now)
static Panel testPanels[] = {
    {
        .text =
            "A mover awakens in the wild.\n\n"
            "No tools. No shelter.\n"
            "Only hands and hunger.",
        .art = NULL,  // Skip ASCII art for now
        .typewriterSpeed = 30.0f,
    },
    {
        .text =
            "Gather grass.\n"
            "Gather sticks.\n"
            "Gather stones.\n\n"
            "Survival begins with\n"
            "what the land offers.",
        .art = NULL,
        .typewriterSpeed = 40.0f,
    }
};

void InitCutscene(const Panel* panels, int count) {
    cutsceneState.active = true;
    cutsceneState.panels = panels;
    cutsceneState.panelCount = count;
    cutsceneState.currentPanel = 0;
    cutsceneState.revealedChars = 0;
    cutsceneState.timer = 0.0f;
    cutsceneState.skipTypewriter = false;
}

void UpdateCutscene(float dt) {
    if (!cutsceneState.active) return;

    const Panel* panel = &cutsceneState.panels[cutsceneState.currentPanel];
    int textLen = (int)strlen(panel->text);

    // Typewriter effect
    if (!cutsceneState.skipTypewriter && cutsceneState.revealedChars < textLen) {
        cutsceneState.timer += dt;
        float charsPerSecond = panel->typewriterSpeed;
        int targetChars = (int)(cutsceneState.timer * charsPerSecond);
        if (targetChars > cutsceneState.revealedChars) {
            cutsceneState.revealedChars = targetChars;
            if (cutsceneState.revealedChars > textLen) {
                cutsceneState.revealedChars = textLen;
            }
        }
    }

    // Input: SPACE advances or skips typewriter
    if (IsKeyPressed(KEY_SPACE)) {
        if (cutsceneState.revealedChars < textLen) {
            // Skip typewriter, reveal all
            cutsceneState.revealedChars = textLen;
            cutsceneState.skipTypewriter = true;
        } else {
            // Advance to next panel
            cutsceneState.currentPanel++;
            if (cutsceneState.currentPanel >= cutsceneState.panelCount) {
                CloseCutscene();
            } else {
                cutsceneState.revealedChars = 0;
                cutsceneState.timer = 0.0f;
                cutsceneState.skipTypewriter = false;
            }
        }
    }

    // ESC: skip entire cutscene
    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseCutscene();
    }
}

void RenderCutscene(void) {
    if (!cutsceneState.active) return;

    const Panel* panel = &cutsceneState.panels[cutsceneState.currentPanel];

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    // Background
    ClearBackground((Color){20, 20, 25, 255});

    // Double border using raylib primitives
    DrawRectangleLinesEx((Rectangle){40, 40, screenW - 80, screenH - 80}, 3, RAYWHITE);
    DrawRectangleLinesEx((Rectangle){50, 50, screenW - 100, screenH - 100}, 2, GRAY);

    // Text with typewriter effect
    char buffer[1024];
    int len = cutsceneState.revealedChars;
    if (len > 1023) len = 1023;
    strncpy(buffer, panel->text, len);
    buffer[len] = '\0';

    // Use your existing font
    // extern Font yourGameFont;  // whatever it's called
    // DrawTextEx(yourGameFont, buffer, (Vector2){100, 150}, 24, 2, RAYWHITE);

    // Or use raylib default for now
    DrawText(buffer, 100, 150, 24, RAYWHITE);

    // Prompt
    bool textFullyRevealed = cutsceneState.revealedChars >= (int)strlen(panel->text);
    if (textFullyRevealed) {
        const char* prompt = "[SPACE] to continue";
        int promptW = MeasureText(prompt, 20);
        DrawText(prompt, screenW - promptW - 80, screenH - 80, 20, GRAY);
    }

    // Panel counter
    char counter[32];
    snprintf(counter, sizeof(counter), "%d / %d",
             cutsceneState.currentPanel + 1, cutsceneState.panelCount);
    DrawText(counter, 80, screenH - 80, 20, DARKGRAY);
}

void CloseCutscene(void) {
    cutsceneState.active = false;
}

bool IsCutsceneActive(void) {
    return cutsceneState.active;
}

// Test function: call from main to try it
void PlayTestCutscene(void) {
    InitCutscene(testPanels, 2);
}
```

---

## Integration into Main Game Loop

### main.c changes

```c
#include "ui/cutscene.h"

// In main() or InitGame()
// (Optional: auto-play on first launch)
if (firstLaunch) {
    PlayTestCutscene();
}

// Or bind to a key for testing
if (IsKeyPressed(KEY_I)) {  // 'I' for Intro
    PlayTestCutscene();
}

// In update loop
if (IsCutsceneActive()) {
    UpdateCutscene(GetFrameTime());
    return;  // Don't update game while cutscene plays
}

// ... normal game update ...

// In render loop
if (IsCutsceneActive()) {
    RenderCutscene();
    return;  // Don't render game while cutscene plays
}

// ... normal game render ...
```

---

## Test Panels Content

### Panel 1: Awakening
```c
{
    .text =
        "A mover awakens in the wild.\n\n"
        "No tools. No shelter.\n"
        "Only hands and hunger.",
    .art = NULL,
    .typewriterSpeed = 30.0f,
}
```

### Panel 2: Gathering
```c
{
    .text =
        "Gather grass. Gather sticks.\n"
        "Gather stones from the river.\n\n"
        "Survival begins with\n"
        "what the land offers.\n\n"
        "[Press W to Work]",
    .art = NULL,
    .typewriterSpeed = 40.0f,
}
```

---

## Visual Mockup (What You'll See)

```
╔════════════════════════════════════════════════════════════╗
║                                                            ║
║   A mover awakens in the wild.                             ║
║                                                            ║
║   No tools. No shelter.                                    ║
║   Only hands and hunger.                                   ║
║                                                            ║
║                                                            ║
║                                                            ║
║                                                            ║
║                                                            ║
║                                  [SPACE] to continue       ║
║                                                            ║
║  1 / 2                                                     ║
╚════════════════════════════════════════════════════════════╝
```

*(Double border with raylib DrawRectangleLinesEx)*
*(Text types out character by character, then prompt appears)*

---

## Implementation Checklist

### Step 1: Files (~15 min)
- [ ] Create `src/ui/cutscene.h`
- [ ] Create `src/ui/cutscene.c`
- [ ] Add to `src/unity.c`: `#include "ui/cutscene.c"`

### Step 2: Basic Structure (~30 min)
- [ ] Implement `CutsceneState` and `Panel` structs
- [ ] Implement `InitCutscene()`, `CloseCutscene()`, `IsCutsceneActive()`
- [ ] Add 2 hardcoded test panels

### Step 3: Rendering (~45 min)
- [ ] Implement `RenderCutscene()`
- [ ] Draw background (dark color)
- [ ] Draw border (rectangle outline)
- [ ] Draw ASCII art (if present)
- [ ] Draw text with partial string (typewriter effect)
- [ ] Draw "[SPACE] to continue" prompt when text fully revealed
- [ ] Draw panel counter "1 / 2"

### Step 4: Typewriter Effect (~30 min)
- [ ] Implement `UpdateCutscene()`
- [ ] Timer-based character reveal
- [ ] SPACE skips typewriter (reveals all at once)
- [ ] SPACE advances to next panel when text fully revealed

### Step 5: Integration (~15 min)
- [ ] Hook `UpdateCutscene()` into main game loop
- [ ] Hook `RenderCutscene()` into main render loop
- [ ] Add test key (KEY_I) to trigger cutscene
- [ ] Pause game simulation while cutscene active

### Step 6: Polish (~30 min)
- [ ] ESC skips entire cutscene
- [ ] Test with different text lengths
- [ ] Adjust typewriter speed to feel good
- [ ] Test resolution scaling (resize window)

**Total Time:** ~2.5 hours

---

## Color Experiments (After Basic Works)

Try these color schemes:

### Classic Terminal (Amber)
```c
Color bgColor = {10, 10, 5, 255};       // Very dark brown
Color textColor = {255, 176, 0, 255};   // Amber
Color borderColor = {200, 140, 0, 255}; // Darker amber
```

### Classic Terminal (Green)
```c
Color bgColor = {0, 10, 0, 255};        // Very dark green
Color textColor = {0, 255, 0, 255};     // Bright green
Color borderColor = {0, 180, 0, 255};   // Medium green
```

### Modern Dark Theme
```c
Color bgColor = {20, 20, 25, 255};      // Dark blue-gray
Color textColor = {220, 220, 220, 255}; // Off-white
Color borderColor = {100, 100, 120, 255}; // Gray-blue
```

**Recommendation:** Start with modern dark theme, iterate from there.

---

## What Success Looks Like

After ~2-3 hours you should have:
1. Press `I` key → cutscene starts
2. Panel 1 appears with ASCII art
3. Text types out character-by-character
4. Press SPACE → text reveals fully
5. Press SPACE again → panel 2 appears
6. Text types out (no art this time)
7. Press SPACE → cutscene ends, back to game
8. ESC at any time → skip cutscene

**Then iterate:**
- Adjust colors
- Tweak typewriter speed
- Add more panels
- Improve layout/spacing
- Add sound effects (typewriter click, panel advance)

---

## Next Steps After Prototype Works

1. **Create more panels** (first workshop, first wall, etc.)
2. **Add milestone triggers** (auto-play on events)
3. **Better ASCII art** (more detailed, varied)
4. **Panel transitions** (fade, scroll)
5. **Color per panel** (fire = warm, water = cool)
6. **Sound effects** (typewriter tick, panel whoosh)

---

## Questions to Answer After Seeing Prototype

1. **Does the typewriter speed feel good?** Too fast? Too slow?
2. **Is the text readable?** Font size OK? Line spacing?
3. **Does the border frame it well?** Or is it distracting?
4. **ASCII art legible?** Characters render correctly?
5. **Color scheme feel right?** White on dark? Or try amber/green?
6. **Layout flexible enough?** Need different panel types (split screen, etc.)?

---

## Future: Adding ASCII Art (Optional)

If you want fancy ASCII art later, you have two options:

### Option 1: Add CP437 Chars to Your Tile Atlas
- Add 8x8 sprites for box-drawing chars: `─│┌┐└┘╔═╗║╚╝`
- Add 8x8 sprites for block chars: `█▓▒░▀▄▌▐`
- Render "ASCII art" by drawing sprites in a grid
- More work but gives you full control

### Option 2: Just Use Raylib Primitives
- Draw boxes with `DrawRectangle()` / `DrawRectangleLinesEx()`
- Draw gradients with colored rectangles
- Draw simple shapes (trees, houses, etc.) with primitives
- Simpler but less "ASCII art" aesthetic

**For now:** Just text panels with typewriter effect. ASCII art can wait.

---

## Summary

**Minimal working prototype:**
- 2 hardcoded text-only panels
- Typewriter effect (30-40 chars/sec)
- SPACE advances, ESC skips
- Double border with raylib `DrawRectangleLinesEx()`
- White text on dark background
- Use existing Comic Sans BMFont from codebase
- ~2-3 hours to implement

**File it under:** `docs/doing/` since we're actively building it

Let me know if you want me to write the actual C code or if you want to take this spec and run with it!
