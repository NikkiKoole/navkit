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

// Shared file I/O helpers
#include "../engines/file_helpers.h"

// Write aliases
#define _dw fileWriteFloat
#define _di fileWriteInt
#define _db fileWriteBool
#define _ds fileWriteStr

// Use canonical waveTypeNames[] from synth.h
// (extended from 14 to 16 to include "bowed" and "pipe")
#define _dwWaveNames waveTypeNames
#define _dwWaveCount waveTypeCount

static void _dwWritePatch(FILE *f, const char *sec, const SynthPatch *p) {
    fprintf(f, "\n[%s]\n", sec);
    if (p->p_name[0]) _ds(f, "name", p->p_name);
    if (p->p_waveType >= 0 && p->p_waveType < _dwWaveCount) fprintf(f, "waveType = %s\n", _dwWaveNames[p->p_waveType]);
    else _di(f, "waveType", p->p_waveType);
    _di(f, "scwIndex", p->p_scwIndex);
    _db(f, "envelopeEnabled", p->p_envelopeEnabled);
    _dw(f, "attack", p->p_attack); _dw(f, "decay", p->p_decay);
    _dw(f, "sustain", p->p_sustain); _dw(f, "release", p->p_release);
    _db(f, "expRelease", p->p_expRelease); _db(f, "expDecay", p->p_expDecay);
    _db(f, "oneShot", p->p_oneShot);
    _dw(f, "volume", p->p_volume);
    _dw(f, "pulseWidth", p->p_pulseWidth); _dw(f, "pwmRate", p->p_pwmRate); _dw(f, "pwmDepth", p->p_pwmDepth);
    _dw(f, "vibratoRate", p->p_vibratoRate); _dw(f, "vibratoDepth", p->p_vibratoDepth);
    _db(f, "filterEnabled", p->p_filterEnabled);
    _dw(f, "filterCutoff", p->p_filterCutoff); _dw(f, "filterResonance", p->p_filterResonance);
    _di(f, "filterType", p->p_filterType); _di(f, "filterModel", p->p_filterModel);
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
    _dw(f, "filterLfoPhaseOffset", p->p_filterLfoPhaseOffset); _dw(f, "resoLfoPhaseOffset", p->p_resoLfoPhaseOffset);
    _dw(f, "ampLfoPhaseOffset", p->p_ampLfoPhaseOffset); _dw(f, "pitchLfoPhaseOffset", p->p_pitchLfoPhaseOffset);
    _db(f, "arpEnabled", p->p_arpEnabled); _di(f, "arpMode", p->p_arpMode);
    _di(f, "arpRateDiv", p->p_arpRateDiv); _dw(f, "arpRate", p->p_arpRate);
    _di(f, "arpChord", p->p_arpChord);
    _di(f, "unisonCount", p->p_unisonCount); _dw(f, "unisonDetune", p->p_unisonDetune);
    _dw(f, "unisonMix", p->p_unisonMix);
    _db(f, "monoMode", p->p_monoMode); _db(f, "monoRetrigger", p->p_monoRetrigger); _db(f, "monoHardRetrigger", p->p_monoHardRetrigger); _dw(f, "glideTime", p->p_glideTime);
    _dw(f, "legatoWindow", p->p_legatoWindow); _di(f, "notePriority", p->p_notePriority);
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
    _di(f, "granularScwIndex", p->p_granularScwIndex); _di(f, "granularSamplerSlot", p->p_granularSamplerSlot); _dw(f, "granularGrainSize", p->p_granularGrainSize);
    _dw(f, "granularDensity", p->p_granularDensity); _dw(f, "granularPosition", p->p_granularPosition);
    _dw(f, "granularPosRandom", p->p_granularPosRandom); _dw(f, "granularPitch", p->p_granularPitch);
    _dw(f, "granularPitchRandom", p->p_granularPitchRandom); _dw(f, "granularAmpRandom", p->p_granularAmpRandom);
    _dw(f, "granularSpread", p->p_granularSpread); _db(f, "granularFreeze", p->p_granularFreeze);
    _dw(f, "fmLfoRate", p->p_fmLfoRate); _dw(f, "fmLfoDepth", p->p_fmLfoDepth);
    _di(f, "fmLfoShape", p->p_fmLfoShape); _di(f, "fmLfoSync", p->p_fmLfoSync);
    _dw(f, "fmLfoPhaseOffset", p->p_fmLfoPhaseOffset);
    _dw(f, "fmModRatio", p->p_fmModRatio); _dw(f, "fmModIndex", p->p_fmModIndex);
    _dw(f, "fmFeedback", p->p_fmFeedback);
    _dw(f, "fmMod2Ratio", p->p_fmMod2Ratio); _dw(f, "fmMod2Index", p->p_fmMod2Index);
    _di(f, "fmAlgorithm", p->p_fmAlgorithm);
    _di(f, "pdWaveType", p->p_pdWaveType); _dw(f, "pdDistortion", p->p_pdDistortion);
    _di(f, "membranePreset", p->p_membranePreset); _dw(f, "membraneDamping", p->p_membraneDamping);
    _dw(f, "membraneStrike", p->p_membraneStrike); _dw(f, "membraneBend", p->p_membraneBend);
    _dw(f, "membraneBendDecay", p->p_membraneBendDecay);
    _di(f, "metallicPreset", p->p_metallicPreset); _dw(f, "metallicRingMix", p->p_metallicRingMix);
    _dw(f, "metallicNoiseLevel", p->p_metallicNoiseLevel); _dw(f, "metallicBrightness", p->p_metallicBrightness);
    _dw(f, "metallicPitchEnv", p->p_metallicPitchEnv); _dw(f, "metallicPitchEnvDecay", p->p_metallicPitchEnvDecay);
    _dw(f, "bowPressure", p->p_bowPressure); _dw(f, "bowSpeed", p->p_bowSpeed);
    _dw(f, "bowPosition", p->p_bowPosition);
    _dw(f, "pipeBreath", p->p_pipeBreath); _dw(f, "pipeEmbouchure", p->p_pipeEmbouchure);
    _dw(f, "pipeBore", p->p_pipeBore); _dw(f, "pipeOverblow", p->p_pipeOverblow);
    _dw(f, "reedStiffness", p->p_reedStiffness); _dw(f, "reedAperture", p->p_reedAperture);
    _dw(f, "reedBlowPressure", p->p_reedBlowPressure); _dw(f, "reedBore", p->p_reedBore);
    _dw(f, "reedVibratoDepth", p->p_reedVibratoDepth);
    _dw(f, "brassBlowPressure", p->p_brassBlowPressure); _dw(f, "brassLipTension", p->p_brassLipTension);
    _dw(f, "brassLipAperture", p->p_brassLipAperture); _dw(f, "brassBore", p->p_brassBore);
    _dw(f, "brassMute", p->p_brassMute);
    _di(f, "guitarPreset", p->p_guitarPreset); _dw(f, "guitarBodyMix", p->p_guitarBodyMix);
    _dw(f, "guitarBodyBrightness", p->p_guitarBodyBrightness);
    _dw(f, "guitarPickPosition", p->p_guitarPickPosition); _dw(f, "guitarBuzz", p->p_guitarBuzz);
    _di(f, "mandolinPreset", p->p_mandolinPreset); _dw(f, "mandolinBodyMix", p->p_mandolinBodyMix);
    _dw(f, "mandolinCourseDetune", p->p_mandolinCourseDetune);
    _dw(f, "mandolinPickPosition", p->p_mandolinPickPosition);
    _di(f, "whistlePreset", p->p_whistlePreset); _dw(f, "whistleBreath", p->p_whistleBreath);
    _dw(f, "whistleNoiseGain", p->p_whistleNoiseGain);
    _dw(f, "whistleFippleFreqMod", p->p_whistleFippleFreqMod);
    _di(f, "stifkarpPreset", p->p_stifkarpPreset);
    _dw(f, "stifkarpHardness", p->p_stifkarpHardness); _dw(f, "stifkarpStiffness", p->p_stifkarpStiffness);
    _dw(f, "stifkarpStrikePos", p->p_stifkarpStrikePos); _dw(f, "stifkarpBodyMix", p->p_stifkarpBodyMix);
    _dw(f, "stifkarpBodyBrightness", p->p_stifkarpBodyBrightness);
    _dw(f, "stifkarpDamper", p->p_stifkarpDamper); _dw(f, "stifkarpSympathetic", p->p_stifkarpSympathetic);
    _dw(f, "stifkarpDetune", p->p_stifkarpDetune);
    _di(f, "shakerPreset", p->p_shakerPreset); _dw(f, "shakerParticles", p->p_shakerParticles);
    _dw(f, "shakerDecayRate", p->p_shakerDecayRate); _dw(f, "shakerResonance", p->p_shakerResonance);
    _dw(f, "shakerBrightness", p->p_shakerBrightness); _dw(f, "shakerScrape", p->p_shakerScrape);
    _di(f, "bandedwgPreset", p->p_bandedwgPreset); _dw(f, "bandedwgBowPressure", p->p_bandedwgBowPressure);
    _dw(f, "bandedwgBowSpeed", p->p_bandedwgBowSpeed); _dw(f, "bandedwgStrikePos", p->p_bandedwgStrikePos);
    _dw(f, "bandedwgBrightness", p->p_bandedwgBrightness); _dw(f, "bandedwgSustain", p->p_bandedwgSustain);
    _di(f, "vfPhoneme", p->p_vfPhoneme); _di(f, "vfPhonemeTarget", p->p_vfPhonemeTarget);
    _dw(f, "vfMorphRate", p->p_vfMorphRate); _dw(f, "vfAspiration", p->p_vfAspiration);
    _dw(f, "vfOpenQuotient", p->p_vfOpenQuotient); _dw(f, "vfSpectralTilt", p->p_vfSpectralTilt);
    _dw(f, "vfFormantShift", p->p_vfFormantShift); _dw(f, "vfVibratoDepth", p->p_vfVibratoDepth);
    _dw(f, "vfVibratoRate", p->p_vfVibratoRate); _di(f, "vfConsonant", p->p_vfConsonant);
    _di(f, "birdType", p->p_birdType); _dw(f, "birdChirpRange", p->p_birdChirpRange);
    _dw(f, "birdTrillRate", p->p_birdTrillRate); _dw(f, "birdTrillDepth", p->p_birdTrillDepth);
    _dw(f, "birdAmRate", p->p_birdAmRate); _dw(f, "birdAmDepth", p->p_birdAmDepth);
    _dw(f, "birdHarmonics", p->p_birdHarmonics);
    _dw(f, "epHardness", p->p_epHardness); _dw(f, "epToneBar", p->p_epToneBar);
    _dw(f, "epPickupPos", p->p_epPickupPos); _dw(f, "epPickupDist", p->p_epPickupDist);
    _dw(f, "epDecay", p->p_epDecay); _dw(f, "epBell", p->p_epBell);
    _dw(f, "epBellTone", p->p_epBellTone); _di(f, "epPickupType", p->p_epPickupType);
    _di(f, "epRatioSet", p->p_epRatioSet);
    for (int i = 0; i < ORGAN_DRAWBARS; i++) { char k[20]; snprintf(k,sizeof(k),"orgDrawbar%d",i); _dw(f, k, p->p_orgDrawbar[i]); }
    _dw(f, "orgClick", p->p_orgClick); _dw(f, "orgCrosstalk", p->p_orgCrosstalk);
    _dw(f, "pitchEnvAmount", p->p_pitchEnvAmount); _dw(f, "pitchEnvDecay", p->p_pitchEnvDecay);
    _dw(f, "pitchEnvCurve", p->p_pitchEnvCurve); _db(f, "pitchEnvLinear", p->p_pitchEnvLinear);
    _dw(f, "noiseMix", p->p_noiseMix); _dw(f, "noiseTone", p->p_noiseTone);
    _dw(f, "noiseHP", p->p_noiseHP); _dw(f, "noiseDecay", p->p_noiseDecay);
    _di(f, "retriggerCount", p->p_retriggerCount); _dw(f, "retriggerSpread", p->p_retriggerSpread);
    _db(f, "retriggerOverlap", p->p_retriggerOverlap); _dw(f, "retriggerBurstDecay", p->p_retriggerBurstDecay);
    _dw(f, "osc2Ratio", p->p_osc2Ratio); _dw(f, "osc2Level", p->p_osc2Level); _dw(f, "osc2Decay", p->p_osc2Decay);
    _dw(f, "osc3Ratio", p->p_osc3Ratio); _dw(f, "osc3Level", p->p_osc3Level); _dw(f, "osc3Decay", p->p_osc3Decay);
    _dw(f, "osc4Ratio", p->p_osc4Ratio); _dw(f, "osc4Level", p->p_osc4Level); _dw(f, "osc4Decay", p->p_osc4Decay);
    _dw(f, "osc5Ratio", p->p_osc5Ratio); _dw(f, "osc5Level", p->p_osc5Level); _dw(f, "osc5Decay", p->p_osc5Decay);
    _dw(f, "osc6Ratio", p->p_osc6Ratio); _dw(f, "osc6Level", p->p_osc6Level); _dw(f, "osc6Decay", p->p_osc6Decay);
    _dw(f, "oscVelSens", p->p_oscVelSens);
    _dw(f, "velToFilter", p->p_velToFilter); _dw(f, "velToClick", p->p_velToClick); _dw(f, "velToDrive", p->p_velToDrive);
    _dw(f, "drive", p->p_drive); _di(f, "driveMode", p->p_driveMode);
    _dw(f, "clickLevel", p->p_clickLevel); _dw(f, "clickTime", p->p_clickTime);
    _di(f, "noiseMode", p->p_noiseMode); _di(f, "oscMixMode", p->p_oscMixMode);
    _dw(f, "retriggerCurve", p->p_retriggerCurve); _db(f, "phaseReset", p->p_phaseReset);
    _db(f, "noiseLPBypass", p->p_noiseLPBypass); _di(f, "noiseType", p->p_noiseType);
    _db(f, "useTriggerFreq", p->p_useTriggerFreq); _dw(f, "triggerFreq", p->p_triggerFreq);
    _db(f, "choke", p->p_choke);
    _db(f, "analogRolloff", p->p_analogRolloff); _db(f, "tubeSaturation", p->p_tubeSaturation);
    _db(f, "analogVariance", p->p_analogVariance);
    _db(f, "ringMod", p->p_ringMod); _dw(f, "ringModFreq", p->p_ringModFreq);
    _dw(f, "wavefoldAmount", p->p_wavefoldAmount);
    _db(f, "hardSync", p->p_hardSync); _dw(f, "hardSyncRatio", p->p_hardSyncRatio);
    // 303 acid mode
    _db(f, "acidMode", p->p_acidMode);
    _dw(f, "accentSweepAmt", p->p_accentSweepAmt); _dw(f, "gimmickDipAmt", p->p_gimmickDipAmt);
    // Per-voice formant filter
    _db(f, "formantEnabled", p->p_formantEnabled);
    _di(f, "formantFrom", p->p_formantFrom); _di(f, "formantTo", p->p_formantTo);
    _dw(f, "formantMorphTime", p->p_formantMorphTime);
    _dw(f, "formantShift", p->p_formantShift);
    _dw(f, "formantQ", p->p_formantQ);
    _dw(f, "formantMix", p->p_formantMix);
    _db(f, "formantRandom", p->p_formantRandom);
    _di(f, "formantMode", p->p_formantMode);
    _dw(f, "fbBaseFreq", p->p_fbBaseFreq);
    _dw(f, "fbSpacing", p->p_fbSpacing);
    _dw(f, "fbAlpha", p->p_fbAlpha);
    _dw(f, "fbBeta", p->p_fbBeta);
    _dw(f, "fbQ", p->p_fbQ);
    _dw(f, "fbKeyTrack", p->p_fbKeyTrack);
    _dw(f, "fbMorphOsc", p->p_fbMorphOsc);
    _dw(f, "fbRandomize", p->p_fbRandomize);
    _dw(f, "fbEnvAlpha", p->p_fbEnvAlpha);
    _dw(f, "fbLfoRate", p->p_fbLfoRate);
    _di(f, "fbLfoSync", p->p_fbLfoSync);
    _dw(f, "fbLfoAlpha", p->p_fbLfoAlpha);
    _dw(f, "fbLfoBeta", p->p_fbLfoBeta);
    _di(f, "fbLfoShape", p->p_fbLfoShape);
    _dw(f, "fbNoiseMix", p->p_fbNoiseMix);
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
    // Single-track format: one trackType + length per pattern
    const char *ttName = (p->trackType == TRACK_DRUM) ? "drum" :
                         (p->trackType == TRACK_SAMPLER) ? "sampler" : "melodic";
    fprintf(f, "trackType = %s\n", ttName);
    fprintf(f, "length = %d\n", p->length);

    if (p->trackType == TRACK_DRUM) {
        for (int s = 0; s < p->length; s++) {
            if (!patGetDrum(p, s)) continue;
            fprintf(f, "d step=%d vel=%.3g", s, (double)patGetDrumVel(p, s));
            float dp = patGetDrumPitch(p, s);
            if (dp != 0.0f) fprintf(f, " pitch=%.3g", (double)dp);
            float dProb = patGetDrumProb(p, s);
            if (dProb > 0.0f && dProb < 1.0f) fprintf(f, " prob=%.3g", (double)dProb);
            int dCond = patGetDrumCond(p, s);
            if (dCond != COND_ALWAYS) fprintf(f, " cond=%s", _dwCondNames[dCond]);
            fprintf(f, "\n");
        }
    } else if (p->trackType == TRACK_SAMPLER) {
        for (int s = 0; s < p->length; s++) {
            const StepV2 *sv = &p->steps[s];
            if (sv->noteCount == 0) continue;
            for (int v = 0; v < sv->noteCount; v++) {
                const StepNote *sn = &sv->notes[v];
                if (sn->note == SEQ_NOTE_OFF) continue;
                fprintf(f, "s step=%d slice=%d note=%d vel=%.3g",
                        s, (int)sn->slice, (int)sn->note, (double)velU8ToFloat(sn->velocity));
                if (sn->nudge != 0) fprintf(f, " nudge=%d", (int)sn->nudge);
                if (v == 0) {
                    float sProb = probU8ToFloat(sv->probability);
                    if (sProb > 0.0f && sProb < 1.0f) fprintf(f, " prob=%.3g", (double)sProb);
                    int sCond = (int)sv->condition;
                    if (sCond != COND_ALWAYS) fprintf(f, " cond=%s", _dwCondNames[sCond]);
                }
                fprintf(f, "\n");
            }
        }
    } else {
        // TRACK_MELODIC
        for (int s = 0; s < p->length; s++) {
            const StepV2 *sv = &p->steps[s];
            if (sv->noteCount == 0) continue;
            for (int v = 0; v < sv->noteCount; v++) {
                const StepNote *sn = &sv->notes[v];
                if (sn->note == SEQ_NOTE_OFF) continue;
                char nn[8]; _dwMidiToName(sn->note, nn, sizeof(nn));
                fprintf(f, "m step=%d note=%s vel=%.3g gate=%d",
                        s, nn, (double)velU8ToFloat(sn->velocity), (int)sn->gate);
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
            fprintf(f, "p step=%d param=%s val=%.6g\n",
                    pl->step, _dwPlockNames[pl->param], (double)pl->value);
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
    if (daw.songName[0]) _ds(f, "songName", daw.songName);
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
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "trackSwing%d", i);
        _di(f, key, seq.trackSwing[i]);
        snprintf(key, sizeof(key), "trackTranspose%d", i);
        _di(f, key, seq.trackTranspose[i]);
    }
    _di(f, "melodyTimingJitter", seq.humanize.timingJitter);
    _dw(f, "melodyVelocityJitter", seq.humanize.velocityJitter);

    // [settings]
    fprintf(f, "\n[settings]\n");
    _dw(f, "masterVol", daw.masterVol);
    _db(f, "voiceRandomVowel", daw.voiceRandomVowel);
    _dw(f, "masterSpeed", daw.masterSpeed);
    _db(f, "chromaticMode", daw.chromaticMode);
    _di(f, "chromaticSample", daw.chromaticSample);
    _di(f, "chromaticRoot", daw.chromaticRootNote);

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
        snprintf(k, sizeof(k), "delaySend%d", b); _dw(f, k, daw.mixer.delaySend[b]);
        snprintf(k, sizeof(k), "mute%d", b); _db(f, k, daw.mixer.mute[b]);
        snprintf(k, sizeof(k), "solo%d", b); _db(f, k, daw.mixer.solo[b]);
        snprintf(k, sizeof(k), "filterOn%d", b); _db(f, k, daw.mixer.filterOn[b]);
        snprintf(k, sizeof(k), "filterCut%d", b); _dw(f, k, daw.mixer.filterCut[b]);
        snprintf(k, sizeof(k), "filterRes%d", b); _dw(f, k, daw.mixer.filterRes[b]);
        snprintf(k, sizeof(k), "filterType%d", b); _di(f, k, daw.mixer.filterType[b]);
        snprintf(k, sizeof(k), "distOn%d", b); _db(f, k, daw.mixer.distOn[b]);
        snprintf(k, sizeof(k), "distDrive%d", b); _dw(f, k, daw.mixer.distDrive[b]);
        snprintf(k, sizeof(k), "distMix%d", b); _dw(f, k, daw.mixer.distMix[b]);
        snprintf(k, sizeof(k), "distMode%d", b); _di(f, k, daw.mixer.distMode[b]);
        snprintf(k, sizeof(k), "delayOn%d", b); _db(f, k, daw.mixer.delayOn[b]);
        snprintf(k, sizeof(k), "delaySync%d", b); _db(f, k, daw.mixer.delaySync[b]);
        snprintf(k, sizeof(k), "delaySyncDiv%d", b); _di(f, k, daw.mixer.delaySyncDiv[b]);
        snprintf(k, sizeof(k), "delayTime%d", b); _dw(f, k, daw.mixer.delayTime[b]);
        snprintf(k, sizeof(k), "delayFB%d", b); _dw(f, k, daw.mixer.delayFB[b]);
        snprintf(k, sizeof(k), "delayMix%d", b); _dw(f, k, daw.mixer.delayMix[b]);
        snprintf(k, sizeof(k), "eqOn%d", b); _db(f, k, daw.mixer.eqOn[b]);
        snprintf(k, sizeof(k), "eqLowGain%d", b); _dw(f, k, daw.mixer.eqLowGain[b]);
        snprintf(k, sizeof(k), "eqHighGain%d", b); _dw(f, k, daw.mixer.eqHighGain[b]);
        snprintf(k, sizeof(k), "eqLowFreq%d", b); _dw(f, k, daw.mixer.eqLowFreq[b]);
        snprintf(k, sizeof(k), "eqHighFreq%d", b); _dw(f, k, daw.mixer.eqHighFreq[b]);
        snprintf(k, sizeof(k), "chorusOn%d", b); _db(f, k, daw.mixer.chorusOn[b]);
        snprintf(k, sizeof(k), "chorusBBD%d", b); _db(f, k, daw.mixer.chorusBBD[b]);
        snprintf(k, sizeof(k), "chorusRate%d", b); _dw(f, k, daw.mixer.chorusRate[b]);
        snprintf(k, sizeof(k), "chorusDepth%d", b); _dw(f, k, daw.mixer.chorusDepth[b]);
        snprintf(k, sizeof(k), "chorusMix%d", b); _dw(f, k, daw.mixer.chorusMix[b]);
        snprintf(k, sizeof(k), "chorusDelay%d", b); _dw(f, k, daw.mixer.chorusDelay[b]);
        snprintf(k, sizeof(k), "chorusFB%d", b); _dw(f, k, daw.mixer.chorusFB[b]);
        snprintf(k, sizeof(k), "phaserOn%d", b); _db(f, k, daw.mixer.phaserOn[b]);
        snprintf(k, sizeof(k), "phaserRate%d", b); _dw(f, k, daw.mixer.phaserRate[b]);
        snprintf(k, sizeof(k), "phaserDepth%d", b); _dw(f, k, daw.mixer.phaserDepth[b]);
        snprintf(k, sizeof(k), "phaserMix%d", b); _dw(f, k, daw.mixer.phaserMix[b]);
        snprintf(k, sizeof(k), "phaserFB%d", b); _dw(f, k, daw.mixer.phaserFB[b]);
        snprintf(k, sizeof(k), "phaserStages%d", b); _di(f, k, daw.mixer.phaserStages[b]);
        snprintf(k, sizeof(k), "combOn%d", b); _db(f, k, daw.mixer.combOn[b]);
        snprintf(k, sizeof(k), "combFreq%d", b); _dw(f, k, daw.mixer.combFreq[b]);
        snprintf(k, sizeof(k), "combFB%d", b); _dw(f, k, daw.mixer.combFB[b]);
        snprintf(k, sizeof(k), "combMix%d", b); _dw(f, k, daw.mixer.combMix[b]);
        snprintf(k, sizeof(k), "combDamping%d", b); _dw(f, k, daw.mixer.combDamping[b]);
        snprintf(k, sizeof(k), "octaverOn%d", b); _db(f, k, daw.mixer.octaverOn[b]);
        snprintf(k, sizeof(k), "octaverMix%d", b); _dw(f, k, daw.mixer.octaverMix[b]);
        snprintf(k, sizeof(k), "octaverSub%d", b); _dw(f, k, daw.mixer.octaverSubLevel[b]);
        snprintf(k, sizeof(k), "octaverTone%d", b); _dw(f, k, daw.mixer.octaverTone[b]);
        snprintf(k, sizeof(k), "tremoloOn%d", b); _db(f, k, daw.mixer.tremoloOn[b]);
        snprintf(k, sizeof(k), "tremoloRate%d", b); _dw(f, k, daw.mixer.tremoloRate[b]);
        snprintf(k, sizeof(k), "tremoloDepth%d", b); _dw(f, k, daw.mixer.tremoloDepth[b]);
        snprintf(k, sizeof(k), "tremoloShape%d", b); _di(f, k, daw.mixer.tremoloShape[b]);
        snprintf(k, sizeof(k), "leslieOn%d", b); _db(f, k, daw.mixer.leslieOn[b]);
        snprintf(k, sizeof(k), "leslieSpeed%d", b); _di(f, k, daw.mixer.leslieSpeed[b]);
        snprintf(k, sizeof(k), "leslieDrive%d", b); _dw(f, k, daw.mixer.leslieDrive[b]);
        snprintf(k, sizeof(k), "leslieBal%d", b); _dw(f, k, daw.mixer.leslieBalance[b]);
        snprintf(k, sizeof(k), "leslieDopp%d", b); _dw(f, k, daw.mixer.leslieDoppler[b]);
        snprintf(k, sizeof(k), "leslieMix%d", b); _dw(f, k, daw.mixer.leslieMix[b]);
        snprintf(k, sizeof(k), "wahOn%d", b); _db(f, k, daw.mixer.wahOn[b]);
        snprintf(k, sizeof(k), "wahMode%d", b); _di(f, k, daw.mixer.wahMode[b]);
        snprintf(k, sizeof(k), "wahRate%d", b); _dw(f, k, daw.mixer.wahRate[b]);
        snprintf(k, sizeof(k), "wahSens%d", b); _dw(f, k, daw.mixer.wahSensitivity[b]);
        snprintf(k, sizeof(k), "wahFreqLow%d", b); _dw(f, k, daw.mixer.wahFreqLow[b]);
        snprintf(k, sizeof(k), "wahFreqHigh%d", b); _dw(f, k, daw.mixer.wahFreqHigh[b]);
        snprintf(k, sizeof(k), "wahRes%d", b); _dw(f, k, daw.mixer.wahResonance[b]);
        snprintf(k, sizeof(k), "wahMix%d", b); _dw(f, k, daw.mixer.wahMix[b]);
        snprintf(k, sizeof(k), "ringModOn%d", b); _db(f, k, daw.mixer.ringModOn[b]);
        snprintf(k, sizeof(k), "ringModFreq%d", b); _dw(f, k, daw.mixer.ringModFreq[b]);
        snprintf(k, sizeof(k), "ringModMix%d", b); _dw(f, k, daw.mixer.ringModMix[b]);
        snprintf(k, sizeof(k), "compOn%d", b); _db(f, k, daw.mixer.compOn[b]);
        snprintf(k, sizeof(k), "compThresh%d", b); _dw(f, k, daw.mixer.compThreshold[b]);
        snprintf(k, sizeof(k), "compRatio%d", b); _dw(f, k, daw.mixer.compRatio[b]);
        snprintf(k, sizeof(k), "compAtk%d", b); _dw(f, k, daw.mixer.compAttack[b]);
        snprintf(k, sizeof(k), "compRel%d", b); _dw(f, k, daw.mixer.compRelease[b]);
        snprintf(k, sizeof(k), "compMakeup%d", b); _dw(f, k, daw.mixer.compMakeup[b]);
    }

    // [sidechain]
    fprintf(f, "\n[sidechain]\n");
    _db(f, "on", daw.sidechain.on);
    _di(f, "source", daw.sidechain.source); _di(f, "target", daw.sidechain.target);
    _dw(f, "depth", daw.sidechain.depth); _dw(f, "attack", daw.sidechain.attack);
    _dw(f, "release", daw.sidechain.release); _dw(f, "hpFreq", daw.sidechain.hpFreq);
    _db(f, "envOn", daw.sidechain.envOn);
    _di(f, "envSource", daw.sidechain.envSource); _di(f, "envTarget", daw.sidechain.envTarget);
    _dw(f, "envDepth", daw.sidechain.envDepth); _dw(f, "envAttack", daw.sidechain.envAttack);
    _dw(f, "envHold", daw.sidechain.envHold); _dw(f, "envRelease", daw.sidechain.envRelease);
    _di(f, "envCurve", daw.sidechain.envCurve); _dw(f, "envHPFreq", daw.sidechain.envHPFreq);

    // [masterfx]
    fprintf(f, "\n[masterfx]\n");
    _db(f, "octaverOn", daw.masterFx.octaverOn); _dw(f, "octaverMix", daw.masterFx.octaverMix);
    _dw(f, "octaverSubLevel", daw.masterFx.octaverSubLevel); _dw(f, "octaverTone", daw.masterFx.octaverTone);
    _db(f, "tremoloOn", daw.masterFx.tremoloOn); _dw(f, "tremoloRate", daw.masterFx.tremoloRate);
    _dw(f, "tremoloDepth", daw.masterFx.tremoloDepth); _di(f, "tremoloShape", daw.masterFx.tremoloShape);
    _db(f, "leslieOn", daw.masterFx.leslieOn); _di(f, "leslieSpeed", daw.masterFx.leslieSpeed);
    _dw(f, "leslieDrive", daw.masterFx.leslieDrive); _dw(f, "leslieBal", daw.masterFx.leslieBalance);
    _dw(f, "leslieDopp", daw.masterFx.leslieDoppler); _dw(f, "leslieMix", daw.masterFx.leslieMix);
    _db(f, "wahOn", daw.masterFx.wahOn); _di(f, "wahMode", daw.masterFx.wahMode);
    _dw(f, "wahRate", daw.masterFx.wahRate); _dw(f, "wahSensitivity", daw.masterFx.wahSensitivity);
    _dw(f, "wahFreqLow", daw.masterFx.wahFreqLow); _dw(f, "wahFreqHigh", daw.masterFx.wahFreqHigh);
    _dw(f, "wahResonance", daw.masterFx.wahResonance); _dw(f, "wahMix", daw.masterFx.wahMix);
    _db(f, "distOn", daw.masterFx.distOn); _dw(f, "distDrive", daw.masterFx.distDrive);
    _dw(f, "distTone", daw.masterFx.distTone); _dw(f, "distMix", daw.masterFx.distMix); _di(f, "distMode", daw.masterFx.distMode);
    _db(f, "crushOn", daw.masterFx.crushOn); _dw(f, "crushBits", daw.masterFx.crushBits);
    _dw(f, "crushRate", daw.masterFx.crushRate); _dw(f, "crushMix", daw.masterFx.crushMix);
    _db(f, "chorusOn", daw.masterFx.chorusOn); _db(f, "chorusBBD", daw.masterFx.chorusBBD);
    _dw(f, "chorusRate", daw.masterFx.chorusRate);
    _dw(f, "chorusDepth", daw.masterFx.chorusDepth); _dw(f, "chorusMix", daw.masterFx.chorusMix);
    _db(f, "flangerOn", daw.masterFx.flangerOn); _dw(f, "flangerRate", daw.masterFx.flangerRate);
    _dw(f, "flangerDepth", daw.masterFx.flangerDepth); _dw(f, "flangerFeedback", daw.masterFx.flangerFeedback);
    _dw(f, "flangerMix", daw.masterFx.flangerMix);
    _db(f, "phaserOn", daw.masterFx.phaserOn); _dw(f, "phaserRate", daw.masterFx.phaserRate);
    _dw(f, "phaserDepth", daw.masterFx.phaserDepth); _dw(f, "phaserMix", daw.masterFx.phaserMix);
    _dw(f, "phaserFeedback", daw.masterFx.phaserFeedback); _di(f, "phaserStages", daw.masterFx.phaserStages);
    _db(f, "combOn", daw.masterFx.combOn); _dw(f, "combFreq", daw.masterFx.combFreq);
    _dw(f, "combFeedback", daw.masterFx.combFeedback); _dw(f, "combMix", daw.masterFx.combMix);
    _dw(f, "combDamping", daw.masterFx.combDamping);
    _db(f, "ringModOn", daw.masterFx.ringModOn); _dw(f, "ringModFreq", daw.masterFx.ringModFreq);
    _dw(f, "ringModMix", daw.masterFx.ringModMix);
    _db(f, "tapeOn", daw.masterFx.tapeOn); _dw(f, "tapeSaturation", daw.masterFx.tapeSaturation);
    _dw(f, "tapeWow", daw.masterFx.tapeWow); _dw(f, "tapeFlutter", daw.masterFx.tapeFlutter);
    _dw(f, "tapeHiss", daw.masterFx.tapeHiss);
    _db(f, "delayOn", daw.masterFx.delayOn); _dw(f, "delayTime", daw.masterFx.delayTime);
    _dw(f, "delayFeedback", daw.masterFx.delayFeedback); _dw(f, "delayTone", daw.masterFx.delayTone);
    _dw(f, "delayMix", daw.masterFx.delayMix);
    _db(f, "reverbOn", daw.masterFx.reverbOn); _db(f, "reverbFDN", daw.masterFx.reverbFDN); _dw(f, "reverbSize", daw.masterFx.reverbSize);
    _dw(f, "reverbDamping", daw.masterFx.reverbDamping); _dw(f, "reverbPreDelay", daw.masterFx.reverbPreDelay);
    _dw(f, "reverbMix", daw.masterFx.reverbMix); _dw(f, "reverbBass", daw.masterFx.reverbBass);
    _db(f, "eqOn", daw.masterFx.eqOn); _dw(f, "eqLowGain", daw.masterFx.eqLowGain);
    _dw(f, "eqHighGain", daw.masterFx.eqHighGain); _dw(f, "eqLowFreq", daw.masterFx.eqLowFreq);
    _dw(f, "eqHighFreq", daw.masterFx.eqHighFreq);
    _db(f, "subBassBoost", daw.masterFx.subBassBoost);
    _db(f, "compOn", daw.masterFx.compOn); _dw(f, "compThreshold", daw.masterFx.compThreshold);
    _dw(f, "compRatio", daw.masterFx.compRatio); _dw(f, "compAttack", daw.masterFx.compAttack);
    _dw(f, "compRelease", daw.masterFx.compRelease); _dw(f, "compMakeup", daw.masterFx.compMakeup); _dw(f, "compKnee", daw.masterFx.compKnee);
    _db(f, "mbOn", daw.masterFx.mbOn);
    _dw(f, "mbLowCross", daw.masterFx.mbLowCross); _dw(f, "mbHighCross", daw.masterFx.mbHighCross);
    _dw(f, "mbLowGain", daw.masterFx.mbLowGain); _dw(f, "mbMidGain", daw.masterFx.mbMidGain); _dw(f, "mbHighGain", daw.masterFx.mbHighGain);
    _dw(f, "mbLowDrive", daw.masterFx.mbLowDrive); _dw(f, "mbMidDrive", daw.masterFx.mbMidDrive); _dw(f, "mbHighDrive", daw.masterFx.mbHighDrive);
    _db(f, "vinylOn", daw.masterFx.vinylOn); _dw(f, "vinylCrackle", daw.masterFx.vinylCrackle);
    _dw(f, "vinylNoise", daw.masterFx.vinylNoise); _dw(f, "vinylWarp", daw.masterFx.vinylWarp);
    _dw(f, "vinylWarpRate", daw.masterFx.vinylWarpRate); _dw(f, "vinylTone", daw.masterFx.vinylTone);

    // [tapefx]
    fprintf(f, "\n[tapefx]\n");
    _db(f, "enabled", daw.tapeFx.enabled);
    _dw(f, "headTime", daw.tapeFx.headTime); _dw(f, "feedback", daw.tapeFx.feedback);
    _dw(f, "mix", daw.tapeFx.mix); _di(f, "inputSource", daw.tapeFx.inputSource);
    _db(f, "preReverb", daw.tapeFx.preReverb);
    _dw(f, "saturation", daw.tapeFx.saturation); _dw(f, "toneHigh", daw.tapeFx.toneHigh);
    _dw(f, "noise", daw.tapeFx.noise); _dw(f, "degradeRate", daw.tapeFx.degradeRate);
    _dw(f, "wow", daw.tapeFx.wow); _dw(f, "flutter", daw.tapeFx.flutter);
    _dw(f, "drift", daw.tapeFx.drift); _dw(f, "speedTarget", daw.tapeFx.speedTarget); _dw(f, "speedSlew", daw.tapeFx.speedSlew);
    _dw(f, "rewindTime", daw.tapeFx.rewindTime); _dw(f, "rewindMinSpeed", daw.tapeFx.rewindMinSpeed);
    _dw(f, "rewindVinyl", daw.tapeFx.rewindVinyl); _dw(f, "rewindWobble", daw.tapeFx.rewindWobble);
    _dw(f, "rewindFilter", daw.tapeFx.rewindFilter); _di(f, "rewindCurve", daw.tapeFx.rewindCurve);
    _dw(f, "tapeStopTime", daw.tapeFx.tapeStopTime); _di(f, "tapeStopCurve", daw.tapeFx.tapeStopCurve);
    _db(f, "tapeStopSpinBack", daw.tapeFx.tapeStopSpinBack); _dw(f, "tapeStopSpinTime", daw.tapeFx.tapeStopSpinTime);
    _di(f, "beatRepeatDiv", daw.tapeFx.beatRepeatDiv); _dw(f, "beatRepeatDecay", daw.tapeFx.beatRepeatDecay);
    _dw(f, "beatRepeatPitch", daw.tapeFx.beatRepeatPitch); _dw(f, "beatRepeatMix", daw.tapeFx.beatRepeatMix);
    _dw(f, "beatRepeatGate", daw.tapeFx.beatRepeatGate);
    _di(f, "djfxLoopDiv", daw.tapeFx.djfxLoopDiv);

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

    // [pertrack] — per-track arrangement grid
    if (daw.arr.length > 0) {
        fprintf(f, "\n[pertrack]\n");
        _db(f, "arrMode", daw.arr.arrMode);
        _di(f, "arrLength", daw.arr.length);
        for (int t = 0; t < ARR_MAX_TRACKS; t++) {
            char k[16]; snprintf(k, sizeof(k), "arr%d", t);
            fprintf(f, "%s =", k);
            for (int b = 0; b < daw.arr.length; b++) fprintf(f, " %d", daw.arr.cells[b][t]);
            fprintf(f, "\n");
        }
    }

    // [launcher] — clip launcher slot assignments
    {
        bool hasSlots = false;
        for (int t = 0; t < ARR_MAX_TRACKS && !hasSlots; t++)
            for (int s = 0; s < LAUNCHER_MAX_SLOTS && !hasSlots; s++)
                if (daw.launcher.tracks[t].pattern[s] >= 0) hasSlots = true;
        if (hasSlots) {
            fprintf(f, "\n[launcher]\n");
            for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                LauncherTrack *lt = &daw.launcher.tracks[t];
                // Find last non-empty slot for this track
                int last = -1;
                for (int s = LAUNCHER_MAX_SLOTS - 1; s >= 0; s--) {
                    if (lt->pattern[s] >= 0) { last = s; break; }
                }
                if (last >= 0) {
                    char k[16]; snprintf(k, sizeof(k), "t%d", t);
                    fprintf(f, "%s =", k);
                    for (int s = 0; s <= last; s++) fprintf(f, " %d", lt->pattern[s]);
                    fprintf(f, "\n");
                    // Save next actions for slots that have them
                    for (int s = 0; s <= last; s++) {
                        if (lt->nextAction[s] != NEXT_ACTION_LOOP && lt->nextActionLoops[s] > 0) {
                            fprintf(f, "t%d.%d.na = %d %d\n", t, s, (int)lt->nextAction[s], lt->nextActionLoops[s]);
                        }
                    }
                }
            }
        }
    }

#ifdef DAW_HAS_CHOP_STATE
    // [sample] — scratch space recipe
    if (scratchHasData(&scratch) && chopState.sourcePath[0]) {
        fprintf(f, "\n[sample]\n");
        _ds(f, "sourceFile", chopState.sourcePath);
        _di(f, "sourceLoops", chopState.sourceLoops);
        if (chopState.hasSelection) {
            _dw(f, "selStart", chopState.selStart);
            _dw(f, "selEnd", chopState.selEnd);
        }
        _di(f, "sliceCount", chopState.sliceCount);
        _di(f, "chopMode", chopState.chopMode);
        _db(f, "normalize", chopState.normalize);
        _dw(f, "sensitivity", chopState.sensitivity);
        for (int t = 0; t < 4; t++) {
            char k[16]; snprintf(k, sizeof(k), "padMap%d", t);
            _di(f, k, daw.chopSliceMap[t]);
        }
        // Scratch markers
        if (scratch.markerCount > 0) {
            _di(f, "markerCount", scratch.markerCount);
            fprintf(f, "markers = ");
            for (int m = 0; m < scratch.markerCount; m++) {
                if (m > 0) fprintf(f, ",");
                fprintf(f, "%d", scratch.markers[m]);
            }
            fprintf(f, "\n");
        }
    }
#endif

    // Per-slot flags (pitched/oneShot) — only save non-default values
    {
        bool hasSlotFlags = false;
        for (int s = 0; s < SAMPLER_MAX_SAMPLES; s++) {
            if (samplerCtx->samples[s].loaded &&
                (samplerCtx->samples[s].pitched || !samplerCtx->samples[s].oneShot)) {
                hasSlotFlags = true;
                break;
            }
        }
        if (hasSlotFlags) {
            fprintf(f, "\n[slotflags]\n");
            for (int s = 0; s < SAMPLER_MAX_SAMPLES; s++) {
                Sample *sl = &samplerCtx->samples[s];
                if (!sl->loaded) continue;
                if (sl->pitched) {
                    char k[32]; snprintf(k, sizeof(k), "slot.%d.pitched", s);
                    _db(f, k, true);
                }
                if (!sl->oneShot) {
                    char k[32]; snprintf(k, sizeof(k), "slot.%d.loop", s);
                    _db(f, k, true);
                }
            }
        }
    }

    // Patterns (only non-empty)
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) {
        const Pattern *p = &seq.patterns[i];
        bool empty = true;
        for (int s = 0; s < p->length && empty; s++)
            if (p->steps[s].noteCount > 0) empty = false;
        if (p->plockCount > 0) empty = false;
        if (!empty) _dwWritePattern(f, i, p);
    }

    fclose(f);
    return true;
}

// ============================================================================
// LOAD HELPERS — read aliases from file_helpers.h
// ============================================================================

#define _dpf fileParseFloat
#define _dpi fileParseInt
#define _dpb fileParseBool
#define _dwStrip fileStrip
#define _dwStripQuotes fileStripQuotes

static int _dwLookupWave(const char *name) {
    int idx = fileLookupName(name, _dwWaveNames, _dwWaveCount);
    return idx >= 0 ? idx : atoi(name);
}

static void _dwApplyPatchKV(SynthPatch *p, const char *key, const char *val) {
    if (strcmp(key,"name")==0) { char t[64]; strncpy(t,val,63); t[63]=0; _dwStripQuotes(t); strncpy(p->p_name,t,31); p->p_name[31]=0; }
    else if (strcmp(key,"waveType")==0) p->p_waveType = _dwLookupWave(val);
    else if (strcmp(key,"scwIndex")==0) p->p_scwIndex = _dpi(val);
    else if (strcmp(key,"envelopeEnabled")==0) p->p_envelopeEnabled = _dpb(val);
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
    else if (strcmp(key,"filterEnabled")==0) p->p_filterEnabled = _dpb(val);
    else if (strcmp(key,"filterCutoff")==0) p->p_filterCutoff = _dpf(val);
    else if (strcmp(key,"filterResonance")==0) p->p_filterResonance = _dpf(val);
    else if (strcmp(key,"filterType")==0) p->p_filterType = _dpi(val);
    else if (strcmp(key,"filterModel")==0) p->p_filterModel = _dpi(val);
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
    else if (strcmp(key,"filterLfoPhaseOffset")==0) p->p_filterLfoPhaseOffset = _dpf(val);
    else if (strcmp(key,"resoLfoPhaseOffset")==0) p->p_resoLfoPhaseOffset = _dpf(val);
    else if (strcmp(key,"ampLfoPhaseOffset")==0) p->p_ampLfoPhaseOffset = _dpf(val);
    else if (strcmp(key,"pitchLfoPhaseOffset")==0) p->p_pitchLfoPhaseOffset = _dpf(val);
    else if (strcmp(key,"arpEnabled")==0) p->p_arpEnabled = _dpb(val);
    else if (strcmp(key,"arpMode")==0) p->p_arpMode = _dpi(val);
    else if (strcmp(key,"arpRateDiv")==0) p->p_arpRateDiv = _dpi(val);
    else if (strcmp(key,"arpRate")==0) p->p_arpRate = _dpf(val);
    else if (strcmp(key,"arpChord")==0) p->p_arpChord = _dpi(val);
    else if (strcmp(key,"unisonCount")==0) p->p_unisonCount = _dpi(val);
    else if (strcmp(key,"unisonDetune")==0) p->p_unisonDetune = _dpf(val);
    else if (strcmp(key,"unisonMix")==0) p->p_unisonMix = _dpf(val);
    else if (strcmp(key,"monoMode")==0) p->p_monoMode = _dpb(val);
    else if (strcmp(key,"monoRetrigger")==0) p->p_monoRetrigger = _dpb(val);
    else if (strcmp(key,"monoHardRetrigger")==0) p->p_monoHardRetrigger = _dpb(val);
    else if (strcmp(key,"glideTime")==0) p->p_glideTime = _dpf(val);
    else if (strcmp(key,"legatoWindow")==0) p->p_legatoWindow = _dpf(val);
    else if (strcmp(key,"notePriority")==0) p->p_notePriority = _dpi(val);
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
    else if (strcmp(key,"granularSamplerSlot")==0) p->p_granularSamplerSlot = _dpi(val);
    else if (strcmp(key,"granularGrainSize")==0) p->p_granularGrainSize = _dpf(val);
    else if (strcmp(key,"granularDensity")==0) p->p_granularDensity = _dpf(val);
    else if (strcmp(key,"granularPosition")==0) p->p_granularPosition = _dpf(val);
    else if (strcmp(key,"granularPosRandom")==0) p->p_granularPosRandom = _dpf(val);
    else if (strcmp(key,"granularPitch")==0) p->p_granularPitch = _dpf(val);
    else if (strcmp(key,"granularPitchRandom")==0) p->p_granularPitchRandom = _dpf(val);
    else if (strcmp(key,"granularAmpRandom")==0) p->p_granularAmpRandom = _dpf(val);
    else if (strcmp(key,"granularSpread")==0) p->p_granularSpread = _dpf(val);
    else if (strcmp(key,"granularFreeze")==0) p->p_granularFreeze = _dpb(val);
    else if (strcmp(key,"fmLfoRate")==0) p->p_fmLfoRate = _dpf(val);
    else if (strcmp(key,"fmLfoDepth")==0) p->p_fmLfoDepth = _dpf(val);
    else if (strcmp(key,"fmLfoShape")==0) p->p_fmLfoShape = _dpi(val);
    else if (strcmp(key,"fmLfoSync")==0) p->p_fmLfoSync = _dpi(val);
    else if (strcmp(key,"fmLfoPhaseOffset")==0) p->p_fmLfoPhaseOffset = _dpf(val);
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
    else if (strcmp(key,"metallicPreset")==0) p->p_metallicPreset = _dpi(val);
    else if (strcmp(key,"metallicRingMix")==0) p->p_metallicRingMix = _dpf(val);
    else if (strcmp(key,"metallicNoiseLevel")==0) p->p_metallicNoiseLevel = _dpf(val);
    else if (strcmp(key,"metallicBrightness")==0) p->p_metallicBrightness = _dpf(val);
    else if (strcmp(key,"metallicPitchEnv")==0) p->p_metallicPitchEnv = _dpf(val);
    else if (strcmp(key,"metallicPitchEnvDecay")==0) p->p_metallicPitchEnvDecay = _dpf(val);
    else if (strcmp(key,"bowPressure")==0) p->p_bowPressure = _dpf(val);
    else if (strcmp(key,"bowSpeed")==0) p->p_bowSpeed = _dpf(val);
    else if (strcmp(key,"bowPosition")==0) p->p_bowPosition = _dpf(val);
    else if (strcmp(key,"pipeBreath")==0) p->p_pipeBreath = _dpf(val);
    else if (strcmp(key,"pipeEmbouchure")==0) p->p_pipeEmbouchure = _dpf(val);
    else if (strcmp(key,"pipeBore")==0) p->p_pipeBore = _dpf(val);
    else if (strcmp(key,"pipeOverblow")==0) p->p_pipeOverblow = _dpf(val);
    else if (strcmp(key,"reedStiffness")==0) p->p_reedStiffness = _dpf(val);
    else if (strcmp(key,"reedAperture")==0) p->p_reedAperture = _dpf(val);
    else if (strcmp(key,"reedBlowPressure")==0) p->p_reedBlowPressure = _dpf(val);
    else if (strcmp(key,"reedBore")==0) p->p_reedBore = _dpf(val);
    else if (strcmp(key,"reedVibratoDepth")==0) p->p_reedVibratoDepth = _dpf(val);
    else if (strcmp(key,"brassBlowPressure")==0) p->p_brassBlowPressure = _dpf(val);
    else if (strcmp(key,"brassLipTension")==0) p->p_brassLipTension = _dpf(val);
    else if (strcmp(key,"brassLipAperture")==0) p->p_brassLipAperture = _dpf(val);
    else if (strcmp(key,"brassBore")==0) p->p_brassBore = _dpf(val);
    else if (strcmp(key,"brassMute")==0) p->p_brassMute = _dpf(val);
    else if (strcmp(key,"guitarPreset")==0) p->p_guitarPreset = _dpi(val);
    else if (strcmp(key,"guitarBodyMix")==0) p->p_guitarBodyMix = _dpf(val);
    else if (strcmp(key,"guitarBodyBrightness")==0) p->p_guitarBodyBrightness = _dpf(val);
    else if (strcmp(key,"guitarPickPosition")==0) p->p_guitarPickPosition = _dpf(val);
    else if (strcmp(key,"guitarBuzz")==0) p->p_guitarBuzz = _dpf(val);
    else if (strcmp(key,"mandolinPreset")==0) p->p_mandolinPreset = _dpi(val);
    else if (strcmp(key,"mandolinBodyMix")==0) p->p_mandolinBodyMix = _dpf(val);
    else if (strcmp(key,"mandolinCourseDetune")==0) p->p_mandolinCourseDetune = _dpf(val);
    else if (strcmp(key,"mandolinPickPosition")==0) p->p_mandolinPickPosition = _dpf(val);
    else if (strcmp(key,"whistlePreset")==0) p->p_whistlePreset = _dpi(val);
    else if (strcmp(key,"whistleBreath")==0) p->p_whistleBreath = _dpf(val);
    else if (strcmp(key,"whistleNoiseGain")==0) p->p_whistleNoiseGain = _dpf(val);
    else if (strcmp(key,"whistleFippleFreqMod")==0) p->p_whistleFippleFreqMod = _dpf(val);
    else if (strcmp(key,"stifkarpPreset")==0) p->p_stifkarpPreset = _dpi(val);
    else if (strcmp(key,"stifkarpHardness")==0) p->p_stifkarpHardness = _dpf(val);
    else if (strcmp(key,"stifkarpStiffness")==0) p->p_stifkarpStiffness = _dpf(val);
    else if (strcmp(key,"stifkarpStrikePos")==0) p->p_stifkarpStrikePos = _dpf(val);
    else if (strcmp(key,"stifkarpBodyMix")==0) p->p_stifkarpBodyMix = _dpf(val);
    else if (strcmp(key,"stifkarpBodyBrightness")==0) p->p_stifkarpBodyBrightness = _dpf(val);
    else if (strcmp(key,"stifkarpDamper")==0) p->p_stifkarpDamper = _dpf(val);
    else if (strcmp(key,"stifkarpSympathetic")==0) p->p_stifkarpSympathetic = _dpf(val);
    else if (strcmp(key,"stifkarpDetune")==0) p->p_stifkarpDetune = _dpf(val);
    else if (strcmp(key,"shakerPreset")==0) p->p_shakerPreset = _dpi(val);
    else if (strcmp(key,"shakerParticles")==0) p->p_shakerParticles = _dpf(val);
    else if (strcmp(key,"shakerDecayRate")==0) p->p_shakerDecayRate = _dpf(val);
    else if (strcmp(key,"shakerResonance")==0) p->p_shakerResonance = _dpf(val);
    else if (strcmp(key,"shakerBrightness")==0) p->p_shakerBrightness = _dpf(val);
    else if (strcmp(key,"shakerScrape")==0) p->p_shakerScrape = _dpf(val);
    else if (strcmp(key,"bandedwgPreset")==0) p->p_bandedwgPreset = _dpi(val);
    else if (strcmp(key,"bandedwgBowPressure")==0) p->p_bandedwgBowPressure = _dpf(val);
    else if (strcmp(key,"bandedwgBowSpeed")==0) p->p_bandedwgBowSpeed = _dpf(val);
    else if (strcmp(key,"bandedwgStrikePos")==0) p->p_bandedwgStrikePos = _dpf(val);
    else if (strcmp(key,"bandedwgBrightness")==0) p->p_bandedwgBrightness = _dpf(val);
    else if (strcmp(key,"bandedwgSustain")==0) p->p_bandedwgSustain = _dpf(val);
    else if (strcmp(key,"vfPhoneme")==0) p->p_vfPhoneme = _dpi(val);
    else if (strcmp(key,"vfPhonemeTarget")==0) p->p_vfPhonemeTarget = _dpi(val);
    else if (strcmp(key,"vfMorphRate")==0) p->p_vfMorphRate = _dpf(val);
    else if (strcmp(key,"vfAspiration")==0) p->p_vfAspiration = _dpf(val);
    else if (strcmp(key,"vfOpenQuotient")==0) p->p_vfOpenQuotient = _dpf(val);
    else if (strcmp(key,"vfSpectralTilt")==0) p->p_vfSpectralTilt = _dpf(val);
    else if (strcmp(key,"vfFormantShift")==0) p->p_vfFormantShift = _dpf(val);
    else if (strcmp(key,"vfVibratoDepth")==0) p->p_vfVibratoDepth = _dpf(val);
    else if (strcmp(key,"vfVibratoRate")==0) p->p_vfVibratoRate = _dpf(val);
    else if (strcmp(key,"vfConsonant")==0) p->p_vfConsonant = _dpi(val);
    else if (strcmp(key,"birdType")==0) p->p_birdType = _dpi(val);
    else if (strcmp(key,"birdChirpRange")==0) p->p_birdChirpRange = _dpf(val);
    else if (strcmp(key,"birdTrillRate")==0) p->p_birdTrillRate = _dpf(val);
    else if (strcmp(key,"birdTrillDepth")==0) p->p_birdTrillDepth = _dpf(val);
    else if (strcmp(key,"birdAmRate")==0) p->p_birdAmRate = _dpf(val);
    else if (strcmp(key,"birdAmDepth")==0) p->p_birdAmDepth = _dpf(val);
    else if (strcmp(key,"birdHarmonics")==0) p->p_birdHarmonics = _dpf(val);
    else if (strcmp(key,"epHardness")==0) p->p_epHardness = _dpf(val);
    else if (strcmp(key,"epToneBar")==0) p->p_epToneBar = _dpf(val);
    else if (strcmp(key,"epPickupPos")==0) p->p_epPickupPos = _dpf(val);
    else if (strcmp(key,"epPickupDist")==0) p->p_epPickupDist = _dpf(val);
    else if (strcmp(key,"epDecay")==0) p->p_epDecay = _dpf(val);
    else if (strcmp(key,"epBell")==0) p->p_epBell = _dpf(val);
    else if (strcmp(key,"epBellTone")==0) p->p_epBellTone = _dpf(val);
    else if (strcmp(key,"epPickupType")==0) p->p_epPickupType = _dpi(val);
    else if (strcmp(key,"epRatioSet")==0) p->p_epRatioSet = _dpi(val);
    else if (strncmp(key,"orgDrawbar",10)==0 && key[10]>='0' && key[10]<='8' && key[11]=='\0') p->p_orgDrawbar[key[10]-'0'] = _dpf(val);
    else if (strcmp(key,"orgClick")==0) p->p_orgClick = _dpf(val);
    else if (strcmp(key,"orgCrosstalk")==0) p->p_orgCrosstalk = _dpf(val);
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
    else if (strcmp(key,"osc2Decay")==0) p->p_osc2Decay = _dpf(val);
    else if (strcmp(key,"osc3Ratio")==0) p->p_osc3Ratio = _dpf(val);
    else if (strcmp(key,"osc3Level")==0) p->p_osc3Level = _dpf(val);
    else if (strcmp(key,"osc3Decay")==0) p->p_osc3Decay = _dpf(val);
    else if (strcmp(key,"osc4Ratio")==0) p->p_osc4Ratio = _dpf(val);
    else if (strcmp(key,"osc4Level")==0) p->p_osc4Level = _dpf(val);
    else if (strcmp(key,"osc4Decay")==0) p->p_osc4Decay = _dpf(val);
    else if (strcmp(key,"osc5Ratio")==0) p->p_osc5Ratio = _dpf(val);
    else if (strcmp(key,"osc5Level")==0) p->p_osc5Level = _dpf(val);
    else if (strcmp(key,"osc5Decay")==0) p->p_osc5Decay = _dpf(val);
    else if (strcmp(key,"osc6Ratio")==0) p->p_osc6Ratio = _dpf(val);
    else if (strcmp(key,"osc6Level")==0) p->p_osc6Level = _dpf(val);
    else if (strcmp(key,"osc6Decay")==0) p->p_osc6Decay = _dpf(val);
    else if (strcmp(key,"oscVelSens")==0) p->p_oscVelSens = _dpf(val);
    else if (strcmp(key,"velToFilter")==0) p->p_velToFilter = _dpf(val);
    else if (strcmp(key,"velToClick")==0) p->p_velToClick = _dpf(val);
    else if (strcmp(key,"velToDrive")==0) p->p_velToDrive = _dpf(val);
    else if (strcmp(key,"drive")==0) p->p_drive = _dpf(val);
    else if (strcmp(key,"driveMode")==0) p->p_driveMode = _dpi(val);
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
    else if (strcmp(key,"analogVariance")==0) p->p_analogVariance = _dpb(val);
    else if (strcmp(key,"ringMod")==0) p->p_ringMod = _dpb(val);
    else if (strcmp(key,"ringModFreq")==0) p->p_ringModFreq = _dpf(val);
    else if (strcmp(key,"wavefoldAmount")==0) p->p_wavefoldAmount = _dpf(val);
    else if (strcmp(key,"hardSync")==0) p->p_hardSync = _dpb(val);
    else if (strcmp(key,"hardSyncRatio")==0) p->p_hardSyncRatio = _dpf(val);
    // 303 acid mode
    else if (strcmp(key,"acidMode")==0) p->p_acidMode = _dpb(val);
    else if (strcmp(key,"accentSweepAmt")==0) p->p_accentSweepAmt = _dpf(val);
    else if (strcmp(key,"gimmickDipAmt")==0) p->p_gimmickDipAmt = _dpf(val);
    // Per-voice formant filter
    else if (strcmp(key,"formantEnabled")==0) p->p_formantEnabled = _dpb(val);
    else if (strcmp(key,"formantFrom")==0) p->p_formantFrom = _dpi(val);
    else if (strcmp(key,"formantTo")==0) p->p_formantTo = _dpi(val);
    else if (strcmp(key,"formantMorphTime")==0) p->p_formantMorphTime = _dpf(val);
    else if (strcmp(key,"formantShift")==0) p->p_formantShift = _dpf(val);
    else if (strcmp(key,"formantQ")==0) p->p_formantQ = _dpf(val);
    else if (strcmp(key,"formantMix")==0) p->p_formantMix = _dpf(val);
    else if (strcmp(key,"formantRandom")==0) p->p_formantRandom = _dpb(val);
    else if (strcmp(key,"formantMode")==0) p->p_formantMode = _dpi(val);
    else if (strcmp(key,"fbBaseFreq")==0) p->p_fbBaseFreq = _dpf(val);
    else if (strcmp(key,"fbSpacing")==0) p->p_fbSpacing = _dpf(val);
    else if (strcmp(key,"fbAlpha")==0) p->p_fbAlpha = _dpf(val);
    else if (strcmp(key,"fbBeta")==0) p->p_fbBeta = _dpf(val);
    else if (strcmp(key,"fbQ")==0) p->p_fbQ = _dpf(val);
    else if (strcmp(key,"fbQ1")==0) p->p_fbQ = _dpf(val);
    else if (strcmp(key,"fbQ2")==0) {}
    else if (strcmp(key,"fbQ3")==0) {}
    else if (strcmp(key,"fbKeyTrack")==0) p->p_fbKeyTrack = _dpf(val);
    else if (strcmp(key,"fbMorphOsc")==0) p->p_fbMorphOsc = _dpf(val);
    else if (strcmp(key,"fbLayout")==0) {}
    else if (strcmp(key,"fbRandomize")==0) p->p_fbRandomize = _dpf(val);
    else if (strcmp(key,"fbEnvAlpha")==0) p->p_fbEnvAlpha = _dpf(val);
    else if (strcmp(key,"fbLfoRate")==0) p->p_fbLfoRate = _dpf(val);
    else if (strcmp(key,"fbLfoSync")==0) p->p_fbLfoSync = _dpi(val);
    else if (strcmp(key,"fbLfoAlpha")==0) p->p_fbLfoAlpha = _dpf(val);
    else if (strcmp(key,"fbLfoBeta")==0) p->p_fbLfoBeta = _dpf(val);
    else if (strcmp(key,"fbLfoShape")==0) p->p_fbLfoShape = _dpi(val);
    else if (strcmp(key,"fbNoiseMix")==0) p->p_fbNoiseMix = _dpf(val);
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

// Extract track= value from event line (for legacy multi-track migration)
static int _dwExtractTrack(const char *line) {
    const char *p = strstr(line, "track=");
    if (!p) return 0;
    return atoi(p + 6);
}

static void _dwParseDrumEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[32]; int n = _dwTokenize(lc+1, toks, 32);
    int step=0; float vel=0.8f, pitch=0.0f, prob=1.0f; int cond=COND_ALWAYS;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"step")==0) step=_dpi(v);
            else if (strcmp(k,"vel")==0) vel=_dpf(v);
            else if (strcmp(k,"pitch")==0) pitch=_dpf(v);
            else if (strcmp(k,"prob")==0) prob=_dpf(v);
            else if (strcmp(k,"cond")==0) { int idx=_dwLookupName(v,_dwCondNames,11); if(idx>=0) cond=idx; }
            // "track=" ignored — pattern is single-track
        }
    }
    if (step>=0&&step<SEQ_MAX_STEPS) {
        patSetDrum(p, step, vel, pitch);
        patSetDrumProb(p, step, prob);
        patSetDrumCond(p, step, cond);
    }
}

static void _dwParseSamplerEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[32]; int n = _dwTokenize(lc+1, toks, 32);
    int step=0, slice=0, note=60, nudge=0; float vel=0.8f, prob=1.0f; int cond=COND_ALWAYS;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"step")==0) step=_dpi(v);
            else if (strcmp(k,"slice")==0) slice=_dpi(v);
            else if (strcmp(k,"note")==0) note=_dpi(v);
            else if (strcmp(k,"vel")==0) vel=_dpf(v);
            else if (strcmp(k,"nudge")==0) nudge=_dpi(v);
            else if (strcmp(k,"prob")==0) prob=_dpf(v);
            else if (strcmp(k,"cond")==0) { int idx=_dwLookupName(v,_dwCondNames,11); if(idx>=0) cond=idx; }
        }
    }
    if (step>=0&&step<SEQ_MAX_STEPS) {
        StepV2 *sv = &p->steps[step];
        if (sv->noteCount < SEQ_V2_MAX_POLY) {
            int vi = sv->noteCount++;
            sv->notes[vi].slice = (uint8_t)slice;
            sv->notes[vi].note = (int8_t)note;
            sv->notes[vi].velocity = velFloatToU8(vel);
            sv->notes[vi].nudge = (int8_t)nudge;
            if (vi == 0) {
                sv->probability = probFloatToU8(prob);
                sv->condition = (uint8_t)cond;
            }
        }
    }
}

static void _dwParseMelodyEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[32]; int n = _dwTokenize(lc+1, toks, 32);
    int step=0, gate=4, sustain=0; char noteName[16]="C4";
    int nudge=0, gateNudge=0;
    float vel=0.8f, prob=1.0f; int cond=COND_ALWAYS;
    int pickMode=PICK_ALL;
    bool slide=false, accent=false, hasChord=false;
    int chordType=CHORD_SINGLE;
    int customNotes[8]={0}; int customNoteCount=0;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"step")==0) step=_dpi(v);
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
            // "track=" ignored — pattern is single-track
        } else {
            if (strcmp(k,"slide")==0) slide=true;
            else if (strcmp(k,"accent")==0) accent=true;
        }
    }
    if (step>=0&&step<SEQ_MAX_STEPS) {
        if (hasChord && chordType==CHORD_CUSTOM && customNoteCount>0) {
            patSetChordCustom(p, step, vel, gate,
                customNoteCount>0?customNotes[0]:-1, customNoteCount>1?customNotes[1]:-1,
                customNoteCount>2?customNotes[2]:-1, customNoteCount>3?customNotes[3]:-1);
        } else if (hasChord) {
            patSetChord(p, step, _dwNameToMidi(noteName), (ChordType)chordType, vel, gate);
        } else {
            // v2 polyphony: if step already has notes, add to it
            StepV2 *sv = &p->steps[step];
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
            patSetNote(p, step, _dwNameToMidi(noteName), vel, gate);
        }
        StepV2 *sv = &p->steps[step];
        if (sv->noteCount > 0) {
            sv->notes[0].slide = slide;
            sv->notes[0].accent = accent;
            sv->notes[0].nudge = (int8_t)nudge;
            sv->notes[0].gateNudge = (int8_t)gateNudge;
        }
        patSetNoteSustain(p, step, sustain);
        patSetNoteProb(p, step, prob);
        patSetNoteCond(p, step, cond);
        if (pickMode != PICK_ALL) patSetPickMode(p, step, pickMode);
    }
}

static void _dwParsePlockEvent(const char *line, Pattern *p) {
    char lc[512]; strncpy(lc,line,511); lc[511]='\0';
    char *toks[16]; int n = _dwTokenize(lc+1, toks, 16);
    int step=0, param=-1; float value=0.0f;
    char k[32], v[64];
    for (int i=0; i<n; i++) {
        if (_dwParseKV(toks[i],k,32,v,64)) {
            if (strcmp(k,"step")==0) step=_dpi(v);
            else if (strcmp(k,"param")==0) { int idx=_dwLookupName(v,_dwPlockNames,PLOCK_COUNT); param=idx>=0?idx:_dpi(v); }
            else if (strcmp(k,"val")==0) value=_dpf(v);
            // "track=" ignored — pattern is single-track
        }
    }
    if (param>=0 && param<PLOCK_COUNT) seqSetPLock(p, 0, step, param, value);
}

// ============================================================================
// dawLoad
// ============================================================================

typedef enum {
    _DW_SEC_NONE, _DW_SEC_SONG, _DW_SEC_SCALE, _DW_SEC_GROOVE,
    _DW_SEC_SETTINGS, _DW_SEC_PATCH, _DW_SEC_MIXER, _DW_SEC_SIDECHAIN,
    _DW_SEC_MASTERFX, _DW_SEC_TAPEFX, _DW_SEC_CROSSFADER,
    _DW_SEC_SPLIT, _DW_SEC_ARRANGEMENT, _DW_SEC_PATTERN,
    _DW_SEC_SAMPLE, _DW_SEC_PERTRACK, _DW_SEC_LAUNCHER,
    _DW_SEC_SLOTFLAGS,
} _DwSection;

static bool dawLoad(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    // Temp storage for scratch markers parsed from [sample] section
    int _loadedMarkers[SCRATCH_MAX_MARKERS];
    int _loadedMarkerCount = 0;

    // Stop playback
    daw.transport.playing = false;

    // Clear chop state so stale sourcePath doesn't trigger a re-bounce.
    // If the file has a [sample] section, the parser will repopulate it.
    // Skip during offline bounce — renderPatternToBuffer calls dawLoad internally.
#ifdef DAW_HAS_CHOP_STATE
    if (!_chopBounceActive) chopState.sourcePath[0] = '\0';
#endif

    // Reset patterns
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) initPattern(&seq.patterns[i]);

    // Reset arrangement to empty (backward compat: old files won't have [pertrack])
    daw.arr.arrMode = false;
    daw.arr.length = 0;
    for (int _b = 0; _b < ARR_MAX_BARS; _b++)
        for (int _t = 0; _t < ARR_MAX_TRACKS; _t++)
            daw.arr.cells[_b][_t] = ARR_EMPTY;

    _DwSection section = _DW_SEC_NONE;
    int subIndex = -1;
    char line[512];
    // Legacy multi-track migration: base pattern index for expanded per-track patterns
    // -1 = new format; N*SEQ_V2_MAX_TRACKS = old pattern N is being expanded
    int _legacyBase = -1;
    bool _anyLegacy = false;

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
            else if (strcmp(sec,"pertrack")==0) {
                section = _DW_SEC_PERTRACK;
                // Initialize arrangement to empty
                memset(&daw.arr, 0, sizeof(daw.arr));
                for (int _b = 0; _b < ARR_MAX_BARS; _b++)
                    for (int _t = 0; _t < ARR_MAX_TRACKS; _t++)
                        daw.arr.cells[_b][_t] = ARR_EMPTY;
            }
            else if (strcmp(sec,"launcher")==0) {
                section = _DW_SEC_LAUNCHER;
                // Reset launcher
                daw.launcher.active = false;
                for (int _t = 0; _t < ARR_MAX_TRACKS; _t++) {
                    daw.launcher.tracks[_t].playingSlot = -1;
                    daw.launcher.tracks[_t].queuedSlot = -1;
                    daw.launcher.tracks[_t].loopCount = 0;
                    for (int _s = 0; _s < LAUNCHER_MAX_SLOTS; _s++) {
                        daw.launcher.tracks[_t].pattern[_s] = -1;
                        daw.launcher.tracks[_t].state[_s] = CLIP_EMPTY;
                        daw.launcher.tracks[_t].nextAction[_s] = NEXT_ACTION_LOOP;
                        daw.launcher.tracks[_t].nextActionLoops[_s] = 0;
                    }
                }
            }
            else if (strcmp(sec,"slotflags")==0) {
                section = _DW_SEC_SLOTFLAGS;
            }
            else if (strcmp(sec,"sample")==0) {
                section = _DW_SEC_SAMPLE;
            }
            else if (strncmp(sec,"patch.",6)==0) {
                section = _DW_SEC_PATCH;
                subIndex = atoi(sec + 6);
                if (subIndex >= 0 && subIndex < NUM_PATCHES)
                    daw.patches[subIndex] = createDefaultPatch(WAVE_SAW);
            }
            else if (strncmp(sec,"pattern.",8)==0) {
                section = _DW_SEC_PATTERN;
                subIndex = atoi(sec + 8);
                _legacyBase = -1;  // reset on each new pattern section
                // In legacy mode, sub-patterns were already expanded into the flat pool;
                // don't re-init them here (drumTrackLength inits the correct sub-patterns).
                if (!_anyLegacy && subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS)
                    initPattern(&seq.patterns[subIndex]);
            }
            else section = _DW_SEC_NONE;
            continue;
        }

        // Pattern events
        if (section == _DW_SEC_PATTERN && subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
            if (_legacyBase >= 0) {
                // Legacy multi-track format: route by track= field to per-track sub-patterns
                if (s[0]=='d' && s[1]==' ') {
                    int t = _dwExtractTrack(s);
                    int ni = _legacyBase + t;
                    if (t >= 0 && t < SEQ_DRUM_TRACKS && ni < SEQ_NUM_PATTERNS) {
                        seq.patterns[ni].trackType = TRACK_DRUM;
                        _dwParseDrumEvent(s, &seq.patterns[ni]);
                    }
                    continue;
                }
                if (s[0]=='m' && s[1]==' ') {
                    int t = _dwExtractTrack(s);
                    int ni = _legacyBase + SEQ_DRUM_TRACKS + t;
                    if (t >= 0 && t < SEQ_MELODY_TRACKS && ni < SEQ_NUM_PATTERNS) {
                        seq.patterns[ni].trackType = TRACK_MELODIC;
                        _dwParseMelodyEvent(s, &seq.patterns[ni]);
                    }
                    continue;
                }
                if (s[0]=='s' && s[1]==' ') {
                    int ni = _legacyBase + SEQ_TRACK_SAMPLER;
                    if (ni < SEQ_NUM_PATTERNS) {
                        seq.patterns[ni].trackType = TRACK_SAMPLER;
                        _dwParseSamplerEvent(s, &seq.patterns[ni]);
                    }
                    continue;
                }
                if (s[0]=='p' && s[1]==' ') {
                    int t = _dwExtractTrack(s);
                    int ni = (t < SEQ_DRUM_TRACKS) ? _legacyBase + t
                           : (t < SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS) ? _legacyBase + t
                           : _legacyBase + SEQ_TRACK_SAMPLER;
                    if (ni >= 0 && ni < SEQ_NUM_PATTERNS)
                        _dwParsePlockEvent(s, &seq.patterns[ni]);
                    continue;
                }
            } else {
                // Check for old multi-track format: "d track=X step=Y" or "m track=X step=Y"
                // (new single-track format uses "d step=Y" without track= field)
                bool _hasTrackField = (strstr(s, "track=") != NULL);
                if (_hasTrackField && (s[0]=='d'||s[0]=='m'||s[0]=='s'||s[0]=='p')) {
                    // Route as legacy: expand into per-track sub-patterns
                    _legacyBase = subIndex * SEQ_V2_MAX_TRACKS;
                    _anyLegacy = true;
                    int t = _dwExtractTrack(s);
                    if (s[0]=='d') {
                        int ni = _legacyBase + t;
                        if (t >= 0 && t < SEQ_DRUM_TRACKS && ni < SEQ_NUM_PATTERNS) {
                            seq.patterns[ni].trackType = TRACK_DRUM;
                            _dwParseDrumEvent(s, &seq.patterns[ni]);
                        }
                    } else if (s[0]=='m') {
                        int ni = _legacyBase + SEQ_DRUM_TRACKS + t;
                        if (t >= 0 && t < SEQ_MELODY_TRACKS && ni < SEQ_NUM_PATTERNS) {
                            seq.patterns[ni].trackType = TRACK_MELODIC;
                            _dwParseMelodyEvent(s, &seq.patterns[ni]);
                        }
                    } else if (s[0]=='s') {
                        int ni = _legacyBase + SEQ_TRACK_SAMPLER;
                        if (ni < SEQ_NUM_PATTERNS) {
                            seq.patterns[ni].trackType = TRACK_SAMPLER;
                            _dwParseSamplerEvent(s, &seq.patterns[ni]);
                        }
                    } else if (s[0]=='p') {
                        int ni = (t < SEQ_DRUM_TRACKS) ? _legacyBase + t
                               : (t < SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS) ? _legacyBase + t
                               : _legacyBase + SEQ_TRACK_SAMPLER;
                        if (ni >= 0 && ni < SEQ_NUM_PATTERNS)
                            _dwParsePlockEvent(s, &seq.patterns[ni]);
                    }
                    continue;
                }
                if (s[0]=='d' && s[1]==' ') { _dwParseDrumEvent(s, &seq.patterns[subIndex]); continue; }
                if (s[0]=='m' && s[1]==' ') { _dwParseMelodyEvent(s, &seq.patterns[subIndex]); continue; }
                if (s[0]=='s' && s[1]==' ') { _dwParseSamplerEvent(s, &seq.patterns[subIndex]); continue; }
                if (s[0]=='p' && s[1]==' ') { _dwParsePlockEvent(s, &seq.patterns[subIndex]); continue; }
            }
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
            else if (strcmp(key,"songName")==0) { char t[64]; strncpy(t,val,63); t[63]=0; _dwStripQuotes(t); strncpy(daw.songName,t,63); daw.songName[63]=0; }
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
            else if (strncmp(key,"trackSwing",10)==0) {
                int t = atoi(key + 10);
                if (t >= 0 && t < SEQ_V2_MAX_TRACKS) seq.trackSwing[t] = _dpi(val);
            }
            else if (strncmp(key,"trackTranspose",14)==0) {
                int t = atoi(key + 14);
                if (t >= 0 && t < SEQ_V2_MAX_TRACKS) seq.trackTranspose[t] = _dpi(val);
            }
            else if (strcmp(key,"melodyTimingJitter")==0) seq.humanize.timingJitter = _dpi(val);
            else if (strcmp(key,"melodyVelocityJitter")==0) seq.humanize.velocityJitter = _dpf(val);
            break;
        case _DW_SEC_SETTINGS:
            if (strcmp(key,"masterVol")==0) daw.masterVol = _dpf(val);
            else if (strcmp(key,"voiceRandomVowel")==0) daw.voiceRandomVowel = _dpb(val);
            else if (strcmp(key,"masterSpeed")==0) daw.masterSpeed = _dpf(val);
            else if (strcmp(key,"chromaticMode")==0) daw.chromaticMode = _dpb(val);
            else if (strcmp(key,"chromaticSample")==0) daw.chromaticSample = _dpi(val);
            else if (strcmp(key,"chromaticRoot")==0) daw.chromaticRootNote = _dpi(val);
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
                    else if (strcmp(base,"delaySend")==0) daw.mixer.delaySend[b]=_dpf(val);
                    else if (strcmp(base,"mute")==0) daw.mixer.mute[b]=_dpb(val);
                    else if (strcmp(base,"solo")==0) daw.mixer.solo[b]=_dpb(val);
                    else if (strcmp(base,"filterOn")==0) daw.mixer.filterOn[b]=_dpb(val);
                    else if (strcmp(base,"filterCut")==0) daw.mixer.filterCut[b]=_dpf(val);
                    else if (strcmp(base,"filterRes")==0) daw.mixer.filterRes[b]=_dpf(val);
                    else if (strcmp(base,"filterType")==0) daw.mixer.filterType[b]=_dpi(val);
                    else if (strcmp(base,"distOn")==0) daw.mixer.distOn[b]=_dpb(val);
                    else if (strcmp(base,"distDrive")==0) daw.mixer.distDrive[b]=_dpf(val);
                    else if (strcmp(base,"distMix")==0) daw.mixer.distMix[b]=_dpf(val);
                    else if (strcmp(base,"distMode")==0) daw.mixer.distMode[b]=_dpi(val);
                    else if (strcmp(base,"delayOn")==0) daw.mixer.delayOn[b]=_dpb(val);
                    else if (strcmp(base,"delaySync")==0) daw.mixer.delaySync[b]=_dpb(val);
                    else if (strcmp(base,"delaySyncDiv")==0) daw.mixer.delaySyncDiv[b]=_dpi(val);
                    else if (strcmp(base,"delayTime")==0) daw.mixer.delayTime[b]=_dpf(val);
                    else if (strcmp(base,"delayFB")==0) daw.mixer.delayFB[b]=_dpf(val);
                    else if (strcmp(base,"delayMix")==0) daw.mixer.delayMix[b]=_dpf(val);
                    else if (strcmp(base,"eqOn")==0) daw.mixer.eqOn[b]=_dpb(val);
                    else if (strcmp(base,"eqLowGain")==0) daw.mixer.eqLowGain[b]=_dpf(val);
                    else if (strcmp(base,"eqHighGain")==0) daw.mixer.eqHighGain[b]=_dpf(val);
                    else if (strcmp(base,"eqLowFreq")==0) daw.mixer.eqLowFreq[b]=_dpf(val);
                    else if (strcmp(base,"eqHighFreq")==0) daw.mixer.eqHighFreq[b]=_dpf(val);
                    else if (strcmp(base,"chorusOn")==0) daw.mixer.chorusOn[b]=_dpb(val);
                    else if (strcmp(base,"chorusBBD")==0) daw.mixer.chorusBBD[b]=_dpb(val);
                    else if (strcmp(base,"chorusRate")==0) daw.mixer.chorusRate[b]=_dpf(val);
                    else if (strcmp(base,"chorusDepth")==0) daw.mixer.chorusDepth[b]=_dpf(val);
                    else if (strcmp(base,"chorusMix")==0) daw.mixer.chorusMix[b]=_dpf(val);
                    else if (strcmp(base,"chorusDelay")==0) daw.mixer.chorusDelay[b]=_dpf(val);
                    else if (strcmp(base,"chorusFB")==0) daw.mixer.chorusFB[b]=_dpf(val);
                    else if (strcmp(base,"phaserOn")==0) daw.mixer.phaserOn[b]=_dpb(val);
                    else if (strcmp(base,"phaserRate")==0) daw.mixer.phaserRate[b]=_dpf(val);
                    else if (strcmp(base,"phaserDepth")==0) daw.mixer.phaserDepth[b]=_dpf(val);
                    else if (strcmp(base,"phaserMix")==0) daw.mixer.phaserMix[b]=_dpf(val);
                    else if (strcmp(base,"phaserFB")==0) daw.mixer.phaserFB[b]=_dpf(val);
                    else if (strcmp(base,"phaserStages")==0) daw.mixer.phaserStages[b]=_dpi(val);
                    else if (strcmp(base,"combOn")==0) daw.mixer.combOn[b]=_dpb(val);
                    else if (strcmp(base,"combFreq")==0) daw.mixer.combFreq[b]=_dpf(val);
                    else if (strcmp(base,"combFB")==0) daw.mixer.combFB[b]=_dpf(val);
                    else if (strcmp(base,"combMix")==0) daw.mixer.combMix[b]=_dpf(val);
                    else if (strcmp(base,"combDamping")==0) daw.mixer.combDamping[b]=_dpf(val);
                    else if (strcmp(base,"octaverOn")==0) daw.mixer.octaverOn[b]=_dpb(val);
                    else if (strcmp(base,"octaverMix")==0) daw.mixer.octaverMix[b]=_dpf(val);
                    else if (strcmp(base,"octaverSub")==0) daw.mixer.octaverSubLevel[b]=_dpf(val);
                    else if (strcmp(base,"octaverTone")==0) daw.mixer.octaverTone[b]=_dpf(val);
                    else if (strcmp(base,"tremoloOn")==0) daw.mixer.tremoloOn[b]=_dpb(val);
                    else if (strcmp(base,"tremoloRate")==0) daw.mixer.tremoloRate[b]=_dpf(val);
                    else if (strcmp(base,"tremoloDepth")==0) daw.mixer.tremoloDepth[b]=_dpf(val);
                    else if (strcmp(base,"tremoloShape")==0) daw.mixer.tremoloShape[b]=_dpi(val);
                    else if (strcmp(base,"leslieOn")==0) daw.mixer.leslieOn[b]=_dpb(val);
                    else if (strcmp(base,"leslieSpeed")==0) daw.mixer.leslieSpeed[b]=_dpi(val);
                    else if (strcmp(base,"leslieDrive")==0) daw.mixer.leslieDrive[b]=_dpf(val);
                    else if (strcmp(base,"leslieBal")==0) daw.mixer.leslieBalance[b]=_dpf(val);
                    else if (strcmp(base,"leslieDopp")==0) daw.mixer.leslieDoppler[b]=_dpf(val);
                    else if (strcmp(base,"leslieMix")==0) daw.mixer.leslieMix[b]=_dpf(val);
                    else if (strcmp(base,"wahOn")==0) daw.mixer.wahOn[b]=_dpb(val);
                    else if (strcmp(base,"wahMode")==0) daw.mixer.wahMode[b]=_dpi(val);
                    else if (strcmp(base,"wahRate")==0) daw.mixer.wahRate[b]=_dpf(val);
                    else if (strcmp(base,"wahSens")==0) daw.mixer.wahSensitivity[b]=_dpf(val);
                    else if (strcmp(base,"wahFreqLow")==0) daw.mixer.wahFreqLow[b]=_dpf(val);
                    else if (strcmp(base,"wahFreqHigh")==0) daw.mixer.wahFreqHigh[b]=_dpf(val);
                    else if (strcmp(base,"wahRes")==0) daw.mixer.wahResonance[b]=_dpf(val);
                    else if (strcmp(base,"wahMix")==0) daw.mixer.wahMix[b]=_dpf(val);
                    else if (strcmp(base,"ringModOn")==0) daw.mixer.ringModOn[b]=_dpb(val);
                    else if (strcmp(base,"ringModFreq")==0) daw.mixer.ringModFreq[b]=_dpf(val);
                    else if (strcmp(base,"ringModMix")==0) daw.mixer.ringModMix[b]=_dpf(val);
                    else if (strcmp(base,"compOn")==0) daw.mixer.compOn[b]=_dpb(val);
                    else if (strcmp(base,"compThresh")==0) daw.mixer.compThreshold[b]=_dpf(val);
                    else if (strcmp(base,"compRatio")==0) daw.mixer.compRatio[b]=_dpf(val);
                    else if (strcmp(base,"compAtk")==0) daw.mixer.compAttack[b]=_dpf(val);
                    else if (strcmp(base,"compRel")==0) daw.mixer.compRelease[b]=_dpf(val);
                    else if (strcmp(base,"compMakeup")==0) daw.mixer.compMakeup[b]=_dpf(val);
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
            else if (strcmp(key,"hpFreq")==0) daw.sidechain.hpFreq=_dpf(val);
            else if (strcmp(key,"envOn")==0) daw.sidechain.envOn=_dpb(val);
            else if (strcmp(key,"envSource")==0) daw.sidechain.envSource=_dpi(val);
            else if (strcmp(key,"envTarget")==0) daw.sidechain.envTarget=_dpi(val);
            else if (strcmp(key,"envDepth")==0) daw.sidechain.envDepth=_dpf(val);
            else if (strcmp(key,"envAttack")==0) daw.sidechain.envAttack=_dpf(val);
            else if (strcmp(key,"envHold")==0) daw.sidechain.envHold=_dpf(val);
            else if (strcmp(key,"envRelease")==0) daw.sidechain.envRelease=_dpf(val);
            else if (strcmp(key,"envCurve")==0) daw.sidechain.envCurve=_dpi(val);
            else if (strcmp(key,"envHPFreq")==0) daw.sidechain.envHPFreq=_dpf(val);
            break;
        case _DW_SEC_MASTERFX:
            if (strcmp(key,"octaverOn")==0) daw.masterFx.octaverOn=_dpb(val);
            else if (strcmp(key,"octaverMix")==0) daw.masterFx.octaverMix=_dpf(val);
            else if (strcmp(key,"octaverSubLevel")==0) daw.masterFx.octaverSubLevel=_dpf(val);
            else if (strcmp(key,"octaverTone")==0) daw.masterFx.octaverTone=_dpf(val);
            else if (strcmp(key,"tremoloOn")==0) daw.masterFx.tremoloOn=_dpb(val);
            else if (strcmp(key,"tremoloRate")==0) daw.masterFx.tremoloRate=_dpf(val);
            else if (strcmp(key,"tremoloDepth")==0) daw.masterFx.tremoloDepth=_dpf(val);
            else if (strcmp(key,"tremoloShape")==0) daw.masterFx.tremoloShape=_dpi(val);
            else if (strcmp(key,"leslieOn")==0) daw.masterFx.leslieOn=_dpb(val);
            else if (strcmp(key,"leslieSpeed")==0) daw.masterFx.leslieSpeed=_dpi(val);
            else if (strcmp(key,"leslieDrive")==0) daw.masterFx.leslieDrive=_dpf(val);
            else if (strcmp(key,"leslieBal")==0) daw.masterFx.leslieBalance=_dpf(val);
            else if (strcmp(key,"leslieDopp")==0) daw.masterFx.leslieDoppler=_dpf(val);
            else if (strcmp(key,"leslieMix")==0) daw.masterFx.leslieMix=_dpf(val);
            else if (strcmp(key,"wahOn")==0) daw.masterFx.wahOn=_dpb(val);
            else if (strcmp(key,"wahMode")==0) daw.masterFx.wahMode=_dpi(val);
            else if (strcmp(key,"wahRate")==0) daw.masterFx.wahRate=_dpf(val);
            else if (strcmp(key,"wahSensitivity")==0) daw.masterFx.wahSensitivity=_dpf(val);
            else if (strcmp(key,"wahFreqLow")==0) daw.masterFx.wahFreqLow=_dpf(val);
            else if (strcmp(key,"wahFreqHigh")==0) daw.masterFx.wahFreqHigh=_dpf(val);
            else if (strcmp(key,"wahResonance")==0) daw.masterFx.wahResonance=_dpf(val);
            else if (strcmp(key,"wahMix")==0) daw.masterFx.wahMix=_dpf(val);
            else if (strcmp(key,"distOn")==0) daw.masterFx.distOn=_dpb(val);
            else if (strcmp(key,"distDrive")==0) daw.masterFx.distDrive=_dpf(val);
            else if (strcmp(key,"distTone")==0) daw.masterFx.distTone=_dpf(val);
            else if (strcmp(key,"distMix")==0) daw.masterFx.distMix=_dpf(val);
            else if (strcmp(key,"distMode")==0) daw.masterFx.distMode=_dpi(val);
            else if (strcmp(key,"crushOn")==0) daw.masterFx.crushOn=_dpb(val);
            else if (strcmp(key,"crushBits")==0) daw.masterFx.crushBits=_dpf(val);
            else if (strcmp(key,"crushRate")==0) daw.masterFx.crushRate=_dpf(val);
            else if (strcmp(key,"crushMix")==0) daw.masterFx.crushMix=_dpf(val);
            else if (strcmp(key,"chorusOn")==0) daw.masterFx.chorusOn=_dpb(val);
            else if (strcmp(key,"chorusBBD")==0) daw.masterFx.chorusBBD=_dpb(val);
            else if (strcmp(key,"chorusRate")==0) daw.masterFx.chorusRate=_dpf(val);
            else if (strcmp(key,"chorusDepth")==0) daw.masterFx.chorusDepth=_dpf(val);
            else if (strcmp(key,"chorusMix")==0) daw.masterFx.chorusMix=_dpf(val);
            else if (strcmp(key,"flangerOn")==0) daw.masterFx.flangerOn=_dpb(val);
            else if (strcmp(key,"flangerRate")==0) daw.masterFx.flangerRate=_dpf(val);
            else if (strcmp(key,"flangerDepth")==0) daw.masterFx.flangerDepth=_dpf(val);
            else if (strcmp(key,"flangerFeedback")==0) daw.masterFx.flangerFeedback=_dpf(val);
            else if (strcmp(key,"flangerMix")==0) daw.masterFx.flangerMix=_dpf(val);
            else if (strcmp(key,"phaserOn")==0) daw.masterFx.phaserOn=_dpb(val);
            else if (strcmp(key,"phaserRate")==0) daw.masterFx.phaserRate=_dpf(val);
            else if (strcmp(key,"phaserDepth")==0) daw.masterFx.phaserDepth=_dpf(val);
            else if (strcmp(key,"phaserMix")==0) daw.masterFx.phaserMix=_dpf(val);
            else if (strcmp(key,"phaserFeedback")==0) daw.masterFx.phaserFeedback=_dpf(val);
            else if (strcmp(key,"phaserStages")==0) daw.masterFx.phaserStages=_dpi(val);
            else if (strcmp(key,"combOn")==0) daw.masterFx.combOn=_dpb(val);
            else if (strcmp(key,"combFreq")==0) daw.masterFx.combFreq=_dpf(val);
            else if (strcmp(key,"combFeedback")==0) daw.masterFx.combFeedback=_dpf(val);
            else if (strcmp(key,"combMix")==0) daw.masterFx.combMix=_dpf(val);
            else if (strcmp(key,"combDamping")==0) daw.masterFx.combDamping=_dpf(val);
            else if (strcmp(key,"ringModOn")==0) daw.masterFx.ringModOn=_dpb(val);
            else if (strcmp(key,"ringModFreq")==0) daw.masterFx.ringModFreq=_dpf(val);
            else if (strcmp(key,"ringModMix")==0) daw.masterFx.ringModMix=_dpf(val);
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
            else if (strcmp(key,"reverbFDN")==0) daw.masterFx.reverbFDN=_dpb(val);
            else if (strcmp(key,"reverbSize")==0) daw.masterFx.reverbSize=_dpf(val);
            else if (strcmp(key,"reverbDamping")==0) daw.masterFx.reverbDamping=_dpf(val);
            else if (strcmp(key,"reverbPreDelay")==0) daw.masterFx.reverbPreDelay=_dpf(val);
            else if (strcmp(key,"reverbMix")==0) daw.masterFx.reverbMix=_dpf(val);
            else if (strcmp(key,"reverbBass")==0) daw.masterFx.reverbBass=_dpf(val);
            else if (strcmp(key,"eqOn")==0) daw.masterFx.eqOn=_dpb(val);
            else if (strcmp(key,"eqLowGain")==0) daw.masterFx.eqLowGain=_dpf(val);
            else if (strcmp(key,"eqHighGain")==0) daw.masterFx.eqHighGain=_dpf(val);
            else if (strcmp(key,"eqLowFreq")==0) daw.masterFx.eqLowFreq=_dpf(val);
            else if (strcmp(key,"eqHighFreq")==0) daw.masterFx.eqHighFreq=_dpf(val);
            else if (strcmp(key,"subBassBoost")==0) daw.masterFx.subBassBoost=_dpb(val);
            else if (strcmp(key,"compOn")==0) daw.masterFx.compOn=_dpb(val);
            else if (strcmp(key,"compThreshold")==0) daw.masterFx.compThreshold=_dpf(val);
            else if (strcmp(key,"compRatio")==0) daw.masterFx.compRatio=_dpf(val);
            else if (strcmp(key,"compAttack")==0) daw.masterFx.compAttack=_dpf(val);
            else if (strcmp(key,"compRelease")==0) daw.masterFx.compRelease=_dpf(val);
            else if (strcmp(key,"compMakeup")==0) daw.masterFx.compMakeup=_dpf(val);
            else if (strcmp(key,"compKnee")==0) daw.masterFx.compKnee=_dpf(val);
            else if (strcmp(key,"mbOn")==0) daw.masterFx.mbOn=_dpb(val);
            else if (strcmp(key,"mbLowCross")==0) daw.masterFx.mbLowCross=_dpf(val);
            else if (strcmp(key,"mbHighCross")==0) daw.masterFx.mbHighCross=_dpf(val);
            else if (strcmp(key,"mbLowGain")==0) daw.masterFx.mbLowGain=_dpf(val);
            else if (strcmp(key,"mbMidGain")==0) daw.masterFx.mbMidGain=_dpf(val);
            else if (strcmp(key,"mbHighGain")==0) daw.masterFx.mbHighGain=_dpf(val);
            else if (strcmp(key,"mbLowDrive")==0) daw.masterFx.mbLowDrive=_dpf(val);
            else if (strcmp(key,"mbMidDrive")==0) daw.masterFx.mbMidDrive=_dpf(val);
            else if (strcmp(key,"mbHighDrive")==0) daw.masterFx.mbHighDrive=_dpf(val);
            else if (strcmp(key,"vinylOn")==0) daw.masterFx.vinylOn=_dpb(val);
            else if (strcmp(key,"vinylCrackle")==0) daw.masterFx.vinylCrackle=_dpf(val);
            else if (strcmp(key,"vinylNoise")==0) daw.masterFx.vinylNoise=_dpf(val);
            else if (strcmp(key,"vinylWarp")==0) daw.masterFx.vinylWarp=_dpf(val);
            else if (strcmp(key,"vinylWarpRate")==0) daw.masterFx.vinylWarpRate=_dpf(val);
            else if (strcmp(key,"vinylTone")==0) daw.masterFx.vinylTone=_dpf(val);
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
            else if (strcmp(key,"speedSlew")==0) daw.tapeFx.speedSlew=_dpf(val);
            else if (strcmp(key,"rewindTime")==0) daw.tapeFx.rewindTime=_dpf(val);
            else if (strcmp(key,"rewindMinSpeed")==0) daw.tapeFx.rewindMinSpeed=_dpf(val);
            else if (strcmp(key,"rewindVinyl")==0) daw.tapeFx.rewindVinyl=_dpf(val);
            else if (strcmp(key,"rewindWobble")==0) daw.tapeFx.rewindWobble=_dpf(val);
            else if (strcmp(key,"rewindFilter")==0) daw.tapeFx.rewindFilter=_dpf(val);
            else if (strcmp(key,"rewindCurve")==0) daw.tapeFx.rewindCurve=_dpi(val);
            else if (strcmp(key,"tapeStopTime")==0) daw.tapeFx.tapeStopTime=_dpf(val);
            else if (strcmp(key,"tapeStopCurve")==0) daw.tapeFx.tapeStopCurve=_dpi(val);
            else if (strcmp(key,"tapeStopSpinBack")==0) daw.tapeFx.tapeStopSpinBack=_dpb(val);
            else if (strcmp(key,"tapeStopSpinTime")==0) daw.tapeFx.tapeStopSpinTime=_dpf(val);
            else if (strcmp(key,"beatRepeatDiv")==0) daw.tapeFx.beatRepeatDiv=_dpi(val);
            else if (strcmp(key,"beatRepeatDecay")==0) daw.tapeFx.beatRepeatDecay=_dpf(val);
            else if (strcmp(key,"beatRepeatPitch")==0) daw.tapeFx.beatRepeatPitch=_dpf(val);
            else if (strcmp(key,"beatRepeatMix")==0) daw.tapeFx.beatRepeatMix=_dpf(val);
            else if (strcmp(key,"beatRepeatGate")==0) daw.tapeFx.beatRepeatGate=_dpf(val);
            else if (strcmp(key,"djfxLoopDiv")==0) daw.tapeFx.djfxLoopDiv=_dpi(val);
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
        case _DW_SEC_PERTRACK:
            if (strcmp(key,"arrMode")==0) daw.arr.arrMode=_dpb(val);
            else if (strcmp(key,"arrLength")==0) daw.arr.length=_dpi(val);
            else if (strncmp(key,"arr",3)==0 && isdigit(key[3])) {
                int trackIdx = atoi(key+3);
                if (trackIdx >= 0 && trackIdx < ARR_MAX_TRACKS) {
                    // Parse space-separated int list into cells[bar][track]
                    char *p = val;
                    for (int b = 0; b < daw.arr.length && b < ARR_MAX_BARS; b++) {
                        while (*p == ' ') p++;
                        if (!*p) break;
                        daw.arr.cells[b][trackIdx] = atoi(p);
                        while (*p && *p != ' ') p++;
                    }
                }
            }
            break;
        case _DW_SEC_LAUNCHER:
            // "t0 = 3 5 -1 2" — pattern indices per slot for track 0
            // "t0.1.na = 2 4" — next action type and loop count for track 0 slot 1
            if (key[0] == 't' && isdigit(key[1])) {
                int ti = atoi(key + 1);
                if (ti >= 0 && ti < ARR_MAX_TRACKS) {
                    // Check for ".N.na" suffix (next action)
                    char *dot = strchr(key + 1, '.');
                    if (dot && strstr(dot, ".na")) {
                        int si = atoi(dot + 1);
                        if (si >= 0 && si < LAUNCHER_MAX_SLOTS) {
                            int naType = 0, naLoops = 0;
                            sscanf(val, "%d %d", &naType, &naLoops);
                            if (naType >= 0 && naType < NEXT_ACTION_COUNT) {
                                daw.launcher.tracks[ti].nextAction[si] = (NextAction)naType;
                                daw.launcher.tracks[ti].nextActionLoops[si] = naLoops;
                            }
                        }
                    } else {
                        // Pattern list for this track
                        char *p = val;
                        for (int s = 0; s < LAUNCHER_MAX_SLOTS; s++) {
                            while (*p == ' ') p++;
                            if (!*p) break;
                            int pat = atoi(p);
                            daw.launcher.tracks[ti].pattern[s] = pat;
                            daw.launcher.tracks[ti].state[s] = (pat >= 0) ? CLIP_STOPPED : CLIP_EMPTY;
                            while (*p && *p != ' ') p++;
                        }
                    }
                }
            }
            break;
        case _DW_SEC_PATTERN:
            if (subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
                Pattern *p = &seq.patterns[subIndex];
                if (strcmp(key,"length")==0) {
                    int l = _dpi(val);
                    if (l >= 1 && l <= SEQ_MAX_STEPS) p->length = l;
                }
                else if (strcmp(key,"trackType")==0) {
                    if (strcmp(val,"drum")==0) p->trackType = TRACK_DRUM;
                    else if (strcmp(val,"sampler")==0) p->trackType = TRACK_SAMPLER;
                    else p->trackType = TRACK_MELODIC;
                }
                // Legacy keys: expand multi-track pattern into per-track sub-patterns
                else if (strcmp(key,"drumTrackLength")==0) {
                    _legacyBase = subIndex * SEQ_V2_MAX_TRACKS;
                    _anyLegacy = true;
                    int lens[SEQ_DRUM_TRACKS];
                    _dwParseIntList(val, lens, SEQ_DRUM_TRACKS);
                    for (int _t = 0; _t < SEQ_DRUM_TRACKS; _t++) {
                        int ni = _legacyBase + _t;
                        if (ni < SEQ_NUM_PATTERNS) {
                            initPattern(&seq.patterns[ni]);
                            seq.patterns[ni].length = (lens[_t] > 0) ? lens[_t] : 16;
                            seq.patterns[ni].trackType = TRACK_DRUM;
                        }
                    }
                }
                else if (strcmp(key,"melodyTrackLength")==0) {
                    if (_legacyBase >= 0) {
                        int lens[SEQ_MELODY_TRACKS];
                        _dwParseIntList(val, lens, SEQ_MELODY_TRACKS);
                        for (int _t = 0; _t < SEQ_MELODY_TRACKS; _t++) {
                            int ni = _legacyBase + SEQ_DRUM_TRACKS + _t;
                            if (ni < SEQ_NUM_PATTERNS) {
                                if (seq.patterns[ni].length == 16 || lens[_t] > 0) {
                                    seq.patterns[ni].length = (lens[_t] > 0) ? lens[_t] : 16;
                                }
                                seq.patterns[ni].trackType = TRACK_MELODIC;
                            }
                        }
                    }
                }
                else if (strcmp(key,"samplerTrackLength")==0) {
                    if (_legacyBase >= 0) {
                        int ni = _legacyBase + SEQ_TRACK_SAMPLER;
                        if (ni < SEQ_NUM_PATTERNS) {
                            int l = _dpi(val);
                            if (l > 0) seq.patterns[ni].length = l;
                            seq.patterns[ni].trackType = TRACK_SAMPLER;
                        }
                    }
                }
            }
            break;
#ifdef DAW_HAS_CHOP_STATE
        case _DW_SEC_SAMPLE:
            if (strcmp(key,"sourceFile")==0) {
                char t[256]; strncpy(t,val,255); t[255]=0; _dwStripQuotes(t);
                strncpy(chopState.sourcePath,t,255); chopState.sourcePath[255]=0;
            }
            else if (strcmp(key,"sourcePattern")==0) { /* legacy, ignored */ }
            else if (strcmp(key,"sourceLoops")==0) chopState.sourceLoops = _dpi(val);
            else if (strcmp(key,"selStart")==0) { chopState.selStart = _dpf(val); chopState.hasSelection = true; }
            else if (strcmp(key,"selEnd")==0) { chopState.selEnd = _dpf(val); chopState.hasSelection = true; }
            else if (strcmp(key,"sliceCount")==0) chopState.sliceCount = _dpi(val);
            else if (strcmp(key,"chopMode")==0) chopState.chopMode = _dpi(val);
            else if (strcmp(key,"normalize")==0) chopState.normalize = _dpb(val);
            else if (strcmp(key,"sensitivity")==0) chopState.sensitivity = _dpf(val);
            else if (strcmp(key,"fadeMs")==0) { /* legacy, ignored */ }
            else if (strncmp(key,"padMap",6)==0) {
                int idx = key[6] - '0';
                if (idx >= 0 && idx < 4) daw.chopSliceMap[idx] = _dpi(val);
            }
            else if (strncmp(key,"slice.",6)==0) { /* legacy per-slice params, ignored */ }
            else if (strcmp(key,"markerCount")==0) { /* info only, markers key has the data */ }
            else if (strcmp(key,"markers")==0) {
                // Parse comma-separated marker positions into temp storage
                // (scratch data doesn't exist yet — will be created by re-bounce)
                _loadedMarkerCount = 0;
                const char *p = val;
                while (*p) {
                    while (*p == ' ' || *p == ',') p++;
                    if (!*p) break;
                    int pos = 0;
                    while (*p >= '0' && *p <= '9') { pos = pos * 10 + (*p - '0'); p++; }
                    if (pos > 0 && _loadedMarkerCount < SCRATCH_MAX_MARKERS)
                        _loadedMarkers[_loadedMarkerCount++] = pos;
                }
            }
            break;
#endif
        case _DW_SEC_SLOTFLAGS:
            // Parse "slot.N.pitched" or "slot.N.loop"
            if (strncmp(key,"slot.",5)==0) {
                int si = 0; const char *p = key + 5;
                while (*p >= '0' && *p <= '9') { si = si*10 + (*p - '0'); p++; }
                if (*p == '.' && si >= 0 && si < SAMPLER_MAX_SAMPLES) {
                    p++;
                    if (strcmp(p,"pitched")==0) samplerCtx->samples[si].pitched = _dpb(val);
                    else if (strcmp(p,"loop")==0) {
                        if (_dpb(val)) samplerCtx->samples[si].oneShot = false;
                    }
                }
            }
            break;
        default: break;
        }
    }

    fclose(f);

    // Legacy migration (case 2): old multi-track events found in an already-arrMode song.
    // The arr cells reference old multi-track pattern indices — remap to per-track sub-patterns.
    if (_anyLegacy && daw.arr.arrMode && daw.arr.length > 0) {
        for (int b = 0; b < daw.arr.length && b < ARR_MAX_BARS; b++) {
            for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                int oldIdx = daw.arr.cells[b][t];
                if (oldIdx >= 0) {
                    int ni = oldIdx * SEQ_V2_MAX_TRACKS + t;
                    daw.arr.cells[b][t] = (ni < SEQ_NUM_PATTERNS) ? ni : ARR_EMPTY;
                }
            }
        }
    }

    // Legacy migration: if old multi-track format detected, build per-track arrangement
    // so the sequencer can use perTrackPatterns=true to play each track independently.
    if (_anyLegacy && !daw.arr.arrMode) {
        daw.arr.arrMode = true;
        if (daw.song.songMode && daw.song.length > 0) {
            // Song mode: build per-bar arrangement from song pattern list
            daw.arr.length = daw.song.length;
            int firstOldPat = daw.song.patterns[0];
            int track0PatIdx = firstOldPat * SEQ_V2_MAX_TRACKS + 0;
            daw.arr.barLengthSteps = (track0PatIdx < SEQ_NUM_PATTERNS)
                ? seq.patterns[track0PatIdx].length : 16;
            for (int b = 0; b < daw.song.length && b < ARR_MAX_BARS; b++) {
                int oldPat = daw.song.patterns[b];
                for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                    int ni = oldPat * SEQ_V2_MAX_TRACKS + t;
                    daw.arr.cells[b][t] = (ni < SEQ_NUM_PATTERNS) ? ni : ARR_EMPTY;
                }
            }
        } else {
            // Pattern mode: build single-bar arrangement from pattern 0
            // (song-render starts from pattern 0; DAW uses currentPattern cycling)
            daw.arr.length = 1;
            int pat0 = 0;
            int track0PatIdx = pat0 * SEQ_V2_MAX_TRACKS + 0;
            daw.arr.barLengthSteps = (track0PatIdx < SEQ_NUM_PATTERNS && seq.patterns[track0PatIdx].length > 0)
                ? seq.patterns[track0PatIdx].length : 16;
            for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                int ni = pat0 * SEQ_V2_MAX_TRACKS + t;
                daw.arr.cells[0][t] = (ni < SEQ_NUM_PATTERNS) ? ni : ARR_EMPTY;
            }
        }
    }

    // Backward compat: old files without trackSwing keys — propagate global swing
    {
        bool allZero = true;
        for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
            if (seq.trackSwing[i] != 0) { allZero = false; break; }
        }
        if (allZero && seq.dilla.swing != 0) {
            for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++)
                seq.trackSwing[i] = seq.dilla.swing;
        }
    }

    // Reset transport to start
    daw.transport.currentStep = 0;
    daw.transport.stepAccumulator = 0;
    daw.transport.currentPattern = 0;
    seq.currentPattern = 0;

    // Backward compat: multiband defaults (old files saved all zeros when mbOn=false)
    if (daw.masterFx.mbLowGain == 0.0f && daw.masterFx.mbMidGain == 0.0f && daw.masterFx.mbHighGain == 0.0f) {
        daw.masterFx.mbLowCross = 200.0f;
        daw.masterFx.mbHighCross = 3000.0f;
        daw.masterFx.mbLowGain = 1.0f;
        daw.masterFx.mbMidGain = 1.0f;
        daw.masterFx.mbHighGain = 1.0f;
        daw.masterFx.mbLowDrive = 1.0f;
        daw.masterFx.mbMidDrive = 1.0f;
        daw.masterFx.mbHighDrive = 1.0f;
    }

    // Reset preset tracking (loaded patches are custom)
    for (int i = 0; i < NUM_PATCHES; i++) patchPresetIndex[i] = -1;

#ifdef DAW_HAS_CHOP_STATE
    // Re-bounce sample recipe if [sample] section was present.
    // Guard against infinite recursion: renderPatternToBuffer calls dawLoad which
    // would call scratchFromSelection which calls renderPatternToBuffer again.
    // _chopBounceActive (from sample_chop.h) suppresses re-bounce during offline bounce.
    static bool _dawLoadRebouncing = false;
    if (chopState.sourcePath[0] && !_dawLoadRebouncing && !_chopBounceActive) {
        _dawLoadRebouncing = true;

        // Bounce full song (blocking — renders all patterns needed for chop)
        chopBounceFullSong();
        while (chopBounceNextPattern()) {}

        // Extract selection into scratch space + auto-chop
        scratchFromSelection();

        // Restore markers from file (overrides auto-chop markers)
        if (_loadedMarkerCount > 0 && scratchHasData(&scratch)) {
            scratch.markerCount = 0;
            for (int m = 0; m < _loadedMarkerCount; m++) {
                if (_loadedMarkers[m] > 0 && _loadedMarkers[m] < scratch.length)
                    scratchAddMarker(&scratch, _loadedMarkers[m]);
            }
        }

        // Commit slices to sampler bank so they play immediately
        if (scratchHasData(&scratch)) {
            dawAudioGate();
            scratchCommitToBank(&scratch, 0);
            dawAudioUngate();
        }

        _dawLoadRebouncing = false;
    }
#endif

    return true;
}

#endif // _DAW_FILE_SAVE_LOAD

#endif // DAW_FILE_H
