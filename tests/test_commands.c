// Commands as data (src/core/command_args.c): argument specs, parsing, usage text.
// A local table here; the game's table is src/ui/commands.c (game build only).
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/command_args.h"
#include "../src/entities/items.h"
#include <string.h>

static bool test_verbose = false;

static const char* const sizes[] = { "32", "64", "128", "256" };
static const char* const rainCh[] = { "light", "medium", "heavy", "stop" };
static const char* const names[] = { "ladders", "trash", "heat" };
static int NameCount(void) { return 3; }
static const char* NameAt(int i) { return names[i]; }
static int ran;
static void Run(const CmdValue* v) { (void)v; ran++; }

static const CommandDef cmdGrid = { "grid", "World/Grid", "resize", 1,
    { { .name = "size", .type = CARG_CHOICE, .choices = sizes, .choiceCount = 4 } }, Run, NULL };
static const CommandDef cmdSpawn = { "spawn", "Entities/Items", "spawn items", 2,
    { { .name = "count", .type = CARG_INT, .lo = 1, .hi = 100 }, { .name = "item", .type = CARG_ITEM } }, Run, NULL };
static const CommandDef cmdTp = { "tp", "View", "camera", 3,
    { { .name = "x", .type = CARG_INT }, { .name = "y", .type = CARG_INT }, { .name = "z", .type = CARG_INT, .optional = true, .def = -1 } }, Run, NULL };
static const CommandDef cmdRain = { "rain", "Simulation/Water", "rain", 1,
    { { .name = "amount", .type = CARG_CHOICE, .optional = true, .choices = rainCh, .choiceCount = 4, .lo = 1, .hi = 100 } }, Run, NULL };
static const CommandDef cmdScenario = { "scenario", "Games", "start", 1,
    { { .name = "scenario", .type = CARG_NAME, .nameCount = NameCount, .nameAt = NameAt } }, Run, NULL };
static const CommandDef cmdFind = { "find", "Dev", "search", 1,
    { { .name = "text", .type = CARG_TEXT, .optional = true } }, Run, NULL };
static const CommandDef cmdSpeed = { "speed", "Time", "game speed", 1,
    { { .name = "x", .type = CARG_FLOAT, .lo = 0, .hi = 50 } }, Run, NULL };

#define PARSE(d, ...) do { const char* argv_[] = { __VA_ARGS__ }; \
    ok = CommandParse(&(d), (int)(sizeof(argv_) / sizeof(argv_[0])), argv_, v, err, sizeof(err)); } while (0)

describe(command_usage) {
    it("usage text comes from the spec") {
        char u[256];
        CommandUsage(&cmdGrid, u, sizeof(u));    expect(strcmp(u, "grid <32|64|128|256>") == 0);
        CommandUsage(&cmdSpawn, u, sizeof(u));   expect(strcmp(u, "spawn <count 1-100> <item>") == 0);
        CommandUsage(&cmdTp, u, sizeof(u));      expect(strcmp(u, "tp <x> <y> [z]") == 0);
        CommandUsage(&cmdRain, u, sizeof(u));    expect(strcmp(u, "rain [light|medium|heavy|stop|1-100]") == 0);
        CommandUsage(&cmdScenario, u, sizeof(u)); expect(strcmp(u, "scenario <scenario|list>") == 0);
    }
}

describe(command_parse) {
    CmdValue v[CMD_MAX_ARGS];
    char err[160];
    bool ok;

    it("choices: index, case-insensitive; unknown and missing are errors") {
        PARSE(cmdGrid, "grid", "128");   expect(ok && v[0].choice == 2);
        PARSE(cmdRain, "rain", "HEAVY"); expect(ok && v[0].choice == 2);
        PARSE(cmdGrid, "grid", "99");    expect(!ok && strstr(err, "unknown size") != NULL);
        PARSE(cmdGrid, "grid");          expect(!ok && strstr(err, "missing <size>") != NULL);
    }

    it("a choice can also take a number in its range; optional choice left out") {
        PARSE(cmdRain, "rain", "33");  expect(ok && v[0].choice == -1 && v[0].i == 33 && v[0].given);
        PARSE(cmdRain, "rain", "300"); expect(!ok);
        PARSE(cmdRain, "rain");        expect(ok && !v[0].given && v[0].choice == -1);
    }

    it("ints: range checked, not-a-number rejected, optional gets its default") {
        PARSE(cmdTp, "tp", "4", "7");        expect(ok && v[0].i == 4 && v[1].i == 7 && !v[2].given && v[2].i == -1);
        PARSE(cmdTp, "tp", "4", "7", "2");   expect(ok && v[2].given && v[2].i == 2);
        PARSE(cmdSpawn, "spawn", "0", "rock"); expect(!ok && strstr(err, "1-100") != NULL);
        PARSE(cmdTp, "tp", "4x", "7");       expect(!ok && strstr(err, "not a whole number") != NULL);
        PARSE(cmdTp, "tp", "1", "2", "3", "4"); expect(!ok && strstr(err, "too many") != NULL);
    }

    it("floats in range") {
        PARSE(cmdSpeed, "speed", "2.5"); expect(ok && v[0].f > 2.49f && v[0].f < 2.51f);
        PARSE(cmdSpeed, "speed", "51");  expect(!ok);
    }

    it("an item by name takes the rest of the line ('empty can', case-insensitive prefix)") {
        PARSE(cmdSpawn, "spawn", "3", "empty", "can"); expect(ok && v[0].i == 3 && v[1].i == ITEM_EMPTY_CAN);
        PARSE(cmdSpawn, "spawn", "1", "BACKPACK");     expect(ok && v[1].i == ITEM_BACKPACK);
        PARSE(cmdSpawn, "spawn", "1", "unobtainium");  expect(!ok && strstr(err, "unknown item") != NULL);
    }

    it("names from another table; 'list' parses (index -1) so the command can list them") {
        PARSE(cmdScenario, "scenario", "Trash"); expect(ok && v[0].i == 1);
        PARSE(cmdScenario, "scenario", "list");  expect(ok && v[0].i == -1);
        PARSE(cmdScenario, "scenario", "nope");  expect(!ok && strstr(err, "scenario list") != NULL);
        PARSE(cmdFind, "find", "fire", "pit");   expect(ok && v[0].given && strcmp(v[0].s, "fire pit") == 0);
        PARSE(cmdFind, "find");                  expect(ok && !v[0].given);
        char usage[64];
        CommandUsage(&cmdFind, usage, sizeof(usage));
        expect(strcmp(usage, "find [text...]") == 0);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(command_usage);
    test(command_parse);
    return summary();
}
