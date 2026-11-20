// scenarios.h - Built-in demo/test scenarios: one registry, three ways in
// (--scenario <name>, the dev UI "Scenarios" list, the console "scenario" command),
// plus headless runs (--headless --scenario <name> --ticks N).
//
// Adding one: write a setup function in scenarios.c and add a row to the table
// there. RunScenario() first resets the world (terrain from the row, or the flat
// default map), then calls setup, which only adds what is special (cells, movers,
// settings). Setup must set every toggle it relies on.
#ifndef SCENARIOS_H
#define SCENARIOS_H

#include <stdbool.h>

typedef struct {
    const char* name;          // CLI/console name, e.g. "ladders"
    const char* description;   // one line, shown in lists and tooltips
    const char* terrain;       // terrainNames[] entry to generate first, or NULL for the flat default map
    void (*setup)(void);       // adds cells, movers, settings on top of the reset world
    void (*tickStats)(void);   // optional: sampled after every headless tick
    void (*printStats)(void);  // optional: printed at the end of a headless run
} Scenario;

int ScenarioCount(void);
const Scenario* ScenarioAt(int index);
const Scenario* ScenarioFind(const char* name);   // case-insensitive, NULL if unknown
const char** ScenarioNames(void);                 // ScenarioCount() names, for autocomplete
const Scenario* ScenarioCurrent(void);            // last scenario started, or NULL

// Reset the world and run the scenario. Returns false (and prints the known
// names to stdout) if the name is unknown.
bool RunScenario(const char* name);
void ScenarioPrintList(void);                     // names + descriptions to stdout

// Clear entities and simulations, generate the terrain (NULL = flat 32x32 dirt
// map, same as startup) and rebuild derived state (spatial grids, sims, lighting,
// HPA* graph). Settings that scenarios change are restored to their values from
// before the first scenario ran.
void ScenarioResetWorld(const char* terrain);

extern int scenarioMovers;   // mover count override for crowd scenarios (0 = default)

#endif
