# Soundsystem Dead Code Audit

**Date**: 2026-03-11
**Scope**: `soundsystem/`

---

## Genuinely Dead (superseded or abandoned)

### `processDrumsWithSidechain` — drums.h:678
55-line function, zero callers anywhere. Sidechain routing is handled differently now.

### `plockIndexRemove` — sequencer.h:873
`__attribute__((unused))`. Superseded by the shift+rebuild approach in `seqClearPLock`.

### `setTrackCallbacks` — sequencer.h:1219
New unified track callback API, never adopted. All code uses `initSequencer`/`setMelodyCallbacks`.

### `seqTrackNames[]` — sequencer.h:63
Static array `{"bass","lead","chord"}`, never referenced. Track names are set directly in `initSequencer()` via `seq.trackNames[]`.

### `processDubLoopWithVoices` — effects.h:1062
Only reference is the `(void)` suppression in `soundsystem.h`. No real callers.

### `CHORD_OCTAVES` enum — sequencer.h:142
Handled in `buildChordNotes` switch but never passed as an argument from any call site.

---

## Not Dead, Just Not Integrated Yet

These are planned features that have implementations but no callers yet. Keeping them intentionally.

### Sampler API (sampler.h)
Most of the sampler API (15 of ~19 functions) only has callers via the `(void)` suppression list. Only `samplerPlay` and `processSampler` are used in demo.c. The rest (`samplerPlayPanned`, `samplerPlayLooped`, `samplerStopVoice`, `samplerStopAll`, stereo processing, query functions, load/free) are waiting for deeper integration.

### `rhythm_prob_maps.h`
Entire file (structs, enums, probability map data) is unreferenced from compiled code. Only referenced in docs and the Lua generator tool. This is the grids/rhythm generator feature — not yet wired up.

### `DRUM_IS_SAMPLE` / `DRUM_SAMPLE_SLOT` / `DRUM_MAX_TOTAL` — drums.h:62-65
Infrastructure for sample-based drum slots. Minimal use in demo.c, waiting for full sample-drum integration.

---

## Cleanup Candidates

### `soundsystem_suppress_warnings` — soundsystem.h:69
This function exists solely to `(void)`-cast unused functions and hide compiler warnings. It's a dead code museum — many of the functions it suppresses are genuinely unused (see "Genuinely Dead" above). Consider removing the suppression for truly dead functions so the compiler can flag them naturally.
