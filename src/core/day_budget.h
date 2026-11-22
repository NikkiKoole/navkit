// day_budget.h - where a mover's day goes (headless measuring tool)
//
// --day-budget on a headless run (--game, --scenario or --load): every tick each active
// mover's time is put in one bucket (sleep, work, walking to work, walking for a need,
// eating, drinking, toilet, warming, idle) and meals / drinks / toilet visits / jobs are
// counted. At the end it prints hours per mover per game day, per day and averaged over
// the full days. Use it to see what a balance change (speeds, need rates, day length)
// does to a day: run the same setup with and without the change.
#ifndef DAY_BUDGET_H
#define DAY_BUDGET_H

void DayBudgetBegin(void);   // before the first tick
void DayBudgetTick(void);    // after every SimStep
void DayBudgetPrint(void);   // after the run

#endif
