// day_budget.c - where a mover's day goes (see day_budget.h)
#include "day_budget.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "../entities/jobs.h"
#include "time.h"
#include "../simulation/balance.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    DB_SLEEP,        // resting (bed, chair, ground)
    DB_WORK,         // has a job, standing still: digging, building, crafting, ...
    DB_TO_WORK,      // has a job, moving: walking to it or carrying for it
    DB_TO_NEED,      // walking to food, drink, a toilet, a bed, warmth
    DB_EAT,
    DB_DRINK,
    DB_TOILET,
    DB_WARM,         // standing at a fire
    DB_IDLE,         // no job, no need
    DB_COUNT
} DayBucket;

static const char* bucketNames[DB_COUNT] = {
    "sleep", "work", ">work", ">needs", "eat", "drink", "toilet", "warm", "idle"
};

// >needs split by what the mover walks to
typedef enum { DW_FOOD, DW_DRINK, DW_TOILET, DW_BED, DW_WARMTH, DW_COUNT } DayWalk;
static const char* walkNames[DW_COUNT] = { "food", "drink", "toilet", "bed", "warmth" };

typedef enum { DC_MEALS, DC_DRINKS, DC_TOILET, DC_JOBS, DC_COUNT } DayCount;
static const char* countNames[DC_COUNT] = { "meals", "drinks", "toilet", "jobs" };

#define DB_MAX_DAYS 64
typedef struct {
    double moverSeconds;          // game seconds summed over active movers
    double secs[DB_COUNT];        // game seconds per bucket, summed over movers
    double walkSecs[DW_COUNT];    // the DB_TO_NEED seconds by target
    int counts[DC_COUNT];
    double daySeconds;            // game seconds of this day the run covered
} DayRecord;

static DayRecord days[DB_MAX_DAYS];
static int firstDay = -1;
static float lastX[MAX_MOVERS], lastY[MAX_MOVERS], lastZ[MAX_MOVERS];
static int lastState[MAX_MOVERS], lastJob[MAX_MOVERS];

static DayBucket BucketOf(const Mover* m, bool moving) {
    switch (m->freetimeState) {
        case FREETIME_RESTING:               return DB_SLEEP;
        case FREETIME_EATING:
        case FREETIME_FORAGING:
        case FREETIME_SCROUNGING:            return DB_EAT;
        case FREETIME_DRINKING:
        case FREETIME_DRINKING_NATURAL:
        case FREETIME_DRINKING_TAP:          return DB_DRINK;
        case FREETIME_USING_TOILET:
        case FREETIME_USING_BUSH:            return DB_TOILET;
        case FREETIME_WARMING:               return DB_WARM;
        case FREETIME_SEEKING_FOOD:
        case FREETIME_SEEKING_REST:
        case FREETIME_SEEKING_WARMTH:
        case FREETIME_SEEKING_DRINK:
        case FREETIME_SEEKING_NATURAL_WATER:
        case FREETIME_SEEKING_TAP:
        case FREETIME_SEEKING_TOILET:
        case FREETIME_SEEKING_BUSH:
        case FREETIME_SEEKING_FORAGE:
        case FREETIME_SEEKING_SCROUNGE:
        case FREETIME_SEEKING_BIN:           return DB_TO_NEED;
        default: break;
    }
    if (m->currentJobId >= 0) return moving ? DB_TO_WORK : DB_WORK;
    return DB_IDLE;
}

static int WalkOf(int s) {
    switch (s) {
        case FREETIME_SEEKING_FOOD:
        case FREETIME_SEEKING_SCROUNGE:
        case FREETIME_SEEKING_FORAGE:        return DW_FOOD;
        case FREETIME_SEEKING_DRINK:
        case FREETIME_SEEKING_NATURAL_WATER:
        case FREETIME_SEEKING_TAP:           return DW_DRINK;
        case FREETIME_SEEKING_TOILET:
        case FREETIME_SEEKING_BUSH:          return DW_TOILET;
        case FREETIME_SEEKING_REST:          return DW_BED;
        case FREETIME_SEEKING_WARMTH:        return DW_WARMTH;
        default:                             return -1;
    }
}

static bool IsDrinking(int s) {
    return s == FREETIME_DRINKING || s == FREETIME_DRINKING_NATURAL || s == FREETIME_DRINKING_TAP;
}

void DayBudgetBegin(void) {
    memset(days, 0, sizeof(days));
    firstDay = dayNumber;
    for (int i = 0; i < moverCount; i++) {
        lastX[i] = movers[i].x; lastY[i] = movers[i].y; lastZ[i] = movers[i].z;
        lastState[i] = movers[i].freetimeState;
        lastJob[i] = movers[i].currentJobId;
    }
}

void DayBudgetTick(void) {
    if (firstDay < 0) return;
    int d = dayNumber - firstDay;
    if (d < 0 || d >= DB_MAX_DAYS) return;
    DayRecord* r = &days[d];
    float dt = gameDeltaTime;
    r->daySeconds += dt;
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        float dx = m->x - lastX[i], dy = m->y - lastY[i], dz = m->z - lastZ[i];
        bool moving = dx * dx + dy * dy + dz * dz > 0.01f;
        r->moverSeconds += dt;
        r->secs[BucketOf(m, moving)] += dt;
        int w = WalkOf(m->freetimeState);
        if (w >= 0) r->walkSecs[w] += dt;

        int s = m->freetimeState;
        if (s == FREETIME_EATING && lastState[i] != FREETIME_EATING) r->counts[DC_MEALS]++;
        if (IsDrinking(s) && !IsDrinking(lastState[i])) r->counts[DC_DRINKS]++;
        if ((s == FREETIME_USING_TOILET || s == FREETIME_USING_BUSH) &&
            lastState[i] != FREETIME_USING_TOILET && lastState[i] != FREETIME_USING_BUSH) r->counts[DC_TOILET]++;
        // A job ended when the mover leaves it and the job is gone (done or cancelled;
        // a job handed back by UnassignJob stays active and is not counted)
        int pj = lastJob[i];
        if (pj >= 0 && m->currentJobId != pj && !jobs[pj].active) r->counts[DC_JOBS]++;

        lastX[i] = m->x; lastY[i] = m->y; lastZ[i] = m->z;
        lastState[i] = s;
        lastJob[i] = m->currentJobId;
    }
}

static void PrintRow(const char* label, const double secs[DB_COUNT], const double counts[DC_COUNT],
                     double moverDays, double secPerHour) {
    printf("  %-8s", label);
    for (int b = 0; b < DB_COUNT; b++) printf(" %6.1f", secs[b] / moverDays / secPerHour);
    printf("  |");
    for (int c = 0; c < DC_COUNT; c++) printf(" %6.1f", counts[c] / moverDays);
    printf("\n");
}

void DayBudgetPrint(void) {
    if (firstDay < 0) return;
    double secPerHour = dayLength / 24.0;
    printf("\n=== DAY BUDGET: game hours per mover per day (dayLength %.0f s, base speed %.0f) ===\n",
           dayLength, balance.baseMoverSpeed);
    printf("  %-8s", "day");
    for (int b = 0; b < DB_COUNT; b++) printf(" %6s", bucketNames[b]);
    printf("  |");
    for (int c = 0; c < DC_COUNT; c++) printf(" %6s", countNames[c]);
    printf("\n");

    double totSecs[DB_COUNT] = {0}, totCounts[DC_COUNT] = {0}, totWalk[DW_COUNT] = {0}, totMoverDays = 0.0;
    int fullDays = 0;
    for (int d = 0; d < DB_MAX_DAYS; d++) {
        DayRecord* r = &days[d];
        if (r->moverSeconds <= 0.0) continue;
        // hours per mover per day: divide by mover-days (movers present x fraction of the day)
        double moverDays = r->moverSeconds / dayLength;
        double counts[DC_COUNT];
        for (int c = 0; c < DC_COUNT; c++) counts[c] = r->counts[c];
        bool full = r->daySeconds >= dayLength * 0.99;
        char label[16];
        snprintf(label, sizeof(label), "%d%s", firstDay + d, full ? "" : "*");
        // a partial day is shown scaled to a whole day
        PrintRow(label, r->secs, counts, moverDays, secPerHour);
        if (full) {
            for (int b = 0; b < DB_COUNT; b++) totSecs[b] += r->secs[b];
            for (int c = 0; c < DC_COUNT; c++) totCounts[c] += r->counts[c];
            for (int w = 0; w < DW_COUNT; w++) totWalk[w] += r->walkSecs[w];
            totMoverDays += moverDays;
            fullDays++;
        }
    }
    if (fullDays > 0) {
        PrintRow("avg", totSecs, totCounts, totMoverDays, secPerHour);
        printf("  >needs (avg) by target:");
        for (int w = 0; w < DW_COUNT; w++) printf(" %s %.1f", walkNames[w], totWalk[w] / totMoverDays / secPerHour);
        printf("\n");
    }
    printf("  (* = partial day, scaled to 24h; avg = full days only. >work = moving with a job,\n"
           "   >needs = walking to food/drink/toilet/bed/warmth. Counts are per mover per day.)\n");
}
