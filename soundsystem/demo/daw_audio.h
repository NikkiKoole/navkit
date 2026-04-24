// PixelSynth DAW - Audio Engine Bridge
// Audio callback, piano key mapping, voice tracking, sequencer trigger callbacks
// Extracted from daw.c — include after state/debug globals are defined

#ifndef PIXELSYNTH_DAW_AUDIO_H
#define PIXELSYNTH_DAW_AUDIO_H

// ============================================================================
// AUDIO
// ============================================================================

// Piano-style key mapping (white keys on ASDFGHJKL, black keys on WERTYUIOP)
typedef struct { int key; int semitone; } DawPianoKey;
static const DawPianoKey dawPianoKeys[] = {
    // White keys — GarageBand layout (A S D F G H J K L ; ')
    {KEY_A,         0},   // C
    {KEY_S,         2},   // D
    {KEY_D,         4},   // E
    {KEY_F,         5},   // F
    {KEY_G,         7},   // G
    {KEY_H,         9},   // A
    {KEY_J,        11},   // B
    {KEY_K,        12},   // C+1
    {KEY_L,        14},   // D+1
    {KEY_SEMICOLON,16},   // E+1
    {KEY_APOSTROPHE,17},  // F+1
    // Black keys — skipping R (E-F gap) and I (B-C gap)
    {KEY_W,  1},   // C#
    {KEY_E,  3},   // D#
    {KEY_T,  6},   // F#
    {KEY_Y,  8},   // G#
    {KEY_U, 10},   // A#
    {KEY_O, 13},   // C#+1
    {KEY_P, 15},   // D#+1
};
#define NUM_DAW_PIANO_KEYS (sizeof(dawPianoKeys) / sizeof(dawPianoKeys[0]))
static int dawPianoKeyVoices[NUM_DAW_PIANO_KEYS];
static int dawCurrentOctave = 4;

static float dawSemitoneToFreq(int semitone, int octave) {
    float c0 = 16.351597831287414f;
    int totalSemitones = octave * 12 + semitone;
    // constrainToScale reads from synthCtx, synced by dawSyncEngineState()
    totalSemitones = constrainToScale(totalSemitones);
    return c0 * powf(2.0f, totalSemitones / 12.0f);
}

// Track which bus each voice belongs to (-1 = keyboard/preview → CHORD bus)
static int voiceBus[NUM_VOICES];

// Rolling capture buffer: always-on circular buffer recording master output (mono).
// UI thread grabs from it via rollingBufferGrab(). Replaces the old manual resample system.
#define ROLLING_BUFFER_SECONDS 120
#define ROLLING_BUFFER_LENGTH (SAMPLE_RATE * ROLLING_BUFFER_SECONDS)  // ~10MB

static float rollingBuffer[ROLLING_BUFFER_LENGTH];
static volatile int rollingWritePos = 0;
static volatile int rollingTotalWritten = 0;  // monotonically increasing, for "grab last N" math

// Grab the last N seconds from the rolling buffer. Returns malloc'd buffer, caller frees.
static float *rollingBufferGrab(float seconds, int *outLength) {
    int samples = (int)(seconds * SAMPLE_RATE);
    if (samples > ROLLING_BUFFER_LENGTH) samples = ROLLING_BUFFER_LENGTH;
    int available = rollingTotalWritten < ROLLING_BUFFER_LENGTH
                    ? rollingTotalWritten : ROLLING_BUFFER_LENGTH;
    if (samples > available) samples = available;
    if (samples < 1) { *outLength = 0; return NULL; }

    float *buf = (float *)malloc(samples * sizeof(float));
    if (!buf) { *outLength = 0; return NULL; }

    int wp = rollingWritePos;  // snapshot — audio thread may advance, that's OK
    int readPos = (wp - samples + ROLLING_BUFFER_LENGTH) % ROLLING_BUFFER_LENGTH;
    for (int i = 0; i < samples; i++) {
        buf[i] = rollingBuffer[(readPos + i) % ROLLING_BUFFER_LENGTH];
    }
    *outLength = samples;
    return buf;
}


// Double-buffer sync: main thread snapshots daw → shadow, audio thread applies
static DawState dawSyncShadow;
static volatile bool dawSyncPending = false;
static void dawSyncEngineStateFrom(const DawState *d);  // forward decl

static void DawAudioCallback(void *buffer, unsigned int frames) {
    // During bounce, the global context pointers are swapped to a temp system.
    // Output silence to avoid dereferencing invalid pointers.
    if (dawBouncingActive) {
        dawAudioIdle = true;  // acknowledge: audio thread is idle
        memset(buffer, 0, frames * 2 * sizeof(short));
        return;
    }

    double startTime = GetTime();
    short *d = (short *)buffer;
    float dt = 1.0f / SAMPLE_RATE;

    // Drain queued preview commands (main thread → audio thread)
    samplerDrainQueue();

    // Apply pending sync snapshot (main thread → audio thread, atomic swap)
    if (dawSyncPending) {
        dawSyncEngineStateFrom(&dawSyncShadow);
        dawSyncPending = false;
    }

    // Scale BPM by master speed so sequencer + audio stay in sync
    float effectiveBpm = daw.transport.bpm * (daw.masterSpeed > 0.01f ? daw.masterSpeed : 1.0f);
    setMixerTempo(effectiveBpm);
    synthCtx->bpm = effectiveBpm;
    if (seq.playing) {
        synthCtx->beatPosition = seq.beatPosition;
    }

    for (unsigned int i = 0; i < frames; i++) {
        // When sequencer is stopped, advance beat position from BPM so arp still works
        if (!seq.playing) {
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
                // Keyboard/preview voices → chord bus
                busInputs[BUS_CHORD] += s;
            }
        }

        // Sidechain: extract source signal
        float sidechainSample = 0.0f;
        if (fx.sidechainEnabled) {
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

        // Sidechain envelope (note-triggered): apply ducking
        if (fx.scEnvEnabled) {
            float gainMult = updateSidechainEnvelopeNote(dt);
            switch (fx.scEnvTarget) {
                case SIDECHAIN_TGT_BASS:  busInputs[BUS_BASS] = applySidechainEnvelopeDucking(busInputs[BUS_BASS], gainMult); break;
                case SIDECHAIN_TGT_LEAD:  busInputs[BUS_LEAD] = applySidechainEnvelopeDucking(busInputs[BUS_LEAD], gainMult); break;
                case SIDECHAIN_TGT_CHORD: busInputs[BUS_CHORD] = applySidechainEnvelopeDucking(busInputs[BUS_CHORD], gainMult); break;
                default:
                    busInputs[BUS_BASS]  = applySidechainEnvelopeDucking(busInputs[BUS_BASS], gainMult);
                    busInputs[BUS_LEAD]  = applySidechainEnvelopeDucking(busInputs[BUS_LEAD], gainMult);
                    busInputs[BUS_CHORD] = applySidechainEnvelopeDucking(busInputs[BUS_CHORD], gainMult);
                    break;
            }
        }

        // Mix sampler output into sampler bus (mono sum for uniform bus processing)
        {
            float sampL = 0, sampR = 0;
            processSamplerStereo(dt, &sampL, &sampR);
            busInputs[BUS_SAMPLER] += (sampL + sampR) * 0.5f;
        }

        // Full mixer → bus FX → master FX chain (stereo with bus pan)
        float sampleL, sampleR;
        processMixerOutputStereo(busInputs, dt, &sampleL, &sampleR);

        sampleL *= daw.masterVol;
        sampleR *= daw.masterVol;

        // Track peak level for output meter (before clipping)
        float absL = fabsf(sampleL), absR = fabsf(sampleR);
        float absS = (absL > absR) ? absL : absR;
        if (absS > dawPeakLevel) dawPeakLevel = absS;

        if (sampleL > 1.0f) sampleL = 1.0f;
        if (sampleL < -1.0f) sampleL = -1.0f;
        if (sampleR > 1.0f) sampleR = 1.0f;
        if (sampleR < -1.0f) sampleR = -1.0f;

        // Interleaved stereo: L, R, L, R, ...
        d[i * 2]     = (short)(sampleL * 32000.0f);
        d[i * 2 + 1] = (short)(sampleR * 32000.0f);
        dawRecSample(d[i * 2]);

        // Rolling capture: always write mono mix to circular buffer
        rollingBuffer[rollingWritePos] = (sampleL + sampleR) * 0.5f;
        rollingWritePos = (rollingWritePos + 1) % ROLLING_BUFFER_LENGTH;
        rollingTotalWritten++;

        // Scope buffer: small ring for sidebar waveform display
        dawScopeBuffer[dawScopeWritePos] = (sampleL + sampleR) * 0.5f;
        dawScopeWritePos = (dawScopeWritePos + 1) % DAW_SCOPE_SIZE;
    }

    double elapsed = (GetTime() - startTime) * 1000000.0;
    dawAudioTimeUs = dawAudioTimeUs * 0.95 + elapsed * 0.05;
    dawAudioFrameCount = frames;
}

// Shared sync: DawState → engine contexts (single source of truth)
#include "../engines/daw_sync.h"

// Sync DAW state → engine contexts (called on audio thread with shadow copy)
static void dawSyncEngineStateFrom(const DawState *d) {
    dawSyncEngineStateFromEx(d, dawPattern());
}

// Resample: start capturing master output

// Main-thread entry point: snapshot daw → shadow, set pending for audio thread
static void dawSyncEngineState(void) {
    memcpy(&dawSyncShadow, &daw, sizeof(DawState));
    dawSyncPending = true;
}

// Map patch index (0-7) to bus index for voice routing
// Patches 0-3 = drums → BUS_DRUM0-3, 4 = bass, 5 = lead, 6+ = chord
static int dawPatchToBus(int patchIdx) {
    if (patchIdx >= 0 && patchIdx <= 3) return BUS_DRUM0 + patchIdx;
    if (patchIdx == 4) return BUS_BASS;
    if (patchIdx == 5) return BUS_LEAD;
    return BUS_CHORD;
}

// ============================================================================
// SEQUENCER CALLBACKS (wiring sequencer.h engine to DAW audio)
// ============================================================================

// Voice indices per track (for release on next trigger / choke)
static int dawDrumVoice[SEQ_DRUM_TRACKS] = {-1, -1, -1, -1};
static int dawMelodyVoice[SEQ_MELODY_TRACKS][SEQ_V2_MAX_POLY];
static int dawMelodyVoiceCount[SEQ_MELODY_TRACKS] = {0, 0, 0};
// Per-track mono voice index — prevents mono patches on different tracks from stealing each other
static int dawMonoVoiceIdx[SEQ_MELODY_TRACKS] = {-1, -1, -1};

// Release all voices on a patch's bus (used when changing presets to prevent orphaned sounds)
static void dawReleaseVoicesForPatch(int patchIdx) {
    int bus = dawPatchToBus(patchIdx);
    for (int v = 0; v < NUM_VOICES; v++) {
        if (voiceBus[v] == bus && synthCtx->voices[v].envStage > 0) {
            voiceLogPush("REL v%d bus=%d preset-change", v, bus);
            releaseNote(v);
        }
    }
    if (patchIdx < SEQ_DRUM_TRACKS) {
        dawDrumVoice[patchIdx] = -1;
    } else if (patchIdx - SEQ_DRUM_TRACKS < SEQ_MELODY_TRACKS) {
        int mt = patchIdx - SEQ_DRUM_TRACKS;
        for (int vi = 0; vi < SEQ_V2_MAX_POLY; vi++) dawMelodyVoice[mt][vi] = -1;
        dawMelodyVoiceCount[mt] = 0;
        // Clear mono voice reservation so stale mono state doesn't leak into the new preset
        int mvi = dawMonoVoiceIdx[mt];
        if (mvi >= 0) {
            synthCtx->voices[mvi].monoReserved = false;
            dawMonoVoiceIdx[mt] = -1;
        }
    }
}

// Generic drum trigger with full P-lock support
// Temporarily patches SynthPatch fields for decay/tone, triggers, then restores
static void dawDrumTriggerGeneric(int trackIdx, int busIdx, float vel, float pitchMod) {
    // Sidechain envelope: trigger if this drum matches the source
    if (fx.scEnvEnabled) {
        bool match = false;
        switch (fx.scEnvSource) {
            case SIDECHAIN_SRC_KICK:  match = (trackIdx == 0); break;
            case SIDECHAIN_SRC_SNARE: match = (trackIdx == 1); break;
            case SIDECHAIN_SRC_CLAP:  match = (trackIdx == 3); break;
            case SIDECHAIN_SRC_HIHAT: match = (trackIdx == 2); break;
            default: match = true; break; // ALL
        }
        if (match) triggerSidechainEnvelope();
    }

    // Chop/flip: if this drum track has a sampler slice mapped, play that instead
    int sliceSlot = daw.chopSliceMap[trackIdx];
    if (sliceSlot >= 0 && sliceSlot < SAMPLER_MAX_SAMPLES &&
        samplerCtx && samplerCtx->samples[sliceSlot].loaded &&
        samplerCtx->samples[sliceSlot].data) {
        float pVol = plockValue(PLOCK_VOLUME, vel);
        pVol *= daw.chopSliceVolume[sliceSlot];
        float pitchMod = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
        // Combine p-lock pitch with per-slice pitch offset
        float totalPitch = pitchMod + daw.chopSlicePitch[sliceSlot];
        float pitch_speed = (totalPitch != 0.0f) ? powf(2.0f, totalPitch / 12.0f) : 1.0f;
        samplerPlay(sliceSlot, pVol, pitch_speed);
        return;
    }

    SynthPatch *p = &daw.patches[trackIdx];
    float pVol = plockValue(PLOCK_VOLUME, vel);
    seqSoundLog("DAW_DRUM  track=%d bus=%d vel=%.2f pVol=%.2f freq=%.1f mute=%d solo=%d",
                trackIdx, busIdx, vel, pVol, p->p_triggerFreq,
                daw.mixer.mute[busIdx], daw.mixer.solo[busIdx]);

    // Apply step pitch (from pitchMod) + p-lock pitch offset
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    float trigFreq = p->p_triggerFreq * pitchMod;
    if (pitchOffset != 0.0f) trigFreq *= powf(2.0f, pitchOffset / 12.0f);

    // Save & apply decay/tone p-locks to patch before trigger
    float origDecay = p->p_decay;
    float origCutoff = p->p_filterCutoff;
    float pDecay = plockValue(PLOCK_DECAY, -1.0f);
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    if (pDecay >= 0.0f) p->p_decay = pDecay;
    if (pTone >= 0.0f) p->p_filterCutoff = pTone;

    // Handle previous voice on retrigger:
    if (dawDrumVoice[trackIdx] >= 0 && voiceBus[dawDrumVoice[trackIdx]] == busIdx) {
        int prev = dawDrumVoice[trackIdx];
        if (p->p_choke) {
            // Choke: hard-kill (hi-hats, etc.)
            voiceLogPush("KILL drum[%d] v%d choke bus=%d", trackIdx, prev, busIdx);
            synthCtx->voices[prev].envStage = 0;
            synthCtx->voices[prev].envLevel = 0.0f;
        } else if (!p->p_oneShot && synthCtx->voices[prev].envStage > 0 &&
                   synthCtx->voices[prev].envStage < 4) {
            // Melodic drum (oneShot=false): release previous voice so it fades out
            // via its release envelope instead of hanging in sustain forever
            voiceLogPush("REL drum[%d] v%d melodic-retrigger bus=%d", trackIdx, prev, busIdx);
            releaseNote(prev);
        }
    }
    // Respect patch oneShot: true = normal drum (skip sustain), false = melodic drum (sustain holds until retrigger)
    int v = playNoteWithPatch(trigFreq, p);
    dawDrumVoice[trackIdx] = v;
    if (v >= 0) {
        voiceBus[v] = busIdx;
        synthCtx->voices[v].volume *= pVol;
        voiceAge[v] = 0.0f;
        voiceLogPush("ALLOC drum[%d] v%d bus=%d freq=%.0f", trackIdx, v, busIdx, trigFreq);
    }
    int activeVoices = 0;
    for (int vi = 0; vi < NUM_VOICES; vi++)
        if (synthCtx->voices[vi].envStage > 0) activeVoices++;
    seqSoundLog("DAW_DRUM_VOICE  track=%d voice=%d choke=%d active=%d/16 vol=%.3f env=%.3f stage=%d bus=%d",
                trackIdx, v, p->p_choke, activeVoices,
                v >= 0 ? synthCtx->voices[v].volume : -1.0f,
                v >= 0 ? synthCtx->voices[v].envLevel : -1.0f,
                v >= 0 ? synthCtx->voices[v].envStage : -1,
                v >= 0 ? voiceBus[v] : -1);

    // Restore original patch values
    p->p_decay = origDecay;
    p->p_filterCutoff = origCutoff;
}

static void dawDrumTrigger0(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; dawDrumTriggerGeneric(0, BUS_DRUM0, vel, pitchMod); }
static void dawDrumTrigger1(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; dawDrumTriggerGeneric(1, BUS_DRUM1, vel, pitchMod); }
static void dawDrumTrigger2(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; dawDrumTriggerGeneric(2, BUS_DRUM2, vel, pitchMod); }
static void dawDrumTrigger3(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) { (void)note; (void)gateTime; (void)slide; (void)accent; dawDrumTriggerGeneric(3, BUS_DRUM3, vel, pitchMod); }

// Melody trigger callback: called by sequencer.h for each melody note
// Full p-lock support matching demo: filter cutoff/reso/env, decay, pitch, volume
static void dawMelodyTriggerGeneric(int trackIdx, int note, float vel,
                                     float gateTime, bool slide, bool accent) {
    (void)gateTime; // gate handled by sequencer.h's tick-based countdown

    int busTrack = trackIdx + SEQ_DRUM_TRACKS;
    SynthPatch *p = &daw.patches[busTrack];

    float pVol = plockValue(PLOCK_VOLUME, vel);
    float accentFilterBoost = accent ? 0.3f : 0.0f;
    if (accent) pVol = fminf(pVol * 1.3f, 1.0f);

    float freq = patchMidiToFreq(note);
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    if (pitchOffset != 0.0f) freq *= powf(2.0f, pitchOffset / 12.0f);

    // Get p-lock values for filter (use patch values as defaults)
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    float pCutoff = (pTone >= 0.0f) ? pTone : plockValue(PLOCK_FILTER_CUTOFF, p->p_filterCutoff);
    float pReso = plockValue(PLOCK_FILTER_RESO, p->p_filterResonance);
    float pFilterEnv = plockValue(PLOCK_FILTER_ENV, p->p_filterEnvAmt) + accentFilterBoost;
    float pDecay = plockValue(PLOCK_DECAY, p->p_decay);

    // Slide: glide the most recent voice instead of retriggering
    // Scan backwards for any active voice (slots may not be contiguous in poly mode)
    int lastVoice = -1;
    for (int _si = SEQ_V2_MAX_POLY - 1; _si >= 0; _si--) {
        if (dawMelodyVoice[trackIdx][_si] >= 0) { lastVoice = dawMelodyVoice[trackIdx][_si]; break; }
    }
    if (slide && lastVoice >= 0 &&
        voiceBus[lastVoice] == dawPatchToBus(busTrack) &&
        synthCtx->voices[lastVoice].envStage > 0) {
        Voice *sv = &synthCtx->voices[lastVoice];
        sv->targetFrequency = freq;

        if (p->p_acidMode) {
            // Authentic 303: constant-time RC slew (~22ms time constant, 95% in ~60ms)
            sv->glideRate = 1.0f / 0.06f;
            sv->volume = pVol * p->p_volume;
            sv->filterCutoff = pCutoff;
            sv->filterResonance = pReso;
            sv->filterEnvAmt = pFilterEnv;
            // 303: filter env does NOT retrigger on slide — continues its decay
            // Accent on a slide step: charge the accent sweep capacitor + force short decay
            if (accent) {
                sv->accentSweepLevel += 1.0f; // Capacitor charges (accumulates!)
                sv->filterEnvDecay = 0.2f;    // Accent forces ~200ms decay
                sv->filterEnvAmt = pFilterEnv;
            }
        } else {
            // Generic slide: pitch-proportional glide
            float semitoneDistance = 12.0f * fabsf(logf(freq / sv->baseFrequency) / logf(2.0f));
            float slideTime = 0.06f + semitoneDistance * 0.003f;
            if (slideTime > 0.2f) slideTime = 0.2f;
            sv->glideRate = 1.0f / slideTime;
            sv->volume = pVol * p->p_volume;
            sv->filterCutoff = pCutoff;
            sv->filterResonance = pReso;
            sv->filterEnvAmt = pFilterEnv;
            sv->decay = pDecay;
            // Generic: retrigger filter env on every slide step
            sv->filterEnvLevel = 1.0f;
            sv->filterEnvStage = 2;
            sv->filterEnvPhase = 0.0f;
        }
        return;
    }

    // New note — temporarily apply p-lock values to patch, trigger, restore
    seqSoundLog("DAW_MELODY  track=%d note=%s bus=%d vel=%.2f slide=%d accent=%d", trackIdx, seqNoteName(note), dawPatchToBus(busTrack), pVol, slide, accent);

    float origCutoff = p->p_filterCutoff, origReso = p->p_filterResonance;
    float origFilterEnvAmt = p->p_filterEnvAmt, origDecay = p->p_decay;
    float origVolume = p->p_volume;

    p->p_filterCutoff = pCutoff;
    p->p_filterResonance = pReso;
    p->p_filterEnvAmt = pFilterEnv;
    p->p_decay = pDecay;
    p->p_volume = pVol * origVolume;

    // Set per-track mono voice so mono patches on different tracks don't steal each other
    int savedMonoVoiceIdx = monoVoiceIdx;
    bool forceNewVoice = false;
    if (p->p_monoMode) {
        int mvi = dawMonoVoiceIdx[trackIdx];
        if (mvi >= 0 && voiceBus[mvi] == dawPatchToBus(busTrack)) {
            // Voice still belongs to this track — reuse for mono glide
            monoVoiceIdx = mvi;
            seqSoundLog("MONO_GLIDE  track=%d reuse v%d (bus=%d, envStage=%d, freq=%.1f->%.1f)",
                trackIdx, mvi, voiceBus[mvi], synthCtx->voices[mvi].envStage,
                synthCtx->voices[mvi].baseFrequency, freq);
        } else {
            // No valid mono voice (first note or voice was stolen by another track)
            // Force fresh allocation via findVoice() to avoid gliding from wrong pitch
            seqSoundLog("MONO_STOLEN track=%d mvi=%d (bus=%d, expected=%d) -> forceNewVoice",
                trackIdx, mvi, mvi >= 0 ? voiceBus[mvi] : -1, dawPatchToBus(busTrack));
            if (mvi >= 0) synthCtx->voices[mvi].monoReserved = false;  // Release old reservation
            dawMonoVoiceIdx[trackIdx] = -1;
            forceNewVoice = true;
        }
    }

    // Temporarily disable mono mode if we need a fresh voice, so the engine
    // uses findVoice() instead of the stale/uninitialized monoVoiceIdx
    bool origMono = p->p_monoMode;
    if (forceNewVoice) p->p_monoMode = false;

    int vi = playNoteWithPatch(freq, p);

    if (forceNewVoice) p->p_monoMode = origMono;
    if (vi >= 0) {
        voiceBus[vi] = dawPatchToBus(busTrack);
        voiceAge[vi] = 0.0f;
        if (p->p_monoMode) {
            dawMonoVoiceIdx[trackIdx] = vi;
            synthCtx->voices[vi].monoReserved = true;  // Protect from findVoice() stealing
        }
        voiceLogPush("ALLOC mel[%d] v%d bus=%d freq=%.0f%s", trackIdx, vi, dawPatchToBus(busTrack), freq,
            forceNewVoice ? " (FORCED_NEW, no glide)" : "");
        // Track this voice for later release — use the sequencer's poly slot so
        // dawMelodyReleaseGeneric can release exactly the right voice when gate expires.
        int trigSlot = seq.trackNoteOnSlot[busTrack];
        if (trigSlot >= 0 && trigSlot < SEQ_V2_MAX_POLY) {
            dawMelodyVoice[trackIdx][trigSlot] = vi;
        }
        int _vc = 0;
        for (int _si = 0; _si < SEQ_V2_MAX_POLY; _si++) if (dawMelodyVoice[trackIdx][_si] >= 0) _vc = _si + 1;
        dawMelodyVoiceCount[trackIdx] = _vc;
        // Initialize 303 acid mode state on the voice
        Voice *nv = &synthCtx->voices[vi];
        nv->acidMode = p->p_acidMode;
        nv->accentSweepAmt = p->p_accentSweepAmt;
        nv->gimmickDipAmt = p->p_gimmickDipAmt;
        nv->accentSweepLevel = 0.0f;
        if (p->p_acidMode && accent) {
            nv->accentSweepLevel = 1.0f;       // First accent charges cap
            nv->filterEnvDecay = 0.2f;          // Accent forces ~200ms decay
        }
    }

    // Restore mono voice idx so other tracks aren't affected
    if (p->p_monoMode) monoVoiceIdx = savedMonoVoiceIdx;

    // Restore original patch values
    p->p_filterCutoff = origCutoff;
    p->p_filterResonance = origReso;
    p->p_filterEnvAmt = origFilterEnvAmt;
    p->p_decay = origDecay;
    p->p_volume = origVolume;
}

static void dawMelodyTrigger0(int note, float vel, float gt, float pitchMod, bool sl, bool ac) { (void)pitchMod; dawMelodyTriggerGeneric(0, note, vel, gt, sl, ac); }
static void dawMelodyTrigger1(int note, float vel, float gt, float pitchMod, bool sl, bool ac) { (void)pitchMod; dawMelodyTriggerGeneric(1, note, vel, gt, sl, ac); }
static void dawMelodyTrigger2(int note, float vel, float gt, float pitchMod, bool sl, bool ac) { (void)pitchMod; dawMelodyTriggerGeneric(2, note, vel, gt, sl, ac); }

static void dawMelodyReleaseGeneric(int t) {
    // Release only the specific poly slot whose gate just expired.
    // The sequencer sets trackNoteOffSlot before calling this callback.
    int busTrack = t + SEQ_DRUM_TRACKS;
    int slot = seq.trackNoteOffSlot[busTrack];
    int vi = (slot >= 0 && slot < SEQ_V2_MAX_POLY) ? dawMelodyVoice[t][slot] : -1;

    // Check for mono fallback: pop [0] from the 2-slot stack and resume it.
    // This replicates "return to previously held key" in live mono playing.
    int fallbackNote = seq.trackMonoFallbackNote[busTrack][0];
    int fallbackGate = seq.trackMonoFallbackGate[busTrack][0];
    seqSoundLog("MONO_REL  t=%d slot=%d vi=%d fb[0]=%s(%d) fb[1]=%s(%d)",
        t, slot, vi,
        seqNoteName(fallbackNote), fallbackGate,
        seqNoteName(seq.trackMonoFallbackNote[busTrack][1]), seq.trackMonoFallbackGate[busTrack][1]);
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
        // Restore sequencer state so the fallback's gate counts down normally
        seq.trackCurrentNote[busTrack][slot] = fallbackNote;
        seq.trackGateRemaining[busTrack][slot] = fallbackGate;
        seq.trackNoteOffVetoed[busTrack] = true;
        seqSoundLog("MONO_RESUME t=%d -> %s gate=%d", t, seqNoteName(fallbackNote), fallbackGate);
        voiceLogPush("RESUME mel[%d] v%d note=%s gate=%d", t, vi, seqNoteName(fallbackNote), fallbackGate);
        int _vc2 = 0;
        for (int _si = 0; _si < SEQ_V2_MAX_POLY; _si++) if (dawMelodyVoice[t][_si] >= 0) _vc2 = _si + 1;
        dawMelodyVoiceCount[t] = _vc2;
        return;
    }

    // Normal release — clear entire fallback stack
    seq.trackMonoFallbackNote[busTrack][0] = SEQ_NOTE_OFF;
    seq.trackMonoFallbackGate[busTrack][0] = 0;
    seq.trackMonoFallbackNote[busTrack][1] = SEQ_NOTE_OFF;
    seq.trackMonoFallbackGate[busTrack][1] = 0;
    if (vi >= 0) {
        voiceLogPush("REL mel[%d] v%d slot=%d gate-end", t, vi, slot);
        releaseNote(vi);
        dawMelodyVoice[t][slot] = -1;
    }
    int _vc = 0;
    for (int _si = 0; _si < SEQ_V2_MAX_POLY; _si++) if (dawMelodyVoice[t][_si] >= 0) _vc = _si + 1;
    dawMelodyVoiceCount[t] = _vc;
}
static void dawMelodyRelease0(void) { dawMelodyReleaseGeneric(0); }
static void dawMelodyRelease1(void) { dawMelodyReleaseGeneric(1); }
static void dawMelodyRelease2(void) { dawMelodyReleaseGeneric(2); }

/// Sampler track trigger: note = slice index, vel = volume, pitchMod = speed
static void dawSamplerTrigger(int note, float vel, float gateTime, float pitchMod,
                               bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (dawBouncingActive) return;
    int sliceIdx = note;
    if (sliceIdx < 0 || sliceIdx >= SAMPLER_MAX_SAMPLES) return;
    if (!samplerCtx || !samplerCtx->samples[sliceIdx].loaded) return;
    if (!samplerCtx->samples[sliceIdx].data) return;
    float totalPitch = daw.chopSlicePitch[sliceIdx];
    if (pitchMod != 1.0f) totalPitch += 12.0f * logf(pitchMod) / logf(2.0f);
    float speed = (totalPitch != 0.0f) ? powf(2.0f, totalPitch / 12.0f) : 1.0f;
    int voiceIdx = samplerPlay(sliceIdx, vel, speed);
    // samplerPlay forces pitch=1.0 when !sample->pitched — override with sequencer pitch
    if (voiceIdx >= 0 && speed != 1.0f) {
        Sample *s = &samplerCtx->samples[sliceIdx];
        float rateRatio = (float)s->sampleRate / (float)samplerCtx->sampleRate;
        samplerCtx->voices[voiceIdx].speed = speed * rateRatio;
    }
}
static void dawSamplerRelease(void) {
    // For non-oneShot (sustain/loop) samples, stop all looping voices on release.
    // One-shot samples play to completion regardless.
    _ensureSamplerCtx();
    for (int i = 0; i < SAMPLER_MAX_VOICES; i++) {
        SamplerVoice *v = &samplerCtx->voices[i];
        if (v->active && v->loop) {
            v->active = false;
        }
    }
}

// Initialize the sequencer engine with DAW callbacks
static void dawInitSequencer(void) {
    memset(dawMelodyVoice, -1, sizeof(dawMelodyVoice));
    memset(dawMelodyVoiceCount, 0, sizeof(dawMelodyVoiceCount));
    initSequencer(dawDrumTrigger0, dawDrumTrigger1, dawDrumTrigger2, dawDrumTrigger3);
    setMelodyCallbacks(0, dawMelodyTrigger0, dawMelodyRelease0);
    setMelodyCallbacks(1, dawMelodyTrigger1, dawMelodyRelease1);
    setMelodyCallbacks(2, dawMelodyTrigger2, dawMelodyRelease2);

    // Sampler track (track 7)
    seq.trackNoteOn[SEQ_TRACK_SAMPLER] = dawSamplerTrigger;
    seq.trackNoteOff[SEQ_TRACK_SAMPLER] = dawSamplerRelease;
    seq.trackNames[SEQ_TRACK_SAMPLER] = "Sampler";

    // Default demo beat: one pattern per drum voice
    // Pattern 0: kick (beats 1, 2, 3, 4 of a 16-step bar)
    seq.patterns[0].trackType = TRACK_DRUM;
    patSetDrum(&seq.patterns[0], 0, 0.8f, 0.0f);
    patSetDrum(&seq.patterns[0], 4, 0.8f, 0.0f);
    patSetDrum(&seq.patterns[0], 8, 0.8f, 0.0f);
    patSetDrum(&seq.patterns[0], 12, 0.8f, 0.0f);
    // Pattern 1: snare (beats 2 and 4)
    seq.patterns[1].trackType = TRACK_DRUM;
    patSetDrum(&seq.patterns[1], 4, 0.8f, 0.0f);
    patSetDrum(&seq.patterns[1], 12, 0.8f, 0.0f);
    // Pattern 2: hihat (every other step)
    seq.patterns[2].trackType = TRACK_DRUM;
    for (int i = 0; i < 16; i += 2) patSetDrum(&seq.patterns[2], i, 0.8f, 0.0f);

    // Pattern for sampler track: must be TRACK_SAMPLER so the sequencer takes
    // the sampler branch (reads slice field, calls dawSamplerTrigger with the
    // slot index). Without this, recorded notes go through the melodic branch
    // and dawSamplerTrigger receives the MIDI note as sliceIdx → out-of-range
    // → silent. Default arrangement wires track 7 to pattern 7.
    seq.patterns[SEQ_TRACK_SAMPLER].trackType = TRACK_SAMPLER;

    // Dilla timing starts at zero (clean grid) — user enables via Groove panel
    seq.dilla.kickNudge = 0;
    seq.dilla.snareDelay = 0;
    seq.dilla.hatNudge = 0;
    seq.dilla.clapDelay = 0;
    seq.dilla.swing = 0;
    seq.dilla.jitter = 0;
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) seq.trackSwing[i] = 0;
    seq.humanize.timingJitter = 0;
    seq.humanize.velocityJitter = 0.0f;

    // Initialize arrangement cells to ARR_EMPTY (zero-init gives 0, not -1)
    for (int b = 0; b < ARR_MAX_BARS; b++)
        for (int t = 0; t < ARR_MAX_TRACKS; t++)
            daw.arr.cells[b][t] = ARR_EMPTY;
    // Wire arrangement bar 0: each track gets its own pattern
    for (int t = 0; t < ARR_MAX_TRACKS; t++) daw.arr.cells[0][t] = t;
    daw.arr.length = 1;

    // Initialize launcher slots to empty
    daw.launcher.active = false;
    daw.launcher.quantize = 0;  // bar-quantized by default
    for (int t = 0; t < ARR_MAX_TRACKS; t++) {
        daw.launcher.tracks[t].playingSlot = -1;
        daw.launcher.tracks[t].queuedSlot = -1;
        daw.launcher.tracks[t].loopCount = 0;
        for (int s = 0; s < LAUNCHER_MAX_SLOTS; s++) {
            daw.launcher.tracks[t].pattern[s] = -1;
            daw.launcher.tracks[t].state[s] = CLIP_EMPTY;
            daw.launcher.tracks[t].nextAction[s] = NEXT_ACTION_LOOP;
            daw.launcher.tracks[t].nextActionLoops[s] = 0;
        }
    }
}

// Sync DAW transport state to sequencer engine
static void dawSyncSequencer(void) {
    seq.bpm = daw.transport.bpm;
    seq.playing = daw.transport.playing;

    // Pattern lock: force pattern loop when recording in song mode
    bool patLocked = recPatternLock && recMode == REC_RECORDING;

    // Arrangement mode: per-track pattern grid (highest priority)
    if (daw.arr.arrMode && daw.arr.length > 0 && !patLocked) {
        seq.perTrackPatterns = true;
        seq.barLengthSteps = daw.arr.barLengthSteps;  // 0 = legacy track-0 wrap
        // Chain drives bar timing (via track 0 wrap, or bar clock if barLengthSteps > 0)
        seq.chainLength = daw.arr.length;
        seq.chainDefaultLoops = 1;
        for (int i = 0; i < daw.arr.length; i++) {
            // Use track 0's pattern for chain timing; fallback to 0 if empty
            int pat0 = daw.arr.cells[i][0];
            seq.chain[i] = (pat0 >= 0) ? pat0 : 0;
            seq.chainLoops[i] = 0;  // use default
        }
        // Set per-track patterns for current bar + clear wrap flags
        int bar = seq.chainPos;
        if (bar >= 0 && bar < daw.arr.length) {
            for (int t = 0; t < seq.trackCount; t++) {
                seq.trackPatternIdx[t] = (t < ARR_MAX_TRACKS) ? daw.arr.cells[bar][t] : -1;
                seq.trackWrapped[t] = false;
            }
        }
        daw.transport.currentPattern = seq.currentPattern;
        seqSoundLog("ARR_SYNC bar=%d curPat=%d tpi=[%d %d %d %d %d %d %d %d]",
            bar, seq.currentPattern,
            seq.trackPatternIdx[0], seq.trackPatternIdx[1],
            seq.trackPatternIdx[2], seq.trackPatternIdx[3],
            seq.trackPatternIdx[4], seq.trackPatternIdx[5],
            seq.trackPatternIdx[6], seq.trackPatternIdx[7]);
    }
    // Song mode: push arrangement chain to sequencer
    else if (daw.song.songMode && daw.song.length > 0 && !patLocked) {
        seq.perTrackPatterns = false;
        for (int i = 0; i < daw.song.length; i++) {
            seq.chain[i] = daw.song.patterns[i];
            seq.chainLoops[i] = daw.song.loopsPerSection[i];
        }
        seq.chainLength = daw.song.length;
        seq.chainDefaultLoops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
        // Read back current pattern from sequencer (chain controls it)
        daw.transport.currentPattern = seq.currentPattern;
    } else if ((recMode != REC_OFF || (prChainView && daw.transport.playing)) && recChainLength > 1) {
        seq.perTrackPatterns = false;
        // Chain recording: let the chain we set up drive pattern advance
        // Sync UI pattern selector back from sequencer
        daw.transport.currentPattern = seq.currentPattern;
    } else {
        // Pattern mode: no chain, DAW controls current pattern directly
        seq.chainLength = 0;
        seq.currentPattern = daw.transport.currentPattern;
        // With single-track patterns each track always has its own pattern;
        // wire trackPatternIdx from the currently selected bar (pattern buttons).
        if (daw.arr.length > 0) {
            int bar = (editBar >= 0 && editBar < daw.arr.length) ? editBar : 0;
            seq.perTrackPatterns = true;
            for (int _t = 0; _t < seq.trackCount; _t++) {
                seq.trackPatternIdx[_t] = (_t < ARR_MAX_TRACKS) ? daw.arr.cells[bar][_t] : -1;
                seq.trackWrapped[_t] = false;  // unblock deferred trigger so step 0 fires
            }
        } else {
            seq.perTrackPatterns = false;
        }
    }

    // Helper: kill all active notes on a track
    #define _launcherSilenceTrack(t) do { \
        if (seq.trackNoteOff[t]) seq.trackNoteOff[t](); \
        for (int _v = 0; _v < SEQ_V2_MAX_POLY; _v++) { \
            seq.trackGateRemaining[t][_v] = 0; \
            seq.trackCurrentNote[t][_v] = SEQ_NOTE_OFF; \
        } \
        seq.trackActiveVoices[t] = 0; \
    } while(0)

    // Clip launcher: process per-track wrap events and override trackPatternIdx
    seq.launchQuantize = daw.launcher.quantize;
    if (daw.launcher.active) {
        seq.perTrackPatterns = true;
        for (int t = 0; t < ARR_MAX_TRACKS; t++) {
            LauncherTrack *lt = &daw.launcher.tracks[t];
            // Process wrap events (clip transitions happen on bar boundary)
            if (seq.trackWrapped[t]) {
                if (lt->queuedSlot >= 0) {
                    // Start queued clip (manual trigger takes priority)
                    if (lt->playingSlot >= 0)
                        lt->state[lt->playingSlot] = CLIP_STOPPED;
                    lt->playingSlot = lt->queuedSlot;
                    lt->state[lt->playingSlot] = CLIP_PLAYING;
                    lt->queuedSlot = -1;
                    lt->loopCount = 0;
                } else if (lt->playingSlot >= 0 && lt->state[lt->playingSlot] == CLIP_STOP_QUEUED) {
                    lt->state[lt->playingSlot] = CLIP_STOPPED;
                    lt->playingSlot = -1;
                    _launcherSilenceTrack(t);
                } else if (lt->playingSlot >= 0) {
                    lt->loopCount++;
                    // Check next action trigger
                    int ps = lt->playingSlot;
                    int nal = lt->nextActionLoops[ps];
                    if (nal > 0 && lt->loopCount >= nal) {
                        NextAction na = lt->nextAction[ps];
                        int nextSlot = -1;
                        switch (na) {
                            case NEXT_ACTION_STOP:
                                lt->state[ps] = CLIP_STOPPED;
                                lt->playingSlot = -1;
                                _launcherSilenceTrack(t);
                                break;
                            case NEXT_ACTION_NEXT:
                                for (int ns = ps + 1; ns < LAUNCHER_MAX_SLOTS; ns++) {
                                    if (lt->pattern[ns] >= 0) { nextSlot = ns; break; }
                                }
                                if (nextSlot < 0) { lt->state[ps] = CLIP_STOPPED; lt->playingSlot = -1; }
                                break;
                            case NEXT_ACTION_PREV:
                                for (int ns = ps - 1; ns >= 0; ns--) {
                                    if (lt->pattern[ns] >= 0) { nextSlot = ns; break; }
                                }
                                if (nextSlot < 0) { lt->state[ps] = CLIP_STOPPED; lt->playingSlot = -1; }
                                break;
                            case NEXT_ACTION_FIRST:
                                for (int ns = 0; ns < LAUNCHER_MAX_SLOTS; ns++) {
                                    if (lt->pattern[ns] >= 0) { nextSlot = ns; break; }
                                }
                                break;
                            case NEXT_ACTION_RANDOM: {
                                int candidates[LAUNCHER_MAX_SLOTS], nc = 0;
                                for (int ns = 0; ns < LAUNCHER_MAX_SLOTS; ns++) {
                                    if (lt->pattern[ns] >= 0) candidates[nc++] = ns;
                                }
                                if (nc > 0) nextSlot = candidates[rand() % nc];
                                break;
                            }
                            case NEXT_ACTION_RETURN:
                                lt->state[ps] = CLIP_STOPPED;
                                lt->playingSlot = -1;
                                _launcherSilenceTrack(t);
                                // Track falls back to arrangement in the override below
                                break;
                            default: break; // LOOP: do nothing
                        }
                        if (nextSlot >= 0) {
                            lt->state[ps] = CLIP_STOPPED;
                            lt->playingSlot = nextSlot;
                            lt->state[nextSlot] = CLIP_PLAYING;
                            lt->loopCount = 0;
                        }
                    }
                }
                seq.trackWrapped[t] = false;
            }
            // Launcher overrides trackPatternIdx — when launcher is active it owns all tracks
            if (lt->playingSlot >= 0) {
                seq.trackPatternIdx[t] = lt->pattern[lt->playingSlot];
            } else {
                // No launcher clip on this track: silence it
                seq.trackPatternIdx[t] = -1;
            }
        }
        // Auto-deactivate launcher when nothing is playing or queued
        bool anyActive = false;
        for (int t = 0; t < ARR_MAX_TRACKS; t++) {
            LauncherTrack *lt = &daw.launcher.tracks[t];
            if (lt->playingSlot >= 0 || lt->queuedSlot >= 0) { anyActive = true; break; }
        }
        if (!anyActive) daw.launcher.active = false;
    }

    // Apply per-pattern overrides (BPM, groove, mutes) without modifying DAW state
    Pattern *curPat = dawPattern();
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_BPM) {
        seq.bpm = ovr->bpm;  // Override sequencer BPM, not daw.transport.bpm
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

    // Track the current step for playhead display
    // Use drum track 0 as master step reference
    daw.transport.currentStep = seq.trackStep[0];
}

static void dawStopSequencer(void) {
    seq.playing = false;
    daw.transport.playing = false;
    daw.transport.currentStep = 0;
    // Reset sequencer playback state
    for (int t = 0; t < seq.trackCount; t++) {
        seq.trackStep[t] = 0;
        seq.trackTick[t] = 0;
        seq.trackTriggered[t] = false;
        memset(seq.trackGateRemaining[t], 0, sizeof(seq.trackGateRemaining[t]));
        for (int v = 0; v < SEQ_V2_MAX_POLY; v++) seq.trackCurrentNote[t][v] = SEQ_NOTE_OFF;
        seq.trackActiveVoices[t] = 0;
        seq.trackSustainRemaining[t] = 0;
    }
    // Release ALL active voices — not just tracked ones, since voice indices
    // only remember the last voice per track (orphaned voices would linger).
    // releaseNote starts the release envelope so voices fade out naturally.
    // This is safe on stop because no new notes will allocate fresh slots.
    for (int i = 0; i < NUM_VOICES; i++) {
        if (synthCtx->voices[i].envStage > 0 && synthCtx->voices[i].envStage < 4) {
            releaseNote(i);
        }
    }
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) dawDrumVoice[t] = -1;
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) { memset(dawMelodyVoice[t], -1, sizeof(dawMelodyVoice[t])); dawMelodyVoiceCount[t] = 0; }
    seq.tickTimer = 0.0f;
    // Reset chain playback to beginning
    seq.chainPos = 0;
    seq.chainLoopCount = 0;
    seq._barStep = 0;
    seq._barTick = 0;
    if (daw.arr.arrMode && daw.arr.length > 0) {
        // Rewind arrangement to first bar, set track 0's pattern
        int pat0 = daw.arr.cells[0][0];
        daw.transport.currentPattern = (pat0 >= 0) ? pat0 : 0;
        seq.currentPattern = daw.transport.currentPattern;
    } else if (daw.song.songMode && daw.song.length > 0) {
        daw.transport.currentPattern = daw.song.patterns[0];
        seq.currentPattern = daw.song.patterns[0];
    } else if (prChainView && recChainStart >= 0) {
        // Rewind to start of chain
        daw.transport.currentPattern = recChainStart;
        seq.currentPattern = recChainStart;
    }
}

// Arp keyboard state: single voice collecting held keys
static int dawArpVoice = -1;
static bool dawArpKeyHeld[20] = {false}; // per-piano-key held state
static int dawArpPrevHeldCount = 0;
static float dawArpPrevFreqs[8] = {0};

#endif // PIXELSYNTH_DAW_AUDIO_H
