// find.h - fills the find index (src/core/find_index.h) from the game's tables: commands,
// tools (ACTION_REGISTRY), settings, selection verbs, items, scenarios, games.
// docs/todo/ui/find-palette.md. The console's `find <text>` searches it, `find /<path>` browses.
#ifndef FIND_H
#define FIND_H

#include "../core/find_index.h"

void FindBuildIndex(void);   // rebuild every row (cheap; call before a search or browse)

// Lines describing a row for lists: "Fire pit   tool   Tools/Work/Build/Workshop   W B H F"
void FindRowLine(int row, char* buf, int len);

#endif
