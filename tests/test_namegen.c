#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/terrain.h"
#include "../src/entities/mover.h"
#include "../src/entities/namegen.h"
#include "../src/entities/jobs.h"
#include "test_helpers.h"
#include <string.h>
#include <ctype.h>

static bool test_verbose = false;

// =============================================================================
// Name generation
// =============================================================================

describe(name_generation) {
    it("generates non-empty names under 16 chars") {
        for (int i = 0; i < 100; i++) {
            char name[16];
            GenerateMoverName(name, i % 2, (uint32_t)(i * 12345 + 67890));
            expect(name[0] != '\0');
            expect(strlen(name) < 16);
            expect(isupper(name[0]));
        }
    }

    it("generates unique names for different seeds") {
        char name1[16], name2[16];
        ClearMovers();
        GenerateMoverName(name1, GENDER_MALE, 11111);
        GenerateMoverName(name2, GENDER_MALE, 99999);
        expect(strcmp(name1, name2) != 0);
    }

    it("male names tend to end with consonants") {
        int consonantEnds = 0;
        for (int i = 0; i < 100; i++) {
            char name[16];
            ClearMovers();
            GenerateMoverName(name, GENDER_MALE, (uint32_t)(i * 7919));
            char last = (char)tolower(name[strlen(name) - 1]);
            if (last != 'a' && last != 'e' && last != 'i' && last != 'o' && last != 'u')
                consonantEnds++;
        }
        // >60% should end with consonants
        expect(consonantEnds > 60);
    }

    it("female names tend to be longer") {
        int maleTotal = 0, femaleTotal = 0;
        for (int i = 0; i < 100; i++) {
            char name[16];
            ClearMovers();
            GenerateMoverName(name, GENDER_MALE, (uint32_t)(i * 3571));
            maleTotal += (int)strlen(name);
            ClearMovers();
            GenerateMoverName(name, GENDER_FEMALE, (uint32_t)(i * 3571));
            femaleTotal += (int)strlen(name);
        }
        // Female average should be >= male average
        expect(femaleTotal >= maleTotal);
    }
}

// =============================================================================
// Uniqueness and display
// =============================================================================

describe(name_uniqueness) {
    it("IsNameUnique detects duplicates") {
        ClearMovers();
        Mover* m = &movers[0];
        InitMover(m, 100, 100, 0, (Point){5, 5, 0}, 200);
        strncpy(m->name, "Krog", 16);
        moverCount = 1;

        expect(!IsNameUnique("Krog"));
        expect(IsNameUnique("Zala"));
    }

    it("MoverDisplayName returns name when set") {
        ClearMovers();
        Mover* m = &movers[0];
        InitMover(m, 100, 100, 0, (Point){5, 5, 0}, 200);
        strncpy(m->name, "Thrak", 16);
        moverCount = 1;

        expect(strcmp(MoverDisplayName(0), "Thrak") == 0);
    }

    it("MoverDisplayName fallback for unnamed mover") {
        ClearMovers();
        Mover* m = &movers[0];
        InitMover(m, 100, 100, 0, (Point){5, 5, 0}, 200);
        // name[0] is '\0' after InitMover
        moverCount = 1;

        const char* dn = MoverDisplayName(0);
        expect(strstr(dn, "Mover") != NULL);
    }
}

// =============================================================================
// Draft mode
// =============================================================================

describe(draft_mode) {
    it("drafted movers excluded from idle list") {
        // z=0 CELL_AIR is walkable (implicit bedrock)
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n"
            "....\n", 4, 4);
        ClearMovers();
        ClearJobs();

        Mover* m0 = &movers[0];
        InitMover(m0, 1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, (Point){1, 1, 0}, 200);
        m0->isDrafted = true;
        Mover* m1 = &movers[1];
        InitMover(m1, 2 * CELL_SIZE + 16, 2 * CELL_SIZE + 16, 0, (Point){2, 2, 0}, 200);
        m1->isDrafted = false;
        moverCount = 2;

        RebuildIdleMoverList();

        // Only m1 (undrafted) should be in idle list
        bool m0Idle = false, m1Idle = false;
        for (int i = 0; i < idleMoverCount; i++) {
            if (idleMoverList[i] == 0) m0Idle = true;
            if (idleMoverList[i] == 1) m1Idle = true;
        }
        expect(!m0Idle);
        expect(m1Idle);
    }

    it("undrafting returns mover to idle list") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n"
            "....\n"
            "....\n", 4, 4);
        ClearMovers();
        ClearJobs();

        Mover* m0 = &movers[0];
        InitMover(m0, 1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0, (Point){1, 1, 0}, 200);
        m0->isDrafted = true;
        moverCount = 1;

        RebuildIdleMoverList();
        expect(idleMoverCount == 0);

        m0->isDrafted = false;
        RebuildIdleMoverList();
        expect(idleMoverCount == 1);
    }
}

// =============================================================================
// Pronouns
// =============================================================================

describe(pronouns) {
    it("returns correct pronouns based on gender") {
        ClearMovers();
        Mover* m = &movers[0];
        InitMover(m, 100, 100, 0, (Point){5, 5, 0}, 200);
        m->gender = GENDER_MALE;
        moverCount = 1;

        expect(strcmp(MoverPronoun(0), "he") == 0);
        expect(strcmp(MoverPossessive(0), "his") == 0);

        m->gender = GENDER_FEMALE;
        expect(strcmp(MoverPronoun(0), "she") == 0);
        expect(strcmp(MoverPossessive(0), "her") == 0);
    }
}

int main(int argc, char** argv) {
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    if (quiet) set_quiet_mode(1);

    test(name_generation);
    test(name_uniqueness);
    test(draft_mode);
    test(pronouns);

    return summary();
}
