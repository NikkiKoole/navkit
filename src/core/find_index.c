// find_index.c - the find palette's index: rows, browse tree, ranked search (see find_index.h)
#include "find_index.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static FindRow rows[FIND_MAX_ROWS];
static int rowCount;

// Recent and pinned are kept by kind + ref (rows are rebuilt; a row index is not stable)
typedef struct { FindKind kind; int ref; } FindKey;
static FindKey recent[FIND_MAX_RECENT];
static int recentCount;
static FindKey pinned[FIND_MAX_PINNED];
static int pinnedCount;

static const char* const kindNames[FIND_KIND_COUNT] = {
    "command", "tool", "setting", "verb", "item", "scenario", "game"
};
// Ties go to what a developer most likely means: commands, then games / tools, ...
static const int kindWeight[FIND_KIND_COUNT] = { 60, 40, 20, 10, 0, 20, 40 };

// Words that mean the same thing here: a query word on the left also matches the right
static const struct { const char* word; const char* alias; } synonyms[] = {
    { "campfire", "fire pit" },  { "fire", "fire pit" },
    { "teleport", "tp" },        { "camera", "tp" },        { "goto", "tp" },
    { "delete", "clear" },       { "remove", "clear" },     { "erase", "clear" },  { "reset", "clear" },
    { "build", "construct" },    { "construct", "build" },
    { "settler", "mover" },      { "settlers", "mover" },   { "colonist", "mover" }, { "people", "mover" },
    { "person", "mover" },       { "villager", "mover" },
    { "toilet", "latrine" },     { "latrine", "toilet" },
    { "bed", "pile" },
    { "time", "speed" },         { "speed", "time" },
    { "water", "rain" },         { "storm", "thunder" },
    { "map", "terrain" },        { "world", "terrain" },    { "size", "grid" },
    { "fog", "reveal" },         { "explore", "reveal" },
    { "level", "scenario" },     { "demo", "scenario" },    { "mode", "game" },
};
#define SYNONYM_COUNT ((int)(sizeof(synonyms) / sizeof(synonyms[0])))

static void CopyStr(char* dst, int len, const char* src) {
    snprintf(dst, (size_t)len, "%s", src ? src : "");
}

void FindClear(void) { rowCount = 0; }

int FindAdd(FindKind kind, int ref, const char* path, const char* label,
            const char* name, const char* key, const char* help) {
    if (rowCount >= FIND_MAX_ROWS) return -1;
    FindRow* r = &rows[rowCount];
    r->kind = kind;
    r->ref = ref;
    CopyStr(r->path, FIND_PATH_LEN, path);
    CopyStr(r->label, FIND_LABEL_LEN, label);
    CopyStr(r->name, FIND_NAME_LEN, name);
    CopyStr(r->key, FIND_KEY_LEN, key);
    r->help = help;
    r->words[0] = '\0';
    return rowCount++;
}

void FindSetWords(int row, const char* words) {
    if (row >= 0 && row < rowCount) CopyStr(rows[row].words, FIND_WORDS_LEN, words);
}

int FindCount(void) { return rowCount; }
const FindRow* FindRowAt(int i) { return (i >= 0 && i < rowCount) ? &rows[i] : NULL; }

int FindRowOf(FindKind kind, int ref) {
    for (int i = 0; i < rowCount; i++) if (rows[i].kind == kind && rows[i].ref == ref) return i;
    return -1;
}

const char* FindKindName(FindKind kind) {
    return (kind >= 0 && kind < FIND_KIND_COUNT) ? kindNames[kind] : "?";
}

// ---------------------------------------------------------------------------
// Browse tree
// ---------------------------------------------------------------------------

// The segment of `rowPath` right below `path`, or false when the row is not below it
static bool NextSegment(const char* rowPath, const char* path, char* seg, int segLen) {
    size_t n = strlen(path);
    const char* rest;
    if (n == 0) rest = rowPath;
    else {
        if (strncmp(rowPath, path, n) != 0 || rowPath[n] != '/') return false;
        rest = rowPath + n + 1;
    }
    if (!*rest) return false;
    int k = 0;
    while (rest[k] && rest[k] != '/' && k < segLen - 1) { seg[k] = rest[k]; k++; }
    seg[k] = '\0';
    return true;
}

int FindTreeChildren(const char* path, FindChild* out, int max) {
    if (!path) path = "";
    int n = 0;
    for (int i = 0; i < rowCount && n < max; i++) {
        char seg[FIND_LABEL_LEN];
        if (!NextSegment(rows[i].path, path, seg, sizeof(seg))) continue;
        bool seen = false;
        for (int j = 0; j < n && !seen; j++) seen = strcmp(out[j].name, seg) == 0;
        if (seen) continue;
        out[n].isBranch = true;
        CopyStr(out[n].name, FIND_LABEL_LEN, seg);
        out[n].row = -1;
        n++;
    }
    for (int i = 0; i < rowCount && n < max; i++) {
        if (strcmp(rows[i].path, path) != 0) continue;
        out[n].isBranch = false;
        CopyStr(out[n].name, FIND_LABEL_LEN, rows[i].label);
        out[n].row = i;
        n++;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

static void Lower(char* dst, int len, const char* src) {
    int i = 0;
    for (; src && src[i] && i < len - 1; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static bool StartsWith(const char* s, const char* p) { return strncmp(s, p, strlen(p)) == 0; }

// Does a word of `s` (split on space, '/', '_', '(', '-') start with `p`?
static bool WordStartsWith(const char* s, const char* p) {
    for (const char* c = s; *c; c++) {
        bool start = (c == s) || strchr(" /_(-", c[-1]) != NULL;
        if (start && StartsWith(c, p)) return true;
    }
    return false;
}

typedef struct {
    char label[FIND_LABEL_LEN], name[FIND_NAME_LEN * 2], rawName[FIND_NAME_LEN];
    char path[FIND_PATH_LEN], words[FIND_WORDS_LEN];
} LowerRow;

// "fireSpread" -> "fire spread" (camelCase words become searchable on their own)
static void LowerSplitCamel(char* dst, int len, const char* src) {
    int k = 0;
    for (int i = 0; src[i] && k < len - 2; i++) {
        if (i > 0 && isupper((unsigned char)src[i]) && islower((unsigned char)src[i - 1])) dst[k++] = ' ';
        dst[k++] = (char)tolower((unsigned char)src[i]);
    }
    dst[k] = '\0';
}

static void LowerRowOf(const FindRow* r, LowerRow* l) {
    Lower(l->label, sizeof(l->label), r->label);
    LowerSplitCamel(l->name, sizeof(l->name), r->name);
    Lower(l->rawName, sizeof(l->rawName), r->name);
    Lower(l->path, sizeof(l->path), r->path);
    Lower(l->words, sizeof(l->words), r->words);
}

// How well one (lower-case) term matches the row, 0 = not at all. Only at the start of a word:
// "rain" should not find "terrain" or "train".
static int TermScore(const LowerRow* l, const char* t) {
    if (!*t) return 0;
    if (StartsWith(l->label, t)) return 120;
    if (WordStartsWith(l->label, t)) return 90;
    if (l->rawName[0] && StartsWith(l->rawName, t)) return 80;   // "firespread"
    if (l->name[0] && StartsWith(l->name, t)) return 80;
    if (l->name[0] && WordStartsWith(l->name, t)) return 60;
    if (l->words[0] && WordStartsWith(l->words, t)) return 40;
    if (WordStartsWith(l->path, t)) return 30;
    return 0;
}

// A query word: the word itself, or a synonym (counts 70%)
static int WordScore(const LowerRow* l, const char* w) {
    int best = TermScore(l, w);
    for (int s = 0; s < SYNONYM_COUNT; s++) {
        if (strcmp(synonyms[s].word, w) != 0) continue;
        int a = TermScore(l, synonyms[s].alias) * 7 / 10;
        if (a > best) best = a;
    }
    return best;
}

static int Split(char* q, char** words, int max) {
    int n = 0;
    for (char* tok = strtok(q, " \t"); tok && n < max; tok = strtok(NULL, " \t")) words[n++] = tok;
    return n;
}

static int KeyIndex(const FindKey* list, int count, FindKind kind, int ref) {
    for (int i = 0; i < count; i++) if (list[i].kind == kind && list[i].ref == ref) return i;
    return -1;
}

static int UseBonus(const FindRow* r) {
    int bonus = 0;
    int ri = KeyIndex(recent, recentCount, r->kind, r->ref);
    if (ri >= 0) bonus += 600 - 20 * ri;   // what you just used comes first
    if (KeyIndex(pinned, pinnedCount, r->kind, r->ref) >= 0) bonus += 400;
    return bonus;
}

int FindScore(int row, const char* query) {
    const FindRow* r = FindRowAt(row);
    if (!r || !query) return 0;
    char q[128], whole[128];
    Lower(q, sizeof(q), query);
    Lower(whole, sizeof(whole), query);
    char* words[8];
    int wc = Split(q, words, 8);
    if (wc == 0) return 0;
    LowerRow l;
    LowerRowOf(r, &l);
    int score = 0;
    for (int w = 0; w < wc; w++) {
        int s = WordScore(&l, words[w]);
        if (s == 0) return 0;   // every word must match
        score += s;
    }
    // The whole query: exactly the row, or the start of its label
    char* t = whole;
    while (*t == ' ') t++;
    if (strcmp(l.label, t) == 0 || (l.rawName[0] && strcmp(l.rawName, t) == 0)) score += 300;
    else if (StartsWith(l.label, t)) score += 100;
    return score + kindWeight[r->kind] + UseBonus(r);
}

// Edit distance (Levenshtein plus swapped neighbours), short strings only
static int EditDistance(const char* a, const char* b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > 31 || lb > 31) return 99;
    int d[32][32];
    for (int i = 0; i <= la; i++) d[i][0] = i;
    for (int j = 0; j <= lb; j++) d[0][j] = j;
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++) {
            int c = d[i - 1][j - 1] + (a[i - 1] != b[j - 1]);
            if (d[i - 1][j] + 1 < c) c = d[i - 1][j] + 1;
            if (d[i][j - 1] + 1 < c) c = d[i][j - 1] + 1;
            // Two letters swapped count as one slip ("fier" -> "fire")
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] && d[i - 2][j - 2] + 1 < c)
                c = d[i - 2][j - 2] + 1;
            d[i][j] = c;
        }
    return d[la][lb];
}

// Closest word of `s` (split like WordStartsWith) to `w`
static int ClosestWord(const char* s, const char* w) {
    int best = 99;
    char word[32];
    int k = 0;
    for (const char* c = s;; c++) {
        if (*c == '\0' || strchr(" /_(-)", *c)) {
            word[k] = '\0';
            if (k > 0) { int dd = EditDistance(word, w); if (dd < best) best = dd; }
            k = 0;
            if (*c == '\0') break;
        } else if (k < 31) word[k++] = *c;
    }
    return best;
}

// Near-miss score for did-you-mean, 0 = too far
static int NearScore(const FindRow* r, char** words, int wc) {
    LowerRow l;
    LowerRowOf(r, &l);
    int total = 0;
    for (int w = 0; w < wc; w++) {
        int len = (int)strlen(words[w]);
        int allowed = len <= 3 ? 0 : (len <= 5 ? 1 : 2);
        int dd = ClosestWord(l.label, words[w]);
        int dn = l.name[0] ? ClosestWord(l.name, words[w]) : 99;
        if (dn < dd) dd = dn;
        // A misspelt synonym ("setlers" -> settlers -> mover)
        for (int k = 0; k < SYNONYM_COUNT && dd > 0; k++) {
            int ds = EditDistance(synonyms[k].word, words[w]);
            if (ds < dd && TermScore(&l, synonyms[k].alias) > 0) dd = ds;
        }
        if (dd > allowed) return 0;
        total += dd;
    }
    return 100 - 30 * total + kindWeight[r->kind] + UseBonus(r);
}

// Keep the best `max` (index, score) pairs, best first
static int Insert(int* out, int* scores, int n, int max, int row, int score) {
    int pos = n;
    while (pos > 0 && scores[pos - 1] < score) pos--;
    if (pos >= max) return n;
    int last = n < max ? n : max - 1;
    for (int k = last; k > pos; k--) { out[k] = out[k - 1]; scores[k] = scores[k - 1]; }
    out[pos] = row;
    scores[pos] = score;
    return n < max ? n + 1 : n;
}

int FindSearch(const char* query, int* out, int max, bool* didYouMean) {
    if (didYouMean) *didYouMean = false;
    if (!query || max <= 0) return 0;
    int scores[256];
    if (max > 256) max = 256;
    int n = 0;
    for (int i = 0; i < rowCount; i++) {
        int s = FindScore(i, query);
        if (s > 0) n = Insert(out, scores, n, max, i, s);
    }
    if (n > 0) return n;
    char q[128];
    Lower(q, sizeof(q), query);
    char* words[8];
    int wc = Split(q, words, 8);
    if (wc == 0) return 0;
    for (int i = 0; i < rowCount; i++) {
        int s = NearScore(&rows[i], words, wc);
        if (s > 0) n = Insert(out, scores, n, max, i, s);
    }
    if (didYouMean && n > 0) *didYouMean = true;
    return n;
}

// ---------------------------------------------------------------------------
// Recent and pinned
// ---------------------------------------------------------------------------

void FindNoteUsed(int row) {
    const FindRow* r = FindRowAt(row);
    if (!r) return;
    int at = KeyIndex(recent, recentCount, r->kind, r->ref);
    if (at < 0) at = recentCount < FIND_MAX_RECENT ? recentCount++ : FIND_MAX_RECENT - 1;
    for (int k = at; k > 0; k--) recent[k] = recent[k - 1];
    recent[0] = (FindKey){ r->kind, r->ref };
}

static int RowsOf(const FindKey* list, int count, int* out, int max) {
    int n = 0;
    for (int i = 0; i < count && n < max; i++) {
        int row = FindRowOf(list[i].kind, list[i].ref);
        if (row >= 0) out[n++] = row;
    }
    return n;
}

int FindRecent(int* out, int max) { return RowsOf(recent, recentCount, out, max); }
int FindPinned(int* out, int max) { return RowsOf(pinned, pinnedCount, out, max); }

bool FindIsPinned(int row) {
    const FindRow* r = FindRowAt(row);
    return r && KeyIndex(pinned, pinnedCount, r->kind, r->ref) >= 0;
}

void FindSetPinned(int row, bool on) {
    const FindRow* r = FindRowAt(row);
    if (!r) return;
    int at = KeyIndex(pinned, pinnedCount, r->kind, r->ref);
    if (on && at < 0 && pinnedCount < FIND_MAX_PINNED) pinned[pinnedCount++] = (FindKey){ r->kind, r->ref };
    if (!on && at >= 0) {
        for (int k = at; k < pinnedCount - 1; k++) pinned[k] = pinned[k + 1];
        pinnedCount--;
    }
}

void FindForgetUse(void) { recentCount = 0; pinnedCount = 0; }
