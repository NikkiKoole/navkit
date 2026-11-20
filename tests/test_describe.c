// test_describe.c - Entity descriptions (src/core/describe.c): labels, tones, levels
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/mood.h"
#include "../src/core/describe.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

static void SetupFlat(void) {
    InitTestGrid(10, 10);
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            GRID(x, y, 0) = CELL_WALL;
            GRID(x, y, 1) = CELL_AIR;
        }
    ClearMovers();
    ClearItems();
    ClearJobs();
    InitBalance();
    bladderEnabled = false;
    moodEnabled = true;
}

static int SpawnTestMover(void) {
    int idx = moverCount++;
    Point goal = {3, 3, 1};
    InitMover(&movers[idx], 3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    return idx;
}

// Line that starts with `prefix`, or NULL
static const DescLine* FindLine(const DescLines* d, const char* prefix) {
    for (int i = 0; i < d->count; i++)
        if (strncmp(d->lines[i].text, prefix, strlen(prefix)) == 0) return &d->lines[i];
    return NULL;
}

static void Dump(const DescLines* d) {
    if (!test_verbose) return;
    for (int i = 0; i < d->count; i++) printf("    [%d] %s\n", d->lines[i].tone, d->lines[i].text);
}

describe(need_labels) {
    it("names the four hunger levels with rising urgency") {
        DescTone t;
        expect(strcmp(HungerLabel(0.9f, &t), "Full") == 0 && t == DESC_GOOD);
        expect(strcmp(HungerLabel(0.4f, &t), "Peckish") == 0 && t == DESC_MILD);
        expect(strcmp(HungerLabel(0.2f, &t), "Hungry") == 0 && t == DESC_WARN);
        expect(strcmp(HungerLabel(0.05f, &t), "Starving") == 0 && t == DESC_BAD);
    }

    it("uses the same thresholds for thirst, bladder and energy") {
        DescTone t;
        expect(strcmp(ThirstLabel(0.2f, &t), "Thirsty") == 0 && t == DESC_WARN);
        expect(strcmp(BladderLabel(0.05f, &t), "Desperate") == 0 && t == DESC_BAD);
        expect(strcmp(EnergyLabel(0.4f, &t), "Drowsy") == 0 && t == DESC_MILD);
    }

    it("reads body temperature against the balance thresholds") {
        InitBalance();
        DescTone t;
        expect(strcmp(BodyTempLabel(37.0f, &t), "Normal") == 0 && t == DESC_GOOD);
        expect(strcmp(BodyTempLabel(balance.severeColdThreshold - 1.0f, &t), "Hypothermic") == 0 && t == DESC_COLD);
    }
}

describe(describe_mover) {
    it("describes needs with label and tone") {
        SetupFlat();
        int m = SpawnTestMover();
        movers[m].hunger = 0.2f;
        DescLines d;
        DescribeMover(m, DESC_PLAYER, &d);
        Dump(&d);
        const DescLine* h = FindLine(&d, "Hunger: 20% (Hungry)");
        expect(h != NULL);
        if (h) expect(h->tone == DESC_WARN);
        expect(FindLine(&d, "Job: IDLE") != NULL);
    }

    it("keeps path and position out of the player view") {
        SetupFlat();
        int m = SpawnTestMover();
        DescLines player, debug;
        DescribeMover(m, DESC_PLAYER, &player);
        DescribeMover(m, DESC_DEBUG, &debug);
        expect(FindLine(&player, "Pos:") == NULL);
        expect(FindLine(&player, "Path:") == NULL);
        expect(FindLine(&debug, "Pos:") != NULL);
        expect(FindLine(&debug, "Path:") != NULL);
        expect(debug.count > player.count);
    }

    it("shows bladder only when bladder needs are on") {
        SetupFlat();
        int m = SpawnTestMover();
        DescLines d;
        DescribeMover(m, DESC_PLAYER, &d);
        expect(FindLine(&d, "Bladder:") == NULL);
        bladderEnabled = true;
        DescribeMover(m, DESC_PLAYER, &d);
        expect(FindLine(&d, "Bladder:") != NULL);
        bladderEnabled = false;
    }

    it("returns no lines for an inactive or out-of-range mover") {
        SetupFlat();
        DescLines d;
        DescribeMover(0, DESC_DEBUG, &d);
        expect(d.count == 0);
        DescribeMover(-1, DESC_DEBUG, &d);
        expect(d.count == 0);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(need_labels);
    test(describe_mover);
    return summary();
}
