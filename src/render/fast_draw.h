// Fast sprite and rectangle drawing for the world passes.
//
// FastSprite / FastRect give exactly the image of
//   DrawTexturePro(tex, src, dest, (Vector2){0, 0}, 0, tint)   and   DrawRectangleRec(rec, color)
// (same vertices, texture coordinates and colors, in the same order, into raylib's own batch,
// so drawing order with every other raylib call is unchanged), but write the quad in one call
// (rlQuadFast, a small patch in vendor/raylib/rlgl.h) instead of ~12 per-vertex calls.
// Zoomed out on a big map the world is ~500K quads per frame: this is where the time goes.
// Check rendering changes with --screenshot (root CLAUDE.md, "Checking rendering changes").
#ifndef FAST_DRAW_H
#define FAST_DRAW_H

#include "raylib.h"
#include "world_batch.h"

// vendor/raylib/rlgl.h (NavKit patch)
void rlQuadFast(unsigned int textureId, float x0, float y0, float x1, float y1,
                float u0, float v0, float u1, float v1,
                unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void rlDrawRenderBatchActive(void);

static inline void FastSprite(Texture2D tex, Rectangle src, Rectangle dest, Color tint) {
    if (tex.id == 0) return;   // DrawTexturePro draws nothing either
    if (src.width < 0 || src.height < 0 || dest.width < 0 || dest.height < 0) {
        DrawTexturePro(tex, src, dest, (Vector2){0, 0}, 0, tint);   // flips: the general path
        return;
    }
    float w = (float)tex.width, h = (float)tex.height;
    if (worldBatchActive) {
        if (tex.id == worldBatchTexId) {
            WorldBatchPush(dest.x, dest.y, dest.x + dest.width, dest.y + dest.height,
                           src.x / w, src.y / h, (src.x + src.width) / w, (src.y + src.height) / h, tint);
            return;
        }
        WorldBatchFlush();   // another texture: keep the order, draw it through raylib now
        rlQuadFast(tex.id, dest.x, dest.y, dest.x + dest.width, dest.y + dest.height,
                   src.x / w, src.y / h, (src.x + src.width) / w, (src.y + src.height) / h,
                   tint.r, tint.g, tint.b, tint.a);
        rlDrawRenderBatchActive();
        return;
    }
    rlQuadFast(tex.id, dest.x, dest.y, dest.x + dest.width, dest.y + dest.height,
               src.x / w, src.y / h, (src.x + src.width) / w, (src.y + src.height) / h,
               tint.r, tint.g, tint.b, tint.a);
}

static inline void FastRect(Rectangle rec, Color c) {
    if (worldBatchActive) {   // a white atlas texel: white * c, as raylib's white texture gives
        WorldBatchPush(rec.x, rec.y, rec.x + rec.width, rec.y + rec.height,
                       worldBatchWhiteU, worldBatchWhiteV, worldBatchWhiteU, worldBatchWhiteV, c);
        return;
    }
    Texture2D t = GetShapesTexture();
    Rectangle s = GetShapesTextureRectangle();
    rlQuadFast(t.id, rec.x, rec.y, rec.x + rec.width, rec.y + rec.height,
               s.x / t.width, s.y / t.height, (s.x + s.width) / t.width, (s.y + s.height) / t.height,
               c.r, c.g, c.b, c.a);
}

#endif
