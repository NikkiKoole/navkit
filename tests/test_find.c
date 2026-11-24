// The find palette's index (src/core/find_index.c): browse tree, ranked search, synonyms,
// did-you-mean, recent and pinned. Rows are added here by hand; the game fills them from its
// tables in src/ui/find.c (game build; `find <text>` in the console).
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/find_index.h"
#include <string.h>

static bool test_verbose = false;

static void Rows(void) {
    FindClear();
    FindAdd(FIND_TOOL, 1, "Tools/Work/Build", "Wall", NULL, "W B W", NULL);
    FindAdd(FIND_TOOL, 2, "Tools/Work/Build/Workshop", "Fire pit", NULL, "W B H F", NULL);
    FindAdd(FIND_TOOL, 3, "Tools/Draw/Transport", "Train", NULL, "D N N", NULL);
    int clear = FindAdd(FIND_COMMAND, 0, "Commands/World/Clear", "clear", "clear", NULL, "Clear things");
    FindSetWords(clear, "items movers fire water");
    FindAdd(FIND_COMMAND, 1, "Commands/Simulation/Water", "rain", "rain", NULL, "Rain");
    FindAdd(FIND_COMMAND, 2, "Commands/World/Terrain", "terrain", "terrain", NULL, "Generate");
    FindAdd(FIND_SETTING, 0, "Settings/Simulation/Fire", "Spread Interval (GH)", "fireSpread", NULL, NULL);
    FindAdd(FIND_ITEM, 7, "Items", "Rain Coat", NULL, NULL, NULL);
    FindAdd(FIND_ITEM, 8, "Items", "Stone Axe", NULL, NULL, NULL);
}

static int Top(const char* q, int* n, bool* near) {
    int out[16];
    *n = FindSearch(q, out, 16, near);
    return *n > 0 ? out[0] : -1;
}

static bool Finds(const char* q, const char* label) {
    int out[16];
    int n = FindSearch(q, out, 16, NULL);
    for (int i = 0; i < n; i++) if (strcmp(FindRowAt(out[i])->label, label) == 0) return true;
    return false;
}

describe(find_tree) {
    it("lists the top branches in the order they were added") {
        Rows();
        FindChild k[16];
        int n = FindTreeChildren("", k, 16);
        expect(n == 4);
        expect(k[0].isBranch && strcmp(k[0].name, "Tools") == 0);
        expect(strcmp(k[1].name, "Commands") == 0 && strcmp(k[2].name, "Settings") == 0);
        expect(strcmp(k[3].name, "Items") == 0);
    }
    it("puts sub-branches before the rows of a level") {
        Rows();
        FindChild k[16];
        int n = FindTreeChildren("Tools/Work/Build", k, 16);
        expect(n == 2);
        expect(k[0].isBranch && strcmp(k[0].name, "Workshop") == 0);
        expect(!k[1].isBranch && strcmp(k[1].name, "Wall") == 0 && k[1].row == 0);
    }
    it("does not mistake a longer name for a child (Tools/Work vs Tools/Workshop)") {
        FindClear();
        FindAdd(FIND_TOOL, 1, "Tools/Workshop", "A", NULL, NULL, NULL);
        FindChild k[4];
        expect(FindTreeChildren("Tools/Work", k, 4) == 0);
    }
}

describe(find_search) {
    it("ranks the rain command above the Rain Coat") {
        Rows();
        int n; bool near;
        int top = Top("rain", &n, &near);
        expect(!near && top >= 0 && FindRowAt(top)->kind == FIND_COMMAND);
        expect(Finds("rain", "Rain Coat"));
    }
    it("matches only at the start of a word (rain is not in terrain or train)") {
        Rows();
        expect(!Finds("rain", "terrain") && !Finds("rain", "Train"));
        expect(Finds("pit", "Fire pit"));
    }
    it("needs every word to match") {
        Rows();
        expect(Finds("stone axe", "Stone Axe"));
        expect(!Finds("stone wall", "Stone Axe"));
    }
    it("finds a command by its choices (clear water)") {
        Rows();
        int n; bool near;
        int top = Top("clear water", &n, &near);
        expect(top >= 0 && strcmp(FindRowAt(top)->label, "clear") == 0);
    }
    it("finds a setting by its camelCase name") {
        Rows();
        expect(Finds("fire spread", "Spread Interval (GH)"));
        expect(Finds("firespread", "Spread Interval (GH)"));
    }
    it("knows synonyms (campfire = fire pit, delete = clear)") {
        Rows();
        expect(Finds("campfire", "Fire pit"));
        expect(Finds("delete", "clear"));
    }
    it("offers near misses when nothing matches (did you mean)") {
        Rows();
        int n; bool near;
        int top = Top("wal", &n, &near);   // 3 letters: a prefix, not a near miss
        expect(top >= 0 && !near && strcmp(FindRowAt(top)->label, "Wall") == 0);
        top = Top("fier pit", &n, &near);
        expect(near && top >= 0 && strcmp(FindRowAt(top)->label, "Fire pit") == 0);
        top = Top("xyzzy", &n, &near);
        expect(n == 0 && !near);
    }
}

describe(find_use) {
    it("puts recent rows first and keeps them across a rebuild") {
        Rows();
        FindForgetUse();
        int coat = FindRowOf(FIND_ITEM, 7);
        FindNoteUsed(coat);
        Rows();   // rebuilt: same rows, recent is kept by kind + ref
        int n; bool near;
        int top = Top("rain", &n, &near);
        expect(top == FindRowOf(FIND_ITEM, 7));
        int r[4];
        expect(FindRecent(r, 4) == 1 && r[0] == coat);
    }
    it("moves a row used again to the front, without duplicates") {
        Rows();
        FindForgetUse();
        FindNoteUsed(0); FindNoteUsed(1); FindNoteUsed(0);
        int r[4];
        expect(FindRecent(r, 4) == 2 && r[0] == 0 && r[1] == 1);
    }
    it("pins and unpins") {
        Rows();
        FindForgetUse();
        FindSetPinned(1, true);
        FindSetPinned(1, true);
        int p[4];
        expect(FindIsPinned(1) && FindPinned(p, 4) == 1 && p[0] == 1);
        FindSetPinned(1, false);
        expect(!FindIsPinned(1) && FindPinned(p, 4) == 0);
        FindForgetUse();
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(find_tree);
    test(find_search);
    test(find_use);
    return summary();
}
