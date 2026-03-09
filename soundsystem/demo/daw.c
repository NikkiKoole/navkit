// PixelSynth DAW - Opinionated 3-zone layout
// All UI controls from the full demo represented (placeholder state, not wired)
//
// Layout:
//   Transport bar (top) - play/bpm/pattern/groove/scenes
//   Sidebar (left, tabs 1-4) - Synth/Drums/Effects/TapeFX
//   Main area (center, F1-F3) - Sequencer/PianoRoll/Song
//   Detail strip (bottom) - context-sensitive (step inspector / bus fx)

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

#define TRANSPORT_H 36
#define SIDEBAR_W 310
#define DETAIL_H 140
#define TAB_H 26

#define MAIN_X SIDEBAR_W
#define MAIN_Y (TRANSPORT_H + TAB_H)
#define MAIN_W (SCREEN_WIDTH - SIDEBAR_W)
#define MAIN_H (SCREEN_HEIGHT - TRANSPORT_H - TAB_H - DETAIL_H)
#define SIDEBAR_Y TRANSPORT_H
#define SIDEBAR_H (SCREEN_HEIGHT - TRANSPORT_H - DETAIL_H)
#define DETAIL_Y (SCREEN_HEIGHT - DETAIL_H)

// ============================================================================
// TABS
// ============================================================================

typedef enum { SIDE_SYNTH, SIDE_DRUMS, SIDE_FX, SIDE_TAPE, SIDE_MIX, SIDE_COUNT } SideTab;
static SideTab sideTab = SIDE_SYNTH;
static const char* sideTabNames[] = {"Synth", "Drums", "FX", "Tape", "Mix"};
static const int sideTabKeys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE};

typedef enum { MAIN_SEQ, MAIN_PIANO, MAIN_SONG, MAIN_COUNT } MainTab;
static MainTab mainTab = MAIN_SEQ;
static const char* mainTabNames[] = {"Sequencer", "Piano Roll", "Song"};
static const int mainTabKeys[] = {KEY_F1, KEY_F2, KEY_F3};

// ============================================================================
// ALL PLACEHOLDER STATE (mirrors the full demo)
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
    // Envelope
    float attack, decay, sustain, release;
    bool expRelease;
    // Vibrato
    float vibratoRate, vibratoDepth;
    // Filter
    float filterCutoff, filterResonance;
    float filterEnvAmt, filterEnvAttack, filterEnvDecay;
    // Volume
    float volume;
    // Mono/Glide
    bool monoMode;
    float glideTime;
    // Unison
    int unisonCount;
    float unisonDetune;
    // Arpeggiator
    bool arpEnabled;
    int arpMode, arpChord, arpRateDiv;
    float arpRate;
    // Scale Lock
    bool scaleLockEnabled;
    int scaleRoot, scaleType;
    // Wave-specific: PWM
    float pulseWidth, pwmRate, pwmDepth;
    // SCW
    int scwIndex;
    // Voice
    int voiceVowel;
    bool voiceRandomVowel;
    float voicePitch, voiceSpeed, voiceFormantShift;
    float voiceBreathiness, voiceBuzziness;
    bool voiceConsonant, voiceNasal;
    float voiceConsonantAmt, voiceNasalAmt;
    float voicePitchEnv, voicePitchEnvTime, voicePitchEnvCurve;
    // Pluck
    float pluckBrightness, pluckDamping, pluckDamp;
    // Additive
    int additivePreset;
    float additiveBrightness, additiveShimmer, additiveInharmonicity;
    // Mallet
    int malletPreset;
    float malletStiffness, malletHardness, malletStrikePos;
    float malletResonance, malletDamp;
    float malletTremolo, malletTremoloRate;
    // Granular
    int granularScwIndex;
    float granularGrainSize, granularDensity;
    float granularPosition, granularPosRandom;
    float granularPitch, granularPitchRandom, granularAmpRandom;
    bool granularFreeze;
    // FM
    float fmModRatio, fmModIndex, fmFeedback;
    // Phase Distortion
    int pdWaveType;
    float pdDistortion;
    // Membrane
    int membranePreset;
    float membraneDamping, membraneStrike;
    float membraneBend, membraneBendDecay;
    // Bird
    int birdType;
    float birdChirpRange, birdHarmonics;
    float birdTrillRate, birdTrillDepth;
    float birdAmRate, birdAmDepth;
    // LFOs
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
    // Give each patch a unique name and wave type so switching shows different UIs
    snprintf(patches[0].name, 16, "Bass");       patches[0].wave = 0;  // Square
    snprintf(patches[1].name, 16, "Lead");        patches[1].wave = 1;  // Saw
    snprintf(patches[2].name, 16, "Chord");       patches[2].wave = 7;  // Additive
    snprintf(patches[3].name, 16, "Voice");       patches[3].wave = 5;  // Voice/Formant
    snprintf(patches[4].name, 16, "Pluck");       patches[4].wave = 6;  // Pluck
    snprintf(patches[5].name, 16, "FM Bell");     patches[5].wave = 10; // FM
    snprintf(patches[6].name, 16, "Mallet");      patches[6].wave = 8;  // Mallet
    snprintf(patches[7].name, 16, "Bird");        patches[7].wave = 13; // Bird
    // Tweak some defaults to make them sound distinct when wired up
    patches[0].monoMode = true; patches[0].filterCutoff = 0.4f; // Bass: mono, filtered
    patches[1].unisonCount = 2; patches[1].vibratoDepth = 0.3f; // Lead: unison, vibrato
    patches[2].attack = 0.05f; patches[2].release = 0.8f;       // Chord: slow envelope
    patches[3].voiceVowel = 2; patches[3].voiceBreathiness = 0.2f;
    patches[5].fmModRatio = 3.5f; patches[5].fmModIndex = 2.0f; // FM Bell: bright
    patches[7].birdType = 4; // Nightingale
    patchesInit = true;
}

// --- Drums ---
static float drumVolume = 0.6f;
// Kick
static float kickPitch = 50.0f, kickDecay = 0.5f, kickPunchPitch = 150.0f;
static float kickClick = 0.3f, kickTone = 0.5f;
// Snare
static float snarePitch = 180.0f, snareDecay = 0.2f, snareSnappy = 0.6f, snareTone = 0.5f;
// Clap
static float clapDecay = 0.3f, clapTone = 0.5f, clapSpread = 0.015f;
// HiHat
static float hhDecayClosed = 0.05f, hhDecayOpen = 0.4f, hhTone = 0.5f;
// Toms
static float tomPitch = 1.0f, tomDecay = 0.3f, tomPunchDecay = 0.05f;
// Rimshot
static float rimPitch = 1500.0f, rimDecay = 0.03f;
// Cowbell
static float cowbellPitch = 600.0f, cowbellDecay = 0.3f;
// Clave
static float clavePitch = 2500.0f, claveDecay = 0.03f;
// Maracas
static float maracasDecay = 0.05f, maracasTone = 0.5f;
// CR78
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

// Sequencer grid
static bool drumSteps[4][16] = {{0}};
static bool drumStepsInit = false;

// --- Effects ---
// Distortion
static bool distOn = false;
static float distDrive = 2.0f, distTone = 0.7f, distMix = 0.5f;
// Delay
static bool delayOn = false;
static float delayTime = 0.3f, delayFeedback = 0.4f, delayTone = 0.5f, delayMix = 0.3f;
// Tape
static bool tapeOn = false;
static float tapeSaturation = 0.3f, tapeWow = 0.1f, tapeFlutter = 0.1f, tapeHiss = 0.05f;
// Bitcrusher
static bool crushOn = false;
static float crushBits = 8.0f, crushRate = 4.0f, crushMix = 0.5f;
// Chorus
static bool chorusOn = false;
static float chorusRate = 1.0f, chorusDepth = 0.3f, chorusMix = 0.3f;
// Reverb
static bool reverbOn = false;
static float reverbSize = 0.5f, reverbDamping = 0.5f, reverbPreDelay = 0.02f, reverbMix = 0.3f;

// --- Tape FX ---
// Dub Loop
static bool dubLoopEnabled = false;
static float dubHeadTime = 0.5f, dubFeedback = 0.6f, dubMix = 0.5f;
static int dubInputSource = 0;
static bool dubPreReverb = false;
static float dubSaturation = 0.3f, dubToneHigh = 0.7f, dubNoise = 0.05f, dubDegradeRate = 0.1f;
static float dubWow = 0.1f, dubFlutter = 0.1f, dubDrift = 0.05f, dubSpeedTarget = 1.0f;
// Rewind
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
// Per-bus effects
static bool busFilterOn[7] = {false};
static float busFilterCut[7] = {1,1,1,1,1,1,1};
static float busFilterRes[7] = {0};
static int busFilterType[7] = {0}; // 0=LP 1=HP 2=BP
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

// ============================================================================
// TRANSPORT
// ============================================================================

static void drawTransport(void) {
    DrawRectangle(0, 0, SCREEN_WIDTH, TRANSPORT_H, (Color){25, 25, 30, 255});
    DrawLine(0, TRANSPORT_H - 1, SCREEN_WIDTH, TRANSPORT_H - 1, (Color){50, 50, 60, 255});

    int y = 8;
    float x = 12;

    DrawTextShadow("PixelSynth", (int)x, y, 20, WHITE);
    x += 140;

    if (PushButton(x, y, playing ? "Stop" : "Play")) playing = !playing;
    x += 70;

    DraggableFloat(x, y, "BPM", &bpm, 2.0f, 60.0f, 200.0f);
    x += 130;

    if (playing) DrawTextShadow(">>>", (int)x, y, 16, GREEN);

    // Crossfader indicator
    if (crossfaderEnabled) {
        DrawTextShadow(TextFormat("XF:%d>%d %.0f%%", sceneA + 1, sceneB + 1, crossfaderPos * 100), SCREEN_WIDTH - 180, y + 2, 10, (Color){180, 180, 100, 255});
    }

    DrawTextShadow(TextFormat("%d fps", GetFPS()), SCREEN_WIDTH - 60, y + 2, 10, (Color){50, 50, 50, 255});
}

// ============================================================================
// TAB BAR
// ============================================================================

static void drawTabBar(float x, float y, float w, const char** names, const int* keys,
                       int count, int* current, const char* keyPrefix) {
    DrawRectangle((int)x, (int)y, (int)w, TAB_H, (Color){28, 29, 35, 255});
    DrawLine((int)x, (int)y + TAB_H - 1, (int)(x + w), (int)y + TAB_H - 1, (Color){50, 50, 60, 255});

    Vector2 mouse = GetMousePosition();
    float tx = x + 4;

    for (int i = 0; i < count; i++) {
        const char* label = TextFormat("%s%d %s", keyPrefix, i + 1, names[i]);
        int tw = MeasureTextUI(label, 12) + 12;
        Rectangle r = {tx, y + 3, (float)tw, TAB_H - 6};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == *current);

        Color bg = act ? (Color){48, 52, 65, 255} : (hov ? (Color){38, 40, 50, 255} : (Color){28, 29, 35, 255});
        DrawRectangleRec(r, bg);
        if (act) DrawRectangle((int)tx, (int)y + TAB_H - 3, tw, 2, ORANGE);

        DrawTextShadow(label, (int)tx + 6, (int)y + 8, 12, act ? WHITE : (hov ? LIGHTGRAY : GRAY));

        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *current = i; ui_consume_click(); }
        if (IsKeyPressed(keys[i])) *current = i;

        tx += tw + 2;
    }
}

// ============================================================================
// SIDEBAR: SYNTH (patch + wave-specific + envelope + filter + mono + arp + scale)
// ============================================================================

static void drawSideSynth(float x, float y, float w) {
    (void)w;
    SynthPatch *p = &patches[selectedPatch];
    UIColumn c = ui_column(x, y, 18);

    // Patch selector
    ui_col_sublabel(&c, "Patch:", ORANGE);
    {
        const char* patchNames[NUM_PATCHES];
        for (int i = 0; i < NUM_PATCHES; i++) patchNames[i] = patches[i].name;
        ui_col_cycle(&c, "Slot", patchNames, NUM_PATCHES, &selectedPatch);
        p = &patches[selectedPatch];
    }
    ui_col_space(&c, 3);

    // Oscillator
    ui_col_sublabel(&c, "Oscillator:", ORANGE);
    ui_col_cycle(&c, "Wave", waveNames, 14, &p->wave);
    ui_col_space(&c, 3);

    // Wave-specific params (conditional)
    if (p->wave == 0) { // Square
        ui_col_sublabel(&c, "PWM:", (Color){140, 160, 200, 255});
        ui_col_float(&c, "Width", &p->pulseWidth, 0.05f, 0.1f, 0.9f);
        ui_col_float(&c, "Rate", &p->pwmRate, 0.5f, 0.1f, 20.0f);
        ui_col_float(&c, "Depth", &p->pwmDepth, 0.02f, 0.0f, 0.4f);
    } else if (p->wave == 4) { // SCW
        ui_col_sublabel(&c, "Wavetable:", (Color){140, 160, 200, 255});
        ui_col_int(&c, "SCW", &p->scwIndex, 0.3f, 0, 20);
    } else if (p->wave == 5) { // Voice
        ui_col_sublabel(&c, "Formant:", (Color){140, 160, 200, 255});
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
        ui_col_sublabel(&c, "Pitch Env:", (Color){120, 140, 180, 255});
        ui_col_float(&c, "Bend", &p->voicePitchEnv, 0.5f, -12.0f, 12.0f);
        ui_col_float(&c, "Time", &p->voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
        ui_col_float(&c, "Curve", &p->voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
    } else if (p->wave == 6) { // Pluck
        ui_col_sublabel(&c, "Pluck:", (Color){140, 160, 200, 255});
        ui_col_float(&c, "Bright", &p->pluckBrightness, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Sustain", &p->pluckDamping, 0.0002f, 0.995f, 0.9998f);
        ui_col_float(&c, "Damp", &p->pluckDamp, 0.05f, 0.0f, 1.0f);
    } else if (p->wave == 7) { // Additive
        ui_col_sublabel(&c, "Additive:", (Color){140, 160, 200, 255});
        ui_col_cycle(&c, "Preset", additivePresetNames, 5, &p->additivePreset);
        ui_col_float(&c, "Bright", &p->additiveBrightness, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Shimmer", &p->additiveShimmer, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Inharm", &p->additiveInharmonicity, 0.005f, 0.0f, 0.1f);
    } else if (p->wave == 8) { // Mallet
        ui_col_sublabel(&c, "Mallet:", (Color){140, 160, 200, 255});
        ui_col_cycle(&c, "Preset", malletPresetNames, 5, &p->malletPreset);
        ui_col_float(&c, "Stiff", &p->malletStiffness, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Hard", &p->malletHardness, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Strike", &p->malletStrikePos, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Reson", &p->malletResonance, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Damp", &p->malletDamp, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Tremolo", &p->malletTremolo, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "TremSpd", &p->malletTremoloRate, 0.5f, 1.0f, 12.0f);
    } else if (p->wave == 9) { // Granular
        ui_col_sublabel(&c, "Granular:", (Color){140, 160, 200, 255});
        ui_col_int(&c, "Source", &p->granularScwIndex, 0.3f, 0, 20);
        ui_col_float(&c, "Size", &p->granularGrainSize, 5.0f, 10.0f, 200.0f);
        ui_col_float(&c, "Density", &p->granularDensity, 2.0f, 1.0f, 100.0f);
        ui_col_float(&c, "Pos", &p->granularPosition, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "PosRnd", &p->granularPosRandom, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Pitch", &p->granularPitch, 0.1f, 0.25f, 4.0f);
        ui_col_float(&c, "PitRnd", &p->granularPitchRandom, 0.5f, 0.0f, 12.0f);
        ui_col_toggle(&c, "Freeze", &p->granularFreeze);
    } else if (p->wave == 10) { // FM
        ui_col_sublabel(&c, "FM Synth:", (Color){140, 160, 200, 255});
        ui_col_float(&c, "Ratio", &p->fmModRatio, 0.5f, 0.5f, 16.0f);
        ui_col_float(&c, "Index", &p->fmModIndex, 0.1f, 0.0f, 10.0f);
        ui_col_float(&c, "Feedback", &p->fmFeedback, 0.05f, 0.0f, 1.0f);
    } else if (p->wave == 11) { // PD
        ui_col_sublabel(&c, "Phase Dist:", (Color){140, 160, 200, 255});
        ui_col_cycle(&c, "Wave", pdWaveNames, 5, &p->pdWaveType);
        ui_col_float(&c, "Distort", &p->pdDistortion, 0.05f, 0.0f, 1.0f);
    } else if (p->wave == 12) { // Membrane
        ui_col_sublabel(&c, "Membrane:", (Color){140, 160, 200, 255});
        ui_col_cycle(&c, "Preset", membranePresetNames, 5, &p->membranePreset);
        ui_col_float(&c, "Damping", &p->membraneDamping, 0.05f, 0.1f, 1.0f);
        ui_col_float(&c, "Strike", &p->membraneStrike, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Bend", &p->membraneBend, 0.02f, 0.0f, 0.5f);
        ui_col_float(&c, "BndDcy", &p->membraneBendDecay, 0.01f, 0.02f, 0.3f);
    } else if (p->wave == 13) { // Bird
        ui_col_sublabel(&c, "Bird:", (Color){140, 160, 200, 255});
        ui_col_cycle(&c, "Type", birdTypeNames, 5, &p->birdType);
        ui_col_float(&c, "Range", &p->birdChirpRange, 0.1f, 0.5f, 2.0f);
        ui_col_float(&c, "Harmon", &p->birdHarmonics, 0.05f, 0.0f, 1.0f);
        ui_col_sublabel(&c, "Trill:", (Color){120, 140, 180, 255});
        ui_col_float(&c, "Rate", &p->birdTrillRate, 1.0f, 0.0f, 30.0f);
        ui_col_float(&c, "Depth", &p->birdTrillDepth, 0.2f, 0.0f, 5.0f);
        ui_col_sublabel(&c, "Flutter:", (Color){120, 140, 180, 255});
        ui_col_float(&c, "AM Rate", &p->birdAmRate, 1.0f, 0.0f, 20.0f);
        ui_col_float(&c, "AM Dep", &p->birdAmDepth, 0.05f, 0.0f, 1.0f);
    }
    ui_col_space(&c, 4);

    // Envelope
    ui_col_sublabel(&c, "Envelope:", ORANGE);
    ui_col_float(&c, "Atk", &p->attack, 0.5f, 0.001f, 2.0f);
    ui_col_float(&c, "Dec", &p->decay, 0.5f, 0.0f, 2.0f);
    ui_col_float(&c, "Sus", &p->sustain, 0.5f, 0.0f, 1.0f);
    ui_col_float(&c, "Rel", &p->release, 0.5f, 0.01f, 3.0f);
    ui_col_toggle(&c, "Exp Release", &p->expRelease);
    ui_col_space(&c, 3);

    // Filter
    ui_col_sublabel(&c, "Filter:", ORANGE);
    ui_col_float(&c, "Cut", &p->filterCutoff, 0.05f, 0.01f, 1.0f);
    ui_col_float(&c, "Res", &p->filterResonance, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "EnvAmt", &p->filterEnvAmt, 0.05f, -1.0f, 1.0f);
    ui_col_float(&c, "EnvAtk", &p->filterEnvAttack, 0.01f, 0.001f, 0.5f);
    ui_col_float(&c, "EnvDcy", &p->filterEnvDecay, 0.05f, 0.01f, 2.0f);
    ui_col_space(&c, 3);

    // Vibrato
    ui_col_sublabel(&c, "Vibrato:", ORANGE);
    ui_col_float(&c, "Rate", &p->vibratoRate, 0.5f, 0.5f, 15.0f);
    ui_col_float(&c, "Depth", &p->vibratoDepth, 0.2f, 0.0f, 2.0f);
    ui_col_space(&c, 3);

    // Volume
    ui_col_sublabel(&c, "Volume:", ORANGE);
    ui_col_float(&c, "Note", &p->volume, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Master", &masterVolume, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    // Mono/Glide
    if (p->wave != 6 && p->wave != 8) { // not Pluck or Mallet
        ui_col_sublabel(&c, "Mono/Glide:", ORANGE);
        ui_col_toggle(&c, "Mono", &p->monoMode);
        if (p->monoMode) ui_col_float(&c, "Glide", &p->glideTime, 0.02f, 0.01f, 1.0f);
        ui_col_space(&c, 3);
    }

    // Unison
    if (p->wave <= 2) { // Square/Saw/Triangle
        ui_col_sublabel(&c, "Unison:", ORANGE);
        ui_col_int(&c, "Count", &p->unisonCount, 1, 1, 4);
        if (p->unisonCount > 1) ui_col_float(&c, "Detune", &p->unisonDetune, 1.0f, 0.0f, 50.0f);
        ui_col_space(&c, 3);
    }

    // Arpeggiator
    ui_col_sublabel(&c, "Arpeggiator:", ORANGE);
    ui_col_toggle(&c, "On", &p->arpEnabled);
    if (p->arpEnabled) {
        ui_col_cycle(&c, "Mode", arpModeNames, 4, &p->arpMode);
        ui_col_cycle(&c, "Chord", arpChordNames, 7, &p->arpChord);
        ui_col_cycle(&c, "Rate", arpRateNames, 5, &p->arpRateDiv);
        if (p->arpRateDiv == 4) ui_col_float(&c, "Hz", &p->arpRate, 0.5f, 1.0f, 30.0f);
    }
    ui_col_space(&c, 3);

    // Scale Lock
    ui_col_sublabel(&c, "Scale Lock:", ORANGE);
    ui_col_toggle(&c, "On", &p->scaleLockEnabled);
    if (p->scaleLockEnabled) {
        ui_col_cycle(&c, "Root", rootNoteNames, 12, &p->scaleRoot);
        ui_col_cycle(&c, "Scale", scaleNames, 9, &p->scaleType);
    }
    ui_col_space(&c, 3);

    // LFOs
    ui_col_sublabel(&c, "Filter LFO:", (Color){140, 140, 200, 255});
    ui_col_float(&c, "Rate", &p->lfoFilterRate, 0.5f, 0.0f, 20.0f);
    ui_col_float(&c, "Depth", &p->lfoFilterDepth, 0.05f, 0.0f, 2.0f);
    ui_col_cycle(&c, "Shape", lfoShapeNames, 5, &p->lfoFilterShape);
    ui_col_cycle(&c, "Sync", lfoSyncNames, 8, &p->lfoFilterSync);
    ui_col_space(&c, 2);

    ui_col_sublabel(&c, "Reso LFO:", (Color){140, 140, 200, 255});
    ui_col_float(&c, "Rate", &p->lfoResoRate, 0.5f, 0.0f, 20.0f);
    ui_col_float(&c, "Depth", &p->lfoResoDepth, 0.05f, 0.0f, 1.0f);
    ui_col_cycle(&c, "Shape", lfoShapeNames, 5, &p->lfoResoShape);
    ui_col_space(&c, 2);

    ui_col_sublabel(&c, "Amp LFO:", (Color){140, 140, 200, 255});
    ui_col_float(&c, "Rate", &p->lfoAmpRate, 0.5f, 0.0f, 20.0f);
    ui_col_float(&c, "Depth", &p->lfoAmpDepth, 0.05f, 0.0f, 1.0f);
    ui_col_cycle(&c, "Shape", lfoShapeNames, 5, &p->lfoAmpShape);
    ui_col_space(&c, 2);

    ui_col_sublabel(&c, "Pitch LFO:", (Color){140, 140, 200, 255});
    ui_col_float(&c, "Rate", &p->lfoPitchRate, 0.5f, 0.0f, 20.0f);
    ui_col_float(&c, "Depth", &p->lfoPitchDepth, 0.05f, 0.0f, 1.0f);
    ui_col_cycle(&c, "Shape", lfoShapeNames, 5, &p->lfoPitchShape);
}

// ============================================================================
// SIDEBAR: DRUMS (all drum types + sidechain)
// ============================================================================

static void drawSideDrums(float x, float y, float w) {
    (void)w;
    UIColumn c = ui_column(x, y, 18);

    ui_col_float(&c, "Volume", &drumVolume, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 4);

    ui_col_sublabel(&c, "Kick:", ORANGE);
    ui_col_float(&c, "Pitch", &kickPitch, 3.0f, 30.0f, 100.0f);
    ui_col_float(&c, "Decay", &kickDecay, 0.07f, 0.1f, 1.5f);
    ui_col_float(&c, "Punch", &kickPunchPitch, 10.0f, 80.0f, 300.0f);
    ui_col_float(&c, "Click", &kickClick, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Tone", &kickTone, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Snare:", ORANGE);
    ui_col_float(&c, "Pitch", &snarePitch, 10.0f, 100.0f, 350.0f);
    ui_col_float(&c, "Decay", &snareDecay, 0.03f, 0.05f, 0.6f);
    ui_col_float(&c, "Snappy", &snareSnappy, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Tone", &snareTone, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Clap:", ORANGE);
    ui_col_float(&c, "Decay", &clapDecay, 0.03f, 0.1f, 0.6f);
    ui_col_float(&c, "Tone", &clapTone, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Spread", &clapSpread, 0.001f, 0.005f, 0.03f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "HiHat:", ORANGE);
    ui_col_float(&c, "Closed", &hhDecayClosed, 0.01f, 0.01f, 0.2f);
    ui_col_float(&c, "Open", &hhDecayOpen, 0.05f, 0.1f, 1.0f);
    ui_col_float(&c, "Tone", &hhTone, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Toms:", ORANGE);
    ui_col_float(&c, "Pitch", &tomPitch, 0.1f, 0.5f, 2.0f);
    ui_col_float(&c, "Decay", &tomDecay, 0.03f, 0.1f, 0.8f);
    ui_col_float(&c, "PnchDcy", &tomPunchDecay, 0.01f, 0.01f, 0.2f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Rimshot:", ORANGE);
    ui_col_float(&c, "Pitch", &rimPitch, 100.0f, 800.0f, 3000.0f);
    ui_col_float(&c, "Decay", &rimDecay, 0.005f, 0.01f, 0.1f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Cowbell:", ORANGE);
    ui_col_float(&c, "Pitch", &cowbellPitch, 20.0f, 400.0f, 1000.0f);
    ui_col_float(&c, "Decay", &cowbellDecay, 0.03f, 0.1f, 0.6f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Clave:", ORANGE);
    ui_col_float(&c, "Pitch", &clavePitch, 100.0f, 1500.0f, 4000.0f);
    ui_col_float(&c, "Decay", &claveDecay, 0.005f, 0.01f, 0.1f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Maracas:", ORANGE);
    ui_col_float(&c, "Decay", &maracasDecay, 0.01f, 0.02f, 0.2f);
    ui_col_float(&c, "Tone", &maracasTone, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 6);

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

// ============================================================================
// SIDEBAR: EFFECTS (all 6 effect types + presets)
// ============================================================================

static void drawSideFx(float x, float y, float w) {
    (void)w;
    UIColumn c = ui_column(x, y, 18);

    ui_col_sublabel(&c, "Distortion:", ORANGE);
    ui_col_toggle(&c, "On", &distOn);
    ui_col_float(&c, "Drive", &distDrive, 0.5f, 1.0f, 20.0f);
    ui_col_float(&c, "Tone", &distTone, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Mix", &distMix, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Delay:", ORANGE);
    ui_col_toggle(&c, "On", &delayOn);
    ui_col_float(&c, "Time", &delayTime, 0.05f, 0.05f, 1.0f);
    ui_col_float(&c, "Fdbk", &delayFeedback, 0.05f, 0.0f, 0.9f);
    ui_col_float(&c, "Tone", &delayTone, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Mix", &delayMix, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Tape:", ORANGE);
    ui_col_toggle(&c, "On", &tapeOn);
    ui_col_float(&c, "Saturat", &tapeSaturation, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Wow", &tapeWow, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Flutter", &tapeFlutter, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Hiss", &tapeHiss, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Bitcrusher:", ORANGE);
    ui_col_toggle(&c, "On", &crushOn);
    ui_col_float(&c, "Bits", &crushBits, 0.5f, 2.0f, 16.0f);
    ui_col_float(&c, "Rate", &crushRate, 1.0f, 1.0f, 32.0f);
    ui_col_float(&c, "Mix", &crushMix, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Chorus:", ORANGE);
    ui_col_toggle(&c, "On", &chorusOn);
    ui_col_float(&c, "Rate", &chorusRate, 0.1f, 0.1f, 5.0f);
    ui_col_float(&c, "Depth", &chorusDepth, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Mix", &chorusMix, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Reverb:", ORANGE);
    ui_col_toggle(&c, "On", &reverbOn);
    ui_col_float(&c, "Size", &reverbSize, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Damping", &reverbDamping, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "PreDly", &reverbPreDelay, 0.005f, 0.0f, 0.1f);
    ui_col_float(&c, "Mix", &reverbMix, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 6);

    ui_col_sublabel(&c, "Piku FX:", PINK);
    ui_col_button(&c, "Clean");
    ui_col_button(&c, "9-Bit");
    ui_col_button(&c, "Wobbly");
    ui_col_button(&c, "Toy");
    ui_col_space(&c, 3);

    ui_col_sublabel(&c, "Mac FX:", SKYBLUE);
    ui_col_button(&c, "Tape+Chorus");
    ui_col_button(&c, "Chorus");
    ui_col_button(&c, "Lo-Fi");
}

// ============================================================================
// SIDEBAR: TAPE FX (Dub Loop + Rewind)
// ============================================================================

static void drawSideTape(float x, float y, float w) {
    (void)w;
    UIColumn c = ui_column(x, y, 18);

    ui_col_sublabel(&c, "Dub Loop:", ORANGE);
    ui_col_toggle(&c, "On", &dubLoopEnabled);
    ui_col_float(&c, "Time", &dubHeadTime, 0.05f, 0.05f, 4.0f);
    ui_col_float(&c, "Fdbk", &dubFeedback, 0.05f, 0.0f, 0.95f);
    ui_col_float(&c, "Mix", &dubMix, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 2);

    ui_col_cycle(&c, "Input", dubInputNames, 4, &dubInputSource);
    ui_col_toggle(&c, "PreReverb", &dubPreReverb);
    ui_col_space(&c, 2);

    ui_col_sublabel(&c, "Degradation:", (Color){140, 140, 200, 255});
    ui_col_float(&c, "Saturat", &dubSaturation, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "ToneHi", &dubToneHigh, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Noise", &dubNoise, 0.02f, 0.0f, 0.5f);
    ui_col_float(&c, "Degrade", &dubDegradeRate, 0.02f, 0.0f, 0.5f);
    ui_col_space(&c, 2);

    ui_col_sublabel(&c, "Modulation:", (Color){140, 140, 200, 255});
    ui_col_float(&c, "Wow", &dubWow, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Flutter", &dubFlutter, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Drift", &dubDrift, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Speed", &dubSpeedTarget, 0.1f, 0.25f, 2.0f);
    ui_col_space(&c, 3);

    ui_col_button(&c, "Throw");
    ui_col_button(&c, "Cut");
    ui_col_button(&c, "1/2 Speed");
    ui_col_button(&c, "Normal");
    ui_col_space(&c, 8);

    ui_col_sublabel(&c, "Rewind:", ORANGE);
    ui_col_float(&c, "Time", &rewindTime, 0.1f, 0.3f, 3.0f);
    ui_col_cycle(&c, "Curve", rewindCurveNames, 3, &rewindCurve);
    ui_col_float(&c, "MinSpd", &rewindMinSpeed, 0.05f, 0.05f, 0.8f);
    ui_col_space(&c, 2);
    ui_col_float(&c, "Vinyl", &rewindVinyl, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Wobble", &rewindWobble, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c, "Filter", &rewindFilter, 0.05f, 0.0f, 1.0f);
    ui_col_space(&c, 3);
    ui_col_button(&c, isRewinding ? ">>..." : "Rewind");
}

// ============================================================================
// SIDEBAR: MIX (compact vertical mixer)
// ============================================================================

static void drawSideMix(float x, float y, float w) {
    Vector2 mouse = GetMousePosition();
    float stripH = 22;
    float labelW = 48;
    float faderW = w - labelW - 48;
    if (faderW < 30) faderW = 30;

    for (int b = 0; b < 7; b++) {
        float sy = y + b * (stripH + 2);
        bool isSel = (selectedBus == b);

        DrawTextShadow(busNames[b], (int)x, (int)sy + 4, 10, isSel ? ORANGE : LIGHTGRAY);

        float fx = x + labelW;
        Rectangle faderRect = {fx, sy + 3, faderW, 13};
        DrawRectangleRec(faderRect, (Color){20, 20, 25, 255});
        float fillW = faderW * busVolumes[b];
        Color volCol = busMute[b] ? (Color){80, 40, 40, 255} : (Color){50, 130, 50, 255};
        DrawRectangle((int)fx + 1, (int)sy + 4, (int)fillW - 2, 11, volCol);

        if (CheckCollisionPointRec(mouse, faderRect) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            busVolumes[b] = (mouse.x - fx) / faderW;
            if (busVolumes[b] < 0) busVolumes[b] = 0;
            if (busVolumes[b] > 1) busVolumes[b] = 1;
        }

        float btnX = fx + faderW + 3;
        Rectangle mRect = {btnX, sy + 2, 14, 14};
        bool mHov = CheckCollisionPointRec(mouse, mRect);
        DrawRectangleRec(mRect, busMute[b] ? (Color){170, 55, 55, 255} : (mHov ? (Color){55, 40, 40, 255} : (Color){35, 30, 30, 255}));
        DrawTextShadow("M", (int)btnX + 2, (int)sy + 3, 10, busMute[b] ? WHITE : GRAY);
        if (mHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busMute[b] = !busMute[b]; ui_consume_click(); }

        Rectangle sRect = {btnX + 16, sy + 2, 14, 14};
        bool sHov = CheckCollisionPointRec(mouse, sRect);
        DrawRectangleRec(sRect, busSolo[b] ? (Color){170, 170, 55, 255} : (sHov ? (Color){55, 55, 40, 255} : (Color){35, 35, 30, 255}));
        DrawTextShadow("S", (int)btnX + 18, (int)sy + 3, 10, busSolo[b] ? BLACK : GRAY);
        if (sHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { busSolo[b] = !busSolo[b]; ui_consume_click(); }

        Rectangle nameRect = {x, sy, labelW, stripH};
        if (CheckCollisionPointRec(mouse, nameRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedBus = (selectedBus == b) ? -1 : b;
            detailCtx = (selectedBus >= 0) ? DETAIL_BUS_FX : DETAIL_NONE;
            ui_consume_click();
        }
    }

    // Master
    float my = y + 7 * (stripH + 2) + 4;
    DrawTextShadow("Master", (int)x, (int)my + 4, 10, YELLOW);
    float mfx = x + labelW;
    Rectangle mfRect = {mfx, my + 3, faderW, 13};
    DrawRectangleRec(mfRect, (Color){20, 20, 25, 255});
    DrawRectangle((int)mfx + 1, (int)my + 4, (int)(faderW * masterVol) - 2, 11, (Color){170, 150, 50, 255});
    if (CheckCollisionPointRec(mouse, mfRect) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        masterVol = (mouse.x - mfx) / faderW;
        if (masterVol < 0) masterVol = 0;
        if (masterVol > 1) masterVol = 1;
    }

    // Groove below mixer
    float gy = my + 28;
    DrawTextShadow("Groove:", (int)x, (int)gy, 11, ORANGE);
    gy += 16;
    DraggableInt(x, gy, "Kick", &grooveKickNudge, 0.3f, -12, 12); gy += 18;
    DraggableInt(x, gy, "Snare", &grooveSnareDelay, 0.3f, -12, 12); gy += 18;
    DraggableInt(x, gy, "HH", &grooveHatNudge, 0.3f, -12, 12); gy += 18;
    DraggableInt(x, gy, "Swing", &grooveSwing, 0.3f, 0, 12); gy += 18;
    DraggableInt(x, gy, "Jitter", &grooveJitter, 0.3f, 0, 6); gy += 22;

    // Scenes
    DrawTextShadow("Scenes:", (int)x, (int)gy, 11, ORANGE);
    gy += 16;
    ToggleBool(x, gy, "XFade", &crossfaderEnabled); gy += 18;
    if (crossfaderEnabled) {
        DraggableFloat(x, gy, "Pos", &crossfaderPos, 0.02f, 0.0f, 1.0f); gy += 18;
        DraggableInt(x, gy, "A", &sceneA, 0.3f, 0, 7); gy += 18;
        DraggableInt(x, gy, "B", &sceneB, 0.3f, 0, 7);
    }
}

// ============================================================================
// MAIN: SEQUENCER
// ============================================================================

static void drawMainSeq(float x, float y, float w, float h) {
    if (!drumStepsInit) {
        drumSteps[0][0] = drumSteps[0][4] = drumSteps[0][8] = drumSteps[0][12] = true;
        drumSteps[1][4] = drumSteps[1][12] = true;
        for (int i = 0; i < 16; i += 2) drumSteps[2][i] = true;
        drumStepsInit = true;
    }

    Vector2 mouse = GetMousePosition();

    // Pattern selector row
    DrawTextShadow("Pattern:", (int)x, (int)y + 2, 12, GRAY);
    float px = x + 60;
    for (int i = 0; i < 8; i++) {
        Rectangle r = {px + i * 26, y, 24, 20};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == currentPattern);
        Color bg = act ? (Color){50, 90, 50, 255} : (hov ? (Color){42, 44, 52, 255} : (Color){33, 34, 40, 255});
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, act ? GREEN : (Color){48, 48, 58, 255});
        DrawTextShadow(TextFormat("%d", i + 1), (int)px + i * 26 + 8, (int)y + 4, 11, act ? WHITE : GRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { currentPattern = i; ui_consume_click(); }
    }
    y += 24;
    h -= 24;

    int labelW = 50;
    int cellW = (int)((w - labelW - 10) / 16);
    if (cellW < 10) cellW = 10;
    int cellH = (int)((h - 30) / 7);
    if (cellH < 16) cellH = 16;
    if (cellH > 32) cellH = 32;

    const char* trackNames[] = {"Kick", "Snare", "CH", "Clap", "Bass", "Lead", "Chord"};

    for (int i = 0; i < 16; i++) {
        int sx = (int)x + labelW + i * cellW;
        Color numCol = (i % 4 == 0) ? (Color){80, 80, 90, 255} : (Color){50, 50, 58, 255};
        DrawTextShadow(TextFormat("%d", i + 1), sx + cellW / 2 - 3, (int)y, 9, numCol);
    }

    for (int track = 0; track < 7; track++) {
        int ty = (int)y + 14 + track * (cellH + 2);
        bool isDrum = (track < 4);
        if (track == 4) DrawLine((int)x, ty - 2, (int)(x + w), ty - 2, (Color){55, 55, 70, 255});

        DrawTextShadow(trackNames[track], (int)x + 2, ty + cellH / 2 - 5, 11,
                       isDrum ? LIGHTGRAY : (Color){140, 180, 255, 255});

        for (int step = 0; step < 16; step++) {
            int sx = (int)x + labelW + step * cellW;
            Rectangle cell = {(float)sx, (float)ty, (float)(cellW - 1), (float)cellH};
            bool hov = CheckCollisionPointRec(mouse, cell);
            Color bg = (step / 4) % 2 == 0 ? (Color){36, 36, 42, 255} : (Color){30, 30, 35, 255};

            if (isDrum) {
                if (drumSteps[track][step]) bg = (Color){50, 125, 65, 255};
                if (hov) { bg.r += 20; bg.g += 20; bg.b += 20; }
                DrawRectangleRec(cell, bg);
                DrawRectangleLinesEx(cell, 1, (Color){48, 48, 56, 255});
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    drumSteps[track][step] = !drumSteps[track][step];
                    detailCtx = DETAIL_STEP; detailTrack = track; detailStep = step;
                    ui_consume_click();
                }
            } else {
                if (hov) { bg.r += 15; bg.g += 15; bg.b += 15; }
                DrawRectangleRec(cell, bg);
                DrawRectangleLinesEx(cell, 1, (Color){42, 42, 52, 255});
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    detailCtx = DETAIL_STEP; detailTrack = track; detailStep = step;
                    ui_consume_click();
                }
            }

            if (track == detailTrack && step == detailStep && detailCtx == DETAIL_STEP)
                DrawRectangleLinesEx(cell, 2, ORANGE);
        }
    }
    (void)h;
}

// ============================================================================
// MAIN: PIANO ROLL
// ============================================================================

static void drawMainPiano(float x, float y, float w, float h) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){20, 20, 25, 255});
    int keyW = 36, noteH = (int)(h / 24);
    if (noteH < 4) noteH = 4;
    const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int vis = (int)(h / noteH); if (vis > 24) vis = 24;

    for (int i = 0; i < vis; i++) {
        int ny = (int)(y + h) - (i + 1) * noteH;
        int ni = i % 12;
        bool blk = (ni==1||ni==3||ni==6||ni==8||ni==10);
        DrawRectangle((int)x, ny, keyW, noteH - 1, blk ? (Color){28,28,33,255} : (Color){42,42,48,255});
        if (noteH >= 10) DrawTextShadow(TextFormat("%s%d", noteNames[ni], 3+i/12), (int)x+2, ny+1, 8, GRAY);
        DrawLine((int)x+keyW, ny+noteH-1, (int)(x+w), ny+noteH-1, ni==0 ? (Color){55,55,65,255} : (Color){32,32,38,255});
    }
    int stepW = (int)((w - keyW) / 16);
    for (int i = 0; i <= 16; i++) {
        int lx = (int)x + keyW + i * stepW;
        DrawLine(lx, (int)y, lx, (int)(y+h), i%4==0 ? (Color){60,60,72,255} : (Color){36,36,42,255});
    }
    DrawTextShadow("Piano Roll", (int)(x+w/2-36), (int)(y+h/2-6), 14, (Color){50,50,60,255});
    DrawRectangleLinesEx((Rectangle){x,y,w,h}, 1, (Color){48,48,58,255});
}

// ============================================================================
// MAIN: SONG
// ============================================================================

static void drawMainSong(float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    DrawTextShadow("Pattern Chain:", (int)x+4, (int)y+4, 14, (Color){180,180,255,255});
    float cy = y + 24; int chW = 30, chH = 26;

    for (int i = 0; i < chainLength; i++) {
        float cx = x + 4 + i * (chW + 3);
        if (cx + chW > x + w - 4) break;
        Rectangle r = {cx, cy, (float)chW, (float)chH};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){55,58,68,255} : (Color){40,40,50,255});
        DrawRectangleLinesEx(r, 1, (Color){62,62,72,255});
        DrawTextShadow(TextFormat("%d", chain[i]+1), (int)cx+10, (int)cy+7, 12, WHITE);
        if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            for (int j = i; j < chainLength-1; j++) chain[j] = chain[j+1];
            chainLength--; ui_consume_click();
        }
        if (hov) { float wh = GetMouseWheelMove(); if (wh>0) chain[i]=(chain[i]+1)%8; else if (wh<0) chain[i]=(chain[i]+7)%8; }
    }
    if (chainLength < 64) {
        float ax = x + 4 + chainLength * (chW + 3);
        if (ax + chW <= x + w - 4) {
            Rectangle ar = {ax, cy, (float)chW, (float)chH};
            bool ah = CheckCollisionPointRec(mouse, ar);
            DrawRectangleRec(ar, ah ? (Color){60,62,72,255} : (Color){35,36,45,255});
            DrawRectangleLinesEx(ar, 1, (Color){70,70,82,255});
            DrawTextShadow("+", (int)ax+11, (int)cy+7, 12, ah ? WHITE : GRAY);
            if (ah && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { chain[chainLength++] = currentPattern; ui_consume_click(); }
        }
    }

    float sy = cy + chH + 14;
    DrawTextShadow("Scenes:", (int)x+4, (int)sy, 14, (Color){180,200,255,255});
    sy += 20;
    for (int i = 0; i < (int)sceneCount; i++) {
        float sx = x + 4 + i * 72;
        if (sx + 66 > x + w) break;
        Rectangle r = {sx, sy, 66, 30};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){50,54,64,255} : (Color){36,38,46,255});
        DrawRectangleLinesEx(r, 1, (Color){60,60,72,255});
        DrawTextShadow(TextFormat("Scene %d", i+1), (int)sx+6, (int)sy+9, 11, LIGHTGRAY);
    }

    float tlY = sy + 44, tlH = y + h - tlY - 4;
    if (tlH > 30) {
        DrawRectangle((int)x, (int)tlY, (int)w, (int)tlH, (Color){20,20,25,255});
        DrawRectangleLinesEx((Rectangle){x,tlY,w,tlH}, 1, (Color){45,45,55,255});
        DrawTextShadow("Arrangement Timeline", (int)(x+w/2-75), (int)(tlY+tlH/2-6), 14, (Color){48,48,58,255});
    }
}

// ============================================================================
// DETAIL STRIP
// ============================================================================

static void drawDetail(void) {
    DrawRectangle(0, DETAIL_Y, SCREEN_WIDTH, DETAIL_H, (Color){26, 27, 33, 255});
    DrawLine(0, DETAIL_Y, SCREEN_WIDTH, DETAIL_Y, (Color){50, 50, 62, 255});

    float x = 12, y = DETAIL_Y + 6;

    if (detailCtx == DETAIL_STEP && detailStep >= 0) {
        const char* tn[] = {"Kick","Snare","CH","Clap","Bass","Lead","Chord"};
        DrawTextShadow(TextFormat("Step %d - %s", detailStep+1,
                       detailTrack >= 0 && detailTrack < 7 ? tn[detailTrack] : "?"),
                       (int)x, (int)y, 16, ORANGE);

        static float stepVel = 1.0f, stepProb = 1.0f, stepGate = 1.0f;
        static int stepCond = 0, stepRetrig = 0;
        static const char* condNames[] = {"Always","1:2","2:2","1:3","2:3","3:3","1:4","Fill","!Fill","First","Last"};
        static const char* retrigNames[] = {"Off","1/8","1/16","1/32"};

        UIColumn c1 = ui_column(x + 180, y, 18);
        ui_col_float(&c1, "Velocity", &stepVel, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c1, "Probability", &stepProb, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c1, "Gate", &stepGate, 0.05f, 0.1f, 2.0f);

        UIColumn c2 = ui_column(x + 400, y, 18);
        ui_col_cycle(&c2, "Condition", condNames, 11, &stepCond);
        ui_col_cycle(&c2, "Retrig", retrigNames, 4, &stepRetrig);

        UIColumn c3 = ui_column(x + 650, y, 18);
        ui_col_sublabel(&c3, "P-Locks:", (Color){140,140,200,255});
        DrawTextShadow("(shift+click any param to lock)", (int)x+650, (int)y+20, 11, (Color){65,65,75,255});

        // Slide/Accent for melody tracks
        if (detailTrack >= 4) {
            static bool stepSlide = false, stepAccent = false;
            UIColumn c4 = ui_column(x + 870, y, 18);
            ui_col_toggle(&c4, "Slide", &stepSlide);
            ui_col_toggle(&c4, "Accent", &stepAccent);
        }

    } else if (detailCtx == DETAIL_BUS_FX && selectedBus >= 0) {
        DrawTextShadow(TextFormat("%s - Bus Settings", busNames[selectedBus]), (int)x, (int)y, 16, ORANGE);
        int b = selectedBus;

        // Row 1: Volume, Pan, Reverb Send
        UIColumn c1 = ui_column(x + 180, y, 18);
        ui_col_float(&c1, "Volume", &busVolumes[b], 0.02f, 0.0f, 2.0f);
        ui_col_float(&c1, "Pan", &busPan[b], 0.02f, -1.0f, 1.0f);
        ui_col_float(&c1, "RevSend", &busReverbSend[b], 0.02f, 0.0f, 1.0f);

        // Filter
        UIColumn c2 = ui_column(x + 400, y, 18);
        ui_col_toggle(&c2, "Filter", &busFilterOn[b]);
        if (busFilterOn[b]) {
            ui_col_float(&c2, "Cut", &busFilterCut[b], 0.02f, 0.0f, 1.0f);
            ui_col_float(&c2, "Res", &busFilterRes[b], 0.02f, 0.0f, 1.0f);
            ui_col_cycle(&c2, "Type", filterTypeNames, 3, &busFilterType[b]);
        }

        // Distortion
        UIColumn c3 = ui_column(x + 620, y, 18);
        ui_col_toggle(&c3, "Dist", &busDistOn[b]);
        if (busDistOn[b]) {
            ui_col_float(&c3, "Drive", &busDistDrive[b], 0.05f, 1.0f, 4.0f);
            ui_col_float(&c3, "Mix", &busDistMix[b], 0.02f, 0.0f, 1.0f);
        }

        // Delay
        UIColumn c4 = ui_column(x + 830, y, 18);
        ui_col_toggle(&c4, "Delay", &busDelayOn[b]);
        if (busDelayOn[b]) {
            ui_col_toggle(&c4, "Sync", &busDelaySync[b]);
            if (busDelaySync[b]) {
                ui_col_cycle(&c4, "Div", delaySyncNames, 5, &busDelaySyncDiv[b]);
            } else {
                ui_col_float(&c4, "Time", &busDelayTime[b], 0.01f, 0.01f, 1.0f);
            }
            ui_col_float(&c4, "FB", &busDelayFB[b], 0.02f, 0.0f, 0.8f);
            ui_col_float(&c4, "Mix", &busDelayMix[b], 0.02f, 0.0f, 1.0f);
        }

    } else {
        DrawTextShadow("Click a step or bus name for details", (int)x, (int)y + 4, 14, (Color){55,55,65,255});
        DrawTextShadow("Sidebar: 1=Synth 2=Drums 3=FX 4=Tape 5=Mix   Main: F1=Seq F2=Piano F3=Song", (int)x, (int)y + 26, 12, (Color){50,50,60,255});
        DrawTextShadow("Space=Play  Click bus name in mixer for per-bus FX", (int)x, (int)y + 44, 12, (Color){45,45,55,255});
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth DAW");
    SetTargetFPS(60);

    Font font = LoadEmbeddedFont();
    ui_init(&font);
    initPatches();

    // Sidebar needs scrolling since synth tab can be very tall
    static float sideScroll = 0;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) playing = !playing;

        BeginDrawing();
        ClearBackground((Color){22, 22, 28, 255});
        ui_begin_frame();

        drawTransport();

        // === SIDEBAR ===
        DrawRectangle(0, SIDEBAR_Y, SIDEBAR_W, SIDEBAR_H, (Color){28, 29, 35, 255});
        DrawLine(SIDEBAR_W - 1, SIDEBAR_Y, SIDEBAR_W - 1, SIDEBAR_Y + SIDEBAR_H, (Color){50, 50, 62, 255});

        // Sidebar tabs
        int sideInt = (int)sideTab;
        drawTabBar(0, SIDEBAR_Y, SIDEBAR_W, sideTabNames, sideTabKeys, SIDE_COUNT, &sideInt, "");
        if (sideInt != (int)sideTab) { sideTab = (SideTab)sideInt; sideScroll = 0; }

        // Scroll handling for sidebar
        Vector2 mouse = GetMousePosition();
        if (mouse.x < SIDEBAR_W && mouse.y > SIDEBAR_Y + TAB_H && mouse.y < DETAIL_Y) {
            sideScroll -= GetMouseWheelMove() * 30;
            if (sideScroll < 0) sideScroll = 0;
        }

        // Clip sidebar content
        BeginScissorMode(0, SIDEBAR_Y + TAB_H, SIDEBAR_W, SIDEBAR_H - TAB_H);
        float sideX = 8;
        float sideY = SIDEBAR_Y + TAB_H + 6 - sideScroll;
        float sideW = SIDEBAR_W - 16;
        switch (sideTab) {
            case SIDE_SYNTH: drawSideSynth(sideX, sideY, sideW); break;
            case SIDE_DRUMS: drawSideDrums(sideX, sideY, sideW); break;
            case SIDE_FX:    drawSideFx(sideX, sideY, sideW); break;
            case SIDE_TAPE:  drawSideTape(sideX, sideY, sideW); break;
            case SIDE_MIX:   drawSideMix(sideX, sideY, sideW); break;
            default: break;
        }
        EndScissorMode();

        // Scroll indicator
        if (sideScroll > 0) {
            DrawTextShadow("^ scroll ^", 70, SIDEBAR_Y + TAB_H + 2, 10, (Color){80,80,90,255});
        }

        // === MAIN AREA ===
        int mainInt = (int)mainTab;
        drawTabBar(MAIN_X, TRANSPORT_H, MAIN_W, mainTabNames, mainTabKeys, MAIN_COUNT, &mainInt, "F");
        mainTab = (MainTab)mainInt;

        float mainX = MAIN_X + 6, mainY = MAIN_Y + 6;
        float mainW = MAIN_W - 12, mainH = MAIN_H - 12;
        switch (mainTab) {
            case MAIN_SEQ:   drawMainSeq(mainX, mainY, mainW, mainH); break;
            case MAIN_PIANO: drawMainPiano(mainX, mainY, mainW, mainH); break;
            case MAIN_SONG:  drawMainSong(mainX, mainY, mainW, mainH); break;
            default: break;
        }

        // === DETAIL ===
        drawDetail();

        ui_update();
        DrawTooltip();
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
