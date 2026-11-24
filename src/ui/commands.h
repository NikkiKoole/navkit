// commands.h - the command table (dev / cheat commands as data)
// docs/todo/ui/find-palette.md "Data tidying" step 1. Each row (src/ui/commands.c) is one
// command with typed arguments (src/core/command_args.h). The console parses from it, `help`
// prints usage made from it, and the dev sidebar's command buttons run through it, so a button
// and its console command can't drift apart. A later find palette reads the same rows.
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include "../core/command_args.h"

int  CommandCount(void);
const CommandDef* CommandAt(int i);
const CommandDef* CommandFind(const char* name);      // case-insensitive, NULL = none
void Commands_RegisterWithConsole(void);              // each row becomes a console command
bool CommandRunLine(const char* line);                // "clear water": parse + run (false: bad line)

// Dev sidebar: a button that runs a command line (the sidebar keeps its own layout and labels)
bool  CommandButton(float x, float y, const char* label, const char* line);
float CommandButtonInline(float x, float y, const char* label, const char* line);   // returns width

#endif
