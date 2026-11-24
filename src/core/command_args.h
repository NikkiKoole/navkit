// command_args.h - commands as data: argument specs, parsing, usage text
// (docs/todo/ui/find-palette.md "Data tidying" step 1).
//
// A command is a row: name, group path, help, up to CMD_MAX_ARGS typed arguments and a run
// function. The console parses typed arguments from the spec, usage and help text are made
// from it, and the dev sidebar draws its buttons from it (src/ui/commands.c holds the table).
// A later find palette reads the same rows. This file is pure (no console, no drawing) so the
// parser is tested on its own (tests/test_commands.c).
#ifndef COMMAND_ARGS_H
#define COMMAND_ARGS_H

#include <stdbool.h>

#define CMD_MAX_ARGS 3

typedef enum {
    CARG_INT,       // a whole number in lo..hi
    CARG_FLOAT,     // a number in lo..hi
    CARG_CHOICE,    // one of choices[]; when hi > lo a number in lo..hi is accepted too (rain 1-100)
    CARG_ITEM,      // an item type by name (case-insensitive prefix, "stone hammer"): takes the rest of the line
    CARG_NAME,      // a name from another table (terrains, scenarios, games): nameCount / nameAt
} CmdArgType;

typedef struct {
    const char* name;            // shown in usage: "count", "what"
    CmdArgType type;
    bool optional;               // may be left out: the value is `def` (INT/FLOAT) or choice -1
    float lo, hi, def;
    const char* const* choices;  // CARG_CHOICE
    int choiceCount;
    int (*nameCount)(void);      // CARG_NAME
    const char* (*nameAt)(int i);
} CmdArg;

typedef struct {
    bool given;          // the argument was typed (not defaulted)
    int i;               // INT, ITEM (item type), NAME (index), CHOICE number fallback
    float f;             // FLOAT (INT too, as float)
    int choice;          // CHOICE index, -1 = a number was given (in i) or left out
    const char* s;       // the text as typed (NAME, CHOICE)
} CmdValue;

typedef struct CommandDef {
    const char* name;            // "clear"
    const char* group;           // "World/Clear": where the sidebar and a palette put it
    const char* help;            // one line
    int argCount;
    CmdArg args[CMD_MAX_ARGS];
    void (*run)(const CmdValue* v);
    void (*status)(void);        // optional: printed with the usage when a required argument is missing
} CommandDef;

// "clear <items|movers|...>", "spawn <count 1-100> <item>", "tp <x> <y> [z]"
int  CommandUsage(const CommandDef* d, char* buf, int len);
// Typed values from argv (argv[0] is the command name). false: *err says what was wrong.
bool CommandParse(const CommandDef* d, int argc, const char** argv, CmdValue* out, char* err, int errLen);
// Index of `choice` in the argument's choices (case-insensitive), -1 = not one of them
int  CommandChoiceIndex(const CmdArg* a, const char* choice);

#endif
