// command_args.c - argument specs, parsing, usage text (see command_args.h)
#include "command_args.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

int CommandChoiceIndex(const CmdArg* a, const char* choice) {
    for (int i = 0; i < a->choiceCount; i++)
        if (strcasecmp(a->choices[i], choice) == 0) return i;
    return -1;
}

static int Append(char* buf, int len, int at, const char* text) {
    if (at >= len) return at;
    int n = snprintf(buf + at, (size_t)(len - at), "%s", text);
    return at + (n > 0 ? n : 0);
}

// "<count 1-100>", "<items|movers>", "[z]", "<item>", "<name>"
static int ArgUsage(const CmdArg* a, char* buf, int len, int at) {
    char inner[256];
    int n = 0;
    switch (a->type) {
        case CARG_INT:
            if (a->hi > a->lo) n = snprintf(inner, sizeof(inner), "%s %d-%d", a->name, (int)a->lo, (int)a->hi);
            else n = snprintf(inner, sizeof(inner), "%s", a->name);
            break;
        case CARG_FLOAT:
            if (a->hi > a->lo) n = snprintf(inner, sizeof(inner), "%s %g-%g", a->name, a->lo, a->hi);
            else n = snprintf(inner, sizeof(inner), "%s", a->name);
            break;
        case CARG_CHOICE:
            for (int i = 0; i < a->choiceCount && n < (int)sizeof(inner) - 1; i++)
                n += snprintf(inner + n, sizeof(inner) - (size_t)n, "%s%s", i ? "|" : "", a->choices[i]);
            if (a->hi > a->lo && n < (int)sizeof(inner) - 1)
                n += snprintf(inner + n, sizeof(inner) - (size_t)n, "|%d-%d", (int)a->lo, (int)a->hi);
            break;
        case CARG_ITEM: n = snprintf(inner, sizeof(inner), "%s", a->name); break;
        case CARG_NAME: n = snprintf(inner, sizeof(inner), "%s|list", a->name); break;
        case CARG_TEXT: n = snprintf(inner, sizeof(inner), "%s...", a->name); break;
    }
    (void)n;
    at = Append(buf, len, at, a->optional ? "[" : "<");
    at = Append(buf, len, at, inner);
    return Append(buf, len, at, a->optional ? "]" : ">");
}

int CommandUsage(const CommandDef* d, char* buf, int len) {
    if (len <= 0) return 0;
    buf[0] = '\0';
    int at = Append(buf, len, 0, d->name);
    for (int i = 0; i < d->argCount; i++) {
        at = Append(buf, len, at, " ");
        at = ArgUsage(&d->args[i], buf, len, at);
    }
    return at;
}

// A whole string that is a number (strtol/strtof leave nothing behind)
static bool ParseInt(const char* s, int* out) {
    if (!s || !*s) return false;
    char* end;
    long v = strtol(s, &end, 10);
    if (*end) return false;
    *out = (int)v;
    return true;
}

static bool ParseFloat(const char* s, float* out) {
    if (!s || !*s) return false;
    char* end;
    float v = strtof(s, &end);
    if (*end) return false;
    *out = v;
    return true;
}

// Item type named `name`, else the first whose name starts with it (case-insensitive),
// ITEM_NONE = none. Exact first: "rope" must not pick a "Rope Coil" listed before "Rope".
static int FindItemType(const char* name) {
    for (int t = 0; t < ITEM_TYPE_COUNT; t++)
        if (itemDefs[t].name && strcasecmp(itemDefs[t].name, name) == 0) return t;
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        const char* p = itemDefs[t].name;
        const char* q = name;
        while (*p && *q && tolower((unsigned char)*p) == tolower((unsigned char)*q)) { p++; q++; }
        if (*q == '\0') return t;
    }
    return ITEM_NONE;
}

bool CommandParse(const CommandDef* d, int argc, const char** argv, CmdValue* out, char* err, int errLen) {
    err[0] = '\0';
    int at = 1;   // argv[0] is the command
    for (int k = 0; k < d->argCount; k++) {
        const CmdArg* a = &d->args[k];
        CmdValue* v = &out[k];
        memset(v, 0, sizeof(*v));
        v->choice = -1;
        if (at >= argc) {
            if (!a->optional) { snprintf(err, (size_t)errLen, "missing <%s>", a->name); return false; }
            v->i = (int)a->def;
            v->f = a->def;
            continue;
        }
        const char* s = argv[at++];
        v->given = true;
        v->s = s;
        switch (a->type) {
            case CARG_INT:
                if (!ParseInt(s, &v->i)) { snprintf(err, (size_t)errLen, "%s: not a whole number: %s", a->name, s); return false; }
                if (a->hi > a->lo && (v->i < (int)a->lo || v->i > (int)a->hi)) {
                    snprintf(err, (size_t)errLen, "%s must be %d-%d (got %d)", a->name, (int)a->lo, (int)a->hi, v->i);
                    return false;
                }
                v->f = (float)v->i;
                break;
            case CARG_FLOAT:
                if (!ParseFloat(s, &v->f)) { snprintf(err, (size_t)errLen, "%s: not a number: %s", a->name, s); return false; }
                if (a->hi > a->lo && (v->f < a->lo || v->f > a->hi)) {
                    snprintf(err, (size_t)errLen, "%s must be %g-%g (got %g)", a->name, a->lo, a->hi, v->f);
                    return false;
                }
                v->i = (int)v->f;
                break;
            case CARG_CHOICE:
                v->choice = CommandChoiceIndex(a, s);
                if (v->choice < 0) {
                    if (a->hi > a->lo && ParseInt(s, &v->i) && v->i >= (int)a->lo && v->i <= (int)a->hi) break;
                    snprintf(err, (size_t)errLen, "unknown %s: %s", a->name, s);
                    return false;
                }
                break;
            case CARG_TEXT:
            case CARG_ITEM: {
                // The rest of the line is the item name ("stone hammer") or the text
                static char joined[128];
                int n = 0;
                for (int j = at - 1; j < argc && n < (int)sizeof(joined) - 1; j++)
                    n += snprintf(joined + n, sizeof(joined) - (size_t)n, "%s%s", j > at - 1 ? " " : "", argv[j]);
                at = argc;
                v->s = joined;
                if (a->type == CARG_TEXT) break;
                v->i = FindItemType(joined);
                if (v->i == ITEM_NONE) { snprintf(err, (size_t)errLen, "unknown item: %s", joined); return false; }
                break;
            }
            case CARG_NAME:
                v->i = -1;
                for (int i = 0; a->nameCount && i < a->nameCount(); i++)
                    if (strcasecmp(a->nameAt(i), s) == 0) { v->i = i; break; }
                if (v->i < 0 && strcasecmp(s, "list") != 0) {
                    snprintf(err, (size_t)errLen, "unknown %s: %s (use '%s list')", a->name, s, d->name);
                    return false;
                }
                break;   // "list": v->i stays -1, the caller lists the names
        }
    }
    if (at < argc) { snprintf(err, (size_t)errLen, "too many arguments (%s ...)", argv[at]); return false; }
    return true;
}
