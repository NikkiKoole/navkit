// sim_step.h - One game tick: every system, in order, in one place.
//
// The windowed game and the headless runner both call SimStep, so headless
// results (tests, scenarios-check, benchmarks) run exactly what the game runs.
//
// TickWithDt (mover.c) stays as it is: time, HPA* updates, weather, the cell
// simulations (water, fire, smoke, steam, temperature, wear), lighting, rooms,
// plants, spatial grids, repaths and mover movement. Many tests call it directly
// to run just movers and simulations. SimStep is TickWithDt plus the systems
// that make it a game: items, animals, trains, designations, needs, mood, jobs.
#ifndef SIM_STEP_H
#define SIM_STEP_H

void SimStep(float dt);

#endif
