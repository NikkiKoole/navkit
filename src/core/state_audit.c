#include "state_audit.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/stockpiles.h"
#include "../entities/jobs.h"
#include "../entities/mover.h"
#include "../world/designations.h"
#include "../world/construction.h"
#include "../world/grid.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static bool auditUseStdout = false;

void SetAuditOutputStdout(bool useStdout) {
    auditUseStdout = useStdout;
}

// Audit log macro — switches between printf (inspect) and TraceLog (runtime)
static void AuditLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(buf, sizeof(buf), fmt, args);
#pragma clang diagnostic pop
    va_end(args);
    if (auditUseStdout) {
        printf("[AUDIT] %s\n", buf);
    } else {
        TraceLog(LOG_WARNING, "[AUDIT] %s", buf);
    }
}
#define AUDIT_LOG AuditLog

// ---------------------------------------------------------------------------
// 1. Item ↔ Stockpile consistency
// ---------------------------------------------------------------------------
int AuditItemStockpileConsistency(bool verbose) {
    int violations = 0;

    // Check: every ITEM_IN_STOCKPILE item should be in an active cell of an active stockpile
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_IN_STOCKPILE) continue;

        int ix = (int)(items[i].x / CELL_SIZE);
        int iy = (int)(items[i].y / CELL_SIZE);
        int iz = (int)items[i].z;

        bool found = false;
        for (int s = 0; s < MAX_STOCKPILES; s++) {
            Stockpile* sp = &stockpiles[s];
            if (!sp->active) continue;
            if (sp->z != iz) continue;

            int lx = ix - sp->x;
            int ly = iy - sp->y;
            if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) continue;

            int idx = ly * sp->width + lx;
            if (sp->cells[idx]) {
                found = true;
                break;
            } else {
                // Item is at a stockpile cell, but cell is inactive
                violations++;
                if (verbose) {
                    AUDIT_LOG("Item %d (%s) at (%d,%d,z%d) is IN_STOCKPILE but stockpile %d cell (%d,%d) is inactive",
                              i, ItemName(items[i].type), ix, iy, iz, s, lx, ly);
                }
                found = true;  // found the stockpile, but it's broken
                break;
            }
        }

        if (!found) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Item %d (%s) at (%d,%d,z%d) is IN_STOCKPILE but no active stockpile covers that cell",
                          i, ItemName(items[i].type), ix, iy, iz);
            }
        }
    }

    // Check: every stockpile slot with slotCounts > 0 should have slots[] → active item
    for (int s = 0; s < MAX_STOCKPILES; s++) {
        Stockpile* sp = &stockpiles[s];
        if (!sp->active) continue;

        int totalSlots = sp->width * sp->height;
        for (int idx = 0; idx < totalSlots; idx++) {
            if (!sp->cells[idx]) continue;
            if (sp->slotCounts[idx] <= 0) continue;

            int itemIdx = sp->slots[idx];
            if (itemIdx < 0 || itemIdx >= MAX_ITEMS) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Stockpile %d slot %d has slotCounts=%d but slots[]=%d (invalid index)",
                              s, idx, sp->slotCounts[idx], itemIdx);
                }
            } else if (!items[itemIdx].active) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Stockpile %d slot %d has slotCounts=%d but item %d is inactive",
                              s, idx, sp->slotCounts[idx], itemIdx);
                }
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// 2. Item reservation consistency
// ---------------------------------------------------------------------------
int AuditItemReservations(bool verbose) {
    int violations = 0;

    // Check: every item with reservedBy != -1 should have a matching active job
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].reservedBy == -1) continue;

        int reservedBy = items[i].reservedBy;

        // reservedBy is a mover index — find if any active job references this item
        bool foundJob = false;
        for (int j = 0; j < activeJobCount; j++) {
            int jobId = activeJobList[j];
            Job* job = &jobs[jobId];
            if (!job->active) continue;
            if (job->targetItem == i || job->carryingItem == i ||
                job->targetItem2 == i || job->fuelItem == i ||
                job->toolItem == i) {
                foundJob = true;
                break;
            }
        }

        // Also check if this item is a mover's equipped tool (no job needed)
        bool isEquippedTool = false;
        if (reservedBy >= 0 && reservedBy < MAX_MOVERS && movers[reservedBy].active &&
            movers[reservedBy].equippedTool == i) {
            isEquippedTool = true;
        }

        if (!foundJob && !isEquippedTool) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Item %d (%s) at (%.0f,%.0f,z%.0f) reservedBy=%d but no active job references it and not equipped",
                          i, ItemName(items[i].type), items[i].x, items[i].y, items[i].z, reservedBy);
            }
        }
    }

    // Check: every active job's item references should be valid
    for (int j = 0; j < activeJobCount; j++) {
        int jobId = activeJobList[j];
        Job* job = &jobs[jobId];
        if (!job->active) continue;

        // targetItem
        if (job->targetItem >= 0 && job->targetItem < MAX_ITEMS) {
            if (!items[job->targetItem].active) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Job %d (%s) targetItem=%d is inactive",
                              jobId, JobTypeName(job->type), job->targetItem);
                }
            }
        }

        // carryingItem
        if (job->carryingItem >= 0 && job->carryingItem < MAX_ITEMS) {
            if (!items[job->carryingItem].active) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Job %d (%s) carryingItem=%d is inactive",
                              jobId, JobTypeName(job->type), job->carryingItem);
                }
            }
        }

        // targetItem2
        if (job->targetItem2 >= 0 && job->targetItem2 < MAX_ITEMS) {
            if (!items[job->targetItem2].active) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Job %d (%s) targetItem2=%d is inactive",
                              jobId, JobTypeName(job->type), job->targetItem2);
                }
            }
        }

        // fuelItem
        if (job->fuelItem >= 0 && job->fuelItem < MAX_ITEMS) {
            if (!items[job->fuelItem].active) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Job %d (%s) fuelItem=%d is inactive",
                              jobId, JobTypeName(job->type), job->fuelItem);
                }
            }
        }

        // toolItem
        if (job->toolItem >= 0 && job->toolItem < MAX_ITEMS) {
            if (!items[job->toolItem].active) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Job %d (%s) toolItem=%d is inactive",
                              jobId, JobTypeName(job->type), job->toolItem);
                }
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// 3. Mover ↔ Job consistency
// ---------------------------------------------------------------------------
int AuditMoverJobConsistency(bool verbose) {
    int violations = 0;

    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;

        int jobId = movers[i].currentJobId;
        if (jobId < 0) continue;

        if (jobId >= MAX_JOBS) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d has currentJobId=%d (out of range)", i, jobId);
            }
            continue;
        }

        if (!jobs[jobId].active) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d has currentJobId=%d but that job is inactive", i, jobId);
            }
            continue;
        }

        if (jobs[jobId].assignedMover != i) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d has currentJobId=%d but job.assignedMover=%d (mismatch)",
                          i, jobId, jobs[jobId].assignedMover);
            }
        }
    }

    // Reverse check: every assigned job should have its mover pointing back
    for (int j = 0; j < activeJobCount; j++) {
        int jobId = activeJobList[j];
        Job* job = &jobs[jobId];
        if (!job->active) continue;
        if (job->assignedMover < 0) continue;

        int mi = job->assignedMover;
        if (mi >= MAX_MOVERS || !movers[mi].active) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Job %d (%s) assignedMover=%d but mover is %s",
                          jobId, JobTypeName(job->type), mi,
                          (mi >= MAX_MOVERS) ? "out of range" : "inactive");
            }
        } else if (movers[mi].currentJobId != jobId) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Job %d (%s) assignedMover=%d but mover.currentJobId=%d (mismatch)",
                          jobId, JobTypeName(job->type), mi, movers[mi].currentJobId);
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// 4. Blueprint reservation consistency
// ---------------------------------------------------------------------------
int AuditBlueprintReservations(bool verbose) {
    int violations = 0;

    for (int b = 0; b < MAX_BLUEPRINTS; b++) {
        if (!blueprints[b].active) continue;
        Blueprint* bp = &blueprints[b];

        for (int s = 0; s < MAX_INPUTS_PER_STAGE; s++) {
            StageDelivery* sd = &bp->stageDeliveries[s];
            if (sd->reservedCount <= 0) continue;

            // Count active haul-to-blueprint jobs targeting this blueprint+stage
            int jobCount = 0;
            for (int j = 0; j < activeJobCount; j++) {
                int jobId = activeJobList[j];
                Job* job = &jobs[jobId];
                if (!job->active) continue;
                if (job->type != JOBTYPE_HAUL_TO_BLUEPRINT) continue;
                if (job->targetBlueprint != b) continue;
                jobCount++;
            }

            if (sd->reservedCount > jobCount) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Blueprint %d at (%d,%d,z%d) stage slot %d: reservedCount=%d but only %d active haul-to-bp jobs",
                              b, bp->x, bp->y, bp->z, s, sd->reservedCount, jobCount);
                }
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// 5. Stockpile slot reservation consistency
// ---------------------------------------------------------------------------
int AuditStockpileSlotReservations(bool verbose) {
    int violations = 0;

    for (int s = 0; s < MAX_STOCKPILES; s++) {
        Stockpile* sp = &stockpiles[s];
        if (!sp->active) continue;

        int totalSlots = sp->width * sp->height;
        for (int idx = 0; idx < totalSlots; idx++) {
            if (!sp->cells[idx]) continue;
            if (sp->reservedBy[idx] <= 0) continue;

            // Count active haul jobs targeting this stockpile+slot
            int lx = idx % sp->width;
            int ly = idx / sp->width;
            int worldX = sp->x + lx;
            int worldY = sp->y + ly;

            int jobCount = 0;
            for (int j = 0; j < activeJobCount; j++) {
                int jobId = activeJobList[j];
                Job* job = &jobs[jobId];
                if (!job->active) continue;
                if (job->type != JOBTYPE_HAUL) continue;
                if (job->targetStockpile != s) continue;
                if (job->targetSlotX != worldX || job->targetSlotY != worldY) continue;
                jobCount++;
            }

            if (sp->reservedBy[idx] != jobCount) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Stockpile %d slot (%d,%d): reservedBy=%d but %d active haul jobs target it",
                              s, worldX, worldY, sp->reservedBy[idx], jobCount);
                }
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// 6. Stockpile freeSlotCount consistency
// ---------------------------------------------------------------------------
int AuditStockpileFreeSlotCounts(bool verbose) {
    int violations = 0;

    for (int s = 0; s < MAX_STOCKPILES; s++) {
        Stockpile* sp = &stockpiles[s];
        if (!sp->active) continue;

        int computed = 0;
        int totalSlots = sp->width * sp->height;
        for (int idx = 0; idx < totalSlots; idx++) {
            if (!sp->cells[idx]) continue;
            if (sp->groundItemIdx[idx] >= 0) continue;

            int lx = idx % sp->width;
            int ly = idx / sp->width;
            int worldX = sp->x + lx;
            int worldY = sp->y + ly;
            if (!IsCellWalkableAt(sp->z, worldY, worldX)) continue;

            if (sp->slotIsContainer[idx]) {
                int containerIdx = sp->slots[idx];
                if (containerIdx >= 0 && containerIdx < MAX_ITEMS && items[containerIdx].active) {
                    const ContainerDef* def = GetContainerDef(items[containerIdx].type);
                    if (def && items[containerIdx].contentCount + sp->reservedBy[idx] < def->maxContents) {
                        computed++;
                    }
                }
            } else if (sp->slotCounts[idx] + sp->reservedBy[idx] < sp->maxStackSize) {
                computed++;
            }
        }

        if (computed != sp->freeSlotCount) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Stockpile %d at (%d,%d,z%d): freeSlotCount=%d but computed=%d",
                          s, sp->x, sp->y, sp->z, sp->freeSlotCount, computed);
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// 7. Equipped tool consistency
// ---------------------------------------------------------------------------
static int AuditEquippedTools(bool verbose) {
    int violations = 0;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];

        if (!m->active) {
            if (m->equippedTool >= 0) {
                violations++;
                if (verbose) {
                    AUDIT_LOG("Inactive mover %d has equippedTool=%d (should be -1)", i, m->equippedTool);
                }
            }
            continue;
        }

        int toolIdx = m->equippedTool;
        if (toolIdx < 0) continue;

        if (toolIdx >= MAX_ITEMS) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d equippedTool=%d (out of range)", i, toolIdx);
            }
            continue;
        }

        Item* tool = &items[toolIdx];
        if (!tool->active) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d equippedTool=%d but item is inactive", i, toolIdx);
            }
            continue;
        }

        if (tool->state != ITEM_CARRIED) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d equippedTool=%d but item state is %d (expected ITEM_CARRIED=%d)",
                          i, toolIdx, tool->state, ITEM_CARRIED);
            }
        }

        if (tool->reservedBy != i) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d equippedTool=%d but item.reservedBy=%d (expected %d)",
                          i, toolIdx, tool->reservedBy, i);
            }
        }

        if (!(itemDefs[tool->type].flags & IF_TOOL)) {
            violations++;
            if (verbose) {
                AUDIT_LOG("Mover %d equippedTool=%d (%s) but item lacks IF_TOOL flag",
                          i, toolIdx, ItemName(tool->type));
            }
        }
    }

    return violations;
}

// ---------------------------------------------------------------------------
// Run all audits
// ---------------------------------------------------------------------------
int RunStateAudit(bool verbose) {
    int total = 0;

    total += AuditItemStockpileConsistency(verbose);
    total += AuditItemReservations(verbose);
    total += AuditMoverJobConsistency(verbose);
    total += AuditBlueprintReservations(verbose);
    total += AuditStockpileSlotReservations(verbose);
    total += AuditStockpileFreeSlotCounts(verbose);
    total += AuditEquippedTools(verbose);

    if (verbose) {
        if (auditUseStdout) {
            printf("[AUDIT] Total violations: %d\n", total);
        } else {
            TraceLog(total > 0 ? LOG_WARNING : LOG_INFO, "[AUDIT] Total violations: %d", total);
        }
    }

    return total;
}
