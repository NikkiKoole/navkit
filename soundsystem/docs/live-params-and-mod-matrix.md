# Live Parameters + Modulation Matrix

> Status: PHASES 1-2 COMPLETE — live params working. Phases 3-7 (mod matrix) open.
> Supersedes: `live-parameter-update.md`, `mod-matrix-design.md`
> Part of: **Sound System Feature Chain** (see below)

## How This Fits In

Four features form a dependency chain. This doc covers steps 1-2:

```
1. Patch pointer on Voice         ← THIS DOC (Phase 1)
   └── 2. Live param reads        ← THIS DOC (Phase 2)
       └── 3. Scenes + crossfader   (scene-crossfader-spec.md)
           └── 4. Lua conductor     (lua-conductor-architecture.md)
   └── 2b. Mod matrix             ← THIS DOC (Phase 3-7)
```

**Why this order matters:** The crossfader morphs patch `p_` fields between scenes. Without live param reads, those changes only reach voices on the next note-on — defeating the point of smooth morphing. Phase 1-2 here are prerequisites for the crossfader to work properly.

The mod matrix (Phase 3+) is independent of scenes — it builds on the same patch pointer foundation but adds per-voice modulation routing.

---

## The Problem (shared by both features)

`applyPatchToGlobals()` runs at note-on only. Every voice parameter is a frozen snapshot:

```
UI knob / MIDI CC → patch field (p_filterCutoff)
    → applyPatchToGlobals()  ← only at note trigger
        → synth global (noteFilterCutoff)
            → voice copy (v->filterCutoff)  ← frozen for note lifetime
```

This means:
- **Tweaking knobs while a note is held** has no effect until the next note (bad for filterbank, filter, wavefold)
- **LFO modulation targets are hardwired** — can't route LFO1 to drive instead of filter
- **Velocity routing is hardcoded** — 4 fixed targets with dedicated code each
- **No register-dependent shaping** — can't make high notes brighter or low notes warmer per-patch

All of these are the same underlying issue: voices don't have live access to patch values or flexible modulation routing.

## The Solution: Patch Pointer + Mod Matrix

One mechanism solves both: store a `const SynthPatch *patch` on each Voice, and add a per-sample modulation step.

### Per-sample parameter flow

```
each sample, per active voice:
  1. Read base value live from patch:   base = v->patch->p_filterCutoff
  2. Accumulate mod matrix offsets:     base += modRouteOffset(LFO1, FILTER_CUTOFF)
  3. Voice uses the result:             apply to DSP
```

Knob tweaks and MIDI CC both write to `p_` fields on the patch. The voice reads them every sample. Mod routes add offsets on top. Both features work through the same path.

---

## Phase 1: Patch Pointer on Voice (~50 lines) — DONE

**The prerequisite for everything else.**

**Files:** `synth.h`, `patch_trigger.h`

Add to Voice struct:
```c
const SynthPatch *patch;   // set at note-on, read per-sample for live params
```

Set in `playNote` / `playNoteWithPatch`:
```c
v->patch = currentPatch;   // pointer to daw.patches[bus] or equivalent
```

The `daw.patches[]` array is fixed-size and never reallocated, so the pointer is stable for the voice's lifetime.

**Test:** Verify `v->patch` is non-NULL for active voices, matches the patch used at trigger time.

---

## Phase 2: Live Parameter Reads (~100 lines) — DONE

**Immediate payoff — knob tweaks affect held notes.**

**Files:** `synth.h` (processVoice), `synth_oscillators.h`

In `processVoice()`, replace frozen `v->` reads with live reads from `v->patch->` for selected params:

**Live (read from patch each sample):**
- Filterbank: all `fb*` params, `formantMix`, `formantShift`, `formantQ`
- Main filter: `filterCutoff`, `filterResonance` (base value — LFO/env still modulate on top)
- Synthesis: `wavefoldAmount`, `ringModFreq`, `hardSyncRatio`
- Drive, click level

**Snapshot (keep frozen at note-on):**
- ADSR times (attack, decay, sustain, release) — changing mid-note would cause envelope jumps
- Pitch envelope — same reason
- Oscillator type, unison count — would require voice reinit
- Per-note randomization offsets (filterbank `fbRandomize`) — store random deltas at note-on, add to live base

```c
// In processVoice, replace:
float cutoff = v->filterCutoff;
// With:
float cutoff = v->patch ? v->patch->p_filterCutoff : v->filterCutoff;
```

The fallback to `v->filterCutoff` handles edge cases (voice triggered without patch pointer, e.g. SFX).

**MIDI Learn already works for free:** MIDI CC writes to `&daw.patches[x].p_fbNoiseMix` directly. With live reads, both MIDI CC and UI knobs affect held notes instantly. No new sync mechanism needed.

**Test:** Hold a note, move filterbank knob in DAW, verify sound changes in real time.

---

## Phase 3: Mod Matrix Core (~150 lines)

**Flexible source → destination routing.**

**Files:** new `synth_modmatrix.h` (or section in `synth.h`), `synth_patch.h`

### Data structures

```c
typedef enum {
    MOD_SRC_NONE = 0,
    MOD_SRC_VELOCITY,       // per-note, 0-1 (captured at note-on)
    MOD_SRC_NOTE_NUM,       // per-note, normalized 0-1 (C1=0, C7=1)
    MOD_SRC_LFO1,           // per-sample, -1 to +1
    MOD_SRC_LFO2,
    MOD_SRC_LFO3,
    MOD_SRC_LFO4,
    MOD_SRC_LFO5,
    MOD_SRC_ENVELOPE,       // per-voice ADSR output, 0-1
    MOD_SRC_RANDOM,         // per-note sample-and-hold, 0-1
    MOD_SRC_COUNT
} ModSource;

typedef enum {
    MOD_DST_NONE = 0,
    // Universal
    MOD_DST_FILTER_CUTOFF,
    MOD_DST_FILTER_RESO,
    MOD_DST_DRIVE,
    MOD_DST_CLICK_LEVEL,
    MOD_DST_OSC_LEVEL,
    MOD_DST_PITCH,
    MOD_DST_AMP,
    MOD_DST_PAN,
    MOD_DST_FM_DEPTH,
    MOD_DST_PWM,
    // Electric piano
    MOD_DST_EP_BELL,
    MOD_DST_EP_HARDNESS,
    MOD_DST_EP_PICKUP_POS,
    MOD_DST_EP_PICKUP_DIST,
    MOD_DST_COUNT
} ModDest;

#define MAX_MOD_ROUTES 12

typedef struct {
    ModSource source;
    ModDest   dest;
    float     amount;   // bipolar, -N to +N
    float     curve;    // 0=linear, 1=squared, -1=sqrt
} ModRoute;
```

In `SynthPatch`:
```c
ModRoute p_modRoutes[MAX_MOD_ROUTES];
int      p_modRouteCount;
```

### Runtime

Two functions, split by update rate:

```c
// Called once at note-on (velocity, note number, random — don't change during note)
void applyModRoutes_NoteOn(Voice *v, SynthContext *ctx);

// Called per-sample in processVoice (LFO, envelope sources)
void applyModRoutes_PerSample(Voice *v, SynthContext *ctx);
```

Both accumulate into a `float modAccum[MOD_DST_COUNT]` array, then add offsets to the live base values from Phase 2.

### Curve shaping

Per-route curve parameter preserves existing velocity behavior:
```
curve = 0   →  linear (good for LFOs)
curve = 1   →  squared (current velocity behavior: soft=0.25, hard=1.0)
curve = -1  →  sqrt (always-present even at soft velocities)
```

Add `noteNumNorm` and `randomHold` to Voice struct (set at note-on).

**Test:** Hardcode one route in a preset (`VELOCITY → FILTER_CUTOFF, 0.5, squared`), verify identical to current `p_velToFilter = 0.5`.

---

## Phase 4: Migrate Velocity Params (~100 lines)

**Files:** `instrument_presets.h`, `synth.h`, `patch_trigger.h`

Convert the 4 hardwired velocity targets to mod routes:
```c
// Old:
preset.p_velToFilter = 0.5f;
preset.p_velToDrive = 4.0f;
preset.p_velToClick = 0.6f;
preset.p_oscVelSens = 1.0f;

// New:
.p_modRouteCount = 4,
.p_modRoutes = {
    { MOD_SRC_VELOCITY, MOD_DST_FILTER_CUTOFF, 0.5f, 1.0f },
    { MOD_SRC_VELOCITY, MOD_DST_DRIVE,         4.0f, 1.0f },
    { MOD_SRC_VELOCITY, MOD_DST_CLICK_LEVEL,   0.6f, 1.0f },
    { MOD_SRC_VELOCITY, MOD_DST_OSC_LEVEL,     1.0f, 1.0f },
},
```

Keep old `p_velTo*` fields temporarily. Patch loading auto-migrates: if `p_modRouteCount == 0` but old fields are nonzero, generate equivalent routes.

Remove old velocity application code from synth.h (~50 lines of `if (amount > 0) { value += amount * vel * vel; }` blocks).

**Test:** A/B every preset with velocity sensitivity — must sound identical.

---

## Phase 5: Register Shaping + New Presets (~100 lines)

**Files:** `synth_oscillators.h`, presets

Add note-number routes for register-dependent sound:
```c
// "Rhodes Dynamic" preset
{ MOD_SRC_NOTE_NUM, MOD_DST_EP_BELL,        0.6f,  0.0f },  // high = bell
{ MOD_SRC_NOTE_NUM, MOD_DST_EP_HARDNESS,    0.3f,  0.0f },  // high = harder
{ MOD_SRC_NOTE_NUM, MOD_DST_EP_PICKUP_POS, -0.4f,  0.0f },  // low = warm
{ MOD_SRC_NOTE_NUM, MOD_DST_FILTER_CUTOFF,  0.2f,  0.0f },  // high = brighter
```

This replaces the hardcoded spectral tilt in `initEPianoSettings()` with per-patch configurable register behavior. Every preset can define its own curve.

**Test:** Play chromatic runs — low notes warm, high notes bell-like.

---

## Phase 6: Migrate LFO Routing (~100 lines)

**Files:** `synth.h` (LFO application code), presets

Current hardwired LFO→target assignments become default routes:
```c
{ MOD_SRC_LFO1, MOD_DST_FILTER_CUTOFF, depth, 0.0f },  // was: LFO1 always → filter
{ MOD_SRC_LFO3, MOD_DST_AMP,           depth, 0.0f },  // was: LFO3 always → amplitude
```

Now LFO1 can target drive, LFO2 can target pan, two LFOs can target the same destination. Remove old hardwired LFO application blocks.

Uses `applyModRoutes_PerSample` — these sources change every sample.

**Test:** Existing LFO-heavy presets (tremolo, auto-wah) sound identical.

---

## Phase 7: DAW UI (~300 lines)

**Files:** `daw.c`

Mod matrix panel — each route is one row:
- Source dropdown (velocity, note, LFO1-5, envelope, random)
- Dest dropdown (filter, drive, pitch, EP params, etc.)
- Amount slider (bipolar)
- Curve selector (linear / squared / sqrt)

Visual feedback: highlight destinations being modulated, show current mod value. Reuse cyan velocity indicator pattern.

---

## Cost Summary

| Phase | Lines changed | What you get |
|-------|---------------|-------------|
| 1 | ~50 | Patch pointer on Voice (prerequisite) |
| 2 | ~200-300 | Live knob tweaks on held notes. Touches 50-100 individual read sites across synth.h and synth_oscillators.h — each `v->` field that should be live becomes `v->patch->p_`. Per-note random deltas need separate storage. Each engine (filterbank, epiano, organ, FM, pluck, etc.) must be tested for pops/glitches on mid-note knob changes. |
| 3 | ~150 | Mod matrix engine + data structures |
| 4 | ~100-150 | Velocity via matrix — migrate all 263 presets' `p_velTo*` fields to mod routes, remove old velocity application code |
| 5 | ~100 | Register-dependent Rhodes, new presets |
| 6 | ~150-200 | Flexible LFO routing — migrate existing hardwired LFO→target code to routes. Need `applyModRoutes_PerSample` running in the voice loop. Each LFO-heavy preset must be A/B tested. |
| 7 | ~300 | Full DAW UI for editing routes |
| **Total** | **~1100-1250** | |

Phase 2 is the most labor-intensive despite sounding simple — it's a broad refactor touching every engine's inner loop, not a localized feature addition.

Phases 1-2 are the quick win (live params, ~150 lines). Phase 3 builds on the same foundation. Phases 4-6 are cleanup/migration. Phase 7 is polish.

---

## What This Intentionally Doesn't Do

- **No modulation of modulation amounts** — no secondary routing. Simple mental model.
- **No per-route LFO** — reuses existing 5 LFOs. Add LFO6 to source enum if needed later.
- **No MIDI CC sources yet** — trivial to add later as `MOD_SRC_CC1..CC4`.
- **No effect parameter destinations** — effect params live on buses, not voices. Could add as `MOD_DST_FX_*` later.
- **12 slots max** — more than enough. Hardware synths use 8-16.

## Backward Compatibility

- Old `p_velTo*` and LFO fields stay in patch struct during migration
- Patch loading auto-migrates old fields to routes when `p_modRouteCount == 0`
- Save format: routes are `int, int, float, float` per slot — straightforward to serialize
- Song file: add `[modroutes]` section, backward-compatible (old files just have no routes)

## Thread Safety Note

The patch pointer approach is safe because:
- `daw.patches[]` is a fixed-size array, never reallocated
- UI/MIDI writes single floats to `p_` fields (effectively atomic on x86/ARM)
- Audio thread reads the same floats — sees either old or new value, never torn
- Same safety model as the current direct-write approach, just reading from patch instead of voice copy
