// settings.h - Tunable game settings: one table, one row per setting.
//
// A row says what the setting is (name, label, type, range, help). From that one row:
//   - the console registers it (get/set, --set on the command line, range-clamped),
//   - the dev sidebar draws it (DrawSettingsGroup in ui_panels.c draws a whole group).
//
// Adding a setting: add one row to the table in settings.c. Its group decides where it
// shows in the dev sidebar ("Simulation/Fire/Advanced" = the Advanced box under Fire).
// Hand-written sidebar parts (buttons, notes, live readouts) stay in ui_panels.c and
// call DrawSettingsGroup for the rows around them.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

// SETTING_CHOICE: an int index into options[] (sidebar cycles through the names).
typedef enum { SETTING_BOOL, SETTING_INT, SETTING_FLOAT, SETTING_CHOICE } SettingType;

typedef struct {
    const char* name;      // console/--set name, e.g. "fireSpread"
    const char* label;     // sidebar label, e.g. "Spread Interval (s)"
    const char* group;     // sidebar group path, e.g. "Simulation/Fire"
    SettingType type;
    void* ptr;             // bool*, int* or float* (matches type)
    float speed;           // drag speed in the sidebar (ignored for bools)
    float min, max;        // range; console clamps to it (ignored for bools)
    const char* help;      // one or two sentences: sidebar tooltip, console 'get'
    const char* (*describe)(void);   // optional: tooltip with live values (replaces help in the sidebar)
    void (*onChange)(float oldValue); // optional: run after the value changes (sidebar or console)
    const char** options;             // SETTING_CHOICE: option names
    int optionCount;
} Setting;

// Value of a setting as float (bools 0/1), for onChange and comparisons.
float SettingValue(const Setting* s);

int SettingCount(void);
const Setting* SettingAt(int index);
const Setting* SettingFind(const char* name);   // case-insensitive, NULL if unknown

// Tooltip text for the sidebar: describe() if the row has one, else help.
const char* SettingTooltip(const Setting* s);

// Register every row as a console variable (with range + help).
void Settings_RegisterConsoleVars(void);

#endif
