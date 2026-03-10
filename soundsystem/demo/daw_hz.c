// PixelSynth DAW - Horizontal Layout
// Layout: narrow sidebar (left) + workspace (top) + params (bottom)
//
// Build: same as daw.c (add target to Makefile)
//
// Sidebar (74px): play/stop, 8 patch slots, drum mutes
// Workspace (full width, top): Sequencer / Piano Roll / Song [F1/F2/F3]
// Params (full width, bottom): Patch / Drums / Bus FX / Master FX / Tape [1-5]

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>
#include "../engines/synth.h"
// Undefine synth.h macros that conflict with our local DAW state
#undef masterVolume
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode
#include "../engines/synth_patch.h"
#include "../engines/instrument_presets.h"

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 800

// ============================================================================
// LAYOUT
// ============================================================================

#define SIDEBAR_W 74
#define TRANSPORT_H 36
#define TAB_H 24
#define SPLIT_Y 420

#define CONTENT_X SIDEBAR_W
#define CONTENT_W (SCREEN_WIDTH - SIDEBAR_W)

// Workspace
#define WORK_TAB_Y TRANSPORT_H
#define WORK_Y (TRANSPORT_H + TAB_H)
#define WORK_H (SPLIT_Y - WORK_Y)

// Params
#define PARAM_TAB_Y SPLIT_Y
#define PARAM_Y (SPLIT_Y + TAB_H)
#define PARAM_H (SCREEN_HEIGHT - PARAM_Y)

// ============================================================================
// TABS
// ============================================================================

typedef enum { WORK_SEQ, WORK_PIANO, WORK_SONG, WORK_COUNT } WorkTab;
static WorkTab workTab = WORK_SEQ;
static const char* workTabNames[] = {"Sequencer", "Piano Roll", "Song"};
static const int workTabKeys[] = {KEY_F1, KEY_F2, KEY_F3};

typedef enum { PARAM_PATCH, PARAM_DRUMS, PARAM_BUS, PARAM_MASTER, PARAM_TAPE, PARAM_COUNT } ParamTab;
static ParamTab paramTab = PARAM_PATCH;
static const char* paramTabNames[] = {"Patch", "Drums", "Bus FX", "Master FX", "Tape"};
static const int paramTabKeys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE};

// Patch sub-tabs: Main (all params) | Adv (algorithm modes)
// All patch params on one page (no sub-tabs)

// Name arrays for new params
static const char* filterTypeNames[] = {"LP", "HP", "BP", "1pLP", "1pHP", "ResoBP"};
static const char* noiseModeNames[] = {"Mix", "Replace", "PerBurst"};
static const char* oscMixModeNames[] = {"Weighted", "Additive"};
static const char* noiseTypeNames[] = {"LFSR", "TimeHash"};

// ============================================================================
// ALL STATE
// ============================================================================

// --- Transport ---
static bool playing = false;
static float bpm = 120.0f;
static int currentPattern = 0;

// --- Groove / Dilla ---
static int grooveKickNudge = 0, grooveSnareDelay = 0, grooveHatNudge = 0, grooveClapDelay = 0;
static int grooveSwing = 0, grooveJitter = 0;

// --- Scenes / Crossfader ---
static bool crossfaderEnabled = false;
static float crossfaderPos = 0.5f;
static int sceneA = 0, sceneB = 1;
static int sceneCount = 8;

// --- Synth Patch ---
#define NUM_PATCHES 8

static const char* waveNames[] = {"Square", "Saw", "Triangle", "Noise", "SCW",
    "Voice", "Pluck", "Additive", "Mallet", "Granular", "FM", "PD", "Membrane", "Bird"};
static const char* vowelNames[] = {"A", "E", "I", "O", "U"};
static const char* additivePresetNames[] = {"Organ", "Bell", "Choir", "Brass", "Strings"};
static const char* malletPresetNames[] = {"Marimba", "Vibes", "Xylo", "Glock", "Tubular"};
static const char* pdWaveNames[] = {"Saw", "Square", "Pulse", "SyncSaw", "SyncSq"};
static const char* membranePresetNames[] = {"Tabla", "Conga", "Bongo", "Djembe", "Tom"};
static const char* birdTypeNames[] = {"Sparrow", "Robin", "Wren", "Finch", "Nightingale"};
static const char* arpModeNames[] = {"Up", "Down", "UpDown", "Random"};
static const char* arpChordNames[] = {"Octave", "Major", "Minor", "Dom7", "Min7", "Sus4", "Power"};
static const char* arpRateNames[] = {"1/4", "1/8", "1/16", "1/32", "Free"};
// rootNoteNames and scaleNames provided by synth.h
static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};
static const char* lfoSyncNames[] = {"Off", "4bar", "2bar", "1bar", "1/2", "1/4", "1/8", "1/16"};

static int selectedPatch = 0;
static int selectedDrum = 0; // 0=Kick, 1=Snare, 2=HiHat, 3=Clap
static SynthPatch patches[NUM_PATCHES];
// Engine-level settings (not per-patch in synth engine) // TODO: find proper home
static bool daw_scaleLockEnabled = false;
static int daw_scaleRoot = 0, daw_scaleType = 0;
static bool daw_voiceRandomVowel = false;
static bool patchesInit = false;
static float daw_masterVolume = 0.8f;

// --- Preset tracking ---
static int patchPresetIndex[NUM_PATCHES] = {-1,-1,-1,-1,-1,-1,-1,-1};
static SynthPatch patchPresetSnapshot[NUM_PATCHES];
static bool presetPickerOpen = false;
static bool presetPickerJustClosed = false;
static bool presetsInitialized = false;

static void initPatches(void) {
    if (patchesInit) return;
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    for (int i = 0; i < NUM_PATCHES; i++) patches[i] = createDefaultPatch(WAVE_SAW);
    // Drum tracks 0-3: load 808 presets
    patches[0] = instrumentPresets[24].patch; snprintf(patches[0].p_name, 32, "Kick");
    patches[1] = instrumentPresets[25].patch; snprintf(patches[1].p_name, 32, "Snare");
    patches[2] = instrumentPresets[27].patch; snprintf(patches[2].p_name, 32, "CH");
    patches[3] = instrumentPresets[26].patch; snprintf(patches[3].p_name, 32, "Clap");
    patchPresetIndex[0] = 24; patchPresetSnapshot[0] = patches[0];
    patchPresetIndex[1] = 25; patchPresetSnapshot[1] = patches[1];
    patchPresetIndex[2] = 27; patchPresetSnapshot[2] = patches[2];
    patchPresetIndex[3] = 26; patchPresetSnapshot[3] = patches[3];
    // Melody tracks 4-6
    snprintf(patches[4].p_name, 32, "Bass");       patches[4].p_waveType = WAVE_SQUARE;
    snprintf(patches[5].p_name, 32, "Lead");        patches[5].p_waveType = WAVE_SAW;
    snprintf(patches[6].p_name, 32, "Chord");       patches[6].p_waveType = WAVE_ADDITIVE;
    patches[4].p_monoMode = true; patches[4].p_filterCutoff = 0.4f;
    patches[5].p_unisonCount = 2; patches[5].p_vibratoDepth = 0.3f;
    patches[6].p_attack = 0.05f; patches[6].p_release = 0.8f;
    // Extra patches 7
    snprintf(patches[7].p_name, 32, "Bird");        patches[7].p_waveType = WAVE_BIRD;
    patches[7].p_birdType = 4;
    patchesInit = true;
}

// (preset tracking vars moved above initPatches)

static bool isPatchDirty(int patchIdx) {
    if (patchPresetIndex[patchIdx] < 0) return false;
    return memcmp(&patches[patchIdx], &patchPresetSnapshot[patchIdx], sizeof(SynthPatch)) != 0;
}

static void loadPresetIntoPatch(int patchIdx, int presetIdx) {
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    // Copy preset params but keep the patch name
    char nameBak[32];
    memcpy(nameBak, patches[patchIdx].p_name, 32);
    patches[patchIdx] = instrumentPresets[presetIdx].patch;
    memcpy(patches[patchIdx].p_name, nameBak, 32);
    patchPresetIndex[patchIdx] = presetIdx;
    patchPresetSnapshot[patchIdx] = patches[patchIdx];
}

// --- Drums ---
static float drumVolume = 0.6f;
static float kickPitch = 50.0f, kickDecay = 0.5f, kickPunchPitch = 150.0f;
static float kickClick = 0.3f, kickTone = 0.5f;
static float snarePitch = 180.0f, snareDecay = 0.2f, snareSnappy = 0.6f, snareTone = 0.5f;
static float clapDecay = 0.3f, clapTone = 0.5f, clapSpread = 0.015f;
static float hhDecayClosed = 0.05f, hhDecayOpen = 0.4f, hhTone = 0.5f;
static float tomPitch = 1.0f, tomDecay = 0.3f, tomPunchDecay = 0.05f;
static float rimPitch = 1500.0f, rimDecay = 0.03f;
static float cowbellPitch = 600.0f, cowbellDecay = 0.3f;
static float clavePitch = 2500.0f, claveDecay = 0.03f;
static float maracasDecay = 0.05f, maracasTone = 0.5f;
static float cr78KickPitch = 80.0f, cr78KickDecay = 0.3f, cr78KickResonance = 0.8f;
static float cr78SnarePitch = 200.0f, cr78SnareDecay = 0.15f, cr78SnareSnappy = 0.5f;
static float cr78HHDecay = 0.05f, cr78HHTone = 0.5f;
static float cr78MetalPitch = 800.0f, cr78MetalDecay = 0.2f;

// Sidechain
static bool sidechainOn = false;
static int sidechainSource = 0, sidechainTarget = 0;
static float sidechainDepth = 0.8f, sidechainAttack = 0.005f, sidechainRelease = 0.15f;
static const char* sidechainSourceNames[] = {"Kick", "Snare", "Clap", "HiHat", "AllDrm"};
static const char* sidechainTargetNames[] = {"Bass", "Lead", "Chord", "AllSyn"};

// Sequencer grid (32 steps max, toggle between 16/32)
static bool drumSteps[4][32] = {{0}};
static bool drumStepsInit = false;
static int seqStepCount = 16; // 16 or 32

// --- Master Effects ---
static bool distOn = false;
static float distDrive = 2.0f, distTone = 0.7f, distMix = 0.5f;
static bool delayOn = false;
static float delayTime = 0.3f, delayFeedback = 0.4f, delayTone = 0.5f, delayMix = 0.3f;
static bool tapeOn = false;
static float tapeSaturation = 0.3f, tapeWow = 0.1f, tapeFlutter = 0.1f, tapeHiss = 0.05f;
static bool crushOn = false;
static float crushBits = 8.0f, crushRate = 4.0f, crushMix = 0.5f;
static bool chorusOn = false;
static float chorusRate = 1.0f, chorusDepth = 0.3f, chorusMix = 0.3f;
static bool reverbOn = false;
static float reverbSize = 0.5f, reverbDamping = 0.5f, reverbPreDelay = 0.02f, reverbMix = 0.3f;

// --- Tape FX ---
static bool dubLoopEnabled = false;
static float dubHeadTime = 0.5f, dubFeedback = 0.6f, dubMix = 0.5f;
static int dubInputSource = 0;
static bool dubPreReverb = false;
static float dubSaturation = 0.3f, dubToneHigh = 0.7f, dubNoise = 0.05f, dubDegradeRate = 0.1f;
static float dubWow = 0.1f, dubFlutter = 0.1f, dubDrift = 0.05f, dubSpeedTarget = 1.0f;
static float rewindTime = 1.5f;
static int rewindCurve = 0;
static float rewindMinSpeed = 0.2f, rewindVinyl = 0.3f, rewindWobble = 0.2f, rewindFilter = 0.5f;
static bool isRewinding = false;
static const char* rewindCurveNames[] = {"Linear", "Expo", "S-Curve"};
static const char* dubInputNames[] = {"All", "Drums", "Synth", "Manual"};

// --- Mixer (7 buses) ---
static const char* busNames[] = {"Kick", "Snare", "HiHat", "Clap", "Bass", "Lead", "Chord"};
static float busVolumes[7] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
static float busPan[7] = {0};
static float busReverbSend[7] = {0};
static bool busMute[7] = {false};
static bool busSolo[7] = {false};
static bool busFilterOn[7] = {false};
static float busFilterCut[7] = {1,1,1,1,1,1,1};
static float busFilterRes[7] = {0};
static int busFilterType[7] = {0};
static bool busDistOn[7] = {false};
static float busDistDrive[7] = {2,2,2,2,2,2,2};
static float busDistMix[7] = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f};
static bool busDelayOn[7] = {false};
static bool busDelaySync[7] = {false};
static int busDelaySyncDiv[7] = {2};
static float busDelayTime[7] = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f};
static float busDelayFB[7] = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f};
static float busDelayMix[7] = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f};
static float masterVol = 0.8f;
static int selectedBus = -1;
static const char* busFilterTypeNames[] = {"LP", "HP", "BP"};
static const char* delaySyncNames[] = {"1/16", "1/8", "1/4", "1/2", "1bar"};

// --- Song ---
static int chainLength = 0;
static int chain[64] = {0};

// --- Detail context ---
typedef enum { DETAIL_NONE, DETAIL_STEP, DETAIL_BUS_FX } DetailContext;
static DetailContext detailCtx = DETAIL_NONE;
static int detailStep = -1, detailTrack = -1;

// Polyrhythmic track lengths
static int drumTrackLength[4] = {16, 16, 16, 16};
static int melodyTrackLength[3] = {16, 16, 16};

// Split keyboard
static bool splitEnabled = false;
static int splitPoint = 60;
static int splitLeftPatch = 1, splitRightPatch = 2;
static int splitLeftOctave = 0, splitRightOctave = 0;

// Sidebar scroll (not needed much but just in case)
static float paramScroll = 0;

// ============================================================================
// P-LOCK BADGE
// ============================================================================

static void plockDot(float x, float y) {
    DrawCircle((int)(x + 3), (int)(y + 5), 2.5f, (Color){220, 130, 50, 180});
}

// P-lockable float: draws orange dot before the control
static bool ui_col_float_p(UIColumn *c, const char *label, float *val, float speed, float mn, float mx) {
    plockDot(c->x - 8, c->y);
    return ui_col_float(c, label, val, speed, mn, mx);
}

// Section-level non-default indicator: colored left stripe + subtle bg
// active=true means at least one param in this section differs from default
static void sectionHighlight(float x, float y, float w, float h, bool active) {
    if (active) {
        DrawRectangle((int)x, (int)y, 2, (int)h, ORANGE);
    }
    (void)w;
}

// ============================================================================
// BESPOKE WIDGETS (same as daw.c)
// ============================================================================

static void drawWaveThumb(float x, float y, float w, float h, int waveType, bool selected, bool hovered) {
    Color bg = selected ? (Color){50, 65, 80, 255} : (hovered ? (Color){42, 44, 52, 255} : (Color){30, 31, 38, 255});
    DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);
    if (selected) DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, ORANGE);
    else DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){48, 48, 58, 255});

    float cx = x + 2, cy = y + 2, cw = w - 4, ch = h - 4;
    float mid = cy + ch * 0.5f;
    Color lineCol = selected ? WHITE : (Color){120, 130, 150, 255};
    int steps = (int)cw;

    for (int i = 0; i < steps - 1; i++) {
        float t0 = (float)i / (float)steps;
        float t1 = (float)(i + 1) / (float)steps;
        float v0 = 0, v1 = 0;
        switch (waveType) {
            case 0: v0 = t0 < 0.5f ? 1.0f : -1.0f; v1 = t1 < 0.5f ? 1.0f : -1.0f; break;
            case 1: v0 = 1.0f - 2.0f * t0; v1 = 1.0f - 2.0f * t1; break;
            case 2: v0 = t0<0.5f?(4*t0-1):(3-4*t0); v1 = t1<0.5f?(4*t1-1):(3-4*t1); break;
            case 3: v0 = ((float)((i*7+13)%17))/8.5f-1; v1 = ((float)(((i+1)*7+13)%17))/8.5f-1; break;
            case 4: v0 = sinf(t0*6.28f)*0.6f+sinf(t0*12.56f)*0.3f; v1 = sinf(t1*6.28f)*0.6f+sinf(t1*12.56f)*0.3f; break;
            case 5: v0 = sinf(t0*6.28f)*(1-0.5f*sinf(t0*18.84f)); v1 = sinf(t1*6.28f)*(1-0.5f*sinf(t1*18.84f)); break;
            case 6: v0 = sinf(t0*25)*expf(-t0*3); v1 = sinf(t1*25)*expf(-t1*3); break;  // Pluck (decay was wrong in original: used (1-t))
            case 7: v0 = sinf(t0*6.28f)*0.5f+sinf(t0*12.56f)*0.3f+sinf(t0*18.84f)*0.2f; v1 = sinf(t1*6.28f)*0.5f+sinf(t1*12.56f)*0.3f+sinf(t1*18.84f)*0.2f; break;
            case 8: v0 = sinf(t0*12.56f)*expf(-t0*3); v1 = sinf(t1*12.56f)*expf(-t1*3); break;
            case 9: v0 = sinf(t0*31.4f)*(((int)(t0*6)%2)?0.8f:0.3f); v1 = sinf(t1*31.4f)*(((int)(t1*6)%2)?0.8f:0.3f); break;
            case 10: v0 = sinf(t0*6.28f+2*sinf(t0*12.56f)); v1 = sinf(t1*6.28f+2*sinf(t1*12.56f)); break;
            case 11: { float p0=t0<0.5f?t0*t0*2:0.5f+(t0-0.5f)*(2-t0*2); float p1=t1<0.5f?t1*t1*2:0.5f+(t1-0.5f)*(2-t1*2); v0=sinf(p0*6.28f); v1=sinf(p1*6.28f); } break;
            case 12: v0=(sinf(t0*6.28f)*0.5f+sinf(t0*9.8f)*0.3f+sinf(t0*15.2f)*0.2f)*expf(-t0*2); v1=(sinf(t1*6.28f)*0.5f+sinf(t1*9.8f)*0.3f+sinf(t1*15.2f)*0.2f)*expf(-t1*2); break;
            case 13: v0=sinf(t0*6.28f*(1+t0*3))*(1-t0*0.5f); v1=sinf(t1*6.28f*(1+t1*3))*(1-t1*0.5f); break;
            default: v0 = sinf(t0*6.28f); v1 = sinf(t1*6.28f);
        }
        DrawLine((int)(cx+i), (int)(mid-v0*ch*0.4f), (int)(cx+i+1), (int)(mid-v1*ch*0.4f), lineCol);
    }
}

static float drawWaveSelector(float x, float y, float w, int* wave) {
    int waveCount = 5, engineCount1 = 5, engineCount2 = 4;
    float thumbH = 20;
    Vector2 mouse = GetMousePosition();
    float totalH = 0;

    float thumbW = (w - (waveCount - 1) * 2) / waveCount;
    for (int i = 0; i < waveCount; i++) {
        float tx = x + i * (thumbW + 2), ty = y;
        bool sel = (i == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, thumbW, thumbH});
        drawWaveThumb(tx, ty, thumbW, thumbH, i, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = i; ui_consume_click(); }
    }
    totalH += thumbH + 2;

    DrawTextShadow("Engines:", (int)x, (int)(y + totalH), 9, (Color){70, 70, 85, 255});
    totalH += 12;

    float eW = (w - (engineCount1 - 1) * 2) / engineCount1;
    for (int i = 0; i < engineCount1; i++) {
        int wi = 5 + i;
        float tx = x + i * (eW + 2), ty = y + totalH;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, eW, thumbH});
        drawWaveThumb(tx, ty, eW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 2;

    eW = (w - (engineCount2 - 1) * 2) / engineCount2;
    for (int i = 0; i < engineCount2; i++) {
        int wi = 10 + i;
        float tx = x + i * (eW + 2), ty = y + totalH;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, eW, thumbH});
        drawWaveThumb(tx, ty, eW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 4;
    return totalH;
}

static float drawADSRCurve(float x, float y, float w, float h,
                            float *atk, float *dec, float *sus, float *rel, bool expRel) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){22, 22, 28, 255});
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){42, 42, 52, 255});

    float totalTime = *atk + *dec + 0.3f + *rel;
    float atkW = (*atk / totalTime) * w;
    float decW = (*dec / totalTime) * w;
    float susW = (0.3f / totalTime) * w;
    float relW = (*rel / totalTime) * w;
    float bot = y + h - 2, top = y + 2, range = bot - top;
    float susY = bot - *sus * range;
    Color cc = (Color){100, 200, 120, 255};

    DrawLine((int)x, (int)bot, (int)(x+atkW), (int)top, cc);
    DrawLine((int)(x+atkW), (int)top, (int)(x+atkW+decW), (int)susY, cc);
    DrawLine((int)(x+atkW+decW), (int)susY, (int)(x+atkW+decW+susW), (int)susY, cc);
    float relStartX = x + atkW + decW + susW;
    if (expRel) {
        int steps = (int)relW;
        for (int i = 0; i < steps; i++) {
            float t0 = (float)i/steps, t1 = (float)(i+1)/steps;
            float v0 = *sus * expf(-t0*3), v1 = *sus * expf(-t1*3);
            DrawLine((int)(relStartX+i), (int)(bot-v0*range), (int)(relStartX+i+1), (int)(bot-v1*range), cc);
        }
    } else {
        DrawLine((int)relStartX, (int)susY, (int)(relStartX+relW), (int)bot, cc);
    }

    Vector2 mouse = GetMousePosition();
    Color dotCol = (Color){255, 180, 60, 255};
    Rectangle atkDot = {x+atkW-4, top-4, 8, 8};
    DrawRectangleRec(atkDot, CheckCollisionPointRec(mouse, atkDot) ? WHITE : dotCol);
    Rectangle susDot = {x+atkW+decW+susW*0.5f-4, susY-4, 8, 8};
    DrawRectangleRec(susDot, CheckCollisionPointRec(mouse, susDot) ? WHITE : dotCol);

    Rectangle atkZone = {x, y, atkW+decW*0.5f, h};
    if (CheckCollisionPointRec(mouse, atkZone) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float newAtk = ((mouse.x - x) / w) * totalTime;
        *atk = newAtk < 0.001f ? 0.001f : (newAtk > 2.0f ? 2.0f : newAtk);
    }
    Rectangle susZone = {x+atkW+decW*0.5f, y, susW+decW*0.5f+relW*0.3f, h};
    if (CheckCollisionPointRec(mouse, susZone) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float newSus = 1.0f - (mouse.y - top) / range;
        *sus = newSus < 0 ? 0 : (newSus > 1 ? 1 : newSus);
    }
    return h + 4;
}

static float drawFilterXY(float x, float y, float size, float *cutoff, float *resonance) {
    DrawRectangle((int)x, (int)y, (int)size, (int)size, (Color){22, 22, 28, 255});
    DrawRectangleLinesEx((Rectangle){x, y, size, size}, 1, (Color){42, 42, 52, 255});
    for (int i = 1; i < 4; i++) {
        float gx = x + size*i*0.25f, gy = y + size*i*0.25f;
        DrawLine((int)gx, (int)y, (int)gx, (int)(y+size), (Color){32,32,38,255});
        DrawLine((int)x, (int)gy, (int)(x+size), (int)gy, (Color){32,32,38,255});
    }
    DrawTextShadow("Cut", (int)x+2, (int)(y+size-12), 9, (Color){60,60,70,255});
    DrawTextShadow("Res", (int)(x+size-20), (int)y+2, 9, (Color){60,60,70,255});
    float cx = x + (*cutoff)*size, cy = y + (1-*resonance)*size;
    DrawLine((int)cx, (int)y, (int)cx, (int)(y+size), (Color){60,80,100,200});
    DrawLine((int)x, (int)cy, (int)(x+size), (int)cy, (Color){60,80,100,200});
    DrawCircle((int)cx, (int)cy, 5, (Color){255,140,40,255});
    DrawCircle((int)cx, (int)cy, 3, WHITE);
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, (Rectangle){x,y,size,size}) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float nc = (mouse.x-x)/size, nr = 1-(mouse.y-y)/size;
        *cutoff = nc<0.01f?0.01f:(nc>1?1:nc);
        *resonance = nr<0?0:(nr>1?1:nr);
    }
    return size + 4;
}

static float drawLFOPreview(float x, float y, float w, float h, int shape, float rate, float depth) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){22, 22, 28, 255});
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){42, 42, 52, 255});
    if (depth < 0.001f || rate < 0.001f) {
        DrawTextShadow("off", (int)(x+w*0.5f-8), (int)(y+h*0.5f-5), 10, (Color){50,50,58,255});
        return h + 2;
    }
    float mid = y + h*0.5f, amp = h*0.4f*(depth>1?1:depth);
    float t = (float)GetTime();
    Color lc = (Color){130,130,220,255};
    for (int i = 0; i < (int)w - 1; i++) {
        float p0 = ((float)i/w)*2+t*rate*0.2f, p1 = ((float)(i+1)/w)*2+t*rate*0.2f;
        float v0=0, v1=0;
        switch (shape) {
            case 0: v0=sinf(p0*6.28f); v1=sinf(p1*6.28f); break;
            case 1: v0=fmodf(p0,1); v0=v0<0.5f?(4*v0-1):(3-4*v0); v1=fmodf(p1,1); v1=v1<0.5f?(4*v1-1):(3-4*v1); break;
            case 2: v0=fmodf(p0,1)<0.5f?1:-1; v1=fmodf(p1,1)<0.5f?1:-1; break;
            case 3: v0=1-2*fmodf(p0,1); v1=1-2*fmodf(p1,1); break;
            case 4: v0=((int)(p0*5)*7+3)%11/5.5f-1; v1=((int)(p1*5)*7+3)%11/5.5f-1; break;
        }
        DrawLine((int)(x+i),(int)(mid-v0*amp),(int)(x+i+1),(int)(mid-v1*amp),lc);
    }
    DrawLine((int)x,(int)mid,(int)(x+w),(int)mid,(Color){35,35,42,255});
    return h + 2;
}

// ============================================================================
// TAB BAR
// ============================================================================

static void drawTabBar(float x, float y, float w, const char** names, const int* keys,
                       int count, int* current, const char* keyPrefix) {
    DrawRectangle((int)x, (int)y, (int)w, TAB_H, (Color){28, 29, 35, 255});
    DrawLine((int)x, (int)y+TAB_H-1, (int)(x+w), (int)y+TAB_H-1, (Color){50,50,60,255});
    Vector2 mouse = GetMousePosition();
    float tx = x + 4;
    for (int i = 0; i < count; i++) {
        const char* label = TextFormat("%s%d %s", keyPrefix, i+1, names[i]);
        int tw = MeasureTextUI(label, 11) + 12;
        Rectangle r = {tx, y+2, (float)tw, TAB_H-4};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == *current);
        Color bg = act ? (Color){48,52,65,255} : (hov ? (Color){38,40,50,255} : (Color){28,29,35,255});
        DrawRectangleRec(r, bg);
        if (act) DrawRectangle((int)tx, (int)y+TAB_H-3, tw, 2, ORANGE);
        DrawTextShadow(label, (int)tx+6, (int)y+7, 11, act ? WHITE : (hov ? LIGHTGRAY : GRAY));
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *current = i; ui_consume_click(); }
        if (IsKeyPressed(keys[i])) *current = i;
        tx += tw + 2;
    }
}

// ============================================================================
// NARROW SIDEBAR
// ============================================================================

static void drawSidebar(void) {
    DrawRectangle(0, 0, SIDEBAR_W, SCREEN_HEIGHT, (Color){24, 25, 30, 255});
    DrawLine(SIDEBAR_W-1, 0, SIDEBAR_W-1, SCREEN_HEIGHT, (Color){50, 50, 62, 255});

    Vector2 mouse = GetMousePosition();
    float x = 4, y = 6;

    // Play/Stop
    {
        Rectangle r = {x, y, SIDEBAR_W-8, 20};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = playing ? (Color){40, 80, 40, 255} : (hov ? (Color){45, 45, 55, 255} : (Color){32, 33, 40, 255});
        DrawRectangleRec(r, bg);
        DrawTextShadow(playing ? "Stop" : "Play", (int)x+20, (int)y+4, 11, playing ? GREEN : LIGHTGRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { playing = !playing; ui_consume_click(); }
    }
    y += 24;

    // BPM (compact)
    DrawTextShadow(TextFormat("%.0f", bpm), (int)x+2, (int)y, 10, (Color){140,140,150,255});
    DrawTextShadow("bpm", (int)x+30, (int)y, 9, (Color){70,70,80,255});
    y += 16;

    // Divider
    DrawLine((int)x, (int)y, SIDEBAR_W-4, (int)y, (Color){42,42,52,255});
    y += 6;

    // Master volume
    DrawTextShadow("Mstr", (int)x, (int)y, 9, (Color){170,160,80,255});
    y += 12;
    float barH = 60;
    float barX = x + 20, barW = 16;
    DrawRectangle((int)barX, (int)y, (int)barW, (int)barH, (Color){20,20,25,255});
    float fillH = masterVol * barH;
    DrawRectangle((int)barX, (int)(y+barH-fillH), (int)barW, (int)fillH, (Color){170,150,50,255});
    Rectangle volR = {barX, y, barW, barH};
    if (CheckCollisionPointRec(mouse, volR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        masterVol = 1.0f - (mouse.y - y) / barH;
        if (masterVol < 0) masterVol = 0;
        if (masterVol > 1) masterVol = 1;
    }
    DrawTextShadow(TextFormat("%.0f%%", masterVol*100), (int)x, (int)(y+barH+2), 9, (Color){100,100,110,255});
}

// ============================================================================
// TRANSPORT BAR
// ============================================================================

static void drawTransport(void) {
    float x = CONTENT_X, w = CONTENT_W;
    DrawRectangle((int)x, 0, (int)w, TRANSPORT_H, (Color){25, 25, 30, 255});
    DrawLine((int)x, TRANSPORT_H-1, (int)(x+w), TRANSPORT_H-1, (Color){50,50,60,255});

    float tx = x + 10, ty = 8;
    DrawTextShadow("PixelSynth", (int)tx, (int)ty, 18, WHITE);
    tx += 130;

    DraggableFloat(tx, ty, "BPM", &bpm, 2.0f, 60.0f, 200.0f);
    tx += 130;

    if (playing) DrawTextShadow(">>>", (int)tx, (int)ty, 14, GREEN);
    tx += 50;

    // Pattern indicator
    DrawTextShadow(TextFormat("Pat:%d", currentPattern+1), (int)tx, (int)ty+2, 11, (Color){120,120,140,255});
    tx += 60;

    // Crossfader indicator
    if (crossfaderEnabled) {
        DrawTextShadow(TextFormat("XF:%d>%d %.0f%%", sceneA+1, sceneB+1, crossfaderPos*100),
                       (int)(x+w-180), (int)ty+2, 10, (Color){180,180,100,255});
    }

    DrawTextShadow(TextFormat("%d fps", GetFPS()), (int)(x+w-55), (int)ty+2, 10, (Color){50,50,50,255});
}

// ============================================================================
// WORKSPACE: SEQUENCER (grid + step inspector below)
// ============================================================================

// Step inspector state
static float stepVel = 1.0f, stepProb = 1.0f, stepGate = 1.0f;
static int stepCond = 0, stepRetrig = 0;
static bool stepSlide = false, stepAccent = false;
static const char* condNames[] = {"Always","1:2","2:2","1:3","2:3","3:3","1:4","Fill","!Fill","First","Last"};
static const char* retrigNames[] = {"Off","1/8","1/16","1/32"};

static void drawWorkSeq(float x, float y, float w, float h) {
    if (!drumStepsInit) {
        drumSteps[0][0] = drumSteps[0][4] = drumSteps[0][8] = drumSteps[0][12] = true;
        drumSteps[1][4] = drumSteps[1][12] = true;
        for (int i = 0; i < 16; i += 2) drumSteps[2][i] = true;
        drumStepsInit = true;
    }

    Vector2 mouse = GetMousePosition();
    int steps = seqStepCount;

    // Top row: Pattern selector + 16/32 toggle
    DrawTextShadow("Pattern:", (int)x, (int)y+2, 11, GRAY);
    float px = x + 60;
    for (int i = 0; i < 8; i++) {
        Rectangle r = {px + i*28, y, 26, 18};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == currentPattern);
        Color bg = act ? (Color){50,90,50,255} : (hov ? (Color){42,44,52,255} : (Color){33,34,40,255});
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, act ? GREEN : (Color){48,48,58,255});
        DrawTextShadow(TextFormat("%d", i+1), (int)px+i*28+9, (int)y+3, 10, act ? WHITE : GRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { currentPattern = i; ui_consume_click(); }
    }

    // 16/32 toggle
    float togX = px + 8 * 28 + 12;
    {
        Rectangle r16 = {togX, y, 26, 18};
        Rectangle r32 = {togX + 28, y, 26, 18};
        bool h16 = CheckCollisionPointRec(mouse, r16);
        bool h32 = CheckCollisionPointRec(mouse, r32);
        DrawRectangleRec(r16, steps==16 ? (Color){60,80,60,255} : (h16 ? (Color){42,44,52,255} : (Color){33,34,40,255}));
        DrawRectangleRec(r32, steps==32 ? (Color){60,80,60,255} : (h32 ? (Color){42,44,52,255} : (Color){33,34,40,255}));
        DrawRectangleLinesEx(r16, 1, steps==16 ? GREEN : (Color){48,48,58,255});
        DrawRectangleLinesEx(r32, 1, steps==32 ? GREEN : (Color){48,48,58,255});
        DrawTextShadow("16", (int)togX+6, (int)y+3, 10, steps==16 ? WHITE : GRAY);
        DrawTextShadow("32", (int)togX+34, (int)y+3, 10, steps==32 ? WHITE : GRAY);
        if (h16 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { seqStepCount = 16; ui_consume_click(); }
        if (h32 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { seqStepCount = 32; ui_consume_click(); }
    }

    y += 22; h -= 22;

    // Fixed cell size — always reserve space for inspector so grid doesn't jump
    int labelW = 124; // mute + solo + volume bar + name
    int cellW = (int)((w - labelW - 8) / steps);
    if (cellW < 6) cellW = 6;
    int cellH;
    if (steps == 32) {
        cellH = cellW; // square cells at 32 steps
    } else {
        // Always compute as if inspector is showing, so no jump on toggle
        float gridH = h - 80;
        cellH = (int)((gridH - 18) / 7);
    }
    if (cellH < 12) cellH = 12;
    if (cellH > 28) cellH = 28;

    const char* trackNames[] = {"Kick", "Snare", "CH", "Clap", "Bass", "Lead", "Chord"};

    // Step numbers (show every 1 for 16, every 2 for 32)
    int numSkip = (steps == 32) ? 2 : 1;
    for (int i = 0; i < steps; i++) {
        int sx = (int)x + labelW + i * cellW;
        if (i % numSkip != 0 && steps == 32) continue;
        Color numCol = (i%4==0) ? (Color){80,80,90,255} : (Color){50,50,58,255};
        DrawTextShadow(TextFormat("%d", i+1), sx + (steps==32 ? 1 : cellW/2-3), (int)y, steps==32 ? 8 : 9, numCol);
    }

    for (int track = 0; track < 7; track++) {
        int ty = (int)y + 14 + track * (cellH + 2);
        bool isDrum = (track < 4);
        if (track == 4) DrawLine((int)x, ty-2, (int)(x+w), ty-2, (Color){55,55,70,255});

        // --- Inline mute + volume bar + track name ---
        int lx = (int)x;
        int lcy = ty + cellH/2; // vertical center of row

        // Mute button (12x12)
        Rectangle muteR = {(float)lx, (float)(lcy-6), 12, 12};
        bool muteHov = CheckCollisionPointRec(mouse, muteR);
        Color muteBg = busMute[track] ? (Color){150,50,50,255} : (muteHov ? (Color){55,45,45,255} : (Color){35,30,30,255});
        DrawRectangleRec(muteR, muteBg);
        DrawTextShadow("M", lx+2, lcy-5, 9, busMute[track] ? WHITE : GRAY);
        if (muteHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busMute[track] = !busMute[track]; ui_consume_click(); }

        // Solo button (12x12)
        Rectangle soloR = {(float)(lx+14), (float)(lcy-6), 12, 12};
        bool soloHov = CheckCollisionPointRec(mouse, soloR);
        Color soloBg = busSolo[track] ? (Color){170,170,55,255} : (soloHov ? (Color){55,55,40,255} : (Color){35,35,30,255});
        DrawRectangleRec(soloR, soloBg);
        DrawTextShadow("S", lx+16, lcy-5, 9, busSolo[track] ? BLACK : GRAY);
        if (soloHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busSolo[track] = !busSolo[track]; ui_consume_click(); }

        // Volume bar (horizontal, 40px wide)
        float volBarX = lx + 29, volBarW = 40, volBarH = 8;
        float volBarY = lcy - volBarH/2;
        DrawRectangle((int)volBarX, (int)volBarY, (int)volBarW, (int)volBarH, (Color){20,20,25,255});
        float volFill = busVolumes[track] * volBarW;
        Color volCol = busMute[track] ? (Color){80,40,40,255} : (isDrum ? (Color){50,110,50,255} : (Color){50,80,140,255});
        DrawRectangle((int)volBarX, (int)volBarY, (int)volFill, (int)volBarH, volCol);
        Rectangle volR = {volBarX, (float)ty, volBarW, (float)cellH}; // full row height for easier dragging
        if (CheckCollisionPointRec(mouse, volR) && !muteHov && !soloHov && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            busVolumes[track] = (mouse.x - volBarX) / volBarW;
            if (busVolumes[track] < 0) busVolumes[track] = 0;
            if (busVolumes[track] > 1) busVolumes[track] = 1;
        }

        // Track name (clickable — selects patch/drums in param panel)
        Rectangle nameR = {(float)(lx + 70), (float)ty, (float)(labelW - 70), (float)cellH};
        bool nameHov = CheckCollisionPointRec(mouse, nameR);
        Color nameCol = isDrum ? LIGHTGRAY : (Color){140,180,255,255};
        if (nameHov) { nameCol.r = (unsigned char)(nameCol.r + 40 > 255 ? 255 : nameCol.r + 40);
                       nameCol.g = (unsigned char)(nameCol.g + 40 > 255 ? 255 : nameCol.g + 40);
                       nameCol.b = (unsigned char)(nameCol.b + 40 > 255 ? 255 : nameCol.b + 40); }
        DrawTextShadow(trackNames[track], lx + 72, lcy-5, 9, nameCol);
        if (nameHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedPatch = track; // all tracks map 1:1 to patches[]
            if (isDrum) selectedDrum = track;
            paramTab = PARAM_PATCH;
            ui_consume_click();
        }

        for (int step = 0; step < steps; step++) {
            int sx = (int)x + labelW + step * cellW;
            Rectangle cell = {(float)sx, (float)ty, (float)(cellW-1), (float)cellH};
            bool hov = CheckCollisionPointRec(mouse, cell);
            Color bg = (step/4)%2==0 ? (Color){36,36,42,255} : (Color){30,30,35,255};

            if (isDrum && step < 32 && drumSteps[track][step]) bg = (Color){50,125,65,255};
            if (hov) { bg.r += 18; bg.g += 18; bg.b += 18; }
            DrawRectangleRec(cell, bg);
            DrawRectangleLinesEx(cell, 1, (Color){48,48,56,255});

            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (isDrum && step < 32) drumSteps[track][step] = !drumSteps[track][step];
                ui_consume_click();
            }
            if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                if (detailCtx == DETAIL_STEP && detailTrack == track && detailStep == step) {
                    detailCtx = DETAIL_NONE; detailStep = -1; detailTrack = -1;
                } else {
                    detailCtx = DETAIL_STEP; detailTrack = track; detailStep = step;
                }
                ui_consume_click();
            }

            if (track == detailTrack && step == detailStep && detailCtx == DETAIL_STEP)
                DrawRectangleLinesEx(cell, 2, ORANGE);
        }
    }

    // === STEP INSPECTOR (below grid) ===
    if (detailCtx == DETAIL_STEP && detailStep >= 0) {
        float iy = y + 14 + 7 * (cellH + 2) + 6;
        DrawLine((int)x, (int)iy-2, (int)(x+w), (int)iy-2, (Color){50,50,62,255});

        // Step header
        const char* tn = (detailTrack >= 0 && detailTrack < 7) ? trackNames[detailTrack] : "?";
        bool isDrumTrack = (detailTrack < 4);
        DrawTextShadow(TextFormat("Step %d - %s", detailStep+1, tn),
                       (int)x+4, (int)iy+2, 13, ORANGE);

        // Params spread horizontally
        float cx = x + 130;
        UIColumn c1 = ui_column(cx, iy, 15);
        ui_col_float_p(&c1, "Velocity", &stepVel, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c1, "Probability", &stepProb, 0.05f, 0.0f, 1.0f);

        UIColumn c2 = ui_column(cx + 180, iy, 15);
        ui_col_float(&c2, "Gate", &stepGate, 0.05f, 0.1f, 2.0f);
        ui_col_cycle(&c2, "Condition", condNames, 11, &stepCond);

        UIColumn c3 = ui_column(cx + 360, iy, 15);
        ui_col_cycle(&c3, "Retrig", retrigNames, 4, &stepRetrig);
        if (!isDrumTrack) {
            ui_col_toggle(&c3, "Slide", &stepSlide);
        }

        UIColumn c4 = ui_column(cx + 520, iy, 15);
        if (!isDrumTrack) {
            ui_col_toggle(&c4, "Accent", &stepAccent);
        }
        ui_col_sublabel(&c4, "P-Locks:", (Color){140,140,200,255});
        DrawTextShadow("shift+click param to lock", (int)(cx+520), (int)iy+20, 9, (Color){55,55,65,255});

        // Deselect on Escape
        if (IsKeyPressed(KEY_ESCAPE)) { detailCtx = DETAIL_NONE; detailStep = -1; detailTrack = -1; }
    }
}

// ============================================================================
// WORKSPACE: PIANO ROLL
// ============================================================================

static void drawWorkPiano(float x, float y, float w, float h) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){20,20,25,255});
    int keyW = 34, noteH = (int)(h / 24);
    if (noteH < 4) noteH = 4;
    const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int vis = (int)(h / noteH); if (vis > 24) vis = 24;

    for (int i = 0; i < vis; i++) {
        int ny = (int)(y+h) - (i+1)*noteH;
        int ni = i % 12;
        bool blk = (ni==1||ni==3||ni==6||ni==8||ni==10);
        DrawRectangle((int)x, ny, keyW, noteH-1, blk ? (Color){28,28,33,255} : (Color){42,42,48,255});
        if (noteH >= 10) DrawTextShadow(TextFormat("%s%d", noteNames[ni], 3+i/12), (int)x+2, ny+1, 8, GRAY);
        DrawLine((int)x+keyW, ny+noteH-1, (int)(x+w), ny+noteH-1, ni==0 ? (Color){55,55,65,255} : (Color){32,32,38,255});
    }
    int stepW = (int)((w - keyW) / 16);
    for (int i = 0; i <= 16; i++) {
        int lx = (int)x + keyW + i*stepW;
        DrawLine(lx, (int)y, lx, (int)(y+h), i%4==0 ? (Color){60,60,72,255} : (Color){36,36,42,255});
    }
    DrawTextShadow("Piano Roll", (int)(x+w/2-36), (int)(y+h/2-6), 14, (Color){50,50,60,255});
    DrawRectangleLinesEx((Rectangle){x,y,w,h}, 1, (Color){48,48,58,255});
}

// ============================================================================
// WORKSPACE: SONG
// ============================================================================

static void drawWorkSong(float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    DrawTextShadow("Pattern Chain:", (int)x+4, (int)y+4, 13, (Color){180,180,255,255});
    float cy = y + 22; int chW = 28, chH = 24;

    for (int i = 0; i < chainLength; i++) {
        float cx = x + 4 + i * (chW+3);
        if (cx + chW > x + w - 4) break;
        Rectangle r = {cx, cy, (float)chW, (float)chH};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){55,58,68,255} : (Color){40,40,50,255});
        DrawRectangleLinesEx(r, 1, (Color){62,62,72,255});
        DrawTextShadow(TextFormat("%d", chain[i]+1), (int)cx+9, (int)cy+6, 11, WHITE);
        if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            for (int j = i; j < chainLength-1; j++) chain[j] = chain[j+1];
            chainLength--; ui_consume_click();
        }
        if (hov) { float wh = GetMouseWheelMove(); if (wh>0) chain[i]=(chain[i]+1)%8; else if (wh<0) chain[i]=(chain[i]+7)%8; }
    }
    if (chainLength < 64) {
        float ax = x + 4 + chainLength*(chW+3);
        if (ax + chW <= x + w - 4) {
            Rectangle ar = {ax, cy, (float)chW, (float)chH};
            bool ah = CheckCollisionPointRec(mouse, ar);
            DrawRectangleRec(ar, ah ? (Color){60,62,72,255} : (Color){35,36,45,255});
            DrawRectangleLinesEx(ar, 1, (Color){70,70,82,255});
            DrawTextShadow("+", (int)ax+10, (int)cy+6, 11, ah ? WHITE : GRAY);
            if (ah && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { chain[chainLength++] = currentPattern; ui_consume_click(); }
        }
    }

    // Scenes
    float sy = cy + chH + 12;
    DrawTextShadow("Scenes:", (int)x+4, (int)sy, 13, (Color){180,200,255,255});
    sy += 18;
    for (int i = 0; i < sceneCount; i++) {
        float sx = x + 4 + i * 68;
        if (sx + 62 > x + w) break;
        Rectangle r = {sx, sy, 62, 26};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){50,54,64,255} : (Color){36,38,46,255});
        DrawRectangleLinesEx(r, 1, (Color){60,60,72,255});
        DrawTextShadow(TextFormat("Scene %d", i+1), (int)sx+6, (int)sy+7, 10, LIGHTGRAY);
    }

    // Arrangement placeholder
    float tlY = sy + 38, tlH = y + h - tlY - 4;
    if (tlH > 20) {
        DrawRectangle((int)x, (int)tlY, (int)w, (int)tlH, (Color){20,20,25,255});
        DrawRectangleLinesEx((Rectangle){x,tlY,w,tlH}, 1, (Color){45,45,55,255});
        DrawTextShadow("Arrangement Timeline", (int)(x+w/2-75), (int)(tlY+tlH/2-6), 13, (Color){48,48,58,255});
    }
}

// ============================================================================
// PARAMS: PATCH (Synth) - Horizontal 4-column layout
// ============================================================================

static void drawParamPatch(float x, float y, float w, float h) {
    // After closing preset picker, skip until mouse is released to prevent click-through
    if (presetPickerJustClosed) {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) presetPickerJustClosed = false;
        return;
    }

    SynthPatch *p = &patches[selectedPatch];
    SynthPatch dp = createDefaultPatch(p->p_waveType); // default for comparison

    // Preset selector + sub-tab row
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    float presetRowY = y;
    {
        int fs = 14;

        // Preset button (left)
        int pi = patchPresetIndex[selectedPatch];
        const char* presetName = (pi >= 0) ? instrumentPresets[pi].name : "Custom";
        bool dirty = isPatchDirty(selectedPatch);
        const char* display = dirty ? TextFormat("[%s *]", presetName) : TextFormat("[%s]", presetName);

        DrawTextShadow(display, (int)x + 8, (int)y + 2, fs, (Color){180,180,220,255});

        int tw = MeasureText(display, fs) + 16;
        Rectangle presetR = {x + 4, y, (float)tw, 18};
        if (CheckCollisionPointRec(GetMousePosition(), presetR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            presetPickerOpen = !presetPickerOpen;
            ui_consume_click();
        }

        // (sub-tabs removed — everything is on one page now)

        y += 20;
        h -= 20;
    }

    // If preset picker is open, skip all column content (drawn after return)
    if (presetPickerOpen) {
        // Draw popup on top
        float popX = x + 4, popY = presetRowY + 20;
        float popW = 400;
        int pcols = 3, perCol = (NUM_INSTRUMENT_PRESETS + pcols - 1) / pcols;
        float popH = perCol * 18 + 8;
        Vector2 mouse = GetMousePosition();

        DrawRectangle((int)popX, (int)popY, (int)popW, (int)popH, (Color){25,25,35,245});
        DrawRectangleLinesEx((Rectangle){popX,popY,popW,popH}, 1, (Color){80,80,100,255});

        float colW = popW / pcols;
        bool clickedItem = false;
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            int col = i / perCol, row = i % perCol;
            float ix = popX + col * colW + 6;
            float iy = popY + 4 + row * 18;
            Rectangle itemR = {ix - 2, iy, colW - 4, 17};
            bool hov = CheckCollisionPointRec(mouse, itemR);
            bool selected = (i == patchPresetIndex[selectedPatch]);
            if (selected) DrawRectangleRec(itemR, (Color){50,50,80,255});
            else if (hov) DrawRectangleRec(itemR, (Color){40,40,60,255});
            Color tc = selected ? ORANGE : (hov ? WHITE : (Color){160,160,180,255});
            DrawTextShadow(instrumentPresets[i].name, (int)ix, (int)iy + 1, 13, tc);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                loadPresetIntoPatch(selectedPatch, i);
                presetPickerOpen = false;
                presetPickerJustClosed = true;
                clickedItem = true;
                ui_consume_click();
            }
        }

        // Click outside popup closes it (also exclude the preset button itself)
        Rectangle popR = {popX, popY, popW, popH};
        Rectangle btnR = {x + 4, presetRowY, 200, 20};
        if (!clickedItem && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && !CheckCollisionPointRec(mouse, popR)
            && !CheckCollisionPointRec(mouse, btnR)) {
            presetPickerOpen = false;
            ui_consume_click();
        }
        return;
    }

    // Compact font for all patch sub-tabs
    #define PCOL(px, py) ui_column_small((px), (py), 13, 12)
    // Section highlight helpers — compare float/int/bool fields against defaults
    #define DF(field) (fabsf(p->field - dp.field) > 0.001f)
    #define DI(field) (p->field != dp.field)
    #define DB(field) (p->field != dp.field)

    // ---- SUB-TAB: MAIN ----
    {

    float col1X = x, col1W = 150;
    float col2X = x + 155;
    float col3X = x + 295;
    float col4X = x + 430;
    float col5X = x + 575;
    float col6X = x + 720;
    float col7X = x + 870;
    float percColW = 140;

    // COL 1: Oscillator + wave-specific
    {
        UIColumn c = PCOL(col1X + 8, y);
        ui_col_sublabel(&c, TextFormat("OSC: %s [%s]", patches[selectedPatch].p_name, waveNames[p->p_waveType]), ORANGE);
        c.y += drawWaveSelector(c.x, c.y, col1W - 16, &p->p_waveType);
        ui_col_space(&c, 2);

        if (p->p_waveType == WAVE_SQUARE) {
            ui_col_sublabel(&c, "PWM:", (Color){140,160,200,255});
            ui_col_float_p(&c, "Width", &p->p_pulseWidth, 0.05f, 0.1f, 0.9f);
            ui_col_float(&c, "Rate", &p->p_pwmRate, 0.5f, 0.1f, 20.0f);
            ui_col_float(&c, "Depth", &p->p_pwmDepth, 0.02f, 0.0f, 0.4f);
        } else if (p->p_waveType == WAVE_SCW) {
            ui_col_sublabel(&c, "Wavetable:", (Color){140,160,200,255});
            ui_col_int(&c, "SCW", &p->p_scwIndex, 0.3f, 0, 20);
        } else if (p->p_waveType == WAVE_VOICE) {
            ui_col_sublabel(&c, "Formant:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Vowel", vowelNames, 5, &p->p_voiceVowel);
            ui_col_toggle(&c, "Random", &daw_voiceRandomVowel);
            ui_col_float(&c, "Pitch", &p->p_voicePitch, 0.1f, 0.3f, 2.0f);
            ui_col_float(&c, "Speed", &p->p_voiceSpeed, 1.0f, 4.0f, 20.0f);
            ui_col_float(&c, "Formant", &p->p_voiceFormantShift, 0.05f, 0.5f, 1.5f);
            ui_col_float(&c, "Breath", &p->p_voiceBreathiness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Buzz", &p->p_voiceBuzziness, 0.05f, 0.0f, 1.0f);
            ui_col_toggle(&c, "Consonant", &p->p_voiceConsonant);
            if (p->p_voiceConsonant) ui_col_float(&c, "ConsAmt", &p->p_voiceConsonantAmt, 0.05f, 0.0f, 1.0f);
            ui_col_toggle(&c, "Nasal", &p->p_voiceNasal);
            if (p->p_voiceNasal) ui_col_float(&c, "NasalAmt", &p->p_voiceNasalAmt, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Pitch Env:", (Color){120,140,180,255});
            ui_col_float(&c, "Bend", &p->p_voicePitchEnv, 0.5f, -12.0f, 12.0f);
            ui_col_float(&c, "Time", &p->p_voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
            ui_col_float(&c, "Curve", &p->p_voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
        } else if (p->p_waveType == WAVE_PLUCK) {
            ui_col_sublabel(&c, "Pluck:", (Color){140,160,200,255});
            ui_col_float(&c, "Bright", &p->p_pluckBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Sustain", &p->p_pluckDamping, 0.0002f, 0.995f, 0.9998f);
            ui_col_float(&c, "Damp", &p->p_pluckDamp, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_ADDITIVE) {
            ui_col_sublabel(&c, "Additive:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", additivePresetNames, 5, &p->p_additivePreset);
            ui_col_float(&c, "Bright", &p->p_additiveBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Shimmer", &p->p_additiveShimmer, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Inharm", &p->p_additiveInharmonicity, 0.005f, 0.0f, 0.1f);
        } else if (p->p_waveType == WAVE_MALLET) {
            ui_col_sublabel(&c, "Mallet:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", malletPresetNames, 5, &p->p_malletPreset);
            ui_col_float(&c, "Stiff", &p->p_malletStiffness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Hard", &p->p_malletHardness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_malletStrikePos, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Reson", &p->p_malletResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Damp", &p->p_malletDamp, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Tremolo", &p->p_malletTremolo, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "TremSpd", &p->p_malletTremoloRate, 0.5f, 1.0f, 12.0f);
        } else if (p->p_waveType == WAVE_GRANULAR) {
            ui_col_sublabel(&c, "Granular:", (Color){140,160,200,255});
            ui_col_int(&c, "Source", &p->p_granularScwIndex, 0.3f, 0, 20);
            ui_col_float(&c, "Size", &p->p_granularGrainSize, 5.0f, 10.0f, 200.0f);
            ui_col_float(&c, "Density", &p->p_granularDensity, 2.0f, 1.0f, 100.0f);
            ui_col_float(&c, "Pos", &p->p_granularPosition, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "PosRnd", &p->p_granularPosRandom, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Pitch", &p->p_granularPitch, 0.1f, 0.25f, 4.0f);
            ui_col_float(&c, "PitRnd", &p->p_granularPitchRandom, 0.5f, 0.0f, 12.0f);
            ui_col_toggle(&c, "Freeze", &p->p_granularFreeze);
        } else if (p->p_waveType == WAVE_FM) {
            ui_col_sublabel(&c, "FM Synth:", (Color){140,160,200,255});
            ui_col_float(&c, "Ratio", &p->p_fmModRatio, 0.5f, 0.5f, 16.0f);
            ui_col_float(&c, "Index", &p->p_fmModIndex, 0.1f, 0.0f, 10.0f);
            ui_col_float(&c, "Feedback", &p->p_fmFeedback, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_PD) {
            ui_col_sublabel(&c, "Phase Dist:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Wave", pdWaveNames, 5, &p->p_pdWaveType);
            ui_col_float(&c, "Distort", &p->p_pdDistortion, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_MEMBRANE) {
            ui_col_sublabel(&c, "Membrane:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", membranePresetNames, 5, &p->p_membranePreset);
            ui_col_float(&c, "Damping", &p->p_membraneDamping, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_membraneStrike, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bend", &p->p_membraneBend, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "BndDcy", &p->p_membraneBendDecay, 0.01f, 0.02f, 0.3f);
        } else if (p->p_waveType == WAVE_BIRD) {
            ui_col_sublabel(&c, "Bird:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Type", birdTypeNames, 5, &p->p_birdType);
            ui_col_float(&c, "Range", &p->p_birdChirpRange, 0.1f, 0.5f, 2.0f);
            ui_col_float(&c, "Harmon", &p->p_birdHarmonics, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Trill:", (Color){120,140,180,255});
            ui_col_float(&c, "Rate", &p->p_birdTrillRate, 1.0f, 0.0f, 30.0f);
            ui_col_float(&c, "Depth", &p->p_birdTrillDepth, 0.2f, 0.0f, 5.0f);
            ui_col_sublabel(&c, "Flutter:", (Color){120,140,180,255});
            ui_col_float(&c, "AM Rate", &p->p_birdAmRate, 1.0f, 0.0f, 20.0f);
            ui_col_float(&c, "AM Dep", &p->p_birdAmDepth, 0.05f, 0.0f, 1.0f);
        }
    }

    // Vertical divider
    DrawLine((int)col2X-4, (int)y, (int)col2X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 2: Envelope + Filter
    {
        float envX = col2X, envW = 130;

        // Envelope section highlight
        bool envActive = DF(p_attack) || DF(p_decay) || DF(p_sustain) || DF(p_release)
                         || DB(p_expRelease) || DB(p_expDecay) || DB(p_oneShot);

        // ADSR curve
        UIColumn c = PCOL(envX, y);
        float envStartY = c.y;
        ui_col_sublabel(&c, "Envelope:", ORANGE);
        c.y += drawADSRCurve(envX, c.y, envW, 55, &p->p_attack, &p->p_decay, &p->p_sustain, &p->p_release, p->p_expRelease);

        ui_col_float_p(&c, "Atk", &p->p_attack, 0.5f, 0.001f, 2.0f);
        ui_col_float_p(&c, "Dec", &p->p_decay, 0.5f, 0.0f, 2.0f);
        ui_col_float(&c, "Sus", &p->p_sustain, 0.5f, 0.0f, 1.0f);
        ui_col_float(&c, "Rel", &p->p_release, 0.5f, 0.01f, 3.0f);
        ui_col_toggle(&c, "Exp Release", &p->p_expRelease);
        ui_col_toggle(&c, "Exp Decay", &p->p_expDecay);
        ui_col_toggle(&c, "One Shot", &p->p_oneShot);
        sectionHighlight(envX - 2, envStartY, envW + 4, c.y - envStartY, envActive);
        ui_col_space(&c, 6);

        // Filter section highlight
        bool filtActive = DF(p_filterCutoff) || DF(p_filterResonance) || DI(p_filterType)
                          || DF(p_filterEnvAmt) || DF(p_filterEnvAttack) || DF(p_filterEnvDecay);
        float filtStartY = c.y;

        ui_col_sublabel(&c, "Filter:", ORANGE);
        ui_col_cycle(&c, "Type", filterTypeNames, 6, &p->p_filterType);
        float filtY = c.y;
        ui_col_float_p(&c, "Cut", &p->p_filterCutoff, 0.05f, 0.01f, 1.0f);
        ui_col_float_p(&c, "Res", &p->p_filterResonance, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "EnvAmt", &p->p_filterEnvAmt, 0.05f, -1.0f, 1.0f);
        ui_col_float(&c, "EnvAtk", &p->p_filterEnvAttack, 0.01f, 0.001f, 0.5f);
        ui_col_float(&c, "EnvDcy", &p->p_filterEnvDecay, 0.05f, 0.01f, 2.0f);
        sectionHighlight(envX - 2, filtStartY, envW + 4, c.y - filtStartY, filtActive);

        // XY pad below filter sliders
        float padSize = 60;
        drawFilterXY(envX, c.y, padSize, &p->p_filterCutoff, &p->p_filterResonance);
        c.y += padSize + 4;
    }

    // Vertical divider
    DrawLine((int)col3X-4, (int)y, (int)col3X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 3: Voice params (vibrato, volume, mono, unison, arp, scale)
    {
        float col3W = 130;
        UIColumn c = PCOL(col3X, y);

        bool vibActive = DF(p_vibratoRate) || DF(p_vibratoDepth);
        float secY = c.y;
        ui_col_sublabel(&c, "Vibrato:", ORANGE);
        ui_col_float(&c, "Rate", &p->p_vibratoRate, 0.5f, 0.5f, 15.0f);
        ui_col_float(&c, "Depth", &p->p_vibratoDepth, 0.2f, 0.0f, 2.0f);
        sectionHighlight(col3X - 2, secY, col3W, c.y - secY, vibActive);
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Volume:", ORANGE);
        ui_col_float_p(&c, "Note", &p->p_volume, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Master", &daw_masterVolume, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 3);

        if (p->p_waveType != WAVE_PLUCK && p->p_waveType != WAVE_MALLET) {
            bool monoActive = DB(p_monoMode) || DF(p_glideTime);
            secY = c.y;
            ui_col_sublabel(&c, "Mono/Glide:", ORANGE);
            ui_col_toggle(&c, "Mono", &p->p_monoMode);
            if (p->p_monoMode) ui_col_float(&c, "Glide", &p->p_glideTime, 0.02f, 0.01f, 1.0f);
            sectionHighlight(col3X - 2, secY, col3W, c.y - secY, monoActive);
            ui_col_space(&c, 3);
        }

        if (p->p_waveType <= WAVE_TRIANGLE) {
            bool uniActive = DI(p_unisonCount) || DF(p_unisonDetune) || DF(p_unisonMix);
            secY = c.y;
            ui_col_sublabel(&c, "Unison:", ORANGE);
            ui_col_int(&c, "Count", &p->p_unisonCount, 1, 1, 4);
            if (p->p_unisonCount > 1) {
                ui_col_float(&c, "Detune", &p->p_unisonDetune, 1.0f, 0.0f, 50.0f);
                ui_col_float(&c, "Mix", &p->p_unisonMix, 0.05f, 0.0f, 1.0f);
            }
            sectionHighlight(col3X - 2, secY, col3W, c.y - secY, uniActive);
            ui_col_space(&c, 3);
        }

        bool arpActive = DB(p_arpEnabled);
        secY = c.y;
        ui_col_sublabel(&c, "Arpeggiator:", ORANGE);
        ui_col_toggle(&c, "On", &p->p_arpEnabled);
        if (p->p_arpEnabled) {
            ui_col_cycle(&c, "Mode", arpModeNames, 4, &p->p_arpMode);
            ui_col_cycle(&c, "Chord", arpChordNames, 7, &p->p_arpChord);
            ui_col_cycle(&c, "Rate", arpRateNames, 5, &p->p_arpRateDiv);
            if (p->p_arpRateDiv == 4) ui_col_float(&c, "Hz", &p->p_arpRate, 0.5f, 1.0f, 30.0f);
        }
        sectionHighlight(col3X - 2, secY, col3W, c.y - secY, arpActive);
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Scale Lock:", ORANGE);
        ui_col_toggle(&c, "On", &daw_scaleLockEnabled);
        if (daw_scaleLockEnabled) {
            ui_col_cycle(&c, "Root", rootNoteNames, 12, &daw_scaleRoot);
            ui_col_cycle(&c, "Scale", scaleNames, 9, &daw_scaleType);
        }
    }

    // Vertical divider
    DrawLine((int)col4X-4, (int)y, (int)col4X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 4: LFOs (4 stacked, each with preview)
    {
        float lfoW = w - (col4X - x) - 8;
        float previewW = lfoW * 0.4f;
        float sliderW = lfoW - previewW - 8;
        (void)sliderW;

        struct { const char* name; float *rate, *depth; int *shape; int *sync; } lfos[] = {
            {"Filter LFO", &p->p_filterLfoRate, &p->p_filterLfoDepth, &p->p_filterLfoShape, &p->p_filterLfoSync},
            {"Reso LFO",   &p->p_resoLfoRate,   &p->p_resoLfoDepth,   &p->p_resoLfoShape,   NULL},
            {"Amp LFO",    &p->p_ampLfoRate,     &p->p_ampLfoDepth,    &p->p_ampLfoShape,    NULL},
            {"Pitch LFO",  &p->p_pitchLfoRate,   &p->p_pitchLfoDepth,  &p->p_pitchLfoShape,  NULL},
        };

        float ly = y;
        for (int i = 0; i < 4; i++) {
            UIColumn c = PCOL(col4X, ly);
            ui_col_sublabel(&c, lfos[i].name, (Color){140,140,200,255});
            float sliderY = c.y;
            ui_col_float(&c, "Rate", lfos[i].rate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&c, "Depth", lfos[i].depth, 0.05f, 0.0f, 2.0f);
            ui_col_cycle(&c, "Shape", lfoShapeNames, 5, lfos[i].shape);
            if (lfos[i].sync) ui_col_cycle(&c, "Sync", lfoSyncNames, 8, lfos[i].sync);

            float preH = c.y - sliderY - 4;
            if (preH < 30) preH = 30;
            drawLFOPreview(col4X + lfoW - previewW, sliderY, previewW, preH,
                           *lfos[i].shape, *lfos[i].rate, *lfos[i].depth);

            ly = c.y + 6;
        }
    }

    // Vertical divider
    DrawLine((int)col5X-4, (int)y, (int)col5X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 5: Pitch Envelope + Drive + Click
    {
        UIColumn c = PCOL(col5X + 4, y);
        bool pitchActive = DF(p_pitchEnvAmount) || DF(p_pitchEnvDecay) || DF(p_pitchEnvCurve) || DB(p_pitchEnvLinear);
        float secY = c.y;
        ui_col_sublabel(&c, "Pitch Env:", ORANGE);
        ui_col_float(&c, "Amount", &p->p_pitchEnvAmount, 1.0f, -48.0f, 48.0f);
        ui_col_float(&c, "Decay", &p->p_pitchEnvDecay, 0.01f, 0.005f, 0.5f);
        ui_col_float(&c, "Curve", &p->p_pitchEnvCurve, 0.1f, -1.0f, 1.0f);
        ui_col_toggle(&c, "Linear Hz", &p->p_pitchEnvLinear);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, pitchActive);
        ui_col_space(&c, 6);

        bool driveActive = DF(p_drive);
        secY = c.y;
        ui_col_sublabel(&c, "Drive:", ORANGE);
        ui_col_float(&c, "Drive", &p->p_drive, 0.05f, 0.0f, 1.0f);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, driveActive);
        ui_col_space(&c, 6);

        bool clickActive = DF(p_clickLevel);
        secY = c.y;
        ui_col_sublabel(&c, "Click:", ORANGE);
        ui_col_float(&c, "Level", &p->p_clickLevel, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Time", &p->p_clickTime, 0.002f, 0.001f, 0.05f);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, clickActive);
    }

    // Vertical divider
    DrawLine((int)col6X-4, (int)y, (int)col6X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 6: Noise + Retrigger
    {
        UIColumn c = PCOL(col6X + 4, y);
        bool noiseActive = DF(p_noiseMix) || DF(p_noiseTone) || DF(p_noiseHP) || DF(p_noiseDecay)
                           || DI(p_noiseMode) || DI(p_noiseType) || DB(p_noiseLPBypass);
        float secY = c.y;
        ui_col_sublabel(&c, "Noise:", ORANGE);
        ui_col_float(&c, "Mix", &p->p_noiseMix, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Tone", &p->p_noiseTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "HP", &p->p_noiseHP, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Decay", &p->p_noiseDecay, 0.02f, 0.0f, 1.0f);
        ui_col_cycle(&c, "Mode", noiseModeNames, 3, &p->p_noiseMode);
        ui_col_cycle(&c, "Type", noiseTypeNames, 2, &p->p_noiseType);
        ui_col_toggle(&c, "LP Bypass", &p->p_noiseLPBypass);
        sectionHighlight(col6X + 2, secY, percColW, c.y - secY, noiseActive);
        ui_col_space(&c, 6);

        bool retrigActive = DI(p_retriggerCount);
        secY = c.y;
        ui_col_sublabel(&c, "Retrigger:", ORANGE);
        ui_col_int(&c, "Count", &p->p_retriggerCount, 1, 0, 8);
        if (p->p_retriggerCount > 0) {
            ui_col_float(&c, "Spread", &p->p_retriggerSpread, 0.002f, 0.003f, 0.1f);
            ui_col_toggle(&c, "Overlap", &p->p_retriggerOverlap);
            if (p->p_retriggerOverlap)
                ui_col_float(&c, "BrstDcy", &p->p_retriggerBurstDecay, 0.005f, 0.005f, 0.2f);
            ui_col_float(&c, "Curve", &p->p_retriggerCurve, 0.05f, 0.0f, 1.0f);
        }
        sectionHighlight(col6X + 2, secY, percColW, c.y - secY, retrigActive);

        ui_col_space(&c, 4);
        ui_col_toggle(&c, "PhaseReset", &p->p_phaseReset);
    }

    // Vertical divider
    DrawLine((int)col7X-4, (int)y, (int)col7X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 7: Extra Oscillators
    {
        UIColumn c = PCOL(col7X + 4, y);
        bool oscActive = DF(p_osc2Level) || DF(p_osc3Level) || DF(p_osc4Level)
                         || DF(p_osc5Level) || DF(p_osc6Level);
        float secY = c.y;
        ui_col_sublabel(&c, "Extra Osc:", ORANGE);
        ui_col_cycle(&c, "MixMode", oscMixModeNames, 2, &p->p_oscMixMode);
        ui_col_space(&c, 3);

        struct { const char* label; float *ratio, *level; } oscs[] = {
            {"Osc2", &p->p_osc2Ratio, &p->p_osc2Level},
            {"Osc3", &p->p_osc3Ratio, &p->p_osc3Level},
            {"Osc4", &p->p_osc4Ratio, &p->p_osc4Level},
            {"Osc5", &p->p_osc5Ratio, &p->p_osc5Level},
            {"Osc6", &p->p_osc6Ratio, &p->p_osc6Level},
        };
        for (int i = 0; i < 5; i++) {
            ui_col_float(&c, TextFormat("%s R", oscs[i].label), oscs[i].ratio, 0.1f, 0.0f, 16.0f);
            ui_col_float(&c, TextFormat("%s L", oscs[i].label), oscs[i].level, 0.05f, 0.0f, 1.0f);
        }
        sectionHighlight(col7X + 2, secY, percColW, c.y - secY, oscActive);
    }

    (void)h;

    } // end patch columns

    #undef PCOL
    #undef DF
    #undef DI
    #undef DB
}

// ============================================================================
// PARAMS: DRUMS - Horizontal columns
// ============================================================================

static void drawParamDrums(float x, float y, float w, float h) {
    float colW = 160;

    float cols[] = {x, x+colW, x+2*colW, x+3*colW, x+4*colW, x+5*colW};

    // Kick
    {
        UIColumn c = ui_column(cols[0]+4, y, 16);
        ui_col_sublabel(&c, "Kick:", selectedDrum == 0 ? ORANGE : GRAY);
        ui_col_float_p(&c, "Pitch", &kickPitch, 3.0f, 30.0f, 100.0f);
        ui_col_float_p(&c, "Decay", &kickDecay, 0.07f, 0.1f, 1.5f);
        ui_col_float_p(&c, "Punch", &kickPunchPitch, 10.0f, 80.0f, 300.0f);
        ui_col_float(&c, "Click", &kickClick, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "Tone", &kickTone, 0.05f, 0.0f, 1.0f);
    }

    // Snare
    {
        UIColumn c = ui_column(cols[1]+4, y, 16);
        ui_col_sublabel(&c, "Snare:", selectedDrum == 1 ? ORANGE : GRAY);
        ui_col_float_p(&c, "Pitch", &snarePitch, 10.0f, 100.0f, 350.0f);
        ui_col_float_p(&c, "Decay", &snareDecay, 0.03f, 0.05f, 0.6f);
        ui_col_float(&c, "Snappy", &snareSnappy, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "Tone", &snareTone, 0.05f, 0.0f, 1.0f);
    }

    // HiHat
    {
        UIColumn c = ui_column(cols[2]+4, y, 16);
        ui_col_sublabel(&c, "HiHat:", selectedDrum == 2 ? ORANGE : GRAY);
        ui_col_float(&c, "Closed", &hhDecayClosed, 0.01f, 0.01f, 0.2f);
        ui_col_float(&c, "Open", &hhDecayOpen, 0.05f, 0.1f, 1.0f);
        ui_col_float_p(&c, "Tone", &hhTone, 0.05f, 0.0f, 1.0f);
    }

    // Clap
    {
        UIColumn c = ui_column(cols[3]+4, y, 16);
        ui_col_sublabel(&c, "Clap:", selectedDrum == 3 ? ORANGE : GRAY);
        ui_col_float_p(&c, "Decay", &clapDecay, 0.03f, 0.1f, 0.6f);
        ui_col_float_p(&c, "Tone", &clapTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Spread", &clapSpread, 0.001f, 0.005f, 0.03f);
    }

    // Groove
    {
        UIColumn c = ui_column(cols[4]+4, y, 16);
        ui_col_sublabel(&c, "Groove:", (Color){140,140,200,255});
        ui_col_int(&c, "Kick", &grooveKickNudge, 0.3f, -12, 12);
        ui_col_int(&c, "Snare", &grooveSnareDelay, 0.3f, -12, 12);
        ui_col_int(&c, "HH", &grooveHatNudge, 0.3f, -12, 12);
        ui_col_int(&c, "Swing", &grooveSwing, 0.3f, 0, 12);
        ui_col_int(&c, "Jitter", &grooveJitter, 0.3f, 0, 6);
    }

    // Sidechain + Drum volume
    {
        UIColumn c = ui_column(cols[5]+4, y, 16);
        ui_col_sublabel(&c, "Sidechain:", (Color){140,140,200,255});
        ui_col_toggle(&c, "On", &sidechainOn);
        if (sidechainOn) {
            ui_col_cycle(&c, "Source", sidechainSourceNames, 5, &sidechainSource);
            ui_col_cycle(&c, "Target", sidechainTargetNames, 4, &sidechainTarget);
            ui_col_float(&c, "Depth", &sidechainDepth, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Attack", &sidechainAttack, 0.002f, 0.001f, 0.05f);
            ui_col_float(&c, "Release", &sidechainRelease, 0.02f, 0.05f, 0.5f);
        }
        ui_col_space(&c, 4);
        ui_col_float_p(&c, "DrumVol", &drumVolume, 0.05f, 0.0f, 1.0f);
    }

    (void)w; (void)h;
}

// ============================================================================
// PARAMS: BUS FX - 7 buses + master as strips
// ============================================================================

static void drawParamBus(float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    int nBuses = 7;
    int fs = 14; // compact font size
    int row = fs + 2; // row height
    float stripW = w / (nBuses + 1);
    if (stripW > 150) stripW = 150;

    for (int b = 0; b < nBuses; b++) {
        float sx = x + b * stripW;
        bool isSel = (selectedBus == b);

        // Bus header
        Rectangle nameR = {sx, y, stripW-2, 14};
        bool nameHov = CheckCollisionPointRec(mouse, nameR);
        Color hdrBg = isSel ? (Color){50,55,68,255} : (nameHov ? (Color){40,42,52,255} : (Color){30,31,38,255});
        DrawRectangleRec(nameR, hdrBg);
        if (isSel) DrawRectangle((int)sx, (int)y+12, (int)stripW-2, 2, ORANGE);
        bool isDrum = (b < 4);
        DrawTextShadow(busNames[b], (int)sx+4, (int)y+1, 10, isSel ? ORANGE : (isDrum ? LIGHTGRAY : (Color){140,180,255,255}));

        if (nameHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedBus = (selectedBus == b) ? -1 : b;
            ui_consume_click();
        }

        float cy = y + 16;

        float rightX = sx + 4;
        float rightW = stripW - 10;

        // Pan, Rev, then FX
        float ry = cy;
        float barH = 8;

        // Pan bar (center-notched, visible track)
        {
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, (Color){32,36,45,255}); // visible track color
            int panCenter = (int)(rightX + rightW * 0.5f);
            DrawLine(panCenter, (int)ry, panCenter, (int)(ry+barH), (Color){55,60,72,255});
            float panNorm = (busPan[b] + 1.0f) * 0.5f;
            int panPos = (int)(rightX + panNorm * rightW);
            if (panPos > panCenter) {
                DrawRectangle(panCenter, (int)ry+1, panPos-panCenter, (int)barH-2, (Color){80,120,180,255});
            } else {
                DrawRectangle(panPos, (int)ry+1, panCenter-panPos, (int)barH-2, (Color){80,120,180,255});
            }
            DrawLine(panPos, (int)ry, panPos, (int)(ry+barH), WHITE);
            Rectangle panR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, panR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                busPan[b] = ((mouse.x - rightX) / rightW) * 2.0f - 1.0f;
                if (busPan[b] < -1) busPan[b] = -1;
                if (busPan[b] > 1) busPan[b] = 1;
            }
            DrawTextShadow("Pan", (int)rightX, (int)(ry+barH+1), 11, (Color){60,60,70,255});
            ry += barH + 12;
        }

        // Rev send bar
        {
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, (Color){35,28,50,255}); // visible track
            float revFill = busReverbSend[b] * rightW;
            DrawRectangle((int)rightX, (int)ry, (int)revFill, (int)barH, (Color){80,60,140,255});
            Rectangle revR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, revR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                busReverbSend[b] = (mouse.x - rightX) / rightW;
                if (busReverbSend[b] < 0) busReverbSend[b] = 0;
                if (busReverbSend[b] > 1) busReverbSend[b] = 1;
            }
            DrawTextShadow("RevSend", (int)rightX, (int)(ry+barH+1), 11, (Color){60,60,70,255});
            ry += barH + 14;
        }

        // FX controls (right of fader, below pan/rev)
        // Filter
        ToggleBoolS(rightX, ry, "Filter", &busFilterOn[b], fs); ry += row;
        if (busFilterOn[b]) {
            float xySize = rightW * 0.45f;
            if (xySize > 50) xySize = 50;
            if (xySize < 30) xySize = 30;
            drawFilterXY(rightX, ry, xySize, &busFilterCut[b], &busFilterRes[b]);
            float textX = rightX + xySize + 4;
            int sfs = fs - 2;
            int srow = sfs + 4;
            DraggableFloatS(textX, ry, "Cut", &busFilterCut[b], 0.02f, 0.0f, 1.0f, sfs);
            DraggableFloatS(textX, ry + srow, "Res", &busFilterRes[b], 0.02f, 0.0f, 1.0f, sfs);
            CycleOptionS(textX, ry + srow*2, "Type", busFilterTypeNames, 3, &busFilterType[b], sfs);
            ry += xySize + 2;
        }
        ry += 2;

        // Distortion
        ToggleBoolS(rightX, ry, "Dist", &busDistOn[b], fs); ry += row;
        if (busDistOn[b]) {
            DraggableFloatS(rightX, ry, "Drive", &busDistDrive[b], 0.05f, 1.0f, 4.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &busDistMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Delay
        ToggleBoolS(rightX, ry, "Delay", &busDelayOn[b], fs); ry += row;
        if (busDelayOn[b]) {
            ToggleBoolS(rightX, ry, "Sync", &busDelaySync[b], fs); ry += row;
            if (busDelaySync[b]) { CycleOptionS(rightX, ry, "Div", delaySyncNames, 5, &busDelaySyncDiv[b], fs); ry += row; }
            else { DraggableFloatS(rightX, ry, "Time", &busDelayTime[b], 0.01f, 0.01f, 1.0f, fs); ry += row; }
            DraggableFloatS(rightX, ry, "FB", &busDelayFB[b], 0.02f, 0.0f, 0.8f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &busDelayMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }

        // Vertical separator
        if (b < nBuses - 1) {
            DrawLine((int)(sx+stripW-1), (int)y, (int)(sx+stripW-1), (int)(y+h), (Color){38,38,48,255});
        }
    }

    // Master strip
    float mx = x + nBuses * stripW;
    DrawRectangle((int)mx, (int)y, (int)stripW, 14, (Color){40,38,28,255});
    DrawTextShadow("Master", (int)mx+4, (int)y+1, 10, YELLOW);
    DrawLine((int)mx-1, (int)y, (int)mx-1, (int)(y+h), (Color){55,55,45,255});

    float mcy = y + 16;
    {
        float fW = stripW - 12;
        Rectangle fr = {mx+4, mcy+1, fW, 8};
        DrawRectangleRec(fr, (Color){20,20,25,255});
        DrawRectangle((int)(mx+4), (int)mcy+1, (int)(fW*masterVol), 8, (Color){170,150,50,255});
        if (CheckCollisionPointRec(mouse, fr) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            masterVol = (mouse.x - (mx+4)) / fW;
            if (masterVol < 0) masterVol = 0;
            if (masterVol > 1) masterVol = 1;
        }
        mcy += 12;
    }
    mcy += 4;

    // Scenes in master strip
    DrawTextShadow("Scenes:", (int)mx+4, (int)mcy, 9, (Color){140,140,200,255}); mcy += row;
    ToggleBoolS(mx+4, mcy, "XFade", &crossfaderEnabled, fs); mcy += row;
    if (crossfaderEnabled) {
        DraggableFloatS(mx+4, mcy, "Pos", &crossfaderPos, 0.02f, 0.0f, 1.0f, fs); mcy += row;
        DraggableIntS(mx+4, mcy, "A", &sceneA, 0.3f, 0, 7, fs); mcy += row;
        DraggableIntS(mx+4, mcy, "B", &sceneB, 0.3f, 0, 7, fs); mcy += row;
    }

    (void)h; (void)mcy;
}

// ============================================================================
// PARAMS: MASTER FX - Signal chain left to right
// ============================================================================

static void drawParamMasterFx(float x, float y, float w, float h) {
    // 6 effect blocks in signal chain order
    float blockW = w / 6.0f;
    if (blockW > 190) blockW = 190;

    // Draw signal flow arrows between blocks
    for (int i = 0; i < 5; i++) {
        float ax = x + (i+1)*blockW - 6;
        DrawTextShadow(">", (int)ax, (int)(y + h*0.5f - 5), 12, (Color){50,50,60,255});
    }

    // Signal chain label
    DrawTextShadow("Signal: Dist > Crush > Chorus > Tape > Delay > Reverb",
                   (int)x, (int)(y+h-14), 9, (Color){55,55,65,255});

    // Block 1: Distortion
    {
        UIColumn c = ui_column(x+4, y, 16);
        ui_col_sublabel(&c, "Distortion:", ORANGE);
        ui_col_toggle(&c, "On", &distOn);
        ui_col_float(&c, "Drive", &distDrive, 0.5f, 1.0f, 20.0f);
        ui_col_float(&c, "Tone", &distTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &distMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+blockW), (int)y, (int)(x+blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 2: Bitcrusher
    {
        UIColumn c = ui_column(x+blockW+4, y, 16);
        ui_col_sublabel(&c, "Bitcrusher:", ORANGE);
        ui_col_toggle(&c, "On", &crushOn);
        ui_col_float(&c, "Bits", &crushBits, 0.5f, 2.0f, 16.0f);
        ui_col_float(&c, "Rate", &crushRate, 1.0f, 1.0f, 32.0f);
        ui_col_float(&c, "Mix", &crushMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+2*blockW), (int)y, (int)(x+2*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 3: Chorus
    {
        UIColumn c = ui_column(x+2*blockW+4, y, 16);
        ui_col_sublabel(&c, "Chorus:", ORANGE);
        ui_col_toggle(&c, "On", &chorusOn);
        ui_col_float(&c, "Rate", &chorusRate, 0.1f, 0.1f, 5.0f);
        ui_col_float(&c, "Depth", &chorusDepth, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &chorusMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+3*blockW), (int)y, (int)(x+3*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 4: Tape
    {
        UIColumn c = ui_column(x+3*blockW+4, y, 16);
        ui_col_sublabel(&c, "Tape:", ORANGE);
        ui_col_toggle(&c, "On", &tapeOn);
        ui_col_float(&c, "Saturat", &tapeSaturation, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Wow", &tapeWow, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Flutter", &tapeFlutter, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Hiss", &tapeHiss, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+4*blockW), (int)y, (int)(x+4*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 5: Delay
    {
        UIColumn c = ui_column(x+4*blockW+4, y, 16);
        ui_col_sublabel(&c, "Delay:", ORANGE);
        ui_col_toggle(&c, "On", &delayOn);
        ui_col_float(&c, "Time", &delayTime, 0.05f, 0.05f, 1.0f);
        ui_col_float(&c, "Fdbk", &delayFeedback, 0.05f, 0.0f, 0.9f);
        ui_col_float(&c, "Tone", &delayTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &delayMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+5*blockW), (int)y, (int)(x+5*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 6: Reverb
    {
        UIColumn c = ui_column(x+5*blockW+4, y, 16);
        ui_col_sublabel(&c, "Reverb:", ORANGE);
        ui_col_toggle(&c, "On", &reverbOn);
        ui_col_float(&c, "Size", &reverbSize, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Damping", &reverbDamping, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "PreDly", &reverbPreDelay, 0.005f, 0.0f, 0.1f);
        ui_col_float(&c, "Mix", &reverbMix, 0.05f, 0.0f, 1.0f);
    }

    // Presets row at bottom
    float preY = y + h - 36;
    DrawLine((int)x, (int)preY-4, (int)(x+w), (int)preY-4, (Color){40,40,50,255});
    float px = x + 4;
    DrawTextShadow("Presets:", (int)px, (int)preY, 10, (Color){80,80,95,255});
    px += 60;
    const char* presets[] = {"Clean", "9-Bit", "Wobbly", "Toy", "Tape+Chorus", "Lo-Fi"};
    for (int i = 0; i < 6; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "[%s]", presets[i]);
        int btnW = MeasureTextUI(buf, 18) + 10;
        if (PushButton(px, preY, presets[i])) { /* preset logic */ }
        px += btnW + 8;
    }
}

// ============================================================================
// PARAMS: TAPE/DUB - Two halves
// ============================================================================

static void drawParamTape(float x, float y, float w, float h) {
    float halfW = w * 0.55f;

    // Left: Dub Loop
    {
        UIColumn c = ui_column(x+4, y, 16);
        ui_col_sublabel(&c, "Dub Loop:", ORANGE);
        ui_col_toggle(&c, "On", &dubLoopEnabled);
        ui_col_float(&c, "Time", &dubHeadTime, 0.05f, 0.05f, 4.0f);
        ui_col_float(&c, "Fdbk", &dubFeedback, 0.05f, 0.0f, 0.95f);
        ui_col_float(&c, "Mix", &dubMix, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 2);
        ui_col_cycle(&c, "Input", dubInputNames, 4, &dubInputSource);
        ui_col_toggle(&c, "PreReverb", &dubPreReverb);
        ui_col_space(&c, 2);

        ui_col_sublabel(&c, "Degradation:", (Color){140,140,200,255});
        ui_col_float(&c, "Saturat", &dubSaturation, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "ToneHi", &dubToneHigh, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Noise", &dubNoise, 0.02f, 0.0f, 0.5f);
        ui_col_float(&c, "Degrade", &dubDegradeRate, 0.02f, 0.0f, 0.5f);

        // Second column for modulation + buttons
        UIColumn c2 = ui_column(x + 180, y, 16);
        ui_col_sublabel(&c2, "Modulation:", (Color){140,140,200,255});
        ui_col_float(&c2, "Wow", &dubWow, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Flutter", &dubFlutter, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Drift", &dubDrift, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Speed", &dubSpeedTarget, 0.1f, 0.25f, 2.0f);
        ui_col_space(&c2, 4);
        ui_col_button(&c2, "Throw");
        ui_col_button(&c2, "Cut");
        ui_col_button(&c2, "1/2 Speed");
        ui_col_button(&c2, "Normal");
    }

    // Divider
    DrawLine((int)(x+halfW), (int)y, (int)(x+halfW), (int)(y+h), (Color){45,45,55,255});

    // Right: Rewind
    {
        UIColumn c = ui_column(x+halfW+8, y, 16);
        ui_col_sublabel(&c, "Rewind:", ORANGE);
        ui_col_float(&c, "Time", &rewindTime, 0.1f, 0.3f, 3.0f);
        ui_col_cycle(&c, "Curve", rewindCurveNames, 3, &rewindCurve);
        ui_col_float(&c, "MinSpd", &rewindMinSpeed, 0.05f, 0.05f, 0.8f);
        ui_col_space(&c, 2);
        ui_col_float(&c, "Vinyl", &rewindVinyl, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Wobble", &rewindWobble, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Filter", &rewindFilter, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 4);
        ui_col_button(&c, isRewinding ? ">>..." : "Rewind");
    }

    (void)h;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth DAW (Horizontal)");
    SetTargetFPS(60);

    Font font = LoadEmbeddedFont();
    ui_init(&font);
    initPatches();

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) playing = !playing;

        BeginDrawing();
        ClearBackground((Color){22, 22, 28, 255});
        ui_begin_frame();

        // === SIDEBAR ===
        drawSidebar();

        // === TRANSPORT ===
        drawTransport();

        // === WORKSPACE TAB BAR ===
        int workInt = (int)workTab;
        drawTabBar(CONTENT_X, WORK_TAB_Y, CONTENT_W, workTabNames, workTabKeys, WORK_COUNT, &workInt, "F");
        workTab = (WorkTab)workInt;

        // === WORKSPACE ===
        {
            float wx = CONTENT_X + 6, wy = WORK_Y + 4;
            float ww = CONTENT_W - 12, wh = WORK_H - 8;
            switch (workTab) {
                case WORK_SEQ:   drawWorkSeq(wx, wy, ww, wh); break;
                case WORK_PIANO: drawWorkPiano(wx, wy, ww, wh); break;
                case WORK_SONG:  drawWorkSong(wx, wy, ww, wh); break;
                default: break;
            }
        }

        // === SPLIT LINE ===
        DrawLine(CONTENT_X, SPLIT_Y, SCREEN_WIDTH, SPLIT_Y, (Color){55, 55, 65, 255});

        // === PARAM TAB BAR ===
        int paramInt = (int)paramTab;
        drawTabBar(CONTENT_X, PARAM_TAB_Y, CONTENT_W, paramTabNames, paramTabKeys, PARAM_COUNT, &paramInt, "");
        paramTab = (ParamTab)paramInt;

        // === PARAMS ===
        {
            float px = CONTENT_X + 4, py = PARAM_Y + 4;
            float pw = CONTENT_W - 8, ph = PARAM_H - 8;

            BeginScissorMode((int)px, PARAM_Y, (int)pw + 4, PARAM_H);
            switch (paramTab) {
                case PARAM_PATCH:  drawParamPatch(px, py, pw, ph); break;
                case PARAM_DRUMS:  drawParamDrums(px, py, pw, ph); break;
                case PARAM_BUS:    drawParamBus(px, py, pw, ph); break;
                case PARAM_MASTER: drawParamMasterFx(px, py, pw, ph); break;
                case PARAM_TAPE:   drawParamTape(px, py, pw, ph); break;
                default: break;
            }
            EndScissorMode();
        }

        ui_update();
        DrawTooltip();
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
