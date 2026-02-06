// core/saveload.c - Save and Load functions
#include "../game_state.h"
#include "../entities/workshops.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../core/sim_manager.h"
#include "../world/material.h"

#define SAVE_VERSION 20  // Add ITEM_BRICKS, ITEM_CHARCOAL (kiln)
#define V19_ITEM_TYPE_COUNT 23  // ITEM_TYPE_COUNT before ITEM_BRICKS/ITEM_CHARCOAL were added
#define V18_ITEM_TYPE_COUNT 21  // ITEM_TYPE_COUNT before ITEM_PLANKS/ITEM_STICKS were added
#define SAVE_MAGIC 0x4E41564B  // "NAVK"

// Section markers (readable in hex dump)
#define MARKER_GRIDS    0x47524944  // "GRID"
#define MARKER_ENTITIES 0x454E5449  // "ENTI"
#define MARKER_VIEW     0x56494557  // "VIEW"
#define MARKER_SETTINGS 0x53455454  // "SETT"
#define MARKER_END      0x454E4421  // "END!"

bool SaveWorld(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        AddMessage(TextFormat("Failed to open %s for writing", filename), RED);
        return false;
    }
    
    // Header
    uint32_t magic = SAVE_MAGIC;
    uint32_t version = SAVE_VERSION;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    
    // World seed (for reproducible terrain regeneration)
    fwrite(&worldSeed, sizeof(worldSeed), 1, f);
    
    // Grid dimensions
    fwrite(&gridWidth, sizeof(gridWidth), 1, f);
    fwrite(&gridHeight, sizeof(gridHeight), 1, f);
    fwrite(&gridDepth, sizeof(gridDepth), 1, f);
    fwrite(&chunkWidth, sizeof(chunkWidth), 1, f);
    fwrite(&chunkHeight, sizeof(chunkHeight), 1, f);
    
    // === GRIDS SECTION ===
    uint32_t marker = MARKER_GRIDS;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(grid[z][y], sizeof(CellType), gridWidth, f);
        }
    }

    // Tree type grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(treeTypeGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Tree part grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(treePartGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(waterGrid[z][y], sizeof(WaterCell), gridWidth, f);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(fireGrid[z][y], sizeof(FireCell), gridWidth, f);
        }
    }
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(smokeGrid[z][y], sizeof(SmokeCell), gridWidth, f);
        }
    }
    
    // Steam grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(steamGrid[z][y], sizeof(SteamCell), gridWidth, f);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Wall material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wallMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Floor material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Wall natural grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(wallNatural[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Floor natural grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(floorNatural[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Temperature grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(temperatureGrid[z][y], sizeof(TempCell), gridWidth, f);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(designations[z][y], sizeof(Designation), gridWidth, f);
        }
    }
    
    // === ENTITIES SECTION ===
    marker = MARKER_ENTITIES;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Items
    fwrite(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    fwrite(items, sizeof(Item), itemHighWaterMark, f);
    
    // Stockpiles
    fwrite(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    
    // Gather zones
    fwrite(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fwrite(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fwrite(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Workshops
    fwrite(workshops, sizeof(Workshop), MAX_WORKSHOPS, f);
    
    // Movers
    fwrite(&moverCount, sizeof(moverCount), 1, f);
    fwrite(movers, sizeof(Mover), moverCount, f);
    
    // Jobs
    fwrite(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fwrite(&activeJobCount, sizeof(activeJobCount), 1, f);
    fwrite(jobs, sizeof(Job), jobHighWaterMark, f);
    fwrite(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fwrite(activeJobList, sizeof(int), activeJobCount, f);
    
    // === VIEW SECTION ===
    marker = MARKER_VIEW;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // View state
    fwrite(&currentViewZ, sizeof(currentViewZ), 1, f);
    fwrite(&zoom, sizeof(zoom), 1, f);
    fwrite(&offset, sizeof(offset), 1, f);
    
    // === SETTINGS SECTION ===
    marker = MARKER_SETTINGS;
    fwrite(&marker, sizeof(marker), 1, f);
    
    // Simulation settings
    // NOTE: When adding new tweakable settings, add them here AND in LoadWorld below!
    // Water
    fwrite(&waterEnabled, sizeof(waterEnabled), 1, f);
    fwrite(&waterEvaporationEnabled, sizeof(waterEvaporationEnabled), 1, f);
    fwrite(&waterEvapInterval, sizeof(waterEvapInterval), 1, f);
    fwrite(&waterSpeedShallow, sizeof(waterSpeedShallow), 1, f);
    fwrite(&waterSpeedMedium, sizeof(waterSpeedMedium), 1, f);
    fwrite(&waterSpeedDeep, sizeof(waterSpeedDeep), 1, f);
    
    // Fire
    fwrite(&fireEnabled, sizeof(fireEnabled), 1, f);
    fwrite(&fireSpreadInterval, sizeof(fireSpreadInterval), 1, f);
    fwrite(&fireFuelInterval, sizeof(fireFuelInterval), 1, f);
    fwrite(&fireWaterReduction, sizeof(fireWaterReduction), 1, f);
    fwrite(&fireSpreadBase, sizeof(fireSpreadBase), 1, f);
    fwrite(&fireSpreadPerLevel, sizeof(fireSpreadPerLevel), 1, f);
    
    // Smoke
    fwrite(&smokeEnabled, sizeof(smokeEnabled), 1, f);
    fwrite(&smokeRiseInterval, sizeof(smokeRiseInterval), 1, f);
    fwrite(&smokeDissipationTime, sizeof(smokeDissipationTime), 1, f);
    fwrite(&smokeGenerationRate, sizeof(smokeGenerationRate), 1, f);
    
    // Steam
    fwrite(&steamEnabled, sizeof(steamEnabled), 1, f);
    fwrite(&steamRiseInterval, sizeof(steamRiseInterval), 1, f);
    fwrite(&steamCondensationTemp, sizeof(steamCondensationTemp), 1, f);
    fwrite(&steamGenerationTemp, sizeof(steamGenerationTemp), 1, f);
    
    // Temperature
    fwrite(&temperatureEnabled, sizeof(temperatureEnabled), 1, f);
    fwrite(&ambientSurfaceTemp, sizeof(ambientSurfaceTemp), 1, f);
    fwrite(&ambientDepthDecay, sizeof(ambientDepthDecay), 1, f);
    fwrite(&heatTransferInterval, sizeof(heatTransferInterval), 1, f);
    fwrite(&tempDecayInterval, sizeof(tempDecayInterval), 1, f);
    fwrite(&heatSourceTemp, sizeof(heatSourceTemp), 1, f);
    fwrite(&coldSourceTemp, sizeof(coldSourceTemp), 1, f);
    fwrite(&heatRiseBoost, sizeof(heatRiseBoost), 1, f);
    fwrite(&heatSinkReduction, sizeof(heatSinkReduction), 1, f);
    fwrite(&heatDecayPercent, sizeof(heatDecayPercent), 1, f);
    fwrite(&diagonalTransferPercent, sizeof(diagonalTransferPercent), 1, f);
    
    // Ground wear
    fwrite(&groundWearEnabled, sizeof(groundWearEnabled), 1, f);
    fwrite(&wearGrassToDirt, sizeof(wearGrassToDirt), 1, f);
    fwrite(&wearDirtToGrass, sizeof(wearDirtToGrass), 1, f);
    fwrite(&wearTrampleAmount, sizeof(wearTrampleAmount), 1, f);
    fwrite(&wearDecayRate, sizeof(wearDecayRate), 1, f);
    fwrite(&wearRecoveryInterval, sizeof(wearRecoveryInterval), 1, f);
    fwrite(&wearMax, sizeof(wearMax), 1, f);
    
    // Trees
    fwrite(&saplingGrowTicks, sizeof(saplingGrowTicks), 1, f);
    fwrite(&trunkGrowTicks, sizeof(trunkGrowTicks), 1, f);
    fwrite(&saplingRegrowthEnabled, sizeof(saplingRegrowthEnabled), 1, f);
    fwrite(&saplingRegrowthChance, sizeof(saplingRegrowthChance), 1, f);
    fwrite(&saplingMinTreeDistance, sizeof(saplingMinTreeDistance), 1, f);
    
    // === END MARKER ===
    marker = MARKER_END;
    fwrite(&marker, sizeof(marker), 1, f);
    
    fclose(f);
    return true;
}

bool LoadWorld(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        AddMessage(TextFormat("Failed to open %s", filename), RED);
        return false;
    }
    
    // Check header
    uint32_t magic, version;
    fread(&magic, sizeof(magic), 1, f);
    fread(&version, sizeof(version), 1, f);
    
    if (magic != SAVE_MAGIC) {
        printf("ERROR: Invalid save file (bad magic: 0x%08X, expected 0x%08X)\n", magic, SAVE_MAGIC);
        AddMessage("Invalid save file (bad magic)", RED);
        fclose(f);
        return false;
    }
    
    if (version > SAVE_VERSION || version < 15) {
        printf("ERROR: Save version mismatch (file: %d, supported: %d-%d)\n", version, 15, SAVE_VERSION);
        AddMessage(TextFormat("Save version mismatch (file: %d, supported: %d-%d)", version, 15, SAVE_VERSION), RED);
        fclose(f);
        return false;
    }
    
    // World seed
    fread(&worldSeed, sizeof(worldSeed), 1, f);
    
    // Grid dimensions
    int newWidth, newHeight, newDepth, newChunkW, newChunkH;
    fread(&newWidth, sizeof(newWidth), 1, f);
    fread(&newHeight, sizeof(newHeight), 1, f);
    fread(&newDepth, sizeof(newDepth), 1, f);
    fread(&newChunkW, sizeof(newChunkW), 1, f);
    fread(&newChunkH, sizeof(newChunkH), 1, f);
    
    // Reinitialize grid if dimensions don't match
    if (newWidth != gridWidth || newHeight != gridHeight || 
        newChunkW != chunkWidth || newChunkH != chunkHeight) {
        InitGridWithSizeAndChunkSize(newWidth, newHeight, newChunkW, newChunkH);
        InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
    }
    gridDepth = newDepth;
    
    // Clear current state
    ClearMovers();
    ClearJobs();
    ClearGatherZones();
    
    // === GRIDS SECTION ===
    uint32_t marker;
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_GRIDS) {
        printf("ERROR: Bad GRID marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_GRIDS);
        AddMessage(TextFormat("Bad GRID marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(grid[z][y], sizeof(CellType), gridWidth, f);
        }
    }

    // Tree type grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(treeTypeGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    // Tree part grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(treePartGrid[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Water grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(waterGrid[z][y], sizeof(WaterCell), gridWidth, f);
        }
    }
    
    // Fire grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(fireGrid[z][y], sizeof(FireCell), gridWidth, f);
        }
    }
    
    // Smoke grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(smokeGrid[z][y], sizeof(SmokeCell), gridWidth, f);
        }
    }
    
    // Steam grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(steamGrid[z][y], sizeof(SteamCell), gridWidth, f);
        }
    }
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Wall material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(wallMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Floor material grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(floorMaterial[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }

    if (version >= 16) {
        // Wall natural grid
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(wallNatural[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }

        // Floor natural grid
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                fread(floorNatural[z][y], sizeof(uint8_t), gridWidth, f);
            }
        }
    } else {
        // Migrate legacy materials (v15): MAT_RAW/MAT_STONE/MAT_WOOD -> new materials + natural flags
        const uint8_t LEGACY_MAT_NONE = 0;
        const uint8_t LEGACY_MAT_RAW = 1;
        const uint8_t LEGACY_MAT_STONE = 2;
        const uint8_t LEGACY_MAT_WOOD = 3;
        const uint8_t LEGACY_MAT_DIRT = 4;
        const uint8_t LEGACY_MAT_IRON = 5;
        const uint8_t LEGACY_MAT_GLASS = 6;
        (void)LEGACY_MAT_NONE;

        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    uint8_t oldWall = wallMaterial[z][y][x];
                    uint8_t oldFloor = floorMaterial[z][y][x];

                    bool wallNat = false;
                    bool floorNat = false;

                    switch (oldWall) {
                        case LEGACY_MAT_RAW:
                            wallMaterial[z][y][x] = MAT_GRANITE;
                            wallNat = true;
                            break;
                        case LEGACY_MAT_STONE:
                            wallMaterial[z][y][x] = MAT_GRANITE;
                            break;
                        case LEGACY_MAT_WOOD:
                            wallMaterial[z][y][x] = MAT_OAK;
                            break;
                        case LEGACY_MAT_DIRT:
                            wallMaterial[z][y][x] = MAT_DIRT;
                            break;
                        case LEGACY_MAT_IRON:
                            wallMaterial[z][y][x] = MAT_IRON;
                            break;
                        case LEGACY_MAT_GLASS:
                            wallMaterial[z][y][x] = MAT_GLASS;
                            break;
                        default:
                            wallMaterial[z][y][x] = MAT_NONE;
                            break;
                    }

                    switch (oldFloor) {
                        case LEGACY_MAT_RAW:
                            floorMaterial[z][y][x] = MAT_GRANITE;
                            floorNat = true;
                            break;
                        case LEGACY_MAT_STONE:
                            floorMaterial[z][y][x] = MAT_GRANITE;
                            break;
                        case LEGACY_MAT_WOOD:
                            floorMaterial[z][y][x] = MAT_OAK;
                            break;
                        case LEGACY_MAT_DIRT:
                            floorMaterial[z][y][x] = MAT_DIRT;
                            break;
                        case LEGACY_MAT_IRON:
                            floorMaterial[z][y][x] = MAT_IRON;
                            break;
                        case LEGACY_MAT_GLASS:
                            floorMaterial[z][y][x] = MAT_GLASS;
                            break;
                        default:
                            floorMaterial[z][y][x] = MAT_NONE;
                            break;
                    }

                    wallNatural[z][y][x] = wallNat ? 1 : 0;
                    floorNatural[z][y][x] = floorNat ? 1 : 0;
                }
            }
        }
    }
    
    // Temperature grid
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(temperatureGrid[z][y], sizeof(TempCell), gridWidth, f);
        }
    }
    
    // Designations
    activeDesignationCount = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(designations[z][y], sizeof(Designation), gridWidth, f);
            // Count active designations for early-exit optimizations
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type != DESIGNATION_NONE) {
                    activeDesignationCount++;
                }
            }
        }
    }
    
    // === ENTITIES SECTION ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_ENTITIES) {
        printf("ERROR: Bad ENTI marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_ENTITIES);
        AddMessage(TextFormat("Bad ENTI marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // Items
    fread(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    if (version >= 16) {
        fread(items, sizeof(Item), itemHighWaterMark, f);
    } else {
        typedef struct {
            float x, y, z;
            ItemType type;
            ItemState state;
            uint8_t treeType;
            bool active;
            int reservedBy;
            float unreachableCooldown;
        } ItemV15;
        ItemV15* legacyItems = malloc(sizeof(ItemV15) * itemHighWaterMark);
        if (!legacyItems) {
            fclose(f);
            AddMessage("Failed to allocate memory for legacy items", RED);
            return false;
        }
        fread(legacyItems, sizeof(ItemV15), itemHighWaterMark, f);
        for (int i = 0; i < itemHighWaterMark; i++) {
            items[i].x = legacyItems[i].x;
            items[i].y = legacyItems[i].y;
            items[i].z = legacyItems[i].z;
            items[i].type = legacyItems[i].type;
            items[i].state = legacyItems[i].state;
            items[i].active = legacyItems[i].active;
            items[i].reservedBy = legacyItems[i].reservedBy;
            items[i].unreachableCooldown = legacyItems[i].unreachableCooldown;
            items[i].natural = false;

            MaterialType mat = MAT_NONE;
            TreeType treeType = (TreeType)legacyItems[i].treeType;
            if (items[i].type == ITEM_LOG) {
                mat = MaterialFromTreeType(treeType);
            } else if (IsSaplingItem(items[i].type)) {
                mat = MaterialFromTreeType(TreeTypeFromSaplingItem(items[i].type));
            } else if (IsLeafItem(items[i].type)) {
                switch (items[i].type) {
                    case ITEM_LEAVES_PINE: mat = MAT_PINE; break;
                    case ITEM_LEAVES_BIRCH: mat = MAT_BIRCH; break;
                    case ITEM_LEAVES_WILLOW: mat = MAT_WILLOW; break;
                    case ITEM_LEAVES_OAK:
                    default: mat = MAT_OAK; break;
                }
            } else {
                mat = (MaterialType)DefaultMaterialForItemType(items[i].type);
            }
            items[i].material = (uint8_t)mat;
        }
        free(legacyItems);
    }

    // Ensure default materials for any missing entries
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].material == MAT_NONE) {
            items[i].material = DefaultMaterialForItemType(items[i].type);
        }
    }
    
    // Stockpiles
    if (version >= 20) {
        fread(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    } else if (version >= 19) {
        // V19: had 23 item types (before ITEM_BRICKS/ITEM_CHARCOAL)
        typedef struct {
            int x, y, z;
            int width, height;
            bool active;
            bool allowedTypes[V19_ITEM_TYPE_COUNT];
            bool allowedMaterials[MAT_COUNT];
            bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int maxStackSize;
            int priority;
            int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int freeSlotCount;
        } StockpileV19;

        StockpileV19 legacyStockpiles[MAX_STOCKPILES];
        fread(legacyStockpiles, sizeof(StockpileV19), MAX_STOCKPILES, f);

        for (int i = 0; i < MAX_STOCKPILES; i++) {
            memset(&stockpiles[i], 0, sizeof(Stockpile));
            stockpiles[i].x = legacyStockpiles[i].x;
            stockpiles[i].y = legacyStockpiles[i].y;
            stockpiles[i].z = legacyStockpiles[i].z;
            stockpiles[i].width = legacyStockpiles[i].width;
            stockpiles[i].height = legacyStockpiles[i].height;
            stockpiles[i].active = legacyStockpiles[i].active;
            for (int t = 0; t < V19_ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = legacyStockpiles[i].allowedTypes[t];
            }
            for (int t = V19_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, legacyStockpiles[i].allowedMaterials, sizeof(legacyStockpiles[i].allowedMaterials));
            memcpy(stockpiles[i].cells, legacyStockpiles[i].cells, sizeof(legacyStockpiles[i].cells));
            memcpy(stockpiles[i].slots, legacyStockpiles[i].slots, sizeof(legacyStockpiles[i].slots));
            memcpy(stockpiles[i].reservedBy, legacyStockpiles[i].reservedBy, sizeof(legacyStockpiles[i].reservedBy));
            memcpy(stockpiles[i].slotCounts, legacyStockpiles[i].slotCounts, sizeof(legacyStockpiles[i].slotCounts));
            memcpy(stockpiles[i].slotTypes, legacyStockpiles[i].slotTypes, sizeof(legacyStockpiles[i].slotTypes));
            memcpy(stockpiles[i].slotMaterials, legacyStockpiles[i].slotMaterials, sizeof(legacyStockpiles[i].slotMaterials));
            stockpiles[i].maxStackSize = legacyStockpiles[i].maxStackSize;
            stockpiles[i].priority = legacyStockpiles[i].priority;
            memcpy(stockpiles[i].groundItemIdx, legacyStockpiles[i].groundItemIdx, sizeof(legacyStockpiles[i].groundItemIdx));
            stockpiles[i].freeSlotCount = legacyStockpiles[i].freeSlotCount;
        }
    } else if (version >= 18) {
        // V18: had 21 item types (before ITEM_PLANKS/ITEM_STICKS)
        typedef struct {
            int x, y, z;
            int width, height;
            bool active;
            bool allowedTypes[V18_ITEM_TYPE_COUNT];
            bool allowedMaterials[MAT_COUNT];
            bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int maxStackSize;
            int priority;
            int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int freeSlotCount;
        } StockpileV18;

        StockpileV18 legacyStockpiles[MAX_STOCKPILES];
        fread(legacyStockpiles, sizeof(StockpileV18), MAX_STOCKPILES, f);

        for (int i = 0; i < MAX_STOCKPILES; i++) {
            memset(&stockpiles[i], 0, sizeof(Stockpile));
            stockpiles[i].x = legacyStockpiles[i].x;
            stockpiles[i].y = legacyStockpiles[i].y;
            stockpiles[i].z = legacyStockpiles[i].z;
            stockpiles[i].width = legacyStockpiles[i].width;
            stockpiles[i].height = legacyStockpiles[i].height;
            stockpiles[i].active = legacyStockpiles[i].active;
            // Copy old allowedTypes and default new ones to true
            for (int t = 0; t < V18_ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = legacyStockpiles[i].allowedTypes[t];
            }
            for (int t = V18_ITEM_TYPE_COUNT; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[i].allowedTypes[t] = true;
            }
            memcpy(stockpiles[i].allowedMaterials, legacyStockpiles[i].allowedMaterials, sizeof(legacyStockpiles[i].allowedMaterials));
            memcpy(stockpiles[i].cells, legacyStockpiles[i].cells, sizeof(legacyStockpiles[i].cells));
            memcpy(stockpiles[i].slots, legacyStockpiles[i].slots, sizeof(legacyStockpiles[i].slots));
            memcpy(stockpiles[i].reservedBy, legacyStockpiles[i].reservedBy, sizeof(legacyStockpiles[i].reservedBy));
            memcpy(stockpiles[i].slotCounts, legacyStockpiles[i].slotCounts, sizeof(legacyStockpiles[i].slotCounts));
            memcpy(stockpiles[i].slotTypes, legacyStockpiles[i].slotTypes, sizeof(legacyStockpiles[i].slotTypes));
            memcpy(stockpiles[i].slotMaterials, legacyStockpiles[i].slotMaterials, sizeof(legacyStockpiles[i].slotMaterials));
            stockpiles[i].maxStackSize = legacyStockpiles[i].maxStackSize;
            stockpiles[i].priority = legacyStockpiles[i].priority;
            memcpy(stockpiles[i].groundItemIdx, legacyStockpiles[i].groundItemIdx, sizeof(legacyStockpiles[i].groundItemIdx));
            stockpiles[i].freeSlotCount = legacyStockpiles[i].freeSlotCount;
        }
    } else if (version >= 17) {
        typedef struct {
            int x, y, z;
            int width, height;
            bool active;
            bool allowedTypes[ITEM_TYPE_COUNT];
            bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            uint8_t slotMaterials[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int maxStackSize;
            int priority;
            int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int freeSlotCount;
        } StockpileV17;

        StockpileV17 legacyStockpiles[MAX_STOCKPILES];
        fread(legacyStockpiles, sizeof(StockpileV17), MAX_STOCKPILES, f);

        for (int i = 0; i < MAX_STOCKPILES; i++) {
            stockpiles[i].x = legacyStockpiles[i].x;
            stockpiles[i].y = legacyStockpiles[i].y;
            stockpiles[i].z = legacyStockpiles[i].z;
            stockpiles[i].width = legacyStockpiles[i].width;
            stockpiles[i].height = legacyStockpiles[i].height;
            stockpiles[i].active = legacyStockpiles[i].active;
            memcpy(stockpiles[i].allowedTypes, legacyStockpiles[i].allowedTypes, sizeof(legacyStockpiles[i].allowedTypes));
            memcpy(stockpiles[i].cells, legacyStockpiles[i].cells, sizeof(legacyStockpiles[i].cells));
            memcpy(stockpiles[i].slots, legacyStockpiles[i].slots, sizeof(legacyStockpiles[i].slots));
            memcpy(stockpiles[i].reservedBy, legacyStockpiles[i].reservedBy, sizeof(legacyStockpiles[i].reservedBy));
            memcpy(stockpiles[i].slotCounts, legacyStockpiles[i].slotCounts, sizeof(legacyStockpiles[i].slotCounts));
            memcpy(stockpiles[i].slotTypes, legacyStockpiles[i].slotTypes, sizeof(legacyStockpiles[i].slotTypes));
            memcpy(stockpiles[i].slotMaterials, legacyStockpiles[i].slotMaterials, sizeof(legacyStockpiles[i].slotMaterials));
            stockpiles[i].maxStackSize = legacyStockpiles[i].maxStackSize;
            stockpiles[i].priority = legacyStockpiles[i].priority;
            memcpy(stockpiles[i].groundItemIdx, legacyStockpiles[i].groundItemIdx, sizeof(legacyStockpiles[i].groundItemIdx));
            stockpiles[i].freeSlotCount = legacyStockpiles[i].freeSlotCount;

            for (int m = 0; m < MAT_COUNT; m++) {
                stockpiles[i].allowedMaterials[m] = true;
            }
        }
    } else {
        typedef struct {
            int x, y, z;
            int width, height;
            bool active;
            bool allowedTypes[ITEM_TYPE_COUNT];
            bool cells[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slots[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int reservedBy[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int slotCounts[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            ItemType slotTypes[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int maxStackSize;
            int priority;
            int groundItemIdx[MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE];
            int freeSlotCount;
        } StockpileV16;

        StockpileV16 legacyStockpiles[MAX_STOCKPILES];
        fread(legacyStockpiles, sizeof(StockpileV16), MAX_STOCKPILES, f);

        for (int i = 0; i < MAX_STOCKPILES; i++) {
            stockpiles[i].x = legacyStockpiles[i].x;
            stockpiles[i].y = legacyStockpiles[i].y;
            stockpiles[i].z = legacyStockpiles[i].z;
            stockpiles[i].width = legacyStockpiles[i].width;
            stockpiles[i].height = legacyStockpiles[i].height;
            stockpiles[i].active = legacyStockpiles[i].active;
            memcpy(stockpiles[i].allowedTypes, legacyStockpiles[i].allowedTypes, sizeof(legacyStockpiles[i].allowedTypes));
            memcpy(stockpiles[i].cells, legacyStockpiles[i].cells, sizeof(legacyStockpiles[i].cells));
            memcpy(stockpiles[i].slots, legacyStockpiles[i].slots, sizeof(legacyStockpiles[i].slots));
            memcpy(stockpiles[i].reservedBy, legacyStockpiles[i].reservedBy, sizeof(legacyStockpiles[i].reservedBy));
            memcpy(stockpiles[i].slotCounts, legacyStockpiles[i].slotCounts, sizeof(legacyStockpiles[i].slotCounts));
            memcpy(stockpiles[i].slotTypes, legacyStockpiles[i].slotTypes, sizeof(legacyStockpiles[i].slotTypes));
            stockpiles[i].maxStackSize = legacyStockpiles[i].maxStackSize;
            stockpiles[i].priority = legacyStockpiles[i].priority;
            memcpy(stockpiles[i].groundItemIdx, legacyStockpiles[i].groundItemIdx, sizeof(legacyStockpiles[i].groundItemIdx));
            stockpiles[i].freeSlotCount = legacyStockpiles[i].freeSlotCount;

            int totalSlots = MAX_STOCKPILE_SIZE * MAX_STOCKPILE_SIZE;
            for (int s = 0; s < totalSlots; s++) {
                if (stockpiles[i].slotCounts[s] > 0 && stockpiles[i].slotTypes[s] >= 0) {
                    stockpiles[i].slotMaterials[s] = DefaultMaterialForItemType(stockpiles[i].slotTypes[s]);
                } else {
                    stockpiles[i].slotMaterials[s] = MAT_NONE;
                }
            }

            for (int m = 0; m < MAT_COUNT; m++) {
                stockpiles[i].allowedMaterials[m] = true;
            }
        }
    }
    
    // Gather zones
    fread(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fread(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fread(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Workshops
    fread(workshops, sizeof(Workshop), MAX_WORKSHOPS, f);
    
    // Movers
    fread(&moverCount, sizeof(moverCount), 1, f);
    fread(movers, sizeof(Mover), moverCount, f);
    
    // Initialize canPlant for old saves (field added later)
    for (int i = 0; i < moverCount; i++) {
        movers[i].capabilities.canPlant = true;
    }
    
    // Jobs
    fread(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fread(&activeJobCount, sizeof(activeJobCount), 1, f);
    fread(jobs, sizeof(Job), jobHighWaterMark, f);
    fread(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fread(activeJobList, sizeof(int), activeJobCount, f);
    
    // === VIEW SECTION ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_VIEW) {
        printf("ERROR: Bad VIEW marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_VIEW);
        AddMessage(TextFormat("Bad VIEW marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // View state
    fread(&currentViewZ, sizeof(currentViewZ), 1, f);
    fread(&zoom, sizeof(zoom), 1, f);
    fread(&offset, sizeof(offset), 1, f);
    
    // === SETTINGS SECTION ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_SETTINGS) {
        printf("ERROR: Bad SETT marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_SETTINGS);
        AddMessage(TextFormat("Bad SETT marker: 0x%08X", marker), RED);
        fclose(f);
        return false;
    }
    
    // Simulation settings
    // NOTE: When adding new tweakable settings, add them here AND in SaveWorld above!
    // Water
    fread(&waterEnabled, sizeof(waterEnabled), 1, f);
    fread(&waterEvaporationEnabled, sizeof(waterEvaporationEnabled), 1, f);
    fread(&waterEvapInterval, sizeof(waterEvapInterval), 1, f);
    fread(&waterSpeedShallow, sizeof(waterSpeedShallow), 1, f);
    fread(&waterSpeedMedium, sizeof(waterSpeedMedium), 1, f);
    fread(&waterSpeedDeep, sizeof(waterSpeedDeep), 1, f);
    
    // Fire
    fread(&fireEnabled, sizeof(fireEnabled), 1, f);
    fread(&fireSpreadInterval, sizeof(fireSpreadInterval), 1, f);
    fread(&fireFuelInterval, sizeof(fireFuelInterval), 1, f);
    fread(&fireWaterReduction, sizeof(fireWaterReduction), 1, f);
    fread(&fireSpreadBase, sizeof(fireSpreadBase), 1, f);
    fread(&fireSpreadPerLevel, sizeof(fireSpreadPerLevel), 1, f);
    
    // Smoke
    fread(&smokeEnabled, sizeof(smokeEnabled), 1, f);
    fread(&smokeRiseInterval, sizeof(smokeRiseInterval), 1, f);
    fread(&smokeDissipationTime, sizeof(smokeDissipationTime), 1, f);
    fread(&smokeGenerationRate, sizeof(smokeGenerationRate), 1, f);
    
    // Steam
    fread(&steamEnabled, sizeof(steamEnabled), 1, f);
    fread(&steamRiseInterval, sizeof(steamRiseInterval), 1, f);
    fread(&steamCondensationTemp, sizeof(steamCondensationTemp), 1, f);
    fread(&steamGenerationTemp, sizeof(steamGenerationTemp), 1, f);
    
    // Temperature
    fread(&temperatureEnabled, sizeof(temperatureEnabled), 1, f);
    fread(&ambientSurfaceTemp, sizeof(ambientSurfaceTemp), 1, f);
    fread(&ambientDepthDecay, sizeof(ambientDepthDecay), 1, f);
    fread(&heatTransferInterval, sizeof(heatTransferInterval), 1, f);
    fread(&tempDecayInterval, sizeof(tempDecayInterval), 1, f);
    fread(&heatSourceTemp, sizeof(heatSourceTemp), 1, f);
    fread(&coldSourceTemp, sizeof(coldSourceTemp), 1, f);
    fread(&heatRiseBoost, sizeof(heatRiseBoost), 1, f);
    fread(&heatSinkReduction, sizeof(heatSinkReduction), 1, f);
    fread(&heatDecayPercent, sizeof(heatDecayPercent), 1, f);
    fread(&diagonalTransferPercent, sizeof(diagonalTransferPercent), 1, f);
    
    // Ground wear
    fread(&groundWearEnabled, sizeof(groundWearEnabled), 1, f);
    fread(&wearGrassToDirt, sizeof(wearGrassToDirt), 1, f);
    fread(&wearDirtToGrass, sizeof(wearDirtToGrass), 1, f);
    fread(&wearTrampleAmount, sizeof(wearTrampleAmount), 1, f);
    fread(&wearDecayRate, sizeof(wearDecayRate), 1, f);
    fread(&wearRecoveryInterval, sizeof(wearRecoveryInterval), 1, f);
    fread(&wearMax, sizeof(wearMax), 1, f);
    
    // Trees (added in version 13)
    if (version >= 13) {
        fread(&saplingGrowTicks, sizeof(saplingGrowTicks), 1, f);
        fread(&trunkGrowTicks, sizeof(trunkGrowTicks), 1, f);
        fread(&saplingRegrowthEnabled, sizeof(saplingRegrowthEnabled), 1, f);
        fread(&saplingRegrowthChance, sizeof(saplingRegrowthChance), 1, f);
        fread(&saplingMinTreeDistance, sizeof(saplingMinTreeDistance), 1, f);
    }
    
    // === END MARKER ===
    fread(&marker, sizeof(marker), 1, f);
    if (marker != MARKER_END) {
        printf("ERROR: Bad END marker: 0x%08X (expected 0x%08X) - file may be truncated or corrupted\n", marker, MARKER_END);
        AddMessage(TextFormat("Bad END marker: 0x%08X (file may be truncated or corrupted)", marker), RED);
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    // Reset simulation accumulators (they weren't saved, grid data was)
    ResetSmokeAccumulators();
    ResetSteamAccumulators();
    
    // Rebuild active cell counters from loaded simulation grids
    RebuildSimActivityCounts();
    
    // Rebuild HPA* graph after loading - mark all chunks dirty
    hpaNeedsRebuild = true;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y += chunkHeight) {
            for (int x = 0; x < gridWidth; x += chunkWidth) {
                MarkChunkDirty(x, y, z);
            }
        }
    }
    
    // Rebuild spatial grids
    BuildMoverSpatialGrid();
    BuildItemSpatialGrid();
    
    // Validate and cleanup any invalid ramps (e.g., from older saves)
    int removedRamps = ValidateAllRamps();
    if (removedRamps > 0) {
        AddMessage(TextFormat("Cleaned up %d invalid ramps", removedRamps), YELLOW);
    }
    
    // Rebuild pathfinding graph (entrances + edges)
    BuildEntrances();
    BuildGraph();
    
    return true;
}
