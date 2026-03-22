# Modulation Matrix Design

## What Problem Does This Solve?

We already have three modulation systems doing real work, but they're all hardwired:

**1. Velocity (per-note, 4 fixed targets)**
- `p_oscVelSens` → oscillator 2-6 amplitude (squared curve)
- `p_velToFilter` → filter cutoff (squared curve)
- `p_velToClick` → click transient level (squared curve)
- `p_velToDrive` → drive/saturation (squared curve)

These live as dedicated patch fields, applied with dedicated code in `synth.h` lines 3240-3412. Each one has its own `if (amount > 0) { value += amount * vel * vel; }` block. Adding a fifth velocity target means adding another patch field, another global, another apply line, and another UI control.

**2. Note number / register (implicit, buried in DSP)**
- `initEPianoSettings()` blends mode amplitudes by frequency — higher notes naturally get more bell partials, lower notes more fundamental weight
- But this is baked into the oscillator init, not configurable per-patch
- A Rhodes player expects: low register = warm bark, high register = bell-like shimmer
- Currently no way to say "in this patch, note number should also open the filter" or "higher notes should have less drive"

**3. LFOs (5 slots, fixed destinations)**
- LFO1 → filter cutoff (always)
- LFO2 → filter resonance (always)
- LFO3 → amplitude (always)
- LFO4 → pitch (always)
- LFO5 → FM depth (always)

Want LFO1 to modulate drive instead of filter? Can't. Want two LFOs on the same target? Can't.

**The pattern is identical in all three cases: source x amount → destination.**

A mod matrix makes this explicit, configurable, and extensible without touching DSP code every time we want a new routing.

---

## What It Enables

### Register-dependent sound shaping (the Rhodes thing)
Instead of hardcoding spectral tilt in the oscillator, express it as patch routes:
```
NOTE_NUM  →  EP_BELL       amount=+0.6   (higher notes = more bell)
NOTE_NUM  →  EP_HARDNESS   amount=+0.3   (higher notes = harder hammer)
NOTE_NUM  →  EP_PICKUP_POS amount=-0.4   (lower notes = centered pickup = warmer)
NOTE_NUM  →  FILTER_CUTOFF amount=+0.2   (higher notes = brighter filter)
```
Now every preset can define its own register behavior. An aggressive bark preset could invert these. A Wurlitzer preset would use different curves entirely.

### Velocity routing without dedicated code
The four existing velocity targets become default routes in presets:
```
VELOCITY  →  OSC_LEVEL     amount=1.0  curve=squared
VELOCITY  →  FILTER_CUTOFF amount=0.3  curve=squared
VELOCITY  →  CLICK_LEVEL   amount=0.6  curve=squared
VELOCITY  →  DRIVE         amount=4.0  curve=squared
```
Want velocity to also modulate bell emphasis? Add a route. No code change.

### Flexible LFO routing
```
LFO1  →  FILTER_CUTOFF  amount=0.5   (classic auto-wah)
LFO2  →  DRIVE          amount=0.3   (pulsing distortion)
LFO3  →  EP_PICKUP_POS  amount=0.2   (pickup position wobble — unique Rhodes effect)
```

### Cross-engine usefulness
Not just for Rhodes. The same matrix works for:
- Subtractive synth: velocity → filter + resonance, LFO → PWM
- Organ: note number → drawbar balance, LFO → leslie speed
- FM: velocity → FM depth, note number → operator ratios
- Drums: velocity → pitch + decay + drive

---

## Data Structures

### ModSource enum
```c
typedef enum {
    MOD_SRC_NONE = 0,
    MOD_SRC_VELOCITY,       // per-note, 0-1 (captured at note-on)
    MOD_SRC_NOTE_NUM,       // per-note, normalized 0-1 (C1=0, C7=1 roughly)
    MOD_SRC_LFO1,
    MOD_SRC_LFO2,
    MOD_SRC_LFO3,
    MOD_SRC_LFO4,
    MOD_SRC_LFO5,
    MOD_SRC_ENVELOPE,       // per-voice ADSR output, 0-1
    MOD_SRC_RANDOM,         // per-note sample-and-hold random, 0-1
    MOD_SRC_COUNT
} ModSource;
```

**Why these sources:**
- Velocity and note number are the two things that vary per-note and are already doing modulation work
- LFO 1-5 reuses the existing LFO infrastructure (no new oscillators needed)
- Envelope gives amp-follower-style modulation (filter opens with attack, closes with release)
- Random gives per-note variation (humanize). Sample-and-hold: one random value per note-on, held for note duration

### ModDest enum
```c
typedef enum {
    MOD_DST_NONE = 0,

    // Universal targets (work for any engine)
    MOD_DST_FILTER_CUTOFF,
    MOD_DST_FILTER_RESO,
    MOD_DST_DRIVE,
    MOD_DST_CLICK_LEVEL,
    MOD_DST_OSC_LEVEL,      // osc 2-6 amplitude scaling
    MOD_DST_PITCH,           // semitones offset
    MOD_DST_AMP,             // output amplitude
    MOD_DST_PAN,
    MOD_DST_FM_DEPTH,
    MOD_DST_PWM,

    // Electric piano targets (only meaningful for WAVE_EPIANO)
    MOD_DST_EP_BELL,         // bell emphasis 0-1
    MOD_DST_EP_HARDNESS,     // hammer hardness 0-1
    MOD_DST_EP_PICKUP_POS,   // pickup position 0-1
    MOD_DST_EP_PICKUP_DIST,  // pickup distance (nonlinearity) 0-1

    MOD_DST_COUNT
} ModDest;
```

Engine-specific destinations are fine — they just do nothing when the engine doesn't use them. No conditional logic needed; the EP globals simply won't be read by a subtractive voice.

### ModRoute struct
```c
#define MAX_MOD_ROUTES 12

typedef struct {
    ModSource source;
    ModDest   dest;
    float     amount;   // bipolar, -N to +N (not clamped to -1..1 — drive wants 4.0)
    float     curve;    // response shape: 0=linear, 1=squared, -1=sqrt
} ModRoute;
```

### Curve parameter

This is important. The existing velocity code all uses squared curves (`vel * vel`). The EP bell modes use sqrt curves. Making this a per-route parameter preserves that behavior and opens new possibilities.

```
curve = 0   →  output = input                    (linear, good for LFOs)
curve = 1   →  output = input * input             (squared, current velocity behavior)
curve = -1  →  output = sqrt(input)               (sqrt, bell modes — always present even soft)
curve = 0.5 →  output = input^1.5                 (in between)
```

Implementation: `output = powf(fabsf(input), 1.0f + curve) * signf(input)`

Squared velocity means: soft (0.5) → 0.25 effect, medium (0.75) → 0.56, hard (1.0) → 1.0. This dramatic contrast is what makes Rhodes velocity feel right. Linear would make soft notes too bright/driven.

### Patch integration
```c
// In SynthPatch (synth_patch.h)
ModRoute p_modRoutes[MAX_MOD_ROUTES];
int      p_modRouteCount;
```

12 slots is generous. Most hardware synths have 8-16 and users rarely fill them all. If we ever need more, just bump the constant.

---

## How It Works At Runtime

### Source value resolution
Called once per voice per sample (or once per block if we optimize later):

```c
float getModSourceValue(Voice *v, SynthContext *ctx, ModSource src) {
    switch (src) {
        case MOD_SRC_VELOCITY:  return v->noteVolume;       // captured at note-on
        case MOD_SRC_NOTE_NUM:  return v->noteNumNorm;      // 0-1 normalized
        case MOD_SRC_LFO1:     return ctx->lfo1Value;       // -1 to +1
        case MOD_SRC_LFO2:     return ctx->lfo2Value;
        case MOD_SRC_LFO3:     return ctx->lfo3Value;
        case MOD_SRC_LFO4:     return ctx->lfo4Value;
        case MOD_SRC_LFO5:     return ctx->lfo5Value;
        case MOD_SRC_ENVELOPE: return v->envValue;          // 0-1
        case MOD_SRC_RANDOM:   return v->randomHold;        // 0-1, set at note-on
        default:               return 0.0f;
    }
}
```

### Curve application
```c
float applyCurve(float input, float curve) {
    if (curve == 0.0f) return input;                                   // fast path: linear
    float absIn = fabsf(input);
    float shaped = powf(absIn + 1e-10f, 1.0f + curve);                // epsilon avoids pow(0,x) edge cases
    return (input >= 0.0f) ? shaped : -shaped;                         // preserve sign for bipolar sources
}
```

### Destination accumulation
The core function — run after `applyPatchToGlobals()`, before voice processing:

```c
void applyModRoutes(Voice *v, SynthContext *ctx, SynthPatch *patch) {
    // Accumulator per destination, initialized to 0
    float modAccum[MOD_DST_COUNT] = {0};

    for (int i = 0; i < patch->p_modRouteCount; i++) {
        ModRoute *r = &patch->p_modRoutes[i];
        if (r->source == MOD_SRC_NONE || r->dest == MOD_DST_NONE) continue;

        float srcVal = getModSourceValue(v, ctx, r->source);
        float shaped = applyCurve(srcVal, r->curve);
        modAccum[r->dest] += shaped * r->amount;
    }

    // Apply accumulated modulation to voice/globals (additive)
    v->filterCutoff  += modAccum[MOD_DST_FILTER_CUTOFF];
    v->filterReso    += modAccum[MOD_DST_FILTER_RESO];
    v->drive         += modAccum[MOD_DST_DRIVE];
    v->clickLevel    *= (1.0f + modAccum[MOD_DST_CLICK_LEVEL]);  // multiplicative feels better for click
    // ... etc for each destination
    ctx->epBell      = clampf(ctx->epBell + modAccum[MOD_DST_EP_BELL], 0.0f, 1.0f);
    ctx->epHardness  = clampf(ctx->epHardness + modAccum[MOD_DST_EP_HARDNESS], 0.0f, 1.0f);
    ctx->epPickupPos = clampf(ctx->epPickupPos + modAccum[MOD_DST_EP_PICKUP_POS], 0.0f, 1.0f);
    ctx->epPickupDist= clampf(ctx->epPickupDist + modAccum[MOD_DST_EP_PICKUP_DIST], 0.0f, 1.0f);
}
```

**Additive modulation**: base value comes from the patch, mod routes add/subtract. This matches how every synth mod matrix works — the patch sets the center point, modulation moves around it.

**Clamping on EP params**: These are 0-1 bounded physical parameters. Filter cutoff and drive can go wherever they want (the DSP clamps internally), but pickup position = 1.3 doesn't make physical sense.

### Where it hooks in

In `patch_trigger.h`, the note-on flow is:
1. `applyPatchToGlobals(patch)` — copy all `p_` fields to context globals
2. **`applyModRoutes(voice, ctx, patch)`** — NEW: accumulate and apply mod offsets
3. `initEPianoSettings()` / oscillator init — reads the (now-modulated) globals
4. Voice starts processing

For per-sample modulation (LFO targets), `applyModRoutes` runs in the voice render loop too, but only for routes with LFO/envelope sources. Velocity and note-number routes are note-on only (they don't change during a note).

---

## Implementation Plan

### Phase 1: Data structure + apply function (~150 lines)
**Files touched:** `synth_patch.h`, `synth.h` (or new `synth_modmatrix.h`)

- Add `ModSource`, `ModDest`, `ModRoute` enums and struct
- Add `p_modRoutes[12]` and `p_modRouteCount` to `SynthPatch`
- Write `getModSourceValue()`, `applyCurve()`, `applyModRoutes()`
- Hook `applyModRoutes()` into voice trigger path after `applyPatchToGlobals()`
- Add `noteNumNorm` to Voice (normalize MIDI note to 0-1 range at note-on)
- Add `randomHold` to Voice (random float set at note-on)
- **Test**: hardcode one route in a preset (e.g., `VELOCITY → FILTER_CUTOFF, 0.5, squared`), verify it sounds identical to the current `p_velToFilter = 0.5` behavior

### Phase 2: Migrate velocity params (~100 lines)
**Files touched:** `instrument_presets.h`, `synth.h`, `patch_trigger.h`

- Convert each preset's `p_velToFilter/Drive/Click` and `p_oscVelSens` into mod routes
- Example for "Rhodes Bark" preset:
  ```c
  .p_modRouteCount = 3,
  .p_modRoutes = {
      { MOD_SRC_VELOCITY, MOD_DST_DRIVE,         4.0f, 1.0f },
      { MOD_SRC_VELOCITY, MOD_DST_FILTER_CUTOFF,  0.5f, 1.0f },
      { MOD_SRC_VELOCITY, MOD_DST_CLICK_LEVEL,    0.6f, 1.0f },
  },
  ```
- Keep old `p_velTo*` fields temporarily for backward compat, but stop reading them in DSP
- Remove old velocity application code blocks from synth.h
- **Test**: A/B every Rhodes/EP preset to confirm identical sound

### Phase 3: Note number source + register shaping (~100 lines)
**Files touched:** `synth_oscillators.h`, presets

- `MOD_SRC_NOTE_NUM` normalization: `noteNumNorm = (midiNote - 24) / 72.0f` (C1=0, C7=1, clamped)
- Add EP-specific destinations: `MOD_DST_EP_BELL`, `MOD_DST_EP_HARDNESS`, `MOD_DST_EP_PICKUP_POS`, `MOD_DST_EP_PICKUP_DIST`
- These modulate the globals *before* `initEPianoSettings()` reads them
- Create "Rhodes Dynamic" preset that uses both velocity and note-number routes:
  ```c
  .p_modRouteCount = 6,
  .p_modRoutes = {
      { MOD_SRC_VELOCITY, MOD_DST_DRIVE,          3.0f,  1.0f },  // hard = barky
      { MOD_SRC_VELOCITY, MOD_DST_FILTER_CUTOFF,   0.4f,  1.0f },  // hard = brighter
      { MOD_SRC_VELOCITY, MOD_DST_CLICK_LEVEL,     0.5f,  1.0f },  // hard = more click
      { MOD_SRC_NOTE_NUM, MOD_DST_EP_BELL,         0.6f,  0.0f },  // high = bell
      { MOD_SRC_NOTE_NUM, MOD_DST_EP_HARDNESS,     0.3f,  0.0f },  // high = harder hammer
      { MOD_SRC_NOTE_NUM, MOD_DST_EP_PICKUP_POS,  -0.4f,  0.0f },  // low = centered (warm)
  },
  ```
- **Test**: play chromatic runs, verify low notes sound warmer, high notes more bell-like

### Phase 4: Migrate LFO routing (~100 lines)
**Files touched:** `synth.h` (LFO application code), presets

- Current hardwired LFO targets become default routes in presets
- For patches that use LFO1 for filter: `{ MOD_SRC_LFO1, MOD_DST_FILTER_CUTOFF, amount, 0.0f }`
- LFO sources need per-sample update path (separate from note-on-only sources)
- Split `applyModRoutes` into `applyModRoutes_NoteOn` (velocity, note, random) and `applyModRoutes_PerSample` (LFO, envelope)
- Remove old hardwired LFO application code
- **Test**: existing LFO-heavy presets (tremolo, auto-wah) sound identical

### Phase 5: DAW UI (~300 lines)
**Files touched:** `daw.c`

- Mod matrix panel (toggled from main view or as a tab)
- Each route = one row: [Source dropdown] [Dest dropdown] [Amount slider] [Curve slider]
- Show active routes at top, empty slot at bottom for adding
- Source/dest dropdowns as scrollable lists or left/right arrow selectors
- Amount: horizontal slider, bipolar (-N to +N), center = 0
- Curve: small slider or preset buttons (linear / squared / sqrt)
- Visual feedback: highlight destinations that are being modulated, show current mod value
- Reuse the cyan velocity indicator pattern from existing UI for showing mod amounts on target parameters

---

## What This Intentionally Doesn't Do

- **No modulation of modulation amounts** (no "mod of mod" / secondary routing). Keeps mental model simple.
- **No per-route LFO** — reuses the existing 5 LFOs. If you need 6 LFOs, add LFO6 to the source enum later.
- **No MIDI CC sources yet** — trivial to add (`MOD_SRC_CC1` through `MOD_SRC_CC4`, read from MIDI input). Not needed until external controller support.
- **No per-route polarity/invert toggle** — negative `amount` already handles inversion.
- **No effect parameter destinations yet** — effect params live on buses not voices. Could add later as `MOD_DST_FX_*` that write to bus effect params.
- **12 slots max** — more than enough. If a preset needs 13 routes, the patch design is probably overcomplicated.

## Backward Compatibility

- Old `p_velToFilter` / `p_velToDrive` / `p_velToClick` / `p_oscVelSens` fields stay in the patch struct through migration
- Patch loading: if `p_modRouteCount == 0` but old velocity fields are nonzero, auto-generate equivalent routes (one-time migration at load)
- Once all presets are converted, mark old fields as deprecated
- Save format: mod routes are just `int, int, float, float` per slot — straightforward to serialize

## Cost Estimate

| Phase | Lines | What you get |
|-------|-------|-------------|
| 1 | ~150 | Core engine, one test route working |
| 2 | ~100 | All velocity modulation via matrix, old code removed |
| 3 | ~100 | Register-dependent Rhodes sound, new presets |
| 4 | ~100 | LFO routing flexible, old hardwired LFOs removed |
| 5 | ~300 | Full UI for editing routes in DAW |
| **Total** | **~750** | |

Phases 1-3 are the high-value ones — they solve the Rhodes register problem and unify velocity handling. Phase 4 is cleanup. Phase 5 is polish.
