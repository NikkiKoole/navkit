// Single-header UI library for raylib
// Usage: #define UI_IMPLEMENTATION in ONE .c file before including
#ifndef UI_H
#define UI_H

#include "../vendor/raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// Font size constants — change these to resize all text-based UI in one sweep
#define UI_FONT_MEDIUM 14   // Default for all widgets (draggables, toggles, buttons, columns)
#define UI_FONT_SMALL  12   // Compact variant (patch settings, dense panels)

// ============================================================================
// COLOR PALETTE — canonical colors for the dark DAW theme
// ============================================================================

// Backgrounds (darkest → lightest)
#define UI_BG_DEEPEST   (Color){20, 20, 25, 255}   // Grid workspace, meter backgrounds
#define UI_BG_DARK      (Color){25, 25, 30, 255}   // Main panels, sidebar, transport
#define UI_BG_PANEL     (Color){30, 31, 38, 255}   // Panel/section base
#define UI_BG_BUTTON    (Color){33, 34, 40, 255}   // Inactive button/control background
#define UI_BG_HOVER     (Color){45, 45, 55, 255}   // Hover state for buttons/controls
#define UI_BG_ACTIVE    (Color){50, 55, 68, 255}   // Active/selected tab or panel header
#define UI_BG_POPUP     (Color){25, 25, 35, 245}   // Popup/overlay (semi-transparent)

// Borders & dividers
#define UI_BORDER        (Color){48, 48, 58, 255}   // Default border for buttons/panels
#define UI_BORDER_LIGHT  (Color){60, 60, 72, 255}   // Lighter border (active panels)
#define UI_BORDER_SUBTLE (Color){42, 42, 52, 255}   // Subtle dividers, grid lines
#define UI_DIVIDER       (Color){50, 50, 62, 255}   // Section/panel dividers

// Text colors
#define UI_TEXT_MUTED    (Color){70, 70, 85, 255}   // Very muted labels (sidebar, axis)
#define UI_TEXT_DIM      (Color){100, 100, 120, 255} // Dim text (inactive values)
#define UI_TEXT_SUBTLE   (Color){120, 120, 140, 255} // Subtle labels (section counts)
#define UI_TEXT_LABEL    (Color){140, 140, 160, 255} // Parameter labels, row labels
#define UI_TEXT_BRIGHT   (Color){160, 160, 180, 255} // Bright secondary text
#define UI_TEXT_BLUE     (Color){140, 180, 255, 255} // Blue info text (notes, types)
#define UI_TEXT_SUBLABEL (Color){140, 160, 200, 255} // Section sublabels (pale blue)

// Semantic accent colors (active/highlight tints for button backgrounds)
#define UI_TINT_GREEN    (Color){50, 90, 50, 255}   // Active green (play, enabled, patterns)
#define UI_TINT_GREEN_HI (Color){60, 100, 60, 255}  // Green hover
#define UI_TINT_RED      (Color){90, 45, 45, 255}   // Active red (mute, danger)
#define UI_TINT_RED_HI   (Color){140, 50, 50, 255}  // Red hover/strong
#define UI_TINT_ORANGE   (Color){80, 60, 30, 255}   // Active orange (debug, warm)
#define UI_TINT_BLUE     (Color){50, 70, 90, 255}   // Active blue (scale, cool)

// Accent border colors (colored borders when active)
#define UI_ACCENT_GREEN  (Color){140, 200, 100, 255} // Green border (groove, enabled)
#define UI_ACCENT_RED    (Color){220, 100, 100, 255}  // Red border (mute, alert)
#define UI_ACCENT_BLUE   (Color){100, 150, 220, 255}  // Blue border (scale, info)
#define UI_ACCENT_GOLD   (Color){200, 180, 80, 255}   // Gold border (BPM, master)

// Special
#define UI_GOLD          (Color){170, 160, 80, 255}   // Master volume, crossfader
#define UI_FILL_GREEN    (Color){80, 160, 80, 200}    // Volume/level bar fill
#define UI_FILL_BLUE     (Color){80, 120, 180, 255}   // Pan bar fill
#define UI_FILL_PURPLE   (Color){80, 60, 140, 255}    // Reverb send fill

// Dark tints (inactive colored backgrounds)
#define UI_BG_RED_DARK   (Color){50, 35, 35, 255}     // Dark red base (record off, clear)
#define UI_BG_RED_MED    (Color){80, 50, 50, 255}     // Medium red (alert hover)
#define UI_BG_GREEN_DARK (Color){45, 55, 45, 255}     // Dark green base
#define UI_BG_BROWN      (Color){55, 45, 30, 255}     // Dark brown/warm base

// P-Lock / step accent colors
#define UI_PLOCK_DRUM    (Color){255, 180, 100, 255}   // Drum p-lock labels
#define UI_PLOCK_MELODY  (Color){180, 120, 255, 255}   // Melody p-lock labels (defined in daw.c)

// Warm accent text
#define UI_TEXT_GOLD     (Color){200, 160, 80, 255}    // Gold accent text (groove, warm)
#define UI_TEXT_GREEN    (Color){100, 200, 120, 255}   // Green info text (BPM, voices)

// Initialize UI (call once at startup)
void ui_init(Font* font);

// Call at end of frame to update cursor
void ui_update(void);

// Returns true if mouse is over any UI element (use to block clicks)
bool ui_wants_mouse(void);

// Call at start of UI drawing to reset flags
void ui_begin_frame(void);

// Mark mouse click as consumed (blocks click-through to game world)
void ui_consume_click(void);

// Mark custom UI element as hovered (blocks click-through next frame)
void ui_set_hovered(void);

// Register a blocking rectangle for this frame (checked immediately in ui_wants_mouse)
// Call this BEFORE HandleInput for persistent UI areas like bottom bar
#define UI_MAX_BLOCK_RECTS 16
void ui_add_block_rect(Rectangle rect);
void ui_clear_block_rects(void);

// Draw text with shadow
void DrawTextShadow(const char* text, int x, int y, int size, Color col);

// ============================================================================
// MESSAGE SYSTEM (Stack & Fade)
// ============================================================================

#define MSG_MAX_MESSAGES 5
#define MSG_MAX_LENGTH 128
#define MSG_LIFETIME 4.0f      // seconds before fade starts
#define MSG_FADE_TIME 1.0f     // seconds to fade out
#define MSG_LINE_HEIGHT 20     // pixels between messages

// Add a message to the stack (newest at bottom, older shift up)
void AddMessage(const char* text, Color color);

// Update message timers (call once per frame with delta time, paused=true freezes messages)
void UpdateMessages(float dt, bool paused);

// Draw messages at bottom of screen (call during drawing)
void DrawMessages(int screenWidth, int screenHeight);

// MIDI learn callback (set by app, called by DraggableFloat on right-click)
// Signature: void callback(float* target, float min, float max, const char* label)
typedef void (*UIMidiLearnFunc)(float* target, float min, float max, const char* label);
typedef bool (*UIMidiLearnIsWaitingFunc)(float* target);
typedef int  (*UIMidiLearnGetCCFunc)(float* target);

// Set MIDI learn hooks (call once at init, NULL to disable)
void ui_set_midi_learn_hooks(UIMidiLearnFunc arm, UIMidiLearnIsWaitingFunc isWaiting, UIMidiLearnGetCCFunc getCC);

// Draggable float value - returns true if changed
// Right-click: arm MIDI learn (if hooks set). Right-click again: unlearn.
bool DraggableFloat(float x, float y, const char* label, float* value, float speed, float min, float max);

// Draggable int value - returns true if changed
bool DraggableInt(float x, float y, const char* label, int* value, float speed, int min, int max);

// Draggable int value with logarithmic scale - good for large ranges (1 to 100000+)
bool DraggableIntLog(float x, float y, const char* label, int* value, float speed, int min, int max);

// Toggle boolean - click to toggle
void ToggleBool(float x, float y, const char* label, bool* value);

// Push button - returns true when clicked
bool PushButton(float x, float y, const char* label);

// Inline push button - returns button width (for inline layouts)
float PushButtonInline(float x, float y, const char* label, bool* clicked);

// Cycle through options - click to advance
void CycleOption(float x, float y, const char* label, const char** options, int count, int* value);

// Sized variants - same as above but with configurable font size
bool DraggableFloatS(float x, float y, const char* label, float* value, float speed, float min, float max, int fontSize);
bool DraggableIntS(float x, float y, const char* label, int* value, float speed, int min, int max, int fontSize);
void ToggleBoolS(float x, float y, const char* label, bool* value, int fontSize);
void CycleOptionS(float x, float y, const char* label, const char** options, int count, int* value, int fontSize);

// Collapsible section header - returns true if section is open
bool SectionHeader(float x, float y, const char* label, bool* open);

// ============================================================================
// TOOLTIP VARIANTS - same as above but show tooltip on hover
// ============================================================================

// Draggable float with tooltip
bool DraggableFloatT(float x, float y, const char* label, float* value, float speed, float min, float max, const char* tooltip);

// Draggable int with tooltip
bool DraggableIntT(float x, float y, const char* label, int* value, float speed, int min, int max, const char* tooltip);

// Toggle boolean with tooltip
void ToggleBoolT(float x, float y, const char* label, bool* value, const char* tooltip);

// Draw pending tooltip (call at END of frame, after all UI)
void DrawTooltip(void);

// Measure text width using UI font
int MeasureTextUI(const char* text, int size);

// ============================================================================
// COLUMN LAYOUT HELPERS
// ============================================================================

// Column state - tracks position for auto-layout
typedef struct {
    float x;           // Column X position
    float y;           // Current Y position (advances with each element)
    float startY;      // Starting Y (for reset)
    float spacing;     // Vertical spacing between elements
    int fontSize;       // 0 = default (UI_FONT_MEDIUM), >0 = custom font size for all elements
} UIColumn;

// Create a new column layout
UIColumn ui_column(float x, float y, float spacing);
UIColumn ui_column_small(float x, float y, float spacing, int fontSize);

// Reset column to starting position
void ui_col_reset(UIColumn* col);

// Add vertical space
void ui_col_space(UIColumn* col, float pixels);

// Draw a label/header
void ui_col_label(UIColumn* col, const char* text, Color color);

// Draw a smaller sub-label
void ui_col_sublabel(UIColumn* col, const char* text, Color color);

// Draggable float control
bool ui_col_float(UIColumn* col, const char* label, float* value, float speed, float min, float max);

// Draggable int control
bool ui_col_int(UIColumn* col, const char* label, int* value, float speed, int min, int max);

// Toggle boolean control
void ui_col_toggle(UIColumn* col, const char* label, bool* value);

// Cycle through options control
void ui_col_cycle(UIColumn* col, const char* label, const char** options, int count, int* value);

// Push button - returns true when clicked
bool ui_col_button(UIColumn* col, const char* label);

// ============================================================================
// PANEL SYSTEM - Draggable, closable, toggleable panels
// ============================================================================

#define UI_MAX_PANELS 16
#define UI_PANEL_TITLE_H 22
#define UI_PANEL_PAD 8

// Panel flags
#define UI_PANEL_DRAGGABLE  (1 << 0)
#define UI_PANEL_CLOSABLE   (1 << 1)
#define UI_PANEL_RESIZABLE  (1 << 2)

typedef void (*UIPanelDrawFunc)(float x, float y, float w, float h);

typedef struct {
    const char* title;          // Panel title (displayed in title bar)
    float x, y, w, h;          // Position and size
    float minW, minH;          // Minimum size (for resize)
    int hotkey;                 // Raylib KEY_* to toggle, 0 = none
    int flags;                  // UI_PANEL_* flags
    bool open;                  // Visible?
    UIPanelDrawFunc draw;       // Content draw callback
    // Internal state
    int zOrder;                 // Higher = drawn on top
} UIPanel;

// Initialize panel system (call once)
void ui_panels_init(void);

// Register a panel - returns panel index (or -1 if full)
int ui_panel_add(const char* title, float x, float y, float w, float h,
                 int hotkey, int flags, UIPanelDrawFunc draw);

// Set minimum size for resizable panels
void ui_panel_set_min_size(int idx, float minW, float minH);

// Process input for all panels (call before drawing, after ui_begin_frame)
void ui_panels_update(void);

// Draw all open panels in z-order (call during drawing)
void ui_panels_draw(void);

// Get panel by index
UIPanel* ui_panel_get(int idx);

// Toggle a panel open/closed
void ui_panel_toggle(int idx);

// Check if mouse is over any open panel
bool ui_panels_wants_mouse(void);

// Draw a hotkey legend bar showing all panels and their toggle keys
void ui_panels_draw_legend(float x, float y);

#endif // UI_H

// ============================================================================
// IMPLEMENTATION
// ============================================================================
#if defined(UI_IMPLEMENTATION) && !defined(UI_IMPLEMENTATION_DONE)
#define UI_IMPLEMENTATION_DONE

static Font* g_ui_font = NULL;
static bool g_ui_draggableAnyHovered = false;
static bool g_ui_toggleAnyHovered = false;
static bool g_ui_buttonAnyHovered = false;
static bool g_ui_cycleAnyHovered = false;
static bool g_ui_clickConsumed = false;  // Set when UI consumes a click
static bool g_ui_customHovered = false;  // For custom UI elements (like profiler)
static bool g_ui_isDragging = false;     // True while any draggable is being dragged

// MIDI learn hooks (set by app via ui_set_midi_learn_hooks)
static UIMidiLearnFunc g_ui_midiLearnArm = NULL;
static UIMidiLearnIsWaitingFunc g_ui_midiLearnIsWaiting = NULL;
static UIMidiLearnGetCCFunc g_ui_midiLearnGetCC = NULL;

// Block rectangles - registered before HandleInput, checked immediately
static Rectangle g_ui_blockRects[UI_MAX_BLOCK_RECTS];
static int g_ui_blockRectCount = 0;

void ui_init(Font* font) {
    g_ui_font = font;
}

void ui_update(void) {
    if (g_ui_draggableAnyHovered) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
    } else if (g_ui_toggleAnyHovered || g_ui_buttonAnyHovered || g_ui_cycleAnyHovered) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
    // Don't reset hover/click flags here - they persist until next frame's drawing
}

bool ui_wants_mouse(void) {
    // If any UI element is being dragged, always block
    if (g_ui_isDragging) return true;
    
    // Check block rectangles (immediate, same-frame check)
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < g_ui_blockRectCount; i++) {
        if (CheckCollisionPointRec(mouse, g_ui_blockRects[i])) {
            return true;
        }
    }
    // Fall back to hover flags (frame-delayed)
    return g_ui_clickConsumed || g_ui_draggableAnyHovered || g_ui_toggleAnyHovered ||
           g_ui_buttonAnyHovered || g_ui_cycleAnyHovered || g_ui_customHovered;
}

void ui_begin_frame(void) {
    g_ui_draggableAnyHovered = false;
    g_ui_toggleAnyHovered = false;
    g_ui_buttonAnyHovered = false;
    g_ui_cycleAnyHovered = false;
    g_ui_clickConsumed = false;
    g_ui_customHovered = false;
    // Note: block rects are NOT cleared here - they persist until explicitly cleared
}

void ui_clear_block_rects(void) {
    g_ui_blockRectCount = 0;
}

void ui_add_block_rect(Rectangle rect) {
    if (g_ui_blockRectCount < UI_MAX_BLOCK_RECTS) {
        g_ui_blockRects[g_ui_blockRectCount++] = rect;
    }
}

void ui_consume_click(void) {
    g_ui_clickConsumed = true;
}

void ui_set_hovered(void) {
    g_ui_customHovered = true;
}

void DrawTextShadow(const char* text, int x, int y, int size, Color col) {
    if (g_ui_font && g_ui_font->texture.id > 0) {
        Vector2 pos = {(float)x, (float)y};
        DrawTextEx(*g_ui_font, text, (Vector2){pos.x + 1, pos.y + 1}, (float)size, 1, BLACK);
        DrawTextEx(*g_ui_font, text, pos, (float)size, 1, col);
    } else {
        DrawText(text, x + 1, y + 1, size, BLACK);
        DrawText(text, x, y, size, col);
    }
}

// Measure text width using the UI font (matches DrawTextShadow)
int MeasureTextUI(const char* text, int size) {
    if (g_ui_font && g_ui_font->texture.id > 0) {
        Vector2 measure = MeasureTextEx(*g_ui_font, text, (float)size, 1);
        return (int)measure.x;
    } else {
        return MeasureText(text, size);
    }
}

// Set MIDI learn hooks (call once at init)
void ui_set_midi_learn_hooks(UIMidiLearnFunc arm, UIMidiLearnIsWaitingFunc isWaiting, UIMidiLearnGetCCFunc getCC) {
    g_ui_midiLearnArm = arm;
    g_ui_midiLearnIsWaiting = isWaiting;
    g_ui_midiLearnGetCC = getCC;
}

bool DraggableFloat(float x, float y, const char* label, float* value, float speed, float min, float max) {
    // Check MIDI learn state for this parameter
    bool midiWaiting = g_ui_midiLearnIsWaiting && g_ui_midiLearnIsWaiting(value);
    int midiCC = g_ui_midiLearnGetCC ? g_ui_midiLearnGetCC(value) : -1;

    char buf[64];
    if (midiWaiting) {
        snprintf(buf, sizeof(buf), "%s: MIDI?", label);
    } else if (midiCC >= 0) {
        snprintf(buf, sizeof(buf), "%s: %.2f [CC%d]", label, *value, midiCC);
    } else {
        snprintf(buf, sizeof(buf), "%s: %.2f", label, *value);
    }

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_draggableAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    if (midiWaiting) col = (Color){255, 100, 255, 255}; // magenta = waiting
    else if (midiCC >= 0) col = hovered ? YELLOW : (Color){100, 200, 255, 255}; // cyan = mapped
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    // Right-click: arm/disarm MIDI learn
    if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && g_ui_midiLearnArm) {
        g_ui_midiLearnArm(value, min, max, label);
        g_ui_clickConsumed = true;
    }

    static bool dragging = false;
    static float* dragTarget = NULL;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        g_ui_clickConsumed = true;
        g_ui_isDragging = true;
    }

    if (dragging && dragTarget == value) {
        float delta = GetMouseDelta().x * speed * 0.1f;
        *value += delta;
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
            dragTarget = NULL;
            g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

bool DraggableFloatS(float x, float y, const char* label, float* value, float speed, float min, float max, int fontSize) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %.2f", label, *value);
    int textWidth = MeasureTextUI(buf, fontSize);
    int rowH = fontSize + 2;
    Rectangle bounds = {x, y, (float)textWidth + 6, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    if (hovered) g_ui_draggableAnyHovered = true;
    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fontSize, col);
    if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && g_ui_midiLearnArm) {
        g_ui_midiLearnArm(value, min, max, label);
        g_ui_clickConsumed = true;
    }
    static bool dragging = false;
    static float* dragTarget = NULL;
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true; dragTarget = value;
        g_ui_clickConsumed = true; g_ui_isDragging = true;
    }
    if (dragging && dragTarget == value) {
        float delta = GetMouseDelta().x * speed * 0.1f;
        *value += delta;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false; dragTarget = NULL; g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

bool DraggableIntS(float x, float y, const char* label, int* value, float speed, int min, int max, int fontSize) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", label, *value);
    int textWidth = MeasureTextUI(buf, fontSize);
    int rowH = fontSize + 2;
    Rectangle bounds = {x, y, (float)textWidth + 6, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    if (hovered) g_ui_draggableAnyHovered = true;
    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fontSize, col);
    static bool dragging = false;
    static int* dragTarget = NULL;
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true; dragTarget = value;
        g_ui_clickConsumed = true; g_ui_isDragging = true;
    }
    if (dragging && dragTarget == value) {
        float delta = GetMouseDelta().x * speed * 0.1f;
        int newVal = *value + (int)(delta > 0 ? delta + 0.5f : delta - 0.5f);
        if (newVal < min) newVal = min;
        if (newVal > max) newVal = max;
        if (newVal != *value) *value = newVal;
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false; dragTarget = NULL; g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

void ToggleBoolS(float x, float y, const char* label, bool* value, int fontSize) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%c] %s", *value ? 'X' : ' ', label);
    int textWidth = MeasureTextUI(buf, fontSize);
    int rowH = fontSize + 2;
    Rectangle bounds = {x, y, (float)textWidth + 6, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    if (hovered) g_ui_toggleAnyHovered = true;
    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fontSize, col);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !*value; g_ui_clickConsumed = true;
    }
}

void CycleOptionS(float x, float y, const char* label, const char** options, int count, int* value, int fontSize) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: < %s >", label, options[*value]);
    int textWidth = MeasureTextUI(buf, fontSize);
    int rowH = fontSize + 2;
    Rectangle bounds = {x, y, (float)textWidth + 6, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    if (hovered) g_ui_cycleAnyHovered = true;
    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fontSize, col);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = (*value + 1) % count; g_ui_clickConsumed = true;
    }
    if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        *value = (*value - 1 + count) % count; g_ui_clickConsumed = true;
    }
}

bool DraggableInt(float x, float y, const char* label, int* value, float speed, int min, int max) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", label, *value);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_draggableAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    static bool dragging = false;
    static int* dragTarget = NULL;
    static float accumulator = 0;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        accumulator = 0;
        g_ui_clickConsumed = true;
        g_ui_isDragging = true;
    }

    if (dragging && dragTarget == value) {
        accumulator += GetMouseDelta().x * speed * 0.1f;
        int delta = (int)accumulator;
        if (delta != 0) {
            *value += delta;
            accumulator -= delta;
        }
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
            dragTarget = NULL;
            g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

bool DraggableIntLog(float x, float y, const char* label, int* value, float speed, int min, int max) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", label, *value);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_draggableAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    static bool dragging = false;
    static int* dragTarget = NULL;
    static float accumulator = 0;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        accumulator = 0;
        g_ui_clickConsumed = true;
        g_ui_isDragging = true;
    }

    if (dragging && dragTarget == value) {
        // Logarithmic: drag speed scales with current value
        // Small values = fine control, large values = fast jumps
        float scaleFactor = (*value < 10) ? 1.0f : (float)(*value) * 0.1f;
        accumulator += GetMouseDelta().x * speed * 0.1f * scaleFactor;
        int delta = (int)accumulator;
        if (delta != 0) {
            *value += delta;
            accumulator -= delta;
        }
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
            dragTarget = NULL;
            g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

void ToggleBool(float x, float y, const char* label, bool* value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%c] %s", *value ? 'X' : ' ', label);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_toggleAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !*value;
        g_ui_clickConsumed = true;
    }
}

bool PushButton(float x, float y, const char* label) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%s]", label);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_buttonAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g_ui_clickConsumed = true;
        return true;
    }
    return false;
}

float PushButtonInline(float x, float y, const char* label, bool* clicked) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%s]", label);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_buttonAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g_ui_clickConsumed = true;
        *clicked = true;
    }
    return (float)textWidth + 15;  // width + spacing
}

void CycleOption(float x, float y, const char* label, const char** options, int count, int* value) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: < %s >", label, options[*value]);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_cycleAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = (*value + 1) % count;
        g_ui_clickConsumed = true;
    }
    if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        *value = (*value - 1 + count) % count;
        g_ui_clickConsumed = true;
    }
}

bool SectionHeader(float x, float y, const char* label, bool* open) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[%c] %s", *open ? '-' : '+', label);

    int textWidth = MeasureTextUI(buf, 14);
    Rectangle bounds = {x, y, (float)textWidth + 10, 18};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_toggleAnyHovered = true;

    Color col = hovered ? YELLOW : GRAY;
    DrawTextShadow(buf, (int)x, (int)y, 14, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *open = !*open;
        g_ui_clickConsumed = true;
    }

    return *open;
}

// ============================================================================
// MESSAGE SYSTEM IMPLEMENTATION
// ============================================================================

typedef struct {
    char text[MSG_MAX_LENGTH];
    Color color;
    float timeLeft;
    bool active;
} UIMessage;

static UIMessage g_messages[MSG_MAX_MESSAGES] = {0};

void AddMessage(const char* text, Color color) {
    // Shift all messages up (oldest at index 0 gets pushed out)
    for (int i = 0; i < MSG_MAX_MESSAGES - 1; i++) {
        g_messages[i] = g_messages[i + 1];
    }
    
    // Add new message at bottom
    UIMessage* msg = &g_messages[MSG_MAX_MESSAGES - 1];
    snprintf(msg->text, MSG_MAX_LENGTH, "%s", text);
    msg->color = color;
    msg->timeLeft = MSG_LIFETIME + MSG_FADE_TIME;
    msg->active = true;
}

void UpdateMessages(float dt, bool paused) {
    if (paused) return;  // Freeze messages when paused
    for (int i = 0; i < MSG_MAX_MESSAGES; i++) {
        if (g_messages[i].active) {
            g_messages[i].timeLeft -= dt;
            if (g_messages[i].timeLeft <= 0) {
                g_messages[i].active = false;
            }
        }
    }
}

void DrawMessages(int screenWidth, int screenHeight) {
    // Count active messages, find max alpha and max text width
    int activeCount = 0;
    float maxAlpha = 0.0f;
    int maxTextWidth = 0;
    for (int i = 0; i < MSG_MAX_MESSAGES; i++) {
        if (g_messages[i].active) {
            activeCount++;
            float alpha = 1.0f;
            if (g_messages[i].timeLeft < MSG_FADE_TIME) {
                alpha = g_messages[i].timeLeft / MSG_FADE_TIME;
            }
            if (alpha > maxAlpha) maxAlpha = alpha;
            int textWidth = MeasureTextUI(g_messages[i].text, 16);
            if (textWidth > maxTextWidth) maxTextWidth = textWidth;
        }
    }
    
    if (activeCount == 0) return;
    
    // Message area: sized to fit content, aligned to bottom-right
    int padding = 8;
    int panelWidth = maxTextWidth + padding * 2;
    int panelX = screenWidth - panelWidth;
    int panelHeight = activeCount * MSG_LINE_HEIGHT + padding * 2;
    int panelY = screenHeight - panelHeight;
    
    // Draw backdrop
    Color backdrop = {0, 0, 0, (unsigned char)(maxAlpha * 128)};
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, backdrop);
    
    // Draw messages (oldest to newest, bottom-aligned)
    int y = panelY + panelHeight - padding - MSG_LINE_HEIGHT;
    for (int i = MSG_MAX_MESSAGES - 1; i >= 0; i--) {
        UIMessage* msg = &g_messages[i];
        if (!msg->active) continue;
        
        // Calculate alpha for fade
        float alpha = 1.0f;
        if (msg->timeLeft < MSG_FADE_TIME) {
            alpha = msg->timeLeft / MSG_FADE_TIME;
        }
        
        // Apply alpha to color
        Color col = msg->color;
        col.a = (unsigned char)(alpha * 255);
        
        // Right-aligned within panel, clamped to panel left edge
        int textWidth = MeasureTextUI(msg->text, 16);
        int x = screenWidth - textWidth - padding;
        if (x < panelX + padding) x = panelX + padding;
        
        DrawTextShadow(msg->text, x, y, 16, col);
        y -= MSG_LINE_HEIGHT;
    }
}

// ============================================================================
// TOOLTIP SYSTEM
// ============================================================================

#define TOOLTIP_MAX_LENGTH 256
#define TOOLTIP_HOVER_DELAY 0.3f  // seconds before tooltip appears

static char g_tooltip_text[TOOLTIP_MAX_LENGTH] = {0};
static float g_tooltip_hover_time = 0.0f;
static Vector2 g_tooltip_mouse_pos = {0, 0};

// Internal: set tooltip to show (called by tooltip variants)
static void ui_set_tooltip(const char* text) {
    Vector2 mouse = GetMousePosition();
    
    // Reset hover time if mouse moved significantly
    float dx = mouse.x - g_tooltip_mouse_pos.x;
    float dy = mouse.y - g_tooltip_mouse_pos.y;
    if (dx*dx + dy*dy > 25.0f) {  // 5 pixel threshold
        g_tooltip_hover_time = 0.0f;
        g_tooltip_mouse_pos = mouse;
    }
    
    g_tooltip_hover_time += GetFrameTime();
    snprintf(g_tooltip_text, TOOLTIP_MAX_LENGTH, "%s", text);
}

void DrawTooltip(void) {
    if (g_tooltip_text[0] == '\0') return;
    if (g_tooltip_hover_time < TOOLTIP_HOVER_DELAY) {
        g_tooltip_text[0] = '\0';
        return;
    }
    
    Vector2 mouse = GetMousePosition();
    int fontSize = 14;
    int padding = 6;
    int maxWidth = 250;
    
    // Word-wrap: split into lines
    char lines[8][128];
    int lineCount = 0;
    int maxLineWidth = 0;
    
    const char* src = g_tooltip_text;
    while (*src && lineCount < 8) {
        // Copy words until line is full
        char* dst = lines[lineCount];
        int lineLen = 0;
        int lastSpace = -1;
        
        while (*src && lineLen < 127) {
            // Handle explicit newlines
            if (*src == '\n') {
                src++;
                break;
            }
            if (*src == ' ') lastSpace = lineLen;
            dst[lineLen++] = *src++;
            dst[lineLen] = '\0';
            
            int w = MeasureTextUI(dst, fontSize);
            if (w > maxWidth && lastSpace > 0) {
                // Wrap at last space
                dst[lastSpace] = '\0';
                src -= (lineLen - lastSpace - 1);
                lineLen = lastSpace;
                break;
            }
        }
        
        int w = MeasureTextUI(dst, fontSize);
        if (w > maxLineWidth) maxLineWidth = w;
        lineCount++;
    }
    
    int boxW = maxLineWidth + padding * 2;
    int boxH = lineCount * (fontSize + 2) + padding * 2;
    
    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    
    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){30, 30, 30, 230});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){80, 80, 80, 255});
    
    // Draw text lines
    int textY = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        DrawTextShadow(lines[i], tx + padding, textY, fontSize, WHITE);
        textY += fontSize + 2;
    }
    
    // Clear tooltip for next frame
    g_tooltip_text[0] = '\0';
}

bool DraggableFloatT(float x, float y, const char* label, float* value, float speed, float min, float max, const char* tooltip) {
    bool midiWaiting = g_ui_midiLearnIsWaiting && g_ui_midiLearnIsWaiting(value);
    int midiCC = g_ui_midiLearnGetCC ? g_ui_midiLearnGetCC(value) : -1;

    char buf[64];
    if (midiWaiting) {
        snprintf(buf, sizeof(buf), "%s: MIDI?", label);
    } else if (midiCC >= 0) {
        snprintf(buf, sizeof(buf), "%s: %.2f [CC%d]", label, *value, midiCC);
    } else {
        snprintf(buf, sizeof(buf), "%s: %.2f", label, *value);
    }

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) {
        g_ui_draggableAnyHovered = true;
        if (tooltip) ui_set_tooltip(tooltip);
    }

    Color col = hovered ? YELLOW : LIGHTGRAY;
    if (midiWaiting) col = (Color){255, 100, 255, 255};
    else if (midiCC >= 0) col = hovered ? YELLOW : (Color){100, 200, 255, 255};
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    // Right-click: arm/disarm MIDI learn
    if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && g_ui_midiLearnArm) {
        g_ui_midiLearnArm(value, min, max, label);
        g_ui_clickConsumed = true;
    }

    static bool dragging = false;
    static float* dragTarget = NULL;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        g_ui_clickConsumed = true;
        g_ui_isDragging = true;
    }

    if (dragging && dragTarget == value) {
        float delta = GetMouseDelta().x * speed * 0.1f;
        *value += delta;
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
            dragTarget = NULL;
            g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

bool DraggableIntT(float x, float y, const char* label, int* value, float speed, int min, int max, const char* tooltip) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", label, *value);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) {
        g_ui_draggableAnyHovered = true;
        if (tooltip) ui_set_tooltip(tooltip);
    }

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    static bool dragging = false;
    static int* dragTarget = NULL;
    static float accumulator = 0;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        accumulator = 0;
        g_ui_clickConsumed = true;
        g_ui_isDragging = true;
    }

    if (dragging && dragTarget == value) {
        accumulator += GetMouseDelta().x * speed * 0.1f;
        int delta = (int)accumulator;
        if (delta != 0) {
            *value += delta;
            accumulator -= delta;
        }
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
            dragTarget = NULL;
            g_ui_isDragging = false;
        }
        return true;
    }
    return false;
}

void ToggleBoolT(float x, float y, const char* label, bool* value, const char* tooltip) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%c] %s", *value ? 'X' : ' ', label);

    int fs = UI_FONT_MEDIUM;
    int textWidth = MeasureTextUI(buf, fs);
    int rowH = fs + 2;
    Rectangle bounds = {x, y, (float)textWidth + 10, (float)rowH};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) {
        g_ui_toggleAnyHovered = true;
        if (tooltip) ui_set_tooltip(tooltip);
    }

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, fs, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !*value;
        g_ui_clickConsumed = true;
    }
}

// ============================================================================
// COLUMN LAYOUT IMPLEMENTATION
// ============================================================================

UIColumn ui_column(float x, float y, float spacing) {
    UIColumn col;
    col.x = x;
    col.y = y;
    col.startY = y;
    col.spacing = spacing;
    col.fontSize = 0;  // 0 = default (UI_FONT_MEDIUM)
    return col;
}

UIColumn ui_column_small(float x, float y, float spacing, int fontSize) {
    UIColumn col = ui_column(x, y, spacing);
    col.fontSize = fontSize;
    return col;
}

void ui_col_reset(UIColumn* col) {
    col->y = col->startY;
}

void ui_col_space(UIColumn* col, float pixels) {
    col->y += pixels;
}

void ui_col_label(UIColumn* col, const char* text, Color color) {
    int fs = col->fontSize > 0 ? col->fontSize : UI_FONT_MEDIUM;
    DrawTextShadow(text, (int)col->x, (int)col->y, fs, color);
    col->y += col->spacing;
}

void ui_col_sublabel(UIColumn* col, const char* text, Color color) {
    int fs = col->fontSize > 0 ? col->fontSize - 2 : UI_FONT_SMALL;
    if (fs < 8) fs = 8;
    DrawTextShadow(text, (int)col->x, (int)col->y, fs, color);
    col->y += col->spacing - 2;
}

bool ui_col_float(UIColumn* col, const char* label, float* value, float speed, float min, float max) {
    bool changed;
    if (col->fontSize > 0) {
        changed = DraggableFloatS(col->x, col->y, label, value, speed, min, max, col->fontSize);
    } else {
        changed = DraggableFloat(col->x, col->y, label, value, speed, min, max);
    }
    col->y += col->spacing;
    return changed;
}

bool ui_col_int(UIColumn* col, const char* label, int* value, float speed, int min, int max) {
    bool changed;
    if (col->fontSize > 0) {
        changed = DraggableIntS(col->x, col->y, label, value, speed, min, max, col->fontSize);
    } else {
        changed = DraggableInt(col->x, col->y, label, value, speed, min, max);
    }
    col->y += col->spacing;
    return changed;
}

void ui_col_toggle(UIColumn* col, const char* label, bool* value) {
    if (col->fontSize > 0) {
        ToggleBoolS(col->x, col->y, label, value, col->fontSize);
    } else {
        ToggleBool(col->x, col->y, label, value);
    }
    col->y += col->spacing;
}

void ui_col_cycle(UIColumn* col, const char* label, const char** options, int count, int* value) {
    if (col->fontSize > 0) {
        CycleOptionS(col->x, col->y, label, options, count, value, col->fontSize);
    } else {
        CycleOption(col->x, col->y, label, options, count, value);
    }
    col->y += col->spacing + 2;  // Cycles are slightly taller
}

bool ui_col_button(UIColumn* col, const char* label) {
    bool clicked = PushButton(col->x, col->y, label);
    col->y += col->spacing;
    return clicked;
}

// ============================================================================
// PANEL SYSTEM IMPLEMENTATION
// ============================================================================

static UIPanel g_panels[UI_MAX_PANELS];
static int g_panelCount = 0;
static int g_panelNextZ = 1;

// Drag state
static int g_panelDragIdx = -1;
static float g_panelDragOffX = 0, g_panelDragOffY = 0;

// Resize state
static int g_panelResizeIdx = -1;
static float g_panelResizeOffX = 0, g_panelResizeOffY = 0;

void ui_panels_init(void) {
    g_panelCount = 0;
    g_panelNextZ = 1;
    g_panelDragIdx = -1;
    g_panelResizeIdx = -1;
}

int ui_panel_add(const char* title, float x, float y, float w, float h,
                 int hotkey, int flags, UIPanelDrawFunc draw) {
    if (g_panelCount >= UI_MAX_PANELS) return -1;
    int idx = g_panelCount++;
    UIPanel* p = &g_panels[idx];
    p->title = title;
    p->x = x; p->y = y; p->w = w; p->h = h;
    p->minW = 150; p->minH = 80;
    p->hotkey = hotkey;
    p->flags = flags;
    p->open = false;
    p->draw = draw;
    p->zOrder = g_panelNextZ++;
    return idx;
}

void ui_panel_set_min_size(int idx, float minW, float minH) {
    if (idx >= 0 && idx < g_panelCount) {
        g_panels[idx].minW = minW;
        g_panels[idx].minH = minH;
    }
}

UIPanel* ui_panel_get(int idx) {
    if (idx >= 0 && idx < g_panelCount) return &g_panels[idx];
    return NULL;
}

void ui_panel_toggle(int idx) {
    if (idx >= 0 && idx < g_panelCount) {
        g_panels[idx].open = !g_panels[idx].open;
        if (g_panels[idx].open) {
            g_panels[idx].zOrder = g_panelNextZ++;
        }
    }
}

// Bring panel to front
static void ui_panel_focus(int idx) {
    g_panels[idx].zOrder = g_panelNextZ++;
}

// Get sorted draw order (back to front)
static void ui_panels_sorted(int* order, int* count) {
    *count = 0;
    for (int i = 0; i < g_panelCount; i++) {
        if (g_panels[i].open) order[(*count)++] = i;
    }
    // Simple insertion sort by zOrder
    for (int i = 1; i < *count; i++) {
        int key = order[i];
        int j = i - 1;
        while (j >= 0 && g_panels[order[j]].zOrder > g_panels[key].zOrder) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
}

// Get the topmost panel under the mouse, or -1
static int ui_panel_hit_test(Vector2 mouse) {
    int order[UI_MAX_PANELS];
    int count;
    ui_panels_sorted(order, &count);
    // Check back-to-front (highest z last), return highest hit
    for (int i = count - 1; i >= 0; i--) {
        UIPanel* p = &g_panels[order[i]];
        Rectangle r = {p->x, p->y, p->w, p->h};
        if (CheckCollisionPointRec(mouse, r)) return order[i];
    }
    return -1;
}

void ui_panels_update(void) {
    Vector2 mouse = GetMousePosition();

    // Hotkey toggles
    for (int i = 0; i < g_panelCount; i++) {
        if (g_panels[i].hotkey && IsKeyPressed(g_panels[i].hotkey)) {
            ui_panel_toggle(i);
        }
    }

    // Handle ongoing drag
    if (g_panelDragIdx >= 0) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            UIPanel* p = &g_panels[g_panelDragIdx];
            p->x = mouse.x - g_panelDragOffX;
            p->y = mouse.y - g_panelDragOffY;
            // Clamp to screen
            if (p->x < 0) p->x = 0;
            if (p->y < 0) p->y = 0;
            if (p->x + p->w > GetScreenWidth()) p->x = GetScreenWidth() - p->w;
            if (p->y + UI_PANEL_TITLE_H > GetScreenHeight()) p->y = GetScreenHeight() - UI_PANEL_TITLE_H;
            g_ui_clickConsumed = true;
        } else {
            g_panelDragIdx = -1;
        }
        return; // Don't process other clicks while dragging
    }

    // Handle ongoing resize
    if (g_panelResizeIdx >= 0) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            UIPanel* p = &g_panels[g_panelResizeIdx];
            p->w = mouse.x - p->x + g_panelResizeOffX;
            p->h = mouse.y - p->y + g_panelResizeOffY;
            if (p->w < p->minW) p->w = p->minW;
            if (p->h < p->minH) p->h = p->minH;
            g_ui_clickConsumed = true;
        } else {
            g_panelResizeIdx = -1;
        }
        return;
    }

    // Click handling
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int hit = ui_panel_hit_test(mouse);
        if (hit >= 0) {
            UIPanel* p = &g_panels[hit];
            ui_panel_focus(hit);

            Rectangle titleBar = {p->x, p->y, p->w, UI_PANEL_TITLE_H};

            // Close button (top-right of title bar)
            if (p->flags & UI_PANEL_CLOSABLE) {
                Rectangle closeBtn = {p->x + p->w - 20, p->y + 2, 18, 18};
                if (CheckCollisionPointRec(mouse, closeBtn)) {
                    p->open = false;
                    g_ui_clickConsumed = true;
                    return;
                }
            }

            // Resize handle (bottom-right corner, 14x14)
            if (p->flags & UI_PANEL_RESIZABLE) {
                Rectangle resizeHandle = {p->x + p->w - 14, p->y + p->h - 14, 14, 14};
                if (CheckCollisionPointRec(mouse, resizeHandle)) {
                    g_panelResizeIdx = hit;
                    g_panelResizeOffX = (p->x + p->w) - mouse.x;
                    g_panelResizeOffY = (p->y + p->h) - mouse.y;
                    g_ui_clickConsumed = true;
                    return;
                }
            }

            // Title bar drag
            if ((p->flags & UI_PANEL_DRAGGABLE) && CheckCollisionPointRec(mouse, titleBar)) {
                g_panelDragIdx = hit;
                g_panelDragOffX = mouse.x - p->x;
                g_panelDragOffY = mouse.y - p->y;
                g_ui_clickConsumed = true;
                return;
            }
        }
    }
}

bool ui_panels_wants_mouse(void) {
    if (g_panelDragIdx >= 0 || g_panelResizeIdx >= 0) return true;
    Vector2 mouse = GetMousePosition();
    return ui_panel_hit_test(mouse) >= 0;
}

void ui_panels_draw(void) {
    int order[UI_MAX_PANELS];
    int count;
    ui_panels_sorted(order, &count);

    for (int i = 0; i < count; i++) {
        UIPanel* p = &g_panels[order[i]];

        // Panel background
        DrawRectangle((int)p->x, (int)p->y, (int)p->w, (int)p->h, (Color){28, 28, 34, 240});

        // Title bar
        Color titleBg = (order[i] == ui_panel_hit_test(GetMousePosition()) || g_panelDragIdx == order[i])
            ? (Color){50, 55, 70, 255} : (Color){38, 40, 50, 255};
        DrawRectangle((int)p->x, (int)p->y, (int)p->w, UI_PANEL_TITLE_H, titleBg);
        DrawTextShadow(p->title, (int)p->x + 8, (int)p->y + 4, 14, ORANGE);

        // Hotkey hint
        if (p->hotkey) {
            const char* keyName = TextFormat("[%c]", p->hotkey >= KEY_A && p->hotkey <= KEY_Z
                ? 'A' + (p->hotkey - KEY_A) : '?');
            int kw = MeasureTextUI(keyName, 11);
            DrawTextShadow(keyName, (int)(p->x + p->w - 24 - kw), (int)p->y + 6, 11, GRAY);
        }

        // Close button
        if (p->flags & UI_PANEL_CLOSABLE) {
            int cx = (int)(p->x + p->w - 18);
            int cy = (int)p->y + 4;
            Vector2 mouse = GetMousePosition();
            Rectangle closeRect = {(float)cx - 2, (float)cy - 2, 18, 18};
            bool closeHovered = CheckCollisionPointRec(mouse, closeRect);
            DrawTextShadow("x", cx, cy, 14, closeHovered ? RED : GRAY);
        }

        // Border
        DrawRectangleLinesEx((Rectangle){p->x, p->y, p->w, p->h}, 1, (Color){60, 60, 75, 255});

        // Separator under title
        DrawLine((int)p->x, (int)p->y + UI_PANEL_TITLE_H, (int)(p->x + p->w), (int)p->y + UI_PANEL_TITLE_H,
                 (Color){60, 60, 75, 255});

        // Content area
        float cx = p->x + UI_PANEL_PAD;
        float cy = p->y + UI_PANEL_TITLE_H + UI_PANEL_PAD;
        float cw = p->w - UI_PANEL_PAD * 2;
        float ch = p->h - UI_PANEL_TITLE_H - UI_PANEL_PAD * 2;
        if (p->draw) p->draw(cx, cy, cw, ch);

        // Resize handle
        if (p->flags & UI_PANEL_RESIZABLE) {
            int rx = (int)(p->x + p->w - 12);
            int ry = (int)(p->y + p->h - 12);
            for (int d = 0; d < 3; d++) {
                int off = d * 4;
                DrawLine(rx + off, ry + 10, rx + 10, ry + off, (Color){80, 80, 100, 255});
            }
        }
    }
}

void ui_panels_draw_legend(float x, float y) {
    Vector2 mouse = GetMousePosition();
    float px = x;
    int fontSize = 14;
    int btnH = 20;
    int btnPad = 6;

    for (int i = 0; i < g_panelCount; i++) {
        UIPanel* p = &g_panels[i];
        if (!p->hotkey) continue;

        char keyChar = (p->hotkey >= KEY_A && p->hotkey <= KEY_Z)
            ? 'A' + (p->hotkey - KEY_A) : '?';

        const char* text = TextFormat("%c %s", keyChar, p->title);
        int tw = MeasureTextUI(text, fontSize);
        int btnW = tw + btnPad * 2;

        Rectangle btn = {px, y, (float)btnW, (float)btnH};
        bool hovered = CheckCollisionPointRec(mouse, btn);

        // Background: lit up when open, dim when closed
        Color bg = p->open ? (Color){50, 55, 68, 255} : (Color){32, 33, 40, 255};
        if (hovered) { bg.r += 18; bg.g += 18; bg.b += 18; }
        DrawRectangleRec(btn, bg);

        // Underline when open
        if (p->open) {
            DrawRectangle((int)px, (int)y + btnH - 2, btnW, 2, ORANGE);
        }

        DrawRectangleLinesEx(btn, 1, p->open ? (Color){70, 70, 85, 255} : (Color){48, 48, 58, 255});

        Color textCol = p->open ? WHITE : (hovered ? LIGHTGRAY : GRAY);
        DrawTextShadow(text, (int)px + btnPad, (int)y + 3, fontSize, textCol);

        // Click to toggle
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            ui_panel_toggle(i);
            g_ui_clickConsumed = true;
        }

        px += btnW + 4;
    }
}

#endif // UI_IMPLEMENTATION
