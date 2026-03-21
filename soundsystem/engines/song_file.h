// song_file.h — .song/.patch/.bank file format parser and serializer
//
// Header-only. Include after synth.h, sequencer.h, drums.h, effects.h.
//
// Format: custom text (INI-style sections + key=value event lines)
//   [section]
//   key = value
//   d track=0 step=0 vel=0.90
//   m track=0 step=0 note=D2 vel=0.35 gate=8 slide
//   p track=6 step=0 param=cutoff val=0.3
//
// Future-proofing rules:
//   1. format = N version header in [song]
//   2. Unknown sections/keys are skipped (warn, don't error)
//   3. Events are fully key=value (no positional fields)
//   4. Track counts are explicit (drumTracks, melodyTracks)
//   5. Missing keys get engine defaults
//
#ifndef SONG_FILE_H
#define SONG_FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SONG_FILE_FORMAT_VERSION 1
#define SONG_FILE_MAX_LINE 512
#define SONG_FILE_MAX_NAME 64

// ============================================================================
// SongFileData — intermediate struct for serialization
// ============================================================================
// Caller fills this from their state, then calls songFileSave().
// Or calls songFileLoad() which fills this, then caller applies to state.

typedef struct {
    // Song header
    char name[SONG_FILE_MAX_NAME];
    float bpm;
    int ticksPerStep;           // 24 = 16th, 12 = 32nd
    int loopsPerPattern;
    int drumTracks;             // currently 4
    int melodyTracks;           // currently 3

    // Scale lock (prefixed to avoid synth.h macro collision)
    bool songScaleLockEnabled;
    int songScaleRoot;
    int songScaleType;

    // Groove
    DillaTiming dilla;
    int trackSwing[SEQ_V2_MAX_TRACKS];
    int trackTranspose[SEQ_V2_MAX_TRACKS];
    MelodyHumanize humanize;

    // Instruments (3 melody patches)
    SynthPatch instruments[3];  // bass, lead, chord

    // Mix
    float sfMasterVolume;
    float sfDrumVolume;
    float sfTrackVolume[SEQ_V2_MAX_TRACKS];

    // Master effects
    Effects sfEffects;

    // Per-bus effects
    BusEffects sfBusEffects[NUM_BUSES];

    // Dub loop
    DubLoopParams sfDubLoop;

    // Patterns
    int patternCount;
    Pattern patterns[SEQ_NUM_PATTERNS];

    // Chain (pattern play order)
    int chain[SEQ_MAX_CHAIN];
    int chainLength;                 // 0 = no chain

    // Metadata
    char author[SONG_FILE_MAX_NAME];
    char description[SONG_FILE_MAX_NAME];
    char mood[SONG_FILE_MAX_NAME];      // comma-separated tags
    char energy[16];                     // "low", "medium", "high"
    char context[SONG_FILE_MAX_NAME];   // comma-separated tags

    // Transitions
    float fadeIn;
    float fadeOut;
    bool crossfade;
} SongFileData;

static void songFileDataInit(SongFileData *d) {
    memset(d, 0, sizeof(*d));
    d->bpm = 120.0f;
    d->ticksPerStep = SEQ_TICKS_PER_STEP_16TH;
    d->loopsPerPattern = 1;
    d->drumTracks = SEQ_DRUM_TRACKS;
    d->melodyTracks = SEQ_MELODY_TRACKS;
    d->sfMasterVolume = 0.25f;
    d->sfDrumVolume = 0.6f;
    d->patternCount = SEQ_NUM_PATTERNS;
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) d->sfTrackVolume[i] = 1.0f;
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) initPattern(&d->patterns[i]);
    strcpy(d->energy, "medium");
}

// ============================================================================
// HELPER: note name conversion
// ============================================================================

static const char* _sf_noteNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static void _sf_midiToName(int midi, char *buf, int bufSize) {
    if (midi == SEQ_NOTE_OFF || midi < 0) {
        snprintf(buf, bufSize, "REST");
        return;
    }
    int octave = (midi / 12) - 1;
    int note = midi % 12;
    snprintf(buf, bufSize, "%s%d", _sf_noteNames[note], octave);
}

static int _sf_nameToMidi(const char *name) {
    if (!name || !name[0]) return SEQ_NOTE_OFF;
    if (strcmp(name, "REST") == 0 || strcmp(name, "-") == 0) return SEQ_NOTE_OFF;

    // Try numeric
    if (isdigit(name[0]) || (name[0] == '-' && isdigit(name[1]))) {
        return atoi(name);
    }

    // Parse note name: C#4, Eb2, D4, etc.
    int note = -1;
    int pos = 0;
    switch (toupper(name[0])) {
        case 'C': note = 0; break;
        case 'D': note = 2; break;
        case 'E': note = 4; break;
        case 'F': note = 5; break;
        case 'G': note = 7; break;
        case 'A': note = 9; break;
        case 'B': note = 11; break;
        default: return SEQ_NOTE_OFF;
    }
    pos = 1;
    if (name[pos] == '#') { note++; pos++; }
    else if (name[pos] == 'b') { note--; pos++; }
    if (note < 0) note += 12;
    if (note > 11) note -= 12;

    int octave = 4; // default
    if (name[pos] == '-' || isdigit(name[pos])) {
        octave = atoi(&name[pos]);
    }
    return (octave + 1) * 12 + note;
}

// ============================================================================
// HELPER: enum name tables for serialization
// ============================================================================

// Use canonical waveTypeNames[] and waveTypeCount from synth.h
// (extended from 14 to 16 to include WAVE_BOWED and WAVE_PIPE)
#define _sf_waveTypeNames waveTypeNames
#define _sf_waveTypeCount waveTypeCount

static const char* _sf_scaleTypeNames[] = {
    "chromatic", "major", "minor", "pentatonic", "minorPentatonic",
    "blues", "dorian", "mixolydian", "harmonicMinor"
};
static const int _sf_scaleTypeCount = 9;

static const char* _sf_conditionNames[] = {
    "always", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4",
    "fill", "notFill", "first", "notFirst"
};
static const int _sf_conditionCount = 11;

static const char* _sf_chordTypeNames[] = {
    "single", "fifth", "triad", "inv1", "inv2", "seventh",
    "octave", "octaves", "custom"
};
static const int _sf_chordTypeCount = 9;

static const char* _sf_pickModeNames[] = {
    "cycleUp", "cycleDown", "pingpong", "random", "randomWalk", "all"
};
static const int _sf_pickModeCount = 6;

static const char* _sf_plockParamNames[] = {
    "cutoff", "reso", "filterEnv", "decay", "volume",
    "pitch", "pulseWidth", "tone", "punch", "nudge",
    "flamTime", "flamVelocity"
};
static const int _sf_plockParamCount = 12;

// Shared file I/O helpers (write/read/parse primitives)
#include "file_helpers.h"

// Aliases for backward compatibility with existing _sf_ call sites
#define _sf_lookupName fileLookupName
#define _sf_writeStr fileWriteStr
#define _sf_writeFloat fileWriteFloat
#define _sf_writeInt fileWriteInt
#define _sf_writeBool fileWriteBool
#define _sf_writeEnum fileWriteEnum

// Parse "key=value" from a token. Returns 1 if found, 0 if bare word.
static int _sf_parseKV(const char *token, char *key, int keySize, char *val, int valSize) {
    const char *eq = strchr(token, '=');
    if (!eq) {
        // Bare word (boolean flag like "slide", "accent")
        strncpy(key, token, keySize - 1);
        key[keySize - 1] = '\0';
        val[0] = '\0';
        return 0;
    }
    int klen = (int)(eq - token);
    if (klen >= keySize) klen = keySize - 1;
    memcpy(key, token, klen);
    key[klen] = '\0';
    strncpy(val, eq + 1, valSize - 1);
    val[valSize - 1] = '\0';
    return 1;
}

// Tokenize a line by spaces, respecting quotes
// Note: '#' is only a comment at token boundaries (start of token after whitespace),
// NOT inside tokens — because note names like D#1 contain '#'.
static int _sf_tokenize(char *line, char **tokens, int maxTokens) {
    int count = 0;
    char *p = line;
    while (*p && count < maxTokens) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') break;  // '#' at token start = comment
        if (*p == '"') {
            p++;
            tokens[count++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            tokens[count++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    return count;
}

static void _sf_writePatch(FILE *f, const char *section, const SynthPatch *p) {
    fprintf(f, "\n[%s]\n", section);
    if (p->p_name[0]) _sf_writeStr(f, "name", p->p_name);
    _sf_writeEnum(f, "waveType", p->p_waveType, _sf_waveTypeNames, _sf_waveTypeCount);
    _sf_writeInt(f, "scwIndex", p->p_scwIndex);
    _sf_writeFloat(f, "attack", p->p_attack);
    _sf_writeFloat(f, "decay", p->p_decay);
    _sf_writeFloat(f, "sustain", p->p_sustain);
    _sf_writeFloat(f, "release", p->p_release);
    _sf_writeBool(f, "expRelease", p->p_expRelease);
    _sf_writeFloat(f, "volume", p->p_volume);
    _sf_writeFloat(f, "pulseWidth", p->p_pulseWidth);
    _sf_writeFloat(f, "pwmRate", p->p_pwmRate);
    _sf_writeFloat(f, "pwmDepth", p->p_pwmDepth);
    _sf_writeFloat(f, "vibratoRate", p->p_vibratoRate);
    _sf_writeFloat(f, "vibratoDepth", p->p_vibratoDepth);
    _sf_writeFloat(f, "filterCutoff", p->p_filterCutoff);
    _sf_writeFloat(f, "filterResonance", p->p_filterResonance);
    _sf_writeFloat(f, "filterEnvAmt", p->p_filterEnvAmt);
    _sf_writeFloat(f, "filterEnvAttack", p->p_filterEnvAttack);
    _sf_writeFloat(f, "filterEnvDecay", p->p_filterEnvDecay);
    _sf_writeFloat(f, "filterLfoRate", p->p_filterLfoRate);
    _sf_writeFloat(f, "filterLfoDepth", p->p_filterLfoDepth);
    _sf_writeInt(f, "filterLfoShape", p->p_filterLfoShape);
    _sf_writeInt(f, "filterLfoSync", p->p_filterLfoSync);
    _sf_writeFloat(f, "resoLfoRate", p->p_resoLfoRate);
    _sf_writeFloat(f, "resoLfoDepth", p->p_resoLfoDepth);
    _sf_writeInt(f, "resoLfoShape", p->p_resoLfoShape);
    _sf_writeInt(f, "resoLfoSync", p->p_resoLfoSync);
    _sf_writeFloat(f, "ampLfoRate", p->p_ampLfoRate);
    _sf_writeFloat(f, "ampLfoDepth", p->p_ampLfoDepth);
    _sf_writeInt(f, "ampLfoShape", p->p_ampLfoShape);
    _sf_writeInt(f, "ampLfoSync", p->p_ampLfoSync);
    _sf_writeFloat(f, "pitchLfoRate", p->p_pitchLfoRate);
    _sf_writeFloat(f, "pitchLfoDepth", p->p_pitchLfoDepth);
    _sf_writeInt(f, "pitchLfoShape", p->p_pitchLfoShape);
    _sf_writeInt(f, "pitchLfoSync", p->p_pitchLfoSync);
    _sf_writeFloat(f, "filterLfoPhaseOffset", p->p_filterLfoPhaseOffset);
    _sf_writeFloat(f, "resoLfoPhaseOffset", p->p_resoLfoPhaseOffset);
    _sf_writeFloat(f, "ampLfoPhaseOffset", p->p_ampLfoPhaseOffset);
    _sf_writeFloat(f, "pitchLfoPhaseOffset", p->p_pitchLfoPhaseOffset);
    _sf_writeBool(f, "arpEnabled", p->p_arpEnabled);
    _sf_writeInt(f, "arpMode", p->p_arpMode);
    _sf_writeInt(f, "arpRateDiv", p->p_arpRateDiv);
    _sf_writeFloat(f, "arpRate", p->p_arpRate);
    _sf_writeInt(f, "arpChord", p->p_arpChord);
    _sf_writeInt(f, "unisonCount", p->p_unisonCount);
    _sf_writeFloat(f, "unisonDetune", p->p_unisonDetune);
    _sf_writeFloat(f, "unisonMix", p->p_unisonMix);
    _sf_writeBool(f, "monoMode", p->p_monoMode);
    _sf_writeBool(f, "monoRetrigger", p->p_monoRetrigger);
    _sf_writeFloat(f, "glideTime", p->p_glideTime);
    _sf_writeFloat(f, "legatoWindow", p->p_legatoWindow);
    _sf_writeInt(f, "notePriority", p->p_notePriority);
    _sf_writeFloat(f, "pluckBrightness", p->p_pluckBrightness);
    _sf_writeFloat(f, "pluckDamping", p->p_pluckDamping);
    _sf_writeFloat(f, "pluckDamp", p->p_pluckDamp);
    _sf_writeInt(f, "additivePreset", p->p_additivePreset);
    _sf_writeFloat(f, "additiveBrightness", p->p_additiveBrightness);
    _sf_writeFloat(f, "additiveShimmer", p->p_additiveShimmer);
    _sf_writeFloat(f, "additiveInharmonicity", p->p_additiveInharmonicity);
    _sf_writeInt(f, "malletPreset", p->p_malletPreset);
    _sf_writeFloat(f, "malletStiffness", p->p_malletStiffness);
    _sf_writeFloat(f, "malletHardness", p->p_malletHardness);
    _sf_writeFloat(f, "malletStrikePos", p->p_malletStrikePos);
    _sf_writeFloat(f, "malletResonance", p->p_malletResonance);
    _sf_writeFloat(f, "malletTremolo", p->p_malletTremolo);
    _sf_writeFloat(f, "malletTremoloRate", p->p_malletTremoloRate);
    _sf_writeFloat(f, "malletDamp", p->p_malletDamp);
    _sf_writeInt(f, "voiceVowel", p->p_voiceVowel);
    _sf_writeFloat(f, "voiceFormantShift", p->p_voiceFormantShift);
    _sf_writeFloat(f, "voiceBreathiness", p->p_voiceBreathiness);
    _sf_writeFloat(f, "voiceBuzziness", p->p_voiceBuzziness);
    _sf_writeFloat(f, "voiceSpeed", p->p_voiceSpeed);
    _sf_writeFloat(f, "voicePitch", p->p_voicePitch);
    _sf_writeBool(f, "voiceConsonant", p->p_voiceConsonant);
    _sf_writeFloat(f, "voiceConsonantAmt", p->p_voiceConsonantAmt);
    _sf_writeBool(f, "voiceNasal", p->p_voiceNasal);
    _sf_writeFloat(f, "voiceNasalAmt", p->p_voiceNasalAmt);
    _sf_writeFloat(f, "voicePitchEnv", p->p_voicePitchEnv);
    _sf_writeFloat(f, "voicePitchEnvTime", p->p_voicePitchEnvTime);
    _sf_writeFloat(f, "voicePitchEnvCurve", p->p_voicePitchEnvCurve);
    _sf_writeInt(f, "granularScwIndex", p->p_granularScwIndex);
    _sf_writeFloat(f, "granularGrainSize", p->p_granularGrainSize);
    _sf_writeFloat(f, "granularDensity", p->p_granularDensity);
    _sf_writeFloat(f, "granularPosition", p->p_granularPosition);
    _sf_writeFloat(f, "granularPosRandom", p->p_granularPosRandom);
    _sf_writeFloat(f, "granularPitch", p->p_granularPitch);
    _sf_writeFloat(f, "granularPitchRandom", p->p_granularPitchRandom);
    _sf_writeFloat(f, "granularAmpRandom", p->p_granularAmpRandom);
    _sf_writeFloat(f, "granularSpread", p->p_granularSpread);
    _sf_writeBool(f, "granularFreeze", p->p_granularFreeze);
    _sf_writeFloat(f, "fmLfoRate", p->p_fmLfoRate);
    _sf_writeFloat(f, "fmLfoDepth", p->p_fmLfoDepth);
    _sf_writeInt(f, "fmLfoShape", p->p_fmLfoShape);
    _sf_writeInt(f, "fmLfoSync", p->p_fmLfoSync);
    _sf_writeFloat(f, "fmLfoPhaseOffset", p->p_fmLfoPhaseOffset);
    _sf_writeFloat(f, "fmModRatio", p->p_fmModRatio);
    _sf_writeFloat(f, "fmModIndex", p->p_fmModIndex);
    _sf_writeFloat(f, "fmFeedback", p->p_fmFeedback);
    _sf_writeFloat(f, "fmMod2Ratio", p->p_fmMod2Ratio);
    _sf_writeFloat(f, "fmMod2Index", p->p_fmMod2Index);
    _sf_writeInt(f, "fmAlgorithm", p->p_fmAlgorithm);
    _sf_writeInt(f, "pdWaveType", p->p_pdWaveType);
    _sf_writeFloat(f, "pdDistortion", p->p_pdDistortion);
    _sf_writeInt(f, "membranePreset", p->p_membranePreset);
    _sf_writeFloat(f, "membraneDamping", p->p_membraneDamping);
    _sf_writeFloat(f, "membraneStrike", p->p_membraneStrike);
    _sf_writeFloat(f, "membraneBend", p->p_membraneBend);
    _sf_writeFloat(f, "membraneBendDecay", p->p_membraneBendDecay);
    _sf_writeInt(f, "metallicPreset", p->p_metallicPreset);
    _sf_writeFloat(f, "metallicRingMix", p->p_metallicRingMix);
    _sf_writeFloat(f, "metallicNoiseLevel", p->p_metallicNoiseLevel);
    _sf_writeFloat(f, "metallicBrightness", p->p_metallicBrightness);
    _sf_writeFloat(f, "metallicPitchEnv", p->p_metallicPitchEnv);
    _sf_writeFloat(f, "metallicPitchEnvDecay", p->p_metallicPitchEnvDecay);
    _sf_writeInt(f, "guitarPreset", p->p_guitarPreset);
    _sf_writeFloat(f, "guitarBodyMix", p->p_guitarBodyMix);
    _sf_writeFloat(f, "guitarBodyBrightness", p->p_guitarBodyBrightness);
    _sf_writeFloat(f, "guitarPickPosition", p->p_guitarPickPosition);
    _sf_writeFloat(f, "guitarBuzz", p->p_guitarBuzz);
    _sf_writeFloat(f, "bowPressure", p->p_bowPressure);
    _sf_writeFloat(f, "bowSpeed", p->p_bowSpeed);
    _sf_writeFloat(f, "bowPosition", p->p_bowPosition);
    _sf_writeFloat(f, "pipeBreath", p->p_pipeBreath);
    _sf_writeFloat(f, "pipeEmbouchure", p->p_pipeEmbouchure);
    _sf_writeFloat(f, "pipeBore", p->p_pipeBore);
    _sf_writeFloat(f, "pipeOverblow", p->p_pipeOverblow);
    _sf_writeFloat(f, "reedStiffness", p->p_reedStiffness);
    _sf_writeFloat(f, "reedAperture", p->p_reedAperture);
    _sf_writeFloat(f, "reedBlowPressure", p->p_reedBlowPressure);
    _sf_writeFloat(f, "reedBore", p->p_reedBore);
    _sf_writeFloat(f, "reedVibratoDepth", p->p_reedVibratoDepth);
    _sf_writeFloat(f, "brassBlowPressure", p->p_brassBlowPressure);
    _sf_writeFloat(f, "brassLipTension", p->p_brassLipTension);
    _sf_writeFloat(f, "brassLipAperture", p->p_brassLipAperture);
    _sf_writeFloat(f, "brassBore", p->p_brassBore);
    _sf_writeFloat(f, "brassMute", p->p_brassMute);
    _sf_writeInt(f, "birdType", p->p_birdType);
    _sf_writeFloat(f, "birdChirpRange", p->p_birdChirpRange);
    _sf_writeFloat(f, "birdTrillRate", p->p_birdTrillRate);
    _sf_writeFloat(f, "birdTrillDepth", p->p_birdTrillDepth);
    _sf_writeFloat(f, "birdAmRate", p->p_birdAmRate);
    _sf_writeFloat(f, "birdAmDepth", p->p_birdAmDepth);
    _sf_writeFloat(f, "birdHarmonics", p->p_birdHarmonics);
    // Electric piano
    _sf_writeFloat(f, "epHardness", p->p_epHardness);
    _sf_writeFloat(f, "epToneBar", p->p_epToneBar);
    _sf_writeFloat(f, "epPickupPos", p->p_epPickupPos);
    _sf_writeFloat(f, "epPickupDist", p->p_epPickupDist);
    _sf_writeFloat(f, "epDecay", p->p_epDecay);
    _sf_writeFloat(f, "epBell", p->p_epBell);
    _sf_writeFloat(f, "epBellTone", p->p_epBellTone);
    _sf_writeInt(f, "epPickupType", p->p_epPickupType);
    // Organ
    for (int db = 0; db < ORGAN_DRAWBARS; db++) {
        char dbKey[16]; snprintf(dbKey, sizeof(dbKey), "orgDrawbar%d", db);
        _sf_writeFloat(f, dbKey, p->p_orgDrawbar[db]);
    }
    _sf_writeFloat(f, "orgClick", p->p_orgClick);
    _sf_writeInt(f, "orgPercOn", p->p_orgPercOn);
    _sf_writeInt(f, "orgPercHarmonic", p->p_orgPercHarmonic);
    _sf_writeInt(f, "orgPercSoft", p->p_orgPercSoft);
    _sf_writeInt(f, "orgPercFast", p->p_orgPercFast);
    _sf_writeFloat(f, "orgCrosstalk", p->p_orgCrosstalk);
    _sf_writeInt(f, "orgVibratoMode", p->p_orgVibratoMode);
    // Pitch envelope
    _sf_writeFloat(f, "pitchEnvAmount", p->p_pitchEnvAmount);
    _sf_writeFloat(f, "pitchEnvDecay", p->p_pitchEnvDecay);
    _sf_writeFloat(f, "pitchEnvCurve", p->p_pitchEnvCurve);
    _sf_writeBool(f, "pitchEnvLinear", p->p_pitchEnvLinear);
    // Noise mix
    _sf_writeFloat(f, "noiseMix", p->p_noiseMix);
    _sf_writeFloat(f, "noiseTone", p->p_noiseTone);
    _sf_writeFloat(f, "noiseHP", p->p_noiseHP);
    _sf_writeFloat(f, "noiseDecay", p->p_noiseDecay);
    // Retrigger
    _sf_writeInt(f, "retriggerCount", p->p_retriggerCount);
    _sf_writeFloat(f, "retriggerSpread", p->p_retriggerSpread);
    _sf_writeBool(f, "retriggerOverlap", p->p_retriggerOverlap);
    _sf_writeFloat(f, "retriggerBurstDecay", p->p_retriggerBurstDecay);
    // Extra oscillators
    _sf_writeFloat(f, "osc2Ratio", p->p_osc2Ratio);
    _sf_writeFloat(f, "osc2Level", p->p_osc2Level);
    _sf_writeFloat(f, "osc2Decay", p->p_osc2Decay);
    _sf_writeFloat(f, "osc3Ratio", p->p_osc3Ratio);
    _sf_writeFloat(f, "osc3Level", p->p_osc3Level);
    _sf_writeFloat(f, "osc3Decay", p->p_osc3Decay);
    _sf_writeFloat(f, "osc4Ratio", p->p_osc4Ratio);
    _sf_writeFloat(f, "osc4Level", p->p_osc4Level);
    _sf_writeFloat(f, "osc4Decay", p->p_osc4Decay);
    _sf_writeFloat(f, "osc5Ratio", p->p_osc5Ratio);
    _sf_writeFloat(f, "osc5Level", p->p_osc5Level);
    _sf_writeFloat(f, "osc5Decay", p->p_osc5Decay);
    _sf_writeFloat(f, "osc6Ratio", p->p_osc6Ratio);
    _sf_writeFloat(f, "osc6Level", p->p_osc6Level);
    _sf_writeFloat(f, "osc6Decay", p->p_osc6Decay);
    _sf_writeFloat(f, "oscVelSens", p->p_oscVelSens);
    _sf_writeFloat(f, "velToFilter", p->p_velToFilter);
    _sf_writeFloat(f, "velToClick", p->p_velToClick);
    _sf_writeFloat(f, "velToDrive", p->p_velToDrive);
    // Drive, click, filter type
    _sf_writeFloat(f, "drive", p->p_drive);
    _sf_writeInt(f, "driveMode", p->p_driveMode);
    _sf_writeFloat(f, "clickLevel", p->p_clickLevel);
    _sf_writeFloat(f, "clickTime", p->p_clickTime);
    _sf_writeInt(f, "filterType", p->p_filterType);
    _sf_writeInt(f, "filterModel", p->p_filterModel);
    // Algorithm modes
    _sf_writeInt(f, "noiseMode", p->p_noiseMode);
    _sf_writeInt(f, "oscMixMode", p->p_oscMixMode);
    _sf_writeFloat(f, "retriggerCurve", p->p_retriggerCurve);
    _sf_writeBool(f, "phaseReset", p->p_phaseReset);
    _sf_writeBool(f, "noiseLPBypass", p->p_noiseLPBypass);
    _sf_writeInt(f, "noiseType", p->p_noiseType);
    // Envelope flags
    _sf_writeBool(f, "expDecay", p->p_expDecay);
    _sf_writeBool(f, "oneShot", p->p_oneShot);
    // Envelope/filter bypass toggles
    _sf_writeBool(f, "envelopeEnabled", p->p_envelopeEnabled);
    _sf_writeBool(f, "filterEnabled", p->p_filterEnabled);
    // 303 acid mode
    _sf_writeBool(f, "acidMode", p->p_acidMode);
    _sf_writeFloat(f, "accentSweepAmt", p->p_accentSweepAmt);
    _sf_writeFloat(f, "gimmickDipAmt", p->p_gimmickDipAmt);
}

static void _sf_writePattern(FILE *f, int idx, const Pattern *p) {
    fprintf(f, "\n[pattern.%d]\n", idx);

    // Track lengths
    fprintf(f, "drumTrackLength =");
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) fprintf(f, " %d", p->trackLength[t]);
    fprintf(f, "\n");
    fprintf(f, "melodyTrackLength =");
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) fprintf(f, " %d", p->trackLength[SEQ_DRUM_TRACKS + t]);
    fprintf(f, "\n");

    // Drum events — only write active steps
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < p->trackLength[t]; s++) {
            if (!patGetDrum(p, t, s)) continue;
            fprintf(f, "d track=%d step=%d vel=%.3g", t, s, (double)patGetDrumVel(p, t, s));
            float dp = patGetDrumPitch(p, t, s);
            if (dp != 0.0f)
                fprintf(f, " pitch=%.3g", (double)dp);
            float dProb = patGetDrumProb(p, t, s);
            if (dProb > 0.0f && dProb < 1.0f)
                fprintf(f, " prob=%.3g", (double)dProb);
            int dCond = patGetDrumCond(p, t, s);
            if (dCond != COND_ALWAYS)
                fprintf(f, " cond=%s", _sf_conditionNames[dCond]);
            fprintf(f, "\n");
        }
    }

    // Melody events — write all voices per step
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        int absTrack = SEQ_DRUM_TRACKS + t;
        for (int s = 0; s < p->trackLength[absTrack]; s++) {
            const StepV2 *sv = &p->steps[absTrack][s];
            if (sv->noteCount == 0) continue;

            for (int v = 0; v < sv->noteCount; v++) {
                const StepNote *sn = &sv->notes[v];
                if (sn->note == SEQ_NOTE_OFF) continue;
                char noteName[8];
                _sf_midiToName(sn->note, noteName, sizeof(noteName));
                fprintf(f, "m track=%d step=%d note=%s vel=%.3g gate=%d",
                        t, s, noteName, (double)velU8ToFloat(sn->velocity), (int)sn->gate);
                if (sn->slide) fprintf(f, " slide");
                if (sn->accent) fprintf(f, " accent");
                if (sn->nudge != 0) fprintf(f, " nudge=%d", (int)sn->nudge);
                if (sn->gateNudge != 0) fprintf(f, " gateNudge=%d", (int)sn->gateNudge);
                // Per-step fields: only write on first voice to avoid duplication
                if (v == 0) {
                    int mSus = (int)sv->sustain;
                    if (mSus > 0) fprintf(f, " sustain=%d", mSus);
                    float mProb = probU8ToFloat(sv->probability);
                    if (mProb > 0.0f && mProb < 1.0f)
                        fprintf(f, " prob=%.3g", (double)mProb);
                    int mCond = (int)sv->condition;
                    if (mCond != COND_ALWAYS)
                        fprintf(f, " cond=%s", _sf_conditionNames[mCond]);
                    if (sv->pickMode != PICK_ALL)
                        fprintf(f, " pick=%d", (int)sv->pickMode);
                }
                fprintf(f, "\n");
            }
        }
    }

    // P-locks
    for (int i = 0; i < p->plockCount; i++) {
        const PLock *pl = &p->plocks[i];
        if (pl->param < _sf_plockParamCount) {
            fprintf(f, "p track=%d step=%d param=%s val=%.4g\n",
                    pl->track, pl->step, _sf_plockParamNames[pl->param], (double)pl->value);
        }
    }
}

static bool songFileSave(const char *filepath, const SongFileData *d) {
    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    // Song header
    fprintf(f, "# Song file (format %d)\n", SONG_FILE_FORMAT_VERSION);
    fprintf(f, "\n[song]\n");
    _sf_writeInt(f, "format", SONG_FILE_FORMAT_VERSION);
    _sf_writeStr(f, "name", d->name);
    _sf_writeFloat(f, "bpm", d->bpm);
    _sf_writeInt(f, "ticksPerStep", d->ticksPerStep);
    _sf_writeInt(f, "loopsPerPattern", d->loopsPerPattern);
    _sf_writeInt(f, "drumTracks", d->drumTracks);
    _sf_writeInt(f, "melodyTracks", d->melodyTracks);

    // Scale
    fprintf(f, "\n[scale]\n");
    _sf_writeBool(f, "enabled", d->songScaleLockEnabled);
    _sf_writeInt(f, "root", d->songScaleRoot);
    _sf_writeEnum(f, "type", d->songScaleType, _sf_scaleTypeNames, _sf_scaleTypeCount);

    // Groove
    fprintf(f, "\n[groove]\n");
    _sf_writeInt(f, "kickNudge", d->dilla.kickNudge);
    _sf_writeInt(f, "snareDelay", d->dilla.snareDelay);
    _sf_writeInt(f, "hatNudge", d->dilla.hatNudge);
    _sf_writeInt(f, "clapDelay", d->dilla.clapDelay);
    _sf_writeInt(f, "swing", d->dilla.swing);
    _sf_writeInt(f, "jitter", d->dilla.jitter);
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "trackSwing%d", i);
        _sf_writeInt(f, key, d->trackSwing[i]);
        snprintf(key, sizeof(key), "trackTranspose%d", i);
        _sf_writeInt(f, key, d->trackTranspose[i]);
    }
    _sf_writeInt(f, "melodyTimingJitter", d->humanize.timingJitter);
    _sf_writeFloat(f, "melodyVelocityJitter", d->humanize.velocityJitter);

    // Instruments
    _sf_writePatch(f, "instrument.bass", &d->instruments[0]);
    _sf_writePatch(f, "instrument.lead", &d->instruments[1]);
    _sf_writePatch(f, "instrument.chord", &d->instruments[2]);

    // Mix
    fprintf(f, "\n[mix]\n");
    _sf_writeFloat(f, "masterVolume", d->sfMasterVolume);
    _sf_writeFloat(f, "drumVolume", d->sfDrumVolume);
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "track%d", i);
        _sf_writeFloat(f, key, d->sfTrackVolume[i]);
    }

    // Master effects
    fprintf(f, "\n[effects]\n");
    _sf_writeBool(f, "distEnabled", d->sfEffects.distEnabled);
    _sf_writeFloat(f, "distDrive", d->sfEffects.distDrive);
    _sf_writeFloat(f, "distTone", d->sfEffects.distTone);
    _sf_writeFloat(f, "distMix", d->sfEffects.distMix);
    _sf_writeBool(f, "delayEnabled", d->sfEffects.delayEnabled);
    _sf_writeFloat(f, "delayTime", d->sfEffects.delayTime);
    _sf_writeFloat(f, "delayFeedback", d->sfEffects.delayFeedback);
    _sf_writeFloat(f, "delayMix", d->sfEffects.delayMix);
    _sf_writeFloat(f, "delayTone", d->sfEffects.delayTone);
    _sf_writeBool(f, "tapeEnabled", d->sfEffects.tapeEnabled);
    _sf_writeFloat(f, "tapeWow", d->sfEffects.tapeWow);
    _sf_writeFloat(f, "tapeFlutter", d->sfEffects.tapeFlutter);
    _sf_writeFloat(f, "tapeSaturation", d->sfEffects.tapeSaturation);
    _sf_writeFloat(f, "tapeHiss", d->sfEffects.tapeHiss);
    _sf_writeBool(f, "crushEnabled", d->sfEffects.crushEnabled);
    _sf_writeFloat(f, "crushBits", d->sfEffects.crushBits);
    _sf_writeFloat(f, "crushRate", d->sfEffects.crushRate);
    _sf_writeFloat(f, "crushMix", d->sfEffects.crushMix);
    _sf_writeBool(f, "chorusEnabled", d->sfEffects.chorusEnabled);
    _sf_writeBool(f, "chorusBBD", d->sfEffects.chorusBBD);
    _sf_writeFloat(f, "chorusRate", d->sfEffects.chorusRate);
    _sf_writeFloat(f, "chorusDepth", d->sfEffects.chorusDepth);
    _sf_writeFloat(f, "chorusMix", d->sfEffects.chorusMix);
    _sf_writeFloat(f, "chorusDelay", d->sfEffects.chorusDelay);
    _sf_writeFloat(f, "chorusFeedback", d->sfEffects.chorusFeedback);
    _sf_writeBool(f, "flangerEnabled", d->sfEffects.flangerEnabled);
    _sf_writeFloat(f, "flangerRate", d->sfEffects.flangerRate);
    _sf_writeFloat(f, "flangerDepth", d->sfEffects.flangerDepth);
    _sf_writeFloat(f, "flangerFeedback", d->sfEffects.flangerFeedback);
    _sf_writeFloat(f, "flangerMix", d->sfEffects.flangerMix);
    _sf_writeBool(f, "phaserEnabled", d->sfEffects.phaserEnabled);
    _sf_writeFloat(f, "phaserRate", d->sfEffects.phaserRate);
    _sf_writeFloat(f, "phaserDepth", d->sfEffects.phaserDepth);
    _sf_writeFloat(f, "phaserMix", d->sfEffects.phaserMix);
    _sf_writeFloat(f, "phaserFeedback", d->sfEffects.phaserFeedback);
    _sf_writeInt(f, "phaserStages", d->sfEffects.phaserStages);
    _sf_writeBool(f, "combEnabled", d->sfEffects.combEnabled);
    _sf_writeFloat(f, "combFreq", d->sfEffects.combFreq);
    _sf_writeFloat(f, "combFeedback", d->sfEffects.combFeedback);
    _sf_writeFloat(f, "combMix", d->sfEffects.combMix);
    _sf_writeFloat(f, "combDamping", d->sfEffects.combDamping);
    _sf_writeBool(f, "octaverEnabled", d->sfEffects.octaverEnabled);
    _sf_writeFloat(f, "octaverMix", d->sfEffects.octaverMix);
    _sf_writeFloat(f, "octaverSubLevel", d->sfEffects.octaverSubLevel);
    _sf_writeFloat(f, "octaverTone", d->sfEffects.octaverTone);
    _sf_writeBool(f, "tremoloEnabled", d->sfEffects.tremoloEnabled);
    _sf_writeFloat(f, "tremoloRate", d->sfEffects.tremoloRate);
    _sf_writeFloat(f, "tremoloDepth", d->sfEffects.tremoloDepth);
    _sf_writeInt(f, "tremoloShape", d->sfEffects.tremoloShape);
    _sf_writeBool(f, "leslieEnabled", d->sfEffects.leslieEnabled);
    _sf_writeInt(f, "leslieSpeed", d->sfEffects.leslieSpeed);
    _sf_writeFloat(f, "leslieDrive", d->sfEffects.leslieDrive);
    _sf_writeFloat(f, "leslieBalance", d->sfEffects.leslieBalance);
    _sf_writeFloat(f, "leslieDoppler", d->sfEffects.leslieDoppler);
    _sf_writeFloat(f, "leslieMix", d->sfEffects.leslieMix);
    _sf_writeBool(f, "wahEnabled", d->sfEffects.wahEnabled);
    _sf_writeInt(f, "wahMode", d->sfEffects.wahMode);
    _sf_writeFloat(f, "wahRate", d->sfEffects.wahRate);
    _sf_writeFloat(f, "wahSensitivity", d->sfEffects.wahSensitivity);
    _sf_writeFloat(f, "wahFreqLow", d->sfEffects.wahFreqLow);
    _sf_writeFloat(f, "wahFreqHigh", d->sfEffects.wahFreqHigh);
    _sf_writeFloat(f, "wahResonance", d->sfEffects.wahResonance);
    _sf_writeFloat(f, "wahMix", d->sfEffects.wahMix);
    _sf_writeBool(f, "ringModEnabled", d->sfEffects.ringModEnabled);
    _sf_writeFloat(f, "ringModFreq", d->sfEffects.ringModFreq);
    _sf_writeFloat(f, "ringModMix", d->sfEffects.ringModMix);
    _sf_writeBool(f, "reverbEnabled", d->sfEffects.reverbEnabled);
    _sf_writeBool(f, "reverbFDN", d->sfEffects.reverbFDN);
    _sf_writeFloat(f, "reverbSize", d->sfEffects.reverbSize);
    _sf_writeFloat(f, "reverbDamping", d->sfEffects.reverbDamping);
    _sf_writeFloat(f, "reverbMix", d->sfEffects.reverbMix);
    _sf_writeFloat(f, "reverbPreDelay", d->sfEffects.reverbPreDelay);
    _sf_writeFloat(f, "reverbBass", d->sfEffects.reverbBass);
    _sf_writeBool(f, "sidechainEnabled", d->sfEffects.sidechainEnabled);
    _sf_writeInt(f, "sidechainSource", d->sfEffects.sidechainSource);
    _sf_writeInt(f, "sidechainTarget", d->sfEffects.sidechainTarget);
    _sf_writeFloat(f, "sidechainDepth", d->sfEffects.sidechainDepth);
    _sf_writeFloat(f, "sidechainAttack", d->sfEffects.sidechainAttack);
    _sf_writeFloat(f, "sidechainRelease", d->sfEffects.sidechainRelease);
    _sf_writeFloat(f, "sidechainHPFreq", d->sfEffects.sidechainHPFreq);
    _sf_writeBool(f, "scEnvEnabled", d->sfEffects.scEnvEnabled);
    _sf_writeInt(f, "scEnvSource", d->sfEffects.scEnvSource);
    _sf_writeInt(f, "scEnvTarget", d->sfEffects.scEnvTarget);
    _sf_writeFloat(f, "scEnvDepth", d->sfEffects.scEnvDepth);
    _sf_writeFloat(f, "scEnvAttack", d->sfEffects.scEnvAttack);
    _sf_writeFloat(f, "scEnvHold", d->sfEffects.scEnvHold);
    _sf_writeFloat(f, "scEnvRelease", d->sfEffects.scEnvRelease);
    _sf_writeInt(f, "scEnvCurve", d->sfEffects.scEnvCurve);
    _sf_writeFloat(f, "scEnvHPFreq", d->sfEffects.scEnvHPFreq);
    _sf_writeBool(f, "subBassBoost", d->sfEffects.subBassBoost);
    _sf_writeBool(f, "mbEnabled", d->sfEffects.mbEnabled);
    _sf_writeFloat(f, "mbLowCrossover", d->sfEffects.mbLowCrossover);
    _sf_writeFloat(f, "mbHighCrossover", d->sfEffects.mbHighCrossover);
    _sf_writeFloat(f, "mbLowGain", d->sfEffects.mbLowGain);
    _sf_writeFloat(f, "mbMidGain", d->sfEffects.mbMidGain);
    _sf_writeFloat(f, "mbHighGain", d->sfEffects.mbHighGain);
    _sf_writeFloat(f, "mbLowDrive", d->sfEffects.mbLowDrive);
    _sf_writeFloat(f, "mbMidDrive", d->sfEffects.mbMidDrive);
    _sf_writeFloat(f, "mbHighDrive", d->sfEffects.mbHighDrive);
    _sf_writeBool(f, "vinylEnabled", d->sfEffects.vinylEnabled);
    _sf_writeFloat(f, "vinylCrackle", d->sfEffects.vinylCrackle);
    _sf_writeFloat(f, "vinylNoise", d->sfEffects.vinylNoise);
    _sf_writeFloat(f, "vinylWarp", d->sfEffects.vinylWarp);
    _sf_writeFloat(f, "vinylWarpRate", d->sfEffects.vinylWarpRate);
    _sf_writeFloat(f, "vinylToneLP", d->sfEffects.vinylToneLP);

    // Dub loop
    fprintf(f, "\n[dub]\n");
    _sf_writeBool(f, "enabled", d->sfDubLoop.enabled);
    _sf_writeFloat(f, "feedback", d->sfDubLoop.feedback);
    _sf_writeFloat(f, "mix", d->sfDubLoop.mix);
    _sf_writeInt(f, "inputSource", d->sfDubLoop.inputSource);
    _sf_writeBool(f, "preReverb", d->sfDubLoop.preReverb);
    _sf_writeFloat(f, "speed", d->sfDubLoop.speed);
    _sf_writeFloat(f, "saturation", d->sfDubLoop.saturation);
    _sf_writeFloat(f, "toneHigh", d->sfDubLoop.toneHigh);
    _sf_writeFloat(f, "toneLow", d->sfDubLoop.toneLow);
    _sf_writeFloat(f, "noise", d->sfDubLoop.noise);
    _sf_writeFloat(f, "degradeRate", d->sfDubLoop.degradeRate);
    _sf_writeFloat(f, "wow", d->sfDubLoop.wow);
    _sf_writeFloat(f, "flutter", d->sfDubLoop.flutter);
    _sf_writeFloat(f, "drift", d->sfDubLoop.drift);
    _sf_writeFloat(f, "driftRate", d->sfDubLoop.driftRate);
    _sf_writeInt(f, "numHeads", d->sfDubLoop.numHeads);
    for (int i = 0; i < d->sfDubLoop.numHeads && i < DUB_LOOP_MAX_HEADS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "headTime%d", i);
        _sf_writeFloat(f, key, d->sfDubLoop.headTime[i]);
        snprintf(key, sizeof(key), "headLevel%d", i);
        _sf_writeFloat(f, key, d->sfDubLoop.headLevel[i]);
    }

    // Per-bus effects
    static const char* busNames[] = {"drum0", "drum1", "drum2", "drum3", "bass", "lead", "chord", "sampler"};
    for (int b = 0; b < NUM_BUSES; b++) {
        const BusEffects *bus = &d->sfBusEffects[b];
        fprintf(f, "\n[bus.%s]\n", busNames[b]);
        _sf_writeFloat(f, "volume", bus->volume);
        _sf_writeFloat(f, "pan", bus->pan);
        _sf_writeBool(f, "mute", bus->mute);
        _sf_writeBool(f, "solo", bus->solo);
        _sf_writeBool(f, "filterEnabled", bus->filterEnabled);
        _sf_writeFloat(f, "filterCutoff", bus->filterCutoff);
        _sf_writeFloat(f, "filterResonance", bus->filterResonance);
        _sf_writeInt(f, "filterType", bus->filterType);
        _sf_writeBool(f, "distEnabled", bus->distEnabled);
        _sf_writeFloat(f, "distDrive", bus->distDrive);
        _sf_writeFloat(f, "distMix", bus->distMix);
        _sf_writeBool(f, "delayEnabled", bus->delayEnabled);
        _sf_writeFloat(f, "delayTime", bus->delayTime);
        _sf_writeFloat(f, "delayFeedback", bus->delayFeedback);
        _sf_writeFloat(f, "delayMix", bus->delayMix);
        _sf_writeBool(f, "delayTempoSync", bus->delayTempoSync);
        _sf_writeInt(f, "delaySyncDiv", bus->delaySyncDiv);
        _sf_writeBool(f, "eqEnabled", bus->eqEnabled);
        _sf_writeFloat(f, "eqLowGain", bus->eqLowGain);
        _sf_writeFloat(f, "eqHighGain", bus->eqHighGain);
        _sf_writeFloat(f, "eqLowFreq", bus->eqLowFreq);
        _sf_writeFloat(f, "eqHighFreq", bus->eqHighFreq);
        _sf_writeBool(f, "chorusEnabled", bus->chorusEnabled);
        _sf_writeBool(f, "chorusBBD", bus->chorusBBD);
        _sf_writeFloat(f, "chorusRate", bus->chorusRate);
        _sf_writeFloat(f, "chorusDepth", bus->chorusDepth);
        _sf_writeFloat(f, "chorusMix", bus->chorusMix);
        _sf_writeFloat(f, "chorusDelay", bus->chorusDelay);
        _sf_writeFloat(f, "chorusFeedback", bus->chorusFeedback);
        _sf_writeBool(f, "phaserEnabled", bus->phaserEnabled);
        _sf_writeFloat(f, "phaserRate", bus->phaserRate);
        _sf_writeFloat(f, "phaserDepth", bus->phaserDepth);
        _sf_writeFloat(f, "phaserMix", bus->phaserMix);
        _sf_writeFloat(f, "phaserFeedback", bus->phaserFeedback);
        _sf_writeInt(f, "phaserStages", bus->phaserStages);
        _sf_writeBool(f, "combEnabled", bus->combEnabled);
        _sf_writeFloat(f, "combFreq", bus->combFreq);
        _sf_writeFloat(f, "combFeedback", bus->combFeedback);
        _sf_writeFloat(f, "combMix", bus->combMix);
        _sf_writeFloat(f, "combDamping", bus->combDamping);
        _sf_writeBool(f, "octaverEnabled", bus->octaverEnabled);
        _sf_writeFloat(f, "octaverMix", bus->octaverMix);
        _sf_writeFloat(f, "octaverSubLevel", bus->octaverSubLevel);
        _sf_writeFloat(f, "octaverTone", bus->octaverTone);
        _sf_writeBool(f, "tremoloEnabled", bus->tremoloEnabled);
        _sf_writeFloat(f, "tremoloRate", bus->tremoloRate);
        _sf_writeFloat(f, "tremoloDepth", bus->tremoloDepth);
        _sf_writeInt(f, "tremoloShape", bus->tremoloShape);
        _sf_writeBool(f, "leslieEnabled", bus->leslieEnabled);
        _sf_writeInt(f, "leslieSpeed", bus->leslieSpeed);
        _sf_writeFloat(f, "leslieDrive", bus->leslieDrive);
        _sf_writeFloat(f, "leslieBalance", bus->leslieBalance);
        _sf_writeFloat(f, "leslieDoppler", bus->leslieDoppler);
        _sf_writeFloat(f, "leslieMix", bus->leslieMix);
        _sf_writeBool(f, "wahEnabled", bus->wahEnabled);
        _sf_writeInt(f, "wahMode", bus->wahMode);
        _sf_writeFloat(f, "wahRate", bus->wahRate);
        _sf_writeFloat(f, "wahSensitivity", bus->wahSensitivity);
        _sf_writeFloat(f, "wahFreqLow", bus->wahFreqLow);
        _sf_writeFloat(f, "wahFreqHigh", bus->wahFreqHigh);
        _sf_writeFloat(f, "wahResonance", bus->wahResonance);
        _sf_writeFloat(f, "wahMix", bus->wahMix);
        _sf_writeBool(f, "ringModEnabled", bus->ringModEnabled);
        _sf_writeFloat(f, "ringModFreq", bus->ringModFreq);
        _sf_writeFloat(f, "ringModMix", bus->ringModMix);
        _sf_writeFloat(f, "reverbSend", bus->reverbSend);
    }

    // Metadata
    fprintf(f, "\n[metadata]\n");
    _sf_writeStr(f, "author", d->author);
    _sf_writeStr(f, "description", d->description);
    _sf_writeStr(f, "mood", d->mood);
    if (d->energy[0]) fprintf(f, "energy = %s\n", d->energy);
    _sf_writeStr(f, "context", d->context);

    // Transitions
    fprintf(f, "\n[transitions]\n");
    _sf_writeFloat(f, "fadeIn", d->fadeIn);
    _sf_writeFloat(f, "fadeOut", d->fadeOut);
    _sf_writeBool(f, "crossfade", d->crossfade);

    // Chain
    if (d->chainLength > 0) {
        fprintf(f, "\n[chain]\norder =");
        for (int i = 0; i < d->chainLength; i++) {
            fprintf(f, "%s %d", i > 0 ? "," : "", d->chain[i]);
        }
        fprintf(f, "\n");
    }

    // Patterns
    for (int i = 0; i < d->patternCount; i++) {
        _sf_writePattern(f, i, &d->patterns[i]);
    }

    fclose(f);
    return true;
}

// Save a single patch to .patch file
static bool songFileSavePatch(const char *filepath, const SynthPatch *patch) {
    FILE *f = fopen(filepath, "w");
    if (!f) return false;
    fprintf(f, "# Patch file (format %d)\n", SONG_FILE_FORMAT_VERSION);
    _sf_writePatch(f, "patch", patch);
    fclose(f);
    return true;
}

// ============================================================================
// LOAD — Parse .song file
// ============================================================================

// Apply a key=value pair to the current section context
typedef enum {
    _SF_SEC_NONE,
    _SF_SEC_SONG,
    _SF_SEC_SCALE,
    _SF_SEC_GROOVE,
    _SF_SEC_INSTRUMENT,   // .bass, .lead, .chord
    _SF_SEC_DRUMS,
    _SF_SEC_MIX,
    _SF_SEC_EFFECTS,
    _SF_SEC_DUB,
    _SF_SEC_BUS,          // .drum0, .drum1, ..., .chord
    _SF_SEC_METADATA,
    _SF_SEC_TRANSITIONS,
    _SF_SEC_CHAIN,
    _SF_SEC_PATTERN,
    _SF_SEC_PATCH,        // standalone .patch file
} _SFSection;

// Read/parse aliases — implementations in file_helpers.h
#define _sf_parseFloat fileParseFloat
#define _sf_parseInt fileParseInt
#define _sf_parseBool fileParseBool
#define _sf_strip fileStrip
#define _sf_stripQuotes fileStripQuotes

static void _sf_applyPatchKV(SynthPatch *p, const char *key, const char *val) {
    if (strcmp(key, "name") == 0) { char tmp[64]; strncpy(tmp, val, 63); tmp[63]='\0'; _sf_stripQuotes(tmp); strncpy(p->p_name, tmp, 31); p->p_name[31]='\0'; }
    else if (strcmp(key, "waveType") == 0) {
        int idx = _sf_lookupName(val, _sf_waveTypeNames, _sf_waveTypeCount);
        p->p_waveType = idx >= 0 ? idx : _sf_parseInt(val);
    }
    else if (strcmp(key, "scwIndex") == 0) p->p_scwIndex = _sf_parseInt(val);
    else if (strcmp(key, "attack") == 0) p->p_attack = _sf_parseFloat(val);
    else if (strcmp(key, "decay") == 0) p->p_decay = _sf_parseFloat(val);
    else if (strcmp(key, "sustain") == 0) p->p_sustain = _sf_parseFloat(val);
    else if (strcmp(key, "release") == 0) p->p_release = _sf_parseFloat(val);
    else if (strcmp(key, "expRelease") == 0) p->p_expRelease = _sf_parseBool(val);
    else if (strcmp(key, "volume") == 0) p->p_volume = _sf_parseFloat(val);
    else if (strcmp(key, "pulseWidth") == 0) p->p_pulseWidth = _sf_parseFloat(val);
    else if (strcmp(key, "pwmRate") == 0) p->p_pwmRate = _sf_parseFloat(val);
    else if (strcmp(key, "pwmDepth") == 0) p->p_pwmDepth = _sf_parseFloat(val);
    else if (strcmp(key, "vibratoRate") == 0) p->p_vibratoRate = _sf_parseFloat(val);
    else if (strcmp(key, "vibratoDepth") == 0) p->p_vibratoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "filterCutoff") == 0) p->p_filterCutoff = _sf_parseFloat(val);
    else if (strcmp(key, "filterResonance") == 0) p->p_filterResonance = _sf_parseFloat(val);
    else if (strcmp(key, "filterEnvAmt") == 0) p->p_filterEnvAmt = _sf_parseFloat(val);
    else if (strcmp(key, "filterEnvAttack") == 0) p->p_filterEnvAttack = _sf_parseFloat(val);
    else if (strcmp(key, "filterEnvDecay") == 0) p->p_filterEnvDecay = _sf_parseFloat(val);
    else if (strcmp(key, "filterLfoRate") == 0) p->p_filterLfoRate = _sf_parseFloat(val);
    else if (strcmp(key, "filterLfoDepth") == 0) p->p_filterLfoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "filterLfoShape") == 0) p->p_filterLfoShape = _sf_parseInt(val);
    else if (strcmp(key, "filterLfoSync") == 0) p->p_filterLfoSync = _sf_parseInt(val);
    else if (strcmp(key, "resoLfoRate") == 0) p->p_resoLfoRate = _sf_parseFloat(val);
    else if (strcmp(key, "resoLfoDepth") == 0) p->p_resoLfoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "resoLfoShape") == 0) p->p_resoLfoShape = _sf_parseInt(val);
    else if (strcmp(key, "resoLfoSync") == 0) p->p_resoLfoSync = _sf_parseInt(val);
    else if (strcmp(key, "ampLfoRate") == 0) p->p_ampLfoRate = _sf_parseFloat(val);
    else if (strcmp(key, "ampLfoDepth") == 0) p->p_ampLfoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "ampLfoShape") == 0) p->p_ampLfoShape = _sf_parseInt(val);
    else if (strcmp(key, "ampLfoSync") == 0) p->p_ampLfoSync = _sf_parseInt(val);
    else if (strcmp(key, "pitchLfoRate") == 0) p->p_pitchLfoRate = _sf_parseFloat(val);
    else if (strcmp(key, "pitchLfoDepth") == 0) p->p_pitchLfoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "pitchLfoShape") == 0) p->p_pitchLfoShape = _sf_parseInt(val);
    else if (strcmp(key, "pitchLfoSync") == 0) p->p_pitchLfoSync = _sf_parseInt(val);
    else if (strcmp(key, "filterLfoPhaseOffset") == 0) p->p_filterLfoPhaseOffset = _sf_parseFloat(val);
    else if (strcmp(key, "resoLfoPhaseOffset") == 0) p->p_resoLfoPhaseOffset = _sf_parseFloat(val);
    else if (strcmp(key, "ampLfoPhaseOffset") == 0) p->p_ampLfoPhaseOffset = _sf_parseFloat(val);
    else if (strcmp(key, "pitchLfoPhaseOffset") == 0) p->p_pitchLfoPhaseOffset = _sf_parseFloat(val);
    else if (strcmp(key, "arpEnabled") == 0) p->p_arpEnabled = _sf_parseBool(val);
    else if (strcmp(key, "arpMode") == 0) p->p_arpMode = _sf_parseInt(val);
    else if (strcmp(key, "arpRateDiv") == 0) p->p_arpRateDiv = _sf_parseInt(val);
    else if (strcmp(key, "arpRate") == 0) p->p_arpRate = _sf_parseFloat(val);
    else if (strcmp(key, "arpChord") == 0) p->p_arpChord = _sf_parseInt(val);
    else if (strcmp(key, "unisonCount") == 0) p->p_unisonCount = _sf_parseInt(val);
    else if (strcmp(key, "unisonDetune") == 0) p->p_unisonDetune = _sf_parseFloat(val);
    else if (strcmp(key, "unisonMix") == 0) p->p_unisonMix = _sf_parseFloat(val);
    else if (strcmp(key, "monoMode") == 0) p->p_monoMode = _sf_parseBool(val);
    else if (strcmp(key, "monoRetrigger") == 0) p->p_monoRetrigger = _sf_parseBool(val);
    else if (strcmp(key, "glideTime") == 0) p->p_glideTime = _sf_parseFloat(val);
    else if (strcmp(key, "legatoWindow") == 0) p->p_legatoWindow = _sf_parseFloat(val);
    else if (strcmp(key, "notePriority") == 0) p->p_notePriority = _sf_parseInt(val);
    else if (strcmp(key, "pluckBrightness") == 0) p->p_pluckBrightness = _sf_parseFloat(val);
    else if (strcmp(key, "pluckDamping") == 0) p->p_pluckDamping = _sf_parseFloat(val);
    else if (strcmp(key, "pluckDamp") == 0) p->p_pluckDamp = _sf_parseFloat(val);
    else if (strcmp(key, "additivePreset") == 0) p->p_additivePreset = _sf_parseInt(val);
    else if (strcmp(key, "additiveBrightness") == 0) p->p_additiveBrightness = _sf_parseFloat(val);
    else if (strcmp(key, "additiveShimmer") == 0) p->p_additiveShimmer = _sf_parseFloat(val);
    else if (strcmp(key, "additiveInharmonicity") == 0) p->p_additiveInharmonicity = _sf_parseFloat(val);
    else if (strcmp(key, "malletPreset") == 0) p->p_malletPreset = _sf_parseInt(val);
    else if (strcmp(key, "malletStiffness") == 0) p->p_malletStiffness = _sf_parseFloat(val);
    else if (strcmp(key, "malletHardness") == 0) p->p_malletHardness = _sf_parseFloat(val);
    else if (strcmp(key, "malletStrikePos") == 0) p->p_malletStrikePos = _sf_parseFloat(val);
    else if (strcmp(key, "malletResonance") == 0) p->p_malletResonance = _sf_parseFloat(val);
    else if (strcmp(key, "malletTremolo") == 0) p->p_malletTremolo = _sf_parseFloat(val);
    else if (strcmp(key, "malletTremoloRate") == 0) p->p_malletTremoloRate = _sf_parseFloat(val);
    else if (strcmp(key, "malletDamp") == 0) p->p_malletDamp = _sf_parseFloat(val);
    else if (strcmp(key, "voiceVowel") == 0) p->p_voiceVowel = _sf_parseInt(val);
    else if (strcmp(key, "voiceFormantShift") == 0) p->p_voiceFormantShift = _sf_parseFloat(val);
    else if (strcmp(key, "voiceBreathiness") == 0) p->p_voiceBreathiness = _sf_parseFloat(val);
    else if (strcmp(key, "voiceBuzziness") == 0) p->p_voiceBuzziness = _sf_parseFloat(val);
    else if (strcmp(key, "voiceSpeed") == 0) p->p_voiceSpeed = _sf_parseFloat(val);
    else if (strcmp(key, "voicePitch") == 0) p->p_voicePitch = _sf_parseFloat(val);
    else if (strcmp(key, "voiceConsonant") == 0) p->p_voiceConsonant = _sf_parseBool(val);
    else if (strcmp(key, "voiceConsonantAmt") == 0) p->p_voiceConsonantAmt = _sf_parseFloat(val);
    else if (strcmp(key, "voiceNasal") == 0) p->p_voiceNasal = _sf_parseBool(val);
    else if (strcmp(key, "voiceNasalAmt") == 0) p->p_voiceNasalAmt = _sf_parseFloat(val);
    else if (strcmp(key, "voicePitchEnv") == 0) p->p_voicePitchEnv = _sf_parseFloat(val);
    else if (strcmp(key, "voicePitchEnvTime") == 0) p->p_voicePitchEnvTime = _sf_parseFloat(val);
    else if (strcmp(key, "voicePitchEnvCurve") == 0) p->p_voicePitchEnvCurve = _sf_parseFloat(val);
    else if (strcmp(key, "granularScwIndex") == 0) p->p_granularScwIndex = _sf_parseInt(val);
    else if (strcmp(key, "granularGrainSize") == 0) p->p_granularGrainSize = _sf_parseFloat(val);
    else if (strcmp(key, "granularDensity") == 0) p->p_granularDensity = _sf_parseFloat(val);
    else if (strcmp(key, "granularPosition") == 0) p->p_granularPosition = _sf_parseFloat(val);
    else if (strcmp(key, "granularPosRandom") == 0) p->p_granularPosRandom = _sf_parseFloat(val);
    else if (strcmp(key, "granularPitch") == 0) p->p_granularPitch = _sf_parseFloat(val);
    else if (strcmp(key, "granularPitchRandom") == 0) p->p_granularPitchRandom = _sf_parseFloat(val);
    else if (strcmp(key, "granularAmpRandom") == 0) p->p_granularAmpRandom = _sf_parseFloat(val);
    else if (strcmp(key, "granularSpread") == 0) p->p_granularSpread = _sf_parseFloat(val);
    else if (strcmp(key, "granularFreeze") == 0) p->p_granularFreeze = _sf_parseBool(val);
    else if (strcmp(key, "fmLfoRate") == 0) p->p_fmLfoRate = _sf_parseFloat(val);
    else if (strcmp(key, "fmLfoDepth") == 0) p->p_fmLfoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "fmLfoShape") == 0) p->p_fmLfoShape = _sf_parseInt(val);
    else if (strcmp(key, "fmLfoSync") == 0) p->p_fmLfoSync = _sf_parseInt(val);
    else if (strcmp(key, "fmLfoPhaseOffset") == 0) p->p_fmLfoPhaseOffset = _sf_parseFloat(val);
    else if (strcmp(key, "fmModRatio") == 0) p->p_fmModRatio = _sf_parseFloat(val);
    else if (strcmp(key, "fmModIndex") == 0) p->p_fmModIndex = _sf_parseFloat(val);
    else if (strcmp(key, "fmFeedback") == 0) p->p_fmFeedback = _sf_parseFloat(val);
    else if (strcmp(key, "fmMod2Ratio") == 0) p->p_fmMod2Ratio = _sf_parseFloat(val);
    else if (strcmp(key, "fmMod2Index") == 0) p->p_fmMod2Index = _sf_parseFloat(val);
    else if (strcmp(key, "fmAlgorithm") == 0) p->p_fmAlgorithm = _sf_parseInt(val);
    else if (strcmp(key, "pdWaveType") == 0) p->p_pdWaveType = _sf_parseInt(val);
    else if (strcmp(key, "pdDistortion") == 0) p->p_pdDistortion = _sf_parseFloat(val);
    else if (strcmp(key, "membranePreset") == 0) p->p_membranePreset = _sf_parseInt(val);
    else if (strcmp(key, "membraneDamping") == 0) p->p_membraneDamping = _sf_parseFloat(val);
    else if (strcmp(key, "membraneStrike") == 0) p->p_membraneStrike = _sf_parseFloat(val);
    else if (strcmp(key, "membraneBend") == 0) p->p_membraneBend = _sf_parseFloat(val);
    else if (strcmp(key, "membraneBendDecay") == 0) p->p_membraneBendDecay = _sf_parseFloat(val);
    else if (strcmp(key, "metallicPreset") == 0) p->p_metallicPreset = _sf_parseInt(val);
    else if (strcmp(key, "metallicRingMix") == 0) p->p_metallicRingMix = _sf_parseFloat(val);
    else if (strcmp(key, "metallicNoiseLevel") == 0) p->p_metallicNoiseLevel = _sf_parseFloat(val);
    else if (strcmp(key, "metallicBrightness") == 0) p->p_metallicBrightness = _sf_parseFloat(val);
    else if (strcmp(key, "metallicPitchEnv") == 0) p->p_metallicPitchEnv = _sf_parseFloat(val);
    else if (strcmp(key, "metallicPitchEnvDecay") == 0) p->p_metallicPitchEnvDecay = _sf_parseFloat(val);
    else if (strcmp(key, "guitarPreset") == 0) p->p_guitarPreset = _sf_parseInt(val);
    else if (strcmp(key, "guitarBodyMix") == 0) p->p_guitarBodyMix = _sf_parseFloat(val);
    else if (strcmp(key, "guitarBodyBrightness") == 0) p->p_guitarBodyBrightness = _sf_parseFloat(val);
    else if (strcmp(key, "guitarPickPosition") == 0) p->p_guitarPickPosition = _sf_parseFloat(val);
    else if (strcmp(key, "guitarBuzz") == 0) p->p_guitarBuzz = _sf_parseFloat(val);
    else if (strcmp(key, "bowPressure") == 0) p->p_bowPressure = _sf_parseFloat(val);
    else if (strcmp(key, "bowSpeed") == 0) p->p_bowSpeed = _sf_parseFloat(val);
    else if (strcmp(key, "bowPosition") == 0) p->p_bowPosition = _sf_parseFloat(val);
    else if (strcmp(key, "pipeBreath") == 0) p->p_pipeBreath = _sf_parseFloat(val);
    else if (strcmp(key, "pipeEmbouchure") == 0) p->p_pipeEmbouchure = _sf_parseFloat(val);
    else if (strcmp(key, "pipeBore") == 0) p->p_pipeBore = _sf_parseFloat(val);
    else if (strcmp(key, "pipeOverblow") == 0) p->p_pipeOverblow = _sf_parseFloat(val);
    else if (strcmp(key, "reedStiffness") == 0) p->p_reedStiffness = _sf_parseFloat(val);
    else if (strcmp(key, "reedAperture") == 0) p->p_reedAperture = _sf_parseFloat(val);
    else if (strcmp(key, "reedBlowPressure") == 0) p->p_reedBlowPressure = _sf_parseFloat(val);
    else if (strcmp(key, "reedBore") == 0) p->p_reedBore = _sf_parseFloat(val);
    else if (strcmp(key, "reedVibratoDepth") == 0) p->p_reedVibratoDepth = _sf_parseFloat(val);
    else if (strcmp(key, "brassBlowPressure") == 0) p->p_brassBlowPressure = _sf_parseFloat(val);
    else if (strcmp(key, "brassLipTension") == 0) p->p_brassLipTension = _sf_parseFloat(val);
    else if (strcmp(key, "brassLipAperture") == 0) p->p_brassLipAperture = _sf_parseFloat(val);
    else if (strcmp(key, "brassBore") == 0) p->p_brassBore = _sf_parseFloat(val);
    else if (strcmp(key, "brassMute") == 0) p->p_brassMute = _sf_parseFloat(val);
    else if (strcmp(key, "birdType") == 0) p->p_birdType = _sf_parseInt(val);
    else if (strcmp(key, "birdChirpRange") == 0) p->p_birdChirpRange = _sf_parseFloat(val);
    else if (strcmp(key, "birdTrillRate") == 0) p->p_birdTrillRate = _sf_parseFloat(val);
    else if (strcmp(key, "birdTrillDepth") == 0) p->p_birdTrillDepth = _sf_parseFloat(val);
    else if (strcmp(key, "birdAmRate") == 0) p->p_birdAmRate = _sf_parseFloat(val);
    else if (strcmp(key, "birdAmDepth") == 0) p->p_birdAmDepth = _sf_parseFloat(val);
    else if (strcmp(key, "birdHarmonics") == 0) p->p_birdHarmonics = _sf_parseFloat(val);
    // Electric piano
    else if (strcmp(key, "epHardness") == 0) p->p_epHardness = _sf_parseFloat(val);
    else if (strcmp(key, "epToneBar") == 0) p->p_epToneBar = _sf_parseFloat(val);
    else if (strcmp(key, "epPickupPos") == 0) p->p_epPickupPos = _sf_parseFloat(val);
    else if (strcmp(key, "epPickupDist") == 0) p->p_epPickupDist = _sf_parseFloat(val);
    else if (strcmp(key, "epDecay") == 0) p->p_epDecay = _sf_parseFloat(val);
    else if (strcmp(key, "epBell") == 0) p->p_epBell = _sf_parseFloat(val);
    else if (strcmp(key, "epBellTone") == 0) p->p_epBellTone = _sf_parseFloat(val);
    else if (strcmp(key, "epPickupType") == 0) p->p_epPickupType = _sf_parseInt(val);
    // Organ
    else if (strncmp(key, "orgDrawbar", 10) == 0) {
        int idx = atoi(key + 10);
        if (idx >= 0 && idx < ORGAN_DRAWBARS) p->p_orgDrawbar[idx] = _sf_parseFloat(val);
    }
    else if (strcmp(key, "orgClick") == 0) p->p_orgClick = _sf_parseFloat(val);
    else if (strcmp(key, "orgPercOn") == 0) p->p_orgPercOn = _sf_parseInt(val);
    else if (strcmp(key, "orgPercHarmonic") == 0) p->p_orgPercHarmonic = _sf_parseInt(val);
    else if (strcmp(key, "orgPercSoft") == 0) p->p_orgPercSoft = _sf_parseInt(val);
    else if (strcmp(key, "orgPercFast") == 0) p->p_orgPercFast = _sf_parseInt(val);
    else if (strcmp(key, "orgCrosstalk") == 0) p->p_orgCrosstalk = _sf_parseFloat(val);
    else if (strcmp(key, "orgVibratoMode") == 0) p->p_orgVibratoMode = _sf_parseInt(val);
    // Pitch envelope
    else if (strcmp(key, "pitchEnvAmount") == 0) p->p_pitchEnvAmount = _sf_parseFloat(val);
    else if (strcmp(key, "pitchEnvDecay") == 0) p->p_pitchEnvDecay = _sf_parseFloat(val);
    else if (strcmp(key, "pitchEnvCurve") == 0) p->p_pitchEnvCurve = _sf_parseFloat(val);
    else if (strcmp(key, "pitchEnvLinear") == 0) p->p_pitchEnvLinear = _sf_parseBool(val);
    // Noise mix
    else if (strcmp(key, "noiseMix") == 0) p->p_noiseMix = _sf_parseFloat(val);
    else if (strcmp(key, "noiseTone") == 0) p->p_noiseTone = _sf_parseFloat(val);
    else if (strcmp(key, "noiseHP") == 0) p->p_noiseHP = _sf_parseFloat(val);
    else if (strcmp(key, "noiseDecay") == 0) p->p_noiseDecay = _sf_parseFloat(val);
    // Retrigger
    else if (strcmp(key, "retriggerCount") == 0) p->p_retriggerCount = _sf_parseInt(val);
    else if (strcmp(key, "retriggerSpread") == 0) p->p_retriggerSpread = _sf_parseFloat(val);
    else if (strcmp(key, "retriggerOverlap") == 0) p->p_retriggerOverlap = _sf_parseBool(val);
    else if (strcmp(key, "retriggerBurstDecay") == 0) p->p_retriggerBurstDecay = _sf_parseFloat(val);
    // Extra oscillators
    else if (strcmp(key, "osc2Ratio") == 0) p->p_osc2Ratio = _sf_parseFloat(val);
    else if (strcmp(key, "osc2Level") == 0) p->p_osc2Level = _sf_parseFloat(val);
    else if (strcmp(key, "osc2Decay") == 0) p->p_osc2Decay = _sf_parseFloat(val);
    else if (strcmp(key, "osc3Ratio") == 0) p->p_osc3Ratio = _sf_parseFloat(val);
    else if (strcmp(key, "osc3Level") == 0) p->p_osc3Level = _sf_parseFloat(val);
    else if (strcmp(key, "osc3Decay") == 0) p->p_osc3Decay = _sf_parseFloat(val);
    else if (strcmp(key, "osc4Ratio") == 0) p->p_osc4Ratio = _sf_parseFloat(val);
    else if (strcmp(key, "osc4Level") == 0) p->p_osc4Level = _sf_parseFloat(val);
    else if (strcmp(key, "osc4Decay") == 0) p->p_osc4Decay = _sf_parseFloat(val);
    else if (strcmp(key, "osc5Ratio") == 0) p->p_osc5Ratio = _sf_parseFloat(val);
    else if (strcmp(key, "osc5Level") == 0) p->p_osc5Level = _sf_parseFloat(val);
    else if (strcmp(key, "osc5Decay") == 0) p->p_osc5Decay = _sf_parseFloat(val);
    else if (strcmp(key, "osc6Ratio") == 0) p->p_osc6Ratio = _sf_parseFloat(val);
    else if (strcmp(key, "osc6Level") == 0) p->p_osc6Level = _sf_parseFloat(val);
    else if (strcmp(key, "osc6Decay") == 0) p->p_osc6Decay = _sf_parseFloat(val);
    else if (strcmp(key, "oscVelSens") == 0) p->p_oscVelSens = _sf_parseFloat(val);
    else if (strcmp(key, "velToFilter") == 0) p->p_velToFilter = _sf_parseFloat(val);
    else if (strcmp(key, "velToClick") == 0) p->p_velToClick = _sf_parseFloat(val);
    else if (strcmp(key, "velToDrive") == 0) p->p_velToDrive = _sf_parseFloat(val);
    // Drive, click, filter type
    else if (strcmp(key, "drive") == 0) p->p_drive = _sf_parseFloat(val);
    else if (strcmp(key, "driveMode") == 0) p->p_driveMode = _sf_parseInt(val);
    else if (strcmp(key, "clickLevel") == 0) p->p_clickLevel = _sf_parseFloat(val);
    else if (strcmp(key, "clickTime") == 0) p->p_clickTime = _sf_parseFloat(val);
    else if (strcmp(key, "filterType") == 0) p->p_filterType = _sf_parseInt(val);
    else if (strcmp(key, "filterModel") == 0) p->p_filterModel = _sf_parseInt(val);
    // Algorithm modes
    else if (strcmp(key, "noiseMode") == 0) p->p_noiseMode = _sf_parseInt(val);
    else if (strcmp(key, "oscMixMode") == 0) p->p_oscMixMode = _sf_parseInt(val);
    else if (strcmp(key, "retriggerCurve") == 0) p->p_retriggerCurve = _sf_parseFloat(val);
    else if (strcmp(key, "phaseReset") == 0) p->p_phaseReset = _sf_parseBool(val);
    else if (strcmp(key, "noiseLPBypass") == 0) p->p_noiseLPBypass = _sf_parseBool(val);
    else if (strcmp(key, "noiseType") == 0) p->p_noiseType = _sf_parseInt(val);
    // Envelope flags
    else if (strcmp(key, "expDecay") == 0) p->p_expDecay = _sf_parseBool(val);
    else if (strcmp(key, "oneShot") == 0) p->p_oneShot = _sf_parseBool(val);
    // Envelope/filter bypass toggles
    else if (strcmp(key, "envelopeEnabled") == 0) p->p_envelopeEnabled = _sf_parseBool(val);
    else if (strcmp(key, "filterEnabled") == 0) p->p_filterEnabled = _sf_parseBool(val);
    // 303 acid mode
    else if (strcmp(key, "acidMode") == 0) p->p_acidMode = _sf_parseBool(val);
    else if (strcmp(key, "accentSweepAmt") == 0) p->p_accentSweepAmt = _sf_parseFloat(val);
    else if (strcmp(key, "gimmickDipAmt") == 0) p->p_gimmickDipAmt = _sf_parseFloat(val);
    // Unknown keys silently skipped (future-proof)
}

static void _sf_parseDrumEvent(const char *line, Pattern *p) {
    char lineCopy[SONG_FILE_MAX_LINE];
    strncpy(lineCopy, line, SONG_FILE_MAX_LINE - 1);
    lineCopy[SONG_FILE_MAX_LINE - 1] = '\0';
    char *tokens[32];
    int n = _sf_tokenize(lineCopy + 1, tokens, 32); // skip 'd'

    int track = 0, step = 0;
    float vel = 0.8f, pitch = 0.0f, prob = 1.0f;
    int cond = COND_ALWAYS;

    char key[32], val[64];
    for (int i = 0; i < n; i++) {
        if (_sf_parseKV(tokens[i], key, 32, val, 64)) {
            if (strcmp(key, "track") == 0) track = _sf_parseInt(val);
            else if (strcmp(key, "step") == 0) step = _sf_parseInt(val);
            else if (strcmp(key, "vel") == 0) vel = _sf_parseFloat(val);
            else if (strcmp(key, "pitch") == 0) pitch = _sf_parseFloat(val);
            else if (strcmp(key, "prob") == 0) prob = _sf_parseFloat(val);
            else if (strcmp(key, "cond") == 0) {
                int idx = _sf_lookupName(val, _sf_conditionNames, _sf_conditionCount);
                if (idx >= 0) cond = idx;
            }
        }
    }

    if (track >= 0 && track < SEQ_DRUM_TRACKS && step >= 0 && step < SEQ_MAX_STEPS) {
        patSetDrum(p, track, step, vel, pitch);
        patSetDrumProb(p, track, step, prob);
        patSetDrumCond(p, track, step, cond);
    }
}

static void _sf_parseMelodyEvent(const char *line, Pattern *p) {
    char lineCopy[SONG_FILE_MAX_LINE];
    strncpy(lineCopy, line, SONG_FILE_MAX_LINE - 1);
    lineCopy[SONG_FILE_MAX_LINE - 1] = '\0';
    char *tokens[32];
    int n = _sf_tokenize(lineCopy + 1, tokens, 32); // skip 'm'

    int track = 0, step = 0, gate = 4, sustain = 0;
    int nudge = 0, gateNudge = 0;
    char noteName[16] = "C4";
    float vel = 0.8f, prob = 1.0f;
    int cond = COND_ALWAYS;
    int pickMode = PICK_ALL;
    bool slide = false, accent = false;
    bool hasChord = false;
    int chordType = CHORD_SINGLE;
    int customNotes[8] = {0};
    int customNoteCount = 0;

    char key[32], val[64];
    for (int i = 0; i < n; i++) {
        if (_sf_parseKV(tokens[i], key, 32, val, 64)) {
            if (strcmp(key, "track") == 0) track = _sf_parseInt(val);
            else if (strcmp(key, "step") == 0) step = _sf_parseInt(val);
            else if (strcmp(key, "note") == 0) strncpy(noteName, val, 15);
            else if (strcmp(key, "vel") == 0) vel = _sf_parseFloat(val);
            else if (strcmp(key, "gate") == 0) gate = _sf_parseInt(val);
            else if (strcmp(key, "sustain") == 0) sustain = _sf_parseInt(val);
            else if (strcmp(key, "nudge") == 0) nudge = _sf_parseInt(val);
            else if (strcmp(key, "gateNudge") == 0) gateNudge = _sf_parseInt(val);
            else if (strcmp(key, "prob") == 0) prob = _sf_parseFloat(val);
            else if (strcmp(key, "cond") == 0) {
                int idx = _sf_lookupName(val, _sf_conditionNames, _sf_conditionCount);
                if (idx >= 0) cond = idx;
            }
            else if (strcmp(key, "chord") == 0) {
                // Legacy NotePool chord → convert to v2 multi-note step
                hasChord = true;
                int idx = _sf_lookupName(val, _sf_chordTypeNames, _sf_chordTypeCount);
                if (idx >= 0) chordType = idx;
            }
            else if (strcmp(key, "pick") == 0) {
                int pv = _sf_parseInt(val);
                if (pv >= 0 && pv < PICK_COUNT) pickMode = pv;
            }
            else if (strcmp(key, "notes") == 0) {
                // Legacy custom chord notes: "C4,E4,G4" → v2 multi-note step
                hasChord = true;
                chordType = CHORD_CUSTOM;
                char notesBuf[128];
                strncpy(notesBuf, val, 127);
                notesBuf[127] = '\0';
                char *tok = strtok(notesBuf, ",");
                while (tok && customNoteCount < 8) {
                    customNotes[customNoteCount++] = _sf_nameToMidi(tok);
                    tok = strtok(NULL, ",");
                }
            }
        } else {
            // Bare word flags
            if (strcmp(key, "slide") == 0) slide = true;
            else if (strcmp(key, "accent") == 0) accent = true;
        }
    }

    if (track >= 0 && track < SEQ_MELODY_TRACKS && step >= 0 && step < SEQ_MAX_STEPS) {
        int absTrack = SEQ_DRUM_TRACKS + track;
        if (hasChord && chordType == CHORD_CUSTOM && customNoteCount > 0) {
            // Legacy custom chord → v2 multi-note step
            patSetChordCustom(p, absTrack, step, vel, gate,
                customNoteCount > 0 ? customNotes[0] : -1,
                customNoteCount > 1 ? customNotes[1] : -1,
                customNoteCount > 2 ? customNotes[2] : -1,
                customNoteCount > 3 ? customNotes[3] : -1);
        } else if (hasChord) {
            // Legacy standard chord → v2 multi-note step
            patSetChord(p, absTrack, step, _sf_nameToMidi(noteName), (ChordType)chordType, vel, gate);
        } else {
            // v2 polyphony: if step already has notes, add to it (chord from multiple m lines)
            StepV2 *sv = &p->steps[absTrack][step];
            if (sv->noteCount > 0) {
                int vi = stepV2AddNote(sv, _sf_nameToMidi(noteName), velFloatToU8(vel), (int8_t)gate);
                if (vi >= 0) {
                    sv->notes[vi].slide = slide;
                    sv->notes[vi].accent = accent;
                    sv->notes[vi].nudge = (int8_t)nudge;
                    sv->notes[vi].gateNudge = (int8_t)gateNudge;
                }
                // Per-step fields: prob/cond/sustain from first voice, don't overwrite
                return;
            }
            patSetNote(p, absTrack, step, _sf_nameToMidi(noteName), vel, gate);
        }
        // Set per-voice fields on notes[0] (or legacy chord's first note)
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

static void _sf_parsePlockEvent(const char *line, Pattern *p) {
    char lineCopy[SONG_FILE_MAX_LINE];
    strncpy(lineCopy, line, SONG_FILE_MAX_LINE - 1);
    lineCopy[SONG_FILE_MAX_LINE - 1] = '\0';
    char *tokens[16];
    int n = _sf_tokenize(lineCopy + 1, tokens, 16); // skip 'p'

    int track = 0, step = 0, param = -1;
    float value = 0.0f;

    char key[32], val[64];
    for (int i = 0; i < n; i++) {
        if (_sf_parseKV(tokens[i], key, 32, val, 64)) {
            if (strcmp(key, "track") == 0) track = _sf_parseInt(val);
            else if (strcmp(key, "step") == 0) step = _sf_parseInt(val);
            else if (strcmp(key, "param") == 0) {
                int idx = _sf_lookupName(val, _sf_plockParamNames, _sf_plockParamCount);
                param = idx >= 0 ? idx : _sf_parseInt(val);
            }
            else if (strcmp(key, "val") == 0) value = _sf_parseFloat(val);
        }
    }

    if (param >= 0 && param < PLOCK_COUNT) {
        seqSetPLock(p, track, step, param, value);
    }
}

static int _sf_parseBusIndex(const char *name) {
    if (strcmp(name, "drum0") == 0) return BUS_DRUM0;
    if (strcmp(name, "drum1") == 0) return BUS_DRUM1;
    if (strcmp(name, "drum2") == 0) return BUS_DRUM2;
    if (strcmp(name, "drum3") == 0) return BUS_DRUM3;
    if (strcmp(name, "bass") == 0) return BUS_BASS;
    if (strcmp(name, "lead") == 0) return BUS_LEAD;
    if (strcmp(name, "chord") == 0) return BUS_CHORD;
    if (strcmp(name, "sampler") == 0) return BUS_SAMPLER;
    return -1;
}

static int _sf_parseInstrumentIndex(const char *name) {
    if (strcmp(name, "bass") == 0) return 0;
    if (strcmp(name, "lead") == 0) return 1;
    if (strcmp(name, "chord") == 0) return 2;
    return -1;
}

// Parse int list like "16 16 16 16"
static int _sf_parseIntList(const char *val, int *out, int maxCount) {
    char buf[128];
    strncpy(buf, val, 127);
    buf[127] = '\0';
    int count = 0;
    char *tok = strtok(buf, " ,\t");
    while (tok && count < maxCount) {
        out[count++] = atoi(tok);
        tok = strtok(NULL, " ,\t");
    }
    return count;
}

static bool songFileLoad(const char *filepath, SongFileData *d) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    songFileDataInit(d);

    _SFSection section = _SF_SEC_NONE;
    int subIndex = -1;      // instrument index, bus index, pattern index
    char line[SONG_FILE_MAX_LINE];

    while (fgets(line, SONG_FILE_MAX_LINE, f)) {
        char *s = _sf_strip(line);
        if (!*s || *s == '#') continue;

        // Section header
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end) continue;
            *end = '\0';
            char *secName = s + 1;

            if (strcmp(secName, "song") == 0) { section = _SF_SEC_SONG; }
            else if (strcmp(secName, "scale") == 0) { section = _SF_SEC_SCALE; }
            else if (strcmp(secName, "groove") == 0) { section = _SF_SEC_GROOVE; }
            else if (strcmp(secName, "drums") == 0) { section = _SF_SEC_DRUMS; }
            else if (strcmp(secName, "mix") == 0) { section = _SF_SEC_MIX; }
            else if (strcmp(secName, "effects") == 0) { section = _SF_SEC_EFFECTS; }
            else if (strcmp(secName, "dub") == 0) { section = _SF_SEC_DUB; }
            else if (strcmp(secName, "metadata") == 0) { section = _SF_SEC_METADATA; }
            else if (strcmp(secName, "transitions") == 0) { section = _SF_SEC_TRANSITIONS; }
            else if (strcmp(secName, "chain") == 0) { section = _SF_SEC_CHAIN; }
            else if (strcmp(secName, "patch") == 0) { section = _SF_SEC_PATCH; subIndex = 0; }
            else if (strncmp(secName, "instrument.", 11) == 0) {
                section = _SF_SEC_INSTRUMENT;
                subIndex = _sf_parseInstrumentIndex(secName + 11);
            }
            else if (strncmp(secName, "bus.", 4) == 0) {
                section = _SF_SEC_BUS;
                subIndex = _sf_parseBusIndex(secName + 4);
            }
            else if (strncmp(secName, "pattern.", 8) == 0) {
                section = _SF_SEC_PATTERN;
                subIndex = atoi(secName + 8);
                if (subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
                    initPattern(&d->patterns[subIndex]);
                }
            }
            else {
                section = _SF_SEC_NONE; // Unknown section — skip
            }
            continue;
        }

        // Event lines (in pattern sections)
        if (section == _SF_SEC_PATTERN && subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
            if (s[0] == 'd' && s[1] == ' ') {
                _sf_parseDrumEvent(s, &d->patterns[subIndex]);
                continue;
            }
            if (s[0] == 'm' && s[1] == ' ') {
                _sf_parseMelodyEvent(s, &d->patterns[subIndex]);
                continue;
            }
            if (s[0] == 'p' && s[1] == ' ') {
                _sf_parsePlockEvent(s, &d->patterns[subIndex]);
                continue;
            }
        }

        // Key = value
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = _sf_strip(s);
        char *val = _sf_strip(eq + 1);
        _sf_stripQuotes(val);

        switch (section) {
        case _SF_SEC_SONG:
            if (strcmp(key, "name") == 0) strncpy(d->name, val, SONG_FILE_MAX_NAME - 1);
            else if (strcmp(key, "bpm") == 0) d->bpm = _sf_parseFloat(val);
            else if (strcmp(key, "ticksPerStep") == 0) d->ticksPerStep = _sf_parseInt(val);
            else if (strcmp(key, "loopsPerPattern") == 0) d->loopsPerPattern = _sf_parseInt(val);
            else if (strcmp(key, "drumTracks") == 0) d->drumTracks = _sf_parseInt(val);
            else if (strcmp(key, "melodyTracks") == 0) d->melodyTracks = _sf_parseInt(val);
            break;

        case _SF_SEC_SCALE:
            if (strcmp(key, "enabled") == 0) d->songScaleLockEnabled = _sf_parseBool(val);
            else if (strcmp(key, "root") == 0) d->songScaleRoot = _sf_parseInt(val);
            else if (strcmp(key, "type") == 0) {
                int idx = _sf_lookupName(val, _sf_scaleTypeNames, _sf_scaleTypeCount);
                d->songScaleType = idx >= 0 ? idx : _sf_parseInt(val);
            }
            break;

        case _SF_SEC_GROOVE:
            if (strcmp(key, "kickNudge") == 0) d->dilla.kickNudge = _sf_parseInt(val);
            else if (strcmp(key, "snareDelay") == 0) d->dilla.snareDelay = _sf_parseInt(val);
            else if (strcmp(key, "hatNudge") == 0) d->dilla.hatNudge = _sf_parseInt(val);
            else if (strcmp(key, "clapDelay") == 0) d->dilla.clapDelay = _sf_parseInt(val);
            else if (strcmp(key, "swing") == 0) d->dilla.swing = _sf_parseInt(val);
            else if (strcmp(key, "jitter") == 0) d->dilla.jitter = _sf_parseInt(val);
            else if (strncmp(key, "trackSwing", 10) == 0) {
                int t = atoi(key + 10);
                if (t >= 0 && t < SEQ_V2_MAX_TRACKS) d->trackSwing[t] = _sf_parseInt(val);
            }
            else if (strncmp(key, "trackTranspose", 14) == 0) {
                int t = atoi(key + 14);
                if (t >= 0 && t < SEQ_V2_MAX_TRACKS) d->trackTranspose[t] = _sf_parseInt(val);
            }
            else if (strcmp(key, "melodyTimingJitter") == 0) d->humanize.timingJitter = _sf_parseInt(val);
            else if (strcmp(key, "melodyVelocityJitter") == 0) d->humanize.velocityJitter = _sf_parseFloat(val);
            break;

        case _SF_SEC_INSTRUMENT:
            if (subIndex >= 0 && subIndex < 3)
                _sf_applyPatchKV(&d->instruments[subIndex], key, val);
            break;

        case _SF_SEC_PATCH:
            _sf_applyPatchKV(&d->instruments[0], key, val); // standalone .patch → slot 0
            break;

        case _SF_SEC_DRUMS:
            break;  // Legacy: silently ignore [drums] sections from old .song files

        case _SF_SEC_MIX:
            if (strcmp(key, "masterVolume") == 0) d->sfMasterVolume = _sf_parseFloat(val);
            else if (strcmp(key, "drumVolume") == 0) d->sfDrumVolume = _sf_parseFloat(val);
            else if (strncmp(key, "track", 5) == 0 && isdigit(key[5])) {
                int t = key[5] - '0';
                if (t >= 0 && t < SEQ_V2_MAX_TRACKS) d->sfTrackVolume[t] = _sf_parseFloat(val);
            }
            break;

        case _SF_SEC_EFFECTS:
            if (strcmp(key, "distEnabled") == 0) d->sfEffects.distEnabled = _sf_parseBool(val);
            else if (strcmp(key, "distDrive") == 0) d->sfEffects.distDrive = _sf_parseFloat(val);
            else if (strcmp(key, "distTone") == 0) d->sfEffects.distTone = _sf_parseFloat(val);
            else if (strcmp(key, "distMix") == 0) d->sfEffects.distMix = _sf_parseFloat(val);
            else if (strcmp(key, "delayEnabled") == 0) d->sfEffects.delayEnabled = _sf_parseBool(val);
            else if (strcmp(key, "delayTime") == 0) d->sfEffects.delayTime = _sf_parseFloat(val);
            else if (strcmp(key, "delayFeedback") == 0) d->sfEffects.delayFeedback = _sf_parseFloat(val);
            else if (strcmp(key, "delayMix") == 0) d->sfEffects.delayMix = _sf_parseFloat(val);
            else if (strcmp(key, "delayTone") == 0) d->sfEffects.delayTone = _sf_parseFloat(val);
            else if (strcmp(key, "tapeEnabled") == 0) d->sfEffects.tapeEnabled = _sf_parseBool(val);
            else if (strcmp(key, "tapeWow") == 0) d->sfEffects.tapeWow = _sf_parseFloat(val);
            else if (strcmp(key, "tapeFlutter") == 0) d->sfEffects.tapeFlutter = _sf_parseFloat(val);
            else if (strcmp(key, "tapeSaturation") == 0) d->sfEffects.tapeSaturation = _sf_parseFloat(val);
            else if (strcmp(key, "tapeHiss") == 0) d->sfEffects.tapeHiss = _sf_parseFloat(val);
            else if (strcmp(key, "crushEnabled") == 0) d->sfEffects.crushEnabled = _sf_parseBool(val);
            else if (strcmp(key, "crushBits") == 0) d->sfEffects.crushBits = _sf_parseFloat(val);
            else if (strcmp(key, "crushRate") == 0) d->sfEffects.crushRate = _sf_parseFloat(val);
            else if (strcmp(key, "crushMix") == 0) d->sfEffects.crushMix = _sf_parseFloat(val);
            else if (strcmp(key, "chorusEnabled") == 0) d->sfEffects.chorusEnabled = _sf_parseBool(val);
            else if (strcmp(key, "chorusBBD") == 0) d->sfEffects.chorusBBD = _sf_parseBool(val);
            else if (strcmp(key, "chorusRate") == 0) d->sfEffects.chorusRate = _sf_parseFloat(val);
            else if (strcmp(key, "chorusDepth") == 0) d->sfEffects.chorusDepth = _sf_parseFloat(val);
            else if (strcmp(key, "chorusMix") == 0) d->sfEffects.chorusMix = _sf_parseFloat(val);
            else if (strcmp(key, "chorusDelay") == 0) d->sfEffects.chorusDelay = _sf_parseFloat(val);
            else if (strcmp(key, "chorusFeedback") == 0) d->sfEffects.chorusFeedback = _sf_parseFloat(val);
            else if (strcmp(key, "flangerEnabled") == 0) d->sfEffects.flangerEnabled = _sf_parseBool(val);
            else if (strcmp(key, "flangerRate") == 0) d->sfEffects.flangerRate = _sf_parseFloat(val);
            else if (strcmp(key, "flangerDepth") == 0) d->sfEffects.flangerDepth = _sf_parseFloat(val);
            else if (strcmp(key, "flangerFeedback") == 0) d->sfEffects.flangerFeedback = _sf_parseFloat(val);
            else if (strcmp(key, "flangerMix") == 0) d->sfEffects.flangerMix = _sf_parseFloat(val);
            else if (strcmp(key, "phaserEnabled") == 0) d->sfEffects.phaserEnabled = _sf_parseBool(val);
            else if (strcmp(key, "phaserRate") == 0) d->sfEffects.phaserRate = _sf_parseFloat(val);
            else if (strcmp(key, "phaserDepth") == 0) d->sfEffects.phaserDepth = _sf_parseFloat(val);
            else if (strcmp(key, "phaserMix") == 0) d->sfEffects.phaserMix = _sf_parseFloat(val);
            else if (strcmp(key, "phaserFeedback") == 0) d->sfEffects.phaserFeedback = _sf_parseFloat(val);
            else if (strcmp(key, "phaserStages") == 0) d->sfEffects.phaserStages = _sf_parseInt(val);
            else if (strcmp(key, "combEnabled") == 0) d->sfEffects.combEnabled = _sf_parseBool(val);
            else if (strcmp(key, "combFreq") == 0) d->sfEffects.combFreq = _sf_parseFloat(val);
            else if (strcmp(key, "combFeedback") == 0) d->sfEffects.combFeedback = _sf_parseFloat(val);
            else if (strcmp(key, "combMix") == 0) d->sfEffects.combMix = _sf_parseFloat(val);
            else if (strcmp(key, "combDamping") == 0) d->sfEffects.combDamping = _sf_parseFloat(val);
            else if (strcmp(key, "octaverEnabled") == 0) d->sfEffects.octaverEnabled = _sf_parseBool(val);
            else if (strcmp(key, "octaverMix") == 0) d->sfEffects.octaverMix = _sf_parseFloat(val);
            else if (strcmp(key, "octaverSubLevel") == 0) d->sfEffects.octaverSubLevel = _sf_parseFloat(val);
            else if (strcmp(key, "octaverTone") == 0) d->sfEffects.octaverTone = _sf_parseFloat(val);
            else if (strcmp(key, "tremoloEnabled") == 0) d->sfEffects.tremoloEnabled = _sf_parseBool(val);
            else if (strcmp(key, "tremoloRate") == 0) d->sfEffects.tremoloRate = _sf_parseFloat(val);
            else if (strcmp(key, "tremoloDepth") == 0) d->sfEffects.tremoloDepth = _sf_parseFloat(val);
            else if (strcmp(key, "tremoloShape") == 0) d->sfEffects.tremoloShape = _sf_parseInt(val);
            else if (strcmp(key, "leslieEnabled") == 0) d->sfEffects.leslieEnabled = _sf_parseBool(val);
            else if (strcmp(key, "leslieSpeed") == 0) d->sfEffects.leslieSpeed = _sf_parseInt(val);
            else if (strcmp(key, "leslieDrive") == 0) d->sfEffects.leslieDrive = _sf_parseFloat(val);
            else if (strcmp(key, "leslieBalance") == 0) d->sfEffects.leslieBalance = _sf_parseFloat(val);
            else if (strcmp(key, "leslieDoppler") == 0) d->sfEffects.leslieDoppler = _sf_parseFloat(val);
            else if (strcmp(key, "leslieMix") == 0) d->sfEffects.leslieMix = _sf_parseFloat(val);
            else if (strcmp(key, "wahEnabled") == 0) d->sfEffects.wahEnabled = _sf_parseBool(val);
            else if (strcmp(key, "wahMode") == 0) d->sfEffects.wahMode = _sf_parseInt(val);
            else if (strcmp(key, "wahRate") == 0) d->sfEffects.wahRate = _sf_parseFloat(val);
            else if (strcmp(key, "wahSensitivity") == 0) d->sfEffects.wahSensitivity = _sf_parseFloat(val);
            else if (strcmp(key, "wahFreqLow") == 0) d->sfEffects.wahFreqLow = _sf_parseFloat(val);
            else if (strcmp(key, "wahFreqHigh") == 0) d->sfEffects.wahFreqHigh = _sf_parseFloat(val);
            else if (strcmp(key, "wahResonance") == 0) d->sfEffects.wahResonance = _sf_parseFloat(val);
            else if (strcmp(key, "wahMix") == 0) d->sfEffects.wahMix = _sf_parseFloat(val);
            else if (strcmp(key, "ringModEnabled") == 0) d->sfEffects.ringModEnabled = _sf_parseBool(val);
            else if (strcmp(key, "ringModFreq") == 0) d->sfEffects.ringModFreq = _sf_parseFloat(val);
            else if (strcmp(key, "ringModMix") == 0) d->sfEffects.ringModMix = _sf_parseFloat(val);
            else if (strcmp(key, "reverbEnabled") == 0) d->sfEffects.reverbEnabled = _sf_parseBool(val);
            else if (strcmp(key, "reverbFDN") == 0) d->sfEffects.reverbFDN = _sf_parseBool(val);
            else if (strcmp(key, "reverbSize") == 0) d->sfEffects.reverbSize = _sf_parseFloat(val);
            else if (strcmp(key, "reverbDamping") == 0) d->sfEffects.reverbDamping = _sf_parseFloat(val);
            else if (strcmp(key, "reverbMix") == 0) d->sfEffects.reverbMix = _sf_parseFloat(val);
            else if (strcmp(key, "reverbPreDelay") == 0) d->sfEffects.reverbPreDelay = _sf_parseFloat(val);
            else if (strcmp(key, "reverbBass") == 0) d->sfEffects.reverbBass = _sf_parseFloat(val);
            else if (strcmp(key, "sidechainEnabled") == 0) d->sfEffects.sidechainEnabled = _sf_parseBool(val);
            else if (strcmp(key, "sidechainSource") == 0) d->sfEffects.sidechainSource = _sf_parseInt(val);
            else if (strcmp(key, "sidechainTarget") == 0) d->sfEffects.sidechainTarget = _sf_parseInt(val);
            else if (strcmp(key, "sidechainDepth") == 0) d->sfEffects.sidechainDepth = _sf_parseFloat(val);
            else if (strcmp(key, "sidechainAttack") == 0) d->sfEffects.sidechainAttack = _sf_parseFloat(val);
            else if (strcmp(key, "sidechainRelease") == 0) d->sfEffects.sidechainRelease = _sf_parseFloat(val);
            else if (strcmp(key, "sidechainHPFreq") == 0) d->sfEffects.sidechainHPFreq = _sf_parseFloat(val);
            else if (strcmp(key, "scEnvEnabled") == 0) d->sfEffects.scEnvEnabled = _sf_parseBool(val);
            else if (strcmp(key, "scEnvSource") == 0) d->sfEffects.scEnvSource = _sf_parseInt(val);
            else if (strcmp(key, "scEnvTarget") == 0) d->sfEffects.scEnvTarget = _sf_parseInt(val);
            else if (strcmp(key, "scEnvDepth") == 0) d->sfEffects.scEnvDepth = _sf_parseFloat(val);
            else if (strcmp(key, "scEnvAttack") == 0) d->sfEffects.scEnvAttack = _sf_parseFloat(val);
            else if (strcmp(key, "scEnvHold") == 0) d->sfEffects.scEnvHold = _sf_parseFloat(val);
            else if (strcmp(key, "scEnvRelease") == 0) d->sfEffects.scEnvRelease = _sf_parseFloat(val);
            else if (strcmp(key, "scEnvCurve") == 0) d->sfEffects.scEnvCurve = _sf_parseInt(val);
            else if (strcmp(key, "scEnvHPFreq") == 0) d->sfEffects.scEnvHPFreq = _sf_parseFloat(val);
            else if (strcmp(key, "subBassBoost") == 0) d->sfEffects.subBassBoost = _sf_parseBool(val);
            else if (strcmp(key, "mbEnabled") == 0) d->sfEffects.mbEnabled = _sf_parseBool(val);
            else if (strcmp(key, "mbLowCrossover") == 0) d->sfEffects.mbLowCrossover = _sf_parseFloat(val);
            else if (strcmp(key, "mbHighCrossover") == 0) d->sfEffects.mbHighCrossover = _sf_parseFloat(val);
            else if (strcmp(key, "mbLowGain") == 0) d->sfEffects.mbLowGain = _sf_parseFloat(val);
            else if (strcmp(key, "mbMidGain") == 0) d->sfEffects.mbMidGain = _sf_parseFloat(val);
            else if (strcmp(key, "mbHighGain") == 0) d->sfEffects.mbHighGain = _sf_parseFloat(val);
            else if (strcmp(key, "mbLowDrive") == 0) d->sfEffects.mbLowDrive = _sf_parseFloat(val);
            else if (strcmp(key, "mbMidDrive") == 0) d->sfEffects.mbMidDrive = _sf_parseFloat(val);
            else if (strcmp(key, "mbHighDrive") == 0) d->sfEffects.mbHighDrive = _sf_parseFloat(val);
            else if (strcmp(key, "vinylEnabled") == 0) d->sfEffects.vinylEnabled = _sf_parseBool(val);
            else if (strcmp(key, "vinylCrackle") == 0) d->sfEffects.vinylCrackle = _sf_parseFloat(val);
            else if (strcmp(key, "vinylNoise") == 0) d->sfEffects.vinylNoise = _sf_parseFloat(val);
            else if (strcmp(key, "vinylWarp") == 0) d->sfEffects.vinylWarp = _sf_parseFloat(val);
            else if (strcmp(key, "vinylWarpRate") == 0) d->sfEffects.vinylWarpRate = _sf_parseFloat(val);
            else if (strcmp(key, "vinylToneLP") == 0) d->sfEffects.vinylToneLP = _sf_parseFloat(val);
            break;

        case _SF_SEC_DUB:
            if (strcmp(key, "enabled") == 0) d->sfDubLoop.enabled = _sf_parseBool(val);
            else if (strcmp(key, "feedback") == 0) d->sfDubLoop.feedback = _sf_parseFloat(val);
            else if (strcmp(key, "mix") == 0) d->sfDubLoop.mix = _sf_parseFloat(val);
            else if (strcmp(key, "inputSource") == 0) d->sfDubLoop.inputSource = _sf_parseInt(val);
            else if (strcmp(key, "preReverb") == 0) d->sfDubLoop.preReverb = _sf_parseBool(val);
            else if (strcmp(key, "speed") == 0) d->sfDubLoop.speed = _sf_parseFloat(val);
            else if (strcmp(key, "saturation") == 0) d->sfDubLoop.saturation = _sf_parseFloat(val);
            else if (strcmp(key, "toneHigh") == 0) d->sfDubLoop.toneHigh = _sf_parseFloat(val);
            else if (strcmp(key, "toneLow") == 0) d->sfDubLoop.toneLow = _sf_parseFloat(val);
            else if (strcmp(key, "noise") == 0) d->sfDubLoop.noise = _sf_parseFloat(val);
            else if (strcmp(key, "degradeRate") == 0) d->sfDubLoop.degradeRate = _sf_parseFloat(val);
            else if (strcmp(key, "wow") == 0) d->sfDubLoop.wow = _sf_parseFloat(val);
            else if (strcmp(key, "flutter") == 0) d->sfDubLoop.flutter = _sf_parseFloat(val);
            else if (strcmp(key, "drift") == 0) d->sfDubLoop.drift = _sf_parseFloat(val);
            else if (strcmp(key, "driftRate") == 0) d->sfDubLoop.driftRate = _sf_parseFloat(val);
            else if (strcmp(key, "numHeads") == 0) d->sfDubLoop.numHeads = _sf_parseInt(val);
            else if (strncmp(key, "headTime", 8) == 0) {
                int h = key[8] - '0';
                if (h >= 0 && h < DUB_LOOP_MAX_HEADS) d->sfDubLoop.headTime[h] = _sf_parseFloat(val);
            }
            else if (strncmp(key, "headLevel", 9) == 0) {
                int h = key[9] - '0';
                if (h >= 0 && h < DUB_LOOP_MAX_HEADS) d->sfDubLoop.headLevel[h] = _sf_parseFloat(val);
            }
            break;

        case _SF_SEC_BUS:
            if (subIndex >= 0 && subIndex < NUM_BUSES) {
                BusEffects *bus = &d->sfBusEffects[subIndex];
                if (strcmp(key, "volume") == 0) bus->volume = _sf_parseFloat(val);
                else if (strcmp(key, "pan") == 0) bus->pan = _sf_parseFloat(val);
                else if (strcmp(key, "mute") == 0) bus->mute = _sf_parseBool(val);
                else if (strcmp(key, "solo") == 0) bus->solo = _sf_parseBool(val);
                else if (strcmp(key, "filterEnabled") == 0) bus->filterEnabled = _sf_parseBool(val);
                else if (strcmp(key, "filterCutoff") == 0) bus->filterCutoff = _sf_parseFloat(val);
                else if (strcmp(key, "filterResonance") == 0) bus->filterResonance = _sf_parseFloat(val);
                else if (strcmp(key, "filterType") == 0) bus->filterType = _sf_parseInt(val);
                else if (strcmp(key, "distEnabled") == 0) bus->distEnabled = _sf_parseBool(val);
                else if (strcmp(key, "distDrive") == 0) bus->distDrive = _sf_parseFloat(val);
                else if (strcmp(key, "distMix") == 0) bus->distMix = _sf_parseFloat(val);
                else if (strcmp(key, "delayEnabled") == 0) bus->delayEnabled = _sf_parseBool(val);
                else if (strcmp(key, "delayTime") == 0) bus->delayTime = _sf_parseFloat(val);
                else if (strcmp(key, "delayFeedback") == 0) bus->delayFeedback = _sf_parseFloat(val);
                else if (strcmp(key, "delayMix") == 0) bus->delayMix = _sf_parseFloat(val);
                else if (strcmp(key, "delayTempoSync") == 0) bus->delayTempoSync = _sf_parseBool(val);
                else if (strcmp(key, "delaySyncDiv") == 0) bus->delaySyncDiv = _sf_parseInt(val);
                else if (strcmp(key, "eqEnabled") == 0) bus->eqEnabled = _sf_parseBool(val);
                else if (strcmp(key, "eqLowGain") == 0) bus->eqLowGain = _sf_parseFloat(val);
                else if (strcmp(key, "eqHighGain") == 0) bus->eqHighGain = _sf_parseFloat(val);
                else if (strcmp(key, "eqLowFreq") == 0) bus->eqLowFreq = _sf_parseFloat(val);
                else if (strcmp(key, "eqHighFreq") == 0) bus->eqHighFreq = _sf_parseFloat(val);
                else if (strcmp(key, "chorusEnabled") == 0) bus->chorusEnabled = _sf_parseBool(val);
                else if (strcmp(key, "chorusBBD") == 0) bus->chorusBBD = _sf_parseBool(val);
                else if (strcmp(key, "chorusRate") == 0) bus->chorusRate = _sf_parseFloat(val);
                else if (strcmp(key, "chorusDepth") == 0) bus->chorusDepth = _sf_parseFloat(val);
                else if (strcmp(key, "chorusMix") == 0) bus->chorusMix = _sf_parseFloat(val);
                else if (strcmp(key, "chorusDelay") == 0) bus->chorusDelay = _sf_parseFloat(val);
                else if (strcmp(key, "chorusFeedback") == 0) bus->chorusFeedback = _sf_parseFloat(val);
                else if (strcmp(key, "phaserEnabled") == 0) bus->phaserEnabled = _sf_parseBool(val);
                else if (strcmp(key, "phaserRate") == 0) bus->phaserRate = _sf_parseFloat(val);
                else if (strcmp(key, "phaserDepth") == 0) bus->phaserDepth = _sf_parseFloat(val);
                else if (strcmp(key, "phaserMix") == 0) bus->phaserMix = _sf_parseFloat(val);
                else if (strcmp(key, "phaserFeedback") == 0) bus->phaserFeedback = _sf_parseFloat(val);
                else if (strcmp(key, "phaserStages") == 0) bus->phaserStages = _sf_parseInt(val);
                else if (strcmp(key, "combEnabled") == 0) bus->combEnabled = _sf_parseBool(val);
                else if (strcmp(key, "combFreq") == 0) bus->combFreq = _sf_parseFloat(val);
                else if (strcmp(key, "combFeedback") == 0) bus->combFeedback = _sf_parseFloat(val);
                else if (strcmp(key, "combMix") == 0) bus->combMix = _sf_parseFloat(val);
                else if (strcmp(key, "combDamping") == 0) bus->combDamping = _sf_parseFloat(val);
                else if (strcmp(key, "octaverEnabled") == 0) bus->octaverEnabled = _sf_parseBool(val);
                else if (strcmp(key, "octaverMix") == 0) bus->octaverMix = _sf_parseFloat(val);
                else if (strcmp(key, "octaverSubLevel") == 0) bus->octaverSubLevel = _sf_parseFloat(val);
                else if (strcmp(key, "octaverTone") == 0) bus->octaverTone = _sf_parseFloat(val);
                else if (strcmp(key, "tremoloEnabled") == 0) bus->tremoloEnabled = _sf_parseBool(val);
                else if (strcmp(key, "tremoloRate") == 0) bus->tremoloRate = _sf_parseFloat(val);
                else if (strcmp(key, "tremoloDepth") == 0) bus->tremoloDepth = _sf_parseFloat(val);
                else if (strcmp(key, "tremoloShape") == 0) bus->tremoloShape = _sf_parseInt(val);
                else if (strcmp(key, "leslieEnabled") == 0) bus->leslieEnabled = _sf_parseBool(val);
                else if (strcmp(key, "leslieSpeed") == 0) bus->leslieSpeed = _sf_parseInt(val);
                else if (strcmp(key, "leslieDrive") == 0) bus->leslieDrive = _sf_parseFloat(val);
                else if (strcmp(key, "leslieBalance") == 0) bus->leslieBalance = _sf_parseFloat(val);
                else if (strcmp(key, "leslieDoppler") == 0) bus->leslieDoppler = _sf_parseFloat(val);
                else if (strcmp(key, "leslieMix") == 0) bus->leslieMix = _sf_parseFloat(val);
                else if (strcmp(key, "wahEnabled") == 0) bus->wahEnabled = _sf_parseBool(val);
                else if (strcmp(key, "wahMode") == 0) bus->wahMode = _sf_parseInt(val);
                else if (strcmp(key, "wahRate") == 0) bus->wahRate = _sf_parseFloat(val);
                else if (strcmp(key, "wahSensitivity") == 0) bus->wahSensitivity = _sf_parseFloat(val);
                else if (strcmp(key, "wahFreqLow") == 0) bus->wahFreqLow = _sf_parseFloat(val);
                else if (strcmp(key, "wahFreqHigh") == 0) bus->wahFreqHigh = _sf_parseFloat(val);
                else if (strcmp(key, "wahResonance") == 0) bus->wahResonance = _sf_parseFloat(val);
                else if (strcmp(key, "wahMix") == 0) bus->wahMix = _sf_parseFloat(val);
                else if (strcmp(key, "ringModEnabled") == 0) bus->ringModEnabled = _sf_parseBool(val);
                else if (strcmp(key, "ringModFreq") == 0) bus->ringModFreq = _sf_parseFloat(val);
                else if (strcmp(key, "ringModMix") == 0) bus->ringModMix = _sf_parseFloat(val);
                else if (strcmp(key, "reverbSend") == 0) bus->reverbSend = _sf_parseFloat(val);
            }
            break;

        case _SF_SEC_METADATA:
            if (strcmp(key, "author") == 0) strncpy(d->author, val, SONG_FILE_MAX_NAME - 1);
            else if (strcmp(key, "description") == 0) strncpy(d->description, val, SONG_FILE_MAX_NAME - 1);
            else if (strcmp(key, "mood") == 0) strncpy(d->mood, val, SONG_FILE_MAX_NAME - 1);
            else if (strcmp(key, "energy") == 0) strncpy(d->energy, val, 15);
            else if (strcmp(key, "context") == 0) strncpy(d->context, val, SONG_FILE_MAX_NAME - 1);
            break;

        case _SF_SEC_TRANSITIONS:
            if (strcmp(key, "fadeIn") == 0) d->fadeIn = _sf_parseFloat(val);
            else if (strcmp(key, "fadeOut") == 0) d->fadeOut = _sf_parseFloat(val);
            else if (strcmp(key, "crossfade") == 0) d->crossfade = _sf_parseBool(val);
            break;

        case _SF_SEC_CHAIN:
            if (strcmp(key, "order") == 0) {
                // Parse comma-separated pattern indices: "0, 1, 1, 2, 3"
                d->chainLength = 0;
                char *tok = val;
                while (*tok && d->chainLength < SEQ_MAX_CHAIN) {
                    while (*tok == ' ' || *tok == ',') tok++;
                    if (!*tok) break;
                    d->chain[d->chainLength++] = atoi(tok);
                    while (*tok && *tok != ',') tok++;
                }
            }
            break;

        case _SF_SEC_PATTERN:
            if (subIndex >= 0 && subIndex < SEQ_NUM_PATTERNS) {
                Pattern *p = &d->patterns[subIndex];
                if (strcmp(key, "drumTrackLength") == 0) {
                    _sf_parseIntList(val, p->trackLength, SEQ_DRUM_TRACKS);
                }
                else if (strcmp(key, "melodyTrackLength") == 0) {
                    int melLengths[SEQ_MELODY_TRACKS];
                    _sf_parseIntList(val, melLengths, SEQ_MELODY_TRACKS);
                    for (int _t = 0; _t < SEQ_MELODY_TRACKS; _t++)
                        p->trackLength[SEQ_DRUM_TRACKS + _t] = melLengths[_t];
                }
            }
            break;

        default:
            break; // Unknown section — skip
        }
    }

    fclose(f);

    // Backward compat: old files without trackSwing keys — propagate global swing
    {
        bool allZero = true;
        for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
            if (d->trackSwing[i] != 0) { allZero = false; break; }
        }
        if (allZero && d->dilla.swing != 0) {
            for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++)
                d->trackSwing[i] = d->dilla.swing;
        }
    }

    return true;
}

// Load a single .patch file into a SynthPatch
static bool songFileLoadPatch(const char *filepath, SynthPatch *patch) {
    SongFileData d;
    songFileDataInit(&d);
    // Patch loading reuses the song parser — [patch] section maps to instruments[0]
    if (!songFileLoad(filepath, &d)) return false;
    *patch = d.instruments[0];
    return true;
}

#endif // SONG_FILE_H
