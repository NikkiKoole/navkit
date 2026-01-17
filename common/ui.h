// Single-header UI library for raylib
// Usage: #define UI_IMPLEMENTATION in ONE .c file before including
#ifndef UI_H
#define UI_H

#include "../vendor/raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// Initialize UI (call once at startup)
void ui_init(Font* font);

// Call at end of frame to update cursor
void ui_update(void);

// Returns true if mouse is over any UI element (use to block clicks)
bool ui_wants_mouse(void);

// Call at start of UI drawing to reset flags
void ui_begin_frame(void);

// Draw text with shadow
void DrawTextShadow(const char* text, int x, int y, int size, Color col);

// Draggable float value - returns true if changed
bool DraggableFloat(float x, float y, const char* label, float* value, float speed, float min, float max);

// Draggable int value - returns true if changed
bool DraggableInt(float x, float y, const char* label, int* value, float speed, int min, int max);

// Toggle boolean - click to toggle
void ToggleBool(float x, float y, const char* label, bool* value);

// Push button - returns true when clicked
bool PushButton(float x, float y, const char* label);

// Cycle through options - click to advance
void CycleOption(float x, float y, const char* label, const char** options, int count, int* value);

#endif // UI_H

// ============================================================================
// IMPLEMENTATION
// ============================================================================
#ifdef UI_IMPLEMENTATION

static Font* g_ui_font = NULL;
static bool g_ui_draggableAnyHovered = false;
static bool g_ui_toggleAnyHovered = false;
static bool g_ui_buttonAnyHovered = false;
static bool g_ui_cycleAnyHovered = false;
static bool g_ui_clickConsumed = false;  // Set when UI consumes a click

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
    return g_ui_clickConsumed || g_ui_draggableAnyHovered || g_ui_toggleAnyHovered ||
           g_ui_buttonAnyHovered || g_ui_cycleAnyHovered;
}

void ui_begin_frame(void) {
    g_ui_draggableAnyHovered = false;
    g_ui_toggleAnyHovered = false;
    g_ui_buttonAnyHovered = false;
    g_ui_cycleAnyHovered = false;
    g_ui_clickConsumed = false;
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

bool DraggableFloat(float x, float y, const char* label, float* value, float speed, float min, float max) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %.2f", label, *value);

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_draggableAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, 18, col);

    static bool dragging = false;
    static float* dragTarget = NULL;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        g_ui_clickConsumed = true;
    }

    if (dragging && dragTarget == value) {
        float delta = GetMouseDelta().x * speed * 0.1f;
        *value += delta;
        if (*value < min) *value = min;
        if (*value > max) *value = max;

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
            dragTarget = NULL;
        }
        return true;
    }
    return false;
}

bool DraggableInt(float x, float y, const char* label, int* value, float speed, int min, int max) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", label, *value);

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_draggableAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, 18, col);

    static bool dragging = false;
    static int* dragTarget = NULL;
    static float accumulator = 0;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragging = true;
        dragTarget = value;
        accumulator = 0;
        g_ui_clickConsumed = true;
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
        }
        return true;
    }
    return false;
}

void ToggleBool(float x, float y, const char* label, bool* value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%c] %s", *value ? 'X' : ' ', label);

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_toggleAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, 18, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !*value;
        g_ui_clickConsumed = true;
    }
}

bool PushButton(float x, float y, const char* label) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%s]", label);

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_buttonAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, 18, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g_ui_clickConsumed = true;
        return true;
    }
    return false;
}

void CycleOption(float x, float y, const char* label, const char** options, int count, int* value) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: < %s >", label, options[*value]);

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) g_ui_cycleAnyHovered = true;

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, 18, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = (*value + 1) % count;
        g_ui_clickConsumed = true;
    }
}

#endif // UI_IMPLEMENTATION
