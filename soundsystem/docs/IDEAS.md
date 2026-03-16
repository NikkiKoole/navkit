# Soundsystem Feature Ideas

Analysis of potential features: choke groups, sidechaining, mixing/routing, and modulation matrix.

## Feature Overview

| Feature | Complexity | Value | Dependencies |
|---------|------------|-------|--------------|
| Choke Groups | Low | High | None |
| Mixing/Routing | Medium | High | None |
| Sidechain Compression | Medium | High | Mixing/Routing |
| Mod Matrix | High | Very High | None (but benefits from routing) |

---

## 1. Choke Groups (Easiest - Do First)

### What It Is
Drums that cut each other off. When one triggers, it silences another.

### Common Use Cases
- Closed hihat chokes open hihat (already partially implemented)
- Kick chokes long bass drum tail
- Muted/open conga pairs

### Why It's Easy
- No new DSP required
- Just add a check: "when drum X triggers, silence drum Y"
- Already have `drumVoices[].active` flag to set false

### Implementation Sketch
```c
// In drums.h, add choke group definitions
typedef struct {
    DrumType members[4];  // Up to 4 drums in a group
    int count;
} ChokeGroup;

static ChokeGroup chokeGroups[] = {
    {{DRUM_CLOSED_HH, DRUM_OPEN_HH}, 2},  // Hihats choke each other
    // User-configurable groups...
};

// In triggerDrum(), before triggering:
for (each group containing this drum) {
    for (each other member) {
        drumVoices[member].active = false;  // Choke it
    }
}
```

### Effort Estimate
~30-50 lines of code. Could be done in an hour.

---

## 2. Mixing/Routing (Medium - Do Second)

### What It Is
Separate audio buses so signals can be processed independently before final mix.

### Current Architecture (Problem)
```
Synth voices ──┐
               ├──► Single mix ──► Effects ──► Output
Drums ─────────┘
```

Everything gets summed immediately. No way to:
- Apply effects to drums only
- Sidechain one thing from another
- Have separate reverb sends

### Proposed Architecture
```
Synth voices ──► Synth Bus ──┐
                             ├──► Master Bus ──► Master Effects ──► Output
Drums ─────────► Drum Bus ───┘
                    │
                    └──► (sidechain signal available here)
```

### Implementation Sketch
```c
typedef struct {
    float synthBus;
    float drumBus;
    float masterBus;
} MixBuses;

// In audio callback:
MixBuses bus = {0};

for (int v = 0; v < NUM_VOICES; v++) {
    bus.synthBus += processVoice(&synthVoices[v], SAMPLE_RATE);
}
bus.drumBus = processDrums(dt);

// Now we have separate signals to work with!
bus.masterBus = bus.synthBus + bus.drumBus;
bus.masterBus = processEffects(bus.masterBus, dt);
```

### Why Do This Before Sidechain
Sidechain needs access to separate signals. Without buses, there's nothing to sidechain from/to.

### Effort Estimate
~100-150 lines. Refactoring existing code, not complex logic.

---

## 3. Sidechain Compression (Medium - Do Third)

### What It Is
One signal's amplitude controls another signal's volume.

### Depends On
- Mixing/Routing (need separate buses)

### Components Needed

#### A. Envelope Follower
Tracks the amplitude of the sidechain source (e.g., kick).

```c
typedef struct {
    float envelope;      // Current envelope value (0-1)
    float attack;        // How fast envelope rises (ms)
    float release;       // How fast envelope falls (ms)
} EnvelopeFollower;

static float updateEnvelope(EnvelopeFollower* env, float input, float dt) {
    float inputAbs = fabsf(input);
    if (inputAbs > env->envelope) {
        // Attack
        env->envelope += (inputAbs - env->envelope) * (1.0f - expf(-dt / env->attack));
    } else {
        // Release
        env->envelope += (inputAbs - env->envelope) * (1.0f - expf(-dt / env->release));
    }
    return env->envelope;
}
```

#### B. Gain Reduction
Apply ducking based on envelope.

```c
typedef struct {
    EnvelopeFollower env;
    float depth;         // 0-1, how much to duck
    float threshold;     // Level above which ducking starts
} Sidechain;

static float applySidechain(Sidechain* sc, float target, float source, float dt) {
    float env = updateEnvelope(&sc->env, source, dt);
    
    // Calculate gain reduction
    float reduction = 0.0f;
    if (env > sc->threshold) {
        reduction = (env - sc->threshold) * sc->depth;
    }
    
    return target * (1.0f - reduction);
}
```

#### C. Routing Options
- Source: kick only, all drums, specific drum
- Target: bass patch, all synths, specific voice

### Common Sidechain Configurations

| Name | Source | Target | Character |
|------|--------|--------|-----------|
| Classic EDM pump | Kick | Bass + Pads | Obvious pumping |
| Subtle bass duck | Kick | Bass only | Tight low end |
| Full mix pump | Kick | Everything | Extreme effect |

### Effort Estimate
~150-200 lines. The DSP is simple, but UI and routing options add complexity.

---

## 4. Mod Matrix (Most Complex - Do Last)

### What It Is
A flexible system where any modulation source can control any parameter.

### Why It's Complex
- Need to enumerate all possible sources
- Need to enumerate all possible destinations  
- Need per-voice vs global routing
- UI becomes complicated
- Performance considerations (many mod routes = many calculations)

### Modulation Sources

| Source | Type | Notes |
|--------|------|-------|
| LFO 1-4 | Continuous | Already have LFOs, need to generalize |
| Envelope (ADSR) | Continuous | Per-voice |
| Envelope Follower | Continuous | From any audio signal |
| Note On trigger | One-shot | Fires on key press |
| Sequencer step | One-shot | Fires on step advance |
| Velocity | Per-note | Already exists |
| Random | Per-note or continuous | S&H or noise |
| MIDI CC | External | Future consideration |

### Modulation Destinations

| Category | Parameters |
|----------|------------|
| Oscillator | Pitch, PWM, wave morph, detune |
| Filter | Cutoff, resonance, drive |
| Amplitude | Volume, pan |
| Effects | Any effect parameter |
| LFO | Rate, depth (mod the modulators!) |
| Drums | Pitch, decay, tone per drum |

### Architecture Options

#### Option A: Fixed Slots (Simpler)
```c
#define MAX_MOD_ROUTES 8

typedef struct {
    ModSource source;
    ModDest destination;
    float amount;        // -1 to +1
    bool enabled;
} ModRoute;

ModRoute modMatrix[MAX_MOD_ROUTES];
```

#### Option B: Per-Parameter Sources (More Flexible)
```c
// Each parameter knows its mod sources
typedef struct {
    float baseValue;
    ModSource source1;
    float amount1;
    ModSource source2;
    float amount2;
} ModulatedParam;
```

### Implementation Phases

#### Phase 1: Global LFO → Any Effect Parameter
Easiest starting point. One global LFO, simple dropdown to pick destination.

#### Phase 2: Multiple LFOs with Shape Options
Add 2-4 LFOs, each with rate/depth/shape.

#### Phase 3: Envelope Sources
Use amplitude envelope or filter envelope as mod source.

#### Phase 4: Full Matrix UI
Grid-based UI: sources on Y axis, destinations on X axis, amounts at intersections.

### "On Note Trigger" Specifically

This is a simpler subset of mod matrix:
```c
typedef struct {
    ModDest destination;
    float amount;
    float duration;      // How long the modulation lasts
} NoteTriggerMod;

// On note-on:
for (each trigger mod) {
    apply one-shot modulation to destination
}
```

Use cases:
- Pitch bend down on every note (plucky bass)
- Filter sweep on each note
- Random pan position per note

### Effort Estimate
- Phase 1: ~200 lines
- Phase 2: ~150 more lines
- Phase 3: ~200 more lines
- Phase 4: ~400+ lines (mostly UI)

Full mod matrix is a significant project (1000+ lines total).

---

## Recommended Implementation Order

```
1. Choke Groups          [Easy]     ← Start here, quick win
        ↓
2. Mixing/Routing        [Medium]   ← Foundation for sidechain
        ↓
3. Sidechain Compression [Medium]   ← Depends on routing
        ↓
4. Mod Matrix Phase 1    [Medium]   ← Global LFO to effects
        ↓
5. Mod Matrix Phase 2-4  [Complex]  ← Incremental expansion
```

### Rationale

1. **Choke groups first**: Immediate musical value, trivial to implement, no dependencies.

2. **Routing second**: Architectural change that enables sidechain. Better to do this refactoring before adding more features on top.

3. **Sidechain third**: Now possible because routing exists. High musical value for electronic music.

4. **Mod matrix last**: Most complex, but also most flexible. Benefits from having routing infrastructure in place. Can be done incrementally.

---

## What Makes Sense vs. What Doesn't

### Makes Sense ✓

| Feature | Why |
|---------|-----|
| Kick → Bass sidechain | Classic technique, your system already has kick and bass separation conceptually |
| Hihat choke groups | Standard drum machine behavior, users expect this |
| LFO → Filter cutoff | Already exists per-voice, generalizing it makes sense |
| Note trigger → Pitch envelope | Common in bass sounds, plucks |
| Sequencer step → Parameter | Already have P-locks, this is similar concept |

### Questionable / Overkill ?

| Feature | Concern |
|---------|---------|
| Full MIDI-style mod matrix | May be overengineering for a chiptune/game synth |
| Drum → Drum sidechain | Rarely needed, choke groups cover most cases |
| Per-voice mod matrix | Complexity explosion, probably not needed |
| External modulation sources | Scope creep unless you need MIDI integration |

### Recommendation

Keep it game/chiptune focused:
- Choke groups: Yes, essential for drums
- Sidechain: Yes, but just kick→bass is probably enough
- Mod matrix: Start with "LFO to any effect" and "note trigger pitch envelope", expand only if needed

Don't try to build Serum. Build something that fits your demo's scope.

---

## Quick Reference: Complexity vs Value

```
                        VALUE
                    Low    High
                ┌────────┬────────┐
           Low  │        │ Choke  │
COMPLEXITY      │        │ Groups │
                ├────────┼────────┤
          High  │ Full   │ Side-  │
                │ ModMat │ chain  │
                └────────┴────────┘

Sweet spot: High value, low-medium complexity
= Choke groups, basic sidechain, simple LFO routing
```
