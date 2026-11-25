// effects.c - Short visual effects (see effects.h; drawn by src/render/effects_draw.c)
#include "effects.h"
#include "../entities/mover.h"     // CELL_SIZE
#include "../world/material.h"
#include <math.h>
#include <string.h>

Fx fx[MAX_FX];
int fxCount;
bool effectsEnabled = true;   // setting effects

#define MAX_EMITTERS 128
typedef struct { uint8_t kind; uint8_t r, g, b; int x, y, z; float t, acc, rate; } FxEmitter;
static FxEmitter emitters[MAX_EMITTERS];
static int emitterCount;
static uint32_t fxRng = 0x9e3779b9u;
static int hitStopFrames;
static float fxClock;

float FxClock(void) { return fxClock; }

// Deterministic (headless runs and screenshots repeat)
static float FxRand01(void) {
    fxRng ^= fxRng << 13; fxRng ^= fxRng >> 17; fxRng ^= fxRng << 5;
    return (float)(fxRng >> 8) / 16777216.0f;
}
static float FxRandRange(float a, float b) { return a + (b - a) * FxRand01(); }

void ClearFx(void) {
    fxCount = 0;
    hitStopFrames = 0;
    emitterCount = 0;
    fxRng = 0x9e3779b9u;
}

int FxCount(void) { return fxCount; }

static Fx* NewFx(FxKind kind) {
    if (!effectsEnabled || fxCount >= MAX_FX) return NULL;
    Fx* f = &fx[fxCount++];
    memset(f, 0, sizeof(*f));
    f->kind = (uint8_t)kind;
    f->seed = fxRng;
    FxRand01();
    return f;
}

// What a broken or falling bit of a material looks like in dust and chunks
static void MaterialRGB(int mat, uint8_t* r, uint8_t* g, uint8_t* b) {
    int c[3] = { 130, 125, 120 };
    switch (mat) {
        case MAT_GRANITE:   c[0] = 145; c[1] = 142; c[2] = 145; break;
        case MAT_SANDSTONE: c[0] = 205; c[1] = 172; c[2] = 120; break;
        case MAT_SLATE:     c[0] = 95;  c[1] = 100; c[2] = 112; break;
        case MAT_DIRT:      c[0] = 125; c[1] = 88;  c[2] = 56;  break;
        case MAT_CLAY:      c[0] = 172; c[1] = 102; c[2] = 70;  break;
        case MAT_GRAVEL:    c[0] = 150; c[1] = 145; c[2] = 135; break;
        case MAT_SAND:      c[0] = 218; c[1] = 196; c[2] = 140; break;
        case MAT_PEAT:      c[0] = 75;  c[1] = 58;  c[2] = 42;  break;
        case MAT_BRICK:     c[0] = 170; c[1] = 82;  c[2] = 62;  break;
        case MAT_CONCRETE:  c[0] = 172; c[1] = 172; c[2] = 166; break;
        case MAT_IRON:      c[0] = 112; c[1] = 112; c[2] = 122; break;
        case MAT_GLASS:     c[0] = 200; c[1] = 228; c[2] = 238; break;
        case MAT_MUD: case MAT_COB: c[0] = 132; c[1] = 96; c[2] = 66; break;
        default:
            if (mat >= 0 && mat < MAT_COUNT && IsWoodMaterial((MaterialType)mat)) { c[0] = 150; c[1] = 106; c[2] = 62; }
            break;
    }
    *r = (uint8_t)c[0]; *g = (uint8_t)c[1]; *b = (uint8_t)c[2];
}

bool FxHitStopFrame(void) {
    if (hitStopFrames <= 0) return false;
    hitStopFrames--;
    return true;
}

void FxBlast(float wx, float wy, int z, float power) {
    if (!effectsEnabled) return;
    float R = power * CELL_SIZE * 0.8f, life = 0.45f + 0.05f * power;
    // The fireball: a cluster of lumps, not one disc. A big one in the middle, smaller ones off
    // centre, each with its own size, start and length (expands fast, then slows, unevenly)
    int lumps = 4 + (int)(power * 0.5f);
    if (lumps > 9) lumps = 9;
    Fx* f = NULL;
    for (int i = 0; i < lumps; i++) {
        Fx* l = NewFx(FX_BLAST);
        if (!l) break;
        if (i == 0) f = l;
        float a = FxRandRange(0.0f, 6.2832f), d = i == 0 ? 0.0f : R * FxRandRange(0.15f, 0.4f);
        l->x = wx + cosf(a) * d; l->y = wy + sinf(a) * d; l->z = z;
        l->radius = i == 0 ? R * 0.75f : R * FxRandRange(0.4f, 0.65f);
        l->life = life * (i == 0 ? 1.0f : FxRandRange(0.75f, 1.1f));
        l->age = i == 0 ? 0.0f : -FxRandRange(0.0f, 0.07f);
    }
    if (!f) return;
    // Hit stop: a big bang freezes the picture for a moment
    int stop = power >= 8.0f ? 4 : power >= 6.0f ? 2 : 0;
    if (stop > hitStopFrames) hitStopFrames = stop;
    Fx* fl = NewFx(FX_FLASH);
    if (fl) { fl->x = wx; fl->y = wy; fl->z = z; fl->radius = power * CELL_SIZE * 0.7f; fl->life = 0.1f; }
    // Smoke left behind: a cloud over the blast as the fireball dies, rising and swelling
    int puffs = 10 + (int)(2.0f * power);
    float spread = power * CELL_SIZE * 0.55f;
    for (int i = 0; i < puffs; i++) {
        Fx* s = NewFx(FX_SMOKE);
        if (!s) break;
        float a = FxRandRange(0.0f, 6.2832f), d = spread * sqrtf(FxRand01());
        s->x = wx + cosf(a) * d; s->y = wy + sinf(a) * d; s->z = z;
        s->vx = cosf(a) * FxRandRange(2.0f, 8.0f) + FxRandRange(-3.0f, 3.0f);
        s->vy = sinf(a) * FxRandRange(2.0f, 8.0f) - FxRandRange(6.0f, 14.0f);   // drifts up the screen
        s->radius = CELL_SIZE * FxRandRange(0.5f, 1.0f) * (0.7f + 0.06f * power);
        s->age = -FxRandRange(0.25f, 0.6f) * f->life;   // appears while the fireball fades
        s->life = FxRandRange(2.5f, 4.5f);
        int g = FxRand01() < 0.5f ? 70 : 96;   // two shades: some depth
        s->r = (uint8_t)g; s->g = (uint8_t)(g - 3); s->b = (uint8_t)(g - 6);
    }
}

void FxDust(float wx, float wy, int z, int mat, int n, float spread) {
    uint8_t r, g, b;
    MaterialRGB(mat, &r, &g, &b);
    if (mat < 0) { r = 92; g = 88; b = 84; }   // smoke
    for (int i = 0; i < n; i++) {
        Fx* f = NewFx(FX_DUST);
        if (!f) return;
        float a = FxRandRange(0.0f, 6.2832f), d = FxRandRange(0.0f, spread);
        f->x = wx + cosf(a) * d; f->y = wy + sinf(a) * d; f->z = z;
        f->vx = FxRandRange(-6.0f, 6.0f);
        f->vy = FxRandRange(-14.0f, -6.0f);   // drifts up the screen (rising)
        f->radius = CELL_SIZE * FxRandRange(0.3f, 0.5f);
        f->life = FxRandRange(0.9f, 1.6f);
        f->r = r; f->g = g; f->b = b;
    }
}

void FxChunks(int cx, int cy, int cz, int mat, float fromX, float fromY, int n, float speed) {
    uint8_t r, g, b;
    MaterialRGB(mat, &r, &g, &b);
    float px = (cx + 0.5f) * CELL_SIZE, py = (cy + 0.5f) * CELL_SIZE;
    float dx = px - fromX, dy = py - fromY, d = sqrtf(dx * dx + dy * dy);
    if (d < 1.0f) { dx = 0.0f; dy = -1.0f; d = 1.0f; }
    for (int i = 0; i < n; i++) {
        Fx* f = NewFx(FX_CHUNK);
        if (!f) return;
        float a = atan2f(dy, dx) + FxRandRange(-0.6f, 0.6f), s = speed * FxRandRange(0.5f, 1.2f);
        f->x = px + FxRandRange(-10.0f, 10.0f); f->y = py + FxRandRange(-10.0f, 10.0f); f->z = cz;
        f->vx = cosf(a) * s; f->vy = sinf(a) * s;
        f->h = FxRandRange(2.0f, 10.0f); f->vh = FxRandRange(60.0f, 140.0f);
        f->life = 1.2f;
        float shade = FxRandRange(0.7f, 1.05f);
        f->r = (uint8_t)fminf(255.0f, r * shade); f->g = (uint8_t)fminf(255.0f, g * shade); f->b = (uint8_t)fminf(255.0f, b * shade);
        f->radius = FxRand01() < 0.3f ? 2.0f : 1.0f;   // sprite pixels across
    }
}

void FxSplash(float wx, float wy, int z, float dirX, float dirY, int n, float speed) {
    for (int i = 0; i < n; i++) {
        Fx* f = NewFx(FX_CHUNK);   // a droplet: a pixel flying in an arc, like a chunk
        if (!f) return;
        float a = atan2f(dirY, dirX) + FxRandRange(-0.7f, 0.7f), s = speed * FxRandRange(0.4f, 1.1f);
        f->x = wx + FxRandRange(-10.0f, 10.0f); f->y = wy + FxRandRange(-10.0f, 10.0f); f->z = z;
        f->vx = cosf(a) * s; f->vy = sinf(a) * s;
        f->h = FxRandRange(0.0f, 6.0f); f->vh = FxRandRange(90.0f, 180.0f);
        f->life = 1.0f;
        if (FxRand01() < 0.5f) { f->r = 226; f->g = 238; f->b = 250; }
        else { f->r = 120; f->g = 170; f->b = 224; }
        f->radius = 1.0f;
    }
    Fx* p = NewFx(FX_DUST);   // spray
    if (!p) return;
    p->x = wx; p->y = wy; p->z = z;
    p->vx = dirX * 10.0f; p->vy = dirY * 10.0f - 8.0f;
    p->radius = CELL_SIZE * FxRandRange(0.35f, 0.55f);
    p->life = FxRandRange(0.5f, 0.9f);
    p->r = 228; p->g = 236; p->b = 244;
}

void FxFlame(float wx, float wy, int z, float lvl) {
    Fx* f = NewFx(FX_BLAST);   // one short lump of fireball per cell: together they roll as a front
    if (!f) return;
    f->x = wx + FxRandRange(-8.0f, 8.0f); f->y = wy + FxRandRange(-8.0f, 8.0f); f->z = z;
    f->radius = CELL_SIZE * (0.5f + 0.07f * lvl) * FxRandRange(0.8f, 1.2f);
    f->life = FxRandRange(0.3f, 0.45f);
    if (FxRand01() < 0.3f) {
        Fx* s = NewFx(FX_SMOKE);
        if (!s) return;
        s->x = wx; s->y = wy; s->z = z;
        s->vx = FxRandRange(-4.0f, 4.0f); s->vy = -FxRandRange(6.0f, 12.0f);
        s->radius = CELL_SIZE * FxRandRange(0.4f, 0.7f);
        s->age = -0.25f;
        s->life = FxRandRange(1.2f, 2.2f);
        int g = FxRand01() < 0.5f ? 70 : 96;
        s->r = (uint8_t)g; s->g = (uint8_t)(g - 3); s->b = (uint8_t)(g - 6);
    }
}

static void AddEmitter(FxKind kind, int x, int y, int z, int mat, float seconds, float rate) {
    if (!effectsEnabled || emitterCount >= MAX_EMITTERS) return;
    FxEmitter* e = &emitters[emitterCount++];
    memset(e, 0, sizeof(*e));
    e->kind = (uint8_t)kind; e->x = x; e->y = y; e->z = z; e->t = seconds; e->rate = rate;
    MaterialRGB(mat, &e->r, &e->g, &e->b);
}

void FxEmitSparks(int x, int y, int z, float seconds) { AddEmitter(FX_SPARK, x, y, z, -1, seconds, 24.0f); }
void FxEmitDustFall(int x, int y, int z, int mat, float seconds) { AddEmitter(FX_DUST, x, y, z, mat, seconds, 5.0f); }

static void EmitterTick(FxEmitter* e, float dt) {
    e->acc += dt * e->rate;
    while (e->acc >= 1.0f) {
        e->acc -= 1.0f;
        Fx* f = NewFx((FxKind)e->kind);
        if (!f) return;
        f->z = e->z;
        if (e->kind == FX_SPARK) {
            f->x = (e->x + 0.5f) * CELL_SIZE + FxRandRange(-4.0f, 4.0f);
            f->y = (e->y + 0.5f) * CELL_SIZE + FxRandRange(-4.0f, 4.0f);
            float a = FxRandRange(0.0f, 6.2832f), s = FxRandRange(20.0f, 60.0f);
            f->vx = cosf(a) * s; f->vy = sinf(a) * s - 20.0f;
            f->life = FxRandRange(0.15f, 0.35f);
            bool white = FxRand01() < 0.4f;
            f->r = 255; f->g = white ? 250 : 210; f->b = white ? 220 : 70;
        } else {   // dust trickling from the underside: small, slow, falling away
            f->x = (e->x + FxRandRange(0.15f, 0.85f)) * CELL_SIZE;
            f->y = (e->y + FxRandRange(0.15f, 0.85f)) * CELL_SIZE;
            f->vx = FxRandRange(-2.0f, 2.0f); f->vy = FxRandRange(2.0f, 6.0f);
            f->radius = CELL_SIZE * FxRandRange(0.12f, 0.2f);
            f->life = FxRandRange(0.5f, 0.9f);
            f->r = e->r; f->g = e->g; f->b = e->b;
        }
    }
}

void FxTick(float dt) {
    if (dt <= 0.0f) return;
    fxClock += dt;
    if (fxClock > 10000.0f) fxClock -= 10000.0f;
    for (int i = 0; i < emitterCount; ) {
        EmitterTick(&emitters[i], dt);
        emitters[i].t -= dt;
        if (emitters[i].t <= 0.0f) emitters[i] = emitters[--emitterCount];
        else i++;
    }
    int w = 0;
    for (int i = 0; i < fxCount; i++) {
        Fx* f = &fx[i];
        f->age += dt;
        if (f->age >= f->life) continue;
        if (f->age < 0.0f) { if (w != i) fx[w] = *f; w++; continue; }   // not there yet
        f->x += f->vx * dt;
        f->y += f->vy * dt;
        if (f->kind == FX_CHUNK) {
            f->vh -= 420.0f * dt;   // gravity (world units / s^2)
            f->h += f->vh * dt;
            if (f->h <= 0.0f) {      // lands: a small bounce, then slides to a stop
                f->h = 0.0f;
                f->vh = f->vh < -60.0f ? -f->vh * 0.3f : 0.0f;
                f->vx *= 0.5f; f->vy *= 0.5f;
            }
        } else if (f->kind == FX_DUST) {
            f->vx *= 1.0f - 1.5f * dt; f->vy *= 1.0f - 1.5f * dt;
        } else if (f->kind == FX_SMOKE) {
            f->vx *= 1.0f - 0.4f * dt; f->vy *= 1.0f - 0.4f * dt;
        }
        if (w != i) fx[w] = *f;
        w++;
    }
    fxCount = w;
}
