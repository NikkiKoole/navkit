// daw_file.h — Save/load DAW state to .song text files
//
// Header-only. Include after synth_patch.h, sequencer.h, effects.h.
// Does NOT depend on drums.h.
//
// Format: INI-style sections + key=value, same family as song_file.h
// but with DAW-specific sections (mixer, masterfx, tapefx, etc.)
//
// Usage in daw.c:
//   #include "daw_file.h"
//   dawSave("songs/mysong.song", &daw, &seq);
//   dawLoad("songs/mysong.song", &daw, &seq);

#ifndef DAW_FILE_H
#define DAW_FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DAW_FILE_FORMAT 2

// ============================================================================
// WRITE HELPERS
// ============================================================================

static void _dw(FILE *f, const char *k, float v)  { fprintf(f, "%s = %.6g\n", k, (double)v); }
static void _di(FILE *f, const char *k, int v)     { fprintf(f, "%s = %d\n", k, v); }
static void _db(FILE *f, const char *k, bool v)    { fprintf(f, "%s = %s\n", k, v ? "true" : "false"); }
static void _ds(FILE *f, const char *k, const char *v) { if (v[0]) fprintf(f, "%s = \"%s\"\n", k, v); }

static const char* _dwWaveNames[] = {
    "square","saw","triangle","noise","scw","voice","pluck",
    "additive","mallet","granular","fm","pd","membrane","bird"
};

static void _dwWritePatch(FILE *f, const char *sec, const SynthPatch *p) {
    fprintf(f, "\n[%s]\n", sec);
    if (p->p_name[0]) _ds(f, "name", p->p_name);
    if (p->p_waveType >= 0 && p->p_waveType < 14) fprintf(f, "waveType = %s\n", _dwWaveNames[p->p_waveType]);
    else _di(f, "waveType", p->p_waveType);
    _di(f, "scwIndex", p->p_scwIndex);
    _dw(f, "attack", p->p_attack); _dw(f, "decay", p->p_decay);
    _dw(f, "sustain", p->p_sustain); _dw(f, "release", p->p_release);
    _db(f, "expRelease", p->p_expRelease); _db(f, "expDecay", p->p_expDecay);
    _db(f, "oneShot", p->p_oneShot);
    _dw(f, "volume", p->p_volume);
    _dw(f, "pulseWidth", p->p_pulseWidth); _dw(f, "pwmRate", p->p_pwmRate); _dw(f, "pwmDepth", p->p_pwmDepth);
    _dw(f, "vibratoRate", p->p_vibratoRate); _dw(f, "vibratoDepth", p->p_vibratoDepth);
    _dw(f, "filterCutoff", p->p_filterCutoff); _dw(f, "filterResonance", p->p_filterResonance);
    _di(f, "filterType", p->p_filterType);
    _dw(f, "filterEnvAmt", p->p_filterEnvAmt); _dw(f, "filterEnvAttack", p->p_filterEnvAttack);
    _dw(f, "filterEnvDecay", p->p_filterEnvDecay);
    _dw(f, "filterKeyTrack", p->p_filterKeyTrack);
    _dw(f, "filterLfoRate", p->p_filterLfoRate); _dw(f, "filterLfoDepth", p->p_filterLfoDepth);
    _di(f, "filterLfoShape", p->p_filterLfoShape); _di(f, "filterLfoSync", p->p_filterLfoSync);
    _dw(f, "resoLfoRate", p->p_resoLfoRate); _dw(f, "resoLfoDepth", p->p_resoLfoDepth);
    _di(f, "resoLfoShape", p->p_resoLfoShape); _di(f, "resoLfoSync", p->p_resoLfoSync);
    _dw(f, "ampLfoRate", p->p_ampLfoRate); _dw(f, "ampLfoDepth", p->p_ampLfoDepth);
    _di(f, "ampLfoShape", p->p_ampLfoShape); _di(f, "ampLfoSync", p->p_ampLfoSync);
    _dw(f, "pitchLfoRate", p->p_pitchLfoRate); _dw(f, "pitchLfoDepth", p->p_pitchLfoDepth);
    _di(f, "pitchLfoShape", p->p_pitchLfoShape); _di(f, "pitchLfoSync", p->p_pitchLfoSync);
    _db(f, "arpEnabled", p->p_arpEnabled); _di(f, "arpMode", p->p_arpMode);
    _di(f, "arpRateDiv", p->p_arpRateDiv); _dw(f, "arpRate", p->p_arpRate);
    _di(f, "arpChord", p->p_arpChord);
    _di(f, "unisonCount", p->p_unisonCount); _dw(f, "unisonDetune", p->p_unisonDetune);
    _dw(f, "unisonMix", p->p_unisonMix);
    _db(f, "monoMode", p->p_monoMode); _dw(f, "glideTime", p->p_glideTime);
    _dw(f, "pluckBrightness", p->p_pluckBrightness); _dw(f, "pluckDamping", p->p_pluckDamping);
    _dw(f, "pluckDamp", p->p_pluckDamp);
    _di(f, "additivePreset", p->p_additivePreset); _dw(f, "additiveBrightness", p->p_additiveBrightness);
    _dw(f, "additiveShimmer", p->p_additiveShimmer); _dw(f, "additiveInharmonicity", p->p_additiveInharmonicity);
    _di(f, "malletPreset", p->p_malletPreset); _dw(f, "malletStiffness", p->p_malletStiffness);
    _dw(f, "malletHardness", p->p_malletHardness); _dw(f, "malletStrikePos", p->p_malletStrikePos);
    _dw(f, "malletResonance", p->p_malletResonance); _dw(f, "malletTremolo", p->p_malletTremolo);
    _dw(f, "malletTremoloRate", p->p_malletTremoloRate); _dw(f, "malletDamp", p->p_malletDamp);
    _di(f, "voiceVowel", p->p_voiceVowel); _dw(f, "voiceFormantShift", p->p_voiceFormantShift);
    _dw(f, "voiceBreathiness", p->p_voiceBreathiness); _dw(f, "voiceBuzziness", p->p_voiceBuzziness);
    _dw(f, "voiceSpeed", p->p_voiceSpeed); _dw(f, "voicePitch", p->p_voicePitch);
    _db(f, "voiceConsonant", p->p_voiceConsonant); _dw(f, "voiceConsonantAmt", p->p_voiceConsonantAmt);
    _db(f, "voiceNasal", p->p_voiceNasal); _dw(f, "voiceNasalAmt", p->p_voiceNasalAmt);
    _dw(f, "voicePitchEnv", p->p_voicePitchEnv); _dw(f, "voicePitchEnvTime", p->p_voicePitchEnvTime);
    _dw(f, "voicePitchEnvCurve", p->p_voicePitchEnvCurve);
    _di(f, "granularScwIndex", p->p_granularScwIndex); _dw(f, "granularGrainSize", p->p_granularGrainSize);
    _dw(f, "granularDensity", p->p_granularDensity); _dw(f, "granularPosition", p->p_granularPosition);
    _dw(f, "granularPosRandom", p->p_granularPosRandom); _dw(f, "granularPitch", p->p_granularPitch);
    _dw(f, "granularPitchRandom", p->p_granularPitchRandom); _dw(f, "granularAmpRandom", p->p_granularAmpRandom);
    _dw(f, "granularSpread", p->p_granularSpread); _db(f, "granularFreeze", p->p_granularFreeze);
    _dw(f, "fmModRatio", p->p_fmModRatio); _dw(f, "fmModIndex", p->p_fmModIndex);
    _dw(f, "fmFeedback", p->p_fmFeedback);
    _dw(f, "fmMod2Ratio", p->p_fmMod2Ratio); _dw(f, "fmMod2Index", p->p_fmMod2Index);
    _di(f, "fmAlgorithm", p->p_fmAlgorithm);
    _di(f, "pdWaveType", p->p_pdWaveType); _dw(f, "pdDistortion", p->p_pdDistortion);
    _di(f, "membranePreset", p->p_membranePreset); _dw(f, "membraneDamping", p->p_membraneDamping);
    _dw(f, "membraneStrike", p->p_membraneStrike); _dw(f, "membraneBend", p->p_membraneBend);
    _dw(f, "membraneBendDecay", p->p_membraneBendDecay);
    _dw(f, "bowPressure", p->p_bowPressure); _dw(f, "bowSpeed", p->p_bowSpeed);
    _dw(f, "bowPosition", p->p_bowPosition);
    _dw(f, "pipeBreath", p->p_pipeBreath); _dw(f, "pipeEmbouchure", p->p_pipeEmbouchure);
    _dw(f, "pipeBore", p->p_pipeBore); _dw(f, "pipeOverblow", p->p_pipeOverblow);
    _di(f, "birdType", p->p_birdType); _dw(f, "birdChirpRange", p->p_birdChirpRange);
    _dw(f, "birdTrillRate", p->p_birdTrillRate); _dw(f, "birdTrillDepth", p->p_birdTrillDepth);
    _dw(f, "birdAmRate", p->p_birdAmRate); _dw(f, "birdAmDepth", p->p_birdAmDepth);
    _dw(f, "birdHarmonics", p->p_birdHarmonics);
    _dw(f, "pitchEnvAmount", p->p_pitchEnvAmount); _dw(f, "pitchEnvDecay", p->p_pitchEnvDecay);
    _dw(f, "pitchEnvCurve", p->p_pitchEnvCurve); _db(f, "pitchEnvLinear", p->p_pitchEnvLinear);
    _dw(f, "noiseMix", p->p_noiseMix); _dw(f, "noiseTone", p->p_noiseTone);
    _dw(f, "noiseHP", p->p_noiseHP); _dw(f, "noiseDecay", p->p_noiseDecay);
    _di(f, "retriggerCount", p->p_retriggerCount); _dw(f, "retriggerSpread", p->p_retriggerSpread);
    _db(f, "retriggerOverlap", p->p_retriggerOverlap); _dw(f, "retriggerBurstDecay", p->p_retriggerBurstDecay);
    _dw(f, "osc2Ratio", p->p_osc2Ratio); _dw(f, "osc2Level", p->p_osc2Level);
    _dw(f, "osc3Ratio", p->p_osc3Ratio); _dw(f, "osc3Level", p->p_osc3Level);
    _dw(f, "osc4Ratio", p->p_osc4Ratio); _dw(f, "osc4Level", p->p_osc4Level);
    _dw(f, "osc5Ratio", p->p_osc5Ratio); _dw(f, "osc5Level", p->p_osc5Level);
    _dw(f, "osc6Ratio", p->p_osc6Ratio); _dw(f, "osc6Level", p->p_osc6Level);
    _dw(f, "drive", p->p_drive);
    _dw(f, "clickLevel", p->p_clickLevel); _dw(f, "clickTime", p->p_clickTime);
    _di(f, "noiseMode", p->p_noiseMode); _di(f, "oscMixMode", p->p_oscMixMode);
    _dw(f, "retriggerCurve", p->p_retriggerCurve); _db(f, "phaseReset", p->p_phaseReset);
    _db(f, "noiseLPBypass", p->p_noiseLPBypass); _di(f, "noiseType", p->p_noiseType);
    _db(f, "useTriggerFreq", p->p_useTriggerFreq); _dw(f, "triggerFreq", p->p_triggerFreq);
    _db(f, "choke", p->p_choke);
    _db(f, "analogRolloff", p->p_analogRolloff); _db(f, "tubeSaturation", p->p_tubeSaturation);
    _db(f, "ringMod", p->p_ringMod); _dw(f, "ringModFreq", p->p_ringModFreq);
    _dw(f, "wavefoldAmount", p->p_wavefoldAmount);
    _db(f, "hardSync", p->p_hardSync); _dw(f, "hardSyncRatio", p->p_hardSyncRatio);
    // 303 acid mode
    _db(f, "acidMode", p->p_acidMode);
    _dw(f, "accentSweepAmt", p->p_accentSweepAmt); _dw(f, "gimmickDipAmt", p->p_gimmickDipAmt);
}

// ============================================================================
// PATTERN WRITE
// ============================================================================

static const char* _dwNoteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static void _dwMidiToName(int midi, char *buf, int sz) {
    if (midi == SEQ_NOTE_OFF || midi < 0) { snprintf(buf, sz, "REST"); return; }
    snprintf(buf, sz, "%s%d", _dwNoteNames[midi % 12], (midi / 12) - 1);
}

static const char* _dwCondNames[] = {"always","1:2","2:2","1:4","2:4","3:4","4:4","fill","notFill","first","notFirst"};
static const char* _dwChordNames[] = {"single","fifth","triad","inv1","inv2","seventh","octave","octaves","custom"};
static const char* _dwPickNames[] = {"cycleUp","cycleDown","pingpong","random","randomWalk","all"};
static const char* _dwPlockNames[] = {"cutoff","reso","filterEnv","decay","volume","pitch","pulseWidth","tone","punch","nudge","flamTime","flamVelocity","gateNudge"};
_Static_assert(sizeof(_dwPlockNames)/sizeof(_dwPlockNames[0]) == PLOCK_COUNT, "Update _dwPlockNames when adding p-lock params");

static void _dwWritePattern(FILE *f, int idx, const Pattern *p) {
    fprintf(f, "\n[pattern.%d]\n", idx);
    fprintf(f, "drumTrackLength =");
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) fprintf(f, " %d", p->trackLength[t]);
    fprintf(f, "\nmelodyTrackLength =");
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) fprintf(f, " %d", p->trackLength[SEQ_DRUM_TRACKS + t]);
    fprintf(f, "\n");

    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < p->trackLength[t]; s++) {
            if (!patGetDrum((Pattern*)p, t, s)) continue;
            fprintf(f, "d track=%d step=%d vel=%.3g", t, s, (double)patGetDrumVel((Pattern*)p, t, s));
            float dp = patGetDrumPitch((Pattern*)p, t, s);
            if (dp != 0.0f) fprintf(f, " pitch=%.3g", (double)dp);
            float dProb = patGetDrumProb((Pattern*)p, t, s);
            if (dProb > 0.0f && dProb < 1.0f) fprintf(f, " prob=%.3g", (double)dProb);
            int dCond = patGetDrumCond((Pattern*)p, t, s);
            if (dCond != COND_ALWAYS) fprintf(f, " cond=%s", _dwCondNames[dCond]);
            fprintf(f, "\n");
        }
    }

    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        int absTrack = SEQ_DRUM_TRACKS + t;
        for (int s = 0; s < p->trackLength[absTrack]; s++) {
            const StepV2 *sv = &p->steps[absTrack][s];
            if (sv->noteCount == 0) continue;

            for (int v = 0; v < sv->noteCount; v++) {
                const StepNote *sn = &sv->notes[v];
                if (sn->note == SEQ_NOTE_OFF) continue;
                char nn[8]; _dwMidiToName(sn->note, nn, sizeof(nn));
                fprintf(f, "m track=%d step=%d note=%s vel=%.3g gate=%d",
                        t, s, nn, (double)velU8ToFloat(sn->velocity), (int)sn->gate);
                if (sn->slide) fprintf(f, " slide");
                if (sn->accent) fprintf(f, " accent");
                if (sn->nudge != 0) fprintf(f, " nudge=%d", (int)sn->nudge);
                if (sn->gateNudge != 0) fprintf(f, " gateNudge=%d", (int)sn->gateNudge);
                if (v == 0) {
                    int mSus = (int)sv->sustain;
                    if (mSus > 0) fprintf(f, " sustain=%d", mSus);
                    float mProb = probU8ToFloat(sv->probability);
                    if (mProb > 0.0f && mProb < 1.0f) fprintf(f, " prob=%.3g", (double)mProb);
                    int mCond = (int)sv->condition;
                    if (mCond != COND_ALWAYS) fprintf(f, " cond=%s", _dwCondNames[mCond]);
                    if (sv->pickMode != PICK_ALL)
                        fprintf(f, " pick=%d", (int)sv->pickMode);
                }
                fprintf(f, "\n");
            }
        }
    }

    for (int i = 0; i < p->plockCount; i++) {
        const PLock *pl = &p->plocks[i];
        if (pl->param < PLOCK_COUNT)
            fprintf(f, "p track=%d step=%d param=%s val=%.6g\n",
                    pl->track, pl->step, _dwPlockNames[pl->param], (double)pl->value);
    }
}

// ============================================================================
// dawSave — requires DawState*, SequencerContext* (via seq macro)
// ============================================================================
// Note: This function accesses daw.* and seq.* globals directly.
// The DawState and sequencer types must be visible before including this header.

#ifndef _DAW_FILE_SAVE_LOAD
#define _DAW_FILE_SAVE_LOAD

static bool dawSave(const char *filepath) {
    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "# PixelSynth DAW song (format %d)\n", DAW_FILE_FORMAT);

    // [song]
    fprintf(f, "\n[song]\n");
    _di(f, "format", DAW_FILE_FORMAT);
    _dw(f, "bpm", daw.transport.bpm);
    _di(f, "stepCount", daw.stepCount);
    _di(f, "grooveSwing", daw.transport.grooveSwing);
    _di(f, "grooveJitter", daw.transport.grooveJitter);

    // [scale]
    fprintf(f, "\n[scale]\n");
    _db(f, "enabled", daw.scaleLockEnabled);
    _di(f, "root", daw.scaleRoot);
    _di(f, "type", daw.scaleType);

    // [groove]
    fprintf(f, "\n[groove]\n");
    _di(f, "kickNudge", seq.dilla.kickNudge);
    _di(f, "snareDelay", seq.dilla.snareDelay);
    _di(f, "hatNudge", seq.dilla.hatNudge);
    _di(f, "clapDelay", seq.dilla.clapDelay);
    _di(f, "swing", seq.dilla.swing);
    _di(f, "jitter", seq.dilla.jitter);
    _di(f, "melodyTimingJitter", seq.humanize.timingJitter);
    _dw(f, "melodyVelocityJitter", seq.humanize.velocityJitter);

    // [settings]
    fprintf(f, "\n[settings]\n");
    _dw(f, "masterVol", daw.masterVol);
    _db(f, "voiceRandomVowel", daw.voiceRandomVowel);

    // [patch.0] through [patch.7]
    for (int i = 0; i < NUM_PATCHES; i++) {
        char sec[16]; snprintf(sec, sizeof(sec), "patch.%d", i);
        _dwWritePatch(f, sec, &daw.patches[i]);
    }

    // [mixer]
    fprintf(f, "\n[mixer]\n");
    for (int b = 0; b < NUM_BUSES; b++) {
        char k[32];
        snprintf(k, sizeof(k), "vol%d", b); _dw(f, k, daw.mixer.volume[b]);
        snprintf(k, sizeof(k), "pan%d", b); _dw(f, k, daw.mixer.pan[b]);
        snprintf(k, sizeof(k), "reverbSend%d", b); _dw(f, k, daw.mixer.reverbSend[b]);
        snprintf(k, sizeof(k), "mute%d", b); _db(f, k, daw.mixer.mute[b]);
        snprintf(k, sizeof(k), "solo%d", b); _db(f, k, daw.mixer.solo[b]);
        snprintf(k, sizeof(k), "filterOn%d", b); _db(f, k, daw.mixer.filterOn[b]);
        snprintf(k, sizeof(k), "filterCut%d", b); _dw(f, k, daw.mixer.filterCut[b]);
        snprintf(k, sizeof(k), "filterRes%d", b); _dw(f, k, daw.mixer.filterRes[b]);
        snprintf(k, sizeof(k), "filterType%d", b); _di(f, k, daw.mixer.filterType[b]);
        snprintf(k, sizeof(k), "distOn%d", b); _db(f, k, daw.mixer.distOn[b]);
        snprintf(k, sizeof(k), "distDrive%d", b); _dw(f, k, daw.mixer.distDrive[b]);
        snprintf(k, sizeof(k), "distMix%d", b); _dw(f, k, daw.mixer.distMix[b]);
        snprintf(k, sizeof(k), "delayOn%d", b); _db(f, k, daw.mixer.delayOn[b]);
        snprintf(k, sizeof(k), "delaySync%d", b); _db(f, k, daw.mixer.delaySync[b]);
        snprintf(k, sizeof(k), "delaySyncDiv%d", b); _di(f, k, daw.mixer.delaySyncDiv[b]);
        snprintf(k, sizeof(k), "delayTime%d", b); _dw(f, k, daw.mixer.delayTime[b]);
        snprintf(k, sizeof(k), "delayFB%d", b); _dw(f, k, daw.mixer.delayFB[b]);
        snprintf(k, sizeof(k), "delayMix%d", b); _dw(f, k, daw.mixer.delayMix[b]);
    }

    // [sidechain]
    fprintf(f, "\n[sidechain]\n");
    _db(f, "on", daw.sidechain.on);
    _di(f, "source", daw.sidechain.source); _di(f, "target", daw.sidechain.target);
    _dw(f, "depth", daw.sidechain.depth); _dw(f, "attack", daw.sidechain.attack);
    _dw(f, "release", daw.sidechain.release);

    // [masterfx]
    fprintf(f, "\n[masterfx]\n");
    _db(f, "distOn", daw.masterFx.distOn); _dw(f, "distDrive", daw.masterFx.distDrive);
    _dw(f, "distTone", daw.masterFx.distTone); _dw(f, "distMix", daw.masterFx.distMix);
    _db(f, "crushOn", daw.masterFx.crushOn); _dw(f, "crushBits", daw.masterFx.crushBits);
    _dw(f, "crushRate", daw.masterFx.crushRate); _dw(f, "crushMix", daw.masterFx.crushMix);
    _db(f, "chorusOn", daw.masterFx.chorusOn); _dw(f, "chorusRate", daw.masterFx.chorusRate);
    _dw(f, "chorusDepth", daw.masterFx.chorusDepth); _dw(f, "chorusMix", daw.masterFx.chorusMix);
    _db(f, "flangerOn", daw.masterFx.flangerOn); _dw(f, "flangerRate", daw.masterFx.flangerRate);
    _dw(f, "flangerDepth", daw.masterFx.flangerDepth); _dw(f, "flangerFeedback", daw.masterFx.flangerFeedback);
    _dw(f, "flangerMix", daw.masterFx.flangerMix);
    _db(f, "tapeOn", daw.masterFx.tapeOn); _dw(f, "tapeSaturation", daw.masterFx.tapeSaturation);
    _dw(f, "tapeWow", daw.masterFx.tapeWow); _dw(f, "tapeFlutter", daw.masterFx.tapeFlutter);
    _dw(f, "tapeHiss", daw.masterFx.tapeHiss);
    _db(f, "delayOn", daw.masterFx.delayOn); _dw(f, "delayTime", daw.masterFx.delayTime);
    _dw(f, "delayFeedback", daw.masterFx.delayFeedback); _dw(f, "delayTone", daw.masterFx.delayTone);
    _dw(f, "delayMix", daw.masterFx.delayMix);
    _db(f, "reverbOn", daw.masterFx.reverbOn); _dw(f, "reverbSize", daw.masterFx.reverbSize);
    _dw(f, "reverbDamping", daw.masterFx.reverbDamping); _dw(f, "reverbPreDelay", daw.masterFx.reverbPreDelay);
    _dw(f, "reverbMix", daw.masterFx.reverbMix);
    _db(f, "eqOn", daw.masterFx.eqOn); _dw(f, "eqLowGain", daw.masterFx.eqLowGain);
    _dw(f, "eqHighGain", daw.masterFx.eqHighGain); _dw(f, "eqLowFreq", daw.masterFx.eqLowFreq);
    _dw(f, "eqHighFreq", daw.masterFx.eqHighFreq);
    _db(f, "compOn", daw.masterFx.compOn); _dw(f, "compThreshold", daw.masterFx.compThreshold);
    _dw(f, "compRatio", daw.masterFx.compRatio); _dw(f, "compAttack", daw.masterFx.compAttack);
    _dw(f, "compRelease", daw.masterFx.compRelease); _dw(f, "compMakeup", daw.masterFx.compMakeup);

    // [tapefx]
    fprintf(f, "\n[tapefx]\n");
    _db(f, "enabled", daw.tapeFx.enabled);
    _dw(f, "headTime", daw.tapeFx.headTime); _dw(f, "feedback", daw.tapeFx.feedback);
    _dw(f, "mix", daw.tapeFx.mix); _di(f, "inputSource", daw.tapeFx.inputSource);
    _db(f, "preReverb", daw.tapeFx.preReverb);
    _dw(f, "saturation", daw.tapeFx.saturation); _dw(f, "toneHigh", daw.tapeFx.toneHigh);
    _dw(f, "noise", daw.tapeFx.noise); _dw(f, "degradeRate", daw.tapeFx.degradeRate);
    _dw(f, "wow", daw.tapeFx.wow); _dw(f, "flutter", daw.tapeFx.flutter);
    _dw(f, "drift", daw.tapeFx.drift); _dw(f, "speedTarget", daw.tapeFx.speedTarget);
    _dw(f, "rewindTime", daw.tapeFx.rewindTime); _dw(f, "rewindMinSpeed", daw.tapeFx.rewindMinSpeed);
    _dw(f, "rewindVinyl", daw.tapeFx.rewindVinyl); _dw(f, "rewindWobble", daw.tapeFx.rewindWobble);
    _dw(f, "rewindFilter", daw.tapeFx.rewindFilter); _di(f, "rewindCurve", daw.tapeFx.rewindCurve);

    // [crossfader]
    fprintf(f, "\n[crossfader]\n");
    _db(f, "enabled", daw.crossfader.enabled); _dw(f, "pos", daw.crossfader.pos);
    _di(f, "sceneA", daw.crossfader.sceneA); _di(f, "sceneB", daw.crossfader.sceneB);
    _di(f, "count", daw.crossfader.count);

    // [split]
    fprintf(f, "\n[split]\n");
    _db(f, "enabled", daw.splitEnabled); _di(f, "point", daw.splitPoint);
    _di(f, "leftPatch", daw.splitLeftPatch); _di(f, "rightPatch", daw.splitRightPatch);
    _di(f, "leftOctave", daw.splitLeftOctave); _di(f, "rightOctave", daw.splitRightOctave);

    // [arrangement]
    fprintf(f, "\n[arrangement]\n");
    _di(f, "length", daw.song.length);
    _di(f, "loopsPerPattern", daw.song.loopsPerPattern);
    _db(f, "songMode", daw.song.songMode);
    if (daw.song.length > 0) {
        fprintf(f, "patterns =");
        for (int i = 0; i < daw.song.length; i++) fprintf(f, " %d", daw.song.patterns[i]);
        fprintf(f, "\nloops =");
        for (int i = 0; i < daw.song.length; i++) fprintf(f, " %d", daw.song.loopsPerSection[i]);
        fprintf(f, "\n");
        for (int i = 0; i < daw.song.length; i++) {
            if (daw.song.names[i][0]) {
                char k[16]; snprintf(k, sizeof(k), "name%d", i);
                _ds(f, k, daw.song.names[i]);
            }
        }
    }

    // Patterns (only non-empty)
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) {
        const Pattern *p = &seq.patterns[i];
        bool empty = true;
        for (int t = 0; t < SEQ_DRUM_TRACKS && empty; t++)
            for (int s = 0; s < p->trackLength[t] && empty; s++)
                if (patGetDrum((Pattern*)p, t, s)) empty = false;
        for (int t = 0; t < SEQ_MELODY_TRACKS && empty; t++)
            for (int s = 0; s < p->trackLength[SEQ_DRUM_TRACKS + t] && empty; s++)
                if (patGetNote((Pattern*)p, SEQ_DRUM_TRACKS + t, s) != SEQ_NOTE_OFF) empty = false;
        if (p->plockCount > 0) empty = false;
        if (!empty) _dwWritePattern(f, i, p);
    }

    fclose(f);
    return true;
}

// ============================================================================
// LOAD HELPERS
// ============================================================================

static float _dpf(const char *v) { return (float)atof(v); }
static int _dpi(const char *v) { return atoi(v); }
static bool _dpb(const char *v) { return strcmp(v,"true")==0 || strcmp(v,"1")==0; }

static char* _dwStrip(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) s[--len]='\0';
    return s;
}

static void _dwStripQuotes(char *s) {
    int len = (int)strlen(s);
    if (len >= 2 && s[0]=='"' && s[len-1]=='"') { memmove(s, s+1, len-2); s[len-2]='\0'; }
}

static int _dwLookupWave(const char *name) {
    for (int i = 0; i < 14; i++) if (strcasecmp(name, _dwWaveNames[i])==0) return i;
    return atoi(name);
}

static void _dwApplyPatchKV(SynthPatch *p, const char *key, const char *val) {
    if (strcmp(key,"name")==0) { char t[64]; strncpy(t,val,63); t[63]=0; _dwStripQuotes(t); strncpy(p->p_name,t,31); p->p_name[31]=0; }
    else if (strcmp(key,"waveType")==0) p->p_waveType = _dwLookupWave(val);
    else if (strcmp(key,"scwIndex")==0) p->p_scwIndex = _dpi(val);
    else if (strcmp(key,"attack")==0) p->p_attack = _dpf(val);
    else if (strcmp(key,"decay")==0) p->p_decay = _dpf(val);
    else if (strcmp(key,"sustain")==0) p->p_sustain = _dpf(val);
    else if (strcmp(key,"release")==0) p->p_release = _dpf(val);
    else if (strcmp(key,"expRelease")==0) p->p_expRelease = _dpb(val);
    else if (strcmp(key,"expDecay")==0) p->p_expDecay = _dpb(val);
    else if (strcmp(key,"oneShot")==0) p->p_oneShot = _dpb(val);
    else if (strcmp(key,"volume")==0) p->p_volume = _dpf(val);
    else if (strcmp(key,"pulseWidth")==0) p->p_pulseWidth = _dpf(val);
    else if (strcmp(key,"pwmRate")==0) p->p_pwmRate = _dpf(val);
    else if (strcmp(key,"pwmDepth")==0) p->p_pwmDepth = _dpf(val);
    else if (strcmp(key,"vibratoRate")==0) p->p_vibratoRate = _dpf(val);
    else if (strcmp(key,"vibratoDepth")==0) p->p_vibratoDepth = _dpf(val);
    else if (strcmp(key,"filterCutoff")==0) p->p_filterCutoff = _dpf(val);
    else if (strcmp(key,"filterResonance")==0) p->p_filterResonance = _dpf(val);
    else if (strcmp(key,"filterType")==0) p->p_filterType = _dpi(val);
    else if (strcmp(key,"filterEnvAmt")==0) p->p_filterEnvAmt = _dpf(val);
    else if (strcmp(key,"filterEnvAttack")==0) p->p_filterEnvAttack = _dpf(val);
    else if (strcmp(key,"filterEnvDecay")==0) p->p_filterEnvDecay = _dpf(val);
    else if (strcmp(key,"filterKeyTrack")==0) p->p_filterKeyTrack = _dpf(val);
    else if (strcmp(key,"filterLfoRate")==0) p->p_filterLfoRate = _dpf(val);
    else if (strcmp(key,"filterLfoDepth")==0) p->p_filterLfoDepth = _dpf(val);
    else if (strcmp(key,"filterLfoShape")==0) p->p_filterLfoShape = _dpi(val);
    else if (strcmp(key,"filterLfoSync")==0) p->p_filterLfoSync = _dpi(val);
    else if (strcmp(key,"resoLfoRate")==0) p->p_resoLfoRate = _dpf(val);
    else if (strcmp(key,"resoLfoDepth")==0) p->p_resoLfoDepth = _dpf(val);
    else if (strcmp(key,"resoLfoShape")==0) p->p_resoLfoShape = _dpi(val);
    else if (strcmp(key,"resoLfoSync")==0) p->p_resoLfoSync = _dpi(val);
    else if (strcmp(key,"ampLfoRate")==0) p->p_ampLfoRate = _dpf(val);
    else if (strcmp(key,"ampLfoDepth")==0) p->p_ampLfoDepth = _dpf(val);
    else if (strcmp(key,"ampLfoShape")==0) p->p_ampLfoShape = _dpi(val);
    else if (strcmp(key,"ampLfoSync")==0) p->p_ampLfoSync = _dpi(val);
    else if (strcmp(key,"pitchLfoRate")==0) p->p_pitchLfoRate = _dpf(val);
    else if (strcmp(key,"pitchLfoDepth")==0) p->p_pitchLfoDepth = _dpf(val);
    else if (strcmp(key,"pitchLfoShape")==0) p->p_pitchLfoShape = _dpi(val);
    else if (strcmp(key,"pitchLfoSync")==0) p->p_pitchLfoSync = _dpi(val);
    else if (strcmp(key,"arpEnabled")==0) p->p_arpEnabled = _dpb(val);
    else if (strcmp(key,"arpMode")==0) p->p_arpMode = _dpi(val);
    else if (strcmp(key,"arpRateDiv")==0) p->p_arpRateDiv = _dpi(val);
    else if (strcmp(key,"arpRate")==0) p->p_arpRate = _dpf(val);
    else if (strcmp(key,"arpChord")==0) p->p_arpChord = _dpi(val);
    else if (strcmp(key,"unisonCount")==0) p->p_unisonCount = _dpi(val);
    else if (strcmp(key,"unisonDetune")==0) p->p_unisonDetune = _dpf(val);
    else if (strcmp(key,"unisonMix")==0) p->p_unisonMix = _dpf(val);
    else if (strcmp(key,"monoMode")==0) p->p_monoMode = _dpb(val);
    else if (strcmp(key,"glideTime")==0) p->p_glideTime = _dpf(val);
    else if (strcmp(key,"pluckBrightness")==0) p->p_pluckBrightness = _dpf(val);
    else if (strcmp(key,"pluckDamping")==0) p->p_pluckDamping = _dpf(val);
    else if (strcmp(key,"pluckDamp")==0) p->p_pluckDamp = _dpf(val);
    else if (strcmp(key,"additivePreset")==0) p->p_additivePreset = _dpi(val);
    else if (strcmp(key,"additiveBrightness")==0) p->p_additiveBrightness = _dpf(val);
    else if (strcmp(key,"additiveShimmer")==0) p->p_additiveShimmer = _dpf(val);
    else if (strcmp(key,"additiveInharmonicity")==0) p->p_additiveInharmonicity = _dpf(val);
    else if (strcmp(key,"malletPreset")==0) p->p_malletPreset = _dpi(val);
    else if (strcmp(key,"malletStiffness")==0) p->p_malletStiffness = _dpf(val);
    else if (strcmp(key,"malletHardness")==0) p->p_malletHardness = _dpf(val);
    else if (strcmp(key,"malletStrikePos")==0) p->p_malletStrikePos = _dpf(val);
    else if (strcmp(key,"malletResonance")==0) p->p_malletResonance = _dpf(val);
    else if (strcmp(key,"malletTremolo")==0) p->p_malletTremolo = _dpf(val);
    else if (strcmp(key,"malletTremoloRate")==0) p->p_malletTremoloRate = _dpf(val);
    else if (strcmp(key,"malletDamp")==0) p->p_malletDamp = _dpf(val);
    else if (strcmp(key,"voiceVowel")==0) p->p_voiceVowel = _dpi(val);
    else if (strcmp(key,"voiceFormantShift")==0) p->p_voiceFormantShift = _dpf(val);
    else if (strcmp(key,"voiceBreathiness")==0) p->p_voiceBreathiness = _dpf(val);
    else if (strcmp(key,"voiceBuzziness")==0) p->p_voiceBuzziness = _dpf(val);
    else if (strcmp(key,"voiceSpeed")==0) p->p_voiceSpeed = _dpf(val);
    else if (strcmp(key,"voicePitch")==0) p->p_voicePitch = _dpf(val);
    else if (strcmp(key,"voiceConsonant")==0) p->p_voiceConsonant = _dpb(val);
    else if (strcmp(key,"voiceConsonantAmt")==0) p->p_voiceConsonantAmt = _dpf(val);
    else if (strcmp(key,"voiceNasal")==0) p->p_voiceNasal = _dpb(val);
    else if (strcmp(key,"voiceNasalAmt")==0) p->p_voiceNasalAmt = _dpf(val);
    else if (strcmp(key,"voicePitchEnv")==0) p->p_voicePitchEnv = _dpf(val);
    else if (strcmp(key,"voicePitchEnvTime")==0) p->p_voicePitchEnvTime = _dpf(val);
    else if (strcmp(key,"voicePitchEnvCurve")==0) p->p_voicePitchEnvCurve = _dpf(val);
    else if (strcmp(key,"granularScwIndex")==0) p->p_granularScwIndex = _dpi(val);
    else if (strcmp(key,"granularGrainSize")==0) p->p_granularGrainSize = _dpf(val);
    else if (strcmp(key,"granularDensity")==0) p->p_granularDensity = _dpf(val);
    else if (strcmp(key,"granularPosition")==0) p->p_granularPosition = _dpf(val);
    else if (strcmp(key,"granularPosRandom")==0) p->p_granularPosRandom = _dpf(val);
    else if (strcmp(key,"granularPitch")==0) p->p_granularPitch = _dpf(val);
    else if (strcmp(key,"granularPitchRandom")==0) p->p_granularPitchRandom = _dpf(val);
    else if (strcmp(key,"granularAmpRandom")==0) p->p_granularAmpRandom = _dpf(val);
    else if (strcmp(key,"granularSpread")==0) p->p_granularSpread = _dpf(val);
    else if (strcmp(key,"granularFreeze")==0) p->p_granularFreeze = _dpb(val);
    else if (strcmp(key,"fmModRatio")==0) p->p_fmModRatio = _dpf(val);
    else if (strcmp(key,"fmModIndex")==0) p->p_fmModIndex = _dpf(val);
    else if (strcmp(key,"fmFeedback")==0) p->p_fmFeedback = _dpf(val);
    else if (strcmp(key,"fmMod2Ratio")==0) p->p_fmMod2Ratio = _dpf(val);
    else if (strcmp(key,"fmMod2Index")==0) p->p_fmMod2Index = _dpf(val);
    else if (strcmp(key,"fmAlgorithm")==0) p->p_fmAlgorithm = _dpi(val);
    else if (strcmp(key,"pdWaveType")==0) p->p_pdWaveType = _dpi(val);
    else if (strcmp(key,"pdDistortion")==0) p->p_pdDistortion = _dpf(val);
    else if (strcmp(key,"membranePreset")==0) p->p_membranePreset = _dpi(val);
    else if (strcmp(key,"membraneDamping")==0) p->p_membraneDamping = _dpf(val);
    else if (strcmp(key,"membraneStrike")==0) p->p_membraneStrike = _dpf(val);
    else if (strcmp(key,"membraneBend")==0) p->p_membraneBend = _dpf(val);
    else if (strcmp(key,"membraneBendDecay")==0) p->p_membraneBendDecay = _dpf(val);
    else if (strcmp(key,"bowPressure")==0) p->p_bowPressure = _dpf(val);
    else if (strcmp(key,"bowSpeed")==0) p->p_bowSpeed = _dpf(val);
    else if (strcmp(key,"bowPosition")==0) p->p_bowPosition = _dpf(val);
    else if (strcmp(key,"pipeBreath")==0) p->p_pipeBreath = _dpf(val);
    else if (strcmp(key,"pipeEmbouchure")==0) p->p_pipeEmbouchure = _dpf(val);
    else if (strcmp(key,"pipeBore")==0) p->p_pipeBore = _dpf(val);
    else if (strcmp(key,"pipeOverblow")==0) p->p_pipeOverblow = _dpf(val);
    else if (strcmp(key,"birdType")==0) p->p_birdType = _dpi(val);
    else if (strcmp(key,"birdChirpRange")==0) p->p_birdChirpRange = _dpf(val);
    else if (strcmp(key,"birdTrillRate")==0) p->p_birdTrillRate = _dpf(val);
    else if (strcmp(key,"birdTrillDepth")==0) p->p_birdTrillDepth = _dpf(val);
    else if (strcmp(key,"birdAmRate")==0) p->p_birdAmRate = _dpf(val);
    else if (strcmp(key,"birdAmDepth")==0) p->p_birdAmDepth = _dpf(val);
    else if (strcmp(key,"birdHarmonics")==0) p->p_birdHarmonics = _dpf(val);
    else if (strcmp(key,"pitchEnvAmount")==0) p->p_pitchEnvAmount = _dpf(val);
    else if (strcmp(key,"pitchEnvDecay")==0) p->p_pitchEnvDecay = _dpf(val);
    else if (strcmp(key,"pitchEnvCurve")==0) p->p_pitchEnvCurve = _dpf(val);
    else if (strcmp(key,"pitchEnvLinear")==0) p->p_pitchEnvLinear = _dpb(val);
    else if (strcmp(key,"noiseMix")==0) p->p_noiseMix = _dpf(val);
    else if (strcmp(key,"noiseTone")==0) p->p_noiseTone = _dpf(val);
    else if (strcmp(key,"noiseHP")==0) p->p_noiseHP = _dpf(val);
    else if (strcmp(key,"noiseDecay")==0) p->p_noiseDecay = _dpf(val);
    else if (strcmp(key,"retriggerCount")==0) p->p_retriggerCount = _dpi(val);
    else if (strcmp(key,"retriggerSpread")==0) p->p_retriggerSpread = _dpf(val);
    else if (strcmp(key,"retriggerOverlap")==0) p->p_retriggerOverlap = _dpb(val);
    else if (strcmp(key,"retriggerBurstDecay")==0) p->p_retriggerBurstDecay = _dpf(val);
    else if (strcmp(key,"osc2Ratio")==0) p->p_osc2Ratio = _dpf(val);
    else if (strcmp(key,"osc2Level")==0) p->p_osc2Level = _dpf(val);
    else if (strcmp(key,"osc3Ratio")==0) p->p_osc3Ratio = _dpf(val);
    else if (strcmp(key,"osc3Level")==0) p->p_osc3Level = _dpf(val);
    else if (strcmp(key,"osc4Ratio")==0) p->p_osc4Ratio = _dpf(val);
    else if (strcmp(key,"osc4Level")==0) p->p_osc4Level = _dpf(val);
    else if (strcmp(key,"osc5Ratio")==0) p->p_osc5Ratio = _dpf(val);
    else if (strcmp(key,"osc5Level")==0) p->p_osc5Level = _dpf(val);
    else if (strcmp(key,"osc6Ratio")==0) p->p_osc6Ratio = _dpf(val);
    else if (strcmp(key,"osc6Level")==0) p->p_osc6Level = _dpf(val);
    else if (strcmp(key,"drive")==0) p->p_drive = _dpf(val);
    else if (strcmp(key,"clickLevel")==0) p->p_clickLevel = _dpf(val);
    else if (strcmp(key,"clickTime")==0) p->p_clickTime = _dpf(val);
    else if (strcmp(key,"noiseMode")==0) p->p_noiseMode = _dpi(val);
    else if (strcmp(key,"oscMixMode")==0) p->p_oscMixMode = _dpi(val);
    else if (strcmp(key,"retriggerCurve")==0) p->p_retriggerCurve = _dpf(val);
    else if (strcmp(key,"phaseReset")==0) p->p_phaseReset = _dpb(val);
    else if (strcmp(key,"noiseLPBypass")==0) p->p_noiseLPBypass = _dpb(val);
    else if (strcmp(key,"noiseType")==0) p->p_noiseType = _dpi(val);
    else if (strcmp(key,"useTriggerFreq")==0) p->p_useTriggerFreq = _dpb(val);
    else if (strcmp(key,"triggerFreq")==0) p->p_triggerFreq = _dpf(val);
    else if (strcmp(key,"choke")==0) p->p_choke = _dpb(val);
    else if (strcmp(key,"analogRolloff")==0) p->p_analogRolloff = _dpb(val);
    else if (strcmp(key,"tubeSaturation")==0) p->p_tubeSaturation = _dpb(val);
    else if (strcmp(key,"ringMod")==0) p->p_ringMod = _dpb(val);
    else if (strcmp(key,"ringModFreq")==0) p->p_ringModFreq = _dpf(val);
    else if (strcmp(key,"wavefoldAmount")==0) p->p_wavefoldAmount = _dpf(val);
    else if (strcmp(key,"hardSync")==0) p->p_hardSync = _dpb(val);
    else if (strcmp(key,"hardSyncRatio")==0) p->p_hardSyncRatio = _dpf(val);
    // 303 acid mode
    else if (strcmp(key,"acidMode")==0) p->p_acidMode = _dpb(val);
    else if (strcmp(key,"accentSweepAmt")==0) p->p_accentSweepAmt = _dpf(val);
    else if (strcmp(key,"gimmickDipAmt")==0) p->p_gimmickDipAmt = _dpf(val);
}

static int _dwParseIntList(const char *val, int *out, int maxCount) {
    char buf[256]; strncpy(buf, val, 255); buf[255]='\0';
    int count = 0;
    char *tok = strtok(buf, " ,\t");
    while (tok && count < maxCount) { out[count++] = atoi(tok); tok = strtok(NULL, " ,\t"); }
    return count;
}

static int _dwNameToMidi(const char *name) {
    if (!name || !name[0] || strcmp(name,"REST")==0 || strcmp(name,"-")==0) return SEQ_NOTE_OFF;
    if (isdigit(name[0]) || (name[0]=='-' && isdigit(name[1]))) return atoi(name);
    int note = -1, pos = 0;
    switch (toupper(name[0])) {
        case 'C': note=0; break; case 'D': note=2; break; case 'E': note=4; break;
        case 'F': note=5; break; case 'G': note=7; break; case 'A': note=9; break;
        case 'B': note=11; break; default: return SEQ_NOTE_OFF;
    }
    pos = 1;
    if (name[pos]=='#') { note++; pos++; } else if (name[pos]=='b') { note--; pos++; }
    if (note < 0) note += 12; if (note > 11) note -= 12;
    int oct = name[pos] ? atoi(&name[pos]) : 4;
    return (oct + 1) * 12 + note;
}

static int _dwTokenize(char *line, char **tokens, int max) {
    int count = 0; char *p = line;
    while (*p && count < max) {
        while (*p==' '||*p=='\t') p++;
        if (!*p || *p=='#') break;
        if (*p=='"') { p++; tokens[count++]=p; while (*p&&*p!='"') p++; if (*p=='"') *p++='\0'; }
        else { tokens[count++]=p; while (*p&&*p!=' '&&*p!='\t') p++; if (*p) *p++='\0'; }
    }
    return count;
}

static int _dwParseKV(const char *tok, char *k, int ks, char *v, int vs) {
    const char *eq = strchr(tok, '=');
    if (!eq) { strncpy(k,tok,ks-1); k[ks-1]='\0'; v[0]='\0'; return 0; }
    int kl = (int)(eq-tok); if (kl>=ks) kl=ks-1;
    memcpy(k,tok,kl); k[kl]='\0'; strncpy(v,eq+1,vs-1); v[vs-1]='\0'; return 1;
}

static int _dwLookupName(const char *name, const char **tbl, int cnt) {
    for (int i = 0; i < cnt; i++) if (strcasecmp(name,tbl[i])==0) return i;
    return -1;
}

static void _dwParseDrumEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[32]; int n = _dwTokenize(lc+1, toks, 32);
    int track=0, step=0; float vel=0.8f, pitch=0.0f, prob=1.0f; int cond=COND_ALWAYS;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"track")==0) track=_dpi(v);
            else if (strcmp(k,"step")==0) step=_dpi(v);
            else if (strcmp(k,"vel")==0) vel=_dpf(v);
            else if (strcmp(k,"pitch")==0) pitch=_dpf(v);
            else if (strcmp(k,"prob")==0) prob=_dpf(v);
            else if (strcmp(k,"cond")==0) { int idx=_dwLookupName(v,_dwCondNames,11); if(idx>=0) cond=idx; }
        }
    }
    if (track>=0&&track<SEQ_DRUM_TRACKS&&step>=0&&step<SEQ_MAX_STEPS) {
        patSetDrum(p, track, step, vel, pitch);
        patSetDrumProb(p, track, step, prob);
        patSetDrumCond(p, track, step, cond);
    }
}

static void _dwParseMelodyEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[32]; int n = _dwTokenize(lc+1, toks, 32);
    int track=0, step=0, gate=4, sustain=0; char noteName[16]="C4";
    int nudge=0, gateNudge=0;
    float vel=0.8f, prob=1.0f; int cond=COND_ALWAYS;
    int pickMode=PICK_ALL;
    bool slide=false, accent=false, hasChord=false;
    int chordType=CHORD_SINGLE;
    int customNotes[8]={0}; int customNoteCount=0;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"track")==0) track=_dpi(v);
            else if (strcmp(k,"step")==0) step=_dpi(v);
            else if (strcmp(k,"note")==0) strncpy(noteName,v,15);
            else if (strcmp(k,"vel")==0) vel=_dpf(v);
            else if (strcmp(k,"gate")==0) gate=_dpi(v);
            else if (strcmp(k,"sustain")==0) sustain=_dpi(v);
            else if (strcmp(k,"nudge")==0) nudge=_dpi(v);
            else if (strcmp(k,"gateNudge")==0) gateNudge=_dpi(v);
            else if (strcmp(k,"prob")==0) prob=_dpf(v);
            else if (strcmp(k,"cond")==0) { int idx=_dwLookupName(v,_dwCondNames,11); if(idx>=0) cond=idx; }
            else if (strcmp(k,"chord")==0) { hasChord=true; int idx=_dwLookupName(v,_dwChordNames,9); if(idx>=0) chordType=idx; }
            else if (strcmp(k,"pick")==0) { int pv=_dpi(v); if(pv>=0&&pv<PICK_COUNT) pickMode=pv; }
            else if (strcmp(k,"notes")==0) {
                hasChord=true; chordType=CHORD_CUSTOM;
                char nb[128]; strncpy(nb,v,127); nb[127]='\0';
                char *t2=strtok(nb,",");
                while (t2 && customNoteCount<8) { customNotes[customNoteCount++]=_dwNameToMidi(t2); t2=strtok(NULL,","); }
            }
        } else {
            if (strcmp(k,"slide")==0) slide=true;
            else if (strcmp(k,"accent")==0) accent=true;
        }
    }
    if (track>=0&&track<SEQ_MELODY_TRACKS&&step>=0&&step<SEQ_MAX_STEPS) {
        int absTrack = SEQ_DRUM_TRACKS + track;
        if (hasChord && chordType==CHORD_CUSTOM && customNoteCount>0) {
            patSetChordCustom(p, absTrack, step, vel, gate,
                customNoteCount>0?customNotes[0]:-1, customNoteCount>1?customNotes[1]:-1,
                customNoteCount>2?customNotes[2]:-1, customNoteCount>3?customNotes[3]:-1);
        } else if (hasChord) {
            patSetChord(p, absTrack, step, _dwNameToMidi(noteName), (ChordType)chordType, vel, gate);
        } else {
            // v2 polyphony: if step already has notes, add to it
            StepV2 *sv = &p->steps[absTrack][step];
            if (sv->noteCount > 0) {
                int vi = stepV2AddNote(sv, _dwNameToMidi(noteName), velFloatToU8(vel), (int8_t)gate);
                if (vi >= 0) {
                    sv->notes[vi].slide = slide;
                    sv->notes[vi].accent = accent;
                    sv->notes[vi].nudge = (int8_t)nudge;
                    sv->notes[vi].gateNudge = (int8_t)gateNudge;
                }
                return;
            }
            patSetNote(p, absTrack, step, _dwNameToMidi(noteName), vel, gate);
        }
        StepV2 *sv = &p->steps[absTrack][step];
        if (sv->noteCount > 0) {
            sv->notes[0].slide = slide;
            sv->notes[0].accent = accent;
            sv->notes[0].nudge = (int8_t)nudge;
            sv->notes[0].gateNudge = (int8_t)gateNudge;
        }
        patSetNoteSustain(p, absTrack, step, sustain);
        patSetNoteProb(p, absTrack, step, prob);
        patSetNoteCond(p, absTrack, step, cond);
        if (pickMode != PICK_ALL) patSetPickMode(p, absTrack, step, pickMode);
    }
}

static void _dwParsePlockEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[16]; int n = _dwTokenize(lc+1, toks, 16);
    int track=0, step=0, param=-1; float value=0.0f;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"track")==0) track=_dpi(v);
            else if (strcmp(k,"step")==0) step=_dpi(v);
            else if (strcmp(k,"param")==0) { int idx=_dwLookupName(v,_dwPlockNames,PLOCK_COUNT); param=idx>=0?idx:_dpi(v); }
            else if (strcmp(k,"val")==0) value=_dpf(v);
        }
    }
    if (param>=0 && param<PLOCK_COUNT) seqSetPLock(p, track, step, param, value);
}

// ============================================================================
// dawLoad
// ============================================================================

typedef enum {
    _DW_SEC_NONE, _DW_SEC_SONG, _DW_SEC_SCALE, _DW_SEC_GROOVE,
    _DW_SEC_SETTINGS, _DW_SEC_PATCH, _DW_SEC_MIXER, _DW_SEC_SIDECHAIN,
    _DW_SEC_MASTERFX, _DW_SEC_TAPEFX, _DW_SEC_CROSSFADER,
    _DW_SEC_SPLIT, _DW_SEC_ARRANGEMENT, _DW_SEC_PATTERN,
} _DwSection;

static bool dawLoad(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    // Stop playback
    daw.transport.playing = false;

    // Reset patterns
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) initPattern(&seq.patterns[i]);

    _DwSection section = _DW_SEC_NONE;
    int subIndex = -1;
    char line[512];

    while (fgets(line, 512, f)) {
        char *s = _dwStrip(line);
        if (!*s || *s == '#') continue;

        // Section header
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end) continue;
            *end = '\0';
            char *sec = s + 1;
            if (strcmp(sec,"song")==0) section = _DW_SEC_SONG;
            else if (strcmp(sec,"scale")==0) section = _DW_SEC_SCALE;
            else if (strcmp(sec,"groove")==0) section = _DW_SEC_GROOVE;
            else if (strcmp(sec,"settings")==0) section = _DW_SEC_SETTINGS;
            else if (strcmp(sec,"mixer")==0) section = _DW_SEC_MIXER;
            else if (strcmp(sec,"sidechain")==0) section = _DW_SEC_SIDECHAIN;
            else if (strcmp(sec,"masterfx")==0) section = _DW_SEC_MASTERFX;
            else if (strcmp(sec,"tapefx")==0) section = _DW_SEC_TAPEFX;
            else if (strcmp(sec,"crossfader")==0) section = _DW_SEC_CROSSFADER;
            else if (strcmp(sec,"split")==0) section = _DW_SEC_SPLIT;
            else if (strcmp(sec,"arrangement")==0) section = _DW_SEC_ARRANGEMENT;
            else if (strncmp(sec,"patch.",6)==0) {
                section = _DW_SEC_PATCH;
                subIndex = atoi(sec + 6);
                if (subIndex >= 0 && subIndex < NUM_PATCHES)
                    daw.patches[subIndex] = createDefaultPatch(WAVE_SAW);
            }
            else if (strncmp(sec,"pattern.",8)==0) {
                section = _DW_SEC_PATTERN;
                subIndex = atoi(sec + 8);
                if (subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS)
                    initPattern(&seq.patterns[subIndex]);
            }
            else section = _DW_SEC_NONE;
            continue;
        }

        // Pattern events
        if (section == _DW_SEC_PATTERN && subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
            if (s[0]=='d' && s[1]==' ') { _dwParseDrumEvent(s, &seq.patterns[subIndex]); continue; }
            if (s[0]=='m' && s[1]==' ') { _dwParseMelodyEvent(s, &seq.patterns[subIndex]); continue; }
            if (s[0]=='p' && s[1]==' ') { _dwParsePlockEvent(s, &seq.patterns[subIndex]); continue; }
        }

        // Key = value
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = _dwStrip(s);
        char *val = _dwStrip(eq + 1);
        _dwStripQuotes(val);

        switch (section) {
        case _DW_SEC_SONG:
            if (strcmp(key,"bpm")==0) daw.transport.bpm = _dpf(val);
            else if (strcmp(key,"stepCount")==0) daw.stepCount = _dpi(val);
            else if (strcmp(key,"grooveSwing")==0) daw.transport.grooveSwing = _dpi(val);
            else if (strcmp(key,"grooveJitter")==0) daw.transport.grooveJitter = _dpi(val);
            break;
        case _DW_SEC_SCALE:
            if (strcmp(key,"enabled")==0) daw.scaleLockEnabled = _dpb(val);
            else if (strcmp(key,"root")==0) daw.scaleRoot = _dpi(val);
            else if (strcmp(key,"type")==0) daw.scaleType = _dpi(val);
            break;
        case _DW_SEC_GROOVE:
            if (strcmp(key,"kickNudge")==0) seq.dilla.kickNudge = _dpi(val);
            else if (strcmp(key,"snareDelay")==0) seq.dilla.snareDelay = _dpi(val);
            else if (strcmp(key,"hatNudge")==0) seq.dilla.hatNudge = _dpi(val);
            else if (strcmp(key,"clapDelay")==0) seq.dilla.clapDelay = _dpi(val);
            else if (strcmp(key,"swing")==0) seq.dilla.swing = _dpi(val);
            else if (strcmp(key,"jitter")==0) seq.dilla.jitter = _dpi(val);
            else if (strcmp(key,"melodyTimingJitter")==0) seq.humanize.timingJitter = _dpi(val);
            else if (strcmp(key,"melodyVelocityJitter")==0) seq.humanize.velocityJitter = _dpf(val);
            break;
        case _DW_SEC_SETTINGS:
            if (strcmp(key,"masterVol")==0) daw.masterVol = _dpf(val);
            else if (strcmp(key,"voiceRandomVowel")==0) daw.voiceRandomVowel = _dpb(val);
            break;
        case _DW_SEC_PATCH:
            if (subIndex >= 0 && subIndex < NUM_PATCHES)
                _dwApplyPatchKV(&daw.patches[subIndex], key, val);
            break;
        case _DW_SEC_MIXER: {
            int len = (int)strlen(key);
            if (len > 1 && isdigit(key[len-1])) {
                int b = key[len-1] - '0';
                if (b >= 0 && b < NUM_BUSES) {
                    char base[32]; strncpy(base,key,len-1); base[len-1]='\0';
                    if (strcmp(base,"vol")==0) daw.mixer.volume[b]=_dpf(val);
                    else if (strcmp(base,"pan")==0) daw.mixer.pan[b]=_dpf(val);
                    else if (strcmp(base,"reverbSend")==0) daw.mixer.reverbSend[b]=_dpf(val);
                    else if (strcmp(base,"mute")==0) daw.mixer.mute[b]=_dpb(val);
                    else if (strcmp(base,"solo")==0) daw.mixer.solo[b]=_dpb(val);
                    else if (strcmp(base,"filterOn")==0) daw.mixer.filterOn[b]=_dpb(val);
                    else if (strcmp(base,"filterCut")==0) daw.mixer.filterCut[b]=_dpf(val);
                    else if (strcmp(base,"filterRes")==0) daw.mixer.filterRes[b]=_dpf(val);
                    else if (strcmp(base,"filterType")==0) daw.mixer.filterType[b]=_dpi(val);
                    else if (strcmp(base,"distOn")==0) daw.mixer.distOn[b]=_dpb(val);
                    else if (strcmp(base,"distDrive")==0) daw.mixer.distDrive[b]=_dpf(val);
                    else if (strcmp(base,"distMix")==0) daw.mixer.distMix[b]=_dpf(val);
                    else if (strcmp(base,"delayOn")==0) daw.mixer.delayOn[b]=_dpb(val);
                    else if (strcmp(base,"delaySync")==0) daw.mixer.delaySync[b]=_dpb(val);
                    else if (strcmp(base,"delaySyncDiv")==0) daw.mixer.delaySyncDiv[b]=_dpi(val);
                    else if (strcmp(base,"delayTime")==0) daw.mixer.delayTime[b]=_dpf(val);
                    else if (strcmp(base,"delayFB")==0) daw.mixer.delayFB[b]=_dpf(val);
                    else if (strcmp(base,"delayMix")==0) daw.mixer.delayMix[b]=_dpf(val);
                }
            }
            break;
        }
        case _DW_SEC_SIDECHAIN:
            if (strcmp(key,"on")==0) daw.sidechain.on=_dpb(val);
            else if (strcmp(key,"source")==0) daw.sidechain.source=_dpi(val);
            else if (strcmp(key,"target")==0) daw.sidechain.target=_dpi(val);
            else if (strcmp(key,"depth")==0) daw.sidechain.depth=_dpf(val);
            else if (strcmp(key,"attack")==0) daw.sidechain.attack=_dpf(val);
            else if (strcmp(key,"release")==0) daw.sidechain.release=_dpf(val);
            break;
        case _DW_SEC_MASTERFX:
            if (strcmp(key,"distOn")==0) daw.masterFx.distOn=_dpb(val);
            else if (strcmp(key,"distDrive")==0) daw.masterFx.distDrive=_dpf(val);
            else if (strcmp(key,"distTone")==0) daw.masterFx.distTone=_dpf(val);
            else if (strcmp(key,"distMix")==0) daw.masterFx.distMix=_dpf(val);
            else if (strcmp(key,"crushOn")==0) daw.masterFx.crushOn=_dpb(val);
            else if (strcmp(key,"crushBits")==0) daw.masterFx.crushBits=_dpf(val);
            else if (strcmp(key,"crushRate")==0) daw.masterFx.crushRate=_dpf(val);
            else if (strcmp(key,"crushMix")==0) daw.masterFx.crushMix=_dpf(val);
            else if (strcmp(key,"chorusOn")==0) daw.masterFx.chorusOn=_dpb(val);
            else if (strcmp(key,"chorusRate")==0) daw.masterFx.chorusRate=_dpf(val);
            else if (strcmp(key,"chorusDepth")==0) daw.masterFx.chorusDepth=_dpf(val);
            else if (strcmp(key,"chorusMix")==0) daw.masterFx.chorusMix=_dpf(val);
            else if (strcmp(key,"flangerOn")==0) daw.masterFx.flangerOn=_dpb(val);
            else if (strcmp(key,"flangerRate")==0) daw.masterFx.flangerRate=_dpf(val);
            else if (strcmp(key,"flangerDepth")==0) daw.masterFx.flangerDepth=_dpf(val);
            else if (strcmp(key,"flangerFeedback")==0) daw.masterFx.flangerFeedback=_dpf(val);
            else if (strcmp(key,"flangerMix")==0) daw.masterFx.flangerMix=_dpf(val);
            else if (strcmp(key,"tapeOn")==0) daw.masterFx.tapeOn=_dpb(val);
            else if (strcmp(key,"tapeSaturation")==0) daw.masterFx.tapeSaturation=_dpf(val);
            else if (strcmp(key,"tapeWow")==0) daw.masterFx.tapeWow=_dpf(val);
            else if (strcmp(key,"tapeFlutter")==0) daw.masterFx.tapeFlutter=_dpf(val);
            else if (strcmp(key,"tapeHiss")==0) daw.masterFx.tapeHiss=_dpf(val);
            else if (strcmp(key,"delayOn")==0) daw.masterFx.delayOn=_dpb(val);
            else if (strcmp(key,"delayTime")==0) daw.masterFx.delayTime=_dpf(val);
            else if (strcmp(key,"delayFeedback")==0) daw.masterFx.delayFeedback=_dpf(val);
            else if (strcmp(key,"delayTone")==0) daw.masterFx.delayTone=_dpf(val);
            else if (strcmp(key,"delayMix")==0) daw.masterFx.delayMix=_dpf(val);
            else if (strcmp(key,"reverbOn")==0) daw.masterFx.reverbOn=_dpb(val);
            else if (strcmp(key,"reverbSize")==0) daw.masterFx.reverbSize=_dpf(val);
            else if (strcmp(key,"reverbDamping")==0) daw.masterFx.reverbDamping=_dpf(val);
            else if (strcmp(key,"reverbPreDelay")==0) daw.masterFx.reverbPreDelay=_dpf(val);
            else if (strcmp(key,"reverbMix")==0) daw.masterFx.reverbMix=_dpf(val);
            else if (strcmp(key,"eqOn")==0) daw.masterFx.eqOn=_dpb(val);
            else if (strcmp(key,"eqLowGain")==0) daw.masterFx.eqLowGain=_dpf(val);
            else if (strcmp(key,"eqHighGain")==0) daw.masterFx.eqHighGain=_dpf(val);
            else if (strcmp(key,"eqLowFreq")==0) daw.masterFx.eqLowFreq=_dpf(val);
            else if (strcmp(key,"eqHighFreq")==0) daw.masterFx.eqHighFreq=_dpf(val);
            else if (strcmp(key,"compOn")==0) daw.masterFx.compOn=_dpb(val);
            else if (strcmp(key,"compThreshold")==0) daw.masterFx.compThreshold=_dpf(val);
            else if (strcmp(key,"compRatio")==0) daw.masterFx.compRatio=_dpf(val);
            else if (strcmp(key,"compAttack")==0) daw.masterFx.compAttack=_dpf(val);
            else if (strcmp(key,"compRelease")==0) daw.masterFx.compRelease=_dpf(val);
            else if (strcmp(key,"compMakeup")==0) daw.masterFx.compMakeup=_dpf(val);
            break;
        case _DW_SEC_TAPEFX:
            if (strcmp(key,"enabled")==0) daw.tapeFx.enabled=_dpb(val);
            else if (strcmp(key,"headTime")==0) daw.tapeFx.headTime=_dpf(val);
            else if (strcmp(key,"feedback")==0) daw.tapeFx.feedback=_dpf(val);
            else if (strcmp(key,"mix")==0) daw.tapeFx.mix=_dpf(val);
            else if (strcmp(key,"inputSource")==0) daw.tapeFx.inputSource=_dpi(val);
            else if (strcmp(key,"preReverb")==0) daw.tapeFx.preReverb=_dpb(val);
            else if (strcmp(key,"saturation")==0) daw.tapeFx.saturation=_dpf(val);
            else if (strcmp(key,"toneHigh")==0) daw.tapeFx.toneHigh=_dpf(val);
            else if (strcmp(key,"noise")==0) daw.tapeFx.noise=_dpf(val);
            else if (strcmp(key,"degradeRate")==0) daw.tapeFx.degradeRate=_dpf(val);
            else if (strcmp(key,"wow")==0) daw.tapeFx.wow=_dpf(val);
            else if (strcmp(key,"flutter")==0) daw.tapeFx.flutter=_dpf(val);
            else if (strcmp(key,"drift")==0) daw.tapeFx.drift=_dpf(val);
            else if (strcmp(key,"speedTarget")==0) daw.tapeFx.speedTarget=_dpf(val);
            else if (strcmp(key,"rewindTime")==0) daw.tapeFx.rewindTime=_dpf(val);
            else if (strcmp(key,"rewindMinSpeed")==0) daw.tapeFx.rewindMinSpeed=_dpf(val);
            else if (strcmp(key,"rewindVinyl")==0) daw.tapeFx.rewindVinyl=_dpf(val);
            else if (strcmp(key,"rewindWobble")==0) daw.tapeFx.rewindWobble=_dpf(val);
            else if (strcmp(key,"rewindFilter")==0) daw.tapeFx.rewindFilter=_dpf(val);
            else if (strcmp(key,"rewindCurve")==0) daw.tapeFx.rewindCurve=_dpi(val);
            break;
        case _DW_SEC_CROSSFADER:
            if (strcmp(key,"enabled")==0) daw.crossfader.enabled=_dpb(val);
            else if (strcmp(key,"pos")==0) daw.crossfader.pos=_dpf(val);
            else if (strcmp(key,"sceneA")==0) daw.crossfader.sceneA=_dpi(val);
            else if (strcmp(key,"sceneB")==0) daw.crossfader.sceneB=_dpi(val);
            else if (strcmp(key,"count")==0) daw.crossfader.count=_dpi(val);
            break;
        case _DW_SEC_SPLIT:
            if (strcmp(key,"enabled")==0) daw.splitEnabled=_dpb(val);
            else if (strcmp(key,"point")==0) daw.splitPoint=_dpi(val);
            else if (strcmp(key,"leftPatch")==0) daw.splitLeftPatch=_dpi(val);
            else if (strcmp(key,"rightPatch")==0) daw.splitRightPatch=_dpi(val);
            else if (strcmp(key,"leftOctave")==0) daw.splitLeftOctave=_dpi(val);
            else if (strcmp(key,"rightOctave")==0) daw.splitRightOctave=_dpi(val);
            break;
        case _DW_SEC_ARRANGEMENT:
            if (strcmp(key,"length")==0) daw.song.length=_dpi(val);
            else if (strcmp(key,"loopsPerPattern")==0) daw.song.loopsPerPattern=_dpi(val);
            else if (strcmp(key,"songMode")==0) daw.song.songMode=_dpb(val);
            else if (strcmp(key,"patterns")==0) _dwParseIntList(val, daw.song.patterns, SONG_MAX_SECTIONS);
            else if (strcmp(key,"loops")==0) _dwParseIntList(val, daw.song.loopsPerSection, SONG_MAX_SECTIONS);
            else if (strncmp(key,"name",4)==0 && isdigit(key[4])) {
                int idx = atoi(key+4);
                if (idx>=0 && idx<SONG_MAX_SECTIONS) strncpy(daw.song.names[idx], val, SONG_SECTION_NAME_LEN-1);
            }
            break;
        case _DW_SEC_PATTERN:
            if (subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
                Pattern *p = &seq.patterns[subIndex];
                if (strcmp(key,"drumTrackLength")==0) {
                    _dwParseIntList(val, p->trackLength, SEQ_DRUM_TRACKS);
                }
                else if (strcmp(key,"melodyTrackLength")==0) {
                    int melLengths[SEQ_MELODY_TRACKS];
                    _dwParseIntList(val, melLengths, SEQ_MELODY_TRACKS);
                    for (int _t = 0; _t < SEQ_MELODY_TRACKS; _t++)
                        p->trackLength[SEQ_DRUM_TRACKS + _t] = melLengths[_t];
                }
            }
            break;
        default: break;
        }
    }

    fclose(f);

    // Reset transport to start
    daw.transport.currentStep = 0;
    daw.transport.stepAccumulator = 0;
    daw.transport.currentPattern = 0;
    seq.currentPattern = 0;

    // Reset preset tracking (loaded patches are custom)
    for (int i = 0; i < NUM_PATCHES; i++) patchPresetIndex[i] = -1;

    return true;
}

#endif // _DAW_FILE_SAVE_LOAD

#endif // DAW_FILE_H
