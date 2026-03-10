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
static const char* rootNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char* scaleNames[] = {"Chromatic", "Major", "Minor", "Pentatonic", "Blues",
    "Dorian", "Mixolydian", "HarmMinor", "Phrygian"};
static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};
static const char* lfoSyncNames[] = {"Off", "4bar", "2bar", "1bar", "1/2", "1/4", "1/8", "1/16"};

typedef struct {
    char name[16];
    int wave;
    float attack, decay, sustain, release;
    bool expRelease;
    float vibratoRate, vibratoDepth;
    float filterCutoff, filterResonance;
    float filterEnvAmt, filterEnvAttack, filterEnvDecay;
    float volume;
    bool monoMode;
    float glideTime;
    int unisonCount;
    float unisonDetune;
    bool arpEnabled;
    int arpMode, arpChord, arpRateDiv;
    float arpRate;
    bool scaleLockEnabled;
    int scaleRoot, scaleType;
    float pulseWidth, pwmRate, pwmDepth;
    int scwIndex;
    int voiceVowel;
    bool voiceRandomVowel;
    float voicePitch, voiceSpeed, voiceFormantShift;
    float voiceBreathiness, voiceBuzziness;
    bool voiceConsonant, voiceNasal;
    float voiceConsonantAmt, voiceNasalAmt;
    float voicePitchEnv, voicePitchEnvTime, voicePitchEnvCurve;
    float pluckBrightness, pluckDamping, pluckDamp;
    int additivePreset;
    float additiveBrightness, additiveShimmer, additiveInharmonicity;
    int malletPreset;
    float malletStiffness, malletHardness, malletStrikePos;
    float malletResonance, malletDamp;
    float malletTremolo, malletTremoloRate;
    int granularScwIndex;
    float granularGrainSize, granularDensity;
    float granularPosition, granularPosRandom;
    float granularPitch, granularPitchRandom, granularAmpRandom;
    bool granularFreeze;
    float fmModRatio, fmModIndex, fmFeedback;
    int pdWaveType;
    float pdDistortion;
    int membranePreset;
    float membraneDamping, membraneStrike;
    float membraneBend, membraneBendDecay;
    int birdType;
    float birdChirpRange, birdHarmonics;
    float birdTrillRate, birdTrillDepth;
    float birdAmRate, birdAmDepth;
    float lfoFilterRate, lfoFilterDepth;
    int lfoFilterShape, lfoFilterSync;
    float lfoResoRate, lfoResoDepth;
    int lfoResoShape;
    float lfoAmpRate, lfoAmpDepth;
    int lfoAmpShape;
    float lfoPitchRate, lfoPitchDepth;
    int lfoPitchShape;
} SynthPatch;

static SynthPatch defaultPatch(void) {
    return (SynthPatch){
        .name = "Init", .wave = 0,
        .attack = 0.01f, .decay = 0.1f, .sustain = 0.5f, .release = 0.3f, .expRelease = false,
        .vibratoRate = 5.0f, .vibratoDepth = 0.0f,
        .filterCutoff = 1.0f, .filterResonance = 0.0f,
        .filterEnvAmt = 0.0f, .filterEnvAttack = 0.01f, .filterEnvDecay = 0.2f,
        .volume = 0.8f, .monoMode = false, .glideTime = 0.1f,
        .unisonCount = 1, .unisonDetune = 10.0f,
        .arpEnabled = false, .arpMode = 0, .arpChord = 0, .arpRateDiv = 2, .arpRate = 8.0f,
        .scaleLockEnabled = false, .scaleRoot = 0, .scaleType = 0,
        .pulseWidth = 0.5f, .pwmRate = 2.0f, .pwmDepth = 0.1f,
        .scwIndex = 0,
        .voiceVowel = 0, .voiceRandomVowel = false,
        .voicePitch = 1.0f, .voiceSpeed = 10.0f, .voiceFormantShift = 1.0f,
        .voiceBreathiness = 0.0f, .voiceBuzziness = 0.3f,
        .voiceConsonant = false, .voiceNasal = false,
        .voiceConsonantAmt = 0.3f, .voiceNasalAmt = 0.3f,
        .voicePitchEnv = 0.0f, .voicePitchEnvTime = 0.1f, .voicePitchEnvCurve = 0.0f,
        .pluckBrightness = 0.5f, .pluckDamping = 0.998f, .pluckDamp = 0.3f,
        .additivePreset = 0, .additiveBrightness = 0.8f, .additiveShimmer = 0.0f, .additiveInharmonicity = 0.0f,
        .malletPreset = 0, .malletStiffness = 0.5f, .malletHardness = 0.5f, .malletStrikePos = 0.3f,
        .malletResonance = 0.5f, .malletDamp = 0.3f, .malletTremolo = 0.0f, .malletTremoloRate = 5.0f,
        .granularScwIndex = 0, .granularGrainSize = 50.0f, .granularDensity = 20.0f,
        .granularPosition = 0.5f, .granularPosRandom = 0.1f,
        .granularPitch = 1.0f, .granularPitchRandom = 0.0f, .granularAmpRandom = 0.0f,
        .granularFreeze = false,
        .fmModRatio = 2.0f, .fmModIndex = 1.0f, .fmFeedback = 0.0f,
        .pdWaveType = 0, .pdDistortion = 0.5f,
        .membranePreset = 0, .membraneDamping = 0.5f, .membraneStrike = 0.5f,
        .membraneBend = 0.1f, .membraneBendDecay = 0.1f,
        .birdType = 0, .birdChirpRange = 1.0f, .birdHarmonics = 0.3f,
        .birdTrillRate = 12.0f, .birdTrillDepth = 1.0f, .birdAmRate = 5.0f, .birdAmDepth = 0.3f,
    };
}

static int selectedPatch = 0;
static SynthPatch patches[NUM_PATCHES];
static bool patchesInit = false;
static float masterVolume = 0.8f;

static void initPatches(void) {
    if (patchesInit) return;
    for (int i = 0; i < NUM_PATCHES; i++) patches[i] = defaultPatch();
    snprintf(patches[0].name, 16, "Bass");       patches[0].wave = 0;
    snprintf(patches[1].name, 16, "Lead");        patches[1].wave = 1;
    snprintf(patches[2].name, 16, "Chord");       patches[2].wave = 7;
    snprintf(patches[3].name, 16, "Voice");       patches[3].wave = 5;
    snprintf(patches[4].name, 16, "Pluck");       patches[4].wave = 6;
    snprintf(patches[5].name, 16, "FM Bell");     patches[5].wave = 10;
    snprintf(patches[6].name, 16, "Mallet");      patches[6].wave = 8;
    snprintf(patches[7].name, 16, "Bird");        patches[7].wave = 13;
    patches[0].monoMode = true; patches[0].filterCutoff = 0.4f;
    patches[1].unisonCount = 2; patches[1].vibratoDepth = 0.3f;
    patches[2].attack = 0.05f; patches[2].release = 0.8f;
    patches[3].voiceVowel = 2; patches[3].voiceBreathiness = 0.2f;
    patches[5].fmModRatio = 3.5f; patches[5].fmModIndex = 2.0f;
    patches[7].birdType = 4;
    patchesInit = true;
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
static const char* filterTypeNames[] = {"LP", "HP", "BP"};
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

    // Patch slots
    DrawTextShadow("Patch", (int)x, (int)y, 9, (Color){70,70,85,255});
    y += 12;

    for (int i = 0; i < NUM_PATCHES; i++) {
        Rectangle r = {x, y, SIDEBAR_W-8, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool sel = (i == selectedPatch);
        Color bg = sel ? (Color){50, 55, 68, 255} : (hov ? (Color){38, 40, 48, 255} : (Color){24, 25, 30, 255});
        DrawRectangleRec(r, bg);
        if (sel) DrawRectangle((int)x, (int)y, 2, 16, ORANGE);

        // Colored dot
        Color dot = sel ? ORANGE : (Color){60, 70, 90, 255};
        DrawCircle((int)x+9, (int)y+8, 3, dot);

        // Short name (truncate to 4 chars)
        char short_name[6];
        strncpy(short_name, patches[i].name, 5);
        short_name[5] = '\0';
        DrawTextShadow(short_name, (int)x+16, (int)y+3, 10, sel ? WHITE : GRAY);

        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedPatch = i;
            if (paramTab != PARAM_PATCH) paramTab = PARAM_PATCH;
            ui_consume_click();
        }
        y += 18;
    }

    // Divider
    y += 4;
    DrawLine((int)x, (int)y, SIDEBAR_W-4, (int)y, (Color){42,42,52,255});
    y += 6;

    // Drum track mutes
    DrawTextShadow("Drums", (int)x, (int)y, 9, (Color){70,70,85,255});
    y += 12;

    const char* drumShort[] = {"K", "S", "H", "C"};
    for (int i = 0; i < 4; i++) {
        Rectangle r = {x, y, SIDEBAR_W-8, 14};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawTextShadow(drumShort[i], (int)x+4, (int)y+2, 10, LIGHTGRAY);

        // Mute toggle
        Rectangle mr = {x+18, y+1, 12, 12};
        bool mHov = CheckCollisionPointRec(mouse, mr);
        Color mc = busMute[i] ? (Color){150,50,50,255} : (mHov ? (Color){50,90,50,255} : (Color){40,80,40,255});
        DrawRectangleRec(mr, mc);
        DrawTextShadow(busMute[i] ? "M" : "", (int)x+19, (int)y+2, 9, WHITE);

        // Tiny level bar
        float barX = x + 34, barW = SIDEBAR_W - 42;
        DrawRectangle((int)barX, (int)y+3, (int)barW, 8, (Color){20,20,25,255});
        float fill = busVolumes[i] * barW;
        DrawRectangle((int)barX, (int)y+3, (int)fill, 8, busMute[i] ? (Color){80,40,40,255} : (Color){50,110,50,255});

        if (mHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busMute[i] = !busMute[i]; ui_consume_click(); }
        else if (hov && !mHov && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            busVolumes[i] = (mouse.x - barX) / barW;
            if (busVolumes[i] < 0) busVolumes[i] = 0;
            if (busVolumes[i] > 1) busVolumes[i] = 1;
        }
        y += 16;
    }

    // Melody bus mutes
    y += 2;
    const char* melShort[] = {"B", "L", "C"};
    for (int i = 0; i < 3; i++) {
        int bi = 4 + i;
        DrawTextShadow(melShort[i], (int)x+4, (int)y+2, 10, (Color){140,180,255,255});

        Rectangle mr = {x+18, y+1, 12, 12};
        bool mHov = CheckCollisionPointRec(mouse, mr);
        Color mc = busMute[bi] ? (Color){150,50,50,255} : (mHov ? (Color){50,90,50,255} : (Color){40,80,40,255});
        DrawRectangleRec(mr, mc);
        if (busMute[bi]) DrawTextShadow("M", (int)x+19, (int)y+2, 9, WHITE);

        float barX = x + 34, barW = SIDEBAR_W - 42;
        DrawRectangle((int)barX, (int)y+3, (int)barW, 8, (Color){20,20,25,255});
        float fill = busVolumes[bi] * barW;
        DrawRectangle((int)barX, (int)y+3, (int)fill, 8, busMute[bi] ? (Color){80,40,40,255} : (Color){50,80,140,255});

        if (mHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busMute[bi] = !busMute[bi]; ui_consume_click(); }
        y += 16;
    }

    // Divider
    y += 4;
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
    int labelW = 42;
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

        DrawTextShadow(trackNames[track], (int)x+2, ty+cellH/2-5, steps==32 ? 9 : 10,
                       isDrum ? LIGHTGRAY : (Color){140,180,255,255});

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
    SynthPatch *p = &patches[selectedPatch];

    float col1X = x, col1W = 260;
    float col2X = x + 268;
    float col3X = x + 528;
    float col4X = x + 778;

    // COL 1: Oscillator + wave-specific
    {
        UIColumn c = ui_column(col1X + 8, y, 16);
        ui_col_sublabel(&c, TextFormat("OSC: %s [%s]", patches[selectedPatch].name, waveNames[p->wave]), ORANGE);
        c.y += drawWaveSelector(c.x, c.y, col1W - 16, &p->wave);
        ui_col_space(&c, 2);

        if (p->wave == 0) {
            ui_col_sublabel(&c, "PWM:", (Color){140,160,200,255});
            ui_col_float_p(&c, "Width", &p->pulseWidth, 0.05f, 0.1f, 0.9f);
            ui_col_float(&c, "Rate", &p->pwmRate, 0.5f, 0.1f, 20.0f);
            ui_col_float(&c, "Depth", &p->pwmDepth, 0.02f, 0.0f, 0.4f);
        } else if (p->wave == 4) {
            ui_col_sublabel(&c, "Wavetable:", (Color){140,160,200,255});
            ui_col_int(&c, "SCW", &p->scwIndex, 0.3f, 0, 20);
        } else if (p->wave == 5) {
            ui_col_sublabel(&c, "Formant:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Vowel", vowelNames, 5, &p->voiceVowel);
            ui_col_toggle(&c, "Random", &p->voiceRandomVowel);
            ui_col_float(&c, "Pitch", &p->voicePitch, 0.1f, 0.3f, 2.0f);
            ui_col_float(&c, "Speed", &p->voiceSpeed, 1.0f, 4.0f, 20.0f);
            ui_col_float(&c, "Formant", &p->voiceFormantShift, 0.05f, 0.5f, 1.5f);
            ui_col_float(&c, "Breath", &p->voiceBreathiness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Buzz", &p->voiceBuzziness, 0.05f, 0.0f, 1.0f);
            ui_col_toggle(&c, "Consonant", &p->voiceConsonant);
            if (p->voiceConsonant) ui_col_float(&c, "ConsAmt", &p->voiceConsonantAmt, 0.05f, 0.0f, 1.0f);
            ui_col_toggle(&c, "Nasal", &p->voiceNasal);
            if (p->voiceNasal) ui_col_float(&c, "NasalAmt", &p->voiceNasalAmt, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Pitch Env:", (Color){120,140,180,255});
            ui_col_float(&c, "Bend", &p->voicePitchEnv, 0.5f, -12.0f, 12.0f);
            ui_col_float(&c, "Time", &p->voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
            ui_col_float(&c, "Curve", &p->voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
        } else if (p->wave == 6) {
            ui_col_sublabel(&c, "Pluck:", (Color){140,160,200,255});
            ui_col_float(&c, "Bright", &p->pluckBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Sustain", &p->pluckDamping, 0.0002f, 0.995f, 0.9998f);
            ui_col_float(&c, "Damp", &p->pluckDamp, 0.05f, 0.0f, 1.0f);
        } else if (p->wave == 7) {
            ui_col_sublabel(&c, "Additive:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", additivePresetNames, 5, &p->additivePreset);
            ui_col_float(&c, "Bright", &p->additiveBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Shimmer", &p->additiveShimmer, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Inharm", &p->additiveInharmonicity, 0.005f, 0.0f, 0.1f);
        } else if (p->wave == 8) {
            ui_col_sublabel(&c, "Mallet:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", malletPresetNames, 5, &p->malletPreset);
            ui_col_float(&c, "Stiff", &p->malletStiffness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Hard", &p->malletHardness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Strike", &p->malletStrikePos, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Reson", &p->malletResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Damp", &p->malletDamp, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Tremolo", &p->malletTremolo, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "TremSpd", &p->malletTremoloRate, 0.5f, 1.0f, 12.0f);
        } else if (p->wave == 9) {
            ui_col_sublabel(&c, "Granular:", (Color){140,160,200,255});
            ui_col_int(&c, "Source", &p->granularScwIndex, 0.3f, 0, 20);
            ui_col_float(&c, "Size", &p->granularGrainSize, 5.0f, 10.0f, 200.0f);
            ui_col_float(&c, "Density", &p->granularDensity, 2.0f, 1.0f, 100.0f);
            ui_col_float(&c, "Pos", &p->granularPosition, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "PosRnd", &p->granularPosRandom, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Pitch", &p->granularPitch, 0.1f, 0.25f, 4.0f);
            ui_col_float(&c, "PitRnd", &p->granularPitchRandom, 0.5f, 0.0f, 12.0f);
            ui_col_toggle(&c, "Freeze", &p->granularFreeze);
        } else if (p->wave == 10) {
            ui_col_sublabel(&c, "FM Synth:", (Color){140,160,200,255});
            ui_col_float(&c, "Ratio", &p->fmModRatio, 0.5f, 0.5f, 16.0f);
            ui_col_float(&c, "Index", &p->fmModIndex, 0.1f, 0.0f, 10.0f);
            ui_col_float(&c, "Feedback", &p->fmFeedback, 0.05f, 0.0f, 1.0f);
        } else if (p->wave == 11) {
            ui_col_sublabel(&c, "Phase Dist:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Wave", pdWaveNames, 5, &p->pdWaveType);
            ui_col_float(&c, "Distort", &p->pdDistortion, 0.05f, 0.0f, 1.0f);
        } else if (p->wave == 12) {
            ui_col_sublabel(&c, "Membrane:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", membranePresetNames, 5, &p->membranePreset);
            ui_col_float(&c, "Damping", &p->membraneDamping, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Strike", &p->membraneStrike, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bend", &p->membraneBend, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "BndDcy", &p->membraneBendDecay, 0.01f, 0.02f, 0.3f);
        } else if (p->wave == 13) {
            ui_col_sublabel(&c, "Bird:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Type", birdTypeNames, 5, &p->birdType);
            ui_col_float(&c, "Range", &p->birdChirpRange, 0.1f, 0.5f, 2.0f);
            ui_col_float(&c, "Harmon", &p->birdHarmonics, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Trill:", (Color){120,140,180,255});
            ui_col_float(&c, "Rate", &p->birdTrillRate, 1.0f, 0.0f, 30.0f);
            ui_col_float(&c, "Depth", &p->birdTrillDepth, 0.2f, 0.0f, 5.0f);
            ui_col_sublabel(&c, "Flutter:", (Color){120,140,180,255});
            ui_col_float(&c, "AM Rate", &p->birdAmRate, 1.0f, 0.0f, 20.0f);
            ui_col_float(&c, "AM Dep", &p->birdAmDepth, 0.05f, 0.0f, 1.0f);
        }
    }

    // Vertical divider
    DrawLine((int)col2X-4, (int)y, (int)col2X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 2: Envelope + Filter
    {
        float envX = col2X, envW = 240;

        // ADSR curve
        UIColumn c = ui_column(envX, y, 16);
        ui_col_sublabel(&c, "Envelope:", ORANGE);
        c.y += drawADSRCurve(envX, c.y, envW, 55, &p->attack, &p->decay, &p->sustain, &p->release, p->expRelease);

        ui_col_float_p(&c, "Atk", &p->attack, 0.5f, 0.001f, 2.0f);
        ui_col_float_p(&c, "Dec", &p->decay, 0.5f, 0.0f, 2.0f);
        ui_col_float(&c, "Sus", &p->sustain, 0.5f, 0.0f, 1.0f);
        ui_col_float(&c, "Rel", &p->release, 0.5f, 0.01f, 3.0f);
        ui_col_toggle(&c, "Exp Release", &p->expRelease);
        ui_col_space(&c, 6);

        // Filter
        ui_col_sublabel(&c, "Filter:", ORANGE);
        float filtY = c.y;
        ui_col_float_p(&c, "Cut", &p->filterCutoff, 0.05f, 0.01f, 1.0f);
        ui_col_float_p(&c, "Res", &p->filterResonance, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "EnvAmt", &p->filterEnvAmt, 0.05f, -1.0f, 1.0f);
        ui_col_float(&c, "EnvAtk", &p->filterEnvAttack, 0.01f, 0.001f, 0.5f);
        ui_col_float(&c, "EnvDcy", &p->filterEnvDecay, 0.05f, 0.01f, 2.0f);

        // XY pad next to filter sliders
        float padSize = c.y - filtY - 4;
        if (padSize < 50) padSize = 50;
        if (padSize > 90) padSize = 90;
        drawFilterXY(envX + envW - padSize - 4, filtY, padSize, &p->filterCutoff, &p->filterResonance);
    }

    // Vertical divider
    DrawLine((int)col3X-4, (int)y, (int)col3X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 3: Voice params (vibrato, volume, mono, unison, arp, scale)
    {
        UIColumn c = ui_column(col3X, y, 16);

        ui_col_sublabel(&c, "Vibrato:", ORANGE);
        ui_col_float(&c, "Rate", &p->vibratoRate, 0.5f, 0.5f, 15.0f);
        ui_col_float(&c, "Depth", &p->vibratoDepth, 0.2f, 0.0f, 2.0f);
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Volume:", ORANGE);
        ui_col_float_p(&c, "Note", &p->volume, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Master", &masterVolume, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 3);

        if (p->wave != 6 && p->wave != 8) {
            ui_col_sublabel(&c, "Mono/Glide:", ORANGE);
            ui_col_toggle(&c, "Mono", &p->monoMode);
            if (p->monoMode) ui_col_float(&c, "Glide", &p->glideTime, 0.02f, 0.01f, 1.0f);
            ui_col_space(&c, 3);
        }

        if (p->wave <= 2) {
            ui_col_sublabel(&c, "Unison:", ORANGE);
            ui_col_int(&c, "Count", &p->unisonCount, 1, 1, 4);
            if (p->unisonCount > 1) ui_col_float(&c, "Detune", &p->unisonDetune, 1.0f, 0.0f, 50.0f);
            ui_col_space(&c, 3);
        }

        ui_col_sublabel(&c, "Arpeggiator:", ORANGE);
        ui_col_toggle(&c, "On", &p->arpEnabled);
        if (p->arpEnabled) {
            ui_col_cycle(&c, "Mode", arpModeNames, 4, &p->arpMode);
            ui_col_cycle(&c, "Chord", arpChordNames, 7, &p->arpChord);
            ui_col_cycle(&c, "Rate", arpRateNames, 5, &p->arpRateDiv);
            if (p->arpRateDiv == 4) ui_col_float(&c, "Hz", &p->arpRate, 0.5f, 1.0f, 30.0f);
        }
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Scale Lock:", ORANGE);
        ui_col_toggle(&c, "On", &p->scaleLockEnabled);
        if (p->scaleLockEnabled) {
            ui_col_cycle(&c, "Root", rootNoteNames, 12, &p->scaleRoot);
            ui_col_cycle(&c, "Scale", scaleNames, 9, &p->scaleType);
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
            {"Filter LFO", &p->lfoFilterRate, &p->lfoFilterDepth, &p->lfoFilterShape, &p->lfoFilterSync},
            {"Reso LFO",   &p->lfoResoRate,   &p->lfoResoDepth,   &p->lfoResoShape,   NULL},
            {"Amp LFO",    &p->lfoAmpRate,     &p->lfoAmpDepth,    &p->lfoAmpShape,    NULL},
            {"Pitch LFO",  &p->lfoPitchRate,   &p->lfoPitchDepth,  &p->lfoPitchShape,  NULL},
        };

        float ly = y;
        for (int i = 0; i < 4; i++) {
            UIColumn c = ui_column(col4X, ly, 15);
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

    (void)h;
}

// ============================================================================
// PARAMS: DRUMS - Horizontal columns
// ============================================================================

static void drawParamDrums(float x, float y, float w, float h) {
    // Row 1: Main drums + volume + sidechain
    float colW = 140;
    float cols[] = {x, x+colW, x+2*colW, x+3*colW, x+4*colW, x+5*colW, x+6*colW};

    // Volume
    {
        UIColumn c = ui_column(cols[0]+4, y, 16);
        ui_col_sublabel(&c, "Drums:", ORANGE);
        ui_col_float_p(&c, "Volume", &drumVolume, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 4);

        ui_col_sublabel(&c, "Groove:", (Color){140,140,200,255});
        ui_col_int(&c, "Kick", &grooveKickNudge, 0.3f, -12, 12);
        ui_col_int(&c, "Snare", &grooveSnareDelay, 0.3f, -12, 12);
        ui_col_int(&c, "HH", &grooveHatNudge, 0.3f, -12, 12);
        ui_col_int(&c, "Swing", &grooveSwing, 0.3f, 0, 12);
        ui_col_int(&c, "Jitter", &grooveJitter, 0.3f, 0, 6);
    }

    // Kick
    {
        UIColumn c = ui_column(cols[1]+4, y, 16);
        ui_col_sublabel(&c, "Kick:", ORANGE);
        ui_col_float_p(&c, "Pitch", &kickPitch, 3.0f, 30.0f, 100.0f);
        ui_col_float_p(&c, "Decay", &kickDecay, 0.07f, 0.1f, 1.5f);
        ui_col_float_p(&c, "Punch", &kickPunchPitch, 10.0f, 80.0f, 300.0f);
        ui_col_float(&c, "Click", &kickClick, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "Tone", &kickTone, 0.05f, 0.0f, 1.0f);
    }

    // Snare
    {
        UIColumn c = ui_column(cols[2]+4, y, 16);
        ui_col_sublabel(&c, "Snare:", ORANGE);
        ui_col_float_p(&c, "Pitch", &snarePitch, 10.0f, 100.0f, 350.0f);
        ui_col_float_p(&c, "Decay", &snareDecay, 0.03f, 0.05f, 0.6f);
        ui_col_float(&c, "Snappy", &snareSnappy, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "Tone", &snareTone, 0.05f, 0.0f, 1.0f);
    }

    // Clap
    {
        UIColumn c = ui_column(cols[3]+4, y, 16);
        ui_col_sublabel(&c, "Clap:", ORANGE);
        ui_col_float_p(&c, "Decay", &clapDecay, 0.03f, 0.1f, 0.6f);
        ui_col_float_p(&c, "Tone", &clapTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Spread", &clapSpread, 0.001f, 0.005f, 0.03f);
    }

    // HiHat
    {
        UIColumn c = ui_column(cols[4]+4, y, 16);
        ui_col_sublabel(&c, "HiHat:", ORANGE);
        ui_col_float(&c, "Closed", &hhDecayClosed, 0.01f, 0.01f, 0.2f);
        ui_col_float(&c, "Open", &hhDecayOpen, 0.05f, 0.1f, 1.0f);
        ui_col_float_p(&c, "Tone", &hhTone, 0.05f, 0.0f, 1.0f);
    }

    // Sidechain
    {
        UIColumn c = ui_column(cols[5]+4, y, 16);
        ui_col_sublabel(&c, "Sidechain:", ORANGE);
        ui_col_toggle(&c, "On", &sidechainOn);
        if (sidechainOn) {
            ui_col_cycle(&c, "Source", sidechainSourceNames, 5, &sidechainSource);
            ui_col_cycle(&c, "Target", sidechainTargetNames, 4, &sidechainTarget);
            ui_col_float(&c, "Depth", &sidechainDepth, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Attack", &sidechainAttack, 0.002f, 0.001f, 0.05f);
            ui_col_float(&c, "Release", &sidechainRelease, 0.02f, 0.05f, 0.5f);
        }
    }

    // Row 2: Percussion + CR78
    float row2Y = y + h * 0.5f + 4;
    float pcols[] = {x, x+colW, x+colW*2, x+colW*3, x+colW*4, x+colW*5};

    // Toms
    {
        UIColumn c = ui_column(pcols[0]+4, row2Y, 16);
        ui_col_sublabel(&c, "Toms:", ORANGE);
        ui_col_float_p(&c, "Pitch", &tomPitch, 0.1f, 0.5f, 2.0f);
        ui_col_float_p(&c, "Decay", &tomDecay, 0.03f, 0.1f, 0.8f);
        ui_col_float(&c, "PnchDcy", &tomPunchDecay, 0.01f, 0.01f, 0.2f);
    }

    // Rimshot
    {
        UIColumn c = ui_column(pcols[1]+4, row2Y, 16);
        ui_col_sublabel(&c, "Rimshot:", ORANGE);
        ui_col_float_p(&c, "Pitch", &rimPitch, 100.0f, 800.0f, 3000.0f);
        ui_col_float_p(&c, "Decay", &rimDecay, 0.005f, 0.01f, 0.1f);
    }

    // Cowbell
    {
        UIColumn c = ui_column(pcols[2]+4, row2Y, 16);
        ui_col_sublabel(&c, "Cowbell:", ORANGE);
        ui_col_float_p(&c, "Pitch", &cowbellPitch, 20.0f, 400.0f, 1000.0f);
        ui_col_float_p(&c, "Decay", &cowbellDecay, 0.03f, 0.1f, 0.6f);
    }

    // Clave
    {
        UIColumn c = ui_column(pcols[3]+4, row2Y, 16);
        ui_col_sublabel(&c, "Clave:", ORANGE);
        ui_col_float_p(&c, "Pitch", &clavePitch, 100.0f, 1500.0f, 4000.0f);
        ui_col_float_p(&c, "Decay", &claveDecay, 0.005f, 0.01f, 0.1f);
    }

    // Maracas
    {
        UIColumn c = ui_column(pcols[4]+4, row2Y, 16);
        ui_col_sublabel(&c, "Maracas:", ORANGE);
        ui_col_float_p(&c, "Decay", &maracasDecay, 0.01f, 0.02f, 0.2f);
        ui_col_float_p(&c, "Tone", &maracasTone, 0.05f, 0.0f, 1.0f);
    }

    // CR-78
    {
        UIColumn c = ui_column(pcols[5]+4, row2Y, 16);
        ui_col_sublabel(&c, "CR-78:", (Color){180,140,100,255});
        ui_col_float(&c, "KPitch", &cr78KickPitch, 5.0f, 40.0f, 150.0f);
        ui_col_float(&c, "KDecay", &cr78KickDecay, 0.03f, 0.1f, 0.6f);
        ui_col_float(&c, "KReso", &cr78KickResonance, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "SPitch", &cr78SnarePitch, 10.0f, 100.0f, 350.0f);
        ui_col_float(&c, "SDecay", &cr78SnareDecay, 0.03f, 0.05f, 0.4f);
        ui_col_float(&c, "SSnap", &cr78SnareSnappy, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "HDecay", &cr78HHDecay, 0.01f, 0.01f, 0.2f);
        ui_col_float(&c, "HTone", &cr78HHTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "MetPch", &cr78MetalPitch, 50.0f, 400.0f, 1200.0f);
        ui_col_float(&c, "MetDcy", &cr78MetalDecay, 0.03f, 0.05f, 0.5f);
    }

    // Divider between rows
    DrawLine((int)x, (int)row2Y-4, (int)(x+w), (int)row2Y-4, (Color){45,45,55,255});
    (void)w;
}

// ============================================================================
// PARAMS: BUS FX - 7 buses + master as strips
// ============================================================================

static void drawParamBus(float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    int nBuses = 7;
    float stripW = w / (nBuses + 1); // +1 for master
    if (stripW > 150) stripW = 150;

    for (int b = 0; b < nBuses; b++) {
        float sx = x + b * stripW;
        bool isSel = (selectedBus == b);

        // Bus header
        Rectangle nameR = {sx, y, stripW-2, 18};
        bool nameHov = CheckCollisionPointRec(mouse, nameR);
        Color hdrBg = isSel ? (Color){50,55,68,255} : (nameHov ? (Color){40,42,52,255} : (Color){30,31,38,255});
        DrawRectangleRec(nameR, hdrBg);
        if (isSel) DrawRectangle((int)sx, (int)y+16, (int)stripW-2, 2, ORANGE);
        bool isDrum = (b < 4);
        DrawTextShadow(busNames[b], (int)sx+4, (int)y+3, 11, isSel ? ORANGE : (isDrum ? LIGHTGRAY : (Color){140,180,255,255}));

        if (nameHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedBus = (selectedBus == b) ? -1 : b;
            ui_consume_click();
        }

        UIColumn c = ui_column(sx+4, y+22, 15);

        // Volume fader
        {
            float fW = stripW - 40;
            if (fW < 30) fW = 30;
            Rectangle fr = {c.x, c.y+2, fW, 10};
            DrawRectangleRec(fr, (Color){20,20,25,255});
            float fill = fW * busVolumes[b];
            Color vc = busMute[b] ? (Color){80,40,40,255} : (Color){50,120,50,255};
            DrawRectangle((int)c.x, (int)c.y+2, (int)fill, 10, vc);
            if (CheckCollisionPointRec(mouse, fr) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                busVolumes[b] = (mouse.x - c.x) / fW;
                if (busVolumes[b] < 0) busVolumes[b] = 0;
                if (busVolumes[b] > 1) busVolumes[b] = 1;
            }
            // M/S buttons
            float btnX = c.x + fW + 2;
            Rectangle mR = {btnX, c.y, 14, 13};
            bool mH = CheckCollisionPointRec(mouse, mR);
            DrawRectangleRec(mR, busMute[b] ? (Color){170,55,55,255} : (mH ? (Color){55,40,40,255} : (Color){35,30,30,255}));
            DrawTextShadow("M", (int)btnX+2, (int)c.y+1, 9, busMute[b] ? WHITE : GRAY);
            if (mH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busMute[b] = !busMute[b]; ui_consume_click(); }

            Rectangle sR = {btnX+15, c.y, 14, 13};
            bool sH = CheckCollisionPointRec(mouse, sR);
            DrawRectangleRec(sR, busSolo[b] ? (Color){170,170,55,255} : (sH ? (Color){55,55,40,255} : (Color){35,35,30,255}));
            DrawTextShadow("S", (int)btnX+17, (int)c.y+1, 9, busSolo[b] ? BLACK : GRAY);
            if (sH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busSolo[b] = !busSolo[b]; ui_consume_click(); }
            c.y += 16;
        }

        ui_col_float(&c, "Pan", &busPan[b], 0.02f, -1.0f, 1.0f);
        ui_col_float(&c, "RevSnd", &busReverbSend[b], 0.02f, 0.0f, 1.0f);
        ui_col_space(&c, 2);

        // Filter
        ui_col_toggle(&c, "Filter", &busFilterOn[b]);
        if (busFilterOn[b]) {
            ui_col_float(&c, "Cut", &busFilterCut[b], 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "Res", &busFilterRes[b], 0.02f, 0.0f, 1.0f);
            ui_col_cycle(&c, "Type", filterTypeNames, 3, &busFilterType[b]);
        }
        ui_col_space(&c, 2);

        // Distortion
        ui_col_toggle(&c, "Dist", &busDistOn[b]);
        if (busDistOn[b]) {
            ui_col_float(&c, "Drive", &busDistDrive[b], 0.05f, 1.0f, 4.0f);
            ui_col_float(&c, "Mix", &busDistMix[b], 0.02f, 0.0f, 1.0f);
        }
        ui_col_space(&c, 2);

        // Delay
        ui_col_toggle(&c, "Delay", &busDelayOn[b]);
        if (busDelayOn[b]) {
            ui_col_toggle(&c, "Sync", &busDelaySync[b]);
            if (busDelaySync[b]) ui_col_cycle(&c, "Div", delaySyncNames, 5, &busDelaySyncDiv[b]);
            else ui_col_float(&c, "Time", &busDelayTime[b], 0.01f, 0.01f, 1.0f);
            ui_col_float(&c, "FB", &busDelayFB[b], 0.02f, 0.0f, 0.8f);
            ui_col_float(&c, "Mix", &busDelayMix[b], 0.02f, 0.0f, 1.0f);
        }

        // Vertical separator
        if (b < nBuses - 1) {
            DrawLine((int)(sx+stripW-1), (int)y, (int)(sx+stripW-1), (int)(y+h), (Color){38,38,48,255});
        }
    }

    // Master strip
    float mx = x + nBuses * stripW;
    DrawRectangle((int)mx, (int)y, (int)stripW, 18, (Color){40,38,28,255});
    DrawTextShadow("Master", (int)mx+4, (int)y+3, 11, YELLOW);
    DrawLine((int)mx-1, (int)y, (int)mx-1, (int)(y+h), (Color){55,55,45,255});

    UIColumn mc = ui_column(mx+4, y+22, 15);
    {
        float fW = stripW - 12;
        Rectangle fr = {mc.x, mc.y+2, fW, 10};
        DrawRectangleRec(fr, (Color){20,20,25,255});
        DrawRectangle((int)mc.x, (int)mc.y+2, (int)(fW*masterVol), 10, (Color){170,150,50,255});
        if (CheckCollisionPointRec(mouse, fr) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            masterVol = (mouse.x - mc.x) / fW;
            if (masterVol < 0) masterVol = 0;
            if (masterVol > 1) masterVol = 1;
        }
        mc.y += 16;
    }
    ui_col_space(&mc, 4);

    // Scenes in master strip
    ui_col_sublabel(&mc, "Scenes:", (Color){140,140,200,255});
    ui_col_toggle(&mc, "XFade", &crossfaderEnabled);
    if (crossfaderEnabled) {
        ui_col_float(&mc, "Pos", &crossfaderPos, 0.02f, 0.0f, 1.0f);
        ui_col_int(&mc, "A", &sceneA, 0.3f, 0, 7);
        ui_col_int(&mc, "B", &sceneB, 0.3f, 0, 7);
    }

    (void)h;
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
        if (PushButton(px, preY, presets[i])) { /* preset logic */ }
        px += MeasureTextUI(presets[i], 11) + 20;
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
