// describe.c - Entity descriptions as toned text lines (see describe.h)
#include "describe.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "../entities/jobs.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/furniture.h"
#include "../entities/namegen.h"
#include "../entities/animals.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/trees.h"
#include "../simulation/floordirt.h"
#include "../simulation/rooms.h"
#include "../simulation/farming.h"
#include "../simulation/temperature.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/steam.h"
#include "../simulation/smoke.h"
#include "../world/designations.h"
#include "../world/material.h"
#include "../simulation/balance.h"
#include "../simulation/mood.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void DescAdd(DescLines* d, DescTone tone, const char* fmt, ...) {
    if (d->count >= DESC_MAX_LINES) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d->lines[d->count].text, DESC_LINE_LEN, fmt, ap);
    va_end(ap);
    d->lines[d->count].tone = tone;
    d->count++;
}

// "Oak Planks", "Granite Rock", "Berries": material prefix where the item type uses one
void DescItemName(const Item* item, char* out, size_t outSize) {
    const char* base = (item->type < ITEM_TYPE_COUNT) ? ItemName(item->type) : "?";
    MaterialType mat = (MaterialType)item->material;
    if (mat == MAT_NONE) {
        mat = (MaterialType)DefaultMaterialForItemType(item->type);
    }
    if (mat != MAT_NONE && ItemTypeUsesMaterialName(item->type)) {
        char matName[32];
        snprintf(matName, sizeof(matName), "%s", MaterialName(mat));
        if (matName[0] >= 'a' && matName[0] <= 'z') {
            matName[0] = (char)(matName[0] - 'a' + 'A');
        }
        snprintf(out, outSize, "%s %s", matName, base);
        return;
    }
    snprintf(out, outSize, "%s", base);
}

// ---------------------------------------------------------------------------
// Need labels (thresholds shared by every screen)
// ---------------------------------------------------------------------------

static const char* LevelLabel(float v, const char* words[4], DescTone* tone) {
    if (v > 0.5f) { *tone = DESC_GOOD; return words[0]; }
    if (v > 0.3f) { *tone = DESC_MILD; return words[1]; }
    if (v > 0.1f) { *tone = DESC_WARN; return words[2]; }
    *tone = DESC_BAD; return words[3];
}

const char* HungerLabel(float hunger, DescTone* tone) {
    static const char* w[4] = {"Full", "Peckish", "Hungry", "Starving"};
    return LevelLabel(hunger, w, tone);
}
const char* ThirstLabel(float thirst, DescTone* tone) {
    static const char* w[4] = {"Hydrated", "Parched", "Thirsty", "Dehydrating"};
    return LevelLabel(thirst, w, tone);
}
const char* BladderLabel(float bladder, DescTone* tone) {
    static const char* w[4] = {"Fine", "Need to go", "Urgent", "Desperate"};
    return LevelLabel(bladder, w, tone);
}
const char* EnergyLabel(float energy, DescTone* tone) {
    static const char* w[4] = {"Rested", "Drowsy", "Tired", "Exhausted"};
    return LevelLabel(energy, w, tone);
}

const char* BodyTempLabel(float bodyTemp, DescTone* tone) {
    if (bodyTemp < balance.severeColdThreshold)   { *tone = DESC_COLD; return "Hypothermic"; }
    if (bodyTemp < balance.moderateColdThreshold) { *tone = DESC_COLD; return "Freezing"; }
    if (bodyTemp < balance.mildColdThreshold)     { *tone = DESC_INFO; return "Cold"; }
    if (bodyTemp > balance.heatThreshold)         { *tone = DESC_BAD;  return "Overheating"; }
    *tone = DESC_GOOD; return "Normal";
}

DescTone MoodTone(float mood) {
    if (mood < -0.5f) return DESC_BAD;
    if (mood < -0.2f) return DESC_WARN;
    if (mood < 0.2f)  return DESC_MILD;
    if (mood < 0.5f)  return DESC_GOOD;
    return DESC_GREAT;
}

// ---------------------------------------------------------------------------
// Mover
// ---------------------------------------------------------------------------

static bool ValidFurniture(int idx) {
    return idx >= 0 && idx < MAX_FURNITURE && furniture[idx].active;
}

static void DescribeMoverNeeds(const Mover* m, const Job* job, DescLines* d) {
    DescTone tone;

    // Hunger
    {
        const char* label = HungerLabel(m->hunger, &tone);
        float hoursToStarve = (balance.hungerDrainPerGH > 0) ? m->hunger / balance.hungerDrainPerGH : 999.0f;
        DescAdd(d, tone, "Hunger: %.0f%% (%s) — starving in %.1fh", m->hunger * 100.0f, label, hoursToStarve);
        if (m->freetimeState == FREETIME_SEEKING_FOOD) DescAdd(d, DESC_WARN, "  Seeking food...");
        else if (m->freetimeState == FREETIME_EATING)
            DescAdd(d, DESC_GOOD, "  Eating (%.0f%%)", (m->needProgress / 2.0f) * 100.0f);
    }

    // Thirst
    {
        const char* label = ThirstLabel(m->thirst, &tone);
        float hoursToDehydrate = (balance.thirstDrainPerGH > 0) ? m->thirst / balance.thirstDrainPerGH : 999.0f;
        DescAdd(d, tone, "Thirst: %.0f%% (%s) — dehydrating in %.1fh", m->thirst * 100.0f, label, hoursToDehydrate);
        if (m->freetimeState == FREETIME_SEEKING_DRINK) DescAdd(d, DESC_WARN, "  Seeking drink...");
        else if (m->freetimeState == FREETIME_DRINKING)
            DescAdd(d, DESC_WATER, "  Drinking (%.0f%%)",
                    (m->needProgress / GameHoursToGameSeconds(balance.drinkingDurationGH)) * 100.0f);
        else if (m->freetimeState == FREETIME_SEEKING_NATURAL_WATER) DescAdd(d, DESC_WARN, "  Seeking natural water...");
        else if (m->freetimeState == FREETIME_DRINKING_NATURAL)
            DescAdd(d, DESC_WATER, "  Drinking from water (%.0f%%)",
                    (m->needProgress / GameHoursToGameSeconds(balance.naturalDrinkDurationGH)) * 100.0f);
    }

    // Bladder
    if (bladderEnabled) {
        const char* label = BladderLabel(m->bladder, &tone);
        float hoursLeft = (balance.bladderDrainPerGH > 0) ? m->bladder / balance.bladderDrainPerGH : 999.0f;
        DescAdd(d, tone, "Bladder: %.0f%% (%s) — %.1fh", m->bladder * 100.0f, label, hoursLeft);
        if (m->freetimeState == FREETIME_SEEKING_TOILET) DescAdd(d, DESC_WARN, "  Seeking toilet...");
        else if (m->freetimeState == FREETIME_USING_TOILET) DescAdd(d, DESC_GOOD, "  Using toilet");
        else if (m->freetimeState == FREETIME_SEEKING_BUSH) DescAdd(d, DESC_WARN, "  Seeking bush...");
        else if (m->freetimeState == FREETIME_USING_BUSH) DescAdd(d, DESC_MILD, "  Behind a bush...");
    }

    // Energy, with a projection: time until rested (resting) or until tired (awake)
    {
        const char* label = EnergyLabel(m->energy, &tone);
        char proj[48] = "";
        if (m->freetimeState == FREETIME_RESTING) {
            float recoveryPerGH = balance.groundRecoveryPerGH;
            const char* restOn = "ground";
            if (ValidFurniture(m->needTarget)) {
                recoveryPerGH = balance.bedRecoveryPerGH;
                restOn = "bed";
            }
            if (recoveryPerGH > 0) {
                float hoursToRested = (balance.energyWakeThreshold - m->energy) / recoveryPerGH;
                if (hoursToRested > 0) snprintf(proj, sizeof(proj), " — rested in %.1fh (%s)", hoursToRested, restOn);
            }
        } else {
            float drainPerGH = job ? balance.energyDrainWorkPerGH : balance.energyDrainIdlePerGH;
            if (drainPerGH > 0) {
                float hoursToTired = (m->energy - balance.energyTiredThreshold) / drainPerGH;
                if (hoursToTired > 0) snprintf(proj, sizeof(proj), " — tired in %.1fh", hoursToTired);
            }
        }
        DescAdd(d, tone, "Energy: %.0f%% (%s)%s", m->energy * 100.0f, label, proj);

        if (m->freetimeState == FREETIME_SEEKING_REST) {
            if (ValidFurniture(m->needTarget))
                DescAdd(d, DESC_WARN, "  Seeking %s...", GetFurnitureDef(furniture[m->needTarget].type)->name);
            else
                DescAdd(d, DESC_WARN, "  Seeking rest...");
        } else if (m->freetimeState == FREETIME_RESTING) {
            if (ValidFurniture(m->needTarget))
                DescAdd(d, DESC_GOOD, "  Resting (%s)", GetFurnitureDef(furniture[m->needTarget].type)->name);
            else
                DescAdd(d, DESC_GOOD, "  Resting (ground)");
        } else if (m->freetimeState == FREETIME_SEEKING_WARMTH) {
            DescAdd(d, DESC_WARN, "  Seeking warmth...");
        } else if (m->freetimeState == FREETIME_WARMING) {
            DescAdd(d, DESC_MILD, "  Warming up by fire");
        }
    }

    // Body temperature
    {
        const char* label = BodyTempLabel(m->bodyTemp, &tone);
        DescAdd(d, tone, "Body Temp: %.1fC (%s)", m->bodyTemp, label);
    }
}

static void DescribeMoverMood(const Mover* m, DescLines* d) {
    if (!moodEnabled) return;
    DescAdd(d, MoodTone(m->mood), "Mood: %.0f%% (%s)", (m->mood + 1.0f) * 50.0f, MoodLevelName(m->mood));

    for (int mi = 0; mi < m->moodletCount; mi++) {
        if (m->moodlets[mi].remainingTime <= 0.0f) continue;
        float hoursLeft = m->moodlets[mi].remainingTime / GameHoursToGameSeconds(1.0f);
        DescAdd(d, m->moodlets[mi].value >= 0 ? DESC_GOOD : DESC_WARN, "  %s %+.0f (%.1fh)",
                moodletDefs[m->moodlets[mi].type].name, m->moodlets[mi].value, hoursLeft);
    }

    if (m->traits[0] != TRAIT_NONE || m->traits[1] != TRAIT_NONE) {
        char traitBuf[64] = "Traits: ";
        if (m->traits[0] != TRAIT_NONE)
            strncat(traitBuf, traitDefs[m->traits[0]].name, sizeof(traitBuf) - strlen(traitBuf) - 1);
        if (m->traits[1] != TRAIT_NONE) {
            strncat(traitBuf, ", ", sizeof(traitBuf) - strlen(traitBuf) - 1);
            strncat(traitBuf, traitDefs[m->traits[1]].name, sizeof(traitBuf) - strlen(traitBuf) - 1);
        }
        DescAdd(d, DESC_INFO, "%s", traitBuf);
    }
}

static void DescribeMoverGear(const Mover* m, const Job* job, DescLines* d) {
    char name[64];
    int carrying = job ? job->carryingItem : -1;
    if (carrying >= 0 && items[carrying].active) {
        DescItemName(&items[carrying], name, sizeof(name));
        DescAdd(d, DESC_WARN, "Carrying: #%d (%s)", carrying, name);
    } else {
        DescAdd(d, DESC_MUTED, "Carrying: none");
    }

    if (m->equippedTool >= 0 && m->equippedTool < MAX_ITEMS && items[m->equippedTool].active) {
        DescItemName(&items[m->equippedTool], name, sizeof(name));
        DescAdd(d, DESC_INFO, "Equipped: %s", name);
    } else {
        DescAdd(d, DESC_MUTED, "Equipped: none");
    }

    if (m->equippedClothing >= 0 && m->equippedClothing < MAX_ITEMS && items[m->equippedClothing].active) {
        DescItemName(&items[m->equippedClothing], name, sizeof(name));
        float reduction = GetClothingCoolingReduction(items[m->equippedClothing].type);
        DescAdd(d, DESC_INFO, "Clothing: %s (%.0f%% insulation)", name, reduction * 100.0f);
    } else {
        DescAdd(d, DESC_MUTED, "Clothing: none");
    }
}

static void DescribeMoverDebug(const Mover* m, const Job* job, DescLines* d) {
    DescAdd(d, m->pathLength > 0 ? DESC_NORMAL : DESC_BAD, "Path: %d/%d, Goal: (%d,%d,z%d)",
            m->pathIndex >= 0 ? m->pathIndex + 1 : 0, m->pathLength, m->goal.x, m->goal.y, m->goal.z);

    if (m->needsRepath) DescAdd(d, DESC_BAD, "NEEDS REPATH (cooldown: %d)", m->repathCooldown);
    else if (m->repathCooldown > 0) DescAdd(d, DESC_MUTED, "Repath cooldown: %d", m->repathCooldown);

    if (m->timeWithoutProgress > 0.5f) {
        DescAdd(d, m->timeWithoutProgress > 2.0f ? DESC_BAD : DESC_WARN, "No progress: %.1fs%s",
                m->timeWithoutProgress, m->timeWithoutProgress > 2.0f ? " STUCK!" : "");
    }

    if (job && job->targetStockpile >= 0) {
        DescAdd(d, DESC_NORMAL, "Target SP: %d, Slot: (%d,%d)", job->targetStockpile, job->targetSlotX, job->targetSlotY);
    }

    if (job && job->targetItem >= 0 && items[job->targetItem].active) {
        const Item* item = &items[job->targetItem];
        char name[64];
        DescItemName(item, name, sizeof(name));
        float dx = m->x - item->x, dy = m->y - item->y;
        float dist = sqrtf(dx * dx + dy * dy);
        float pickupRadius = CELL_SIZE * 0.75f;
        DescAdd(d, DESC_INFO, "Target: #%d %s at (%.0f,%.0f)", job->targetItem, name, item->x, item->y);
        DescAdd(d, dist < pickupRadius ? DESC_GOOD : DESC_BAD, "  dist=%.1f %s",
                dist, dist < pickupRadius ? "IN RANGE" : "OUT OF RANGE");
    }

    if (job && (job->type == JOBTYPE_MINE || job->type == JOBTYPE_CHANNEL || job->type == JOBTYPE_REMOVE_FLOOR)) {
        DescAdd(d, DESC_WARN, "Mining: (%d,%d,z%d) %.0f%%",
                job->targetMineX, job->targetMineY, job->targetMineZ, job->progress * 100);
    }

    if (job && job->targetBlueprint >= 0) {
        const Blueprint* bp = &blueprints[job->targetBlueprint];
        DescAdd(d, DESC_INFO, "Blueprint: #%d at (%d,%d,z%d)", job->targetBlueprint, bp->x, bp->y, bp->z);
    }

    if (m->transportState != TRANSPORT_NONE) {
        static const char* tsNames[] = {"NONE", "WALK_TO_STN", "WAITING", "RIDING"};
        int ts = m->transportState;
        DescAdd(d, DESC_MILD, "Transport: %s (stn=%d, exit=%d)", (ts >= 0 && ts < 4) ? tsNames[ts] : "?",
                m->transportStation, m->transportExitStation);
        DescAdd(d, DESC_MILD, "  FinalGoal: (%d,%d,z%d)",
                m->transportFinalGoal.x, m->transportFinalGoal.y, m->transportFinalGoal.z);
    }
}

void DescribeMover(int idx, DescDetail detail, DescLines* out) {
    out->count = 0;
    if (idx < 0 || idx >= moverCount || !movers[idx].active) return;
    const Mover* m = &movers[idx];
    const Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    // Title: index + name, or "Mover #N"
    DescTone header = (m->gender == GENDER_FEMALE) ? DESC_HEADER_FEMALE : DESC_HEADER_MALE;
    const char* drafted = m->isDrafted ? " [DRAFTED]" : "";
    if (m->name[0]) DescAdd(out, header, "#%d %s%s", idx, m->name, drafted);
    else DescAdd(out, header, "Mover #%d%s", idx, drafted);

    if (detail == DESC_DEBUG) {
        DescAdd(out, DESC_NORMAL, "Pos: (%.1f, %.1f, z%.0f) cell (%d,%d)",
                m->x, m->y, m->z, (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE));
    }

    DescribeMoverNeeds(m, job, out);
    DescribeMoverMood(m, out);

    DescAdd(out, job ? DESC_GOOD : DESC_MUTED, "Job: %s (step %d)",
            job ? JobTypeName(job->type) : "IDLE", job ? job->step : 0);
    DescribeMoverGear(m, job, out);

    if (detail == DESC_DEBUG) DescribeMoverDebug(m, job, out);
}

// ---------------------------------------------------------------------------
// Animal
// ---------------------------------------------------------------------------

void DescribeAnimal(int idx, DescLines* out) {
    out->count = 0;
    if (idx < 0 || idx >= animalCount || !animals[idx].active) return;
    const Animal* a = &animals[idx];
    static const char* stateNames[] = {"Idle", "Walking", "Grazing", "Hunting", "Being Hunted"};
    static const char* typeNames[] = {"Grazer", "Predator"};
    static const char* behaviorNames[] = {"Simple", "Steering", "Predator"};

    DescAdd(out, DESC_TITLE, "Animal #%d (%s, %s)", idx, typeNames[a->type], behaviorNames[a->behavior]);
    int cellX = (int)(a->x / CELL_SIZE), cellY = (int)(a->y / CELL_SIZE);
    DescAdd(out, DESC_NORMAL, "Pos: cell (%d,%d) z%.0f", cellX, cellY, a->z);
    DescAdd(out, a->state == ANIMAL_GRAZING ? DESC_GOOD : (a->state == ANIMAL_WALKING ? DESC_INFO : DESC_MUTED),
            "State: %s", stateNames[a->state]);

    if (a->state == ANIMAL_GRAZING) {
        DescAdd(out, DESC_GOOD, "Grazing: veg=%d, timer=%.1f/%.1f",
                (int)GetVegetation(cellX, cellY, (int)a->z), a->grazeTimer, ANIMAL_GRAZE_TIME);
    } else if (a->state == ANIMAL_WALKING) {
        DescAdd(out, DESC_INFO, "Target: (%d,%d)", a->targetCellX, a->targetCellY);
    } else if (a->state == ANIMAL_HUNTING) {
        if (a->targetAnimalIdx >= 0) DescAdd(out, DESC_BAD, "Prey: #%d", a->targetAnimalIdx);
        else DescAdd(out, DESC_BAD, "Prey: none");
    }

    if (a->markedForHunt) DescAdd(out, DESC_BAD, "Marked for Hunt");
    if (a->reservedByHunter >= 0) DescAdd(out, DESC_SOFT_BAD, "Hunter: Mover #%d", a->reservedByHunter);
}

// ---------------------------------------------------------------------------
// Cell
// ---------------------------------------------------------------------------

static void DescribeCellFarm(int x, int y, int z, DescLines* out) {
    const FarmCell* fc = &FARM(x, y, z);
    if (!fc->tilled) return;
    static const char* cropNames[] = {"None", "Wheat", "Lentils", "Flax"};
    static const char* stageNames[] = {"Bare", "Sprouted", "Growing", "Mature", "Ripe"};
    DescAdd(out, DESC_NORMAL, "Farm Plot (%s)", MaterialName(GetWallMaterial(x, y, z - 1)));
    DescAdd(out, DESC_NORMAL, "Fertility: %d/255", fc->fertility);
    if (fc->cropType != CROP_NONE) {
        const char* cname = (fc->cropType < CROP_TYPE_COUNT) ? cropNames[fc->cropType] : "?";
        const char* sname = (fc->growthStage <= CROP_STAGE_RIPE) ? stageNames[fc->growthStage] : "?";
        DescAdd(out, DESC_NORMAL, "Crop: %s (%s)", cname, sname);
        if (fc->frostDamaged) DescAdd(out, DESC_NORMAL, "Frost damaged!");
    } else if (fc->desiredCropType != CROP_NONE) {
        DescAdd(out, DESC_NORMAL, "Queued: %s",
                (fc->desiredCropType < CROP_TYPE_COUNT) ? cropNames[fc->desiredCropType] : "?");
    }
    if (fc->weedLevel > WEED_THRESHOLD) DescAdd(out, DESC_NORMAL, "Weeds: heavy (%d)", fc->weedLevel);
    else if (fc->weedLevel > 0) DescAdd(out, DESC_NORMAL, "Weeds: light (%d)", fc->weedLevel);
}

static void DescribeCellFluids(int x, int y, int z, DescLines* out) {
    const WaterCell* water = &WATER(x, y, z);
    if (water->level > 0 || water->isSource || water->isDrain) {
        DescAdd(out, water->isFrozen ? DESC_ICE : DESC_WATER, "Water: %d/7%s",
                water->level, water->isFrozen ? " [FROZEN]" : "");
        if (water->isSource) DescAdd(out, DESC_WATER, "  WATER SOURCE");
        if (water->isDrain) DescAdd(out, DESC_MUTED, "  WATER DRAIN");
        if (water->hasPressure) DescAdd(out, DESC_NORMAL, "  Pressure (z=%d)", water->pressureSourceZ);
    }

    const FireCell* fire = &FIRE(x, y, z);
    if (fire->level > 0 || fire->isSource) {
        DescAdd(out, DESC_WARN, "Fire: %d/7", fire->level);
        if (fire->isSource) DescAdd(out, DESC_WARN, "  FIRE SOURCE");
        if (fire->fuel > 0) DescAdd(out, DESC_NORMAL, "  Fuel: %d", fire->fuel);
    }

    const SteamCell* steam = &STEAM(x, y, z);
    if (steam->level > 0) DescAdd(out, DESC_STEAM, "Steam: %d/7", steam->level);

    const SmokeCell* smoke = &SMOKE(x, y, z);
    if (smoke->level > 0) DescAdd(out, DESC_MUTED, "Smoke: %d/7", smoke->level);
}

void DescribeCell(int x, int y, int z, DescLines* out) {
    out->count = 0;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (!IsExplored(x, y, z)) {
        DescAdd(out, DESC_MUTED, "Unexplored");
        return;
    }

    DescAdd(out, DESC_TITLE, "Cell (%d, %d, z%d)", x, y, z);

    // Type; trees name their wood, burned cells say so
    CellType ct = GRID(x, y, z);
    bool burned = HAS_CELL_FLAG(x, y, z, CELL_FLAG_BURNED);
    MaterialType treeMat = MAT_NONE;
    if (ct == CELL_TREE_TRUNK || ct == CELL_TREE_BRANCH || ct == CELL_TREE_ROOT ||
        ct == CELL_TREE_FELLED || ct == CELL_TREE_LEAVES || ct == CELL_SAPLING) {
        treeMat = GetWallMaterial(x, y, z);
    }
    DescTone typeTone = burned ? DESC_BURNT : DESC_NORMAL;
    const char* burnedTag = burned ? " [BURNED]" : "";
    if (IsWoodMaterial(treeMat)) {
        DescAdd(out, typeTone, "Type: %s (%s)%s", CellName(ct), TreeTypeName(treeMat), burnedTag);
        if (ct == CELL_TREE_TRUNK) {
            int baseZ = z;
            while (baseZ > 0 && GRID(x, y, baseZ - 1) == CELL_TREE_TRUNK) baseZ--;
            uint8_t hs = TREE_HARVEST(x, y, baseZ);
            if (hs > 0) DescAdd(out, DESC_NORMAL, "Harvest: %d/%d", hs, TREE_HARVEST_MAX);
            else DescAdd(out, DESC_NORMAL, "Harvest: depleted");
        }
    } else {
        DescAdd(out, typeTone, "Type: %s%s", CellName(ct), burnedTag);
    }

    // Wall and floor material
    MaterialType wallMat = GetWallMaterial(x, y, z);
    if (wallMat != MAT_NONE && CellBlocksMovement(ct)) {
        DescAdd(out, DESC_NORMAL, "Wall: %s%s", MaterialName(wallMat), IsWallNatural(x, y, z) ? " (natural)" : "");
    }
    MaterialType floorMat = GetFloorMaterial(x, y, z);
    if (floorMat != MAT_NONE) {
        DescAdd(out, DESC_NORMAL, "Floor: %s%s", MaterialName(floorMat), IsFloorNatural(x, y, z) ? " (natural)" : "");
    }

    // Dirt on the floor
    int dirt = GetFloorDirt(x, y, z);
    if (dirt > 0) {
        const char* desc = dirt >= DIRT_CLEAN_THRESHOLD ? "Dirty" :
                           dirt >= DIRT_VISIBLE_THRESHOLD ? "Slightly dirty" : "Trace dirt";
        DescAdd(out, DESC_NORMAL, "Cleanliness: %s (%d%s)", desc, dirt,
                IsStoneMaterial(floorMat) ? ", stone: 50% rate" : "");
    }

    // Room
    const DetectedRoom* room = GetRoom(GetRoomAt(x, y, z));
    if (room) DescAdd(out, DESC_NORMAL, "Room: %s (q: %+.2f)", RoomTypeName(room->type), room->quality);

    DescribeCellFarm(x, y, z, out);

    // Wetness / mud (this cell, else the ground below)
    {
        int wetZ = z;
        int wetness = GET_CELL_WETNESS(x, y, wetZ);
        if (wetness == 0 && wetZ > 0) {
            wetZ = z - 1;
            wetness = GET_CELL_WETNESS(x, y, wetZ);
        }
        if (wetness > 0) {
            bool muddy = IsMuddy(x, y, wetZ);
            const char* wet = wetness >= 3 ? (muddy ? "Soaked (muddy)" : "Soaked") :
                              wetness >= 2 ? (muddy ? "Wet (muddy)" : "Wet") : "Damp";
            DescAdd(out, DESC_NORMAL, "Wetness: %s", wet);
        }
    }

    // Temperature and sources
    int temp = GetTemperature(x, y, z);
    int ambient = GetAmbientTemperature(z);
    DescAdd(out, DESC_NORMAL, "Temp: %d C", temp);
    DescAdd(out, DESC_NORMAL, "Ambient: %d C [%+d]", ambient, temp - ambient);
    const TempCell* tc = &TEMPERATURE(x, y, z);
    if (tc->isHeatSource) DescAdd(out, DESC_BAD, "HEAT SOURCE (%d C)", heatSourceTemp);
    if (tc->isColdSource) DescAdd(out, DESC_INFO, "COLD SOURCE (%d C)", coldSourceTemp);

    int tier = GetInsulationTier(x, y, z);
    DescAdd(out, DESC_NORMAL, "Insulation: %s", tier == 2 ? "Stone (5%)" : tier == 1 ? "Wood (20%)" : "Air (100%)");

    DescribeCellFluids(x, y, z, out);
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

void DescribeItemLine(int idx, DescLines* out) {
    if (idx < 0 || idx >= MAX_ITEMS) return;
    const Item* item = &items[idx];
    static const char* stateNames[] = {"Ground", "Carried", "Stockpile"};
    char name[64];
    DescItemName(item, name, sizeof(name));
    const char* state = (item->state >= 0 && item->state < 3) ? stateNames[item->state] : "?";

    // Freshness for spoilable items
    const char* freshness = "";
    bool spoils = ItemSpoils(item->type);
    if (spoils) {
        if (item->condition == CONDITION_ROTTEN) freshness = " Rotten";
        else if (item->condition == CONDITION_STALE) freshness = " Stale";
        else freshness = " Fresh";
    }
    // Hydration for drinkable items
    char hydration[32] = "";
    float h = GetItemHydration(item->type);
    if (h > 0.0f) snprintf(hydration, sizeof(hydration), " +%.0f%% hydration", h * 100.0f);
    // Temperature for hot food/drink
    const char* temp = item->temperature >= 60.0f ? " piping hot" : (item->temperature >= 40.0f ? " warm" : "");

    // Color: test crates by their color, spoiling food tinted
    DescTone tone = DESC_NORMAL;
    if (item->type == ITEM_RED) tone = DESC_BAD;
    else if (item->type == ITEM_GREEN) tone = DESC_GOOD;
    else if (item->type == ITEM_BLUE) tone = DESC_BLUE;
    if (spoils) {
        if (item->condition == CONDITION_ROTTEN) tone = DESC_SOFT_BAD;
        else if (item->condition == CONDITION_STALE) tone = DESC_SOFT_WARN;
    }

    if (item->stackCount > 1)
        DescAdd(out, tone, "#%d: %s x%d (%s%s%s%s)", idx, name, item->stackCount, state, freshness, hydration, temp);
    else
        DescAdd(out, tone, "#%d: %s (%s%s%s%s)", idx, name, state, freshness, hydration, temp);
}

void DescribeItemsAt(const int* itemIndices, int itemCount, int cellX, int cellY, DescLines* out) {
    out->count = 0;
    if (itemCount <= 0) return;
    int totalUnits = 0;
    for (int i = 0; i < itemCount; i++) totalUnits += items[itemIndices[i]].stackCount;
    DescAdd(out, DESC_MILD, "Cell (%d,%d): %d unit%s", cellX, cellY, totalUnits, totalUnits == 1 ? "" : "s");
    // At most 16 item lines, as before
    for (int i = 0; i < itemCount && i < 16; i++) DescribeItemLine(itemIndices[i], out);
}
