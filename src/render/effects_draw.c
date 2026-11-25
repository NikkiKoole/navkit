// effects_draw.c - Draws src/core/effects.c's effects (blasts, flashes, dust, chunks, sparks).
//
// Blasts, flashes and dust are dithered discs: one quad each through a small shader that works
// on the 8x8 sprite pixel grid (CELL_SIZE / 8 world units per pixel), keeps a pixel when a 4x4
// Bayer threshold (anchored to the world, so it doesn't crawl with the camera) is below the
// disc's density there, and colours it in hard steps. Fading = fewer pixels, never alpha.
// Chunks and sparks are single sprite pixels. With no effects alive nothing is drawn.
#include "../core/effects.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "raylib.h"
#include "raylib/rlgl.h"
#include <math.h>

#define FX_PIXEL (CELL_SIZE / 8.0f)   // world units per sprite pixel

static const char* fxDiscFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform vec2 quadPix;\n"      // quad size in sprite pixels
    "uniform vec2 quadOrigin;\n"   // world sprite-pixel coordinate of the quad's top-left corner
    "uniform vec2 center;\n"       // disc centre, quad-local sprite pixels
    "uniform float radius;\n"      // sprite pixels
    "uniform float density;\n"     // 0..1 at the centre
    "uniform float falloff;\n"     // how much the density drops toward the rim (0..1)
    "uniform float ragged;\n"      // how uneven the rim is (0..1)
    "uniform float innerFrac;\n"   // inside this fraction of the radius: the inner colour
    "uniform vec3 innerCol;\n"
    "uniform vec3 outerCol;\n"
    "uniform float seed;\n"
    "uniform float noiseAmt;\n"   // clumping: density +- this, from blobs about 3 pixels across
    "uniform float lump;\n"       // how far the outline bulges in and out (0 = round)
    "uniform float ring;\n"       // > 0: only the outer band this thick (fraction of the radius)
    "uniform float time;\n"       // the effect's age: the noise drifts with it (boils)
    "const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,\n"
    "                                  3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233)) + seed) * 43758.5453); }\n"
    "float vnoise(vec2 p) {\n"     // value noise: smooth blobs, so the dither clumps instead of a checkerboard
    "    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);\n"
    "}\n"
    "void main() {\n"
    "    vec2 local = floor(fragTexCoord * quadPix);\n"
    "    vec2 world = local + quadOrigin;\n"
    "    vec2 dir = local + 0.5 - center;\n"
    "    float len = length(dir);\n"
    "    vec2 unit = len > 0.0 ? dir / len : vec2(1.0, 0.0);\n"
    "    float outline = 1.0 + (vnoise(unit * 1.7 + vec2(seed * 0.13, time * 0.9)) - 0.5) * 2.0 * lump;\n"
    "    float r = len / max(radius * outline, 0.5);\n"
    "    float rim = 1.0 + (hash(floor(world / 2.0)) - 0.5) * ragged;\n"   // in 2-pixel blocks: chunky
    "    if (r > rim) discard;\n"
    "    if (ring > 0.0 && r < 1.0 - ring) discard;\n"
    "    vec2 drift = vec2(time * 1.7, -time * 2.6);\n"   // the clumps churn as it ages
    "    float dens = density * (1.0 - falloff * r * r) + (vnoise(world / 3.0 + drift) - 0.5) * 2.0 * noiseAmt;\n"
    "    ivec2 b = ivec2(mod(world, 4.0));\n"
    "    if ((bayer[b.x + b.y * 4] + 0.5) / 16.0 >= dens) discard;\n"
    "    float edge = innerFrac + (vnoise(world / 2.0 + 17.0) - 0.5) * 0.3;\n"
    "    finalColor = vec4(r < edge ? innerCol : outerCol, 1.0);\n"
    "}\n";

static Shader fxShader;
static Texture2D fxWhite;
static bool fxReady = false, fxFailed = false;
static int locQuadPix, locQuadOrigin, locCenter, locRadius, locDensity, locFalloff, locRagged,
           locInnerFrac, locInnerCol, locOuterCol, locSeed, locNoise, locLump, locRing, locTime;

static bool FxInit(void) {
    if (fxReady) return true;
    if (fxFailed) return false;
    fxShader = LoadShaderFromMemory(NULL, fxDiscFS);
    if (fxShader.id == (unsigned int)rlGetShaderIdDefault()) { fxFailed = true; TraceLog(LOG_WARNING, "Effects: disc shader failed"); return false; }
    Image img = GenImageColor(1, 1, WHITE);
    fxWhite = LoadTextureFromImage(img);
    UnloadImage(img);
    locQuadPix = GetShaderLocation(fxShader, "quadPix");
    locQuadOrigin = GetShaderLocation(fxShader, "quadOrigin");
    locCenter = GetShaderLocation(fxShader, "center");
    locRadius = GetShaderLocation(fxShader, "radius");
    locDensity = GetShaderLocation(fxShader, "density");
    locFalloff = GetShaderLocation(fxShader, "falloff");
    locRagged = GetShaderLocation(fxShader, "ragged");
    locInnerFrac = GetShaderLocation(fxShader, "innerFrac");
    locInnerCol = GetShaderLocation(fxShader, "innerCol");
    locOuterCol = GetShaderLocation(fxShader, "outerCol");
    locSeed = GetShaderLocation(fxShader, "seed");
    locNoise = GetShaderLocation(fxShader, "noiseAmt");
    locLump = GetShaderLocation(fxShader, "lump");
    locRing = GetShaderLocation(fxShader, "ring");
    locTime = GetShaderLocation(fxShader, "time");
    fxReady = true;
    return true;
}

static void Col3(float* out, int r, int g, int b) { out[0] = r / 255.0f; out[1] = g / 255.0f; out[2] = b / 255.0f; }

// A dithered disc centred at (cx, cy) world units, radius in world units
typedef struct { float lump, ring, time; } DiscShape;   // outline bulge, ring band, age

static void DrawDisc(float cx, float cy, float radius, float density, float falloff, float ragged,
                     float noise, float innerFrac, const float* inner, const float* outer, uint32_t seed,
                     DiscShape shape) {
    if (density <= 0.0f || radius <= 0.0f) return;
    float reach = radius * (1.0f + 0.5f * ragged) * (1.0f + shape.lump) + FX_PIXEL;
    float x0 = floorf((cx - reach) / FX_PIXEL), y0 = floorf((cy - reach) / FX_PIXEL);
    float x1 = ceilf((cx + reach) / FX_PIXEL), y1 = ceilf((cy + reach) / FX_PIXEL);
    float pix = FX_PIXEL * zoom;
    Rectangle dest = { offset.x + x0 * pix, offset.y + y0 * pix, (x1 - x0) * pix, (y1 - y0) * pix };
    if (dest.x > GetScreenWidth() || dest.y > GetScreenHeight() || dest.x + dest.width < 0 || dest.y + dest.height < 0) return;
    float quadPix[2] = { x1 - x0, y1 - y0 }, quadOrigin[2] = { x0, y0 };
    float center[2] = { cx / FX_PIXEL - x0, cy / FX_PIXEL - y0 };
    float rad = radius / FX_PIXEL, s = (float)(seed % 1000u) * 0.37f;
    SetShaderValue(fxShader, locQuadPix, quadPix, SHADER_UNIFORM_VEC2);
    SetShaderValue(fxShader, locQuadOrigin, quadOrigin, SHADER_UNIFORM_VEC2);
    SetShaderValue(fxShader, locCenter, center, SHADER_UNIFORM_VEC2);
    SetShaderValue(fxShader, locRadius, &rad, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locDensity, &density, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locFalloff, &falloff, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locRagged, &ragged, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locInnerFrac, &innerFrac, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locInnerCol, inner, SHADER_UNIFORM_VEC3);
    SetShaderValue(fxShader, locOuterCol, outer, SHADER_UNIFORM_VEC3);
    SetShaderValue(fxShader, locSeed, &s, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locNoise, &noise, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locLump, &shape.lump, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locRing, &shape.ring, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fxShader, locTime, &shape.time, SHADER_UNIFORM_FLOAT);
    DrawTexturePro(fxWhite, (Rectangle){ 0, 0, 1, 1 }, dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    rlDrawRenderBatchActive();   // the next disc sets other uniforms
}

// A fireball's colours over its life: hard steps, hotter inside
static void BlastColours(float t, float* inner, float* outer) {
    if (t < 0.12f)      { Col3(inner, 255, 255, 240); Col3(outer, 255, 230, 110); }
    else if (t < 0.3f)  { Col3(inner, 255, 230, 110); Col3(outer, 255, 150, 40); }
    else if (t < 0.5f)  { Col3(inner, 255, 150, 40);  Col3(outer, 200, 70, 30); }
    else if (t < 0.7f)  { Col3(inner, 200, 70, 30);   Col3(outer, 110, 100, 95); }
    else                { Col3(inner, 110, 100, 95);  Col3(outer, 80, 76, 72); }
}

// The viewed level, the one below (blast craters) and the one above (dust over a fallen slab)
static bool FxVisible(int z) { return z <= currentViewZ + 1 && z >= currentViewZ - 1; }

void DrawEffects(void) {
    if (fxCount == 0 || !FxInit()) return;
    float inner[3], outer[3];
    // Discs: flashes under the rest, then dust, fireballs, and the smoke over them
    BeginShaderMode(fxShader);
    for (int pass = 0; pass < 4; pass++) {
        FxKind want = pass == 0 ? FX_FLASH : pass == 1 ? FX_DUST : pass == 2 ? FX_BLAST : FX_SMOKE;
        for (int i = 0; i < fxCount; i++) {
            const Fx* f = &fx[i];
            if (f->kind != want || f->age < 0.0f || !FxVisible(f->z)) continue;
            float t = f->age / f->life;
            if (f->kind == FX_BLAST) {
                float grow = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                float dens = t < 0.3f ? 1.0f : 1.0f - (t - 0.3f) / 0.7f;
                BlastColours(t, inner, outer);
                DrawDisc(f->x, f->y, f->radius * (0.3f + 0.7f * grow), dens, 0.45f, 0.35f, 0.1f + 0.35f * t, 0.55f, inner, outer, f->seed,
                         (DiscShape){ 0.22f, 0.0f, f->age });
            } else if (f->kind == FX_SMOKE) {
                // Thick at first, thinning late; swells as it rises; a lighter edge
                float dens = 0.62f * (1.0f - t * t);
                Col3(inner, f->r, f->g, f->b);
                Col3(outer, f->r + 22, f->g + 22, f->b + 22);
                DrawDisc(f->x, f->y, f->radius * (1.0f + 1.4f * t), dens, 0.55f, 0.5f, 0.3f, 0.6f, inner, outer, f->seed,
                         (DiscShape){ 0.25f, 0.0f, f->age });
            } else if (f->kind == FX_FLASH) {
                Col3(inner, 255, 255, 235);
                DrawDisc(f->x, f->y, f->radius, 0.6f * (1.0f - t), 1.0f, 0.3f, 0.2f, 1.0f, inner, inner, f->seed,
                         (DiscShape){ 0.2f, 0.0f, f->age });   // a halo, thin at the rim
            } else {
                Col3(outer, f->r, f->g, f->b);
                Col3(inner, f->r + (255 - f->r) / 5, f->g + (255 - f->g) / 5, f->b + (255 - f->b) / 5);   // a shade lighter
                DrawDisc(f->x, f->y, f->radius * (1.0f + 0.8f * t), 0.7f * (1.0f - t), 0.6f, 0.6f, 0.25f, 0.4f, inner, outer, f->seed,
                         (DiscShape){ 0.25f, 0.0f, f->age });
            }
        }
    }
    EndShaderMode();
    // Chunks and sparks: sprite pixels
    float pix = FX_PIXEL * zoom;
    for (int i = 0; i < fxCount; i++) {
        const Fx* f = &fx[i];
        if ((f->kind != FX_CHUNK && f->kind != FX_SPARK) || !FxVisible(f->z)) continue;
        float px = floorf(f->x / FX_PIXEL), py = floorf((f->y - f->h) / FX_PIXEL);
        float size = f->kind == FX_CHUNK ? f->radius : 1.0f;
        DrawRectangleRec((Rectangle){ offset.x + px * pix, offset.y + py * pix, size * pix, size * pix },
                         (Color){ f->r, f->g, f->b, 255 });
    }
}

// ---------------------------------------------------------------------------------------------
// Gas clouds: the smoke and steam simulations drawn the same way. The caller fills one texel per
// visible cell (r = density on the viewed level, g = the strongest below it, 0..255); the texture
// is sampled smoothly so neighbouring cells blend into one cloud, then dithered on the sprite
// pixel grid with drifting noise (boils with FxClock).

static const char* fxGasFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 quadPix;\n"
    "uniform vec2 quadOrigin;\n"
    "uniform float time;\n"
    "uniform vec3 denseCol;\n"     // thick parts on the viewed level
    "uniform vec3 thinCol;\n"      // thin parts on the viewed level
    "uniform vec3 deepCol;\n"      // seen below the viewed level
    "const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,\n"
    "                                  3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }\n"
    "float vnoise(vec2 p) {\n"
    "    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);\n"
    "}\n"
    "void main() {\n"
    "    vec2 local = floor(fragTexCoord * quadPix);\n"
    "    vec2 world = local + quadOrigin;\n"
    "    vec4 d = texture(texture0, (local + 0.5) / quadPix);\n"
    "    if (d.r + d.g < 0.004) discard;\n"
    "    float n = (vnoise(world / 4.0 + vec2(time * 1.1, -time * 1.8)) - 0.5) * 0.7\n"
    "            + (vnoise(world / 9.0 - vec2(time * 0.4, time * 0.6)) - 0.5) * 0.5;\n"
    "    ivec2 b = ivec2(mod(world, 4.0));\n"
    "    float th = (bayer[b.x + b.y * 4] + 0.5) / 16.0;\n"
    "    float cur = d.r * 1.15 + n;\n"
    "    if (cur > th) { finalColor = vec4(cur > 0.75 ? denseCol : thinCol, 1.0); return; }\n"
    "    if (d.g * 0.8 + n > th) { finalColor = vec4(deepCol, 1.0); return; }\n"
    "    discard;\n"
    "}\n";

static Shader gasShader;
static bool gasReady = false, gasFailed = false;
static int gasLocQuadPix, gasLocQuadOrigin, gasLocTime, gasLocDense, gasLocThin, gasLocDeep;
static Texture2D gasTex[4];
static int gasTexW[4], gasTexH[4];

static bool GasInit(void) {
    if (gasReady) return true;
    if (gasFailed) return false;
    gasShader = LoadShaderFromMemory(NULL, fxGasFS);
    if (gasShader.id == (unsigned int)rlGetShaderIdDefault()) { gasFailed = true; TraceLog(LOG_WARNING, "Effects: gas shader failed"); return false; }
    gasLocQuadPix = GetShaderLocation(gasShader, "quadPix");
    gasLocQuadOrigin = GetShaderLocation(gasShader, "quadOrigin");
    gasLocTime = GetShaderLocation(gasShader, "time");
    gasLocDense = GetShaderLocation(gasShader, "denseCol");
    gasLocThin = GetShaderLocation(gasShader, "thinCol");
    gasLocDeep = GetShaderLocation(gasShader, "deepCol");
    gasReady = true;
    return true;
}

// Can the clouds be drawn (shader built, or not tried yet)? If not, smoke and steam draw the old way
bool GasCloudsAvailable(void) { return !gasFailed; }

// rgba: w * h texels (r = viewed level, g = below), cells minX.. / minY.. ; slot 0 smoke, 1 steam, 2 petrol vapour, 3 firedamp.
// Returns false if it can't draw (no shader): the caller draws the old way.
bool DrawGasDithered(int slot, int minX, int minY, int w, int h, const unsigned char* rgba,
                     Color dense, Color thin, Color deep) {
    if (w <= 0 || h <= 0 || slot < 0 || slot > 3 || !GasInit()) return false;
    if (gasTexW[slot] != w || gasTexH[slot] != h) {
        if (gasTexW[slot] > 0) UnloadTexture(gasTex[slot]);
        Image img = GenImageColor(w, h, BLANK);
        gasTex[slot] = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(gasTex[slot], TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(gasTex[slot], TEXTURE_WRAP_CLAMP);
        gasTexW[slot] = w; gasTexH[slot] = h;
    }
    UpdateTexture(gasTex[slot], rgba);
    float pix = FX_PIXEL * zoom, cell = CELL_SIZE * zoom;
    float quadPix[2] = { w * 8.0f, h * 8.0f }, quadOrigin[2] = { minX * 8.0f, minY * 8.0f };
    float t = FxClock(), dc[3], tc[3], ec[3];
    Col3(dc, dense.r, dense.g, dense.b); Col3(tc, thin.r, thin.g, thin.b); Col3(ec, deep.r, deep.g, deep.b);
    (void)pix;
    BeginShaderMode(gasShader);
    SetShaderValue(gasShader, gasLocQuadPix, quadPix, SHADER_UNIFORM_VEC2);
    SetShaderValue(gasShader, gasLocQuadOrigin, quadOrigin, SHADER_UNIFORM_VEC2);
    SetShaderValue(gasShader, gasLocTime, &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(gasShader, gasLocDense, dc, SHADER_UNIFORM_VEC3);
    SetShaderValue(gasShader, gasLocThin, tc, SHADER_UNIFORM_VEC3);
    SetShaderValue(gasShader, gasLocDeep, ec, SHADER_UNIFORM_VEC3);
    DrawTexturePro(gasTex[slot], (Rectangle){ 0, 0, (float)w, (float)h },
                   (Rectangle){ offset.x + minX * cell, offset.y + minY * cell, w * cell, h * cell },
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    return true;
}
