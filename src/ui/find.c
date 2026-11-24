// find.c - fill the find index from the game's tables (see find.h)
#include "find.h"
#include "commands.h"
#include "../core/action_registry.h"
#include "../core/settings.h"
#include "../core/selection.h"
#include "../core/scenarios.h"
#include "../core/games.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// "FIRE PIT" / "REFUSE_PILE" -> "Fire pit" / "Refuse pile"
static void LabelFromName(const char* name, char* out, int len) {
    int i = 0;
    for (; name[i] && i < len - 1; i++) {
        char c = name[i] == '_' ? ' ' : name[i];
        out[i] = (char)(i == 0 ? toupper((unsigned char)c) : tolower((unsigned char)c));
    }
    out[i] = '\0';
}

static const char* ModeLabel(InputMode m) {
    switch (m) {
        case MODE_DRAW:    return "Draw";
        case MODE_WORK:    return "Work";
        case MODE_SANDBOX: return "Sandbox";
        default:           return NULL;
    }
}
static char ModeKey(InputMode m) {
    return m == MODE_DRAW ? 'D' : m == MODE_WORK ? 'W' : m == MODE_SANDBOX ? 'S' : 0;
}
static const char* SubLabel(WorkSubMode s) {
    switch (s) {
        case SUBMODE_DIG:     return "Dig";
        case SUBMODE_BUILD:   return "Build";
        case SUBMODE_HARVEST: return "Harvest";
        default:              return NULL;
    }
}
static char SubKey(WorkSubMode s) {
    return s == SUBMODE_DIG ? 'D' : s == SUBMODE_BUILD ? 'B' : s == SUBMODE_HARVEST ? 'H' : 0;
}

static void AddKey(char* keys, int len, char k) {
    if (!k) return;
    int n = (int)strlen(keys);
    if (n + 2 >= len) return;
    if (n) keys[n++] = ' ';
    keys[n++] = (char)toupper((unsigned char)k);
    keys[n] = '\0';
}

static bool HasChildren(const ActionDef* d) {
    const ActionDef* kids[2];
    return GetActionsForContext(d->requiredMode, d->requiredSubMode, d->action, kids, 2) > 0;
}

// A Draw tool with a Work twin is reached through the Work tool (now or by order, I flips it),
// so it gets no row of its own; the same goes for the Draw kinds under a twinned category.
static bool ReachedByTwin(const ActionDef* d) {
    if (d->requiredMode != MODE_DRAW) return false;
    if (d->twin != ACTION_NONE) return true;
    return d->parentAction != ACTION_NONE && GetActionDef(d->parentAction)->twin != ACTION_NONE;
}

static void AddTools(void) {
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* d = &ACTION_REGISTRY[i];
        const char* mode = ModeLabel(d->requiredMode);
        if (!mode || d->barKey == 0 || d->action == ACTION_NONE) continue;
        if (ReachedByTwin(d) || HasChildren(d)) continue;   // categories are branches
        char path[FIND_PATH_LEN], label[FIND_LABEL_LEN], keys[FIND_KEY_LEN] = "";
        int n = snprintf(path, sizeof(path), "Tools/%s", mode);
        AddKey(keys, sizeof(keys), ModeKey(d->requiredMode));
        const char* sub = SubLabel(d->requiredSubMode);
        if (sub) n += snprintf(path + n, sizeof(path) - (size_t)n, "/%s", sub);
        AddKey(keys, sizeof(keys), SubKey(d->requiredSubMode));
        if (d->parentAction != ACTION_NONE) {
            const ActionDef* p = GetActionDef(d->parentAction);
            char plabel[FIND_LABEL_LEN];
            LabelFromName(p->name, plabel, sizeof(plabel));
            snprintf(path + n, sizeof(path) - (size_t)n, "/%s", plabel);
            AddKey(keys, sizeof(keys), p->barKey);
        }
        AddKey(keys, sizeof(keys), d->barKey);
        LabelFromName(d->name, label, sizeof(label));
        FindAdd(FIND_TOOL, d->action, path, label, NULL, keys,
                d->twin != ACTION_NONE ? "Builds now or by order (I flips)" : NULL);
    }
}

static void AddCommands(void) {
    for (int i = 0; i < CommandCount(); i++) {
        const CommandDef* c = CommandAt(i);
        char path[FIND_PATH_LEN];
        snprintf(path, sizeof(path), "Commands/%s", c->group);
        int row = FindAdd(FIND_COMMAND, i, path, c->name, c->name, NULL, c->help);
        // Its choices are search words: "clear water" finds clear
        char words[FIND_WORDS_LEN] = "";
        int n = 0;
        for (int a = 0; a < c->argCount; a++)
            for (int k = 0; k < c->args[a].choiceCount && n < (int)sizeof(words) - 1; k++)
                n += snprintf(words + n, sizeof(words) - (size_t)n, "%s%s", n ? " " : "", c->args[a].choices[k]);
        FindSetWords(row, words);
    }
}

static void AddSettings(void) {
    for (int i = 0; i < SettingCount(); i++) {
        const Setting* s = SettingAt(i);
        char path[FIND_PATH_LEN];
        snprintf(path, sizeof(path), "Settings/%s", s->group);
        int row = FindAdd(FIND_SETTING, i, path, s->label, s->name, NULL, s->help);
        char words[FIND_WORDS_LEN] = "";
        int n = 0;
        for (int k = 0; k < s->optionCount && n < (int)sizeof(words) - 1; k++)
            n += snprintf(words + n, sizeof(words) - (size_t)n, "%s%s", n ? " " : "", s->options[k]);
        FindSetWords(row, words);
    }
}

static const char* SelKindLabel(SelectionKind k) {
    switch (k) {
        case SEL_MOVER:     return "Mover";
        case SEL_ANIMAL:    return "Animal";
        case SEL_BLUEPRINT: return "Blueprint";
        case SEL_WORKSHOP:  return "Workshop";
        case SEL_STOCKPILE: return "Stockpile";
        default:            return "Other";
    }
}

static const char* VerbKeyName(int key) {
    static char b[2];
    if (key >= KEY_A && key <= KEY_Z) { b[0] = (char)('A' + (key - KEY_A)); b[1] = 0; return b; }
    switch (key) {
        case KEY_EQUAL: return "=";
        case KEY_MINUS: return "-";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        default: return "";
    }
}

static void AddVerbs(void) {
    for (int i = 0; i < SelectionVerbCount(); i++) {
        const SelectionVerb* v = SelectionVerbAt(i);
        char path[FIND_PATH_LEN], key[FIND_KEY_LEN];
        snprintf(path, sizeof(path), "Selection/%s", SelKindLabel(v->kind));
        snprintf(key, sizeof(key), "%s%s", v->shift && v->key ? "Shift " : "", VerbKeyName(v->key));
        FindAdd(FIND_VERB, i, path, v->name, NULL, key, NULL);
    }
}

static void AddItems(void) {
    for (int t = 0; t < ITEM_TYPE_COUNT; t++)
        if (itemDefs[t].name && itemDefs[t].name[0])
            FindAdd(FIND_ITEM, t, "Items", itemDefs[t].name, NULL, NULL, "Spawn at the mouse");
}

static void AddScenarios(void) {
    for (int i = 0; i < ScenarioCount(); i++) {
        const Scenario* s = ScenarioAt(i);
        FindAdd(FIND_SCENARIO, i, "Scenarios", s->name, s->name, NULL, s->description);
    }
}

static void AddGames(void) {
    for (int i = 0; i < GameCount(); i++) {
        const GameDef* g = GameAt(i);
        FindAdd(FIND_GAME, i, "Games", g->label, g->name, NULL, g->description);
    }
}

void FindBuildIndex(void) {
    FindClear();
    AddTools();
    AddCommands();
    AddSettings();
    AddVerbs();
    AddItems();
    AddScenarios();
    AddGames();
}

void FindRowLine(int row, char* buf, int len) {
    const FindRow* r = FindRowAt(row);
    if (!r) { snprintf(buf, (size_t)len, "?"); return; }
    snprintf(buf, (size_t)len, "%-26s %-8s %-36s %s", r->label, FindKindName(r->kind), r->path, r->key);
}
