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

// Mark mouse click as consumed (blocks click-through to game world)
void ui_consume_click(void);

// Mark custom UI element as hovered (blocks click-through next frame)
void ui_set_hovered(void);

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

// Draggable float value - returns true if changed
bool DraggableFloat(float x, float y, const char* label, float* value, float speed, float min, float max);

// Draggable int value - returns true if changed
bool DraggableInt(float x, float y, const char* label, int* value, float speed, int min, int max);

// Draggable int value with logarithmic scale - good for large ranges (1 to 100000+)
bool DraggableIntLog(float x, float y, const char* label, int* value, float speed, int min, int max);

// Toggle boolean - click to toggle
void ToggleBool(float x, float y, const char* label, bool* value);

// Push button - returns true when clicked
bool PushButton(float x, float y, const char* label);

// Cycle through options - click to advance
void CycleOption(float x, float y, const char* label, const char** options, int count, int* value);

// Collapsible section header - returns true if section is open
bool SectionHeader(float x, float y, const char* label, bool* open);

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
static bool g_ui_customHovered = false;  // For custom UI elements (like profiler)

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
           g_ui_buttonAnyHovered || g_ui_cycleAnyHovered || g_ui_customHovered;
}

void ui_begin_frame(void) {
    g_ui_draggableAnyHovered = false;
    g_ui_toggleAnyHovered = false;
    g_ui_buttonAnyHovered = false;
    g_ui_cycleAnyHovered = false;
    g_ui_clickConsumed = false;
    g_ui_customHovered = false;
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

bool DraggableIntLog(float x, float y, const char* label, int* value, float speed, int min, int max) {
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
    if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        *value = (*value - 1 + count) % count;
        g_ui_clickConsumed = true;
    }
}

bool SectionHeader(float x, float y, const char* label, bool* open) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[%c] %s", *open ? '-' : '+', label);

    int textWidth = MeasureText(buf, 14);
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

#endif // UI_IMPLEMENTATION
