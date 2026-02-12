#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdarg.h>

#define CON_MAX_SCROLLBACK 128
#define CON_MAX_LINE       256
#define CON_MAX_HISTORY    32

void Console_Init(void);
void Console_Toggle(void);
bool Console_IsOpen(void);
void Console_Update(void);   // Handle text input, keys (call when console is open)
void Console_Draw(void);     // Render overlay (call every frame)

void Console_Print(const char* text, Color color);
void Console_Printf(Color color, const char* fmt, ...);

// Set as raylib TraceLog callback via SetTraceLogCallback()
void Console_LogCallback(int logLevel, const char* text, va_list args);

#endif
