// daw_state.h — Shared DAW type definitions
//
// Single source of truth for Transport, Sidechain, MasterFX, TapeFX,
// Mixer, Crossfader, Song, and DawState structs. Used by daw.c,
// bridge_export.c, song_render.c, and daw_file.h.

#ifndef DAW_STATE_H
#define DAW_STATE_H

#include <stdbool.h>

// NUM_BUSES (7) provided by effects.h (must be included before this header)

#define NUM_PATCHES 8
#define SONG_MAX_SECTIONS 64
#define SONG_SECTION_NAME_LEN 12

typedef struct {
    bool playing;
    float bpm;
    int currentPattern;
    int grooveSwing, grooveJitter;
    int currentStep;
    double stepAccumulator;  // fractional step accumulator (in seconds)
} Transport;

typedef struct {
    bool on;
    int source, target;
    float depth, attack, release;
    float hpFreq;  // Bass-preserve HP frequency (0=off, 40-120Hz)
    // Note-triggered envelope mode (LFOTool/Kickstart style)
    bool envOn;
    int envSource, envTarget;
    float envDepth, envAttack, envHold, envRelease;
    int envCurve;    // 0=linear, 1=exp, 2=S-curve
    float envHPFreq;
} Sidechain;

typedef struct {
    bool distOn;    float distDrive, distTone, distMix; int distMode;
    bool crushOn;   float crushBits, crushRate, crushMix;
    bool chorusOn;  float chorusRate, chorusDepth, chorusMix;
    bool flangerOn; float flangerRate, flangerDepth, flangerFeedback, flangerMix;
    bool phaserOn;  float phaserRate, phaserDepth, phaserMix, phaserFeedback; int phaserStages;
    bool combOn;    float combFreq, combFeedback, combMix, combDamping;
    bool tapeOn;    float tapeSaturation, tapeWow, tapeFlutter, tapeHiss;
    bool delayOn;   float delayTime, delayFeedback, delayTone, delayMix;
    bool reverbOn;  float reverbSize, reverbDamping, reverbPreDelay, reverbMix, reverbBass;
    bool eqOn;      float eqLowGain, eqHighGain, eqLowFreq, eqHighFreq;
    bool compOn;    float compThreshold, compRatio, compAttack, compRelease, compMakeup, compKnee;
    bool subBassBoost;
    bool mbOn;      float mbLowCross, mbHighCross;
                    float mbLowGain, mbMidGain, mbHighGain;
                    float mbLowDrive, mbMidDrive, mbHighDrive;
} MasterFX;

typedef struct {
    bool enabled;
    float headTime, feedback, mix;
    int inputSource;
    bool preReverb;
    float saturation, toneHigh, noise, degradeRate;
    float wow, flutter, drift, speedTarget, speedSlew;
    int throwBus;             // Per-bus throw target (-1 = none, 0-6 = bus index)
    float rewindTime, rewindMinSpeed, rewindVinyl, rewindWobble, rewindFilter;
    int rewindCurve;
    bool isRewinding;
} TapeFX;

typedef struct {
    float volume[NUM_BUSES];
    float pan[NUM_BUSES];
    float reverbSend[NUM_BUSES];
    float delaySend[NUM_BUSES];
    bool mute[NUM_BUSES];
    bool solo[NUM_BUSES];
    // Per-bus FX
    bool filterOn[NUM_BUSES];  float filterCut[NUM_BUSES]; float filterRes[NUM_BUSES]; int filterType[NUM_BUSES];
    bool distOn[NUM_BUSES];    float distDrive[NUM_BUSES]; float distMix[NUM_BUSES]; int distMode[NUM_BUSES];
    bool eqOn[NUM_BUSES];      float eqLowGain[NUM_BUSES]; float eqHighGain[NUM_BUSES];
    float eqLowFreq[NUM_BUSES]; float eqHighFreq[NUM_BUSES];
    bool chorusOn[NUM_BUSES];  float chorusRate[NUM_BUSES]; float chorusDepth[NUM_BUSES];
    float chorusMix[NUM_BUSES]; float chorusDelay[NUM_BUSES]; float chorusFB[NUM_BUSES];
    bool phaserOn[NUM_BUSES];  float phaserRate[NUM_BUSES]; float phaserDepth[NUM_BUSES];
    float phaserMix[NUM_BUSES]; float phaserFB[NUM_BUSES]; int phaserStages[NUM_BUSES];
    bool combOn[NUM_BUSES];    float combFreq[NUM_BUSES]; float combFB[NUM_BUSES];
    float combMix[NUM_BUSES];  float combDamping[NUM_BUSES];
    bool delayOn[NUM_BUSES];   bool delaySync[NUM_BUSES];  int delaySyncDiv[NUM_BUSES];
    float delayTime[NUM_BUSES]; float delayFB[NUM_BUSES];  float delayMix[NUM_BUSES];
} Mixer;

typedef struct {
    bool enabled;
    float pos;
    int sceneA, sceneB, count;
} Crossfader;

typedef struct {
    int length;
    int patterns[SONG_MAX_SECTIONS];
    char names[SONG_MAX_SECTIONS][SONG_SECTION_NAME_LEN];
    int loopsPerSection[SONG_MAX_SECTIONS];  // 0 = use global default
    int loopsPerPattern;     // global default (1-8, default 2)
    bool songMode;           // true = follow arrangement, false = loop current pattern
} Song;

typedef struct {
    Transport transport;
    Crossfader crossfader;
    int stepCount;         // Display/loop length: 16 or 32
    Song song;
    Mixer mixer;
    Sidechain sidechain;
    MasterFX masterFx;
    TapeFX tapeFx;

    // Patches
    SynthPatch patches[NUM_PATCHES];
    int selectedPatch;
    float masterVol;

    // Engine-level settings (not per-patch)
    bool scaleLockEnabled;
    int scaleRoot, scaleType;
    bool voiceRandomVowel;

    // Song name
    char songName[64];

    // Chop/flip: per-drum-track sampler slot mapping (-1 = synth, 0+ = sampler slot)
    int chopSliceMap[4];  // SEQ_DRUM_TRACKS = 4
    float chopSlicePitch[32]; // per-slot pitch offset in semitones (applied at trigger)

    // Split keyboard
    bool splitEnabled;
    int splitPoint;
    int splitLeftPatch, splitRightPatch;
    int splitLeftOctave, splitRightOctave;
} DawState;

#endif // DAW_STATE_H
