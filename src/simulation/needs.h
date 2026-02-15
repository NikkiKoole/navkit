// needs.h - Mover needs system (eating, future: sleep, hygiene)
// Manages the freetime state machine for autonomous mover behaviors.

#ifndef NEEDS_H
#define NEEDS_H

// Process all movers' freetime needs (food search, eating)
// Call after NeedsTick(), before AssignJobs().
void ProcessFreetimeNeeds(void);

#endif
