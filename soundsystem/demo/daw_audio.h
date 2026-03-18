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
    // White keys (bottom row)
    {KEY_A, 0},   // C
    {KEY_S, 2},   // D
    {KEY_D, 4},   // E
    {KEY_F, 5},   // F
    {KEY_G, 7},   // G
    {KEY_H, 9},   // A
    {KEY_J, 11},  // B
    {KEY_K, 12},  // C+1
    {KEY_L, 14},  // D+1
    // Black keys (top row)
    {KEY_W, 1},   // C#
    {KEY_E, 3},   // D#
    {KEY_R, 6},   // F#
    {KEY_T, 8},   // G#
    {KEY_Y, 10},  // A#
    {KEY_U, 13},  // C#+1
    {KEY_I, 15},  // D#+1
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

// Resample capture: record master output to a buffer, then load into sampler slot
#define RESAMPLE_MAX_LENGTH (SAMPLE_RATE * 16)  // 16 seconds max
static float resampleBuffer[RESAMPLE_MAX_LENGTH];
static volatile int resampleWritePos = 0;
static volatile bool resampleCapturing = false;

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

        // Resample capture: mono mix of final output
        if (resampleCapturing && resampleWritePos < RESAMPLE_MAX_LENGTH) {
            resampleBuffer[resampleWritePos++] = (sampleL + sampleR) * 0.5f;
        }
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
static void resampleStart(void) {
    resampleWritePos = 0;
    resampleCapturing = true;
}

// Resample: stop capturing and load into next free sampler slot
// Returns slot index on success, -1 if no room
static int resampleStop(void) {
    resampleCapturing = false;
    int length = resampleWritePos;
    if (length < 64) return -1;  // too short

    _ensureSamplerCtx();

    // Find next free slot
    int slot = -1;
    for (int i = 0; i < SAMPLER_MAX_SAMPLES; i++) {
        if (!samplerCtx->samples[i].loaded) { slot = i; break; }
    }
    if (slot < 0) return -1;  // all slots full

    // Truncate to sampler max if needed
    if (length > SAMPLER_MAX_SAMPLE_LENGTH) length = SAMPLER_MAX_SAMPLE_LENGTH;

    // Allocate and copy
    float *data = (float *)malloc(length * sizeof(float));
    if (!data) return -1;
    memcpy(data, resampleBuffer, length * sizeof(float));

    // Free existing if any
    samplerFreeSample(slot);

    // Load into slot
    samplerCtx->samples[slot].data = data;
    samplerCtx->samples[slot].length = length;
    samplerCtx->samples[slot].sampleRate = SAMPLE_RATE;
    samplerCtx->samples[slot].loaded = true;
    samplerCtx->samples[slot].embedded = false;
    snprintf(samplerCtx->samples[slot].name, 64, "Resample %d", slot);

    return slot;
}

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
    int vc = dawMelodyVoiceCount[trackIdx];
    int lastVoice = (vc > 0) ? dawMelodyVoice[trackIdx][vc - 1] : -1;
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
        // Track this voice for later release
        if (vc < SEQ_V2_MAX_POLY) {
            dawMelodyVoice[trackIdx][vc] = vi;
            dawMelodyVoiceCount[trackIdx] = vc + 1;
        }
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
    for (int i = 0; i < dawMelodyVoiceCount[t]; i++) {
        if (dawMelodyVoice[t][i] >= 0) {
            voiceLogPush("REL mel[%d] v%d gate-end", t, dawMelodyVoice[t][i]);
            releaseNote(dawMelodyVoice[t][i]);
            dawMelodyVoice[t][i] = -1;
        }
    }
    dawMelodyVoiceCount[t] = 0;
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
    samplerPlay(sliceIdx, vel, speed);
}
static void dawSamplerRelease(void) { /* one-shot, no release needed */ }

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

    // Default demo beat on pattern 0
    Pattern *pat = &seq.patterns[0];
    patSetDrum(pat, 0, 0, 0.8f, 0.0f); patSetDrum(pat, 0, 4, 0.8f, 0.0f); patSetDrum(pat, 0, 8, 0.8f, 0.0f); patSetDrum(pat, 0, 12, 0.8f, 0.0f);
    patSetDrum(pat, 1, 4, 0.8f, 0.0f); patSetDrum(pat, 1, 12, 0.8f, 0.0f);
    for (int i = 0; i < 16; i += 2) patSetDrum(pat, 2, i, 0.8f, 0.0f);

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
        // Chain drives bar timing via track 0's pattern
        seq.chainLength = daw.arr.length;
        seq.chainDefaultLoops = 1;
        for (int i = 0; i < daw.arr.length; i++) {
            // Use track 0's pattern for chain timing; fallback to 0 if empty
            int pat0 = daw.arr.cells[i][0];
            seq.chain[i] = (pat0 >= 0) ? pat0 : 0;
            seq.chainLoops[i] = 0;  // use default
        }
        // Set per-track patterns for current bar
        int bar = seq.chainPos;
        if (bar >= 0 && bar < daw.arr.length) {
            for (int t = 0; t < seq.trackCount; t++) {
                seq.trackPatternIdx[t] = (t < ARR_MAX_TRACKS) ? daw.arr.cells[bar][t] : -1;
            }
        }
        daw.transport.currentPattern = seq.currentPattern;
        // Arrangement sync diagnostics (active when seqSoundLogEnabled, dump with F9)
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
        seq.perTrackPatterns = false;
        // Pattern mode: no chain, DAW controls current pattern directly
        seq.chainLength = 0;
        seq.currentPattern = daw.transport.currentPattern;
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
