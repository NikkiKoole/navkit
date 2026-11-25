// explosion.h - Explosions: a one-off blast that breaks walls and floors, pushes loose items
// by their weight and knocks movers back (docs/todo/world/explosions.md).
//
// Power is in cells: in open air a blast of power 6 reaches 6 cells. A wall or floor in the way
// soaks up its strength (ExplosionCellStrength) from what is left; if enough is left it breaks
// (mined: natural rock and built walls alike, drops included) and the rest goes on through it.
// Bedrock and tree cells stop it. Transient state (fuses, flying items, knocked movers) is not
// saved: after a load items lie where they are and movers stand.
#ifndef EXPLOSION_H
#define EXPLOSION_H

#include <stdbool.h>

#define EXPLOSION_MAX_POWER 24.0f

extern float blastPower;   // setting blastPower: one gunpowder charge set by a blast-mining job
extern float blastFuse;    // setting blastFuse: game seconds from lighting to the bang

typedef struct {
    int cellsBroken;     // walls / windows
    int floorsBroken;
    int itemsPushed;
    int moversKnocked;
    int cellsIgnited;
} ExplosionResult;

// Blast at cell (x, y, z). A charge inside a wall (drilled in) breaks that wall first.
ExplosionResult Explode(int x, int y, int z, float power);

// A lit charge: explodes after `seconds` game seconds. Returns false if all fuse slots are taken.
bool ExplosionLightFuse(int x, int y, int z, float power, float seconds);
int ExplosionFuseCount(void);

// Fuses + flying items (SimStep, after ItemsTick)
void ExplosionsTick(float dt);

// Movement loop (UpdateMovers): true while the mover is flying back or dazed (it doesn't walk).
// False once it lands somewhere it can't stand: the normal update lets it fall.
bool ExplosionMoverTick(int moverIdx, float dt);
bool MoverKnocked(int moverIdx);
// Knock a mover away from (fromX, fromY) (cell units) with the push of a blast of `power` there
// (structural collapse uses it for whoever a wall lands on)
void ExplosionKnockMover(int moverIdx, float fromX, float fromY, float power);

// Strength of the wall at (x, y, z) in power units, -1 = unbreakable (bedrock, trees), 0 = open
float ExplosionCellStrength(int x, int y, int z);

int ExplosionFlyingCount(void);
void ClearExplosions(void);

#endif
