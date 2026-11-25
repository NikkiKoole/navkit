// effects.h - Short visual effects: blasts, flashes, dust, flying chunks, sparks.
//
// Data and timing only (drawn by src/render/effects_draw.c): spawned by the simulation
// (explosion.c, support.c), aged on sim ticks (FxTick from SimStep: frozen while paused, the
// same in headless runs and screenshots). Positions in world units (like items: CELL_SIZE per
// cell). Not saved. Blasts, flashes and dust are drawn as dithered circles on the 8x8 sprite
// pixel grid; chunks and sparks as single sprite pixels.
#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FX_BLAST,    // fireball: white -> yellow -> orange -> red -> smoke, growing, thinning out
    FX_FLASH,    // a big faint white disc for a moment
    FX_DUST,     // a small puff in a material's colour, drifting and thinning out
    FX_CHUNK,    // a bit of the broken wall flying in an arc
    FX_SPARK,    // a bright pixel (fuses, gunpowder by a fire)
    FX_SMOKE,    // a blast's smoke: shows as the fireball dies, rises, swells, thins out late
} FxKind;

typedef struct {
    uint8_t kind;
    uint8_t r, g, b;
    int z;
    float x, y;          // world units
    float vx, vy;        // world units per second
    float h, vh;         // chunks: height above the ground (world units), drawn as an offset up
    float age, life;     // seconds (age < 0: not there yet, a delayed start)
    float radius;        // world units (blast: full size; dust: start size)
    uint32_t seed;
} Fx;

#define MAX_FX 2048
extern Fx fx[MAX_FX];
extern int fxCount;
extern bool effectsEnabled;   // setting effects

// The blast of an explosion of `power` (cells of reach) centred at (wx, wy) on level z
void FxBlast(float wx, float wy, int z, float power);
// n dust puffs around (wx, wy) within `spread` world units, in the colour of material mat
void FxDust(float wx, float wy, int z, int mat, int n, float spread);
// n chunks of material mat thrown from cell (cx, cy, cz) away from (fromX, fromY) (world units)
void FxChunks(int cx, int cy, int cz, int mat, float fromX, float fromY, int n, float speed);
// A splash: n droplets thrown along (dirX, dirY) from (wx, wy) at `speed` world units/s, and spray
void FxSplash(float wx, float wy, int z, float dirX, float dirY, int n, float speed);
// Sparks from cell (x, y, z) for `seconds` (a lit fuse)
void FxEmitSparks(int x, int y, int z, float seconds);
// Dust trickling from cell (x, y, z) for `seconds` (an unsupported piece creaking)
void FxEmitDustFall(int x, int y, int z, int mat, float seconds);

void FxTick(float dt);   // SimStep

// Hit stop: a big blast freezes the game for a few frames (the windowed loop asks every frame:
// true = skip this frame's ticks). Headless runs never ask, so they're unaffected.
bool FxHitStopFrame(void);

// Seconds of effect time (advances with FxTick, so it stands still while paused): drifts the
// noise in drawn clouds (smoke and steam from their simulations too)
float FxClock(void);
void ClearFx(void);
int FxCount(void);

#endif
