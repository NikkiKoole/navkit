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

// ============================================================================
// TOOLTIP VARIANTS - same as above but show tooltip on hover
// ============================================================================

// Draggable int with tooltip
bool DraggableIntT(float x, float y, const char* label, int* value, float speed, int min, int max, const char* tooltip);

// Toggle boolean with tooltip
void ToggleBoolT(float x, float y, const char* label, bool* value, const char* tooltip);

// Draw pending tooltip (call at END of frame, after all UI)
void DrawTooltip(void);

// Measure text width using UI font
int MeasureTextUI(const char* text, int size);

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

bool DraggableIntT(float x, float y, const char* label, int* value, float speed, int min, int max, const char* tooltip) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", label, *value);

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) {
        g_ui_draggableAnyHovered = true;
        if (tooltip) ui_set_tooltip(tooltip);
    }

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

    int textWidth = MeasureText(buf, 18);
    Rectangle bounds = {x, y, (float)textWidth + 10, 20};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);

    if (hovered) {
        g_ui_toggleAnyHovered = true;
        if (tooltip) ui_set_tooltip(tooltip);
    }

    Color col = hovered ? YELLOW : LIGHTGRAY;
    DrawTextShadow(buf, (int)x, (int)y, 18, col);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !*value;
        g_ui_clickConsumed = true;
    }
}

#endif // UI_IMPLEMENTATION
