// core/saveload.c - Save and Load functions
#include "../game_state.h"

#define SAVE_VERSION 2  // Added fire, smoke, and cellFlags
#define SAVE_MAGIC 0x4E41564B  // "NAVK"

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
    
    // Grid dimensions
    fwrite(&gridWidth, sizeof(gridWidth), 1, f);
    fwrite(&gridHeight, sizeof(gridHeight), 1, f);
    fwrite(&gridDepth, sizeof(gridDepth), 1, f);
    fwrite(&chunkWidth, sizeof(chunkWidth), 1, f);
    fwrite(&chunkHeight, sizeof(chunkHeight), 1, f);
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(grid[z][y], sizeof(CellType), gridWidth, f);
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
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fwrite(designations[z][y], sizeof(Designation), gridWidth, f);
        }
    }
    
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
    
    // Movers
    fwrite(&moverCount, sizeof(moverCount), 1, f);
    fwrite(movers, sizeof(Mover), moverCount, f);
    
    // Jobs
    fwrite(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fwrite(&activeJobCount, sizeof(activeJobCount), 1, f);
    fwrite(jobs, sizeof(Job), jobHighWaterMark, f);
    fwrite(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fwrite(activeJobList, sizeof(int), activeJobCount, f);
    
    // View state
    fwrite(&currentViewZ, sizeof(currentViewZ), 1, f);
    fwrite(&zoom, sizeof(zoom), 1, f);
    fwrite(&offset, sizeof(offset), 1, f);
    
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
        AddMessage("Invalid save file (bad magic)", RED);
        fclose(f);
        return false;
    }
    
    if (version != SAVE_VERSION) {
        AddMessage(TextFormat("Save version mismatch (file: %d, current: %d)", version, SAVE_VERSION), RED);
        fclose(f);
        return false;
    }
    
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
    
    // Grid cells
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(grid[z][y], sizeof(CellType), gridWidth, f);
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
    
    // Cell flags
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(cellFlags[z][y], sizeof(uint8_t), gridWidth, f);
        }
    }
    
    // Designations
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            fread(designations[z][y], sizeof(Designation), gridWidth, f);
        }
    }
    
    // Items
    fread(&itemHighWaterMark, sizeof(itemHighWaterMark), 1, f);
    fread(items, sizeof(Item), itemHighWaterMark, f);
    
    // Stockpiles
    fread(stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    
    // Gather zones
    fread(&gatherZoneCount, sizeof(gatherZoneCount), 1, f);
    fread(gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    fread(blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Movers
    fread(&moverCount, sizeof(moverCount), 1, f);
    fread(movers, sizeof(Mover), moverCount, f);
    
    // Jobs
    fread(&jobHighWaterMark, sizeof(jobHighWaterMark), 1, f);
    fread(&activeJobCount, sizeof(activeJobCount), 1, f);
    fread(jobs, sizeof(Job), jobHighWaterMark, f);
    fread(jobIsActive, sizeof(bool), jobHighWaterMark, f);
    fread(activeJobList, sizeof(int), activeJobCount, f);
    
    // View state
    fread(&currentViewZ, sizeof(currentViewZ), 1, f);
    fread(&zoom, sizeof(zoom), 1, f);
    fread(&offset, sizeof(offset), 1, f);
    
    fclose(f);
    
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
    
    return true;
}
