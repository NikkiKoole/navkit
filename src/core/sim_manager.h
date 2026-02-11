#ifndef SIM_MANAGER_H
#define SIM_MANAGER_H

#include <stdbool.h>

// =============================================================================
// SIMULATION ACTIVITY TRACKING
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
extern int treeActiveCells;    // Saplings + growing trunks
extern int wearActiveCells;    // Dirt tiles with wear > 0
extern int dirtActiveCells;    // Floor tiles with tracked-in dirt > 0

void InitSimActivity(void);
void RebuildSimActivityCounts(void);  // Rebuild counters from grids (call after load)
bool ValidateSimActivityCounts(void); // Validate counters, auto-correct if drift detected (returns true if valid)

#endif // SIM_MANAGER_H
