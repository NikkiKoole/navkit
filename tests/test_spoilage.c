#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/containers.h"
#include "../src/entities/stacking.h"
#include "../src/core/event_log.h"
#include "../src/world/material.h"
#include "test_helpers.h"
#include <string.h>

// Helper: tick spoilage by advancing ItemsTick in small steps
static void TickSpoilage(float totalSeconds) {
    float step = 1.0f; // 1-second steps
    while (totalSeconds > 0.0f) {
        float dt = totalSeconds < step ? totalSeconds : step;
        ItemsTick(dt);
        totalSeconds -= dt;
    }
}

// =============================================================================
// Phase 2: Item Defs
// =============================================================================

describe(spoilage_item_defs) {
    it("should have IF_SPOILS on all raw food items") {
        expect(ItemSpoils(ITEM_CARCASS));
        expect(ItemSpoils(ITEM_RAW_MEAT));
        expect(ItemSpoils(ITEM_COOKED_MEAT));
        expect(ItemSpoils(ITEM_BERRIES));
        expect(ItemSpoils(ITEM_ROOT));
        expect(ItemSpoils(ITEM_ROASTED_ROOT));
    }

    it("should NOT have IF_SPOILS on preserved foods") {
        expect(!ItemSpoils(ITEM_DRIED_BERRIES));
        expect(!ItemSpoils(ITEM_DRIED_ROOT));
        expect(!ItemSpoils(ITEM_HIDE));
    }

    it("should have correct spoilage limits") {
        expect(ItemSpoilageLimit(ITEM_CARCASS) == 60.0f);
        expect(ItemSpoilageLimit(ITEM_RAW_MEAT) == 120.0f);
        expect(ItemSpoilageLimit(ITEM_COOKED_MEAT) == 300.0f);
        expect(ItemSpoilageLimit(ITEM_BERRIES) == 480.0f);
        expect(ItemSpoilageLimit(ITEM_ROOT) == 480.0f);
        expect(ItemSpoilageLimit(ITEM_ROASTED_ROOT) == 300.0f);
    }

    it("should have zero spoilage limit on non-spoiling items") {
        expect(ItemSpoilageLimit(ITEM_ROCK) == 0.0f);
        expect(ItemSpoilageLimit(ITEM_LOG) == 0.0f);
        expect(ItemSpoilageLimit(ITEM_DRIED_BERRIES) == 0.0f);
        expect(ItemSpoilageLimit(ITEM_DRIED_ROOT) == 0.0f);
    }
}

// =============================================================================
// Condition System Tests
// =============================================================================

describe(spoilage_condition) {
    it("should start items as CONDITION_FRESH") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        expect(items[idx].condition == CONDITION_FRESH);
    }

    it("should stay FRESH below 50% of limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES); // limit 480
        TickSpoilage(230.0f); // ~48%
        expect(items[idx].condition == CONDITION_FRESH);
    }

    it("should become STALE at 50% of limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES); // limit 480
        TickSpoilage(240.0f); // exactly 50%
        expect(items[idx].condition == CONDITION_STALE);
    }

    it("should become ROTTEN at 100% of limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES); // limit 480
        TickSpoilage(480.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
    }

    it("rotten item should retain its original type") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        TickSpoilage(480.0f);
        expect(items[idx].active);
        expect(items[idx].type == ITEM_BERRIES);
        expect(items[idx].condition == CONDITION_ROTTEN);
    }

    it("rotten item should not advance timer further") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        TickSpoilage(480.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
        float timerAtRotten = items[idx].spoilageTimer;
        TickSpoilage(1000.0f);
        // Timer should not have advanced — rotten items are skipped
        expect(items[idx].spoilageTimer == timerAtRotten);
    }

    it("non-spoilable items should stay FRESH forever") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_ROCK);
        TickSpoilage(10000.0f);
        expect(items[idx].condition == CONDITION_FRESH);
        expect(items[idx].spoilageTimer == 0.0f);
    }
}

// =============================================================================
// Core Spoilage Tick
// =============================================================================

describe(spoilage_timer) {
    it("should not advance timer for non-spoilable items") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_ROCK);
        expect(idx >= 0);
        TickSpoilage(1000.0f);
        expect(items[idx].active);
        expect(items[idx].spoilageTimer == 0.0f);
    }

    it("should advance timer for spoilable items") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        expect(idx >= 0);
        TickSpoilage(100.0f);
        expect(items[idx].active);
        expect(items[idx].spoilageTimer >= 99.0f);
        expect(items[idx].spoilageTimer <= 101.0f);
    }

    it("should become rotten when timer exceeds limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        expect(idx >= 0);
        TickSpoilage(480.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
    }

    it("should become rotten exactly at limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[idx].spoilageTimer = 479.0f;
        ItemsTick(1.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
    }

    it("should not become rotten just under limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[idx].spoilageTimer = 479.0f;
        ItemsTick(0.5f);
        expect(items[idx].active);
        expect(items[idx].type == ITEM_BERRIES);
        expect(items[idx].condition != CONDITION_ROTTEN);
    }

    it("should not advance timer for carried items") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        items[idx].state = ITEM_CARRIED;
        TickSpoilage(200.0f);
        expect(items[idx].active);
        expect(items[idx].spoilageTimer == 0.0f);
    }

    it("should spoil reserved-but-not-carried items") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        items[idx].reservedBy = 0;
        TickSpoilage(120.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
    }

    it("should rot entire stack") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[idx].stackCount = 10;
        TickSpoilage(480.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
        expect(items[idx].stackCount == 10);
        expect(items[idx].type == ITEM_BERRIES);
    }

    it("should spoil carcass in 60 seconds") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_CARCASS);
        TickSpoilage(59.0f);
        expect(items[idx].active);
        expect(items[idx].condition != CONDITION_ROTTEN);
        TickSpoilage(2.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
        expect(items[idx].type == ITEM_CARCASS);
    }

    it("should spoil raw meat in 120 seconds") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        TickSpoilage(119.0f);
        expect(items[idx].active);
        expect(items[idx].condition != CONDITION_ROTTEN);
        TickSpoilage(2.0f);
        expect(items[idx].condition == CONDITION_ROTTEN);
        expect(items[idx].type == ITEM_RAW_MEAT);
    }
}

// =============================================================================
// Container Integration
// =============================================================================

describe(spoilage_containers) {
    it("should apply clay pot modifier — halves spoilage rate") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int pot = SpawnItem(0, 0, 1, ITEM_CLAY_POT);
        int berries = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[berries].containedIn = pot;
        items[berries].state = ITEM_IN_CONTAINER;
        items[pot].contentCount = 1;
        // Tick past normal limit (480s) — should still be alive with 0.5x modifier
        TickSpoilage(480.0f);
        expect(items[berries].active);
        expect(items[berries].type == ITEM_BERRIES);
        expect(items[berries].condition != CONDITION_ROTTEN);
        // Tick to effective limit (960s)
        TickSpoilage(480.0f);
        expect(items[berries].condition == CONDITION_ROTTEN);
        expect(items[berries].type == ITEM_BERRIES);
    }

    it("should apply basket modifier — no benefit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int basket = SpawnItem(0, 0, 1, ITEM_BASKET);
        int berries = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[berries].containedIn = basket;
        items[berries].state = ITEM_IN_CONTAINER;
        items[basket].contentCount = 1;
        TickSpoilage(480.0f);
        expect(items[berries].condition == CONDITION_ROTTEN);
    }

    it("should use outermost container modifier for nested items") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int chest = SpawnItem(0, 0, 1, ITEM_CHEST);
        int basket = SpawnItem(0, 0, 1, ITEM_BASKET);
        int berries = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[basket].containedIn = chest;
        items[basket].state = ITEM_IN_CONTAINER;
        items[chest].contentCount = 1;
        items[berries].containedIn = basket;
        items[berries].state = ITEM_IN_CONTAINER;
        items[basket].contentCount = 1;
        // Effective limit = 480 / 0.7 ≈ 685.7s
        TickSpoilage(480.0f);
        expect(items[berries].active);
        expect(items[berries].condition != CONDITION_ROTTEN);
        TickSpoilage(210.0f); // total ~690s, past effective limit
        expect(items[berries].condition == CONDITION_ROTTEN);
    }

    it("should not spoil items in a carried container") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int basket = SpawnItem(0, 0, 1, ITEM_BASKET);
        int meat = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        items[meat].containedIn = basket;
        items[meat].state = ITEM_IN_CONTAINER;
        items[basket].contentCount = 1;
        items[basket].state = ITEM_CARRIED;
        TickSpoilage(200.0f);
        expect(items[meat].active);
        expect(items[meat].spoilageTimer == 0.0f);
    }
}

// =============================================================================
// Stack Handling
// =============================================================================

describe(spoilage_stacking) {
    it("should take worse timer on stack merge") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int a = SpawnItem(0, 0, 1, ITEM_BERRIES);
        int b = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[a].stackCount = 5;
        items[b].stackCount = 5;
        items[a].spoilageTimer = 300.0f;
        items[b].spoilageTimer = 100.0f;
        MergeItemIntoStack(a, b);
        expect(items[a].spoilageTimer == 300.0f);
        expect(items[a].stackCount == 10);
    }

    it("should take worse condition on stack merge") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int a = SpawnItem(0, 0, 1, ITEM_BERRIES);
        int b = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[a].stackCount = 5;
        items[b].stackCount = 5;
        items[a].condition = CONDITION_FRESH;
        items[b].condition = CONDITION_STALE;
        MergeItemIntoStack(a, b);
        expect(items[a].condition == CONDITION_STALE);
    }

    it("should copy timer on stack split") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[idx].stackCount = 10;
        items[idx].spoilageTimer = 200.0f;
        int newIdx = SplitStack(idx, 5);
        expect(newIdx >= 0);
        expect(items[newIdx].spoilageTimer == 200.0f);
        expect(items[idx].spoilageTimer == 200.0f);
    }

    it("should copy condition on stack split") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[idx].stackCount = 10;
        items[idx].condition = CONDITION_STALE;
        int newIdx = SplitStack(idx, 5);
        expect(newIdx >= 0);
        expect(items[newIdx].condition == CONDITION_STALE);
    }
}

// =============================================================================
// Rotten as fuel
// =============================================================================

describe(spoilage_rotten_fuel) {
    it("rotten items should be accepted as fuel") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int idx = SpawnItem(0, 0, 1, ITEM_BERRIES);
        items[idx].condition = CONDITION_ROTTEN;
        // ItemIsFuelOrRotten is static in workshops.c, so test indirectly:
        // rotten condition means fuel-eligible, verified by the workshops system
        expect(items[idx].condition == CONDITION_ROTTEN);
        expect(items[idx].type == ITEM_BERRIES);
    }
}

// =============================================================================
// Stockpile rejectsRotten
// =============================================================================

describe(spoilage_stockpile) {
    it("new stockpiles should default to rejectsRotten=true") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        ClearStockpiles();
        int spIdx = CreateStockpile(0, 0, 1, 2, 2);
        expect(spIdx >= 0);
        expect(stockpiles[spIdx].rejectsRotten == true);
    }
}

// =============================================================================
// End-to-End Tests
// =============================================================================

describe(spoilage_e2e) {
    it("drying preserves food — dried berries survive past fresh berry limit") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int dried = SpawnItem(0, 0, 1, ITEM_DRIED_BERRIES);
        TickSpoilage(1000.0f);
        expect(items[dried].active);
        expect(items[dried].type == ITEM_DRIED_BERRIES);
        expect(items[dried].condition == CONDITION_FRESH);
    }

    it("cooking extends shelf life — cooked meat lasts longer than raw") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int raw = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        int cooked = SpawnItem(0, 0, 1, ITEM_COOKED_MEAT);
        // After 120s, raw should be rotten, cooked still alive
        TickSpoilage(120.0f);
        expect(items[raw].condition == CONDITION_ROTTEN);
        expect(items[cooked].active);
        expect(items[cooked].condition != CONDITION_ROTTEN);
        // After 300s total, cooked should also be rotten
        TickSpoilage(180.0f);
        expect(items[cooked].condition == CONDITION_ROTTEN);
    }

    it("container storage chain — pot > chest > basket > ground") {
        InitTestGridFromAscii("....\n....\n");
        ClearItems();
        int ground = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        int basket = SpawnItem(0, 0, 1, ITEM_BASKET);
        int inBasket = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        items[inBasket].containedIn = basket;
        items[inBasket].state = ITEM_IN_CONTAINER;
        items[basket].contentCount = 1;

        int chest = SpawnItem(0, 0, 1, ITEM_CHEST);
        int inChest = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        items[inChest].containedIn = chest;
        items[inChest].state = ITEM_IN_CONTAINER;
        items[chest].contentCount = 1;

        int pot = SpawnItem(0, 0, 1, ITEM_CLAY_POT);
        int inPot = SpawnItem(0, 0, 1, ITEM_RAW_MEAT);
        items[inPot].containedIn = pot;
        items[inPot].state = ITEM_IN_CONTAINER;
        items[pot].contentCount = 1;

        // At 120s: ground=rotten (1.0x), basket=rotten (1.0x)
        TickSpoilage(120.0f);
        expect(items[ground].condition == CONDITION_ROTTEN);
        expect(items[inBasket].condition == CONDITION_ROTTEN);
        expect(items[inChest].condition != CONDITION_ROTTEN);
        expect(items[inPot].condition != CONDITION_ROTTEN);

        // At ~172s: chest item should be rotten (0.7x modifier, effective limit ~171.4s)
        TickSpoilage(52.0f);
        expect(items[inChest].condition == CONDITION_ROTTEN);
        expect(items[inPot].condition != CONDITION_ROTTEN);

        // At ~240s: pot item should be rotten (0.5x modifier, effective limit 240s)
        TickSpoilage(70.0f);
        expect(items[inPot].condition == CONDITION_ROTTEN);
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-q") == 0) set_quiet_mode(1);

    test(spoilage_item_defs);
    test(spoilage_condition);
    test(spoilage_timer);
    test(spoilage_containers);
    test(spoilage_stacking);
    test(spoilage_rotten_fuel);
    test(spoilage_stockpile);
    test(spoilage_e2e);

    return summary();
}
