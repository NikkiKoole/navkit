// games.h - The games this engine hosts: one table, one row per game.
//
// A game is what the player picks: it builds its own world and sets its own rules
// (which needs are on, death rules via gameMode, speed, day length, intro).
// Scenarios (scenarios.h) are dev/test setups; games are player-facing.
//
// Started from the play HUD, the dev UI, the console ("game <name>"), or the CLI
// (--game <name>, also headless).
//
// The dev UI (TAB) is an overlay on top of any game: opening it does not change
// the game or its rules.
//
// Adding one: write a start function in games.c and add a row to the table there.
// The start function must set every rule toggle it relies on; it may not assume
// what the previous game left behind.
#ifndef GAMES_H
#define GAMES_H

#include <stdbool.h>

typedef struct {
    const char* name;          // CLI/console name, e.g. "survival"
    const char* label;         // button text, e.g. "Survival"
    const char* description;   // one line, shown in lists and tooltips
    void (*start)(void);       // builds the world and sets the rules
    bool devUI;                // dev overlay open when the game starts
} GameDef;

int GameCount(void);
const GameDef* GameAt(int index);
const GameDef* GameFind(const char* name);   // case-insensitive, NULL if unknown
const char** GameNames(void);                // GameCount() names, for autocomplete
const GameDef* GameCurrent(void);            // last game started, or NULL

// Start a game by name. Returns false (and prints the known names) if unknown.
bool StartGame(const char* name);
void GamePrintList(void);                    // names + descriptions to stdout

#endif
