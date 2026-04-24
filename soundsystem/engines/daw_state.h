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

// Arrangement (per-track pattern grid)
#define ARR_MAX_BARS 64
#define ARR_EMPTY -1
#define ARR_MAX_TRACKS 8  // Matches SEQ_V2_MAX_TRACKS (drum0-3, bass, lead, chord, sampler)

typedef struct {
    int cells[ARR_MAX_BARS][ARR_MAX_TRACKS];  // [bar][track] = pattern index or ARR_EMPTY
    int length;                                // number of bars (0 = inactive)
    bool arrMode;                              // true = arrangement drives playback
    int barLengthSteps;                        // explicit bar length in steps (0 = use track 0 wrap)
} Arrangement;

// Clip Launcher (Bitwig-style, per-track clip triggering)
#define LAUNCHER_MAX_SLOTS 16  // clip slots per track (scenes)

typedef enum {
    CLIP_EMPTY = 0,     // No clip in slot
    CLIP_STOPPED,       // Has clip, not playing
    CLIP_PLAYING,       // Currently playing
    CLIP_QUEUED,        // Will start at next quantize point
    CLIP_STOP_QUEUED,   // Playing, will stop at next quantize point
} ClipState;

typedef enum {
    NEXT_ACTION_LOOP = 0,  // Keep looping (default)
    NEXT_ACTION_STOP,      // Stop after N loops
    NEXT_ACTION_NEXT,      // Play next clip in column
    NEXT_ACTION_PREV,      // Play previous clip in column
    NEXT_ACTION_FIRST,     // Play first clip in column
    NEXT_ACTION_RANDOM,    // Random clip from column
    NEXT_ACTION_RETURN,    // Return to arrangement
    NEXT_ACTION_COUNT,
} NextAction;

typedef struct {
    int pattern[LAUNCHER_MAX_SLOTS];        // pattern index per slot (-1 = empty)
    ClipState state[LAUNCHER_MAX_SLOTS];
    NextAction nextAction[LAUNCHER_MAX_SLOTS]; // what happens after nextActionLoops
    int nextActionLoops[LAUNCHER_MAX_SLOTS];   // trigger after N loops (0 = disabled)
    int playingSlot;                         // which slot is playing (-1 = none)
    int queuedSlot;                          // which slot is queued (-1 = none)
    int loopCount;                           // loops completed on current clip
} LauncherTrack;

typedef struct {
    LauncherTrack tracks[ARR_MAX_TRACKS];
    bool active;                             // true = launcher is driving some tracks
    int quantize;                            // 0=bar, 1=1/16, 2=1/8, 4=beat, 8=half-bar
} Launcher;

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
    bool octaverOn; float octaverMix, octaverSubLevel, octaverTone;
    bool tremoloOn; float tremoloRate, tremoloDepth; int tremoloShape;
    bool leslieOn;  int leslieSpeed; float leslieDrive, leslieBalance, leslieDoppler, leslieMix;
    bool wahOn;     int wahMode; float wahRate, wahSensitivity, wahFreqLow, wahFreqHigh, wahResonance, wahMix;
    bool distOn;    float distDrive, distTone, distMix; int distMode;
    bool crushOn;   float crushBits, crushRate, crushMix;
    bool chorusOn;  bool chorusBBD; float chorusRate, chorusDepth, chorusMix;
    bool flangerOn; float flangerRate, flangerDepth, flangerFeedback, flangerMix;
    bool phaserOn;  float phaserRate, phaserDepth, phaserMix, phaserFeedback; int phaserStages;
    bool combOn;    float combFreq, combFeedback, combMix, combDamping;
    bool ringModOn; float ringModFreq, ringModMix;
    bool tapeOn;    float tapeSaturation, tapeWow, tapeFlutter, tapeHiss;
    bool delayOn;   float delayTime, delayFeedback, delayTone, delayMix;
    bool reverbOn;  bool reverbFDN; float reverbSize, reverbDamping, reverbPreDelay, reverbMix, reverbBass;
    bool eqOn;      float eqLowGain, eqHighGain, eqLowFreq, eqHighFreq;
    bool compOn;    float compThreshold, compRatio, compAttack, compRelease, compMakeup, compKnee;
    bool subBassBoost;
    bool mbOn;      float mbLowCross, mbHighCross;
                    float mbLowGain, mbMidGain, mbHighGain;
                    float mbLowDrive, mbMidDrive, mbHighDrive;
    bool vinylOn;   float vinylCrackle, vinylNoise, vinylWarp, vinylWarpRate, vinylTone;
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
    // Tape stop
    float tapeStopTime, tapeStopSpinTime;
    int tapeStopCurve;
    bool tapeStopSpinBack, isTapeStopping;
    // Beat repeat
    int beatRepeatDiv;
    float beatRepeatDecay, beatRepeatPitch, beatRepeatMix, beatRepeatGate;
    bool isBeatRepeating;
    // DJFX looper
    int djfxLoopDiv;
    bool isDjfxLooping;
} TapeFX;

typedef struct {
    float volume[NUM_BUSES];
    float pan[NUM_BUSES];
    float reverbSend[NUM_BUSES];
    float delaySend[NUM_BUSES];
    bool mute[NUM_BUSES];
    bool solo[NUM_BUSES];
    // Per-bus FX
    bool octaverOn[NUM_BUSES]; float octaverMix[NUM_BUSES]; float octaverSubLevel[NUM_BUSES]; float octaverTone[NUM_BUSES];
    bool tremoloOn[NUM_BUSES]; float tremoloRate[NUM_BUSES]; float tremoloDepth[NUM_BUSES]; int tremoloShape[NUM_BUSES];
    bool leslieOn[NUM_BUSES]; int leslieSpeed[NUM_BUSES]; float leslieDrive[NUM_BUSES];
    float leslieBalance[NUM_BUSES]; float leslieDoppler[NUM_BUSES]; float leslieMix[NUM_BUSES];
    bool wahOn[NUM_BUSES]; int wahMode[NUM_BUSES]; float wahRate[NUM_BUSES]; float wahSensitivity[NUM_BUSES];
    float wahFreqLow[NUM_BUSES]; float wahFreqHigh[NUM_BUSES]; float wahResonance[NUM_BUSES]; float wahMix[NUM_BUSES];
    bool filterOn[NUM_BUSES];  float filterCut[NUM_BUSES]; float filterRes[NUM_BUSES]; int filterType[NUM_BUSES];
    bool distOn[NUM_BUSES];    float distDrive[NUM_BUSES]; float distMix[NUM_BUSES]; int distMode[NUM_BUSES];
    bool eqOn[NUM_BUSES];      float eqLowGain[NUM_BUSES]; float eqHighGain[NUM_BUSES];
    float eqLowFreq[NUM_BUSES]; float eqHighFreq[NUM_BUSES];
    bool chorusOn[NUM_BUSES];  bool chorusBBD[NUM_BUSES]; float chorusRate[NUM_BUSES]; float chorusDepth[NUM_BUSES];
    float chorusMix[NUM_BUSES]; float chorusDelay[NUM_BUSES]; float chorusFB[NUM_BUSES];
    bool phaserOn[NUM_BUSES];  float phaserRate[NUM_BUSES]; float phaserDepth[NUM_BUSES];
    float phaserMix[NUM_BUSES]; float phaserFB[NUM_BUSES]; int phaserStages[NUM_BUSES];
    bool combOn[NUM_BUSES];    float combFreq[NUM_BUSES]; float combFB[NUM_BUSES];
    float combMix[NUM_BUSES];  float combDamping[NUM_BUSES];
    bool wingieOn[NUM_BUSES];  int wingieMode[NUM_BUSES]; float wingiePitch[NUM_BUSES];
    float wingiePitch2[NUM_BUSES]; float wingiePitch3[NUM_BUSES];
    float wingieDecay[NUM_BUSES]; float wingieMix[NUM_BUSES];
    bool wingieCaveOn[NUM_BUSES][WINGIE_NUM_PARTIALS];
    bool pitchOn[NUM_BUSES];   float pitchSemitones[NUM_BUSES]; float pitchMix[NUM_BUSES];
    bool ringModOn[NUM_BUSES]; float ringModFreq[NUM_BUSES]; float ringModMix[NUM_BUSES];
    bool delayOn[NUM_BUSES];   bool delaySync[NUM_BUSES];  int delaySyncDiv[NUM_BUSES];
    float delayTime[NUM_BUSES]; float delayFB[NUM_BUSES];  float delayMix[NUM_BUSES];
    bool compOn[NUM_BUSES];    float compThreshold[NUM_BUSES]; float compRatio[NUM_BUSES];
    float compAttack[NUM_BUSES]; float compRelease[NUM_BUSES]; float compMakeup[NUM_BUSES];
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
    float chopSliceVolume[32]; // per-slot volume multiplier (applied at trigger, 1.0 = unity)

    // Master speed (global playback rate, 1.0 = normal)
    float masterSpeed;  // 0.25-2.0, default 1.0 (right-click to reset)

    // Chromatic sampler mode
    bool chromaticMode;       // keyboard plays sampler chromatically
    int chromaticSample;      // which sample slot to play (0-31)
    int chromaticRootNote;    // MIDI note for original pitch (default 60 = C4)

    // Arrangement (per-track pattern grid)
    Arrangement arr;

    // Clip launcher
    Launcher launcher;

    // Split keyboard
    bool splitEnabled;
    int splitPoint;
    int splitLeftPatch, splitRightPatch;
    int splitLeftOctave, splitRightOctave;
} DawState;

#endif // DAW_STATE_H
