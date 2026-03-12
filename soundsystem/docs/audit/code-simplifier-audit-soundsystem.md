# Soundsystem Code Simplifier Audit

**Date**: 2026-03-11
**Scope**: `soundsystem/` — all engine headers, demo files, file format code

---

## HIGH (hurts readability significantly)

### H1. 130+ `#define` macros hiding global state

**Files**: `synth.h:856-996`, `drums.h:252-254`, `sampler.h:82-84`, `sequencer.h:480-482`

Each engine uses file-static global context + `#define` macros that make struct fields look like plain variables:

```c
#define masterVolume (synthCtx->masterVolume)
#define noteAttack (synthCtx->noteAttack)
// ... 100+ more
```

This makes it impossible to know where state lives without memorizing the mapping. The macros also collide across files — `daw.c` has to `#undef masterVolume`, `#undef scaleLockEnabled`, `#undef monoMode` (lines 23-27). Any `#include` ordering change can break things silently.

**Suggestion**: Long-term, pass context pointers explicitly. Short-term, group the macros into documented accessor blocks and consolidate the "ensure initialized" idiom.

---

### H2. `applyPatchToGlobals()` is 147 lines of field-by-field copy

**File**: `patch_trigger.h:16-147`

```c
static void applyPatchToGlobals(SynthPatch *p) {
    noteAttack = p->p_attack;
    noteDecay = p->p_decay;
    // ... 130 more lines
}
```

Every new SynthPatch field requires updating: the macro list, `applyPatchToGlobals()`, `createDefaultPatch()`, and song_file save/load. Extremely error-prone.

**Suggestion**: Data-driven approach — a table of `{offsetof(SynthPatch, field), offsetof(SynthContext, field), size}` entries iterated by a single loop. Or a naming convention that lets a macro generate the copy.

---

### H3. Duplicated file write/read helpers

**Files**: `song_file.h:273-288`, `daw_file.h:28-31`

song_file.h:
```c
static void _sf_writeFloat(FILE *f, const char *key, float val) { fprintf(f, "%s = %.4g\n", ...); }
```

daw_file.h:
```c
static void _dw(FILE *f, const char *k, float v) { fprintf(f, "%s = %.6g\n", ...); }
```

Identical functionality with different names and subtly different float precision (`%.4g` vs `%.6g`) — could cause round-trip precision differences.

**Suggestion**: Extract shared write/read helpers into a common `file_helpers.h` included by both.

---

### H4. Wave type name tables duplicated 3 times

**Files**: `song_file.h:173`, `daw_file.h:33`, `daw.c:90`

```c
// song_file.h
static const char* _sf_waveTypeNames[] = { "square", "saw", "triangle", ... };
// daw_file.h
static const char* _dwWaveNames[] = { "square","saw","triangle", ... };
// daw.c
static const char* waveNames[] = {"Square", "Saw", "Triangle", ... };
```

If a new WaveType is added, all three must be updated in sync.

**Suggestion**: Single canonical table in `synth.h` or `synth_patch.h`. UI display names derived from it if capitalization differs.

---

### H5. `drawWaveThumb()` packs waveform formulas into unreadable single lines

**File**: `daw.c:485-507`

```c
case 11: { float p0=t0<0.5f?t0*t0*2:0.5f+(t0-0.5f)*(2-t0*2); float p1=t1<0.5f?t1*t1*2:0.5f+(t1-0.5f)*(2-t1*2); v0=sinf(p0*6.28f); v1=sinf(p1*6.28f); } break;
```

14 cases, each with multiple semicolons on one line, nested ternaries.

**Suggestion**: Extract `float waveThumbSample(int waveType, float t)` helper. Each case gets properly formatted lines.

---

## MEDIUM (inconsistent or could be clearer)

### M1. Naming convention inconsistency across modules

All engine headers mix conventions:
- `initDrumsContext`, `initSamplerContext` — camelCase, no prefix
- `MidiInput_Init`, `MidiInput_Shutdown` — CamelCase with underscore prefix
- `triggerDrumFull`, `processKick`, `expDecay` — camelCase, no prefix
- `seqRandInt`, `seqRandFloat` — module prefix, camelCase

CLAUDE.md says `CamelCase` with module prefix. Only `midi_input.h` follows this.

---

### M2. `sizeof(Type)` instead of `sizeof(*var)` in memset calls

**Files**: `drums.h:168`, `synth.h:741`, `sampler.h:69`

```c
memset(ctx, 0, sizeof(DrumsContext));
```

Should be `sizeof(*ctx)` to stay correct if the pointer type changes.

---

### M3. CR-78 Kick preset has dead code (possible bug)

**File**: `instrument_presets.h:508,513`

```c
instrumentPresets[32].patch.p_decay = 0.25f;     // line ~508
// ... 4 lines later ...
instrumentPresets[32].patch.p_decay = 0.036f;    // line ~513, overrides the first!
```

First assignment is dead code. Second has comment explaining the effective decay calculation. Should remove the first assignment.

---

### M4. Missing `const` on read-only pointer parameters

**Files**: Multiple

```c
// sampler.h:430 — data is never modified
static float samplerInterpolate(float* data, int length, float position);
// Should be: const float* data

// patch_trigger.h:16 — patch is only read
static void applyPatchToGlobals(SynthPatch *p);
// Should be: const SynthPatch *p

// patch_trigger.h:152
static int playNoteWithPatch(float freq, SynthPatch *p);
// Should be: const SynthPatch *p
```

---

### M5. `seqNoteName()` uses a static buffer — not reentrant

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

### M6. `DRUM_PROC_BEGIN` macro silently declares variable in caller scope

**File**: `drums.h:300-303`

```c
#define DRUM_PROC_BEGIN(dv, dt) \
    if (!(dv)->active) return 0.0f; \
    DrumParams *p = &drumParams; \
    (dv)->time += (dt)
```

Readers of `processKick()` see `p->kickPitch` without knowing where `p` comes from. Some processors (`processHihat`, `processTom`) don't use this macro and manually write the same boilerplate.

**Suggestion**: Either use consistently across all processors, or remove and use explicit code everywhere.

---

### M7. Magic numbers in daw.c mixer initializer

**File**: `daw.c:326-332`

```c
.volume = {0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f},
.filterCut = {1,1,1,1,1,1,1},
.distDrive = {2,2,2,2,2,2,2},
```

Array size is implicitly 7 (NUM_BUSES). If NUM_BUSES changes, these won't compile-error — they'll silently have wrong size.

---

### M8. WAV recording variable names suggest endian awareness that doesn't exist

**File**: `daw.c:245-268`

```c
int le32 = fileSize; fwrite(&le32, 4, 1, f);
short le16 = 1; fwrite(&le16, 2, 1, f);
```

Named `le32`/`le16` but just writes native-endian. Works on macOS (always LE) but names are misleading.

---

### M9. `processSnare()` duplicates envelope logic

**File**: `drums.h:416-422`

Custom two-component envelope instead of using `DRUM_PROC_END` pattern. Same with `processCR78Snare()` at line 592. Justified by the more complex envelope, but means silence threshold logic is scattered across multiple functions.

---

## LOW (minor style nits)

### L1. `PI` defined 3 times

**Files**: `synth.h:11`, `drums.h:10`, `effects.h:14`

Define once in a shared header.

---

### L2. `rhythmPatterns` uses double literals instead of float

**File**: `rhythm_patterns.h:72-198`

```c
.kick = {1.0, 0.0, 0.0, 0.0, ...}
```

Should use `1.0f`, `0.0f` to avoid implicit narrowing.

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

### L5. Thin convenience wrappers in drums.h may be unused

**File**: `drums.h:809-826`

```c
static void drumKick(void) { triggerDrum(DRUM_KICK); }
static void drumSnare(void) { triggerDrum(DRUM_SNARE); }
// 10 more...
```

16 thin wrappers. If only used for keyboard shortcuts in demo.c, they add clutter to the engine header.

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
| H1 | 130+ #define macros hiding state | High (readability, collision risk) | Large (architectural) |
| H2 | 147-line field-by-field copy | High (maintenance, bug risk) | Medium (data-driven table) |
| H3 | Duplicated file helpers | Medium (inconsistent precision) | Low (extract shared header) |
| H4 | Wave names duplicated 3x | Medium (sync risk) | Low (single table) |
| H5 | Unreadable waveform one-liners | Medium (readability) | Low (extract helper) |
| M3 | Dead code in CR-78 preset | Medium (possible bug) | Trivial (delete one line) |
| M4 | Missing const | Low (correctness signal) | Low (add const) |
| M6 | DRUM_PROC_BEGIN hidden variable | Medium (readability) | Low (pick one approach) |
| L2 | Double literals in float arrays | Low (implicit narrowing) | Trivial (add f suffix) |

**Quick wins**: M3 (delete dead line), H4 (single name table), H5 (extract helper), L2 (add f suffix)
**Biggest long-term payoff**: H1 (explicit context passing), H2 (data-driven patch copy)
