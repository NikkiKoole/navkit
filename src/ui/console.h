#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdarg.h>

// Forward declare raylib Color type (avoid full include in header)
typedef struct Color Color;

#define CON_MAX_SCROLLBACK 128
#define CON_MAX_LINE       256
#define CON_MAX_HISTORY    32
#define CON_MAX_COMMANDS   64
#define CON_MAX_VARS       512   // (the settings table alone has ~370 rows: past 256 the last ones were silently not console variables)

void Console_Init(void);
void Console_Toggle(void);
bool Console_IsOpen(void);
void Console_Update(void);   // Handle text input, keys (call when console is open)
void Console_Draw(void);     // Render overlay (call every frame)

void Console_Print(const char* text, Color color);
void Console_Printf(Color color, const char* fmt, ...);

// Set as raylib TraceLog callback via SetTraceLogCallback()
void Console_LogCallback(int logLevel, const char* text, va_list args);

// Command registry (Phase 2)
typedef void (*ConsoleCmdFn)(int argc, const char** argv);
void Console_RegisterCmd(const char* name, ConsoleCmdFn fn, const char* help);

// Variable registry (Phase 3)
typedef enum { CVAR_BOOL, CVAR_INT, CVAR_FLOAT } CVarType;
void Console_RegisterVar(const char* name, void* ptr, CVarType type);
// With a range (set clamps to [min, max]; ignored for bools or when min >= max), help text,
// and an optional hook run after set changes the value (gets the old value; bools 0/1).
void Console_RegisterVarEx(const char* name, void* ptr, CVarType type, float min, float max,
                           const char* help, void (*onChange)(float oldValue));
void Console_RegisterGameVars(void);  // Register common game variables
bool Console_SetVar(const char* name, const char* value);  // false if unknown or bad bool
const char* Console_GetVarString(const char* name);        // current value as text, NULL if unknown
void Console_Execute(const char* line);                     // run one command line (as typed)

#endif
