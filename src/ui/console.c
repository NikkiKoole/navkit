#include "console.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static struct {
    bool open;

    // Scrollback ring buffer
    char lines[CON_MAX_SCROLLBACK][CON_MAX_LINE];
    Color lineColors[CON_MAX_SCROLLBACK];
    int lineHead;       // next write slot
    int lineCount;      // total stored (capped at CON_MAX_SCROLLBACK)
    int scrollOffset;   // 0 = bottom, positive = scrolled up

    // Input line
    char input[CON_MAX_LINE];
    int inputLen;
    int cursor;

    // Command history
    char history[CON_MAX_HISTORY][CON_MAX_LINE];
    int historyCount;   // total stored
    int historyHead;    // next write slot
    int historyIdx;     // browsing index (-1 = not browsing)
    char inputSaved[CON_MAX_LINE]; // saved input when browsing history

    // Cursor blink
    float blinkTimer;
    bool blinkOn;
} con;

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void Console_Init(void) {
    memset(&con, 0, sizeof(con));
    con.historyIdx = -1;
    con.blinkOn = true;
}

void Console_Toggle(void) {
    con.open = !con.open;
    if (con.open) {
        con.scrollOffset = 0;
        con.blinkTimer = 0;
        con.blinkOn = true;
    }
}

bool Console_IsOpen(void) {
    return con.open;
}

// ---------------------------------------------------------------------------
// Scrollback
// ---------------------------------------------------------------------------
void Console_Print(const char* text, Color color) {
    // Handle multi-line text (split on newlines)
    const char* start = text;
    while (*start) {
        const char* nl = strchr(start, '\n');
        int len = nl ? (int)(nl - start) : (int)strlen(start);
        if (len >= CON_MAX_LINE) len = CON_MAX_LINE - 1;

        char* slot = con.lines[con.lineHead];
        memcpy(slot, start, len);
        slot[len] = '\0';
        con.lineColors[con.lineHead] = color;

        con.lineHead = (con.lineHead + 1) % CON_MAX_SCROLLBACK;
        if (con.lineCount < CON_MAX_SCROLLBACK) con.lineCount++;

        if (!nl) break;
        start = nl + 1;
    }
}

void Console_Printf(Color color, const char* fmt, ...) {
    char buf[CON_MAX_LINE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Console_Print(buf, color);
}

// ---------------------------------------------------------------------------
// TraceLog callback
// ---------------------------------------------------------------------------
void Console_LogCallback(int logLevel, const char* text, va_list args) {
    char buf[CON_MAX_LINE];
    vsnprintf(buf, sizeof(buf), text, args);

    Color color;
    switch (logLevel) {
        case LOG_ERROR:   color = RED;       break;
        case LOG_WARNING: color = YELLOW;    break;
        case LOG_DEBUG:   color = GRAY;      break;
        default:          color = LIGHTGRAY; break;
    }
    Console_Print(buf, color);

    // Also print to stderr so terminal output still works
    fprintf(stderr, "%s\n", buf);
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------
static void HistoryPush(const char* line) {
    if (line[0] == '\0') return;
    strncpy(con.history[con.historyHead], line, CON_MAX_LINE - 1);
    con.history[con.historyHead][CON_MAX_LINE - 1] = '\0';
    con.historyHead = (con.historyHead + 1) % CON_MAX_HISTORY;
    if (con.historyCount < CON_MAX_HISTORY) con.historyCount++;
    con.historyIdx = -1;
}

static const char* HistoryGet(int idx) {
    // idx 0 = most recent, 1 = previous, etc.
    if (idx < 0 || idx >= con.historyCount) return NULL;
    int slot = (con.historyHead - 1 - idx + CON_MAX_HISTORY) % CON_MAX_HISTORY;
    return con.history[slot];
}

static void HistoryBrowse(int direction) {
    // direction: +1 = older, -1 = newer
    int newIdx = con.historyIdx + direction;

    if (newIdx < -1) newIdx = -1;
    if (newIdx >= con.historyCount) return; // can't go further back

    if (con.historyIdx == -1 && direction > 0) {
        // Save current input before browsing
        strncpy(con.inputSaved, con.input, CON_MAX_LINE);
    }

    con.historyIdx = newIdx;

    if (con.historyIdx == -1) {
        // Returned to current input
        strncpy(con.input, con.inputSaved, CON_MAX_LINE);
        con.inputLen = (int)strlen(con.input);
    } else {
        const char* hist = HistoryGet(con.historyIdx);
        if (hist) {
            strncpy(con.input, hist, CON_MAX_LINE - 1);
            con.input[CON_MAX_LINE - 1] = '\0';
            con.inputLen = (int)strlen(con.input);
        }
    }
    con.cursor = con.inputLen;
}

// ---------------------------------------------------------------------------
// Submit
// ---------------------------------------------------------------------------
static void Submit(void) {
    if (con.inputLen == 0) return;

    // Echo to scrollback
    char echo[CON_MAX_LINE + 4];
    snprintf(echo, sizeof(echo), "> %s", con.input);
    Console_Print(echo, WHITE);

    // Push to history
    HistoryPush(con.input);

    // TODO: command parsing goes here in Phase 2

    // Clear input
    con.input[0] = '\0';
    con.inputLen = 0;
    con.cursor = 0;
    con.scrollOffset = 0;
}

// ---------------------------------------------------------------------------
// Update (call when console is open)
// ---------------------------------------------------------------------------
void Console_Update(void) {
    if (!con.open) return;

    // Blink cursor
    con.blinkTimer += GetFrameTime();
    if (con.blinkTimer >= 0.5f) {
        con.blinkTimer -= 0.5f;
        con.blinkOn = !con.blinkOn;
    }

    // Character input
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        // Ignore backtick (would re-toggle console)
        if (ch == '`' || ch == '~') continue;
        if (ch < 32 || ch > 126) continue; // printable ASCII only

        if (con.inputLen < CON_MAX_LINE - 1) {
            // Insert at cursor
            memmove(&con.input[con.cursor + 1], &con.input[con.cursor],
                    con.inputLen - con.cursor + 1);
            con.input[con.cursor] = (char)ch;
            con.inputLen++;
            con.cursor++;
            con.blinkOn = true;
            con.blinkTimer = 0;
        }
    }

    // Special keys
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (con.cursor > 0) {
            memmove(&con.input[con.cursor - 1], &con.input[con.cursor],
                    con.inputLen - con.cursor + 1);
            con.cursor--;
            con.inputLen--;
            con.blinkOn = true;
            con.blinkTimer = 0;
        }
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (con.cursor < con.inputLen) {
            memmove(&con.input[con.cursor], &con.input[con.cursor + 1],
                    con.inputLen - con.cursor);
            con.inputLen--;
        }
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        if (con.cursor > 0) {
            con.cursor--;
            con.blinkOn = true;
            con.blinkTimer = 0;
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        if (con.cursor < con.inputLen) {
            con.cursor++;
            con.blinkOn = true;
            con.blinkTimer = 0;
        }
    }

    if (IsKeyPressed(KEY_HOME)) {
        con.cursor = 0;
        con.blinkOn = true;
        con.blinkTimer = 0;
    }

    if (IsKeyPressed(KEY_END)) {
        con.cursor = con.inputLen;
        con.blinkOn = true;
        con.blinkTimer = 0;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        Submit();
    }

    if (IsKeyPressed(KEY_UP)) {
        HistoryBrowse(+1);
    }

    if (IsKeyPressed(KEY_DOWN)) {
        HistoryBrowse(-1);
    }

    // Scroll
    int wheel = (int)GetMouseWheelMove();
    if (wheel != 0) {
        con.scrollOffset += wheel;
        if (con.scrollOffset < 0) con.scrollOffset = 0;
        int maxScroll = con.lineCount > 0 ? con.lineCount - 1 : 0;
        if (con.scrollOffset > maxScroll) con.scrollOffset = maxScroll;
    }

    // ESC closes console
    if (IsKeyPressed(KEY_ESCAPE)) {
        con.open = false;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void Console_Draw(void) {
    if (!con.open) return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int consoleH = screenH * 2 / 5;  // 40% of screen
    int fontSize = 16;
    int lineH = fontSize + 2;
    int padding = 8;

    // Background
    DrawRectangle(0, 0, screenW, consoleH, (Color){0, 0, 0, 200});
    DrawRectangle(0, consoleH, screenW, 2, (Color){100, 100, 100, 255}); // separator

    // Input line
    int inputY = consoleH - lineH - padding;
    DrawTextShadow(">", padding, inputY, fontSize, GREEN);
    int promptW = MeasureTextUI("> ", fontSize);

    // Draw input text
    if (con.inputLen > 0) {
        DrawTextShadow(con.input, padding + promptW, inputY, fontSize, WHITE);
    }

    // Draw cursor
    if (con.blinkOn) {
        // Measure text up to cursor position
        char tmp[CON_MAX_LINE];
        strncpy(tmp, con.input, con.cursor);
        tmp[con.cursor] = '\0';
        int cursorX = padding + promptW + MeasureTextUI(tmp, fontSize);
        DrawRectangle(cursorX, inputY, 2, fontSize, GREEN);
    }

    // Scrollback lines (draw bottom-up from above input line)
    int y = inputY - lineH;
    int visibleLines = (y - padding) / lineH;

    for (int i = 0; i < visibleLines && i + con.scrollOffset < con.lineCount; i++) {
        int lineIdx = (con.lineHead - 1 - i - con.scrollOffset + CON_MAX_SCROLLBACK) % CON_MAX_SCROLLBACK;
        DrawTextShadow(con.lines[lineIdx], padding, y, fontSize, con.lineColors[lineIdx]);
        y -= lineH;
        if (y < padding) break;
    }

    // Scroll indicator
    if (con.scrollOffset > 0) {
        const char* indicator = TextFormat("-- scrolled %d --", con.scrollOffset);
        int tw = MeasureTextUI(indicator, fontSize);
        DrawTextShadow(indicator, (screenW - tw) / 2, inputY + lineH - 2, fontSize - 2, GRAY);
    }
}
