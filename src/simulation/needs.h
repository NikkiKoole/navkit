// needs.h - Mover needs system (eating, future: sleep, hygiene)
// Manages the freetime state machine for autonomous mover behaviors.

#ifndef NEEDS_H
#define NEEDS_H

// Process all movers' freetime needs (food search, eating)
// Call after NeedsTick(), before AssignJobs().
void ProcessFreetimeNeeds(void);

// Energy lost per game hour by a mover that is awake: work vs idle, x cold, x carrying.
// Used by NeedsTick and by the mover description, so the tooltip matches the sim.
float MoverEnergyDrainPerGH(int moverIdx);

// Hunger / thirst lost per game hour: slower while resting (balance.sleepMetabolismMult).
float MoverHungerDrainPerGH(int moverIdx);
float MoverThirstDrainPerGH(int moverIdx);

// Soft day rhythm (setting sleepRhythm). Nothing makes movers tired at night; instead:
//  - lying in: a mover in bed doesn't get up while it is dark, unless a need is critical
//  - early night: a mover with nothing to do, in the dark, below balance.idleBedEnergy goes to bed
// With sleepMetabolismMult a meal and a drink last the night, so movers sleep until dawn and settle into a
// day/night rhythm even with endless work. Measured: docs/todo/schedule-system.md.
extern bool sleepRhythmEnabled;
bool IsSkyDark(void);
bool MoverLyingIn(int moverIdx);   // resting, rested, staying in bed until it gets light

// Walking cost (10 per cell, same rules as the pathfinder) from a cell to the nearest
// water a mover can drink from (in or next to a cell it can stand on, that level or one
// below); -1 if none within maxCost. A thirsty mover searches up to NEEDS_WATER_SEARCH_COST.
// *outWaterZ (may be NULL) = the level the water found is at.
#define NEEDS_WATER_SEARCH_COST (64 * 10)
int WalkCostToWater(int x, int y, int z, int maxCost, int* outWaterZ);

// Trash (docs/vision/city-1999.md "Trash, bins and deposit bottles"): what a meal or drink
// leaves behind (ItemLeavesBehind) goes in the mover's hand (Mover.heldTrash), then into the
// nearest trash bin with room within trashBinWalkCells (Tidy: twice as far, Slob: only a bin
// right there), else onto the ground as litter. It becomes a real item only then.
extern int trashBinWalkCells;
extern int latrineDirtPerUse;   // floor dirt a pit latrine gets per use (setting latrineDirt)
int FindTrashBinFor(int moverIdx, ItemType trash);   // bin item index with room, or -1

#endif
