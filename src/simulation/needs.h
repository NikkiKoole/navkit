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

#endif
