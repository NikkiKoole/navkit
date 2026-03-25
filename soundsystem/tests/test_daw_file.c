// test_daw_file.c — Round-trip test for DAW save/load (daw_file.h)
//
// Build:
//   clang -std=c11 -O0 -g -I. -Ivendor -Wno-unused-function -Wno-unused-variable \
//         -o build/bin/test_daw_file soundsystem/tests/test_daw_file.c -lm
// Run:
//   ./build/bin/test_daw_file

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

// Pull in the synth engine (provides WaveType, enums, NUM_VOICES, etc.)
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/effects.h"
#include "../engines/sequencer.h"
#include "../engines/sampler.h"

// Undef engine macros that clash with DawState fields
#undef masterVolume
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode

// Use the real struct definitions from daw_state.h
#include "../engines/daw_state.h"

// Globals that daw_file.h accesses directly
static DawState daw;
static int patchPresetIndex[NUM_PATCHES];

// ============================================================================
// CHOP STATE — enables [sample] section round-trip testing
// ============================================================================

static struct {
    char sourcePath[256];
    int sourceLoops;
    int sourceSongIdx;
    float *fullData;
    int fullLength;
    int songChain[64];
    int songChainLen;
    int chainOffsets[65];
    float chainBpm;
    bool fullBounced;
    bool structureLoaded;
    bool patternBounced[64];
    float *patternCache[64];
    int patternCacheLen[64];
    float viewStart;
    float viewEnd;
    float selStart;
    float selEnd;
    bool hasSelection;
    bool draggingSel;
    bool draggingView;
    float dragViewOffset;
    int sliceCount;
    int chopMode;
    float sensitivity;
    bool normalize;
    int selectedSlice;
    bool browsingFiles;
    int browseScroll;
    bool tapSliceMode;
    bool showDetail;
    int draggingMarker;
    bool captureDropdownOpen;
    int captureGrabSeconds;
    int bankScrollOffset;
} chopState = {0};

// Track whether re-bounce was triggered during dawLoad
static int _testRebounceCallCount = 0;

// Stubs — re-bounce not tested here, just INI parsing
static void chopBounceFullSong(void) {}
static bool chopBounceNextPattern(void) { return false; }
static void dawAudioGate(void) {}
static void dawAudioUngate(void) {}

static bool _chopBounceActive = false;

#include "../engines/scratch_space.h"
static ScratchSpace scratch = {0};
static void scratchFromSelection(void) {
    _testRebounceCallCount++;
    // In test: create dummy scratch data so markers can be restored
    scratchFree(&scratch);
    float *buf = (float *)malloc(10000 * sizeof(float));
    if (buf) {
        for (int i = 0; i < 10000; i++) buf[i] = 0.0f;
        scratchLoad(&scratch, buf, 10000, 44100, 120.0f);
    }
}

static void chopStateClearTest(void) {
    memset(&chopState, 0, sizeof(chopState));
    chopState.selectedSlice = -1;
    chopState.sourceSongIdx = -1;
    for (int t = 0; t < 4; t++) daw.chopSliceMap[t] = -1;
}

#define DAW_HAS_CHOP_STATE

// Include the header-only save/load code
#include "../demo/daw_file.h"

// Helper to access saved_seq fields without triggering the `seq` macro
// saved_seq is a SequencerContext; its Sequencer is the first field (.seq)
// We use a pointer to access it safely.
#define SAVED_SEQ_DS(field) (saved_seq_ptr->field)

// ============================================================================
// TEST INFRASTRUCTURE
// ============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ_INT(a, b, msg) do { \
    int _a = (a), _b = (b); \
    if (_a != _b) { \
        printf("  FAIL: %s: expected %d, got %d\n", msg, _b, _a); \
        tests_failed++; return; \
    } else { tests_passed++; } \
} while(0)

#define ASSERT_EQ_FLOAT(a, b, msg) do { \
    float _a = (a), _b = (b); \
    if (fabsf(_a - _b) > 0.05f) { /* 0.05 to accommodate v2 quantization (uint8 vel, int8 pitch) */ \
        printf("  FAIL: %s: expected %.4f, got %.4f\n", msg, (double)_b, (double)_a); \
        tests_failed++; return; \
    } else { tests_passed++; } \
} while(0)

#define ASSERT_EQ_BOOL(a, b, msg) do { \
    bool _a = (a), _b = (b); \
    if (_a != _b) { \
        printf("  FAIL: %s: expected %s, got %s\n", msg, _b?"true":"false", _a?"true":"false"); \
        tests_failed++; return; \
    } else { tests_passed++; } \
} while(0)

#define ASSERT_EQ_STR(a, b, msg) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: %s: expected \"%s\", got \"%s\"\n", msg, (b), (a)); \
        tests_failed++; return; \
    } else { tests_passed++; } \
} while(0)

// ============================================================================
// SETUP: populate DawState + seq with non-default test values
// ============================================================================

static DawState saved_daw;
static SequencerContext saved_seq;
// Pointer to saved sequencer's Sequencer, avoids `seq` macro collision
static Sequencer *saved_seq_ptr;

static void setupTestState(void) {
    // Zero everything first
    memset(&daw, 0, sizeof(DawState));
    memset(&_seqCtx, 0, sizeof(SequencerContext));

    // -- Transport --
    daw.transport.bpm = 137.5f;
    daw.transport.grooveSwing = 5;
    daw.transport.grooveJitter = 3;
    daw.stepCount = 32;

    // -- Scale --
    daw.scaleLockEnabled = true;
    daw.scaleRoot = 7;   // G
    daw.scaleType = 3;

    // -- Groove (dilla + humanize) --
    seq.dilla.kickNudge = -3;
    seq.dilla.snareDelay = 4;
    seq.dilla.hatNudge = -1;
    seq.dilla.clapDelay = 2;
    seq.dilla.swing = 6;
    seq.dilla.jitter = 2;
    seq.humanize.timingJitter = 4;
    seq.humanize.velocityJitter = 0.15f;

    // -- Settings --
    daw.masterVol = 0.65f;
    daw.voiceRandomVowel = true;

    // -- Patches --
    for (int i = 0; i < NUM_PATCHES; i++) {
        daw.patches[i] = createDefaultPatch(WAVE_SAW);
        snprintf(daw.patches[i].p_name, 32, "Test%d", i);
        // Set distinctive values per patch
        daw.patches[i].p_waveType = i % 14;
        daw.patches[i].p_attack = 0.02f + i * 0.01f;
        daw.patches[i].p_decay = 0.15f + i * 0.02f;
        daw.patches[i].p_sustain = 0.3f + i * 0.05f;
        daw.patches[i].p_release = 0.2f + i * 0.03f;
        daw.patches[i].p_volume = 0.4f + i * 0.05f;
        daw.patches[i].p_filterCutoff = 0.3f + i * 0.08f;
        daw.patches[i].p_filterResonance = 0.1f + i * 0.1f;
        daw.patches[i].p_filterType = i % 3;
        daw.patches[i].p_filterEnvAmt = 0.2f + i * 0.05f;
        daw.patches[i].p_filterEnvAttack = 0.005f + i * 0.01f;
        daw.patches[i].p_filterEnvDecay = 0.1f + i * 0.02f;
        daw.patches[i].p_filterKeyTrack = 0.2f + i * 0.1f;
        daw.patches[i].p_filterLfoRate = 1.0f + i * 0.5f;
        daw.patches[i].p_filterLfoDepth = 0.1f + i * 0.1f;
        daw.patches[i].p_filterLfoShape = i % 5;
        daw.patches[i].p_filterLfoSync = i % 8;
        daw.patches[i].p_resoLfoRate = 2.0f + i;
        daw.patches[i].p_resoLfoDepth = 0.05f * i;
        daw.patches[i].p_resoLfoShape = (i + 1) % 5;
        daw.patches[i].p_ampLfoRate = 3.0f + i;
        daw.patches[i].p_ampLfoDepth = 0.1f * i;
        daw.patches[i].p_ampLfoShape = (i + 2) % 5;
        daw.patches[i].p_pitchLfoRate = 4.0f + i;
        daw.patches[i].p_pitchLfoDepth = 0.05f * i;
        daw.patches[i].p_pitchLfoShape = (i + 3) % 5;
        daw.patches[i].p_pulseWidth = 0.3f + i * 0.05f;
        daw.patches[i].p_pwmRate = 2.0f + i;
        daw.patches[i].p_pwmDepth = 0.1f * i;
        daw.patches[i].p_vibratoRate = 4.0f + i;
        daw.patches[i].p_vibratoDepth = 0.01f * i;
        daw.patches[i].p_arpEnabled = (i % 3 == 0);
        daw.patches[i].p_arpMode = i % 4;
        daw.patches[i].p_arpRateDiv = i % 5;
        daw.patches[i].p_arpRate = 6.0f + i;
        daw.patches[i].p_arpChord = i % 7;
        daw.patches[i].p_unisonCount = 1 + (i % 4);
        daw.patches[i].p_unisonDetune = 5.0f + i * 3.0f;
        daw.patches[i].p_unisonMix = 0.3f + i * 0.05f;
        daw.patches[i].p_monoMode = (i % 2 == 0);
        daw.patches[i].p_glideTime = 0.05f + i * 0.02f;
        daw.patches[i].p_pluckBrightness = 0.4f + i * 0.05f;
        daw.patches[i].p_pluckDamping = 0.99f - i * 0.002f;
        daw.patches[i].p_pluckDamp = 0.1f * i;
        daw.patches[i].p_additivePreset = i % 5;
        daw.patches[i].p_additiveBrightness = 0.3f + i * 0.05f;
        daw.patches[i].p_additiveShimmer = 0.1f * i;
        daw.patches[i].p_additiveInharmonicity = 0.05f * i;
        daw.patches[i].p_malletPreset = i % 5;
        daw.patches[i].p_malletStiffness = 0.2f + i * 0.05f;
        daw.patches[i].p_malletHardness = 0.3f + i * 0.05f;
        daw.patches[i].p_malletStrikePos = 0.1f + i * 0.05f;
        daw.patches[i].p_malletResonance = 0.5f + i * 0.05f;
        daw.patches[i].p_malletTremolo = 0.05f * i;
        daw.patches[i].p_malletTremoloRate = 4.0f + i;
        daw.patches[i].p_malletDamp = 0.1f * i;
        daw.patches[i].p_voiceVowel = i % 5;
        daw.patches[i].p_voiceFormantShift = 0.8f + i * 0.05f;
        daw.patches[i].p_voiceBreathiness = 0.05f + i * 0.05f;
        daw.patches[i].p_voiceBuzziness = 0.4f + i * 0.05f;
        daw.patches[i].p_voiceSpeed = 8.0f + i;
        daw.patches[i].p_voicePitch = 0.9f + i * 0.02f;
        daw.patches[i].p_voiceConsonant = (i % 2 == 1);
        daw.patches[i].p_voiceConsonantAmt = 0.3f + i * 0.05f;
        daw.patches[i].p_voiceNasal = (i % 3 == 0);
        daw.patches[i].p_voiceNasalAmt = 0.2f + i * 0.05f;
        daw.patches[i].p_voicePitchEnv = 0.1f * i;
        daw.patches[i].p_voicePitchEnvTime = 0.1f + i * 0.02f;
        daw.patches[i].p_voicePitchEnvCurve = -0.5f + i * 0.15f;
        daw.patches[i].p_granularScwIndex = i % 4;
        daw.patches[i].p_granularGrainSize = 30.0f + i * 5.0f;
        daw.patches[i].p_granularDensity = 15.0f + i * 2.0f;
        daw.patches[i].p_granularPosition = 0.2f + i * 0.1f;
        daw.patches[i].p_granularPosRandom = 0.05f + i * 0.02f;
        daw.patches[i].p_granularPitch = 0.8f + i * 0.05f;
        daw.patches[i].p_granularPitchRandom = 0.01f * i;
        daw.patches[i].p_granularAmpRandom = 0.05f + i * 0.02f;
        daw.patches[i].p_granularSpread = 0.3f + i * 0.05f;
        daw.patches[i].p_granularFreeze = (i % 4 == 0);
        daw.patches[i].p_fmModRatio = 1.5f + i * 0.3f;
        daw.patches[i].p_fmModIndex = 0.5f + i * 0.2f;
        daw.patches[i].p_fmFeedback = 0.1f * i;
        daw.patches[i].p_pdWaveType = i % 5;
        daw.patches[i].p_pdDistortion = 0.3f + i * 0.05f;
        daw.patches[i].p_membranePreset = i % 5;
        daw.patches[i].p_membraneDamping = 0.2f + i * 0.05f;
        daw.patches[i].p_membraneStrike = 0.2f + i * 0.05f;
        daw.patches[i].p_membraneBend = 0.1f + i * 0.02f;
        daw.patches[i].p_membraneBendDecay = 0.05f + i * 0.01f;
        daw.patches[i].p_birdType = i % 5;
        daw.patches[i].p_birdChirpRange = 0.5f + i * 0.1f;
        daw.patches[i].p_birdTrillRate = 0.1f * i;
        daw.patches[i].p_birdTrillDepth = 0.05f * i;
        daw.patches[i].p_birdAmRate = 0.2f * i;
        daw.patches[i].p_birdAmDepth = 0.1f * i;
        daw.patches[i].p_birdHarmonics = 0.1f + i * 0.05f;
        // Pitch envelope
        daw.patches[i].p_pitchEnvAmount = 12.0f - i * 2.0f;
        daw.patches[i].p_pitchEnvDecay = 0.02f + i * 0.01f;
        daw.patches[i].p_pitchEnvCurve = -0.3f + i * 0.1f;
        daw.patches[i].p_pitchEnvLinear = (i % 2 == 0);
        // Noise
        daw.patches[i].p_noiseMix = 0.1f * i;
        daw.patches[i].p_noiseTone = 0.3f + i * 0.05f;
        daw.patches[i].p_noiseHP = 0.05f * i;
        daw.patches[i].p_noiseDecay = 0.02f * i;
        // Retrigger
        daw.patches[i].p_retriggerCount = i % 5;
        daw.patches[i].p_retriggerSpread = 0.01f + i * 0.005f;
        daw.patches[i].p_retriggerOverlap = (i % 3 == 0);
        daw.patches[i].p_retriggerBurstDecay = 0.01f + i * 0.005f;
        // Extra oscs
        daw.patches[i].p_osc2Ratio = 1.5f + i * 0.1f;
        daw.patches[i].p_osc2Level = 0.3f + i * 0.05f;
        daw.patches[i].p_osc3Ratio = 2.0f + i * 0.1f;
        daw.patches[i].p_osc3Level = 0.2f + i * 0.05f;
        daw.patches[i].p_osc4Ratio = 3.0f + i * 0.1f;
        daw.patches[i].p_osc4Level = 0.15f + i * 0.03f;
        daw.patches[i].p_osc5Ratio = 4.0f + i * 0.2f;
        daw.patches[i].p_osc5Level = 0.1f + i * 0.02f;
        daw.patches[i].p_osc6Ratio = 5.0f + i * 0.2f;
        daw.patches[i].p_osc6Level = 0.08f + i * 0.01f;
        // Drive, click
        daw.patches[i].p_drive = 0.1f * i;
        daw.patches[i].p_clickLevel = 0.2f + i * 0.05f;
        daw.patches[i].p_clickTime = 0.003f + i * 0.001f;
        // Algorithm modes
        daw.patches[i].p_noiseMode = i % 3;
        daw.patches[i].p_oscMixMode = i % 2;
        daw.patches[i].p_retriggerCurve = 0.1f * i;
        daw.patches[i].p_phaseReset = (i % 2 == 1);
        daw.patches[i].p_noiseLPBypass = (i % 3 == 0);
        daw.patches[i].p_noiseType = i % 2;
        // Trigger freq
        daw.patches[i].p_useTriggerFreq = (i < 4);
        daw.patches[i].p_triggerFreq = 50.0f + i * 100.0f;
        daw.patches[i].p_choke = (i == 2);
        // Exp/oneShot
        daw.patches[i].p_expRelease = (i % 2 == 0);
        daw.patches[i].p_expDecay = (i % 3 == 0);
        daw.patches[i].p_oneShot = (i < 4);
        daw.patches[i].p_scwIndex = i * 2;
    }

    // -- Mixer --
    for (int b = 0; b < NUM_BUSES; b++) {
        daw.mixer.volume[b] = 0.5f + b * 0.05f;
        daw.mixer.pan[b] = -0.3f + b * 0.1f;
        daw.mixer.reverbSend[b] = 0.1f + b * 0.05f;
        daw.mixer.mute[b] = (b == 2);
        daw.mixer.solo[b] = (b == 5);
        daw.mixer.filterOn[b] = (b % 2 == 0);
        daw.mixer.filterCut[b] = 0.4f + b * 0.08f;
        daw.mixer.filterRes[b] = 0.1f + b * 0.1f;
        daw.mixer.filterType[b] = b % 3;
        daw.mixer.distOn[b] = (b % 3 == 0);
        daw.mixer.distDrive[b] = 1.5f + b * 0.3f;
        daw.mixer.distMix[b] = 0.3f + b * 0.05f;
        daw.mixer.delayOn[b] = (b % 2 == 1);
        daw.mixer.delaySync[b] = (b % 3 == 1);
        daw.mixer.delaySyncDiv[b] = b % 5;
        daw.mixer.delayTime[b] = 0.2f + b * 0.05f;
        daw.mixer.delayFB[b] = 0.2f + b * 0.05f;
        daw.mixer.delayMix[b] = 0.2f + b * 0.04f;
    }

    // -- Sidechain --
    daw.sidechain.on = true;
    daw.sidechain.source = 0;
    daw.sidechain.target = 4;
    daw.sidechain.depth = 0.7f;
    daw.sidechain.attack = 0.01f;
    daw.sidechain.release = 0.2f;

    // -- MasterFX --
    daw.masterFx.distOn = true;
    daw.masterFx.distDrive = 3.0f;
    daw.masterFx.distTone = 0.6f;
    daw.masterFx.distMix = 0.4f;
    daw.masterFx.crushOn = true;
    daw.masterFx.crushBits = 12.0f;
    daw.masterFx.crushRate = 3.0f;
    daw.masterFx.crushMix = 0.35f;
    daw.masterFx.chorusOn = true;
    daw.masterFx.chorusRate = 1.5f;
    daw.masterFx.chorusDepth = 0.4f;
    daw.masterFx.chorusMix = 0.25f;
    daw.masterFx.flangerOn = true;
    daw.masterFx.flangerRate = 0.7f;
    daw.masterFx.flangerDepth = 0.6f;
    daw.masterFx.flangerFeedback = 0.4f;
    daw.masterFx.flangerMix = 0.35f;
    daw.masterFx.tapeOn = true;
    daw.masterFx.tapeSaturation = 0.4f;
    daw.masterFx.tapeWow = 0.15f;
    daw.masterFx.tapeFlutter = 0.12f;
    daw.masterFx.tapeHiss = 0.08f;
    daw.masterFx.delayOn = true;
    daw.masterFx.delayTime = 0.35f;
    daw.masterFx.delayFeedback = 0.45f;
    daw.masterFx.delayTone = 0.55f;
    daw.masterFx.delayMix = 0.25f;
    daw.masterFx.reverbOn = true;
    daw.masterFx.reverbSize = 0.6f;
    daw.masterFx.reverbDamping = 0.4f;
    daw.masterFx.reverbPreDelay = 0.03f;
    daw.masterFx.reverbMix = 0.35f;
    daw.masterFx.vinylOn = true;
    daw.masterFx.vinylCrackle = 0.4f;
    daw.masterFx.vinylNoise = 0.15f;
    daw.masterFx.vinylWarp = 0.2f;
    daw.masterFx.vinylWarpRate = 0.8f;
    daw.masterFx.vinylTone = 0.7f;

    // -- TapeFX --
    daw.tapeFx.enabled = true;
    daw.tapeFx.headTime = 0.6f;
    daw.tapeFx.feedback = 0.55f;
    daw.tapeFx.mix = 0.45f;
    daw.tapeFx.inputSource = 2;
    daw.tapeFx.preReverb = true;
    daw.tapeFx.saturation = 0.35f;
    daw.tapeFx.toneHigh = 0.65f;
    daw.tapeFx.noise = 0.04f;
    daw.tapeFx.degradeRate = 0.12f;
    daw.tapeFx.wow = 0.08f;
    daw.tapeFx.flutter = 0.09f;
    daw.tapeFx.drift = 0.04f;
    daw.tapeFx.speedTarget = 0.95f;
    daw.tapeFx.speedSlew = 0.15f;
    daw.tapeFx.rewindTime = 1.8f;
    daw.tapeFx.rewindMinSpeed = 0.15f;
    daw.tapeFx.rewindVinyl = 0.35f;
    daw.tapeFx.rewindWobble = 0.25f;
    daw.tapeFx.rewindFilter = 0.45f;
    daw.tapeFx.rewindCurve = 2;
    daw.tapeFx.tapeStopTime = 0.8f;
    daw.tapeFx.tapeStopCurve = 2;
    daw.tapeFx.tapeStopSpinBack = true;
    daw.tapeFx.tapeStopSpinTime = 0.5f;
    daw.tapeFx.beatRepeatDiv = 1;
    daw.tapeFx.beatRepeatDecay = 0.2f;
    daw.tapeFx.beatRepeatPitch = -3.0f;
    daw.tapeFx.beatRepeatMix = 0.8f;
    daw.tapeFx.beatRepeatGate = 0.75f;
    daw.tapeFx.djfxLoopDiv = 3;

    // -- Crossfader --
    daw.crossfader.enabled = true;
    daw.crossfader.pos = 0.7f;
    daw.crossfader.sceneA = 2;
    daw.crossfader.sceneB = 5;
    daw.crossfader.count = 4;

    // -- Split keyboard --
    daw.splitEnabled = true;
    daw.splitPoint = 64;
    daw.splitLeftPatch = 3;
    daw.splitRightPatch = 5;
    daw.splitLeftOctave = -1;
    daw.splitRightOctave = 2;

    // -- Song arrangement --
    daw.song.length = 6;
    daw.song.loopsPerPattern = 4;
    daw.song.songMode = true;
    daw.song.patterns[0] = 0; daw.song.patterns[1] = 1; daw.song.patterns[2] = 2;
    daw.song.patterns[3] = 3; daw.song.patterns[4] = 0; daw.song.patterns[5] = 7;
    daw.song.loopsPerSection[0] = 2; daw.song.loopsPerSection[1] = 4;
    daw.song.loopsPerSection[2] = 1; daw.song.loopsPerSection[3] = 3;
    daw.song.loopsPerSection[4] = 2; daw.song.loopsPerSection[5] = 1;
    strncpy(daw.song.names[0], "intro", SONG_SECTION_NAME_LEN - 1);
    strncpy(daw.song.names[1], "verse", SONG_SECTION_NAME_LEN - 1);
    strncpy(daw.song.names[2], "chorus", SONG_SECTION_NAME_LEN - 1);
    strncpy(daw.song.names[3], "bridge", SONG_SECTION_NAME_LEN - 1);
    strncpy(daw.song.names[5], "outro", SONG_SECTION_NAME_LEN - 1);

    // -- Patterns --
    for (int p = 0; p < SEQ_NUM_PATTERNS; p++) initPattern(&seq.patterns[p]);

    // Pattern 0: drum steps + melody notes + plocks + note pools
    Pattern *p0 = &seq.patterns[0];

    // Drum steps
    patSetDrumLength(p0, 0, 16); patSetDrumLength(p0, 1, 16);
    patSetDrumLength(p0, 2, 12); patSetDrumLength(p0, 3, 8);
    patSetDrum(p0, 0, 0, 0.9f, 0.1f);
    patSetDrumProb(p0, 0, 0, 0.8f);
    patSetDrum(p0, 0, 4, 0.85f, 0.0f);
    patSetDrumCond(p0, 0, 4, COND_1_2);
    patSetDrum(p0, 1, 4, 0.75f, -0.2f);
    patSetDrumProb(p0, 1, 4, 0.7f);
    patSetDrumCond(p0, 1, 4, COND_FILL);
    patSetDrum(p0, 2, 0, 0.6f, 0.0f);
    patSetDrum(p0, 2, 2, 0.5f, 0.0f);
    patSetDrum(p0, 3, 3, 0.7f, 0.0f);
    patSetDrumProb(p0, 3, 3, 0.5f);
    patSetDrumCond(p0, 3, 3, COND_2_4);

    // Melody notes
    patSetMelodyLength(p0, SEQ_DRUM_TRACKS + 0, 16); patSetMelodyLength(p0, SEQ_DRUM_TRACKS + 1, 8); patSetMelodyLength(p0, SEQ_DRUM_TRACKS + 2, 16);
    patSetNote(p0, SEQ_DRUM_TRACKS + 0, 0, 48, 0.8f, 4);  // C3
    patSetNoteFlags(p0, SEQ_DRUM_TRACKS + 0, 0, false, true);
    patSetNoteSustain(p0, SEQ_DRUM_TRACKS + 0, 0, 2);
    patSetNoteProb(p0, SEQ_DRUM_TRACKS + 0, 0, 0.9f);

    patSetNote(p0, SEQ_DRUM_TRACKS + 0, 4, 55, 0.7f, 2);  // G3
    patSetNoteFlags(p0, SEQ_DRUM_TRACKS + 0, 4, true, false);
    patSetNoteProb(p0, SEQ_DRUM_TRACKS + 0, 4, 0.6f);
    patSetNoteCond(p0, SEQ_DRUM_TRACKS + 0, 4, COND_NOT_FIRST);

    patSetNote(p0, SEQ_DRUM_TRACKS + 1, 0, 60, 0.9f, 8);  // C4

    // Custom chord on melody track 0, step 0 (v2 multi-note step)
    patSetChordCustom(p0, SEQ_DRUM_TRACKS + 0, 0, 0.8f, 4, 48, 52, 55, -1);

    // Standard chord on melody track 0, step 4 (v2 multi-note step)
    patSetChord(p0, SEQ_DRUM_TRACKS + 0, 4, patGetNote(p0, SEQ_DRUM_TRACKS + 0, 4), CHORD_TRIAD, patGetNoteVel(p0, SEQ_DRUM_TRACKS + 0, 4), patGetNoteGate(p0, SEQ_DRUM_TRACKS + 0, 4));

    // P-locks
    seqSetPLock(p0, 0, 0, PLOCK_FILTER_CUTOFF, 0.75f);
    seqSetPLock(p0, 0, 0, PLOCK_VOLUME, 0.9f);
    seqSetPLock(p0, 1, 4, PLOCK_DECAY, 0.3f);
    seqSetPLock(p0, 4, 0, PLOCK_PITCH_OFFSET, 2.0f);

    // Pattern 3: a second non-empty pattern (different track lengths)
    Pattern *p3 = &seq.patterns[3];
    patSetDrumLength(p3, 0, 32); patSetDrumLength(p3, 1, 16);
    patSetDrumLength(p3, 2, 16); patSetDrumLength(p3, 3, 16);
    patSetMelodyLength(p3, SEQ_DRUM_TRACKS + 0, 32); patSetMelodyLength(p3, SEQ_DRUM_TRACKS + 1, 16); patSetMelodyLength(p3, SEQ_DRUM_TRACKS + 2, 16);
    patSetDrum(p3, 0, 0, 1.0f, 0.0f);
    patSetDrum(p3, 0, 8, 0.8f, 0.0f);
    patSetNote(p3, SEQ_DRUM_TRACKS + 0, 0, 36, 0.85f, 6);  // C2

    // Sampler track on pattern 0: slices with pitch offsets (new model: slice field + note=pitch)
    p0->trackLength[SEQ_TRACK_SAMPLER] = 8;
    {
        int t = SEQ_TRACK_SAMPLER;
        // Step 0: slice 3, pitch 65 (+5 from center), vel 0.9
        StepV2 *sv0 = &p0->steps[t][0];
        sv0->noteCount = 1;
        sv0->notes[0].slice = 3;
        sv0->notes[0].note = 65;  // 60 + 5
        sv0->notes[0].velocity = velFloatToU8(0.9f);
        // Step 2: slice 7, pitch 60 (center), vel 0.6
        StepV2 *sv2 = &p0->steps[t][2];
        sv2->noteCount = 1;
        sv2->notes[0].slice = 7;
        sv2->notes[0].note = 60;
        sv2->notes[0].velocity = velFloatToU8(0.6f);
        // Step 5: slice 0, pitch 57 (-3 from center), vel 1.0, with prob + cond
        StepV2 *sv5 = &p0->steps[t][5];
        sv5->noteCount = 1;
        sv5->notes[0].slice = 0;
        sv5->notes[0].note = 57;  // 60 - 3
        sv5->notes[0].velocity = velFloatToU8(1.0f);
        sv5->probability = probFloatToU8(0.75f);
        sv5->condition = COND_1_2;
    }

    // Save copies for comparison
    memcpy(&saved_daw, &daw, sizeof(DawState));
    memcpy(&saved_seq, &_seqCtx, sizeof(SequencerContext));
    // Use pointer arithmetic to get the Sequencer field (first member)
    // without triggering the `seq` macro
    saved_seq_ptr = (Sequencer *)&saved_seq;
}

// ============================================================================
// VERIFY FUNCTIONS
// ============================================================================

static void verifySongHeader(void) {
    printf("  [song header]\n");
    ASSERT_EQ_FLOAT(daw.transport.bpm, saved_daw.transport.bpm, "bpm");
    ASSERT_EQ_INT(daw.stepCount, saved_daw.stepCount, "stepCount");
    ASSERT_EQ_INT(daw.transport.grooveSwing, saved_daw.transport.grooveSwing, "grooveSwing");
    ASSERT_EQ_INT(daw.transport.grooveJitter, saved_daw.transport.grooveJitter, "grooveJitter");
}

static void verifyScale(void) {
    printf("  [scale]\n");
    ASSERT_EQ_BOOL(daw.scaleLockEnabled, saved_daw.scaleLockEnabled, "scaleLockEnabled");
    ASSERT_EQ_INT(daw.scaleRoot, saved_daw.scaleRoot, "scaleRoot");
    ASSERT_EQ_INT(daw.scaleType, saved_daw.scaleType, "scaleType");
}

static void verifyGroove(void) {
    printf("  [groove]\n");
    ASSERT_EQ_INT(seq.dilla.kickNudge, SAVED_SEQ_DS(dilla.kickNudge), "kickNudge");
    ASSERT_EQ_INT(seq.dilla.snareDelay, SAVED_SEQ_DS(dilla.snareDelay), "snareDelay");
    ASSERT_EQ_INT(seq.dilla.hatNudge, SAVED_SEQ_DS(dilla.hatNudge), "hatNudge");
    ASSERT_EQ_INT(seq.dilla.clapDelay, SAVED_SEQ_DS(dilla.clapDelay), "clapDelay");
    ASSERT_EQ_INT(seq.dilla.swing, SAVED_SEQ_DS(dilla.swing), "swing");
    ASSERT_EQ_INT(seq.dilla.jitter, SAVED_SEQ_DS(dilla.jitter), "jitter");
    ASSERT_EQ_INT(seq.humanize.timingJitter, SAVED_SEQ_DS(humanize.timingJitter), "melodyTimingJitter");
    ASSERT_EQ_FLOAT(seq.humanize.velocityJitter, SAVED_SEQ_DS(humanize.velocityJitter), "melodyVelocityJitter");
}

static void verifySettings(void) {
    printf("  [settings]\n");
    ASSERT_EQ_FLOAT(daw.masterVol, saved_daw.masterVol, "masterVol");
    ASSERT_EQ_BOOL(daw.voiceRandomVowel, saved_daw.voiceRandomVowel, "voiceRandomVowel");
}

static void verifyPatch(int i) {
    char label[64];
    const SynthPatch *a = &daw.patches[i];
    const SynthPatch *b = &saved_daw.patches[i];

    #define PF(field) do { snprintf(label, sizeof(label), "patch[%d]." #field, i); \
        ASSERT_EQ_FLOAT(a->field, b->field, label); } while(0)
    #define PINT(field) do { snprintf(label, sizeof(label), "patch[%d]." #field, i); \
        ASSERT_EQ_INT(a->field, b->field, label); } while(0)
    #define PB(field) do { snprintf(label, sizeof(label), "patch[%d]." #field, i); \
        ASSERT_EQ_BOOL(a->field, b->field, label); } while(0)

    snprintf(label, sizeof(label), "patch[%d].p_name", i);
    ASSERT_EQ_STR(a->p_name, b->p_name, label);

    PINT(p_waveType); PINT(p_scwIndex);
    PF(p_attack); PF(p_decay); PF(p_sustain); PF(p_release); PF(p_volume);
    PB(p_expRelease); PB(p_expDecay); PB(p_oneShot);
    PF(p_pulseWidth); PF(p_pwmRate); PF(p_pwmDepth);
    PF(p_vibratoRate); PF(p_vibratoDepth);
    PF(p_filterCutoff); PF(p_filterResonance); PINT(p_filterType);
    PF(p_filterEnvAmt); PF(p_filterEnvAttack); PF(p_filterEnvDecay);
    PF(p_filterLfoRate); PF(p_filterLfoDepth); PINT(p_filterLfoShape); PINT(p_filterLfoSync);
    PF(p_resoLfoRate); PF(p_resoLfoDepth); PINT(p_resoLfoShape);
    PF(p_ampLfoRate); PF(p_ampLfoDepth); PINT(p_ampLfoShape);
    PF(p_pitchLfoRate); PF(p_pitchLfoDepth); PINT(p_pitchLfoShape);
    PB(p_arpEnabled); PINT(p_arpMode); PINT(p_arpRateDiv); PF(p_arpRate); PINT(p_arpChord);
    PINT(p_unisonCount); PF(p_unisonDetune); PF(p_unisonMix);
    PB(p_monoMode); PF(p_glideTime);
    PF(p_pluckBrightness); PF(p_pluckDamping); PF(p_pluckDamp);
    PINT(p_additivePreset); PF(p_additiveBrightness); PF(p_additiveShimmer); PF(p_additiveInharmonicity);
    PINT(p_malletPreset); PF(p_malletStiffness); PF(p_malletHardness); PF(p_malletStrikePos);
    PF(p_malletResonance); PF(p_malletTremolo); PF(p_malletTremoloRate); PF(p_malletDamp);
    PINT(p_voiceVowel); PF(p_voiceFormantShift); PF(p_voiceBreathiness); PF(p_voiceBuzziness);
    PF(p_voiceSpeed); PF(p_voicePitch);
    PB(p_voiceConsonant); PF(p_voiceConsonantAmt); PB(p_voiceNasal); PF(p_voiceNasalAmt);
    PF(p_voicePitchEnv); PF(p_voicePitchEnvTime); PF(p_voicePitchEnvCurve);
    PINT(p_granularScwIndex); PF(p_granularGrainSize); PF(p_granularDensity);
    PF(p_granularPosition); PF(p_granularPosRandom); PF(p_granularPitch);
    PF(p_granularPitchRandom); PF(p_granularAmpRandom); PF(p_granularSpread);
    PB(p_granularFreeze);
    PF(p_fmModRatio); PF(p_fmModIndex); PF(p_fmFeedback);
    PINT(p_pdWaveType); PF(p_pdDistortion);
    PINT(p_membranePreset); PF(p_membraneDamping); PF(p_membraneStrike); PF(p_membraneBend); PF(p_membraneBendDecay);
    PINT(p_birdType); PF(p_birdChirpRange); PF(p_birdTrillRate); PF(p_birdTrillDepth);
    PF(p_birdAmRate); PF(p_birdAmDepth); PF(p_birdHarmonics);
    PF(p_pitchEnvAmount); PF(p_pitchEnvDecay); PF(p_pitchEnvCurve); PB(p_pitchEnvLinear);
    PF(p_noiseMix); PF(p_noiseTone); PF(p_noiseHP); PF(p_noiseDecay);
    PINT(p_retriggerCount); PF(p_retriggerSpread); PB(p_retriggerOverlap); PF(p_retriggerBurstDecay);
    PF(p_osc2Ratio); PF(p_osc2Level); PF(p_osc3Ratio); PF(p_osc3Level);
    PF(p_osc4Ratio); PF(p_osc4Level); PF(p_osc5Ratio); PF(p_osc5Level);
    PF(p_osc6Ratio); PF(p_osc6Level);
    PF(p_drive);
    PF(p_clickLevel); PF(p_clickTime);
    PINT(p_noiseMode); PINT(p_oscMixMode); PF(p_retriggerCurve);
    PB(p_phaseReset); PB(p_noiseLPBypass); PINT(p_noiseType);
    PB(p_useTriggerFreq); PF(p_triggerFreq); PB(p_choke);

    #undef PF
    #undef PINT
    #undef PB
}

static void verifyAllPatches(void) {
    printf("  [patches]\n");
    for (int i = 0; i < NUM_PATCHES; i++) verifyPatch(i);
}

static void verifyMixer(void) {
    printf("  [mixer]\n");
    char label[64];
    for (int b = 0; b < NUM_BUSES; b++) {
        #define MF(field) do { snprintf(label, sizeof(label), "mixer." #field "[%d]", b); \
            ASSERT_EQ_FLOAT(daw.mixer.field[b], saved_daw.mixer.field[b], label); } while(0)
        #define MI(field) do { snprintf(label, sizeof(label), "mixer." #field "[%d]", b); \
            ASSERT_EQ_INT(daw.mixer.field[b], saved_daw.mixer.field[b], label); } while(0)
        #define MB(field) do { snprintf(label, sizeof(label), "mixer." #field "[%d]", b); \
            ASSERT_EQ_BOOL(daw.mixer.field[b], saved_daw.mixer.field[b], label); } while(0)

        MF(volume); MF(pan); MF(reverbSend);
        MB(mute); MB(solo);
        MB(filterOn); MF(filterCut); MF(filterRes); MI(filterType);
        MB(distOn); MF(distDrive); MF(distMix);
        MB(delayOn); MB(delaySync); MI(delaySyncDiv);
        MF(delayTime); MF(delayFB); MF(delayMix);

        #undef MF
        #undef MI
        #undef MB
    }
}

static void verifySidechain(void) {
    printf("  [sidechain]\n");
    ASSERT_EQ_BOOL(daw.sidechain.on, saved_daw.sidechain.on, "sc.on");
    ASSERT_EQ_INT(daw.sidechain.source, saved_daw.sidechain.source, "sc.source");
    ASSERT_EQ_INT(daw.sidechain.target, saved_daw.sidechain.target, "sc.target");
    ASSERT_EQ_FLOAT(daw.sidechain.depth, saved_daw.sidechain.depth, "sc.depth");
    ASSERT_EQ_FLOAT(daw.sidechain.attack, saved_daw.sidechain.attack, "sc.attack");
    ASSERT_EQ_FLOAT(daw.sidechain.release, saved_daw.sidechain.release, "sc.release");
}

static void verifyMasterFx(void) {
    printf("  [masterfx]\n");
    #define MXF(field) ASSERT_EQ_FLOAT(daw.masterFx.field, saved_daw.masterFx.field, "mfx." #field)
    #define MXB(field) ASSERT_EQ_BOOL(daw.masterFx.field, saved_daw.masterFx.field, "mfx." #field)
    MXB(distOn); MXF(distDrive); MXF(distTone); MXF(distMix);
    MXB(crushOn); MXF(crushBits); MXF(crushRate); MXF(crushMix);
    MXB(chorusOn); MXF(chorusRate); MXF(chorusDepth); MXF(chorusMix);
    MXB(flangerOn); MXF(flangerRate); MXF(flangerDepth); MXF(flangerFeedback); MXF(flangerMix);
    MXB(tapeOn); MXF(tapeSaturation); MXF(tapeWow); MXF(tapeFlutter); MXF(tapeHiss);
    MXB(delayOn); MXF(delayTime); MXF(delayFeedback); MXF(delayTone); MXF(delayMix);
    MXB(reverbOn); MXF(reverbSize); MXF(reverbDamping); MXF(reverbPreDelay); MXF(reverbMix);
    MXB(vinylOn); MXF(vinylCrackle); MXF(vinylNoise); MXF(vinylWarp); MXF(vinylWarpRate); MXF(vinylTone);
    #undef MXF
    #undef MXB
}

static void verifyTapeFx(void) {
    printf("  [tapefx]\n");
    #define TF(field) ASSERT_EQ_FLOAT(daw.tapeFx.field, saved_daw.tapeFx.field, "tape." #field)
    #define TB(field) ASSERT_EQ_BOOL(daw.tapeFx.field, saved_daw.tapeFx.field, "tape." #field)
    #define TI(field) ASSERT_EQ_INT(daw.tapeFx.field, saved_daw.tapeFx.field, "tape." #field)
    TB(enabled); TF(headTime); TF(feedback); TF(mix); TI(inputSource); TB(preReverb);
    TF(saturation); TF(toneHigh); TF(noise); TF(degradeRate);
    TF(wow); TF(flutter); TF(drift); TF(speedTarget);
    TF(rewindTime); TF(rewindMinSpeed); TF(rewindVinyl); TF(rewindWobble); TF(rewindFilter);
    TI(rewindCurve);
    TF(tapeStopTime); TI(tapeStopCurve); TB(tapeStopSpinBack); TF(tapeStopSpinTime);
    TI(beatRepeatDiv); TF(beatRepeatDecay); TF(beatRepeatPitch); TF(beatRepeatMix); TF(beatRepeatGate);
    TI(djfxLoopDiv);
    #undef TF
    #undef TB
    #undef TI
}

static void verifyCrossfader(void) {
    printf("  [crossfader]\n");
    ASSERT_EQ_BOOL(daw.crossfader.enabled, saved_daw.crossfader.enabled, "xf.enabled");
    ASSERT_EQ_FLOAT(daw.crossfader.pos, saved_daw.crossfader.pos, "xf.pos");
    ASSERT_EQ_INT(daw.crossfader.sceneA, saved_daw.crossfader.sceneA, "xf.sceneA");
    ASSERT_EQ_INT(daw.crossfader.sceneB, saved_daw.crossfader.sceneB, "xf.sceneB");
    ASSERT_EQ_INT(daw.crossfader.count, saved_daw.crossfader.count, "xf.count");
}

static void verifySplit(void) {
    printf("  [split]\n");
    ASSERT_EQ_BOOL(daw.splitEnabled, saved_daw.splitEnabled, "split.enabled");
    ASSERT_EQ_INT(daw.splitPoint, saved_daw.splitPoint, "split.point");
    ASSERT_EQ_INT(daw.splitLeftPatch, saved_daw.splitLeftPatch, "split.leftPatch");
    ASSERT_EQ_INT(daw.splitRightPatch, saved_daw.splitRightPatch, "split.rightPatch");
    ASSERT_EQ_INT(daw.splitLeftOctave, saved_daw.splitLeftOctave, "split.leftOctave");
    ASSERT_EQ_INT(daw.splitRightOctave, saved_daw.splitRightOctave, "split.rightOctave");
}

static void verifyArrangement(void) {
    printf("  [arrangement]\n");
    ASSERT_EQ_INT(daw.song.length, saved_daw.song.length, "song.length");
    ASSERT_EQ_INT(daw.song.loopsPerPattern, saved_daw.song.loopsPerPattern, "song.loopsPerPattern");
    ASSERT_EQ_BOOL(daw.song.songMode, saved_daw.song.songMode, "song.songMode");
    char label[64];
    for (int i = 0; i < saved_daw.song.length; i++) {
        snprintf(label, sizeof(label), "song.patterns[%d]", i);
        ASSERT_EQ_INT(daw.song.patterns[i], saved_daw.song.patterns[i], label);
        snprintf(label, sizeof(label), "song.loops[%d]", i);
        ASSERT_EQ_INT(daw.song.loopsPerSection[i], saved_daw.song.loopsPerSection[i], label);
        snprintf(label, sizeof(label), "song.names[%d]", i);
        ASSERT_EQ_STR(daw.song.names[i], saved_daw.song.names[i], label);
    }
}

static void verifyPatterns(void) {
    printf("  [patterns]\n");
    char label[128];

    for (int pi = 0; pi < SEQ_NUM_PATTERNS; pi++) {
        Pattern *a = &seq.patterns[pi];
        Pattern *b = &SAVED_SEQ_DS(patterns[pi]);

        // Track lengths
        for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
            snprintf(label, sizeof(label), "pat[%d].trackLength[%d]", pi, t);
            ASSERT_EQ_INT(a->trackLength[t], b->trackLength[t], label);
        }
        for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
            snprintf(label, sizeof(label), "pat[%d].trackLength[%d]", pi, SEQ_DRUM_TRACKS + t);
            ASSERT_EQ_INT(a->trackLength[SEQ_DRUM_TRACKS + t], b->trackLength[SEQ_DRUM_TRACKS + t], label);
        }

        // Drum steps
        for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
            for (int s = 0; s < b->trackLength[t]; s++) {
                if (!patGetDrum((Pattern*)b, t, s)) {
                    snprintf(label, sizeof(label), "pat[%d].drumSteps[%d][%d]", pi, t, s);
                    ASSERT_EQ_BOOL(patGetDrum((Pattern*)a, t, s), false, label);
                    continue;
                }
                snprintf(label, sizeof(label), "pat[%d].drumSteps[%d][%d]", pi, t, s);
                ASSERT_EQ_BOOL(patGetDrum((Pattern*)a, t, s), true, label);
                snprintf(label, sizeof(label), "pat[%d].drumVel[%d][%d]", pi, t, s);
                ASSERT_EQ_FLOAT(patGetDrumVel((Pattern*)a, t, s), patGetDrumVel((Pattern*)b, t, s), label);
                snprintf(label, sizeof(label), "pat[%d].drumPitch[%d][%d]", pi, t, s);
                ASSERT_EQ_FLOAT(patGetDrumPitch((Pattern*)a, t, s), patGetDrumPitch((Pattern*)b, t, s), label);
                snprintf(label, sizeof(label), "pat[%d].drumProb[%d][%d]", pi, t, s);
                ASSERT_EQ_FLOAT(probU8ToFloat(a->steps[t][s].probability), probU8ToFloat(b->steps[t][s].probability), label);
                snprintf(label, sizeof(label), "pat[%d].drumCond[%d][%d]", pi, t, s);
                ASSERT_EQ_INT((int)a->steps[t][s].condition, (int)b->steps[t][s].condition, label);
            }
        }

        // Melody notes
        for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
            for (int s = 0; s < b->trackLength[SEQ_DRUM_TRACKS + t]; s++) {
                snprintf(label, sizeof(label), "pat[%d].melodyNote[%d][%d]", pi, t, s);
                ASSERT_EQ_INT(patGetNote((Pattern*)a, SEQ_DRUM_TRACKS + t, s), patGetNote((Pattern*)b, SEQ_DRUM_TRACKS + t, s), label);
                if (patGetNote((Pattern*)b, SEQ_DRUM_TRACKS + t, s) == SEQ_NOTE_OFF) continue;

                snprintf(label, sizeof(label), "pat[%d].melodyVel[%d][%d]", pi, t, s);
                ASSERT_EQ_FLOAT(patGetNoteVel((Pattern*)a, SEQ_DRUM_TRACKS + t, s), patGetNoteVel((Pattern*)b, SEQ_DRUM_TRACKS + t, s), label);
                snprintf(label, sizeof(label), "pat[%d].melodyGate[%d][%d]", pi, t, s);
                ASSERT_EQ_INT(patGetNoteGate((Pattern*)a, SEQ_DRUM_TRACKS + t, s), patGetNoteGate((Pattern*)b, SEQ_DRUM_TRACKS + t, s), label);
                snprintf(label, sizeof(label), "pat[%d].melodySlide[%d][%d]", pi, t, s);
                ASSERT_EQ_BOOL(patGetNoteSlide((Pattern*)a, SEQ_DRUM_TRACKS + t, s), patGetNoteSlide((Pattern*)b, SEQ_DRUM_TRACKS + t, s), label);
                snprintf(label, sizeof(label), "pat[%d].melodyAccent[%d][%d]", pi, t, s);
                ASSERT_EQ_BOOL(patGetNoteAccent((Pattern*)a, SEQ_DRUM_TRACKS + t, s), patGetNoteAccent((Pattern*)b, SEQ_DRUM_TRACKS + t, s), label);
                snprintf(label, sizeof(label), "pat[%d].melodySustain[%d][%d]", pi, t, s);
                ASSERT_EQ_INT(patGetNoteSustain((Pattern*)a, SEQ_DRUM_TRACKS + t, s), patGetNoteSustain((Pattern*)b, SEQ_DRUM_TRACKS + t, s), label);
                snprintf(label, sizeof(label), "pat[%d].melodyProb[%d][%d]", pi, t, s);
                ASSERT_EQ_FLOAT(probU8ToFloat(a->steps[SEQ_DRUM_TRACKS + t][s].probability), probU8ToFloat(b->steps[SEQ_DRUM_TRACKS + t][s].probability), label);
                snprintf(label, sizeof(label), "pat[%d].melodyCond[%d][%d]", pi, t, s);
                ASSERT_EQ_INT((int)a->steps[SEQ_DRUM_TRACKS + t][s].condition, (int)b->steps[SEQ_DRUM_TRACKS + t][s].condition, label);

                // V2 step notes (chord data)
                int absT = SEQ_DRUM_TRACKS + t;
                const StepV2 *sa = &a->steps[absT][s];
                const StepV2 *sb = &b->steps[absT][s];
                snprintf(label, sizeof(label), "pat[%d].steps[%d][%d].noteCount", pi, absT, s);
                ASSERT_EQ_INT(sa->noteCount, sb->noteCount, label);
                for (int v = 0; v < sb->noteCount; v++) {
                    snprintf(label, sizeof(label), "pat[%d].steps[%d][%d].notes[%d].note", pi, absT, s, v);
                    ASSERT_EQ_INT(sa->notes[v].note, sb->notes[v].note, label);
                }
            }
        }

        // Sampler track (track 7)
        {
            int st = SEQ_TRACK_SAMPLER;
            snprintf(label, sizeof(label), "pat[%d].trackLength[%d]", pi, st);
            ASSERT_EQ_INT(a->trackLength[st], b->trackLength[st], label);
            for (int s = 0; s < b->trackLength[st]; s++) {
                const StepV2 *sa2 = &a->steps[st][s];
                const StepV2 *sb2 = &b->steps[st][s];
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].noteCount", pi, s);
                ASSERT_EQ_INT(sa2->noteCount, sb2->noteCount, label);
                if (sb2->noteCount == 0) continue;
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].slice", pi, s);
                ASSERT_EQ_INT(sa2->notes[0].slice, sb2->notes[0].slice, label);
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].note", pi, s);
                ASSERT_EQ_INT(sa2->notes[0].note, sb2->notes[0].note, label);
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].vel", pi, s);
                ASSERT_EQ_INT(sa2->notes[0].velocity, sb2->notes[0].velocity, label);
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].nudge", pi, s);
                ASSERT_EQ_INT((int)sa2->notes[0].nudge, (int)sb2->notes[0].nudge, label);
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].prob", pi, s);
                ASSERT_EQ_INT(sa2->probability, sb2->probability, label);
                snprintf(label, sizeof(label), "pat[%d].sampler[%d].cond", pi, s);
                ASSERT_EQ_INT(sa2->condition, sb2->condition, label);
            }
        }

        // P-locks
        snprintf(label, sizeof(label), "pat[%d].plockCount", pi);
        ASSERT_EQ_INT(a->plockCount, b->plockCount, label);
        for (int j = 0; j < b->plockCount; j++) {
            snprintf(label, sizeof(label), "pat[%d].plock[%d].track", pi, j);
            ASSERT_EQ_INT(a->plocks[j].track, b->plocks[j].track, label);
            snprintf(label, sizeof(label), "pat[%d].plock[%d].step", pi, j);
            ASSERT_EQ_INT(a->plocks[j].step, b->plocks[j].step, label);
            snprintf(label, sizeof(label), "pat[%d].plock[%d].param", pi, j);
            ASSERT_EQ_INT(a->plocks[j].param, b->plocks[j].param, label);
            snprintf(label, sizeof(label), "pat[%d].plock[%d].value", pi, j);
            ASSERT_EQ_FLOAT(a->plocks[j].value, b->plocks[j].value, label);
        }
    }
}

// ============================================================================
// GUARDRAIL TESTS — catch when new fields are added but save/load not updated
// ============================================================================

// 1. Compile-time: sizeof checks. If you add a field to any of these structs,
//    the static assert fires → update save/load in daw_file.h, THEN update
//    the expected size here.
_Static_assert(sizeof(SynthPatch) == 1168,
    "SynthPatch size changed! Update _dwWritePatch/_dwApplyPatchKV in daw_file.h, then update this assert.");
_Static_assert(sizeof(Mixer) == 1960,
    "Mixer size changed! Update dawSave/dawLoad mixer section, then update this assert.");
_Static_assert(sizeof(MasterFX) == 388,
    "MasterFX size changed! Update dawSave/dawLoad masterfx section, then update this assert.");
_Static_assert(sizeof(TapeFX) == 140,
    "TapeFX size changed! Update dawSave/dawLoad tapefx section, then update this assert.");
_Static_assert(sizeof(Sidechain) == 64,
    "Sidechain size changed! Update dawSave/dawLoad sidechain section, then update this assert.");
_Static_assert(sizeof(Crossfader) == 20,
    "Crossfader size changed! Update dawSave/dawLoad crossfader section, then update this assert.");

// 2. Runtime: SynthPatch field-by-field coverage test.
//    Creates a patch with ALL fields set to non-default, non-zero values,
//    saves and loads it, then compares field-by-field. Any unserialized
//    field will be detected as a default/zero value after load.
static void verifyPatchFieldCoverage(void) {
    printf("  [patch field coverage guardrail]\n");
    const char *tmppath = "/tmp/test_daw_guardrail.song";

    // Create a patch where every field is set to a distinctive non-zero value
    SynthPatch orig;
    memset(&orig, 0, sizeof(SynthPatch));
    snprintf(orig.p_name, 32, "GuardTest");
    orig.p_waveType = 3;       // WAVE_TRIANGLE
    orig.p_scwIndex = 7;
    orig.p_attack = 0.123f;    orig.p_decay = 0.456f;
    orig.p_sustain = 0.789f;   orig.p_release = 0.321f;
    orig.p_volume = 0.654f;
    orig.p_pulseWidth = 0.432f; orig.p_pwmRate = 2.34f; orig.p_pwmDepth = 0.567f;
    orig.p_vibratoRate = 5.67f; orig.p_vibratoDepth = 0.123f;
    orig.p_filterCutoff = 0.345f; orig.p_filterResonance = 0.678f;
    orig.p_filterType = 2;
    orig.p_filterEnvAmt = 0.234f; orig.p_filterEnvAttack = 0.012f; orig.p_filterEnvDecay = 0.345f;
    orig.p_filterKeyTrack = 0.567f;
    orig.p_filterLfoRate = 3.45f; orig.p_filterLfoDepth = 0.456f;
    orig.p_filterLfoShape = 2; orig.p_filterLfoSync = 3;
    orig.p_arpEnabled = true; orig.p_arpMode = 2; orig.p_arpRateDiv = 3;
    orig.p_arpRate = 7.89f; orig.p_arpChord = 4;
    orig.p_unisonCount = 3; orig.p_unisonDetune = 15.0f; orig.p_unisonMix = 0.7f;
    orig.p_resoLfoRate = 4.56f; orig.p_resoLfoDepth = 0.234f; orig.p_resoLfoShape = 3;
    orig.p_ampLfoRate = 5.67f; orig.p_ampLfoDepth = 0.345f; orig.p_ampLfoShape = 4;
    orig.p_pitchLfoRate = 6.78f; orig.p_pitchLfoDepth = 0.456f; orig.p_pitchLfoShape = 1;
    orig.p_monoMode = true; orig.p_glideTime = 0.123f;
    orig.p_pluckBrightness = 0.567f; orig.p_pluckDamping = 0.89f; orig.p_pluckDamp = 0.234f;
    orig.p_additivePreset = 3; orig.p_additiveBrightness = 0.678f;
    orig.p_additiveShimmer = 0.345f; orig.p_additiveInharmonicity = 0.123f;
    orig.p_malletPreset = 2; orig.p_malletStiffness = 0.456f; orig.p_malletHardness = 0.567f;
    orig.p_malletStrikePos = 0.234f; orig.p_malletResonance = 0.789f;
    orig.p_malletTremolo = 0.123f; orig.p_malletTremoloRate = 6.78f; orig.p_malletDamp = 0.345f;
    orig.p_voiceVowel = 3; orig.p_voiceFormantShift = 1.23f;
    orig.p_voiceBreathiness = 0.234f; orig.p_voiceBuzziness = 0.567f;
    orig.p_voiceSpeed = 9.87f; orig.p_voicePitch = 1.12f;
    orig.p_voiceConsonant = true; orig.p_voiceConsonantAmt = 0.456f;
    orig.p_voiceNasal = true; orig.p_voiceNasalAmt = 0.345f;
    orig.p_voicePitchEnv = 0.678f; orig.p_voicePitchEnvTime = 0.234f;
    orig.p_voicePitchEnvCurve = 0.567f;
    orig.p_granularScwIndex = 5; orig.p_granularGrainSize = 45.0f;
    orig.p_granularDensity = 20.0f; orig.p_granularPosition = 0.567f;
    orig.p_granularPosRandom = 0.123f; orig.p_granularPitch = 1.23f;
    orig.p_granularPitchRandom = 0.045f; orig.p_granularAmpRandom = 0.089f;
    orig.p_granularSpread = 0.567f; orig.p_granularFreeze = true;
    orig.p_fmModRatio = 2.34f; orig.p_fmModIndex = 1.23f; orig.p_fmFeedback = 0.456f;
    orig.p_pdWaveType = 3; orig.p_pdDistortion = 0.678f;
    orig.p_membranePreset = 2; orig.p_membraneDamping = 0.345f;
    orig.p_membraneStrike = 0.456f; orig.p_membraneBend = 0.234f; orig.p_membraneBendDecay = 0.089f;
    orig.p_birdType = 3; orig.p_birdChirpRange = 0.789f;
    orig.p_birdTrillRate = 0.345f; orig.p_birdTrillDepth = 0.234f;
    orig.p_birdAmRate = 0.567f; orig.p_birdAmDepth = 0.456f; orig.p_birdHarmonics = 0.345f;
    orig.p_pitchEnvAmount = 18.0f; orig.p_pitchEnvDecay = 0.067f;
    orig.p_pitchEnvCurve = -0.45f; orig.p_pitchEnvLinear = true;
    orig.p_noiseMix = 0.345f; orig.p_noiseTone = 0.567f;
    orig.p_noiseHP = 0.234f; orig.p_noiseDecay = 0.089f;
    orig.p_retriggerCount = 3; orig.p_retriggerSpread = 0.023f;
    orig.p_retriggerOverlap = true; orig.p_retriggerBurstDecay = 0.034f;
    orig.p_osc2Ratio = 1.67f; orig.p_osc2Level = 0.456f;
    orig.p_osc3Ratio = 2.34f; orig.p_osc3Level = 0.345f;
    orig.p_osc4Ratio = 3.12f; orig.p_osc4Level = 0.234f;
    orig.p_osc5Ratio = 4.56f; orig.p_osc5Level = 0.123f;
    orig.p_osc6Ratio = 5.78f; orig.p_osc6Level = 0.089f;
    orig.p_drive = 0.345f;
    orig.p_clickLevel = 0.456f; orig.p_clickTime = 0.0078f;
    orig.p_noiseMode = 2; orig.p_oscMixMode = 1;
    orig.p_retriggerCurve = 0.345f; orig.p_phaseReset = true;
    orig.p_noiseLPBypass = true; orig.p_noiseType = 1;
    orig.p_useTriggerFreq = true; orig.p_triggerFreq = 123.45f; orig.p_choke = true;
    orig.p_expRelease = true; orig.p_expDecay = true; orig.p_oneShot = true;

    // Put it into daw state patch 0, save
    daw.patches[0] = orig;
    dawSave(tmppath);

    // Reset and load
    memset(&daw, 0, sizeof(DawState));
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    memset(&_seqCtx, 0, sizeof(SequencerContext));
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) initPattern(&seq.patterns[i]);
    dawLoad(tmppath);

    SynthPatch *loaded = &daw.patches[0];

    // Compare every field. This is the guardrail: if you add a field to SynthPatch
    // and set it in orig above but forget to add it to save/load, this will catch it.
    #define CK_F(field) ASSERT_EQ_FLOAT(loaded->field, orig.field, "guardrail: " #field)
    #define CK_I(field) ASSERT_EQ_INT(loaded->field, orig.field, "guardrail: " #field)
    #define CK_B(field) ASSERT_EQ_BOOL(loaded->field, orig.field, "guardrail: " #field)

    CK_I(p_waveType); CK_I(p_scwIndex);
    CK_F(p_attack); CK_F(p_decay); CK_F(p_sustain); CK_F(p_release); CK_F(p_volume);
    CK_F(p_pulseWidth); CK_F(p_pwmRate); CK_F(p_pwmDepth);
    CK_F(p_vibratoRate); CK_F(p_vibratoDepth);
    CK_F(p_filterCutoff); CK_F(p_filterResonance); CK_I(p_filterType);
    CK_F(p_filterEnvAmt); CK_F(p_filterEnvAttack); CK_F(p_filterEnvDecay);
    CK_F(p_filterKeyTrack);
    CK_F(p_filterLfoRate); CK_F(p_filterLfoDepth); CK_I(p_filterLfoShape); CK_I(p_filterLfoSync);
    CK_B(p_arpEnabled); CK_I(p_arpMode); CK_I(p_arpRateDiv); CK_F(p_arpRate); CK_I(p_arpChord);
    CK_I(p_unisonCount); CK_F(p_unisonDetune); CK_F(p_unisonMix);
    CK_F(p_resoLfoRate); CK_F(p_resoLfoDepth); CK_I(p_resoLfoShape);
    CK_F(p_ampLfoRate); CK_F(p_ampLfoDepth); CK_I(p_ampLfoShape);
    CK_F(p_pitchLfoRate); CK_F(p_pitchLfoDepth); CK_I(p_pitchLfoShape);
    CK_B(p_monoMode); CK_F(p_glideTime);
    CK_F(p_pluckBrightness); CK_F(p_pluckDamping); CK_F(p_pluckDamp);
    CK_I(p_additivePreset); CK_F(p_additiveBrightness); CK_F(p_additiveShimmer); CK_F(p_additiveInharmonicity);
    CK_I(p_malletPreset); CK_F(p_malletStiffness); CK_F(p_malletHardness); CK_F(p_malletStrikePos);
    CK_F(p_malletResonance); CK_F(p_malletTremolo); CK_F(p_malletTremoloRate); CK_F(p_malletDamp);
    CK_I(p_voiceVowel); CK_F(p_voiceFormantShift); CK_F(p_voiceBreathiness); CK_F(p_voiceBuzziness);
    CK_F(p_voiceSpeed); CK_F(p_voicePitch);
    CK_B(p_voiceConsonant); CK_F(p_voiceConsonantAmt); CK_B(p_voiceNasal); CK_F(p_voiceNasalAmt);
    CK_F(p_voicePitchEnv); CK_F(p_voicePitchEnvTime); CK_F(p_voicePitchEnvCurve);
    CK_I(p_granularScwIndex); CK_F(p_granularGrainSize); CK_F(p_granularDensity);
    CK_F(p_granularPosition); CK_F(p_granularPosRandom); CK_F(p_granularPitch);
    CK_F(p_granularPitchRandom); CK_F(p_granularAmpRandom); CK_F(p_granularSpread);
    CK_B(p_granularFreeze);
    CK_F(p_fmModRatio); CK_F(p_fmModIndex); CK_F(p_fmFeedback);
    CK_I(p_pdWaveType); CK_F(p_pdDistortion);
    CK_I(p_membranePreset); CK_F(p_membraneDamping); CK_F(p_membraneStrike);
    CK_F(p_membraneBend); CK_F(p_membraneBendDecay);
    CK_I(p_birdType); CK_F(p_birdChirpRange); CK_F(p_birdTrillRate); CK_F(p_birdTrillDepth);
    CK_F(p_birdAmRate); CK_F(p_birdAmDepth); CK_F(p_birdHarmonics);
    CK_F(p_pitchEnvAmount); CK_F(p_pitchEnvDecay); CK_F(p_pitchEnvCurve); CK_B(p_pitchEnvLinear);
    CK_F(p_noiseMix); CK_F(p_noiseTone); CK_F(p_noiseHP); CK_F(p_noiseDecay);
    CK_I(p_retriggerCount); CK_F(p_retriggerSpread); CK_B(p_retriggerOverlap); CK_F(p_retriggerBurstDecay);
    CK_F(p_osc2Ratio); CK_F(p_osc2Level); CK_F(p_osc3Ratio); CK_F(p_osc3Level);
    CK_F(p_osc4Ratio); CK_F(p_osc4Level); CK_F(p_osc5Ratio); CK_F(p_osc5Level);
    CK_F(p_osc6Ratio); CK_F(p_osc6Level);
    CK_F(p_drive);
    CK_F(p_clickLevel); CK_F(p_clickTime);
    CK_I(p_noiseMode); CK_I(p_oscMixMode);
    CK_F(p_retriggerCurve); CK_B(p_phaseReset); CK_B(p_noiseLPBypass); CK_I(p_noiseType);
    CK_B(p_useTriggerFreq); CK_F(p_triggerFreq); CK_B(p_choke);
    ASSERT_EQ_STR(loaded->p_name, "GuardTest", "guardrail: p_name");
    CK_B(p_expRelease); CK_B(p_expDecay); CK_B(p_oneShot);

    #undef CK_F
    #undef CK_I
    #undef CK_B

    remove(tmppath);
}

// ============================================================================
// SAMPLE SECTION ROUND-TRIP
// ============================================================================

static void verifySampleSectionRoundTrip(void) {
    printf("  [sample section round-trip]\n");
    const char *tmppath = "/tmp/test_daw_sample_rt.song";

    // 1. Set up chopState with non-default values
    chopStateClearTest();
    strncpy(chopState.sourcePath, "songs/testbeat.song", 255);
    chopState.sourceLoops = 3;
    chopState.selStart = 0.1f;
    chopState.selEnd = 0.6f;
    chopState.hasSelection = true;
    chopState.sliceCount = 16;
    chopState.chopMode = 1;  // transient
    chopState.sensitivity = 0.75f;

    // Load scratch with test data + markers (required for save to write [sample])
    scratchFree(&scratch);
    float *testBuf = (float *)malloc(10000 * sizeof(float));
    for (int i = 0; i < 10000; i++) testBuf[i] = 0.1f;
    scratchLoad(&scratch, testBuf, 10000, 44100, 120.0f);
    scratchAddMarker(&scratch, 2000);
    scratchAddMarker(&scratch, 5000);
    scratchAddMarker(&scratch, 8000);

    // Pad mappings
    daw.chopSliceMap[0] = 3;
    daw.chopSliceMap[1] = 7;
    daw.chopSliceMap[2] = -1;
    daw.chopSliceMap[3] = 12;

    // 2. Save
    memset(&daw.transport, 0, sizeof(Transport));
    daw.transport.bpm = 120.0f;
    bool ok = dawSave(tmppath);
    if (!ok) { printf("  FAIL: dawSave failed for sample section test\n"); tests_failed++; return; }
    tests_passed++;

    // 3. Save expected values
    char savedPath[256];
    strncpy(savedPath, chopState.sourcePath, 255);
    int savedLoops = chopState.sourceLoops;
    float savedSelStart = chopState.selStart;
    float savedSelEnd = chopState.selEnd;
    int savedSliceCount = chopState.sliceCount;
    int savedChopMode = chopState.chopMode;
    float savedSensitivity = chopState.sensitivity;
    int savedPadMap[4] = { daw.chopSliceMap[0], daw.chopSliceMap[1], daw.chopSliceMap[2], daw.chopSliceMap[3] };
    int savedMarkerCount = scratch.markerCount;
    int savedMarkers[3] = { scratch.markers[0], scratch.markers[1], scratch.markers[2] };

    // 4. Clear everything
    chopStateClearTest();
    scratchFree(&scratch);
    memset(&daw, 0, sizeof(DawState));
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    memset(&_seqCtx, 0, sizeof(SequencerContext));
    for (int t = 0; t < 4; t++) daw.chopSliceMap[t] = -1;

    // 5. Load
    ok = dawLoad(tmppath);
    if (!ok) { printf("  FAIL: dawLoad failed for sample section test\n"); tests_failed++; remove(tmppath); return; }
    tests_passed++;

    // 6. Verify parsed values
    ASSERT_EQ_STR(chopState.sourcePath, savedPath, "sample.sourceFile");
    ASSERT_EQ_INT(chopState.sourceLoops, savedLoops, "sample.sourceLoops");
    ASSERT_EQ_FLOAT(chopState.selStart, savedSelStart, "sample.selStart");
    ASSERT_EQ_FLOAT(chopState.selEnd, savedSelEnd, "sample.selEnd");
    ASSERT_EQ_INT(chopState.sliceCount, savedSliceCount, "sample.sliceCount");
    ASSERT_EQ_INT(chopState.chopMode, savedChopMode, "sample.chopMode");
    ASSERT_EQ_FLOAT(chopState.sensitivity, savedSensitivity, "sample.sensitivity");

    // Pad mappings
    ASSERT_EQ_INT(daw.chopSliceMap[0], savedPadMap[0], "sample.padMap0");
    ASSERT_EQ_INT(daw.chopSliceMap[1], savedPadMap[1], "sample.padMap1");
    ASSERT_EQ_INT(daw.chopSliceMap[2], savedPadMap[2], "sample.padMap2");
    ASSERT_EQ_INT(daw.chopSliceMap[3], savedPadMap[3], "sample.padMap3");

    // Scratch markers round-trip
    ASSERT_EQ_INT(scratch.markerCount, savedMarkerCount, "scratch.markerCount");
    ASSERT_EQ_INT(scratch.markers[0], savedMarkers[0], "scratch.markers[0]");
    ASSERT_EQ_INT(scratch.markers[1], savedMarkers[1], "scratch.markers[1]");
    ASSERT_EQ_INT(scratch.markers[2], savedMarkers[2], "scratch.markers[2]");

    remove(tmppath);
}

static void verifySampleSectionEmpty(void) {
    printf("  [sample section empty — no [sample] when scratch empty]\n");
    const char *tmppath = "/tmp/test_daw_sample_empty.song";

    // No scratch data → no [sample] section should be written
    chopStateClearTest();
    scratchFree(&scratch);

    memset(&daw, 0, sizeof(DawState));
    daw.transport.bpm = 120.0f;
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);

    bool ok = dawSave(tmppath);
    if (!ok) { printf("  FAIL: dawSave failed\n"); tests_failed++; return; }
    tests_passed++;

    // Verify file doesn't contain [sample]
    FILE *f = fopen(tmppath, "r");
    if (!f) { printf("  FAIL: can't open saved file\n"); tests_failed++; return; }
    bool foundSample = false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "[sample]", 8) == 0) { foundSample = true; break; }
    }
    fclose(f);

    if (foundSample) {
        printf("  FAIL: [sample] section written when not bounced\n");
        tests_failed++;
    } else {
        tests_passed++;
    }

    remove(tmppath);
}

// ============================================================================
// BOUNCE GUARD TESTS — prevent recursive bounce inside dawLoad
// ============================================================================

static void verifyBounceGuardPreventsRebounce(void) {
    printf("  [bounce guard: _chopBounceActive suppresses re-bounce]\n");
    const char *tmppath = "/tmp/test_daw_bounce_guard.song";

    // 1. Set up a song with a [sample] section
    chopStateClearTest();
    strncpy(chopState.sourcePath, "songs/testbeat.song", 255);
    chopState.sourceLoops = 1;
    chopState.sliceCount = 8;
    // Load scratch data to trigger [sample] section in save
    scratchFree(&scratch);
    float *guardBuf = (float *)malloc(1000 * sizeof(float));
    for (int i = 0; i < 1000; i++) guardBuf[i] = 0.1f;
    scratchLoad(&scratch, guardBuf, 1000, 44100, 120.0f);

    memset(&daw, 0, sizeof(DawState));
    daw.transport.bpm = 120.0f;
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);

    bool ok = dawSave(tmppath);
    if (!ok) { printf("  FAIL: dawSave failed\n"); tests_failed++; return; }

    // 2. Load with _chopBounceActive = true (simulates being inside renderPatternToBuffer)
    chopStateClearTest();
    memset(&daw, 0, sizeof(DawState));
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    memset(&_seqCtx, 0, sizeof(SequencerContext));

    _testRebounceCallCount = 0;
    _chopBounceActive = true;  // simulate being inside a bounce

    ok = dawLoad(tmppath);
    if (!ok) { printf("  FAIL: dawLoad failed\n"); tests_failed++; _chopBounceActive = false; remove(tmppath); return; }

    _chopBounceActive = false;

    // 3. Verify: scratchFromSelection should NOT have been called
    if (_testRebounceCallCount != 0) {
        printf("  FAIL: re-bounce triggered %d times despite _chopBounceActive=true!\n",
               _testRebounceCallCount);
        tests_failed++;
    } else {
        tests_passed++;
    }

    // 4. Now load WITHOUT the guard — should trigger re-bounce
    chopStateClearTest();
    memset(&daw, 0, sizeof(DawState));
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    memset(&_seqCtx, 0, sizeof(SequencerContext));

    _testRebounceCallCount = 0;
    _chopBounceActive = false;

    ok = dawLoad(tmppath);
    if (!ok) { printf("  FAIL: dawLoad failed (second load)\n"); tests_failed++; remove(tmppath); return; }

    if (_testRebounceCallCount != 1) {
        printf("  FAIL: expected 1 re-bounce call, got %d\n", _testRebounceCallCount);
        tests_failed++;
    } else {
        tests_passed++;
    }

    remove(tmppath);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    const char *tmpfile = "/tmp/test_daw_roundtrip.song";

    printf("DAW File Round-Trip Test\n");
    printf("========================\n\n");

    // 1. Set up test state with non-default values
    printf("Setting up test state...\n");
    setupTestState();

    // 2. Save to file
    printf("Saving to %s...\n", tmpfile);
    bool ok = dawSave(tmpfile);
    if (!ok) { printf("FATAL: dawSave() failed!\n"); return 1; }

    // 3. Reset all state to defaults
    printf("Resetting state to defaults...\n");
    memset(&daw, 0, sizeof(DawState));
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    memset(&_seqCtx, 0, sizeof(SequencerContext));
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) initPattern(&seq.patterns[i]);

    // 4. Load from file
    printf("Loading from %s...\n\n", tmpfile);
    ok = dawLoad(tmpfile);
    if (!ok) { printf("FATAL: dawLoad() failed!\n"); return 1; }

    // 5. Verify every section
    printf("Verifying round-trip...\n");
    verifySongHeader();
    verifyScale();
    verifyGroove();
    verifySettings();
    verifyAllPatches();
    verifyMixer();
    verifySidechain();
    verifyMasterFx();
    verifyTapeFx();
    verifyCrossfader();
    verifySplit();
    verifyArrangement();
    verifyPatterns();

    // 6. Sample section round-trip
    verifySampleSectionRoundTrip();
    verifySampleSectionEmpty();

    // 7. Bounce guard (prevents recursive bounce inside dawLoad)
    verifyBounceGuardPreventsRebounce();

    // 7. Guardrail: field coverage
    printf("  [guardrail tests]\n");

    // Re-setup, save, reset, load for guardrail test
    setupTestState();
    verifyPatchFieldCoverage();

    // 7. Report
    printf("\n========================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    if (tests_failed == 0) {
        printf("ALL TESTS PASSED\n");
    } else {
        printf("SOME TESTS FAILED\n");
    }

    // Cleanup
    remove(tmpfile);

    return tests_failed > 0 ? 1 : 0;
}
