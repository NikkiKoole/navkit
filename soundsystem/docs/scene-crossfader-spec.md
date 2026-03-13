# Scene & Crossfader System — Spec for DAW Migration

Extracted from `prototype.c`. The DAW already has `Crossfader` struct + UI shell (`daw.c:163-167`, Song tab scenes section, Mix tab XFade controls) but **no scene storage or blend logic**. This doc describes what to implement.

---

## Decision: Sweeps, Automation, and Scenes (2026-03-13)

Three systems that change parameters over time were considered. This is the decision:

| System | Decision | Rationale |
|--------|----------|-----------|
| **P-lock interpolation** | **Won't do** | Was proposed to enable smooth sweeps within patterns, but that need was driven by bridge song migration (House/Deep House). Now that scenes own long sweeps, the motivation is gone. Discrete p-locks at step granularity (50-100ms) already sound smooth enough for pattern-level use. If a smooth sweep is needed, set more p-locks. Revisit only if a real composition need emerges. |
| **Automation lanes** | **Won't do** | Over-engineering. P-locks already cover pattern-level parameter changes. Full lanes would add ~500-900 lines of engine+UI+serialization for marginal benefit. |
| **Scenes + crossfader** | **Do it** (medium effort) | Replaces the bridge's `sweepPhase` hack. Song-level and game-level parameter morphing belongs here, not in per-pattern sequencer data. |

**How they divide responsibilities:**

- **P-locks** (discrete, as-is) = **pattern-level detail**. The musician's tool. Per-step accent variation, filter snaps, parameter tweaks. Scope: one parameter, one step. No interpolation — values snap, which is musically useful (sharp filter opens on downbeats, etc.). For smoother transitions, just set more p-locks across adjacent steps.
- **Scenes + crossfader** = **song-level / game-level morphing**. Long arcs (30-60 second tonal shifts), game state transitions (calm↔combat). Scope: everything at once — all patches, effects, mixer, BPM.

**What this replaces:**

The bridge's `sweepPhase`/`sweepRate` hack in `sound_synth_bridge.c` is a poor man's crossfader — one float driving multiple instrument parameters on a slow sine cycle. Once scenes work, House and Deep House songs become: Scene A = filter closed, Scene B = filter open, crossfader position driven by a slow oscillator or game state. The per-song trigger callbacks (`melodyTriggerAcidBass`, etc.) and `getSweepValue()` can be deleted.

**What this means for the bridge song migration (songs.h → .song):**

- 12 of 14 songs use no sweeps — convert directly to .song format now.
- House + Deep House (the 2 sweep songs) — convert after scenes are implemented. Their sweep behavior becomes scene A/B + crossfader automation.

See also: `done/daw-demo-gaps.md` §"Scenes vs Automation vs P-locks" for the full analysis that led to this decision.

---

## Concepts

**Scene** = a complete snapshot of all sound state at a point in time (patches, effects, mixer, BPM, etc.). Think of it as a "preset for the entire session."

**Crossfader** = smoothly blend between two scenes (A and B). Position 0.0 = pure scene A, 1.0 = pure scene B, anything in between = interpolated mix. This is how you morph between two different sounds live, or map game state to tonal changes.

Inspired by: Elektron Octatrack scenes, Ableton Session View.

---

## Scene Data Model

8 scene slots. Each stores a full copy of everything that affects sound output.

### What a Scene captures (adapt to DAW's DawState)

| Data | Prototype field | DAW equivalent |
|------|----------------|----------------|
| All melody patches | `SynthPatch patches[NUM_PATCHES]` | `daw.patches[SEQ_V2_MAX_TRACKS]` |
| Master effects | `Effects effects` | `daw.masterFx` |
| Mixer (volumes, pans, sends, per-bus FX) | partial (masterVol, drumVol) | `daw.mixer` |
| BPM | `float bpm` | `seq.bpm` |
| Step resolution | `int ticksPerStep` | `seq.ticksPerStep` |
| Track volumes | `float trackVolume[MAX_TRACKS]` | `seq.trackVolume[]` |
| Scale lock (enabled, root, type) | 3 fields | `synthCtx->scaleLock*` |
| Sidechain settings | not in prototype | `daw.sidechain` |
| Tape FX | not in prototype | `daw.tapeFx` |

### What a Scene does NOT capture

- **Pattern data** (steps, notes, p-locks) — prototype included this but it's debatable. Patterns are compositional, not tonal. Consider keeping them out in DAW, or making it a toggle.
- **Transport state** (playing, stopped, position)
- **Internal filter/delay/reverb buffer state** — only user-facing parameters, never DSP state

### Struct sketch

```c
#define NUM_SCENES 8

typedef struct {
    bool initialized;
    SynthPatch patches[SEQ_V2_MAX_TRACKS];  // 7 tracks (4 drum + 3 melody)
    Mixer mixer;
    MasterFX masterFx;
    TapeFX tapeFx;
    Sidechain sidechain;
    float bpm;
    int ticksPerStep;
    float trackVolume[SEQ_V2_MAX_TRACKS];
    bool scaleLockEnabled;
    int scaleRoot;
    int scaleType;
} Scene;

static Scene scenes[NUM_SCENES];
static int currentScene = -1;  // -1 = none, 0-7 = active
```

---

## Scene Operations

### saveScene(int idx)
Copy all current live state into `scenes[idx]`. Set `initialized = true`, `currentScene = idx`.

### loadScene(int idx)
If `scenes[idx].initialized`, copy all stored state back to live state. Set `currentScene = idx`.

**Important:** when restoring effects (MasterFX, TapeFX, bus effects), only copy **user parameters** (knobs/toggles), never internal DSP state (filter memory, delay buffer contents, reverb comb state). The prototype does this by blending into a temp struct, then copying field-by-field. A cleaner approach for DAW: separate user params from DSP state in the structs, or use a whitelist of fields to copy.

### clearScene(int idx)
Set `scenes[idx].initialized = false`. If it was `currentScene`, reset to -1.

---

## Crossfader Blend

When `crossfaderEnabled`, called every frame before rendering. Blends between `scenes[sceneA]` and `scenes[sceneB]` using `position` (0.0–1.0).

### Blending rules

Two types of parameters:

**Continuous (float)** — linear interpolation:
```c
out = a + (b - a) * t;   // lerpf(a, b, t)
```
This covers: all envelope times, volumes, filter cutoff/resonance/env amounts, LFO rates/depths, FM ratios/indices, glide time, drive, effect parameters, BPM, etc.

**Discrete (int, bool, enum)** — threshold switch at 50%:
```c
out = (t < 0.5f) ? a : b;
```
This covers: wave type, preset indices (additive, mallet, membrane, bird, PD), LFO shapes, LFO sync divs, arp mode/rate/chord, unison count, mono mode, filter type, bool toggles (effect enables, arp enable, consonant, nasal, freeze, etc.).

### What gets blended

1. **All SynthPatch fields** — every float is lerped, every int/bool is switched. The prototype's `blendSynthPatch()` explicitly lists ~90 fields. A better approach: iterate SynthPatch fields with a descriptor table, or just do a bulk lerp on all floats (they're laid out contiguously in the struct).

2. **Effects** — same principle. User params lerp/switch, internal DSP state untouched. The prototype copies blended values field-by-field onto the live `fx` struct to avoid clobbering filter memory etc.

3. **Mixer** — volumes and pans lerp, mute/solo switch.

4. **Master volume** — lerp.

5. **BPM** — lerp (smooth tempo morph!).

### Gotcha: new SynthPatch fields

Every time a new field is added to SynthPatch (like we just did with `p_acidMode`, `p_accentSweepAmt`, `p_gimmickDipAmt`), the blend function needs updating. The prototype's approach of listing every field is fragile. Consider:

- **Option A**: Metadata table describing each field (offset, type, blend mode). Auto-generate or maintain manually. Blend function iterates the table.
- **Option B**: Memcpy for bulk float fields, explicit list only for discrete fields. Less safe but way less maintenance.
- **Option C**: Just keep the explicit list (prototype approach). Simpler, just needs discipline when adding fields.

---

## UI

### Scene buttons (Song tab, already has placeholder)
- 8 numbered buttons in a row
- **Click** = load scene (if initialized)
- **Shift+Click** = save current state to scene
- **Right-Click** = clear scene
- Color coding: gray = empty, blue = has content, green = currently active, yellow = hovered
- "Save" button: saves to current scene (or first empty slot if none active)

### Crossfader controls (Mix tab, already has placeholder)
- **XFade toggle** — enable/disable crossfader blending
- **A:** / **B:** — click to cycle scene assignment (0-7)
- **Pos slider** — 0.0 to 1.0, draggable
- Color: A side blue `{100,150,255}`, B side orange `{255,150,100}`
- Transport bar indicator when enabled: `XF:1>2 45%`

### Prototype UI reference
The prototype puts everything in a collapsible "Scenes" section in the top bar. The DAW already has scene slots drawn in the Song tab (`daw.c:2578`) and XFade controls in the Mix tab (`daw.c:3270`), just with no backing logic.

---

## Save/Load

### Song file format
The scratch.song already has `[crossfader]` and `[split]` sections:
```
[crossfader]
enabled = false
pos = 0.5
sceneA = 0
sceneB = 1
count = 8
```

Scenes would need new sections:
```
[scene.0]
initialized = true
bpm = 120
ticksPerStep = 24
scaleLockEnabled = false
scaleRoot = 0
scaleType = 0
masterVol = 0.8
; ... then per-patch subsections like [scene.0.patch.0], [scene.0.patch.1], etc.
; ... and mixer/fx subsections
```

This is a lot of data per scene (8 × full state). Consider:
- Only save initialized scenes
- Compress by only writing fields that differ from defaults (the song file already skips default-value fields implicitly since unknown keys are ignored on load)

---

## Game Audio Use Case

The crossfader is the primary tool for **game state → sound morphing**:

```c
// In game code:
void onGameStateChanged(float intensity) {
    // Scene 1 = calm (soft pads, slow filter, gentle BPM)
    // Scene 2 = combat (heavy bass, fast filter, higher BPM)
    crossfader.position = intensity;  // 0.0 = calm, 1.0 = combat
}
```

This gives continuous, musical transitions between game states without hard cuts. Combined with conditional triggers and mute groups, it's a full adaptive audio system.

---

## Implementation Priority

1. **Scene struct + save/load/clear** — get the data model working
2. **Scene UI** — hook up the existing button placeholders
3. **blendSynthPatch()** — the big one, ~90 fields
4. **blendMixer() + blendMasterFx()** — smaller blend functions
5. **updateCrossfaderBlend()** — call from main loop when enabled
6. **Song file save/load** for scenes
7. **Game audio API** — `setCrossfaderPosition(float)` etc.
