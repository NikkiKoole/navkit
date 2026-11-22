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

#endif
