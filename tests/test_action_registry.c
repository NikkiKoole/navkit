// test_action_registry.c - Hotkey sanity for the tool menus (src/core/action_registry.c)
//
// Keys are matched case-insensitively and the first sibling in the registry wins,
// so two tools in one menu with the same key make the second one unreachable by key.
#include "c89spec.h"
#include "../src/core/action_registry.c"   // not in test_unity.o (input-side data)
#include "../src/entities/stockpiles.h"      // FILTER_CATEGORY_KEYS (data in test_unity.o)
#include <ctype.h>
#include <stdio.h>

static bool test_verbose = false;

static bool SameMenu(const ActionDef* a, const ActionDef* b) {
    return a->requiredMode == b->requiredMode &&
           a->requiredSubMode == b->requiredSubMode &&
           a->parentAction == b->parentAction;
}

describe(action_registry_keys) {
    it("gives every tool in a menu its own key") {
        int clashes = 0;
        for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
            const ActionDef* a = &ACTION_REGISTRY[i];
            if (a->barKey == 0) continue;
            for (int j = i + 1; j < ACTION_REGISTRY_COUNT; j++) {
                const ActionDef* b = &ACTION_REGISTRY[j];
                if (b->barKey == 0 || !SameMenu(a, b)) continue;
                if (tolower((unsigned char)a->barKey) == tolower((unsigned char)b->barKey)) {
                    printf("  key '%c' used by both %s and %s (same menu)\n", a->barKey, a->name, b->name);
                    clashes++;
                }
            }
        }
        expect(clashes == 0);
    }

    it("underlines the letter that is the key") {
        int wrong = 0;
        for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
            const ActionDef* a = &ACTION_REGISTRY[i];
            if (a->barKey == 0 || a->barUnderlinePos < 0 || !a->barDisplayText) continue;
            int len = (int)strlen(a->barDisplayText);
            if (a->barUnderlinePos >= len ||
                tolower((unsigned char)a->barDisplayText[a->barUnderlinePos]) != tolower((unsigned char)a->barKey)) {
                printf("  %s: key '%c' but underline %d in \"%s\"\n",
                       a->name, a->barKey, a->barUnderlinePos, a->barDisplayText);
                wrong++;
            }
        }
        expect(wrong == 0);
    }
}

// Stockpile hover keys (input.c): a category key enters that category; X toggles all filters,
// so no category may use X (Textile did: toggle-all was unreachable) and no two may share a key
describe(stockpile_filter_keys) {
    it("category keys are unique letters and none is X") {
        for (int a = 0; a < FILTER_CAT_COUNT; a++) {
            char k = FILTER_CATEGORY_KEYS[a];
            expect(k >= 'A' && k <= 'Z');
            expect(k != 'X');   // toggles all filters
            expect(k != 'L');   // ends stockpile linking mode (checked while hovering a stockpile)
            for (int b = a + 1; b < FILTER_CAT_COUNT; b++) { expect(FILTER_CATEGORY_KEYS[b] != k); }
        }
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(action_registry_keys);
    test(stockpile_filter_keys);
    return summary();
}
