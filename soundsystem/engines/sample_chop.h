// PixelSynth - Sample Chop Pipeline
// Bounce .song patterns to float buffers, slice into chops, load into sampler.
//
// Requires: synth.h, synth_patch.h, patch_trigger.h, effects.h, sequencer.h,
//           sampler.h, instrument_presets.h, daw_state.h, daw_file.h
// Must be included AFTER all of the above (uses their types and globals).

#ifndef PIXELSYNTH_SAMPLE_CHOP_H
#define PIXELSYNTH_SAMPLE_CHOP_H

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef SAMPLE_CHOP_SAMPLE_RATE
#define SAMPLE_CHOP_SAMPLE_RATE 44100
#endif

// SoundSystem struct for temp context management.
// If soundsystem.h is already included this is a no-op.
#ifndef PIXELSYNTH_SOUNDSYSTEM_H
typedef struct {
    SynthContext synth;
    EffectsContext effects;
    SequencerContext sequencer;
    SamplerContext sampler;
} _ChopSoundSystem;

static void _chopUseSoundSystem(_ChopSoundSystem *ss) {
    synthCtx = &ss->synth;
    fxCtx = &ss->effects;
    seqCtx = &ss->sequencer;
    samplerCtx = &ss->sampler;
}
#define _CHOP_SS_TYPE _ChopSoundSystem
#define _CHOP_USE_SS  _chopUseSoundSystem
#else
#define _CHOP_SS_TYPE SoundSystem
#define _CHOP_USE_SS  useSoundSystem
#endif

// ============================================================================
// TYPES
// ============================================================================

typedef struct {
    float *data;        // PCM buffer (caller frees)
    int length;         // total samples
    int sampleRate;     // always 44100
    float bpm;          // source BPM (for beat-sync math)
    int stepCount;      // steps in the rendered pattern
} RenderedPattern;

typedef struct {
    int sliceCount;                     // how many slices
    int sliceLength;                    // samples per slice (equal mode), or max length (transient mode)
    int sliceLengths[SAMPLER_MAX_SAMPLES]; // per-slice lengths (0 = use sliceLength)
    float *slices[SAMPLER_MAX_SAMPLES]; // heap-allocated per-slice buffers
    float bpm;                          // inherited from source
    int stepsPerSlice;                  // sequencer steps each slice covers
} ChoppedSample;

// ============================================================================
// BOUNCE: INTERNAL CALLBACKS (static, used only by renderPatternToBuffer)
// ============================================================================

// These mirror the simplified callbacks in song_render.c.
// They exist in this header so the bounce is self-contained.

static int _chop_voiceBus[NUM_VOICES];
static int _chop_drumVoice[SEQ_DRUM_TRACKS];
static int _chop_melodyVoice[SEQ_MELODY_TRACKS][SEQ_V2_MAX_POLY];
static int _chop_melodyVoiceCount[SEQ_MELODY_TRACKS];
static int _chop_monoVoiceIdx[SEQ_MELODY_TRACKS];

// Forward reference — the bounce creates a temporary DawState on the stack,
// but the callbacks need to read patches from it. We use a file-scope pointer
// set before the render loop.
static DawState *_chop_daw;

static int _chopPatchToBus(int pi) {
    if (pi >= 0 && pi <= 3) return BUS_DRUM0 + pi;
    if (pi == 4) return BUS_BASS;
    if (pi == 5) return BUS_LEAD;
    return BUS_CHORD;
}

static void _chopDrumTrigger(int trackIdx, int busIdx, float vel, float pitchMod) {
    SynthPatch *p = &_chop_daw->patches[trackIdx];
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

    if (p->p_choke && _chop_drumVoice[trackIdx] >= 0 &&
        _chop_voiceBus[_chop_drumVoice[trackIdx]] == busIdx) {
        synthCtx->voices[_chop_drumVoice[trackIdx]].envStage = 0;
        synthCtx->voices[_chop_drumVoice[trackIdx]].envLevel = 0.0f;
    }
    bool origOneShot = p->p_oneShot;
    p->p_oneShot = true;
    int v = playNoteWithPatch(trigFreq, p);
    p->p_oneShot = origOneShot;
    _chop_drumVoice[trackIdx] = v;
    if (v >= 0) {
        _chop_voiceBus[v] = busIdx;
        synthCtx->voices[v].volume *= pVol;
    }

    p->p_decay = origDecay;
    p->p_filterCutoff = origCutoff;
}

static void _chopDrum0(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; _chopDrumTrigger(0, BUS_DRUM0, vel, pitchMod); }
static void _chopDrum1(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; _chopDrumTrigger(1, BUS_DRUM1, vel, pitchMod); }
static void _chopDrum2(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; _chopDrumTrigger(2, BUS_DRUM2, vel, pitchMod); }
static void _chopDrum3(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; _chopDrumTrigger(3, BUS_DRUM3, vel, pitchMod); }

static void _chopMelodyTrigger(int trackIdx, int note, float vel,
                                float gateTime, bool slide, bool accent) {
    (void)gateTime;
    int busTrack = trackIdx + SEQ_DRUM_TRACKS;
    SynthPatch *p = &_chop_daw->patches[busTrack];

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

    int vc = _chop_melodyVoiceCount[trackIdx];
    int lastVoice = (vc > 0) ? _chop_melodyVoice[trackIdx][vc - 1] : -1;
    if (slide && lastVoice >= 0 &&
        _chop_voiceBus[lastVoice] == _chopPatchToBus(busTrack) &&
        synthCtx->voices[lastVoice].envStage > 0) {
        Voice *sv = &synthCtx->voices[lastVoice];
        sv->targetFrequency = freq;
        float semitoneDistance = 12.0f * fabsf(logf(freq / sv->baseFrequency) / logf(2.0f));
        float slideTime = 0.06f + semitoneDistance * 0.003f;
        if (slideTime > 0.2f) slideTime = 0.2f;
        sv->glideRate = 1.0f / slideTime;
        sv->volume = pVol * p->p_volume;
        sv->filterCutoff = pCutoff;
        sv->filterResonance = pReso;
        sv->filterEnvAmt = pFilterEnv;
        sv->decay = pDecay;
        sv->filterEnvLevel = 1.0f;
        sv->filterEnvStage = 2;
        sv->filterEnvPhase = 0.0f;
        return;
    }

    float origCutoff = p->p_filterCutoff, origReso = p->p_filterResonance;
    float origFilterEnvAmt = p->p_filterEnvAmt, origDecay = p->p_decay;
    float origVolume = p->p_volume;

    p->p_filterCutoff = pCutoff;
    p->p_filterResonance = pReso;
    p->p_filterEnvAmt = pFilterEnv;
    p->p_decay = pDecay;
    p->p_volume = pVol * origVolume;

    int savedMonoVoiceIdx = monoVoiceIdx;
    if (p->p_monoMode && _chop_monoVoiceIdx[trackIdx] >= 0) {
        monoVoiceIdx = _chop_monoVoiceIdx[trackIdx];
    }

    int vi = playNoteWithPatch(freq, p);
    if (vi >= 0) {
        _chop_voiceBus[vi] = _chopPatchToBus(busTrack);
        if (p->p_monoMode) _chop_monoVoiceIdx[trackIdx] = vi;
        if (vc < SEQ_V2_MAX_POLY) {
            _chop_melodyVoice[trackIdx][vc] = vi;
            _chop_melodyVoiceCount[trackIdx] = vc + 1;
        }
    }

    if (p->p_monoMode) monoVoiceIdx = savedMonoVoiceIdx;

    p->p_filterCutoff = origCutoff;
    p->p_filterResonance = origReso;
    p->p_filterEnvAmt = origFilterEnvAmt;
    p->p_decay = origDecay;
    p->p_volume = origVolume;
}

static void _chopMel0(int n, float v, float g, float pm, bool s, bool a) { (void)pm; _chopMelodyTrigger(0, n, v, g, s, a); }
static void _chopMel1(int n, float v, float g, float pm, bool s, bool a) { (void)pm; _chopMelodyTrigger(1, n, v, g, s, a); }
static void _chopMel2(int n, float v, float g, float pm, bool s, bool a) { (void)pm; _chopMelodyTrigger(2, n, v, g, s, a); }

static void _chopMelRelease(int t) {
    for (int i = 0; i < _chop_melodyVoiceCount[t]; i++) {
        if (_chop_melodyVoice[t][i] >= 0) {
            releaseNote(_chop_melodyVoice[t][i]);
            _chop_melodyVoice[t][i] = -1;
        }
    }
    _chop_melodyVoiceCount[t] = 0;
}
static void _chopMelRel0(void) { _chopMelRelease(0); }
static void _chopMelRel1(void) { _chopMelRelease(1); }
static void _chopMelRel2(void) { _chopMelRelease(2); }

// ============================================================================
// BOUNCE: SYNC STATE (mirrors song_render.c renderSyncState/renderSyncSequencer)
// ============================================================================

static void _chopSyncState(DawState *d) {
    synthCtx->scaleLockEnabled = d->scaleLockEnabled;
    synthCtx->scaleRoot = d->scaleRoot;
    synthCtx->scaleType = d->scaleType;

    Pattern *curPat = &seq.patterns[seq.currentPattern];
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_SCALE) {
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = ovr->ovrScaleRoot;
        synthCtx->scaleType = ovr->ovrScaleType;
    }

    fx.distEnabled     = d->masterFx.distOn;
    fx.distDrive       = d->masterFx.distDrive;
    fx.distTone        = d->masterFx.distTone;
    fx.distMix         = d->masterFx.distMix;
    fx.crushEnabled    = d->masterFx.crushOn;
    fx.crushBits       = d->masterFx.crushBits;
    fx.crushRate       = d->masterFx.crushRate;
    fx.crushMix        = d->masterFx.crushMix;
    fx.chorusEnabled   = d->masterFx.chorusOn;
    fx.chorusRate      = d->masterFx.chorusRate;
    fx.chorusDepth     = d->masterFx.chorusDepth;
    fx.chorusMix       = d->masterFx.chorusMix;
    fx.flangerEnabled  = d->masterFx.flangerOn;
    fx.flangerRate     = d->masterFx.flangerRate;
    fx.flangerDepth    = d->masterFx.flangerDepth;
    fx.flangerFeedback = d->masterFx.flangerFeedback;
    fx.flangerMix      = d->masterFx.flangerMix;
    fx.phaserEnabled   = d->masterFx.phaserOn;
    fx.phaserRate      = d->masterFx.phaserRate;
    fx.phaserDepth     = d->masterFx.phaserDepth;
    fx.phaserMix       = d->masterFx.phaserMix;
    fx.phaserFeedback  = d->masterFx.phaserFeedback;
    fx.phaserStages    = d->masterFx.phaserStages;
    fx.combEnabled     = d->masterFx.combOn;
    fx.combFreq        = d->masterFx.combFreq;
    fx.combFeedback    = d->masterFx.combFeedback;
    fx.combMix         = d->masterFx.combMix;
    fx.combDamping     = d->masterFx.combDamping;
    fx.tapeEnabled     = d->masterFx.tapeOn;
    fx.tapeSaturation  = d->masterFx.tapeSaturation;
    fx.tapeWow         = d->masterFx.tapeWow;
    fx.tapeFlutter     = d->masterFx.tapeFlutter;
    fx.tapeHiss        = d->masterFx.tapeHiss;
    fx.delayEnabled    = d->masterFx.delayOn;
    fx.delayTime       = d->masterFx.delayTime;
    fx.delayFeedback   = d->masterFx.delayFeedback;
    fx.delayTone       = d->masterFx.delayTone;
    fx.delayMix        = d->masterFx.delayMix;
    fx.reverbEnabled   = d->masterFx.reverbOn;
    fx.reverbSize      = d->masterFx.reverbSize;
    fx.reverbDamping   = d->masterFx.reverbDamping;
    fx.reverbPreDelay  = d->masterFx.reverbPreDelay;
    fx.reverbMix       = d->masterFx.reverbMix;
    fx.eqEnabled       = d->masterFx.eqOn;
    fx.eqLowGain       = d->masterFx.eqLowGain;
    fx.eqHighGain      = d->masterFx.eqHighGain;
    fx.eqLowFreq       = d->masterFx.eqLowFreq;
    fx.eqHighFreq      = d->masterFx.eqHighFreq;
    fx.compEnabled     = d->masterFx.compOn;
    fx.compThreshold   = d->masterFx.compThreshold;
    fx.compRatio       = d->masterFx.compRatio;
    fx.compAttack      = d->masterFx.compAttack;
    fx.compRelease     = d->masterFx.compRelease;
    fx.compMakeup      = d->masterFx.compMakeup;

    fx.sidechainEnabled = d->sidechain.on;
    fx.sidechainSource  = d->sidechain.source;
    fx.sidechainTarget  = d->sidechain.target;
    fx.sidechainDepth   = d->sidechain.depth;
    fx.sidechainAttack  = d->sidechain.attack;
    fx.sidechainRelease = d->sidechain.release;

    for (int b = 0; b < NUM_BUSES; b++) {
        setBusVolume(b, d->mixer.volume[b]);
        setBusPan(b, d->mixer.pan[b]);
        setBusMute(b, d->mixer.mute[b]);
        setBusSolo(b, d->mixer.solo[b]);
        setBusReverbSend(b, d->mixer.reverbSend[b]);
        setBusFilter(b, d->mixer.filterOn[b], d->mixer.filterCut[b],
                     d->mixer.filterRes[b], d->mixer.filterType[b]);
        setBusDistortion(b, d->mixer.distOn[b], d->mixer.distDrive[b],
                         d->mixer.distMix[b]);
        setBusDelay(b, d->mixer.delayOn[b], d->mixer.delayTime[b],
                    d->mixer.delayFB[b], d->mixer.delayMix[b]);
        setBusDelaySync(b, d->mixer.delaySync[b], d->mixer.delaySyncDiv[b]);
    }

    dubLoop.enabled      = d->tapeFx.enabled;
    dubLoop.headTime[0]  = d->tapeFx.headTime;
    dubLoop.feedback     = d->tapeFx.feedback;
    dubLoop.mix          = d->tapeFx.mix;
    dubLoop.inputSource  = d->tapeFx.inputSource;
    dubLoop.saturation   = d->tapeFx.saturation;
}

static void _chopSyncSequencer(DawState *d) {
    seq.bpm = d->transport.bpm;
    seq.playing = d->transport.playing;

    // Single pattern mode — no song chain for bounce
    seq.chainLength = 0;
    seq.currentPattern = d->transport.currentPattern;

    Pattern *curPat = &seq.patterns[seq.currentPattern];
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_BPM) seq.bpm = ovr->bpm;
    if (ovr->flags & PAT_OVR_GROOVE) {
        seq.dilla.swing = ovr->swing;
        seq.dilla.jitter = ovr->jitter;
        for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++)
            seq.trackSwing[t] = ovr->swing;
    }
    if (ovr->flags & PAT_OVR_MUTE) {
        for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++)
            seq.trackVolume[t] = ovr->trackMute[t] ? 0.0f : d->mixer.volume[t];
    }
}

// ============================================================================
// BOUNCE: renderPatternToBuffer
// ============================================================================

// Bounce one pattern from a .song file to a float buffer.
// Spins up a temporary SoundSystem context, renders offline, restores original.
// patternIdx: which pattern (0-7) to render.
// loops: how many times to loop the pattern (1 = one pass).
// tailSeconds: extra seconds after the last note for reverb/delay tail.
// Returns heap-allocated buffer; caller must free .data.
// On failure (bad path, alloc failure), returns .data = NULL.
static RenderedPattern renderPatternToBuffer(const char *songPath, int patternIdx,
                                              int loops, float tailSeconds) {
    RenderedPattern result = {0};

    if (!songPath || patternIdx < 0 || patternIdx >= SEQ_NUM_PATTERNS || loops < 1)
        return result;

    // Save the live system's global pointers
    SynthContext *savedSynth = synthCtx;
    EffectsContext *savedFx = fxCtx;
    SequencerContext *savedSeq = seqCtx;
    SamplerContext *savedSampler = samplerCtx;

    // Create temporary contexts on the heap
    _CHOP_SS_TYPE *temp = (_CHOP_SS_TYPE *)calloc(1, sizeof(_CHOP_SS_TYPE));
    if (!temp) goto restore;

    _CHOP_USE_SS(temp);

    synthCtx->bpm = 120.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
        _chop_voiceBus[i] = -1;
    }

    initEffects();
    initInstrumentPresets();

    // Temporary DawState for the bounce (matches song_render.c defaults)
    DawState *tempDaw = (DawState *)calloc(1, sizeof(DawState));
    if (!tempDaw) goto cleanup;

    tempDaw->transport.bpm = 120.0f;
    tempDaw->crossfader.pos = 0.5f;
    tempDaw->crossfader.sceneB = 1;
    tempDaw->crossfader.count = 8;
    tempDaw->stepCount = 16;
    tempDaw->song.loopsPerPattern = 2;
    tempDaw->masterVol = 0.8f;
    tempDaw->splitPoint = 60;
    tempDaw->splitLeftPatch = 1;
    tempDaw->splitRightPatch = 2;
    for (int b = 0; b < NUM_BUSES; b++) {
        tempDaw->mixer.volume[b] = 0.8f;
        tempDaw->mixer.filterCut[b] = 1.0f;
        tempDaw->mixer.distDrive[b] = 2.0f;
        tempDaw->mixer.distMix[b] = 0.5f;
        tempDaw->mixer.delayTime[b] = 0.3f;
        tempDaw->mixer.delayFB[b] = 0.3f;
        tempDaw->mixer.delayMix[b] = 0.3f;
    }
    tempDaw->sidechain.depth = 0.8f;
    tempDaw->sidechain.attack = 0.005f;
    tempDaw->sidechain.release = 0.15f;
    tempDaw->masterFx.distDrive = 2.0f;
    tempDaw->masterFx.distTone = 0.7f;
    tempDaw->masterFx.distMix = 0.5f;
    tempDaw->masterFx.crushBits = 8.0f;
    tempDaw->masterFx.crushRate = 4.0f;
    tempDaw->masterFx.crushMix = 0.5f;
    tempDaw->masterFx.chorusRate = 1.0f;
    tempDaw->masterFx.chorusDepth = 0.3f;
    tempDaw->masterFx.chorusMix = 0.3f;
    tempDaw->masterFx.flangerRate = 0.5f;
    tempDaw->masterFx.flangerDepth = 0.5f;
    tempDaw->masterFx.flangerFeedback = 0.3f;
    tempDaw->masterFx.flangerMix = 0.3f;
    tempDaw->masterFx.tapeSaturation = 0.3f;
    tempDaw->masterFx.tapeWow = 0.1f;
    tempDaw->masterFx.tapeFlutter = 0.1f;
    tempDaw->masterFx.tapeHiss = 0.05f;
    tempDaw->masterFx.delayTime = 0.3f;
    tempDaw->masterFx.delayFeedback = 0.4f;
    tempDaw->masterFx.delayTone = 0.5f;
    tempDaw->masterFx.delayMix = 0.3f;
    tempDaw->masterFx.reverbSize = 0.5f;
    tempDaw->masterFx.reverbDamping = 0.5f;
    tempDaw->masterFx.reverbPreDelay = 0.02f;
    tempDaw->masterFx.reverbMix = 0.3f;
    tempDaw->masterFx.reverbBass = 1.0f;
    tempDaw->masterFx.eqLowFreq = 80.0f;
    tempDaw->masterFx.eqHighFreq = 6000.0f;
    tempDaw->masterFx.compThreshold = -12.0f;
    tempDaw->masterFx.compRatio = 4.0f;
    tempDaw->masterFx.compAttack = 0.01f;
    tempDaw->masterFx.compRelease = 0.1f;
    tempDaw->tapeFx.headTime = 0.5f;
    tempDaw->tapeFx.feedback = 0.6f;
    tempDaw->tapeFx.mix = 0.5f;
    tempDaw->tapeFx.saturation = 0.3f;
    tempDaw->tapeFx.toneHigh = 0.7f;
    tempDaw->tapeFx.noise = 0.05f;
    tempDaw->tapeFx.degradeRate = 0.1f;
    tempDaw->tapeFx.wow = 0.1f;
    tempDaw->tapeFx.flutter = 0.1f;
    tempDaw->tapeFx.drift = 0.05f;
    tempDaw->tapeFx.speedTarget = 1.0f;
    tempDaw->tapeFx.rewindTime = 1.5f;
    tempDaw->tapeFx.rewindMinSpeed = 0.2f;
    tempDaw->tapeFx.rewindVinyl = 0.3f;
    tempDaw->tapeFx.rewindWobble = 0.2f;
    tempDaw->tapeFx.rewindFilter = 0.5f;

    for (int i = 0; i < NUM_PATCHES; i++)
        tempDaw->patches[i] = createDefaultPatch(WAVE_SAW);

    // Set up callback pointer and init sequencer.
    // Callbacks are now stored per-instance in seq.trackNoteOn[] (part of
    // SequencerContext), so the bounce's initSequencer writes to the temp
    // context — no global adapter state to save/restore.
    _chop_daw = tempDaw;
    memset(_chop_melodyVoice, -1, sizeof(_chop_melodyVoice));
    memset(_chop_melodyVoiceCount, 0, sizeof(_chop_melodyVoiceCount));
    memset(_chop_monoVoiceIdx, -1, sizeof(_chop_monoVoiceIdx));
    memset(_chop_drumVoice, -1, sizeof(_chop_drumVoice));

    initSequencer(_chopDrum0, _chopDrum1, _chopDrum2, _chopDrum3);
    setMelodyCallbacks(0, _chopMel0, _chopMelRel0);
    setMelodyCallbacks(1, _chopMel1, _chopMelRel1);
    setMelodyCallbacks(2, _chopMel2, _chopMelRel2);

    // dawLoad reads from the global `daw` and `seq` — we need to temporarily
    // make our tempDaw the `daw` global. Since daw_file.h uses `daw` directly,
    // we swap the extern DawState. But daw is a static in the including file,
    // not a pointer. So we save/restore via memcpy.
    //
    // NOTE: This function is designed to be called from daw.c (which has a
    // static DawState daw) or from song_render.c (same). The dawLoad() function
    // reads/writes that static directly. We save its contents, load the song
    // (which overwrites daw), copy the result into our tempDaw, then restore.
    {
        // Save current daw state
        DawState savedDaw;
        memcpy(&savedDaw, &daw, sizeof(DawState));

        // Load the song (overwrites global `daw` and `seq`)
        if (!dawLoad(songPath)) {
            memcpy(&daw, &savedDaw, sizeof(DawState));
            goto cleanup_daw;
        }

        // Copy loaded state into our temp
        memcpy(tempDaw, &daw, sizeof(DawState));
        _chop_daw = tempDaw;

        // Restore original daw state
        memcpy(&daw, &savedDaw, sizeof(DawState));
    }

    // Configure for single-pattern render
    tempDaw->transport.currentPattern = patternIdx;
    tempDaw->transport.playing = true;
    tempDaw->song.songMode = false;  // Force pattern mode

    // Calculate duration: steps × loops × time-per-step + tail
    int stepCount = 16;  // default
    for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
        int tl = seq.patterns[patternIdx].trackLength[t];
        if (tl > stepCount) stepCount = tl;
    }
    float bpm = tempDaw->transport.bpm;
    {
        PatternOverrides *ovr = &seq.patterns[patternIdx].overrides;
        if (ovr->flags & PAT_OVR_BPM) bpm = ovr->bpm;
    }
    float secondsPerStep = 60.0f / bpm / 4.0f;  // 16th note
    float patternDuration = stepCount * loops * secondsPerStep;
    float totalDuration = patternDuration + tailSeconds;
    int totalSamples = (int)(totalDuration * SAMPLE_CHOP_SAMPLE_RATE);

    float *buf = (float *)malloc(totalSamples * sizeof(float));
    if (!buf) goto cleanup_daw;

    // Sync and render
    _chopSyncState(tempDaw);
    _chopSyncSequencer(tempDaw);

    float dt = 1.0f / SAMPLE_CHOP_SAMPLE_RATE;
    #define _CHOP_SEQ_UPDATE_INTERVAL 512
    float seqDt = (float)_CHOP_SEQ_UPDATE_INTERVAL / SAMPLE_CHOP_SAMPLE_RATE;

    for (int i = 0; i < totalSamples; i++) {
        if ((i % _CHOP_SEQ_UPDATE_INTERVAL) == 0) {
            setMixerTempo(tempDaw->transport.bpm);
            synthCtx->bpm = tempDaw->transport.bpm;
            _chopSyncSequencer(tempDaw);
            _chopSyncState(tempDaw);
            updateSequencer(seqDt);
        }

        if (seq.playing) {
            synthCtx->beatPosition = seq.beatPosition;
        } else {
            synthCtx->beatPosition += (double)dt * (tempDaw->transport.bpm / 60.0);
        }

        float busInputs[NUM_BUSES] = {0};
        for (int v = 0; v < NUM_VOICES; v++) {
            float s = processVoice(&synthVoices[v], SAMPLE_CHOP_SAMPLE_RATE);
            int bus = _chop_voiceBus[v];
            if (bus >= 0 && bus < NUM_BUSES)
                busInputs[bus] += s;
            else
                busInputs[BUS_CHORD] += s;
        }

        if (fx.sidechainEnabled) {
            float sc = 0.0f;
            switch (fx.sidechainSource) {
                case SIDECHAIN_SRC_KICK:  sc = busInputs[BUS_DRUM0]; break;
                case SIDECHAIN_SRC_SNARE: sc = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_CLAP:  sc = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_HIHAT: sc = busInputs[BUS_DRUM2]; break;
                default:
                    sc = busInputs[BUS_DRUM0] + busInputs[BUS_DRUM1] +
                         busInputs[BUS_DRUM2] + busInputs[BUS_DRUM3];
                    break;
            }
            updateSidechainEnvelope(sc, dt);
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

        float sample = processMixerOutput(busInputs, dt);
        sample *= tempDaw->masterVol;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        buf[i] = sample;
    }
    #undef _CHOP_SEQ_UPDATE_INTERVAL

    result.data = buf;
    result.length = totalSamples;
    result.sampleRate = SAMPLE_CHOP_SAMPLE_RATE;
    result.bpm = bpm;
    result.stepCount = stepCount;

cleanup_daw:
    free(tempDaw);
cleanup:
    free(temp);
restore:
    // Restore the live system's global pointers
    synthCtx = savedSynth;
    fxCtx = savedFx;
    seqCtx = savedSeq;
    samplerCtx = savedSampler;

    return result;
}

// ============================================================================
// CHOP: Equal-division slicer
// ============================================================================

// Split a rendered buffer into sliceCount equal-length segments.
// Each slice is a separate heap allocation; caller must call chopFree().
static ChoppedSample chopEqual(const RenderedPattern *src, int sliceCount) {
    ChoppedSample chop = {0};
    if (!src || !src->data || sliceCount < 1 || sliceCount > SAMPLER_MAX_SAMPLES)
        return chop;

    int sliceLen = src->length / sliceCount;
    if (sliceLen < 1) return chop;

    chop.sliceCount = sliceCount;
    chop.sliceLength = sliceLen;
    chop.bpm = src->bpm;
    chop.stepsPerSlice = src->stepCount / sliceCount;
    if (chop.stepsPerSlice < 1) chop.stepsPerSlice = 1;

    for (int s = 0; s < sliceCount; s++) {
        chop.slices[s] = (float *)malloc(sliceLen * sizeof(float));
        if (!chop.slices[s]) {
            // Cleanup on alloc failure
            for (int j = 0; j < s; j++) free(chop.slices[j]);
            memset(&chop, 0, sizeof(chop));
            return chop;
        }
        memcpy(chop.slices[s], src->data + s * sliceLen, sliceLen * sizeof(float));
    }

    return chop;
}

// ============================================================================
// CHOP: Transient-detection slicer
// ============================================================================

// Detect transient onsets and slice at those points.
// sensitivity: 0.0 = few slices (only big hits), 1.0 = many slices.
// maxSlices: maximum number of slices to return (1-SAMPLER_MAX_SAMPLES).
// Slices have variable lengths (unlike chopEqual).
static ChoppedSample chopAtTransients(const RenderedPattern *src, float sensitivity,
                                       int maxSlices) {
    ChoppedSample chop = {0};
    if (!src || !src->data || src->length < 1 || maxSlices < 1)
        return chop;
    if (maxSlices > SAMPLER_MAX_SAMPLES) maxSlices = SAMPLER_MAX_SAMPLES;

    // Parameters
    int windowSize = 220;  // ~5ms at 44100
    int minDistSamples = 2205;  // ~50ms minimum between onsets
    float threshold = 3.0f - sensitivity * 2.0f;  // sens 0->3.0, sens 1->1.0
    if (threshold < 1.2f) threshold = 1.2f;

    int numWindows = src->length / windowSize;
    if (numWindows < 3) return chopEqual(src, maxSlices);  // too short, fallback

    // Step 1: compute RMS energy per window
    float *energy = (float *)calloc(numWindows, sizeof(float));
    if (!energy) return chop;

    for (int w = 0; w < numWindows; w++) {
        float sum = 0;
        int base = w * windowSize;
        for (int i = 0; i < windowSize && base + i < src->length; i++) {
            float s = src->data[base + i];
            sum += s * s;
        }
        energy[w] = sqrtf(sum / windowSize);
    }

    // Step 2: compute energy ratio (spectral flux) and find peaks
    // Collect onset candidates: (sample position, strength)
    int *onsetPos = (int *)calloc(numWindows, sizeof(int));
    float *onsetStrength = (float *)calloc(numWindows, sizeof(float));
    int numOnsets = 0;

    if (!onsetPos || !onsetStrength) {
        free(energy); free(onsetPos); free(onsetStrength);
        return chop;
    }

    for (int w = 1; w < numWindows; w++) {
        float prev = energy[w - 1];
        if (prev < 0.0001f) prev = 0.0001f;  // avoid division by zero
        float ratio = energy[w] / prev;

        if (ratio > threshold) {
            int samplePos = w * windowSize;
            // Enforce minimum distance from last onset
            if (numOnsets > 0 && samplePos - onsetPos[numOnsets - 1] < minDistSamples)
                continue;
            onsetPos[numOnsets] = samplePos;
            onsetStrength[numOnsets] = ratio;
            numOnsets++;
        }
    }

    free(energy);

    if (numOnsets < 1) {
        free(onsetPos); free(onsetStrength);
        return chopEqual(src, maxSlices);  // no transients found, fallback
    }

    // Step 3: if too many onsets, keep the strongest
    if (numOnsets > maxSlices - 1) {
        // Sort by strength descending (simple selection sort, small N)
        for (int i = 0; i < maxSlices - 1; i++) {
            int best = i;
            for (int j = i + 1; j < numOnsets; j++) {
                if (onsetStrength[j] > onsetStrength[best]) best = j;
            }
            if (best != i) {
                int tp = onsetPos[i]; onsetPos[i] = onsetPos[best]; onsetPos[best] = tp;
                float ts = onsetStrength[i]; onsetStrength[i] = onsetStrength[best]; onsetStrength[best] = ts;
            }
        }
        numOnsets = maxSlices - 1;

        // Re-sort by position ascending
        for (int i = 1; i < numOnsets; i++) {
            int tp = onsetPos[i]; float ts = onsetStrength[i];
            int j = i - 1;
            while (j >= 0 && onsetPos[j] > tp) {
                onsetPos[j + 1] = onsetPos[j]; onsetStrength[j + 1] = onsetStrength[j];
                j--;
            }
            onsetPos[j + 1] = tp; onsetStrength[j + 1] = ts;
        }
    }

    // Step 4: build slices from onset points
    // Slice boundaries: [0, onset0, onset1, ..., end]
    int sliceCount = numOnsets + 1;
    chop.sliceCount = sliceCount;
    chop.bpm = src->bpm;
    chop.stepsPerSlice = src->stepCount / sliceCount;
    if (chop.stepsPerSlice < 1) chop.stepsPerSlice = 1;

    for (int s = 0; s < sliceCount; s++) {
        int start = (s == 0) ? 0 : onsetPos[s - 1];
        int end = (s < numOnsets) ? onsetPos[s] : src->length;
        int len = end - start;
        if (len < 1) len = 1;

        chop.slices[s] = (float *)malloc(len * sizeof(float));
        if (!chop.slices[s]) {
            for (int j = 0; j < s; j++) free(chop.slices[j]);
            memset(&chop, 0, sizeof(chop));
            free(onsetPos); free(onsetStrength);
            return chop;
        }
        memcpy(chop.slices[s], src->data + start, len * sizeof(float));
        chop.sliceLengths[s] = len;
    }
    // sliceLength = average (for display); actual per-slice lengths in sliceLengths[]
    chop.sliceLength = src->length / sliceCount;

    free(onsetPos);
    free(onsetStrength);
    return chop;
}

// Free all slice buffers.
static void chopFree(ChoppedSample *chop) {
    if (!chop) return;
    for (int s = 0; s < chop->sliceCount; s++) {
        free(chop->slices[s]);
        chop->slices[s] = NULL;
    }
    chop->sliceCount = 0;
}

// ============================================================================
// LOAD: Copy slices into sampler slots
// ============================================================================

// Load all slices into consecutive sampler slots starting at startSlot.
// Sets .data, .length, .sampleRate, .loaded, .name on each Sample.
// The sampler takes ownership of the slice data (embedded=false so it can free).
// Returns number of slices loaded, or 0 on error.
static int chopLoadIntoSampler(ChoppedSample *chop, int startSlot) {
    if (!chop || chop->sliceCount < 1) return 0;
    if (startSlot < 0 || startSlot + chop->sliceCount > SAMPLER_MAX_SAMPLES) return 0;

    _ensureSamplerCtx();

    for (int s = 0; s < chop->sliceCount; s++) {
        Sample *slot = &samplerCtx->samples[startSlot + s];

        // Free any previous data in this slot
        if (slot->loaded && slot->data && !slot->embedded) {
            free(slot->data);
        }

        slot->data = chop->slices[s];
        slot->length = chop->sliceLengths[s] > 0 ? chop->sliceLengths[s] : chop->sliceLength;
        slot->sampleRate = SAMPLE_CHOP_SAMPLE_RATE;
        slot->loaded = true;
        slot->embedded = false;  // sampler can free this
        snprintf(slot->name, sizeof(slot->name), "slice_%02d", s);

        // Transfer ownership — null out the chop pointer so chopFree won't double-free
        chop->slices[s] = NULL;
    }

    int loaded = chop->sliceCount;
    chop->sliceCount = 0;  // ownership transferred
    return loaded;
}

// ============================================================================
// FREEZE: Capture live audio buffers into sampler slots
// ============================================================================

// Freeze the dub loop's tape buffer into a sampler slot.
// Reads the circular buffer from oldest to newest sample.
// Returns the slot index on success, or -1 on failure.
static int dubLoopFreezeToSampler(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) return -1;
    _ensureSamplerCtx();

    int bufSize = DUB_LOOP_BUFFER_SIZE;
    float *data = (float *)malloc(bufSize * sizeof(float));
    if (!data) return -1;

    // Read circular buffer: start from write position (oldest sample)
    int writePos = (int)dubLoopWritePos % bufSize;
    for (int i = 0; i < bufSize; i++) {
        int readIdx = (writePos + i) % bufSize;
        data[i] = dubLoopBuffer[readIdx];
    }

    // Trim silence from the end (find last non-silent sample)
    int trimLen = bufSize;
    for (int i = bufSize - 1; i > 0; i--) {
        if (fabsf(data[i]) > 0.001f) { trimLen = i + 1; break; }
    }

    Sample *slot = &samplerCtx->samples[slotIndex];
    if (slot->loaded && slot->data && !slot->embedded) free(slot->data);
    slot->data = data;
    slot->length = trimLen;
    slot->sampleRate = SAMPLE_CHOP_SAMPLE_RATE;
    slot->loaded = true;
    slot->embedded = false;
    snprintf(slot->name, sizeof(slot->name), "dub_freeze");
    return slotIndex;
}

// Freeze the rewind effect's capture buffer into a sampler slot.
// The rewind buffer contains the last 3 seconds of audio that was
// captured before the rewind was triggered.
static int rewindFreezeToSampler(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= SAMPLER_MAX_SAMPLES) return -1;
    _ensureSamplerCtx();

    int bufSize = REWIND_BUFFER_SIZE;
    float *data = (float *)malloc(bufSize * sizeof(float));
    if (!data) return -1;

    // Read circular buffer from write position (oldest sample)
    int writePos = rewindWritePos % bufSize;
    for (int i = 0; i < bufSize; i++) {
        int readIdx = (writePos + i) % bufSize;
        data[i] = rewindBuffer[readIdx];
    }

    int trimLen = bufSize;
    for (int i = bufSize - 1; i > 0; i--) {
        if (fabsf(data[i]) > 0.001f) { trimLen = i + 1; break; }
    }

    Sample *slot = &samplerCtx->samples[slotIndex];
    if (slot->loaded && slot->data && !slot->embedded) free(slot->data);
    slot->data = data;
    slot->length = trimLen;
    slot->sampleRate = SAMPLE_CHOP_SAMPLE_RATE;
    slot->loaded = true;
    slot->embedded = false;
    snprintf(slot->name, sizeof(slot->name), "rewind_freeze");
    return slotIndex;
}

#endif // PIXELSYNTH_SAMPLE_CHOP_H
