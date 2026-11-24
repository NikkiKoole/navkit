// test_action_registry.c - Hotkey sanity for the tool menus (src/core/action_registry.c)
//
// Keys are matched case-insensitively and the first sibling in the registry wins,
// so two tools in one menu with the same key make the second one unreachable by key.
#include "c89spec.h"
#include "../src/core/action_registry.c"   // not in test_unity.o (input-side data)
#include "../src/entities/stockpiles.h"      // FILTER_CATEGORY_KEYS (data in test_unity.o)
#include "../src/core/selection.h"          // selection verbs (data in test_unity.o)
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

// Draw tool <-> Work tool (find-palette.md step 3): the twin links are symmetric, join a Draw
// row to a Work row, and cover every action that exists in both (ACTION_DRAW_X / ACTION_WORK_X)
describe(action_twins) {
    it("twins point at each other, one Draw and one Work") {
        int linked = 0;
        for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
            const ActionDef* d = &ACTION_REGISTRY[i];
            if (d->twin == ACTION_NONE) continue;
            const ActionDef* t = GetActionDef(d->twin);
            expect(t->twin == d->action);
            expect((d->requiredMode == MODE_DRAW && t->requiredMode == MODE_WORK) ||
                   (d->requiredMode == MODE_WORK && t->requiredMode == MODE_DRAW));
            linked++;
        }
        expect(linked == 48);   // 23 + the workshop category + wall/construct, both ways
    }
    it("the Draw bar leaves out twinned tools (picked in the Work bar, now or by order)") {
        const ActionDef* defs[64];
        int n = GetActionsForContext(MODE_DRAW, SUBMODE_NONE, ACTION_NONE, defs, 64);
        expect(n > 0);   // stockpile, refuse pile, soil, train stay
        for (int i = 0; i < n; i++) { expect(defs[i]->twin == ACTION_NONE); }
        // Inside the Draw workshop category (reached by flipping) its kinds are still listed
        expect(GetActionsForContext(MODE_DRAW, SUBMODE_NONE, ACTION_DRAW_WORKSHOP, defs, 64) == 19);
    }
    it("no key clash: I (the flip) is no bar key") {
        for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) { expect(ACTION_REGISTRY[i].barKey != 'i'); }
    }
}

// Selection verbs (selection.c, find-palette.md step 4): every verb runs something, and within a
// kind no two keys collide (same key with the same Shift state)
describe(selection_verbs) {
    it("every verb has a name and a run function") {
        expect(SelectionVerbCount() > 0);
        for (int i = 0; i < SelectionVerbCount(); i++) {
            const SelectionVerb* v = SelectionVerbAt(i);
            expect(v->name != NULL && v->run != NULL);
        }
    }
    it("no two verbs of one kind share a key (with the same Shift state)") {
        for (int i = 0; i < SelectionVerbCount(); i++)
            for (int j = i + 1; j < SelectionVerbCount(); j++) {
                const SelectionVerb* a = SelectionVerbAt(i);
                const SelectionVerb* b = SelectionVerbAt(j);
                if (a->kind != b->kind || a->shift != b->shift) continue;
                int ka[2] = { a->key, a->altKey }, kb[2] = { b->key, b->altKey };
                for (int x = 0; x < 2; x++)
                    for (int y = 0; y < 2; y++) { if (ka[x]) { expect(ka[x] != kb[y]); } }
            }
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(action_registry_keys);
    test(stockpile_filter_keys);
    test(action_twins);
    test(selection_verbs);
    return summary();
}
