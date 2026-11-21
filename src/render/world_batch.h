// World batch: the terrain passes' sprites as compact instances, drawn with one instanced
// call per flush through our own shader (a copy of raylib's default one, so the image is
// identical). 36 bytes per sprite instead of raylib's 144, no per-vertex work.
//
// Use: WorldBatchBegin(); <passes that draw only through FastSprite / FastRect>; WorldBatchEnd();
// Begin draws whatever raylib still has pending, End draws ours: drawing order with every
// other raylib call stays as it was. A raylib draw call inside the scope would be drawn out
// of order; WorldBatchEnd warns (once) when that happens.
// Base for shader effects later (docs/todo/architecture/rendering-optimization-candidates.md).
#ifndef WORLD_BATCH_H
#define WORLD_BATCH_H

#include "raylib.h"
#include <stdbool.h>

typedef struct {
    float x0, y0, x1, y1;      // screen rectangle (as DrawTexturePro computes it)
    float u0, v0, u1, v1;      // atlas coordinates
    unsigned char r, g, b, a;
    unsigned char kind, pad[3];   // WB_KIND_*: what the shader may do with this sprite
} WorldBatchSprite;

// Sprite kinds for shader effects (WorldBatchSetKind); 0 draws exactly as raylib would
enum { WB_KIND_PLAIN = 0, WB_KIND_WATER = 1, WB_KIND_FIRE = 2 };
extern unsigned char worldBatchKind;   // kind given to pushed sprites

#define WORLD_BATCH_CAP 65536

extern bool worldBatchActive;          // inside WorldBatchBegin/End
extern unsigned int worldBatchTexId;   // the atlas: other textures go through raylib
extern float worldBatchWhiteU, worldBatchWhiteV;   // centre of a pure white atlas texel
extern WorldBatchSprite worldBatchBuf[WORLD_BATCH_CAP];
extern int worldBatchCount;

bool WorldBatchInit(Texture2D atlas);  // false: no shader (falls back to raylib's batch)
void WorldBatchBegin(void);
void WorldBatchEnd(void);
void WorldBatchFlush(void);            // draw what's queued (keeps the scope open)
// Camera and clock for effects: screen = offset + cell * cellPixels; time in seconds
void WorldBatchSetView(float offsetX, float offsetY, float cellPixels, float time);
static inline void WorldBatchSetKind(unsigned char kind) { worldBatchKind = kind; }

static inline void WorldBatchPush(float x0, float y0, float x1, float y1,
                                  float u0, float v0, float u1, float v1, Color c) {
    if (worldBatchCount == WORLD_BATCH_CAP) WorldBatchFlush();
    WorldBatchSprite* s = &worldBatchBuf[worldBatchCount++];
    s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
    s->u0 = u0; s->v0 = v0; s->u1 = u1; s->v1 = v1;
    s->r = c.r; s->g = c.g; s->b = c.b; s->a = c.a;
    s->kind = worldBatchKind;
}

#endif
