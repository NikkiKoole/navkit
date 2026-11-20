// service.c - service points: who waits for what, in arrival order (see service.h)

#include "service.h"
#include "mover.h"
#include <string.h>

ServicePoint servicePoints[MAX_SERVICE_POINTS];
int servicePointCount = 0;
static int moverService[MAX_MOVERS];   // point each mover waits at, -1 none
static bool serviceInit = false;

static void EnsureInit(void) {
    if (serviceInit) return;
    for (int i = 0; i < MAX_MOVERS; i++) moverService[i] = -1;
    serviceInit = true;
}

void ClearServicePoints(void) {
    memset(servicePoints, 0, sizeof(servicePoints));
    servicePointCount = 0;
    for (int i = 0; i < MAX_MOVERS; i++) moverService[i] = -1;
    serviceInit = true;
}

int ServiceFind(ServiceKind kind, int x, int y, int z) {
    for (int i = 0; i < servicePointCount; i++) {
        const ServicePoint* p = &servicePoints[i];
        if (p->active && p->kind == kind && p->x == x && p->y == y && p->z == z) return i;
    }
    return -1;
}

int ServiceGet(ServiceKind kind, int x, int y, int z, int capacity) {
    EnsureInit();
    int sp = ServiceFind(kind, x, y, z);
    if (sp >= 0) return sp;
    for (int i = 0; i < MAX_SERVICE_POINTS; i++) {
        if (servicePoints[i].active) continue;
        servicePoints[i] = (ServicePoint){.active = true, .kind = kind, .x = x, .y = y, .z = z, .capacity = capacity};
        if (i >= servicePointCount) servicePointCount = i + 1;
        return i;
    }
    return -1;
}

int ServiceOf(int mover) {
    EnsureInit();
    return (mover >= 0 && mover < MAX_MOVERS) ? moverService[mover] : -1;
}

const ServiceEntry* ServiceEntryOf(int sp, int mover) {
    if (sp < 0 || sp >= servicePointCount) return NULL;
    const ServicePoint* p = &servicePoints[sp];
    for (int k = 0; k < p->lineCount; k++)
        if (p->line[k].mover == mover) return &p->line[k];
    return NULL;
}

bool ServiceJoin(int sp, int mover, int end, double now) {
    EnsureInit();
    if (sp < 0 || sp >= servicePointCount || mover < 0 || mover >= MAX_MOVERS) return false;
    if (moverService[mover] == sp) return true;
    if (moverService[mover] >= 0) ServiceLeave(moverService[mover], mover);
    ServicePoint* p = &servicePoints[sp];
    if (p->lineCount >= MAX_SERVICE_LINE) return false;
    p->line[p->lineCount++] = (ServiceEntry){mover, end, now};
    moverService[mover] = sp;
    return true;
}

void ServiceLeave(int sp, int mover) {
    EnsureInit();
    if (sp < 0 || sp >= servicePointCount) return;
    ServicePoint* p = &servicePoints[sp];
    for (int k = 0; k < p->lineCount; k++) {
        if (p->line[k].mover != mover) continue;
        memmove(&p->line[k], &p->line[k + 1], (size_t)(p->lineCount - k - 1) * sizeof(ServiceEntry));
        p->lineCount--;
        break;
    }
    if (mover >= 0 && mover < MAX_MOVERS && moverService[mover] == sp) moverService[mover] = -1;
}

void ServiceLeaveAll(int mover) {
    int sp = ServiceOf(mover);
    if (sp >= 0) ServiceLeave(sp, mover);
}

int ServiceAhead(int sp, int mover) {
    if (sp < 0 || sp >= servicePointCount) return -1;
    const ServicePoint* p = &servicePoints[sp];
    int end = -1, ahead = -1;
    for (int k = 0; k < p->lineCount; k++)
        if (p->line[k].mover == mover) { end = p->line[k].end; break; }
    for (int k = 0; k < p->lineCount; k++) {
        if (p->line[k].mover == mover) return ahead;
        if (p->line[k].end == end) ahead = p->line[k].mover;
    }
    return -1;
}

bool ServiceIsNext(int sp, int mover) {
    if (sp < 0 || sp >= servicePointCount) return false;
    const ServicePoint* p = &servicePoints[sp];
    // The line is in arrival order: the first entry overall is the earliest
    // front of any end
    return p->lineCount > 0 && p->line[0].mover == mover;
}

int ServiceLineLength(int sp, int end) {
    if (sp < 0 || sp >= servicePointCount) return 0;
    const ServicePoint* p = &servicePoints[sp];
    int n = 0;
    for (int k = 0; k < p->lineCount; k++) if (p->line[k].end == end) n++;
    return n;
}

void ServiceRecordDuration(int sp, float seconds) {
    if (sp < 0 || sp >= servicePointCount) return;
    ServicePoint* p = &servicePoints[sp];
    p->avgServiceTime = p->served == 0 ? seconds : p->avgServiceTime * 0.8f + seconds * 0.2f;
    p->served++;
}
