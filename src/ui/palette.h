// palette.h - the find palette: / opens it (the dev / cheat layer, docs/todo/ui/find-palette.md).
// Empty: pinned + recent, then the browse tree of every table row (src/core/find_index.h).
// Typing searches. Picking a row does it: a tool arms (now or by order), a command runs (one
// with arguments takes them on the input line), a setting changes in place, an item becomes a
// spawn brush, a verb runs on the selection, a scenario or game starts.
#ifndef PALETTE_H
#define PALETTE_H

#include <stdbool.h>

bool Palette_IsOpen(void);
void Palette_Open(void);
void Palette_Close(void);
// Call every frame before the game's input. Opens on '/', handles the palette's keys and mouse
// while open, and the item brush after. True = it used the input this frame (skip HandleInput).
bool Palette_Update(void);
void Palette_Draw(void);      // the window and the brush label (call after the UI)

#endif
