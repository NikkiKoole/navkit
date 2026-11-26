// describe.c - Entity descriptions as toned text lines (see describe.h)
#include "describe.h"
#include "../entities/town.h"
#include "../entities/action_queue.h"
#include "../entities/containers.h"
#include "../entities/inventory.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "../entities/jobs.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/furniture.h"
#include "../entities/namegen.h"
#include "../entities/animals.h"
#include "../entities/notice.h"
#include "../entities/talk.h"
#include "../entities/relations.h"
#include "../core/time.h"
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
#include "../world/construction.h"
#include "../entities/tool_quality.h"
#include "../entities/trains.h"
#include "../entities/workshops.h"
#include "../entities/stockpiles.h"
#include "../world/designations.h"
#include "../world/material.h"
#include "../simulation/balance.h"
#include "../simulation/mood.h"
#include "../simulation/needs.h"
#include <math.h>
#include "../entities/social.h"
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
const char* SocialLabel(float social, DescTone* tone) {
    static const char* w[4] = {"Content", "Wants company", "Lonely", "Very lonely"};
    return LevelLabel(social, w, tone);
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

static void DescribeMoverNeeds(const Mover* m, DescLines* d) {
    DescTone tone;

    // Hunger
    {
        const char* label = HungerLabel(m->hunger, &tone);
        float hungerPerGH = MoverHungerDrainPerGH((int)(m - movers));
        float hoursToStarve = (hungerPerGH > 0) ? m->hunger / hungerPerGH : 999.0f;
        DescAdd(d, tone, "Hunger: %.0f%% (%s) — starving in %.1fh", m->hunger * 100.0f, label, hoursToStarve);
        if (m->freetimeState == FREETIME_SEEKING_FOOD) DescAdd(d, DESC_WARN, "  Seeking food...");
        else if (m->freetimeState == FREETIME_SEEKING_FORAGE) DescAdd(d, DESC_WARN, "  Going to pick berries...");
        else if (m->freetimeState == FREETIME_FORAGING) DescAdd(d, DESC_GOOD, "  Picking berries");
        else if (m->freetimeState == FREETIME_SEEKING_SCROUNGE) DescAdd(d, DESC_WARN, "  Going to search a bin...");
        else if (m->freetimeState == FREETIME_SCROUNGING) DescAdd(d, DESC_MILD, "  Searching a bin");
        if (m->freetimeState == FREETIME_FETCHING_OWN && m->needTarget >= 0 && m->needTarget < MAX_ITEMS)
            DescAdd(d, DESC_INFO, "  Going back for its %s", ItemName(items[m->needTarget].type));
        else if (m->freetimeState == FREETIME_EATING)
            DescAdd(d, DESC_GOOD, "  Eating (%.0f%%)", (m->needProgress / 2.0f) * 100.0f);
    }

    // Thirst
    {
        const char* label = ThirstLabel(m->thirst, &tone);
        float thirstPerGH = MoverThirstDrainPerGH((int)(m - movers));
        float hoursToDehydrate = (thirstPerGH > 0) ? m->thirst / thirstPerGH : 999.0f;
        DescAdd(d, tone, "Thirst: %.0f%% (%s) — dehydrating in %.1fh", m->thirst * 100.0f, label, hoursToDehydrate);
        if (m->freetimeState == FREETIME_SEEKING_DRINK) DescAdd(d, DESC_WARN, "  Seeking drink...");
        else if (m->freetimeState == FREETIME_DRINKING)
            DescAdd(d, DESC_WATER, "  Drinking (%.0f%%)",
                    (m->needProgress / GameHoursToGameSeconds(balance.drinkingDurationGH)) * 100.0f);
        else if (m->freetimeState == FREETIME_SEEKING_TAP) DescAdd(d, DESC_WARN, "  Going to the tap...");
        else if (m->freetimeState == FREETIME_DRINKING_TAP)
            DescAdd(d, DESC_WATER, "  Drinking at the tap (%.0f%%)",
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

    // Company (social.h): only where people have a social life
    if (SocialActive()) {
        int idx = (int)(m - movers), with[16];
        const char* label = SocialLabel(moverSocial[idx], &tone);
        DescAdd(d, tone, "Social: %.0f%% (%s)", moverSocial[idx] * 100.0f, label);
        int n = SocialCompany(idx, with, 16);
        if (n > 0) {
            bool wanted = SocialCompanyScale(idx, with, n) > 0.0f;
            char who[32];
            if (n == 1) snprintf(who, sizeof(who), "%s", movers[with[0]].name[0] ? movers[with[0]].name : "someone");
            else snprintf(who, sizeof(who), "%d others", n);
            DescAdd(d, wanted ? DESC_GOOD : DESC_MUTED, "  In company (with %s)%s", who, wanted ? "" : ": not the company it wants");
        }
    }

    // Trash in hand (what the last snack or drink left)
    if (m->heldTrash > ITEM_NONE && m->heldTrash < ITEM_TYPE_COUNT) {
        DescAdd(d, DESC_MUTED, "Holding: %s%s", ItemName(m->heldTrash),
                m->freetimeState == FREETIME_SEEKING_BIN ? " (taking it to a bin)" : "");
    }

    // Energy, with a projection: time until rested (resting) or until tired (awake)
    {
        const char* label = EnergyLabel(m->energy, &tone);
        char proj[48] = "";
        if (m->freetimeState == FREETIME_RESTING) {
            float recoveryPerGH = balance.groundRecoveryPerGH;
            const char* restOn = "ground";
            if (ValidFurniture(m->needTarget)) {
                recoveryPerGH = FurnitureRecoveryPerGH(furniture[m->needTarget].type);
                restOn = GetFurnitureDef(furniture[m->needTarget].type)->name;
            }
            if (recoveryPerGH > 0) {
                float hoursToRested = (balance.energyWakeThreshold - m->energy) / recoveryPerGH;
                if (hoursToRested > 0) snprintf(proj, sizeof(proj), " — rested in %.1fh (%s)", hoursToRested, restOn);
            }
            if (MoverLyingIn((int)(m - movers))) snprintf(proj, sizeof(proj), " — lying in until it gets light");
        } else {
            float drainPerGH = MoverEnergyDrainPerGH((int)(m - movers));
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
            DescAdd(d, DESC_MILD, "  Warming up");
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
        const ContainerDef* pockets = GetContainerDef(items[m->equippedClothing].type);
        if (pockets)
            DescAdd(d, DESC_INFO, "Clothing: %s (%.0f%% insulation, pockets %.1f/%.0f L)", name, reduction * 100.0f,
                    ContainerContentsVolume(m->equippedClothing), pockets->capacityL);
        else
            DescAdd(d, DESC_INFO, "Clothing: %s (%.0f%% insulation)", name, reduction * 100.0f);
    } else {
        DescAdd(d, DESC_MUTED, "Clothing: none");
    }

    // Hands and back (inventory.c): a two-handed item fills both
    char hand[2][64];
    for (int h = 0; h < 2; h++) {
        int it = m->hands[h];
        if (it < 0 || it >= MAX_ITEMS || !items[it].active) { snprintf(hand[h], sizeof(hand[h]), "free"); continue; }
        DescItemName(&items[it], name, sizeof(name));
        const ContainerDef* cd = GetContainerDef(items[it].type);
        if (cd && cd->capacityL > 0.0f) snprintf(hand[h], sizeof(hand[h]), "%s (%.1f/%.0f L)", name, ContainerContentsVolume(it), cd->capacityL);
        else if (cd) snprintf(hand[h], sizeof(hand[h]), "%s (%d inside)", name, items[it].contentCount);
        else snprintf(hand[h], sizeof(hand[h]), "%s", name);
    }
    int jobCarry = MoverJobCarry((int)(m - movers));
    if (jobCarry >= 0) {   // a job's load sits in the hands too
        DescItemName(&items[jobCarry], name, sizeof(name));
        bool both = ItemIsTwoHanded(items[jobCarry].type);
        if (both) snprintf(hand[0], sizeof(hand[0]), "%s, hauling", name);
        else for (int h = 0; h < 2; h++) if (m->hands[h] < 0) { snprintf(hand[h], sizeof(hand[h]), "%s, hauling", name); break; }
        if (both) DescAdd(d, DESC_INFO, "Hands: %s (both)", hand[0]);
        else DescAdd(d, DESC_INFO, "Hands: %s / %s", hand[0], hand[1]);
    } else if (m->hands[0] >= 0 && m->hands[0] == m->hands[1]) DescAdd(d, DESC_INFO, "Hands: %s (both)", hand[0]);
    else if (m->hands[0] < 0 && m->hands[1] < 0) DescAdd(d, DESC_MUTED, "Hands: free");
    else DescAdd(d, DESC_INFO, "Hands: %s / %s", hand[0], hand[1]);
    if (m->wornBack >= 0 && m->wornBack < MAX_ITEMS && items[m->wornBack].active) {
        DescItemName(&items[m->wornBack], name, sizeof(name));
        const ContainerDef* cd = GetContainerDef(items[m->wornBack].type);
        DescAdd(d, DESC_INFO, "Back: %s (%.1f/%.0f L)", name, ContainerContentsVolume(m->wornBack), cd ? cd->capacityL : 0.0f);
    }
    // Carried weight (the back counts half) and what it does to walking
    float kg = MoverCarriedWeight((int)(m - movers));
    if (kg > 0.05f) {
        float speed = CarriedSpeedFactor((int)(m - movers));
        DescAdd(d, kg > carryLimitKg * 0.75f ? DESC_WARN : DESC_INFO, "Load: %.1f kg of %.0f (walking %.0f%%)",
                kg, carryLimitKg, speed * 100.0f);
    }
}

// Who this mover knows: the 3 that matter most (player), all with numbers (debug)
static void DescribeMoverRelations(int idx, DescDetail detail, DescLines* d) {
    const MoverRelations* mr = &moverRelations[idx];
    int order[MAX_RELATIONS], n = 0;
    for (int k = 0; k < mr->count; k++)
        if (RelationGet(idx, mr->r[k].other)) order[n++] = k;
    if (n == 0) return;
    for (int a = 1; a < n; a++)   // most important first: familiarity + strength of feeling
        for (int b = a; b > 0; b--) {
            const Relation* x = &mr->r[order[b]]; const Relation* y = &mr->r[order[b - 1]];
            if (x->familiarity + fabsf(x->like) * 2.0f <= y->familiarity + fabsf(y->like) * 2.0f) break;
            int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
        }
    int show = detail == DESC_DEBUG ? n : (n < 3 ? n : 3);
    char buf[256] = "";
    size_t len = 0;
    for (int s = 0; s < show && len < sizeof(buf); s++) {
        const Relation* r = &mr->r[order[s]];
        const char* name = movers[r->other].name[0] ? movers[r->other].name : "someone";
        if (detail == DESC_DEBUG)
            len += (size_t)snprintf(buf + len, sizeof(buf) - len, "%s%s (%+.0f, %.0f)", s ? ", " : "", name,
                                    r->like, r->familiarity);
        else
            len += (size_t)snprintf(buf + len, sizeof(buf) - len, "%s%s (%s)", s ? ", " : "", name,
                                    RelationLabel(r->like, r->familiarity));
    }
    DescAdd(d, DESC_INFO, "Knows: %s", buf);
}

// Who this mover noticed (debug): "Notices: Anna 3s, #4 0s" (docs/todo/social/noticing.md)
static void DescribeMoverNotices(int idx, DescLines* d) {
    if (!noticeEnabled) return;
    const MoverNotice* n = &moverNotice[idx];
    if (n->count == 0) { DescAdd(d, DESC_MUTED, "Notices: nobody"); return; }
    char buf[160] = "";
    size_t len = 0;
    for (int k = 0; k < n->count && len < sizeof(buf); k++) {
        int j = n->e[k].mover;
        if (j < 0 || j >= moverCount || !movers[j].active) continue;
        char who[24];
        if (movers[j].name[0]) snprintf(who, sizeof(who), "%s", movers[j].name);
        else snprintf(who, sizeof(who), "#%d", j);
        len += (size_t)snprintf(buf + len, sizeof(buf) - len, "%s%s %.0fs", len ? ", " : "", who,
                                gameTime - n->e[k].since);
    }
    DescAdd(d, DESC_INFO, "Notices: %s", buf);
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

    if (townLotHighWater > 0) {   // town: where they live
        int home = TownHomeOf(idx);
        if (home >= 0) DescAdd(out, DESC_INFO, "Lives in home #%d (%d living there)", home, TownResidents(home));
        else DescAdd(out, DESC_WARN, "Homeless");
        int job = TownJobOf(idx);
        if (job >= 0) DescAdd(out, DESC_INFO, "Works at the %s #%d (%s, %d-%d h)",
                              townLots[job].kind == LOT_MILL ? "mill" : townLots[job].kind == LOT_FARM ? "farm" : "bakery", job,
                              TownLotOpen(job) ? "open" : "closed", townOpenHour, townCloseHour);
        else if (home >= 0) DescAdd(out, DESC_MUTED, "No job");
    }
    DescribeMoverNeeds(m, out);
    DescribeMoverMood(m, out);

    DescAdd(out, job ? DESC_GOOD : DESC_MUTED, "Job: %s (step %d)",
            job ? JobTypeName(job->type) : "IDLE", job ? job->step : 0);
    if (QueueBusy(idx)) {   // the action queue (action_queue.h): what it will do, in order
        char q[200];
        QueueDescribe(idx, q, sizeof(q));
        DescAdd(out, DESC_INFO, "Queue: %s", q);
    }
    DescribeMoverGear(m, job, out);
    if (MoverCoins(idx) > 0) { char mb[24]; DescAdd(out, DESC_INFO, "Money: %s", MoneyFormat(MoverCoins(idx), mb, sizeof mb)); }
    DescribeMoverRelations(idx, detail, out);
    if (MoverIsMingling(idx)) DescAdd(out, DESC_INFO, "Going over to another group");
    int partner = TalkPartner(idx);
    if (MoverIsPerforming(idx)) {
        DescAdd(out, DESC_INFO, "Performing for %d", talks[MoverTalk(idx)].count);
    } else if (MoverIsWatching(idx) && partner < 0) {   // round a place
        const char* what = talks[MoverTalk(idx)].what;
        DescAdd(out, DESC_INFO, "Gathered round %s", what ? what : "something");
    } else if (MoverIsWatching(idx) && partner >= 0) {
        if (movers[partner].name[0]) DescAdd(out, DESC_INFO, "Watching %s", movers[partner].name);
        else DescAdd(out, DESC_INFO, "Watching #%d", partner);
    } else if (MoverIsWalkingWith(idx) && partner >= 0) {
        DescAdd(out, DESC_INFO, "Walking with %s%s", movers[partner].name[0] ? movers[partner].name : "someone",
                talks[MoverTalk(idx)].count > 2 ? " and others" : "");
    } else if (partner >= 0) {
        int t = MoverTalk(idx);
        if (movers[partner].name[0]) DescAdd(out, DESC_INFO, "Talking with %s%s", movers[partner].name,
                                             talks[t].count > 2 ? " and others" : "");
        else DescAdd(out, DESC_INFO, "Talking with #%d", partner);
        if (detail == DESC_DEBUG) DescAdd(out, DESC_MUTED, "Talk %d: %.0f s left, %s", t, talks[t].timeLeft,
                                          MoverIsSpeaking(idx) ? "speaking" : "listening");
    }
    {   // its part in a game (chase.c)
        char g[80];
        if (ChaseDescribe(idx, g, sizeof(g))) {
            DescAdd(out, DESC_INFO, "%s", g);
            int a = MoverActivity(idx);
            if (detail == DESC_DEBUG && a >= 0)
                DescAdd(out, DESC_MUTED, "Game %d: %d players, %.0f s%s", a, activities[a].count, activities[a].age,
                        activities[a].timeLeft > 0.0f ? TextFormat(" of %.0f", activities[a].timeLeft) : "");
        }
    }

    // Train trip: the lines still ahead (planned when it last chose, i.e. at its last stop)
    int routeLines[TRAIN_PLAN_CHANGES_MAX + 1];
    int routeCount = TrainPlannedRoute(idx, routeLines, TRAIN_PLAN_CHANGES_MAX + 1);
    if (routeCount > 0) {
        char route[160] = "";
        size_t len = 0;
        for (int i = 0; i < routeCount && len < sizeof(route); i++)
            len += (size_t)snprintf(route + len, sizeof(route) - len, "%s%s", i ? " > " : "", TrackLineName(routeLines[i]));
        if (routeCount > 1) DescAdd(out, DESC_INFO, "By train: %s (%d change%s)", route, routeCount - 1, routeCount > 2 ? "s" : "");
        else DescAdd(out, DESC_INFO, "By train: %s line", route);
    }

    if (detail == DESC_DEBUG) {
        DescribeMoverDebug(m, job, out);
        DescribeMoverNotices(idx, out);
    }
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
                (int)GetVegetation(cellX, cellY, (int)a->z), a->grazeTimer, GameHoursToGameSeconds(ANIMAL_GRAZE_TIME_GH));
    } else if (a->state == ANIMAL_WALKING) {
        DescAdd(out, DESC_INFO, "Target: (%d,%d)", a->targetCellX, a->targetCellY);
    } else if (a->state == ANIMAL_HUNTING) {
        if (a->targetAnimalIdx >= 0) DescAdd(out, DESC_BAD, "Prey: #%d", a->targetAnimalIdx);
        else DescAdd(out, DESC_BAD, "Prey: none");
    }

    if (AnimalDazed(idx)) DescAdd(out, DESC_WARN, "Dazed (hit by a stone)");
    else if (AnimalScared(idx)) DescAdd(out, DESC_WARN, "Scared: running");
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
    if (IsLadderCell(ct) && ONE_WAY(x, y, z) == ONEWAY_UP) DescAdd(out, DESC_INFO, "One-way: up only");
    if (IsLadderCell(ct) && ONE_WAY(x, y, z) == ONEWAY_DOWN) DescAdd(out, DESC_INFO, "One-way: down only");
    if (ct == CELL_DOOR) DescAdd(out, DESC_INFO, "Closed: keeps out smoke, water and cold");
    if (ct == CELL_DOOR_OPEN) DescAdd(out, DESC_WARN, "Open: smoke, water and cold get through");
    if (CellIsDoor(ct) && ONE_WAY(x, y, z) >= ONEWAY_N && ONE_WAY(x, y, z) <= ONEWAY_W) {
        static const char* const ways[] = { "north", "east", "south", "west" };
        DescAdd(out, DESC_INFO, "One-way: pass %s only", ways[ONE_WAY(x, y, z) - ONEWAY_N]);
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
// Water
// ---------------------------------------------------------------------------

void DescribeWater(int x, int y, int z, DescLines* out) {
    out->count = 0;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (!IsExplored(x, y, z)) return;
    const WaterCell* w = &WATER(x, y, z);
    if (w->level == 0 && !w->isSource && !w->isDrain) return;

    DescAdd(out, DESC_WATER, "Water (%d,%d,z%d)", x, y, z);
    DescAdd(out, DESC_NORMAL, "Level: %d/7", w->level);
    DescAdd(out, DESC_NORMAL, "Speed: %.0f%%", GetWaterSpeedMultiplier(x, y, z) * 100);
    if (w->isSource) DescAdd(out, DESC_ICE, "SOURCE");
    if (w->isDrain) DescAdd(out, DESC_MUTED, "DRAIN");
    if (w->hasPressure) {
        DescAdd(out, DESC_MILD, "Pressure (src z=%d)", w->pressureSourceZ);
        DescAdd(out, DESC_NORMAL, "Max rise: z=%d", w->pressureSourceZ - 1);
    }
    if (w->stable) DescAdd(out, DESC_MUTED, "[stable]");
}

// ---------------------------------------------------------------------------
// Blueprint
// ---------------------------------------------------------------------------

void DescribeBlueprint(int idx, DescLines* out) {
    out->count = 0;
    if (idx < 0 || idx >= MAX_BLUEPRINTS || !blueprints[idx].active) return;
    const Blueprint* bp = &blueprints[idx];
    static const char* stateNames[] = {"Awaiting materials", "Ready to build", "Building", "Clearing"};
    static const DescTone stateTones[] = {DESC_WARN, DESC_GOOD, DESC_INFO, DESC_WARN};

    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    DescAdd(out, DESC_TITLE, "%s (%d,%d,%d)", recipe ? recipe->name : "Construction", bp->x, bp->y, bp->z);
    if (bp->state < 4) DescAdd(out, stateTones[bp->state], "Status: %s", stateNames[bp->state]);
    else DescAdd(out, DESC_NORMAL, "Status: ?");

    // Delivered materials for the current stage
    if (recipe && bp->stage < recipe->stageCount) {
        const ConstructionStage* stage = &recipe->stages[bp->stage];
        for (int s = 0; s < stage->inputCount; s++) {
            const ConstructionInput* input = &stage->inputs[s];
            const char* name = input->anyBuildingMat ? "Building Mat" : ItemName(input->alternatives[0].itemType);
            DescAdd(out, DESC_STEAM, "  %s: %d/%d", name, bp->stageDeliveries[s].deliveredCount, input->count);
        }
    }
    if (recipe && recipe->stageCount > 1) {
        DescAdd(out, DESC_NORMAL, "Stage: %d/%d", bp->stage + 1, recipe->stageCount);
    }

    // What it waits for
    if (bp->state == BLUEPRINT_CLEARING) {
        DescAdd(out, DESC_WARN, "Clearing items from site");
    } else if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
        DescAdd(out, DESC_MUTED, "Waiting for hauler");
    } else if (bp->state == BLUEPRINT_READY_TO_BUILD) {
        DescAdd(out, DESC_MUTED, "Waiting for builder");
    } else if (bp->state == BLUEPRINT_BUILDING) {
        if (bp->assignedBuilder >= 0) DescAdd(out, DESC_GOOD, "Builder: Mover #%d", bp->assignedBuilder);
        DescAdd(out, DESC_NORMAL, "Progress: %d%%", (int)(bp->progress * 100));
    }
}

// ---------------------------------------------------------------------------
// Designation
// ---------------------------------------------------------------------------

static const struct { const char* name; const char* worker; } designationText[DESIGNATION_TYPE_COUNT] = {
    [DESIGNATION_NONE]           = {"None",           "Worker"},
    [DESIGNATION_MINE]           = {"Mining",         "Miner"},
    [DESIGNATION_CHANNEL]        = {"Channeling",     "Digger"},
    [DESIGNATION_DIG_RAMP]       = {"Dig Ramp",       "Digger"},
    [DESIGNATION_REMOVE_FLOOR]   = {"Remove Floor",   "Worker"},
    [DESIGNATION_REMOVE_RAMP]    = {"Remove Ramp",    "Worker"},
    [DESIGNATION_CHOP]           = {"Chop Tree",      "Woodcutter"},
    [DESIGNATION_CHOP_FELLED]    = {"Chop Felled",    "Woodcutter"},
    [DESIGNATION_GATHER_SAPLING] = {"Gather Sapling", "Gatherer"},
    [DESIGNATION_PLANT_SAPLING]  = {"Plant Sapling",  "Planter"},
    [DESIGNATION_GATHER_GRASS]   = {"Gather Grass",   "Gatherer"},
    [DESIGNATION_GATHER_REEDS]   = {"Gather Reeds",   "Gatherer"},
    [DESIGNATION_GATHER_TREE]    = {"Gather Tree",    "Gatherer"},
    [DESIGNATION_CLEAN]          = {"Clean",          "Cleaner"},
    [DESIGNATION_HARVEST_BERRY]  = {"Harvest Berries","Gatherer"},
    [DESIGNATION_KNAP]           = {"Knap Stone",     "Knapper"},
    [DESIGNATION_DIG_ROOTS]      = {"Dig Roots",      "Digger"},
    [DESIGNATION_EXPLORE]        = {"Explore",        "Explorer"},
    [DESIGNATION_FARM]           = {"Farm",           "Farmer"},
    [DESIGNATION_REMOVE_BELT]    = {"Remove Belt",    "Worker"},
    [DESIGNATION_BLAST]          = {"Blast",          "Blaster"},
    [DESIGNATION_CLOSE_DOOR]     = {"Close Door",     "Worker"},
    [DESIGNATION_OPEN_DOOR]      = {"Open Door",      "Worker"},
};

const char* DesignationDisplayName(int type) {
    if (type < 0 || type >= DESIGNATION_TYPE_COUNT || !designationText[type].name) return "Unknown";
    return designationText[type].name;
}

// Hard tool requirement for the job a designation creates ("Needs cutting:2 tool")
static void DescribeDesignationTool(const Designation* des, int x, int y, int z, DescLines* out) {
    if (!toolRequirementsEnabled) return;
    static const char* qualityNames[] = {
        [QUALITY_CUTTING] = "cutting", [QUALITY_HAMMERING] = "hammering", [QUALITY_DIGGING] = "digging",
        [QUALITY_SAWING] = "sawing", [QUALITY_FINE] = "fine",
    };
    int jobType = -1;
    switch (des->type) {
        case DESIGNATION_MINE:        jobType = JOBTYPE_MINE; break;
        case DESIGNATION_CHANNEL:     jobType = JOBTYPE_CHANNEL; break;
        case DESIGNATION_DIG_RAMP:    jobType = JOBTYPE_DIG_RAMP; break;
        case DESIGNATION_CHOP:        jobType = JOBTYPE_CHOP; break;
        case DESIGNATION_CHOP_FELLED: jobType = JOBTYPE_CHOP_FELLED; break;
        default: return;
    }
    // Channel digs the level below
    MaterialType mat = GetWallMaterial(x, y, (des->type == DESIGNATION_CHANNEL && z > 0) ? z - 1 : z);
    JobToolReq req = GetJobToolRequirement(jobType, mat);
    if (req.hasRequirement && !req.isSoft && req.minLevel > 0) {
        DescAdd(out, DESC_NORMAL, "Needs %s:%d tool", qualityNames[req.qualityType], req.minLevel);
    }
}

void DescribeDesignation(int x, int y, int z, DescLines* out) {
    out->count = 0;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (!IsExplored(x, y, z)) return;
    const Designation* des = GetDesignation(x, y, z);
    if (!des || des->type == DESIGNATION_NONE) return;
    const char* worker = (des->type < DESIGNATION_TYPE_COUNT && designationText[des->type].worker)
                         ? designationText[des->type].worker : "Worker";

    DescAdd(out, DESC_TITLE, "%s (%d,%d,%d)", DesignationDisplayName(des->type), x, y, z);
    DescAdd(out, DESC_NORMAL, "Target: %s", CellName(GRID(x, y, z)));
    if (des->assignedMover >= 0) {
        DescAdd(out, DESC_GOOD, "%s: Mover #%d", worker, des->assignedMover);
        DescAdd(out, DESC_NORMAL, "Progress: %d%%", (int)(des->progress * 100));
    } else if (des->unreachableCooldown > 0) {
        DescAdd(out, DESC_BAD, "Unreachable (%.1fs)", des->unreachableCooldown);
    } else {
        DescAdd(out, DESC_MUTED, "Waiting for %s", worker);
        DescribeDesignationTool(des, x, y, z, out);
    }
}

// ---------------------------------------------------------------------------
// Workshop
// ---------------------------------------------------------------------------

static const Workshop* ActiveWorkshop(int wsIdx) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS || !workshops[wsIdx].active) return NULL;
    return &workshops[wsIdx];
}

// Any unreserved item on the workshop's level that this input accepts
static bool InputAvailable(const Workshop* ws, const Recipe* recipe, int which, bool needUnreserved) {
    ItemType type = which == 1 ? recipe->inputType : which == 2 ? recipe->inputType2 : recipe->inputType3;
    if (which > 1 && type == ITEM_NONE) return true;   // not needed
    for (int i = 0; i < itemHighWaterMark; i++) {
        const Item* item = &items[i];
        if (!item->active || (int)item->z != ws->z) continue;
        if (needUnreserved && item->reservedBy != -1) continue;
        if (which == 1 ? RecipeInputMatches(recipe, item) : item->type == type) return true;
    }
    return false;
}

static const Recipe* BillRecipe(const Workshop* ws, const Bill* bill) {
    int recipeCount;
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    return (bill->recipeIdx >= 0 && bill->recipeIdx < recipeCount) ? &recipes[bill->recipeIdx] : NULL;
}

static const char* RecipeInputName(const Recipe* r, int which) {
    if (which == 1) return r->inputItemMatch == ITEM_MATCH_ANY_FUEL ? "Any Fuel" : ItemName(r->inputType);
    return ItemName(which == 2 ? r->inputType2 : r->inputType3);
}

void DescribeWorkshopStatus(int wsIdx, DescLines* out) {
    out->count = 0;
    const Workshop* ws = ActiveWorkshop(wsIdx);
    if (!ws) return;
    const char* typeName = (ws->type < WORKSHOP_TYPE_COUNT) ? workshopDefs[ws->type].displayName : "Unknown";
    DescAdd(out, DESC_TITLE, "%s Workshop #%d", typeName, wsIdx);
    DescAdd(out, DESC_NORMAL, "Position: (%d, %d, z%d)", ws->x, ws->y, ws->z);

    if (ws->markedForDeconstruct) {
        if (ws->assignedDeconstructor >= 0) DescAdd(out, DESC_BAD, "DECONSTRUCTING (Mover #%d)", ws->assignedDeconstructor);
        else DescAdd(out, DESC_BAD, "MARKED FOR DECONSTRUCTION");
    }
    if (ws->assignedCrafter >= 0) DescAdd(out, DESC_GOOD, "Crafter: Mover #%d", ws->assignedCrafter);
    else DescAdd(out, DESC_MUTED, "Crafter: None");

    // Problem states (working is silent); details are about the first bill
    const Recipe* first = ws->billCount > 0 ? BillRecipe(ws, &ws->bills[0]) : NULL;
    switch (ws->visualState) {
        case WORKSHOP_VISUAL_OUTPUT_FULL:
            DescAdd(out, DESC_SOFT_BAD, "Status: Output Blocked");
            if (first && first->outputType != ITEM_NONE) {
                DescAdd(out, DESC_SOFT_WARN, "  Output blocked: %s (x%d)", ItemName(first->outputType), first->outputCount);
                DescAdd(out, DESC_MILD, "  Hint: Build stockpile accepting %s", ItemName(first->outputType));
            }
            break;
        case WORKSHOP_VISUAL_INPUT_EMPTY:
            DescAdd(out, DESC_MILD, "Status: Waiting for Input");
            if (first) {
                if (!InputAvailable(ws, first, 1, true))
                    DescAdd(out, DESC_SOFT_WARN, "  Waiting for: %s (x%d)", RecipeInputName(first, 1), first->inputCount);
                if (!InputAvailable(ws, first, 2, true))
                    DescAdd(out, DESC_SOFT_WARN, "  Waiting for: %s (x%d)", RecipeInputName(first, 2), first->inputCount2);
                if (!InputAvailable(ws, first, 3, true))
                    DescAdd(out, DESC_SOFT_WARN, "  Waiting for: %s (x%d)", RecipeInputName(first, 3), first->inputCount3);
                if (first->fuelRequired > 0 && !WorkshopHasFuelForRecipe(&workshops[wsIdx], 100))
                    DescAdd(out, DESC_SOFT_WARN, "  Waiting for: Fuel (any burnable item)");
            }
            break;
        case WORKSHOP_VISUAL_NO_WORKER:
            DescAdd(out, DESC_MUTED, "Status: No Worker Assigned");
            break;
        case WORKSHOP_VISUAL_WORKING:
            break;
    }

    if (workshopDefs[ws->type].passive) {
        if (ws->passiveReady) DescAdd(out, DESC_WARN, "Status: Burning (%.0f%%)", ws->passiveProgress * 100.0f);
        else DescAdd(out, DESC_MUTED, "Status: Not ignited");
    }
}

void DescribeWorkshopBill(int wsIdx, int billIdx, bool marked, DescLines* out) {
    const Workshop* ws = ActiveWorkshop(wsIdx);
    if (!ws || billIdx < 0 || billIdx >= ws->billCount) return;
    static const char* modeNames[] = {"Do X times", "Do until X", "Do forever"};
    const Bill* bill = &ws->bills[billIdx];
    const Recipe* recipe = BillRecipe(ws, bill);

    char status[32] = "";
    if (bill->suspended && bill->suspendedNoStorage) snprintf(status, sizeof(status), " [NO STORAGE]");
    else if (bill->suspended) snprintf(status, sizeof(status), " [PAUSED]");
    else if (bill->mode == BILL_DO_X_TIMES) snprintf(status, sizeof(status), " (%d/%d)", bill->completedCount, bill->targetCount);
    else if (bill->mode == BILL_DO_UNTIL_X)
        snprintf(status, sizeof(status), " (%d/%d)", recipe ? CountItemsInStockpiles(recipe->outputType) : 0, bill->targetCount);

    DescTone tone = bill->suspended ? (bill->suspendedNoStorage ? DESC_WARN : DESC_BAD) : DESC_BILL;
    DescAdd(out, tone, "%c%d. %s (%s)%s", marked ? '>' : ' ', billIdx + 1, recipe ? recipe->name : "Unknown",
            bill->mode < 3 ? modeNames[bill->mode] : "?", status);

    // Why it can't run yet: missing inputs or fuel (only while nobody is crafting)
    if (recipe && !bill->suspended && ws->assignedCrafter < 0) {
        bool in1 = InputAvailable(ws, recipe, 1, false);
        bool in2 = InputAvailable(ws, recipe, 2, false);
        bool in3 = InputAvailable(ws, recipe, 3, false);
        bool fuel = recipe->fuelRequired <= 0 || WorkshopHasFuelForRecipe(&workshops[wsIdx], 100);
        if (!in1 || !in2 || !in3 || !fuel) {
            char needs[128] = "    Needs:";
            bool prev = false;
            if (!in1) { strncat(needs, " ", sizeof(needs) - strlen(needs) - 1);
                        strncat(needs, RecipeInputName(recipe, 1), sizeof(needs) - strlen(needs) - 1); prev = true; }
            if (!in2) { strncat(needs, prev ? " +" : " ", sizeof(needs) - strlen(needs) - 1);
                        strncat(needs, RecipeInputName(recipe, 2), sizeof(needs) - strlen(needs) - 1); prev = true; }
            if (!in3) { strncat(needs, prev ? " +" : " ", sizeof(needs) - strlen(needs) - 1);
                        strncat(needs, RecipeInputName(recipe, 3), sizeof(needs) - strlen(needs) - 1); prev = true; }
            if (!fuel) strncat(needs, prev ? " + fuel" : " fuel", sizeof(needs) - strlen(needs) - 1);
            DescAdd(out, DESC_SOFT_BAD, "%s", needs);
        }
    }
}

void DescribeWorkshopLinks(int wsIdx, DescLines* out) {
    const Workshop* ws = ActiveWorkshop(wsIdx);
    if (!ws) return;
    DescAdd(out, DESC_NORMAL, "Linked Input Stockpiles:");
    if (ws->linkedInputCount == 0) {
        DescAdd(out, DESC_MUTED, "  (none)");
        return;
    }
    for (int i = 0; i < ws->linkedInputCount; i++) {
        int sp = ws->linkedInputStockpiles[i];
        if (sp >= 0 && sp < stockpileCount && stockpiles[sp].active)
            DescAdd(out, DESC_GOOD, "  [%d] Stockpile #%d (Pri:%d)", i + 1, sp, stockpiles[sp].priority);
        else
            DescAdd(out, DESC_BAD, "  [%d] <invalid #%d>", i + 1, sp);
    }
}

void WorkshopRecipeSummary(int wsIdx, int recipeIdx, char* buf, size_t size) {
    buf[0] = '\0';
    const Workshop* ws = ActiveWorkshop(wsIdx);
    if (!ws) return;
    int recipeCount;
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    if (recipeIdx < 0 || recipeIdx >= recipeCount) return;
    const Recipe* r = &recipes[recipeIdx];
    char output[64];
    if (r->outputType == ITEM_NONE) snprintf(output, sizeof(output), "Products");
    else if (r->outputType2 != ITEM_NONE) snprintf(output, sizeof(output), "%s+%s", ItemName(r->outputType), ItemName(r->outputType2));
    else snprintf(output, sizeof(output), "%s", ItemName(r->outputType));
    if (r->inputType3 != ITEM_NONE)
        snprintf(buf, size, "%s (%s+%s+%s -> %s)", r->name, RecipeInputName(r, 1), RecipeInputName(r, 2), RecipeInputName(r, 3), output);
    else if (r->inputType2 != ITEM_NONE)
        snprintf(buf, size, "%s (%s+%s -> %s)", r->name, RecipeInputName(r, 1), RecipeInputName(r, 2), output);
    else
        snprintf(buf, size, "%s (%s -> %s)", r->name, RecipeInputName(r, 1), output);
}

// ---------------------------------------------------------------------------
// Stockpile
// ---------------------------------------------------------------------------

void DescFillMeter(char* out, size_t outSize, float ratio, int width) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    if (width > 28) width = 28;
    int filled = (int)(ratio * width + 0.5f);
    if (filled > width) filled = width;
    char bar[32];
    int idx = 0;
    bar[idx++] = '[';
    for (int i = 0; i < width; i++) bar[idx++] = (i < filled) ? '|' : ' ';
    bar[idx++] = ']';
    bar[idx] = '\0';
    snprintf(out, outSize, "%s %d%%", bar, (int)(ratio * 100.0f + 0.5f));
}

void DescribeStockpile(int spIdx, DescLines* out) {
    out->count = 0;
    if (spIdx < 0 || spIdx >= MAX_STOCKPILES || !stockpiles[spIdx].active) return;
    const Stockpile* sp = &stockpiles[spIdx];

    int totalItems = 0, activeCells = 0;
    for (int i = 0; i < sp->width * sp->height; i++) {
        if (!sp->cells[i]) continue;
        activeCells++;
        totalItems += sp->slotCounts[i];
    }

    DescAdd(out, DESC_TITLE, "Stockpile #%d", spIdx);
    if (stockpileForSale[spIdx]) {   // a shop (money.h)
        char mb[24];
        DescAdd(out, DESC_GOOD, "For sale, till: %s", MoneyFormat(stockpileTill[spIdx], mb, sizeof mb));
    }
    DescAdd(out, DESC_NORMAL, "Priority: %d", sp->priority);
    DescAdd(out, DESC_NORMAL, "Stack size: %d", sp->maxStackSize);
    if (sp->maxContainers > 0)
        DescAdd(out, DESC_NORMAL, "Containers: %d/%d", CountInstalledContainers(spIdx), sp->maxContainers);
    DescAdd(out, IsStockpileOverfull(spIdx) ? DESC_BAD : DESC_NORMAL, "Storage: %d/%d (%d cells)",
            totalItems, activeCells * sp->maxStackSize, activeCells);
    char meter[48];
    DescFillMeter(meter, sizeof(meter), GetStockpileFillRatio(spIdx), 10);
    DescAdd(out, DESC_NORMAL, "Fill: %s", meter);
}

// ---------------------------------------------------------------------------
// Town lot
// ---------------------------------------------------------------------------

// One person in a lot's list: name, hunger, and for a worker whether they are at work now
static void DescribeLotPerson(int m, int lot, bool worker, DescLines* out) {
    DescTone tone;
    const char* hunger = HungerLabel(movers[m].hunger, &tone);
    const TownLot* L = &townLots[lot];
    int mx = (int)(movers[m].x / CELL_SIZE), my = (int)(movers[m].y / CELL_SIZE);
    bool here = (int)movers[m].z == L->z && mx >= L->x1 && mx <= L->x2 && my >= L->y1 && my <= L->y2;
    const char* where = here ? (worker ? "at work" : "at home") : "out";
    DescAdd(out, tone == DESC_GOOD || tone == DESC_GREAT ? DESC_NORMAL : tone, "  %s: %s, %s", MoverDisplayName(m), hunger, where);
}

void DescribeLot(int lot, DescLines* out) {
    out->count = 0;
    if (lot < 0 || lot >= townLotHighWater || !townLots[lot].active) return;
    const TownLot* L = &townLots[lot];
    DescAdd(out, DESC_TITLE, "%s #%d", TownLotKindName(L->kind), lot);
    DescAdd(out, DESC_MUTED, "%dx%d lot", L->x2 - L->x1 + 1, L->y2 - L->y1 + 1);
    switch (L->kind) {
        case LOT_HOME: {
            int people = TownResidents(lot);
            DescAdd(out, people < L->beds ? DESC_INFO : DESC_NORMAL, "Beds: %d (%d free)", L->beds, L->beds - people);
            for (int m = 0; m < moverCount; m++)
                if (movers[m].active && TownHomeOf(m) == lot) DescribeLotPerson(m, lot, false, out);
            int bread = TownLotStock(lot, ITEM_BREAD);
            DescAdd(out, bread == 0 ? DESC_WARN : DESC_NORMAL, "Pantry: %d bread", bread);
            break;
        }
        case LOT_WORK:
        case LOT_MILL: {
            bool open = TownLotOpen(lot);
            DescAdd(out, open ? DESC_GOOD : DESC_MUTED, "%s, %d-%d h", open ? "Open" : "Closed", townOpenHour, townCloseHour);
            int workers = TownWorkers(lot);
            DescAdd(out, workers < L->beds ? DESC_WARN : DESC_NORMAL, "Jobs: %d of %d filled", workers, L->beds);
            for (int m = 0; m < moverCount; m++)
                if (movers[m].active && TownJobOf(m) == lot) DescribeLotPerson(m, lot, true, out);
            int made = 0, units = 0;   // batches, and what they made (recipe output count)
            for (int i = 0; i < L->wsCount; i++) {
                const Workshop* w = &workshops[L->ws[i]];
                if (!w->active || w->billCount == 0) continue;
                int rc;
                const Recipe* rs = GetRecipesForWorkshop(w->type, &rc);
                int r = w->bills[0].recipeIdx;
                made += w->bills[0].completedCount;
                if (r >= 0 && r < rc) units += w->bills[0].completedCount * rs[r].outputCount;
            }
            if (L->kind == LOT_WORK) {
                DescAdd(out, DESC_NORMAL, "Flour in store: %d", TownLotStock(lot, ITEM_FLOUR));
                DescAdd(out, DESC_NORMAL, "Bread on the shelf: %d", TownLotStock(lot, ITEM_BREAD));
                DescAdd(out, DESC_INFO, "Baked so far: %d batches (%d loaves)", made, units);
            } else {
                DescAdd(out, DESC_NORMAL, "Wheat in the granary: %d", TownLotStock(lot, ITEM_WHEAT));
                DescAdd(out, DESC_INFO, "Ground so far: %d batches (%d flour)", made, units);
            }
            break;
        }
        case LOT_FARM: {
            bool open = TownLotOpen(lot);
            DescAdd(out, open ? DESC_GOOD : DESC_MUTED, "%s, %d-%d h", open ? "Open" : "Closed", townOpenHour, townCloseHour);
            int hands = TownWorkers(lot);
            DescAdd(out, hands < L->beds ? DESC_WARN : DESC_NORMAL, "Farmhands: %d of %d", hands, L->beds);
            for (int m = 0; m < moverCount; m++)
                if (movers[m].active && TownJobOf(m) == lot) DescribeLotPerson(m, lot, true, out);
            TownFarmStats st = TownFarmStatsOf(lot);
            DescAdd(out, DESC_NORMAL, "Field: %d cells", st.fields);
            DescAdd(out, DESC_GOOD, "  growing %d, ripe %d", st.growing, st.ripe);
            DescAdd(out, st.waiting > st.fields / 2 ? DESC_WARN : DESC_MUTED, "  waiting to be planted %d", st.waiting);
            if (st.tilled < st.fields) DescAdd(out, DESC_MUTED, "  not tilled yet %d", st.fields - st.tilled);
            if (st.tilled > 0) {
                int fert = st.fertilitySum / st.tilled;
                DescAdd(out, fert < FERTILITY_LOW ? DESC_WARN : DESC_NORMAL, "Fertility: %d/255", fert);
            }
            if (st.heavyWeeds > 0) DescAdd(out, DESC_WARN, "Heavy weeds: %d cells", st.heavyWeeds);
            if (CropSeasonModifier(CROP_WHEAT, (int)GetCurrentSeason()) <= 0.0f) DescAdd(out, DESC_COLD, "Winter: nothing grows");
            else if (!CropWorthPlantingNow(CROP_WHEAT)) DescAdd(out, DESC_INFO, "Too late to sow before winter");
            DescAdd(out, DESC_NORMAL, "Seed: %d, barn: %d wheat", TownLotStock(lot, ITEM_WHEAT_SEEDS), TownLotStock(lot, ITEM_WHEAT));
            DescAdd(out, DESC_INFO, "Irrigated: %s", townIrrigation ? "yes" : "no (rain only)");
            break;
        }
        case LOT_GRANARY: {
            int wheat = TownLotStock(lot, ITEM_WHEAT), flour = TownLotStock(lot, ITEM_FLOUR), room = TownLotStoreRoom(lot);
            DescAdd(out, DESC_NORMAL, "Holds %d wheat, %d flour", wheat, flour);
            char meter[48];
            DescFillMeter(meter, sizeof(meter), room > 0 ? (float)(wheat + flour) / room : 0.0f, 10);
            DescAdd(out, DESC_NORMAL, "Full: %s (%d of %d)", meter, wheat + flour, room);
            break;
        }
        default: break;
    }
}

void DescribeTownOverview(char* buf, int len) {
    int people = 0;
    for (int i = 0; i < moverCount; i++) people += movers[i].active;
    snprintf(buf, len, "%d people   %d homeless   %d without a job   bread %d   flour %d   wheat %d (stores %d)   imported %d wheat, %d flour",
             people, TownHomeless(), TownUnemployed(), TownTotalStock(ITEM_BREAD), TownTotalStock(ITEM_FLOUR),
             TownTotalStock(ITEM_WHEAT), TownStoreCapacity(), townImported[ITEM_WHEAT], townImported[ITEM_FLOUR]);
}

// ---------------------------------------------------------------------------
// Train station
// ---------------------------------------------------------------------------

void DescribeStation(int idx, DescLines* out) {
    out->count = 0;
    if (idx < 0 || idx >= stationCount || !stations[idx].active) return;
    const TrainStation* st = &stations[idx];
    DescAdd(out, DESC_INFO, "Station #%d", idx);
    if (TrackLineCount() > 1) DescAdd(out, DESC_NORMAL, "%s line", TrackLineName(StationLine(idx)));
    DescAdd(out, DESC_NORMAL, "Track: (%d, %d, %d)", st->trackX, st->trackY, st->z);
    DescAdd(out, DESC_NORMAL, "Platform: %d cell%s", st->platformCellCount, st->platformCellCount != 1 ? "s" : "");
    DescAdd(out, st->waitingCount > 0 ? DESC_MILD : DESC_NORMAL, "Waiting: %d", st->waitingCount);
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

    if (ItemIsMoney(item->type)) {   // stackCount is coins (money.h)
        char mb[24];
        DescAdd(out, DESC_GOOD, "#%d: %s %s (%s)", idx, name, MoneyFormat(item->stackCount, mb, sizeof mb), state);
    } else if (item->stackCount > 1)
        DescAdd(out, tone, "#%d: %s x%d (%s%s%s%s)", idx, name, item->stackCount, state, freshness, hydration, temp);
    else
        DescAdd(out, tone, "#%d: %s (%s%s%s%s)", idx, name, state, freshness, hydration, temp);
}

void DescribeItemsAt(const int* itemIndices, int itemCount, int cellX, int cellY, DescLines* out) {
    out->count = 0;
    if (itemCount <= 0) return;
    int totalUnits = 0;
    for (int i = 0; i < itemCount; i++) totalUnits += ItemIsMoney(items[itemIndices[i]].type) ? 1 : items[itemIndices[i]].stackCount;   // coins: stackCount is coins
    DescAdd(out, DESC_MILD, "Cell (%d,%d): %d unit%s", cellX, cellY, totalUnits, totalUnits == 1 ? "" : "s");
    // At most 16 item lines, as before
    for (int i = 0; i < itemCount && i < 16; i++) DescribeItemLine(itemIndices[i], out);
}
