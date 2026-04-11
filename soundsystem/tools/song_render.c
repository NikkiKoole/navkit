// song_render.c — Headless .song file renderer (DAW format → WAV)
//
// Loads a DAW .song file, ticks the sequencer, renders audio, writes WAV.
// No GUI dependency (no raylib).
//
// Build: make song-render
// Usage: ./build/bin/song-render <file.song> [-d <seconds>] [-o <output.wav>]
//        ./build/bin/song-render <file.song> --info
//
// Examples:
//   ./build/bin/song-render soundsystem/demo/songs/scratch.song
//   ./build/bin/song-render scratch.song -d 60 -o myrender.wav
//   ./build/bin/song-render scratch.song --info

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Include synth engine (no raylib dependency)
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/effects.h"
#include "../engines/sequencer.h"
// Undefine engine macros that conflict with our DAW state (same as daw.c)
#undef masterVolume
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode
#include "../engines/sampler.h"
#include "../engines/scratch_space.h"
#include "../engines/instrument_presets.h"

// WAV writer
#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"

#define SAMPLE_RATE 44100

// DAW state types (shared with daw.c)
#include "../engines/daw_state.h"

static Pattern* dawPattern(void) {
    return &seq.patterns[seq.currentPattern];
}

static DawState daw = {
    .transport = { .bpm = 120.0f },
    .crossfader = { .pos = 0.5f, .sceneB = 1, .count = 8 },
    .stepCount = 16,
    .masterSpeed = 1.0f,
    .song = { .loopsPerPattern = 2 },
    .mixer = {
        .volume = {0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f},
        .filterCut = {1,1,1,1,1,1,1},
        .distDrive = {2,2,2,2,2,2,2},
        .distMix = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .delayTime = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
        .delayFB = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
        .delayMix = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
    },
    .sidechain = { .depth = 0.8f, .attack = 0.005f, .release = 0.15f,
                    .envDepth = 0.8f, .envAttack = 0.005f, .envHold = 0.02f,
                    .envRelease = 0.15f, .envCurve = 1 },
    .masterFx = {
        .distDrive = 2.0f, .distTone = 0.7f, .distMix = 0.5f,
        .crushBits = 8.0f, .crushRate = 4.0f, .crushMix = 0.5f,
        .chorusRate = 1.0f, .chorusDepth = 0.3f, .chorusMix = 0.3f,
        .flangerRate = 0.5f, .flangerDepth = 0.5f, .flangerFeedback = 0.3f, .flangerMix = 0.3f,
        .tapeSaturation = 0.3f, .tapeWow = 0.1f, .tapeFlutter = 0.1f, .tapeHiss = 0.05f,
        .delayTime = 0.3f, .delayFeedback = 0.4f, .delayTone = 0.5f, .delayMix = 0.3f,
        .reverbSize = 0.5f, .reverbDamping = 0.5f, .reverbPreDelay = 0.02f, .reverbMix = 0.3f, .reverbBass = 1.0f,
        .eqLowGain = 0.0f, .eqHighGain = 0.0f, .eqLowFreq = 80.0f, .eqHighFreq = 6000.0f,
        .compThreshold = -12.0f, .compRatio = 4.0f, .compAttack = 0.01f, .compRelease = 0.1f, .compMakeup = 0.0f,
    },
    .tapeFx = {
        .headTime = 0.5f, .feedback = 0.6f, .mix = 0.5f,
        .saturation = 0.3f, .toneHigh = 0.7f, .noise = 0.05f, .degradeRate = 0.1f,
        .wow = 0.1f, .flutter = 0.1f, .drift = 0.05f, .speedTarget = 1.0f,
        .rewindTime = 1.5f, .rewindMinSpeed = 0.2f, .rewindVinyl = 0.3f, .rewindWobble = 0.2f, .rewindFilter = 0.5f,
    },
    .masterVol = 0.8f,
    .splitPoint = 60, .splitLeftPatch = 1, .splitRightPatch = 2,
};

// Preset tracking stub (required by daw_file.h)
static int patchPresetIndex[NUM_PATCHES] = {-1,-1,-1,-1,-1,-1,-1,-1};

// Include DAW file loader (uses daw.* and seq.* globals)
#include "../demo/daw_file.h"

// ============================================================================
// VOICE ROUTING (simplified from daw.c)
// ============================================================================

static int voiceBus[NUM_VOICES];
static int dawDrumVoice[SEQ_DRUM_TRACKS] = {-1, -1, -1, -1};
static int dawMelodyVoice[SEQ_MELODY_TRACKS][SEQ_V2_MAX_POLY];
static int dawMelodyVoiceCount[SEQ_MELODY_TRACKS] = {0, 0, 0};
static int dawMonoVoiceIdx[SEQ_MELODY_TRACKS] = {-1, -1, -1};

static int dawPatchToBus(int patchIdx) {
    if (patchIdx >= 0 && patchIdx <= 3) return BUS_DRUM0 + patchIdx;
    if (patchIdx == 4) return BUS_BASS;
    if (patchIdx == 5) return BUS_LEAD;
    return BUS_CHORD;
}

// ============================================================================
// SEQUENCER CALLBACKS (matches daw.c trigger logic)
// ============================================================================

static void renderDrumTrigger(int trackIdx, int busIdx, float vel, float pitchMod) {
    SynthPatch *p = &daw.patches[trackIdx];
    float pVol = plockValue(PLOCK_VOLUME, vel);

    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    float trigFreq = p->p_triggerFreq * pitchMod;
    if (pitchOffset != 0.0f) trigFreq *= powf(2.0f, pitchOffset / 12.0f);

    float origDecay = p->p_decay;
    float origCutoff = p->p_filterCutoff;
    float pDecay = plockValue(PLOCK_DECAY, -1.0f);
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    if (pDecay >= 0.0f) p->p_decay = pDecay;
    if (pTone >= 0.0f) p->p_filterCutoff = pTone;

    // Choke previous voice
    if (p->p_choke && dawDrumVoice[trackIdx] >= 0 && voiceBus[dawDrumVoice[trackIdx]] == busIdx) {
        synthCtx->voices[dawDrumVoice[trackIdx]].envStage = 0;
        synthCtx->voices[dawDrumVoice[trackIdx]].envLevel = 0.0f;
    }
    // Force one-shot for drum tracks — drums never sustain, prevents voice hang
    bool origOneShot = p->p_oneShot;
    p->p_oneShot = true;
    int v = playNoteWithPatch(trigFreq, p);
    p->p_oneShot = origOneShot;
    dawDrumVoice[trackIdx] = v;
    if (v >= 0) {
        voiceBus[v] = busIdx;
        synthCtx->voices[v].volume *= pVol;
    }

    p->p_decay = origDecay;
    p->p_filterCutoff = origCutoff;
}

static void renderDrumTrigger0(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; renderDrumTrigger(0, BUS_DRUM0, vel, pitchMod); }
static void renderDrumTrigger1(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; renderDrumTrigger(1, BUS_DRUM1, vel, pitchMod); }
static void renderDrumTrigger2(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; renderDrumTrigger(2, BUS_DRUM2, vel, pitchMod); }
static void renderDrumTrigger3(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; renderDrumTrigger(3, BUS_DRUM3, vel, pitchMod); }

static void renderMelodyTrigger(int trackIdx, int note, float vel,
                                 float gateTime, bool slide, bool accent) {
    (void)gateTime;
    int busTrack = trackIdx + SEQ_DRUM_TRACKS;
    SynthPatch *p = &daw.patches[busTrack];

    float pVol = plockValue(PLOCK_VOLUME, vel);
    float accentFilterBoost = accent ? 0.3f : 0.0f;
    if (accent) pVol = fminf(pVol * 1.3f, 1.0f);

    float freq = patchMidiToFreq(note);
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    if (pitchOffset != 0.0f) freq *= powf(2.0f, pitchOffset / 12.0f);

    float pTone = plockValue(PLOCK_TONE, -1.0f);
    float pCutoff = (pTone >= 0.0f) ? pTone : plockValue(PLOCK_FILTER_CUTOFF, p->p_filterCutoff);
    float pReso = plockValue(PLOCK_FILTER_RESO, p->p_filterResonance);
    float pFilterEnv = plockValue(PLOCK_FILTER_ENV, p->p_filterEnvAmt) + accentFilterBoost;
    float pDecay = plockValue(PLOCK_DECAY, p->p_decay);

    // Slide: glide existing voice
    int vc = dawMelodyVoiceCount[trackIdx];
    int lastVoice = (vc > 0) ? dawMelodyVoice[trackIdx][vc - 1] : -1;
    if (slide && lastVoice >= 0 &&
        voiceBus[lastVoice] == dawPatchToBus(busTrack) &&
        synthCtx->voices[lastVoice].envStage > 0) {
        Voice *sv = &synthCtx->voices[lastVoice];
        sv->targetFrequency = freq;
        // 303-style glide: scale time with pitch interval for musical feel
        float semitoneDistance = 12.0f * fabsf(logf(freq / sv->baseFrequency) / logf(2.0f));
        float slideTime = 0.06f + semitoneDistance * 0.003f; // 60ms base + 3ms per semitone
        if (slideTime > 0.2f) slideTime = 0.2f;
        sv->glideRate = 1.0f / slideTime;
        sv->volume = pVol * p->p_volume;
        sv->filterCutoff = pCutoff;
        sv->filterResonance = pReso;
        sv->filterEnvAmt = pFilterEnv;
        sv->decay = pDecay;
        // 303: filter envelope always retriggers on each step (accent just boosts amount)
        sv->filterEnvLevel = 1.0f;
        sv->filterEnvStage = 2;
        sv->filterEnvPhase = 0.0f;
        return;
    }

    // New note
    float origCutoff = p->p_filterCutoff, origReso = p->p_filterResonance;
    float origFilterEnvAmt = p->p_filterEnvAmt, origDecay = p->p_decay;
    float origVolume = p->p_volume;

    p->p_filterCutoff = pCutoff;
    p->p_filterResonance = pReso;
    p->p_filterEnvAmt = pFilterEnv;
    p->p_decay = pDecay;
    p->p_volume = pVol * origVolume;

    int savedMonoVoiceIdx = monoVoiceIdx;
    if (p->p_monoMode && dawMonoVoiceIdx[trackIdx] >= 0) {
        monoVoiceIdx = dawMonoVoiceIdx[trackIdx];
    }

    int vi = playNoteWithPatch(freq, p);
    if (vi >= 0) {
        voiceBus[vi] = dawPatchToBus(busTrack);
        if (p->p_monoMode) dawMonoVoiceIdx[trackIdx] = vi;
        int trigSlot = seq.trackNoteOnSlot[busTrack];
        if (trigSlot >= 0 && trigSlot < SEQ_V2_MAX_POLY) {
            dawMelodyVoice[trackIdx][trigSlot] = vi;
        }
        int _vc2 = 0;
        for (int _si = 0; _si < SEQ_V2_MAX_POLY; _si++) if (dawMelodyVoice[trackIdx][_si] >= 0) _vc2 = _si + 1;
        dawMelodyVoiceCount[trackIdx] = _vc2;
    }

    if (p->p_monoMode) monoVoiceIdx = savedMonoVoiceIdx;

    p->p_filterCutoff = origCutoff;
    p->p_filterResonance = origReso;
    p->p_filterEnvAmt = origFilterEnvAmt;
    p->p_decay = origDecay;
    p->p_volume = origVolume;
}

static void renderMelodyTrigger0(int n, float v, float g, float pm, bool s, bool a) { (void)pm; renderMelodyTrigger(0, n, v, g, s, a); }
static void renderMelodyTrigger1(int n, float v, float g, float pm, bool s, bool a) { (void)pm; renderMelodyTrigger(1, n, v, g, s, a); }
static void renderMelodyTrigger2(int n, float v, float g, float pm, bool s, bool a) { (void)pm; renderMelodyTrigger(2, n, v, g, s, a); }

static void renderMelodyRelease(int t) {
    int busTrack = t + SEQ_DRUM_TRACKS;
    int slot = seq.trackNoteOffSlot[busTrack];
    int vi = (slot >= 0 && slot < SEQ_V2_MAX_POLY) ? dawMelodyVoice[t][slot] : -1;

    int fallbackNote = seq.trackMonoFallbackNote[busTrack][0];
    int fallbackGate = seq.trackMonoFallbackGate[busTrack][0];
    if (vi >= 0 && fallbackNote != SEQ_NOTE_OFF && fallbackGate > 0) {
        // Pop [0], shift [1] → [0]
        seq.trackMonoFallbackNote[busTrack][0] = seq.trackMonoFallbackNote[busTrack][1];
        seq.trackMonoFallbackGate[busTrack][0] = seq.trackMonoFallbackGate[busTrack][1];
        seq.trackMonoFallbackNote[busTrack][1] = SEQ_NOTE_OFF;
        seq.trackMonoFallbackGate[busTrack][1] = 0;
        // Glide the still-active voice to the fallback pitch.
        float fallbackFreq = patchMidiToFreq(fallbackNote);
        SynthPatch *fp = &daw.patches[busTrack];
        float glideT = fp->p_glideTime > 0.0f ? fp->p_glideTime : 0.03f;
        Voice *rv = &synthCtx->voices[vi];
        // Retrigger envelope only if the voice has fully decayed (plucky/percussive patches).
        // Sustaining patches glide smoothly without retriggering. Matches synth arp logic.
        if (rv->envStage == 0 || rv->envStage == 4 || rv->sustain < 0.001f || fp->p_monoHardRetrigger) {
            if (rv->envLevel > 0.01f) { rv->antiClickSamples = 44; rv->antiClickSample = rv->prevOutput; }
            rv->envPhase = 0.0f;
            rv->envLevel = 0.0f;
            rv->envStage = 1;
        }
        rv->targetFrequency = fallbackFreq;
        rv->glideRate = 1.0f / glideT;
        seq.trackCurrentNote[busTrack][slot] = fallbackNote;
        seq.trackGateRemaining[busTrack][slot] = fallbackGate;
        seq.trackNoteOffVetoed[busTrack] = true;
        int _vc2 = 0;
        for (int _si = 0; _si < SEQ_V2_MAX_POLY; _si++) if (dawMelodyVoice[t][_si] >= 0) _vc2 = _si + 1;
        dawMelodyVoiceCount[t] = _vc2;
        return;
    }

    seq.trackMonoFallbackNote[busTrack][0] = SEQ_NOTE_OFF;
    seq.trackMonoFallbackGate[busTrack][0] = 0;
    seq.trackMonoFallbackNote[busTrack][1] = SEQ_NOTE_OFF;
    seq.trackMonoFallbackGate[busTrack][1] = 0;
    if (vi >= 0) {
        releaseNote(vi);
        dawMelodyVoice[t][slot] = -1;
    }
    int _vc = 0;
    for (int _si = 0; _si < SEQ_V2_MAX_POLY; _si++) if (dawMelodyVoice[t][_si] >= 0) _vc = _si + 1;
    dawMelodyVoiceCount[t] = _vc;
}
static void renderMelodyRelease0(void) { renderMelodyRelease(0); }
static void renderMelodyRelease1(void) { renderMelodyRelease(1); }
static void renderMelodyRelease2(void) { renderMelodyRelease(2); }

// ============================================================================
// SYNC DAW STATE → ENGINE
// ============================================================================

#include "../engines/daw_sync.h"

static void renderSyncState(void) {
    dawSyncEngineStateFromEx(&daw, dawPattern());
}

#if 0 // replaced by daw_sync.h — kept as tombstone for git blame
static void renderSyncState_OLD(void) {
    synthCtx->scaleLockEnabled = daw.scaleLockEnabled;
    synthCtx->scaleRoot = daw.scaleRoot;
    synthCtx->scaleType = daw.scaleType;

    // Pattern overrides
    Pattern *curPat = dawPattern();
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_SCALE) {
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = ovr->ovrScaleRoot;
        synthCtx->scaleType = ovr->ovrScaleType;
    }

    // Master FX → effects engine
    fx.distEnabled     = daw.masterFx.distOn;
    fx.distDrive       = daw.masterFx.distDrive;
    fx.distTone        = daw.masterFx.distTone;
    fx.distMix         = daw.masterFx.distMix;
    fx.crushEnabled    = daw.masterFx.crushOn;
    fx.crushBits       = daw.masterFx.crushBits;
    fx.crushRate       = daw.masterFx.crushRate;
    fx.crushMix        = daw.masterFx.crushMix;
    fx.chorusEnabled   = daw.masterFx.chorusOn;
    fx.chorusRate      = daw.masterFx.chorusRate;
    fx.chorusDepth     = daw.masterFx.chorusDepth;
    fx.chorusMix       = daw.masterFx.chorusMix;
    fx.flangerEnabled  = daw.masterFx.flangerOn;
    fx.flangerRate     = daw.masterFx.flangerRate;
    fx.flangerDepth    = daw.masterFx.flangerDepth;
    fx.flangerFeedback = daw.masterFx.flangerFeedback;
    fx.flangerMix      = daw.masterFx.flangerMix;
    fx.phaserEnabled   = daw.masterFx.phaserOn;
    fx.phaserRate      = daw.masterFx.phaserRate;
    fx.phaserDepth     = daw.masterFx.phaserDepth;
    fx.phaserMix       = daw.masterFx.phaserMix;
    fx.phaserFeedback  = daw.masterFx.phaserFeedback;
    fx.phaserStages    = daw.masterFx.phaserStages;
    fx.combEnabled     = daw.masterFx.combOn;
    fx.combFreq        = daw.masterFx.combFreq;
    fx.combFeedback    = daw.masterFx.combFeedback;
    fx.combMix         = daw.masterFx.combMix;
    fx.combDamping     = daw.masterFx.combDamping;
    fx.tapeEnabled     = daw.masterFx.tapeOn;
    fx.tapeSaturation  = daw.masterFx.tapeSaturation;
    fx.tapeWow         = daw.masterFx.tapeWow;
    fx.tapeFlutter     = daw.masterFx.tapeFlutter;
    fx.tapeHiss        = daw.masterFx.tapeHiss;
    fx.delayEnabled    = daw.masterFx.delayOn;
    fx.delayTime       = daw.masterFx.delayTime;
    fx.delayFeedback   = daw.masterFx.delayFeedback;
    fx.delayTone       = daw.masterFx.delayTone;
    fx.delayMix        = daw.masterFx.delayMix;
    fx.reverbEnabled   = daw.masterFx.reverbOn;
    fx.reverbSize      = daw.masterFx.reverbSize;
    fx.reverbDamping   = daw.masterFx.reverbDamping;
    fx.reverbPreDelay  = daw.masterFx.reverbPreDelay;
    fx.reverbMix       = daw.masterFx.reverbMix;
    fx.eqEnabled       = daw.masterFx.eqOn;
    fx.eqLowGain       = daw.masterFx.eqLowGain;
    fx.eqHighGain      = daw.masterFx.eqHighGain;
    fx.eqLowFreq       = daw.masterFx.eqLowFreq;
    fx.eqHighFreq      = daw.masterFx.eqHighFreq;
    fx.compEnabled     = daw.masterFx.compOn;
    fx.compThreshold   = daw.masterFx.compThreshold;
    fx.compRatio       = daw.masterFx.compRatio;
    fx.compAttack      = daw.masterFx.compAttack;
    fx.compRelease     = daw.masterFx.compRelease;
    fx.compMakeup      = daw.masterFx.compMakeup;
    fx.vinylEnabled    = daw.masterFx.vinylOn;
    fx.vinylCrackle    = daw.masterFx.vinylCrackle;
    fx.vinylNoise      = daw.masterFx.vinylNoise;
    fx.vinylWarp       = daw.masterFx.vinylWarp;
    fx.vinylWarpRate   = daw.masterFx.vinylWarpRate;
    fx.vinylToneLP     = daw.masterFx.vinylTone;

    // Sidechain
    fx.sidechainEnabled = daw.sidechain.on;
    fx.sidechainSource  = daw.sidechain.source;
    fx.sidechainTarget  = daw.sidechain.target;
    fx.sidechainDepth   = daw.sidechain.depth;
    fx.sidechainAttack  = daw.sidechain.attack;
    fx.sidechainRelease = daw.sidechain.release;

    // Per-bus mixer
    for (int b = 0; b < NUM_BUSES; b++) {
        setBusVolume(b, daw.mixer.volume[b]);
        setBusPan(b, daw.mixer.pan[b]);
        setBusMute(b, daw.mixer.mute[b]);
        setBusSolo(b, daw.mixer.solo[b]);
        setBusReverbSend(b, daw.mixer.reverbSend[b]);
        setBusFilter(b, daw.mixer.filterOn[b], daw.mixer.filterCut[b],
                     daw.mixer.filterRes[b], daw.mixer.filterType[b]);
        setBusDistortion(b, daw.mixer.distOn[b], daw.mixer.distDrive[b],
                         daw.mixer.distMix[b]);
        setBusDelay(b, daw.mixer.delayOn[b], daw.mixer.delayTime[b],
                    daw.mixer.delayFB[b], daw.mixer.delayMix[b]);
        setBusDelaySync(b, daw.mixer.delaySync[b], daw.mixer.delaySyncDiv[b]);
    }

    // Tape/dub loop
    dubLoop.enabled      = daw.tapeFx.enabled;
    dubLoop.headTime[0]  = daw.tapeFx.headTime;
    dubLoop.feedback     = daw.tapeFx.feedback;
    dubLoop.mix          = daw.tapeFx.mix;
    dubLoop.inputSource  = daw.tapeFx.inputSource;
    dubLoop.saturation   = daw.tapeFx.saturation;
}
#endif // old renderSyncState

static void renderSyncSequencer(void) {
    seq.bpm = daw.transport.bpm;
    seq.playing = daw.transport.playing;

    // Arrangement mode (per-track patterns) takes priority — used for legacy migration too
    if (daw.arr.arrMode && daw.arr.length > 0) {
        seq.perTrackPatterns = true;
        // Use track-0 wrap for chain advance (not bar clock) so trackPatternIdx updates
        // synchronously via perTrackTable — eliminates 1-frame deferred trigger latency.
        seq.barLengthSteps = 0;
        if (daw.song.songMode) {
            // Song mode with arrangement: advance through bars as a chain.
            seq.chainLength = daw.arr.length;
            seq.chainDefaultLoops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
            for (int i = 0; i < daw.arr.length; i++) {
                // Legacy migration: song.patterns[i] holds old logical index (matches baseline logs).
                if (i < SONG_MAX_SECTIONS) {
                    seq.chain[i] = daw.song.patterns[i];
                } else {
                    int pat0 = daw.arr.cells[i][0];
                    seq.chain[i] = (pat0 >= 0) ? pat0 : 0;
                }
                seq.chainLoops[i] = daw.song.loopsPerSection[i];
            }
            // Populate per-track table for synchronous trackPatternIdx updates on bar advance
            seq.perTrackTableLen = (daw.arr.length < 64) ? daw.arr.length : 64;
            for (int _b = 0; _b < seq.perTrackTableLen; _b++)
                for (int _t = 0; _t < 8; _t++)
                    seq.perTrackTable[_b][_t] = (_t < ARR_MAX_TRACKS) ? daw.arr.cells[_b][_t] : -1;
        } else {
            // arrMode without songMode: render as pattern mode — loop old pattern 0 forever.
            // The baseline was captured with seq.currentPattern=0 and perTrackPatterns=false,
            // so all tracks played from the old multi-track pattern 0 (now sub-patterns 0..7).
            // chainLength=0 prevents chain advancement; bar stays 0 in the log.
            seq.chainLength = 0;
            // Populate perTrackTable (perTrackTableLen>0) so tracks fire step 0 immediately
            // after wrap (bypassing the clip-launcher deferred-trigger path).
            seq.perTrackTableLen = 1;
            int _base = daw.transport.currentPattern * SEQ_V2_MAX_TRACKS;
            for (int _t2 = 0; _t2 < 8; _t2++) {
                int ni = _base + _t2;
                seq.perTrackTable[0][_t2] = (ni < SEQ_NUM_PATTERNS) ? ni : -1;
            }
            for (int t = 0; t < seq.trackCount; t++) {
                int ni = _base + t;
                seq.trackPatternIdx[t] = (ni < SEQ_NUM_PATTERNS) ? ni : -1;
                seq.trackWrapped[t] = false;
            }
            daw.transport.currentPattern = seq.currentPattern;
        }
        if (daw.song.songMode) {
            int bar = seq.chainPos;
            if (bar >= 0 && bar < daw.arr.length) {
                for (int t = 0; t < seq.trackCount; t++) {
                    if (t < ARR_MAX_TRACKS) {
                        seq.trackPatternIdx[t] = daw.arr.cells[bar][t];
                    } else {
                        // Tracks beyond arr.cells (8-11): compute sub-pattern from old song
                        // pattern index stored in seq.chain[]. Replicates the old multi-track
                        // sequencer behavior where these tracks were MELODIC and consumed
                        // humanize.timingJitter RNG (not drum jitter RNG) on each step advance.
                        int oldPat = (bar < seq.chainLength) ? seq.chain[bar] : 0;
                        int ni = oldPat * SEQ_V2_MAX_TRACKS + t;
                        seq.trackPatternIdx[t] = (ni >= 0 && ni < SEQ_NUM_PATTERNS) ? ni : -1;
                    }
                    seq.trackWrapped[t] = false;
                }
            }
        }
        daw.transport.currentPattern = seq.currentPattern;
    } else if (daw.song.songMode && daw.song.length > 0) {
        seq.perTrackPatterns = false;
        for (int i = 0; i < daw.song.length; i++) {
            seq.chain[i] = daw.song.patterns[i];
            seq.chainLoops[i] = daw.song.loopsPerSection[i];
        }
        seq.chainLength = daw.song.length;
        seq.chainDefaultLoops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
        daw.transport.currentPattern = seq.currentPattern;
    } else {
        seq.chainLength = 0;
        seq.currentPattern = daw.transport.currentPattern;
        // With single-track patterns each track has its own pattern index even without
        // full arrangement mode — match daw_audio.h behavior for arrMode=false + arr.length>0.
        if (daw.arr.length > 0) {
            seq.perTrackPatterns = true;
            for (int _t = 0; _t < seq.trackCount; _t++) {
                seq.trackPatternIdx[_t] = (_t < ARR_MAX_TRACKS) ? daw.arr.cells[0][_t] : -1;
                seq.trackWrapped[_t] = false;
            }
        } else {
            seq.perTrackPatterns = false;
        }
    }

    // Per-pattern overrides
    Pattern *curPat = dawPattern();
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_BPM) {
        seq.bpm = ovr->bpm;
    }
    if (ovr->flags & PAT_OVR_GROOVE) {
        seq.dilla.swing = ovr->swing;
        seq.dilla.jitter = ovr->jitter;
        for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++)
            seq.trackSwing[t] = ovr->swing;
    }
    if (ovr->flags & PAT_OVR_MUTE) {
        for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++)
            seq.trackVolume[t] = ovr->trackMute[t] ? 0.0f : daw.mixer.volume[t];
    }

    // Sync mono mode per melodic track from patch setting
    for (int t = SEQ_DRUM_TRACKS; t < seq.trackCount; t++) {
        int patchIdx = t < NUM_PATCHES ? t : 0;
        seq.trackMono[t] = daw.patches[patchIdx].p_monoMode;
    }
}

// ============================================================================
// PRINT SONG INFO
// ============================================================================

static void printSongInfo(const char *filepath) {
    printf("Song: %s\n", filepath);
    printf("  BPM: %.1f\n", daw.transport.bpm);
    printf("  Scale lock: %s", daw.scaleLockEnabled ? "on" : "off");
    if (daw.scaleLockEnabled) {
        printf(" (root=%d, type=%d)", daw.scaleRoot, daw.scaleType);
    }
    printf("\n");
    printf("  Master vol: %.2f\n", daw.masterVol);

    // Count active patterns
    int activePatterns = 0;
    for (int p = 0; p < SEQ_NUM_PATTERNS; p++) {
        const Pattern *pat = &seq.patterns[p];
        bool hasContent = false;
        for (int s = 0; s < pat->length && !hasContent; s++) {
            if (pat->steps[s].noteCount > 0) hasContent = true;
        }
        if (hasContent) activePatterns++;
    }
    printf("  Patterns with content: %d\n", activePatterns);

    // Arrangement
    if (daw.song.songMode && daw.song.length > 0) {
        printf("  Song mode: ON (%d sections)\n", daw.song.length);
        printf("  Arrangement: ");
        for (int i = 0; i < daw.song.length; i++) {
            if (i > 0) printf(" → ");
            printf("P%d", daw.song.patterns[i] + 1);
            int l = daw.song.loopsPerSection[i];
            if (l > 0) printf("x%d", l);
            if (daw.song.names[i][0]) printf("(%s)", daw.song.names[i]);
        }
        printf("\n");
        printf("  Default loops/pattern: %d\n", daw.song.loopsPerPattern);
    } else {
        printf("  Song mode: OFF (loops pattern %d)\n", daw.transport.currentPattern + 1);
    }

    // Patches
    printf("  Patches:\n");
    const char *slotNames[] = {"Drum0", "Drum1", "Drum2", "Drum3", "Bass", "Lead", "Chord", "Extra"};
    for (int i = 0; i < NUM_PATCHES; i++) {
        SynthPatch *p = &daw.patches[i];
        if (p->p_name[0])
            printf("    [%d] %s: %s\n", i, slotNames[i], p->p_name);
        else
            printf("    [%d] %s: (default)\n", i, slotNames[i]);
    }

    // Effects
    printf("  Master FX:");
    if (daw.masterFx.distOn) printf(" dist");
    if (daw.masterFx.crushOn) printf(" crush");
    if (daw.masterFx.chorusOn) printf(" chorus");
    if (daw.masterFx.flangerOn) printf(" flanger");
    if (daw.masterFx.phaserOn) printf(" phaser");
    if (daw.masterFx.combOn) printf(" comb");
    if (daw.masterFx.tapeOn) printf(" tape");
    if (daw.masterFx.delayOn) printf(" delay");
    if (daw.masterFx.reverbOn) printf(" reverb");
    if (daw.masterFx.eqOn) printf(" eq");
    if (daw.masterFx.compOn) printf(" comp");
    if (!daw.masterFx.distOn && !daw.masterFx.crushOn && !daw.masterFx.chorusOn &&
        !daw.masterFx.flangerOn && !daw.masterFx.tapeOn && !daw.masterFx.delayOn &&
        !daw.masterFx.reverbOn && !daw.masterFx.eqOn && !daw.masterFx.compOn)
        printf(" (none)");
    printf("\n");
}

// ============================================================================
// ESTIMATE SONG DURATION
// ============================================================================

static float estimateSongDuration(void) {
    float bpm = daw.transport.bpm;
    float secondsPerStep = 60.0f / bpm / 4.0f;  // 16th note

    if (daw.arr.arrMode && daw.arr.length > 1 && daw.song.songMode) {
        // Legacy song-mode migrations: arrMode was auto-set from songMode.
        // Use track-0 pattern per bar for correct bar length (daw.song.patterns
        // stores old logical indices that no longer map to the right sub-patterns).
        int totalSteps = 0;
        for (int i = 0; i < daw.arr.length; i++) {
            int patIdx = daw.arr.cells[i][0];
            int maxLen = (patIdx >= 0 && patIdx < SEQ_NUM_PATTERNS) ? seq.patterns[patIdx].length : 16;
            // Old code used max(16, trackLength) as minimum — match that behavior
            // so the render window is long enough for all baseline-captured events.
            if (maxLen < 16) maxLen = 16;
            int loops = (i < SONG_MAX_SECTIONS) ? daw.song.loopsPerSection[i] : 0;
            if (loops <= 0) loops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
            totalSteps += maxLen * loops;
        }
        return totalSteps * secondsPerStep;
    } else if (daw.song.songMode && daw.song.length > 0) {
        int totalSteps = 0;
        for (int i = 0; i < daw.song.length; i++) {
            int patIdx = daw.song.patterns[i];
            int maxLen = seq.patterns[patIdx].length;
            if (maxLen < 1) maxLen = 16;
            int loops = daw.song.loopsPerSection[i];
            if (loops <= 0) loops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
            totalSteps += maxLen * loops;
        }
        return totalSteps * secondsPerStep;
    } else {
        // Pattern mode: can't know duration, default to a reasonable amount
        return 0;  // caller should use -d
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: song-render <file.song> [-d <seconds>] [-o <output.wav>] [--info] [--triggers <file.log>]\n");
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -d <seconds>        Render duration (default: auto for song mode, 30s for pattern mode)\n");
        fprintf(stderr, "  -o <file.wav>       Output WAV path (default: <songname>.wav, use /dev/null to skip)\n");
        fprintf(stderr, "  --info              Print song info and exit (no render)\n");
        fprintf(stderr, "  --tail <sec>        Extra seconds after song ends for reverb tail (default: 2.0)\n");
        fprintf(stderr, "  --triggers <file>   Dump note trigger log to file (for regression testing)\n");
        fprintf(stderr, "  --convert           Load and re-save in clean single-track format (in-place)\n");
        return 1;
    }

    // Parse args
    const char *songPath = NULL;
    const char *outputPath = NULL;
    const char *triggerLogPath = NULL;
    float duration = -1;
    float tailSeconds = 2.0f;
    bool infoOnly = false;
    bool verbose = false;
    bool convertMode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i+1 < argc) { duration = atof(argv[++i]); }
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) { outputPath = argv[++i]; }
        else if (strcmp(argv[i], "--tail") == 0 && i+1 < argc) { tailSeconds = atof(argv[++i]); }
        else if (strcmp(argv[i], "--triggers") == 0 && i+1 < argc) { triggerLogPath = argv[++i]; }
        else if (strcmp(argv[i], "--info") == 0) { infoOnly = true; }
        else if (strcmp(argv[i], "--convert") == 0) { convertMode = true; }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) { verbose = true; }
        else if (argv[i][0] != '-' && !songPath) { songPath = argv[i]; }
    }

    if (!songPath) {
        fprintf(stderr, "Error: no .song file specified\n");
        return 1;
    }

    // Init synth engine
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;
    synthCtx->bpm = 120.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
        voiceBus[i] = -1;
    }

    initEffects();
    initInstrumentPresets();

    // Init default patches
    for (int i = 0; i < NUM_PATCHES; i++)
        daw.patches[i] = createDefaultPatch(WAVE_SAW);

    // Init sequencer with our callbacks
    memset(dawMelodyVoice, -1, sizeof(dawMelodyVoice));
    initSequencer(renderDrumTrigger0, renderDrumTrigger1, renderDrumTrigger2, renderDrumTrigger3);
    setMelodyCallbacks(0, renderMelodyTrigger0, renderMelodyRelease0);
    setMelodyCallbacks(1, renderMelodyTrigger1, renderMelodyRelease1);
    setMelodyCallbacks(2, renderMelodyTrigger2, renderMelodyRelease2);

    // Load song
    if (!dawLoad(songPath)) {
        fprintf(stderr, "Error: failed to load '%s'\n", songPath);
        return 1;
    }

    printf("Loaded: %s\n", songPath);

    if (convertMode) {
        // Re-save in clean single-track format (migration + fill already ran during load)
        if (dawSave(songPath)) {
            printf("Converted: %s\n", songPath);
            return 0;
        } else {
            fprintf(stderr, "Error: failed to save '%s'\n", songPath);
            return 1;
        }
    }

    if (infoOnly) {
        printSongInfo(songPath);
        return 0;
    }

    // Determine duration
    float songDur = estimateSongDuration();
    if (duration < 0) {
        if (songDur > 0) {
            duration = songDur + tailSeconds;
            printf("Song mode: %.1fs + %.1fs tail = %.1fs total\n", songDur, tailSeconds, duration);
        } else {
            duration = 30.0f;
            printf("Pattern mode: rendering %.1fs (use -d to change)\n", duration);
        }
    } else {
        printf("Rendering %.1fs\n", duration);
    }

    // Build output path
    char defaultOutput[256];
    if (!outputPath) {
        // Strip directory and extension from song path
        const char *base = strrchr(songPath, '/');
        base = base ? base + 1 : songPath;
        strncpy(defaultOutput, base, sizeof(defaultOutput) - 5);
        defaultOutput[sizeof(defaultOutput) - 5] = '\0';
        char *dot = strrchr(defaultOutput, '.');
        if (dot) *dot = '\0';
        strcat(defaultOutput, ".wav");
        outputPath = defaultOutput;
    }

    // Enable trigger logging if requested
    if (triggerLogPath) {
        seqSoundLogReset();
        seqSoundLogEnabled = true;
    }

    // Sync state and start playback
    daw.transport.playing = true;
    daw.transport.currentPattern = 0;
    seq.currentPattern = 0;

    // If song mode, start from beginning
    if (daw.song.songMode && daw.song.length > 0) {
        daw.transport.currentPattern = daw.song.patterns[0];
        seq.currentPattern = daw.song.patterns[0];
    }

    renderSyncState();
    renderSyncSequencer();

    // Allocate output buffer
    int totalSamples = (int)(duration * SAMPLE_RATE);
    float *outBuf = (float *)malloc(totalSamples * sizeof(float));
    if (!outBuf) {
        fprintf(stderr, "Error: failed to allocate %.1f MB for %ds of audio\n",
                totalSamples * sizeof(float) / (1024.0f * 1024.0f), (int)duration);
        return 1;
    }

    // Render!
    float dt = 1.0f / SAMPLE_RATE;
    int lastPercent = -1;
    float peakLevel = 0.0f;

    // Sequencer update rate: every 512 samples (~86Hz, similar to 60fps frame update)
    #define SEQ_UPDATE_INTERVAL 64
    float seqDt = (float)SEQ_UPDATE_INTERVAL / SAMPLE_RATE;

    for (int i = 0; i < totalSamples; i++) {
        // Update sequencer at ~86Hz (matches frame-rate update in DAW)
        if ((i % SEQ_UPDATE_INTERVAL) == 0) {
            setMixerTempo(daw.transport.bpm);
            synthCtx->bpm = daw.transport.bpm;
            renderSyncSequencer();
            renderSyncState();
            updateSequencer(seqDt);
        }

        if (seq.playing) {
            synthCtx->beatPosition = seq.beatPosition;
        } else {
            synthCtx->beatPosition += (double)dt * (daw.transport.bpm / 60.0);
        }

        float busInputs[NUM_BUSES] = {0};

        // Process all voices and route to buses
        for (int v = 0; v < NUM_VOICES; v++) {
            float s = processVoice(&synthVoices[v], SAMPLE_RATE);
            int bus = voiceBus[v];
            if (bus >= 0 && bus < NUM_BUSES) {
                busInputs[bus] += s;
            } else {
                busInputs[BUS_CHORD] += s;
            }
        }

        // Voice state logging (once per second)
        if (verbose && i % SAMPLE_RATE == 0) {
            int active = 0, sustaining = 0, releasing = 0;
            int busCounts[NUM_BUSES] = {0};
            int busStuck[NUM_BUSES] = {0};
            for (int v = 0; v < NUM_VOICES; v++) {
                if (synthVoices[v].envStage > 0) {
                    active++;
                    if (synthVoices[v].envStage == 3) sustaining++;
                    if (synthVoices[v].envStage == 4) releasing++;
                    int b = voiceBus[v];
                    if (b >= 0 && b < NUM_BUSES) {
                        busCounts[b]++;
                        if (synthVoices[v].envStage == 3) busStuck[b]++;
                    }
                }
            }
            if (active > 0) {
                fprintf(stderr, "  t=%ds: %d active (%d sustain, %d release) | buses:",
                        i / SAMPLE_RATE, active, sustaining, releasing);
                for (int b = 0; b < NUM_BUSES; b++) {
                    if (busCounts[b] > 0)
                        fprintf(stderr, " b%d=%d(%ds)", b, busCounts[b], busStuck[b]);
                }
                fprintf(stderr, "\n");
            }
        }

        // Sidechain
        if (fx.sidechainEnabled) {
            float sidechainSample = 0.0f;
            switch (fx.sidechainSource) {
                case SIDECHAIN_SRC_KICK:  sidechainSample = busInputs[BUS_DRUM0]; break;
                case SIDECHAIN_SRC_SNARE: sidechainSample = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_CLAP:  sidechainSample = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_HIHAT: sidechainSample = busInputs[BUS_DRUM2]; break;
                default:
                    sidechainSample = busInputs[BUS_DRUM0] + busInputs[BUS_DRUM1] +
                                      busInputs[BUS_DRUM2] + busInputs[BUS_DRUM3];
                    break;
            }
            updateSidechainEnvelope(sidechainSample, dt);
            switch (fx.sidechainTarget) {
                case SIDECHAIN_TGT_BASS:  busInputs[BUS_BASS] = applySidechainDucking(busInputs[BUS_BASS]); break;
                case SIDECHAIN_TGT_LEAD:  busInputs[BUS_LEAD] = applySidechainDucking(busInputs[BUS_LEAD]); break;
                case SIDECHAIN_TGT_CHORD: busInputs[BUS_CHORD] = applySidechainDucking(busInputs[BUS_CHORD]); break;
                default:
                    busInputs[BUS_BASS]  = applySidechainDucking(busInputs[BUS_BASS]);
                    busInputs[BUS_LEAD]  = applySidechainDucking(busInputs[BUS_LEAD]);
                    busInputs[BUS_CHORD] = applySidechainDucking(busInputs[BUS_CHORD]);
                    break;
            }
        }

        // Mixer → bus FX → master FX
        float sample = processMixerOutput(busInputs, dt);
        sample *= daw.masterVol;

        // Clip
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;

        outBuf[i] = sample;

        float absS = fabsf(sample);
        if (absS > peakLevel) peakLevel = absS;

        // Progress
        int percent = (int)((float)i / totalSamples * 100);
        if (percent != lastPercent && percent % 10 == 0) {
            printf("\r  Rendering... %d%%", percent);
            fflush(stdout);
            lastPercent = percent;
        }
    }
    printf("\r  Rendering... 100%%\n");

    // Write WAV
    waWriteWav(outputPath, outBuf, totalSamples, SAMPLE_RATE);
    if (strcmp(outputPath, "/dev/null") != 0) {
        printf("Written: %s (%.1fs, peak=%.3f%s)\n", outputPath, duration, peakLevel,
               peakLevel > 0.99f ? " CLIPPING!" : "");
    }

    // Dump trigger log
    if (triggerLogPath) {
        seqSoundLogEnabled = false;
        int triggerCount = seqSoundLogCount;
        seqSoundLogDump(triggerLogPath);
        printf("Triggers: %d events -> %s\n", triggerCount, triggerLogPath);
    }

    free(outBuf);
    return 0;
}
