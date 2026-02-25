#ifndef STATE_AUDIT_H
#define STATE_AUDIT_H

#include <stdbool.h>

// Run all audit checks. Returns total violation count.
// If verbose, logs each violation. If not, just counts.
int RunStateAudit(bool verbose);

// Individual sub-checks (each returns violation count)
int AuditItemStockpileConsistency(bool verbose);
int AuditItemReservations(bool verbose);
int AuditMoverJobConsistency(bool verbose);
int AuditBlueprintReservations(bool verbose);
int AuditStockpileSlotReservations(bool verbose);
int AuditStockpileFreeSlotCounts(bool verbose);
int AuditFarmConsistency(bool verbose);

// Set output mode: false = TraceLog (runtime), true = printf (inspect CLI)
void SetAuditOutputStdout(bool useStdout);

#endif
