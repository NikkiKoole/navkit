// midi_file.h — Standard MIDI File (.mid) parser + DAW importer
//
// Header-only. Parses SMF format 0 and 1. Populates DAW state (patches,
// patterns, arrangement) from MIDI note/program/tempo events.
//
// Usage:
//   #include "midi_file.h"
//   bool ok = midiFileToDaw(filepath);  // populates daw.* and seq.*
//
// Requires: sequencer.h, synth_patch.h, instrument_presets.h loaded.
// Also requires gm_preset_map.h for GM program → preset mapping.

#ifndef MIDI_FILE_H
#define MIDI_FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "../tools/gm_preset_map.h"

// ---------------------------------------------------------------------------
// SMF binary parser
// ---------------------------------------------------------------------------

// Read big-endian uint16
static uint16_t _mf_read16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

// Read big-endian uint32
static uint32_t _mf_read32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// Read variable-length quantity, advance *pos
static uint32_t _mf_readVLQ(const uint8_t *data, int *pos, int len) {
    uint32_t val = 0;
    for (int i = 0; i < 4 && *pos < len; i++) {
        uint8_t b = data[(*pos)++];
        val = (val << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
    }
    return val;
}

// A parsed MIDI note event
typedef struct {
    int channel;
    int note;           // MIDI note 0-127
    int velocity;       // 0-127
    int program;        // GM program number at time of note
    uint32_t startTick;
    uint32_t durationTick;
} MfNoteEvent;

// Parsed MIDI file
typedef struct {
    float bpm;
    int ticksPerBeat;
    int timeSigNum;     // time signature numerator (default 4)
    int timeSigDenom;   // time signature denominator (default 4)
    MfNoteEvent *notes;
    int noteCount;
    int noteCapacity;
    int channelProgram[16];  // last program per channel
} MfParsedMidi;

static void _mf_addNote(MfParsedMidi *m, MfNoteEvent ev) {
    if (m->noteCount >= m->noteCapacity) {
        m->noteCapacity = m->noteCapacity ? m->noteCapacity * 2 : 1024;
        m->notes = (MfNoteEvent *)realloc(m->notes, (size_t)m->noteCapacity * sizeof(MfNoteEvent));
    }
    m->notes[m->noteCount++] = ev;
}

static bool _mf_parse(const uint8_t *data, int dataLen, MfParsedMidi *out) {
    memset(out, 0, sizeof(*out));
    out->bpm = 120.0f;
    out->timeSigNum = 4;
    out->timeSigDenom = 4;
    out->noteCapacity = 0;
    out->notes = NULL;
    out->noteCount = 0;

    if (dataLen < 14) return false;
    // MThd header
    if (memcmp(data, "MThd", 4) != 0) return false;
    uint32_t headerLen = _mf_read32(data + 4);
    (void)headerLen;
    int format = _mf_read16(data + 8);
    int numTracks = _mf_read16(data + 10);
    int division = _mf_read16(data + 12);
    (void)format;

    if (division & 0x8000) {
        // SMPTE time — not supported, use default tpb
        out->ticksPerBeat = 120;
    } else {
        out->ticksPerBeat = division;
    }

    int pos = 8 + (int)headerLen;

    // Per-channel active notes: (channel, note) → (startTick, velocity, program)
    // Simple flat array: 16 channels × 128 notes
    typedef struct { uint32_t start; int velocity; int program; bool active; } ActiveNote;
    ActiveNote *active = (ActiveNote *)calloc(16 * 128, sizeof(ActiveNote));

    for (int t = 0; t < numTracks && pos + 8 <= dataLen; t++) {
        if (memcmp(data + pos, "MTrk", 4) != 0) { free(active); return false; }
        uint32_t trackLen = _mf_read32(data + pos + 4);
        int trkStart = pos + 8;
        int trkEnd = trkStart + (int)trackLen;
        if (trkEnd > dataLen) trkEnd = dataLen;
        int tp = trkStart;
        uint32_t absTick = 0;
        uint8_t runningStatus = 0;
        int chProg[16] = {0};

        while (tp < trkEnd) {
            uint32_t delta = _mf_readVLQ(data, &tp, trkEnd);
            absTick += delta;

            if (tp >= trkEnd) break;
            uint8_t status = data[tp];

            // Meta event
            if (status == 0xFF) {
                tp++;
                if (tp >= trkEnd) break;
                uint8_t metaType = data[tp++];
                uint32_t metaLen = _mf_readVLQ(data, &tp, trkEnd);
                if (metaType == 0x51 && metaLen == 3 && tp + 3 <= trkEnd) {
                    // Set Tempo
                    uint32_t uspqn = ((uint32_t)data[tp] << 16) | ((uint32_t)data[tp+1] << 8) | data[tp+2];
                    if (uspqn > 0) out->bpm = 60000000.0f / (float)uspqn;
                }
                if (metaType == 0x58 && metaLen >= 2 && tp + 2 <= trkEnd) {
                    // Time Signature: nn dd (dd is log2 of denominator)
                    out->timeSigNum = data[tp];
                    out->timeSigDenom = 1 << data[tp + 1];
                }
                tp += (int)metaLen;
                continue;
            }

            // SysEx
            if (status == 0xF0 || status == 0xF7) {
                tp++;
                uint32_t sysLen = _mf_readVLQ(data, &tp, trkEnd);
                tp += (int)sysLen;
                continue;
            }

            // Channel message
            uint8_t cmd, chan, d1, d2;
            if (status & 0x80) {
                cmd = status & 0xF0;
                chan = status & 0x0F;
                runningStatus = status;
                tp++;
            } else {
                // Running status
                cmd = runningStatus & 0xF0;
                chan = runningStatus & 0x0F;
            }

            switch (cmd) {
                case 0x90: // Note On
                    if (tp + 1 >= trkEnd) { tp = trkEnd; break; }
                    d1 = data[tp++]; d2 = data[tp++];
                    if (d2 > 0) {
                        int idx = chan * 128 + d1;
                        active[idx] = (ActiveNote){absTick, d2, chProg[chan], true};
                    } else {
                        // velocity 0 = note off
                        int idx = chan * 128 + d1;
                        if (active[idx].active) {
                            uint32_t dur = absTick - active[idx].start;
                            _mf_addNote(out, (MfNoteEvent){
                                .channel = chan, .note = d1, .velocity = active[idx].velocity,
                                .program = active[idx].program,
                                .startTick = active[idx].start, .durationTick = dur > 0 ? dur : 1
                            });
                            active[idx].active = false;
                        }
                    }
                    break;
                case 0x80: // Note Off
                    if (tp + 1 >= trkEnd) { tp = trkEnd; break; }
                    d1 = data[tp++]; d2 = data[tp++];
                    {
                        int idx = chan * 128 + d1;
                        if (active[idx].active) {
                            uint32_t dur = absTick - active[idx].start;
                            _mf_addNote(out, (MfNoteEvent){
                                .channel = chan, .note = d1, .velocity = active[idx].velocity,
                                .program = active[idx].program,
                                .startTick = active[idx].start, .durationTick = dur > 0 ? dur : 1
                            });
                            active[idx].active = false;
                        }
                    }
                    break;
                case 0xC0: // Program Change (1 data byte)
                    if (tp >= trkEnd) break;
                    d1 = data[tp++];
                    chProg[chan] = d1;
                    out->channelProgram[chan] = d1;
                    break;
                case 0xD0: // Channel Pressure (1 data byte)
                    if (tp >= trkEnd) break;
                    tp++;
                    break;
                default: // 2 data bytes: A0, B0, E0
                    if (tp + 1 >= trkEnd) { tp = trkEnd; break; }
                    tp += 2;
                    break;
            }
        }
        pos = trkEnd;
    }

    free(active);
    return out->noteCount > 0;
}

static void _mf_free(MfParsedMidi *m) {
    free(m->notes);
    m->notes = NULL;
    m->noteCount = 0;
}

// ---------------------------------------------------------------------------
// MIDI → DAW state
// ---------------------------------------------------------------------------

// Drum note → track mapping (same as Python version)
static int _mf_drumTrack(int note) {
    switch (note) {
        case 35: case 36: case 41: case 43: case 45: case 47: case 48: case 50:
            return 0; // kick/toms
        case 37: case 38: case 40:
            return 1; // snare/side stick
        case 42: case 44: case 46: case 51: case 53:
            return 2; // hihats + ride cymbal/bell
        default:
            return 3; // perc/cymbals/etc
    }
}

// Compare notes by start tick for qsort
static int _mf_noteCompare(const void *a, const void *b) {
    const MfNoteEvent *na = (const MfNoteEvent *)a;
    const MfNoteEvent *nb = (const MfNoteEvent *)b;
    if (na->startTick != nb->startTick) return (na->startTick < nb->startTick) ? -1 : 1;
    if (na->channel != nb->channel) return na->channel - nb->channel;
    return na->note - nb->note;
}

// Load a MIDI file into DAW state. Returns true on success.
// Assumes daw.*, seq.*, and instrumentPresets[] are available as globals.
static bool midiFileToDaw(const char *filepath) {
    // Read file into memory
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fileSize <= 0 || fileSize > 10 * 1024 * 1024) { fclose(f); return false; } // 10MB limit
    uint8_t *data = (uint8_t *)malloc((size_t)fileSize);
    if (!data) { fclose(f); return false; }
    size_t readBytes = fread(data, 1, (size_t)fileSize, f);
    fclose(f);
    if ((long)readBytes != fileSize) { free(data); return false; }

    // Parse MIDI
    MfParsedMidi midi;
    if (!_mf_parse(data, (int)fileSize, &midi)) { free(data); return false; }
    free(data);

    // Sort notes by time
    qsort(midi.notes, (size_t)midi.noteCount, sizeof(MfNoteEvent), _mf_noteCompare);

    // --- Setup DAW state ---
    // Compute steps per bar from time signature: numerator * (16 / denominator)
    int timeSigSteps = midi.timeSigNum * (16 / midi.timeSigDenom);
    if (timeSigSteps < 4) timeSigSteps = 4;
    if (timeSigSteps > SEQ_MAX_STEPS) timeSigSteps = SEQ_MAX_STEPS;
    int stepsPerPattern = timeSigSteps;
    daw.stepCount = stepsPerPattern;
    float ticksPerStep = (float)midi.ticksPerBeat / 4.0f; // 16th notes
    float ticksPerPattern = ticksPerStep * stepsPerPattern;

    // Reset patterns
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) initPattern(&seq.patterns[i]);

    // Set tempo
    daw.transport.bpm = midi.bpm;

    // Figure out how many patterns (cap at 8)
    uint32_t maxTick = 0;
    for (int i = 0; i < midi.noteCount; i++) {
        uint32_t end = midi.notes[i].startTick + midi.notes[i].durationTick;
        if (end > maxTick) maxTick = end;
    }
    int numPatterns = (int)(maxTick / ticksPerPattern) + 1;
    if (numPatterns > SEQ_NUM_PATTERNS) numPatterns = SEQ_NUM_PATTERNS;

    // --- Channel → melody track assignment ---
    // Collect melodic channels (not ch9) with average pitch + resolved preset
    typedef struct { int channel; float avgPitch; int noteCount; int program; int preset; } ChInfo;
    ChInfo chInfo[16];
    int chCount = 0;
    {
        long chSum[16] = {0};
        int chN[16] = {0};
        int chProg[16] = {0};
        for (int i = 0; i < midi.noteCount; i++) {
            MfNoteEvent *e = &midi.notes[i];
            if (e->channel == 9) continue;
            chSum[e->channel] += e->note;
            chN[e->channel]++;
            chProg[e->channel] = e->program;
        }
        for (int c = 0; c < 16; c++) {
            if (chN[c] > 0 && c != 9) {
                int p = gmProgramPreset(chProg[c]);
                chInfo[chCount++] = (ChInfo){c, (float)chSum[c] / chN[c], chN[c], chProg[c], p >= 0 ? p : 1};
            }
        }
    }

    // Group channels by resolved preset, compute combined avg pitch per group
    typedef struct { int preset; float avgPitch; int channels[16]; int chCount; int program; } PresetGroup;
    PresetGroup groups[16];
    int groupCount = 0;
    for (int i = 0; i < chCount; i++) {
        // Find existing group with same preset
        int g = -1;
        for (int j = 0; j < groupCount; j++) {
            if (groups[j].preset == chInfo[i].preset) { g = j; break; }
        }
        if (g < 0) {
            g = groupCount++;
            groups[g] = (PresetGroup){chInfo[i].preset, 0, {0}, 0, chInfo[i].program};
        }
        groups[g].channels[groups[g].chCount++] = chInfo[i].channel;
        // Weighted average pitch across all channels in group
        float totalPitch = 0; int totalNotes = 0;
        for (int k = 0; k < groups[g].chCount; k++) {
            for (int ci = 0; ci < chCount; ci++) {
                if (chInfo[ci].channel == groups[g].channels[k]) {
                    totalPitch += chInfo[ci].avgPitch * chInfo[ci].noteCount;
                    totalNotes += chInfo[ci].noteCount;
                }
            }
        }
        groups[g].avgPitch = totalNotes > 0 ? totalPitch / totalNotes : 60.0f;
    }

    // Sort groups by average pitch (lowest first)
    for (int i = 0; i < groupCount - 1; i++)
        for (int j = i + 1; j < groupCount; j++)
            if (groups[i].avgPitch > groups[j].avgPitch) {
                PresetGroup tmp = groups[i]; groups[i] = groups[j]; groups[j] = tmp;
            }

    // Assign groups to tracks: lowest→bass, highest→lead, rest→chord
    int chToTrack[16]; // channel → melody track (0=bass, 1=lead, 2=chord), -1=unmapped
    memset(chToTrack, -1, sizeof(chToTrack));
    if (groupCount == 1) {
        for (int k = 0; k < groups[0].chCount; k++) chToTrack[groups[0].channels[k]] = 0;
    } else if (groupCount == 2) {
        for (int k = 0; k < groups[0].chCount; k++) chToTrack[groups[0].channels[k]] = 0;
        for (int k = 0; k < groups[1].chCount; k++) chToTrack[groups[1].channels[k]] = 1;
    } else if (groupCount >= 3) {
        for (int k = 0; k < groups[0].chCount; k++) chToTrack[groups[0].channels[k]] = 0;
        for (int k = 0; k < groups[groupCount-1].chCount; k++) chToTrack[groups[groupCount-1].channels[k]] = 1;
        for (int i = 1; i < groupCount - 1; i++)
            for (int k = 0; k < groups[i].chCount; k++) chToTrack[groups[i].channels[k]] = 2;
    }

    // --- Assign presets (drums + melody) ---
    // Drums: default 808 kit
    int drumPresets[4] = {24, 25, 27, 26}; // kick, snare, CH, clap
    for (int i = 0; i < 4; i++) {
        daw.patches[i] = instrumentPresets[drumPresets[i]].patch;
        snprintf(daw.patches[i].p_name, 32, "%s", instrumentPresets[drumPresets[i]].name);
    }

    // Melody: use preset from each group
    int melodyDefaults[3] = {1, 1, 1}; // Fat Bass fallback
    if (groupCount >= 1) melodyDefaults[0] = groups[0].preset;
    if (groupCount >= 2) melodyDefaults[1] = groups[groupCount-1].preset;
    if (groupCount >= 3) melodyDefaults[2] = groups[1].preset;
    for (int i = 0; i < 3; i++) {
        int pi = melodyDefaults[i];
        daw.patches[4 + i] = instrumentPresets[pi].patch;
        snprintf(daw.patches[4 + i].p_name, 32, "%s", instrumentPresets[pi].name);
    }

    // --- Populate patterns ---
    for (int i = 0; i < midi.noteCount; i++) {
        MfNoteEvent *e = &midi.notes[i];
        int patIdx = (int)(e->startTick / ticksPerPattern);
        if (patIdx >= numPatterns) continue;

        float localTick = (float)e->startTick - patIdx * ticksPerPattern;
        float stepF = localTick / ticksPerStep;
        int step = (int)roundf(stepF);

        // Wrap boundary notes to next pattern's step 0
        if (step >= stepsPerPattern) {
            step = 0;
            patIdx++;
            if (patIdx >= numPatterns) continue;
        }

        Pattern *pat = &seq.patterns[patIdx];
        float vel = e->velocity / 127.0f;

        if (e->channel == 9) {
            // Drum event — keep loudest hit per track+step
            int track = _mf_drumTrack(e->note);
            StepV2 *sv = &pat->steps[step];
            if (sv->noteCount == 0 || vel > velU8ToFloat(sv->notes[0].velocity)) {
                patSetDrum(pat, step, vel, 0.0f);
            }
        } else {
            // Melody event
            int melTrack = chToTrack[e->channel];
            if (melTrack < 0) continue;
            int absTrack = SEQ_DRUM_TRACKS + melTrack; // 4 + melTrack

            int durSteps = (int)roundf(e->durationTick / ticksPerStep);
            if (durSteps < 1) durSteps = 1;
            if (durSteps > 64) durSteps = 64;

            // Sub-step timing nudge (24 ticks per 16th step, ±12 range)
            float frac = stepF - (int)roundf(stepF);
            int nudgeTicks = (int)roundf(frac * (float)SEQ_TICKS_PER_STEP_16TH);
            if (nudgeTicks < -12) nudgeTicks = -12;
            if (nudgeTicks > 12) nudgeTicks = 12;

            StepV2 *sv = &pat->steps[step];

            // Check for duplicate note (same pitch on same step) — keep longest gate
            bool isDuplicate = false;
            for (int n = 0; n < sv->noteCount; n++) {
                if (sv->notes[n].note == (int8_t)e->note) {
                    if (durSteps > sv->notes[n].gate) {
                        sv->notes[n].gate = (int8_t)durSteps;
                        sv->notes[n].velocity = velFloatToU8(vel);
                    }
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate) {
                if (sv->noteCount == 0) {
                    patSetNote(pat, step, e->note, vel, durSteps);
                } else if (sv->noteCount < SEQ_V2_MAX_POLY) {
                    stepV2AddNote(sv, e->note, velFloatToU8(vel), (int8_t)durSteps);
                }
            }

            // Store nudge on the note (sequencer reads notes[0].nudge for melody timing)
            if (nudgeTicks != 0 && sv->noteCount > 0) {
                // Apply to first note (drives step timing), don't overwrite if already set
                if (sv->notes[0].nudge == 0) {
                    sv->notes[0].nudge = (int8_t)nudgeTicks;
                }
            }
        }
    }

    // Set track lengths
    for (int p = 0; p < numPatterns; p++) {
        for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
            seq.patterns[p].length = stepsPerPattern;
        }
    }

    // --- Arrangement ---
    daw.song.songMode = true;
    daw.song.length = numPatterns;
    daw.song.loopsPerPattern = 1;
    for (int i = 0; i < numPatterns; i++) {
        daw.song.patterns[i] = i;
        daw.song.loopsPerSection[i] = 0;
    }

    // Extract song name from filename
    const char *fname = strrchr(filepath, '/');
    if (!fname) fname = strrchr(filepath, '\\');
    fname = fname ? fname + 1 : filepath;
    strncpy(daw.songName, fname, 63);
    daw.songName[63] = '\0';
    char *dot = strrchr(daw.songName, '.');
    if (dot) *dot = '\0';

    // Reset preset tracking
    for (int i = 0; i < NUM_PATCHES; i++) patchPresetIndex[i] = -1;

    // --- Post-import: build per-track arrangement + launcher ---
    // For each track, find unique patterns by comparing step data
    {
        // Per-track: which pattern index is the "canonical" version for each unique content
        // canonical[track][pat] = first pattern with same content on this track, or pat itself
        int canonical[ARR_MAX_TRACKS][SEQ_NUM_PATTERNS];
        // Per-track unique count
        int uniqueCount[ARR_MAX_TRACKS];
        // Per-track list of unique pattern indices (for launcher)
        int uniquePats[ARR_MAX_TRACKS][LAUNCHER_MAX_SLOTS];

        for (int t = 0; t < ARR_MAX_TRACKS; t++) {
            uniqueCount[t] = 0;
            for (int p = 0; p < numPatterns; p++) canonical[t][p] = p;
        }

        for (int t = 0; t < ARR_MAX_TRACKS && t < SEQ_V2_MAX_TRACKS; t++) {
            for (int p = 0; p < numPatterns; p++) {
                // Check if this pattern's track content matches any earlier pattern
                bool found = false;
                for (int q = 0; q < p; q++) {
                    // Compare step data for this track
                    bool same = true;
                    Pattern *pp = &seq.patterns[p];
                    Pattern *pq = &seq.patterns[q];
                    if (pp->length != pq->length) { same = false; }
                    else {
                        int len = pp->length;
                        for (int s = 0; s < len && same; s++) {
                            StepV2 *a = &pp->steps[s];
                            StepV2 *b = &pq->steps[s];
                            if (a->noteCount != b->noteCount) { same = false; break; }
                            for (int v = 0; v < a->noteCount && same; v++) {
                                if (a->notes[v].note != b->notes[v].note ||
                                    a->notes[v].gate != b->notes[v].gate) same = false;
                            }
                        }
                    }
                    if (same) {
                        canonical[t][p] = canonical[t][q];
                        found = true;
                        break;
                    }
                }
                if (!found) canonical[t][p] = p;
            }

            // Build unique list for launcher
            for (int p = 0; p < numPatterns && uniqueCount[t] < LAUNCHER_MAX_SLOTS; p++) {
                if (canonical[t][p] == p) {
                    // Check if track actually has content in this pattern
                    bool hasContent = false;
                    Pattern *pp = &seq.patterns[p];
                    int len = pp->length;
                    for (int s = 0; s < len; s++) {
                        if (pp->steps[s].noteCount > 0) { hasContent = true; break; }
                    }
                    if (hasContent) {
                        uniquePats[t][uniqueCount[t]++] = p;
                    }
                }
            }
        }

        // Build per-track arrangement
        daw.arr.arrMode = true;
        daw.arr.length = numPatterns;
        daw.song.songMode = false;  // switch from song mode to arrangement mode
        for (int b = 0; b < numPatterns && b < ARR_MAX_BARS; b++) {
            for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                if (t < SEQ_V2_MAX_TRACKS) {
                    // Use canonical pattern for this track (deduplicates)
                    int cp = canonical[t][b];
                    // Check if track has any content in this canonical pattern
                    bool hasContent = false;
                    Pattern *pp = &seq.patterns[cp];
                    int len = pp->length;
                    for (int s = 0; s < len; s++) {
                        if (pp->steps[s].noteCount > 0) { hasContent = true; break; }
                    }
                    daw.arr.cells[b][t] = hasContent ? cp : ARR_EMPTY;
                } else {
                    daw.arr.cells[b][t] = ARR_EMPTY;
                }
            }
        }

        // Build launcher: unique clips per track
        daw.launcher.active = false;
        daw.launcher.quantize = 0;
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
            // Fill with unique patterns for this track
            for (int s = 0; s < uniqueCount[t]; s++) {
                daw.launcher.tracks[t].pattern[s] = uniquePats[t][s];
                daw.launcher.tracks[t].state[s] = CLIP_STOPPED;
            }
        }
    }

    _mf_free(&midi);
    return true;
}

#endif // MIDI_FILE_H
