// find_index.h - the find palette's index: every table row as one searchable, browsable row
// (docs/todo/ui/find-palette.md). Pure: no drawing, no game state. Rows are added by
// src/ui/find.c from the game's tables (commands, tools, settings, items, verbs, scenarios,
// games); tests add their own.
//
// - Browse: rows sit in a tree by path ("Tools/Work/Build/Workshop"); FindTreeChildren lists a
//   level (sub-branches first, then the rows there).
// - Search: every query word must match the start of a word of the row (label, name split at
//   capitals, path, extra words such as a command's choices, or a synonym);
//   ranked by how well it matches, then kind (commands first), recent and pinned on top.
//   No match at all: did-you-mean by edit distance.
#ifndef FIND_INDEX_H
#define FIND_INDEX_H

#include <stdbool.h>

typedef enum {
    FIND_COMMAND,    // ref: command table index
    FIND_TOOL,       // ref: InputAction
    FIND_SETTING,    // ref: settings table index
    FIND_VERB,       // ref: selection verb index
    FIND_ITEM,       // ref: ItemType (a spawn brush)
    FIND_SCENARIO,   // ref: scenario index
    FIND_GAME,       // ref: game index
    FIND_KIND_COUNT
} FindKind;

#define FIND_MAX_ROWS   2048
#define FIND_PATH_LEN   80
#define FIND_LABEL_LEN  48
#define FIND_NAME_LEN   40
#define FIND_KEY_LEN    16
#define FIND_WORDS_LEN  160

typedef struct {
    FindKind kind;
    int ref;
    char path[FIND_PATH_LEN];     // branch, '/'-separated, no leading slash: "Settings/Simulation/Fire"
    char label[FIND_LABEL_LEN];   // what the row shows: "Fire pit", "Spread Interval (GH)"
    char name[FIND_NAME_LEN];     // machine name, searchable too: "fireSpread", "clear" ("" = none)
    char key[FIND_KEY_LEN];       // how to reach it by keys: "W B T F" ("" = none)
    const char* help;             // one line (not copied), NULL = none
    char words[FIND_WORDS_LEN];   // more searchable words, e.g. a command's choices ("water fire")
} FindRow;

void FindClear(void);
// Add a row; returns its index (-1: full). label/path/name/key are copied (truncated).
int  FindAdd(FindKind kind, int ref, const char* path, const char* label,
             const char* name, const char* key, const char* help);
void FindSetWords(int row, const char* words);   // the row's extra search words (copied)
int  FindCount(void);
const FindRow* FindRowAt(int i);
int  FindRowOf(FindKind kind, int ref);     // -1 = no such row
const char* FindKindName(FindKind kind);    // "command", "tool", ...

// Browse: the children of `path` ("" = the top). Sub-branches first (in the order their first
// row was added), then the rows whose path is exactly `path`.
typedef struct {
    bool isBranch;
    char name[FIND_LABEL_LEN];   // branch: its segment ("Work"); row: the row's label
    int row;                     // row index (rows), -1 for branches
} FindChild;
int  FindTreeChildren(const char* path, FindChild* out, int max);

// Search: row indices, best first. *didYouMean (may be NULL) is set when nothing matched every
// word and the results are near-misses by spelling instead.
int  FindSearch(const char* query, int* out, int max, bool* didYouMean);
int  FindScore(int row, const char* query);   // 0 = no match (for tests and debugging)

// Recent (most recently used first) and pinned rows: ranked first in a search, shown on top of
// an empty palette. Keyed by kind + ref, so they survive FindClear + rebuilding the rows.
#define FIND_MAX_RECENT 12
#define FIND_MAX_PINNED 32
void FindNoteUsed(int row);
int  FindRecent(int* out, int max);          // row indices, most recent first
void FindSetPinned(int row, bool pinned);
bool FindIsPinned(int row);
int  FindPinned(int* out, int max);          // row indices, in pin order
void FindForgetUse(void);                    // clear recent + pinned (tests)

// A row's stable name across builds ("Tools/Work/Build/Wall"): path + "/" + label. Used to save
// pins and recent to a file (src/ui/palette.c); a row that is gone simply isn't found.
void FindRowKey(int row, char* buf, int len);
int  FindRowByKey(const char* key);          // -1 = none

#endif
