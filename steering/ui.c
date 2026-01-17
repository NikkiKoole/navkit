#include "ui.h"
#include <math.h>
#include <stddef.h>

// ============================================================================
// UI State
// ============================================================================

static Font* g_font = NULL;

// Draggable Value System (Blender-style click+drag)
static bool dragActive = false;
static float* dragTarget = NULL;
static float dragStartValue;
static float dragStartX;
static float dragSensitivity;
static float dragMinVal;
static float dragMaxVal;
static bool dragAnyHovered = false;
static bool toggleAnyHovered = false;

// ============================================================================
// Public Functions
// ============================================================================

void ui_init(Font* font) {
    g_font = font;
}

void DrawTextShadow(const char *text, int x, int y, int size, Color col) {
    if (g_font && g_font->texture.id > 0) {
        Vector2 pos = { (float)x, (float)y };
        DrawTextEx(*g_font, text, (Vector2){ pos.x + 1, pos.y + 1 }, (float)size, 1, BLACK);
        DrawTextEx(*g_font, text, pos, (float)size, 1, col);
    } else {
        DrawText(text, x + 1, y + 1, size, BLACK);
        DrawText(text, x, y, size, col);
    }
}

void DraggableFloat(float x, float y, const char* label, float* value, float sensitivity, float minVal, float maxVal) {
    const char* text = TextFormat("%s: %.1f", label, *value);
    Vector2 textSize = {0};
    if (g_font && g_font->texture.id > 0) {
        textSize = MeasureTextEx(*g_font, text, 18, 1);
    } else {
        textSize.x = MeasureText(text, 18);
        textSize.y = 18;
    }
    int width = (int)textSize.x + 10;
    Rectangle bounds = {x, y, width, textSize.y + 4};

    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) dragAnyHovered = true;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragActive = true;
        dragTarget = value;
        dragStartValue = *value;
        dragStartX = GetMouseX();
        dragSensitivity = sensitivity;
        dragMinVal = minVal;
        dragMaxVal = maxVal;
    }

    Color col = (hovered || dragTarget == value) ? YELLOW : LIGHTGRAY;
    if (g_font && g_font->texture.id > 0) {
        DrawTextEx(*g_font, text, (Vector2){x, y}, 18, 1, col);
    } else {
        DrawText(text, (int)x, (int)y, 18, col);
    }
}

void ToggleBool(float x, float y, const char* label, bool* value) {
    const char* text = TextFormat("[%c] %s", *value ? 'x' : ' ', label);
    Vector2 textSize = {0};
    if (g_font && g_font->texture.id > 0) {
        textSize = MeasureTextEx(*g_font, text, 18, 1);
    } else {
        textSize.x = MeasureText(text, 18);
        textSize.y = 18;
    }
    Rectangle bounds = {x, y, textSize.x + 10, textSize.y + 4};

    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) toggleAnyHovered = true;

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !(*value);
    }

    Color col = hovered ? YELLOW : LIGHTGRAY;
    if (g_font && g_font->texture.id > 0) {
        DrawTextEx(*g_font, text, (Vector2){x, y}, 18, 1, col);
    } else {
        DrawText(text, (int)x, (int)y, 18, col);
    }
}

void ui_update(void) {
    if (dragActive && dragTarget) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float delta = GetMouseX() - dragStartX;
            float newVal = dragStartValue + delta * dragSensitivity;
            if (dragMinVal == dragMinVal) newVal = fmaxf(newVal, dragMinVal);  // NAN != NAN
            if (dragMaxVal == dragMaxVal) newVal = fminf(newVal, dragMaxVal);
            *dragTarget = newVal;
        } else {
            dragActive = false;
            dragTarget = NULL;
        }
    }

    if (dragActive || dragAnyHovered) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
    } else if (toggleAnyHovered) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    // Reset for next frame
    dragAnyHovered = false;
    toggleAnyHovered = false;
}
