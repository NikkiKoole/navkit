// global_keys.c - the global key table (see global_keys.h)
#include "global_keys.h"
#include "../../vendor/raylib.h"
#include <string.h>

static const GlobalKey globalKeys[] = {
    { 0,          NULL,          "--- View ---",                                NULL,           false },
    { 0,          "Esc",         "Back / exit menu",                            NULL,           false },
    { KEY_PERIOD, ".",           "Level up",                                    "level up",     false },
    { KEY_COMMA,  ",",           "Level down",                                  "level down",   false },
    { 0,          "Scroll",      "Zoom in / out",                               NULL,           false },
    { 0,          "Middle-drag", "Pan view",                                    NULL,           false },
    { KEY_SPACE,  "Space",       "Pause / resume",                              "speed toggle", false },
    { KEY_F,      "F",           "Follow the hovered (or selected) mover",      "follow",       true  },
    { 0,          NULL,          "--- General ---",                             NULL,           false },
    { 0,          "L-click",     "Select a mover, animal, workshop, stockpile", NULL,           false },
    { 0,          "/",           "Find anything (over a thing: its actions)",   NULL,           false },
    { KEY_TAB,    "Tab",         "Dev overlay (tools, settings)",               "overlay",      false },
    { 0,          "`",           "Console (type help)",                         NULL,           false },
    { 0,          "I",           "Build tools: instant / by order (Work bar)",  NULL,           false },
    { KEY_F5,     "F5",          "Quick save",                                  "save",         false },
    { KEY_F6,     "F6",          "Quick load",                                  "load",         false },
    { KEY_F7,     "F7",          "State audit",                                 "audit",        false },
    { KEY_F8,     "F8",          "Dump the event log",                          "events",       false },
};
#define GLOBAL_KEY_COUNT ((int)(sizeof(globalKeys) / sizeof(globalKeys[0])))

int GlobalKeyCount(void) { return GLOBAL_KEY_COUNT; }
const GlobalKey* GlobalKeyAt(int i) { return (i >= 0 && i < GLOBAL_KEY_COUNT) ? &globalKeys[i] : NULL; }

const char* GlobalKeyForLine(const char* line) {
    for (int i = 0; i < GLOBAL_KEY_COUNT; i++)
        if (globalKeys[i].key && globalKeys[i].line && strcmp(globalKeys[i].line, line) == 0) return globalKeys[i].label;
    return NULL;
}
