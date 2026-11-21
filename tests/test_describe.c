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
#include "../src/simulation/temperature.h"
#include "../src/entities/animals.h"
#include "../src/entities/workshops.h"
#include "../src/entities/tool_quality.h"
#include "../src/world/designations.h"
#include "../src/world/construction.h"
#include "../src/world/material.h"
#include "../src/simulation/water.h"
#include "../src/core/describe.h"
#include "../src/core/selection.h"
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

describe(describe_cell) {
    it("titles the cell and reports its temperature") {
        SetupFlat();
        gameMode = GAME_MODE_SANDBOX;   // everything explored
        InitTemperature();
        DescLines d;
        DescribeCell(4, 4, 1, &d);
        Dump(&d);
        expect(d.count > 0);
        if (d.count > 0) {
            expect(strcmp(d.lines[0].text, "Cell (4, 4, z1)") == 0);
            expect(d.lines[0].tone == DESC_TITLE);
        }
        expect(FindLine(&d, "Temp:") != NULL);
        expect(FindLine(&d, "Insulation:") != NULL);
    }

    it("marks a heat source in red tone") {
        SetupFlat();
        gameMode = GAME_MODE_SANDBOX;
        InitTemperature();
        SetHeatSource(4, 4, 1, true);
        DescLines d;
        DescribeCell(4, 4, 1, &d);
        const DescLine* h = FindLine(&d, "HEAT SOURCE");
        expect(h != NULL);
        if (h) expect(h->tone == DESC_BAD);
    }

    it("says only Unexplored under fog of war") {
        SetupFlat();
        gameMode = GAME_MODE_SURVIVAL;
        CLEAR_GRID_LAYER(unexploredGrid, 1);   // all unexplored
        DescLines d;
        DescribeCell(4, 4, 1, &d);
        expect(d.count == 1);
        if (d.count == 1) expect(strcmp(d.lines[0].text, "Unexplored") == 0);
        gameMode = GAME_MODE_SANDBOX;
    }

    it("returns nothing outside the map") {
        SetupFlat();
        DescLines d;
        DescribeCell(-1, 0, 0, &d);
        expect(d.count == 0);
        DescribeCell(0, 0, 99, &d);
        expect(d.count == 0);
    }
}

describe(describe_items_and_animals) {
    it("counts units on a cell and lists each item") {
        SetupFlat();
        float px = 4 * CELL_SIZE + CELL_SIZE * 0.5f, py = 4 * CELL_SIZE + CELL_SIZE * 0.5f;
        int a = SpawnItem(px, py, 1.0f, ITEM_ROCK);
        int b = SpawnItem(px, py, 1.0f, ITEM_STICKS);
        expect(a >= 0 && b >= 0);
        if (a < 0 || b < 0) return;
        items[b].stackCount = 3;
        int idx[2] = {a, b};
        DescLines d;
        DescribeItemsAt(idx, 2, 4, 4, &d);
        Dump(&d);
        expect(d.count == 3);
        expect(FindLine(&d, "Cell (4,4): 4 units") != NULL);
    }

    it("tints rotten food") {
        SetupFlat();
        int f = SpawnItem(CELL_SIZE * 2.5f, CELL_SIZE * 2.5f, 1.0f, ITEM_BERRIES);
        expect(f >= 0);
        if (f < 0) return;
        items[f].condition = CONDITION_ROTTEN;
        DescLines d = {0};
        DescribeItemLine(f, &d);
        expect(d.count == 1);
        if (d.count == 1) {
            expect(strstr(d.lines[0].text, "Rotten") != NULL);
            expect(d.lines[0].tone == DESC_SOFT_BAD);
        }
    }

    it("returns nothing for an inactive animal") {
        SetupFlat();
        DescLines d;
        DescribeAnimal(0, &d);
        expect(d.count == 0);
    }
}

describe(describe_world_things) {
    it("names every designation kind") {
        for (int t = DESIGNATION_NONE + 1; t < DESIGNATION_TYPE_COUNT; t++) {
            if (strcmp(DesignationDisplayName(t), "Unknown") == 0) printf("  no name for designation %d\n", t);
            expect(strcmp(DesignationDisplayName(t), "Unknown") != 0);
        }
    }

    it("describes a mining designation waiting for a miner") {
        SetupFlat();
        gameMode = GAME_MODE_SANDBOX;
        InitDesignations();
        GRID(5, 5, 1) = CELL_WALL;
        SetWallMaterial(5, 5, 1, MAT_GRANITE);
        toolRequirementsEnabled = false;
        expect(DesignateMine(5, 5, 1));
        DescLines d;
        DescribeDesignation(5, 5, 1, &d);
        Dump(&d);
        expect(d.count >= 3);
        if (d.count > 0) expect(strncmp(d.lines[0].text, "Mining (5,5,1)", 14) == 0);
        const DescLine* w = FindLine(&d, "Waiting for Miner");
        expect(w != NULL);
        if (w) expect(w->tone == DESC_MUTED);
        DescribeDesignation(6, 6, 1, &d);   // no designation there
        expect(d.count == 0);
    }

    it("describes water only where there is some") {
        SetupFlat();
        gameMode = GAME_MODE_SANDBOX;
        InitWater();
        DescLines d;
        DescribeWater(3, 3, 1, &d);
        expect(d.count == 0);
        SetWaterLevel(3, 3, 1, 5);
        DescribeWater(3, 3, 1, &d);
        Dump(&d);
        expect(FindLine(&d, "Level: 5/7") != NULL);
    }

    it("shows a new workshop blueprint as awaiting materials") {
        SetupFlat();
        InitDesignations();
        int bp = CreateWorkshopBlueprint(2, 2, 1, GetConstructionRecipeForWorkshopType(WORKSHOP_CAMPFIRE));
        expect(bp >= 0);
        if (bp < 0) return;
        DescLines d;
        DescribeBlueprint(bp, &d);
        Dump(&d);
        const DescLine* s = FindLine(&d, "Status: Awaiting materials");
        expect(s != NULL);
        if (s) expect(s->tone == DESC_WARN);
        expect(FindLine(&d, "Waiting for hauler") != NULL);
    }

    it("returns nothing for an unknown station") {
        DescLines d;
        DescribeStation(-1, &d);
        expect(d.count == 0);
    }
}

// Selection (src/core/selection.c): what a click picks and keeps
describe(click_selection) {
    it("picks the mover under the click, else nothing") {
        SetupFlat();
        int m = SpawnTestMover();   // centered in cell (3,3), z1
        Selection s = PickSelectionAt(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 1);
        expect(s.kind == SEL_MOVER && s.idx == m);
        s = PickSelectionAt(8 * CELL_SIZE, 8 * CELL_SIZE, 1);
        expect(s.kind == SEL_NONE);
        s = PickSelectionAt(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 2);   // other level
        expect(s.kind == SEL_NONE);
    }

    it("picks a blueprint by its cell") {
        SetupFlat();
        InitDesignations();
        int bp = CreateWorkshopBlueprint(6, 6, 1, GetConstructionRecipeForWorkshopType(WORKSHOP_CAMPFIRE));
        expect(bp >= 0);
        Selection s = PickSelectionAt(6 * CELL_SIZE + 2, 6 * CELL_SIZE + 2, 1);
        expect(s.kind == SEL_BLUEPRINT && s.idx == bp);
    }

    it("clears itself when the selected mover is gone") {
        SetupFlat();
        int m = SpawnTestMover();
        Select(SEL_MOVER, m);
        expect(SelectionValid());
        movers[m].active = false;
        expect(!SelectionValid());
        expect(selection.kind == SEL_NONE);
    }

    it("drafts and undrafts a mover") {
        SetupFlat();
        int m = SpawnTestMover();
        expect(!movers[m].isDrafted);
        ToggleMoverDraft(m);
        expect(movers[m].isDrafted);
        ToggleMoverDraft(m);
        expect(!movers[m].isDrafted);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(need_labels);
    test(describe_mover);
    test(describe_cell);
    test(describe_items_and_animals);
    test(describe_world_things);
    test(click_selection);
    return summary();
}
