// World batch: see world_batch.h
#include "world_batch.h"
#include "raylib/rlgl.h"
#include <stddef.h>

int rlNavKitPendingVertices(void);   // vendor/raylib/rlgl.h (NavKit patch)

bool worldBatchActive = false;
unsigned int worldBatchTexId = 0;
float worldBatchWhiteU = 0.0f, worldBatchWhiteV = 0.0f;
WorldBatchSprite worldBatchBuf[WORLD_BATCH_CAP];
int worldBatchCount = 0;

static unsigned int wbShader, wbVao, wbCornerVbo, wbInstVbo;
static int wbLocMvp = -1, wbLocColDiffuse = -1, wbLocTexture = -1;
static bool wbReady = false, wbWarned = false;

// raylib's default shader (rlgl.h, GL 3.3), except that the quad's corners come from the
// instance: the corner picks x0 or x1 and u0 or u1, the values raylib would have put in the
// vertices. Same arithmetic in the fragment shader, so the pixels are the same.
static const char* wbVS =
    "#version 330\n"
    "in vec2 corner;\n"            // (0 or 1, 0 or 1)
    "in vec4 rect;\n"              // x0 y0 x1 y1
    "in vec4 uvRect;\n"            // u0 v0 u1 v1
    "in vec4 color;\n"
    "out vec2 fragTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    vec2 p = vec2(corner.x < 0.5 ? rect.x : rect.z, corner.y < 0.5 ? rect.y : rect.w);\n"
    "    fragTexCoord = vec2(corner.x < 0.5 ? uvRect.x : uvRect.z, corner.y < 0.5 ? uvRect.y : uvRect.w);\n"
    "    fragColor = color;\n"
    "    gl_Position = mvp*vec4(p, -0.5, 1.0);\n"
    "}\n";
static const char* wbFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    finalColor = texelColor*colDiffuse*fragColor;\n"
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
    int locCorner = rlGetLocationAttrib(wbShader, "corner");
    int locRect = rlGetLocationAttrib(wbShader, "rect");
    int locUv = rlGetLocationAttrib(wbShader, "uvRect");
    int locColor = rlGetLocationAttrib(wbShader, "color");
    if (locCorner < 0 || locRect < 0 || locUv < 0 || locColor < 0) return false;

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
    rlDisableVertexArray();

    worldBatchTexId = atlas.id;
    wbReady = true;
    return true;
}

void WorldBatchFlush(void) {
    if (worldBatchCount == 0) return;
    rlEnableShader(wbShader);
    Matrix mvp = WbMul(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlSetUniformMatrix(wbLocMvp, mvp);
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    rlSetUniform(wbLocColDiffuse, white, RL_SHADER_UNIFORM_VEC4, 1);
    int slot = 0;
    rlSetUniform(wbLocTexture, &slot, RL_SHADER_UNIFORM_SAMPLER2D, 1);
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
