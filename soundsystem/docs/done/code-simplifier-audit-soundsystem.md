# Soundsystem Code Simplifier Audit

**Date**: 2026-03-11
**Scope**: `soundsystem/` — all engine headers, demo files, file format code

---

## HIGH (hurts readability significantly)

### ~~H1. 130+ `#define` macros hiding global state~~ — WON'T FIX

**Reason**: The cure is worse than the disease. Replacing macros with explicit `ctx->` prefixes would touch thousands of lines across ~20 files, adding visual noise to dense DSP math where short names actually help readability. The collision issue is real but stable in practice (only 5 `#undef`s in 3 files, unchanged for months). Massive blast radius with no feature on the roadmap blocked by it. Reconsider only if thread-safe contexts become necessary.

---

### ~~H2. `applyPatchToGlobals()` is 147 lines of field-by-field copy~~ — WON'T FIX

**Reason**: A data-driven offsetof table trades "obvious but tedious" for "clever but opaque." The field names don't match between SynthPatch (`p_attack`) and SynthContext (`noteAttack`), so auto-generation isn't possible — you'd still need an explicit table. A wrong table entry causes silent audio data corruption, whereas a wrong line in the current copy is easy to spot in diffs. The current approach is verbose but reliable for a solo/small-team project.

---

### ~~H3. Duplicated file write/read helpers~~ **Done**

Extracted shared `file_helpers.h` — `song_file.h` includes it and `#define`s `_sf_writeFloat` to `fileWriteFloat`. `daw_file.h` also uses the shared helpers.

---

### ~~H4. Wave type name tables duplicated 3 times~~ **Done**

Single canonical `waveTypeNames[]` in `synth.h:115`. Both `song_file.h` and `daw_file.h` `#define` their local names to it.

---

### ~~H5. `drawWaveThumb()` packs waveform formulas into unreadable single lines~~ **Done**

Extracted `waveThumbSample(int waveType, float t, int i)` helper in `daw_widgets.h:36`. `drawWaveThumb()` now calls it cleanly.

---

## MEDIUM (inconsistent or could be clearer)

### ~~M1. Naming convention inconsistency across modules~~ **Partially done**

midi_input.h renamed from `MidiInput_Init`/`MidiLearn_Arm` style → `midiInputInit`/`midiLearnArm` (camelCase, matching the rest of the codebase). Remaining `seqPrefix`/`patPrefix` functions kept as-is — the module prefix is intentional scoping.

---

### ~~M2. `sizeof(Type)` instead of `sizeof(*var)` in memset calls~~ **Done**

Changed `sizeof(EffectsContext)` → `sizeof(*ctx)` and `sizeof(MixerContext)` → `sizeof(*ctx)` in effects.h. drums.h and sampler.h instances already gone.

---

### ~~M3. CR-78 Kick preset has dead code (possible bug)~~ **Done**

Duplicate `p_decay` assignment removed. Only the effective value (`0.036f`) remains at line ~558.

---

### ~~M4. Missing `const` on read-only pointer parameters~~ **Done**

All three already have `const`: `samplerInterpolate(const float* data, ...)`, `applyPatchToGlobals(const SynthPatch *p)`, `playNoteWithPatch(float freq, const SynthPatch *p)`.

---

### ~~M5. `seqNoteName()` uses a static buffer — not reentrant~~ **Done**

**File**: `sequencer.h:95-102`

```c
static const char* seqNoteName(int midi) {
    static char buf[8];
    snprintf(buf, sizeof(buf), "%s%d", seqNoteNames[note], octave);
    return buf;
}
```

Calling it twice in the same expression (`printf("%s %s", seqNoteName(60), seqNoteName(72))`) clobbers the first result.

---

### ~~M6. `DRUM_PROC_BEGIN` macro silently declares variable in caller scope~~ **N/A** — `drums.h` deleted, drum processing now in unified preset system

---

### ~~M7. Magic numbers in daw.c mixer initializer~~ **Done**

Arrays now have 8 elements matching `NUM_BUSES = 8`. The implicit-size risk remains if NUM_BUSES changes again, but values are currently in sync.

---

### ~~M8. WAV recording variable names suggest endian awareness that doesn't exist~~ **Done**

**File**: `daw.c:245-268`

```c
int le32 = fileSize; fwrite(&le32, 4, 1, f);
short le16 = 1; fwrite(&le16, 2, 1, f);
```

Named `le32`/`le16` but just writes native-endian. Works on macOS (always LE) but names are misleading.

---

### ~~M9. `processSnare()` duplicates envelope logic~~ **N/A** — `drums.h` deleted, drum processing now in unified preset system

---

## LOW (minor style nits)

### ~~L1. `PI` defined 3 times~~ **N/A** — drums.h deleted, remaining 2 use `#ifndef` guards (no actual duplication at runtime)

---

### ~~L2. `rhythmPatterns` uses double literals instead of float~~ **Done**

All values now use `f` suffix (`1.0f`, `0.0f`, etc.).

---

### L3. `instrument_presets.h` uses raw array indices

**File**: `instrument_presets.h` throughout

```c
instrumentPresets[24].name = "808 Kick";
instrumentPresets[25].name = "808 Snare";
```

Named constants like `PRESET_808_KICK = 24` would make references in daw.c self-documenting.

---

### L4. `samplerLoadWav` is 180 lines

**File**: `sampler.h:115-293`

Could extract `readWavHeader()`, `readWavSamples()`, `convertStereoToMono()`.

---

### ~~L5. Thin convenience wrappers in drums.h may be unused~~ **N/A** — `drums.h` deleted

---

### L6. Duplicated voice-processing logic between mono/stereo sampler

**File**: `sampler.h:442-527`

`processSampler()` and `processSamplerStereo()` share ~30 lines of identical loop body (interpolation, position advance, loop handling). Could extract shared inner loop.

---

### L7. Scattered file-static globals in daw.c

**File**: `daw.c:224-296`

Recording state, debug state, voice tracking, and MIDI state declared as scattered variables. Could be grouped into structs (`DawRecordingState`, `DawDebugState`, etc.).

---

## IMPACT SUMMARY

| # | Issue | Impact | Effort |
|---|-------|--------|--------|
| ~~H1~~ | ~~130+ #define macros hiding state~~ | ~~High~~ | Won't fix — ctx-> prefix hurts DSP readability, stable in practice |
| ~~H2~~ | ~~147-line field-by-field copy~~ | ~~High~~ | Won't fix — offsetof table trades obvious bugs for silent ones |
| ~~H3~~ | ~~Duplicated file helpers~~ | ~~Medium~~ | Done — shared `file_helpers.h` |
| ~~H4~~ | ~~Wave names duplicated 3x~~ | ~~Medium~~ | Done — single `waveTypeNames[]` in synth.h |
| ~~H5~~ | ~~Unreadable waveform one-liners~~ | ~~Medium~~ | Done — `waveThumbSample()` in daw_widgets.h |
| ~~M3~~ | ~~Dead code in CR-78 preset~~ | ~~Medium~~ | Done — duplicate removed |
| M4 | Missing const | Low (correctness signal) | Low (add const) |
| ~~M6~~ | ~~DRUM_PROC_BEGIN hidden variable~~ | ~~Medium~~ | N/A — drums.h deleted |
| ~~L2~~ | ~~Double literals in float arrays~~ | ~~Low~~ | Done — `f` suffixes added |

**All quick wins resolved**: H3, H4, H5, M3, L2 — all done.
**Won't fix**: H1 (macros stable in practice, refactor hurts DSP readability), H2 (verbose but reliable, offsetof table not worth the risk)
