// preset_dump.c — Dump all instrument presets to a text file (song_file.h format)
//
// Outputs one [preset.N] section per preset, using the same key=value format
// as .song patch sections. Used by midi_import.py to embed real preset data.
//
// Usage:
//   make preset-dump && ./build/bin/preset-dump > soundsystem/tools/presets.txt
//   make preset-dump && ./build/bin/preset-dump -o soundsystem/tools/presets.txt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include synth engine (no raylib dependency)
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/instrument_presets.h"

// File I/O helpers shared with song_file.h
#include "../engines/file_helpers.h"

// Wave type names (from synth.h — already declared there)

static void writePatch(FILE *f, const char *section, const char *name, const SynthPatch *p) {
    fprintf(f, "\n[%s]\n", section);
    fprintf(f, "name = \"%s\"\n", name);
    if (p->p_waveType >= 0 && p->p_waveType < waveTypeCount)
        fprintf(f, "waveType = %s\n", waveTypeNames[p->p_waveType]);
    else
        fileWriteInt(f, "waveType", p->p_waveType);
    fileWriteInt(f, "scwIndex", p->p_scwIndex);
    fileWriteFloat(f, "attack", p->p_attack);
    fileWriteFloat(f, "decay", p->p_decay);
    fileWriteFloat(f, "sustain", p->p_sustain);
    fileWriteFloat(f, "release", p->p_release);
    fileWriteBool(f, "expRelease", p->p_expRelease);
    fileWriteBool(f, "expDecay", p->p_expDecay);
    fileWriteBool(f, "oneShot", p->p_oneShot);
    fileWriteFloat(f, "volume", p->p_volume);
    fileWriteFloat(f, "pulseWidth", p->p_pulseWidth);
    fileWriteFloat(f, "pwmRate", p->p_pwmRate);
    fileWriteFloat(f, "pwmDepth", p->p_pwmDepth);
    fileWriteFloat(f, "vibratoRate", p->p_vibratoRate);
    fileWriteFloat(f, "vibratoDepth", p->p_vibratoDepth);
    fileWriteFloat(f, "filterCutoff", p->p_filterCutoff);
    fileWriteFloat(f, "filterResonance", p->p_filterResonance);
    fileWriteInt(f, "filterType", p->p_filterType);
    fileWriteFloat(f, "filterEnvAmt", p->p_filterEnvAmt);
    fileWriteFloat(f, "filterEnvAttack", p->p_filterEnvAttack);
    fileWriteFloat(f, "filterEnvDecay", p->p_filterEnvDecay);
    fileWriteFloat(f, "filterKeyTrack", p->p_filterKeyTrack);
    fileWriteFloat(f, "filterLfoRate", p->p_filterLfoRate);
    fileWriteFloat(f, "filterLfoDepth", p->p_filterLfoDepth);
    fileWriteInt(f, "filterLfoShape", p->p_filterLfoShape);
    fileWriteInt(f, "filterLfoSync", p->p_filterLfoSync);
    fileWriteFloat(f, "resoLfoRate", p->p_resoLfoRate);
    fileWriteFloat(f, "resoLfoDepth", p->p_resoLfoDepth);
    fileWriteInt(f, "resoLfoShape", p->p_resoLfoShape);
    fileWriteInt(f, "resoLfoSync", p->p_resoLfoSync);
    fileWriteFloat(f, "ampLfoRate", p->p_ampLfoRate);
    fileWriteFloat(f, "ampLfoDepth", p->p_ampLfoDepth);
    fileWriteInt(f, "ampLfoShape", p->p_ampLfoShape);
    fileWriteInt(f, "ampLfoSync", p->p_ampLfoSync);
    fileWriteFloat(f, "pitchLfoRate", p->p_pitchLfoRate);
    fileWriteFloat(f, "pitchLfoDepth", p->p_pitchLfoDepth);
    fileWriteInt(f, "pitchLfoShape", p->p_pitchLfoShape);
    fileWriteInt(f, "pitchLfoSync", p->p_pitchLfoSync);
    fileWriteFloat(f, "filterLfoPhaseOffset", p->p_filterLfoPhaseOffset);
    fileWriteFloat(f, "resoLfoPhaseOffset", p->p_resoLfoPhaseOffset);
    fileWriteFloat(f, "ampLfoPhaseOffset", p->p_ampLfoPhaseOffset);
    fileWriteFloat(f, "pitchLfoPhaseOffset", p->p_pitchLfoPhaseOffset);
    fileWriteBool(f, "arpEnabled", p->p_arpEnabled);
    fileWriteInt(f, "arpMode", p->p_arpMode);
    fileWriteInt(f, "arpRateDiv", p->p_arpRateDiv);
    fileWriteFloat(f, "arpRate", p->p_arpRate);
    fileWriteInt(f, "arpChord", p->p_arpChord);
    fileWriteInt(f, "unisonCount", p->p_unisonCount);
    fileWriteFloat(f, "unisonDetune", p->p_unisonDetune);
    fileWriteFloat(f, "unisonMix", p->p_unisonMix);
    fileWriteBool(f, "monoMode", p->p_monoMode);
    fileWriteFloat(f, "glideTime", p->p_glideTime);
    fileWriteFloat(f, "pluckBrightness", p->p_pluckBrightness);
    fileWriteFloat(f, "pluckDamping", p->p_pluckDamping);
    fileWriteFloat(f, "pluckDamp", p->p_pluckDamp);
    fileWriteInt(f, "additivePreset", p->p_additivePreset);
    fileWriteFloat(f, "additiveBrightness", p->p_additiveBrightness);
    fileWriteFloat(f, "additiveShimmer", p->p_additiveShimmer);
    fileWriteFloat(f, "additiveInharmonicity", p->p_additiveInharmonicity);
    fileWriteInt(f, "malletPreset", p->p_malletPreset);
    fileWriteFloat(f, "malletStiffness", p->p_malletStiffness);
    fileWriteFloat(f, "malletHardness", p->p_malletHardness);
    fileWriteFloat(f, "malletStrikePos", p->p_malletStrikePos);
    fileWriteFloat(f, "malletResonance", p->p_malletResonance);
    fileWriteFloat(f, "malletTremolo", p->p_malletTremolo);
    fileWriteFloat(f, "malletTremoloRate", p->p_malletTremoloRate);
    fileWriteFloat(f, "malletDamp", p->p_malletDamp);
    fileWriteInt(f, "voiceVowel", p->p_voiceVowel);
    fileWriteFloat(f, "voiceFormantShift", p->p_voiceFormantShift);
    fileWriteFloat(f, "voiceBreathiness", p->p_voiceBreathiness);
    fileWriteFloat(f, "voiceBuzziness", p->p_voiceBuzziness);
    fileWriteFloat(f, "voiceSpeed", p->p_voiceSpeed);
    fileWriteFloat(f, "voicePitch", p->p_voicePitch);
    fileWriteBool(f, "voiceConsonant", p->p_voiceConsonant);
    fileWriteFloat(f, "voiceConsonantAmt", p->p_voiceConsonantAmt);
    fileWriteBool(f, "voiceNasal", p->p_voiceNasal);
    fileWriteFloat(f, "voiceNasalAmt", p->p_voiceNasalAmt);
    fileWriteFloat(f, "voicePitchEnv", p->p_voicePitchEnv);
    fileWriteFloat(f, "voicePitchEnvTime", p->p_voicePitchEnvTime);
    fileWriteFloat(f, "voicePitchEnvCurve", p->p_voicePitchEnvCurve);
    fileWriteInt(f, "granularScwIndex", p->p_granularScwIndex);
    fileWriteFloat(f, "granularGrainSize", p->p_granularGrainSize);
    fileWriteFloat(f, "granularDensity", p->p_granularDensity);
    fileWriteFloat(f, "granularPosition", p->p_granularPosition);
    fileWriteFloat(f, "granularPosRandom", p->p_granularPosRandom);
    fileWriteFloat(f, "granularPitch", p->p_granularPitch);
    fileWriteFloat(f, "granularPitchRandom", p->p_granularPitchRandom);
    fileWriteFloat(f, "granularAmpRandom", p->p_granularAmpRandom);
    fileWriteFloat(f, "granularSpread", p->p_granularSpread);
    fileWriteBool(f, "granularFreeze", p->p_granularFreeze);
    fileWriteFloat(f, "fmLfoRate", p->p_fmLfoRate);
    fileWriteFloat(f, "fmLfoDepth", p->p_fmLfoDepth);
    fileWriteInt(f, "fmLfoShape", p->p_fmLfoShape);
    fileWriteInt(f, "fmLfoSync", p->p_fmLfoSync);
    fileWriteFloat(f, "fmLfoPhaseOffset", p->p_fmLfoPhaseOffset);
    fileWriteFloat(f, "fmModRatio", p->p_fmModRatio);
    fileWriteFloat(f, "fmModIndex", p->p_fmModIndex);
    fileWriteFloat(f, "fmFeedback", p->p_fmFeedback);
    fileWriteFloat(f, "fmMod2Ratio", p->p_fmMod2Ratio);
    fileWriteFloat(f, "fmMod2Index", p->p_fmMod2Index);
    fileWriteInt(f, "fmAlgorithm", p->p_fmAlgorithm);
    fileWriteInt(f, "pdWaveType", p->p_pdWaveType);
    fileWriteFloat(f, "pdDistortion", p->p_pdDistortion);
    fileWriteInt(f, "membranePreset", p->p_membranePreset);
    fileWriteFloat(f, "membraneDamping", p->p_membraneDamping);
    fileWriteFloat(f, "membraneStrike", p->p_membraneStrike);
    fileWriteFloat(f, "membraneBend", p->p_membraneBend);
    fileWriteFloat(f, "membraneBendDecay", p->p_membraneBendDecay);
    fileWriteFloat(f, "bowPressure", p->p_bowPressure);
    fileWriteFloat(f, "bowSpeed", p->p_bowSpeed);
    fileWriteFloat(f, "bowPosition", p->p_bowPosition);
    fileWriteFloat(f, "pipeBreath", p->p_pipeBreath);
    fileWriteFloat(f, "pipeEmbouchure", p->p_pipeEmbouchure);
    fileWriteFloat(f, "pipeBore", p->p_pipeBore);
    fileWriteFloat(f, "pipeOverblow", p->p_pipeOverblow);
    fileWriteInt(f, "birdType", p->p_birdType);
    fileWriteFloat(f, "birdChirpRange", p->p_birdChirpRange);
    fileWriteFloat(f, "birdTrillRate", p->p_birdTrillRate);
    fileWriteFloat(f, "birdTrillDepth", p->p_birdTrillDepth);
    fileWriteFloat(f, "birdAmRate", p->p_birdAmRate);
    fileWriteFloat(f, "birdAmDepth", p->p_birdAmDepth);
    fileWriteFloat(f, "birdHarmonics", p->p_birdHarmonics);
    fileWriteFloat(f, "pitchEnvAmount", p->p_pitchEnvAmount);
    fileWriteFloat(f, "pitchEnvDecay", p->p_pitchEnvDecay);
    fileWriteFloat(f, "pitchEnvCurve", p->p_pitchEnvCurve);
    fileWriteBool(f, "pitchEnvLinear", p->p_pitchEnvLinear);
    fileWriteFloat(f, "noiseMix", p->p_noiseMix);
    fileWriteFloat(f, "noiseTone", p->p_noiseTone);
    fileWriteFloat(f, "noiseHP", p->p_noiseHP);
    fileWriteFloat(f, "noiseDecay", p->p_noiseDecay);
    fileWriteInt(f, "retriggerCount", p->p_retriggerCount);
    fileWriteFloat(f, "retriggerSpread", p->p_retriggerSpread);
    fileWriteBool(f, "retriggerOverlap", p->p_retriggerOverlap);
    fileWriteFloat(f, "retriggerBurstDecay", p->p_retriggerBurstDecay);
    fileWriteFloat(f, "osc2Ratio", p->p_osc2Ratio);
    fileWriteFloat(f, "osc2Level", p->p_osc2Level);
    fileWriteFloat(f, "osc3Ratio", p->p_osc3Ratio);
    fileWriteFloat(f, "osc3Level", p->p_osc3Level);
    fileWriteFloat(f, "osc4Ratio", p->p_osc4Ratio);
    fileWriteFloat(f, "osc4Level", p->p_osc4Level);
    fileWriteFloat(f, "osc5Ratio", p->p_osc5Ratio);
    fileWriteFloat(f, "osc5Level", p->p_osc5Level);
    fileWriteFloat(f, "osc6Ratio", p->p_osc6Ratio);
    fileWriteFloat(f, "osc6Level", p->p_osc6Level);
    fileWriteFloat(f, "drive", p->p_drive);
    fileWriteFloat(f, "clickLevel", p->p_clickLevel);
    fileWriteFloat(f, "clickTime", p->p_clickTime);
    fileWriteInt(f, "noiseMode", p->p_noiseMode);
    fileWriteInt(f, "oscMixMode", p->p_oscMixMode);
    fileWriteFloat(f, "retriggerCurve", p->p_retriggerCurve);
    fileWriteBool(f, "phaseReset", p->p_phaseReset);
    fileWriteBool(f, "noiseLPBypass", p->p_noiseLPBypass);
    fileWriteInt(f, "noiseType", p->p_noiseType);
    fileWriteBool(f, "envelopeEnabled", p->p_envelopeEnabled);
    fileWriteBool(f, "filterEnabled", p->p_filterEnabled);
    fileWriteBool(f, "acidMode", p->p_acidMode);
    fileWriteFloat(f, "accentSweepAmt", p->p_accentSweepAmt);
    fileWriteFloat(f, "gimmickDipAmt", p->p_gimmickDipAmt);
}

int main(int argc, char *argv[]) {
    const char *outpath = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outpath = argv[++i];
    }

    initInstrumentPresets();

    FILE *f = outpath ? fopen(outpath, "w") : stdout;
    if (!f) { fprintf(stderr, "Cannot open %s\n", outpath); return 1; }

    fprintf(f, "# PixelSynth preset dump (%d presets)\n", NUM_INSTRUMENT_PRESETS);
    fprintf(f, "count = %d\n", NUM_INSTRUMENT_PRESETS);

    for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
        char sec[32];
        snprintf(sec, sizeof(sec), "preset.%d", i);
        writePatch(f, sec, instrumentPresets[i].name, &instrumentPresets[i].patch);
    }

    if (outpath) fclose(f);
    fprintf(stderr, "Dumped %d presets%s%s\n", NUM_INSTRUMENT_PRESETS,
            outpath ? " to " : "", outpath ? outpath : "");
    return 0;
}
