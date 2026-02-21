#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/tool_quality.h"
#include "../src/entities/mover.h"
#include "test_helpers.h"
#include <string.h>

static bool test_verbose = false;

// ===========================================================================
// GetItemQualityLevel tests
// ===========================================================================
describe(quality_lookup) {
    it("should return hammer:1 for ITEM_ROCK") {
        expect(GetItemQualityLevel(ITEM_ROCK, QUALITY_HAMMERING) == 1);
    }

    it("should return 0 for ITEM_ROCK cutting quality") {
        expect(GetItemQualityLevel(ITEM_ROCK, QUALITY_CUTTING) == 0);
    }

    it("should return 0 for ITEM_ROCK digging quality") {
        expect(GetItemQualityLevel(ITEM_ROCK, QUALITY_DIGGING) == 0);
    }

    it("should return cutting:1 for ITEM_SHARP_STONE") {
        expect(GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_CUTTING) == 1);
    }

    it("should return fine:1 for ITEM_SHARP_STONE") {
        expect(GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_FINE) == 1);
    }

    it("should return 0 for ITEM_SHARP_STONE hammering quality") {
        expect(GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_HAMMERING) == 0);
    }

    it("should return 0 for non-tool items") {
        expect(GetItemQualityLevel(ITEM_STICKS, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_STICKS, QUALITY_HAMMERING) == 0);
        expect(GetItemQualityLevel(ITEM_STICKS, QUALITY_DIGGING) == 0);
        expect(GetItemQualityLevel(ITEM_LOG, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_PLANKS, QUALITY_SAWING) == 0);
    }

    it("should return 0 for invalid item types") {
        expect(GetItemQualityLevel(-1, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(ITEM_TYPE_COUNT, QUALITY_CUTTING) == 0);
        expect(GetItemQualityLevel(999, QUALITY_CUTTING) == 0);
    }
}

// ===========================================================================
// ItemHasAnyQuality tests
// ===========================================================================
describe(has_any_quality) {
    it("should return true for ITEM_ROCK") {
        expect(ItemHasAnyQuality(ITEM_ROCK) == true);
    }

    it("should return true for ITEM_SHARP_STONE") {
        expect(ItemHasAnyQuality(ITEM_SHARP_STONE) == true);
    }

    it("should return false for non-tool items") {
        expect(ItemHasAnyQuality(ITEM_STICKS) == false);
        expect(ItemHasAnyQuality(ITEM_LOG) == false);
        expect(ItemHasAnyQuality(ITEM_PLANKS) == false);
        expect(ItemHasAnyQuality(ITEM_CORDAGE) == false);
    }

    it("should return false for invalid types") {
        expect(ItemHasAnyQuality(-1) == false);
        expect(ItemHasAnyQuality(ITEM_TYPE_COUNT) == false);
    }
}

// ===========================================================================
// IF_TOOL flag tests
// ===========================================================================
describe(tool_flag) {
    it("should have IF_TOOL set on ITEM_ROCK") {
        expect(ItemIsTool(ITEM_ROCK) != 0);
    }

    it("should have IF_TOOL set on ITEM_SHARP_STONE") {
        expect(ItemIsTool(ITEM_SHARP_STONE) != 0);
    }

    it("should NOT have IF_TOOL on regular items") {
        expect(ItemIsTool(ITEM_STICKS) == 0);
        expect(ItemIsTool(ITEM_LOG) == 0);
        expect(ItemIsTool(ITEM_PLANKS) == 0);
        expect(ItemIsTool(ITEM_CORDAGE) == 0);
        expect(ItemIsTool(ITEM_DIRT) == 0);
    }
}

// ===========================================================================
// GetToolSpeedMultiplier tests - soft jobs
// ===========================================================================
describe(speed_multiplier_soft) {
    // Soft jobs: bare hands (level 0) = 0.5x, level 1 = 1.0x, level 2 = 1.5x, level 3 = 2.0x

    it("should return 0.5x for bare hands on soft job") {
        float mult = GetToolSpeedMultiplier(0, 0, true);
        expect(mult > 0.49f && mult < 0.51f);
    }

    it("should return 1.0x for level 1 tool on soft job") {
        float mult = GetToolSpeedMultiplier(1, 0, true);
        expect(mult > 0.99f && mult < 1.01f);
    }

    it("should return 1.5x for level 2 tool on soft job") {
        float mult = GetToolSpeedMultiplier(2, 0, true);
        expect(mult > 1.49f && mult < 1.51f);
    }

    it("should return 2.0x for level 3 tool on soft job") {
        float mult = GetToolSpeedMultiplier(3, 0, true);
        expect(mult > 1.99f && mult < 2.01f);
    }
}

// ===========================================================================
// GetToolSpeedMultiplier tests - hard-gated jobs
// ===========================================================================
describe(speed_multiplier_hard) {
    // Hard gate: below min = 0.0 (can't do), at min = 1.0x, above = +0.5x per level

    it("should return 0.0 when tool level below minimum") {
        // Rock mining needs hammer:2, bare hands = 0
        float mult = GetToolSpeedMultiplier(0, 2, false);
        expect(mult < 0.01f);
    }

    it("should return 0.0 when tool level 1 below minimum of 2") {
        // Rock (hammer:1) can't mine rock (needs hammer:2)
        float mult = GetToolSpeedMultiplier(1, 2, false);
        expect(mult < 0.01f);
    }

    it("should return 1.0x when tool meets minimum exactly") {
        // Stone hammer (hammer:2) mines rock at baseline
        float mult = GetToolSpeedMultiplier(2, 2, false);
        expect(mult > 0.99f && mult < 1.01f);
    }

    it("should return 1.5x when tool exceeds minimum by 1") {
        float mult = GetToolSpeedMultiplier(3, 2, false);
        expect(mult > 1.49f && mult < 1.51f);
    }

    it("should return 1.0x for level 1 tool on hard gate min 1") {
        // Sharp stone (cutting:1) can chop trees (needs cutting:1)
        float mult = GetToolSpeedMultiplier(1, 1, false);
        expect(mult > 0.99f && mult < 1.01f);
    }

    it("should return 0.0 for bare hands on hard gate min 1") {
        // Can't chop trees bare-handed
        float mult = GetToolSpeedMultiplier(0, 1, false);
        expect(mult < 0.01f);
    }

    it("should return 1.5x for level 2 tool on hard gate min 1") {
        // Stone axe (cutting:2) chops trees at 1.5x
        float mult = GetToolSpeedMultiplier(2, 1, false);
        expect(mult > 1.49f && mult < 1.51f);
    }
}

// ===========================================================================
// Mover equippedTool initialization tests
// ===========================================================================
describe(mover_equipped_tool) {
    it("should initialize equippedTool to -1") {
        InitTestGrid(8, 8);
        ClearMovers();
        Point goal = {4*CELL_SIZE, 4*CELL_SIZE, 0};
        InitMover(&movers[0], 1*CELL_SIZE, 1*CELL_SIZE, 0, goal, MOVER_SPEED);
        moverCount = 1;

        expect(movers[0].equippedTool == -1);
    }
}

// ===========================================================================
// Scenario tests: tool quality lookups for game scenarios
// ===========================================================================
describe(game_scenarios) {
    it("rock provides hammer:1 — helps building but not rock mining") {
        // Rock has hammering:1
        int rockHammer = GetItemQualityLevel(ITEM_ROCK, QUALITY_HAMMERING);
        expect(rockHammer == 1);

        // Building is soft (minLevel=0): rock at hammer:1 → 1.0x
        float buildSpeed = GetToolSpeedMultiplier(rockHammer, 0, true);
        expect(buildSpeed > 0.99f && buildSpeed < 1.01f);

        // Rock mining is hard (minLevel=2): rock at hammer:1 → 0.0 (can't do)
        float mineSpeed = GetToolSpeedMultiplier(rockHammer, 2, false);
        expect(mineSpeed < 0.01f);
    }

    it("sharp stone provides cutting:1 — can chop trees") {
        int ssCutting = GetItemQualityLevel(ITEM_SHARP_STONE, QUALITY_CUTTING);
        expect(ssCutting == 1);

        // Chopping is hard (minLevel=1): sharp stone at cutting:1 → 1.0x
        float chopSpeed = GetToolSpeedMultiplier(ssCutting, 1, false);
        expect(chopSpeed > 0.99f && chopSpeed < 1.01f);
    }

    it("bare hands can dig soil at 0.5x but not mine rock") {
        // Soil digging is soft (minLevel=0)
        float soilSpeed = GetToolSpeedMultiplier(0, 0, true);
        expect(soilSpeed > 0.49f && soilSpeed < 0.51f);

        // Rock mining is hard (minLevel=2)
        float rockSpeed = GetToolSpeedMultiplier(0, 2, false);
        expect(rockSpeed < 0.01f);
    }

    it("effective work time halves when speed doubles") {
        float baseTime = 10.0f;
        // Bare hands on soft job: 0.5x → effective time = 10 / 0.5 = 20
        float bareTime = baseTime / GetToolSpeedMultiplier(0, 0, true);
        expect(bareTime > 19.9f && bareTime < 20.1f);

        // Level 1 tool on soft job: 1.0x → effective time = 10 / 1.0 = 10
        float toolTime = baseTime / GetToolSpeedMultiplier(1, 0, true);
        expect(toolTime > 9.9f && toolTime < 10.1f);

        // Level 3 tool on soft job: 2.0x → effective time = 10 / 2.0 = 5
        float fastTime = baseTime / GetToolSpeedMultiplier(3, 0, true);
        expect(fastTime > 4.9f && fastTime < 5.1f);
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) test_verbose = true;
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(quality_lookup);
    test(has_any_quality);
    test(tool_flag);
    test(speed_multiplier_soft);
    test(speed_multiplier_hard);
    test(mover_equipped_tool);
    test(game_scenarios);

    return summary();
}
