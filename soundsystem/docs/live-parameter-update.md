# Live Parameter Update for Filterbank (and all voice params)

## Problem

Tweaking filterbank knobs (Alpha, Q, Spacing, Noise, etc.) while holding a note has no effect. The change only applies on the **next** note trigger. This feels wrong for a filterbank — real analog filterbanks (Grendel RA-99, MS-20, DFAM) respond instantly because knobs are voltage-connected to the circuit.

## Root Cause

The parameter flow has three hops, and the middle one only fires at note-on:

```
UI knob (p_fbNoiseMix)
    → applyPatchToGlobals()  ← only called at note trigger
        → synth global (noteFbNoiseMix)
            → voice copy (v->fbNoiseMix)  ← snapshot, frozen for note lifetime
```

The same issue affects **all** voice parameters (filter cutoff/resonance, wavefold, ring mod, etc.) — they're all snapshot at note-on. The filterbank is where it hurts most because the whole point is hands-on tweaking.

## Why It's Not Trivial

`processVoice()` reads from `v->` fields (per-voice copies), not from globals or the patch. To make params live, we need the voice to continuously receive updated values. But:

1. **Globals are stale too** — `noteXxx` globals are only set by `applyPatchToGlobals()`, which is only called at note trigger. Updating `v->` from globals in processVoice doesn't help if the globals themselves are stale.

2. **processVoice has no access to the patch** — it only sees the `Voice` struct and `synthCtx`. The `SynthPatch` lives in the DAW layer (`daw.patches[]`).

3. **Multi-timbral conflict** — different voices may belong to different patches (drum on bus 0, bass on bus 4). A single global can't serve all voices simultaneously.

4. **Per-note randomization** — filterbank `fbRandomize` adds offsets at note-on. Live-updating from globals would wipe those offsets. Need to store random deltas separately or accept the tradeoff.

## Possible Approaches

### A: Continuous `applyPatchToGlobals` per bus (smallest change)

In `DawAudioCallback`, before the voice loop, call `applyPatchToGlobals(&daw.patches[bus])` for each bus. Then in `processVoice`, pull from globals for the filterbank params only.

- **Pro**: Minimal code change, reuses existing infrastructure
- **Con**: Only one bus's globals are "current" at a time — would need to apply-per-voice or apply-per-bus-group. Messy with the current single-global design.

### B: Store patch pointer on Voice

Add `const SynthPatch *patch` to the `Voice` struct, set at note-on. In `processVoice`, read live params directly from `v->patch->p_fbBaseFreq` etc.

- **Pro**: Clean, each voice reads its own patch. Works with multi-timbral.
- **Con**: Pointer could go stale if patch array is reallocated (unlikely but fragile). Need to decide which params are "live" vs "snapshot" (envelope amounts, ADSR times should stay snapshot).

### C: Per-bus "live override" struct

Add a small `LiveParams` struct per bus that the DAW continuously updates. Voices read from their bus's LiveParams for filterbank/filter knobs.

- **Pro**: Clean separation, no pointer-to-patch fragility, explicit about what's live
- **Con**: New struct, new sync path, more fields to maintain

### D: Flag certain params as "continuous" in the voice

Keep the snapshot model but add a per-frame update block in `processVoice` that selectively overwrites certain `v->` fields from globals. Pair with making `dawSyncEngineStateFrom` also push patch params to globals continuously.

- **Pro**: Incremental, can start with just filterbank and expand
- **Con**: Which globals? Multi-timbral still conflicts unless we scope by bus.

## Recommendation

**Approach B** (patch pointer on Voice) is cleanest long-term. The `daw.patches[]` array is stable (fixed size, never reallocated), so the pointer is safe. Mark filterbank + filter + wavefold + ring mod params as "live-read from patch" and keep envelope/ADSR/pitch as "snapshot at note-on."

This also benefits the main filter — cutoff/resonance would respond live too, which is equally desirable.

## MIDI Learn Already Proves the Path

MIDI Learn (`midi_input.h`) stores a `float*` directly to the `p_` field (e.g. `&daw.patches[x].p_fbNoiseMix`) and writes to it on every CC event (`*m->target = value`). So MIDI CC already updates the patch in real time — but the voice never sees it because it reads from its own frozen copy.

With Approach B (patch pointer on Voice), both MIDI CC and UI knobs would work live for free: both write to the `p_` field, and `processVoice` would read `v->patch->p_fbNoiseMix` directly each sample. No new sync mechanism needed.

## Scope

- Filterbank: all `fb*` params + `formantMix`/`formantShift`/`formantQ`
- Main filter: `filterCutoff`, `filterResonance` (already modulated by env+LFO, but base value is frozen)
- Synthesis modes: `wavefoldAmount`, `ringModFreq`, `hardSyncRatio`
- NOT live: ADSR times, pitch envelope, oscillator type, unison count (these should be note-on only)
