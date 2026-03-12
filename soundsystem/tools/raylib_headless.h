// raylib_headless.h — Stub raylib types & functions for headless compilation
// Provides just enough to compile daw.c without any GUI or audio device.
#ifndef RAYLIB_HEADLESS_H
#define RAYLIB_HEADLESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// TYPES
// ============================================================================

typedef struct Color { unsigned char r, g, b, a; } Color;
typedef struct Vector2 { float x, y; } Vector2;
typedef struct Rectangle { float x, y, width, height; } Rectangle;
typedef struct Image { void *data; int width, height, mipmaps, format; } Image;
typedef struct Texture { unsigned int id; int width, height, mipmaps, format; } Texture;
typedef Texture Texture2D;
typedef struct GlyphInfo { int value, offsetX, offsetY, advanceX; Image image; } GlyphInfo;
typedef struct Font { int baseSize, glyphCount, glyphPadding; Texture2D texture; Rectangle *recs; GlyphInfo *glyphs; } Font;

// Opaque audio types
typedef struct rAudioBuffer rAudioBuffer;
typedef struct rAudioProcessor rAudioProcessor;
typedef struct AudioStream {
    rAudioBuffer *buffer;
    rAudioProcessor *processor;
    unsigned int sampleRate, sampleSize, channels;
} AudioStream;

// ============================================================================
// COLOR CONSTANTS
// ============================================================================

#ifndef CLITERAL
#define CLITERAL(type) (type)
#endif

#define LIGHTGRAY  CLITERAL(Color){ 200, 200, 200, 255 }
#define GRAY       CLITERAL(Color){ 130, 130, 130, 255 }
#define DARKGRAY   CLITERAL(Color){ 80, 80, 80, 255 }
#define YELLOW     CLITERAL(Color){ 253, 249, 0, 255 }
#define GOLD       CLITERAL(Color){ 255, 203, 0, 255 }
#define ORANGE     CLITERAL(Color){ 255, 161, 0, 255 }
#define PINK       CLITERAL(Color){ 255, 109, 194, 255 }
#define RED        CLITERAL(Color){ 230, 41, 55, 255 }
#define MAROON     CLITERAL(Color){ 190, 33, 55, 255 }
#define GREEN      CLITERAL(Color){ 0, 228, 48, 255 }
#define LIME       CLITERAL(Color){ 0, 158, 47, 255 }
#define DARKGREEN  CLITERAL(Color){ 0, 117, 44, 255 }
#define SKYBLUE    CLITERAL(Color){ 102, 191, 255, 255 }
#define BLUE       CLITERAL(Color){ 0, 121, 241, 255 }
#define DARKBLUE   CLITERAL(Color){ 0, 82, 172, 255 }
#define PURPLE     CLITERAL(Color){ 200, 122, 255, 255 }
#define VIOLET     CLITERAL(Color){ 135, 60, 190, 255 }
#define DARKPURPLE CLITERAL(Color){ 112, 31, 126, 255 }
#define BEIGE      CLITERAL(Color){ 211, 176, 131, 255 }
#define BROWN      CLITERAL(Color){ 127, 106, 79, 255 }
#define WHITE      CLITERAL(Color){ 255, 255, 255, 255 }
#define BLACK      CLITERAL(Color){ 0, 0, 0, 255 }
#define BLANK      CLITERAL(Color){ 0, 0, 0, 0 }
#define MAGENTA    CLITERAL(Color){ 255, 0, 255, 255 }

// ============================================================================
// ENUMS
// ============================================================================

typedef enum {
    KEY_NULL = 0,
    KEY_APOSTROPHE = 39, KEY_COMMA = 44, KEY_MINUS = 45, KEY_PERIOD = 46, KEY_SLASH = 47,
    KEY_ZERO = 48, KEY_ONE = 49, KEY_TWO = 50, KEY_THREE = 51, KEY_FOUR = 52,
    KEY_FIVE = 53, KEY_SIX = 54, KEY_SEVEN = 55, KEY_EIGHT = 56, KEY_NINE = 57,
    KEY_SEMICOLON = 59, KEY_EQUAL = 61,
    KEY_A = 65, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_LEFT_BRACKET = 91, KEY_BACKSLASH = 92, KEY_RIGHT_BRACKET = 93, KEY_GRAVE = 96,
    KEY_SPACE = 32, KEY_ESCAPE = 256, KEY_ENTER = 257, KEY_TAB = 258, KEY_BACKSPACE = 259,
    KEY_INSERT = 260, KEY_DELETE = 261,
    KEY_RIGHT = 262, KEY_LEFT = 263, KEY_DOWN = 264, KEY_UP = 265,
    KEY_PAGE_UP = 266, KEY_PAGE_DOWN = 267, KEY_HOME = 268, KEY_END = 269,
    KEY_CAPS_LOCK = 280, KEY_SCROLL_LOCK = 281, KEY_NUM_LOCK = 282,
    KEY_PRINT_SCREEN = 283, KEY_PAUSE = 284,
    KEY_F1 = 290, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_LEFT_SHIFT = 340, KEY_LEFT_CONTROL = 341, KEY_LEFT_ALT = 342, KEY_LEFT_SUPER = 343,
    KEY_RIGHT_SHIFT = 344, KEY_RIGHT_CONTROL = 345, KEY_RIGHT_ALT = 346, KEY_RIGHT_SUPER = 347,
    KEY_KB_MENU = 348,
    KEY_KP_0 = 320, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_DECIMAL = 330, KEY_KP_DIVIDE, KEY_KP_MULTIPLY, KEY_KP_SUBTRACT, KEY_KP_ADD, KEY_KP_ENTER, KEY_KP_EQUAL,
    KEY_BACK = 4, KEY_MENU = 5, KEY_VOLUME_UP = 24, KEY_VOLUME_DOWN = 25,
} KeyboardKey;

typedef enum {
    MOUSE_BUTTON_LEFT = 0, MOUSE_BUTTON_RIGHT = 1, MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_SIDE = 3, MOUSE_BUTTON_EXTRA = 4, MOUSE_BUTTON_FORWARD = 5, MOUSE_BUTTON_BACK = 6,
} MouseButton;

#define MOUSE_LEFT_BUTTON   MOUSE_BUTTON_LEFT
#define MOUSE_RIGHT_BUTTON  MOUSE_BUTTON_RIGHT
#define MOUSE_MIDDLE_BUTTON MOUSE_BUTTON_MIDDLE

typedef enum {
    MOUSE_CURSOR_DEFAULT = 0, MOUSE_CURSOR_ARROW = 1, MOUSE_CURSOR_IBEAM = 2,
    MOUSE_CURSOR_CROSSHAIR = 3, MOUSE_CURSOR_POINTING_HAND = 4,
    MOUSE_CURSOR_RESIZE_EW = 5, MOUSE_CURSOR_RESIZE_NS = 6,
    MOUSE_CURSOR_RESIZE_NWSE = 7, MOUSE_CURSOR_RESIZE_NESW = 8,
    MOUSE_CURSOR_RESIZE_ALL = 9, MOUSE_CURSOR_NOT_ALLOWED = 10,
} MouseCursor;

typedef enum {
    LOG_ALL = 0, LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL, LOG_NONE,
} TraceLogLevel;

// ============================================================================
// STUB FUNCTIONS — all static inline to avoid link issues
// ============================================================================

// Window
static inline void InitWindow(int w, int h, const char *t) { (void)w; (void)h; (void)t; }
static inline bool WindowShouldClose(void) { return true; }
static inline void CloseWindow(void) {}
static inline void SetTargetFPS(int fps) { (void)fps; }

// Drawing
static inline void BeginDrawing(void) {}
static inline void EndDrawing(void) {}
static inline void ClearBackground(Color c) { (void)c; }
static inline void BeginScissorMode(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static inline void EndScissorMode(void) {}
static inline void DrawRectangle(int x, int y, int w, int h, Color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static inline void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
static inline void DrawRectangleLinesEx(Rectangle r, float t, Color c) { (void)r; (void)t; (void)c; }
static inline void DrawLine(int x1, int y1, int x2, int y2, Color c) { (void)x1; (void)y1; (void)x2; (void)y2; (void)c; }
static inline void DrawCircle(int x, int y, float r, Color c) { (void)x; (void)y; (void)r; (void)c; }
static inline void DrawText(const char *t, int x, int y, int s, Color c) { (void)t; (void)x; (void)y; (void)s; (void)c; }

// Text
static inline int MeasureText(const char *t, int s) { (void)s; return t ? (int)strlen(t) * 6 : 0; }
static inline const char *TextFormat(const char *fmt, ...) {
    static char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}
static inline void TraceLog(int level, const char *fmt, ...) { (void)level; (void)fmt; }

// Input
static inline bool IsKeyPressed(int k) { (void)k; return false; }
static inline bool IsKeyDown(int k) { (void)k; return false; }
static inline bool IsKeyReleased(int k) { (void)k; return false; }
static inline int GetCharPressed(void) { return 0; }
static inline Vector2 GetMousePosition(void) { return (Vector2){0, 0}; }
static inline bool IsMouseButtonPressed(int b) { (void)b; return false; }
static inline bool IsMouseButtonDown(int b) { (void)b; return false; }
static inline float GetMouseWheelMove(void) { return 0.0f; }
static inline void SetMouseCursor(int c) { (void)c; }
static inline bool CheckCollisionPointRec(Vector2 p, Rectangle r) { (void)p; (void)r; return false; }

// Timing
static inline float GetFrameTime(void) { return 0.016f; }
static inline double GetTime(void) { return 0.0; }
static inline int GetFPS(void) { return 60; }

// Audio
static inline void InitAudioDevice(void) {}
static inline void CloseAudioDevice(void) {}
static inline void SetAudioStreamBufferSizeDefault(int s) { (void)s; }
static inline AudioStream LoadAudioStream(unsigned int sr, unsigned int ss, unsigned int ch) {
    (void)sr; (void)ss; (void)ch;
    return (AudioStream){0};
}
static inline void UnloadAudioStream(AudioStream s) { (void)s; }
static inline void SetAudioStreamCallback(AudioStream s, void(*cb)(void*,unsigned int)) { (void)s; (void)cb; }
static inline void PlayAudioStream(AudioStream s) { (void)s; }

// Font
static inline Font LoadEmbeddedFont(void) { return (Font){0}; }
static inline void UnloadFont(Font f) { (void)f; }

#endif // RAYLIB_HEADLESS_H
