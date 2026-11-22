// World batch: see world_batch.h
#include "world_batch.h"
#include "raylib/rlgl.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int rlNavKitPendingVertices(void);   // vendor/raylib/rlgl.h (NavKit patch)

bool worldBatchActive = false;
unsigned int worldBatchTexId = 0;
float worldBatchWhiteU = 0.0f, worldBatchWhiteV = 0.0f;
WorldBatchSprite worldBatchBuf[WORLD_BATCH_CAP];
int worldBatchCount = 0;
unsigned char worldBatchKind = WB_KIND_PLAIN;
extern bool shaderEffects;   // src/simulation/lighting.c (setting "shaderEffects")
static float wbOffsetX, wbOffsetY, wbCellPixels = 32.0f, wbTime;

static unsigned int wbShader, wbVao, wbCornerVbo, wbInstVbo;
static int wbLocMvp = -1, wbLocColDiffuse = -1, wbLocTexture = -1;
static int wbLocView = -1, wbLocTime = -1, wbLocEffects = -1;
static bool wbReady = false, wbWarned = false;
static int wbLocCorner = -1, wbLocRect = -1, wbLocUv = -1, wbLocColor = -1, wbLocKind = -1;

// Instance attributes of the bound vertex array, reading from the bound buffer
static void WbSetInstanceAttribs(void) {
    int stride = (int)sizeof(WorldBatchSprite);
    rlSetVertexAttribute((unsigned)wbLocRect, 4, RL_FLOAT, false, stride, (int)offsetof(WorldBatchSprite, x0));
    rlEnableVertexAttribute((unsigned)wbLocRect);
    rlSetVertexAttributeDivisor((unsigned)wbLocRect, 1);
    rlSetVertexAttribute((unsigned)wbLocUv, 4, RL_FLOAT, false, stride, (int)offsetof(WorldBatchSprite, u0));
    rlEnableVertexAttribute((unsigned)wbLocUv);
    rlSetVertexAttributeDivisor((unsigned)wbLocUv, 1);
    rlSetVertexAttribute((unsigned)wbLocColor, 4, RL_UNSIGNED_BYTE, true, stride, (int)offsetof(WorldBatchSprite, r));
    rlEnableVertexAttribute((unsigned)wbLocColor);
    rlSetVertexAttributeDivisor((unsigned)wbLocColor, 1);
    rlSetVertexAttribute((unsigned)wbLocKind, 1, RL_UNSIGNED_BYTE, false, stride, (int)offsetof(WorldBatchSprite, kind));
    rlEnableVertexAttribute((unsigned)wbLocKind);
    rlSetVertexAttributeDivisor((unsigned)wbLocKind, 1);
}

// raylib's default shader (rlgl.h, GL 3.3), except that the quad's corners come from the
// instance: the corner picks x0 or x1 and u0 or u1, the values raylib would have put in the
// vertices. Same arithmetic in the fragment shader, so the pixels are the same.
static const char* wbVS =
    "#version 330\n"
    "in vec2 corner;\n"            // (0 or 1, 0 or 1)
    "in vec4 rect;\n"              // x0 y0 x1 y1
    "in vec4 uvRect;\n"            // u0 v0 u1 v1
    "in vec4 color;\n"
    "in float kind;\n"             // WB_KIND_*
    "out vec2 fragTexCoord;\n"
    "out vec4 fragColor;\n"
    "out vec2 cellPos;\n"          // world position in cells (for effects)
    "flat out int fragKind;\n"
    "uniform mat4 mvp;\n"
    "uniform vec3 view;\n"         // offset x, offset y, pixels per cell
    "void main() {\n"
    "    vec2 p = vec2(corner.x < 0.5 ? rect.x : rect.z, corner.y < 0.5 ? rect.y : rect.w);\n"
    "    fragTexCoord = vec2(corner.x < 0.5 ? uvRect.x : uvRect.z, corner.y < 0.5 ? uvRect.y : uvRect.w);\n"
    "    fragColor = color;\n"
    "    cellPos = (p - view.xy) / view.z;\n"
    "    fragKind = int(kind + 0.5);\n"
    "    gl_Position = mvp*vec4(p, -0.5, 1.0);\n"
    "}\n";
// With effects off (or kind 0) the result is raylib's default shader's exactly. The effects
// work on the 8x8 sprite pixel grid (positions snapped to 1/8 cell) so they stay pixel art.
static const char* wbFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "in vec2 cellPos;\n"
    "flat in int fragKind;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform float time;\n"
    "uniform int effects;\n"
    "float hash(vec2 c) { return fract(sin(dot(c, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    finalColor = texelColor*colDiffuse*fragColor;\n"
    "    if (effects == 0 || fragKind == 0) return;\n"
    "    vec2 px = floor(cellPos * 8.0) / 8.0;\n"          // sprite pixel grid
    "    if (fragKind == 1) {\n"                          // water: drifting light bands + glints
    "        float w = sin(px.x * 1.7 + time * 1.3) * sin(px.y * 1.3 - time * 0.9)\n"
    "                + 0.6 * sin((px.x + px.y) * 0.9 + time * 2.1);\n"
    "        finalColor.rgb *= 1.0 + 0.14 * w;\n"
    "        float g = hash(floor(cellPos * 8.0) + floor(time * 3.0));\n"
    "        if (w > 1.2 && g > 0.93) finalColor.rgb += vec3(0.25, 0.3, 0.35);\n"
    "    } else if (fragKind == 2) {\n"                   // fire: each cell flickers on its own
    "        float n = hash(floor(cellPos));\n"
    "        float f = 0.8 + 0.2 * sin(time * 11.0 + n * 40.0) + 0.12 * sin(time * 23.0 + n * 9.0 + px.y * 6.0);\n"
    "        finalColor.rgb = min(finalColor.rgb * f * 1.15, vec3(1.0));\n"
    "    }\n"
    "}\n";

// Same as rlgl's (static) rlMatrixMultiply, so the MVP matches raylib's to the bit
static Matrix WbMul(Matrix l, Matrix r) {
    Matrix m;
    m.m0  = l.m0*r.m0  + l.m1*r.m4  + l.m2*r.m8   + l.m3*r.m12;
    m.m1  = l.m0*r.m1  + l.m1*r.m5  + l.m2*r.m9   + l.m3*r.m13;
    m.m2  = l.m0*r.m2  + l.m1*r.m6  + l.m2*r.m10  + l.m3*r.m14;
    m.m3  = l.m0*r.m3  + l.m1*r.m7  + l.m2*r.m11  + l.m3*r.m15;
    m.m4  = l.m4*r.m0  + l.m5*r.m4  + l.m6*r.m8   + l.m7*r.m12;
    m.m5  = l.m4*r.m1  + l.m5*r.m5  + l.m6*r.m9   + l.m7*r.m13;
    m.m6  = l.m4*r.m2  + l.m5*r.m6  + l.m6*r.m10  + l.m7*r.m14;
    m.m7  = l.m4*r.m3  + l.m5*r.m7  + l.m6*r.m11  + l.m7*r.m15;
    m.m8  = l.m8*r.m0  + l.m9*r.m4  + l.m10*r.m8  + l.m11*r.m12;
    m.m9  = l.m8*r.m1  + l.m9*r.m5  + l.m10*r.m9  + l.m11*r.m13;
    m.m10 = l.m8*r.m2  + l.m9*r.m6  + l.m10*r.m10 + l.m11*r.m14;
    m.m11 = l.m8*r.m3  + l.m9*r.m7  + l.m10*r.m11 + l.m11*r.m15;
    m.m12 = l.m12*r.m0 + l.m13*r.m4 + l.m14*r.m8  + l.m15*r.m12;
    m.m13 = l.m12*r.m1 + l.m13*r.m5 + l.m14*r.m9  + l.m15*r.m13;
    m.m14 = l.m12*r.m2 + l.m13*r.m6 + l.m14*r.m10 + l.m15*r.m14;
    m.m15 = l.m12*r.m3 + l.m13*r.m7 + l.m14*r.m11 + l.m15*r.m15;
    return m;
}

bool WorldBatchInit(Texture2D atlas) {
    wbReady = false;
    // A pure white texel: rectangles sample it instead of raylib's 1x1 white texture
    // (white * color either way), so rectangles and sprites share one texture
    Image img = LoadImageFromTexture(atlas);
    if (!img.data) return false;
    bool found = false;
    for (int y = 0; y < img.height && !found; y++)
        for (int x = 0; x < img.width && !found; x++) {
            Color c = GetImageColor(img, x, y);
            if (c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255) {
                worldBatchWhiteU = (x + 0.5f) / atlas.width;
                worldBatchWhiteV = (y + 0.5f) / atlas.height;
                found = true;
            }
        }
    UnloadImage(img);
    if (!found) return false;

    wbShader = rlLoadShaderCode(wbVS, wbFS);
    if (wbShader == 0) return false;
    wbLocMvp = rlGetLocationUniform(wbShader, "mvp");
    wbLocColDiffuse = rlGetLocationUniform(wbShader, "colDiffuse");
    wbLocTexture = rlGetLocationUniform(wbShader, "texture0");
    wbLocView = rlGetLocationUniform(wbShader, "view");
    wbLocTime = rlGetLocationUniform(wbShader, "time");
    wbLocEffects = rlGetLocationUniform(wbShader, "effects");
    int locCorner = rlGetLocationAttrib(wbShader, "corner");
    int locRect = rlGetLocationAttrib(wbShader, "rect");
    int locUv = rlGetLocationAttrib(wbShader, "uvRect");
    int locColor = rlGetLocationAttrib(wbShader, "color");
    int locKind = rlGetLocationAttrib(wbShader, "kind");
    if (locCorner < 0 || locRect < 0 || locUv < 0 || locColor < 0 || locKind < 0) return false;
    wbLocCorner = locCorner; wbLocRect = locRect; wbLocUv = locUv; wbLocColor = locColor; wbLocKind = locKind;

    wbVao = rlLoadVertexArray();
    rlEnableVertexArray(wbVao);
    // Two triangles in raylib's order for a quad (TL BL BR, TL BR TR)
    static const float corners[12] = { 0,0, 0,1, 1,1,  0,0, 1,1, 1,0 };
    wbCornerVbo = rlLoadVertexBuffer(corners, sizeof(corners), false);
    rlSetVertexAttribute((unsigned)locCorner, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute((unsigned)locCorner);

    wbInstVbo = rlLoadVertexBuffer(NULL, (int)sizeof(worldBatchBuf), true);
    int stride = (int)sizeof(WorldBatchSprite);
    rlSetVertexAttribute((unsigned)locRect, 4, RL_FLOAT, false, stride, (int)offsetof(WorldBatchSprite, x0));
    rlEnableVertexAttribute((unsigned)locRect);
    rlSetVertexAttributeDivisor((unsigned)locRect, 1);
    rlSetVertexAttribute((unsigned)locUv, 4, RL_FLOAT, false, stride, (int)offsetof(WorldBatchSprite, u0));
    rlEnableVertexAttribute((unsigned)locUv);
    rlSetVertexAttributeDivisor((unsigned)locUv, 1);
    rlSetVertexAttribute((unsigned)locColor, 4, RL_UNSIGNED_BYTE, true, stride, (int)offsetof(WorldBatchSprite, r));
    rlEnableVertexAttribute((unsigned)locColor);
    rlSetVertexAttributeDivisor((unsigned)locColor, 1);
    rlSetVertexAttribute((unsigned)locKind, 1, RL_UNSIGNED_BYTE, false, stride, (int)offsetof(WorldBatchSprite, kind));
    rlEnableVertexAttribute((unsigned)locKind);
    rlSetVertexAttributeDivisor((unsigned)locKind, 1);
    rlDisableVertexArray();

    worldBatchTexId = atlas.id;
    wbReady = true;
    return true;
}

WorldBatchCapture* worldBatchCapture = NULL;

void WorldBatchPushMoved(const WorldBatchSprite* s, int n, float dx, float dy) {
    while (n > 0) {
        if (worldBatchCount == WORLD_BATCH_CAP) WorldBatchFlush();
        int m = WORLD_BATCH_CAP - worldBatchCount;
        if (m > n) m = n;
        WorldBatchSprite* d = &worldBatchBuf[worldBatchCount];
        for (int i = 0; i < m; i++) {
            d[i] = s[i];
            d[i].x0 += dx; d[i].x1 += dx; d[i].y0 += dy; d[i].y1 += dy;
        }
        worldBatchCount += m; s += m; n -= m;
    }
}

void WorldBatchFlush(void) {
    if (worldBatchCount == 0) return;
    if (worldBatchCapture) {
        WorldBatchCapture* c = worldBatchCapture;
        if (c->count + worldBatchCount > c->cap) {
            int cap = c->cap ? c->cap : 1024;
            while (cap < c->count + worldBatchCount) cap *= 2;
            WorldBatchSprite* nb = realloc(c->buf, (size_t)cap * sizeof(WorldBatchSprite));
            if (!nb) { worldBatchCount = 0; return; }
            c->buf = nb; c->cap = cap;
        }
        memcpy(c->buf + c->count, worldBatchBuf, (size_t)worldBatchCount * sizeof(WorldBatchSprite));
        c->count += worldBatchCount;
        worldBatchCount = 0;
        return;
    }
    rlEnableShader(wbShader);
    Matrix mvp = WbMul(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlSetUniformMatrix(wbLocMvp, mvp);
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    rlSetUniform(wbLocColDiffuse, white, RL_SHADER_UNIFORM_VEC4, 1);
    int slot = 0;
    rlSetUniform(wbLocTexture, &slot, RL_SHADER_UNIFORM_SAMPLER2D, 1);
    float view[3] = {wbOffsetX, wbOffsetY, wbCellPixels > 0.0f ? wbCellPixels : 1.0f};
    rlSetUniform(wbLocView, view, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetUniform(wbLocTime, &wbTime, RL_SHADER_UNIFORM_FLOAT, 1);
    int fx = shaderEffects ? 1 : 0;
    rlSetUniform(wbLocEffects, &fx, RL_SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
    rlEnableTexture(worldBatchTexId);
    rlEnableVertexArray(wbVao);
    rlUpdateVertexBuffer(wbInstVbo, worldBatchBuf, worldBatchCount * (int)sizeof(WorldBatchSprite), 0);
    rlDrawVertexArrayInstanced(0, 6, worldBatchCount);
    rlDisableVertexArray();
    rlDisableTexture();
    rlDisableShader();
    worldBatchCount = 0;
}

void WorldBatchBegin(void) {
    if (!wbReady) return;   // FastSprite/FastRect keep using raylib's batch
    rlDrawRenderBatchActive();   // raylib's pending draws come first
    worldBatchActive = true;
}

void WorldBatchEnd(void) {
    if (!worldBatchActive) return;
    if (rlNavKitPendingVertices() > 0 && !wbWarned) {
        TraceLog(LOG_WARNING, "World batch: a raylib draw call inside WorldBatchBegin/End "
                              "(drawn out of order); use FastSprite/FastRect there");
        wbWarned = true;
    }
    WorldBatchFlush();
    worldBatchActive = false;
}

void WorldBatchSetView(float offsetX, float offsetY, float cellPixels, float time) {
    wbOffsetX = offsetX; wbOffsetY = offsetY; wbCellPixels = cellPixels; wbTime = time;
}

// Recorded sprites kept on the GPU (terrain cache): uploaded once, drawn moved by (dx, dy)
void WorldBatchStaticFree(WorldBatchStatic* st) {
    if (st->vao) rlUnloadVertexArray(st->vao);
    if (st->vbo) rlUnloadVertexBuffer(st->vbo);
    st->vao = st->vbo = 0; st->count = 0;
}

void WorldBatchStaticUpload(WorldBatchStatic* st, const WorldBatchSprite* s, int n) {
    WorldBatchStaticFree(st);   // a fresh buffer: no waiting on draws still using the old one
    if (!wbReady || n <= 0) return;
    st->vao = rlLoadVertexArray();
    rlEnableVertexArray(st->vao);
    rlEnableVertexBuffer(wbCornerVbo);
    rlSetVertexAttribute((unsigned)wbLocCorner, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute((unsigned)wbLocCorner);
    st->vbo = rlLoadVertexBuffer(s, n * (int)sizeof(WorldBatchSprite), false);
    WbSetInstanceAttribs();
    rlDisableVertexArray();
    st->count = n;
}

void WorldBatchStaticDraw(const WorldBatchStatic* st, float dx, float dy) {
    if (!st->vao || st->count == 0) return;
    WorldBatchFlush();   // keep the order with what is queued
    rlEnableShader(wbShader);
    Matrix t = { 1,0,0,dx, 0,1,0,dy, 0,0,1,0, 0,0,0,1 };
    Matrix mvp = WbMul(WbMul(t, rlGetMatrixModelview()), rlGetMatrixProjection());
    rlSetUniformMatrix(wbLocMvp, mvp);
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    rlSetUniform(wbLocColDiffuse, white, RL_SHADER_UNIFORM_VEC4, 1);
    int slot = 0;
    rlSetUniform(wbLocTexture, &slot, RL_SHADER_UNIFORM_SAMPLER2D, 1);
    float view[3] = {0.0f, 0.0f, wbCellPixels > 0.0f ? wbCellPixels : 1.0f};   // recorded at offset 0
    rlSetUniform(wbLocView, view, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetUniform(wbLocTime, &wbTime, RL_SHADER_UNIFORM_FLOAT, 1);
    int fx = shaderEffects ? 1 : 0;
    rlSetUniform(wbLocEffects, &fx, RL_SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
    rlEnableTexture(worldBatchTexId);
    rlEnableVertexArray(st->vao);
    rlDrawVertexArrayInstanced(0, 6, st->count);
    rlDisableVertexArray();
    rlDisableTexture();
    rlDisableShader();
}
