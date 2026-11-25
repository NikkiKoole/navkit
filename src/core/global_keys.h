// global_keys.h - the keys that work everywhere, as data (docs/todo/ui/find-palette.md step 5)
// Each key runs a command line from the command table (src/ui/commands.c), so a key, its
// console command and its palette row are the same thing. Rows without a key (or without a
// line) are help only: the `?` panel lists every row, input.c runs the ones with both.
#ifndef GLOBAL_KEYS_H
#define GLOBAL_KEYS_H

#include <stdbool.h>

typedef struct {
    int key;              // raylib KEY_*, 0 = help-only row
    const char* label;    // how the help shows the key ("F5", "Middle-drag"); NULL = a heading
    const char* help;     // what it does (a heading's text when label is NULL)
    const char* line;     // command line it runs ("save"), NULL = handled elsewhere
    bool normalOnly;      // only in normal mode (F: the tools use F themselves)
} GlobalKey;

int GlobalKeyCount(void);
const GlobalKey* GlobalKeyAt(int i);
const char* GlobalKeyForLine(const char* line);   // the key label running exactly `line`, NULL = none

#endif
