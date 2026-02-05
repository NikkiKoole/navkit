#ifndef SIM_PRESENCE_H
#define SIM_PRESENCE_H

#include <stdbool.h>

// =============================================================================
// SIMULATION PRESENCE TRACKING
// Active cell counts for early exit optimization in Update loops.
// When count is 0, the simulation can skip its entire update.
// =============================================================================

// Active cell counts for early exit optimization
extern int waterActiveCells;
extern int steamActiveCells;
extern int fireActiveCells;
extern int smokeActiveCells;
extern int tempSourceCount;
extern int tempUnstableCells;  // Cells that are unstable OR differ from ambient

void InitSimPresence(void);
void RebuildSimPresenceCounts(void);  // Rebuild counters from grids (call after load)

#endif
