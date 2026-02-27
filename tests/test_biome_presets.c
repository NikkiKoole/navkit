#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/biome.h"
#include "../src/world/material.h"
#include "../src/world/grid.h"

describe(biome_preset_validation) {
    it("all presets have valid stone type") {
        for (int i = 0; i < BIOME_COUNT; i++) {
            expect(IsStoneMaterial(biomePresets[i].stoneType));
        }
    }

    it("all presets have positive soil weight totals") {
        for (int i = 0; i < BIOME_COUNT; i++) {
            float total = biomePresets[i].soilDirt + biomePresets[i].soilClay +
                          biomePresets[i].soilSand + biomePresets[i].soilGravel +
                          biomePresets[i].soilPeat;
            expect(total > 0.0f);
        }
    }

    it("all presets have sane temperature ranges") {
        for (int i = 0; i < BIOME_COUNT; i++) {
            expect(biomePresets[i].baseSurfaceTemp >= -20);
            expect(biomePresets[i].baseSurfaceTemp <= 50);
            expect(biomePresets[i].seasonalAmplitude >= 0);
            expect(biomePresets[i].seasonalAmplitude <= 40);
            expect(biomePresets[i].diurnalAmplitude >= 0);
            expect(biomePresets[i].diurnalAmplitude <= 25);
        }
    }

    it("all presets have valid height variation") {
        for (int i = 0; i < BIOME_COUNT; i++) {
            expect(biomePresets[i].heightVariation >= 0);
            expect(biomePresets[i].heightVariation <= 3);
        }
    }

    it("temperate preset matches current defaults") {
        const BiomePreset* bp = &biomePresets[0];
        expect(bp->baseSurfaceTemp == 15);
        expect(bp->seasonalAmplitude == 25);
        expect(bp->diurnalAmplitude == 5);
        expect(bp->heightVariation == 1);
        expect(bp->stoneType == MAT_GRANITE);
        expect(bp->treeDensity == 1.0f);
        expect(bp->grassDensity == 1.0f);
        expect(bp->bushDensity == 1.0f);
        expect(bp->wildCropDensity == 1.0f);
        expect(bp->boulderDensity == 1.0f);
        expect(bp->riverCount == 2);
        expect(bp->lakeCount == 2);
    }

    it("all presets have valid names") {
        for (int i = 0; i < BIOME_COUNT; i++) {
            expect(biomePresets[i].name != NULL);
            expect(biomePresets[i].description != NULL);
            expect(biomeNames[i] != NULL);
        }
    }

    it("all presets have non-negative density multipliers") {
        for (int i = 0; i < BIOME_COUNT; i++) {
            expect(biomePresets[i].treeDensity >= 0.0f);
            expect(biomePresets[i].grassDensity >= 0.0f);
            expect(biomePresets[i].bushDensity >= 0.0f);
            expect(biomePresets[i].wildCropDensity >= 0.0f);
            expect(biomePresets[i].boulderDensity >= 0.0f);
        }
    }
}

describe(biome_soil_selection) {
    it("PickSoilForBiome produces expected dominant soil per biome") {
        // Verify the soil weights lead to the expected dominant soil type
        // Arid should be sand-dominant
        expect(biomePresets[1].soilSand > biomePresets[1].soilDirt);
        expect(biomePresets[1].soilSand > biomePresets[1].soilClay);
        // Boreal should be peat-dominant
        expect(biomePresets[2].soilPeat > biomePresets[2].soilDirt);
        // Wetland should be peat-dominant
        expect(biomePresets[3].soilPeat > biomePresets[3].soilClay);
        // Highland should be gravel-dominant
        expect(biomePresets[4].soilGravel > biomePresets[4].soilDirt);
        // Riverlands should be dirt-dominant
        expect(biomePresets[5].soilDirt > biomePresets[5].soilClay);
    }
}

describe(biome_tree_selection) {
    it("PickTreeTypeForBiome weights match expected dominant species") {
        // Arid: birch-dominant
        expect(biomePresets[1].treeBirch > biomePresets[1].treeOak);
        expect(biomePresets[1].treeBirch > biomePresets[1].treePine);
        // Boreal: pine-dominant
        expect(biomePresets[2].treePine > biomePresets[2].treeOak);
        expect(biomePresets[2].treePine > biomePresets[2].treeBirch);
        // Wetland: willow-dominant
        expect(biomePresets[3].treeWillow > biomePresets[3].treeOak);
        expect(biomePresets[3].treeWillow > biomePresets[3].treePine);
        // Highland: pine-dominant
        expect(biomePresets[4].treePine > biomePresets[4].treeOak);
        expect(biomePresets[4].treePine > biomePresets[4].treeBirch);
    }
}

int main(int argc, char** argv) {
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    SetTraceLogLevel(LOG_NONE);
    if (quiet) set_quiet_mode(1);

    test(biome_preset_validation);
    test(biome_soil_selection);
    test(biome_tree_selection);

    return summary();
}
